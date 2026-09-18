#include "Hud.h"

#include <emscripten.h>

#include <cstdio>

#include "vf/Command.h"

namespace vf {

namespace {

EM_JS(void, sendHotbar, (int selected, const char* payload), {
    if (typeof window.renderHotbar === 'function')
        window.renderHotbar(selected, UTF8ToString(payload));
});

EM_JS(void, sendHud, (const char* payload), {
    if (typeof window.renderHud === 'function')
        window.renderHud(JSON.parse(UTF8ToString(payload)));
});

std::string quoted(const std::string& text) {
    std::string out = "\"";
    for (char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    out += '"';
    return out;
}

class JsonObject {
public:
    void field(const char* key, const std::string& text) { add(key, quoted(text)); }
    void field(const char* key, bool flag) { add(key, flag ? "true" : "false"); }
    void field(const char* key, int number) { add(key, std::to_string(number)); }
    void field(const char* key, long long number) { add(key, std::to_string(number)); }
    void field(const char* key, float number) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f", number);
        add(key, buffer);
    }
    std::string close() const { return body + "}"; }

private:
    void add(const char* key, const std::string& value) {
        body += body.size() > 1 ? "," : "";
        body += quoted(key) + ":" + value;
    }

    std::string body = "{";
};

}

void pushHotbar(const Hotbar& hotbar) {
    std::string payload;
    for (int i = 0; i < hotbarSize; ++i) {
        const ItemStack& stack = hotbar.slot(i);
        payload += stack.empty() ? "" : blockDisplayName(stack.block);
        payload += ",";
        payload += std::to_string(stack.empty() ? 0 : stack.count);
        if (i + 1 < hotbarSize) payload += ";";
    }
    sendHotbar(hotbar.selectedIndex(), payload.c_str());
}

void pushHud(const HudFrame& frame) {
    JsonObject json;
    json.field("fps", frame.fps);
    json.field("x", frame.position.x);
    json.field("y", frame.position.y);
    json.field("z", frame.position.z);
    json.field("biome", frame.biome);
    json.field("phase", frame.phase);
    json.field("day", frame.day);
    json.field("hour", frame.hour);
    json.field("minute", frame.minute);
    json.field("moon", frame.moonPhase);
    json.field("skyLight", frame.skyLight);
    json.field("weather", frame.weather);
    json.field("timeScale", frame.timeScale);
    json.field("health", frame.health);
    json.field("maxHealth", frame.maxHealth);
    json.field("hunger", frame.hunger);
    json.field("maxHunger", frame.maxHunger);
    json.field("air", frame.air);
    json.field("maxAir", frame.maxAir);
    json.field("food", frame.food);
    json.field("flying", frame.flying);
    json.field("swimming", frame.swimming);
    json.field("underwater", frame.underwater);
    json.field("passive", frame.passive);
    json.field("hostile", frame.hostile);
    json.field("villagers", frame.villagers);
    json.field("employed", frame.employed);
    json.field("beds", frame.beds);
    json.field("workstations", frame.workstations);
    json.field("waterSites", frame.waterSites);
    json.field("flowingCells", frame.flowingCells);
    json.field("columns", frame.columns);
    json.field("meshes", frame.meshes);
    json.field("pending", frame.pending);
    json.field("target", frame.target);
    json.field("mining", frame.mining);
    json.field("message", frame.message);
    sendHud(json.close().c_str());
}

}
