#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace vf {

enum class JsonType : uint8_t { Null, Bool, Number, String, Array, Object };

// Deep enough for any authored document, shallow enough that the recursive
// descent parser cannot run the stack out before it reports the error.
constexpr int jsonDefaultMaxDepth = 64;

// One JSON value. Arrays and objects hold JsonValue by value, so a parsed
// document is one self-contained object with no shared ownership.
//
// Object members keep insertion order: a document written back out lists its
// keys exactly as they were read, which is what makes the round trip byte
// stable. Equality is order sensitive for the same reason.
class JsonValue {
public:
    using Member = std::pair<std::string, JsonValue>;

    JsonValue() = default;

    static JsonValue makeNull();
    static JsonValue makeBool(bool value);
    static JsonValue makeNumber(double value);
    static JsonValue makeInt(long long value);
    static JsonValue makeString(std::string value);
    static JsonValue makeArray();
    static JsonValue makeObject();

    JsonType kind() const { return type; }
    bool isNull() const { return type == JsonType::Null; }
    bool isBool() const { return type == JsonType::Bool; }
    bool isNumber() const { return type == JsonType::Number; }
    bool isString() const { return type == JsonType::String; }
    bool isArray() const { return type == JsonType::Array; }
    bool isObject() const { return type == JsonType::Object; }

    // True when the number was written without a fraction or exponent and fits
    // in a 64 bit integer, so it can be read back with no precision loss.
    bool isInteger() const { return type == JsonType::Number && integral; }

    bool boolValue() const { return boolPayload; }
    double numberValue() const;
    long long intValue() const;
    const std::string& stringValue() const { return stringPayload; }

    const std::vector<JsonValue>& elements() const { return arrayPayload; }
    std::vector<JsonValue>& elements() { return arrayPayload; }
    const std::vector<Member>& members() const { return objectPayload; }
    std::vector<Member>& members() { return objectPayload; }

    // Element count for arrays, member count for objects, 0 for scalars.
    std::size_t size() const;
    bool empty() const { return size() == 0u; }

    void push(JsonValue value);
    // Overwrites an existing member in place, keeping its position.
    void set(const std::string& key, JsonValue value);
    bool erase(const std::string& key);

    const JsonValue* find(const std::string& key) const;
    JsonValue* find(const std::string& key);
    bool has(const std::string& key) const { return find(key) != nullptr; }
    const JsonValue* at(std::size_t index) const;

    // Typed reads that fall back instead of failing, for optional fields.
    bool boolOr(bool fallback) const;
    double numberOr(double fallback) const;
    long long intOr(long long fallback) const;
    std::string stringOr(const std::string& fallback) const;

    bool boolOr(const std::string& key, bool fallback) const;
    double numberOr(const std::string& key, double fallback) const;
    long long intOr(const std::string& key, long long fallback) const;
    std::string stringOr(const std::string& key, const std::string& fallback) const;

    bool operator==(const JsonValue& other) const;
    bool operator!=(const JsonValue& other) const { return !(*this == other); }

private:
    JsonType type = JsonType::Null;
    bool boolPayload = false;
    bool integral = false;
    double numberPayload = 0.0;
    long long integerPayload = 0;
    std::string stringPayload;
    std::vector<JsonValue> arrayPayload;
    std::vector<Member> objectPayload;
};

// Where a document stopped parsing. Line and column are 1 based and count
// bytes, and point at the offending character rather than past it.
struct JsonParseError {
    int line = 0;
    int column = 0;
    std::string message;

    bool present() const { return line > 0; }
    // "line:column: message", for logs and for DataPackError.
    std::string text() const;
};

struct JsonParseResult {
    bool ok = false;
    JsonValue root;
    JsonParseError error;
};

// Strict RFC 8259: no comments, no trailing commas, no unquoted keys, one root
// value plus trailing whitespace, control characters and malformed UTF-8
// rejected inside strings. Nesting deeper than maxDepth is an error, not a
// crash. Never throws and never aborts on malformed input.
JsonParseResult parseJson(const std::string& text, int maxDepth = jsonDefaultMaxDepth);

// Serializer. indentWidth 0 writes the compact form (no spaces at all), any
// positive width writes one member or element per line.
class JsonWriter {
public:
    int indentWidth = 0;
    // Emit every code point above ASCII as \uXXXX, surrogate pairs included, so
    // the output stays pure ASCII whatever went in.
    bool escapeNonAscii = true;

    std::string write(const JsonValue& value) const;

    static std::string compact(const JsonValue& value);
    static std::string indented(const JsonValue& value, int indentWidth = 2);

private:
    void writeValue(const JsonValue& value, int depth, std::string& out) const;
    void writeString(const std::string& value, std::string& out) const;
};

// RFC 6901 pointer: "" is the whole document, "/foo/0/bar" walks into it, "~1"
// stands for "/" and "~0" for "~". An array token must be a plain decimal index
// with no leading zeros.
class JsonPointer {
public:
    JsonPointer() = default;
    explicit JsonPointer(const std::string& pointer);

    bool valid() const { return wellFormed; }
    const std::vector<std::string>& tokens() const { return parts; }
    // The escaped text form, which round trips through the constructor.
    std::string text() const;

    const JsonValue* resolve(const JsonValue& root) const;
    JsonValue* resolve(JsonValue& root) const;

    static const JsonValue* find(const JsonValue& root, const std::string& pointer);

private:
    std::vector<std::string> parts;
    bool wellFormed = true;
};

} // namespace vf
