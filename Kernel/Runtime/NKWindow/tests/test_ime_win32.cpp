// =============================================================================
// test_ime_win32.cpp
//
// La composition IME sous Win32, eprouvee SANS IME installe et SANS toucher au
// clavier du poste : les messages sont POSES dans la file de NOTRE fenetre
// (PostMessageW vers un HWND de ce processus), jamais injectes dans la session.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (i1)  conversion : « abc » curseur 2 -> « abc », curseur 2 ; « にほん »
//         -> 9 octets UTF-8, curseur 3 ; un emoji (DEUX unites UTF-16)
//         compte pour UN code point : curseur UTF-16 3 -> 2
//   (i2)  conversion tronquee : jamais au milieu d'un caractere
//   (i3)  une fenetre de ce processus recoit START / COMPOSITION / END poses
//         dans sa file : trois NkTextCompositionEvent, DEBUT, MISE A JOUR, FIN,
//         dans cet ordre, avec l'identifiant de la fenetre
//   (i4)  WM_CHAR en deux demi-codes (D83D, DE00) : UN NkTextInputEvent,
//         U+1F600 ; (i4n) un « a » ordinaire reste un « a »
//   (i5)  SetImeInlineComposition : faux par defaut, vrai quand on le demande,
//         faux quand on le retire
//   (i6)  emballage des coordonnees de la zone : l'aller-retour garde les
//         negatifs
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NKWindow/NKWindow.h"
#include "NKWindow/Platform/Win32/NkWin32Ime.h"

#include <cstring>

using namespace nkentseu;

TEST_CASE(NKWindowIme, I1_Conversion) {
	char s[64];
	int32 c = -1;
	const wchar_t abc[] = L"abc";
	ASSERT_EQUAL(3u, NkWin32ImeConvertir(abc, 3, 2, s, sizeof(s), c));
	ASSERT_TRUE(std::strcmp(s, "abc") == 0);
	ASSERT_EQUAL(2, c);

	const wchar_t nihon[] = {0x306B, 0x307B, 0x3093, 0};
	ASSERT_EQUAL(9u, NkWin32ImeConvertir(nihon, 3, 3, s, sizeof(s), c));
	ASSERT_EQUAL(3, c);
	ASSERT_TRUE(static_cast<unsigned char>(s[0]) == 0xE3u);

	// a, U+1F600 (D83D DE00), b : curseur apres l'emoji = 3 unites UTF-16
	const wchar_t emoji[] = {L'a', 0xD83D, 0xDE00, L'b', 0};
	ASSERT_EQUAL(6u, NkWin32ImeConvertir(emoji, 4, 3, s, sizeof(s), c));
	ASSERT_EQUAL(2, c);
	ASSERT_TRUE(static_cast<unsigned char>(s[1]) == 0xF0u);
}

TEST_CASE(NKWindowIme, I2_Troncature) {
	char s[5]; // 4 octets utiles
	int32 c = 0;
	const wchar_t nihon[] = {0x306B, 0x307B, 0};
	// 2 caracteres de 3 octets : un seul tient, le second n'est pas coupe
	ASSERT_EQUAL(3u, NkWin32ImeConvertir(nihon, 2, 2, s, sizeof(s), c));
	ASSERT_EQUAL(3u, static_cast<uint32>(std::strlen(s)));
}

namespace {

	struct NkFenetreDeBanc {
			NkWindow fenetre;
			bool ok = false;

			NkFenetreDeBanc() {
				NkWESystem::Instance().Initialise();
				NkWindowConfig cfg;
				cfg.title = "NKWindow_Tests IME";
				cfg.width = 320;
				cfg.height = 200;
				// INVISIBLE et sans activation : le banc ne prend jamais le focus
				// a la personne qui travaille devant l'ecran.
				cfg.visible = false;
				cfg.noActivate = true;
				ok = fenetre.Create(cfg);
			}

			~NkFenetreDeBanc() {
				fenetre.Close();
			}

			HWND Hwnd() {
				return fenetre.mData.mHwnd;
			}
	};

	/// Vide la file, en gardant ce qui interesse le banc.
	struct NkReleve {
			NkCompositionPhase phases[8];
			uint64 fenetres[8];
			uint32 nComp = 0;
			uint32 codes[8];
			uint32 nTexte = 0;
	};

