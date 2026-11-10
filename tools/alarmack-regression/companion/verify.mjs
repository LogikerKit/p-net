#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import {copyFile, lstat, mkdir, readFile, writeFile} from 'node:fs/promises';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {performance} from 'node:perf_hooks';
import {auditAxioms, compare, header, rows} from './bridge.mjs';

const root = dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
const option = (name, fallback) => {
  const i = args.indexOf(name);
  if(i < 0) return fallback;
  assert(i + 1 < args.length && !args[i + 1].startsWith('--'), `Missing ${name}`);
  return args[i + 1];
};
const scope = option('--scope', 'parser');
assert(['parser', 'recovery', 'both'].includes(scope));
const source = resolve(option('--source', '../source'));
const deps = resolve(option('--dependencies', '../dependencies'));
const build = resolve(option('--build', '../build'));
const evidence = resolve(option('--evidence', join(build, 'evidence', new Date().toISOString().replace(/[:.]/g, '-'))));
const lean = option('--lean-bin', 'lean');
const negative = args.includes('--expect-counterexample');
const pin = '19782d52c56640526156f8db04efe93317f9c530';
const pins = {
  pnal: '3e650720850e1f31f8a419c8aa135a2e81b44ddb',
  osal: '88784fcfff7a1b74e20bdeb71a55c2cdd22b4a59',
  gtest: '25808659d317cb03409e7949914b274e10e6824f',
};
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const started = performance.now();
const receipt = {started: new Date().toISOString(), status: 'RUNNING', scope,
  source, dependencies: deps, build, expectedCounterexample: negative,
  pins: {pnet: pin, ...pins}, phases: [], hashes: {}};
await mkdir(dirname(evidence), {recursive: true});
await mkdir(evidence, {recursive: false});

