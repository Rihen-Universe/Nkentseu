// =============================================================================
// NkShape.cpp — Implementation du Draw partage : fan-triangulation + outline.
//
// Pattern : on compose states.transform avec notre GetTransform() (transformable
// herite), on triangule la liste de points en fan (convex only), puis on
// submit en NK_TRIANGLES via target.Draw(raw vertices). Outline en NK_LINES.
// =============================================================================

#include "NKCanvas/Renderer/Shapes/NkShape.h"
#include "NKCanvas/Renderer/Targets/NkRenderTarget.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			// Buffer temporaire de NkVertex : stack pour <= 256, heap sinon.
			struct VtxBuf {
					NkVertex stack[256];
					NkVertex *heap{nullptr};

					NkVertex *Acquire(uint32 n) {
						if (n <= 256)
							return stack;
						heap = static_cast<NkVertex *>(nkentseu::memory::NkAlloc(sizeof(NkVertex) * n));
						return heap;
					}

					~VtxBuf() {
						if (heap)
							nkentseu::memory::NkFree(heap);
					}
			};

			// Construit un NkVertex (POD) depuis position + couleur + UV.
			inline NkVertex MakeVertex(NkVec2f pos, NkColor2D c, float32 u = 0.f, float32 v = 0.f) noexcept {
				NkVertex out;
				out.x = pos.x;
				out.y = pos.y;
				out.u = u;
				out.v = v;
				out.r = c.r;
				out.g = c.g;
				out.b = c.b;
				out.a = c.a;
				return out;
			}
		} // namespace

		void NkShape::Draw(NkRenderTarget &target, const NkRenderStates &parentStates) const {
			const uint32 n = GetPointCount();
			if (n < 2)
				return;

			// Composition : etat du parent * transform local.
			NkRenderStates s = parentStates;
			s.transform *= GetTransform();
			if (mTexture)
				s.texture = mTexture;

			// ── Fill (n >= 3 : aire pleine, NK_TRIANGLES list directe) ────────
			// On triangule en fan COTE NkShape (1 triangle = (p0, p_i, p_{i+1}))
			// et on emet directement en NK_TRIANGLES. Pourquoi pas NK_TRIANGLE_FAN
			// (n vertices, plus economique) ? Car NkRenderWindow::Draw raw fait
			// une expansion TRIANGLE_FAN -> TRIANGLES dans un buffer scratch
			// local au switch ; si le backend batch lazyement ce buffer (et ne
			// recopie pas immediatement), le scratch est detruit avant flush ->
			// donnees garbage / artefacts (visible : « rectangles creux »).
			// En emettant TRIANGLES direct, le buffer `v` de NkShape reste vivant
			// pendant tout l'appel target.Draw, et NkRenderWindow se contente
			// d'un identity-indices submit deterministe.
			if (n >= 3) {
				const NkRect2f bounds = GetLocalBounds();
				const uint32 triCount = n - 2;
				const uint32 vertCount = triCount * 3;

				VtxBuf vbuf;
				NkVertex *v = vbuf.Acquire(vertCount);

				// Pre-calcul du sommet central (p0) et son UV (reutilise pour tous les triangles).
				const NkVec2f p0 = GetPoint(0);
				const NkVec2f uv0 = GetPointUV(0, bounds);
				const NkVertex v0 = MakeVertex(p0, mFillColor, uv0.x, uv0.y);

				uint32 w = 0;
				for (uint32 i = 1; i + 1 < n; ++i) {
					const NkVec2f p1 = GetPoint(i);
					const NkVec2f p2 = GetPoint(i + 1);
					const NkVec2f uv1 = GetPointUV(i, bounds);
					const NkVec2f uv2 = GetPointUV(i + 1, bounds);
					v[w++] = v0;
					v[w++] = MakeVertex(p1, mFillColor, uv1.x, uv1.y);
					v[w++] = MakeVertex(p2, mFillColor, uv2.x, uv2.y);
				}
				target.Draw(v, vertCount, NkPrimitiveType::NK_TRIANGLES, s);
			}

			// ── Contour : un ruban de triangles, donc une epaisseur reelle ─────
			//
			// Le contour partait en NK_LINE_STRIP, et l'epaisseur etait PERDUE :
			// NkRenderWindow::Draw traduit chaque segment par DrawLine(..., 1.f),
			// avec 1 en dur. SetOutlineThickness(4.f) et SetOutlineThickness(0.1f)
			// rendaient donc exactement le meme trait d'un pixel, et seul le test
			// « > 0 » comptait. Constate le 2026-09-05.
			//
			// Le ruban corrige au passage un second defaut sans rapport apparent :
			// NkRenderTexture::Draw ne sait traiter que NK_TRIANGLES, donc le
			// contour de toute forme disparaissait silencieusement des qu'on
			// rendait dans une texture. En triangles, il survit.
			//
			// Geometrie : un ruban ferme entre un contour interieur et un contour
			// exterieur. Chaque sommet est decale le long de sa BISSECTRICE, la
			// moyenne des normales des deux aretes qui s'y rejoignent, et non le
			// long de la normale d'une seule arete. C'est ce qui fait une jointure
			// en onglet : sans elle, deux quads voisins laissent une encoche a
			// chaque coin, tres visible des que l'epaisseur depasse quelques
			// pixels.
			//
			// Le facteur 1 / (m . n) rallonge le decalage dans l'angle, pour que
			// les deux bords restent a une demi-epaisseur de leur arete. Il est
			// borne : sur un angle tres aigu il tendrait vers l'infini et
			// produirait une pointe. On accepte alors un onglet tronque.
			//
			// La triangulation en eventail du remplissage suppose deja une forme
			// convexe ; l'onglet fait la meme hypothese, et elle est donc sans
			// cout supplementaire.
			if (mOutlineThickness > 0.f && n >= 2) {
				const float32 demi = mOutlineThickness * 0.5f;
				const uint32 vertCount = n * 6; // n aretes, 2 triangles chacune
				VtxBuf vbuf;
				NkVertex *v = vbuf.Acquire(vertCount);

				// Normale unitaire de l'arete i vers i+1.
				auto normaleArete = [&](uint32 i) -> NkVec2f {
					const NkVec2f a = GetPoint(i);
					const NkVec2f b = GetPoint((i + 1) % n);
					float32 dx = b.x - a.x, dy = b.y - a.y;
					const float32 len = math::NkSqrt(dx * dx + dy * dy);
					if (len < 1e-6f)
						return NkVec2f{0.f, 0.f};
					return NkVec2f{-dy / len, dx / len};
				};

				// Decalage du sommet i, le long de sa bissectrice.
				auto decalage = [&](uint32 i) -> NkVec2f {
					const NkVec2f np = normaleArete((i + n - 1) % n); // arete entrante
					const NkVec2f nn = normaleArete(i);				  // arete sortante
					float32 mx = np.x + nn.x, my = np.y + nn.y;
					float32 len = math::NkSqrt(mx * mx + my * my);
					if (len < 1e-6f) { // aretes opposees : demi-tour, pas d'onglet
						return NkVec2f{nn.x * demi, nn.y * demi};
					}
					mx /= len;
					my /= len;
					float32 cos = mx * nn.x + my * nn.y;
					if (cos < 0.25f)
						cos = 0.25f; // onglet tronque sur les angles tres aigus
					return NkVec2f{mx * demi / cos, my * demi / cos};
				};

				uint32 k = 0;
				for (uint32 i = 0; i < n; ++i) {
					const uint32 j = (i + 1) % n;
					const NkVec2f a = GetPoint(i), b = GetPoint(j);
					const NkVec2f da = decalage(i), db = decalage(j);

					const NkVec2f a0{a.x + da.x, a.y + da.y}, a1{a.x - da.x, a.y - da.y};
					const NkVec2f b0{b.x + db.x, b.y + db.y}, b1{b.x - db.x, b.y - db.y};

					v[k++] = MakeVertex(a0, mOutlineColor);
					v[k++] = MakeVertex(b0, mOutlineColor);
					v[k++] = MakeVertex(b1, mOutlineColor);
					v[k++] = MakeVertex(a0, mOutlineColor);
					v[k++] = MakeVertex(b1, mOutlineColor);
					v[k++] = MakeVertex(a1, mOutlineColor);
				}

				if (k > 0) {
					NkRenderStates outlineStates = s;
					outlineStates.texture = nullptr; // l'outline ne texture pas
					target.Draw(v, k, NkPrimitiveType::NK_TRIANGLES, outlineStates);
				}
			}
		}

	} // namespace renderer
} // namespace nkentseu
