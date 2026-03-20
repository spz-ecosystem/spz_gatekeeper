# SPZ Gatekeeper 扩展注册表、自测闭环与 Web 接线设计

## 1. 背景

`spz_gatekeeper` 当前已经具备以下能力：

- 基于 `InspectSpzBlob(...)` 完成 gzip 解压、header 校验、base payload 尺寸推导、TLV trailer 解析与扩展验证。
- 通过 `ExtensionValidatorRegistry` 完成运行时扩展验证器查找与调用。
- 通过 `check-spz`、`dump-trailer`、`guide`、`--self-test` 提供基础 CLI 能力。
- 通过 `wasm_main.cc` 暴露 `inspectSpz`、`dumpTrailer`、`inspectSpzText` 三个 WebAssembly 接口。

但当前仍存在 4 个结构性缺口：

1. **只有验证器注册，没有规范元数据注册**。
   - `vendor_name` 仍在 `spz.cc` 中硬编码。
   - 无法表达“已登记但暂未实现 validator”的扩展。
2. **用户自测闭环不完整**。
   - 测试里已有最小 SPZ/TLV 构造能力，但未产品化为公开 CLI。
3. **Web 页面仍是空壳**。
   - 页面有上传交互，但尚未真正接入 WASM 模块与 JSON 报告展示。
4. **缺少公开的扩展治理入口**。
   - 目前只有 `docs/Vendor_ID_Allocation.md` 这样的静态文档，不具备 glTF 扩展注册表那种“可枚举、可查询、可展示”的结构化能力。

本设计的目标是分阶段补齐以上缺口，并保持门卫的核心职责不变：

- 以 **L2 合法性与兼容性校验** 为核心；
- 不参与具体压缩算法优劣排名；
- 优先提供 **登记、验证、自测、兼容性展示** 能力。

---

## 2. 设计目标

### 2.1 目标

1. 新增 **双层注册表**：
   - `ExtensionSpecRegistry`：登记扩展规范元数据。
   - `ExtensionValidatorRegistry`：保留现有运行时验证器注册能力。
2. 新增 3 个 CLI 命令：
   - `registry`
   - `compat-check`
   - `gen-fixture`
3. 打通 Web 页面：
   - 真正加载 `spz_gatekeeper.js`
   - 读取 `.spz` 文件并调用 WASM 接口
   - 展示 L2 校验结果、TLV 列表、扩展登记状态
4. 建立“快速自测闭环”：
   - 让扩展作者能在最短路径内验证 `type`、TLV、validator、兼容性
5. 输出 **兼容性看板**，而不是传统“算法排行榜”。

### 2.2 非目标

1. 不在此阶段实现真实压缩算法 benchmark 体系。
2. 不在此阶段引入服务器端数据库或远程注册中心。
3. 不在此阶段改变核心 SPZ header 结构。
4. 不在此阶段为所有未知扩展自动推断 payload 语义。

---

## 3. 设计原则

1. **职责分离**：规范登记与运行时校验解耦。
2. **向后兼容**：现有 `check-spz` / `dump-trailer` 输出语义尽量保持稳定，仅增量扩展字段。
3. **机器可读优先**：新增注册表必须能驱动 CLI、WASM、Web UI，而不只是文档表格。
4. **未知扩展可跳过**：继续遵守 TLV `length` 可跳过原则。
5. **先兼容性，后展示性**：先保证 CLI/WASM/测试闭环，再做 Web 体验层。
6. **门卫不做“评奖系统”**：只做合规成熟度展示，不做算法优劣评判。

---

## 4. 总体架构

### 4.1 当前结构

当前结构可以概括为：

`SPZ 文件 -> InspectSpzBlob -> TLV 记录 -> ExtensionValidatorRegistry -> GateReport`

问题在于：
- `vendor_name` 在 `spz.cc` 内部硬编码；
- `ExtensionReport` 信息太少；
- 无法枚举所有“已登记扩展”；
- Web UI 无法直接展示完整生态视图。

