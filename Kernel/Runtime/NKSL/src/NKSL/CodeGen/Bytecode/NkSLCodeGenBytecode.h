#pragma once
// =============================================================================
// NkSLCodeGenBytecode.h — Générateur AST NkSL → bytecode NkSLByteProgram
// =============================================================================
// Compile la fonction d'entrée d'un stage en bytecode à registres exécutable par
// NkSLVM (rasterizer Software). Couvre : littéraux, identifiants (in/out/uniform/
// local), swizzles & accès membre UBO, constructeurs vecN/matN, arithmétique,
// builtins (normalize/dot/mix/clamp/pow/texture…), if / for / return.
//
// Convention I/O (alignée sur NkSLByteProgram) :
//   - inputs  : attributs (vertex) ou varyings (fragment), offset-FLOAT par location
//   - outputs : position+varyings (vertex) ou fragColor (fragment), offset-FLOAT
//   - uniforms: membres du 1er bloc UBO, byteOffset std140 (mat4/vec4/float tight)
//   - samplers: par binding (= reflection)
// =============================================================================
#include "NKSL/Core/NkSLAST.h"
#include "NKSL/VM/NkSLByteCode.h"

namespace nkentseu {

    class NkSLCodeGenBytecode {
    public:
        // Compile le programme pour un stage donné. success=false + errors si échec.
        bool Generate(NkSLProgramNode* ast, NkSLStage stage,
                      NkSLByteProgram& out, NkVector<NkSLCompileError>& errors);

    private:
        // ── Catégories de symboles ───────────────────────────────────────────
        enum class SymKind : uint8 { INPUT, OUTPUT, UNIFORM, LOCAL, SAMPLER, UBO_INSTANCE };
        struct Sym {
            SymKind      kind;
            NkSLBaseType type   = NkSLBaseType::NK_FLOAT;
            uint32       offset = 0;   // float-offset (in/out) ou byteOffset (uniform) ou sampler idx
            uint16       reg    = 0;   // registre (local)
            uint8        count  = 1;
            bool         isShadow = false;
        };

        // Table de symboles LINÉAIRE (petit nombre de symboles par shader). Évite
        // le bug de rehash de NkUnorderedMap (Find peut renvoyer null après insert).
        struct SymTable {
            NkVector<NkString> names;
            NkVector<Sym>      syms;
            Sym& operator[](const NkString& n) {
                for (usize i=0;i<names.Size();++i) if (names[i]==n) return syms[i];
                names.PushBack(n); syms.PushBack(Sym{}); return syms[syms.Size()-1];
            }
            Sym* Find(const NkString& n) {
                for (usize i=0;i<names.Size();++i) if (names[i]==n) return &syms[i];
                return nullptr;
            }
            void Clear() { names.Clear(); syms.Clear(); }
        };

        // ── État de génération ───────────────────────────────────────────────
        NkSLByteProgram*               mProg   = nullptr;
        NkVector<NkSLCompileError>*     mErr    = nullptr;
        SymTable                        mSyms;
        uint16                          mNextReg = 0;
        NkString                        mUboInstance;   // nom de l'instance UBO (ex "ubo")

        // ── Helpers ──────────────────────────────────────────────────────────
        uint16 AllocReg();
        int32  AddConst(const NkSLValue& v);
        void   Emit(NkSLOp op, uint16 a=0, uint16 b=0, uint16 c=0, int32 imm=0, uint8 aux=0);
        void   Error(uint32 line, const NkString& msg);

        void   CollectSymbols(NkSLProgramNode* ast, NkSLStage stage);
        NkSLFunctionDeclNode* FindEntry(NkSLProgramNode* ast, NkSLStage stage);

        // Génère une EXPRESSION -> retourne le registre résultat. *outCount = composantes.
        uint16 GenExpr(NkSLNode* n, uint8* outCount);
        // Génère un STATEMENT.
        void   GenStmt(NkSLNode* n);
        // Assignation (lhs = rhs) avec gestion swizzle / output / local.
        void   GenAssign(NkSLNode* lhs, uint16 rhsReg, uint8 rhsCount);

        // Swizzle "xyzw" -> masque ; -1 si invalide.
        static int32 ParseSwizzle(const NkString& s);
        // Builtin NkSL -> opcode ; OP_NOP si inconnu.
        static NkSLOp BuiltinOp(const NkString& name);
        // Constructeur vecN/matN -> count cible ; 0 si pas un constructeur.
        static uint8 ConstructorCount(const NkString& name);
        static NkSLBaseType TypeFromCtor(const NkString& name);
    };

} // namespace nkentseu
