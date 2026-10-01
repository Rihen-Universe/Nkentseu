// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Script/NkUnkenyModulesCpp.cpp
// DESCRIPTION: Le chargement et le rechargement a chaud des DLL de scripts C++.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Script/NkUnkenyModulesCpp.h"
#include "Unkeny/Script/NkUnkenyScripts.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define NK_UNK_DYNAMIQUE 1
#elif defined(__EMSCRIPTEN__) || defined(__ANDROID__) || defined(__OHOS__) ||                                          \
	(defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define NK_UNK_DYNAMIQUE 0
#else
#include <dlfcn.h>
#define NK_UNK_DYNAMIQUE 1
#endif

namespace nkentseu {
	namespace unkeny {

		bool NkModulesCppDynamiques() noexcept {
			return NK_UNK_DYNAMIQUE != 0;
		}

		namespace {
			/// « D:/a/Scripts.dll » -> (« D:/a/Scripts », « .dll »).
			void Couper(const NkString &chemin, NkString &base, NkString &ext) {
				const char *c = chemin.CStr();
				const char *point = std::strrchr(c, '.');
				const char *barre1 = std::strrchr(c, '/');
				const char *barre2 = std::strrchr(c, '\\');
				const char *barre = barre1 > barre2 ? barre1 : barre2;
				if (point == nullptr || (barre != nullptr && point < barre)) {
					base = chemin;
					ext = "";
					return;
				}
				base = NkString(c, static_cast<usize>(point - c));
				ext = NkString(point);
			}

			void *Ouvrir(const char *chemin, NkString &erreur) {
#if NK_UNK_DYNAMIQUE && defined(_WIN32)
				HMODULE h = ::LoadLibraryA(chemin);
				if (h == nullptr) {
					erreur = NkString::Format("LoadLibrary a refuse %s (code %lu)", chemin, static_cast<unsigned long>(::GetLastError()));
				}
				return reinterpret_cast<void *>(h);
#elif NK_UNK_DYNAMIQUE
				void *h = ::dlopen(chemin, RTLD_NOW | RTLD_LOCAL);
				if (h == nullptr) {
					const char *e = ::dlerror();
					erreur = NkString::Format("dlopen a refuse %s : %s", chemin, e != nullptr ? e : "?");
				}
				return h;
#else
				(void)chemin;
				erreur = "les scripts C++ ne se chargent pas dynamiquement sur cette plateforme (liez-les en statique)";
				return nullptr;
#endif
			}

			void *SymboleBibli(void *bibli, const char *nom) {
#if NK_UNK_DYNAMIQUE && defined(_WIN32)
				return reinterpret_cast<void *>(::GetProcAddress(reinterpret_cast<HMODULE>(bibli), nom));
#elif NK_UNK_DYNAMIQUE
				return ::dlsym(bibli, nom);
#else
				(void)bibli;
				(void)nom;
				return nullptr;
#endif
			}

			void Fermer(void *bibli) {
				if (bibli == nullptr) {
					return;
				}
#if NK_UNK_DYNAMIQUE && defined(_WIN32)
				::FreeLibrary(reinterpret_cast<HMODULE>(bibli));
#elif NK_UNK_DYNAMIQUE
				::dlclose(bibli);
#endif
			}
		} // namespace

		NkModulesCpp::~NkModulesCpp() {
			Decharger();
		}

		void NkModulesCpp::Decharger() {
			Fermer(mBibli);
			mBibli = nullptr;
			mModule = nullptr;
			if (!mCopie.Empty()) {
				NkFile::Delete(mCopie.CStr());
				mCopie.Clear();
			}
		}

		bool NkModulesCpp::Recharger(const char *chemin, NkScripts2D &scripts, NkHoteScripts2D *hote, NkString *erreur) {
			NkString err;
			auto echec = [&](const NkString &pourquoi) {
				if (erreur != nullptr) {
					*erreur = pourquoi;
				}
				return false;
			};
			if (chemin == nullptr || !NkFile::Exists(chemin)) {
				return echec(NkString::Format("module introuvable : %s", chemin != nullptr ? chemin : "(nul)"));
			}
			// La COPIE par generation : l'original reste reinscriptible par le
			// compilateur pendant que la copie est chargee.
			NkString base, ext;
			Couper(NkString(chemin), base, ext);
			const uint32 gen = mGeneration + 1u;
			const NkString copie = NkString::Format("%s.gen%u%s", base.CStr(), static_cast<unsigned>(gen), ext.CStr());
			NkFile::Delete(copie.CStr());
			if (!NkFile::Copy(chemin, copie.CStr(), true)) {
				return echec(NkString::Format("copie impossible vers %s", copie.CStr()));
			}
			void *bibli = Ouvrir(copie.CStr(), err);
			if (bibli == nullptr) {
				NkFile::Delete(copie.CStr());
				return echec(err);
			}
			NkUnkPointEntreeV1 entree = reinterpret_cast<NkUnkPointEntreeV1>(SymboleBibli(bibli, NK_UNK_POINT_ENTREE));
			const NkUnkModuleV1 *module = entree != nullptr ? entree() : nullptr;
			if (module == nullptr) {
				Fermer(bibli);
				NkFile::Delete(copie.CStr());
				return echec(NkString::Format("%s n'exporte pas %s (registre absent ?)", chemin, NK_UNK_POINT_ENTREE));
			}
			if (!scripts.EnregistrerModuleCpp(module, &err, mModule)) {
				Fermer(bibli);
				NkFile::Delete(copie.CStr());
				return echec(err);
			}
			// Les instances : detruites par l'ANCIEN code (encore charge), recreees
			// par le nouveau. Puis seulement l'ancienne generation s'en va.
			if (hote != nullptr) {
				hote->MigrerInstances();
			}
			Fermer(mBibli);
			if (!mCopie.Empty()) {
				NkFile::Delete(mCopie.CStr());
			}
			mBibli = bibli;
			mModule = module;
			mCopie = copie;
			mGeneration = gen;
			return true;
		}

		void *NkModulesCpp::Symbole(const char *nom) const noexcept {
			return mBibli != nullptr && nom != nullptr ? SymboleBibli(mBibli, nom) : nullptr;
		}

		uint32 NkModulesCpp::NettoyerCopies(const char *chemin) {
			if (chemin == nullptr) {
				return 0u;
			}
			NkString base, ext;
			Couper(NkString(chemin), base, ext);
			const char *c = base.CStr();
			const char *b1 = std::strrchr(c, '/');
			const char *b2 = std::strrchr(c, '\\');
			const char *barre = b1 > b2 ? b1 : b2;
			const NkString dossier = barre != nullptr ? NkString(c, static_cast<usize>(barre - c)) : NkString(".");
			const NkString nom = barre != nullptr ? NkString(barre + 1) : base;
			const NkString motif = NkString::Format("%s.gen*%s", nom.CStr(), ext.CStr());
			if (!NkDirectory::Exists(dossier.CStr())) {
				return 0u;
			}
			const NkVector<NkString> copies = NkDirectory::GetFiles(dossier.CStr(), motif.CStr());
			uint32 n = 0;
			for (uint32 i = 0; i < copies.Size(); ++i) {
				n += NkFile::Delete(copies[i].CStr()) ? 1u : 0u;
			}
			return n;
		}

	} // namespace unkeny
} // namespace nkentseu
