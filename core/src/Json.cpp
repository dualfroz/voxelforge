#include "vf/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vf {

namespace {

void appendUtf8(uint32_t codePoint, std::string& out) {
    if (codePoint < 0x80u) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800u) {
        out.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else if (codePoint < 0x10000u) {
        out.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    }
}

std::size_t decodeUtf8(const std::string& text, std::size_t at, uint32_t& codePoint) {
    unsigned char lead = static_cast<unsigned char>(text[at]);
    std::size_t length = 0;
    uint32_t value = 0;
    if (lead < 0x80u) {
        codePoint = lead;
        return 1u;
    }
    if ((lead & 0xE0u) == 0xC0u) {
        length = 2u;
        value = lead & 0x1Fu;
    } else if ((lead & 0xF0u) == 0xE0u) {
        length = 3u;
        value = lead & 0x0Fu;
    } else if ((lead & 0xF8u) == 0xF0u) {
        length = 4u;
        value = lead & 0x07u;
    } else {
        return 0u;
    }
    if (at + length > text.size()) return 0u;
    for (std::size_t i = 1u; i < length; ++i) {
        unsigned char next = static_cast<unsigned char>(text[at + i]);
        if ((next & 0xC0u) != 0x80u) return 0u;
        value = (value << 6) | (next & 0x3Fu);
    }
    if (length == 2u && value < 0x80u) return 0u;
    if (length == 3u && value < 0x800u) return 0u;
    if (length == 4u && value < 0x10000u) return 0u;
    if (value > 0x10FFFFu) return 0u;
    if (value >= 0xD800u && value <= 0xDFFFu) return 0u;
    codePoint = value;
    return length;
}

int hexDigitValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

class Parser {
public:
    Parser(const std::string& text, int maxDepth) : src(text), depthLimit(maxDepth) {}

    JsonParseResult run() {
        JsonParseResult parsed;
        skipWhitespace();
        if (atEnd()) {
            fail("unexpected end of input, expected a value");
            parsed.error = error;
            return parsed;
        }
        JsonValue root;
        if (!parseValue(1, root)) {
            parsed.error = error;
            return parsed;
        }
        skipWhitespace();
        if (!atEnd()) {
            fail("trailing character after the document root");
            parsed.error = error;
            return parsed;
        }
        parsed.ok = true;
        parsed.root = std::move(root);
        return parsed;
    }

private:
    const std::string& src;
    std::size_t cursor = 0u;
    int line = 1;
    int column = 1;
    int depthLimit = jsonDefaultMaxDepth;
    JsonParseError error;

    bool atEnd() const { return cursor >= src.size(); }
    char peek() const { return src[cursor]; }

    char advance() {
        char c = src[cursor++];
        if (c == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
        return c;
    }

    void skipWhitespace() {
        while (!atEnd()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                advance();
                continue;
            }
            break;
        }
    }

    void fail(const std::string& message) {
        if (error.present()) return;
        error.line = line;
        error.column = column;
        error.message = message;
    }

    void failAt(int atLine, int atColumn, const std::string& message) {
        if (error.present()) return;
        error.line = atLine;
        error.column = atColumn;
        error.message = message;
    }

    bool expect(char c, const std::string& message) {
        if (atEnd() || peek() != c) {
            fail(message);
            return false;
        }
        advance();
        return true;
    }

