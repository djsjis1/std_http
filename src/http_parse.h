#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "llhttp.h"

// llhttp 的 C++ 封装:流式喂入数据,自动累积解析结果(method/url/headers/body)。
// 支持一条连接上连续解析多条消息(keep-alive)。
//
// 状态契约:
//   - feed(): 增量喂入数据,可多次调用。出错返回 false,错误信息保留在 error() 中,
//             解析器自动重置以接收下一条消息。
//   - feed_all(): 解析一条完整消息。先 reset(),再 feed()。适用于一次性传入整条消息。
//   - reset(): 重置解析器内部状态,不清空已累积的解析结果(http_method/http_url 等),
//              但会清空 error()。新消息开始时 on_message_begin 回调会清空结果。
//
// 资源限制:
//   通过 body_limit / url_limit / header_field_limit / header_value_limit / header_count_limit
//   控制解析资源消耗,防止恶意或畸形输入导致资源耗尽。超限将中止解析并返回错误。
class http_parse
{
public:
    using void_callback = std::function<void()>;
    using data_callback = std::function<void(const char *, size_t)>;

    // type: HTTP_REQUEST / HTTP_RESPONSE / HTTP_BOTH
    explicit http_parse(llhttp_type_t type = HTTP_REQUEST);

    // parser->data 指向本对象,禁止拷贝
    http_parse(const http_parse &) = delete;
    http_parse &operator=(const http_parse &) = delete;

    // 增量喂入数据,可多次调用。出错返回 false,错误信息见 error()
    // 出错后解析器自动重置,可继续接收下一条消息
    bool feed(const char *data, size_t len);

    // 解析一条完整消息:先重置解析器,再 feed(一次性传入整条消息)
    bool feed_all(const char *data, size_t len);

    // 重置解析器内部状态(不清空已累积的解析结果,但会清空 error())
    void reset();

    // 最近一次解析错误的描述(出错后保留,直到下次成功解析或 reset())
    const std::string &error() const { return error_; }

    // 按字段名取头部,大小写不敏感(参考 RFC 7230);不存在返回空串
    const std::string &header(const std::string &field) const;

    // 获取指定字段名的所有值(支持 Set-Cookie 等同名多值头部)
    std::vector<std::string> headers(const std::string &field) const;

    // 当前连接是否应复用(消息解析完成后可调用)
    bool keep_alive() const;

    // ---- 解析结果(每条新消息开始时自动清空) ----
    std::string http_method;                              // 请求方法,仅解析请求时有效
    std::string http_url;                                 // 请求 URL,仅解析请求时有效
    std::string http_version;                             // 协议版本,如 "1.1"
    int status_code = 0;                                  // 状态码,仅解析响应时有效
    std::string http_body;                                // 消息体
    std::multimap<std::string, std::string> http_headers; // 头部,支持同名多值

    // ---- 资源限制(0 表示不限制) ----
    size_t body_limit = 8 * 1024 * 1024;  // 消息体上限,默认 8MB
    size_t url_limit = 8 * 1024;          // URL 上限,默认 8KB
    size_t header_field_limit = 4 * 1024; // 单个头部字段名上限,默认 4KB
    size_t header_value_limit = 8 * 1024; // 单个头部值上限,默认 8KB
    size_t header_count_limit = 100;      // 头部数量上限,默认 100 个

    // ---- 可选的用户回调,按需赋值 ----
    void_callback message_begin;    // 消息开始(结果已清空)
    void_callback message_complete; // 消息完整结束
    data_callback url;              // URL 数据块
    data_callback body;             // 消息体数据块
    data_callback header_field;     // 头部字段数据块
    data_callback header_value;     // 头部值数据块

private:
    void setup_callbacks();
    void clear_result();
    void set_error(llhttp_errno_t err);

    llhttp_t parser_;
    llhttp_settings_t settings_;
    llhttp_type_t type_;
    std::string current_field_; // 正在接收的头部字段名
    std::string current_value_; // 正在接收的头部值
    std::string error_;
    size_t header_count_ = 0; // 当前消息的头部计数
    static const std::string empty_string_;
};
