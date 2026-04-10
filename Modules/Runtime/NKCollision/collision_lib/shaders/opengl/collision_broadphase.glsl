// collision_broadphase.glsl
// OpenGL Compute Shader — AABB broadphase
// Dispatch: (N/64, N/64, 1) groups, 64x1x1 threads per group
#version 450 core

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct BodyAABB {
    vec4  minPad;   // xyz=min, w=pad
    vec4  maxPad;   // xyz=max, w=pad
    uvec2 bodyId;   // 64-bit body ID split as two uint
    uvec2 pad2;
};

struct Pair {
    uint idxA;
    uint idxB;
};

layout(std430, binding=0) readonly  buffer AABBs   { BodyAABB aabbs[]; };
layout(std430, binding=1) writeonly buffer Pairs   { Pair pairs[]; };
layout(binding=0, offset=0) uniform atomic_uint pairCounter;

uniform uint uCount;
uniform uint uMaxPairs;

void main() {
    uint i = gl_WorkGroupID.x * 64u + gl_LocalInvocationID.x;
    uint j = gl_WorkGroupID.y * 64u + gl_LocalInvocationID.y;

    if(i >= uCount || j >= uCount || j <= i) return;

    vec3 minA = aabbs[i].minPad.xyz;
    vec3 maxA = aabbs[i].maxPad.xyz;
    vec3 minB = aabbs[j].minPad.xyz;
    vec3 maxB = aabbs[j].maxPad.xyz;

    bvec3 overlap = bvec3(
        (minA.x <= maxB.x) && (maxA.x >= minB.x),
        (minA.y <= maxB.y) && (maxA.y >= minB.y),
        (minA.z <= maxB.z) && (maxA.z >= minB.z)
    );

    if(all(overlap)) {
        uint idx = atomicCounterIncrement(pairCounter);
        if(idx < uMaxPairs) {
            pairs[idx].idxA = i;
            pairs[idx].idxB = j;
        }
    }
}