    bool parseValue(int depth, JsonValue& out) {
        if (atEnd()) {
            fail("unexpected end of input, expected a value");
            return false;
        }
        char c = peek();
        if (c == '{') return parseObject(depth, out);
        if (c == '[') return parseArray(depth, out);
        if (c == '"') {
            std::string text;
            if (!parseString(text)) return false;
            out = JsonValue::makeString(std::move(text));
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(out);
        if (c == 't') return parseLiteral("true", JsonValue::makeBool(true), out);
        if (c == 'f') return parseLiteral("false", JsonValue::makeBool(false), out);
        if (c == 'n') return parseLiteral("null", JsonValue::makeNull(), out);
        fail(std::string("unexpected character '") + c + "', expected a value");
        return false;
    }

    bool parseLiteral(const char* word, JsonValue value, JsonValue& out) {
        std::size_t length = std::strlen(word);
        if (src.compare(cursor, length, word) != 0) {
            fail(std::string("unknown literal, expected ") + word);
            return false;
        }
        for (std::size_t i = 0u; i < length; ++i) advance();
        out = std::move(value);
        return true;
    }

    bool parseObject(int depth, JsonValue& out) {
        if (depth > depthLimit) {
            fail("nesting deeper than the " + std::to_string(depthLimit) + " level limit");
            return false;
        }
        advance();
        out = JsonValue::makeObject();
        skipWhitespace();
        if (!atEnd() && peek() == '}') {
            advance();
            return true;
        }
        for (;;) {
            skipWhitespace();
            if (atEnd() || peek() != '"') {
                fail("expected a quoted member name");
                return false;
            }
            std::string key;
            if (!parseString(key)) return false;
            skipWhitespace();
            if (!expect(':', "expected ':' after the member name")) return false;
            skipWhitespace();
            JsonValue member;
            if (!parseValue(depth + 1, member)) return false;
            out.set(key, std::move(member));
            skipWhitespace();
            if (atEnd()) {
                fail("unexpected end of input, expected ',' or '}'");
                return false;
            }
            char c = peek();
            if (c == ',') {
                advance();
                skipWhitespace();
                if (!atEnd() && peek() == '}') {
                    fail("trailing comma before '}'");
                    return false;
                }
                continue;
            }
            if (c == '}') {
                advance();
                return true;
            }
            fail(std::string("unexpected character '") + c + "', expected ',' or '}'");
            return false;
        }
    }

    bool parseArray(int depth, JsonValue& out) {
        if (depth > depthLimit) {
            fail("nesting deeper than the " + std::to_string(depthLimit) + " level limit");
            return false;
        }
        advance();
        out = JsonValue::makeArray();
        skipWhitespace();
        if (!atEnd() && peek() == ']') {
            advance();
            return true;
        }
        for (;;) {
            skipWhitespace();
            JsonValue element;
            if (!parseValue(depth + 1, element)) return false;
            out.push(std::move(element));
            skipWhitespace();
            if (atEnd()) {
                fail("unexpected end of input, expected ',' or ']'");
                return false;
            }
            char c = peek();
            if (c == ',') {
                advance();
                skipWhitespace();
                if (!atEnd() && peek() == ']') {
                    fail("trailing comma before ']'");
                    return false;
                }
                continue;
            }
            if (c == ']') {
                advance();
                return true;
            }
            fail(std::string("unexpected character '") + c + "', expected ',' or ']'");
            return false;
        }
    }

    bool parseHexQuad(uint32_t& out) {
        uint32_t value = 0u;
        for (int i = 0; i < 4; ++i) {
            if (atEnd()) {
                fail("unexpected end of input inside a \\u escape");
                return false;
            }
            int digit = hexDigitValue(peek());
            if (digit < 0) {
                fail(std::string("'") + peek() + "' is not a hexadecimal digit");
                return false;
            }
            value = (value << 4) | static_cast<uint32_t>(digit);
            advance();
        }
        out = value;
        return true;
    }

    bool parseEscape(std::string& out) {
        if (atEnd()) {
            fail("unexpected end of input after '\\'");
            return false;
        }
        int escapeLine = line;
        int escapeColumn = column;
        char c = advance();
        switch (c) {
            case '"': out.push_back('"'); return true;
            case '\\': out.push_back('\\'); return true;
            case '/': out.push_back('/'); return true;
            case 'b': out.push_back('\b'); return true;
            case 'f': out.push_back('\f'); return true;
            case 'n': out.push_back('\n'); return true;
            case 'r': out.push_back('\r'); return true;
            case 't': out.push_back('\t'); return true;
            case 'u': break;
            default:
                failAt(escapeLine, escapeColumn,
                       std::string("unknown escape sequence '\\") + c + "'");
                return false;
        }
        uint32_t unit = 0u;
        if (!parseHexQuad(unit)) return false;
        if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            failAt(escapeLine, escapeColumn, "low surrogate without a preceding high surrogate");
            return false;
        }
        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            if (cursor + 1u >= src.size() || src[cursor] != '\\' || src[cursor + 1u] != 'u') {
                failAt(escapeLine, escapeColumn, "high surrogate without a following low surrogate");
                return false;
            }
            advance();
            advance();
            uint32_t low = 0u;
            if (!parseHexQuad(low)) return false;
            if (low < 0xDC00u || low > 0xDFFFu) {
                failAt(escapeLine, escapeColumn, "high surrogate followed by a non low surrogate");
                return false;
            }
            uint32_t combined = 0x10000u + ((unit - 0xD800u) << 10) + (low - 0xDC00u);
            appendUtf8(combined, out);
            return true;
        }
        appendUtf8(unit, out);
        return true;
    }

