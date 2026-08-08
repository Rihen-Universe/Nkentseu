// =============================================================================
// NkQ6KGpu.cpp — implémentation des poids Q6_K résidents GPU + noyaux NkSL.
//
// Les noyaux ci-dessous rejouent TERME À TERME DequantQ6_K (NkGGUFDequant.cpp,
// transcrit de dequantize_row_q6_K dans ggml-quants.c) : mêmes entiers, même
// recentrage -32, même ORDRE de multiplication ((d·échelle)·quant). C'est
// délibéré : le test vise |Δ| == 0 sur la déquantification pure, pas « du même
// ordre de grandeur ». Toute divergence est donc un bug, jamais du bruit.
// =============================================================================
#include "NKInfer/NkQ6KGpu.h"
#include "NKInfer/NkQKGpuShaderCommon.h"

#include "NKTensor/NkTensorGpu.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {

				// -----------------------------------------------------------------
				// Préambule NkSL commun aux deux noyaux Q6_K.
				//
				// Il suppose que le storage buffer du binding 0 s'appelle `W` et
				// contient les blocs BRUTS lus tels quels du GGUF.
				//
				// TOUT EST EN OFFSET D'OCTET ABSOLU : un bloc Q6_K fait 210 octets,
				// non divisible par 4, donc il n'existe aucun « mot de départ du
				// bloc » comme en Q4_K. Le prix est une division par 4 et un
				// décalage par accès ; c'est le prix du format, pas un choix.
				// -----------------------------------------------------------------
				static const char *kQ6KHelpers = R"NKSL(
// Lecture d'un OCTET à l'offset ABSOLU `off` dans le tampon de blocs. Le GPU lit
// ses mots en little-endian, comme le x86 qui a écrit le GGUF : l'octet off vit
// dans le mot off/4, décalé de (off%4)*8 bits.
uint nkq6kByte(uint off) {
    uint w = W.data[off >> 2u];
    return (w >> ((off & 0x3u) * 8u)) & 0xFFu;
}

// Échelle de sous-bloc : int8 SIGNÉ (contrairement à Q4_K dont les échelles sont
// des entiers 6 bits NON signés). Un GPU ne sait pas lire un octet signé depuis
// un uint : on étend le signe à la main, sinon -1 vaudrait 255 et le poids
// changerait de signe ET d'amplitude.
int nkq6kScale(uint off) {
    int v = int(nkq6kByte(off));
    if (v > 0x7F) {
        v = v - 0x100;
    }
    return v;
}

// Super-échelle fp16 du bloc dont le premier octet est `base` : elle occupe les
// octets 208-209, donc — 210 n'étant pas multiple de 4 — elle est À CHEVAL sur
// deux mots un bloc sur deux. Les deux octets sont lus séparément puis
// recomposés en little-endian ; c'est exactement pour ce cas que l'accès
// octet par octet existe.
float nkq6kD(uint base) {
    uint lo = nkq6kByte(base + 208u);
    uint hi = nkq6kByte(base + 209u);
    return nkqkF16(lo | (hi << 8u));
}
)NKSL";

				// -----------------------------------------------------------------
				// Noyau 1 — déquantification seule.
				// Bindings de RunConvOp : buffers 0/1 (W, Y) + UBO 12 uints au
				// binding 2, dispatch 1D local_size_x = 64.
				//
				// UN THREAD POUR QUATRE ÉLÉMENTS, et pas un thread par élément :
				// la boucle CPU produit les quants par QUADRUPLETS (q1..q4) qui
				// partagent le MÊME octet de qh. Découper plus fin relirait quatre
				// fois cet octet et quatre fois la super-échelle. Un super-bloc
				// donne donc 64 threads (2 moitiés × 32 positions l), ce qui tombe
				// pile sur la taille de groupe.
				//
				// Correspondance avec la boucle CPU (n = 0 puis 128) :
				//   moitié h -> ql += h*64, qh += h*32, scales += h*8, y += h*128
				//   q1 = ql[l]     bas  | qh[l] bits 0-1   -> y[l+ 0], échelle is+0
				//   q2 = ql[l+32]  bas  | qh[l] bits 2-3   -> y[l+32], échelle is+2
				//   q3 = ql[l]     haut | qh[l] bits 4-5   -> y[l+64], échelle is+4
				//   q4 = ql[l+32]  haut | qh[l] bits 6-7   -> y[l+96], échelle is+6
				// avec is = l/16 (les échelles sont par sous-bloc de 16).
				// -----------------------------------------------------------------
				static const char *kQ6KDequantBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.quads) {
        uint b = idx >> 6u;          // super-bloc
        uint r = idx & 0x3Fu;        // rang dans le super-bloc (0..63)
        uint h = r >> 5u;            // moitié 0 ou 1
        uint l = r & 0x1Fu;          // position 0..31
        uint is = l >> 4u;           // sous-bloc de 16 : 0 ou 1
        uint base = b * 210u;
        uint qlB = base + h * 64u;
        uint qhB = base + 128u + h * 32u;
        uint scB = base + 192u + h * 8u;
        float d = nkq6kD(base);
        uint qla = nkq6kByte(qlB + l);
        uint qlb = nkq6kByte(qlB + l + 32u);
        uint qhv = nkq6kByte(qhB + l);
        int q1 = int((qla & 0xFu) | (((qhv >> 0u) & 0x3u) << 4u)) - 32;
        int q2 = int((qlb & 0xFu) | (((qhv >> 2u) & 0x3u) << 4u)) - 32;
        int q3 = int((qla >> 4u)  | (((qhv >> 4u) & 0x3u) << 4u)) - 32;
        int q4 = int((qlb >> 4u)  | (((qhv >> 6u) & 0x3u) << 4u)) - 32;
        uint yb = b * 256u + h * 128u + l;
        Y.data[yb +  0u] = (d * float(nkq6kScale(scB + is + 0u))) * float(q1);
        Y.data[yb + 32u] = (d * float(nkq6kScale(scB + is + 2u))) * float(q2);
        Y.data[yb + 64u] = (d * float(nkq6kScale(scB + is + 4u))) * float(q3);
        Y.data[yb + 96u] = (d * float(nkq6kScale(scB + is + 6u))) * float(q4);
    }
}
)NKSL";

				static const char *kQ6KDequantHead = R"NKSL(
