#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamilleContenu.h
// @Brief   LE TIROIR « CONTENU » de la famille : le Content Browser d'Unreal du
//          kit (NkDrawContentBrowser, variante `unreal`, sept zones) BRANCHE sur
//          un dossier du disque -- le rail (Favoris, le projet et son arbre
//          « Contenu », Collections), le fil d'Ariane « Tout > Contenu > ... »,
//          les cartes des dossiers puis des fichiers, les puces de nature, la
//          navigation (rail, miettes, double-clic, precedent / suivant).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI ICI : le COMPOSANT est deja partage (NkContentBrowserModel.h) ; son
// BRANCHEMENT sur un dossier, lui, etait ecrit dans UnkenyEditor seul
// (NkEditeurTiroir.cpp, avec ses gestes propres : collections, couper-coller,
// renommage...). Celui-ci est le branchement MINIMAL commun : lire, montrer,
// naviguer, ouvrir. Une application lui donne ses NATURES de fichiers (sa
// touche : scenes .nkscene, maillages .obj/.gltf, materiaux...) et recoit le
// double-clic d'un fichier.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleStyle.h"
#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editorkit {

		/// Une nature de fichier : son extension (minuscules, avec le point), son
		/// libelle (pied de carte et puce), son role de couleur, son icone du kit.
		struct NkFamilleNatureFichier {
				const char *extension = "";
				const char *libelle = "";
				NkRole role = NkRole::TextMuted;
				uint8 icone = 0; ///< NkAssetIcone
		};

		struct NkFamilleContenu {
				NkContentBrowserModel modele;
				NkComponentInstance reglages;
				bool pret = false;
				/// Le dossier du disque montre sous « Contenu ».
				NkString racine = ".";
				/// Le titre de la section du projet dans le rail (Unreal : le nom du projet).
				NkString nomProjet = "Projet";
				/// Le dossier courant, RELATIF a `racine` (« » = sa racine).
				NkString courant;
				/// Le fil d'Ariane est sur « Tout » (les racines en cartes).
				bool tout = false;
				/// Les sous-dossiers (relatifs, ordre prefixe), relus au plus une fois par seconde.
				NkVector<NkString> sousDossiers;
				float32 age = 99.f;
				bool perime = true;
				/// Le dernier fichier ouvert (double-clic), chemin du DISQUE ; consomme par l'application.
				NkString ouvert;
				/// Le chemin (navigateur) de la carte active, garde d'une trame a l'autre.
				NkString actif;

				/// Va au dossier relatif `rel` (« » = la racine du Contenu).
				void Aller(const NkString &rel);
				/// Le chemin du DISQUE d'un chemin du navigateur (« Contenu/a/b »).
				NkString Disque(const char *cheminNavigateur) const;
		};

		/// Le tiroir dans `zone`. Rend vrai si un fichier vient d'etre ouvert
		/// (double-clic) : son chemin est dans `contenu.ouvert`.
		bool NkFamilleDessinerContenu(NkFamilleCtx &c, const nkgui::NkRect &zone, NkFamilleContenu &contenu,
									  const NkFamilleNatureFichier *natures, int32 nbNatures, float32 dt);

	} // namespace editorkit
} // namespace nkentseu
