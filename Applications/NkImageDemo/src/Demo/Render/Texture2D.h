#pragma once
// =============================================================================
// Texture2D.h
// -----------------------------------------------------------------------------
// Texture GPU 2D chargee depuis un fichier image via NKImage.
// Stockage interne : GL_RGBA8, mipmaps generes automatiquement.
// Usage : LoadFromFile("Resources/Pong/Textures/logo.png") puis Bind().
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"

// NkImage est desormais rendue PAR VALEUR par DecodeFromFile : une declaration
// anticipee ne suffit plus (le type doit etre complet pour un retour par
// valeur), l'en-tete est donc inclus. C'etait le seul cout de la migration ici.
#include "NKImage/Core/NkImage.h"

namespace nkentseu {
	namespace demo {

		class Texture2D {
			public:
				Texture2D() = default;
				~Texture2D();

				// ── Lifecycle ────────────────────────────────────────────────────
				/// Charge un fichier image (PNG/JPG/etc.) via NkImage puis upload
				/// en texture GL_RGBA8. Genere les mipmaps. Bloque le thread.
				/// Pour un chargement asynchrone, utiliser DecodeFromFile() depuis
				/// un thread worker, puis UploadFromImage() depuis le thread GL.
				bool LoadFromFile(const char *path);

				/// Decode UNIQUEMENT (lit le fichier + decode RGBA8 via NkImage).
				/// SAFE depuis un thread worker (n'appelle aucune fonction GL).
				/// Rend l'image PAR VALEUR ; l'echec se teste par `IsValid()`.
				/// L'appelant n'a RIEN a liberer : la destruction de la valeur
				/// suffit, et un passage vers un autre thread se fait par move.
				static NkImage DecodeFromFile(const char *path);

				/// Upload une image deja decodee en texture GL_RGBA8 + mipmaps.
				/// DOIT etre appele depuis le thread GL (main thread).
				/// Ne libere RIEN : l'image reste la propriete de l'appelant.
				bool UploadFromImage(const NkImage &img);

				/// Libere la texture GL et l'image source.
				void Shutdown();

				// ── Accesseurs ───────────────────────────────────────────────────
				uint32 Id() const noexcept {
					return mTextureId;
				}

				int Width() const noexcept {
					return mWidth;
				}

				int Height() const noexcept {
					return mHeight;
				}

				bool IsValid() const noexcept {
					return mTextureId != 0;
				}

				float AspectRatio() const noexcept {
					return (mHeight > 0) ? (float)mWidth / (float)mHeight : 1.0f;
				}

			private:
				uint32 mTextureId = 0;
				int mWidth = 0;
				int mHeight = 0;
		};

	} // namespace demo
} // namespace nkentseu
