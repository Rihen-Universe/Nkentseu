#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiImages.h
// @Brief   Le registre d'images du monteur : un NOM de fichier -> un `texId`
//          televerse une seule fois.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE CE FICHIER FERME
// =============================================================================
//  Mesure du 27/09 : `image` et `source` etaient au schema de `Image`,
//  `ImageButton` et `Tile`, les documents en ecrivaient, et les DEUX sites du
//  monteur qui les voyaient se contentaient de `++rap.attributsNonHonores`.
//  Autrement dit : une image demandee etait RECONNUE, VALIDEE, COMPTEE, et
//  jamais dessinee. `set x.image = "..."` rangeait meme la valeur dans l'etat
//  sans que personne ne la lise.
//
//  Rodolf, 27/09 : « ajoute icon et image si possible » puis « oui je le veux »
//  pour le crochet de rendu.
//
// =============================================================================
//  POURQUOI LE REGISTRE VIT ICI, ET NON DANS CHAQUE APPLICATION
// =============================================================================
//  Laisser chaque application decoder, mettre en cache et allouer ses `texId`
//  aurait donne QUATRE implementations du meme mecanisme -- NKUIDesign,
//  NkAnimaEditor, NK3DModeler, Nogee -- dont trois finiraient par diverger de la
//  premiere. C'est la faute que ce depot paie le plus souvent.
//
//  ⚠️ MAIS LE DECODAGE NE PEUT PAS VIVRE ICI, ET LA MESURE L'A TRANCHE CONTRE MOI.
//     `NKGui.jenga` declare bien `NKImage` (ligne 24), et j'en ai conclu que
//     NKGui pouvait decoder. Il ne peut pas : `NkImage.h` tire
//     `NKStream/NKIResource.h`, qui n'est pas dans ses chemins d'inclusion.
//     Le graphe de dependances impose la coupe, et l'elargir pour une image
//     aurait fait payer NKStream a tout ce qui affiche un bouton.
//
//  Le partage est donc :
//     ICI (NKGui)        le CACHE `nom -> texId`, la numerotation, les comptes.
//     `Integrations/`    le CHARGEUR : decoder avec NKImage, televerser avec
//                        `NkGuiRHIBackend`. Les deux y cohabitent deja.
//
//  Une seule implementation du chargeur pour les quatre applications, et zero
//  dependance nouvelle dans le noyau. Meme partage que les polices et les
//  icones : le kit tient la table, le dorsal envoie les pixels.
//
// =============================================================================
//  ⚠️ LES NUMEROS DE TEXTURE NE SE CHOISISSENT PAS AU HASARD
// =============================================================================
//  L'espace des `texId` est DEJA partage, et une collision ne provoque aucune
//  erreur : elle dessine simplement la mauvaise texture, ce qui se remarque des
//  semaines plus tard sur une capture.
//
//      0        l'atlas de la police
//      16..     les icones
//      4096     le viewport 3D hors-ecran (`nk3d::kViewportTexId`, choisi « loin
//               de la police (0) et des icones (16..) » -- son commentaire le dit)
//      8192..   LES IMAGES, ce fichier
//
//  8192 laisse quatre mille numeros au viewport pour qu'il en prenne d'autres
//  (plusieurs vues 3D dans une fenetre : c'est prevu).
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKGUI_DOC_NKGUIIMAGES_H__
#define __NKENTSEU_NKGUI_DOC_NKGUIIMAGES_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKGui/Core/NkGuiTypes.h" // NkVec2 : la MEME porte que la draw-list

namespace nkentseu {
	namespace nkgui {

		/// Le premier numero de texture des images. Voir l'en-tete : cet espace est
		/// partage, et une collision ne CRIE PAS.
		constexpr uint32 kNkGuiImageTexId0 = 8192u;

		/// Ce que le chargeur doit faire : lire le fichier `nom`, le decoder, et
		/// faire en sorte que le dorsal serve ses pixels sous `texId`. Il rend la
		/// taille en pixels -- c'est elle qui donne les proportions, et sans elle une
		/// image serait etiree pour remplir son rectangle.
		///
		/// ⚠️ C'EST LE SEUL MORCEAU A ECRIRE HORS DU NOYAU, il est ecrit UNE FOIS
		///    (`Integrations/NKGui/NkGuiImageLoader.h`) et les applications ne font
		///    que l'installer. Tout le reste -- cache, numerotation, comptes,
		///    marqueur visible -- est ici et se partage.
		using NkGuiChargeurImage = bool (*)(const char *nom, uint32 texId, NkVec2 &tailleOut,
											void *user);

		void NkGuiPoserChargeurImage(NkGuiChargeurImage fn, void *user) noexcept;
		bool NkGuiChargeurImagePose() noexcept;

		/// Ce que le registre a fait, pour que rien ne se perde en silence.
		struct NkGuiImagesRapport {
				uint32 demandees = 0;	 ///< appels a `Resoudre`
				uint32 servies = 0;		 ///< un `texId` valide a ete rendu
				uint32 duCache = 0;		 ///< servies sans relire le fichier
				uint32 introuvables = 0;  ///< le fichier n'existe pas / ne se decode pas
				uint32 sansChargeur = 0; ///< aucun chargeur pose : rien ne peut monter
				/// Le premier nom qui a manque. « il manque une image » sans dire
				/// LAQUELLE renvoie a chercher dans trente documents.
				NkString premiereIntrouvable;
		};

		/// Resout un nom en numero de texture, en televersant au premier appel.
		///
		/// Le nom est un chemin, relatif a la racine d'execution comme tout le reste
		/// des ressources (`Resources/...`).
		///
		/// ⚠️ UN ECHEC N'EST PAS UNE EXCEPTION NI UN ZERO SILENCIEUX : la fonction
		///    rend `false`, le rapport compte, et le monteur peint un marqueur
		///    visible. Un carre muet a la place d'une image est precisement ce que
		///    ce dépot a deja paye sur les icones.
		bool NkGuiResoudreImage(const char *nom, uint32 &texIdOut, NkVec2 &tailleOut) noexcept;

		/// Le relevé courant, et sa remise a zero (une par image de rendu, comme les
		/// autres compteurs du monteur).
		const NkGuiImagesRapport &NkGuiImagesReleve() noexcept;
		void NkGuiImagesRemiseAZero() noexcept;

		/// Vide le cache ET libere les numeros. A appeler quand le dorsal est
		/// recree : les `texId` d'avant ne designent plus rien.
		///
		/// ⚠️ SANS ELLE, UN CHANGEMENT DE DORSAL LAISSE LE CACHE MENTIR. Il dirait
		///    « deja televersee » pour des numeros que le nouveau dorsal ne connait
		///    pas, et les images disparaitraient sans que rien ne le signale.
		void NkGuiImagesOublier() noexcept;

	} // namespace nkgui
} // namespace nkentseu

#endif // __NKENTSEU_NKGUI_DOC_NKGUIIMAGES_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
