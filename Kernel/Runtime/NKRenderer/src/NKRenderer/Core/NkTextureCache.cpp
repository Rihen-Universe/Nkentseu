// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkTextureCache.cpp — cache d'actifs cuits, nomme par empreinte du CONTENU
// =============================================================================
#include "NkTextureCache.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include "NKSerialization/Asset/NkTextureAssetFormat.h"

#include <cstdlib>

namespace nkentseu {
	namespace renderer {

		namespace {

			nk_uint32 g_touches = 0u;
			nk_uint32 g_manques = 0u;
			nk_uint32 g_refus = 0u;
			float64 g_msLecture = 0.0;
			float64 g_msDecodage = 0.0;
			float64 g_msTeleversement = 0.0;
			nk_uint64 g_octetsLus = 0u;
			NkString g_decision;

			constexpr nk_uint64 kFnvBase = 14695981039346656037ULL;
			constexpr nk_uint64 kFnvPrime = 1099511628211ULL;

			inline nk_uint64 Melanger(nk_uint64 h, const void *donnees, nk_size n) noexcept {
				const nk_uint8 *p = static_cast<const nk_uint8 *>(donnees);
				for (nk_size i = 0; i < n; ++i)
					h = (h ^ p[i]) * kFnvPrime;
				return h;
			}

		} // namespace

		bool NkTextureCache::Actif() noexcept {
			// Lu UNE fois : un interrupteur qui change en cours d'execution
			// rendrait la mesure incomparable d'un chargement a l'autre.
			static const bool s_actif = [] {
				const char *v = std::getenv("NK_TEX_CACHE");
				return !(v && v[0] == '0' && v[1] == '\0');
			}();
			return s_actif;
		}

		// Un renvoi, et c'est le point : le four (`Tools/NkTexBake`) et le moteur
		// doivent calculer la MEME empreinte, sinon la pre-cuisson ne sert jamais
		// et rien ne le signale. La definition unique vit dans `NkTextureOven.h`.
		nk_uint64 NkTextureCache::Empreinte(const char *cheminSource, const NkTexOvenReglages &reglages) noexcept {
			return NkTexCacheNommage::Empreinte(cheminSource, reglages);
		}

		NkString NkTextureCache::Chemin(nk_uint64 empreinte) noexcept {
			return NkTexCacheNommage::Chemin(empreinte);
		}

		bool NkTextureCache::Existe(const NkString &chemin) noexcept {
			return NkFile::Exists(chemin.CStr());
		}

		bool NkTextureCache::Ecrire(const NkString &chemin, const nk_uint8 *payload, nk_size taille,
									const NkString &cheminSource) noexcept {
			if (!payload || taille == 0u)
				return false;

			if (!NkDirectory::Exists(Racine()) && !NkDirectory::CreateRecursive(Racine())) {
				logger.Warn("[NkTextureCache] impossible de creer {0} : les actifs seront recuits a chaque "
							"lancement\n",
							Racine());
				return false;
			}

			NkString err;
			// Le chemin logique porte l'empreinte : deux sources differentes ne
			// peuvent pas partager d'identifiant d'actif.
			NkString logique = NkString("/Cache/");
			{
				NkString base(chemin);
				const nk_size barre = base.RFind('/');
				if (barre != NkString::npos)
					base = base.SubStr(barre + 1);
				logique.Append(base.View());
			}
			if (!NkEcrireActifTexture(payload, taille, chemin.CStr(), logique.View(), cheminSource.View(), nullptr,
									  &err)) {
				logger.Warn("[NkTextureCache] ecriture refusee ({0}) : {1}\n", err.CStr(), chemin.CStr());
				return false;
			}
			return true;
		}