    bool parseString(std::string& out) {
        advance();
        out.clear();
        for (;;) {
            if (atEnd()) {
                fail("unexpected end of input inside a string");
                return false;
            }
            unsigned char c = static_cast<unsigned char>(peek());
            if (c == '"') {
                advance();
                return true;
            }
            if (c == '\\') {
                advance();
                if (!parseEscape(out)) return false;
                continue;
            }
            if (c < 0x20u) {
                fail("raw control character in a string, escape it as \\u00" +
                     std::string(1, "0123456789abcdef"[(c >> 4) & 0xFu]) +
                     std::string(1, "0123456789abcdef"[c & 0xFu]));
                return false;
            }
            if (c < 0x80u) {
                out.push_back(static_cast<char>(c));
                advance();
                continue;
            }
            uint32_t codePoint = 0u;
            std::size_t length = decodeUtf8(src, cursor, codePoint);
            if (length == 0u) {
                fail("malformed UTF-8 byte sequence in a string");
                return false;
            }
            for (std::size_t i = 0u; i < length; ++i) out.push_back(advance());
        }
    }

    bool parseNumber(JsonValue& out) {
        std::size_t start = cursor;
        int startLine = line;
        int startColumn = column;
        bool negative = false;
        if (peek() == '-') {
            negative = true;
            advance();
        }
        if (atEnd()) {
            fail("unexpected end of input inside a number");
            return false;
        }
        if (peek() == '0') {
            advance();
            if (!atEnd() && peek() >= '0' && peek() <= '9') {
                fail("leading zero in a number");
                return false;
            }
        } else if (peek() >= '1' && peek() <= '9') {
            while (!atEnd() && peek() >= '0' && peek() <= '9') advance();
        } else {
            fail("expected a digit after the sign");
            return false;
        }

        bool fractional = false;
        if (!atEnd() && peek() == '.') {
            fractional = true;
            advance();
            if (atEnd() || peek() < '0' || peek() > '9') {
                fail("expected a digit after the decimal point");
                return false;
            }
            while (!atEnd() && peek() >= '0' && peek() <= '9') advance();
        }
        if (!atEnd() && (peek() == 'e' || peek() == 'E')) {
            fractional = true;
            advance();
            if (!atEnd() && (peek() == '+' || peek() == '-')) advance();
            if (atEnd() || peek() < '0' || peek() > '9') {
                fail("expected a digit in the exponent");
                return false;
            }
            while (!atEnd() && peek() >= '0' && peek() <= '9') advance();
        }

        std::string token = src.substr(start, cursor - start);
        if (!fractional) {
            const unsigned long long limit =
                negative ? 9223372036854775808ull : 9223372036854775807ull;
            unsigned long long magnitude = 0ull;
            bool overflow = false;
            for (char c : token) {
                if (c == '-') continue;
                magnitude = magnitude * 10ull + static_cast<unsigned long long>(c - '0');
                if (magnitude > limit) {
                    overflow = true;
                    break;
                }
            }
            if (!overflow) {
                if (magnitude == 9223372036854775808ull) {
                    out = JsonValue::makeInt(-9223372036854775807ll - 1ll);
                } else {
                    long long whole = static_cast<long long>(magnitude);
                    out = JsonValue::makeInt(negative ? -whole : whole);
                }
                return true;
            }
        }
        double value = std::strtod(token.c_str(), nullptr);
        if (!std::isfinite(value)) {
            failAt(startLine, startColumn, "number out of the representable range");
            return false;
        }
        out = JsonValue::makeNumber(value);
        return true;
    }
};

