// =============================================================================
// NkTerrainHeightMap.cpp — NKRenderer / Mesh
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
#include "NkTerrainHeightMap.h"

#include "NKImage/Core/NkImage.h"

namespace nkentseu {
	namespace renderer {

		const char *NkTerrainStatutNom(NkTerrainStatut s) {
			switch (s) {
				case NkTerrainStatut::NK_OK:
					return "NK_OK";
				case NkTerrainStatut::NK_IMAGE_INVALIDE:
					return "NK_IMAGE_INVALIDE";
				case NkTerrainStatut::NK_IMAGE_TROP_PETITE:
					return "NK_IMAGE_TROP_PETITE";
				case NkTerrainStatut::NK_CANAL_ABSENT:
					return "NK_CANAL_ABSENT";
				case NkTerrainStatut::NK_FORMAT_INCONNU:
					return "NK_FORMAT_INCONNU";
			}
			return "?";
		}

		NkTerrainStatut NkTerrainLireNiveaux(const NkImage &img, const NkTerrainParams &p,
											 NkVector<float32> &outNiveaux, uint32 &outN, uint32 &outM) {
			outNiveaux.Clear();
			outN = 0;
			outM = 0;

			if (!img.IsValid())
				return NkTerrainStatut::NK_IMAGE_INVALIDE;

			const int32 w = img.Width(), h = img.Height();
			if (w <= 0 || h <= 0)
				return NkTerrainStatut::NK_IMAGE_INVALIDE;

			const int32 canaux = img.Channels();
			if (p.canal < 0 || p.canal >= canaux)
				return NkTerrainStatut::NK_CANAL_ABSENT;

			const NkImagePixelFormat fmt = img.Format();
			const bool flottant = (fmt == NkImagePixelFormat::NK_RGBA128F || fmt == NkImagePixelFormat::NK_RGB96F);
			const bool entier =
				(fmt == NkImagePixelFormat::NK_GRAY8 || fmt == NkImagePixelFormat::NK_GRAY_A16 ||
				 fmt == NkImagePixelFormat::NK_RGB24 || fmt == NkImagePixelFormat::NK_RGBA32);
			if (!flottant && !entier)
				return NkTerrainStatut::NK_FORMAT_INCONNU;

			outN = (uint32)w;
			outM = (uint32)h;
			outNiveaux.Resize(outN * outM);

			// ⚠️ ON PASSE PAR `RowPtr`, PAS PAR `GetPixel`. `GetPixel` rend un
			// `NkColor` 8 bits et REFUSE les images HDR (il rend (0,0,0,0) au lieu
			// d'échouer) : tout un chemin d'entrée serait devenu un terrain plat
			// SANS AUCUN MESSAGE. C'est exactement la forme de panne que ce lot
			// existe pour rendre impossible.
			const int32 bpp = img.BytesPP();
			for (uint32 j = 0; j < outM; ++j) {
				const uint8 *ligne = img.RowPtr((int32)j);
				for (uint32 i = 0; i < outN; ++i) {
					float32 niveau;
					if (flottant) {
						const float32 *pf = (const float32 *)(ligne + (usize)i * (usize)bpp);
						niveau = pf[p.canal];
					} else {
						niveau = (float32)(ligne[(usize)i * (usize)bpp + (usize)p.canal]);
					}
					outNiveaux[j * outN + i] = niveau;
				}
			}
			return NkTerrainStatut::NK_OK;
		}

