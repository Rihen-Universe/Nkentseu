// =============================================================================
// NkGGUFLoader.cpp — lecteur de format GGUF (NKAI, Phase 7).
// =============================================================================
#include "NKInfer/NkGGUFLoader.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {

				// Garde-fou contre un fichier corrompu/tronqué : aucune chaîne GGUF
				// légitime (nom de tenseur, texte de métadonnée) ne dépasse quelques Mo.
				constexpr uint64 kMaxStringLen = 64ull * 1024 * 1024;
				// Idem pour les compteurs d'en-tête (tensor_count / metadata_kv_count) :
				// un vrai modèle en a quelques centaines à quelques milliers.
				constexpr uint64 kMaxReasonableCount = 10000000ull;
				constexpr uint32 kMaxReasonableDims = 8;

				bool ReadRaw(NkFile &f, void *dst, usize n) {
					return f.Read(dst, n) == n;
				}

				template <typename T> bool ReadPOD(NkFile &f, T &out) {
					return ReadRaw(f, &out, sizeof(T));
				}

				// gguf_string = uint64 longueur + octets bruts (sans terminateur nul).
				bool ReadGGUFString(NkFile &f, NkString &out) {
					uint64 len = 0;
					if (!ReadPOD(f, len))
						return false;
					if (len > kMaxStringLen)
						return false;
					if (len == 0) {
						out = NkString();
						return true;
					}
					NkVector<char> buf;
					buf.Resize((NkVector<char>::SizeType)len);
					if (!ReadRaw(f, buf.Data(), (usize)len))
						return false;
					out = NkString(buf.Data(), (NkString::SizeType)len);
					return true;
				}

				// Avance le curseur au-delà d'une gguf_string SANS matérialiser son
				// contenu (utilisé pour les éléments de tableau au-delà de l'aperçu).
				bool SkipGGUFString(NkFile &f) {
					uint64 len = 0;
					if (!ReadPOD(f, len))
						return false;
					if (len > kMaxStringLen)
						return false;
					if (len == 0)
						return true;
					return f.Seek((nk_int64)len, NkSeekOrigin::NK_CURRENT);
				}

				// Taille en octets d'un type scalaire de métadonnée (0 = STRING/inconnu,
				// géré séparément).
				usize GGUFScalarByteSize(NkGGUFValueType t) {
					switch (t) {
						case NkGGUFValueType::NK_GGUF_UINT8:
						case NkGGUFValueType::NK_GGUF_INT8:
						case NkGGUFValueType::NK_GGUF_BOOL:
							return 1;
						case NkGGUFValueType::NK_GGUF_UINT16:
						case NkGGUFValueType::NK_GGUF_INT16:
							return 2;
						case NkGGUFValueType::NK_GGUF_UINT32:
						case NkGGUFValueType::NK_GGUF_INT32:
						case NkGGUFValueType::NK_GGUF_FLOAT32:
							return 4;
						case NkGGUFValueType::NK_GGUF_UINT64:
						case NkGGUFValueType::NK_GGUF_INT64:
						case NkGGUFValueType::NK_GGUF_FLOAT64:
							return 8;
						default:
							return 0;
					}
				}

				bool ReadScalar(NkFile &f, NkGGUFValueType t, NkGGUFScalar &out) {
					out.type = t;
					switch (t) {
						case NkGGUFValueType::NK_GGUF_UINT8:
							return ReadPOD(f, out.u8);
						case NkGGUFValueType::NK_GGUF_INT8:
							return ReadPOD(f, out.i8);
						case NkGGUFValueType::NK_GGUF_UINT16:
							return ReadPOD(f, out.u16);
						case NkGGUFValueType::NK_GGUF_INT16:
							return ReadPOD(f, out.i16);
						case NkGGUFValueType::NK_GGUF_UINT32:
							return ReadPOD(f, out.u32);
						case NkGGUFValueType::NK_GGUF_INT32:
							return ReadPOD(f, out.i32);
						case NkGGUFValueType::NK_GGUF_FLOAT32:
							return ReadPOD(f, out.f32);
						case NkGGUFValueType::NK_GGUF_BOOL: {
							uint8 b = 0;
							if (!ReadPOD(f, b))
								return false;
							out.b32 = (b != 0) ? 1 : 0;
							return true;
						}
						case NkGGUFValueType::NK_GGUF_UINT64:
							return ReadPOD(f, out.u64);
						case NkGGUFValueType::NK_GGUF_INT64:
							return ReadPOD(f, out.i64);
						case NkGGUFValueType::NK_GGUF_FLOAT64:
							return ReadPOD(f, out.f64);
						case NkGGUFValueType::NK_GGUF_STRING:
							return ReadGGUFString(f, out.str);
						default:
							return false; // type inconnu : on ne sait pas combien d'octets sauter
					}
				}

				bool SkipScalar(NkFile &f, NkGGUFValueType t) {
					if (t == NkGGUFValueType::NK_GGUF_STRING)
						return SkipGGUFString(f);
					usize sz = GGUFScalarByteSize(t);
					if (sz == 0)
						return false;
					return f.Seek((nk_int64)sz, NkSeekOrigin::NK_CURRENT);
				}

				// Lit une valeur de métadonnée complète (scalaire ou tableau) dans `kv`.
				// `kv.type`/`kv.key` doivent déjà être renseignés par l'appelant.
				bool ReadMetadataValue(NkFile &f, NkGGUFValueType type, NkGGUFMetadataKV &kv) {
					kv.type = type;
					if (type == NkGGUFValueType::NK_GGUF_ARRAY) {
						uint32 elemTypeRaw = 0;
						uint64 len = 0;
						if (!ReadPOD(f, elemTypeRaw))
							return false;
						if (!ReadPOD(f, len))
							return false;
						kv.arrayElemType = (NkGGUFValueType)elemTypeRaw;
						kv.arrayLen = len;
						// Le format GGUF ne supporte pas les tableaux imbriqués.
						if (kv.arrayElemType == NkGGUFValueType::NK_GGUF_ARRAY)
							return false;
						if (len > kMaxReasonableCount)
							return false;
						for (uint64 i = 0; i < len; ++i) {
							if (i < kNkGGUFMaxArrayPreview) {
								NkGGUFScalar s;
								if (!ReadScalar(f, kv.arrayElemType, s))
									return false;
								kv.preview.PushBack(s);
							} else {
								if (!SkipScalar(f, kv.arrayElemType))
									return false;
							}
						}
						return true;
					}
					return ReadScalar(f, type, kv.scalar);
				}

				// Table des tailles de bloc/type par quantification (source : spec
				// publique ggml/llama.cpp, table `type_traits` de ggml.c — reproduite de
				// mémoire, non re-vérifiée octet-par-octet ici). Renvoie false pour tout
				// type non tabulé (IQ*/TQ*/futurs K-quants) : sizeKnown restera false
				// pour ces tenseurs, mais nom/dims/offset restent lus normalement.
				bool NkGGUFTensorBlockInfo(uint32 rawType, uint64 &blockSize, uint64 &typeSize) {
					switch (rawType) {
						case (uint32)NkGGUFTensorType::NK_GGML_F32:
							blockSize = 1;
							typeSize = 4;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_F16:
							blockSize = 1;
							typeSize = 2;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q4_0:
							blockSize = 32;
							typeSize = 18;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q4_1:
							blockSize = 32;
							typeSize = 20;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q5_0:
							blockSize = 32;
							typeSize = 22;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q5_1:
							blockSize = 32;
							typeSize = 24;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q8_0:
							blockSize = 32;
							typeSize = 34;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q8_1:
							blockSize = 32;
							typeSize = 36;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q2_K:
							blockSize = 256;
							typeSize = 84;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q3_K:
							blockSize = 256;
							typeSize = 110;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q4_K:
							blockSize = 256;
							typeSize = 144;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q5_K:
							blockSize = 256;
							typeSize = 176;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q6_K:
							blockSize = 256;
							typeSize = 210;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_Q8_K:
							blockSize = 256;
							typeSize = 292;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_I8:
							blockSize = 1;
							typeSize = 1;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_I16:
							blockSize = 1;
							typeSize = 2;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_I32:
							blockSize = 1;
							typeSize = 4;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_I64:
							blockSize = 1;
							typeSize = 8;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_F64:
							blockSize = 1;
							typeSize = 8;
							return true;
						case (uint32)NkGGUFTensorType::NK_GGML_BF16:
							blockSize = 1;
							typeSize = 2;
							return true;
						default:
							return false;
					}
				}

			} // namespace

			const char *NkGGUFValueTypeName(NkGGUFValueType type) {
				switch (type) {
					case NkGGUFValueType::NK_GGUF_UINT8:
						return "UINT8";
					case NkGGUFValueType::NK_GGUF_INT8:
						return "INT8";
					case NkGGUFValueType::NK_GGUF_UINT16:
						return "UINT16";
					case NkGGUFValueType::NK_GGUF_INT16:
						return "INT16";
					case NkGGUFValueType::NK_GGUF_UINT32:
						return "UINT32";
					case NkGGUFValueType::NK_GGUF_INT32:
						return "INT32";
					case NkGGUFValueType::NK_GGUF_FLOAT32:
						return "FLOAT32";
					case NkGGUFValueType::NK_GGUF_BOOL:
						return "BOOL";
					case NkGGUFValueType::NK_GGUF_STRING:
						return "STRING";
					case NkGGUFValueType::NK_GGUF_ARRAY:
						return "ARRAY";
					case NkGGUFValueType::NK_GGUF_UINT64:
						return "UINT64";
					case NkGGUFValueType::NK_GGUF_INT64:
						return "INT64";
					case NkGGUFValueType::NK_GGUF_FLOAT64:
						return "FLOAT64";
					default:
						return "UNKNOWN";
				}
			}

			const char *NkGGUFTensorTypeName(uint32 rawType) {
				switch (rawType) {
					case 0:
						return "F32";
					case 1:
						return "F16";
					case 2:
						return "Q4_0";
					case 3:
						return "Q4_1";
					case 6:
						return "Q5_0";
					case 7:
						return "Q5_1";
					case 8:
						return "Q8_0";
					case 9:
						return "Q8_1";
					case 10:
						return "Q2_K";
					case 11:
						return "Q3_K";
					case 12:
						return "Q4_K";
					case 13:
						return "Q5_K";
					case 14:
						return "Q6_K";
					case 15:
						return "Q8_K";
					case 16:
						return "IQ2_XXS";
					case 17:
						return "IQ2_XS";
					case 18:
						return "IQ3_XXS";
					case 19:
						return "IQ1_S";
					case 20:
						return "IQ4_NL";
					case 21:
						return "IQ3_S";
					case 22:
						return "IQ2_S";
					case 23:
						return "IQ4_XS";
					case 24:
						return "I8";
					case 25:
						return "I16";
					case 26:
						return "I32";
					case 27:
						return "I64";
					case 28:
						return "F64";
					case 29:
						return "IQ1_M";
					case 30:
						return "BF16";
					default:
						return "UNKNOWN";
				}
			}

			bool NkGGUFLoader::Load(const char *path, NkGGUFFile &outFile) {
				outFile = NkGGUFFile();
				outFile.path = NkString(path);

				NkFile f(path, NkFileMode::NK_READ_BINARY);
				if (!f.IsOpen()) {
					outFile.error = NkString("impossible d'ouvrir le fichier");
					return false;
				}

				outFile.fileSize = (uint64)f.GetSize();
				if ((int64)outFile.fileSize < 24) {
					outFile.error = NkString("fichier trop petit pour contenir un en-tête GGUF");
					return false;
				}

				uint8 magic[4] = {0, 0, 0, 0};
				if (!ReadRaw(f, magic, 4)) {
					outFile.error = NkString("lecture du magic échouée");
					return false;
				}
				if (!(magic[0] == 'G' && magic[1] == 'G' && magic[2] == 'U' && magic[3] == 'F')) {
					outFile.error = NkString("magic invalide : ce n'est pas un fichier GGUF");
					return false;
				}

				if (!ReadPOD(f, outFile.version)) {
					outFile.error = NkString("lecture de la version échouée");
					return false;
				}
				// v1 (32-bit) est un format historique différent (comptes/longueurs sur
				// 32 bits) : on refuse plutôt que de mal l'interpréter en 64-bit.
				if (outFile.version < 2 || outFile.version > 3) {
					outFile.error = NkString("version GGUF non supportée (attendu 2 ou 3)");
					return false;
				}

				if (!ReadPOD(f, outFile.tensorCount)) {
					outFile.error = NkString("lecture de tensor_count échouée");
					return false;
				}
				if (!ReadPOD(f, outFile.metadataCount)) {
					outFile.error = NkString("lecture de metadata_kv_count échouée");
					return false;
				}
				if (outFile.tensorCount > kMaxReasonableCount || outFile.metadataCount > kMaxReasonableCount) {
					outFile.error = NkString("tensor_count/metadata_kv_count aberrants (fichier corrompu ?)");
					return false;
				}

				outFile.metadata.Reserve((NkVector<NkGGUFMetadataKV>::SizeType)outFile.metadataCount);
				for (uint64 i = 0; i < outFile.metadataCount; ++i) {
					NkGGUFMetadataKV kv;
					if (!ReadGGUFString(f, kv.key)) {
						outFile.error = NkString("clé de métadonnée illisible");
						return false;
					}
					uint32 typeRaw = 0;
					if (!ReadPOD(f, typeRaw)) {
						outFile.error = NkString("type de métadonnée illisible pour la clé '");
						outFile.error.Append(kv.key);
						outFile.error.Append("'");
						return false;
					}
					if (!ReadMetadataValue(f, (NkGGUFValueType)typeRaw, kv)) {
						outFile.error = NkString("valeur de métadonnée illisible pour la clé '");
						outFile.error.Append(kv.key);
						outFile.error.Append("'");
						return false;
					}
					outFile.metadata.PushBack(kv);
				}

				// Alignement de la zone de données tensor (general.alignment, défaut 32).
				outFile.alignment = 32;
				uint64 align = 0;
				if (NkGGUFGetUInt(outFile, "general.alignment", align) && align > 0)
					outFile.alignment = align;

				outFile.tensors.Reserve((NkVector<NkGGUFTensorInfo>::SizeType)outFile.tensorCount);
				for (uint64 i = 0; i < outFile.tensorCount; ++i) {
					NkGGUFTensorInfo t;
					if (!ReadGGUFString(f, t.name)) {
						outFile.error = NkString("nom de tenseur illisible");
						return false;
					}
					uint32 nDims = 0;
					if (!ReadPOD(f, nDims)) {
						outFile.error = NkString("n_dims illisible pour le tenseur '");
						outFile.error.Append(t.name);
						outFile.error.Append("'");
						return false;
					}
					if (nDims > kMaxReasonableDims) {
						outFile.error = NkString("n_dims aberrant pour le tenseur '");
						outFile.error.Append(t.name);
						outFile.error.Append("'");
						return false;
					}
					t.dims.Reserve(nDims);
					uint64 numel = 1;
					for (uint32 d = 0; d < nDims; ++d) {
						uint64 dim = 0;
						if (!ReadPOD(f, dim)) {
							outFile.error = NkString("dimension illisible pour le tenseur '");
							outFile.error.Append(t.name);
							outFile.error.Append("'");
							return false;
						}
						t.dims.PushBack(dim);
						numel *= (dim == 0 ? 1ull : dim);
					}
					t.numElements = numel;

					if (!ReadPOD(f, t.rawType)) {
						outFile.error = NkString("type de tenseur illisible pour '");
						outFile.error.Append(t.name);
						outFile.error.Append("'");
						return false;
					}
					t.type = (NkGGUFTensorType)t.rawType;

					if (!ReadPOD(f, t.offset)) {
						outFile.error = NkString("offset illisible pour le tenseur '");
						outFile.error.Append(t.name);
						outFile.error.Append("'");
						return false;
					}

					uint64 blockSize = 0, typeSize = 0;
					if (NkGGUFTensorBlockInfo(t.rawType, blockSize, typeSize) && blockSize > 0) {
						uint64 nBlocks = (t.numElements + blockSize - 1) / blockSize;
						t.sizeBytes = nBlocks * typeSize;
						t.sizeKnown = true;
					} else {
						t.sizeBytes = 0;
						t.sizeKnown = false;
					}

					outFile.tensors.PushBack(t);
				}

				// Les données tensor démarrent à la position courante, alignées.
				nk_int64 pos = f.Tell();
				if (pos < 0) {
					outFile.error = NkString("position de fichier invalide après tensor_info");
					return false;
				}
				uint64 upos = (uint64)pos;
				uint64 rem = outFile.alignment > 0 ? (upos % outFile.alignment) : 0;
				outFile.tensorDataOffset = upos + (rem == 0 ? 0 : (outFile.alignment - rem));

				// Validation de cohérence : chaque tenseur doit tenir dans le fichier.
				// On NE LIT AUCUN OCTET de donnée ici, seulement de l'arithmétique sur
				// les offsets/tailles calculées.
				for (uint32 i = 0; i < outFile.tensors.Size(); ++i) {
					const NkGGUFTensorInfo &t = outFile.tensors[i];
					uint64 absOffset = outFile.tensorDataOffset + t.offset;
					if (absOffset > outFile.fileSize) {
						outFile.error = NkString("tenseur '");
						outFile.error.Append(t.name);
						outFile.error.Append("' : offset hors des bornes du fichier");
						return false;
					}
					if (t.sizeKnown) {
						uint64 end = absOffset + t.sizeBytes;
						if (end > outFile.fileSize) {
							outFile.error = NkString("tenseur '");
							outFile.error.Append(t.name);
							outFile.error.Append("' : taille attendue dépasse la fin du fichier");
							return false;
						}
					}
				}

				outFile.valid = true;
				return true;
			}

			const NkGGUFMetadataKV *NkGGUFFindMeta(const NkGGUFFile &file, const char *key) {
				for (uint32 i = 0; i < file.metadata.Size(); ++i)
					if (file.metadata[i].key.Compare(key) == 0)
						return &file.metadata[i];
				return nullptr;
			}

			bool NkGGUFGetString(const NkGGUFFile &file, const char *key, NkString &out) {
				const NkGGUFMetadataKV *kv = NkGGUFFindMeta(file, key);
				if (!kv || kv->type != NkGGUFValueType::NK_GGUF_STRING)
					return false;
				out = kv->scalar.str;
				return true;
			}

			bool NkGGUFGetUInt(const NkGGUFFile &file, const char *key, uint64 &out) {
				const NkGGUFMetadataKV *kv = NkGGUFFindMeta(file, key);
				if (!kv)
					return false;
				switch (kv->scalar.type) {
					case NkGGUFValueType::NK_GGUF_UINT8:
						out = kv->scalar.u8;
						return true;
					case NkGGUFValueType::NK_GGUF_UINT16:
						out = kv->scalar.u16;
						return true;
					case NkGGUFValueType::NK_GGUF_UINT32:
						out = kv->scalar.u32;
						return true;
					case NkGGUFValueType::NK_GGUF_UINT64:
						out = kv->scalar.u64;
						return true;
					case NkGGUFValueType::NK_GGUF_INT8:
						out = (uint64)kv->scalar.i8;
						return true;
					case NkGGUFValueType::NK_GGUF_INT16:
						out = (uint64)kv->scalar.i16;
						return true;
					case NkGGUFValueType::NK_GGUF_INT32:
						out = (uint64)kv->scalar.i32;
						return true;
					case NkGGUFValueType::NK_GGUF_INT64:
						out = (uint64)kv->scalar.i64;
						return true;
					default:
						return false;
				}
			}

			bool NkGGUFGetInt(const NkGGUFFile &file, const char *key, int64 &out) {
				uint64 u = 0;
				if (!NkGGUFGetUInt(file, key, u))
					return false;
				out = (int64)u;
				return true;
			}

			bool NkGGUFGetFloat(const NkGGUFFile &file, const char *key, float64 &out) {
				const NkGGUFMetadataKV *kv = NkGGUFFindMeta(file, key);
				if (!kv)
					return false;
				if (kv->scalar.type == NkGGUFValueType::NK_GGUF_FLOAT32) {
					out = (float64)kv->scalar.f32;
					return true;
				}
				if (kv->scalar.type == NkGGUFValueType::NK_GGUF_FLOAT64) {
					out = kv->scalar.f64;
					return true;
				}
				return false;
			}

			bool NkGGUFGetBool(const NkGGUFFile &file, const char *key, bool &out) {
				const NkGGUFMetadataKV *kv = NkGGUFFindMeta(file, key);
				if (!kv || kv->scalar.type != NkGGUFValueType::NK_GGUF_BOOL)
					return false;
				out = kv->scalar.b32 != 0;
				return true;
			}

			uint64 NkGGUFArrayLength(const NkGGUFFile &file, const char *key) {
				const NkGGUFMetadataKV *kv = NkGGUFFindMeta(file, key);
				if (!kv || kv->type != NkGGUFValueType::NK_GGUF_ARRAY)
					return 0;
				return kv->arrayLen;
			}

			// Voir le commentaire de la déclaration (NkGGUFLoader.h) : parcours DÉDIÉ,
			// n'appelle PAS Load() et ne touche à AUCUNE structure existante -- réutilise
			// seulement ReadRaw/ReadPOD/ReadGGUFString/SkipGGUFString/SkipScalar (fonctions
			// du namespace anonyme ci-dessus, visibles dans cette unité de traduction).
			bool NkGGUFReadFullStringArray(const char *path, const char *key, NkVector<NkString> &out) {
				out.Clear();
				NkFile f(path, NkFileMode::NK_READ_BINARY);
				if (!f.IsOpen())
					return false;

				uint8 magic[4] = {0, 0, 0, 0};
				if (!ReadRaw(f, magic, 4))
					return false;
				if (!(magic[0] == 'G' && magic[1] == 'G' && magic[2] == 'U' && magic[3] == 'F'))
					return false;

				uint32 version = 0;
				if (!ReadPOD(f, version) || version < 2 || version > 3)
					return false;

				uint64 tensorCount = 0, metaCount = 0;
				if (!ReadPOD(f, tensorCount))
					return false;
				if (!ReadPOD(f, metaCount))
					return false;
				if (tensorCount > kMaxReasonableCount || metaCount > kMaxReasonableCount)
					return false;

				for (uint64 i = 0; i < metaCount; ++i) {
					NkString k;
					if (!ReadGGUFString(f, k))
						return false;
					uint32 typeRaw = 0;
					if (!ReadPOD(f, typeRaw))
						return false;
					const NkGGUFValueType type = (NkGGUFValueType)typeRaw;
					const bool isTarget = (k.Compare(key) == 0);

					if (type == NkGGUFValueType::NK_GGUF_ARRAY) {
						uint32 elemTypeRaw = 0;
						uint64 len = 0;
						if (!ReadPOD(f, elemTypeRaw))
							return false;
						if (!ReadPOD(f, len))
							return false;
						const NkGGUFValueType elemType = (NkGGUFValueType)elemTypeRaw;
						if (elemType == NkGGUFValueType::NK_GGUF_ARRAY)
							return false; // tableaux imbriqués : non supporté (comme Load())
						if (len > kMaxReasonableCount)
							return false;

						if (isTarget && elemType == NkGGUFValueType::NK_GGUF_STRING) {
							out.Reserve((NkVector<NkString>::SizeType)len);
							for (uint64 j = 0; j < len; ++j) {
								NkString s;
								if (!ReadGGUFString(f, s))
									return false;
								out.PushBack(s);
							}
							return true; // trouvé et lu en entier : inutile de continuer
						}
						for (uint64 j = 0; j < len; ++j) {
							if (elemType == NkGGUFValueType::NK_GGUF_STRING) {
								if (!SkipGGUFString(f))
									return false;
							} else {
								if (!SkipScalar(f, elemType))
									return false;
							}
						}
					} else {
						if (!SkipScalar(f, type))
							return false;
					}
				}
				return false; // clé absente ou pas un tableau de chaînes
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
