// =============================================================================
// NKCore/Text/NkSortie.cpp
// Voir NkSortie.h : tampon par flux, politique de vidage de la libc, un appel
// système par paquet, UTF-8 vers UTF-16 pour la console Windows.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE : 2026-10-22
// LICENCE : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#include "pch.h"
#include "NKCore/Text/NkSortie.h"
#include "NKCore/Text/NkSnprintf.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

namespace nkentseu {

	namespace {

		// 64 Kio : seize fois le tampon de la CRT de Windows (4 Kio) et de la
		// glibc pour un fichier (4 à 8 Kio) : seize fois moins d'appels système.
		constexpr usize kTamponMax = 64u * 1024u;

		enum Etat : int {
			EtatInconnu = 0,
			EtatConsole = 1, // un terminal : vidé à chaque fin de ligne
			EtatFichier = 2, // un fichier ou un tube : vidé quand le tampon est plein
			EtatAucun = 3	 // pas de flux (application fenêtrée sans console)
		};

		// AUCUN constructeur : ces objets sont mis à zéro avant toute
		// initialisation dynamique, donc utilisables par le code qui écrit
		// pendant l'initialisation des autres variables globales.
		struct Flux {
				char tampon[kTamponMax];
				usize n;				 // octets retenus
				usize capacite;			 // 0 = pas encore fixée ; voir CapaciteEffective
				bool capaciteFixee;		 // fixée par NkSortieFixerTampon
				bool sansTampon;		 // capacité fixée à 0 : chaque appel va au système
				bool fini;				 // après la fin du programme : plus rien n'est retenu
				int etat;				 // Etat
				volatile long verrou;	 // verrou rotatif minimal
				uint64 appels;			 // appels système faits (pour les bancs)
				void *poignee;			 // la poignée lue au dernier appel système
				unsigned char reste[4];	 // console Windows : octets UTF-8 d'une séquence incomplète
				usize nReste;
		};

		Flux gSortie;
		Flux gErreur;

		Flux &Choisir(NkFlux flux) noexcept {
			return flux == NkFlux::Erreur ? gErreur : gSortie;
		}

		// ── Le verrou ───────────────────────────────────────────────────────
		void Verrouiller(Flux &f) noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
			while (_InterlockedExchange(&f.verrou, 1) != 0) {
				YieldProcessor();
			}
#else
			while (__atomic_exchange_n(&f.verrou, 1L, __ATOMIC_ACQUIRE) != 0) {
#if defined(__i386__) || defined(__x86_64__)
				__builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
				__asm__ __volatile__("yield");
#endif
			}
#endif
		}

		void Deverrouiller(Flux &f) noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
			_InterlockedExchange(&f.verrou, 0);
#else
			__atomic_store_n(&f.verrou, 0L, __ATOMIC_RELEASE);
#endif
		}

		usize Longueur(const char *texte) noexcept {
			usize n = 0;
			while (texte[n] != '\0')
				++n;
			return n;
		}

		void Copier(char *dst, const char *src, usize n) noexcept {
			for (usize i = 0; i < n; ++i)
				dst[i] = src[i];
		}

		bool ContientFinDeLigne(const char *p, usize n) noexcept {
			for (usize i = 0; i < n; ++i) {
				if (p[i] == '\n')
					return true;
			}
			return false;
		}

		usize CapaciteEffective(const Flux &f) noexcept {
			if (f.sansTampon)
				return 0;
			if (f.capaciteFixee)
				return f.capacite;
			return kTamponMax;
		}

		// ── L'appel système ─────────────────────────────────────────────────
#if defined(_WIN32)
		// Longueur du plus long préfixe fait de séquences UTF-8 COMPLÈTES : une
		// séquence coupée en deux attend la suite au prochain appel.
		usize PrefixeComplet(const char *p, usize n) noexcept {
#if defined(NK_SORTIE_CONTRE_EPREUVE_DECOUPE)
			// Contre-épreuve du banc (bancs/banc_sortie_libc.cpp, opération
			// « console ») : on coupe sans regarder, le « é » à cheval sur deux
			// blocs doit alors se relire FAUX. Jamais défini dans le produit.
			(void)p;
			return n;
#endif
			usize debut = n;
			for (usize recul = 0; recul < 4 && debut > 0; ++recul) {
				--debut;
				const unsigned char c = (unsigned char)p[debut];
				if ((c & 0xC0) != 0x80) { // un octet de tête (ou ASCII)
					usize attendu = 1;
					if ((c & 0xE0) == 0xC0)
						attendu = 2;
					else if ((c & 0xF0) == 0xE0)
						attendu = 3;
					else if ((c & 0xF8) == 0xF0)
						attendu = 4;
					return (debut + attendu > n) ? debut : n;
				}
			}
			return n;
		}

		bool EcrireConsole(Flux &f, HANDLE h, const char *p, usize n) noexcept {
			constexpr usize kBloc = 2048;
			char bloc[kBloc + 4];
			wchar_t large[kBloc + 4];
			while (n > 0) {
				usize k = 0;
				for (usize i = 0; i < f.nReste; ++i)
					bloc[k++] = (char)f.reste[i];
				f.nReste = 0;
				const usize prendre = (n < kBloc - k) ? n : kBloc - k;
				Copier(bloc + k, p, prendre);
				k += prendre;
				p += prendre;
				n -= prendre;
				const usize complet = PrefixeComplet(bloc, k);
				for (usize i = complet; i < k; ++i)
					f.reste[f.nReste++] = (unsigned char)bloc[i];
				if (complet == 0)
					continue;
				const int nl = MultiByteToWideChar(CP_UTF8, 0, bloc, (int)complet, large, (int)(kBloc + 4));
				int fait = 0;
				while (fait < nl) {
					DWORD ecrit = 0;
					if (!WriteConsoleW(h, large + fait, (DWORD)(nl - fait), &ecrit, nullptr) || ecrit == 0)
						return false;
					++f.appels;
					fait += (int)ecrit;
				}
			}
			return true;
		}
