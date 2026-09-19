#include "http_parse.h"

#include <cctype>

#include "http_utils.h"

namespace
{

    // llhttp_t::data 中取回封装对象
    inline http_parse *self(llhttp_t *parser)
    {
        return static_cast<http_parse *>(parser->data);
    }

} // namespace

const std::string http_parse::empty_string_;

const std::string &http_parse::header(const std::string &field) const
{
    for (const auto &h : http_headers)
    {
        if (http_utils::iequals(h.first, field))
            return h.second;
    }
    return empty_string_;
}

std::vector<std::string> http_parse::headers(const std::string &field) const
{
    std::vector<std::string> result;
    for (const auto &h : http_headers)
    {
        if (http_utils::iequals(h.first, field))
            result.push_back(h.second);
    }
    return result;
}

bool http_parse::keep_alive() const
{
    return llhttp_should_keep_alive(&parser_) != 0;
}

http_parse::http_parse(llhttp_type_t type) : type_(type)
{
    setup_callbacks();
    reset();
}

void http_parse::setup_callbacks()
{
    llhttp_settings_init(&settings_);

    settings_.on_message_begin = [](llhttp_t *parser) -> int
    {
        http_parse *p = self(parser);
        p->clear_result();
        p->header_count_ = 0;
        if (p->message_begin)
        {
            p->message_begin();
        }
        return 0;
    };

    settings_.on_url = [](llhttp_t *parser, const char *at, size_t length) -> int
    {
        http_parse *p = self(parser);
        // URL 长度限制检查(避免 size() + length 整数溢出)
        if (p->url_limit && length > p->url_limit - p->http_url.size())
        {
            llhttp_set_error_reason(parser, "url limit exceeded");
            return -1;
        }
        p->http_url.append(at, length);
        if (p->url)
        {
            p->url(at, length);
        }
        return 0;
    };

    // 头部字段/值可能分多个数据块到达,只做追加;
    // 在 *_complete 回调中才提交到 http_headers
    settings_.on_header_field = [](llhttp_t *parser, const char *at, size_t length) -> int
    {
        http_parse *p = self(parser);
        // 字段名长度限制检查(避免 size() + length 整数溢出)
        if (p->header_field_limit && length > p->header_field_limit - p->current_field_.size())
        {
            llhttp_set_error_reason(parser, "header field length limit exceeded");
            return -1;
        }
        p->current_field_.append(at, length);
        if (p->header_field)
        {
            p->header_field(at, length);
        }
        return 0;
    };

    settings_.on_header_value = [](llhttp_t *parser, const char *at, size_t length) -> int
    {
        http_parse *p = self(parser);
        // 字段值长度限制检查(避免 size() + length 整数溢出)
        if (p->header_value_limit && length > p->header_value_limit - p->current_value_.size())
        {
            llhttp_set_error_reason(parser, "header value length limit exceeded");
            return -1;
        }
        p->current_value_.append(at, length);
        if (p->header_value)
        {
            p->header_value(at, length);
        }
        return 0;
    };

    settings_.on_header_value_complete = [](llhttp_t *parser) -> int
    {
        http_parse *p = self(parser);
        // 头部数量限制检查
        if (p->header_count_limit && p->header_count_ >= p->header_count_limit)
        {
            llhttp_set_error_reason(parser, "header count limit exceeded");
            return -1;
        }
        p->http_headers.emplace(p->current_field_, p->current_value_);
        p->header_count_++;
        p->current_field_.clear();
        p->current_value_.clear();
        return 0;
    };

    settings_.on_body = [](llhttp_t *parser, const char *at, size_t length) -> int
    {
        http_parse *p = self(parser);
        // 防止超大消息体打爆内存,超限则中止解析(避免 size() + length 整数溢出)
        if (p->body_limit && length > p->body_limit - p->http_body.size())
        {
            llhttp_set_error_reason(parser, "body limit exceeded");
            return -1;
        }
        p->http_body.append(at, length);
        if (p->body)
        {
            p->body(at, length);
        }
        return 0;
    };

    settings_.on_message_complete = [](llhttp_t *parser) -> int
    {
        http_parse *p = self(parser);
        // 版本字符串直接构造,避免 to_string 临时对象
        p->http_version.resize(3);
        p->http_version[0] = '0' + static_cast<char>(llhttp_get_http_major(parser));
        p->http_version[1] = '.';
        p->http_version[2] = '0' + static_cast<char>(llhttp_get_http_minor(parser));
        if (llhttp_get_type(parser) == HTTP_REQUEST)
        {
            // 请求行此时已解析完毕,method 可用
            p->http_method = llhttp_method_name(static_cast<llhttp_method_t>(llhttp_get_method(parser)));
        }
        else if (llhttp_get_type(parser) == HTTP_RESPONSE)
        {
            p->status_code = llhttp_get_status_code(parser);
        }
        if (p->message_complete)
        {
            p->message_complete();
        }
        return 0;
    };
}

void http_parse::reset()
{
    error_.clear();
    current_field_.clear();
    current_value_.clear();
    header_count_ = 0;
    llhttp_init(&parser_, type_, &settings_);
    parser_.data = this;
}

bool http_parse::feed(const char *data, size_t len)
{
    error_.clear();
    llhttp_errno_t err = llhttp_execute(&parser_, data, len);
    if (err != HPE_OK)
    {
        set_error(err);
        return false;
    }

    // 仅在消息已完成时才调用 llhttp_finish 验证
    // 避免对无 Content-Length 的响应过早触发 finish,影响增量解析
    if (!llhttp_message_needs_eof(&parser_))
    {
        return true;
    }

    // 消息需要 EOF,检查是否已完成
    err = llhttp_finish(&parser_);
    if (err != HPE_OK)
    {
        set_error(err);
        return false;
    }

    return true;
}

bool http_parse::feed_all(const char *data, size_t len)
{
    reset();
    clear_result(); // 清空旧结果,防止 feed 失败时残留
    return feed(data, len);
}

void http_parse::clear_result()
{
    http_method.clear();
    http_url.clear();
    http_version.clear();
    http_body.clear();
    http_headers.clear();
    status_code = 0;
}

void http_parse::set_error(llhttp_errno_t err)
{
    // 出错后解析器进入错误态,重置以便接收下一条消息
    std::string error_msg = std::string(llhttp_errno_name(err)) + ": " + llhttp_get_error_reason(&parser_);
    reset();
    error_ = error_msg; // 保存错误信息
}
