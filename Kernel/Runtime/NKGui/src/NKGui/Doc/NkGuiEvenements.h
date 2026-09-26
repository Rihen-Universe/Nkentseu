// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkGuiEvenements.h — QUAND un `behavior` se déclenche.
// =============================================================================
//  C'était la limite (5) de `NkGuiInteraction.h`, écrite noir sur blanc :
//
//      « QUAND un `behavior` se declenche. Le langage ne le dit pas ; c'est
//        `on ... -> Behavior "..."` qui le dirait, et il n'existe pas dans le
//        corpus. L'HOTE tranche donc : il les passe TOUS, une fois par image. »
//
//  Conséquence directe, et elle coûte cher : **tout appel devait être
//  idempotent**. Un `Callback` qui bascule un état était un clignotant à 60 Hz.
//  Un composant ne pouvait pas RÉPONDRE, seulement ÊTRE DANS UN ÉTAT.
//
//  Rodolf, le 26/09 : « penser à les brancher ». Et : « si tu juges que d'autres
//  événements doivent être ajoutés, même pas encore utilisés mais pouvant avoir
//  une utilité définie, alors ajoute-les ».
//
// =============================================================================
//  LA SYNTAXE N'INVENTE RIEN — l'archive la porte déjà
// =============================================================================
//      behavior "boucle"           { ... }   <- chaque image, comme avant
//      behavior "boucle"(Changed)  { ... }   <- SEULEMENT quand la valeur change
//      behavior "sauver"(Click)    { ... }   <- SEULEMENT au clic
//
//  La parenthèse est le même emplacement que `appearance(Hover)` : l'archive le
//  lit déjà (`NkGuiArchive::StateOf`). Aucune grammaire à ajouter, aucun lecteur
//  à retoucher, et les documents existants ne changent pas de conduite.
//
//  ⚠️ STRICTEMENT ADDITIF, ET C'EST LA CONDITION. Un `behavior` SANS parenthèse
//     se comporte EXACTEMENT comme avant : passé à chaque image. C'est ce qui
//     laisse les documents du corpus — et les trois de NkAnimaEditor — tourner
//     sans une ligne de changement.
//
//  📌 Le document 2 §5 prévoit une autre écriture, `on Changed(value) ->
//     Behavior "..."`, qui porte en plus les PARAMÈTRES de l'événement. Elle
//     n'est pas implémentée ici et ce n'est pas un oubli : un paramètre suppose
//     que l'événement en transporte un, et aucun des douze ci-dessous n'en
//     transporte encore. La parenthèse d'état dit QUAND ; le jour où il faudra
//     dire QUOI AVEC, `on` s'ajoutera à côté sans rien casser.
//
// =============================================================================
//  LES DOUZE ÉVÉNEMENTS, ET POURQUOI CHACUN
// =============================================================================
//  Les cinq premiers sont ceux du document 9. Les sept suivants sont ajoutés
//  parce que leur utilité est définie — pas parce qu'ils complétaient une liste.
//
//   ── DÉCLENCHÉS AUJOURD'HUI ────────────────────────────────────────────────
//   Click        le geste de base. Détecté là où l'hôte tire déjà l'action.
//   Changed      une valeur liée a changé depuis l'image d'avant. C'est LUI qui
//                supprime l'obligation d'idempotence : une bascule devient
//                possible.
//   Hover        le pointeur est sur le widget. Sert aux aperçus qui se lancent
//                au survol (la vignette animée d'une `Tile`).
//
//   ── RECONNUS, PAS ENCORE DÉCLENCHÉS ───────────────────────────────────────
//   ⚠️ Ils sont ÉCRITS, VALIDÉS et COMPTÉS (`evenementsSansSource`), jamais
//      silencieusement ignorés. Un document qui les emploie dit son intention et
//      le compteur dit qu'elle n'est pas encore servie. *Ce qui n'aurait pas été
//      écrit serait perdu ; ce qui est écrit s'animera sans réécriture.*
//
//   DoubleClick  renommer dans une liste, ouvrir un élément. NKGui a
//                `NkMouseDoubleClickEvent` ; le monteur ne le relaie pas encore.
//   ContextMenu  le clic droit. Le rôle `ContextMenu` existe au format et n'est
//                pas monté : les deux manques se lèveront ensemble.
//   Focus Blur   savoir QUEL champ l'utilisateur édite — pour un panneau de
//                détails qui suit la saisie, et pour valider en quittant.
//   KeyDown      les raccourcis PROPRES à un composant, actifs seulement quand
//                il a le focus. Sans lui, tout raccourci est global.
//   Wheel        le zoom d'une vue, le défilement d'une piste. La molette est
//                déjà réservée par une règle du dépôt ; l'événement dira qui la
//                consomme.
//   Submit       la touche Entrée dans un champ. Aujourd'hui indiscernable d'un
//                `Changed`, alors que l'un demande d'AGIR et l'autre de SUIVRE.
//   DragStart    prendre un mouvement dans la bibliothèque.
//   Drop         le déposer sur un personnage. Le format a `dropEnabled` côté
//                fenêtre ; rien ne relie encore la fenêtre au document.
// =============================================================================

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nkgui {

		enum class NkGuiEvenement : uint8 {
			Aucun = 0,	 ///< pas de parenthèse : le comportement passe à chaque image
			Click,		 ///< déclenché
			Changed,	 ///< déclenché
			Hover,		 ///< déclenché
			DoubleClick, ///< reconnu, sans source
			ContextMenu, ///< reconnu, sans source
			Focus,		 ///< reconnu, sans source
			Blur,		 ///< reconnu, sans source
			KeyDown,	 ///< reconnu, sans source
			Wheel,		 ///< reconnu, sans source
			Submit,		 ///< reconnu, sans source
			DragStart,	 ///< reconnu, sans source
			Drop,		 ///< reconnu, sans source
			Inconnu		 ///< le mot n'est pas du vocabulaire : REFUSÉ et compté
		};

		/// Vrai si l'événement a une source aujourd'hui. Les autres sont reconnus,
		/// comptés, et ne se déclenchent pas — ce n'est pas la même chose qu'un
		/// mot inconnu, et les deux compteurs sont séparés pour cette raison.
		inline bool NkGuiEvenementADesSources(NkGuiEvenement e) noexcept {
			return e == NkGuiEvenement::Click || e == NkGuiEvenement::Changed
				   || e == NkGuiEvenement::Hover;
		}

		namespace detail {
			inline bool NkGEMotEgal(NkStringView a, const char *b) noexcept {
				const uint32 n = (uint32)a.Size();
				uint32 i = 0;
				for (; i < n && b[i]; ++i) {
					char x = a.Data()[i], y = b[i];
					if (x >= 'A' && x <= 'Z')
						x = (char)(x - 'A' + 'a');
					if (y >= 'A' && y <= 'Z')
						y = (char)(y - 'A' + 'a');
					if (x != y)
						return false;
				}
				return i == n && b[i] == '\0';
			}
		} // namespace detail

		inline NkGuiEvenement NkGuiEvenementDepuisNom(NkStringView n) noexcept {
			using detail::NkGEMotEgal;
			if (n.Size() == 0u)
				return NkGuiEvenement::Aucun;
			if (NkGEMotEgal(n, "Click")) return NkGuiEvenement::Click;
			if (NkGEMotEgal(n, "Changed")) return NkGuiEvenement::Changed;
			if (NkGEMotEgal(n, "Hover")) return NkGuiEvenement::Hover;
			if (NkGEMotEgal(n, "DoubleClick")) return NkGuiEvenement::DoubleClick;
			if (NkGEMotEgal(n, "ContextMenu")) return NkGuiEvenement::ContextMenu;
			if (NkGEMotEgal(n, "Focus")) return NkGuiEvenement::Focus;
			if (NkGEMotEgal(n, "Blur")) return NkGuiEvenement::Blur;
			if (NkGEMotEgal(n, "KeyDown")) return NkGuiEvenement::KeyDown;
			if (NkGEMotEgal(n, "Wheel")) return NkGuiEvenement::Wheel;
			if (NkGEMotEgal(n, "Submit")) return NkGuiEvenement::Submit;
			if (NkGEMotEgal(n, "DragStart")) return NkGuiEvenement::DragStart;
			if (NkGEMotEgal(n, "Drop")) return NkGuiEvenement::Drop;
			return NkGuiEvenement::Inconnu;
		}

		inline const char *NkGuiNomEvenement(NkGuiEvenement e) noexcept {
			switch (e) {
				case NkGuiEvenement::Aucun: return "(chaque image)";
				case NkGuiEvenement::Click: return "Click";
				case NkGuiEvenement::Changed: return "Changed";
				case NkGuiEvenement::Hover: return "Hover";
				case NkGuiEvenement::DoubleClick: return "DoubleClick";
				case NkGuiEvenement::ContextMenu: return "ContextMenu";
				case NkGuiEvenement::Focus: return "Focus";
				case NkGuiEvenement::Blur: return "Blur";
				case NkGuiEvenement::KeyDown: return "KeyDown";
				case NkGuiEvenement::Wheel: return "Wheel";
				case NkGuiEvenement::Submit: return "Submit";
				case NkGuiEvenement::DragStart: return "DragStart";
				case NkGuiEvenement::Drop: return "Drop";
				default: return "(inconnu)";
			}
		}

		// =====================================================================
		//  CE QUI S'EST PRODUIT PENDANT UNE IMAGE
		// =====================================================================
		/**
		 * ⚠️ IL SE VIDE À CHAQUE IMAGE, ET C'EST LE POINT. Un événement est un
		 *    FAIT DATÉ : le garder d'une image sur l'autre rejouerait le
		 *    comportement indéfiniment — c'est-à-dire exactement le défaut que
		 *    cette classe existe pour supprimer.
		 */
		class NkGuiJournalEvenements {
			public:
				struct Fait {
						NkGuiEvenement quoi = NkGuiEvenement::Aucun;
						NkString qui; ///< l'identifiant du widget
				};

				void Vider() noexcept {
					mFaits.Clear();
				}

				void Noter(NkGuiEvenement quoi, NkStringView qui) noexcept {
					if (quoi == NkGuiEvenement::Aucun || qui.Size() == 0u)
						return;
					Fait f;
					f.quoi = quoi;
					f.qui = NkString(qui);
					mFaits.PushBack(f);
				}

				bool ADeclenche(NkGuiEvenement quoi, NkStringView qui) const noexcept {
					for (uint32 i = 0; i < (uint32)mFaits.Size(); ++i)
						if (mFaits[i].quoi == quoi
							&& mFaits[i].qui.Compare(NkString(qui).CStr()) == 0)
							return true;
					return false;
				}

				uint32 Nombre() const noexcept {
					return (uint32)mFaits.Size();
				}

			private:
				NkVector<Fait> mFaits;
		};

		// =====================================================================
		//  LA MÉMOIRE DES VALEURS — `Changed` n'a pas d'autre source
		// =====================================================================
		/**
		 * NKGui ne dit pas « cette valeur vient de changer » : elle dit ce
		 * qu'elle vaut. `Changed` se DÉDUIT donc d'une comparaison avec l'image
		 * précédente, et il faut bien la garder quelque part.
		 *
		 * ⚠️ LA PREMIÈRE IMAGE NE DÉCLENCHE RIEN. Une valeur vue pour la première
		 *    fois n'a pas « changé » : elle est apparue. Traiter l'apparition
		 *    comme un changement ferait partir tous les comportements au premier
		 *    tour, ce qui est exactement le comportement d'avant — en pire,
		 *    parce qu'on croirait avoir un événement.
		 */
		class NkGuiMemoireValeurs {
			public:
				struct Valeur {
						NkString cle;
						bool b = false;
						float32 f = 0.f;
						char texte[256] = {0};
						bool vue = false;
				};

				/// Compare et met à jour. Rend vrai si la valeur a CHANGÉ (donc
				/// jamais au premier passage).
				bool Confronter(NkStringView cle, bool b, float32 f, const char *texte) noexcept {
					Valeur *v = Trouver(cle);
					if (!v) {
						Valeur n;
						n.cle = NkString(cle);
						n.b = b;
						n.f = f;
						Copier(n.texte, texte);
						n.vue = true;
						mV.PushBack(n);
						return false; // apparition, pas changement
					}
					const bool change = (v->b != b) || (v->f != f) || !MemeTexte(v->texte, texte);
					v->b = b;
					v->f = f;
					Copier(v->texte, texte);
					return change;
				}

				void Oublier() noexcept {
					mV.Clear();
				}

			private:
				Valeur *Trouver(NkStringView cle) noexcept {
					for (uint32 i = 0; i < (uint32)mV.Size(); ++i)
						if (mV[i].cle.Compare(NkString(cle).CStr()) == 0)
							return &mV[i];
					return nullptr;
				}
				static void Copier(char *dst, const char *src) noexcept {
					uint32 i = 0;
					if (src)
						for (; i < 255u && src[i]; ++i)
							dst[i] = src[i];
					dst[i] = '\0';
				}
				static bool MemeTexte(const char *a, const char *b) noexcept {
					if (!b)
						return a[0] == '\0';
					uint32 i = 0;
					for (; a[i] && b[i]; ++i)
						if (a[i] != b[i])
							return false;
					return a[i] == b[i];
				}
				NkVector<Valeur> mV;
		};

		/// Ce que la couche d'événements a fait pendant une image.
		struct NkGuiRapportEvenements {
				uint32 faits = 0;				///< événements survenus
				uint32 comportementsDeclenches = 0; ///< `behavior` exécutés
				uint32 comportementsIgnores = 0;	///< leur événement ne s'est pas produit
				uint32 evenementsSansSource = 0;	///< reconnus, pas encore déclenchables
				uint32 evenementsInconnus = 0;		///< le mot n'est pas du vocabulaire
		};

	} // namespace nkgui
} // namespace nkentseu
