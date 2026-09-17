#pragma once
// -----------------------------------------------------------------------------
// @File    DesignChat.h
// @Brief   DISCUTER AVANT DE DESSINER — la conversation, et le document de
//          specification qu'elle produit.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA DEMANDE, MOT POUR MOT
// =============================================================================
//  « on doit pouvoir DISCUTER avec lui AVANT de commencer a designer, car
//    DEFINIR UN DOCUMENT DE SPECIFICATION LIE A CE DESIGN est important. »
//
//  Ce que `DesignAI.h` sait faire depuis le 30/08 est un COUP UNIQUE : une
//  invite, une reponse, un document greffe. Il n'y a ni tour de parole, ni
//  memoire de l'echange, ni rien qui survive a la fenetre. Ce fichier ajoute
//  les deux pieces qui manquent, et RIEN d'autre :
//
//    1. `NkDesignConversation` — des TOURS DE PAROLE. Elle ne touche jamais au
//       document : c'est la garantie que « discuter » ne dessine pas.
//    2. `NkSpecification`      — un DOCUMENT, ecrit sur le disque, qui porte un
//       NOM. Ce nom est ce qui atterrit ensuite dans `origine` sur chaque noeud
//       engendre. C'est la « liaison au design » que Rodolf demande, et elle ne
//       coute rien a fabriquer : le format porte deja `origine` par noeud.
//
// =============================================================================
//  TROIS REGLES QUI NE SONT PAS DU CONFORT — elles viennent de la trajectoire
// =============================================================================
//  Rodolf a pose la meme exigence pour les DEUX generateurs (3D et design) :
//  « rassure-toi qu'il va a la longue permettre d'entrainer un modele plus
//  puissant qui fonctionne sur notre systeme de generation ». Cote 3D, le
//  modeleur ne connait QU'UN contrat de processus externe et ignore tout de
//  TripoSR ; le jour ou un modele entraine chez Rihen le remplace, pas une
//  ligne du modeleur ne bouge. Ce fichier garde EXACTEMENT cette propriete :
//
//  1. **RIEN ICI NE CONNAIT QWEN, NI OLLAMA, NI HTTP.** Tout passe par
//     `NkIDesignBackend`, et par sa seule fonction : `Complete(requete,
//     reponse)`. Aucune capacite propre a un moteur n'est ajoutee au contrat —
//     ni streaming, ni outils, ni jetons, ni fenetre de contexte. Un dorsal
//     futur n'a qu'a savoir rendre du texte.
//  2. **LA CONVERSATION PRODUIT UNE DONNEE, JAMAIS UN EFFET.** `Envoyer`
//     ajoute des tours ; `Extraire` rend une specification ; ECRIRE le
//     document est un geste separe, et GREFFER dans le design en est encore un
//     autre. Un dorsal qui agirait directement sur le document obligerait tous
//     les dorsaux futurs a faire pareil — c'est la porte par laquelle le
//     remplacement deviendrait impossible.
//  3. **LA SPECIFICATION EXISTE SANS AUCUN MODELE.** `DepuisConversation` la
//     fabrique MECANIQUEMENT : les exigences sont ce que l'HUMAIN a dit vouloir,
//     tour par tour. `Affiner` ne fait que reformuler avec un dorsal, et
//     N'ECRASE RIEN s'il echoue. Une specification qui n'existerait qu'avec un
//     modele disponible ne serait pas un document : ce serait une sortie.
//
// =============================================================================
//  CE QUI N'EST PAS LA, ET QUI EST NOMME
// =============================================================================
//  - Aucun asynchrone. `Envoyer` bloque le temps de la reponse du dorsal.
//    Mesure du 14/09 sur le moteur local : ~597 ms par token engendre. Une
//    reponse de conversation (quelques dizaines de tokens) se compte en
//    secondes ; c'est vivable, et c'est la meme dette declaree que le pont
//    GENIA du modeleur (« l'attente est SYNCHRONE [...] la mise sur un fil
//    viendra quand la chaine aura prouve qu'elle tient »).
//  - Aucune fenetre de contexte geree. La transcription entiere part a chaque
//    tour. Un dorsal qui deborde doit le DIRE dans son refus ; ce fichier ne
//    tronque pas en silence.
//  - **texte -> 3D n'existe pas** cote modeleur (seul image -> 3D), et
//    l'entrainement du modele de design n'est pas commence. Nomme, pas commence.
// -----------------------------------------------------------------------------

#include "DesignAI.h"

