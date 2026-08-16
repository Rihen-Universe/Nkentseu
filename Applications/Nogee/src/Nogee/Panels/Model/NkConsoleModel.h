#pragma once
// =============================================================================
// Nogee/Panels/Model/NkConsoleModel.h — MODELE NEUTRE de la console
// =============================================================================
// N'inclut AUCUNE bibliotheque d'interface : ni NKUI, ni NKGui, ni NKEditorKit.
// C'est la seule regle de ce dossier, et c'est ce qui lui donne sa valeur.
//
// POURQUOI CE DOSSIER EXISTE (2026-08-17) :
//   Le portage pilote du panneau Console vers NKGui a du RECOPIER ~35 lignes de
//   logique de log, parce que le modele vivait dans `ConsolePanel.h` — un
//   en-tete qui inclut `NKUI/NKUI.h`. L'inclure aurait retraine NKUI dans le
//   panneau porte. Resultat : deux verites pour la meme console.
//   Ce dossier retire cette duplication et empeche qu'elle soit payee trois
//   fois de plus (SceneTree, Inspector, AssetBrowser).
//
// CE QUI RESTE DANS LE PANNEAU, ET POURQUOI :
//   la couleur d'un niveau de log est typee par la bibliotheque d'affichage
//   (`nkui::NkColor` et `nkgui::NkColor` sont deux types distincts, sans
//   conversion). Le PREFIXE textuel, lui, est neutre : il est ici.
//
// Ce modele survit a toute decision sur le doublon NKUI/NKGui : il ne depend
// d'aucune des deux.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLogLevel.h"

namespace nkentseu {
	namespace noge {

		struct NkConsoleLine {
				NkString text;
				NkLogLevel level = NkLogLevel::NK_INFO;
				nk_uint32 count = 1; // repetitions consecutives fusionnees
		};

		// Etat + logique de la console, sans une ligne d'interface.
		struct NkConsoleModel {
				static constexpr nk_uint32 kMaxLines = 2048;

				NkVector<NkConsoleLine> mLines;
				bool mAutoScroll = true;
				bool mScrollToEnd = false;

				// Filtres
				bool mShowInfo = true;
				bool mShowWarn = true;
				bool mShowError = true;
				bool mShowDebug = false;
				char mFilterBuf[128] = {};

				nk_uint32 mErrorCount = 0;
				nk_uint32 mWarnCount = 0;

				// Ajoute une ligne ; fusionne les repetitions consecutives.
				void PushLine(const char *text, NkLogLevel level) noexcept {
					if (!text)
						return;

					if (!mLines.IsEmpty()) {
						auto &last = mLines[mLines.Size() - 1];
						if (last.level == level && last.text == text) {
							++last.count;
							mScrollToEnd = mAutoScroll;
							return;
						}
					}

					if (mLines.Size() >= kMaxLines)
						mLines.Erase(mLines.Begin());

					NkConsoleLine line;
					line.text = NkString(text);
					line.level = level;
					line.count = 1;
					mLines.PushBack(line);

					if (level == NkLogLevel::NK_ERROR || level == NkLogLevel::NK_CRITICAL)
						++mErrorCount;
					if (level == NkLogLevel::NK_WARN)
						++mWarnCount;

					mScrollToEnd = mAutoScroll;
				}

				void Clear() noexcept {
					mLines.Clear();
					mErrorCount = 0;
					mWarnCount = 0;
				}

				// Une ligne passe-t-elle les filtres de niveau et de texte ?
				bool Passes(const NkConsoleLine &line) const noexcept {
					switch (line.level) {
						case NkLogLevel::NK_INFO:
							if (!mShowInfo)
								return false;
							break;
						case NkLogLevel::NK_WARN:
							if (!mShowWarn)
								return false;
							break;
						case NkLogLevel::NK_ERROR:
						case NkLogLevel::NK_CRITICAL:
							if (!mShowError)
								return false;
							break;
						case NkLogLevel::NK_DEBUG:
						case NkLogLevel::NK_TRACE:
							if (!mShowDebug)
								return false;
							break;
						default:
							break;
					}
					if (mFilterBuf[0] != '\0' && !line.text.Contains(mFilterBuf))
						return false;
					return true;
				}

				// Prefixe textuel du niveau (neutre ; la COULEUR reste au panneau).
				static const char *LevelPrefix(NkLogLevel lv) noexcept {
					switch (lv) {
						case NkLogLevel::NK_ERROR:
							return "[ERR] ";
						case NkLogLevel::NK_CRITICAL:
							return "[CRT] ";
						case NkLogLevel::NK_WARN:
							return "[WRN] ";
						case NkLogLevel::NK_DEBUG:
							return "[DBG] ";
						case NkLogLevel::NK_TRACE:
							return "[TRC] ";
						default:
							return "[INF] ";
					}
				}
		};

	} // namespace noge
} // namespace nkentseu
