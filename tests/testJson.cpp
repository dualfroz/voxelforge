#include "testHarness.h"
#include "vf/Json.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace vf;

namespace {

const std::vector<std::string>& uglyDocuments() {
    static const std::vector<std::string> documents = {
        "{}",
        "[]",
        "null",
        "true",
        "false",
        "0",
        "-0",
        "-17",
        "3.5",
        "1e-3",
        "-2.5E+4",
        "1e308",
        "0.1",
        "9223372036854775807",
        "-9223372036854775808",
        "\"\"",
        "\"tab\\tnewline\\nquote\\\"backslash\\\\slash\\/bell\\bform\\fcarriage\\r\"",
        "\"\\u0000 embedded nul, \\u001f control, \\u007f delete\"",
        "\"\\u00e9\\u0141\\u4e2d\\ud83e\\udd8a\"",
        "[[[[[[[[[[1]]]]]]]]]]",
        "[1,2,3,[4,[5,[6]]],{\"deep\":{\"deeper\":{\"deepest\":[]}}}]",
        "{\"\":\"empty key\"}",
        "{\"a/b\":1,\"c~d\":2,\"e f\":3}",
        "{\"duplicate\":1,\"duplicate\":2}",
        "   \n\t  {\n \"spread\" :\t[ 1 ,\n 2 ]\n}  \n",
        "{\"mixed\":[null,true,false,0,-0.0,\"\",{},[]]}",
        "[{\"item\":\"voxelforge:iron_ingot\",\"count\":3,\"chance\":0.25}]",
        "{\"nested\":{\"list\":[{\"a\":[{\"b\":[[[1]]]}]}]}}",
        "{\"unicodeKey\\u00e9\":\"value\\u00e9\"}",
    };
    return documents;
}

JsonValue mustParse(const std::string& text) {
    JsonParseResult parsed = parseJson(text);
    CHECK(parsed.ok);
    if (!parsed.ok) std::printf("  parse failed: %s\n", parsed.error.text().c_str());
    return parsed.root;
}

std::string repeated(const std::string& unit, int times) {
    std::string out;
    out.reserve(unit.size() * static_cast<std::size_t>(times));
    for (int i = 0; i < times; ++i) out += unit;
    return out;
}

struct BadDocument {
    const char* text;
    int line;
    int column;
    const char* fragment;
};

}

VF_TEST(everyUglyDocumentSurvivesParseWriteParseUnchanged) {
    for (const std::string& text : uglyDocuments()) {
        JsonParseResult first = parseJson(text);
        CHECK(first.ok);
        if (!first.ok) {
            std::printf("  rejected: %s -> %s\n", text.c_str(), first.error.text().c_str());
            continue;
        }
        std::string compact = JsonWriter::compact(first.root);
        JsonParseResult second = parseJson(compact);
        CHECK(second.ok);
        CHECK(second.root == first.root);

        std::string indented = JsonWriter::indented(first.root, 2);
        JsonParseResult third = parseJson(indented);
        CHECK(third.ok);
        CHECK(third.root == first.root);

        CHECK_EQ(JsonWriter::compact(second.root), compact);
        CHECK_EQ(JsonWriter::compact(third.root), compact);
    }
}

VF_TEST(writtenDocumentsStayPureAsciiWhateverWentIn) {
    JsonValue root = mustParse("{\"key\":\"\\u00e9\\u4e2d\\ud83e\\udd8a\"}");
    std::string compact = JsonWriter::compact(root);
    for (char c : compact) CHECK(static_cast<unsigned char>(c) < 0x80u);
    CHECK_EQ(compact, std::string("{\"key\":\"\\u00e9\\u4e2d\\ud83e\\udd8a\"}"));

    JsonWriter raw;
    raw.escapeNonAscii = false;
    std::string utf8 = raw.write(root);
    CHECK(utf8.size() < compact.size());
    bool carriesHighBytes = false;
    for (char c : utf8)
        if (static_cast<unsigned char>(c) >= 0x80u) carriesHighBytes = true;
    CHECK(carriesHighBytes);
    CHECK(parseJson(utf8).ok);
    CHECK(parseJson(utf8).root == root);
}

