import http from 'node:http';
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const webRoot = path.join(root, 'web');
const mime = { '.html':'text/html; charset=utf-8', '.js':'text/javascript; charset=utf-8', '.css':'text/css; charset=utf-8', '.json':'application/json; charset=utf-8', '.webmanifest':'application/manifest+json', '.png':'image/png', '.svg':'image/svg+xml', '.woff':'font/woff', '.woff2':'font/woff2', '.txt':'text/plain; charset=utf-8' };
let generation = 0;
let current = null;
const responses = new Set();
const invalidate = () => { current = null; for (const res of responses) res.destroy(); };

const out = value => process.stdout.write(`${JSON.stringify(value)}\n`);
const fail = (gen, message) => out({ type: 'error', generation: Number.isInteger(gen) ? gen : undefined, message: String(message) });
const safeJoin = (base, relative) => { const resolved = path.resolve(base, relative); return resolved === base || resolved.startsWith(`${base}${path.sep}`) ? resolved : null; };
const tokenFor = () => crypto.randomBytes(18).toString('base64url');
const MAX_CENTRAL = 32 * 1024 * 1024, MAX_METADATA = 1 * 1024 * 1024;

async function packageMetadata(file) {
  const handle = await fsp.open(file, 'r');
  try {
    const stat = await handle.stat();
    const tailLength = Math.min(stat.size, 65557);
    const tail = Buffer.alloc(tailLength);
    await handle.read(tail, 0, tailLength, stat.size - tailLength);
    const eocd = tail.lastIndexOf(Buffer.from([0x50,0x4b,0x05,0x06]));
    if (eocd < 0 || eocd + 22 > tail.length) throw new Error('Not a valid H5P ZIP archive');
    if (tail.readUInt16LE(eocd + 4) || tail.readUInt16LE(eocd + 6) || tail.readUInt16LE(eocd + 10) === 65535) throw new Error('Split and ZIP64 archives are not supported by this prototype');
    const centralSize = tail.readUInt32LE(eocd + 12), centralOffset = tail.readUInt32LE(eocd + 16);
    if (centralSize > MAX_CENTRAL || centralOffset === 0xffffffff || centralOffset + centralSize > stat.size - tailLength + eocd) throw new Error('Invalid or unsupported ZIP central directory');
    const central = Buffer.alloc(centralSize);
    if ((await handle.read(central, 0, centralSize, centralOffset)).bytesRead !== centralSize) throw new Error('Truncated ZIP central directory');
    let p = 0;
    while (p + 46 <= central.length && central.readUInt32LE(p) === 0x02014b50) {
      const method = central.readUInt16LE(p + 10), compressed = central.readUInt32LE(p + 20), nameLen = central.readUInt16LE(p + 28), extraLen = central.readUInt16LE(p + 30), commentLen = central.readUInt16LE(p + 32), localOffset = central.readUInt32LE(p + 42);
      if (p + 46 + nameLen + extraLen + commentLen > central.length) throw new Error('Invalid ZIP central directory entry');
      const name = central.subarray(p + 46, p + 46 + nameLen).toString('utf8');
      if (name === 'h5p.json') {
        if (compressed > MAX_METADATA || localOffset + 30 > stat.size) throw new Error('h5p.json is too large or outside the archive');
        const local = Buffer.alloc(30); const localRead = await handle.read(local, 0, 30, localOffset); if (localRead.bytesRead !== 30 || local.readUInt32LE(0) !== 0x04034b50) throw new Error('Invalid h5p.json local header');
        const localNameLen = local.readUInt16LE(26), localExtraLen = local.readUInt16LE(28);
        const dataOffset = localOffset + 30 + localNameLen + localExtraLen; if (dataOffset + compressed > stat.size) throw new Error('h5p.json data is outside the archive');
        const body = Buffer.alloc(compressed); const bodyRead = await handle.read(body, 0, compressed, dataOffset); if (bodyRead.bytesRead !== compressed) throw new Error('Truncated h5p.json');
        const json = method === 0 ? body : method === 8 ? zlib.inflateRawSync(body, { maxOutputLength: MAX_METADATA }) : null;
        if (!json) throw new Error('Unsupported h5p.json compression');
        return JSON.parse(json.toString('utf8'));
      }
      p += 46 + nameLen + extraLen + commentLen;
    }
    throw new Error('H5P package is missing h5p.json');
  } finally { await handle.close(); }
}

async function loadPackage(file, gen) {
  if (typeof file !== 'string' || !path.isAbsolute(file)) throw new Error('An absolute H5P file path is required');
  const absolute = path.resolve(file);
  const stat = await fsp.stat(absolute);
  if (!stat.isFile() || path.extname(absolute).toLowerCase() !== '.h5p') throw new Error('Selected path must be a regular .h5p file');
  const metadata = await packageMetadata(absolute);
  if (gen !== generation) return;
  const token = tokenFor();
  current = { file: absolute, token, generation: gen, name: path.basename(absolute), metadata };
  out({ type: 'loaded', generation: gen, url: `http://127.0.0.1:${server.address().port}/${token}/index.html?generation=${gen}`, name: current.name });
}

