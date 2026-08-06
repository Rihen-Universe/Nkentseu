// =============================================================================
// NkQ4KGpu.cpp — implémentation des poids Q4_K résidents GPU + noyaux NkSL.
//
// Les deux noyaux ci-dessous rejouent LIGNE À LIGNE l'algorithme de
// DequantQ4_K (NkGGUFDequant.cpp, lui-même transcrit de ggml-quants.c) :
// mêmes entiers, mêmes multiplications, même ORDRE de somme (k croissant).
// C'est délibéré : la comparaison GPU/CPU du test doit pouvoir viser |Δ| == 0
// sur la déquantification pure, pas seulement « du même ordre de grandeur ».
// Toute divergence lue par le test est donc un vrai bug, jamais du bruit.
// =============================================================================
#include "NKInfer/NkQ4KGpu.h"
#include "NKInfer/NkQKGpuShaderCommon.h"

#include "NKTensor/NkTensorGpu.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {

				// Un super-bloc Q4_K = 144 octets = 36 mots de 32 bits. Le fait que
				// 144 soit divisible par 4 est ce qui autorise à lire le tampon brut
				// comme un `uint[]` côté shader (aucun SSBO d'octets n'est disponible
				// sans extension) : le bloc b commence exactement au mot b*36.
				constexpr uint32 kWordsPerBlock = 36;
				static_assert(kWordsPerBlock * 4u == kNkQ4KBlockBytes,
							  "un bloc Q4_K doit tenir en un nombre ENTIER de mots 32 bits (le shader indexe en uint)");

				// -----------------------------------------------------------------
				// Préambule NkSL commun aux noyaux Q4_K.
				//
				// Il suppose que le storage buffer du binding 0 s'appelle `W` et
				// contient les blocs BRUTS. Les helpers y accèdent directement (motif
				// déjà employé par les noyaux IBL : helper pur + lecture de buffer).
				// Le décodeur fp16 vient de NkQKGpuShaderCommon.h : Q4_K et Q6_K
				// partagent la MÊME transcription de NkF16BitsToF32, pour qu'aucune
				// des deux ne puisse dériver de l'autre.
				//
				// MASQUES EN HEXADÉCIMAL depuis 2026-08-05 : le lexer NkSL accepte
				// désormais 0x/0b. `0x3Fu` se relit contre « échelle sur 6 bits » de
				// la spec ggml ; `63u` ne se relit pas. Les valeurs générées sont
				// STRICTEMENT identiques (vérifié octet pour octet sur les 7 cibles
				// par NkSLComputeCheck), donc la déquantification reste bit-à-bit
				// celle du CPU — c'est le test (a) qui le prouve à chaque exécution.
				//
				// Dialecte NkSL respecté : boucles `i = i + 1u`, pas de `return`
				// anticipé dans main.
				// -----------------------------------------------------------------
				static const char *kQ4KHelpers = R"NKSL(
// Lecture d'un OCTET à l'intérieur d'un bloc : `base` = index du premier mot du
// bloc, `off` = décalage en octets dans le bloc (0..143). Le GPU lit les mots en
// little-endian, comme le CPU x86 qui a écrit le fichier GGUF : l'octet off est
// donc dans le mot off/4, décalé de (off%4)*8 bits.
uint nkq4kByte(uint base, uint off) {
    uint w = W.data[base + (off >> 2u)];
    return (w >> ((off & 0x3u) * 8u)) & 0xFFu;
}

