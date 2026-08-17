// =============================================================================
// NkLlama.h — bloc transformeur « moderne » (RMSNorm + RoPE + SwiGLU), NKAI.
// -----------------------------------------------------------------------------
// POURQUOI À CÔTÉ DE NkTransformer.h, ET NON DEDANS. `NkTransformerBlock` a
// entraîné les paliers 1 à 3 et le premier jalon d'Ilyana ; on n'y touche pas.
// Ce fichier est **additif** : il propose l'autre bloc, celui de Llama et de
// Qwen2, construit sur les trois opérations autograd vérifiées aux différences
// finies (`autograd::RMSNorm`, `autograd::SwiGLU`, `autograd::RoPE`).
//
// CE QUI CHANGE, ET CE QUE ÇA APPORTE :
//   • RMSNorm au lieu de LayerNorm — pas de moyenne retirée, pas de décalage :
//     moins d'opérations pour un effet équivalent en pratique.
//   • RoPE au lieu de positions apprises — la position n'est plus un vecteur
//     ajouté mais une ROTATION. Conséquence utile : le produit scalaire entre
//     deux jetons ne dépend que de leur ÉCART, ce qu'une table de positions
//     apprises ne garantit pas, et il n'y a plus de table à apprendre ni de
//     longueur maximale câblée dans les poids.
//   • SwiGLU au lieu de GELU — un MLP à porte : une branche décide combien
//     laisser passer de l'autre.
//
// ⚠️ HONNÊTETÉ SUR L'ÉTAT : les trois opérations sont pour l'instant sur le
// chemin CPU (un tenseur GPU fait l'aller-retour en préservant son device).
// Ce bloc est donc **correct mais pas encore rapide** ; il attend ses noyaux
// GPU. C'est délibéré : les mathématiques d'abord, prouvées, la vitesse ensuite.
//
// ⚠️ Attention multi-têtes PLEINE (autant de têtes K/V que de têtes Q). Le
// partage par groupes (GQA) de Qwen2 n'est PAS implémenté ici — ne pas le
// prétendre.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKNN/NkDense.h"
#include "NKNN/NkTransformer.h" // RandnTensor (init des embeddings) — partagée, pas recopiée
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkFunctions.h" // NkSqrt (convention du depot : pas de <cmath>)

namespace nkentseu {
	namespace ai {
		namespace nn {

			// ---- RMSNorm avec gain appris : y = RMSNorm(x) · γ (γ [1,d]) ------------
			// Pas de décalage β : contrairement à LayerNorm, RMSNorm ne recentre pas,
			// donc un décalage n'aurait rien à compenser. C'est le choix de Llama.
			class NkRMSNorm {
				public:
					NkRMSNorm() = default;

					explicit NkRMSNorm(uint32 d, double eps = 1e-6)
						: mGamma(NkVar::Leaf(NkTensor::Ones(NkShape{(int64)1, (int64)d}), true)), mEps(eps) {
					}

					NkVar Forward(const NkVar &x) const {
						const NkShape shp = x.Value().Shape();
						const int64 d = shp[shp.Size() - 1];
						const int64 rows = (d > 0) ? x.Value().Numel() / d : 0;
						NkVar n = autograd::Reshape(autograd::RMSNorm(x, mEps), NkShape{rows, d});
						NkVar y = autograd::Mul(n, mGamma); // broadcast [1,d]
						return autograd::Reshape(y, shp);
					}

					void Parameters(NkVector<NkVar> &o) const {
						o.PushBack(mGamma);
					}

				private:
					NkVar mGamma;
					double mEps = 1e-6;
			};

			// ---- Attention multi-têtes causale, positions par ROTATION --------------
			class NkRoPEAttention {
				public:
					NkRoPEAttention() = default;

					NkRoPEAttention(uint32 dModel, uint32 nHeads, uint32 seed = 1u, double freqBase = 10000.0)
						: mD(dModel), mH(nHeads), mFreqBase(freqBase), mWq(dModel, dModel, seed + 1u),
						  mWk(dModel, dModel, seed + 2u), mWv(dModel, dModel, seed + 3u),
						  mWo(dModel, dModel, seed + 4u) {
					}

