#pragma once
// -----------------------------------------------------------------------------
// @File    NkTheme.h
// @Brief   Systeme de themes : roles de couleur nommes, heritage, chargement.
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// A QUOI CA SERT, ET POURQUOI CE N'EST PAS QU'UNE LISTE DE COULEURS
//   Regle posee dans UI_SPEC 10bis, et c'est elle qui gouverne tout le fichier :
//   les couleurs PORTEUSES DE SENS vivent DANS le theme -- axes X/Y/Z, etats de
//   selection, types d'assets compris. Laissees en dur dans le code, le theme
//   clair serait illisible : un vert d'axe pense pour un fond #141414 disparait
//   sur un fond #F5F5F5, et personne ne s'en apercevrait avant la capture d'ecran.
//
// EN-TETE PUR, ZERO DEPENDANCE — meme raison que NkShortcutTable : le harnais
//   doit pouvoir le tester sans lier NKUI, NKCanvas ni NKFont. Les couleurs sont
//   donc des uint32 empaquetes 0xRRGGBBAA, pas des nkgui::NkColor.
//
// ROLE = ENUM + NOM, et les deux servent
//   L'ENUM est l'acces du code : indexation directe d'un tableau, verifiee a la
//   compilation, sans recherche de chaine a chaque trait dessine.
//   Le NOM est l'acces du FICHIER : un theme utilisateur est un fichier texte
//   qu'on ecrit a la main. C'est exactement le couple deja retenu pour les
//   modificateurs (NkModifierType serialise + NkModParam::name) et les raccourcis.
//   L'enum est donc APPEND-ONLY : on ajoute a la fin, jamais au milieu.
//
// LA FONCTION QUI COMPTE VRAIMENT : L'HERITAGE
//   Un theme utilisateur ne redefinit presque jamais 40 couleurs. Il en change
//   trois et garde le reste. Un chargeur naif laisserait les 37 autres a zero,
//   c'est-a-dire NOIR — et le theme paraitrait « charge » tout en etant
//   inutilisable. Load() part donc TOUJOURS d'une base complete et n'ecrase que
//   ce que le fichier mentionne.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editorkit {

		// Couleur empaquetee 0xRRGGBBAA.
		using NkThemeColor = uint32;

		// ── ROLES ───────────────────────────────────────────────────────────────
		// APPEND-ONLY : ajouter a la FIN. Un role insere au milieu decalerait tous
		// les suivants et un theme enregistre relirait les mauvaises couleurs.
		enum class NkRole : uint16 {
			// Structure — la hierarchie a TROIS niveaux de UI_SPEC 10bis.1. Trois
			// valeurs se lisent sans effort ; un quatrieme gris rendrait les ecarts
			// indistinguables.
			WindowBg = 0,  ///< #141414 — le plus sombre, il recule derriere tout
			PanelBg,	   ///< #212121 — le panneau se detache du vide
			PanelHeader,   ///< #2B2B2B — en-tetes, barres d'outils : ce qui structure se lit d'abord
			Border,
			InputBg,	   ///< fond des champs de saisie
			LabelCol,	   ///< colonne de libelles des lignes de propriete

			// Texte
			Text,
			TextMuted,
			TextOnAccent,

			// LES DEUX FAMILLES D'ACCENT, et c'est une regle, pas une nuance :
			// le BLEU dit l'etat de l'INTERFACE, l'AMBRE dit la selection 3D.
			AccentUi,	 ///< #1177D1
			AccentSel,	 ///< #F2980E — orange UNIQUE du produit (cf. 10bis.2)

			// Les trois etats de selection d'un element de maillage. Ils DOIVENT
			// etre dans le theme : en clair, un « presque noir » d'element non
			// selectionne devient invisible.
			ElemActive,	  ///< l'element ACTIF — blanc pur en sombre
			ElemSelected, ///< selectionne
			ElemIdle,	  ///< ni l'un ni l'autre

			// Axes. Dans le theme pour la meme raison.
			AxisX,
			AxisY,
			AxisZ,

			// Types d'assets du navigateur de projet
			TypeMesh,
			TypeAnim,
			TypeMat,
			TypeTex,

			// Editeur de noeuds. Les deux sarcelles ne different que de 1 a 3 points
			// par canal : elles sont affectees a des etats qui NE COEXISTENT JAMAIS
			// (repos / survol du MEME en-tete), sinon l'utilisateur croirait a une
			// seule couleur qui « bave ». Cf. UI_SPEC 10bis.3.
			NodeDataHeader,	   ///< #0A545E — en-tete de noeud de donnees, au repos
			NodeDataHeaderHot, ///< #095461 — le MEME, survole ou selectionne
			NodeActionHeader,  ///< noeud d'action (ambre)
			NodeBody,
			NodeWire,

			// Vue 3D
			ViewportTop,
			ViewportBottom,
			GridLine,

			Count
		};

		// Cle STABLE du role, telle qu'elle apparait dans un fichier de theme.
		const char *NkRoleName(NkRole r);
		// Role correspondant a une cle, ou Count si inconnue.
		NkRole NkRoleFromName(const char *name);

		// Arrondis. Banani les exporte, la specification s'y refere : ils font
		// partie du theme au meme titre que les couleurs.
		enum class NkRadius : uint8 { Sm = 0, Md, Lg, Xl, Count };

		static const uint16 NK_ROLE_INVALID = 0xFFFFu;

		// ── ROLES D'APPLICATION ─────────────────────────────────────────────────
		// LE GARDE-FOU N.1 DE NKGRAPH, APPLIQUE ICI. NK3DModeler aura besoin d'un
		// « anneau de brosse », NkAnima d'une « cle d'animation », Nogee d'autre
		// chose encore. Les mettre dans l'enumeration COMMUNE ferait trainer a
		// Nogee des roles de sculpt qui ne le concernent pas -- et l'enumeration
		// grossirait a chaque application.
		//
		// Meme reponse que pour les types de sockets de NKGraph : l'APPLICATION
		// ENREGISTRE ses roles au demarrage et recoit un identifiant. Les roles du
		// coeur restent indexes par enumeration (rapides, verifies a la
		// compilation) ; ceux des applications vivent dans une table d'extension.
		//
		// Consequence recherchee : un fichier de theme ecrit pour NK3DModeler se
		// charge SANS ERREUR dans Nogee -- ses roles inconnus sont comptes, pas
		// rejetes. Convention de nommage : « nk3d.anneau_brosse ».
		class NkRoleRegistry {
			public:
				// Idempotent : deux appels avec le meme nom rendent le meme
				// identifiant. Une application peut donc enregistrer ses roles sans
				// se demander si une autre l'a deja fait.
				static uint16 Register(const char *name);
				static uint16 Find(const char *name); ///< NK_ROLE_INVALID si absent
				static const char *Name(uint16 id);
				static uint16 Total(); ///< roles du coeur + extensions enregistrees
		};

		// Resout un nom vers un identifiant, coeur PUIS extensions.
		uint16 NkResolveRole(const char *name);

		// Une paire qui n'atteint pas le contraste exige pour son usage.
		struct NkThemeIssue {
				NkRole fg = NkRole::Count;
				NkRole bg = NkRole::Count;
				float32 ratio = 0.f;
				float32 required = 0.f;
				bool isText = false;
		};

		class NkTheme {
			public:
				NkTheme(); ///< construit le theme SOMBRE par defaut

				static NkTheme Dark();
				static NkTheme Light();

				NkThemeColor Get(NkRole r) const {
					return (uint16)r < (uint16)NkRole::Count ? mColors[(uint16)r] : 0xFF00FFFFu;
				}
				void Set(NkRole r, NkThemeColor c) {
					if ((uint16)r < (uint16)NkRole::Count)
						mColors[(uint16)r] = c;
				}

				// Acces par identifiant : coeur SI id < NkRole::Count, extension
				// au-dela. Pas d'ambiguite avec les surcharges ci-dessus, `NkRole`
				// etant une enumeration a portee (aucune conversion implicite).
				NkThemeColor Get(uint16 id) const;
				void Set(uint16 id, NkThemeColor c);
				float32 Radius(NkRadius k) const {
					return (uint8)k < (uint8)NkRadius::Count ? mRadius[(uint8)k] : 0.f;
				}

				const NkString &Name() const {
					return mName;
				}
				void SetName(const char *n) {
					mName = NkString(n ? n : "");
				}
				bool IsDark() const {
					return mDark;
				}

				// ── FICHIER ─────────────────────────────────────────────────────
				// Format texte, une ligne « role = #RRGGBB[AA] ». Un theme se lit,
				// se compare avec git diff et s'ecrit a la main -- c'est la condition
				// pour que l'utilisateur puisse creer le sien, qui est la demande.
				void Save(NkString &out) const;

				// CHARGE EN HERITANT : `*this` doit deja contenir une base complete
				// (Dark() ou Light()), et seules les lignes presentes l'ecrasent.
				// C'est ce qui permet a un theme de trois lignes de fonctionner.
				// outUnknown compte les roles non reconnus : un theme ecrit pour une
				// version plus recente doit se charger quand meme, en le SIGNALANT
				// plutot qu'en echouant.
				bool Load(const char *text, uint32 *outUnknown = nullptr, uint32 *outApplied = nullptr);

				// ── VALIDATION ──────────────────────────────────────────────────
				// L'utilisateur pouvant fabriquer ses themes, il en fabriquera un
				// illisible : mieux vaut le lui dire que le laisser decouvrir.
				//
				// LE SEUIL N'EST PAS UNIQUE, et le croire etait une erreur que la
				// premiere mesure a revelee. WCAG demande 4,5 pour du TEXTE, mais
				// seulement 3,0 pour un element GRAPHIQUE -- un contour de selection
				// de trois pixels n'a pas a se lire comme un paragraphe. Un seuil
				// unique a 4,5 condamnerait des paires parfaitement lisibles ; un
				// seuil unique a 3,0 laisserait passer du texte trop faible.
				// CE QUE LA VALIDATION NE COUVRE PAS, et il faut le savoir pour ne pas
				// s'y fier a tort : le rapport de contraste est une mesure de
				// LUMINANCE. Il dit si un trait se detache de son FOND ; il ne dit
				// rien de la capacite a distinguer DEUX MARQUES COLOREES l'une de
				// l'autre, qui est une affaire de TEINTE. Blanc et ambre sortent a
				// 2,27 alors que personne ne les confond. On ne verifie donc que ce
				// que la mesure sait juger.
				uint32 Validate(NkThemeIssue *outWorst = nullptr) const;

				// Rapport de contraste le plus faible, tous seuils confondus. Utile
				// comme chiffre brut ; c'est Validate() qui juge.
				float32 WorstContrast(NkRole *outA = nullptr, NkRole *outB = nullptr) const;

				static float32 Contrast(NkThemeColor a, NkThemeColor b);
				static NkThemeColor FromHex(const char *hex); ///< « #RRGGBB » ou « #RRGGBBAA »
				static void ToHex(NkThemeColor c, char out[10]);

			private:
				NkThemeColor mColors[(uint16)NkRole::Count] = {};
				// Roles d'application. Peut etre plus COURTE que le registre : une
				// application peut enregistrer un role apres qu'un theme a ete
				// construit. Get() rend alors le magenta de « role oublie » plutot
				// que de lire hors des bornes.
				NkVector<NkThemeColor> mExt;
				float32 mRadius[(uint8)NkRadius::Count] = {2.f, 3.f, 4.f, 8.f};
				NkString mName;
				bool mDark = true;
		};

		// ── BIBLIOTHEQUE ────────────────────────────────────────────────────────
		// « Choisir un theme parmi plusieurs ou creer le sien » suppose une LISTE,
		// pas deux fonctions ecrites en dur dans le code.
		//
		// ELLE NE LIT AUCUN FICHIER, et c'est deliberé : lire le disque exigerait
		// NKFileSystem, et cet en-tete perdrait sa propriete d'etre testable sans
		// rien lier. L'APPLICATION lit le texte -- elle sait ou sont ses dossiers,
		// livre puis surcharge utilisateur, exactement comme pour les icones -- et
		// le passe ici.
		class NkThemeLibrary {
			public:
				void AddBuiltins(); ///< Sombre et Clair

				// Ajoute un theme depuis un fichier texte. `baseDark` choisit la base
				// dont il HERITE : un theme de trois lignes doit hériter des 26
				// autres, et de la bonne famille. Renvoie l'index, ou -1.
				int32 AddFromText(const char *text, bool baseDark = true);

				uint32 Count() const {
					return (uint32)mThemes.Size();
				}
				const NkTheme &At(uint32 i) const {
					return mThemes[i];
				}
				int32 Find(const char *name) const;
				bool SetCurrent(const char *name);
				void SetCurrentIndex(uint32 i) {
					if (i < (uint32)mThemes.Size())
						mCurrent = i;
				}
				uint32 CurrentIndex() const {
					return mCurrent;
				}
				const NkTheme &Current() const {
					return mThemes[mCurrent];
				}

			private:
				NkVector<NkTheme> mThemes;
				uint32 mCurrent = 0;
		};

	} // namespace editorkit
} // namespace nkentseu

#include "NKEditorKit/NkTheme.inl"