### 4.2 目标结构

升级后结构如下：

`SPZ 文件 -> InspectSpzBlob -> TLV 记录 -> ExtensionSpecRegistry + ExtensionValidatorRegistry -> GateReport/JSON/WASM/Web`

职责分工：

- `ExtensionSpecRegistry`
  - 回答“这是什么扩展”
  - 管理扩展名称、厂商、状态、类别、规范链接、最小版本等元数据
- `ExtensionValidatorRegistry`
  - 回答“如何校验这个扩展 payload”
  - 管理 `type -> validator`
- `InspectSpzBlob`
  - 同时消费两个注册表，产出更完整的 `ExtensionReport`
- `CLI/WASM/Web`
  - 统一基于增强后的 `GateReport` 展示结果

---

## 5. 双层注册表详细设计

## 5.1 `ExtensionSpec`

新增结构 `ExtensionSpec`，建议字段如下：

- `uint32_t type`
- `uint16_t vendor_id`
- `uint16_t extension_id`
- `std::string vendor_name`
- `std::string extension_name`
- `std::string category`
- `std::string status`
- `std::string spec_url`
- `std::string short_description`
- `uint32_t min_spz_version`
- `bool requires_has_extensions_flag`

说明：

- `type` 仍遵循：`type = (vendor_id << 16) | extension_id`
- `category` 建议限制为：
  - `camera`
  - `compression`
  - `metadata`
  - `algorithm`
  - `experimental`
- `status` 建议限制为：
  - `planned`
  - `draft`
  - `stable`
  - `deprecated`

### 5.2 `ExtensionSpecRegistry`

新增单例注册表，接口与现有 validator registry 风格保持一致：

- `Instance()`
- `RegisterSpec(const ExtensionSpec& spec)`
- `GetSpec(uint32_t type)`
- `HasSpec(uint32_t type)`
- `ListSpecs()`
- `SpecCount()`

内部存储：

- `std::map<uint32_t, ExtensionSpec>`
- 使用 `std::mutex` 保持线程安全

### 5.3 自动注册模式

新增与 `RegisterValidator<T>` 类似的辅助机制，例如：

- `RegisterSpecEntry`
- 或基于静态函数/静态对象完成内置 spec 注册

内置 Adobe 扩展需要同时拥有：

1. `ExtensionSpec` 条目
2. `SpzExtensionValidator` 条目

这样才能在任何路径下保持一致。

### 5.4 机器可读登记源

建议新增一个机器可读注册表文件，例如：

- `docs/extension_registry.json`

它作为 **文档与展示源**，不是运行时强依赖配置中心。

运行时初期实现建议仍使用 **C++ 内置静态注册**，理由：

- 简单可靠
- 不引入文件解析依赖
- 对 CLI/WASM/CTest 一致

而 `extension_registry.json` 用于：

- Web registry viewer
- 文档同步
- 未来自动生成注册表页面

### 5.5 为什么不直接只用 JSON

纯 JSON 驱动 registry 虽然对 Web 友好，但当前仓库没有成熟的运行时 JSON 解析与初始化链路。首版先用 **C++ 运行时内置 + JSON 镜像文档** 更稳妥。

---

## 6. `GateReport` 与校验流程增强

## 6.1 `ExtensionReport` 增强字段

在现有基础上增加：

- `bool known_extension`
- `bool has_validator`
- `std::string status`
- `std::string category`
- `std::string spec_url`
- `std::string short_description`

增强后语义：

- `known_extension=false`：未登记扩展
- `known_extension=true && has_validator=false`：已登记但暂未实现 validator
- `known_extension=true && has_validator=true`：已登记且已实现 validator

## 6.2 `InspectSpzBlob(...)` 新流程

对每条 TLV record 的处理改为：

