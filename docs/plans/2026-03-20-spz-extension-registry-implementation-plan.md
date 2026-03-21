# SPZ Extension Registry and Self-Test Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 为 `spz_gatekeeper` 落地双层扩展注册表、快速自测 CLI、真实 WASM/Web 接线与兼容性看板基础数据面。

**Architecture:** 保持现有 `ExtensionValidatorRegistry` 不变，新增 `ExtensionSpecRegistry` 承担规范元数据登记；由 `InspectSpzBlob(...)` 同时消费 spec 与 validator 两层注册表，增强 `GateReport` 输出。CLI、WASM 与 Web 全部复用增强后的报告格式与注册表查询接口，避免各层重复维护扩展元数据。

**Tech Stack:** C++17、CMake、zlib、Emscripten、原生 HTML/CSS/JS、GitHub Pages、CTest

---

### Task 1: 落地 `ExtensionSpecRegistry` 核心 API

**Files:**
- Create: `cpp/include/spz_gatekeeper/extension_spec_registry.h`
- Create: `cpp/src/extension_spec_registry.cc`
- Modify: `cpp/CMakeLists.txt`
- Test: `cpp/tests/extension_spec_registry_test.cc`

**Step 1: 写失败测试，覆盖注册/查询/枚举/覆盖行为**

```cpp
void test_register_and_lookup_spec() {
  spz_gatekeeper::ExtensionSpec spec;
  spec.type = 0xADBE0002u;
  spec.vendor_id = 0xADBEu;
  spec.extension_id = 0x0002u;
  spec.vendor_name = "Adobe";
  spec.extension_name = "Safe Orbit Camera";

  auto& reg = spz_gatekeeper::ExtensionSpecRegistry::Instance();
  reg.RegisterSpec(spec);
  auto got = reg.GetSpec(spec.type);
  ASSERT_TRUE(got.has_value());
  ASSERT_EQ(got->vendor_name, "Adobe");
}
```

**Step 2: 运行单测，确认当前失败**

Run:
```bash
wsl bash -lc "cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel --target extension_spec_registry_test"
```

Expected: 构建失败，提示缺少 `ExtensionSpec` / `ExtensionSpecRegistry`

**Step 3: 实现最小 API**

实现内容：
- `struct ExtensionSpec`
- `class ExtensionSpecRegistry`
- `RegisterSpec / GetSpec / HasSpec / ListSpecs / SpecCount`
- `std::map<uint32_t, ExtensionSpec>` + `std::mutex`

**Step 4: 将新源文件加入核心库与测试目标**

在 `cpp/CMakeLists.txt` 中把 `src/extension_spec_registry.cc` 加入 `SPZ_GATEKEEPER_CORE_SOURCES`，并注册 `extension_spec_registry_test`。

**Step 5: 运行测试，确认通过**

Run:
```bash
wsl bash -lc "cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && cmake --build build --parallel && ctest --test-dir build -R extension_spec_registry_test --output-on-failure"
```

Expected: PASS

---

### Task 2: 接入内置 spec，并增强 `GateReport`

**Files:**
- Modify: `cpp/include/spz_gatekeeper/report.h`
- Modify: `cpp/src/report.cc`
- Modify: `cpp/src/spz.cc`
- Modify: `cpp/include/spz_gatekeeper/safe_orbit_camera_validator.h`（如需补齐名称常量）
- Test: `cpp/tests/extension_integration_test.cc`

**Step 1: 先写失败测试，覆盖 4 种扩展状态**

新增测试场景：
- 已登记 + 有 validator
- 已登记 + 无 validator
- 未登记 + 有 validator
- 未登记 + 无 validator

示例断言：

```cpp
ASSERT_TRUE(report.extension_reports[0].known_extension);
ASSERT_TRUE(report.extension_reports[0].has_validator);
ASSERT_EQ(report.extension_reports[0].status, "stable");
```

**Step 2: 运行对应测试，确认失败**

Run:
```bash
wsl bash -lc "cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && ctest --test-dir build -R extension_integration_test --output-on-failure"
```

Expected: 编译失败或断言失败，因为新字段尚不存在

**Step 3: 扩展 `ExtensionReport` 字段**

