//
// NkEditeurProcessus.cpp
// =============================================================================
// Description :
//   Le lanceur de programme externe de l'editeur (voir l'en-tete).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Livraison/NkEditeurProcessus.h"

#include "NKThreading/NkScopedLock.h"

#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace nkentseu {
	namespace editeur {

		using threading::NkMutex;
		using threading::NkScopedLock;

		namespace {
			void Dormir(int32 ms) {
#if defined(_WIN32)
				::Sleep(static_cast<DWORD>(ms));
#else
				::usleep(static_cast<useconds_t>(ms) * 1000u);
#endif
			}

#if defined(_WIN32)
			/// UTF-8 -> UTF-16, pour les API « W » : un chemin avec un accent
			/// (« Documents\Mes jeux\Gelée ») passe par la page ANSI sinon, et
			/// CreateProcess cherche un dossier qui n'existe pas.
			NkVector<wchar_t> Large(const NkString &s) {
				NkVector<wchar_t> w;
				const int32 n = ::MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, nullptr, 0);
				w.Resize(static_cast<usize>(n > 0 ? n : 1));
				w[0] = L'\0';
				if (n > 0) {
					::MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, w.Data(), n);
				}
				return w;
			}
#endif
		} // namespace

		NkString NkSansAnsi(const char *ligne) {
			NkString sortie;
			if (ligne == nullptr) {
				return sortie;
			}
			for (const char *p = ligne; *p != '\0'; ++p) {
				// ESC '[' parametres, puis un octet final dans '@'..'~'.
				if (*p == 0x1b && p[1] == '[') {
					p += 2;
					while (*p != '\0' && !(*p >= '@' && *p <= '~')) {
						++p;
					}
					if (*p == '\0') {
						break;
					}
					continue;
				}
				if (*p == '\r' || *p == '\n') {
					continue;
				}
				sortie.Append(*p);
			}
			return sortie;
		}

		NkEditeurProcessus::~NkEditeurProcessus() {
			Arreter();
			if (mFil.Joinable()) {
				mFil.Join();
			}
		}

		void NkEditeurProcessus::Environnement(const char *nom, const char *valeur) {
			if (nom == nullptr || nom[0] == '\0') {
				return;
			}
			for (usize i = 0; i < mNomsEnv.Size(); ++i) {
				if (mNomsEnv[i] == NkString(nom)) {
					mValeursEnv[i] = NkString(valeur != nullptr ? valeur : "");
					return;
				}
			}
			mNomsEnv.PushBack(NkString(nom));
			mValeursEnv.PushBack(NkString(valeur != nullptr ? valeur : ""));
		}

		bool NkEditeurProcessus::Lancer(const NkString &commande, const NkString &dossier) {
			{
				NkScopedLock<NkMutex> v(mVerrou);
				if (mEnCours) {
					return false;
				}
				mEnCours = true;
				mCode = 0;
				mArrete = false;
			}
			if (mFil.Joinable()) {
				mFil.Join(); // le fil d'une construction precedente, deja fini
			}
			mCommande = commande;
			mDossier = dossier;
			mFil = threading::NkThread([this](void *) {
				Lire();
			});
			return true;
		}

		void NkEditeurProcessus::Recolter(NkVector<NkString> &lignes) {
			NkScopedLock<NkMutex> v(mVerrou);
			for (usize i = 0; i < mLignes.Size(); ++i) {
				lignes.PushBack(mLignes[i]);
			}
			mLignes.Clear();
		}

		bool NkEditeurProcessus::EnCours() const {
			NkScopedLock<NkMutex> v(mVerrou);
			return mEnCours;
		}

		int32 NkEditeurProcessus::Code() const {
			NkScopedLock<NkMutex> v(mVerrou);
			return mCode;
		}

		void NkEditeurProcessus::Pousser(const NkString &ligne) {
			NkScopedLock<NkMutex> v(mVerrou);
			mLignes.PushBack(ligne);
		}

		void NkEditeurProcessus::Terminer(int32 code) {
			NkScopedLock<NkMutex> v(mVerrou);
			mCode = code;
			mEnCours = false;
			mProcessus = nullptr;
		}

		void NkEditeurProcessus::Arreter() {
#if defined(_WIN32)
			NkScopedLock<NkMutex> v(mVerrou);
			if (mEnCours && mProcessus != nullptr) {
				mArrete = true;
				// ⚠️ jenga lance lui-meme le compilateur : TerminateProcess ne tue
				//    que jenga, et un clang deja parti finit son fichier. C'est
				//    sans danger (il ecrit dans Build/Obj), et c'est dit ici pour
				//    qu'un processus « orphelin » visible une seconde n'etonne pas.
				::TerminateProcess(static_cast<HANDLE>(mProcessus), 124u);
			}
#endif
		}

		int32 NkEditeurProcessus::Attendre(void (*sortie)(const NkString &ligne, void *donnees), void *donnees) {
			NkVector<NkString> lot;
			for (;;) {
				const bool encore = EnCours();
				lot.Clear();
				Recolter(lot);
				for (usize i = 0; i < lot.Size(); ++i) {
					if (sortie != nullptr) {
						sortie(lot[i], donnees);
					}
				}
				if (!encore) {
					break;
				}
				Dormir(20);
			}
			if (mFil.Joinable()) {
				mFil.Join();
			}
			return Code();
		}

		void NkEditeurProcessus::Lire() {
#if defined(_WIN32)
			SECURITY_ATTRIBUTES sa{};
			sa.nLength = sizeof(sa);
			sa.bInheritHandle = TRUE;
			HANDLE lecture = nullptr;
			HANDLE ecriture = nullptr;
			if (!::CreatePipe(&lecture, &ecriture, &sa, 0)) {
				Pousser(NkString("[construire] tuyau de sortie impossible a creer"));
				Terminer(-1);
				return;
			}
			// Le bout LECTURE reste a nous : herite, il garderait le tuyau
			// ouvert apres la fin du programme, et ReadFile ne rendrait jamais.
			::SetHandleInformation(lecture, HANDLE_FLAG_INHERIT, 0);
			HANDLE entree = ::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

			// ⚠️ jenga est un programme PYTHON : sa sortie, redirigee vers un
			//    tuyau, prend la page de code ANSI -- et un « ✓ » y leve une
			//    UnicodeEncodeError qui arrete la construction. On lui impose
			//    UTF-8 (herite par le fils).
			::SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");
			::SetEnvironmentVariableW(L"PYTHONUTF8", L"1");
			// Et SANS TAMPON : vers un tuyau, Python ecrit par blocs de 8 Kio, et
			// le Journal restait muet pendant les trois minutes d'une construction
			// du moteur (mesure du 2026-09-30) avant de tout recevoir d'un coup.
			::SetEnvironmentVariableW(L"PYTHONUNBUFFERED", L"1");
			for (usize i = 0; i < mNomsEnv.Size(); ++i) {
				::SetEnvironmentVariableW(Large(mNomsEnv[i]).Data(), Large(mValeursEnv[i]).Data());
			}

			STARTUPINFOW si{};
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdInput = entree;
			si.hStdOutput = ecriture;
			si.hStdError = ecriture;
			PROCESS_INFORMATION pi{};
			NkVector<wchar_t> ligne = Large(mCommande);
			NkVector<wchar_t> dossier = Large(mDossier);
			const BOOL ok = ::CreateProcessW(nullptr, ligne.Data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
											 mDossier.Empty() ? nullptr : dossier.Data(), &si, &pi);
			const DWORD erreur = ::GetLastError();
			::CloseHandle(ecriture);
			if (entree != INVALID_HANDLE_VALUE) {
				::CloseHandle(entree);
			}
			if (!ok) {
				::CloseHandle(lecture);
				Pousser(NkString::Format("[construire] lancement impossible (erreur Windows %u) : %s",
										 static_cast<unsigned>(erreur), mCommande.CStr()));
				Terminer(-1);
				return;
			}
			{
				NkScopedLock<NkMutex> v(mVerrou);
				mProcessus = pi.hProcess;
			}
			char tampon[4096];
			NkString courante;
			DWORD lus = 0;
			while (::ReadFile(lecture, tampon, sizeof(tampon), &lus, nullptr) && lus > 0) {
				for (DWORD i = 0; i < lus; ++i) {
					const char c = tampon[i];
					if (c == '\n') {
						Pousser(NkSansAnsi(courante.CStr()));
						courante = NkString();
						continue;
					}
					courante.Append(c);
				}
			}
			if (!courante.Empty()) {
				Pousser(NkSansAnsi(courante.CStr()));
			}
			::CloseHandle(lecture);
			::WaitForSingleObject(pi.hProcess, INFINITE);
			DWORD code = 0;
			::GetExitCodeProcess(pi.hProcess, &code);
			::CloseHandle(pi.hThread);
			{
				// Le HANDLE est ferme SOUS le verrou : Arreter() le lit sous le
				// meme verrou, il ne peut donc pas tuer un handle deja rendu.
				NkScopedLock<NkMutex> v(mVerrou);
				::CloseHandle(pi.hProcess);
				mProcessus = nullptr;
			}
			Terminer(mArrete ? 124 : static_cast<int32>(code));
#else
			::setenv("PYTHONUNBUFFERED", "1", 1);
			for (usize i = 0; i < mNomsEnv.Size(); ++i) {
				::setenv(mNomsEnv[i].CStr(), mValeursEnv[i].CStr(), 1);
			}
			NkString complete;
			if (!mDossier.Empty()) {
				complete = NkString::Format("cd \"%s\" && %s 2>&1", mDossier.CStr(), mCommande.CStr());
			} else {
				complete = NkString::Format("%s 2>&1", mCommande.CStr());
			}
			FILE *tuyau = ::popen(complete.CStr(), "r");
			if (tuyau == nullptr) {
				Pousser(NkString("[construire] lancement impossible : ") + mCommande);
				Terminer(-1);
				return;
			}
			char tampon[4096];
			NkString courante;
			while (std::fgets(tampon, sizeof(tampon), tuyau) != nullptr) {
				courante.Append(tampon);
				if (!courante.Empty() && courante.Back() == '\n') {
					Pousser(NkSansAnsi(courante.CStr()));
					courante = NkString();
				}
			}
			if (!courante.Empty()) {
				Pousser(NkSansAnsi(courante.CStr()));
			}
			const int32 statut = ::pclose(tuyau);
			Terminer(statut);
#endif
		}

	} // namespace editeur
} // namespace nkentseu
