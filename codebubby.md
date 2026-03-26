# SPZ Gatekeeper 会话接续

## 当前定位
- 项目：`spz_gatekeeper_project`
- 主线定位：**L2-only SPZ validator**
- 不再承担：`check-glb` / `check-gltf` / GLB 容器校验

## 当前主线协议
- 官方扩展存在位：`0x02` (`has extensions`)
- `0x01` 保持 antialiasing 语义
- TLV：`[u32 type][u32 length][payload...]`
- 未知 TLV 类型必须可跳过
- 版本策略：
  - `version < 1` => error
  - `version 1..4` => normal
  - `version > 4` => warning，继续校验
- Adobe Safe Orbit Camera：**弧度制**
  - `minElevation/maxElevation ∈ [-pi/2, pi/2]`
  - `minRadius >= 0`
- `spz-entropy`：仅按 **vendor extension** 规划，不改 core header

## 本轮已完成
- 主线从 `0x40` 收敛到官方 `0x02`
- 版本上限从 `3` 调整到 `4`
- Adobe 扩展统一为弧度制
- `README.md` / `README-zh.md` / `docs/WIKI.md` 等已对齐到 L2-only
- 已删除旧 GLB/glTF 路线文件：
  - `cpp/include/spz_gatekeeper/gltf.h`
  - `cpp/include/spz_gatekeeper/glb.h`
  - `cpp/src/gltf.cc`
  - `cpp/src/glb.cc`
  - `cpp/include/spz_gatekeeper/base64.h`
  - `cpp/src/base64.cc`
- 已删除误导性旧草案：
  - `docs/interface_design.txt`
  - `docs/plans/2026-02-25-spz-gatekeeper.md`
- `build_wsl/` 已清理
- `CHANGELOG.md` 已重写为当前主线口径

## 当前保留项
- 保留兼容包装头（这轮用户要求不删）：
  - `cpp/extensions/adobe/safe_orbit_camera_validator.h`
  - `cpp/extensions/registry/validator_registry.h`
- 它们不是主线功能，仅是兼容层；后续若全仓引用切到 `spz_gatekeeper/...`，可再评估是否删除

## 当前验证状态
已在 WSL 虚拟环境 `/home/linuxmmlsh/.venv/hunyuan` 下通过：
```bash
cmake -S cpp -B build_wsl -DCMAKE_BUILD_TYPE=Release
cmake --build build_wsl -j
ctest --test-dir build_wsl --output-on-failure
```
结果：`9/9 tests passed`

## 备份
- 因为本仓库删除规则非常严格，删除前创建了备份：
  - `c:/Users/HP/Downloads/HunYuan3D_test_cases/backups/spz_gatekeeper_cleanup_backup_20260318_001500.tar.gz`
- 如果后续确认不需要，可再单独按删除流程处理

## 当前还没做完的事
1. **Git 收口未完成**
   - 当前仍是未提交工作区，存在 `M / D / ??` 状态
   - 新增但未入库的文件包括：
     - `cpp/include/spz_gatekeeper/safe_orbit_camera_validator.h`
     - `cpp/include/spz_gatekeeper/validator_registry.h`
     - `cpp/tests/performance_benchmark_test.cc`
     - `cpp/tests/sh_quantization_test.cc`
     - `docs/Adobe_Safe_Orbit_Camera_Spec.md`
     - `docs/Implementing_Custom_Extension.md`
     - `docs/Vendor_ID_Allocation.md`
2. **是否继续清理兼容包装头（D）**
   - 当前按用户要求保留
   - 若后续删 D，先统一 include，再删
3. **是否做版本号/发布语义调整**
   - `cpp/CMakeLists.txt` 当前仍是 `VERSION 1.1.0`
   - 若按主线重构幅度，后续可讨论是否升到 `1.2.0` 或更高

## 下一步建议
优先顺序：
1. 检查 `git status`
2. 确认哪些 `??` 文件是要正式保留的
3. 若保留，则统一纳入版本控制
4. 再决定是否调整版本号/发布信息
5. 最后再考虑是否移除兼容包装头 D

## 新会话提示词
可直接说：
> 继续处理 `spz_gatekeeper_project`。当前主线是 L2-only SPZ validator，已经切到官方 `0x02`、版本上限 `4`、Adobe 弧度制，GLB/glTF 旧路线已删除但兼容包装头 D 仍保留。先帮我检查当前 `git status` 里的未跟踪/已修改文件该怎么收口。
