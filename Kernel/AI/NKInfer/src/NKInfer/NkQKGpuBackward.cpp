// =============================================================================
// NkQKGpuBackward.cpp — noyaux NkSL du backward quantifié. Voir le .h pour le
// contrat et le raisonnement sur la tuile.
//
// LES FRAGMENTS DE DÉCODAGE DE BLOC SONT RECOPIÉS de NkQ4KGpu.cpp / NkQ6KGpu.cpp
// (ils y vivent dans un namespace anonyme, donc non partageables sans TOUCHER à
// ces fichiers — ce que ce jalon s'interdit, méthode additive). La recopie est
// LITTÉRALE, et le risque de divergence est couvert par une mesure, pas par une
// promesse : NKQwen2SftGpuTest compare `NkQ4KGpuMatmulT` / `NkQ6KGpuMatmulT` à
// un produit CPU calculé depuis `NkGGUFDequantizeRaw` sur un VRAI tenseur du
// blob. Si les deux transcriptions divergeaient d'un bit, ce test le dirait.
// =============================================================================
#include "NKInfer/NkQKGpuBackward.h"
#include "NKInfer/NkQKGpuShaderCommon.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKTensor/NkTensorGpu.h"

#include <cstring>

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {

				void SetErr(NkString *e, const char *msg) {
					if (e)
						*e = NkString(msg);
				}

				uint32 FBits(float32 f) {
					uint32 u = 0;
					std::memcpy(&u, &f, sizeof(u));
					return u;
				}

				// ---- Recopie littérale des helpers Q4_K (NkQ4KGpu.cpp) ----------
				const char *kQ4KHelpers = R"NKSL(
uint nkq4kByte(uint base, uint off) {
    uint w = W.data[base + (off >> 2u)];
    return (w >> ((off & 0x3u) * 8u)) & 0xFFu;
}

uvec2 nkq4kScaleMin(uint base, uint j) {
    uint sd = 0u;
    uint sm = 0u;
    if (j < 4u) {
        sd = nkq4kByte(base, 4u + j) & 0x3Fu;
        sm = nkq4kByte(base, 8u + j) & 0x3Fu;
    } else {
        sd = (nkq4kByte(base, 8u + j) & 0xFu) | ((nkq4kByte(base, j) >> 6u) << 4u);
        sm = (nkq4kByte(base, 8u + j) >> 4u) | ((nkq4kByte(base, 4u + j) >> 6u) << 4u);
    }
    return uvec2(sd, sm);
}
)NKSL";

				// ---- Recopie littérale des helpers Q6_K (NkQ6KGpu.cpp) ----------
				const char *kQ6KHelpers = R"NKSL(
uint nkq6kByte(uint off) {
    uint w = W.data[off >> 2u];
    return (w >> ((off & 0x3u) * 8u)) & 0xFFu;
}

int nkq6kScale(uint off) {
    int v = int(nkq6kByte(off));
    if (v > 0x7F) {
        v = v - 0x100;
    }
    return v;
}

float nkq6kD(uint base) {
    uint lo = nkq6kByte(base + 208u);
    uint hi = nkq6kByte(base + 209u);
    return nkqkF16(lo | (hi << 8u));
}
)NKSL";

				// -------------------------------------------------------------
				// En-tête commun aux deux noyaux transposés (bindings de RunOp3).
				// `kTiles` = K/32 : le nombre de tuiles de k. `accum` décide si dX
				// est écrasé ou accumulé.
				// -------------------------------------------------------------
				const char *kMatmulTHead = R"NKSL(
