// =============================================================================
// main.cpp — NkAnimaEditor : éditeur d'animation (timeline) sur NKEditorKit.
// L'app ne touche QUE l'Editor Kit + AnimBridge (pas NKRenderer directement, pour
// éviter le conflit de types NKRenderer/NKCanvas). L'anim vit dans AnimBridge.cpp.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
#include "AnimBridge.h"
#include "Panels.h"
#include "NkCoquilleDocument.h"			   // L'INTERFACE VIENT D'UN DOCUMENT, PAS D'ICI
#include "NkEditorRHIRenderer.h"		   // UI sur NKRHI/NKRenderer (pas NKCanvas)
#include "NKGui/Core/NkGuiDrawListRaster.h" // la sonde, sans fenetre ni GPU
#include <cstdio>  // traces de la MESURE (canal chrome) : couleur et geometrie reelles
#include <cstdlib> // getenv : negatif et choix de theme, dans le MEME binaire

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkAnimaEditor";
	d.appVersion = "0.1.0";
	return d;
})());

static void CmdUndo(void *) {
	nkanima::AnimUndo();
}

static void CmdRedo(void *) {
	nkanima::AnimRedo();
}

static void CmdInsert(void *) {
	nkanima::AnimInsertKeyAtCursor();
}

static void CmdQuit(void *u) {
	if (u)
		static_cast<NkEditorShell *>(u)->RequestClose();
}

// ═══════════════════════════════════════════════════════════════════════════
//  CE QUI RESTE EN C++ : CE QU'UNE ACTION FAIT
// ═══════════════════════════════════════════════════════════════════════════
//  Le document dit QUELS boutons, OÙ, avec QUEL libellé, et sous QUEL NOM. Il
//  ne dit pas ce qu'ils font, et il ne le dira jamais : appeler
//  `AnimInsertKeyAtCursor()` est du code, pas de la donnée.
//
//  ⚠️ LA TABLE EST FERMÉE, ET UN NOM ABSENT NE SE TAIT PAS. Un bouton dont le
//     document nomme une action inconnue apparaît, se clique, et le compteur
//     `actionsInconnues` monte. C'est la même règle que la zone hôte.
static void ActJouer(void *) {
	nkanima::AnimSetPlaying(!nkanima::AnimIsPlaying());
}
static void ActInserer(void *) {
	nkanima::AnimInsertKeyAtCursor();
}
static void ActSupprimer(void *) {
	nkanima::AnimDeleteSelected();
}

// ── « Jouer en boucle » : une case du document, un effet réel ───────────────
//  ⚠️ APPELÉ À CHAQUE IMAGE, et c'est la limite (5) de `NkGuiInteraction.h` :
//     le format n'a pas encore d'événement, donc l'hôte passe tous les
//     `behavior` une fois par image. Les deux actions sont donc écrites
//     IDEMPOTENTES — une bascule ici serait un clignotant à 60 Hz.
static bool g_boucle = false;
static void ActBoucleActivee(void *) {
	g_boucle = true;
	const float32 d = nkanima::AnimDuration();
	if (nkanima::AnimIsPlaying() && d > 0.f && nkanima::AnimCursor() >= d - 0.001f)
		nkanima::AnimSeek(0.f);
}
static void ActBoucleCoupee(void *) {
	g_boucle = false;
}

static const nkanima::NkActionNommee kActions[] = {
	{"anim.jouer", &ActJouer, nullptr},
	{"anim.inserer", &ActInserer, nullptr},
	{"anim.supprimer", &ActSupprimer, nullptr},
	{"anim.annuler", &CmdUndo, nullptr},
	{"anim.refaire", &CmdRedo, nullptr},
	{"anim.boucle_activee", &ActBoucleActivee, nullptr},
	{"anim.boucle_coupee", &ActBoucleCoupee, nullptr},
};

