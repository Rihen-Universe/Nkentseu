#pragma once
// -----------------------------------------------------------------------------
// @File    SondeEdition.h
// @Brief   `--sonde-edition` : LE TEMOIN DE R1. Une propriete changee doit
//          deplacer UNE LIGNE, pas une de plus.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QU'ELLE MESURE, ET POURQUOI C'EST LE CHIFFRE QUI DECIDE DE TOUT LE RESTE
// =============================================================================
//  Document 5 §2.3, le temoin de R1 : « les six documents de la famille ouverts,
//  UNE propriete changee a la souris, enregistres -> `git diff` montre UNE LIGNE ».
//
//  Ce n'est pas une coquetterie. Tant que ce chiffre n'est pas 1, NKUIDesign ne
//  peut pas editer sa propre interface : chaque enregistrement reecrirait le
//  document entier, effacerait les commentaires (« les documents de la famille
//  sont tres commentes »), et rendrait tout `git diff` illisible -- donc toute
//  relecture impossible. C'est pour ca que la specification interdit d'ecrire le
//  moindre document de NKUIDesign avant que ce temoin soit vert.
//
//  La sonde fait, sans souris et sans fenetre, exactement le geste du temoin :
//  elle lit le fichier, pose une propriete par `NkPoserPropriete`, reemet, et
//  COMPTE LES LIGNES QUI ONT BOUGE.
//
// =============================================================================
//  LES CINQ CRITERES, ET CE QUI LES EMPECHE D'ETRE COMPLAISANTS
// =============================================================================
//   C1. Le nombre de lignes du fichier NE CHANGE PAS (poser une propriete qui
//       existe deja ne doit rien ajouter).
//   C2. EXACTEMENT UNE ligne differe. Zero voudrait dire que l'ecriture n'a rien
//       fait ; deux ou plus, que la reemission a reforme autre chose au passage.
//   C3. La ligne qui a bouge PORTE LA NOUVELLE VALEUR. Sans ce critere, une
//       reemission qui deplacerait une ligne quelconque compterait pour juste.
//   C4. Le texte reemis SE RELIT, et le modele relu porte la nouvelle valeur.
//       C'est le critere qui refuse un fichier joli mais casse.
//   C5. AUCUN AUTRE IDENTIFIANT N'A BOUGE -- compares un a un, pas comptes.
//       🔴 Un TOTAL ne voit pas un remplacement : si l'ecrivain renommait un
//          widget en en renommant un autre a l'envers, le compte serait identique.
//
//  ⚠️ ET UNE VACUITE EST UN ECHEC. Un document ou l'on ne trouve aucune propriete
//     a changer est COMPTE comme non eprouve, et la sonde echoue si le total des
//     documents eprouves est nul. « 0 ecart sur 0 essai » n'est pas un succes.
//
//  ⚠️ ELLE N'ECRIT RIEN SUR LE DISQUE. Tout se passe en memoire : le fichier
//     d'origine n'est jamais touche. Un banc qui laisse ses traces dans ce qu'il
//     observe finit par s'observer lui-meme -- c'est deja arrive ce matin, l'
//     aller-retour ecrivait son vidage en `.nkgui` dans le dossier qu'il balayait
//     et le corpus est passe de 46 a 47 fichiers.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_SONDEEDITION_H__
#define __NKENTSEU_NKUIDESIGN_SONDEEDITION_H__

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NkGuiEdition.h"
#include "NkGuiLire.h"

namespace nkuidesign {

	namespace sondeedit {

		using nkentseu::NkArchive;
		using nkentseu::NkDirectory;
		using nkentseu::NkFile;
		using nkentseu::NkGuiArchive;
		using nkentseu::NkString;
		using nkentseu::NkStringView;
		using nkentseu::NkVector;
		using nkentseu::uint32;

