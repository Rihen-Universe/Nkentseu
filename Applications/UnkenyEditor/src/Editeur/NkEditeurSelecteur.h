//
// NkEditeurSelecteur.h
// =============================================================================
// Description :
//   LE selecteur de fichiers et de dossiers de l'editeur : UN emballage du
//   selecteur de NKEditorKit (NkFilePickerNavState + NkDrawSelecteur, celui de
//   NKCode, NKCraft et NKUIDesign), dessine par le moteur dans le style de
//   l'editeur. « Importer… », « Exporter… » et le « Parcourir… » de la fenetre
//   Construire passent TOUS par lui.
//
// Caracteristiques :
//   - (2026-09-30, lot 1, demande de Rihen) AUCUN selecteur neuf : le kit a
//     deja rail, vignettes, fil d'Ariane, filtres nommes, tri, creation de
//     dossier et selection multiple. Ce fichier ne pose que l'USAGE (titre,
//     bouton, mode, filtres, dossier de depart) et rend le resultat.
//   - L'usage est une DEMANDE lue a la trame (NkEditeurDessinerTrame) : le
//     selecteur est MODAL -- le corps, les menus et la fenetre Construire ne
//     recoivent rien tant qu'il est ouvert.
//   - Le dialogue natif (NkDialogs de NKWindow) n'est plus appele : il reste
//     dans NKWindow pour qui le veut, son filtre repare (lot 1).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURSELECTEUR_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURSELECTEUR_H__

#include "Editeur/NkEditeurInterface.h"

#include "NKEditorKit/NkFilePickerNav.h"

namespace nkentseu {
	namespace editeur {

		/// A quoi sert le selecteur ouvert.
		enum class NkUsageSelecteur : uint8 {
			NK_AUCUN = 0,
			NK_IMPORTER,	   ///< des FICHIERS de l'OS, a copier dans le Contenu (plusieurs)
			NK_EXPORTER,	   ///< un DOSSIER de l'OS ou copier les assets choisis
			NK_DOSSIER_SORTIE  ///< le dossier de sortie de « Construire »
		};

		/// L'etat du selecteur de l'editeur : celui du kit, et son usage (titre et
		/// bouton en dependent -- les points de specialisation du kit).
		class NkEditeurSelecteurEtat : public editorkit::NkFilePickerNavState {
			public:
				NkUsageSelecteur usage = NkUsageSelecteur::NK_AUCUN;
				/// L'usage CONFIRME a la derniere trame (NK_AUCUN sinon) : lu apres
				/// NkEditeurDessinerSelecteur, avec `resultatsMultiples` /
				/// `pickerResultPath`.
				NkUsageSelecteur confirme = NkUsageSelecteur::NK_AUCUN;
				/// Le tampon du chemin que le kit remplit.
				char tampon[512] = {};

				const char *PickerTitle() const override;
				const char *PickerConfirmLabel() const override;
				/// La porte de sortie unique du kit (confirmer, Annuler, Echap) : l'usage
				/// meurt avec la fenetre, qu'il ne survive pas a la suivante.
				void PickerCancel() override;
		};

		/// Ouvre le selecteur pour `usage`, sur `depart` (un dossier ; vide = le
		/// dernier dossier ou il a servi, sinon Documents).
		void NkEditeurOuvrirSelecteur(NkEditeurSelecteurEtat &s, NkUsageSelecteur usage, const char *depart);

		/// Le selecteur est-il ouvert (donc la trame modale) ?
		inline bool NkEditeurSelecteurOuvert(const NkEditeurSelecteurEtat *s) noexcept {
			return s != nullptr && s->pickerOpen;
		}

		/// Dessine le selecteur s'il est ouvert (entree reelle, par-dessus tout).
		/// Rend l'usage CONFIRME a cette trame, NK_AUCUN sinon.
		NkUsageSelecteur NkEditeurDessinerSelecteur(NkEditeurCadre &c, NkEditeurSelecteurEtat &s);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURSELECTEUR_H__