					// x : [B, T, d] -> [B, T, d].
					NkVar Forward(const NkVar &x) const {
						const NkShape xs = x.Value().Shape();
						const int64 B = xs[0], T = xs[1], d = xs[2];
						const int64 h = mH, hd = d / h;
						auto proj = [&](const NkDense &W) {
							NkVar f = autograd::Reshape(x, NkShape{B * T, d});
							NkVar p = W.Forward(f); // [B*T, d]
							p = autograd::Reshape(p, NkShape{B, T, h, hd});
							return autograd::Permute(p, NkShape{0, 2, 1, 3}); // [B, h, T, hd]
						};
						// La rotation s'applique à Q et K, JAMAIS à V : elle encode la
						// position dans la comparaison entre jetons, pas dans le contenu
						// transporté.
						NkVar Q = autograd::RoPE(proj(mWq), 0, mFreqBase);
						NkVar K = autograd::RoPE(proj(mWk), 0, mFreqBase);
						NkVar V = proj(mWv);
						NkVar Kt = autograd::Permute(K, NkShape{0, 1, 3, 2}); // [B, h, hd, T]
						NkVar scores = autograd::Matmul(Q, Kt);				  // [B, h, T, T]
						// Echange std::sqrt -> NkSqrt : NUMERIQUEMENT NEUTRE, verifie a la
						// source. NkSqrt delegue a `sqrt` de la libm, que l'IEEE-754 impose
						// correctement arrondie : meme bit pour tout x > 0 (et `hd` l'est
						// toujours). La seule difference porte sur x <= 0, ou NkSqrt rend 0
						// au lieu de NaN. L'echange pouvait donc se faire a tout moment, y
						// compris en pleine campagne de comparaison.
						scores = autograd::MulScalar(scores, 1.0 / math::NkSqrt((double)hd));
						NkVar attn = autograd::SoftmaxCausal(scores);
						NkVar ctx = autograd::Matmul(attn, V);			  // [B, h, T, hd]
						ctx = autograd::Permute(ctx, NkShape{0, 2, 1, 3}); // [B, T, h, hd]
						ctx = autograd::Reshape(ctx, NkShape{B * T, d});
						return autograd::Reshape(mWo.Forward(ctx), NkShape{B, T, d});
					}

					void Parameters(NkVector<NkVar> &o) const {
						mWq.Parameters(o);
						mWk.Parameters(o);
						mWv.Parameters(o);
						mWo.Parameters(o);
					}

				private:
					uint32 mD = 0, mH = 0;
					double mFreqBase = 10000.0;
					NkDense mWq, mWk, mWv, mWo;
			};

			// ---- MLP à porte : down( silu(gate(x)) ⊙ up(x) ) -----------------------
			class NkSwiGLUMlp {
				public:
					NkSwiGLUMlp() = default;

					NkSwiGLUMlp(uint32 d, uint32 dff, uint32 seed = 1u)
						: mGate(d, dff, seed + 1u), mUp(d, dff, seed + 2u), mDown(dff, d, seed + 3u) {
					}

					// x : [N, d] -> [N, d].
					NkVar Forward(const NkVar &x) const {
						NkVar g = mGate.Forward(x);
						NkVar u = mUp.Forward(x);
						return mDown.Forward(autograd::SwiGLU(g, u));
					}

					void Parameters(NkVector<NkVar> &o) const {
						mGate.Parameters(o);
						mUp.Parameters(o);
						mDown.Parameters(o);
					}

					// Largeur cachée à nombre de paramètres comparable au MLP GELU
					// classique : celui-ci a 2 matrices de d×4d, celui-là en a TROIS,
					// d'où le facteur 8/3 (arrondi à un multiple de 8).
					static uint32 LargeurEquivalente(uint32 d) {
						uint32 f = (8u * d) / 3u;
						return ((f + 7u) / 8u) * 8u;
					}

				private:
					NkDense mGate, mUp, mDown;
			};

