"""Read-only, loopback-only JSON/SSE bridge. Python 3.9+, standard library only."""

import argparse
import json
import os
from pathlib import Path
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parent
MAX_BYTES = 8 * 1024 * 1024
STOP = threading.Event()


class SnapshotSource:
    def __init__(self, repository, explicit):
        self.repository = repository
        self.explicit = explicit
        self.path = explicit
        self.next_scan = 0
        self.signature = None
        self.data = None
        self.lock = threading.Lock()

    def discover(self):
        candidates = []
        for directory, children, files in os.walk(self.repository):
            children[:] = [name for name in children if name not in
                           {'.git', '.vs', 'node_modules', 'MeshCache', 'Backups', '__pycache__'}]
            path = Path(directory)
            if 'SaveData' in path.parts and 'monitor.json' in files:
                candidate = path / 'monitor.json'
                try:
                    candidates.append((candidate.stat().st_mtime_ns, candidate))
                except OSError:
                    pass
        return max(candidates, key=lambda item: item[0])[1] if candidates else None

    def read(self):
        with self.lock:
            now = time.time()
            if not self.explicit and time.monotonic() >= self.next_scan:
                self.next_scan = time.monotonic() + 5
                candidate = self.discover()
                if candidate != self.path:
                    self.path, self.signature, self.data = candidate, None, None
            envelope = {'type': 'monitor.snapshot', 'serverTimeMs': int(now * 1000),
                        'source': str(self.path) if self.path else None,
                        'state': 'waiting', 'ageSeconds': None, 'data': None, 'error': None}
            if not self.path:
                return envelope
            try:
                stat = self.path.stat()
                signature = (stat.st_mtime_ns, stat.st_size)
                if stat.st_size > MAX_BYTES:
                    raise ValueError('Snapshot exceeds 8 MiB limit')
                if signature != self.signature:
                    # Closing before parsing keeps Windows atomic replacements unobstructed.
                    with self.path.open('rb') as file:
                        raw = file.read(MAX_BYTES + 1)
                    if len(raw) > MAX_BYTES:
                        raise ValueError('Snapshot exceeds 8 MiB limit')
                    def reject_constant(value):
                        raise ValueError('Non-finite JSON value: ' + value)
                    data = json.loads(raw.decode('utf-8'), parse_constant=reject_constant)
                    if not isinstance(data, dict) or data.get('schemaVersion') != 1 or data.get('type') != 'world.snapshot':
                        raise ValueError('Unsupported snapshot schema')
                    for key in ('npcs', 'villages', 'monsters', 'actionNames', 'news'):
                        if not isinstance(data.get(key), list):
                            raise ValueError('Missing array: ' + key)
                    if not isinstance(data.get('summary'), dict) or not isinstance(data.get('errors'), dict):
                        raise ValueError('Missing summary/errors')
                    if not isinstance(data.get('publishedAtMs'), (int, float)):
                        raise ValueError('Missing timestamp')
                    for key in ('running', 'paused', 'focused', 'blocked'):
                        if not isinstance(data.get(key), bool):
                            raise ValueError('Missing flag: ' + key)
                    if not isinstance(data.get('session'), str) or not isinstance(data.get('sequence'), int):
                        raise ValueError('Missing sequence/session')
                    self.data, self.signature = data, signature
                data = self.data
                age = max(0, now - data['publishedAtMs'] / 1000)
                state = ('stopped' if not data['running'] else 'stale' if age > 5 else
                         'blocked' if data['blocked'] else 'paused' if data['paused'] else 'live')
                envelope.update(data=data, ageSeconds=round(age, 1), state=state)
            except (OSError, ValueError, TypeError) as error:
                envelope.update(state='error', error=str(error), data=self.data)
                if self.data:
                    envelope['ageSeconds'] = round(max(0, now - self.data['publishedAtMs'] / 1000), 1)
            return envelope


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, format_string, *args):
        # The streaming connection does not create a log entry per snapshot.
        print('[Monitor HTTP] ' + format_string % args)

    def send_headers(self, status, mime, length=None):
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'")
        self.send_header('Connection', 'close')
        self.close_connection = True
        if length is not None:
            self.send_header('Content-Length', str(length))
        self.end_headers()

    def do_GET(self):
        host = self.headers_in_host()
        origin = self.headers.get('Origin')
        if host not in self.server.allowed_hosts or (origin and origin != 'http://' + host):
            self.send_error(403, 'Local same-origin requests only')
            return
        route = urlsplit(self.path).path
        if route == '/events':
            self.connection.settimeout(5)
            self.send_headers(200, 'text/event-stream; charset=utf-8')
            try:
                self.wfile.write(b'retry: 1500\n\n')
                while not STOP.is_set():
                    data = json.dumps(self.server.source.read(), ensure_ascii=False, allow_nan=False)
                    self.wfile.write(('event: snapshot\ndata: ' + data + '\n\n').encode('utf-8'))
                    self.wfile.flush()
                    STOP.wait(1)
            except (OSError, ValueError):
                pass
            return
        if route == '/api/snapshot':
            content = json.dumps(self.server.source.read(), ensure_ascii=False, allow_nan=False).encode('utf-8')
            self.send_headers(200, 'application/json; charset=utf-8', len(content))
            self.wfile.write(content)
            return
        assets = {'/': ('index.html', 'text/html; charset=utf-8'),
                  '/index.html': ('index.html', 'text/html; charset=utf-8'),
                  '/app.js': ('app.js', 'text/javascript; charset=utf-8'),
                  '/styles.css': ('styles.css', 'text/css; charset=utf-8')}
        if route not in assets:
            self.send_error(404)
            return
        filename, mime = assets[route]
        content = (ROOT / 'dist' / filename).read_bytes()
        self.send_headers(200, mime, len(content))
        self.wfile.write(content)

    def headers_in_host(self):
        return self.headers.get('Host', '').lower()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--snapshot', type=Path, help='Exact monitor.json path; otherwise auto-discover newest under repository SaveData')
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error('Port must be between 1024 and 65535')
    source = SnapshotSource(ROOT.parent, args.snapshot.resolve() if args.snapshot else None)
    try:
        server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    except OSError as error:
        parser.exit(1, f'Cannot start local monitor: {error}. Try --port 8766\n')
    server.daemon_threads = True
    server.source = source
    server.allowed_hosts = {f'127.0.0.1:{args.port}', f'localhost:{args.port}'}
    print(f'Open http://127.0.0.1:{args.port} (read-only, local PC only). Ctrl+C stops the server.')
    print('Waiting for game snapshots. Use --snapshot PATH to pin a particular game/save.')
    try:
        server.serve_forever(poll_interval=0.5)
    except KeyboardInterrupt:
        pass
    finally:
        STOP.set()
        server.server_close()


if __name__ == '__main__':
    main()
