// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWin32Fichiers.cpp — LES FICHIERS DANS LE PRESSE-PAPIERS ET DANS UN GLISSER (05/11).
//
// Rodolf (05/11) : « sélection et déplacement multiple doivent aussi fonctionner, pareil pour le déplacement à l'extérieur de
// NKCode ou entre deux instances de NKCode, pareil pour les copier, coller, couper. » Le format de l'Explorateur de Windows :
//   - CF_HDROP (DROPFILES + chemins UTF-16) : la liste des fichiers ;
//   - « Preferred DropEffect » (DROPEFFECT_MOVE) : Couper / Copier ;
//   - un format prive « NKCode.FichiersInterne » : une source NKCode (le depot la DEPLACE par defaut, comme on deplace dans l'arbre).
// Le meme objet de donnees sert au presse-papiers (OleSetClipboard) et au glisser (DoDragDrop). Aucun fichier n'est copie ici :
// la cible (l'Explorateur, une autre instance de NKCode...) lit la liste et agit.
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NkWin32Window.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKContainers/Sequential/NkVector.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <shellapi.h>
#include <cstdlib>
#include <cstring>

namespace nkentseu {

	namespace {

		UINT FormatEffetPrefere() {
			static UINT f = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
			return f;
		}
		UINT FormatNkInterne() {
			static UINT f = RegisterClipboardFormatW(L"NKCode.FichiersInterne");
			return f;
		}

		/// La liste DROPFILES : l'en-tete, puis chaque chemin en UTF-16 (barres obliques inverses), un zero apres chacun, un zero final.
		void ConstruireDropFiles(const NkVector<NkString> &chemins, NkVector<uint8> &octets) {
			NkVector<wchar_t> large;
			for (usize i = 0; i < chemins.Size(); ++i) {
				const int n = MultiByteToWideChar(CP_UTF8, 0, chemins[i].CStr(), -1, nullptr, 0);
				if (n <= 1)
					continue;
				const usize debut = large.Size();
				large.Resize(debut + static_cast<usize>(n));
				MultiByteToWideChar(CP_UTF8, 0, chemins[i].CStr(), -1, large.Data() + debut, n);
				for (usize k = debut; k < large.Size(); ++k)
					if (large[k] == L'/')
						large[k] = L'\\';
			}
			large.PushBack(L'\0'); // le zero final de la liste
			DROPFILES df = {};
			df.pFiles = sizeof(DROPFILES);
			df.fWide = TRUE;
			octets.Resize(sizeof(DROPFILES) + large.Size() * sizeof(wchar_t));
			memcpy(octets.Data(), &df, sizeof(DROPFILES));
			memcpy(octets.Data() + sizeof(DROPFILES), large.Data(), large.Size() * sizeof(wchar_t));
		}

		HGLOBAL CopierEnGlobal(const void *src, usize n) {
			HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, n ? n : 1);
			if (!hg)
				return nullptr;
			void *dst = GlobalLock(hg);
			if (!dst) {
				GlobalFree(hg);
				return nullptr;
			}
			if (n)
				memcpy(dst, src, n);
			GlobalUnlock(hg);
			return hg;
		}