			// ---- Bloc complet : pré-norme → attention → +résiduel → pré-norme → MLP -
			class NkLlamaBlock {
				public:
					NkLlamaBlock() = default;

					NkLlamaBlock(uint32 d, uint32 h, uint32 seed = 1u, uint32 dff = 0)
						: mNorm1(d), mAttn(d, h, seed + 10u), mNorm2(d),
						  mMlp(d, (dff != 0) ? dff : NkSwiGLUMlp::LargeurEquivalente(d), seed + 20u) {
					}

					NkVar Forward(const NkVar &x) const {
						const NkShape xs = x.Value().Shape();
						const int64 B = xs[0], T = xs[1], d = xs[2];
						NkVar x1 = autograd::Add(x, mAttn.Forward(mNorm1.Forward(x)));
						NkVar f = autograd::Reshape(mNorm2.Forward(x1), NkShape{B * T, d});
						NkVar mo = autograd::Reshape(mMlp.Forward(f), NkShape{B, T, d});
						return autograd::Add(x1, mo);
					}

					void Parameters(NkVector<NkVar> &o) const {
						mNorm1.Parameters(o);
						mAttn.Parameters(o);
						mNorm2.Parameters(o);
						mMlp.Parameters(o);
					}

				private:
					NkRMSNorm mNorm1, mNorm2;
					NkRoPEAttention mAttn;
					NkSwiGLUMlp mMlp;
			};

			// ---- Modèle de langue complet, sans table de positions ------------------
			// Différence visible avec `NkGPT` : il n'y a PAS d'embedding positionnel.
			// La position vit dans la rotation, donc le modèle n'a aucune longueur
			// maximale inscrite dans ses poids.
			class NkLlamaLM {
				public:
					NkLlamaLM() = default;

					// `tied` : LIE la projection de sortie a la table d'embedding (weight
					// tying). La tete n'a alors plus sa propre matrice [d, vocab] : elle
					// reutilise la TRANSPOSEE de mTokEmb [vocab, d]. Economie = vocab x d
					// (12,6 M a V=32768, d=384).
					//
					// ⚠️ CE QUE CELA CHANGE, AU-DELA DU COMPTE. La table recoit desormais du
					// gradient par DEUX chemins de natures differentes :
					//   - la recherche en entree : CREUSE, seules les lignes des tokens du lot ;
					//   - la projection de sortie : DENSE, TOUTES les lignes a chaque pas.
					// Le taux effectif sur les embeddings change donc de nature. C'est en
					// general benefique, mais si une course montre une degradation nette,
					// c'est ICI qu'il faut chercher AVANT de mettre en cause le principe
					// (mise a l'echelle d'un des deux chemins, ou initialisation d'une table
					// liee differente de celle d'une table libre).
					NkLlamaLM(uint32 vocab, uint32 dModel, uint32 nHeads, uint32 nLayers, uint32 seed = 1u,
							  bool tied = false)
						: mVocab(vocab), mD(dModel), mTied(tied),
						  mTokEmb(NkVar::Leaf(
							  RandnTensor(NkShape{(int64)vocab, (int64)dModel}, kEcartTypeTable, seed + 1u), true)),
						  mNormF(dModel) {
						for (uint32 l = 0; l < nLayers; ++l)
							mBlocks.PushBack(NkLlamaBlock(dModel, nHeads, seed + 100u + l * 17u));
						if (mTied) {
							// Tete liee : la matrice [d, vocab] n'est PAS construite du tout —
							// c'est tout l'interet. Seul le biais vit : une ligne de `vocab`
							// valeurs, qui ne pese rien et garde les deux variantes comparables.
							mHeadBias = NkVar::Leaf(NkTensor::Zeros(NkShape{(int64)1, (int64)vocab}), true);

							// ⚠️ ESSAI ECARTE, GARDE POUR MEMOIRE : mise a l'echelle des logits.
							//
							// Constat de depart, exact : la tete liee demarre avec des logits
							// 2,57x plus larges qu'une tete libre (la table est initialisee a
							// 0,02, une tete Xavier a sqrt(2/(d+V)) = 0,00777). Perte au pas 1 :
							// 10,4369 contre 10,4051.
							//
							// Remede essaye : multiplier les logits par sqrt(2/(d+V))/0,02.
							// MESURE (14 aout 2026) : corrige bien la perte au pas 1 (10,4066)
							// et DEGRADE massivement l'apprentissage -- metrique 7,1617 contre
							// 6,5961 sans facteur et 6,5664 pour la tete libre. Vingt fois pire
							// que le tying nu.
							//
							// POURQUOI C'ETAIT MAL FORME : c'est une correction PERMANENTE
							// appliquee a un probleme d'INITIALISATION. Le facteur impose une
							// temperature de 2,58 pendant TOUT l'entrainement et divise d'autant
							// le gradient qui remonte vers la table par le chemin dense -- sans
							// toucher au chemin creux. Il modifie donc le RAPPORT entre les deux
							// chemins, la grandeur meme qu'on soupconnait.
							//
							// ET LE DIAGNOSTIC ETAIT FAUX : un handicap de depart produit un
							// decalage CONSTANT. Or le tying nu MENAIT aux pas 100 et 150
							// (7,14032 vs 7,21918 ; 6,84109 vs 6,85602) et ne decrochait qu'apres
							// le pas 200 : c'est une dynamique d'apprentissage, pas un point de
							// depart. Ne pas reintroduire ce facteur sans mesure nouvelle.
						} else {
							mHead = NkDense(dModel, vocab, seed + 3u);
						}
					}