@binding(set=0, binding=0) buffer BufW { uint data[]; } W;
@binding(set=0, binding=1) buffer BufY { float data[]; } Y;
@binding(set=0, binding=2) uniform Params {
    uint quads; uint p1; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
)NKSL";

				// -----------------------------------------------------------------
				// Noyau 2 — matmul FUSÉ déquantification-produit.
				// Bindings de RunOp3 : buffers 0/1/2 (W, X, Y) + UBO au binding 3.
				//
				// L'ORDRE DES BOUCLES N'EST PAS ARBITRAIRE : h (moitié), puis j
				// (le quadruplet q1..q4), puis l donne k = h*128 + j*32 + l, donc
				// un k STRICTEMENT CROISSANT — le même ordre de sommation que la
				// référence CPU. Sans cela l'écart mesuré mélangerait l'erreur du
				// noyau et celle du réordonnancement, et on ne saurait pas laquelle
				// on observe.
				//
				// Le découpage de l en deux tranches de 16 (boucle `t`) n'est pas
				// cosmétique : les échelles Q6_K sont par sous-bloc de 16, donc
				// c'est la granularité à laquelle `ds` est constant. La hisser hors
				// de la boucle interne évite 16 extensions de signe identiques.
				// -----------------------------------------------------------------
				static const char *kQ6KMatmulBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint m = idx % pc.M;
        uint n = idx / pc.M;
        uint rowBase = n * pc.bpr * 210u;
        uint xBase = m * pc.K;
        float acc = 0.0;
        for (uint bb = 0u; bb < pc.bpr; bb = bb + 1u) {
            uint base = rowBase + bb * 210u;
            float d = nkq6kD(base);
            uint kb = xBase + bb * 256u;
            for (uint h = 0u; h < 2u; h = h + 1u) {
                uint qlB = base + h * 64u;
                uint qhB = base + 128u + h * 32u;
                uint scB = base + 192u + h * 8u;
                uint kh = kb + h * 128u;
                for (uint j = 0u; j < 4u; j = j + 1u) {
                    uint lo = (j & 1u) * 32u;     // q2/q4 lisent ql[l+32]
                    uint hiNib = j >> 1u;         // q3/q4 lisent le nibble HAUT
                    uint shq = j * 2u;            // 2 bits hauts : 0,2,4,6
                    for (uint t = 0u; t < 2u; t = t + 1u) {
                        float ds = d * float(nkq6kScale(scB + t + j * 2u));
                        uint kbase = kh + j * 32u + t * 16u;
                        for (uint u = 0u; u < 16u; u = u + 1u) {
                            uint l = t * 16u + u;
                            uint qlv = nkq6kByte(qlB + lo + l);
                            uint nib = qlv & 0xFu;
                            if (hiNib == 1u) {
                                nib = qlv >> 4u;
                            }
                            uint qhv = nkq6kByte(qhB + l);
                            int q = int(nib | (((qhv >> shq) & 0x3u) << 4u)) - 32;
                            acc = acc + X.data[kbase + u] * (ds * float(q));
                        }
                    }
                }
            }
        }
        Y.data[m * pc.N + n] = acc;
    }
}
)NKSL";

				static const char *kQ6KMatmulHead = R"NKSL(
