#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiMonteur.h
// @Brief   LA PORTE MANQUANTE du format `.nkgui` : monter un document lu en
//          widgets NKGui reels, qui se placent et qui dessinent.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUI MANQUAIT, ET CE QUE CE FICHIER EST
// =============================================================================
//  Le format `.nkgui` etait specifie (documents 2, 7, 9), exemplifie (21 fichiers
//  ecrits a la main), lu (`NkGuiArchive`, 2030 lignes, purement syntaxique),
//  reemis a l'octet et valide par role -- et **jamais MONTE**. Mesure du
//  2026-09-14, par ouverture des fichiers et non par comptage de motifs : les
//  seuls consommateurs de `NkGuiArchive.h` etaient `NkGuiRoundTrip.h` (qui
//  mesure l'aller-retour) et `NkGuiValidate.h` (qui pose des diagnostics).
//  **Aucun fichier du depot n'incluait a la fois `NkGuiArchive.h` et
//  `NkGuiWidgets.h`.** Un format que rien n'instancie decrit des interfaces que
//  personne ne voit.
//
//  Ce fichier est cette porte, et rien d'autre : il lit une `NkArchive` produite
//  par `NkGuiArchive::Read` et appelle les widgets que NKGui a DEJA. Il n'ecrit
//  pas un widget de plus -- les dix-huit roles du format avaient tous leur
//  fonction dans `NkGuiWidgets.h` avant qu'une ligne d'ici ne soit ecrite.
//
// =============================================================================
//  OU IL VIT, ET POURQUOI `NKGui.jenga` N'A PAS BOUGE
// =============================================================================
//  `NKGui` ne depend pas de `NKSerialization`. Lui ajouter cette dependance
//  tirerait NKReflection et NKFileSystem chez TOUS ses consommateurs -- la faute
//  exacte que `NKEditorKit` a corrigee le 2026-09-01 en RETIRANT NKCanvas de son
//  jenga. Son commentaire donne la parade, et c'est celle-ci : « l'application
//  qui veut NkEditorCanvasRenderer declare NKCanvas chez elle ».
//
//  Ce fichier est donc un EN-TETE SEUL, non compile (`NKGui.jenga` fait
//  `files(["src/**.cpp"])`, un `.h` n'y ajoute rien), et c'est l'application qui
//  l'inclut qui declare `NKSerialization`. Zero dependance ajoutee a NKGui, et
//  aucun cycle : NKSerialization ne depend pas de NKGui.
//
// =============================================================================
//  CE QU'IL MONTE, ET CE QU'IL NE MONTE PAS -- LA LISTE EST ICI, PAS AILLEURS
// =============================================================================
//  MONTE  :  l'ETAT AU REPOS du document. Les conteneurs (Window, Panel, VBox,
//            HBox, Group), leur `gap`, leur `align`/`justify`, les `Spacer`, et
//            les quatorze roles qui dessinent.
//
//  NE MONTE PAS, et ce n'est pas un oubli :
//   - le COMPORTEMENT qui s'execute (`set r = n1.value * 100`, `if`) : la section
//     `behavior` est une TRANCHE DE SOURCE VERBATIM cote lecteur, son sens n'est
//     modelise nulle part. Elle est COMPTEE, pas interpretee ;
//   - les EVENEMENTS et les ANIMATIONS (`duration`, `easing`) : ils demandent un
//     temps et une boucle, ce banc n'a ni l'un ni l'autre ;
//   - les QUATRE ETATS D'APPARENCE autres que le repos (`Hover`, `Pressed`,
//     `Focus`/`FocusVisible`, `Disabled`). Ils sont LUS et COMPTES
//     (`etatsNonAppliques`) mais jamais peints : **sans interaction, aucun d'eux
//     ne peut etre atteint**, et en peindre un serait montrer un rendu que le
//     document n'a pas demande pour cet instant ;
//   - les LIAISONS `bind` : le chemin est lu et conserve comme CLE D'ETAT, mais
//     il ne suit aucune donnee vivante -- il n'y a pas de modele a lier.
//
//  ⚠️ LE `Panel` N'OUVRE PAS `BeginPanel`, ET C'EST DELIBERE. `BeginPanel` pose
//     un clip et sa doc dit « niveau unique pour l'instant ; l'imbrication
//     viendra avec une pile ». Un document en imbrique (`Panel` > `VBox` > `HBox`)
//     et un clip couperait la mesure au lieu de la rendre. Le monteur peint donc
//     le fond (`PanelBackground`, la meme primitive) et laisse le contenu dans le
//     flux. Le jour ou `BeginPanel` empile, cette ligne-ci change, et elle seule.
//
//  ⚠️ `items = [a, b]` ET `values = [1, 2, 3]` SONT DES JETONS NUS. Le lecteur le
//     dit lui-meme : « un outil qui veut iterer dessus doit encore l'analyser
//     lui-meme ». Le monteur le fait donc ici, du cote du CONSOMMATEUR --
//     `LireListeNombres` / `CompterElements`. Le format n'est pas touche.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkentseu {
	namespace nkgui {

		// =====================================================================
		//  LES ROLES -- le vocabulaire du document 7, celui que le corpus emploie
		// =====================================================================
		enum class NkGuiRole : uint8 {
			Inconnu = 0,
			Window,
			Panel,
			VBox,
			HBox,
			Group,
			Text,
			Button,
			RepeatButton,
			Checkbox,
			Slider,
			TextField,
			Dropdown,
			Progress,
			Separator,
			Spacer,
			Image,
			Chart,
			Callback
		};

		/// Comparaison de noms sans <cstring> (le depot est zero-STL).
		inline bool NkGMotEgal(NkStringView v, const char *lit) noexcept {
			uint32 i = 0;
			for (; i < (uint32)v.Size(); ++i) {
				if (lit[i] == '\0' || v.Data()[i] != lit[i])
					return false;
			}
			return lit[i] == '\0';
		}

		inline NkGuiRole NkGuiRoleDepuisNom(NkStringView n) noexcept {
			if (NkGMotEgal(n, "Window")) return NkGuiRole::Window;
			if (NkGMotEgal(n, "Panel")) return NkGuiRole::Panel;
			if (NkGMotEgal(n, "VBox")) return NkGuiRole::VBox;
			if (NkGMotEgal(n, "HBox")) return NkGuiRole::HBox;
			if (NkGMotEgal(n, "Group")) return NkGuiRole::Group;
			if (NkGMotEgal(n, "Text")) return NkGuiRole::Text;
			if (NkGMotEgal(n, "Button")) return NkGuiRole::Button;
			if (NkGMotEgal(n, "RepeatButton")) return NkGuiRole::RepeatButton;
			if (NkGMotEgal(n, "Checkbox")) return NkGuiRole::Checkbox;
			if (NkGMotEgal(n, "Slider")) return NkGuiRole::Slider;
			if (NkGMotEgal(n, "TextField")) return NkGuiRole::TextField;
			if (NkGMotEgal(n, "Dropdown")) return NkGuiRole::Dropdown;
			if (NkGMotEgal(n, "Progress")) return NkGuiRole::Progress;
			if (NkGMotEgal(n, "Separator")) return NkGuiRole::Separator;
			if (NkGMotEgal(n, "Spacer")) return NkGuiRole::Spacer;
			if (NkGMotEgal(n, "Image")) return NkGuiRole::Image;
			if (NkGMotEgal(n, "Chart")) return NkGuiRole::Chart;
			if (NkGMotEgal(n, "Callback")) return NkGuiRole::Callback;
			return NkGuiRole::Inconnu;
		}

		/// Vrai pour les blocs du vocabulaire d'APPARENCE (document 9 §3), qui ne
		/// sont pas des widgets : les compter comme tels fausserait tout releve.
		inline bool NkGEstApparence(NkStringView n) noexcept {
			return NkGMotEgal(n, "appearance") || NkGMotEgal(n, "fill") || NkGMotEgal(n, "stroke")
				   || NkGMotEgal(n, "shadow") || NkGMotEgal(n, "font") || NkGMotEgal(n, "text")
				   || NkGMotEgal(n, "layout");
		}

		// =====================================================================
		//  CE QUE LE MONTAGE A PRODUIT -- de quoi mesurer sans regarder l'ecran
		// =====================================================================
		/// Un widget monte, avec le rectangle qu'il a REELLEMENT pris.
		struct NkGuiMonteItem {
				NkString id;		  ///< l'identifiant du fichier (`"colonne"`)
				NkString role;		  ///< le mot qui ouvre le bloc (`"VBox"`)
				NkRect rect;		  ///< ce que le layout lui a donne
				uint32 profondeur = 0;
				bool conteneur = false;
				/// L'axe du conteneur QUI LE PORTE. Deux widgets d'une `HBox`
				/// partagent leur `y` : sans cette information, tout releve
				/// d'empilement vertical les compte comme un chevauchement.
				bool axeHorizontal = false;
		};

		/// Le releve d'un montage. Tout y est compte et nomme : un banc qui n'a
		/// monte aucun widget doit annoncer autre chose qu'un banc qui les a tous
		/// montes.
		struct NkGuiMonteRapport {
				uint32 sections = 0;		   ///< sections de premier niveau lues
				uint32 widgets = 0;			   ///< blocs de role widget rencontres
				uint32 montes = 0;			   ///< ceux qui ont appele une fonction NKGui
				uint32 behaviors = 0;		   ///< blocs de section `behavior`
				uint32 animations = 0;		   ///< blocs de section `animation`
				uint32 rolesInconnus = 0;	   ///< hors vocabulaire du document 7
				uint32 apparencesLues = 0;	   ///< blocs `appearance` rencontres
				uint32 etatsNonAppliques = 0;  ///< `appearance(Hover)` et consorts
				NkVector<NkGuiMonteItem> items;
				NkVector<NkString> nomsSections;
		};

		// =====================================================================
		//  L'ETAT -- il vit HORS du document, parce que le document DECRIT
		// =====================================================================
		/**
		 * @brief Les valeurs que les widgets editent (case cochee, valeur de
		 *        curseur, tampon de saisie), rangees par cle stable.
		 *
		 * ⚠️ DEUX PASSES, ET CE N'EST PAS UN CONFORT. `Checkbox` prend un `bool &`,
		 *    `SliderFloat` un `float32 &`. Si le magasin grandissait PENDANT le
		 *    montage, `NkVector` se reallouerait et ces references pointeraient
		 *    dans de la memoire liberee -- un defaut qui ne se voit pas tout de
		 *    suite et qui se voit tres mal ensuite. `Preparer()` recense donc
		 *    toutes les cles AVANT que la moindre reference ne soit prise, et le
		 *    montage n'ajoute plus rien. Pas de plafond, pas de capacite devinee.
		 */
		class NkGuiMonteEtat {
			public:
				struct Entree {
						NkString cle;
						bool b = false;
						float32 f = 0.f;
						char texte[256] = {0};
						/// Vrai une fois la valeur initiale posee depuis le document.
						/// Sans lui, chaque trame ecraserait ce que l'utilisateur a
						/// change -- un champ que le document reinitialise sans cesse
						/// n'est pas editable.
						bool initialise = false;
				};

				/// Recense une cle (phase 1). Sans effet si elle existe deja.
				void Declarer(NkStringView cle) noexcept {
					if (Trouver(cle) >= 0)
						return;
					Entree e;
					e.cle = NkString(cle);
					mE.PushBack(e);
				}

				/// Rend l'entree (phase 2). Jamais nulle APRES Declarer ; nulle sinon,
				/// et l'appelant doit le traiter plutot que de le supposer.
				Entree *Get(NkStringView cle) noexcept {
					const int32 i = Trouver(cle);
					return i < 0 ? nullptr : &mE[(uint32)i];
				}

				uint32 Taille() const noexcept {
					return (uint32)mE.Size();
				}

			private:
				int32 Trouver(NkStringView cle) const noexcept {
					for (uint32 i = 0; i < (uint32)mE.Size(); ++i) {
						if (mE[i].cle.Compare(NkString(cle)) == 0)
							return (int32)i;
					}
					return -1;
				}
				NkVector<Entree> mE;
		};

		// =====================================================================
		//  LIRE LES VALEURS -- le lecteur type ce qui tient en un jeton, pas plus
		// =====================================================================
		/// Le `$body` d'un bloc, ou nullptr. (Meme forme que `NkGCorps` de la
		/// validation : la structure de l'archive est la, elle ne se redecouvre pas.)
		inline const NkArchiveNode *NkGMonteCorps(const NkArchive &bloc) noexcept {
			const NkArchiveNode *b = bloc.FindNode(NkStringView(NkGuiArchive::KeyBody()));
			return (b && b->IsArray()) ? b : nullptr;
		}

		inline float32 NkGNombre(const NkArchive &b, const char *cle, float32 defaut) noexcept {
			float32 v = defaut;
			if (b.GetFloat32(NkStringView(cle), v))
				return v;
			return defaut;
		}

		inline bool NkGBooleen(const NkArchive &b, const char *cle, bool defaut) noexcept {
			bool v = defaut;
			if (b.GetBool(NkStringView(cle), v))
				return v;
			return defaut;
		}

		/// Le texte d'une propriete. Une CHAINE rend son contenu ; un JETON NU rend
		/// son lexeme (`Start`, `ui.filtre`) -- c'est ce que le consommateur veut
		/// dans les deux cas.
		inline NkString NkGTexte(const NkArchive &b, const char *cle, const char *defaut) noexcept {
			NkString out;
			if (b.GetString(NkStringView(cle), out))
				return out;
			return NkString(defaut);
		}

		inline bool NkGA(const NkArchive &b, const char *cle) noexcept {
			return b.FindNode(NkStringView(cle)) != nullptr;
		}

		/// Les nombres presents dans un lexeme, dans l'ordre d'ecriture. Les
		/// delimiteurs ne l'interessent pas : `[1, 2, 3]` et `(120, 80)` se lisent
		/// par la meme porte. Rend le nombre d'elements ECRITS (qui peut depasser
		/// `max` -- l'appelant sait alors qu'il en a perdu).
		inline uint32 NkGNombresDansLexeme(NkStringView lex, float32 *out, uint32 max) noexcept {
			const char *p = lex.Data();
			const uint32 len = (uint32)lex.Size();
			uint32 compte = 0, i = 0;
			while (i < len) {
				const char c = p[i];
				const bool debut = (c >= '0' && c <= '9')
								   || (c == '-' && i + 1 < len && p[i + 1] >= '0' && p[i + 1] <= '9');
				if (!debut) {
					++i;
					continue;
				}
				float32 signe = 1.f;
				if (c == '-') {
					signe = -1.f;
					++i;
				}
				float32 ent = 0.f;
				while (i < len && p[i] >= '0' && p[i] <= '9') {
					ent = ent * 10.f + (float32)(p[i] - '0');
					++i;
				}
				if (i < len && p[i] == '.') {
					++i;
					float32 d = 0.1f;
					while (i < len && p[i] >= '0' && p[i] <= '9') {
						ent += (float32)(p[i] - '0') * d;
						d *= 0.1f;
						++i;
					}
				}
				if (compte < max)
					out[compte] = signe * ent;
				++compte;
			}
			return compte;
		}

		/// Les nombres d'une liste ecrite en JETON NU (`values = [1, 2, 3, 5, 8]`).
		/// Le lecteur ne construit pas de tableau -- il le dit -- donc le
		/// consommateur analyse le lexeme.
		inline uint32 NkGListeNombres(const NkArchive &b, const char *cle, float32 *out,
									  uint32 max) noexcept {
			const NkArchiveNode *n = b.FindNode(NkStringView(cle));
			if (!n)
				return 0u;
			return NkGNombresDansLexeme(n->Lexeme(), out, max);
		}

		/// Le nombre d'elements d'une liste en jeton nu (`items = ["un","deux"]`).
		/// Compte les VIRGULES DE PREMIER NIVEAU plus une, crochets vides exceptes.
		inline uint32 NkGListeCompte(const NkArchive &b, const char *cle) noexcept {
			const NkArchiveNode *n = b.FindNode(NkStringView(cle));
			if (!n)
				return 0u;
			const NkString lex(n->Lexeme());
			const char *p = lex.Data();
			const uint32 len = (uint32)lex.Size();
			uint32 profondeur = 0, virgules = 0, contenu = 0;
			bool chaine = false;
			for (uint32 i = 0; i < len; ++i) {
				const char c = p[i];
				if (chaine) {
					if (c == '\\') {
						++i;
						continue;
					}
					if (c == '"')
						chaine = false;
					continue;
				}
				if (c == '"') {
					chaine = true;
					++contenu;
					continue;
				}
				if (c == '[' || c == '(' || c == '{') {
					++profondeur;
					continue;
				}
				if (c == ']' || c == ')' || c == '}') {
					if (profondeur > 0)
						--profondeur;
					continue;
				}
				if (c == ',' && profondeur == 1) {
					++virgules;
					continue;
				}
				if (c != ' ' && c != '\t' && profondeur == 1)
					++contenu;
			}
			if (contenu == 0)
				return 0u;
			return virgules + 1u;
		}

		// =====================================================================
		//  LE MONTEUR
		// =====================================================================
		class NkGuiMonteur {
			public:
				/// Phase 1 : recenser les cles d'etat de tout le document. A appeler
				/// AVANT `Monter` -- voir l'avertissement de `NkGuiMonteEtat`.
				static void Preparer(const NkArchive &doc, NkGuiMonteEtat &etat) noexcept {
					const NkArchiveNode *corps = NkGMonteCorps(doc);
					if (!corps)
						return;
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const NkArchive &sec = *corps->array[i].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(sec), "widgets"))
							continue;
						PreparerCorps(sec, etat);
					}
				}

				/// Phase 2 : monter le document dans `ctx`, au curseur courant.
				/// L'appelant a deja fait `ctx.BeginLayout(region)`.
				static void Monter(NkGuiContext &ctx, const NkArchive &doc, NkGuiMonteEtat &etat,
								   NkGuiMonteRapport &rap) noexcept {
					const NkArchiveNode *corps = NkGMonteCorps(doc);
					if (!corps)
						return;
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const NkArchive &sec = *corps->array[i].object;
						const NkStringView nom = NkGuiArchive::TypeOf(sec);
						++rap.sections;
						rap.nomsSections.PushBack(NkString(nom));
						if (NkGMotEgal(nom, "behavior")) {
							++rap.behaviors;
							continue;
						}
						if (NkGMotEgal(nom, "animation")) {
							++rap.animations;
							continue;
						}
						if (!NkGMotEgal(nom, "widgets"))
							continue;  // geometry, controller, callback, fonts, include
						MonterCorps(ctx, sec, etat, rap, 0u, false);
					}
				}

			private:
				// ── phase 1 ──────────────────────────────────────────────────
				static void PreparerCorps(const NkArchive &bloc, NkGuiMonteEtat &etat) noexcept {
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
						etat.Declarer(NkStringView(CleEtat(w)));
						PreparerCorps(w, etat);
					}
				}

				/// La cle d'etat d'un widget : son `bind` s'il en a un, sinon son id.
				/// Le `bind` d'abord, parce que deux widgets lies a la meme donnee
				/// doivent partager la valeur -- c'est ce que `bind` veut dire.
				static NkString CleEtat(const NkArchive &w) noexcept {
					const NkArchiveNode *b = w.FindNode(NkStringView("bind"));
					if (b)
						return NkString(b->Lexeme());
					return NkString(NkGuiArchive::IdOf(w));
				}

				// ── phase 2 ──────────────────────────────────────────────────
				static void MonterCorps(NkGuiContext &ctx, const NkArchive &bloc, NkGuiMonteEtat &etat,
										NkGuiMonteRapport &rap, uint32 prof, bool horizontal) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(bloc);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						MonterBloc(ctx, *c->array[k].object, etat, rap, prof, horizontal);
					}
				}

				static void MonterBloc(NkGuiContext &ctx, const NkArchive &w, NkGuiMonteEtat &etat,
									   NkGuiMonteRapport &rap, uint32 prof, bool horizontal) noexcept {
					const NkStringView t = NkGuiArchive::TypeOf(w);

					// L'apparence n'est pas un widget : on la compte, on ne la monte pas.
					if (NkGEstApparence(t)) {
						if (NkGMotEgal(t, "appearance")) {
							++rap.apparencesLues;
							if (NkGuiArchive::StateOf(w).Size() > 0
								&& !EtatEstRepos(NkGuiArchive::StateOf(w)))
								++rap.etatsNonAppliques;
						}
						return;
					}

					const NkGuiRole role = NkGuiRoleDepuisNom(t);
					if (role == NkGuiRole::Inconnu) {
						++rap.rolesInconnus;
						return;
					}
					++rap.widgets;

					const NkString id(NkGuiArchive::IdOf(w));
					const char *lbl = id.CStr();
					NkGuiMonteEtat::Entree *e = etat.Get(NkStringView(CleEtat(w)));
					bool aDessine = true;

					switch (role) {
						// ── CONTENEURS ───────────────────────────────────────
						case NkGuiRole::Window:
						case NkGuiRole::Panel: {
							// Le fond, puis le contenu DANS LE FLUX. `BeginPanel`
							// n'est pas ouvert ici : voir l'en-tete du fichier.
							NkRect r = RegionCourante(ctx, w);
							PanelBackground(ctx, r);
							// ⚠️ LE `title` DU FICHIER ETAIT PERDU. Le monteur n'ouvre
							//    pas `BeginPanel` (voir l'en-tete du fichier), mais ne
							//    pas ouvrir un conteneur n'est pas une raison de jeter
							//    ce que le document ecrit. Il est peint a la main, au
							//    meme endroit qu'une barre de titre.
							const NkString titre = NkGTexte(w, "title", "");
							if (titre.Size() > 0 && ctx.font && ctx.font->Valid()) {
								const NkVec2 coin{r.x + ctx.layout.padding,
												  r.y + ctx.layout.padding * 0.5f};
								(void)TextAt(ctx, coin, titre.CStr());
								ctx.layout.cursor.y += ctx.ItemHeight();
							}
							Noter(rap, id, t, r, prof, true, horizontal);
							MonterCorps(ctx, w, etat, rap, prof + 1u, false);
							++rap.montes;
							return;
						}
						case NkGuiRole::Group: {
							const NkVec2 c0 = ctx.layout.cursor;
							BeginGroup(ctx);
							MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal);
							EndGroup(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal);
							++rap.montes;
							return;
						}
						case NkGuiRole::VBox: {
							const float32 gap = NkGA(w, "gap") ? NkGNombre(w, "gap", -1.f) : -1.f;
							const NkVec2 c0 = ctx.layout.cursor;
							BeginVBox(ctx, gap);
							MonterCorps(ctx, w, etat, rap, prof + 1u, false);
							EndVBox(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal);
							++rap.montes;
							return;
						}
						case NkGuiRole::HBox: {
							const float32 gap = NkGA(w, "gap") ? NkGNombre(w, "gap", -1.f) : -1.f;
							const NkVec2 c0 = ctx.layout.cursor;
							BeginHBox(ctx, gap);
							MonterCorps(ctx, w, etat, rap, prof + 1u, true);
							EndHBox(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal);
							++rap.montes;
							return;
						}

						// ── FEUILLES ─────────────────────────────────────────
						case NkGuiRole::Text: {
							const NkString s = NkGTexte(w, "text", "");
							if (NkGBooleen(w, "wrap", false))
								TextWrapped(ctx, s.CStr());
							else
								Text(ctx, s.CStr());
							break;
						}
						case NkGuiRole::Button: {
							const NkString s = NkGTexte(w, "label", id.CStr());
							(void)Button(ctx, s.CStr());
							break;
						}
						case NkGuiRole::RepeatButton: {
							// Le seul role sans forme auto-placee : on lui donne le
							// rectangle que le layout aurait donne, pas un rectangle
							// invente.
							const NkString s = NkGTexte(w, "label", id.CStr());
							const NkRect r = ctx.NextItemRect(0.f, ctx.ItemHeight());
							(void)RepeatButton(ctx, s.CStr(), r, NkGNombre(w, "repeatDelay", -1.f),
											   NkGNombre(w, "repeatRate", -1.f));
							break;
						}
						case NkGuiRole::Checkbox: {
							const NkString s = NkGTexte(w, "label", id.CStr());
							if (e)
								(void)Checkbox(ctx, s.CStr(), e->b);
							else
								aDessine = false;
							break;
						}
						case NkGuiRole::Slider: {
							if (e) {
								const float32 vmin = NkGNombre(w, "min", 0.f);
								const float32 vmax = NkGNombre(w, "max", 1.f);
								// ⚠️ LA VALEUR INITIALE RESPECTE LES BORNES ECRITES. Vu
								//    sur l'image rendue : le curseur de
								//    `01_panneau_reglages` affichait 0.00 alors que le
								//    fichier ecrit `min = 0.5`. Un montage fidele ne
								//    peut pas poser une valeur que le document
								//    interdit -- et `SliderFloat` ne corrige pas une
								//    valeur d'entree hors bornes, il la dessine.
								if (!e->initialise) {
									e->f = NkGNombre(w, "value", vmin);
									e->initialise = true;
								}
								if (e->f < vmin)
									e->f = vmin;
								if (e->f > vmax)
									e->f = vmax;
								(void)SliderFloat(ctx, lbl, e->f, vmin, vmax);
							} else {
								aDessine = false;
							}
							break;
						}
						case NkGuiRole::TextField: {
							if (e) {
								// ⚠️ LE `placeholder` N'EST PAS UNE VALEUR. La premiere
								//    version le copiait dans le tampon : l'image
								//    montrait « Filtrer... » comme si l'utilisateur
								//    l'avait tape. Un texte d'invite s'affiche quand le
								//    champ est VIDE et disparait des qu'on ecrit.
								//    LIMITE NOMMEE : `InputText` n'a pas de
								//    placeholder ; le champ part donc vide, et
								//    l'invite du fichier n'est pas rendue. La
								//    perdre est moins faux que de la faire passer
								//    pour une saisie.
								if (!e->initialise) {
									const NkString v = NkGTexte(w, "value", "");
									Copier(e->texte, (int32)sizeof(e->texte), v);
									e->initialise = true;
								}
								(void)InputText(ctx, lbl, e->texte, (int32)sizeof(e->texte));
							} else {
								aDessine = false;
							}
							break;
						}
						case NkGuiRole::Dropdown: {
							const uint32 n = NkGListeCompte(w, "items");
							const NkString apercu = NkGTexte(w, "preview", "");
							if (BeginCombo(ctx, lbl, apercu.CStr(), (int32)n))
								EndCombo(ctx);
							break;
						}
						case NkGuiRole::Progress: {
							const float32 v = e ? e->f : NkGNombre(w, "value", 0.f);
							ProgressBar(ctx, v);
							break;
						}
						case NkGuiRole::Separator: {
							Separator(ctx);
							break;
						}
						case NkGuiRole::Spacer: {
							// `size = 12` est UN nombre : c'est l'axe du conteneur qui
							// dit lequel des deux cotes il mesure.
							const float32 s = NkGNombre(w, "size", 0.f);
							Spacer(ctx, horizontal ? s : 0.f, horizontal ? 0.f : s);
							break;
						}
						case NkGuiRole::Image: {
							const float32 iw = NkGNombre(w, "width", 64.f);
							const float32 ih = NkGNombre(w, "height", 64.f);
							Image(ctx, 0u, iw, ih);
							break;
						}
						case NkGuiRole::Chart: {
							float32 vals[64];
							const uint32 n = NkGListeNombres(w, "values", vals, 64u);
							const uint32 m = n < 64u ? n : 64u;
							if (m > 0u) {
								const NkString kind = NkGTexte(w, "kind", "Lignes");
								if (NkGMotEgal(NkStringView(kind), "Barres"))
									PlotHistogram(ctx, lbl, vals, (int32)m, NkGNombre(w, "min", 0.f),
												  NkGNombre(w, "max", 0.f), NkGNombre(w, "height", 0.f));
								else
									PlotLines(ctx, lbl, vals, (int32)m, NkGNombre(w, "min", 0.f),
											  NkGNombre(w, "max", 0.f), NkGNombre(w, "height", 0.f));
							} else {
								aDessine = false;
							}
							break;
						}
						case NkGuiRole::Callback: {
							// Ce n'est pas une chose qui se voit : c'est un point
							// d'entree. Compte comme widget du document, jamais monte.
							aDessine = false;
							break;
						}
						default:
							aDessine = false;
							break;
					}

					if (aDessine)
						++rap.montes;
					Noter(rap, id, t, ctx.layout.prevItem, prof, false, horizontal);
					// Un widget feuille peut porter une apparence : elle se compte.
					CompterApparences(w, rap);
				}

				/// Le bloc qu'un CONTENEUR a consomme : du curseur d'avant celui
				/// d'apres. Un conteneur n'appelle pas `NextItemRect` pour lui-meme --
				/// il empile un sous-flux -- donc ni `prevItem` ni `lastItemRect` ne
				/// parlent de lui. Le curseur, si.
				static NkRect BlocConsomme(const NkGuiContext &ctx, const NkVec2 &c0) noexcept {
					NkRect r;
					r.x = c0.x;
					r.y = c0.y;
					r.w = ctx.layout.region.w;
					r.h = ctx.layout.cursor.y - c0.y;
					if (r.h < 0.f)
						r.h = 0.f;
					return r;
				}

				static bool EtatEstRepos(NkStringView lexeme) noexcept {
					// `$state` porte la parenthese entiere telle qu'ecrite :
					// `(Normal)`, `(  Normal  )`, `(Hover)`.
					for (uint32 i = 0; i + 5u < (uint32)lexeme.Size() + 1u; ++i) {
						if (lexeme.Data()[i] == 'N' && i + 6u <= (uint32)lexeme.Size()) {
							bool ok = true;
							const char *m = "Normal";
							for (uint32 j = 0; j < 6u; ++j)
								if (lexeme.Data()[i + j] != m[j])
									ok = false;
							if (ok)
								return true;
						}
					}
					return false;
				}

				static void CompterApparences(const NkArchive &w, NkGuiMonteRapport &rap) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(w);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						const NkArchive &a = *c->array[k].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(a), "appearance"))
							continue;
						++rap.apparencesLues;
						const NkStringView st = NkGuiArchive::StateOf(a);
						if (st.Size() > 0 && !EtatEstRepos(st))
							++rap.etatsNonAppliques;
					}
				}

				/// La region que prend un conteneur de premier plan : son `pos`/`size`
				/// s'il les ecrit, sinon ce qui reste de la region courante.
				static NkRect RegionCourante(NkGuiContext &ctx, const NkArchive &w) noexcept {
					NkRect r = ctx.layout.region;
					float32 x = 0.f, y = 0.f, cw = 0.f, ch = 0.f;
					if (LireVec2(w, "pos", x, y) && LireVec2(w, "size", cw, ch)) {
						r.x = x;
						r.y = y;
						r.w = cw;
						r.h = ch;
					}
					return r;
				}

				/// `pos = (120, 80)` est un JETON NU : deux nombres entre parentheses.
				static bool LireVec2(const NkArchive &b, const char *cle, float32 &x,
									 float32 &y) noexcept {
					const NkArchiveNode *n = b.FindNode(NkStringView(cle));
					if (!n)
						return false;
					float32 v[4];
					const uint32 c = NkGNombresDansLexeme(n->Lexeme(), v, 4u);
					if (c < 2u)
						return false;
					x = v[0];
					y = v[1];
					return true;
				}

				/// ⚠️ LE RECTANGLE D'UN WIDGET SE LIT DANS `layout.prevItem`, PAS DANS
				/// `lastItemRect`. Mesure du 2026-09-14, et elle a coute un releve faux :
				/// `lastItemRect` n'est pose que par `ButtonBehavior` et `BeginDropTarget`,
				/// c'est-a-dire par les widgets INTERACTIFS. Un `Text`, un `Separator`, un
				/// `Spacer` ne l'ecrivent jamais : le relever apres eux rend le rectangle du
				/// dernier BOUTON rencontre. Le premier releve de ce banc annoncait donc
				/// « 4 chevauchements » sur un empilement parfaitement correct.
				/// `layout.prevItem`, lui, est pose par CHAQUE `NextItemRect` -- tous les
				/// widgets auto-places y passent, interactifs ou non.
				static void Noter(NkGuiMonteRapport &rap, const NkString &id, NkStringView role,
								  const NkRect &r, uint32 prof, bool conteneur,
								  bool axeHorizontal) noexcept {
					NkGuiMonteItem it;
					it.id = id;
					it.role = NkString(role);
					it.rect = r;
					it.profondeur = prof;
					it.conteneur = conteneur;
					it.axeHorizontal = axeHorizontal;
					rap.items.PushBack(it);
				}

				static void Copier(char *dst, int32 taille, const NkString &s) noexcept {
					int32 i = 0;
					for (; i < taille - 1 && i < (int32)s.Size(); ++i)
						dst[i] = s.Data()[i];
					dst[i] = '\0';
				}

		};

	} // namespace nkgui
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
