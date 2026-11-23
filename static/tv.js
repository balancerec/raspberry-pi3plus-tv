// Interface de TV extraída do projeto original. Dependências de SPI, LCD, NRF e API
// administrativa removidas; entrada de teclado e Gamepad API mantida.
const focusables = () => [...document.querySelectorAll('.focusable,.nav[data-url],.nav[data-action]')]
  .filter(el => el.offsetParent !== null);
let focusIndex = 0;
let lastGamepad = {};
function setFocus(index) {
  const list = focusables();
  if (!list.length) return;
  focusIndex = (index + list.length) % list.length;
  list.forEach((el, i) => el.classList.toggle('focused', i === focusIndex));
  list[focusIndex].scrollIntoView({block: 'nearest', inline: 'nearest', behavior: 'smooth'});
}
function move(direction) {
  const list = focusables(), current = list[focusIndex];
  if (!current) return;
  const box = current.getBoundingClientRect(), cx = box.left + box.width / 2, cy = box.top + box.height / 2;
  let best = -1, score = Infinity;
  list.forEach((el, i) => {
    if (i === focusIndex) return;
    const r = el.getBoundingClientRect(), dx = r.left + r.width / 2 - cx, dy = r.top + r.height / 2 - cy;
    if (direction === 'LEFT' && dx >= -5 || direction === 'RIGHT' && dx <= 5 ||
        direction === 'UP' && dy >= -5 || direction === 'DOWN' && dy <= 5) return;
    const primary = direction === 'LEFT' || direction === 'RIGHT' ? Math.abs(dx) : Math.abs(dy);
    const cross = direction === 'LEFT' || direction === 'RIGHT' ? Math.abs(dy) : Math.abs(dx);
    const s = primary + cross * 2.4;
    if (s < score) { score = s; best = i; }
  });
  if (best >= 0) setFocus(best);
}
function activate() {
  const el = focusables()[focusIndex];
  if (!el) return;
  if (el.dataset.action === 'apps') {
    document.getElementById('apps').scrollIntoView({behavior: 'smooth'});
    setTimeout(() => setFocus(focusables().findIndex(x => x.classList.contains('app'))), 350);
    return;
  }
  if (el.dataset.url) window.location.href = el.dataset.url;
}
function input(event) {
  if (['LEFT', 'RIGHT', 'UP', 'DOWN'].includes(event)) move(event);
  else if (event === 'ENTER') activate();
  else if (event === 'BACK') { if (history.length > 1) history.back(); else setFocus(0); }
  else if (event === 'HOME') { scrollTo({top: 0, behavior: 'smooth'}); setFocus(0); }
}
document.addEventListener('keydown', e => {
  const map = {ArrowLeft:'LEFT', ArrowRight:'RIGHT', ArrowUp:'UP', ArrowDown:'DOWN', Enter:'ENTER', Escape:'BACK', Home:'HOME'};
  if (map[e.key]) { e.preventDefault(); input(map[e.key]); }
});
document.addEventListener('click', e => {
  const el = e.target.closest('[data-url],[data-action]');
  if (!el) return;
  e.preventDefault();
  focusIndex = focusables().indexOf(el);
  activate();
});
function getGamepad() {
  try { return [...(navigator.getGamepads?.() || [])].find(Boolean) || null; }
  catch { return null; }
}
function showGamepad(pad) {
  document.getElementById('pad-state').textContent = pad ? 'Controle conectado' : 'Sem controle';
  document.getElementById('pad-detail').textContent = pad ? (pad.id || 'Gamepad') : 'Conecte por USB ou Bluetooth';
}
function pollGamepad() {
  const pad = getGamepad();
  const map = {12:'UP', 13:'DOWN', 14:'LEFT', 15:'RIGHT', 0:'ENTER', 1:'BACK', 16:'HOME'};
  if (pad) Object.entries(map).forEach(([i, event]) => {
    const down = Boolean(pad.buttons[i]?.pressed);
    if (down && !lastGamepad[i]) input(event);
    lastGamepad[i] = down;
  });
  else lastGamepad = {};
  if (pollGamepad._lastId !== pad?.id) {
    showGamepad(pad);
    pollGamepad._lastId = pad?.id;
  }
  requestAnimationFrame(pollGamepad);
}
async function refresh() {
  try {
    const response = await fetch('/api/status', {cache:'no-store'});
    if (!response.ok) throw new Error('status indisponível');
    const d = await response.json();
    const sys = d.system || {}, wifi = d.connectivity?.wifi || {};
    document.getElementById('network').textContent = wifi.connected ? (wifi.ssid || 'WI-FI') : 'ETHERNET / LAN';
    document.getElementById('temperature').textContent = Number.isFinite(sys.temperature_c) ? sys.temperature_c.toFixed(1) + ' °C' : 'N/A';
    const cpu = Number.isFinite(sys.cpu_percent) ? Math.round(sys.cpu_percent) + '%' : 'N/A';
    document.getElementById('device-summary').textContent = `${sys.hostname || 'Raspberry'} · ${sys.ip_address || 'local'} · CPU ${cpu}`;
  } catch {
    document.getElementById('network').textContent = 'SEM DADOS';
    document.getElementById('device-summary').textContent = 'Servidor de status indisponível';
  }
}
function clock() {
  const now = new Date();
  document.getElementById('clock').textContent = now.toLocaleTimeString('pt-BR', {hour:'2-digit', minute:'2-digit'});
  document.getElementById('date').textContent = now.toLocaleDateString('pt-BR', {weekday:'long', day:'2-digit', month:'long'});
  const hour = now.getHours();
  document.getElementById('greeting').textContent = hour < 12 ? 'Bom dia.' : hour < 18 ? 'Boa tarde.' : 'Boa noite.';
}
setFocus(0);
clock();
showGamepad(null);
refresh();
pollGamepad();
setInterval(clock, 1000);
setInterval(refresh, 3000);
