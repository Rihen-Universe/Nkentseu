#pragma once
// =============================================================================
// Nogee/Panels/Model/NkAssetBrowserModel.h — MODELE NEUTRE du navigateur d'assets
// =============================================================================
// Aucune bibliotheque d'interface incluse. Cf. NkConsoleModel.h pour la raison
// d'etre de ce dossier.
//
// Contenu : le dossier courant, les entrees listees, la selection, le filtre et
// le budget de vignettes par image. `NkAssetBrowserEntry` etait DEJA neutre
// avant l'extraction — il ne referencait aucun type NKUI ; il etait seulement
// piege dans un en-tete qui incluait `NKUI/NKUI.h`.
//
// ⚠️ `thumbnailHandle` reste un `nk_uint64` OPAQUE : c'est un identifiant de
// texture rendu par l'AssetManager, pas un type d'interface. Le modele ne sait
// pas ce qu'il designe, et c'est voulu — c'est ce qui le garde neutre.
//
// Le panneau HERITE de ce modele (cf. NkSceneTreeModel.h pour le pourquoi).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Nogee/Editor/AssetManager.h" // NkAssetType (+ AssetManager, neutre)
#include "NKFileSystem/NkDirectory.h"  // navigation (logique fichiers, zero UI)
#include "NKFileSystem/NkPath.h"
#include <cstring>
#include <cctype>

namespace nkentseu {
	namespace noge {

		struct NkAssetBrowserEntry {
				NkString name;
				NkString fullPath;
				NkString relativePath;
				NkAssetType type;
				bool isDirectory;
				nk_uint64 thumbnailHandle = 0;
				// [PERF 2026-07-25] Vignette paresseuse : true des qu'une generation
				// a ete tentee (reussie ou non) — evite de re-tenter chaque image un
				// fichier illisible.
				bool thumbnailTried = false;
		};

		struct NkAssetBrowserModel {
				// [PERF 2026-07-25] Budget de vignettes generees PAR IMAGE. Avant :
				// RefreshEntries() chargeait toutes les textures du dossier d'un coup
				// (CPU + televersement GPU) -> plusieurs secondes de demarrage sur un
				// dossier fourni. Depuis : generation etalee au rendu.
				static constexpr int32 THUMB_LOADS_PER_FRAME = 2;

				AssetManager *mAssetMgr = nullptr;
				NkString mProjectDir;
				NkString mCurrentDir;
				NkString mSelectedPath;
				float32 mThumbnailSize = 72.f;

				NkVector<NkAssetBrowserEntry> mEntries;

				int32 mThumbBudget = 0;
				char mFilterBuf[128] = {};

				bool HasSelection() const noexcept {
					return !mSelectedPath.Empty();
				}

				const NkString &SelectedPath() const noexcept {
					return mSelectedPath;
				}

				// ── Navigation (2026-08-17, montee du panneau NKUI) ──────────
				// Deplacee ICI depuis `AssetBrowser.cpp` au portage 4/4 : c'est
				// de la logique fichiers pure (NKFileSystem), sans un type
				// d'interface — exactement ce que ce dossier existe pour
				// partager. La laisser au panneau NKUI aurait force le panneau
				// NKGui a la recopier : la duplication deja payee une fois sur
				// le pilote Console.

				// Recherche sous-chaine insensible a la casse (ASCII) — NkString
				// n'a qu'un Contains sensible a la casse. Convention du panneau
				// d'ORIGINE, conservee au portage (a la difference de Console,
				// qui filtre sensible — divergence historique, pas un choix).
				static bool ContainsInsensitive(const NkString &haystack, const char *needle) noexcept {
					if (!needle || !needle[0])
						return true;
					const char *h = haystack.CStr();
					if (!h)
						return false;
					const nk_usize hLen = haystack.Length();
					const nk_usize nLen = (nk_usize)strlen(needle);
					if (nLen > hLen)
						return false;
					for (nk_usize i = 0; i + nLen <= hLen; ++i) {
						nk_usize j = 0;
						while (j < nLen && tolower((unsigned char)h[i + j]) == tolower((unsigned char)needle[j]))
							++j;
						if (j == nLen)
							return true;
					}
					return false;
				}

				void Init(AssetManager *mgr, const char *projectDir) noexcept {
					mAssetMgr = mgr;
					mProjectDir = NkString(projectDir ? projectDir : ".");
					NavigateTo(mProjectDir.CStr());
				}

				void NavigateTo(const char *dir) noexcept {
					mCurrentDir = NkString(dir);
					RefreshEntries();
				}

				void RefreshEntries() noexcept {
					mEntries.Clear();

					// Dossier inaccessible : liste vide. (L'ancien site emettait un
					// Warnf mal forme — marqueur `{}` dans la famille printf, compte
					// dans les 145 de ROADMAP §10quinquies ; le site a disparu avec
					// le deplacement.)
					if (!NkDirectory::Exists(mCurrentDir.CStr()))
						return;

					// Dossier parent (..)
					if (mCurrentDir != mProjectDir) {
						NkAssetBrowserEntry parent;
						parent.name = "..";
						parent.fullPath = NkPath(mCurrentDir.CStr()).GetDirectory();
						parent.isDirectory = true;
						parent.type = NkAssetType::Unknown;
						mEntries.PushBack(parent);
					}

					NkVector<NkDirectoryEntry> found = NkDirectory::GetEntries(mCurrentDir.CStr());
					for (nk_usize i = 0; i < found.Size(); ++i) {
						const NkDirectoryEntry &d = found[i];

						NkAssetBrowserEntry e;
						e.name = d.Name;
						e.fullPath = d.FullPath.ToString();
						e.isDirectory = d.IsDirectory;
						e.type = e.isDirectory ? NkAssetType::Unknown : AssetManager::DetectType(d.Name.CStr());

						// Chemin relatif au projet (cle du cache d'assets)
						e.relativePath = e.fullPath;
						if (e.relativePath.StartsWith(mProjectDir.CStr())) {
							e.relativePath = e.relativePath.SubStr(mProjectDir.Length());
							if (!e.relativePath.Empty() &&
								(e.relativePath[0] == '/' || e.relativePath[0] == '\\'))
								e.relativePath = e.relativePath.SubStr(1);
						}

						// Vignettes PARESSEUSES : aucune generation ici (cf. budget
						// THUMB_LOADS_PER_FRAME, consomme au rendu par les panneaux).
						mEntries.PushBack(e);
					}
				}
		};

	} // namespace noge
} // namespace nkentseu
