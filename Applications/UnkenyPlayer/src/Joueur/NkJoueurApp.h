//
// NkJoueurApp.h
// =============================================================================
// Description :
//   Le JOUEUR AUTONOME d'Unkeny : l'application qu'on livre. Elle relit le jeu
//   cuit par l'editeur (Fichier > Construire) et le JOUE, avec le code meme
//   de « Jouer » dans l'editeur (Unkeny/Partie/NkUnkenyPartie.h).
//
// Caracteristiques :
//   - Batie sur renderer::NkCanvasGuiApp, comme l'editeur : meme fenetre, meme
//     rendu, meme cycle de vie mobile (pause, surface perdue et refaite).
//   - Le jeu est lu dans un DOSSIER DE DONNEES (NkChargerJeu) : a cote de
//     l'executable sur ordinateur (`assets/`), dans l'APK sur Android, dans le
//     systeme de fichiers prechargé sur le Web. `--jeu=DOSSIER` le remplace.
//   - Une ressource absente n'est jamais un ecran noir : elle est NOMMEE a
//     l'ecran, par-dessus la scene si la scene a pu se charger (temoin l2).
//   - La fenetre montre le MEME MORCEAU DE MONDE que le viseur de l'editeur au
//     moment de la cuisson, quelle que soit sa taille (le zoom suit).
//
// LANCER
//   UnkenyPlayer [--jeu=DOSSIER] [--verifier] [--selftest] [--capture=IMAGE.png]
//                [--zone-sure] [--marges=H,B,G,D]
//     --jeu=DOSSIER  le dossier des donnees (celui qui contient jeu.json)
//     --verifier     relit le jeu SANS fenetre, dit ce qui manque et si
//                    l'empreinte est celle de l'editeur ; code 0 = tout tient
//     --selftest     le banc de la livraison (Unkeny/Banc/NkUnkenyBancLivraison)
//     --zone-sure    surimpression de la zone sure que NKWindow donne au jeu
//     --marges=...   des marges d'ESSAI (pixels) a la place de celles de NKWindow
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYPLAYER_NKJOUEURAPP_H__
#define __NKENTSEU_UNKENYPLAYER_NKJOUEURAPP_H__

#include "Joueur/NkJoueurEntrees.h"

#include "NKCanvas/App/NkCanvasGuiApp.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkOptional.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Livraison/NkUnkenyLivraison.h"
#include "Unkeny/Rendu/NkUnkenyRenduParticules.h"
#include "Unkeny/Rendu/NkUnkenyTextures.h"
#include "Unkeny/Scene/NkUnkenyScene.h"
#include "Unkeny/Son/NkUnkenySon.h"

namespace nkentseu {
	namespace joueur {

		/// Le dossier des donnees quand `--jeu=` n'est pas donne, barre finale
		/// comprise : `<dossier de l'executable>/assets/` sur ordinateur, vide
		/// sur Android et HarmonyOS (NkFile lit alors dans le paquet).
		NkString NkDossierDuJeuParDefaut();

		/// L'etat du jeu en cours. Sur le TAS : `Run<T>` construit l'application
		/// sur la pile, et une scene porte un monde ECS et deux mondes physiques.
		struct NkPartieJouee {
				unkeny::NkScene scene;
				unkeny::NkTextures2D textures;
				unkeny::NkSons2D sons;
				unkeny::NkJeuCharge jeu;
				unkeny::NkOptionsRenduParticules rendu;
				NkJoueurEntrees entrees;
				/// Le zoom relu dans la scene : celui du viseur de l'editeur, pour
				/// une fenetre de jeu.vueLargeur x jeu.vueHauteur.
				float32 zoomRelu = 32.f;
				bool pause = false;
		};

		class NkJoueurApp : public renderer::NkCanvasGuiApp {
			public:
				NkJoueurApp();

			protected:
				NkOptional<int> OnCommandLine(const NkVector<NkString> &args) override;
				bool OnGuiInit() override;
				void OnLayout(const renderer::NkLayoutInfo &info) override;
				void OnTick(float32 deltaTime) override;
				void OnDraw(nkgui::NkGuiDrawList &dl) override;
				bool OnEvent(const NkEvent &event) override;
				void OnPause() override;
				void OnResume() override;
				void OnShutdown() override;

			private:
				/// Le panneau qui NOMME ce qui manque (ou ce qui empeche de jouer).
				void DessinerManques(nkgui::NkGuiDrawList &dl, const nkgui::NkRect &ecran);

				memory::NkUniquePtr<NkPartieJouee> mPartie;
				NkString mDossier;
				/// --zone-sure, --marges= (2026-10-01) : voir OnCommandLine.
				bool mVoirZoneSure = false;
				bool mAMargesEssai = false;
				NkSafeAreaInsets mMargesEssai;
		};

		/// `--verifier` : relit le jeu du dossier sans fenetre ni GPU, ecrit le
		/// rapport sur la sortie standard. 0 = jouable, rien ne manque, et
		/// l'empreinte est celle de l'editeur.
		int32 NkJoueurVerifier(const NkString &dossier);

	} // namespace joueur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYPLAYER_NKJOUEURAPP_H__
