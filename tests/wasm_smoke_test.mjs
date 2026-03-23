import { chromium } from '@playwright/test';

function getBaseUrl() {
  const argUrl = process.argv[2];
  const envUrl = process.env.SPZ_WASM_SMOKE_URL;
  return argUrl || envUrl || 'http://127.0.0.1:4173';
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

async function runSmoke() {
  const baseUrl = getBaseUrl();
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
    await uploadBundleAndAssert(page, {
      name: 'valid_bundle.zip',
      buffer: validBundle,
      expectedBadge: 'PASS',
    });

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