@binding(set=0, binding=0) buffer BufW { uint data[]; } W;
@binding(set=0, binding=1) buffer BufDY { float data[]; } DY;
@binding(set=0, binding=2) buffer BufDX { float data[]; } DX;
@binding(set=0, binding=3) uniform Params {
    uint M; uint N; uint K; uint bpr;
    uint kTiles; uint accum; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
)NKSL";

				// -------------------------------------------------------------
				// Corps Q4_K.
				//
				// LE BOURRAGE À 33 ET NON 32 : la mémoire partagée a 32 bancs de 4
				// octets. Au calcul, les 8 threads d'un même `tk` lisent
				// nkqbTile[j*33 + tk] pour j = 0..7 successifs ; avec un pas de 32,
				// les 8 lignes tomberaient toutes dans le MÊME banc (32 % 32 == 0)
				// et seraient sérialisées. Avec 33, chaque ligne décale d'un banc.
				// Coût : 32 octets.
				//
				// ORDRE DE SOMMATION : n0 croissant, puis j croissant dans la tuile
				// -> n strictement croissant, le même ordre qu'une boucle CPU de
				// référence. C'est ce qui rend la comparaison du test signifiante.
				// -------------------------------------------------------------
				const char *kQ4KMatmulTBody = R"NKSL(
shared float nkqbTile[264];

@stage(compute)
@entry
void main() {
    uint g = gl_WorkGroupID.x;
    uint t = gl_LocalInvocationID.x;
    uint kt = g % pc.kTiles;
    uint mt = g / pc.kTiles;
    uint nl = t >> 3u;
    uint tk = t & 0x7u;
    uint m = mt * 8u + nl;
    uint bb = kt >> 3u;
    uint sb = kt & 0x7u;
    uint k0 = kt * 32u;
    uint dst = nl * 33u;
    uint qoff = 16u + (sb >> 1u) * 32u;
    uint hi = sb & 0x1u;

    float acc0 = 0.0;
    float acc1 = 0.0;
    float acc2 = 0.0;
    float acc3 = 0.0;

    for (uint n0 = 0u; n0 < pc.N; n0 = n0 + 8u) {
        barrier();
        uint base = ((n0 + nl) * pc.bpr + bb) * 36u;
        float d = nkqkF16(W.data[base] & 0xFFFFu);
        float dmin = nkqkF16(W.data[base] >> 16u);
        uvec2 sm = nkq4kScaleMin(base, sb);
        float ds = d * float(sm.x);
        float ms = dmin * float(sm.y);
        for (uint i = 0u; i < 4u; i = i + 1u) {
            uint l = tk + i * 8u;
            uint qb = nkq4kByte(base, qoff + l);
            uint q = qb & 0xFu;
            if (hi == 1u) {
                q = qb >> 4u;
            }
            nkqbTile[dst + l] = ds * float(q) - ms;
        }
        barrier();
        if (m < pc.M) {
            uint db = m * pc.N + n0;
            for (uint j = 0u; j < 8u; j = j + 1u) {
                float dyv = DY.data[db + j];
                uint wb = j * 33u + tk;
                acc0 = acc0 + dyv * nkqbTile[wb];
                acc1 = acc1 + dyv * nkqbTile[wb + 8u];
                acc2 = acc2 + dyv * nkqbTile[wb + 16u];
                acc3 = acc3 + dyv * nkqbTile[wb + 24u];
            }
        }
    }
    if (m < pc.M) {
        uint o = m * pc.K + k0 + tk;
        if (pc.accum == 1u) {
            DX.data[o] = DX.data[o] + acc0;
            DX.data[o + 8u] = DX.data[o + 8u] + acc1;
            DX.data[o + 16u] = DX.data[o + 16u] + acc2;
            DX.data[o + 24u] = DX.data[o + 24u] + acc3;
        } else {
            DX.data[o] = acc0;
            DX.data[o + 8u] = acc1;
            DX.data[o + 16u] = acc2;
            DX.data[o + 24u] = acc3;
        }
    }
}
)NKSL";

				// -------------------------------------------------------------
				// Corps Q6_K — même squelette, seul le décodage change.
				//
				// CORRESPONDANCE k <-> (h, j, l), lue dans le noyau de
				// déquantification du jalon 4 : l'élément écrit à
				// b*256 + h*128 + j*32 + l. Donc pour une tuile de 32 valeurs de k
				// alignée sur 32, l'index de tuile `sub` (0..7) dans le super-bloc
				// donne h = sub/4 et j = sub%4, et l parcourt 0..31. Un seul jeu de
				// métadonnées est touché : c'est exactement pourquoi la tuile fait
				// 32 et pas autre chose.
				// -------------------------------------------------------------
				const char *kQ6KMatmulTBody = R"NKSL(
