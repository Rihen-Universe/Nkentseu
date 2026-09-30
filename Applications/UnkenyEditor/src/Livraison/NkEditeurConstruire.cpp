//
// NkEditeurConstruire.cpp
// =============================================================================
// Description :
//   La construction d'un jeu Unkeny pour une plateforme (voir l'en-tete).
//
// Caracteristiques :
//   - Le workspace genere ne DECLARE PAS le projet du jeu : il pose le
//     dictionnaire UNKENY_JEU (ce qui est propre au jeu) puis inclut
//     Applications/UnkenyPlayer/UnkenyPlayer.jenga, qui le lit. Une seule
//     declaration du joueur, pour le depot et pour chaque jeu construit.
//   - Il reprend de Nkentseu.jenga ce que les modules du moteur supposent :
//     les quatre `useconfig`, `nkentseutoolchain()`, les memes `newoption`
//     (les filtres des modules les lisent), et `unitest` AVANT les includes
//     (chaque module declare ses tests ; sans lui, « Unitest is not
//     configured »).
//   - `jengaconfig` : les modules font `from jengaconfig import *`. Le module
//     est un talon vide que `jenga build` genere... sauf quand on lui demande
//     de ne pas toucher au dossier (JENGA_NO_IDE_CONFIG). On l'ecrit donc
//     nous-memes dans `.jenga-typings/`, que le chargeur met sur sys.path.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Livraison/NkEditeurConstruire.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurEntrees.h"
#include "Livraison/NkEditeurProcessus.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKImage/Core/NkImage.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Livraison/NkUnkenyLivraison.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			struct NkInfoCible {
					const char *nom;
					const char *jenga;
					/// `%{cfg.system}` de Jenga, celui des dossiers Build/Bin/<Cfg>-<Systeme>.
					const char *systeme;
					/// Ce que la construction produit (vide : un executable sans extension).
					const char *extension;
			};

			const NkInfoCible kCibles[NK_NB_PLATEFORMES] = {
				{"Windows", "windows", "Windows", ".exe"},
				{"Android", "android", "Android", ".apk"},
				{"Web", "web", "Web", ".html"},
				{"HarmonyOS", "harmonyos", "HarmonyOS", ".hap"},
				{"Linux", "linux", "Linux", ""},
				{"macOS", "macos", "macOS", ".app"},
				{"iOS", "ios", "iOS", ".ipa"},
			};

			const NkInfoCible &Cible(NkPlateformeJeu p) noexcept {
				const int32 i = static_cast<int32>(p);
				return kCibles[(i >= 0 && i < NK_NB_PLATEFORMES) ? i : 0];
			}

			NkString Oblique(const NkString &s) {
				NkString r;
				for (usize i = 0; i < s.Length(); ++i) {
					r.Append(s[i] == '\\' ? '/' : s[i]);
				}
				return r;
			}

			NkString Barre(const NkString &s) {
				const NkString o = Oblique(s);
				if (o.Empty() || o.EndsWith('/')) {
					return o;
				}
				return o + "/";
			}

			NkString SansBarre(const NkString &s) {
				NkString o = Oblique(s);
				while (o.Length() > 1u && o.EndsWith('/')) {
					o.PopBack();
				}
				return o;
			}

			bool Existe(const NkString &chemin) {
				return !chemin.Empty() && (NkFile::Exists(chemin.CStr()) || NkDirectory::Exists(chemin.CStr()));
			}

			NkString Env(const char *nom) {
				const char *v = std::getenv(nom);
				return NkString(v != nullptr ? v : "");
			}

			/// Le programme `nom` dans le PATH (avec .exe / .bat / .cmd sous
			/// Windows). Vide s'il n'y est pas.
			NkString DansLePath(const char *nom) {
				const NkString path = Env("PATH");
#if defined(_WIN32)
				const char separateur = ';';
				static const char *kSuffixes[] = {".exe", ".bat", ".cmd", ""};
#else
				const char separateur = ':';
				static const char *kSuffixes[] = {""};
#endif
				NkString dossier;
				for (usize i = 0; i <= path.Length(); ++i) {
					if (i < path.Length() && path[i] != separateur) {
						dossier.Append(path[i]);
						continue;
					}
					if (!dossier.Empty()) {
						for (const char *suffixe : kSuffixes) {
							const NkString essai = Barre(dossier) + nom + suffixe;
							if (NkFile::Exists(essai.CStr())) {
								return essai;
							}
						}
					}
					dossier = NkString();
				}
				return NkString();
			}

			/// Le premier chemin qui existe ; vide sinon.
			NkString Premier(const NkVector<NkString> &candidats) {
				for (usize i = 0; i < candidats.Size(); ++i) {
					if (Existe(candidats[i])) {
						return Oblique(candidats[i]);
					}
				}
				return NkString();
			}

			NkString Maison() {
				const NkString profil = Env("USERPROFILE");
				return profil.Empty() ? Env("HOME") : profil;
			}

			/// La commande, entre guillemets quand le chemin a un espace.
			NkString Guillemets(const NkString &s) {
				return NkString("\"") + s + "\"";
			}

			// ── L'icone fabriquee : une gelee, sombre et claire ─────────────────
			struct NkTeinte {
					uint8 r, g, b;
			};

			float32 Borne01(float32 v) noexcept {
				return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
			}

			/// Couverture d'un disque (bord adouci d'un pixel) au point (x, y).
			float32 Disque(float32 x, float32 y, float32 cx, float32 cy, float32 r) noexcept {
				const float32 dx = x - cx;
				const float32 dy = y - cy;
				const float32 d = math::NkSqrt(dx * dx + dy * dy);
				return Borne01(r - d + 0.5f);
			}

			/// Couverture d'un carre aux coins arrondis, centre dans l'image.
			float32 CarreArrondi(float32 x, float32 y, float32 cote, float32 rayon) noexcept {
				const float32 demi = cote * 0.5f;
				const float32 qx = math::NkAbs(x - demi) - (demi - rayon);
				const float32 qy = math::NkAbs(y - demi) - (demi - rayon);
				const float32 ex = qx > 0.f ? qx : 0.f;
				const float32 ey = qy > 0.f ? qy : 0.f;
				const float32 dehors = math::NkSqrt(ex * ex + ey * ey);
				const float32 dedans = (qx > qy ? qx : qy) < 0.f ? (qx > qy ? qx : qy) : 0.f;
				return Borne01(rayon - (dehors + dedans) + 0.5f);
			}

			void Melanger(uint8 *px, const NkTeinte &t, float32 a) noexcept {
				const float32 b = 1.f - a;
				px[0] = static_cast<uint8>(static_cast<float32>(px[0]) * b + static_cast<float32>(t.r) * a);
				px[1] = static_cast<uint8>(static_cast<float32>(px[1]) * b + static_cast<float32>(t.g) * a);
				px[2] = static_cast<uint8>(static_cast<float32>(px[2]) * b + static_cast<float32>(t.b) * a);
			}

			bool PeindreIcone(const char *chemin, const NkTeinte &fond, const NkTeinte &gelee, const NkTeinte &reflet,
							  const NkTeinte &yeux) {
				constexpr int32 N = 512;
				NkImage img = NkImage::Create(static_cast<uint32>(N), static_cast<uint32>(N), NkImagePixelFormat::NK_RGBA32, 0u);
				if (!img.IsValid()) {
					return false;
				}
				for (int32 y = 0; y < N; ++y) {
					uint8 *ligne = img.RowPtr(y);
					for (int32 x = 0; x < N; ++x) {
						uint8 *px = ligne + x * 4;
						const float32 fx = static_cast<float32>(x) + 0.5f;
						const float32 fy = static_cast<float32>(y) + 0.5f;
						// Le fond : un carre arrondi, transparent autour (les coins
						// d'une icone de bureau ne sont pas des angles vifs).
						const float32 aFond = CarreArrondi(fx, fy, static_cast<float32>(N), 96.f);
						px[0] = fond.r;
						px[1] = fond.g;
						px[2] = fond.b;
						px[3] = static_cast<uint8>(aFond * 255.f);
						// La gelee : un corps rond ecrase, et une goutte qui s'en detache.
						const float32 corps = Disque(fx, fy * 1.18f, 256.f, 330.f * 1.18f, 150.f);
						const float32 goutte = Disque(fx, fy, 350.f, 150.f, 42.f);
						Melanger(px, gelee, corps > goutte ? corps : goutte);
						Melanger(px, reflet, Disque(fx, fy, 196.f, 270.f, 34.f) * 0.85f);
						Melanger(px, yeux, Disque(fx, fy, 222.f, 330.f, 17.f));
						Melanger(px, yeux, Disque(fx, fy, 294.f, 330.f, 17.f));
					}
				}
				return img.SavePNG(chemin);
			}

			/// Ecrit `ligne` au journal de la construction (le fichier garde TOUT,
			/// le Journal de l'editeur n'en garde que la fin).
			void AuFichier(const NkString &fichier, const NkString &ligne) {
				if (!fichier.Empty()) {
					NkFile::AppendAllText(fichier.CStr(), (ligne + "\n").CStr());
				}
			}

			/// Le premier fichier d'extension `ext` sous `dossier` (recursif).
			NkString ChercherProduit(const NkString &dossier, const char *ext) {
				if (!NkDirectory::Exists(dossier.CStr()) || ext == nullptr || ext[0] == '\0') {
					return NkString();
				}
				const NkString motif = NkString("*") + ext;
				const NkVector<NkString> trouves = NkDirectory::GetFiles(dossier.CStr(), motif.CStr(), NkSearchOption::NK_ALL_DIRECTORIES);
				return trouves.Empty() ? NkString() : Oblique(trouves[0]);
			}
		} // namespace

		// =====================================================================
		const char *NkPlateformeJeuNom(NkPlateformeJeu p) noexcept {
			return Cible(p).nom;
		}

		const char *NkPlateformeJeuJenga(NkPlateformeJeu p) noexcept {
			return Cible(p).jenga;
		}

		bool NkPlateformeJeuDepuis(const char *nom, NkPlateformeJeu &sortie) noexcept {
			if (nom == nullptr) {
				return false;
			}
			for (int32 i = 0; i < NK_NB_PLATEFORMES; ++i) {
				const char *a = kCibles[i].jenga;
				usize k = 0;
				bool egal = true;
				for (; a[k] != '\0' || nom[k] != '\0'; ++k) {
					const char c = (nom[k] >= 'A' && nom[k] <= 'Z') ? static_cast<char>(nom[k] - 'A' + 'a') : nom[k];
					if (c != a[k]) {
						egal = false;
						break;
					}
				}
				if (egal) {
					sortie = static_cast<NkPlateformeJeu>(i);
					return true;
				}
			}
			return false;
		}

		NkString NkIdentifiantJeu(const char *nom) {
			NkString id;
			if (nom != nullptr) {
				const uint8 *p = reinterpret_cast<const uint8 *>(nom);
				for (usize i = 0; p[i] != 0u; ++i) {
					const uint8 c = p[i];
					if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
						id.Append(static_cast<char>(c));
						continue;
					}
					// Les lettres accentuees du francais (UTF-8 sur deux octets,
					// C3 xx) perdent leur accent : « Gelée » -> « Gelee ».
					if (c == 0xC3u && p[i + 1] != 0u) {
						static const char kBas[] = "aaaaaaaceeeeiiiidnooooo/ouuuuyty";
						const uint8 s = p[i + 1];
						++i;
						if (s == 0x9Fu) {
							id.Append("ss"); // ß
						} else if (s >= 0x80u && s <= 0x9Fu) {
							const char b = kBas[s - 0x80u];
							if (b != '/' && b != 'd' && b != 't') {
								id.Append(static_cast<char>(b - 'a' + 'A'));
							}
						} else if (s >= 0xA0u && s <= 0xBFu) {
							const char b = kBas[s - 0xA0u];
							if (b != '/' && b != 'd' && b != 't') {
								id.Append(b);
							}
						}
					}
				}
			}
			if (id.Empty()) {
				return NkString("Jeu");
			}
			if (id[0] >= '0' && id[0] <= '9') {
				return NkString("Jeu") + id;
			}
			return id;
		}

		NkString NkTrouverDepot() {
			const NkString force = Env("NK_UNKENY_DEPOT");
			if (!force.Empty()) {
				return Barre(force);
			}
			NkPath d = NkPath::GetExecutableDirectory();
			for (int32 k = 0; k < 10; ++k) {
				const NkString s = Barre(d.ToString());
				if (NkFile::Exists((s + "config/modules.jenga").CStr()) &&
					NkFile::Exists((s + "Applications/UnkenyPlayer/UnkenyPlayer.jenga").CStr())) {
					return s;
				}
				const NkPath parent = d.GetParent();
				if (parent.ToString().Empty() || parent.ToString() == d.ToString()) {
					break;
				}
				d = parent;
			}
			return NkString();
		}

		NkString NkSortieParDefaut() {
			NkString base = NkDirectory::GetUserFolder(NkDirectory::NkUserFolder::Documents).ToString();
			if (base.Empty()) {
				base = NkDirectory::GetAppDataDirectory().ToString();
			}
			return Barre(base) + "Unkeny/Jeux";
		}

		bool NkFabriquerIcones(const char *cheminSombre, const char *cheminClair) {
			// Sombre : le fond de l'editeur (#141414, celui de NKCraft), une gelee
			// claire. Clair : #F5F5F5 et une gelee plus soutenue, qui tienne sur
			// du blanc.
			const bool s = PeindreIcone(cheminSombre, NkTeinte{0x14, 0x14, 0x14}, NkTeinte{0x3D, 0xDC, 0x97},
										NkTeinte{0xE8, 0xFF, 0xF4}, NkTeinte{0x10, 0x2A, 0x20});
			const bool c = PeindreIcone(cheminClair, NkTeinte{0xF5, 0xF5, 0xF5}, NkTeinte{0x1E, 0x9E, 0x6A},
										NkTeinte{0xC8, 0xF5, 0xE0}, NkTeinte{0x0B, 0x24, 0x1A});
			return s && c;
		}

		const NkVector<NkString> &NkModulesDuJoueur() {
			// La FERMETURE des dependances du joueur (UnkenyPlayer.jenga) ET du
			// moteur qu'il tire (Engine/Unkeny/Unkeny.jenga), dans l'ordre de
			// Nkentseu.jenga. ⚠️ Un module oublie ici n'est pas une erreur de
			// Jenga : sa dependance est retiree SANS BRUIT du graphe
			// (DependencyResolver) et c'est la compilation ou l'edition de liens
			// qui tombe. Le banc de l'editeur (lg2) compare cette liste aux DEUX
			// .jenga : une dependance ajoutee a Unkeny sans etre ajoutee ici le
			// fait rougir.
			static NkVector<NkString> liste;
			if (liste.Empty()) {
				static const char *kModules[] = {
					"Kernel/Foundation/NKPlatform/NKPlatform.jenga",
					"Kernel/Foundation/NKCore/NKCore.jenga",
					"Kernel/System/NKLogger/NKLogger.jenga",
					"Kernel/Foundation/NKMath/NKMath.jenga",
					"Kernel/Foundation/NKMemory/NKMemory.jenga",
					"Kernel/Foundation/NKContainers/NKContainers.jenga",
					"Kernel/Runtime/NKImage/NKImage.jenga",
					"Kernel/Runtime/NKFont/NKFont.jenga",
					"Kernel/Runtime/NKAudio/NKAudio.jenga",
					"Kernel/Runtime/NKMedia/NKMedia.jenga",
					"Kernel/System/NKTime/NKTime.jenga",
					"Kernel/System/NKStream/NKStream.jenga",
					"Kernel/System/NKThreading/NKThreading.jenga",
					"Kernel/System/NKFileSystem/NKFileSystem.jenga",
					"Kernel/System/NKReflection/NKReflection.jenga",
					"Kernel/System/NKSerialization/NKSerialization.jenga",
					// (30/09) Unkeny en depend depuis NkUnkenyAnimateur (HFSM de
					// NKAnima). Oublie ici, la construction d'un jeu tombait sur
					// « NKAnima/Clip/NkAnimation.h file not found » alors que tous
					// les bancs etaient verts : (lg2) ne regardait que le joueur.
					"Kernel/Runtime/NKAnima/NKAnima.jenga",
					"Externals/Libs/NKGlad/NKGlad.jenga",
					"Kernel/Runtime/NKEvent/NKEvent.jenga",
					"Kernel/Runtime/NKWindow/NKWindow.jenga",
					"Kernel/Runtime/NKCanvas/NKCanvas.jenga",
					"Kernel/Runtime/NKGui/NKGui.jenga",
					"Kernel/Runtime/NKECS/NKECS.jenga",
					"Engine/Unkeny/Unkeny.jenga",
					"Kernel/Runtime/NKCollision/NKCollision.jenga",
					"Kernel/Runtime/NKPhysics/NKPhysics.jenga",
				};
				for (const char *m : kModules) {
					liste.PushBack(NkString(m));
				}
			}
			return liste;
		}

		// =====================================================================
		// LES CHAINES DE CETTE MACHINE
		// =====================================================================
		void NkDetecterPlateformes(NkDisponibilite (&s)[NK_NB_PLATEFORMES]) {
			for (int32 i = 0; i < NK_NB_PLATEFORMES; ++i) {
				s[i] = NkDisponibilite();
			}
			const NkString jenga = DansLePath("jenga");
			const NkString depot = NkTrouverDepot();
			if (jenga.Empty() || depot.Empty()) {
				const char *pourquoi = jenga.Empty()
										   ? "Jenga introuvable dans le PATH : toute construction passe par lui (il vit hors du depot)"
										   : "sources du moteur introuvables : la construction compile le depot Nkentseu (NK_UNKENY_DEPOT)";
				for (int32 i = 0; i < NK_NB_PLATEFORMES; ++i) {
					s[i].raison = NkString(pourquoi);
				}
				return;
			}
			const int32 W = static_cast<int32>(NkPlateformeJeu::NK_WINDOWS);
			const int32 A = static_cast<int32>(NkPlateformeJeu::NK_ANDROID);
			const int32 B = static_cast<int32>(NkPlateformeJeu::NK_WEB);
			const int32 H = static_cast<int32>(NkPlateformeJeu::NK_HARMONYOS);
			const int32 L = static_cast<int32>(NkPlateformeJeu::NK_LINUX);
			const int32 M = static_cast<int32>(NkPlateformeJeu::NK_MACOS);
			const int32 I = static_cast<int32>(NkPlateformeJeu::NK_IOS);

			// ── Windows : la chaine nk-windows-clang-mingw (config/toolchain.jenga)
#if defined(_WIN32)
			NkString clang = DansLePath("clang++");
			if (clang.Empty() && NkFile::Exists("C:/msys64/ucrt64/bin/clang++.exe")) {
				clang = NkString("C:/msys64/ucrt64/bin/clang++.exe");
			}
			s[W].disponible = !clang.Empty();
			s[W].raison = clang.Empty() ? NkString("clang++ (msys2 ucrt64) introuvable : la chaine clang-mingw de config/toolchain.jenga en a besoin")
										: NkString("clang-mingw : ") + Oblique(clang);
#else
			s[W].raison = NkString("se construit sous Windows (chaine clang-mingw) : pas depuis cette machine");
#endif

			// ── Android : SDK, NDK et JDK, cherches dans l'ordre de Jenga ─────────
			NkVector<NkString> cSdk;
			cSdk.PushBack(Env("ANDROID_SDK_ROOT"));
			cSdk.PushBack(Env("ANDROID_HOME"));
			cSdk.PushBack(Env("LOCALAPPDATA") + "/Android/Sdk");
			cSdk.PushBack(NkString("C:/Android/Sdk"));
			const NkString sdk = Premier(cSdk);
			NkVector<NkString> cNdk;
			cNdk.PushBack(Env("ANDROID_NDK_ROOT"));
			cNdk.PushBack(Env("ANDROID_NDK_HOME"));
			if (!sdk.Empty()) {
				const NkVector<NkString> versions = NkDirectory::GetDirectories((Barre(sdk) + "ndk").CStr());
				if (!versions.Empty()) {
					cNdk.PushBack(versions[versions.Size() - 1u]);
				}
				cNdk.PushBack(Barre(sdk) + "ndk-bundle");
			}
			const NkString ndk = Premier(cNdk);
			NkString jdk = Env("JAVA_HOME");
			if (jdk.Empty()) {
				jdk = DansLePath("java");
			}
			NkString manqueAndroid;
			if (sdk.Empty()) {
				manqueAndroid += " SDK (ANDROID_SDK_ROOT)";
			}
			if (ndk.Empty()) {
				manqueAndroid += " NDK (ANDROID_NDK_ROOT)";
			}
			if (jdk.Empty()) {
				manqueAndroid += " JDK (JAVA_HOME)";
			}
			s[A].disponible = manqueAndroid.Empty();
			s[A].raison = s[A].disponible ? NkString("SDK ") + sdk + ", NDK " + ndk : NkString("manque :") + manqueAndroid;

			// ── Web : emsdk ────────────────────────────────────────────────────
			NkVector<NkString> cEm;
			cEm.PushBack(Env("EMSDK"));
			cEm.PushBack(NkString("C:/emsdk"));
			cEm.PushBack(Maison() + "/emsdk");
			NkString emsdk = Premier(cEm);
			if (emsdk.Empty()) {
				emsdk = DansLePath("emcc");
			}
			s[B].disponible = !emsdk.Empty();
			s[B].raison = emsdk.Empty() ? NkString("manque : emsdk (EMSDK, C:/emsdk ou ~/emsdk) -- emcc absent du PATH")
										: NkString("emsdk : ") + emsdk;

			// ── HarmonyOS : le SDK OpenHarmony ────────────────────────────────
			NkVector<NkString> cOh;
			cOh.PushBack(Env("OHOS_SDK"));
			cOh.PushBack(Env("HARMONY_OS_SDK"));
			cOh.PushBack(Env("HARMONY_SDK"));
			cOh.PushBack(NkString("C:/ohos/command-line-tools/sdk/default/openharmony"));
			const NkString ohos = Premier(cOh);
			s[H].disponible = !ohos.Empty();
			s[H].raison = ohos.Empty() ? NkString("manque : SDK OpenHarmony (OHOS_SDK ou C:/ohos/command-line-tools)")
									   : NkString("SDK OpenHarmony : ") + ohos;

			// ── Linux, macOS, iOS : la machine elle-meme ──────────────────────
#if defined(__linux__)
			const NkString clangL = DansLePath("clang++");
			s[L].disponible = !clangL.Empty();
			s[L].raison = clangL.Empty() ? NkString("clang++ introuvable") : NkString("clang : ") + clangL;
#else
			s[L].raison = NkString("se construit SOUS Linux (ou WSL) avec clang : pas depuis cette machine");
#endif
#if defined(__APPLE__)
			const NkString xcrun = DansLePath("xcrun");
			s[M].disponible = !xcrun.Empty();
			s[M].raison = xcrun.Empty() ? NkString("Xcode introuvable (xcrun)") : NkString("Xcode : ") + xcrun;
			s[I].disponible = !xcrun.Empty();
			s[I].raison = xcrun.Empty() ? NkString("Xcode introuvable (xcrun)")
										: NkString("Xcode : ") + xcrun + " (signature Apple requise pour un appareil)";
#else
			s[M].raison = NkString("exige un Mac avec Xcode");
			s[I].raison = NkString("exige un Mac avec Xcode (et une signature Apple)");
#endif
		}

		// =====================================================================
		// LE WORKSPACE DU JEU
		// =====================================================================
		NkString NkEcrireJengaDuJeu(const NkDemandeConstruction &demande, const NkPlanConstruction &plan) {
			const NkString nk = SansBarre(plan.depot);
			const NkString jeu = SansBarre(plan.dossierJeu);
			const NkString sombre = demande.executableSombre ? NkString("icone_sombre.png") : NkString("icone_claire.png");
			const NkString livree = demande.executableSombre ? NkString("icone_claire.png") : NkString("icone_sombre.png");
			// Le nom affiche ne va qu'en COMMENTAIRE : un guillemet ou une fin de
			// ligne y casserait une chaine Python. Le vrai nom est au sommaire.
			NkString nomSur;
			for (usize i = 0; i < demande.nom.Length(); ++i) {
				const char c = demande.nom[i];
				nomSur.Append((c == '\n' || c == '\r') ? ' ' : c);
			}
			NkString id;
			for (usize i = 0; i < plan.projet.Length(); ++i) {
				const char c = plan.projet[i];
				id.Append((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
			}
			NkString t;
			t += "#!/usr/bin/env python3\n";
			t += "# -*- coding: utf-8 -*-\n";
			t += "# =============================================================================\n";
			t += NkString::Format("# %s.jenga -- ECRIT PAR UnkenyEditor (Fichier > Construire).\n", plan.projet.CStr());
			t += "# Refait a chaque construction : le retoucher a la main ne sert a rien.\n";
			t += "#\n";
			t += NkString::Format("# Le jeu « %s » = le joueur autonome d'Unkeny (Applications/UnkenyPlayer du\n", nomSur.CStr());
			t += "# depot) + ses donnees cuites (assets/) + ses icones (icones/). Le moteur est\n";
			t += "# compile ICI (Build/), depuis les sources du depot : rien n'est ecrit dans le\n";
			t += "# depot.\n";
			t += "#\n";
			t += "# A la main :\n";
			t += NkString::Format("#   jenga build --jenga-file \"%s\" --platform windows --config Debug --target %s --no-daemon\n",
								  Oblique(plan.fichierJenga).CStr(), plan.projet.CStr());
			t += "# =============================================================================\n\n";
			t += "from Jenga import *\n\n";
			t += NkString::Format("NK = r\"%s\"\n", nk.CStr());
			t += NkString::Format("JEU = r\"%s\"\n\n", jeu.CStr());
			t += "# Ce que les modules du moteur supposent (voir Nkentseu.jenga).\n";
			t += "useconfig(NK + \"/config/modules.jenga\")\n";
			t += "useconfig(NK + \"/config/toolchain.jenga\")\n";
			t += "useconfig(NK + \"/config/graphics.jenga\")\n";
			t += "useconfig(NK + \"/config/wayland.jenga\")\n\n";
			t += "# Ce qui est PROPRE A CE JEU. Le projet est declare par\n";
			t += "# Applications/UnkenyPlayer/UnkenyPlayer.jenga, qui lit ce dictionnaire.\n";
			t += "UNKENY_JEU = {\n";
			t += NkString::Format("    \"projet\": \"%s\",\n", plan.projet.CStr());
			t += "    \"donnees\": JEU + \"/assets\",\n";
			t += NkString::Format("    \"icone\": JEU + \"/icones/%s\",\n", sombre.CStr());
			t += NkString::Format("    \"icone_livree\": JEU + \"/icones/%s\",\n", livree.CStr());
			t += NkString::Format("    \"id\": \"com.unkeny.%s\",\n", id.CStr());
			t += "    \"version\": \"1.0.0\",\n";
			t += "}\n\n";
			t += NkString::Format("with workspace(\"%s\"):\n", plan.projet.CStr());
			t += "    nkentseutoolchain()\n";
			t += "    configurations([\"Debug\", \"Release\"])\n";
			t += "    # Les options que les FILTRES des modules lisent (Nkentseu.jenga).\n";
			t += "    newoption(trigger=\"linux-backend\", value=\"BACKEND\",\n";
			t += "              allowed=[[\"xlib\", \"X11/XLib\"], [\"xcb\", \"X11/XCB\"], [\"wayland\", \"Wayland\"], [\"headless\", \"sans fenetre\"]],\n";
			t += "              default=\"xlib\", description=\"Backend de fenetrage Linux\")\n";
			t += "    newoption(trigger=\"headless\", description=\"[Obsolete] linux-backend=headless\")\n";
			t += "    newoption(trigger=\"windows-runtime\", value=\"RUNTIME\",\n";
			t += "              allowed=[[\"desktop\", \"Win32\"], [\"uwp\", \"UWP\"]],\n";
			t += "              default=\"desktop\", description=\"Runtime Windows cible\")\n";
			t += "    targetoses([TargetOS.WINDOWS, TargetOS.LINUX, TargetOS.MACOS, TargetOS.ANDROID,\n";
			t += "                TargetOS.IOS, TargetOS.WEB, TargetOS.HARMONYOS])\n";
			t += "    targetarchs([TargetArch.X86_64, TargetArch.ARM64, TargetArch.WASM32])\n";
			t += "    # AVANT les includes : chaque module declare ses tests.\n";
			t += "    with unitest() as u:\n";
			t += "        u.Compile()\n\n";
			const NkVector<NkString> &modules = NkModulesDuJoueur();
			for (usize i = 0; i < modules.Size(); ++i) {
				t += NkString::Format("    with include(NK + \"/%s\"):\n", modules[i].CStr());
				t += "        pass\n";
			}
			t += "\n    # Le joueur, qui lit UNKENY_JEU.\n";
			t += "    with include(NK + \"/Applications/UnkenyPlayer/UnkenyPlayer.jenga\"):\n";
			t += "        pass\n\n";
			t += NkString::Format("    startproject(\"%s\")\n", plan.projet.CStr());
			return t;
		}

		// =====================================================================
		// ETAPE 1 : cuire, poser, ecrire
		// =====================================================================
		bool NkPreparerConstruction(NkEditeurModele &m, const NkDemandeConstruction &demande, float32 vueLargeur,
									float32 vueHauteur, NkPlanConstruction &plan, NkVector<NkString> &journal) {
			plan = NkPlanConstruction();
			plan.depot = NkTrouverDepot();
			if (plan.depot.Empty()) {
				journal.PushBack(NkString("[construire] sources du moteur introuvables : poser NK_UNKENY_DEPOT, ou lancer l'editeur depuis le depot"));
				return false;
			}
			if (demande.iconeSombre.Empty() != demande.iconeClaire.Empty()) {
				journal.PushBack(NkString("[construire] l'icone doit exister en version SOMBRE ET CLAIRE (R17) : donnez les deux, ou aucune (Unkeny fabrique les siennes)"));
				return false;
			}
			plan.projet = NkIdentifiantJeu(demande.nom.CStr());
			const NkString sortie = demande.sortie.Empty() ? NkSortieParDefaut() : demande.sortie;
			plan.dossierJeu = Barre(sortie) + plan.projet + "/";
			plan.fichierJenga = plan.dossierJeu + plan.projet + ".jenga";
			plan.journal = plan.dossierJeu + "construire.log";
			NkDirectory::CreateRecursive((plan.dossierJeu + "icones").CStr());
			NkDirectory::CreateRecursive((plan.dossierJeu + ".jenga-typings").CStr());
			NkFile::WriteAllText(plan.journal.CStr(), "");

			// On construit ce qu'on EDITE : en jeu, la photo d'avant « Jouer »
			// serait perdue au profit d'un instant de simulation (meme regle que
			// NkEditeurSauver).
			if (m.etat != NkEtatJeu::NK_EDITION) {
				NkEditeurArreter(m);
			}

			// ── La cuisson, dans un dossier VIDE : une texture d'une construction
			//    precedente ne doit pas y trainer (le sommaire ne la citerait pas,
			//    mais le paquet l'emporterait).
			const NkString donnees = plan.dossierJeu + "assets";
			NkDirectory::Delete(donnees.CStr(), true);
			unkeny::NkDemandeCuisson cuisson;
			cuisson.dossier = donnees;
			cuisson.nomJeu = demande.nom.Empty() ? plan.projet : demande.nom;
			cuisson.vueLargeur = vueLargeur;
			cuisson.vueHauteur = vueHauteur;
			cuisson.entrees = demande.entrees;
			unkeny::NkRapportCuisson rapport;
			const bool cuit = unkeny::NkCuireJeu(m.scene, m.textures, cuisson, rapport);
			for (usize i = 0; i < rapport.erreurs.Size(); ++i) {
				journal.PushBack(NkString("[cuisson] ") + rapport.erreurs[i]);
			}
			if (!cuit) {
				journal.PushBack(NkString("[cuisson] ECHEC : rien n'est construit"));
				return false;
			}
			plan.empreinte = rapport.empreinte;
			journal.PushBack(NkString::Format("[cuisson] scene + %u texture(s) + %u son(s) -> %s (empreinte %016llx)",
											  static_cast<unsigned>(rapport.textures), static_cast<unsigned>(rapport.sons),
											  donnees.CStr(), static_cast<unsigned long long>(rapport.empreinte)));

			// ── Les icones (R17) ─────────────────────────────────────────────
			const NkString sombre = plan.dossierJeu + "icones/icone_sombre.png";
			const NkString claire = plan.dossierJeu + "icones/icone_claire.png";
			if (demande.iconeSombre.Empty()) {
				if (!NkFabriquerIcones(sombre.CStr(), claire.CStr())) {
					journal.PushBack(NkString("[icones] fabrication impossible : ") + plan.dossierJeu + "icones");
					return false;
				}
				journal.PushBack(NkString("[icones] fabriquees par Unkeny : icone_sombre.png et icone_claire.png"));
			} else {
				if (!NkFile::Copy(demande.iconeSombre.CStr(), sombre.CStr(), true)) {
					journal.PushBack(NkString("[icones] icone sombre illisible : ") + demande.iconeSombre);
					return false;
				}
				if (!NkFile::Copy(demande.iconeClaire.CStr(), claire.CStr(), true)) {
					journal.PushBack(NkString("[icones] icone claire illisible : ") + demande.iconeClaire);
					return false;
				}
				journal.PushBack(NkString("[icones] copiees : ") + demande.iconeSombre + " et " + demande.iconeClaire);
			}
			journal.PushBack(NkString("[icones] l'executable porte la version ") + (demande.executableSombre ? "SOMBRE" : "CLAIRE") +
							 ", l'autre est livree a cote");

			// ── Le workspace ─────────────────────────────────────────────────
			NkFile::WriteAllText((plan.dossierJeu + ".jenga-typings/jengaconfig.py").CStr(),
								 "# Talon vide : les modules du moteur font `from jengaconfig import *`.\n");
			if (!NkFile::WriteAllText(plan.fichierJenga.CStr(), NkEcrireJengaDuJeu(demande, plan).CStr())) {
				journal.PushBack(NkString("[construire] ecriture impossible : ") + plan.fichierJenga);
				return false;
			}
			journal.PushBack(NkString("[construire] workspace du jeu : ") + plan.fichierJenga);

			// ── Les commandes ───────────────────────────────────────────────
			const NkInfoCible &cible = Cible(demande.plateforme);
			const char *config = demande.profil == NkProfilJeu::NK_DEVELOPPEMENT ? "Debug" : "Release";
			NkEtapeConstruction build;
			build.libelle = NkString::Format("Jenga : moteur et jeu pour %s (%s)", cible.nom, config);
			build.commande = NkString::Format("jenga build --jenga-file %s --platform %s --config %s --target %s --no-daemon",
											  Guillemets(plan.fichierJenga).CStr(), cible.jenga, config, plan.projet.CStr());
			plan.etapes.PushBack(build);
			plan.resultat = plan.dossierJeu + "Build/Bin/" + config + "-" + cible.systeme + "/" + plan.projet + "/" + plan.projet +
							cible.extension;
			if (demande.profil == NkProfilJeu::NK_EXPEDITION) {
				NkEtapeConstruction paquet;
				paquet.libelle = NkString::Format("Jenga : paquet %s", cible.nom);
				const bool zip = demande.plateforme == NkPlateformeJeu::NK_WINDOWS || demande.plateforme == NkPlateformeJeu::NK_WEB;
				paquet.commande = NkString::Format("jenga package --jenga-file %s --platform %s --config Release --project %s --output %s%s --no-daemon",
												   Guillemets(plan.fichierJenga).CStr(), cible.jenga, plan.projet.CStr(),
												   Guillemets(plan.dossierJeu + "Livraison").CStr(), zip ? " --type zip" : "");
				plan.etapes.PushBack(paquet);
				plan.paquet = plan.dossierJeu + "Livraison";
			}
			return true;
		}

		// =====================================================================
		// ETAPE 3 : ranger le resultat
		// =====================================================================
		bool NkAcheverConstruction(const NkDemandeConstruction &demande, NkPlanConstruction &plan,
								   NkVector<NkString> &journal) {
			const NkInfoCible &cible = Cible(demande.plateforme);
			NkString produit = plan.resultat;
			if (!Existe(produit)) {
				// Le dossier de sortie de Jenga n'est MESURE que sous Windows ; ailleurs
				// on cherche le produit par son extension plutot que de le deviner.
				produit = ChercherProduit(plan.dossierJeu + "Build/Bin", cible.extension);
			}
			if (produit.Empty() || !Existe(produit)) {
				journal.PushBack(NkString("[construire] rien de produit la ou on l'attendait : ") + plan.resultat);
				return false;
			}
			const bool ordinateur = demande.plateforme == NkPlateformeJeu::NK_WINDOWS || demande.plateforme == NkPlateformeJeu::NK_LINUX ||
									demande.plateforme == NkPlateformeJeu::NK_MACOS;
			if (ordinateur) {
				// Sur ordinateur, le joueur lit `assets/` A COTE DE SON EXECUTABLE
				// (NkDossierDuJeuParDefaut) : Jenga ne copie les dependfiles qu'au
				// paquet, on les pose donc ici pour que l'executable du build se
				// lance tel quel.
				const NkString dossierExe = Barre(NkPath(produit.CStr()).GetParent().ToString());
				NkDirectory::Delete((dossierExe + "assets").CStr(), true);
				if (!NkDirectory::Copy((plan.dossierJeu + "assets").CStr(), (dossierExe + "assets").CStr(), true, true)) {
					journal.PushBack(NkString("[construire] copie des donnees impossible vers ") + dossierExe + "assets");
					return false;
				}
				const char *livree = demande.executableSombre ? "icone_claire.png" : "icone_sombre.png";
				NkFile::Copy((plan.dossierJeu + "icones/" + livree).CStr(), (dossierExe + livree).CStr(), true);
				journal.PushBack(NkString("[construire] donnees et icone ") + livree + " posees a cote de l'executable");
			}
			plan.resultat = produit;
			journal.PushBack(NkString("[construire] LE JEU : ") + produit);
#if defined(_WIN32)
			if (demande.plateforme == NkPlateformeJeu::NK_WINDOWS) {
				plan.verification = Guillemets(produit) + " --verifier";
			}
#endif
			if (!plan.paquet.Empty()) {
				const NkString zip = ChercherProduit(plan.paquet, ".zip");
				const NkString apk = ChercherProduit(plan.paquet, ".apk");
				const NkString p = !zip.Empty() ? zip : apk;
				if (p.Empty()) {
					journal.PushBack(NkString("[construire] aucun paquet dans ") + plan.paquet);
					return false;
				}
				journal.PushBack(NkString("[construire] LE PAQUET : ") + p);
			}
			return true;
		}

		NkString NkLigneDeJenga(const NkString &ligne) {
			NkString sortie;
			const uint8 *p = reinterpret_cast<const uint8 *>(ligne.CStr());
			for (usize i = 0; p[i] != 0u; ++i) {
				// U+2500..U+259F (filets, cadres, blocs) : E2 94..96 xx en UTF-8.
				if (p[i] == 0xE2u && p[i + 1] >= 0x94u && p[i + 1] <= 0x96u && p[i + 2] != 0u) {
					i += 2;
					continue;
				}
				sortie.Append(static_cast<char>(p[i]));
			}
			// Les espaces de tete et de queue que laissent les cadres.
			usize debut = 0;
			while (debut < sortie.Length() && sortie[debut] == ' ') {
				++debut;
			}
			usize fin = sortie.Length();
			while (fin > debut && sortie[fin - 1u] == ' ') {
				--fin;
			}
			return sortie.SubStr(debut, fin - debut);
		}

		// =====================================================================
		// LA LIGNE DE COMMANDE
		// =====================================================================
		namespace {
			void Ecrire(const NkString &ligne, void *donnees) {
				const NkString *fichier = static_cast<const NkString *>(donnees);
				AuFichier(*fichier, ligne);
				const NkString propre = NkLigneDeJenga(ligne);
				if (!propre.Empty()) {
					std::printf("  %s\n", propre.CStr());
					std::fflush(stdout);
				}
			}

			void Aide() {
				std::printf("UnkenyEditor --construire=PLATEFORME [options]\n");
				std::printf("  PLATEFORME          windows, android, web, harmonyos, linux, macos, ios\n");
				std::printf("  --scene=FICHIER     la scene .nkscene a construire (defaut : la scene neuve de l'editeur)\n");
				std::printf("  --sortie=DOSSIER    le dossier parent du jeu (defaut : Documents/Unkeny/Jeux)\n");
				std::printf("  --nom=NOM           le nom du jeu (defaut : MonJeu)\n");
				std::printf("  --profil=P          developpement (Debug) ou expedition (Release + paquet)\n");
				std::printf("  --icone-sombre=PNG --icone-claire=PNG   les deux, ou aucune (R17)\n");
				std::printf("  --icone-exe=claire  l'executable porte la claire (defaut : la sombre)\n");
				std::printf("  --vue=LxH           le viseur de reference, en pixels (defaut : 1280x720)\n");
			}
		} // namespace

		int32 NkEditeurConstruireEnLigne(const NkVector<NkString> &args) {
			NkDemandeConstruction d;
			d.nom = NkString("MonJeu");
			NkString scene;
			NkString plateforme;
			float32 vueL = 1280.f;
			float32 vueH = 720.f;
			for (uint32 i = 0; i < args.Size(); ++i) {
				const NkString &a = args[i];
				if (a.StartsWith("--construire=")) {
					plateforme = NkString(a.SubStr(13));
				} else if (a.StartsWith("--scene=")) {
					scene = NkString(a.SubStr(8));
				} else if (a.StartsWith("--sortie=")) {
					d.sortie = NkString(a.SubStr(9));
				} else if (a.StartsWith("--nom=")) {
					d.nom = NkString(a.SubStr(6));
				} else if (a.StartsWith("--profil=")) {
					const NkString p(a.SubStr(9));
					d.profil = (p.StartsWith("exp") || p == "release") ? NkProfilJeu::NK_EXPEDITION : NkProfilJeu::NK_DEVELOPPEMENT;
				} else if (a.StartsWith("--icone-sombre=")) {
					d.iconeSombre = NkString(a.SubStr(15));
				} else if (a.StartsWith("--icone-claire=")) {
					d.iconeClaire = NkString(a.SubStr(15));
				} else if (a == "--icone-exe=claire") {
					d.executableSombre = false;
				} else if (a.StartsWith("--vue=")) {
					const NkString v(a.SubStr(6));
					char *fin = nullptr;
					vueL = std::strtof(v.CStr(), &fin);
					vueH = (fin != nullptr && *fin == 'x') ? std::strtof(fin + 1, nullptr) : vueL * 0.5625f;
				}
			}
			if (!NkPlateformeJeuDepuis(plateforme.CStr(), d.plateforme)) {
				Aide();
				return 2;
			}
			NkDisponibilite dispo[NK_NB_PLATEFORMES];
			NkDetecterPlateformes(dispo);
			const NkDisponibilite &ici = dispo[static_cast<int32>(d.plateforme)];
			std::printf("UnkenyEditor — construire « %s » pour %s : %s\n", d.nom.CStr(), NkPlateformeJeuNom(d.plateforme),
						ici.raison.CStr());
			if (!ici.disponible) {
				std::printf("CONSTRUCTION IMPOSSIBLE sur cette machine : %s\n", ici.raison.CStr());
				return 3;
			}

			// La scene : celle que l'editeur ouvrirait, avec ses ressources.
			NkEditeurModele *pm = memory::NkGetDefaultAllocator().New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			if (scene.Empty()) {
				NkEditeurNouvelleScene(m);
			} else {
				m.chemin = scene;
				if (!NkEditeurOuvrir(m)) {
					std::printf("SCENE ILLISIBLE : %s (%s)\n", scene.CStr(), m.message.CStr());
					memory::NkGetDefaultAllocator().Delete(pm);
					return 4;
				}
				// Les entrees de la scene, a cote d'elle (.nkentrees), comme
				// l'editeur les relirait ; sans fichier, le joueur prend les
				// liaisons standard.
				const NkString entrees = NkEditeurEntreesFichier(scene.CStr());
				if (NkFile::Exists(entrees.CStr())) {
					d.entrees = NkFile::ReadAllText(entrees.CStr());
				}
			}
			NkPlanConstruction plan;
			NkVector<NkString> journal;
			const bool pret = NkPreparerConstruction(m, d, vueL, vueH, plan, journal);
			memory::NkGetDefaultAllocator().Delete(pm);
			for (usize i = 0; i < journal.Size(); ++i) {
				Ecrire(journal[i], &plan.journal);
			}
			if (!pret) {
				std::printf("CONSTRUCTION IMPOSSIBLE\n");
				return 1;
			}
			for (usize e = 0; e < plan.etapes.Size(); ++e) {
				const NkEtapeConstruction &etape = plan.etapes[e];
				Ecrire(NkString("[etape] ") + etape.libelle, &plan.journal);
				Ecrire(NkString("[etape] ") + etape.commande, &plan.journal);
				NkEditeurProcessus proc;
				proc.Environnement("JENGA_NO_IDE_CONFIG", NK_CONSTRUIRE_SANS_IDE);
				if (!proc.Lancer(etape.commande, plan.dossierJeu)) {
					std::printf("LANCEMENT IMPOSSIBLE : %s\n", etape.commande.CStr());
					return 1;
				}
				const int32 code = proc.Attendre(&Ecrire, &plan.journal);
				if (code != 0) {
					Ecrire(NkString::Format("[etape] ECHEC (code %d) -- tout est dans %s", code, plan.journal.CStr()), &plan.journal);
					std::printf("CONSTRUCTION EN ECHEC\n");
					return 1;
				}
			}
			journal.Clear();
			const bool range = NkAcheverConstruction(d, plan, journal);
			for (usize i = 0; i < journal.Size(); ++i) {
				Ecrire(journal[i], &plan.journal);
			}
			if (!range) {
				std::printf("CONSTRUCTION EN ECHEC\n");
				return 1;
			}
			std::printf("CONSTRUIT : %s\n", plan.resultat.CStr());
			if (plan.verification.Empty()) {
				return 0;
			}
			// Le jeu produit relit SES donnees, sans fenetre : l'empreinte doit etre
			// celle que l'editeur a cuite (temoin l1, sur le vrai produit).
			Ecrire(NkString("[verifier] ") + plan.verification, &plan.journal);
			NkEditeurProcessus verif;
			if (!verif.Lancer(plan.verification, plan.dossierJeu)) {
				std::printf("VERIFICATION IMPOSSIBLE A LANCER\n");
				return 1;
			}
			const int32 code = verif.Attendre(&Ecrire, &plan.journal);
			std::printf("%s\n", code == 0 ? "VERIFIE : le jeu construit relit la scene de l'editeur (meme empreinte)"
										  : "VERIFICATION EN ECHEC");
			return code == 0 ? 0 : 5;
		}

	} // namespace editeur
} // namespace nkentseu
