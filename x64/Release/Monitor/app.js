'use strict';

const $ = id => document.getElementById(id);
const fmt = value => value == null || !Number.isFinite(Number(value)) ? '—' : Number(value).toLocaleString('ko-KR', { maximumFractionDigits: 1 });
const kindName = { citizen: '동적 주민', resident: '안내 주민', soul: '영혼 NPC' };
let snapshot = null;
let envelope = null;
let receivedAt = 0;
let connected = false;
let selected = null;
let page = 0;
let identity = '';
let transportError = '';
const pageSize = 30;

function element(tag, text, className) {
  const result = document.createElement(tag);
  if (text != null) result.textContent = String(text);
  if (className) result.className = className;
  return result;
}

function villageName(id) {
  if (id == null || id < 0) return '야외 / 소속 없음';
  return snapshot?.villages.find(village => village.id === id)?.name ?? '알 수 없음';
}

function metric(label, value, note) {
  const card = element('article', null, 'metric');
  card.append(element('div', label, 'label'), element('div', fmt(value), 'value'), element('small', note));
  return card;
}

function renderSummary() {
  const s = snapshot.summary;
  $('summary').replaceChildren(
    metric('존재하는 NPC', s.npcCount, `동적 ${fmt(s.citizensAlive)} · 안내 ${fmt(s.residents)} · 영혼 ${fmt(s.souls)}`),
    metric('생존 몬스터 · 관리 중', s.monsters, `로드된 위치 ${fmt(s.loadedMonsters)} · 마을 권역 밖 ${fmt(s.outsideMonsters)}`),
    metric('사망 주민 · 교체 대기', s.citizensDead, '현재 사망 상태 · 누적 사망 횟수가 아님'),
    metric('시뮬레이션 경과 (초)', snapshot.simulationSeconds, `LOD 근거리 ${fmt(s.lod[0])} / 중거리 ${fmt(s.lod[1])} / 원거리 ${fmt(s.lod[2])}`)
  );
  const cards = snapshot.villages.map(village => {
    const card = element('article', null, 'village');
    card.append(element('h3', village.name));
    const facts = element('div', null, 'facts');
    for (const [label, value] of [
      ['소속 생존 / 사망', `${fmt(village.homeAlive)} / ${fmt(village.homeDead)}`],
      ['현재 체류 · 모든 NPC', fmt(village.presentCitizens + village.residents + village.souls)],
      ['주변 몬스터', fmt(village.monsters)], ['소속 주민 금화', fmt(village.gold)],
      ['소속 주민 재료', fmt(village.goods)], ['평균 체력 비율', `${fmt(village.averageHealthPercent)}%`],
      ['적대 / 중상 주민', `${fmt(village.hostile)} / ${fmt(village.wounded)}`],
      ['누적 깨달음 합계', fmt(village.insight)], ['지역 자원 (몹 수 아님)', fmt(village.resourceStock)]
    ]) {
      const fact = element('div');
      fact.append(element('span', label), element('strong', value));
      facts.append(fact);
    }
    card.append(facts, element('div', village.actions.map((count, index) => count ? `${snapshot.actionNames[index]} ${count}` : '').filter(Boolean).join(' · ') || '활동 중인 소속 주민 없음', 'activity'));
    return card;
  });
  $('villages').replaceChildren(...cards);
  $('news').replaceChildren(...(snapshot.news.length ? snapshot.news.map(news => element('li', news)) : [element('li', '최근 사건 없음')]));
  const types = s.monsterKinds;
  $('meta').textContent = `허스크 ${fmt(types[0])} · 하운드 ${fmt(types[1])} · 보스 ${fmt(types[2])}\n청크 ${fmt(s.chunks)} · 대기 ${fmt(s.pendingChunks)} · 시드 ${snapshot.seed}`;
  const previous = $('village').value;
  $('village').replaceChildren(new Option('전체 마을', ''), ...snapshot.villages.map(v => new Option(v.name, String(v.id))));
  $('village').value = previous;
}

function filteredNpcs() {
  const search = $('search').value.trim().toLocaleLowerCase();
  const kind = $('kind').value;
  const home = $('village').value;
  const life = $('life').value;
  const rows = snapshot.npcs.filter(npc =>
    (!search || `${npc.name} ${npc.id}`.toLocaleLowerCase().includes(search)) &&
    (!kind || npc.kind === kind) &&
    (home === '' || (npc.kind === 'citizen' ? npc.homeVillage : npc.insideVillage) === Number(home)) &&
    (!life || (life === 'alive' && npc.alive) || (life === 'dead' && !npc.alive) || (life === 'hostile' && npc.hostility > 0))
  );
  const sort = $('sort').value;
  rows.sort((a, b) => {
    if (sort === 'hp') return (a.maxHp ? a.hp / a.maxHp : 2) - (b.maxHp ? b.hp / b.maxHp : 2);
    if (sort === 'gold' || sort === 'insight') return (b[sort] ?? -1) - (a[sort] ?? -1);
    return a.name.localeCompare(b.name, 'ko', { numeric: true });
  });
  return rows;
}

