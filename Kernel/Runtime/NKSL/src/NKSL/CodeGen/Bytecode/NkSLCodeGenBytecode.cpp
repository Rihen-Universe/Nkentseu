// =============================================================================
// NkSLCodeGenBytecode.cpp — AST NkSL → bytecode NkSLByteProgram
// =============================================================================
#include "NKSL/CodeGen/Bytecode/NkSLCodeGenBytecode.h"
#include "NKSL/CodeGen/NkSLCodeGen.h"   // NkSLBaseTypeComponents, NkSLBaseTypeSize

namespace nkentseu {

    // ── Helpers bas niveau ───────────────────────────────────────────────────
    uint16 NkSLCodeGenBytecode::AllocReg() { return mNextReg++; }

    int32 NkSLCodeGenBytecode::AddConst(const NkSLValue& v) {
        mProg->constants.PushBack(v);
        return (int32)mProg->constants.Size() - 1;
    }

    void NkSLCodeGenBytecode::Emit(NkSLOp op, uint16 a, uint16 b, uint16 c, int32 imm, uint8 aux) {
        NkSLInstr in; in.op=op; in.a=a; in.b=b; in.c=c; in.imm=imm; in.aux=aux;
        mProg->code.PushBack(in);
    }

    void NkSLCodeGenBytecode::Error(uint32 line, const NkString& msg) {
        mErr->PushBack({line, 0, "", msg, true});
    }

    // ── std140 : alignement / taille d'un type de base ───────────────────────
    static void Std140(NkSLBaseType t, uint32& align, uint32& size) {
        switch (t) {
            case NkSLBaseType::NK_FLOAT: case NkSLBaseType::NK_INT:
            case NkSLBaseType::NK_UINT:  case NkSLBaseType::NK_BOOL: align=4;  size=4;  break;
            case NkSLBaseType::NK_VEC2:  case NkSLBaseType::NK_IVEC2: align=8;  size=8;  break;
            case NkSLBaseType::NK_VEC3:  case NkSLBaseType::NK_IVEC3: align=16; size=12; break;
            case NkSLBaseType::NK_VEC4:  case NkSLBaseType::NK_IVEC4: align=16; size=16; break;
            case NkSLBaseType::NK_MAT3:  align=16; size=48; break;
            case NkSLBaseType::NK_MAT4:  align=16; size=64; break;
            default: align=4; size=4; break;
        }
    }
    static inline uint32 AlignUp(uint32 v, uint32 a) { return (v + (a-1)) & ~(a-1); }

    static inline uint8 CompOf(NkSLBaseType t) { return (uint8)NkSLBaseTypeComponents(t); }