// get_scale_min_k4 : désempaquette la paire (échelle, minimum) 6 bits du
// sous-bloc j (0..7) depuis les 12 octets `scales` qui commencent à l'octet 4
// du bloc. Transcription terme à terme de ggml-quants.c (cf GetScaleMinK4 dans
// NkGGUFDequant.cpp). Retourne uvec2(échelle, minimum).
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

				// -----------------------------------------------------------------
				// Noyau 1 — déquantification seule (bindings de RunConvOp : 2 storage
				// buffers 0/1 + UBO 12 uints au binding 2).
				//
				// UN THREAD PAR SOUS-BLOC DE 32 (et non par super-bloc de 256) : le
				// sous-bloc est l'unité qui porte UNE paire (échelle, minimum), donc
				// c'est le plus petit découpage qui ne relit pas les métadonnées
				// plusieurs fois, et il donne 8× plus de parallélisme.
				//
				// Correspondance avec la boucle CPU : le CPU traite les 256 éléments
				// par paquets de 64 (is = 0,2,4,6) ; le paquet is écrit d'abord 32
				// éléments à partir des nibbles BAS de qs[is/2*32 .. +31], puis 32 à
				// partir des nibbles HAUTS des MÊMES octets. Donc pour le sous-bloc
				// sb = 0..7 : octets qs à partir de 16 + (sb/2)*32, nibble bas si sb
				// est pair / haut si impair, échelle d'index sb, sortie à sb*32.
				// -----------------------------------------------------------------
				static const char *kQ4KDequantBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.subBlocks) {
        uint b = idx >> 3u;
        uint sb = idx & 0x7u;
        uint base = b * 36u;
        float d = nkqkF16(W.data[base] & 0xFFFFu);
        float dmin = nkqkF16(W.data[base] >> 16u);
        uvec2 sm = nkq4kScaleMin(base, sb);
        float ds = d * float(sm.x);
        float ms = dmin * float(sm.y);
        uint qoff = 16u + (sb >> 1u) * 32u;
        uint hi = sb & 0x1u;
        uint obase = idx * 32u;
        for (uint l = 0u; l < 32u; l = l + 1u) {
            uint qb = nkq4kByte(base, qoff + l);
            uint q = qb & 0xFu;
            if (hi == 1u) {
                q = qb >> 4u;
            }
            Y.data[obase + l] = ds * float(q) - ms;
        }
    }
}
)NKSL";

				static const char *kQ4KDequantHead = R"NKSL(
@binding(set=0, binding=0) buffer BufW { uint data[]; } W;
@binding(set=0, binding=1) buffer BufY { float data[]; } Y;
@binding(set=0, binding=2) uniform Params {
    uint subBlocks; uint p1; uint p2; uint p3;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
)NKSL";

				// -----------------------------------------------------------------
				// Noyau 2 — matmul FUSÉ déquantification-produit (bindings de RunOp3 :
				// 3 storage buffers 0/1/2 + UBO 12 uints au binding 3).
				//
				// Y[m,n] = Σ_k X[m,k] · W[n,k] : W n'est JAMAIS déquantifié en
				// mémoire, chaque bloc est décodé dans des registres puis consommé
				// immédiatement. Un thread = un élément de sortie.
				//
				// Numérotation des threads : m varie le plus vite (m = idx % M,
				// n = idx / M). POURQUOI : M est petit (nombre de tokens, 1 à 8), donc
				// M threads consécutifs — dans le même warp — parcourent la MÊME ligne
				// de W et se partagent ses lectures via le cache, au lieu d'attaquer M
				// lignes distantes de plusieurs kilo-octets.
				//
				// Le k croît strictement, donc l'accumulation suit le MÊME ordre que la
				// référence CPU : l'écart attendu vient uniquement d'une éventuelle
				// contraction FMA côté pilote.
				// -----------------------------------------------------------------
				static const char *kQ4KMatmulBody = R"NKSL(
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint m = idx % pc.M;
        uint n = idx / pc.M;
        uint rowBase = n * pc.bpr * 36u;
        uint xBase = m * pc.K;
        float acc = 0.0;
        for (uint bb = 0u; bb < pc.bpr; bb = bb + 1u) {
            uint base = rowBase + bb * 36u;
            float d = nkqkF16(W.data[base] & 0xFFFFu);
            float dmin = nkqkF16(W.data[base] >> 16u);
            uint kb = xBase + bb * 256u;
            for (uint sb = 0u; sb < 8u; sb = sb + 1u) {
                uvec2 sm = nkq4kScaleMin(base, sb);
                float ds = d * float(sm.x);
                float ms = dmin * float(sm.y);
                uint qoff = 16u + (sb >> 1u) * 32u;
                uint hi = sb & 0x1u;
                uint kbase = kb + sb * 32u;
                for (uint l = 0u; l < 32u; l = l + 1u) {
                    uint qb = nkq4kByte(base, qoff + l);
                    uint q = qb & 0xFu;
                    if (hi == 1u) {
                        q = qb >> 4u;
                    }
                    acc = acc + X.data[kbase + l] * (ds * float(q) - ms);
                }
            }
        }
        Y.data[m * pc.N + n] = acc;
    }
}
)NKSL";

				static const char *kQ4KMatmulHead = R"NKSL(
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
				// Noyau 3 — matmul TUILÉ avec mémoire partagée.
				//
				// LE DÉFAUT CORRIGÉ : dans le noyau ci-dessus, chaque thread relit LA
				// LIGNE ENTIÈRE de W dont il a besoin. Pour M tokens, le poids
				// quantifié traverse donc M fois le bus mémoire. À M = 4 (inférence)
				// c'est tolérable ; à M = 256 — une séquence complète, le régime de
				// l'entraînement LoRA — c'est le mur : 36 Mo × 256 = 9,3 Go à lire.
				//
				// LA TUILE : un groupe de travail = 8 colonnes de sortie (n) × 8 tokens
				// (m) = 64 threads. Pour chaque super-bloc k (256 valeurs) :
				//   1. les 64 threads déquantifient COOPÉRATIVEMENT les 8 super-blocs
				//      correspondants — un thread par (colonne, sous-bloc de 32), ce
				//      qui tombe pile : 8 × 8 = 64 ;
				//   2. le résultat (8 × 256 floats = 8 Ko) va en mémoire partagée ;
				//   3. les 8 tokens de la tuile le consomment TOUS.
				// W n'est donc lu et déquantifié qu'UNE fois par tuile au lieu d'une
				// fois par token : le trafic est divisé par 8, et le coût de
				// déquantification aussi — c'est le second gain, souvent oublié : le
				// décodage fp16 et le désempaquetage 6 bits ne sont pas gratuits.
				//
				// 8 Ko DE MÉMOIRE PARTAGÉE, PAS 16 : X n'est PAS mis en cache. Le
				// minimum garanti par Vulkan (maxComputeSharedMemorySize) est 16 Ko ;
				// y loger W et X saturerait exactement la limite, et un backend qui ne
				// dépasse pas ce minimum refuserait le pipeline. Or X n'en a pas
				// besoin : les 8 threads d'un même token lisent la MÊME valeur de X au
				// même instant, que le matériel diffuse.
				//
				// DEUX BARRIÈRES PAR TOUR, toutes deux nécessaires : celle d'entrée
				// empêche un thread rapide d'écraser la tuile qu'un autre lit encore au
				// tour précédent ; celle de sortie garantit que la tuile est complète
				// avant qu'on la lise. Elles sont hors de tout `if` — une barrière en
				// flot de contrôle non uniforme est un comportement indéfini.
				//
				// L'ordre d'accumulation (k croissant) est EXACTEMENT celui du noyau
				// simple et de la référence CPU : les deux noyaux doivent donner le même
				// résultat, sinon la comparaison ne dirait pas lequel a tort.
				// -----------------------------------------------------------------
				static const char *kQ4KMatmulTiledBody = R"NKSL(
