#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Dimension.h"
#include "World.h"

namespace vf {

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

struct PortalScan {
    bool found = false;
    PortalFrame frame;
    int interiorArea = 0;
    int litBlocks = 0;

    static PortalScan detect(const World& world, const Vec3i& probe, DimensionId dimension);

    static bool interiorCell(Block block);
    static bool intact(const World& world, const PortalFrame& frame);
    static bool interiorClear(const World& world, const PortalFrame& frame);
    static int light(World& world, const PortalFrame& frame);
    static int douse(World& world, const PortalFrame& frame);

    static bool siteUsable(const World& world, const PortalFrame& frame);
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
    bool blocked = false;
    int portalId = -1;
    Vec3i position;
    int distanceSquared = 0;
    TravelResult plan;
};

class PortalNetwork {
public:
    explicit PortalNetwork(DimensionSet& set);

    DimensionSet& dimensions() const { return transit.dimensions(); }

    int registerPortal(DimensionId dimension, const PortalFrame& frame);
    int ignite(DimensionId dimension, const Vec3i& probe);

    const PortalLink* portal(int id) const;
    const std::vector<PortalLink>& links() const { return portals; }
    int activeCountIn(DimensionId dimension) const;

    int validate(DimensionId dimension);

    PortalSearch findNearest(DimensionId dimension, const Vec3i& position, int radius) const;

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

}
