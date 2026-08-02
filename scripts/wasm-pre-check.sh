#!/usr/bin/env bash
# WASM pre-check for spz_gatekeeper.
# Run locally before pushing WASM-related changes to CI.
# Exit codes:
#   0  pass
#   1  usage/internal error
#   2  P0 environment
#   3  P1 build
#   4  P2 symbol exports
#   5  P3 artifact
#   6  P4 frontend
#   7  P5 smoke test
#   8  P6 workflow lint
#   9  P7 file integrity

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-pages"
HTML_FILE="${PROJECT_DIR}/web/index.html"

EMSDK_VERSION="6.0.3"
WASM_MIN_BYTES=$((300 * 1024))        # 300KB (Emscripten 6.x optimized output)
WASM_MAX_BYTES=$((15 * 1024 * 1024))   # 15MB

REQUIRED_SYMBOLS=(
  # Embind exports (wasm_main.cc, in WASM binary)
  "inspectSpz"
  "inspectSpzPtr"
  "inspectSpzWithCompatPtr"
  "inspectCompatSummaryPtr"
  "inspectCompatSummary"
  "inspectSpzText"
  "dumpTrailer"
  "listRegisteredExtensions"
  "getCompatibilityBoard"
  "describeExtension"
  "buildBrowserAuditReport"
  # C runtime exports (CMakeLists.txt, in WASM binary)
  "_malloc"
  "_free"
  # R7: memory pool + reserved buffer C API (wasm_buffer.cc, in WASM binary)
  "_gk_reserve_buffer"
  "_gk_get_buffer_ptr"
  "_gk_get_buffer_size"
  "_gk_get_buffer_used"
  "_gk_set_buffer_used"
  "_gk_release_buffer"
  "_gk_reset_memory_stats"
  "_gk_get_memory_stats"
)
# JS-wrapped functions (spz_gatekeeper.js, not in WASM binary)
REQUIRED_JS_WRAPPERS=(
  "auditWasmBundle"
  # R7: chunked write to reserved buffer
  "writeToReservedBuffer"
)

STRICT=false
SKIP_BUILD=false
SKIP_SMOKE=false
AUTO_FIX=false

while [ $# -gt 0 ]; do
  case "$1" in
    --strict) STRICT=true; shift ;;
    --skip-build) SKIP_BUILD=true; shift ;;
    --skip-smoke) SKIP_SMOKE=true; shift ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --auto-fix) AUTO_FIX=true; shift ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

SITE_DIR="${BUILD_DIR}/site"
WASM_JS="${SITE_DIR}/spz_gatekeeper_wasm.js"
WASM_BINARY="${SITE_DIR}/spz_gatekeeper_wasm.wasm"

fail() {
  local stage="$1" code="$2" msg="$3" hint="${4:-}"
  python3 -c "import json,sys; print(json.dumps({'ok':False,'stage':sys.argv[1],'exit_code':int(sys.argv[2]),'message':sys.argv[3],'hint':sys.argv[4],'logs':[]}, ensure_ascii=False))" "$stage" "$code" "$msg" "$hint"
  exit "$code"
}

pass() {
  python3 -c "import json; print(json.dumps({'ok':True,'stage':'P6_WORKFLOW','exit_code':0,'message':'WASM pre-check passed','logs':[]}, ensure_ascii=False))"
  exit 0
}

