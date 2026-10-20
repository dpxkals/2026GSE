#include "stdafx.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "MonitorServer.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    long long UnixMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    std::string Lower(std::string value)
    {
        for (char& c : value)
        {
            c = char(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    std::string Headers(int status, const std::string& mime, size_t length, bool stream = false)
    {
        return "HTTP/1.1 " + std::to_string(status) + (status == 200 ? " OK\r\n" : " Error\r\n") +
               "Content-Type: " + mime +
               "\r\nCache-Control: no-store\r\n"
               "X-Content-Type-Options: nosniff\r\n"
               "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self'; "
               "connect-src 'self'; frame-ancestors 'none'; base-uri 'none'\r\n"
               "Connection: close\r\n" +
               (stream ? "" : "Content-Length: " + std::to_string(length) + "\r\n") + "\r\n";
    }
}

struct MonitorServer::Impl
{
    struct Asset
    {
        std::string mime, body;
    };

    struct Client
    {
        SOCKET socket = INVALID_SOCKET;
        std::string request, output;
        size_t sent = 0;
        bool handled = false, stream = false;
        Clock::time_point deadline = Clock::now() + std::chrono::seconds(5);
        Clock::time_point nextEvent{};
    };

    SOCKET listener = INVALID_SOCKET;
    bool winsock = false;
    unsigned short port = 0;
    std::atomic<bool> stopping{false};
    std::thread worker;
    std::vector<Client> clients;
    std::map<std::string, Asset> assets;
    std::mutex mutex;
    std::string source, snapshot, state = "waiting";
    Clock::time_point captured{};

    ~Impl()
    {
        stopping = true;
        if (worker.joinable())
        {
            worker.join();
        }
        for (auto& client : clients)
        {
            closesocket(client.socket);
        }
        if (listener != INVALID_SOCKET)
        {
            closesocket(listener);
        }
        if (winsock)
        {
            WSACleanup();
        }
    }

    void Start(const std::filesystem::path& directory)
    {
        for (const auto& entry :
             std::map<std::string, std::string>{{"index.html", "text/html; charset=utf-8"},
                                                {"app.js", "text/javascript; charset=utf-8"},
                                                {"styles.css", "text/css; charset=utf-8"}})
        {
            std::ifstream file(directory / entry.first, std::ios::binary | std::ios::ate);
            if (!file || file.tellg() < 0 || file.tellg() > 4 * 1024 * 1024)
            {
                throw std::runtime_error(
                    "Missing/invalid Monitor assets beside game executable; rebuild project.");
            }
            std::string body(size_t(file.tellg()), '\0');
            file.seekg(0);
            if (!file.read(body.data(), std::streamsize(body.size())))
            {
                throw std::runtime_error("Cannot read Monitor assets.");
            }
            assets.emplace("/" + entry.first, Asset{entry.second, std::move(body)});
        }
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            throw std::runtime_error("WSAStartup failed.");
        }
        winsock = true;
        for (unsigned short candidate = 8765; candidate <= 8774; ++candidate)
        {
            listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listener == INVALID_SOCKET)
            {
                throw std::runtime_error("Cannot create monitor socket.");
            }
            BOOL exclusive = TRUE;
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(candidate);
            if (setsockopt(listener,
                           SOL_SOCKET,
                           SO_EXCLUSIVEADDRUSE,
                           reinterpret_cast<const char*>(&exclusive),
                           sizeof(exclusive)) == 0 &&
                bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 &&
                listen(listener, 8) == 0)
            {
                port = candidate;
                break;
            }
            closesocket(listener);
            listener = INVALID_SOCKET;
        }
        if (!port)
        {
            throw std::runtime_error("Monitor ports 8765-8774 are unavailable.");
        }
        u_long nonblocking = 1;
        if (ioctlsocket(listener, FIONBIO, &nonblocking) != 0)
        {
            throw std::runtime_error("Cannot configure monitor listener.");
        }
        clients.reserve(16);
        worker = std::thread(
            [this]()
            {
                try
                {
                    Run();
                }
                catch (const std::exception& error)
                {
                    std::cerr << "[Monitor HTTP] Worker stopped: " << error.what() << '\n';
                }
            });
        std::cout << "[Monitor HTTP] Started: http://127.0.0.1:" << port
                  << " (automatic; stops with game)\n";
    }

    std::string Envelope()
    {
        std::lock_guard<std::mutex> guard(mutex);
        auto age =
            snapshot.empty()
                ? 0LL
                : std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - captured)
                      .count();
        std::string current = age > 5000 && state != "stopped" ? "stale" : state;
        return "{\"type\":\"monitor.snapshot\",\"serverTimeMs\":" + std::to_string(UnixMs()) +
               ",\"source\":" + source + ",\"state\":\"" + current +
               "\",\"ageSeconds\":" + (snapshot.empty() ? "null" : std::to_string(age / 1000.0)) +
               ",\"error\":null,\"data\":" + (snapshot.empty() ? "null" : snapshot) + "}";
    }

    void Respond(Client& client, int status, const std::string& mime, const std::string& body)
    {
        client.output = Headers(status, mime, body.size()) + body;
        client.handled = true;
        client.deadline = Clock::now() + std::chrono::seconds(5);
    }

    void Request(Client& client)
    {
        std::istringstream input(client.request);
        std::string method, path, version, line;
        input >> method >> path >> version;
        std::getline(input, line);
        std::map<std::string, std::string> headers;
        bool invalid = false;
        while (std::getline(input, line) && line != "\r" && !line.empty())
        {
            auto colon = line.find(':');
            if (colon == std::string::npos)
            {
                invalid = true;
                break;
            }
            std::string name = Lower(line.substr(0, colon));
            auto begin = line.find_first_not_of(" \t", colon + 1);
            auto end = line.find_last_not_of(" \t\r");
            std::string value = begin == std::string::npos || end < begin
                                    ? ""
                                    : line.substr(begin, end - begin + 1);
            if (!headers.emplace(name, value).second)
            {
                invalid = true;
            }
        }
        std::string host = Lower(headers["host"]);
        bool local = host == "127.0.0.1:" + std::to_string(port) ||
                     host == "localhost:" + std::to_string(port);
        auto origin = headers.find("origin");
        if (invalid || !local || (origin != headers.end() && origin->second != "http://" + host))
        {
            Respond(client, 403, "text/plain", "Local same-origin requests only.");
            return;
        }
        if (method != "GET" || (version != "HTTP/1.1" && version != "HTTP/1.0"))
        {
            Respond(client, 405, "text/plain", "GET required.");
            return;
        }
        path = path.substr(0, path.find('?'));
        if (path == "/events")
        {
            client.handled = client.stream = true;
            client.output = Headers(200, "text/event-stream; charset=utf-8", 0, true) +
                            "retry: 1500\n\nevent: snapshot\ndata: " + Envelope() + "\n\n";
            client.nextEvent = Clock::now() + std::chrono::seconds(1);
            client.deadline = Clock::now() + std::chrono::seconds(5);
        }
        else if (path == "/api/snapshot")
        {
            Respond(client, 200, "application/json; charset=utf-8", Envelope());
        }
        else
        {
            auto asset = assets.find(path == "/" ? "/index.html" : path);
            if (asset == assets.end())
            {
                Respond(client, 404, "text/plain", "Not found.");
            }
            else
            {
                Respond(client, 200, asset->second.mime, asset->second.body);
            }
        }
        client.request.clear();
    }

    void Run()
    {
        while (!stopping)
        {
            fd_set reads, writes;
            FD_ZERO(&reads);
            FD_ZERO(&writes);
            FD_SET(listener, &reads);
            auto now = Clock::now();
            for (auto& client : clients)
            {
                if (client.stream && client.output.empty() && now >= client.nextEvent)
                {
                    client.output = "event: snapshot\ndata: " + Envelope() + "\n\n";
                    client.sent = 0;
                    client.nextEvent = now + std::chrono::seconds(1);
                    client.deadline = now + std::chrono::seconds(5);
                }
                FD_SET(client.socket, &reads);
                if (!client.output.empty())
                {
                    FD_SET(client.socket, &writes);
                }
            }
            timeval timeout{0, 100000};
            if (select(0, &reads, &writes, nullptr, &timeout) == SOCKET_ERROR)
            {
                throw std::runtime_error("select failed.");
            }
            if (FD_ISSET(listener, &reads))
            {
                SOCKET incoming = accept(listener, nullptr, nullptr);
                if (incoming != INVALID_SOCKET)
                {
                    u_long nonblocking = 1;
                    if (clients.size() >= 16 || ioctlsocket(incoming, FIONBIO, &nonblocking) != 0)
                    {
                        closesocket(incoming);
                    }
                    else
                    {
                        Client client;
                        client.socket = incoming;
                        clients.push_back(std::move(client));
                    }
                }
            }
            for (auto it = clients.begin(); it != clients.end();)
            {
                bool close = false;
                if (FD_ISSET(it->socket, &reads))
                {
                    char buffer[4096];
                    int count = recv(it->socket, buffer, sizeof(buffer), 0);
                    close = count == 0 ||
                            (count == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK);
                    if (count > 0)
                    {
                        if (it->handled)
                        {
                            close = true; // One request per connection.
                        }
                        else
                        {
                            it->request.append(buffer, size_t(count));
                            if (it->request.size() > 8192)
                            {
                                close = true;
                            }
                            else if (it->request.find("\r\n\r\n") != std::string::npos)
                            {
                                Request(*it);
                            }
                        }
                    }
                }
                if (!close && !it->output.empty() && FD_ISSET(it->socket, &writes))
                {
                    int count = send(it->socket,
                                     it->output.data() + it->sent,
                                     int((std::min)(size_t(65536), it->output.size() - it->sent)),
                                     0);
                    if (count > 0)
                    {
                        it->sent += size_t(count);
                        if (it->sent == it->output.size())
                        {
                            it->output.clear();
                            it->sent = 0;
                            close = !it->stream;
                        }
                    }
                    else if (count == 0 || WSAGetLastError() != WSAEWOULDBLOCK)
                    {
                        close = true;
                    }
                }
                close =
                    close || ((!it->handled || !it->output.empty()) && Clock::now() > it->deadline);
                if (close)
                {
                    closesocket(it->socket);
                    it = clients.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }
};

MonitorServer::MonitorServer(const std::filesystem::path& assets, std::string quotedSource)
    : impl_(std::make_unique<Impl>())
{
    impl_->source = std::move(quotedSource);
    impl_->Start(assets);
}

MonitorServer::~MonitorServer() = default;

void MonitorServer::Update(std::string snapshot, std::string state)
{
    if (snapshot.size() > 8 * 1024 * 1024)
    {
        throw std::runtime_error("Monitor snapshot exceeds 8 MiB.");
    }
    std::lock_guard<std::mutex> guard(impl_->mutex);
    impl_->snapshot = std::move(snapshot);
    impl_->state = std::move(state);
    impl_->captured = Clock::now();
}
