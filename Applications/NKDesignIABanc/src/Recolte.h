#pragma once
// -----------------------------------------------------------------------------
// @File    Recolte.h
// @Brief   LA RECOLTE : garder chaque paire, surtout les ECHECS.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CECI EXISTE, ET POURQUOI C'EST SI PETIT
// =============================================================================
//  Ce n'est pas un chantier : c'est quelques lignes posees LA OU LE VERDICT EST
//  DEJA CALCULE. Rien n'est recalcule ici, rien n'est juge ici -- on ECRIT ce que
//  la course vient de decider, et on ne jette rien.
//
//  ⚠️ LE CORPUS EST L'ACTIF, PAS LES POIDS. Un jeu de paires validees sert
//     N'IMPORTE QUEL modele de base futur ; des poids se periment au premier
//     modele suivant. Et ce corpus-la commence a cout nul : il suffit de ne rien
//     jeter, a partir de maintenant.
//
//  ⚠️ ET NOUS AVONS L'ACTIF RARE : un VALIDATEUR et un MONTEUR. La plupart de
//     ceux qui distillent n'ont aucun moyen de verifier ce que le professeur a
//     produit. Nous pouvons filtrer automatiquement -- c'est ce que les trois
//     taux font deja, et c'est exactement ce qu'on ecrit a cote de chaque paire.
//
//  ⚠️ ON GARDE LES ECHECS, ET C'EST LA REGLE QUI COMPTE LE PLUS.
//     *Un corpus qui ne contient que des reussites n'apprend pas a refuser.* Le
//     refus NOMME est une exigence de ce depot depuis le 17/09 ; un document
//     invalide accompagne du MOTIF de son rejet vaut autant qu'un bon document.
//
//  ⚠️ UNE PAIRE PORTE SA CONDITION. Ce depot a paye trois fois « un chiffre sans
//     sa condition ». Une paire produite par un modele de 7 milliards de
//     parametres et une paire produite par un futur modele maison ne se melangent
//     pas : le dorsal, le modele et l'horodatage sont ECRITS DANS LE FICHIER et
//     dans son nom.
//
//  ⚠️ AUCUNE DONNEE PERSONNELLE, JAMAIS. Regle permanente du depot. Ce fichier
//     n'ecrit que ce que la course a produit : une demande technique, une reponse
//     de modele, un document, un verdict. Aucun identifiant, aucun contenu
//     d'apprenant, aucun chemin de la machine de quelqu'un.
//
//  FORMAT : du TEXTE EN CLAIR, jamais un binaire. Un corpus qu'on ne peut pas
//  lire avec un editeur est un corpus que personne ne relit -- et celui-ci sera
//  relu a la main, longtemps, par ceux qui choisiront quoi garder.
// =============================================================================

#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKTime/NkSystemClock.h"

#include <cstdio>

namespace nkrecolte {

	using nkentseu::NkString;
	using nkentseu::uint32;

	/// L'horodatage de LA COURSE, pris UNE FOIS.
	///
	/// ⚠️ UNE COURSE = UNE CONDITION, donc UN horodatage. Le prendre par paire
	///    laisserait croire que deux paires de la meme course sont deux mesures
	///    independantes ; elles partagent le meme modele, la meme machine et la
	///    meme invite. Un seul tampon le dit.
	inline const NkString &HorodatageCourse() {
		static NkString stamp;
		if (stamp.Size() == 0) {
			const nkentseu::NkSystemDateTime t = nkentseu::NkSystemClock::LocalNow();
			char b[32];
			snprintf(b, sizeof(b), "%04d%02d%02d-%02d%02d%02d", t.year, t.month, t.day,
					 t.hour, t.minute, t.second);
			stamp = NkString(b);
		}
		return stamp;
	}

	/// Ce qu'une paire porte. Tout est deja calcule par la course : on recopie.
	struct NkPaire {
			const char *id = "";		 ///< l'identifiant de la demande (d01...)
			const char *demande = "";	 ///< la demande EXACTE, telle qu'ecrite
			const char *dorsal = "";	 ///< fichier | processus | temoin | ollama
			const char *modele = "";	 ///< le nom du modele, VERSION COMPRISE
			const char *verdictNom = ""; ///< le nom du verdict (NkAIVerdictName)
			const char *motif = "";		 ///< le MOTIF du rejet, vide si accepte
			bool n1 = false;			 ///< le modele a rendu quelque chose
			bool n2 = false;			 ///< le validateur a LU le document
			bool n3 = false;			 ///< le document S'OUVRE en .nkgui
			double ms = 0.0;			 ///< duree de la generation
			const NkString *brut = nullptr;	   ///< la reponse BRUTE, jamais nettoyee
			const NkString *document = nullptr; ///< le .nkgui produit, vide si aucun
	};

