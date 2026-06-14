// =============================================================================
// NkSLVM.cpp — Interpréteur du bytecode NkSL
// =============================================================================
#include "NKSL/VM/NkSLVM.h"
#include <cmath>
#include <cstring>

namespace nkentseu {

    // ── Helpers valeur ───────────────────────────────────────────────────────
    static inline NkSLValue VScalar(float x) { NkSLValue r; r.count=1; r.v[0]=x; return r; }

    // Op composante par composante avec broadcast scalaire (count 1 -> N).
    template <typename F>
    static inline NkSLValue VBin(const NkSLValue& a, const NkSLValue& b, F f) {
        NkSLValue r;
        uint8 n = a.count >= b.count ? a.count : b.count;
        r.count = n;
        for (uint8 i = 0; i < n; ++i) {
            float av = a.count == 1 ? a.v[0] : a.v[i];
            float bv = b.count == 1 ? b.v[0] : b.v[i];
            r.v[i] = f(av, bv);
        }
        return r;
    }
    template <typename F>
    static inline NkSLValue VUn(const NkSLValue& a, F f) {
        NkSLValue r; r.count = a.count;
        for (uint8 i = 0; i < a.count; ++i) r.v[i] = f(a.v[i]);
        return r;
    }

    static inline float VDot(const NkSLValue& a, const NkSLValue& b) {
        float s = 0.f; uint8 n = a.count < b.count ? a.count : b.count;
        for (uint8 i = 0; i < n; ++i) s += a.v[i] * b.v[i];
        return s;
    }
    static inline float VLen(const NkSLValue& a) { return std::sqrt(VDot(a, a)); }

    // mat (column-major) × vec / mat × mat / vec × mat
    static NkSLValue VMatMul(const NkSLValue& A, const NkSLValue& B) {
        // dimensions par count : 9=mat3, 16=mat4 ; 2/3/4 = vec
        auto isMat = [](uint8 c){ return c == 9 || c == 16; };
        if (isMat(A.count) && (B.count == 3 || B.count == 4)) {
            int n = A.count == 16 ? 4 : 3;
            NkSLValue r; r.count = (uint8)n;
            for (int row = 0; row < n; ++row) {
                float s = 0.f;
                for (int k = 0; k < n; ++k) s += A.v[k*n + row] * B.v[k]; // col-major: A[col=k][row]
                r.v[row] = s;
            }
            return r;
        }
        if (isMat(A.count) && isMat(B.count)) {
            int n = A.count == 16 ? 4 : 3;
            NkSLValue r; r.count = A.count;
            for (int col = 0; col < n; ++col)
                for (int row = 0; row < n; ++row) {
                    float s = 0.f;
                    for (int k = 0; k < n; ++k) s += A.v[k*n + row] * B.v[col*n + k];
                    r.v[col*n + row] = s;
                }
            return r;
        }
        if ((A.count == 3 || A.count == 4) && isMat(B.count)) {
            int n = B.count == 16 ? 4 : 3;
            NkSLValue r; r.count = (uint8)n;
            for (int col = 0; col < n; ++col) {
                float s = 0.f;
                for (int k = 0; k < n; ++k) s += A.v[k] * B.v[col*n + k];
                r.v[col] = s;
            }
            return r;
        }
        // fallback : produit composante (broadcast)
        return VBin(A, B, [](float x, float y){ return x*y; });
    }

    static NkSLValue VTranspose(const NkSLValue& A) {
        int n = A.count == 16 ? 4 : (A.count == 9 ? 3 : 0);
        if (!n) return A;
        NkSLValue r; r.count = A.count;
        for (int c = 0; c < n; ++c)
            for (int row = 0; row < n; ++row)
                r.v[c*n + row] = A.v[row*n + c];
        return r;
    }