shared float nkqbTile[264];

@stage(compute)
@entry
void main() {
    uint g = gl_WorkGroupID.x;
    uint t = gl_LocalInvocationID.x;
    uint kt = g % pc.kTiles;
    uint mt = g / pc.kTiles;
    uint nl = t >> 3u;
    uint tk = t & 0x7u;
    uint m = mt * 8u + nl;
    uint bb = kt >> 3u;
    uint sub = kt & 0x7u;
    uint h = sub >> 2u;
    uint jq = sub & 0x3u;
    uint lo = (jq & 0x1u) * 32u;
    uint hiNib = jq >> 1u;
    uint shq = jq * 2u;
    uint k0 = kt * 32u;
    uint dst = nl * 33u;

    float acc0 = 0.0;
    float acc1 = 0.0;
    float acc2 = 0.0;
    float acc3 = 0.0;

    for (uint n0 = 0u; n0 < pc.N; n0 = n0 + 8u) {
        barrier();
        uint base = ((n0 + nl) * pc.bpr + bb) * 210u;
        float d = nkq6kD(base);
        uint qlB = base + h * 64u;
        uint qhB = base + 128u + h * 32u;
        uint scB = base + 192u + h * 8u;
        for (uint i = 0u; i < 4u; i = i + 1u) {
            uint l = tk + i * 8u;
            float ds = d * float(nkq6kScale(scB + (l >> 4u) + jq * 2u));
            uint qlv = nkq6kByte(qlB + lo + l);
            uint nib = qlv & 0xFu;
            if (hiNib == 1u) {
                nib = qlv >> 4u;
            }
            uint qhv = nkq6kByte(qhB + l);
            int q = int(nib | (((qhv >> shq) & 0x3u) << 4u)) - 32;
            nkqbTile[dst + l] = ds * float(q);
        }
        barrier();
        if (m < pc.M) {
            uint db = m * pc.N + n0;
            for (uint j = 0u; j < 8u; j = j + 1u) {
                float dyv = DY.data[db + j];
                uint wb = j * 33u + tk;
                acc0 = acc0 + dyv * nkqbTile[wb];
                acc1 = acc1 + dyv * nkqbTile[wb + 8u];
                acc2 = acc2 + dyv * nkqbTile[wb + 16u];
                acc3 = acc3 + dyv * nkqbTile[wb + 24u];
            }
        }
    }
    if (m < pc.M) {
        uint o = m * pc.K + k0 + tk;
        if (pc.accum == 1u) {
            DX.data[o] = DX.data[o] + acc0;
            DX.data[o + 8u] = DX.data[o + 8u] + acc1;
            DX.data[o + 16u] = DX.data[o + 16u] + acc2;
            DX.data[o + 24u] = DX.data[o + 24u] + acc3;
        } else {
            DX.data[o] = acc0;
            DX.data[o + 8u] = acc1;
            DX.data[o + 16u] = acc2;
            DX.data[o + 24u] = acc3;
        }
    }
}
)NKSL";

				// ---- Produits f32 ------------------------------------------------
				const char *kF32MatmulHead = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufY { float data[]; } Y;
