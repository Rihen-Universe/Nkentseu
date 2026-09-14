#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiInteraction.h
// @Brief   CE QUI S'EXECUTE dans un document `.nkgui` : l'etat d'apparence
//          atteint par l'interaction, le comportement evalue, le `Callback`
//          appele, et la liaison `bind` a une donnee vivante.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA PHRASE QUI A OUVERT CE CHANTIER
// =============================================================================
//  « tu peux dessiner une interface. Tu ne peux pas encore poser un bouton qui
//    fait quelque chose. »
//
//  Elle etait exacte le 2026-09-14 au matin, et elle l'etait pour une raison
//  precise que le monteur nomme lui-meme : les quatre etats hors repos sont
//  « LUS et COMPTES mais jamais peints -- **sans interaction, aucun d'eux ne
//  peut etre atteint** ». Ce fichier ouvre cette interaction, et les etats
//  viennent avec.
//
// =============================================================================
//  CE QUI EXISTAIT DEJA, ET QUE CE FICHIER N'ECRIT DONC PAS
// =============================================================================
//  Mesure du 2026-09-14, par OUVERTURE des fichiers :
//
//   - la machine d'entree : `Core/NkGuiInput.h` tient `mousePos` et
//     `mouseDown[3]` (« brut, pose par l'app ») et derive en `NewFrame()` les
//     `mouseClicked` / `mouseReleased` / `mouseDownDur`. **Rien a ecrire.**
//   - la machine d'etat : `ctx.hotId`, `hotIdPrev`, `activeId`, `inputId`,
//     `BeginDisabled`, et surtout `ButtonBehavior(id, r, ..., &hovered, &held)`.
//     **Rien a ecrire.**
//   - le crochet de re-peinture : `ctx.styleFn` + `NkGuiStyleItem`. **Rien a
//     ecrire** -- mais il n'est cable qu'a QUATRE endroits de
//     `NkGuiWidgets.cpp` (l. 315 Button, 465 CheckMark, 2022 Selectable,
//     103 DockTarget). Le curseur et le champ de saisie n'y passent pas ;
//     c'est dit ici plutot que decouvert en route.
//   - la conversion hexa : `NkParseHex(NkStringView, uint32&)` (NKContainers).
//
//  CE QUI MANQUAIT VRAIMENT, et c'est tout ce que ce fichier ajoute : le widget
//  change bien de couleur au survol -- mais vers `theme.buttonHover`, **jamais
//  vers ce que le fichier declare**. Un document qui ecrit
//  `appearance(Hover) { fill { color = #3A8894 } }` n'avait aucun moyen de se
//  faire entendre.
//
// =============================================================================
//  LA PRIORITE DES ETATS -- elle n'est pas choisie ici, elle est CITEE
// =============================================================================
//  `exemples/valides/10_etats_apparence.nkgui` la tranche, mot pour mot :
//
//      Disabled > Pressed > Hover > FocusVisible > Focus > Normal
//
//  et dit pourquoi : « Le cumul permettrait de produire un rendu que PERSONNE
//  n'a dessine ». `NkGuiResoudreEtat` applique cet ordre et rien d'autre.
//
// =============================================================================
//  LE LANGAGE DE COMPORTEMENT -- il est ECRIT, je ne l'invente pas
// =============================================================================
//  `Applications/NKUIDesign/2_NkUIDesign_Langage_Description_NodeBlueprint.md`
//  §4 le donne en EBNF :
//
//      statement     := assign_stmt | if_stmt | call_stmt
//      assign_stmt   := "set" Identifier '=' expr
//      if_stmt       := "if" expr '{' statement* '}' ("else" '{' statement* '}')?
//      call_stmt     := callback_call
//      callback_call := "Callback" String '(' arg_list? ')'
//      expr          := literal | Identifier | expr op expr | '(' expr ')'
//      op            := '+'|'-'|'*'|'/'|'>'|'<'|'>='|'<='|'=='|'&&'|'||'
//      literal       := String | Number | Color | "true" | "false"
//
//  `NkGuiEvaluateur` implemente CETTE grammaire-la, entierement, y compris le
//  `else`. CE QU'IL NE COUVRE PAS, et la liste est fermee :
//
//   (1) `behavior ... graph` (§6) -- `node` / `wire` / `pin_ref`. Aucun fichier
//       du corpus n'en ecrit un. Une section `graph` est DETECTEE et refusee
//       explicitement (`grapheIgnore`), jamais evaluee a moitie.
//   (2) les litteraux `Color` et `String` dans une EXPRESSION. Le langage les
//       admet ; aucune instruction du corpus n'en porte. Une chaine en
//       expression rend un jeton, une couleur aussi -- ils se comparent par
//       egalite, ils ne s'additionnent pas.
//   (3) `!=`. Il n'est PAS dans la liste d'operateurs du document 2. Ne pas
//       l'ajouter est un choix : le jour ou le document l'ecrit, il s'ajoute.
//   (4) les PARAMETRES D'EVENEMENT. Le document 2 §5 dit que « toute variable
//       non declaree par `set` fait reference a un parametre d'evenement »,
//       transmis par `on Changed(value) -> Behavior "..."`. **Aucun fichier du
//       corpus n'ecrit un seul `on`** : il n'y a donc aucun evenement pour
//       porter ces parametres. Une variable non resolue rend un JETON de son
//       propre nom, ce qui est faux comme nombre et visible comme jeton -- pas
//       un zero silencieux.
//   (5) QUAND un `behavior` se declenche. Le langage ne le dit pas ; c'est
//       `on ... -> Behavior "..."` qui le dirait, et il n'existe pas dans le
//       corpus. L'HOTE tranche donc, et il le dit : `ExecuterComportements()`
//       les passe tous, une fois par image, APRES le montage -- pour que
//       `n1.value` soit la valeur que le widget vient d'avoir, pas celle de
//       l'image d'avant.
//
// =============================================================================
//  OU IL VIT -- meme regle que le monteur, pour la meme raison
// =============================================================================
//  EN-TETE SEUL, non compile. `NKGui.jenga` fait `files(["src/**.cpp"])` : un
//  `.h` n'y ajoute rien, et NKGui ne gagne aucune dependance vers
//  NKSerialization. C'est l'application qui inclut qui declare.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Doc/NkGuiMonteur.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkentseu {
	namespace nkgui {

		// =====================================================================
		//  LES CINQ ETATS (six graphies) -- la liste fermee du 2026-08-27
		// =====================================================================
		enum class NkGuiEtatApp : uint8 {
			Normal = 0,
			Hover,
			Pressed,
			Focus,
			FocusVisible,
			Disabled,
			Compte
		};
		static constexpr uint32 kNkGuiEtatCompte = (uint32)NkGuiEtatApp::Compte;

		inline const char *NkGuiNomEtat(NkGuiEtatApp e) noexcept {
			switch (e) {
				case NkGuiEtatApp::Normal: return "Normal";
				case NkGuiEtatApp::Hover: return "Hover";
				case NkGuiEtatApp::Pressed: return "Pressed";
				case NkGuiEtatApp::Focus: return "Focus";
				case NkGuiEtatApp::FocusVisible: return "FocusVisible";
				case NkGuiEtatApp::Disabled: return "Disabled";
				default: return "?";
			}
		}

		/// L'etat nomme dans `$state`, qui porte la PARENTHESE telle qu'ecrite :
		/// `(Hover)`, `(  Normal  )`. Une absence de `$state` vaut `Normal` --
		/// `appearance { }` et `appearance(Normal) { }` sont le meme etat, le
		/// fichier 10 du corpus le tranche.
		inline NkGuiEtatApp NkGuiEtatDepuisLexeme(NkStringView lex) noexcept {
			if (lex.Size() == 0)
				return NkGuiEtatApp::Normal;
			// FocusVisible AVANT Focus : « Focus » est un prefixe de l'autre, et
			// chercher le court d'abord rendrait `Focus` pour `(FocusVisible)`.
			static const char *kNoms[] = {"FocusVisible", "Disabled", "Pressed",
										  "Hover", "Focus", "Normal"};
			static const NkGuiEtatApp kVals[] = {NkGuiEtatApp::FocusVisible, NkGuiEtatApp::Disabled,
												 NkGuiEtatApp::Pressed, NkGuiEtatApp::Hover,
												 NkGuiEtatApp::Focus, NkGuiEtatApp::Normal};
			for (uint32 k = 0; k < 6u; ++k) {
				const char *m = kNoms[k];
				uint32 lm = 0;
				while (m[lm])
					++lm;
				for (uint32 i = 0; i + lm <= (uint32)lex.Size(); ++i) {
					bool ok = true;
					for (uint32 j = 0; j < lm; ++j)
						if (lex.Data()[i + j] != m[j]) {
							ok = false;
							break;
						}
					if (ok)
						return kVals[k];
				}
			}
			return NkGuiEtatApp::Normal;
		}

		// =====================================================================
		//  UNE COULEUR DU DOCUMENT
		// =====================================================================
		/// `#RRGGBB` ou `#RRGGBBAA`, et rien d'autre (`NkGuiValueKind::Color`).
		/// Le `#` est retire puis la conversion passe par `NkParseHex`, celui de
		/// NKContainers -- je n'en ecris pas un second.
		inline bool NkGuiCouleurDepuisLexeme(NkStringView lex, NkColor &out) noexcept {
			if (lex.Size() < 7 || lex.Data()[0] != '#')
				return false;
			const uint32 n = (uint32)lex.Size() - 1u;
			if (n != 6u && n != 8u)
				return false;
			uint32 v = 0u;
			if (!string::NkParseHex(NkStringView(lex.Data() + 1, (usize)n), v))
				return false;
			if (n == 6u) {
				out.r = (uint8)((v >> 16) & 0xFFu);
				out.g = (uint8)((v >> 8) & 0xFFu);
				out.b = (uint8)(v & 0xFFu);
				out.a = 255u;
			} else {
				out.r = (uint8)((v >> 24) & 0xFFu);
				out.g = (uint8)((v >> 16) & 0xFFu);
				out.b = (uint8)((v >> 8) & 0xFFu);
				out.a = (uint8)(v & 0xFFu);
			}
			return true;
		}

		// =====================================================================
		//  CE QU'UN ETAT DEMANDE DE PEINDRE
		// =====================================================================
		struct NkGuiPeinture {
				bool declare = false; ///< un bloc `appearance` existe pour cet etat
				bool aFond = false;
				NkColor fond{0, 0, 0, 255};
				bool aContour = false;
				NkColor contour{0, 0, 0, 255};
				float32 contourLargeur = 1.f;
				bool aRayon = false;
				float32 rayon = 0.f;
		};

		/// Ce que le document dit d'UN widget : son identite, sa cle d'etat, son
		/// role, s'il est actif, et ses six apparences.
		struct NkGuiInfoWidget {
				NkString id;   ///< `Button "valider"` -> "valider"
				NkString cle;  ///< son `bind` s'il en a un, sinon son id (meme regle que le monteur)
				NkString role; ///< "Button", "TextField"...
				bool actif = true; ///< `enabled = false` le met a faux
				NkGuiPeinture etats[kNkGuiEtatCompte];
		};

		// =====================================================================
		//  LIRE LES APPARENCES -- une passe, sur la meme structure que le monteur
		// =====================================================================
		class NkGuiInfosDoc {
			public:
				void Lire(const NkArchive &doc) noexcept {
					mW.Clear();
					const NkArchiveNode *corps = NkGMonteCorps(doc);
					if (!corps)
						return;
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const NkArchive &sec = *corps->array[i].object;
						if (NkGMotEgal(NkGuiArchive::TypeOf(sec), "widgets"))
							LireCorps(sec);
					}
				}

				const NkGuiInfoWidget *Trouver(NkStringView id) const noexcept {
					for (uint32 i = 0; i < (uint32)mW.Size(); ++i)
						if (mW[i].id.Compare(NkString(id)) == 0)
							return &mW[i];
					return nullptr;
				}

				NkGuiInfoWidget *TrouverMod(NkStringView id) noexcept {
					for (uint32 i = 0; i < (uint32)mW.Size(); ++i)
						if (mW[i].id.Compare(NkString(id)) == 0)
							return &mW[i];
					return nullptr;
				}

				uint32 Taille() const noexcept {
					return (uint32)mW.Size();
				}
				const NkGuiInfoWidget &operator[](uint32 i) const noexcept {
					return mW[i];
				}

				/// Combien d'apparences hors repos le document declare. Ce nombre doit
				/// recouper `NkGuiMonteRapport::etatsNonAppliques` -- deux chemins
				/// independants vers le meme fait, ce qui rend un ecart lisible.
				uint32 EtatsHorsRepos() const noexcept {
					uint32 n = 0;
					for (uint32 i = 0; i < (uint32)mW.Size(); ++i)
						for (uint32 s = 1; s < kNkGuiEtatCompte; ++s)
							if (mW[i].etats[s].declare)
								++n;
					return n;
				}

			private:
				void LireCorps(const NkArchive &bloc) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(bloc);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						const NkArchive &w = *c->array[k].object;
						const NkStringView t = NkGuiArchive::TypeOf(w);
						if (NkGEstApparence(t))
							continue;
						if (NkGuiRoleDepuisNom(t) == NkGuiRole::Inconnu)
							continue;
						NkGuiInfoWidget info;
						info.id = NkString(NkGuiArchive::IdOf(w));
						info.role = NkString(t);
						const NkArchiveNode *b = w.FindNode(NkStringView("bind"));
						info.cle = b ? NkString(b->Lexeme()) : info.id;
						info.actif = NkGBooleen(w, "enabled", true);
						LireApparences(w, info);
						mW.PushBack(info);
						LireCorps(w);
					}
				}

				static void LireApparences(const NkArchive &w, NkGuiInfoWidget &info) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(w);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						const NkArchive &a = *c->array[k].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(a), "appearance"))
							continue;
						const NkGuiEtatApp e = NkGuiEtatDepuisLexeme(NkGuiArchive::StateOf(a));
						NkGuiPeinture &p = info.etats[(uint32)e];
						p.declare = true;
						float32 rad = 0.f;
						if (a.GetFloat32(NkStringView("radius"), rad)) {
							p.aRayon = true;
							p.rayon = rad;
						}
						LireEffets(a, p);
					}
				}

				/// `fill { color = ... }` et `stroke { color = ..., width = ... }`
				/// sont des BLOCS, pas des proprietes : ils vivent dans le `$body`.
				static void LireEffets(const NkArchive &app, NkGuiPeinture &p) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(app);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						const NkArchive &eff = *c->array[k].object;
						const NkStringView t = NkGuiArchive::TypeOf(eff);
						const NkArchiveNode *col = eff.FindNode(NkStringView("color"));
						if (NkGMotEgal(t, "fill")) {
							if (col && NkGuiCouleurDepuisLexeme(col->Lexeme(), p.fond))
								p.aFond = true;
						} else if (NkGMotEgal(t, "stroke")) {
							if (col && NkGuiCouleurDepuisLexeme(col->Lexeme(), p.contour))
								p.aContour = true;
							float32 lw = 1.f;
							if (eff.GetFloat32(NkStringView("width"), lw))
								p.contourLargeur = lw;
						}
						// `shadow` et `blur` sont LUS par le format et NON peints ici :
						// le rasteriseur de ce chantier n'a ni flou ni ombre portee.
						// Le dire vaut mieux que de peindre autre chose a leur place.
					}
				}

				NkVector<NkGuiInfoWidget> mW;
		};

		// =====================================================================
		//  LA RESOLUTION D'ETAT -- l'ordre du document 10, et rien d'autre
		// =====================================================================
		struct NkGuiCondition {
				bool survole = false;
				bool enfonce = false;
				bool focalise = false;
				bool focusClavier = false; ///< le focus vient-il du CLAVIER ?
				bool desactive = false;
		};

		/**
		 * @brief L'apparence EFFECTIVE d'un etat : le repos comme socle, l'etat
		 *        par-dessus, propriete par propriete.
		 *
		 * 🔴 CE QUE L'IMAGE A MONTRE, ET QU'AUCUN COMPTEUR N'AVAIT VU. La premiere
		 *    version peignait l'etat SEUL. `appearance(FocusVisible)` du fichier 10
		 *    ne declare qu'un `stroke` : le bouton perdait donc son fond teal
		 *    (#2F6F7A) et retombait sur le gris du theme des qu'il prenait le focus.
		 *    Les onze criteres de (b1.c) etaient VERTS -- ils comptaient l'anneau,
		 *    et l'anneau etait bien la. **C'est la capture qui a parle.**
		 *
		 * ET LA REGLE N'EST PAS INVENTEE ICI, elle est citee. `10_etats_apparence`
		 * dit : « chez tous les outils qui NOMMENT le repos -- Unity, Godot, WPF,
		 * Figma -- le repos nomme EST le socle ». Un socle, c'est exactement ca :
		 * ce qui reste quand l'etat ne dit rien.
		 *
		 * ⚠️ ET CA NE CONTREDIT PAS « UN SEUL ETAT S'APPLIQUE ». Le fichier interdit
		 *    de CUMULER Hover et Pressed -- deux etats concurrents dont la somme ne
		 *    serait dessinee par personne. Le repos, lui, n'est pas concurrent : il
		 *    est le dessous de tous. Hover ne se melange jamais a Pressed ; les deux
		 *    se posent sur Normal.
		 */
		inline NkGuiPeinture NkGuiPeintureEffective(const NkGuiInfoWidget &info,
													NkGuiEtatApp etat) noexcept {
			const NkGuiPeinture &socle = info.etats[(uint32)NkGuiEtatApp::Normal];
			if (etat == NkGuiEtatApp::Normal)
				return socle;
			const NkGuiPeinture &dessus = info.etats[(uint32)etat];
			NkGuiPeinture r = socle;
			r.declare = socle.declare || dessus.declare;
			if (dessus.aFond) {
				r.aFond = true;
				r.fond = dessus.fond;
			}
			if (dessus.aContour) {
				r.aContour = true;
				r.contour = dessus.contour;
				r.contourLargeur = dessus.contourLargeur;
			}
			if (dessus.aRayon) {
				r.aRayon = true;
				r.rayon = dessus.rayon;
			}
			return r;
		}

		/// ⚠️ UN ETAT QUE LE DOCUMENT NE DECLARE PAS N'EST PAS ATTEINT, il RETOMBE.
		///    Un bouton survole dont le fichier n'ecrit aucun `appearance(Hover)`
		///    doit garder son repos -- pas devenir invisible parce qu'un etat vide
		///    a ete elu. La priorite parcourt donc les etats DECLARES.
		inline NkGuiEtatApp NkGuiResoudreEtat(const NkGuiInfoWidget &info,
											  const NkGuiCondition &c) noexcept {
			const NkGuiEtatApp ordre[] = {NkGuiEtatApp::Disabled, NkGuiEtatApp::Pressed,
										  NkGuiEtatApp::Hover, NkGuiEtatApp::FocusVisible,
										  NkGuiEtatApp::Focus};
			const bool vrai[] = {c.desactive, c.enfonce, c.survole,
								 c.focalise && c.focusClavier, c.focalise};
			for (uint32 i = 0; i < 5u; ++i)
				if (vrai[i] && info.etats[(uint32)ordre[i]].declare)
					return ordre[i];
			return NkGuiEtatApp::Normal;
		}

		// =====================================================================
		//  UNE VALEUR DU LANGAGE DE COMPORTEMENT
		// =====================================================================
		/// ⚠️ TROIS TYPES, ET LE TROISIEME N'EST PAS UN REPLI. `Enum.Haut` n'est ni
		///    un nombre ni un booleen : c'est un JETON, et le critere demande de
		///    verifier l'argument RECU, pas seulement l'appel. Le reduire a 0.0
		///    aurait rendu un compteur vert sur un argument perdu.
		struct NkGuiValeur {
				enum class Type : uint8 { Nombre = 0, Booleen, Jeton };
				Type type = Type::Nombre;
				float32 nombre = 0.f;
				bool booleen = false;
				NkString jeton;

				static NkGuiValeur DeNombre(float32 v) noexcept {
					NkGuiValeur r;
					r.type = Type::Nombre;
					r.nombre = v;
					return r;
				}
				static NkGuiValeur DeBooleen(bool v) noexcept {
					NkGuiValeur r;
					r.type = Type::Booleen;
					r.booleen = v;
					r.nombre = v ? 1.f : 0.f;
					return r;
				}
				static NkGuiValeur DeJeton(NkStringView s) noexcept {
					NkGuiValeur r;
					r.type = Type::Jeton;
					r.jeton = NkString(s);
					return r;
				}
				float32 EnNombre() const noexcept {
					return type == Type::Booleen ? (booleen ? 1.f : 0.f) : nombre;
				}
				bool EnBooleen() const noexcept {
					if (type == Type::Booleen)
						return booleen;
					if (type == Type::Jeton)
						return jeton.Size() > 0;
					return nombre != 0.f;
				}
		};

		/// Un appel de `Callback` qui a REELLEMENT atteint l'application.
		struct NkGuiAppelCallback {
				NkString nom;			  ///< `Callback "alerte"` -> "alerte"
				NkString comportement;	  ///< le `behavior` d'ou il part
				static constexpr uint32 ArgMax = 4;
				NkGuiValeur args[ArgMax];
				uint32 nbArgs = 0;
		};

		using NkGuiCallbackFn = void (*)(const NkGuiAppelCallback &, void *);

		// =====================================================================
		//  LE MODELE -- la donnee VIVANTE derriere `bind = modele.valeur`
		// =====================================================================
		/**
		 * @brief Les chemins de donnees que le document lie, et leur valeur.
		 *
		 * ⚠️ IL EST DEHORS, ET C'EST LE POINT. Le monteur garde deja la valeur
		 *    editee dans `NkGuiMonteEtat`, cle par `bind` -- mais cette valeur
		 *    naissait du FICHIER et y restait. Un modele est ce que l'application
		 *    possede : elle peut l'ecrire (le widget bouge) et le relire (le
		 *    widget l'a change). Sans lui, `bind` n'est qu'un nom de rangement.
		 */
		class NkGuiModele {
			public:
				void Poser(NkStringView chemin, float32 v) noexcept {
					const int32 i = Index(chemin);
					if (i >= 0) {
						mV[(uint32)i].valeur = v;
						++mV[(uint32)i].ecritures;
						return;
					}
					Entree e;
					e.chemin = NkString(chemin);
					e.valeur = v;
					e.ecritures = 1u;
					mV.PushBack(e);
				}

				bool Lire(NkStringView chemin, float32 &out) const noexcept {
					const int32 i = Index(chemin);
					if (i < 0)
						return false;
					out = mV[(uint32)i].valeur;
					return true;
				}

				bool Existe(NkStringView chemin) const noexcept {
					return Index(chemin) >= 0;
				}

				/// Combien de fois ce chemin a ete ECRIT. Le compteur du critere (b4) :
				/// « bouger le widget -> la donnee change » se mesure sur une ecriture
				/// de plus, pas sur une valeur qui se trouve egale.
				uint32 Ecritures(NkStringView chemin) const noexcept {
					const int32 i = Index(chemin);
					return i < 0 ? 0u : mV[(uint32)i].ecritures;
				}

				uint32 Taille() const noexcept {
					return (uint32)mV.Size();
				}

			private:
				struct Entree {
						NkString chemin;
						float32 valeur = 0.f;
						uint32 ecritures = 0u;
				};
				int32 Index(NkStringView c) const noexcept {
					for (uint32 i = 0; i < (uint32)mV.Size(); ++i)
						if (mV[i].chemin.Compare(NkString(c)) == 0)
							return (int32)i;
					return -1;
				}
				NkVector<Entree> mV;
		};

		// =====================================================================
		//  L'EVALUATEUR -- la grammaire du document 2 §4, et elle seule
		// =====================================================================
		/// Ce qu'une passe d'evaluation a fait. Un evaluateur qui n'a rien lu doit
		/// annoncer autre chose qu'un evaluateur dont tout a echoue.
		struct NkGuiRapportEval {
				uint32 instructions = 0;   ///< instructions RECONNUES et executees
				uint32 affectations = 0;   ///< `set`
				uint32 conditions = 0;	   ///< `if` rencontres
				uint32 branchesPrises = 0; ///< `if` dont la condition etait vraie
				uint32 branchesElse = 0;   ///< `else` empruntes
				uint32 appels = 0;		   ///< `Callback` reellement appeles
				uint32 refusees = 0;	   ///< instructions non reconnues (compte, jamais devinees)
				uint32 grapheIgnore = 0;   ///< sections `behavior ... graph`, refusees en bloc
				uint32 variables = 0;	   ///< variables locales posees par `set`
		};

		/// Resolution d'un chemin pointe vers une valeur. Rendue par l'hote.
		using NkGuiResolveurFn = bool (*)(NkStringView chemin, NkGuiValeur &out, void *user);

		class NkGuiEvaluateur {
			public:
				NkGuiResolveurFn resolveur = nullptr;
				void *resolveurUser = nullptr;
				NkGuiCallbackFn rappel = nullptr;
				void *rappelUser = nullptr;

				/// Les appels emis pendant la derniere execution (pour les mesurer).
				NkVector<NkGuiAppelCallback> appels;
				NkGuiRapportEval rapport;

				void Reinitialiser() noexcept {
					appels.Clear();
					mVars.Clear();
					rapport = NkGuiRapportEval();
				}

				/// Toutes les sections `behavior` du document, dans l'ordre du fichier.
				void ExecuterDocument(const NkArchive &doc) noexcept {
					const NkArchiveNode *corps = NkGMonteCorps(doc);
					if (!corps)
						return;
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const NkArchive &sec = *corps->array[i].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(sec), "behavior"))
							continue;
						ExecuterSection(sec);
					}
					rapport.variables = (uint32)mVars.Size();
				}

				/// Une section `behavior "nom" { ... }`.
				void ExecuterSection(const NkArchive &sec) noexcept {
					// ⚠️ `behavior "x" graph { ... }` : le mot `graph` s'intercale entre
					//    l'identifiant et l'accolade. Le lecteur, purement syntaxique,
					//    ne reconnait pas cette forme comme un bloc -- elle tombe donc
					//    en tranche brute et n'arrive meme pas ici. Le controle reste,
					//    parce qu'une evolution du lecteur la ferait arriver, et qu'un
					//    graphe evalue a moitie est pire que pas evalue du tout.
					if (sec.FindNode(NkStringView("graph"))) {
						++rapport.grapheIgnore;
						return;
					}
					mComportement = NkString(NkGuiArchive::IdOf(sec));
					const NkArchiveNode *c = NkGMonteCorps(sec);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						const NkArchiveNode &n = c->array[k];
						if (!n.IsScalar())
							continue; // un BLOC dans un behavior : pas une instruction
						ExecuterTranche(n.Lexeme());
					}
				}

				/// Une tranche de source verbatim : `set r = ...`, ou le `if` ENTIER
				/// (corps compris -- `SpanEnd` court jusqu'a l'equilibre des accolades).
				void ExecuterTranche(NkStringView src) noexcept {
					mSrc = NkString(src);
					mI = 0;
					Lexer();
					mT = 0;
					ExecuterBloc(true);
				}

				/// Valeur d'une variable locale posee par `set` (pour la mesurer).
				bool Variable(NkStringView nom, NkGuiValeur &out) const noexcept {
					for (uint32 i = 0; i < (uint32)mVars.Size(); ++i)
						if (mVars[i].nom.Compare(NkString(nom)) == 0) {
							out = mVars[i].v;
							return true;
						}
					return false;
				}

			private:
				// ── jetons ───────────────────────────────────────────────────
				enum class Tk : uint8 { Fin = 0, Ident, Nombre, Chaine, Sym };
				struct Jeton {
						Tk k = Tk::Fin;
						NkString t;
						float32 n = 0.f;
				};
				struct Var {
						NkString nom;
						NkGuiValeur v;
				};

				NkString mSrc;
				uint32 mI = 0;
				NkVector<Jeton> mJ;
				uint32 mT = 0;
				NkVector<Var> mVars;
				NkString mComportement;

				static bool EstLettre(char c) noexcept {
					return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
				}
				static bool EstChiffre(char c) noexcept {
					return c >= '0' && c <= '9';
				}

				void Lexer() noexcept {
					mJ.Clear();
					const char *p = mSrc.Data();
					const uint32 n = (uint32)mSrc.Size();
					uint32 i = 0;
					while (i < n) {
						const char c = p[i];
						if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
							++i;
							continue;
						}
						// Un commentaire `//` peut suivre une instruction sur sa ligne.
						if (c == '/' && i + 1 < n && p[i + 1] == '/') {
							while (i < n && p[i] != '\n')
								++i;
							continue;
						}
						Jeton j;
						if (EstLettre(c)) {
							const uint32 d = i;
							while (i < n && (EstLettre(p[i]) || EstChiffre(p[i]) || p[i] == '.'))
								++i;
							j.k = Tk::Ident;
							j.t = NkString(p + d, (NkString::SizeType)(i - d));
						} else if (EstChiffre(c)
								   || (c == '-' && i + 1 < n && EstChiffre(p[i + 1])
									   && PrecedentEstOperateur())) {
							const uint32 d = i;
							if (p[i] == '-')
								++i;
							while (i < n && EstChiffre(p[i]))
								++i;
							if (i < n && p[i] == '.') {
								++i;
								while (i < n && EstChiffre(p[i]))
									++i;
							}
							j.k = Tk::Nombre;
							j.t = NkString(p + d, (NkString::SizeType)(i - d));
							j.n = EnFlottant(j.t);
						} else if (c == '"') {
							++i;
							const uint32 d = i;
							while (i < n && p[i] != '"') {
								if (p[i] == '\\' && i + 1 < n)
									++i;
								++i;
							}
							j.k = Tk::Chaine;
							j.t = NkString(p + d, (NkString::SizeType)(i - d));
							if (i < n)
								++i; // le guillemet fermant
						} else {
							// Les operateurs a deux caracteres d'abord : sinon `>=` se
							// lirait `>` puis `=`, et la comparaison changerait de sens.
							const bool deux =
								(i + 1 < n)
								&& ((c == '>' && p[i + 1] == '=') || (c == '<' && p[i + 1] == '=')
									|| (c == '=' && p[i + 1] == '=') || (c == '&' && p[i + 1] == '&')
									|| (c == '|' && p[i + 1] == '|'));
							j.k = Tk::Sym;
							j.t = NkString(p + i, (NkString::SizeType)(deux ? 2u : 1u));
							i += deux ? 2u : 1u;
						}
						mJ.PushBack(j);
					}
					Jeton f;
					f.k = Tk::Fin;
					mJ.PushBack(f);
				}

				/// Un `-` colle a un chiffre est un SIGNE quand ce qui precede est un
				/// operateur ou une ouverture, une SOUSTRACTION sinon. Sans cette
				/// question, `a - 1` se lisait `a` puis `-1` -- deux valeurs de suite,
				/// et l'expression s'arretait la sans rien dire.
				bool PrecedentEstOperateur() const noexcept {
					if (mJ.Empty())
						return true;
					const Jeton &d = mJ[(uint32)mJ.Size() - 1u];
					if (d.k == Tk::Sym)
						return !Sym(d, ")");
					return false;
				}

				static float32 EnFlottant(const NkString &s) noexcept {
					const char *p = s.Data();
					const uint32 n = (uint32)s.Size();
					uint32 i = 0;
					float32 signe = 1.f;
					if (i < n && p[i] == '-') {
						signe = -1.f;
						++i;
					}
					float32 v = 0.f;
					while (i < n && EstChiffre(p[i]))
						v = v * 10.f + (float32)(p[i++] - '0');
					if (i < n && p[i] == '.') {
						++i;
						float32 d = 0.1f;
						while (i < n && EstChiffre(p[i])) {
							v += (float32)(p[i++] - '0') * d;
							d *= 0.1f;
						}
					}
					return signe * v;
				}

				static bool Sym(const Jeton &j, const char *s) noexcept {
					return j.k == Tk::Sym && NkGMotEgal(NkStringView(j.t.Data(), (usize)j.t.Size()), s);
				}
				static bool Mot(const Jeton &j, const char *s) noexcept {
					return j.k == Tk::Ident && NkGMotEgal(NkStringView(j.t.Data(), (usize)j.t.Size()), s);
				}
				const Jeton &J() const noexcept {
					return mJ[mT];
				}
				const Jeton &J(uint32 d) const noexcept {
					const uint32 i = mT + d;
					return mJ[i < (uint32)mJ.Size() ? i : (uint32)mJ.Size() - 1u];
				}

				// ── instructions ─────────────────────────────────────────────
				/// `racine` : la tranche entiere (pas de `}` fermante a consommer).
				void ExecuterBloc(bool racine) noexcept {
					while (J().k != Tk::Fin) {
						if (!racine && Sym(J(), "}")) {
							++mT;
							return;
						}
						if (!Instruction())
							return;
					}
				}

				/// Avance jusqu'apres le bloc `{ ... }` courant SANS rien executer --
				/// c'est la branche non prise d'un `if`. Un saut naif sur la premiere
				/// `}` sauterait le mauvais bloc des qu'un `if` en imbrique un autre.
				void SauterBloc() noexcept {
					if (!Sym(J(), "{"))
						return;
					++mT;
					uint32 prof = 1;
					while (J().k != Tk::Fin && prof > 0) {
						if (Sym(J(), "{"))
							++prof;
						else if (Sym(J(), "}"))
							--prof;
						++mT;
					}
				}

				bool Instruction() noexcept {
					if (Mot(J(), "set"))
						return Affectation();
					if (Mot(J(), "if"))
						return Condition();
					if (Mot(J(), "Callback"))
						return Appel();
					// Rien de connu : on COMPTE et on s'arrete, on ne devine pas.
					++rapport.refusees;
					return false;
				}

				bool Affectation() noexcept {
					++mT; // `set`
					if (J().k != Tk::Ident) {
						++rapport.refusees;
						return false;
					}
					const NkString cible = J().t;
					++mT;
					if (!Sym(J(), "=")) {
						++rapport.refusees;
						return false;
					}
					++mT;
					const NkGuiValeur v = Expression();
					PoserVar(cible, v);
					++rapport.affectations;
					++rapport.instructions;
					return true;
				}

				bool Condition() noexcept {
					++mT; // `if`
					const NkGuiValeur c = Expression();
					++rapport.conditions;
					++rapport.instructions;
					if (!Sym(J(), "{")) {
						++rapport.refusees;
						return false;
					}
					if (c.EnBooleen()) {
						++rapport.branchesPrises;
						++mT; // `{`
						ExecuterBloc(false);
						if (Mot(J(), "else")) {
							++mT;
							SauterBloc();
						}
					} else {
						SauterBloc();
						if (Mot(J(), "else")) {
							++mT;
							if (Sym(J(), "{")) {
								++rapport.branchesElse;
								++mT;
								ExecuterBloc(false);
							}
						}
					}
					return true;
				}

				bool Appel() noexcept {
					++mT; // `Callback`
					if (J().k != Tk::Chaine) {
						++rapport.refusees;
						return false;
					}
					NkGuiAppelCallback a;
					a.nom = J().t;
					a.comportement = mComportement;
					++mT;
					if (!Sym(J(), "(")) {
						++rapport.refusees;
						return false;
					}
					++mT;
					while (!Sym(J(), ")") && J().k != Tk::Fin) {
						const NkGuiValeur v = Expression();
						if (a.nbArgs < NkGuiAppelCallback::ArgMax)
							a.args[a.nbArgs] = v;
						++a.nbArgs;
						if (Sym(J(), ","))
							++mT;
						else
							break;
					}
					if (Sym(J(), ")"))
						++mT;
					appels.PushBack(a);
					++rapport.appels;
					++rapport.instructions;
					if (rappel)
						rappel(a, rappelUser);
					return true;
				}

				// ── expressions (precedence du document 2 §4) ────────────────
				NkGuiValeur Expression() noexcept {
					return OuLogique();
				}

				NkGuiValeur OuLogique() noexcept {
					NkGuiValeur a = EtLogique();
					while (Sym(J(), "||")) {
						++mT;
						const NkGuiValeur b = EtLogique();
						a = NkGuiValeur::DeBooleen(a.EnBooleen() || b.EnBooleen());
					}
					return a;
				}

				NkGuiValeur EtLogique() noexcept {
					NkGuiValeur a = Egalite();
					while (Sym(J(), "&&")) {
						++mT;
						const NkGuiValeur b = Egalite();
						a = NkGuiValeur::DeBooleen(a.EnBooleen() && b.EnBooleen());
					}
					return a;
				}

				NkGuiValeur Egalite() noexcept {
					NkGuiValeur a = Comparaison();
					while (Sym(J(), "==")) {
						++mT;
						const NkGuiValeur b = Comparaison();
						// Deux JETONS se comparent par leur texte : `Enum.Haut == Enum.Haut`
						// est vrai, et le reduire a un nombre l'aurait rendu vrai pour
						// n'importe quelle paire de jetons.
						if (a.type == NkGuiValeur::Type::Jeton && b.type == NkGuiValeur::Type::Jeton)
							a = NkGuiValeur::DeBooleen(a.jeton.Compare(b.jeton) == 0);
						else
							a = NkGuiValeur::DeBooleen(a.EnNombre() == b.EnNombre());
					}
					return a;
				}

				NkGuiValeur Comparaison() noexcept {
					NkGuiValeur a = Somme();
					for (;;) {
						const char *op = nullptr;
						if (Sym(J(), ">="))
							op = ">=";
						else if (Sym(J(), "<="))
							op = "<=";
						else if (Sym(J(), ">"))
							op = ">";
						else if (Sym(J(), "<"))
							op = "<";
						else
							return a;
						++mT;
						const NkGuiValeur b = Somme();
						const float32 x = a.EnNombre(), y = b.EnNombre();
						bool r = false;
						if (op[0] == '>')
							r = (op[1] == '=') ? (x >= y) : (x > y);
						else
							r = (op[1] == '=') ? (x <= y) : (x < y);
						a = NkGuiValeur::DeBooleen(r);
					}
				}

				NkGuiValeur Somme() noexcept {
					NkGuiValeur a = Produit();
					for (;;) {
						if (Sym(J(), "+")) {
							++mT;
							a = NkGuiValeur::DeNombre(a.EnNombre() + Produit().EnNombre());
						} else if (Sym(J(), "-")) {
							++mT;
							a = NkGuiValeur::DeNombre(a.EnNombre() - Produit().EnNombre());
						} else
							return a;
					}
				}

				NkGuiValeur Produit() noexcept {
					NkGuiValeur a = Primaire();
					for (;;) {
						if (Sym(J(), "*")) {
							++mT;
							a = NkGuiValeur::DeNombre(a.EnNombre() * Primaire().EnNombre());
						} else if (Sym(J(), "/")) {
							++mT;
							const float32 d = Primaire().EnNombre();
							// Une division par zero rend zero ET se compte : la taire
							// donnerait un infini qui se propage sans laisser de trace.
							if (d == 0.f) {
								++rapport.refusees;
								a = NkGuiValeur::DeNombre(0.f);
							} else
								a = NkGuiValeur::DeNombre(a.EnNombre() / d);
						} else
							return a;
					}
				}

				NkGuiValeur Primaire() noexcept {
					if (Sym(J(), "(")) {
						++mT;
						const NkGuiValeur v = Expression();
						if (Sym(J(), ")"))
							++mT;
						return v;
					}
					if (Sym(J(), "-")) {
						++mT;
						return NkGuiValeur::DeNombre(-Primaire().EnNombre());
					}
					if (J().k == Tk::Nombre) {
						const float32 v = J().n;
						++mT;
						return NkGuiValeur::DeNombre(v);
					}
					if (J().k == Tk::Chaine) {
						const NkString s = J().t;
						++mT;
						return NkGuiValeur::DeJeton(NkStringView(s.Data(), (usize)s.Size()));
					}
					if (J().k == Tk::Ident) {
						const NkString nom = J().t;
						++mT;
						if (NkGMotEgal(NkStringView(nom.Data(), (usize)nom.Size()), "true"))
							return NkGuiValeur::DeBooleen(true);
						if (NkGMotEgal(NkStringView(nom.Data(), (usize)nom.Size()), "false"))
							return NkGuiValeur::DeBooleen(false);
						NkGuiValeur v;
						if (Variable(NkStringView(nom.Data(), (usize)nom.Size()), v))
							return v;
						if (resolveur
							&& resolveur(NkStringView(nom.Data(), (usize)nom.Size()), v,
										 resolveurUser))
							return v;
						// Non resolu : un JETON de son propre nom. Voir la limite (4)
						// de l'en-tete -- faux comme nombre, mais VISIBLE.
						return NkGuiValeur::DeJeton(NkStringView(nom.Data(), (usize)nom.Size()));
					}
					++rapport.refusees;
					return NkGuiValeur::DeNombre(0.f);
				}

				void PoserVar(const NkString &nom, const NkGuiValeur &v) noexcept {
					for (uint32 i = 0; i < (uint32)mVars.Size(); ++i)
						if (mVars[i].nom.Compare(nom) == 0) {
							mVars[i].v = v;
							return;
						}
					Var nv;
					nv.nom = nom;
					nv.v = v;
					mVars.PushBack(nv);
				}
		};

		// =====================================================================
		//  L'EXECUTION -- ce qui relie tout, et le seul objet que l'hote voit
		// =====================================================================
		/**
		 * @brief Le document monte, PUIS execute : etats peints, comportement
		 *        evalue, callbacks appeles, liaisons vivantes.
		 *
		 * ⚠️ RIEN ICI N'INJECTE D'ENTREE. `PoserPointeur` et `PoserBouton` ecrivent
		 *    dans `ctx.input.mousePos` / `ctx.input.mouseDown[]`, c'est-a-dire
		 *    exactement les champs que `NkGuiInput.h` declare « brut, pose par
		 *    l'app ». Ce sont les fonctions que la boucle d'evenements appelle ;
		 *    aucune API systeme n'est touchee, aucun clic n'est simule au niveau
		 *    de l'OS.
		 */
		class NkGuiExecution : public NkGuiMonteHooks {
			public:
				NkGuiInfosDoc infos;
				NkGuiModele modele;
				NkGuiEvaluateur eval;
				NkGuiMonteEtat *etat = nullptr; ///< pose par l'hote (le magasin du monteur)

				/// Combien de widgets ont ete peints avec l'apparence du DOCUMENT, et
				/// dans quel etat. Le compteur du critere (b1) : il commence a zero et
				/// ce zero se prouve (aucun widget peint tant qu'on n'a pas monte).
				uint32 peintsParEtat[kNkGuiEtatCompte] = {};
				uint32 peints = 0;
				/// Widgets pour lesquels le crochet a ete appele mais dont le document
				/// ne declare AUCUNE apparence : NKGui garde son defaut, et ca se
				/// compte au lieu de se deviner.
				uint32 laissesAuTheme = 0;

				// ── ce que l'evenement appelle, et rien d'autre ──────────────
				void PoserPointeur(NkGuiContext &ctx, float32 x, float32 y) noexcept {
					ctx.input.mousePos.x = x;
					ctx.input.mousePos.y = y;
				}
				void PoserBouton(NkGuiContext &ctx, int32 bouton, bool enfonce) noexcept {
					if (bouton >= 0 && bouton < 3)
						ctx.input.mouseDown[bouton] = enfonce;
				}

				/// Le focus. NKGui n'en a pas pour un bouton (`inputId` ne vaut que
				/// pour un champ de saisie) : il vit donc ici, et il porte SON ORIGINE.
				/// ⚠️ L'ORIGINE N'EST PAS UN DETAIL. `10_etats_apparence.nkgui` :
				///    « un anneau qui apparaitrait au clic de souris est precisement
				///    ce que `:focus-visible` a ete invente pour eviter ». Un focus
				///    pris a la souris ne doit donc PAS peindre `FocusVisible`.
				void DonnerFocus(NkStringView id, bool parClavier) noexcept {
					mFocus = NkString(id);
					mFocusClavier = parClavier;
				}
				void RetirerFocus() noexcept {
					mFocus.Clear();
					mFocusClavier = false;
				}
				NkStringView Focus() const noexcept {
					return NkStringView(mFocus.Data(), (usize)mFocus.Size());
				}

				/// A appeler une fois, apres `infos.Lire(doc)` : branche le crochet de
				/// style et les resolveurs de l'evaluateur.
				void Brancher(NkGuiContext &ctx) noexcept {
					ctx.styleFn = &NkGuiExecution::Style;
					ctx.styleUser = this;
					eval.resolveur = &NkGuiExecution::Resoudre;
					eval.resolveurUser = this;
				}

				void Debrancher(NkGuiContext &ctx) noexcept {
					if (ctx.styleUser == this) {
						ctx.styleFn = nullptr;
						ctx.styleUser = nullptr;
					}
				}

				void ReinitialiserCompteurs() noexcept {
					peints = 0;
					laissesAuTheme = 0;
					for (uint32 i = 0; i < kNkGuiEtatCompte; ++i)
						peintsParEtat[i] = 0;
				}

				/// Les comportements, APRES le montage -- voir la limite (5).
				void ExecuterComportements(const NkArchive &doc) noexcept {
					eval.Reinitialiser();
					eval.ExecuterDocument(doc);
				}

				// ── crochet du monteur ───────────────────────────────────────
				void Avant(NkGuiContext &ctx, const NkArchive &w, NkStringView role,
						   NkGuiMonteEtat::Entree *e) noexcept override {
					(void)role;
					mCourant = infos.TrouverMod(NkGuiArchive::IdOf(w));
					mDesactivePousse = false;
					if (mCourant && !mCourant->actif) {
						ctx.BeginDisabled(true);
						mDesactivePousse = true;
					}
					// La donnee vivante entre dans la valeur editee AVANT le widget.
					if (e && mCourant) {
						float32 v = 0.f;
						if (modele.Lire(NkStringView(mCourant->cle.Data(),
													 (usize)mCourant->cle.Size()),
										v)) {
							e->f = v;
							e->initialise = true; // le modele fait foi, pas le fichier
						}
					}
				}

				void Apres(NkGuiContext &ctx, const NkArchive &w, NkStringView role,
						   NkGuiMonteEtat::Entree *e) noexcept override {
					// L'anneau de focus d'un champ de saisie : `InputText` n'a AUCUN
					// appel a `StyleDraw` (mesure : 4 sites, aucun n'est le sien).
					// Il se peint donc ici, par-dessus, et seulement s'il est focalise.
					if (mCourant && NkGMotEgal(role, "TextField"))
						PeindreContourChamp(ctx);

					// Et ce que l'utilisateur en a fait ressort vers la donnee vivante.
					if (e && mCourant && e->initialise) {
						const NkStringView cle(mCourant->cle.Data(), (usize)mCourant->cle.Size());
						if (modele.Existe(cle)) {
							float32 v = 0.f;
							modele.Lire(cle, v);
							if (v != e->f)
								modele.Poser(cle, e->f);
						}
					}
					if (mDesactivePousse) {
						ctx.EndDisabled();
						mDesactivePousse = false;
					}
					mCourant = nullptr;
					(void)w;
				}

			private:
				NkGuiInfoWidget *mCourant = nullptr;
				bool mDesactivePousse = false;
				NkString mFocus;
				bool mFocusClavier = false;

				bool EstFocalise(const NkGuiInfoWidget &i) const noexcept {
					return mFocus.Size() > 0 && mFocus.Compare(i.id) == 0;
				}

				void PeindreContourChamp(NkGuiContext &ctx) noexcept {
					NkGuiCondition c;
					c.focalise = EstFocalise(*mCourant);
					c.focusClavier = mFocusClavier;
					c.desactive = !mCourant->actif;
					const NkGuiEtatApp s = NkGuiResoudreEtat(*mCourant, c);
					const NkGuiPeinture p = NkGuiPeintureEffective(*mCourant, s);
					if (!p.declare || !p.aContour)
						return;
					// `layout.prevItem` est le rectangle du dernier widget auto-place --
					// le monteur a paye la lecon : `lastItemRect` n'est pose QUE par les
					// widgets interactifs, et il rendrait le rect du dernier BOUTON.
					//
					// 🔴 MAIS `prevItem` EST LA RANGEE ENTIERE, PAS LE CHAMP. Vu sur
					//    l'image `b1_d_champ_focus.png` : l'anneau orange debordait sur
					//    le libelle « recherche » ecrit a sa droite. `InputTextEx`
					//    (NkGuiWidgets.cpp l. 958-963) coupe la rangee :
					//        labelW = MeasureWidth(label, LabelEnd(label)) + 14
					//        field  = { row.x, row.y, row.w - labelW, row.h }
					//    Le meme calcul, ici, et l'anneau epouse le champ. Un compteur
					//    de pixels de la couleur orange etait vert dans les deux cas.
					const char *lbl = mCourant->id.CStr();
					const NkRect rang = ctx.layout.prevItem;
					float32 largeurLbl = 0.f;
					if (ctx.font && ctx.font->Valid() && lbl && LabelEnd(lbl) != lbl)
						largeurLbl = ctx.font->MeasureWidth(lbl, LabelEnd(lbl)) + 14.f;
					const NkRect champ{rang.x, rang.y, rang.w - largeurLbl, rang.h};
					ctx.DL().AddRect(champ, p.contour, p.contourLargeur,
									 p.aRayon ? p.rayon : ctx.theme.rounding);
					++peints;
					++peintsParEtat[(uint32)s];
				}

				/// Le crochet de style de NKGui. Vrai = j'ai peint, NKGui saute son
				/// defaut ; faux = il peint comme d'habitude.
				static bool Style(NkGuiContext &ctx, const NkGuiStyleItem &it, void *user) noexcept {
					NkGuiExecution *self = (NkGuiExecution *)user;
					if (!self || !self->mCourant)
						return false;
					NkGuiInfoWidget &info = *self->mCourant;

					NkGuiCondition c;
					c.survole = it.hovered;
					c.enfonce = it.active;
					c.desactive = it.disabled;
					c.focalise = self->EstFocalise(info);
					c.focusClavier = self->mFocusClavier;
					const NkGuiEtatApp s = NkGuiResoudreEtat(info, c);
					// Le repos est le SOCLE : l'etat se pose dessus, propriete par
					// propriete. Voir `NkGuiPeintureEffective` et l'image qui l'a exige.
					const NkGuiPeinture p = NkGuiPeintureEffective(info, s);

					// Aucune apparence declaree pour AUCUN etat : le document n'a rien a
					// dire sur ce widget, NKGui garde la main. C'est le cas de neuf des
					// onze widgets de `01_panneau_reglages`, et c'est normal.
					bool aQuelqueChose = false;
					for (uint32 k = 0; k < kNkGuiEtatCompte; ++k)
						if (info.etats[k].declare) {
							aQuelqueChose = true;
							break;
						}
					if (!aQuelqueChose) {
						++self->laissesAuTheme;
						return false;
					}
					if (!p.declare && s == NkGuiEtatApp::Normal) {
						// Etats hors repos declares mais pas de repos : le repos reste
						// celui du theme, les autres sont peints. Rien n'est invente.
						++self->laissesAuTheme;
						return false;
					}

					const float32 rayon = p.aRayon ? p.rayon : ctx.theme.rounding;
					if (p.aFond)
						ctx.DL().AddRectFilled(it.rect, p.fond, rayon);
					else
						ctx.DL().AddRectFilled(it.rect, ctx.theme.button, rayon);
					if (p.aContour)
						ctx.DL().AddRect(it.rect, p.contour, p.contourLargeur, rayon);
					else
						ctx.DL().AddRect(it.rect, ctx.theme.border, 1.f, rayon);

					// Le libelle -- sans lui, re-peindre un bouton le rendrait muet.
					if (ctx.font && ctx.font->Valid() && it.label) {
						const char *fin = LabelEnd(it.label);
						const float32 tw = ctx.font->MeasureWidth(it.label, fin);
						const float32 tx = it.rect.x + (it.rect.w - tw) * 0.5f;
						const float32 by = it.rect.y + (it.rect.h - ctx.font->LineHeight()) * 0.5f
										   + ctx.font->Ascent();
						const NkColor lc = it.disabled ? ctx.theme.textDisabled : ctx.theme.text;
						ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {tx, by}, it.label, lc,
										 it.rect.w - 6.f, 0.f, fin);
					}
					++self->peints;
					++self->peintsParEtat[(uint32)s];
					return true;
				}

				/// `n1.value` -> la valeur montee du widget `n1` ; `modele.x` -> le
				/// modele ; sinon, non resolu (et l'evaluateur en fait un jeton).
				static bool Resoudre(NkStringView chemin, NkGuiValeur &out, void *user) noexcept {
					NkGuiExecution *self = (NkGuiExecution *)user;
					if (!self)
						return false;
					// 1. Le modele repond en premier quand le chemin y est ecrit tel quel
					//    (`modele.valeur`). C'est le chemin que `bind` designe.
					float32 v = 0.f;
					if (self->modele.Lire(chemin, v)) {
						out = NkGuiValeur::DeNombre(v);
						return true;
					}
					// 2. `<id>.value` / `<id>.checked` : l'identifiant d'un widget, suivi
					//    de ce qu'on lui demande. On coupe au DERNIER point : un id ne
					//    contient pas de point, mais un chemin de modele, si.
					int32 pt = -1;
					for (int32 i = (int32)chemin.Size() - 1; i >= 0; --i)
						if (chemin.Data()[i] == '.') {
							pt = i;
							break;
						}
					if (pt <= 0 || !self->etat)
						return false;
					const NkStringView id(chemin.Data(), (usize)pt);
					const NkStringView champ(chemin.Data() + pt + 1,
											 (usize)((int32)chemin.Size() - pt - 1));
					const NkGuiInfoWidget *info = self->infos.Trouver(id);
					if (!info)
						return false;
					NkGuiMonteEtat::Entree *e = self->etat->Get(
						NkStringView(info->cle.Data(), (usize)info->cle.Size()));
					if (!e)
						return false;
					if (NkGMotEgal(champ, "value")) {
						out = NkGuiValeur::DeNombre(e->f);
						return true;
					}
					if (NkGMotEgal(champ, "checked")) {
						out = NkGuiValeur::DeBooleen(e->b);
						return true;
					}
					// Un champ que je ne connais pas ne se devine pas.
					return false;
				}
		};

	} // namespace nkgui
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
