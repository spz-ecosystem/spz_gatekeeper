# SPZ Gatekeeper

> 仅做 L2 的 SPZ 合法性检查器，用于审计 header、flags 和 TLV trailer 扩展，同时不破坏原始 SPZ 的基础解码行为。

`spz_gatekeeper` 是一个 **纯 C++17** 的 `.spz` 校验工具。它**不**检查 GLB 容器，也**不**实现压缩、渲染或效果评测；它只负责判断一个 SPZ 文件在携带可选厂商扩展时，是否仍保持与上游 SPZ packed format 的兼容性。

## 作者与版权
- 版权所有者：**Pu Junhan**
- 起始年份：**2026**

## 个人独立作品声明
本仓库为 **Pu Junhan** 独立完成的净室实现（clean-room implementation）。SPZ 文件格式规范（原由 Niantic, Inc. 开发并发布于 https://github.com/nianticlabs/spz）仅作为实现技术互操作性的参考依据。与 "SPZ" 及 "Niantic" 相关的所有商标、商号及知识产权归其各自权利人专有。

## 组织与政治立场分离声明

### 无隶属关系
**本项目与 Niantic, Inc. 或其任何子公司、母公司、关联实体不存在任何隶属、代言或关联关系。**

作者明确且不可撤销地与 Niantic, Inc. 及其关联方所涉及的任何政治立场、言论、行动或争议划清界限。该分离声明涵盖所有政治、意识形态及组织层面的立场。

### 技术独立性
本门卫项目在独立治理框架下运作，并自行制定 SPZ 文件兼容性校验标准。本项目所实现的技术规范仅源于公开文档与独立分析。

## 治理权与衍生合规声明

### 管辖权
作为唯一创建者与维护者，**Pu Junhan** 依据本项目的开源协议主张以下权利：
1. **定义权**：定义何为合规的 SPZ 衍生项目或扩展规范的权利
2. **背书裁量权**：对任何声称 SPZ 兼容的项目给予或拒绝背书的裁量权
3. **标准演进权**：为维护技术完整性与社区标准而更新校验标准的权利

### 衍生合法性
**SPZ 衍生项目的合法性仅以其是否符合本项目发布的规范与协议为判定依据。** Niantic, Inc. 或任何其他外部实体均无权在本治理框架下确认或否定衍生项目的合法性。

### 免责声明
通过本门卫项目的校验或纳入，不构成对任何底层政治、组织或商业实体的背书。

## 工具边界

### 本工具负责
- 校验 SPZ header：magic、version、点数、SH degree、flags、reserved
- 校验 base payload 大小与截断情况
- 校验 base payload 之后的 TLV trailer
- 校验已知厂商扩展，目前内置 Adobe Safe Orbit Camera（`0xADBE0002`）
- 对更高版本 SPZ 发出 warning，并继续 best-effort 校验

### 本工具不负责
- 校验 GLB/glTF 结构
- 检查 SPZ 到 GLB 转换过程是否二进制无损
- 实现压缩、熵编码或渲染

## 主线协议口径
- 官方扩展存在位：`0x02`（`has extensions`）
- 抗锯齿位保持 `0x01`
- 未识别 flags 位按可忽略处理
- 扩展记录采用 TLV：`[u32 type][u32 length][payload...]`
- 未知 TLV 类型必须能通过 `length` 跳过
- 版本策略：
  - `version < 1` => error
  - `version 1..4` => 正常校验
  - `version > 4` => warning，但继续校验

## Web 界面

SPZ Validator 提供在线 Web 界面，无需安装即可使用：

🔗 **在线访问**: https://spz-ecosystem.github.io/spz_gatekeeper/

功能特点：
- 纯浏览器端运行，WASM 本地执行
- 拖拽上传 `.spz` 文件即时验证
- 支持 SPZ v4 格式完整性检查
- 文件不上传服务器，零隐私泄露风险

### 本地构建 Web 版本

