#include "test_framework.hpp"
#include "http_protocol.h"
#include "http_parse.h"

#include <stdexcept>

// 测试基本 GET 请求构建
TEST_CASE(http_protocol_GET_request)
{
    std::string result = http_protocol::request(
        "GET", "/index.html",
        {{"Host", "example.com"}, {"User-Agent", "test"}});

    CHECK_THAT(result, ContainsSubstring("GET /index.html HTTP/1.1\r\n"));
    CHECK_THAT(result, ContainsSubstring("Host: example.com\r\n"));
    CHECK_THAT(result, ContainsSubstring("User-Agent: test\r\n"));
    CHECK_THAT(result, ContainsSubstring("\r\n\r\n"));
}

// 测试 POST 请求带 body
TEST_CASE(http_protocol_POST_request_with_body)
{
    std::string result = http_protocol::request(
        "POST", "/api/data",
        {{"Host", "example.com"}, {"Content-Type", "application/json"}},
        "{\"key\":\"value\"}");

    CHECK_THAT(result, ContainsSubstring("POST /api/data HTTP/1.1\r\n"));
    CHECK_THAT(result, ContainsSubstring("Content-Length: 15\r\n"));
    CHECK_THAT(result, ContainsSubstring("{\"key\":\"value\"}"));
}

// 测试 JSON 请求自动添加 Content-Type
TEST_CASE(http_protocol_JSON_request)
{
    std::string result = http_protocol::json(
        "POST", "/api",
        "{\"data\":123}",
        {{"Host", "example.com"}});

    CHECK_THAT(result, ContainsSubstring("Content-Type: application/json\r\n"));
    CHECK_THAT(result, ContainsSubstring("Content-Length: 12\r\n"));
}

// 测试 JSON 请求不覆盖已有的 Content-Type
TEST_CASE(http_protocol_JSON_request_preserves_Content_Type)
{
    std::string result = http_protocol::json(
        "POST", "/api",
        "{\"data\":123}",
        {{"Host", "example.com"}, {"Content-Type", "application/json; charset=utf-8"}});

    CHECK_THAT(result, ContainsSubstring("Content-Type: application/json; charset=utf-8\r\n"));
}

// 测试表单请求
TEST_CASE(http_protocol_form_request)
{
    std::string result = http_protocol::form(
        "POST", "/search",
        {{"q", "c++ http"}, {"page", "1"}});

    CHECK_THAT(result, ContainsSubstring("Content-Type: application/x-www-form-urlencoded\r\n"));
    CHECK_THAT(result, ContainsSubstring("q=c%2B%2B%20http"));
    CHECK_THAT(result, ContainsSubstring("page=1"));
}

// 测试响应构建
TEST_CASE(http_protocol_response)
{
    std::string result = http_protocol::response(
        200,
        {{"Content-Type", "text/plain"}},
        "hello world");

    CHECK_THAT(result, ContainsSubstring("HTTP/1.1 200 OK\r\n"));
    CHECK_THAT(result, ContainsSubstring("Content-Type: text/plain\r\n"));
    CHECK_THAT(result, ContainsSubstring("Content-Length: 11\r\n"));
    CHECK_THAT(result, ContainsSubstring("hello world"));
}

// 测试自定义 reason
TEST_CASE(http_protocol_response_custom_reason)
{
    std::string result = http_protocol::response(
        200, {}, "ok", "Custom Reason");

    CHECK_THAT(result, ContainsSubstring("HTTP/1.1 200 Custom Reason\r\n"));
}

// 测试 URL 编码
TEST_CASE(http_protocol_url_encode)
{
    CHECK(http_protocol::url_encode("hello world") == "hello%20world");
    CHECK(http_protocol::url_encode("c++ http") == "c%2B%2B%20http");
    CHECK(http_protocol::url_encode("test@example.com") == "test%40example.com");
    CHECK(http_protocol::url_encode("a-b_c.d~e") == "a-b_c.d~e");     // 未保留字符不变
    CHECK(http_protocol::url_encode("中文") == "%E4%B8%AD%E6%96%87"); // UTF-8
}

// 测试链式接口
TEST_CASE(http_protocol_chain_API)
{
    std::string result = http_protocol()
                             .request_line("PUT", "/api/user")
                             .header("Host", "example.com")
                             .header("Content-Type", "application/json")
                             .body("{\"name\":\"alice\"}")
                             .build();

    CHECK_THAT(result, ContainsSubstring("PUT /api/user HTTP/1.1\r\n"));
    CHECK_THAT(result, ContainsSubstring("Host: example.com\r\n"));
    CHECK_THAT(result, ContainsSubstring("{\"name\":\"alice\"}"));
}

