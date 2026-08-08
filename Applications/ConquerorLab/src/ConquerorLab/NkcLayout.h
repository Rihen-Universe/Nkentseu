#pragma once
// =============================================================================
// NkcLayout — OU sont les choses. Deux dispositions, une seule reponse.
//
// LE PROBLEME
// -----------
// L'atelier tourne dans deux mondes :
//
//   DEPOT      d:/.../Nkentseu/            arborescence de developpement
//   KIT        ConquerorLab-Kit/           ce qu'on livre au stagiaire
//
// La premiere version du kit RECOPIAIT l'arborescence du depot :
//
//     Kernel/Foundation/NKCore/src/NKCore/NkTypes.h
//     Kernel/System/NKLogger/src/NKLogger/NkLog.h
//     ... treize racines d'inclusion
//
// C'etait pratique pour le script et absurde pour le stagiaire. `Foundation`,
// `System`, `src` sont de la plomberie de DEPOT : trois niveaux qui ne veulent
// rien dire pour quelqu'un qui veut juste ecrire des regles de jeu, et qui
// separent les modules de ce qu'ils incluent.
//
// LE KIT A DONC SA PROPRE DISPOSITION, PLATE
// ------------------------------------------
//     include/Conqueror/ConquerorRulesABI.h
//     include/NKCore/NkTypes.h
//     include/NKMath/NkFunctions.h
//     include/NKContainers/String/NkString.h
//     lib/NKCore.lib ...
//     travail/rules|ai|boards
//
// UNE seule racine d'inclusion, et elle correspond exactement a ce que le
// stagiaire ecrit : #include "NKMath/NkFunctions.h". Ce qu'on voit dans le
// dossier est ce qu'on tape dans le code.
//
// CE FICHIER EST LE SEUL A CONNAITRE LES DEUX. Tout le reste demande.
// =============================================================================

#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"

namespace nkentseu {
	namespace conqueror {

		/// Disposition detectee.
		enum class NkcLayoutKind : uint8 {
			Repo = 0,  ///< arborescence de developpement
			Kit	 = 1   ///< dossier livre au stagiaire
		};

		class NkcLayout {
			public:
				/// `root` : dossier detecte par FindRepoRoot.
				void Init(const NkString &root) noexcept {
					mRoot = root;
					// Le marqueur du kit : un include/ plat qui contient le contrat.
					NkString probe = root;
					probe += "/include/Conqueror/ConquerorRulesABI.h";
					mKind = NkFile::Exists(probe.CStr()) ? NkcLayoutKind::Kit : NkcLayoutKind::Repo;
				}

				NkcLayoutKind Kind() const noexcept { return mKind; }
				bool IsKit() const noexcept { return mKind == NkcLayoutKind::Kit; }
				const NkString &Root() const noexcept { return mRoot; }

				/// Ou le stagiaire depose son travail.
				NkString WorkDir(const char *sub) const noexcept {
					NkString d = mRoot;
					d += IsKit() ? "/travail/" : "/Build/ConquerorLab/";
					d += sub;
					return d;
				}

				/// Racines d'inclusion passees au compilateur de modules.
				/// Le kit n'en a QU'UNE — c'est tout l'interet de sa disposition.
				/// `out` doit pouvoir accueillir 16 entrees ; renvoie le nombre ecrit.
				int32 Includes(NkString *out, int32 cap) const noexcept {
					if (!out || cap <= 0) return 0;
					if (IsKit()) {
						out[0] = mRoot;
						out[0] += "/include";
						return 1;
					}
					// Depot : les modules vivent chacun dans son <module>/src.
					// CETTE LISTE EST LA SOURCE : Distribuer.ps1 la recopie pour
					// construire include/, et le cours la cite.
					static const char *kRepo[] = {
						"/Applications/ConquerorLab/include",
						"/Kernel/Foundation/NKCore/src",
						"/Kernel/Foundation/NKPlatform/src",
						"/Kernel/Foundation/NKMemory/src",
						"/Kernel/Foundation/NKContainers/src",
						"/Kernel/Foundation/NKMath/src",
						"/Kernel/System/NKLogger/src",
						"/Kernel/System/NKTime/src",
						"/Kernel/System/NKStream/src",
						"/Kernel/System/NKFileSystem/src",
						"/Kernel/System/NKThreading/src",
						"/Kernel/System/NKSerialization/src",
						"/Kernel/System/NKReflection/src",
					};
					const int32 n = static_cast<int32>(sizeof(kRepo) / sizeof(kRepo[0]));
					int32		w = 0;
					for (int32 i = 0; i < n && w < cap; ++i) {
						out[w] = mRoot;
						out[w] += kRepo[i];
						++w;
					}
					return w;
				}

				/// Bibliotheque de grilles LIVREE (lecture seule), recopiee au
				/// premier lancement dans le dossier de travail du stagiaire.
				NkString SeedBoardsDir() const noexcept {
					NkString d = mRoot;
					d += IsKit() ? "/exemples/boards" : "/Applications/ConquerorLab/boards";
					return d;
				}

				/// Dossier des bibliotheques a lier.
				NkString LibDir() const noexcept {
					NkString d = mRoot;
					if (IsKit()) {
						d += "/lib";
						return d;
					}
					// Depot : la configuration de l'atelier lui-meme. Un module
					// Release lie a des .lib Debug melangerait deux runtimes.
					d += "/Build/Lib/";
#if defined(NDEBUG)
					d += "Release";
#else
					d += "Debug";
#endif
#if defined(_WIN32)
					d += "-Windows";
#elif defined(__APPLE__)
					d += "-macOS";
#else
					d += "-Linux";
#endif
					return d;
				}

				/// Ce qu'on affiche dans le panneau Modules — le stagiaire doit
				/// pouvoir dire OU l'atelier cherche sans ouvrir le code.
				const char *KindName() const noexcept {
					return IsKit() ? "kit" : "depot";
				}

			private:
				NkString	  mRoot;
				NkcLayoutKind mKind = NkcLayoutKind::Repo;
		};

	} // namespace conqueror
} // namespace nkentseu
