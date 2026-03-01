#pragma once
// =============================================================================
// NkWASMDropImpl.h — Web Drag & Drop via DataTransfer API (Emscripten)
//
// Attache des callbacks ondragenter, ondragleave, ondragover, ondrop
// sur l'élément canvas identifié par mTargetId.
// Lit les fichiers via FileReader ou URI pour images.
// =============================================================================

#include "../../Events/NkDropEvent.h"
#include <functional>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Class NkWASMDropImpl.
 */
class NkWASMDropImpl {
public:
	using DropFilesCallback = std::function<void(const NkDropFileData &)>;
	using DropTextCallback = std::function<void(const NkDropTextData &)>;
	using DropEnterCallback = std::function<void(const NkDropEnterData &)>;
	using DropLeaveCallback = std::function<void()>;

	explicit NkWASMDropImpl(const char *targetId = "#canvas") : mTargetId(targetId ? targetId : "#canvas") {
#ifdef __EMSCRIPTEN__
		// JS inline pour attacher les callbacks ondrop
		EM_ASM(
			{
				var target = document.querySelector(UTF8ToString($0));
				if (!target)
					return;
				target.addEventListener('dragover', function(e) { e.preventDefault(); });
				target.addEventListener(
					'drop', function(e) {
						e.preventDefault();
						var files = e.dataTransfer.files;
						var paths = [];
						for (var i = 0; i < files.length; i++)
							paths.push(files[i].name);
						Module._NkWASMOnDrop(paths.length, e.clientX, e.clientY);
						var text = e.dataTransfer.getData('text/plain');
						if (text)
							Module._NkWASMOnDropText(text, e.clientX, e.clientY);
					});
				target.addEventListener('dragenter', function(e) { Module._NkWASMOnDragEnter(e.clientX, e.clientY); });
				target.addEventListener('dragleave', function() { Module._NkWASMOnDragLeave(); });
			},
			mTargetId.c_str());
		sInstance = this;
#endif
	}

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

	// Appelé depuis JS via ccall
	static void OnDrop(int numFiles, int x, int y) {
		if (!sInstance || !sInstance->mDropFiles)
			return;
		NkDropFileData d;
		d.x = x;
		d.y = y;
		d.numFiles = static_cast<NkU32>(numFiles);
		// Chemins non accessibles directement en WASM — utiliser File API
		sInstance->mDropFiles(d);
	}

	static void OnDropText(const char *text, int x, int y) {
		if (!sInstance || !sInstance->mDropText)
			return;
		NkDropTextData td;
		td.x = x;
		td.y = y;
		td.text = text ? text : "";
		sInstance->mDropText(td);
	}

	static void OnDragEnter(int x, int y) {
		if (!sInstance || !sInstance->mDropEnter)
			return;
		NkDropEnterData d;
		d.x = x;
		d.y = y;
		sInstance->mDropEnter(d);
	}

	static void OnDragLeave() {
		if (sInstance && sInstance->mDropLeave)
			sInstance->mDropLeave();
	}

private:
	std::string mTargetId;
	DropFilesCallback mDropFiles;
	DropTextCallback mDropText;
	DropEnterCallback mDropEnter;
	DropLeaveCallback mDropLeave;
	static NkWASMDropImpl *sInstance;
};

inline NkWASMDropImpl *NkWASMDropImpl::sInstance = nullptr;

#ifdef __EMSCRIPTEN__
extern "C" {
EMSCRIPTEN_KEEPALIVE void NkWASMOnDrop(int n, int x, int y) {
	NkWASMDropImpl::OnDrop(n, x, y);
}
EMSCRIPTEN_KEEPALIVE void NkWASMOnDropText(const char *t, int x, int y) {
	NkWASMDropImpl::OnDropText(t, x, y);
}
EMSCRIPTEN_KEEPALIVE void NkWASMOnDragEnter(int x, int y) {
	NkWASMDropImpl::OnDragEnter(x, y);
}
EMSCRIPTEN_KEEPALIVE void NkWASMOnDragLeave() {
	NkWASMDropImpl::OnDragLeave();
}
}
#endif

} // namespace nkentseu
