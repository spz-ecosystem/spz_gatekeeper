import { chromium } from '@playwright/test';
import { readFileSync } from 'node:fs';
import { gzipSync } from 'node:zlib';


function pushU32Le(bytes, value) {
  bytes.push(value & 0xff);
  bytes.push((value >>> 8) & 0xff);
  bytes.push((value >>> 16) & 0xff);
  bytes.push((value >>> 24) & 0xff);
}

function buildDecompressedFixture() {
  const bytes = [];
  pushU32Le(bytes, 0x5053474e);
  pushU32Le(bytes, 3);
  pushU32Le(bytes, 1);
  bytes.push(0);
  bytes.push(12);
  bytes.push(0);
  bytes.push(0);

  const payloadLength = 9 + 1 + 3 + 3 + 4;
  for (let i = 0; i < payloadLength; i += 1) {
    bytes.push(0);
  }

  return Buffer.from(bytes);
}

function buildValidFixture() {
  return gzipSync(buildDecompressedFixture(), { level: 9 });
}

function getBaseUrl() {
  const argUrl = process.argv[2];
  const envUrl = process.env.SPZ_WASM_SMOKE_URL;
  return argUrl || envUrl || 'http://127.0.0.1:4173';
}

function getFixturePath() {
  const argPath = process.argv[3];
  const envPath = process.env.SPZ_WASM_SMOKE_FIXTURE_PATH;
  return argPath || envPath || '';
}

function loadFixtureBuffer() {
  const fixturePath = getFixturePath();
  if (!fixturePath) {
    return { buffer: buildValidFixture(), source: 'in-memory fallback fixture' };
  }
  return { buffer: readFileSync(fixturePath), source: fixturePath };
}

async function runSmoke() {
  const baseUrl = getBaseUrl();
  const wasmUrl = new URL('spz_gatekeeper.js', baseUrl).toString();

  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();

  try {
    const wasmResponse = await page.request.get(wasmUrl, { timeout: 15000 });
    if (!wasmResponse.ok()) {
      throw new Error(`WASM 入口请求失败: ${wasmResponse.status()} ${wasmResponse.statusText()}`);
    }

    await page.goto(baseUrl, { waitUntil: 'domcontentloaded', timeout: 30000 });

    await page.waitForFunction(() => {
      const status = document.getElementById('statusText');
      return status && status.textContent && status.textContent.includes('WASM 引擎就绪');
    }, { timeout: 30000 });

    const fixture = loadFixtureBuffer();
    await page.setInputFiles('#fileInput', {
      name: 'synthetic_valid.spz',
      mimeType: 'application/octet-stream',
      buffer: fixture.buffer,
    });


    await page.waitForSelector('#resultContent:not(.hidden)', { timeout: 15000 });
    await page.waitForFunction(() => {
      const badge = document.getElementById('summaryBadge');
      return badge && badge.textContent && badge.textContent.trim() === 'PASS';
    }, { timeout: 15000 });
    await page.waitForFunction(() => {
      const registry = document.getElementById('registryList');
      return registry && registry.textContent && registry.textContent.includes('WASM Gate:');
    }, { timeout: 15000 });

    const statusClasses = await page.$eval('#loadingStatus', (el) => el.className);
    if (statusClasses.includes('error')) {
      throw new Error(`页面状态异常: ${statusClasses}`);
    }

    console.log(`WASM smoke PASS: ${baseUrl} fixture=${fixture.source}`);


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