		NkTerrainStatut NkTerrainDepuisNiveaux(const float32 *niveaux, uint32 N, uint32 M,
											   const NkTerrainParams &p, NkEditMesh &out) {
			if (niveaux == nullptr || N == 0u || M == 0u)
				return NkTerrainStatut::NK_IMAGE_INVALIDE;
			// Le refus nommé. Voir la note du header : (N-1)(M-1) = 0, et
			// fabriquer un quad depuis un pixel reviendrait à inventer trois
			// hauteurs que l'image ne dit pas.
			if (N < 2u || M < 2u)
				return NkTerrainStatut::NK_IMAGE_TROP_PETITE;

			const uint32 nv = NkTerrainNbSommets(N, M);
			const uint32 nq = NkTerrainNbQuads(N, M);

			// ── L'ORIGINE DU QUADRILLAGE ────────────────────────────────────
			// Centrée : le pixel (0,0) est en bas à gauche du repère XZ, et le
			// centre de l'image tombe sur l'origine du monde.
			const float32 x0 = p.centre ? -0.5f * (float32)(N - 1u) * p.pasX : 0.f;
			const float32 z0 = p.centre ? -0.5f * (float32)(M - 1u) * p.pasZ : 0.f;
			// Division faite UNE fois : `i / (N-1)` dans la boucle donnerait un
			// arrondi différent selon l'optimiseur, et l'UV n'est pas mesuré ici.
			const float32 invN = 1.f / (float32)(N - 1u);
			const float32 invM = 1.f / (float32)(M - 1u);

			NkVector<NkVertex3D> v;
			v.Resize(nv);
			for (uint32 j = 0; j < M; ++j) {
				for (uint32 i = 0; i < N; ++i) {
					const uint32 k = j * N + i;
					NkVertex3D s{};
					// ⚠️ LA LIGNE 0 DE L'IMAGE VA EN z LE PLUS NÉGATIF. Aucun
					// retournement. C'est LA convention de ce fichier, elle est
					// arbitraire, et elle est SOUS TÉMOIN : le banc construit une
					// image dont les 60 pixels valent 60 nombres différents, donc
					// toute permutation d'indices (lignes retournées, transposée,
					// colonnes à l'envers) rougit. On ne la défend pas par un
					// raisonnement — le dépôt a déjà payé « sept flips, une cause ».
					s.pos.x = x0 + (float32)i * p.pasX;
					s.pos.y = NkTerrainHauteurDepuisNiveau(p, niveaux[k]);
					s.pos.z = z0 + (float32)j * p.pasZ;
					// Normale provisoire : `RecomputeNormals()` l'écrase plus bas.
					// Elle n'est pas laissée à zéro pour qu'un maillage lu avant
					// ce recalcul ne porte pas un vecteur dégénéré.
					s.normal = {0.f, 1.f, 0.f};
					s.tangent = {1.f, 0.f, 0.f};
					s.uv = {(float32)i * invN, (float32)j * invM};
					s.uv2 = s.uv;
					s.color = 0xFFFFFFFFu;
					v[k] = s;
				}
			}

			// ── LES QUADS, EN CSR ───────────────────────────────────────────
			// L'ORDRE DES COINS N'EST PAS DEVINÉ : il est calé sur
			// `NkMeshSystem::CreatePlaneMesh` (NkMeshSystem.cpp:838-855), la
			// primitive de référence du moteur, dont la nappe « visible d'EN
			// HAUT » enroule (a, a+1, b) avec a = (i,j), a+1 = (i+1,j),
			// b = (i,j+1). Le quad ci-dessous, éventaillé par `Triangulate`,
			// produit le MÊME sens d'enroulement.
			// Et il donne bien +Y sous la convention de `NkEditMesh` :
			// `NkEmFaceCross(p0,p1,p2) = (p2-p0) x (p1-p0)` — qui est l'OPPOSÉE
			// de la convention usuelle. Les deux se recoupent ; c'est pour ça
			// qu'on les a confrontées au lieu d'en croire une.
			NkVector<uint32> faceStart;
			NkVector<uint32> faceVerts;
			faceStart.Resize(nq + 1u);
			faceVerts.Resize(nq * 4u);
			uint32 f = 0;
			for (uint32 j = 0; j + 1u < M; ++j) {
				for (uint32 i = 0; i + 1u < N; ++i) {
					const uint32 a = j * N + i;			 // (i,   j  )
					const uint32 b = a + 1u;			 // (i+1, j  )
					const uint32 c = a + N + 1u;		 // (i+1, j+1)
					const uint32 d = a + N;				 // (i,   j+1)
					faceStart[f] = f * 4u;
					faceVerts[f * 4u + 0u] = a;
					faceVerts[f * 4u + 1u] = b;
					faceVerts[f * 4u + 2u] = c;
					faceVerts[f * 4u + 3u] = d;
					++f;
				}
			}
			faceStart[nq] = nq * 4u;

			// `BuildFromPolygons` recopie les sommets DANS L'ORDRE (NkEditMesh.cpp
			// :1322-1335, `verts.Resize(vc)` puis boucle indexée) : `out.verts[k]`
			// reste donc le pixel (k % N, k / N). C'est ce qui rend le critère
			// vérifiable sommet par sommet, et c'est pourquoi il est cité ici.
			out.BuildFromPolygons(v.Data(), nv, faceStart.Data(), nq, faceVerts.Data(), nullptr);

			// ⚠️ L'ORDRE COMPTE : `SetShadeSmooth` pose `Face::smooth`, et
			// `RecomputeNormals` LE LIT. L'inverse donnerait des normales de
			// facettes sur un maillage déclaré lisse, sans aucun message.
			if (p.lisse)
				out.SetShadeSmooth(true);
			out.RecomputeNormals();

			// ⚠️ CE QUE `RecomputeNormals` FAIT VRAIMENT, ET SA LIMITE.
			// La normale d'une face est prise sur ses TROIS PREMIERS coins
			// (NkEditMesh.cpp:631-632), pas par un Newell sur les quatre. Sur un
			// quad NON PLAN — c'est-à-dire le cas général d'un terrain — les deux
			// diffèrent. Sur un plan incliné exact ils coïncident, et c'est
			// précisément pour ça que le banc mesure les normales SUR LE PLAN
			// EXACT : là, l'attendu est calculable à la main. Le cas non plan est
			// nommé, pas mesuré ; le corriger voudrait dire toucher
			// `RecomputeNormals`, qui sert 16 opérateurs d'édition — hors lot.
			//
			// ⚠️ ET UN PIÈGE DE `SetShadeSmooth(true)` : l'accumulation lissée
			// passe par `BuildVertexMerge`, qui SOUDE les sommets distants de
			// moins de 1e-4. Avec un `pasX`/`pasZ` plus petit que ça, deux pixels
			// voisins deviendraient un seul sommet pour le calcul de normale.
			// Ce n'est pas une panne du terrain, c'est une échelle inutilisable ;
			// mais elle serait SILENCIEUSE, donc elle est écrite ici.
			return NkTerrainStatut::NK_OK;
		}

		NkTerrainStatut NkTerrainDepuisHeightMap(const NkImage &img, const NkTerrainParams &p, NkEditMesh &out) {
			NkVector<float32> niveaux;
			uint32 N = 0, M = 0;
			const NkTerrainStatut st = NkTerrainLireNiveaux(img, p, niveaux, N, M);
			if (st != NkTerrainStatut::NK_OK)
				return st;
			// `out` n'est touché qu'ici : un statut != NK_OK laisse l'appelant
			// avec le maillage qu'il avait, jamais avec un demi-terrain.
			return NkTerrainDepuisNiveaux(niveaux.Data(), N, M, p, out);
		}

	} // namespace renderer
} // namespace nkentseu
