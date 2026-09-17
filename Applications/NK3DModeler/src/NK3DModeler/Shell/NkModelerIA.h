#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Shell/NkModelerIA.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LE PANNEAU APPELLE VRAIMENT — la phrase part, le verbe revient
// =============================================================================
//  Jusqu'ici, le texte tape dans le panneau etait soumis TEL QUEL au pont des
//  verbes : « subdivise deux fois » tombait donc dans le refus nomme, parce que
//  ce n'est pas un verbe. Le panneau avait sa forme, son effet mesure et son
//  annulation, mais il n'avait pas de MODELE. Ce fichier pose le chainon
//  manquant, et rien d'autre.
//
//  ⚠️ CE FICHIER NE CONNAIT AUCUN MODELE, ET C'EST UNE CONSIGNE, PAS UN GOUT.
//     Il parle a `NKConverse` -- une requete, une reponse ou un refus nomme --
//     et le choix du dorsal est un REGLAGE. Ollama est un echafaudage qui
//     debloque cette nuit, pas une destination : `Kernel/AI/NKInfer` lit les
//     MEMES poids GGUF (NkOllamaLocate traduit deja « qwen2.5:7b-instruct » en
//     chemin de blob), et NKAI a distille Qwen. Le jour ou notre moteur sert la
//     reponse, on change le GABARIT et pas une ligne d'ici.
//     ⚠️ Si du vocabulaire propre a un fournisseur remonte jusqu'au panneau,
//        c'est une FUITE : elle se signale, elle ne s'absorbe pas.
//
//  ⚠️ ET ON N'ECRIT PAS DE DORSAL. `NKConverse` porte deja l'interface et un
//     dorsal generique de PROCESSUS (« deux trous, {invite} et {sortie} »).
//     Un autre chantier ecrit en ce moment le dorsal HTTP dans ce module
//     partage : c'est le sien qu'il faudra consommer. Deux dorsaux seraient la
//     dette des trois exemplaires, rouverte le lendemain de son remboursement.
//
//  POURQUOI L'ASYNCHRONE. Une reponse mesuree a 0,4 s sur carte peut valoir des
//  dizaines de secondes sur processeur, ou ne jamais venir. Un appel synchrone
//  gelerait la fenetre, et Rodolf croirait l'application plantee. `NKConverse`
//  porte deja `NkEnvoiAsync` : on le CONSOMME.
// -----------------------------------------------------------------------------

