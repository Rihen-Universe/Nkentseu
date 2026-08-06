// =============================================================================
// NkQwen2LoraGpu.cpp — voir le .h pour la portée, le checkpointing et le choix
// du rang.
//
// RÈGLE QUI GOUVERNE CE FICHIER, la même qu'au jalon 5 : chaque noyau reproduit
// une fonction PRÉCISE de la référence CPU — NkQwen2Block.cpp pour le forward,
// NkQwen2Backward.cpp pour les dérivées — dans le MÊME ORDRE de sommation.
// Les seules différences d'indexation viennent de la disposition [T, n·hd] des
// activations (au lieu de [n, T, hd] côté CPU) : elles ne changent AUCUN calcul,
// seulement l'adresse où il est lu.
//
// Tous les noyaux passent par les deux lanceurs génériques publics de NKTensor
// (RunConvOp / RunOp3). Aucune ligne de NKTensor, ni des jalons 1 à 5, n'est
// modifiée.
// =============================================================================
#include "NKInfer/NkQwen2LoraGpu.h"
#include "NKInfer/NkQKGpuBackward.h"
#include "NKInfer/NkGGUFDequant.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {

				void SetErr(NkString *e, const char *msg) {
					if (e)
						*e = NkString(msg);
				}

				float64 NowSeconds() {
					return (float64)std::clock() / (float64)CLOCKS_PER_SEC;
				}

				uint32 FBits(float32 f) {
					uint32 u = 0;
					std::memcpy(&u, &f, sizeof(u));
					return u;
				}

				// -------------------------------------------------------------
				// FORWARD — transcriptions de NkQwen2Block.cpp.
				// (RMSNorm : somme des carrés par paquets de 64, exactement comme
				// au jalon 5 — le CPU accumule en double, le GPU n'a pas de double
				// fiable ; le paquetage divise l'erreur par ~8 sans compensation de
				// Kahan, qu'un optimiseur aurait le droit de réassocier.)
				// -------------------------------------------------------------
				const char *kRmsNorm = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufW { float data[]; } Wn;
@binding(set=0, binding=2) buffer BufY { float data[]; } Y;
@binding(set=0, binding=3) uniform Params {
    uint rows; uint cols; uint epsBits; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < pc.rows) {
        uint base = r * pc.cols;
        float ss = 0.0;
        uint c = 0u;
        while (c + 64u <= pc.cols) {
            float part = 0.0;
            for (uint j = 0u; j < 64u; j = j + 1u) {
                float v = X.data[base + c + j];
                part = part + v * v;
            }
            ss = ss + part;
            c = c + 64u;
        }
        float tail = 0.0;
        while (c < pc.cols) {
            float v = X.data[base + c];
            tail = tail + v * v;
            c = c + 1u;
        }
        ss = ss + tail;
        float eps = uintBitsToFloat(pc.epsBits);
        float inv = 1.0 / sqrt(ss / float(pc.cols) + eps);
        for (uint k = 0u; k < pc.cols; k = k + 1u) {
            Y.data[base + k] = X.data[base + k] * inv * Wn.data[k];
        }
    }
}
)NKSL";

				const char *kAddBias = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufY { float data[]; } Y;
@binding(set=0, binding=3) uniform Params {
    uint total; uint cols; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.total) {
        Y.data[i] = X.data[i] + B.data[i % pc.cols];
    }
}
)NKSL";

				// RoPE NEOX, position = t (l'entraînement part TOUJOURS de 0 : il
				// n'y a pas de KV-cache, donc pas de décalage de position).
				const char *kRope = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufT { float data[]; } Tab;
@binding(set=0, binding=2) buffer BufY { float data[]; } Y;
@binding(set=0, binding=3) uniform Params {
    uint total; uint nH; uint headDim; uint halfd;
    uint maxSeq; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint i = idx % pc.halfd;
        uint rest = idx / pc.halfd;
        uint h = rest % pc.nH;
        uint t = rest / pc.nH;
        uint base = t * pc.nH * pc.headDim + h * pc.headDim;
        float c = Tab.data[t * pc.halfd + i];
        float s = Tab.data[pc.maxSeq * pc.halfd + t * pc.halfd + i];
        float x1 = X.data[base + i];
        float x2 = X.data[base + i + pc.halfd];
        Y.data[base + i] = x1 * c - x2 * s;
        Y.data[base + i + pc.halfd] = x1 * s + x2 * c;
    }
}
)NKSL";

				// Backward du RoPE : la rotation est ORTHOGONALE, donc Rᵀ = R(−θ),
				// soit le forward avec sin changé de signe. Transcription de
				// NkApplyRoPEBackward, aux tables près (calculées en double au
				// chargement, cf jalon 5 — c'est ce qui tient l'erreur à 1e-6).
				const char *kRopeBwd = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufT { float data[]; } Tab;
@binding(set=0, binding=2) buffer BufY { float data[]; } Y;
@binding(set=0, binding=3) uniform Params {
    uint total; uint nH; uint headDim; uint halfd;
    uint maxSeq; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint i = idx % pc.halfd;
        uint rest = idx / pc.halfd;
        uint h = rest % pc.nH;
        uint t = rest / pc.nH;
        uint base = t * pc.nH * pc.headDim + h * pc.headDim;
        float c = Tab.data[t * pc.halfd + i];
        float s = Tab.data[pc.maxSeq * pc.halfd + t * pc.halfd + i];
        float d1 = X.data[base + i];
        float d2 = X.data[base + i + pc.halfd];
        Y.data[base + i] = d1 * c + d2 * s;
        Y.data[base + i + pc.halfd] = 0.0 - d1 * s + d2 * c;
    }
}
)NKSL";

				// Scores GQA, disposition d'ENTRAÎNEMENT : K vit en [T, nKV·hd]
				// (pas de cache). s[h,t,j] = (q_h,t · k_kvh,j)/sqrt(hd), j <= t.
				const char *kScores = R"NKSL(
@binding(set=0, binding=0) buffer BufQ { float data[]; } Q;
@binding(set=0, binding=1) buffer BufK { float data[]; } Kk;
@binding(set=0, binding=2) buffer BufS { float data[]; } S;
@binding(set=0, binding=3) uniform Params {
    uint total; uint T; uint nH; uint headDim;
    uint group; uint qd; uint kvd; uint scaleBits;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint j = idx % pc.T;
        uint rest = idx / pc.T;
        uint t = rest % pc.T;
        uint h = rest / pc.T;
        float acc = 0.0;
        if (j <= t) {
            uint kvh = h / pc.group;
            uint qb = t * pc.qd + h * pc.headDim;
            uint kb = j * pc.kvd + kvh * pc.headDim;
            for (uint d = 0u; d < pc.headDim; d = d + 1u) {
                acc = acc + Q.data[qb + d] * Kk.data[kb + d];
            }
            acc = acc * uintBitsToFloat(pc.scaleBits);
        }
        S.data[idx] = acc;
    }
}
)NKSL";

				const char *kSoftmax = R"NKSL(
@binding(set=0, binding=0) buffer BufS { float data[]; } S;
@binding(set=0, binding=1) buffer BufP { float data[]; } P;
@binding(set=0, binding=2) uniform Params {
    uint rows; uint T; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < pc.rows) {
        uint base = r * pc.T;
        uint last = r % pc.T;
        float mx = S.data[base];
        for (uint c = 1u; c <= last; c = c + 1u) {
            float v = S.data[base + c];
            if (v > mx) {
                mx = v;
            }
        }
        float sum = 0.0;
        for (uint c = 0u; c <= last; c = c + 1u) {
            float e = exp(S.data[base + c] - mx);
            P.data[base + c] = e;
            sum = sum + e;
        }
        float inv = sum > 0.0 ? 1.0 / sum : 0.0;
        for (uint c = 0u; c <= last; c = c + 1u) {
            P.data[base + c] = P.data[base + c] * inv;
        }
        for (uint c = last + 1u; c < pc.T; c = c + 1u) {
            P.data[base + c] = 0.0;
        }
    }
}
)NKSL";

				const char *kAttnOut = R"NKSL(
@binding(set=0, binding=0) buffer BufP { float data[]; } P;
@binding(set=0, binding=1) buffer BufV { float data[]; } Vv;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform Params {
    uint total; uint T; uint nH; uint headDim;
    uint group; uint qd; uint kvd; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint dh = idx % pc.headDim;
        uint rest = idx / pc.headDim;
        uint h = rest % pc.nH;
        uint t = rest / pc.nH;
        uint kvh = h / pc.group;
        uint pb = (h * pc.T + t) * pc.T;
        float acc = 0.0;
        for (uint j = 0u; j < pc.T; j = j + 1u) {
            acc = acc + P.data[pb + j] * Vv.data[j * pc.kvd + kvh * pc.headDim + dh];
        }
        O.data[idx] = acc;
    }
}
)NKSL";

				const char *kAdd = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params {
    uint total; uint p1; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.total) {
        C.data[i] = A.data[i] + B.data[i];
    }
}
)NKSL";

				const char *kMul = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params {
    uint total; uint p1; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.total) {
        C.data[i] = A.data[i] * B.data[i];
    }
}
)NKSL";

				// C[i] = A[i] · W[i % cols] — produit par un poids de norme, diffusé
				// sur le dernier axe. Sert au backward du RMSNorm (le terme w_j·dy_j,
				// factorisé hors du noyau qui suit, qui n'a droit qu'à trois liaisons).
				const char *kMulRow = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufW { float data[]; } Wn;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params {
    uint total; uint cols; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.total) {
        C.data[i] = A.data[i] * Wn.data[i % pc.cols];
    }
}
)NKSL";

				const char *kSwiGlu = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } G;
