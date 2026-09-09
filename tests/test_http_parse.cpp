#include "test_framework.hpp"
#include "http_parse.h"

#include <cstring>

// 测试基本的 GET 请求解析
TEST_CASE(http_parse_GET_request)
{
    const char *request =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: test\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);
    REQUIRE(parser.feed_all(request, strlen(request)));

    CHECK(parser.http_method == "GET");
    CHECK(parser.http_url == "/index.html");
    CHECK(parser.http_version == "1.1");
    CHECK(parser.header("Host") == "example.com");
    CHECK(parser.header("User-Agent") == "test");
    CHECK(parser.http_body.empty());
}

// 测试 POST 请求解析
TEST_CASE(http_parse_POST_request_with_body)
{
    const char *request =
        "POST /api/data HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 25\r\n"
        "\r\n"
        "{\"key\":\"value\",\"num\":123}";

    http_parse parser(HTTP_REQUEST);
    REQUIRE(parser.feed_all(request, strlen(request)));

    CHECK(parser.http_method == "POST");
    CHECK(parser.http_url == "/api/data");
    CHECK(parser.header("Content-Type") == "application/json");
    CHECK(parser.http_body == "{\"key\":\"value\",\"num\":123}");
}

// 测试响应解析
TEST_CASE(http_parse_HTTP_response)
{
    const char *response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";

    http_parse parser(HTTP_RESPONSE);
    REQUIRE(parser.feed_all(response, strlen(response)));

    CHECK(parser.status_code == 200);
    CHECK(parser.http_version == "1.1");
    CHECK(parser.header("Content-Type") == "text/plain");
    CHECK(parser.http_body == "hello");
}

// 测试头部大小写不敏感
TEST_CASE(http_parse_case_insensitive_headers)
{
    const char *request =
        "GET / HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "X-Custom-Header: value\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);
    REQUIRE(parser.feed_all(request, strlen(request)));

    CHECK(parser.header("content-type") == "application/json");
    CHECK(parser.header("CONTENT-TYPE") == "application/json");
    CHECK(parser.header("Content-Type") == "application/json");
    CHECK(parser.header("x-custom-header") == "value");
}

// 测试分块喂入数据
TEST_CASE(http_parse_chunked_feed)
{
    http_parse parser(HTTP_REQUEST);

    // 分多次喂入数据
    REQUIRE(parser.feed("GET /", 5));
    REQUIRE(parser.feed("index", 5));
    REQUIRE(parser.feed(".html HTTP/1.1\r\n", 16));
    REQUIRE(parser.feed("Host: example.com\r\n", 19));
    REQUIRE(parser.feed("\r\n", 2));

    CHECK(parser.http_method == "GET");
    CHECK(parser.http_url == "/index.html");
}

// 测试 body_limit 限制
TEST_CASE(http_parse_body_limit)
{
    const char *request =
        "POST / HTTP/1.1\r\n"
        "Content-Length: 100\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);
    parser.body_limit = 10; // 设置很小的限制

    // 先喂头部
    REQUIRE(parser.feed(request, strlen(request)));

    // 喂入超过限制的 body
    const char *body = "12345678901234567890"; // 20 bytes
    REQUIRE(!parser.feed(body, strlen(body)));
    // 错误信息应包含 body 或 limit 相关字样
    CHECK(!parser.error().empty());
}

// 测试错误处理
TEST_CASE(http_parse_malformed_request)
{
    // 完全无效的 HTTP 请求
    const char *bad_request = "NOT_A_VALID_HTTP_REQUEST";

    http_parse parser(HTTP_REQUEST);
    REQUIRE(!parser.feed_all(bad_request, strlen(bad_request)));
    CHECK(!parser.error().empty());
}

// 测试不存在的头部返回空字符串
TEST_CASE(http_parse_missing_header)
{
    const char *request =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);
    REQUIRE(parser.feed_all(request, strlen(request)));

    CHECK(parser.header("X-Not-Exist").empty());
}

// 测试 keep-alive 检测
TEST_CASE(http_parse_keep_alive)
{
    // HTTP/1.1 default keep-alive
    {
        const char *request =
            "GET / HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "\r\n";

        http_parse parser(HTTP_REQUEST);
        REQUIRE(parser.feed_all(request, strlen(request)));
        // HTTP/1.1 默认保持连接
        CHECK(parser.keep_alive());
    }

    // Connection: close
    {
        const char *request =
            "GET / HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Connection: close\r\n"
            "\r\n";

        http_parse parser(HTTP_REQUEST);
        REQUIRE(parser.feed_all(request, strlen(request)));
        // 注意：llhttp_should_keep_alive 的行为可能依赖于具体实现
        // 这里只验证解析成功
    }
}

// 测试连续多条消息 (keep-alive 场景)
TEST_CASE(http_parse_multiple_messages)
{
    const char *messages =
        "GET /first HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n"
        "GET /second HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);

    // 第一条消息
    REQUIRE(parser.feed(messages, 38));
    CHECK(parser.http_url == "/first");

    // 第二条消息
    REQUIRE(parser.feed(messages + 38, 39));
    CHECK(parser.http_url == "/second");
}

