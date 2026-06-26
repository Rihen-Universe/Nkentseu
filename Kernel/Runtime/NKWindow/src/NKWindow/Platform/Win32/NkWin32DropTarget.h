#pragma once

// =============================================================================
// NkWin32DropTarget.h
// IDropTarget COM pour Win32 — Drag & Drop OLE (fichiers + texte).
//
// Point 6 appliqué :
//   OleInitialize / OleUninitialize sont retirés de cette classe.
//   Ils sont désormais gérés une seule fois par NkSystem::Initialise()
//   et NkSystem::Close(), avant/après toute création de fenêtre.
//
//   Le constructeur appelle uniquement RegisterDragDrop.
//   Le destructeur appelle uniquement RevokeDragDrop.
//
// Usage :
//   // NkSystem::Initialise() a déjà appelé OleInitialize
//   auto* dt = new NkWin32DropTarget(hwnd);
//   dt->SetDropFileCallback(...);
//   // ...
//   // NkSystem::Close() appellera OleUninitialize
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <functional>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

#include "NKCore/NkAtomic.h"
#include "NKCore/NkTraits.h"
#include "NKContainers/String/NkWString.h"
#include "NKEvent/NkDropEvent.h"

namespace nkentseu {

    class NkWin32DropTarget : public IDropTarget {
		public:
			using DropFileCallback  = NkFunction<void(const NkDropFileEvent&)>;
			using DropTextCallback  = NkFunction<void(const NkDropTextEvent&)>;
			using DropEnterCallback = NkFunction<void(const NkDropEnterEvent&)>;
			using DropLeaveCallback = NkFunction<void(const NkDropLeaveEvent&)>;

			// Point 6 : le constructeur ne touche plus à OLE.
			// OleInitialize a déjà été appelé par NkSystem::Initialise().
			// On enregistre simplement la fenêtre comme cible de drag & drop.
			explicit NkWin32DropTarget(HWND hwnd)
				: mHwnd(hwnd), mRefCount(1)
			{
				// RegisterDragDrop requiert que OLE soit initialisé sur ce thread.
				// C'est garanti par NkSystem::Initialise() qui appelle OleInitialize
				// avant toute création de DropTarget.
				HRESULT hr = RegisterDragDrop(mHwnd, this);
				// hr == S_OK si tout va bien.
				// hr == DRAGDROP_E_ALREADYREGISTERED si la fenêtre est déjà enregistrée.
				(void)hr;
			}

			// Point 6 : le destructeur ne touche plus à OLE.
			// OleUninitialize est appelé par NkSystem::Close().
			~NkWin32DropTarget() {
				RevokeDragDrop(mHwnd);
			}

			// --- Callbacks ---
			void SetDropFileCallback(DropFileCallback cb)   { mDropFile  = traits::NkMove(cb); }
			void SetDropTextCallback(DropTextCallback cb)   { mDropText  = traits::NkMove(cb); }
			void SetDropEnterCallback(DropEnterCallback cb) { mDropEnter = traits::NkMove(cb); }
			void SetDropLeaveCallback(DropLeaveCallback cb) { mDropLeave = traits::NkMove(cb); }

			// =====================================================================
			// IUnknown
			// =====================================================================

			ULONG STDMETHODCALLTYPE AddRef() override {
				return ++mRefCount;
			}

			ULONG STDMETHODCALLTYPE Release() override {
				ULONG r = --mRefCount;
				if (r == 0) delete this;
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

			// =====================================================================
			// IDropTarget
			// =====================================================================

			HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pData,
												DWORD        /*grfKeyState*/,
												POINTL       pt,
												DWORD*       pdwEffect) override
			{
				POINT cp = { pt.x, pt.y };
				ScreenToClient(mHwnd, &cp);

				uint32 numFiles = CountFiles(pData);
				bool  hasText  = HasText(pData);

				NkDropEnterData data{};
				data.x = static_cast<int32>(cp.x);
				data.y = static_cast<int32>(cp.y);
				data.numFiles = numFiles;
				data.hasText = hasText;
				data.hasImage = false;

				NkDropEnterEvent event(data);

				if (mDropEnter) mDropEnter(event);

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
				NkDropLeaveEvent event;
				if (mDropLeave) mDropLeave(event);
				return S_OK;
			}

