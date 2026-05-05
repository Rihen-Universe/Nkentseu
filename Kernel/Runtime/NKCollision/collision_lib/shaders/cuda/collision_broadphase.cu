// collision_broadphase.cu
// GPU broadphase: each thread tests one pair of bodies
// Grid is (N/64) x (N/64), block is 64x1

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdint.h>

struct GPUBodyAABB {
    float    minX, minY, minZ, _pad0;
    float    maxX, maxY, maxZ, _pad1;
    uint64_t bodyId;
    uint32_t _pad2[2];
};

struct GPUPair {
    uint32_t idxA;
    uint32_t idxB;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Broadphase kernel: N²/2 AABB pair tests, one thread per (i,j) pair
//  Uses atomic counter for output compaction
// ─────────────────────────────────────────────────────────────────────────────
__global__ void broadphaseKernel(
    const GPUBodyAABB* __restrict__ aabbs,
    const uint32_t                  count,
    GPUPair*                        pairs,
    const uint32_t                  maxPairs,
    uint32_t*                       pairCount)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j = blockIdx.y * blockDim.y + threadIdx.y;

    if(i >= count || j >= count || j <= i) return;

    const GPUBodyAABB& a = aabbs[i];
    const GPUBodyAABB& b = aabbs[j];

    // SIMD-friendly AABB overlap test (branchless)
    bool overlap =
        (a.minX <= b.maxX) & (a.maxX >= b.minX) &
        (a.minY <= b.maxY) & (a.maxY >= b.minY) &
        (a.minZ <= b.maxZ) & (a.maxZ >= b.minZ);

