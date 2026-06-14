#pragma once
// =============================================================================
// NkSLByteCode.h — ISA du bytecode NkSL (cible NK_BYTECODE)
// =============================================================================
// Représentation intermédiaire COMPACTE à registres, émise depuis l'AST NkSL par
// NkSLCodeGenBytecode et exécutée par NkSLVM (rasterizer Software). Remplace à
// terme les lambdas C++ écrites à la main dans NkSWShaderBridge.
//
// Modèle de valeur : tout est un vecteur de floats (jusqu'à 16 = mat4).
//   - scalaire (float/bool/int) : count=1, v[0]
//   - vecN                       : count=N, v[0..N-1]
//   - mat3                       : count=9  (column-major, col0=v[0..2], …)
//   - mat4                       : count=16 (column-major)
//   bool/int stockés comme float (0.0/1.0 ; conversions explicites via OP_FLOOR…).
//
// Modèle d'exécution : machine à registres (pas de pile d'opérandes). Chaque
// instruction lit/écrit des registres. Le contrôle de flux utilise des sauts
// vers des indices d'instruction (résolus à la génération).
//
// I/O (contrat avec l'hôte NkSLVMEnv) : inputs/outputs sont des tableaux PLATS de
// floats indexés par OFFSET-composante (assigné par le générateur, dans l'ordre
// des locations). Les uniforms sont un blob d'octets (UBO) lu par byteOffset.
// =============================================================================
#include "NKSL/Core/NkSLTypes.h"

namespace nkentseu {

    // ── Valeur runtime (jusqu'à mat4) ────────────────────────────────────────
    struct NkSLValue {
        float  v[16] = {0};
        uint8  count = 1;     // nb de floats valides (1,2,3,4,9,16)

        static NkSLValue Scalar(float x) { NkSLValue r; r.count=1; r.v[0]=x; return r; }
    };

    // ── Opcodes ──────────────────────────────────────────────────────────────
    // Convention opérandes : a=dest, b/c=sources, imm=index/immédiat selon l'op.
    enum class NkSLOp : uint16 {
        OP_NOP = 0,

        // Chargement / déplacement
        OP_LOADK,        // a = const[imm]
        OP_MOV,          // a = b
        OP_LOAD_IN,      // a = inputs[imm..imm+count) ; count dans 'aux'
        OP_LOAD_UNI,     // a = uniforms@byteOffset(imm) ; count dans 'aux'
        OP_STORE_OUT,    // outputs[imm..] = b ; count dans 'aux'

        // Construction / accès composantes
        OP_CONSTRUCT,    // a = vec<aux>( regs b..b+imm-1 concaténés ) ; imm=nb args, aux=count cible
        OP_SWIZZLE,      // a = b.swizzle(imm) ; imm = masque 4×2bits, aux=count cible
        OP_WRITE_COMP,   // a[composantes du masque imm] = b   (assignation swizzle/index, aux=count masque)
        OP_INDEX,        // a = b[ (int)c ]  (colonne de matrice -> vecN, ou composante)

        // Arithmétique (composante par composante, broadcast scalaire)
        OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_NEG, OP_MOD,
        OP_MATMUL,       // a = b * c  (mat*mat, mat*vec, vec*mat selon counts)

        // Comparaisons / logique (résultat scalaire 0/1)
        OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ, OP_NE,
        OP_AND, OP_OR, OP_NOT,

        // Builtins maths
        OP_DOT, OP_CROSS, OP_NORMALIZE, OP_LENGTH, OP_DISTANCE,
        OP_REFLECT, OP_REFRACT, OP_MIX, OP_CLAMP, OP_SATURATE,
        OP_STEP, OP_SMOOTHSTEP, OP_POW, OP_SQRT, OP_INVSQRT,
        OP_ABS, OP_SIGN, OP_FLOOR, OP_CEIL, OP_FRACT, OP_MODF,
        OP_MIN, OP_MAX, OP_EXP, OP_LOG, OP_EXP2, OP_LOG2,
        OP_SIN, OP_COS, OP_TAN, OP_ASIN, OP_ACOS, OP_ATAN,
        OP_RADIANS, OP_DEGREES, OP_TRANSPOSE, OP_INVERSE,

