import { execFile } from 'node:child_process';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { promisify } from 'node:util';

import { chromium } from '@playwright/test';

const execFileAsync = promisify(execFile);



function getBaseUrl() {
  const argUrl = process.argv[2];
  const envUrl = process.env.SPZ_WASM_SMOKE_URL;
  return argUrl || envUrl || 'http://127.0.0.1:4173';
}

function getArtifactPath() {
  return process.argv[3] || process.env.SPZ_WASM_SMOKE_ARTIFACT || '';
}

function getCliBinaryPath() {
  return process.argv[4] || process.env.SPZ_WASM_SMOKE_CLI || 'build-native/spz_gatekeeper';
}

function createRuntimeWasmBytes() {

  return Buffer.from([
    0x00, 0x61, 0x73, 0x6d,
    0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x03, 0x02, 0x01, 0x00,
    0x07, 0x0d, 0x01, 0x09, 0x63, 0x6f, 0x72, 0x65, 0x43, 0x68, 0x65, 0x63, 0x6b, 0x00, 0x00,
    0x0a, 0x06, 0x01, 0x04, 0x00, 0x41, 0x01, 0x0b,
  ]);
}

function createLoaderSource({ invalidFixturePasses = false } = {}) {
  return `let instance = null;
export async function init(context) {
  const result = await WebAssembly.instantiate(context.moduleBytes, {});
  instance = result.instance;
  return true;
}
export function selfCheck() {
  return Boolean(instance && instance.exports.coreCheck && instance.exports.coreCheck() === 1);
}
export function runTinyFixture(fixture) {
  if (!instance) {
    throw new Error('loader not initialized');
  }
  if (fixture.kind === 'valid') {
    return instance.exports.coreCheck() === 1;
  }
  if (fixture.kind === 'invalid') {
    return ${invalidFixturePasses ? 'true' : 'false'};
  }
  return false;
}
`;
}

function makeManifest(extra = {}) {
  return {
    profile: 'spz',
    package_type: 'wasm_audit_bundle',
    entry: 'loader.mjs',
    module: 'module.wasm',
    exports: ['init', 'selfCheck', 'runTinyFixture'],
    budgets: {
      cold_start_ms: 1500,
      tiny_case_ms: 500,
      peak_memory_mb: 256,
      copy_pass_limit: 2,
    },
    ...extra,
  };
}

function encodeJson(value) {
  return Buffer.from(JSON.stringify(value, null, 2), 'utf8');
}

const crcTable = (() => {
  const table = new Uint32Array(256);
  for (let n = 0; n < 256; n += 1) {
    let c = n;
    for (let k = 0; k < 8; k += 1) {
      c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
    }
    table[n] = c >>> 0;
  }
  return table;
})();

