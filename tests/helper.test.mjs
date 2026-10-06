import test, { after } from 'node:test';
import os from 'node:os';
import { metadataZip } from './fixture.mjs';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import http from 'node:http';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..');
const fixtureRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'acs-h5p-test-'));
after(() => fs.rm(fixtureRoot, { recursive: true, force: true }));
const packagePath = path.join(fixtureRoot, 'fixture.h5p');
const unicodePath = path.join(fixtureRoot, 'helper-unicode-測試.h5p');
const corruptPath = path.join(fixtureRoot, 'helper-corrupt.h5p');
await fs.writeFile(packagePath, metadataZip());
await fs.copyFile(packagePath, unicodePath);
await fs.writeFile(corruptPath, Buffer.from('not a zip archive'));

function launch() {
  const child = spawn(process.execPath, ['helper/server.mjs'], { cwd: root, stdio: ['pipe', 'pipe', 'pipe'] });
  let buffer = '';
  const queue = [];
  const pending = [];
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', chunk => { buffer += chunk; let i; while ((i = buffer.indexOf('\n')) >= 0) { const line = buffer.slice(0, i).trim(); buffer = buffer.slice(i + 1); if (!line) continue; const value = JSON.parse(line); const waiter = queue.shift(); if (waiter) waiter(value); else pending.push(value); } });
  const next = (timeout = 5000) => pending.length ? Promise.resolve(pending.shift()) : new Promise((resolve, reject) => { const timer = setTimeout(() => reject(new Error('Timed out waiting for helper response')), timeout); queue.push(value => { clearTimeout(timer); resolve(value); }); });
  const send = async command => { child.stdin.write(`${JSON.stringify(command)}\n`); return next(); };
  return { child, next, send };
}
function rawStatus(url, headers) { return new Promise((resolve, reject) => { const target = new URL(url); const request = http.request({ hostname: target.hostname, port: target.port, path: target.pathname + target.search, headers }, response => { response.resume(); response.once('end', () => resolve(response.statusCode)); }); request.once('error', reject); request.end(); }); }

test('helper exposes tokenized package with ranges and rejects stale shell', async t => {
  const h = launch(); t.after(() => h.child.kill());
  const ready = await h.next(); assert.equal(ready.type, 'ready');
  const initialConfig = await fetch(`${ready.url.replace('/index.html', '/config.json')}`);
  assert.equal(initialConfig.status, 200); assert.equal((await initialConfig.json()).packageUrl, null);
  const loaded = await h.send({ cmd: 'load', path: packagePath, generation: 1 });
  assert.equal(loaded.type, 'loaded');
  const packageUrl = new URL(loaded.url); packageUrl.pathname = packageUrl.pathname.replace('index.html', 'package.h5p');
  const range = await fetch(packageUrl, { headers: { Range: 'bytes=0-9' } });
  assert.equal(range.status, 206); assert.equal((await range.arrayBuffer()).byteLength, 10);
  const suffix = await fetch(packageUrl, { headers: { Range: 'bytes=-7' } });
  assert.equal(suffix.status, 206); assert.equal((await suffix.arrayBuffer()).byteLength, 7);
  const badRange = await fetch(packageUrl, { headers: { Range: 'bytes=999999999-' } });
  assert.equal(badRange.status, 416); assert.match(badRange.headers.get('content-range') || '', /^bytes \*\//);
  const oldShell = await fetch(loaded.url); assert.equal(oldShell.status, 200);
  const next = await h.send({ cmd: 'load', path: packagePath, generation: 2 });
  assert.equal(next.type, 'loaded'); assert.equal((await fetch(loaded.url)).status, 404);
  const clear = await h.send({ cmd: 'clear', generation: 3 }); assert.equal(clear.type, 'cleared');
  assert.equal((await fetch(next.url)).status, 404);
});

test('helper enforces Host and Origin and supports Unicode paths and independent instances', async t => {
  const a = launch(); const b = launch(); t.after(() => { a.child.kill(); b.child.kill(); });
  const [ra, rb] = await Promise.all([a.next(), b.next()]);
  const loaded = await a.send({ cmd: 'load', path: unicodePath, generation: 1 }); assert.equal(loaded.type, 'loaded');
  const good = await fetch(loaded.url); assert.equal(good.status, 200);
  assert.equal(await rawStatus(loaded.url, { Host: 'evil.example' }), 403);
  assert.equal((await fetch(loaded.url, { headers: { Origin: 'http://evil.example' } })).status, 403);
  const bBefore = await fetch(rb.url); assert.equal(bBefore.status, 200);
  const bLoad = await b.send({ cmd: 'load', path: unicodePath, generation: 1 }); assert.equal(bLoad.type, 'loaded');
  assert.notEqual(new URL(loaded.url).pathname, new URL(bLoad.url).pathname);
  assert.equal((await fetch(ra.url)).status, 404);
});

test('corrupt archive returns an error and helper remains alive; load invalidates immediately', async t => {
  const h = launch(); t.after(() => h.child.kill()); const ready = await h.next();
  const first = await h.send({ cmd: 'load', path: packagePath, generation: 1 }); assert.equal(first.type, 'loaded');
  h.child.stdin.write(JSON.stringify({ cmd: 'load', path: corruptPath, generation: 2 }) + '\n');
  assert.equal((await fetch(first.url)).status, 404);
  const error = await h.next(); assert.equal(error.type, 'error'); assert.equal(error.generation, 2);
  const stillLive = await fetch(ready.url); assert.equal(stillLive.status, 404);
  const recovered = await h.send({ cmd: 'load', path: unicodePath, generation: 3 });
  assert.equal(recovered.type, 'loaded');
});

test('helper reports invalid paths and stops on EOF', async t => {
  const h = launch(); t.after(() => h.child.kill());
  await h.next();
  const error = await h.send({ cmd: 'load', path: path.dirname(packagePath), generation: 7 });
  assert.equal(error.type, 'error'); assert.equal(error.generation, 7);
  h.child.stdin.end();
  await new Promise(resolve => h.child.once('exit', resolve));
});