# ---------------------------------------------------------------------------
# P0: Environment
# ---------------------------------------------------------------------------
check_environment() {
  local missing=()
  command -v emcc >/dev/null 2>&1 || missing+=("emcc")
  command -v wasm-objdump >/dev/null 2>&1 || missing+=("wasm-objdump")
  command -v node >/dev/null 2>&1 || missing+=("node")
  command -v python3 >/dev/null 2>&1 || missing+=("python3")

  if [ ${#missing[@]} -gt 0 ]; then
    # Auto-fix: try installing missing tools
    if [ "${AUTO_FIX}" = "true" ]; then
      local fixed=false
      for tool in "${missing[@]}"; do
        case "$tool" in
          wasm-objdump)
            echo "Auto-fix: installing wabt (provides wasm-objdump)..." >&2
            if apt-get update -qq >/dev/null 2>&1 && apt-get install -y -qq wabt >/dev/null 2>&1; then
              fixed=true
              echo "Auto-fix: wabt installed successfully" >&2
            elif add-apt-repository -y universe >/dev/null 2>&1 && apt-get update -qq >/dev/null 2>&1 && apt-get install -y -qq wabt >/dev/null 2>&1; then
              fixed=true
              echo "Auto-fix: wabt installed successfully (after enabling universe)" >&2
            else
              echo "Auto-fix: wabt installation failed" >&2
            fi
            ;;
          emcc)
            echo "Auto-fix: emcc requires emsdk — run: git clone https://github.com/emscripten-core/emsdk && ./emsdk install ${EMSDK_VERSION} && ./emsdk activate ${EMSDK_VERSION}" >&2
            ;;
          node)
            echo "Auto-fix: installing nodejs..." >&2
            if apt-get update -qq >/dev/null 2>&1 && apt-get install -y -qq nodejs >/dev/null 2>&1; then
              fixed=true
              echo "Auto-fix: nodejs installed successfully" >&2
            else
              echo "Auto-fix: nodejs installation failed" >&2
            fi
            ;;
        esac
      done
      if [ "${fixed}" = "true" ]; then
        # Re-check after auto-fix
        missing=()
        command -v emcc >/dev/null 2>&1 || missing+=("emcc")
        command -v wasm-objdump >/dev/null 2>&1 || missing+=("wasm-objdump")
        command -v node >/dev/null 2>&1 || missing+=("node")
        command -v python3 >/dev/null 2>&1 || missing+=("python3")
      fi
    fi

    if [ ${#missing[@]} -gt 0 ]; then
      local hint=""
      for tool in "${missing[@]}"; do
        case "$tool" in
          emcc)        hint+="emcc: git clone https://github.com/emscripten-core/emsdk && ./emsdk install ${EMSDK_VERSION} && ./emsdk activate ${EMSDK_VERSION}; " ;;
          wasm-objdump) hint+="wasm-objdump: sudo apt-get install -y wabt  (Ubuntu)  OR  brew install wabt  (macOS); " ;;
          node)        hint+="node: sudo apt-get install -y nodejs  OR  https://nodejs.org; " ;;
          python3)     hint+="python3: sudo apt-get install -y python3; " ;;
        esac
      done
      fail "P0_ENV" 2 "Missing tools: ${missing[*]}" "${hint}"
    fi
  fi

  local active_version
  active_version="$(emcc --version | head -n1 | grep -oP '[0-9]+\.[0-9]+\.[0-9]+' | head -n1 || true)"
  if [ "${active_version}" != "${EMSDK_VERSION}" ]; then
    fail "P0_ENV" 2 "emsdk version mismatch: expected ${EMSDK_VERSION}, got ${active_version:-unknown}" \
      "Run: ./emsdk install ${EMSDK_VERSION} && ./emsdk activate ${EMSDK_VERSION}"
  fi
}

# ---------------------------------------------------------------------------
# P1: Build
# ---------------------------------------------------------------------------
check_build() {
  local build_ok=false
  local max_attempts=1
  [ "${AUTO_FIX}" = "true" ] && max_attempts=2

  for attempt in $(seq 1 ${max_attempts}); do
    if [ "${attempt}" -gt 1 ]; then
      echo "  Auto-fix: retrying build (attempt ${attempt}/${max_attempts})..." >&2
      rm -rf "${BUILD_DIR}"
    fi

    if ! emcmake cmake -S "${PROJECT_DIR}/cpp" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DSPZ_GATEKEEPER_BUILD_BENCHMARK_TESTS=OFF \
        -DSPZ_GATEKEEPER_BUILD_WASM=ON >/dev/null 2>&1; then
      if [ "${attempt}" -lt "${max_attempts}" ]; then
        echo "  Auto-fix: cmake configure failed, retrying after cleanup..." >&2
        continue
      fi
      fail "P1_BUILD" 3 "emcmake configuration failed" "Check CMake output: emcmake cmake -S cpp -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Release"
    fi

    if ! emmake cmake --build "${BUILD_DIR}" --parallel >/dev/null 2>&1; then
      # Auto-fix: retry with --parallel 1 if OOM suspected
      if [ "${AUTO_FIX}" = "true" ] && [ "${attempt}" -lt "${max_attempts}" ]; then
        echo "  Auto-fix: build failed (possible OOM), retrying with --parallel 1..." >&2
        if emmake cmake --build "${BUILD_DIR}" --parallel 1 >/dev/null 2>&1; then
          build_ok=true
          break
        fi
        echo "  Auto-fix: single-thread build also failed, retrying from clean build..." >&2
        continue
      fi
      fail "P1_BUILD" 3 "emmake build failed" "Check compiler errors in C++ source files"
    fi
    build_ok=true
    break
  done

  if [ "${build_ok}" != "true" ]; then
    fail "P1_BUILD" 3 "emmake build failed after ${max_attempts} attempts" "Check compiler errors, disk space, and memory"
  fi
}

