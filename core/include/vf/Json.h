#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace vf {

enum class JsonType : uint8_t { Null, Bool, Number, String, Array, Object };

constexpr int jsonDefaultMaxDepth = 64;

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

    bool isInteger() const { return type == JsonType::Number && integral; }

    bool boolValue() const { return boolPayload; }
    double numberValue() const;
    long long intValue() const;
    const std::string& stringValue() const { return stringPayload; }

    const std::vector<JsonValue>& elements() const { return arrayPayload; }
    std::vector<JsonValue>& elements() { return arrayPayload; }
    const std::vector<Member>& members() const { return objectPayload; }
    std::vector<Member>& members() { return objectPayload; }

    std::size_t size() const;
    bool empty() const { return size() == 0u; }

    void push(JsonValue value);
    void set(const std::string& key, JsonValue value);
    bool erase(const std::string& key);

    const JsonValue* find(const std::string& key) const;
    JsonValue* find(const std::string& key);
    bool has(const std::string& key) const { return find(key) != nullptr; }
    const JsonValue* at(std::size_t index) const;

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

struct JsonParseError {
    int line = 0;
    int column = 0;
    std::string message;

    bool present() const { return line > 0; }
    std::string text() const;
};

struct JsonParseResult {
    bool ok = false;
    JsonValue root;
    JsonParseError error;
};

JsonParseResult parseJson(const std::string& text, int maxDepth = jsonDefaultMaxDepth);

class JsonWriter {
public:
    int indentWidth = 0;
    bool escapeNonAscii = true;

    std::string write(const JsonValue& value) const;

    static std::string compact(const JsonValue& value);
    static std::string indented(const JsonValue& value, int indentWidth = 2);

private:
    void writeValue(const JsonValue& value, int depth, std::string& out) const;
    void writeString(const std::string& value, std::string& out) const;
};

class JsonPointer {
public:
    JsonPointer() = default;
    explicit JsonPointer(const std::string& pointer);

    bool valid() const { return wellFormed; }
    const std::vector<std::string>& tokens() const { return parts; }
    std::string text() const;

    const JsonValue* resolve(const JsonValue& root) const;
    JsonValue* resolve(JsonValue& root) const;

    static const JsonValue* find(const JsonValue& root, const std::string& pointer);

private:
    std::vector<std::string> parts;
    bool wellFormed = true;
};

}