std::string formatNumber(const JsonValue& value) {
    if (value.isInteger()) return std::to_string(value.intValue());
    double raw = value.numberValue();
    if (!std::isfinite(raw)) return "0";
    char buffer[48];
    for (int precision = 15; precision <= 17; ++precision) {
        std::snprintf(buffer, sizeof buffer, "%.*g", precision, raw);
        if (std::strtod(buffer, nullptr) == raw) break;
    }
    std::string text(buffer);
    if (text.find_first_of(".eE") == std::string::npos) text += ".0";
    return text;
}

}

JsonValue JsonValue::makeNull() { return JsonValue(); }

JsonValue JsonValue::makeBool(bool value) {
    JsonValue made;
    made.type = JsonType::Bool;
    made.boolPayload = value;
    return made;
}

JsonValue JsonValue::makeNumber(double value) {
    JsonValue made;
    made.type = JsonType::Number;
    made.numberPayload = value;
    return made;
}

JsonValue JsonValue::makeInt(long long value) {
    JsonValue made;
    made.type = JsonType::Number;
    made.integral = true;
    made.integerPayload = value;
    made.numberPayload = static_cast<double>(value);
    return made;
}

JsonValue JsonValue::makeString(std::string value) {
    JsonValue made;
    made.type = JsonType::String;
    made.stringPayload = std::move(value);
    return made;
}

JsonValue JsonValue::makeArray() {
    JsonValue made;
    made.type = JsonType::Array;
    return made;
}

JsonValue JsonValue::makeObject() {
    JsonValue made;
    made.type = JsonType::Object;
    return made;
}

double JsonValue::numberValue() const {
    if (type != JsonType::Number) return 0.0;
    return integral ? static_cast<double>(integerPayload) : numberPayload;
}

long long JsonValue::intValue() const {
    if (type != JsonType::Number) return 0;
    if (integral) return integerPayload;
    double rounded = numberPayload < 0.0 ? std::ceil(numberPayload - 0.5)
                                         : std::floor(numberPayload + 0.5);
    if (rounded >= 9223372036854775296.0) return 9223372036854775807ll;
    if (rounded <= -9223372036854775296.0) return -9223372036854775807ll - 1ll;
    return static_cast<long long>(rounded);
}

std::size_t JsonValue::size() const {
    if (type == JsonType::Array) return arrayPayload.size();
    if (type == JsonType::Object) return objectPayload.size();
    return 0u;
}

void JsonValue::push(JsonValue value) {
    if (type != JsonType::Array) {
        *this = makeArray();
    }
    arrayPayload.push_back(std::move(value));
}

void JsonValue::set(const std::string& key, JsonValue value) {
    if (type != JsonType::Object) {
        *this = makeObject();
    }
    for (Member& member : objectPayload) {
        if (member.first == key) {
            member.second = std::move(value);
            return;
        }
    }
    objectPayload.emplace_back(key, std::move(value));
}

