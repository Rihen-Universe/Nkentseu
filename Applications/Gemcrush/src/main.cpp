// =============================================================================
// FICHIER: src/main.cpp
// DESCRIPTION: GemCrush — jeu style Candy Crush construit sur Nkentseu.
//              Un seul fichier compilé pour TOUTES les plateformes (le point
//              d'entrée natif est abstrait par NKWindow/NkMain.h) :
//                PC     : Windows, Linux, macOS
//                Mobile : Android, iOS, HarmonyOS
//                Web    : Emscripten
//
//              CE QUE FAIT CE FICHIER, ET RIEN D'AUTRE :
//                - monte fenêtre, rendu, interface NKGui et audio ;
//                - tient la MACHINE À ÉTATS des écrans (menu / carte / partie) ;
//                - tient les RÈGLES DE NIVEAU (coups, chrono, objectifs) ;
//                - route les entrées : barre de titre, puis HUD, puis plateau ;
//                - dessine dans l'ordre : fond -> cadre -> gemmes -> HUD.
//              L'art des gemmes est dans Ui/NkGemArt, la mise en page dans
//              Ui/NkGemHud, les écrans dans Ui/NkGemScreens, les règles du
//              plateau dans NkGemBoard, la définition des niveaux dans
//              NkGemLevels.
//
//   GemCrush.exe --backend=dx11   (vk / dx11 / dx12 / sw / opengl)
//   GemCrush.exe --selftest       (banc du geste, sans fenêtre)
//   GemCrush.exe --capture=x.png [--capture-frame=n] [--screen=menu|map|game]
//   GemCrush.exe --size=LxH       (éprouver le paysage sur PC)
//   GemCrush.exe --showcase       (vitrine des gemmes spéciales)
// =============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"
#include "NKMath/NkRandom.h"
#include "NKMemory/NkAllocator.h"

#include "NKFont/Embedded/NkFontEmbedded.h"

#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/UI/NkGuiCanvasBackend.h"

#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"

#include "Gemcrush/NkGem.h"
#include "Gemcrush/NkGemBoard.h"
#include "Gemcrush/NkGemLevels.h"
#include "Gemcrush/NkMatchFinder.h"
#include "Gemcrush/Ui/NkGemArt.h"
#include "Gemcrush/Ui/NkGemAudio.h"
#include "Gemcrush/Ui/NkGemHud.h"
#include "Gemcrush/Ui/NkGemScreens.h"
#include "Gemcrush/Ui/NkGemSplash.h"
#include "Gemcrush/Ui/NkGemTutorial.h"
#include "Gemcrush/Ui/NkGemTheme.h"

using namespace nkentseu;
using namespace nkentseu::game;
using namespace nkentseu::game::ui;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "GemCrush";
	d.appVersion = "1.0.0";
	return d;
})());

// -----------------------------------------------------------------------------
// BARRE DE TITRE PERSONNALISÉE : bureau UNIQUEMENT.
//
// Sur mobile et sur le Web, la fenêtre appartient au système : il n'y a rien à
// décorer, et une fausse barre volerait de la hauteur d'écran. La distinction
// est faite ICI, à la compilation, plutôt que par un test à l'exécution — une
// plateforme qui n'a pas de barre n'a pas non plus à en porter le code.
// -----------------------------------------------------------------------------
#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS)
#	define GEMCRUSH_CUSTOM_TITLEBAR 1
#else
#	define GEMCRUSH_CUSTOM_TITLEBAR 0
#endif

namespace {

	constexpr float32 kComboBannerSeconds = 1.4f;

	/// Écran courant. La partie n'est qu'UN des trois — c'est ce qui permet au
	/// menu et à la carte de ne rien savoir du plateau.
	///
	/// ⚠️ PAS `Screen` tout court : sous Linux, <X11/Xlib.h> définit un type
	/// GLOBAL du même nom, et le compilateur ne peut plus départager les deux.
	/// Invisible sous Windows, qui n'inclut jamais cet en-tête — c'est la
	/// construction WSL2 qui l'a révélé. Même raison pour NoPointer ci-dessous :
	/// <X11/X.h> fait de `None` une MACRO, qui transformerait
	/// `PointerPhase::NoPointer` en `PointerPhase::0L`.
	enum class GemScreen : uint8 { Splash, Menu, Map, Game };

	// =====================================================================
	// Options de ligne de commande
	// =====================================================================
	NkGraphicsApi ParseBackend(const NkVector<NkString> &args) {
		for (usize i = 1; i < args.Size(); ++i) {
			const NkString &a = args[i];
			if (a == "--backend=vulkan" || a == "-bvk")
				return NkGraphicsApi::NK_GFX_API_VULKAN;
			if (a == "--backend=dx11" || a == "-bdx11")
				return NkGraphicsApi::NK_GFX_API_DX11;
			if (a == "--backend=dx12" || a == "-bdx12")
				return NkGraphicsApi::NK_GFX_API_DX12;
			if (a == "--backend=sw" || a == "-bsw")
				return NkGraphicsApi::NK_GFX_API_SOFTWARE;
			if (a == "--backend=opengl" || a == "-bgl")
				return NkGraphicsApi::NK_GFX_API_OPENGL;
		}
#if defined(NKENTSEU_PLATFORM_WINDOWS)
		return NkGraphicsApi::NK_GFX_API_DX11;
#else
		return NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
	}

	bool HasFlag(const NkVector<NkString> &args, const char *flag) {
		for (usize i = 1; i < args.Size(); ++i) {
			if (args[i] == flag) {
				return true;
			}
		}
		return false;
	}

	/// Entier qui suit un préfixe (`--capture-frame=90` -> 90). Rend `fallback`
	/// si l'option est absente ou mal formée.
	int32 ParseInt(const NkVector<NkString> &args, const char *prefix, int32 fallback) {
		const usize prefixLength = NkString(prefix).Size();
		for (usize i = 1; i < args.Size(); ++i) {
			if (!args[i].StartsWith(prefix)) {
				continue;
			}
			const NkString value = args[i].SubStr(prefixLength);
			int32 parsed = 0;
			bool any = false;
			for (usize k = 0; k < value.Size(); ++k) {
				const char c = value[k];
				if (c < '0' || c > '9') {
					return fallback;
				}
				parsed = parsed * 10 + static_cast<int32>(c - '0');
				any = true;
			}
			return any ? parsed : fallback;
		}
		return fallback;
	}

	NkString ParseText(const NkVector<NkString> &args, const char *prefix) {
		const usize prefixLength = NkString(prefix).Size();
		for (usize i = 1; i < args.Size(); ++i) {
			if (args[i].StartsWith(prefix)) {
				return args[i].SubStr(prefixLength);
			}
		}
		return NkString();
	}

	bool ParseWindowSize(const NkVector<NkString> &args, uint32 &outWidth, uint32 &outHeight) {
		const NkString value = ParseText(args, "--size=");
		if (value.Size() == 0) {
			return false;
		}
		uint32 numbers[2] = {0u, 0u};
		int32 slot = 0;
		bool any = false;
		for (usize k = 0; k < value.Size() && slot < 2; ++k) {
			const char c = value[k];
			if (c >= '0' && c <= '9') {
				numbers[slot] = numbers[slot] * 10u + static_cast<uint32>(c - '0');
				any = true;
			} else if (c == 'x' || c == 'X') {
				++slot;
			}
		}
		if (any && numbers[0] >= 320u && numbers[1] >= 320u) {
			outWidth = numbers[0];
			outHeight = numbers[1];
			return true;
		}
		return false;
	}

	// =====================================================================
	// Couleur de départ qui évite un alignement immédiat au remplissage.
	// =====================================================================
	NkGemColor PickInitialColor(const NkGemBoard &board, int32 row, int32 column, int32 colorCount) {
		const uint32 palette = static_cast<uint32>(math::NkClamp(colorCount, 3, 6));
		NkGemColor color = NkGemColor::NK_GEM_COLOR_RED;

		for (int32 attempt = 0; attempt < 12; ++attempt) {
			color = static_cast<NkGemColor>(math::NkRandom::Instance().NextUInt32(1u, palette + 1u));

			bool horizontalRun = false;
			if (column >= 2) {
				const NkGem *left1 = board.GetGem(row, column - 1);
				const NkGem *left2 = board.GetGem(row, column - 2);
				horizontalRun = left1 != nullptr && left2 != nullptr && left1->GetColor() == color &&
								left2->GetColor() == color;
			}
			bool verticalRun = false;
			if (row >= 2) {
				const NkGem *top1 = board.GetGem(row - 1, column);
				const NkGem *top2 = board.GetGem(row - 2, column);
				verticalRun = top1 != nullptr && top2 != nullptr && top1->GetColor() == color && top2->GetColor() == color;
			}
			if (!horizontalRun && !verticalRun) {
				break;
			}
		}
		return color;
	}

