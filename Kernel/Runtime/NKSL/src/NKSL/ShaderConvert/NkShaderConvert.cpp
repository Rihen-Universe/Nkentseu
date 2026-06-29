// =============================================================================
// NkShaderConvert.cpp
// Implémentation de NkShaderFileResolver, NkShaderConverter, NkShaderCache.
// =============================================================================
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKFileSystem/NkFile.h"   // I/O via NKFileSystem (pas de fopen CRT dans NKRHI)

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

// ── glslang (GLSL → SPIR-V) ──────────────────────────────────────────────────
#ifdef NK_RHI_GLSLANG_ENABLED
#   include <glslang/Public/ShaderLang.h>
#   include <glslang/Public/ResourceLimits.h>
#   include <SPIRV/GlslangToSpv.h>
#endif

// ── SPIRV-Cross (SPIR-V → GLSL / HLSL / MSL) ─────────────────────────────────
#ifdef NK_RHI_SPIRVCROSS_ENABLED
#   include <spirv_cross/spirv_glsl.hpp>
#   include <spirv_cross/spirv_hlsl.hpp>
#   include <spirv_cross/spirv_msl.hpp>
#endif

// ── Platform: file removal ────────────────────────────────────────────────────
#ifdef _WIN32
#   include <windows.h>
#   include <direct.h>
#   define NK_MKDIR(p) _mkdir(p)
#else
#   include <sys/stat.h>
#   include <dirent.h>
#   define NK_MKDIR(p) mkdir((p), 0755)
#endif

#include "NKLogger/NkLog.h"

namespace nkentseu {

    // =============================================================================
    // Helpers internes
    // =============================================================================

    static std::string ToStd(const NkString& s) { return std::string(s.CStr()); }
    static NkString   FromStd(const std::string& s) { return NkString(s.c_str()); }

    // Récupère la dernière extension (après le dernier '.')
    static std::string LastExt(const std::string& path) {
        auto dot = path.rfind('.');
        return (dot == std::string::npos) ? "" : path.substr(dot + 1);
    }

    // Retire la dernière extension
    static std::string DropLastExt(const std::string& path) {
        auto dot = path.rfind('.');
        return (dot == std::string::npos) ? path : path.substr(0, dot);
    }

    // =============================================================================
    // NkShaderFileResolver
    // =============================================================================

    NkString NkShaderFileResolver::BasePath(const NkString& path) {
        return FromStd(DropLastExt(ToStd(path)));
    }

    NkString NkShaderFileResolver::FormatExt(const NkString& path) {
        return FromStd(LastExt(ToStd(path)));
    }

    NkString NkShaderFileResolver::StageExt(const NkString& path) {
        // Retire la dernière extension puis extrait la nouvelle dernière extension
        std::string base = DropLastExt(ToStd(path));
        return FromStd(LastExt(base));
    }

    NkSLStage NkShaderFileResolver::StageFrom(const NkString& path) {
        std::string ext = ToStd(StageExt(path));
        if (ext == "vert" || ext == "vs")  return NkSLStage::NK_VERTEX;
        if (ext == "frag" || ext == "fs" || ext == "ps") return NkSLStage::NK_FRAGMENT;
        if (ext == "geom" || ext == "gs")  return NkSLStage::NK_GEOMETRY;
        if (ext == "tesc" || ext == "hs")  return NkSLStage::NK_TESS_CONTROL;
        if (ext == "tese" || ext == "ds")  return NkSLStage::NK_TESS_EVAL;
        if (ext == "comp" || ext == "cs")  return NkSLStage::NK_COMPUTE;
        return NkSLStage::NK_VERTEX; // fallback
    }

    NkString NkShaderFileResolver::ResolveVariant(const NkString& path,
                                                const NkString& targetFmtExt) {
        // "shader.vert.glsl" + "spirv" → "shader.vert.spirv"
        std::string base = DropLastExt(ToStd(path)); // "shader.vert"
        return FromStd(base + "." + ToStd(targetFmtExt));
    }

    bool NkShaderFileResolver::FileExists(const NkString& path) {
        return NkFile::Exists(path.CStr());
    }

    NkVector<NkString> NkShaderFileResolver::FindVariants(const NkString& path) {
        NkVector<NkString> result;
        static const char* kFormats[] = { "glsl", "spirv", "spv", "hlsl", "msl", nullptr };
        for (int i = 0; kFormats[i]; ++i) {
            NkString variant = ResolveVariant(path, NkString(kFormats[i]));
            if (FileExists(variant))
                result.PushBack(variant);
        }
        return result;
    }

    // =============================================================================
    // glslang — initialisation one-time
    // =============================================================================

    #ifdef NK_RHI_GLSLANG_ENABLED

    static EShLanguage ToGlslangStage(NkSLStage stage) {
        switch (stage) {
            case NkSLStage::NK_VERTEX:       return EShLangVertex;
            case NkSLStage::NK_FRAGMENT:     return EShLangFragment;
            case NkSLStage::NK_GEOMETRY:     return EShLangGeometry;
            case NkSLStage::NK_TESS_CONTROL: return EShLangTessControl;
            case NkSLStage::NK_TESS_EVAL:    return EShLangTessEvaluation;
            case NkSLStage::NK_COMPUTE:      return EShLangCompute;
            default:                         return EShLangVertex;
        }
    }

