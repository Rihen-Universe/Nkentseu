// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    main.cpp
// @Brief   SONDE DE MESURE de NkWindowConfig sur Win32 — elle interroge le
//          SYSTEME, jamais nos variables.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QU'ELLE MESURE, ET POURQUOI ELLE NE PEUT PAS SE MENTIR
// =============================================================================
//  Un utilisateur a pose `resizable = false` et sa fenetre s'est redimensionnee
//  quand meme. Le dorsal Win32 posait `WS_OVERLAPPEDWINDOW` quoi qu'on demande,
//  et ce seul mot contient `WS_THICKFRAME`, `WS_MINIMIZEBOX` et
//  `WS_MAXIMIZEBOX`. Quinze champs de `NkWindowConfig` n'etaient jamais lus.
//
//  ⚠️ CETTE SONDE NE LIT AUCUN DE NOS CHAMPS POUR CONCLURE. Un banc qui
//     compare `mConfig.resizable` a `false` rendrait vert sur le code CASSE :
//     la variable a toujours ete juste, c'est la FENETRE qui ne la suivait pas.
//     Elle demande donc a Windows, et a lui seul :
//
//       GetWindowLongW(GWL_STYLE)        -> WS_THICKFRAME / MINIMIZEBOX / MAXIMIZEBOX
//       GetWindowLongW(GWL_EXSTYLE)      -> WS_EX_TOPMOST / TRANSPARENT / LAYERED / NOACTIVATE
//       GetMenuState(GetSystemMenu(...)) -> SC_CLOSE / SC_MOVE / SC_SIZE / SC_MINIMIZE / SC_MAXIMIZE
//       GetLayeredWindowAttributes       -> l'octet alpha reellement pose
//       SendMessageW(WM_GETMINMAXINFO)   -> les bornes que Windows appliquera
//
//  Le dernier point vaut d'etre dit : `WM_GETMINMAXINFO` est envoye A LA
//  FENETRE. La reponse qui revient est celle que notre `WndProc` a ecrite, puis
//  que `DefWindowProc` a bornee — c'est-a-dire ce que Windows fera vraiment, et
//  pas ce que nous avions eu l'intention de lui dire.
//
// =============================================================================
//  LE NEGATIF — `--ancien-style`
// =============================================================================
//  Un banc qui ne sait dire que « oui » ne mesure rien. Avec `--ancien-style`,
//  la sonde REMET sur chaque fenetre le style d'avant le correctif
//  (`WS_OVERLAPPEDWINDOW`, menu systeme reactive) et relance les MEMES criteres.
//  Ils doivent ROUGIR. S'ils restent verts, c'est la sonde qu'il faut reparer,
//  pas le dorsal.
//
//  ⚠️ Le negatif mute la FENETRE, pas le critere : si on mutait le critere, on
//     mesurerait le critere. Ici le sujet change et l'instrument ne bouge pas.
//
// =============================================================================
//  CE QU'ELLE NE FAIT PAS
// =============================================================================
//  Aucune fenetre VISIBLE : toutes sont creees `visible = false`. Rien
//  n'apparait sur l'ecran de Rodolf, rien ne prend le focus, aucune entree
//  n'est injectee, rien n'est capture. Les fenetres portent quand meme
//  `*** SONDE DE MESURE ***` dans leur titre : si l'une d'elles se montrait par
//  accident, elle dirait ce qu'elle est.
//
//  Elle ne mesure QUE Win32. Sur un autre dorsal elle le dit et rend 0 : un
//  banc qui pretendrait mesurer XLib sans savoir interroger X11 mentirait.
//
//  POINT D'EXTENSION : ajouter un essai = ajouter un `Critere(...)` dans
//  `Mesurer()`. Rien d'autre a toucher.
// =============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkWindowAudit.h" // NkWindowRefuserMethode : essai J

#include <cstdio>
#include <cstring>
#include <share.h> // _fsopen : lire un journal que le logger tient ouvert

using namespace nkentseu;

namespace {

	int gEssais = 0;
	int gEchecs = 0;
	bool gAttenduRouge = false; // vrai en mode negatif : un critere VERT devient l'echec

	// Un critere : ce qui etait demande, ce que le SYSTEME repond.
	//
	// ⚠️ L'ETIQUETTE DIT LE FAIT OBSERVE, pas seulement « ok ». En mode negatif
	//    un critere CONFORME est l'echec — et un lecteur presse qui verrait
	//    « ok » des deux cotes conclurait l'inverse de la mesure. On ecrit donc
	//    le FAIT observe a cote du verdict, qui dit ce que ce fait vaut DANS CE
	//    MODE.
	void Critere(const char *nom, bool conforme, const char *detail) {
		++gEssais;
		const bool echec = gAttenduRouge ? conforme : !conforme;
		if (echec)
			++gEchecs;
		const char *verdict;
		if (!gAttenduRouge)
			verdict = conforme ? "[  ok  ]" : "[ECHEC ]";
		else
			verdict = conforme ? "[ECHEC ]" : "[rougi ]";
		std::printf("  %s %-34s %-12s %s\n", verdict, nom, conforme ? "conforme" : "NON CONFORME",
					detail ? detail : "");
	}

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

	// L'entree du menu systeme est-elle GRISEE ? `GetMenuState` rend 0xFFFFFFFF
	// quand l'item n'existe pas — ce qui n'est PAS « grise ». On distingue les
	// deux : un item absent et un item actif ne se corrigent pas pareil.
	enum class EtatMenu { Absent, Actif, Grise };