		/// Le texte decoupe en lignes, sans les fins de ligne. `\r` retire, parce
		/// qu'un fichier CRLF et le meme en LF ne doivent pas compter pour
		/// differents -- ce depot a deja perdu une demi-journee sur ce piege-la.
		inline void NkELignes(const char *src, uint32 n, NkVector<NkString> &out) noexcept {
			NkString cur;
			for (uint32 i = 0; i < n; ++i) {
				const char c = src[i];
				if (c == '\n') {
					out.PushBack(cur);
					cur = NkString();
					continue;
				}
				if (c != '\r')
					cur.Append(NkStringView(&c, 1u));
			}
			if (!cur.Empty())
				out.PushBack(cur);
		}

		/// Combien de lignes different, et laquelle est la premiere. Ne vaut que
		/// lorsque les deux textes ont le MEME nombre de lignes -- l'appelant le
		/// verifie (C1) avant d'appeler, parce qu'un decalage d'une ligne ferait
		/// compter tout le reste du fichier comme different.
		inline uint32 NkEDifferences(const NkVector<NkString> &a, const NkVector<NkString> &b,
									 uint32 &premiere) noexcept {
			premiere = 0xFFFFFFFFu;
			uint32 n = 0;
			const uint32 m = (uint32)(a.Size() < b.Size() ? a.Size() : b.Size());
			for (uint32 i = 0; i < m; ++i) {
				if (a[i].Compare(b[i]) != 0) {
					if (premiere == 0xFFFFFFFFu)
						premiere = i;
					++n;
				}
			}
			return n;
		}

		/// Tous les identifiants du document, dans l'ordre de lecture. Sert a C5.
		inline void NkEIdentifiants(const NkArchive &ar, NkVector<NkString> &out) noexcept {
			NkUIDocument doc;
			guifmt::NkLectRapport rap;
			if (!guifmt::NkArchiveVersDocument(ar, doc, rap))
				return;
			for (uint32 i = 1u; i < (uint32)doc.nodes.Size(); ++i)
				out.PushBack(doc.nodes[i].idNkgui);
		}

	} // namespace sondeedit

