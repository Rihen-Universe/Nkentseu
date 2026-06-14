#pragma once
// =============================================================================
// NkSLVM.h — Machine virtuelle d'exécution du bytecode NkSL (rasterizer Software)
// =============================================================================
// Interprète un NkSLByteProgram (un stage) pour UN vertex ou UN fragment, en
// lisant/écrivant les tableaux plats de l'environnement NkSLVMEnv.
//
// Usage typique (device Software) :
//   NkSLVMEnv env;  env.inputs=...; env.outputs=...; env.uniforms=...; env.sampleTex=...;
//   NkSLVM::Execute(fragProgram, env);   // remplit env.outputs (ou env.discarded)
// =============================================================================
#include "NKSL/VM/NkSLByteCode.h"

namespace nkentseu {

    class NkSLVM {
    public:
        // Exécute `prog` avec `env`. Les sorties sont écrites dans env.outputs.
        // Retourne false si le fragment a été rejeté (OP_DISCARD), true sinon.
        static bool Execute(const NkSLByteProgram& prog, NkSLVMEnv& env);
    };

} // namespace nkentseu