// 测试 header replace
TEST_CASE(http_protocol_header_replace)
{
    std::string result = http_protocol()
                             .request_line("GET", "/")
                             .header("Host", "old.com")
                             .header("Host", "new.com", true) // replace
                             .build();

    CHECK_THAT(result, ContainsSubstring("Host: new.com\r\n"));
}

// 测试 CRLF 注入防护
TEST_CASE(http_protocol_CRLF_injection_prevention)
{
    // header field with CRLF
    bool caught = false;
    try
    {
        http_protocol::request("GET", "/", {{"Host\r\nX-Injected", "evil"}});
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);

    // header value with CRLF
    caught = false;
    try
    {
        http_protocol::request("GET", "/", {{"Host", "example.com\r\nX-Injected: evil"}});
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);

    // url with CRLF
    caught = false;
    try
    {
        http_protocol::request("GET", "/path\r\nX-Injected: evil");
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);

    // method with invalid char
    caught = false;
    try
    {
        http_protocol::request("GET\r\n", "/");
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);
}

// 测试无效输入
TEST_CASE(http_protocol_invalid_input)
{
    // empty method
    bool caught = false;
    try
    {
        http_protocol::request("", "/");
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);

    // empty url
    caught = false;
    try
    {
        http_protocol::request("GET", "");
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);

    // invalid version
    caught = false;
    try
    {
        http_protocol::request("GET", "/", {}, "", "HTTP/");
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);

    // invalid status code
    caught = false;
    try
    {
        http_protocol::response(9999);
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);
}

// 测试空 body 不添加 Content-Length
TEST_CASE(http_protocol_no_Content_Length_for_empty_body)
{
    std::string result = http_protocol::request(
        "GET", "/",
        {{"Host", "example.com"}});

    CHECK(result.find("Content-Length:") == std::string::npos);
}

// 测试常见状态码的默认 reason
TEST_CASE(http_protocol_default_reason_phrases)
{
    CHECK_THAT(http_protocol::response(200), ContainsSubstring("200 OK"));
    CHECK_THAT(http_protocol::response(404), ContainsSubstring("404 Not Found"));
    CHECK_THAT(http_protocol::response(500), ContainsSubstring("500 Internal Server Error"));
    CHECK_THAT(http_protocol::response(301), ContainsSubstring("301 Moved Permanently"));
}

// 测试 clear 方法
TEST_CASE(http_protocol_clear)
{
    http_protocol p;
    p.request_line("GET", "/first");
    p.header("Host", "example.com");
    p.body("data");

    p.clear();

    p.request_line("GET", "/second");
    std::string result = p.build();

    CHECK_THAT(result, ContainsSubstring("/second"));
    CHECK(result.find("/first") == std::string::npos);
    CHECK(result.find("data") == std::string::npos);
}

// 测试生成和解析回环
TEST_CASE(http_protocol_round_trip_with_parser)
{
    std::string request_text = http_protocol::json(
        "POST", "/api/test",
        "{\"value\":42}",
        {{"Host", "localhost"}});

    http_parse parser(HTTP_REQUEST);
    REQUIRE(parser.feed_all(request_text.data(), request_text.size()));

    CHECK(parser.http_method == "POST");
    CHECK(parser.http_url == "/api/test");
    CHECK(parser.header("Content-Type") == "application/json");
    CHECK(parser.http_body == "{\"value\":42}");
}

// 测试 URL 解码
TEST_CASE(http_protocol_url_decode)
{
    CHECK(http_protocol::url_decode("hello%20world") == "hello world");
    CHECK(http_protocol::url_decode("c%2B%2B%20http") == "c++ http");
    CHECK(http_protocol::url_decode("test%40example.com") == "test@example.com");
    CHECK(http_protocol::url_decode("a-b_c.d~e") == "a-b_c.d~e");     // 未编码字符不变
    CHECK(http_protocol::url_decode("%E4%B8%AD%E6%96%87") == "中文"); // UTF-8
    CHECK(http_protocol::url_decode("hello+world") == "hello world"); // + 表示空格
}

// 测试 status_text
TEST_CASE(http_protocol_status_text)
{
    CHECK(std::string(http_protocol::status_text(200)) == "OK");
    CHECK(std::string(http_protocol::status_text(404)) == "Not Found");
    CHECK(std::string(http_protocol::status_text(500)) == "Internal Server Error");
    CHECK(std::string(http_protocol::status_text(9999)) == ""); // 未知状态码
}

// 测试空 body 不添加 Content-Length
TEST_CASE(http_protocol_empty_body_no_content_length)
{
    std::string result = http_protocol()
                             .request_line("POST", "/api")
                             .header("Host", "example.com")
                             .body("") // 空 body
                             .build();

    CHECK(result.find("Content-Length:") == std::string::npos);
}

