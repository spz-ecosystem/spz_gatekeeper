# 贡献指南

欢迎参与 SPZ Gatekeeper 项目的开发！本文档将帮助你快速上手贡献流程。

## 🚀 快速开始

### 1. Fork 项目

在 GitHub 上点击 "Fork" 按钮创建你自己的副本。

### 2. 克隆仓库

```bash
git clone https://github.com/spz-ecosystem/spz_gatekeeper.git
cd spz_gatekeeper
```

### 3. 创建开发分支

```bash
git checkout -b feature/your-feature-name
```

## 🛠️ 开发环境搭建

### 环境要求

- **编译器**：GCC 7+ / Clang 5+ / MSVC 2017+
- **CMake**：3.16+
- **依赖**：zlib

### 构建项目

```bash
# 配置
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Debug

# 编译
cmake --build build --parallel

# 运行测试
ctest --test-dir build --output-on-failure
```

## 📝 贡献流程

### 1. 选择 Issue

查看 [GitHub Issues](https://github.com/spz-ecosystem/spz_gatekeeper/issues) 寻找可以贡献的任务：
- `good first issue` - 适合新手
- `help wanted` - 需要帮助
- `bug` - 修复 bug
- `enhancement` - 新功能

### 2. 开发规范

#### 代码风格

- 遵循 Google C++ Style Guide
- 使用 2 空格缩进
- 函数命名使用驼峰式（CamelCase）
- 变量命名使用小写 + 下划线（snake_case）
- 添加必要的注释（尤其是公开 API）

#### 提交规范

Commit message 格式：
```
<type>(<scope>): <subject>

<body>

<footer>
```

Type 类型：
- `feat`: 新功能
- `fix`: Bug 修复
- `docs`: 文档更新
- `style`: 代码格式调整
- `refactor`: 重构
- `test`: 测试相关
- `chore`: 构建/工具相关

示例：
```
feat(spz): add TLV record validation

- Add type range checking for TLV records
- Improve error messages for invalid TLV data
- Add unit tests for edge cases

Closes #123
```

### 3. 编写测试

所有新功能必须包含测试：
- 在 `cpp/tests/` 添加测试文件
- 确保测试覆盖正常路径和错误路径
- 运行 `ctest` 验证所有测试通过

### 4. 提交 Pull Request

1. 推送分支到你的 fork：
   ```bash
   git push origin feature/your-feature-name
   ```

2. 在 GitHub 上创建 Pull Request
3. 填写 PR 模板，说明：
   - 变更目的
   - 实现方案
   - 测试情况
   - 相关 Issue

### 5. Code Review

- 维护者会在 48 小时内 review
- 根据反馈及时修改
- 所有讨论需保持礼貌和专业

## 📚 开发指南

### 添加新的 L2 检查

1. 在 `cpp/include/spz_gatekeeper/spz.h` 定义错误码
2. 在 `cpp/src/spz.cc` 实现检查逻辑
3. 在 `cpp/tests/spz_gatekeeper_test.cc` 添加测试
4. 更新 `README.md` 文档

### 添加新的 TLV 类型

1. 先确定 `vendor_id` 和 `extension_id`，再按 `type = (vendor_id << 16) | extension_id` 组合完整 type
2. 在公开文档中说明 payload/value 格式
3. 确保向后兼容（未知 type 可按 `length` 跳过）

### 调试技巧

```bash
# 使用 Debug 模式构建
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Debug

# 使用 GDB 调试
gdb --args build/spz_gatekeeper check-spz test.spz

# 使用 Valgrind 检查内存
valgrind --leak-check=full build/spz_gatekeeper check-spz test.spz
```

## 🧪 测试要求

### 单元测试

每个核心模块都应有单元测试：
- 正常输入测试
- 边界条件测试
- 错误输入测试

### 集成测试

测试完整的检查流程：
- 准备测试数据集
- 验证输出结果
- 检查性能指标

### 运行所有测试

```bash
cd build
ctest --output-on-failure
```

## 📖 文档贡献

- 代码注释使用 Doxygen 格式
- 更新 README.md 相关章节
- 添加使用示例
- 保持中英文文档同步（如可能）

## ❓ 常见问题

### Q: 如何报告 Bug？

A: 在 GitHub Issues 创建 issue，选择 "Bug Report" 模板，提供：
- 复现步骤
- 预期行为
- 实际行为
- 环境信息

### Q: 如何提议新功能？

A: 在 GitHub Issues 创建 issue，选择 "Feature Request" 模板，说明：
- 功能描述
- 使用场景
- 实现建议

### Q: 代码许可是什么？

A: 项目采用 MIT License，你的贡献也将使用此许可。

## 🎯 当前需求

我们特别需要以下方面的贡献：

1. **测试用例**：增加边界条件、错误注入测试
2. **文档**：完善 API 文档、使用示例
3. **性能优化**：提升大文件处理速度
4. **新特性**：支持更多 SPZ 扩展字段

## 📞 联系方式

- GitHub Issues: 提问和讨论
- Pull Request / Issue 线程：继续补充上下文与 review 反馈

感谢你的贡献！