// 8 colonnes × 256 valeurs, mais avec un pas de 257 ET NON 256.
//
// POURQUOI CE +1 : la mémoire partagée est découpée en 32 bancs de 4 octets, et
// deux threads qui visent le même banc au même cycle sont SÉRIALISÉS. Avec un
// pas de 256, l'adresse tn*256 + kk tombe toujours dans le banc kk % 32, quelle
// que soit la colonne tn : les 8 colonnes lues simultanément par un warp se
// battent pour UN SEUL banc — un conflit 8 voies, soit 8 fois le temps d'accès.
// Avec 257 (= 256 + 1, et 257 % 32 = 1), la colonne tn décale l'adresse d'un
// banc : les 8 colonnes atterrissent dans 8 bancs distincts. Le coût est de
// 32 octets de mémoire partagée en tout.
shared float nkq4kTile[2056];

@stage(compute)
@entry
void main() {
    uint g = gl_WorkGroupID.x;
    uint t = gl_LocalInvocationID.x;
    uint nt = g % pc.nTiles;
    uint mt = g / pc.nTiles;

    // Rôles pour le CHARGEMENT : colonne locale = t/8, sous-bloc = t%8.
    uint lc = t >> 3u;
    uint lsb = t & 0x7u;
    uint nLoad = nt * 8u + lc;

    // Rôles pour le CALCUL : colonne locale = t%8 (des n consécutifs sur des
    // threads consécutifs -> écriture de Y coalescée), token local = t/8.
    uint tn = t & 0x7u;
    uint tm = t >> 3u;
    uint n = nt * 8u + tn;
    uint m = mt * 8u + tm;

    float acc = 0.0;
    for (uint bb = 0u; bb < pc.bpr; bb = bb + 1u) {
        barrier();
        uint dst = lc * 257u + lsb * 32u;
        if (nLoad < pc.N) {
            uint base = (nLoad * pc.bpr + bb) * 36u;
            float d = nkqkF16(W.data[base] & 0xFFFFu);
            float dmin = nkqkF16(W.data[base] >> 16u);
            uvec2 sm = nkq4kScaleMin(base, lsb);
            float ds = d * float(sm.x);
            float ms = dmin * float(sm.y);
            uint qoff = 16u + (lsb >> 1u) * 32u;
            uint hi = lsb & 0x1u;
            for (uint l = 0u; l < 32u; l = l + 1u) {
                uint qb = nkq4kByte(base, qoff + l);
                uint q = qb & 0xFu;
                if (hi == 1u) {
                    q = qb >> 4u;
                }
                nkq4kTile[dst + l] = ds * float(q) - ms;
            }
        } else {
            // Colonne au-delà du poids (N non multiple de 8) : on remplit quand
            // même de zéros, sinon la tuile garderait les valeurs du tour
            // précédent et un token valide les additionnerait.
            for (uint l = 0u; l < 32u; l = l + 1u) {
                nkq4kTile[dst + l] = 0.0;
            }
        }
        barrier();
        if (m < pc.M) {
            uint xb = m * pc.K + bb * 256u;
            uint wb = tn * 257u;
            for (uint kk = 0u; kk < 256u; kk = kk + 1u) {
                acc = acc + X.data[xb + kk] * nkq4kTile[wb + kk];
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

				// Les sources complètes sont assemblées une seule fois (statiques
				// locales) : NkTensorGpu met déjà le PIPELINE en cache par nom, mais
				// autant ne pas reconcaténer les chaînes à chaque appel.
				const NkString &DequantSource() {
					static NkString src = NkString(kQ4KDequantHead) + NkString(kNkQKGpuF16Helper) +
										  NkString(kQ4KHelpers) + NkString(kQ4KDequantBody);
					return src;
				}

				const NkString &MatmulSource() {
					static NkString src = NkString(kQ4KMatmulHead) + NkString(kNkQKGpuF16Helper) +
										  NkString(kQ4KHelpers) + NkString(kQ4KMatmulBody);
					return src;
				}

				const NkString &MatmulTiledSource() {
					static NkString src = NkString(kQ4KMatmulHead) + NkString(kNkQKGpuF16Helper) +
										  NkString(kQ4KHelpers) + NkString(kQ4KMatmulTiledBody);
					return src;
				}

				void SetErr(NkString *outError, const char *msg) {
					if (outError)
						*outError = NkString(msg);
				}

			} // namespace

			uint64 NkQ4KExpectedBytes(int64 rows, int64 cols) {
				if (rows <= 0 || cols <= 0)
					return 0;
				if ((uint64)cols % kNkQ4KBlockElems != 0)
					return 0; // pas de découpage en super-blocs -> format invalide
				return (uint64)rows * ((uint64)cols / kNkQ4KBlockElems) * kNkQ4KBlockBytes;
			}

			bool NkQ4KGpuUpload(const void *rawBlocks, uint64 rawByteCount, int64 rows, int64 cols,
								NkQ4KGpuWeight &out, NkString *outError) {
				out = NkQ4KGpuWeight{};
				if (!rawBlocks) {
					SetErr(outError, "NkQ4KGpuUpload : buffer source nul");
					return false;
				}
				const uint64 need = NkQ4KExpectedBytes(rows, cols);
				if (need == 0) {
					SetErr(outError, "NkQ4KGpuUpload : dimensions invalides (cols doit être un multiple de 256)");
					return false;
				}
				if (rawByteCount != need) {
					SetErr(outError, "NkQ4KGpuUpload : rawByteCount != taille Q4_K attendue pour [rows, cols]");
					return false;
				}
				NkTensorGpu &gpu = NkTensorGpu::Get();
				if (!gpu.IsAvailable()) {
					SetErr(outError, "NkQ4KGpuUpload : aucun device compute GPU disponible");
					return false;
				}
				// Arrondi à 16 octets : certains backends exigent un multiple de 16
				// pour un storage buffer. On alloue plus, on n'écrit que `need`.
				const uint64 alloc = (need + 15ull) & ~15ull;
				uint64 buf = gpu.CreateBuffer((nk_size)alloc);
				if (buf == 0) {
					SetErr(outError, "NkQ4KGpuUpload : CreateBuffer a échoué (VRAM ou limite de ressource)");
					return false;
				}
				if (!gpu.Upload(buf, rawBlocks, (nk_size)need)) {
					gpu.DestroyBuffer(buf);
					SetErr(outError, "NkQ4KGpuUpload : Upload a échoué");
					return false;
				}
				out.buffer = buf;
				out.rows = rows;
				out.cols = cols;
				out.blocksPerRow = cols / (int64)kNkQ4KBlockElems;
				out.byteCount = need;
				return true;
			}

			void NkQ4KGpuRelease(NkQ4KGpuWeight &w) {
				if (w.buffer != 0)
					NkTensorGpu::Get().DestroyBuffer(w.buffer);
				w = NkQ4KGpuWeight{};
			}

			bool NkQ4KGpuDequantizeTo(const NkQ4KGpuWeight &w, uint64 outBuffer, NkString *outError) {
				if (!w.IsValid() || outBuffer == 0) {
					SetErr(outError, "NkQ4KGpuDequantizeTo : poids ou tampon de sortie invalide");
					return false;
				}
				const uint64 nBlocks = (uint64)w.rows * (uint64)w.blocksPerRow;
				const uint64 subBlocks = nBlocks * 8ull;
				if (subBlocks > 0xFFFFFFFFull) {
					SetErr(outError, "NkQ4KGpuDequantizeTo : trop de sous-blocs pour un dispatch 32 bits");
					return false;
				}
				uint32 p12[12] = {0};
				p12[0] = (uint32)subBlocks;
				if (!NkTensorGpu::Get().RunConvOp("q4k_dequant", DequantSource(), w.buffer, outBuffer, p12,
												  (uint32)subBlocks)) {
					SetErr(outError, "NkQ4KGpuDequantizeTo : dispatch du noyau q4k_dequant a échoué");
					return false;
				}
				return true;
			}

			bool NkQ4KGpuDequantize(const NkQ4KGpuWeight &w, NkVector<float32> &out, NkString *outError) {
				out.Clear();
				if (!w.IsValid()) {
					SetErr(outError, "NkQ4KGpuDequantize : poids invalide");
					return false;
				}
				const uint64 n = (uint64)w.Numel();
				NkTensorGpu &gpu = NkTensorGpu::Get();
				uint64 ybuf = gpu.CreateBuffer((nk_size)(n * sizeof(float32)));
				if (ybuf == 0) {
					SetErr(outError, "NkQ4KGpuDequantize : CreateBuffer (sortie f32) a échoué");
					return false;
				}
				bool ok = NkQ4KGpuDequantizeTo(w, ybuf, outError);
				if (ok) {
					out.Resize((NkVector<float32>::SizeType)n);
					ok = gpu.Download(ybuf, out.Data(), (nk_size)(n * sizeof(float32)));
					if (!ok)
						SetErr(outError, "NkQ4KGpuDequantize : Download a échoué");
				}
				gpu.DestroyBuffer(ybuf);
				return ok;
			}

			bool NkQ4KGpuMatmul(const NkQ4KGpuWeight &w, uint64 xBuffer, uint64 yBuffer, int64 M, NkString *outError) {
				return NkQ4KGpuMatmulEx(w, xBuffer, yBuffer, M, NkQ4KMatmulKernel::NK_AUTO, outError);
			}

			bool NkQ4KGpuMatmulEx(const NkQ4KGpuWeight &w, uint64 xBuffer, uint64 yBuffer, int64 M,
								  NkQ4KMatmulKernel kernel, NkString *outError) {
				if (!w.IsValid() || xBuffer == 0 || yBuffer == 0 || M <= 0) {
					SetErr(outError, "NkQ4KGpuMatmul : arguments invalides");
					return false;
				}
				const uint64 total = (uint64)M * (uint64)w.rows;
				if (total > 0xFFFFFFFFull) {
					SetErr(outError, "NkQ4KGpuMatmul : M·N dépasse un dispatch 32 bits");
					return false;
				}
				const bool automatic = (kernel == NkQ4KMatmulKernel::NK_AUTO);
				if (automatic)
					kernel = (M < kNkQ4KTiledMinM) ? NkQ4KMatmulKernel::NK_SIMPLE : NkQ4KMatmulKernel::NK_TILED;

				uint32 p12[12] = {0};
				p12[0] = (uint32)M;
				p12[1] = (uint32)w.rows;		 // N
				p12[2] = (uint32)w.cols;		 // K
				p12[3] = (uint32)w.blocksPerRow; // bpr
				p12[4] = (uint32)total;

				if (kernel == NkQ4KMatmulKernel::NK_SIMPLE) {
					if (!NkTensorGpu::Get().RunOp3("q4k_matmul", MatmulSource(), w.buffer, xBuffer, yBuffer, p12,
												   (uint32)total)) {
						SetErr(outError, "NkQ4KGpuMatmul : dispatch du noyau q4k_matmul a échoué");
						return false;
					}
					return true;
				}

				// Noyau tuilé : RunOp3 dispatche (count + 63) / 64 groupes de 64
				// threads. On lui passe donc `groupes × 64` pour obtenir EXACTEMENT le
				// nombre de groupes voulu — la tuile n'est pas un découpage d'index
				// plat, c'est le groupe lui-même qui porte la tuile (via
				// gl_WorkGroupID). Aucune ligne de NkTensorGpu n'a besoin de changer.
				const uint64 nTiles = ((uint64)w.rows + 7ull) / 8ull;
				const uint64 mTiles = ((uint64)M + 7ull) / 8ull;
				const uint64 groups = nTiles * mTiles;
				if (groups * 64ull > 0xFFFFFFFFull) {
					SetErr(outError, "NkQ4KGpuMatmul : trop de groupes de travail pour un dispatch 32 bits");
					return false;
				}
				p12[5] = (uint32)nTiles;
				if (!NkTensorGpu::Get().RunOp3("q4k_matmul_tiled", MatmulTiledSource(), w.buffer, xBuffer, yBuffer, p12,
											   (uint32)(groups * 64ull))) {
					// REPLI EN MODE AUTOMATIQUE UNIQUEMENT : un backend qui refuse la
					// mémoire partagée ou les barrières doit continuer à calculer juste,
					// même moins vite. Quand le noyau tuilé a été demandé EXPLICITEMENT
					// (mesure comparative), on ne masque rien : l'échec remonte tel quel,
					// sinon le tableau de mesures comparerait le tuilé… au simple.
					if (automatic) {
						p12[5] = 0;
						if (NkTensorGpu::Get().RunOp3("q4k_matmul", MatmulSource(), w.buffer, xBuffer, yBuffer, p12,
													  (uint32)total))
							return true;
					}
					SetErr(outError, "NkQ4KGpuMatmul : dispatch du noyau q4k_matmul_tiled a échoué");
					return false;
				}
				return true;
			}

			bool NkQ4KGpuMatmulCpu(const NkQ4KGpuWeight &w, const float32 *x, int64 M, NkVector<float32> &outY,
								   NkString *outError) {
				return NkQ4KGpuMatmulCpuEx(w, x, M, NkQ4KMatmulKernel::NK_AUTO, outY, outError);
			}

			bool NkQ4KGpuMatmulCpuEx(const NkQ4KGpuWeight &w, const float32 *x, int64 M, NkQ4KMatmulKernel kernel,
									 NkVector<float32> &outY, NkString *outError) {
				outY.Clear();
				if (!w.IsValid() || !x || M <= 0) {
					SetErr(outError, "NkQ4KGpuMatmulCpu : arguments invalides");
					return false;
				}
				NkTensorGpu &gpu = NkTensorGpu::Get();
				const uint64 xn = (uint64)M * (uint64)w.cols;
				const uint64 yn = (uint64)M * (uint64)w.rows;
				uint64 xbuf = gpu.CreateBuffer((nk_size)(xn * sizeof(float32)));
				uint64 ybuf = gpu.CreateBuffer((nk_size)(yn * sizeof(float32)));
				bool ok = (xbuf != 0 && ybuf != 0);
				if (!ok)
					SetErr(outError, "NkQ4KGpuMatmulCpu : CreateBuffer a échoué");
				if (ok) {
					ok = gpu.Upload(xbuf, x, (nk_size)(xn * sizeof(float32)));
					if (!ok)
						SetErr(outError, "NkQ4KGpuMatmulCpu : Upload de X a échoué");
				}
				if (ok)
					ok = NkQ4KGpuMatmulEx(w, xbuf, ybuf, M, kernel, outError);
				if (ok) {
					outY.Resize((NkVector<float32>::SizeType)yn);
					ok = gpu.Download(ybuf, outY.Data(), (nk_size)(yn * sizeof(float32)));
					if (!ok)
						SetErr(outError, "NkQ4KGpuMatmulCpu : Download de Y a échoué");
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
