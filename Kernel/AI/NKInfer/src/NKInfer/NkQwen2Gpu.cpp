// =============================================================================
// NkQwen2Gpu.cpp — forward complet du 7B sur GPU. Voir NkQwen2Gpu.h pour la
// portée et les choix de conception.
//
// RÈGLE QUI GOUVERNE TOUT CE FICHIER : chaque noyau ci-dessous reproduit une
// fonction PRÉCISE de NkQwen2Block.cpp (la vérité CPU), dans le MÊME ORDRE de
// sommation. Ce n'est pas du zèle : si l'ordre différait, l'écart mesuré par le
// test mélangerait l'erreur du noyau et celle du réordonnancement, et on ne
// saurait pas laquelle on observe. Les seules libertés prises sont écrites en
// commentaire à l'endroit exact où elles sont prises.
//
// Bindings : on n'utilise QUE les deux lanceurs génériques déjà publics de
// NkTensorGpu — RunConvOp (2 storage buffers 0/1 + UBO 12 uints au binding 2)
// et RunOp3 (3 buffers 0/1/2 + UBO au binding 3), dispatch 1D local_size_x=64.
// Aucune ligne de NKTensor n'est modifiée (même méthode qu'au jalon 4).
// =============================================================================
#include "NKInfer/NkQwen2Gpu.h"
#include "NKInfer/NkGGUFDequant.h"
#include "NKInfer/NkSampling.h"
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

				// Bits d'un float passés dans l'UBO d'uints. Le shader fait
				// l'opération inverse avec uintBitsToFloat — le même aller-retour
				// que le décodeur fp16 du jalon 4, et pour la même raison : les
				// lanceurs génériques ne transportent que des uints, et on ne
				// modifie pas NkTensorGpu pour un jalon additif.
				// Horloge : std::clock() et PAS NkChrono, parce que NKInfer ne
				// dépend pas de NKTime — et on n'ajoute pas une dépendance à un
				// module existant pour un jalon additif (même règle qu'au jalon 3,
				// où l'Adam a été ré-implémenté plutôt que de tirer NKOptim).
				// Sur Windows, std::clock() est du temps MURAL, ce qui est bien ce
				// qu'on veut mesurer ici (les attentes GPU comptent). Les mesures
				// PUBLIÉES du jalon viennent du test, qui chronomètre avec NkChrono.
				float64 NowSeconds() {
					return (float64)std::clock() / (float64)CLOCKS_PER_SEC;
				}

				uint32 FBits(float32 f) {
					uint32 u = 0;
					std::memcpy(&u, &f, sizeof(u));
					return u;
				}

				// -------------------------------------------------------------
				// RMSNorm — transcription de NkRMSNorm (NkQwen2Block.cpp:134).
				//
				// UNE LIBERTÉ ASSUMÉE : le CPU accumule la somme des carrés en
				// DOUBLE ; le GPU n'a pas de double fiable sur tous les backends.
				// On accumule donc par PAQUETS DE 64 puis on somme les paquets :
				// l'erreur passe de ~sqrt(3584)·eps à ~sqrt(64)·eps + sqrt(56)·eps,
				// soit ~8× moins, sans compensation de Kahan (qu'un optimiseur
				// pourrait légitimement supprimer par réassociation). C'est la
				// seule divergence d'ORDRE de somme de tout ce fichier, et elle va
				// dans le sens de la précision.
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

				// Biais de projection : y = x + b[col]. Transcription de
				// ops::Add(y, b) avec broadcast sur le dernier axe (Linear,
				// NkQwen2Block.cpp:66).
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

				// -------------------------------------------------------------
				// RoPE style NEOX (paires (i, i+headDim/2)) — transcription de
				// NkApplyRoPE (NkQwen2Block.cpp:189), à ceci près que cos/sin ne
				// sont PAS recalculés ici : ils sont lus d'une table construite en
				// DOUBLE au chargement (cf en-tête de NkQwen2Gpu.h, point 2). Le
				// shader ne fait donc que la rotation elle-même — exactement les
				// quatre multiplications et deux additions du CPU.
				//
				// Disposition d'entrée [T, H*headDim] (une ligne par token), qui
				// est celle produite par les projections : on évite ainsi la
				// permutation [T,H,hd] -> [H,T,hd] que le CPU matérialise. Cela ne
				// change AUCUN calcul, seulement l'indexation.
				// -------------------------------------------------------------
				const char *kRope = R"NKSL(
@binding(set=0, binding=0) buffer BufX { float data[]; } X;
@binding(set=0, binding=1) buffer BufT { float data[]; } Tab;
@binding(set=0, binding=2) buffer BufY { float data[]; } Y;
@binding(set=0, binding=3) uniform Params {
    uint total; uint nH; uint headDim; uint pos0;
    uint halfd; uint maxSeq; uint p6; uint p7;
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
        uint pos = pc.pos0 + t;
        float c = Tab.data[pos * pc.halfd + i];
        float s = Tab.data[pc.maxSeq * pc.halfd + pos * pc.halfd + i];
        float x1 = X.data[base + i];
        float x2 = X.data[base + i + pc.halfd];
        Y.data[base + i] = x1 * c - x2 * s;
        Y.data[base + i + pc.halfd] = x1 * s + x2 * c;
    }
}
)NKSL";

				// Écriture des nouvelles clés/valeurs dans le cache résident.
				// Source [T, nKV*headDim] -> cache [nKV, maxSeq, headDim].
				// Équivalent de NkKVCacheAppend, mais SANS réallocation : le cache
				// est dimensionné une fois pour maxSeqLen, et on n'écrit que la
				// tranche [pos0, pos0+T).
				const char *kKvAppend = R"NKSL(
