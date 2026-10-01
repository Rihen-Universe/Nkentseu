// =============================================================================
// NkTerminalPtyCommun.cpp — ce qui ne depend d'aucune plateforme : le
// decoupage d'une ligne de commande et l'interrupteur des contre-epreuves.
// =============================================================================
#include "NKEditorKit/Terminal/NkTerminalPty.h"

#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		void NkTerminalDecouperCommande(const char *ligne, NkVector<NkString> &args) {
			args.Clear();
			if (!ligne)
				return;
			NkString cur;
			bool dansGuillemets = false, aMot = false;
			for (const char *p = ligne; *p; ++p) {
				const char c = *p;
				if (c == '"') {
					// Les guillemets REGROUPENT ; ils ne font pas partie du mot.
					// « "" » produit donc un argument VIDE, voulu (aMot = true).
					dansGuillemets = !dansGuillemets;
					aMot = true;
					continue;
				}
				if (!dansGuillemets && (c == ' ' || c == '\t')) {
					if (aMot) {
						args.PushBack(cur);
						cur = NkString();
						aMot = false;
					}
					continue;
				}
				cur += c;
				aMot = true;
			}
			if (aMot)
				args.PushBack(cur);
		}

		void NkPty::Write(const char *s) {
			if (s)
				Write(s, static_cast<usize>(std::strlen(s)));
		}

		bool NkTerminalMutation(const char *nom) {
			// Lue a chaque appel : un banc peut basculer d'une mutation a l'autre
			// sans relancer le processus. Hors banc, getenv rend nul et c'est tout.
			const char *v = std::getenv("NK_TERMINAL_MUTATION");
			return v && nom && std::strcmp(v, nom) == 0;
		}

	} // namespace editorkit
} // namespace nkentseu
