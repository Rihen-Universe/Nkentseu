// =============================================================================
// NKAgent/NkAgentLLMReasoning.cpp — voir NkAgentLLMReasoning.h pour la portée,
// le scénario, la justification de l'encodage numérique du prompt et les
// limites (latence, absence d'encodeur BPE).
// =============================================================================
#include "NKAgent/NkAgentLLMReasoning.h"
#include "NKInfer/NkGGUFDequant.h"
#include "NKFileSystem/NkFile.h"
#include "NKTensor/NkTensor.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace ai {
		namespace agent {

			namespace {

				const infer::NkGGUFTensorInfo *FindTensor(const infer::NkGGUFFile &gguf, const char *name) {
					for (uint32 i = 0; i < gguf.tensors.Size(); ++i)
						if (gguf.tensors[i].name.Compare(name) == 0)
							return &gguf.tensors[i];
					return nullptr;
				}

				// Cherche un token EXACT dans le vocabulaire (recherche linéaire — le
				// vocabulaire n'est parcouru qu'à la CHARGE du modèle, pas à chaque
				// décision). Essaie la forme brute ("0") puis la forme préfixée "Ġ"
				// (U+0120, convention BPE d'un token précédé d'un espace) — l'une des
				// deux existe presque toujours pour un chiffre isolé dans un
				// vocabulaire de cette taille (152064 tokens, Qwen2.5).
				int32 FindDigitToken(const NkVector<NkString> &vocab, char digit) {
					char bare[2] = {digit, '\0'};
					char spacePrefixed[4];
					spacePrefixed[0] = (char)0xC4; // U+0120 'Ġ' encodé en UTF-8 : 0xC4 0x80
					spacePrefixed[1] = (char)0x80;
					spacePrefixed[2] = digit;
					spacePrefixed[3] = '\0';
					for (uint32 i = 0; i < vocab.Size(); ++i)
						if (vocab[i].Compare(bare) == 0)
							return (int32)i;
					for (uint32 i = 0; i < vocab.Size(); ++i)
						if (vocab[i].Compare(spacePrefixed) == 0)
							return (int32)i;
					return -1;
				}

				bool DequantNamed(const char *path, const infer::NkGGUFFile &gguf, const char *name, NkTensor &out) {
					const infer::NkGGUFTensorInfo *t = FindTensor(gguf, name);
					if (!t)
						return false;
					return infer::NkGGUFDequantizeTensor(path, gguf, *t, out, nullptr);
				}

				bool LoadLayerWeights(const char *path, const infer::NkGGUFFile &gguf, uint32 layer,
									   infer::NkQwen2LayerWeights &w) {
					char buf[160];
					bool ok = true;
#define NK_AGENT_LLM_LOAD(field, suffix)                                                                             \
	std::snprintf(buf, sizeof(buf), "blk.%u." suffix, layer);                                                        \
	ok = ok && DequantNamed(path, gguf, buf, w.field)
					NK_AGENT_LLM_LOAD(attnNorm, "attn_norm.weight");
					NK_AGENT_LLM_LOAD(wq, "attn_q.weight");
					NK_AGENT_LLM_LOAD(bq, "attn_q.bias");
					NK_AGENT_LLM_LOAD(wk, "attn_k.weight");
					NK_AGENT_LLM_LOAD(bk, "attn_k.bias");
					NK_AGENT_LLM_LOAD(wv, "attn_v.weight");
					NK_AGENT_LLM_LOAD(bv, "attn_v.bias");
					NK_AGENT_LLM_LOAD(wo, "attn_output.weight");
					NK_AGENT_LLM_LOAD(ffnNorm, "ffn_norm.weight");
					NK_AGENT_LLM_LOAD(wGate, "ffn_gate.weight");
					NK_AGENT_LLM_LOAD(wUp, "ffn_up.weight");
					NK_AGENT_LLM_LOAD(wDown, "ffn_down.weight");
#undef NK_AGENT_LLM_LOAD
					return ok;
				}

				// Déquantifie SEULEMENT les lignes `rowIds` d'un tenseur 2D [vocab,d]
				// (Q4_K/Q6_K/Q8_0/F16/F32) SANS matérialiser le tenseur entier : chaque
				// ligne (un token) est un bloc CONTIGU d'octets dans le fichier (ne0=d
				// = dimension la plus rapide côté ggml). Réutilisée pour DEUX usages :
				// (1) l'embedding des tokens du prompt (`rowIds` = ids de tokens), (2)
				// l'extraction des 4 lignes candidates de lm_head/output.weight
				// (`rowIds` = les 4 tokens-chiffres '0'..'3') — les deux tenseurs
				// partagent la même convention [vocab,d]. Même algorithme que
				// DequantEmbeddingRows (Applications/NKLLMInferTest/src/main.cpp),
				// généralisé ici pour être réutilisable des deux côtés.
				bool DequantSelectiveRows(const char *path, const infer::NkGGUFFile &gguf,
										   const infer::NkGGUFTensorInfo &t, const NkVector<int64> &rowIds,
										   NkTensor &outRows) {
					if (!t.sizeKnown || t.dims.Size() != 2)
						return false;
					const int64 rowLen = (int64)t.dims[0];
					const int64 vocab = (int64)t.dims[1];
					if (vocab <= 0 || t.numElements != (uint64)(rowLen * vocab))
						return false;
					const uint64 bytesPerRow = t.sizeBytes / (uint64)vocab;

					NkFile f(path, NkFileMode::NK_READ_BINARY);
					if (!f.IsOpen())
						return false;
					outRows = NkTensor::Zeros(NkShape{(int64)rowIds.Size(), rowLen});
					NkVector<uint8> raw;
					raw.Resize((NkVector<uint8>::SizeType)bytesPerRow);
					for (uint32 ri = 0; ri < rowIds.Size(); ++ri) {
						const int64 row = rowIds[ri];
						if (row < 0 || row >= vocab)
							return false;
						const uint64 absOffset = gguf.tensorDataOffset + t.offset + (uint64)row * bytesPerRow;
						if (!f.Seek((nk_int64)absOffset, NkSeekOrigin::NK_BEGIN))
							return false;
						const usize got = f.Read(raw.Data(), (usize)bytesPerRow);
						if (got != (usize)bytesPerRow)
							return false;
						NkVector<float32> rowData;
						if (!infer::NkGGUFDequantizeRaw(t.rawType, raw.Data(), raw.Size(), (uint64)rowLen, rowData, nullptr))
							return false;
						std::memcpy(outRows.DataAs<float>() + (int64)ri * rowLen, rowData.Data(),
									(usize)rowLen * sizeof(float));
					}
					return true;
				}

				// Décompose `value` en ses chiffres décimaux, ajoute chaque
				// token-chiffre correspondant (model.digitTokenId, cf en-tête) dans
				// `ids`.
				void AppendDigits(const NkAgentLLMModel &model, uint32 value, NkVector<int64> &ids) {
					char buf[16];
					std::snprintf(buf, sizeof(buf), "%u", value);
					for (const char *p = buf; *p; ++p)
						ids.PushBack((int64)model.digitTokenId[*p - '0']);
				}

			} // namespace

			bool NkAgentLoadLLMModel(const char *ggufPath, NkAgentLLMModel &outModel, NkString *outError) {
				outModel = NkAgentLLMModel();
				for (int i = 0; i < 10; ++i)
					outModel.digitTokenId[i] = -1;

				if (!infer::NkGGUFLoader::Load(ggufPath, outModel.gguf) || !outModel.gguf.valid) {
					if (outError)
						*outError = outModel.gguf.error;
					return false;
				}

				NkString arch;
				if (!infer::NkGGUFGetString(outModel.gguf, "general.architecture", arch) || arch.Compare("qwen2") != 0) {
					if (outError)
						*outError = "general.architecture != \"qwen2\"";
					return false;
				}

				uint64 dModel = 0, ffnDim = 0, headCount = 0, headCountKv = 0, blockCount = 0;
				float64 ropeFreqBase = 0.0, rmsEps = 0.0;
				bool metaOk = true;
				metaOk = metaOk && infer::NkGGUFGetUInt(outModel.gguf, "qwen2.block_count", blockCount);
				metaOk = metaOk && infer::NkGGUFGetUInt(outModel.gguf, "qwen2.embedding_length", dModel);
				metaOk = metaOk && infer::NkGGUFGetUInt(outModel.gguf, "qwen2.feed_forward_length", ffnDim);
				metaOk = metaOk && infer::NkGGUFGetUInt(outModel.gguf, "qwen2.attention.head_count", headCount);
				metaOk = metaOk && infer::NkGGUFGetUInt(outModel.gguf, "qwen2.attention.head_count_kv", headCountKv);
				metaOk = metaOk && infer::NkGGUFGetFloat(outModel.gguf, "qwen2.rope.freq_base", ropeFreqBase);
				metaOk = metaOk &&
						 infer::NkGGUFGetFloat(outModel.gguf, "qwen2.attention.layer_norm_rms_epsilon", rmsEps);
				if (!metaOk) {
					if (outError)
						*outError = "métadonnées qwen2.* manquantes dans ce GGUF";
					return false;
				}

				outModel.blockCount = blockCount;
				outModel.cfg.dModel = (int32)dModel;
				outModel.cfg.nHeads = (int32)headCount;
				outModel.cfg.nKVHeads = (int32)headCountKv;
				outModel.cfg.headDim = headCount > 0 ? (int32)(dModel / headCount) : 0;
				outModel.cfg.ffnDim = (int32)ffnDim;
				outModel.cfg.ropeFreqBase = (float32)ropeFreqBase;
				outModel.cfg.rmsEps = (float32)rmsEps;
				if (!outModel.cfg.IsValid()) {
					if (outError)
						*outError = "NkQwen2Config invalide (formes incohérentes)";
					return false;
				}

				if (!infer::NkGGUFGetUInt(outModel.gguf, "tokenizer.ggml.bos_token_id", outModel.bosId)) {
					if (outError)
						*outError = "tokenizer.ggml.bos_token_id absent";
					return false;
				}

				outModel.embTensor = FindTensor(outModel.gguf, "token_embd.weight");
				if (!outModel.embTensor) {
					if (outError)
						*outError = "tenseur 'token_embd.weight' introuvable";
					return false;
				}
				outModel.lmHeadTensor = FindTensor(outModel.gguf, "output.weight"); // nullptr = tied (réutilise embTensor)

				if (!infer::NkGGUFReadFullStringArray(ggufPath, "tokenizer.ggml.tokens", outModel.vocab)) {
					if (outError)
						*outError = "vocabulaire (tokenizer.ggml.tokens) illisible";
					return false;
				}

				NkString missing;
				for (char d = '0'; d <= '9'; ++d) {
					const int32 id = FindDigitToken(outModel.vocab, d);
					outModel.digitTokenId[d - '0'] = id;
					if (id < 0) {
						if (!missing.Empty())
							missing += ',';
						missing += d;
					}
				}
				if (!missing.Empty()) {
					if (outError) {
						*outError = "chiffre(s) introuvable(s) dans le vocabulaire : ";
						*outError += missing;
					}
					return false;
				}

				outModel.path = ggufPath;
				outModel.ready = true;
				return true;
			}

			NkAgentLLMDecision NkAgentDecideViaLLM(const NkAgentLLMModel &model, uint32 rawState, uint32 targetState,
													uint32 nLayers) {
				NkAgentLLMDecision res;
				res.candidateLogits[0] = res.candidateLogits[1] = res.candidateLogits[2] = res.candidateLogits[3] = 0.0f;
				if (!model.ready || !model.embTensor)
					return res;
				if (nLayers > model.blockCount)
					nLayers = (uint32)model.blockCount;
				if (nLayers == 0)
					return res;

				NkChrono clock;
				const char *path = model.path.CStr();

				// ---- Encodage du prompt : BOS réel + chiffres décimaux de l'état et
				// du but (cf en-tête de fichier : PAS de texte libre, aucun encodeur
				// BPE dans ce dépôt). ----
				NkVector<int64> promptIds;
				promptIds.PushBack((int64)model.bosId);
				AppendDigits(model, rawState, promptIds);
				AppendDigits(model, targetState, promptIds);

				NkTensor x;
				if (!DequantSelectiveRows(path, model.gguf, *model.embTensor, promptIds, x))
					return res;

				infer::NkKVCache cache;
				cache.Reset(nLayers);
				NkTensor h = x;
				for (uint32 l = 0; l < nLayers; ++l) {
					infer::NkQwen2LayerWeights w;
					if (!LoadLayerWeights(path, model.gguf, l, w))
						return res;
					h = infer::NkQwen2LayerForward(model.cfg, w, h, cache.layers[l]);
					if (!h.IsValid())
						return res;
				}

				NkTensor outputNorm;
				if (!DequantNamed(path, model.gguf, "output_norm.weight", outputNorm))
					return res;

				const int64 T = h.Shape()[0];
				NkTensor hLast = h.Slice(0, T - 1, T, 1).Reshape(NkShape{1, (int64)model.cfg.dModel}); // dernière position
				NkTensor hn = infer::NkRMSNorm(hLast, outputNorm, model.cfg.rmsEps);
				if (!hn.IsValid())
					return res;

				// ---- Logits restreints aux 4 tokens-chiffres '0'..'3' (= les 4
				// actions) : SEULES ces 4 lignes de lm_head sont déquantifiées (pas
				// les 152064 lignes complètes). ----
				NkVector<int64> candidateIds;
				for (int32 d = 0; d <= 3; ++d)
					candidateIds.PushBack(model.digitTokenId[d]);
				const infer::NkGGUFTensorInfo *lmT = model.lmHeadTensor ? model.lmHeadTensor : model.embTensor;
				NkTensor candRows;
				if (!DequantSelectiveRows(path, model.gguf, *lmT, candidateIds, candRows))
					return res;

				NkTensor logits = infer::NkLinearNoBias(hn, candRows); // [1,4]
				if (!logits.IsValid())
					return res;

				const float *lp = logits.DataAs<float>();
				int32 bestA = 0;
				float best = lp[0];
				for (int32 a = 0; a < 4; ++a) {
					res.candidateLogits[a] = lp[a];
					if (lp[a] > best) {
						best = lp[a];
						bestA = a;
					}
				}

				res.ok = true;
				res.action = bestA;
				res.nLayers = nLayers;
				res.seconds = clock.Elapsed().ToSeconds();
				return res;
			}

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
