#pragma once

#include <GLES3/gl3.h>

#include "vf/Mesh.h"

namespace vf {

struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLsizei indexCount = 0;
};

GLuint buildProgram(const char* vertexSource, const char* fragmentSource);
GLuint uploadAtlasTexture(int tileSize);

void uploadChunkMesh(GpuMesh& gpu, const Mesh& mesh);
void freeGpuMesh(GpuMesh& mesh);
void drawGpuMesh(const GpuMesh& mesh);

}