    // =============================================================================
    // Collecte des symboles (in/out/uniform/sampler) + offsets
    // =============================================================================
    void NkSLCodeGenBytecode::CollectSymbols(NkSLProgramNode* ast, NkSLStage stage) {
        uint32 inOff = 0, outOff = 0;
        uint32 samplerIdx = 0;

        // gl_Position (builtin de sortie VERTEX) : RÉSERVÉ en outputs[0..3]. Les
        // varyings déclarés démarrent donc à l'offset 4. Contrat hôte (Software) :
        //   position = outputs[0..3] ; attrs[k] = outputs[4+k].
        if (stage == NkSLStage::NK_VERTEX) {
            Sym s; s.kind=SymKind::OUTPUT; s.type=NkSLBaseType::NK_VEC4; s.offset=0; s.count=4;
            mSyms["gl_Position"] = s;
            NkSLVMSymbol sym; sym.name="gl_Position"; sym.offset=0; sym.count=4; sym.location=0;
            mProg->outputs.PushBack(sym);
            outOff = 4;
        }

        for (auto* child : ast->children) {
            if (!child) continue;

            // Variables globales in/out
            if (child->kind == NkSLNodeKind::NK_DECL_VAR ||
                child->kind == NkSLNodeKind::NK_DECL_INPUT ||
                child->kind == NkSLNodeKind::NK_DECL_OUTPUT) {
                auto* v = static_cast<NkSLVarDeclNode*>(child);
                if (!v->type) continue;
                NkSLBaseType bt = v->type->baseType;

                if (NkSLTypeIsSampler(bt)) {
                    Sym s; s.kind=SymKind::SAMPLER; s.type=bt; s.offset=samplerIdx;
                    s.isShadow = (bt==NkSLBaseType::NK_SAMPLER2D_SHADOW ||
                                  bt==NkSLBaseType::NK_SAMPLER_CUBE_SHADOW ||
                                  bt==NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW);
                    if (v->binding.HasBinding()) s.offset = (uint32)v->binding.binding;
                    mSyms[v->name] = s;
                    NkSLVMSampler smp; smp.name=v->name; smp.index=s.offset; smp.isShadow=s.isShadow;
                    mProg->samplers.PushBack(smp);
                    samplerIdx = s.offset + 1;
                    continue;
                }

                uint8 comp = CompOf(bt);
                if (v->storage == NkSLStorageQual::NK_IN) {
                    Sym s; s.kind=SymKind::INPUT; s.type=bt; s.offset=inOff; s.count=comp;
                    mSyms[v->name] = s;
                    NkSLVMSymbol sym; sym.name=v->name; sym.offset=inOff; sym.count=comp;
                    sym.location=(uint32)(v->binding.HasLocation()?v->binding.location:0);
                    mProg->inputs.PushBack(sym);
                    inOff += comp;
                } else if (v->storage == NkSLStorageQual::NK_OUT) {
                    Sym s; s.kind=SymKind::OUTPUT; s.type=bt; s.offset=outOff; s.count=comp;
                    mSyms[v->name] = s;
                    NkSLVMSymbol sym; sym.name=v->name; sym.offset=outOff; sym.count=comp;
                    sym.location=(uint32)(v->binding.HasLocation()?v->binding.location:0);
                    mProg->outputs.PushBack(sym);
                    outOff += comp;
                }
                continue;
            }

            // Bloc UBO : instance + membres std140
            if (child->kind == NkSLNodeKind::NK_DECL_UNIFORM_BLOCK) {
                auto* b = static_cast<NkSLBlockDeclNode*>(child);
                if (!b->instanceName.Empty()) {
                    mUboInstance = b->instanceName;
                    Sym inst; inst.kind=SymKind::UBO_INSTANCE; mSyms[b->instanceName] = inst;
                }
                uint32 cursor = 0;
                for (auto* m : b->members) {
                    if (!m || !m->type) continue;
                    NkSLBaseType bt = m->type->baseType;
                    uint32 al, sz; Std140(bt, al, sz);
                    cursor = AlignUp(cursor, al);
                    Sym s; s.kind=SymKind::UNIFORM; s.type=bt; s.offset=cursor; s.count=CompOf(bt);
                    // clé qualifiée "instance.membre" ET nom court
                    if (!b->instanceName.Empty()) mSyms[b->instanceName + "." + m->name] = s;
                    mSyms[m->name] = s;
                    NkSLVMSymbol sym; sym.name=m->name; sym.offset=cursor; sym.count=CompOf(bt);
                    mProg->uniforms.PushBack(sym);
                    cursor += sz;
                }
            }
        }

        mProg->inputFloats  = inOff;
        mProg->outputFloats = outOff;
    }

    NkSLFunctionDeclNode* NkSLCodeGenBytecode::FindEntry(NkSLProgramNode* ast, NkSLStage stage) {
        NkSLFunctionDeclNode* fallbackMain = nullptr;
        for (auto* child : ast->children) {
            if (child && child->kind == NkSLNodeKind::NK_DECL_FUNCTION) {
                auto* fn = static_cast<NkSLFunctionDeclNode*>(child);
                if (fn->isEntry) return fn;
                if (fn->name == "main") fallbackMain = fn;
            }
        }
        return fallbackMain;
    }

