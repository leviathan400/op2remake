#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// A small JSON reader for scenario files: objects, arrays, strings (with the
// standard escapes), integers, booleans and null. Numbers with a fraction or
// exponent are rejected (scenarios are integer-only). Throws std::runtime_error
// with the byte offset on bad input.

namespace op2::inspect {

struct Json {
    enum class Kind { Null, Bool, Int, String, Array, Object } kind = Kind::Null;
    bool b = false;
    std::int64_t i = 0;
    std::string s;
    std::vector<Json> a;
    std::map<std::string, Json> o;

    bool has(const std::string& key) const { return kind == Kind::Object && o.count(key) > 0; }
    const Json& at(const std::string& key) const {
        const auto it = o.find(key);
        if (kind != Kind::Object || it == o.end()) throw std::runtime_error("missing \"" + key + "\"");
        return it->second;
    }
    std::int64_t asInt() const {
        if (kind != Kind::Int) throw std::runtime_error("expected an integer");
        return i;
    }
    const std::string& asString() const {
        if (kind != Kind::String) throw std::runtime_error("expected a string");
        return s;
    }
    const std::vector<Json>& asArray() const {
        if (kind != Kind::Array) throw std::runtime_error("expected an array");
        return a;
    }
    // An optional integer field.
    std::int64_t intOr(const std::string& key, std::int64_t def) const { return has(key) ? at(key).asInt() : def; }

    static Json parse(const std::string& text);
};

namespace detail {

class Parser {
public:
    explicit Parser(const std::string& t) : t_(t) {}

    Json document() {
        Json v = value();
        ws();
        if (p_ != t_.size()) fail("trailing characters");
        return v;
    }

private:
    [[noreturn]] void fail(const char* what) const {
        throw std::runtime_error(std::string("json: ") + what + " at byte " + std::to_string(p_));
    }
    void ws() {
        while (p_ < t_.size() && (t_[p_] == ' ' || t_[p_] == '\t' || t_[p_] == '\n' || t_[p_] == '\r')) ++p_;
    }
    bool eat(char c) {
        ws();
        if (p_ < t_.size() && t_[p_] == c) { ++p_; return true; }
        return false;
    }
    void expect(char c) {
        if (!eat(c)) fail("unexpected character");
    }
    bool word(const char* w) {
        const std::string s(w);
        if (t_.compare(p_, s.size(), s) == 0) { p_ += s.size(); return true; }
        return false;
    }

    Json value() {
        if (++depth_ > 64) fail("nested too deeply");
        ws();
        if (p_ >= t_.size()) fail("unexpected end");
        Json v;
        const char c = t_[p_];
        if (c == '{') {
            ++p_;
            v.kind = Json::Kind::Object;
            if (!eat('}')) {
                do {
                    ws();
                    std::string key = string();
                    expect(':');
                    v.o[key] = value();
                } while (eat(','));
                expect('}');
            }
        } else if (c == '[') {
            ++p_;
            v.kind = Json::Kind::Array;
            if (!eat(']')) {
                do { v.a.push_back(value()); } while (eat(','));
                expect(']');
            }
        } else if (c == '"') {
            v.kind = Json::Kind::String;
            v.s = string();
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            v.kind = Json::Kind::Int;
            v.i = integer();
        } else if (word("true")) {
            v.kind = Json::Kind::Bool; v.b = true;
        } else if (word("false")) {
            v.kind = Json::Kind::Bool;
        } else if (!word("null")) {
            fail("unexpected character");
        }
        --depth_;
        return v;
    }

    std::int64_t integer() {
        const bool neg = t_[p_] == '-';
        if (neg) ++p_;
        if (p_ >= t_.size() || t_[p_] < '0' || t_[p_] > '9') fail("bad number");
        std::int64_t v = 0;
        while (p_ < t_.size() && t_[p_] >= '0' && t_[p_] <= '9') {
            if (v > (INT64_MAX - 9) / 10) fail("number too large");
            v = v * 10 + (t_[p_++] - '0');
        }
        if (p_ < t_.size() && (t_[p_] == '.' || t_[p_] == 'e' || t_[p_] == 'E')) fail("only integers are supported");
        return neg ? -v : v;
    }

    std::string string() {
        if (p_ >= t_.size() || t_[p_] != '"') fail("expected a string");
        ++p_;
        std::string out;
        while (true) {
            if (p_ >= t_.size()) fail("unterminated string");
            const char c = t_[p_++];
            if (c == '"') break;
            if (c != '\\') { out += c; continue; }
            if (p_ >= t_.size()) fail("unterminated string");
            const char e = t_[p_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (t_.size() - p_ < 4) fail("bad \\u escape");
                    unsigned cp = 0;
                    for (int k = 0; k < 4; ++k) {
                        const char h = t_[p_++];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                        else fail("bad \\u escape");
                    }
                    // UTF-8 (BMP only; scenario files are ASCII in practice).
                    if (cp < 0x80) out += static_cast<char>(cp);
                    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
                    else { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
                    break;
                }
                default: fail("bad escape");
            }
        }
        return out;
    }

    const std::string& t_;
    std::size_t p_ = 0;
    int depth_ = 0;
};

} // namespace detail

inline Json Json::parse(const std::string& text) { return detail::Parser(text).document(); }

} // namespace op2::inspect
