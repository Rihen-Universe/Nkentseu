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
		/// 🔴 CETTE FONCTION ÉTAIT UN SECOND ANALYSEUR (corrigé le 27/09). Elle
		///    lisait la même syntaxe que `NkGuiCouleur` du monteur, par un autre
		///    chemin — `NkParseHex` et des décalages écrits à la main contre
		///    `NkColorF::FromHex`. Les deux s'accordaient ; **rien ne les y
		///    obligeait**, et il a suffi que P1 ajoute `@nom` pour que la question
		///    devienne « où faut-il l'ajouter ? ». La réponse n'est jamais
		///    « dans les deux ».
		///
		///    Elle garde son nom parce que trois appelants l'emploient, et parce
		///    que le nom dit ce qu'elle fait. Elle ne garde pas son corps.
		inline bool NkGuiCouleurDepuisLexeme(NkStringView lex, NkColor &out) noexcept {
			return NkGuiCouleur(lex, out);
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
				// ── L'ENCRE (2026-09-26) ─────────────────────────────────────
				/// 🔴 CE QUI MANQUAIT, ET CE QUE CA COUTAIT. `text { color }` etait LU
				///    par le format et honore par le monteur sur un `Text`
				///    (`NkGuiApparenceRepos::encre`) -- et ce chemin-ci n'en savait
				///    rien. Or des qu'un widget declare UNE apparence, c'est CE chemin
				///    qui peint, y compris AU REPOS. Le bouton orange de NkAnimaEditor
				///    demandait donc une encre sombre (#10222B) et recevait celle du
				///    theme : mesure du 26/09, **0 pixel** de la couleur demandee pour
				///    2 420 pixels d'orange. *Le document ecrivait, personne ne lisait.*
				///
				/// ⚠️ ET ELLE SE PORTE PAR ETAT, comme le fond. Un bouton dont le repos
				///    est clair et le survol sombre a besoin des deux encres, sinon le
				///    libelle disparait dans l'un des deux etats.
				bool aEncre = false;
				NkColor encre{0, 0, 0, 255};
				/// Les couleurs de cet état que le lecteur n'a pas su lire —
				/// écriture fautive, ou jeton `@nom` que le thème ne connaît pas.
				/// ⚠️ Comptées, jamais tues : une couleur illisible rendait ce
				///    widget exactement comme s'il n'avait rien demandé.
				uint32 nonLues = 0;
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
						// ⚠️ MÊME CORRECTIF QUE CÔTÉ MONTEUR (27/09) : une couleur
						//    illisible — ou un jeton `@nom` inconnu du thème — se
						//    COMPTE. Sans ce compteur, l'état d'un widget dont la
						//    couleur est mal écrite se rendrait comme s'il n'avait
						//    rien déclaré, et rien ne le dirait.
						if (NkGMotEgal(t, "fill")) {
							if (col && NkGuiCouleurDepuisLexeme(col->Lexeme(), p.fond))
								p.aFond = true;
							else
								++p.nonLues;
						} else if (NkGMotEgal(t, "stroke")) {
							if (col && NkGuiCouleurDepuisLexeme(col->Lexeme(), p.contour))
								p.aContour = true;
							float32 lw = 1.f;
							if (eff.GetFloat32(NkStringView("width"), lw))
								p.contourLargeur = lw;
						} else if (NkGMotEgal(t, "text")) {
							// ⚠️ MEME PORTE QUE LE MONTEUR : `text { color }`. Le monteur
							//    l'appelle « encre » et l'applique a `theme.text` sur un
							//    `Text` ; ici elle sert le LIBELLE du widget repeint.
							if (col && NkGuiCouleurDepuisLexeme(col->Lexeme(), p.encre))
								p.aEncre = true;
							else
								++p.nonLues;
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
			// L'encre suit la MEME regle de socle : l'etat la remplace s'il en
			// declare une, sinon celle du repos reste. Un `appearance(Hover)` qui ne
			// parle que du fond ne doit pas faire disparaitre le libelle.
			//
			// ⚠️ ET CETTE LIGNE-CI N'A AUJOURD'HUI AUCUN EFFET OBSERVABLE -- c'est
			//    MESURE, pas suppose. La supprimer laisse le banc a 172/172. Raison :
			//    quand le REPOS declare une encre, le monteur l'a deja posee dans
			//    `theme.text` autour du widget (`EncreDuDocument`), et le repli de
			//    `Style()` retombe donc sur la meme couleur. Les deux chemins lisent
			//    le MEME `text { color }` du MEME document : il n'existe pas de cas
			//    ou l'un l'aurait et pas l'autre, donc **aucun temoin ne peut les
			//    separer**. *Une propriete garantie deux fois est une propriete dont
			//    l'echec est masque* -- ici on ne peut pas construire le second
			//    temoin, alors on ecrit la redondance au lieu de la croire prouvee.
			//    Elle reste parce que cette fonction doit etre COMPLETE en
			//    elle-meme : fond, contour, rayon et encre s'y heritent pareil, et un
			//    appelant futur qui n'aurait pas le monteur derriere lui trouverait
			//    l'encre a sa place. (L'encre d'un ETAT, elle, n'a que ce chemin-la,
			//    et le cas (b1.i) la mesure.)
			if (dessus.aEncre) {
				r.aEncre = true;
				r.encre = dessus.encre;
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
				// ── P25 : LES INSTRUCTIONS D'INTERFACE (27/09) ───────────────
				/// ⚠️ TROIS COMPTEURS, PARCE QUE TROIS ISSUES DIFFERENTES.
				///    `uiReconnues` : l'evaluateur a compris l'instruction.
				///    `uiServies`   : l'hote l'a faite. **La difference entre les
				///                    deux est exactement ce que l'application ne
				///                    sait pas encore faire** — et c'est la seule
				///                    facon de le voir sans lancer l'application.
				///    `uiSansHote`  : aucun hote pose. Ce n'est pas un refus de
				///                    l'hote, c'est son absence : les confondre
				///                    ferait chercher un defaut la ou il n'y a
				///                    qu'un cablage manquant.
				uint32 uiReconnues = 0;
				uint32 uiServies = 0;
				uint32 uiSansHote = 0;
		};

		// =====================================================================
		//  P25 — CE QUE L'INTERFACE **FAIT** EN REPONSE
		// =====================================================================
		//  « Un comportement sait calculer et appeler l'application ; il ne sait
		//  pas encore agir sur l'interface. » C'est le chantier O du document 19,
		//  et ce sont les instructions du §5.4 du document 2.
		//
		//  ⚠️ L'EVALUATEUR NE TOUCHE RIEN LUI-MEME, et c'est ce qui le garde
		//     mesurable. Il ne connait ni les widgets, ni l'ecran courant, ni les
		//     services : il RECONNAIT l'instruction, evalue ses arguments, et la
		//     remet a l'hote. La meme separation que `resolveur` pour la lecture.
		enum class NkGuiInstruction : uint8 {
			Montrer = 0,  ///< `show x`
			Cacher,		  ///< `hide x`
			Basculer,	  ///< `toggle x`
			Activer,	  ///< `enable x`
			Desactiver,	  ///< `disable x because "raison"`
			Focaliser,	  ///< `focus x`
			PoserPropriete, ///< `set x.prop = expr`
			Ouvrir,		  ///< `open "ecran"` / `as modal`
			Fermer,		  ///< `close` / `close "ecran"`
			Retour,		  ///< `back`
			Message,	  ///< `message titre texte buttons [...]`
			Notifier,	  ///< `toast "texte"`
			Emettre,	  ///< `emit "evenement"(args)`
			Differer,	  ///< `after ms { ... }`
			Service,	  ///< `call "service"(args)`
			Theme		  ///< `theme "Nom"`
		};

		/// Ce qu'une instruction demande. Les champs inutiles a l'instruction
		/// restent vides — leur presence ne se devine pas, elle se lit.
		struct NkGuiDemande {
				NkGuiInstruction quoi = NkGuiInstruction::Montrer;
				NkString cible;	   ///< widget, ecran, service, evenement ou theme
				NkString propriete; ///< `set x.PROP = ...`
				NkGuiValeur valeur; ///< la valeur de `set`, le texte d'un `toast`…
				NkString raison;   ///< `disable ... because` — jamais invente
				bool modal = false; ///< `open ... as modal`
				float32 delai = 0.f; ///< `after MS { ... }`
				/// Le corps d'un `after` / `message -> r { }` / `call -> r { }`,
				/// en SOURCE VERBATIM. ⚠️ Il n'est pas evalue ici : l'hote decide
				/// QUAND, et le rejouera par `ExecuterTranche`.
				NkString corps;
				NkString variable; ///< le `-> r` qui recevra la reponse
		};

		/// L'hote sert l'instruction. Faux = « je ne sais pas faire ceci » — et
		/// l'evaluateur le COMPTE au lieu de croire que c'est fait.
		using NkGuiAgirFn = bool (*)(const NkGuiDemande &d, void *user);

		/// Resolution d'un chemin pointe vers une valeur. Rendue par l'hote.
		using NkGuiResolveurFn = bool (*)(NkStringView chemin, NkGuiValeur &out, void *user);

		class NkGuiEvaluateur {
			public:
				NkGuiResolveurFn resolveur = nullptr;
				void *resolveurUser = nullptr;
				NkGuiCallbackFn rappel = nullptr;
				void *rappelUser = nullptr;
				/// P25 — l'hôte qui SERT les instructions d'interface. Absent, elles
				/// sont reconnues et comptées (`uiSansHote`), jamais devinées.
				NkGuiAgirFn agir = nullptr;
				void *agirUser = nullptr;

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
							// ⚠️ `!=` EST ENTRE DANS CETTE LISTE LE 27/09, et son absence
							//    n'etait pas benigne : `a != b` se lisait `!` puis `=`,
							//    donc une negation suivie d'une affectation dans une
							//    EXPRESSION — du charabia que l'evaluateur comptait en
							//    `refusees` sans dire lequel.
							const bool deux =
								(i + 1 < n)
								&& ((c == '>' && p[i + 1] == '=') || (c == '<' && p[i + 1] == '=')
									|| (c == '=' && p[i + 1] == '=') || (c == '!' && p[i + 1] == '=')
									|| (c == '&' && p[i + 1] == '&')
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
					// ── P25 : LES INSTRUCTIONS D'INTERFACE ───────────────────
					//  ⚠️ ELLES SONT ESSAYEES APRES LES TROIS ANCIENNES, et l'ordre
					//     n'est pas indifferent : `set` a deux formes désormais
					//     (`set var = ...` et `set "x".prop = ...`), et c'est
					//     `Affectation` qui les distingue — une seule porte pour un
					//     seul mot.
					if (InstructionInterface())
						return true;
					// Rien de connu : on COMPTE et on s'arrete, on ne devine pas.
					++rapport.refusees;
					return false;
				}

				/// Les quinze instructions du §5.4. Rend faux si le mot courant n'en
				/// est pas une — sans rien consommer.
				bool InstructionInterface() noexcept {
					if (J().k != Tk::Ident)
						return false;
					const NkStringView mot(J().t.Data(), (usize)J().t.Size());

					// ── Celles qui prennent UN widget ────────────────────────
					NkGuiInstruction q = NkGuiInstruction::Montrer;
					bool surWidget = false;
					if (NkGMotEgal(mot, "show")) {
						q = NkGuiInstruction::Montrer;
						surWidget = true;
					} else if (NkGMotEgal(mot, "hide")) {
						q = NkGuiInstruction::Cacher;
						surWidget = true;
					} else if (NkGMotEgal(mot, "toggle")) {
						q = NkGuiInstruction::Basculer;
						surWidget = true;
					} else if (NkGMotEgal(mot, "enable")) {
						q = NkGuiInstruction::Activer;
						surWidget = true;
					} else if (NkGMotEgal(mot, "focus")) {
						q = NkGuiInstruction::Focaliser;
						surWidget = true;
					}
					if (surWidget) {
						++mT;
						NkGuiDemande d;
						d.quoi = q;
						if (!LireReferenceWidget(d.cible))
							return VraiApresRefus();
						return Servir(d);
					}

					// ── `disable x because "raison"` ─────────────────────────
					//  ⚠️ LA RAISON N'EST PAS FACULTATIVE DANS L'ESPRIT DE LA
					//     FAMILLE : « un element desactive dit pourquoi » (doc 3
					//     §14quater). La grammaire la rend optionnelle ; l'absence
					//     se COMPTE donc, au lieu de passer inapercue.
					if (NkGMotEgal(mot, "disable")) {
						++mT;
						NkGuiDemande d;
						d.quoi = NkGuiInstruction::Desactiver;
						if (!LireReferenceWidget(d.cible))
							return VraiApresRefus();
						if (J().k == Tk::Ident
							&& NkGMotEgal(NkStringView(J().t.Data(), (usize)J().t.Size()),
										  "because")) {
							++mT;
							const NkGuiValeur r = Expression();
							d.raison = NkString(TexteDe(r));
						}
						return Servir(d);
					}

					// ── `open "ecran" [as modal]` ────────────────────────────
					if (NkGMotEgal(mot, "open")) {
						++mT;
						NkGuiDemande d;
						d.quoi = NkGuiInstruction::Ouvrir;
						if (J().k != Tk::Chaine)
							return VraiApresRefus();
						d.cible = J().t;
						++mT;
						if (J().k == Tk::Ident
							&& NkGMotEgal(NkStringView(J().t.Data(), (usize)J().t.Size()), "as")) {
							++mT;
							if (J().k == Tk::Ident
								&& NkGMotEgal(NkStringView(J().t.Data(), (usize)J().t.Size()),
											  "modal")) {
								d.modal = true;
								++mT;
							} else {
								return VraiApresRefus();
							}
						}
						return Servir(d);
					}

					// ── `close` / `close "ecran"` / `back` ───────────────────
					if (NkGMotEgal(mot, "close")) {
						++mT;
						NkGuiDemande d;
						d.quoi = NkGuiInstruction::Fermer;
						if (J().k == Tk::Chaine) {
							d.cible = J().t;
							++mT;
						}
						return Servir(d);
					}
					if (NkGMotEgal(mot, "back")) {
						++mT;
						NkGuiDemande d;
						d.quoi = NkGuiInstruction::Retour;
						return Servir(d);
					}

					// ── `toast expr` et `theme expr` ─────────────────────────
					if (NkGMotEgal(mot, "toast") || NkGMotEgal(mot, "theme")) {
						const bool estToast = NkGMotEgal(mot, "toast");
						++mT;
						NkGuiDemande d;
						d.quoi = estToast ? NkGuiInstruction::Notifier : NkGuiInstruction::Theme;
						d.valeur = Expression();
						d.cible = NkString(TexteDe(d.valeur));
						return Servir(d);
					}

					// ── `emit "evt"(args)` et `call "service"(args) -> r { }` ─
					if (NkGMotEgal(mot, "emit") || NkGMotEgal(mot, "call")) {
						const bool estEmit = NkGMotEgal(mot, "emit");
						++mT;
						NkGuiDemande d;
						d.quoi = estEmit ? NkGuiInstruction::Emettre : NkGuiInstruction::Service;
						if (J().k != Tk::Chaine)
							return VraiApresRefus();
						d.cible = J().t;
						++mT;
						if (!Sym(J(), "("))
							return VraiApresRefus();
						++mT;
						// Les arguments sont EVALUES — leur effet de bord compte —
						// puis le premier sert de valeur portee a l'hote.
						bool premier = true;
						while (!Sym(J(), ")") && J().k != Tk::Fin) {
							const NkGuiValeur a = Expression();
							if (premier) {
								d.valeur = a;
								premier = false;
							}
							if (Sym(J(), ","))
								++mT;
						}
						if (Sym(J(), ")"))
							++mT;
						LireSuiteEtCorps(d);
						return Servir(d);
					}

					// ── `message titre texte buttons [...] -> r { }` ─────────
					if (NkGMotEgal(mot, "message")) {
						++mT;
						NkGuiDemande d;
						d.quoi = NkGuiInstruction::Message;
						const NkGuiValeur titre = Expression();
						d.cible = NkString(TexteDe(titre));
						d.valeur = Expression();
						if (J().k == Tk::Ident
							&& NkGMotEgal(NkStringView(J().t.Data(), (usize)J().t.Size()),
										  "buttons")) {
							++mT;
							// La liste des boutons : `[ "a", "b" ]`. Elle voyage dans
							// `raison`, faute d'un champ de liste — et c'est ecrit
							// plutot que devine.
							if (Sym(J(), "["))
								d.raison = LireListeTextuelle();
						}
						LireSuiteEtCorps(d);
						return Servir(d);
					}

					// ── `after ms { ... }` ───────────────────────────────────
					if (NkGMotEgal(mot, "after")) {
						++mT;
						NkGuiDemande d;
						d.quoi = NkGuiInstruction::Differer;
						d.delai = Expression().EnNombre();
						d.corps = LireCorpsVerbatim();
						return Servir(d);
					}

					return false;
				}

				/// Un `widget_ref` : un identifiant nu, ou une chaine quand il
				/// contient un point.
				bool LireReferenceWidget(NkString &out) noexcept {
					if (J().k == Tk::Chaine || J().k == Tk::Ident) {
						out = J().t;
						++mT;
						return true;
					}
					return false;
				}

				/// ⚠️ ON COMPTE ET ON CONTINUE, plutot que de rendre faux : le mot
				///    ETAIT une instruction d'interface, c'est sa suite qui est
				///    fautive. Rendre faux la ferait compter une seconde fois en
				///    `refusees` par l'appelant, et deux compteurs pour une faute
				///    laisseraient croire a deux fautes.
				bool VraiApresRefus() noexcept {
					++rapport.refusees;
					SauterJusquAuBoutDeLInstruction();
					return true;
				}

				/// Jusqu'a la fin de la ligne logique : la tranche verbatim d'une
				/// instruction ne porte qu'elle, donc la fin des jetons suffit.
				void SauterJusquAuBoutDeLInstruction() noexcept {
					while (J().k != Tk::Fin)
						++mT;
				}

				/// `[ "a", "b" ]` rendu tel quel, separe par des barres verticales.
				NkString LireListeTextuelle() noexcept {
					NkString out;
					if (!Sym(J(), "["))
						return out;
					++mT;
					while (!Sym(J(), "]") && J().k != Tk::Fin) {
						if (J().k == Tk::Chaine || J().k == Tk::Ident) {
							if (out.Size() > 0u)
								out.Append("|");
							out.Append(J().t.CStr());
						}
						++mT;
						if (Sym(J(), ","))
							++mT;
					}
					if (Sym(J(), "]"))
						++mT;
					return out;
				}

				/// `-> r { ... }` et son `else e { ... }` eventuel.
				///
				/// ⚠️ LE CORPS N'EST PAS EVALUE ICI. `message`, `call` et `after`
				///    repondent PLUS TARD : evaluer leur corps maintenant
				///    l'executerait avant la reponse, c'est-a-dire toujours, et dans
				///    les deux branches a la fois.
				void LireSuiteEtCorps(NkGuiDemande &d) noexcept {
					if (Sym(J(), "-") && mT + 1u < (uint32)mJ.Size() && Sym(mJ[mT + 1u], ">"))
						mT += 2u;
					else if (!Sym(J(), "->"))
						return;
					else
						++mT;
					if (J().k == Tk::Ident) {
						d.variable = J().t;
						++mT;
					}
					d.corps = LireCorpsVerbatim();
				}

				/// Le texte d'un `{ ... }`, accolades comprises, sans l'evaluer.
				NkString LireCorpsVerbatim() noexcept {
					NkString out;
					if (!Sym(J(), "{"))
						return out;
					int32 prof = 0;
					for (;;) {
						if (J().k == Tk::Fin)
							break;
						if (Sym(J(), "{"))
							++prof;
						else if (Sym(J(), "}"))
							--prof;
						if (out.Size() > 0u)
							out.Append(" ");
						out.Append(J().t.CStr());
						++mT;
						if (prof == 0)
							break;
					}
					return out;
				}

				/// Remet la demande a l'hote, et COMPTE les trois issues.
				bool Servir(const NkGuiDemande &d) noexcept {
					++rapport.uiReconnues;
					++rapport.instructions;
					if (!agir) {
						++rapport.uiSansHote;
						return true;
					}
					if (agir(d, agirUser))
						++rapport.uiServies;
					return true;
				}

				bool Affectation() noexcept {
					++mT; // `set`
					// ⚠️ `set` A DEUX FORMES, ET UNE SEULE PORTE LES DISTINGUE.
					//    `set r = ...` pose une VARIABLE locale ; `set "titre".text
					//    = ...` regle une PROPRIETE d'un autre widget (P25). La
					//    difference se lit au point qui suit la cible — et elle se
					//    lit ICI, parce que deux portes pour un meme mot finiraient
					//    par ne plus s'accorder sur laquelle prend la main.
					if (J().k != Tk::Ident && J().k != Tk::Chaine) {
						++rapport.refusees;
						return false;
					}
					const NkString cible = J().t;
					const bool citee = (J().k == Tk::Chaine);
					++mT;

					if (Sym(J(), ".")) {
						++mT;
						if (J().k != Tk::Ident) {
							++rapport.refusees;
							return false;
						}
						NkGuiDemande d;
						d.quoi = NkGuiInstruction::PoserPropriete;
						d.cible = cible;
						d.propriete = J().t;
						++mT;
						if (!Sym(J(), "=")) {
							++rapport.refusees;
							return false;
						}
						++mT;
						d.valeur = Expression();
						return Servir(d);
					}

					// ⚠️ UNE CIBLE CITEE SANS POINT N'EST PAS UNE VARIABLE. Les noms
					//    de variables sont des identifiants nus ; `set "x" = 1`
					//    n'est ni l'une ni l'autre forme, et se compte.
					if (citee) {
						++rapport.refusees;
						return false;
					}
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
					NkGuiValeur a = NonLogique();
					while (Sym(J(), "&&")) {
						++mT;
						const NkGuiValeur b = NonLogique();
						a = NkGuiValeur::DeBooleen(a.EnBooleen() && b.EnBooleen());
					}
					return a;
				}

				/// `not expr` et `! expr` — la meme chose, deux graphies.
				///
				/// ⚠️ IL EST AU-DESSUS DE LA COMPARAISON, PAS AU-DESSOUS. La grammaire
				///    l'ecrit ainsi (`and_expr := not_expr ("&&" not_expr)*`), et ce
				///    n'est pas un detail : `not a == b` doit nier LA COMPARAISON, pas
				///    `a`. Le mettre dans `Primaire` aurait donne `(not a) == b`, qui
				///    est une autre question et rend souvent l'inverse.
				NkGuiValeur NonLogique() noexcept {
					const bool mot = (J().k == Tk::Ident)
									 && NkGMotEgal(NkStringView(J().t.Data(), (usize)J().t.Size()),
												   "not");
					if (mot || Sym(J(), "!")) {
						++mT;
						return NkGuiValeur::DeBooleen(!NonLogique().EnBooleen());
					}
					return Egalite();
				}

				NkGuiValeur Egalite() noexcept {
					NkGuiValeur a = Comparaison();
					for (;;) {
						const bool egal = Sym(J(), "==");
						const bool different = Sym(J(), "!=");
						if (!egal && !different)
							return a;
						++mT;
						const NkGuiValeur b = Comparaison();
						a = ComparerEgalite(a, b);
						if (different)
							a = NkGuiValeur::DeBooleen(!a.EnBooleen());
					}
				}

				/// ⚠️ UNE SEULE REGLE D'EGALITE, PARTAGEE PAR `==` ET `!=`. Deux
				///    ecritures de la meme comparaison finiraient par ne plus etre
				///    d'accord — et `a != b` cesserait d'etre l'exact contraire de
				///    `a == b`, ce que personne ne penserait a verifier.
				static NkGuiValeur ComparerEgalite(const NkGuiValeur &a,
												   const NkGuiValeur &b) noexcept {
					// Deux JETONS se comparent par leur texte : `Enum.Haut == Enum.Haut`
					// est vrai, et le reduire a un nombre l'aurait rendu vrai pour
					// n'importe quelle paire de jetons.
					if (a.type == NkGuiValeur::Type::Jeton && b.type == NkGuiValeur::Type::Jeton)
						return NkGuiValeur::DeBooleen(a.jeton.Compare(b.jeton) == 0);
					return NkGuiValeur::DeBooleen(a.EnNombre() == b.EnNombre());
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
						// ⚠️ UNE CHAINE SUIVIE D'UN POINT EST UN WIDGET, PAS UN JETON.
						//    C'est la graphie que la grammaire impose aux identifiants
						//    qui CONTIENNENT un point — ceux que les composants
						//    prefixent : `"transport.lecture".enabled`. Sans ce cas, le
						//    resolveur couperait au dernier point et chercherait un
						//    widget nomme `"transport`.
						if (Sym(J(), ".") && mT + 1u < (uint32)mJ.Size()
							&& mJ[mT + 1u].k == Tk::Ident) {
							const NkString champ = mJ[mT + 1u].t;
							mT += 2u;
							NkString chemin = s;
							chemin.Append(".");
							chemin.Append(champ.CStr());
							NkGuiValeur v;
							if (resolveur
								&& resolveur(NkStringView(chemin.Data(), (usize)chemin.Size()), v,
											 resolveurUser))
								return v;
							++rapport.refusees;
							return NkGuiValeur::DeNombre(0.f);
						}
						return NkGuiValeur::DeJeton(NkStringView(s.Data(), (usize)s.Size()));
					}
					if (J().k == Tk::Ident) {
						const NkString nom = J().t;
						++mT;
						// ── LES QUATRE FONCTIONS DE LA GRAMMAIRE ─────────────
						//  `empty` `length` `matches` `contains` — §3.4.
						//
						//  ⚠️ ELLES SONT ICI ET NON DANS L'HOTE, parce qu'elles ne
						//     touchent a RIEN : elles lisent leurs arguments et
						//     rendent une valeur. Les confier a l'application
						//     donnerait autant de definitions de « vide » que
						//     d'applications.
						if (Sym(J(), "(") && EstFonction(NkStringView(nom.Data(),
																	  (usize)nom.Size()))) {
							++mT;
							NkVector<NkGuiValeur> args;
							while (!Sym(J(), ")") && J().k != Tk::Fin) {
								args.PushBack(Expression());
								if (Sym(J(), ","))
									++mT;
							}
							if (Sym(J(), ")"))
								++mT;
							return AppelerFonction(NkStringView(nom.Data(), (usize)nom.Size()),
												   args);
						}
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

				/// Les quatre noms que la grammaire réserve (§3.4 `func_call`).
				static bool EstFonction(NkStringView n) noexcept {
					return NkGMotEgal(n, "empty") || NkGMotEgal(n, "length")
						   || NkGMotEgal(n, "matches") || NkGMotEgal(n, "contains");
				}

				/// ⚠️ LE TEXTE D'UNE VALEUR, ET IL N'Y EN A QU'UN. Un jeton porte son
				///    texte ; un nombre n'en a pas — le convertir ici donnerait une
				///    graphie (« 1 » ou « 1.000000 » ?) que personne n'a choisie, et
				///    `empty(x)` répondrait sur cette graphie plutôt que sur la valeur.
				///    Un nombre n'est donc jamais « vide », et sa longueur est 0.
				static NkStringView TexteDe(const NkGuiValeur &v) noexcept {
					if (v.type == NkGuiValeur::Type::Jeton)
						return NkStringView(v.jeton.Data(), (usize)v.jeton.Size());
					return NkStringView("", 0u);
				}

				/// `contains(a, b)` et `matches(a, b)` sur des textes.
				///
				/// ⚠️ `matches` EST UNE ÉGALITÉ EXACTE, PAS UNE EXPRESSION RÉGULIÈRE,
				///    et c'est écrit plutôt que supposé. La grammaire ne dit pas
				///    laquelle des deux ; implémenter des expressions régulières
				///    ferait entrer un moteur entier par la petite porte, et un
				///    document écrit pour l'égalité se mettrait à correspondre à
				///    autre chose le jour où on l'ajouterait. *Le plus petit sens
				///    défendable, écrit.*
				static bool Contient(NkStringView a, NkStringView b) noexcept {
					if (b.Size() == 0u)
						return true;
					if (b.Size() > a.Size())
						return false;
					for (usize i = 0; i + b.Size() <= a.Size(); ++i) {
						usize k = 0;
						while (k < b.Size() && a.Data()[i + k] == b.Data()[k])
							++k;
						if (k == b.Size())
							return true;
					}
					return false;
				}

				NkGuiValeur AppelerFonction(NkStringView nom,
											const NkVector<NkGuiValeur> &args) noexcept {
					const uint32 n = (uint32)args.Size();
					if (NkGMotEgal(nom, "empty")) {
						if (n != 1u) {
							++rapport.refusees;
							return NkGuiValeur::DeBooleen(true);
						}
						return NkGuiValeur::DeBooleen(TexteDe(args[0]).Size() == 0u);
					}
					if (NkGMotEgal(nom, "length")) {
						if (n != 1u) {
							++rapport.refusees;
							return NkGuiValeur::DeNombre(0.f);
						}
						return NkGuiValeur::DeNombre((float32)TexteDe(args[0]).Size());
					}
					// `matches` et `contains` prennent DEUX arguments : en donner un
					// autre nombre est une faute d'écriture, et elle se compte.
					if (n != 2u) {
						++rapport.refusees;
						return NkGuiValeur::DeBooleen(false);
					}
					const NkStringView a = TexteDe(args[0]), b = TexteDe(args[1]);
					if (NkGMotEgal(nom, "contains"))
						return NkGuiValeur::DeBooleen(Contient(a, b));
					// `matches` : égalité exacte.
					if (a.Size() != b.Size())
						return NkGuiValeur::DeBooleen(false);
					for (usize i = 0; i < a.Size(); ++i)
						if (a.Data()[i] != b.Data()[i])
							return NkGuiValeur::DeBooleen(false);
					return NkGuiValeur::DeBooleen(true);
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
					eval.agir = &NkGuiExecution::Agir;
					eval.agirUser = this;
				}

				/// Ce que l'hôte sert des instructions d'interface (P25).
				///
				/// ⚠️ IL NE SERT QUE CE QUI TOUCHE AUX WIDGETS, et c'est une limite
				///    ÉCRITE, pas un oubli. `open`, `close`, `back`, `message`,
				///    `toast`, `emit`, `after`, `call` et `theme` parlent de
				///    NAVIGATION, de DIALOGUES et de SERVICES — des choses que cette
				///    couche ne possède pas. Elles sont reconnues, comptées
				///    (`uiReconnues` sans `uiServies`), et **la différence entre les
				///    deux chiffres est exactement ce que l'application ne sait pas
				///    encore faire**. C'est visible sans lancer l'application.
				struct ServicesHote {
						virtual ~ServicesHote() = default;
						/// Rendre faux = « je ne sais pas faire ceci ». L'évaluateur
						/// le compte ; il ne le devine pas.
						virtual bool Servir(const NkGuiDemande &) noexcept {
							return false;
						}
				};
				ServicesHote *services = nullptr;

				static bool Agir(const NkGuiDemande &d, void *user) noexcept {
					NkGuiExecution *self = (NkGuiExecution *)user;
					if (!self || !self->etat)
						return false;

					// ── CE QUI TOUCHE UN WIDGET ─────────────────────────────
					switch (d.quoi) {
						case NkGuiInstruction::Montrer:
						case NkGuiInstruction::Cacher:
						case NkGuiInstruction::Basculer:
						case NkGuiInstruction::Activer:
						case NkGuiInstruction::Desactiver:
						case NkGuiInstruction::Focaliser:
						case NkGuiInstruction::PoserPropriete:
							break;
						default:
							// Navigation, dialogue, service : à l'hôte, s'il y en a un.
							return self->services ? self->services->Servir(d) : false;
					}

					// ⚠️ LE WIDGET SE TROUVE PAR SON IDENTIFIANT, ET SON ÉTAT PAR SA
					//    CLÉ. Les deux diffèrent dès qu'un `bind` existe : écrire
					//    dans l'entrée de l'identifiant laisserait le widget lire
					//    celle du `bind`, et l'instruction n'aurait aucun effet
					//    VISIBLE tout en paraissant réussir.
					const NkGuiInfoWidget *info =
						self->infos.Trouver(NkStringView(d.cible.Data(), (usize)d.cible.Size()));
					if (!info)
						return false; // widget inconnu : compté par l'évaluateur
					NkGuiMonteEtat::Entree *e = self->etat->Get(
						NkStringView(info->cle.Data(), (usize)info->cle.Size()));
					if (!e)
						return false;

					switch (d.quoi) {
						case NkGuiInstruction::Montrer:
							e->visible = true;
							e->visibiliteDite = true;
							return true;
						case NkGuiInstruction::Cacher:
							e->visible = false;
							e->visibiliteDite = true;
							return true;
						case NkGuiInstruction::Basculer:
							e->visible = !e->visible;
							e->visibiliteDite = true;
							return true;
						case NkGuiInstruction::Activer:
							e->actif = true;
							e->activiteDite = true;
							e->raison[0] = '\0';
							return true;
						case NkGuiInstruction::Desactiver: {
							e->actif = false;
							e->activiteDite = true;
							// La raison VOYAGE jusqu'à l'infobulle. Sans elle, un
							// élément grisé ne dit pas pourquoi — ce que la famille
							// interdit.
							const char *r = d.raison.CStr();
							uint32 k = 0;
							for (; r && r[k] && k + 1u < sizeof(e->raison); ++k)
								e->raison[k] = r[k];
							e->raison[k] = '\0';
							return true;
						}
						case NkGuiInstruction::Focaliser:
							self->mFocus = info->id;
							// ⚠️ LE FOCUS PAR INSTRUCTION N'EST PAS UN FOCUS CLAVIER.
							//    `FocusVisible` ne doit s'allumer qu'au clavier ;
							//    `focus x` vient d'un comportement, donc il pose le
							//    focus sans prétendre à l'anneau.
							self->mFocusClavier = false;
							return true;
						case NkGuiInstruction::PoserPropriete:
							return PoserProprieteWidget(*e, d);
						default:
							return false;
					}
				}

				/// `set x.prop = expr` — les propriétés qu'un état peut porter.
				static bool PoserProprieteWidget(NkGuiMonteEtat::Entree &e,
												 const NkGuiDemande &d) noexcept {
					const NkStringView p(d.propriete.Data(), (usize)d.propriete.Size());
					if (NkGMotEgal(p, "text")) {
						const NkStringView t = (d.valeur.type == NkGuiValeur::Type::Jeton)
												   ? NkStringView(d.valeur.jeton.Data(),
																  (usize)d.valeur.jeton.Size())
												   : NkStringView("", 0u);
						uint32 k = 0;
						for (; k < (uint32)t.Size() && k + 1u < sizeof(e.texte); ++k)
							e.texte[k] = t.Data()[k];
						e.texte[k] = '\0';
						e.initialise = true;
						return true;
					}
					if (NkGMotEgal(p, "value")) {
						e.f = d.valeur.EnNombre();
						e.initialise = true;
						return true;
					}
					if (NkGMotEgal(p, "checked")) {
						e.b = d.valeur.EnBooleen();
						e.initialise = true;
						return true;
					}
					if (NkGMotEgal(p, "visible")) {
						e.visible = d.valeur.EnBooleen();
						e.visibiliteDite = true;
						return true;
					}
					if (NkGMotEgal(p, "enabled")) {
						e.actif = d.valeur.EnBooleen();
						e.activiteDite = true;
						return true;
					}
					// ⚠️ UNE PROPRIÉTÉ QU'ON NE SAIT PAS POSER N'EST PAS POSÉE. La
					//    ranger « quelque part » donnerait un document qui croit
					//    avoir réglé ce qu'il n'a pas réglé.
					return false;
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
					// ⚠️ DEUX SOURCES, ET L'ORDRE COMPTE : le DOCUMENT pose l'etat de
					//    depart (`enabled = false`), un COMPORTEMENT le change ensuite
					//    (`enable` / `disable ... because`). Le second l'emporte, sinon
					//    l'instruction n'aurait aucun effet visible — et c'est la seule
					//    lecture, il n'y en a pas une seconde ailleurs.
					const bool actifCourant =
						(e && e->activiteDite) ? e->actif : (mCourant ? mCourant->actif : true);
					if (!actifCourant) {
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
						// ── QUELLE ENCRE, ET L'ARBITRAGE QUE CA DEMANDE ──────────
						//  L'encre du document passe devant celle du theme. Mais un
						//  widget DESACTIVE pose une question que le fond ne posait pas :
						//  le document a-t-il dit a quoi ressemble son etat desactive ?
						//
						//  ⚠️ S'IL NE L'A PAS DIT, LE THEME GRISE. Herite de `Normal`,
						//     l'encre rendrait un libelle pleine force sur un bouton
						//     grise -- il se lirait comme actif, et c'est exactement le
						//     genre de rendu que *personne n'a dessine*.
						//     S'il l'a dit (`appearance(Disabled)` existe), son choix
						//     vaut, y compris l'encre heritee du repos : la, quelqu'un a
						//     regarde.
						const bool disabledDeclare =
							info.etats[(uint32)NkGuiEtatApp::Disabled].declare;
						const NkColor lc =
							(it.disabled && !disabledDeclare)
								? ctx.theme.textDisabled
								: (p.aEncre ? p.encre
											: (it.disabled ? ctx.theme.textDisabled
														   : ctx.theme.text));
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
					// ── LES SIX CHAMPS DE PLUS (27/09, §3.4 `Field`) ─────────
					//  ⚠️ ILS LISENT L'ETAT DU MONTAGE, PAS LE DOCUMENT. Un
					//     `behavior` demande « ce champ est-il vide MAINTENANT ? »,
					//     pas « qu'est-ce que le fichier disait au depart ? ». Lire
					//     le document rendrait la condition toujours identique, et
					//     `if empty("nom".text)` serait vrai a jamais.
					if (NkGMotEgal(champ, "text")) {
						out = NkGuiValeur::DeJeton(NkStringView(e->texte));
						return true;
					}
					if (NkGMotEgal(champ, "selected")) {
						// Un choix dans une liste : l'indice vit dans `f`, la case
						// dans `b`. Les deux disent « celui-ci est choisi ».
						out = NkGuiValeur::DeBooleen(e->b || e->f > 0.5f);
						return true;
					}
					if (NkGMotEgal(champ, "visible")) {
						out = NkGuiValeur::DeBooleen(e->visible);
						return true;
					}
					if (NkGMotEgal(champ, "enabled")) {
						out = NkGuiValeur::DeBooleen(e->actif);
						return true;
					}
					if (NkGMotEgal(champ, "focused")) {
						// ⚠️ `Resoudre` EST STATIQUE (c'est un rappel), donc le focus
						//    se lit sur `self`, qui le porte. Passer par `EstFocalise`
						//    demanderait une instance ; la comparaison est la meme.
						out = NkGuiValeur::DeBooleen(self->mFocus.Size() > 0u
													 && self->mFocus.Compare(info->id) == 0);
						return true;
					}
					if (NkGMotEgal(champ, "count")) {
						// ⚠️ `count` EST LE NOMBRE D'ELEMENTS D'UNE LISTE, et le
						//    magasin n'en tient pas. Le rendre a 0 ferait passer une
						//    liste pleine pour vide ; on REFUSE, donc l'expression
						//    se compte en `refusees` et le document sait que sa
						//    question n'a pas ete servie.
						return false;
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