```bash
# 在已配置 Emscripten 的 WSL 环境中执行
emcmake cmake -S cpp -B build-pages -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
emmake cmake --build build-pages --parallel

# 构建产物会保留在 build-pages/site，不污染 web/
cd build-pages/site && python3 -m http.server 8080
# 打开 http://localhost:8080
```

### v1.1.1 浏览器 smoke 基线（仅合成 fixture）

`v1.1.1` 起默认采用“合成 `.spz` 夹具”进行浏览器验收，避免真实样例的协议风险：

```bash
# 1）先构建本地 CLI（用于生成 fixture）
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --parallel

# 2）构建 WASM 站点
emcmake cmake -S cpp -B build-pages -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
emmake cmake --build build-pages --parallel

# 3）生成合成合法样例（仓库内可控路径）
./build/spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out build-pages/site/synthetic_valid.spz

# 4）启动本地服务并跑浏览器 smoke
python3 -m http.server 4173 --directory build-pages/site
node tests/wasm_smoke_test.mjs http://127.0.0.1:4173 build-pages/site/synthetic_valid.spz
```

策略说明：CI/Web smoke 默认只依赖合成 fixture；真实本地样例仅可选，不作为发布门禁必需输入。

## 快速开始

### 在 WSL/Linux/macOS 构建
```bash
git clone https://github.com/spz-ecosystem/spz_gatekeeper.git
cd spz_gatekeeper
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/spz_gatekeeper check-spz model.spz
```

### 从 Windows 通过 WSL 构建
```bash
wsl bash -lc "
  cd /path/to/spz_gatekeeper && \
  cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release && \
  cmake --build build -j && \
  ctest --test-dir build --output-on-failure
"
```

### 依赖
```bash
# Ubuntu / Debian
sudo apt-get install -y zlib1g-dev
```

## CLI

### 校验 SPZ
```bash
spz_gatekeeper check-spz <file.spz> [--strict|--no-strict] [--json]
```

### 查看已登记扩展
```bash
spz_gatekeeper registry [--json]
spz_gatekeeper registry show 0xADBE0002 [--json]
```

### 导出 trailer TLV 记录
```bash
spz_gatekeeper dump-trailer <file.spz> [--strict|--no-strict] [--json]
```

### 查看单个资产的兼容性摘要
```bash
spz_gatekeeper compat-check <file.spz> [--json]
```

### 查看兼容性成熟度看板
```bash
spz_gatekeeper compat-board [--json]
```

### 生成最小 SPZ 夹具
```bash
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out fixture.spz
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode invalid-size --out fixture_bad.spz
```

### 查看扩展开发指南
```bash
spz_gatekeeper guide [--json]
```

### 运行内置自测
```bash
spz_gatekeeper --self-test
```

### 命令分工速览
- `check-spz`：主入口的 L2 合法性校验，适合 CI、发布前检查和人工审计。
- `dump-trailer`：查看 base payload 之后的原始 TLV 记录、offset 与 length。
- `registry`：浏览内置扩展规范；需要单条契约细节时用 `registry show <type>`。
- `gen-fixture`：为扩展验证器开发生成最小合法/非法 `.spz` 样例。
- `compat-check`：对单个资产执行 strict / non-strict 的快速兼容性摘要。
- `compat-board`：查看扩展接入成熟度看板，不是算法性能排行榜。
- `guide`：输出扩展作者开发清单与治理说明。
- `--self-test`：不依赖外部资产，直接验证二进制内置假设。

## Registry、自测与兼容性看板的分工
- `registry` 是内置扩展契约的机器可读目录。
- `--self-test` 用于验证门卫自身的 header / TLV 假设，不依赖外部资产。
- `compat-board` 展示的是扩展接入成熟度，不是算法排行榜。
- `docs/extension_registry.json` 提供当前内置 registry 与 compatibility board 的文档镜像，供文档或 Web 页面复用。
- Web/WASM 与 CLI 复用同一套 registry / report 字段口径，保证终端、浏览器和文档里的 JSON 结构一致。

