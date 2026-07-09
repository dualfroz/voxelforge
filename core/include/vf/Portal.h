#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Dimension.h"
#include "World.h"

namespace vf {

// Rectangular nether portal opening. `origin` is the lowest corner of the
// hole, never of the obsidian ring. alongX means the opening widens along +x
// and is entered from either side of z; otherwise it widens along +z.
struct PortalFrame {
    Vec3i origin;
    int width = 2;
    int height = 3;
    bool alongX = true;
    DimensionId dimension = DimensionId::Overworld;

    static constexpr int minWidth = 2;
    static constexpr int minHeight = 3;
    static constexpr int maxWidth = 21;
    static constexpr int maxHeight = 21;

    Vec3i interiorBlock(int across, int up) const;
    // Ring block at column `across` of the bottom/top row, or row `up` of the
    // left/right column. Corner blocks are reachable through neither.
    Vec3i bottomRow(int across) const { return interiorBlock(across, -1); }
    Vec3i topRow(int across) const { return interiorBlock(across, height); }
    Vec3i leftColumn(int up) const { return interiorBlock(-1, up); }
    Vec3i rightColumn(int up) const { return interiorBlock(width, up); }

    int interiorArea() const { return width * height; }
    Vec3i center() const { return interiorBlock(width / 2, height / 2); }
    Vec3i entry() const { return interiorBlock(width / 2, 0); }
    bool containsInterior(const Vec3i& pos) const;

    bool operator==(const PortalFrame& other) const;
    bool operator!=(const PortalFrame& other) const { return !(*this == other); }
};

// Result of looking for a frame around a probe position, plus the whole
// static frame vocabulary: verification, lighting, dousing and construction.
struct PortalScan {
    bool found = false;
    PortalFrame frame;
    int interiorArea = 0;
    int litBlocks = 0;

    // Walks outward from `probe` in both candidate planes and returns the
    // first complete obsidian frame that encloses it. Corner blocks are
    // optional, exactly like the four sides are not.
    static PortalScan detect(const World& world, const Vec3i& probe, DimensionId dimension);

    static bool interiorCell(Block block);
    static bool intact(const World& world, const PortalFrame& frame);
    static bool interiorClear(const World& world, const PortalFrame& frame);
    static int light(World& world, const PortalFrame& frame);
    static int douse(World& world, const PortalFrame& frame);

    // Whether the frame's bounding box can be carved (no bedrock in the way).
    static bool siteUsable(const World& world, const PortalFrame& frame);
    // Clears the bounding box and lays the obsidian ring, corners included.
    static void buildFrame(World& world, const PortalFrame& frame);
};

struct PortalLink {
    int id = -1;
    DimensionId dimension = DimensionId::Overworld;
    PortalFrame frame;
    int linkedId = -1;
    bool active = true;
};

struct PortalSearch {
    bool found = false;
    bool built = false;
    bool blocked = false;      // traveller still on cooldown or not charged yet
    int portalId = -1;
    Vec3i position;
    int distanceSquared = 0;
    TravelResult plan;
};

// Registry of lit portals across every dimension, the pairing between them,
// and the per-entity timers that gate travel. Nether portals are searched for
// and, failing that, raised on the far side; end portals are a separate
// twelve-slot frame that ignites once every eye is in.
class PortalNetwork {
public:
    explicit PortalNetwork(DimensionSet& set);

    DimensionSet& dimensions() const { return transit.dimensions(); }

    int registerPortal(DimensionId dimension, const PortalFrame& frame);
    // Finds and lights the frame around `probe`; returns its id, or -1.
    int ignite(DimensionId dimension, const Vec3i& probe);

    const PortalLink* portal(int id) const;
    const std::vector<PortalLink>& links() const { return portals; }
    int activeCountIn(DimensionId dimension) const;

    // Re-checks every registered frame in the dimension and douses the broken
    // ones. Returns how many portals were destroyed.
    int validate(DimensionId dimension);

    PortalSearch findNearest(DimensionId dimension, const Vec3i& position, int radius) const;

    // Full crossing: plans through DimensionTravel, reuses a destination
    // portal inside the search radius or raises a new one, pairs the two and
    // puts the traveller on cooldown.
    PortalSearch travel(int entityId, int sourcePortalId, DimensionId to);
    void linkPortals(int firstId, int secondId);

    void tickEntity(int entityId, bool insidePortal);
    int standingTicks(int entityId) const;
    int cooldownTicks(int entityId) const;
    bool readyToTravel(int entityId) const;
    void startCooldown(int entityId);

    int requiredStandingTicks() const { return standingThreshold; }
    void setRequiredStandingTicks(int ticks) { standingThreshold = ticks; }
    int cooldownLength() const { return cooldownAfterTravel; }
    void setCooldownLength(int ticks) { cooldownAfterTravel = ticks; }

    static constexpr int endFrameSlots = 12;

    // Twelve EndPortalFrame blocks around the 3x3 opening centred on `center`.
    int buildEndFrame(DimensionId dimension, const Vec3i& center);
    bool insertEndEye(DimensionId dimension, const Vec3i& framePos);
    bool hasEndEye(DimensionId dimension, const Vec3i& framePos) const;
    int endEyeCount(DimensionId dimension, const Vec3i& center) const;
    bool endFrameComplete(DimensionId dimension, const Vec3i& center) const;
    bool activateEndPortal(DimensionId dimension, const Vec3i& center);

private:
    int buildPortalNear(DimensionId dimension, const Vec3i& target);
    void unlink(int id);

    DimensionTravel transit;
    std::vector<PortalLink> portals;
    int standingThreshold = 80;
    int cooldownAfterTravel = 300;
    std::unordered_map<int, int> standing;
    std::unordered_map<int, int> cooldown;
    std::vector<std::unordered_set<Vec3i, Vec3iHash>> eyes;
};

} // namespace vf