@binding(set=0, binding=3) uniform Params {
    uint M; uint N; uint K; uint total;
    uint accum; uint alphaBits; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
)NKSL";

				const char *kMatmulABtBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint n = idx % pc.N;
        uint m = idx / pc.N;
        uint ab = m * pc.K;
        uint bbase = n * pc.K;
        float acc = 0.0;
        for (uint k = 0u; k < pc.K; k = k + 1u) {
            acc = acc + A.data[ab + k] * B.data[bbase + k];
        }
        acc = acc * uintBitsToFloat(pc.alphaBits);
        if (pc.accum == 1u) {
            acc = acc + Y.data[idx];
        }
        Y.data[idx] = acc;
    }
}
)NKSL";

				const char *kMatmulABBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint n = idx % pc.N;
        uint m = idx / pc.N;
        uint ab = m * pc.K;
        float acc = 0.0;
        for (uint k = 0u; k < pc.K; k = k + 1u) {
            acc = acc + A.data[ab + k] * B.data[k * pc.N + n];
        }
        acc = acc * uintBitsToFloat(pc.alphaBits);
        if (pc.accum == 1u) {
            acc = acc + Y.data[idx];
        }
        Y.data[idx] = acc;
    }
}
)NKSL";

				const char *kMatmulAtBBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint n = idx % pc.N;
        uint m = idx / pc.N;
        float acc = 0.0;
        for (uint k = 0u; k < pc.K; k = k + 1u) {
            acc = acc + A.data[k * pc.M + m] * B.data[k * pc.N + n];
        }
        acc = acc * uintBitsToFloat(pc.alphaBits);
        if (pc.accum == 1u) {
            acc = acc + Y.data[idx];
        }
        Y.data[idx] = acc;
    }
}
)NKSL";

				// ---- Utilitaires (RunConvOp : 2 tampons + UBO au binding 2) ------
				const char *kAxpy = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufY { float data[]; } Y;
@binding(set=0, binding=2) uniform Params {
    uint count; uint alphaBits; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        Y.data[i] = Y.data[i] + uintBitsToFloat(pc.alphaBits) * X.data[i];
    }
}
)NKSL";

				// Somme des carrés par tranches de 1024 : un thread par tranche.
				const char *kSumSquares = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufP { float data[]; } P;
