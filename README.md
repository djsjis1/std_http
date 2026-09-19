# std_http

基于 [llhttp](https://github.com/nodejs/llhttp) 的轻量级 C++ HTTP 解析与构建库。

## 特性

### HTTP 解析 (`http_parse`)

- 流式解析：支持增量喂入数据，适合网络场景
- 自动累积解析结果（method/url/headers/body）
- 支持 keep-alive 连续消息解析
- 可配置的资源限制，防止恶意输入：
  - `body_limit`: 消息体上限（默认 8MB）
  - `url_limit`: URL 长度上限（默认 8KB）
  - `header_field_limit`: 头部字段名上限（默认 4KB）
  - `header_value_limit`: 头部值上限（默认 8KB）
  - `header_count_limit`: 头部数量上限（默认 100）
- 支持同名多值头部（如 Set-Cookie）

### HTTP 构建 (`http_protocol`)

- 一站式接口：`request()` / `json()` / `form()` / `response()`
- 链式接口：细粒度控制报文构建
- 自动补全 Content-Length
- RFC 7230 校验，防止 CRLF 头注入
- URL 编码/解码支持
- 状态码原因短语查询

## 快速开始

### 构建

```bash
cmake -B build -DSTD_HTTP_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release
```

### 解析 HTTP 请求

```cpp
#include "http_parse.h"
#include <iostream>

int main() {
    const char *request =
        "POST /api/login HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 27\r\n"
        "\r\n"
        "{\"user\":\"alice\",\"pwd\":\"123\"}";

    http_parse parser(HTTP_REQUEST);
    if (!parser.feed_all(request, strlen(request))) {
        std::cerr << "parse failed: " << parser.error() << "\n";
        return 1;
    }

    std::cout << "method: " << parser.http_method << "\n"
              << "url: " << parser.http_url << "\n"
              << "content-type: " << parser.header("content-type") << "\n"
              << "body: " << parser.http_body << "\n";
    return 0;
}
```

### 流式解析

```cpp
http_parse parser(HTTP_REQUEST);

// 分块喂入数据
parser.feed("GET /ind", 7);
parser.feed("ex.html HTTP/1.1\r\n", 18);
parser.feed("Host: example.com\r\n\r\n", 21);

std::cout << "url: " << parser.http_url << "\n";
```

### 构建 HTTP 请求

```cpp
#include "http_protocol.h"
#include <iostream>

int main() {
    // 一站式接口
    std::string request = http_protocol::json(
        "POST", "/api/data",
        R"({"key":"value"})",
        {{"Host", "example.com"}});

    std::cout << request;

    // 链式接口
    std::string custom = http_protocol()
        .request_line("PUT", "/api/user")
        .header("Host", "example.com")
        .header("Authorization", "Bearer token123")
        .body(R"({"name":"alice"})")
        .build();

    return 0;
}
```

### 构建表单请求

```cpp
std::string form = http_protocol::form(
    "POST", "/search",
    {{"q", "c++ http"}, {"page", "1"}});
// 自动 URL 编码并设置 Content-Type
```

### 同名多值头部

```cpp
http_parse parser(HTTP_RESPONSE);
parser.feed_all(response_data, len);

// 获取所有 Set-Cookie 值
auto cookies = parser.headers("Set-Cookie");
for (const auto &cookie : cookies) {
    std::cout << "cookie: " << cookie << "\n";
}
```

### URL 编解码

```cpp
// 编码
std::string encoded = http_protocol::url_encode("hello world");
// encoded = "hello%20world"

// 解码
std::string decoded = http_protocol::url_decode("hello%20world");
// decoded = "hello world"

// 表单解码（+ 表示空格）
std::string form_decoded = http_protocol::url_decode("q=c%2B%2B+http");
// form_decoded = "q=c++ http"
```

### 状态码查询

```cpp
const char *reason = http_protocol::status_text(404);
// reason = "Not Found"
```

## CMake 集成

### 作为子目录

```cmake
add_subdirectory(path/to/std_http)
target_link_libraries(your_target PRIVATE http::http)
```

### 使用安装包

```cmake
find_package(http CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE http::http)
```

## API 参考

### http_parse

| 方法 | 说明 |
| ------ | ------ |
| `feed(data, len)` | 增量喂入数据，出错返回 false |
| `feed_all(data, len)` | 解析一条完整消息 |
| `reset()` | 重置解析器状态 |
| `error()` | 获取最近一次错误信息 |
| `header(field)` | 按字段名获取头部（大小写不敏感） |
| `headers(field)` | 获取同名头部的所有值 |
| `keep_alive()` | 判断连接是否应复用 |

### http_protocol

#### 一站式接口

| 方法 | 说明 |
| ------ | ------ |
| `request(method, url, headers, body)` | 构建请求 |
| `json(method, url, body, headers)` | 构建 JSON 请求 |
| `form(method, url, fields, headers)` | 构建表单请求 |
| `response(status, headers, body)` | 构建响应 |
| `url_encode(text)` | URL 编码 |
| `url_decode(text)` | URL 解码 |
| `status_text(code)` | 获取状态码原因短语 |

#### 链式接口

| 方法 | 说明 |
| ------ | ------ |
| `request_line(method, url, version)` | 设置请求行 |
| `status_line(status, reason, version)` | 设置响应状态行 |
| `header(field, value, replace)` | 添加头部，`replace=true` 时替换同名头部 |
| `body(text)` | 设置消息体 |
| `build()` | 生成完整报文 |
| `clear()` | 清空所有内容，可复用 |

## 依赖

- C++17 编译器
- CMake 3.25+
- llhttp 9.3.0（已包含在 `third/llhttp`）

## 许可证

MIT License