    static NkSLValue VInverse3(const NkSLValue& m) {
        // m col-major 3x3
        float a=m.v[0],b=m.v[3],c=m.v[6], d=m.v[1],e=m.v[4],f=m.v[7], g=m.v[2],h=m.v[5],k=m.v[8];
        float det = a*(e*k-f*h) - b*(d*k-f*g) + c*(d*h-e*g);
        float inv = det != 0.f ? 1.f/det : 0.f;
        NkSLValue r; r.count = 9;
        r.v[0]=(e*k-f*h)*inv; r.v[1]=-(d*k-f*g)*inv; r.v[2]=(d*h-e*g)*inv;
        r.v[3]=-(b*k-c*h)*inv; r.v[4]=(a*k-c*g)*inv; r.v[5]=-(a*h-b*g)*inv;
        r.v[6]=(b*f-c*e)*inv; r.v[7]=-(a*f-c*d)*inv; r.v[8]=(a*e-b*d)*inv;
        return r;
    }

    // =============================================================================
    bool NkSLVM::Execute(const NkSLByteProgram& prog, NkSLVMEnv& env) {
        const uint32 nregs = prog.regCount + 1;
        // Petit register file sur la pile si possible, sinon heap.
        NkSLValue  stackRegs[64];
        NkVector<NkSLValue> heapRegs;
        NkSLValue* reg = stackRegs;
        if (nregs > 64) { heapRegs.Resize(nregs); reg = heapRegs.Data(); }

        const NkSLInstr* code = prog.code.Data();
        const int32 ncode = (int32)prog.code.Size();
        env.discarded = false;

        // Garde-fou : un bytecode mal formé (saut en boucle) ne doit JAMAIS geler le
        // process. Budget large mais borné (couvre les PCF imbriqués légitimes).
        uint64 budget = 1000000ull;

        for (int32 pc = 0; pc < ncode; ++pc) {
            if (--budget == 0) return false;  // boucle suspecte -> bail
            const NkSLInstr& in = code[pc];
            NkSLValue& A = reg[in.a];
            switch (in.op) {
                case NkSLOp::OP_NOP: break;
                case NkSLOp::OP_LOADK: A = prog.constants[(uint32)in.imm]; break;
                case NkSLOp::OP_MOV:   A = reg[in.b]; break;

                case NkSLOp::OP_LOAD_IN: {
                    A.count = in.aux;
                    if (env.inputs) for (uint8 i=0;i<in.aux;++i) A.v[i] = env.inputs[(uint32)in.imm + i];
                    break;
                }
                case NkSLOp::OP_LOAD_UNI: {
                    A.count = in.aux;
                    if (env.uniforms) std::memcpy(A.v, env.uniforms + (uint32)in.imm, (size_t)in.aux * sizeof(float));
                    break;
                }
                case NkSLOp::OP_STORE_OUT: {
                    const NkSLValue& S = reg[in.b];
                    if (env.outputs) for (uint8 i=0;i<in.aux;++i) env.outputs[(uint32)in.imm + i] = S.v[i];
                    break;
                }

                case NkSLOp::OP_CONSTRUCT: {
                    // matN(matM) : extrait la sous-matrice supérieure-gauche (GLSL).
                    // mat3(mat4) = colonnes 0..2 × lignes 0..2 (col-major) — pas les
                    // 9 premiers floats ! Indispensable pour la matrice des normales.
                    if (in.imm == 1 && in.aux == 9 && reg[in.b].count == 16) {
                        NkSLValue r; r.count = 9; const NkSLValue& s = reg[in.b];
                        for (int c=0;c<3;++c) for (int rr=0;rr<3;++rr) r.v[c*3+rr] = s.v[c*4+rr];
                        A = r; break;
                    }
                    if (in.imm == 1 && in.aux == 16 && reg[in.b].count == 9) {
                        NkSLValue r; r.count = 16; const NkSLValue& s = reg[in.b];
                        for (uint8 i=0;i<16;++i) r.v[i]=0.f;
                        for (int c=0;c<3;++c) for (int rr=0;rr<3;++rr) r.v[c*4+rr] = s.v[c*3+rr];
                        r.v[15]=1.f; A = r; break;
                    }
                    NkSLValue r; r.count = in.aux; uint8 w = 0;
                    for (int32 k = 0; k < in.imm && w < 16; ++k) {
                        const NkSLValue& s = reg[in.b + k];
                        for (uint8 i = 0; i < s.count && w < 16; ++i) r.v[w++] = s.v[i];
                    }
                    // broadcast scalaire unique -> remplir (ex vec3(0.0))
                    if (in.imm == 1 && reg[in.b].count == 1 && in.aux > 1)
                        for (uint8 i=1;i<in.aux;++i) r.v[i] = r.v[0];
                    A = r;
                    break;
                }
                case NkSLOp::OP_SWIZZLE: {
                    const NkSLValue& s = reg[in.b];
                    uint32 n = NkSLSwizzleCount(in.imm);
                    NkSLValue r; r.count = (uint8)n;
                    for (uint32 i=0;i<n;++i) r.v[i] = s.v[ NkSLSwizzleIdx(in.imm, i) ];
                    A = r;
                    break;
                }
                case NkSLOp::OP_WRITE_COMP: {
                    const NkSLValue& s = reg[in.b];
                    uint32 n = NkSLSwizzleCount(in.imm);
                    for (uint32 i=0;i<n;++i) A.v[ NkSLSwizzleIdx(in.imm, i) ] = (s.count==1? s.v[0] : s.v[i]);
                    break;
                }
                case NkSLOp::OP_INDEX: {
                    const NkSLValue& b = reg[in.b];
                    int idx = (int)reg[in.c].v[0];
                    if (b.count == 9 || b.count == 16) {
                        int n = b.count == 16 ? 4 : 3;
                        NkSLValue r; r.count=(uint8)n;
                        for (int i=0;i<n;++i) r.v[i] = b.v[idx*n + i];
                        A = r;
                    } else {
                        A = VScalar(b.v[idx < (int)b.count ? idx : 0]);
                    }
                    break;
                }

                case NkSLOp::OP_ADD: A = VBin(reg[in.b], reg[in.c], [](float x,float y){return x+y;}); break;
                case NkSLOp::OP_SUB: A = VBin(reg[in.b], reg[in.c], [](float x,float y){return x-y;}); break;
                case NkSLOp::OP_MUL: A = VBin(reg[in.b], reg[in.c], [](float x,float y){return x*y;}); break;
                case NkSLOp::OP_DIV: A = VBin(reg[in.b], reg[in.c], [](float x,float y){return y!=0.f?x/y:0.f;}); break;
                case NkSLOp::OP_MOD: A = VBin(reg[in.b], reg[in.c], [](float x,float y){return std::fmod(x,y);}); break;
                case NkSLOp::OP_NEG: A = VUn(reg[in.b], [](float x){return -x;}); break;
                case NkSLOp::OP_MATMUL: A = VMatMul(reg[in.b], reg[in.c]); break;

                case NkSLOp::OP_LT: A = VScalar(reg[in.b].v[0] <  reg[in.c].v[0] ? 1.f:0.f); break;
                case NkSLOp::OP_GT: A = VScalar(reg[in.b].v[0] >  reg[in.c].v[0] ? 1.f:0.f); break;
                case NkSLOp::OP_LE: A = VScalar(reg[in.b].v[0] <= reg[in.c].v[0] ? 1.f:0.f); break;
                case NkSLOp::OP_GE: A = VScalar(reg[in.b].v[0] >= reg[in.c].v[0] ? 1.f:0.f); break;
                case NkSLOp::OP_EQ: A = VScalar(reg[in.b].v[0] == reg[in.c].v[0] ? 1.f:0.f); break;
                case NkSLOp::OP_NE: A = VScalar(reg[in.b].v[0] != reg[in.c].v[0] ? 1.f:0.f); break;
                case NkSLOp::OP_AND:A = VScalar((reg[in.b].v[0]!=0.f && reg[in.c].v[0]!=0.f)?1.f:0.f); break;
                case NkSLOp::OP_OR: A = VScalar((reg[in.b].v[0]!=0.f || reg[in.c].v[0]!=0.f)?1.f:0.f); break;
                case NkSLOp::OP_NOT:A = VScalar(reg[in.b].v[0]==0.f?1.f:0.f); break;

                case NkSLOp::OP_DOT:       A = VScalar(VDot(reg[in.b], reg[in.c])); break;
                case NkSLOp::OP_LENGTH:    A = VScalar(VLen(reg[in.b])); break;
                case NkSLOp::OP_DISTANCE:  A = VScalar(VLen(VBin(reg[in.b],reg[in.c],[](float x,float y){return x-y;}))); break;
                case NkSLOp::OP_NORMALIZE: { float l=VLen(reg[in.b]); A=VUn(reg[in.b],[l](float x){return l>1e-12f?x/l:0.f;}); break; }
                case NkSLOp::OP_CROSS: {
                    const NkSLValue& u=reg[in.b]; const NkSLValue& v=reg[in.c];
                    NkSLValue r; r.count=3;
                    r.v[0]=u.v[1]*v.v[2]-u.v[2]*v.v[1];
                    r.v[1]=u.v[2]*v.v[0]-u.v[0]*v.v[2];
                    r.v[2]=u.v[0]*v.v[1]-u.v[1]*v.v[0];
                    A=r; break;
                }
                case NkSLOp::OP_REFLECT: { // I - 2*dot(N,I)*N
                    const NkSLValue& I=reg[in.b]; const NkSLValue& N=reg[in.c];
                    float d=2.f*VDot(N,I); NkSLValue r; r.count=I.count;
                    for (uint8 i=0;i<I.count;++i) r.v[i]=I.v[i]-d*N.v[i];
                    A=r; break;
                }
                case NkSLOp::OP_MIX: { // mix(a,b,t)  -> b/c regs + t dans reg[in.imm]
                    const NkSLValue& a=reg[in.b]; const NkSLValue& b=reg[in.c]; const NkSLValue& t=reg[(uint16)in.imm];
                    NkSLValue r; r.count=a.count;
                    for (uint8 i=0;i<a.count;++i){ float ti=(t.count==1?t.v[0]:t.v[i]); r.v[i]=a.v[i]*(1.f-ti)+b.v[i]*ti; }
                    A=r; break;
                }
                case NkSLOp::OP_CLAMP: { // clamp(x,lo,hi) : x=b, lo=c, hi=reg[imm]
                    const NkSLValue& x=reg[in.b]; const NkSLValue& lo=reg[in.c]; const NkSLValue& hi=reg[(uint16)in.imm];
                    NkSLValue r; r.count=x.count;
                    for (uint8 i=0;i<x.count;++i){ float l=(lo.count==1?lo.v[0]:lo.v[i]); float h=(hi.count==1?hi.v[0]:hi.v[i]); float v=x.v[i]; r.v[i]=v<l?l:(v>h?h:v); }
                    A=r; break;
                }
                case NkSLOp::OP_SATURATE: A=VUn(reg[in.b],[](float x){return x<0.f?0.f:(x>1.f?1.f:x);}); break;
                case NkSLOp::OP_STEP: { // step(edge,x) edge=b x=c
                    A=VBin(reg[in.b],reg[in.c],[](float e,float x){return x<e?0.f:1.f;}); break;
                }
                case NkSLOp::OP_SMOOTHSTEP: { // smoothstep(e0,e1,x) e0=b e1=c x=reg[imm]
                    const NkSLValue& e0=reg[in.b]; const NkSLValue& e1=reg[in.c]; const NkSLValue& x=reg[(uint16)in.imm];
                    NkSLValue r; r.count=x.count;
                    for (uint8 i=0;i<x.count;++i){ float a=(e0.count==1?e0.v[0]:e0.v[i]); float b=(e1.count==1?e1.v[0]:e1.v[i]); float t=(x.v[i]-a)/((b-a)!=0.f?(b-a):1.f); t=t<0.f?0.f:(t>1.f?1.f:t); r.v[i]=t*t*(3.f-2.f*t); }
                    A=r; break;
                }
                case NkSLOp::OP_POW:   A=VBin(reg[in.b],reg[in.c],[](float x,float y){return std::pow(x,y);}); break;
                case NkSLOp::OP_MIN:   A=VBin(reg[in.b],reg[in.c],[](float x,float y){return x<y?x:y;}); break;
                case NkSLOp::OP_MAX:   A=VBin(reg[in.b],reg[in.c],[](float x,float y){return x>y?x:y;}); break;
                case NkSLOp::OP_SQRT:  A=VUn(reg[in.b],[](float x){return std::sqrt(x);}); break;
                case NkSLOp::OP_INVSQRT:A=VUn(reg[in.b],[](float x){return x>0.f?1.f/std::sqrt(x):0.f;}); break;
                case NkSLOp::OP_ABS:   A=VUn(reg[in.b],[](float x){return std::fabs(x);}); break;
                case NkSLOp::OP_SIGN:  A=VUn(reg[in.b],[](float x){return (x>0.f)-(x<0.f);}); break;
                case NkSLOp::OP_FLOOR: A=VUn(reg[in.b],[](float x){return std::floor(x);}); break;
                case NkSLOp::OP_CEIL:  A=VUn(reg[in.b],[](float x){return std::ceil(x);}); break;
                case NkSLOp::OP_FRACT: A=VUn(reg[in.b],[](float x){return x-std::floor(x);}); break;
                case NkSLOp::OP_EXP:   A=VUn(reg[in.b],[](float x){return std::exp(x);}); break;
                case NkSLOp::OP_LOG:   A=VUn(reg[in.b],[](float x){return std::log(x);}); break;
                case NkSLOp::OP_EXP2:  A=VUn(reg[in.b],[](float x){return std::exp2(x);}); break;
                case NkSLOp::OP_LOG2:  A=VUn(reg[in.b],[](float x){return std::log2(x);}); break;
                case NkSLOp::OP_SIN:   A=VUn(reg[in.b],[](float x){return std::sin(x);}); break;
                case NkSLOp::OP_COS:   A=VUn(reg[in.b],[](float x){return std::cos(x);}); break;
                case NkSLOp::OP_TAN:   A=VUn(reg[in.b],[](float x){return std::tan(x);}); break;
                case NkSLOp::OP_ASIN:  A=VUn(reg[in.b],[](float x){return std::asin(x);}); break;
                case NkSLOp::OP_ACOS:  A=VUn(reg[in.b],[](float x){return std::acos(x);}); break;
                case NkSLOp::OP_ATAN:  A=VUn(reg[in.b],[](float x){return std::atan(x);}); break;
                case NkSLOp::OP_RADIANS:A=VUn(reg[in.b],[](float x){return x*0.01745329252f;}); break;
                case NkSLOp::OP_DEGREES:A=VUn(reg[in.b],[](float x){return x*57.2957795131f;}); break;
                case NkSLOp::OP_TRANSPOSE:A=VTranspose(reg[in.b]); break;
                case NkSLOp::OP_INVERSE: A=(reg[in.b].count==9)?VInverse3(reg[in.b]):reg[in.b]; break;

                case NkSLOp::OP_TEX_SAMPLE: {
                    const NkSLValue& uv=reg[in.b];
                    float lod = 0.f;
                    NkSLValue r; r.count=4; r.v[0]=r.v[1]=r.v[2]=0.f; r.v[3]=1.f;
                    if (env.sampleTex) env.sampleTex(env.ctx, in.imm, uv.v[0], uv.v[1], lod, r.v);
                    A=r; break;
                }
                case NkSLOp::OP_TEX_SAMPLE_SHADOW: {
                    const NkSLValue& uvz=reg[in.b];
                    float s = 1.f;
                    if (env.sampleShadow) s = env.sampleShadow(env.ctx, in.imm, uvz.v[0], uvz.v[1], uvz.v[2]);
                    A=VScalar(s); break;
                }
                case NkSLOp::OP_TEX_SIZE: {
                    NkSLValue r; r.count=2; r.v[0]=r.v[1]=1.f;
                    if (env.texSize) env.texSize(env.ctx, in.imm, r.v);
                    A=r; break;
                }

                case NkSLOp::OP_JMP:  pc = in.imm - 1; break;
                case NkSLOp::OP_JZ:   if (reg[in.b].v[0] == 0.f) pc = in.imm - 1; break;
                case NkSLOp::OP_JNZ:  if (reg[in.b].v[0] != 0.f) pc = in.imm - 1; break;
                case NkSLOp::OP_DISCARD: env.discarded = true; return false;
                case NkSLOp::OP_RET:  return true;
                default: break;
            }
        }
        return true;
    }

} // namespace nkentseu
