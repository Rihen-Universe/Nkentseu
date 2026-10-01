//
// NogeDemoApp.h
// =============================================================================
// Description :
//   La fenetre de NogeDemo : une NkApplication (Noge) qui pousse NkEngineLayer
//   -- donc les systemes du moteur, le rendu 3D et la carte d'entree du noyau
//   -- puis joue NogeDemoJeu a chaque image.
//
//   `--capture=IMAGE.png` : la meme application, fenetre HORS ECRAN et sans
//   focus, qui rend la scene dans une cible hors ecran (NkOffscreenTarget),
//   ecrit l'image, VERIFIE des pixels (le cube rouge, la balle jaune, le sol)
//   puis sort avec le verdict. Les vraies entrees y sont IGNOREES : elle ne
//   peut ni recevoir ni fabriquer un geste de l'utilisateur.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NOGEDEMO_NOGEDEMOAPP_H__
#define __NKENTSEU_NOGEDEMO_NOGEDEMOAPP_H__

#include "NogeDemoJeu.h"

#include "Noge/Core/NkApplication.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"

namespace nkentseu {

	class NkEngineLayer;

	namespace nogedemo {

		struct NogeDemoOptions {
				NkString cheminScene = "Build/NogeDemo/scene.nkscene";
				NkString cheminEntrees = "Build/NogeDemo/entrees.nkinput";
				/// Non vide : mode capture (voir l'en-tete).
				NkString capture;
				/// Temps de SIMULATION avant la capture : la balle est posee.
				float32 tempsCapture = 3.f;
				/// Au demarrage : charger la scene sauvegardee plutot que la neuve.
				bool chargerAuDepart = false;
		};

		class NogeDemoApp final : public NkApplication {
			public:
				NogeDemoApp(const NkApplicationConfig &config, const NogeDemoOptions &options);

				/// 0 = tout tient (en capture : les pixels sont ceux attendus).
				[[nodiscard]] int32 CodeSortie() const noexcept {
					return mCodeSortie;
				}

			protected:
				void OnInit() override;
				void OnUpdate(float dt) override;
				void OnShutdown() override;

			private:
				void ExecuterDemandes();
				void PiloterCapture();
				void VerifierCapture();

				NogeDemoOptions mOptions;
				NkEngineLayer *mCouche = nullptr; ///< possedee par la pile de couches
				NkDemoEtat mEtat;
				float32 mTemps = 0.f;
				int32 mCodeSortie = 0;

				// --- La capture ---------------------------------------------------
				renderer::NkOffscreenTarget mCible;
				int32 mPhase = 0;			 ///< 0 attente, 1 armee, 2 lue (joueur visible), 3 lue (cache), 4 fini
				int32 mImagesDepuisPhase = 0;
				NkVector<uint8> mPixelsVisible; ///< RGBA8, rangees de HAUT en bas
				NkVector<uint8> mPixelsCache;
				math::NkMat4f mVueProj;			///< celle du rendu de l'image lue
				uint32 mLargeur = 0;
				uint32 mHauteur = 0;
		};

	} // namespace nogedemo
} // namespace nkentseu

#endif // __NKENTSEU_NOGEDEMO_NOGEDEMOAPP_H__