@binding(set=0, binding=0) buffer BufS { float data[]; } S;
@binding(set=0, binding=1) buffer BufC { float data[]; } C;
@binding(set=0, binding=2) uniform Params {
    uint total; uint nKV; uint headDim; uint maxSeq;
    uint pos0; uint p5; uint p6; uint p7;
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
        uint t = rest / pc.nKV;
        uint src = t * pc.nKV * pc.headDim + kvh * pc.headDim + dh;
        uint dst = kvh * pc.maxSeq * pc.headDim + (pc.pos0 + t) * pc.headDim + dh;
        C.data[dst] = S.data[src];
    }
}
)NKSL";

				// Scores d'attention GQA : s[h,t,j] = (q_h,t · k_kvh,j) / sqrt(hd).
				// kvh = h / (nH/nKV) — le mapping EXACT de NkQwen2LayerForward
				// (NkQwen2Block.cpp:275). Les positions futures reçoivent 0 (elles
				// ne sont jamais lues : le softmax ci-dessous s'arrête à `pos`).
				// L'accumulation est en float SIMPLE, comme CpuMatmul2D — ici on ne
				// « fait pas mieux » que la référence exprès : on la reproduit.
				const char *kScores = R"NKSL(
@binding(set=0, binding=0) buffer BufQ { float data[]; } Q;
@binding(set=0, binding=1) buffer BufK { float data[]; } Kc;
@binding(set=0, binding=2) buffer BufS { float data[]; } S;
@binding(set=0, binding=3) uniform Params {
    uint total; uint T; uint nH; uint headDim;
    uint maxSeq; uint Ttot; uint pos0; uint group;
    uint scaleBits; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.total) {
        uint j = idx % pc.Ttot;
        uint rest = idx / pc.Ttot;
        uint t = rest % pc.T;
        uint h = rest / pc.T;
        float acc = 0.0;
        if (j <= pc.pos0 + t) {
            uint kvh = h / pc.group;
            uint qb = t * pc.nH * pc.headDim + h * pc.headDim;
            uint kb = kvh * pc.maxSeq * pc.headDim + j * pc.headDim;
            for (uint d = 0u; d < pc.headDim; d = d + 1u) {
                acc = acc + Q.data[qb + d] * Kc.data[kb + d];
            }
            acc = acc * uintBitsToFloat(pc.scaleBits);
        }
        S.data[idx] = acc;
    }
}
)NKSL";

				// Softmax causal par ligne — transcription de la boucle de masque
				// + softmax de NkQwen2LayerForward (NkQwen2Block.cpp:289-309) :
				// maximum sur les colonnes AUTORISÉES seulement, exponentielle,
				// normalisation, puis zéro strict au-delà (le futur ne doit pas
				// seulement être petit, il doit être NUL — sinon il contribuerait
				// au produit avec V).
				const char *kSoftmax = R"NKSL(
@binding(set=0, binding=0) buffer BufS { float data[]; } S;
@binding(set=0, binding=1) buffer BufP { float data[]; } P;
@binding(set=0, binding=2) uniform Params {
    uint rows; uint T; uint Ttot; uint pos0;
    uint p4; uint p5; uint p6; uint p7;
    uint p8; uint p9; uint p10; uint p11;
} pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint r = gl_GlobalInvocationID.x;
    if (r < pc.rows) {
        uint base = r * pc.Ttot;
        uint t = r % pc.T;
        uint last = pc.pos0 + t;
        if (last >= pc.Ttot) {
            last = pc.Ttot - 1u;
        }
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
        for (uint c = last + 1u; c < pc.Ttot; c = c + 1u) {
            P.data[base + c] = 0.0;
        }
    }
}
)NKSL";

				// Contexte : out[t, h*hd + dh] = Σ_j p[h,t,j] · V[kvh, j, dh].
				// Sortie DIRECTEMENT en disposition [T, nH*headDim] : c'est la
				// permutation + reshape que le CPU matérialise (NkQwen2Block.cpp:318)
				// obtenue gratuitement par le choix des indices, pas par une copie.
				const char *kAttnOut = R"NKSL(