function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) {
    crc = crcTable[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function buildStoredZip(entries) {
  const localParts = [];
  const centralParts = [];
  let offset = 0;

  for (const entry of entries) {
    const nameBytes = Buffer.from(entry.name, 'utf8');
    const data = Buffer.isBuffer(entry.data) ? entry.data : Buffer.from(entry.data);
    const checksum = crc32(data);

    const localHeader = Buffer.alloc(30);
    localHeader.writeUInt32LE(0x04034b50, 0);
    localHeader.writeUInt16LE(20, 4);
    localHeader.writeUInt16LE(0, 6);
    localHeader.writeUInt16LE(0, 8);
    localHeader.writeUInt16LE(0, 10);
    localHeader.writeUInt16LE(0, 12);
    localHeader.writeUInt32LE(checksum, 14);
    localHeader.writeUInt32LE(data.length, 18);
    localHeader.writeUInt32LE(data.length, 22);
    localHeader.writeUInt16LE(nameBytes.length, 26);
    localHeader.writeUInt16LE(0, 28);
    localParts.push(localHeader, nameBytes, data);

    const centralHeader = Buffer.alloc(46);
    centralHeader.writeUInt32LE(0x02014b50, 0);
    centralHeader.writeUInt16LE(20, 4);
    centralHeader.writeUInt16LE(20, 6);
    centralHeader.writeUInt16LE(0, 8);
    centralHeader.writeUInt16LE(0, 10);
    centralHeader.writeUInt16LE(0, 12);
    centralHeader.writeUInt16LE(0, 14);
    centralHeader.writeUInt32LE(checksum, 16);
    centralHeader.writeUInt32LE(data.length, 20);
    centralHeader.writeUInt32LE(data.length, 24);
    centralHeader.writeUInt16LE(nameBytes.length, 28);
    centralHeader.writeUInt16LE(0, 30);
    centralHeader.writeUInt16LE(0, 32);
    centralHeader.writeUInt16LE(0, 34);
    centralHeader.writeUInt16LE(0, 36);
    centralHeader.writeUInt32LE(0, 38);
    centralHeader.writeUInt32LE(offset, 42);
    centralParts.push(centralHeader, nameBytes);

    offset += localHeader.length + nameBytes.length + data.length;
  }

  const centralDirectory = Buffer.concat(centralParts);
  const endOfCentralDirectory = Buffer.alloc(22);
  endOfCentralDirectory.writeUInt32LE(0x06054b50, 0);
  endOfCentralDirectory.writeUInt16LE(0, 4);
  endOfCentralDirectory.writeUInt16LE(0, 6);
  endOfCentralDirectory.writeUInt16LE(entries.length, 8);
  endOfCentralDirectory.writeUInt16LE(entries.length, 10);
  endOfCentralDirectory.writeUInt32LE(centralDirectory.length, 12);
  endOfCentralDirectory.writeUInt32LE(offset, 16);
  endOfCentralDirectory.writeUInt16LE(0, 20);

  return Buffer.concat([...localParts, centralDirectory, endOfCentralDirectory]);
}

function createBundleBuffer({
  includeManifest = true,
  manifest = makeManifest(),
  loaderSource = createLoaderSource(),
  moduleBytes = createRuntimeWasmBytes(),
  includeValidFixture = true,
  includeInvalidFixture = true,
} = {}) {
  const entries = [];
  if (includeManifest) {
    entries.push({ name: 'manifest.json', data: encodeJson(manifest) });
  }
  entries.push({ name: manifest.entry || 'loader.mjs', data: Buffer.from(loaderSource, 'utf8') });
  entries.push({ name: manifest.module || 'module.wasm', data: Buffer.from(moduleBytes) });
  if (includeValidFixture) {
    entries.push({ name: 'tiny_fixtures/valid_case.json', data: encodeJson({ kind: 'valid' }) });
  }
  if (includeInvalidFixture) {
    entries.push({ name: 'tiny_fixtures/invalid_case.json', data: encodeJson({ kind: 'invalid' }) });
  }
  return buildStoredZip(entries);
}

async function waitForReady(page) {
  await page.waitForFunction(() => {
    const status = document.getElementById('statusText');
    return status && status.textContent && status.textContent.includes('WASM 引擎就绪');
  }, { timeout: 30000 });
}

async function uploadBundleAndAssert(page, { name, buffer, expectedBadge, expectedIssueCode }) {
  await page.setInputFiles('#fileInput', {
    name,
    mimeType: 'application/zip',
    buffer,
  });

  await page.waitForSelector('#resultContent:not(.hidden)', { timeout: 15000 });
  await page.waitForFunction((badgeText) => {
    const badge = document.getElementById('summaryBadge');
    return badge && badge.textContent && badge.textContent.trim() === badgeText;
  }, expectedBadge, { timeout: 15000 });

  if (expectedIssueCode) {
    await page.waitForFunction((issueCode) => {
      const issues = document.getElementById('issuesList');
      return issues && issues.textContent && issues.textContent.includes(issueCode);
    }, expectedIssueCode, { timeout: 15000 });
  }
}

async function evaluateBundleAudit(page, buffer, fileName, options = {}) {
  return page.evaluate(async ({ bytes, name, options: auditOptions }) => {
    const moduleFactory = (await import('./spz_gatekeeper.js')).default;
    const wasm = await moduleFactory();
    const bundle = new Uint8Array(bytes);
    const report = await wasm.auditWasmBundle(bundle, name, auditOptions);
    return {
      hasSharedBuilder: typeof wasm.buildBrowserAuditReport === 'function',
      report,
      handoff: wasm.buildBrowserToCliHandoff(report),
    };
  }, { bytes: Array.from(buffer), name: fileName, options });
}

async function runCliRoundTrip(cliBinaryPath, artifactPath, handoff) {
  if (!artifactPath) {
    throw new Error('缺少 CLI roundtrip 所需的 .spz artifact 路径');
  }

  const tempDir = await mkdtemp(join(tmpdir(), 'spz-wasm-smoke-'));
  const handoffPath = join(tempDir, 'browser_handoff.json');
  try {
    await writeFile(handoffPath, JSON.stringify(handoff, null, 2), 'utf8');
    const { stdout } = await execFileAsync(cliBinaryPath, [
      'compat-check',
      artifactPath,
      '--handoff',
      handoffPath,
      '--json',
    ]);
    return stdout;
  } finally {
    await rm(tempDir, { recursive: true, force: true });
  }
}

async function runSmoke() {
  const baseUrl = getBaseUrl();
  const artifactPath = getArtifactPath();
  const cliBinaryPath = getCliBinaryPath();
  const wrapperUrl = new URL('spz_gatekeeper.js', baseUrl).toString();


  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();

  try {
    const wrapperResponse = await page.request.get(wrapperUrl, { timeout: 15000 });
    if (!wrapperResponse.ok()) {
      throw new Error(`WASM 包装入口请求失败: ${wrapperResponse.status()} ${wrapperResponse.statusText()}`);
    }

    await page.goto(baseUrl, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await waitForReady(page);

    await page.waitForFunction(() => {
      const registry = document.getElementById('registryList');
      return registry && registry.textContent && registry.textContent.includes('WASM Gate:');
    }, { timeout: 15000 });

    const validBundle = createBundleBuffer();
    const validAudit = await evaluateBundleAudit(page, validBundle, 'valid_bundle.zip');
    if (!validAudit.hasSharedBuilder) {
      throw new Error('WASM runtime 未导出共享 browser schema builder');
    }
    if (validAudit.report.audit_profile !== 'spz') {
      throw new Error('browser report audit_profile 不正确');
    }
    if (validAudit.report.policy_name !== 'spz_gatekeeper_policy' || validAudit.report.policy_version !== '2.0.0') {
      throw new Error('browser report policy metadata 不正确');
    }
    if (validAudit.report.audit_mode !== 'browser_lightweight_wasm_audit') {
      throw new Error('browser report audit_mode 不正确');
    }
    if (validAudit.report.policy_mode !== 'release') {
      throw new Error('browser report policy_mode 不正确');
    }
    if (validAudit.report.bundle_verdict !== 'pass') {
      throw new Error('browser report bundle_verdict 不正确');
    }
    if (validAudit.report.final_verdict !== 'pass' || validAudit.report.release_ready !== true) {
      throw new Error('browser report final_verdict/release_ready 不正确');
    }
    if (!validAudit.report.summary || validAudit.report.summary.bundle_name !== 'valid_bundle.zip') {
      throw new Error('browser report summary.bundle_name 不正确');
    }
    if (!validAudit.report.budgets || !validAudit.report.budgets.cold_start_ms) {
      throw new Error('browser report budgets 缺少 cold_start_ms');
    }
    if (!validAudit.report.copy_breakdown || validAudit.report.copy_breakdown.total_passes !== 1) {
      throw new Error('browser report copy_breakdown.total_passes 不正确');
    }
    if (!Array.isArray(validAudit.report.copy_breakdown.stages) || !validAudit.report.copy_breakdown.stages.some((stage) => stage.name === 'module_clone' && stage.count === 1)) {
      throw new Error('browser report copy_breakdown.stages 缺少 module_clone');
    }
    if (!Array.isArray(validAudit.report.issues)) {
      throw new Error('browser report issues 不是数组');
    }

    const derivedReleaseReadyReport = await page.evaluate(async () => {
      const moduleFactory = (await import('./spz_gatekeeper.js')).default;
      const wasm = await moduleFactory();
      if (typeof wasm.buildBrowserAuditReport !== 'function') {
        return null;
      }
      return wasm.buildBrowserAuditReport({
        bundle_id: 'sha256:derived-release-ready',
        policy_mode: 'dev',
        verdict: 'pass',
        final_verdict: 'review_required',
        release_ready: true,
        next_action: 'review_bundle_before_cli',
        audit_duration_ms: 1,
        summary: {
          bundle_name: 'derived_release_ready.zip',
          file_count: 1,
          issue_count: 1,
          declared_export_count: 1,
          loader_export_count: 1,
          wasm_export_count: 1,
          valid_tiny_passed: true,
          invalid_tiny_handled: true,
          runtime_available: true,
        },
        manifest_summary: {},
        budgets: {},
        issues: [{ severity: 'warning', code: 'BUNDLE_REVIEW', message: 'review required' }],
        bundle_entries: [],
        wasm_export_summary: [],
        empty_shell_risk: false,
        copy_budget_wired: true,
        memory_budget_wired: true,
        performance_budget_wired: true,
      });
    });
    if (!derivedReleaseReadyReport) {
      throw new Error('共享 browser schema builder 不可用');
    }
    if (derivedReleaseReadyReport.final_verdict !== 'review_required' || derivedReleaseReadyReport.release_ready !== false) {
      throw new Error('browser report release_ready 应仅由 final_verdict 决定');
    }

    const devPolicyBundle = createBundleBuffer({
      manifest: makeManifest({ budgets: { cold_start_ms: 1500, tiny_case_ms: 500, peak_memory_mb: 256 } }),
    });
    const devPolicyAudit = await evaluateBundleAudit(page, devPolicyBundle, 'dev_policy_bundle.zip', {
      policy_mode: 'dev',
    });
    if (devPolicyAudit.report.policy_mode !== 'dev') {
      throw new Error('dev policy_mode 不正确');
    }
    if (devPolicyAudit.report.budgets.copy_pass_limit.status !== 'observed_without_budget') {
      throw new Error('dev copy budget 状态应为 observed_without_budget');
    }
    if (devPolicyAudit.report.final_verdict !== 'pass') {
      throw new Error('dev copy budget 默认不应阻断通过');
    }

    const challengePolicyBundle = createBundleBuffer({
      manifest: makeManifest({ budgets: { cold_start_ms: 1500, tiny_case_ms: 500, peak_memory_mb: 256 } }),
    });
    const challengePolicyAudit = await evaluateBundleAudit(
      page,
      challengePolicyBundle,
      'challenge_policy_bundle.zip',
      { policy_mode: 'challenge' },
    );
    if (challengePolicyAudit.report.policy_mode !== 'challenge') {
      throw new Error('challenge policy_mode 不正确');
    }
    if (challengePolicyAudit.report.budgets.copy_pass_limit.declared !== 2) {
      throw new Error('challenge copy budget 默认 declared 不正确');
    }
    if (challengePolicyAudit.report.budgets.copy_pass_limit.status !== 'within_budget') {
      throw new Error('challenge copy budget 默认应为 within_budget');
    }

    const releaseCopyNotCollectedAudit = await evaluateBundleAudit(
      page,
      createBundleBuffer(),
      'release_copy_not_collected_bundle.zip',
      { policy_mode: 'release', disable_copy_budget_collection: true },
    );
    if (releaseCopyNotCollectedAudit.report.budgets.copy_pass_limit.status !== 'not_collected') {
      throw new Error('release copy budget 未采集状态不正确');
    }
    if (!releaseCopyNotCollectedAudit.report.issues.some((issue) => issue.code === 'BUNDLE_COPY_BUDGET_NOT_COLLECTED')) {
      throw new Error('release copy budget 未采集 issue 缺失');
    }
    if (releaseCopyNotCollectedAudit.report.final_verdict !== 'review_required') {
      throw new Error('release copy budget 未采集应升级为 review_required');
    }

    await uploadBundleAndAssert(page, {
      name: 'valid_bundle.zip',
      buffer: validBundle,
      expectedBadge: 'PASS',
    });

    const downloadPromise = page.waitForEvent('download');

    await page.click('#exportHandoffButton');
    const handoffDownload = await downloadPromise;
    const handoffPath = await handoffDownload.path();
    if (!handoffPath) {
      throw new Error('handoff 下载文件路径为空');
    }
    const handoffText = await readFile(handoffPath, 'utf8');
    if (!handoffText.includes('"audit_mode": "browser_lightweight_wasm_audit"')) {
      throw new Error('handoff 缺少 browser audit mode');
    }
    if (!handoffText.includes('"bundle_id": "sha256:')) {
      throw new Error('handoff 缺少 bundle_id');
    }
    if (!handoffText.includes('"tool_version": "1.0.0"')) {
      throw new Error('handoff 缺少 tool_version');
    }
    if (!handoffText.includes('"policy_mode": "release"')) {
      throw new Error('handoff 缺少 policy_mode');
    }
    if (!handoffText.includes('"bundle_verdict": "pass"')) {
      throw new Error('handoff 缺少 bundle_verdict');
    }
    if (!handoffText.includes('"next_action": "allow_local_cli_audit"')) {
      throw new Error('handoff next_action 不正确');
    }

    const cliRoundTripJson = await runCliRoundTrip(cliBinaryPath, artifactPath, validAudit.handoff);
    if (!cliRoundTripJson.includes('"audit_mode":"local_cli_spz_artifact_audit"')) {
      throw new Error('CLI roundtrip 缺少 local_cli_spz_artifact_audit');
    }
    if (!cliRoundTripJson.includes('"upstream_audit":{')) {
      throw new Error('CLI roundtrip 缺少 upstream_audit');
    }
    if (!cliRoundTripJson.includes('"evidence_chain":["browser_lightweight_wasm_audit","local_cli_spz_artifact_audit"]')) {
      throw new Error('CLI roundtrip evidence_chain 不正确');
    }
    if (!cliRoundTripJson.includes('"final_verdict":"pass"')) {
      throw new Error('CLI roundtrip final_verdict 不正确');
    }

    const missingManifestBundle = createBundleBuffer({ includeManifest: false });


    await uploadBundleAndAssert(page, {
      name: 'missing_manifest_bundle.zip',
      buffer: missingManifestBundle,
      expectedBadge: 'BLOCK',
      expectedIssueCode: 'BUNDLE_MANIFEST_MISSING',
    });

    const mismatchedManifestBundle = createBundleBuffer({
      manifest: makeManifest({ exports: ['init', 'selfCheck', 'runTinyFixture', 'missingExport'] }),
    });
    await uploadBundleAndAssert(page, {
      name: 'mismatched_exports_bundle.zip',
      buffer: mismatchedManifestBundle,
      expectedBadge: 'BLOCK',
      expectedIssueCode: 'BUNDLE_EXPORT_MISMATCH',
    });

    const invalidTinyFixtureBundle = createBundleBuffer({
      loaderSource: createLoaderSource({ invalidFixturePasses: true }),
    });
    await uploadBundleAndAssert(page, {
      name: 'invalid_tiny_fixture_bundle.zip',
      buffer: invalidTinyFixtureBundle,
      expectedBadge: 'REVIEW_REQUIRED',
      expectedIssueCode: 'BUNDLE_TINY_INVALID_EXPECTED_FAIL',
    });

    const copyOverBudgetBundle = createBundleBuffer({
      manifest: makeManifest({ budgets: { cold_start_ms: 1500, tiny_case_ms: 500, peak_memory_mb: 256, copy_pass_limit: 0 } }),
    });
    await uploadBundleAndAssert(page, {
      name: 'copy_over_budget_bundle.zip',
      buffer: copyOverBudgetBundle,
      expectedBadge: 'REVIEW_REQUIRED',
      expectedIssueCode: 'BUNDLE_COPY_OVER_BUDGET',
    });

    const statusClasses = await page.$eval('#loadingStatus', (el) => el.className);

    if (statusClasses.includes('error')) {
      throw new Error(`页面状态异常: ${statusClasses}`);
    }

    console.log(`WASM smoke PASS: ${baseUrl}`);
  } catch (error) {
    await page.screenshot({ path: 'wasm-smoke-failure.png', fullPage: true });
    throw error;
  } finally {
    await browser.close();
  }
}

runSmoke().catch((error) => {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`WASM smoke FAIL: ${message}`);
  process.exit(1);
});