VF_TEST(surrogatePairsBecomeOneCodePointAndComeBackOutAsAPair) {
    JsonValue root = mustParse("[\"\\ud83e\\udd8a\"]");
    const JsonValue* text = root.at(0u);
    CHECK(text != nullptr);
    CHECK_EQ(text->stringValue().size(), static_cast<std::size_t>(4));
    CHECK_EQ(static_cast<unsigned char>(text->stringValue()[0]), 0xF0u);
    CHECK_EQ(static_cast<unsigned char>(text->stringValue()[1]), 0x9Fu);
    CHECK_EQ(static_cast<unsigned char>(text->stringValue()[2]), 0xA6u);
    CHECK_EQ(static_cast<unsigned char>(text->stringValue()[3]), 0x8Au);
    CHECK_EQ(JsonWriter::compact(root), std::string("[\"\\ud83e\\udd8a\"]"));

    JsonValue latin = mustParse("\"\\u00e9\"");
    CHECK_EQ(latin.stringValue().size(), static_cast<std::size_t>(2));
    CHECK_EQ(JsonWriter::compact(latin), std::string("\"\\u00e9\""));
}

VF_TEST(brokenSurrogatesAndUnknownEscapesAreRefused) {
    const char* broken[] = {
        "\"\\ud83e\"",
        "\"\\ud83eX\"",
        "\"\\ud83e\\u0041\"",
        "\"\\udd8a\"",
        "\"\\q\"",
        "\"\\u00g1\"",
        "\"\\u00\"",
    };
    for (const char* text : broken) {
        JsonParseResult parsed = parseJson(text);
        CHECK(!parsed.ok);
        CHECK(parsed.error.present());
        CHECK(!parsed.error.message.empty());
    }
}

VF_TEST(everySyntaxErrorNamesItsOwnLineAndColumn) {
    const BadDocument cases[] = {
        {"", 1, 1, "end of input"},
        {"   ", 1, 4, "end of input"},
        {"{\"a\": 1,}", 1, 9, "trailing comma"},
        {"[1, 2,]", 1, 7, "trailing comma"},
        {"{\"a\" 1}", 1, 6, "expected ':'"},
        {"{'a':1}", 1, 2, "quoted member name"},
        {"{\"a\": \"x\" \"b\": 1}", 1, 11, "expected ',' or '}'"},
        {"[1 2]", 1, 4, "expected ',' or ']'"},
        {"01", 1, 2, "leading zero"},
        {"[1.]", 1, 4, "after the decimal point"},
        {"[1e]", 1, 4, "exponent"},
        {"[-]", 1, 3, "digit after the sign"},
        {"\"abc", 1, 5, "end of input inside a string"},
        {"{}garbage", 1, 3, "trailing character"},
        {"[\"a\tb\"]", 1, 4, "control character"},
        {"[\"\xC3\"]", 1, 3, "malformed UTF-8"},
        {"{\n  \"a\": [1, 2],\n  \"b\": nul\n}", 3, 8, "literal"},
        {"[\n  1,\n  2,\n]", 4, 1, "trailing comma"},
        {"{\n \"a\": @\n}", 2, 7, "unexpected character"},
    };
    for (const BadDocument& bad : cases) {
        JsonParseResult parsed = parseJson(bad.text);
        CHECK(!parsed.ok);
        CHECK_EQ(parsed.error.line, bad.line);
        CHECK_EQ(parsed.error.column, bad.column);
        bool mentions = parsed.error.message.find(bad.fragment) != std::string::npos;
        CHECK(mentions);
        if (!mentions || parsed.error.line != bad.line || parsed.error.column != bad.column)
            std::printf("  document [%s] gave %s\n", bad.text, parsed.error.text().c_str());
    }
}

VF_TEST(deepNestingIsReportedAsAnErrorInsteadOfOverflowingTheStack) {
    std::string tooDeep = repeated("[", 200) + repeated("]", 200);
    JsonParseResult parsed = parseJson(tooDeep);
    CHECK(!parsed.ok);
    CHECK(parsed.error.message.find("nesting deeper") != std::string::npos);
    CHECK_EQ(parsed.error.line, 1);
    CHECK_EQ(parsed.error.column, jsonDefaultMaxDepth + 1);

    JsonParseResult runaway = parseJson(repeated("[", 20000));
    CHECK(!runaway.ok);
    CHECK_EQ(runaway.error.column, jsonDefaultMaxDepth + 1);

    JsonParseResult objects = parseJson(repeated("{\"a\":", 100) + "1" + repeated("}", 100));
    CHECK(!objects.ok);
    CHECK(objects.error.message.find("nesting deeper") != std::string::npos);

    std::string atLimit = repeated("[", jsonDefaultMaxDepth) + repeated("]", jsonDefaultMaxDepth);
    CHECK(parseJson(atLimit).ok);
    CHECK(!parseJson("[" + atLimit + "]").ok);

    CHECK(parseJson("[[[1]]]", 3).ok);
    CHECK(!parseJson("[[[[1]]]]", 3).ok);
}