@binding(set=0, binding=1) buffer BufU { float data[]; } U;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform Params {
    uint total; uint p1; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.total) {
        float g = G.data[i];
        O.data[i] = (g / (1.0 + exp(-g))) * U.data[i];
    }
}
)NKSL";

				// C = A ⊙ silu(B) — le dU du SwiGLU (dU = dH ⊙ silu(g)).
				const char *kSiluMul = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params {
    uint total; uint p1; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.total) {
        float g = B.data[i];
        float sig = 1.0 / (1.0 + exp(-g));
        C.data[i] = A.data[i] * (g * sig);
    }
}
)NKSL";

				// C = A ⊙ silu'(B), silu'(g) = σ(g)·(1 + g·(1−σ(g))) — transcription
				// littérale de NkSwiGLUBackward.
				const char *kDSiluMul = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params {
    uint total; uint p1; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.total) {
        float g = B.data[i];
        float sig = 1.0 / (1.0 + exp(-g));
        C.data[i] = A.data[i] * (sig * (1.0 + g * (1.0 - sig)));
    }
}
)NKSL";

				// Backward du RMSNorm : dx_j = inv·tn_j − (inv³/d)·x_j·Σ_i(x_i·tn_i),
				// où tn = w ⊙ dy a DÉJÀ été calculé par kMulRow. La somme des carrés
				// est refaite ici (par paquets de 64, comme au forward) : la stocker
				// aurait coûté un tampon et une passe de plus pour économiser une
				// lecture déjà en cache.
				const char *kRmsNormBwd = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufT { float data[]; } Tn;
@binding(set=0, binding=2) buffer BufD { float data[]; } D;
@binding(set=0, binding=3) uniform Params {
    uint rows; uint cols; uint epsBits; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < pc.rows) {
        uint base = r * pc.cols;
        float ss = 0.0;
        uint c = 0u;
        while (c + 64u <= pc.cols) {
            float part = 0.0;
            for (uint j = 0u; j < 64u; j = j + 1u) {
                float v = X.data[base + c + j];
                part = part + v * v;
            }
            ss = ss + part;
            c = c + 64u;
        }
        while (c < pc.cols) {
            float v = X.data[base + c];
            ss = ss + v * v;
            c = c + 1u;
        }
        float eps = uintBitsToFloat(pc.epsBits);
        float inv = 1.0 / sqrt(ss / float(pc.cols) + eps);
        float dot = 0.0;
        uint c2 = 0u;
        while (c2 + 64u <= pc.cols) {
            float part = 0.0;
            for (uint j = 0u; j < 64u; j = j + 1u) {
                part = part + X.data[base + c2 + j] * Tn.data[base + c2 + j];
            }
            dot = dot + part;
            c2 = c2 + 64u;
        }
        while (c2 < pc.cols) {
            dot = dot + X.data[base + c2] * Tn.data[base + c2];
            c2 = c2 + 1u;
        }
        float inv3d = inv * inv * inv / float(pc.cols);
        for (uint j = 0u; j < pc.cols; j = j + 1u) {
            D.data[base + j] = inv * Tn.data[base + j] - inv3d * X.data[base + j] * dot;
        }
    }
}
)NKSL";

				// dV[j, kvh·hd+dh] = Σ_{h du groupe} Σ_{t >= j} P[h,t,j]·dCtx[t, h·hd+dh]
				// GQA : les têtes Q d'un groupe ACCUMULENT sur leur tête K/V — ici
				// l'accumulation est faite PAR UN SEUL THREAD, donc sans atomique et
				// dans un ordre déterministe (h croissant, puis t croissant).
				const char *kDV = R"NKSL(
@binding(set=0, binding=0) buffer BufP { float data[]; } P;
@binding(set=0, binding=1) buffer BufG { float data[]; } DC;
@binding(set=0, binding=2) buffer BufO { float data[]; } DV;
@binding(set=0, binding=3) uniform Params {
    uint total; uint T; uint nKV; uint headDim;
    uint group; uint qd; uint kvd; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint dh = idx % pc.headDim;
        uint rest = idx / pc.headDim;
        uint kvh = rest % pc.nKV;
        uint j = rest / pc.nKV;
        float acc = 0.0;
        for (uint gi = 0u; gi < pc.group; gi = gi + 1u) {
            uint h = kvh * pc.group + gi;
            uint pb = h * pc.T * pc.T + j;
            for (uint t = j; t < pc.T; t = t + 1u) {
                acc = acc + P.data[pb + t * pc.T] * DC.data[t * pc.qd + h * pc.headDim + dh];
            }
        }
        DV.data[idx] = acc;
    }
}
)NKSL";

				// dP[h,t,j] = Σ_dh dCtx[t, h·hd+dh]·V[j, kvh·hd+dh]
				const char *kDP = R"NKSL(
@binding(set=0, binding=0) buffer BufG { float data[]; } DC;
@binding(set=0, binding=1) buffer BufV { float data[]; } Vv;
@binding(set=0, binding=2) buffer BufO { float data[]; } DP;
@binding(set=0, binding=3) uniform Params {
    uint total; uint T; uint nH; uint headDim;
    uint group; uint qd; uint kvd; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint j = idx % pc.T;
        uint rest = idx / pc.T;
        uint t = rest % pc.T;
        uint h = rest / pc.T;
        float acc = 0.0;
        if (j <= t) {
            uint kvh = h / pc.group;
            uint gb = t * pc.qd + h * pc.headDim;
            uint vb = j * pc.kvd + kvh * pc.headDim;
            for (uint d = 0u; d < pc.headDim; d = d + 1u) {
                acc = acc + DC.data[gb + d] * Vv.data[vb + d];
            }
        }
        DP.data[idx] = acc;
    }
}
)NKSL";

				// ds_c = scale·P_c·(dP_c − Σ_{c'} P_c'·dP_c') sur les colonnes
				// autorisées, 0 STRICT au-delà. `scale` = 1/√headDim, remontée ici
				// vers les scores bruts — exactement comme NkGQAAttentionBackward.
				const char *kSoftmaxBwd = R"NKSL(
@binding(set=0, binding=0) buffer BufP { float data[]; } P;
@binding(set=0, binding=1) buffer BufD { float data[]; } DP;
@binding(set=0, binding=2) buffer BufO { float data[]; } DS;
@binding(set=0, binding=3) uniform Params {
    uint rows; uint T; uint scaleBits; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < pc.rows) {
        uint base = r * pc.T;
        uint last = r % pc.T;
        float dot = 0.0;
        for (uint c = 0u; c <= last; c = c + 1u) {
            dot = dot + P.data[base + c] * DP.data[base + c];
        }
        float sc = uintBitsToFloat(pc.scaleBits);
        for (uint c = 0u; c <= last; c = c + 1u) {
            DS.data[base + c] = sc * P.data[base + c] * (DP.data[base + c] - dot);
        }
        for (uint c = last + 1u; c < pc.T; c = c + 1u) {
            DS.data[base + c] = 0.0;
        }
    }
}
)NKSL";

				// dQ[t, h·hd+dh] = Σ_{j <= t} dS[h,t,j]·K[j, kvh·hd+dh]
				const char *kDQ = R"NKSL(
@binding(set=0, binding=0) buffer BufS { float data[]; } DS;
@binding(set=0, binding=1) buffer BufK { float data[]; } Kk;
@binding(set=0, binding=2) buffer BufO { float data[]; } DQ;
@binding(set=0, binding=3) uniform Params {
    uint total; uint T; uint nH; uint headDim;
    uint group; uint qd; uint kvd; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint dh = idx % pc.headDim;
        uint rest = idx / pc.headDim;
        uint h = rest % pc.nH;
        uint t = rest / pc.nH;
        uint kvh = h / pc.group;
        uint sb = (h * pc.T + t) * pc.T;
        float acc = 0.0;
        for (uint j = 0u; j <= t; j = j + 1u) {
            acc = acc + DS.data[sb + j] * Kk.data[j * pc.kvd + kvh * pc.headDim + dh];
        }
        DQ.data[idx] = acc;
    }
}
)NKSL";

				// dK[j, kvh·hd+dh] = Σ_{h du groupe} Σ_{t >= j} dS[h,t,j]·Q[t, h·hd+dh]
				const char *kDK = R"NKSL(
@binding(set=0, binding=0) buffer BufS { float data[]; } DS;
@binding(set=0, binding=1) buffer BufQ { float data[]; } Q;
@binding(set=0, binding=2) buffer BufO { float data[]; } DK;
@binding(set=0, binding=3) uniform Params {
    uint total; uint T; uint nKV; uint headDim;
    uint group; uint qd; uint kvd; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint dh = idx % pc.headDim;
        uint rest = idx / pc.headDim;
        uint kvh = rest % pc.nKV;
        uint j = rest / pc.nKV;
        float acc = 0.0;
        for (uint gi = 0u; gi < pc.group; gi = gi + 1u) {
            uint h = kvh * pc.group + gi;
            uint sb = h * pc.T * pc.T + j;
            for (uint t = j; t < pc.T; t = t + 1u) {
                acc = acc + DS.data[sb + t * pc.T] * Q.data[t * pc.qd + h * pc.headDim + dh];
            }
        }
        DK.data[idx] = acc;
    }
}
)NKSL";

				// -------------------------------------------------------------
				// Cross-entropie MASQUÉE, un thread par POSITION.
				//
				// mode 0 -> OUT[t] = perte de la ligne t (0 si masquée)
				// mode 1 -> OUT[t·V + c] = (softmax − onehot)/nActives, et 0
				//           EXACTEMENT sur les lignes masquées.
				//
				// POURQUOI LE GRADIENT DOIT ÊTRE STRUCTURELLEMENT NUL sur le prompt,
				// et pas un one-hot mis à zéro : dLogits = softmax − onehot ; avec un
				// one-hot nul le gradient vaudrait softmax ≠ 0 et pousserait les
				// positions du prompt vers l'uniforme. C'est le piège du patch
				// 03_LOSS_MASKING, évité ici en ne touchant la ligne que pour la
				// mettre à zéro.
				//
				// EN MODE 1, OUT EST LE TAMPON DE LOGITS LUI-MÊME. Ce n'est pas une
				// aliasation dangereuse : un thread ne lit et n'écrit que SA ligne, et
				// il a fini de lire (max, somme) avant d'écrire. Les logits ne servent
				// plus après — économie de 78 Mo à T=128.
				// -------------------------------------------------------------
				const char *kCrossEntropy = R"NKSL(