1. 查 `ExtensionSpecRegistry`
2. 查 `ExtensionValidatorRegistry`
3. 构造增强版 `ExtensionReport`
4. 根据组合状态发出不同级别 issue

建议状态矩阵：

### A. 已登记 + 有 validator
- 正常执行 `Validate(...)`
- 校验失败：`L2_EXT_VALIDATION`
- 校验通过：正常记录

### B. 已登记 + 无 validator
- 产出 `ExtensionReport`
- 发出 `warning` 或 `note`：`L2_EXT_REGISTERED_NO_VALIDATOR`
- 不判定 payload 非法，只说明“已知但未实现自动校验”

### C. 未登记 + 有 validator
- 理论上允许，但应视为治理异常
- 发出 `warning`：`L2_EXT_UNREGISTERED_VALIDATOR`
- 说明运行时实现与公开注册表不一致

### D. 未登记 + 无 validator
- 保持当前 unknown 语义
- 发出 `warning`：`L2_EXT_UNKNOWN`

## 6.3 去除 `spz.cc` 中的硬编码 vendor 逻辑

`GetVendorName(uint32_t type)` 仅作为最后兜底，主路径改为：

- 优先从 `ExtensionSpecRegistry` 获取 `vendor_name`
- 查不到时再使用高 16 位生成 `Unknown (0xXXXX)`

---

## 7. CLI 设计

新增 3 个命令：

- `registry`
- `compat-check`
- `gen-fixture`

## 7.1 `registry`

### 目标
列出所有已登记扩展，承担“glTF-style registry viewer 的 CLI 入口”。

### 语法

```bash
spz_gatekeeper registry [--json]
spz_gatekeeper registry list [--json]
spz_gatekeeper registry show <type> [--json]
```

### 输出内容

#### `registry list`
- `type`
- `vendor_name`
- `extension_name`
- `category`
- `status`
- `has_validator`

#### `registry show <type>`
- 上述全部字段
- `spec_url`
- `short_description`
- `min_spz_version`
- `requires_has_extensions_flag`

### 实现建议
首版可以将 `registry` 与 `registry list` 视为同义命令，避免命令树过深。

## 7.2 `compat-check`

### 目标
把“快速自测闭环”产品化。

### 语法

```bash
spz_gatekeeper compat-check <file.spz> [--json]
```

### 检查内容

第一阶段先做仓库内可稳定实现的内容：

1. 调用 `InspectSpzBlob(..., strict=true)`
2. 调用 `InspectSpzBlob(..., strict=false)`
3. 汇总：
   - header 是否通过
   - TLV 是否可解析
   - 是否存在 unknown extension
   - 各扩展 validator 结果

第二阶段再扩展外部工具兼容性：

- 尝试调用 `spz_info`
- 尝试调用 `spz_to_ply`
- 若本机缺失这些工具，则报告 `skipped` 而不是失败

### 输出语义
输出不应只是 pass/fail，而应包含阶段结果：

- `strict_ok`
- `non_strict_ok`
- `registry_summary`
- `extension_summary`
- `upstream_tools`（`pass` / `fail` / `skipped`）

## 7.3 `gen-fixture`

### 目标
把测试中已有的最小 SPZ/TLV 构造能力对外暴露。

### 语法

```bash
spz_gatekeeper gen-fixture --type <u32> --out <file.spz>
spz_gatekeeper gen-fixture --type <u32> --mode valid --out <file.spz>
spz_gatekeeper gen-fixture --type <u32> --mode invalid-size --out <file.spz>
```

### 首版能力
首版不追求复杂 payload 编排，只支持：

- 生成一个最小合法 SPZ
- 根据 `type` 附加一个最小 TLV record
- 为已知内置扩展生成：
  - `valid`
  - `invalid-size`

对于未知类型：
- 默认生成零 payload 或指定长度的占位 payload

### 实现来源
直接复用当前测试中的构造逻辑：

- 最小 SPZ 构造
- TLV 记录写入
- gzip 压缩

