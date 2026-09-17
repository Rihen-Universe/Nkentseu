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
//            Depuis le 2026-09-17, il monte aussi la ZONE HOTE (`Host`) : un
//            rectangle que l'APPLICATION remplit par le crochet `RemplirHote`.
//            C'est la frontiere du format -- le document dit OU et QUOI, l'hote
//            garde QUAND et COMMENT -- et une zone que personne ne remplit se
//            SIGNALE (hachures + nom) au lieu de disparaitre.
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
#include <cstdlib> // getenv : la mutation de banc `sansmarqueur` de la zone hote

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
			TabBar,
			Expander,
			Splitter,
			// 🔴 `Callback` ETAIT ICI, ET C'ETAIT UN HOMONYME. Le mot appartient deja DEUX
			//    FOIS au format : `callback` est l'une des huit sections, et
			//    `Callback "alerte"(...)` est un APPEL DE COMPORTEMENT -- la grammaire
			//    l'ecrit (`NkGuiInteraction.h:66`) et le corpus l'emploie ainsi
			//    (`valides/05_animation_comportement.nkgui:29`, qui valide a 0 erreur).
			//    Le role de widget du meme nom n'etait dans AUCUN document et le
			//    validateur le refusait (`E-ROLE-INCONNU`, mesure) : du code que seul un
			//    document invalide pouvait atteindre. On ne le legalise pas, on le retire.
			/// LA ZONE QUE L'APPLICATION REMPLIT -- viseur 3D, toile, editeur de texte.
			/// Le document dit OU et QUOI ; l'hote garde QUAND et COMMENT.
			Host
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
			if (NkGMotEgal(n, "TabBar")) return NkGuiRole::TabBar;
			if (NkGMotEgal(n, "Expander")) return NkGuiRole::Expander;
			if (NkGMotEgal(n, "Splitter")) return NkGuiRole::Splitter;
			if (NkGMotEgal(n, "Host")) return NkGuiRole::Host;
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
				/// ⚠️ LA VALEUR REELLEMENT MONTEE, et elle existe parce qu'un
				///    releve de POSITIONS ne suffit pas. Le curseur de
				///    `01_panneau_reglages` a affiche 0.00 pour un fichier qui
				///    ecrit `min = 0.5` : la poignee, elle, etait au MEME pixel
				///    dans les deux cas (`SliderFloat` borne `tt` a [0,1], donc
				///    0.0 et 0.5 tombent tous deux a l'extremite gauche).
				///    **Un critere en pixels seul n'aurait pas pu rougir.**
				float32 valeur = 0.f;
				bool aValeur = false;
				/// Vrai si un champ de saisie est VIDE -- donc si ce qu'on voit
				/// dedans est une invite, et non une saisie.
				bool champVide = false;
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
				// ── LE PLACEMENT PAR WIDGET (2026-09-14) ─────────────────────
				// `pos` est l'interrupteur : un widget qui l'ecrit est POSE.
				uint32 poses = 0;			 ///< widgets qui ecrivent `pos`
				uint32 posesHonores = 0;	 ///< ceux dont le rectangle monte EST celui-la
				/// ⚠️ UN RECTANGLE POSE MAIS NON CONSOMME. `SetNextItemRect` ne vaut
				///    que pour le PROCHAIN widget auto-place ; un role qui ne passe
				///    pas par `NextItemRect` le laisserait ARME pour le suivant. On
				///    le desarme et on le COMPTE, plutot que de contaminer le voisin.
				uint32 posesNonConsommes = 0;
				// ── LA ZONE HOTE (2026-09-17) ────────────────────────────────
				/// Les `Host` rencontres : une zone se compte, remplie ou non.
				uint32 hotes = 0;
				/// ⚠️ CELLES QUE PERSONNE N'A REMPLIES. Ce compteur est la raison d'etre
				///    du marqueur : une zone vide qui ne se compte pas se fait prendre
				///    pour un fond, et le document a l'air monte alors qu'il manque sa
				///    partie la plus importante.
				uint32 hotesNonRemplis = 0;
				uint32 modales = 0;			 ///< `Window { modal = true }` rencontres
				/// Les drapeaux d'INTERACTION (`NoMove`, `NoResize`, `NoClose`,
				/// `NoScrollbar`) : ce monteur monte l'etat AU REPOS, il n'a aucune
				/// interaction a empecher. Comptes, jamais fait semblant.
				uint32 flagsNonAppliques = 0;
				// ── LA SECTION `geometry` (document 2 §1 et §3) ───────────────
				uint32 formes = 0;			 ///< blocs `shape` rencontres
				uint32 formesPeintes = 0;	 ///< celles dont la nature sait se peindre
				uint32 formesNonPeintes = 0; ///< `path`, `frame`, `image` sans texture
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

		/// Retire les espaces de bord d'un element de liste (`[ Scene , Rendu ]`).
		inline NkString NkGCouper(const NkString &s) noexcept {
			uint32 a = 0, b = (uint32)s.Size();
			while (a < b && (s.Data()[a] == ' ' || s.Data()[a] == '\t'))
				++a;
			while (b > a && (s.Data()[b - 1u] == ' ' || s.Data()[b - 1u] == '\t'))
				--b;
			NkString r;
			for (uint32 i = a; i < b; ++i)
				r += s.Data()[i];
			return r;
		}

		/// LES LIBELLES d'une liste en jeton nu : `tabs = [Scene, "Materiaux", Rendu]`.
		///
		/// ⚠️ DU COTE DU CONSOMMATEUR, ET C'EST LA REGLE DE CE FICHIER. Le lecteur le dit
		///    lui-meme : « un outil qui veut iterer dessus doit encore l'analyser lui-meme ».
		///    `NkGListeCompte` rend un NOMBRE -- ce dont `Dropdown` se contente. Pour appeler
		///    `TabBar(ctx, id, labels, count)` il faut les CHAINES. **Le format n'est pas
		///    touche** : ses listes restent des jetons nus, c'est ici qu'on les lit.
		///
		/// Accepte les elements nus (`Scene`) et les chaines (`"Materiaux"`). Les
		/// espaces de bord sont retires ; une liste vide rend zero element.
		inline uint32 NkGListeChaines(const NkArchive &b, const char *cle, NkVector<NkString> &out) noexcept {
			out.Clear();
			const NkArchiveNode *n = b.FindNode(NkStringView(cle));
			if (!n)
				return 0u;
			const NkString lex(n->Lexeme());
			const char *p = lex.Data();
			const uint32 len = (uint32)lex.Size();
			uint32 profondeur = 0;
			bool chaine = false;
			NkString courant;
			bool aDuContenu = false;
			for (uint32 i = 0; i < len; ++i) {
				const char c = p[i];
				if (chaine) {
					if (c == '\\' && i + 1u < len) {
						++i;
						courant += p[i];
						continue;
					}
					if (c == '"') {
						chaine = false;
						continue;
					}
					courant += c;
					aDuContenu = true;
					continue;
				}
				if (c == '"') {
					chaine = true;
					aDuContenu = true;
					continue;
				}
				if (c == '[' || c == '(' || c == '{') {
					++profondeur;
					continue;
				}
				if (c == ']' || c == ')' || c == '}') {
					if (profondeur > 0)
						--profondeur;
					if (profondeur == 0 && aDuContenu) {
						out.PushBack(NkGCouper(courant));
						courant = NkString();
						aDuContenu = false;
					}
					continue;
				}
				if (c == ',' && profondeur == 1) {
					out.PushBack(NkGCouper(courant));
					courant = NkString();
					aDuContenu = false;
					continue;
				}
				if (profondeur >= 1) {
					courant += c;
					if (c != ' ' && c != '\t')
						aDuContenu = true;
				}
			}
			return (uint32)out.Size();
		}

		// =====================================================================
		//  LE PLACEMENT EST UNE PROPRIETE DU CONTENEUR -- tranche le 2026-09-14
		// =====================================================================
		//
		//  Rodolf : « on dois pouvoir avoir du placement absolut comme non absolut
		//  ca va dependre de l'utilisateur », puis « au vu de son parent, un
		//  conteneur ne pourra jamais porter les deux. »
		//
		//  D'ou la regle, et elle tient en une ligne : **un conteneur declare
		//  `placement = absolute`, et alors SES enfants directs portent `pos`.**
		//  Sans cette declaration, le conteneur est en FLUX -- le defaut, inchange.
		//
		//  ⚠️ LE MECANISME EXISTAIT DEJA, ET IL N'A PAS ETE ECRIT POUR CECI.
		//     `NkGuiContext::SetNextItemRect` est la depuis le 2026-08-18, pour
		//     NK3DModeler : « un rectangle POSE pour le PROCHAIN widget seulement,
		//     consomme par NextItemRect ». Le FORMAT n'avait pas le mot ; NKGui
		//     avait deja la porte. On ne construit donc pas un second placement.
		//
		//  ⚠️ LES COORDONNEES SONT RELATIVES A LA REGION DU CONTENEUR. C'est ce
		//     qui fait qu'un dialogue pose EMPORTE son contenu quand on le deplace.
		//     A la racine, la region est la vue : les coordonnees y sont donc celles
		//     de l'ecran, et `03_virgule_vecteur_couleur` ne bouge pas d'un pixel.
		
		/// Vrai si ce bloc declare `placement = absolute`. Le defaut est le FLUX.
		inline bool NkGuiEstAbsolu(const NkArchive &b) noexcept {
			const NkArchiveNode *n = b.FindNode(NkStringView("placement"));
			if (!n)
				return false;
			NkStringView v = n->Lexeme();
			// L'etiquette peut s'ecrire nue ou entre guillemets : le schema dit 'e'.
			if (v.Size() >= 2u && v.Data()[0] == '\"')
				return NkGMotEgal(NkStringView(v.Data() + 1, (uint32)v.Size() - 2u), "absolute");
			return NkGMotEgal(v, "absolute");
		}
		
		/// Le placement ECRIT par un widget, exprime dans la region `reg` qui le
		/// contient. Rend faux quand il n'y a pas de `pos` -- c'est-a-dire dans
		/// l'immense majorite des cas, et c'est le defaut.
		///
		/// ⚠️ LA TAILLE PAR DEFAUT N'EST PAS INVENTEE : c'est celle que le flux
		///    aurait donnee (la largeur restante, `ItemHeight()` en hauteur). `pos`
		///    seul deplace donc un widget SANS le redimensionner, ce qui est
		///    exactement ce qu'on attend de « pose au point (x, y) ».
		struct NkGuiPlacement {
			bool pose = false;
			bool aTaille = false;
			NkRect rect{0.f, 0.f, 0.f, 0.f};
		};
		
		// =====================================================================
		//  LES TAILLES RELATIVES -- « cette colonne fait 16 % de son parent,
		//  au minimum 180 px »
		// =====================================================================
		//  ⚠️ POURQUOI ELLES EXISTENT, ET CE QUE MESURE L'INVENTAIRE DU 17/09 :
		//     `pos` et `size` sont en PIXELS ABSOLUS. Un document qui decrirait
		//     l'interface de NK3DModeler avec eux la FIGERAIT a une seule taille de
		//     fenetre -- alors que sa disposition reelle est ecrite en fractions
		//     (`NkLayout::Compute(W, H, fLeft = 0.16f, fRight = 0.29f)`). C'est ce
		//     qui separe un ecran de demonstration d'une fenetre d'editeur.
		//
		//  ⚠️ ET C'EST STRICTEMENT ADDITIF. `size` garde exactement son sens : il
		//     n'agit qu'avec `pos`. Les 226 noeuds absolus du depot ne changent pas
		//     d'un pixel -- c'est mesure par la non-regression du banc, pas espere.
		//
		//  Une composante <= 0 veut dire « cet axe n'est pas contraint » : on peut
		//  ne dire que la largeur, `sizeRel = (0.16, 0)`.
		struct NkGuiTailleRel {
				bool aW = false, aH = false;
				float32 w = 0.f, h = 0.f;
		};

		inline NkGuiTailleRel NkGuiLireTailleRelative(const NkArchive &b, const NkRect &region) noexcept {
			NkGuiTailleRel t;
			// MUTATION DE BANC, NK_TAILLE_MUTATION=absolu : `sizeRel` est ignore, et la
			// disposition redevient celle du flux. Elle sert a prouver que le critere des
			// deux fenetres teste quelque chose ; sans la variable, rien ne change.
			static const bool kIgnorer = []() {
				const char *v = getenv("NK_TAILLE_MUTATION");
				return v && v[0] == 'a';
			}();
			if (kIgnorer)
				return t;
			const NkArchiveNode *nr = b.FindNode(NkStringView("sizeRel"));
			if (!nr)
				return t;
			float32 v[4];
			if (NkGNombresDansLexeme(nr->Lexeme(), v, 4u) < 2u)
				return t; // `sizeRel = 0.16` n'est pas un vecteur : la validation le dit
			float32 mn[4] = {0.f, 0.f, 0.f, 0.f};
			float32 mx[4] = {0.f, 0.f, 0.f, 0.f};
			const NkArchiveNode *nmn = b.FindNode(NkStringView("minSize"));
			const NkArchiveNode *nmx = b.FindNode(NkStringView("maxSize"));
			if (nmn)
				(void)NkGNombresDansLexeme(nmn->Lexeme(), mn, 4u);
			if (nmx)
				(void)NkGNombresDansLexeme(nmx->Lexeme(), mx, 4u);
			if (v[0] > 0.f) {
				t.aW = true;
				t.w = region.w * v[0];
				if (mn[0] > 0.f && t.w < mn[0])
					t.w = mn[0];
				if (mx[0] > 0.f && t.w > mx[0])
					t.w = mx[0];
			}
			if (v[1] > 0.f) {
				t.aH = true;
				t.h = region.h * v[1];
				if (mn[1] > 0.f && t.h < mn[1])
					t.h = mn[1];
				if (mx[1] > 0.f && t.h > mx[1])
					t.h = mx[1];
			}
			return t;
		}

		inline NkGuiPlacement NkGuiLirePlacement(const NkArchive &w, const NkRect &reg,
						float32 hauteurRangee, bool conteneur) noexcept {
			NkGuiPlacement pl;
			const NkArchiveNode *np = w.FindNode(NkStringView("pos"));
			if (!np)
				return pl;
			float32 v[4];
			if (NkGNombresDansLexeme(np->Lexeme(), v, 4u) < 2u)
				return pl; // `pos = 3` n'est pas un point : la validation le dit deja
			pl.pose = true;
			pl.rect.x = reg.x + v[0];
			pl.rect.y = reg.y + v[1];
			pl.rect.w = conteneur ? (reg.w - v[0]) : 0.f;
			pl.rect.h = conteneur ? (reg.h - v[1]) : hauteurRangee;
			const NkArchiveNode *ns = w.FindNode(NkStringView("size"));
			if (ns && NkGNombresDansLexeme(ns->Lexeme(), v, 4u) >= 2u) {
				pl.aTaille = true;
				pl.rect.w = v[0];
				pl.rect.h = v[1];
			}
			return pl;
		}
		
		/// Vrai si le lexeme de `flags` contient CE drapeau. On travaille sur le
		/// TEXTE : `flags = NoTitleBar | NoMove` est un JETON NU, le lecteur ne
		/// construit aucune liste (il le dit lui-meme), donc c'est au CONSOMMATEUR
		/// de l'analyser -- exactement comme `items` et `values` plus haut.
		inline bool NkGuiDrapeau(const NkArchive &b, const char *nom) noexcept {
			const NkArchiveNode *n = b.FindNode(NkStringView("flags"));
			if (!n)
				return false;
			const NkStringView lex = n->Lexeme();
			uint32 l = 0;
			while (nom[l])
				++l;
			if (l == 0u || (uint32)lex.Size() < l)
				return false;
			for (uint32 i = 0; i + l <= (uint32)lex.Size(); ++i) {
				uint32 k = 0;
				while (k < l && lex.Data()[i + k] == nom[k])
					++k;
				if (k != l)
					continue;
				// ⚠️ ET IL FAUT VERIFIER LA FRONTIERE : sans elle, `NoMove` serait
				//    trouve dans un hypothetique `NoMoveX`. Un drapeau est un
				//    identifiant ENTIER, pas une sous-chaine.
				const char c = (i + l < (uint32)lex.Size()) ? lex.Data()[i + l] : ' ';
				const bool suite = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
								 || (c >= '0' && c <= '9') || c == '_';
				if (!suite)
					return true;
			}
			return false;
		}
		
		/// Une couleur `#RRGGBB` / `#RRGGBBAA` du format, en `NkColor`.
		/// ⚠️ PAS DE PARSEUR MAISON : `NkColorF::FromHex` existe dans NKMath et
		///    traite les deux longueurs. En reecrire un ici aurait donne deux
		///    lectures d'une meme couleur, qui finissent par diverger.
		inline bool NkGuiCouleur(NkStringView lex, NkColor &out) noexcept {
			const uint32 n = (uint32)lex.Size();
			if ((n != 7u && n != 9u) || lex.Data()[0] != '#')
				return false;
			char buf[10];
			for (uint32 i = 0; i < n; ++i)
				buf[i] = lex.Data()[i];
			buf[n] = '\0';
			out = math::NkColorF::FromHex(buf).ToColor();
			return true;
		}
		
		// =====================================================================
		//  LE CROCHET D'EXECUTION -- et pourquoi il est ici et pas ailleurs
		// =====================================================================
		/**
		 * @brief Ce que le monteur laisse faire a qui veut EXECUTER le document.
		 *
		 * Le monteur monte l'ETAT AU REPOS, c'est ce que son en-tete promet et
		 * c'est ce qu'il continuera de faire seul. Mais quatre choses du format
		 * ne peuvent se faire qu'au moment PRECIS ou un widget se dessine :
		 *
		 *   - desactiver le widget (`enabled = false`) AVANT qu'il ne peigne,
		 *     parce que `BeginDisabled` est une pile et qu'elle se pose autour ;
		 *   - savoir QUEL widget du document NKGui est en train de peindre, pour
		 *     que le crochet de style (`ctx.styleFn`) puisse choisir la couleur
		 *     que le FICHIER declare pour l'etat courant ;
		 *   - pousser la donnee liee (`bind`) dans la valeur editee juste avant,
		 *     et relire ce que l'utilisateur en a fait juste apres ;
		 *   - peindre par-dessus ce que le widget ne sait pas peindre (l'anneau
		 *     de focus d'un champ de saisie : `InputText` n'a pas de crochet).
		 *
		 * ⚠️ C'EST UN CROCHET, PAS UNE SECONDE PORTE. Le monteur reste le seul a
		 *    monter ; ce qui suit ne fait que l'encadrer. `Monter` garde sa
		 *    signature a quatre arguments -- les 162 criteres de `NKGuiMonteTest`
		 *    l'appellent encore comme avant et restent la mesure de
		 *    non-regression.
		 *
		 * ⚠️ `Apres` EST APPELE MEME QUAND LE WIDGET SORT TOT. Les conteneurs
		 *    (`Window`, `VBox`, `HBox`, `Group`) quittent `MonterBloc` par un
		 *    `return` au milieu du `switch`. Un appariement ecrit a la main
		 *    aurait donc laisse une pile `BeginDisabled` ouverte sur le premier
		 *    conteneur desactive rencontre -- et la faute ne se serait vue que
		 *    beaucoup plus loin, sur un widget qui n'a rien demande. L'appariement
		 *    passe par un objet de pile (`Garde`), pas par de la discipline.
		 */
		struct NkGuiMonteHooks {
				virtual ~NkGuiMonteHooks() = default;
				/// Avant que le widget ne dessine. `e` peut etre nul (role sans etat).
				virtual void Avant(NkGuiContext &ctx, const NkArchive &w, NkStringView role,
								   NkGuiMonteEtat::Entree *e) noexcept {
					(void)ctx;
					(void)w;
					(void)role;
					(void)e;
				}
				/// Apres, quelle que soit la porte de sortie.
				virtual void Apres(NkGuiContext &ctx, const NkArchive &w, NkStringView role,
								   NkGuiMonteEtat::Entree *e) noexcept {
					(void)ctx;
					(void)w;
					(void)role;
					(void)e;
				}

				/// LA ZONE HOTE : l'hote peint `zone`, et rend VRAI s'il l'a fait.
				///
				/// ⚠️ LE DEFAUT EST `false`, ET C'EST VOULU. Un hote qui ne connait pas ce
				///    nom ne doit pas repondre oui : le monteur peindra alors son marqueur
				///    et comptera la zone comme non remplie. Repondre vrai sans peindre
				///    ferait disparaitre la zone en silence -- exactement ce que ce role
				///    existe pour empecher.
				virtual bool RemplirHote(NkGuiContext &ctx, const char *nom, const NkRect &zone) noexcept {
					(void)ctx;
					(void)nom;
					(void)zone;
					return false;
				}
		};

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
									   NkGuiMonteRapport &rap,
									   NkGuiMonteHooks *hooks = nullptr) noexcept {
					const NkArchiveNode *corps = NkGMonteCorps(doc);
					if (!corps)
						return;
					// ⚠️ LES CALQUES D'ABORD, QUEL QUE SOIT L'ORDRE DU FICHIER. Le
					//    document 2 §1 appelle `geometry` « les calques du canvas » : un
					//    calque qui passerait devant les widgets ne serait plus un calque.
					//    Une PASSE SEPAREE le garantit ; le peindre dans la boucle d'apres
					//    aurait rendu l'ordre de peinture dependant de l'ordre d'ecriture.
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
					// ⚠️ LA PASSE `geometry` NE RECOIT PAS LE CROCHET, ET C'EST MOTIVE.
					//    Une `shape` est un CALQUE : ni role, ni etat, ni evenement. Le
					//    crochet sert a desactiver un widget, a lui choisir la couleur de
					//    son etat et a pousser sa donnee liee -- une forme n'a aucune des
					//    trois. L'appeler ici ferait courir `Avant`/`Apres` sur un objet
					//    qui n'est pas un widget, et fausserait les compteurs par etat.
					if (NkGMotEgal(NkGuiArchive::TypeOf(*corps->array[i].object), "geometry"))
						MonterGeometrie(ctx, *corps->array[i].object, rap);
					}
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
						// La racine de `widgets` est ABSOLUE par nature : rien ne la contient,
						// donc aucun conteneur ne peut y declarer son mode -- et le corpus
						// l'atteste (`03` pose son `Window` a la racine depuis le premier jour).
						MonterCorps(ctx, sec, etat, rap, 0u, false, /*parentAbsolu=*/true, hooks);
					}
				}

			private:
				// ── phase 1 ──────────────────────────────────────────────────
				// ── LES CALQUES (`geometry`) ─────────────────────────
				// Une forme ne touche NI le curseur NI la region : elle n'est dans aucun
				// flux, elle EST son rectangle. C'est ce qui la distingue d'un widget
				// pose -- un widget garde un role, un etat, un evenement ; un calque n'en
				// a aucun (document 3, a propos de `geometry` : « ce qui en ferait un
				// calque et non un widget, donc sans role, sans etat, sans evenement »).
				static void MonterGeometrie(NkGuiContext &ctx, const NkArchive &sec,
									NkGuiMonteRapport &rap) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(sec);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						const NkArchive &fo = *c->array[k].object;
						if (!NkGMotEgal(NkGuiArchive::TypeOf(fo), "shape"))
							continue; // la validation le signale ; le monteur ne devine pas
						++rap.formes;
						float32 v[4];
						NkRect r{0.f, 0.f, 0.f, 0.f};
						const NkArchiveNode *np = fo.FindNode(NkStringView("pos"));
						const NkArchiveNode *ns = fo.FindNode(NkStringView("size"));
						if (np && NkGNombresDansLexeme(np->Lexeme(), v, 4u) >= 2u) {
							r.x = v[0];
							r.y = v[1];
						}
						if (ns && NkGNombresDansLexeme(ns->Lexeme(), v, 4u) >= 2u) {
							r.w = v[0];
							r.h = v[1];
						}
						const NkString nature = NkGTexte(fo, "kind", "rect");
						const NkString id(NkGuiArchive::IdOf(fo));
						// La couleur : un JETON NU `#RRGGBB`. Faute de couleur ecrite, on prend
						// l'encre du THEME -- jamais un noir invente, invisible sur fond sombre.
						NkColor col = ctx.theme.text;
						const NkArchiveNode *nc = fo.FindNode(NkStringView("color"));
						if (nc)
							(void)NkGuiCouleur(nc->Lexeme(), col);
						bool peinte = true;
						if (NkGMotEgal(NkStringView(nature), "rect")) {
							ctx.DL().AddRectFilled(r, col, NkGNombre(fo, "radius", 0.f));
						} else if (NkGMotEgal(NkStringView(nature), "ellipse")) {
							ctx.DL().AddEllipseFilled({r.x + r.w * 0.5f, r.y + r.h * 0.5f}, r.w * 0.5f,
											   r.h * 0.5f, col);
						} else if (NkGMotEgal(NkStringView(nature), "text")) {
							const NkString txt = NkGTexte(fo, "text", "");
							if (txt.Size() > 0 && ctx.font && ctx.font->Valid())
								ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
												 {r.x, r.y + ctx.font->Ascent()}, txt.CStr(), col);
							else
								peinte = false;
						} else {
							// `image`, `path`, `frame` : la nature est CONNUE du format et ce
							// monteur ne sait pas la peindre (une image demande une texture, un
							// trace demande ses points). Comptee, jamais devinee.
							peinte = false;
						}
						if (peinte)
							++rap.formesPeintes;
						else
							++rap.formesNonPeintes;
						Noter(rap, id, NkStringView("shape"), r, 0u, false, false);
					}
				}
				
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
									NkGuiMonteRapport &rap, uint32 prof, bool horizontal,
									bool parentAbsolu, NkGuiMonteHooks *hooks) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(bloc);
					if (!c)
						return;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						MonterBloc(ctx, *c->array[k].object, etat, rap, prof, horizontal, parentAbsolu,
									   hooks);
					}
				}

				/// LES DEUX ENFANTS D'UN SEPARATEUR, chacun dans SA moitie.
				///
				/// ⚠️ ELLE EXISTE POUR UN SEUL APPELANT, ET C'EST ASSUME. `MonterCorps` monte
				///    tous les enfants dans la MEME region ; un separateur en donne une
				///    DIFFERENTE a chacun de ses deux premiers enfants. Plutot que d'ajouter
				///    un parametre a `MonterCorps` -- que ses six autres appelants auraient
				///    du passer a vide -- on ecrit la boucle qui fait autre chose.
				/// Les enfants au-dela du deuxieme sont montes dans la seconde moitie : un
				/// document qui en met trois n'en perd aucun, et le releve le montrera.
				static void MonterCorpsBorne(NkGuiContext &ctx, const NkArchive &bloc, NkGuiMonteEtat &etat,
											 NkGuiMonteRapport &rap, uint32 prof, NkGuiMonteHooks *hooks,
											 const NkRect &regionA, const NkRect &regionB) noexcept {
					const NkArchiveNode *c = NkGMonteCorps(bloc);
					if (!c)
						return;
					uint32 rang = 0;
					for (uint32 k = 0; k < (uint32)c->array.Size(); ++k) {
						if (!c->array[k].IsObject() || !c->array[k].object)
							continue;
						const NkGuiLayout sauve = ctx.layout;
						ctx.BeginLayout(rang == 0u ? regionA : regionB);
						MonterBloc(ctx, *c->array[k].object, etat, rap, prof, false, false, hooks);
						ctx.layout = sauve;
						++rang;
					}
				}

				/// Appariement du crochet par objet de PILE : `Apres` part meme quand
				/// le `switch` de `MonterBloc` sort par un `return` (les conteneurs le
				/// font tous). Voir l'avertissement de `NkGuiMonteHooks`.
				struct Garde {
						NkGuiMonteHooks *h = nullptr;
						NkGuiContext *ctx = nullptr;
						const NkArchive *w = nullptr;
						NkStringView role;
						NkGuiMonteEtat::Entree *e = nullptr;
						Garde(NkGuiMonteHooks *hooks, NkGuiContext &c, const NkArchive &bloc,
							  NkStringView r, NkGuiMonteEtat::Entree *ent) noexcept
							: h(hooks), ctx(&c), w(&bloc), role(r), e(ent) {
							if (h)
								h->Avant(*ctx, *w, role, e);
						}
						~Garde() {
							if (h)
								h->Apres(*ctx, *w, role, e);
						}
				};

				static void MonterBloc(NkGuiContext &ctx, const NkArchive &w, NkGuiMonteEtat &etat,
								   NkGuiMonteRapport &rap, uint32 prof, bool horizontal,
								   bool parentAbsolu, NkGuiMonteHooks *hooks) noexcept {
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
					
					// ── LE PLACEMENT ─────────────────────────────────
					// `pos` n'est LU que si le parent est absolu. Sous un conteneur en flux,
					// des coordonnees ne sont pas ignorees en silence : le VALIDATEUR les
					// refuse (`E-PLACEMENT`). Le monteur n'a pas a deviner ce que le format
					// interdit -- il monte ce qui est juste.
					const bool estConteneur = (role == NkGuiRole::Window || role == NkGuiRole::Panel
										  || role == NkGuiRole::Group || role == NkGuiRole::VBox
										  || role == NkGuiRole::HBox);
					NkGuiPlacement pl;
					if (parentAbsolu)
						pl = NkGuiLirePlacement(w, ctx.layout.region, ctx.ItemHeight(), estConteneur);
					if (pl.pose)
						++rap.poses;
					// Ce que CE conteneur impose a SES enfants -- independant de ce que son
					// propre parent lui impose. Confondre les deux rendrait l'absolu contagieux
					// vers le bas, et un dialogue pose enfermerait toute sa descendance dans un
					// mode qu'elle n'a pas demande.
					const bool enfantsAbsolus = NkGuiEstAbsolu(w);
					// ⚠️ UNE FEUILLE POSEE PASSE PAR LA PORTE QUE NKGUI A DEJA.
					//    `SetNextItemRect` vaut pour LE PROCHAIN widget auto-place, et le
					//    curseur ne bouge pas. Un conteneur, lui, ouvre une REGION (plus bas) :
					//    ce n'est pas le meme geste, parce qu'un conteneur doit emporter ses
					//    enfants avec lui.
					// 🔴 L'ARMEMENT A CHANGE DE PLACE A LA FUSION, ET CE N'EST PAS COSMETIQUE.
					//    Il vivait ICI, avant que `e` ne soit lu. Depuis que le crochet
					//    d'execution existe, `Avant()` s'execute ENTRE ce point et le widget :
					//    tout ce qu'un crochet ferait qui consomme `NextItemRect` mangerait le
					//    rectangle a la place du widget qui l'attend.
					//    Mesure faite AVANT de deplacer : le seul `Avant()` du depot appelle
					//    `BeginDisabled`, qui ne fait qu'empiler un booleen -- **aujourd'hui**
					//    les deux places donnent le meme resultat. Un crochet est justement
					//    l'endroit ou quelqu'un d'autre ecrira du code demain. On arme donc
					//    AU PLUS PRES DU CONSOMMATEUR, juste avant le `switch`.
					
					const NkString id(NkGuiArchive::IdOf(w));
					const char *lbl = id.CStr();
					NkGuiMonteEtat::Entree *e = etat.Get(NkStringView(CleEtat(w)));
					// Le crochet encadre TOUT ce qui suit, sorties anticipees comprises.
					Garde garde(hooks, ctx, w, t, e);
					// ── L'ARMEMENT, AU PLUS PRES DU CONSOMMATEUR ──────────────────
					// `SetNextItemRect` ne vaut que pour le PROCHAIN widget auto-place. Rien
					// ne doit s'intercaler entre cette ligne et le `switch` -- surtout pas
					// un crochet, qui est du code ecrit par quelqu'un d'autre.
					if (pl.pose && !estConteneur)
						ctx.SetNextItemRect(pl.rect);
					bool aDessine = true;
					float32 valeurMontee = 0.f;
					bool aValeurMontee = false;
					bool champVideMonte = false;

					switch (role) {
						// ── CONTENEURS ───────────────────────────────────────
						case NkGuiRole::Window:
						case NkGuiRole::Panel: {
							// Le fond, puis le contenu. `BeginPanel` n'est pas ouvert ici : voir
							// l'en-tete du fichier.
							NkRect r = pl.pose ? pl.rect : RegionCourante(ctx, w);
							// ⚠️ UN CONTENEUR ABSOLU QUI VIT DANS UN FLUX PART DU CURSEUR, pas du
							//    bord de la region. Sans cette ligne il reprendrait TOUTE la region
							//    de son parent et se peindrait par-dessus ses freres deja poses --
							//    un chevauchement que personne n'a demande. Il ne peut pas, lui,
							//    porter de `pos` : son parent est en flux, et le validateur le
							//    refuse. Le curseur est donc la seule chose qui dise ou il commence.
							if (!pl.pose && enfantsAbsolus) {
								r.w = ctx.layout.region.w - (ctx.layout.cursor.x - ctx.layout.region.x);
								r.h = ctx.layout.region.h - (ctx.layout.cursor.y - ctx.layout.region.y);
								r.x = ctx.layout.cursor.x;
								r.y = ctx.layout.cursor.y;
							}
							// ── LA TAILLE RELATIVE : « 16 % de mon parent, au minimum 180 px » ──
							// Un conteneur qui declare `sizeRel` DECOUPE son rectangle dans le flux
							// du parent par la porte que NKGui a deja (`NextItemRect`) : c'est elle
							// qui sait avancer le curseur en X dans une HBox et en Y ailleurs. En
							// ecrire une seconde aurait fait deux verites sur la meme chose.
							const NkGuiTailleRel rel = NkGuiLireTailleRelative(w, ctx.layout.region);
							bool decoupeRel = false;
							if (!pl.pose && (rel.aW || rel.aH)) {
								r = ctx.NextItemRect(rel.aW ? rel.w : -1.f,
													 rel.aH ? rel.h : ctx.AvailHeight());
								decoupeRel = true;
							}
							// ⚠️ LE VOILE D'UNE MODALE, PEINT AVANT LE FOND.
							//    `modal` etait DECLARE par le format (doc 7 §3.6) et lu par PERSONNE :
							//    ni le monteur, ni le validateur au-dela du type. Une propriete que
							//    rien ne lit est une promesse que le fichier fait et que l'outil ne
							//    tient pas. C'est aussi la reponse a « boite de dialogue » : ce n'est
							//    pas un role a inventer, c'est `Window { modal = true }`, que le
							//    document 7 §4 avait deja tranche (« la modalite est une PROPRIETE »).
							//    ⚠️ LA COULEUR EST CELLE QUE LE KIT A DEJA CHOISIE pour ses dialogues
							//       (`NkEditorModal.h` : `NkColor{0, 0, 0, 120}`). En prendre une autre
							//       aurait donne deux voiles differents pour la meme idee.
							if (NkGBooleen(w, "modal", false)) {
								++rap.modales;
								ctx.DL().AddRectFilled(ctx.layout.region, NkColor{0, 0, 0, 120});
							}
							// Les drapeaux d'INTERACTION : ce monteur monte l'etat AU REPOS, il n'a
							// aucun deplacement ni redimensionnement a empecher. Ils sont COMPTES,
							// jamais fait semblant d'appliquer.
							{
								static const char *kInteraction[] = {"NoMove", "NoResize", "NoClose",
												  "NoCollapse", "NoScrollbar"};
								for (uint32 fi = 0; fi < 5u; ++fi)
									if (NkGuiDrapeau(w, kInteraction[fi]))
										++rap.flagsNonAppliques;
							}
							PanelBackground(ctx, r);
							// ⚠️ LE `title` DU FICHIER ETAIT PERDU. Le monteur n'ouvre
							//    pas `BeginPanel` (voir l'en-tete du fichier), mais ne
							//    pas ouvrir un conteneur n'est pas une raison de jeter
							//    ce que le document ecrit. Il est peint a la main, au
							//    meme endroit qu'une barre de titre.
							const NkString titre = NkGTexte(w, "title", "");
							// ⚠️ `NoTitleBar` EST LE SEUL DRAPEAU QUI CHANGE QUELQUE CHOSE ICI,
							//    parce que c'est le seul qui parle de ce qui se PEINT. Les quatre
							//    autres parlent de GESTES, et ce monteur n'en a aucun.
							const bool sansTitre = NkGuiDrapeau(w, "NoTitleBar");
							const bool aTitre = !sansTitre && titre.Size() > 0 && ctx.font && ctx.font->Valid();
							if (aTitre) {
								const NkVec2 coin{r.x + ctx.layout.padding, r.y + ctx.layout.padding * 0.5f};
								(void)TextAt(ctx, coin, titre.CStr());
							}
							Noter(rap, id, t, r, prof, true, horizontal);
							// ── OUVRIR UNE REGION, OU NON ─────────────────────────
							// ⚠️ STRICTEMENT ADDITIF, ET C'EST CETTE CONDITION QUI LE GARANTIT. Un
							//    conteneur qui n'est ni POSE ni ABSOLU se comporte EXACTEMENT comme
							//    avant : le contenu reste dans le flux du parent, le titre pousse le
							//    curseur exterieur. C'est ce qui laisse les dix `.nkgui` valides du
							//    corpus rendre le meme pixel qu'hier -- mesure, pas espere.
							//
							// ⚠️ ET `BeginLayout` N'A PAS DE PILE. On sauve `ctx.layout` par valeur et
							//    on le repose : c'est exactement ce que NKGui fait deja pour ses popups
							//    (`popupSaved[PopupMax]`, un `NkGuiLayout` par niveau). On reprend son
							//    procede plutot que d'en inventer un second.
							const bool ouvrirRegion = pl.pose || enfantsAbsolus || decoupeRel;
							if (ouvrirRegion) {
								const NkGuiLayout sauve = ctx.layout;
								ctx.BeginLayout(r);
								if (aTitre)
									ctx.layout.cursor.y += ctx.ItemHeight();
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, enfantsAbsolus, hooks);
								ctx.layout = sauve;
							} else {
								if (aTitre)
									ctx.layout.cursor.y += ctx.ItemHeight();
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, enfantsAbsolus, hooks);
							}
							++rap.montes;
							return;
						}
						case NkGuiRole::Group: {
							// ⚠️ UN `Group` PEUT DECLARER `placement = absolute` -- c'est l'un des
							//    trois conteneurs NEUTRES (avec `Window` et `Panel`). Une `VBox` ne le
							//    peut pas : son NOM dit deja son agencement.
							if (pl.pose || enfantsAbsolus) {
								NkRect r = pl.pose ? pl.rect : ctx.layout.region;
								if (!pl.pose) {
									// Meme raison que pour `Window`/`Panel` : un groupe absolu pose
									// dans un flux part du curseur, jamais du bord de la region.
									r.w -= (ctx.layout.cursor.x - r.x);
									r.h -= (ctx.layout.cursor.y - r.y);
									r.x = ctx.layout.cursor.x;
									r.y = ctx.layout.cursor.y;
								}
								const NkGuiLayout sauve = ctx.layout;
								ctx.BeginLayout(r);
								MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, enfantsAbsolus, hooks);
								ctx.layout = sauve;
								Noter(rap, id, t, r, prof, true, horizontal);
								++rap.montes;
								return;
							}
							const NkVec2 c0 = ctx.layout.cursor;
							BeginGroup(ctx);
							MonterCorps(ctx, w, etat, rap, prof + 1u, horizontal, false, hooks);
							EndGroup(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal);
							++rap.montes;
							return;
						}
						case NkGuiRole::VBox: {
							const float32 gap = NkGA(w, "gap") ? NkGNombre(w, "gap", -1.f) : -1.f;
							// ⚠️ UNE BOITE SE POSE, MAIS NE DECLARE PAS `placement`. Son NOM dit
							//    deja son agencement, et le validateur refuse `placement` sur elle
							//    (propriete hors schema). Elle se pose quand SON parent est absolu,
							//    et ses enfants a elle restent en FLUX : c'est exactement la toolbox
							//    de Rodolf -- posee dans une fenetre, boutons alignes tout seuls.
							if (pl.pose) {
								const NkGuiLayout sauve = ctx.layout;
								ctx.BeginLayout(pl.rect);
								BeginVBox(ctx, gap);
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
								EndVBox(ctx);
								ctx.layout = sauve;
								Noter(rap, id, t, pl.rect, prof, true, horizontal);
								++rap.montes;
								return;
							}
							const NkVec2 c0 = ctx.layout.cursor;
							BeginVBox(ctx, gap);
							MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
							EndVBox(ctx);
							Noter(rap, id, t, BlocConsomme(ctx, c0), prof, true, horizontal);
							++rap.montes;
							return;
						}
						case NkGuiRole::HBox: {
							const float32 gap = NkGA(w, "gap") ? NkGNombre(w, "gap", -1.f) : -1.f;
							// ⚠️ UNE BOITE SE POSE, MAIS NE DECLARE PAS `placement`. Son NOM dit
							//    deja son agencement, et le validateur refuse `placement` sur elle
							//    (propriete hors schema). Elle se pose quand SON parent est absolu,
							//    et ses enfants a elle restent en FLUX : c'est exactement la toolbox
							//    de Rodolf -- posee dans une fenetre, boutons alignes tout seuls.
							if (pl.pose) {
								const NkGuiLayout sauve = ctx.layout;
								ctx.BeginLayout(pl.rect);
								BeginHBox(ctx, gap);
								MonterCorps(ctx, w, etat, rap, prof + 1u, true, false, hooks);
								EndHBox(ctx);
								ctx.layout = sauve;
								Noter(rap, id, t, pl.rect, prof, true, horizontal);
								++rap.montes;
								return;
							}
							const NkVec2 c0 = ctx.layout.cursor;
							BeginHBox(ctx, gap);
							MonterCorps(ctx, w, etat, rap, prof + 1u, true, false, hooks);
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
								valeurMontee = e->f;
								aValeurMontee = true;
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
								champVideMonte = (e->texte[0] == '\0');
								aValeurMontee = false;
								// ⚠️ L'INVITE EST PEINTE, DANS LA COULEUR DU TEXTE
								//    GRISE. La premiere version la copiait dans le
								//    tampon -- l'image montrait « Filtrer... » comme
								//    si quelqu'un l'avait tape, ce qui est le plus
								//    trompeur des trois defauts : rien ne distinguait
								//    une invite d'une valeur reelle. La deuxieme ne la
								//    peignait plus du tout, et perdait ce que le
								//    fichier ecrit. Elle se peint donc PAR-DESSUS le
								//    champ vide, avec `theme.textDisabled` -- la
								//    couleur qui dit « ceci n'est pas votre saisie ».
								//    `InputText` n'ayant pas de placeholder, c'est le
								//    monteur qui le pose ; le jour ou NKGui en portera
								//    un, ces lignes s'en vont.
								if (champVideMonte && ctx.font && ctx.font->Valid()) {
									const NkString ph = NkGTexte(w, "placeholder", "");
									if (ph.Size() > 0) {
										// Le champ occupe la rangee MOINS le libelle,
										// exactement comme `InputTextEx` le calcule.
										const NkRect rang = ctx.layout.prevItem;
										const float32 largeurLbl =
											(lbl && LabelEnd(lbl) != lbl)
												? ctx.font->MeasureWidth(lbl, LabelEnd(lbl)) + 14.f
												: 0.f;
										const float32 lh = ctx.font->LineHeight();
										const NkVec2 coin{rang.x + 6.f,
														  rang.y + (rang.h - lh) * 0.5f};
										(void)largeurLbl;
										(void)TextAt(ctx, coin, ph.CStr(), ctx.theme.textDisabled);
									}
								}
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
						// ── LE SEPARATEUR : il ne se dessine pas, il PARTAGE ─
						// ⚠️ CE QUI FAIT UN SEPARATEUR N'EST PAS SON TRAIT. Un trait gris de
						//    quatre pixels se dessine sans rien separer. C'est qu'il DEPLACE
						//    la frontiere et que les DEUX voisins se recalculent -- et c'est
						//    pour cela qu'il est un CONTENEUR DE DEUX ENFANTS ici : ses
						//    voisins sont ses enfants, donc « les deux se recalculent » se
						//    mesure sans aller fouiller la fratrie, mecanisme que ce monteur
						//    n'a nulle part ailleurs.
						//
						// ⚠️ LE DOCUMENT POSE LE DEFAUT, LE GESTE VIT A COTE, et le depot
						//    avait deja le mecanisme : `NkGuiMonteEtat::Entree::initialise`
						//    -- « sans lui, chaque trame ecraserait ce que l'utilisateur a
						//    change ». Le ratio tire par l'utilisateur vit dans l'ETAT du
						//    montage, pour la duree de la session ; **rien n'est reecrit dans
						//    le `.nkgui`**, et c'est ce qui garde le fichier partageable.
						//    Le ratio plutot que des pixels : `SplitterRatio` le dit
						//    lui-meme, « un ratio survit au redimensionnement de la fenetre ».
						case NkGuiRole::Splitter: {
							// MUTATION DE BANC, NK_SEPARATEUR_MUTATION=fige : le ratio de
							// l'etat est ignore et le defaut du document repris a chaque
							// image. Le geste n'a alors plus d'effet -- c'est ce que le
							// critere du deplacement doit voir.
							static const bool kFige = []() {
								const char *v = getenv("NK_SEPARATEUR_MUTATION");
								return v && v[0] == 'f';
							}();
							const float32 defaut = NkGNombre(w, "ratio", 0.5f);
							const float32 rmin = NkGNombre(w, "min", 0.1f);
							const float32 rmax = NkGNombre(w, "max", 0.9f);
							const bool vertical = !NkGMotEgal(NkStringView(NkGTexte(w, "orientation", "Vertical").CStr()),
															  "Horizontal");
							if (e && !e->initialise) {
								e->f = defaut;
								e->initialise = true;
							}
							float32 ratio = (e && !kFige) ? e->f : defaut;
							if (ratio < rmin)
								ratio = rmin;
							if (ratio > rmax)
								ratio = rmax;
							const NkRect zone = pl.pose ? pl.rect : ctx.layout.region;
							const float32 epaisseur = 4.f;
							NkRect visuel{}, prise{};
							SplitterRects(zone, vertical, ratio, epaisseur, 12.f, &visuel, &prise);
							// La poignee, et le geste : `SplitterRatio` ecrit dans `ratio`.
							(void)SplitterRatio(ctx, lbl, zone, vertical, &ratio, rmin, rmax, epaisseur, 12.f);
							if (e && !kFige)
								e->f = ratio; // LE GESTE, garde dans l'etat -- jamais dans le document
							// Les deux enfants, chacun dans SA moitie. C'est ici que « les
							// deux voisins se recalculent » se produit.
							NkRect a = zone, b = zone;
							if (vertical) {
								a.w = visuel.x - zone.x;
								b.x = visuel.x + epaisseur;
								b.w = (zone.x + zone.w) - b.x;
							} else {
								a.h = visuel.y - zone.y;
								b.y = visuel.y + epaisseur;
								b.h = (zone.y + zone.h) - b.y;
							}
							MonterCorpsBorne(ctx, w, etat, rap, prof + 1u, hooks, a, b);
							Noter(rap, id, t, zone, prof, true, horizontal);
							++rap.montes;
							return;
						}
						// ── L'EN-TETE REPLIABLE (l'inspecteur du modeleur) ───
						// ⚠️ CE QUI FAIT UN PLIABLE N'EST PAS SON TRIANGLE, C'EST CE QU'IL
						//    CACHE. Un bloc replie ne monte PAS ses enfants : c'est le seul
						//    critere qui distingue un pliable d'un simple titre, et c'est
						//    celui que le banc mesure (l'enfant absent du releve).
						// ⚠️ `expanded` VIENT DU DOCUMENT, et il faut le poser AVANT d'appeler
						//    `CollapsingHeader` : le widget garde son etat par identifiant, et
						//    sans cette ligne le document ne commanderait rien -- la propriete
						//    serait un decor, exactement ce que ce fichier reproche ailleurs.
						case NkGuiRole::Expander: {
							// MUTATION DE BANC, NK_PLIABLE_MUTATION=toujours : tout est ouvert.
							// Elle prouve que le critere de l'enfant cache teste quelque chose.
							static const bool kToujours = []() {
								const char *v = getenv("NK_PLIABLE_MUTATION");
								return v && v[0] == 't';
							}();
							const NkString titre = NkGTexte(w, "label", id.CStr());
							const bool voulu = kToujours || NkGBooleen(w, "expanded", false);
							ctx.SetNodeOpen(ctx.GetId(titre.CStr()), voulu);
							if (CollapsingHeader(ctx, titre.CStr()))
								MonterCorps(ctx, w, etat, rap, prof + 1u, false, false, hooks);
							break;
						}
						// ── LA BANDE D'ONGLETS ───────────────────────────────
						// ⚠️ LE FORMAT DIT `tabs`, PAS `items`. Le schema du role
						//    (`NkGuiValidate.h`) porte `tabs, bind, editable, closable` :
						//    un document qui ecrirait `items` se fait refuser, et c'est le
						//    negatif de ce lot.
						// ⚠️ ET ON N'ECRIT PAS DE WIDGET : `TabBar` existe dans NKGui depuis
						//    toujours. Le monteur appelle ce qui est la, comme pour les
						//    quatorze autres roles.
						case NkGuiRole::TabBar: {
							// MUTATION DE BANC, NK_ONGLETS_MUTATION=vide : la liste est
							// ignoree. Elle prouve que le critere de l'image teste quelque
							// chose ; sans la variable, rien ne change.
							static const bool kVide = []() {
								const char *v = getenv("NK_ONGLETS_MUTATION");
								return v && v[0] == 'v';
							}();
							NkVector<NkString> libelles;
							const uint32 n = kVide ? 0u : NkGListeChaines(w, "tabs", libelles);
							if (n == 0u) {
								// Une bande sans onglet ne peint rien : il n'y a rien a
								// selectionner, donc rien a souligner. On le COMPTE.
								aDessine = false;
								break;
							}
							// `TabBar` attend un tableau de `const char *` : on le construit
							// ici, sur la pile, borne par le plafond du kit.
							const char *ptr[32];
							uint32 m = n > 32u ? 32u : n;
							for (uint32 k = 0; k < m; ++k)
								ptr[k] = libelles[k].CStr();
							(void)TabBar(ctx, lbl, ptr, (int32)m);
							break;
						}
						// ═══════════════════════════════════════════════════════════
						//  LA ZONE HOTE -- le document dit OU, l'application peint QUOI
						// ═══════════════════════════════════════════════════════════
						//  C'est la frontiere du format, et elle est ici. Un viseur 3D, une
						//  toile, un editeur de texte ne sont pas des widgets : ce sont des
						//  RECTANGLES que l'hote remplit. Sans ce role, aucun document ne
						//  pourra jamais decrire une application reelle.
						//
						//  ⚠️ UNE ZONE QUE PERSONNE NE REMPLIT NE DISPARAIT PAS EN SILENCE,
						//     ET NE PEINT PAS DE FAUX CONTENU. Elle se signale : des hachures
						//     et son nom -- ce qu'aucun contenu plausible ne ressemble -- et
						//     elle se COMPTE (`hotesNonRemplis`). Un manque muet se fait
						//     prendre pour un fond ; un manque qui se voit se repare.
						case NkGuiRole::Host: {
							++rap.hotes;
							// Un `pos`/`size` pose a deja arme `SetNextItemRect` avant le switch :
							// `NextItemRect` le rend tel quel. Sinon, la zone lit sa TAILLE RELATIVE
							// (« 60 % de mon parent ») ; a defaut, elle prend la largeur disponible et
							// quatre hauteurs d'item -- assez pour se voir.
							const NkGuiTailleRel relH = NkGuiLireTailleRelative(w, ctx.layout.region);
							const NkRect zone = ctx.NextItemRect(relH.aW ? relH.w : -1.f,
																 relH.aH ? relH.h : ctx.ItemHeight() * 4.f);
							const bool rempli = hooks && hooks->RemplirHote(ctx, id.CStr(), zone);
							if (!rempli) {
								++rap.hotesNonRemplis;
								// MUTATION DE BANC, NK_HOTE_MUTATION=sansmarqueur : la zone vide ne
								// trace plus rien. Elle sert a prouver que le critere du marqueur
								// teste quelque chose ; sans la variable, rien ne change.
								static const bool kSansMarqueur = []() {
									const char *v = getenv("NK_HOTE_MUTATION");
									return v && v[0] == 's';
								}();
								if (kSansMarqueur)
									break;
								NkGuiDrawList &dl = ctx.DL();
								// 🔴 LA COULEUR DU MARQUEUR EST CELLE DU TEXTE SECONDAIRE, PAS CELLE
								//    DE LA BORDURE. Premiere version : `theme.border`. Le banc
								//    comptait 3 778 pixels traces et passait au VERT -- et l'image
								//    ne montrait RIEN : la bordure est a 9 de luminance de l'aplat du
								//    panneau. Un marqueur qu'aucun oeil ne voit ne signale rien, et
								//    c'est exactement le defaut muet que ce role existe pour empecher.
								//    Le critere du banc compte desormais les pixels dont la LUMINANCE
								//    s'ecarte de l'aplat : la bordure y rougirait.
								const NkColor trait = ctx.theme.textMuted;
								dl.AddRect(zone, trait, 1.f);
								// LES HACHURES : elles disent « rien n'est monte ici », et aucun
								// contenu d'application ne leur ressemble.
								const float32 pas = 12.f;
								for (float32 d = 0.f; d < zone.w + zone.h; d += pas) {
									float32 x0 = zone.x + d, y0 = zone.y;
									float32 x1 = zone.x, y1 = zone.y + d;
									if (x0 > zone.x + zone.w) {
										y0 += x0 - (zone.x + zone.w);
										x0 = zone.x + zone.w;
									}
									if (y1 > zone.y + zone.h) {
										x1 += y1 - (zone.y + zone.h);
										y1 = zone.y + zone.h;
									}
									if (y0 <= zone.y + zone.h && x1 <= zone.x + zone.w)
										dl.AddLine({x0, y0}, {x1, y1}, trait, 1.f);
								}
								// ET SON NOM : « zone non remplie » sans dire LAQUELLE renverrait
								// l'hote a chercher. Le nom du noeud est la cle qu'il doit servir.
								if (ctx.font && ctx.font->Valid()) {
									const NkString h = NkGTexte(w, "hint", id.CStr());
									dl.AddText(ctx.font->Face(), ctx.font->TexId(),
											   {zone.x + 6.f, zone.y + 4.f}, h.CStr(), ctx.theme.textMuted);
								}
							}
							break;
						}
						default:
							aDessine = false;
							break;
					}

					// ⚠️ UN RECTANGLE POSE QUI N'A PAS ETE CONSOMME EST DESARME ICI.
					//    `SetNextItemRect` vaut pour le prochain widget AUTO-PLACE ; un role
					//    qui ne passe pas par `NextItemRect` le laisserait arme pour le
					//    SUIVANT, qui partirait aux coordonnees d'un autre. On le retire et on
					//    le COMPTE : un placement qui n'a pas pris doit se voir dans le releve,
					//    pas contaminer son voisin.
					if (pl.pose && !estConteneur) {
						if (ctx.nextItemRectSet) {
							ctx.nextItemRectSet = false;
							++rap.posesNonConsommes;
						} else {
							const NkRect &pv = ctx.layout.prevItem;
							const float32 dx = pv.x - pl.rect.x, dy = pv.y - pl.rect.y;
							if ((dx > -0.5f && dx < 0.5f) && (dy > -0.5f && dy < 0.5f))
								++rap.posesHonores;
						}
					}
					if (aDessine)
						++rap.montes;
					Noter(rap, id, t, ctx.layout.prevItem, prof, false, horizontal);
					if (rap.items.Size() > 0) {
						NkGuiMonteItem &dernier = rap.items[(uint32)rap.items.Size() - 1u];
						dernier.valeur = valeurMontee;
						dernier.aValeur = aValeurMontee;
						dernier.champVide = champVideMonte;
					}
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
