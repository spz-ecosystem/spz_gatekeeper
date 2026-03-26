# Implementing Custom Extension / 自定义扩展实现

## Scope / 范围
`spz_gatekeeper` only audits `SPZ`. It does not audit `GLB` or `spz2glb`.
`spz_gatekeeper` 只审 `SPZ`，不审 `GLB`，也不审 `spz2glb`。

Browser UI is only for `browser_lightweight_wasm_audit` on a standard zip audit bundle.
Web 界面只承担标准 zip 审查包的 `browser_lightweight_wasm_audit`。

Real artifact approval always comes from `local_cli_spz_artifact_audit` on `.spz` outputs.
真实成品是否可放行，始终以面向 `.spz` 产物的 `local_cli_spz_artifact_audit` 为准。

Optional browser evidence can be exported as `browser_to_cli_handoff`, but it never replaces CLI audit.
浏览器证据可以可选导出为 `browser_to_cli_handoff`，但不能替代 CLI 成品审查。

## Step 1: Define Type / 定义类型
Use `type = (vendor_id << 16) | extension_id`.
使用 `type = (vendor_id << 16) | extension_id`。

## Step 2: Implement Validator / 实现验证器
Inherit `SpzExtensionValidator` and implement:
继承 `SpzExtensionValidator` 并实现：
- `GetName()`
- `GetExtensionType()`
- `Validate(const uint8_t* data, size_t size, std::string* error)`

## Step 3: Register Validator / 注册验证器
```cpp
auto validator = std::make_shared<MyValidator>();
spz_gatekeeper::ExtensionValidatorRegistry::Instance().RegisterValidator(
  validator->GetExtensionType(), validator);
```

or auto registration / 或自动注册：
```cpp
static spz_gatekeeper::RegisterValidator<MyValidator> g_reg;
```

## Step 4: Encode TLV / 写入 TLV
Append after base payload:
`[u32 type][u32 length][value bytes]`

在标准 payload 后追加：
`[u32 type][u32 length][value bytes]`

## Step 5: Verify / 验证
```bash
spz_gatekeeper registry show 0xADBE0002 --json
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out fixture.spz
spz_gatekeeper gen-fixture --type 0xADBE0002 --mode invalid-size --out fixture_bad.spz
spz_gatekeeper check-spz fixture.spz --json
spz_gatekeeper dump-trailer fixture.spz --json
spz_gatekeeper compat-check fixture.spz --json
spz_gatekeeper compat-check fixture.spz --handoff browser_audit.json --json
spz_gatekeeper compat-check --dir ./fixtures --json
spz_gatekeeper compat-check --manifest ./fixtures/manifest.json --json
spz_gatekeeper compat-board --json
```

- Use `registry` to confirm the spec contract is visible before sharing assets.
- Use `gen-fixture` to create the shortest possible positive / negative samples.
- Use `compat-check` for real `.spz` artifact evidence.
- Use `--handoff` only as an optional merge of browser-side evidence into CLI JSON output.
- Final release decisions still come from CLI.

- 在分享资产前，先用 `registry` 确认规范条目已可见。
- 用 `gen-fixture` 生成最短路径的正例 / 负例样本。
- 用 `compat-check` 获取真实 `.spz` 成品证据。
- `--handoff` 仅用于可选地把浏览器证据并入 CLI JSON 输出。
- 最终放行结论仍以 CLI 为准。

## Recommended local flow / 推荐本地流程
1. Browser: run `browser_lightweight_wasm_audit` on one standard zip audit bundle.
2. Optional: export `browser_to_cli_handoff`.
3. CLI: run `local_cli_spz_artifact_audit` on the real `.spz` artifact.
4. Merge handoff only when you need a combined evidence chain.

1. 浏览器端先对标准 zip 审查包执行 `browser_lightweight_wasm_audit`。
2. 如有需要，可导出 `browser_to_cli_handoff`。
3. CLI 再对真实 `.spz` 产物执行 `local_cli_spz_artifact_audit`。
4. 仅在需要双段证据链时并入 handoff。

## Planned vendor-extension route / 规划中的 vendor-extension 路线
`spz-entropy` should follow the vendor-extension path instead of changing the core SPZ header.
`spz-entropy` 应沿 vendor extension 路线实现，而不是修改 core SPZ header。

Recommended placeholder / 推荐占位：
- Vendor: `0x4E41` (`Niantic`)
- Extension ID: `TBD`

## Minimal Test Checklist / 最小测试清单
- Valid payload passes
- Invalid size fails
- Boundary values validated
- Unknown type can be skipped
- Browser bundle verdict does not replace CLI artifact audit
- `browser_to_cli_handoff` remains optional

- 有效负载通过
- 错误长度失败
- 边界值正确校验
- 未知类型可跳过
- 浏览器 bundle 结论不能替代 CLI 成品审查
- `browser_to_cli_handoff` 必须保持可选
