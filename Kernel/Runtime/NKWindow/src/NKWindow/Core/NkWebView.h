// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkWebView.h — UNE VUE WEB DANS UNE FENETRE (05/11).
//
// Rodolf : « de tel sorte qu'on puisse voir le resultat sur NKCode sans ouvrir un navigateur ». Windows : WebView2 (Edge, le
// runtime Evergreen deja present sur Windows 11), une fenetre FILLE de la fenetre de l'application. Les autres plateformes : pas de
// vue (Disponible rend faux, avec son motif), l'appelant ouvre le navigateur.
//
// L'interface ne montre AUCUN type Windows ni COM : tout vit dans Platform/Win32/NkWin32WebView.cpp.
//
// LE CHARGEUR : WebView2Loader.dll n'est PAS lie, il est charge par LoadLibrary (a cote de l'executable, ou NK_WEBVIEW2_LOADER). Absent,
// ou runtime absent : Disponible rend faux avec un message clair, Creer rend nullptr, jamais un plantage.
//
// LA CREATION EST ASYNCHRONE : Creer rend tout de suite ; l'etat passe de Creation a Pret (ou Echec) quand la boucle de messages de
// l'application a servi les rappels (elle les sert deja a chaque image).
//
// UNE FENETRE FILLE RECOUVRE LE RENDU de l'application : l'appelant la cache (Montrer(false)) des qu'un menu, une modale ou un autre
// panneau passe par-dessus, et la repositionne (Positionner) a chaque image.
// =============================================================================
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

	class NkWindow;

	class NkWebView {
		public:
			enum class Etat : int32 { Creation = 0, Pret = 1, Echec = -1 };

			/// Une vue est-elle possible ici ? Faux : `motif` dit pourquoi (chargeur absent, runtime absent, plateforme).
			static bool Disponible(NkString &motif);

			/// Cree la vue, fille de `fenetre`, au rectangle client donne. nullptr (avec `motif`) si la plateforme ne sait pas.
			/// `dossierDonnees` : le dossier du profil de la vue (cache, cookies) ; il est cree s'il manque.
			static NkWebView *Creer(NkWindow &fenetre, int32 x, int32 y, int32 w, int32 h, const NkString &dossierDonnees,
									NkString &motif);

			~NkWebView();
			NkWebView(const NkWebView &) = delete;
			NkWebView &operator=(const NkWebView &) = delete;

			Etat GetEtat() const;
			const NkString &Motif() const; ///< Echec : pourquoi

			void Naviguer(const NkString &url); ///< avant Pret : retenue, jouee des que la vue est prete
			void Recharger();
			void Precedent();
			void Suivant();
			void Positionner(int32 x, int32 y, int32 w, int32 h);
			void Montrer(bool visible);
			bool EstVisible() const;

			const NkString &Adresse() const; ///< l'adresse affichee
			const NkString &Titre() const;	 ///< le dernier titre lu (DemanderTitre ou le titre du document)
			bool PeutReculer() const;
			bool PeutAvancer() const;
			bool Chargee() const;	  ///< la derniere navigation est terminee
			bool EnErreur() const;	  ///< ... et elle a echoue (l'adresse ne repond pas)
			int32 Navigations() const; ///< combien de navigations terminees (les bancs)

			/// Lance `document.title` dans la page ; le resultat arrive dans Titre() (asynchrone).
			void DemanderTitre();
			/// Ecrit une capture PNG de la vue (asynchrone, marche meme fenetre cachee) ; faux si la vue n'est pas prete.
			bool CapturerPng(const NkString &chemin);
			bool CaptureFaite() const;
			long CaptureCode() const; ///< HRESULT de la capture (1 : pas de reponse), pour les bancs

			/// Ferme proprement (idempotent) : plus aucun rappel ne touche cette vue apres.
			void Fermer();

			/// L'etat interne (opaque : defini dans la plateforme). Public pour que les rappels de la plateforme le nomment.
			struct Impl;

		private:
			NkWebView();
			Impl *mImpl;
	};

} // namespace nkentseu
