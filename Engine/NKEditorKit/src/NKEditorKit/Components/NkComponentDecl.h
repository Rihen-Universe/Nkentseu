#pragma once
// -----------------------------------------------------------------------------
// @File    NkComponentDecl.h
// @Brief   LA FORME DE DECLARATION d'un composant de la bibliotheque.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ ETAT : DEMONSTRATION DU DEVIS D'ARCHITECTURE (2026-08-18). Ce fichier n'est
//    inclus par AUCUNE application et ne figure dans AUCUNE cible de build. Il
//    existe pour que la forme proposee dans `ROADMAP.md` soit lisible et
//    VERIFIABLE, pas pour etre appelee. Rien ne l'appelle, rien ne casse.
//
// POURQUOI UNE DECLARATION, ET PAS SEULEMENT UNE FONCTION
//   Directive de Rodolf du 2026-08-18 (NKUIEditor) : « un editeur ne peut
//   composer que ce qui est DECRIT PAR DES DONNEES ». Un composant qui n'existe
//   qu'en C++ compile ne peut etre ni assemble, ni parametre, ni sauve par un
//   editeur d'interfaces. La declaration ci-dessous est le minimum qui rende un
//   composant DESCRIPTIBLE, et elle coute quelques dizaines de lignes par
//   composant AUJOURD'HUI contre la reecriture de sa surface de dessin APRES.
//
// ⚠️ CE QUI N'EST PAS TRANCHE ICI, ET NE DOIT PAS L'ETRE
//   Le FORMAT DE FICHIER (texte a la NkTheme, JSON, binaire) est une decision de
//   Rodolf. Ce fichier ne decrit que la STRUCTURE en memoire. Elle se serialise
//   dans n'importe lequel de ces formats -- c'est justement pourquoi on peut
//   l'ecrire avant que le format soit choisi.
//
// EN-TETE PUR, ZERO DEPENDANCE D'INTERFACE — meme regle que NkTheme.h et
//   NkShortcutTable.h. Test de conformite, et il est EXECUTABLE (voir ROADMAP,
//   section « le banc de neutralite ») : une compilation en -fsyntax-only avec
//   les seuls chemins NKCore/NKContainers. S'il cesse de passer, c'est qu'un
//   type d'interface s'est invite : c'est exactement le defaut que ce fichier
//   existe pour empecher.
//
// LE PATRON SUIVI EST DEJA DANS LE DEPOT, TROIS FOIS
//   1. `NkTheme.h`      — roles = ENUM (acces du code) + NOM (acces du fichier),
//                         plus un REGISTRE d'extension pour les roles propres a
//                         une application. La structure ci-dessous copie ce
//                         couple, deliberement.
//   2. `NkDirBrowser.h` — `NkDirBrowserState` : coeur neutre reutilisable, dont
//                         l'application DERIVE (NkOpenWsState dans NKCode).
//   3. `Nogee/Panels/Model/*.h` — modeles neutres dont le panneau herite.
//   Ce n'est donc pas une invention : c'est le motif que ce depot a deja ecrit
//   trois fois separement, nomme une bonne fois.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editorkit {

		// ── NATURE D'UN PARAMETRE ───────────────────────────────────────────────
		// APPEND-ONLY, meme raison que NkRole : la valeur est destinee a etre
		// ecrite dans un fichier de description, un insert au milieu ferait relire
		// la mauvaise nature a toutes les descriptions deja enregistrees.
		enum class NkParamKind : uint8 {
			Float = 0,
			Int,
			Bool,
			Enum,	   ///< choix parmi `enumNames`
			Text,	   ///< chaine libre (libelle, gabarit de format)
			RoleRef,   ///< REFERENCE a un role de theme — jamais une couleur
			MetricRef, ///< REFERENCE a une metrique de theme — jamais un nombre de pixels
			Count
		};

		// ── UN PARAMETRE ────────────────────────────────────────────────────────
		// Ce que l'editeur d'interfaces pourra exposer dans son inspecteur, et ce
		// que le code de dessin lit pour ses defauts. UNE SEULE SOURCE DE VERITE :
		// si le dessin relit `defVal` ici au lieu d'ecrire 96.f dans son corps, il
		// devient impossible que la description et le dessin divergent.
		struct NkParamDecl {
				const char *name = "";	///< cle STABLE, telle qu'elle apparaitra dans un fichier
				const char *label = ""; ///< libelle affichable (traduisible)
				NkParamKind kind = NkParamKind::Float;
				float32 defVal = 0.f; ///< defaut ; pour Bool : 0 ou 1 ; pour Enum : l'index
				float32 minVal = 0.f; ///< borne basse (Float/Int) — 0 si sans objet
				float32 maxVal = 0.f; ///< borne haute — maxVal <= minVal signifie « non borne »
				const char *const *enumNames = nullptr; ///< Enum seulement
				uint8 enumCount = 0;
		};

		// ── UNE VARIANTE ────────────────────────────────────────────────────────
		// Directive de Rodolf du 2026-08-18 : « on peut avoir plusieurs
		// representations de la meme chose, et chaque application utilise celle qui
		// lui plait ». La variante est donc DECLAREE, et le dessin la recoit en
		// PARAMETRE.
		//
		// ⚠️ LA REGLE QUI REND LA SEPARATION VRAIE : ajouter une variante ne doit
		//    dupliquer NI le modele NI la logique de selection. Si elle les
		//    duplique, la variante n'en est pas une -- c'est un second composant
		//    qui se fait passer pour une option, et la duplication qu'on cherche a
		//    supprimer vient de rentrer par la fenetre.
		struct NkVariantDecl {
				const char *name = ""; ///< cle stable : « grid », « dense_list », « columns »
				const char *label = "";
				const char *summary = ""; ///< a quoi elle sert, pour l'editeur et pour l'humain
		};

		// ── UN JETON DE THEME ───────────────────────────────────────────────────
		// Le composant ne nomme JAMAIS une couleur : il nomme un USAGE, et dit de
		// quel role du theme cet usage herite par defaut. L'application (ou
		// l'editeur) peut reaffecter le jeton a un autre role sans toucher au
		// dessin -- c'est la deuxieme exigence de Rodolf (« en changeant le
		// theme »), rendue verifiable.
		//
		// `defaultRole` est un NOM, pas une valeur d'enumeration, parce que le role
		// peut etre un role d'APPLICATION enregistre a l'execution
		// (`NkRoleRegistry::Register`, cf. NkTheme.h) : « nk3d.anneau_brosse » est
		// une cle legitime ici. La resolution passe par `NkResolveRole`.
		struct NkTokenDecl {
				const char *name = "";		  ///< usage : « card_bg », « active_outline »
				const char *defaultRole = ""; ///< role du theme dont il herite
				const char *purpose = "";	  ///< ce que ce jeton peint, en une ligne
		};

		// ── UNE METRIQUE ────────────────────────────────────────────────────────
		// LE MANQUE MESURE LE 18/08 : `NkTheme` porte 30 roles de COULEUR et
		// seulement 4 rayons ; `NkGuiTheme` porte 16 couleurs et 3 metriques. Ni
		// l'un ni l'autre n'a de jeton d'espacement, de hauteur de ligne ou
		// d'epaisseur de trait. Consequence chiffree : `NkModelerBrowser.h` porte
		// 249 litteraux flottants nus. Tant que les metriques ne sont pas des
		// jetons, « changer le theme » ne change que les couleurs -- la moitie de
		// l'exigence.
		struct NkMetricDecl {
				const char *name = ""; ///< « card_gap », « row_h », « stroke_w »
				float32 defVal = 0.f;  ///< en pixels LOGIQUES (avant l'echelle d'ecran)
				const char *purpose = "";
		};

		// ── UN POINT DE GREFFE ──────────────────────────────────────────────────
		// Troisieme exigence de Rodolf : « en y integrant d'autres graphiques ».
		// L'application greffe son propre dessin SANS MODIFIER le composant.
		//
		// Le point de greffe est DECLARE (nom + signature en clair) pour que
		// l'editeur d'interfaces sache qu'il existe et puisse y raccrocher quelque
		// chose. `signature` est du TEXTE : c'est une description lisible, pas un
		// type C++ -- le type vit dans la structure de crochets du composant, qui,
		// elle, peut connaitre le peintre.
		struct NkHookDecl {
				const char *name = "";		///< « card_overlay », « extra_column »
				const char *signature = ""; ///< « (user, rect carte, index) -> void »
				const char *purpose = "";
		};

		// ── LA DECLARATION D'UN COMPOSANT ───────────────────────────────────────
		// Tout est en `const char*` et en tableaux statiques : une declaration est
		// une CONSTANTE de compilation, elle ne s'alloue pas, elle ne se detruit
		// pas, et elle peut vivre en donnees en lecture seule. Le jour ou NKUIEditor
		// chargera des declarations depuis un fichier, il construira les memes
		// structures a la main -- la forme ne change pas, seule la provenance.
		struct NkComponentDecl {
				const char *name = "";	///< cle stable : « content_browser »
				const char *title = ""; ///< libelle affichable
				const char *summary = "";

				const NkParamDecl *params = nullptr;
				uint16 paramCount = 0;
				const NkVariantDecl *variants = nullptr;
				uint16 variantCount = 0;
				const NkTokenDecl *tokens = nullptr;
				uint16 tokenCount = 0;
				const NkMetricDecl *metrics = nullptr;
				uint16 metricCount = 0;
				const NkHookDecl *hooks = nullptr;
				uint16 hookCount = 0;

				static bool StrEq(const char *a, const char *b) {
					if (!a || !b)
						return false;
					for (; *a && *b; ++a, ++b)
						if (*a != *b)
							return false;
					return *a == *b;
				}

				const NkParamDecl *FindParam(const char *n) const {
					for (uint16 i = 0; i < paramCount; ++i)
						if (StrEq(params[i].name, n))
							return &params[i];
					return nullptr;
				}
				const NkTokenDecl *FindToken(const char *n) const {
					for (uint16 i = 0; i < tokenCount; ++i)
						if (StrEq(tokens[i].name, n))
							return &tokens[i];
					return nullptr;
				}
				const NkMetricDecl *FindMetric(const char *n) const {
					for (uint16 i = 0; i < metricCount; ++i)
						if (StrEq(metrics[i].name, n))
							return &metrics[i];
					return nullptr;
				}

				// C'EST CETTE FONCTION QUE LE DESSIN APPELLE, au lieu d'ecrire un
				// nombre : le litteral n'existe alors qu'a UN endroit, dans la
				// declaration, ou un editeur peut le lire et le changer.
				float32 Metric(const char *n, float32 fallback = 0.f) const {
					const NkMetricDecl *m = FindMetric(n);
					return m ? m->defVal : fallback;
				}
				float32 Param(const char *n, float32 fallback = 0.f) const {
					const NkParamDecl *p = FindParam(n);
					return p ? p->defVal : fallback;
				}
				int32 VariantIndex(const char *n) const {
					for (uint16 i = 0; i < variantCount; ++i)
						if (StrEq(variants[i].name, n))
							return (int32)i;
					return -1;
				}
		};

		// ── LE REGISTRE ─────────────────────────────────────────────────────────
		// Meme forme que `NkRoleRegistry` (NkTheme.h), et pour la meme raison :
		// l'editeur d'interfaces a besoin d'ENUMERER ce qui existe, ce qu'une liste
		// ecrite en dur dans son code ne lui donnerait pas.
		//
		// ⚠️ DECLARE ICI, DEFINI NULLE PART. Ce fichier est une demonstration de
		//    forme ; le definir supposerait de choisir un stockage et de l'ajouter
		//    a une cible de build, c'est-a-dire d'implementer. Ce n'est pas la
		//    commande de cette seance.
		class NkComponentRegistry {
			public:
				static void Register(const NkComponentDecl &d); ///< idempotent sur `name`
				static uint16 Count();
				static const NkComponentDecl *At(uint16 i);
				static const NkComponentDecl *Find(const char *name);
		};

	} // namespace editorkit
} // namespace nkentseu
