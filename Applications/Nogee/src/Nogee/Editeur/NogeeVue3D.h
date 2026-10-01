#pragma once
// -----------------------------------------------------------------------------
// @File    NogeeVue3D.h
// @Brief   LA VUE 3D AU CENTRE de Nogee : un SECOND NkRenderer, sur le device de
//          l'application, qui rend la scene du moteur (NkEngineLayer) dans une
//          cible HORS ECRAN de la taille du viseur ; l'interface l'affiche comme
//          une image.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI UN SECOND RENDERER, ET PAS LA CIBLE FINALE DU PREMIER
//   Le renderer de NkApplication termine son graphe par la passe Overlay2D, ou
//   l'interface se dessine. Rediriger sa sortie finale (SetFinalColorTarget)
//   vers une texture y emmenerait AUSSI l'interface : plus rien n'atteindrait
//   l'ecran. Le chemin de NKCraft (NkViewport3D.cpp, NkMatPreview3D.h) est
//   donc repris tel quel : un renderer DEDIE a la vue, sortie redirigee, dont on
//   EXECUTE le graphe dans le command buffer de l'image -- avant la passe
//   d'interface du renderer de l'application, qui lit la texture.
//
//   Ce que BeginFrame ferait pour lui (il ne l'appelle jamais : la frame est a
//   l'application) est rejoue par `PreparerTrame` : graphe en attente,
//   ResetFrame, upload des materiaux.
//
// ⚠️ Ce fichier ne montre AUCUN type de NKRenderer (seulement NKRHI) : l'unite
//    de l'interface l'inclut sans tirer NKRenderer.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {

	namespace renderer {
		class NkRenderer;
	}

	namespace nogee {

		class NogeeVue3D {
			public:
				NogeeVue3D() = default;
				NogeeVue3D(const NogeeVue3D &) = delete;
				NogeeVue3D &operator=(const NogeeVue3D &) = delete;

				/// Cree le renderer de la vue et sa cible (w x h). Faux = pas de vue.
				bool Init(NkIDevice *device, uint32 w, uint32 h);
				void Shutdown();
				bool Pret() const noexcept {
					return mRendu != nullptr && mCible != nullptr;
				}
				renderer::NkRenderer *Rendu() noexcept {
					return mRendu;
				}

				/// AVANT les soumissions de la trame : la taille (refait la cible si
				/// elle change), le graphe en attente, la remise a zero de Render3D.
				void PreparerTrame(uint32 w, uint32 h);
				/// APRES les soumissions : execute le graphe dans `cmd`.
				void Executer(NkICommandBuffer *cmd);

				/// La couleur finale de la vue (a publier aupres du backend NKGui).
				NkTextureHandle Texture() const noexcept;
				/// Change quand la cible est refaite : l'interface republie la texture.
				uint32 Generation() const noexcept {
					return mGeneration;
				}
				uint32 Largeur() const noexcept {
					return mW;
				}
				uint32 Hauteur() const noexcept {
					return mH;
				}

			private:
				NkIDevice *mDevice = nullptr;
				renderer::NkRenderer *mRendu = nullptr;
				void *mCible = nullptr; ///< renderer::NkOffscreenTarget (masque ici)
				uint32 mW = 0, mH = 0;
				uint32 mGeneration = 0;
		};

	} // namespace nogee
} // namespace nkentseu
