# 更新日志

本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/) 规范。

## [1.1.0] - 2026-10-26

### 修复

- **解析**：无 Content-Length 的响应增量 `feed()` 时不再提前/重复触发 `message_complete`；`message_complete` 的补发移至新增的 `finish()`（连接关闭时调用）。头部跨数据块到达时也不再误报 `Invalid EOF`
- **解析**：101 Switching Protocols / CONNECT 合法消息不再误报为解析失败（`HPE_PAUSED_UPGRADE` 不按错误处理）
- **解析**：解析中途调小 limit 后限制继续生效（修复 `limit - size()` 的 size_t 下溢使检查失效）
- **解析**：`feed_all()` 对不完整的输入现在返回 false（此前静默返回 true）
- **构建**：已有 `Transfer-Encoding` 头时不再叠加 `Content-Length`（RFC 7230 禁止 TE + CL 并存）
- **构建**：链式 `body("")` 显式空消息体现在输出 `Content-Length: 0`（此前 `has_body_` 被短路，导致服务器等待消息体挂起）
- **构建**：头部值/请求目标中拒绝全部控制字符（CTL/DEL，HTAB 除外），而不仅是 CR/LF/NUL
- **CMake**：使用已安装 llhttp 时正确探测 `llhttp::llhttp_static` / `llhttp::llhttp_shared` 目标名，不再因 ALIAS 缺失导致配置失败

### 新增

- 解析器 `finish()`：连接关闭(EOF)时结束依赖 EOF 的消息并补发完成事件
- 解析器 `is_upgrade()` / `resume_after_upgrade()`：协议升级(101/CONNECT)识别与恢复
- 解析器 `headers_complete` 回调：头部接收完毕时 `method/url/version/headers/status_code` 即已就绪
- 解析器 `chunk_header` / `chunk_complete` 回调：chunked 编码的 chunk 边界
- CI 增加 install 冒烟测试（安装后通过 `find_package(http)` 消费验证）

### 变更

- `method` / `status_code` / `version` 的赋值时机从 `message_complete` 提前到 `headers_complete`
- `url_encode` 不再依赖全局 locale（显式 ASCII 判断）
- 移除 `http_utils::iequals` 冗余的 `(const std::string&, const char*)` 重载（string_view 版本已覆盖）

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
