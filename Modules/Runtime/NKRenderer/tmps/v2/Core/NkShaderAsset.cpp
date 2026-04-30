#include "NkShaderAsset.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include <cstring>

namespace nkentseu {
    namespace renderer {

        uint64 NkShaderAsset::sIDCounter = 1;

        namespace {
            static uint64 Fnv1a64(const char* s) {
                uint64 h = 1469598103934665603ull;
                if (!s) return h;
                while (*s) {
                    h ^= (uint8)(*s++);
                    h *= 1099511628211ull;
                }
                return h;
            }

            static uint64 Fnv1a64Combine(uint64 h, const char* s) {
                if (!s) return h;
                while (*s) {
                    h ^= (uint8)(*s++);
                    h *= 1099511628211ull;
                }
                return h;
            }

            static bool IsDxApi(NkGraphicsApi api) {
                return api == NkGraphicsApi::NK_GFX_API_D3D11 || api == NkGraphicsApi::NK_GFX_API_D3D12;
            }
        }

        void NkShaderVariantKey::ComputeHash() {
            // Stable hash independent of insertion order.
            NkVector<NkShaderDefine> sorted = defines;
            for (usize i = 0; i < sorted.Size(); ++i) {
                usize minIdx = i;
                for (usize j = i + 1; j < sorted.Size(); ++j) {
                    if (std::strcmp(sorted[j].name.CStr(), sorted[minIdx].name.CStr()) < 0) {
                        minIdx = j;
                    }
                }
                if (minIdx != i) {
                    NkShaderDefine tmp = sorted[i];
                    sorted[i] = sorted[minIdx];
                    sorted[minIdx] = tmp;
                }
            }

            uint64 h = 1469598103934665603ull;
            for (const auto& d : sorted) {
                h = Fnv1a64Combine(h, d.name.CStr());
                h = Fnv1a64Combine(h, "=");
                h = Fnv1a64Combine(h, d.value.CStr());
                h = Fnv1a64Combine(h, ";");
            }
            hash = h;
        }

        NkString NkShaderAsset::InjectDefines(const NkString& src,
                                              const NkVector<NkShaderDefine>& defines) const {
            if (src.Empty() || defines.Empty()) return src;

            const char* c = src.CStr();
            usize len = src.Length();

            usize insertPos = 0;
            if (len >= 8 && std::strncmp(c, "#version", 8) == 0) {
                while (insertPos < len && c[insertPos] != '\n') {
                    ++insertPos;
                }
                if (insertPos < len) {
                    ++insertPos;
                }
            }

            NkString out;
            out.Append(c, insertPos);
            for (const auto& d : defines) {
                out.Append("#define ");
                out.Append(d.name);
                out.Append(" ");
                out.Append(d.value);
                out.Append("\n");
            }
            out.Append(c + insertPos, len - insertPos);
            return out;
        }

        NkShaderDesc NkShaderAsset::BuildShaderDesc(const NkShaderVariantKey& variant,
                                                     NkGraphicsApi api) const {
            NkShaderDesc d;
            d.debugName = mName.CStr();
            mBuildSourceCache.Clear();
            mBuildSourceCache.Reserve(16);

            NkVector<NkShaderDefine> allDefines = mDesc.defines;
            for (const auto& vd : variant.defines) {
                bool replaced = false;
                for (auto& ad : allDefines) {
                    if (ad.name == vd.name) {
                        ad.value = vd.value;
                        replaced = true;
                        break;
                    }
                }
                if (!replaced) {
                    allDefines.PushBack(vd);
                }
            }

            const bool isVk = api == NkGraphicsApi::NK_GFX_API_VULKAN;
            const bool isDx = IsDxApi(api);
            const bool isMetal = api == NkGraphicsApi::NK_GFX_API_METAL;

            const NkString& vGLSL = (isVk && !mDesc.glslVkVertSrc.Empty()) ? mDesc.glslVkVertSrc : mDesc.glslVertSrc;
            const NkString& fGLSL = (isVk && !mDesc.glslVkFragSrc.Empty()) ? mDesc.glslVkFragSrc : mDesc.glslFragSrc;
            const NkString& gGLSL = (isVk && !mDesc.glslVkGeomSrc.Empty()) ? mDesc.glslVkGeomSrc : mDesc.glslGeomSrc;
            const NkString& cGLSL = (isVk && !mDesc.glslVkCompSrc.Empty()) ? mDesc.glslVkCompSrc : mDesc.glslCompSrc;
            auto cacheSource = [&](const NkString& src) -> const char* {
                mBuildSourceCache.PushBack(InjectDefines(src, allDefines));
                return mBuildSourceCache.Back().CStr();
            };

            if (isDx) {
                const NkString& vs = !mDesc.hlslVSSrc.Empty() ? mDesc.hlslVSSrc : mDesc.hlslSrc;
                const NkString& ps = !mDesc.hlslPSSrc.Empty() ? mDesc.hlslPSSrc : mDesc.hlslSrc;
                if (!vs.Empty()) d.AddHLSL(NkShaderStage::NK_VERTEX,   cacheSource(vs), mDesc.vertEntry.CStr());
                if (!ps.Empty()) d.AddHLSL(NkShaderStage::NK_FRAGMENT, cacheSource(ps), mDesc.fragEntry.CStr());
            } else if (isMetal) {
                if (!mDesc.mslSrc.Empty()) {
                    // Use same source for all stages as a fallback for Metal wrappers.
                    d.AddMSL(NkShaderStage::NK_VERTEX,   mDesc.mslSrc.CStr(), mDesc.vertEntry.CStr());
                    d.AddMSL(NkShaderStage::NK_FRAGMENT, mDesc.mslSrc.CStr(), mDesc.fragEntry.CStr());
                } else {
                    if (!vGLSL.Empty()) d.AddGLSL(NkShaderStage::NK_VERTEX,   cacheSource(vGLSL), "main");
                    if (!fGLSL.Empty()) d.AddGLSL(NkShaderStage::NK_FRAGMENT, cacheSource(fGLSL), "main");
                }
            } else {
                if (!vGLSL.Empty()) d.AddGLSL(NkShaderStage::NK_VERTEX,   cacheSource(vGLSL), "main");
                if (!fGLSL.Empty()) d.AddGLSL(NkShaderStage::NK_FRAGMENT, cacheSource(fGLSL), "main");
                if (!gGLSL.Empty()) d.AddGLSL(NkShaderStage::NK_GEOMETRY, cacheSource(gGLSL), "main");
                if (!cGLSL.Empty()) d.AddGLSL(NkShaderStage::NK_COMPUTE,  cacheSource(cGLSL), "main");
            }

            return d;
        }