async function command(name, program, commandArgs, options = {}) {
  const start = performance.now();
  const result = await new Promise((done, reject) => {
    const child = spawn('prlimit', ['--as=4294967296', '--cpu=45:46', '--core=0', '--',
      program, ...commandArgs], {
      cwd: options.cwd ?? root,
      env: {...process.env, ...options.env},
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    const chunks = [];
    let bytes = 0;
    const timer = setTimeout(() => child.kill('SIGKILL'), options.timeout ?? 60000);
    const collect = chunk => {
      bytes += chunk.length;
      if(bytes > 12000000) child.kill('SIGKILL');
      else chunks.push(chunk);
    };
    child.stdout.on('data', collect);
    child.stderr.on('data', collect);
    child.on('error', error => {clearTimeout(timer); reject(error);});
    child.on('close', (code, signal) => {
      clearTimeout(timer);
      done({code, signal, text: Buffer.concat(chunks).toString('utf8')});
    });
  });
  await writeFile(join(evidence, name + '.log'), result.text);
  receipt.phases.push({name, seconds: (performance.now() - start) / 1000,
    exitCode: result.code, signal: result.signal, program, args: commandArgs});
  if(!options.allowFailure) assert.equal(result.code, 0, `${name} failed: ${join(evidence, name + '.log')}`);
  return result;
}

async function identity(path, revision, label, allowAlarmEdit = false) {
  const head = await command(label + '-revision', 'git', ['rev-parse', 'HEAD'], {cwd: path});
  assert.equal(head.text.trim(), revision, `${label} is not the pinned revision`);
  const diff = await command(label + '-diff', 'git', ['diff', '--name-only', 'HEAD', '--'], {cwd: path});
  const changed = diff.text.trim().split('\n').filter(Boolean);
  assert(changed.every(file => allowAlarmEdit && file === 'src/common/pf_alarm.c'),
    `${label} has unrelated tracked changes`);
  const names = await command(label + '-files', 'git', ['ls-files', '-z'], {cwd: path});
  const files = {};
  for(const name of names.text.split('\0').filter(Boolean)) {
    assert(!name.startsWith('/') && !name.split('/').includes('..'));
    const info = await lstat(join(path, name));
    if(info.isDirectory()) continue;
    assert(info.isFile() && !info.isSymbolicLink(), 'Unsafe input: ' + name);
    files[name] = sha(await readFile(join(path, name)));
  }
  return {revision, changed, files};
}

async function snapshot(input) {
  const staged = join(build, 'source');
  await mkdir(staged, {recursive: true});
  for(const name of Object.keys(input.files)) {
    await mkdir(dirname(join(staged, name)), {recursive: true});
    await copyFile(join(source, name), join(staged, name));
  }
  const adaptations = [];
  async function adapt(name, transform) {
    const path = join(staged, name);
    const before = await readFile(path, 'utf8');
    const after = transform(before);
    assert.notEqual(before, after, 'Compatibility hook did not match: ' + name);
    await writeFile(path, after);
    adaptations.push({name, originalSha256: sha(before), adaptedSha256: sha(after)});
  }
  for(const name of ['test/mocks.cpp', 'test/mocks.h']) {
    await adapt(name, text => text.replace(
      'mock_pnal_eth_init (\n   const char * if_name,\n   const pnal_cfg_t * pnal_cfg,',
      'mock_pnal_eth_init (\n   const char * if_name,\n   pnal_ethertype_t eth_receive_type,\n   const pnal_cfg_t * pnal_cfg,'));
  }
  const definitions = await readFile(join(staged, 'test/utils_for_testing.cpp'), 'utf8');
  await adapt('test/utils_for_testing.h', text =>
    text.replace(/\nint (my_\w+)\s*\(.*?\);/gs, (declaration, symbol) => {
      const signature = definitions.match(new RegExp(`\\n(int ${symbol}\\s*\\([^{}]*\\))\\s*(?:/\\*.*?\\*/\\s*)?\\{`, 's'));
      assert(signature, 'Unmatched callback signature ' + symbol);
      return '\n' + signature[1] + ';';
    }));
  await adapt('test/mocks.cpp', text => {
    const marker = 'int mock_pnal_eth_send (pnal_eth_handle_t * handle, pnal_buf_t * p_buf)\n{';
    assert.equal(text.split(marker).length, 2);
    const start = text.indexOf(marker);
    const end = text.indexOf('\n}', start);
    const body = text.slice(start, end);
    assert.equal(body.split('return p_buf->len;').length, 2);
    return text.slice(0, start) +
      'extern "C" int alarmack_test_send_observe (const uint8_t *, uint16_t);\n\n' +
      body.replace('return p_buf->len;', 'return alarmack_test_send_observe (static_cast<const uint8_t *> (p_buf->payload), p_buf->len);') +
      text.slice(end);
  });
  await adapt('test/mocks.cpp', text => {
    const start = text.indexOf('int mock_pnal_udp_recvfrom (');
    assert(start >= 0);
    const end = text.indexOf('\n}', start);
    const body = text.slice(start, end);
    assert.equal(body.split('   int len;').length, 2);
    return text.slice(0, start) + body.replace('   int len;',
      '   int len;\n\n   /* Deterministic peer metadata, not protocol state. */\n' +
      '   *p_dst_addr = PNAL_MAKEU32 (192, 168, 0, 1);\n' +
      '   *p_dst_port = 0xc001;') + text.slice(end);
  });
  await writeFile(join(evidence, 'support-adaptations.json'), JSON.stringify(adaptations, null, 2) + '\n');
  return staged;
}

async function verifyNative(corpus, tag, binary) {
  const result = await command(tag + '-native', binary,
    [`--gtest_filter=*AlarmAck${tag === 'parser' ? 'Parser' : 'Recovery'}*`,
      '--gtest_output=xml:' + join(evidence, tag + '.xml')],
    {cwd: evidence, allowFailure: true});
  assert.equal(result.signal, null, 'Native run did not complete');
  assert([0, 1].includes(result.code), 'Native crash or inconclusive process result');
  const actual = rows(result.text, tag);
  const differences = compare(corpus, actual, tag);
  const xml = await readFile(join(evidence, tag + '.xml'), 'utf8');
  assert.match(xml, new RegExp(`<testsuites tests="${corpus.length}"`), 'Wrong native test count');
  await writeFile(join(evidence, tag + '-observations.json'), JSON.stringify(actual, null, 2) + '\n');
  await writeFile(join(evidence, tag + '-differences.json'), JSON.stringify(differences, null, 2) + '\n');
  if(negative) {
    assert.equal(result.code, 1, 'Counterexample control unexpectedly passed');
    assert(differences.length > 0, 'Failure has no model-contract witness');
  } else {
    assert.equal(result.code, 0, 'Native regression failed');
    assert.deepEqual(differences, [], 'Native/model disagreement');
  }
  return {histories: actual.length, failingHistories: new Set(differences.map(d => d.id)).size,
    differingObservations: differences.length, exitCode: result.code};
}

try {
  await mkdir(build, {recursive: true});
  const input = await identity(source, pin, 'pnet', true);
  receipt.inputs = {pnet: input};
  for(const [name, revision] of Object.entries(pins))
    receipt.inputs[name] = await identity(join(deps, name), revision, name);
  for(const name of ['Common.lean', 'Parser.lean', 'Recovery.lean', 'Emit.lean',
    'bridge.mjs', 'bridge.test.mjs', 'verify.mjs', 'lean-toolchain', 'lakefile.lean']) {
    const bytes = await readFile(join(root, name));
    receipt.hashes[name] = sha(bytes);
    if(name.endsWith('.lean'))
      assert(!/^\s*(?:axiom|unsafe)\b|\b(?:sorry|admit|native_decide)\b/m.test(bytes.toString()),
        'Unapproved proof escape: ' + name);
  }
  for(const name of ['CMakeLists.txt', 'alarmack_fixture.h', 'alarmack_fixture.cpp',
    'test_alarmack_parser.cpp', 'test_alarmack_recovery.cpp'])
    receipt.hashes['harness/' + name] = sha(await readFile(resolve(root, '../harness', name)));
  const version = await command('lean-version', lean, ['--version']);
  assert.match(version.text, /version 4\.19\.0,/);
  const leanEnv = {LEAN_PATH: evidence, LEAN_NUM_THREADS: '1', MALLOC_ARENA_MAX: '1'};
  const flags = ['-j1', '-s1024', '-DElab.async=false'];
  for(const [name, count] of [['Common', 0], ['Parser', 6], ['Recovery', 10]]) {
    const checked = await command('proof-' + name, lean,
      [...flags, '-o', join(evidence, name + '.olean'), join(root, name + '.lean')],
      {env: leanEnv});
    if(count) auditAxioms(checked.text, count);
  }
  const corpora = {};
  const generated = join(build, 'generated');
  await mkdir(generated, {recursive: true});
  for(const tag of ['parser', 'recovery']) {
    const json = await command(tag + '-json', lean,
      [...flags, '--run', join(root, 'Emit.lean'), tag + '-json'], {env: leanEnv});
    const corpus = json.text.trim().split('\n').map(line => JSON.parse(line));
    assert.equal(corpus.length, tag === 'parser' ? 288 : 36);
    assert(corpus.every((c, id) => c.id === id));
    const emitted = await command(tag + '-header', lean,
      [...flags, '--run', join(root, 'Emit.lean'), tag + '-header'], {env: leanEnv});
    assert.equal(emitted.text, header(corpus, tag), 'JSON/header serialization disagreement');
    await writeFile(join(generated, tag + '_cases.h'), emitted.text);
    receipt.hashes[tag + '_cases.h'] = sha(emitted.text);
    corpora[tag] = corpus;
  }
  const falseProof = join(evidence, 'FalseCertificate.lean');
  await writeFile(falseProof, 'import Parser\nopen AlarmAck\nexample : Parser.parse (Parser.encode Parser.fixtureBody (Status.mk 0 0 0 0)) 22 = some (Status.mk 1 0 0 0) := by decide\n');
  const rejected = await command('false-certificate-control', lean,
    [...flags, falseProof], {env: leanEnv, allowFailure: true});
  assert.equal(rejected.signal, null);
  assert.equal(rejected.code, 1);
  assert.match(rejected.text, /tactic.*failed|type mismatch|unsolved goals|is false/);
  await command('bridge-controls', process.execPath, [join(root, 'bridge.test.mjs')]);

  const staged = await snapshot(input);
  const frames = await readFile(join(staged, 'test/test_cmrpc.cpp'), 'utf8');
  const arrays = ['/* GPL-3.0: exact fixture arrays from pinned p-net test_cmrpc.cpp. */'];
  receipt.startupFrames = {};
  for(const name of ['connect_req', 'write_req', 'prm_end_req', 'appl_rdy_rsp', 'data_packet']) {
    const matches = [...frames.matchAll(new RegExp(`static uint8_t ${name}\\[\\]\\s*=\\s*\\{(.*?)\\};`, 'gs'))];
    assert.equal(matches.length, 1, 'Ambiguous startup fixture ' + name);
    const body = matches[0][1].replace(/\/\*.*?\*\/|\/\/[^\n]*/gs, '');
    const values = [...body.matchAll(/0x([0-9a-fA-F]{2})\b/g)].map(m => parseInt(m[1], 16));
    receipt.startupFrames[name] = {bytes: values.length, sha256: sha(Buffer.from(values))};
    arrays.push(`static uint8_t ${name}[] = {${values.join(',')}};`);
  }
  await writeFile(join(generated, 'startup_frames.h'), arrays.join('\n') + '\n');
  await command('configure', 'cmake', ['-S', resolve(root, '../harness'), '-B', join(build, 'native'),
    '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug', '-DSOURCE=' + staged,
    '-DPNAL=' + join(deps, 'pnal'), '-DOSAL=' + join(deps, 'osal'),
    '-DGTEST=' + join(deps, 'gtest'), '-DGENERATED=' + generated,
    '-DCMAKE_C_FLAGS=' + option('--c-flags', ''),
    '-DCMAKE_CXX_FLAGS=' + option('--c-flags', '')]);
  await command('build', 'cmake', ['--build', join(build, 'native'), '--parallel', '3']);
  const binary = join(build, 'native/alarmack_regressions');
  receipt.hashes.binary = sha(await readFile(binary));
  receipt.native = {};
  for(const tag of scope === 'both' ? ['parser', 'recovery'] : [scope])
    receipt.native[tag] = await verifyNative(corpora[tag], tag, binary);
  for(const name of Object.keys(input.files))
    assert.equal(sha(await readFile(join(source, name))), input.files[name], 'Original input changed');
  receipt.status = negative ? 'EXPECTED_MODEL_COUNTEREXAMPLE' : 'PASS';
} catch(error) {
  receipt.status = 'FAIL';
  receipt.error = error.stack ?? String(error);
  process.exitCode = 1;
  console.error(receipt.error);
} finally {
  receipt.seconds = (performance.now() - started) / 1000;
  await writeFile(join(evidence, 'receipt.json'), JSON.stringify(receipt, null, 2) + '\n');
  console.log(`${receipt.status}: ${receipt.seconds.toFixed(2)} s; ${evidence}`);
}