// ── LA ZONE HÔTE : le document dit OÙ, l'application dit QUOI ───────────────
//  Une bande des clés de l'animation courante. Elle rend VRAI parce qu'elle
//  peint ; le jour où elle n'a rien à peindre, elle rend FAUX et le monteur
//  couvre la zone de hachures avec son nom. Répondre oui sans peindre ferait
//  disparaître la zone en silence.
static bool ZoneApercuCles(nkgui::NkGuiContext &ctx, const nkgui::NkRect &z, void *) {
	auto &dl = ctx.DL();
	dl.AddRectFilled(z, ctx.theme.track, ctx.theme.rounding);
	dl.AddRect(z, ctx.theme.border, 1.f, ctx.theme.rounding);
	const float32 dur = nkanima::AnimDuration();
	if (!nkanima::AnimLoaded() || dur <= 0.f) {
		if (ctx.font && ctx.font->Valid())
			dl.AddText(ctx.font->Face(), ctx.font->TexId(), {z.x + 6.f, z.y + 4.f},
					   "aucun clip chargé", ctx.theme.textDisabled);
		return true; // la zone est SERVIE : elle dit qu'il n'y a rien, ce n'est pas rien
	}
	NkVector<float32> temps;
	nkanima::AnimGetKeyTimes(temps);
	for (usize i = 0; i < temps.Size(); ++i) {
		const float32 t = temps[(uint32)i] / dur;
		const float32 x = z.x + 2.f + t * (z.w - 4.f);
		dl.AddRectFilled({x - 1.f, z.y + 3.f, 2.f, z.h - 6.f}, ctx.theme.accent, 0.f);
	}
	// Le curseur de lecture, en blanc : il bouge, donc la zone est VIVANTE.
	const float32 cx = z.x + 2.f + (nkanima::AnimCursor() / dur) * (z.w - 4.f);
	dl.AddRectFilled({cx - 1.f, z.y + 1.f, 2.f, z.h - 2.f}, nkgui::NkColor{255, 255, 255, 255}, 0.f);
	return true;
}

static const nkanima::NkZoneNommee kZones[] = {
	{"apercu_cles", &ZoneApercuCles, nullptr},
};

static nkanima::NkCoquilleDocument g_coquille;

