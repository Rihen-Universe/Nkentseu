#ifndef __SHADER_H__
#define __SHADER_H__

struct Vertice {
    float x, y, z; // position
    float u, v;  // uv texture coordinates
    float nx, ny, nz; // normals
};

struct DrawData {
    uint mesh;
    uint material;
    uint lod;
    uint indexOffset;
    uint vertexOffset;
    uint transformIndex;
};

#endif /* __SHADER_H__ */
