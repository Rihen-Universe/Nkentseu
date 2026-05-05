#include "NkDescs.h"
// #include "NkSkSL.h"

#include "NKLogger/NkLog.h"

namespace nkentseu {
    // static void _StoreSWFns(NkVertexShaderSoftware&& vFn, NkPixelShaderSoftware&& fFn, NkShaderDesc& stages) {
    //     // Stocker les deux fonctions dans des NkShaderStageDesc avec hasSwFn=true
    //     {
    //         NkShaderStageDesc d;
    //         d.stage    = NkShaderStage::NK_VERTEX;
    //         d.hasSwFn  = true;
    //         d.swVertFn = traits::NkMove(vFn);
    //         stages.AddStage(traits::NkMove(d));
    //     }
    //     {
    //         NkShaderStageDesc d;
    //         d.stage    = NkShaderStage::NK_FRAGMENT;
    //         d.hasSwFn  = true;
    //         d.swFragFn = traits::NkMove(fFn);
    //         stages.AddStage(traits::NkMove(d));
    //     }
    // }

    // bool NkShaderDesc::AddSWSkSLFile(const char* nsklPath, uint32 stride) {
    //     auto r = sksl::NkCompiler::CompileFile(nsklPath, stride);
    //     if (!r.success) {
    //         logger.Errorf("[Shader] AddSWSkSLFile '%s': %s", nsklPath, r.error.CStr());
    //         return false;
    //     }
    //     _StoreSWFns(traits::NkMove(r.vertFn), traits::NkMove(r.fragFn), *this);
    //     return true;
    // }

    // // Option B : depuis un blob binaire en mémoire (assets embarqués / chargés)
    // bool NkShaderDesc::AddSWSkSL(const uint8* data, usize size, uint32 stride) {
    //     sksl::NkBinary bin;
    //     if (!sksl::NkBinary::Deserialize(data, size, bin)) {
    //         logger.Errorf("[Shader] AddSWSkSL: invalid blob");
    //         return false;
    //     }
    //     auto r = sksl::NkCompiler::Compile(bin, stride);
    //     if (!r.success) {
    //         logger.Errorf("[Shader] AddSWSkSL compile: %s", r.error.CStr());
    //         return false;
    //     }
    //     _StoreSWFns(traits::NkMove(r.vertFn), traits::NkMove(r.fragFn), *this);
    //     return true;
    // }

    // // Option C : depuis source SkSL inline (dev / prototypage)
    // bool NkShaderDesc::AddSWSkSLSource(const char* skslText, uint32 stride) {
    //     auto r = sksl::NkCompiler::CompileSrc(skslText, stride);
    //     if (!r.success) {
    //         logger.Errorf("[Shader] AddSWSkSLSource: %s", r.error.CStr());
    //         return false;
    //     }
    //     _StoreSWFns(traits::NkMove(r.vertFn), traits::NkMove(r.fragFn), *this);
    //     return true;
    // }
}