// ═══════════════════════════════════════════════════════════════════════════
//  LA SONDE — elle prouve le fil SANS fenêtre, SANS GPU, SANS souris
// ═══════════════════════════════════════════════════════════════════════════
//  ⚠️ ELLE COMPTE DES PIXELS, PAS SEULEMENT DES WIDGETS. Ce dépôt a payé
//     « un compteur vert n'est pas un rendu juste » : un montage peut annoncer
//     douze widgets et ne rien peindre. Le critère porte donc sur les DEUX.
static int SondeCoquille(const char *dossier) {
	nkgui::NkGuiFont police;
	const bool policeOk = police.LoadEmbedded(NkEmbeddedFontId::DroidSans, 15.f, false);
	std::printf("=== SONDE COQUILLE-DOCUMENT (NkAnimaEditor) ===\n");
	std::printf("    dossier : %s\n", dossier);
	std::printf("    police embarquee : %s\n", policeOk ? "chargee" : "ABSENTE");

	if (!g_coquille.ChargerDepuisDossier(dossier))
		std::printf("    /!\\ %u document(s) REFUSE(S)\n", g_coquille.RefusTotal());
	g_coquille.PoserTables(kActions, (uint32)(sizeof(kActions) / sizeof(kActions[0])), kZones,
						   (uint32)(sizeof(kZones) / sizeof(kZones[0])));

	struct Bande {
			const char *nom;
			nkanima::NkBandeDocument *b;
			int32 w, h;
	};
	const Bande bandes[3] = {
		{"barre_outils", &g_coquille.barreOutils, 900, 34},
		{"barre_etat", &g_coquille.barreEtat, 900, 26},
		{"panneau_outils", &g_coquille.panneau, 260, 420},
	};

	int32 rouges = 0;
	for (uint32 i = 0; i < 3u; ++i) {
		const Bande &d = bandes[i];
		if (!d.b->lu) {
			std::printf("  [ KO ] %-16s REFUS NOMME : %s\n", d.nom, d.b->refus.CStr());
			++rouges;
			continue;
		}
		nkgui::NkGuiContext ctx;
		ctx.viewW = d.w;
		ctx.viewH = d.h;
		if (policeOk)
			ctx.font = &police;
		ctx.BeginFrame(0.016f);
		ctx.BeginLayout({0.f, 0.f, (float32)d.w, (float32)d.h});
		ctx.DL().Reset();
		d.b->Monter(ctx);

		// 🔴 PREMIERE VERSION : « pixels differents du fond efface ». Elle rendait
		//    30 600 sur une bande de 900x34, 23 400 sur 900x26, 109 200 sur
		//    260x420 — c'est-a-dire EXACTEMENT l'aire, a chaque fois. Le compteur
		//    ne comptait donc pas le CONTENU, il comptait l'APLAT que le premier
		//    rectangle de fond etale sur toute la bande. Un tel critere ne peut
		//    pas rougir : il serait vert pour un document qui ne monte qu'un fond.
		//    *Une preuve qui ne peut pas echouer ne prouve rien.*
		//
		//    On compte desormais ce qui S'ECARTE DE LA COULEUR DOMINANTE, qui est
		//    l'aplat lui-meme et se DERIVE de l'image au lieu d'etre ecrite en dur
		//    (l'aplat depend du theme : l'ecrire serait le perimer au premier
		//    changement de theme).
		uint32 contenu = 0;
		nkgui::NkGuiDrawListRaster ras;
		if (ras.Init(d.w, d.h)) {
			ras.Effacer(0xFF101010u);
			if (policeOk && police.pixels)
				ras.PoserTexture(police.TexId(), police.pixels, police.atlasW, police.atlasH, 1);
			ras.Rasteriser(ctx.DL());
			const uint8 *px = ras.Pixels();
			const usize n = (usize)ras.Largeur() * (usize)ras.Hauteur();
			// La dominante, par simple vote sur les triplets quantifies.
			uint32 meilleur = 0u, nbMeilleur = 0u;
			for (usize k = 0; k < n; ++k) {
				const uint32 c = ((uint32)px[k * 4u] << 16) | ((uint32)px[k * 4u + 1u] << 8)
								 | (uint32)px[k * 4u + 2u];
				uint32 compte = 0u;
				for (usize j = 0; j < n; j += 97u) { // echantillon : le vote n'a pas besoin de tout
					const uint32 d2 = ((uint32)px[j * 4u] << 16) | ((uint32)px[j * 4u + 1u] << 8)
									  | (uint32)px[j * 4u + 2u];
					if (d2 == c)
						++compte;
				}
				if (compte > nbMeilleur) {
					nbMeilleur = compte;
					meilleur = c;
				}
				if (k > 400u) // la dominante se decide sur le haut de l'image, pas sur tout
					break;
			}
			for (usize k = 0; k < n; ++k) {
				const uint32 c = ((uint32)px[k * 4u] << 16) | ((uint32)px[k * 4u + 1u] << 8)
								 | (uint32)px[k * 4u + 2u];
				if (c != meilleur)
					++contenu;
			}
		}
		// LE CRITERE, ECRIT AVANT LA MESURE : un document monte au moins un widget
		// ET peint du CONTENU sur au moins 1 % de sa bande, sans couvrir plus de
		// 95 % (ce qui voudrait dire qu'on a pris un aplat pour du contenu).
		const uint32 aire = (uint32)(d.w * d.h);
		const bool ok = d.b->rap.montes > 0u && contenu > aire / 100u && contenu < (aire * 95u) / 100u;
		std::printf("  [ %s ] %-16s widgets=%u montes=%u contenu=%u  inconnus=%u  hotes=%u/%u\n",
					ok ? "OK" : "KO", d.nom, d.b->rap.widgets, d.b->rap.montes, contenu,
					d.b->rap.rolesInconnus, d.b->zonesRemplies, d.b->rap.hotes);
		if (!ok)
			++rouges;
	}

	// ── LE BOUTON QUI AGIT, sans souris : on tire l'action par son NOM ──────
	//  ⚠️ CE N'EST PAS LE CHEMIN DE LA SOURIS, ET IL FAUT LE DIRE. La souris
	//     passe par `Apres()` (la derivation de l'appui) ; ici on entre par
	//     `Declencher`, la MEME porte que `Apres` emprunte ensuite. La sonde
	//     prouve donc la TABLE et l'EFFET, pas la detection du clic.
	const uint32 avant = g_coquille.panneau.actionsTirees;
	g_coquille.panneau.Declencher(NkStringView("anim.jouer"));
	const uint32 apres = g_coquille.panneau.actionsTirees;
	const bool actionOk = (apres == avant + 1u);
	std::printf("  [ %s ] action nommee    tirees %u -> %u\n", actionOk ? "OK" : "KO", avant, apres);
	if (!actionOk)
		++rouges;

	// ── LES DEUX CONFIGURATIONS RENDENT-ELLES LA MEME CHOSE ? ──────────
	//  Decision de Rodolf, 25/09 : le document se LIT en developpement et
	//  s'EMBARQUE a la livraison -- deux configurations du MEME document, jamais
	//  deux produits. Le coordinateur exige le banc qui le prouve.
	//
	//  🔴 PREMIERE VERSION, ET ELLE NE POUVAIT PAS REFUTER. Je comparais
	//     `ChargerDepuisFichier` a `Adopter` -- mais le premier APPELLE le second.
	//     Je comparais un chemin a lui-meme. Contre-epreuve faite : en retirant
	//     `Preparer` de `Adopter`, le panneau est tombe de 10 widgets montes a 9
	//     (la case a cocher a cesse de se peindre) et la ligne est restee VERTE.
	//     *Un negatif incapable de refuter ne refute rien.*
	//
	//  CE QU'ON COMPARE MAINTENANT, et les deux chemins sont VRAIMENT distincts :
	//     (a) le texte du disque -> arbre -> montage ;
	//     (b) ce meme arbre REEMIS EN TEXTE (`NkGuiArchive::Write`) -> relu ->
	//         arbre -> montage.
	//  (b) traverse l'ECRIVAIN, que (a) ne traverse pas -- et c'est precisement
	//  l'ecrivain dont l'embarquement se servira. Un document que l'ecrivain
	//  appauvrit fait diverger les deux, et la ligne rougit.
	{
		const uint32 mA = g_coquille.panneau.rap.montes;
		uint32 mB = 0u;
		bool relu = false;
		if (g_coquille.panneau.lu) {
			const NkGuiStyle style; // la mise en forme par defaut : ce qu'on compare
									// est le CONTENU monte, pas l'indentation
			const NkString texte = NkGuiArchive::Write(g_coquille.panneau.doc, style);
			NkArchive arbre2;
			NkGuiDiag err2;
			if (NkGuiArchive::Read(texte.CStr(), (uint32)texte.Size(), arbre2, err2)) {
				nkanima::NkBandeDocument parLEcrivain;
				parLEcrivain.actions = kActions;
				parLEcrivain.nbActions = (uint32)(sizeof(kActions) / sizeof(kActions[0]));
				parLEcrivain.zones = kZones;
				parLEcrivain.nbZones = (uint32)(sizeof(kZones) / sizeof(kZones[0]));
				parLEcrivain.Adopter(arbre2);
				nkgui::NkGuiContext ctx;
				ctx.viewW = 260;
				ctx.viewH = 420;
				if (policeOk)
					ctx.font = &police;
				ctx.BeginFrame(0.016f);
				ctx.BeginLayout({0.f, 0.f, 260.f, 420.f});
				ctx.DL().Reset();
				parLEcrivain.Monter(ctx);
				mB = parLEcrivain.rap.montes;
				relu = true;
			} else {
				std::printf("      reemission ILLISIBLE : %s\n", err2.message.CStr());
			}
		}
		// LE CRITERE : le meme document, monte depuis le disque et depuis sa
		// reemission, rend le MEME nombre de widgets -- et ce nombre n'est pas nul
		// (deux zeros seraient d'accord sans rien prouver).
		const bool memeResultat = relu && mA == mB && mA > 0u;
		std::printf("  [ %s ] disque == reemis  montes disque=%u  reemis=%u\n",
					memeResultat ? "OK" : "KO", mA, mB);
		if (!memeResultat)
			++rouges;
	}

	// ET LE NEGATIF : un nom que personne ne sert doit se COMPTER, pas se taire.
	const uint32 incAvant = g_coquille.panneau.actionsInconnues;
	g_coquille.panneau.Declencher(NkStringView("anim.nexiste_pas"));
	const bool negOk = (g_coquille.panneau.actionsInconnues == incAvant + 1u);
	std::printf("  [ %s ] nom non servi    inconnues %u -> %u\n", negOk ? "OK" : "KO", incAvant,
				g_coquille.panneau.actionsInconnues);
	if (!negOk)
		++rouges;

	std::printf("=== %s ===\n", rouges == 0 ? "TOUT PASSE" : "AU MOINS UN ECHEC");
	return rouges == 0 ? 0 : 1;
}