@binding(set=0, binding=0) buffer BufL { float data[]; } L;
@binding(set=0, binding=1) buffer BufM { float data[]; } Meta;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform Params {
    uint rows; uint V; uint mode; uint invActBits;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < pc.rows) {
        uint base = r * pc.V;
        float msk = Meta.data[pc.rows + r];
        if (msk == 0.0) {
            if (pc.mode == 0u) {
                O.data[r] = 0.0;
            } else {
                for (uint c = 0u; c < pc.V; c = c + 1u) {
                    O.data[base + c] = 0.0;
                }
            }
        } else {
            uint tgt = uint(Meta.data[r]);
            float mx = L.data[base];
            for (uint c = 1u; c < pc.V; c = c + 1u) {
                float v = L.data[base + c];
                if (v > mx) {
                    mx = v;
                }
            }
            float sum = 0.0;
            for (uint c = 0u; c < pc.V; c = c + 1u) {
                sum = sum + exp(L.data[base + c] - mx);
            }
            if (pc.mode == 0u) {
                O.data[r] = (mx + log(sum)) - L.data[base + tgt];
            } else {
                float ia = uintBitsToFloat(pc.invActBits);
                float invSum = 1.0 / sum;
                for (uint c = 0u; c < pc.V; c = c + 1u) {
                    O.data[base + c] = exp(L.data[base + c] - mx) * invSum * ia;
                }
                O.data[base + tgt] = O.data[base + tgt] - ia;
            }
        }
    }
}
)NKSL";

				const char *kCopyRow = R"NKSL(
@binding(set=0, binding=0) buffer BufS { float data[]; } S;
@binding(set=0, binding=1) buffer BufD { float data[]; } D;
@binding(set=0, binding=2) uniform Params {
    uint count; uint srcOff; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        D.data[i] = S.data[pc.srcOff + i];
    }
}
)NKSL";

				// Les sources sont converties en NkString UNE fois (statiques
				// locales) : le pipeline est déjà mis en cache par nom côté
				// NkTensorGpu, mais un pas d'entraînement lance ces noyaux des
				// milliers de fois.
#define NK_LG_SRC(fn, var)                                                                                             \
	const NkString &fn() {                                                                                             \
		static NkString s(var);                                                                                        \
		return s;                                                                                                      \
	}
				NK_LG_SRC(SrcRmsNorm, kRmsNorm)
				NK_LG_SRC(SrcAddBias, kAddBias)
				NK_LG_SRC(SrcRope, kRope)
				NK_LG_SRC(SrcRopeBwd, kRopeBwd)
				NK_LG_SRC(SrcScores, kScores)
				NK_LG_SRC(SrcSoftmax, kSoftmax)
				NK_LG_SRC(SrcAttnOut, kAttnOut)
				NK_LG_SRC(SrcAdd, kAdd)
				NK_LG_SRC(SrcMul, kMul)
				NK_LG_SRC(SrcMulRow, kMulRow)
				NK_LG_SRC(SrcSwiGlu, kSwiGlu)
				NK_LG_SRC(SrcSiluMul, kSiluMul)
				NK_LG_SRC(SrcDSiluMul, kDSiluMul)
				NK_LG_SRC(SrcRmsNormBwd, kRmsNormBwd)
				NK_LG_SRC(SrcDV, kDV)
				NK_LG_SRC(SrcDP, kDP)
				NK_LG_SRC(SrcSoftmaxBwd, kSoftmaxBwd)
				NK_LG_SRC(SrcDQ, kDQ)
				NK_LG_SRC(SrcDK, kDK)
				NK_LG_SRC(SrcCE, kCrossEntropy)
				NK_LG_SRC(SrcCopyRow, kCopyRow)
