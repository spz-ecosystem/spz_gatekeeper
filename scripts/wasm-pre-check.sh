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
  "dumpTrailer"
  "listRegisteredExtensions"
  "getCompatibilityBoard"
  "inspectCompatSummary"
  # C runtime exports (CMakeLists.txt, in WASM binary)
  "_malloc"
  "_free"
)
# JS-wrapped functions (spz_gatekeeper.js, not in WASM binary)
REQUIRED_JS_WRAPPERS=(
  "auditWasmBundle"
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
  rm -rf "${BUILD_DIR}"
  if ! emcmake cmake -S "${PROJECT_DIR}/cpp" -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF \
      -DSPZ_GATEKEEPER_BUILD_BENCHMARK_TESTS=OFF \
      -DSPZ_GATEKEEPER_BUILD_WASM=ON >/dev/null 2>&1; then
    fail "P1_BUILD" 3 "emcmake configuration failed" "Check CMake output: emcmake cmake -S cpp -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Release"
  fi

  if ! emmake cmake --build "${BUILD_DIR}" --parallel >/dev/null 2>&1; then
    fail "P1_BUILD" 3 "emmake build failed" "Check compiler errors in C++ source files"
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
      _malloc|_free)
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
}

# ---------------------------------------------------------------------------
# P3: Artifact
# ---------------------------------------------------------------------------
check_artifact() {
  if [ ! -f "${WASM_JS}" ]; then
    fail "P3_ARTIFACT" 5 "WASM artifact not found: ${WASM_JS}" "Run build first: emcmake cmake -S cpp -B ${BUILD_DIR} && emmake cmake --build ${BUILD_DIR}"
  fi

  local size
  size="$(stat -c%s "${WASM_JS}" 2>/dev/null || stat -f%z "${WASM_JS}" 2>/dev/null || echo 0)"
  if [ "${size}" -lt "${WASM_MIN_BYTES}" ] || [ "${size}" -gt "${WASM_MAX_BYTES}" ]; then
    fail "P3_ARTIFACT" 5 "WASM artifact size ${size} bytes out of range [${WASM_MIN_BYTES}, ${WASM_MAX_BYTES}]" "Check -Oz/-O3 flag and -sSINGLE_FILE=1 in CMakeLists.txt"
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

  # If the frontend uses _malloc/_free (zero-copy path), verify they are exported.
  if grep -qE "wasmModule\._malloc\b" "${HTML_FILE}"; then
    if ! grep -qE "\\b_malloc\\b" "${BUILD_DIR}/wasm-exports.txt" 2>/dev/null; then
      fail "P4_FRONTEND" 6 "Frontend uses wasmModule._malloc but it is not exported"
    fi
  fi
  if grep -qE "wasmModule\._free\b" "${HTML_FILE}"; then
    if ! grep -qE "\\b_free\\b" "${BUILD_DIR}/wasm-exports.txt" 2>/dev/null; then
      fail "P4_FRONTEND" 6 "Frontend uses wasmModule._free but it is not exported"
    fi
  fi
}

# ---------------------------------------------------------------------------
# P5: Smoke test
# ---------------------------------------------------------------------------
check_smoke() {
  local port=4173
  local server_pid
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

  if [ "${ready}" != "true" ]; then
    fail "P5_SMOKE" 7 "Local HTTP server did not become ready"
  fi

  # Minimal runtime sanity: the module can be instantiated.
  if ! node -e "
const createModule = require('${WASM_JS}');
createModule().then(m => {
  if (typeof m.inspectSpz !== 'function' && typeof m.inspectSpzPtr !== 'function') {
    process.exit(1);
  }
  process.exit(0);
}).catch(() => process.exit(1));
" >/dev/null 2>&1; then
    fail "P5_SMOKE" 7 "WASM module failed to instantiate or missing expected exports"
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
  pass
}

main "$@"