		nk_uint32 NkTextureCache::Vider() noexcept {
			if (!NkDirectory::Exists(Racine()))
				return 0u;
			NkVector<NkString> fichiers =
				NkDirectory::GetFiles(Racine(), "*.nktex", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
			nk_uint32 n = 0u;
			for (nk_size i = 0; i < fichiers.Size(); ++i) {
				if (NkFile::Delete(fichiers[i].CStr()))
					++n;
			}
			// On le DIT : un geste qui efface des fichiers sans laisser de trace
			// se confond avec un geste qui n'a rien fait.
			logger.Info("[NkTextureCache] cache vide : {0} actif(s) supprime(s) sur {1} — ils seront recuits au "
						"prochain chargement\n",
						n, (unsigned long long)fichiers.Size());
			return n;
		}

		bool NkTextureCache::Invalider(const char *cheminSource, const NkTexOvenReglages &reglages) noexcept {
			const nk_uint64 empreinte = Empreinte(cheminSource, reglages);
			if (empreinte == 0u)
				return false;
			const NkString chemin = Chemin(empreinte);
			if (!NkFile::Exists(chemin.CStr()))
				return false;
			return NkFile::Delete(chemin.CStr());
		}

		nk_uint32 NkTextureCache::Touches() noexcept {
			return g_touches;
		}

		nk_uint32 NkTextureCache::Manques() noexcept {
			return g_manques;
		}

		nk_uint32 NkTextureCache::Refus() noexcept {
			return g_refus;
		}

		void NkTextureCache::RemettreCompteursAZero() noexcept {
			g_touches = 0u;
			g_manques = 0u;
			g_refus = 0u;
			g_msLecture = 0.0;
			g_msDecodage = 0.0;
			g_msTeleversement = 0.0;
			g_octetsLus = 0u;
			g_decision = NkString();
		}

		float64 NkTextureCache::MsLecture() noexcept {
			return g_msLecture;
		}

		float64 NkTextureCache::MsDecodage() noexcept {
			return g_msDecodage;
		}

		float64 NkTextureCache::MsTeleversement() noexcept {
			return g_msTeleversement;
		}

		nk_uint64 NkTextureCache::OctetsLus() noexcept {
			return g_octetsLus;
		}

		void NkTextureCache::AjouterMsLecture(float64 ms, nk_uint64 octets) noexcept {
			g_msLecture += ms;
			g_octetsLus += octets;
		}

		void NkTextureCache::AjouterMsDecodage(float64 ms) noexcept {
			g_msDecodage += ms;
		}

		void NkTextureCache::AjouterMsTeleversement(float64 ms) noexcept {
			g_msTeleversement += ms;
		}

		NkString NkTextureCache::Resume() noexcept {
			const nk_uint32 total = g_touches + g_manques + g_refus;
			if (total == 0u)
				return NkString("aucune texture chargee par le cache");
			NkString s = NkString::Fmtf("%u texture(s) : %u depuis le cache", total, g_touches);
			if (g_octetsLus > 0u)
				s.Append(NkString::Fmtf(" (%.1f Mo lus)", double(g_octetsLus) / 1048576.0).View());
			s.Append(NkString::Fmtf(", %u cuite(s)", g_manques).View());
			if (g_refus > 0u)
				s.Append(NkString::Fmtf(", %u non cuisinable(s)", g_refus).View());
			if (!g_decision.Empty()) {
				s.Append(" — ");
				s.Append(g_decision.View());
			}
			if (!Actif())
				s.Append("  [CACHE COUPE : NK_TEX_CACHE=0]");
			return s;
		}

		NkString NkTextureCache::DerniereDecision() noexcept {
			return g_decision;
		}

		void NkTextureCache::NoterDecision(const NkString &d) noexcept {
			if (!d.Empty())
				g_decision = d;
		}

		void NkTextureCache::CompterTouche() noexcept {
			++g_touches;
		}

		void NkTextureCache::CompterManque() noexcept {
			++g_manques;
		}

		void NkTextureCache::CompterRefus() noexcept {
			++g_refus;
		}

	} // namespace renderer
} // namespace nkentseu