function requestAllowed(req, port) {
  const host = req.headers.host;
  const origin = req.headers.origin;
  return (host === `127.0.0.1:${port}` || host === `localhost:${port}`) && (!origin || origin === `http://127.0.0.1:${port}` || origin === `http://localhost:${port}`);
}
async function sendFile(req, res, file, range) {
  if (!file) { res.writeHead(404).end('No package selected'); return; }
  const handle = await fsp.open(file, 'r');
  let handedOff = false;
  try {
  const stat = await handle.stat(), size = stat.size;
  if (!stat.isFile()) throw new Error('Not a regular file');
  let start = 0, end = size - 1, status = 200;
  if (range) {
    const match = /^bytes=(\d*)-(\d*)$/.exec(range);
    if (!match) { res.writeHead(416, { 'Content-Range': `bytes */${size}` }); return res.end(); }
    if (size === 0) { res.writeHead(416, { 'Content-Range': 'bytes */0' }); return res.end(); }
    if (match[1] === '') { const suffix = Number(match[2]); if (!suffix) { res.writeHead(416, { 'Content-Range': `bytes */${size}` }); return res.end(); } start = Math.max(0, size - suffix); end = size - 1; }
    else { start = Number(match[1]); end = match[2] ? Number(match[2]) : size - 1; }
    if (!Number.isSafeInteger(start) || !Number.isSafeInteger(end) || start >= size || end < start) { res.writeHead(416, { 'Content-Range': `bytes */${size}` }); return res.end(); }
    end = Math.min(end, size - 1); status = 206;
  }
  const headers = { 'Content-Type': mime[path.extname(file).toLowerCase()] || 'application/octet-stream', 'Content-Length': end - start + 1, 'Accept-Ranges': 'bytes', 'Cache-Control': 'no-store' };
  if (status === 206) headers['Content-Range'] = `bytes ${start}-${end}/${size}`;
  if (res.destroyed) return;
  res.writeHead(status, headers); if (req.method === 'HEAD' || size === 0) return res.end();
  const stream = handle.createReadStream({ start, end, autoClose: true }); handedOff = true;
  stream.on('error', () => res.destroy()); res.on('close', () => stream.destroy()); stream.pipe(res);
  } finally { if (!handedOff) await handle.close(); }
}

const server = http.createServer(async (req, res) => {
  responses.add(res); res.on('close', () => responses.delete(res));
  try {
  const port = server.address()?.port;
  if (!requestAllowed(req, port)) return res.writeHead(403).end('Local clients only');
  if (!['GET', 'HEAD'].includes(req.method)) return res.writeHead(405, { Allow: 'GET, HEAD' }).end('Read only');
  let pathname; try { pathname = decodeURIComponent(new URL(req.url, `http://127.0.0.1:${port}`).pathname); } catch { return res.writeHead(400).end('Malformed URL'); }
  const parts = pathname.split('/').filter(Boolean);
  if (!current || parts[0] !== current.token) return res.writeHead(404).end('Not found');
  if (parts[1] === 'package.h5p' && parts.length === 2) return await sendFile(req, res, current.file, req.headers.range);
  if (parts[1] === 'config.json' && parts.length === 2) {
    const body = Buffer.from(JSON.stringify({ generation: current.generation, name: current.name, packageUrl: current.file ? `/${current.token}/package.h5p` : null }));
    res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8', 'Content-Length': body.length, 'Cache-Control': 'no-store' });
    return req.method === 'HEAD' ? res.end() : res.end(body);
  }
  const relative = parts.slice(1).join('/') || 'index.html';
  const allowed = relative === 'index.html' || relative === 'app.js' || relative === 'sw.js' || relative.startsWith('assets/');
  if (!allowed) return res.writeHead(404).end('Not found');
  const file = safeJoin(webRoot, relative);
  if (!file || !fs.existsSync(file) || fs.statSync(file).isDirectory()) return res.writeHead(404).end('Not found');
  const realWeb = fs.realpathSync(webRoot), realFile = fs.realpathSync(file);
  if (!realFile.startsWith(`${realWeb}${path.sep}`)) return res.writeHead(404).end('Not found');
  if (relative === 'config.json') { res.setHeader('Cache-Control', 'no-store'); }
  await sendFile(req, res, file, req.headers.range);
  } catch {
    if (!res.headersSent && !res.destroyed) res.writeHead(404).end('Resource unavailable'); else res.destroy();
  }
});

const shutdown = () => { server.closeAllConnections?.(); server.close(() => process.exit(0)); };
process.stdin.setEncoding('utf8'); let input = '';
process.stdin.on('data', chunk => {
  input += chunk;
  if (input.length > 262144) { fail(undefined, 'Command exceeds size limit'); shutdown(); return; }
  let i;
  while ((i = input.indexOf('\n')) >= 0) {
    const line = input.slice(0, i).trim(); input = input.slice(i + 1);
    if (!line) continue;
    let cmd; try { cmd = JSON.parse(line); if (!cmd || typeof cmd !== 'object') throw new Error(); } catch { fail(undefined, 'Invalid JSON command'); continue; }
    if (cmd.cmd === 'shutdown') { shutdown(); return; }
    if (cmd.cmd !== 'clear' && cmd.cmd !== 'load') { fail(cmd.generation, 'Unknown command'); continue; }
    const gen = Number.isSafeInteger(cmd.generation) ? cmd.generation : generation + 1;
    if (gen <= generation) { fail(gen, 'Stale generation'); continue; }
    generation = gen; invalidate();
    if (cmd.cmd === 'clear') out({ type: 'cleared', generation });
    else loadPackage(cmd.path, gen).catch(e => { if (gen === generation) fail(gen, e.message); });
  }
});
process.stdin.on('end', shutdown);
server.listen(0, '127.0.0.1', () => { const port = server.address().port; const token = tokenFor(); current = { token, generation: 0, file: null, name: null }; out({ type: 'ready', port, url: `http://127.0.0.1:${port}/${token}/index.html` }); });
