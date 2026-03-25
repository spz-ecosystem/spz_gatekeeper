const kAuditProfile = 'spz';
const kAuditPolicyName = 'spz_gatekeeper_policy';
const kAuditPolicyVersion = '2.0.0';
const kAuditPolicyModeRelease = 'release';
const kAuditToolVersion = '1.0.0';
const kBrowserAuditMode = 'browser_lightweight_wasm_audit';


const kSupportedZipCompressionStored = 0;
const kSupportedZipCompressionDeflate = 8;
const kLocalFileHeaderSignature = 0x04034b50;
const kCentralDirectorySignature = 0x02014b50;
const kEndOfCentralDirectorySignature = 0x06054b50;
const textDecoder = new TextDecoder('utf-8');

function nowMs() {
  return typeof performance !== 'undefined' && typeof performance.now === 'function'
    ? performance.now()
    : Date.now();
}

function toUint8Array(input) {
  if (input instanceof Uint8Array) {
    return input;
  }
  if (input instanceof ArrayBuffer) {
    return new Uint8Array(input);
  }
  if (ArrayBuffer.isView(input)) {
    return new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
  }
  throw new Error('unsupported bundle input');
}

function readU16(bytes, offset) {
  return bytes[offset] | (bytes[offset + 1] << 8);
}

function readU32(bytes, offset) {
  return (bytes[offset]) |
    (bytes[offset + 1] << 8) |
    (bytes[offset + 2] << 16) |
    (bytes[offset + 3] << 24);
}

function decodeText(bytes) {
  return textDecoder.decode(bytes);
}

