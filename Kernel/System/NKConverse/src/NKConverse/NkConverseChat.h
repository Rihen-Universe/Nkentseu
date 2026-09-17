#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverseChat.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// LA CONVERSATION : des tours, un historique, et le dorsal qu'on interroge.
// Extrait de `NKUIDesign/DesignChat.h`, ou la mesure a montre ZERO occurrence
// du document : ce fichier etait deja neutre, il ne le savait pas.
// -----------------------------------------------------------------------------

#include "NKConverse/NkConverse.h"

namespace nkentseu::converse {


	// ── UN TOUR DE PAROLE ───────────────────────────────────────────────────
	enum class NkQui : nkentseu::uint8 {
		Moi = 0,  ///< l'humain
		IA,		  ///< le dorsal, quel qu'il soit
		Count
	};

	inline const char *NkQuiNom(NkQui q) {
		return q == NkQui::Moi ? "moi" : "ia";
	}
	inline NkQui NkQuiParse(const char *s) {
		return (s && s[0] == 'i' && s[1] == 'a') ? NkQui::IA : NkQui::Moi;
	}

	struct NkConverseTour {
			NkQui qui = NkQui::Moi;
			NkString texte;
	};

	// ── L'ECHAPPEMENT DES SAUTS DE LIGNE ────────────────────────────────────
	// Un tour tient sur UNE ligne dans le fichier ; une reponse de modele, non.
	// On echappe donc `\n` en `\n` (deux caracteres) et `\` en `\\`. Le choix
	// est celui du moindre appareil : le format reste lisible a l'oeil et
	// l'aller-retour est EXACT — la sonde le prouve sur un texte qui contient
	// les deux caracteres pieges.
	inline void NkEchapper(const NkString &in, NkString &out) {
		const char *p = in.Data() ? in.Data() : "";
		for (; *p; ++p) {
			if (*p == '\\')
				out.Append("\\\\");
			else if (*p == '\n')
				out.Append("\\n");
			else if (*p == '\r')
				continue; // un CR seul n'a jamais rien signifie ici
			else
				out.Append(*p);
		}
	}
	inline void NkDesechapper(const char *p, NkString &out) {
		for (; p && *p; ++p) {
			if (*p != '\\') {
				out.Append(*p);
				continue;
			}
			++p;
			if (*p == 'n')
				out.Append('\n');
			else if (*p == '\\')
				out.Append('\\');
			else if (!*p) // un antislash final : on le rend tel quel
				return (void)out.Append('\\');
			else {
				out.Append('\\');
				out.Append(*p);
			}
		}
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  LA CONVERSATION
	// ═══════════════════════════════════════════════════════════════════════
	//  ⚠️ ELLE NE PREND AUCUN DOCUMENT EN PARAMETRE, et ce n'est pas un oubli :
	//     c'est la seule facon de rendre VRAI par construction « discuter ne
	//     dessine pas ». Une conversation qui aurait acces au document
	//     finirait, un jour, par le modifier « juste un peu ».
	class NkConverseConversation {
		public:
			/// Le sujet, pose une fois : de quoi on parle. Il entre dans l'invite
			/// a chaque tour (le dorsal n'a pas de memoire garantie).
			NkString sujet;

			const nkentseu::NkVector<NkConverseTour> &Tours() const {
				return mTours;
			}
			nkentseu::uint32 Count() const {
				return (nkentseu::uint32)mTours.Size();
			}
			bool Vide() const {
				return mTours.Size() == 0;
			}
			void Effacer() {
				mTours.Clear();
			}
			/// Poser un tour a la main — ce qui permet de REJOUER une conversation
			/// enregistree, et a la sonde d'exercer la chaine sans dorsal.
			void Ajouter(NkQui qui, const char *texte) {
				NkConverseTour t;
				t.qui = qui;
				t.texte = NkString(texte ? texte : "");
				mTours.PushBack(t);
			}

			/// ENVOYER. Ajoute le tour humain, demande au dorsal, ajoute le tour
			/// IA. Rend faux avec `erreur` remplie si le dorsal refuse.
			///
			/// ⚠️ SUR REFUS, LE TOUR HUMAIN RESTE ET AUCUN TOUR IA N'EST AJOUTE.
			///    Retirer ce que l'utilisateur vient de taper parce que le modele
			///    n'a pas repondu lui ferait perdre sa phrase — et un tour IA vide
			///    ferait croire que la machine a repondu « rien ».
			bool Envoyer(NkIConverseBackend *dorsal, const char *texte, NkString &erreur) {
				erreur = NkString("");
				if (!texte || !*texte) {
					erreur = NkString("rien a envoyer : l'invite est vide");
					return false;
				}
				Ajouter(NkQui::Moi, texte);
				if (!dorsal) {
					erreur = NkString("aucun dorsal branche");
					return false;
				}
				NkConverseRequest req;
				BatirInvite(req.prompt);
				NkConverseReply rep;
				if (!dorsal->Complete(req, rep) || rep.text.Length() == 0) {
					erreur = rep.error.Length() > 0
								 ? rep.error
								 : NkString("le dorsal n'a rien rendu");
					return false;
				}
				Ajouter(NkQui::IA, rep.text.Data());
				return true;
			}

			/// La transcription lisible — celle qu'on colle dans la specification
			/// et celle que le panneau affiche. Une seule fonction : deux
			/// transcriptions auraient diverge des le premier champ ajoute.
			void Transcrire(NkString &out) const {
				for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)mTours.Size(); ++i) {
					out.Append(NkQuiNom(mTours[i].qui));
					out.Append("> ");
					out.Append(mTours[i].texte);
					out.Append('\n');
				}
			}

