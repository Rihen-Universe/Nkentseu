// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkRendererResourcePath.cpp — cf. l'en-tete : pourquoi une troisieme racine.
//
// (2026-10-01, integration) LA RECHERCHE N'EST PLUS ECRITE ICI. Elle s'appuie
// sur la fonction COMMUNE `NkPath::LocateResource` (NKFileSystem), celle que
// NKCode, NKUIDesign, Nogee et NogeDemo emploient deja. Deux remontees ecrites
// deux fois finissent par diverger (ordre des dossiers, nombre de niveaux,
// Android) ; il n'y en a plus qu'une. L'ordre de `LocateResource` garde le
// contrat de l'en-tete : le dossier COURANT d'abord, puis l'executable et ses
// parents -- un lancement depuis la racine voit les memes fichiers qu'avant.
// =============================================================================
#include "NkRendererResourcePath.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			/// Le dossier qui CONTIENT `Resources/NKRenderer`, barre finale
			/// comprise : le resultat de LocateResource sans ce suffixe.
			NkString Chercher() {
				static const char kSuffixe[] = "Resources/NKRenderer";
				const NkString p = NkPath::LocateResource(kSuffixe, false);
				if (p.Empty())
					return NkString();
				const usize n = sizeof(kSuffixe) - 1u;
				// LocateResource rend des '/' ; un chemin qui n'est QUE le suffixe
				// (paquet Android, trouve tel quel) n'a pas de racine a ajouter.
				if (p.Size() <= n)
					return NkString();
				return p.SubStr(0, p.Size() - n);
			}
		} // namespace

		const NkString &NkRendererResourceRoot() noexcept {
			// Statique locale : initialisation unique et sure entre threads (C++11).
			static const NkString sRacine = Chercher();
			return sRacine;
		}

		NkString NkRendererResolvePath(const NkString &relatif) noexcept {
			if (relatif.Empty())
				return relatif;
			// Meme ordre que l'ancienne recherche (dossier courant d'abord), mais
			// par la porte commune. Introuvable : le chemin d'origine INCHANGE,
			// pour que le message de l'appelant dise ce qu'on cherchait.
			const NkString p = NkPath::LocateResource(relatif.CStr(), false);
			return p.Empty() ? relatif : p;
		}

		NkString NkRendererResourceSearchReport() noexcept {
			const NkString &racine = NkRendererResourceRoot();
			if (!racine.Empty())
				return NkString("racine trouvee par NkPath::LocateResource : ") + racine;
			return NkString("aucun dossier contenant Resources/NKRenderer (NkPath::LocateResource : dossier "
							"courant, executable et parents) depuis ") +
				   NkPath::GetExecutableDirectory().ToString();
		}

	} // namespace renderer
} // namespace nkentseu