@binding(set=0, binding=0) buffer BufP { float data[]; } P;
@binding(set=0, binding=1) buffer BufV { float data[]; } Vc;
@binding(set=0, binding=2) buffer BufO { float data[]; } O;
@binding(set=0, binding=3) uniform Params {
    uint total; uint T; uint nH; uint headDim;
    uint maxSeq; uint Ttot; uint group; uint p7;
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
        uint pb = (h * pc.T + t) * pc.Ttot;
        uint vb = kvh * pc.maxSeq * pc.headDim + dh;
        float acc = 0.0;
        for (uint j = 0u; j < pc.Ttot; j = j + 1u) {
            acc = acc + P.data[pb + j] * Vc.data[vb + j * pc.headDim];
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

				// SwiGLU : silu(gate) ⊙ up — transcription de NkSiLU + ops::Mul
				// (NkQwen2Block.cpp:172, :330-334).
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

				// Extraction d'une ligne (la dernière position) vers un tampon à
				// part : le matmul du jalon 4 lit X à partir de l'index 0, il faut
				// donc que la ligne visée y soit. Une copie de 14 Ko contre le
				// calcul de 152 064 logits inutiles.
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
				// locales) : NkTensorGpu met le PIPELINE en cache par nom, mais le
				// forward appelle ces noyaux ~340 fois par token — autant ne pas
				// reconstruire dix chaînes de plusieurs kilo-octets à chaque appel.
				// Même motif qu'au jalon 4 (NkQ4KGpu.cpp::MatmulSource).
				const NkString &SrcRmsNorm() {
					static NkString s(kRmsNorm);
					return s;
				}
				const NkString &SrcAddBias() {
					static NkString s(kAddBias);
					return s;
				}
				const NkString &SrcRope() {
					static NkString s(kRope);
					return s;
				}
				const NkString &SrcKvAppend() {
					static NkString s(kKvAppend);
					return s;
				}
				const NkString &SrcScores() {
					static NkString s(kScores);
					return s;
				}
				const NkString &SrcSoftmax() {
					static NkString s(kSoftmax);
					return s;
				}
				const NkString &SrcAttnOut() {
					static NkString s(kAttnOut);
					return s;
				}
				const NkString &SrcAdd() {
					static NkString s(kAdd);
					return s;
				}
				const NkString &SrcSwiGlu() {
					static NkString s(kSwiGlu);
					return s;
				}
				const NkString &SrcCopyRow() {
					static NkString s(kCopyRow);
					return s;
				}

			} // namespace

			// =============================================================
			// Cycle de vie
			// =============================================================
			NkQwen2Gpu::~NkQwen2Gpu() {
				Release();
			}

			const char *NkQwen2Gpu::BackendName() const {
				return NkTensorGpu::Get().BackendName();
			}

			uint64 NkQwen2Gpu::NewBuffer(uint64 bytes, uint64 &accumulator) {
				// Arrondi à 16 octets comme les tampons du jalon 4 : certains
				// backends exigent un multiple de 16 pour un storage buffer.
				const uint64 alloc = (bytes + 15ull) & ~15ull;
				uint64 id = NkTensorGpu::Get().CreateBuffer((nk_size)alloc);
				if (id != 0) {
					accumulator += alloc;
					mStats.bufferCount++;
				}
				return id;
			}

			void NkQwen2Gpu::Release() {
				NkTensorGpu &gpu = NkTensorGpu::Get();
				auto kill = [&gpu](uint64 &b) {
					if (b != 0) {
						gpu.DestroyBuffer(b);
						b = 0;
					}
				};
				for (uint32 i = 0; i < mLayers.Size(); ++i) {
					NkQwen2GpuLayer &L = mLayers[i];
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
					kill(L.kCache);
					kill(L.vCache);
				}
				mLayers.Clear();
				if (mLmHead.kind == NkQwen2GpuQuant::NK_Q4K)
					NkQ4KGpuRelease(mLmHead.q4);
				else if (mLmHead.kind == NkQwen2GpuQuant::NK_Q6K)
					NkQ6KGpuRelease(mLmHead.q6);
				mLmHead.kind = NkQwen2GpuQuant::NK_NONE;
				kill(mOutputNorm);
				uint64 *scratch[] = {&mBufX,	 &mBufX1,	 &mBufXn,	&mBufAttn, &mBufDown,	&mBufQ,
									 &mBufQb,	 &mBufQr,	 &mBufCtx,	&mBufK,	   &mBufKb,		&mBufKr,
									 &mBufV,	 &mBufVb,	 &mBufScores, &mBufProbs, &mBufGate, &mBufUp,
									 &mBufAct, &mBufLast, &mBufLogits, &mBufRope};
				for (uint32 i = 0; i < sizeof(scratch) / sizeof(scratch[0]); ++i)
					kill(*scratch[i]);
				mLoaded = false;
				mCacheLen = 0;
				mEmbedInfo = nullptr;
			}

			const NkGGUFTensorInfo *NkQwen2Gpu::Find(const char *name) const {
				for (uint32 i = 0; i < mGguf.tensors.Size(); ++i)
					if (mGguf.tensors[i].name.Compare(name) == 0)
						return &mGguf.tensors[i];
				return nullptr;
			}

			// =============================================================
			// Chargement
			// =============================================================
			bool NkQwen2Gpu::UploadQuant(const NkGGUFTensorInfo &info, NkQwen2GpuWeight &out, NkString *err) {
				out = NkQwen2GpuWeight{};
				if (info.dims.Size() != 2) {
					SetErr(err, "UploadQuant : un poids de projection doit avoir 2 dimensions");
					return false;
				}
				// Convention ggml : ne[0] = in_features (contigu), ne[1] = out.
				const int64 cols = (int64)info.dims[0];
				const int64 rows = (int64)info.dims[1];

				NkVector<uint8> raw;
				const float64 readT0 = NowSeconds();
				if (!NkGGUFReadTensorRawBytes(mPath.CStr(), mGguf, info, raw, err))
					return false;
				mStats.readSeconds += NowSeconds() - readT0;

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
					// REFUS EXPLICITE, jamais un résultat silencieusement faux :
					// ce chemin n'a de noyau que pour Q4_K et Q6_K. Un blob avec
					// des Q5_K doit s'arrêter ici, pas produire du charabia.
					SetErr(err, "UploadQuant : format de poids non supporté par le chemin GPU (Q4_K/Q6_K seulement)");
					return false;
				}
				mStats.bufferCount++;
				return true;
			}

			bool NkQwen2Gpu::UploadF32(const NkGGUFTensorInfo &info, uint64 &outBuffer, int64 expectedNumel,
									   NkString *err) {
				NkVector<float32> data;
				NkVector<uint8> raw;
				const float64 readT0 = NowSeconds();
				if (!NkGGUFReadTensorRawBytes(mPath.CStr(), mGguf, info, raw, err))
					return false;
				mStats.readSeconds += NowSeconds() - readT0;
				if (!NkGGUFDequantizeRaw(info.rawType, raw.Data(), (uint64)raw.Size(), info.numElements, data, err))
					return false;
				if (expectedNumel > 0 && (int64)data.Size() != expectedNumel) {
					SetErr(err, "UploadF32 : nombre d'éléments inattendu pour ce tenseur");
					return false;
				}
				outBuffer = NewBuffer((uint64)data.Size() * sizeof(float32), mStats.weightBytes);
				if (outBuffer == 0) {
					SetErr(err, "UploadF32 : CreateBuffer a échoué");
					return false;
				}
				if (!NkTensorGpu::Get().Upload(outBuffer, data.Data(), (nk_size)((uint64)data.Size() * sizeof(float32)))) {
					SetErr(err, "UploadF32 : Upload a échoué");
					return false;
				}
				mStats.f32Tensors++;
				return true;
			}

			void NkQwen2Gpu::BuildRopeTables() {
				// Formule EXACTE de NkApplyRoPE (NkQwen2Block.cpp:204), calculée en
				// double comme elle : theta = pos · freqBase^(-2i/headDim).
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

			bool NkQwen2Gpu::AllocScratch(NkString *err) {
				const int64 d = mCfg.dModel;
				const int64 qd = (int64)mCfg.nHeads * mCfg.headDim;
				const int64 kvd = (int64)mCfg.nKVHeads * mCfg.headDim;
				const int64 ffn = mCfg.ffnDim;
				const int64 mT = mOpt.maxBatchTokens;
				const int64 mS = mOpt.maxSeqLen;
				const uint64 F = sizeof(float32);

				struct {
						uint64 *slot;
						uint64 elems;
				} req[] = {
					{&mBufX, (uint64)(mT * d)},		 {&mBufX1, (uint64)(mT * d)},
					{&mBufXn, (uint64)(mT * d)},	 {&mBufAttn, (uint64)(mT * d)},
					{&mBufDown, (uint64)(mT * d)},	 {&mBufQ, (uint64)(mT * qd)},
					{&mBufQb, (uint64)(mT * qd)},	 {&mBufQr, (uint64)(mT * qd)},
					{&mBufCtx, (uint64)(mT * qd)},	 {&mBufK, (uint64)(mT * kvd)},
					{&mBufKb, (uint64)(mT * kvd)},	 {&mBufKr, (uint64)(mT * kvd)},
					{&mBufV, (uint64)(mT * kvd)},	 {&mBufVb, (uint64)(mT * kvd)},
					{&mBufScores, (uint64)((int64)mCfg.nHeads * mT * mS)},
					{&mBufProbs, (uint64)((int64)mCfg.nHeads * mT * mS)},
					{&mBufGate, (uint64)(mT * ffn)}, {&mBufUp, (uint64)(mT * ffn)},
					{&mBufAct, (uint64)(mT * ffn)},	 {&mBufLast, (uint64)d},
					{&mBufLogits, (uint64)mVocabSize},
					{&mBufRope, (uint64)(mS * (mCfg.headDim / 2) * 2)},
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

			bool NkQwen2Gpu::Load(const char *ggufPath, const NkQwen2GpuOptions &opt, NkString *err) {
				Release();
				mStats = NkQwen2GpuStats{};
				mOpt = opt;
				mPath = NkString(ggufPath);
				const float64 loadT0 = NowSeconds();

				NkTensorGpu &gpu = NkTensorGpu::Get();
				if (!gpu.IsAvailable()) {
					SetErr(err, "NkQwen2Gpu::Load : aucun device compute GPU disponible");
					return false;
				}

				if (!NkGGUFLoader::Load(ggufPath, mGguf) || !mGguf.valid) {
					SetErr(err, "NkQwen2Gpu::Load : GGUF illisible");
					return false;
				}

				// ---- hyperparamètres RÉELS (aucune valeur supposée) ----------
				uint64 blockCount = 0, dModel = 0, ffnDim = 0, headCount = 0, headCountKv = 0;
				float64 ropeFreqBase = 0.0, rmsEps = 0.0;
				bool okMeta = NkGGUFGetUInt(mGguf, "qwen2.block_count", blockCount) &&
							  NkGGUFGetUInt(mGguf, "qwen2.embedding_length", dModel) &&
							  NkGGUFGetUInt(mGguf, "qwen2.feed_forward_length", ffnDim) &&
							  NkGGUFGetUInt(mGguf, "qwen2.attention.head_count", headCount) &&
							  NkGGUFGetUInt(mGguf, "qwen2.attention.head_count_kv", headCountKv) &&
							  NkGGUFGetFloat(mGguf, "qwen2.rope.freq_base", ropeFreqBase) &&
							  NkGGUFGetFloat(mGguf, "qwen2.attention.layer_norm_rms_epsilon", rmsEps);
				if (!okMeta) {
					SetErr(err, "NkQwen2Gpu::Load : métadonnées qwen2.* incomplètes");
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
					SetErr(err, "NkQwen2Gpu::Load : NkQwen2Config incohérente");
					return false;
				}

				mEmbedInfo = Find("token_embd.weight");
				if (!mEmbedInfo || mEmbedInfo->dims.Size() != 2 || !mEmbedInfo->sizeKnown) {
					SetErr(err, "NkQwen2Gpu::Load : token_embd.weight absent ou non dimensionné");
					return false;
				}
				mVocabSize = (int64)mEmbedInfo->dims[1];
				mEmbedBytesPerRow = mEmbedInfo->sizeBytes / (uint64)mVocabSize;

				uint32 nLayers = (uint32)blockCount;
				if (mOpt.maxLayers > 0 && mOpt.maxLayers < nLayers)
					nLayers = mOpt.maxLayers;

				if (mOpt.verbose) {
					printf("  [NkQwen2Gpu] backend = %s\n", gpu.BackendName());
					printf("  [NkQwen2Gpu] L=%u d=%d ffn=%d nH=%d nKV=%d headDim=%d vocab=%lld theta=%g eps=%g\n", nLayers,
						   mCfg.dModel, mCfg.ffnDim, mCfg.nHeads, mCfg.nKVHeads, mCfg.headDim, (long long)mVocabSize,
						   (double)mCfg.ropeFreqBase, (double)mCfg.rmsEps);
				}

				// ---- tampons d'activation + tables RoPE ---------------------
				if (!AllocScratch(err))
					return false;
				BuildRopeTables();

				// ---- poids, couche par couche, BRUTS ------------------------
				const int64 d = mCfg.dModel;
				const int64 qd = (int64)mCfg.nHeads * mCfg.headDim;
				const int64 kvd = (int64)mCfg.nKVHeads * mCfg.headDim;
				mLayers.Resize((NkVector<NkQwen2GpuLayer>::SizeType)nLayers);
				char name[160];
				for (uint32 l = 0; l < nLayers; ++l) {
					NkQwen2GpuLayer &L = mLayers[l];
					struct {
							const char *suffix;
							NkQwen2GpuWeight *dst;
					} quant[] = {
						{"attn_q.weight", &L.wq},		{"attn_k.weight", &L.wk},	 {"attn_v.weight", &L.wv},
						{"attn_output.weight", &L.wo},	{"ffn_gate.weight", &L.wGate}, {"ffn_up.weight", &L.wUp},
						{"ffn_down.weight", &L.wDown},
					};
					for (uint32 i = 0; i < sizeof(quant) / sizeof(quant[0]); ++i) {
						std::snprintf(name, sizeof(name), "blk.%u.%s", l, quant[i].suffix);
						const NkGGUFTensorInfo *info = Find(name);
						if (!info) {
							SetErr(err, "NkQwen2Gpu::Load : tenseur de couche introuvable");
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
							SetErr(err, "NkQwen2Gpu::Load : norme/biais de couche introuvable");
							return false;
						}
						if (!UploadF32(*info, *plain[i].dst, plain[i].numel, err))
							return false;
					}
					// KV-cache résident de la couche.
					const uint64 kvElems = (uint64)mCfg.nKVHeads * (uint64)mOpt.maxSeqLen * (uint64)mCfg.headDim;
					L.kCache = NewBuffer(kvElems * sizeof(float32), mStats.kvBytes);
					L.vCache = NewBuffer(kvElems * sizeof(float32), mStats.kvBytes);
					if (L.kCache == 0 || L.vCache == 0) {
						SetErr(err, "NkQwen2Gpu::Load : allocation du KV-cache a échoué");
						return false;
					}
					if (mOpt.verbose && (l % 7 == 0 || l + 1 == nLayers))
						printf("    couche %u/%u chargée (%.1f s, %.2f Go en VRAM)\n", l + 1, nLayers,
							   NowSeconds() - loadT0, (double)mStats.TotalBytes() / 1073741824.0);
				}

				// ---- norme finale + lm_head ---------------------------------
				{
					const NkGGUFTensorInfo *on = Find("output_norm.weight");
					if (!on || !UploadF32(*on, mOutputNorm, d, err)) {
						SetErr(err, "NkQwen2Gpu::Load : output_norm.weight absent ou illisible");
						return false;
					}
				}
				{
					const NkGGUFTensorInfo *out = Find("output.weight");
					mTied = (out == nullptr);
					// EMBEDDINGS LIÉES : pas de `output.weight` -> c'est
					// `token_embd.weight` qui sert de lm_head, et il DOIT alors
					// monter en VRAM (306 Mo). Cas géré, mais ce blob-ci a bien un
					// output.weight distinct : on économise ces 306 Mo.
					const NkGGUFTensorInfo *src = mTied ? mEmbedInfo : out;
					if (!UploadQuant(*src, mLmHead, err))
						return false;
					if (mOpt.verbose)
						printf("    lm_head = %s (%s, %lld x %lld, %.0f Mo)\n", mTied ? "token_embd.weight (lié)" : "output.weight",
							   NkGGUFTensorTypeName(src->rawType), (long long)mLmHead.Rows(), (long long)mLmHead.Cols(),
							   (double)mLmHead.Bytes() / 1048576.0);
				}

				mCacheLen = 0;
				mLoaded = true;
				mStats.loadSeconds = NowSeconds() - loadT0;
				return true;
			}

			// =============================================================
			// Embeddings (lecture de LIGNES — cf en-tête du header)
			// =============================================================
			bool NkQwen2Gpu::EmbedTokens(const int32 *ids, int32 count, NkTensor &out, NkString *err) {
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
						SetErr(err, "EmbedTokens : identifiant de token hors vocabulaire");
						return false;
					}
					const uint64 off =
						mGguf.tensorDataOffset + mEmbedInfo->offset + (uint64)tok * mEmbedBytesPerRow;
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
			// Forward
			// =============================================================
			bool NkQwen2Gpu::Matmul(const NkQwen2GpuWeight &w, uint64 xBuf, uint64 yBuf, int64 M, NkString *err) {
				if (w.kind == NkQwen2GpuQuant::NK_Q4K)
					return NkQ4KGpuMatmul(w.q4, xBuf, yBuf, M, err);
				if (w.kind == NkQwen2GpuQuant::NK_Q6K)
					return NkQ6KGpuMatmul(w.q6, xBuf, yBuf, M, err);
				SetErr(err, "Matmul : poids non résident");
				return false;
			}

			void NkQwen2Gpu::ResetCache() {
				mCacheLen = 0;
			}

			bool NkQwen2Gpu::Forward(const int32 *ids, int32 count, NkTensor &outLogits, NkQwen2GpuTrace *trace,
									 NkString *err) {
				if (!mLoaded) {
					SetErr(err, "Forward : modèle non chargé");
					return false;
				}
				if (count <= 0 || count > mOpt.maxBatchTokens) {
					SetErr(err, "Forward : count doit être dans [1, maxBatchTokens]");
					return false;
				}
				if (mCacheLen + count > mOpt.maxSeqLen) {
					SetErr(err, "Forward : maxSeqLen dépassé (agrandir NkQwen2GpuOptions::maxSeqLen)");
					return false;
				}
				return ForwardChunk(ids, count, &outLogits, trace, err);
			}

			bool NkQwen2Gpu::ForwardChunk(const int32 *ids, int32 T32, NkTensor *outLogits, NkQwen2GpuTrace *trace,
										  NkString *err) {
				NkTensorGpu &gpu = NkTensorGpu::Get();
				const int64 T = T32;
				const int64 d = mCfg.dModel;
				const int64 hd = mCfg.headDim;
				const int64 nH = mCfg.nHeads;
				const int64 nKV = mCfg.nKVHeads;
				const int64 qd = nH * hd;
				const int64 kvd = nKV * hd;
				const int64 ffn = mCfg.ffnDim;
				const int64 half = hd / 2;
				const int64 group = nH / nKV;
				const int32 pos0 = mCacheLen;
				const int64 Ttot = (int64)pos0 + T;
				const uint32 eps = FBits(mCfg.rmsEps);
				const uint32 scale = FBits((float32)(1.0 / std::sqrt((double)hd)));

				// ---- entrée : lignes d'embedding -> VRAM --------------------
				NkTensor emb;
				if (!EmbedTokens(ids, T32, emb, err))
					return false;
				if (!gpu.Upload(mBufX, emb.DataAs<float>(), (nk_size)(T * d * (int64)sizeof(float32)))) {
					SetErr(err, "ForwardChunk : upload des embeddings a échoué");
					return false;
				}

				uint32 p[12];
				auto zero = [&p]() {
					for (int i = 0; i < 12; ++i)
						p[i] = 0;
				};
				bool ok = true;

				for (uint32 l = 0; l < mLayers.Size() && ok; ++l) {
					const NkQwen2GpuLayer &L = mLayers[l];

					// --- pré-norme d'attention ---
					zero();
					p[0] = (uint32)T;
					p[1] = (uint32)d;
					p[2] = eps;
					ok = gpu.RunOp3("qwen2_rmsnorm", SrcRmsNorm(), mBufX, L.attnNorm, mBufXn, p, (uint32)T);

					// --- projections Q/K/V + biais ---
					ok = ok && Matmul(L.wq, mBufXn, mBufQ, T, err);
					ok = ok && Matmul(L.wk, mBufXn, mBufK, T, err);
					ok = ok && Matmul(L.wv, mBufXn, mBufV, T, err);
					if (ok) {
						zero();
						p[0] = (uint32)(T * qd);
						p[1] = (uint32)qd;
						ok = gpu.RunOp3("qwen2_addbias", SrcAddBias(), mBufQ, L.bq, mBufQb, p, p[0]);
					}
					if (ok) {
						zero();
						p[0] = (uint32)(T * kvd);
						p[1] = (uint32)kvd;
						ok = gpu.RunOp3("qwen2_addbias", SrcAddBias(), mBufK, L.bk, mBufKb, p, p[0]);
					}
					if (ok) {
						zero();
						p[0] = (uint32)(T * kvd);
						p[1] = (uint32)kvd;
						ok = gpu.RunOp3("qwen2_addbias", SrcAddBias(), mBufV, L.bv, mBufVb, p, p[0]);
					}

					// --- RoPE sur Q et K (PAS sur V : ce sont des valeurs) ---
					if (ok) {
						zero();
						p[0] = (uint32)(T * nH * half);
						p[1] = (uint32)nH;
						p[2] = (uint32)hd;
						p[3] = (uint32)pos0;
						p[4] = (uint32)half;
						p[5] = (uint32)mOpt.maxSeqLen;
						ok = gpu.RunOp3("qwen2_rope", SrcRope(), mBufQb, mBufRope, mBufQr, p, p[0]);
					}
					if (ok) {
						zero();
						p[0] = (uint32)(T * nKV * half);
						p[1] = (uint32)nKV;
						p[2] = (uint32)hd;
						p[3] = (uint32)pos0;
						p[4] = (uint32)half;
						p[5] = (uint32)mOpt.maxSeqLen;
						ok = gpu.RunOp3("qwen2_rope", SrcRope(), mBufKb, mBufRope, mBufKr, p, p[0]);
					}

					// --- extension du KV-cache résident ---
					if (ok) {
						zero();
						p[0] = (uint32)(T * kvd);
						p[1] = (uint32)nKV;
						p[2] = (uint32)hd;
						p[3] = (uint32)mOpt.maxSeqLen;
						p[4] = (uint32)pos0;
						ok = gpu.RunConvOp("qwen2_kvappend", SrcKvAppend(), mBufKr, L.kCache, p, p[0]);
					}
					if (ok) {
						zero();
						p[0] = (uint32)(T * kvd);
						p[1] = (uint32)nKV;
						p[2] = (uint32)hd;
						p[3] = (uint32)mOpt.maxSeqLen;
						p[4] = (uint32)pos0;
						ok = gpu.RunConvOp("qwen2_kvappend", SrcKvAppend(), mBufVb, L.vCache, p, p[0]);
					}

					// --- attention GQA causale ---
					if (ok) {
						zero();
						p[0] = (uint32)(nH * T * Ttot);
						p[1] = (uint32)T;
						p[2] = (uint32)nH;
						p[3] = (uint32)hd;
						p[4] = (uint32)mOpt.maxSeqLen;
						p[5] = (uint32)Ttot;
						p[6] = (uint32)pos0;
						p[7] = (uint32)group;
						p[8] = scale;
						ok = gpu.RunOp3("qwen2_scores", SrcScores(), mBufQr, L.kCache, mBufScores, p, p[0]);
					}
					if (ok) {
						zero();
						p[0] = (uint32)(nH * T);
						p[1] = (uint32)T;
						p[2] = (uint32)Ttot;
						p[3] = (uint32)pos0;
						ok = gpu.RunConvOp("qwen2_softmax", SrcSoftmax(), mBufScores, mBufProbs, p, p[0]);
					}
					if (ok) {
						zero();
						p[0] = (uint32)(T * qd);
						p[1] = (uint32)T;
						p[2] = (uint32)nH;
						p[3] = (uint32)hd;
						p[4] = (uint32)mOpt.maxSeqLen;
						p[5] = (uint32)Ttot;
						p[6] = (uint32)group;
						ok = gpu.RunOp3("qwen2_attnout", SrcAttnOut(), mBufProbs, L.vCache, mBufCtx, p, p[0]);
					}

					// --- projection de sortie + résiduel ---
					ok = ok && Matmul(L.wo, mBufCtx, mBufAttn, T, err);
					if (ok) {
						zero();
						p[0] = (uint32)(T * d);
						ok = gpu.RunOp3("qwen2_add", SrcAdd(), mBufX, mBufAttn, mBufX1, p, p[0]);
					}

					// --- MLP SwiGLU (pré-norme) + résiduel ---
					if (ok) {
						zero();
						p[0] = (uint32)T;
						p[1] = (uint32)d;
						p[2] = eps;
						ok = gpu.RunOp3("qwen2_rmsnorm", SrcRmsNorm(), mBufX1, L.ffnNorm, mBufXn, p, (uint32)T);
					}
					ok = ok && Matmul(L.wGate, mBufXn, mBufGate, T, err);
					ok = ok && Matmul(L.wUp, mBufXn, mBufUp, T, err);
					if (ok) {
						zero();
						p[0] = (uint32)(T * ffn);
						ok = gpu.RunOp3("qwen2_swiglu", SrcSwiGlu(), mBufGate, mBufUp, mBufAct, p, p[0]);
					}
					ok = ok && Matmul(L.wDown, mBufAct, mBufDown, T, err);
					if (ok) {
						zero();
						p[0] = (uint32)(T * d);
						ok = gpu.RunOp3("qwen2_add", SrcAdd(), mBufX1, mBufDown, mBufX, p, p[0]);
					}

					// --- capture éventuelle de l'état de sortie de la couche ---
					if (ok && trace) {
						for (uint32 c = 0; c < trace->layers.Size(); ++c) {
							if (trace->layers[c] != (int32)l)
								continue;
							NkTensor h = NkTensor::Zeros(NkShape{T, d});
							if (!gpu.Download(mBufX, h.DataAs<float>(), (nk_size)(T * d * (int64)sizeof(float32)))) {
								SetErr(err, "ForwardChunk : relecture de l'état caché a échoué");
								return false;
							}
							if (trace->hidden.Size() <= c)
								trace->hidden.Resize(trace->layers.Size());
							trace->hidden[c] = h;
						}
					}
					if (!ok) {
						if (err && err->Size() == 0)
							SetErr(err, "ForwardChunk : un dispatch de noyau a échoué");
						return false;
					}
				}

				// ---- norme finale ------------------------------------------
				zero();
				p[0] = (uint32)T;
				p[1] = (uint32)d;
				p[2] = eps;
				if (!gpu.RunOp3("qwen2_rmsnorm", SrcRmsNorm(), mBufX, mOutputNorm, mBufXn, p, (uint32)T)) {
					SetErr(err, "ForwardChunk : RMSNorm finale a échoué");
					return false;
				}
				if (trace && trace->captureFinalNorm) {
					trace->finalNorm = NkTensor::Zeros(NkShape{T, d});
					gpu.Download(mBufXn, trace->finalNorm.DataAs<float>(),
								 (nk_size)(T * d * (int64)sizeof(float32)));
				}

				mCacheLen = (int32)Ttot;

				// ---- lm_head sur la DERNIÈRE position uniquement ------------
				if (outLogits) {
					zero();
					p[0] = (uint32)d;
					p[1] = (uint32)((T - 1) * d);
					if (!gpu.RunConvOp("qwen2_copyrow", SrcCopyRow(), mBufXn, mBufLast, p, (uint32)d)) {
						SetErr(err, "ForwardChunk : extraction de la dernière ligne a échoué");
						return false;
					}
					if (!Matmul(mLmHead, mBufLast, mBufLogits, 1, err))
						return false;
					*outLogits = NkTensor::Zeros(NkShape{1, mVocabSize});
					if (!gpu.Download(mBufLogits, outLogits->DataAs<float>(),
									  (nk_size)(mVocabSize * (int64)sizeof(float32)))) {
						SetErr(err, "ForwardChunk : relecture des logits a échoué");
						return false;
					}
				}
				return true;
			}

			// =============================================================
			// Génération
			// =============================================================
			bool NkQwen2Gpu::Generate(const NkVector<int32> &promptIds, int32 nNew, float32 temperature, int32 topK,
									  uint32 &rngState, int32 stopId, NkVector<int32> &outIds,
									  float64 *outPrefillSeconds, float64 *outMsPerToken, NkString *err) {
				outIds.Clear();
				if (!mLoaded || promptIds.Size() == 0) {
					SetErr(err, "Generate : modèle non chargé ou prompt vide");
					return false;
				}
				NkTensor logits;
				const float64 prefillT0 = NowSeconds();
				// Préfill par tranches de maxBatchTokens : mathématiquement
				// équivalent à un préfill d'un bloc (c'est la propriété prouvée du
				// KV-cache au jalon 3), et borné en mémoire.
				uint32 i = 0;
				while (i < promptIds.Size()) {
					int32 chunk = (int32)(promptIds.Size() - i);
					if (chunk > mOpt.maxBatchTokens)
						chunk = mOpt.maxBatchTokens;
					if (!Forward(&promptIds[i], chunk, logits, nullptr, err))
						return false;
					i += (uint32)chunk;
				}
				if (outPrefillSeconds)
					*outPrefillSeconds = NowSeconds() - prefillT0;

				const float64 genT0 = NowSeconds();
				int32 produced = 0;
				for (int32 s = 0; s < nNew; ++s) {
					int32 next = (temperature > 0.0f) ? NkSampleTopK(logits, temperature, topK, rngState)
													  : NkSampleGreedy(logits);
					if (next < 0) {
						SetErr(err, "Generate : échantillonnage a échoué");
						return false;
					}
					outIds.PushBack(next);
					produced++;
					if (next == stopId)
						break;
					if (s + 1 < nNew) {
						if (!Forward(&next, 1, logits, nullptr, err))
							return false;
					}
				}
				if (outMsPerToken && produced > 0)
					*outMsPerToken = (NowSeconds() - genT0) * 1000.0 / (double)produced;
				return true;
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