在 `report.h` 中新增：
- `known_extension`
- `has_validator`
- `status`
- `category`
- `spec_url`
- `short_description`

并在 `report.cc` 中同步更新 `ToJson()` / `ToText()`。

**Step 4: 在 `spz.cc` 中接入 `ExtensionSpecRegistry`**

实现要点：
- 启动时自动注册 Adobe `ExtensionSpec`
- 逐条 TLV 先查 spec，再查 validator
- 生成增强版 `ExtensionReport`
- 为“已登记但无 validator”等情况补充 issue code
- 将 `GetVendorName(...)` 改为兜底逻辑，而不是主数据源

**Step 5: 运行集成测试并修正文本/JSON 输出**

Run:
```bash
wsl bash -lc "cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && cmake --build build --parallel && ctest --test-dir build -R 'extension_integration_test|spz_gatekeeper_unit_test' --output-on-failure"
```

Expected: PASS

---

### Task 3: 新增 `registry` CLI 命令

**Files:**
- Modify: `cpp/src/main.cc`
- Test: `cpp/tests/registry_cli_test.cc`
- Modify: `README.md`
- Modify: `README-zh.md`

**Step 1: 写失败测试，定义 CLI 输出契约**

覆盖：
- `spz_gatekeeper registry --json`
- `spz_gatekeeper registry list --json`
- `spz_gatekeeper registry show 0xADBE0002 --json`

断言示例：

```cpp
ASSERT_TRUE(output.find("Safe Orbit Camera") != std::string::npos);
ASSERT_TRUE(output.find("stable") != std::string::npos);
```

**Step 2: 运行测试，确认失败**

Run:
```bash
wsl bash -lc "cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && ctest --test-dir build -R registry_cli_test --output-on-failure"
```

Expected: 未知命令或测试目标不存在

**Step 3: 在 `main.cc` 中实现命令分发**

实现：
- `registry`
- `registry list`
- `registry show <type>`
- 支持文本与 JSON 输出

要求：
- 文本模式适合终端查看
- JSON 模式字段完整，供 Web/脚本复用

**Step 4: 更新 README 中的 CLI 说明**

补充：
- 新命令语法
- registry 的使用场景

**Step 5: 运行测试验证**

Run:
```bash
wsl bash -lc "cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && cmake --build build --parallel && ctest --test-dir build -R 'registry_cli_test|spz_gatekeeper_self_test' --output-on-failure"
```

Expected: PASS

---

### Task 4: 新增 `compat-check` 与 `gen-fixture`

**Files:**
- Modify: `cpp/src/main.cc`
- Create: `cpp/tests/compat_check_test.cc`
- Create: `cpp/tests/gen_fixture_test.cc`
- Possibly Extract: `cpp/tests/test_fixtures.h`（若要复用最小 SPZ/TLV 构造逻辑）
- Modify: `docs/Implementing_Custom_Extension.md`

**Step 1: 先提取测试侧通用构造函数**

从 `cpp/tests/extension_integration_test.cc` 中抽出：
- 最小 SPZ 构造
- TLV record 构造
- gzip 压缩

避免 CLI 生成 fixture 与测试生成样例各维护一套逻辑。

**Step 2: 写 `gen-fixture` 失败测试**

覆盖：
- 生成合法 Adobe fixture
- 生成错误长度 fixture
- 生成未知 type fixture

**Step 3: 实现 `gen-fixture`**

首版只支持：
- `--type`
- `--mode valid|invalid-size`
- `--out`

若 type 未知：
- 生成占位 TLV payload
- 明确在输出中标注 `placeholder`

**Step 4: 写 `compat-check` 失败测试**

覆盖：
- strict / non-strict 双路径结果
- extension summary
- issue summary

**Step 5: 实现 `compat-check`**

首版仅做仓库内稳定检查：
- strict 模式
- non-strict 模式
- registry / extension 汇总

外部工具兼容性留为第二轮：
- 若 `spz_info` / `spz_to_ply` 不存在则输出 `skipped`

**Step 6: 运行测试验证**

Run:
```bash
wsl bash -lc "cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && cmake --build build --parallel && ctest --test-dir build -R 'compat_check_test|gen_fixture_test|extension_integration_test' --output-on-failure"
```

Expected: PASS

---