        bool NkShaderAsset::Compile(NkIDevice* device, const NkShaderAssetDesc& desc,
                                    const NkShaderVariantKey& variant) {
            if (!device || !device->IsValid()) {
                return false;
            }

            Destroy();

            mDevice = device;
            mDesc = desc;
            mName = desc.name;
            mID = { sIDCounter++ };

            NkShaderVariantKey key = variant;
            key.ComputeHash();

            NkShaderDesc shaderDesc = BuildShaderDesc(key, device->GetApi());
            mDefaultShader = device->CreateShader(shaderDesc);
            if (!mDefaultShader.IsValid()) {
                return false;
            }

            VariantEntry ve{};
            ve.shader = mDefaultShader;
            ve.key = key;
            mVariants.PushBack(ve);

            return true;
        }

        NkShaderHandle NkShaderAsset::GetVariant(NkIDevice* device,
                                                  const NkShaderVariantKey& variant) {
            if (!device || !device->IsValid()) return {};
            if (mDevice && mDevice != device) return {};

            NkShaderVariantKey key = variant;
            key.ComputeHash();

            for (const auto& v : mVariants) {
                if (v.key.hash == key.hash) {
                    return v.shader;
                }
            }

            if (!mDevice) {
                mDevice = device;
            }

            NkShaderDesc desc = BuildShaderDesc(key, device->GetApi());
            NkShaderHandle h = device->CreateShader(desc);
            if (!h.IsValid()) {
                return {};
            }

            VariantEntry ve{};
            ve.shader = h;
            ve.key = key;
            mVariants.PushBack(ve);
            if (!mDefaultShader.IsValid()) {
                mDefaultShader = h;
            }
            return h;
        }

        void NkShaderAsset::Destroy() {
            if (mDevice) {
                for (auto& v : mVariants) {
                    if (v.shader.IsValid()) {
                        mDevice->DestroyShader(v.shader);
                    }
                }
            }
            mVariants.Clear();
            mBuildSourceCache.Clear();
            mDefaultShader = {};
            mDesc = {};
            mName.Clear();
            mDevice = nullptr;
            mID = {};
        }

        namespace NkBuiltinShaders {
            static constexpr const char* kWireframeGeom = R"GLSL(
            #version 460 core
            layout(triangles) in;
            layout(line_strip, max_vertices = 6) out;
            void main() {
                for (int i = 0; i < 3; ++i) {
                    gl_Position = gl_in[i].gl_Position; EmitVertex();
                    gl_Position = gl_in[(i+1)%3].gl_Position; EmitVertex();
                    EndPrimitive();
                }
            }
            )GLSL";

            const char* PBRMetallicVert()   { return kGLSLPBRVert; }
            const char* PBRMetallicFrag()   { return kGLSLPBRFrag; }
            const char* PBRSpecGlossFrag()  { return kGLSLPBRFrag; }
            const char* PrincipledFrag()    { return kGLSLPBRFrag; }
            const char* UnlitVert()         { return kGLSLUnlitVert; }
            const char* UnlitFrag()         { return kGLSLUnlitFrag; }
            const char* ToonFrag()          { return kGLSLUnlitFrag; }
            const char* SubsurfaceFrag()    { return kGLSLPBRFrag; }
            const char* HairFrag()          { return kGLSLPBRFrag; }
            const char* ClothFrag()         { return kGLSLPBRFrag; }
            const char* EyeFrag()           { return kGLSLPBRFrag; }
            const char* WaterFrag()         { return kGLSLPBRFrag; }
            const char* GlassFrag()         { return kGLSLPBRFrag; }