	// =====================================================================
	// Pointeur unifié : souris ET tactile produisent le MÊME geste.
	// =====================================================================
	enum class PointerPhase : uint8 { NoPointer, Down, Move, Up };

	PointerPhase ReadPointer(const NkEvent &event, math::NkVec2f &outPosition) {
		if (const auto *press = event.As<NkMouseButtonPressEvent>()) {
			if (press->GetButton() != NkMouseButton::NK_MB_LEFT) {
				return PointerPhase::NoPointer;
			}
			outPosition = math::NkVec2f(static_cast<float32>(press->GetX()), static_cast<float32>(press->GetY()));
			return PointerPhase::Down;
		}
		if (const auto *release = event.As<NkMouseButtonReleaseEvent>()) {
			if (release->GetButton() != NkMouseButton::NK_MB_LEFT) {
				return PointerPhase::NoPointer;
			}
			outPosition = math::NkVec2f(static_cast<float32>(release->GetX()), static_cast<float32>(release->GetY()));
			return PointerPhase::Up;
		}
		if (const auto *move = event.As<NkMouseMoveEvent>()) {
			outPosition = math::NkVec2f(static_cast<float32>(move->GetX()), static_cast<float32>(move->GetY()));
			return PointerPhase::Move;
		}
		if (const auto *begin = event.As<NkTouchBeginEvent>()) {
			if (begin->GetNumTouches() == 0) {
				return PointerPhase::NoPointer;
			}
			outPosition = math::NkVec2f(begin->GetTouch(0).clientX, begin->GetTouch(0).clientY);
			return PointerPhase::Down;
		}
		if (const auto *move = event.As<NkTouchMoveEvent>()) {
			if (move->GetNumTouches() == 0) {
				return PointerPhase::NoPointer;
			}
			outPosition = math::NkVec2f(move->GetTouch(0).clientX, move->GetTouch(0).clientY);
			return PointerPhase::Move;
		}
		if (const auto *end = event.As<NkTouchEndEvent>()) {
			if (end->GetNumTouches() == 0) {
				return PointerPhase::NoPointer;
			}
			outPosition = math::NkVec2f(end->GetTouch(0).clientX, end->GetTouch(0).clientY);
			return PointerPhase::Up;
		}
		return PointerPhase::NoPointer;
	}

	bool LoadFonts(nkgui::NkGuiFont *body, nkgui::NkGuiFont *title, nkgui::NkGuiFont *small,
				   renderer::NkGuiCanvasBackend *backend, float32 bodyPx) {
		// ⚠️ texId DISTINCT par police : toute NkGuiFont porte le même id par
		// défaut ('NKFT'). Deux polices qui le gardent partagent le même atlas
		// côté backend et s'écrasent — piège déjà payé dans Mou.
		body->texId = 0x4E4B4654u;
		title->texId = body->texId + 1u;
		small->texId = body->texId + 2u;

		const bool okBody = body->LoadEmbedded(NkEmbeddedFontId::DroidSans, bodyPx) ||
							body->LoadEmbedded(NkEmbeddedFontId::ProggyClean, bodyPx);
		title->LoadEmbedded(NkEmbeddedFontId::DroidSans, bodyPx * 1.85f);
		small->LoadEmbedded(NkEmbeddedFontId::DroidSans, bodyPx * 0.78f);

		nkgui::NkGuiFont *fonts[3] = {body, title, small};
		for (int32 i = 0; i < 3; ++i) {
			if (fonts[i]->pixels != nullptr && fonts[i]->atlasW > 0 && fonts[i]->atlasH > 0) {
				backend->UploadFontGray8(fonts[i]->TexId(), fonts[i]->pixels, fonts[i]->atlasW, fonts[i]->atlasH);
			}
		}
		return okBody;
	}

