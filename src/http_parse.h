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
//   - finish(): 连接关闭(EOF)时调用,结束依赖 EOF 才能结束的消息(如无
//               Content-Length 的响应),补发 message_complete。
//   - feed_all(): 解析一条完整消息:先 reset(),再 feed(),必要时 finish()。
//                 输入不是一条完整消息时返回 false。
//   - reset(): 重置解析器内部状态,不清空已累积的解析结果(http_method/http_url 等),
//              但会清空 error()。新消息开始时 on_message_begin 回调会清空结果。
//
// 消息进度:
//   method/url/version/headers/status_code 在 headers_complete 时即已就绪;
//   http_body 随数据累积。
//
// 协议升级(101 Switching Protocols / CONNECT):
//   升级消息解析成功(feed/feed_all 返回 true,is_upgrade() 为 true),解析器随后
//   暂停——升级后的数据属于隧道协议,不应再喂给本解析器。如需忽略升级继续按
//   HTTP 解析(少见),可调用 resume_after_upgrade()。
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
    // 协议升级(101/CONNECT)不算错误:返回 true,解析器暂停,见 is_upgrade()
    bool feed(const char *data, size_t len);

    // 连接关闭(EOF)时调用:结束依赖 EOF 的消息(如无 Content-Length 的响应),
    // 补发 message_complete。返回调用后最近一条消息是否已完整结束:
    // 此前已完成的消息返回 true 且不重复回调;连接上没有完整消息时返回 false
    bool finish();

    // 解析一条完整消息:先重置解析器,再 feed(一次性传入整条消息)
    bool feed_all(const char *data, size_t len);

    // 重置解析器内部状态(不清空已累积的解析结果,但会清空 error())
    void reset();

    // 最近一次解析错误的描述(出错后保留,直到下次成功解析或 reset())
    const std::string &error() const { return error_; }

    // 最后一条消息是否以协议升级结束(101 Switching Protocols / CONNECT)
    // llhttp_get_upgrade 未声明 const,此处 const_cast 不修改解析器状态
    bool is_upgrade() const { return llhttp_get_upgrade(const_cast<llhttp_t *>(&parser_)) != 0; }

    // 忽略已发生的协议升级,继续按 HTTP 解析(隧道场景不需要,见类注释)
    void resume_after_upgrade() { llhttp_resume_after_upgrade(&parser_); }

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
    void_callback headers_complete; // 头部接收完毕(method/url/version/headers/status_code 已就绪)
    void_callback message_complete; // 消息完整结束
    data_callback url;              // URL 数据块
    data_callback body;             // 消息体数据块
    data_callback header_field;     // 头部字段数据块
    data_callback header_value;     // 头部值数据块
    void_callback chunk_header;     // chunked 编码:一个 chunk 开始
    void_callback chunk_complete;   // chunked 编码:一个 chunk 数据结束

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
    size_t header_count_ = 0;       // 当前消息的头部计数
    bool message_complete_ = false; // 当前消息是否已完成
    static const std::string empty_string_;
};