#include "NK3DModeler/Shell/NkModelerContrat.h"
#include "NK3DModeler/Shell/NkModelerInput.h" // NkModelerState, NkVpAction
#include "NKConverse/NkConverseChatAsync.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace nk3d {

		/// Le verbe -> l'indice de commande. Pose par `main.cpp`, qui porte la
		/// table : la lui demander evite de la recopier ici, et une recopie
		/// pourrait mentir sans que rien ne le signale.
		inline int32 (*NkIaCmdDuVerbe)(NkVpAction) = nullptr;

		// ── LE CONTRAT, DONNE AU MODELE ────────────────────────────────────────
		// ⚠️ UN MODELE QUI CONNAIT LA GRAMMAIRE N'A PLUS A L'INVENTER. C'est la
		//    raison d'etre de la table de verbes devenue DONNEE : elle est lue
		//    ici comme elle est lue par l'imprimeur du contrat et par le pont.
		//    UNE SEULE AUTORITE, TROIS LECTEURS -- un verbe ajoute est reconnu,
		//    documente ET enseigne au modele du meme geste.
		// ⚠️ Les bornes ne sont pas recopiees : elles viennent de
		//    `Demo3DHostOpParamInfo`, la table que le panneau de proprietes
		//    affiche et que le clamp applique.
		inline void NkIaEcrireContratDansInvite(char *dst, uint32 taille, bool avecContrat) {
			if (!dst || taille == 0)
				return;
			dst[0] = 0;
			uint32 n = 0;
			auto ajout = [&](const char *s) {
				if (!s)
					return;
				while (*s && n + 1u < taille)
					dst[n++] = *s++;
				dst[n] = 0;
			};
			ajout("Tu traduis une demande en UNE commande, et rien d'autre.\n");
			ajout("Reponds par UNE SEULE LIGNE, sans phrase, sans explication, sans ponctuation\n");
			ajout("finale, au format  verbe  ou  verbe:parametre1:parametre2\n");
			if (!avecContrat) {
				// ⚠️ LE NEGATIF DE LA MESURE « AVEC CONTRAT CONTRE SANS ». Sans la
				//    table, le modele doit DEVINER le vocabulaire. C'est ce que
				//    mesure la sonde : ce que NOTRE outillage apporte, et non ce
				//    que le modele sait deja.
				ajout("Le vocabulaire n'est pas donne : devine le verbe anglais usuel.\n");
				return;
			}
			ajout("\nVerbes autorises, avec leurs parametres et leurs bornes :\n");
			int32 nv = 0;
			const NkVerbe *V = NkVerbes(nv);
			const int32 np = demo::Demo3DHostOpParamCount();
			for (int32 i = 0; i < nv; ++i) {
				char ligne[256];
				int32 pos = snprintf(ligne, sizeof(ligne), "- %s", V[i].nom);
				const int32 cmd = NkIaCmdDuVerbe ? NkIaCmdDuVerbe(V[i].act) : -1;
				int32 ecrits = 0;
				for (int32 p = 0; p < np && pos > 0 && pos < (int32)sizeof(ligne); ++p) {
					int32 pc = -1, type = 0;
					const char *lib = "";
					float32 vmin = 0.f, vmax = 0.f;
					if (!demo::Demo3DHostOpParamInfo(p, &pc, &lib, &type, &vmin, &vmax))
						continue;
					if (cmd < 0 || pc != cmd)
						continue;
					pos += snprintf(ligne + pos, sizeof(ligne) - (size_t)pos, "%s%s (%s, %g a %g)",
									ecrits ? ", " : " : ", lib,
									type == 0 ? "0 ou 1" : (type == 1 ? "entier" : "reel"),
									(double)vmin, (double)vmax);
					++ecrits;
				}
				if (pos > 0 && pos < (int32)sizeof(ligne))
					snprintf(ligne + pos, sizeof(ligne) - (size_t)pos, " -- %s\n", V[i].effet);
				ajout(ligne);
			}
			ajout("\nUne valeur hors bornes est ramenee aux bornes ; un parametre en trop est\n");
			ajout("refuse. Si aucune commande ne convient, reponds exactement : aucune\n");
		}

		// ── LA REPONSE -> UN VERBE ─────────────────────────────────────────────
		// ⚠️ ON NE FAIT PAS CONFIANCE A LA FORME DE LA REPONSE. Un modele ajoute
		//    des guillemets, des puces, des blocs de code, une phrase de
		//    politesse. On prend la PREMIERE ligne qui ressemble a un verbe, et
		//    si rien ne ressemble, on REFUSE EN LE DISANT plutot que de soumettre
		//    une phrase au pont -- qui la refuserait avec un motif exact mais
		//    incomprehensible pour Rodolf (« je ne connais pas "voici la
		//    commande :" »).
		inline bool NkIaExtraireVerbe(const char *reponse, char *dst, uint32 taille) {
			if (!dst || taille == 0)
				return false;
			dst[0] = 0;
			if (!reponse)
				return false;
			const char *c = reponse;
			while (*c) {
				// debut de ligne : on saute les ornements usuels
				while (*c == ' ' || *c == '\t' || *c == '-' || *c == '*' || *c == '`' || *c == '"' ||
					   *c == '\'' || *c == '>')
					++c;
				const char *deb = c;
				while (*c && *c != '\n' && *c != '\r')
					++c;
				const char *fin = c;
				while (fin > deb && (fin[-1] == ' ' || fin[-1] == '\t' || fin[-1] == '.' ||
									 fin[-1] == '`' || fin[-1] == '"' || fin[-1] == '\'' ||
									 fin[-1] == ';'))
					--fin;
				if (fin > deb) {
					uint32 n = 0;
					for (const char *p = deb; p < fin && n + 1u < taille; ++p)
						dst[n++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
					dst[n] = 0;
					// Un verbe n'a ni espace ni accent : « subdivide:2 » oui,
					// « voici la commande » non.
					bool plausible = n > 0;
					for (uint32 k = 0; k < n && plausible; ++k) {
						const char x = dst[k];
						const bool ok = (x >= 'a' && x <= 'z') || (x >= '0' && x <= '9') || x == ':' ||
										x == '.' || x == '-' || x == '+' || x == '_';
						if (!ok)
							plausible = false;
					}
					if (plausible)
						return true;
					dst[0] = 0;
				}
				while (*c == '\n' || *c == '\r')
					++c;
			}
			return false;
		}

		// ── LE DORSAL, ET IL EST UN REGLAGE ────────────────────────────────────
		// `NK_IA_CMD` porte le gabarit, deux trous `{invite}` et `{sortie}`.
		// Absent, on compose un defaut a partir de `NK_IA_PYTHON` (sinon
		// `python`) et du script livre. ⚠️ CE DEFAUT N'EST PAS UNE DEPENDANCE :
		// il rend l'application utilisable sans configuration, et il se remplace
		// par une variable le jour ou le dorsal du module arrive.
		struct NkIaCanal {
				converse::NkConverseBackendProcessus dorsal;
				converse::NkEnvoiAsync envoi;
				bool pret = false;
				char motif[256] = {0};

				void Preparer(const char *racine) {
					dorsal.nom = NkString("assistant");
					dorsal.invitePath = NkString("nk3dmodeler_invite.txt");
					dorsal.sortiePath = NkString("nk3dmodeler_reponse.txt");
					if (const char *g = std::getenv("NK_IA_CMD")) {
						if (*g)
							dorsal.gabarit = NkString(g);
					}
					if (dorsal.gabarit.Length() == 0) {
						const char *py = std::getenv("NK_IA_PYTHON");
						char buf[512];
						snprintf(buf, sizeof(buf), "%s \"%s%sTools/Genia/ia_verbe.py\" \"{invite}\" \"{sortie}\"",
								 (py && *py) ? py : "python", racine ? racine : "",
								 (racine && *racine) ? "/" : "");
						dorsal.gabarit = NkString(buf);
					}
					pret = dorsal.IsAvailable();
					if (!pret)
						snprintf(motif, sizeof(motif),
								 "Aucun dorsal de conversation : posez NK_IA_CMD.");
				}
		};

	} // namespace nk3d
} // namespace nkentseu
