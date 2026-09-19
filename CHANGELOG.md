# 更新日志

本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/) 规范。

## [1.0.0] - 2026-09-19

### 新增

- HTTP 解析器 (`http_parse`)
  - 流式解析：支持增量喂入数据
  - 自动累积解析结果（method/url/headers/body）
  - 支持 keep-alive 连续消息解析
  - 可配置的资源限制（body/url/header 大小和数量）
  - 支持同名多值头部（如 Set-Cookie）

- HTTP 构建器 (`http_protocol`)
  - 一站式接口：`request()` / `json()` / `form()` / `response()`
  - 链式接口：细粒度控制报文构建
  - 自动补全 Content-Length
  - RFC 7230 校验，防止 CRLF 头注入
  - URL 编码/解码支持
  - 状态码原因短语查询

### 基础设施

- CI/CD：GitHub Actions 支持 Windows/Linux/macOS 多平台构建
- Sanitizer 检测：AddressSanitizer 和 UndefinedBehaviorSanitizer
- 完整的单元测试覆盖
- MIT 许可证