// Hook pré-UI : rend le viewport 3D dans l'offscreen (device partagé) AVANT la passe
// UI, puis publie sa texture au backend NKGui pour AddImage. user = NkEditorRHIRenderer*.
static void PreUI3D(NkICommandBuffer *cmd, void *user) {
	auto *r = static_cast<nkanima::NkEditorRHIRenderer *>(user);
	nkanima::Anim3DRenderOffscreen(cmd);
	nkanima::Anim3DRegisterInto(&r->GetBackend(), nkanima::ANIM_VIEWPORT_TEXID);
}

int nkmain(const NkEntryState &state) {
	// Backend graphique depuis les args (-bgl défaut, -bvk, -bdx11, -bdx12, -bsw).
	// Tout argument SANS tiret est le chemin du modèle à charger (défaut CesiumMan)
	// — c'est ce qui permet de pointer un autre rig (XBot Mixamo…) sans recompiler.
	NkEditorGfxApi gfx = NkEditorGfxApi::OpenGL;
	const char *modelPath = "Resources/Models/CesiumMan/CesiumMan.glb";
	// LE DOSSIER DES DOCUMENTS D'INTERFACE. Il se change en ligne de commande :
	// c'est par là qu'une VARIANTE arrivera (`--interface=.../Nogee`), et c'est
	// pour ça qu'il n'est pas écrit en dur ailleurs qu'ici.
	const char *dossierUI = "Resources/Interface/NkAnimaEditor";
	// ⚠️ ON SAUTE args[0] : C'EST L'IDENTITE DU PROGRAMME, PAS UN ARGUMENT.
	//    Sans ce saut, la clause « tout argument sans tiret est un chemin de
	//    modele » ci-dessous capturait le CHEMIN DE L'EXECUTABLE, et l'editeur
	//    tentait de charger son propre .exe comme fichier glTF. Le defaut
	//    CesiumMan n'etait donc JAMAIS atteint, et le viewport 3D restait vide
	//    a chaque lancement -- avec pour seule trace un [WRN] NkGLTFLoader et un
	//    [ERR] AnimBridge, qu'on pouvait prendre pour « pas encore implante ».
	//
	//    Mesure du 2026-09-14 : 21 sites du depot sautent ce premier element a la
	//    main, 7 y sont immunises parce qu'ils ne comparent qu'a des litteraux
	//    exacts, et CELUI-CI etait le seul a se tromper. `NkAudioPlayer` a le
	//    meme besoin -- un chemin de fichier libre -- et part de 1 lui aussi
	//    (main.cpp:105).
	const NkVector<NkString> &args = state.GetArgs();
	bool sonde = false;
	for (usize i = 1; i < args.Size(); ++i) {
		const NkString &a = args[i];
		if (a == "--sonde-coquille") {
			sonde = true;
			continue;
		}
		if (a.StartsWith("--interface=")) {
			dossierUI = a.CStr() + 12; // les args vivent dans state : durée de vie OK
			continue;
		}
		if (a == "-bvk" || a == "--backend=vulkan")
			gfx = NkEditorGfxApi::Vulkan;
		else if (a == "-bdx11" || a == "--backend=dx11")
			gfx = NkEditorGfxApi::DX11;
		else if (a == "-bdx12" || a == "--backend=dx12")
			gfx = NkEditorGfxApi::DX12;
		else if (a == "-bsw" || a == "--backend=sw")
			gfx = NkEditorGfxApi::Software;
		else if (a == "-bgl" || a == "--backend=opengl")
			gfx = NkEditorGfxApi::OpenGL;
		else if (!a.Empty() && a.Data()[0] != '-')
			modelPath = a.CStr(); // les args vivent dans state : durée de vie OK
	}

	// LA SONDE SORT AVANT TOUTE FENETRE : elle ne demande ni GPU ni souris.
	if (sonde)
		return SondeCoquille(dossierUI);

	auto shell = memory::NkMakeUnique<NkEditorShell>();
	// Backend de rendu NKRHI/NKRenderer injecte (PAS NKCanvas) : l'UI NKGui et le
	// viewport 3D partageront ce device. Doit survivre au shell.
	static nkanima::NkEditorRHIRenderer rhi;
	NkEditorShellConfig cfg;
	cfg.title = "NkAnimaEditor — timeline (NkAnima M1.c)";
	cfg.width = 1280;
	cfg.height = 720;
	cfg.graphicsApi = gfx; // choisi par -b<backend> (OpenGL par défaut)
	cfg.renderer = &rhi;
	if (!shell || !shell->Init(cfg))
		return -1;

	// ── Thème « Dark Pro » épuré (cf interface.md §1 : fond #1a1a1a, accent cyan) ──
	{
		nkgui::NkGuiTheme t;
		t.bgPrimary = {26, 26, 28, 255}; // #1a1a1c quasi-noir
		t.panel = {30, 31, 34, 255};
		t.header = {34, 35, 39, 255};
		t.button = {40, 42, 47, 255};
		t.buttonHover = {52, 56, 64, 255};
		t.buttonActive = {0, 168, 208, 255}; // cyan profond
		t.border = {46, 48, 55, 255};
		t.text = {222, 226, 232, 255};
		t.textDisabled = {110, 114, 124, 255};
		t.selection = {0, 150, 190, 200};
		t.accent = {0, 212, 255, 255}; // #00d4ff cyan
		t.track = {24, 25, 28, 255};
		t.tabBar = {20, 20, 22, 255};
		t.tab = {28, 29, 32, 255};
		t.tabHover = {40, 42, 47, 255};
		t.tabActive = {34, 36, 41, 255};
		t.rounding = 4.f;
		t.framePadX = 12.f;
		t.framePadY = 7.f;
		shell->Ui().theme = t;
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  L'HABILLAGE DE LA COQUILLE — ON APPELLE, ON NE REDESSINE PAS
	// ═══════════════════════════════════════════════════════════════════════
	//  Demande de Rodolf : NkAnimaEditor doit porter « exactement la meme
	//  interface » que NK3DModeler. Mesure prealable (canal chrome, lot 1) :
	//  cette application montait la coquille NUE -- AddPanel x2,
	//  RegisterCommand x4, et AUCUN des ~30 points d'extension de chrome.
	//
	//  LA COTE VIENT DE LA SOURCE, PAS D'UNE AUTRE APPLICATION :
	//  `NkModelerUI.h:113`, `NkLayout::Compute` -> `menuH = S(30.f)`
	//  (et `toolH = S(34.f)` en ligne 119).
	//
	//  ⚠️ LE SECOND PARAMETRE EST INERTE ICI, ET IL FAUT LE DIRE. La coquille
	//     ne reserve la bande d'outils que si l'application pose un
	//     `SetToolbar` : `toolbarH = (mToolbarFn && !fullScreen) ? bandH : 0.f`
	//     (NkEditorShell.cpp:831). NkAnimaEditor n'en pose pas. On passe donc
	//     34 pour rester fidele a la maquette le jour ou une barre d'outils
	//     arrivera -- mais aujourd'hui ce 34 ne peint rien, et l'ecrire sans le
	//     dire serait « declarer sans livrer ».
	//
	//  ⚠️ `NKANIMA_SANS_HABILLAGE=1` saute les deux appels : c'est LE NEGATIF,
	//     et il vit dans le MEME binaire. Comparer deux constructions aurait
	//     compare deux binaires differant peut-etre par autre chose.
	const bool sansHabillage = std::getenv("NKANIMA_SANS_HABILLAGE") != nullptr;
	if (!sansHabillage)
		shell->SetHeaderLayout(30.f, 34.f, 0.f);

	// ── LE THEME : DEUX CANDIDATS, ET C'EST RODOLF QUI TRANCHE ──────────────
	//  Le bloc ci-dessus pose un theme EN DUR (accent cyan #00d4ff, cf.
	//  interface.md §1). Il n'est PAS supprime : un theme en dur qu'on retire
	//  sans le dire, c'est une apparence qui change sans que personne sache
	//  pourquoi.
	//
	//  `ApplyTheme` est le point de synchronisation des deux objets theme
	//  (roles editeur -> jetons de dessin). NK3DModeler part de
	//  `NkTheme::Dark()` (NkModelerTheme.h:111) ; « exactement la meme
	//  interface » exige donc la meme source. Applique APRES le bloc en dur, il
	//  le remplace.
	//
	//  `NKANIMA_THEME=dur` rend le comportement d'avant, pour que les deux
	//  soient comparables cote a cote.
	{
		const char *th = std::getenv("NKANIMA_THEME");
		const bool garderDur = th && (th[0] == 'd' || th[0] == 'D');
		const bool clair = th && (th[0] == 'l' || th[0] == 'L');
		const nkgui::NkColor av = shell->Ui().theme.header;
		if (!sansHabillage && !garderDur)
			shell->ApplyTheme(clair ? NkTheme::Light() : NkTheme::Dark());
		const nkgui::NkColor ap = shell->Ui().theme.header;
		std::printf("[CHROME] theme=%s  header avant=(%d,%d,%d)  apres=(%d,%d,%d)  "
					"framePadY=%.1f\n",
					sansHabillage ? "SANS-HABILLAGE" : (garderDur ? "en dur" : (clair ? "Light" : "Dark")),
					(int)av.r, (int)av.g, (int)av.b, (int)ap.r, (int)ap.g, (int)ap.b,
					(double)shell->Ui().theme.framePadY);
		std::printf("[CHROME] ItemHeight=%.2f  scale=%.4f\n", (double)shell->Ui().ItemHeight(),
					(double)shell->Ui().scale);
		std::fflush(stdout);
	}

	nkanima::AnimInit(modelPath);

	// Viewport 3D : partage le device NKRHI de l'éditeur + rend en début de frame.
	nkanima::Anim3DSetSharedDevice(rhi.GetDevice());
	rhi.SetPreUI(&PreUI3D, &rhi);

	// ═══════════════════════════════════════════════════════════════════════
	//  L'INTERFACE DEVIENT UNE DONNÉE — LE FIL, ET IL TIENT EN SIX LIGNES
	// ═══════════════════════════════════════════════════════════════════════
	//  Demande de Rodolf, 25/09 : « c'est pour ça que j'ai demandé de les
	//  reproduire avec NKGui grâce à NKUIDesign, qui peut déjà générer des
	//  fichiers d'interface partageables. »
	//
	//  Tout ce qui suit ne fait que BRANCHER des choses finies : l'écrivain
	//  (NKUIDesign), le lecteur (`NkGuiArchive`), le monteur (`NkGuiMonteur`,
	//  2 242 l.), l'exécution (`NkGuiInteraction`, 1 385 l.) et les crochets de
	//  bande de la coquille. Aucune de ces cinq pièces n'a été touchée.
	//
	//  ⚠️ `NKANIMA_SANS_DOCUMENT=1` EST LE NÉGATIF, ET IL VIT DANS LE MÊME
	//     BINAIRE. Sans lui, il aurait fallu comparer deux constructions — donc
	//     deux binaires pouvant différer par autre chose. Avec lui : la barre
	//     d'outils disparaît, le panneau « Outils » disparaît, la barre d'état
	//     redevient celle de la coquille. Si l'interface ne change PAS, c'est
	//     que ce qu'on regarde ne vient pas des documents.
	const bool sansDocument = std::getenv("NKANIMA_SANS_DOCUMENT") != nullptr;
	static nkanima::PanneauDocument panneauDoc(g_coquille.panneau);
	if (!sansDocument) {
		const bool chargee = g_coquille.ChargerDepuisDossier(dossierUI);
		g_coquille.PoserTables(kActions, (uint32)(sizeof(kActions) / sizeof(kActions[0])), kZones,
							   (uint32)(sizeof(kZones) / sizeof(kZones[0])));
		std::printf("[COQUILLE] documents : %s (%s)  refus=%u\n", dossierUI,
					chargee ? "charges" : "INCOMPLETS", g_coquille.RefusTotal());
		if (!g_coquille.barreOutils.lu)
			std::printf("[COQUILLE] barre d'outils REFUSEE : %s\n",
						g_coquille.barreOutils.refus.CStr());
		if (!g_coquille.barreEtat.lu)
			std::printf("[COQUILLE] barre d'etat REFUSEE : %s\n", g_coquille.barreEtat.refus.CStr());
		if (!g_coquille.panneau.lu)
			std::printf("[COQUILLE] panneau REFUSE : %s\n", g_coquille.panneau.refus.CStr());
		std::fflush(stdout);

		// ⚠️ ON POSE LA BANDE MÊME SI LE DOCUMENT EST REFUSÉ. C'est délibéré :
		//    une bande posée écrit son refus à l'écran (`PeindreRefus`), une
		//    bande non posée disparaît sans rien dire. Rodolf doit LIRE la
		//    panne, pas la deviner à une bande manquante.
		shell->SetToolbar(&nkanima::NkCoquilleDocument::MonterBarreOutils, &g_coquille);
		shell->SetStatusBarFn(&nkanima::NkCoquilleDocument::MonterBarreEtat, &g_coquille);
		shell->AddPanel(&panneauDoc);
	}

	static nkanima::PreviewPanel preview;
	static nkanima::TimelinePanel timeline;
	shell->AddPanel(&preview);
	shell->AddPanel(&timeline);

	shell->RegisterCommand("Edition: Inserer cle", &CmdInsert, nullptr, "I");
	shell->RegisterCommand("Edition: Annuler", &CmdUndo, nullptr, "Ctrl+Z");
	shell->RegisterCommand("Edition: Refaire", &CmdRedo, nullptr, "Ctrl+Y");
	shell->RegisterCommand("Application: Quitter", &CmdQuit, shell.Get(), "Ctrl+Q");

	return shell->Run();
}
