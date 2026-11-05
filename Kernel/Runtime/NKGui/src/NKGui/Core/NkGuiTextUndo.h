// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkGuiTextUndo.h — ANNULER / REFAIRE D'UN CHAMP DE SAISIE (Ctrl+Z, Ctrl+Maj+Z, Ctrl+Y), 05/11.
//
// Rodolf (05/11) : « vérifie que le copier-coller fonctionne correctement Ctrl+C Ctrl+V Ctrl+X, pareil pour Ctrl+Z et
// Ctrl+Shift+Z, ceci partout dans NKCode ». Les deux champs partages (NkOverlayTextField du kit, InputTextMultiline de
// NKGui) copiaient, coupaient et collaient, mais AUCUN ne savait annuler : Ctrl+Z n'y faisait rien.
//
// Un champ est edite a la fois : l'historique est UN, rattache au tampon qui a le caret (le proprietaire). Il tient des
// INSTANTANES (texte + caret), pas des operations : un instantane avant chaque groupe de modifications. La frappe de
// lettres successives fait UN pas (comme VS Code, un mot a la fois) tant qu'elle se suit sans pause ni saut de caret ;
// tout autre geste (coller, couper, effacer, remplacer) est un pas a lui seul.
//
// Usage, dans le champ, quand il a le focus et avant d'editer :
//     NkGuiTextUndo &u = NkGuiTextUndoDe();
//     u.Debut(buf, buf_proprietaire, caret, temps);      // photographie « avant »
//     ... les editions ...
//     u.Fin(buf, caret, frappe);                          // empile « avant » si le texte a change
// et, sur Ctrl+Z / Ctrl+Maj+Z / Ctrl+Y : u.Annuler(...) / u.Refaire(...) -> le texte et le caret a reprendre.
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKPlatform/NkEnv.h" // la contre-epreuve nommee (NK_NKCODE_MUTATION)

namespace nkentseu {
	namespace nkgui {

		struct NkGuiTextUndo {
				struct Etat {
						NkString texte;
						int32 caret = 0;
				};
				static constexpr int32 kMax = 100; ///< pas gardes (les plus vieux partent)

				const void *proprio = nullptr; ///< le tampon dont c'est l'historique
				NkVector<Etat> pile;		   ///< ce qu'on peut annuler (le dernier = le plus recent)
				NkVector<Etat> refaire;		   ///< ce qu'on peut refaire
				Etat avant;					   ///< le texte au debut de l'image
				bool frappePrecedente = false; ///< le dernier pas empile etait une frappe
				float32 tempsDernierPas = 0.f;
				int32 caretFinPas = -1; ///< ou etait le caret apres le dernier pas de frappe

				/// Photographie « avant » ; un autre champ prend la main : l'historique repart a vide.
				void Debut(const char *buf, const void *champ, int32 caret, float32 temps) {
					if (proprio != champ) {
						proprio = champ;
						pile.Clear();
						refaire.Clear();
						frappePrecedente = false;
						caretFinPas = -1;
					}
					avant.texte = NkString(buf ? buf : "");
					avant.caret = caret;
					tempsActuel = temps;
				}

				/// Apres les editions : si le texte a change, « avant » devient un pas annulable. `frappe` : le
				/// changement n'est que des lettres tapees (elles se regroupent).
				void Fin(const char *buf, int32 caret, bool frappe) {
					const NkString maintenant(buf ? buf : "");
					if (maintenant == avant.texte)
						return;
					const bool regroupe = frappe && frappePrecedente && caretFinPas == avant.caret &&
										  (tempsActuel - tempsDernierPas) < 1.0f && !pile.Empty();
					if (!regroupe) {
						if (static_cast<int32>(pile.Size()) >= kMax)
							pile.Erase(pile.Begin());
						pile.PushBack(avant);
					}
					refaire.Clear();
					frappePrecedente = frappe;
					tempsDernierPas = tempsActuel;
					caretFinPas = caret;
				}

				/// Contre-epreuve NK_NKCODE_MUTATION=saisie-sans-annuler : l'etat d'avant, Ctrl+Z et Ctrl+Maj+Z ne font rien.
				static bool Muet() {
					const char *v = env::GetEnvVar("NK_NKCODE_MUTATION");
					const char *m = "saisie-sans-annuler";
					if (!v)
						return false;
					for (; *m && *v == *m; ++m, ++v) {
					}
					return !*m && !*v;
				}

				/// Ctrl+Z : rend le texte d'avant (et empile l'actuel dans `refaire`). Faux : rien a annuler.
				bool Annuler(const char *actuel, int32 caretActuel, NkString &texte, int32 &caret) {
					if (pile.Empty() || Muet())
						return false;
					Etat e = pile[pile.Size() - 1];
					pile.PopBack();
					Etat cur;
					cur.texte = NkString(actuel ? actuel : "");
					cur.caret = caretActuel;
					refaire.PushBack(cur);
					texte = e.texte;
					caret = e.caret;
					frappePrecedente = false; // le pas suivant ne se colle pas a un pas annule
					return true;
				}

				/// Ctrl+Maj+Z / Ctrl+Y : rend ce qu'on venait d'annuler. Faux : rien a refaire.
				bool Refaire(const char *actuel, int32 caretActuel, NkString &texte, int32 &caret) {
					if (refaire.Empty() || Muet())
						return false;
					Etat e = refaire[refaire.Size() - 1];
					refaire.PopBack();
					Etat cur;
					cur.texte = NkString(actuel ? actuel : "");
					cur.caret = caretActuel;
					pile.PushBack(cur);
					texte = e.texte;
					caret = e.caret;
					frappePrecedente = false;
					return true;
				}

			private:
				float32 tempsActuel = 0.f;
		};

		/// L'historique du champ en cours d'edition (un seul champ est edite a la fois).
		inline NkGuiTextUndo &NkGuiTextUndoDe() {
			static NkGuiTextUndo s;
			return s;
		}

	} // namespace nkgui
} // namespace nkentseu