    struct GlslangInit {
        GlslangInit()  { glslang::InitializeProcess(); }
        ~GlslangInit() { glslang::FinalizeProcess(); }
    };

    static GlslangInit& GetGlslangInit() {
        static GlslangInit s;
        return s;
    }

    #endif // NK_RHI_GLSLANG_ENABLED

    // =============================================================================
    // NkShaderConverter — capacités
    // =============================================================================

    bool NkShaderConverter::CanGlslToSpirv() {
    #ifdef NK_RHI_GLSLANG_ENABLED
        return true;
    #else
        return false;
    #endif
    }

    bool NkShaderConverter::CanSpirvToGlsl() {
    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        return true;
    #else
        return false;
    #endif
    }

    bool NkShaderConverter::CanSpirvToHlsl() {
    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        return true;
    #else
        return false;
    #endif
    }

    bool NkShaderConverter::CanSpirvToMsl() {
    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        return true;
    #else
        return false;
    #endif
    }

    // =============================================================================
    // NkShaderConverter::GlslToSpirv
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::GlslToSpirv(
        const NkString& glslSource,
        NkSLStage       stage,
        const NkString& debugName)
    {
        NkShaderConvertResult out;

    #ifdef NK_RHI_GLSLANG_ENABLED
        (void)GetGlslangInit(); // assure l'initialisation

        EShLanguage lang = ToGlslangStage(stage);
        glslang::TShader shader(lang);

        const char* src = glslSource.CStr();
        const char* names[1] = { debugName.CStr() };
        shader.setStringsWithLengthsAndNames(&src, nullptr, names, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        shader.setEnvTarget(glslang::EShTargetSpv,   glslang::EShTargetSpv_1_0);
        shader.setEntryPoint("main");

        const TBuiltInResource* resources = GetDefaultResources();
        EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

        if (!shader.parse(resources, 450, false, messages)) {
            out.success = false;
            out.errors  = NkString(shader.getInfoLog());
            return out;
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(messages)) {
            out.success = false;
            out.errors  = NkString(program.getInfoLog());
            return out;
        }

        std::vector<unsigned int> spirvWords;
        glslang::SpvOptions spvOptions;
        spvOptions.generateDebugInfo = false;
        spvOptions.disableOptimizer  = true;
        spvOptions.optimizeSize      = false;

        glslang::GlslangToSpv(*program.getIntermediate(lang), spirvWords, &spvOptions);

        if (spirvWords.empty() || spirvWords[0] != 0x07230203) {
            out.success = false;
            out.errors = NkString("SPIR-V generation failed: invalid magic number");
            return out;
        }

        // Packer les uint32 en bytes
        out.binary.Resize((uint32)(spirvWords.size() * sizeof(unsigned int)));
        memcpy(out.binary.Data(), spirvWords.data(), out.binary.Size());
        out.success = true;

        // LOG : vérifier le magic number
        if (out.SpirvWordCount() > 0) {
            logger.Info("[GlslToSpirv] Generated SPIR-V for {0}: {1} words, magic=0x{2:08x}\n",
                        debugName.CStr(),
                        (unsigned long long)out.SpirvWordCount(),
                        (unsigned long long)out.SpirvWords()[0]);
        }
    #else
        out.success = false;
        out.errors  = NkString("NK_RHI_GLSLANG_ENABLED non défini — glslang non disponible.");
    #endif

        return out;
    }

    // =============================================================================
    // NkShaderConverter::SpirvToGlsl
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::SpirvToGlsl(
        const uint32* spirvWords, uint32 wordCount, NkSLStage stage)
    {
        NkShaderConvertResult out;

    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        try {
            spirv_cross::CompilerGLSL compiler(spirvWords, wordCount);

            // Inspecter les ressources avant de compiler.
            auto resources = compiler.get_shader_resources();

            spirv_cross::CompilerGLSL::Options opts;
            opts.version = 450;
            opts.es      = false;
            // flip_vert_y : pour les VS géométrie dont les matrices sont en convention
            // VK (NDC Y=-1 au top). Deux exceptions ne doivent PAS être flippées :
            //
            // 1. VS "2D pur" (ex. Render2D) : push_constant sans aucun UBO.
            //    La matrice ortho est en convention GL (NDC Y=+1 au top) ; la flipper
            //    l'inverserait. Distinguant : has_pc && !has_ubo.
            //
            // 2. VS depth-only (ex. shadow pass) : aucun varying en sortie (stage_outputs
            //    vide — seul gl_Position est écrit). La convention Y du shadow map doit
            //    rester cohérente avec le sampling PBR (cascadeMats non-flippées) ;
            //    flipper créerait un décalage UV Y → ombres sur les mauvais pixels.
            if (stage == NkSLStage::NK_VERTEX && !resources.stage_inputs.empty()) {
                bool purePC   = !resources.push_constant_buffers.empty()
                             && resources.uniform_buffers.empty();
                bool depthOnly = resources.stage_outputs.empty();
                opts.vertex.flip_vert_y = !purePC && !depthOnly;
            }
            compiler.set_common_options(opts);

            // Renommer la variable push_constant en _PushConstants pour que
            // NkOpenGLCommandBuffer::PushConstants (emulation GL) puisse la trouver.
            for (auto& pc_res : resources.push_constant_buffers) {
                compiler.set_name(pc_res.id, "_PushConstants");
            }
            out.source  = NkString(compiler.compile().c_str());
            out.success = true;
        } catch (const std::exception& e) {
            out.success = false;
            out.errors  = NkString(e.what());
        }
    #else
        out.success = false;
        out.errors  = NkString("NK_RHI_SPIRVCROSS_ENABLED non défini — SPIRV-Cross non disponible.");
    #endif

        return out;
    }

    // =============================================================================
    // NkShaderConverter::SpirvToHlsl
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::SpirvToHlsl(
        const uint32* spirvWords, uint32 wordCount, NkSLStage stage,
        uint32 hlslShaderModel)
    {
        NkShaderConvertResult out;

    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        try {
            spirv_cross::CompilerHLSL compiler(spirvWords, wordCount);
            spirv_cross::CompilerHLSL::Options opts;
            opts.shader_model = hlslShaderModel;
            compiler.set_hlsl_options(opts);

            // ── Registres DX = bindings GLSL (space0, pas de spaces) ───────────────
            // Le device DX indexe sa table fusionnee par le BINDING GLSL (cf. la boucle
            // d'assignation plus bas) => register HLSL == binding GLSL. On garde TOUT en
            // space0 (useSpaces=false) : la root signature DX12 (NkDirectX12Device::
            // CreateRootSignature) declare ses ranges CBV/SRV/UAV/SAMPLER UNIQUEMENT en
            // space0. Si on emettait register(tN, space<set>) (espace = descriptor set),
            // les shaders set=1 (FXAA/Bloom/Blur/SSAO : layout(set=1,binding=0)) liraient
            // t0,space1 -> aucun range root ne couvre space1 -> SRV nulle -> noir/surexpose.
            // Note : l'ancienne COMPACTION par classe (t0,t1,...) est SUPPRIMEE car elle
            // desalignait register vs binding pour les bindings non contigus (skin).
            const bool useSpaces = false;
            spv::ExecutionModel em =
                (stage == NkSLStage::NK_VERTEX)   ? spv::ExecutionModelVertex   :
                (stage == NkSLStage::NK_FRAGMENT) ? spv::ExecutionModelFragment :
                (stage == NkSLStage::NK_COMPUTE)  ? spv::ExecutionModelGLCompute:
                (stage == NkSLStage::NK_GEOMETRY) ? spv::ExecutionModelGeometry :
                                                    spv::ExecutionModelFragment;

            // ── Semantics des attributs de vertex (input layout DX) ────────────────
            // Par defaut SPIRV-Cross nomme TOUS les inputs vertex "TEXCOORD<location>".
            // Or l'input layout du device (NkDirectX12/DX11Device) declare des semantics
            // NOMMEES alignees sur la convention de maillage NKRenderer :
            //   loc0=POSITION loc1=NORMAL loc2=TANGENT loc3=TEXCOORD0 loc4=TEXCOORD1
            //   loc5=COLOR loc6=BLENDINDICES loc7=BLENDWEIGHT
            // Sans remap, CreateInputLayout echoue ("input signature expects TEXCOORD/2
            // but declaration doesn't provide a matching name") -> PSO E_INVALIDARG.
            // Les shaders passant par NkSL (PBR/Shadow/Skybox) generent deja ces noms ;
            // ce remap aligne le chemin SPIRV-Cross (ex. Skin) sur la meme convention.
            if (stage == NkSLStage::NK_VERTEX) {
                static const char* kVtxSemantic[] = {
                    "POSITION", "NORMAL", "TANGENT", "TEXCOORD0", "TEXCOORD1",
                    "COLOR", "BLENDINDICES", "BLENDWEIGHT"
                };
                auto vtxRes = compiler.get_shader_resources();
                for (auto& in : vtxRes.stage_inputs) {
                    uint32 loc = compiler.get_decoration(in.id, spv::DecorationLocation);
                    if (loc < (sizeof(kVtxSemantic) / sizeof(kVtxSemantic[0]))) {
                        spirv_cross::HLSLVertexAttributeRemap rm{};
                        rm.location = loc;
                        rm.semantic = kVtxSemantic[loc];
                        compiler.add_vertex_attribute_remap(rm);
                    }
                }
            }

            struct ResEntry { uint32 set; uint32 binding; spirv_cross::ID id; int cls; }; // cls 0=cbv 1=srv 2=combined 3=sampler 4=uav 5=srv-storage-buffer
            std::vector<ResEntry> entries;
            auto add = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& list, int cls) {
                for (auto& r : list) {
                    uint32 set = compiler.get_decoration(r.id, spv::DecorationDescriptorSet);
                    uint32 bnd = compiler.get_decoration(r.id, spv::DecorationBinding);
                    entries.push_back({set, bnd, r.id, cls});
                }
            };
            auto resources = compiler.get_shader_resources();
            add(resources.uniform_buffers,   0); // CBV
            // Skinning GPU : le SSBO de bones (readonly buffer) devient une SRV
            // StructuredBuffer cote HLSL. Classe DEDIEE (5) : son register t#
            // doit RESTER egal au binding GLSL (pas de compaction), car le device
            // DX11/DX12 binde la SRV du storage buffer par NUMERO DE BINDING brut
            // (et non via le mapping compacte out.dxBindings). Sans ca, bones
            // compacte a t0 cote shader mais le device le binde a t<binding> ->
            // SRV nulle -> bones=0 -> skin a l'origine -> INVISIBLE sur DX.
            add(resources.storage_buffers,   5); // SRV storage buffer (register = binding, pas de compaction)
            add(resources.separate_images,   1); // SRV
            add(resources.sampled_images,    2); // SRV + Sampler (combiné)
            add(resources.separate_samplers, 3); // Sampler
            add(resources.storage_images,    4); // UAV

            // Ordre déterministe (set, binding) pour une assignation reproductible côté device.
            std::sort(entries.begin(), entries.end(), [](const ResEntry& a, const ResEntry& b){
                return (a.set != b.set) ? (a.set < b.set) : (a.binding < b.binding);
            });

            // ── Register HLSL = binding GLSL (PAS de compaction) ──────────────────
            // Le device DX (DX11/DX12) indexe sa table « fusionnee » par le NUMERO
            // DE BINDING GLSL : NkDirectX12CommandBuffer ecrit mMergedCbv/Srv/Samp
            // [b.slot] (b.slot = binding GLSL) puis copie au slot ring OFF_CBV/SRV/
            // SAMP + binding ; la root signature DX12 declare CBV b0, SRV t0, SAMP s0
            // sur NUM_CBV/SRV=32 slots => register(tN) <-> ring slot OFF_SRV+N <->
            // mMergedSrv[N] = ressource de binding N. Donc le HLSL DOIT avoir
            // register == binding GLSL. Toute COMPACTION (t0,t1,... compacts)
            // desaligne register vs binding : ex. skin frag tAlbedo(binding3) compacte
            // a t0 mais le device met l'albedo a mMergedSrv[3]=t3 => t0 lit null => 0
            // (modele skinne texture noir / discard sur alpha=0 tuait le mesh).
            // On EPINGLE donc register = binding pour TOUTES les classes (CBV/SRV/
            // sampler/UAV). Plus de risque X4567 DX11 ici : les shaders passant par
            // SpirvToHlsl (skin, FXAA/Bloom/Blur) ont des bindings bas (<=8) ; les
            // shaders a bindings eleves (25,26,27) passent par NkSL (registres deja
            // epingles), pas par ce chemin. (cls 5 = storage buffer garde t0, cf bas.)
            for (auto& e : entries) {
                spirv_cross::HLSLResourceBinding hb{};
                hb.stage    = em;
                hb.desc_set = e.set;
                hb.binding  = e.binding;
                NkDXResourceBinding map{};
                map.set = e.set; map.binding = e.binding;
                map.space = useSpaces ? e.set : 0u;
                const uint32 reg = e.binding;   // register HLSL == binding GLSL
                switch (e.cls) {
                    case 0: { hb.cbv.register_space=map.space; hb.cbv.register_binding=reg; map.cbvReg=reg; } break;
                    case 1: { hb.srv.register_space=map.space; hb.srv.register_binding=reg; map.srvReg=reg; } break;
                    case 2: { hb.srv.register_space=map.space; hb.srv.register_binding=reg;
                              hb.sampler.register_space=map.space; hb.sampler.register_binding=reg;
                              map.srvReg=reg; map.samplerReg=reg; } break;
                    case 3: { hb.sampler.register_space=map.space; hb.sampler.register_binding=reg; map.samplerReg=reg; } break;
                    case 4: { hb.uav.register_space=map.space; hb.uav.register_binding=reg; map.uavReg=reg; } break;
                    // cls 5 : storage buffer lu en SRV (ByteAddressBuffer).
                    // IMPORTANT : ce SPIRV-Cross assigne les ByteAddressBuffer a
                    // un compteur SRV PROPRE partant de t0, en IGNORANT a la fois
                    // add_hlsl_resource_binding ET la decoration Binding. Les
                    // storage buffers de NKRenderer (bones skin, transforms
                    // instancing) sont TOUJOURS seuls dans leur stage (aucune
                    // texture au meme register dans le meme stage), donc le 1er
                    // (et unique) storage buffer atterrit a t0. On enregistre
                    // donc srvReg=0 dans dxBindings pour refleter la realite ; le
                    // device DX binde la SRV du storage buffer a t0 (cf.
                    // NkDX12/DX11 CommandBuffer : kStorageBufferSrvReg=0).
                    case 5: { hb.srv.register_space=map.space; hb.srv.register_binding=0; map.srvReg=0; } break;
                    default: break;
                }
                compiler.add_hlsl_resource_binding(hb);
                out.dxBindings.PushBack(map);
            }

            std::string hlsl = compiler.compile();

            // ── Flip Y VK->DX (symetrique au flip_vert_y du chemin GL) ─────────────
            // Les sources .vk.glsl sont ecrites en convention Vulkan (NDC Y=-1 au top).
            // Le chemin GL (SpirvToGlsl) applique opts.vertex.flip_vert_y -> SPIRV-Cross
            // emet un « gl_Position.y = -gl_Position.y; » SUPPLEMENTAIRE pour convertir
            // VK->GL. SPIRV-Cross HLSL n'expose AUCUN equivalent : sans ce flip, le HLSL
            // genere garde la convention VK alors que DX a la MEME convention NDC que GL
            // (Y=+1 au top) -> la geometrie sort a l'envers / hors-ecran (mesh skinne
            // INVISIBLE sur DX11/DX12, alors qu'il s'affiche en GL). On reproduit donc
            // ICI le meme flip que le chemin GL, sous les MEMES conditions d'exclusion :
            //   - 2D pur (push_constant sans UBO) : matrice ortho deja en convention GL.
            //   - depth-only (aucun varying) : convention Y du shadow map preservee.
            // PREUVE : dump GL du skin = DEUX « gl_Position.y=-... » (manuel source +
            // flip_vert_y) = net nul ; dump HLSL = UN seul -> net inverse. Ce flip
            // ramene le HLSL au meme net nul que GL (rendu correct).
            if (stage == NkSLStage::NK_VERTEX) {
                auto vtxRes2 = compiler.get_shader_resources();
                bool purePC    = !vtxRes2.push_constant_buffers.empty()
                              && vtxRes2.uniform_buffers.empty();
                bool depthOnly = vtxRes2.stage_outputs.empty();
                if (!purePC && !depthOnly && !vtxRes2.stage_inputs.empty()) {
                    // SPIRV-Cross HLSL assigne la sortie clip dans main() :
                    //   « stage_output.gl_Position = gl_Position; »
                    // On insere le flip JUSTE APRES (sur la sortie SV_Position).
                    const std::string anchor = "stage_output.gl_Position = gl_Position;";
                    std::size_t apos = hlsl.find(anchor);
                    if (apos != std::string::npos) {
                        hlsl.insert(apos + anchor.size(),
                            "\n    stage_output.gl_Position.y = -stage_output.gl_Position.y;");
                    }
                }
            }

            out.source  = NkString(hlsl.c_str());
            out.success = true;
        } catch (const std::exception& e) {
            out.success = false;
            out.errors  = NkString(e.what());
            // Logger l'erreur SPIRV-Cross (sinon elle etait avalee silencieusement et
            // le HLSL vide remontait sans cause visible).
            logger.Errorf("[SpirvToHlsl] SM%u EXCEPTION: %s\n",
                          (unsigned)hlslShaderModel, e.what());
        }
    #else
        out.success = false;
        out.errors  = NkString("NK_RHI_SPIRVCROSS_ENABLED non défini — SPIRV-Cross non disponible.");
    #endif

        return out;
    }

