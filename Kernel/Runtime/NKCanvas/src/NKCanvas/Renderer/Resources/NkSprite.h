#pragma once
// =============================================================================
// NkSprite.h + NkText.h — High-level 2D drawables (similar to SFML)
//
// UNIFICATION (2026-09-05) : NkSprite et NkText heritent de NkTransformable et
// de NkDrawable, exactement comme NkShape. Trois defauts disparaissent d'un coup.
//
//   1. Ils heritaient AUSSI de NkIDrawable2D, et NkRenderTarget exposant une
//      surcharge pour chacune des deux interfaces, `target.Draw(sprite)` etait
//      AMBIGU et ne compilait pas. Cinq fichiers sur cinq ecrivaient un
//      static_cast, et RihenDefi castait vers une interface pour son texte et
//      vers l'autre pour son sprite, a six lignes d'ecart.
//   2. Leur rotation etait en DEGRES quand celle de NkShape etait en radians.
//      Elle prend maintenant un math::NkAngle, le type qui porte son unite, et
//      qui est deja celui des rotations 3D du moteur.
//   3. Leur Draw(target, states) JETAIT l'etat recu : le parametre etait
//      commente. Un sprite ne pouvait donc pas etre l'enfant d'un objet
//      transforme, alors qu'une forme le pouvait. Il le compose desormais.
//
// L'ancienne voie reste ouverte : NkIRenderer2D::Draw(const NkSprite&) prend le
// sprite par son type concret, donc le code qui l'appelait continue de marcher.
// =============================================================================
#include "NkTexture.h"
#include "NkFont.h"
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"
#include "NKCanvas/Renderer/Core/NkDrawable.h"
#include "NKCanvas/Renderer/Core/NkRenderStates.h"
#include "NKCanvas/Renderer/Core/NkTransformable.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace renderer {

		class NkRenderTarget;

		// =========================================================================
		// NkSprite — textured, transformable 2D quad
		// Similar to sf::Sprite
		// =========================================================================
		class NkSprite : public NkTransformable, public NkDrawable {
			public:
				NkSprite() = default;
				explicit NkSprite(const NkTexture &texture);
				NkSprite(const NkTexture &texture, const NkRect2i &textureRect);

				// ── Texture ───────────────────────────────────────────────────────────
				void SetTexture(const NkTexture &texture, bool resetRect = false);
				void SetTextureRect(const NkRect2i &rect);

				const NkTexture *GetTexture() const {
					return mTexture;
				}

				NkRect2i GetTextureRect() const {
					return mTextureRect;
				}

				// ── Transform ─────────────────────────────────────────────────────────
				// SetPosition, SetRotation, SetScale, SetOrigin, Move, Rotate, Scale,
				// leurs getters et GetTransform() viennent tous de NkTransformable,
				// exactement comme pour NkShape. Rien n'est redefini ici : c'etait la
				// duplication qui creait l'ecart d'unite entre les deux familles.

				// ── Color tint ────────────────────────────────────────────────────────
				void SetColor(const NkColor2D &color) {
					mColor = color;
				}

				NkColor2D GetColor() const {
					return mColor;
				}

				// ── Flip ──────────────────────────────────────────────────────────────
				void SetFlipX(bool flip) {
					mFlipX = flip;
				}

				void SetFlipY(bool flip) {
					mFlipY = flip;
				}

				bool GetFlipX() const;
				bool GetFlipY() const;

				// ── Bounds ────────────────────────────────────────────────────────────
				NkRect2f GetLocalBounds() const;
				NkRect2f GetGlobalBounds() const;

				// ── Voie directe, conservee ───────────────────────────────────────────
				// Prend le sprite par son type concret, donc sans ambiguite. C'est ce
				// que les cinq backends implementent, et ce que le code anterieur
				// appelle par NkIRenderer2D::Draw(sprite).
				void Draw(NkIRenderer2D &renderer) const;

				// ── NkDrawable ────────────────────────────────────────────────────────
				/// Compose sa transformation avec celle recue, pose sa texture, puis
				/// soumet quatre sommets a la cible.
				void Draw(NkRenderTarget &target, const NkRenderStates &states) const override;

			private:
				const NkTexture *mTexture = nullptr;
				NkRect2i mTextureRect;
				NkColor2D mColor = NkColor2D::White;
				bool mFlipX = false;
				bool mFlipY = false;
		};

		// =========================================================================
		// NkText — UTF-8 text drawable with FreeType-rasterized glyphs
		// Similar to sf::Text
		// =========================================================================
		class NkText : public NkTransformable, public NkDrawable {
			public:
				NkText() = default;
				NkText(const NkFont &font, const char *string, uint32 characterSize = 24);

				// ── Content ───────────────────────────────────────────────────────────
				void SetString(const char *utf8String);
				void SetFont(const NkFont &font);
				void SetCharacterSize(uint32 size);
				void SetStyle(NkTextStyle style);
				void SetLetterSpacing(float32 factor); // multiplier on advance (1.0 = normal)
				void SetLineSpacing(float32 factor);   // multiplier on line height

				const char *GetString() const {
					return mString.Data();
				}

				uint32 GetCharacterSize() const {
					return mCharacterSize;
				}

				NkTextStyle GetStyle() const {
					return mStyle;
				}

				// ── Transform ───────────────────────────────────────────────────────────
				// Tout vient de NkTransformable, comme pour NkSprite et NkShape.
				// Le texte avait ici sa propre copie du bloc, avec les memes degres et
				// le meme pi ecrit en dur, mais SANS Rotate, SANS SetScale(x, y),
				// SANS SetOrigin(x, y) et SANS GetOrigin : l'asymetrie n'etait pas que
				// sur l'unite d'angle. Unifie le 2026-09-05.

				// ── Color ─────────────────────────────────────────────────────────────
				void SetFillColor(const NkColor2D &c) {
					mFillColor = c;
				}

				void SetOutlineColor(const NkColor2D &c) {
					mOutlineColor = c;
				}

				void SetOutlineThickness(float32 t) {
					mOutlineThickness = t;
				}

				NkColor2D GetFillColor() const {
					return mFillColor;
				}

				NkColor2D GetOutlineColor() const {
					return mOutlineColor;
				}

				float32 GetOutlineThickness() const {
					return mOutlineThickness;
				}

				// ── Bounds ────────────────────────────────────────────────────────────
				NkRect2f GetLocalBounds() const;
				NkRect2f GetGlobalBounds() const;

				// Find the index of the character at a given position
				uint32 FindCharacterPos(NkVec2f point) const;

				// ── Voie directe, conservee ───────────────────────────────────────────
				void Draw(NkIRenderer2D &renderer) const;

				// ── NkDrawable ────────────────────────────────────────────────────────
				/// Compose sa transformation avec celle recue, puis soumet un quad par
				/// glyphe. (TODO d'origine : la page d'atlas correspondant a la taille
				/// de caractere doit encore etre enveloppee dans une NkTexture.)
				void Draw(NkRenderTarget &target, const NkRenderStates &states) const override;

				// Internal: build vertex array for the current string
				struct GlyphVertex {
						NkVertex2D v[4];
				};

				const NkVector<GlyphVertex> &GetVertices() const;

			private:
				void EnsureGeometryUpdate() const;

				const NkFont *mFont = nullptr;
				NkVector<char> mString;
				uint32 mCharacterSize = 24;
				NkTextStyle mStyle = NkTextStyle::NK_REGULAR;
				float32 mLetterSpacing = 1.f;
				float32 mLineSpacing = 1.f;
				NkColor2D mFillColor = NkColor2D::White;
				NkColor2D mOutlineColor = NkColor2D::Black;
				float32 mOutlineThickness = 0.f;

				mutable bool mGeometryDirty = true;
				mutable NkVector<GlyphVertex> mVertices;
				mutable NkRect2f mBounds;
		};

	} // namespace renderer
} // namespace nkentseu