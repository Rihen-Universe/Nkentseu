// -----------------------------------------------------------------------------
// @File    main.cpp
// @Brief   Le point d'entree de NKScena (voir NKScena.jenga et ROADMAP.md).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKFileSystem/NkDirectory.h"
#include "NKWindow/NKMain.h"

#include <cstdio>

nkentseu::NkAppData appData = [] {
	nkentseu::NkAppData d{};
	d.appName = "NKScena";
	d.appVersion = "0.1.0";
	return d;
}();
NKENTSEU_APP_DATA_DEFINED(appData);

namespace nkentseu {
	namespace nkscena {
		int32 NkScenaLancerBanc(); // NKScena/NKScenaBanc.cpp
	} // namespace nkscena
} // namespace nkentseu

int nkmain(const nkentseu::NkEntryState &state) {
	using namespace nkentseu;
	for (const auto &a : state.GetArgs()) {
		if (a == "--selftest") {
			return nkscena::NkScenaLancerBanc();
		}
	}
	return 0;
}
