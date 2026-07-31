// =============================================================================
// main.cpp — Point d'entree de NK3DModeler.
//
// SQUELETTE : coquille NKEditorKit + disposition de l'ecran A (maquette Banani
// validee par Rihen), alimentee par le systeme de themes et par la table de
// raccourcis. Aucune vue 3D reelle a ce stade -- c'est voulu : on valide la
// disposition et la grammaire visuelle AVANT de brancher le contenu, sinon il
// faut tout redecouper une fois le contenu en place.
//
// CE QUI EST DEJA VRAI ICI, et qui n'est pas de la maquette :
//   * les six roles de couleur propres au produit sont ENREGISTRES et poses ;
//   * la table de raccourcis est REMPLIE, avec detection de conflits -- c'est
//     elle qui alimentera les quatre chemins d'acces de UI_SPEC 9bis (menus,
//     menu contextuel, palette de recherche, panneau T) sans qu'aucune liste de
//     commandes soit ecrite deux fois.
// =============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKPlatform/NkEnv.h"

#include "NK3DModeler/Shell/NkModelerTheme.h"
#include "NK3DModeler/Shell/Panels.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::editorkit;
using namespace nkentseu::nk3d;

namespace {

	// Les raccourcis de la modelisation, repris de Demo3D ou ils etaient EN DUR.
	// Une seule table : les menus, le menu contextuel et la palette de recherche
	// la liront tous les trois. Une liste ecrite deux fois finit toujours par
	// diverger, et c'est l'utilisateur qui le decouvre.
	void FillShortcuts(NkShortcutTable &t) {
		// -- contexte OBJET --
		t.Bind("objet.deplacer", "Deplacer", NkKey::NK_G, 0, NK_SCTX_OBJECT);
		t.Bind("objet.tourner", "Tourner", NkKey::NK_R, 0, NK_SCTX_OBJECT);
		t.Bind("objet.echelle", "Redimensionner", NkKey::NK_S, 0, NK_SCTX_OBJECT);
		t.Bind("objet.dupliquer", "Dupliquer", NkKey::NK_D, NK_SC_SHIFT, NK_SCTX_OBJECT);
		t.Bind("objet.supprimer", "Supprimer", NkKey::NK_X, 0, NK_SCTX_OBJECT);
		t.Bind("objet.mode_edition", "Mode edition", NkKey::NK_TAB, 0, NK_SCTX_OBJECT);

		// -- contexte EDITION --
		t.Bind("edit.extruder", "Extruder la region", NkKey::NK_E, 0, NK_SCTX_EDIT);
		t.Bind("edit.inserer", "Inserer une face", NkKey::NK_I, 0, NK_SCTX_EDIT);
		t.Bind("edit.biseauter", "Biseauter", NkKey::NK_B, NK_SC_CTRL, NK_SCTX_EDIT);
		t.Bind("edit.fusionner", "Fusionner", NkKey::NK_M, 0, NK_SCTX_EDIT);
		t.Bind("edit.subdiviser", "Subdiviser", NkKey::NK_W, 0, NK_SCTX_EDIT);
		t.Bind("edit.mode_objet", "Mode objet", NkKey::NK_TAB, 0, NK_SCTX_EDIT);

		// -- contexte GLOBAL --
		t.Bind("app.palette", "Rechercher une commande", NkKey::NK_F3, 0, NK_SCTX_GLOBAL);
		t.Bind("app.panneau_outils", "Afficher le panneau d'outils", NkKey::NK_T, 0, NK_SCTX_GLOBAL);
		t.Bind("app.annuler", "Annuler", NkKey::NK_Z, NK_SC_CTRL, NK_SCTX_GLOBAL);
		t.Bind("app.refaire", "Refaire", NkKey::NK_Y, NK_SC_CTRL, NK_SCTX_GLOBAL);
	}

} // namespace

int nkmain(const NkEntryState &entry) {
	(void)entry;

	NkModelerState state;

	// ── THEMES ──────────────────────────────────────────────────────────────
	// Les roles propres au produit AVANT tout chargement : ApplyDefaults les
	// pose sur chaque theme, et un fichier utilisateur garde le dernier mot.
	NkModelerRoles roles;
	roles.Register();

	NkString appThemes = NkString("data/themes");
	NkString userThemes;
	{
		// Meme convention que les icones : surcharge utilisateur d'abord dans
		// l'ordre de priorite, livre en repli.
		const char *appdata = env::GetEnvVar("APPDATA");
		if (appdata && *appdata) {
			userThemes = NkString(appdata);
			userThemes.Append("/NK3DModeler/themes");
		}
	}
	const uint32 fromDisk = LoadThemes(state.themes, roles, appThemes.CStr(), userThemes.CStr());
	state.themes.SetCurrent("Sombre");

	// Un theme illisible est fabricable par n'importe qui : on le SIGNALE au
	// demarrage plutot que de laisser l'utilisateur le decouvrir a l'usage.
	NkThemeIssue issue{};
	const uint32 defauts = state.themes.Current().Validate(&issue);
	if (defauts > 0) {
		printf("[theme] %u paire(s) sous le seuil de contraste. La pire : %s sur %s = %.2f (exige %.1f)\n",
			   defauts, NkRoleName(issue.fg), NkRoleName(issue.bg), (double)issue.ratio,
			   (double)issue.required);
	}

	// ── RACCOURCIS ──────────────────────────────────────────────────────────
	FillShortcuts(state.shortcuts);
	const uint32 conflits = state.shortcuts.ConflictCount();
	if (conflits > 0) {
		// On ne REFUSE pas une liaison en conflit -- l'utilisateur a le droit de
		// remplacer un raccourci. On le lui DIT, c'est tout.
		printf("[raccourcis] %u conflit(s) detecte(s).\n", conflits);
	}

	printf("[nk3d] %u themes (%u depuis le disque), %u raccourcis.\n", state.themes.Count(), fromDisk,
		   state.shortcuts.Count());

	// ── COQUILLE ────────────────────────────────────────────────────────────
	NkEditorShellConfig cfg;
	cfg.title = "NK3DModeler";
	cfg.width = 1600;
	cfg.height = 900;

	NkEditorShell shell;
	if (!shell.Init(cfg)) {
		printf("[nk3d] echec d'initialisation de la coquille.\n");
		return 1;
	}

	// L'ORDRE D'AJOUT PORTE LES ADJACENCES imposees en UI_SPEC 2 : hierarchie a
	// GAUCHE, proprietes AU-DESSUS des details a DROITE, navigateur EN BAS.
	HierarchyPanel hierarchy(state);
	ToolPanel tools(state);
	ViewportPanel viewport(state);
	PropertiesPanel properties(state);
	DetailsPanel details(state);
	ProjectBrowserPanel browser(state);

	shell.AddPanel(&hierarchy);
	shell.AddPanel(&tools);
	shell.AddPanel(&viewport);
	shell.AddPanel(&properties);
	shell.AddPanel(&details);
	shell.AddPanel(&browser);

	shell.Run();
	return 0;
}