    // =============================================================================
    // NkShaderConverter::SpirvToMsl
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::SpirvToMsl(
        const uint32* spirvWords, uint32 wordCount, NkSLStage /*stage*/)
    {
        NkShaderConvertResult out;

    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        try {
            spirv_cross::CompilerMSL compiler(spirvWords, wordCount);
            spirv_cross::CompilerMSL::Options opts;
            opts.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 0);
            compiler.set_msl_options(opts);
            out.source  = NkString(compiler.compile().c_str());
            out.success = true;
        } catch (const std::exception& e) {
            out.success = false;
            out.errors  = NkString(e.what());
        }
    #else
        out.success = false;
        out.errors  = NkString("NK_RHI_SPIRVCROSS_ENABLED non défini — SPIRV-Cross non disponible.");
    #endif

        return out;
    }

    // =============================================================================
    // NkShaderConverter::LoadFile
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::LoadFile(const NkString& path) {
        NkShaderConvertResult out;
        std::string ext = ToStd(NkShaderFileResolver::FormatExt(path));

        if (!NkFile::Exists(path.CStr())) {
            out.errors = NkString("Impossible d'ouvrir : ") + path;
            return out;
        }

        bool isBinary = (ext == "spirv" || ext == "spv");

        if (isBinary) {
            out.binary = NkFile::ReadAllBytes(path.CStr());
        } else {
            out.source = NkFile::ReadAllText(path.CStr());
        }
        out.success = true;
        return out;
    }

    // =============================================================================
    // NkShaderConverter::LoadAsSpirv
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::LoadAsSpirv(const NkString& path) {
        std::string ext = ToStd(NkShaderFileResolver::FormatExt(path));
        if (ext == "spirv" || ext == "spv") {
            return LoadFile(path);
        }
        if (ext == "glsl") {
            NkShaderConvertResult src = LoadFile(path);
            if (!src.success) return src;
            NkSLStage stage = NkShaderFileResolver::StageFrom(path);
            return GlslToSpirv(src.source, stage, path);
        }
        NkShaderConvertResult out;
        out.errors = NkString("Impossible de charger comme SPIR-V : extension non supportée (") +
                    NkString(ext.c_str()) + NkString(")");
        return out;
    }

    // =============================================================================
    // NkShaderConverter — raccourcis fichier GLSL → texte cible
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::GlslFileToHlsl(const NkString& glslPath, uint32 sm) {
        NkShaderConvertResult spv = LoadAsSpirv(glslPath);
        if (!spv.success) return spv;
        return SpirvToHlsl(spv, NkShaderFileResolver::StageFrom(glslPath), sm);
    }

    NkShaderConvertResult NkShaderConverter::GlslFileToMsl(const NkString& glslPath) {
        NkShaderConvertResult spv = LoadAsSpirv(glslPath);
        if (!spv.success) return spv;
        return SpirvToMsl(spv, NkShaderFileResolver::StageFrom(glslPath));
    }

    NkShaderConvertResult NkShaderConverter::GlslFileToGlsl(const NkString& glslPath) {
        NkShaderConvertResult spv = LoadAsSpirv(glslPath);
        if (!spv.success) return spv;
        return SpirvToGlsl(spv, NkShaderFileResolver::StageFrom(glslPath));
    }

    // =============================================================================
    // NkShaderConverter — conversion GLSL Vulkan-style → cible (memory)
    // Source canonique : GLSL Vulkan (avec layout(set=,binding=), push_constant…).
    // Chaine GlslToSpirv (glslang) → SpirvToHlsl/Msl/Glsl (SPIRV-Cross). En cas
    // d'echec d'une etape, l'erreur est propagee sans appeler la suivante.
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::GlslToHlsl(
        const NkString& glslSource, NkSLStage stage, uint32 hlslShaderModel,
        const NkString& debugName)
    {
        NkShaderConvertResult spv = GlslToSpirv(glslSource, stage, debugName);
        if (!spv.success) return spv;
        return SpirvToHlsl(spv, stage, hlslShaderModel);
    }

    NkShaderConvertResult NkShaderConverter::GlslToMsl(
        const NkString& glslSource, NkSLStage stage, const NkString& debugName)
    {
        NkShaderConvertResult spv = GlslToSpirv(glslSource, stage, debugName);
        if (!spv.success) return spv;
        return SpirvToMsl(spv, stage);
    }

    NkShaderConvertResult NkShaderConverter::GlslToGlsl(
        const NkString& glslSource, NkSLStage stage, const NkString& debugName)
    {
        NkShaderConvertResult spv = GlslToSpirv(glslSource, stage, debugName);
        if (!spv.success) return spv;
        return SpirvToGlsl(spv, stage);
    }

    // =============================================================================
    // NkShaderCache — helpers internes
    // =============================================================================

    static void EnsureDirExists(const std::string& dir) {
        if (dir.empty()) return;
    #ifdef _WIN32
        CreateDirectoryA(dir.c_str(), nullptr);
    #else
        NK_MKDIR(dir.c_str());
    #endif
    }

    // FNV-1a 64-bit
    static uint64 Fnv1a64(const void* data, size_t len, uint64 hash = 14695981039346656037ULL) {
        const uint8* p = static_cast<const uint8*>(data);
        for (size_t i = 0; i < len; ++i) {
            hash ^= (uint64)p[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    static const uint32 kNkscMagic = 0x4353474E; // 'NKSC' little-endian

    // =============================================================================
    // NkShaderCache
    // =============================================================================

    void NkShaderCache::SetCacheDir(const NkString& dir) noexcept {
        mCacheDir = dir;
        EnsureDirExists(ToStd(dir));
    }

    uint64 NkShaderCache::ComputeKey(const NkString& source,
                                    NkSLStage       stage,
                                    const NkString& targetFormat) noexcept {
        uint64 h = 14695981039346656037ULL;
        h = Fnv1a64(source.CStr(), strlen(source.CStr()), h);
        h = Fnv1a64(&stage, sizeof(stage), h);
        h = Fnv1a64(targetFormat.CStr(), strlen(targetFormat.CStr()), h);
        return h;
    }

    NkString NkShaderCache::KeyToPath(uint64 key) const noexcept {
        char buf[32];
        snprintf(buf, sizeof(buf), "%016llx.nksc", (unsigned long long)key);
        std::string dir = ToStd(mCacheDir);
        if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
            dir += '/';
        return FromStd(dir + buf);
    }

    // NkShaderConvertResult NkShaderCache::Load(uint64 key) const noexcept {
    //     NkShaderConvertResult out;
    //     if (mCacheDir.Empty()) return out;

    //     NkString path = KeyToPath(key);
    //     FILE* f = fopen(path.CStr(), "rb");
    //     if (!f) return out;

    //     uint32 magic = 0; uint64 storedKey = 0; uint32 size = 0;
    //     if (fread(&magic,     sizeof(magic),     1, f) != 1 || magic != kNkscMagic ||
    //         fread(&storedKey, sizeof(storedKey), 1, f) != 1 || storedKey != key    ||
    //         fread(&size,      sizeof(size),      1, f) != 1) {
    //         fclose(f);
    //         return out;
    //     }

    //     out.binary.Resize(size);
    //     if (fread(out.binary.Data(), 1, size, f) != size) {
    //         fclose(f);
    //         out.binary.Clear();
    //         return out;
    //     }
    //     fclose(f);
    //     out.success = true;
    //     return out;
    // }

    NkShaderConvertResult NkShaderCache::Load(uint64 key) const noexcept {
        NkShaderConvertResult out;
        if (mCacheDir.Empty()) return out;

        NkString path = KeyToPath(key);
        if (!NkFile::Exists(path.CStr())) return out;
        NkVector<nk_uint8> fbuf = NkFile::ReadAllBytes(path.CStr());
        usize off = 0;
        auto rd = [&](void* p, usize n) -> bool {
            if (off + n > fbuf.Size()) return false;
            memcpy(p, fbuf.Data() + off, n); off += n; return true;
        };

        uint32 magic = 0;
        uint64 storedKey = 0;
        uint32 size = 0;

        if (!rd(&magic, sizeof(magic))         || magic != kNkscMagic ||
            !rd(&storedKey, sizeof(storedKey)) || storedKey != key     ||
            !rd(&size, sizeof(size))) {
            return out;
        }

        // Allouer seulement pour les données SPIR-V, pas pour le header
        out.binary.Resize(size);
        if (size && !rd(out.binary.Data(), size)) {
            out.binary.Clear();
            return out;
        }
        out.success = true;
        // Cache hit -> marquer la cle comme touchee pour la GC.
        MarkTouched(key);
        return out;
    }

    bool NkShaderCache::Save(uint64 key, const NkShaderConvertResult& result) noexcept {
        if (mCacheDir.Empty() || !result.success) return false;
        EnsureDirExists(ToStd(mCacheDir));

        NkString path = KeyToPath(key);

        // Sérialisation binaire en mémoire puis écriture via NKFileSystem (pas de fopen).
        // Header commun : magic(u32) + key(u64) + size(u32) + data.
        NkVector<nk_uint8> fbuf;
        auto wr = [&](const void* p, usize n) {
            const nk_uint8* b = (const nk_uint8*)p;
            for (usize i = 0; i < n; ++i) fbuf.PushBack(b[i]);
        };

        if (!result.binary.IsEmpty()) {
            // Vérifier que c'est bien du SPIR-V valide
            const uint32* words = reinterpret_cast<const uint32*>(result.binary.Data());
            if (result.binary.Size() >= 4 && words[0] != 0x07230203) {
                logger.Info("[ShaderCache] Warning: saving non-SPIRV data (magic=0x{0:08x})\n", words[0]);
            }
            uint32 dataSize = (uint32)result.binary.Size();
            wr(&kNkscMagic, sizeof(kNkscMagic));
            wr(&key,        sizeof(key));
            wr(&dataSize,   sizeof(dataSize));
            wr(result.binary.Data(), dataSize);
        } else {
            std::string srcBuf = ToStd(result.source);
            uint32 size = (uint32)srcBuf.size();
            wr(&kNkscMagic, sizeof(kNkscMagic));
            wr(&key,        sizeof(key));
            wr(&size,       sizeof(size));
            if (size) wr(srcBuf.data(), size);
        }

        if (!NkFile::WriteAllBytes(path.CStr(), fbuf)) return false;
        MarkTouched(key);
        return true;
    }

    void NkShaderCache::Invalidate(uint64 key) noexcept {
        if (mCacheDir.Empty()) return;
        NkString path = KeyToPath(key);
    #ifdef _WIN32
        DeleteFileA(path.CStr());
    #else
        remove(path.CStr());
    #endif
    }

    void NkShaderCache::Clear() noexcept {
        if (mCacheDir.Empty()) return;
        std::string dir = ToStd(mCacheDir);
    #ifdef _WIN32
        WIN32_FIND_DATAA fd;
        std::string pattern = dir + "\\*.nksc";
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                std::string full = dir + "\\" + fd.cFileName;
                DeleteFileA(full.c_str());
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    #else
        DIR* d = opendir(dir.c_str());
        if (d) {
            struct dirent* ent;
            while ((ent = readdir(d))) {
                std::string name = ent->d_name;
                if (name.size() > 5 && name.substr(name.size()-5) == ".nksc") {
                    std::string full = dir + "/" + name;
                    remove(full.c_str());
                }
            }
            closedir(d);
        }
    #endif
    }

    NkShaderCache& NkShaderCache::Global() noexcept {
        static NkShaderCache s;
        return s;
    }

    // ── Tracking session ─────────────────────────────────────────────────────
    void NkShaderCache::MarkTouched(uint64 key) const noexcept {
        // Insertion ordonnee + dedup : on garde mTouchedKeys tri pour lookup
        // O(log n) via binary search. Cout par-Load/Save acceptable (10s
        // d'entrees par session typique).
        if (!mTouchedSorted) {
            mTouchedKeys.PushBack(key);
            mTouchedSorted = false;
            return;
        }
        // binary search insert
        nk_size lo = 0, hi = mTouchedKeys.Size();
        while (lo < hi) {
            nk_size mid = (lo + hi) >> 1u;
            if (mTouchedKeys[mid] < key) lo = mid + 1;
            else                          hi = mid;
        }
        if (lo < mTouchedKeys.Size() && mTouchedKeys[lo] == key) return; // dedup
        mTouchedKeys.PushBack(key);  // append puis on retriera au prochain IsTouched
        mTouchedSorted = false;
    }

    bool NkShaderCache::IsTouched(uint64 key) const noexcept {
        if (!mTouchedSorted) {
            std::sort(mTouchedKeys.begin(), mTouchedKeys.end());
            mTouchedSorted = true;
        }
        nk_size lo = 0, hi = mTouchedKeys.Size();
        while (lo < hi) {
            nk_size mid = (lo + hi) >> 1u;
            if (mTouchedKeys[mid] < key)      lo = mid + 1;
            else if (mTouchedKeys[mid] > key) hi = mid;
            else                              return true;
        }
        return false;
    }

    // ── Purge generique : parcourt le dossier, supprime selon le predicat ────
    uint32 NkShaderCache::PurgeImpl(const NkVector<uint64>& keepKeys,
                                     bool ageCheck, uint64 maxAgeSeconds) noexcept {
        if (mCacheDir.Empty()) return 0;
        // Trier keepKeys pour binary search
        NkVector<uint64> sorted = keepKeys;
        std::sort(sorted.begin(), sorted.end());

        auto isKept = [&](uint64 k) -> bool {
            nk_size lo = 0, hi = sorted.Size();
            while (lo < hi) {
                nk_size mid = (lo + hi) >> 1u;
                if (sorted[mid] < k)      lo = mid + 1;
                else if (sorted[mid] > k) hi = mid;
                else                       return true;
            }
            return false;
        };

        // Now() en secondes Unix epoch (pour ageCheck).
        uint64 nowSec = (uint64)time(nullptr);
        uint32 removed = 0;
        std::string dir = ToStd(mCacheDir);

    #ifdef _WIN32
        WIN32_FIND_DATAA fd;
        std::string pattern = dir + "\\*.nksc";
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                const char* name = fd.cFileName;
                // Parse 16-char hex prefix -> key
                if (strlen(name) < 21) continue; // "%016llx.nksc" = 21 chars
                uint64 key = 0;
                if (sscanf(name, "%16llx.nksc", (unsigned long long*)&key) != 1) continue;

                bool shouldRemove = false;
                if (!keepKeys.Empty()) {
                    shouldRemove = !isKept(key);
                }
                if (ageCheck) {
                    ULARGE_INTEGER ull;
                    ull.LowPart  = fd.ftLastWriteTime.dwLowDateTime;
                    ull.HighPart = fd.ftLastWriteTime.dwHighDateTime;
                    // FILETIME = 100ns intervalles depuis 1601. Conversion en sec Unix.
                    uint64 fileSec = (ull.QuadPart / 10000000ULL) - 11644473600ULL;
                    if (nowSec > fileSec && (nowSec - fileSec) > maxAgeSeconds)
                        shouldRemove = true;
                }
                if (shouldRemove) {
                    std::string full = dir + "\\" + name;
                    if (DeleteFileA(full.c_str())) ++removed;
                }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    #else
        DIR* d = opendir(dir.c_str());
        if (d) {
            struct dirent* ent;
            while ((ent = readdir(d))) {
                std::string name = ent->d_name;
                if (name.size() != 21 || name.substr(name.size()-5) != ".nksc") continue;
                uint64 key = 0;
                if (sscanf(name.c_str(), "%16llx.nksc", (unsigned long long*)&key) != 1) continue;

                std::string full = dir + "/" + name;
                bool shouldRemove = false;
                if (!keepKeys.Empty()) {
                    shouldRemove = !isKept(key);
                }
                if (ageCheck) {
                    struct stat st {};
                    if (stat(full.c_str(), &st) == 0) {
                        uint64 fileSec = (uint64)st.st_mtime;
                        if (nowSec > fileSec && (nowSec - fileSec) > maxAgeSeconds)
                            shouldRemove = true;
                    }
                }
                if (shouldRemove && remove(full.c_str()) == 0) ++removed;
            }
            closedir(d);
        }
    #endif
        return removed;
    }

    uint32 NkShaderCache::PurgeUnused(const NkVector<uint64>& livingKeys) noexcept {
        return PurgeImpl(livingKeys, /*ageCheck=*/false, 0);
    }

    uint32 NkShaderCache::PurgeUnusedThisSession() noexcept {
        // Tri local (la signature de PurgeImpl trie de toute façon).
        if (!mTouchedSorted) {
            std::sort(mTouchedKeys.begin(), mTouchedKeys.end());
            mTouchedSorted = true;
        }
        return PurgeImpl(mTouchedKeys, /*ageCheck=*/false, 0);
    }

    uint32 NkShaderCache::PurgeOlderThan(uint64 maxAgeSeconds) noexcept {
        NkVector<uint64> empty;
        return PurgeImpl(empty, /*ageCheck=*/true, maxAgeSeconds);
    }

} // namespace nkentseu
