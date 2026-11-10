#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// Fetch public, immutable inputs into a new directory. Never reuse dirty trees.
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdir} from 'node:fs/promises';
import {join, resolve} from 'node:path';

assert.equal(process.argv.length, 3, 'Usage: node fetch-inputs.mjs NEW_DIRECTORY');
const root = resolve(process.argv[2]);
await mkdir(root, {recursive: false});
function git(path, ...args) {
  const result = spawnSync('git', args, {cwd: path, stdio: 'inherit'});
  assert.equal(result.status, 0, 'git failed; retain partial directory for inspection');
}
const inputs = [
  ['source', 'https://github.com/rtlabs-com/p-net.git',
    '19782d52c56640526156f8db04efe93317f9c530'],
  ['dependencies/pnal', 'https://github.com/rtlabs-com/p-net.git',
    '3e650720850e1f31f8a419c8aa135a2e81b44ddb'],
  ['dependencies/osal', 'https://github.com/rtlabs-com/osal.git',
    '88784fcfff7a1b74e20bdeb71a55c2cdd22b4a59'],
  ['dependencies/gtest', 'https://github.com/google/googletest.git',
    '25808659d317cb03409e7949914b274e10e6824f'],
];
for(const [name, url, revision] of inputs) {
  const path = join(root, name);
  await mkdir(path, {recursive: true});
  git(path, 'init', '--quiet');
  git(path, 'remote', 'add', 'origin', url);
  git(path, 'fetch', '--depth=1', 'origin', revision);
  git(path, 'checkout', '--detach', '--quiet', 'FETCH_HEAD');
  const head = spawnSync('git', ['rev-parse', 'HEAD'], {cwd: path, encoding: 'utf8'});
  assert.equal(head.status, 0);
  assert.equal(head.stdout.trim(), revision);
}
console.log('Fetched pinned public inputs into ' + root);