#undef NK_LG_SRC

			} // namespace

			// =============================================================
			// Cycle de vie / chargement
			// =============================================================
			NkQwen2LoraGpu::~NkQwen2LoraGpu() {
				Release();
			}

			const char *NkQwen2LoraGpu::BackendName() const {
				return NkTensorGpu::Get().BackendName();
			}

			uint64 NkQwen2LoraGpu::NewBuffer(uint64 bytes, uint64 &accumulator) {
				const uint64 alloc = (bytes + 15ull) & ~15ull;
				uint64 id = NkTensorGpu::Get().CreateBuffer((nk_size)alloc);
				if (id != 0) {
					accumulator += alloc;
					mStats.bufferCount++;
				}
				return id;
			}

			void NkQwen2LoraGpu::Release() {
				NkTensorGpu &gpu = NkTensorGpu::Get();
				auto kill = [&gpu](uint64 &b) {
					if (b != 0) {
						gpu.DestroyBuffer(b);
						b = 0;
					}
				};
				for (uint32 i = 0; i < mLayers.Size(); ++i) {
					Layer &L = mLayers[i];
					NkQwen2GpuWeight *ws[7] = {&L.wq, &L.wk, &L.wv, &L.wo, &L.wGate, &L.wUp, &L.wDown};
					for (int k = 0; k < 7; ++k) {
						if (ws[k]->kind == NkQwen2GpuQuant::NK_Q4K)
							NkQ4KGpuRelease(ws[k]->q4);
						else if (ws[k]->kind == NkQwen2GpuQuant::NK_Q6K)
							NkQ6KGpuRelease(ws[k]->q6);
						ws[k]->kind = NkQwen2GpuQuant::NK_NONE;
					}
					kill(L.attnNorm);
					kill(L.ffnNorm);
					kill(L.bq);
					kill(L.bk);
					kill(L.bv);
				}
				mLayers.Clear();
				for (uint32 i = 0; i < mLora.Size(); ++i)
					for (int32 k = 0; k < NkLoraGpuSet::kCount; ++k)
						NkLoraGpuRelease(*mLora[i].At(k));
				mLora.Clear();
				for (uint32 i = 0; i < mCkpt.Size(); ++i)
					kill(mCkpt[i]);
				mCkpt.Clear();
				if (mLmHead.kind == NkQwen2GpuQuant::NK_Q4K)
					NkQ4KGpuRelease(mLmHead.q4);
				else if (mLmHead.kind == NkQwen2GpuQuant::NK_Q6K)
					NkQ6KGpuRelease(mLmHead.q6);
				mLmHead.kind = NkQwen2GpuQuant::NK_NONE;
				kill(mOutputNorm);
				uint64 *scratch[] = {&mBufX,	  &mBufX1,	  &mBufXn1,		&mBufXn2,	 &mBufAttn,	  &mBufDown,
									 &mBufQ,	  &mBufQb,	  &mBufQr,		&mBufCtx,	 &mBufK,	  &mBufKb,
									 &mBufKr,	  &mBufV,	  &mBufVb,		&mBufScores, &mBufProbs,  &mBufGate,
									 &mBufUp,	  &mBufAct,	  &mBufRope,	&mBufLogits, &mBufRow,	  &mBufDX,
									 &mBufDX1,	  &mBufDXn,	  &mBufDTmp,	&mBufDNrm,	 &mBufDAct,	  &mBufDGate,
									 &mBufDUp,	  &mBufDTmpF, &mBufDCtx,	&mBufDQr,	 &mBufDQ,	  &mBufDK,
									 &mBufDKr,	  &mBufDV,	  &mBufDScores, &mBufDProbs, &mBufU,	  &mBufDU,
									 &mBufMeta,	  &mBufLoss,  &mBufNormPart};
				for (uint32 i = 0; i < sizeof(scratch) / sizeof(scratch[0]); ++i)
					kill(*scratch[i]);
				mLoaded = false;
				mEmbedInfo = nullptr;
			}

			const NkGGUFTensorInfo *NkQwen2LoraGpu::Find(const char *name) const {
				for (uint32 i = 0; i < mGguf.tensors.Size(); ++i)
					if (mGguf.tensors[i].name.Compare(name) == 0)
						return &mGguf.tensors[i];
				return nullptr;
			}

			bool NkQwen2LoraGpu::UploadQuant(const NkGGUFTensorInfo &info, NkQwen2GpuWeight &out, NkString *err) {
				out = NkQwen2GpuWeight{};
				if (info.dims.Size() != 2) {
					SetErr(err, "UploadQuant : un poids de projection doit avoir 2 dimensions");
					return false;
				}
				const int64 cols = (int64)info.dims[0]; // ne[0] = in_features (contigu)
				const int64 rows = (int64)info.dims[1]; // ne[1] = out_features
				NkVector<uint8> raw;
				const float64 t0 = NowSeconds();
				if (!NkGGUFReadTensorRawBytes(mPath.CStr(), mGguf, info, raw, err))
					return false;
				mStats.readSeconds += NowSeconds() - t0;
				if (info.rawType == (uint32)NkGGUFTensorType::NK_GGML_Q4_K) {
					if (!NkQ4KGpuUpload(raw.Data(), (uint64)raw.Size(), rows, cols, out.q4, err))
						return false;
					out.kind = NkQwen2GpuQuant::NK_Q4K;
					mStats.weightBytes += (out.q4.byteCount + 15ull) & ~15ull;
					mStats.q4Tensors++;
				} else if (info.rawType == (uint32)NkGGUFTensorType::NK_GGML_Q6_K) {
					if (!NkQ6KGpuUpload(raw.Data(), (uint64)raw.Size(), rows, cols, out.q6, err))
						return false;
					out.kind = NkQwen2GpuQuant::NK_Q6K;
					mStats.weightBytes += (out.q6.byteCount + 15ull) & ~15ull;
					mStats.q6Tensors++;
				} else {
					SetErr(err, "UploadQuant : format non supporté (Q4_K/Q6_K seulement)");
					return false;
				}
				mStats.bufferCount++;
				return true;
			}

			bool NkQwen2LoraGpu::UploadF32(const NkGGUFTensorInfo &info, uint64 &outBuffer, int64 expectedNumel,
										   NkString *err) {
				NkVector<float32> data;
				NkVector<uint8> raw;
				const float64 t0 = NowSeconds();
				if (!NkGGUFReadTensorRawBytes(mPath.CStr(), mGguf, info, raw, err))
					return false;
				mStats.readSeconds += NowSeconds() - t0;
				if (!NkGGUFDequantizeRaw(info.rawType, raw.Data(), (uint64)raw.Size(), info.numElements, data, err))
					return false;
				if (expectedNumel > 0 && (int64)data.Size() != expectedNumel) {
					SetErr(err, "UploadF32 : nombre d'éléments inattendu");
					return false;
				}
				outBuffer = NewBuffer((uint64)data.Size() * sizeof(float32), mStats.weightBytes);
				if (outBuffer == 0) {
					SetErr(err, "UploadF32 : CreateBuffer a échoué");
					return false;
				}
				if (!NkTensorGpu::Get().Upload(outBuffer, data.Data(),
											   (nk_size)((uint64)data.Size() * sizeof(float32)))) {
					SetErr(err, "UploadF32 : Upload a échoué");
					return false;
				}
				mStats.f32Tensors++;
				return true;
			}

			void NkQwen2LoraGpu::BuildRopeTables() {
				// Formule EXACTE de NkApplyRoPE, calculée en DOUBLE comme elle :
				// theta = pos · freqBase^(-2i/headDim). Un pow() en float dans le
				// shader décalerait theta de ~1e-6 relatif, donc une erreur d'ANGLE
				// qui grandit avec la position (leçon chiffrée du jalon 5).
				const int64 half = mCfg.headDim / 2;
				const int64 maxSeq = mOpt.maxSeqLen;
				NkVector<float32> tab;
				tab.Resize((NkVector<float32>::SizeType)(maxSeq * half * 2));
				for (int64 pos = 0; pos < maxSeq; ++pos) {
					for (int64 i = 0; i < half; ++i) {
						const double theta =
							(double)pos * std::pow((double)mCfg.ropeFreqBase, -2.0 * (double)i / (double)mCfg.headDim);
						tab[(NkVector<float32>::SizeType)(pos * half + i)] = (float32)std::cos(theta);
						tab[(NkVector<float32>::SizeType)(maxSeq * half + pos * half + i)] = (float32)std::sin(theta);
					}
				}
				NkTensorGpu::Get().Upload(mBufRope, tab.Data(), (nk_size)((uint64)tab.Size() * sizeof(float32)));
			}

			bool NkQwen2LoraGpu::AllocScratch(NkString *err) {
				const int64 d = mCfg.dModel;
				const int64 qd = (int64)mCfg.nHeads * mCfg.headDim;
				const int64 kvd = (int64)mCfg.nKVHeads * mCfg.headDim;
				const int64 ffn = mCfg.ffnDim;
				const int64 mT = mOpt.maxSeqLen;
				const int64 att = (int64)mCfg.nHeads * mT * mT;
				const uint64 F = sizeof(float32);
				const int64 r = mOpt.loraRank;

				struct {
						uint64 *slot;
						uint64 elems;
				} req[] = {
					{&mBufX, (uint64)(mT * d)},
					{&mBufX1, (uint64)(mT * d)},
					{&mBufXn1, (uint64)(mT * d)},
					{&mBufXn2, (uint64)(mT * d)},
					{&mBufAttn, (uint64)(mT * d)},
					{&mBufDown, (uint64)(mT * d)},
					{&mBufQ, (uint64)(mT * qd)},
					{&mBufQb, (uint64)(mT * qd)},
					{&mBufQr, (uint64)(mT * qd)},
					{&mBufCtx, (uint64)(mT * qd)},
					{&mBufK, (uint64)(mT * kvd)},
					{&mBufKb, (uint64)(mT * kvd)},
					{&mBufKr, (uint64)(mT * kvd)},
					{&mBufV, (uint64)(mT * kvd)},
					{&mBufVb, (uint64)(mT * kvd)},
					{&mBufScores, (uint64)att},
					{&mBufProbs, (uint64)att},
					{&mBufGate, (uint64)(mT * ffn)},
					{&mBufUp, (uint64)(mT * ffn)},
					{&mBufAct, (uint64)(mT * ffn)},
					{&mBufRope, (uint64)(mT * (mCfg.headDim / 2) * 2)},
					{&mBufLogits, (uint64)(mT * mVocabSize)},
					{&mBufRow, (uint64)mVocabSize},
					{&mBufDX, (uint64)(mT * d)},
					{&mBufDX1, (uint64)(mT * d)},
					{&mBufDXn, (uint64)(mT * d)},
					{&mBufDTmp, (uint64)(mT * d)},
					{&mBufDNrm, (uint64)(mT * d)},
					{&mBufDAct, (uint64)(mT * ffn)},
					{&mBufDGate, (uint64)(mT * ffn)},
					{&mBufDUp, (uint64)(mT * ffn)},
					{&mBufDTmpF, (uint64)(mT * ffn)},
					{&mBufDCtx, (uint64)(mT * qd)},
					{&mBufDQr, (uint64)(mT * qd)},
					{&mBufDQ, (uint64)(mT * qd)},
					{&mBufDK, (uint64)(mT * kvd)},
					{&mBufDKr, (uint64)(mT * kvd)},
					{&mBufDV, (uint64)(mT * kvd)},
					{&mBufDScores, (uint64)att},
					{&mBufDProbs, (uint64)att},
					{&mBufU, (uint64)(mT * r)},
					{&mBufDU, (uint64)(mT * r)},
					{&mBufMeta, (uint64)(2 * mT)},
					{&mBufLoss, (uint64)mT},
					{&mBufNormPart, 512ull},
				};
				for (uint32 i = 0; i < sizeof(req) / sizeof(req[0]); ++i) {
					*req[i].slot = NewBuffer(req[i].elems * F, mStats.scratchBytes);
					if (*req[i].slot == 0) {
						SetErr(err, "AllocScratch : CreateBuffer a échoué (VRAM insuffisante ?)");
						return false;
					}
				}
				return true;
			}

			bool NkQwen2LoraGpu::CreateAdapters(NkString *err) {
				const int32 d = mCfg.dModel;
				const int32 qd = mCfg.nHeads * mCfg.headDim;
				const int32 kvd = mCfg.nKVHeads * mCfg.headDim;
				const int32 ffn = mCfg.ffnDim;
				const int32 r = mOpt.loraRank;
				const float32 a = mOpt.loraAlpha;
				const float32 sg = mOpt.loraSigma;
				NkLoraRng rng(mOpt.loraSeed);

				mLora.Resize((NkVector<NkLoraGpuSet>::SizeType)mLayers.Size());
				uint64 allBytes = 0;
				for (uint32 l = 0; l < mLayers.Size(); ++l) {
					NkLoraGpuSet &S = mLora[l];
					// (out, in) de chaque projection, dans l'ordre canonique
					// q,k,v,o,gate,up,down — MÊME convention que le GGUF : [out, in].
					const int32 outs[7] = {qd, kvd, kvd, d, ffn, ffn, d};
					const int32 ins[7] = {d, d, d, qd, d, d, ffn};
					for (int32 i = 0; i < NkLoraGpuSet::kCount; ++i) {
						if (!NkLoraGpuCreate(*S.At(i), outs[i], ins[i], r, a, sg, rng, true, &allBytes, err))
							return false;
						mStats.loraParams += S.At(i)->Params();
					}
				}
				// Sur les huit tampons d'une paire, DEUX portent les paramètres et
				// SIX l'optimisation (gradients + deux moments). On sépare les deux
				// comptes pour que le chiffre publié ne mélange pas « ce qu'on
				// apprend » et « ce que ça coûte de l'apprendre ».
				mStats.loraBytes = allBytes / 4;
				mStats.optimBytes = allBytes - allBytes / 4;
				return true;
			}

			bool NkQwen2LoraGpu::Load(const char *ggufPath, const NkQwen2LoraGpuOptions &opt, NkString *err) {
				Release();
				mStats = NkQwen2LoraGpuStats{};
				mOpt = opt;
				mPath = NkString(ggufPath);
				const float64 t0 = NowSeconds();

				NkTensorGpu &gpu = NkTensorGpu::Get();
				if (!gpu.IsAvailable()) {
					SetErr(err, "NkQwen2LoraGpu::Load : aucun device compute GPU disponible");
					return false;
				}
				if (!NkGGUFLoader::Load(ggufPath, mGguf) || !mGguf.valid) {
					SetErr(err, "NkQwen2LoraGpu::Load : GGUF illisible");
					return false;
				}

				uint64 blockCount = 0, dModel = 0, ffnDim = 0, headCount = 0, headCountKv = 0;
				float64 ropeFreqBase = 0.0, rmsEps = 0.0;
				const bool okMeta = NkGGUFGetUInt(mGguf, "qwen2.block_count", blockCount) &&
									NkGGUFGetUInt(mGguf, "qwen2.embedding_length", dModel) &&
									NkGGUFGetUInt(mGguf, "qwen2.feed_forward_length", ffnDim) &&
									NkGGUFGetUInt(mGguf, "qwen2.attention.head_count", headCount) &&
									NkGGUFGetUInt(mGguf, "qwen2.attention.head_count_kv", headCountKv) &&
									NkGGUFGetFloat(mGguf, "qwen2.rope.freq_base", ropeFreqBase) &&
									NkGGUFGetFloat(mGguf, "qwen2.attention.layer_norm_rms_epsilon", rmsEps);
				if (!okMeta) {
					SetErr(err, "NkQwen2LoraGpu::Load : métadonnées qwen2.* incomplètes");
					return false;
				}
				mCfg.dModel = (int32)dModel;
				mCfg.nHeads = (int32)headCount;
				mCfg.nKVHeads = (int32)headCountKv;
				mCfg.headDim = (int32)(dModel / headCount);
				mCfg.ffnDim = (int32)ffnDim;
				mCfg.ropeFreqBase = (float32)ropeFreqBase;
				mCfg.rmsEps = (float32)rmsEps;
				if (!mCfg.IsValid()) {
					SetErr(err, "NkQwen2LoraGpu::Load : NkQwen2Config incohérente");
					return false;
				}

				mEmbedInfo = Find("token_embd.weight");
				if (!mEmbedInfo || mEmbedInfo->dims.Size() != 2 || !mEmbedInfo->sizeKnown) {
					SetErr(err, "NkQwen2LoraGpu::Load : token_embd.weight absent");
					return false;
				}
				mVocabSize = (int64)mEmbedInfo->dims[1];
				mEmbedBytesPerRow = mEmbedInfo->sizeBytes / (uint64)mVocabSize;

				uint32 nLayers = (uint32)blockCount;
				if (mOpt.maxLayers > 0 && mOpt.maxLayers < nLayers)
					nLayers = mOpt.maxLayers;

				if (mOpt.verbose) {
					printf("  [NkQwen2LoraGpu] backend = %s\n", gpu.BackendName());
					printf("  [NkQwen2LoraGpu] L=%u d=%d ffn=%d nH=%d nKV=%d hd=%d vocab=%lld maxT=%d r=%d alpha=%g\n",
						   nLayers, mCfg.dModel, mCfg.ffnDim, mCfg.nHeads, mCfg.nKVHeads, mCfg.headDim,
						   (long long)mVocabSize, mOpt.maxSeqLen, mOpt.loraRank, (double)mOpt.loraAlpha);
				}

				if (!AllocScratch(err))
					return false;
				BuildRopeTables();

				const int64 d = mCfg.dModel;
				const int64 qd = (int64)mCfg.nHeads * mCfg.headDim;
				const int64 kvd = (int64)mCfg.nKVHeads * mCfg.headDim;
				mLayers.Resize((NkVector<Layer>::SizeType)nLayers);
				char name[160];
				for (uint32 l = 0; l < nLayers; ++l) {
					Layer &L = mLayers[l];
					struct {
							const char *suffix;
							NkQwen2GpuWeight *dst;
					} quant[] = {
						{"attn_q.weight", &L.wq},		{"attn_k.weight", &L.wk},	  {"attn_v.weight", &L.wv},
						{"attn_output.weight", &L.wo},	{"ffn_gate.weight", &L.wGate}, {"ffn_up.weight", &L.wUp},
						{"ffn_down.weight", &L.wDown},
					};
					for (uint32 i = 0; i < sizeof(quant) / sizeof(quant[0]); ++i) {
						std::snprintf(name, sizeof(name), "blk.%u.%s", l, quant[i].suffix);
						const NkGGUFTensorInfo *info = Find(name);
						if (!info) {
							SetErr(err, "NkQwen2LoraGpu::Load : tenseur de couche introuvable");
							return false;
						}
						if (!UploadQuant(*info, *quant[i].dst, err))
							return false;
					}
					struct {
							const char *suffix;
							uint64 *dst;
							int64 numel;
					} plain[] = {
						{"attn_norm.weight", &L.attnNorm, d}, {"ffn_norm.weight", &L.ffnNorm, d},
						{"attn_q.bias", &L.bq, qd},			  {"attn_k.bias", &L.bk, kvd},
						{"attn_v.bias", &L.bv, kvd},
					};
					for (uint32 i = 0; i < sizeof(plain) / sizeof(plain[0]); ++i) {
						std::snprintf(name, sizeof(name), "blk.%u.%s", l, plain[i].suffix);
						const NkGGUFTensorInfo *info = Find(name);
						if (!info) {
							SetErr(err, "NkQwen2LoraGpu::Load : norme/biais introuvable");
							return false;
						}
						if (!UploadF32(*info, *plain[i].dst, plain[i].numel, err))
							return false;
					}
					if (mOpt.verbose && (l % 7 == 0 || l + 1 == nLayers))
						printf("    couche %u/%u chargée (%.1f s, %.2f Go)\n", l + 1, nLayers, NowSeconds() - t0,
							   (double)mStats.TotalBytes() / 1073741824.0);
				}

				{
					const NkGGUFTensorInfo *on = Find("output_norm.weight");
					if (!on || !UploadF32(*on, mOutputNorm, d, err)) {
						SetErr(err, "NkQwen2LoraGpu::Load : output_norm.weight absent ou illisible");
						return false;
					}
				}
				{
					const NkGGUFTensorInfo *out = Find("output.weight");
					mTied = (out == nullptr);
					const NkGGUFTensorInfo *src = mTied ? mEmbedInfo : out;
					if (!UploadQuant(*src, mLmHead, err))
						return false;
					if (mOpt.verbose)
						printf("    lm_head = %s (%s, %lld x %lld, %.0f Mo)\n",
							   mTied ? "token_embd.weight (lié)" : "output.weight",
							   NkGGUFTensorTypeName(src->rawType), (long long)mLmHead.Rows(),
							   (long long)mLmHead.Cols(), (double)mLmHead.Bytes() / 1048576.0);
				}

				// Checkpoints : L+1 tampons [maxT, d]. Le dernier porte la SORTIE de
				// la dernière couche, dont le backward du RMSNorm final a besoin.
				mCkpt.Resize((NkVector<uint64>::SizeType)(nLayers + 1));
				for (uint32 i = 0; i <= nLayers; ++i) {
					mCkpt[i] = NewBuffer((uint64)mOpt.maxSeqLen * (uint64)d * sizeof(float32), mStats.ckptBytes);
					if (mCkpt[i] == 0) {
						SetErr(err, "NkQwen2LoraGpu::Load : allocation des checkpoints a échoué");
						return false;
					}
				}

				if (!CreateAdapters(err))
					return false;

				mLoaded = true;
				mStats.loadSeconds = NowSeconds() - t0;
				if (mOpt.verbose)
					printf("    adaptateurs : %lld paramètres entraînables (%.0f Mo) + %.0f Mo d'état Adam\n",
						   (long long)mStats.loraParams, (double)mStats.loraBytes / 1048576.0,
						   (double)mStats.optimBytes / 1048576.0);
				return true;
			}

			// =============================================================
			// Embeddings (lecture de LIGNES dans le fichier — cf jalon 5)
			// =============================================================
			bool NkQwen2LoraGpu::EmbedTokens(const int32 *ids, int32 count, NkTensor &out, NkString *err) {
				if (!mEmbedInfo || count <= 0) {
					SetErr(err, "EmbedTokens : modèle non chargé ou count nul");
					return false;
				}
				const int64 rowLen = (int64)mEmbedInfo->dims[0];
				NkFile f(mPath.CStr(), NkFileMode::NK_READ_BINARY);
				if (!f.IsOpen()) {
					SetErr(err, "EmbedTokens : ouverture du GGUF impossible");
					return false;
				}
				out = NkTensor::Zeros(NkShape{(int64)count, rowLen});
				NkVector<uint8> raw;
				raw.Resize((NkVector<uint8>::SizeType)mEmbedBytesPerRow);
				NkVector<float32> row;
				for (int32 i = 0; i < count; ++i) {
					const int64 tok = (int64)ids[i];
					if (tok < 0 || tok >= mVocabSize) {
						SetErr(err, "EmbedTokens : identifiant hors vocabulaire");
						return false;
					}
					const uint64 off = mGguf.tensorDataOffset + mEmbedInfo->offset + (uint64)tok * mEmbedBytesPerRow;
					if (!f.Seek((nk_int64)off, NkSeekOrigin::NK_BEGIN) ||
						f.Read(raw.Data(), (usize)mEmbedBytesPerRow) != (usize)mEmbedBytesPerRow) {
						SetErr(err, "EmbedTokens : lecture de la ligne d'embedding a échoué");
						return false;
					}
					if (!NkGGUFDequantizeRaw(mEmbedInfo->rawType, raw.Data(), (uint64)raw.Size(), (uint64)rowLen, row,
											 err))
						return false;
					std::memcpy(out.DataAs<float>() + (int64)i * rowLen, row.Data(), (usize)rowLen * sizeof(float));
				}
				return true;
			}

			// =============================================================
			// Produits avec le socle gelé
			// =============================================================
			bool NkQwen2LoraGpu::Matmul(const NkQwen2GpuWeight &w, uint64 xBuf, uint64 yBuf, int64 M, NkString *err) {
				if (w.kind == NkQwen2GpuQuant::NK_Q4K)
					return NkQ4KGpuMatmul(w.q4, xBuf, yBuf, M, err);
				if (w.kind == NkQwen2GpuQuant::NK_Q6K)
					return NkQ6KGpuMatmul(w.q6, xBuf, yBuf, M, err);
				SetErr(err, "Matmul : poids non résident");
				return false;
			}

			bool NkQwen2LoraGpu::MatmulT(const NkQwen2GpuWeight &w, uint64 dyBuf, uint64 dxBuf, int64 M, bool accum,
										 NkString *err) {
				if (w.kind == NkQwen2GpuQuant::NK_Q4K)
					return NkQ4KGpuMatmulT(w.q4, dyBuf, dxBuf, M, accum, err);
				if (w.kind == NkQwen2GpuQuant::NK_Q6K)
					return NkQ6KGpuMatmulT(w.q6, dyBuf, dxBuf, M, accum, err);
				SetErr(err, "MatmulT : poids non résident");
				return false;
			}

			// =============================================================
			// Forward d'UNE couche — sert au forward complet ET au rejeu du
			// backward. UNE SEULE écriture de ce code : les deux ne peuvent
			// donc pas diverger, ce qui rendrait le gradient faux sans rien
			// casser de visible.
			// =============================================================
			bool NkQwen2LoraGpu::LayerForward(uint32 l, uint64 xBuf, uint64 outBuf, int64 T, NkString *err) {
				NkTensorGpu &gpu = NkTensorGpu::Get();
				const Layer &L = mLayers[l];
				const NkLoraGpuSet &A = mLora[l];
				const int64 d = mCfg.dModel;
				const int64 hd = mCfg.headDim;
				const int64 nH = mCfg.nHeads;
				const int64 nKV = mCfg.nKVHeads;
				const int64 qd = nH * hd;
				const int64 kvd = nKV * hd;
				const int64 ffn = mCfg.ffnDim;
				const int64 half = hd / 2;
				const int64 group = nH / nKV;
				const uint32 eps = FBits(mCfg.rmsEps);
				const uint32 scale = FBits((float32)(1.0 / std::sqrt((double)hd)));
				uint32 p[12];
				auto zero = [&p]() {
					for (int i = 0; i < 12; ++i)
						p[i] = 0;
				};
				bool ok = true;

				// --- pré-norme d'attention ---
				zero();
				p[0] = (uint32)T;
				p[1] = (uint32)d;
				p[2] = eps;
				ok = gpu.RunOp3("lg_rmsnorm", SrcRmsNorm(), xBuf, L.attnNorm, mBufXn1, p, (uint32)T);

				// --- Q/K/V : socle, puis biais, puis delta LoRA -------------
				// L'ORDRE COMPTE, et c'est celui du jalon 2 : y = W₀x + b, PUIS
				// + delta. Avec B = 0 le delta est nul, donc le chemin est
				// bit-pour-bit celui de l'inférence tant que rien n'a appris.
				ok = ok && Matmul(L.wq, mBufXn1, mBufQ, T, err);
				ok = ok && Matmul(L.wk, mBufXn1, mBufK, T, err);
				ok = ok && Matmul(L.wv, mBufXn1, mBufV, T, err);
				if (ok) {
					zero();
					p[0] = (uint32)(T * qd);
					p[1] = (uint32)qd;
					ok = gpu.RunOp3("lg_addbias", SrcAddBias(), mBufQ, L.bq, mBufQb, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * kvd);
					p[1] = (uint32)kvd;
					ok = gpu.RunOp3("lg_addbias", SrcAddBias(), mBufK, L.bk, mBufKb, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * kvd);
					p[1] = (uint32)kvd;
					ok = gpu.RunOp3("lg_addbias", SrcAddBias(), mBufV, L.bv, mBufVb, p, p[0]);
				}
				ok = ok && NkLoraGpuForward(A.q, mBufXn1, mBufQb, T, mBufU, err);
				ok = ok && NkLoraGpuForward(A.k, mBufXn1, mBufKb, T, mBufU, err);
				ok = ok && NkLoraGpuForward(A.v, mBufXn1, mBufVb, T, mBufU, err);

				// --- RoPE sur Q et K (jamais sur V : ce sont des valeurs) ---
				if (ok) {
					zero();
					p[0] = (uint32)(T * nH * half);
					p[1] = (uint32)nH;
					p[2] = (uint32)hd;
					p[3] = (uint32)half;
					p[4] = (uint32)mOpt.maxSeqLen;
					ok = gpu.RunOp3("lg_rope", SrcRope(), mBufQb, mBufRope, mBufQr, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * nKV * half);
					p[1] = (uint32)nKV;
					p[2] = (uint32)hd;
					p[3] = (uint32)half;
					p[4] = (uint32)mOpt.maxSeqLen;
					ok = gpu.RunOp3("lg_rope", SrcRope(), mBufKb, mBufRope, mBufKr, p, p[0]);
				}

				// --- attention GQA causale ---
				if (ok) {
					zero();
					p[0] = (uint32)(nH * T * T);
					p[1] = (uint32)T;
					p[2] = (uint32)nH;
					p[3] = (uint32)hd;
					p[4] = (uint32)group;
					p[5] = (uint32)qd;
					p[6] = (uint32)kvd;
					p[7] = scale;
					ok = gpu.RunOp3("lg_scores", SrcScores(), mBufQr, mBufKr, mBufScores, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(nH * T);
					p[1] = (uint32)T;
					ok = gpu.RunConvOp("lg_softmax", SrcSoftmax(), mBufScores, mBufProbs, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * qd);
					p[1] = (uint32)T;
					p[2] = (uint32)nH;
					p[3] = (uint32)hd;
					p[4] = (uint32)group;
					p[5] = (uint32)qd;
					p[6] = (uint32)kvd;
					ok = gpu.RunOp3("lg_attnout", SrcAttnOut(), mBufProbs, mBufVb, mBufCtx, p, p[0]);
				}

				// --- projection de sortie + résiduel ---
				ok = ok && Matmul(L.wo, mBufCtx, mBufAttn, T, err);
				ok = ok && NkLoraGpuForward(A.o, mBufCtx, mBufAttn, T, mBufU, err);
				if (ok) {
					zero();
					p[0] = (uint32)(T * d);
					ok = gpu.RunOp3("lg_add", SrcAdd(), xBuf, mBufAttn, mBufX1, p, p[0]);
				}

				// --- MLP SwiGLU (pré-norme) + résiduel ---
				if (ok) {
					zero();
					p[0] = (uint32)T;
					p[1] = (uint32)d;
					p[2] = eps;
					ok = gpu.RunOp3("lg_rmsnorm", SrcRmsNorm(), mBufX1, L.ffnNorm, mBufXn2, p, (uint32)T);
				}
				ok = ok && Matmul(L.wGate, mBufXn2, mBufGate, T, err);
				ok = ok && Matmul(L.wUp, mBufXn2, mBufUp, T, err);
				ok = ok && NkLoraGpuForward(A.gate, mBufXn2, mBufGate, T, mBufU, err);
				ok = ok && NkLoraGpuForward(A.up, mBufXn2, mBufUp, T, mBufU, err);
				if (ok) {
					zero();
					p[0] = (uint32)(T * ffn);
					ok = gpu.RunOp3("lg_swiglu", SrcSwiGlu(), mBufGate, mBufUp, mBufAct, p, p[0]);
				}
				ok = ok && Matmul(L.wDown, mBufAct, mBufDown, T, err);
				ok = ok && NkLoraGpuForward(A.down, mBufAct, mBufDown, T, mBufU, err);
				if (ok && outBuf != 0) {
					zero();
					p[0] = (uint32)(T * d);
					ok = gpu.RunOp3("lg_add", SrcAdd(), mBufX1, mBufDown, outBuf, p, p[0]);
				}
				if (!ok && err && err->Size() == 0)
					SetErr(err, "LayerForward : un dispatch de noyau a échoué");
				return ok;
			}

			// =============================================================
			// Backward d'UNE couche. `xBuf` = son entrée (checkpoint) ; le
			// gradient de sortie est dans mBufDX et y est REMPLACÉ par le
			// gradient d'entrée.
			// =============================================================
			bool NkQwen2LoraGpu::LayerBackward(uint32 l, uint64 xBuf, int64 T, NkString *err) {
				NkTensorGpu &gpu = NkTensorGpu::Get();
				const Layer &L = mLayers[l];
				const NkLoraGpuSet &A = mLora[l];
				const int64 d = mCfg.dModel;
				const int64 hd = mCfg.headDim;
				const int64 nH = mCfg.nHeads;
				const int64 nKV = mCfg.nKVHeads;
				const int64 qd = nH * hd;
				const int64 kvd = nKV * hd;
				const int64 ffn = mCfg.ffnDim;
				const int64 half = hd / 2;
				const int64 group = nH / nKV;
				const uint32 eps = FBits(mCfg.rmsEps);
				const uint32 scale = FBits((float32)(1.0 / std::sqrt((double)hd)));
				uint32 p[12];
				auto zero = [&p]() {
					for (int i = 0; i < 12; ++i)
						p[i] = 0;
				};

				// REJEU du forward de la couche : le « checkpointing absorbé ». Il
				// remplit Xn1/Qr/Kr/Vb/Probs/Ctx/X1/Xn2/Gate/Up/Act — les activations
				// dont le backward a besoin et qu'on n'a PAS gardées.
				if (!LayerForward(l, xBuf, 0, T, err))
					return false;

				bool ok = true;
				// ---- MLP ----
				ok = ok && MatmulT(L.wDown, mBufDX, mBufDAct, T, false, err);
				ok = ok && NkLoraGpuBackward(A.down, mBufAct, mBufDX, T, mBufU, mBufDU, mBufDAct, err);
				if (ok) {
					zero();
					p[0] = (uint32)(T * ffn);
					ok = gpu.RunOp3("lg_mul", SrcMul(), mBufDAct, mBufUp, mBufDTmpF, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * ffn);
					ok = gpu.RunOp3("lg_dsilu", SrcDSiluMul(), mBufDTmpF, mBufGate, mBufDGate, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * ffn);
					ok = gpu.RunOp3("lg_silumul", SrcSiluMul(), mBufDAct, mBufGate, mBufDUp, p, p[0]);
				}
				ok = ok && MatmulT(L.wGate, mBufDGate, mBufDXn, T, false, err);
				ok = ok && MatmulT(L.wUp, mBufDUp, mBufDXn, T, true, err);
				ok = ok && NkLoraGpuBackward(A.gate, mBufXn2, mBufDGate, T, mBufU, mBufDU, mBufDXn, err);
				ok = ok && NkLoraGpuBackward(A.up, mBufXn2, mBufDUp, T, mBufU, mBufDU, mBufDXn, err);
				if (ok) {
					zero();
					p[0] = (uint32)(T * d);
					p[1] = (uint32)d;
					ok = gpu.RunOp3("lg_mulrow", SrcMulRow(), mBufDXn, L.ffnNorm, mBufDNrm, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)T;
					p[1] = (uint32)d;
					p[2] = eps;
					ok = gpu.RunOp3("lg_rmsnorm_bwd", SrcRmsNormBwd(), mBufX1, mBufDNrm, mBufDTmp, p, (uint32)T);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * d);
					ok = gpu.RunOp3("lg_add", SrcAdd(), mBufDX, mBufDTmp, mBufDX1, p, p[0]);
				}

				// ---- attention ----
				ok = ok && MatmulT(L.wo, mBufDX1, mBufDCtx, T, false, err);
				ok = ok && NkLoraGpuBackward(A.o, mBufCtx, mBufDX1, T, mBufU, mBufDU, mBufDCtx, err);
				if (ok) {
					zero();
					p[0] = (uint32)(T * kvd);
					p[1] = (uint32)T;
					p[2] = (uint32)nKV;
					p[3] = (uint32)hd;
					p[4] = (uint32)group;
					p[5] = (uint32)qd;
					p[6] = (uint32)kvd;
					ok = gpu.RunOp3("lg_dv", SrcDV(), mBufProbs, mBufDCtx, mBufDV, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(nH * T * T);
					p[1] = (uint32)T;
					p[2] = (uint32)nH;
					p[3] = (uint32)hd;
					p[4] = (uint32)group;
					p[5] = (uint32)qd;
					p[6] = (uint32)kvd;
					ok = gpu.RunOp3("lg_dp", SrcDP(), mBufDCtx, mBufVb, mBufDProbs, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(nH * T);
					p[1] = (uint32)T;
					p[2] = scale;
					ok = gpu.RunOp3("lg_softmax_bwd", SrcSoftmaxBwd(), mBufProbs, mBufDProbs, mBufDScores, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * qd);
					p[1] = (uint32)T;
					p[2] = (uint32)nH;
					p[3] = (uint32)hd;
					p[4] = (uint32)group;
					p[5] = (uint32)qd;
					p[6] = (uint32)kvd;
					ok = gpu.RunOp3("lg_dq", SrcDQ(), mBufDScores, mBufKr, mBufDQr, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * kvd);
					p[1] = (uint32)T;
					p[2] = (uint32)nKV;
					p[3] = (uint32)hd;
					p[4] = (uint32)group;
					p[5] = (uint32)qd;
					p[6] = (uint32)kvd;
					ok = gpu.RunOp3("lg_dk", SrcDK(), mBufDScores, mBufQr, mBufDKr, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * nH * half);
					p[1] = (uint32)nH;
					p[2] = (uint32)hd;
					p[3] = (uint32)half;
					p[4] = (uint32)mOpt.maxSeqLen;
					ok = gpu.RunOp3("lg_rope_bwd", SrcRopeBwd(), mBufDQr, mBufRope, mBufDQ, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * nKV * half);
					p[1] = (uint32)nKV;
					p[2] = (uint32)hd;
					p[3] = (uint32)half;
					p[4] = (uint32)mOpt.maxSeqLen;
					ok = gpu.RunOp3("lg_rope_bwd", SrcRopeBwd(), mBufDKr, mBufRope, mBufDK, p, p[0]);
				}
				// Les BIAIS sont GELÉS : le gradient les traverse sans être collecté
				// (dY est le même de part et d'autre d'une addition de constante).
				ok = ok && MatmulT(L.wq, mBufDQ, mBufDXn, T, false, err);
				ok = ok && MatmulT(L.wk, mBufDK, mBufDXn, T, true, err);
				ok = ok && MatmulT(L.wv, mBufDV, mBufDXn, T, true, err);
				ok = ok && NkLoraGpuBackward(A.q, mBufXn1, mBufDQ, T, mBufU, mBufDU, mBufDXn, err);
				ok = ok && NkLoraGpuBackward(A.k, mBufXn1, mBufDK, T, mBufU, mBufDU, mBufDXn, err);
				ok = ok && NkLoraGpuBackward(A.v, mBufXn1, mBufDV, T, mBufU, mBufDU, mBufDXn, err);
				if (ok) {
					zero();
					p[0] = (uint32)(T * d);
					p[1] = (uint32)d;
					ok = gpu.RunOp3("lg_mulrow", SrcMulRow(), mBufDXn, L.attnNorm, mBufDNrm, p, p[0]);
				}
				if (ok) {
					zero();
					p[0] = (uint32)T;
					p[1] = (uint32)d;
					p[2] = eps;
					ok = gpu.RunOp3("lg_rmsnorm_bwd", SrcRmsNormBwd(), xBuf, mBufDNrm, mBufDTmp, p, (uint32)T);
				}
				if (ok) {
					zero();
					p[0] = (uint32)(T * d);
					ok = gpu.RunOp3("lg_add", SrcAdd(), mBufDX1, mBufDTmp, mBufDX, p, p[0]);
				}
				if (!ok && err && err->Size() == 0)
					SetErr(err, "LayerBackward : un dispatch de noyau a échoué");
				return ok;
			}

			// =============================================================
			// Forward complet
			// =============================================================
			bool NkQwen2LoraGpu::Forward(const int32 *ids, int32 T, NkString *err) {
				if (!mLoaded) {
					SetErr(err, "Forward : modèle non chargé");
					return false;
				}
				if (T <= 0 || T > mOpt.maxSeqLen) {
					SetErr(err, "Forward : T doit être dans [1, maxSeqLen]");
					return false;
				}
				NkTensorGpu &gpu = NkTensorGpu::Get();
				const int64 d = mCfg.dModel;

				NkTensor emb;
				if (!EmbedTokens(ids, T, emb, err))
					return false;
				if (!gpu.Upload(mCkpt[0], emb.DataAs<float>(), (nk_size)((int64)T * d * (int64)sizeof(float32)))) {
					SetErr(err, "Forward : upload des embeddings a échoué");
					return false;
				}
				for (uint32 l = 0; l < mLayers.Size(); ++l) {
					if (!LayerForward(l, mCkpt[l], mCkpt[l + 1], T, err))
						return false;
				}
				uint32 p[12] = {0};
				p[0] = (uint32)T;
				p[1] = (uint32)d;
				p[2] = FBits(mCfg.rmsEps);
				if (!gpu.RunOp3("lg_rmsnorm", SrcRmsNorm(), mCkpt[mLayers.Size()], mOutputNorm, mBufXn1, p,
								(uint32)T)) {
					SetErr(err, "Forward : RMSNorm finale a échoué");
					return false;
				}
				return Matmul(mLmHead, mBufXn1, mBufLogits, T, err);
			}

			bool NkQwen2LoraGpu::Loss(const NkVector<int32> &targets, const NkVector<float32> &mask, bool wantGrad,
									  float64 &outLoss, int64 &outActive, NkString *err) {
				const int32 T = (int32)targets.Size();
				if (T <= 0 || targets.Size() != mask.Size() || T > mOpt.maxSeqLen) {
					SetErr(err, "Loss : cibles/masque incohérents");
					return false;
				}
				int64 active = 0;
				for (uint32 t = 0; t < mask.Size(); ++t)
					if (mask[t] != 0.0f)
						++active;
				outActive = active;
				if (active == 0) {
					outLoss = 0.0;
					return true;
				}
				NkTensorGpu &gpu = NkTensorGpu::Get();
				NkVector<float32> meta;
				meta.Resize((NkVector<float32>::SizeType)(2 * T));
				for (int32 t = 0; t < T; ++t) {
					meta[(NkVector<float32>::SizeType)t] = (float32)targets[(uint32)t];
					meta[(NkVector<float32>::SizeType)(T + t)] = mask[(uint32)t];
				}
				if (!gpu.Upload(mBufMeta, meta.Data(), (nk_size)(2 * (int64)T * (int64)sizeof(float32)))) {
					SetErr(err, "Loss : upload des cibles/masque a échoué");
					return false;
				}
				const float32 invActive = 1.0f / (float32)active;
				uint32 p[12] = {0};
				p[0] = (uint32)T;
				p[1] = (uint32)mVocabSize;
				p[2] = 0; // mode 0 : perte par ligne
				p[3] = FBits(invActive);
				if (!gpu.RunOp3("lg_ce", SrcCE(), mBufLogits, mBufMeta, mBufLoss, p, (uint32)T)) {
					SetErr(err, "Loss : dispatch de la cross-entropie a échoué");
					return false;
				}
				NkVector<float32> rows;
				rows.Resize((NkVector<float32>::SizeType)T);
				if (!gpu.Download(mBufLoss, rows.Data(), (nk_size)((int64)T * (int64)sizeof(float32)))) {
					SetErr(err, "Loss : relecture des pertes par ligne a échoué");
					return false;
				}
				// Somme en float64 côté CPU : la perte sert de référence à la
				// vérification par différences finies, son bruit doit rester sous
				// celui du forward.
				float64 sum = 0.0;
				for (int32 t = 0; t < T; ++t)
					sum += (float64)rows[(uint32)t];
				outLoss = sum / (float64)active;

				if (wantGrad) {
					p[2] = 1; // mode 1 : dLogits EN PLACE dans le tampon de logits
					if (!gpu.RunOp3("lg_ce", SrcCE(), mBufLogits, mBufMeta, mBufLogits, p, (uint32)T)) {
						SetErr(err, "Loss : dispatch du gradient de cross-entropie a échoué");
						return false;
					}
				}
				return true;
			}

			bool NkQwen2LoraGpu::Backward(int32 T, NkString *err) {
				if (!mLoaded || T <= 0 || T > mOpt.maxSeqLen) {
					SetErr(err, "Backward : état ou T invalide");
					return false;
				}
				NkTensorGpu &gpu = NkTensorGpu::Get();
				const int64 d = mCfg.dModel;
				// lm_head GELÉ : dXn = dLogits·W (aucun dW).
				if (!MatmulT(mLmHead, mBufLogits, mBufDXn, T, false, err))
					return false;
				uint32 p[12] = {0};
				p[0] = (uint32)((int64)T * d);
				p[1] = (uint32)d;
				if (!gpu.RunOp3("lg_mulrow", SrcMulRow(), mBufDXn, mOutputNorm, mBufDNrm, p, p[0])) {
					SetErr(err, "Backward : produit par la norme finale a échoué");
					return false;
				}
				for (int i = 0; i < 12; ++i)
					p[i] = 0;
				p[0] = (uint32)T;
				p[1] = (uint32)d;
				p[2] = FBits(mCfg.rmsEps);
				if (!gpu.RunOp3("lg_rmsnorm_bwd", SrcRmsNormBwd(), mCkpt[mLayers.Size()], mBufDNrm, mBufDX, p,
								(uint32)T)) {
					SetErr(err, "Backward : RMSNorm finale (backward) a échoué");
					return false;
				}
				for (int32 l = (int32)mLayers.Size() - 1; l >= 0; --l) {
					if (!LayerBackward((uint32)l, mCkpt[l], T, err))
						return false;
				}
				// Le dX sous la couche 0 est ABANDONNÉ : l'embedding est gelé, il n'y
				// a rien à apprendre en dessous.
				return true;
			}

			bool NkQwen2LoraGpu::ZeroAllGrads(NkString *err) {
				for (uint32 l = 0; l < mLora.Size(); ++l)
					for (int32 i = 0; i < NkLoraGpuSet::kCount; ++i)
						if (!NkLoraGpuZeroGrads(*mLora[l].At(i), err))
							return false;
				return true;
			}

			bool NkQwen2LoraGpu::AdamStep(float32 lr, int64 stepIndex, NkString *err) {
				const float32 b1 = 0.9f, b2 = 0.999f, eps = 1e-8f;
				const float32 b1t = 1.0f - (float32)std::pow((double)b1, (double)stepIndex);
				const float32 b2t = 1.0f - (float32)std::pow((double)b2, (double)stepIndex);
				for (uint32 l = 0; l < mLora.Size(); ++l)
					for (int32 i = 0; i < NkLoraGpuSet::kCount; ++i)
						if (!NkLoraGpuAdamStep(*mLora[l].At(i), lr, b1, b2, eps, b1t, b2t, err))
							return false;
				return true;
			}

			bool NkQwen2LoraGpu::GradGlobalNorm(float64 &outNorm, NkString *err) {
				NkTensorGpu &gpu = NkTensorGpu::Get();
				NkVector<float32> part;
				float64 total = 0.0;
				for (uint32 l = 0; l < mLora.Size(); ++l) {
					for (int32 i = 0; i < NkLoraGpuSet::kCount; ++i) {
						const NkLoraGpuPair &pr = *mLora[l].At(i);
						if (!pr.HasGrads())
							continue;
						const uint64 counts[2] = {(uint64)pr.NumelA(), (uint64)pr.NumelB()};
						const uint64 bufs[2] = {pr.dA, pr.dB};
						for (int s = 0; s < 2; ++s) {
							const uint64 chunks = (counts[s] + kNkGpuSumSquaresChunk - 1ull) / kNkGpuSumSquaresChunk;
							if (!NkGpuSumSquares(bufs[s], mBufNormPart, counts[s], err))
								return false;
							part.Resize((NkVector<float32>::SizeType)chunks);
							if (!gpu.Download(mBufNormPart, part.Data(), (nk_size)(chunks * sizeof(float32)))) {
								SetErr(err, "GradGlobalNorm : relecture des partiels a échoué");
								return false;
							}
							for (uint64 c = 0; c < chunks; ++c)
								total += (float64)part[(NkVector<float32>::SizeType)c];
						}
					}
				}
				outNorm = std::sqrt(total);
				return true;
			}

			bool NkQwen2LoraGpu::DownloadLogitsRow(int32 row, int32 T, NkVector<float32> &out, NkString *err) {
				if (row < 0 || row >= T) {
					SetErr(err, "DownloadLogitsRow : ligne hors séquence");
					return false;
				}
				NkTensorGpu &gpu = NkTensorGpu::Get();
				uint32 p[12] = {0};
				p[0] = (uint32)mVocabSize;
				p[1] = (uint32)((int64)row * mVocabSize);
				if (!gpu.RunConvOp("lg_copyrow", SrcCopyRow(), mBufLogits, mBufRow, p, (uint32)mVocabSize)) {
					SetErr(err, "DownloadLogitsRow : extraction de la ligne a échoué");
					return false;
				}
				out.Resize((NkVector<float32>::SizeType)mVocabSize);
				if (!gpu.Download(mBufRow, out.Data(), (nk_size)(mVocabSize * (int64)sizeof(float32)))) {
					SetErr(err, "DownloadLogitsRow : relecture a échoué");
					return false;
				}
				return true;
			}

			bool NkQwen2LoraGpu::Generate(const NkVector<int32> &promptIds, int32 nNew, int32 stopId,
										  NkVector<int32> &outIds, float64 *outSeconds, NkString *err) {
				outIds.Clear();
				if (!mLoaded || promptIds.Size() == 0) {
					SetErr(err, "Generate : modèle non chargé ou prompt vide");
					return false;
				}
				const float64 t0 = NowSeconds();
				NkVector<int32> seq = promptIds;
				NkVector<float32> logits;
				for (int32 s = 0; s < nNew; ++s) {
					if ((int32)seq.Size() > mOpt.maxSeqLen)
						break; // borne franche : on ne tronque pas en silence
					if (!Forward(seq.Data(), (int32)seq.Size(), err))
						return false;
					if (!DownloadLogitsRow((int32)seq.Size() - 1, (int32)seq.Size(), logits, err))
						return false;
					int64 best = 0;
					for (int64 i = 1; i < (int64)logits.Size(); ++i)
						if (logits[(uint32)i] > logits[(uint32)best])
							best = i;
					const int32 next = (int32)best;
					outIds.PushBack(next);
					if (next == stopId)
						break;
					seq.PushBack(next);
				}
				if (outSeconds)
					*outSeconds = NowSeconds() - t0;
				return true;
			}

			// =====================================================================
			// Trainer
			// =====================================================================
			bool NkQwen2LoraGpuTrainer::Init(NkQwen2LoraGpu *model, float32 lr, NkString *err) {
				if (!model || !model->IsLoaded()) {
					SetErr(err, "Trainer::Init : modèle non chargé");
					return false;
				}
				mModel = model;
				mLr = lr;
				mT = 0;
				return true;
			}

			bool NkQwen2LoraGpuTrainer::Split(const NkQwen2SftExample &ex, NkVector<int32> &inputs,
											  NkVector<int32> &targets, NkVector<float32> &mask) const {
				const uint32 n = ex.tokens.Size();
				if (n < 2 || ex.lossMask.Size() != n)
					return false;
				inputs.Clear();
				targets.Clear();
				mask.Clear();
				bool anyActive = false;
				for (uint32 t = 0; t + 1 < n; ++t) {
					inputs.PushBack(ex.tokens[t]);
					targets.PushBack(ex.tokens[t + 1]);
					mask.PushBack(ex.lossMask[t + 1]);
					if (ex.lossMask[t + 1] != 0.0f)
						anyActive = true;
				}
				return anyActive;
			}

			double NkQwen2LoraGpuTrainer::TrainExample(const NkQwen2SftExample &ex, NkString *err) {
				if (!mModel) {
					SetErr(err, "TrainExample : trainer non initialisé");
					return -1.0;
				}
				NkVector<int32> inputs, targets;
				NkVector<float32> mask;
				if (!Split(ex, inputs, targets, mask)) {
					SetErr(err, "TrainExample : exemple trop court ou sans cible active");
					return -1.0;
				}
				if ((int32)inputs.Size() > mModel->MaxSeqLen()) {
					SetErr(err, "TrainExample : séquence plus longue que maxSeqLen");
					return -1.0;
				}
				if (!mModel->ZeroAllGrads(err))
					return -1.0;
				if (!mModel->Forward(inputs.Data(), (int32)inputs.Size(), err))
					return -1.0;
				float64 loss = 0.0;
				int64 active = 0;
				if (!mModel->Loss(targets, mask, true, loss, active, err))
					return -1.0;
				if (!mModel->Backward((int32)inputs.Size(), err))
					return -1.0;
				++mT;
				if (!mModel->AdamStep(mLr, mT, err))
					return -1.0;
				return loss;
			}

			double NkQwen2LoraGpuTrainer::EvaluateExample(const NkQwen2SftExample &ex, NkString *err) {
				if (!mModel) {
					SetErr(err, "EvaluateExample : trainer non initialisé");
					return -1.0;
				}
				NkVector<int32> inputs, targets;
				NkVector<float32> mask;
				if (!Split(ex, inputs, targets, mask)) {
					SetErr(err, "EvaluateExample : exemple trop court ou sans cible active");
					return -1.0;
				}
				if ((int32)inputs.Size() > mModel->MaxSeqLen()) {
					SetErr(err, "EvaluateExample : séquence plus longue que maxSeqLen");
					return -1.0;
				}
				if (!mModel->Forward(inputs.Data(), (int32)inputs.Size(), err))
					return -1.0;
				float64 loss = 0.0;
				int64 active = 0;
				if (!mModel->Loss(targets, mask, false, loss, active, err))
					return -1.0;
				return loss;
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