			HRESULT STDMETHODCALLTYPE Drop(IDataObject* pData,
										DWORD        /*grfKeyState*/,
										POINTL       pt,
										DWORD*       pdwEffect) override
			{
				POINT cp = { pt.x, pt.y };
				ScreenToClient(mHwnd, &cp);
				const int32 dropX = static_cast<int32>(cp.x);
				const int32 dropY = static_cast<int32>(cp.y);

				// --- Fichiers ---
				NkVector<NkString> files = ExtractFiles(pData);
					if (!files.Empty() && mDropFile) {
						NkDropFileData data{};
						data.x = dropX;
						data.y = dropY;
						data.paths = traits::NkMove(files);
						NkDropFileEvent event(data);
						mDropFile(event);
					}

				// --- Texte ---
				NkString text = ExtractText(pData);
					if (!text.Empty() && mDropText) {
						NkDropTextData data{};
						data.x = dropX;
						data.y = dropY;
						data.text = traits::NkMove(text);
						data.mimeType = "text/plain";
						NkDropTextEvent event(data);
						mDropText(event);
					}

				*pdwEffect = DROPEFFECT_COPY;
				return S_OK;
			}

		private:
			HWND               mHwnd;
			NkAtomic<ULONG> mRefCount;

			DropFileCallback  mDropFile;
			DropTextCallback  mDropText;
			DropEnterCallback mDropEnter;
			DropLeaveCallback mDropLeave;

			// =====================================================================
			// Helpers extraction
			// =====================================================================

			static uint32 CountFiles(IDataObject* pData) {
				FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
				STGMEDIUM stg = {};
				if (FAILED(pData->GetData(&fmt, &stg))) return 0;
				HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
				uint32 n = hDrop ? DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0) : 0;
				GlobalUnlock(stg.hGlobal);
				ReleaseStgMedium(&stg);
				return n;
			}

			static NkVector<NkString> ExtractFiles(IDataObject* pData) {
				NkVector<NkString> result;
				FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
				STGMEDIUM stg = {};
				if (FAILED(pData->GetData(&fmt, &stg))) return result;

				HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
				if (hDrop) {
					UINT n = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
					for (UINT i = 0; i < n; ++i) {
						UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
						NkWString ws(len + 1, L'\0');
						DragQueryFileW(hDrop, i, ws.Data(), len + 1);
						ws.Resize(len);
						int sz = WideCharToMultiByte(CP_UTF8, 0, ws.CStr(), -1,
													nullptr, 0, nullptr, nullptr);
						NkString s(sz, '\0');
						WideCharToMultiByte(CP_UTF8, 0, ws.CStr(), -1,
											s.Data(), sz, nullptr, nullptr);
						if (!s.Empty() && s.Back() == '\0') s.PopBack();
						result.PushBack(NkString(s.CStr()));
					}
				}
				GlobalUnlock(stg.hGlobal);
				ReleaseStgMedium(&stg);
				return result;
			}

			static bool HasText(IDataObject* pData) {
				FORMATETC fmt = { CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
				STGMEDIUM stg = {};
				if (FAILED(pData->GetData(&fmt, &stg))) return false;
				ReleaseStgMedium(&stg);
				return true;
			}

			static NkString ExtractText(IDataObject* pData) {
				FORMATETC fmt = { CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
				STGMEDIUM stg = {};
				if (FAILED(pData->GetData(&fmt, &stg))) return {};

				const wchar_t* ws = static_cast<const wchar_t*>(GlobalLock(stg.hGlobal));
				NkString result;
				if (ws) {
					int sz = WideCharToMultiByte(CP_UTF8, 0, ws, -1,
												nullptr, 0, nullptr, nullptr);
					NkString s(sz, '\0');
					WideCharToMultiByte(CP_UTF8, 0, ws, -1,
										s.Data(), sz, nullptr, nullptr);
					if (!s.Empty() && s.Back() == '\0') s.PopBack();
					result = NkString(s.CStr());
				}
				GlobalUnlock(stg.hGlobal);
				ReleaseStgMedium(&stg);
				return result;
			}
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS && !NKENTSEU_PLATFORM_UWP && !NKENTSEU_PLATFORM_XBOX