    // ── Tables builtins / constructeurs / swizzle ────────────────────────────
    int32 NkSLCodeGenBytecode::ParseSwizzle(const NkString& s) {
        if (s.Empty() || s.Size() > 4) return -1;
        uint8 idx[4] = {0xFF,0xFF,0xFF,0xFF};
        for (usize i=0;i<s.Size();++i) {
            char c = s.CStr()[i];
            switch (c) {
                case 'x': case 'r': case 's': idx[i]=0; break;
                case 'y': case 'g': case 't': idx[i]=1; break;
                case 'z': case 'b': case 'p': idx[i]=2; break;
                case 'w': case 'a': case 'q': idx[i]=3; break;
                default: return -1;
            }
        }
        return NkSLMakeSwizzle(idx[0], idx[1], idx[2], idx[3]);
    }

    NkSLOp NkSLCodeGenBytecode::BuiltinOp(const NkString& n) {
        if (n=="dot")        return NkSLOp::OP_DOT;
        if (n=="cross")      return NkSLOp::OP_CROSS;
        if (n=="normalize")  return NkSLOp::OP_NORMALIZE;
        if (n=="length")     return NkSLOp::OP_LENGTH;
        if (n=="distance")   return NkSLOp::OP_DISTANCE;
        if (n=="reflect")    return NkSLOp::OP_REFLECT;
        if (n=="mix")        return NkSLOp::OP_MIX;
        if (n=="lerp")       return NkSLOp::OP_MIX;
        if (n=="clamp")      return NkSLOp::OP_CLAMP;
        if (n=="saturate")   return NkSLOp::OP_SATURATE;
        if (n=="step")       return NkSLOp::OP_STEP;
        if (n=="smoothstep") return NkSLOp::OP_SMOOTHSTEP;
        if (n=="pow")        return NkSLOp::OP_POW;
        if (n=="sqrt")       return NkSLOp::OP_SQRT;
        if (n=="inversesqrt")return NkSLOp::OP_INVSQRT;
        if (n=="abs")        return NkSLOp::OP_ABS;
        if (n=="sign")       return NkSLOp::OP_SIGN;
        if (n=="floor")      return NkSLOp::OP_FLOOR;
        if (n=="ceil")       return NkSLOp::OP_CEIL;
        if (n=="fract")      return NkSLOp::OP_FRACT;
        if (n=="min")        return NkSLOp::OP_MIN;
        if (n=="max")        return NkSLOp::OP_MAX;
        if (n=="exp")        return NkSLOp::OP_EXP;
        if (n=="log")        return NkSLOp::OP_LOG;
        if (n=="exp2")       return NkSLOp::OP_EXP2;
        if (n=="log2")       return NkSLOp::OP_LOG2;
        if (n=="sin")        return NkSLOp::OP_SIN;
        if (n=="cos")        return NkSLOp::OP_COS;
        if (n=="tan")        return NkSLOp::OP_TAN;
        if (n=="asin")       return NkSLOp::OP_ASIN;
        if (n=="acos")       return NkSLOp::OP_ACOS;
        if (n=="atan")       return NkSLOp::OP_ATAN;
        if (n=="radians")    return NkSLOp::OP_RADIANS;
        if (n=="degrees")    return NkSLOp::OP_DEGREES;
        if (n=="transpose")  return NkSLOp::OP_TRANSPOSE;
        if (n=="inverse")    return NkSLOp::OP_INVERSE;
        return NkSLOp::OP_NOP;
    }

