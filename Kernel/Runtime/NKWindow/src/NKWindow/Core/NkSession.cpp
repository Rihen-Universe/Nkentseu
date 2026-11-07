// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkSession.cpp
// -----------------------------------------------------------------------------
// Voir NkSession.h. Windows : RegisterApplicationRestart ; ailleurs, un repli qui compile.
// =============================================================================

#include "NKWindow/Core/NkSession.h"
#include "NKPlatform/NkPlatformDetect.h"

#include <cstring>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace nkentseu {

	namespace {
		struct NkSessionEtat {
				NkSessionRouvrir mode = NkSessionRouvrir::AuRedemarrage;
				NkSession::Rappel rappel = nullptr;
				void *utilisateur = nullptr;
				bool fin = false;
		};
		// Portee de fichier, initialisation constante : une statique DE FONCTION demanderait __cxa_guard_acquire
		// (libstdc++), et l'edition de liens de NKCode ne tire pas cette bibliotheque.
		NkSessionEtat sEtat;
		NkSessionEtat &Etat() {
			return sEtat;
		}
		constexpr const char *kArgumentRelance = "--reouvrir-session";
	} // namespace

	const char *NkSession::ArgumentRelance() {
		return kArgumentRelance;
	}

	NkSessionRouvrir NkSession::Mode() {
		return Etat().mode;
	}

	void NkSession::SurFinDeSession(Rappel rappel, void *utilisateur) {
		Etat().rappel = rappel;
		Etat().utilisateur = utilisateur;
	}

	bool NkSession::FinDeSessionAnnoncee() {
		return Etat().fin;
	}

	bool NkSession::LanceParLeSysteme(int argc, const char *const *argv) {
		for (int i = 1; i < argc; ++i)
			if (argv && argv[i] && std::strcmp(argv[i], kArgumentRelance) == 0)
				return true;
		return false;
	}

	bool NkSession::NotifierDemande() {
		return true; // on ne s'oppose jamais a la fin de session : c'est le rappel de la FIN qui ecrit la memoire
	}

	void NkSession::NotifierFin(bool reelle) {
		if (!reelle)
			return; // la fin a ete annulee par un autre programme : rien ne s'arrete
		NkSessionEtat &e = Etat();
		if (e.fin)
			return; // une seule fois, meme si le systeme envoie le message deux fois
		e.fin = true;
		if (e.rappel)
			e.rappel(e.utilisateur);
	}

#if defined(NKENTSEU_PLATFORM_WINDOWS)

	bool NkSession::Activer(NkSessionRouvrir mode) {
		Etat().mode = mode;
		if (mode != NkSessionRouvrir::AuRedemarrage) {
			::UnregisterApplicationRestart();
			return true;
		}
		// RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_PATCH : un plantage ou un blocage ne relance pas ;
		// un redemarrage du PC, si.
		const HRESULT hr = ::RegisterApplicationRestart(L"--reouvrir-session", RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_PATCH);
		return SUCCEEDED(hr);
	}

	bool NkSession::SystemeRelanceActive(bool *connu) {
		// Lecture seule : Parametres > Comptes > Options de connexion > « Redemarrer les applications ».
		// advapi32 est charge a la demande : NKWindow n'a pas a l'ajouter a son edition de liens.
		if (connu)
			*connu = false;
		HMODULE adv = ::LoadLibraryW(L"advapi32.dll");
		if (!adv)
			return false;
		using FnGet = LONG(WINAPI *)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);
		FnGet get = reinterpret_cast<FnGet>(reinterpret_cast<void *>(::GetProcAddress(adv, "RegGetValueW")));
		bool actif = false;
		if (get) {
			DWORD valeur = 0, taille = sizeof(valeur);
			const LONG r = get(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon", L"RestartApps",
							   0x00000010 /* RRF_RT_REG_DWORD */, nullptr, &valeur, &taille);
			if (r == 0) {
				actif = valeur != 0;
				if (connu)
					*connu = true;
			}
		}
		::FreeLibrary(adv);
		return actif;
	}

#else

	bool NkSession::Activer(NkSessionRouvrir mode) {
		Etat().mode = mode;
		return mode != NkSessionRouvrir::AuRedemarrage; // aucune relance par le systeme ici
	}

	bool NkSession::SystemeRelanceActive(bool *connu) {
		if (connu)
			*connu = false;
		return false;
	}

#endif

} // namespace nkentseu