// 测试 url_decode 边界情况
TEST_CASE(http_protocol_url_decode_edge_cases)
{
    // 空字符串
    CHECK(http_protocol::url_decode("") == "");

    // 不完整的百分号序列（应该保留原样）
    CHECK(http_protocol::url_decode("%") == "%");
    CHECK(http_protocol::url_decode("%2") == "%2");

    // 无效的十六进制字符（应该保留原样）
    CHECK(http_protocol::url_decode("%GG") == "%GG");
    CHECK(http_protocol::url_decode("%ZZ") == "%ZZ");

    // 混合有效和无效序列
    CHECK(http_protocol::url_decode("hello%20world%GG") == "hello world%GG");

    // 连续百分号: 第一个%后是%2,不是有效十六进制,保留;然后%20解码为空格
    CHECK(http_protocol::url_decode("%%20") == "% ");

    // 特殊字符解码
    CHECK(http_protocol::url_decode("%26") == "&"); // &
    CHECK(http_protocol::url_decode("%3D") == "="); // =
    CHECK(http_protocol::url_decode("%3F") == "?"); // ?
}

// 测试构建器状态码边界
TEST_CASE(http_protocol_status_code_boundary)
{
    // 最小有效状态码
    CHECK_THAT(http_protocol::response(100), ContainsSubstring("100"));

    // 最大有效状态码
    CHECK_THAT(http_protocol::response(999), ContainsSubstring("999"));

    // 无效状态码应该抛出异常
    bool caught = false;
    try
    {
        http_protocol::response(99);
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);

    caught = false;
    try
    {
        http_protocol::response(1000);
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);
}

// 测试 header replace 不存在的头部
TEST_CASE(http_protocol_header_replace_nonexistent)
{
    std::string result = http_protocol()
                             .request_line("GET", "/")
                             .header("Host", "example.com")
                             .header("X-Nonexistent", "value", true) // replace 不存在的头部
                             .build();

    // 应该正常添加新头部
    CHECK_THAT(result, ContainsSubstring("X-Nonexistent: value\r\n"));
}

// 测试多次 clear 和复用
TEST_CASE(http_protocol_reuse_after_clear)
{
    http_protocol p;

    // 第一次使用
    std::string first = p.request_line("GET", "/first")
                            .header("Host", "first.com")
                            .build();
    CHECK_THAT(first, ContainsSubstring("/first"));

    // 清空并复用
    p.clear();
    std::string second = p.request_line("POST", "/second")
                             .header("Host", "second.com")
                             .body("data")
                             .build();
    CHECK_THAT(second, ContainsSubstring("/second"));
    CHECK(second.find("/first") == std::string::npos);
    CHECK(second.find("first.com") == std::string::npos);
}

// 测试表单请求 round-trip（构建后解析）
TEST_CASE(http_protocol_form_roundtrip)
{
    std::string form_text = http_protocol::form(
        "POST", "/search",
        {{"q", "c++ http"}, {"page", "1"}, {"name", "张三"}});

    http_parse parser(HTTP_REQUEST);
    REQUIRE(parser.feed_all(form_text.data(), form_text.size()));

    CHECK(parser.http_method == "POST");
    CHECK(parser.http_url == "/search");
    CHECK(parser.header("Content-Type") == "application/x-www-form-urlencoded");

    // 验证 body 可以正确解码
    std::string decoded_body = parser.http_body;
    CHECK(decoded_body.find("q=c%2B%2B%20http") != std::string::npos);
    CHECK(decoded_body.find("page=1") != std::string::npos);
}

// 测试 status_line 链式接口
TEST_CASE(http_protocol_status_line_chain)
{
    std::string result = http_protocol()
                             .status_line(404, "Not Found")
                             .header("Content-Type", "text/plain")
                             .body("page not found")
                             .build();

    CHECK_THAT(result, ContainsSubstring("HTTP/1.1 404 Not Found\r\n"));
    CHECK_THAT(result, ContainsSubstring("Content-Type: text/plain\r\n"));
    CHECK_THAT(result, ContainsSubstring("page not found"));

    // 测试自定义 reason
    std::string custom = http_protocol()
                             .status_line(200, "All Good")
                             .build();
    CHECK_THAT(custom, ContainsSubstring("HTTP/1.1 200 All Good\r\n"));
}

// 测试未设置起始行直接 build
TEST_CASE(http_protocol_build_without_start_line)
{
    std::string result = http_protocol()
                             .header("Host", "example.com")
                             .body("data")
                             .build();

    // 应该以空行开头（无起始行），但头部和 body 应该正常
    CHECK_THAT(result, ContainsSubstring("Host: example.com\r\n"));
    CHECK_THAT(result, ContainsSubstring("data"));
}