    uint8 NkSLCodeGenBytecode::ConstructorCount(const NkString& n) {
        if (n=="float"||n=="int"||n=="uint"||n=="bool") return 1;
        if (n=="vec2"||n=="ivec2") return 2;
        if (n=="vec3"||n=="ivec3") return 3;
        if (n=="vec4"||n=="ivec4") return 4;
        if (n=="mat3") return 9;
        if (n=="mat4") return 16;
        return 0;
    }
    NkSLBaseType NkSLCodeGenBytecode::TypeFromCtor(const NkString& n) {
        if (n=="vec2") return NkSLBaseType::NK_VEC2;
        if (n=="vec3") return NkSLBaseType::NK_VEC3;
        if (n=="vec4") return NkSLBaseType::NK_VEC4;
        if (n=="mat3") return NkSLBaseType::NK_MAT3;
        if (n=="mat4") return NkSLBaseType::NK_MAT4;
        return NkSLBaseType::NK_FLOAT;
    }

    // =============================================================================
    // Expressions
    // =============================================================================
    uint16 NkSLCodeGenBytecode::GenExpr(NkSLNode* n, uint8* outCount) {
        uint8 dummy; if (!outCount) outCount = &dummy; *outCount = 1;
        if (!n) { uint16 r=AllocReg(); Emit(NkSLOp::OP_LOADK, r, 0,0, AddConst(NkSLValue::Scalar(0.f))); return r; }

        switch (n->kind) {
            case NkSLNodeKind::NK_EXPR_LITERAL: {
                auto* lit = static_cast<NkSLLiteralNode*>(n);
                float val = (lit->baseType==NkSLBaseType::NK_BOOL) ? (lit->boolVal?1.f:0.f)
                          : (lit->baseType==NkSLBaseType::NK_INT||lit->baseType==NkSLBaseType::NK_UINT)
                            ? (float)lit->intVal : (float)lit->floatVal;
                uint16 r=AllocReg(); Emit(NkSLOp::OP_LOADK, r, 0,0, AddConst(NkSLValue::Scalar(val)));
                *outCount=1; return r;
            }
            case NkSLNodeKind::NK_EXPR_IDENT: {
                auto* id = static_cast<NkSLIdentNode*>(n);
                auto* s = mSyms.Find(id->name);
                if (!s) { Error(n->line, NkString("symbole inconnu: ")+id->name); return AllocReg(); }
                if (s->kind==SymKind::LOCAL)  { *outCount=s->count; return s->reg; }
                if (s->kind==SymKind::INPUT)  { uint16 r=AllocReg(); Emit(NkSLOp::OP_LOAD_IN, r,0,0,(int32)s->offset,s->count); *outCount=s->count; return r; }
                if (s->kind==SymKind::UNIFORM){ uint16 r=AllocReg(); Emit(NkSLOp::OP_LOAD_UNI,r,0,0,(int32)s->offset,s->count); *outCount=s->count; return r; }
                Error(n->line, NkString("identifiant non chargeable: ")+id->name);
                return AllocReg();
            }
            case NkSLNodeKind::NK_EXPR_MEMBER: {
                auto* m = static_cast<NkSLMemberNode*>(n);
                // ubo.field ?
                if (m->object && m->object->kind==NkSLNodeKind::NK_EXPR_IDENT) {
                    auto* obj = static_cast<NkSLIdentNode*>(m->object);
                    auto* os = mSyms.Find(obj->name);
                    if (os && os->kind==SymKind::UBO_INSTANCE) {
                        auto* fs = mSyms.Find(obj->name + "." + m->member);
                        if (!fs) fs = mSyms.Find(m->member);
                        if (fs) { uint16 r=AllocReg(); Emit(NkSLOp::OP_LOAD_UNI,r,0,0,(int32)fs->offset,fs->count); *outCount=fs->count; return r; }
                    }
                }
                // sinon : swizzle
                uint8 oc; uint16 ro = GenExpr(m->object, &oc);
                int32 mask = ParseSwizzle(m->member);
                if (mask < 0) { Error(n->line, NkString("membre invalide: ")+m->member); *outCount=oc; return ro; }
                uint16 r=AllocReg(); uint32 nc=NkSLSwizzleCount(mask);
                Emit(NkSLOp::OP_SWIZZLE, r, ro, 0, mask, (uint8)nc);
                *outCount=(uint8)nc; return r;
            }
            case NkSLNodeKind::NK_EXPR_BINARY: {
                auto* b = static_cast<NkSLBinaryNode*>(n);
                uint8 lc, rc; uint16 lr=GenExpr(b->left,&lc); uint16 rr=GenExpr(b->right,&rc);
                uint16 r=AllocReg(); NkSLOp op=NkSLOp::OP_ADD; uint8 oc = lc>=rc?lc:rc;
                const NkString& o=b->op;
                if      (o=="+") op=NkSLOp::OP_ADD;
                else if (o=="-") op=NkSLOp::OP_SUB;
                else if (o=="*") { // mat*vec / mat*mat -> MATMUL si une matrice
                    bool lm=(lc==9||lc==16), rm=(rc==9||rc==16);
                    if (lm||rm) { op=NkSLOp::OP_MATMUL; oc = lm?(lc==16?4:3):rc; if(lm&&rm)oc=lc; }
                    else op=NkSLOp::OP_MUL;
                }
                else if (o=="/") op=NkSLOp::OP_DIV;
                else if (o=="%") op=NkSLOp::OP_MOD;
                else if (o=="<") { op=NkSLOp::OP_LT; oc=1; }
                else if (o==">") { op=NkSLOp::OP_GT; oc=1; }
                else if (o=="<="){ op=NkSLOp::OP_LE; oc=1; }
                else if (o==">="){ op=NkSLOp::OP_GE; oc=1; }
                else if (o=="=="){ op=NkSLOp::OP_EQ; oc=1; }
                else if (o=="!="){ op=NkSLOp::OP_NE; oc=1; }
                else if (o=="&&"){ op=NkSLOp::OP_AND;oc=1; }
                else if (o=="||"){ op=NkSLOp::OP_OR; oc=1; }
                Emit(op, r, lr, rr); *outCount=oc; return r;
            }
            case NkSLNodeKind::NK_EXPR_UNARY: {
                auto* u = static_cast<NkSLUnaryNode*>(n);
                uint8 oc; uint16 rr=GenExpr(u->operand,&oc);
                // ++ / -- : modifient EN PLACE le registre de l'opérande (un local
                // renvoie son propre registre via GenExpr) — indispensable pour les
                // boucles for, sinon la variable ne change jamais (boucle infinie).
                if (u->op=="++" || u->op=="--") {
                    uint16 kreg=AllocReg();
                    Emit(NkSLOp::OP_LOADK, kreg, 0,0, AddConst(NkSLValue::Scalar(1.f)));
                    NkSLOp aop = (u->op=="++") ? NkSLOp::OP_ADD : NkSLOp::OP_SUB;
                    if (u->prefix) { Emit(aop, rr, rr, kreg); *outCount=oc; return rr; }
                    uint16 old=AllocReg();
                    Emit(NkSLOp::OP_MOV, old, rr);   // postfix : valeur d'avant
                    Emit(aop, rr, rr, kreg);
                    *outCount=oc; return old;
                }
                uint16 r=AllocReg();
                if (u->op=="-") Emit(NkSLOp::OP_NEG, r, rr);
                else if (u->op=="!") { Emit(NkSLOp::OP_NOT, r, rr); oc=1; }
                else Emit(NkSLOp::OP_MOV, r, rr);
                *outCount=oc; return r;
            }
            case NkSLNodeKind::NK_EXPR_TERNARY: {
                // cond ? a : b  -> via sauts
                uint8 cc; uint16 cr=GenExpr(n->children.Size()>0?n->children[0]:nullptr,&cc);
                uint16 res=AllocReg();
                Emit(NkSLOp::OP_JZ, 0, cr, 0, 0); int32 jzPos=(int32)mProg->code.Size()-1;
                uint8 ac; uint16 ar=GenExpr(n->children.Size()>1?n->children[1]:nullptr,&ac);
                Emit(NkSLOp::OP_MOV, res, ar);
                Emit(NkSLOp::OP_JMP, 0,0,0,0); int32 jmpPos=(int32)mProg->code.Size()-1;
                mProg->code[jzPos].imm=(int32)mProg->code.Size();
                uint8 bc; uint16 br=GenExpr(n->children.Size()>2?n->children[2]:nullptr,&bc);
                Emit(NkSLOp::OP_MOV, res, br);
                mProg->code[jmpPos].imm=(int32)mProg->code.Size();
                *outCount=ac; return res;
            }
            case NkSLNodeKind::NK_EXPR_CALL: {
                auto* call = static_cast<NkSLCallNode*>(n);
                const NkString& name = call->callee;
                // Texture sampling
                if (name=="texture" || name=="textureLod") {
                    // arg0 = sampler ident, arg1 = uv
                    int samp=0; bool shadow=false;
                    if (call->args.Size()>0 && call->args[0]->kind==NkSLNodeKind::NK_EXPR_IDENT) {
                        auto* sid=static_cast<NkSLIdentNode*>(call->args[0]);
                        auto* ss=mSyms.Find(sid->name);
                        if (ss){ samp=(int)ss->offset; shadow=ss->isShadow; }
                    }
                    uint8 uc; uint16 uvr=GenExpr(call->args.Size()>1?call->args[1]:nullptr,&uc);
                    uint16 r=AllocReg();
                    if (shadow){ Emit(NkSLOp::OP_TEX_SAMPLE_SHADOW,r,uvr,0,samp); *outCount=1; }
                    else { Emit(NkSLOp::OP_TEX_SAMPLE,r,uvr,0,samp); *outCount=4; }
                    return r;
                }
                if (name=="textureSize") {
                    int samp=0;
                    if (call->args.Size()>0 && call->args[0]->kind==NkSLNodeKind::NK_EXPR_IDENT) {
                        auto* ss=mSyms.Find(static_cast<NkSLIdentNode*>(call->args[0])->name);
                        if (ss) samp=(int)ss->offset;
                    }
                    uint16 r=AllocReg(); Emit(NkSLOp::OP_TEX_SIZE,r,0,0,samp); *outCount=2; return r;
                }
                // Constructeur vecN/matN
                uint8 ctorCount = ConstructorCount(name);
                if (ctorCount) {
                    // 1. Évaluer TOUS les args d'abord (GenExpr alloue des registres
                    //    en interne → ne PAS capturer 'base' avant, sinon les MOV ne
                    //    sont pas contigus et CONSTRUCT lit les mauvais registres).
                    NkVector<uint16> argRegs;
                    for (auto* a : call->args) { uint8 ac; uint16 ar=GenExpr(a,&ac); (void)ac; argRegs.PushBack(ar); }
                    // 2. Bloc de registres CONTIGUS : MOV chaque arg dedans.
                    uint16 base = mNextReg;
                    for (usize i=0;i<argRegs.Size();++i) { uint16 dst=AllocReg(); Emit(NkSLOp::OP_MOV,dst,argRegs[i]); }
                    uint16 r=AllocReg();
                    Emit(NkSLOp::OP_CONSTRUCT, r, base, 0, (int32)call->args.Size(), ctorCount);
                    *outCount=ctorCount; return r;
                }
                // Builtin
                NkSLOp bop = BuiltinOp(name);
                if (bop != NkSLOp::OP_NOP) {
                    uint8 c0=1,c1=1,c2=1; uint16 r0=0,r1=0,r2=0;
                    if (call->args.Size()>0) r0=GenExpr(call->args[0],&c0);
                    if (call->args.Size()>1) r1=GenExpr(call->args[1],&c1);
                    if (call->args.Size()>2) r2=GenExpr(call->args[2],&c2);
                    uint16 r=AllocReg();
                    // 3 args : 3e opérande passé via imm (=registre)
                    if (call->args.Size()>2) Emit(bop, r, r0, r1, (int32)r2);
                    else                      Emit(bop, r, r0, r1);
                    // count résultat : scalaire pour dot/length/distance, sinon count arg0
                    *outCount = (bop==NkSLOp::OP_DOT||bop==NkSLOp::OP_LENGTH||bop==NkSLOp::OP_DISTANCE)?1:c0;
                    return r;
                }
                Error(n->line, NkString("fonction non supportée (VM): ")+name);
                return AllocReg();
            }
            case NkSLNodeKind::NK_EXPR_CAST: {
                auto* c = static_cast<NkSLCastNode*>(n);
                return GenExpr(c->expr, outCount); // cast numérique : no-op (tout est float)
            }
            case NkSLNodeKind::NK_EXPR_INDEX: {
                auto* ix = static_cast<NkSLIndexNode*>(n);
                uint8 ac; uint16 ar=GenExpr(ix->array,&ac); uint8 ic; uint16 ir=GenExpr(ix->index,&ic);
                uint16 r=AllocReg(); Emit(NkSLOp::OP_INDEX,r,ar,ir);
                *outCount = (ac==16)?4:(ac==9?3:1); return r;
            }
            case NkSLNodeKind::NK_EXPR_ASSIGN: {
                auto* as = static_cast<NkSLAssignNode*>(n);
                uint8 rc; uint16 rr=GenExpr(as->rhs,&rc); GenAssign(as->lhs, rr, rc);
                *outCount=rc; return rr;
            }
            default:
                Error(n->line, "noeud expression non supporté (VM)");
                return AllocReg();
        }
    }