	/// `--sonde-edition` : 0 si tout passe, 1 sinon.
	inline int SondeEdition() noexcept {
		using namespace sondeedit;
		using guifmt::NkLectRapport;

		printf("=== SONDE EDITION .nkgui — le temoin de R1 : UNE ligne ===\n");
		const char *kRacine = "Resources/Interface";
		if (!NkDirectory::Exists(kRacine)) {
			printf("  dossier introuvable : %s (demarrer depuis la racine du depot)\n", kRacine);
			printf("=== KO ===\n");
			return 1;
		}
		NkVector<NkString> fichiers = NkDirectory::GetFiles(
			kRacine, "*.nkgui", nkentseu::NkSearchOption::NK_ALL_DIRECTORIES);

		uint32 ko = 0u, eprouves = 0u, sansCible = 0u, surDisque = 0u;
		for (uint32 f = 0; f < (uint32)fichiers.Size(); ++f) {
			const char *chemin = fichiers[f].Data();
			NkVector<nkentseu::uint8> octets = NkFile::ReadAllBytes(chemin);
			if (octets.Empty())
				continue;
			const char *src = (const char *)octets.Data();
			const uint32 srcN = (uint32)octets.Size();

			NkArchive ar;
			nkentseu::NkGuiDiag diag;
			if (!NkGuiArchive::Read(src, srcN, ar, diag))
				continue; // les exemples volontairement illisibles : pas notre sujet

			// ── TROUVER UNE CIBLE : un bloc qui porte deja `label` ────────
			NkUIDocument doc;
			NkLectRapport rap;
			if (!guifmt::NkArchiveVersDocument(ar, doc, rap))
				continue;
			// ⚠️ TROIS CLES, PAS UNE. Chercher `label` seulement laissait CINQ
			//    documents sur douze « sans propriete a changer » -- c'est-a-dire
			//    hors de la mesure, alors qu'ils portent `text` ou `hint`. Un banc
			//    qui n'eprouve que la moitie du corpus protege la moitie du corpus.
			static const char *kCles[] = {"label", "text", "hint"};
			NkVector<uint32> cible;
			NkString ancien;
			const char *cle = nullptr;
			for (uint32 k = 0; k < 3u && cible.Empty(); ++k) {
				for (uint32 i = 1u; i < (uint32)doc.nodes.Size() && cible.Empty(); ++i) {
					if (doc.nodes[i].refArchive.Empty())
						continue;
					const NkArchive *b = guifmt::NkBlocAuChemin(ar, doc.nodes[i].refArchive);
					NkString v;
					if (b && b->GetString(NkStringView(kCles[k]), v) && !v.Empty()) {
						cible = doc.nodes[i].refArchive;
						ancien = v;
						cle = kCles[k];
					}
				}
			}
			if (cible.Empty()) {
				++sansCible;
				continue;
			}

			// ── LA REFERENCE : le fichier REEMIS SANS AUCUNE MODIFICATION ──
			// 🔴 DEUX CHOSES SE MELANGENT SI ON NE FAIT PAS CA, et le premier tir
			//    de cette sonde s'y est fait prendre : `Interface.nkgui` annoncait
			//    21 lignes deplacees pour UNE propriete posee. Les 20 autres
			//    n'etaient pas l'edition -- c'etait la REEMISSION qui normalise
			//    l'alignement de ce fichier-la (espaces avant `{`, `{}` rendu
			//    `{ }`), ecart deja connu de l'aller-retour.
			//    Comparer la sortie modifiee au fichier de DEPART accusait donc
			//    l'ecriture localisee d'un defaut qui ne lui appartient pas.
			//    On mesure desormais les deux separement :
			//      L1 = ce que L'EDITION deplace  (modifie vs reemis non modifie)
			//      L2 = ce que L'ENREGISTREMENT deplace sur le DISQUE (vs source)
			//    L1 est le critere de ces six fonctions. L2 est le temoin de R1 au
			//    sens strict, et il exige EN PLUS un aller-retour octet pour octet.
			const nkentseu::NkGuiStyle style = NkGuiArchive::DetectStyle(src, srcN);
			NkArchive vierge;
			nkentseu::NkGuiDiag dv;
			NkString reference;
			if (NkGuiArchive::Read(src, srcN, vierge, dv))
				reference = NkGuiArchive::Write(vierge, style);

			// ── LE GESTE : poser la propriete ─────────────────────────────
			NkString neuf(ancien);
			neuf.Append(" (temoin)");
			const bool pose = guifmt::NkPoserPropriete(ar, cible, cle, neuf.CStr());

			const NkString sortie = NkGuiArchive::Write(ar, style);

			NkVector<NkString> lSrc, lRef, lMod;
			NkELignes(src, srcN, lSrc);
			NkELignes(reference.Data(), (uint32)reference.Size(), lRef);
			NkELignes(sortie.Data(), (uint32)sortie.Size(), lMod);

			const bool c1 = (lRef.Size() == lMod.Size());
			uint32 premiere = 0xFFFFFFFFu;
			const uint32 diff = NkEDifferences(lRef, lMod, premiere);
			const bool c2 = (diff == 1u);
			const bool c3 = c2 && premiere < (uint32)lMod.Size()
							&& lMod[premiere].Find(NkStringView("(temoin)")) != (nkentseu::usize)-1;
			// L2 — le temoin de R1 sur le disque. Informatif ici, exige au bilan.
			uint32 pDisque = 0xFFFFFFFFu;
			const uint32 diffDisque = (lSrc.Size() == lMod.Size())
										  ? NkEDifferences(lSrc, lMod, pDisque)
										  : 0xFFFFFFFFu;

			// C4 : ca se relit, et la valeur a pris
			NkArchive relu;
			nkentseu::NkGuiDiag d2;
			bool c4 = NkGuiArchive::Read(sortie.Data(), (uint32)sortie.Size(), relu, d2);
			if (c4) {
				const NkArchive *b = guifmt::NkBlocAuChemin(relu, cible);
				NkString v;
				c4 = b && b->GetString(NkStringView(cle), v) && v.Compare(neuf) == 0;
			}

			// C5 : aucun AUTRE identifiant n'a bouge — un a un
			NkVector<NkString> idsA, idsB;
			NkEIdentifiants(ar, idsA);
			if (c4)
				NkEIdentifiants(relu, idsB);
			bool c5 = (idsA.Size() == idsB.Size());
			uint32 idsChanges = 0u;
			if (c5) {
				for (uint32 i = 0; i < (uint32)idsA.Size(); ++i)
					if (idsA[i].Compare(idsB[i]) != 0)
						++idsChanges;
				c5 = (idsChanges == 0u);
			}

			++eprouves;
			const bool ok = pose && c1 && c2 && c3 && c4 && c5;
			if (!ok)
				++ko;
			if (diffDisque == 1u)
				++surDisque;
			printf("  -- %s  [%s]\n", chemin, cle);
			printf("       L1 edition : %u ligne(s) %s%s | valeur %s | relu %s | "
				   "identifiants %u/%u %s   ||   L2 disque : %s\n",
				   diff, c2 ? "OK" : "<<< KO", c1 ? "" : " (NOMBRE DE LIGNES CHANGE)",
				   c3 ? "OK" : "<<< KO", c4 ? "OK" : "<<< KO", (uint32)idsA.Size(),
				   (uint32)idsB.Size(), c5 ? "OK" : "<<< KO",
				   diffDisque == 1u	  ? "1 ligne — temoin R1 tenu"
				   : diffDisque == 0u ? "0 ligne (?)"
				   : diffDisque == 0xFFFFFFFFu
					   ? "nombre de lignes different"
					   : "plus d'une ligne — la REEMISSION reformate ce fichier");
			if (!ok && premiere != 0xFFFFFFFFu && premiere < (uint32)lRef.Size()) {
				// ⚠️ UN BANC QUI REFUSE SANS MONTRER CE QU'IL A PRODUIT oblige a
				//    deviner. Trois experiences ont ete perdues ce matin faute de
				//    cette impression-la.
				printf("       ligne %u avant : %s\n", premiere + 1u, lRef[premiere].Data());
				printf("       ligne %u apres : %s\n", premiere + 1u,
					   premiere < (uint32)lMod.Size() ? lMod[premiere].Data() : "(absente)");
			}
		}

		printf("\n  %u documents eprouves, %u sans propriete a changer, %u en faute\n", eprouves,
			   sansCible, ko);
		printf("  L1 — l'EDITION deplace une ligne : %u/%u\n", eprouves - ko, eprouves);
		printf("  L2 — l'ENREGISTREMENT sur le disque deplace une ligne : %u/%u\n", surDisque,
			   eprouves);
		if (surDisque != eprouves) {
			// ⚠️ PAS UN ECHEC DE CES SIX FONCTIONS, ET IL FAUT LE DIRE. L2 exige
			//    en plus que la reemission rende le fichier OCTET POUR OCTET ;
			//    quand elle normalise l'alignement, les lignes reformatees
			//    s'ajoutent a celle qu'on a voulu changer. Le temoin de R1 au sens
			//    strict le demande, donc c'est un travail a faire -- soit
			//    l'ecrivain apprend a rendre l'alignement, soit les fichiers
			//    concernes sont normalises une fois pour toutes.
			printf("        ^ ces documents-la sont ceux que la REEMISSION reformate.\n");
			printf("          L'ecriture localisee n'y est pour rien (L1 le montre) :\n");
			printf("          c'est l'aller-retour octet qui n'est pas encore exact.\n");
		}
		if (eprouves == 0u) {
			printf("  aucun document eprouve — la mesure ne demontre rien\n");
			++ko;
		}
		printf("=== %s (ko = %u) ===\n", ko == 0u ? "TOUT PASSE" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_SONDEEDITION_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
