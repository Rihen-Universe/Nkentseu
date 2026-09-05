#pragma once
// =============================================================================
// NkRenderer2DTypes.h — Shared types for the NKRenderer 2D system
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace renderer {

		// ── Color (RGBA, 0-255) ──────────────────────────────────────────────────
		using NkColor2D = math::NkColor;

		// ── 2D integer vector ────────────────────────────────────────────────────
		using NkVec2i = math::NkVec2i;

		// ── 2D float vector ──────────────────────────────────────────────────────
		using NkVec2f = math::NkVec2f;

		// ── Rectangles : noms canoniques domicilies dans NKMath, re-exportes ici ──
		// (cf. NKMath/NkRectangle.h — ce sont des types purement math). NkRect2i et
		// NkRect2f restent identiques a math::NkIntRect / NkFloatRect (meme NkRectT).
		using NkRect2i = math::NkRect2i; // int32  — clip, viewport, UI
		using NkRect2f = math::NkRect2f; // float32 — geometrie 2D
		using NkRect2u = math::NkRect2u; // uint32
		using NkRect2d = math::NkRect2d; // float64

		// ── 2D transform (position, rotation in radians, scale, origin) ──────────
		struct NkTransform2D {
				NkVec2f position = {0.f, 0.f};
				float32 rotation = 0.f; // radians
				NkVec2f scale = {1.f, 1.f};
				NkVec2f origin = {0.f, 0.f}; // pivot point (local coords)

				// Returns a 4x4 (column-major, but we store as float[16]) matrix
				// compatible with standard OpenGL/Vulkan/DX uniform layouts.
				void ToMatrix4(float32 out[16]) const;
		};

		// ── View (2D camera orthographic) ────────────────────────────────────────
		struct NkView2D {
				NkVec2f center = {0.f, 0.f};   // center of view in world coords
				NkVec2f size = {800.f, 600.f}; // visible area
				float32 rotation = 0.f;

				void ToProjectionMatrix(float32 out[16]) const;
		};

		// ── Politique de redimensionnement ───────────────────────────────────────
		// Que devient l'image quand la fenetre change de taille ? Il y a cinq
		// reponses possibles, plus le refus de repondre, et le moteur ne doit pas
		// choisir en silence : le choix change le jeu. Suivre la fenetre donne au
		// joueur au grand ecran un champ de vision plus large, ce qui est un
		// avantage deloyal en competitif ; etirer est equitable et laid ; les
		// bandes noires sont equitables et honnetes.
		//
		// Les trois politiques d'ajustement travaillent par rapport a une TAILLE
		// DE REFERENCE (design size) : la resolution pour laquelle le jeu est
		// dessine. Sans elle, il n'y a rien a ajuster.
		enum class NkResizePolicy : uint8 {
			/// Un pixel reste un pixel : la vue grandit avec la fenetre et l'on
			/// VOIT PLUS DE MONDE. Aucune deformation, aucune bande.
			/// C'est le comportement historique de Nkentseu, et le defaut.
			NK_FOLLOW_WINDOW = 0,

			/// Le meme rectangle de monde remplit toute la fenetre : l'image
			/// GRANDIT, et se DEFORME si le rapport largeur/hauteur a change.
			/// C'est le defaut de SFML.
			NK_STRETCH,

			/// Rapport conserve, tout le monde de reference reste visible, et
			/// des BANDES apparaissent sur deux cotes. Les bandes prennent la
			/// couleur du dernier Clear, qui couvre tout le framebuffer.
			NK_FIT_LETTERBOX,

			/// Rapport conserve, la fenetre est entierement remplie, et l'on
			/// PERD les bords sur l'axe le plus long.
			NK_FIT_CROP,

			/// Comme NK_FIT_LETTERBOX, mais l'agrandissement est arrondi a
			/// l'entier inferieur : chaque pixel de reference occupe un carre
			/// entier de pixels ecran. Indispensable au pixel art, ou un
			/// agrandissement fractionnaire fait baver les bords.
			/// Si la fenetre est plus petite que la reference, l'entier vaudrait
			/// zero : on retombe alors sur l'ajustement exact, faute de mieux.
			NK_INTEGER_SCALE,

			/// Le moteur ne touche a rien : ni la vue, ni le viewport. A vous
			/// d'appeler SetView et SetViewport dans votre reponse au
			/// redimensionnement.
			/// ATTENTION : sans viewport a jour, le rendu reste CLIPPE a
			/// l'ancienne zone. C'est le prix du controle total.
			NK_MANUAL,
		};

		/// Nom lisible d'une politique, pour les journaux et les rapports.
		inline const char *NkResizePolicyName(NkResizePolicy p) noexcept {
			switch (p) {
				case NkResizePolicy::NK_FOLLOW_WINDOW:
					return "FollowWindow";
				case NkResizePolicy::NK_STRETCH:
					return "Stretch";
				case NkResizePolicy::NK_FIT_LETTERBOX:
					return "FitLetterbox";
				case NkResizePolicy::NK_FIT_CROP:
					return "FitCrop";
				case NkResizePolicy::NK_INTEGER_SCALE:
					return "IntegerScale";
				case NkResizePolicy::NK_MANUAL:
					return "Manual";
			}
			return "Inconnu";
		}

		// ── Blending mode ────────────────────────────────────────────────────────
		enum class NkBlendMode : uint8 {
			NK_ALPHA,	 // standard alpha blending (premul src, 1-src_alpha dst)
			NK_ADD,		 // additive (fire, glow effects)
			NK_MULTIPLY, // multiply (shadows)
			NK_NONE,	 // no blending (overwrite)
		};

		// ── Primitive type ───────────────────────────────────────────────────────
		// Type de primitive consommee par NkVertexArray et le backend.
		// Calque SFML : POINTS, LINES, LINE_STRIP, TRIANGLES, TRIANGLE_STRIP,
		// TRIANGLE_FAN. Pas de QUADS — les quads sont decomposes en 2 triangles
		// par le batcher (compatible cross-API : Vulkan/DX/Metal n'ont pas de
		// QUADS natif).
		enum class NkPrimitiveType : uint8 {
			NK_POINTS = 0,
			NK_LINES = 1,
			NK_LINE_STRIP = 2,
			NK_TRIANGLES = 3,
			NK_TRIANGLE_STRIP = 4,
			NK_TRIANGLE_FAN = 5,
		};

		// ── Vertex for 2D rendering ──────────────────────────────────────────────
		struct NkVertex2D {
				float32 x, y;	  // position
				float32 u, v;	  // texture coords
				uint8 r, g, b, a; // color
		};

		// Alias SFML-friendly : NkVertex (sans le « 2D ») pour les nouvelles APIs.
		// NkVertex2D est conserve comme nom canonique du POD pour la compat des
		// sous-systemes existants (NkIRenderer2D::DrawVertices, batchers, etc.).
		using NkVertex = NkVertex2D;

		// ── Render stats (per frame) ─────────────────────────────────────────────
		struct NkRenderStats2D {
				uint32 drawCalls = 0;
				uint32 vertexCount = 0;
				uint32 indexCount = 0;
				uint32 textureSwap = 0;
		};

	} // namespace renderer
} // namespace nkentseu