    // =============================================================================
    // Assignation
    // =============================================================================
    void NkSLCodeGenBytecode::GenAssign(NkSLNode* lhs, uint16 rhsReg, uint8 rhsCount) {
        if (!lhs) return;
        if (lhs->kind==NkSLNodeKind::NK_EXPR_IDENT) {
            auto* id=static_cast<NkSLIdentNode*>(lhs);
            auto* s=mSyms.Find(id->name);
            if (!s) { Error(lhs->line, NkString("affectation symbole inconnu: ")+id->name); return; }
            if (s->kind==SymKind::OUTPUT) { Emit(NkSLOp::OP_STORE_OUT, 0, rhsReg, 0, (int32)s->offset, s->count); return; }
            if (s->kind==SymKind::LOCAL)  { Emit(NkSLOp::OP_MOV, s->reg, rhsReg); return; }
            Error(lhs->line, NkString("affectation non supportée: ")+id->name); return;
        }
        if (lhs->kind==NkSLNodeKind::NK_EXPR_MEMBER) {
            auto* m=static_cast<NkSLMemberNode*>(lhs);
            // swizzle d'un local : out.xyz = ... ou v.xy = ...
            if (m->object && m->object->kind==NkSLNodeKind::NK_EXPR_IDENT) {
                auto* obj=static_cast<NkSLIdentNode*>(m->object);
                auto* os=mSyms.Find(obj->name);
                int32 mask=ParseSwizzle(m->member);
                if (os && os->kind==SymKind::LOCAL && mask>=0) {
                    Emit(NkSLOp::OP_WRITE_COMP, os->reg, rhsReg, 0, mask, (uint8)NkSLSwizzleCount(mask));
                    return;
                }
                // out.xyz : charger via reg de travail non géré -> TODO (rare). Fallback store complet.
                if (os && os->kind==SymKind::OUTPUT && mask>=0) {
                    Emit(NkSLOp::OP_STORE_OUT, 0, rhsReg, 0, (int32)os->offset, (uint8)NkSLSwizzleCount(mask));
                    return;
                }
            }
        }
        Error(lhs->line, "cible d'affectation non supportée (VM)");
    }

