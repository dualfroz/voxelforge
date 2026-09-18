#include "vf/Portal.h"

#include <cstdlib>

namespace vf {

namespace {

const Vec3i endRingOffsets[PortalNetwork::endFrameSlots] = {
    {-2, 0, -1}, {-2, 0, 0}, {-2, 0, 1},
    {2, 0, -1},  {2, 0, 0},  {2, 0, 1},
    {-1, 0, -2}, {0, 0, -2}, {1, 0, -2},
    {-1, 0, 2},  {0, 0, 2},  {1, 0, 2},
};

std::size_t dimensionIndex(DimensionId id) {
    return static_cast<std::size_t>(id);
}

}

Vec3i PortalFrame::interiorBlock(int across, int up) const {
    if (alongX) return {origin.x + across, origin.y + up, origin.z};
    return {origin.x, origin.y + up, origin.z + across};
}

bool PortalFrame::containsInterior(const Vec3i& pos) const {
    int up = pos.y - origin.y;
    if (up < 0 || up >= height) return false;
    if (alongX) {
        if (pos.z != origin.z) return false;
        int across = pos.x - origin.x;
        return across >= 0 && across < width;
    }
    if (pos.x != origin.x) return false;
    int across = pos.z - origin.z;
    return across >= 0 && across < width;
}

bool PortalFrame::operator==(const PortalFrame& other) const {
    return origin == other.origin && width == other.width && height == other.height &&
           alongX == other.alongX && dimension == other.dimension;
}

bool PortalScan::interiorCell(Block block) {
    return block == Block::Air || block == Block::NetherPortal;
}

PortalScan PortalScan::detect(const World& world, const Vec3i& probe, DimensionId dimension) {
    PortalScan scan;

    for (int attempt = 0; attempt < 2; ++attempt) {
        bool alongX = attempt == 0;
        int plane = alongX ? probe.z : probe.x;
        int seed = alongX ? probe.x : probe.z;

        auto sample = [&](int across, int y) {
            return alongX ? world.blockAt(across, y, plane) : world.blockAt(plane, y, across);
        };

        if (!interiorCell(sample(seed, probe.y))) continue;

        int bottom = probe.y;
        int steps = 0;
        while (steps < PortalFrame::maxHeight && interiorCell(sample(seed, bottom - 1))) {
            --bottom;
            ++steps;
        }
        if (sample(seed, bottom - 1) != Block::Obsidian) continue;

        int left = seed;
        steps = 0;
        while (steps < PortalFrame::maxWidth && interiorCell(sample(left - 1, bottom))) {
            --left;
            ++steps;
        }
        if (sample(left - 1, bottom) != Block::Obsidian) continue;

        int right = seed;
        steps = 0;
        while (steps < PortalFrame::maxWidth && interiorCell(sample(right + 1, bottom))) {
            ++right;
            ++steps;
        }
        if (sample(right + 1, bottom) != Block::Obsidian) continue;

        int width = right - left + 1;
        if (width < PortalFrame::minWidth || width > PortalFrame::maxWidth) continue;

        int top = bottom;
        steps = 0;
        while (steps < PortalFrame::maxHeight && interiorCell(sample(left, top + 1))) {
            ++top;
            ++steps;
        }
        int height = top - bottom + 1;
        if (height < PortalFrame::minHeight || height > PortalFrame::maxHeight) continue;

        PortalFrame frame;
        frame.alongX = alongX;
        frame.dimension = dimension;
        frame.width = width;
        frame.height = height;
        frame.origin = alongX ? Vec3i{left, bottom, plane} : Vec3i{plane, bottom, left};

        if (!intact(world, frame) || !interiorClear(world, frame)) continue;

        scan.found = true;
        scan.frame = frame;
        scan.interiorArea = frame.interiorArea();
        return scan;
    }
    return scan;
}

bool PortalScan::intact(const World& world, const PortalFrame& frame) {
    for (int across = 0; across < frame.width; ++across) {
        Vec3i low = frame.bottomRow(across);
        Vec3i high = frame.topRow(across);
        if (world.blockAt(low.x, low.y, low.z) != Block::Obsidian) return false;
        if (world.blockAt(high.x, high.y, high.z) != Block::Obsidian) return false;
    }
    for (int up = 0; up < frame.height; ++up) {
        Vec3i left = frame.leftColumn(up);
        Vec3i right = frame.rightColumn(up);
        if (world.blockAt(left.x, left.y, left.z) != Block::Obsidian) return false;
        if (world.blockAt(right.x, right.y, right.z) != Block::Obsidian) return false;
    }
    return true;
}

bool PortalScan::interiorClear(const World& world, const PortalFrame& frame) {
    for (int across = 0; across < frame.width; ++across) {
        for (int up = 0; up < frame.height; ++up) {
            Vec3i cell = frame.interiorBlock(across, up);
            if (!interiorCell(world.blockAt(cell.x, cell.y, cell.z))) return false;
        }
    }
    return true;
}

int PortalScan::light(World& world, const PortalFrame& frame) {
    if (!intact(world, frame) || !interiorClear(world, frame)) return 0;

    int lit = 0;
    for (int across = 0; across < frame.width; ++across) {
        for (int up = 0; up < frame.height; ++up) {
            Vec3i cell = frame.interiorBlock(across, up);
            if (world.blockAt(cell.x, cell.y, cell.z) == Block::NetherPortal) continue;
            world.setBlock(cell.x, cell.y, cell.z, Block::NetherPortal);
            ++lit;
        }
    }
    return lit;
}

int PortalScan::douse(World& world, const PortalFrame& frame) {
    int cleared = 0;
    for (int across = 0; across < frame.width; ++across) {
        for (int up = 0; up < frame.height; ++up) {
            Vec3i cell = frame.interiorBlock(across, up);
            if (world.blockAt(cell.x, cell.y, cell.z) != Block::NetherPortal) continue;
            world.setBlock(cell.x, cell.y, cell.z, Block::Air);
            ++cleared;
        }
    }
    return cleared;
}

bool PortalScan::siteUsable(const World& world, const PortalFrame& frame) {
    for (int across = -1; across <= frame.width; ++across) {
        for (int up = -1; up <= frame.height; ++up) {
            Vec3i cell = frame.interiorBlock(across, up);
            if (world.blockAt(cell.x, cell.y, cell.z) == Block::Bedrock) return false;
        }
    }
    return true;
}

void PortalScan::buildFrame(World& world, const PortalFrame& frame) {
    for (int across = -1; across <= frame.width; ++across) {
        for (int up = -1; up <= frame.height; ++up) {
            Vec3i cell = frame.interiorBlock(across, up);
            bool ring = across == -1 || across == frame.width || up == -1 || up == frame.height;
            world.setBlock(cell.x, cell.y, cell.z, ring ? Block::Obsidian : Block::Air);
        }
    }

    int stepX = frame.alongX ? 0 : 1;
    int stepZ = frame.alongX ? 1 : 0;
    for (int side = -1; side <= 1; side += 2) {
        for (int across = 0; across < frame.width; ++across) {
            Vec3i deck = frame.interiorBlock(across, -1);
            world.setBlock(deck.x + stepX * side, deck.y, deck.z + stepZ * side, Block::Obsidian);
            for (int up = 0; up < frame.height; ++up) {
                Vec3i cell = frame.interiorBlock(across, up);
                world.setBlock(cell.x + stepX * side, cell.y, cell.z + stepZ * side, Block::Air);
            }
        }
    }
}

PortalNetwork::PortalNetwork(DimensionSet& set) : transit(set) {
    eyes.resize(static_cast<std::size_t>(DimensionSet::dimensionCount));
}

int PortalNetwork::registerPortal(DimensionId dimension, const PortalFrame& frame) {
    PortalLink link;
    link.id = static_cast<int>(portals.size());
    link.dimension = dimension;
    link.frame = frame;
    link.frame.dimension = dimension;
    portals.push_back(link);
    return link.id;
}

int PortalNetwork::ignite(DimensionId dimension, const Vec3i& probe) {
    World& world = dimensions().dimension(dimension).world();
    PortalScan scan = PortalScan::detect(world, probe, dimension);
    if (!scan.found) return -1;

    PortalScan::light(world, scan.frame);
    for (PortalLink& link : portals) {
        if (link.dimension != dimension || link.frame != scan.frame) continue;
        link.active = true;
        return link.id;
    }
    return registerPortal(dimension, scan.frame);
}

const PortalLink* PortalNetwork::portal(int id) const {
    if (id < 0 || id >= static_cast<int>(portals.size())) return nullptr;
    return &portals[static_cast<std::size_t>(id)];
}

int PortalNetwork::activeCountIn(DimensionId dimension) const {
    int total = 0;
    for (const PortalLink& link : portals)
        if (link.active && link.dimension == dimension) ++total;
    return total;
}

void PortalNetwork::unlink(int id) {
    const PortalLink* link = portal(id);
    if (!link || link->linkedId < 0) return;

    int partner = link->linkedId;
    portals[static_cast<std::size_t>(id)].linkedId = -1;
    if (partner >= 0 && partner < static_cast<int>(portals.size()))
        portals[static_cast<std::size_t>(partner)].linkedId = -1;
}

void PortalNetwork::linkPortals(int firstId, int secondId) {
    if (!portal(firstId) || !portal(secondId)) return;
    unlink(firstId);
    unlink(secondId);
    portals[static_cast<std::size_t>(firstId)].linkedId = secondId;
    portals[static_cast<std::size_t>(secondId)].linkedId = firstId;
}

int PortalNetwork::validate(DimensionId dimension) {
    World& world = dimensions().dimension(dimension).world();
    int destroyed = 0;

    for (std::size_t index = 0; index < portals.size(); ++index) {
        PortalLink& link = portals[index];
        if (!link.active || link.dimension != dimension) continue;
        if (PortalScan::intact(world, link.frame)) continue;

        PortalScan::douse(world, link.frame);
        link.active = false;
        unlink(link.id);
        ++destroyed;
    }
    return destroyed;
}

PortalSearch PortalNetwork::findNearest(DimensionId dimension, const Vec3i& position,
                                        int radius) const {
    PortalSearch search;
    long long best = -1;

    for (const PortalLink& link : portals) {
        if (!link.active || link.dimension != dimension) continue;
        Vec3i entry = link.frame.entry();
        long long dx = entry.x - position.x;
        long long dy = entry.y - position.y;
        long long dz = entry.z - position.z;
        if (std::llabs(dx) > radius || std::llabs(dz) > radius) continue;

        long long distance = dx * dx + dy * dy + dz * dz;
        if (best >= 0 && distance >= best) continue;
        best = distance;
        search.found = true;
        search.portalId = link.id;
        search.position = entry;
    }
    if (search.found) search.distanceSquared = static_cast<int>(best);
    return search;
}

int PortalNetwork::buildPortalNear(DimensionId dimension, const Vec3i& target) {
    Dimension& destination = dimensions().dimension(dimension);
    destination.generateArea(floorDiv(target.x, chunkSize), floorDiv(target.z, chunkSize), 1);

    PortalFrame frame;
    frame.dimension = dimension;
    frame.alongX = true;
    frame.width = 2;
    frame.height = 3;

    const DimensionConfig& cfg = destination.config();
    int radius = DimensionTravel::buildRadiusFor(dimension);
    Vec3i spot;
    bool natural = transit.findSafeSpot(dimension, target, radius, frame.height + 1, spot);
    if (!natural) spot = {target.x, cfg.clampY(target.y), target.z};

    if (spot.y - 1 < cfg.floorY || spot.y + frame.height >= cfg.ceilingY - 1) return -1;

    frame.origin = spot;
    if (!natural || !PortalScan::siteUsable(destination.world(), frame)) {
        transit.carvePlatform(dimension, spot, 3, frame.height + 2);
        if (!PortalScan::siteUsable(destination.world(), frame)) return -1;
    }

    PortalScan::buildFrame(destination.world(), frame);
    if (PortalScan::light(destination.world(), frame) == 0) return -1;
    return registerPortal(dimension, frame);
}

PortalSearch PortalNetwork::travel(int entityId, int sourcePortalId, DimensionId to) {
    PortalSearch search;
    const PortalLink* source = portal(sourcePortalId);
    if (!source || !source->active) return search;
    if (!readyToTravel(entityId)) {
        search.blocked = true;
        return search;
    }

    DimensionId from = source->dimension;
    Vec3i position = source->frame.entry();

    std::vector<Vec3i> candidates;
    std::vector<int> candidateIds;
    for (const PortalLink& link : portals) {
        if (!link.active || link.dimension != to) continue;
        candidates.push_back(link.frame.entry());
        candidateIds.push_back(link.id);
    }

    search.plan = transit.plan(from, to, position, candidates);

    if (!search.plan.needsPortal) {
        std::size_t index = static_cast<std::size_t>(search.plan.nearestIndex);
        search.found = true;
        search.portalId = candidateIds[index];
        search.position = candidates[index];
        search.distanceSquared = search.plan.nearestDistanceSquared;
    } else {
        int raised = buildPortalNear(to, search.plan.target);
        if (raised < 0) return search;
        search.found = true;
        search.built = true;
        search.portalId = raised;
        search.position = portals[static_cast<std::size_t>(raised)].frame.entry();
    }

    linkPortals(sourcePortalId, search.portalId);
    startCooldown(entityId);
    return search;
}

void PortalNetwork::tickEntity(int entityId, bool insidePortal) {
    int& remaining = cooldown[entityId];
    int& charged = standing[entityId];

    if (remaining > 0) {
        --remaining;
        charged = 0;
        return;
    }
    if (insidePortal) ++charged;
    else charged = 0;
}

int PortalNetwork::standingTicks(int entityId) const {
    auto it = standing.find(entityId);
    return it == standing.end() ? 0 : it->second;
}

int PortalNetwork::cooldownTicks(int entityId) const {
    auto it = cooldown.find(entityId);
    return it == cooldown.end() ? 0 : it->second;
}

bool PortalNetwork::readyToTravel(int entityId) const {
    return cooldownTicks(entityId) == 0 && standingTicks(entityId) >= standingThreshold;
}

void PortalNetwork::startCooldown(int entityId) {
    cooldown[entityId] = cooldownAfterTravel;
    standing[entityId] = 0;
}

int PortalNetwork::buildEndFrame(DimensionId dimension, const Vec3i& center) {
    World& world = dimensions().dimension(dimension).world();
    int placed = 0;

    for (const Vec3i& offset : endRingOffsets) {
        Vec3i slot = center + offset;
        world.setBlock(slot.x, slot.y, slot.z, Block::EndPortalFrame);
        ++placed;
    }
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
            world.setBlock(center.x + dx, center.y, center.z + dz, Block::Air);
    return placed;
}

bool PortalNetwork::hasEndEye(DimensionId dimension, const Vec3i& framePos) const {
    const auto& sockets = eyes[dimensionIndex(dimension)];
    return sockets.find(framePos) != sockets.end();
}

bool PortalNetwork::insertEndEye(DimensionId dimension, const Vec3i& framePos) {
    World& world = dimensions().dimension(dimension).world();
    if (world.blockAt(framePos.x, framePos.y, framePos.z) != Block::EndPortalFrame) return false;
    if (!eyes[dimensionIndex(dimension)].insert(framePos).second) return false;

    for (const Vec3i& offset : endRingOffsets) {
        Vec3i center{framePos.x - offset.x, framePos.y - offset.y, framePos.z - offset.z};
        activateEndPortal(dimension, center);
    }
    return true;
}

int PortalNetwork::endEyeCount(DimensionId dimension, const Vec3i& center) const {
    const World& world = dimensions().dimension(dimension).world();
    int filled = 0;

    for (const Vec3i& offset : endRingOffsets) {
        Vec3i slot = center + offset;
        if (world.blockAt(slot.x, slot.y, slot.z) != Block::EndPortalFrame) continue;
        if (hasEndEye(dimension, slot)) ++filled;
    }
    return filled;
}

bool PortalNetwork::endFrameComplete(DimensionId dimension, const Vec3i& center) const {
    return endEyeCount(dimension, center) == endFrameSlots;
}

bool PortalNetwork::activateEndPortal(DimensionId dimension, const Vec3i& center) {
    if (!endFrameComplete(dimension, center)) return false;

    World& world = dimensions().dimension(dimension).world();
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
            world.setBlock(center.x + dx, center.y, center.z + dz, Block::EndPortal);
    return true;
}

}
