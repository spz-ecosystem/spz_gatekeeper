# GitHub 发布前检查清单

## 核心范围
- [ ] 项目定位保持为 **L2-only SPZ validator**
- [ ] CLI 对外口径为 `check-spz` / `dump-trailer` / `registry` / `compat-check` / `compat-board` / `gen-fixture` / `guide` / `--self-test`
- [ ] 不对外承担 `check-glb` / `check-gltf` / GLB 容器校验
- [ ] `compat-board` 被明确描述为兼容性成熟度看板，而不是算法排行榜

## 协议一致性
- [ ] 官方扩展存在位为 `0x02`
- [ ] `0x01` 抗锯齿语义保持不变
- [ ] 未知 flags 位按可忽略处理
- [ ] TLV 结构固定为 `[u32 type][u32 length][payload...]`
- [ ] 未知 TLV 类型可按 `length` 跳过
- [ ] 版本策略为：`<1` error，`1..4` 正常，`>4` warning

## Adobe 扩展
- [ ] 文档与代码统一使用弧度制语义
- [ ] `minElevation/maxElevation` 范围为 `[-pi/2, pi/2]`
- [ ] `minRadius >= 0`
- [ ] Adobe 单元测试与集成测试已覆盖弧度制边界

## 文档一致性
- [ ] `README.md` 与 `README-zh.md` 结构和口径一致，并完整覆盖当前 CLI 命令说明
- [ ] `docs/WIKI.md` 与公开协议口径一致，`README` 承担完整 CLI guide
- [ ] `docs/extension_registry.json` 与内置 registry / compatibility board 快照一致
- [ ] `spz-entropy` 被描述为 vendor extension 规划项，而非 core header 修改

## 构建与测试
- [ ] 在 WSL 中完成配置、编译、测试
- [ ] `spz_gatekeeper_self_test` 通过
- [ ] `adobe_extension_test` 通过
- [ ] `extension_integration_test` 通过
- [ ] `registry_cli_test` / `compat_check_test` / `gen_fixture_test` 通过
- [ ] `./build/spz_gatekeeper --help` 与 README 中的命令面一致
- [ ] GitHub Pages / Emscripten 构建通过或已明确记录非本次范围

## 发布步骤
```bash
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/spz_gatekeeper --help
./build/spz_gatekeeper registry --json
./build/spz_gatekeeper compat-board --json
emcmake cmake -S cpp -B build-pages -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
emmake cmake --build build-pages --parallel
```

## 后续规划
- [ ] 若要实现 `spz-entropy`，先固定公开的 vendor/type 口径
- [ ] 先补 payload 规范，再补 validator 和 TLV 集成测试
