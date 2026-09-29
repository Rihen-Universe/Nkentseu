// =============================================================================
// NkUnkenyTextures.cpp
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Rendu/NkUnkenyTextures.h"

#include "NKImage/Codecs/SVG/NkSVGCodec.h"
#include "NKImage/Core/NkImage.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			void Copier(char *dst, usize taille, const char *src) {
				if (taille == 0) {
					return;
				}
				usize i = 0;
				if (src != nullptr) {
					for (; src[i] != '\0' && i + 1 < taille; ++i) {
						dst[i] = src[i];
					}
				}
				dst[i] = '\0';
			}

			bool EstSvg(const char *chemin) {
				const char *point = std::strrchr(chemin, '.');
				return point != nullptr && (std::strcmp(point, ".svg") == 0 || std::strcmp(point, ".SVG") == 0);
			}

			/// Ramene n'importe quelle image decodee a du RGBA 8 bits.
			/// ⚠️ Un PNG en niveaux de gris ou sans alpha arrive a 1 ou 3 canaux
			/// meme quand on en demande 4 a certains codecs : on ne suppose rien.
			bool VersRGBA(NkImage &img) {
				if (!img.IsValid()) {
					return false;
				}
				if (img.Format() != NkImagePixelFormat::NK_RGBA32) {
					NkImage c = img.Convert(NkImagePixelFormat::NK_RGBA32);
					if (!c.IsValid()) {
						return false;
					}
					img = static_cast<NkImage &&>(c);
				}
				return true;
			}
		} // namespace

		NkTextures2D::NkTextures2D() {
#if defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_ANDROID)
			Copier(mRacine, sizeof(mRacine), "");
#else
			Copier(mRacine, sizeof(mRacine), "assets/");
#endif
		}

		void NkTextures2D::Brancher(NkTeleverseur televerseur, void *contexte) {
			mTeleverseur = televerseur;
			mContexte = contexte;
			for (uint32 i = 0; i < mEntrees.Size(); ++i) {
				if (!mEntrees[i].televersee) {
					Envoyer(mEntrees[i]);
				}
			}
		}

		void NkTextures2D::PoserRacine(const char *racine) {
			Copier(mRacine, sizeof(mRacine), racine);
		}

		uint32 NkTextures2D::Charger(const char *chemin, int32 largeurSvg, int32 hauteurSvg) {
			if (chemin == nullptr || chemin[0] == '\0') {
				return 0u;
			}
			if (const uint32 deja = Trouver(chemin)) {
				return deja;
			}
			// Un chemin absolu (ou qui commence deja par la racine) n'est pas prefixe.
			char complet[320];
			const bool absolu = chemin[0] == '/' || (chemin[0] != '\0' && chemin[1] == ':');
			std::snprintf(complet, sizeof(complet), "%s%s", absolu ? "" : mRacine, chemin);

			NkImage img;
			if (EstSvg(chemin)) {
				img = NkSVGCodec::DecodeFromFile(complet, largeurSvg, hauteurSvg);
			} else {
				img.Load(complet, 4);
			}
			if (!VersRGBA(img)) {
				logger.Warn("[unkeny] texture introuvable ou illisible : {0}", complet);
				return 0u;
			}
			return Enregistrer(img.Pixels(), img.Width(), img.Height(), img.Stride(), chemin);
		}

		uint32 NkTextures2D::ChargerMemoire(const uint8 *donnees, usize taille, const char *nom) {
			if (donnees == nullptr || taille == 0) {
				return 0u;
			}
			if (nom != nullptr && nom[0] != '\0') {
				if (const uint32 deja = Trouver(nom)) {
					return deja;
				}
			}
			NkImage img;
			if (nom != nullptr && EstSvg(nom)) {
				img = NkSVGCodec::Decode(donnees, taille, 256, 256);
			} else {
				img.LoadFromMemory(donnees, taille, 4);
			}
			if (!VersRGBA(img)) {
				logger.Warn("[unkeny] image en memoire illisible : {0}", nom != nullptr ? nom : "(sans nom)");
				return 0u;
			}
			return Enregistrer(img.Pixels(), img.Width(), img.Height(), img.Stride(), nom);
		}

		uint32 NkTextures2D::Creer(const uint8 *rgba, int32 w, int32 h, const char *nom) {
			if (rgba == nullptr || w <= 0 || h <= 0) {
				return 0u;
			}
			return Enregistrer(rgba, w, h, w * 4, nom);
		}

		bool NkTextures2D::Remplacer(uint32 id, const uint8 *rgba, int32 w, int32 h) {
			NkEntree *e = Entree(id);
			if (e == nullptr || rgba == nullptr || w <= 0 || h <= 0) {
				return false;
			}
			e->w = w;
			e->h = h;
			e->rgba.Resize(static_cast<usize>(w) * static_cast<usize>(h) * 4u);
			std::memcpy(e->rgba.Data(), rgba, e->rgba.Size());
			e->televersee = false;
			return Envoyer(*e);
		}

		uint32 NkTextures2D::Enregistrer(const uint8 *rgba, int32 w, int32 h, int32 stride, const char *nom) {
			NkEntree e;
			e.id = mProchainId++;
			Copier(e.nom, sizeof(e.nom), nom);
			e.w = w;
			e.h = h;
			// Copie ligne par ligne : le decodeur peut rendre des lignes
			// ALIGNEES (stride > 4w). Le backend, lui, les veut jointives.
			const usize ligne = static_cast<usize>(w) * 4u;
			e.rgba.Resize(ligne * static_cast<usize>(h));
			for (int32 y = 0; y < h; ++y) {
				std::memcpy(e.rgba.Data() + ligne * static_cast<usize>(y),
							rgba + static_cast<usize>(stride > 0 ? stride : static_cast<int32>(ligne)) * static_cast<usize>(y), ligne);
			}
			mEntrees.PushBack(static_cast<NkEntree &&>(e));
			Envoyer(mEntrees[mEntrees.Size() - 1u]);
			return mEntrees[mEntrees.Size() - 1u].id;
		}

		bool NkTextures2D::Envoyer(NkEntree &e) {
			if (mTeleverseur == nullptr || e.rgba.Size() == 0) {
				return false; // il partira au branchement
			}
			e.televersee = mTeleverseur(mContexte, e.id, e.rgba.Data(), e.w, e.h);
			if (!e.televersee) {
				logger.Warn("[unkeny] televersement refuse par le rendu : {0}", e.nom[0] != '\0' ? e.nom : "(sans nom)");
			} else if (!garderPixels) {
				e.rgba.Clear();
				e.rgba.ShrinkToFit();
			}
			return e.televersee;
		}

		uint32 NkTextures2D::Reteleverser() {
			uint32 n = 0;
			for (uint32 i = 0; i < mEntrees.Size(); ++i) {
				mEntrees[i].televersee = false;
				n += Envoyer(mEntrees[i]) ? 1u : 0u;
			}
			return n;
		}

		uint32 NkTextures2D::EnAttente() const noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < mEntrees.Size(); ++i) {
				n += mEntrees[i].televersee ? 0u : 1u;
			}
			return n;
		}

		NkTextures2D::NkEntree *NkTextures2D::Entree(uint32 id) noexcept {
			for (uint32 i = 0; i < mEntrees.Size(); ++i) {
				if (mEntrees[i].id == id) {
					return &mEntrees[i];
				}
			}
			return nullptr;
		}

		const NkTextures2D::NkEntree *NkTextures2D::Entree(uint32 id) const noexcept {
			for (uint32 i = 0; i < mEntrees.Size(); ++i) {
				if (mEntrees[i].id == id) {
					return &mEntrees[i];
				}
			}
			return nullptr;
		}

		bool NkTextures2D::Taille(uint32 id, int32 &w, int32 &h) const noexcept {
			const NkEntree *e = Entree(id);
			if (e == nullptr) {
				return false;
			}
			w = e->w;
			h = e->h;
			return true;
		}

		const char *NkTextures2D::Nom(uint32 id) const noexcept {
			const NkEntree *e = Entree(id);
			return e != nullptr ? e->nom : "";
		}

		uint32 NkTextures2D::Trouver(const char *nom) const noexcept {
			if (nom == nullptr || nom[0] == '\0') {
				return 0u;
			}
			for (uint32 i = 0; i < mEntrees.Size(); ++i) {
				if (std::strcmp(mEntrees[i].nom, nom) == 0) {
					return mEntrees[i].id;
				}
			}
			return 0u;
		}

		const uint8 *NkTextures2D::Pixels(uint32 id) const noexcept {
			const NkEntree *e = Entree(id);
			return (e != nullptr && e->rgba.Size() > 0) ? e->rgba.Data() : nullptr;
		}

		NkSprite2D NkTextures2D::Sprite(uint32 id, float32 pixelsParUnite) const noexcept {
			NkSprite2D s;
			s.texId = id;
			int32 w = 0, h = 0;
			if (Taille(id, w, h) && pixelsParUnite > 0.f) {
				s.taille = math::NkVec2f(static_cast<float32>(w) / pixelsParUnite, static_cast<float32>(h) / pixelsParUnite);
			}
			return s;
		}

		void NkTextures2D::RegionGrille(int32 colonnes, int32 lignes, int32 index, math::NkVec2f &uv0,
										math::NkVec2f &uv1) noexcept {
			const int32 c = colonnes > 0 ? colonnes : 1;
			const int32 l = lignes > 0 ? lignes : 1;
			const int32 i = index >= 0 ? index % (c * l) : 0;
			const float32 w = 1.f / static_cast<float32>(c);
			const float32 h = 1.f / static_cast<float32>(l);
			uv0 = math::NkVec2f(static_cast<float32>(i % c) * w, static_cast<float32>(i / c) * h);
			uv1 = math::NkVec2f(uv0.x + w, uv0.y + h);
		}

	} // namespace unkeny
} // namespace nkentseu