function renderNpcs() {
  if (!snapshot) return;
  const focusId = document.activeElement?.dataset.npc;
  const rows = filteredNpcs();
  const pages = Math.max(1, Math.ceil(rows.length / pageSize));
  page = Math.min(page, pages - 1);
  const fragment = document.createDocumentFragment();
  for (const npc of rows.slice(page * pageSize, (page + 1) * pageSize)) {
    const row = element('tr', null, `${npc.id === selected ? 'selected ' : ''}${npc.alive ? '' : 'dead'}`);
    const name = element('td');
    const button = element('button', npc.name);
    button.dataset.npc = npc.id;
    name.append(button, element('small', `${kindName[npc.kind] ?? npc.kind} · ${npc.id}`));
    const action = element('td', npc.tendency ?? '해당 없음');
    action.append(element('small', npc.alive ? npc.action : `교체까지 ${fmt(npc.replacementIn)}초`));
    const village = element('td', villageName(npc.homeVillage));
    village.append(element('small', `${villageName(npc.insideVillage)} · (${fmt(npc.position.x)}, ${fmt(npc.position.y)})`));
    const health = element('td', npc.maxHp ? `${fmt(npc.hp)} / ${fmt(npc.maxHp)} · Lv ${fmt(npc.level)}` : '체력 모델 없음');
    if (npc.maxHp) {
      const bar = element('progress');
      bar.max = npc.maxHp;
      bar.value = npc.hp;
      bar.setAttribute('aria-label', `${npc.name} 체력`);
      health.append(bar);
    }
    const status = element('td', npc.alive ? npc.hostility > 0 ? '적대' : '중립' : '사망', npc.hostility > 0 ? 'danger' : '');
    status.append(element('small', npc.kind === 'citizen' ? `LOD ${npc.lod} · ${npc.loaded ? '로드됨' : '원거리'}` : npc.loaded ? '로드됨' : '기록 보존'));
    row.append(name, action, village, health, element('td', `${fmt(npc.gold)} / ${fmt(npc.goods)}`), element('td', fmt(npc.insight)), status);
    fragment.append(row);
  }
  if (!rows.length) {
    const row = element('tr');
    const cell = element('td', '조건에 맞는 NPC가 없습니다.', 'empty');
    cell.colSpan = 7;
    row.append(cell); fragment.append(row);
  }
  $('npcs').replaceChildren(fragment);
  if (focusId) [...$('npcs').querySelectorAll('button')].find(button => button.dataset.npc === focusId)?.focus({ preventScroll: true });
  $('result-count').textContent = `${rows.length}명 / 전체 기록 ${snapshot.npcs.length}명`;
  $('page').textContent = `${page + 1} / ${pages}`;
  $('prev').disabled = page === 0;
  $('next').disabled = page >= pages - 1;
  renderDetail();
}

function renderDetail() {
  const panel = $('detail');
  panel.hidden = !selected;
  if (!selected) return;
  const npc = snapshot.npcs.find(n => n.id === selected);
  const title = element('div', null, 'section-title');
  title.append(element('h2', npc ? `${npc.name} · 상세 관찰` : '기록에서 사라진 NPC'));
  const close = element('button', '닫기');
  close.addEventListener('click', () => { selected = null; renderNpcs(); });
  title.append(close);
  panel.replaceChildren(title);
  if (!npc) { panel.append(element('p', '사망 후 인구 교체 또는 플레이 초기화로 이전 ID가 제거되었습니다.')); return; }
  const grid = element('div', null, 'detail-grid');
  const info = element('div');
  info.append(element('p', `현재 목표: ${npc.goal ?? '—'}`));
  info.append(element('p', `현재 행동: ${npc.action} · 성향: ${npc.tendency ?? '별도 행동 AI 없음'}`));
  if (npc.kind === 'citizen') {
    info.append(element('p', `목적지 (${fmt(npc.destination.x)}, ${fmt(npc.destination.y)}) · 대상 ${['없음', '몬스터', '주민', '플레이어'][npc.targetKind]} #${npc.targetId}`));
    info.append(element('p', `경험치 ${fmt(npc.xp)} · 적대 유지 ${fmt(npc.hostility)}초`));
    const traits = ['공격성', '탐욕', '사회성', '용기', '도덕성', '호기심'];
    info.append(element('p', traits.map((name, i) => `${name} ${fmt(npc.personality[i])}`).join(' · '), 'subtle'));
    info.append(element('p', '선택 가중치는 마지막 의사결정 시점의 값입니다. 경험 편향은 보상·피해·시간 감쇠로 변하며, 확률(%)이나 고정 직업이 아닙니다.', 'subtle'));
    const weights = element('div', null, 'weights');
    snapshot.actionNames.forEach((name, i) => {
      const item = element('div', name, 'weight');
      item.append(element('small', `선택 ${fmt(npc.weights[i])} / 경험 ${fmt(npc.learned[i])}`));
      weights.append(item);
    });
    grid.append(info, weights);
  } else {
    info.append(element('p', `대화 횟수 ${fmt(npc.meetings)} · 독립 체력/경제/가중치 모델은 없습니다.`, 'subtle'));
    grid.append(info);
  }
  panel.append(grid);
}

