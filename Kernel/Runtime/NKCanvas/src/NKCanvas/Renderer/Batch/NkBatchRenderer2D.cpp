// =============================================================================
// NkBatchRenderer2D.cpp — CPU geometry generation + batching logic
// =============================================================================
#include "NkBatchRenderer2D.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKLogger/NkLog.h"

#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace nkentseu {
	namespace renderer {

		// =============================================================================
		NkBatchRenderer2D::NkBatchRenderer2D() {
			mVertices.Reserve((NkVector<NkVertex2D>::SizeType)kMaxVertices);
			mIndices.Reserve((NkVector<uint32>::SizeType)kMaxIndices);
			mGroups.Reserve(64);
		}

		// =============================================================================
		bool NkBatchRenderer2D::Begin() {
			if (mInFrame) {
				logger.Warnf("[NkBatch] Begin() called twice");
				return false;
			}
			mInFrame = true;
			mVertices.Clear();
			mIndices.Clear();
			mGroups.Clear();
			mCurrentTexture = nullptr;
			// Chaque frame demarre sans clip (le scissor sera (re)desactive au
			// premier Flush via ApplyScissor(false, ...)).
			mClipStack.Clear();
			mClipRect = NkRect2i{};
			mHasClip = false;
			BeginBackend();
			return true;
		}

		// =============================================================================
		void NkBatchRenderer2D::End() {
			if (!mInFrame)
				return;
			Flush();
			EndBackend();
			mInFrame = false;
		}

		// =============================================================================
		void NkBatchRenderer2D::Flush() {
			if (mGroups.Empty() || mVertices.Empty())
				return;

			// Close current group
			if (!mGroups.Empty()) {
				auto &g = mGroups.Back();
				g.indexCount = mIndices.Size() - g.indexStart;
			}

			// Remove empty groups
			uint32 validCount = 0;
			for (uint32 i = 0; i < mGroups.Size(); ++i) {
				if (mGroups[i].indexCount > 0) {
					if (validCount != i)
						mGroups[validCount] = mGroups[i];
					++validCount;
				}
			}

			if (validCount > 0) {
				// Applique le clip courant juste avant la soumission : tout le batch
				// partage ce scissor (un changement de clip a Flush() au prealable).
				ApplyScissor(mHasClip, mClipRect);
				// GARDE-FOU : ne JAMAIS soumettre plus que la capacité des buffers GPU
				// (l'upload écrirait hors buffer -> crash). Troncature (indices en
				// multiple de 3) : dégradation visuelle plutôt que corruption mémoire.
				uint32 vSub = mVertices.Size(), iSub = mIndices.Size();
				if (vSub > kMaxVertices)
					vSub = kMaxVertices;
				if (iSub > kMaxIndices)
					iSub = kMaxIndices - (kMaxIndices % 3);
				SubmitBatches(mGroups.Data(), validCount, mVertices.Data(), vSub, mIndices.Data(), iSub);
				mStats.drawCalls += validCount;
				mStats.vertexCount += mVertices.Size();
				mStats.indexCount += mIndices.Size();
			}

			mVertices.Clear();
			mIndices.Clear();
			mGroups.Clear();
			mCurrentTexture = nullptr;
		}

		// =============================================================================
		// Intersection de deux rects entiers (clip nesting : enfant ⊆ parent).
		// =============================================================================
		static NkRect2i NkIntersectClip(const NkRect2i &a, const NkRect2i &b) {
			const int32 x0 = (a.x > b.x) ? a.x : b.x;
			const int32 y0 = (a.y > b.y) ? a.y : b.y;
			const int32 ax1 = a.x + a.width, ay1 = a.y + a.height;
			const int32 bx1 = b.x + b.width, by1 = b.y + b.height;
			const int32 x1 = (ax1 < bx1) ? ax1 : bx1;
			const int32 y1 = (ay1 < by1) ? ay1 : by1;
			int32 w = x1 - x0;
			if (w < 0)
				w = 0;
			int32 h = y1 - y0;
			if (h < 0)
				h = 0;
			return NkRect2i(x0, y0, w, h);
		}

		// =============================================================================
		void NkBatchRenderer2D::SetClip(const NkRect2i &rect) {
			Flush(); // committe la geometrie en cours avec le clip actuel
			const NkRect2i clip = mHasClip ? NkIntersectClip(mClipRect, rect) : rect;
			mClipStack.PushBack(clip);
			mClipRect = clip;
			mHasClip = true;
		}

		// =============================================================================
		void NkBatchRenderer2D::PopClip() {
			if (mClipStack.Empty())
				return;
			Flush();
			mClipStack.PopBack();
			if (mClipStack.Empty()) {
				mHasClip = false;
				mClipRect = NkRect2i{};
			} else {
				mHasClip = true;
				mClipRect = mClipStack.Back();
			}
		}

		// =============================================================================
		void NkBatchRenderer2D::ResetClip() {
			if (!mHasClip && mClipStack.Empty())
				return;
			Flush();
			mClipStack.Clear();
			mHasClip = false;
			mClipRect = NkRect2i{};
		}

		// =============================================================================
		// SetView — pose une vue CUSTOM.
		//
		// Le drapeau mViewIsCustom remplace l'ancienne comparaison en egalite
		// flottante exacte entre mCurrentView et mDefaultView. Cette comparaison
		// avait deux torts silencieux : SetView(GetDefaultView()) ne comptait pas
		// comme une vue custom, et une camera valant par hasard la vue par defaut,
		// ce qui arrive tout le temps au demarrage, se faisait confisquer au
		// premier redimensionnement. Signale par Rodolf le 2026-09-05.
		// =============================================================================
		void NkBatchRenderer2D::SetView(const NkView2D &view) {
			SetViewInternal(view);
			mViewIsCustom = true;
		}

		// =============================================================================
		// SetViewInternal — pose la vue SANS la marquer custom (machinerie interne).
		// =============================================================================
		void NkBatchRenderer2D::SetViewInternal(const NkView2D &view) {
			Flush(); // commit current batch before changing projection
			mCurrentView = view;
			float proj[16];
			view.ToProjectionMatrix(proj);
			UploadProjection(proj);
		}

		// =============================================================================
		void NkBatchRenderer2D::ResetView() {
			SetViewInternal(mDefaultView);
			mViewIsCustom = false;
		}

		// =============================================================================
		// SetResizePolicy — choisit ce que devient l'image quand la cible change de
		// taille, et l'applique TOUT DE SUITE a la taille courante.
		//
		// L'application immediate compte : sans elle, regler la politique ne se
		// verrait qu'au premier redimensionnement, et un jeu lance en plein ecran
		// ne la verrait jamais.
		// =============================================================================
		void NkBatchRenderer2D::SetResizePolicy(NkResizePolicy policy, NkVec2f designSize) {
			mResizePolicy = policy;

			// Taille de reference : celle demandee, sinon la vue courante. Regler la
			// politique une fois la scene cadree est le geste naturel, et il donne la
			// bonne reference sans que l'on ait a la repeter.
			if (designSize.x > 0.f && designSize.y > 0.f) {
				mDesignSize = designSize;
			} else if (mDesignSize.x <= 0.f || mDesignSize.y <= 0.f) {
				mDesignSize = mCurrentView.size;
			}

			// Applique a la taille courante. Le viewport plein-cadre est la seule
			// source fiable de la taille du framebuffer a ce niveau.
			const int32 w = mViewport.width > 0 ? mViewport.width : static_cast<int32>(mCurrentView.size.x);
			const int32 h = mViewport.height > 0 ? mViewport.height : static_cast<int32>(mCurrentView.size.y);
			if (w > 0 && h > 0) {
				// Le viewport doit repartir plein-cadre, sinon une politique posee
				// apres une autre heriterait des bandes de la precedente.
				mViewport = {0, 0, w, h};
				OnResize(static_cast<uint32>(w), static_cast<uint32>(h));
			}
		}

		// =============================================================================
		// CentreDeReference — le centre du rectangle de reference, (0,0)-(design).
		//
		// Tant que l'utilisateur n'a pas pose sa propre camera, une politique
		// d'ajustement doit cadrer le monde de reference, pas le centre de la
		// fenetre : sinon la scene dessinee en 0..L x 0..H se retrouve decalee d'un
		// demi-ecran des que la fenetre n'a pas la taille de la reference.
		NkVec2f NkBatchRenderer2D::CentreDeReference() const noexcept {
			return NkVec2f{mDesignSize.x * 0.5f, mDesignSize.y * 0.5f};
		}

		// =============================================================================
		// OnResize — applique la politique de redimensionnement.
		//
		// Deux leviers, et deux seulement :
		//   - la VUE dit quel rectangle de monde est regarde ;
		//   - le VIEWPORT dit quelle region de pixels le recoit.
		// Les six politiques ne sont que six facons de les regler. Le viewport part
		// toujours plein-cadre, sinon le rendu resterait clippe a l'ancienne zone.
		//
		// Une vue CUSTOM (posee par SetView) n'est jamais recalee : c'est la camera
		// du jeu, elle appartient au jeu. Seule sa fenetre de projection, c'est-a-dire
		// le viewport et la taille visible, suit la politique.
		// =============================================================================
		void NkBatchRenderer2D::OnResize(uint32 width, uint32 height) noexcept {
			if (width == 0 || height == 0)
				return;

			const float32 fw = static_cast<float32>(width);
			const float32 fh = static_cast<float32>(height);
			const int32 iw = static_cast<int32>(width);
			const int32 ih = static_cast<int32>(height);

			// La vue par defaut dit toujours la verite sur l'ecran, quelle que soit la
			// politique : GetDefaultView() doit rester utilisable pour dessiner une
			// interface en coordonnees ecran.
			mDefaultView.center = {fw * 0.5f, fh * 0.5f};
			mDefaultView.size = {fw, fh};
			mDefaultView.rotation = 0.f;

			// NK_MANUAL : on ne touche a rien d'autre. Le viewport reste ce qu'il
			// etait, donc le rendu reste clippe a l'ancienne zone tant que
			// l'utilisateur n'appelle pas SetViewport. C'est le contrat.
			if (mResizePolicy == NkResizePolicy::NK_MANUAL) {
				return;
			}

			// Reference des politiques d'ajustement. Sans reference utilisable, elles
			// n'ont aucun sens : on retombe sur le suivi de fenetre.
			NkResizePolicy politique = mResizePolicy;
			const bool referenceValide = mDesignSize.x > 0.f && mDesignSize.y > 0.f;
			if (!referenceValide && politique != NkResizePolicy::NK_FOLLOW_WINDOW) {
				politique = NkResizePolicy::NK_FOLLOW_WINDOW;
			}

			// Viewport plein-cadre par defaut ; les ajustements le retreciront.
			mViewport = {0, 0, iw, ih};

			switch (politique) {
				case NkResizePolicy::NK_FOLLOW_WINDOW: {
					// Un pixel reste un pixel : la vue prend la taille de la fenetre.
					if (!mViewIsCustom) {
						SetViewInternal(mDefaultView);
					} else {
						// Vue custom : on garde son centre et sa rotation, mais sa taille
						// suit la fenetre, sinon « suivre la fenetre » ne voudrait rien
						// dire pour une camera qui se deplace.
						NkView2D v = mCurrentView;
						v.size = {fw, fh};
						SetViewInternal(v);
					}
					break;
				}

				case NkResizePolicy::NK_STRETCH: {
					// Le monde de reference remplit la fenetre. La vue ne change pas de
					// taille : c'est le viewport plein-cadre qui l'etire, avec la
					// deformation que cela implique si le rapport a change.
					NkView2D v = mCurrentView;
					v.size = mDesignSize;
					if (!mViewIsCustom)
						v.center = CentreDeReference();
					SetViewInternal(v);
					break;
				}

				case NkResizePolicy::NK_FIT_LETTERBOX:
				case NkResizePolicy::NK_INTEGER_SCALE: {
					// Rapport conserve, tout le monde de reference visible, bandes sur
					// deux cotes. La vue garde la taille de reference ; c'est le viewport
					// qui retrecit et se centre.
					float32 echelle = fw / mDesignSize.x;
					const float32 echelleY = fh / mDesignSize.y;
					if (echelleY < echelle)
						echelle = echelleY;

					if (politique == NkResizePolicy::NK_INTEGER_SCALE && echelle >= 1.f) {
						// Arrondi a l'entier inferieur : chaque pixel de reference occupe
						// un carre entier de pixels ecran, sans quoi le pixel art bave.
						// En dessous de 1, l'entier vaudrait zero : on garde l'ajustement
						// exact, faute de mieux.
						echelle = static_cast<float32>(static_cast<int32>(echelle));
						if (echelle < 1.f)
							echelle = 1.f;
					}

					const int32 vw = static_cast<int32>(mDesignSize.x * echelle + 0.5f);
					const int32 vh = static_cast<int32>(mDesignSize.y * echelle + 0.5f);
					mViewport = {(iw - vw) / 2, (ih - vh) / 2, vw, vh};

					NkView2D v = mCurrentView;
					v.size = mDesignSize;
					if (!mViewIsCustom)
						v.center = CentreDeReference();
					SetViewInternal(v);
					break;
				}

				case NkResizePolicy::NK_FIT_CROP: {
					// Rapport conserve, fenetre entierement remplie, bords perdus sur
					// l'axe le plus long. Ici c'est la VUE qui retrecit, pas le viewport :
					// on regarde une part plus etroite du monde de reference, et elle
					// occupe tout l'ecran sans deformation.
					const float32 rapportFenetre = fw / fh;
					const float32 rapportReference = mDesignSize.x / mDesignSize.y;

					NkView2D v = mCurrentView;
					if (rapportFenetre > rapportReference) {
						// Fenetre plus large : on garde la largeur, on rogne en hauteur.
						v.size = {mDesignSize.x, mDesignSize.x / rapportFenetre};
					} else {
						// Fenetre plus haute : on garde la hauteur, on rogne en largeur.
						v.size = {mDesignSize.y * rapportFenetre, mDesignSize.y};
					}
					if (!mViewIsCustom)
						v.center = CentreDeReference();
					SetViewInternal(v);
					break;
				}

				case NkResizePolicy::NK_MANUAL:
					break; // deja traite plus haut
			}
		}

		// =============================================================================
		void NkBatchRenderer2D::SetBlendMode(NkBlendMode mode) {
			if (mBlendMode == mode)
				return;
			Flush();
			mBlendMode = mode;
		}

		// =============================================================================
		void NkBatchRenderer2D::EnsureGroup(const NkTexture *tex, NkBlendMode blend) {
			bool needNew = mGroups.Empty();
			if (!needNew) {
				const auto &back = mGroups.Back();
				needNew = (back.texture != tex || back.blendMode != blend);
			}
			if (needNew) {
				// Close previous group
				if (!mGroups.Empty()) {
					mGroups.Back().indexCount = mIndices.Size() - mGroups.Back().indexStart;
				}
				NkBatchGroup g;
				g.texture = tex;
				g.blendMode = blend;
				g.indexStart = mIndices.Size();
				g.indexCount = 0;
				mGroups.PushBack(g);
				if (tex != mCurrentTexture) {
					++mStats.textureSwap;
					mCurrentTexture = tex;
				}
			}
		}

		// =============================================================================
		void NkBatchRenderer2D::PushQuad(NkVec2f tl, NkVec2f tr, NkVec2f br, NkVec2f bl, NkVec2f uvTL, NkVec2f uvBR,
										 const NkColor2D &color, const NkTexture *texture) {
			// Auto-flush if full
			if (mVertices.Size() + 4 > kMaxVertices || mIndices.Size() + 6 > kMaxIndices) {
				Flush();
			}
			EnsureGroup(texture, mBlendMode);

			const uint32 base = mVertices.Size();
			NkVertex2D v;
			v.r = color.r;
			v.g = color.g;
			v.b = color.b;
			v.a = color.a;

			v.x = tl.x;
			v.y = tl.y;
			v.u = uvTL.x;
			v.v = uvTL.y;
			mVertices.PushBack(v);
			v.x = tr.x;
			v.y = tr.y;
			v.u = uvBR.x;
			v.v = uvTL.y;
			mVertices.PushBack(v);
			v.x = br.x;
			v.y = br.y;
			v.u = uvBR.x;
			v.v = uvBR.y;
			mVertices.PushBack(v);
			v.x = bl.x;
			v.y = bl.y;
			v.u = uvTL.x;
			v.v = uvBR.y;
			mVertices.PushBack(v);

			mIndices.PushBack(base + 0);
			mIndices.PushBack(base + 1);
			mIndices.PushBack(base + 2);
			mIndices.PushBack(base + 0);
			mIndices.PushBack(base + 2);
			mIndices.PushBack(base + 3);
		}

		// =============================================================================
		void NkBatchRenderer2D::Draw(const NkSprite &sprite) {
			const NkTexture *tex = sprite.GetTexture();
			if (!tex)
				return;

			NkRect2i srcRect = sprite.GetTextureRect();
			NkRect2f uvRect = tex->GetTexCoords(srcRect);
			NkColor2D col = sprite.GetColor();

			const float32 w = (float32)srcRect.width;
			const float32 h = (float32)srcRect.height;

			// Local corners (centered at origin before transform)
			NkVec2f corners[4] = {{0, 0}, {w, 0}, {w, h}, {0, h}};

			// ── La transformation du sprite ────────────────────────────────────
			// Une seule ligne, parce qu'elle vient de NkTransformable, qui la garde
			// en cache et ne la recalcule que si un accesseur l'a salie.
			//
			// Il y avait ici une TROISIEME implementation du meme calcul « origine,
			// echelle, rotation, translation », a cote de celle de NkTransformable
			// et de celle de NkText, chacune avec ses propres cos et sin recalcules
			// a chaque trame. Supprimee le 2026-09-05.
			const NkTransform &t = sprite.GetTransform();
			for (auto &c : corners)
				c = t.TransformPoint(c);

			// ── Miroirs ────────────────────────────────────────────────────────
			// Retourner un sprite, c'est echanger ses coordonnees de texture : la
			// geometrie ne bouge pas, c'est l'image qui se lit a l'envers.
			//
			// SetFlipX et SetFlipY existaient, avec leurs getters et leurs membres,
			// et NE FAISAIENT RIEN. Il ne restait ici qu'une ligne commentee qui,
			// decommentee, n'aurait rien fait non plus. Les miroirs de NkRef, seule
			// application a s'en servir, etaient donc inoperants depuis toujours.
			// Constate le 2026-09-05.
			NkVec2f uvTL = {uvRect.left, uvRect.top};
			NkVec2f uvBR = {uvRect.left + uvRect.width, uvRect.top + uvRect.height};
			if (sprite.GetFlipX()) {
				const float32 t = uvTL.x;
				uvTL.x = uvBR.x;
				uvBR.x = t;
			}
			if (sprite.GetFlipY()) {
				const float32 t = uvTL.y;
				uvTL.y = uvBR.y;
				uvBR.y = t;
			}

			if (mVertices.Size() + 4 > kMaxVertices || mIndices.Size() + 6 > kMaxIndices)
				Flush();
			EnsureGroup(tex, mBlendMode);

			const uint32 base = mVertices.Size();
			NkVertex2D v;
			v.r = col.r;
			v.g = col.g;
			v.b = col.b;
			v.a = col.a;

			v.x = corners[0].x;
			v.y = corners[0].y;
			v.u = uvTL.x;
			v.v = uvTL.y;
			mVertices.PushBack(v);
			v.x = corners[1].x;
			v.y = corners[1].y;
			v.u = uvBR.x;
			v.v = uvTL.y;
			mVertices.PushBack(v);
			v.x = corners[2].x;
			v.y = corners[2].y;
			v.u = uvBR.x;
			v.v = uvBR.y;
			mVertices.PushBack(v);
			v.x = corners[3].x;
			v.y = corners[3].y;
			v.u = uvTL.x;
			v.v = uvBR.y;
			mVertices.PushBack(v);

			mIndices.PushBack(base + 0);
			mIndices.PushBack(base + 1);
			mIndices.PushBack(base + 2);
			mIndices.PushBack(base + 0);
			mIndices.PushBack(base + 2);
			mIndices.PushBack(base + 3);
		}

		// =============================================================================
		void NkBatchRenderer2D::Draw(const NkText &text) {
			if (!text.GetString() || !*text.GetString())
				return;
			// Geometry is built lazily inside NkText::GetVertices()
			// We just submit it here with the atlas texture.
			const auto &verts = text.GetVertices();
			if (verts.Empty())
				return;

			// Resolve atlas texture for this character size
			// (NkText caches font reference; we pull the atlas from it)
			// This is a simplified path — a full implementation would call font.GetAtlasTexture()
			// In practice NkText::Draw() could directly emit into the batch via DrawVertices().
			text.Draw(*this);
		}

		// =============================================================================
		void NkBatchRenderer2D::DrawPoint(NkVec2f pos, const NkColor2D &col, float32 size) {
			const float32 half = size * 0.5f;
			DrawFilledRect({pos.x - half, pos.y - half, size, size}, col);
		}

		// =============================================================================
		void NkBatchRenderer2D::DrawLine(NkVec2f a, NkVec2f b, const NkColor2D &col, float32 thick) {
			// Build a thick line as a quad
			float32 dx = b.x - a.x;
			float32 dy = b.y - a.y;
			const float32 len = sqrtf(dx * dx + dy * dy);
			if (len < 1e-5f)
				return;
			dx /= len;
			dy /= len;
			// Perpendicular
			const float32 px = -dy * thick * 0.5f;
			const float32 py = dx * thick * 0.5f;

			NkTexture *white = NkTexture::GetWhiteTexture(*this);
			PushQuad({a.x + px, a.y + py}, {b.x + px, b.y + py}, {b.x - px, b.y - py}, {a.x - px, a.y - py}, {0, 0},
					 {1, 1}, col, white);
		}

		// =============================================================================
		void NkBatchRenderer2D::DrawFilledRect(NkRect2f r, const NkColor2D &col) {
			NkTexture *white = NkTexture::GetWhiteTexture(*this);
			PushQuad({r.left, r.top}, {r.left + r.width, r.top}, {r.left + r.width, r.top + r.height},
					 {r.left, r.top + r.height}, {0, 0}, {1, 1}, col, white);
		}

		// =============================================================================
		void NkBatchRenderer2D::DrawRect(NkRect2f r, const NkColor2D &col, float32 outline, const NkColor2D &oc) {
			DrawFilledRect(r, col);
			if (outline > 0.f) {
				const float32 t = outline;
				// Top
				DrawFilledRect({r.left - t, r.top - t, r.width + 2 * t, t}, oc);
				// Bottom
				DrawFilledRect({r.left - t, r.top + r.height, r.width + 2 * t, t}, oc);
				// Left
				DrawFilledRect({r.left - t, r.top, t, r.height}, oc);
				// Right
				DrawFilledRect({r.left + r.width, r.top, t, r.height}, oc);
			}
		}

		// =============================================================================
		void NkBatchRenderer2D::DrawFilledCircle(NkVec2f center, float32 radius, const NkColor2D &col, uint32 segs) {
			if (segs < 3)
				segs = 3;
			NkTexture *white = NkTexture::GetWhiteTexture(*this);

			// Fan triangulation: center + rim
			const uint32 triCount = segs;
			if (mVertices.Size() + triCount * 3 > kMaxVertices)
				Flush();
			EnsureGroup(white, mBlendMode);

			for (uint32 i = 0; i < segs; ++i) {
				const float32 a0 = (float32)(i) / segs * (float32)(M_PI * 2.0);
				const float32 a1 = (float32)(i + 1) / segs * (float32)(M_PI * 2.0);

				const uint32 base = mVertices.Size();
				NkVertex2D v;
				v.r = col.r;
				v.g = col.g;
				v.b = col.b;
				v.a = col.a;

				v.x = center.x;
				v.y = center.y;
				v.u = 0.5f;
				v.v = 0.5f;
				mVertices.PushBack(v);
				v.x = center.x + cosf(a0) * radius;
				v.y = center.y + sinf(a0) * radius;
				v.u = 1.f;
				v.v = 0.5f;
				mVertices.PushBack(v);
				v.x = center.x + cosf(a1) * radius;
				v.y = center.y + sinf(a1) * radius;
				v.u = 1.f;
				v.v = 1.f;
				mVertices.PushBack(v);

				mIndices.PushBack(base);
				mIndices.PushBack(base + 1);
				mIndices.PushBack(base + 2);
			}
		}

		// =============================================================================
		void NkBatchRenderer2D::DrawCircle(NkVec2f center, float32 radius, const NkColor2D &col, uint32 segs,
										   float32 outline, const NkColor2D &oc) {
			DrawFilledCircle(center, radius, col, segs);
			if (outline > 0.f) {
				DrawFilledCircle(center, radius + outline, oc, segs);
				DrawFilledCircle(center, radius, col, segs); // overdraw inner
			}
		}

		// =============================================================================
		void NkBatchRenderer2D::DrawFilledTriangle(NkVec2f a, NkVec2f b, NkVec2f c, const NkColor2D &col) {
			NkTexture *white = NkTexture::GetWhiteTexture(*this);
			if (mVertices.Size() + 3 > kMaxVertices || mIndices.Size() + 3 > kMaxIndices)
				Flush();
			EnsureGroup(white, mBlendMode);

			const uint32 base = mVertices.Size();
			NkVertex2D v;
			v.r = col.r;
			v.g = col.g;
			v.b = col.b;
			v.a = col.a;
			v.x = a.x;
			v.y = a.y;
			v.u = 0;
			v.v = 0;
			mVertices.PushBack(v);
			v.x = b.x;
			v.y = b.y;
			v.u = 1;
			v.v = 0;
			mVertices.PushBack(v);
			v.x = c.x;
			v.y = c.y;
			v.u = 0;
			v.v = 1;
			mVertices.PushBack(v);
			mIndices.PushBack(base);
			mIndices.PushBack(base + 1);
			mIndices.PushBack(base + 2);
		}

		// =============================================================================
		void NkBatchRenderer2D::DrawTriangle(NkVec2f a, NkVec2f b, NkVec2f c, const NkColor2D &col, float32 outline,
											 const NkColor2D &oc) {
			DrawFilledTriangle(a, b, c, col);
			if (outline > 0.f) {
				DrawLine(a, b, oc, outline);
				DrawLine(b, c, oc, outline);
				DrawLine(c, a, oc, outline);
			}
		}

		// =============================================================================
		void NkBatchRenderer2D::DrawVertices(const NkVertex2D *verts, uint32 vCount, const uint32 *idx, uint32 iCount,
											 const NkTexture *tex) {
			if (!verts || vCount == 0 || !idx || iCount == 0)
				return;
			if (mVertices.Size() + vCount > kMaxVertices || mIndices.Size() + iCount > kMaxIndices)
				Flush();
			EnsureGroup(tex, mBlendMode);

			const uint32 base = mVertices.Size();
			for (uint32 i = 0; i < vCount; ++i)
				mVertices.PushBack(verts[i]);
			for (uint32 i = 0; i < iCount; ++i)
				mIndices.PushBack(idx[i] + base);
		}

		// =============================================================================
		NkVec2f NkBatchRenderer2D::MapPixelToCoords(NkVec2i pixel) const {
			// Simple orthographic un-projection
			const float32 nx = ((float32)pixel.x / mViewport.width * 2.f - 1.f);
			const float32 ny = (1.f - (float32)pixel.y / mViewport.height * 2.f);
			return {nx * mCurrentView.size.x * 0.5f + mCurrentView.center.x,
					ny * mCurrentView.size.y * 0.5f + mCurrentView.center.y};
		}

		NkVec2i NkBatchRenderer2D::MapCoordsToPixel(NkVec2f point) const {
			const float32 nx = (point.x - mCurrentView.center.x) / (mCurrentView.size.x * 0.5f);
			const float32 ny = (point.y - mCurrentView.center.y) / (mCurrentView.size.y * 0.5f);
			return {(int32)((nx + 1.f) * 0.5f * mViewport.width), (int32)((1.f - ny) * 0.5f * mViewport.height)};
		}

	} // namespace renderer
} // namespace nkentseu