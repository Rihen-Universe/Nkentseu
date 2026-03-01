#pragma once

// =============================================================================
// NkWin32DropTarget.h
// IDropTarget COM pour Win32 — Drag & Drop OLE (fichiers + texte).
//
// Enregistré via RegisterDragDrop(hwnd, &target) dans
// NkWin32EventImpl::Initialize si config.dropEnabled == true.
//
// Les événements émis sont les classes définies dans NkDropEvent.h :
//   NkDropEnterEvent   — objet glissé entrant dans la fenêtre
//   NkDropLeaveEvent   — objet quittant sans dépôt
//   NkDropFileEvent    — fichier(s) déposé(s)
//   NkDropTextEvent    — texte UTF-8 déposé
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <functional>
#include <atomic>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

// Fichier 9 — hiérarchie complète des événements Drop
#include "../../Events/NkDropEvent.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	// ---------------------------------------------------------------------------

	class NkWin32DropTarget : public IDropTarget {
	public:
		// Callbacks utilisant les classes événements de NkDropEvent.h (fichier 9)
		using DropFileCallback  = std::function<void(const NkDropFileEvent&)>;
		using DropTextCallback  = std::function<void(const NkDropTextEvent&)>;
		using DropEnterCallback = std::function<void(const NkDropEnterEvent&)>;
		using DropLeaveCallback = std::function<void(const NkDropLeaveEvent&)>;

		explicit NkWin32DropTarget(HWND hwnd) : mHwnd(hwnd), mRefCount(1) {
			OleInitialize(nullptr);
			RegisterDragDrop(mHwnd, this);
		}

		~NkWin32DropTarget() {
			RevokeDragDrop(mHwnd);
			OleUninitialize();
		}

		// Callbacks appelés sur les événements drop
		void SetDropFileCallback(DropFileCallback cb)   { mDropFile  = std::move(cb); }
		void SetDropTextCallback(DropTextCallback cb)   { mDropText  = std::move(cb); }
		void SetDropEnterCallback(DropEnterCallback cb) { mDropEnter = std::move(cb); }
		void SetDropLeaveCallback(DropLeaveCallback cb) { mDropLeave = std::move(cb); }

		// -----------------------------------------------------------------------
		// IUnknown
		// -----------------------------------------------------------------------

		ULONG STDMETHODCALLTYPE AddRef() override {
			return ++mRefCount;
		}

		ULONG STDMETHODCALLTYPE Release() override {
			ULONG r = --mRefCount;
			if (r == 0)
				delete this;
			return r;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
			if (riid == IID_IUnknown || riid == IID_IDropTarget) {
				*ppv = static_cast<IDropTarget*>(this);
				AddRef();
				return S_OK;
			}
			*ppv = nullptr;
			return E_NOINTERFACE;
		}

		// -----------------------------------------------------------------------
		// IDropTarget
		// -----------------------------------------------------------------------

		HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pData,
											DWORD        /*grfKeyState*/,
											POINTL       pt,
											DWORD*       pdwEffect) override
		{
			POINT cp = { pt.x, pt.y };
			ScreenToClient(mHwnd, &cp);

			NkU32 numFiles = CountFiles(pData);
			bool  hasText  = HasText(pData);

			NkDropType dropType = numFiles > 0 ? NkDropType::NK_DROP_TYPE_FILE
								: hasText      ? NkDropType::NK_DROP_TYPE_TEXT
											: NkDropType::NK_DROP_TYPE_UNKNOWN;

			// Construire NkDropEnterEvent (fichier 9)
			NkDropEnterEvent event(
				static_cast<NkI32>(cp.x),
				static_cast<NkI32>(cp.y),
				dropType,
				numFiles,
				hasText,
				/*hasImage=*/false);

			if (mDropEnter)
				mDropEnter(event);

			*pdwEffect = DROPEFFECT_COPY;
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE DragOver(DWORD  /*grfKeyState*/,
											POINTL /*pt*/,
											DWORD* pdwEffect) override
		{
			*pdwEffect = DROPEFFECT_COPY;
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE DragLeave() override {
			// Construire NkDropLeaveEvent (fichier 9)
			NkDropLeaveEvent event;
			if (mDropLeave)
				mDropLeave(event);
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE Drop(IDataObject* pData,
										DWORD        /*grfKeyState*/,
										POINTL       pt,
										DWORD*       pdwEffect) override
		{
			POINT cp = { pt.x, pt.y };
			ScreenToClient(mHwnd, &cp);
			const NkI32 dropX = static_cast<NkI32>(cp.x);
			const NkI32 dropY = static_cast<NkI32>(cp.y);

			// --- Fichiers → NkDropFileEvent (fichier 9) ---
			std::vector<std::string> files = ExtractFiles(pData);
			if (!files.empty() && mDropFile) {
				NkDropFileEvent event(std::move(files), dropX, dropY);
				mDropFile(event);
			}

			// --- Texte → NkDropTextEvent (fichier 9) ---
			std::string text = ExtractText(pData);
			if (!text.empty() && mDropText) {
				NkDropTextEvent event(std::move(text), "text/plain", dropX, dropY);
				mDropText(event);
			}

			*pdwEffect = DROPEFFECT_COPY;
			return S_OK;
		}

	private:
		HWND mHwnd;
		std::atomic<ULONG> mRefCount;

		DropFileCallback  mDropFile;
		DropTextCallback  mDropText;
		DropEnterCallback mDropEnter;
		DropLeaveCallback mDropLeave;

		// -----------------------------------------------------------------------
		// Helpers extraction
		// -----------------------------------------------------------------------

		static NkU32 CountFiles(IDataObject* pData) {
			FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
			STGMEDIUM stg = {};
			if (FAILED(pData->GetData(&fmt, &stg)))
				return 0;
			HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
			NkU32 n = hDrop ? DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0) : 0;
			GlobalUnlock(stg.hGlobal);
			ReleaseStgMedium(&stg);
			return n;
		}

		static std::vector<std::string> ExtractFiles(IDataObject* pData) {
			std::vector<std::string> result;
			FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
			STGMEDIUM stg = {};
			if (FAILED(pData->GetData(&fmt, &stg)))
				return result;

			HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
			if (hDrop) {
				UINT n = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
				for (UINT i = 0; i < n; ++i) {
					UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
					std::wstring ws(len + 1, L'\0');
					DragQueryFileW(hDrop, i, ws.data(), len + 1);
					ws.resize(len);
					// Convertir en UTF-8
					int sz = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1,
												nullptr, 0, nullptr, nullptr);
					std::string s(sz, '\0');
					WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1,
										s.data(), sz, nullptr, nullptr);
					if (!s.empty() && s.back() == '\0')
						s.pop_back();
					result.push_back(std::move(s));
				}
			}
			GlobalUnlock(stg.hGlobal);
			ReleaseStgMedium(&stg);
			return result;
		}

		static bool HasText(IDataObject* pData) {
			FORMATETC fmt = { CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
			STGMEDIUM stg = {};
			if (FAILED(pData->GetData(&fmt, &stg)))
				return false;
			ReleaseStgMedium(&stg);
			return true;
		}

		static std::string ExtractText(IDataObject* pData) {
			FORMATETC fmt = { CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
			STGMEDIUM stg = {};
			if (FAILED(pData->GetData(&fmt, &stg)))
				return {};

			const wchar_t* ws = static_cast<const wchar_t*>(GlobalLock(stg.hGlobal));
			std::string result;
			if (ws) {
				int sz = WideCharToMultiByte(CP_UTF8, 0, ws, -1,
											nullptr, 0, nullptr, nullptr);
				result.resize(sz);
				WideCharToMultiByte(CP_UTF8, 0, ws, -1,
									result.data(), sz, nullptr, nullptr);
				if (!result.empty() && result.back() == '\0')
					result.pop_back();
			}
			GlobalUnlock(stg.hGlobal);
			ReleaseStgMedium(&stg);
			return result;
		}
	};

} // namespace nkentseu