        // Textures (samplerIdx = imm)
        OP_TEX_SAMPLE,        // a = sample2D(imm, b.xy [, lod=c.x])
        OP_TEX_SAMPLE_SHADOW, // a = shadow2D(imm, b.xyz)  (xy=uv, z=compare) -> scalaire
        OP_TEX_SIZE,          // a = textureSize(imm) -> vec2

        // Contrôle de flux
        OP_JMP,          // pc = imm
        OP_JZ,           // si b.x == 0 : pc = imm
        OP_JNZ,          // si b.x != 0 : pc = imm
        OP_DISCARD,      // fragment : rejeter
        OP_RET,          // fin du shader

        OP_COUNT
    };

    // ── Instruction ──────────────────────────────────────────────────────────
    struct NkSLInstr {
        NkSLOp   op  = NkSLOp::OP_NOP;
        uint16   a   = 0;   // registre destination
        uint16   b   = 0;   // source 1
        uint16   c   = 0;   // source 2
        int32    imm = 0;   // constante / index in/uni/out / sampler / cible saut / masque
        uint8    aux = 0;   // count auxiliaire (composantes I/O, taille construct…)
    };

    // ── Symbole d'I/O (offset-composante dans le tableau plat) ────────────────
    struct NkSLVMSymbol {
        NkString name;
        uint32   offset    = 0;  // offset en FLOATS (in/out) ou en OCTETS (uniform)
        uint8    count     = 1;  // nb de composantes (floats)
        uint32   location  = 0;  // location/ordre déclaré (in/out)
    };

    struct NkSLVMSampler {
        NkString name;
        uint32   index    = 0;   // slot de binding (= reflection)
        bool     isShadow = false;
    };

    // ── Programme bytecode (un stage) ─────────────────────────────────────────
    struct NkSLByteProgram {
        NkSLStage                stage = NkSLStage::NK_VERTEX;
        NkVector<NkSLInstr>      code;
        NkVector<NkSLValue>      constants;
        uint32                   regCount = 0;   // nb de registres requis

        NkVector<NkSLVMSymbol>   inputs;         // attributs (vertex) ou varyings (fragment)
        NkVector<NkSLVMSymbol>   outputs;        // position+varyings (vertex) ou fragColor (fragment)
        NkVector<NkSLVMSymbol>   uniforms;       // membres d'UBO (par byteOffset)
        NkVector<NkSLVMSampler>  samplers;

        uint32 inputFloats  = 0;  // taille du tableau d'inputs (floats)
        uint32 outputFloats = 0;  // taille du tableau d'outputs (floats)

        bool IsValid() const { return !code.Empty(); }
    };

    // ── Helpers de masque swizzle (4 composantes × 2 bits) ────────────────────
    inline int32 NkSLMakeSwizzle(uint8 a, uint8 b=0xFF, uint8 c=0xFF, uint8 d=0xFF) {
        // chaque composante : 0..3 = x/y/z/w, 0xFF = absente. On encode count + indices.
        uint32 n = 1;
        if (b != 0xFF) n = 2;
        if (c != 0xFF) n = 3;
        if (d != 0xFF) n = 4;
        uint32 m = (a & 3) | ((b & 3) << 2) | ((c & 3) << 4) | ((d & 3) << 6) | (n << 8);
        return (int32)m;
    }
    inline uint32 NkSLSwizzleCount(int32 mask) { return (uint32)((mask >> 8) & 7); }
    inline uint8  NkSLSwizzleIdx(int32 mask, uint32 i) { return (uint8)((mask >> (i*2)) & 3); }

    // ── Environnement d'exécution (fourni par l'hôte : device Software) ────────
    // Tableaux plats remplis/lus par l'hôte selon le layout des symboles ci-dessus.
    struct NkSLVMEnv {
        const float* inputs   = nullptr;  // floats d'entrée  (taille = inputFloats)
        float*       outputs  = nullptr;  // floats de sortie (taille = outputFloats)
        const uint8* uniforms = nullptr;  // blob UBO

        // Échantillonnage texture (callbacks vers NkSWTexture). ctx = opaque hôte.
        void* ctx = nullptr;
        void  (*sampleTex)   (void* ctx, int sampler, float u, float v, float lod, float out[4]) = nullptr;
        float (*sampleShadow)(void* ctx, int sampler, float u, float v, float z)                 = nullptr;
        void  (*texSize)     (void* ctx, int sampler, float out[2])                              = nullptr;

        bool discarded = false;  // mis à true par OP_DISCARD
    };

} // namespace nkentseu