@binding(set=0, binding=0) buffer BufW { uint data[]; } W;
@binding(set=0, binding=1) buffer BufX { float data[]; } X;
@binding(set=0, binding=2) buffer BufY { float data[]; } Y;
@binding(set=0, binding=3) uniform Params {
    uint M; uint N; uint K; uint bpr;
    uint total; uint nTiles; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
)NKSL";

				// -----------------------------------------------------------------
				// Noyau 3 — matmul Q6_K TUILÉ avec mémoire partagée.
				//
				// Même structure et mêmes raisons qu'en Q4_K (cf. NkQ4KGpu.cpp) :
				// tuile de 8 colonnes × 8 tokens, 64 threads, W déquantifié UNE fois
				// par tuile dans 8 Ko de mémoire partagée au lieu d'une fois par
				// token, pas de 257 pour éviter le conflit de bancs 8 voies.
				//
				// LE PARTAGE DU TRAVAIL DIFFÈRE, PARCE QUE L'UNITÉ DE DÉCODAGE
				// DIFFÈRE. En Q4_K, un super-bloc se découpe naturellement en 8
				// sous-blocs de 32, donc 8 colonnes × 8 sous-blocs = 64 threads
				// tombent pile. En Q6_K, l'unité est le QUADRUPLET : une position l
				// produit quatre éléments (l, l+32, l+64, l+96) qui partagent le même
				// octet de qh, et il y a 64 positions (2 moitiés × 32) par super-bloc.
				// On donne donc à chaque thread 8 positions consécutives d'une même
				// moitié — 8 colonnes × 8 paquets = 64 threads, et 8 × 4 = 32 valeurs
				// écrites par thread, exactement comme en Q4_K.
				// -----------------------------------------------------------------
				static const char *kQ6KMatmulTiledBody = R"NKSL(
shared float nkq6kTile[2056];

