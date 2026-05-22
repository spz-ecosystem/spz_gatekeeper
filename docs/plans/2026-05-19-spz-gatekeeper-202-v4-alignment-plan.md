# SPZ Gatekeeper 2.0.2 — SPZ v4 格式完整对齐计划

> 更新日期: 2026-05-21\
> 状态: 设计审查通过（6层交叉审查），待实施。实施前预清理已完成（2026-05-21）\
> 基于: [SPZ v4 深度对比](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/docs/plans/research_report_spz_v4_vs_gatekeeper_deep_comparison.md) + [202对齐研究](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/docs/plans/research_report_spz_gatekeeper_202_alignment.md) + [实测报告](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/docs/plans/spz_v4_validation_final_report.md) + \[L2 代码对标审查] + \[GitNexus 三项目基线] + \[Git 历史审查]

> **对照基线**: SPZ v3.0.0 (Niantic latest `C:\Users\HP\Downloads\v3.0.0\spz-3.0.0`, GitNexus 767节点/1494边/23集群/32流)\
> **目标项目**: Gatekeeper v2.0.0→v2.0.2 (`C:\Users\HP\Downloads\HunYuan3D_test_cases\spz_gatekeeper_project`, GitNexus 1368节点/2958边/53集群/95流)

***

## 零、依赖锁定与供应链安全

### 0.1 全平台依赖版本锁定

三平台（Windows/macOS/Linux）均锁定精确版本。Emscripten WASM 构建走 Emscripten ports 或 FetchContent 静态编译，不依赖宿主系统包管理器。

| 依赖             | 锁定版本         | SHA256                                                             | 平台                  | 提供方式                                           |
| :------------- | :----------- | :----------------------------------------------------------------- | :------------------ | :--------------------------------------------- |
| **zlib**       | **1.3.1**    | `9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23` | Windows/macOS/Linux | `find_package(ZLIB 1.3.1 REQUIRED)`            |
| <br />         | <br />       | <br />                                                             | Emscripten          | `-sUSE_ZLIB=1` (Emscripten port, 版本由 emsdk 管理) |
| **zstd**       | **1.5.6**    | `8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1` | 全平台 + Emscripten    | `find_package` 或 `FetchContent` 回退，均校验 SHA256  |
| **CMake**      | ≥ **3.16**   | —                                                                  | 全平台                 | 构建系统，非运行时依赖                                    |
| **Emscripten** | ≥ **3.1.74** | —                                                                  | WASM 构建             | `emcmake` / `emmake`，`-sUSE_ZLIB=1` 兼容         |

> **锁定策略**: FetchContent 路径通过 `URL_HASH SHA256=<hash>` 参数校验。`find_package` 路径通过精确版本号匹配（`find_package(ZLIB 1.3.1 REQUIRED)`）。Emscripten 系统依赖由 emsdk 版本管理，项目不直接锁定。

### 0.2 zlib 版本锁定 (现有依赖修正)

