# SPZ Gatekeeper 2.0.2 — SPZ v4 格式完整对齐计划

> 更新日期: 2026-05-21\
> 状态: 设计审查通过（6层交叉审查），实施中。**R1 ✅ 已完成** (2026-05-22, CI 全平台通过)\
> 基于: [SPZ v4 深度对比](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/docs/plans/research_report_spz_v4_vs_gatekeeper_deep_comparison.md) + [202对齐研究](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/docs/plans/research_report_spz_gatekeeper_202_alignment.md) + [实测报告](file:///C:/Users/HP/Downloads/HunYuan3D_test_cases/spz_gatekeeper_project/docs/plans/spz_v4_validation_final_report.md) + \[L2 代码对标审查] + \[GitNexus 三项目基线] + \[Git 历史审查]

> **对照基线**: SPZ v3.0.0 (Niantic latest `C:\Users\HP\Downloads\v3.0.0\spz-3.0.0`, `gitnexus analyze -r spz-3.0.0` 767节点/1494边/23集群/32流)\
> **目标项目**: Gatekeeper v2.0.0→v2.0.2 (`C:\Users\HP\Downloads\HunYuan3D_test_cases\spz_gatekeeper_project`, `gitnexus analyze -r spz_gatekeeper` 1,454节点/3,488边/42集群/58流 post-R4)

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

### 0.4 令牌与分支状态

> ✅ **令牌**: 2026-05-25 已轮换。
>
> ✅ **R4 已完成**: PR #11。
>
> ✅ **R5 已完成** (2026-05-26): PR #12 合并 — TLV→ILV + copyright PuJunhan + T22c coord_system + L2_→SPZ_ error codes。
> 4 commits, 22 files, CTest 13/13。
>
> ✅ **R4 已完成**: `feature/spz-gatekeeper-2.0.2-r4` → L1-L6 交叉审查通过 (PASS_WITH_NOTES)，3 files +201/-119。
>
> 🔜 **R5 已完成** (2026-05-26): TLV→ILV 术语统一 + 函数/文件全局重命名。

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
| 提交消息格式 | `feat(R#): <描述>` 或 `refactor(R#): <描述>`, 含 `gitnexus impact: <files> <risk>` |
| GitNexus 命令 | 统一 `gitnexus <cmd> -r spz_gatekeeper`（PATH 含 `C:\Program Files\Git\bin`，无 `--skip-git`） |

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

1. `header.numStreams == 0` → 报错 `SPZ_TOC_NUM_STREAMS_ZERO`
2. `header.numStreams > 6` → 警告 `SPZ_TOC_NUM_STREAMS_EXCEEDS`（SPZ v4 规范当前定义≤6 流，但 `numStreams` 字段允许未来扩展。不拦截，仅记录）
3. `header.tocByteOffset < 32` → 报错 `SPZ_TOC_OFFSET`
4. `tocEnd (= tocByteOffset + numStreams * 16) > size` → 报错 `SPZ_TOC_TRUNCATED`
5. 遍历每个 stream entry: 读 `compressedSize`/`uncompressedSize`，累加 `compressedOffset`
6. `compressedOffset != size` → 报错 `SPZ_TOC_SIZE_MISMATCH`

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

**R1 完成状态 (2026-05-22)**:

| #    | 状态 | 说明 |
| :--- | :--: | :--- |
| T00  |  ✅  | `find_package(ZLIB QUIET)` + `FetchContent(zlib-1.3.2 SHA256)` 双轨（对标 SPZ PR#89） |
| T01  |  ✅  | `ParseToc()` — 3 项内部校验；header 级校验推迟到 R2 |
| T02  |  ✅  | `DecompressZstdStream()` — `ZSTD_getFrameContentSize` 动态边界 |
| T03  |  ✅  | `DecompressNgspStreams()` — `#if __EMSCRIPTEN__` 守卫 + `std::async` + `goto`→`bool` |
| T01b |  ✅  | `CompressZstdStream()` — `ZSTD_compressBound` + level=12 |
| T02b |  ✅  | `CompressNgspStreams()` — 与 T03 对称实现 |
| T03b |  ✅  | `BuildNgspBlob()` — 完整 32B header + extensions + TOC + chunks |
| T04  |  ✅  | `find_package(zstd QUIET)` + `FetchContent(SOURCE_SUBDIR build/cmake)` + `zstd::zstd` alias（对标 Niantic v3.0.0 L52-75） |
| —    |  ✅  | `#include <zstd.h>` + `<future>` + `<thread>`（`__EMSCRIPTEN__` 守卫） |

**CI 验证**: Run #75 — Ubuntu ✅ macOS ✅ Windows ✅ WASM ✅ (2026-05-22 12:55 UTC)\
**CI 验证**: Run #76 — Ubuntu ✅ macOS ✅ Windows ✅ WASM ✅ (zlib 1.3.2 FetchContent, 2026-05-22 ~13:30 UTC)

**GitNexus 基线**: 1,343 节点 / 3,315 边 / 40 集群 / 58 流 (`gitnexus analyze -r spz_gatekeeper`)

**关键教训**: `SOURCE_SUBDIR build/cmake` 缺失导致 5 轮 CI 失败。zstd 仓库的 CMakeLists.txt 不在根目录。对标 Niantic CMakeLists.txt L61 后发现并修复。\
**zlib 升级**: 对标 SPZ PR#89，zlib 由 `find_package(REQUIRED)` 改为 `find_package QUIET` + `FetchContent(zlib-1.3.2 SHA256)` 双轨，与 zstd 模式统一。

**GitNexus 节奏**: R1 完成后 `gitnexus analyze`（新增 zstd 调用链 + CMake 依赖变更）。Windows Git 2.54 winget 安装，无 `--skip-git`。

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
3. `magic != 0x5053474e` → error `SPZ_FORMAT_MAGIC`
4. `version < 4` → error `SPZ_FORMAT_VERSION` (v4 路径不应该收到 <4)
4.5 `version > kKnownMaxVersion` → warning `SPZ_FORMAT_VERSION` (社区定制/未来版本，不拦截，继续校验)
5. `num_points == 0` → error `SPZ_FORMAT_NUM_POINTS`
6. `sh_degree > 4` → error `SPZ_FORMAT_SH_DEGREE`
7. `reserved[12]` 全零检查 → error `SPZ_FORMAT_RESERVED` 如有非零字节 (⚠️ 审查发现 CR-IMP-1)

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

**GitNexus 节奏**: ~~❌ 不 analyze~~ → ✅ R2 后执行 `gitnexus analyze` + `gitnexus context` 逐个符号死代码检测: 1,377 nodes pre / 1,391 nodes post (+14 +1.0%)。见 §4.6 审计表。

**R2 完成状态 (2026-05-24)**:

| #   | 状态 | 说明 |
| :-- | :--: | :--- |
| T05 | ✅   | `SpzHeaderV4` struct — 字节布局与 Niantic NgspFileHeader (L146-157) 完全对齐（命名风格 snake_case 与门卫现有 SpzHeader 一致） |
| T06 | ✅   | `ParseHeaderV4()` — 7 项校验 (size<32/magic/version<4/num_points==0/sh_degree>4/reserved[12]全零) |
| T07 | ✅   | `SpzL2Info` 扩展 — fractional_bits/num_streams/toc_byte_offset 三字段 |
| T08 | ✅   | `kFlagAntialiased = 0x01` — 对标 Niantic L143 `FlagAntialiased = 0x1` |

**CI 验证**: Run #82 — Ubuntu ✅ macOS ✅ Windows ✅ WASM ✅ (2026-05-24 08:16 UTC)

**L4 交叉审查**: SpzHeaderV4 字节布局 1:1 对齐 Niantic NgspFileHeader，字段类型/顺序/size/static_assert 完全一致。命名差异（camelCase→snake_case）与门卫现有 SpzHeader 风格一致，非偏离。

**GitNexus post-merge**: 1,391 nodes / 3,409 edges / 42 clusters / 58 flows (+14 nodes +1.0% vs pre-R, ±5% 内正常)。结构体+常量级变更，回调图影响极低。

**PR**: [#9](https://github.com/spz-ecosystem/spz_gatekeeper/pull/9) — merged 2026-05-24

### 4.6 R1-R2 死代码审计 (2026-05-24, GitNexus context + rg 生产入口点扫描)

| 符号 | incoming | 消费方 | 生产入口 | 判定 |
|:--|:--|:--|:--|:--|
| `CompressGzip` | 0 | — | — | ⚠️ 已标 `[[maybe_unused]]`，gzip 压缩工具，v4 路径不经过。下次 PR 可移除 |
| `CompressNgspStreams` | 0 | — | — | 🟡 R6 fixture 生成才消费 |
| `CompressZstdStream` | 1 | `CompressNgspStreams` | `spz.cc` 内部调用 | ✅ |
| `ParseToc` | 0 | — | — | 🟡 R5 v4 多流解压才消费 |
| `DecompressNgspStreams` | 0 | — | — | 🟡 R5 v4 解压路径才消费 |
| `DecompressZstdStream` | 1 | `DecompressNgspStreams` | `spz.cc` 内部调用 | ✅ |
| `SpzHeaderV4` | 0 | — | — | ✅ R3-R4 预计接入 `InspectSpzBlobV4` |
| `ParseHeaderV4` | 0 | — | — | ✅ R3-R4 预计接入 `InspectSpzBlobV4` |
| `DecompressGzip` | 1 | `InspectSpzBlob` | `spz.cc`→`main.cc`/`wasm_main.cc` 生产链 | ✅ 生产入口点确认 |
| `RebindTlvRecordViews` | 1 | `InspectSpzBlob` | `spz.cc`→`main.cc`/`wasm_main.cc` 生产链 | ✅ 生产入口点确认 |

**三关检测摘要**:
- 第一关 `context`: 4 个 `incoming: {}` → spiraling 豁免；2 个 `incoming: {spz.cc 内部}` → 内部调用链；2 个 `incoming: {InspectSpzBlob}` → 生产入口链
- 第二关 `rg`: InspectSpzBlob 在 `spz.cc` 中被 `main.cc`/`wasm_main.cc` dispatch → 确认为生产消费者，非测试自循环
- 第三关 plan: 所有 spiraling 符号在对应 R 阶段均有显式消费目标登记

**结论**: 无死代码，无测试自循环。Bridge 的 `verify_integration.py` self-cycle 模式在 Gatekeeper 中通过对 InspectSpzBlob dispatch chain 的二级验证被排除。

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

**GitNexus 节奏**: R3 后 `gitnexus analyze` + `gitnexus context` 逐个新增函数（ParseHeaderZoneExtensions 等）

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
  │   └─ else → AddIssue(SPZ_FORMAT_UNKNOWN) → return GateReport
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

if (raw_size < 4) { AddIssue(SPZ_FORMAT_TOO_SMALL); return rep; }

uint32_t magic = ReadU32LE(raw, 0);

if (magic == 0x5053474e) {
  // NGSP magic → v4 路径 (32B header + ZSTD)
  return InspectSpzBlobV4(raw_spz, opt, where);
} else if (raw[0] == 0x1f && raw[1] == 0x8b) {
  // gzip magic → v1-v3 legacy 路径
  return InspectSpzBlobLegacy(raw_spz, opt, where);  // 现有逻辑提取为函数
} else {
  AddIssue(&rep, Severity::kError, "SPZ_FORMAT_UNKNOWN", "unrecognized SPZ format", where);
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

**GitNexus 节奏**: R4 后 **必须** `gitnexus analyze` + `gitnexus impact -r spz_gatekeeper InspectSpzBlob` 爆破半径确认（spz.cc 调用图拓扑大改）

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
| T22d | 错误码前缀 `L2_` → `SPZ_<CATEGORY>_` 标准化 (17 个旧码 + 新码全部重命名)                                    | 7 文件                                               | \~50 |

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

**GitNexus 节奏**: R5 后 `gitnexus analyze` + `gitnexus detect-changes --base-ref <pre-R5-sha>` 确认文件重命名符号映射正确

### 7.5 错误码前缀标准化 (T22d)

**问题**: 当前所有错误码使用 `L2_` 前缀，是早期设计遗留（L1 的 glb 验证被拆分后留下的考古标记），对外部消费者无信息量。且所有码混在同一前缀下，无法按类别区分。

**对齐标准**: 业界主流三段式分类前缀——`<NAMESPACE>_<CATEGORY>_<DESCRIPTION>`（如微服务 `ORD-API-001`，嵌入式 `ERR_DB_CONNECT_FAILED`）。

**方案**: 全量 17 个旧码 + R1-R4 新增码统一映射到 `SPZ_<CATEGORY>_` 二级分类前缀：

| 旧码 | 新码 | 类别 |
|:--|:--|:--|
| `L2_MAGIC` | `SPZ_FORMAT_MAGIC` | 格式校验 |
| `L2_VERSION` | `SPZ_FORMAT_VERSION` | 格式校验 |
| `L2_SH_DEGREE` | `SPZ_FORMAT_SH_DEGREE` | 格式校验 |
| `L2_RESERVED` | `SPZ_FORMAT_RESERVED` | 格式校验 |
| `L2_HEADER` | `SPZ_FORMAT_HEADER` | 格式校验 |
| `L2_TRUNCATED` | `SPZ_FORMAT_TRUNCATED` | 格式校验 |
| `L2_BASE_SIZE` | `SPZ_FORMAT_BASE_SIZE` | 格式校验 |
| `L2_NUM_POINTS` (新增) | `SPZ_FORMAT_NUM_POINTS` | 格式校验 |
| `L2_TOO_SMALL` (新增) | `SPZ_FORMAT_TOO_SMALL` | 格式校验 |
| `L2_UNKNOWN_FORMAT` (新增) | `SPZ_FORMAT_UNKNOWN` | 格式校验 |
| `L2_GZIP_DECOMPRESS` | `SPZ_DECOMPRESS_GZIP` | 压缩/解压 |
| `L2_ZSTD_DECOMPRESS` (新增) | `SPZ_DECOMPRESS_ZSTD` | 压缩/解压 |
| `L2_TOC_OFFSET` (新增) | `SPZ_TOC_OFFSET` | TOC |
| `L2_TOC_TRUNCATED` (新增) | `SPZ_TOC_TRUNCATED` | TOC |
| `L2_TOC_SIZE_MISMATCH` (新增) | `SPZ_TOC_SIZE_MISMATCH` | TOC |
| `L2_NUM_STREAMS_ZERO` (新增) | `SPZ_TOC_NUM_STREAMS_ZERO` | TOC |
| `L2_NUM_STREAMS_EXCEEDS_EXPECTED` (新增) | `SPZ_TOC_NUM_STREAMS_EXCEEDS` | TOC |
| `L2_TLV_PARSE` | `SPZ_EXT_PARSE` | 扩展 |
| `L2_EXT_VALIDATION` | `SPZ_EXT_VALIDATION` | 扩展 |
| `L2_EXT_REGISTERED_NO_VALIDATOR` | `SPZ_EXT_REGISTERED_NO_VALIDATOR` | 扩展 |
| `L2_EXT_UNREGISTERED_VALIDATOR` | `SPZ_EXT_UNREGISTERED_VALIDATOR` | 扩展 |
| `L2_EXT_UNKNOWN` | `SPZ_EXT_UNKNOWN` | 扩展 |
| `L2_UNDECLARED_TRAILER` | `SPZ_EXT_UNDECLARED_TRAILER` | 扩展 |
| `L2_EXT_DECLARED_NO_TRAILER` | `SPZ_EXT_DECLARED_NO_DATA` | 扩展 |

**影响面**: 7 个文件，32 处字符串引用（`spz.cc` 17 + `main.cc` 4 + 3 个测试文件 7 + `README.md/zh` 4）。

**变更量**: 7 文件，\~50 行。

> **为何并入 R5**: 去 `L2_` 前缀和去 `TLV` 命名同为早期设计遗留清理，在同一 R 阶段集中处理避免跨 R 字符串碎片化。

***

## 七.5、R5.5 — Gatekeeper CLI 治理硬化 (SkillOpt 启发) (预估 0.5 天)

> ✅ **R5.5 已完成** (2026-05-28): T29 Edit Budget + T30 Rejected-Edit Buffer + T31 Slow/Meta Update。~100 行改动。全部验证通过。
>
> **Commits** (5):
> - `d6f5a55` feat(R5.5): T29 Edit Budget + T30 Rejected-Edit Buffer + T31 Slow/Meta Update
> - `d50aacc` fix(R5.5): T30 rejected_edits closed loop — read-back + previous_rejections
> - `4213654` feat(R5.5): GitNexus optimizer integration — monitor --gitnexus + advance gate
> - `4b1466e` fix(R5.5): GitNexus via Windows Node.js (powershell.exe) + PRM monitor integration
> - `c492cad` chore: silence pwd -W error in gitnexus fallback path

> **定位**: R6 编码阶段的治理基础设施预备。参考 Microsoft SkillOpt 论文 (arXiv:2605.23904) 的 5 步受控循环，将 3 个关键控制机制移植到 Gatekeeper CLI，确保 R6 编码阶段的 agent 行为可控。
>
> **触发事件**: R5 裸跑暴露了 agent 自由编辑无约束的问题。SkillOpt ablation 证明：edit budget (+3.8) + rejected buffer (+4.6) + slow update (+22.5) 是稳定 skill 优化的三个必要条件。

### T29: Edit Budget (textual learning rate)

**文件**: `gatekeeper-cli.sh` → `cmd_advance()`

**设计**: advance 通过时，记录本次 R-phase 的规则变更数。超过 budget 的变更被拒绝。

```bash
# advance 输出新增 edit_budget 字段
{"allowed":true,...,"edit_budget":{"max":4,"used":2,"remaining":2}}
```

**变更量**: ~15 行

### T30: Rejected-Edit Buffer

**文件**: `gatekeeper-cli.sh` → `cmd_advance()` + `.gatekeeper.state`

**设计**: advance 拒绝时，将 violations 写入 `gate_data.rejected_edits`（带时间戳和 stage）。下次 advance 时，检查是否重复同样的 violations。

```json
"gate_data": {
  "rejected_edits": [
    {"stage":"STAGE_1","violations":["build_exists=false","no commits"],"timestamp":"2026-05-26T..."}
  ]
}
```

**变更量**: ~20 行

### T31: Slow/Meta Update (跨 R-phase 记忆)

**文件**: `gatekeeper-cli.sh` → `cmd_advance()` + `references/retrospect.md`

**设计**: 每个 R-phase 完成时（advance DONE），自动比较当前 R 和上一个 R 的 gate_data 差异。写入 `references/retrospect-{R}.md`。

```bash
# advance DONE 时自动生成 retrospect
bash gatekeeper-cli.sh advance DONE
# → 自动生成 references/retrospect-r6.md (比较 R5 vs R6 的 gate_data)
```

**变更量**: ~30 行

### R5.5 产出清单

| # | 事项 | 文件 | 变更量 | 依赖 |
|:--|:--|:--|:--:|:--|
| T29 | Edit Budget | gatekeeper-cli.sh | +15 | — |
| T30 | Rejected-Edit Buffer | gatekeeper-cli.sh + .gatekeeper.state | +20 | T29 |
| T31 | Slow/Meta Update | gatekeeper-cli.sh + references/ | +30 | T30 |

---

## 七.6、R5.6 — GitNexus Optimizer + PRM 集成 (预估 1 天)

> **定位**: 在 R5.5 (SkillOpt 3 机制) 基础上，将 GitNexus 接入 CLI 形成完整优化器。R5.5 提供 gate+buffer，R5.6 提供 analysis engine。
>
> **触发**: R6 工作流展示 Worktree Confusion 后，发现 R5.5 的 T30 rejected_edits 只存不读。修复后需 GitNexus 作为分析引擎形成完整 SkillOpt 闭环。
>
> **参考文献**:
> - SkillOpt arXiv:2605.23904 §3 (完整优化器: Reflect/Edit/Gate/Buffer)
> - PRM Survey arXiv:2510.08049 (步骤级 PRM + 生成式审计)
> - CSO ACL 2026 arXiv:2602.03412 (16% 关键步骤监督)
> - GuardAgent ICML 2025 (确定性 guardrail + LLM 双层)
> - HL Harness (翁家翌 2026 — 规则即策略)
>
> **Commits** (3):
> - `7a44998` feat(R5.6): T34 PRM+GitNexus fusion — inject code graph into API prompt
> - `87f4303` fix(R5.6): STAGE_0 gate hardening — cross-review triage + 3 fixes
> - `65318fb` fix(R5.6): P0+P1+P2 cross-review — 7 vulns 3 fixed 3 patched

### T32: rejected_edits 闭环（已实现）

**文件**: `gatekeeper-cli.sh` → `cmd_advance()`

**现状** (R5.5): T30 已实现 rejected_edits 写入，但从不回读。buffer 只存不读，反馈循环断裂。

**修复** (commit `d50aacc`):
- `cmd_advance()` 入口回读 `gate_data.rejected_edits`
- ALLOWED/BLOCKED_BY_MONITOR/BLOCKED_BY_GATES 三条路径均输出 `previous_rejections`（最近 5 条）
- agent 通过 JSON 的 `previous_rejections` 字段获取结构化失败历史

**SkillOpt §3.5 对照**: rejected buffer ablation 提升 +4.6 (SpreadsheetBench)。buffer 生效前提是下一轮优化器消费——当前 `previous_rejections` 输出实现了消费接口。

**变更量**: ~35 行

### T33: GitNexus 分析引擎触发（已实现）

**文件**: `gatekeeper-cli.sh` → `cmd_monitor()` + `cmd_advance()`

**设计**: 新增 `monitor --gitnexus <stage>` 命令，分阶段触发不同 GitNexus 模式：

```bash
# STAGE_0: 全量 analyze — 基线 baseline
bash gatekeeper-cli.sh monitor --gitnexus STAGE_0
# 输出: {"nodes":"1415","edges":"3537","clusters":"42","flows":"66",...}

# 存储到 gate_data.gitnexus
# STAGE_3/4 advance 时检查 baseline 是否存在（WARN）
```

**GitNexus 调用路径**: 全部走 Windows Node.js (`powershell.exe -Command "gitnexus analyze"`)，避免 WSL Node.js 版本过低不支持 `??` 操作符的问题。已验证 Windows 原生输出: 1415 nodes / 3537 edges / 42 clusters / 66 flows。

**分阶段触发设计**（待实现扩展）:

| Stage | GitNexus Mode | 用途 | 优先级 |
|:--|:--|:--|:--:|
| STAGE_0 | `analyze` | 全量代码图基线 | P0 |
| STAGE_1 | `context <file>` | 编辑时调用链指引 | P1 |
| STAGE_2 | `detect-changes` | CI 前后代码变化 | P2 |
| STAGE_3 | `impact <symbol>` | 审查时爆破半径 | P0 |
| STAGE_4 | `analyze` (delta) | 合并后基线更新 | P1 |
| STAGE_5 | `status` | 退休确认 | P2 |

**变更量**: ~30 行

### T34: PRM monitor + GitNexus 融合（部分实现）

**文件**: `gatekeeper-cli.sh` → `_collect_hard_evidence()` + `_monitor_local()`

**已实现**:
- `_collect_hard_evidence()` 通过 Windows `powershell.exe -Command "gitnexus status"` 获取 GitNexus 状态
- `_monitor_local()` 检查 `l3_gitnexus_baseline` 作为 STAGE_3 评分项
- STAGE_3/4 advance gates 检查 `gate_data.gitnexus.baseline` 是否存在

**论文对照 — PRM Survey (P1)**:

| PRM 原则 | P1 要求 | 当前实现 | 差距 |
|:--|:--|:--|:--:|
| 步骤级评估 | 每步独立评分 | 5 stage schema 独立 | ✅ |
| 生成式审计 | checks[].evidence | JSON 每条附带 evidence | ✅ |
| 硬证据防御 | 不可伪造来源 | gitnexus_status, ctest_output 等 10 项高价值 | ✅ |
| **语义+代码图** | **多维度交叉验证** | **gitnexus context 未注入 API prompt** | ❌ P1 |

**待实现**: `monitor --api` prompt 注入 gitnexus context/impact JSON，使 API 语义评分能看到代码图分析（"此符号被 3 个模块调用，爆破半径 HIGH"）。当前缺失。

**变更量**: ~20 行（待实现）

### T35: GitNexus 分阶段触发 — advance 门禁（已部分实现）

**文件**: `gatekeeper-cli.sh` → `cmd_advance()`

**已实现**:
- STAGE_3→4 gate: 检查 `gate_data.gitnexus.nodes` 非空（WARN 级）
- GitNexus 不可用时优雅降级 (nodes="unavailable")

**设计理由** (CSO P4): 仅关键阶段需要深度 GitNexus 分析。STAGE_3/4 是 CSO 定义的"verified critical step"——代码审查+合并决策需要代码图验证。STAGE_1/2 仅需轻量 status 检查。

| Gate | 检查强度 | GitNexus 需求 | 原因 |
|:--|:--:|:--|:--|
| STAGE_0→1 | local | `status` (仅 up-to-date) | 启动阶段，代码图无需深度分析 |
| STAGE_1→2 | local | `detect-changes` | 首次提交，需确认代码变化范围 |
| STAGE_2→3 | local | `status` | CI 阶段，仅需确认 baseline 有效 |
| STAGE_3→4 | **API** | `impact` + `context` | 关键审查：需知道符号调用链 |
| STAGE_4→5 | local | `analyze` (delta) | 合并完成，更新基线 |
| STAGE_5→DONE | local | `status` | 退休确认 |


### R5.6 产出清单

| # | 事项 | 文件 | 变更量 | 依赖 |
|:--|:--|:--|:--:|:--|
| T32 | rejected_edits 闭环 | gatekeeper-cli.sh | +35 | T30 |
| T33 | GitNexus 引擎触发 | gatekeeper-cli.sh | +30 | T32 |
| T34 | PRM + GitNexus 融合 | gatekeeper-cli.sh | +20 (待实现) | T33 |
| T35 | advance 门禁 | gatekeeper-cli.sh | +15 | T33 |
| — | GitNexus Windows Node.js 迁移 | gatekeeper-cli.sh | +13/-12 | — |

---

## 七.7、R5.7 — 全面文档对齐 & 架构描述修正 (预估 0.5 天)

> **定位**: R5.5 + R5.6 的跨文档一致性修复。所有引用 R5.5/R5.6 的文档需同步更新：reference 文件、project_rules.md、agent-self-correction、SKILL.md、stage references。
>
> **架构原则升级**: "文档是 CLI 的输出, 不是输入" → 新增: "GitNexus 是优化器的分析引擎, PRM 是评分机制, rejected_edits 是内存 — 三者不存于文档中, 存在于 structured state 中。文档仅是审计快照。"
>
> **Commits** (1):
> - `32508bb` docs(R5.7): document alignment — project_rules#19 + prm-monitor + plan

### T36: 优化器架构文档统一

| 文档 | 需更新内容 |
|:--|:--|
| `SKILL.md` §CLI Execution Layer | 新增 Optimizer Architecture: GitNexus + PRM + Buffer 三组件描述 |
| `references/stage-6-r6-flow.md` §架构原则 | 更新: 文档是 CLI 的输出 + GitNexus 是优化器分析引擎 |
| `references/prm-monitor-methodology.md` | 新增 GitNexus 分阶段触发表 + 论文交叉引用 |
| `references/game-path-audit.md` | 新增 "agent 绕过 CLI 写主 worktree" 游戏路径 |
| `project_rules.md` | #19 T30 缺陷状态更新为"已修复" |
| `202 plan` R5.5 产出清单 | 更新 T30 状态为"已闭环(t32)" |

### T37: Red Flag 更新

新增 Red Flag:
```
- pre-tool-check edit 返回 BLOCK → 检查目标路径是否在 feature worktree 上
  (P7 检查: 路径必须 ∈ 活跃 worktree)
- advance JSON 不含 previous_rejections 字段 → 检查 state 文件
  (T30 闭环后必须输出)
- gitnexus 调用失败 (SyntaxError: ??)
  → 检查 WSL Node.js 版本, 改用 Windows Node.js
- agent 写文件到 C:\Users\HP\Downloads\...
  → 这是 main worktree, 必须用 feature worktree: /root/.../feature/<branch>/
```

---

## 七.8、R5.8 — 交叉审查漏洞修复 + 硬化验证 (预估 1 天)

> **定位**: R5.5-R5.7 的 4 路并行子 agent 交叉审查发现 12 项漏洞（2 CRITICAL + 7 HIGH + 3 MEDIUM）。本章记录全部发现及修复。
>
> **审查团队**: SA1 (gate 逻辑) + SA2 (GitNexus 集成) + SA3 (对抗绕过) + SA4 (论文偏差)
>
> **参考文献**: PRM Survey arXiv:2510.08049 §7.3, CSO arXiv:2602.03412 §3.2, SkillOpt §3.5, GuardAgent ICML 2025
>
> **Commits** (6):
> - `48e0ca4` fix(cross-review): 4 CRITICAL+HIGH vulns from sub-agent audit
> - `00325a0` fix(R5.8): realpath symlink detection + stash check + stash WARN
> - `8dbed5c` fix(R5.8): MAIN_REPO undefined bug (SA2 round 2) — use MAIN_REPO_WIN
> - `28c8ecd` fix(R5.8 R3): gitnexus status --repo unsupported + realpath BLOCK
> - `60f0a80` fix(R5.8): P1 stash dead-worktree detection + CSO §3.2 CI note
> - `d953c97` fix(R5.8 R4): checksum_verify now exits 1 on mismatch

### 发现 A: 结构门禁 — CRITICAL (SA1)

| 漏洞 | 位置 | 行号 | 修复 commit |
|:--|:--|:--:|:--:|
| STAGE_2→STAGE_3 仅 `ACTION:` 提示，不阻断 | gatekeeper-cli.sh | 828→828 | `48e0ca4` |
| STAGE_4→STAGE_5 仅 `ACTION:` 提示，不阻断 | gatekeeper-cli.sh | 838→842 | `48e0ca4` |
| Push URL 正则 `^https://github\.com` 匹配子域名 | gatekeeper-cli.sh | 408→408 | `48e0ca4` |
| SHA256 `_state_checksum_verify` 定义但从不调用 | gatekeeper-cli.sh | 198→759 | `48e0ca4` |

**CSO §3.2 对照**: STAGE_2→3 和 STAGE_4→5 是 CSO 定义的"关键决策点"(alternate actions flip task outcomes)，但原先完全无验证。Agent 可在 CI 失败、PR 未合并的情况下直接推进。

### 发现 B: 空字段绕过 — HIGH (SA1 + SA3)

| 漏洞 | 位置 | 行号 | 修复 commit |
|:--|:--|:--:|:--:|
| 空 worktree: `rg ""` 匹配 git worktree list 所有行 | gatekeeper-cli.sh | 816→816 | `48e0ca4` |
| 空 branch: `rg -cF ""` 匹配 CI yml 所有行 | gatekeeper-cli.sh | 827→827 | `48e0ca4` |
| 空 worktree: `pre_tool_check` 整个路径检查跳过 | gatekeeper-cli.sh | 379→759 | `48e0ca4` |

**绕过链**: 空 worktree + 空 branch + SHA256 不验证 = agent 可编辑 state 文件后直接推进整个 R-phase，零编码、零 CI、零 PR。

### 发现 C: 对抗绕过 — HIGH (SA3)

| 漏洞 | 说明 | 优先级 |
|:--|:--|:--:|
| 符号链接从 worktree 指向 main worktree | `ln -s` 创建，路径检查通过但写入主仓库 | P0 |
| 8.3 短文件名绕过 `C:\Users\DOWNLO~1\...` | Windows 路径别名，正则无法匹配完整路径 | P0 |
| Junction/mount point 绕过 | `C:\Users\HP\Downloads\spz_link → spz_gatekeeper_project` | P1 |
| `git commit --allow-empty` 绕过 | 零代码变更的提交通过检查 | P0 fix |
| `git stash` 隐藏未提交变更 | advance 时 `git status` 为空，检查后 pop | P0 fix |
| `git commit --amend` 绕过 | 先提交合法消息再修改 | P1 |
| agent 不调用 pre_tool_check 直接 git push | CLI 是主动调用，不是强制拦截 | 架构限制 |

### 发现 D: 论文偏差 (SA4)

| 论文发现 | 我们的映射 | 风险 |
|:--|:--|:--:|
| PRM Survey §7.3: 规则奖励"最脆弱" | STAGE_0/1/2/5 纯规则闸门 | 🟡 API PRM 仅在 STAGE_3/4 生效 |
| PRM Survey §7.4: ORM 泛化优于 PRM | 同一 v4-flash 用于所有 API stage | 🟡 无跨 stage 泛化测试 |
| PRM Survey §2.2: 形式化验证最稳健 | git worktree list 验证 | ✅ 已使用 |
| CSO §3.2: 关键步骤需验证 | STAGE_0→1 工具选择 = 关键步骤 | ✅ 已加固 (87f4303) |

### R5.8 修复清单

| 优先级 | 修复 | 文件 | 工作量 |
|:--:|:--|:--|:--:|
| P0 | `pre_tool_check` 调用 `realpath` 解引用符号链 | gatekeeper-cli.sh | ~10 行 |
| P0 | STAGE_1→2 检测 `git stash` 存在 | gatekeeper-cli.sh | ~5 行 |
| P0 | STAGE_2→3 检测 `--allow-empty` 提交 | gatekeeper-cli.sh | ✅ 已有 |
| P0 | Push URL 仓库名正则验证 | gatekeeper-cli.sh | ✅ 已有 |
| P1 | STAGE_1 分支基于 main 的最新 commit 验证 | gatekeeper-cli.sh | ~10 行 |
| P1 | `git commit --amend` 检测 | gatekeeper-cli.sh | ~5 行 |
| P2 | 第二轮回合审查验证修复 | gatekeeper-cli.sh | ~30 行测试 |

### 修复确认 (第二轮子 agent 审查)

R5.8 完成后，启动 4 路子 agent 二次审查验证：

| 审查 | 结论 | 新发现 |
|:--|:--|:--|
| SA1: gate 逻辑验证 | 7/7 修复全部到位 | — |
| SA2: GitNexus 集成 | JSON 解析正确（json.loads，非 regex） | **`$MAIN_REPO` 未定义 Bug**: L279/L314 使用未定义变量 `$MAIN_REPO`（应为 `$MAIN_REPO_WIN`），gitnexus status + --repo 参数永远失败 |
| SA3: 对抗绕过验证 | symlink → `realpath` 阻断；stash → WARN 但不阻断；`--allow-empty` → FAIL 阻断 | **架构限制**: 无 git hooks 强制 CLI 调用——agent 可绕过所有门禁直接 `git commit/push` |
| SA4: 论文偏差验证 | PRM Survey §7.3 规则脆弱性被 STAGE_3/4 API 兜底缓解 | STAGE_0/1/2/5 纯规则闸门仍最暴露 |

### 发现 E: 二轮新漏洞 (SA2)

| 漏洞 | 位置 | 严重性 | 修复 |
|:--|:--:|:--:|:--:|
| `$MAIN_REPO` 未定义变量 | cli.sh L279/L314 | 🟡 MEDIUM | 改为 `$MAIN_REPO_WIN` |
| 无 git pre-push hook 强制 CLI | 架构 | 🔴 CRITICAL | 需创建 `.git/hooks/pre-push` |

### R5.8 产出清单

| # | 事项 | 文件 | 变更量 | 状态 |
|:--:|:--|:--|:--:|:--:|
| P0 | `realpath` 符号链解析 | gatekeeper-cli.sh | +4 | ✅ |
| P0 | `git stash` 检测 | gatekeeper-cli.sh | +1 | ✅ |
| P0 | STAGE_2→3 `--allow-empty` 阻断 | gatekeeper-cli.sh | +4 | ✅ |
| P0 | Push URL 正则加固 | gatekeeper-cli.sh | +1/-1 | ✅ |
| P0 | SHA256 verify 入口调用 | gatekeeper-cli.sh | +3 | ✅ |
| P0 | 空 worktree/branch 守卫 | gatekeeper-cli.sh | +2 | ✅ |
| P1 | `$MAIN_REPO` → `$MAIN_REPO_WIN` | gatekeeper-cli.sh | +2/-2 | ✅ |
| P2 | 二轮审查验证 | 4 sub-agent | — | ✅ |

### T38: Bridge 架构文档同步

在 Bridge `2026-05-10-architecture-v4.md` 中:
- 更新 Gatekeeper 的 GitNexus 集成状态 (之前标记为 "unavailable" → 改为 "Windows Node.js 可用")
- 更新 `prm-monitor-methodology.md` §4.3 gitnexus_baseline 状态为已验证

### R5.7 产出清单

| # | 事项 | 文件 | 变更量 | 依赖 |
|:--|:--|:--|:--:|:--|
| T36 | 优化器架构文档统一 | 6 文件 | ~50 行注释+文档 | R5.6 |
| T37 | Red Flag 更新 | SKILL.md | ~15 行 | R5.6 |
| T38 | Bridge 同步 | 2 文件 | ~10 行 | R5.6 |

---

## 七.9、R5.9 — 优化器完备：Protected Regions + Scheduler + Snapshot (预估 1 天)

> **定位**: R5.5-R5.8 铸造了优化器基座（Gate+Buffer+Analysis），R5.9 补齐剩余三个组件完成 SkillOpt 完整映射。四路 SA1-SA4 子 agent 设计产出（2026-05-28），论文+源码交叉验证。
>
> **参考文献**: SkillOpt arXiv:2605.23904 §3.4-3.6 + 源码 `scheduler.py`/`slow_update.py`/`gate.py`/`skill.py`，CSO §3.2，PRM Survey §7.3

### T39: Protected Regions (SkillOpt slow_update.py L29-32 模式, +22.5 gain)

**Marker 格式**:
```bash
# PROTECTED_REGION_START::transition
# ... 受保护逻辑 ...
# PROTECTED_REGION_END::transition
```

**5 个保护区**（~440 行, 37% 脚本）:

| Region | 保护函数 | 行范围 | 为什么 |
|:--|:--|:--:|:--|
| `constants` | `MAIN_REPO_WSL`, `STATE_FILE`, `CRITICAL_STAGES` | L10-95 | 改路径=治理失效 |
| `gate` | `pre_tool_check()`, `cmd_guard_edit()` | L348-483 | 改门禁=可绕过 |
| `evidence` | `_collect_hard_evidence()` | L221-341 | 改证据=monitor 盲 |
| `transition` | `cmd_advance()`, `_write_gate_data()` | L752-985 | 改门闸=跳过阶段 |
| `appeal` | `cmd_appeal()`, `cmd_escalate()` | L987-1112 | 改上诉=无限绕过 |

**实现**: `guard_edit()` 检查目标文件是否含 `PROTECTED_REGION_START` 标记，含则 BLOCK。`pre_tool_check` 同样检查。对应 SkillOpt `skill.py` L18-28 `_is_in_slow_update_region()`。

**变更量**: ~25 行

### T40: Cosine Scheduler (SkillOpt scheduler.py L74-82 模式)

**4 模式**:

| 模式 | 行为 | 对应 SkillOpt |
|:--|:--|:--:|
| `constant` | 固定 `edit_budget_max=4`（当前行为） | L56-60 |
| `linear` | `max_lr + (min_lr - max_lr) * t` | L63-71 |
| `cosine` | `min_lr + 0.5 * (max_lr - min_lr) * (1 + cos(πt))` | L74-82 |
| `autonomous` | 999（无限制） | L85-91 |

**推荐参数**:
```
Gatekeeper: max_lr=4, min_lr=1, total_steps=6
Bridge:     max_lr=8, min_lr=2, total_steps=8
```

**向后兼容**: `gate_data.scheduler` 不存在 → `s is None` → 回退 `constant(4)`。零破坏。

**变更量**: ~27 行

### T41: Best Snapshot + Reflect (SkillOpt gate.py L31-73 模式)

**Best snapshot** 新增 `gate_data.best`:
```json
{"score":0.86,"stage":"STAGE_3","r_phase":"R5","timestamp":"..."}
```
每个 ALLOWED advance 更新。类似 SkillOpt `evaluate_gate()` 的 `best_score/best_step`。

**Reflect CLI 子命令**:
```bash
gatekeeper-cli.sh reflect <stage>
# 消费 rejected_edits + previous_rejections
# 调用 v4-flash API 生成改进建议
# 写入 <!-- REFLECT_GUIDANCE_START --> 保护区
```

**变更量**: ~65 行（snapshot 15 + reflect 50）

### R5.9 产出清单

| # | 事项 | 文件 | 变更量 | 依赖 | 设计来源 |
|:--:|:--|:--|:--:|:--|:--:|
| T39 | Protected Regions (marker + guard) | gatekeeper-cli.sh | ~25 | R5.8 | SA1 + SkillOpt slow_update.py |
| T40 | Cosine Scheduler (4 mode + .gatekeeper.state) | gatekeeper-cli.sh | ~27 | R5.8 | SA2 + SkillOpt scheduler.py |
| T41 | Best Snapshot + Reflect CLI | gatekeeper-cli.sh | ~65 | T39 | SA3 + SkillOpt gate.py |
| — | Bridge arch doc §R7.8/T75 同步 | 2026-05-10-architecture-v4.md | ~30 | R5.9 | SA4 |

---

## 七.10、R5.9++ (R5.10) — CI/CD 优化器集成 (预估 2-3 天)

> **定位**: 修复当前优化器最大的流程级漏洞——CI/CD 盲区。优化器 ①-⑧ 全部围绕代码编辑质量设计，不覆盖 CI/CD 流水线质量。spz_gatekeeper/spz 本体/spz2glb 都有大量 CI/CD 流程（6-job 矩阵、3 平台、WASM 双 profile、SHA-256 审计、回滚机制），优化器对此毫无感知：CI 触发后完全失控，失败无自动诊断，无自动修复尝试，无经验积累。本阶段将这些能力集成到 Gatekeeper CLI 优化器。
>
> **三路并行调研结论**:
> - **本地 CI/CD 现状**: 5 个 spz_gatekeeper workflow + Niantic spz 3 版本 12+6+12 矩阵 + spz2glb 回滚机制 + Bridge 245 测试/11 层安全
> - **18 篇论文 TOP-8**: LogPTR (CI 日志+代码上下文 89% 诊断率) / BuildFast (70% 修复只改 1-2 文件) / CIGAR (知识图谱 RAG +23%) / Self-HR (自动修复减少 45% 人工) / ML CI-Prediction (AUC 0.92) / DeFlaker / FlakeSync / SkillOpt
> - **12 个工具 TOP-6**: actionlint / zizmor (已有, 当前 `|| true` 静默) / scorecard / act / slsa-verifier / LLM CI 诊断 (复用 DS v4-flash API)
>
> **参考文献**: LogPTR Wang 2024, BuildFast Ma 2024 MSR, CIGAR Kim 2025 FSE, Self-HR Zhang 2025 ICSE, Chen 2022 TSE (ML prediction), DeFlaker Lam 2019 ICSE, FlakeSync Shi 2023 FSE, SkillOpt arXiv:2605.23904

### T42: Pre-Push CI Gate (STAGE_1→STAGE_2 前置门禁)

**设计**: push 前运行静态检查，阻断有风险的工作流：

1. **actionlint** — `_run_linter()` 增加 `yml|yaml` 映射到 `actionlint`，验证 workflow 语法/表达式/runner 标签/内置 shellcheck
2. **zizmor 去静默** — 当前 `ci.yml` 中 `zizmor --format plain .github/workflows/  || true` 的 `|| true` 移除，让安全扫描真正阻断 CI
3. **act dry-run** (P2) — `act -n` 本地验证 workflow，需 Docker 环境

**失败处理**: actionlint 报 error → BLOCK 不推送；zizmor 发现 HIGH 级别漏洞 → BLOCK

**变更量**: gatekeeper-cli.sh `_run_linter()` +2 行 (yml|yaml case) + ci.yml -4 行 (移除 `|| true`)

### T43: CI Failure Diagnosis (CI 失败自动诊断)

**设计**: 复用 Gatekeeper CLI 已有的 `_monitor_api` 通道 (DS v4-flash)，新增 `cmd_ci_diagnose` 子命令：

```bash
gatekeeper-cli.sh ci-diagnose <run-id>
# 1. gh run view <run-id> --log-failed → 获取失败日志
# 2. gitnexus detect-changes → 获取变更代码图
# 3. 拼接 prompt → 调用 DS v4-flash API → 输出根因分析
```

**Prompt 结构** (LogPTR 论文验证: 代码上下文提升 89% 诊断率):
```
System: You are a CI failure diagnosis expert. Analyze the CI log and code changes.
User:
<ci_log>
  [最近 200 行失败日志，按 job 分段]
</ci_log>
<code_changes>
  [GitNexus detect-changes 输出的变更符号列表]
</code_changes>
Output: {root_cause, failure_type, suggested_fix, file_scope, confidence}
```

**失败类型枚举**: `compile` / `test` / `dependency` / `flaky` / `infra` / `security`

**变更量**: gatekeeper-cli.sh +~60 行 (新命令)

### T44: Auto-Fix Loop (Self-HR 模式自动修复)

**设计**: CI 失败后，优化器进入自动修复循环：

1. `cmd_ci_diagnose` → LLM 生成候选修复 (edit_budget=4, BuildFast 70% 规则: 大多数修复只改 1-2 文件)
2. `guard-edit` → CLI 验证修复不触碰保护区
3. `act -n` (如有) → 本地验证修复语法
4. push + 重试 CI
5. 连续 3 次自动修复失败 → `escalate` 人类介入 (Self-HR 45% 减少人工)

**变更量**: gatekeeper-cli.sh +~50 行 (修复循环逻辑)

### T45: CI Failure Buffer (CIGAR 模式经验积累)

**设计**: 每次 CI 失败写入 `gate_data.ci_history[]`，形成可检索的失败经验库：

```json
{
  "ci_history": [
    {
      "run_id": 123,
      "timestamp": "2026-05-30T...",
      "failure_type": "compile",
      "root_cause": "missing zstd include path",
      "fix": "add_target_include_dirs",
      "similar_count": 2
    }
  ]
}
```

**消费路径**: 下次 `cmd_ci_diagnose` 时，检索 `ci_history` 中同类型最近 3 条失败作为 few-shot 示例注入 prompt (CIGAR RAG 模式)。

**变更量**: gatekeeper-cli.sh +~30 行 + state 文件 schema 扩展

### T46: CI Observability (Trend Monitoring)

**设计**: `cmd_ci_stats` 命令通过 `gh api` 拉最近 CI 趋势：

```bash
gatekeeper-cli.sh ci-stats [--last N]
# 输出: pass_rate, avg_duration, platform_fail_rates, trend (up/down/stable)
```

**变更量**: gatekeeper-cli.sh +~40 行

### T47: Security Gate Hardening

**设计**: 安全检查从 L7 pre-push 扩展为持续集成：

1. **zizmor 阻塞化** — 移除 `|| true` (T42 边界项)
2. **scorecard 集成** — `cmd_ci_security` 运行 OpenSSF Scorecard 18 指标
3. **SHA pin 自动化验证** — `rg "uses:.*@v[0-9]" .github/workflows/` → 未 pin 的 warn
4. **SLSA provenance 验证** (P2) — `slsa-verifier verify-artifact` 在 release 阶段

**变更量**: gatekeeper-cli.sh +~30 行 (scorecard 集成)

### T48: Pipeline 文件自检 (优化器保护)

**设计**: `.github/workflows/*.yml` 改动纳入 guard-edit 保护范围——workflow 文件是 CI/CD 的源代码，不受控则优化器自动修复的前提不成立：

1. `PROTECTED_REGION` 保护区扩展到 `github/workflows/` 目录
2. workflow 文件修改前必须调用 `guard-edit` 检查
3. `actionlint` 自动验证修改后的语法

**变更量**: gatekeeper-cli.sh +~10 行 (路径检查扩展)

### R5.9++ 产出清单

| # | 事项 | 文件 | 变更量 | 依赖 | 设计来源 |
|:--:|:--|:--|:--:|:--|:--:|
| T42 | Pre-Push CI Gate (actionlint + zizmor) | gatekeeper-cli.sh + ci.yml | ~6 | R5.9 | GHAnalyzer FSE 2023 |
| T43 | CI Failure Diagnosis | gatekeeper-cli.sh | ~60 | T42 | LogPTR + DS v4-flash API |
| T44 | Auto-Fix Loop (Self-HR) | gatekeeper-cli.sh | ~50 | T43 | BuildFast MSR 2024 |
| T45 | CI Failure Buffer (CIGAR) | gatekeeper-cli.sh + state | ~30 | T43 | CIGAR FSE 2025 |
| T46 | CI Observability | gatekeeper-cli.sh | ~40 | T42 | gh-stats 模式 |
| T47 | Security Gate Hardening | gatekeeper-cli.sh | ~30 | T42 | OSSF Scorecard |
| T48 | Pipeline 文件自检 | gatekeeper-cli.sh | ~10 | R5.9 T39 | Protected Regions 扩展 |

---

## 七.11、R5.9+++ (R5.11) — WASM 构建工程优化 (预估 1-2 天，未来冲刺)

> **定位**: R6 之后、2.0.2 发布之前的可选优化冲刺。当前 Gatekeeper WASM 是"一次验一个"的轻量验证台，与 spz2glb 的生产级 WASM（双 profile、64/128MB 初始内存、HotObjectPool、BumpAllocator、MemoryTracker）差距大。本阶段不追求 spz2glb 级别的重型工程优化，而是集成 CI/CD 工具链中的 WASM 专项检查和构建加速。
>
> **触发条件**: 当主动混沌测试或 Bridge 一体化工作台进入设计阶段时提升优先级。

### 当前 WASM CI 瓶颈

| 瓶颈 | 当前 | 目标 |
|:--|:--|:--|
| 构建耗时 | ~5-8 min (emsdk 3.1.56 完整编译) | ~3-4 min (缓存 zstd 预编译 + Emscripten cache) |
| 测试覆盖 | 单 profile, `-sASSERTIONS=0` 必开 | 双 profile (compat + perf-lite, 参照 spz_ecosystem_simplified) |
| 浏览器测试 | Playwright + HTTP server (竞态风险) | `wasm-objdump` 静态分析 + SW 缓存 SHA-256 matrix |
| 依赖管理 | FetchContent 每次 CI 重新编译 zstd | actions/cache 缓存 zstd 构建产物 + Emscripten cache |

### 计划任务 (T49-T52, 可选)

| # | 事项 | 文件 | 变更量 | 说明 |
|:--:|:--|:--|:--:|:--|
| T49 | WASM 构建缓存 (actions/cache) | ci.yml | +5 | Emscripten sysroot + zstd .a 缓存，预估节省 2-3 min/run |
| T50 | WASM 双 profile (compat + perf-lite) | ci.yml + CMakeLists.txt | +15 | 对标 spz_ecosystem_simplified，验证兼容性与性能 |
| T51 | WASM 静态分析 | ci.yml | +10 | `wasm-objdump` 检查二进制大小/分段/导入导出 |
| T52 | WASM smoke test 加固 | ci.yml | +5 | 超时兜底 + 重试 + HTTP server 健康检查 |

### R5.9+++ 依赖: R6 WASM 验证先行，R5.9++ 的 cache + actionlint 架构到位后实施。

---

## 八、R6 — 回归测试 + WASM 构建验证 (预估 1-2 天)

### 8.1 v4 专项测试 (T23)

**参照**: Niantic `tests/python/test_io.py` (pytest fixture 模式)

| 测试                        | 文件                               | 内容                                                                     |
| :------------------------ | :------------------------------- | :--------------------------------------------------------------------- |
| `v4_header_parse_test`    | 新增 `cpp/tests/v4_format_test.cc` | 合法 32B header、损坏 header（magic 错误、version<4、numStreams=0、reserved 非零）   |
| `v4_zstd_decompress_test` | 同上                               | 构建合法 v4 blob（32B header + 1 stream ZSTD），验证 `DecompressNgspStreams` 成功 |
| `v4_zstd_corrupt_test`    | 同上                               | 故意损坏 ZSTD stream 数据，验证返回 `SPZ_DECOMPRESS_ZSTD`                          |
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

**GitNexus 节奏**: R6 后 `gitnexus analyze` + `gitnexus cypher "MATCH (n) RETURN count(n)"` 全量拓扑确认（文件增删 + 调用图最终态）

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
| <br /> | T22d    | 错误码前缀 `L2_`→`SPZ_<CATEGORY>_` 标准化             | 7 文件                 |  \~50 | R5 同期       |
| **R6** | T23     | v4 专项测试 (6 类)                                      | v4\_format\_test.cc   |  +200 | R1-R5 全     |
| <br /> | T24     | `gen-fixture` v4 扩展                                | main.cc+wasm\_main.cc |  +50  | R1-R5       |
| <br /> | T25     | WASM 生产标志 `-sASSERTIONS=0` + `-sNO_EXIT_RUNTIME=1` | CMakeLists.txt        |   +2  | —           |
| <br /> | T26     | WASM 构建验证                                          | —                     |   —   | R6 全部       |
| <br /> | T27     | 版本号 bump + 文档                                      | 4 文件                  |  +30  | R6 验证后      |
| <br /> | T28     | 删除 `tlv.h`/`tlv.cc`                                | —                     | -2 文件 | T15-T22 后   |
| **R5.5** | T29   | Edit Budget (textual learning rate)                  | gatekeeper-cli.sh     |  +15  | —           |
| <br /> | T30     | Rejected-Edit Buffer                                 | gatekeeper-cli.sh + state | +20 | T29         |
| <br /> | T31     | Slow/Meta Update (跨 R-phase 记忆)                    | gatekeeper-cli.sh + refs | +30 | T30         |
| **R5.9++** | T42 | Pre-Push CI Gate (actionlint + zizmor)           | gatekeeper-cli.sh + ci.yml | ~6 | R5.9 |
| <br /> | T43 | CI Failure Diagnosis (DS v4-flash LLM 分析)      | gatekeeper-cli.sh | ~60 | T42 |
| <br /> | T44 | Auto-Fix Loop (Self-HR 模式)                     | gatekeeper-cli.sh | ~50 | T43 |
| <br /> | T45 | CI Failure Buffer (CIGAR 经验库)                 | gatekeeper-cli.sh + state | ~30 | T43 |
| <br /> | T46 | CI Observability (cmd_ci_stats)                  | gatekeeper-cli.sh | ~40 | T42 |
| <br /> | T47 | Security Gate Hardening (scorecard)              | gatekeeper-cli.sh | ~30 | T42 |
| <br /> | T48 | Pipeline 文件自检 (workflow 保护区)               | gatekeeper-cli.sh | ~10 | R5.9 T39 |
| **R5.9+++** | T49 | WASM 构建缓存 (actions/cache)                     | ci.yml | ~5 | R6 |
| <br /> | T50 | WASM 双 profile (compat + perf-lite)              | ci.yml + CMakeLists.txt | ~15 | R6 |
| <br /> | T51 | WASM 静态分析 (wasm-objdump)                      | ci.yml | ~10 | R6 |
| <br /> | T52 | WASM smoke test 加固                               | ci.yml | ~5 | R6 |

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
                                              R5.5 (Gatekeeper 治理硬化)
                                                     │
                                                     ├──► R5.8 (交叉审查硬化)
                                                     │        │
                                                     ▼        ▼
                                              R5.9 (优化器完备)
                                                     │
                                                     ▼
                                         ┌──── R5.9++ (CI/CD 优化器) ────┐
                                         │  T42 Pre-Push Gate              │
                                         │  T43 CI Diagnosis               │
                                         │  T44 Auto-Fix Loop              │
                                         │  T45 CI Failure Buffer          │
                                         │  T46 CI Observability           │
                                         │  T47 Security Gate              │
                                         │  T48 Pipeline 自检              │
                                         └──────────┬──────────────────────┘
                                                     │
                     ┌───────────────────────────────┤
                     ▼                               ▼
              R5.9+++ (WASM 工程)                R6 (测试 + WASM + 发布)
              T49 构建缓存                           T23 v4 专项测试
              T50 双 profile                         T24 gen-fixture v4
              T51 wasm-objdump                       T25 WASM 生产标志
              T52 smoke test 加固                    T26 WASM 构建验证
              (可选, 未来冲刺)                        T27 版本 bump
                                                     T28 删除旧文件
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

每 R 阶段完成后**必须**执行交叉审查 + GitNexus 分析（`analyze` + `context` 死代码检测），**禁止**跳步。

| 阶段 | 提交信息 | GitNexus | 交叉审查重点 |
| :-- | :-- | :--: | :-- |
| R1 完成 | `feat(R1): integrate libzstd — ParseToc + DecompressZstdStream + DecompressNgspStreams` | `analyze` + `context` (←新增) | ZSTD 调用链拓扑、CMake 依赖变更 |
| R2 完成 | `feat(R2): add 32B SpzHeaderV4 — ParseHeaderV4 + SpzL2Info extension + kFlagAntialiased` | `analyze` + `context` (→§4.6) | report.h 数据结构扩散 + 死代码审计 |
| R3 完成 | `feat(R3): header zone extension parsing — ParseHeaderZoneExtensions for v4 ILV records` | `analyze` + `context` | spz.cc 调用图结构变更 |
| R4 完成 | `refactor(R4): InspectSpzBlob dual-path architecture — V4 ZSTD + Legacy gzip dispatch` | `analyze` + `impact` | 入口分流对测试/CLI/WASM 的涟漪效应 |
| R5 完成 | `refactor(R5): TLV→ILV terminology alignment (16 files, -2 old)` | `analyze` + `detect-changes` | 文件重命名 + 符号变更全量确认 |
| R5.9++ 完成 | `feat(R5.9++): CI/CD optimizer — Pre-Push Gate + CI Diagnosis + Auto-Fix Loop + CI Buffer` | `analyze` + `detect-changes` | CI 诊断调用链、actionlint/zizmor 集成、修复循环 |
| R5.9+++ 完成 | `feat(R5.9+++): WASM build caching + dual profile + smoke test hardening` | `analyze` + `context` | WASM 构建缓存命中率、profile 兼容性 |
| R6 完成 | `test(R6): v4 format 6-category fixture tests + WASM build verification` | `analyze` + `cypher` | 调用图最终态无退化 |

**门禁标准**：
- `gitnexus analyze` 节点/边/集群数不得出现非预期跳变（±5% 以内为正常）
- `gitnexus context <func>` 每个新增符号 incoming 非空或有消费计划登记（spiraling dev 豁免）
- 新增符号调用链完整（无孤立节点）
- 交叉审查发现的 CR/IMP 项必须在下个 R 阶段前全部闭环

> **GitNexus 命令约定** (R2 实战定稿):
> - 统一格式: `gitnexus <cmd> -r spz_gatekeeper`（或 `--repo spz_gatekeeper`）
> - **无需 `--skip-git`**: Windows `winget` 已装 Git 2.54 → sandbox 设置 `$env:PATH = "C:\Program Files\Git\bin;$env:PATH"` 后全功能可用
> - **死代码检测**: `context` 为第一关 → `incoming: {}` 时用 `rg` 做第二步验证（`try/except ImportError` 条件导入不会被 AST 追踪）→ 确认后写入 202 plan §对应R阶段，标记 spiraling 消费目标或死代码
> - **`--repo` vs `-r`**: `detect-changes` 用 `--repo`，其余用 `-r`，均指向 `spz_gatekeeper`（3 个索引库共存：`.codex` / `spz_gatekeeper` / `spz-3.0.0`）

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
[编码] → [CTest 验证] → [git commit]
                           ↓
                    [GitNexus analyze]          ← 节点/边/集群基线
                           ↓
                    [GitNexus context <func>]   ← 死代码检测 (每个新增符号)
                           ↓
                    [交叉审查 (L1-L6)]
                           ↓
              [发现 CR/IMP → 修复 → 重新 CTest]
                           ↓
                       [下一 R]
```

**禁止的行为**：
- ❌ 跨 R 合并提交（如 "R1+R2 done" 一个 commit）
- ❌ 跳过 GitNexus `analyze` + `context` 直接进入下一 R
- ❌ `context` 返回 `incoming: {}` 时不查清消费目标就标记完成
- ❌ 交叉审查发现问题后不停下来修复

### 11.3 完成条件

- 13 个现有 C++ 测试 + 6 类 v4 专项测试全部通过
- `emcmake cmake` + `emmake make spz_gatekeeper_wasm_site` 成功
- `wasm_smoke_test.mjs` 通过
- 6 轮 `gitnexus analyze` 均无退化 (±5%)
- 6 轮 `gitnexus context` 死代码审计全部通过（无未登记孤立节点）
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

**轻型 WASM 优化（R5.9+++ §七.11）**: 不急重型优化，但 2.0.2 发布前可选执行 R5.9+++ 的 4 项轻型任务：构建缓存（-2~3min/run）、双 profile、wasm-objdump 静态分析、smoke test 加固。这些不涉及对象池/分配器等架构变更，仅 CI 配置 + CMake 微调。

**不急于现在**。开源策略和实现方案都未定型，2.0.2 聚焦 v4 格式支持 + CI/CD 优化器集成，WASM 轻型优化在 R5.9+++ 执行，重型优化留给未来冲刺。

***

**计划版本**: 1.9  
**审查状态**: 6 层交叉审查通过 (L1✅ L2✅ L3✅ L4✅ L5 N/A L6 待实施后同步)  
**GitNexus 三项目基线**: Bridge 1768节点 · Gatekeeper 1,391节点 · SPZ v3.0.0 767节点 (2026-05-24 post-R2)
> `gitnexus analyze` 统一 `-r <repo>`，PATH 含 `C:\Program Files\Git\bin`，无 `--skip-git`。  
**变更总量**: +898 行新增 / ~257 行修改 / 53 个任务 (T00-T52)
