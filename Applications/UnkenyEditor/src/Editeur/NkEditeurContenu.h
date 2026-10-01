//
// NkEditeurContenu.h
// =============================================================================
// Description :
//   Le CONTENU DU PROJET, tel que le navigateur le montre : le dossier
//   « Contenu » a cote de la scene, ses sous-dossiers, ses fichiers et leur
//   nature ; l'IMPORT (copier des fichiers de l'OS dans le dossier courant)
//   et l'EXPORT (copier des assets choisis vers un dossier de l'OS).
//
// Caracteristiques :
//   - PUR, sans fenetre ni NKGui : le banc importe et exporte de vrais
//     fichiers (temoin e49). Le selecteur de fichiers (celui de NKEditorKit,
//     NkEditeurSelecteur.h) et le depot de l'OS (NkDropFileEvent) ne font que
//     fournir des chemins.
//   - IMPORTER = COPIER (CONVENTIONS_FICHIERS.md, « Import : COPIER ») : le
//     fichier source est copie tel quel, le moteur lit les sources (PNG, WAV,
//     TTF...) par leur chemin. Sa NATURE vient de la table existante :
//     NkAssetTypeFromExtension pour les formats du projet (.nkscene,
//     .nkprefab...), NkAssetImporter::DetectType pour les sources. Aucune
//     table n'est recopiee ici.
//   - Un nom deja pris n'est JAMAIS ecrase : « caisse.png » devient
//     « caisse_2.png ». Un import ne detruit pas l'existant.
//   - Aucune arborescence imposee (meme document) : la destination est le
//     dossier COURANT du navigateur, la racine « Contenu » a defaut.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURCONTENU_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURCONTENU_H__

#include "Editeur/NkEditeurModele.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"

namespace nkentseu {
	namespace editeur {

		/// Le nom du dossier du contenu, et la racine de ses chemins dans le
		/// navigateur (« Contenu/Textures/caisse.png »).
		constexpr const char *NK_CONTENU_RACINE = "Contenu";

		/// Ce que le navigateur montre de la nature d'un fichier.
		struct NkNatureContenu {
				NkAssetType type = NkAssetType::Unknown;
				const char *libelle = "Fichier"; ///< « Texture », « Son »... (litteral statique)
				uint8 icone = 0;				 ///< editorkit::NkAssetIcone
				uint16 role = 0;				 ///< editorkit::NkRole de la nature
				/// Un format que l'editeur sait IMPORTER (image, son, police, scene,
				/// prefab...). Un format inconnu est refuse, et l'annonce le dit.
				bool importable = false;
		};

		/// La nature d'un fichier, lue a son EXTENSION (l'en-tete des formats du
		/// projet reste la verite ; l'extension est un indice).
		NkNatureContenu NkEditeurNatureFichier(const char *chemin) noexcept;

		/// Le dossier « Contenu » du projet : a cote de la scene (NkEditeurChemin).
		/// Sans separateur final ; PAS cree (l'import le cree).
		NkString NkEditeurDossierContenu(NkEditeurModele &m);

		/// Le chemin ABSOLU d'un chemin du navigateur (« Contenu/a/b.png »,
		/// « Contenu », ou relatif « a/b.png »).
		NkString NkEditeurCheminContenu(NkEditeurModele &m, const char *cheminNavigateur);

		/// Un chemin du navigateur est-il dans le Contenu (« Contenu », « Contenu/... ») ?
		bool NkEditeurCheminEstContenu(const char *cheminNavigateur) noexcept;

		/// Le chemin RELATIF (depuis Contenu) d'un chemin du navigateur.
		NkString NkEditeurRelatifContenu(const char *cheminNavigateur);

		/// Un element d'un dossier du contenu.
		struct NkElementContenu {
				NkString nom;	   ///< « caisse.png »
				NkString relatif;  ///< « Textures/caisse.png » (depuis Contenu, « / »)
				bool dossier = false;
				NkNatureContenu nature;
				nk_int64 taille = 0;
				nk_int64 date = 0; ///< (2026-10-01) derniere modification (epoch) : le tri par date
		};

		/// Les elements du dossier `relatif` (« » = la racine) : dossiers
		/// d'abord, puis fichiers, chacun par nom. Les caches (« . ») sont tus.
		void NkEditeurListerContenu(NkEditeurModele &m, const char *relatif, NkVector<NkElementContenu> &sortie);

		/// Tous les sous-dossiers du contenu, RELATIFS, en ordre prefixe (un
		/// parent avant ses enfants) : le rail du navigateur. Borne a `maxi`.
		void NkEditeurDossiersContenu(NkEditeurModele &m, NkVector<NkString> &sortie, uint32 maxi = 256u);

		/// Ce qu'un import a fait.
		struct NkRapportImport {
				uint32 importes = 0; ///< FICHIERS copies (ceux des dossiers deposes compris)
				uint32 refuses = 0; ///< format inconnu, ou pas un fichier
				uint32 echecs = 0;	///< la copie a echoue (droits, disque)
				/// Les chemins du NAVIGATEUR des fichiers crees (« Contenu/... »).
				NkVector<NkString> crees;
				/// (2026-10-01) Les DOSSIERS crees : un dossier depose est recopie avec
				/// son arborescence (il etait refuse avant). AJOUTE A LA FIN.
				uint32 dossiers = 0;
		};