	// =====================================================================
	// --selftest : banc du GESTE, sans fenêtre, sans GPU, sans regarder.
	//
	// ⚠️ IL CONTIENT SON CAS NÉGATIF : un glissé SOUS le seuil ne doit RIEN
	// échanger. Sans lui, un code qui échangerait au moindre pixel de
	// mouvement passerait tous les autres cas.
	// =====================================================================
	int32 RunSelfTest() {
		int32 failures = 0;
		const float32 cell = 64.f;
		const float32 threshold = cell * 0.30f;
		const int32 rows = 8, columns = 8;

		auto makeBoard = [&](NkGemBoard &board) {
			board.Fill([&board](int32 row, int32 column) { return PickInitialColor(board, row, column, 6); });
			board.SetCellSize(cell);
			board.SetOrigin(math::NkVec2f(0.f, 0.f));
			board.GetGem(4, 4)->SetColor(NkGemColor::NK_GEM_COLOR_RED);
			board.GetGem(4, 5)->SetColor(NkGemColor::NK_GEM_COLOR_BLUE);
		};
		const math::NkVec2f centre(4.f * cell + cell * 0.5f, 4.f * cell + cell * 0.5f);

		auto press = [](NkGemBoard &board, const math::NkVec2f &p) {
			NkMouseButtonPressEvent e(NkMouseButton::NK_MB_LEFT, static_cast<int32>(p.x), static_cast<int32>(p.y));
			board.OnEvent(e);
		};
		auto move = [](NkGemBoard &board, const math::NkVec2f &p) {
			NkMouseMoveEvent e(static_cast<int32>(p.x), static_cast<int32>(p.y), 0, 0, 0, 0);
			board.OnEvent(e);
		};
		auto release = [](NkGemBoard &board, const math::NkVec2f &p) {
			NkMouseButtonReleaseEvent e(NkMouseButton::NK_MB_LEFT, static_cast<int32>(p.x), static_cast<int32>(p.y));
			board.OnEvent(e);
		};

		// -- CAS 1 : glissé AU-DELÀ du seuil -> échange -------------------
		{
			NkGemBoard board(rows, columns, cell);
			makeBoard(board);
			press(board, centre);
			move(board, math::NkVec2f(centre.x + threshold + 6.f, centre.y));
			const bool swapped = board.GetGem(4, 4)->GetColor() == NkGemColor::NK_GEM_COLOR_BLUE &&
								 board.GetGem(4, 5)->GetColor() == NkGemColor::NK_GEM_COLOR_RED;
			const bool busy = !board.IsIdle();
			if (swapped && busy) {
				logger.Info("[selftest] CAS 1 glisse au-dela du seuil -> echange : OK");
			} else {
				logger.Error("[selftest] CAS 1 ECHEC (echange={0} occupe={1})", swapped, busy);
				++failures;
			}
			release(board, math::NkVec2f(centre.x + threshold + 6.f, centre.y));
		}

		// -- CAS 2 (NÉGATIF) : glissé SOUS le seuil -> rien ---------------
		{
			NkGemBoard board(rows, columns, cell);
			makeBoard(board);
			press(board, centre);
			move(board, math::NkVec2f(centre.x + threshold * 0.4f, centre.y));
			const bool untouched = board.GetGem(4, 4)->GetColor() == NkGemColor::NK_GEM_COLOR_RED &&
								   board.GetGem(4, 5)->GetColor() == NkGemColor::NK_GEM_COLOR_BLUE;
			const bool idle = board.IsIdle();
			if (untouched && idle) {
				logger.Info("[selftest] CAS 2 glisse sous le seuil -> aucun echange : OK");
			} else {
				logger.Error("[selftest] CAS 2 ECHEC (intact={0} repos={1})", untouched, idle);
				++failures;
			}
			release(board, math::NkVec2f(centre.x + threshold * 0.4f, centre.y));
		}

		// -- CAS 3 : deux TAPS successifs -> échange ----------------------
		{
			NkGemBoard board(rows, columns, cell);
			makeBoard(board);
			press(board, centre);
			release(board, centre);
			const bool selected = board.GetSelectedCell().x == 4 && board.GetSelectedCell().y == 4;
			const math::NkVec2f voisine(centre.x + cell, centre.y);
			press(board, voisine);
			release(board, voisine);
			const bool swapped = board.GetGem(4, 4)->GetColor() == NkGemColor::NK_GEM_COLOR_BLUE &&
								 board.GetGem(4, 5)->GetColor() == NkGemColor::NK_GEM_COLOR_RED;
			if (selected && swapped) {
				logger.Info("[selftest] CAS 3 tap + tap -> echange : OK");
			} else {
				logger.Error("[selftest] CAS 3 ECHEC (selection={0} echange={1})", selected, swapped);
				++failures;
			}
		}

		// -- CAS 4 : glissé en DIAGONALE -> un seul axe -------------------
		{
			NkGemBoard board(rows, columns, cell);
			makeBoard(board);
			board.GetGem(5, 5)->SetColor(NkGemColor::NK_GEM_COLOR_GREEN);
			press(board, centre);
			move(board, math::NkVec2f(centre.x + threshold + 10.f, centre.y + threshold * 0.6f));
			const bool horizontalWon = board.GetGem(4, 4)->GetColor() == NkGemColor::NK_GEM_COLOR_BLUE;
			const bool diagonalUntouched = board.GetGem(5, 5)->GetColor() == NkGemColor::NK_GEM_COLOR_GREEN;
			if (horizontalWon && diagonalUntouched) {
				logger.Info("[selftest] CAS 4 diagonale -> axe dominant seul : OK");
			} else {
				logger.Error("[selftest] CAS 4 ECHEC (axeX={0} diagonale={1})", horizontalWon, diagonalUntouched);
				++failures;
			}
			release(board, centre);
		}

		// -- CAS 5 : la carte DÉSIGNE ce qu'elle DESSINE ------------------
		// Le clic et le dessin partagent NkGemMapGeometry. Ce cas vérifie
		// l'accord : viser le centre d'un nœud doit rendre CE niveau.
		{
			NkGemLayout layout;
			layout.safe = NkRect{0.f, 0.f, 480.f, 800.f};
			layout.scale = 1.f;
			NkGemMapGeometry geometry = NkGemMapGeometry::Compute(layout, 0.f);
			bool allMatch = true;
			int32 tested = 0;
			for (int32 level = 1; level <= 6; ++level) {
				const math::NkVec2f c = geometry.NodeCenter(level);
				if (c.y < layout.safe.y || c.y > layout.safe.y + layout.safe.h) {
					continue;
				}
				++tested;
				allMatch = allMatch && (geometry.HitTest(c) == level);
			}
			// Cas négatif : un point loin de tout nœud ne désigne RIEN.
			const bool emptyMisses = (geometry.HitTest(math::NkVec2f(5.f, 5.f)) == -1);
			if (allMatch && tested >= 3 && emptyMisses) {
				logger.Info("[selftest] CAS 5 carte : dessin et clic d'accord sur {0} noeuds : OK", tested);
			} else {
				logger.Error("[selftest] CAS 5 ECHEC (accord={0} testes={1} vide={2})", allMatch, tested, emptyMisses);
				++failures;
			}
		}

		// -- CAS 6 : les niveaux sont DÉTERMINISTES -----------------------
		// Deux appels doivent rendre le même niveau, sinon la progression et
		// les étoiles ne veulent rien dire.
		{
			bool stable = true;
			for (int32 level = 1; level <= NK_GEM_LEVEL_COUNT; ++level) {
				const NkGemLevelDef a = NkMakeLevel(level);
				const NkGemLevelDef b = NkMakeLevel(level);
				stable = stable && a.mode == b.mode && a.moves == b.moves && a.targetScore == b.targetScore &&
						 a.objectiveCount == b.objectiveCount;
				for (int32 k = 0; k < a.objectiveCount; ++k) {
					stable = stable && a.objectives[k].color == b.objectives[k].color &&
							 a.objectives[k].goal == b.objectives[k].goal;
				}
			}
			if (stable) {
				logger.Info("[selftest] CAS 6 les {0} niveaux sont deterministes : OK", NK_GEM_LEVEL_COUNT);
			} else {
				logger.Error("[selftest] CAS 6 ECHEC : un niveau change d'un appel a l'autre");
				++failures;
			}
		}

		// -- CAS 7 : les étoiles suivent la RESSOURCE, et les deux chemins
		//    (jauge affichée / étoiles accordées) sont d'accord --------------
		{
			bool ok = true;

			// Les trois bandes.
			ok = ok && NkStarsFromSpend(true, 0.10f) == 3;
			ok = ok && NkStarsFromSpend(true, 0.50f) == 2;
			ok = ok && NkStarsFromSpend(true, 0.95f) == 1;

			// Cas NÉGATIF : perdre ne rapporte rien, même en n'ayant presque
			// rien dépensé. Sans ce cas, une règle qui rendrait toujours 3
			// étoiles passerait les trois lignes ci-dessus.
			const bool loserGetsNothing = (NkStarsFromSpend(false, 0.f) == 0);
			ok = ok && loserGetsNothing;

			// La fonte est CONTINUE et décroissante.
			ok = ok && NkStarFill(2, 0.f) > 0.99f;
			ok = ok && !NkStarIsLit(2, 1.f);
			// Le total part de 3 (trois etoiles pleines) : initialiser le
			// temoin a 2 faisait rougir des la premiere iteration. Defaut du
			// BANC, pas de la regle — corrige ici.
			float32 previous = 4.f;
			for (int32 k = 0; k <= 20; ++k) {
				const float32 spent = static_cast<float32>(k) / 20.f;
				const float32 total = NkStarFill(0, spent) + NkStarFill(1, spent) + NkStarFill(2, spent);
				ok = ok && (total <= previous + 0.001f); // jamais croissante
				previous = total;
			}

			// ACCORD : le nombre d'étoiles encore ENTAMÉES à l'écran doit être
			// celui que l'écran de fin accorde. Deux chemins, un seul verdict —
			// c'est la divergence qu'on ne verrait pas autrement.
			bool agree = true;
			for (int32 movesLeft = 0; movesLeft <= 24; ++movesLeft) {
				NkGemHudState probe;
				probe.mode = NkGemMode::NK_MODE_MOVES;
				probe.movesTotal = 24;
				probe.movesLeft = movesLeft;
				const float32 spent = NkGemSpentFraction(probe);
				int32 shown = 0;
				for (int32 i = 0; i < 3; ++i) {
					if (NkStarIsLit(i, spent)) {
						++shown;
					}
				}
				const int32 awarded = NkStarsFromSpend(true, spent);
				// Tant qu il reste de la ressource, les deux doivent donner LE
				// MEME nombre. A l epuisement exact, l affichage tombe a 0 et le
				// verdict garde 1 : c est le plancher "gagner reste gagner",
				// et c est le seul ecart tolere.
				if (spent < 0.999f) {
					if (shown != awarded) {
						logger.Error("[selftest] desaccord : coupsRestants={0} depense={1:.4} montre={2} accorde={3}",
									 movesLeft, spent, shown, awarded);
					}
					agree = agree && (shown == awarded);
				} else {
					agree = agree && (shown == 0 && awarded == 1);
				}
			}
			ok = ok && agree;

			if (ok) {
				logger.Info("[selftest] CAS 7 etoiles a la ressource + accord affichage/verdict : OK");
			} else {
				logger.Error("[selftest] CAS 7 ECHEC (perdant={0} accord={1})", loserGetsNothing, agree);
				++failures;
			}
		}

		// -- CAS 8 : la ressource GÈLE dès l'objectif atteint --------------
		//
		// C'est le défaut signalé par Rodolf : en mode CHRONO, le temps
		// continuait de couler pendant la cascade qui suit le coup gagnant, si
		// bien que les étoiles ACCORDÉES ne reflétaient plus le temps auquel le
		// joueur avait RÉUSSI. On rejoue ici la règle de la boucle de jeu.
		{
			auto makeTimedHud = [](int32 collected) {
				NkGemHudState h;
				h.mode = NkGemMode::NK_MODE_TIMED;
				h.timeTotal = 60.f;
				h.timeLeft = 48.f; // 20 % consommé -> trois étoiles
				h.objectiveCount = 1;
				h.objectives[0].color = NkGemColor::NK_GEM_COLOR_RED;
				h.objectives[0].goal = 10;
				h.objectives[0].collected = collected;
				return h;
			};
			// Douze secondes de cascade, à la cadence de la boucle de jeu.
			auto runCascade = [](NkGemHudState &h) {
				for (int32 f = 0; f < 720; ++f) {
					if (NkGemResourceRuns(h)) {
						h.timeLeft = math::NkMax(0.f, h.timeLeft - 1.f / 60.f);
					}
				}
			};

			// (a) Objectif ATTEINT : le temps ne doit plus bouger.
			NkGemHudState won = makeTimedHud(10);
			const bool frozen = !NkGemResourceRuns(won);
			const int32 starsAtWin = NkStarsFromSpend(true, NkGemSpentFraction(won));
			runCascade(won);
			const int32 starsAfter = NkStarsFromSpend(true, NkGemSpentFraction(won));

			// (b) CONTRÔLE NÉGATIF — objectif NON atteint : le temps DOIT couler,
			// et douze secondes doivent coûter une étoile. Sans ce cas, une règle
			// qui gèlerait TOUJOURS passerait le point (a) sans rien prouver.
			NkGemHudState playing = makeTimedHud(4);
			const bool running = NkGemResourceRuns(playing);
			const int32 starsBefore = NkStarsFromSpend(true, NkGemSpentFraction(playing));
			runCascade(playing);
			const int32 starsPenalised = NkStarsFromSpend(true, NkGemSpentFraction(playing));

			const bool ok = frozen && running && starsAtWin == 3 && starsAfter == starsAtWin &&
							starsBefore == 3 && starsPenalised < starsBefore;
			if (ok) {
				logger.Info("[selftest] CAS 8 ressource gelee a la victoire ({0} etoiles conservees), "
							"et consommee sinon ({1} -> {2}) : OK",
							starsAfter, starsBefore, starsPenalised);
			} else {
				logger.Error("[selftest] CAS 8 ECHEC (gele={0} tourne={1} victoire={2}->{3} en_jeu={4}->{5})", frozen,
							 running, starsAtWin, starsAfter, starsBefore, starsPenalised);
				++failures;
			}
		}

		// -- CAS 9 : l'ouverture se termine, et elle se saute ---------------
		//
		// Un ecran d'ouverture qui ne se termine jamais bloque le jeu AVANT
		// qu'on ait pu jouer — et sur mobile, on ne peut meme pas le tuer
		// autrement qu'en fermant l'application.
		{
			// (a) Elle finit SEULE, et en un temps raisonnable.
			NkGemSplash automatic;
			const bool doneAtStart = automatic.IsDone(); // doit etre faux
			float32 elapsed = 0.f;
			bool finished = false;
			for (int32 f = 0; f < 1200 && !finished; ++f) { // 20 s de marge
				elapsed += 1.f / 60.f;
				finished = automatic.Update(1.f / 60.f);
			}
			// Deux volets : ni instantane (on ne verrait rien), ni interminable.
			const bool duration = finished && elapsed > 3.0f && elapsed < 6.0f;

			// (b) Elle se SAUTE : un appui par volet, deux volets.
			NkGemSplash skipped;
			skipped.Skip();
			const bool afterFirst = !skipped.IsDone(); // le 1er saut passe au 2e volet
			skipped.Skip();
			const bool afterSecond = skipped.IsDone();

			const bool ok = !doneAtStart && duration && afterFirst && afterSecond;
			if (ok) {
				logger.Info("[selftest] CAS 9 ouverture : se termine en {0:.2} s et se saute en 2 appuis : OK",
							elapsed);
			} else {
				logger.Error("[selftest] CAS 9 ECHEC (depart={0} duree={1:.2}s finie={2} saut1={3} saut2={4})",
							 doneAtStart, elapsed, finished, afterFirst, afterSecond);
				++failures;
			}
		}

		// -- CAS 10 : le didacticiel se termine TOUJOURS --------------------
		//
		// Il attend une action precise (un alignement, un objectif, une
		// speciale). Un joueur peut tres bien faire autre chose : si la consigne
		// l'attendait indefiniment, elle deviendrait un mur, et sur mobile il ne
		// pourrait que fermer l'application.
		{
			bool ok = true;

			// (a) Sans AUCUNE action du joueur, il s'efface tout seul.
			NkGemTutorial idle;
			idle.Begin(1);
			const bool armed = idle.IsActive();
			float32 elapsed = 0.f;
			while (idle.IsActive() && elapsed < 120.f) {
				idle.Update(1.f / 60.f);
				elapsed += 1.f / 60.f;
			}
			const bool endedAlone = !idle.IsActive();

			// (b) Avec l'action attendue, il passe a la felicitation puis finit.
			NkGemTutorial played;
			played.Begin(1);
			played.Notify(NkGemTutorialEvent::MatchResolved);
			const bool stillOnPraise = played.IsActive(); // etape de felicitation
			for (int32 f = 0; f < 600 && played.IsActive(); ++f) {
				played.Update(1.f / 60.f);
			}
			const bool endedAfterPraise = !played.IsActive();

			// (c) CONTROLE NEGATIF : au-dela du niveau 3, rien ne s'arme. Sans ce
			// cas, un didacticiel qui s'afficherait a TOUS les niveaux passerait
			// les deux premiers points.
			NkGemTutorial late;
			late.Begin(7);
			const bool silentLate = !late.IsActive();

			ok = armed && endedAlone && stillOnPraise && endedAfterPraise && silentLate;
			if (ok) {
				logger.Info("[selftest] CAS 10 didacticiel : s'efface seul en {0:.1} s, muet au niveau 7 : OK",
							elapsed);
			} else {
				logger.Error("[selftest] CAS 10 ECHEC (arme={0} seul={1} felicitation={2} fini={3} muet7={4})", armed,
							 endedAlone, stillOnPraise, endedAfterPraise, silentLate);
				++failures;
			}
		}

		// -- CAS 11 : la progression SURVIT a une fermeture -----------------
		//
		// Defaut signale sur mobile : le jeu ne conservait rien. Cause mesuree :
		// hors Windows, GetAppDataDirectory() rend $HOME/.config, et sur Android
		// HOME n'est pas defini -> "/.config", non inscriptible.
		//
		// Ce cas ECRIT vraiment sur le disque, puis relit avec un objet NEUF.
		// Sans le second objet, on relirait la memoire et le test passerait meme
		// si rien n'atteignait le disque.
		{
			// On rend sa progression au joueur a la fin : un banc ne doit pas
			// effacer la partie de celui qui le lance.
			NkGemProgress avant;
			const bool avaitUneSauvegarde = avant.Load();

			NkGemProgress ecriture;
			ecriture.Reset();
			ecriture.RecordResult(1, 3);
			ecriture.RecordResult(2, 2);
			ecriture.RecordScore(4242);
			const bool ecrit = ecriture.Save();

			NkGemProgress relecture; // objet NEUF : rien en memoire
			const bool relu = relecture.Load();
			const bool memeEtoiles = relecture.GetStars(1) == 3 && relecture.GetStars(2) == 2;
			const bool memeScore = relecture.GetBestScore() == 4242;
			const bool memeDeblocage = relecture.GetHighestUnlocked() >= 3;

			// CONTROLE NEGATIF : apres une remise a zero ecrite sur le disque, une
			// relecture NEUVE doit rendre zero. Sans lui, un Load() qui ne lirait
			// rien du tout et laisserait l'objet a ses valeurs par defaut pourrait
			// passer pour un succes si on ne testait que des valeurs non nulles.
			NkGemProgress remise;
			remise.Reset();
			remise.Save();
			NkGemProgress apresRemise;
			apresRemise.Load();
			const bool bienEfface = apresRemise.GetStars(1) == 0 && apresRemise.GetBestScore() == 0;

			// Restauration de l'etat du joueur.
			if (avaitUneSauvegarde) {
				avant.Save();
			}

			const bool ok = ecrit && relu && memeEtoiles && memeScore && memeDeblocage && bienEfface;
			if (ok) {
				logger.Info("[selftest] CAS 11 progression : ecrite, relue a neuf, et effacable : OK");
			} else {
				logger.Error("[selftest] CAS 11 ECHEC (ecrit={0} relu={1} etoiles={2} score={3} deblocage={4} efface={5})",
							 ecrit, relu, memeEtoiles, memeScore, memeDeblocage, bienEfface);
				++failures;
			}
		}

		if (failures == 0) {
			logger.Info("[selftest] 11 cas sur 11 : VERT");
		} else {
			logger.Error("[selftest] {0} cas en ECHEC sur 11", failures);
		}
		return failures;
	}

} // namespace