					// tokens : [B, T] d'identifiants (f32). Renvoie les logits [B*T, vocab].
					NkVar Forward(const NkTensor &tokens) const {
						const int64 B = tokens.Shape()[0], T = tokens.Shape()[1];
						NkVar x = autograd::Embedding(mTokEmb, tokens); // [B,T,d]
						for (uint32 l = 0; l < mBlocks.Size(); ++l)
							x = mBlocks[l].Forward(x);
						x = mNormF.Forward(x);
						NkVar plat = autograd::Reshape(x, NkShape{B * T, (int64)mD});
						if (!mTied)
							return mHead.Forward(plat);
						// logits = x · mTokEmbᵀ + b. La transposition passe par Permute, donc
						// le gradient remonte jusqu'a mTokEmb : c'est ce qui LIE reellement les
						// deux usages. Un simple partage de valeurs, sans passer par le graphe,
						// donnerait le bon compte de parametres et un mauvais entrainement.
						NkShape ordre;
						ordre.PushBack(1);
						ordre.PushBack(0);
						NkVar logits = autograd::Matmul(plat, autograd::Permute(mTokEmb, ordre));
						return autograd::Add(logits, mHeadBias); // Add diffuse la ligne, cf. NkDense
					}

					void Parameters(NkVector<NkVar> &o) const {
						o.PushBack(mTokEmb);
						for (uint32 l = 0; l < mBlocks.Size(); ++l)
							mBlocks[l].Parameters(o);
						mNormF.Parameters(o);
						if (mTied) {
							// mTokEmb est DEJA dans la liste : la reposer ici la ferait mettre
							// a jour DEUX FOIS par l'optimiseur, donc avancer au double du taux
							// demande — un defaut qui ne planterait pas et se lirait comme
							// « le tying degrade ».
							o.PushBack(mHeadBias);
						} else {
							mHead.Parameters(o);
						}
					}

					bool Tied() const {
						return mTied;
					}

					uint32 Vocab() const {
						return mVocab;
					}

				private:
					// Ecart-type d'initialisation de la table. NOMME parce que le facteur
					// d'echelle des logits le relit : les deux ne peuvent plus diverger.
					static constexpr double kEcartTypeTable = 0.02;
					uint32 mVocab = 0, mD = 0;
					bool mTied = false;
					NkVar mHeadBias; // seulement si mTied
					NkVar mTokEmb;
					NkVector<NkLlamaBlock> mBlocks;
					NkRMSNorm mNormF;
					NkDense mHead;
			};

		} // namespace nn
	} // namespace ai
} // namespace nkentseu
