// SPDX-License-Identifier: GPL-3.0-or-later
import assert from 'node:assert/strict';
import {compare, rows, header} from './bridge.mjs';

const c = {id: 0, separate: false, mutation: 0, accepted: true,
  status: [0, 0, 0, 0], block: [128, 1, 0, 18, 1, 0, ...Array(16).fill(0)], advertised: 22};
const frame = Buffer.alloc(38);
frame.writeUInt16BE(22, 14);
Buffer.from(c.block).copy(frame, 16);
const a = {scope: 'parser', id: 0, frame_hex: frame.toString('hex'), block_offset: 16,
  first_callbacks: 1, first_sequence: 0, first_tacks: 1,
  callbacks: 1, status: c.status, rejected_without_effects: true,
  duplicate_suppressed: true, next_sent: true};
assert.deepEqual(compare([c], [a], 'parser'), []);
assert(compare([c], [{...a, status: [1, 0, 0, 0]}], 'parser').length > 0);
assert(compare([c], [{...a, first_sequence: 65535}], 'parser').length > 0);
assert.throws(() => compare([c], [], 'parser'));
assert.throws(() => compare([c, {...c, id: 1}], [a, a], 'parser'));
assert.throws(() => compare([c], [{...a, frame_hex: frame.toString('hex') + 'ff'}], 'parser'));
assert.throws(() => compare([c], [{...a, scope: 'recovery'}], 'parser'));
assert.throws(() => rows('ALARM_MODEL_JSON ' + JSON.stringify(a) + '\nALARM_MODEL_JSON ' + JSON.stringify(a), 'parser'));
assert.match(header([c], 'parser'), /\{0,\{0,0,0,0\},false,0,true\}/);
const recovery = {id: 0, separate: false, status: c.status, send_result: -1,
  first_callbacks: 1, retry_callbacks: 1, next_sequence: 0};
const tack = Buffer.alloc(32);
const observed = {scope: 'recovery', id: 0, frame_hex: frame.toString('hex'),
  block_offset: 16, tack_hex: tack.toString('hex'), first_send_result: -1,
  attempted_tacks: 3, retry_send_results: [32, 32], first_callbacks: 1,
  retry_callbacks: 1, callbacks: 1, first_sequence: 0, status: c.status,
  next_sent: true};
assert.deepEqual(compare([recovery], [observed], 'recovery'), []);
assert(compare([recovery], [{...observed, first_callbacks: 0}], 'recovery').length > 0);
assert(compare([recovery], [{...observed, retry_callbacks: 2}], 'recovery').length > 0);
assert.throws(() => compare([recovery], [{...observed, first_send_result: 0}], 'recovery'));
assert.throws(() => compare([recovery], [{...observed, retry_send_results: [-1, 32]}], 'recovery'));
assert.throws(() => compare([recovery], [{...observed, attempted_tacks: 2}], 'recovery'));
console.log('PASS: seven parser and five recovery corruption controls');