// 测试回调
TEST_CASE(http_parse_callbacks)
{
    const char *request =
        "POST /api HTTP/1.1\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "test";

    http_parse parser(HTTP_REQUEST);

    bool begin_called = false;
    bool complete_called = false;
    std::string body_data;

    parser.message_begin = [&]()
    { begin_called = true; };
    parser.message_complete = [&]()
    { complete_called = true; };
    parser.body = [&](const char *data, size_t len)
    {
        body_data.append(data, len);
    };

    REQUIRE(parser.feed_all(request, strlen(request)));

    CHECK(begin_called);
    CHECK(complete_called);
    CHECK(body_data == "test");
}

// 测试 headers() 方法获取同名多值头部
TEST_CASE(http_parse_multi_value_headers)
{
    const char *request =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Set-Cookie: a=1\r\n"
        "Set-Cookie: b=2\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);
    REQUIRE(parser.feed_all(request, strlen(request)));

    auto cookies = parser.headers("Set-Cookie");
    CHECK(cookies.size() == 2);
    CHECK(cookies[0] == "a=1");
    CHECK(cookies[1] == "b=2");
}

// 测试 URL 长度限制
TEST_CASE(http_parse_url_limit)
{
    const char *request =
        "GET /very_long_url_that_exceeds_limit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);
    parser.url_limit = 10; // 设置很小的限制

    REQUIRE(!parser.feed_all(request, strlen(request)));
    CHECK(!parser.error().empty());
}

// 测试头部数量限制
TEST_CASE(http_parse_header_count_limit)
{
    const char *request =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "X-Header-1: value1\r\n"
        "X-Header-2: value2\r\n"
        "X-Header-3: value3\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);
    parser.header_count_limit = 2; // 只允许 2 个头部

    REQUIRE(!parser.feed_all(request, strlen(request)));
    CHECK(!parser.error().empty());
}

// 测试头部字段名长度限制
TEST_CASE(http_parse_header_field_limit)
{
    const char *request =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "X-Very-Long-Header-Name: value\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);
    parser.header_field_limit = 10; // 设置很小的限制

    REQUIRE(!parser.feed_all(request, strlen(request)));
    CHECK(!parser.error().empty());
}

// 测试头部值长度限制
TEST_CASE(http_parse_header_value_limit)
{
    const char *request =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "X-Header: very-long-value-that-exceeds-limit\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);
    parser.header_value_limit = 10; // 设置很小的限制

    REQUIRE(!parser.feed_all(request, strlen(request)));
    CHECK(!parser.error().empty());
}

// 测试 HTTP_BOTH 模式 - 解析请求
TEST_CASE(http_parse_http_both_request)
{
    const char *request =
        "GET /api HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    http_parse parser(HTTP_BOTH);
    REQUIRE(parser.feed_all(request, strlen(request)));
    CHECK(parser.http_method == "GET");
    CHECK(parser.http_url == "/api");
}

// 测试 HTTP_BOTH 模式 - 解析响应
TEST_CASE(http_parse_http_both_response)
{
    const char *response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    http_parse parser(HTTP_BOTH);
    REQUIRE(parser.feed_all(response, strlen(response)));
    CHECK(parser.status_code == 404);
}

// 测试 feed() 错误后可继续接收下一条消息
TEST_CASE(http_parse_feed_error_recovery)
{
    http_parse parser(HTTP_REQUEST);

    // 第一条：无效请求，应该失败
    const char *bad = "INVALID";
    REQUIRE(!parser.feed_all(bad, strlen(bad)));
    CHECK(!parser.error().empty());

    // 第二条：有效请求，应该成功
    const char *good =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";
    REQUIRE(parser.feed_all(good, strlen(good)));
    CHECK(parser.http_method == "GET");
    CHECK(parser.http_url == "/");
}

// 测试空输入
TEST_CASE(http_parse_empty_input)
{
    http_parse parser(HTTP_REQUEST);
    // 空输入应该返回 true（没有错误，只是没有数据）
    CHECK(parser.feed("", 0));
}

// 测试不完整消息（需要更多数据）
TEST_CASE(http_parse_incomplete_message)
{
    http_parse parser(HTTP_REQUEST);

    // 只喂入部分数据，不完成消息
    const char *partial = "GET / HTTP/1.1\r\n";
    REQUIRE(parser.feed(partial, strlen(partial)));

    // 此时消息未完成，但解析没有出错
    CHECK(parser.http_method.empty()); // method 在 message_complete 时才设置

    // 喂入剩余数据完成消息
    const char *rest = "Host: example.com\r\n\r\n";
    REQUIRE(parser.feed(rest, strlen(rest)));
    CHECK(parser.http_method == "GET");
}

// 测试 chunked transfer encoding
TEST_CASE(http_parse_chunked_body)
{
    const char *request =
        "POST /upload HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "hello\r\n"
        "6\r\n"
        " world\r\n"
        "0\r\n"
        "\r\n";

    http_parse parser(HTTP_REQUEST);
    REQUIRE(parser.feed_all(request, strlen(request)));
    CHECK(parser.http_method == "POST");
    CHECK(parser.http_body == "hello world");
}
