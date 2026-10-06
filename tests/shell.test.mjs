import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

test('player shell uses local assets with learner resume disabled', async () => {
  const html = await fs.readFile(new URL('../web/index.html', import.meta.url), 'utf8');
  assert.match(html, /sw="assets\/h5p-sw\.js"/);
  assert.match(html, /assets-base="assets\/frame-assets\/"/);
  assert.match(html, /resume="off"/);
  assert.match(html, /id="reload"/);
  assert.doesNotMatch(html, /hub|https?:\/\//i);
});
