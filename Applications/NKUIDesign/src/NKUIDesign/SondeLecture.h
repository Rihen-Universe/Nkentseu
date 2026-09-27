#pragma once
// -----------------------------------------------------------------------------
// @File    SondeLecture.h
// @Brief   `--sonde-lecture` : ce que l'ouverture d'un `.nkgui` construit, et ce
//          qu'elle coute. Sans fenetre, sans GPU.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QU'ELLE DEMANDE, ET POURQUOI CHAQUE CRITERE PEUT ECHOUER
// =============================================================================
//  Le chantier A construit `NkArchiveVersDocument` (`NkGuiLire.h`). Un lecteur
//  qui « marche » se reconnait a trois choses, et la troisieme est la seule qui
//  engage l'avenir :
//
//   1. IL N'OUBLIE PERSONNE — autant de noeuds que de blocs.
//   2. IL NE RENOMME PERSONNE — l'identifiant du fichier arrive intact dans le
//      modele. C'est ce qui manquait ce matin : l'ecrivain FABRIQUAIT un
//      identifiant (`<libelle>_<indice>`), donc un document ouvert puis
//      enregistre voyait TOUS ses identifiants changer -- et avec eux toutes ses
//      actions, puisqu'un identifiant EST la cle d'etat du monteur.
//   3. CHAQUE NOEUD RETROUVE SON BLOC — l'invariant 3 du document 5 §1.1.
//
// =============================================================================
//  🔴 LE CRITERE 3 NE FAIT PAS CONFIANCE AU RAPPORT DU LECTEUR
// =============================================================================
//  Il ne lit AUCUN compteur de `NkLectRapport`. Il reprend le chemin d'indices
//  de chaque noeud, redescend l'archive a la main, et compare l'identifiant qu'il
//  trouve la-bas a celui que le noeud porte. Deux chemins de code sans rien en
//  commun : si le lecteur se trompait de branche, ses propres compteurs le
//  diraient juste -- ils compteraient fidelement des chemins faux.
//
//  ⚠️ ET IL EXIGE D'AVOIR VERIFIE TOUT LE MONDE. `verifiees == noeuds` est une
//     condition du verdict, pas une statistique. Sans elle, un lecteur qui ne
//     poserait AUCUN chemin d'archive passerait le critere 3 avec « 0 ecart » --
//     zero ecart sur zero verification. Un critere qui ne peut pas echouer ne
//     protege rien.
//
//  ⚠️ CE QUI N'EST PAS PORTE S'IMPRIME, ET NE COMPTE PAS COMME UNE FAUTE. Le
//     modele ne tient aujourd'hui que `label`, `text`, `size`, `pos`, `bind` : le
//     reste vit dans l'archive, que l'appelant garde (c'est la couche de l'octet,
//     doc 5 §1.1). Transformer ca en echec ferait echouer la sonde sur un dépot
//     sain ; le taire ferait croire le modele complet. Elle le NOMME.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_SONDELECTURE_H__
#define __NKENTSEU_NKUIDESIGN_SONDELECTURE_H__

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NkGuiEcrire.h"
#include "NkGuiLire.h"

namespace nkuidesign {

	namespace sondelect {

		using nkentseu::NkArchive;
		using nkentseu::NkArchiveNode;
		using nkentseu::NkDirectory;
		using nkentseu::NkFile;
		using nkentseu::NkGuiArchive;
		using nkentseu::NkString;
		using nkentseu::NkStringView;
		using nkentseu::NkVector;
		using nkentseu::uint32;

		/// Redescend l'archive par un chemin d'indices. Ecrite ICI, a la main, et
		/// pas empruntee au lecteur : c'est tout l'interet (voir l'en-tete).
		inline const NkArchive *NkSSuivreChemin(const NkArchive &racine,
												const NkVector<uint32> &chemin) noexcept {
			const NkArchive *cur = &racine;
			for (uint32 i = 0; i < (uint32)chemin.Size(); ++i) {
				const NkArchiveNode *corps =
					cur->FindNode(NkStringView(NkGuiArchive::KeyBody()));
				if (!corps || chemin[i] >= (uint32)corps->array.Size())
					return nullptr;
				const NkArchiveNode &e = corps->array[chemin[i]];
				if (!e.IsObject() || !e.object)
					return nullptr;
				cur = e.object;
			}
			return cur;
		}

	} // namespace sondelect

