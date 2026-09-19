#include <cstring>
#include <iostream>
#include <stdexcept>

#include "http_parse.h"
#include "http_protocol.h"

int main()
{
    // ---- 1. 一站式生成(推荐用法) ----
    std::string get_text = http_protocol::request(
        "GET", "/", {{"Host", "example.com"}, {"User-Agent", "demo"}});

    std::string json_text = http_protocol::json(
        "POST", "/api/login", R"({"user":"alice","pwd":"123456"})",
        {{"Host", "example.com"}});

    std::string form_text = http_protocol::form(
        "POST", "/search", {{"q", "c++ http"}, {"page", "1"}});

    std::string resp_text = http_protocol::response(
        200, {{"Content-Type", "text/plain"}}, "hello world");

    // 链式细控:临时对象直接链,一行到底
    std::string chain_text = http_protocol()
                                 .request_line("PUT", "/api/user")
                                 .header("Host", "example.com")
                                 .body("update")
                                 .build();

    std::cout << "===== GET =====\n"
              << get_text
              << "===== JSON =====\n"
              << json_text
              << "===== FORM =====\n"
              << form_text
              << "===== RESPONSE =====\n"
              << resp_text
              << "===== CHAIN =====\n"
              << chain_text;

    // ---- 2. 非法输入会被校验拦截(防 CRLF 头注入) ----
    try
    {
        http_protocol::request("GET", "/", {{"Host\r\nX-Injected", "evil"}});
    }
    catch (const std::invalid_argument &e)
    {
        std::cout << "rejected: " << e.what() << "\n";
    }

    // ---- 3. 生成 + 解析回环验证 ----
    http_parse parser(HTTP_REQUEST);
    if (!parser.feed_all(json_text.data(), json_text.size()))
    {
        std::cerr << "parse failed: " << parser.error() << "\n";
        return 1;
    }
    std::cout << "===== parsed JSON request =====\n"
              << "method:  " << parser.http_method << "\n"
              << "url:     " << parser.http_url << "\n"
              << "version: " << parser.http_version << "\n"
              << "keep-alive: " << std::boolalpha << parser.keep_alive() << "\n"
              << "content-type: " << parser.header("content-type") << "\n"
              << "body:    " << parser.http_body << "\n";

    http_parse rparser(HTTP_RESPONSE);
    if (!rparser.feed_all(resp_text.data(), resp_text.size()))
    {
        std::cerr << "parse failed: " << rparser.error() << "\n";
        return 1;
    }
    std::cout << "===== parsed response =====\n"
              << "status: " << rparser.status_code << "\n"
              << "status-text: " << http_protocol::status_text(rparser.status_code) << "\n"
              << "body:   " << rparser.http_body << "\n";

    // ---- 4. URL 解码示例 ----
    std::cout << "===== URL decode =====\n"
              << "encoded: hello%20world -> " << http_protocol::url_decode("hello%20world") << "\n"
              << "form encoded: c%2B%2B+http -> " << http_protocol::url_decode("c%2B%2B+http") << "\n";

    // ---- 5. 同名多值头部(如 Set-Cookie) ----
    const char *multi_cookie_resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Set-Cookie: session=abc123\r\n"
        "Set-Cookie: user=alice\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "ok";

    http_parse cookie_parser(HTTP_RESPONSE);
    if (!cookie_parser.feed_all(multi_cookie_resp, strlen(multi_cookie_resp)))
    {
        std::cerr << "parse failed: " << cookie_parser.error() << "\n";
        return 1;
    }
    std::cout << "===== multi-value headers =====\n";
    auto cookies = cookie_parser.headers("Set-Cookie");
    for (const auto &cookie : cookies)
    {
        std::cout << "  cookie: " << cookie << "\n";
    }

    // ---- 6. HTTP_BOTH: 同时解析请求和响应 ----
    http_parse both_parser(HTTP_BOTH);

    // 解析请求
    const char *req = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    if (both_parser.feed_all(req, strlen(req)))
    {
        std::cout << "===== HTTP_BOTH (request) =====\n"
                  << "type: request\n"
                  << "method: " << both_parser.http_method << "\n";
    }

    // 解析响应
    const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    if (both_parser.feed_all(resp, strlen(resp)))
    {
        std::cout << "===== HTTP_BOTH (response) =====\n"
                  << "type: response\n"
                  << "status: " << both_parser.status_code << "\n";
    }

    return 0;
}
