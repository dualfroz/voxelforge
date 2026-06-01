#include "vf/LitMesh.h"

namespace vf {

namespace {

Face faceForDir(int axis, int sign) {
    switch (axis) {
        case 0: return sign > 0 ? Face::PosX : Face::NegX;
        case 1: return sign > 0 ? Face::PosY : Face::NegY;
        default: return sign > 0 ? Face::PosZ : Face::NegZ;
    }
}

void setAxis(int v[3], int axis, int value) { v[axis] = value; }

struct CornerSample {
    float ao;
    float light;
};

} // namespace

ChunkMesh meshChunkLit(const World& world, const Vec3i& chunkCoord, const LightField& light) {
    ChunkMesh out;
    const int origin[3] = {chunkCoord.x * chunkSize, chunkCoord.y * chunkSize,
                           chunkCoord.z * chunkSize};

    auto occludes = [&](int gx, int gy, int gz) {
        return occludesLight(world.blockAt(gx, gy, gz));
    };

    for (int lx = 0; lx < chunkSize; ++lx) {
        for (int ly = 0; ly < chunkSize; ++ly) {
            for (int lz = 0; lz < chunkSize; ++lz) {
                int gx = origin[0] + lx, gy = origin[1] + ly, gz = origin[2] + lz;
                Block self = world.blockAt(gx, gy, gz);
                if (isAir(self)) continue;

                for (int axis = 0; axis < 3; ++axis) {
                    for (int sign = -1; sign <= 1; sign += 2) {
                        int normal[3] = {0, 0, 0};
                        setAxis(normal, axis, sign);
                        Block neighbour = world.blockAt(gx + normal[0], gy + normal[1],
                                                        gz + normal[2]);
                        if (!faceVisible(self, neighbour)) continue;

                        int b = (axis + 1) % 3;
                        int c = (axis + 2) % 3;
                        int airCell[3] = {gx + normal[0], gy + normal[1], gz + normal[2]};
                        int tile = faceTile(self, faceForDir(axis, sign));

                        CornerSample corner[4];
                        static const int cornerBc[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
                        Vec3 pos[4];
                        for (int k = 0; k < 4; ++k) {
                            int bb = cornerBc[k][0];
                            int cc = cornerBc[k][1];
                            int du = bb ? 1 : -1;
                            int dv = cc ? 1 : -1;

                            int s1[3] = {airCell[0], airCell[1], airCell[2]};
                            s1[b] += du;
                            int s2[3] = {airCell[0], airCell[1], airCell[2]};
                            s2[c] += dv;
                            int diag[3] = {airCell[0], airCell[1], airCell[2]};
                            diag[b] += du;
                            diag[c] += dv;

                            bool o1 = occludes(s1[0], s1[1], s1[2]);
                            bool o2 = occludes(s2[0], s2[1], s2[2]);
                            bool od = occludes(diag[0], diag[1], diag[2]);
                            corner[k].ao = static_cast<float>(vertexAo(o1, o2, od)) / 3.0f;

                            float sum = light.combinedAtWorld(airCell[0], airCell[1], airCell[2]);
                            int contributors = 1;
                            if (!o1) { sum += light.combinedAtWorld(s1[0], s1[1], s1[2]); ++contributors; }
                            if (!o2) { sum += light.combinedAtWorld(s2[0], s2[1], s2[2]); ++contributors; }
                            if (!od) { sum += light.combinedAtWorld(diag[0], diag[1], diag[2]); ++contributors; }
                            corner[k].light = sum / static_cast<float>(contributors);

                            int vp[3] = {gx, gy, gz};
                            vp[axis] += (sign > 0) ? 1 : 0;
                            vp[b] += bb;
                            vp[c] += cc;
                            pos[k] = Vec3{static_cast<float>(vp[0]), static_cast<float>(vp[1]),
                                          static_cast<float>(vp[2])};
                        }

                        Mesh& target = isTransparent(self) ? out.transparent : out.opaque;
                        uint32_t start = static_cast<uint32_t>(target.vertices.size());
                        Vec3 nrm{static_cast<float>(normal[0]), static_cast<float>(normal[1]),
                                 static_cast<float>(normal[2])};
                        static const float uvBc[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
                        for (int k = 0; k < 4; ++k) {
                            Vertex vert;
                            vert.px = pos[k].x; vert.py = pos[k].y; vert.pz = pos[k].z;
                            vert.nx = nrm.x; vert.ny = nrm.y; vert.nz = nrm.z;
                            vert.u = uvBc[k][0]; vert.v = uvBc[k][1];
                            vert.tile = static_cast<float>(tile);
                            vert.light = corner[k].light;
                            vert.ao = corner[k].ao;
                            target.vertices.push_back(vert);
                        }

                        bool flip = (corner[0].ao + corner[2].ao) < (corner[1].ao + corner[3].ao);
                        uint32_t triA[6] = {start, start + 1, start + 2, start, start + 2, start + 3};
                        uint32_t triB[6] = {start + 1, start + 2, start + 3,
                                            start + 1, start + 3, start};
                        const uint32_t* tri = flip ? triB : triA;
                        if (sign > 0) {
                            for (int k = 0; k < 6; ++k) target.indices.push_back(tri[k]);
                        } else {
                            for (int k = 0; k < 6; ++k) target.indices.push_back(tri[5 - k]);
                        }
                    }
                }
            }
        }
    }

    return out;
}

} // namespace vf
