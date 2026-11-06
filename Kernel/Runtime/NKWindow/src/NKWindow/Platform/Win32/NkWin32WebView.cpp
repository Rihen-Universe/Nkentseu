// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWin32WebView.cpp — WEBVIEW2 SOUS WINDOWS, derriere NkWebView (05/11).
//
// Le SDK (en-tete WebView2.h, BSD-3, third_party/WebView2) donne les interfaces COM ; la BIBLIOTHEQUE n'est PAS liee : WebView2Loader.dll
// se charge par LoadLibrary (a cote de l'executable, ou par NK_WEBVIEW2_LOADER). Rien de Windows ni de COM ne sort de ce fichier.
//
// UN ETAT PARTAGE COMPTE (Impl) : chaque rappel COM tient une reference. Fermer() le marque mort et relache la vue ; un rappel qui
// arrive apres ne touche plus rien.
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NKWindow/Core/NkWebView.h"
#include "NKWindow/Core/NkWindow.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <shlwapi.h>
// L'en-tete du SDK (MIDL) porte des « #pragma warning » que Clang ne connait pas : zero avertissement, sans toucher a l'en-tete livre.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#endif
#include "../../../../third_party/WebView2/WebView2.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace nkentseu {

	namespace {

		/// Un pointeur COM minimal (WRL tire des classes de la bibliotheque standard qui se heurtent a la liaison statique de libstdc++).
		template <typename T> class NkComPtr {
			public:
				NkComPtr() = default;
				NkComPtr(const NkComPtr &) = delete;
				NkComPtr &operator=(const NkComPtr &) = delete;
				~NkComPtr() {
					Reset();
				}
				NkComPtr &operator=(T *p) { // prend UNE reference de plus
					if (p)
						p->AddRef();
					Reset();
					mP = p;
					return *this;
				}
				void Attach(T *p) { // prend possession (sans AddRef)
					Reset();
					mP = p;
				}
				void Reset() {
					if (mP) {
						mP->Release();
						mP = nullptr;
					}
				}
				T **operator&() { // pour les get_xxx(T **) : relache l'ancien
					Reset();
					return &mP;
				}
				T *operator->() const {
					return mP;
				}
				explicit operator bool() const {
					return mP != nullptr;
				}
				T *Get() const {
					return mP;
				}

			private:
				T *mP = nullptr;
		};

		using FnCreerEnv = HRESULT(STDAPICALLTYPE *)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *,
													 ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);
		using FnVersion = HRESULT(STDAPICALLTYPE *)(PCWSTR, LPWSTR *);

		NkString VersLocal(const wchar_t *w) {
			if (!w)
				return NkString();
			const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
			if (n <= 1)
				return NkString();
			NkVector<char> t;
			t.Resize(static_cast<usize>(n));
			WideCharToMultiByte(CP_UTF8, 0, w, -1, t.Data(), n, nullptr, nullptr);
			return NkString(t.Data());
		}

		NkVector<wchar_t> VersLarge(const NkString &s) {
			NkVector<wchar_t> w;
			const int n = MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, nullptr, 0);
			if (n <= 0) {
				w.PushBack(L'\0');
				return w;
			}
			w.Resize(static_cast<usize>(n));
			MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, w.Data(), n);
			return w;
		}

		/// Le chargeur : NK_WEBVIEW2_LOADER (meme invalide : c'est ce que le banc exige), sinon a cote de l'executable, et RIEN d'autre.
		/// Jamais un nom nu : LoadLibrary suivrait l'ordre de recherche de Windows (dossier courant, PATH), et dans un projet recu ce serait la
		/// DLL DU PROJET qui se chargerait (execution de code). La DLL n'est pas du systeme : ce repli n'a aucun usage legitime.
		HMODULE ChargerLoader(NkString &motif) {
			wchar_t env[1024] = {};
			if (GetEnvironmentVariableW(L"NK_WEBVIEW2_LOADER", env, 1024) > 0) {
				HMODULE h = LoadLibraryW(env);
				if (!h)
					motif = NkString("WebView2Loader.dll introuvable (NK_WEBVIEW2_LOADER : ") + VersLocal(env).CStr() + ")";
				return h;
			}
			wchar_t exe[MAX_PATH] = {};
			if (GetModuleFileNameW(nullptr, exe, MAX_PATH) > 0) {
				wchar_t *p = wcsrchr(exe, L'\\');
				if (p) {
					*(p + 1) = L'\0';
					wcscat_s(exe, L"WebView2Loader.dll");
					HMODULE h = LoadLibraryW(exe);
					if (h)
						return h;
				}
			}
			// Contre-epreuve du banc (NK_NKCODE_MUTATION=chargeur-nom-nu) : l'ancien repli par nom nu, pour prouver que le banc le voit.
			wchar_t mut[64] = {};
			HMODULE h = nullptr;
			if (GetEnvironmentVariableW(L"NK_NKCODE_MUTATION", mut, 64) > 0 && wcscmp(mut, L"chargeur-nom-nu") == 0)
				h = LoadLibraryW(L"WebView2Loader.dll");
			if (!h)
				motif = NkString("WebView2Loader.dll est introuvable (elle doit etre a cote de NKCode.exe)");
			return h;
		}

	} // namespace

	// ── l'etat partage ──────────────────────────────────────────────────────────────
	struct NkWebView::Impl {
			LONG ref = 1;
			bool vivant = true;
			HWND parent = nullptr;
			HMODULE loader = nullptr;
			NkComPtr<ICoreWebView2Environment> env;
			NkComPtr<ICoreWebView2Controller> controleur;
			NkComPtr<ICoreWebView2> vue;
			EventRegistrationToken jetonNav = {}, jetonSource = {}, jetonTitre = {};
			Etat etat = Etat::Creation;
			NkString motif, adresse, titre, urlEnAttente, captureChemin;
			RECT bornes = {0, 0, 0, 0};
			bool visible = true, charge = false, erreur = false, reculer = false, avancer = false, captureFaite = false;
			long captureCode = 1; // HRESULT rendu par la capture (1 : pas encore de reponse)
			int32 navigations = 0;
			NkComPtr<IStream> flux; ///< la capture en cours

			void AddRef() {
				InterlockedIncrement(&ref);
			}
			void Release() {
				if (InterlockedDecrement(&ref) == 0)
					delete this;
			}
			void Majuscule() { // met a jour ce qui se lit apres une navigation
				if (!vue)
					return;
				BOOL b = FALSE;
				if (SUCCEEDED(vue->get_CanGoBack(&b)))
					reculer = b != FALSE;
				if (SUCCEEDED(vue->get_CanGoForward(&b)))
					avancer = b != FALSE;
				LPWSTR s = nullptr;
				if (SUCCEEDED(vue->get_Source(&s)) && s) {
					adresse = VersLocal(s);
					CoTaskMemFree(s);
				}
			}
			void Appliquer() {
				if (!controleur)
					return;
				controleur->put_Bounds(bornes);
				controleur->put_IsVisible(visible ? TRUE : FALSE);
			}
	};

	namespace {

		// ── les rappels COM : IUnknown minimal, une reference a l'etat partage ───────────
// MinGW : MIDL_INTERFACE n'attache aucun uuid (pas de __uuidof) et les IID_* ne sont definis nulle part : les IID des rappels viennent de
// WebView2.h (MIDL_INTERFACE("...")), ecrits ici.
		const GUID kIidControleur = {0x6c4819f3, 0xc9b7, 0x4260, {0x81, 0x27, 0xc9, 0xf5, 0xbd, 0xe7, 0xf6, 0x8c}};
		const GUID kIidEnvironnement = {0x4e8a3389, 0xc9d8, 0x4bd2, {0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d}};
		const GUID kIidNavigation = {0xd33a35bf, 0x1c49, 0x4f98, {0x93, 0xab, 0x00, 0x6e, 0x05, 0x33, 0xfe, 0x1c}};
		const GUID kIidSource = {0x3c067f9f, 0x5388, 0x4772, {0x8b, 0x48, 0x79, 0xf7, 0xef, 0x1a, 0xb3, 0x7c}};
		const GUID kIidTitre = {0xf5f2b923, 0x953e, 0x4042, {0x9f, 0x95, 0xf3, 0xa1, 0x18, 0xe1, 0xaf, 0xd4}};
		const GUID kIidScript = {0x49511172, 0xcc67, 0x4bca, {0x99, 0x23, 0x13, 0x71, 0x12, 0xf4, 0xc4, 0xcc}};
		const GUID kIidCapture = {0x697e05e9, 0x3d8f, 0x45fa, {0x96, 0xf4, 0x8f, 0xfe, 0x1e, 0xde, 0xda, 0xf5}};

#define NK_WV_UNKNOWN(Interface, iid)                                                                                 \
	ULONG STDMETHODCALLTYPE AddRef() override {                                                                      \
		return static_cast<ULONG>(InterlockedIncrement(&mRef));                                                      \
	}                                                                                                                \
	ULONG STDMETHODCALLTYPE Release() override {                                                                     \
		const LONG r = InterlockedDecrement(&mRef);                                                                  \
		if (r == 0)                                                                                                  \
			delete this;                                                                                             \
		return static_cast<ULONG>(r);                                                                                \
	}                                                                                                                \
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {                                     \
		if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, iid)) {                                                   \
			*ppv = static_cast<Interface *>(this);                                                                   \
			AddRef();                                                                                                \
			return S_OK;                                                                                             \
		}                                                                                                            \
		*ppv = nullptr;                                                                                              \
		return E_NOINTERFACE;                                                                                        \
	}

		using ImplT = NkWebView::Impl;

		class RappelControleur final : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
			public:
				explicit RappelControleur(ImplT *i) : mI(i) {
					mI->AddRef();
				}
				~RappelControleur() {
					mI->Release();
				}
				NK_WV_UNKNOWN(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, kIidControleur)
				HRESULT STDMETHODCALLTYPE Invoke(HRESULT code, ICoreWebView2Controller *c) override;

			private:
				LONG mRef = 1;
				ImplT *mI;
		};

		class RappelEnvironnement final : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
			public:
				explicit RappelEnvironnement(ImplT *i) : mI(i) {
					mI->AddRef();
				}
				~RappelEnvironnement() {
					mI->Release();
				}
				NK_WV_UNKNOWN(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, kIidEnvironnement)
				HRESULT STDMETHODCALLTYPE Invoke(HRESULT code, ICoreWebView2Environment *e) override {
					if (!mI->vivant)
						return S_OK;
					if (FAILED(code) || !e) {
						mI->etat = NkWebView::Etat::Echec;
						mI->motif = NkString("Le runtime WebView2 n'a pas pu demarrer (environnement refuse)");
						return S_OK;
					}
					mI->env = e;
					RappelControleur *h = new RappelControleur(mI);
					const HRESULT hr = e->CreateCoreWebView2Controller(mI->parent, h);
					h->Release();
					if (FAILED(hr)) {
						mI->etat = NkWebView::Etat::Echec;
						mI->motif = NkString("La vue WebView2 n'a pas pu etre creee");
					}
					return S_OK;
				}

			private:
				LONG mRef = 1;
				ImplT *mI;
		};

		class RappelNavigation final : public ICoreWebView2NavigationCompletedEventHandler {
			public:
				explicit RappelNavigation(ImplT *i) : mI(i) {
					mI->AddRef();
				}
				~RappelNavigation() {
					mI->Release();
				}
				NK_WV_UNKNOWN(ICoreWebView2NavigationCompletedEventHandler, kIidNavigation)
				HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *a) override {
					if (!mI->vivant)
						return S_OK;
					BOOL ok = FALSE;
					if (a)
						a->get_IsSuccess(&ok);
					mI->charge = true;
					mI->erreur = (ok == FALSE);
					++mI->navigations;
					mI->Majuscule();
					return S_OK;
				}

			private:
				LONG mRef = 1;
				ImplT *mI;
		};

		class RappelSource final : public ICoreWebView2SourceChangedEventHandler {
			public:
				explicit RappelSource(ImplT *i) : mI(i) {
					mI->AddRef();
				}
				~RappelSource() {
					mI->Release();
				}
				NK_WV_UNKNOWN(ICoreWebView2SourceChangedEventHandler, kIidSource)
				HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *, ICoreWebView2SourceChangedEventArgs *) override {
					if (mI->vivant)
						mI->Majuscule();
					return S_OK;
				}

			private:
				LONG mRef = 1;
				ImplT *mI;
		};

		class RappelTitre final : public ICoreWebView2DocumentTitleChangedEventHandler {
			public:
				explicit RappelTitre(ImplT *i) : mI(i) {
					mI->AddRef();
				}
				~RappelTitre() {
					mI->Release();
				}
				NK_WV_UNKNOWN(ICoreWebView2DocumentTitleChangedEventHandler, kIidTitre)
				HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *v, IUnknown *) override {
					if (!mI->vivant || !v)
						return S_OK;
					LPWSTR t = nullptr;
					if (SUCCEEDED(v->get_DocumentTitle(&t)) && t) {
						mI->titre = VersLocal(t);
						CoTaskMemFree(t);
					}
					return S_OK;
				}

			private:
				LONG mRef = 1;
				ImplT *mI;
		};

		class RappelScript final : public ICoreWebView2ExecuteScriptCompletedHandler {
			public:
				explicit RappelScript(ImplT *i) : mI(i) {
					mI->AddRef();
				}
				~RappelScript() {
					mI->Release();
				}
				NK_WV_UNKNOWN(ICoreWebView2ExecuteScriptCompletedHandler, kIidScript)
				HRESULT STDMETHODCALLTYPE Invoke(HRESULT code, LPCWSTR json) override {
					if (!mI->vivant || FAILED(code) || !json)
						return S_OK;
					NkString s = VersLocal(json); // le resultat est du JSON : « "titre" »
					if (s.Length() >= 2 && s.CStr()[0] == '"' && s.CStr()[s.Length() - 1] == '"')
						s = s.SubStr(1, s.Length() - 2);
					mI->titre = s;
					return S_OK;
				}

			private:
				LONG mRef = 1;
				ImplT *mI;
		};

		class RappelCapture final : public ICoreWebView2CapturePreviewCompletedHandler {
			public:
				explicit RappelCapture(ImplT *i) : mI(i) {
					mI->AddRef();
				}
				~RappelCapture() {
					mI->Release();
				}
				NK_WV_UNKNOWN(ICoreWebView2CapturePreviewCompletedHandler, kIidCapture)
				HRESULT STDMETHODCALLTYPE Invoke(HRESULT code) override {
					mI->captureCode = (long)code;
					if (mI->vivant && SUCCEEDED(code) && mI->flux) {
						LARGE_INTEGER zero = {};
						mI->flux->Seek(zero, STREAM_SEEK_SET, nullptr);
						const NkVector<wchar_t> chemin = VersLarge(mI->captureChemin);
						HANDLE f = CreateFileW(chemin.Data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
						if (f != INVALID_HANDLE_VALUE) {
							char tampon[8192];
							ULONG lu = 0;
							while (SUCCEEDED(mI->flux->Read(tampon, sizeof(tampon), &lu)) && lu > 0) {
								DWORD ecrit = 0;
								WriteFile(f, tampon, lu, &ecrit, nullptr);
							}
							CloseHandle(f);
							mI->captureFaite = true;
						}
					}
					if (mI->vivant)
						mI->flux.Reset();
					return S_OK;
				}

			private:
				LONG mRef = 1;
				ImplT *mI;
		};

		HRESULT STDMETHODCALLTYPE RappelControleur::Invoke(HRESULT code, ICoreWebView2Controller *c) {
			if (!mI->vivant)
				return S_OK;
			if (FAILED(code) || !c) {
				mI->etat = NkWebView::Etat::Echec;
				mI->motif = NkString("La vue WebView2 n'a pas pu etre creee (controleur refuse)");
				return S_OK;
			}
			mI->controleur = c;
			c->get_CoreWebView2(&mI->vue);
			mI->Appliquer();
			if (mI->vue) {
				RappelNavigation *n = new RappelNavigation(mI);
				mI->vue->add_NavigationCompleted(n, &mI->jetonNav);
				n->Release();
				RappelSource *s = new RappelSource(mI);
				mI->vue->add_SourceChanged(s, &mI->jetonSource);
				s->Release();
				RappelTitre *t = new RappelTitre(mI);
				mI->vue->add_DocumentTitleChanged(t, &mI->jetonTitre);
				t->Release();
			}
			mI->etat = NkWebView::Etat::Pret;
			if (!mI->urlEnAttente.Empty() && mI->vue) {
				const NkVector<wchar_t> u = VersLarge(mI->urlEnAttente);
				mI->charge = false;
				mI->vue->Navigate(u.Data());
				mI->urlEnAttente = NkString();
			}
			return S_OK;
		}

	} // namespace

	// ── l'interface ─────────────────────────────────────────────────────────────────
	NkWebView::NkWebView() : mImpl(new Impl()) {
	}

	NkWebView::~NkWebView() {
		Fermer();
		if (mImpl) {
			mImpl->Release();
			mImpl = nullptr;
		}
	}

	bool NkWebView::Disponible(NkString &motif) {
		NkString m;
		HMODULE h = ChargerLoader(m);
		if (!h) {
			motif = m;
			return false;
		}
		FnVersion fn = reinterpret_cast<FnVersion>(GetProcAddress(h, "GetAvailableCoreWebView2BrowserVersionString"));
		bool ok = false;
		if (fn) {
			LPWSTR v = nullptr;
			if (SUCCEEDED(fn(nullptr, &v)) && v) {
				ok = true;
				CoTaskMemFree(v);
			}
		}
		FreeLibrary(h);
		if (!ok)
			motif = NkString("Le runtime WebView2 d'Edge n'est pas installe (Evergreen)");
		return ok;
	}

	NkWebView *NkWebView::Creer(NkWindow &fenetre, int32 x, int32 y, int32 w, int32 h, const NkString &dossierDonnees,
								NkString &motif) {
		NkString m;
		HMODULE loader = ChargerLoader(m);
		if (!loader) {
			motif = m;
			return nullptr;
		}
		FnCreerEnv creer = reinterpret_cast<FnCreerEnv>(GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions"));
		HWND parent = static_cast<HWND>(fenetre.NativeWindowHandle());
		if (!creer || !parent) {
			FreeLibrary(loader);
			motif = NkString("WebView2Loader.dll ne porte pas CreateCoreWebView2EnvironmentWithOptions, ou la fenetre n'a pas de poignee");
			return nullptr;
		}
		NkWebView *v = new NkWebView();
		v->mImpl->parent = parent;
		v->mImpl->loader = loader;
		v->mImpl->bornes = {x, y, x + w, y + h};
		const NkVector<wchar_t> dossier = VersLarge(dossierDonnees);
		if (dossier.Size() > 1)
			CreateDirectoryW(dossier.Data(), nullptr);
		RappelEnvironnement *r = new RappelEnvironnement(v->mImpl);
		const HRESULT hr = creer(nullptr, dossier.Size() > 1 ? dossier.Data() : nullptr, nullptr, r);
		r->Release();
		if (FAILED(hr)) {
			motif = NkString("Le runtime WebView2 d'Edge est absent ou refuse de demarrer");
			delete v;
			return nullptr;
		}
		return v;
	}

	NkWebView::Etat NkWebView::GetEtat() const {
		return mImpl->etat;
	}
	const NkString &NkWebView::Motif() const {
		return mImpl->motif;
	}
	const NkString &NkWebView::Adresse() const {
		return mImpl->adresse;
	}
	const NkString &NkWebView::Titre() const {
		return mImpl->titre;
	}
	bool NkWebView::PeutReculer() const {
		return mImpl->reculer;
	}
	bool NkWebView::PeutAvancer() const {
		return mImpl->avancer;
	}
	bool NkWebView::Chargee() const {
		return mImpl->charge;
	}
	bool NkWebView::EnErreur() const {
		return mImpl->erreur;
	}
	int32 NkWebView::Navigations() const {
		return mImpl->navigations;
	}
	bool NkWebView::EstVisible() const {
		return mImpl->visible;
	}
	long NkWebView::CaptureCode() const {
		return mImpl->captureCode;
	}
	bool NkWebView::CaptureFaite() const {
		return mImpl->captureFaite;
	}

	void NkWebView::Naviguer(const NkString &url) {
		mImpl->adresse = url;
		mImpl->charge = false;
		mImpl->erreur = false;
		if (mImpl->vue) {
			const NkVector<wchar_t> u = VersLarge(url);
			mImpl->vue->Navigate(u.Data());
		} else
			mImpl->urlEnAttente = url;
	}
	void NkWebView::Recharger() {
		if (mImpl->vue) {
			mImpl->charge = false;
			mImpl->vue->Reload();
		}
	}
	void NkWebView::Precedent() {
		if (mImpl->vue && mImpl->reculer)
			mImpl->vue->GoBack();
	}
	void NkWebView::Suivant() {
		if (mImpl->vue && mImpl->avancer)
			mImpl->vue->GoForward();
	}
	void NkWebView::Positionner(int32 x, int32 y, int32 w, int32 h) {
		mImpl->bornes = {x, y, x + w, y + h};
		mImpl->Appliquer();
	}
	void NkWebView::Montrer(bool visible) {
		mImpl->visible = visible;
		mImpl->Appliquer();
	}
	void NkWebView::DemanderTitre() {
		if (!mImpl->vue)
			return;
		RappelScript *s = new RappelScript(mImpl);
		mImpl->vue->ExecuteScript(L"document.title", s);
		s->Release();
	}
	bool NkWebView::CapturerPng(const NkString &chemin) {
		if (!mImpl->vue)
			return false;
		mImpl->captureChemin = chemin;
		mImpl->captureFaite = false;
		IStream *flux = SHCreateMemStream(nullptr, 0);
		if (!flux)
			return false;
		mImpl->flux.Attach(flux);
		RappelCapture *c = new RappelCapture(mImpl);
		const HRESULT hr = mImpl->vue->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG, flux, c);
		c->Release();
		return SUCCEEDED(hr);
	}

	void NkWebView::Fermer() {
		if (!mImpl || !mImpl->vivant)
			return;
		mImpl->vivant = false; // plus aucun rappel ne touche cette vue
		if (mImpl->vue) {
			mImpl->vue->remove_NavigationCompleted(mImpl->jetonNav);
			mImpl->vue->remove_SourceChanged(mImpl->jetonSource);
			mImpl->vue->remove_DocumentTitleChanged(mImpl->jetonTitre);
		}
		if (mImpl->controleur)
			mImpl->controleur->Close();
		mImpl->vue.Reset();
		mImpl->controleur.Reset();
		mImpl->env.Reset();
		mImpl->flux.Reset();
		// le chargeur reste charge : le runtime peut avoir encore des fils a terminer ; il part avec le processus
		mImpl->loader = nullptr;
	}

} // namespace nkentseu

#endif // desktop Win32