function drawMap() {
  if (!snapshot) return;
  const canvas = $('map');
  const ratio = window.devicePixelRatio || 1;
  const width = Math.max(200, canvas.clientWidth), height = 240;
  canvas.width = Math.round(width * ratio); canvas.height = Math.round(height * ratio);
  const ctx = canvas.getContext('2d');
  ctx.scale(ratio, ratio);
  const living = snapshot.npcs.filter(n => n.alive);
  const points = [...living, ...snapshot.monsters, ...snapshot.villages, snapshot.player].map(n => n.position).filter(p => Number.isFinite(p?.x) && Number.isFinite(p?.y));
  if (!points.length) return;
  let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
  for (const p of points) { minX = Math.min(minX, p.x); maxX = Math.max(maxX, p.x); minY = Math.min(minY, p.y); maxY = Math.max(maxY, p.y); }
  const scale = Math.min((width - 64) / Math.max(12, maxX - minX), (height - 50) / Math.max(12, maxY - minY));
  const project = p => [width / 2 + (p.x - (minX + maxX) / 2) * scale, height / 2 + (p.y - (minY + maxY) / 2) * scale];
  ctx.font = '12px sans-serif';
  for (const v of snapshot.villages) {
    const [x, y] = project(v.position);
    const radius = v.radius ?? 4.5;
    ctx.strokeStyle = '#7c7450'; ctx.strokeRect(x - scale * radius, y - scale * radius, scale * radius * 2, scale * radius * 2);
    ctx.fillStyle = '#d6ba79'; ctx.fillText(v.name, x + 7, y - 10);
  }
  for (const mob of snapshot.monsters) {
    const [x, y] = project(mob.position);
    ctx.fillStyle = '#d58a73'; ctx.beginPath(); ctx.moveTo(x, y - 3); ctx.lineTo(x + 3, y); ctx.lineTo(x, y + 3); ctx.lineTo(x - 3, y); ctx.closePath(); ctx.fill();
  }
  for (const npc of living) {
    const [x, y] = project(npc.position);
    ctx.fillStyle = npc.id === selected ? '#fff' : npc.kind === 'soul' ? '#9cbae8' : npc.hostility > 0 ? '#f37b77' : '#80c5a2';
    ctx.beginPath(); ctx.arc(x, y, npc.id === selected ? 5 : 3, 0, Math.PI * 2); ctx.fill();
  }
  const [x, y] = project(snapshot.player.position);
  ctx.strokeStyle = '#fff'; ctx.beginPath(); ctx.moveTo(x - 6, y); ctx.lineTo(x + 6, y); ctx.moveTo(x, y - 6); ctx.lineTo(x, y + 6); ctx.stroke();
}

