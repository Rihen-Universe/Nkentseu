#pragma once
// =============================================================================
// NkAndroidDropImpl.h — Android Drag & Drop (API 24+ / View.OnDragListener)
//
// Sur Android, le drag & drop fonctionne via :
//  - DragEvent.ACTION_DRAG_STARTED
//  - DragEvent.ACTION_DROP → ClipData (fichiers URI ou texte)
//
// Ceci est un bridge JNI : côté Java, attachez un OnDragListener à la View
// et appelez NkAndroidDropBridge.onDragEvent(action, mimeType, data)
// depuis le JNI pour notifier le moteur natif.
// =============================================================================

#include "../../Events/NkDropEvent.h"
#include <functional>
#include <string>
#include <vector>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	/**
	 * @brief Class NkAndroidDropImpl.
	 */
	class NkAndroidDropImpl {
		public:
			using DropFilesCallback = std::function<void(const NkDropFileData &)>;
			using DropTextCallback = std::function<void(const NkDropTextData &)>;
			using DropEnterCallback = std::function<void(const NkDropEnterData &)>;
			using DropLeaveCallback = std::function<void()>;

			void SetDropFilesCallback(DropFilesCallback cb) {
				mDropFiles = std::move(cb);
			}
			void SetDropTextCallback(DropTextCallback cb) {
				mDropText = std::move(cb);
			}
			void SetDropEnterCallback(DropEnterCallback cb) {
				mDropEnter = std::move(cb);
			}
			void SetDropLeaveCallback(DropLeaveCallback cb) {
				mDropLeave = std::move(cb);
			}

			// Appelé depuis le bridge JNI
			void OnDragStarted(float x, float y, int numItems, bool hasText) {
				NkDropEnterData d;
				d.x = static_cast<int>(x);
				d.y = static_cast<int>(y);
				d.numFiles = numItems;
				d.hasText = hasText;
				d.dropType = numItems > 0 ? NkDropType::NK_DROP_TYPE_FILE
							: hasText	  ? NkDropType::NK_DROP_TYPE_TEXT
										: NkDropType::NK_DROP_TYPE_UNKNOWN;
				if (mDropEnter)
					mDropEnter(d);
			}

			void OnDragLeft() {
				if (mDropLeave)
					mDropLeave();
			}

			void OnDropFiles(float x, float y, const std::vector<std::string> &paths) {
				if (!mDropFiles)
					return;
				NkDropFileData d;
				d.x = (int)x;
				d.y = (int)y;
				d.numFiles = static_cast<NkU32>(paths.size());
				for (const auto &p : paths) {
					NkDropFilePath fp;
					strncpy(fp.path, p.c_str(), sizeof(fp.path) - 1);
					d.files.push_back(fp);
				}
				mDropFiles(d);
			}

			void OnDropText(float x, float y, const std::string &text) {
				if (!mDropText)
					return;
				NkDropTextData td;
				td.x = (int)x;
				td.y = (int)y;
				td.text = text;
				mDropText(td);
			}

		private:
			DropFilesCallback mDropFiles;
			DropTextCallback mDropText;
			DropEnterCallback mDropEnter;
			DropLeaveCallback mDropLeave;
	};

} // namespace nkentseu