bool JsonValue::erase(const std::string& key) {
    for (std::size_t i = 0u; i < objectPayload.size(); ++i) {
        if (objectPayload[i].first == key) {
            objectPayload.erase(objectPayload.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

const JsonValue* JsonValue::find(const std::string& key) const {
    for (const Member& member : objectPayload)
        if (member.first == key) return &member.second;
    return nullptr;
}

JsonValue* JsonValue::find(const std::string& key) {
    for (Member& member : objectPayload)
        if (member.first == key) return &member.second;
    return nullptr;
}

const JsonValue* JsonValue::at(std::size_t index) const {
    if (type != JsonType::Array || index >= arrayPayload.size()) return nullptr;
    return &arrayPayload[index];
}

bool JsonValue::boolOr(bool fallback) const { return isBool() ? boolPayload : fallback; }

double JsonValue::numberOr(double fallback) const { return isNumber() ? numberValue() : fallback; }

long long JsonValue::intOr(long long fallback) const { return isNumber() ? intValue() : fallback; }

std::string JsonValue::stringOr(const std::string& fallback) const {
    return isString() ? stringPayload : fallback;
}

bool JsonValue::boolOr(const std::string& key, bool fallback) const {
    const JsonValue* member = find(key);
    return member ? member->boolOr(fallback) : fallback;
}

double JsonValue::numberOr(const std::string& key, double fallback) const {
    const JsonValue* member = find(key);
    return member ? member->numberOr(fallback) : fallback;
}

long long JsonValue::intOr(const std::string& key, long long fallback) const {
    const JsonValue* member = find(key);
    return member ? member->intOr(fallback) : fallback;
}

std::string JsonValue::stringOr(const std::string& key, const std::string& fallback) const {
    const JsonValue* member = find(key);
    return member ? member->stringOr(fallback) : fallback;
}

bool JsonValue::operator==(const JsonValue& other) const {
    if (type != other.type) return false;
    switch (type) {
        case JsonType::Null: return true;
        case JsonType::Bool: return boolPayload == other.boolPayload;
        case JsonType::Number: return numberValue() == other.numberValue();
        case JsonType::String: return stringPayload == other.stringPayload;
        case JsonType::Array: return arrayPayload == other.arrayPayload;
        case JsonType::Object: return objectPayload == other.objectPayload;
    }
    return false;
}

std::string JsonParseError::text() const {
    if (!present()) return "no error";
    return std::to_string(line) + ":" + std::to_string(column) + ": " + message;
}

JsonParseResult parseJson(const std::string& text, int maxDepth) {
    Parser parser(text, maxDepth < 1 ? 1 : maxDepth);
    return parser.run();
}

void JsonWriter::writeString(const std::string& value, std::string& out) const {
    static const char* hexDigits = "0123456789abcdef";
    out.push_back('"');
    std::size_t at = 0u;
    while (at < value.size()) {
        unsigned char c = static_cast<unsigned char>(value[at]);
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(static_cast<char>(c));
            ++at;
            continue;
        }
        if (c == '\b') { out += "\\b"; ++at; continue; }
        if (c == '\f') { out += "\\f"; ++at; continue; }
        if (c == '\n') { out += "\\n"; ++at; continue; }
        if (c == '\r') { out += "\\r"; ++at; continue; }
        if (c == '\t') { out += "\\t"; ++at; continue; }
        if (c < 0x20u) {
            out += "\\u00";
            out.push_back(hexDigits[(c >> 4) & 0xFu]);
            out.push_back(hexDigits[c & 0xFu]);
            ++at;
            continue;
        }
        if (c < 0x80u || !escapeNonAscii) {
            out.push_back(value[at]);
            ++at;
            continue;
        }
        uint32_t codePoint = 0u;
        std::size_t length = decodeUtf8(value, at, codePoint);
        if (length == 0u) {
            out.push_back(value[at]);
            ++at;
            continue;
        }
        if (codePoint >= 0x10000u) {
            uint32_t rest = codePoint - 0x10000u;
            uint32_t high = 0xD800u + (rest >> 10);
            uint32_t low = 0xDC00u + (rest & 0x3FFu);
            for (uint32_t unit : {high, low}) {
                out += "\\u";
                for (int shift = 12; shift >= 0; shift -= 4)
                    out.push_back(hexDigits[(unit >> shift) & 0xFu]);
            }
        } else {
            out += "\\u";
            for (int shift = 12; shift >= 0; shift -= 4)
                out.push_back(hexDigits[(codePoint >> shift) & 0xFu]);
        }
        at += length;
    }
    out.push_back('"');
}

void JsonWriter::writeValue(const JsonValue& value, int depth, std::string& out) const {
    const bool pretty = indentWidth > 0;
    const std::string inner = pretty ? std::string(static_cast<std::size_t>(indentWidth * (depth + 1)), ' ')
                                     : std::string();
    const std::string outer = pretty ? std::string(static_cast<std::size_t>(indentWidth * depth), ' ')
                                     : std::string();
    switch (value.kind()) {
        case JsonType::Null: out += "null"; return;
        case JsonType::Bool: out += value.boolValue() ? "true" : "false"; return;
        case JsonType::Number: out += formatNumber(value); return;
        case JsonType::String: writeString(value.stringValue(), out); return;
        case JsonType::Array: {
            if (value.elements().empty()) {
                out += "[]";
                return;
            }
            out.push_back('[');
            bool first = true;
            for (const JsonValue& element : value.elements()) {
                if (!first) out.push_back(',');
                first = false;
                if (pretty) {
                    out.push_back('\n');
                    out += inner;
                }
                writeValue(element, depth + 1, out);
            }
            if (pretty) {
                out.push_back('\n');
                out += outer;
            }
            out.push_back(']');
            return;
        }
        case JsonType::Object: {
            if (value.members().empty()) {
                out += "{}";
                return;
            }
            out.push_back('{');
            bool first = true;
            for (const JsonValue::Member& member : value.members()) {
                if (!first) out.push_back(',');
                first = false;
                if (pretty) {
                    out.push_back('\n');
                    out += inner;
                }
                writeString(member.first, out);
                out.push_back(':');
                if (pretty) out.push_back(' ');
                writeValue(member.second, depth + 1, out);
            }
            if (pretty) {
                out.push_back('\n');
                out += outer;
            }
            out.push_back('}');
            return;
        }
    }
}

std::string JsonWriter::write(const JsonValue& value) const {
    std::string out;
    writeValue(value, 0, out);
    return out;
}

std::string JsonWriter::compact(const JsonValue& value) {
    JsonWriter writer;
    return writer.write(value);
}

std::string JsonWriter::indented(const JsonValue& value, int indentWidth) {
    JsonWriter writer;
    writer.indentWidth = indentWidth;
    return writer.write(value);
}

JsonPointer::JsonPointer(const std::string& pointer) {
    if (pointer.empty()) return;
    if (pointer[0] != '/') {
        wellFormed = false;
        return;
    }
    std::string token;
    for (std::size_t i = 1u; i <= pointer.size(); ++i) {
        if (i == pointer.size() || pointer[i] == '/') {
            parts.push_back(token);
            token.clear();
            continue;
        }
        if (pointer[i] == '~') {
            if (i + 1u >= pointer.size()) {
                wellFormed = false;
                return;
            }
            char next = pointer[i + 1u];
            if (next == '0') {
                token.push_back('~');
            } else if (next == '1') {
                token.push_back('/');
            } else {
                wellFormed = false;
                return;
            }
            ++i;
            continue;
        }
        token.push_back(pointer[i]);
    }
}

std::string JsonPointer::text() const {
    std::string out;
    for (const std::string& token : parts) {
        out.push_back('/');
        for (char c : token) {
            if (c == '~') {
                out += "~0";
            } else if (c == '/') {
                out += "~1";
            } else {
                out.push_back(c);
            }
        }
    }
    return out;
}

namespace {

bool arrayIndexOf(const std::string& token, std::size_t& out) {
    if (token.empty()) return false;
    if (token.size() > 1u && token[0] == '0') return false;
    std::size_t index = 0u;
    for (char c : token) {
        if (c < '0' || c > '9') return false;
        index = index * 10u + static_cast<std::size_t>(c - '0');
        if (index > 100000000u) return false;
    }
    out = index;
    return true;
}

}

const JsonValue* JsonPointer::resolve(const JsonValue& root) const {
    if (!wellFormed) return nullptr;
    const JsonValue* here = &root;
    for (const std::string& token : parts) {
        if (here->isObject()) {
            here = here->find(token);
            if (here == nullptr) return nullptr;
            continue;
        }
        if (here->isArray()) {
            std::size_t index = 0u;
            if (!arrayIndexOf(token, index)) return nullptr;
            here = here->at(index);
            if (here == nullptr) return nullptr;
            continue;
        }
        return nullptr;
    }
    return here;
}

JsonValue* JsonPointer::resolve(JsonValue& root) const {
    return const_cast<JsonValue*>(resolve(static_cast<const JsonValue&>(root)));
}

const JsonValue* JsonPointer::find(const JsonValue& root, const std::string& pointer) {
    JsonPointer path(pointer);
    return path.resolve(root);
}

}