    // =============================================================================
    // Statements
    // =============================================================================
    void NkSLCodeGenBytecode::GenStmt(NkSLNode* n) {
        if (!n) return;
        switch (n->kind) {
            case NkSLNodeKind::NK_STMT_BLOCK:
                for (auto* c : n->children) GenStmt(c);
                break;
            case NkSLNodeKind::NK_DECL_VAR: {
                auto* v=static_cast<NkSLVarDeclNode*>(n);
                uint16 reg=AllocReg();
                Sym s; s.kind=SymKind::LOCAL; s.reg=reg;
                s.type = v->type?v->type->baseType:NkSLBaseType::NK_FLOAT;
                s.count = v->type?CompOf(v->type->baseType):1;
                if (v->initializer) { uint8 ic; uint16 ir=GenExpr(v->initializer,&ic); Emit(NkSLOp::OP_MOV,reg,ir); s.count=ic; }
                mSyms[v->name]=s;
                break;
            }
            case NkSLNodeKind::NK_STMT_EXPR:
                if (n->children.Size()>0) { uint8 c; GenExpr(n->children[0],&c); }
                break;
            case NkSLNodeKind::NK_EXPR_ASSIGN: { uint8 c; GenExpr(n,&c); break; }
            case NkSLNodeKind::NK_STMT_IF: {
                auto* f=static_cast<NkSLIfNode*>(n);
                uint8 cc; uint16 cr=GenExpr(f->condition,&cc);
                Emit(NkSLOp::OP_JZ,0,cr,0,0); int32 jz=(int32)mProg->code.Size()-1;
                GenStmt(f->thenBranch);
                if (f->elseBranch) {
                    Emit(NkSLOp::OP_JMP,0,0,0,0); int32 jmp=(int32)mProg->code.Size()-1;
                    mProg->code[jz].imm=(int32)mProg->code.Size();
                    GenStmt(f->elseBranch);
                    mProg->code[jmp].imm=(int32)mProg->code.Size();
                } else {
                    mProg->code[jz].imm=(int32)mProg->code.Size();
                }
                break;
            }
            case NkSLNodeKind::NK_STMT_FOR: {
                auto* fr=static_cast<NkSLForNode*>(n);
                if (fr->init) GenStmt(fr->init);
                int32 top=(int32)mProg->code.Size();
                int32 jz=-1;
                if (fr->condition) { uint8 cc; uint16 cr=GenExpr(fr->condition,&cc); Emit(NkSLOp::OP_JZ,0,cr,0,0); jz=(int32)mProg->code.Size()-1; }
                GenStmt(fr->body);
                if (fr->increment) { uint8 c; GenExpr(fr->increment,&c); }
                Emit(NkSLOp::OP_JMP,0,0,0,top);
                if (jz>=0) mProg->code[jz].imm=(int32)mProg->code.Size();
                break;
            }
            case NkSLNodeKind::NK_STMT_RETURN:
                Emit(NkSLOp::OP_RET);
                break;
            case NkSLNodeKind::NK_STMT_DISCARD:
                Emit(NkSLOp::OP_DISCARD);
                break;
            default:
                // statements non gérés : ignorés (best-effort MVP)
                break;
        }
    }

    // =============================================================================
    // Entrée
    // =============================================================================
    bool NkSLCodeGenBytecode::Generate(NkSLProgramNode* ast, NkSLStage stage,
                                       NkSLByteProgram& out, NkVector<NkSLCompileError>& errors) {
        mProg = &out; mErr = &errors; mNextReg = 0; mSyms.Clear();
        out.stage = stage;

        CollectSymbols(ast, stage);
        NkSLFunctionDeclNode* entry = FindEntry(ast, stage);
        if (!entry || !entry->body) { Error(0, "VM: fonction d'entrée introuvable"); return false; }

        GenStmt(entry->body);
        Emit(NkSLOp::OP_RET);
        out.regCount = mNextReg;
        return errors.Empty();
    }

} // namespace nkentseu