    if(overlap) {
        uint32_t idx = atomicAdd(pairCount, 1u);
        if(idx < maxPairs) {
            pairs[idx].idxA = i;
            pairs[idx].idxB = j;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Optimized broadphase using shared memory + sorting
//  Block-level AABB merge to skip whole blocks early
// ─────────────────────────────────────────────────────────────────────────────
__global__ void broadphaseKernelOpt(
    const GPUBodyAABB* __restrict__ aabbs,
    const uint32_t                  count,
    GPUPair*                        pairs,
    const uint32_t                  maxPairs,
    uint32_t*                       pairCount)
{
    const int BLOCK = 64;
    __shared__ GPUBodyAABB sharedA[BLOCK];
    __shared__ GPUBodyAABB sharedB[BLOCK];

    int tx = threadIdx.x;
    int gx = blockIdx.x * BLOCK;
    int gy = blockIdx.y * BLOCK;

    // Load block A
    if(gx+tx < (int)count) sharedA[tx] = aabbs[gx+tx];
    // Load block B
    if(gy+tx < (int)count) sharedB[tx] = aabbs[gy+tx];
    __syncthreads();

    // Block-level AABB test (skip if blocks don't overlap at all)
    // Compute merged AABB for each block using warp reductions
    float blockAminX = sharedA[0].minX, blockAmaxX = sharedA[0].maxX;
    float blockBminX = sharedB[0].minX, blockBmaxX = sharedB[0].maxX;
    // ... (simplified: do full test per thread pair)

    for(int bj=0; bj<BLOCK; bj++) {
        int j = gy + bj;
        int i = gx + tx;
        if(i >= (int)count || j >= (int)count || j <= i) continue;

        const GPUBodyAABB& a = sharedA[tx];
        const GPUBodyAABB& b = sharedB[bj];

        bool overlap =
            (a.minX <= b.maxX) & (a.maxX >= b.minX) &
            (a.minY <= b.maxY) & (a.maxY >= b.minY) &
            (a.minZ <= b.maxZ) & (a.maxZ >= b.minZ);

        if(overlap) {
            uint32_t idx = atomicAdd(pairCount, 1u);
            if(idx < maxPairs) {
                pairs[idx].idxA = (uint32_t)i;
                pairs[idx].idxB = (uint32_t)j;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sphere-Sphere narrowphase on GPU
//  One thread per pair; fills contact buffer
// ─────────────────────────────────────────────────────────────────────────────
struct GPUContact {
    float    posAx, posAy, posAz, _p0;
    float    posBx, posBy, posBz, _p1;
    float    normX, normY, normZ, depth;
    uint64_t bodyA;
    uint64_t bodyB;
};

__global__ void narrowphaseSphereKernel(
    const GPUBodyAABB* __restrict__ aabbs,
    const GPUPair*    __restrict__ pairs,
    const uint32_t                  pairCount,
    GPUContact*                     contacts,
    const uint32_t                  maxContacts,
    uint32_t*                       contactCount)
{
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    if(tid >= pairCount) return;

    const GPUPair&    p = pairs[tid];
    const GPUBodyAABB& a = aabbs[p.idxA];
    const GPUBodyAABB& b = aabbs[p.idxB];

    // Approximate as sphere test using AABB centers + half-extents as radii
    float cAx=(a.minX+a.maxX)*0.5f, cAy=(a.minY+a.maxY)*0.5f, cAz=(a.minZ+a.maxZ)*0.5f;
    float cBx=(b.minX+b.maxX)*0.5f, cBy=(b.minY+b.maxY)*0.5f, cBz=(b.minZ+b.maxZ)*0.5f;
    float rA = sqrtf((a.maxX-a.minX)*(a.maxX-a.minX) +
                     (a.maxY-a.minY)*(a.maxY-a.minY) +
                     (a.maxZ-a.minZ)*(a.maxZ-a.minZ)) * 0.5f;
    float rB = sqrtf((b.maxX-b.minX)*(b.maxX-b.minX) +
                     (b.maxY-b.minY)*(b.maxY-b.minY) +
                     (b.maxZ-b.minZ)*(b.maxZ-b.minZ)) * 0.5f;

    float dx=cBx-cAx, dy=cBy-cAy, dz=cBz-cAz;
    float distSq = dx*dx+dy*dy+dz*dz;
    float radSum = rA+rB;

    if(distSq < radSum*radSum) {
        float dist = sqrtf(distSq);
        float invDist = dist>1e-6f ? 1.f/dist : 0.f;
        float nx=dx*invDist, ny=dy*invDist, nz=dz*invDist;
        float depth = radSum - dist;

        uint32_t idx = atomicAdd(contactCount, 1u);
        if(idx < maxContacts) {
            GPUContact& c = contacts[idx];
            c.posAx=cAx+nx*rA; c.posAy=cAy+ny*rA; c.posAz=cAz+nz*rA;
            c.posBx=cBx-nx*rB; c.posBy=cBy-ny*rB; c.posBz=cBz-nz*rB;
            c.normX=nx; c.normY=ny; c.normZ=nz; c.depth=depth;
            c.bodyA=a.bodyId; c.bodyB=b.bodyId;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Host-callable launchers
// ─────────────────────────────────────────────────────────────────────────────
extern "C" {

uint32_t col_cuda_broadphase(
    const GPUBodyAABB* d_aabbs, uint32_t count,
    GPUPair* d_pairs, uint32_t maxPairs)
{
    uint32_t* d_count;
    cudaMalloc(&d_count, sizeof(uint32_t));
    cudaMemset(d_count, 0, sizeof(uint32_t));

    dim3 block(32, 32, 1);
    dim3 grid((count+31)/32, (count+31)/32, 1);
    broadphaseKernel<<<grid,block>>>(d_aabbs,count,d_pairs,maxPairs,d_count);
    cudaDeviceSynchronize();

    uint32_t result;
    cudaMemcpy(&result, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost);
    cudaFree(d_count);
    return result;
}

uint32_t col_cuda_narrowphase(
    const GPUBodyAABB* d_aabbs,
    const GPUPair* d_pairs, uint32_t pairCount,
    GPUContact* d_contacts, uint32_t maxContacts)
{
    if(pairCount==0) return 0;

    uint32_t* d_count;
    cudaMalloc(&d_count, sizeof(uint32_t));
    cudaMemset(d_count, 0, sizeof(uint32_t));

    dim3 block(128,1,1);
    dim3 grid((pairCount+127)/128,1,1);
    narrowphaseSphereKernel<<<grid,block>>>(
        d_aabbs,d_pairs,pairCount,d_contacts,maxContacts,d_count);
    cudaDeviceSynchronize();

    uint32_t result;
    cudaMemcpy(&result, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost);
    cudaFree(d_count);
    return result;
}

} // extern "C"