	EtatMenu LireMenu(HWND hwnd, UINT item) {
		HMENU menu = GetSystemMenu(hwnd, FALSE);
		if (!menu)
			return EtatMenu::Absent;
		const UINT st = GetMenuState(menu, item, MF_BYCOMMAND);
		if (st == static_cast<UINT>(-1))
			return EtatMenu::Absent;
		return (st & (MF_GRAYED | MF_DISABLED)) ? EtatMenu::Grise : EtatMenu::Actif;
	}

	const char *NomEtat(EtatMenu e) {
		return e == EtatMenu::Absent ? "absent" : (e == EtatMenu::Grise ? "grise" : "ACTIF");
	}

	// Les bornes que Windows appliquera, demandees a la fenetre elle-meme.
	void LireBornes(HWND hwnd, POINT &mini, POINT &maxi) {
		MINMAXINFO mm{};
		// Valeurs d'entree de Windows : DefWindowProc les remplit d'abord. On les
		// initialise comme lui pour que le seul ecart vienne de notre WndProc.
		mm.ptMinTrackSize.x = GetSystemMetrics(SM_CXMINTRACK);
		mm.ptMinTrackSize.y = GetSystemMetrics(SM_CYMINTRACK);
		mm.ptMaxTrackSize.x = GetSystemMetrics(SM_CXMAXTRACK);
		mm.ptMaxTrackSize.y = GetSystemMetrics(SM_CYMAXTRACK);
		SendMessageW(hwnd, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&mm));
		mini = mm.ptMinTrackSize;
		maxi = mm.ptMaxTrackSize;
	}

	// LA MUTATION. Elle remet exactement ce que faisait le dorsal avant le
	// 25/09 : le style fourre-tout, et un menu systeme intact.
	void RemettreAncienStyle(HWND hwnd, bool frame) {
		const DWORD ancien = frame ? WS_OVERLAPPEDWINDOW
								   : (WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
									  WS_MAXIMIZEBOX);
		SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(ancien));
		SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		// `GetSystemMenu(hwnd, TRUE)` reconstruit le menu par defaut : tout
		// redevient actif, comme avant le correctif.
		GetSystemMenu(hwnd, TRUE);
	}

	NkWindowConfig ConfigDeBase(const char *nom) {
		NkWindowConfig c;
		// ⚠️ LE TITRE EST UNE GARDE, PAS UNE DECORATION : si cette fenetre se
		//    montrait par accident, elle dirait ce qu'elle est.
		c.title = NkString::Fmtf("*** SONDE DE MESURE *** %s", nom);
		c.name = "NkWindowSonde";
		c.width = 640;
		c.height = 480;
		c.visible = false; // RIEN ne s'affiche sur l'ecran de Rodolf
		c.centered = false;
		return c;
	}

	// -------------------------------------------------------------------------
	// Essai A — les cinq comportements a false
	// -------------------------------------------------------------------------
	void EssaiComportements() {
		NkWindowConfig c = ConfigDeBase("A-comportements");
		c.resizable = false;
		c.movable = false;
		c.closable = false;
		c.minimizable = false;
		c.maximizable = false;

		NkWindow w;
		if (!w.Create(c)) {
			std::printf("  [ECHEC] la fenetre A n'a pas pu etre creee\n");
			++gEssais;
			++gEchecs;
			return;
		}
		HWND hwnd = w.GetSurfaceDesc().hwnd;
		if (gAttenduRouge)
			RemettreAncienStyle(hwnd, c.frame);

		const DWORD st = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
		char buf[160];

		std::snprintf(buf, sizeof(buf), "GWL_STYLE=0x%08lX", static_cast<unsigned long>(st));
		Critere("resizable=false -> !WS_THICKFRAME", (st & WS_THICKFRAME) == 0, buf);
		Critere("maximizable=false -> !WS_MAXIMIZEBOX", (st & WS_MAXIMIZEBOX) == 0, buf);
		Critere("minimizable=false -> !WS_MINIMIZEBOX", (st & WS_MINIMIZEBOX) == 0, buf);

		const EtatMenu mClose = LireMenu(hwnd, SC_CLOSE);
		const EtatMenu mMove = LireMenu(hwnd, SC_MOVE);
		const EtatMenu mSize = LireMenu(hwnd, SC_SIZE);
		std::snprintf(buf, sizeof(buf), "SC_CLOSE=%s", NomEtat(mClose));
		Critere("closable=false -> SC_CLOSE grise", mClose == EtatMenu::Grise, buf);
		std::snprintf(buf, sizeof(buf), "SC_MOVE=%s", NomEtat(mMove));
		Critere("movable=false -> SC_MOVE grise", mMove == EtatMenu::Grise, buf);
		std::snprintf(buf, sizeof(buf), "SC_SIZE=%s", NomEtat(mSize));
		Critere("resizable=false -> SC_SIZE grise", mSize == EtatMenu::Grise, buf);

		w.Close();
	}

	// -------------------------------------------------------------------------
	// Essai B — les bornes de taille, en coordonnees FENETRE
	//
	// Le piege que ce critere attrape : remplir `ptMinTrackSize` avec les
	// valeurs CLIENT telles quelles. Un minimum client de 500x400 doit produire
	// un minimum fenetre STRICTEMENT plus grand (la barre de titre et les
	// bordures s'ajoutent). Si la sonde lit exactement 500x400, c'est que
	// `AdjustWindowRect` a ete oublie et que la fenetre descendra plus bas que
	// demande.
	// -------------------------------------------------------------------------
	void EssaiBornes() {
		NkWindowConfig c = ConfigDeBase("B-bornes");
		c.minWidth = 500;
		c.minHeight = 400;
		c.maxWidth = 900;
		c.maxHeight = 700;

		NkWindow w;
		if (!w.Create(c)) {
			std::printf("  [ECHEC] la fenetre B n'a pas pu etre creee\n");
			++gEssais;
			++gEchecs;
			return;
		}
		// ⚠️ PAS DE NEGATIF POUR CET ESSAI, ET IL FAUT LE DIRE. Les bornes sont
		//    calculees par NOTRE `WndProc`, qu'on ne peut pas remplacer depuis
		//    ici sans reecrire le dorsal. Muter le critere au lieu du sujet
		//    mesurerait le critere. `Mesurer()` saute donc cet essai en mode
		//    negatif plutot que de fabriquer une preuve. Ce qui le protege a la
		//    place : le second critere exige mini.x > 500 STRICTEMENT, ce que
		//    l'ancien code rendait faux par construction (il rendait l'egalite).
		HWND hwnd = w.GetSurfaceDesc().hwnd;

		// Ce que le cadre ajoute autour du client, pour CE style precis.
		const DWORD st = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
		const DWORD ex = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
		RECT ref = {0, 0, 500, 400};
		AdjustWindowRectEx(&ref, st, FALSE, ex);
		const LONG attenduMinW = ref.right - ref.left;
		const LONG attenduMinH = ref.bottom - ref.top;

		POINT mini{}, maxi{};
		LireBornes(hwnd, mini, maxi);

		char buf[200];
		std::snprintf(buf, sizeof(buf), "ptMinTrackSize=%ldx%ld, attendu %ldx%ld (client 500x400)", mini.x, mini.y,
					  attenduMinW, attenduMinH);
		Critere("minWidth/minHeight en coords FENETRE", mini.x == attenduMinW && mini.y == attenduMinH, buf);

		// Le critere qui distingue le correctif d'un hasard : la borne rendue
		// doit etre STRICTEMENT superieure au client demande. C'est faux dans
		// l'ancien code par construction (il rendait l'egalite).
		std::snprintf(buf, sizeof(buf), "%ld > 500 et %ld > 400 ?", mini.x, mini.y);
		Critere("le cadre est compte dans le minimum", mini.x > 500 && mini.y > 400, buf);

		RECT ref2 = {0, 0, 900, 700};
		AdjustWindowRectEx(&ref2, st, FALSE, ex);
		const LONG attenduMaxW = ref2.right - ref2.left;
		const LONG attenduMaxH = ref2.bottom - ref2.top;
		std::snprintf(buf, sizeof(buf), "ptMaxTrackSize=%ldx%ld, attendu %ldx%ld", maxi.x, maxi.y, attenduMaxW,
					  attenduMaxH);
		Critere("maxWidth/maxHeight appliques", maxi.x == attenduMaxW && maxi.y == attenduMaxH, buf);

		w.Close();
	}

	// -------------------------------------------------------------------------
	// Essai C — la fenetre discrete : styles etendus et alpha REELLEMENT pose
	// -------------------------------------------------------------------------
	void EssaiDiscrete() {
		NkWindowConfig c = ConfigDeBase("C-discrete");
		c.alwaysOnTop = true;
		c.clickThrough = true;
		c.noActivate = true;
		c.opacity = 0.5f;

		NkWindow w;
		if (!w.Create(c)) {
			std::printf("  [ECHEC] la fenetre C n'a pas pu etre creee\n");
			++gEssais;
			++gEchecs;
			return;
		}
		HWND hwnd = w.GetSurfaceDesc().hwnd;
		const DWORD ex = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
		char buf[160];
		std::snprintf(buf, sizeof(buf), "GWL_EXSTYLE=0x%08lX", static_cast<unsigned long>(ex));
		Critere("alwaysOnTop -> WS_EX_TOPMOST", (ex & WS_EX_TOPMOST) != 0, buf);
		Critere("clickThrough -> WS_EX_TRANSPARENT", (ex & WS_EX_TRANSPARENT) != 0, buf);
		Critere("opacity<1 -> WS_EX_LAYERED", (ex & WS_EX_LAYERED) != 0, buf);
		// noActivate a ete livre le 25/09 par un autre agent. Le mesurer ici,
		// c'est le proteger : sa ligne est a deux lignes des miennes.
		Critere("noActivate -> WS_EX_NOACTIVATE", (ex & WS_EX_NOACTIVATE) != 0, buf);

		// L'alpha REELLEMENT pose, pas celui qu'on a stocke.
		COLORREF cle = 0;
		BYTE alpha = 0;
		DWORD flags = 0;
		const BOOL lu = GetLayeredWindowAttributes(hwnd, &cle, &alpha, &flags);
		const BYTE attendu = static_cast<BYTE>(0.5f * 255.0f + 0.5f);
		std::snprintf(buf, sizeof(buf), "GetLayeredWindowAttributes -> %s alpha=%u attendu=%u",
					  lu ? "oui" : "NON", static_cast<unsigned>(alpha), static_cast<unsigned>(attendu));
		Critere("opacity=0.5 -> alpha pose a 128", lu != 0 && (flags & LWA_ALPHA) != 0 && alpha == attendu, buf);

		w.Close();
	}

	// -------------------------------------------------------------------------
	// Essai D — le temoin NEGATIF permanent : la configuration PAR DEFAUT
	//
	// Elle doit rendre EXACTEMENT l'ancien style. NKWindow est partage par
	// toutes les applications ; si ce critere tombe, ce n'est pas une propriete
	// qui manque, c'est NK3DModeler, Nogee, NKUIDesign, NKCode et NkAnimaEditor
	// dont la fenetre a change sans qu'on le demande.
	//
	// ⚠️ Ce critere ne s'inverse PAS en mode negatif : la mutation remet
	//    precisement ce style-la, donc il resterait vert et compterait pour un
	//    echec alors qu'il dit la verite. Il est evalue a part.
	// -------------------------------------------------------------------------
	void EssaiDefauts() {
		NkWindowConfig c = ConfigDeBase("D-defauts");
		NkWindow w;
		if (!w.Create(c)) {
			std::printf("  [ECHEC] la fenetre D n'a pas pu etre creee\n");
			++gEssais;
			++gEchecs;
			return;
		}
		HWND hwnd = w.GetSurfaceDesc().hwnd;
		const DWORD st = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
		// WS_VISIBLE/WS_CLIPSIBLINGS peuvent s'ajouter : on compare les bits qui
		// nous appartiennent.
		const DWORD masque = WS_OVERLAPPEDWINDOW;
		const bool conforme = (st & masque) == masque;
		char buf[160];
		std::snprintf(buf, sizeof(buf), "GWL_STYLE=0x%08lX contient WS_OVERLAPPEDWINDOW ?",
					  static_cast<unsigned long>(st));
		const bool sauve = gAttenduRouge;
		gAttenduRouge = false; // temoin de NON-REGRESSION : vert dans les deux modes
		Critere("config par defaut == WS_OVERLAPPEDWINDOW", conforme, buf);
		gAttenduRouge = sauve;
		w.Close();
	}

	// -------------------------------------------------------------------------
	// Essai E — CE QUI NE PEUT PAS AGIR LE DIT-IL VRAIMENT ?
	//
	// Le refus nomme est la moitie du lot qui ne se voit pas a l'ecran. On le
	// mesure la ou il atterrit : `logs/app.log`, relu sur le disque.
	//
	// ⚠️ ET IL PORTE SON PROPRE NEGATIF, dans le meme essai : `modal` n'est PAS
	//    tenu par Win32 et doit produire une ligne ; `resizable` EST tenu depuis
	//    le correctif et ne doit produire AUCUNE ligne. Un audit qui crie sur
	//    tout ne vaut pas mieux qu'un audit muet — il apprend a l'utilisateur a
	//    ne plus lire son journal.
	// -------------------------------------------------------------------------
	// ⚠️ `_fsopen(..., _SH_DENYNO)` ET PAS `fopen_s`. Mesure du 25/09 : la
	//    premiere version de cette fonction utilisait `fopen_s`, qui sous MSVC
	//    ouvre en acces EXCLUSIF. Le journal etant deja ouvert par le sink du
	//    logger, l'ouverture echouait et la sonde concluait « aucun refus au
	//    journal » — alors que les trois lignes y etaient, horodatees, et
	//    verifiables au `grep`. L'INSTRUMENT ACCUSAIT LE PRODUIT DE SON PROPRE
	//    DEFAUT. Un banc rouge se verifie avant d'etre cru.
	// Combien de fois le motif apparait dans le journal. `JournalContient` ne
	// suffit pas pour l'essai J : « le refus est sorti » et « il n'est sorti
	// QU'UNE FOIS » sont deux questions, et un booleen ne repond qu'a la
	// premiere.
	int JournalCompte(const char *motif) {
		FILE *f = _fsopen("logs/app.log", "rb", _SH_DENYNO);
		if (!f)
			return 0;
		int n = 0;
		char ligne[2048];
		while (std::fgets(ligne, sizeof(ligne), f))
			if (std::strstr(ligne, motif))
				++n;
		std::fclose(f);
		return n;
	}

	bool JournalContient(const char *motif) {
		FILE *f = _fsopen("logs/app.log", "rb", _SH_DENYNO);
		if (!f)
			return false;
		bool trouve = false;
		char ligne[2048];
		while (std::fgets(ligne, sizeof(ligne), f)) {
			if (std::strstr(ligne, motif)) {
				trouve = true;
				break;
			}
		}
		std::fclose(f);
		return trouve;
	}

	void EssaiRefus() {
		NkWindowConfig c = ConfigDeBase("E-refus");
		c.name = "NkWindowSondeRefus";
		// Deux reglages qui n'ont AUCUN sens sur un bureau Windows. Ils sont les
		// seuls qui restent hors promesse ici : depuis le 25/09, tout le reste
		// est TENU sur Win32.
		c.hideSystemUI = true;		// pas de barre systeme a masquer
		c.respectSafeArea = false;	// pas d'encoche a contourner

		NkWindow w;
		if (!w.Create(c)) {
			std::printf("  [ECHEC ] la fenetre E n'a pas pu etre creee\n");
			++gEssais;
			++gEchecs;
			return;
		}
		w.Close();

		Critere("hideSystemUI sans objet -> REFUS", JournalContient("NkWindowConfig::hideSystemUI"),
				"cherche dans logs/app.log");
		Critere("respectSafeArea sans objet -> REFUS", JournalContient("NkWindowConfig::respectSafeArea"),
				"cherche dans logs/app.log");

		// ── LE NEGATIF DE L'AUDIT : ce qui EST tenu doit rester SILENCIEUX ────
		// ⚠️ CE CRITERE A DEJA ATTRAPE UN ATTENDU PERIME, LE JOUR MEME. Il
		//    exigeait un refus sur `canFullscreen`, ce qui etait VRAI le matin
		//    et FAUX l'apres-midi, une fois `canFullscreen` implemente. Le banc
		//    a rougi sur le lot qui le rendait obsolete — c'est le bon sens du
		//    rouge, et la raison d'ecrire les deux directions.
		//    Cette liste EST la promesse de Win32 : si l'une de ces proprietes
		//    cesse d'agir, le journal parlera et ce critere rougira.
		const bool sauve = gAttenduRouge;
		gAttenduRouge = false;
		Critere("resizable EST tenu -> aucun refus", !JournalContient("NkWindowConfig::resizable"),
				"le journal ne doit PAS en parler");
		Critere("canFullscreen EST tenu -> aucun refus", !JournalContient("NkWindowConfig::canFullscreen"),
				"tenu depuis le 25/09 : plus aucun refus attendu");
		Critere("bgColor EST tenu -> aucun refus", !JournalContient("NkWindowConfig::bgColor"),
				"tenu depuis le 25/09 (brosse de classe)");
		gAttenduRouge = sauve;
	}

	// -------------------------------------------------------------------------
	// Essai F — `bgColor` et l'arbitrage des accesseurs (26/09)
	//
	// (F1) La brosse de la CLASSE porte-t-elle la couleur demandee ? On la lit
	//      par `GetClassLongPtrW` puis `GetObject`, c'est-a-dire chez GDI et non
	//      chez nous. C'est cette brosse que Windows etale pendant un
	//      redimensionnement : la mesurer, c'est mesurer le clignotement.
	//
	// (F2) UN ACCESSEUR DECRIT LE MONDE. Sur une fenetre fermee — donc sans
	//      fenetre native — `IsAlwaysOnTop()` et `IsClickThrough()` doivent rendre
	//      FALSE, meme si la configuration les demandait. Avant l'arbitrage ils
	//      rendaient `mConfig`, c'est-a-dire notre memoire au lieu du monde.
	//      ⚠️ Ce critere ne s'inverse pas en mode negatif : il ne depend pas du
	//         style de fenetre que la mutation remet.
	// -------------------------------------------------------------------------
	void EssaiFondEtAccesseurs() {
		NkWindowConfig c = ConfigDeBase("F-fond");
		c.name = "NkWindowSondeFond"; // classe DISTINCTE : la brosse est par classe
		c.bgColor = 0x2A5F6EFF;		  // petrole Rihen, et aucune valeur par defaut
		c.alwaysOnTop = true;
		c.clickThrough = true;

		NkWindow w;
		if (!w.Create(c)) {
			std::printf("  [ECHEC ] la fenetre F n'a pas pu etre creee\n");
			++gEssais;
			++gEchecs;
			return;
		}
		HWND hwnd = w.GetSurfaceDesc().hwnd;

		HBRUSH brosse = reinterpret_cast<HBRUSH>(GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND));
		LOGBRUSH lb = {};
		const int lu = GetObjectW(brosse, sizeof(lb), &lb);
		const COLORREF attendu = RGB(0x2A, 0x5F, 0x6E);
		char buf[200];
		std::snprintf(buf, sizeof(buf), "brosse de classe = 0x%06lX, attendu 0x%06lX (style %lu)",
					  static_cast<unsigned long>(lb.lbColor), static_cast<unsigned long>(attendu),
					  static_cast<unsigned long>(lb.lbStyle));
		Critere("bgColor -> brosse de fond de la classe", lu != 0 && lb.lbStyle == BS_SOLID && lb.lbColor == attendu,
				buf);

		// La fenetre EXISTE : les deux accesseurs doivent dire vrai.
		Critere("fenetre vivante -> IsAlwaysOnTop vrai", w.IsAlwaysOnTop(), "lu sur WS_EX_TOPMOST");
		Critere("fenetre vivante -> IsClickThrough vrai", w.IsClickThrough(), "lu sur WS_EX_TRANSPARENT");

		w.Close();

		// La fenetre N'EXISTE PLUS : rien n'est applique, donc rien n'est vrai.
		const bool sauve = gAttenduRouge;
		gAttenduRouge = false;
		Critere("fenetre fermee -> IsAlwaysOnTop FAUX", !w.IsAlwaysOnTop(),
				"un accesseur decrit le monde, pas mConfig");
		Critere("fenetre fermee -> IsClickThrough FAUX", !w.IsClickThrough(),
				"un accesseur decrit le monde, pas mConfig");
		gAttenduRouge = sauve;
	}

	// -------------------------------------------------------------------------
	// Essai G — LA TAILLE. Le defaut du 20/09, et son critere.
	//
	// (G1) Demander 1280x720 doit rendre 1280x720 de zone CLIENT — mesure par
	//      `GetClientRect`, chez Windows.
	// (G2) DIX cycles de `SetSize(GetSize())` ne doivent RIEN deplacer. C'est LE
	//      critere qui attrape la derive : un seul cycle pourrait passer
	//      inapercu, dix rendent +16/+39 par tour parfaitement visibles.
	//
	// ⚠️ LA FENETRE SANS CADRE EST LE CAS QUI ECHOUAIT, pas celle a cadre. Les
	//    editeurs (NKUIDesign, Nogee, NkAnimaEditor, NKCode, NK3DModeler) ont
	//    une barre de titre a eux, donc `frame = false` : leur style garde
	//    WS_CAPTION et WS_THICKFRAME pendant que WM_NCCALCSIZE rend toute la
	//    fenetre cliente. `AdjustWindowRectEx` y ajoutait un cadre qui n'existe
	//    pas. Les DEUX cas sont mesures, et le sans-cadre en premier.
	//
	// LE NEGATIF (--ancien-style) : la sonde refait le geste de l'ANCIEN code —
	// `AdjustWindowRectEx` applique SANS la garde `mBorderless`, puis
	// `SetWindowPos` — dix fois, et le critere doit rougir. On mute le GESTE,
	// pas le critere : c'est exactement la ligne qui a ete corrigee.
	// -------------------------------------------------------------------------
	void MesurerTaille(const char *nom, bool avecCadre) {
		NkWindowConfig c = ConfigDeBase(nom);
		c.name = avecCadre ? "NkWindowSondeTailleC" : "NkWindowSondeTailleS";
		c.frame = avecCadre;
		c.width = 1280;
		c.height = 720;

		NkWindow w;
		if (!w.Create(c)) {
			std::printf("  [ECHEC ] la fenetre %s n'a pas pu etre creee\n", nom);
			++gEssais;
			++gEchecs;
			return;
		}
		HWND hwnd = w.GetSurfaceDesc().hwnd;
		char buf[220];

		RECT cr = {};
		GetClientRect(hwnd, &cr);
		const LONG cw0 = cr.right - cr.left, ch0 = cr.bottom - cr.top;
		std::snprintf(buf, sizeof(buf), "GetClientRect = %ldx%ld, demande 1280x720 (%s)", cw0, ch0,
					  avecCadre ? "avec cadre" : "SANS cadre");
		// ⚠️ UNE MUTATION CIBLEE NE PROUVE QUE SA CIBLE. `--ancien-style` refait la
		//    ligne de `SetSize`, et rien d'autre : la taille A LA CREATION est
		//    donc juste dans les deux modes, et ce critere ne s'inverse PAS. Le
		//    compter comme un echec en mode negatif ferait croire que la
		//    mutation couvre un terrain qu'elle ne touche pas.
		{
			const bool sauve = gAttenduRouge;
			gAttenduRouge = false;
			Critere(avecCadre ? "1280x720 demande -> client (cadre)" : "1280x720 demande -> client (sans cadre)",
					cw0 == 1280 && ch0 == 720, buf);
			gAttenduRouge = sauve;
		}

		// Dix cycles. En mode negatif, on refait le GESTE de l'ancien code.
		for (int i = 0; i < 10; ++i) {
			const math::NkVec2u t = w.GetSize();
			if (!gAttenduRouge) {
				w.SetSize(t);
			} else {
				// ANCIEN CODE, mot pour mot : AdjustWindowRectEx sans la garde.
				RECT rc = {0, 0, (LONG)t.x, (LONG)t.y};
				AdjustWindowRectEx(&rc, (DWORD)GetWindowLongW(hwnd, GWL_STYLE), FALSE,
								   (DWORD)GetWindowLongW(hwnd, GWL_EXSTYLE));
				SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
							 SWP_NOMOVE | SWP_NOZORDER);
			}
		}
		GetClientRect(hwnd, &cr);
		const LONG cw1 = cr.right - cr.left, ch1 = cr.bottom - cr.top;
		std::snprintf(buf, sizeof(buf), "apres 10 cycles : %ldx%ld (etait %ldx%ld, derive %+ld %+ld)", cw1, ch1,
					  cw0, ch0, cw1 - cw0, ch1 - ch0);
		// ⚠️ SEULE LA FENETRE SANS CADRE S'INVERSE, et c'est la MESURE qui le dit,
		//    pas une precaution : sous l'ancien code, la fenetre A CADRE ne
		//    derivait pas (`AdjustWindowRectEx` y etait legitime), et la fenetre
		//    SANS cadre derivait de +16/+39 par cycle. C'est precisement pourquoi
		//    le defaut n'a frappe que les editeurs, qui ont leur propre barre de
		//    titre. Exiger que le cas a cadre rougisse serait exiger de la
		//    mutation un effet qu'elle n'a pas.
		const bool sauve = gAttenduRouge;
		gAttenduRouge = gAttenduRouge && !avecCadre;
		Critere(avecCadre ? "10x SetSize(GetSize()) = identite (cadre)"
						  : "10x SetSize(GetSize()) = identite (sans)",
				cw1 == cw0 && ch1 == ch0, buf);
		gAttenduRouge = sauve;

		w.Close();
	}

	void EssaiTaille() {
		MesurerTaille("G-sans-cadre", false);
		MesurerTaille("G-avec-cadre", true);
	}

	// -------------------------------------------------------------------------
	// Essai H — `modal` et `canFullscreen` (consigne ecrite du 25/09)
	//
	// (H1) `modal` avec parent : le parent doit devenir INACTIF —
	//      `IsWindowEnabled` le dit, et c'est Windows qui repond.
	// (H2) a la fermeture de la modale, le parent doit REDEVENIR actif. C'est le
	//      critere qui compte le plus : un parent laisse desactive est une
	//      application morte a l'ecran, sans message d'erreur.
	// (H3) `canFullscreen = false` : SC_MAXIMIZE, tel que le systeme l'envoie
	//      pour Win+Haut et le double-clic de titre, ne doit PAS maximiser —
	//      `IsZoomed` repond.
	//
	// ⚠️ SendMessageW(WM_SYSCOMMAND) N'EST PAS DE L'INJECTION D'ENTREE : c'est un
	//    message envoye a NOTRE fenetre, dans NOTRE processus. Aucune touche,
	//    aucun clic n'est simule, et rien ne part vers les fenetres de Rodolf.
	// -------------------------------------------------------------------------
	void EssaiModalEtPleinEcran() {
		NkWindowConfig pc = ConfigDeBase("H-parent");
		pc.name = "NkWindowSondeParent";
		NkWindow parent;
		if (!parent.Create(pc)) {
			std::printf("  [ECHEC ] la fenetre parent H n'a pas pu etre creee\n");
			++gEssais;
			++gEchecs;
			return;
		}
		HWND hParent = parent.GetSurfaceDesc().hwnd;
		char buf[200];

		{
			NkWindowConfig mc = ConfigDeBase("H-modale");
			mc.name = "NkWindowSondeModale";
			mc.modal = true;
			mc.native.parentWindowHandle = reinterpret_cast<uintptr>(hParent);
			NkWindow modale;
			if (!modale.Create(mc)) {
				std::printf("  [ECHEC ] la modale H n'a pas pu etre creee\n");
				++gEssais;
				++gEchecs;
			} else {
				const bool actifPendant = IsWindowEnabled(hParent) != 0;
				std::snprintf(buf, sizeof(buf), "IsWindowEnabled(parent) = %s", actifPendant ? "VRAI" : "faux");
				Critere("modal -> le parent est desactive", !actifPendant, buf);
				modale.Close();
				const bool actifApres = IsWindowEnabled(hParent) != 0;
				std::snprintf(buf, sizeof(buf), "IsWindowEnabled(parent) = %s", actifApres ? "vrai" : "FAUX");
				Critere("fermeture -> le parent revit", actifApres, buf);
			}
		}

		{
			NkWindowConfig fc = ConfigDeBase("H-plein-ecran");
			fc.name = "NkWindowSondePlein";
			fc.canFullscreen = false;
			NkWindow f;
			if (!f.Create(fc)) {
				std::printf("  [ECHEC ] la fenetre H-plein-ecran n'a pas pu etre creee\n");
				++gEssais;
				++gEchecs;
			} else {
				HWND h = f.GetSurfaceDesc().hwnd;
				SendMessageW(h, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
				const bool zoome = IsZoomed(h) != 0;
				std::snprintf(buf, sizeof(buf), "IsZoomed apres SC_MAXIMIZE = %s", zoome ? "VRAI" : "faux");
				Critere("canFullscreen=false -> SC_MAXIMIZE refuse", !zoome, buf);
				f.Close();
			}
		}

		parent.Close();
	}

	// -- ESSAI I : LE CURSEUR, INTERROGE AU SYSTEME ------------------------
	// Un utilisateur reel, 26/09 : « les methodes pour curseur dans NKWindow ne
	// fonctionnent pas ». Cet essai demande a WINDOWS ce qu'il en est, jamais a
	// notre propre memoire : GetClipCursor, GetCapture, ShowCursor.
	//
	// ⚠️ CE QUI N'EST PAS MESURABLE ICI, ET IL FAUT LE DIRE : la FORME du curseur
	//    (SetCursor). Elle ne s'applique immediatement que si le curseur SURVOLE
	//    notre fenetre -- garde volontaire, sans laquelle une application
	//    d'arriere-plan ecrasait en continu le curseur du premier plan. Or les
	//    fenetres de ce banc sont INVISIBLES et rien n'est injecte : aucune
	//    souris ne les survole, donc GetCursor() ne peut rien confirmer.
	//    Ce n'est pas une lacune du banc : c'est la MEME raison qui fait dire a
	//    l'utilisateur que « ca ne marche pas ». Un essai qui pretendrait la
	//    mesurer ici mesurerait autre chose.
	void EssaiCurseur() {
		NkWindowConfig c = ConfigDeBase("I-curseur");
		c.name = "NkWindowSondeCurseur";
		c.width = 640;
		c.height = 480;
		NkWindow w;
		if (!w.Create(c)) {
			std::printf("  [ECHEC ] la fenetre I-curseur n'a pas pu etre creee\n");
			++gEssais;
			++gEchecs;
			return;
		}
		HWND hwnd = w.GetSurfaceDesc().hwnd;

		// 1. LE CONFINEMENT. GetClipCursor rend le rectangle ECRAN en vigueur ; on
		//    le compare a notre zone cliente convertie en ecran. Sans appel, il
		//    rend le bureau entier : le test distingue donc bien les deux etats.
		RECT avant{};
		::GetClipCursor(&avant);
		w.ClipMouseToClient(true);
		RECT apres{};
		::GetClipCursor(&apres);
		RECT cl{};
		::GetClientRect(hwnd, &cl);
		POINT o{cl.left, cl.top}, e{cl.right, cl.bottom};
		::ClientToScreen(hwnd, &o);
		::ClientToScreen(hwnd, &e);
		const bool confine = (apres.left == o.x && apres.top == o.y && apres.right == e.x &&
							  apres.bottom == e.y);
		char d1[192];
		std::snprintf(d1, sizeof(d1), "demande=(%ld,%ld)-(%ld,%ld) systeme=(%ld,%ld)-(%ld,%ld)",
					  (long)o.x, (long)o.y, (long)e.x, (long)e.y, (long)apres.left,
					  (long)apres.top, (long)apres.right, (long)apres.bottom);
		Critere("ClipMouseToClient confine", confine, d1);

		// ⚠️ ET ON RELACHE TOUT DE SUITE. Un banc qui laisserait la souris de
		//    Rodolf confinee a une fenetre invisible AGIRAIT sur son poste -- la
		//    regle dit d'interroger, jamais d'agir.
		w.ClipMouseToClient(false);
		RECT rendu{};
		::GetClipCursor(&rendu);
		const bool relache = (rendu.left == avant.left && rendu.top == avant.top &&
							  rendu.right == avant.right && rendu.bottom == avant.bottom);
		Critere("ClipMouseToClient relache", relache, "le bureau est rendu tel qu'il etait");

		// 2. LA CAPTURE. GetCapture rend la fenetre du THREAD courant qui capture.
		w.CaptureMouse(true);
		const HWND capt = ::GetCapture();
		char d2[128];
		std::snprintf(d2, sizeof(d2), "GetCapture=%p attendu=%p", (void *)capt, (void *)hwnd);
		Critere("CaptureMouse capture", capt == hwnd, d2);
		w.CaptureMouse(false);
		Critere("CaptureMouse relache", ::GetCapture() == nullptr, "GetCapture rend nul");

		// 3. L'AFFICHAGE. ShowCursor tient un COMPTEUR, pas un booleen : chaque
		//    masquage le decremente. On le lit par un couple (+1, -1) qui rend la
		//    valeur sans la laisser changee.
		const int cptAvant = ::ShowCursor(TRUE);
		::ShowCursor(FALSE);
		w.ShowMouse(false);
		const int cptApres = ::ShowCursor(TRUE);
		::ShowCursor(FALSE);
		char d3[128];
		std::snprintf(d3, sizeof(d3), "compteur %d -> %d (ShowMouse(false) doit le baisser)",
					  cptAvant, cptApres);
		Critere("ShowMouse baisse le compteur", cptApres < cptAvant, d3);
		w.ShowMouse(true); // on rend le curseur, quoi qu'il arrive

		w.Close();
	}

	// -- ESSAI J : LE MECANISME DU REFUS DE METHODE -----------------------
	// ⚠️ IL MESURE LE MECANISME, PAS LES DORSAUX, et il faut le dire : les
	//    quatre sites qui refusent vraiment (SetCursor sur X11/Wayland/Cocoa,
	//    CaptureMouse sur XLib/XCB, les deux de Wayland) ne sont pas executables
	//    depuis Windows. Ce banc ne prouve donc PAS qu'ils parlent -- il prouve
	//    que la porte par laquelle ils parlent fonctionne.
	//
	// ⚠️ ET IL VERIFIE QUE LE COMPTEUR COMPTE PAR CLE, pas globalement. Un
	//    mecanisme qui ne dirait qu'UNE seule chose pour tout le programme
	//    passerait un critere « une seule ligne » sans qu'on s'en apercoive :
	//    il faut donc DEUX methodes differentes et exiger DEUX lignes.
	//
	// ⚠️ ON MESURE DES DELTAS. Le journal n'est pas vide entre deux executions ;
	//    un compte absolu melangerait les lignes d'hier aux notres.
	void EssaiRefusMethode() {
		const int a0 = JournalCompte("NkWindow::SondeMethodeA");
		const int b0 = JournalCompte("NkWindow::SondeMethodeB");

		// Trois appels sur la MEME cle : un seul message est du.
		NkWindowRefuserMethode("SondeMethodeA", "sonde", "essai du mecanisme, sans effet reel.");
		NkWindowRefuserMethode("SondeMethodeA", "sonde", "essai du mecanisme, sans effet reel.");
		NkWindowRefuserMethode("SondeMethodeA", "sonde", "essai du mecanisme, sans effet reel.");
		// Une autre cle : elle doit avoir SA ligne.
		NkWindowRefuserMethode("SondeMethodeB", "sonde", "essai du mecanisme, sans effet reel.");

		NkLog::Instance().Flush();
		const int a = JournalCompte("NkWindow::SondeMethodeA") - a0;
		const int b = JournalCompte("NkWindow::SondeMethodeB") - b0;

		char d1[128];
		std::snprintf(d1, sizeof(d1), "3 appels sur la meme cle -> %d ligne(s), exige 1", a);
		Critere("refus de methode : une fois par cle", a == 1, d1);

		char d2[128];
		std::snprintf(d2, sizeof(d2), "une SECONDE cle -> %d ligne(s), exige 1 (sinon le compteur "
									  "est global)", b);
		Critere("refus de methode : compte par cle", b == 1, d2);
	}

	int Mesurer(bool negatif) {
		gAttenduRouge = negatif;
		std::printf("\n=== NkWindowSonde — %s ===\n",
					negatif ? "NEGATIF (--ancien-style) : les criteres DOIVENT rougir"
							: "MESURE : les criteres doivent etre verts");
		std::printf("--- G. LA TAILLE : SetSize(GetSize()) est-il l'identite ?\n");
		EssaiTaille();
		std::printf("--- A. les cinq comportements a false\n");
		EssaiComportements();
		if (!negatif) { // le WndProc ne se demute pas : l'essai B n'a pas de negatif
			std::printf("--- B. les bornes de taille\n");
			EssaiBornes();
			std::printf("--- C. la fenetre discrete\n");
			EssaiDiscrete();
			std::printf("--- E. les refus nommes au journal\n");
			EssaiRefus();
			std::printf("--- F. bgColor et l'arbitrage des accesseurs\n");
			EssaiFondEtAccesseurs();
			std::printf("--- H. modal et canFullscreen\n");
			EssaiModalEtPleinEcran();
		}
		if (!negatif) { // ces appels agissent deja sur Win32 : pas de negatif ici
			std::printf("--- I. le CURSEUR, interroge au systeme\n");
			EssaiCurseur();
		}
		if (!negatif) {
			std::printf("--- J. le MECANISME du refus de methode\n");
			EssaiRefusMethode();
		}
		std::printf("--- D. temoin de NON-REGRESSION (config par defaut)\n");
		EssaiDefauts();
		return gEchecs;
	}

#endif // Windows

} // namespace

int nkmain(const NkEntryState &state) {
	bool negatif = false;
	for (uint32 i = 0; i < state.args.Size(); ++i) {
		if (std::strcmp(state.args[i].CStr(), "--ancien-style") == 0)
			negatif = true;
	}

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
	const int echecs = Mesurer(negatif);
	std::printf("\nStatus: %s  (%d essais, %d echecs)%s\n", echecs == 0 ? "VERT" : "ROUGE", gEssais, echecs,
				negatif ? "  [mode negatif : VERT = le banc A BIEN ROUGI sur l'ancien style]" : "");
	std::fflush(stdout);
	return echecs == 0 ? 0 : 1;
#else
	// Un banc qui pretendrait mesurer un dorsal qu'il ne sait pas interroger
	// mentirait. Il le dit et il sort.
	(void)negatif;
	std::printf("Status: SANS OBJET — cette sonde n'interroge que Win32 "
				"(GetWindowLong / GetMenuState / GetLayeredWindowAttributes).\n");
	std::fflush(stdout);
	return 0;
#endif
}