            const char* ShadowDepthVert()   { return kGLSLUnlitVert; }
            const char* ShadowDepthFrag()   { return kGLSLUnlitFrag; }
            const char* DepthOnlyVert()     { return kGLSLUnlitVert; }
            const char* DepthOnlyFrag()     { return kGLSLUnlitFrag; }
            const char* Basic3DVert()       { return kGLSLBasic3DVert; }
            const char* Basic3DFrag()       { return kGLSLBasic3DFrag; }
            const char* Basic3DVertVK()     { return kGLSLVulkanBasic3DVert; }
            const char* Basic3DFragVK()     { return kGLSLVulkanBasic3DFrag; }
            const char* Basic3DVertHLSL()   { return kHLSLBasic3DVert; }
            const char* Basic3DFragHLSL()   { return kHLSLBasic3DFrag; }
            const char* Basic3DVertMSL()    { return kMSLBasic3DVert; }
            const char* Basic3DFragMSL()    { return kMSLBasic3DFrag; }
            const char* SkyboxVert()        { return kGLSLSkyboxVert; }
            const char* SkyboxFrag()        { return kGLSLSkyboxFrag; }
            const char* WireframeGeom()     { return kWireframeGeom; }

            const char* FullscreenVert()    { return kGLSLFullscreenVert; }
            const char* TonemapFrag()       { return kGLSLTonemapFrag; }
            const char* BloomFrag()         { return kGLSLTonemapFrag; }
            const char* FXAAFrag()          { return kGLSLTonemapFrag; }
            const char* SSAOFrag()          { return kGLSLTonemapFrag; }
            const char* SSRFrag()           { return kGLSLTonemapFrag; }
            const char* DOFFrag()           { return kGLSLTonemapFrag; }
            const char* MotionBlurFrag()    { return kGLSLTonemapFrag; }
            const char* VignetteChromaticFrag() { return kGLSLTonemapFrag; }
            const char* ColorGradeFrag()    { return kGLSLTonemapFrag; }

            const char* SpriteVert()        { return kGLSLSpriteVert; }
            const char* SpriteFrag()        { return kGLSLSpriteFrag; }
            const char* SpriteVertVK()      { return kGLSLVulkanSpriteVert; }
            const char* SpriteFragVK()      { return kGLSLVulkanSpriteFrag; }
            const char* SpriteVertHLSL()    { return kHLSLSpriteVert; }
            const char* SpriteFragHLSL()    { return kHLSLSpriteFrag; }
            const char* SpriteVertMSL()     { return kMSLSpriteVert; }
            const char* SpriteFragMSL()     { return kMSLSpriteFrag; }
            const char* Text2DVert()        { return kGLSLSpriteVert; }
            const char* Text2DFrag()        { return kGLSLSpriteFrag; }
            const char* Shape2DVert()       { return kGLSLSpriteVert; }
            const char* Shape2DFrag()       { return kGLSLSpriteFrag; }
            const char* Shape2DVertVK()     { return kGLSLVulkanSpriteVert; }
            const char* Shape2DFragVK()     { return kGLSLVulkanSpriteFrag; }
            const char* Shape2DVertHLSL()   { return kHLSLSpriteVert; }
            const char* Shape2DFragHLSL()   { return kHLSLSpriteFrag; }
            const char* Shape2DVertMSL()    { return kMSLSpriteVert; }
            const char* Shape2DFragMSL()    { return kMSLSpriteFrag; }

            const char* Text3DVert()        { return kGLSLUnlitVert; }
            const char* Text3DFrag()        { return kGLSLUnlitFrag; }

            const char* EquirectToCubeVert(){ return kGLSLFullscreenVert; }
            const char* EquirectToCubeFrag(){ return kGLSLTonemapFrag; }
            const char* IrradianceVert()    { return kGLSLFullscreenVert; }
            const char* IrradianceFrag()    { return kGLSLTonemapFrag; }
            const char* PrefilterVert()     { return kGLSLFullscreenVert; }
            const char* PrefilterFrag()     { return kGLSLTonemapFrag; }
            const char* BRDFLUTVert()       { return kGLSLFullscreenVert; }
            const char* BRDFLUTFrag()       { return kGLSLTonemapFrag; }

            const char* ParticleVert()      { return kGLSLSpriteVert; }
            const char* ParticleFrag()      { return kGLSLSpriteFrag; }
            const char* TrailVert()         { return kGLSLSpriteVert; }
            const char* TrailFrag()         { return kGLSLSpriteFrag; }
        }

    } // namespace renderer
} // namespace nkentseu
