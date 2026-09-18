#include "Storage.h"

#include <emscripten.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "Base64.h"
#include "vf/Region.h"

namespace vf {

namespace {

EM_JS(void, storeText, (const char* text), {
    try { localStorage.setItem('voxelforgeRegions', UTF8ToString(text)); } catch (e) {}
});

EM_JS(char*, readText, (), {
    var text = String.fromCharCode();
    try {
        var stored = localStorage.getItem('voxelforgeRegions');
        if (stored) text = stored;
    } catch (e) {}
    var size = lengthBytesUTF8(text) + 1;
    var ptr = _malloc(size);
    stringToUTF8(text, ptr, size);
    return ptr;
});

}

std::size_t saveTerrain(const World& world) {
    std::vector<uint8_t> bytes = serializeRegions(world);
    storeText(base64Encode(bytes).c_str());
    return bytes.size();
}

LoadOutcome loadTerrain(World& world) {
    char* text = readText();
    std::string encoded(text);
    std::free(text);
    if (encoded.empty()) return LoadOutcome::NothingSaved;
    std::vector<uint8_t> bytes = base64Decode(encoded);
    if (!deserializeRegions(bytes.data(), bytes.size(), world)) return LoadOutcome::Damaged;
    return LoadOutcome::Loaded;
}

}
