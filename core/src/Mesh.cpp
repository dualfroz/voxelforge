#include "vf/Mesh.h"

namespace vf {

namespace {

struct MaskCell {
    bool set = false;
    bool backface = false;
    Block block = Block::Air;
    int tile = 0;

    bool operator==(const MaskCell& o) const {
        return set == o.set && backface == o.backface && block == o.block && tile == o.tile;
    }
    bool operator!=(const MaskCell& o) const { return !(*this == o); }
};

Face faceFor(int axis, bool backface) {
    switch (axis) {
        case 0: return backface ? Face::NegX : Face::PosX;
        case 1: return backface ? Face::NegY : Face::PosY;
        default: return backface ? Face::NegZ : Face::PosZ;
    }
}

void emitQuad(Mesh& mesh, const Vec3& base, const Vec3& du, const Vec3& dv, const Vec3& normal,
              int width, int height, float tile, bool backface) {
    Vec3 p0 = base;
    Vec3 p1 = base + du;
    Vec3 p2 = base + du + dv;
    Vec3 p3 = base + dv;

    uint32_t start = static_cast<uint32_t>(mesh.vertices.size());
    float w = static_cast<float>(width);
    float h = static_cast<float>(height);

    mesh.vertices.push_back({p0.x, p0.y, p0.z, normal.x, normal.y, normal.z, 0.0f, 0.0f, tile});
    mesh.vertices.push_back({p1.x, p1.y, p1.z, normal.x, normal.y, normal.z, w, 0.0f, tile});
    mesh.vertices.push_back({p2.x, p2.y, p2.z, normal.x, normal.y, normal.z, w, h, tile});
    mesh.vertices.push_back({p3.x, p3.y, p3.z, normal.x, normal.y, normal.z, 0.0f, h, tile});

    if (backface) {
        mesh.indices.insert(mesh.indices.end(),
                            {start, start + 2, start + 1, start, start + 3, start + 2});
    } else {
        mesh.indices.insert(mesh.indices.end(),
                            {start, start + 1, start + 2, start, start + 2, start + 3});
    }
}

} // namespace

Mesh greedyMeshChunk(const World& world, const Vec3i& chunkCoord) {
    Mesh mesh;
    const int origin[3] = {chunkCoord.x * chunkSize, chunkCoord.y * chunkSize,
                           chunkCoord.z * chunkSize};
    const int dims[3] = {chunkSize, chunkSize, chunkSize};

    auto voxel = [&](int lx, int ly, int lz) {
        return world.blockAt(origin[0] + lx, origin[1] + ly, origin[2] + lz);
    };

    std::vector<MaskCell> mask(chunkSize * chunkSize);

    for (int d = 0; d < 3; ++d) {
        int u = (d + 1) % 3;
        int v = (d + 2) % 3;
        int x[3] = {0, 0, 0};
        int q[3] = {0, 0, 0};
        q[d] = 1;

        for (x[d] = -1; x[d] < dims[d];) {
            int n = 0;
            for (x[v] = 0; x[v] < dims[v]; ++x[v]) {
                for (x[u] = 0; x[u] < dims[u]; ++x[u], ++n) {
                    Block a = voxel(x[0], x[1], x[2]);
                    Block b = voxel(x[0] + q[0], x[1] + q[1], x[2] + q[2]);

                    bool aFace = (x[d] >= 0) && faceVisible(a, b);
                    bool bFace = (x[d] < dims[d] - 1) && faceVisible(b, a);

                    MaskCell cell;
                    if (aFace && !bFace) {
                        cell.set = true;
                        cell.backface = false;
                        cell.block = a;
                        cell.tile = faceTile(a, faceFor(d, false));
                    } else if (bFace && !aFace) {
                        cell.set = true;
                        cell.backface = true;
                        cell.block = b;
                        cell.tile = faceTile(b, faceFor(d, true));
                    }
                    mask[n] = cell;
                }
            }

            ++x[d];

            n = 0;
            for (int j = 0; j < dims[v]; ++j) {
                for (int i = 0; i < dims[u];) {
                    MaskCell current = mask[n];
                    if (!current.set) {
                        ++i;
                        ++n;
                        continue;
                    }

                    int width = 1;
                    while (i + width < dims[u] && mask[n + width] == current) ++width;

                    int height = 1;
                    bool stop = false;
                    while (j + height < dims[v] && !stop) {
                        for (int k = 0; k < width; ++k) {
                            if (mask[n + k + height * dims[u]] != current) {
                                stop = true;
                                break;
                            }
                        }
                        if (!stop) ++height;
                    }

                    x[u] = i;
                    x[v] = j;

                    Vec3 base{static_cast<float>(origin[0] + x[0]),
                              static_cast<float>(origin[1] + x[1]),
                              static_cast<float>(origin[2] + x[2])};
                    Vec3 du{0, 0, 0};
                    Vec3 dv{0, 0, 0};
                    (&du.x)[u] = static_cast<float>(width);
                    (&dv.x)[v] = static_cast<float>(height);

                    Vec3 normal{0, 0, 0};
                    (&normal.x)[d] = current.backface ? -1.0f : 1.0f;

                    emitQuad(mesh, base, du, dv, normal, width, height,
                             static_cast<float>(current.tile), current.backface);

                    for (int l = 0; l < height; ++l)
                        for (int k = 0; k < width; ++k)
                            mask[n + k + l * dims[u]] = MaskCell{};

                    i += width;
                    n += width;
                }
            }
        }
    }

    return mesh;
}

int vertexAo(bool side1, bool side2, bool corner) {
    if (side1 && side2) return 0;
    return 3 - (static_cast<int>(side1) + static_cast<int>(side2) + static_cast<int>(corner));
}

int culledFaceCount(const World& world, const Vec3i& chunkCoord) {
    const int origin[3] = {chunkCoord.x * chunkSize, chunkCoord.y * chunkSize,
                           chunkCoord.z * chunkSize};
    static const int offsets[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                      {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    int faces = 0;
    for (int lx = 0; lx < chunkSize; ++lx) {
        for (int ly = 0; ly < chunkSize; ++ly) {
            for (int lz = 0; lz < chunkSize; ++lz) {
                Block self = world.blockAt(origin[0] + lx, origin[1] + ly, origin[2] + lz);
                if (isAir(self)) continue;
                for (auto& o : offsets) {
                    Block other = world.blockAt(origin[0] + lx + o[0], origin[1] + ly + o[1],
                                                origin[2] + lz + o[2]);
                    if (faceVisible(self, other)) ++faces;
                }
            }
        }
    }
    return faces;
}

} // namespace vf