VF_TEST(numbersKeepIntegerExactnessAndFractionsSurviveTheRoundTrip) {
    JsonValue whole = mustParse("9223372036854775807");
    CHECK(whole.isInteger());
    CHECK_EQ(whole.intValue(), 9223372036854775807ll);
    CHECK_EQ(JsonWriter::compact(whole), std::string("9223372036854775807"));

    JsonValue negative = mustParse("-9223372036854775808");
    CHECK(negative.isInteger());
    CHECK_EQ(negative.intValue(), -9223372036854775807ll - 1ll);

    JsonValue huge = mustParse("99999999999999999999");
    CHECK(!huge.isInteger());
    CHECK(huge.numberValue() > 9.9e19);

    JsonValue fraction = mustParse("[0.1,2.5,-1.5e-7,1e308]");
    CHECK_EQ(JsonWriter::compact(fraction), std::string("[0.1,2.5,-1.5e-07,1e+308]"));
    CHECK(parseJson(JsonWriter::compact(fraction)).root == fraction);

    CHECK(!parseJson("1e999").ok);
    CHECK(parseJson("1e999").error.message.find("representable") != std::string::npos);

    JsonValue exponent = mustParse("1e2");
    CHECK_EQ(exponent.numberValue(), 100.0);
    CHECK_EQ(exponent.intValue(), 100);
    CHECK(!exponent.isInteger());
    CHECK_EQ(JsonWriter::compact(exponent), std::string("100.0"));
    CHECK_EQ(JsonWriter::compact(mustParse("-0.0")), std::string("-0.0"));
    CHECK_EQ(JsonWriter::compact(mustParse("-0")), std::string("0"));
}

VF_TEST(indentedFormLaysOutOneMemberPerLine) {
    JsonValue root = mustParse("{\"a\":[1,2],\"b\":{},\"c\":[]}");
    const std::string expected =
        "{\n"
        "  \"a\": [\n"
        "    1,\n"
        "    2\n"
        "  ],\n"
        "  \"b\": {},\n"
        "  \"c\": []\n"
        "}";
    CHECK_EQ(JsonWriter::indented(root, 2), expected);
    CHECK_EQ(JsonWriter::compact(root), std::string("{\"a\":[1,2],\"b\":{},\"c\":[]}"));
    CHECK(parseJson(expected).root == root);

    JsonWriter wide;
    wide.indentWidth = 4;
    CHECK(wide.write(root).find("\n    \"a\": [") != std::string::npos);
}

VF_TEST(objectMembersKeepOrderAndTheLastDuplicateKeyWins) {
    JsonValue root = mustParse("{\"z\":1,\"a\":2,\"z\":3}");
    CHECK_EQ(root.size(), static_cast<std::size_t>(2));
    CHECK_EQ(root.members()[0].first, std::string("z"));
    CHECK_EQ(root.members()[1].first, std::string("a"));
    CHECK_EQ(root.intOr("z", 0), 3);
    CHECK_EQ(JsonWriter::compact(root), std::string("{\"z\":3,\"a\":2}"));

    JsonValue built = JsonValue::makeObject();
    built.set("z", JsonValue::makeInt(3));
    built.set("a", JsonValue::makeInt(2));
    CHECK(built == root);

    JsonValue reordered = JsonValue::makeObject();
    reordered.set("a", JsonValue::makeInt(2));
    reordered.set("z", JsonValue::makeInt(3));
    CHECK(reordered != root);
}

VF_TEST(jsonPointerWalksIntoObjectsAndArrays) {
    JsonValue root = mustParse(
        "{\"foo\":[\"bar\",\"baz\"],\"\":0,\"a/b\":1,\"c%d\":2,\"e^f\":3,\"m~n\":8,"
        "\"nested\":{\"list\":[{\"leaf\":true}]}}");

    CHECK(JsonPointer::find(root, "") == &root);
    const JsonValue* bar = JsonPointer::find(root, "/foo/0");
    CHECK(bar != nullptr);
    CHECK_EQ(bar->stringValue(), std::string("bar"));
    CHECK_EQ(JsonPointer::find(root, "/foo/1")->stringValue(), std::string("baz"));
    CHECK_EQ(JsonPointer::find(root, "/")->intValue(), 0);
    CHECK_EQ(JsonPointer::find(root, "/a~1b")->intValue(), 1);
    CHECK_EQ(JsonPointer::find(root, "/m~0n")->intValue(), 8);
    CHECK(JsonPointer::find(root, "/nested/list/0/leaf")->boolValue());

    CHECK(JsonPointer::find(root, "/foo/2") == nullptr);
    CHECK(JsonPointer::find(root, "/foo/-") == nullptr);
    CHECK(JsonPointer::find(root, "/foo/01") == nullptr);
    CHECK(JsonPointer::find(root, "/missing") == nullptr);
    CHECK(JsonPointer::find(root, "/foo/0/deeper") == nullptr);

    JsonPointer path("/nested/list/0/leaf");
    CHECK(path.valid());
    CHECK_EQ(path.tokens().size(), static_cast<std::size_t>(4));
    CHECK_EQ(path.text(), std::string("/nested/list/0/leaf"));
    CHECK_EQ(JsonPointer("/a~1b").text(), std::string("/a~1b"));
    CHECK_EQ(JsonPointer("/m~0n").text(), std::string("/m~0n"));

    JsonValue* leaf = JsonPointer("/nested/list/0/leaf").resolve(root);
    CHECK(leaf != nullptr);
    *leaf = JsonValue::makeString("changed");
    CHECK_EQ(JsonPointer::find(root, "/nested/list/0/leaf")->stringValue(),
             std::string("changed"));
}