			/// L'INVITE DE CONVERSATION. Publique parce que la sonde veut pouvoir
			/// la LIRE : une invite qu'on ne peut pas inspecter est une invite
			/// qu'on croit sur parole.
			///
			/// ⚠️ ELLE NE DEMANDE PAS DE DOCUMENT, ET C'EST TOUT L'INTERET. Elle
			///    demande de POSER DES QUESTIONS et de resumer des besoins. C'est
			///    la difference entre `DesignAI::BuildPrompt` (qui exige un
			///    `nkuidoc` et rejette tout le reste) et celle-ci : ici, du texte
			///    libre est la bonne reponse.
			void BatirInvite(NkString &out) const {
				out = NkString("Tu aides a DEFINIR une interface avant de la dessiner.\n");
				out.Append("Tu ne produis AUCUN document, AUCUN code, AUCUNE maquette a ce stade.\n");
				out.Append("Tu poses des questions courtes et tu resumes les besoins, en francais.\n\n");
				if (sujet.Length() > 0) {
					out.Append("Sujet : ");
					out.Append(sujet);
					out.Append('\n');
				}
				out.Append("\nEchange :\n");
				Transcrire(out);
				out.Append("\nReponds au dernier tour, en francais, en trois phrases au plus.\n");
			}

		private:
			nkentseu::NkVector<NkConverseTour> mTours;
	};

	// ═══════════════════════════════════════════════════════════════════════
	//  LE DOCUMENT DE SPECIFICATION
	// ═══════════════════════════════════════════════════════════════════════
	//  Format `nkuispec 1` — volontairement le MEME patron que `nkuidoc 1` :
	//  une ligne d'en-tete, des cles `nom = valeur`, un mot-cle par element de
	//  liste. Un second style de fichier dans la meme application aurait fait
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
			nkentseu::NkVector<NkConverseTour> tours; ///< la tracabilite : d'ou viennent les exigences

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
			static void DepuisConversation(const NkConverseConversation &c, const char *nomSpec,
										   NkSpecification &out) {
				out = NkSpecification();
				out.nom = NkString(nomSpec ? nomSpec : "");
				out.titre = c.sujet;
				const nkentseu::NkVector<NkConverseTour> &t = c.Tours();
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
			static bool Affiner(const NkConverseConversation &c, NkIConverseBackend *dorsal,
								NkSpecification &spec, NkString &pourquoi) {
				pourquoi = NkString("");
				if (!dorsal) {
					pourquoi = NkString("aucun dorsal branche");
					return false;
				}
				NkConverseRequest req;
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

				NkConverseReply rep;
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
						NkConverseTour t;
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

} // namespace nkentseu::converse
