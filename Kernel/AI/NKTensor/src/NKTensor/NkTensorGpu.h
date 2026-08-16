// =============================================================================
// NkTensorGpu.h — contexte GPU de NKTensor (Jalon 3).
//
// Encapsule le chemin compute PROUVÉ : kernel écrit en NkSL -> GLSL-Vulkan ->
// (glslang -> SPIR-V -> SPIRV-Cross) -> HLSL/SPIRV/MSL -> pipeline compute NKRHI.
// Le tenseur ne connaît pas NKRHI : il ne manipule que des handles opaques (uint64)
// de buffers GPU gérés ici. Device headless (compute-only, sans fenêtre).
//
// Tout est derrière un pimpl : NkTensor.h/.cpp restent CPU-only et sans dépendance
// NKRHI ; seul NkTensorGpu.cpp tire NKRHI + NKSL.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
// NkTensor.h est CPU-only et LEGER (aucune dependance NKRHI/NKSL) : l'inclure ici
// donne `NkShape`/`NkDType` aux fabriques GPU declarees plus bas, sans rien tirer
// de NKRHI dans les en-tetes. Le sens de dependance reste celui d'origine — le
// contexte GPU connait le tenseur, le tenseur ne connait pas NKRHI.
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {

		class NkTensorGpu {
			public:
				// Singleton paresseux. Le device GPU n'est créé qu'au premier usage.
				static NkTensorGpu &Get();

				// Un device compute headless est-il disponible sur cette machine ?
				bool IsAvailable();
				const char *BackendName(); // "DirectX 11" / "Vulkan" / … / "none"

				// ---- Buffers GPU (stockage des tenseurs) ---------------------------
				// Retourne un id opaque (>0) ou 0 en échec. Le buffer est un storage
				// buffer (SSBO / UAV) utilisable en compute et relisible par le CPU.
				uint64 CreateBuffer(nk_size bytes);

				// Nombre de DEFAUTS GPU signales depuis le demarrage (allocation refusee,
				// tampon invalide). Un entrainement doit le consulter : un calcul qui
				// n'a pas lieu ne produit AUCUNE erreur, seulement une perte immobile.
				static int64 DefautCount();

				// Operations GPU lancees depuis le demarrage. Divise par le nombre de pas,
				// il donne le cout fixe par operation — la grandeur qui dit si le moteur
				// est limite par le calcul ou par le lancement des noyaux.
				static int64 OpCount();

				// ---- PROFIL PAR NOYAU ---------------------------------------------
				// OpCount dit COMBIEN d'operations ; il ne dit pas LESQUELLES coutent.
				// Diviser le temps total par le nombre d'operations donne une moyenne
				// qui melange un produit matriciel de 1,6 GFLOP et un `add` de 400 Ko :
				// elle ne peut designer aucun coupable.
				//
				// ⚠️ CE QUE CE PROFIL MESURE, ET CE QU'IL NE MESURE PAS. Chaque
				// dispatch est suivi d'un `WaitIdle` : le temps mural pris autour de
				// l'appel contient donc le temps GPU du noyau PLUS le cout fixe de
				// lancement (descripteur, command buffer, soumission, synchronisation).
				// Ce n'est PAS un temps GPU pur — c'est le temps mural attribue a
				// l'operation, c'est-a-dire exactement la grandeur qui compose la duree
				// d'un pas. Des timestamps GPU separeraient les deux ; celui-ci dit
				// d'abord OU va le budget.
				//
				// Les transferts et les allocations sont instrumentes SEPAREMENT
				// (`~upload`, `~download`, `~alloc`, `~free`) : un profil CPU avait
				// designe `ToGPU` et `DestroyBuffer` en tete, et on en avait conclu a
				// tort que le temps y etait PERDU. Les mettre dans la meme table que
				// les noyaux rend la comparaison directe au lieu de deductive.
				static void ProfilRaz(bool actif);							 // vide la table et (des)active
				static void ProfilRapport(double secondesMurales, int64 pas); // journalise le tableau

				// ---- Occupation VRAM suivie ---------------------------------------
				// PIC de la somme des tampons vivants, en octets. C'est la SEULE
				// grandeur qui decide si une configuration tient : une moyenne, ou un
				// releve a un instant arbitraire, rate le moment ou la passe ARRIERE
				// materialise les gradients par-dessus les activations.
				//
				// ⚠️ Ne compte QUE nos tampons de calcul : ni le pilote, ni la
				// fragmentation, ni les allocations des autres modules. C'est donc un
				// PLANCHER de l'occupation reelle, jamais son total — d'ou la marge a
				// exiger avant de conclure qu'une configuration tient sur 8 Go.
				static int64 VramPic();
				static int64 VramVivante();
				void RazVramPic(); // repart du niveau courant (avant une phase mesuree)

				// ---- RESERVE DE TAMPONS (chantier n°2) ------------------------------
				// Recycle les tampons par TAILLE EXACTE au lieu de detruire/recreer.
				// Mesure qui la motive : une allocation coute +427 a +492 us, chiffre
				// obtenu DEUX FOIS independamment — au banc `add` et au banc `matmul`,
				// deux noyaux sans rapport. C'est une propriete du PILOTE.
				//
				// ⚠️ UNE RESERVE EST UN CACHE, ET UN CACHE REPOND TOUJOURS.
				// C'est la famille de defauts que ce depot paie depuis une semaine
				// (`ClearBuffer` qui accepte et ne fait rien, `device` non honore, un
				// build qui annonce SUCCESS sans recompiler). Un cache mal cable rend
				// un tampon et parait marcher : le gain se mesure, la justesse non.
				// D'ou le TEMOIN, cable AVANT la premiere mesure :
				//   - `ReserveServis()` compte les tampons rendus PAR LA RESERVE ;
				//   - `ReserveNeufs()`  compte les allocations REELLES ;
				//   - leur somme DOIT egaler le nombre d'appels a CreateBuffer, sinon
				//     l'instrument ment et aucun gain n'est lisible.
				// Sans ces deux compteurs, « la reserve marche » serait indiscernable
				// de « la reserve ne sert jamais et tout passe en allocation ».
				//
				// ⚠️ ELLE NE LIBERE PLUS LA VRAM. Un tampon retenu reste alloue. Le pic
				// mesure est de 6 659 Mo sur 8 Go : sans plafond, la reserve ferait
				// deborder. D'ou `ReserveBudget()` — au-dela, on detruit vraiment.
				//
				// ⚠️ TAILLE EXACTE, jamais « assez grand ». Rendre un tampon plus grand
				// que demande marcherait a l'usage et fausserait tout calcul de bornes.
				static void ReserveActive(bool actif); // interrupteur : LEGACY / NEUF
				static bool ReserveEstActive();
				static void ReserveBudget(int64 octetsMax); // plafond de retention
				static void ReserveVider();					// detruit tout ce qui est retenu
				static int64 ReserveServis();				// TEMOIN : rendus par la reserve
				static int64 ReserveNeufs();				// TEMOIN : allocations reelles
				static int64 ReserveOctetsRetenus();		// VRAM immobilisee par la reserve
				static int64 ReserveTamponsRetenus();
				static int64 ReserveEvictions(); // detruits faute de budget
				static void ReserveRazCompteurs();

				void DestroyBuffer(uint64 id);
				bool Upload(uint64 id, const void *data, nk_size bytes);
				bool Download(uint64 id, void *out, nk_size bytes);

				// ---- Remise a zero SUR PLACE, sans transfert depuis l'hote ----------
				// Remplit `bytes` octets du tampon avec le motif 32 bits `motif`
				// (0 = zeros). Passe par `NkICommandBuffer::ClearBuffer` (NKRHI).
				//
				// ⚠️ POURQUOI CETTE FONCTION EXISTE. `NkVar::Backward()` remettait a
				// zero chaque accumulateur de gradient en fabriquant un tenseur CPU nul
				// et en le MONTANT sur le GPU : 12,77 Go de zeros par pas, 99,9 % de
				// tout le trafic CPU->GPU de l'entrainement, depuis une seule ligne. Il
				// n'y a aucune information dans ce transfert.
				//
				// Renvoie false si le tampon est invalide OU si la primitive n'est pas
				// disponible sur le backend courant — voir ClearDisponible().
				bool Clear(uint64 id, nk_size bytes, uint32 motif = 0);

				// La primitive de remise a zero fonctionne-t-elle VRAIMENT ici ?
				//
				// ⚠️ Verifie par un TEMOIN, pas par une declaration : au premier appel,
				// on alloue un petit tampon, on y ecrit un motif non nul, on appelle
				// Clear, on relit, et on exige des zeros. C'est la lecon du 16/08 —
				// `ClearBuffer` etait declare sur les six backends et implemente sur
				// AUCUN, et un `grep` du nom plus un appelant avaient suffi a le croire
				// implemente. Une signature ne prouve rien ; une relecture, si.
				//
				// Le resultat est calcule UNE fois et journalise (disponible ou non).
				static bool ClearDisponible();

				// ---- Kernels ------------------------------------------------------
				// Élémentaire binaire : C = f(A, B) sur `count` éléments f32.
				// Le kernel NkSL doit déclarer buffers 0,1,2 (A,B,C) + UBO binding 3
				// { uint count }, workgroup local_size_x=64. Compilé et mis en cache par nom.
				bool RunBinary(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint64 c, uint32 count);

				// Élémentaire unaire : B = f(A) sur `count` éléments f32.
				// Kernel : buffers 0,1 (A,B) + UBO binding 2 { uint count }.
				bool RunUnary(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint32 count);

				// Unaire avec scalaire : B = f(A, s). Kernel : buffers 0,1 + UBO binding 2
				// { uint count; float s; }. Pour mulscalar / addscalar / step.
				bool RunUnaryScalar(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint32 count,
									float s);

				// MatMul C[M,N] = A[M,K] · B[K,N] (row-major, f32).
				// Kernel : buffers 0,1,2 (A,B,C) + UBO binding 3 { uint M,N,K },
				// workgroup 16x16.
				bool RunMatMul(uint64 a, uint64 b, uint64 c, uint32 M, uint32 N, uint32 K);

				// Réduction segmentée : vue [outer, reduce, inner], sortie [outer, inner]
				// (out[o,i] = reduce_r in[o*reduce*inner + r*inner + i]). Couvre toutes les
				// réductions d'axe (axe 0 : outer=1 ; dernier axe : inner=1 ; globale :
				// outer=inner=1). Kernel : buffers 0,1 (A,B) + UBO binding 2 { uint outer,
				// reduce, inner }, un thread par élément de sortie.
				bool RunReduce(const char *name, const NkString &nkslSrc, uint64 a, uint64 out, uint32 outer,
							   uint32 reduce, uint32 inner);

				// Matérialisation contiguë d'une vue strided (gather par strides). Chaque
				// thread lit un élément de sortie à src = offset + Σ coord_d · stride_d.
				// Couvre permute/transpose sur GPU (rang ≤ 8). shape/strides en ÉLÉMENTS.
				bool RunGather(uint64 in, uint64 out, uint32 rank, uint32 offset, const uint32 *shape,
							   const uint32 *strides, uint32 count);

				// im2col / col2im GPU. `p` = 12 uints {B,Cin,H,W,kH,kW,stride,pad,outH,outW,K,M}.
				// Kernel : buffers 0,1 (A,B) + UBO binding 2. `count` = nb de threads (éléments
				// de sortie). Utilisé par NkGpuIm2Col / NkGpuCol2Im.
				bool RunConvOp(const char *name, const NkString &nkslSrc, uint64 in, uint64 out, const uint32 *p12,
							   uint32 count);

				// Générique 3 buffers (a,b,c) + UBO {12 uints} binding 3. Pour maxpool
				// (fwd : in/out/arg ; bwd : grad/arg/dx).
				bool RunOp3(const char *name, const NkString &nkslSrc, uint64 a, uint64 b, uint64 c, const uint32 *p12,
							uint32 count);

				// Pas d'Adam FUSÉ : un seul dispatch met à jour param/m/v EN PLACE.
				// Buffers 0,1,2,3 = param(rw), grad(r), m(rw), v(rw) + UBO binding 4.
				// `wd` = weight decay découplé (0 -> Adam ; >0 -> AdamW).
				bool RunAdam(uint64 param, uint64 grad, uint64 m, uint64 v, uint32 count, float lr, float b1, float b2,
							 float eps, float b1t, float b2t, float wd);

				void Shutdown(); // libère device + pipelines (appelé à l'arrêt)

			private:
				NkTensorGpu() = default;
				~NkTensorGpu();
				NkTensorGpu(const NkTensorGpu &) = delete;
				NkTensorGpu &operator=(const NkTensorGpu &) = delete;

				struct Impl;
				Impl *mImpl = nullptr;
				bool EnsureInit();
		};

		// Tenseur de ZEROS fabrique DIRECTEMENT sur le GPU : allocation + remise a
		// zero sur place, AUCUN transfert depuis l'hote.
		//
		// ⚠️ C'est la fonction a utiliser a la place de
		// `ToDevOf(NkTensor::Zeros(shape), ref)` : cette forme-la fabrique les zeros
		// sur CPU puis les monte, et c'est elle qui produisait 12,77 Go de trafic
		// CPU->GPU par pas d'entrainement.
		//
		// ⚠️ Ce n'est PAS `NkTensor::Zeros(shape, dtype, NK_GPU)` : le parametre
		// `device` des fabriques de NkTensor n'est pas honore (il pose `mDevice` sans
		// allouer de tampon GPU) — il echoue desormais bruyamment au lieu de mentir.
		//
		// Renvoie un tenseur INVALIDE si le GPU est indisponible, si l'allocation
		// echoue, ou si la remise a zero n'est pas realisable sur ce backend
		// (NkTensorGpu::ClearDisponible). Jamais un tampon au contenu indetermine :
		// des gradients faux ne se distinguent pas de gradients justes.
		NkTensor NkGpuZeros(const NkShape &shape, NkDType dtype = NkDType::NK_F32);

		// Ops GPU (appelées par ops::Add / ops::Matmul quand un opérande est sur GPU).
		// Déplacent au besoin les opérandes sur GPU ; renvoient un tenseur device=GPU.
		// Renvoient un tenseur invalide si le GPU est indisponible.
		NkTensor NkGpuAdd(const NkTensor &a, const NkTensor &b);	// élémentaire (mêmes formes)
		NkTensor NkGpuMatmul(const NkTensor &a, const NkTensor &b); // a[M,K] · b[K,N] -> [M,N]
		// Matmul par LOTS : a[batch,M,K] · b[batch,K,N] -> [batch,M,N] (attention transformer).
		NkTensor NkGpuBatchedMatmul(const NkTensor &a, const NkTensor &b);
		// Broadcast d'un vecteur vec[C] sur le DERNIER axe de big[..,C] (biais Dense, affine
		// LayerNorm) — reste sur GPU. out[i] = big[i] (op) vec[i%C].
		NkTensor NkGpuAddBroadcastRow(const NkTensor &big, const NkTensor &vec);
		NkTensor NkGpuMulBroadcastRow(const NkTensor &big, const NkTensor &vec);
		NkTensor NkGpuMul(const NkTensor &a, const NkTensor &b); // élémentaire A ⊙ B
		NkTensor NkGpuSub(const NkTensor &a, const NkTensor &b); // élémentaire A − B
		NkTensor NkGpuRelu(const NkTensor &a);					 // max(A,0)
		NkTensor NkGpuSigmoid(const NkTensor &a);				 // 1/(1+e^−A)
		NkTensor NkGpuTanh(const NkTensor &a);					 // tanh(A)
		NkTensor NkGpuMulScalar(const NkTensor &a, double s);	 // A · s
		NkTensor NkGpuAddScalar(const NkTensor &a, double s);	 // A + s
		NkTensor NkGpuStep(const NkTensor &a);					 // (A>0)?1:0  (masque ReLU')
		NkTensor NkGpuDiv(const NkTensor &a, const NkTensor &b); // A / B (élémentaire)
		NkTensor NkGpuSqrt(const NkTensor &a);					 // sqrt(A)  (Adam résident)
		NkTensor NkGpuGelu(const NkTensor &a);					 // GELU(A) (tanh-approx)
		NkTensor NkGpuGeluBackward(const NkTensor &x, const NkTensor &grad);

		// Entropie croisée à cible par INDICES, entièrement sur GPU. Évite de
		// rapatrier le tenseur de logits [lignes, vocabulaire] à chaque micro-lot —
		// 201 Mo dans chaque sens, deux fois par pas, avant ces deux fonctions.
		// `probs` = softmax déjà calculé ; `cibles` = un indice de classe par ligne,
		// négatif pour une ligne masquée.
		NkTensor NkGpuCeIdxForward(const NkTensor &probs, const NkTensor &cibles);	// -> pertes par ligne [B]
		NkTensor NkGpuCeIdxBackward(const NkTensor &probs, const NkTensor &cibles, double coef); // -> dLogits

		// Embedding : table[vocab,d], indices (ids en f32) -> lignes rassemblées ; backward =
		// scatter-add (gather par ligne de table, sans course).
		NkTensor NkGpuEmbedding(const NkTensor &table, const NkTensor &idx);
		NkTensor NkGpuEmbeddingBackward(const NkTensor &grad, const NkTensor &idx, int64 vocab, int64 d);

		// Pas d'Adam FUSÉ (1 seul dispatch) sur des tenseurs GPU : met à jour param, m, v
		// EN PLACE. Renvoie false si l'un n'est pas résident GPU (l'appelant reprend la
		// voie ops::). b1t = 1−β1ᵗ, b2t = 1−β2ᵗ (correction de biais, calculée côté CPU).
		bool NkGpuAdamStep(const NkTensor &param, const NkTensor &grad, const NkTensor &m, const NkTensor &v, float lr,
						   float b1, float b2, float eps, float b1t, float b2t, float wd);

		// Réductions GPU (kind : 0=Sum, 1=Mean, 2=Max). Opèrent sur un tenseur GPU
		// contigu et renvoient un tenseur GPU. Argmax reste CPU (sortie i64).
		NkTensor NkGpuReduceAll(const NkTensor &a, int kind);				// -> {1}
		NkTensor NkGpuReduceAxis(const NkTensor &a, uint32 axis, int kind); // -> shape sans l'axe

		// Rend contigu un tenseur GPU strided (permute/transpose) sans repasser CPU.
		// Renvoie un tenseur GPU contigu de même forme. Rang > 8 -> repli CPU.
		NkTensor NkGpuContiguous(const NkTensor &t);

		// Convolution comme réarrangement mémoire, sur GPU (conv résidente).
		// NkGpuIm2Col : x GPU [B,Cin,H,W] -> col GPU [M=B·outH·outW, K=Cin·kH·kW].
		// NkGpuCol2Im : col GPU [M,K] -> dx GPU [B,Cin,H,W] (accumulation, formulation
		// gather sans atomics : un thread par élément de dx).
		NkTensor NkGpuIm2Col(const NkTensor &x, int64 kH, int64 kW, int64 stride, int64 pad, int64 outH, int64 outW);
		NkTensor NkGpuCol2Im(const NkTensor &col, int64 B, int64 Cin, int64 H, int64 W, int64 kH, int64 kW,
							 int64 stride, int64 pad, int64 outH, int64 outW);

		NkTensor NkGpuExp(const NkTensor &a); // exp(A) élémentaire

		// Suréchantillonnage nearest ×2 : [B,C,H,W] -> [B,C,2H,2W] (fwd) ; backward = somme
		// des 4 sorties par entrée.
		NkTensor NkGpuUpsample2x(const NkTensor &x);
		NkTensor NkGpuUpsample2xBackward(const NkTensor &grad, int64 B, int64 C, int64 H, int64 W);

		// Convolution transposée 2D (upsampling appris). x[B,Cin,H,W], w[Cin,Cout,kH,kW]
		// -> y[B,Cout,outH,outW]. Formulations gather (sans course).
		NkTensor NkGpuConvTranspose2D(const NkTensor &x, const NkTensor &w, int64 stride, int64 pad);
		NkTensor NkGpuConvTranspose2DBackwardX(const NkTensor &grad, const NkTensor &w, int64 B, int64 Cin, int64 H,
											   int64 W, int64 Cout, int64 kH, int64 kW, int64 stride, int64 pad,
											   int64 outH, int64 outW);
		NkTensor NkGpuConvTranspose2DBackwardW(const NkTensor &x, const NkTensor &grad, int64 B, int64 Cin, int64 H,
											   int64 W, int64 Cout, int64 kH, int64 kW, int64 stride, int64 pad,
											   int64 outH, int64 outW);

		// Convolution 3D (voxels). x[B,Cin,D,H,W], w[Cout,Cin,kD,kH,kW]. Gather (sans course).
		NkTensor NkGpuConv3D(const NkTensor &x, const NkTensor &w, int64 stride, int64 pad);
		NkTensor NkGpuConv3DBackwardX(const NkTensor &grad, const NkTensor &w, const NkTensor &x, int64 stride,
									  int64 pad);
		NkTensor NkGpuConv3DBackwardW(const NkTensor &grad, const NkTensor &x, const NkTensor &w, int64 stride,
									  int64 pad);
		// Convolution transposée 3D (upsampling voxels). w[Cin,Cout,kD,kH,kW].
		NkTensor NkGpuConvTranspose3D(const NkTensor &x, const NkTensor &w, int64 stride, int64 pad);
		NkTensor NkGpuConvTranspose3DBackwardX(const NkTensor &grad, const NkTensor &w, const NkTensor &x, int64 stride,
											   int64 pad);
		NkTensor NkGpuConvTranspose3DBackwardW(const NkTensor &grad, const NkTensor &x, const NkTensor &w, int64 stride,
											   int64 pad);

		// LayerNorm sur le dernier axe (γ=1,β=0) : y=(x−μ)/√(var+ε). fwd + bwd (recalcul depuis x).
		NkTensor NkGpuLayerNormStd(const NkTensor &x);
		NkTensor NkGpuLayerNormStdBackward(const NkTensor &x, const NkTensor &grad);

		// ---- Trio NkLlamaLM : RMSNorm, RoPE, SwiGLU -------------------------
		// Ces trois operations n'avaient AUCUN chemin GPU : elles redescendaient le
		// tenseur sur le CPU et le remontaient, en avant comme en arriere. Le chemin
		// CPU reste en place et sert d'ORACLE : chaque noyau est valide par
		// equivalence numerique contre lui, avant ET arriere.
		// Signale un DEFAUT GPU : incremente le compteur que l'entrainement consulte
		// (NkTensorGpu::DefautCount) et journalise les douze premiers. Expose ici pour
		// que les couches au-dessus (NKAutograd) puissent signaler ce qu'elles seules
		// peuvent constater — par exemple une entropie croisee par ligne NULLE alors
		// que sa cible est valide, qui denonce une ligne non calculee.
		void NkGpuSignalerDefaut(const char *ou, const char *quoi, int64 valeur);

		NkTensor NkGpuRmsNorm(const NkTensor &x, double eps);
		NkTensor NkGpuRmsNormBackward(const NkTensor &x, const NkTensor &grad, double eps);

		// `table` : [T * (hd/2) * 2] en (cos, sin), construite par l'appelant EN
		// DOUBLE. Le noyau ne calcule aucun cosinus : en flottant simple l'angle
		// atteint ~256 rad, dont l'ulp (~1.5e-5) depasserait de loin la tolerance
		// d'equivalence. `sens` = +1 en avant, −1 en arriere.
		NkTensor NkGpuRoPE(const NkTensor &x, const NkTensor &table, double sens);

		NkTensor NkGpuSwiGLU(const NkTensor &gate, const NkTensor &up);
		NkTensor NkGpuSwiGLUBackwardDu(const NkTensor &gate, const NkTensor &dh);
		// `dhu` = dh ⊙ u, fourni par l'appelant : garde le noyau a deux entrees.
		NkTensor NkGpuSwiGLUBackwardDg(const NkTensor &gate, const NkTensor &dhu);

		// Softmax sur le DERNIER axe (stable). + backward (dx=y⊙(dy−Σ dy⊙y)) + variante CAUSALE
		// (masque les positions futures : dernier axe [.., T, T], requête = row % T).
		NkTensor NkGpuSoftmaxRows(const NkTensor &x);
		NkTensor NkGpuSoftmaxBackward(const NkTensor &y, const NkTensor &grad);
		NkTensor NkGpuSoftmaxCausal(const NkTensor &x);
		// Max-pooling 2D GPU. Forward : renvoie la sortie [B,C,oH,oW] et remplit `argOut`
		// (indices HW du max, en f32). Backward : redistribue le grad vers les argmax.
		NkTensor NkGpuMaxPool2D(const NkTensor &x, int64 kernel, int64 stride, NkTensor &argOut);
		NkTensor NkGpuMaxPool2DBackward(const NkTensor &grad, const NkTensor &arg, int64 B, int64 C, int64 H, int64 W,
										int64 outH, int64 outW, int64 kernel, int64 stride);

	} // namespace ai
} // namespace nkentseu