// ═══════════════════════════════════════════════════════════════════════════
//  LA CONVERSATION A DEMENAGE ; LA SPECIFICATION EST RESTEE
// ═══════════════════════════════════════════════════════════════════════════
//  Les tours, l'historique et l'echappement sont descendus dans
//  `Kernel/System/NKConverse` : ils ne savent rien du design.
//
//  ⚠️ MAIS `NkSpecification` EST RESTEE ICI, ET ELLE A FAILLI PARTIR AVEC.
//     La mesure qui autorisait le demenagement etait « zero occurrence du
//     document dans ce fichier » — et elle est VRAIE. Elle ne prouvait
//     pourtant pas la neutralite : `NkSpecification` ne nomme jamais le
//     document, mais son format s'appelle `nkuispec`, ses champs sont des
//     EXIGENCES de design, et `Panels.h` comme `Probe.h` l'utilisent. Un
//     critere qui cherche un NOM ne trouve que ce nom-la.
//     C'est la construction qui l'a dit, pas la relecture : 20 erreurs,
//     « unknown type name 'NkSpecification' ».
//
//  ⚠️ DEMENAGER, PAS SUPPRIMER : les alias ci-dessous existent pour que PAS
//     UNE LIGNE des appelants ne bouge.
// ═══════════════════════════════════════════════════════════════════════════

#include "NKConverse/NkConverseChat.h"

namespace nkuidesign {

	using NkQui = nkentseu::converse::NkQui;
	using NkDesignTour = nkentseu::converse::NkConverseTour;
	using NkDesignConversation = nkentseu::converse::NkConverseConversation;
	using nkentseu::converse::NkQuiNom;
	using nkentseu::converse::NkQuiParse;
	using nkentseu::converse::NkEchapper;
	using nkentseu::converse::NkDesechapper;

	//  deux analyseurs a tenir d'accord.
	//
	//      nkuispec 1
	//      nom = <l'identite ; c'est ce qui atterrit dans `origine`>
	//      titre = <lisible>
	//      exigence = <une par ligne>
	//      tour moi = <echappe>
	//      tour ia  = <echappe>
	struct NkSpecification {
			NkString nom;   ///< l'identite. VIDE = pas de specification.
			NkString titre; ///< lisible, pour l'affichage
			nkentseu::NkVector<NkString> exigences;
			nkentseu::NkVector<NkDesignTour> tours; ///< la tracabilite : d'ou viennent les exigences

			bool Vide() const {
				return nom.Length() == 0;
			}
			nkentseu::uint32 CountExigences() const {
				return (nkentseu::uint32)exigences.Size();
			}