	void Pomper(NkReleve &r) {
		NkEventSystem &es = NkWESystem::Events();
		for (int32 tour = 0; tour < 4; ++tour) {
			NkEvent *e = nullptr;
			while ((e = es.PollEvent()) != nullptr) {
				if (auto *c = e->As<NkTextCompositionEvent>()) {
					if (r.nComp < 8) {
						r.phases[r.nComp] = c->GetPhase();
						r.fenetres[r.nComp] = c->GetWindowId();
						++r.nComp;
					}
				} else if (auto *t = e->As<NkTextInputEvent>()) {
					if (r.nTexte < 8) {
						r.codes[r.nTexte++] = t->GetCodepoint();
					}
				}
			}
		}
	}

} // namespace

TEST_CASE(NKWindowIme, I3_PhasesDansLaFile) {
	NkFenetreDeBanc f;
	ASSERT_TRUE(f.ok);
	NkReleve vide;
	Pomper(vide); // ce que la creation de la fenetre a pu laisser
	ASSERT_TRUE(::PostMessageW(f.Hwnd(), WM_IME_STARTCOMPOSITION, 0, 0) != 0);
	ASSERT_TRUE(::PostMessageW(f.Hwnd(), WM_IME_COMPOSITION, 0, GCS_COMPSTR) != 0);
	ASSERT_TRUE(::PostMessageW(f.Hwnd(), WM_IME_ENDCOMPOSITION, 0, 0) != 0);
	NkReleve r;
	Pomper(r);
	ASSERT_EQUAL(3u, r.nComp);
	ASSERT_TRUE(r.phases[0] == NkCompositionPhase::NK_COMPOSITION_BEGIN);
	ASSERT_TRUE(r.phases[1] == NkCompositionPhase::NK_COMPOSITION_UPDATE);
	ASSERT_TRUE(r.phases[2] == NkCompositionPhase::NK_COMPOSITION_END);
	ASSERT_EQUAL(f.fenetre.GetId(), r.fenetres[1]);
}

TEST_CASE(NKWindowIme, I4_DemiCodesUtf16) {
	NkFenetreDeBanc f;
	ASSERT_TRUE(f.ok);
	NkReleve vide;
	Pomper(vide);
	ASSERT_TRUE(::PostMessageW(f.Hwnd(), WM_CHAR, 0xD83D, 0) != 0);
	ASSERT_TRUE(::PostMessageW(f.Hwnd(), WM_CHAR, 0xDE00, 0) != 0);
	ASSERT_TRUE(::PostMessageW(f.Hwnd(), WM_CHAR, L'a', 0) != 0);
	NkReleve r;
	Pomper(r);
	ASSERT_EQUAL(2u, r.nTexte);
	ASSERT_EQUAL(0x1F600u, r.codes[0]);
	ASSERT_EQUAL(static_cast<uint32>('a'), r.codes[1]);
}

TEST_CASE(NKWindowIme, I5_CompositionDessineeParLApplication) {
	NkFenetreDeBanc f;
	ASSERT_TRUE(f.ok);
	ASSERT_FALSE(f.fenetre.GetImeInlineComposition());
	f.fenetre.SetImeInlineComposition(true);
	ASSERT_TRUE(f.fenetre.GetImeInlineComposition());
	f.fenetre.SetTextInputArea(10, 20, 100, 18);
	f.fenetre.SetImeInlineComposition(false);
	ASSERT_FALSE(f.fenetre.GetImeInlineComposition());
}

TEST_CASE(NKWindowIme, I6_EmballageZone) {
	int32 a = 0;
	int32 b = 0;
	ASSERT_TRUE(NkWin32ImeUnpack(NkWin32ImePack(-12, 700), a, b));
	ASSERT_EQUAL(-12, a);
	ASSERT_EQUAL(700, b);
	ASSERT_TRUE(NkWin32ImeUnpack(NkWin32ImePack(0, 0), a, b));
	ASSERT_EQUAL(0, a);
	ASSERT_EQUAL(0, b);
	ASSERT_FALSE(NkWin32ImeUnpack(nullptr, a, b));
}

#endif // Win32