@binding(set=0, binding=2) uniform Params {
    uint chunks; uint count; uint chunk; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint c = gl_GlobalInvocationID.x;
    if (c < pc.chunks) {
        uint b = c * pc.chunk;
        uint e = b + pc.chunk;
        if (e > pc.count) {
            e = pc.count;
        }
        float s = 0.0;
        for (uint i = b; i < e; i = i + 1u) {
            float v = X.data[i];
            s = s + v * v;
        }
        P.data[c] = s;
    }
}
)NKSL";

				// Sources assemblées UNE fois (statiques locales) : le pipeline est
				// déjà mis en cache par nom côté NkTensorGpu, mais le backward
				// appelle ces noyaux des milliers de fois par pas — autant ne pas
				// reconcaténer des kilo-octets de chaînes à chaque appel.
				const NkString &SrcQ4KMatmulT() {
					static NkString s = NkString(kMatmulTHead) + NkString(kNkQKGpuF16Helper) + NkString(kQ4KHelpers) +
										NkString(kQ4KMatmulTBody);
					return s;
				}
				const NkString &SrcQ6KMatmulT() {
					static NkString s = NkString(kMatmulTHead) + NkString(kNkQKGpuF16Helper) + NkString(kQ6KHelpers) +
										NkString(kQ6KMatmulTBody);
					return s;
				}
				const NkString &SrcABt() {
					static NkString s = NkString(kF32MatmulHead) + NkString(kMatmulABtBody);
					return s;
				}
				const NkString &SrcAB() {
					static NkString s = NkString(kF32MatmulHead) + NkString(kMatmulABBody);
					return s;
				}
				const NkString &SrcAtB() {
					static NkString s = NkString(kF32MatmulHead) + NkString(kMatmulAtBBody);
					return s;
				}
				const NkString &SrcAxpy() {
					static NkString s(kAxpy);
					return s;
				}
				const NkString &SrcSumSquares() {
					static NkString s(kSumSquares);
					return s;
				}

				// Facteur commun aux deux backwards quantifiés : contrôles, calcul
				// du nombre de groupes, remplissage de l'UBO.
				bool DispatchMatmulT(const char *name, const NkString &src, uint64 wBuf, int64 rows, int64 cols,
									 int64 blocksPerRow, uint64 dyBuffer, uint64 dxBuffer, int64 M, bool accumulate,
									 NkString *outError) {
					if (wBuf == 0 || dyBuffer == 0 || dxBuffer == 0 || M <= 0 || rows <= 0 || cols <= 0) {
						SetErr(outError, "MatmulT : arguments invalides");
						return false;
					}
					if ((cols % 256) != 0) {
						SetErr(outError, "MatmulT : K doit être un multiple de 256 (format K-quant)");
						return false;
					}
					// REFUS EXPLICITE plutôt qu'un arrondi : la tuile de n fait 8, et
					// un N non multiple de 8 ferait lire dY hors de ses bornes sur la
					// dernière tuile. Toutes les projections de Qwen2.5 satisfont la
					// condition ; un modèle qui ne la satisfait pas doit s'arrêter
					// ici, pas produire un gradient faux.
					if ((rows % 8) != 0) {
						SetErr(outError, "MatmulT : N doit être un multiple de 8 (tuile de n)");
						return false;
					}
					const uint64 kTiles = (uint64)cols / 32ull;
					const uint64 mTiles = ((uint64)M + 7ull) / 8ull;
					const uint64 groups = kTiles * mTiles;
					if (groups * 64ull > 0xFFFFFFFFull) {
						SetErr(outError, "MatmulT : trop de groupes de travail pour un dispatch 32 bits");
						return false;
					}
					uint32 p12[12] = {0};
					p12[0] = (uint32)M;
					p12[1] = (uint32)rows;
					p12[2] = (uint32)cols;
					p12[3] = (uint32)blocksPerRow;
					p12[4] = (uint32)kTiles;
					p12[5] = accumulate ? 1u : 0u;
					if (!NkTensorGpu::Get().RunOp3(name, src, wBuf, dyBuffer, dxBuffer, p12,
												   (uint32)(groups * 64ull))) {
						SetErr(outError, "MatmulT : dispatch du noyau a échoué");
						return false;
					}
					return true;
				}

				bool DispatchF32(const char *name, const NkString &src, uint64 a, uint64 b, uint64 y, int64 M, int64 N,
								 int64 K, float32 alpha, bool accumulate, NkString *outError) {
					if (a == 0 || b == 0 || y == 0 || M <= 0 || N <= 0 || K <= 0) {
						SetErr(outError, "matmul f32 : arguments invalides");
						return false;
					}
					const uint64 total = (uint64)M * (uint64)N;
					if (total > 0xFFFFFFFFull) {
						SetErr(outError, "matmul f32 : M·N dépasse un dispatch 32 bits");
						return false;
					}
					uint32 p12[12] = {0};
					p12[0] = (uint32)M;
					p12[1] = (uint32)N;
					p12[2] = (uint32)K;
					p12[3] = (uint32)total;
					p12[4] = accumulate ? 1u : 0u;
					p12[5] = FBits(alpha);
					if (!NkTensorGpu::Get().RunOp3(name, src, a, b, y, p12, (uint32)total)) {
						SetErr(outError, "matmul f32 : dispatch du noyau a échoué");
						return false;
					}
					return true;
				}

			} // namespace

			bool NkQ4KGpuMatmulT(const NkQ4KGpuWeight &w, uint64 dyBuffer, uint64 dxBuffer, int64 M, bool accumulate,
								 NkString *outError) {
				if (!w.IsValid()) {
					SetErr(outError, "NkQ4KGpuMatmulT : poids non résident");
					return false;
				}
				return DispatchMatmulT("q4k_matmul_t", SrcQ4KMatmulT(), w.buffer, w.rows, w.cols, w.blocksPerRow,
									   dyBuffer, dxBuffer, M, accumulate, outError);
			}

			bool NkQ6KGpuMatmulT(const NkQ6KGpuWeight &w, uint64 dyBuffer, uint64 dxBuffer, int64 M, bool accumulate,
								 NkString *outError) {
				if (!w.IsValid()) {
					SetErr(outError, "NkQ6KGpuMatmulT : poids non résident");
					return false;
				}
				return DispatchMatmulT("q6k_matmul_t", SrcQ6KMatmulT(), w.buffer, w.rows, w.cols, w.blocksPerRow,
									   dyBuffer, dxBuffer, M, accumulate, outError);
			}

			bool NkGpuMatmulABt(uint64 a, uint64 b, uint64 y, int64 M, int64 N, int64 K, float32 alpha,
								bool accumulate, NkString *outError) {
				return DispatchF32("nk_mm_abt", SrcABt(), a, b, y, M, N, K, alpha, accumulate, outError);
			}

			bool NkGpuMatmulAB(uint64 a, uint64 b, uint64 y, int64 M, int64 N, int64 K, float32 alpha, bool accumulate,
							   NkString *outError) {
				return DispatchF32("nk_mm_ab", SrcAB(), a, b, y, M, N, K, alpha, accumulate, outError);
			}

			bool NkGpuMatmulAtB(uint64 a, uint64 b, uint64 y, int64 M, int64 N, int64 K, float32 alpha, bool accumulate,
								NkString *outError) {
				return DispatchF32("nk_mm_atb", SrcAtB(), a, b, y, M, N, K, alpha, accumulate, outError);
			}

			bool NkGpuZeroBuffer(uint64 dst, uint64 bytes, NkString *outError) {
				if (dst == 0 || bytes == 0) {
					SetErr(outError, "NkGpuZeroBuffer : tampon nul");
					return false;
				}
				// UN SEUL Upload, donc un tampon CPU de la taille demandée. Ce n'est
				// pas un oubli : `NkTensorGpu::Upload` écrit TOUJOURS depuis
				// l'octet 0 du tampon GPU — il n'existe pas d'upload partiel, donc
				// pas de découpage possible. Le tampon de zéros est statique et ne
				// grandit qu'au plus gros besoin ; ici le plus gros tenseur
				// d'adaptateur fait 8 × 18 944 floats = 607 Ko, et rien d'autre ne
				// passe par ce chemin (les activations sont toujours écrites en
				// entier par un noyau, jamais mises à zéro).
				static NkVector<uint8> zeros;
				if ((uint64)zeros.Size() < bytes) {
					zeros.Resize((NkVector<uint8>::SizeType)bytes);
					std::memset(zeros.Data(), 0, (usize)bytes);
				}
				if (!NkTensorGpu::Get().Upload(dst, zeros.Data(), (nk_size)bytes)) {
					SetErr(outError, "NkGpuZeroBuffer : Upload a échoué");
					return false;
				}
				return true;
			}

			bool NkGpuAxpy(uint64 x, uint64 y, uint64 count, float32 alpha, NkString *outError) {
				if (x == 0 || y == 0 || count == 0) {
					SetErr(outError, "NkGpuAxpy : arguments invalides");
					return false;
				}
				if (count > 0xFFFFFFFFull) {
					SetErr(outError, "NkGpuAxpy : count dépasse un dispatch 32 bits");
					return false;
				}
				uint32 p12[12] = {0};
				p12[0] = (uint32)count;
				p12[1] = FBits(alpha);
				if (!NkTensorGpu::Get().RunConvOp("nk_axpy", SrcAxpy(), x, y, p12, (uint32)count)) {
					SetErr(outError, "NkGpuAxpy : dispatch a échoué");
					return false;
				}
				return true;
			}

			bool NkGpuSumSquares(uint64 x, uint64 partials, uint64 count, NkString *outError) {
				if (x == 0 || partials == 0 || count == 0) {
					SetErr(outError, "NkGpuSumSquares : arguments invalides");
					return false;
				}
				const uint64 chunks = (count + kNkGpuSumSquaresChunk - 1ull) / kNkGpuSumSquaresChunk;
				uint32 p12[12] = {0};
				p12[0] = (uint32)chunks;
				p12[1] = (uint32)count;
				p12[2] = (uint32)kNkGpuSumSquaresChunk;
				if (!NkTensorGpu::Get().RunConvOp("nk_sumsq", SrcSumSquares(), x, partials, p12, (uint32)chunks)) {
					SetErr(outError, "NkGpuSumSquares : dispatch a échoué");
					return false;
				}
				return true;
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
