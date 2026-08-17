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
		};

	} // namespace noge
} // namespace nkentseu
