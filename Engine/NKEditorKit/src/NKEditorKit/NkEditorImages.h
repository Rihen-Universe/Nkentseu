#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorImages.h
// @Brief   LE chargeur d'images des documents `.nkgui` : decoder avec NKImage,
//          televerser par le renderer du kit. Ecrit une fois, installe partout.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI IL EST ICI -- ET PAS DANS NKGui, NI DANS `Integrations/`, NI DANS
//  CHAQUE APPLICATION
// =============================================================================
//  Il lui faut deux choses : DECODER (NKImage) et TELEVERSER.
//
//  - NKGui ne peut pas decoder : `NkImage.h` tire `NKStream/NKIResource.h`, hors
//    de ses chemins d'inclusion (mesure du 27/09 -- elle a corrige ma premiere
//    conception, qui s'appuyait sur la seule ligne `NKImage` du jenga).
//  - `Integrations/NKGui/` aurait marche, mais contre `NkGuiRHIBackend` -- donc
//    seulement pour les applications 3D. **NKUIDesign rend en NKCanvas**, pas en
//    NKRHI : son chargeur n'aurait rien televerse, et le defaut se serait vu au
//    premier document a image, pas avant.
//  - Chaque application pourrait, et c'est bien le probleme : quatre copies dont
//    trois divergeraient.
//
//  NKEditorKit est le bon etage : il declare deja `NKImage`, et il possede
//  `NkIEditorRenderer::UploadImageRGBA` -- **la porte commune aux deux
//  renderers**, celui de NKCanvas comme celui de NKRHI. Un seul chargeur pour
//  les applications 2D et 3D.
//
// =============================================================================
//  ⚠️ DEUX ALLOCATEURS DE `texId` COEXISTENT, ET CE N'EST PAS UN OUBLI
// =============================================================================
//  Le kit alloue les siens depuis `0x4E4B0100` (1 313 046 784 -- le « NK » de
//  l'hexadecimal dit que le choix etait deliberé) pour les logos et les icones
//  de l'application. Le registre de NKGui alloue les images de documents depuis
//  **8192**. Les deux plages ne se rencontrent pas, et c'etait a verifier avant
//  d'ecrire ce fichier : une collision de `texId` ne provoque AUCUNE erreur --
//  elle dessine simplement la mauvaise texture, ce qui se decouvre des semaines
//  plus tard sur une capture.
//
//  On passe donc par `UpdateRGBA(texId, ...)`, qui televerse SOUS LE NUMERO
//  DONNE, et non par `UploadRGBA(...)`, qui en alloue un de son cote -- ce qui
//  aurait fait deux verites sur le numero d'une meme image.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKEDITORKIT_IMAGES_H__
#define __NKENTSEU_NKEDITORKIT_IMAGES_H__

#include "NKEditorKit/NkEditorShell.h"
#include "NKGui/Doc/NkGuiImages.h"
#include "NKImage/Core/NkImage.h"

namespace nkentseu {
	namespace nkgui {

		/// Conforme a `NkGuiChargeurImage`. `user` = le `NkEditorShell`.
		inline bool NkEditorChargerImage(const char *nom, uint32 texId, NkVec2 &tailleOut,
										 void *user) noexcept {
			auto *shell = (editorkit::NkEditorShell *)user;
			if (!shell || !nom || !*nom) {
				return false;
			}
			NkImage img;
			// QUATRE CANAUX DEMANDES. Le televersement ne connait que RGBA8 : une
			// image a trois canaux envoyee telle quelle se dessinerait avec un
			// decalage d'un octet par pixel -- des couleurs fausses, jamais un
			// plantage, donc un defaut qui survit a tous les bancs.
			if (!img.Load(nom, 4) || img.Width() <= 0 || img.Height() <= 0 || !img.Pixels()) {
				return false;
			}
			if (!shell->UpdateRGBA(texId, img.Pixels(), img.Width(), img.Height())) {
				return false;
			}
			tailleOut = NkVec2((float32)img.Width(), (float32)img.Height());
			return true;
		}

		/// A appeler APRES que la coquille ait son renderer, une fois.
		///
		/// ⚠️ APRES, ET L'ORDRE SE VOIT DANS LE RELEVE. Installe trop tot, rien ne
		///    casse : les images demandees avant se comptent `sansChargeur` dans
		///    `NkGuiImagesReleve()` et ne sont PAS memorisees comme fautives -- elles
		///    se rechargeront. C'etait le but de ce compteur-la : distinguer « le
		///    fichier n'existe pas » de « l'hote n'etait pas pret », qui ne se
		///    corrigent pas du tout de la meme facon.
		inline void NkEditorInstallerChargeurImage(editorkit::NkEditorShell *shell) noexcept {
			NkGuiPoserChargeurImage(&NkEditorChargerImage, shell);
		}

	} // namespace nkgui
} // namespace nkentseu

#endif // __NKENTSEU_NKEDITORKIT_IMAGES_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