VF_TEST(malformedPointersResolveToNothing) {
    JsonValue root = mustParse("{\"foo\":1}");
    CHECK(!JsonPointer("foo").valid());
    CHECK(!JsonPointer("/foo~").valid());
    CHECK(!JsonPointer("/foo~2").valid());
    CHECK(JsonPointer("foo").resolve(root) == nullptr);
    CHECK(JsonPointer("/foo~2").resolve(root) == nullptr);
    CHECK(JsonPointer("").valid());
    CHECK_EQ(JsonPointer("").text(), std::string(""));
}

VF_TEST(typedReadsFallBackWhenTheFieldIsMissingOrOfTheWrongType) {
    JsonValue root = mustParse(
        "{\"count\":7,\"ratio\":0.5,\"label\":\"stone\",\"flag\":true,\"nothing\":null}");
    CHECK_EQ(root.intOr("count", -1), 7);
    CHECK_EQ(root.numberOr("ratio", 0.0), 0.5);
    CHECK_EQ(root.stringOr("label", "none"), std::string("stone"));
    CHECK(root.boolOr("flag", false));

    CHECK_EQ(root.intOr("absent", -1), -1);
    CHECK_EQ(root.stringOr("count", "fallback"), std::string("fallback"));
    CHECK(root.boolOr("nothing", true));
    CHECK_EQ(root.numberOr("label", 2.5), 2.5);
    CHECK(!root.has("absent"));
    CHECK(root.has("nothing"));
    CHECK(root.find("nothing")->isNull());

    CHECK_EQ(root.find("count")->intOr(0), 7);
    CHECK_EQ(root.find("label")->stringOr("x"), std::string("stone"));
    CHECK_EQ(root.at(0u), nullptr);
    CHECK_EQ(root.size(), static_cast<std::size_t>(5));
    CHECK_EQ(mustParse("[1,2,3]").size(), static_cast<std::size_t>(3));
    CHECK(mustParse("7").empty());
}

VF_TEST(handBuiltDocumentsWriteAndReadBackTheSameWay) {
    JsonValue recipe = JsonValue::makeObject();
    recipe.set("type", JsonValue::makeString("craftingShaped"));
    JsonValue pattern = JsonValue::makeArray();
    pattern.push(JsonValue::makeString("XX"));
    pattern.push(JsonValue::makeString("XX"));
    recipe.set("pattern", std::move(pattern));
    JsonValue key = JsonValue::makeObject();
    key.set("X", JsonValue::makeString("voxelforge:planks"));
    recipe.set("key", std::move(key));
    JsonValue result = JsonValue::makeObject();
    result.set("item", JsonValue::makeString("voxelforge:crafting_table"));
    result.set("count", JsonValue::makeInt(1));
    recipe.set("result", std::move(result));
    recipe.set("mirror", JsonValue::makeBool(false));
    recipe.set("weightHint", JsonValue::makeNumber(0.25));

    std::string text = JsonWriter::compact(recipe);
    CHECK_EQ(text,
             std::string("{\"type\":\"craftingShaped\",\"pattern\":[\"XX\",\"XX\"],"
                         "\"key\":{\"X\":\"voxelforge:planks\"},"
                         "\"result\":{\"item\":\"voxelforge:crafting_table\",\"count\":1},"
                         "\"mirror\":false,\"weightHint\":0.25}"));
    JsonParseResult reread = parseJson(text);
    CHECK(reread.ok);
    CHECK(reread.root == recipe);

    CHECK(recipe.erase("weightHint"));
    CHECK(!recipe.erase("weightHint"));
    CHECK(reread.root != recipe);
}