function normalizeEntryName(name) {
  const normalized = String(name).replaceAll('\\', '/').replace(/^\/+/, '').replace(/^\.\//, '');
  if (!normalized || normalized.startsWith('..') || normalized.includes('/../')) {
    throw new Error(`invalid zip entry path: ${name}`);
  }
  return normalized;
}

function findEndOfCentralDirectory(bytes) {
  const minOffset = Math.max(0, bytes.length - 0xffff - 22);
  for (let offset = bytes.length - 22; offset >= minOffset; offset -= 1) {
    if (readU32(bytes, offset) === kEndOfCentralDirectorySignature) {
      return offset;
    }
  }
  throw new Error('zip end of central directory not found');
}

function markCopyPass(counter) {
  if (counter) {
    counter.pass_count += 1;
  }
}

async function inflateRaw(bytes, copyBudget) {
  if (typeof DecompressionStream !== 'function') {
    throw new Error('DecompressionStream(deflate-raw) not available in this browser');
  }
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
  const arrayBuffer = await new Response(stream).arrayBuffer();
  markCopyPass(copyBudget);
  return new Uint8Array(arrayBuffer);
}

async function readZipEntries(input, copyBudget) {

  const bytes = toUint8Array(input);
  const eocdOffset = findEndOfCentralDirectory(bytes);
  const entryCount = readU16(bytes, eocdOffset + 10);
  const centralDirectorySize = readU32(bytes, eocdOffset + 12);
  const centralDirectoryOffset = readU32(bytes, eocdOffset + 16);
  const centralDirectoryEnd = centralDirectoryOffset + centralDirectorySize;
  if (centralDirectoryEnd > bytes.length) {
    throw new Error('zip central directory is truncated');
  }

  const entries = new Map();
  let offset = centralDirectoryOffset;
  for (let index = 0; index < entryCount; index += 1) {
    if (readU32(bytes, offset) !== kCentralDirectorySignature) {
      throw new Error('zip central directory signature mismatch');
    }

    const generalPurpose = readU16(bytes, offset + 8);
    const compressionMethod = readU16(bytes, offset + 10);
    const compressedSize = readU32(bytes, offset + 20);
    const uncompressedSize = readU32(bytes, offset + 24);
    const nameLength = readU16(bytes, offset + 28);
    const extraLength = readU16(bytes, offset + 30);
    const commentLength = readU16(bytes, offset + 32);
    const localHeaderOffset = readU32(bytes, offset + 42);
    const nameOffset = offset + 46;
    const rawName = decodeText(bytes.subarray(nameOffset, nameOffset + nameLength));

    const name = normalizeEntryName(rawName);

    if ((generalPurpose & 0x08) !== 0) {
      throw new Error(`zip entry uses unsupported data descriptor: ${name}`);
    }
    if (readU32(bytes, localHeaderOffset) !== kLocalFileHeaderSignature) {
      throw new Error(`zip local header signature mismatch: ${name}`);
    }

    const localNameLength = readU16(bytes, localHeaderOffset + 26);
    const localExtraLength = readU16(bytes, localHeaderOffset + 28);
    const dataOffset = localHeaderOffset + 30 + localNameLength + localExtraLength;
    const dataEnd = dataOffset + compressedSize;
    if (dataEnd > bytes.length) {
      throw new Error(`zip entry payload is truncated: ${name}`);
    }

    const compressedBytes = bytes.subarray(dataOffset, dataEnd);
    let payloadBytes;
    if (compressionMethod === kSupportedZipCompressionStored) {
      payloadBytes = compressedBytes;
    } else if (compressionMethod === kSupportedZipCompressionDeflate) {
      payloadBytes = await inflateRaw(compressedBytes, copyBudget);

    } else {
      throw new Error(`unsupported zip compression method ${compressionMethod}: ${name}`);
    }

    if (payloadBytes.length !== uncompressedSize) {
      throw new Error(`zip entry size mismatch: ${name}`);
    }

    entries.set(name, {
      name,
      compressionMethod,
      compressedSize,
      uncompressedSize,
      bytes: payloadBytes,
    });

    offset = nameOffset + nameLength + extraLength + commentLength;
  }

  return entries;
}

function pushIssue(issues, severity, code, message) {
  issues.push({ severity, code, message });
}

function resolveVerdict(issues) {
  if (issues.some((issue) => issue.severity === 'error')) {
    return 'block';
  }
  if (issues.some((issue) => issue.severity === 'warning')) {
    return 'review_required';
  }
  return 'pass';
}

function resolveNextAction(verdict) {
  if (verdict === 'pass') {
    return 'allow_local_cli_audit';
  }
  if (verdict === 'review_required') {
    return 'review_bundle_before_cli';
  }
  return 'block_bundle';
}

function isTruthyResult(value) {
  if (typeof value === 'object' && value !== null) {
    const maybeOk = value.ok;
    if (typeof maybeOk !== 'undefined') {
      return maybeOk === true;
    }
  }
  return Boolean(value);
}


function toSafeJson(bytes, name) {
  try {
    return JSON.parse(decodeText(bytes));
  } catch (error) {
    const reason = error instanceof Error ? error.message : String(error);
    throw new Error(`${name} is not valid JSON: ${reason}`);
  }
}

function describeBudget(declared, observed, collected = true) {
  if (typeof declared !== 'number' || !Number.isFinite(declared)) {
    return {
      declared: null,
      observed: collected ? observed : null,
      status: collected ? 'observed_without_budget' : 'not_collected',
      within_budget: null,
    };
  }
  if (!collected) {
    return {
      declared,
      observed: null,
      status: 'not_collected',
      within_budget: null,
    };
  }
  return {
    declared,
    observed,
    status: observed <= declared ? 'within_budget' : 'over_budget',
    within_budget: observed <= declared,
  };
}

function currentMemoryMb() {
  if (typeof performance === 'undefined' || !performance.memory || typeof performance.memory.usedJSHeapSize !== 'number') {
    return null;
  }
  return performance.memory.usedJSHeapSize / (1024 * 1024);
}

async function buildBundleId(bytes) {
  if (!globalThis.crypto || !globalThis.crypto.subtle || typeof globalThis.crypto.subtle.digest !== 'function') {
    return 'sha256:unavailable';
  }
  const hashBuffer = await globalThis.crypto.subtle.digest('SHA-256', toUint8Array(bytes));
  const hashHex = Array.from(new Uint8Array(hashBuffer), (value) => value.toString(16).padStart(2, '0')).join('');
  return `sha256:${hashHex}`;
}

async function importLoaderModule(sourceText) {

  const blob = new Blob([sourceText], { type: 'text/javascript' });
  const url = URL.createObjectURL(blob);
  try {
    return await import(url);
  } finally {
    URL.revokeObjectURL(url);
  }
}

async function maybeLoadRuntime() {
  try {
    const runtimeModule = await import('./spz_gatekeeper_wasm.js');
    if (typeof runtimeModule.default !== 'function') {
      return null;
    }
    return await runtimeModule.default();
  } catch (_error) {
    return null;
  }
}

function createRuntimeStatus(runtime) {
  return {
    available: runtime !== null,
    mode: kBrowserAuditMode,
    runtime_engine: runtime !== null ? 'spz_gatekeeper_wasm' : 'browser_only_wrapper',
  };
}

function buildLegacyBrowserAuditReport(data) {
  return {
    audit_profile: kAuditProfile,
    audit_mode: kBrowserAuditMode,
    bundle_id: data.bundle_id,
    tool_version: kAuditToolVersion,
    verdict: data.verdict,
    summary: data.summary,
    budgets: data.budgets,
    issues: data.issues,
    next_action: data.next_action,
    manifest_summary: data.manifest_summary,
    bundle_entries: data.bundle_entries,
    wasm_export_summary: data.wasm_export_summary,
    wasm_quality_gate: {
      coverage_level: 'browser_lightweight',
      validator_coverage_ok: true,
      empty_shell_risk: data.empty_shell_risk,
      api_surface_wired: true,
      browser_smoke_wired: true,
      empty_shell_guard_wired: true,
      warning_budget_wired: true,
      copy_budget_wired: data.copy_budget_wired,
      memory_budget_wired: data.memory_budget_wired,

      performance_budget_wired: data.performance_budget_wired,
      artifact_audit_wired: false,
      release_ready: data.verdict === 'pass',
    },
    audit_duration_ms: data.audit_duration_ms,
  };
}

function buildBrowserAuditReport(data, runtime) {
  if (runtime && typeof runtime.buildBrowserAuditReport === 'function') {
    return runtime.buildBrowserAuditReport(data);
  }
  return buildLegacyBrowserAuditReport(data);
}

function buildBrowserToCliHandoff(report) {

  return {
    audit_profile: report.audit_profile,
    audit_mode: report.audit_mode,
    bundle_id: report.bundle_id,
    tool_version: report.tool_version,
    verdict: report.verdict,
    summary: report.summary || {},
    budgets: report.budgets || {},
    issues: report.issues || [],
    next_action: report.next_action,
  };
}

async function auditWasmBundle(input, fileName = 'bundle.zip', runtime = null) {

  const issues = [];
  const auditStartedAt = nowMs();
  const bundleBytes = toUint8Array(input);
  const bundleId = await buildBundleId(bundleBytes);
  let manifest = null;

  let entries = new Map();
  let loaderModule = null;
  let wasmExports = [];
  let coldStartMs = null;
  let validTinyMs = null;
  let invalidTinyHandled = false;
  let memoryObservedMb = currentMemoryMb();
  const copyBudget = { pass_count: 0 };


  try {
    entries = await readZipEntries(bundleBytes);
  } catch (error) {
    const reason = error instanceof Error ? error.message : String(error);
    pushIssue(issues, 'error', 'BUNDLE_ZIP_INVALID', reason);
  }

  if (issues.length === 0 && !entries.has('manifest.json')) {
    pushIssue(issues, 'error', 'BUNDLE_MANIFEST_MISSING', 'zip 审查包缺少 manifest.json');
  }

  if (issues.length === 0) {
    try {
      manifest = toSafeJson(entries.get('manifest.json').bytes, 'manifest.json');
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      pushIssue(issues, 'error', 'BUNDLE_MANIFEST_INVALID', reason);
    }
  }

  const manifestSummary = {
    profile: manifest?.profile ?? null,
    package_type: manifest?.package_type ?? null,
    entry: manifest?.entry ?? null,
    module: manifest?.module ?? null,
    declared_exports: Array.isArray(manifest?.exports) ? [...manifest.exports] : [],
  };

  let entryName = null;
  let moduleName = null;
  if (issues.length === 0) {
    if (manifest.profile !== kAuditProfile) {
      pushIssue(issues, 'error', 'BUNDLE_PROFILE_UNSUPPORTED', `只支持 profile=${kAuditProfile}`);
    }
    if (manifest.package_type !== 'wasm_audit_bundle') {
      pushIssue(issues, 'error', 'BUNDLE_PACKAGE_TYPE_INVALID', 'package_type 必须为 wasm_audit_bundle');
    }
    if (!Array.isArray(manifest.exports) || manifest.exports.length === 0) {
      pushIssue(issues, 'error', 'BUNDLE_EXPORT_DECLARATION_MISSING', 'manifest.exports 必须是非空数组');
    }

    try {
      entryName = normalizeEntryName(manifest.entry);
      moduleName = normalizeEntryName(manifest.module);
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      pushIssue(issues, 'error', 'BUNDLE_ENTRY_PATH_INVALID', reason);
    }
  }

  const loaderEntry = entryName ? entries.get(entryName) : null;
  const moduleEntry = moduleName ? entries.get(moduleName) : null;
  if (issues.length === 0 && loaderEntry == null) {
    pushIssue(issues, 'error', 'BUNDLE_ENTRY_MISSING', `缺少 loader 入口: ${entryName}`);
  }
  if (issues.length === 0 && moduleEntry == null) {
    pushIssue(issues, 'error', 'BUNDLE_MODULE_MISSING', `缺少 wasm 模块: ${moduleName}`);
  }

  if (issues.length === 0) {
    try {
      loaderModule = await importLoaderModule(decodeText(loaderEntry.bytes));
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      pushIssue(issues, 'error', 'BUNDLE_LOADER_IMPORT_FAILED', reason);
    }
  }

  const loaderExportNames = loaderModule != null ? Object.keys(loaderModule).sort() : [];
  if (issues.length === 0) {
    const missingExports = manifest.exports.filter((name) => !(name in loaderModule));
    const extraExports = loaderExportNames.filter((name) => !manifest.exports.includes(name));
    if (missingExports.length > 0) {
      pushIssue(
        issues,
        'error',
        'BUNDLE_EXPORT_MISMATCH',
        `manifest 声明导出缺失: ${missingExports.join(', ')}`,
      );
    }
    if (extraExports.length > 0) {
      pushIssue(
        issues,
        'warning',
        'BUNDLE_EXPORT_EXTRA',
        `loader 导出超出 manifest 声明: ${extraExports.join(', ')}`,
      );
    }
  }

  if (issues.filter((issue) => issue.severity === 'error').length === 0 && moduleEntry != null) {
    try {
      const compiledModule = await WebAssembly.compile(moduleEntry.bytes);
      wasmExports = WebAssembly.Module.exports(compiledModule);
      const wasmFunctionExportCount = wasmExports.filter((item) => item.kind === 'function').length;
      if (wasmFunctionExportCount === 0) {
        pushIssue(issues, 'warning', 'BUNDLE_EMPTY_SHELL_RISK', 'module.wasm 未暴露任何函数导出，存在空壳风险');
      }
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      pushIssue(issues, 'error', 'BUNDLE_MODULE_COMPILE_FAILED', reason);
    }
  }

  const validFixtureEntry = entries.get('tiny_fixtures/valid_case.json');
  const invalidFixtureEntry = entries.get('tiny_fixtures/invalid_case.json');
  let validFixture = null;
  let invalidFixture = null;
  if (validFixtureEntry != null) {
    try {
      validFixture = toSafeJson(validFixtureEntry.bytes, 'tiny_fixtures/valid_case.json');
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      pushIssue(issues, 'error', 'BUNDLE_TINY_VALID_INVALID_JSON', reason);
    }
  } else {
    pushIssue(issues, 'warning', 'BUNDLE_TINY_VALID_MISSING', '缺少 tiny_fixtures/valid_case.json');
  }

  if (invalidFixtureEntry != null) {
    try {
      invalidFixture = toSafeJson(invalidFixtureEntry.bytes, 'tiny_fixtures/invalid_case.json');
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      pushIssue(issues, 'error', 'BUNDLE_TINY_INVALID_INVALID_JSON', reason);
    }
  } else {
    pushIssue(issues, 'warning', 'BUNDLE_TINY_INVALID_MISSING', '缺少 tiny_fixtures/invalid_case.json');
  }

  const context = {
    fileName,
    manifest,
    moduleBytes: moduleEntry != null
      ? (() => {
          markCopyPass(copyBudget);
          return new Uint8Array(moduleEntry.bytes);
        })()
      : new Uint8Array(),
    runtime,
    wasmExports,
  };


  if (issues.filter((issue) => issue.severity === 'error').length === 0 && typeof loaderModule.init === 'function') {
    const initStartedAt = nowMs();
    try {
      const initResult = await loaderModule.init(context);
      coldStartMs = nowMs() - initStartedAt;
      if (typeof initResult !== 'undefined' && !isTruthyResult(initResult)) {
        pushIssue(issues, 'error', 'BUNDLE_INIT_FAILED', 'loader.init 返回失败结果');
      }
    } catch (error) {
      coldStartMs = nowMs() - initStartedAt;
      const reason = error instanceof Error ? error.message : String(error);
      pushIssue(issues, 'error', 'BUNDLE_INIT_FAILED', reason);
    }
  }

  if (issues.filter((issue) => issue.severity === 'error').length === 0 && typeof loaderModule.selfCheck === 'function') {
    try {
      const selfCheckResult = await loaderModule.selfCheck(context);
      if (!isTruthyResult(selfCheckResult)) {
        pushIssue(issues, 'error', 'BUNDLE_SELF_CHECK_FAILED', 'loader.selfCheck 未通过');
      }
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      pushIssue(issues, 'error', 'BUNDLE_SELF_CHECK_FAILED', reason);
    }
  }

  if (issues.filter((issue) => issue.severity === 'error').length === 0 && typeof loaderModule.runTinyFixture === 'function' && validFixture != null) {
    const validStartedAt = nowMs();
    try {
      const validResult = await loaderModule.runTinyFixture(validFixture, context);
      validTinyMs = nowMs() - validStartedAt;
      if (!isTruthyResult(validResult)) {
        pushIssue(issues, 'error', 'BUNDLE_TINY_VALID_FAILED', 'valid tiny fixture 未通过');
      }
    } catch (error) {
      validTinyMs = nowMs() - validStartedAt;
      const reason = error instanceof Error ? error.message : String(error);
      pushIssue(issues, 'error', 'BUNDLE_TINY_VALID_FAILED', reason);
    }
  }

  if (issues.filter((issue) => issue.severity === 'error').length === 0 && typeof loaderModule.runTinyFixture === 'function' && invalidFixture != null) {
    try {
      const invalidResult = await loaderModule.runTinyFixture(invalidFixture, context);
      invalidTinyHandled = !isTruthyResult(invalidResult);
      if (!invalidTinyHandled) {
        pushIssue(issues, 'warning', 'BUNDLE_TINY_INVALID_EXPECTED_FAIL', 'invalid tiny fixture 未能稳定失败');
      }
    } catch (_error) {
      invalidTinyHandled = true;
    }
  }

  memoryObservedMb = currentMemoryMb() ?? memoryObservedMb;
  const declaredBudgets = manifest?.budgets ?? {};
  const budgets = {
    cold_start_ms: describeBudget(declaredBudgets.cold_start_ms, coldStartMs, coldStartMs !== null),
    tiny_case_ms: describeBudget(declaredBudgets.tiny_case_ms, validTinyMs, validTinyMs !== null),
    peak_memory_mb: describeBudget(declaredBudgets.peak_memory_mb, memoryObservedMb, memoryObservedMb !== null),
    copy_pass_limit: describeBudget(declaredBudgets.copy_pass_limit, copyBudget.pass_count, true),
  };


  if (budgets.cold_start_ms.status === 'over_budget') {
    pushIssue(issues, 'warning', 'BUNDLE_COLD_START_OVER_BUDGET', '冷启动耗时超出 manifest 预算');
  }
  if (budgets.tiny_case_ms.status === 'over_budget') {
    pushIssue(issues, 'warning', 'BUNDLE_TINY_CASE_OVER_BUDGET', 'tiny fixture 耗时超出 manifest 预算');
  }
  if (budgets.peak_memory_mb.status === 'over_budget') {
    pushIssue(issues, 'warning', 'BUNDLE_MEMORY_OVER_BUDGET', '内存占用超出 manifest 预算');
  }

  const verdict = resolveVerdict(issues);
  const summary = {
    bundle_name: fileName,
    file_count: entries.size,
    issue_count: issues.length,
    declared_export_count: manifestSummary.declared_exports.length,
    loader_export_count: loaderExportNames.length,
    wasm_export_count: wasmExports.length,
    valid_tiny_passed: validTinyMs !== null && !issues.some((issue) => issue.code === 'BUNDLE_TINY_VALID_FAILED'),
    invalid_tiny_handled: invalidTinyHandled,
    runtime_available: runtime !== null,
  };

  return buildBrowserAuditReport({
    bundle_id: bundleId,
    verdict,
    summary,
    budgets,
    issues,
    next_action: resolveNextAction(verdict),
    manifest_summary: manifestSummary,
    bundle_entries: Array.from(entries.values()).map((entry) => ({
      name: entry.name,
      compression_method: entry.compressionMethod,
      compressed_size: entry.compressedSize,
      uncompressed_size: entry.uncompressedSize,
    })),
    wasm_export_summary: wasmExports,
    empty_shell_risk: issues.some((issue) => issue.code === 'BUNDLE_EMPTY_SHELL_RISK'),
    copy_budget_wired: budgets.copy_pass_limit.status !== 'not_collected',
    memory_budget_wired: budgets.peak_memory_mb.status !== 'not_collected',

    performance_budget_wired:
      budgets.cold_start_ms.status !== 'not_collected' ||
      budgets.tiny_case_ms.status !== 'not_collected',
    audit_duration_ms: nowMs() - auditStartedAt,
  }, runtime);
}


function createUnavailableMethod(method) {
  return () => {
    throw new Error(`${method} is unavailable in browser_lightweight_wasm_audit mode`);
  };
}

export default async function createSpzGatekeeperModule() {
  const runtime = await maybeLoadRuntime();
  const runtimeStatus = createRuntimeStatus(runtime);

  return {
    auditWasmBundle: (input, fileName) => auditWasmBundle(input, fileName, runtime),
    buildBrowserAuditReport: runtime?.buildBrowserAuditReport?.bind(runtime),
    buildBrowserToCliHandoff,
    getRuntimeStatus: () => runtimeStatus,

    inspectSpz: runtime?.inspectSpz?.bind(runtime) ?? createUnavailableMethod('inspectSpz'),
    dumpTrailer: runtime?.dumpTrailer?.bind(runtime) ?? createUnavailableMethod('dumpTrailer'),
    inspectSpzText: runtime?.inspectSpzText?.bind(runtime) ?? createUnavailableMethod('inspectSpzText'),
    inspectCompatSummary: runtime?.inspectCompatSummary?.bind(runtime) ?? createUnavailableMethod('inspectCompatSummary'),
    listRegisteredExtensions: runtime?.listRegisteredExtensions?.bind(runtime) ?? (() => ({ count: 0, extensions: [] })),
    describeExtension: runtime?.describeExtension?.bind(runtime) ?? ((type) => ({ error: 'runtime unavailable', type })),