# ---------------------------------------------------------------------------
# P2: Symbol exports
# ---------------------------------------------------------------------------
check_symbols() {
  local missing=()
  local wasm_cc="${PROJECT_DIR}/cpp/src/wasm_main.cc"
  local cmake_file="${PROJECT_DIR}/cpp/CMakeLists.txt"
  local js_wrapper="${PROJECT_DIR}/web/spz_gatekeeper.js"

  # Embind exports: verify in wasm_main.cc source (function("name", ...))
  for sym in "${REQUIRED_SYMBOLS[@]}"; do
    case "$sym" in
      _malloc|_free|_gk_*)
        # C runtime exports: verify in CMakeLists.txt EXPORTED_FUNCTIONS
        if ! grep -qE "\b${sym}\b" "${cmake_file}" 2>/dev/null; then
          missing+=("${sym}")
        fi
        ;;
      *)
        # Embind exports: check for emscripten::function("<sym>", ...)
        if ! grep -qE 'function\("'"${sym}"'"' "${wasm_cc}" 2>/dev/null; then
          missing+=("${sym}")
        fi
        ;;
    esac
  done

  # JS wrappers: verify in spz_gatekeeper.js
  if [ -f "${js_wrapper}" ]; then
    for sym in "${REQUIRED_JS_WRAPPERS[@]}"; do
      if ! grep -qE "\b${sym}\b" "${js_wrapper}" 2>/dev/null; then
        missing+=("${sym}")
      fi
    done
  fi

  if [ ${#missing[@]} -gt 0 ]; then
    fail "P2_SYMBOL" 4 "Missing exported symbol(s): ${missing[*]}" "Check wasm_main.cc for Embind exports, CMakeLists.txt for EXPORTED_FUNCTIONS, and spz_gatekeeper.js for JS wrappers"
  fi

  # 产物级 embind 绑定检查：embind 绑定名以 UTF-8 字符串存在 wasm 二进制的数据段中。
  # 源码里 emscripten::function("name") 存在 ≠ 绑定真的编进了产物（此前组合导出
  # inspectSpzWithCompatPtr 在 wrapper 缺失时前端 fallback、性能翻倍但 P2 全绿）。
  # 对实际构建产物逐个绑定名做二进制字符串搜索，缺失即拦截。
  local missing_bind=()
  if [ -f "${WASM_BINARY}" ]; then
    for sym in "${REQUIRED_SYMBOLS[@]}"; do
      case "$sym" in
        _malloc|_free|_gk_*)
          # C 导出不依赖 embind 字符串，跳过（由 EXPORTED_FUNCTIONS 保证）
          continue
          ;;
      esac
      if ! grep -aFq "${sym}" "${WASM_BINARY}" 2>/dev/null; then
        missing_bind+=("${sym}")
      fi
    done
    if [ ${#missing_bind[@]} -gt 0 ]; then
      fail "P2_EMBIND_BINARY" 4 "Embend binding(s) absent from built WASM binary: ${missing_bind[*]}" "Bindings declared in wasm_main.cc were not emitted into the wasm — check linker dead-stripping / EMSCRIPTEN_BINDINGS registration"
    fi
  else
    echo "  WARN: WASM binary ${WASM_BINARY} not found — skipping binary-level embind check (build first)" >&2
  fi

  # Emscripten underscore prefix check: C symbols in EXPORTED_FUNCTIONS must use _ prefix
  # R7 CI lesson: gk_* without underscore caused linker error (undefined exported symbol)
  local emsc_issues=0
  if grep -qE 'EXPORTED_FUNCTIONS=[^"]*\bgk_\b' "${cmake_file}" 2>/dev/null; then
    echo "  WARN: EXPORTED_FUNCTIONS contains 'gk_' without Emscripten underscore prefix (should be '_gk_')" >&2
    emsc_issues=1
    if [ "${AUTO_FIX}" = "true" ]; then
      echo "  Auto-fix: adding underscore prefix to gk_* symbols in EXPORTED_FUNCTIONS..." >&2
      sed -i 's/\bgk_\(reserve_buffer\|get_buffer_ptr\|get_buffer_size\|get_buffer_used\|set_buffer_used\|release_buffer\|reset_memory_stats\|get_memory_stats\)/_gk_\1/g' "${cmake_file}"
      if grep -qE 'EXPORTED_FUNCTIONS=[^"]*\bgk_\b' "${cmake_file}" 2>/dev/null; then
        echo "  WARN: auto-fix incomplete — manual review of gk_ symbols in EXPORTED_FUNCTIONS required" >&2
      else
        echo "  Auto-fix: gk_* → _gk_* applied successfully in EXPORTED_FUNCTIONS" >&2
        emsc_issues=0
      fi
    fi
  fi
  if [ "${emsc_issues}" -gt 0 ]; then
    fail "P2_EMSCRIPTEN_PREFIX" 4 "EXPORTED_FUNCTIONS missing underscore prefix for gk_ symbols" "Run with --auto-fix or manually prefix all gk_* with _gk_* in cpp/CMakeLists.txt"
  fi
}

# ---------------------------------------------------------------------------
# P3: Artifact
# ---------------------------------------------------------------------------
check_artifact() {
  # Check JS glue exists (always small, ~tens of KB)
  if [ ! -f "${WASM_JS}" ]; then
    fail "P3_ARTIFACT" 5 "WASM JS glue not found: ${WASM_JS}" "Run build first: emcmake cmake -S cpp -B ${BUILD_DIR} && emmake cmake --build ${BUILD_DIR}"
  fi

  # Check separate .wasm binary exists (split mode, no SINGLE_FILE)
  if [ ! -f "${WASM_BINARY}" ]; then
    fail "P3_ARTIFACT" 5 "WASM binary not found: ${WASM_BINARY}" "Ensure SINGLE_FILE=1 is removed from CMakeLists.txt (split JS+wasm mode)"
  fi

  # Check WASM binary size (the binary, not the JS glue)
  local size
  size="$(stat -c%s "${WASM_BINARY}" 2>/dev/null || stat -f%z "${WASM_BINARY}" 2>/dev/null || echo 0)"
  if [ "${size}" -lt "${WASM_MIN_BYTES}" ] || [ "${size}" -gt "${WASM_MAX_BYTES}" ]; then
    fail "P3_ARTIFACT" 5 "WASM binary size ${size} bytes out of range [${WASM_MIN_BYTES}, ${WASM_MAX_BYTES}]" "Check -Oz/-O3 flag in CMakeLists.txt"
  fi

  # JS glue should be small (< 500KB)
  local js_size
  js_size="$(stat -c%s "${WASM_JS}" 2>/dev/null || stat -f%z "${WASM_JS}" 2>/dev/null || echo 0)"
  if [ "${js_size}" -gt 524288 ]; then
    echo "  WARN: WASM JS glue size ${js_size} bytes > 512KB (SINGLE_FILE may still be active)" >&2
  fi
}

# ---------------------------------------------------------------------------
# P4: Frontend consistency
# ---------------------------------------------------------------------------
check_frontend() {
  if [ ! -f "${HTML_FILE}" ]; then
    fail "P4_FRONTEND" 6 "Frontend file not found: ${HTML_FILE}" "Ensure web/index.html exists in the project"
  fi

  # Ensure fallback inspectSpz is still referenced for older WASM builds.
  if ! grep -qE "inspectSpz\b" "${HTML_FILE}"; then
    fail "P4_FRONTEND" 6 "Fallback inspectSpz not referenced in web/index.html" "Keep inspectSpz fallback for backward compatibility"
  fi

  # If the frontend uses _malloc/_free (zero-copy path), verify they exist in CMakeLists.txt.
  local cmake_file="${PROJECT_DIR}/cpp/CMakeLists.txt"
  if grep -qE "wasmModule\._malloc\b" "${HTML_FILE}"; then
    if ! grep -qE "\<_malloc\>" "${cmake_file}" 2>/dev/null; then
      fail "P4_FRONTEND" 6 "Frontend uses wasmModule._malloc but it is not exported in CMakeLists.txt"
    fi
  fi
  if grep -qE "wasmModule\._free\b" "${HTML_FILE}"; then
    if ! grep -qE "\<_free\>" "${cmake_file}" 2>/dev/null; then
      fail "P4_FRONTEND" 6 "Frontend uses wasmModule._free but it is not exported in CMakeLists.txt"
    fi
  fi

  # Check WASM JS glue + binary pair in web/ directory
  local web_wasm_js="${PROJECT_DIR}/web/spz_gatekeeper_wasm.js"
  local web_wasm_bin="${PROJECT_DIR}/web/spz_gatekeeper_wasm.wasm"
  if [ -f "${web_wasm_js}" ]; then
    if [ ! -f "${web_wasm_bin}" ]; then
      fail "P4_FRONTEND" 6 "WASM JS glue found in web/ but WASM binary missing: ${web_wasm_bin}" "Build WASM first, then copy spz_gatekeeper_wasm.wasm to web/ directory alongside spz_gatekeeper_wasm.js"
    fi
    local wasm_site_size
    wasm_site_size="$(stat -c%s "${web_wasm_bin}" 2>/dev/null || stat -f%z "${web_wasm_bin}" 2>/dev/null || echo 0)"
    if [ "${wasm_site_size}" -lt "${WASM_MIN_BYTES}" ] || [ "${wasm_site_size}" -gt "${WASM_MAX_BYTES}" ]; then
      echo "  WARN: web/spz_gatekeeper_wasm.wasm size ${wasm_site_size} bytes out of range [${WASM_MIN_BYTES}, ${WASM_MAX_BYTES}] — may be stale or corrupted" >&2
    fi
  elif [ -f "${WASM_JS}" ]; then
    # Build output has WASM JS but web/ doesn't — that's OK for deployment, but log it
    echo "  WARN: WASM artifacts in site/ but not in web/ — page served from site/ is fine, but local web/ dev is degraded" >&2
  fi

  # Check for unresolved git conflict markers in all web source files
  local conflict_count=0
  local web_src_files="${PROJECT_DIR}/web/index.html ${PROJECT_DIR}/web/spz_gatekeeper.js ${PROJECT_DIR}/web/smart_memory_manager.js"
  for f in ${web_src_files}; do
    if [ -f "${f}" ]; then
      local markers
      markers="$(grep -cE '<<<<<<< |=======$|>>>>>>> ' "${f}" 2>/dev/null || true)"
      if [ "${markers}" -gt 0 ]; then
        echo "  ERROR: Found ${markers} unresolved conflict marker(s) in ${f}" >&2
        conflict_count=$((conflict_count + markers))
      fi
    fi
  done
  if [ "${conflict_count}" -gt 0 ]; then
    fail "P4_FRONTEND" 6 "Found ${conflict_count} unresolved conflict marker(s) in web/ source files" "Run 'git grep -nE \"<<<<<<< |=======|\>>>>>>> \" web/' to locate and fix them"
  fi
}

# ---------------------------------------------------------------------------
# P5: Smoke test
# ---------------------------------------------------------------------------
check_smoke() {
  local port=4173
  local max_port=4175
  [ "${AUTO_FIX}" = "true" ] && max_port=4175 || max_port=4173
  local server_pid

  for port in $(seq 4173 ${max_port}); do
    python3 -m http.server "${port}" --directory "${SITE_DIR}" >/dev/null 2>&1 &
    server_pid=$!
    trap 'kill "${server_pid}" 2>/dev/null || true' EXIT

    local ready=false
    for _ in $(seq 1 20); do
      if curl -fsS "http://127.0.0.1:${port}/index.html" >/dev/null 2>&1; then
        ready=true
        break
      fi
      sleep 1
    done

    if [ "${ready}" = "true" ]; then
      break
    fi

    # Port in use or server failed - kill and try next
    kill "${server_pid}" 2>/dev/null || true
    if [ "${port}" -lt "${max_port}" ]; then
      echo "  Auto-fix: port ${port} unavailable, trying ${port}..." >&2
    fi
  done

  if [ "${ready}" != "true" ]; then
    fail "P5_SMOKE" 7 "Local HTTP server did not become ready (tried ports 4173-${max_port})"
  fi

  # Minimal runtime sanity: instantiate module via ES module dynamic import
  # (WASM is built with EXPORT_ES6=1, split mode — no CJS require)
  # 运行时级 embind 绑定验证：前端依赖的每个方法必须在实例化模块上真实可用。
  # 此前 inspectSpzWithCompatPtr 绑定缺失时，前端 typeof 检查 false → fallback
  # 双重解压、性能翻倍，但 P2 源码 grep 全绿——此检查在真实模块上逐个拦截。
  local smoke_required=(
    "inspectSpz"
    "inspectSpzPtr"
    "inspectSpzWithCompatPtr"
    "inspectCompatSummaryPtr"
    "inspectCompatSummary"
    "inspectSpzText"
    "dumpTrailer"
    "listRegisteredExtensions"
    "getCompatibilityBoard"
    "describeExtension"
    "buildBrowserAuditReport"
  )
  local smoke_script
  smoke_script="$(printf 'const { default: createModule } = await import(%s);\n' "'${WASM_JS}'")"
  smoke_script+="const m = await createModule();\n"
  smoke_script+="const required = [$(printf '"%s",' "${smoke_required[@]}")];\n"
  smoke_script+="const missing = required.filter(fn => typeof m[fn] !== 'function');\n"
  smoke_script+="if (missing.length) { console.error('MISSING runtime exports:', missing.join(',')); process.exit(1); }\n"
  smoke_script+="process.exit(0);\n"
  if ! node --input-type=module -e "${smoke_script}" >/dev/null 2>&1; then
    fail "P5_SMOKE" 7 "WASM module missing required runtime export(s)" "Run smoke check with debug output; verify EMSCRIPTEN_BINDINGS in wasm_main.cc covers all frontend-required methods"
  fi
}

# ---------------------------------------------------------------------------
# P6: Workflow lint
# ---------------------------------------------------------------------------
check_workflow() {
  if git -C "${PROJECT_DIR}" diff --quiet -- .github/workflows/ 2>/dev/null && \
     git -C "${PROJECT_DIR}" diff --cached --quiet -- .github/workflows/ 2>/dev/null; then
    # No workflow changes; skip lint in non-strict mode.
    if [ "${STRICT}" != "true" ]; then
      return 0
    fi
  fi

  local missing=()
  command -v actionlint >/dev/null 2>&1 || missing+=("actionlint")
  command -v zizmor >/dev/null 2>&1 || missing+=("zizmor")

  if [ ${#missing[@]} -gt 0 ]; then
    if [ "${STRICT}" = "true" ]; then
      fail "P6_WORKFLOW" 8 "Strict mode requires tools: ${missing[*]}"
    fi
    # In default mode, only warn.
    return 0
  fi

  local had_error=false
  for wf in "${PROJECT_DIR}/.github/workflows/"*.yml "${PROJECT_DIR}/.github/workflows/"*.yaml; do
    [ -e "${wf}" ] || continue
    if ! actionlint "${wf}" >/dev/null 2>&1; then
      had_error=true
    fi
    if ! zizmor "${wf}" >/dev/null 2>&1; then
      had_error=true
    fi
  done

  if [ "${had_error}" = "true" ]; then
    fail "P6_WORKFLOW" 8 "actionlint or zizmor reported issues"
  fi
}

# ---------------------------------------------------------------------------
# P7: File integrity (UTF-8 validity + syntax check)
# ---------------------------------------------------------------------------
check_file_integrity() {
  local failures=0

  # Check UTF-8 validity for .cc and .h source files
  while IFS= read -r -d '' f; do
    if ! python3 -c "open('$f','rb').read().decode('utf-8')" 2>/dev/null; then
      echo "  INVALID UTF-8: $f" >&2
      failures=$((failures + 1))
    fi
  done < <(find "${PROJECT_DIR}/cpp" -name '*.cc' -o -name '*.h' 2>/dev/null | tr '\n' '\0')

  if [ "${failures}" -gt 0 ]; then
    fail "P7_INTEGRITY" 9 "${failures} file(s) with UTF-8 encoding errors"
  fi
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
main() {
  cd "${PROJECT_DIR}"
  check_environment
  if [ "${SKIP_BUILD}" != "true" ]; then
    check_build
  fi
  check_symbols
  check_artifact
  check_frontend
  if [ "${SKIP_SMOKE}" != "true" ]; then
    check_smoke
  fi
  check_workflow
  check_file_integrity
  pass
}

main "$@"
