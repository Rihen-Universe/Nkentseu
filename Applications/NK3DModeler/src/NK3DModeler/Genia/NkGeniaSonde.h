#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Genia/NkGeniaSonde.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
//  SONDE DE MESURE — LA PORTE DU GENERATEUR : le motif remonte-t-il ?
//
//  ⚠️ AUCUNE FENETRE, AUCUN DEVICE, AUCUN GPU PRIS. Elle tourne pendant que la
//     carte est occupee -- et elle l'est : l'entrainement d'Ilyana y vit, et la
//     machine s'est coupee deux fois aujourd'hui sur une faute du planificateur
//     video. Une sonde qui aurait besoin d'un contexte graphique pour verifier
//     une remontee de TEXTE serait une sonde qu'on ne peut pas lancer.
//
//  CE QU'ELLE MESURE, ET LES CRITERES SONT ECRITS ICI, AVANT LE CODE :
//   (a) LE MOTIF DU SOUS-PROCESSUS REMONTE. Un script qui refuse en imprimant
//       sa raison doit voir cette raison arriver dans `why`. C'etait le defaut
//       nomme par la navette : le pont ne lisait pas la sortie, et l'ecran
//       affichait un echec SANS CAUSE.
//   (b) LE NEGATIF, ET IL EST INDISPENSABLE : un processus qui ne dit RIEN doit
//       produire un motif qui DIT qu'il n'a rien dit -- jamais un motif vide,
//       jamais une phrase inventee. Sans ce cas, (a) passerait aussi bien avec
//       un texte fabrique par le pont.
//   (c) LA PORTE TEXTE REFUSE PAR DEFAUT, en nommant ce qu'il faut poser. Une
//       implementation qui ne sait pas produire depuis du texte ne doit rien
//       ecrire et le DIRE.
//   (d) LA PORTE TEXTE PRODUIT, quand son gabarit est rempli : le fichier
//       demande existe a la sortie.
//   (e) L'INVITE VIDE est refusee AVANT tout lancement.
// -----------------------------------------------------------------------------

#include "NK3DModeler/Genia/NkGenerateur.h"

namespace nkentseu {
	namespace nk3d {

		inline int32 NkGeniaSonde(const NkString &dir) {
			NkString rapport;
			int32 rouges = 0;
			auto dire = [&](const char *nom, bool ok, const NkString &detail) {
				rapport.Append(ok ? "VERT   " : "ROUGE  ");
				rapport.Append(nom);
				rapport.Append("  ");
				rapport.Append(detail);
				rapport.Append("\n");
				if (!ok)
					++rouges;
				std::printf("%s %s  %s\n", ok ? "VERT  " : "ROUGE ", nom, detail.CStr());
			};

			const NkString sortie = dir + NkString("/nk_genia_out.obj");
			if (NkFile::Exists(sortie.CStr()))
				NkFile::Delete(sortie.CStr());

			NkGenerateurProcessus gen;
			NkString why;

			// (a) un processus qui REFUSE EN PARLANT. `cmd /c` est partout sous
			//     Windows et n'ajoute aucune dependance ; il ecrit sur stderr,
			//     c'est-a-dire le flux que le pont ne lisait pas.
			gen.gabaritTexte = "cmd /c \"echo je refuse : le modele est absent 1>&2 & exit 3\"";
			bool ok = gen.GenererDepuisTexte("un cube", sortie.CStr(), why);
			dire("(a) le MOTIF du sous-processus remonte", !ok && why.Find("le modele est absent") != NkString::npos, why);

			// (b) NEGATIF : un processus MUET. Le motif doit dire qu'il n'a rien dit.
			gen.gabaritTexte = "cmd /c \"exit 4\"";
			why.Clear();
			ok = gen.GenererDepuisTexte("un cube", sortie.CStr(), why);
			dire("(b) NEGATIF : un processus MUET produit un motif qui LE DIT",
				 !ok && why.Find("RIEN imprime") != NkString::npos, why);

			// (c) la porte texte refuse PAR DEFAUT, et nomme ce qu'il faut poser.
			NkGenerateurProcessus nu;
			why.Clear();
			ok = nu.GenererDepuisTexte("un cube", sortie.CStr(), why);
			dire("(c) sans gabarit, la porte texte REFUSE en nommant la variable",
				 !ok && why.Find("NK_GENIA_CMD_TEXTE") != NkString::npos, why);

			// (d) elle PRODUIT quand le gabarit est rempli.
			gen.gabaritTexte = NkString("cmd /c \"echo o 1> \"{out}\"\"");
			why.Clear();
			ok = gen.GenererDepuisTexte("un cube", sortie.CStr(), why);
			const bool existe = NkFile::Exists(sortie.CStr());
			dire("(d) avec un gabarit, elle ECRIT le fichier demande",
				 ok && existe, NkString::Format("rendu=%d fichier=%d", ok ? 1 : 0, existe ? 1 : 0));

			// (e) l'invite vide est refusee AVANT tout lancement.
			why.Clear();
			ok = gen.GenererDepuisTexte("", sortie.CStr(), why);
			dire("(e) LE ZERO : une invite vide est refusee avant tout lancement",
				 !ok && why.Find("aucune invite") != NkString::npos, why);

			const NkString chemin = dir + NkString("/sonde_genia.txt");
			NkFile::WriteAllText(chemin.CStr(), rapport.CStr());
			std::printf(rouges == 0 ? "TOUT VERT (0 rouge)\n" : "%d ROUGE(S)\n", (int)rouges);
			std::fflush(stdout);
			return rouges == 0 ? 0 : 1;
		}

	} // namespace nk3d
} // namespace nkentseu
