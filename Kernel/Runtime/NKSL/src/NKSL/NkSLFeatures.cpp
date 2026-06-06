// =============================================================================
// NkSLFeatures.cpp
// Implémentation des générateurs pour les features avancées NkSL.
// =============================================================================
#include "NkSLFeatures.h"
#include "NkSLCompiler.h"
#include <cstdio>

namespace nkentseu {

// =============================================================================
// HLSL — Mesh shader
// =============================================================================
NkString GenerateMeshShaderHLSL(
    const NkString&           meshEntrySource,
    const NkSLMeshShaderLayout& layout,
    const NkSLCompileOptions&  opts)
{
    NkString out;

    // En-tête
    out += "// Generated NkSL Mesh Shader (HLSL SM6.5+)\n\n";

    // Structs de sortie
    out += "struct MeshVertex {\n";
    out += "    float4 position : SV_Position;\n";
    out += "    float3 normal   : NORMAL;\n";
    out += "    float2 uv       : TEXCOORD0;\n";
    out += "};\n\n";

    out += "struct MeshPrimitive {\n";
    out += "    uint primitiveID : SV_PrimitiveID;\n";
    out += "};\n\n";

    // Attributs du mesh shader
    char buf[256];
    snprintf(buf, sizeof(buf),
             "[NumThreads(%u, %u, %u)]\n"
             "[OutputTopology(\"%s\")]\n",
             layout.groupSizeX, layout.groupSizeY, layout.groupSizeZ,
             layout.topology.CStr());
    out += NkString(buf);

    snprintf(buf, sizeof(buf),
             "void main(\n"
             "    uint  gtid : SV_GroupThreadID,\n"
             "    uint  gid  : SV_GroupID,\n"
             "    out vertices  MeshVertex  verts[%u],\n"
             "    out primitives MeshPrimitive prims[%u],\n"
             "    out indices uint3 tris[%u])\n",
             layout.maxVertices, layout.maxPrimitives, layout.maxPrimitives);
    out += NkString(buf);
    out += "{\n";
    snprintf(buf, sizeof(buf),
             "    SetMeshOutputCounts(%u, %u);\n",
             layout.maxVertices, layout.maxPrimitives);
    out += NkString(buf);

    // Corps généré depuis la source NkSL
    // En pratique on insère le corps de la fonction @entry ici
    out += "    // === User mesh shader code ===\n";
    out += "    " + meshEntrySource + "\n";
    out += "}\n";

    return out;
}

// =============================================================================
// MSL — Mesh shader (Metal 3)
// =============================================================================
NkString GenerateMeshShaderMSL(
    const NkString&           meshEntrySource,
    const NkSLMeshShaderLayout& layout,
    const NkSLCompileOptions&  opts)
{
    NkString out;
    out += "// Generated NkSL Mesh Shader (Metal 3)\n";
    out += "#include <metal_stdlib>\n";
    out += "#include <metal_mesh>\n";
    out += "using namespace metal;\n\n";

    // Structs vertex et primitive
    out += "struct MeshVertex {\n";
    out += "    float4 position [[position]];\n";
    out += "    float3 normal;\n";
    out += "    float2 uv;\n";
    out += "};\n\n";

    out += "struct MeshPrimitive {\n";
    out += "    uint primitiveID [[primitive_id]];\n";
    out += "};\n\n";

    // Type mesh
    char buf[256];
    snprintf(buf, sizeof(buf),
             "using NkMesh = metal::mesh<MeshVertex, MeshPrimitive, %u, %u, topology::%s>;\n\n",
             layout.maxVertices, layout.maxPrimitives,
             layout.topology == "triangle" ? "triangle" : "line");
    out += NkString(buf);

    // Entry point
    snprintf(buf, sizeof(buf),
             "[[mesh, max_total_threads_per_mesh_grid(%u)]]\n"
             "void main_entry(\n"
             "    NkMesh mesh_out,\n"
             "    uint tid [[thread_index_in_threadgroup]],\n"
             "    uint gid [[threadgroup_position_in_grid]])\n",
             layout.groupSizeX);
    out += NkString(buf);
    out += "{\n";
    out += "    // === User mesh shader code ===\n";
    out += "    " + meshEntrySource + "\n";
    out += "}\n";

    return out;
}

// =============================================================================
// HLSL — Tessellation control (hull shader)
// =============================================================================
NkString GenerateTessControlHLSL(
    const NkString&         source,
    const NkSLTessLayout&   layout,
    const NkSLCompileOptions& opts)
{
    NkString out;
    out += "// Generated NkSL Hull Shader (HLSL)\n\n";

    // Structure de données de constante de patch
    uint32 numEdges   = (layout.domain == "tri") ? 3 : (layout.domain=="quad") ? 4 : 2;
    uint32 numInside  = (layout.domain == "tri") ? 1 : (layout.domain=="quad") ? 2 : 0;

    out += "struct HS_PatchData {\n";
    char buf[256];
    snprintf(buf, sizeof(buf),
             "    float edges[%u]  : SV_TessFactor;\n"
             "    float inside[%u] : SV_InsideTessFactor;\n",
             numEdges, numInside);
    out += NkString(buf);
    out += "};\n\n";

    // Patch constant function
    out += "HS_PatchData " + layout.patchConstantFunc + "(\n";
    out += "    InputPatch<VS_Output, " + NkFormat("{0}", layout.outputControlPoints) + "> patch,\n";
    out += "    uint patchID : SV_PrimitiveID)\n";
    out += "{\n";
    out += "    HS_PatchData pd;\n";
    for (uint32 i = 0; i < numEdges; i++) {
        snprintf(buf, sizeof(buf),
                 "    pd.edges[%u] = %.1ff;\n", i, layout.defaultTessFactor);
        out += NkString(buf);
    }
    for (uint32 i = 0; i < numInside; i++) {
        snprintf(buf, sizeof(buf),
                 "    pd.inside[%u] = %.1ff;\n", i, layout.defaultTessFactor);
        out += NkString(buf);
    }
    out += "    return pd;\n";
    out += "}\n\n";

    // Hull shader main
    snprintf(buf, sizeof(buf),
             "[domain(\"%s\")]\n"
             "[partitioning(\"%s\")]\n"
             "[outputtopology(\"%s\")]\n"
             "[outputcontrolpoints(%u)]\n"
             "[patchconstantfunc(\"%s\")]\n",
             layout.domain.CStr(),
             layout.partitioning.CStr(),
             layout.outputTopology.CStr(),
             layout.outputControlPoints,
             layout.patchConstantFunc.CStr());
    out += NkString(buf);
    out += "VS_Output main(\n";
    out += "    InputPatch<VS_Output, " + NkFormat("{0}", layout.outputControlPoints) + "> patch,\n";
    out += "    uint cpID : SV_OutputControlPointID,\n";
    out += "    uint patchID : SV_PrimitiveID)\n";
    out += "{\n";
    out += "    // === User tessellation control code ===\n";
    out += "    " + source + "\n";
    out += "    return patch[cpID];\n";
    out += "}\n";

    return out;
}

// =============================================================================
// HLSL — Tessellation evaluation (domain shader)
// =============================================================================
NkString GenerateTessEvalHLSL(
    const NkString&         source,
    const NkSLTessLayout&   layout,
    const NkSLCompileOptions& opts)
{
    NkString out;
    out += "// Generated NkSL Domain Shader (HLSL)\n\n";

    char buf[256];
    snprintf(buf, sizeof(buf), "[domain(\"%s\")]\n", layout.domain.CStr());
    out += NkString(buf);

    NkString uvwType = (layout.domain == "tri") ? "float3" : "float2";
    out += "VS_Output main(\n";
    out += "    HS_PatchData patchData,\n";
    snprintf(buf, sizeof(buf),
             "    %s domainLoc : SV_DomainLocation,\n", uvwType.CStr());
    out += NkString(buf);
    out += "    const OutputPatch<VS_Output, " +
           NkFormat("{0}", layout.outputControlPoints) + "> patch)\n";
    out += "{\n";
    out += "    VS_Output output;\n";
    out += "    // === User tessellation evaluation code ===\n";
    out += "    " + source + "\n";
    out += "    return output;\n";
    out += "}\n";

    return out;
}

// =============================================================================
// HLSL — Ray tracing (DXR)
// =============================================================================
NkString GenerateRayGenHLSL(const NkString& src, const NkSLCompileOptions& opts) {
    NkString out;
    out += "// Generated NkSL Ray Generation Shader (DXR)\n\n";

    // Structures standard DXR
    out += "struct RayPayload {\n";
    out += "    float4 color;\n";
    out += "    float  distance;\n";
    out += "    int    depth;\n";
    out += "};\n\n";

    out += "struct ShadowPayload {\n";
    out += "    bool hit;\n";
    out += "};\n\n";

    out += "RaytracingAccelerationStructure _scene : register(t0, space0);\n";
    out += "RWTexture2D<float4> _output : register(u0);\n\n";

    out += "[shader(\"raygeneration\")]\n";
    out += "void main()\n";
    out += "{\n";
    out += "    uint2 launchIndex = DispatchRaysIndex().xy;\n";
    out += "    uint2 launchDim   = DispatchRaysDimensions().xy;\n";
    out += "    // === User ray gen code ===\n";
    out += "    " + src + "\n";
    out += "}\n";

    return out;
}

NkString GenerateClosestHitHLSL(const NkString& src, const NkSLCompileOptions& opts) {
    NkString out;
    out += "// Generated NkSL Closest Hit Shader (DXR)\n\n";
    out += "[shader(\"closesthit\")]\n";
    out += "void main(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)\n";
    out += "{\n";
    out += "    float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();\n";
    out += "    float2 bary   = attr.barycentrics;\n";
    out += "    // === User closest hit code ===\n";
    out += "    " + src + "\n";
    out += "}\n";
    return out;
}

NkString GenerateMissHLSL(const NkString& src, const NkSLCompileOptions& opts) {
    NkString out;
    out += "// Generated NkSL Miss Shader (DXR)\n\n";
    out += "[shader(\"miss\")]\n";
    out += "void main(inout RayPayload payload)\n";
    out += "{\n";
    out += "    // === User miss code ===\n";
    out += "    " + src + "\n";
    out += "}\n";
    return out;
}

NkString GenerateAnyHitHLSL(const NkString& src, const NkSLCompileOptions& opts) {
    NkString out;
    out += "// Generated NkSL Any Hit Shader (DXR)\n\n";
    out += "[shader(\"anyhit\")]\n";
    out += "void main(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)\n";
    out += "{\n";
    out += "    // === User any hit code ===\n";
    out += "    " + src + "\n";
    out += "    AcceptHitAndEndSearch();\n";
    out += "}\n";
    return out;
}

// =============================================================================
// Bindless — HLSL (SM6.6 ResourceDescriptorHeap)
// =============================================================================
NkString GenerateBindlessHLSL(const NkSLBindlessConfig& cfg) {
    NkString out;
    out += "// NkSL Bindless Resource Header (HLSL SM6.6+)\n";
    out += "// Usage:\n";
    out += "//   uint texIdx = push_constants.textureIndex;\n";
    out += "//   Texture2D<float4> tex = ResourceDescriptorHeap[texIdx];\n";
    out += "//   SamplerState smp = SamplerDescriptorHeap[push_constants.samplerIndex];\n";
    out += "//   float4 color = tex.Sample(smp, uv);\n\n";

    out += "// Push constants pour les indices de ressources\n";
    out += "struct NkBindlessIndices {\n";
    out += "    uint textureIndex;\n";
    out += "    uint samplerIndex;\n";
    out += "    uint bufferIndex;\n";
    out += "    uint rwTextureIndex;\n";
    out += "};\n";
    out += "ConstantBuffer<NkBindlessIndices> _bindless : register(b0, space1);\n\n";

    out += "// Helper macros\n";
    out += "#define NK_BINDLESS_TEX(idx)    ((Texture2D<float4>)ResourceDescriptorHeap[idx])\n";
    out += "#define NK_BINDLESS_TEX3D(idx)  ((Texture3D<float4>)ResourceDescriptorHeap[idx])\n";
    out += "#define NK_BINDLESS_CUBE(idx)   ((TextureCube<float4>)ResourceDescriptorHeap[idx])\n";
    out += "#define NK_BINDLESS_RWTEX(idx)  ((RWTexture2D<float4>)ResourceDescriptorHeap[idx])\n";
    out += "#define NK_BINDLESS_BUF(T,idx)  ((StructuredBuffer<T>)ResourceDescriptorHeap[idx])\n";
    out += "#define NK_BINDLESS_RWBUF(T,idx)((RWStructuredBuffer<T>)ResourceDescriptorHeap[idx])\n";
    out += "#define NK_BINDLESS_SMP(idx)    ((SamplerState)SamplerDescriptorHeap[idx])\n";
    out += "#define NK_BINDLESS_SMP_CMP(idx)((SamplerComparisonState)SamplerDescriptorHeap[idx])\n";

    return out;
}

// =============================================================================
// Bindless — MSL (Argument Buffers Tier 2)
// =============================================================================
NkString GenerateBindlessMSL(const NkSLBindlessConfig& cfg) {
    NkString out;
    out += "// NkSL Bindless Resource Header (Metal Argument Buffers Tier 2)\n";
    out += "// Usage:\n";
    out += "//   auto tex = resources.textures[push.textureIndex];\n";
    out += "//   float4 color = tex.sample(resources.samplers[push.samplerIndex], uv);\n\n";

    out += "#include <metal_stdlib>\n";
    out += "#include <metal_argument_buffer>\n";
    out += "using namespace metal;\n\n";

    char buf[256];
    snprintf(buf, sizeof(buf),
             "struct NkBindlessResources {\n"
             "    array<texture2d<float>,   %u> textures  [[id(0)]];\n"
             "    array<texture3d<float>,   %u> textures3D[[id(1)]];\n"
             "    array<texturecube<float>,  %u> cubes     [[id(2)]];\n"
             "    array<sampler,            %u> samplers  [[id(3)]];\n"
             "};\n\n",
             cfg.maxDescriptors, cfg.maxDescriptors,
             cfg.maxDescriptors, cfg.maxDescriptors);
    out += NkString(buf);

    out += "// Push constants\n";
    out += "struct NkBindlessIndices {\n";
    out += "    uint textureIndex;\n";
    out += "    uint samplerIndex;\n";
    out += "    uint bufferIndex;\n";
    out += "};\n\n";

    out += "// Helper macros\n";
    out += "#define NK_BINDLESS_TEX(res, idx)   (res.textures[idx])\n";
    out += "#define NK_BINDLESS_SMP(res, idx)   (res.samplers[idx])\n";
    out += "#define NK_BINDLESS_CUBE(res, idx)  (res.cubes[idx])\n";

    return out;
}

} // namespace nkentseu
