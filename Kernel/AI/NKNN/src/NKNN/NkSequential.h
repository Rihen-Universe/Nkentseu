// =============================================================================
// NkSequential.h — conteneur de modèle GÉNÉRIQUE (NKAI).
//
// Absent du code jusqu'ici (audit 2026-07-26) : chaque modèle concret (NkGPT,
// NkTransformerBlock, NKConvTest, NKNNTest...) composait ses couches "à la main"
// dans son propre constructeur/forward — aucune classe réutilisable ne chaînait
// une LISTE de couches hétérogènes existantes en un seul forward.
//
// NkSequential chaîne une suite de "briques" (Dense/Conv2D/Dropout/LayerNorm/
// fonctions pures comme les activations ou MaxPool2D/Flatten) en un seul
// Forward(x) = layerN(...layer1(x)). Chaque brique est possédée (allocation via
// NKMemory, comme le reste du moteur — jamais new/delete bruts) derrière une
// petite interface polymorphe `NkISeqLayer`.
//
// « Zéro STL » : pas de std::function ni de std::vector. L'hétérogénéité vient du
// polymorphisme C++ standard (classe de base + virtuelles, comme `NkTrainCallback`/
// `NkSystem` ailleurs dans le moteur) ; le stockage est un `NkVector<NkISeqLayer*>`
// (NKContainers) et l'allocation passe par `memory::NkGetDefaultAllocator()` (le
// même idiome que `NkMaterialSystem` : New<T>() + PushBack, Delete() + Clear() au
// nettoyage). Les couches "fonctionnelles" (activations, pooling, flatten) sont
// enveloppées via un adaptateur TEMPLATE sur le type de lambda (`NkSeqFn<TFn>`) —
// donc un type concret par lambda, sans érasure de type façon std::function.
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NkAllocator.h"
#include "NKCore/NkTraits.h"

#include "NKNN/NkDense.h"
#include "NKNN/NkConv.h"
#include "NKNN/NkDropout.h"
#include "NKNN/NkTransformer.h" // NkLayerNorm

namespace nkentseu {
	namespace ai {
		namespace nn {

			// -------------------------------------------------------------------
			// Interface commune : toute brique assemblable dans NkSequential.
			// -------------------------------------------------------------------
			class NkISeqLayer {
				public:
					virtual ~NkISeqLayer() = default;

					virtual NkVar Forward(const NkVar &x) = 0;

					// Par défaut : pas de paramètre entraînable (couches fonctionnelles).
					virtual void Parameters(NkVector<NkVar> & /*out*/) const {
					}

					// Par défaut : pas de mode entraînement/évaluation (seul NkDropout en a un).
					virtual void SetTraining(bool /*training*/) {
					}
			};

			// ---- Adaptateurs concrets pour les couches EXISTANTES ---------------
			class NkSeqDense : public NkISeqLayer {
				public:
					explicit NkSeqDense(const NkDense &layer) : mLayer(layer) {
					}

					NkVar Forward(const NkVar &x) override {
						return mLayer.Forward(x);
					}

					void Parameters(NkVector<NkVar> &out) const override {
						mLayer.Parameters(out);
					}

				private:
					NkDense mLayer;
			};

			class NkSeqConv2D : public NkISeqLayer {
				public:
					explicit NkSeqConv2D(const NkConv2D &layer) : mLayer(layer) {
					}

					NkVar Forward(const NkVar &x) override {
						return mLayer.Forward(x);
					}

					void Parameters(NkVector<NkVar> &out) const override {
						mLayer.Parameters(out);
					}

				private:
					NkConv2D mLayer;
			};

			class NkSeqLayerNorm : public NkISeqLayer {
				public:
					explicit NkSeqLayerNorm(const NkLayerNorm &layer) : mLayer(layer) {
					}

					NkVar Forward(const NkVar &x) override {
						return mLayer.Forward(x);
					}

					void Parameters(NkVector<NkVar> &out) const override {
						mLayer.Parameters(out);
					}

				private:
					NkLayerNorm mLayer;
			};

