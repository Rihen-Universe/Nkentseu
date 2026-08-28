#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiIntrospect.h
// @Brief   Introspection NKGui — demander ce qu'il y a a l'ecran.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// LIRE. Pas modifier, pas agir : ces deux chantiers viennent apres, et rien ici
// ne prepare leur venue. Le critere d'acceptation complet, ecrit AVANT ce
// fichier, est dans Kernel/Runtime/NKGui/INTROSPECTION.md.
//
// POURQUOI CET INSTRUMENT EXISTE — trois pertes mesurees la meme semaine, sans
// aucune IA en jeu : trois etapes livrees sans que personne ne voie la fenetre,
// un menu contextuel cable et invisible un tour entier, un sous-menu jamais
// photographie ouvert. Le premier client est le BANC, pas un modele.
//
// ⚠️ POURQUOI UN ENREGISTREMENT ET PAS UNE REQUETE. Mesure du 2026-08-28 :
//    NkGuiContext::BeginFrame remet a zero hotId, idDepth, curPopupLevel,
//    winCount, containerDepth, overlayDepth et les deux listes de dessin.
//    **Aucun arbre de controles n'est retenu.** Le contexte garde de l'etat
//    INDEXE PAR ID (openNodes, tabBarSel, scrollVals, windowMeta, popupStack…)
//    mais ce sont des valeurs par cle, pas une hierarchie : on ne peut pas les
//    parcourir pour reconstituer un ecran, et elles ne disent rien d'un
//    controle sans etat persistant — un bouton, un libelle, une entree de menu.
//    Donc chaque widget DEPOSE sa note en se soumettant, et le releve se lit
//    apres EndFrame.
//
// ⚠️ CE QUI SEPARE CET INSTRUMENT DE `UiRects` (NKUIDesign), QU'IL REMPLACE.
//    UiRects publiait `identifiant = x y w h` — des rectangles SEULS — avec
//    trois defauts qui expliquent a eux seuls pourquoi le sous-menu des
//    backends n'a jamais ete releve :
//      1. l'inscription etait MANUELLE a chaque site d'appel : un controle que
//         personne n'avait pense a instrumenter etait absent du releve, comme
//         s'il n'existait pas. Ici, c'est le WIDGET qui note, une fois pour
//         toutes ses appelants.
//      2. ce qui n'etait pas visible n'etait PAS PUBLIE (garde
//         `layout.region.w < 4`). La garde avait une bonne raison — une cible
//         inatteignable ne doit pas passer pour prete — mais elle rendait
//         structurellement impossible « un controle cable mais invisible doit
//         se voir ». Ici, un rectangle degenere ou hors-vue est releve **et
//         MARQUE** (NK_GUI_ETAT_VIDE / NK_GUI_ETAT_HORS_VUE). Absent et
//         invisible cessent de se confondre.
//      3. ca vivait dans UNE application. Ici c'est le socle : NKEditorKit et
//         toutes les applications en heritent sans une ligne.
//
// ⚠️ SILENCIEUX PAR DEFAUT. Sans activation, aucun fichier, aucun journal,
//    aucune allocation par trame : un instrument qui parle tout le temps finit
//    desactive. Deux voies d'activation, aucune par defaut :
//      - NkGuiIntrospectActiver(ctx, true) depuis le code ;
//      - la variable d'environnement NK_GUI_INTROSPECT=1, lue une seule fois a
//        NkGuiContext::Init.
//
// ⚠️ AUCUNE FONCTION D'INJECTION D'ENTREE N'EST AJOUTEE ICI, et c'est un
//    choix. Un banc qui pilote la souris ecrit dans `ctx.input` — le meme
//    champ public que toute application remplit depuis ses evenements OS. La
//    frontiere lire/agir tient ; le chantier « agir » partira d'une page
//    blanche plutot que d'une demi-abstraction posee par avance.
// -----------------------------------------------------------------------------

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace nkgui {

		struct NkGuiContext; // le releve vit dans le contexte

		// Nature du controle releve. Volontairement GROSSIERE : elle doit
		// permettre de dire « j'attendais un menu, j'ai trouve un bouton », pas
		// de reconstituer l'arbre d'appel. Une nature de plus se paie en
		// instrumentation dans NkGuiWidgets.cpp — on n'en ajoute que sur besoin.
		enum class NkGuiNature : uint8 {
			Inconnu = 0,
			Fenetre,	///< Begin/End (fenetre flottante ou ancree)
			Panneau,	///< BeginPanel/BeginChild (zone titree)
			BarreMenus, ///< BeginMenuBar
			Menu,		///< BeginMenu (titre de barre OU sous-menu)
			EntreeMenu, ///< MenuItem
			Separateur,
			Bouton,
			Case,	 ///< Checkbox / tri-etat
			Texte,	 ///< Text (libelle non interactif)
			Element, ///< Selectable (ligne de liste, item d'arbre)
			Onglet,
			Champ, ///< saisie texte
			Reglage,
			Count
		};

		// Etats, en MASQUE : ils se cumulent (une entree peut etre grisee ET
		// cochee — c'est meme le cas interessant). Seuls des FAITS POSITIFS sont
		// poses : l'absence de NK_GUI_ETAT_GRISE veut dire « actif », et il n'y a
		// donc qu'une source de verite par etat.
		enum : uint16 {
			NK_GUI_ETAT_AUCUN = 0,
			NK_GUI_ETAT_GRISE = 1u << 0,	  ///< non interactif (exige par le critere C3)
			NK_GUI_ETAT_COCHE = 1u << 1,	  ///< coche visible (exige par le critere C3)
			NK_GUI_ETAT_SELECTION = 1u << 2,  ///< selectionne (liste, onglet)
			NK_GUI_ETAT_SURVOLE = 1u << 3,	  ///< sous le pointeur, occlusion resolue
			NK_GUI_ETAT_ENFONCE = 1u << 4,	  ///< maintenu
			NK_GUI_ETAT_OUVERT = 1u << 5,	  ///< menu deroule, noeud deplie — etat INTERMEDIAIRE (C4)
			NK_GUI_ETAT_REPLIE = 1u << 6,	  ///< a du contenu, ferme
			NK_GUI_ETAT_FOCALISE = 1u << 7,	  ///< a le focus clavier
			NK_GUI_ETAT_MIXTE = 1u << 8,	  ///< tri-etat indetermine
			NK_GUI_ETAT_ATTENTE = 1u << 9,	  ///< un apercu attend confirmation (C4)
			NK_GUI_ETAT_HORS_VUE = 1u << 10,  ///< pose hors de la vue -> invisible (C2)
			NK_GUI_ETAT_VIDE = 1u << 11,	  ///< rectangle degenere (w<=0 ou h<=0) (C2)
			NK_GUI_ETAT_COUNT = 12
		};

		// Une note = un controle soumis pendant la trame.
		// ⚠️ TAILLES FIXES, PAS DE NkString : une note se copie, se compare et
		//    ne provoque aucune allocation apres la premiere trame. NKGui est
		//    zero-STL et le reste.
		struct NkGuiNote {
				static constexpr int32 LibelleMax = 64;
				static constexpr int32 AnnexeMax = 32;

				NkGuiId id = NKGUI_ID_NONE;
				NkGuiNature nature = NkGuiNature::Inconnu;
				uint16 etats = NK_GUI_ETAT_AUCUN;
				int16 niveau = 0; ///< imbrication : -1 = couche principale, 0..N = niveau de popup
				NkRect rect = {0.f, 0.f, 0.f, 0.f};
				char libelle[LibelleMax] = {};
				char annexe[AnnexeMax] = {}; ///< raccourci, valeur affichee — vide si sans objet
		};

		// Le releve d'UNE trame. Vit dans NkGuiContext.
		struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiIntrospect {
				/// Plafond dur. ⚠️ IL SE DIT : `perdues` compte ce qui n'a pas
				/// tenu, et l'en-tete du releve l'annonce. Un releve tronque en
				/// silence serait pire qu'un releve absent — il aurait l'air
				/// complet.
				static constexpr int32 Max = 4096;

				bool actif = false;
				NkVector<NkGuiNote> notes;
				int32 perdues = 0; ///< notes refusees faute de place, cette trame
		};

		// ── Activation ────────────────────────────────────────────────────────
		NKENTSEU_NKGUI_API void NkGuiIntrospectActiver(NkGuiContext &ctx, bool actif) noexcept;
		NKENTSEU_NKGUI_API bool NkGuiIntrospectActif(const NkGuiContext &ctx) noexcept;

		// ── Depot d'une note (appele par les widgets) ─────────────────────────
		// Sans frais quand l'introspection est eteinte : un test de booleen.
		// `rect` est pris TEL QUEL — degenere ou hors-vue, il est note et
		// marque, jamais filtre (critere C2).
		NKENTSEU_NKGUI_API void NkGuiNoter(NkGuiContext &ctx, NkGuiNature nature, NkGuiId id, const char *libelle,
										   const NkRect &rect, uint16 etats, const char *annexe = nullptr) noexcept;

		// ── Lecture (apres EndFrame) ──────────────────────────────────────────
		NKENTSEU_NKGUI_API const NkGuiNote *NkGuiIntrospectNotes(const NkGuiContext &ctx, int32 &nombre) noexcept;

		/// Premiere note dont le libelle est exactement `libelle` (et, si
		/// `nature != Count`, de cette nature). nullptr si absente.
		/// ⚠️ RENDRE nullptr N'EST PAS « le controle est cache » : c'est « aucun
		///    controle de ce nom ne s'est soumis ». Un controle invisible, lui,
		///    EST dans le releve avec NK_GUI_ETAT_VIDE ou NK_GUI_ETAT_HORS_VUE.
		NKENTSEU_NKGUI_API const NkGuiNote *NkGuiIntrospectTrouver(const NkGuiContext &ctx, const char *libelle,
																   NkGuiNature nature = NkGuiNature::Count) noexcept;

		// ── Rendu texte ───────────────────────────────────────────────────────
		NKENTSEU_NKGUI_API const char *NkGuiNatureNom(NkGuiNature nature) noexcept;

		/// Ecrit les etats poses sous forme « coche,grise » dans `buf`.
		/// Aucun etat pose -> « normal ». Rend le nombre d'octets ecrits.
		NKENTSEU_NKGUI_API int32 NkGuiEtatsTexte(uint16 etats, char *buf, int32 capacite) noexcept;

		/// Ecrit le releve dans un fichier, une ligne par controle, precede d'un
		/// en-tete qui COMPTE. Rend false si l'introspection est eteinte ou si
		/// l'ecriture echoue.
		///
		/// ⚠️ ECRIT PAR fwrite, JAMAIS PAR UN FORMATEUR A MARQUEURS. Un libelle
		///    contenant une accolade — un extrait de code, une regle CSS, un
		///    JSON dans un champ — verrait son corps disparaitre en silence si
		///    ce releve passait par NkFormat. La porte du depot le dit, et un
		///    instrument de mesure est precisement le dernier endroit ou l'on
		///    peut se permettre de perdre du texte.
		///
		/// ⚠️ N'ECRIT QUE SI LE CONTENU A CHANGE depuis le dernier appel : une
		///    ecriture par trame saturerait le disque, et un fichier reecrit en
		///    permanence est illisible par le script qui le lit au meme moment.
		///    Passer `toujours = true` force l'ecriture (releve unique de banc).
		NKENTSEU_NKGUI_API bool NkGuiIntrospectEcrire(const NkGuiContext &ctx, const char *chemin,
													  bool toujours = false) noexcept;

	} // namespace nkgui
} // namespace nkentseu
