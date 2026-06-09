#include "vf/PlayerData.h"

namespace vf {

namespace {

const char* playerRootName = "Player";

const NbtTag* findTyped(const NbtTag& compound, const std::string& key, NbtType type) {
    const NbtTag* tag = compound.find(key);
    if (!tag || tag->kind() != type) return nullptr;
    return tag;
}

bool readFloat(const NbtTag& compound, const std::string& key, float& out) {
    const NbtTag* tag = findTyped(compound, key, NbtType::Float);
    if (!tag) return false;
    out = tag->asFloat();
    return true;
}

bool readInt(const NbtTag& compound, const std::string& key, int& out) {
    const NbtTag* tag = findTyped(compound, key, NbtType::Int);
    if (!tag) return false;
    out = tag->asInt();
    return true;
}

}  // namespace

NbtTag playerToNbt(const PlayerData& player) {
    NbtTag root = NbtTag::makeCompound();

    NbtTag position = NbtTag::makeList(NbtType::Double);
    position.listAppend(NbtTag::makeDouble(player.position.x));
    position.listAppend(NbtTag::makeDouble(player.position.y));
    position.listAppend(NbtTag::makeDouble(player.position.z));
    root.put("Pos", std::move(position));

    NbtTag rotation = NbtTag::makeList(NbtType::Float);
    rotation.listAppend(NbtTag::makeFloat(player.yaw));
    rotation.listAppend(NbtTag::makeFloat(player.pitch));
    root.put("Rot", std::move(rotation));

    root.put("Health", NbtTag::makeFloat(player.survival.health));
    root.put("Hunger", NbtTag::makeFloat(player.survival.hunger));
    root.put("Saturation", NbtTag::makeFloat(player.survival.saturation));
    root.put("Exhaustion", NbtTag::makeFloat(player.survival.exhaustion));
    root.put("Air", NbtTag::makeFloat(player.survival.airRemaining));
    root.put("Alive", NbtTag::makeByte(player.survival.alive ? 1 : 0));

    root.put("XpLevel", NbtTag::makeInt(player.xpLevel));
    root.put("XpProgress", NbtTag::makeFloat(player.xpProgress));
    root.put("SelectedSlot", NbtTag::makeInt(player.selectedSlot));

    NbtTag inventory = NbtTag::makeList(NbtType::Compound);
    for (const ItemSlot& slot : player.inventory) {
        NbtTag entry = NbtTag::makeCompound();
        entry.put("Id", NbtTag::makeShort(static_cast<int16_t>(slot.item)));
        entry.put("Count", NbtTag::makeInt(slot.count));
        entry.put("Durability", NbtTag::makeInt(slot.durability));
        inventory.listAppend(std::move(entry));
    }
    root.put("Inventory", std::move(inventory));

    return root;
}

bool playerFromNbt(const NbtTag& root, PlayerData& out) {
    if (root.kind() != NbtType::Compound) return false;

    const NbtTag* position = findTyped(root, "Pos", NbtType::List);
    if (!position || position->listElementType() != NbtType::Double ||
        position->asList().size() != 3)
        return false;
    out.position = {static_cast<float>(position->asList()[0].asDouble()),
                    static_cast<float>(position->asList()[1].asDouble()),
                    static_cast<float>(position->asList()[2].asDouble())};

    const NbtTag* rotation = findTyped(root, "Rot", NbtType::List);
    if (!rotation || rotation->listElementType() != NbtType::Float ||
        rotation->asList().size() != 2)
        return false;
    out.yaw = rotation->asList()[0].asFloat();
    out.pitch = rotation->asList()[1].asFloat();

    if (!readFloat(root, "Health", out.survival.health)) return false;
    if (!readFloat(root, "Hunger", out.survival.hunger)) return false;
    if (!readFloat(root, "Saturation", out.survival.saturation)) return false;
    if (!readFloat(root, "Exhaustion", out.survival.exhaustion)) return false;
    if (!readFloat(root, "Air", out.survival.airRemaining)) return false;

    const NbtTag* alive = findTyped(root, "Alive", NbtType::Byte);
    if (!alive) return false;
    out.survival.alive = alive->asByte() != 0;

    if (!readInt(root, "XpLevel", out.xpLevel)) return false;
    if (!readFloat(root, "XpProgress", out.xpProgress)) return false;
    if (!readInt(root, "SelectedSlot", out.selectedSlot)) return false;

    const NbtTag* inventory = findTyped(root, "Inventory", NbtType::List);
    if (!inventory || inventory->listElementType() != NbtType::Compound) return false;
    out.inventory.clear();
    out.inventory.reserve(inventory->asList().size());
    for (const NbtTag& entry : inventory->asList()) {
        const NbtTag* idTag = findTyped(entry, "Id", NbtType::Short);
        const NbtTag* countTag = findTyped(entry, "Count", NbtType::Int);
        const NbtTag* durabilityTag = findTyped(entry, "Durability", NbtType::Int);
        if (!idTag || !countTag || !durabilityTag) return false;
        ItemSlot slot;
        slot.item = static_cast<ItemId>(static_cast<uint16_t>(idTag->asShort()));
        slot.count = countTag->asInt();
        slot.durability = durabilityTag->asInt();
        out.inventory.push_back(slot);
    }

    return true;
}

ByteBuffer savePlayer(const PlayerData& player) {
    std::vector<uint8_t> raw = serializeNbt(playerRootName, playerToNbt(player));
    return ByteBuffer(raw.begin(), raw.end());
}

bool loadPlayer(const uint8_t* data, std::size_t size, PlayerData& out) {
    std::string rootName;
    NbtTag root;
    if (!deserializeNbt(data, size, rootName, root)) return false;
    if (rootName != playerRootName) return false;
    return playerFromNbt(root, out);
}

} // namespace vf