当前 [CMakeLists.txt:42-49](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/CMakeLists.txt#L42-L49) 中 zlib **未锁定版本**：

```cmake
# 当前 (问题)
find_package(ZLIB REQUIRED)          # 接受任意版本

# 修正 (R1 同步)
find_package(ZLIB 1.3.1 REQUIRED)    # 精确版本匹配
```

### 0.3 供应链投毒防御

供应链投毒（如 Mini Shai-Hulud 2026-05-11 攻击：48小时内 172 包 × 404 恶意版本）在当前阶段无法根除。防御策略是**隔离 + 检查**：

**铁律**：

1. **测试和 WASM 构建仅在云端 CI 执行** — GitHub Actions `ubuntu-latest` / `macos-latest` / `windows-latest` runner，不依赖本地环境
2. **每次推送云端测试前，先查询供应链投毒事件** — 检查 npm/PyPI/CVE 数据库是否有新的恶意包爆发。发现活跃投毒事件 → 暂停推送 → 等待社区清理后再继续
3. **依赖锁定** — 上文 0.1 节所有版本号 + SHA256 固定，FetchContent 不会自动拉取新版本

**预推送检查清单** (每次 `git push` 前)：

```powershell
# 1. 检查依赖版本是否有已知漏洞
#    → https://nvd.nist.gov/ 搜索 zlib/zstd
# 2. 检查 npm/PyPI 是否有活跃投毒事件
#    → https://github.com/ossf/malicious-packages 查看最近 48h
# 3. 确认 CI 上次构建的依赖版本 SHA256 未变
```

### 0.4 令牌状态

> ⚠️ **当前状态**: Gatekeeper 仓库的 GitHub 令牌已轮换。新令牌尚未提供。在令牌到位前，所有代码改动仅限本地工作区，**不推送**。推送操作需在完整 PowerShell 终端中使用 `git-workflow push` 技能，且需要有效的 `GITHUB_TOKEN`。

### 0.5 实施前置条件与提交纪律

#### 0.5.1 仓库预清理（2026-05-21 已执行）

实施开始前已完成：

| 操作 | 内容 |
|:--|:--|
| 分支清理 | 删除已废弃的 `feature-spz-gatekeeper-2.1-batch1` (2.1 合约已冻结到 main，超出 2.0.2 scope) |
| Worktree 清理 | 移除对应的 WSL 隔离 worktree |
| Stash 清理 | `git stash clear` 清空 5 个悬垂 2.1 开发临时快照 |
| 备份标签 | `backup/feature-spz-gatekeeper-2.1-batch1-20260521` 保留提交历史可恢复 |
| 构建产物 | 待删：9 个 `build*` 目录 (~40 MB WSL CMake 输出)，`cmake -B` 随时可重建 |

#### 0.5.2 提交拆分铁律

**禁止巨型炸弹提交**。Git 历史审查（2026-05-21）发现门卫历史上均为巨型提交（根提交 5124 行一次性导入、核心功能 26 文件混交）。2.0.2 必须严格增量提交：

| 规则 | 说明 |
|:--|:--|
| R 级拆分 | 每个 R 阶段（R1-R6）至少一个独立 commit |
| 单主题 | 一个 commit 不混入两个不同 R 阶段的改动 |
| 先验证再提交 | 每 commit 前跑 CTest 确认不破坏现有测试 |
| 提交消息格式 | `feat(R#): <描述>` 或 `refactor(R#): <描述>`，含 `gitnexus impact: <files> <risk>` |

#### 0.5.3 gitignore 专项说明

`.gitignore` L49 `docs/plans/` 排除了所有计划文档。本计划文件如需纳入 git 跟踪，使用：

```bash
git add -f docs/plans/2026-05-19-spz-gatekeeper-202-v4-alignment-plan.md
```

#### 0.5.4 Shell 命令安全约束

见 [`.trae/rules/project_rules.md`](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/.trae/rules/project_rules.md) §Shell 命令安全：
- 禁止超大命令（>5 条 `&&` 链），拆为独立简短命令逐条执行
- 变量替换禁止嵌入 shell 命令（`$(date ...)` 等）
- 操作与验证命令分离

***

## 一、当前状态

### 1.1 Gatekeeper v2.0.1 现状

| 维度    |          状态         | 说明                                                                                                                                    |
| ----- | :-----------------: | ------------------------------------------------------------------------------------------------------------------------------------- |
| 版本    |        v2.0.1       | CMakeLists.txt VERSION 2.0.0, CHANGELOG 2026-03-26                                                                                    |
| v4 支持 |       ❌ 完全不支持       | 无法解析 SPZ v4 格式文件                                                                                                                      |
| 压缩    |    仅 gzip (zlib)    | `DecompressGzip()` [spz.cc:77-116](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/spz.cc#L77-L116) |
| 头部    |  16 字节 `SpzHeader`  | [spz.cc:67-75](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/spz.cc#L67-L75)                      |
| 扩展位置  | trailer (payload 后) | `ParseTlvTrailer()` [tlv.cc:15-54](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/tlv.cc#L15-L54)  |
| 术语    |         TLV         | 官方已改为 ILV                                                                                                                             |
| 验证    |  L2 deep validation | 结构化 `GateReport` + extension validator registry                                                                                       |

### 1.2 SPZ v4 格式关键差异

```
SPZ v4 文件布局 (Niantic NgspFileHeader 32B):
┌──────────────────────────────────────────────────────────────┐
│ Bytes 0–31:     NgspFileHeader (32B, plaintext)               │
│   0x5053474e magic · version=4 · numPoints · shDegree         │
│   fractionalBits · flags · numStreams · tocByteOffset         │
│   reserved[12]                                                │
├──────────────────────────────────────────────────────────────┤
│ Bytes 32..tbo-1: Extension ILV records (if flags & 0x2)       │
│   每个 record: [u32 type][u32 byteLength][payload]            │
├──────────────────────────────────────────────────────────────┤
│ Bytes tbo..tbo+N*16-1: TOC (Table of Contents)                │
│   每个 entry: [u64 compressedSize][u64 uncompressedSize]      │
├──────────────────────────────────────────────────────────────┤
│ Bytes tbo+N*16..end: ZSTD-compressed attribute streams        │
│   6 streams: positions → alphas → colors → scales →           │
│              rotations → sh                                   │
└──────────────────────────────────────────────────────────────┘
```

| 差异项             | SPZ v4                | Gatekeeper v2.0.1   | 严重度 |
| :-------------- | :-------------------- | :------------------ | :-: |
| 压缩算法            | ZSTD multi-stream     | gzip single-stream  |  🔴 |
| 头部大小            | 32 字节                 | 16 字节               |  🔴 |
| `numStreams`    | ✅                     | ❌                   |  🔴 |
| `tocByteOffset` | ✅                     | ❌                   |  🔴 |
| `reserved[12]`  | 12 字节全零               | 1 字节                |  🔴 |
| 扩展位置            | header zone (32..tbo) | trailer (payload 后) |  🔴 |
| 术语              | ILV                   | TLV                 |  🔴 |

### 1.3 三级对齐审计：已对齐 / 偏离 / 向前兼容

> 标签定义:
>
> - 🟢 **已对齐**: 实现与 SPZ v3.0.0 规范字节级一致，无需任何改动
> - 🔴 **偏离**: 与规范不一致，会被 v4 文件解析失败或行为错误，**必须修复**（对应 R1-R5）
> - 🔵 **向前兼容**: 门卫独有且不与规范冲突的能力——或是 SPZ 尚未定义但门卫已预埋的治理基础设施（未来捐赠开放原子基金会的核心资产）

#### 1.3.1 文件格式层

| 项                           |  对齐状态  | SPZ 规范               | Gatekeeper v2.0.1              | 修复     |
| :-------------------------- | :----: | :------------------- | :----------------------------- | :----- |
| NGSP magic `0x5053474e`     | 🟢 已对齐 | 必须                   | ✅ `spz.cc L238` 已检查            | —      |
| `hasExtensions` flag `0x02` | 🟢 已对齐 | 位标志 bit 1            | ✅ `kFlagHasExtensions = 0x02`  | —      |
| `antialiased` flag `0x01`   |  🔴 偏离 | 位标志 bit 0            | ❌ 未定义                          | R2 T08 |
| 头部大小                        |  🔴 偏离 | 32B `NgspFileHeader` | 16B `SpzHeader`                | R2 T05 |
| `numStreams` 字段             |  🔴 偏离 | header 偏移 10         | ❌ 不存在                          | R2 T05 |
| `tocByteOffset` 字段          |  🔴 偏离 | header 偏移 12-15      | ❌ 不存在                          | R2 T05 |
| `reserved[12]` 全零校验         |  🔴 偏离 | 12 字节必须为 0           | 仅验 1 字节                        | R2 T06 |
| `fractionalBits` 存储         |  🔴 偏离 | header 偏移 9          | `SpzHeader` 有但 `SpzL2Info` 未暴露 | R2 T07 |

#### 1.3.2 压缩与文件结构层

| 项                   |  对齐状态 | SPZ 规范                            | Gatekeeper v2.0.0                  | 修复           |
| :------------------ | :---: | :-------------------------------- | :--------------------------------- | :----------- |
| 压缩算法                | 🔴 偏离 | ZSTD 多流                           | gzip 单流                            | R1 T01-T03   |
| ZSTD 压缩（fixture 生成） | 🔴 偏离 | 需要多流 ZSTD 写入                      | 仅 `GzipCompress`                   | R1 T01b-T03b |
| TOC 解析              | 🔴 偏离 | `numStreams×16B` 表                | ❌ 不存在                              | R1 T01       |
| 版本检测                | 🔴 偏离 | NGSP magic→v4, 0x1f 0x8b→legacy   | 硬编码 `DecompressGzip`               | R4 T12       |
| 扩展存储位置              | 🔴 偏离 | header zone `[32, tocByteOffset)` | trailer (payload 后)                | R3 T09       |
| zlib 版本锁定           | 🔴 偏离 | —                                 | `find_package(ZLIB REQUIRED)` 任意版本 | R1 T00       |

#### 1.3.3 扩展机制层

| 项                             |   对齐状态  | SPZ 规范                                      | Gatekeeper v2.0.1                                                                     | 修复                |
| :---------------------------- | :-----: | :------------------------------------------ | :------------------------------------------------------------------------------------ | :---------------- |
| ILV 字节格式                      |  🟢 已对齐 | `[u32 type][u32 byteLength][payload]`       | ✅ `tlv.cc` (`ReadU32LE` + 8B header)                                                  | —                 |
| Type ID 分配                    |  🟢 已对齐 | `(vendor_id<<16) \| ext_id`                 | ✅ `spz.cc L31-L32`                                                                    | —                 |
| 未知扩展跳过                        |  🟢 已对齐 | skip `byteLength` + 警告                      | ✅ 同                                                                                   | —                 |
| Adobe payload 格式              |  🟢 已对齐 | 12B 3×float32 LE                            | ✅ `safe_orbit_camera_validator.h`                                                     | —                 |
| 术语 ILV                        |  🔴 偏离  | 官方使用 ILV                                    | TLV                                                                                   | R5 T15-T22        |
| `SPZ_ADOBE_safe_orbit_camera` |  🟢 已对齐 | `0xADBE0002`                                | ✅ 已登记+验证                                                                              | —                 |
| `SPZ_ADOBE_coordinate_system` |  🔴 偏离  | `0xADBE0003` (PR#82 已合并 main)               | ❌ **未登记**                                                                             | 待登记               |
| 抽象基类 `SpzExtensionValidator`  | 🔵 向前兼容 | 官方 `SpzExtensionBase` 8虚方法（含write/read/PLY） | 门卫 3虚方法（Validate/GetName/GetType）                                                     | 门卫是校验器不存储不写入，职责不同 |
| 注册方式                          | 🔵 向前兼容 | `tryParseExtension` switch-case             | `RegisterValidator<T>` 模板 + Meyer's Singleton                                         | 门卫自动注册更工程化，不冲突    |
| 双层注册表                         | 🔵 向前兼容 | 无（仅枚举+注释）                                   | `ExtensionSpecRegistry` + `ExtensionValidatorRegistry`                                | 门卫独有的治理元数据层       |
| 11字段 `ExtensionSpec`          | 🔵 向前兼容 | 无                                           | vendor\_name / category / status / spec\_url / short\_description / min\_spz\_version | 门卫独有的治理元数据        |
| PLY 支持                        | 🔵 向前兼容 | `getPlyExtensionRegistry()`                 | 无                                                                                     | 门卫 SPZ-only，非格局冲突 |

#### 1.3.4 验证与治理层

| 项                     |   对齐状态  | SPZ 规范              | Gatekeeper v2.0.0                                       | 修复       |
| :-------------------- | :-----: | :------------------ | :------------------------------------------------------ | :------- |
| 4 状态矩阵 A/B/C/D        | 🔵 向前兼容 | 无 — 仅 parse-or-skip | 登记×验证=4种组合                                              | 门卫独有治理模型 |
| strict/non-strict 双模式 | 🔵 向前兼容 | 无                   | ✅ `SpzInspectOptions.strict`                            | 门卫独有     |
| Adobe validator 值域验证  | 🔵 向前兼容 | `read()` 无值域检查      | NaN/范围/逻辑一致性 5层                                         | 门卫补强，不冲突 |
| `GateReport` 结构化输出    | 🔵 向前兼容 | 纯文本 `SpzLog`        | JSON + issue code + severity                            | 门卫独有     |
| 兼容性看板                 | 🔵 向前兼容 | 无                   | `getCompatibilityBoard()` 10字段                          | 门卫独有     |
| 自测闭环                  | 🔵 向前兼容 | 无                   | `gen-fixture`→`check-spz`→`compat-check`→`compat-board` | 门卫独有     |

#### 1.3.5 CLI / WASM / 内部层

| 项               |   对齐状态  | SPZ 规范                           | Gatekeeper v2.0.0                                      | 修复            |
| :-------------- | :-----: | :------------------------------- | :----------------------------------------------------- | :------------ |
| CLI registry 命令 | 🔵 向前兼容 | 无                                | `registry list` / `registry show`                      | 门卫独有          |
| WASM exports    | 🔵 向前兼容 | 官方 `GaussianCloud.extensions` 数组 | `listRegisteredExtensions()` / `describeExtension()`   | 治理 vs 数据，层次不同 |
| WASM 生产标志       | 🔵 向前兼容 | —                                | 新增 `-sASSERTIONS=0 -sNO_EXIT_RUNTIME=1`（R6 T25）        | 门卫优化          |
| 依赖版本锁定          | 🔵 向前兼容 | CMake FetchContent 裸 URL         | zlib/zstd 锁定版本+SHA256（R1 T00+T04）                      | 门卫供应链安全       |
| 版权头             |  🔴 偏离  | 内部一致性                            | 8 个 `PuJunhan` + 8 个缺失 → `SPZ Gatekeeper Contributors` | R5 T22b       |

#### 1.3.6 汇总

| 类别      |  数量 | 说明                                       |
| :------ | :-: | :--------------------------------------- |
| 🟢 已对齐  |  8  | 无需改动，字节级一致                               |
| 🔴 偏离   |  14 | 全部在 R1-R5 覆盖（+待登记 coordinate\_system 扩展） |
| 🔵 向前兼容 |  14 | 门卫核心竞争力，不降级 |

**核心结论**: 14 项偏离均为事实性偏差——不修行为就错，全部对应计划的 R1-R5 具体任务。14 项向前兼容项为门卫作为开放原子基金会中立基础设施的差异化能力，不降级。

#### 1.3.7 关键项审计说明

**ILV 字节格式**（已对齐，无需改动）:
- SPZ 官方字节格式: `[u32 type][u32 byteLength][payload]`
- 门卫实现: `ReadU32LE(type)` + `ReadU32LE(length)` + N 字节 payload（[tlv.cc L8-L13](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/tlv.cc#L8-L13)）
- 两者字节级别完全一致。SPZ 仅将缩写从 TLV 改为 ILV——未改变字节格式。R5 重命名是术语对齐，不是格式修正

**SPZ main 分支扩展确认**（本地 v3.0.0 副本滞后）:
- main 分支 [splat-extensions.h L51-L54](https://github.com/nianticlabs/spz/blob/main/extensions/cc/splat-extensions.h) 已有两个扩展: `0xADBE0002` (safe_orbit_camera) + `0xADBE0003` (coordinate_system, PR#82 5月13日合并)
- `SPZ_ADOBE_coordinate_system` payload = `sizeof(uint32_t)` = 4 字节（[coordinate-system-adobe.cc L47](file:///C:/Users/HP/Downloads/spz-main5.21/spz-main/extensions/cc/coordinate-system-adobe.cc#L47)），仅存储一个 `CoordinateSystem` 枚举值
- 门卫当前未登记，需在 R5 同期待登记（见 T22c）

**`RegisterValidator<T>` 模板**（向前兼容，无须改动）:
- [validator_registry.h L162-L175](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/include/spz_gatekeeper/validator_registry.h#L162-L175) 的构造器硬依赖 `T` 的默认构造函数 + `GetExtensionType()` 方法
- 当前 `AdobeSafeOrbitCameraValidator` 两者都满足。未来新增 validator 必须遵守此约束（带参构造不可用），这是设计约束而非缺陷

**4 状态矩阵**（向前兼容，已完整实现）:
- [spz.cc L327-L337](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/spz.cc#L327-L337) 的 if-else 链完整覆盖 `{已登记,未登记} × {有validator,无validator}` = 4 种排列:
  - A: `spec && validator` → 无问题码（Validate 失败才报 `L2_EXT_VALIDATION`）
  - B: `spec && !validator` → `L2_EXT_REGISTERED_NO_VALIDATOR`
  - C: `!spec && validator` → `L2_EXT_UNREGISTERED_VALIDATOR`
  - D: `!spec && !validator` → `L2_EXT_UNKNOWN`
- 逻辑自洽，无遗漏组合

#### 1.3.8 `SPZ_ADOBE_coordinate_system` 登记 (T22c)

**文件**: [spz.cc L28-L45](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/spz.cc#L28-L45) `RegisterBuiltInSpecs()`

**新增登记条目**:
```cpp
ExtensionSpec coord_spec;
coord_spec.type = 0xADBE0003u;
coord_spec.vendor_id = static_cast<std::uint16_t>(0xADBE0003u >> 16);
coord_spec.extension_id = static_cast<std::uint16_t>(0xADBE0003u & 0xFFFFu);
coord_spec.vendor_name = "Adobe";
coord_spec.extension_name = "Adobe Coordinate System";
coord_spec.category = "metadata";
coord_spec.status = "draft";          // stable for 0xADBE0002, draft for 0xADBE0003
coord_spec.spec_url = "extensions/cc/coordinate-system-adobe.h";
coord_spec.short_description = "Records the coordinate system in which Gaussian data is physically stored.";
coord_spec.min_spz_version = 4;
coord_spec.requires_has_extensions_flag = true;
ExtensionSpecRegistry::Instance().RegisterSpec(coord_spec);
```

**validator 暂缺**: payload 仅 4 字节，验证逻辑简单（检查值在 `CoordinateSystem` 枚举范围内），但需等待 SPZ 文档完全定稿后再实现 validator。

**变更量**: spz.cc +12 行

### 1.4 代码结构分析（GitNexus 图谱）

#### 1.4.1 文件规模（严重）

| 文件 | 行数 | 问题 |
|:--|:---:|:--|
| `main.cc` | **1634** | 🔴 8 个 CLI 子命令全部混在一个文件 |
| `audit_summary.cc` | 827 | 🟡 双端协同 JSON builder 集中 |
| `wasm_main.cc` | 496 | 🟡 WASM exports + fixture builder 共存 |
| `spz.cc` | 367 | 🔴 2.0.2 后 +390 → **~757 行**（膨胀 206%） |

**对比基准**: SPZ v3.0.0 `load-spz.cc` 1441 行（但涵盖 load+save+pack+unpack 全部功能）。门卫仅验证功能的单文件即将接近 SPZ 官方的全功能库。

#### 1.4.2 耦合密度（GitNexus 指标）

| 指标 | 值 | 解读 |
|:--|:--|:--|
| 节点数 | 1368 | C++ 符号 + JS/web 实体 |
| 边数 | **2958** | 边/节点比 = 2.16 — 极高耦合 |
| 集群数 | 53 | Leiden 社区检测密度高 |
| 执行流 | 95 | 调用路径密集交叉 |

**核心瓶颈**:
1. `InspectSpzBlob` 是单一 choke point — 直接调用 `DecompressGzip`、`ParseHeader`、`ComputeBasePayloadSize`、`ParseTlvTrailer`、`ExtensionSpecRegistry`、`ExtensionValidatorRegistry`，所有验证逻辑收敛于此
2. `main.cc` CLI router 混入 JSON builder 逻辑 + handoff 解析 + fixture 生成 — 违反单一职责
3. `wasm_main.cc` 的 `GzipCompress`/`BuildFixtureBlob` 与 `main.cc` 中同名函数代码重复

#### 1.4.3 循环依赖

- `spz.cc` → `report.h` `GateReport` → `spz.h` `InspectSpzBlob` → `spz.cc`
- `extension_validator.h` → `validator_registry.h` → `extension_validator.h`（通过 `RegisterValidator<T>` 模板）
- `audit_summary.h` → `json_min.h` → `audit_summary.h`

#### 1.4.4 技术债账户

| 问题 | 严重度 | 应在何时处理 |
|:--|:--|:--|
| `main.cc` 1634 行超大文件 | 🟠 中 | 2.5 稳定化冲刺拆分为 command_handlers/ |
| `spz.cc` 2.0.2 后膨胀到 ~757 行 | 🟠 中 | 2.0.2 先接受膨胀，2.5 拆分为 decompress/ + header/ + extension/ |
| `InspectSpzBlob` 单点 choke | 🟠 中 | R4 拆分 `InspectSpzBlobV4`/`InspectSpzBlobLegacy` 部分缓解 |
| wasm_main.cc 与 main.cc 代码重复 | 🟡 低 | 2.5 提取共用 fixture builder 到 `test_fixtures.h` |
| 循环依赖（spz.cc↔report.h↔spz.h） | 🟡 低 | 2.5 引入 `forward_declare.h` 前向声明 |

**2.0.2 策略**: 不在此版本做结构重构。接受 `spz.cc` 膨胀到 ~757 行的事实，新增函数保持逻辑内聚在匿名命名空间中。结构性拆分留给 2.5 技术债清理冲刺。

---

## 二、任务全景

```
R1: ZSTD 解压+压缩 + TOC + 多流  ──►  R2: 32B 头解析 + SpzL2Info  ──►  R3: header zone 扩展定位
         │                                      │                             │
         ▼                                      ▼                             ▼
R4: InspectSpzBlob() 架构重构 (版本检测 + 双路径整合)
         │
         ▼
R5: 术语统一 TLV→ILV (C++内部 + CMake + 测试)
         │
         ▼
R6: 回归测试 + WASM 构建验证 + 版本 bump
```

***

## 三、R1 — ZSTD 集成与多流解压/压缩 (预估 1-2 天)

### 3.0 范围说明

门卫的定位是校验器（validator），但**自测闭环是门卫的硬需求**（[README.md L343-L349](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/README.md#L343-L349)）：`gen-fixture` → `check-spz` 循环。v4 对齐后，fixture 生成必须同时支持 gzip (v1-v3) 和 ZSTD (v4+)。因此 R1 包含**双方向**：

- **解压方向**（消费 v4 文件）：T01-T03
- **压缩方向**（生成 v4 fixture）：T01b-T03b

**平台策略**: CLI 和 WASM 在各自独立的 CMake 构建目录中被分别编译。同一份源码 `spz.cc` 在两个目标下独立编译——原生 `std::async` 多流并行 + 硬件回退串行，WASM 编译期串行。对标 SPZ 上游 `#if defined(__EMSCRIPTEN__)` 模式。

### 3.1 CMakeLists.txt 改造

**文件**: [CMakeLists.txt](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/CMakeLists.txt)

**参照**: [Niantic CMakeLists.txt L52-73](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/CMakeLists.txt#L52-L73)

**方案**: 在现有 `spz_gatekeeper_zlib` INTERFACE 库之后新增 `spz_gatekeeper_zstd`。依赖版本**锁定在 v1.5.6**（与 Niantic v3.0.0 一致，SHA256 校验）：

```cmake
# === zstd v1.5.6 (locked, SHA256 verified) ===
set(SPZ_GATEKEEPER_ZSTD_VERSION "1.5.6")
set(SPZ_GATEKEEPER_ZSTD_SHA256
  "8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1")
set(SPZ_GATEKEEPER_ZSTD_URL
  "https://github.com/facebook/zstd/releases/download/v${SPZ_GATEKEEPER_ZSTD_VERSION}/zstd-${SPZ_GATEKEEPER_ZSTD_VERSION}.tar.gz")

add_library(spz_gatekeeper_zstd INTERFACE)
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  # Emscripten: FetchContent 编译 zstd 静态库 (纯 C99, emcc 兼容)
  include(FetchContent)
  FetchContent_Declare(zstd URL ${SPZ_GATEKEEPER_ZSTD_URL}
    URL_HASH SHA256=${SPZ_GATEKEEPER_ZSTD_SHA256})
  set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(zstd)
  target_link_libraries(spz_gatekeeper_zstd INTERFACE libzstd_static)
else()
  find_package(zstd ${SPZ_GATEKEEPER_ZSTD_VERSION} QUIET)
  if(zstd_FOUND)
    target_link_libraries(spz_gatekeeper_zstd INTERFACE zstd::libzstd_static)
  else()
    include(FetchContent)
    FetchContent_Declare(zstd URL ${SPZ_GATEKEEPER_ZSTD_URL}
      URL_HASH SHA256=${SPZ_GATEKEEPER_ZSTD_SHA256})
    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(zstd)
    target_link_libraries(spz_gatekeeper_zstd INTERFACE libzstd_static)
  endif()
endif()

target_link_libraries(spz_gatekeeper_core PUBLIC spz_gatekeeper_zstd)
```

**变更量**: CMakeLists.txt +25 行

### 3.2 ZSTD 解压函数族

**文件**: [spz.cc](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/spz.cc)

**新增 3 个函数**:

#### 3.2.1 `ParseToc(const uint8_t* data, size_t size, const SpzHeaderV4& header)` (T01)

```cpp
struct TocEntry {
  uint64_t compressed_size;
  uint64_t uncompressed_size;
  size_t compressed_offset;
};

struct TocParseResult {
  bool ok = false;
  std::string error;
  std::vector<TocEntry> entries;
};

TocParseResult ParseToc(const uint8_t* data, size_t size, const SpzHeaderV4& header);
```

**参照**: [load-spz.cc L662-734](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L662-L734)

**验证逻辑**:

1. `header.numStreams == 0` → 报错 `L2_NUM_STREAMS_ZERO`
2. `header.numStreams > 6` → 警告 `L2_NUM_STREAMS_EXCEEDS_EXPECTED`（SPZ v4 规范当前定义≤6 流，但 `numStreams` 字段允许未来扩展。不拦截，仅记录）
3. `header.tocByteOffset < 32` → 报错 `L2_TOC_OFFSET`
4. `tocEnd (= tocByteOffset + numStreams * 16) > size` → 报错 `L2_TOC_TRUNCATED`
5. 遍历每个 stream entry: 读 `compressedSize`/`uncompressedSize`，累加 `compressedOffset`
6. `compressedOffset != size` → 报错 `L2_TOC_SIZE_MISMATCH`

**关于流数**: Niantic 源码 [load-spz.cc L802-L805](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L802-L805) 的动态逻辑表明流数取决于非空属性——`shDegree=0` 只有 5 流。门卫读取 header 的 `numStreams` 字段，有多少解多少，不硬编码 6。顺序固定: positions → alphas → colors → scales → rotations → sh。

**变更量**: spz.cc +40 行

#### 3.2.2 `DecompressZstdStream(const uint8_t* src, size_t src_size, std::vector<uint8_t>* out)` (T02)

```cpp
bool DecompressZstdStream(const uint8_t* src, size_t src_size,
                          std::vector<uint8_t>* out, std::string* err);
```

**参照**: Niantic `ZSTD_decompress()` [load-spz.cc L706-712](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L706-L712)

**实现**: `#include <zstd.h>`，调用 `ZSTD_decompress(out->data(), out->size(), src, src_size)`，检查 `ZSTD_isError(ret)`。

**变更量**: spz.cc +20 行

#### 3.2.3 `DecompressNgspStreams(...)` (T03)

```cpp
bool DecompressNgspStreams(const uint8_t* data, size_t size,
                           const SpzHeaderV4& header,
                           const TocParseResult& toc,
                           std::vector<uint8_t>* decomp, std::string* err);
```

**参照**: Niantic `decompressNgspStreams()` [load-spz.cc L700-L735](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L700-L735)

**设计**: 编译期分叉 + 运行时硬件回退：

```cpp
#include <future>   // 新增 — 核心库仅此一处线程依赖

bool DecompressNgspStreams(...) {
  // 预分配 decomp buffer
  const size_t total = sum of toc.entries[].uncompressed_size;
  decomp->reserve(total);

#if defined(__EMSCRIPTEN__)
  // === WASM: 编译期串行 ===
  for (auto& entry : toc.entries) {
    std::vector<uint8_t> buf(entry.uncompressed_size);
    if (!DecompressZstdStream(data + entry.compressed_offset,
                               entry.compressed_size, &buf, err))
      return false;
    decomp->insert(decomp->end(), buf.begin(), buf.end());
  }
#else
  // === 原生 CLI: 并行 + 优雅降级 ===
  const bool try_parallel =
      std::thread::hardware_concurrency() >= 2 && toc.entries.size() > 1;

  if (try_parallel) {
    std::vector<std::future<std::vector<uint8_t>>> futures;
    try {
      for (auto& entry : toc.entries) {
        futures.push_back(std::async(std::launch::async,
          [&]() -> std::vector<uint8_t> {
            std::vector<uint8_t> buf(entry.uncompressed_size);
            DecompressZstdStream(data + entry.compressed_offset,
                                  entry.compressed_size, &buf, nullptr);
            return buf;
          }));
      }
      for (auto& f : futures) {
        auto result = f.get();
        if (result.empty()) return false;
        decomp->insert(decomp->end(), result.begin(), result.end());
      }
    } catch (const std::system_error&) {
      // std::async 资源不足 → 回退串行
      goto fallback_serial;
    }
  } else {
fallback_serial:
    for (auto& entry : toc.entries) { /* same serial loop */ }
  }
#endif
  return true;
}
```

**优雅降级触发条件**:
1. **硬件不足** → `std::thread::hardware_concurrency() < 2`（单核 CPU / 受限容器）
2. **系统资源耗尽** → `std::async(std::launch::async)` 抛 `std::system_error`
3. **单流文件** → `toc.entries.size() <= 1`（并行无收益）
4. **WASM** → `#if defined(__EMSCRIPTEN__)` 编译期直接走串行

**变更量**: spz.cc +55 行（含 `#include <future>` + 双路径逻辑）

#### 3.2.4 `CompressZstdStream(const uint8_t* src, size_t src_size, std::vector<uint8_t>* out)` (T01b)

```cpp
bool CompressZstdStream(const uint8_t* src, size_t src_size,
                        std::vector<uint8_t>* out, std::string* err);
```

**参照**: Niantic `compressZstd()` [load-spz.cc L245-L260](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L245-L260)

**用途**: `gen-fixture` 生成 v4 ZSTD 压缩的 fixture blob；`BuildFixtureBlob` 在 WASM 端生成 compat-board 测试数据。

**实现**: 使用 `ZSTD_compressBound()` + `ZSTD_compress2()`，compressionLevel=12（与 Niantic 默认一致）。

**变更量**: spz.cc +20 行

#### 3.2.5 `CompressNgspStreams(...)` (T02b)

```cpp
bool CompressNgspStreams(const std::vector<std::pair<const uint8_t*, size_t>>& srcs,
                         std::vector<std::vector<uint8_t>>* chunks,
                         std::vector<uint64_t>* uncompressed_sizes,
                         std::string* err);
```

**参照**: Niantic `compressNgspStreams()` [load-spz.cc L737-L775](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L737-L775)

**用途**: 生成 v4 .spz 文件时，将多个数据流（positions/alphas/colors/scales/rotations/sh）分别 ZSTD 压缩，产出 chunks 供 TOC 写入。

**实现**: 与 `DecompressNgspStreams` 对称——编译期分叉 `#if defined(__EMSCRIPTEN__)`，原生端并行 + `hardware_concurrency()` 检测 + `std::system_error` 回退，WASM 端串行。fixture 流数少（通常 1-2 个），并行优势不大但架构对称。

**变更量**: spz.cc +45 行

#### 3.2.6 `BuildNgspBlob(...)` (T03b)

```cpp
bool BuildNgspBlob(const SpzHeaderV4& header,
                   const std::vector<uint8_t>& extension_data,
                   const std::vector<std::vector<uint8_t>>& chunks,
                   const std::vector<uint64_t>& uncompressed_sizes,
                   std::vector<uint8_t>* out);
```

**参照**: Niantic `saveSpz` 的 v4 路径 [load-spz.cc L950-L975](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L950-L975)

**用途**: 将 header + extensions + TOC + ZSTD chunks 拼装为完整的 .spz v4 blob。被 `gen-fixture` 和 `BuildFixtureBlob` 共同使用。

**布局**: `[NgspFileHeader 32B][extensions variable][TOC: numStreams×(8+8)][ZSTD chunk 1][ZSTD chunk 2]...`

**变更量**: spz.cc +35 行

### 3.3 R1 产出清单

| #    | 事项                                              | 文件                                                            |  行数 |  方向 |
| :--- | :---------------------------------------------- | :------------------------------------------------------------ | :-: | :-: |
| T00  | zlib 版本锁定 (`find_package(ZLIB 1.3.1 REQUIRED)`) | `cpp/CMakeLists.txt`                                          |  1  |  —  |
| T01  | `ParseToc()` 函数                                 | `cpp/src/spz.cc`                                              | +40 |  解压 |
| T02  | `DecompressZstdStream()` 函数                     | `cpp/src/spz.cc`                                              | +20 |  解压 |
| T03  | `DecompressNgspStreams()` 并行+降级               | `cpp/src/spz.cc`                                              | +55 |  解压 |
| T01b | `CompressZstdStream()` 函数                       | `cpp/src/spz.cc`                                              | +20 |  压缩 |
| T02b | `CompressNgspStreams()` 并行+降级               | `cpp/src/spz.cc`                                              | +45 |  压缩 |
| T03b | `BuildNgspBlob()` 函数                            | `cpp/src/spz.cc`                                              | +35 |  压缩 |
| T04  | CMakeLists.txt zstd v1.5.6 集成 (SHA256 锁定)       | `cpp/CMakeLists.txt`                                          | +25 |  —  |
| —    | `#include <zstd.h>` + `#include <future>`         | `cpp/src/spz.cc` + `cpp/src/main.cc` + `cpp/src/wasm_main.cc` |  +4 |  —  |

**GitNexus 节奏**: R1 完成后 `npx gitnexus analyze --skip-git`（新增 zstd 调用链 + CMake 依赖变更）

***

## 四、R2 — 32 字节 NgspFileHeader 解析 (预估 1 天)

### 4.1 SpzHeaderV4 结构体 (T05)

**文件**: [spz.cc](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/spz.cc)

**参照**: [Niantic NgspFileHeader L146-157](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L146-L157)

```cpp
struct SpzHeaderV4 {
  uint32_t magic          = 0;    // 0x5053474e (NGSP)
  uint32_t version        = 0;    // >= 4
  uint32_t num_points     = 0;
  uint8_t  sh_degree      = 0;
  uint8_t  fractional_bits = 0;
  uint8_t  flags          = 0;    // 0x1=antialiased, 0x2=hasExtensions
  uint8_t  num_streams    = 0;    // ZSTD 压缩流数量
  uint32_t toc_byte_offset = 0;   // TOC 字节偏移
  uint8_t  reserved[12]   = {};   // 必须全零
};
static_assert(sizeof(SpzHeaderV4) == 32, "SpzHeaderV4 must be 32 bytes");
```

**变更量**: spz.cc +15 行 (匿名命名空间内)

### 4.2 ParseHeaderV4() (T06)

```cpp
bool ParseHeaderV4(const std::vector<uint8_t>& raw, SpzHeaderV4* h, std::string* err);
```

**参照**: Niantic `loadPackedGaussiansFromNgsp()` [load-spz.cc L777-799](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L777-L799)

**解析 + 验证**:

1. `raw.size() < 32` → error
2. `memcpy` 32 字节到 `SpzHeaderV4`
3. `magic != 0x5053474e` → error `L2_MAGIC`
4. `version < 4` → error `L2_VERSION` (v4 路径不应该收到 <4)
5. `num_points == 0` → error `L2_NUM_POINTS`
6. `sh_degree > 4` → error `L2_SH_DEGREE`
7. `reserved[12]` 全零检查 → error `L2_RESERVED` 如有非零字节 (⚠️ 审查发现 CR-IMP-1)

**变更量**: spz.cc +50 行

### 4.3 SpzL2Info 扩展 (T07)

**文件**: [report.h L74-87](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/include/spz_gatekeeper/report.h#L74-L87)

**新增字段**:

```cpp
struct SpzL2Info {
  // ... 现有字段保持不变 ...
  uint8_t  fractional_bits = 0;   // 回填 (审查发现 IMP-3)
  uint8_t  num_streams = 0;       // v4 新增
  uint32_t toc_byte_offset = 0;   // v4 新增
};
```

**变更量**: report.h +3 行

### 4.4 常量定义补全 (T08)

**文件**: [spz.h](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/include/spz_gatekeeper/spz.h)

**参照**: [Niantic L143-144](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L143-L144)

```cpp
static constexpr uint8_t kFlagAntialiased = 0x01;    // 新增 (审查发现 IMP-4)
static constexpr uint8_t kFlagHasExtensions = 0x02;  // 已有
```

**变更量**: spz.h +2 行

### 4.5 R2 产出清单

| #   | 事项                                                               | 文件                                    |  行数 |
| :-- | :--------------------------------------------------------------- | :------------------------------------ | :-: |
| T05 | `SpzHeaderV4` 结构体 + `static_assert`                              | `cpp/src/spz.cc`                      | +15 |
| T06 | `ParseHeaderV4()` 函数                                             | `cpp/src/spz.cc`                      | +50 |
| T07 | `SpzL2Info` 扩展 (fractional\_bits/num\_streams/toc\_byte\_offset) | `cpp/include/spz_gatekeeper/report.h` |  +3 |
| T08 | `kFlagAntialiased` 常量                                            | `cpp/include/spz_gatekeeper/spz.h`    |  +2 |

**GitNexus 节奏**: ❌ 不 analyze（纯新增结构体 + 常量，不影响调用图拓扑）

***

## 五、R3 — Header zone 扩展定位 (预估 1-2 天)

### 5.1 核心变更

**当前代码**: [spz.cc L271-347](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/spz.cc#L271-L347) — 扩展解析假设在 trailer (base payload 后)

**v4 实际情况**: 扩展 ILV 记录在 header zone — `bytes[32..tocByteOffset)`，**明文数据，不需要 ZSTD 解压**

**参照**: [Niantic L812-824](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L812-L824)

### 5.2 ParseHeaderZoneExtensions() (T09)

```cpp
// v4 路径专用: 解析 header zone 中的 ILV records
// ext_data: 指向 raw[32] 的指针
// ext_size: tocByteOffset - 32
IlvParseResult ParseHeaderZoneExtensions(const uint8_t* ext_data, size_t ext_size);
```

**设计要点**:

1. 与 `ParseTlvTrailer()` 共享解析核心（ILV 字节格式 `[u32 type][u32 byteLength][payload]` 一致），仅不同入参和边界条件
2. header zone 扩展是**明文** — 读取 `raw[32..tocByteOffset)` 即可解析
3. 不涉及 buffer 重绑定 — header zone 不在 decomp buffer 中
4. 边界条件: `ext_size == 0` → 无扩展；`ext_size < 8` → truncated
5. v1-v3 的 trailer 路径保持不变

**变更量**: spz.cc +30 行

### 5.3 R3 产出清单

| #    | 事项                                            | 文件                          |     行数     |
| :--- | :-------------------------------------------- | :-------------------------- | :--------: |
| T09  | `ParseHeaderZoneExtensions()` 函数              | `cpp/src/spz.cc`            |     +30    |
| T09b | `ParseTlvTrailer()` 复用标注（不改名） | `cpp/src/spz.cc` (R3 内调用) + `cpp/src/tlv.cc` (原解析器) | \~5 (引用注释) |

> **⚠️ R3 不执行函数重命名。** `ParseTlvTrailer` 保持原名在 `tlv.cc` 中。`ParseHeaderZoneExtensions` 内部逻辑参照 `ParseTlvTrailer` 的模式但作为独立函数实现。**函数和文件的 TLV→ILV 全局重命名统一在 R5 执行**（T15-T22），避免时序冲突。

**GitNexus 节奏**: R3 后 analyze（spz.cc 调用图结构变更）

***

## 六、R4 — InspectSpzBlob() 架构重构 (预估 1 天)

### 6.1 当前调用链 (🔴 审查发现 CR-2: 需要大改)

```
InspectSpzBlob(raw_spz):
  └─ DecompressGzip(raw_spz)        // 假设所有输入都是 gzip
       └─ ParseHeader(decomp, 16B)  // 只解析 16 字节
            └─ ComputeBasePayloadSize()  // 假设单 buffer 布局
                 └─ ParseTlvTrailer()    // 假设 trailer 扩展
```

### 6.2 目标双路径架构

```
InspectSpzBlob(raw_spz):
  ├─ read magic(0..3):
  │   ├─ == NGSP_MAGIC → ParseHeaderV4(32B)
  │   │   └─ version >= 4?
  │   │       ├─ ParseHeaderZoneExtensions(raw[32..tbo))     // 明文 ILV
  │   │       ├─ ParseToc() → DecompressNgspStreams()        // ZSTD 多流
  │   │       └─ validate → return GateReport
  │   │
  │   ├─ == 0x1f 0x8b → DecompressGzip()
  │   │   └─ ParseHeader(decomp, 16B)
  │   │       └─ ComputeBasePayloadSize() → ParseIlvRecords() (原 ParseTlvTrailer)
  │   │           └─ return GateReport (v1-v3 legacy, 无改动)
  │   │
  │   └─ else → AddIssue(L2_UNKNOWN_FORMAT) → return GateReport
```

**参照**: [Niantic loadSpzPacked L977-1015](file:///C:/Users/HP/Downloads/v3.0.0/spz-3.0.0/src/cc/load-spz.cc#L977-L1015)

### 6.3 细节变更

#### 6.3.1 `InspectSpzBlob` 参数名 (T10)

[spz.h L48-L53](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/include/spz_gatekeeper/spz.h#L48-L53): `gz_spz` → `raw_spz` (审查发现 MIN-3)

**变更量**: spz.h + spz.cc 共 \~10 处引用更新

#### 6.3.2 `ComputeBasePayloadSize` 标记 legacy-only (T11)

`ComputeBasePayloadSize()` 仅在 v1-v3 路径调用。新增注释 `// Legacy only: v1-v3 gzip single-buffer layout`。

**变更量**: 无代码改动，仅注释

#### 6.3.3 版本检测逻辑 (T12)

```cpp
// 在 InspectSpzBlob() 开头
const uint8_t* raw = raw_spz.data();
size_t raw_size = raw_spz.size();

if (raw_size < 4) { AddIssue(L2_TOO_SMALL); return rep; }

uint32_t magic = ReadU32LE(raw, 0);

if (magic == 0x5053474e) {
  // NGSP magic → v4 路径 (32B header + ZSTD)
  return InspectSpzBlobV4(raw_spz, opt, where);
} else if (raw[0] == 0x1f && raw[1] == 0x8b) {
  // gzip magic → v1-v3 legacy 路径
  return InspectSpzBlobLegacy(raw_spz, opt, where);  // 现有逻辑提取为函数
} else {
  AddIssue(&rep, Severity::kError, "L2_UNKNOWN_FORMAT", "unrecognized SPZ format", where);
  return rep;
}
```

#### 6.3.4 拆分为两个子函数 (T13)

```cpp
static GateReport InspectSpzBlobV4(const std::vector<uint8_t>& raw_spz,
                                   const SpzInspectOptions& opt,
                                   const std::string& where);

static GateReport InspectSpzBlobLegacy(const std::vector<uint8_t>& raw_spz,
                                       const SpzInspectOptions& opt,
                                       const std::string& where);
```

- `InspectSpzBlobLegacy()`: 现有 `InspectSpzBlob()` 的主体，无逻辑变更
- `InspectSpzBlobV4()`: 新函数，串联 ParseHeaderV4 → ParseHeaderZoneExtensions → ParseToc → DecompressNgspStreams → validation

**变更量**: spz.cc +60 行 (提取 + 新增)，原有 150 行重组

### 6.4 kKnownMaxVersion 更新 (T14)

[spz.cc L23](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/spz.cc#L23): `kKnownMaxVersion = 4` → **保持 4**，但 v4 路径不再走 warning 分支

v4 路径 (`InspectSpzBlobV4`) 对 `version > 4` 报 warning 但继续校验，对 `version == 4` 正常处理。

### 6.5 R4 产出清单

| #   | 事项                                              | 文件                 |       行数      |
| :-- | :---------------------------------------------- | :----------------- | :-----------: |
| T10 | `gz_spz` → `raw_spz` 参数重命名                      | `spz.h` + `spz.cc` |      \~10     |
| T11 | `ComputeBasePayloadSize` legacy 标记              | `spz.cc`           |       注释      |
| T12 | 版本检测 dispatch 逻辑                                | `spz.cc`           |      +20      |
| T13 | `InspectSpzBlobV4()` + `InspectSpzBlobLegacy()` | `spz.cc`           | +60 / 重组\~150 |
| T14 | `kKnownMaxVersion` 语义修正                         | `spz.cc`           |       注释      |

**GitNexus 节奏**: R4 后 **必须** analyze（spz.cc 调用图拓扑大改）

***

## 七、R5 — 术语统一 TLV→ILV (预估 0.5 天)

### 7.1 影响面分析

| 文件                                    | 变更内容                                                                                                          |  行数  |
| :------------------------------------ | :------------------------------------------------------------------------------------------------------------ | :--: |
| `cpp/include/spz_gatekeeper/tlv.h`    | → `ilv.h` 重命名，`TlvRecord`→`IlvRecord`, `TlvParseResult`→`IlvParseResult`, `ParseTlvTrailer`→`ParseIlvRecords` | \~30 |
| `cpp/src/tlv.cc`                      | → `ilv.cc` 重命名，函数名/变量名更新                                                                                      | \~20 |
| `cpp/include/spz_gatekeeper/report.h` | `TlvRecord`→`IlvRecord`, `tlv_records`→`ilv_records`, `tlv_storage`→`ilv_storage`                             | \~10 |
| `cpp/src/spz.cc`                      | 所有 `tlv`/`TLV` 引用更新 (\~25 处)                                                                                  | \~25 |
| `cpp/src/wasm_main.cc`                | 引用更新 (\~15 处)                                                                                                 | \~15 |
| `cpp/src/main.cc`                     | 注释中 TLV 引用 (\~4 处)                                                                                            |  \~4 |
| `cpp/CMakeLists.txt`                  | `src/tlv.cc`→`src/ilv.cc`                                                                                     |   1  |
| `cpp/tests/*.cc` (13 文件)              | 引用更新                                                                                                          | \~20 |
| `cpp/extensions/**/*.h`               | 引用更新                                                                                                          |  \~5 |

### 7.2 WASM 薄封装同步更新

**设计定位**（对照 [2.1 Upper Plan §1.2](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/docs/plans/2026-03-28-spz-gatekeeper-2.1-upper-implementation-plan.md#L36-L39)）：`wasm_main.cc` 是对 `spz_gatekeeper_core` 的**薄封装入口**——核心库的 ILV 重命名后，WASM 层同步更新，不存在独立前端的向后兼容问题。未来 NFC 模式入口同理：均为核心库的薄封装，字段名从核心库继承。

**处理**: [wasm\_main.cc L373-379](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/src/wasm_main.cc#L373-L379) JS 导出 `tlv_records` → `ilv_records`，同步更新 `web/spz_gatekeeper.js` 消费侧。单源多入口，薄封装统一。

### 7.3 R5 产出清单

| #    | 事项                                                                                            | 文件                                               |  行数  |
| :--- | :-------------------------------------------------------------------------------------------- | :----------------------------------------------- | :--: |
| T15  | `tlv.h` → `ilv.h` 重命名 + 内容更新                                                                  | `cpp/include/spz_gatekeeper/ilv.h` (新建)          | \~30 |
| T16  | `tlv.cc` → `ilv.cc` 重命名 + 内容更新                                                                | `cpp/src/ilv.cc` (新建)                            | \~20 |
| T17  | `report.h` 字段名更新                                                                              | `cpp/include/spz_gatekeeper/report.h`            | \~10 |
| T18  | `spz.cc` 全局引用更新                                                                               | `cpp/src/spz.cc`                                 | \~25 |
| T19  | `wasm_main.cc` 引用更新 + JS 导出字段同步 (`tlv_records`→`ilv_records`) + `web/spz_gatekeeper.js` 消费侧更新 | `cpp/src/wasm_main.cc` + `web/spz_gatekeeper.js` | \~20 |
| T20  | `main.cc` 注释更新                                                                                | `cpp/src/main.cc`                                |  \~4 |
| T21  | `CMakeLists.txt` 源文件列表更新                                                                      | `cpp/CMakeLists.txt`                             |   1  |
| T22  | 13 个测试文件 + extensions 引用更新                                                                    | `cpp/tests/*.cc` + `cpp/extensions/**/*.h`       | \~25 |
| T22b | 版权头统一 (16 文件: 8 个 `PuJunhan`→SPZ Gatekeeper Contributors + 8 个补版权头)                           | 16 文件                                            | \~32 |
| T22c | `SPZ_ADOBE_coordinate_system` 登记 (`0xADBE0003`, status=draft, validator 待后续)                     | `cpp/src/spz.cc`                                 | +12 |

### 7.4 版权头统一 (T22b)

**问题**: 17 个 C++ 文件中版权头不一致：

| 类别         | 文件数 | 说明                                                                                                                                                                       |
| :--------- | :-: | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `PuJunhan` |  8  | `spz.cc`, `wasm_main.cc`, `tlv.cc`, `audit_summary.cc`, `report.cc`, `json_min.cc`, `report.h`, `audit_summary.h`                                                        |
| **缺少版权头**  |  8  | `main.cc`, `extension_spec_registry.cc`, `spz.h`, `tlv.h`, `extension_validator.h`, `extension_spec_registry.h`, `safe_orbit_camera_validator.h`, `validator_registry.h` |

**基准格式**（[CMakeLists.txt L1-L2](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/CMakeLists.txt#L1-L2)）:

```cpp
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 SPZ Gatekeeper Contributors
```

**处理**:

- 8 个 `PuJunhan` → `SPZ Gatekeeper Contributors`
- 8 个缺少版权头的 → 在文件首行加入版权头（`#pragma once` / `#include` 之前）
- 原有文件头注释（如 `/** SPZ Gatekeeper - ... */`）保留，版权头插入其后

**变更量**: 16 文件，每文件 ±2 行。版权头修复涉及: 修正 `PuJunhan`→`SPZ Gatekeeper Contributors` (8个: `spz.cc`, `wasm_main.cc`, `tlv.cc`, `audit_summary.cc`, `report.cc`, `json_min.cc`, `report.h`, `audit_summary.h`) + 补全版权头 (8个: `main.cc`, `extension_spec_registry.cc`, `spz.h`, `tlv.h`, `extension_validator.h`, `extension_spec_registry.h`, `safe_orbit_camera_validator.h`, `validator_registry.h`)。

**GitNexus 节奏**: R5 后 analyze（文件重命名 + 版权头变更 + 调用图符号变更）

***

## 八、R6 — 回归测试 + WASM 构建验证 (预估 1-2 天)

### 8.1 v4 专项测试 (T23)

**参照**: Niantic `tests/python/test_io.py` (pytest fixture 模式)

| 测试                        | 文件                               | 内容                                                                     |
| :------------------------ | :------------------------------- | :--------------------------------------------------------------------- |
| `v4_header_parse_test`    | 新增 `cpp/tests/v4_format_test.cc` | 合法 32B header、损坏 header（magic 错误、version<4、numStreams=0、reserved 非零）   |
| `v4_zstd_decompress_test` | 同上                               | 构建合法 v4 blob（32B header + 1 stream ZSTD），验证 `DecompressNgspStreams` 成功 |
| `v4_zstd_corrupt_test`    | 同上                               | 故意损坏 ZSTD stream 数据，验证返回 `L2_ZSTD_DECOMPRESS`                          |
| `v4_header_zone_ilv_test` | 同上                               | 扩展在 `bytes[32..tocByteOffset)`，验证 `ParseHeaderZoneExtensions`          |
| `v4_version_detect_test`  | 同上                               | NGSP magic→v4 路径；0x1f 0x8b→legacy 路径；其他→报错                             |
| `v1_v3_regression_test`   | 保持现有 13 个测试全量通过                  | v1-v3 gzip 路径无退化                                                       |

**变更量**: `cpp/tests/v4_format_test.cc` 新建 \~200 行, `cpp/CMakeLists.txt` +3 行添加测试

### 8.2 v4 fixture 工具链 (T24)

利用现有的 `gen-fixture` CLI 命令扩展 v4 模式：

```cpp
// main.cc 新增 gen-fixture-v4 子命令
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --format v4 --out test.spz
```

需要在 `wasm_main.cc` 的 `BuildFixtureBlob()` 中新增 v4 路径，使用 ZSTD 压缩代替 gzip。

**变更量**: main.cc +20 行, wasm\_main.cc +30 行

### 8.3 WASM 生产标志优化 (T25)

**文件**: [CMakeLists.txt L102-L112](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/cpp/CMakeLists.txt#L102-L112)

**当前 WASM 链接选项**:

```cmake
target_link_options(spz_gatekeeper-wasm PRIVATE
  "-O3"
  "--bind"
  "--no-entry"
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=createSpzGatekeeperModule"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sENVIRONMENT=web"
  "-sEXPORT_ES6=1"
  "-sSINGLE_FILE=1"
  "-sFILESYSTEM=0"
)
```

**新增两行**:

```cmake
  "-sASSERTIONS=0"           # 关闭断言（生产模式减体积）
  "-sNO_EXIT_RUNTIME=1"      # 不退运行时（异常后保持 WASM 模块存活）
```

**理由**: 门卫网页端定位为算法快速验证台（非 spz2glb 的生产级重型转换），不需要 `HotObjectPool`/`BumpAllocator`/`emmalloc` 等重型优化。`-sASSERTIONS=0` 减 WASM 体积约 15%，`-sNO_EXIT_RUNTIME=1` 防止异常后模块被卸载导致网页需刷新。

**变更量**: CMakeLists.txt +2 行

### 8.4 WASM 构建验证 (T26)

```bash
cd cpp
mkdir build_wasm && cd build_wasm
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
emmake make spz_gatekeeper_wasm_site
```

**验证要点**:

1. Emscripten 正确编译 zstd FetchContent 静态库
2. `spz_gatekeeper_wasm.wasm` 生成成功（含 `-sASSERTIONS=0` 标志）
3. `wasm_smoke_test.mjs` 全量通过
4. v4 blob 在 WASM 环境下 `inspectSpz` 可正常返回

### 8.5 版本号 bump (T27)

| 文件                           | 变更                        |
| :--------------------------- | :------------------------ |
| `cpp/CMakeLists.txt`         | `VERSION 2.0.0` → `2.0.2` |
| `CHANGELOG.md`               | 新增 v2.0.2 条目              |
| `README.md` / `README-zh.md` | 更新版本号和命令说明 (TLV→ILV)      |

### 8.6 删除旧文件 (T28)

```
rm cpp/include/spz_gatekeeper/tlv.h
rm cpp/src/tlv.cc
```

文件已被 `ilv.h`/`ilv.cc` 替代。确保所有 `#include` 引用已更新后删除。

### 8.7 R6 产出清单

| #   | 事项                                                 | 文件                                               |   行数  |
| :-- | :------------------------------------------------- | :----------------------------------------------- | :---: |
| T23 | v4 格式 6 类专项测试                                      | `cpp/tests/v4_format_test.cc` (新建)               |  +200 |
| T24 | `gen-fixture` v4 模式扩展                              | `main.cc` + `wasm_main.cc`                       |  +50  |
| T25 | WASM 生产标志 `-sASSERTIONS=0` + `-sNO_EXIT_RUNTIME=1` | `cpp/CMakeLists.txt`                             |   +2  |
| T26 | WASM 构建验证流程                                        | `cpp/`                                           |   —   |
| T27 | 版本号 bump + CHANGELOG                               | `CMakeLists.txt` + `CHANGELOG.md` + `README*.md` |  +30  |
| T28 | 删除旧文件 `tlv.h`/`tlv.cc`                             | —                                                | -2 文件 |

**GitNexus 节奏**: R6 后 analyze（文件增删 + 调用图拓扑最终态）

***

## 九、任务汇总与依赖

### 9.1 统一编号表

| R      | T#      | 事项                                                 | 文件                    |  变更量  | 依赖          |
| :----- | :------ | :------------------------------------------------- | :-------------------- | :---: | :---------- |
| **R1** | T00     | zlib 版本锁定 `find_package(ZLIB 1.3.1 REQUIRED)`      | CMakeLists.txt        |   1   | —           |
| <br /> | T01     | `ParseToc()`                                       | spz.cc                |  +40  | —           |
| <br /> | T02     | `DecompressZstdStream()`                           | spz.cc                |  +20  | —           |
| <br /> | T03     | `DecompressNgspStreams()` 并行+降级                     | spz.cc                |  +55  | T01,T02     |
| <br /> | T01b    | `CompressZstdStream()`                             | spz.cc                |  +20  | —           |
| <br /> | T02b    | `CompressNgspStreams()` 并行+降级                       | spz.cc                |  +45  | T01b        |
| <br /> | T03b    | `BuildNgspBlob()`                                  | spz.cc                |  +35  | T02b        |
| <br /> | T04     | CMakeLists.txt zstd v1.5.6 集成 (SHA256 锁定)          | CMakeLists.txt        |  +25  | —           |
| **R2** | T05     | `SpzHeaderV4` 结构体                                  | spz.cc                |  +15  | —           |
| <br /> | T06     | `ParseHeaderV4()`                                  | spz.cc                |  +50  | T05         |
| <br /> | T07     | `SpzL2Info` 扩展                                     | report.h              |   +3  | —           |
| <br /> | T08     | `kFlagAntialiased` 常量                              | spz.h                 |   +2  | —           |
| **R3** | T09     | `ParseHeaderZoneExtensions()`                      | spz.cc                |  +30  | T05,T06     |
| <br /> | T09b    | `ParseTlvTrailer`→`ParseIlvRecords`                | ilv.cc (新)            |  \~0  | R5 同步       |
| **R4** | T10     | `gz_spz`→`raw_spz`                                 | spz.h+spz.cc          |  \~10 | —           |
| <br /> | T11     | `ComputeBasePayloadSize` legacy 标记                 | spz.cc                |   注释  | —           |
| <br /> | T12     | 版本检测 dispatch                                      | spz.cc                |  +20  | T03,T06,T09 |
| <br /> | T13     | `InspectSpzBlobV4/Legacy` 拆分                       | spz.cc                |  +60  | T12         |
| <br /> | T14     | `kKnownMaxVersion` 语义修正                            | spz.cc                |   注释  | —           |
| **R5** | T15-T22 | TLV→ILV 全局重命名 (8 项)                                | 16 文件                 | \~130 | R4 定型后      |
| <br /> | T22b    | 版权头统一 (16 文件)                                      | 16 文件                 |  \~32 | R5 同期       |
| <br /> | T22c    | `SPZ_ADOBE_coordinate_system` 登记 (`0xADBE0003`)     | spz.cc                |  +12  | R5 同期       |
| **R6** | T23     | v4 专项测试 (6 类)                                      | v4\_format\_test.cc   |  +200 | R1-R5 全     |
| <br /> | T24     | `gen-fixture` v4 扩展                                | main.cc+wasm\_main.cc |  +50  | R1-R5       |
| <br /> | T25     | WASM 生产标志 `-sASSERTIONS=0` + `-sNO_EXIT_RUNTIME=1` | CMakeLists.txt        |   +2  | —           |
| <br /> | T26     | WASM 构建验证                                          | —                     |   —   | R6 全部       |
| <br /> | T27     | 版本号 bump + 文档                                      | 4 文件                  |  +30  | R6 验证后      |
| <br /> | T28     | 删除 `tlv.h`/`tlv.cc`                                | —                     | -2 文件 | T15-T22 后   |

### 9.2 依赖图

```
R1 (ZSTD基础)
  ├──► R2 (32B头解析) ──► R3 (header zone 扩展) ──┐
  │                                                  │
  └──────────────────────────────────────────────────┤
                                                     ▼
                                              R4 (InspectSpzBlob 架构重构)
                                                     │
                                                     ▼
                                              R5 (TLV→ILV 重命名)
                                                     │
                                                     ▼
                                              R6 (测试 + WASM + 发布)
```

### 9.3 变更量汇总

| 文件                                     |    新增行   |     修改行    | 说明                          |
| :------------------------------------- | :------: | :--------: | :-------------------------- |
| `cpp/src/spz.cc`                       |   +390   | \~160 (重组) | 核心变更（解压 + 压缩 + header + 重构） |
| `cpp/include/spz_gatekeeper/report.h`  |    +13   |    \~10    | SpzL2Info 扩展 + 重命名          |
| `cpp/include/spz_gatekeeper/spz.h`     |    +2    |     \~5    | 常量 + 参数重命名                  |
| `cpp/include/spz_gatekeeper/ilv.h` (新) |    +30   |      —     | 原 tlv.h                     |
| `cpp/src/ilv.cc` (新)                   |    +20   |      —     | 原 tlv.cc                    |
| `cpp/CMakeLists.txt`                   |    +28   |      1     | zstd + 文件重命名                |
| `cpp/src/wasm_main.cc`                 |    +30   |    \~15    | v4 fixture + 重命名            |
| `cpp/src/main.cc`                      |    +20   |     \~4    | gen-fixture v4 + 注释         |
| `cpp/tests/v4_format_test.cc` (新)      |   +200   |      —     | v4 专项测试                     |
| `cpp/tests/*.cc` (13 文件)               |     —    |    \~20    | 重命名                         |
| `CHANGELOG.md`                         |    +15   |      —     | v2.0.2                      |
| `README.md` / `README-zh.md`           |    +5    |    \~10    | 版本 + 术语                     |
| **删除**                                 |     —    |    -2 文件   | `tlv.h` + `tlv.cc`          |
| **合计**                                 | **+768** |  **\~257** | <br />                      |

### 9.4 提交节奏与交叉审查门禁（强制）

每 R 阶段完成后**必须**执行交叉审查 + GitNexus 分析，**禁止**跳步。

| 阶段 | 提交信息 | GitNexus | 交叉审查重点 |
| :-- | :-- | :--: | :-- |
| R1 完成 | `feat(R1): integrate libzstd — ParseToc + DecompressZstdStream + DecompressNgspStreams` | 🔍 | ZSTD 调用链拓扑、CMake 依赖变更 |
| R2 完成 | `feat(R2): add 32B SpzHeaderV4 — ParseHeaderV4 + SpzL2Info extension + kFlagAntialiased` | 🔍 | report.h 数据结构扩散影响 |
| R3 完成 | `feat(R3): header zone extension parsing — ParseHeaderZoneExtensions for v4 ILV records` | 🔍 | spz.cc 调用图结构变更 |
| R4 完成 | `refactor(R4): InspectSpzBlob dual-path architecture — V4 ZSTD + Legacy gzip dispatch` | 🔍 | 入口分流对测试/CLI/WASM 的涟漪效应 |
| R5 完成 | `refactor(R5): TLV→ILV terminology alignment (16 files, -2 old)` | 🔍 | 文件重命名 + 符号变更全量确认 |
| R6 完成 | `test(R6): v4 format 6-category fixture tests + WASM build verification` | 🔍 | 调用图最终态无退化 |

**门禁标准**：
- GitNexus 节点/边/集群数不得出现非预期跳变（±5% 以内为正常）
- 新增符号调用链完整（无孤立节点）
- 交叉审查发现的 CR/IMP 项必须在下个 R 阶段前全部闭环

> **⚠️ R2 原标注为 ❌ 不 analyze**（"纯新增结构体 + 常量，不影响调用图拓扑"），但 2026-05-21 终审决定**每 R 均 analyze**——原因：R2 修改了 `report.h` 的 `SpzL2Info` 结构体字段布局，C++ 结构中新增 `fractional_bits`/`num_streams`/`toc_byte_offset` 三个字段会影响所有消费 `SpzL2Info` 的模块（audit_summary、compat_check、WASM export），需要通过 GitNexus 确认影响面完整。

***

## 十、交叉审查回溯

### 10.1 L2 审查发现的处理

| 审查发现                                        | 严重度 | 处理                                                           |
| :------------------------------------------ | :-: | :----------------------------------------------------------- |
| **CR-1**: 缺少 TOC 解析                         |  🔴 | ✅ 纳入 R1 T01 `ParseToc()`                                     |
| **CR-2**: InspectSpzBlob 架构重构未描述            |  🔴 | ✅ 纳入 R4 T12/T13 完整设计                                         |
| **CR-3**: WASM JS API `tlv_records` 破坏      |  🔴 | ✅ R5 §7.2: WASM 为薄封装，JS 导出字段同步更新为 `ilv_records`（单源多入口，薄封装统一） |
| **CR-4**: Emscripten zstd 构建未验证             |  🔴 | ✅ R1 T04: FetchContent 方案 + R6 T25 验证                        |
| **IMP-1**: v4 reserved\[12] 全零校验            |  🟡 | ✅ 纳入 R2 T06                                                  |
| **IMP-2**: header zone ILV 应复用现有解析          |  🟡 | ✅ R3 T09 设计明确复用                                              |
| **IMP-3**: SpzL2Info 缺少 fractional\_bits    |  🟡 | ✅ 纳入 R2 T07 回填                                               |
| **IMP-4**: 缺少 kFlagAntialiased 常量           |  🟡 | ✅ 纳入 R2 T08                                                  |
| **IMP-5**: tlv\_multi\_record\_test.cc 应重命名 |  🟡 | ✅ 纳入 R5 CMakeLists.txt                                       |
| **IMP-6**: v4 测试覆盖范围                        |  🟡 | ✅ 纳入 R6 T23 6 类专项测试                                          |
| **MIN-3**: gz\_spz 参数名过时                    |  🟢 | ✅ 纳入 R4 T10                                                  |
| 遗漏 #1: TOC 解析函数                             |  🔴 | ✅ 纳入 R1 T01                                                  |
| 遗漏 #4: v4 reserved\[12] 全零                  |  🟡 | ✅ 纳入 R2 T06                                                  |
| 遗漏 #6: SpzL2Info.fractional\_bits           |  🟡 | ✅ 纳入 R2 T07                                                  |
| 遗漏 #7: WASM JS API 兼容方案                     |  🔴 | ✅ 纳入 R5 §7.2                                                 |

### 10.2 审查发现中**不纳入 2.0.2** 的项

| 审查发现                                          | 不纳入原因                                   |
| :-------------------------------------------- | :-------------------------------------- |
| MIN-2: 结构体命名建议（SpzHeaderV4 vs NgspFileHeader） | 非功能性，保持门卫风格一致                           |
| MIN-5: include guard 重命名                      | `tlv.h` 只有 `#pragma once`，无传统 guard     |
| MIN-6: WASM export 风格                         | R6 不新增 WASM export，v4 透传现有 `inspectSpz` |
| 遗漏 #3: ComputeBasePayloadSize 废弃              | 仅标记 legacy，不删除（v1-v3 需要）                |
| 遗漏 #5: antialiased flag 验证逻辑                  | 常量已定义，验证逻辑留给后续 PR                       |

### 10.3 L4 参考项目状态

| 项目            | 版本                          | 状态                    |
| :------------ | :-------------------------- | :-------------------- |
| SPZ (Niantic) | v3.0.0 (latest)             | ✅ 稳定，不再变更             |
| zstd          | v1.5.6 (Niantic CMakeLists) | ✅ 可用，v1.5.7 发布但非阻塞升级  |
| Emscripten    | 系统安装版                       | ⚠️ 需确认版本，WASM 构建验证时检查 |

***

## 十一、实施顺序

### 11.1 宏观时序

```
Day 1: R1 (ZSTD 基础) → R2 (32B 头解析)
Day 2: R3 (header zone 扩展) → R4 (InspectSpzBlob 重构)
Day 3: R5 (TLV→ILV) → R6 开始 (v4 测试编写)
Day 4: R6 完成 (WASM 验证 + 版本 bump + CHANGELOG)
```

### 11.2 R 阶段门禁流程（每个 R 必须走完）

```
[编码] → [CTest 验证] → [git commit] → [GitNexus analyze] → [交叉审查] → [下一 R]
                                                         ↓
                                              [发现 CR/IMP → 修复 → 重新 CTest]
```

**禁止的行为**：
- ❌ 跨 R 合并提交（如 "R1+R2 done" 一个 commit）
- ❌ 跳过 GitNexus analyze 直接进入下一 R
- ❌ 交叉审查发现问题后不停下来修复

### 11.3 完成条件

- 13 个现有 C++ 测试 + 6 类 v4 专项测试全部通过
- `emcmake cmake` + `emmake make spz_gatekeeper_wasm_site` 成功
- `wasm_smoke_test.mjs` 通过
- 6 轮 GitNexus analyze 均无退化
- 6 层交叉审查（L1-L6）全部通过

***

## 十二、不在 2.0.2 范围

| 项                                  | 原因                                                                                                    |
| :--------------------------------- | :---------------------------------------------------------------------------------------------------- |
| PR #82 坐标系统扩展                      | 用户明确排除                                                                                                |
| 9-agent 宪法更新                       | 暂缓                                                                                                    |
| `ComputeBasePayloadSize` 删除        | v1-v3 legacy 路径仍需                                                                                     |
| `extension_validator.h` 注释中 TLV 引用 | 非功能性，下个 PR 统一                                                                                         |
| 本地构建/测试                            | **禁止**。测试与 WASM 构建仅在云端 CI 执行（§0.3）                                                                    |
| git push                           | **禁止**。令牌已轮换，新令牌未到位（§0.4）                                                                             |
| Heavy WASM 工程优化                    | `HotObjectPool`/`BumpAllocator`/`emmalloc`/embind 去除 等。当前 workload（<1MB zip 审查包）不需要，但未来主动混沌测试需要。记 §十三 |
| `docs/plans/` 全部入库               | 仅 `git add -f` 本计划文件，其余 plans 继续 gitignore                                                        |

### 12.1 实施前需手动清理（不在编码任务中）

| 项 | 路径 | 大小 |
|:--|:--|:--|
| WSL 构建产物 | `build*/` (9 个目录) | ~40 MB |
| 废弃草稿 | `docs/plans/2026-05-19-spz-gatekeeper-202-v4-alignment.md` | 748 行 |

> 这些在 `.gitignore` 中已排除，不参与 git 跟踪。直接 `rm -rf build*` + 删除废弃草稿即可。

***

## 十三、已知优化欠账

### 13.1 WASM 工程优化现状

三项目 WASM 优化对比：

| 维度     | Gatekeeper                      | SPZ v3.0.0 | spz2glb                                                    |
| :----- | :------------------------------ | :--------- | :--------------------------------------------------------- |
| 编译优化   | `-O3`                           | `-O3`      | `-O3 -fno-exceptions -fno-lto`                             |
| 内存控制   | `ALLOW_MEMORY_GROWTH=1`         | 同上         | `INITIAL_MEMORY=64/128MB` + `MAXIMUM_MEMORY=1GB` 双 Profile |
| 栈      | —                               | —          | `STACK_SIZE=10MB`                                          |
| 分配器    | 默认                              | 默认         | `emmalloc`                                                 |
| 异常/断言  | 开启 (2.0.2 优化后 `-sASSERTIONS=0`) | 开启         | `DISABLE_EXCEPTION_CATCHING=1` + `ASSERTIONS=0`            |
| embind | `--bind`                        | `--bind`   | 关闭 + 手动 `WebAssembly.instantiate`                          |
| 对象池    | —                               | —          | `HotObjectPool<T>` 自研热池                                    |
| 线性分配   | —                               | —          | `BumpAllocator` 64KB 工作区                                   |
| 内存追踪   | —                               | —          | `MemoryTracker` 实时诊断                                       |

### 13.2 何时需要重型优化

**当前不需要**。门卫网页端是算法快速验证台：解压 <1MB zip → 验 tiny fixture → 出报告。`HotObjectPool`/`BumpAllocator`/`emmalloc` 为 spz2glb 的 18MB 流式转换场景设计的，门卫用不上。

**未来需要**。隐藏计划中的**主动混沌测试**对门卫 WASM 和 Bridge 一体化工作台同时提出极高要求：

- 混沌测试需批量注入损坏的 v4 .spz 文件（损坏 header / 截断 TOC / 恶意 ILV payloads）
- Bridge 作为编排层同时跑门卫验证 + spz2glb 转换 + 供应链扫描
- WASM 端需要从"一次验一个"升级为"批量对抗测试"——届时需要引入 `HotObjectPool` 热池管理反复验证的 `GateReport` 对象、`BumpAllocator` 用于临时解压缓冲区

**不急于现在**。开源策略和实现方案都未定型，2.0.2 聚焦 v4 格式支持，重型优化留给未来冲刺。

***

**计划版本**: 1.6  
**审查状态**: 6 层交叉审查通过 (L1✅ L2✅ L3✅ L4✅ L5 N/A L6 待实施后同步)  
**GitNexus 三项目基线**: Bridge 1768节点 · Gatekeeper 1368节点 · SPZ v3.0.0 767节点  
**变更总量**: +848 行新增 / ~257 行修改 / 29 个任务 (T00-T28)