	/// Ecrit UNE paire. Rend faux si elle n'a pas pu etre ecrite -- et l'appelant
	/// le dit, parce qu'une recolte qui echoue en silence se decouvre le jour ou
	/// on veut s'en servir.
	///
	/// ⚠️ ELLE N'ECRIT PAS QUE LES REUSSITES. Il n'y a AUCUNE condition sur `n2`
	///    ni `n3` dans cette fonction, et c'est deliberement verifiable d'un coup
	///    d'oeil : le jour ou quelqu'un ajoutera un `if (p.n3)` ici, le corpus
	///    cessera d'apprendre a refuser.
	inline bool Ecrire(const char *dossier, const NkPaire &p) {
		if (!dossier || !*dossier || !p.id || !*p.id)
			return false;
		nkentseu::NkDirectory::CreateRecursive(dossier);

		// Le nom porte deja la condition : on doit pouvoir trier un dossier de
		// mille paires sans ouvrir un seul fichier.
		char nomModele[64];
		{
			uint32 k = 0;
			for (const char *s = p.modele ? p.modele : ""; *s && k + 1u < sizeof(nomModele); ++s)
				nomModele[k++] = (*s == ':' || *s == '/' || *s == '\\' || *s == ' ') ? '_' : *s;
			nomModele[k] = '\0';
			if (k == 0)
				snprintf(nomModele, sizeof(nomModele), "sans-modele");
		}
		char chemin[640];
		snprintf(chemin, sizeof(chemin), "%s/%s_%s_%s_%s.txt", dossier,
				 HorodatageCourse().CStr(), p.dorsal ? p.dorsal : "?", nomModele, p.id);

		NkString out;
		char b[512];

		out.Append("# paire de recolte -- NkUIDesign, texte vers interface\n");
		out.Append("# AUCUNE DONNEE PERSONNELLE. Demande technique, reponse de modele,\n");
		out.Append("# document, verdict. Rien d'autre n'entre ici.\n\n");

		out.Append("[condition]\n");
		snprintf(b, sizeof(b), "horodatage = %s\n", HorodatageCourse().CStr());
		out.Append(b);
		snprintf(b, sizeof(b), "dorsal     = %s\n", p.dorsal ? p.dorsal : "?");
		out.Append(b);
		snprintf(b, sizeof(b), "modele     = %s\n", p.modele ? p.modele : "(inconnu)");
		out.Append(b);
		snprintf(b, sizeof(b), "duree_ms   = %.0f\n\n", p.ms);
		out.Append(b);

		// ⚠️ LE VERDICT AVANT LE CONTENU, et c'est voulu : celui qui trie le
		//    corpus lit les trois lignes du haut et decide s'il ouvre la suite.
		out.Append("[verdict]\n");
		snprintf(b, sizeof(b), "n1_rend_quelque_chose = %s\n", p.n1 ? "oui" : "non");
		out.Append(b);
		snprintf(b, sizeof(b), "n2_document_se_lit    = %s\n", p.n2 ? "oui" : "non");
		out.Append(b);
		snprintf(b, sizeof(b), "n3_document_s_ouvre   = %s\n", p.n3 ? "oui" : "non");
		out.Append(b);
		snprintf(b, sizeof(b), "nom_du_verdict        = %s\n", p.verdictNom ? p.verdictNom : "");
		out.Append(b);
		// LE MOTIF EST LA VALEUR D'UN ECHEC. Sans lui, un document invalide
		// n'apprend qu'a reconnaitre du bruit ; avec lui, il apprend a REFUSER.
		out.Append("motif_du_rejet        = ");
		out.Append(p.motif && *p.motif ? p.motif : "(aucun)");
		out.Append("\n\n");

		out.Append("[demande]\n");
		out.Append(p.demande ? p.demande : "");
		out.Append("\n\n");

		out.Append("[reponse brute du modele]\n");
		if (p.brut && p.brut->Size() > 0)
			out.Append(*p.brut);
		else
			out.Append("(le dorsal n'a rien rendu)");
		out.Append("\n\n");

		out.Append("[document produit]\n");
		if (p.document && p.document->Size() > 0)
			out.Append(*p.document);
		else
			out.Append("(aucun document : voir le motif ci-dessus)");
		out.Append("\n");

		return nkentseu::NkFile::WriteAllText(nkentseu::NkPath(chemin), out);
	}

} // namespace nkrecolte