// =====================================================================
// Point d'entrée — commun PC + mobile + Web
// =====================================================================
int nkmain(const NkEntryState &state) {
	// -- 0. Banc du geste : ni fenêtre ni GPU, donc lançable partout. Le
	//    code de sortie est le VERDICT (0 = vert).
	if (HasFlag(state.args, "--selftest")) {
		return RunSelfTest();
	}

	// -- 1. Fenêtre. Défaut PORTRAIT : c'est l'orientation du jeu. -------
	NkWindowConfig cfg;
	cfg.title = "GemCrush";
	cfg.width = 480;
	cfg.height = 854;
	ParseWindowSize(state.args, cfg.width, cfg.height);
	cfg.centered = true;
	cfg.resizable = true;
#if GEMCRUSH_CUSTOM_TITLEBAR
	// Sans bordure : c'est le jeu qui dessine sa barre de titre.
	// ⚠️ MESURÉ, pas supposé : ce réglage est ignoré par les plateformes qui
	// ne savent pas faire, et le journal dit lequel des deux a été obtenu.
	cfg.frame = HasFlag(state.args, "--native-frame"); // sans bordure par defaut : barre de titre dessinee
#endif
	NkWindow window;
	if (!window.Create(cfg)) {
		logger.Error("[gemcrush] creation de fenetre impossible");
		return -1;
	}

	// -- 2. Cible de rendu NKCanvas -------------------------------------
	NkContextDesc desc;
	desc.api = ParseBackend(state.args);
	renderer::NkRenderWindow target(window, desc);
	if (!target.IsValid()) {
		logger.Error("[gemcrush] initialisation de NkRenderWindow ECHOUEE");
		window.Close();
		return -2;
	}
	logger.Info("[gemcrush] backend graphique retenu = {0}", NkGraphicsApiName(desc.api));

	// -- 3. Interface NKGui sur NKCanvas --------------------------------
	nkgui::NkGuiContext guiContext;
	if (!guiContext.Init(static_cast<int32>(cfg.width), static_cast<int32>(cfg.height))) {
		logger.Error("[gemcrush] initialisation de NkGuiContext ECHOUEE");
		window.Close();
		return -3;
	}
	nkgui::SetCurrentContext(&guiContext);

	renderer::NkGuiCanvasBackend guiBackend;
	if (!guiBackend.Init(target.GetRenderer())) {
		logger.Error("[gemcrush] initialisation de NkGuiCanvasBackend ECHOUEE");
		window.Close();
		return -4;
	}

	nkgui::NkGuiFont fontBody, fontTitle, fontSmall;
	math::NkVec2u size = target.GetSize();
	float32 loadedFontPx =
		NkGemLayout::SuggestedBodyFontPx(static_cast<float32>(size.x), static_cast<float32>(size.y), window.GetDpiScale());
	if (!LoadFonts(&fontBody, &fontTitle, &fontSmall, &guiBackend, loadedFontPx)) {
		logger.Warn("[gemcrush] aucune police chargee — les textes ne s'afficheront pas");
	}
	guiContext.font = &fontBody;

	NkGemFonts fonts;
	fonts.body = &fontBody;
	fonts.title = &fontTitle;
	fonts.small = &fontSmall;

	// -- 4. Audio. Un échec N'ARRÊTE PAS le jeu. ------------------------
	NkGemAudio audio;
	audio.Init();
	audio.StartMusic();

	// -- 5. Progression -------------------------------------------------
	NkGemProgress progress;
	progress.Load(); // absence = première partie, pas une erreur

	// -- 6. État de session ---------------------------------------------
	// L'ouverture est le premier ecran. Elle est SAUTABLE : voir NkGemSplash.
	GemScreen screen = GemScreen::Splash;
	NkGemSplash splash;
	NkGemTutorial tutorial;
	NkGemLevelDef levelDef = NkMakeLevel(1);
	NkGemBoard *board = nullptr;
	NkGemHudState hud;
	NkGemLayout layout;
	NkGemHudButtons hudButtons, overlayButtons;
	NkGemMenuButtons menuButtons;
	NkGemMapButtons mapButtons;
	NkGemTitleBar titleBar;
	NkGemMapGeometry mapGeometry;

	float32 mapScroll = 0.f;
	bool mapDragging = false;
	float32 mapDragStartY = 0.f;
	float32 mapScrollAtDragStart = 0.f;
	bool mapDragMoved = false;

	math::NkVec2f pointer(0.f, 0.f);
	bool pointerCapturedByUi = false;
	float32 gameTime = 0.f;
	float32 hintTimer = 0.f;
	float32 tickAccumulator = 0.f;
	math::NkVec2i hintA(-1, -1), hintB(-1, -1);
	NkGemBoardPhase previousPhase = NkGemBoardPhase::NK_BOARD_PHASE_IDLE;
	bool resultRecorded = false;
	bool layoutDirty = false; ///< la mise en page doit être refaite (nouveau plateau)
	const bool showcase = HasFlag(state.args, "--showcase");

	auto &allocator = memory::NkGetDefaultAllocator();

	// -- Démarrage d'un niveau ------------------------------------------
	auto startLevel = [&](int32 levelIndex, bool forceTimed) {
		levelDef = NkMakeLevel(levelIndex);
		if (forceTimed) {
			// « Défi chrono » depuis le menu : même niveau, autre règle.
			levelDef.mode = NkGemMode::NK_MODE_TIMED;
			levelDef.seconds = 90.f;
		}
		if (board != nullptr) {
			allocator.Delete(board);
			board = nullptr;
		}
		board = allocator.New<NkGemBoard>(levelDef.rowCount, levelDef.columnCount, 64.f);
		const int32 colours = levelDef.colorCount;
		board->Fill([board, colours](int32 row, int32 column) { return PickInitialColor(*board, row, column, colours); });

		hud = NkGemHudState{};
		hud.level = levelDef.index;
		hud.mode = levelDef.mode;
		hud.movesLeft = levelDef.moves;
		hud.movesTotal = levelDef.moves;
		hud.timeLeft = levelDef.seconds;
		hud.timeTotal = levelDef.seconds;
		hud.targetScore = levelDef.targetScore;
		hud.objectiveCount = levelDef.objectiveCount;
		for (int32 i = 0; i < levelDef.objectiveCount; ++i) {
			hud.objectives[i].color = levelDef.objectives[i].color;
			hud.objectives[i].goal = levelDef.objectives[i].goal;
			hud.objectives[i].collected = 0;
		}

		// Score et objectifs : le plateau signale ce qu'il a supprimé, le
		// COMPTAGE est une règle de niveau — pas une affaire de plateau.
		board->SetMatchCallback([&hud, &audio, &tutorial, board](const NkVector<NkGemMatch> &matches) {
			const int32 cascade = math::NkMax(1, board->GetCascadeCount());
			bool special = false;
			bool gainedObjective = false;
			for (typename NkVector<NkGemMatch>::SizeType m = 0; m < matches.Size(); ++m) {
				const int32 count = static_cast<int32>(matches[m].cells.Size());
				hud.score += count * 10 * cascade; // la cascade récompense le bon coup, pas le coup nombreux
				special = special || (matches[m].specialToCreate != NkGemSpecialKind::NK_GEM_SPECIAL_NONE);
				for (int32 i = 0; i < hud.objectiveCount; ++i) {
					if (hud.objectives[i].color == matches[m].color) {
						hud.objectives[i].collected += count;
						gainedObjective = true;
					}
				}
			}
			audio.PlayMatch(cascade);
			tutorial.Notify(NkGemTutorialEvent::MatchResolved);
			if (gainedObjective) {
				tutorial.Notify(NkGemTutorialEvent::ObjectiveGained);
			}
			if (special) {
				audio.PlaySfx(NkGemSfx::Special);
				tutorial.Notify(NkGemTutorialEvent::SpecialCreated);
			}
			if (cascade >= 2) {
				hud.comboCount = cascade;
				hud.comboTimer = kComboBannerSeconds;
			}
		});

		// Vitrine : force une spéciale de chaque type sur la 1re ligne.
		// Elles n'apparaissent qu'après un alignement de 4+ ; sans ce mode,
		// leur dessin ne serait jamais regardé avant d'être joué.
		if (showcase) {
			const NkGemSpecialKind kinds[5] = {
				NkGemSpecialKind::NK_GEM_SPECIAL_STRIPED_HORIZONTAL, NkGemSpecialKind::NK_GEM_SPECIAL_STRIPED_VERTICAL,
				NkGemSpecialKind::NK_GEM_SPECIAL_WRAPPED, NkGemSpecialKind::NK_GEM_SPECIAL_COLOR_BOMB,
				NkGemSpecialKind::NK_GEM_SPECIAL_FISH};
			for (int32 i = 0; i < 5 && i < levelDef.columnCount; ++i) {
				NkGem *gem = board->GetGem(1, i);
				if (gem != nullptr) {
					gem->SetSpecialKind(kinds[i]);
				}
			}
		}

		// Didacticiel : armé pour les niveaux 1 à 3, muet au-delà.
		tutorial.Begin(levelDef.index);
		previousPhase = NkGemBoardPhase::NK_BOARD_PHASE_IDLE;
		resultRecorded = false;
		hintTimer = 0.f;
		hud.hintActive = false;
		screen = GemScreen::Game;

		// ⚠️ LA MISE EN PAGE DOIT ÊTRE REFAITE ICI, et pas seulement au
		// redimensionnement : un niveau peut avoir 9 colonnes (index >= 12) ou
		// 9 lignes (>= 24). Sans ce recalcul, le damier reste découpé pour le
		// plateau PRÉCÉDENT pendant que les gemmes se placent sur le nouveau —
		// deux grilles différentes dessinées l'une sur l'autre.
		layoutDirty = true;
	};

	auto applyLayout = [&]() {
		// ZONE SÛRE : demandée à la plateforme, jamais codée en dur.
		NkSafeAreaInsets insets = window.GetSafeAreaInsets();
#if GEMCRUSH_CUSTOM_TITLEBAR
		// La barre de titre EST une zone sûre : on l'ajoute à l'inset haut,
		// et toute la mise en page l'évite sans rien savoir d'elle.
		if (!cfg.frame) {
			// La hauteur de barre suit la densité, comme le reste de l interface.
			insets.top += NkGemTitleBarHeight(math::NkClamp(window.GetDpiScale(), 0.75f, 3.f));
		}
#endif
		const int32 rows = (board != nullptr) ? board->GetRowCount() : levelDef.rowCount;
		const int32 columns = (board != nullptr) ? board->GetColumnCount() : levelDef.columnCount;
		// Densité réelle de l écran, demandée à la plateforme — jamais devinée.
		layout = NkGemLayout::Compute(static_cast<float32>(size.x), static_cast<float32>(size.y), insets, rows,
									  columns, window.GetDpiScale());
		if (board != nullptr) {
			board->SetCellSize(layout.cellSize);
			board->SetOrigin(layout.boardOrigin);
		}
		guiContext.viewW = static_cast<int32>(size.x);
		guiContext.viewH = static_cast<int32>(size.y);
	};

	// -- 7. Fermeture propre et CYCLE DE VIE ----------------------------
	bool running = true;
	// Declares ICI, et non dans le montage de la boucle : les rappels de
	// cycle de vie ci-dessous les CAPTURENT. Ce bloc n'etant compile que sur
	// mobile, une construction Windows ne pouvait pas voir l'erreur — c'est
	// la construction Android qui l'a rendue.
	uint32 lastWidth = 0, lastHeight = 0;
	bool surfaceAlive = true; ///< false = arrière-plan : la surface native n'existe plus
	bool hasFocus = true;	  ///< false = fenêtre visible mais inactive
	auto &events = NkEvents();
	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });

	// ---------------------------------------------------------------------
	// ARRIÈRE-PLAN — Android, iOS et HarmonyOS ont le MÊME cycle de vie de
	// surface : la fenêtre native appartient au système, elle est DÉTRUITE
	// quand l'application part en arrière-plan et RECRÉÉE au retour.
	//
	// ⚠️ Sans ce bloc, l'application revient sur un écran NOIR : elle rend à
	// pleine vitesse dans une surface que le compositeur n'affiche plus, et
	// pas une seule erreur GL n'est émise. Défaut signalé sur Android le
	// 2026-08-28 ; le patron vient de Mou, qui l'avait déjà payé — et qui
	// avait d'abord oublié HarmonyOS en ne visant qu'Android.
	// ---------------------------------------------------------------------
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_HARMONYOS) || \
	defined(NKENTSEU_PLATFORM_IOS)
	events.AddEventCallback<NkWindowHiddenEvent>([&](NkWindowHiddenEvent *) {
		surfaceAlive = false;
		audio.Suspend(); // le son NE continue PAS en arrière-plan
		logger.Info("[gemcrush] arriere-plan : rendu et audio suspendus");
	});
	events.AddEventCallback<NkWindowShownEvent>([&](NkWindowShownEvent *) {
		// Recréer la surface AVANT de reprendre le rendu, sinon la première
		// trame part encore dans l'ancienne — celle qui n'existe plus.
		const bool recreated = target.RecreateSurface();
		surfaceAlive = true;
		audio.Resume();
		// La taille a pu changer (rotation en arrière-plan) : on force le
		// recalcul plutôt que d'attendre un événement qui ne viendra pas.
		lastWidth = 0;
		lastHeight = 0;
		logger.Info("[gemcrush] retour au premier plan : surface recreee = {0}", recreated);
	});