	/// `--sonde-lecture` : 0 si tout passe, 1 sinon.
	inline int SondeLecture() noexcept {
		using namespace sondelect;
		using nkuidesign::guifmt::NkArchiveVersDocument;
		using nkuidesign::guifmt::NkLectRapport;

		printf("=== SONDE LECTURE .nkgui -> modele NKUIDesign ===\n");
		const char *kRacine = "Resources/Interface";
		if (!NkDirectory::Exists(kRacine)) {
			printf("  dossier introuvable : %s\n", kRacine);
			printf("  (les applications ne demarrent QUE depuis la racine du depot)\n");
			printf("=== KO ===\n");
			return 1;
		}
		NkVector<NkString> fichiers = NkDirectory::GetFiles(
			kRacine, "*.nkgui", nkentseu::NkSearchOption::NK_ALL_DIRECTORIES);
		if (fichiers.Empty()) {
			printf("  aucun document dans %s\n", kRacine);
			printf("=== KO ===\n");
			return 1;
		}

		uint32 ko = 0u, totBlocs = 0u, totNoeuds = 0u, totVerif = 0u, totEcarts = 0u;
		uint32 totNonPortes = 0u, totSectionsIgn = 0u, totIdsVides = 0u;

		for (uint32 f = 0; f < (uint32)fichiers.Size(); ++f) {
			const char *chemin = fichiers[f].Data();
			NkVector<nkentseu::uint8> octets = NkFile::ReadAllBytes(chemin);
			if (octets.Empty()) {
				printf("  -- %s : ILLISIBLE\n", chemin);
				++ko;
				continue;
			}
			NkArchive ar;
			nkentseu::NkGuiDiag diag;
			if (!NkGuiArchive::Read((const char *)octets.Data(), (uint32)octets.Size(), ar,
									diag)) {
				// ⚠️ PAS UNE FAUTE DE LA SONDE. Le corpus contient des exemples
				//    VOLONTAIREMENT illisibles, et ils doivent le rester. Ce qui se
				//    mesure ici est la lecture des documents QUI S'ANALYSENT.
				// Le code ET la ligne : « non analysable » sans position oblige a
				// chercher a la main dans un document de trois mille noeuds.
				printf("  -- %s : non analysable (%s l.%u — %s) — ignore\n", chemin,
					   diag.code.Data(), diag.line, diag.message.Data());
				continue;
			}

			NkUIDocument doc;
			NkLectRapport rap;
			if (!NkArchiveVersDocument(ar, doc, rap)) {
				printf("  -- %s : LECTURE REFUSEE\n", chemin);
				++ko;
				continue;
			}

			// ── CRITERE 3, redescendu a la main ──────────────────────────
			uint32 verif = 0u, ecarts = 0u, idsVides = 0u;
			for (uint32 i = 1u; i < (uint32)doc.nodes.Size(); ++i) { // 0 = la toile
				const NkUINode &n = doc.nodes[i];
				if (n.refArchive.Empty())
					continue;
				++verif;
				const NkArchive *bloc = NkSSuivreChemin(ar, n.refArchive);
				if (!bloc) {
					++ecarts;
					continue;
				}
				const NkString idLa(NkGuiArchive::IdOf(*bloc));
				if (idLa.Compare(n.idNkgui) != 0)
					++ecarts;
				if (n.idNkgui.Empty() && !idLa.Empty())
					++idsVides;
			}

			const bool c1 = (rap.noeuds == rap.blocs);
			const bool c3 = (ecarts == 0u) && (verif == rap.noeuds);
			if (!c1 || !c3)
				++ko;

			printf("  -- %s\n", chemin);
			printf("       blocs %u -> noeuds %u %s | refs verifiees %u/%u, ecarts %u %s\n",
				   rap.blocs, rap.noeuds, c1 ? "OK" : "<<< KO", verif, rap.noeuds, ecarts,
				   c3 ? "OK" : "<<< KO");
			if (rap.attributsNonPortes || rap.sectionsNonLues) {
				printf("       non porte par le modele : %u attribut(s) —", rap.attributsNonPortes);
				for (uint32 k = 0; k < (uint32)rap.nonPortes.Size(); ++k)
					printf(" %s", rap.nonPortes[k].Data());
				if (rap.sectionsNonLues) {
					printf(" | %u section(s) —", rap.sectionsNonLues);
					for (uint32 k = 0; k < (uint32)rap.sectionsIgnorees.Size(); ++k)
						printf(" %s", rap.sectionsIgnorees[k].Data());
				}
				printf("\n");
			}
			totBlocs += rap.blocs;
			totNoeuds += rap.noeuds;
			totVerif += verif;
			totEcarts += ecarts;
			totNonPortes += rap.attributsNonPortes;
			totSectionsIgn += rap.sectionsNonLues;
			totIdsVides += idsVides;
		}

		printf("\n  TOTAL : %u blocs -> %u noeuds | %u refs verifiees, %u ecarts\n", totBlocs,
			   totNoeuds, totVerif, totEcarts);
		printf("          %u attributs et %u sections que le modele ne porte pas\n",
			   totNonPortes, totSectionsIgn);
		if (totIdsVides)
			printf("          %u identifiants PERDUS (le fichier en donnait un)\n", totIdsVides);
		// Un corpus vide passerait tous les criteres : il ne prouve rien.
		if (totNoeuds == 0u) {
			printf("  aucun noeud construit — la mesure ne demontre rien\n");
			++ko;
		}
		if (totIdsVides)
			++ko;
		printf("=== %s (ko = %u) ===\n", ko == 0u ? "TOUT PASSE" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_SONDELECTURE_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