			// ── FABRIQUER, SANS AUCUN MODELE ────────────────────────────────
			/// Les exigences sont ce que L'HUMAIN a dit vouloir, tour par tour.
			/// Aucune invention : on ne recopie pas les reponses du dorsal dans
			/// les exigences, parce qu'une exigence est une DEMANDE, pas une
			/// suggestion. Les reponses restent dans `tours`, qui est la
			/// tracabilite.
			static void DepuisConversation(const NkDesignConversation &c, const char *nomSpec,
										   NkSpecification &out) {
				out = NkSpecification();
				out.nom = NkString(nomSpec ? nomSpec : "");
				out.titre = c.sujet;
				const nkentseu::NkVector<NkDesignTour> &t = c.Tours();
				for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)t.Size(); ++i) {
					out.tours.PushBack(t[i]);
					if (t[i].qui == NkQui::Moi && t[i].texte.Length() > 0)
						out.exigences.PushBack(t[i].texte);
				}
			}

			// ── AFFINER AVEC UN DORSAL — et n'ecraser QUE si ca reussit ──────
			/// Demande au dorsal de reformuler l'echange en exigences numerotees.
			/// Rend vrai seulement si AU MOINS UNE exigence a ete lue dans sa
			/// reponse ; sinon `pourquoi` nomme le refus et `spec` N'EST PAS
			/// TOUCHEE.
			///
			/// ⚠️ POURQUOI « au moins une » ET PAS « la reponse est non vide » :
			///    un modele qui repond « Bien sur ! Voici : » sans liste rendrait
			///    une reponse non vide et une specification VIDE. Le critere porte
			///    donc sur ce qu'on a su LIRE, pas sur ce qu'on a recu.
			static bool Affiner(const NkDesignConversation &c, NkIDesignBackend *dorsal,
								NkSpecification &spec, NkString &pourquoi) {
				pourquoi = NkString("");
				if (!dorsal) {
					pourquoi = NkString("aucun dorsal branche");
					return false;
				}
				NkDesignRequest req;
				req.prompt = NkString("Voici un echange sur une interface a concevoir.\n");
				req.prompt.Append("Reformule-le en EXIGENCES, une par ligne, chacune commencant\n");
				req.prompt.Append("par « - ». Pas d'introduction, pas de conclusion, pas de code.\n\n");
				if (c.sujet.Length() > 0) {
					req.prompt.Append("Sujet : ");
					req.prompt.Append(c.sujet);
					req.prompt.Append('\n');
				}
				req.prompt.Append("\nEchange :\n");
				c.Transcrire(req.prompt);

				NkDesignReply rep;
				if (!dorsal->Complete(req, rep) || rep.text.Length() == 0) {
					pourquoi = rep.error.Length() > 0 ? rep.error
													  : NkString("le dorsal n'a rien rendu");
					return false;
				}
				nkentseu::NkVector<NkString> lues;
				LireExigences(rep.text.Data(), lues);
				if (lues.Size() == 0) {
					pourquoi = NkString("aucune exigence lisible dans la reponse "
										"(attendu : des lignes commencant par « - »)");
					return false;
				}
				spec.exigences = lues;
				return true;
			}

			/// Les lignes qui commencent par `-`, `*` ou un chiffre suivi de `.`
			/// ou `)`. Publique : la sonde veut l'exercer sans dorsal.
			static void LireExigences(const char *texte, nkentseu::NkVector<NkString> &out) {
				out.Clear();
				const char *l = texte;
				while (l && *l) {
					const char *p = l;
					while (*p == ' ' || *p == '\t')
						++p;
					const char *contenu = nullptr;
					if (*p == '-' || *p == '*') {
						contenu = p + 1;
					} else if (*p >= '0' && *p <= '9') {
						const char *q = p;
						while (*q >= '0' && *q <= '9')
							++q;
						if (*q == '.' || *q == ')')
							contenu = q + 1;
					}
					if (contenu) {
						while (*contenu == ' ' || *contenu == '\t')
							++contenu;
						const char *fin = contenu;
						while (*fin && *fin != '\n' && *fin != '\r')
							++fin;
						if (fin > contenu) {
							NkString e;
							e.Append(contenu, (NkString::SizeType)(fin - contenu));
							out.PushBack(e);
						}
					}
					while (*l && *l != '\n')
						++l;
					if (*l)
						++l;
				}
			}

			// ── ECRIRE ET RELIRE ────────────────────────────────────────────
			void Ecrire(NkString &out) const {
				out = NkString("nkuispec 1\n");
				out.Append("nom = ");
				out.Append(nom);
				out.Append("\ntitre = ");
				out.Append(titre);
				out.Append('\n');
				for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)exigences.Size(); ++i) {
					out.Append("exigence = ");
					NkEchapper(exigences[i], out);
					out.Append('\n');
				}
				for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)tours.Size(); ++i) {
					out.Append("tour ");
					out.Append(NkQuiNom(tours[i].qui));
					out.Append(" = ");
					NkEchapper(tours[i].texte, out);
					out.Append('\n');
				}
			}

			bool Lire(const char *texte) {
				*this = NkSpecification();
				if (!texte)
					return false;
				const char *l = texte;
				bool enTete = false;
				while (*l) {
					const char *fin = l;
					while (*fin && *fin != '\n' && *fin != '\r')
						++fin;
					NkString ligne;
					ligne.Append(l, (NkString::SizeType)(fin - l));
					const char *s = ligne.Data() ? ligne.Data() : "";
					if (!enTete) {
						if (!Commence(s, "nkuispec"))
							return false; // pas notre fichier : on ne devine pas
						enTete = true;
					} else if (Commence(s, "nom = ")) {
						nom = NkString(s + 6);
					} else if (Commence(s, "titre = ")) {
						titre = NkString(s + 8);
					} else if (Commence(s, "exigence = ")) {
						NkString e;
						NkDesechapper(s + 11, e);
						exigences.PushBack(e);
					} else if (Commence(s, "tour moi = ") || Commence(s, "tour ia = ")) {
						NkDesignTour t;
						const bool moi = s[5] == 'm';
						t.qui = moi ? NkQui::Moi : NkQui::IA;
						NkDesechapper(s + (moi ? 11 : 10), t.texte);
						tours.PushBack(t);
					}
					l = fin;
					while (*l == '\n' || *l == '\r')
						++l;
				}
				return enTete;
			}

			/// LE TEXTE QUI PART AU GENERATEUR. Ce n'est PAS le fichier entier :
			/// la transcription n'a rien a y faire — elle documente d'ou vient
			/// l'exigence, elle ne la remplace pas, et l'envoyer diluerait la
			/// demande dans du bavardage.
			void PourLeGenerateur(NkString &out) const {
				out = NkString("");
				if (Vide())
					return;
				out.Append("Specification « ");
				out.Append(titre.Length() > 0 ? titre : nom);
				out.Append(" » — l'interface doit respecter :\n");
				for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)exigences.Size(); ++i) {
					out.Append("- ");
					out.Append(exigences[i]);
					out.Append('\n');
				}
			}

			static bool Commence(const char *s, const char *prefixe) {
				if (!s || !prefixe)
					return false;
				while (*prefixe)
					if (*s++ != *prefixe++)
						return false;
				return true;
			}
	};

} // namespace nkuidesign