### Task 5: 增强 WASM 导出与修复 Web 空壳

**Files:**
- Modify: `cpp/src/wasm_main.cc`
- Modify: `web/index.html`
- Test: `web/index.html` 手工验证 + Pages 构建验证

**Step 1: 为 WASM 增加注册表查询导出**

新增导出：
- `listRegisteredExtensions()`
- `describeExtension(type)`

返回结构必须与 CLI JSON 口径一致。

**Step 2: 在 `web/index.html` 中真实加载模块**

实现：
- `import createSpzGatekeeperModule from './spz_gatekeeper.js'`
- 初始化模块成功后再切换状态文本为“WASM 引擎就绪”
- 初始化失败时显示错误

**Step 3: 替换当前 `alert` 交互为真实校验结果区**

结果区至少展示：
- Summary（Pass / Warning / Error）
- Issues
- Extensions
- Trailer TLV 列表

**Step 4: 加入 registry viewer 轻量区块**

不额外引入页面，只在单页内增加：
- 当前内置已登记扩展数
- 可展开查看条目

**Step 5: 本地验证 Web + WASM**

Run:
```bash
wsl bash -lc "source /home/linuxmmlsh/.venv/hunyuan/bin/activate >/dev/null 2>&1 || true; cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && emcmake cmake -S cpp -B build-pages -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF && emmake cmake --build build-pages --parallel"
```

然后：
```bash
wsl bash -lc "cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/web && python3 -m http.server 8080"
```

Expected:
- 页面显示真实 WASM 加载状态
- 上传 `.spz` 能显示 JSON 解析结果而非 `alert`

---

### Task 6: 输出兼容性看板基础数据，不做算法排行榜

**Files:**
- Modify: `cpp/src/main.cc`
- Modify: `cpp/src/wasm_main.cc`
- Create: `docs/extension_registry.json`
- Modify: `docs/Vendor_ID_Allocation.md`
- Modify: `README.md`
- Modify: `README-zh.md`

**Step 1: 定义兼容性看板 JSON 结构**

输出字段：
- `type`
- `vendor_name`
- `extension_name`
- `status`
- `has_spec`
- `has_validator`
- `fixture_valid_pass`
- `fixture_invalid_pass`
- `strict_check_pass`
- `non_strict_check_pass`

**Step 2: 生成机器可读的注册表镜像**

创建 `docs/extension_registry.json`，内容至少包含当前内置 Adobe 扩展。

**Step 3: 更新文档，明确“不做算法排行榜，只做兼容性成熟度看板”**

在中英文 README 与 `Vendor_ID_Allocation.md` 中同步说明：
- registry 的角色
- self-test 的角色
- compatibility board 的角色

**Step 4: 运行回归测试**

Run:
```bash
wsl bash -lc "cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && cmake --build build --parallel && ctest --test-dir build --output-on-failure"
```

Expected: 全量 PASS

**Step 5: 验证 Pages 构建**

Run:
```bash
wsl bash -lc "cd /mnt/c/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project && emcmake cmake -S cpp -B build-pages -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF && emmake cmake --build build-pages --parallel"
```

Expected: 生成 `spz_gatekeeper.js`，页面可加载，registry 数据可见

---

## 执行注意事项

1. 全程在 WSL 中执行，不在 Windows 侧跑构建或测试。
2. 先补测试，再写实现，遵守最小改动原则。
3. 不删除用户现有未跟踪资产，例如 `web/cat_left.png`、`web/cat_right.png`。
4. 新增 JSON 字段采用增量方式，不破坏旧字段。
5. 不引入前端框架，不改造为多页应用。

## 推荐提交节奏

- `feat: add ExtensionSpecRegistry and spec tests`
- `feat: enrich extension reports with registry metadata`
- `feat: add registry CLI for registered extensions`
- `feat: add compat-check and gen-fixture commands`
- `feat: wire WASM validator into web UI`
- `docs: publish extension registry and compatibility board notes`

## 完成定义

以下全部满足才算完成：

- `registry`、`compat-check`、`gen-fixture` 可用
- `check-spz --json` 返回增强后的 `extension_reports`
- Web 页面不再是假加载/假处理
- Pages 构建成功
- 中英文文档同步说明新能力