避免另写一套与测试不一致的生成器。

---

## 8. 快速自测闭环

## 8.1 用户路径

对扩展作者，建议的最短路径为：

1. 预留 `type`
2. 在 `ExtensionSpecRegistry` 中登记元数据
3. 实现 `SpzExtensionValidator`
4. 通过 `gen-fixture` 生成最小样例
5. 跑 `check-spz --json`
6. 跑 `dump-trailer --json`
7. 跑 `compat-check`

## 8.2 自测最小清单

每个扩展至少应覆盖：

1. **valid payload**：正确通过
2. **invalid size**：错误长度被识别
3. **boundary values**：边界值受控
4. **unknown skip**：其他 viewer 可按 `length` 跳过
5. **strict/non-strict**：两种模式输出符合预期

## 8.3 可选增强

后续可以在 `ExtensionSpec` 中增加“推荐自测模板”字段，供 Web UI 生成说明。

---

## 9. Web / WASM 接线方案

## 9.1 当前问题

`web/index.html` 目前只是静态壳子：

- “WASM 引擎就绪” 由 `setTimeout` 模拟
- 选择文件后只是 `alert`
- 没有真正导入 `spz_gatekeeper.js`

## 9.2 目标

Web 页面应具备以下真实能力：

1. 加载 `spz_gatekeeper.js`
2. 初始化 `createSpzGatekeeperModule()`
3. 用户上传 `.spz` 后读取 `ArrayBuffer`
4. 调用：
   - `inspectSpz(...)`
   - `dumpTrailer(...)`
5. 展示：
   - 是否通过
   - 问题列表
   - TLV 列表
   - 扩展登记状态
   - validator 状态

## 9.3 建议交互结构

页面保留当前极简风格，但新增结果区：

- `Summary`
  - `Pass / Warning / Error`
- `Issues`
  - 展示 issue code、severity、message
- `Extensions`
  - 展示每条 TLV 的 `type`、`vendor_name`、`extension_name`、`status`、`has_validator`
- `Trailer`
  - 展示 `offset`、`length`

## 9.4 WASM 需要新增的导出

在现有 `inspectSpz` / `dumpTrailer` / `inspectSpzText` 外，建议增加：

- `listRegisteredExtensions()`
- `describeExtension(type)`

这样 Web 可在不上传文件时，也展示“当前内置已登记扩展列表”。

## 9.5 Pages 部署要求

保持现有 GitHub Pages 工作流不变，只增加：

- Web 页面引用构建产物
- 必要时拷贝 `extension_registry.json`

不引入新的前端框架。

---

## 10. 兼容性看板，不做传统排行榜

## 10.1 为什么不做排行榜

门卫的职责不是评判压缩率、画质、速度高低，而是：

- 格式合法
- 可跳过
- 可验证
- 可兼容

如果现在做“算法排行榜”，会把不同类型扩展混在一起：

- camera 扩展
- metadata 扩展
- compression 扩展
- algorithm 扩展

它们没有统一的单维度优劣标准。

## 10.2 替代方案：Compatibility Board

建议做 **Compatibility Board / 兼容性看板**，字段如下：

- `type`
- `vendor`
- `name`
- `status`
- `has_spec`
- `has_validator`
- `fixture_valid_pass`
- `fixture_invalid_pass`
- `strict_check_pass`
- `non_strict_check_pass`
- `web_visible`

这反映的是“接入成熟度”，不是“算法强弱”。

## 10.3 Compatibility Score（可选）

若后续确实需要一个分数，只允许做“成熟度分数”，不做性能排名。

建议维度：

- 规范完整度
- validator 完整度
- 自测覆盖度
- 严格模式通过率
- Web/WASM 可视化支持度

---

## 11. 文件改动建议

## 11.1 新增文件

建议新增：