function updateStatus() {
  const age = envelope?.ageSeconds == null ? null : envelope.ageSeconds + (Date.now() - receivedAt) / 1000;
  let state = !connected ? 'disconnected' : envelope?.state ?? 'waiting';
  if (state === 'live' || state === 'paused' || state === 'blocked') {
    if (age > 5) state = 'stale';
  }
  const labels = { live: '실시간 수신 중', paused: '게임 일시정지', blocked: '게임 오류로 중단', stopped: '게임 종료', stale: '데이터 갱신 지연', waiting: '게임 데이터 대기', error: 'JSON 읽기 오류', disconnected: '서버 연결 대기 / 재연결' };
  $('status').textContent = labels[state] ?? state;
  $('status').className = state === 'live' ? 'live' : 'problem';
  $('freshness').textContent = age == null ? '아직 수신한 스냅샷 없음' : `마지막 게임 갱신 ${fmt(age)}초 전 · #${snapshot?.sequence ?? '—'}`;
  $('warning').hidden = state === 'live' && !transportError;
  const notes = { waiting: '게임의 첫 상태 정보를 기다리고 있습니다. 게임 콘솔에서 모니터링 상태를 확인해 주세요.', stale: '게임 스냅샷이 5초 넘게 갱신되지 않았습니다. 아래 수치는 마지막 수신 상태이며 실시간 값이 아닙니다.', stopped: '게임이 종료되었습니다. 마지막 상태를 유지해서 보여줍니다.', paused: 'P 키로 일시정지한 상태입니다. 연결과 JSON 수신은 계속됩니다.', blocked: '게임 저장/월드 오류로 시뮬레이션이 중단되었습니다.', disconnected: '게임을 실행하면 서버가 자동으로 시작됩니다. 콘솔의 접속 주소를 확인하세요. 자동 재연결 중이며 아래 수치는 마지막 수신 상태입니다.', error: 'JSON 읽기 또는 형식 오류입니다. 마지막 정상 데이터를 유지합니다.' };
  const errors = snapshot?.errors ? Object.values(snapshot.errors).filter(Boolean).join(' / ') : '';
  $('warning').textContent = [notes[state], envelope?.error, transportError, errors].filter(Boolean).join(' ');
}

for (const id of ['search', 'kind', 'village', 'life', 'sort']) {
  $(id).addEventListener(id === 'search' ? 'input' : 'change', () => { page = 0; renderNpcs(); });
}
$('prev').addEventListener('click', () => { page = Math.max(0, page - 1); renderNpcs(); });
$('next').addEventListener('click', () => { ++page; renderNpcs(); });
$('npcs').addEventListener('click', event => {
  const button = event.target.closest('button[data-npc]');
  if (!button) return;
  selected = button.dataset.npc; renderNpcs(); drawMap();
  $('detail').scrollIntoView({ behavior: 'smooth', block: 'nearest' });
});
$('download').addEventListener('click', () => {
  if (!snapshot) return;
  const url = URL.createObjectURL(new Blob([JSON.stringify(snapshot, null, 2)], { type: 'application/json' }));
  const link = element('a'); link.href = url; link.download = `world-${snapshot.session}-${snapshot.sequence}.json`; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
});
window.addEventListener('resize', drawMap);
setInterval(updateStatus, 1000);

if (location.protocol === 'file:') {
  transportError = '게임을 실행한 뒤 콘솔의 [Monitor HTTP] 주소로 접속하세요. 기본 주소는 http://127.0.0.1:8765 입니다.';
  updateStatus();
} else {
  const stream = new EventSource('/events');
  stream.onopen = () => { connected = true; transportError = ''; updateStatus(); };
  stream.onerror = () => { connected = false; updateStatus(); };
  stream.addEventListener('snapshot', event => {
    try {
      const message = JSON.parse(event.data);
      if (message.type !== 'monitor.snapshot') throw new Error('알 수 없는 메시지 형식');
      connected = true; transportError = ''; envelope = message; receivedAt = Date.now();
      const data = message.data;
      if (data && (data.schemaVersion !== 1 || !Array.isArray(data.npcs))) throw new Error('지원하지 않는 스냅샷');
      if (data && `${message.source}/${data.session}/${data.sequence}` !== identity) {
        if (snapshot && (snapshot.session !== data.session || snapshot.simulationSeconds > data.simulationSeconds)) {
          selected = null; page = 0;
        }
        snapshot = data;
        identity = `${message.source}/${data.session}/${data.sequence}`;
        renderSummary(); renderNpcs(); drawMap();
        $('download').disabled = false;
      }
      if (!data && snapshot) {
        snapshot = null; selected = null; identity = ''; page = 0;
        $('summary').replaceChildren();
        $('villages').replaceChildren(element('div', '새 데이터 수신 대기 중', 'empty'));
        $('npcs').replaceChildren();
        $('news').replaceChildren(element('li', '수신 대기 중'));
        $('meta').textContent = '';
        $('result-count').textContent = '0명';
        $('detail').hidden = true;
        $('download').disabled = true;
        $('map').getContext('2d').clearRect(0, 0, $('map').width, $('map').height);
      }
      $('source').textContent = message.source ?? '데이터 경로 대기 중';
      updateStatus();
    } catch (error) {
      transportError = `메시지 처리 오류: ${error.message}`;
      updateStatus();
    }
  });
}