		/// Copie `sources` (chemins ABSOLUS de l'OS, fichiers OU DOSSIERS) dans le
		/// dossier du contenu `relatif` (« » = la racine), en gardant leur nom (sans
		/// jamais ecraser). Un dossier est recopie avec son arborescence ; ses
		/// fichiers de format inconnu sont refuses, un a un. Annonce le resultat.
		NkRapportImport NkEditeurImporter(NkEditeurModele &m, const char *relatif, const NkVector<NkString> &sources);

		/// Ce qu'un export a fait.
		struct NkRapportExport {
				uint32 exportes = 0;
				uint32 echecs = 0;
		};

		/// Copie les assets `chemins` (du navigateur, « Contenu/... ») dans le
		/// dossier ABSOLU `destination` (sans ecraser : « _2 » au besoin). Un
		/// dossier choisi n'est pas exporte (un asset, oui). Annonce le resultat.
		NkRapportExport NkEditeurExporter(NkEditeurModele &m, const NkVector<NkString> &chemins, const char *destination);

		/// Le filtre du dialogue « Importer » : les motifs des formats connus,
		/// separes par « ; » (la forme de NkDialogs::OpenFileDialog).
		const char *NkEditeurFiltreImport() noexcept;

		// ── LES GESTES DU NAVIGATEUR (2026-10-01, document 02 §3.1) ─────────────
		// Chemins du NAVIGATEUR en entree et en sortie (« Contenu/... »). Le travail
		// sur le disque est celui du KIT (NkContentBrowserDisque.h) : confine au
		// Contenu, jamais d'ecrasement. Ici : la traduction des chemins, l'annonce,
		// et la MEMOIRE du navigateur (couleurs, favoris, collections) qui suit un
		// renommage, un deplacement ou une suppression.

		/// Le chemin du navigateur d'un chemin ABSOLU (ou relatif au repertoire
		/// courant) sous le Contenu ; vide s'il n'y est pas.
		NkString NkEditeurNavigateurDe(NkEditeurModele &m, const char *chemin);
		/// Le chemin ABSOLU d'un chemin du navigateur. NkTextures2D::Charger prefixe
		/// un chemin RELATIF par sa racine d'assets : une image du Contenu doit lui
		/// arriver absolue, sinon elle est cherchee sous « assets/ ».
		NkString NkEditeurCheminContenuAbsolu(NkEditeurModele &m, const char *cheminNav);
		/// Le nom du projet : le dossier qui porte la scene et son Contenu.
		NkString NkEditeurNomProjet(NkEditeurModele &m);
		/// « Nouveau dossier » dans `dossierNav` ; rend son chemin (vide : refuse).
		NkString NkEditeurNouveauDossier(NkEditeurModele &m, const char *dossierNav);
		/// Copie (ou DEPLACE) `chemins` dans `dossierNav`. Rend le nombre reussi ;
		/// `crees` recoit les nouveaux chemins.
		uint32 NkEditeurCopierContenu(NkEditeurModele &m, const NkVector<NkString> &chemins, const char *dossierNav,
									  bool deplacer, NkVector<NkString> *crees = nullptr);
		/// Une copie de chacun, a cote (« caisse_2.png »).
		uint32 NkEditeurDupliquerContenu(NkEditeurModele &m, const NkVector<NkString> &chemins,
										 NkVector<NkString> *crees = nullptr);
		/// Renomme ; refuse un nom pris ou invalide. Rend le nouveau chemin.
		NkString NkEditeurRenommerContenu(NkEditeurModele &m, const char *cheminNav, const char *nouveauNom);
		/// Supprime (vers la CORBEILLE si `corbeille`). Rend le nombre supprime.
		/// ⚠️ Ne pose aucune question : l'editeur ne l'appelle qu'APRES sa boite.
		uint32 NkEditeurSupprimerContenu(NkEditeurModele &m, const NkVector<NkString> &chemins, bool corbeille);
		/// Les scenes et prefabs du Contenu qui CITENT l'un de `chemins` (par son
		/// chemin ou son nom de fichier) : ce que la confirmation montre.
		void NkEditeurReferencesContenu(NkEditeurModele &m, const NkVector<NkString> &chemins, NkVector<NkString> &refs);
		/// Une scene VIDE (« NouvelleScene.nkscene ») dans `dossierNav`.
		NkString NkEditeurNouvelleSceneContenu(NkEditeurModele &m, const char *dossierNav);
		/// Un controleur d'animation (le modele « plateforme » comme point de depart).
		NkString NkEditeurNouveauControleurContenu(NkEditeurModele &m, const char *dossierNav);
		/// Un prefab de la SELECTION de la scene, dans `dossierNav`.
		NkString NkEditeurNouveauPrefabContenu(NkEditeurModele &m, const char *dossierNav);
		/// La memoire du navigateur suit `ancienNav` -> `nouveauNav` (vide : oubli).
		void NkEditeurMetaSuivre(NkEditeurModele &m, const char *ancienNav, const char *nouveauNav);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURCONTENU_H__