#endif

		bool EcrireSysteme(Flux &f, NkFlux flux, const char *p, usize n) noexcept {
#if defined(_WIN32)
			// La poignée se relit à chaque fois : AllocConsole (NKWindow en Debug)
			// remplace les poignées standard après le démarrage.
			HANDLE h = GetStdHandle(flux == NkFlux::Erreur ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
			if ((void *)h != f.poignee || f.etat == EtatInconnu) {
				f.poignee = (void *)h;
				DWORD mode = 0;
				if (h == nullptr || h == INVALID_HANDLE_VALUE)
					f.etat = EtatAucun;
				else if (GetFileType(h) == FILE_TYPE_CHAR && GetConsoleMode(h, &mode))
					f.etat = EtatConsole;
				else
					f.etat = EtatFichier;
			}
			if (f.etat == EtatAucun)
				return true; // comme la CRT d'une application fenêtrée : écrit nulle part, sans erreur
			if (f.etat == EtatConsole)
				return EcrireConsole(f, h, p, n);
			while (n > 0) {
				const DWORD morceau = n > 0x40000000u ? 0x40000000u : (DWORD)n;
				DWORD ecrit = 0;
				if (!WriteFile(h, p, morceau, &ecrit, nullptr))
					return false;
				++f.appels;
				p += ecrit;
				n -= ecrit;
			}
			return true;
#else
			const int fd = flux == NkFlux::Erreur ? 2 : 1;
			if (f.etat == EtatInconnu)
				f.etat = isatty(fd) ? EtatConsole : EtatFichier;
			while (n > 0) {
				const ssize_t r = write(fd, p, n);
				if (r < 0) {
					if (errno == EINTR)
						continue;
					return false;
				}
				++f.appels;
				p += r;
				n -= (usize)r;
			}
			return true;
#endif
		}

		bool Vider(Flux &f, NkFlux flux) noexcept {
			if (f.n == 0)
				return true;
			const usize n = f.n;
			f.n = 0;
			return EcrireSysteme(f, flux, f.tampon, n);
		}

		// L'état du flux doit être connu pour décider du vidage.
		void ConnaitreEtat(Flux &f, NkFlux flux) noexcept {
			if (f.etat == EtatInconnu)
				EcrireSysteme(f, flux, "", 0);
		}

		// Le cœur : ajoute n octets, sous verrou.
		bool Ajouter(Flux &f, NkFlux flux, const char *p, usize n) noexcept {
			const usize cap = f.fini ? 0 : CapaciteEffective(f);
			if (cap == 0 || n >= cap) {
				if (!Vider(f, flux))
					return false;
				return EcrireSysteme(f, flux, p, n);
			}
			if (f.n + n > cap && !Vider(f, flux))
				return false;
			Copier(f.tampon + f.n, p, n);
			f.n += n;
			return true;
		}

		// La politique de fin d'appel : l'erreur ne retient rien ; la console se
		// vide si une fin de ligne a été écrite.
		bool FinDAppel(Flux &f, NkFlux flux, bool finDeLigne) noexcept {
			ConnaitreEtat(f, flux);
			if (flux == NkFlux::Erreur || (finDeLigne && f.etat == EtatConsole))
				return Vider(f, flux);
			return true;
		}

		// À la fin du programme, ce qui reste est écrit ; ensuite plus rien
		// n'est retenu (les destructeurs globaux qui écrivent encore passent
		// directement).
		struct Videur {
				~Videur() {
					NkFflushTout();
					gSortie.fini = true;
					gErreur.fini = true;
				}
		};
		Videur gVideur;

	} // namespace

	int NkFputs(const char *texte, NkFlux flux) noexcept {
		if (texte == nullptr)
			return -1;
		Flux &f = Choisir(flux);
		const usize n = Longueur(texte);
		Verrouiller(f);
		bool ok = Ajouter(f, flux, texte, n);
		ok = FinDAppel(f, flux, ContientFinDeLigne(texte, n)) && ok;
		Deverrouiller(f);
		return ok ? (int)(n & 0x7FFFFFFF) : -1;
	}

	int NkPuts(const char *texte) noexcept {
		if (texte == nullptr)
			return -1;
		Flux &f = gSortie;
		const usize n = Longueur(texte);
		Verrouiller(f);
		bool ok = Ajouter(f, NkFlux::Sortie, texte, n);
		ok = Ajouter(f, NkFlux::Sortie, "\n", 1) && ok;
		ok = FinDAppel(f, NkFlux::Sortie, true) && ok;
		Deverrouiller(f);
		return ok ? (int)((n + 1) & 0x7FFFFFFF) : -1;
	}

	int NkFputc(int c, NkFlux flux) noexcept {
		Flux &f = Choisir(flux);
		const char octet = (char)(unsigned char)c;
		Verrouiller(f);
		bool ok = Ajouter(f, flux, &octet, 1);
		ok = FinDAppel(f, flux, octet == '\n') && ok;
		Deverrouiller(f);
		return ok ? (int)(unsigned char)c : -1;
	}

	usize NkFwrite(const void *donnees, usize taille, NkFlux flux) noexcept {
		if (donnees == nullptr || taille == 0)
			return 0;
		Flux &f = Choisir(flux);
		const char *p = (const char *)donnees;
		Verrouiller(f);
		bool ok = Ajouter(f, flux, p, taille);
		ok = FinDAppel(f, flux, ContientFinDeLigne(p, taille)) && ok;
		Deverrouiller(f);
		return ok ? taille : 0;
	}

	int NkFflush(NkFlux flux) noexcept {
		Flux &f = Choisir(flux);
		Verrouiller(f);
		const bool ok = Vider(f, flux);
		Deverrouiller(f);
		return ok ? 0 : -1;
	}

	void NkFflushTout() noexcept {
		NkFflush(NkFlux::Sortie);
		NkFflush(NkFlux::Erreur);
	}

	int NkVfprintf(NkFlux flux, const char *format, va_list args) noexcept {
		if (format == nullptr)
			return -1;
		Flux &f = Choisir(flux);
		Verrouiller(f);
		const usize cap = f.fini ? 0 : CapaciteEffective(f);

		// Le chemin rapide : formater DIRECTEMENT dans le tampon du flux, sans
		// copie intermédiaire (ce que fait la libc).
		if (cap > 0) {
			for (int essai = 0; essai < 2; ++essai) {
				const usize libre = cap - f.n;
				va_list copie;
				va_copy(copie, args);
				const int n = NkVsnprintf(f.tampon + f.n, libre, format, copie);
				va_end(copie);
				if (n < 0) {
					Deverrouiller(f);
					return -1;
				}
				if ((usize)n < libre) { // tout tient, '\0' compris
					const bool ligne = ContientFinDeLigne(f.tampon + f.n, (usize)n);
					f.n += (usize)n;
					const bool ok = FinDAppel(f, flux, ligne);
					Deverrouiller(f);
					return ok ? n : -1;
				}
				if ((usize)n >= cap || !Vider(f, flux))
					break; // trop grand pour le tampon : le chemin lent
			}
		}

		// Le chemin lent : un texte plus grand que le tampon, ou pas de tampon.
		va_list copie;
		va_copy(copie, args);
		const int n = NkVsnprintf(nullptr, 0, format, copie);
		va_end(copie);
		if (n < 0) {
			Deverrouiller(f);
			return -1;
		}
		char *tas = new char[(usize)n + 1];
		NkVsnprintf(tas, (usize)n + 1, format, args);
		bool ok = Vider(f, flux) && EcrireSysteme(f, flux, tas, (usize)n);
		delete[] tas;
		Deverrouiller(f);
		return ok ? n : -1;
	}

	int NkFprintf(NkFlux flux, const char *format, ...) noexcept {
		va_list args;
		va_start(args, format);
		const int n = NkVfprintf(flux, format, args);
		va_end(args);
		return n;
	}

	int NkPrintfSortie(const char *format, ...) noexcept {
		va_list args;
		va_start(args, format);
		const int n = NkVfprintf(NkFlux::Sortie, format, args);
		va_end(args);
		return n;
	}

	void NkSortieFixerTampon(NkFlux flux, usize taille) noexcept {
		Flux &f = Choisir(flux);
		Verrouiller(f);
		Vider(f, flux);
		f.capaciteFixee = true;
		f.sansTampon = taille == 0;
		f.capacite = taille > kTamponMax ? kTamponMax : taille;
		Deverrouiller(f);
	}

	uint64 NkSortieAppelsSysteme(NkFlux flux) noexcept {
		return Choisir(flux).appels;
	}

} // namespace nkentseu