## 扩展作者快速自测闭环
```bash
spz_gatekeeper registry show 0xADBE0002 --json
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out fixture_valid.spz
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode invalid-size --out fixture_invalid.spz
spz_gatekeeper check-spz fixture_valid.spz --json
spz_gatekeeper dump-trailer fixture_valid.spz --json
spz_gatekeeper compat-check fixture_valid.spz --json
spz_gatekeeper compat-board --json
```

这条闭环路径依次确认：已登记契约、最小合法/非法样例、原始 TLV 布局、结构化兼容性摘要，以及当前成熟度看板状态，适合作为发布前的最短核对流程。

## 校验细节

### Header 校验
- `magic == NGSP`
- version 按上面的策略处理
- `reserved == 0`
- `sh_degree <= 4`

### Trailer 校验
- trailer 只能出现在标准 SPZ 字段之后
- 若 `flags & 0x02` 置位，则 trailer 必须存在且必须能按 TLV 解析
- 若存在 trailer 但未置 `0x02`，则发出 `L2_UNDECLARED_TRAILER` warning
- 在 `--no-strict` 模式下，TLV 解析失败降级为 warning

### Adobe 扩展
内置验证器：`0xADBE0002`（`Adobe Safe Orbit Camera`）

负载格式：
```text
float32 minElevation   // 弧度，范围 [-pi/2, pi/2]
float32 maxElevation   // 弧度，范围 [-pi/2, pi/2]
float32 minRadius      // >= 0
```

## 输出示例

### 文本
```text
asset: extended.spz
ext type=2914910210 vendor="Adobe" name="Adobe Safe Orbit Camera" valid=true
```

### JSON
```json
{
  "asset_path": "extended.spz",
  "issues": [],
  "extension_reports": [
    {
      "type": 2914910210,
      "vendor_name": "Adobe",
      "extension_name": "Adobe Safe Orbit Camera",
      "known_extension": true,
      "has_validator": true,
      "status": "stable",
      "category": "camera",
      "spec_url": "docs/Implementing_Custom_Extension.md",
      "short_description": "Constrains orbit elevation and minimum radius for safer camera control.",
      "validation_result": true,
      "error_message": ""
    }
  ],
  "spz_l2": {
    "header_ok": true,
    "version": 4,
    "num_points": 1000000,
    "sh_degree": 0,
    "flags": 2,
    "reserved": 0,
    "decompressed_size": 28000024,
    "base_payload_size": 28000000,
    "trailer_size": 24,
    "tlv_records": [
      {"type": 2914910210, "length": 12, "offset": 28000000}
    ]
  }
}
```

## 项目结构
```text
spz_gatekeeper/
├── cpp/
│   ├── include/spz_gatekeeper/
│   │   ├── extension_validator.h
│   │   ├── json_min.h
│   │   ├── report.h
│   │   ├── safe_orbit_camera_validator.h
│   │   ├── spz.h
│   │   ├── tlv.h
│   │   └── validator_registry.h
│   ├── extensions/
│   │   ├── adobe/
│   │   │   └── safe_orbit_camera_validator.h
│   │   └── registry/
│   │       └── validator_registry.h
│   ├── src/
│   │   ├── json_min.cc
│   │   ├── main.cc
│   │   ├── report.cc
│   │   ├── spz.cc
│   │   └── tlv.cc
│   ├── tests/
│   └── CMakeLists.txt
├── docs/
│   └── WIKI.md
├── README.md
├── README-zh.md
├── CHANGELOG.md
└── RELEASE_CHECKLIST.md
```

## 规划中的扩展
- `spz-entropy` 规划为 **vendor extension**，不是 core SPZ header 改造。
- 推荐厂商空间：`0x4E41`（`Niantic`），`extension_id` 暂定 `TBD`，待正式定稿。

## 相关项目
- [nianticlabs/spz](https://github.com/nianticlabs/spz) - 上游 SPZ 库
- [spz2glb](https://github.com/spz-ecosystem/spz2glb) - SPZ 到 GLB 的工具链
- [KHR_gaussian_splatting](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_gaussian_splatting) - Khronos 扩展规范

## 许可证
MIT，详见 `LICENSE`。
