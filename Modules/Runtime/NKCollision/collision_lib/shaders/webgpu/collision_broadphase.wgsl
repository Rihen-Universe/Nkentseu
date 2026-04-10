// collision_broadphase.wgsl — WebGPU Shading Language compute shader

struct BodyAABB {
    minPad : vec4<f32>,
    maxPad : vec4<f32>,
    bodyId : vec2<u32>,
    pad2   : vec2<u32>,
};
struct Pair { idxA : u32, idxB : u32 };
struct Counter { count : atomic<u32> };
struct Uniforms { totalCount : u32, maxPairs : u32 };

@group(0) @binding(0) var<storage, read>       aabbs    : array<BodyAABB>;
@group(0) @binding(1) var<storage, read_write> pairs    : array<Pair>;
@group(0) @binding(2) var<storage, read_write> counter  : Counter;
@group(0) @binding(3) var<uniform>             uniforms : Uniforms;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid : vec3<u32>) {
    let i = gid.x;
    let j = gid.y;
    let n = uniforms.totalCount;

    if(i >= n || j >= n || j <= i) { return; }

    let minA = aabbs[i].minPad.xyz;
    let maxA = aabbs[i].maxPad.xyz;
    let minB = aabbs[j].minPad.xyz;
    let maxB = aabbs[j].maxPad.xyz;

    let overlap = all(minA <= maxB) && all(maxA >= minB);

    if(overlap) {
        let idx = atomicAdd(&counter.count, 1u);
        if(idx < uniforms.maxPairs) {
            pairs[idx].idxA = i;
            pairs[idx].idxB = j;
        }
    }
}