#endif

	// PERTE DE FOCUS — toutes plateformes. La fenêtre reste visible, mais le
	// jeu n'a plus la main : on gèle et on coupe le son. Sur mobile, c'est ce
	// qui se produit quand une notification ou un appel passe devant.
	events.AddEventCallback<NkWindowFocusLostEvent>([&](NkWindowFocusLostEvent *) {
		hasFocus = false;
		audio.Suspend();
	});
	events.AddEventCallback<NkWindowFocusGainedEvent>([&](NkWindowFocusGainedEvent *) {
		hasFocus = true;
		audio.Resume();
	});

	// -- 8. Écran de départ demandé en ligne de commande (captures) -----
	{
		// --level=N : entrer directement dans un niveau, meme verrouille. Sert a
		// eprouver les tailles de plateau qui n'apparaissent qu'en fin de
		// parcours (9 colonnes des le niveau 12) sans jouer douze niveaux.
		const int32 forcedLevel = ParseInt(state.args, "--level=", 0);
		const NkString wanted = ParseText(state.args, "--screen=");
		// Toute demande d'ecran precis saute l'ouverture : sans cela, une
		// capture demandee sur la carte montrerait le logo.
		if (wanted.Size() > 0 || forcedLevel > 0 || showcase) {
			screen = GemScreen::Menu;
		}
		if (wanted == "splash") {
			screen = GemScreen::Splash;
		} else if (forcedLevel > 0) {
			startLevel(math::NkClamp(forcedLevel, 1, NK_GEM_LEVEL_COUNT), false);
		} else if (wanted == "game" || showcase) {
			startLevel(math::NkMax(1, progress.GetHighestUnlocked()), false);
		} else if (wanted == "map") {
			screen = GemScreen::Map;
		}
	}

	// -- 9. Boucle principale -------------------------------------------
	NkClock clock;
	// Chrono de TRAME (distinct de l'horloge de jeu) : il sert uniquement a
	// plafonner la cadence et a rendre la main. Voir la fin de la boucle.
	NkChrono chronoTrame;
	const NkString capturePath = ParseText(state.args, "--capture=");
	const int32 captureFrame = ParseInt(state.args, "--capture-frame=", 45);
	int32 frameIndex = 0;

	while (running && window.IsOpen()) {
		(void)chronoTrame.Reset(); // depart de la trame
		float32 deltaTime = clock.Tick().delta;
		if (deltaTime > 0.1f) {
			deltaTime = 1.f / 60.f; // reprise après veille : on ne rattrape pas
		}
		gameTime += deltaTime;

		// ---- Arrière-plan : AUCUN rendu -----------------------------
		// La surface native n'existe pas. On continue de vider la file
		// d'événements (c'est elle qui apportera NkWindowShownEvent), mais on
		// ne dessine ni ne présente rien, et on rend le CPU au système.
		if (!surfaceAlive) {
			while (NkEvent *event = events.PollEvent()) {
				(void)event;
			}
			NkChrono::Sleep(static_cast<int64>(32));
			continue;
		}

		// ---- Redimensionnement / rotation ---------------------------
		size = target.GetSize();
		if (size.x != lastWidth || size.y != lastHeight) {
			if (lastWidth != 0 && size.x > 0 && size.y > 0) {
				target.OnResize(size.x, size.y);
			}
			lastWidth = size.x;
			lastHeight = size.y;
			applyLayout();

			const float32 wanted =
				NkGemLayout::SuggestedBodyFontPx(static_cast<float32>(size.x), static_cast<float32>(size.y), window.GetDpiScale());
			const float32 drift = (wanted > loadedFontPx) ? (wanted / loadedFontPx) : (loadedFontPx / wanted);
			if (drift > 1.15f) { // recharger les atlas coûte cher : seulement si ça bouge vraiment
				LoadFonts(&fontBody, &fontTitle, &fontSmall, &guiBackend, wanted);
				loadedFontPx = wanted;
			}
		}
		if (layoutDirty) {
			applyLayout();
			layoutDirty = false;
		}
		mapGeometry = NkGemMapGeometry::Compute(layout, mapScroll);

		// ---- Entrées -------------------------------------------------
		while (NkEvent *event = events.PollEvent()) {
			if (!running) {
				break;
			}
			math::NkVec2f position(0.f, 0.f);
			const PointerPhase phase = ReadPointer(*event, position);
			if (phase != PointerPhase::NoPointer) {
				pointer = position;
			}

#if GEMCRUSH_CUSTOM_TITLEBAR
			// La barre de titre passe AVANT tout le reste : elle occupe le
			// haut de l'écran, là où un menu poserait aussi des boutons.
			if (!cfg.frame && phase == PointerPhase::Down && NkGemHudButtons::Contains(titleBar.bar, position)) {
				if (NkGemHudButtons::Contains(titleBar.close, position)) {
					running = false;
				} else if (NkGemHudButtons::Contains(titleBar.minimize, position)) {
					window.Minimize();
				} else if (NkGemHudButtons::Contains(titleBar.maximize, position)) {
					if (window.IsMaximized()) {
						window.Restore();
					} else {
						window.Maximize();
					}
				} else if (titleBar.IsInDragZone(position)) {
					// Hand-off NATIF : le système prend la main sur le
					// déplacement. Suivre la souris à la main marche jusqu'au
					// jour où l'on croise un bureau multi-écrans à DPI mixtes.
					window.BeginDragMove();
				}
				continue;
			}
#endif

			if (screen == GemScreen::Splash) {
				// N'IMPORTE quelle entree passe : appui, touche, doigt. Un ecran
				// d'ouverture qu'on ne peut pas sauter est une taxe payee a
				// chaque lancement.
				if (phase == PointerPhase::Up || event->As<NkKeyPressEvent>() != nullptr) {
					splash.Skip();
					if (splash.IsDone()) {
						screen = GemScreen::Menu;
					}
				}
				continue;
			}

			if (screen == GemScreen::Menu) {
				if (phase == PointerPhase::Up) {
					if (NkGemHudButtons::Contains(menuButtons.play, position)) {
						audio.PlaySfx(NkGemSfx::Button);
						mapScroll = mapGeometry.ScrollToCenter(progress.GetHighestUnlocked());
						screen = GemScreen::Map;
					} else if (NkGemHudButtons::Contains(menuButtons.quickPlay, position)) {
						audio.PlaySfx(NkGemSfx::Button);
						startLevel(math::NkMax(1, progress.GetHighestUnlocked()), true);
					} else if (NkGemHudButtons::Contains(menuButtons.sound, position)) {
						audio.SetMuted(!audio.IsMuted());
						audio.PlaySfx(NkGemSfx::Button);
					} else if (NkGemHudButtons::Contains(menuButtons.reset, position)) {
						audio.PlaySfx(NkGemSfx::Button);
						progress.Reset();
						progress.Save();
					}
				}
				continue;
			}

			if (screen == GemScreen::Map) {
				// La carte DÉFILE au doigt. Un tap ne doit pas se déclencher
				// après un défilement : on distingue les deux par la distance
				// parcourue, exactement comme le glissé du plateau.
				if (phase == PointerPhase::Down) {
					mapDragging = true;
					mapDragMoved = false;
					mapDragStartY = position.y;
					mapScrollAtDragStart = mapScroll;
				} else if (phase == PointerPhase::Move && mapDragging) {
					const float32 delta = position.y - mapDragStartY;
					if (delta > 8.f || delta < -8.f) {
						mapDragMoved = true;
					}
					mapScroll = math::NkClamp(mapScrollAtDragStart - delta, 0.f, mapGeometry.MaxScroll());
				} else if (phase == PointerPhase::Up) {
					const bool wasDrag = mapDragMoved;
					mapDragging = false;
					mapDragMoved = false;
					if (!wasDrag) {
						if (NkGemHudButtons::Contains(mapButtons.back, position)) {
							audio.PlaySfx(NkGemSfx::Button);
							screen = GemScreen::Menu;
						} else {
							const int32 level = mapGeometry.HitTest(position);
							if (level > 0 && progress.IsUnlocked(level)) {
								audio.PlaySfx(NkGemSfx::Button);
								startLevel(level, false);
							} else if (level > 0) {
								audio.PlaySfx(NkGemSfx::Invalid); // verrouillé : on le DIT
							}
						}
					}
				}
				continue;
			}

			// ---- Écran de partie ---------------------------------------
			if (hud.paused || hud.gameOver) {
				if (phase == PointerPhase::Up) {
					if (NkGemHudButtons::Contains(overlayButtons.resume, position)) {
						audio.PlaySfx(NkGemSfx::Button);
						hud.paused = false;
					} else if (NkGemHudButtons::Contains(overlayButtons.restart, position)) {
						audio.PlaySfx(NkGemSfx::Button);
						startLevel(levelDef.index, levelDef.mode == NkGemMode::NK_MODE_TIMED && levelDef.seconds == 90.f);
					} else if (NkGemHudButtons::Contains(overlayButtons.next, position)) {
						audio.PlaySfx(NkGemSfx::Button);
						startLevel(math::NkMin(levelDef.index + 1, NK_GEM_LEVEL_COUNT), false);
					} else if (NkGemHudButtons::Contains(overlayButtons.quit, position)) {
						audio.PlaySfx(NkGemSfx::Button);
						mapScroll = mapGeometry.ScrollToCenter(progress.GetHighestUnlocked());
						screen = GemScreen::Map;
					} else if (NkGemHudButtons::Contains(overlayButtons.home, position)) {
						audio.PlaySfx(NkGemSfx::Button);
						screen = GemScreen::Menu;
					}
				}
				continue;
			}

			if (phase == PointerPhase::Down) {
				// Les boutons du HUD passent AVANT le plateau.
				pointerCapturedByUi = NkGemHudButtons::Contains(hudButtons.pause, position) ||
									  NkGemHudButtons::Contains(hudButtons.hint, position) ||
									  NkGemHudButtons::Contains(hudButtons.shuffle, position);
				if (pointerCapturedByUi) {
					continue;
				}
			} else if (phase == PointerPhase::Up && pointerCapturedByUi) {
				// L'action se déclenche au RELÂCHEMENT, sur le même bouton :
				// glisser hors du bouton annule, comme partout ailleurs.
				if (NkGemHudButtons::Contains(hudButtons.pause, position)) {
					hud.paused = true;
					audio.PlaySfx(NkGemSfx::Button);
				} else if (NkGemHudButtons::Contains(hudButtons.hint, position)) {
					audio.PlaySfx(NkGemSfx::Button);
					hud.hintActive = (board != nullptr) && board->FindHintMove(hintA, hintB);
					hintTimer = hud.hintActive ? 3.f : 0.f;
				} else if (NkGemHudButtons::Contains(hudButtons.shuffle, position)) {
					audio.PlaySfx(NkGemSfx::Special);
					if (board != nullptr) {
						board->Shuffle();
					}
					hud.hintActive = false;
					hintTimer = 0.f;
				}
				pointerCapturedByUi = false;
				continue;
			} else if (pointerCapturedByUi && phase == PointerPhase::Move) {
				continue; // le geste appartient au HUD jusqu'au relâchement
			}

			if (board != nullptr) {
				board->OnEvent(*event);
			}
		}
		if (!running) {
			break;
		}

		// ---- Règles de la partie -------------------------------------
		// `hasFocus` gèle la partie sans toucher au drapeau de PAUSE du joueur :
		// au retour, il retrouve exactement l'écran qu'il a quitté, sans avoir à
		// appuyer sur « reprendre » pour une pause qu'il n'a pas demandée.
		if (screen == GemScreen::Game && board != nullptr && hasFocus && !hud.paused && !hud.gameOver) {
			board->Update(deltaTime);

			// UN COUP = une transition IDLE -> occupé. C'est le plateau qui
			// décide qu'un échange a lieu (tap, glissé ou spéciale) : compter
			// ici évite d'avoir trois endroits qui décrémentent.
			const NkGemBoardPhase phase = board->GetPhase();
			if (previousPhase == NkGemBoardPhase::NK_BOARD_PHASE_IDLE &&
				phase != NkGemBoardPhase::NK_BOARD_PHASE_IDLE) {
				if (hud.mode != NkGemMode::NK_MODE_TIMED && hud.movesLeft > 0 && NkGemResourceRuns(hud)) {
					--hud.movesLeft;
				}
				audio.PlaySfx(NkGemSfx::Swap);
				hud.hintActive = false;
				hintTimer = 0.f;
			}
			// L'échange refusé se DIT : sans retour, le joueur croit que son
			// geste n'a pas été pris.
			if (previousPhase != NkGemBoardPhase::NK_BOARD_PHASE_REVERTING &&
				phase == NkGemBoardPhase::NK_BOARD_PHASE_REVERTING) {
				audio.PlaySfx(NkGemSfx::Invalid);
			}
			previousPhase = phase;

			// Chronomètre — il ne tourne QUE tant que la ressource se consomme.
			// Voir NkGemResourceRuns : dès l'objectif atteint, le temps s'arrête,
			// même si le plateau finit encore ses cascades.
			if (hud.mode == NkGemMode::NK_MODE_TIMED && NkGemResourceRuns(hud)) {
				const float32 before = hud.timeLeft;
				hud.timeLeft = math::NkMax(0.f, hud.timeLeft - deltaTime);
				// Un tic par seconde sur les dix dernières : la tension monte
				// à l'oreille avant d'être lue à l'écran.
				if (hud.timeLeft <= 10.f && hud.timeLeft > 0.f) {
					tickAccumulator += (before - hud.timeLeft);
					if (tickAccumulator >= 1.f) {
						tickAccumulator = 0.f;
						audio.PlaySfx(NkGemSfx::Tick);
					}
				}
			}

			// Fin de partie. On attend que le plateau se STABILISE : les
			// cascades en cours appartiennent au joueur.
			const bool stable = (phase == NkGemBoardPhase::NK_BOARD_PHASE_IDLE) && !board->IsDragging();

			// L'OBJECTIF DÉCIDE, DANS TOUS LES MODES. Le mode ne change que la
			// RESSOURCE dépensée pour l'atteindre. Le score, lui, ne décide plus
			// de rien : on peut marquer beaucoup en jouant mal longtemps.
			bool finished = false;
			bool won = false;
			const bool resourceGone =
				(hud.mode == NkGemMode::NK_MODE_TIMED) ? (hud.timeLeft <= 0.f) : (hud.movesLeft <= 0);
			if (stable && NkGemObjectivesComplete(hud)) {
				finished = true;
				won = true;
			} else if (stable && resourceGone) {
				finished = true;
				won = false;
			}

			if (finished && !resultRecorded) {
				hud.gameOver = true;
				hud.victory = won;
				resultRecorded = true;
				// Les étoiles mesurent l'EFFICACITÉ : ce qu'il restait de
				// ressource au moment de réussir. Même fonction, même valeur que
				// celle affichée pendant la partie.
				const int32 stars = NkStarsFromSpend(won, NkGemSpentFraction(hud));
				// Le meilleur score se garde MEME quand la partie est perdue :
				// c'est une performance, pas une recompense de victoire.
				const bool newBest = progress.RecordScore(hud.score);
				const bool newStars = progress.RecordResult(levelDef.index, stars);
				if (newBest || newStars || won) {
					progress.Save();
				}
				audio.PlaySfx(won ? NkGemSfx::Win : NkGemSfx::Fail);
			}

			// Plateau bloqué : on mélange au lieu d'attendre que le joueur
			// s'épuise. Un plateau sans coup possible ne se distingue pas,
			// à l'œil, d'un plateau difficile.
			// Didacticiel du niveau 1 : l'indice reste ALLUME tant que le joueur
			// n'a pas réussi son premier échange. Lui demander d'appuyer sur
			// INDICE avant de lui avoir expliqué le geste n'aurait pas de sens.
			if (tutorial.WantsPermanentHint() && phase == NkGemBoardPhase::NK_BOARD_PHASE_IDLE) {
				hud.hintActive = board->FindHintMove(hintA, hintB);
			}

			if (!finished && phase == NkGemBoardPhase::NK_BOARD_PHASE_IDLE) {
				math::NkVec2i a(-1, -1), b(-1, -1);
				if (!board->FindHintMove(a, b)) {
					logger.Info("[gemcrush] aucun coup possible : melange automatique");
					board->Shuffle();
				}
			}
		}

		if (screen == GemScreen::Game && hasFocus && !hud.paused && !hud.gameOver) {
			tutorial.Update(deltaTime);
		}

		// ---- Ouverture ----------------------------------------------
		if (screen == GemScreen::Splash) {
			if (splash.Update(deltaTime)) {
				screen = GemScreen::Menu;
			}
		}

		// Score affiché qui rattrape le score réel.
		if (hud.displayedScore != hud.score) {
			const int32 gap = hud.score - hud.displayedScore;
			const int32 step = math::NkMax(1, static_cast<int32>(static_cast<float32>(gap) * 6.f * deltaTime));
			hud.displayedScore += (gap > 0) ? math::NkMin(step, gap) : gap;
		}
		if (hud.comboTimer > 0.f) {
			hud.comboTimer -= deltaTime;
		}
		if (hintTimer > 0.f) {
			hintTimer -= deltaTime;
			if (hintTimer <= 0.f) {
				hud.hintActive = false;
			}
		}

		// ---- Rendu ---------------------------------------------------
		nkgui::SetCurrentContext(&guiContext);
		guiContext.viewW = static_cast<int32>(size.x);
		guiContext.viewH = static_cast<int32>(size.y);
		guiContext.BeginFrame(deltaTime);
		nkgui::NkGuiDrawList &dl = guiContext.dl;

		NkGemHud::DrawBackground(dl, layout, gameTime);
		if (screen == GemScreen::Splash) {
			// Le splash dessine SON propre fond, par-dessus celui du jeu : les
			// deux volets ont chacun leur ambiance (petrole de marque, puis
			// nuit du jeu), et le passage de l'un a l'autre se voit.
			splash.Draw(dl, fonts, layout);
		} else if (screen == GemScreen::Menu) {
			menuButtons = NkGemDrawMainMenu(dl, fonts, layout, progress, gameTime, pointer, audio.IsMuted());
		} else if (screen == GemScreen::Map) {
			mapButtons = NkGemDrawAdventureMap(dl, fonts, layout, mapGeometry, progress, gameTime, pointer);
		} else if (board != nullptr) {
			// L'ORDRE EST LE RENDU : cadre -> gemmes -> HUD -> voile.
			NkGemHud::DrawBoardFrame(dl, layout, gameTime, hud.hintActive ? hintA : math::NkVec2i(-1, -1),
									 hud.hintActive ? hintB : math::NkVec2i(-1, -1), board->GetDragTargetCell());
			board->Draw(dl, gameTime);
			NkGemHud::DrawComboBanner(dl, fonts, layout, hud);
			tutorial.Draw(dl, fonts, layout);
			hudButtons = NkGemHud::DrawHud(dl, fonts, layout, hud, gameTime, pointer);
			overlayButtons = NkGemHud::DrawOverlay(dl, fonts, layout, hud, gameTime, pointer);
		}

#if GEMCRUSH_CUSTOM_TITLEBAR
		if (!cfg.frame) {
			titleBar = NkGemDrawTitleBar(dl, fonts, static_cast<float32>(size.x), layout.scale, "GEMCRUSH",
										 window.IsMaximized(), pointer);
		}
#endif

		guiContext.EndFrame();

		target.Clear(renderer::NkColor2D{16, 13, 38, 255});
		guiBackend.Submit(guiContext.dl, size.x, size.y);
		guiBackend.Submit(guiContext.dlOverlay, size.x, size.y);
		target.Display();

		// ⚠️ CEDER LA MAIN — sans cela, l'onglet Web GELE : une boucle `while`
		// qui ne rend jamais la main au navigateur empeche toute peinture. Le
		// wasm demarre, la boucle part, et rien n'apparait. Defaut signale par
		// Rodolf le 2026-09-01 sur le .bat Web.
		//
		// ⚠️ LE MECANISME EXISTAIT ET ETAIT JUSTE : sous Emscripten,
		// NkChrono::Sleep appelle emscripten_sleep(ms) et YieldThread appelle
		// emscripten_sleep(0), avec ASYNCIFY deja actif dans le .jenga. Ce qui
		// manquait, c'etait l'APPEL.
		//
		// ⚠️ ET LE `Sleep(32)` PLUS HAUT NE COMPTE PAS : il est dans
		// `if (!surfaceAlive)`, donc il ne cede que sur le chemin ou l'on ne
		// dessine pas. Un repli qui ne s'exerce qu'au repos ressemble a une
		// boucle qui cede, et n'en est pas une.
		//
		// ⚠️ LES DEUX BRANCHES CEDENT. Une version « ne dormir que si on est en
		// avance » gele l'onglet des la premiere trame lente — donc tout le
		// temps sur le Web. Le `else` n'est pas une optimisation.
		{
			const float64 ecouleMs = chronoTrame.Elapsed().milliseconds;
			if (ecouleMs < 16.0) {
				NkChrono::Sleep(16.0 - ecouleMs);
			} else {
				NkChrono::YieldThread();
			}
		}

		// Capture APRÈS Display() : c'est le tampon présenté qu'on relit.
		++frameIndex;
		if (capturePath.Size() > 0 && frameIndex == captureFrame) {
			if (target.Capture(capturePath.CStr())) {
				logger.Info("[gemcrush] capture ecrite : {0}", capturePath.CStr());
			} else {
				// Le repli CHANGE le verdict : un backend sans readback le dit.
				logger.Error("[gemcrush] capture IMPOSSIBLE sur ce backend ({0})", NkGraphicsApiName(desc.api));
			}
			running = false;
		}
	}

	progress.Save();
	if (board != nullptr) {
		allocator.Delete(board);
		board = nullptr;
	}
	audio.Shutdown();
	logger.Info("[gemcrush] sortie propre");
	window.Close();
	return 0;
}
