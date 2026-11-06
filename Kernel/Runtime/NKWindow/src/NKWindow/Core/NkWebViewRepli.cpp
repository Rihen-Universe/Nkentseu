// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWebViewRepli.cpp — LA VUE WEB, SANS WEBVIEW2 : Linux, macOS, Android, Web, UWP, Xbox (05/11).
//   Windows desktop : Platform/Win32/NkWin32WebView.cpp. Ici, Disponible rend faux avec son motif et Creer rend nullptr : l'application
//   ouvre le NAVIGATEUR du systeme (le repli). Aucune compilation ne depend de WebView2 hors de Windows desktop.
// =============================================================================
#include "NKWindow/Core/NkWebView.h"
#include "NKPlatform/NkPlatformDetect.h"

#if !(defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX))

namespace nkentseu {

	struct NkWebView::Impl {
			NkString vide;
	};

	NkWebView::NkWebView() : mImpl(new Impl()) {
	}
	NkWebView::~NkWebView() {
		delete mImpl;
	}

	bool NkWebView::Disponible(NkString &motif) {
		motif = NkString("l'apercu integre n'existe que sous Windows (WebView2) : le navigateur du systeme est utilise");
		return false;
	}
	NkWebView *NkWebView::Creer(NkWindow &, int32, int32, int32, int32, const NkString &, NkString &motif) {
		Disponible(motif);
		return nullptr;
	}
	NkWebView::Etat NkWebView::GetEtat() const {
		return Etat::Echec;
	}
	const NkString &NkWebView::Motif() const {
		return mImpl->vide;
	}
	void NkWebView::Naviguer(const NkString &) {
	}
	void NkWebView::Recharger() {
	}
	void NkWebView::Precedent() {
	}
	void NkWebView::Suivant() {
	}
	void NkWebView::Positionner(int32, int32, int32, int32) {
	}
	void NkWebView::Montrer(bool) {
	}
	bool NkWebView::EstVisible() const {
		return false;
	}
	const NkString &NkWebView::Adresse() const {
		return mImpl->vide;
	}
	const NkString &NkWebView::Titre() const {
		return mImpl->vide;
	}
	bool NkWebView::PeutReculer() const {
		return false;
	}
	bool NkWebView::PeutAvancer() const {
		return false;
	}
	bool NkWebView::Chargee() const {
		return false;
	}
	bool NkWebView::EnErreur() const {
		return false;
	}
	int32 NkWebView::Navigations() const {
		return 0;
	}
	void NkWebView::DemanderTitre() {
	}
	bool NkWebView::CapturerPng(const NkString &) {
		return false;
	}
	long NkWebView::CaptureCode() const {
		return 1;
	}
	bool NkWebView::CaptureFaite() const {
		return false;
	}
	void NkWebView::Fermer() {
	}

} // namespace nkentseu

#endif
