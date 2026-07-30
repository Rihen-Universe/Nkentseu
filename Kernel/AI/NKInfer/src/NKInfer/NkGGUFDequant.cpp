// =============================================================================
// NkGGUFDequant.cpp — déquantification GGUF vers NkTensor f32 (NKInfer, Jalon 3).
// Voir NkGGUFDequant.h pour la portée, les sources et les formats supportés.
// =============================================================================
#include "NKInfer/NkGGUFDequant.h"
#include "NKFileSystem/NkFile.h"
#include "NKTensor/NkFp16.h"

#include <cstring> // memcpy (bit-cast portable des blocs bruts, comme NkFp16.h)

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {

				// -------------------------------------------------------------------
				// Structures de bloc GGUF/ggml — disposition binaire reproduite
				// FIDÈLEMENT depuis la source de référence llama.cpp/ggml (voir
				// NkGGUFDequant.h pour les URLs exactes consultées le 2026-07-25) :
				//
				//   ggml/src/ggml-common.h :
				//     #define QK_K 256
				//     #define K_SCALE_SIZE 12
				//     typedef struct { ggml_half d; int8_t qs[32]; } block_q8_0;             // 34 o
				//     typedef struct {                                                       // 144 o
				//         union { struct { ggml_half d; ggml_half dmin; }; ggml_half2 dm; };
				//         uint8_t scales[K_SCALE_SIZE];
				//         uint8_t qs[QK_K/2];
				//     } block_q4_K;
				//     typedef struct {                                                       // 210 o
				//         uint8_t ql[QK_K/2];
				//         uint8_t qh[QK_K/4];
				//         int8_t  scales[QK_K/16];
				//         ggml_half d;
				//     } block_q6_K;
				//
				// `ggml_half` = uint16 (bits IEEE754 binary16, PAS un type matériel).
				// Ces structs ne sont JAMAIS utilisées pour lire directement en mémoire
				// via un pointeur casté (le buffer source peut être mal aligné, et ce
				// n'est de toute façon pas portable) : on les lit champ par champ via
				// memcpy depuis le buffer brut, exactement comme NkFp16.h le fait déjà
				// pour f16<->f32. `static_assert` garantit qu'aucun padding de
				// compilateur ne fausse `sizeof` (utilisé pour l'arithmétique de bloc).
				// -------------------------------------------------------------------

				constexpr uint64 kQK_K = 256;			// taille (en éléments) d'un super-bloc K-quant
				constexpr uint64 kKScaleSize = 12;		// octets du paquet scales+mins 6 bits (Q4_K)

				struct BlockQ8_0 {
						uint16 d;		// échelle fp16 (bits)
						int8 qs[32];	// quants signés 8 bits
				};
				static_assert(sizeof(BlockQ8_0) == 34, "block_q8_0 doit faire 34 octets (spec ggml)");

				struct BlockQ4_K {
						uint16 d;					   // super-échelle des échelles (fp16 bits)
						uint16 dmin;				   // super-échelle des minimums (fp16 bits)
						uint8 scales[kKScaleSize];	   // 8 paires (échelle,min) 6 bits, paquet 12 octets
						uint8 qs[kQK_K / 2];		   // 256 nibbles 4 bits (128 octets)
				};
				static_assert(sizeof(BlockQ4_K) == 144, "block_q4_K doit faire 144 octets (spec ggml)");

				struct BlockQ6_K {
						uint8 ql[kQK_K / 2];   // 128 o : 4 bits bas de chaque quant
						uint8 qh[kQK_K / 4];   // 64 o  : 2 bits hauts (2 quants/octet, 4 quants/2 bits-paire)
						int8 scales[kQK_K / 16]; // 16 o : une échelle int8 par sous-bloc de 16
						uint16 d;			   // super-échelle globale (fp16 bits)
				};
				static_assert(sizeof(BlockQ6_K) == 210, "block_q6_K doit faire 210 octets (spec ggml)");

				inline float F16BitsToF32(uint16 bits) {
					return NkF16BitsToF32(bits); // NKTensor/NkFp16.h — conversion bit-exacte RNE
				}

				// -------------------------------------------------------------------
				// get_scale_min_k4 — désempaquette la paire (échelle, min) 6 bits du
				// sous-bloc `j` (0..7) depuis le paquet compact `scales[12]` d'un
				// bloc Q4_K. Reproduit tel quel depuis ggml-quants.c (voir .h) :
				//
				//   if (j < 4) { d = q[j] & 63; m = q[j+4] & 63; }
				//   else       { d = (q[j+4] & 0xF) | ((q[j-4] >> 6) << 4);
				//                m = (q[j+4] >> 4)  | ((q[j-0] >> 6) << 4); }
				//
				// (Le "q[j-0]" de la source == q[j] ; conservé identique pour matcher
				// l'original terme à terme et faciliter toute relecture croisée.)
				// -------------------------------------------------------------------
				inline void GetScaleMinK4(int j, const uint8 *q, uint8 &d, uint8 &m) {
					if (j < 4) {
						d = q[j] & 63;
						m = q[j + 4] & 63;
					} else {
						d = (uint8)((q[j + 4] & 0xF) | ((q[j - 4] >> 6) << 4));
						m = (uint8)((q[j + 4] >> 4) | ((q[j - 0] >> 6) << 4));
					}
				}

				// ---- F32 : passthrough (déjà le format cible) ----------------------
				bool DequantF32(const uint8 *src, uint64 rawByteCount, uint64 numElements, NkVector<float32> &out) {
					const uint64 need = numElements * 4ull;
					if (rawByteCount < need)
						return false;
					out.Resize((NkVector<float32>::SizeType)numElements);
					std::memcpy(out.Data(), src, (usize)need);
					return true;
				}

				// ---- F16 : conversion bit-exacte via NkFp16.h -----------------------
				bool DequantF16(const uint8 *src, uint64 rawByteCount, uint64 numElements, NkVector<float32> &out) {
					const uint64 need = numElements * 2ull;
					if (rawByteCount < need)
						return false;
					out.Resize((NkVector<float32>::SizeType)numElements);
					for (uint64 i = 0; i < numElements; ++i) {
						uint16 bits = 0;
						std::memcpy(&bits, src + i * 2, 2);
						out[(NkVector<float32>::SizeType)i] = F16BitsToF32(bits);
					}
					return true;
				}

				// ---- Q8_0 : blocs de 32, échelle fp16 par bloc ----------------------
				// Reproduit dequantize_row_q8_0 (ggml-quants.c) :
				//   y[i*32+j] = qs[j] * fp16_to_f32(d)
				bool DequantQ8_0(const uint8 *src, uint64 rawByteCount, uint64 numElements, NkVector<float32> &out) {
					constexpr uint64 kBlockElems = 32;
					if (numElements % kBlockElems != 0)
						return false;
					const uint64 nBlocks = numElements / kBlockElems;
					const uint64 need = nBlocks * sizeof(BlockQ8_0);
					if (rawByteCount < need)
						return false;

					out.Resize((NkVector<float32>::SizeType)numElements);
					for (uint64 b = 0; b < nBlocks; ++b) {
						BlockQ8_0 blk;
						std::memcpy(&blk, src + b * sizeof(BlockQ8_0), sizeof(BlockQ8_0));
						const float d = F16BitsToF32(blk.d);
						float *y = out.Data() + b * kBlockElems;
						for (uint64 j = 0; j < kBlockElems; ++j)
							y[j] = (float)blk.qs[j] * d;
					}
					return true;
				}

				// ---- Q4_K : super-blocs de 256 (8 sous-blocs de 32) -----------------
				// Reproduit dequantize_row_q4_K (ggml-quants.c) :
				//   d = fp16_to_f32(x.d) ; min = fp16_to_f32(x.dmin)
				//   pour is = 0,2,4,6 (4 paires de sous-blocs 32+32 = 64 éléments) :
				//     (sc,m) = GetScaleMinK4(is+0) -> d1 = d*sc, m1 = min*m
				//     (sc,m) = GetScaleMinK4(is+1) -> d2 = d*sc, m2 = min*m
				//     32 éléments : y = d1*(nibble bas) - m1
				//     32 éléments : y = d2*(nibble haut) - m2
				bool DequantQ4_K(const uint8 *src, uint64 rawByteCount, uint64 numElements, NkVector<float32> &out) {
					if (numElements % kQK_K != 0)
						return false;
					const uint64 nBlocks = numElements / kQK_K;
					const uint64 need = nBlocks * sizeof(BlockQ4_K);
					if (rawByteCount < need)
						return false;

					out.Resize((NkVector<float32>::SizeType)numElements);
					for (uint64 b = 0; b < nBlocks; ++b) {
						BlockQ4_K blk;
						std::memcpy(&blk, src + b * sizeof(BlockQ4_K), sizeof(BlockQ4_K));
						const float d = F16BitsToF32(blk.d);
						const float dmin = F16BitsToF32(blk.dmin);

						float *y = out.Data() + b * kQK_K;
						const uint8 *q = blk.qs;
						int is = 0;
						for (uint64 j = 0; j < kQK_K; j += 64) {
							uint8 sc = 0, m = 0;
							GetScaleMinK4(is + 0, blk.scales, sc, m);
							const float d1 = d * (float)sc;
							const float m1 = dmin * (float)m;
							GetScaleMinK4(is + 1, blk.scales, sc, m);
							const float d2 = d * (float)sc;
							const float m2 = dmin * (float)m;

							for (int l = 0; l < 32; ++l)
								*y++ = d1 * (float)(q[l] & 0xF) - m1;
							for (int l = 0; l < 32; ++l)
								*y++ = d2 * (float)(q[l] >> 4) - m2;

							q += 32;
							is += 2;
						}
					}
					return true;
				}

				// ---- Q6_K : super-blocs de 256 (quants signés 6 bits) ---------------
				// Reproduit dequantize_row_q6_K (ggml-quants.c) : pour chaque moitié de
				// 128 éléments (2 par super-bloc), 32 quadruplets (q1..q4) reconstruits
				// à partir de ql (4 bits bas) + qh (2 bits hauts), recentrés de -32
				// (plage signée 6 bits [-32,31]), multipliés par l'échelle int8 du
				// sous-bloc de 16 correspondant puis par l'échelle fp16 globale `d`.
				bool DequantQ6_K(const uint8 *src, uint64 rawByteCount, uint64 numElements, NkVector<float32> &out) {
					if (numElements % kQK_K != 0)
						return false;
					const uint64 nBlocks = numElements / kQK_K;
					const uint64 need = nBlocks * sizeof(BlockQ6_K);
					if (rawByteCount < need)
						return false;

					out.Resize((NkVector<float32>::SizeType)numElements);
					for (uint64 b = 0; b < nBlocks; ++b) {
						BlockQ6_K blk;
						std::memcpy(&blk, src + b * sizeof(BlockQ6_K), sizeof(BlockQ6_K));
						const float d = F16BitsToF32(blk.d);

						float *y = out.Data() + b * kQK_K;
						const uint8 *ql = blk.ql;
						const uint8 *qh = blk.qh;
						const int8 *sc = blk.scales;

						for (int n = 0; n < (int)kQK_K; n += 128) {
							for (int l = 0; l < 32; ++l) {
								const int is = l / 16;
								const int8 q1 = (int8)((ql[l + 0] & 0xF) | (((qh[l] >> 0) & 3) << 4)) - 32;
								const int8 q2 = (int8)((ql[l + 32] & 0xF) | (((qh[l] >> 2) & 3) << 4)) - 32;
								const int8 q3 = (int8)((ql[l + 0] >> 4) | (((qh[l] >> 4) & 3) << 4)) - 32;
								const int8 q4 = (int8)((ql[l + 32] >> 4) | (((qh[l] >> 6) & 3) << 4)) - 32;
								y[l + 0] = d * (float)sc[is + 0] * (float)q1;
								y[l + 32] = d * (float)sc[is + 2] * (float)q2;
								y[l + 64] = d * (float)sc[is + 4] * (float)q3;
								y[l + 96] = d * (float)sc[is + 6] * (float)q4;
							}
							y += 128;
							ql += 64;
							qh += 32;
							sc += 8;
						}
					}
					return true;
				}

			} // namespace

			bool NkGGUFDequantSupported(uint32 rawType) {
				switch (rawType) {
					case (uint32)NkGGUFTensorType::NK_GGML_F32:
					case (uint32)NkGGUFTensorType::NK_GGML_F16:
					case (uint32)NkGGUFTensorType::NK_GGML_Q8_0:
					case (uint32)NkGGUFTensorType::NK_GGML_Q4_K:
					case (uint32)NkGGUFTensorType::NK_GGML_Q6_K:
						return true;
					default:
						return false;
				}
			}

			bool NkGGUFDequantizeRaw(uint32 rawType, const void *rawData, uint64 rawByteCount, uint64 numElements,
									 NkVector<float32> &outData, NkString *outError) {
				outData.Clear();
				if (!rawData || numElements == 0) {
					if (outError)
						*outError = NkString("buffer source vide ou numElements == 0");
					return false;
				}
				const uint8 *src = reinterpret_cast<const uint8 *>(rawData);

				bool ok = false;
				switch (rawType) {
					case (uint32)NkGGUFTensorType::NK_GGML_F32:
						ok = DequantF32(src, rawByteCount, numElements, outData);
						break;
					case (uint32)NkGGUFTensorType::NK_GGML_F16:
						ok = DequantF16(src, rawByteCount, numElements, outData);
						break;
					case (uint32)NkGGUFTensorType::NK_GGML_Q8_0:
						ok = DequantQ8_0(src, rawByteCount, numElements, outData);
						break;
					case (uint32)NkGGUFTensorType::NK_GGML_Q4_K:
						ok = DequantQ4_K(src, rawByteCount, numElements, outData);
						break;
					case (uint32)NkGGUFTensorType::NK_GGML_Q6_K:
						ok = DequantQ6_K(src, rawByteCount, numElements, outData);
						break;
					default:
						if (outError) {
							*outError = NkString("type de quantification non supporté par NkGGUFDequant : ");
							outError->Append(NkGGUFTensorTypeName(rawType));
						}
						return false;
				}
				if (!ok && outError) {
					*outError = NkString("buffer source trop court ou numElements incompatible avec la taille de bloc pour ");
					outError->Append(NkGGUFTensorTypeName(rawType));
				}
				return ok;
			}

			bool NkGGUFReadTensorRawBytes(const char *path, const NkGGUFFile &file, const NkGGUFTensorInfo &tensor,
										  NkVector<uint8> &outBytes, NkString *outError) {
				outBytes.Clear();
				if (!tensor.sizeKnown) {
					if (outError) {
						*outError = NkString("taille inconnue pour le tenseur '");
						outError->Append(tensor.name);
						outError->Append("' (type de quantification non tabulé par NkGGUFLoader)");
					}
					return false;
				}

				const uint64 absOffset = file.tensorDataOffset + tensor.offset;
				const uint64 absEnd = absOffset + tensor.sizeBytes;
				if (absEnd > file.fileSize) {
					if (outError)
						*outError = NkString("lecture hors bornes du fichier pour ce tenseur (fichier tronqué ?)");
					return false;
				}

				NkFile f(path, NkFileMode::NK_READ_BINARY);
				if (!f.IsOpen()) {
					if (outError)
						*outError = NkString("impossible d'ouvrir le fichier pour lire les données du tenseur");
					return false;
				}
				if (!f.Seek((nk_int64)absOffset, NkSeekOrigin::NK_BEGIN)) {
					if (outError)
						*outError = NkString("Seek vers les données du tenseur échoué");
					return false;
				}

				outBytes.Resize((NkVector<uint8>::SizeType)tensor.sizeBytes);
				const usize got = f.Read(outBytes.Data(), (usize)tensor.sizeBytes);
				if (got != (usize)tensor.sizeBytes) {
					if (outError)
						*outError = NkString("lecture des données du tenseur incomplète (fichier tronqué ?)");
					outBytes.Clear();
					return false;
				}
				return true;
			}

			bool NkGGUFDequantizeTensor(const char *path, const NkGGUFFile &file, const NkGGUFTensorInfo &tensor,
									   NkTensor &outTensor, NkString *outError) {
				NkVector<uint8> raw;
				if (!NkGGUFReadTensorRawBytes(path, file, tensor, raw, outError))
					return false;

				NkVector<float32> data;
				if (!NkGGUFDequantizeRaw(tensor.rawType, raw.Data(), raw.Size(), tensor.numElements, data, outError))
					return false;

				// Forme row-major (C-order) pour NkTensor = dims GGUF (ne[]) inversés
				// (ne[0] = dimension la plus rapide en mémoire côté ggml ; la mémoire
				// pointée par `data` n'est PAS réordonnée, seule l'INTERPRÉTATION de
				// la forme change).
				NkShape shape;
				shape.Resize((NkShape::SizeType)tensor.dims.Size());
				for (uint32 i = 0; i < tensor.dims.Size(); ++i)
					shape[i] = (int64)tensor.dims[tensor.dims.Size() - 1 - i];

				outTensor = NkTensor::FromData(shape, data.Data(), NkDType::NK_F32, NkDevice::NK_CPU);
				if (!outTensor.IsValid()) {
					if (outError)
						*outError = NkString("NkTensor::FromData a échoué (forme/allocation invalide)");
					return false;
				}
				return true;
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
