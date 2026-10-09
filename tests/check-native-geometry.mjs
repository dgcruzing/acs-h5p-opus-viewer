// Native self-test evidence, deliberately distinct from visual/RDP acceptance.
import { readFileSync } from 'node:fs';
import assert from 'node:assert/strict';
if (!process.argv[2]) throw new Error('Usage: node tests/check-native-geometry.mjs <native-log.jsonl>');
const file = process.argv[2];
const rows = readFileSync(file, 'utf8').trim().split(/\r?\n/).map(JSON.parse);
const geometry = rows.filter(r => r.event === 'geometry_sync');
assert.ok(rows.some(r => r.event === 'webview_ready'));
assert.ok(rows.some(r => r.event === 'package_loaded'));
assert.ok(rows.some(r => r.event === 'navigation_complete' && r.generation === 1 && r.success));
assert.equal(rows.at(-1).event, 'closed');
assert.ok(geometry.length > 0);
for (const r of geometry) {
  assert.equal(r.boundsMatch, true);
  assert.equal(r.boundsResult, 0);
  assert.equal(r.notifyResult, 0);
  assert.equal(r.visibilityResult, 0);
  assert.deepEqual(r.bounds.slice(0, 2), [0, 0]);
}
const initial = geometry[0];
const move = geometry.find(r => r.reason === 'window_position' && r.bounds[2] === 800);
assert.ok(move);
assert.deepEqual(move.bounds, initial.bounds);
assert.notDeepEqual(move.screenOrigin, initial.screenOrigin);
const ancestor = geometry.find(r => r.reason === 'watchdog');
assert.ok(ancestor);
assert.deepEqual(ancestor.bounds, move.bounds);
assert.notDeepEqual(ancestor.screenOrigin, move.screenOrigin);
assert.ok(geometry.some(r => r.bounds[2] === 640 && r.bounds[3] === 360));
for (const reason of ['dpi', 'display', 'show']) assert.ok(geometry.some(r => r.reason === reason));
assert.ok(geometry.some(r => r.generation === 2 && r.visible === false));
const result = {
  pass: true, geometryEvents: geometry.length, dpi: initial.dpi,
  checks: ['real DLL exports', 'package navigation', 'client-relative bounds readback', 'child move without resize',
    'ancestor move detected by timer', 'Opus resize message', 'synthetic DPI/display events', 'hidden after clear', 'clean close'],
  limits: ['Hidden native window; no visual placement assertion.', 'Synthetic DPI/display events do not prove real DPI transitions.',
    'This automated check does not establish visual Opus remote-session acceptance.']
};

console.log(JSON.stringify(result, null, 2));