@stage(compute)
@entry
void main() {
    uint g = gl_WorkGroupID.x;
    uint t = gl_LocalInvocationID.x;
    uint nt = g % pc.nTiles;
    uint mt = g / pc.nTiles;

    // Rôles pour le CHARGEMENT : colonne locale = t/8, paquet de 8 positions = t%8.
    uint lc = t >> 3u;
    uint lp = t & 0x7u;
    uint nLoad = nt * 8u + lc;

    // Rôles pour le CALCUL : colonne locale = t%8, token local = t/8.
    uint tn = t & 0x7u;
    uint tm = t >> 3u;
    uint n = nt * 8u + tn;
    uint m = mt * 8u + tm;

    float acc = 0.0;
    for (uint bb = 0u; bb < pc.bpr; bb = bb + 1u) {
        barrier();
        uint dst = lc * 257u;
        if (nLoad < pc.N) {
            uint base = (nLoad * pc.bpr + bb) * 210u;
            float d = nkq6kD(base);
            uint h = lp >> 2u;              // moitié 0/1 : paquets 0-3 puis 4-7
            uint lbase = (lp & 0x3u) * 8u;  // 8 positions consécutives
            uint qlB = base + h * 64u;
            uint qhB = base + 128u + h * 32u;
            uint scB = base + 192u + h * 8u;
            uint ob = dst + h * 128u;
            for (uint i = 0u; i < 8u; i = i + 1u) {
                uint l = lbase + i;
                uint is = l >> 4u;
                uint qla = nkq6kByte(qlB + l);
                uint qlb = nkq6kByte(qlB + l + 32u);
                uint qhv = nkq6kByte(qhB + l);
                int q1 = int((qla & 0xFu) | (((qhv >> 0u) & 0x3u) << 4u)) - 32;
                int q2 = int((qlb & 0xFu) | (((qhv >> 2u) & 0x3u) << 4u)) - 32;
                int q3 = int((qla >> 4u)  | (((qhv >> 4u) & 0x3u) << 4u)) - 32;
                int q4 = int((qlb >> 4u)  | (((qhv >> 6u) & 0x3u) << 4u)) - 32;
                nkq6kTile[ob + l +  0u] = (d * float(nkq6kScale(scB + is + 0u))) * float(q1);
                nkq6kTile[ob + l + 32u] = (d * float(nkq6kScale(scB + is + 2u))) * float(q2);
                nkq6kTile[ob + l + 64u] = (d * float(nkq6kScale(scB + is + 4u))) * float(q3);
                nkq6kTile[ob + l + 96u] = (d * float(nkq6kScale(scB + is + 6u))) * float(q4);
            }
        } else {
            // Colonne au-delà du poids : zéros, sinon la tuile garderait les
            // valeurs du tour précédent et un token valide les additionnerait.
            for (uint i = 0u; i < 32u; i = i + 1u) {
                nkq6kTile[dst + lp * 32u + i] = 0.0;
            }
        }
        barrier();
        if (m < pc.M) {
            uint xb = m * pc.K + bb * 256u;
            uint wb = tn * 257u;
            for (uint kk = 0u; kk < 256u; kk = kk + 1u) {
                acc = acc + X.data[xb + kk] * nkq6kTile[wb + kk];
            }
        }
    }
    if (m < pc.M) {
        if (n < pc.N) {
            Y.data[m * pc.N + n] = acc;
        }
    }
}
)NKSL";

				const NkString &DequantSource() {
					static NkString src =
						NkString(kQ6KDequantHead) + NkString(kNkQKGpuF16Helper) + NkString(kQ6KHelpers) +
						NkString(kQ6KDequantBody);
					return src;
				}

				const NkString &MatmulSource() {
					static NkString src = NkString(kQ6KMatmulHead) + NkString(kNkQKGpuF16Helper) +
										  NkString(kQ6KHelpers) + NkString(kQ6KMatmulBody);
					return src;
				}

				const NkString &MatmulTiledSource() {
					static NkString src = NkString(kQ6KMatmulHead) + NkString(kNkQKGpuF16Helper) +
										  NkString(kQ6KHelpers) + NkString(kQ6KMatmulTiledBody);
					return src;
				}

				void SetErr(NkString *outError, const char *msg) {
					if (outError)
						*outError = NkString(msg);
				}

			} // namespace

			uint64 NkQ6KExpectedBytes(int64 rows, int64 cols) {
				if (rows <= 0 || cols <= 0)
					return 0;
				if ((uint64)cols % kNkQ6KBlockElems != 0)
					return 0; // pas de découpage en super-blocs -> format invalide
				return (uint64)rows * ((uint64)cols / kNkQ6KBlockElems) * kNkQ6KBlockBytes;
			}

			bool NkQ6KGpuUpload(const void *rawBlocks, uint64 rawByteCount, int64 rows, int64 cols,
								NkQ6KGpuWeight &out, NkString *outError) {
				out = NkQ6KGpuWeight{};
				if (!rawBlocks) {
					SetErr(outError, "NkQ6KGpuUpload : buffer source nul");
					return false;
				}
				const uint64 need = NkQ6KExpectedBytes(rows, cols);
				if (need == 0) {
					SetErr(outError, "NkQ6KGpuUpload : dimensions invalides (cols doit être un multiple de 256)");
					return false;
				}
				if (rawByteCount != need) {
					SetErr(outError, "NkQ6KGpuUpload : rawByteCount != taille Q6_K attendue pour [rows, cols]");
					return false;
				}
				// L'offset d'octet le plus haut lu par le shader est `need - 1`, donc
				// le MOT le plus haut est (need-1)/4. Comme 210 n'est pas multiple de
				// 4, ce mot peut dépasser `need` de trois octets : sans marge, le
				// dernier bloc provoquerait une lecture hors tampon. On arrondit à 16
				// (certains backends exigent déjà ce multiple pour un storage buffer),
				// ce qui couvre les 3 octets au pire.
				const uint64 alloc = (need + 15ull) & ~15ull;
				NkTensorGpu &gpu = NkTensorGpu::Get();
				if (!gpu.IsAvailable()) {
					SetErr(outError, "NkQ6KGpuUpload : aucun device compute GPU disponible");
					return false;
				}
				uint64 buf = gpu.CreateBuffer((nk_size)alloc);
				if (buf == 0) {
					SetErr(outError, "NkQ6KGpuUpload : CreateBuffer a échoué (VRAM ou limite de ressource)");
					return false;
				}
				if (!gpu.Upload(buf, rawBlocks, (nk_size)need)) {
					gpu.DestroyBuffer(buf);
					SetErr(outError, "NkQ6KGpuUpload : Upload a échoué");
					return false;
				}
				out.buffer = buf;
				out.rows = rows;
				out.cols = cols;
				out.blocksPerRow = cols / (int64)kNkQ6KBlockElems;
				out.byteCount = need;
				return true;
			}

			void NkQ6KGpuRelease(NkQ6KGpuWeight &w) {
				if (w.buffer != 0)
					NkTensorGpu::Get().DestroyBuffer(w.buffer);
				w = NkQ6KGpuWeight{};
			}

			bool NkQ6KGpuDequantizeTo(const NkQ6KGpuWeight &w, uint64 outBuffer, NkString *outError) {
				if (!w.IsValid() || outBuffer == 0) {
					SetErr(outError, "NkQ6KGpuDequantizeTo : poids ou tampon de sortie invalide");
					return false;
				}
				const uint64 nBlocks = (uint64)w.rows * (uint64)w.blocksPerRow;
				const uint64 quads = nBlocks * 64ull; // 64 threads par super-bloc, 4 éléments chacun
				if (quads > 0xFFFFFFFFull) {
					SetErr(outError, "NkQ6KGpuDequantizeTo : trop de threads pour un dispatch 32 bits");
					return false;
				}
				uint32 p12[12] = {0};
				p12[0] = (uint32)quads;
				if (!NkTensorGpu::Get().RunConvOp("q6k_dequant", DequantSource(), w.buffer, outBuffer, p12,
												  (uint32)quads)) {
					SetErr(outError, "NkQ6KGpuDequantizeTo : dispatch du noyau q6k_dequant a échoué");
					return false;
				}
				return true;
			}

			bool NkQ6KGpuDequantize(const NkQ6KGpuWeight &w, NkVector<float32> &out, NkString *outError) {
				out.Clear();
				if (!w.IsValid()) {
					SetErr(outError, "NkQ6KGpuDequantize : poids invalide");
					return false;
				}
				const uint64 n = (uint64)w.Numel();
				NkTensorGpu &gpu = NkTensorGpu::Get();
				uint64 ybuf = gpu.CreateBuffer((nk_size)(n * sizeof(float32)));
				if (ybuf == 0) {
					SetErr(outError, "NkQ6KGpuDequantize : CreateBuffer (sortie f32) a échoué");
					return false;
				}
				bool ok = NkQ6KGpuDequantizeTo(w, ybuf, outError);
				if (ok) {
					out.Resize((NkVector<float32>::SizeType)n);
					ok = gpu.Download(ybuf, out.Data(), (nk_size)(n * sizeof(float32)));
					if (!ok)
						SetErr(outError, "NkQ6KGpuDequantize : Download a échoué");
				}
				gpu.DestroyBuffer(ybuf);
				return ok;
			}

			bool NkQ6KGpuMatmul(const NkQ6KGpuWeight &w, uint64 xBuffer, uint64 yBuffer, int64 M, NkString *outError) {
				return NkQ6KGpuMatmulEx(w, xBuffer, yBuffer, M, NkQ6KMatmulKernel::NK_AUTO, outError);
			}

			bool NkQ6KGpuMatmulEx(const NkQ6KGpuWeight &w, uint64 xBuffer, uint64 yBuffer, int64 M,
								  NkQ6KMatmulKernel kernel, NkString *outError) {
				if (!w.IsValid() || xBuffer == 0 || yBuffer == 0 || M <= 0) {
					SetErr(outError, "NkQ6KGpuMatmul : arguments invalides");
					return false;
				}
				const uint64 total = (uint64)M * (uint64)w.rows;
				if (total > 0xFFFFFFFFull) {
					SetErr(outError, "NkQ6KGpuMatmul : M·N dépasse un dispatch 32 bits");
					return false;
				}
				const bool automatic = (kernel == NkQ6KMatmulKernel::NK_AUTO);
				if (automatic)
					kernel = (M < kNkQ6KTiledMinM) ? NkQ6KMatmulKernel::NK_SIMPLE : NkQ6KMatmulKernel::NK_TILED;

				uint32 p12[12] = {0};
				p12[0] = (uint32)M;
				p12[1] = (uint32)w.rows;		 // N
				p12[2] = (uint32)w.cols;		 // K
				p12[3] = (uint32)w.blocksPerRow; // bpr
				p12[4] = (uint32)total;

				if (kernel == NkQ6KMatmulKernel::NK_SIMPLE) {
					if (!NkTensorGpu::Get().RunOp3("q6k_matmul", MatmulSource(), w.buffer, xBuffer, yBuffer, p12,
												   (uint32)total)) {
						SetErr(outError, "NkQ6KGpuMatmul : dispatch du noyau q6k_matmul a échoué");
						return false;
					}
					return true;
				}

				// RunOp3 dispatche (count + 63) / 64 groupes : on lui passe donc
				// `groupes × 64` pour obtenir exactement le nombre de groupes voulu,
				// la tuile étant portée par gl_WorkGroupID.
				const uint64 nTiles = ((uint64)w.rows + 7ull) / 8ull;
				const uint64 mTiles = ((uint64)M + 7ull) / 8ull;
				const uint64 groups = nTiles * mTiles;
				if (groups * 64ull > 0xFFFFFFFFull) {
					SetErr(outError, "NkQ6KGpuMatmul : trop de groupes de travail pour un dispatch 32 bits");
					return false;
				}
				p12[5] = (uint32)nTiles;
				if (!NkTensorGpu::Get().RunOp3("q6k_matmul_tiled", MatmulTiledSource(), w.buffer, xBuffer, yBuffer, p12,
											   (uint32)(groups * 64ull))) {
					// Repli en mode AUTOMATIQUE seulement : un backend sans mémoire
					// partagée doit continuer à calculer juste. Quand le tuilé est
					// demandé explicitement (mesure), l'échec remonte tel quel.
					if (automatic) {
						p12[5] = 0;
						if (NkTensorGpu::Get().RunOp3("q6k_matmul", MatmulSource(), w.buffer, xBuffer, yBuffer, p12,
													  (uint32)total))
							return true;
					}
					SetErr(outError, "NkQ6KGpuMatmul : dispatch du noyau q6k_matmul_tiled a échoué");
					return false;
				}
				return true;
			}

			bool NkQ6KGpuMatmulCpu(const NkQ6KGpuWeight &w, const float32 *x, int64 M, NkVector<float32> &outY,
								   NkString *outError) {
				return NkQ6KGpuMatmulCpuEx(w, x, M, NkQ6KMatmulKernel::NK_AUTO, outY, outError);
			}

			bool NkQ6KGpuMatmulCpuEx(const NkQ6KGpuWeight &w, const float32 *x, int64 M, NkQ6KMatmulKernel kernel,
									 NkVector<float32> &outY, NkString *outError) {
				outY.Clear();
				if (!w.IsValid() || !x || M <= 0) {
					SetErr(outError, "NkQ6KGpuMatmulCpu : arguments invalides");
					return false;
				}
				NkTensorGpu &gpu = NkTensorGpu::Get();
				const uint64 xn = (uint64)M * (uint64)w.cols;
				const uint64 yn = (uint64)M * (uint64)w.rows;
				uint64 xbuf = gpu.CreateBuffer((nk_size)(xn * sizeof(float32)));
				uint64 ybuf = gpu.CreateBuffer((nk_size)(yn * sizeof(float32)));
				bool ok = (xbuf != 0 && ybuf != 0);
				if (!ok)
					SetErr(outError, "NkQ6KGpuMatmulCpu : CreateBuffer a échoué");
				if (ok) {
					ok = gpu.Upload(xbuf, x, (nk_size)(xn * sizeof(float32)));
					if (!ok)
						SetErr(outError, "NkQ6KGpuMatmulCpu : Upload de X a échoué");
				}
				if (ok)
					ok = NkQ6KGpuMatmulEx(w, xbuf, ybuf, M, kernel, outError);
				if (ok) {
					outY.Resize((NkVector<float32>::SizeType)yn);
					ok = gpu.Download(ybuf, outY.Data(), (nk_size)(yn * sizeof(float32)));
					if (!ok)
						SetErr(outError, "NkQ6KGpuMatmulCpu : Download de Y a échoué");
				}
				if (xbuf)
					gpu.DestroyBuffer(xbuf);
				if (ybuf)
					gpu.DestroyBuffer(ybuf);
				return ok;
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
