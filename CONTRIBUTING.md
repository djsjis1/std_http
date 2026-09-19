# 贡献指南

感谢你对 std_http 项目的关注！我们欢迎各种形式的贡献。

## 如何贡献

### 报告 Bug

1. 在 GitHub Issues 中创建问题
2. 描述问题现象、复现步骤和期望行为
3. 包含环境信息（操作系统、编译器版本、CMake 版本）

### 提交代码

1. Fork 本仓库
2. 创建特性分支：`git checkout -b feature/your-feature`
3. 编写代码和测试
4. 确保所有测试通过：`cmake --build build && ctest --test-dir build`
5. 提交更改：`git commit -m "Add your feature"`
6. 推送到分支：`git push origin feature/your-feature`
7. 创建 Pull Request

## 代码风格

- 使用 4 空格缩进
- 遵循现有代码风格和命名约定
- 所有公开 API 必须有中文注释
- 新增功能必须附带单元测试

## 构建与测试

```bash
# 配置构建
cmake -B build -DSTD_HTTP_BUILD_TESTS=ON -DSTD_HTTP_BUILD_EXAMPLES=ON

# 编译
cmake --build build

# 运行测试
ctest --test-dir build --output-on-failure
```

## 提交规范

提交信息应简洁明了，描述所做的更改：

- `feat: 新增功能描述`
- `fix: 修复问题描述`
- `docs: 文档更新`
- `test: 测试相关更改`
- `refactor: 重构（无功能变更）`

## 行为准则

本项目遵循 Contributor Covenant 行为准则。参与贡献即表示你同意遵守该准则，共同营造友好、包容的社区环境。
