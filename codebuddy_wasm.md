# SPZ Gatekeeper WASM 实施交接文档

> **目标**：为 `spz_gatekeeper` 添加可落地的 WebAssembly 支持。
> 
> **参考项目**：`C:\Users\HP\Downloads\spz_ecosystem_simplified`（`spz2glb` 已完成 WASM）
> 
> **当前项目**：`C:\Users\HP\Downloads\HunYuan3D_test_cases\spz_gatekeeper_project`

---

## 一、先说结论：本次必须纠正的接口事实

当前 `spz_gatekeeper` 的真实公开接口不是文档旧稿里假设的：

- 不存在 `ValidationReport`
- 不存在 `validateSpzBuffer(...)`
- 不存在公开的 `parseSpzHeader(...)`
- 不存在公开的 `extractExtensionInfo(...)`
- `SpzHeader` 目前是 `cpp/src/spz.cc` 内部私有结构，不是头文件 API

当前真实可复用的公开接口是：

- `spz_gatekeeper::SpzInspectOptions`
- `spz_gatekeeper::InspectSpzBlob(...)`
- `spz_gatekeeper::GateReport`
- `GateReport::ToJson()`
- `GateReport::ToText()`

因此，WASM 首版必须走“**包一层现有公开 API**”的路线，而不是重新发明一套 `ValidationReport / parseSpzHeader / extractExtensionInfo` 新接口。

这份文档以下内容已全部按该原则重写。

---

## 二、参考项目真实 WASM 模式（spz2glb）

### 2.1 参考项目里真实存在的关键文件

```text
spz_ecosystem_simplified/
├── tools/spz_to_glb/
│   ├── CMakeLists.txt
│   └── src/
│       ├── spz_to_glb.cpp
│       └── emscripten_utils.h
```

补充说明：

- `dist/` 是 **构建输出目录**，不是当前仓库里稳定存在的源码目录。
- `spz2glb` 的真实模块导出名是 `createSpz2GlbModule`。
- `spz2glb` 的真实业务导出函数是 `convertSpzToGlb`。

### 2.2 参考项目可复用的技术路线

`spz2glb` 的可借鉴点只有“技术路线”，不是业务接口名：

- 使用 `Emscripten + Embind`
- `Uint8Array <-> std::vector<uint8_t>` 数据桥接
- `-sMODULARIZE=1`
- `-sEXPORT_ES6=1`
- `-sSINGLE_FILE=1`
- `-sALLOW_MEMORY_GROWTH=1`
- `-sENVIRONMENT=web`
- `-sUSE_ZLIB=1`

这套模式可以迁移到 `spz_gatekeeper`，但 `spz_gatekeeper` 的导出函数必须围绕 `InspectSpzBlob(...)` 重新设计。

---

## 三、当前 Gatekeeper 真实结构与可复用能力

### 3.1 当前目录结构

```text
spz_gatekeeper_project/
├── cpp/
│   ├── CMakeLists.txt
│   ├── include/spz_gatekeeper/
│   │   ├── report.h
│   │   ├── spz.h
│   │   ├── tlv.h
│   │   ├── extension_validator.h
│   │   ├── validator_registry.h
│   │   └── safe_orbit_camera_validator.h
│   ├── src/
│   │   ├── main.cc
│   │   ├── spz.cc
│   │   ├── tlv.cc
│   │   ├── report.cc
│   │   └── json_min.cc
│   ├── extensions/
│   └── tests/
├── build_wsl/
└── README.md
```

### 3.2 当前公开 API

`cpp/include/spz_gatekeeper/spz.h`：

```cpp
struct SpzInspectOptions {
  bool strict = true;
};

GateReport InspectSpzBlob(const std::vector<std::uint8_t>& gz_spz,
                          const SpzInspectOptions& opt,
                          const std::string& where);

double sh_epsilon(int bits);
```

`cpp/include/spz_gatekeeper/report.h`：

```cpp
struct Issue;
struct TlvRecord;
struct SpzL2Info;
struct ExtensionReport;
struct GateReport;

bool GateReport::HasErrors() const;
std::string GateReport::ToJson() const;
std::string GateReport::ToText() const;
```

### 3.3 当前 CLI 已经验证过的 JSON 语义

当前 `main.cc` 已经有两条非常重要的现成语义，可以直接映射到 WASM：

1. `check-spz --json`
   - 本质是：`InspectSpzBlob(...) -> GateReport::ToJson()`