		/// L'objet de donnees : CF_HDROP, « Preferred DropEffect », et, si la source est NKCode, le format prive.
		class NkDonneesFichiers final : public IDataObject {
			public:
				NkDonneesFichiers(const NkVector<NkString> &chemins, DWORD effetPrefere, bool interne)
					: mRef(1), mEffet(effetPrefere), mInterne(interne) {
					ConstruireDropFiles(chemins, mDrop);
				}
				ULONG STDMETHODCALLTYPE AddRef() override {
					return static_cast<ULONG>(InterlockedIncrement(&mRef));
				}
				ULONG STDMETHODCALLTYPE Release() override {
					const LONG r = InterlockedDecrement(&mRef);
					if (r == 0)
						delete this;
					return static_cast<ULONG>(r);
				}
				HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
					if (riid == IID_IUnknown || riid == IID_IDataObject) {
						*ppv = static_cast<IDataObject *>(this);
						AddRef();
						return S_OK;
					}
					*ppv = nullptr;
					return E_NOINTERFACE;
				}
				HRESULT STDMETHODCALLTYPE GetData(FORMATETC *f, STGMEDIUM *m) override {
					if (!f || !m)
						return E_INVALIDARG;
					if (!(f->tymed & TYMED_HGLOBAL))
						return DV_E_TYMED;
					HGLOBAL hg = nullptr;
					if (f->cfFormat == CF_HDROP)
						hg = CopierEnGlobal(mDrop.Data(), mDrop.Size());
					else if (f->cfFormat == FormatEffetPrefere() && mEffet != 0)
						hg = CopierEnGlobal(&mEffet, sizeof(DWORD));
					else if (f->cfFormat == FormatNkInterne() && mInterne) {
						const uint8 un = 1;
						hg = CopierEnGlobal(&un, 1);
					} else
						return DV_E_FORMATETC;
					if (!hg)
						return E_OUTOFMEMORY;
					m->tymed = TYMED_HGLOBAL;
					m->hGlobal = hg;
					m->pUnkForRelease = nullptr;
					return S_OK;
				}
				HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *, STGMEDIUM *) override {
					return E_NOTIMPL;
				}
				HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *f) override {
					if (!f)
						return E_INVALIDARG;
					const bool connu = f->cfFormat == CF_HDROP || (f->cfFormat == FormatEffetPrefere() && mEffet != 0) ||
									   (f->cfFormat == FormatNkInterne() && mInterne);
					if (!connu)
						return DV_E_FORMATETC;
					return (f->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_TYMED;
				}
				HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC *, FORMATETC *sortie) override {
					if (sortie)
						sortie->ptd = nullptr;
					return DATA_S_SAMEFORMATETC;
				}
				HRESULT STDMETHODCALLTYPE SetData(FORMATETC *, STGMEDIUM *, BOOL) override {
					return E_NOTIMPL;
				}
				HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD sens, IEnumFORMATETC **sortie) override {
					if (sens != DATADIR_GET || !sortie)
						return E_NOTIMPL;
					FORMATETC fm[3];
					UINT n = 0;
					fm[n++] = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
					if (mEffet != 0)
						fm[n++] = {static_cast<CLIPFORMAT>(FormatEffetPrefere()), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
					if (mInterne)
						fm[n++] = {static_cast<CLIPFORMAT>(FormatNkInterne()), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
					return SHCreateStdEnumFmtEtc(n, fm, sortie);
				}
				HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *, DWORD, IAdviseSink *, DWORD *) override {
					return OLE_E_ADVISENOTSUPPORTED;
				}
				HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override {
					return OLE_E_ADVISENOTSUPPORTED;
				}
				HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **) override {
					return OLE_E_ADVISENOTSUPPORTED;
				}

			private:
				LONG mRef;
				DWORD mEffet;
				bool mInterne;
				NkVector<uint8> mDrop;
		};

		/// La source du glisser : la souris gauche relachee = deposer ; Echap = annuler ; le curseur du systeme.
		class NkSourceGlisser final : public IDropSource {
			public:
				ULONG STDMETHODCALLTYPE AddRef() override {
					return static_cast<ULONG>(InterlockedIncrement(&mRef));
				}
				ULONG STDMETHODCALLTYPE Release() override {
					const LONG r = InterlockedDecrement(&mRef);
					if (r == 0)
						delete this;
					return static_cast<ULONG>(r);
				}
				HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
					if (riid == IID_IUnknown || riid == IID_IDropSource) {
						*ppv = static_cast<IDropSource *>(this);
						AddRef();
						return S_OK;
					}
					*ppv = nullptr;
					return E_NOINTERFACE;
				}
				HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL echap, DWORD cles) override {
					if (echap)
						return DRAGDROP_S_CANCEL;
					if (!(cles & MK_LBUTTON))
						return DRAGDROP_S_DROP;
					return S_OK;
				}
				HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override {
					return DRAGDROP_S_USEDEFAULTCURSORS;
				}

			private:
				LONG mRef = 1;
		};

	} // namespace

	bool NkWindow::SetClipboardFiles(const NkVector<NkString> &chemins, bool couper) {
		if (chemins.Empty())
			return false;
		NkDonneesFichiers *d = new NkDonneesFichiers(chemins, couper ? DROPEFFECT_MOVE : DROPEFFECT_COPY, true);
		const HRESULT hr = OleSetClipboard(d);
		if (SUCCEEDED(hr))
			OleFlushClipboard(); // les donnees restent au presse-papiers, meme si cet objet part
		d->Release();
		return SUCCEEDED(hr);
	}

	bool NkWindow::GetClipboardFiles(NkVector<NkString> &chemins, bool &couper) const {
		chemins.Clear();
		couper = false;
		IDataObject *pdo = nullptr;
		if (FAILED(OleGetClipboard(&pdo)) || !pdo)
			return false;
		FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		STGMEDIUM stg = {};
		if (SUCCEEDED(pdo->GetData(&fmt, &stg))) {
			HDROP hd = static_cast<HDROP>(GlobalLock(stg.hGlobal));
			if (hd) {
				const UINT n = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
				for (UINT i = 0; i < n; ++i) {
					const UINT len = DragQueryFileW(hd, i, nullptr, 0);
					NkVector<wchar_t> w;
					w.Resize(static_cast<usize>(len) + 1);
					DragQueryFileW(hd, i, w.Data(), len + 1);
					const int taille = WideCharToMultiByte(CP_UTF8, 0, w.Data(), -1, nullptr, 0, nullptr, nullptr);
					if (taille <= 1)
						continue;
					NkVector<char> u;
					u.Resize(static_cast<usize>(taille));
					WideCharToMultiByte(CP_UTF8, 0, w.Data(), -1, u.Data(), taille, nullptr, nullptr);
					chemins.PushBack(NkString(u.Data()));
				}
				GlobalUnlock(stg.hGlobal);
			}
			ReleaseStgMedium(&stg);
		}
		FORMATETC fe = {static_cast<CLIPFORMAT>(FormatEffetPrefere()), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		STGMEDIUM sm = {};
		if (!chemins.Empty() && SUCCEEDED(pdo->GetData(&fe, &sm))) {
			const DWORD *e = static_cast<const DWORD *>(GlobalLock(sm.hGlobal));
			if (e) {
				couper = ((*e & DROPEFFECT_MOVE) != 0) && ((*e & DROPEFFECT_COPY) == 0);
				GlobalUnlock(sm.hGlobal);
			}
			ReleaseStgMedium(&sm);
		}
		pdo->Release();
		return !chemins.Empty();
	}

	bool NkWindow::BeginFileDrag(const NkVector<NkString> &chemins, bool autoriserDeplacer, int32 &effet) {
		effet = 0;
		if (chemins.Empty())
			return false;
		NkDonneesFichiers *d = new NkDonneesFichiers(chemins, 0, true);
		NkSourceGlisser *src = new NkSourceGlisser();
		DWORD obtenu = DROPEFFECT_NONE;
		const HRESULT hr =
			DoDragDrop(d, src, autoriserDeplacer ? (DROPEFFECT_COPY | DROPEFFECT_MOVE) : DROPEFFECT_COPY, &obtenu);
		d->Release();
		src->Release();
		if (hr == DRAGDROP_S_DROP)
			effet = (obtenu & DROPEFFECT_MOVE) ? 2 : ((obtenu & DROPEFFECT_COPY) ? 1 : 0);
		return hr == DRAGDROP_S_DROP || hr == DRAGDROP_S_CANCEL;
	}

	void NkWindow::SetMaxButtonRect(int32 x, int32 y, int32 w, int32 h) {
		// Contre-epreuve NK_NKCODE_MUTATION=snap-sans-bouton : le systeme n'est pas prevenu (l'etat d'avant).
		const char *mu = std::getenv("NK_NKCODE_MUTATION");
		if (mu && std::strcmp(mu, "snap-sans-bouton") == 0)
			w = 0;
		mData.mMaxBtn[0] = x;
		mData.mMaxBtn[1] = y;
		mData.mMaxBtn[2] = w;
		mData.mMaxBtn[3] = h;
		mData.mMaxBtnValid = (w > 0 && h > 0);
	}

	int32 NkWindow::NativeHitTest(int32 x, int32 y) const {
		POINT p = {x, y};
		ClientToScreen(mData.mHwnd, &p);
		return static_cast<int32>(SendMessageW(mData.mHwnd, WM_NCHITTEST, 0, MAKELPARAM((short)p.x, (short)p.y)));
	}

} // namespace nkentseu

#endif // desktop Win32