			class NkSeqDropout : public NkISeqLayer {
				public:
					explicit NkSeqDropout(const NkDropout &layer) : mLayer(layer) {
					}

					NkVar Forward(const NkVar &x) override {
						return mLayer.Forward(x);
					}

					void SetTraining(bool training) override {
						mLayer.SetTraining(training);
					}

				private:
					NkDropout mLayer;
			};

			// Couche "fonctionnelle" générique : enveloppe une closure NkVar(const NkVar&)
			// sans paramètre entraînable (activation, MaxPool2D, Flatten, Reshape...).
			// Un type concret PAR closure (déduit), pas d'érasure de type dynamique.
			template <typename TFn> class NkSeqFn : public NkISeqLayer {
				public:
					explicit NkSeqFn(TFn fn) : mFn(fn) {
					}

					NkVar Forward(const NkVar &x) override {
						return mFn(x);
					}

				private:
					TFn mFn;
			};

			// -------------------------------------------------------------------
			// NkSequential — chaîne les briques en un seul Forward.
			// -------------------------------------------------------------------
			class NkSequential {
				public:
					NkSequential() = default;

					~NkSequential() {
						Clear();
					}

					// Non copiable (possède des pointeurs bruts vers des couches allouées) ;
					// déplaçable (transfert simple de la propriété du vecteur de pointeurs).
					NkSequential(const NkSequential &) = delete;
					NkSequential &operator=(const NkSequential &) = delete;
					NkSequential(NkSequential &&o) noexcept : mLayers(static_cast<NkVector<NkISeqLayer *> &&>(o.mLayers)) {
					}

					NkSequential &operator=(NkSequential &&o) noexcept {
						if (this != &o) {
							Clear();
							mLayers = static_cast<NkVector<NkISeqLayer *> &&>(o.mLayers);
						}
						return *this;
					}

					void AddDense(const NkDense &layer) {
						AddRaw(memory::NkGetDefaultAllocator().New<NkSeqDense>(layer));
					}

					void AddConv2D(const NkConv2D &layer) {
						AddRaw(memory::NkGetDefaultAllocator().New<NkSeqConv2D>(layer));
					}

					void AddLayerNorm(const NkLayerNorm &layer) {
						AddRaw(memory::NkGetDefaultAllocator().New<NkSeqLayerNorm>(layer));
					}

					void AddDropout(const NkDropout &layer) {
						AddRaw(memory::NkGetDefaultAllocator().New<NkSeqDropout>(layer));
					}

					// Ajoute une brique fonctionnelle (activation/pooling/flatten/...) depuis
					// une lambda `NkVar(const NkVar&)`. Ex : seq.AddFn([](const NkVar &x){ return
					// nn::Relu(x); });
					template <typename TFn> void AddFn(TFn fn) {
						AddRaw(memory::NkGetDefaultAllocator().New<NkSeqFn<TFn>>(fn));
					}

					NkVar Forward(const NkVar &x) {
						NkVar h = x;
						for (uint32 i = 0; i < mLayers.Size(); ++i)
							h = mLayers[i]->Forward(h);
						return h;
					}

					void Parameters(NkVector<NkVar> &out) const {
						for (uint32 i = 0; i < mLayers.Size(); ++i)
							mLayers[i]->Parameters(out);
					}

					// Propage le mode entraînement/évaluation à toutes les briques (seules
					// celles qui en ont un, ex. NkDropout, en tiennent compte).
					void SetTraining(bool training) {
						for (uint32 i = 0; i < mLayers.Size(); ++i)
							mLayers[i]->SetTraining(training);
					}

					uint32 Size() const {
						return (uint32)mLayers.Size();
					}

					void Clear() {
						for (uint32 i = 0; i < mLayers.Size(); ++i)
							memory::NkGetDefaultAllocator().Delete(mLayers[i]);
						mLayers.Clear();
					}

				private:
					void AddRaw(NkISeqLayer *layer) {
						mLayers.PushBack(layer);
					}

					NkVector<NkISeqLayer *> mLayers;
			};

		} // namespace nn
	} // namespace ai
} // namespace nkentseu
