// =============================================================================
// NkVar.h — variable à différentiation automatique (NKAI, mode inverse).
//
// Une NkVar enveloppe un NkTensor (valeur) + son gradient, et enregistre dans un
// GRAPHE l'opération qui l'a produite. Backward() remonte ce graphe (backprop) et
// accumule le gradient de chaque feuille via la règle de dérivation en chaîne.
//
// Sans STL : le "backward" n'est pas une std::function mais un TAG d'opération
// (NkAutoOp) + les parents ; Backward() dispatche sur le tag. Les nœuds sont
// refcomptés (alloués via NKMemory), partagés entre NkVar (comme NkTensorStorage).
//
// Convention : gradients de même forme que la valeur ; feuilles requiresGrad=true.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {

		// Opération ayant produit un nœud (pour dispatcher le backward).
		enum class NkAutoOp : uint8 {
			NK_LEAF = 0,	   // feuille (paramètre / entrée) — pas de parents
			NK_ADD,			   // a + b (mêmes formes)
			NK_SUB,			   // a - b
			NK_MUL,			   // a ⊙ b (élémentaire)
			NK_MATMUL,		   // a[M,K] · b[K,N]
			NK_RELU,		   // relu(a)
			NK_SIGMOID,		   // sigmoid(a)
			NK_TANH,		   // tanh(a)
			NK_SUM,			   // somme -> scalaire
			NK_MSE,			   // erreur quadratique moyenne (a=pred, b=cible) -> scalaire
			NK_SOFTMAX_CE,	   // softmax(logits=a) + entropie croisée vs one-hot (b) -> scalaire
			NK_SOFTMAX_CE_IDX, // idem mais cible = INDICES [B] (b), pas one-hot -> économise [B,V]
			NK_CONV2D,		   // convolution 2D : a=entrée [B,Cin,H,W], b=poids [Cout,Cin,kH,kW]
			NK_CONVT2D,		   // conv transposée 2D (upsampling) : b=poids [Cin,Cout,kH,kW]
			NK_MAXPOOL2D,	   // max-pooling 2D (a=entrée) ; argmax mémorisé dans `aux`
			NK_RESHAPE,		   // remodelage (flatten) ; backward = reshape inverse
			NK_EXP,			   // exp(a)  (VAE : écart-type via exp(0.5·logvar))
				NK_LOG,			   // log(max(a,eps)) élément par élément (NKRL/Jalon 3 : log-probabilité PPO)
			NK_MULSCALAR,	   // a · s   (s = iparam encodé en float via aux? -> stocké dans fparam)
			NK_ADDSCALAR,	   // a + s
			NK_CONV3D,		   // convolution 3D : a=[B,Cin,D,H,W], b=[Cout,Cin,kD,kH,kW]
			NK_CONVT3D,		   // conv transposée 3D (upsampling voxels) : b=[Cin,Cout,kD,kH,kW]
			NK_SIGMOID_BCE,	   // sigmoid(logits=a) + entropie croisée binaire vs cible b -> scalaire
			NK_UPSAMPLE2X,	   // suréchantillonnage nearest ×2 : [B,C,H,W] -> [B,C,2H,2W]
			NK_LAYERNORM,	   // normalisation sur le DERNIER axe : (x−μ)/√(var+ε) (γ,β via couches)
			NK_SOFTMAX,		   // softmax sur le DERNIER axe
			NK_SOFTMAX_CAUSAL, // softmax causal (attention : masque le futur), dernier axe [..,T,T]
			NK_GELU,		   // GELU (tanh-approx)
			NK_EMBEDDING,	   // lookup table[vocab,d] par indices (aux) ; backward = scatter-add
			NK_PERMUTE,		   // permutation d'axes (rang ≤ 4) ; backward = permutation inverse
			NK_CONCAT0,		   // concaténation de deux tenseurs sur l'AXE 0 ; backward = découpe
			NK_CTC,			   // perte CTC (forward-backward) ; grad pré-calculé stocké dans `aux`
		};

		// Nœud du graphe (refcompté, partagé).
		struct NkVarNode {
				NkTensor value;			   // sortie de l'opération
				NkTensor grad;			   // gradient accumulé (même forme que value)
				bool requiresGrad = false; // participe au calcul du gradient ?
				NkAutoOp op = NkAutoOp::NK_LEAF;
				NkVarNode *a = nullptr; // parent 1 (incref)
				NkVarNode *b = nullptr; // parent 2 (incref, si binaire)
				int32 refcount = 1;
				int32 iparam[4] = {0, 0, 0, 0}; // params entiers de l'op (stride, pad, kernel…)
				double fparam = 0.0;			// paramètre scalaire de l'op (Mul/AddScalar)
				NkTensor aux;					// données auxiliaires de l'op (ex. argmax maxpool)

				static NkVarNode *New();
				static void Retain(NkVarNode *n);
				static void Release(NkVarNode *n);

				// Nombre de nœuds actuellement VIVANTS (alloués − libérés). Diagnostic/tests
				// uniquement (ex. prouver que le mode sans-gradient ne fait pas grossir le
				// graphe). Thread-UNSAFE comme le reste du module (compteur simple, pas
				// atomique) — cohérent avec le graphe lui-même qui n'est pas thread-safe.
				static int64 LiveCount();
		};

		// -------------------------------------------------------------------------
		// NkVar — handle refcompté vers un NkVarNode.
		// -------------------------------------------------------------------------
		class NkVar {
			public:
				NkVar() = default;
				~NkVar();
				NkVar(const NkVar &o);
				NkVar &operator=(const NkVar &o);
				NkVar(NkVar &&o) noexcept;
				NkVar &operator=(NkVar &&o) noexcept;

				// Crée une feuille depuis un tenseur (requiresGrad : paramètre à apprendre).
				static NkVar Leaf(const NkTensor &value, bool requiresGrad = true);

				bool IsValid() const {
					return mNode != nullptr;
				}

				const NkTensor &Value() const; // valeur (forward)
				const NkTensor &Grad() const;  // gradient (après Backward)
				bool RequiresGrad() const;

				NkVarNode *Node() const {
					return mNode;
				}

				// Remplace la valeur du nœud EN PLACE (feuille persistante) — utilisé par
				// les optimiseurs (NKOptim) pour appliquer `param -= lr·grad` sans casser
				// l'identité du nœud, afin que le prochain forward réutilise le paramètre.
				void SetValue(const NkTensor &v);

					// Remplace le GRADIENT du nœud EN PLACE (même forme que Value()). Utilisé
					// par NKOptim pour le clipping de gradient (norme globale/valeur, cf.
					// Pascanu et al. 2013) AVANT le pas d'optimiseur. Point d'entrée additif.
					void SetGrad(const NkTensor &g);

					// Backward depuis CE nœud (doit être un scalaire, typiquement la perte).
					// Remet à 1 le gradient du scalaire, propage jusqu'aux feuilles.
					void Backward();

					// Remet à zéro les gradients de tout le graphe atteignable.
					void ZeroGrad();

					// Détache : nouvelle feuille portant la MÊME valeur (référence NkTensor
					// partagée, copie légère) mais SANS lien vers ce nœud — le gradient ne
					// remonte jamais au-delà de ce point (stop-gradient). requiresGrad=false.
					// Cf. torch.Tensor.detach(). Usage : couper un résidu/une cible bootstrap
					// (ex. réseau cible en RL) du graphe qui produit l'entrée.
					NkVar Detach() const;

			private:
				explicit NkVar(NkVarNode *n) : mNode(n) {
				}

				NkVarNode *mNode = nullptr;
				friend NkVar NkMakeOp(NkAutoOp, const NkTensor &, NkVarNode *, NkVarNode *);
		};

		// -------------------------------------------------------------------------
		// Mode « sans gradient » (inférence pure) — façon torch.no_grad().
		//
		// Flag GLOBAL : quand actif, les opérations `autograd::*` calculent toujours le
		// forward (valeur correcte) mais N'ENREGISTRENT PLUS le nœud dans le graphe —
		// le résultat est une feuille isolée (requiresGrad=false, aucun parent retenu).
		// Les nœuds intermédiaires ne sont donc plus maintenus vivants par la chaîne de
		// parents et sont libérés dès que leur NkVar temporaire sort de portée : pas
		// d'accumulation mémoire du graphe pendant un forward d'inférence. Backward()
		// sur un résultat produit dans ce mode ne fait rien (pas de parents à remonter).
		//
		// ⚠️ Thread-UNSAFE : un seul flag process-wide, comme le reste du module (le
		// graphe lui-même n'est pas thread-safe). Ne pas (dés)activer depuis plusieurs
		// threads concurrents sans synchronisation externe.
		// -------------------------------------------------------------------------
		bool IsGradEnabled();
		void SetGradEnabled(bool enabled);

		// Garde RAII : désactive le gradient à la construction, restaure l'état
		// précédent à la destruction (imbrication correcte). Équivalent de
		// `with torch.no_grad():`.
		//   { NkNoGradGuard guard; ...forward d'inférence... } // restauré ici
		class NkNoGradGuard {
			public:
				NkNoGradGuard();
				~NkNoGradGuard();

				NkNoGradGuard(const NkNoGradGuard &) = delete;
				NkNoGradGuard &operator=(const NkNoGradGuard &) = delete;

			private:
				bool mPrevEnabled;
		};

		// -------------------------------------------------------------------------
		// Opérations différentiables (forward + enregistrement dans le graphe).
		// -------------------------------------------------------------------------
		namespace autograd {
			NkVar Add(const NkVar &a, const NkVar &b);
			NkVar Sub(const NkVar &a, const NkVar &b);
			NkVar Mul(const NkVar &a, const NkVar &b); // élémentaire
			NkVar Matmul(const NkVar &a, const NkVar &b);
			NkVar Relu(const NkVar &a);
			NkVar Sigmoid(const NkVar &a);
			NkVar Tanh(const NkVar &a);
			NkVar Sum(const NkVar &a);						   // -> scalaire
			NkVar MSE(const NkVar &pred, const NkVar &target); // -> scalaire
			// Softmax (par ligne) + entropie croisée vs cible one-hot [B,C] -> scalaire.
			// Fusionné pour la stabilité numérique ; gradient = (softmax − onehot)/B.
			NkVar SoftmaxCrossEntropy(const NkVar &logits, const NkVar &targetOneHot);

			// Variante à CIBLE PAR INDICES : `targetIdx` = [B] (un id de classe par ligne, float encodé ;
			// valeur < 0 = ligne MASQUÉE, ignorée en loss ET en gradient). Économise le one-hot dense [B,V]
			// (build CPU + transfert GPU) — identique numériquement à SoftmaxCrossEntropy(one-hot).
			NkVar SoftmaxCrossEntropyIndexed(const NkVar &logits, const NkVar &targetIdx);
			// Sigmoid + entropie croisée BINAIRE (par pixel), cible dans [0,1] -> scalaire
			// (moyenne). Fusionné = stable ; gradient = (sigmoid(logits) − cible)/N.
			// Idéal pour reconstruire des images (plus net que MSE). `logits` = pré-sigmoid.
			NkVar SigmoidBCE(const NkVar &logits, const NkVar &target);

			// ---- Convolution / pooling (Phase 2, briques CNN) ----------------
			// Conv2D (cross-corrélation) : input [B,Cin,H,W] ⊛ weight [Cout,Cin,kH,kW]
			// -> [B,Cout,outH,outW]. (Le biais s'ajoute via un Add [1,Cout,1,1] à part.)
			NkVar Conv2D(const NkVar &input, const NkVar &weight, int32 stride = 1, int32 pad = 0);
			// Conv transposée 2D (upsampling appris) : input [B,Cin,H,W], weight
			// [Cin,Cout,kH,kW] -> [B,Cout,(H-1)·stride-2·pad+kH, …]. Décodeurs génératifs.
			NkVar ConvTranspose2D(const NkVar &input, const NkVar &weight, int32 stride = 1, int32 pad = 0);
			// Élémentaires utiles au VAE.
			NkVar Exp(const NkVar &a);
			// log(max(a,eps)) élément par élément (eps=1e-8) ; d/dx log(x) = 1/x. Ajouté pour
			// NKRL/Jalon 3 (PPO) : permet log-softmax(logits) = Log(Softmax(logits)) — combiné à Softmax
			// (déjà différentiable, Jacobien complet) pour la log-probabilité discrète EXACTE. Voir NkPolicyNet.h.
			NkVar Log(const NkVar &a);				   // exp(a)
			NkVar MulScalar(const NkVar &a, double s); // a · s
			NkVar AddScalar(const NkVar &a, double s); // a + s
			// Convolution 3D (voxels) : input [B,Cin,D,H,W] ⊛ weight [Cout,Cin,kD,kH,kW].
			NkVar Conv3D(const NkVar &input, const NkVar &weight, int32 stride = 1, int32 pad = 0);
			// Conv transposée 3D (upsampling voxels) : weight [Cin,Cout,kD,kH,kW].
			NkVar ConvTranspose3D(const NkVar &input, const NkVar &weight, int32 stride = 1, int32 pad = 0);
			// Max-pooling 2D : [B,C,H,W] -> [B,C,H/stride,W/stride] (fenêtre kernel×kernel).
			NkVar MaxPool2D(const NkVar &input, int32 kernel = 2, int32 stride = 2);
			// Remodelage (ex. flatten [B,C,H,W] -> [B,C*H*W]) ; gradient remodelé en retour.
			NkVar Reshape(const NkVar &a, const NkShape &shape);
			// Suréchantillonnage nearest ×2 : [B,C,H,W] -> [B,C,2H,2W]. Combiné à un Conv2D
			// = « resize-conv » (décodeur génératif SANS artefacts damier du ConvTranspose).
			NkVar Upsample2x(const NkVar &a);
			// LayerNorm (normalise sur le DERNIER axe) : y = (x−μ)/√(var+ε). L'affine appris
			// (γ,β) se compose au niveau des couches (Mul/Add broadcast). ε = 1e-5.
			NkVar LayerNorm(const NkVar &x);
			// Softmax sur le dernier axe. Variante CAUSALE pour l'attention (masque le futur :
			// entrée `[.., T, T]`, la requête `i` ne voit que les clés `j ≤ i`).
			NkVar Softmax(const NkVar &x);
			NkVar SoftmaxCausal(const NkVar &x);
			// GELU (activation transformer, tanh-approx).
			NkVar Gelu(const NkVar &x);
			// Embedding : `table` [vocab,d] (paramètre appris), `indices` [..] (ids, non
			// différentiable) -> [.., d]. Backward = scatter-add vers la table.
			NkVar Embedding(const NkVar &table, const NkTensor &indices);
			// Permutation d'axes (rang ≤ 4) ; backward = permutation inverse. Pour découper les
			// têtes d'attention ([B,T,h,hd] ↔ [B,h,T,hd]).
			NkVar Permute(const NkVar &x, const NkShape &order);

				// Concaténation de deux tenseurs sur l'AXE 0 (mêmes dimensions restantes) :
				// [Na,…] ⊕ [Nb,…] -> [Na+Nb,…]. Backward = redécoupe le gradient. Sert à
				// EMPILER les sorties d'un RNN dans le temps ([1,B,H] ⊕ … -> [T,B,H]).
				NkVar Concat0(const NkVar &a, const NkVar &b);

				// Perte CTC (Connectionist Temporal Classification, Graves 2006) — aligne une
				// séquence de logits [T,B,V] sur des cibles de longueurs variables SANS
				// alignement fourni (algorithme forward-backward en espace log). `targets[b]`
				// = suite d'indices de symboles (hors blanc) pour l'exemple b ; `blank` = index
				// du symbole blanc. Renvoie la perte scalaire moyennée sur le lot ; le gradient
				// (déjà moyenné) est pré-calculé et diffusé aux logits au Backward.
				NkVar CTCLoss(const NkVar &logits, const NkVector<NkVector<int32>> &targets, int32 blank = 0);
		} // namespace autograd

	} // namespace ai
} // namespace nkentseu
