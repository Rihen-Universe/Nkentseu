// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKVFX/NkWaterMeshBuilder.cpp — le producteur d'eau. Voir l'en-tête pour ce
// qu'il ne sait pas, et pourquoi c'est voulu.
// =============================================================================
#include "NKVFX/NkWaterMeshBuilder.h"

namespace nkentseu {
	namespace vfx {

		uint32 NkWaterBuildVertices(const math::NkMat4f &proj, const math::NkMat4f &view,
									const math::NkVec3f &eye, const NkWaterMeshParams &p,
									renderer::NkVertex3D *out, uint32 capacity,
									uint32 *missing) noexcept {
			if (missing != nullptr)
				*missing = 0u;

			const uint32 need = NkWaterVertexCount(p.grid);
			if (out == nullptr || capacity < need || p.grid.cols == 0u || p.grid.rows == 0u)
				return 0u;

			// La grille est construite ICI et pas ailleurs : elle dépend de la caméra
			// de CETTE image, et la garder d'une image à l'autre ferait suivre l'eau
			// avec un retard qu'on ne verrait qu'en mouvement.
			const math::NkProjectedGrid g = math::NkProjectedGridBuild(proj, view, eye, p.grid);
			if (!g.visible)
				return 0u; // l'eau n'est pas dans le champ, et la grille le DIT

			uint32 manquants = 0u;
			for (uint32 j = 0; j <= p.grid.rows; ++j) {
				for (uint32 i = 0; i <= p.grid.cols; ++i) {
					const uint32 slot = j * (p.grid.cols + 1u) + i;

					math::NkVec3f base;
					if (!math::NkProjectedGridVertex(g, p.grid, i, j, base)) {
						++manquants;
						continue; // on ne fabrique PAS de sommet de remplacement
					}

					// La houle COMPOSE : la grille rend le plan de repos, `NkWaterEval`
					// déplace le point et rend sa normale et sa tangente analytiques.
					// Aucune des deux n'est refabriquée ici.
					const math::NkWaterPoint w =
						math::NkWaterEval(p.waves, base.x, base.z, p.time);

					renderer::NkVertex3D &v = out[slot];
					// ⚠️ LA COMPOSITION EN HAUTEUR EST ICI, ET ELLE N'EST PAS OPTIONNELLE.
					// `NkWaterEval` rend une hauteur de vague AUTOUR DE ZÉRO : il ne
					// connaît pas `baseY`, et n'a aucune raison de le connaître. La
					// grille, elle, pose ses sommets SUR le plan de repos. C'est donc au
					// producteur de composer les deux — l'oublier dessinerait l'eau à
					// l'altitude zéro quel que soit le plan, un défaut invisible tant
					// qu'on ne teste qu'avec baseY = 0.
					v.pos = math::NkVec3f{w.position.x, p.grid.baseY + w.position.y,
										  w.position.z};
					v.normal = w.normal;
					v.tangent = w.tangent;
					// Les UV viennent des PARAMÈTRES de la grille, pas d'une projection :
					// uniformes par construction, et gratuits.
					v.uv = math::NkVec2f{(float32)i / (float32)p.grid.cols,
										 (float32)j / (float32)p.grid.rows};
					v.uv2 = v.uv;
					v.color = p.color;
				}
			}

			if (missing != nullptr)
				*missing = manquants;
			if (manquants != 0u)
				return 0u; // un maillage à trous ne se livre pas à moitié

			return need;
		}

	} // namespace vfx
} // namespace nkentseu