2. `dump-trailer --json`
   - 本质是：`InspectSpzBlob(...) -> 读取 rep.spz_l2->tlv_records -> 输出 trailer 摘要 JSON`

因此，WASM 最小可落地方案应该直接复用这两条路径。

---

## 四、首版 WASM 的正确接口设计

## 4.1 设计原则

首版只做三件事：

1. **完整检查**：对 `.spz` 二进制执行 `InspectSpzBlob(...)`
2. **TLV 摘要查看**：复用 `dump-trailer --json` 的结构化输出
3. **文本输出**：复用 `GateReport::ToText()`，便于调试

首版**不做**这些事：

- 不新增公开 `SpzHeader` 结构
- 不导出 `parseSpzHeader(...)`
- 不新造 `ValidationReport`
- 不在第一版拆出“header-only 快速校验 API”

如果后续真的需要 header-only 快速路径，先在 `spz.h` 正式增加公共 API，再让 WASM 调用；不要在 `wasm_main.cc` 里直接绕过头文件访问 `spz.cc` 私有实现。

## 4.2 建议导出 API

```javascript
const Module = await createSpzGatekeeperModule();

const full = Module.inspectSpz(spzUint8Array, true);
const trailer = Module.dumpTrailer(spzUint8Array, true);
const text = Module.inspectSpzText(spzUint8Array, true);
```

### `inspectSpz(spzUint8Array, strict)`

输入：
- `Uint8Array`
- `strict: boolean`

输出：
- JavaScript 对象
- 结构直接来自 `GateReport::ToJson()`，外加一层 `ok` 字段

示例返回：

```json
{
  "asset_path": "<wasm>",
  "issues": [],
  "extension_reports": [],
  "spz_l2": {
    "header_ok": true,
    "version": 3,
    "num_points": 123,
    "sh_degree": 0,
    "flags": 2,
    "reserved": 0,
    "decompressed_size": 4096,
    "base_payload_size": 4000,
    "trailer_size": 96,
    "tlv_records": [
      { "type": 2913867777, "length": 88, "offset": 4000 }
    ]
  },
  "ok": true,
  "strict": true
}
```

### `dumpTrailer(spzUint8Array, strict)`

输入：
- `Uint8Array`
- `strict: boolean`

输出：
- JavaScript 对象
- 结构与 CLI `dump-trailer --json` 对齐

示例返回：

```json
{
  "asset_path": "<wasm>",
  "flags": 2,
  "trailer_size": 96,
  "tlv_records": [
    { "type": 2913867777, "length": 88, "offset": 4000 }
  ],
  "ok": true,
  "strict": true
}
```

### `inspectSpzText(spzUint8Array, strict)`

输入：
- `Uint8Array`
- `strict: boolean`

输出：
- `string`
- 直接来自 `GateReport::ToText()`

---

## 五、实施方案

## Phase 1：修改 `cpp/CMakeLists.txt`

### 5.1 修改目标

在保持现有桌面版构建不变的前提下，新增一个专用 Emscripten 目标：

- `spz_gatekeeper-wasm`

这个目标应当：

- 复用现有 `spz_gatekeeper_core`
- 新增一个 `src/wasm_main.cc`
- 输出到 `spz_gatekeeper_project/dist/`

### 5.2 推荐改法

```cmake
# 在 project(...) 之后添加
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(SPZ_GATEKEEPER_BUILD_WASM ON)
  if(NOT DEFINED SPZ_GATEKEEPER_USE_EMSCRIPTEN_ZLIB)
    set(SPZ_GATEKEEPER_USE_EMSCRIPTEN_ZLIB ON)
  endif()
  message(STATUS "Emscripten detected: WASM build enabled")
endif()

option(SPZ_GATEKEEPER_BUILD_WASM "Build WASM version" OFF)

# ZLIB 配置改为条件分支
if(SPZ_GATEKEEPER_USE_EMSCRIPTEN_ZLIB)
  message(STATUS "Using Emscripten port of ZLIB")
else()
  find_package(ZLIB REQUIRED)
endif()
```

现有核心库链接也要配套改成：

```cmake
target_link_libraries(spz_gatekeeper_core
  PUBLIC
    $<$<NOT:$<BOOL:${SPZ_GATEKEEPER_USE_EMSCRIPTEN_ZLIB}>>:ZLIB::ZLIB>
)
```

### 5.3 新增 WASM 目标

