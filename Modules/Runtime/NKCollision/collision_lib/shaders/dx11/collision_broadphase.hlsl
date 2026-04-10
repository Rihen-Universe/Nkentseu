// collision_broadphase.hlsl — DirectX 11/12 Compute Shader
// Compile DX11: fxc /T cs_5_0 /E main collision_broadphase.hlsl
// Compile DX12: dxc -T cs_6_0 -E main collision_broadphase.hlsl

struct BodyAABB {
    float3  minXYZ;
    float   _pad0;
    float3  maxXYZ;
    float   _pad1;
    uint2   bodyId;
    uint2   _pad2;
};

struct Pair {
    uint idxA;
    uint idxB;
};

StructuredBuffer<BodyAABB>   aabbs      : register(t0);
RWStructuredBuffer<Pair>     pairs      : register(u0);
RWBuffer<uint>               pairCount  : register(u1);

cbuffer Constants : register(b0) {
    uint uCount;
    uint uMaxPairs;
    uint2 _pad;
};

// ─────────────────────────────────────────────────────────────────────────────
//  DX11 Compute Shader — 2D dispatch (group = 8x8, global = ceil(N/8) x ceil(N/8))
// ─────────────────────────────────────────────────────────────────────────────
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint i = DTid.x;
    uint j = DTid.y;

    if(i >= uCount || j >= uCount || j <= i) return;

    float3 minA = aabbs[i].minXYZ, maxA = aabbs[i].maxXYZ;
    float3 minB = aabbs[j].minXYZ, maxB = aabbs[j].maxXYZ;

    bool overlap =
        (minA.x <= maxB.x) && (maxA.x >= minB.x) &&
        (minA.y <= maxB.y) && (maxA.y >= minB.y) &&
        (minA.z <= maxB.z) && (maxA.z >= minB.z);

    if(overlap) {
        uint idx;
        InterlockedAdd(pairCount[0], 1u, idx);
        if(idx < uMaxPairs) {
            pairs[idx].idxA = i;
            pairs[idx].idxB = j;
        }
    }
}
