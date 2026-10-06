const player = document.querySelector('#player');
const name = document.querySelector('#name');
const status = document.querySelector('#status');
const setStatus = (text) => { status.textContent = text; };
async function loadConfig() {
  try {
    const config = await (await fetch('./config.json', { cache: 'no-store' })).json();
    name.textContent = config.name || 'No package selected';
    if (config.packageUrl) player.src = config.packageUrl;
  } catch (error) { setStatus(`Waiting for package: ${error.message}`); }
}
player.addEventListener('ready', () => setStatus('Ready'));
player.addEventListener('progress', (event) => { if (player.state === 'ready') return; const p = event.detail; setStatus(p?.fraction == null ? `Loading ${p?.phase || 'package'}…` : `Loading ${Math.round(p.fraction * 100)}%`); });
player.addEventListener('resize', () => {});
player.addEventListener('error', (event) => setStatus(`Error: ${event.detail?.message || 'Unable to load package'}`));
document.querySelector('#reload').addEventListener('click', () => { player.removeAttribute('src'); location.reload(); });
loadConfig();