```cmake
if(SPZ_GATEKEEPER_BUILD_WASM)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dist")

  add_executable(spz_gatekeeper-wasm
    src/wasm_main.cc
  )

  target_link_libraries(spz_gatekeeper-wasm PRIVATE spz_gatekeeper_core)

  target_link_options(spz_gatekeeper-wasm PRIVATE
    "-O3"
    "-sUSE_ZLIB=1"
    "--bind"
    "--no-entry"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createSpzGatekeeperModule"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sENVIRONMENT=web"
    "-sEXPORT_ES6=1"
    "-sSINGLE_FILE=1"
    "-sEXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\"]"
  )

  set_target_properties(spz_gatekeeper-wasm PROPERTIES
    OUTPUT_NAME "spz_gatekeeper"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dist"
  )
endif()
```

### 5.4 为什么这里必须加 `--no-entry`

因为本方案的 `wasm_main.cc` 是“导出 Embind 函数的库入口”，没有原生 `main()`。  
如果不加 `--no-entry`，Emscripten 链接阶段会把它当成普通可执行程序处理，导致入口约束不匹配。

---

## Phase 2：创建 `cpp/include/spz_gatekeeper/emscripten_utils.h`

该文件只保留最小桥接能力：

```cpp
// Copyright (c) 2026 PuJunhan
// SPDX-License-Identifier: MIT

#ifndef SPZ_GATEKEEPER_EMSCRIPTEN_UTILS_H_
#define SPZ_GATEKEEPER_EMSCRIPTEN_UTILS_H_

#ifdef __EMSCRIPTEN__

#include <cstdint>
#include <emscripten/val.h>
#include <vector>

namespace spz_gatekeeper {

inline std::vector<uint8_t> vectorFromJsArray(const emscripten::val& array) {
  const size_t length = array["length"].as<size_t>();
  std::vector<uint8_t> out(length);
  for (size_t i = 0; i < length; ++i) {
    out[i] = array[i].as<unsigned char>();
  }
  return out;
}

}  // namespace spz_gatekeeper

#endif  // __EMSCRIPTEN__

#endif  // SPZ_GATEKEEPER_EMSCRIPTEN_UTILS_H_
```

说明：

- `spz_gatekeeper` 不需要像 `spz2glb` 一样返回 `Uint8Array`，因为这里的主要输出是 JSON 对象和文本。
- 因此首版只需要 `Uint8Array -> vector<uint8_t>` 即可。

---

## Phase 3：创建 `cpp/src/wasm_main.cc`

### 7.1 设计要点

`wasm_main.cc` 必须只依赖现有公开 API：

- `InspectSpzBlob(...)`
- `GateReport::ToJson()`
- `GateReport::ToText()`

其中：

- `inspectSpz(...)`：直接把 `GateReport::ToJson()` 解析成 JS 对象
- `dumpTrailer(...)`：按 `main.cc` 的 `dump-trailer --json` 逻辑重建摘要 JSON
- `inspectSpzText(...)`：直接返回文本

### 7.2 推荐实现

```cpp
// Copyright (c) 2026 PuJunhan
// SPDX-License-Identifier: MIT

#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <sstream>
#include <string>
#include <vector>

#include "spz_gatekeeper/emscripten_utils.h"
#include "spz_gatekeeper/json_min.h"
#include "spz_gatekeeper/report.h"
#include "spz_gatekeeper/spz.h"

namespace {

emscripten::val ParseJsonObject(const std::string& json) {
  return emscripten::val::global("JSON").call<emscripten::val>("parse", json);
}

std::string BuildTrailerJson(const spz_gatekeeper::GateReport& rep, const std::string& asset_path) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"asset_path\":\"" << spz_gatekeeper::JsonEscape(asset_path) << "\"";

  if (rep.spz_l2.has_value()) {
    oss << ",\"flags\":" << static_cast<unsigned>(rep.spz_l2->flags);
    oss << ",\"trailer_size\":" << rep.spz_l2->trailer_size;
    oss << ",\"tlv_records\":[";
    for (std::size_t i = 0; i < rep.spz_l2->tlv_records.size(); ++i) {
      if (i) oss << ",";
      const auto& r = rep.spz_l2->tlv_records[i];
      oss << "{";
      oss << "\"type\":" << r.type;
      oss << ",\"length\":" << r.length;
      oss << ",\"offset\":" << r.offset;
      oss << "}";
    }
    oss << "]";
  } else {
    oss << ",\"error\":\"no l2 info\"";
  }

  oss << "}";
  return oss.str();
}

spz_gatekeeper::GateReport Inspect(const emscripten::val& spz_buffer, bool strict) {
  std::vector<std::uint8_t> data = spz_gatekeeper::vectorFromJsArray(spz_buffer);
  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = strict;
  return spz_gatekeeper::InspectSpzBlob(data, opt, "<wasm>");
}

}  // namespace

emscripten::val inspectSpz(const emscripten::val& spz_buffer, bool strict) {
  spz_gatekeeper::GateReport rep = Inspect(spz_buffer, strict);
  emscripten::val result = ParseJsonObject(rep.ToJson());
  result.set("ok", !rep.HasErrors());
  result.set("strict", strict);
  return result;
}

emscripten::val dumpTrailer(const emscripten::val& spz_buffer, bool strict) {
  spz_gatekeeper::GateReport rep = Inspect(spz_buffer, strict);
  emscripten::val result = ParseJsonObject(BuildTrailerJson(rep, "<wasm>"));
  result.set("ok", !rep.HasErrors());
  result.set("strict", strict);
  return result;
}

std::string inspectSpzText(const emscripten::val& spz_buffer, bool strict) {
  spz_gatekeeper::GateReport rep = Inspect(spz_buffer, strict);
  return rep.ToText();
}

EMSCRIPTEN_BINDINGS(spz_gatekeeper_module) {
  emscripten::function("inspectSpz", &inspectSpz);
  emscripten::function("dumpTrailer", &dumpTrailer);
  emscripten::function("inspectSpzText", &inspectSpzText);
}

#endif  // __EMSCRIPTEN__
```