- `cpp/include/spz_gatekeeper/extension_spec_registry.h`
- `cpp/src/extension_spec_registry.cc`
- `docs/extension_registry.json`
- `docs/plans/2026-03-20-spz-extension-registry-and-selftest-design.md`

如需注册表相关测试，可新增：

- `cpp/tests/extension_spec_registry_test.cc`
- `cpp/tests/compat_check_test.cc`
- `cpp/tests/gen_fixture_test.cc`

## 11.2 修改文件

建议修改：

- `cpp/include/spz_gatekeeper/report.h`
- `cpp/src/report.cc`
- `cpp/src/spz.cc`
- `cpp/src/main.cc`
- `cpp/src/wasm_main.cc`
- `cpp/CMakeLists.txt`
- `docs/Vendor_ID_Allocation.md`
- `docs/Implementing_Custom_Extension.md`
- `README.md`
- `README-zh.md`
- `web/index.html`

---

## 12. 实施顺序

按已确认的优先级推进：

### Phase 1：双层注册表
- 引入 `ExtensionSpec`
- 引入 `ExtensionSpecRegistry`
- 接入 Adobe 内置 spec
- 增强 `ExtensionReport`
- 去除主路径上的 vendor 硬编码依赖

### Phase 2：CLI 与自测闭环
- 新增 `registry`
- 新增 `compat-check`
- 新增 `gen-fixture`
- 增加相应测试

### Phase 3：Web 接线
- 真实加载 WASM
- 增加结果展示区
- 增加 registry viewer 能力

### Phase 4：兼容性看板
- 先输出 JSON 看板
- 再视需要映射到网页显示

---

## 13. 测试策略

## 13.1 单元测试

新增覆盖：

- `ExtensionSpecRegistry` 注册、查询、枚举、覆盖、线程安全
- `ExtensionReport` JSON/Text 序列化新增字段

## 13.2 集成测试

覆盖场景：

1. 已登记 + 有 validator
2. 已登记 + 无 validator
3. 未登记 + 有 validator
4. 未登记 + 无 validator
5. strict / non-strict 模式结果差异

## 13.3 Web/WASM 测试

至少验证：

- 模块初始化成功
- 上传有效 `.spz` 时页面能显示 pass
- 上传带 unknown extension 的 `.spz` 时页面能显示 warning
- `dumpTrailer` 结果可见

---

## 14. 风险与控制

### 风险 1：报告格式变更影响现有使用方
控制：
- JSON 采用增量字段，不删除原字段
- 文本输出保持现有主体格式

### 风险 2：注册表与 validator 不一致
控制：
- 增加测试验证“内置 spec / 内置 validator”一致性
- 对“未登记但有 validator”发 warning

### 风险 3：Web 接线后页面复杂度膨胀
控制：
- 保持单页原生 JS
- 结果区采用极简展开布局
- 不引入框架

### 风险 4：compat-check 依赖外部工具不稳定
控制：
- 外部工具检查作为第二阶段
- 缺失工具统一标记 `skipped`

---

## 15. 验收标准

满足以下条件视为第一轮落地完成：

1. 运行 `spz_gatekeeper registry --json` 可列出已登记扩展。
2. `check-spz` 的 `extension_reports` 含登记状态、类别、规范链接等增量字段。
3. `compat-check` 能对任意 `.spz` 输出结构化兼容性结果。
4. `gen-fixture` 能生成至少一种有效 fixture 与一种无效 fixture。
5. Web 页面能真实调用 WASM，并展示 issues / TLV / 扩展状态。
6. 文档同步更新，说明扩展登记与自测流程。

---

## 16. 最终建议

本项目后续应明确定位为：

- **SPZ 扩展登记中心**
- **SPZ L2 合法性门卫**
- **SPZ 衍生算法快速自测入口**
- **兼容性成熟度展示面板**

而不是传统意义上的算法排行榜站点。

这样既符合当前代码结构，也与 glTF 式“扩展治理 + 查看器支持”的生态路径一致。