### 7.3 为什么不导出 `validateSpzHeader`

因为当前项目里没有公开 `parseSpzHeader(...)`。  
如果在 `wasm_main.cc` 里直接复制 `spz.cc` 的私有 `ParseHeader(...)` 逻辑，会导致：

- 公开 API 和私有实现分叉
- 后续修改 header 规则时容易漏改
- 桌面版与 WASM 版出现行为漂移

所以首版先不做 header-only 接口。

---

## Phase 4：创建 `dist/index.html`

### 8.1 页面目标

只做一个最小可用页面：

- 选择 `.spz`
- 调 `Module.inspectSpz(...)`
- 展示 `issues / spz_l2 / tlv_records`

### 8.2 推荐页面

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>SPZ Gatekeeper</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      max-width: 960px;
      margin: 0 auto;
      padding: 24px;
      background: #f5f7fb;
      color: #222;
    }
    .card {
      background: #fff;
      border-radius: 12px;
      padding: 24px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.08);
    }
    .drop-zone {
      border: 2px dashed #9aa4b2;
      border-radius: 12px;
      padding: 32px;
      text-align: center;
      cursor: pointer;
      background: #fafcff;
    }
    .drop-zone.dragover {
      border-color: #2563eb;
      background: #eef4ff;
    }
    button {
      margin-top: 16px;
      padding: 10px 18px;
      border: 0;
      border-radius: 8px;
      background: #2563eb;
      color: #fff;
      cursor: pointer;
    }
    button:disabled {
      background: #9aa4b2;
      cursor: not-allowed;
    }
    pre {
      margin-top: 16px;
      padding: 16px;
      border-radius: 8px;
      overflow-x: auto;
      background: #0f172a;
      color: #e2e8f0;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>SPZ Gatekeeper</h1>
    <p>浏览器内执行 SPZ 合法性检查与 TLV trailer 摘要查看。</p>

    <div id="drop-zone" class="drop-zone">
      点击或拖拽选择 `.spz` 文件
      <input id="file-input" type="file" accept=".spz" hidden>
    </div>

    <div id="file-meta" style="margin-top: 12px;"></div>
    <button id="inspect-btn" disabled>检查文件</button>
    <pre id="output">等待加载 WASM...</pre>
  </div>

  <script type="module">
    import createSpzGatekeeperModule from './spz_gatekeeper.js';

    const dropZone = document.getElementById('drop-zone');
    const fileInput = document.getElementById('file-input');
    const fileMeta = document.getElementById('file-meta');
    const inspectBtn = document.getElementById('inspect-btn');
    const output = document.getElementById('output');

    let moduleInstance = null;
    let spzData = null;

    output.textContent = '加载 WASM 中...';
    moduleInstance = await createSpzGatekeeperModule();
    output.textContent = 'WASM 已加载，请选择 .spz 文件。';

    dropZone.addEventListener('click', () => fileInput.click());
    dropZone.addEventListener('dragover', (event) => {
      event.preventDefault();
      dropZone.classList.add('dragover');
    });
    dropZone.addEventListener('dragleave', () => {
      dropZone.classList.remove('dragover');
    });
    dropZone.addEventListener('drop', async (event) => {
      event.preventDefault();
      dropZone.classList.remove('dragover');
      const [file] = event.dataTransfer.files;
      if (file) await handleFile(file);
    });
    fileInput.addEventListener('change', async (event) => {
      const [file] = event.target.files;
      if (file) await handleFile(file);
    });

    async function handleFile(file) {
      if (!file.name.endsWith('.spz')) {
        output.textContent = '请选择 .spz 文件。';
        return;
      }
      spzData = new Uint8Array(await file.arrayBuffer());
      fileMeta.textContent = `文件：${file.name}，大小：${spzData.length} bytes`;
      inspectBtn.disabled = false;
      output.textContent = '文件已就绪，点击“检查文件”。';
    }

    inspectBtn.addEventListener('click', () => {
      if (!moduleInstance || !spzData) return;
      const result = moduleInstance.inspectSpz(spzData, true);
      output.textContent = JSON.stringify(result, null, 2);
    });
  </script>
</body>
</html>
```

---

## Phase 5：GitHub Actions（可选，放到本地打通之后）

只有在本地 WASM 产物已经打通后，再补 CI。推荐最小工作流：

```yaml
name: Build WASM

on:
  workflow_dispatch:
  push:
    branches: [ main ]
    paths:
      - 'spz_gatekeeper_project/cpp/**'
      - 'spz_gatekeeper_project/dist/**'
      - '.github/workflows/spz-gatekeeper-wasm.yml'

jobs:
  build-wasm:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v5

      - name: Setup Emscripten
        uses: mymindstorm/setup-emsdk@v14
        with:
          version: latest

      - name: Configure
        working-directory: spz_gatekeeper_project/cpp
        run: emcmake cmake -B build_wasm -DSPZ_GATEKEEPER_BUILD_WASM=ON

      - name: Build
        working-directory: spz_gatekeeper_project/cpp
        run: emmake cmake --build build_wasm --config Release

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: spz-gatekeeper-wasm
          path: spz_gatekeeper_project/dist/
```

说明：

- 首版先做构建产物上传，不强行接 GitHub Pages。
- Pages 部署属于第二阶段，不要和接口修正混在一起。

---

## 六、本地构建命令（仅 WSL）

注意：当前项目规则要求 **只在 WSL 下构建和验证**。  
另外，如本机尚未安装 `emsdk`，安装动作涉及下载，需单独审批后执行。

### 6.1 配置与构建

```bash
cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -B build_wasm -DSPZ_GATEKEEPER_BUILD_WASM=ON
emmake cmake --build build_wasm --config Release
```

### 6.2 本地预览

```bash
cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/dist
python3 -m http.server 8080
```

打开：`http://localhost:8080`

---

## 七、最终导出 API 清单

```javascript
const Module = await createSpzGatekeeperModule();

const report = Module.inspectSpz(spzUint8Array, true);
// 返回 GateReport JSON 对象 + { ok, strict }

const trailer = Module.dumpTrailer(spzUint8Array, true);
// 返回 dump-trailer 风格 JSON 对象 + { ok, strict }

const text = Module.inspectSpzText(spzUint8Array, true);
// 返回文本报告字符串
```

---

## 八、完成标准

- [ ] `cpp/CMakeLists.txt` 支持 Emscripten 构建
- [ ] `cpp/include/spz_gatekeeper/emscripten_utils.h` 创建完成
- [ ] `cpp/src/wasm_main.cc` 创建完成
- [ ] `inspectSpz / dumpTrailer / inspectSpzText` 三个导出函数可用
- [ ] `dist/index.html` 可在浏览器加载并调用 WASM
- [ ] WSL 下本地编译通过
- [ ] 浏览器中能成功检查真实 `.spz` 文件
- [ ] CI 构建产物上传可选完成

---

## 九、实施顺序建议

1. 先改 `CMakeLists.txt`
2. 再写 `emscripten_utils.h`
3. 再写 `wasm_main.cc`
4. 用最小 `index.html` 打通浏览器调用
5. 最后再补 CI

不要反过来先写页面或先写 GitHub Actions；接口和构建链路必须先打通。

---

**预计工作量**：4-6 小时  
**难度**：中等  
**前置条件**：WSL 可用，且 `emsdk` 已准备好或已获批可安装
