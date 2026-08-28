// =============================================================================
// NkEditMesh.cpp — NKRenderer — maillage éditable demi-arête (n-gon)
// =============================================================================
#include "NkEditMesh.h"
#include "NKContainers/Associative/NkHashMap.h"

#include <stddef.h> // offsetof : parametres adressables par nom
#include <cmath> // cosf / sinf / atan2f — profils d'arc du bevel, rotations du spin

namespace nkentseu {
	namespace renderer {

		// ── TABLE PLATE uint64 -> uint32, POUR LES TROIS POINTS CHAUDS ────────
		// POURQUOI UNE TABLE DE PLUS ALORS QUE `NkHashMap` EXISTE, ET MARCHE
		// `NkHashMap` est CORRECT ici depuis la correction du melangeur (Q73). Ce
		// qu'on lui reproche n'est pas sa justesse mais son COUT PAR ENTREE : c'est
		// une table CHAINEE, un noeud alloue par insertion. Le banc `--perf` le
		// mesure hors moteur : 131 072 insertions, MEME avec `Reserve`, 25,6 ms —
		// soit ~195 ns par entree. Une extrusion de 3 faces sur 65 536 faces passe
		// par 262 144 de ces insertions (LinkTwins) plus 66 049 (BuildVertexMerge) :
		// la table EST le cout de l'operation.
		//
		// Ici : adressage ouvert, sondage lineaire, un seul bloc contigu, AUCUNE
		// allocation par entree. 16 octets par case, donc 4 cases par ligne de
		// cache — un sondage rate reste dans la ligne deja chargee.
		//
		// ⚠ CE QU'ELLE NE CHANGE PAS, ET C'EST LA CONDITION POUR L'ADOPTER.
		// Les trois appelants ont tous la meme regle : LA PREMIERE INSERTION GAGNE
		// (`canon[i] = i` au premier passage, `map.Find(opp)` avant d'inserer,
		// `seen.Find(key)` avant de creer l'arete). Cette regle ne depend QUE de
		// l'ordre de la boucle appelante, jamais de l'ordre des seaux — donc
		// remplacer la table ne peut pas changer un seul resultat. C'est
		// exactement ce que la reference du harnais (274 lignes, dont les cinq
		// empreintes `aretes/`) est la pour attester : elle est comparee octet pour
		// octet, et le hachage de Catmull-Clark a deja montre en Q73 qu'un
		// changement d'ordre d'iteration s'y voit.
		//
		// Elle n'expose ni iteration ni suppression : les trois appelants n'en ont
		// pas besoin, et une iteration reintroduirait precisement la dependance a
		// l'ordre des seaux qui a coute une re-etalonnage de reference en Q73.
		class NkEmFlatMap {
			public:
				explicit NkEmFlatMap(uint32 hint) {
					uint32 cap = 16u;
					// Charge maximale 1/2 : au-dela, le sondage lineaire s'allonge
					// vite. `hint` est une BORNE SUPERIEURE chez les trois appelants,
					// donc la table ne grandira normalement jamais.
					while (cap < (hint + 1u) * 2u && cap < 0x40000000u)
						cap <<= 1;
					Alloc(cap);
				}

				const uint32 *Find(uint64 k) const {
					uint32 i = Slot(k);
					while (mSlots[i].used) {
						if (mSlots[i].key == k)
							return &mSlots[i].val;
						i = (i + 1u) & mMask;
					}
					return nullptr;
				}

				// N'ECRASE PAS une cle deja presente : c'est la regle « la premiere
				// insertion gagne » des trois appelants, posee ici une seule fois
				// plutot que re-decidee trois fois par un `if (!found)` a cote.
				void Insert(uint64 k, uint32 v) {
					if (mSize * 2u >= mCap)
						Grow();
					uint32 i = Slot(k);
					while (mSlots[i].used) {
						if (mSlots[i].key == k)
							return;
						i = (i + 1u) & mMask;
					}
					mSlots[i].key = k;
					mSlots[i].val = v;
					mSlots[i].used = 1u;
					++mSize;
				}

				uint32 Size() const {
					return mSize;
				}

			private:
				struct Case {
						uint64 key = 0;
						uint32 val = 0;
						uint32 used = 0;
				};

				// splitmix64 : le MEME finisseur que celui pose dans NkHashMap en Q73,
				// et pour la meme raison — la cle d'arete `(lo << 32) | hi` a des bits
				// bas qui valent `lo ^ hi`, minuscules entre sommets voisins. Un modulo
				// par puissance de deux ne regarderait que ceux-la : 131 072 aretes dans
				// 18 seaux. Ce n'est pas une precaution, c'est le defaut deja paye.
				static uint64 Melange(uint64 x) {
					x += 0x9E3779B97F4A7C15ull;
					x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
					x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
					return x ^ (x >> 31);
				}

				uint32 Slot(uint64 k) const {
					return (uint32)(Melange(k) & (uint64)mMask);
				}

				void Alloc(uint32 cap) {
					mCap = cap;
					mMask = cap - 1u;
					mSize = 0;
					mStore.Resize(cap);
					for (uint32 i = 0; i < cap; ++i) {
						mStore[i].key = 0;
						mStore[i].val = 0;
						mStore[i].used = 0;
					}
					mSlots = mStore.Data();
				}

				void Grow() {
					NkVector<Case> ancien;
					ancien.Resize(mCap);
					for (uint32 i = 0; i < mCap; ++i)
						ancien[i] = mStore[i];
					const uint32 vieuxCap = mCap;
					Alloc(mCap << 1);
					for (uint32 i = 0; i < vieuxCap; ++i)
						if (ancien[i].used)
							Insert(ancien[i].key, ancien[i].val);
				}

				NkVector<Case> mStore;
				Case *mSlots = nullptr;
				uint32 mCap = 0, mMask = 0, mSize = 0;
		};

		// ── Tangente ORTHOGONALE à la normale (anti-NaN) ──────────────────────────
		// ⚠ BUG « carrés blancs » : la tangente était figée à (1,0,0) pour TOUS les
		// sommets. Sur les faces ±X d'un cube (normale (±1,0,0)) la tangente est alors
		// COLINÉAIRE à la normale : le vertex shader PBR fait un Gram-Schmidt
		// T - dot(T,N)*N  == VECTEUR NUL  -> normalize(0) = NaN -> TBN NaN -> N NaN.
		// En mode d'affichage NORMAL (viewMode 2) la couleur vaut N*0.5+0.5, donc NaN,
		// et le NaN se propage dans la chaîne de bloom (down/up) : il ressort en un
		// gros RECTANGLE BLANC aligné écran qui masque l'objet — identique sur TOUS
		// les backends (c'est de l'arithmétique flottante, pas du RHI).
		// On génère donc une tangente réellement perpendiculaire à la normale.
		static NkVec3f NkEmOrthoTangent(const NkVec3f &n) {
			const float32 l = n.Len();
			if (l < 1e-6f)
				return {1.f, 0.f, 0.f}; // normale dégénérée -> tangente arbitraire valide
			const NkVec3f nn = n * (1.f / l);
			// Axe de référence NON colinéaire à nn (seuil large : évite un cross ~nul).
			const NkVec3f ref = (nn.y < 0.9f && nn.y > -0.9f) ? NkVec3f{0.f, 1.f, 0.f} : NkVec3f{1.f, 0.f, 0.f};
			NkVec3f t = ref.Cross(nn);
			const float32 tl = t.Len();
			if (tl < 1e-6f)
				return {1.f, 0.f, 0.f};
			return t * (1.f / tl);
		}

		void NkEditMesh::BuildFromIndexed(const NkVertex3D *v, uint32 vc, const uint32 *idx, uint32 ic,
										  bool quadify, const uint16 *triMaterial, uint32 *outMaterialChanged) {
			Clear();
			verts.Resize(vc);
			for (uint32 i = 0; i < vc; i++) {
				verts[i].pos = v[i].pos;
				verts[i].normal = v[i].normal;
				verts[i].uv = v[i].uv;
				// Attributs TRANSPORTES tels quels : ils ne servent pas a l'edition
				// topologique, mais sans eux l'aller-retour n'est pas une identite
				// (le repere tangent serait reinvente a la sortie) — cf. struct Vert.
				verts[i].tangent = v[i].tangent;
				verts[i].uv2 = v[i].uv2;
				verts[i].color = v[i].color;
				verts[i].hedge = NK_EM_INVALID;
				verts[i].sel = 0;
			}
			const uint32 triCount = ic / 3;
			faces.Reserve(triCount);
			hedges.Reserve(ic);
			for (uint32 t = 0; t < triCount; t++) {
				const uint32 a = idx[t * 3], b = idx[t * 3 + 1], c = idx[t * 3 + 2];
				const NkEmId f = (NkEmId)faces.Size();
				const NkEmId h0 = (NkEmId)hedges.Size(), h1 = h0 + 1, h2 = h0 + 2;
				Hedge e0, e1, e2;
				e0.origin = a;
				e0.next = h1;
				e0.face = f;
				e1.origin = b;
				e1.next = h2;
				e1.face = f;
				e2.origin = c;
				e2.next = h0;
				e2.face = f;
				hedges.PushBack(e0);
				hedges.PushBack(e1);
				hedges.PushBack(e2);
				Face fc;
				fc.hedge = h0;
				fc.alive = 1;
				// OMBRAGE : on DEDUIT flat/smooth des normales SOURCE au lieu de retomber sur
				// le defaut FLAT. Sans cela, entrer en mode edition puis en ressortir SANS
				// RIEN MODIFIER applatissait le modele : la sphere lissee revenait facettee,
				// parce que Face::smooth valait 0 pour toutes les faces reconstruites.
				//
				// Critere : une face est FLAT si ses coins portent la MEME normale dans la
				// source — c'est la definition meme du plat (une normale par face, donc des
				// coins dedoubles). Des qu'ils different, la source portait des normales
				// moyennees, donc SMOOTH.
				//
				// Seuil serre a 0.99999 (~0,26 degre) : plus laxiste, une surface lissee tres
				// dense — dont les coins voisins ne different que de quelques dixiemes de
				// degre — passerait pour plate. LIMITE ASSUMEE : au-dela de la densite ou
				// l'ecart tombe sous 0,26 degre la deduction bascule sur FLAT, mais a cette
				// densite plat et lisse sont visuellement indiscernables.
				{
					const NkVec3f &na = v[a].normal, &nb = v[b].normal, &nc = v[c].normal;
					const float32 kFlatDot = 0.99999f;
					fc.smooth =
						(na.Dot(nb) >= kFlatDot && nb.Dot(nc) >= kFlatDot && nc.Dot(na) >= kFlatDot) ? 0 : 1;
				}
				// MATERIAU PAR FACE : TRANSPORTE, jamais deduit. Contrairement a
				// `smooth` juste au-dessus, aucune donnee geometrique ne le porte —
				// s'il n'est pas fourni ici, il est perdu et toutes les faces
				// retombent sur le slot 0 sans qu'aucune erreur ne se declenche.
				if (triMaterial)
					fc.material = triMaterial[t];
				faces.PushBack(fc);
				if (verts[a].hedge == NK_EM_INVALID)
					verts[a].hedge = h0;
				if (verts[b].hedge == NK_EM_INVALID)
					verts[b].hedge = h1;
				if (verts[c].hedge == NK_EM_INVALID)
					verts[c].hedge = h2;
			}
			LinkTwins();
			RecomputeNormals();
			if (outMaterialChanged)
				*outMaterialChanged = 0;
			if (quadify) {
				// Quadify FUSIONNE des faces : c'est le seul endroit de cette
				// fonction ou un materiau peut se perdre, et le compte remonte.
				const uint32 perdus = Quadify();
				if (outMaterialChanged)
					*outMaterialChanged = perdus;
			}
			// ── NORMALES DE SOMMET : on REND celles de la SOURCE ────────────────
			// RecomputeNormals() (ci-dessus, et de nouveau depuis Quadify) recalcule
			// chaque normale de sommet comme la moyenne, ponderee par l'aire, des faces
			// incidentes. C'est indispensable pour les FACES (Face::normal sert aux
			// operations topologiques) et pour la geometrie CREEE par l'edition — mais
			// c'est FAUX pour une primitive dont les normales sont ANALYTIQUES : sur une
			// sphere UV, la moyenne des facettes n'est pas la normale exacte de la sphere.
			// Mesure : 1040 sommets sur 1089 changeaient au simple fait d'entrer en mode
			// edition et d'en ressortir, d'ou l'impression que le materiau avait change
			// alors que la geometrie n'avait pas bouge d'un micron.
			// Les operations d'edition rappellent RecomputeNormals() apres coup : la
			// geometrie nouvelle obtient bien des normales recalculees ; seule l'ENTREE
			// en edition reste neutre.
			for (uint32 i = 0; i < vc && i < (uint32)verts.Size(); i++)
				verts[i].normal = v[i].normal;
			// Liste d'aretes de premier plan, construite DES l'entree : l'editeur peut
			// ainsi compter/afficher les aretes sans dependre du premier AddWireEdge
			// (qui, lui, garde un rebuild paresseux en filet). Une construction partant
			// de zero n'a par definition aucune arete filaire a preserver.
			RebuildEdges();
		}

		uint32 NkEditMesh::FaceSize(NkEmId f) const {
			if (f >= (NkEmId)faces.Size() || !faces[f].alive)
				return 0;
			const NkEmId start = faces[f].hedge;
			if (start == NK_EM_INVALID)
				return 0;
			NkEmId h = start;
			uint32 n = 0, guard = 0;
			do {
				++n;
				h = hedges[h].next;
				if (++guard > 100000u)
					break;
			} while (h != start && h != NK_EM_INVALID);
			return n;
		}

		float32 NkEditMesh::FaceArea(NkEmId f) const {
			if (f >= (NkEmId)faces.Size() || !faces[f].alive)
				return 0.f;
			const NkEmId start = faces[f].hedge;
			if (start == NK_EM_INVALID)
				return 0.f;
			// NEWELL : somme des produits vectoriels le long du cycle. Vaut pour un
			// n-gon quelconque, la ou 0.5*|AB x AC| ne vaut que pour un triangle —
			// et une face fusionnee est justement un n-gon.
			NkVec3f acc{0.f, 0.f, 0.f};
			NkEmId h = start;
			uint32 n = 0, guard = 0;
			do {
				const NkEmId nx = hedges[h].next;
				if (nx == NK_EM_INVALID)
					break;
				const uint32 a = hedges[h].origin, b = hedges[nx].origin;
				if (a >= (uint32)verts.Size() || b >= (uint32)verts.Size())
					break;
				acc = acc + verts[a].pos.Cross(verts[b].pos);
				++n;
				h = nx;
			} while (h != start && ++guard < 100000u);
			if (n < 3u)
				return 0.f;
			return 0.5f * acc.Len();
		}

		// REGLE DE FUSION DU MATERIAU (arbitrage 2026-08-22), appliquee partout ou
		// des faces fusionnent — Quadify et Dissolve aujourd'hui. Ecrite UNE fois :
		// deux copies d'une meme regle divergent, on l'a deja paye sur les chemins
		// de ressources des bancs.
		//   • contributeur DOMINANT PAR L'AIRE ;
		//   • a aire egale (tolerance RELATIVE, cf. l'en-tete), INDICE LE PLUS BAS.
		// ⚠ « LA PLUS GRANDE AIRE » : DE QUOI ? L'arbitrage dit « la face qui
		// apportait la plus grande aire », mais il le justifie par « la couleur qui
		// couvrait le plus doit rester ». Sur DEUX contributeurs les deux lectures
		// coincident ; sur une region de Dissolve a N faces, elles divergent :
		// deux petites faces slot 1 (0,6 + 0,6) contre une grande slot 2 (1,0)
		// donnent slot 2 par face, slot 1 par couleur.
		// ON RETIENT LA COULEUR — c'est ce que la justification decrit, c'est ce que
		// l'utilisateur voit, et c'est le sur-ensemble : sur deux faces le resultat
		// est identique a l'autre lecture. Ecart signale a l'arbitre.
		// ⚠ LE POIDS N'EST PAS TOUJOURS UNE AIRE, et c'est voulu. Aux sites de FUSION
		// (Quadify, Dissolve) le contributeur est une face absorbee et son poids est son
		// AIRE. Aux sites de CREATION (chanfrein, extrusion d'aretes) le contributeur
		// est une face VOISINE, qui n'est pas absorbee du tout — son poids est la
		// LONGUEUR DE CONTOUR qu'elle partage avec la face creee.
		// Les deux cas sont la meme phrase : « dominance par une mesure, egalite par
		// l'indice le plus bas ». Les separer en deux fonctions aurait fait diverger
		// deux enonces identiques — la faute qu'on a deja payee trois fois ici.
		static uint16 EM_MaterialDominant(const uint16 *mats, const float32 *poids, uint32 n) {
			if (n == 0 || !mats || !poids)
				return 0;
			NkVector<uint16> ids;
			NkVector<float32> tot;
			for (uint32 i = 0; i < n; ++i) {
				uint32 k = 0;
				bool trouve = false;
				for (; k < (uint32)ids.Size(); ++k)
					if (ids[k] == mats[i]) {
						trouve = true;
						break;
					}
				if (!trouve) {
					ids.PushBack(mats[i]);
					tot.PushBack(poids[i]);
				} else
					tot[k] += poids[i];
			}
			uint16 best = ids[0];
			float32 bestA = tot[0];
			for (uint32 k = 1; k < (uint32)ids.Size(); ++k) {
				const float32 ref = (bestA > tot[k]) ? bestA : tot[k];
				const float32 tol = ((ref > 1.f) ? ref : 1.f) * 1e-6f;
				if (tot[k] > bestA + tol) {
					best = ids[k];
					bestA = tot[k];
				} else if (tot[k] >= bestA - tol && ids[k] < best) {
					best = ids[k]; // egalite d'aire : l'indice le plus bas tranche
					if (tot[k] > bestA)
						bestA = tot[k];
				}
			}
			return best;
		}

		// OMBRAGE FUSIONNE : le OU. Ecrit une fois, applique aux MEMES contributeurs
		// que le materiau, donc par la MEME table de parente.
		//
		// ⚠ POURQUOI DEUX REGLES D'AGREGATION ET NON UNE. La table de parente est
		// unique — c'est elle qui devait l'etre — mais les deux attributs n'ont pas
		// la meme nature : `smooth` est un BOOLEEN dont la reunion a un sens (« au
		// moins un contributeur etait lisse »), un index de materiau n'en a pas
		// (l'union de deux couleurs n'est pas une couleur). Appliquer la dominance
		// par l'aire a `smooth` ferait basculer a FLAT une face lissee des qu'un
		// voisin plat et plus grand la rejoint : une regression d'ombrage visible,
		// pour la seule satisfaction d'avoir une phrase unique.
		//   > Une regle unique qui degrade un cas reel n'est pas plus simple, elle
		//   > est seulement plus courte a enoncer.
		static uint8 EM_SmoothMerged(const uint8 *smooths, uint32 n) {
			if (!smooths)
				return 0;
			for (uint32 i = 0; i < n; ++i)
				if (smooths[i])
					return 1;
			return 0;
		}

		// Nombre de contributeurs dont le materiau N'EST PAS celui retenu. C'est la
		// PERTE, et elle se compte — elle ne se suppose pas nulle.
		static uint32 EM_MaterialLost(const uint16 *mats, uint32 n, uint16 retenu) {
			uint32 k = 0;
			for (uint32 i = 0; i < n; ++i)
				if (mats[i] != retenu)
					++k;
			return k;
		}

		// ATTRIBUT D'UNE FACE CREEE (sans mere) : elle herite de ses faces VOISINES.
		// `poids[i]` = longueur de contour que la face creee partage avec la voisine i.
		// Accumule dans `outLost` le nombre de voisines dont le materiau n'a pas ete
		// retenu — la perte, comptee et non supposee. Le compteur est ACCUMULATIF :
		// une operation cree plusieurs faces, et c'est leur total qui interesse.
		static NkEditMesh::FaceAttrib EM_AttribFromNeighbours(const uint16 *mats, const uint8 *smooths,
															  const float32 *poids, uint32 n, uint32 *outLost) {
			NkEditMesh::FaceAttrib a;
			if (n == 0)
				return a; // aucune voisine : slot 0 et FLAT, faute de mieux — et rien de perdu
			a.material = EM_MaterialDominant(mats, poids, n);
			a.smooth = EM_SmoothMerged(smooths, n);
			if (outLost)
				*outLost += EM_MaterialLost(mats, n, a.material);
			return a;
		}

		uint32 NkEditMesh::Quadify(float32 coplanarDot) {
			uint32 materiauxPerdus = 0;
			// Paires de triangles CONSÉCUTIFS (issus de la triangulation quad-par-quad).
			for (uint32 f1 = 0; f1 + 1 < (uint32)faces.Size(); f1 += 2) {
				const uint32 f2 = f1 + 1;
				if (!faces[f1].alive || !faces[f2].alive)
					continue;
				if (FaceSize(f1) != 3 || FaceSize(f2) != 3)
					continue;
				if (faces[f1].normal.Dot(faces[f2].normal) < coplanarDot)
					continue;
				// Demi-arête partagée h (dans f1) dont le twin est dans f2.
				NkEmId h = NK_EM_INVALID, start = faces[f1].hedge, hh = start;
				uint32 guard = 0;
				do {
					const NkEmId tw = hedges[hh].twin;
					if (tw != NK_EM_INVALID && hedges[tw].alive && hedges[tw].face == f2) {
						h = hh;
						break;
					}
					hh = hedges[hh].next;
				} while (hh != start && ++guard < 100000u);
				if (h == NK_EM_INVALID)
					continue; // triangles non adjacents
				// AIRES MESUREES AVANT LA COUTURE : apres, f2 est morte (aire 0) et
				// f1 porte deja le quad. Les lire trop tard ferait gagner f1 a tous
				// les coups — c'est-a-dire une regle qui ne peut pas departager.
				const float32 aires[2] = {FaceArea((NkEmId)f1), FaceArea((NkEmId)f2)};
				const uint16 mats[2] = {faces[f1].material, faces[f2].material};
				const NkEmId tw = hedges[h].twin;
				const NkEmId hA = hedges[h].next, hB = hedges[hA].next;	 // f1 : b->c, c->a
				const NkEmId hC = hedges[tw].next, hD = hedges[hC].next; // f2 : a->d, d->b
				hedges[hB].next = hC;
				hedges[hD].next = hA; // recoud la boucle quad
				hedges[hA].face = f1;
				hedges[hB].face = f1;
				hedges[hC].face = f1;
				hedges[hD].face = f1;
				faces[f1].hedge = hA;
				// f1 survit et absorbe f2 : l'ombrage doit suivre. Les deux triangles d'un
				// meme quad source portent normalement le meme reglage, mais on prend le OU
				// pour ne jamais perdre un lissage lors de la fusion.
				// Passe par EM_SmoothMerged, comme Dissolve : le OU etait ecrit ici en
				// clair, et le second site de fusion aurait eu a le re-decider.
				{
					const uint8 sm[2] = {faces[f1].smooth, faces[f2].smooth};
					faces[f1].smooth = EM_SmoothMerged(sm, 2u);
				}
				// MATERIAU : contributeur dominant par l'aire, egalite tranchee par
				// l'indice le plus bas. Un materiau ne se re-derive pas — contrairement
				// a `smooth` juste au-dessus, qu'un OU suffit a conserver parce qu'il
				// n'a que deux etats. Ici, ne rien faire equivaudrait a « f1 gagne
				// toujours », c'est-a-dire a laisser l'ORDRE DE PARCOURS decider.
				{
					const uint16 retenu = EM_MaterialDominant(mats, aires, 2u);
					materiauxPerdus += EM_MaterialLost(mats, 2u, retenu);
					faces[f1].material = retenu;
				}
				faces[f2].alive = 0;
				const uint32 a = hedges[h].origin, b = hedges[tw].origin;
				hedges[h].alive = 0;
				hedges[tw].alive = 0;
				hedges[h].face = NK_EM_INVALID;
				hedges[tw].face = NK_EM_INVALID;
				verts[a].hedge = hC;
				verts[b].hedge = hA; // repointe (h/tw morts)
			}
			RecomputeNormals();
			return materiauxPerdus;
		}

		// Grille de hachage spatiale : positions quantifiées au pas `eps` puis hachées. Les
		// sommets STRICTEMENT identiques (cas des primitives, qui réutilisent les mêmes
		// coordonnées pour chaque face) tombent forcément dans la même cellule. O(n) : aucune
		// comparaison par paires.
		void NkEditMesh::BuildVertexMerge(NkVector<uint32> &canon, float32 eps) const {
			const uint32 n = (uint32)verts.Size();
			canon.Resize(n);
			if (eps <= 0.f)
				eps = 1e-4f;
			const float32 inv = 1.f / eps;
			NkEmFlatMap cell(n);
			for (uint32 i = 0; i < n; ++i) {
				const NkVec3f p = verts[i].pos;
				// Arrondi (et non plancher) : deux coordonnées EXACTEMENT égales donnent la
				// même clé quel que soit leur signe.
				const int64 qx = (int64)(p.x * inv + (p.x >= 0.f ? 0.5f : -0.5f));
				const int64 qy = (int64)(p.y * inv + (p.y >= 0.f ? 0.5f : -0.5f));
				const int64 qz = (int64)(p.z * inv + (p.z >= 0.f ? 0.5f : -0.5f));
				const uint64 key = ((uint64)(qx & 0x1FFFFF)) | (((uint64)(qy & 0x1FFFFF)) << 21) |
								   (((uint64)(qz & 0x1FFFFF)) << 42);
				const uint32 *found = cell.Find(key);
				if (found)
					canon[i] = *found; // rattaché au représentant du groupe
				else {
					canon[i] = i;
					cell.Insert(key, i);
				}
			}
		}

		void NkEditMesh::PropagateSelectionToCoincident() {
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const uint32 n = (uint32)verts.Size();
			NkVector<uint8> repSel;
			repSel.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				repSel[i] = 0;
			// Le rang est propage AVEC la selection : sans cela, une copie coincidente
			// activee par propagation entrerait dans l'historique avec le rang 0 (ou,
			// pire, un rang plus recent), et « le premier selectionne » pourrait
			// designer une copie que l'utilisateur n'a jamais cliquee. On retient donc,
			// par identite soudee, le PLUS PETIT rang non nul rencontre.
			NkVector<uint32> repOrder;
			repOrder.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				repOrder[i] = 0;
			for (uint32 i = 0; i < n; ++i)
				if (verts[i].sel) {
					repSel[canon[i]] = 1;
					const uint32 o = verts[i].selOrder;
					if (o != 0 && (repOrder[canon[i]] == 0 || o < repOrder[canon[i]]))
						repOrder[canon[i]] = o;
				}
			for (uint32 i = 0; i < n; ++i) {
				verts[i].sel = repSel[canon[i]];
				verts[i].selOrder = verts[i].sel ? repOrder[canon[i]] : 0u;
			}
		}

		void NkEditMesh::LinkTwins() {
			// ⚠ Les jumeaux sont appariés sur l'IDENTITÉ TOPOLOGIQUE (position soudée), PAS
			// sur les indices bruts : sinon, avec des sommets dupliqués par face (primitives),
			// aucune demi-arête ne trouve son opposée dans la face voisine et le maillage
			// reste une collection de faces isolées (loop cut qui ne boucle pas, cage qui
			// compte les arêtes en double). Les attributs par coin ne sont pas touchés
			// -> rendu strictement inchangé.
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const uint32 nv = (uint32)canon.Size();
			auto C = [&](uint32 v) -> uint64 { return (uint64)((v < nv) ? canon[v] : v); };
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h)
				hedges[h].twin = NK_EM_INVALID;
			NkEmFlatMap map((uint32)hedges.Size());
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h) {
				if (!hedges[h].alive || hedges[h].next == NK_EM_INVALID)
					continue;
				const uint64 o = C(hedges[h].origin);
				const uint64 d = C(hedges[hedges[h].next].origin);
				if (o == d)
					continue; // arête dégénérée
				const uint64 opp = (d << 32) | o; // demi-arête opposée (d->o)
				const uint32 *found = map.Find(opp);
				if (found && hedges[*found].twin == NK_EM_INVALID) {
					hedges[h].twin = (NkEmId)*found;
					hedges[*found].twin = h;
				} else if (!found) {
					map.Insert((o << 32) | d, h);
				}
			}
		}

		void NkEditMesh::GetFaceVerts(NkEmId f, NkVector<NkEmId> &out) const {
			out.Clear();
			if (f >= (NkEmId)faces.Size())
				return;
			const NkEmId start = faces[f].hedge;
			if (start == NK_EM_INVALID)
				return;
			NkEmId h = start;
			uint32 guard = 0;
			do {
				out.PushBack(hedges[h].origin);
				h = hedges[h].next;
				if (++guard > 100000u)
					break; // garde-fou (topologie cassée)
			} while (h != start && h != NK_EM_INVALID);
		}

		// CONVENTION DE WINDING — le moteur rend en FRONT = HORAIRE (cf. primitives
		// NkMeshSystem : le cube déclare n[4]={0,1,0} pour la face du dessus dont la
		// boucle {3,7,6,2} donne (p1-p0)x(p2-p0) = -Y). Le produit vectoriel « CCW »
		// standard sort donc des normales INVERSÉES : on prend l'opposé (p2-p0)x(p1-p0).
		// Sans ça : éclairage retourné sur le maillage édité, extrusions vers l'INTÉRIEUR
		// et orientation « Normal » du gizmo à l'envers.
		static inline NkVec3f NkEmFaceCross(const NkVec3f &p0, const NkVec3f &p1, const NkVec3f &p2) {
			return (p2 - p0).Cross(p1 - p0);
		}

		void NkEditMesh::RecomputeNormals() {
			const uint32 nv = (uint32)verts.Size();
			const uint32 nf = (uint32)faces.Size();
			// 1) Normale de chaque face (vecteur NON normalisé = pondération par l'aire).
			NkVector<NkVec3f> fn;
			fn.Resize(nf);
			bool anySmooth = false;
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < nf; ++f) {
				fn[f] = {0.f, 0.f, 0.f};
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue;
				const NkVec3f p0 = verts[loop[0]].pos, p1 = verts[loop[1]].pos, p2 = verts[loop[2]].pos;
				NkVec3f n = NkEmFaceCross(p0, p1, p2);
				const float32 l = n.Len();
				faces[f].normal = (l > 1e-8f) ? n * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
				fn[f] = n;
				if (faces[f].smooth)
					anySmooth = true;
			}
			// 2) Accumulation FLAT (par index de sommet) — chemin historique, aucun coût
			//    supplémentaire tant qu'aucune face n'est lissée.
			NkVector<NkVec3f> flatAcc;
			flatAcc.Resize(nv);
			for (uint32 i = 0; i < nv; ++i)
				flatAcc[i] = {0.f, 0.f, 0.f};
			// 3) Accumulation SMOOTH (par sommet SOUDÉ) : seules les faces smooth y
			//    contribuent, et toutes les copies coïncidentes partagent le résultat.
			NkVector<uint32> canon;
			NkVector<NkVec3f> smoothAcc;
			NkVector<uint8> vertSmooth; // 1 = le sommet appartient à >=1 face lissée
			if (anySmooth) {
				BuildVertexMerge(canon);
				smoothAcc.Resize(nv);
				vertSmooth.Resize(nv);
				for (uint32 i = 0; i < nv; ++i) {
					smoothAcc[i] = {0.f, 0.f, 0.f};
					vertSmooth[i] = 0;
				}
			}
			auto C = [&](uint32 v) { return (anySmooth && v < (uint32)canon.Size()) ? canon[v] : v; };
			for (uint32 f = 0; f < nf; ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue;
				const bool sm = anySmooth && faces[f].smooth != 0;
				for (uint32 k = 0; k < (uint32)loop.Size(); ++k) {
					const uint32 v = loop[k];
					if (v >= nv)
						continue;
					if (sm) {
						const uint32 c = C(v);
						if (c < nv)
							smoothAcc[c] = smoothAcc[c] + fn[f];
						vertSmooth[v] = 1;
					} else
						flatAcc[v] = flatAcc[v] + fn[f];
				}
			}
			// 4) Normale finale du sommet. Un sommet touché À LA FOIS par des faces flat et
			//    des faces smooth (possible seulement si l'index est PARTAGÉ entre faces —
			//    les primitives, elles, dupliquent les coins) mélange les deux contributions :
			//    limite assumée de la structure, qui porte UNE normale par SOMMET et non par
			//    COIN (comme Blender le ferait avec des « loops »).
			for (uint32 i = 0; i < nv; ++i) {
				NkVec3f n = flatAcc[i];
				if (anySmooth && vertSmooth[i]) {
					const uint32 c = C(i);
					if (c < nv)
						n = n + smoothAcc[c];
				}
				const float32 l = n.Len();
				verts[i].normal = (l > 1e-8f) ? n * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
			}
		}

		bool NkEditMesh::SetShadeSmooth(bool smooth, bool selectedOnly) {
			const uint8 want = smooth ? (uint8)1 : (uint8)0;
			// Y a-t-il au moins une face sélectionnée ? Sinon on traite TOUT le maillage
			// (équivalent du « Shade Smooth » appliqué à l'objet entier).
			bool anySel = false;
			if (selectedOnly)
				for (uint32 f = 0; f < (uint32)faces.Size() && !anySel; ++f)
					if (faces[f].alive && FaceIsSelected(f))
						anySel = true;
			bool changed = false;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				if (anySel && !FaceIsSelected(f))
					continue;
				if (faces[f].smooth != want) {
					faces[f].smooth = want;
					changed = true;
				}
			}
			if (changed)
				RecomputeNormals();
			return changed;
		}

		bool NkEditMesh::AnyFaceSmooth() const {
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f)
				if (faces[f].alive && faces[f].smooth)
					return true;
			return false;
		}

		bool NkEditMesh::AllFacesSmooth() const {
			bool any = false;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				any = true;
				if (!faces[f].smooth)
					return false;
			}
			return any;
		}

		// Demi-arête vivante correspondant à l'arête (a,b), comparée sur l'IDENTITÉ
		// TOPOLOGIQUE (sommets soudés) : deux faces voisines n'emploient pas les mêmes
		// indices pour l'arête qu'elles partagent.
		static NkEmId NkEmFindHedge(const NkEditMesh &m, const NkVector<uint32> &canon, uint32 a, uint32 b) {
			const uint32 n = (uint32)canon.Size();
			auto C = [&](uint32 v) { return (v < n) ? canon[v] : v; };
			const uint32 ca = C(a), cb = C(b);
			for (uint32 h = 0; h < (uint32)m.hedges.Size(); ++h) {
				if (!m.hedges[h].alive || m.hedges[h].next == NK_EM_INVALID)
					continue;
				const uint32 o = C(m.hedges[h].origin), d = C(m.hedges[m.hedges[h].next].origin);
				if ((o == ca && d == cb) || (o == cb && d == ca))
					return (NkEmId)h;
			}
			return NK_EM_INVALID;
		}

		// -- ADJACENCE TOPOLOGIQUE POUR LES BOUCLES ---------------------------------
		// Valence (nb d'ARETES uniques incidentes) et « sur un bord » de chaque sommet
		// CANONIQUE (soude). Ces deux informations sont ce qui distingue, facon Blender,
		// un coin de cube (ferme, valence 3) d'un bord de grille (ouvert, valence 3 lui
		// aussi) : sans elles, la boucle derive sur l'un ou deborde sur l'autre.
		struct NkEmVertAdj {
				NkVector<uint16> valence;   // nb d'aretes uniques au sommet canonique
				NkVector<uint8> onBoundary; // 1 = au moins une arete incidente sans jumeau
		};

		static void NkEmBuildVertAdj(const NkEditMesh &m, const NkVector<uint32> &canon, NkEmVertAdj &out) {
			const uint32 nv = (uint32)m.verts.Size();
			const uint32 nc = (uint32)canon.Size();
			auto C = [&](uint32 v) { return (v < nc) ? canon[v] : v; };
			out.valence.Resize(nv);
			out.onBoundary.Resize(nv);
			for (uint32 i = 0; i < nv; ++i) {
				out.valence[i] = 0;
				out.onBoundary[i] = 0;
			}
			NkHashMap<uint64, uint8> seen;
			seen.Reserve((uint32)m.hedges.Size());
			for (uint32 h = 0; h < (uint32)m.hedges.Size(); ++h) {
				if (!m.hedges[h].alive || m.hedges[h].next == NK_EM_INVALID)
					continue;
				const uint32 o = C(m.hedges[h].origin), d = C(m.hedges[m.hedges[h].next].origin);
				if (o == d || o >= nv || d >= nv)
					continue;
				if (m.hedges[h].twin == NK_EM_INVALID) { // arete de BORD (maillage ouvert)
					out.onBoundary[o] = 1;
					out.onBoundary[d] = 1;
				}
				const uint32 lo = o < d ? o : d, hi = o < d ? d : o;
				const uint64 key = ((uint64)lo << 32) | hi;
				if (seen.Find(key))
					continue; // arete deja comptee (l'autre demi-arete)
				seen.InsertOrAssign(key, (uint8)1);
				out.valence[o] = (uint16)(out.valence[o] + 1);
				out.valence[d] = (uint16)(out.valence[d] + 1);
			}
		}

		// -- EDGE LOOP (Alt+clic) : REGLES DE BLENDER --------------------------------
		// La boucle avance d'arete en arete ; a chaque sommet traverse, la regle depend de
		// sa VALENCE (c'est exactement ce que fait le « loop walker » de Blender) :
		//
		//  - valence 4, sommet INTERIEUR (grille de quads reguliere) -> on CONTINUE TOUT
		//    DROIT : l'arete opposee a celle d'ou l'on vient, via next(twin(next(h))). La
		//    boucle file donc tout droit jusqu'au bord du maillage.
		//
		//  - valence 3, sommet INTERIEUR (coin ferme : TOUS les coins d'un cube brut) :
		//    « tout droit » n'existe pas. Blender ne derive PAS au hasard, la boucle suit
		//    le BORD DE LA FACE courante, c.-a-d. next(h). Sur un cube elle referme donc
		//    l'ANNEAU DE 4 ARETES qui fait le tour (le contour de la face cliquee) - au
		//    lieu des 7 aretes que donnait next(twin(next(h))) : cette regle-la tournait
		//    d'une face a chaque coin, puis repartait dans l'autre sens au 2e passage, en
		//    cumulant DEUX anneaux distincts moins l'arete de depart (4 + 4 - 1 = 7).
		//
		//  - sommet de BORD (une arete incidente sans jumeau : bord d'une grille ouverte),
		//    POLE (valence != 3 et != 4), ou face non-quad -> la boucle S'ARRETE.
		//    ATTENTION : c'est ici que la distinction bord/interieur est indispensable, un
		//    sommet du bord d'une grille est AUSSI de valence 3 mais ne doit surtout pas
		//    partir le long du bord - Blender s'y arrete.
		void NkEditMesh::GetEdgeLoop(uint32 a, uint32 b, NkVector<uint32> &outPairs) const {
			outPairs.Clear();
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const NkEmId h0 = NkEmFindHedge(*this, canon, a, b);
			if (h0 == NK_EM_INVALID)
				return;
			const uint32 nc = (uint32)canon.Size();
			auto C = [&](uint32 v) { return (v < nc) ? canon[v] : v; };
			NkEmVertAdj adj;
			NkEmBuildVertAdj(*this, canon, adj);
			NkHashMap<uint64, uint8> seen;
			auto emit = [&](NkEmId h) -> bool { // false si l'arete etait DEJA dans la boucle
				const uint32 o = hedges[h].origin, d = hedges[hedges[h].next].origin;
				const uint32 co = C(o), cd = C(d);
				const uint32 lo = co < cd ? co : cd, hi = co < cd ? cd : co;
				const uint64 key = ((uint64)lo << 32) | hi;
				if (seen.Find(key))
					return false;
				seen.InsertOrAssign(key, (uint8)1);
				outPairs.PushBack(o);
				outPairs.PushBack(d);
				return true;
			};
			// Avance d'un cran : h va (u -> v) ; renvoie la demi-arete sortante de v qui
			// prolonge la boucle selon les regles ci-dessus. NK_EM_INVALID = fin de boucle.
			auto step = [&](NkEmId h) -> NkEmId {
				if (h == NK_EM_INVALID || FaceSize(hedges[h].face) != 4)
					return NK_EM_INVALID; // on ne progresse qu'a travers des QUADS
				const NkEmId hn = hedges[h].next; // v -> w, dans la MEME face
				if (hn == NK_EM_INVALID)
					return NK_EM_INVALID;
				const uint32 v = C(hedges[hn].origin); // sommet traverse
				if (v >= (uint32)adj.valence.Size())
					return NK_EM_INVALID;
				if (adj.onBoundary[v])
					return NK_EM_INVALID; // bord du maillage -> Blender s'arrete
				const uint16 val = adj.valence[v];
				if (val == 4) {
					// RadialTwin et non `twin` : sur une arete portee par plus de deux
					// faces, `twin` en designe une ARBITRAIREMENT et la boucle basculait
					// sur une branche que personne n'avait choisie, sans le dire. On
					// s'arrete, comme Blender.
					const NkEmId tw = RadialTwin(hn); // w -> v, face voisine
					if (tw == NK_EM_INVALID)
						return NK_EM_INVALID;
					return hedges[tw].next; // v -> x : l'arete OPPOSEE a celle d'ou l'on vient
				}
				if (val == 3)
					return hn; // coin ferme : on suit le contour de la face (anneau du cube)
				return NK_EM_INVALID; // pole -> arret
			};
			emit(h0);
			// Sens AVANT. Si l'on retombe sur une arete deja emise, la boucle est FERMEE :
			// inutile (et nuisible) d'explorer le sens arriere - c'est exactement ce qui
			// faisait cumuler deux anneaux sur un cube.
			bool closed = false;
			{
				NkEmId h = h0;
				uint32 guard = 0;
				while (h != NK_EM_INVALID && ++guard < 100000u) {
					h = step(h);
					if (h == NK_EM_INVALID)
						break; // bord / pole atteint : boucle OUVERTE
					if (!emit(h)) {
						closed = true; // on a reboucle
						break;
					}
				}
			}
			if (closed)
				return;
			// Sens ARRIERE (boucle ouverte : grille, bord de maillage) - on repart du jumeau,
			// qui pointe dans l'autre sens. Meme regle : pas de jumeau unique, pas de
			// sens arriere.
			NkEmId h = RadialTwin(h0);
			uint32 guard = 0;
			while (h != NK_EM_INVALID && ++guard < 100000u) {
				h = step(h);
				if (h == NK_EM_INVALID || !emit(h))
					break;
			}
		}

		// -- FACE LOOP / EDGE RING (Alt+clic en mode FACE) ---------------------------
		// Anneau des faces traversees par l'arete (a,b) : de quad en quad par l'arete
		// OPPOSEE (meme parcours que le loop cut). Sur un cube brut cela donne bien les 4
		// faces qui font le tour. Si l'anneau bute sur un BORD, on repart dans l'AUTRE
		// sens depuis l'arete de depart, pour ne pas rendre une demi-boucle.
		void NkEditMesh::GetFaceLoop(uint32 a, uint32 b, NkVector<NkEmId> &outFaces) const {
			outFaces.Clear();
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const NkEmId hStart = NkEmFindHedge(*this, canon, a, b);
			if (hStart == NK_EM_INVALID)
				return;
			NkHashMap<uint64, uint8> seenF;
			// Parcourt l'anneau depuis h ; renvoie true si l'anneau s'est REFERME sur h0.
			auto walk = [&](NkEmId h, NkEmId h0) -> bool {
				uint32 guard = 0;
				while (h != NK_EM_INVALID && ++guard < 100000u) {
					const NkEmId f = hedges[h].face;
					if (f == NK_EM_INVALID || f >= (NkEmId)faces.Size() || !faces[f].alive)
						return false;
					if (!seenF.Find((uint64)f)) {
						seenF.InsertOrAssign((uint64)f, (uint8)1);
						outFaces.PushBack(f);
					}
					if (FaceSize(f) != 4)
						return false; // l'anneau ne traverse que des quads
					const NkEmId hn = hedges[h].next;
					if (hn == NK_EM_INVALID)
						return false;
					const NkEmId hOpp = hedges[hn].next; // arete opposee du quad
					if (hOpp == NK_EM_INVALID)
						return false;
					const NkEmId tw = hedges[hOpp].twin;
					if (tw == NK_EM_INVALID)
						return false; // bord -> anneau ouvert de ce cote
					if (tw == h0)
						return true; // anneau referme
					h = tw;
				}
				return false;
			};
			if (walk(hStart, hStart))
				return; // anneau complet
			const NkEmId back = hedges[hStart].twin;
			if (back != NK_EM_INVALID)
				walk(back, back);
		}

		bool NkEditMesh::FaceIsSelected(NkEmId f) const {
			if (f >= (NkEmId)faces.Size() || !faces[f].alive)
				return false;
			const NkEmId start = faces[f].hedge;
			if (start == NK_EM_INVALID)
				return false;
			NkEmId h = start;
			uint32 guard = 0, n = 0;
			do {
				const uint32 o = hedges[h].origin;
				if (o >= (uint32)verts.Size() || !verts[o].sel)
					return false;
				++n;
				h = hedges[h].next;
				if (++guard > 100000u)
					break;
			} while (h != start && h != NK_EM_INVALID);
			return n >= 3;
		}

		uint32 NkEditMesh::EdgeFaces(uint32 a, uint32 b, NkEmId &f0, NkEmId &f1) const {
			f0 = NK_EM_INVALID;
			f1 = NK_EM_INVALID;
			uint32 n = 0;
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h) {
				if (!hedges[h].alive || hedges[h].next == NK_EM_INVALID)
					continue;
				const uint32 o = hedges[h].origin, d = hedges[hedges[h].next].origin;
				if (!((o == a && d == b) || (o == b && d == a)))
					continue;
				const NkEmId f = hedges[h].face;
				if (f == NK_EM_INVALID || f >= (NkEmId)faces.Size() || !faces[f].alive)
					continue;
				if (n == 0) {
					f0 = f;
					n = 1;
				} else if (f != f0) {
					f1 = f;
					return 2;
				}
			}
			return n;
		}

		void NkEditMesh::GetUniqueEdges(NkVector<uint32> &outPairs) const {
			outPairs.Clear();
			// Aretes FILAIRES d'abord : elles n'ont AUCUNE demi-arete, donc la boucle
			// ci-dessous ne peut pas les trouver. Sans ce premier passage, un segment
			// cree avec F serait invisible en fil de fer — il existerait dans la
			// structure sans jamais etre dessine.
			for (uint32 e = 0; e < (uint32)edges.Size(); ++e) {
				if (!edges[e].alive || edges[e].faceCount != 0)
					continue;
				outPairs.PushBack(edges[e].v0);
				outPairs.PushBack(edges[e].v1);
			}
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h) {
				if (!hedges[h].alive)
					continue; // arête interne dissoute (quadify)
				const NkEmId tw = hedges[h].twin;
				if (tw == NK_EM_INVALID || h < tw) { // une seule des deux demi-arêtes
					const uint32 o = hedges[h].origin;
					const uint32 d = hedges[hedges[h].next].origin;
					outPairs.PushBack(o);
					outPairs.PushBack(d);
				}
			}
		}

		void NkEditMesh::Triangulate(NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx,
									 NkVector<NkEmId> &outTriFace) const {
			outV.Clear();
			outIdx.Clear();
			outTriFace.Clear();
			outV.Resize((uint32)verts.Size());
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				NkVertex3D nv{};
				nv.pos = verts[i].pos;
				nv.normal = verts[i].normal;
				// RESTITUES depuis le sommet, jamais reinventes : ecrire ici une
				// tangente en dur changeait le repere tangent de tout le maillage a
				// chaque aller-retour en edition (cf. struct Vert).
				nv.tangent = verts[i].tangent;
				nv.uv = verts[i].uv;
				nv.uv2 = verts[i].uv2;
				nv.color = verts[i].color;
				outV[i] = nv;
			}
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue;
				for (uint32 i = 1; i + 1 < (uint32)loop.Size(); ++i) { // éventail
					outIdx.PushBack(loop[0]);
					outIdx.PushBack(loop[i]);
					outIdx.PushBack(loop[i + 1]);
					outTriFace.PushBack((NkEmId)f);
				}
			}
			// ORTHOGONALISATION de la tangente, sans la REMPLACER.
			// Historique : cette boucle ecrasait la tangente par NkEmOrthoTangent(normal),
			// ce qui annulait la tangente d'origine transportee depuis la source — un
			// aller-retour en edition changeait donc le repere tangent de tout le maillage.
			// (Correctif initial du bug « carres blancs » : une tangente COLINEAIRE a la
			// normale donnait normalize(0) = NaN. Le probleme etait la colinearite, pas la
			// tangente elle-meme.)
			// On conserve donc la DIRECTION fournie et on retire seulement sa composante
			// le long de la normale (Gram-Schmidt) ; on ne fabrique une tangente de toutes
			// pieces que si le residu est degenere — c'est-a-dire exactement le cas qui
			// produisait le NaN.
			for (uint32 i = 0; i < (uint32)outV.Size(); ++i) {
				const NkVec3f n = outV[i].normal;
				NkVec3f t = outV[i].tangent;
				t = t - n * n.Dot(t);
				const float32 l2 = t.Dot(t);
				outV[i].tangent = (l2 > 1e-12f) ? t * (1.f / sqrtf(l2)) : NkEmOrthoTangent(n);
			}
		}

		void NkEditMesh::TriangulateShaded(NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx,
										   NkVector<NkEmId> &outTriFace) const {
			outV.Clear();
			outIdx.Clear();
			outTriFace.Clear();
			const uint32 nv = (uint32)verts.Size();
			outV.Resize(nv);
			for (uint32 i = 0; i < nv; ++i) {
				NkVertex3D nvx{};
				nvx.pos = verts[i].pos;
				nvx.normal = verts[i].normal;
				nvx.tangent = verts[i].tangent;   // restitue, cf. Triangulate
				nvx.uv = verts[i].uv;
				nvx.uv2 = verts[i].uv2;
				nvx.color = verts[i].color;
				outV[i] = nvx;
			}
			// claimed[v] = 1 -> le slot 1:1 du sommet v porte DÉJÀ la normale d'une face FLAT :
			// tout autre coin (flat d'une autre face, ou smooth) doit être DÉDOUBLÉ.
			NkVector<uint8> claimed;
			claimed.Resize(nv);
			for (uint32 i = 0; i < nv; ++i)
				claimed[i] = 0;
			NkVector<uint32> remap; // indice de sortie du coin, indexé par sommet d'origine
			remap.Resize(nv);
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				const uint32 fn = (uint32)loop.Size();
				if (fn < 3)
					continue;
				const bool flat = (faces[f].smooth == 0);
				for (uint32 k = 0; k < fn; ++k) {
					const uint32 v = loop[k];
					if (v >= nv)
						continue;
					uint32 o = v;
					if (flat) {
						if (!claimed[v]) {
							claimed[v] = 1;
							outV[v].normal = faces[f].normal;
						} else {
							NkVertex3D cp = outV[v];
							cp.pos = verts[v].pos;
							cp.uv = verts[v].uv;
							cp.normal = faces[f].normal;
							o = (uint32)outV.Size();
							outV.PushBack(cp);
						}
					} else if (claimed[v]) { // sommet confisqué par une face flat -> copie lisse
						NkVertex3D cp = outV[v];
						cp.pos = verts[v].pos;
						cp.uv = verts[v].uv;
						cp.normal = verts[v].normal;
						o = (uint32)outV.Size();
						outV.PushBack(cp);
					}
					remap[v] = o; // un sommet n'apparaît qu'UNE fois dans la boucle d'une face
				}
				for (uint32 i = 1; i + 1 < fn; ++i) { // éventail
					outIdx.PushBack(remap[loop[0]]);
					outIdx.PushBack(remap[loop[i]]);
					outIdx.PushBack(remap[loop[i + 1]]);
					outTriFace.PushBack((NkEmId)f);
				}
			}
			// ORTHOGONALISATION de la tangente, sans la REMPLACER.
			// Historique : cette boucle ecrasait la tangente par NkEmOrthoTangent(normal),
			// ce qui annulait la tangente d'origine transportee depuis la source — un
			// aller-retour en edition changeait donc le repere tangent de tout le maillage.
			// (Correctif initial du bug « carres blancs » : une tangente COLINEAIRE a la
			// normale donnait normalize(0) = NaN. Le probleme etait la colinearite, pas la
			// tangente elle-meme.)
			// On conserve donc la DIRECTION fournie et on retire seulement sa composante
			// le long de la normale (Gram-Schmidt) ; on ne fabrique une tangente de toutes
			// pieces que si le residu est degenere — c'est-a-dire exactement le cas qui
			// produisait le NaN.
			for (uint32 i = 0; i < (uint32)outV.Size(); ++i) {
				const NkVec3f n = outV[i].normal;
				NkVec3f t = outV[i].tangent;
				t = t - n * n.Dot(t);
				const float32 l2 = t.Dot(t);
				outV[i].tangent = (l2 > 1e-12f) ? t * (1.f / sqrtf(l2)) : NkEmOrthoTangent(n);
			}
		}

		// ── SOUS-MAILLES DEDUITES DU MATERIAU PAR FACE ──────────────────────────
		// On ne range RIEN : on triangule dans l'ordre courant des faces et on ouvre
		// une plage a chaque changement d'index. Un slot porte par deux faces non
		// voisines produit donc DEUX plages — c'est la propriete demandee (« lier ou
		// non »), pas un defaut a corriger en triant.
		void NkEditMesh::BuildSubMeshRanges(NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx,
											NkVector<SubMeshRange> &outRanges, uint32 *outDistinctSlots) const {
			NkVector<NkEmId> triFace;
			TriangulateShaded(outV, outIdx, triFace);
			outRanges.Clear();

			// Slots distincts : compte de presence sur 16 bits, sans allocation
			// dependant du nombre de faces. On ne peut pas se contenter de compter les
			// plages — deux plages peuvent designer le meme slot, c'est tout l'objet.
			NkVector<uint8> seen;
			uint32 distinct = 0;

			const uint32 triCount = (uint32)triFace.Size();
			for (uint32 t = 0; t < triCount; ++t) {
				const NkEmId f = triFace[t];
				const uint16 mat = (f < (NkEmId)faces.Size()) ? faces[f].material : (uint16)0;
				if ((uint32)mat >= (uint32)seen.Size()) {
					const uint32 old = (uint32)seen.Size();
					seen.Resize((uint32)mat + 1);
					for (uint32 i = old; i <= (uint32)mat; ++i)
						seen[i] = 0;
				}
				if (!seen[mat]) {
					seen[mat] = 1;
					++distinct;
				}
				// Prolonge la plage courante si l'index n'a pas change, sinon en ouvre
				// une neuve a la position courante du tampon d'indices.
				if (outRanges.Size() > 0 && outRanges[(uint32)outRanges.Size() - 1].material == mat) {
					outRanges[(uint32)outRanges.Size() - 1].indexCount += 3;
				} else {
					SubMeshRange r;
					r.material = mat;
					r.firstIndex = t * 3;
					r.indexCount = 3;
					outRanges.PushBack(r);
				}
			}
			if (outDistinctSlots)
				*outDistinctSlots = distinct;
		}

		// Regle de Blender : une face n'est affectee que si TOUS ses coins sont
		// selectionnes (cf. la declaration pour le pourquoi). Un sommet ne porte
		// jamais de materiau.
		uint32 NkEditMesh::AssignMaterialToSelectedFaces(uint16 slot) {
			uint32 n = 0;
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue; // une arete fil n'est pas une surface : pas de materiau
				bool all = true;
				for (uint32 k = 0; k < (uint32)loop.Size(); ++k) {
					if (loop[k] >= (NkEmId)verts.Size() || !verts[loop[k]].sel) {
						all = false;
						break;
					}
				}
				// La face DEJA selectionnee compte aussi : en mode face, c'est `sel` de
				// la face qui porte l'intention, pas celui de ses coins.
				if (all || faces[f].sel) {
					faces[f].material = slot;
					++n;
				}
			}
			return n;
		}

		void NkEditMesh::ToPolygons(NkVector<NkVertex3D> &ov, NkVector<uint32> &ofaceStart,
									NkVector<uint32> &ofaceVerts, NkVector<NkEditMesh::FaceAttrib> *ofaceAttrib) const {
			ov.Resize((uint32)verts.Size());
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				NkVertex3D nv{};
				nv.pos = verts[i].pos;
				nv.normal = verts[i].normal;
				// Tangente STOCKEE si elle est utilisable, sinon repli construit a
				// partir de la normale (sommets NES d'une operation d'edition, qui
				// n'en portent pas encore).
				nv.tangent = (verts[i].tangent.Dot(verts[i].tangent) > 1e-12f)
								 ? verts[i].tangent
								 : NkEmOrthoTangent(verts[i].normal);
				nv.uv = verts[i].uv;
				nv.uv2 = verts[i].uv2;
				nv.color = verts[i].color;
				ov[i] = nv;
			}
			ofaceStart.Clear();
			ofaceVerts.Clear();
			if (ofaceAttrib)
				ofaceAttrib->Clear();
			ofaceStart.PushBack(0);
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				// >= 2 : les « faces » à 2 sommets sont des ARÊTES FIL (extrusion de sommet,
				// façon Blender). Elles ne produisent pas de surface (Triangulate les ignore)
				// mais doivent survivre à l'aller-retour polygones (sinon elles disparaissent
				// dès la commande d'édition suivante).
				if (loop.Size() < 2)
					continue;
				for (uint32 k = 0; k < (uint32)loop.Size(); ++k)
					ofaceVerts.PushBack(loop[k]);
				ofaceStart.PushBack((uint32)ofaceVerts.Size());
				// Emis DANS LA MEME BOUCLE et sous les memes `continue` que
				// ofaceStart : c'est ce qui garantit l'alignement des deux tableaux.
				// Les remplir separement en reparcourant `faces` reintroduirait
				// exactement le decalage que ce transport existe pour eviter (les
				// faces mortes et les aretes fil ne sont pas emises).
				// ⚠ MATERIAU ET OMBRAGE PARTENT ENSEMBLE, dans UNE seule entree. Les
				// emettre dans deux tableaux paralleles obligerait chaque operation a
				// repondre deux fois a « de quelle face vient cette face ? », et deux
				// reponses finissent par diverger sans qu'aucune erreur ne se declenche.
				if (ofaceAttrib) {
					FaceAttrib a;
					a.material = faces[f].material;
					a.smooth = faces[f].smooth;
					ofaceAttrib->PushBack(a);
				}
			}
		}

		void NkEditMesh::BuildFromPolygons(const NkVertex3D *v, uint32 vc, const uint32 *faceStart, uint32 faceCount,
										   const uint32 *faceVerts, const FaceAttrib *faceAttrib) {
			Clear();
			verts.Resize(vc);
			for (uint32 i = 0; i < vc; ++i) {
				verts[i].pos = v[i].pos;
				verts[i].normal = v[i].normal;
				verts[i].uv = v[i].uv;
				// Attributs TRANSPORTES tels quels : ils ne servent pas a l'edition
				// topologique, mais sans eux l'aller-retour n'est pas une identite
				// (le repere tangent serait reinvente a la sortie) — cf. struct Vert.
				verts[i].tangent = v[i].tangent;
				verts[i].uv2 = v[i].uv2;
				verts[i].color = v[i].color;
				verts[i].hedge = NK_EM_INVALID;
				verts[i].sel = 0;
			}
			for (uint32 f = 0; f < faceCount; ++f) {
				const uint32 s = faceStart[f], e = faceStart[f + 1], n = e - s;
				if (n < 2)
					continue; // n == 2 => ARÊTE FIL (cf. ToPolygons)
				const NkEmId h0 = (NkEmId)hedges.Size();
				for (uint32 k = 0; k < n; ++k) {
					Hedge he;
					he.origin = faceVerts[s + k];
					he.next = h0 + ((k + 1) % n);
					he.face = (NkEmId)faces.Size();
					he.alive = 1;
					hedges.PushBack(he);
					if (verts[faceVerts[s + k]].hedge == NK_EM_INVALID)
						verts[faceVerts[s + k]].hedge = h0 + k;
				}
				Face fc;
				fc.hedge = h0;
				fc.alive = 1;
				// Indexe par `f`, l'index de la face D'ENTREE — pas par faces.Size().
				// Les deux different des qu'une face d'entree est sautee ci-dessus, et
				// c'est exactement le decalage silencieux qu'on cherche a eviter.
				if (faceAttrib) {
					fc.material = faceAttrib[f].material;
					// OMBRAGE HERITE DE LA FACE MERE (arbitrage 2026-08-22), et par la
					// MEME table de parente que le materiau. Il retombait a 0 ici :
					// BuildFromIndexed le RE-DEDUIT des normales des coins, mais ce
					// chemin-ci n'a pas de normales de coin a interroger — il n'a que
					// la parente, et elle etait ignoree.
					fc.smooth = faceAttrib[f].smooth;
				}
				faces.PushBack(fc);
			}
			LinkTwins();
			// ⚠️ LA COUCHE D'ARETES DE PREMIERE CLASSE, RECONSTRUITE ICI.
			// Sans cette ligne, TOUTE operation d'edition sortait avec une table
			// d'aretes VIDE -- et, bien plus grave, avec `canonOf` VIDE :
			//     au repos            table=960  canonOf=561
			//     apres subdivision   table=0    canonOf=0
			// Or `VertOwner()` retombe sur l'INDICE BRUT tant que `canonOf` est
			// vide (cf. la note de VertOwner, plus haut dans l'en-tete), et aucun
			// indice brut ne repond a une question topologique. Toute adjacence
			// calculee apres une operation travaillait donc sur une identite NON
			// SOUDEE, en silence. Le « 0 arete » du panneau n'etait que le symptome
			// visible d'une couche topologique absente.
			//
			// POURQUOI ICI, ET NULLE PART AILLEURS : 18 operations reconstruisent le
			// maillage et TOUTES entonnent dans cette fonction. Un appel ici en
			// couvre dix-huit ; un appel par operation serait dix-huit occasions
			// d'en oublier une. C'est deja ce que fait sa soeur `BuildFromIndexed`,
			// et l'ecart entre les deux ETAIT le defaut.
			//
			// COUT MESURE EN APPARIE (meme binaire, 7 runs, dispersion d'un run
			// isole : 93 %) : mediane 2,651 -> 3,603 ms sur une subdivision de
			// sphere, soit +36 %. Ce n'est PAS gratuit : `RebuildEdges` est un
			// parcours SEPARE, pas une fusion dans la boucle ci-dessus.
			// ⚠️ ET C'EST ASSUME : une identite topologique absente est un defaut de
			// CORRECTION, pas de performance -- 0,95 ms sur un geste manuel ne se
			// sent pas, une adjacence fausse se sent plus tard et sans rien lever.
			//
			// ⚠️ CET APPEL EST FAIT POUR MOURIR D'INANITION. Le modele vise est celui
			// de Blender : en edition le maillage est VIVANT et les operateurs le
			// MUTENT -- ils creent leurs aretes au passage, comme le font deja
			// `AddWireEdge` et `ExtrudeSelectedFacesInPlace`. Chaque operation
			// migree cesse de passer par ici ; le jour ou la derniere aura migre,
			// cette ligne n'aura plus d'appelant. On ne la supprime pas d'un coup.
			RebuildEdges();
			RecomputeNormals();
		}

		// =====================================================================
		// COUCHE DE COMMANDES D'ÉDITION — ops paramétrées sur la sélection interne
		// (Vert::sel). Portées depuis Demo3D_*HE : logique topologique PURE (pas de
		// dépendance UI/GPU). L'appelant régénère le rendu (Triangulate) ensuite.
		// =====================================================================

		void NkEditMesh::SelectAll() {
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i)
				verts[i].sel = 1;
		}

		void NkEditMesh::SelectNone() {
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i)
				verts[i].sel = 0;
		}

		bool NkEditMesh::AnyVertSelected() const {
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i)
				if (verts[i].sel)
					return true;
			return false;
		}

		// ── COMPOSANTES CONNEXES ────────────────────────────────────────────────
		// Parcours en profondeur sur l'IDENTITE SOUDEE. Le cycle disque (VertEdges)
		// est deja renseigne pour TOUTE copie coincidente — RebuildEdges donne a
		// chaque copie la tranche de son representant — donc on peut partir de
		// n'importe quel indice brut sans le canoniser d'abord. Verifie dans
		// RebuildEdges AVANT d'ecrire ceci : l'inverse aurait fait mourir le
		// parcours sur place, en silence, avec un resultat qui ressemble a une
		// reponse — une composante d'un seul sommet.
		uint32 NkEditMesh::ComputeConnectedComponents(NkVector<int32> &compOf) const {
			const uint32 n = (uint32)verts.Size();
			compOf.Clear();
			compOf.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				compOf[i] = -1;
			if (n == 0)
				return 0;

			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			auto cn = [&](uint32 v) -> uint32 {
				const uint32 r = (v < (uint32)canon.Size()) ? canon[v] : v;
				return (r < n) ? r : v;
			};

			// Composante du REPRESENTANT ; les copies la recopient a la fin.
			NkVector<int32> compRep;
			compRep.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				compRep[i] = -1;

			NkVector<uint32> stack;
			NkVector<NkEmId> inc;
			uint32 count = 0;

			for (uint32 i = 0; i < n; ++i) {
				const uint32 r0 = cn(i);
				if (compRep[r0] >= 0)
					continue;
				// Nouvelle composante. Un sommet ISOLE (aucune arete) en forme une a
				// lui seul : c'est voulu — Blender traite un sommet libre comme une
				// loose part, et l'invariant de somme l'exige.
				compRep[r0] = (int32)count;
				stack.Clear();
				stack.PushBack(r0);
				while (!stack.Empty()) {
					const uint32 v = stack.Back();
					stack.PopBack();
					VertEdges(v, inc);
					for (uint32 k = 0; k < (uint32)inc.Size(); ++k) {
						const NkEmId e = inc[k];
						if (e >= (NkEmId)edges.Size() || !edges[e].alive)
							continue;
						// L'arete porte deja des indices SOUDES ; on prend l'autre bout.
						const uint32 a = cn(edges[e].v0);
						const uint32 b = cn(edges[e].v1);
						const uint32 o = (a == v) ? b : a;
						if (o < n && compRep[o] < 0) {
							compRep[o] = (int32)count;
							stack.PushBack(o);
						}
					}
				}
				++count;
			}

			for (uint32 i = 0; i < n; ++i)
				compOf[i] = compRep[cn(i)];
			return count;
		}

		bool NkEditMesh::SelectLinked(uint32 seed, bool additive) {
			const uint32 n = (uint32)verts.Size();
			if (seed >= n)
				return false;
			NkVector<int32> compOf;
			ComputeConnectedComponents(compOf);
			const int32 want = compOf[seed];
			if (want < 0)
				return false;
			bool changed = false;
			for (uint32 i = 0; i < n; ++i) {
				if (compOf[i] != want)
					continue;
				if (!verts[i].sel) {
					verts[i].selOrder = ++selCounter; // entre dans l'historique, comme un clic
					verts[i].sel = 1;
					changed = true;
				}
			}
			if (!additive)
				for (uint32 i = 0; i < n; ++i)
					if (compOf[i] != want && verts[i].sel) {
						verts[i].sel = 0;
						verts[i].selOrder = 0;
						changed = true;
					}
			return changed;
		}

		bool NkEditMesh::SelectLinkedFromSelection() {
			const uint32 n = (uint32)verts.Size();
			NkVector<int32> compOf;
			const uint32 nc = ComputeConnectedComponents(compOf);
			if (nc == 0)
				return false;
			// Quelles composantes la selection touche-t-elle deja ?
			NkVector<uint8> hit;
			hit.Resize(nc);
			for (uint32 c = 0; c < nc; ++c)
				hit[c] = 0;
			bool any = false;
			for (uint32 i = 0; i < n; ++i)
				if (verts[i].sel && compOf[i] >= 0) {
					hit[(uint32)compOf[i]] = 1;
					any = true;
				}
			// RIEN DE SELECTIONNE = ON NE FAIT RIEN. Tout selectionner serait une
			// surprise, et un geste qui surprend est un geste qu'on annule.
			if (!any)
				return false;
			bool changed = false;
			for (uint32 i = 0; i < n; ++i) {
				if (compOf[i] < 0 || !hit[(uint32)compOf[i]] || verts[i].sel)
					continue;
				verts[i].selOrder = ++selCounter;
				verts[i].sel = 1;
				changed = true;
			}
			return changed;
		}

		bool NkEditMesh::PolyFaceSelected(const NkVector<uint32> &fv, uint32 s, uint32 e) const {
			for (uint32 k = s; k < e; k++) {
				const uint32 vi = fv[k];
				if (vi >= (uint32)verts.Size() || !verts[vi].sel)
					return false;
			}
			return e > s;
		}

		// ── SELECTION ORDONNEE ──────────────────────────────────────────────────
		// L'ordre est deduit des TRANSITIONS, pas d'un appel par clic : l'editeur
		// pousse son tableau ENTIER a chaque synchronisation, donc seuls les
		// changements reels doivent consommer un rang.
		void NkEditMesh::SetVertSelection(const uint8 *flags, uint32 count) {
			const uint32 n = (uint32)verts.Size();
			for (uint32 i = 0; i < n; ++i) {
				const uint8 want = (flags && i < count) ? (flags[i] ? (uint8)1 : (uint8)0) : (uint8)0;
				if (want && !verts[i].sel)
					verts[i].selOrder = ++selCounter; // nouvelle entree dans l'historique
				else if (!want)
					verts[i].selOrder = 0; // deselectionne : il sort de l'historique
				verts[i].sel = want;
			}
		}

		int32 NkEditMesh::FirstSelected() const {
			int32 best = -1, fallback = -1;
			uint32 bestOrder = 0xFFFFFFFFu;
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				if (!verts[i].sel)
					continue;
				if (fallback < 0)
					fallback = (int32)i;
				const uint32 o = verts[i].selOrder;
				if (o != 0 && o < bestOrder) {
					bestOrder = o;
					best = (int32)i;
				}
			}
			// Repli sur l'indice : une selection sans rang (script, chargement) doit
			// rester operable. Refuser serait plus deroutant qu'un ordre arbitraire.
			return (best >= 0) ? best : fallback;
		}

		int32 NkEditMesh::LastSelected() const {
			int32 best = -1, fallback = -1;
			uint32 bestOrder = 0;
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				if (!verts[i].sel)
					continue;
				fallback = (int32)i;
				const uint32 o = verts[i].selOrder;
				if (o > bestOrder) {
					bestOrder = o;
					best = (int32)i;
				}
			}
			return (best >= 0) ? best : fallback;
		}

		void NkEditMesh::ApplyVertSel(const NkVector<uint8> &vsel) {
			// Reapplication a plat APRES une re-topologie : les rangs de l'ancien
			// maillage ne designent plus rien (les sommets ont change d'identite), on
			// les efface plutot que de les laisser mentir. L'ordre repartira du
			// prochain geste.
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				verts[i].sel = (i < (uint32)vsel.Size()) ? vsel[i] : (uint8)0;
				verts[i].selOrder = 0;
			}
		}

		// EXTRUDE FACES : duplique les faces sélectionnées (cap), crée des quads latéraux sur
		// les arêtes de BORD, décale le cap le long de la normale. p.individual = chaque face
		// le long de SA normale (caps séparés). Préserve les n-gons.
		// ⚠ COMPORTEMENT BLENDER (défaut p.offset == 0) : le cap naît EXACTEMENT sur la face
		// d'origine et la SÉLECTION passe dessus. Rien ne bouge : l'utilisateur déplace/
		// tourne/redimensionne ensuite lui-même (gizmo, axe normal ou contrainte X/Y/Z).
		// ── COMPACTAGE DES FACES ET DEMI-ARETES MORTES ──────────────────────────
		// POURQUOI CETTE FONCTION EXISTE, ET CE QU'ELLE REPARE
		// Le chemin actuel (ToPolygons -> BuildFromPolygons) fait DISPARAITRE les
		// faces mortes sans que personne ne l'ait demande : `ToPolygons` ne les emet
		// pas, `BuildFromPolygons` reconstruit a partir de ce qu'il recoit. Une
		// operation EN PLACE, elle, les laisse ou elles sont.
		//
		// ⚠ CE N'EST PAS UN DETAIL D'HYGIENE. `Quadify` marque des faces mortes des
		// la construction d'une primitive, et le harnais imprime `faces.Size()` — le
		// TABLEAU, pas les vivantes. Une operation en place qui ne compacterait pas
		// rendrait donc un compte DIFFERENT sans qu'aucune topologie n'ait bouge.
		// C'est la forme la plus penible : le resultat est juste, le nombre est faux.
		void NkEditMesh::CompactDead(NkVector<NkEmId> *aRemapper) {
			const uint32 nf = (uint32)faces.Size(), nh = (uint32)hedges.Size();
			NkVector<uint32> fmap, hmap;
			fmap.Resize(nf);
			hmap.Resize(nh);
			uint32 nfAlive = 0, nhAlive = 0;
			for (uint32 f = 0; f < nf; ++f)
				fmap[f] = faces[f].alive ? nfAlive++ : NK_EM_INVALID;
			// Une demi-arete ne survit que si SA FACE survit : une demi-arete vivante
			// rattachee a une face morte est un residu, pas une entite.
			for (uint32 h = 0; h < nh; ++h) {
				const NkEmId hf = hedges[h].face;
				const bool ok = hedges[h].alive && hf != NK_EM_INVALID && hf < nf && faces[hf].alive;
				hmap[h] = ok ? nhAlive++ : NK_EM_INVALID;
			}
			if (nfAlive == nf && nhAlive == nh)
				return; // rien de mort : on ne recopie pas pour rien
			NkVector<Hedge> nhv;
			nhv.Resize(nhAlive);
			for (uint32 h = 0; h < nh; ++h) {
				if (hmap[h] == NK_EM_INVALID)
					continue;
				Hedge he = hedges[h];
				he.next = (he.next < nh && hmap[he.next] != NK_EM_INVALID) ? hmap[he.next] : NK_EM_INVALID;
				he.twin = (he.twin < nh && hmap[he.twin] != NK_EM_INVALID) ? hmap[he.twin] : NK_EM_INVALID;
				he.face = (he.face < nf) ? fmap[he.face] : NK_EM_INVALID;
				// Les cycles radiaux et le lien vers l'arete sont repris a zero par le
				// prochain RebuildEdges : les remapper ici serait entretenir une vue
				// que l'on vient d'invalider.
				he.edge = NK_EM_INVALID;
				he.rNext = NK_EM_INVALID;
				he.rPrev = NK_EM_INVALID;
				nhv[hmap[h]] = he;
			}
			NkVector<Face> nfv;
			nfv.Resize(nfAlive);
			for (uint32 f = 0; f < nf; ++f) {
				if (fmap[f] == NK_EM_INVALID)
					continue;
				Face fa = faces[f];
				fa.hedge = (fa.hedge < nh && hmap[fa.hedge] != NK_EM_INVALID) ? hmap[fa.hedge] : NK_EM_INVALID;
				nfv[fmap[f]] = fa;
			}
			// ⚠ Swap ET NON AFFECTATION. `hedges = nhv;` recopie l integralite du
			// tableau — 262 144 demi-aretes sur une grille 256x256, et une seconde
			// fois pour les faces. MESURE : la compaction pesait ~20 ms sur les 46 ms
			// de l operation, plus que tout le reste reuni. Swap est en O(1) : les
			// deux tableaux echangent leurs pointeurs, et l ancien meurt avec la
			// variable locale.
			hedges.Swap(nhv);
			faces.Swap(nfv);
			// ⚠ LES INDICES QUE L'APPELANT DETIENT SONT REMAPPES ICI, PAS AILLEURS.
			// Cette fonction RENUMEROTE ; toute liste de demi-aretes collectee avant
			// elle designe autre chose apres. La remettre a jour au retour, cote
			// appelant, obligerait a garder une copie de `hmap` -- c'est-a-dire a
			// publier un detail interne pour reparer un effet de bord.
			// Les entrees dont la demi-arete est morte sont RETIREES, pas mises a
			// INVALID : une liste de « choses a re-apparier » qui contiendrait des
			// trous ferait travailler l'appelant sur du vide sans qu'il le sache.
			if (aRemapper) {
				NkVector<NkEmId> compact;
				for (uint32 i = 0; i < (uint32)aRemapper->Size(); ++i) {
					const NkEmId v = (*aRemapper)[i];
					if (v < nh && hmap[v] != NK_EM_INVALID)
						compact.PushBack((NkEmId)hmap[v]);
				}
				aRemapper->Swap(compact);
			}
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i)
				verts[i].hedge = NK_EM_INVALID;
		}

		// ── RE-APPARIEMENT LOCAL DES JUMELLES ───────────────────────────────────
		// CE QU'ELLE REMPLACE, ET CE QU'ELLE NE PEUT PAS REMPLACER
		// `LinkTwins` global fait trois choses en O(MAILLAGE) : l'identite soudee,
		// la remise a zero de TOUTES les jumelles, puis un appariement par table sur
		// TOUTES les demi-aretes. La sonde de phases lui attribue 46 % du cout d'une
		// extrusion en place — de loin le premier poste.
		//
		// ⚠ MAIS L'APPARIEMENT GLOBAL EST UN « PREMIER ARRIVE » SUR TOUT LE MAILLAGE,
		// et cela ne se localise PAS en general. Des que deux demi-aretes portent la
		// meme cle orientee — ce qui arrive exactement quand des sommets se
		// superposent — le resultat depend de l'ordre de parcours de la TOTALITE des
		// demi-aretes. C'est le cas `offset = 0`, ou la nappe extrudee retombe sur
		// l'originale. Le reproduire localement demanderait de refaire le parcours
		// global, c'est-a-dire de ne rien localiser.
		//
		// On ne triche donc pas : on DETECTE cette ambiguite et on retombe sur le
		// chemin global. La condition est locale et exacte — un sommet duplique dont
		// l'identite soudee n'est pas lui-meme s'est pose sur un sommet existant.
		//
		// QUAND ELLE S'APPLIQUE, POURQUOI ELLE SUFFIT
		// L'ensemble a re-apparier se FERME en un pas : les demi-aretes du capuchon
		// (leur sommet d'origine a change), celles des parois (elles sont neuves), et
		// les ANCIENNES jumelles des premieres — car une demi-arete restee dehors qui
		// pointait vers un capuchon pointe maintenant dans le vide. Rien d'autre ne
		// peut avoir besoin de changer : si une demi-arete hors de cet ensemble
		// devait s'apparier a une demi-arete de l'ensemble, sa jumelle d'avant etait
		// deja dans l'ensemble, donc elle y serait aussi.
		//
		// Rend false si l'ambiguite est detectee ; l'appelant appelle alors
		// `LinkTwins()`.
		bool NkEditMesh::LinkTwinsLocal(const NkVector<NkEmId> &touchees, const NkVector<uint32> &copies,
										uint32 nv0) {
			BuildVertexMerge(canonOf);
			const uint32 nc = (uint32)canonOf.Size();
			// ── LA CONDITION D'AMBIGUITE, ET CE QU'ELLE NE DOIT PAS CONFONDRE ────
			// `BuildVertexMerge` donne a chaque cellule le PREMIER sommet qui l'occupe.
			// Une copie ajoutee en fin de tableau n'est donc son propre representant
			// que si sa position etait libre.
			//
			// ⚠ PREMIERE VERSION, TROP LARGE : elle refusait des qu'une copie n'etait
			// pas son propre representant. Or deux copies peuvent tres bien se
			// confondre ENTRE ELLES -- c'est le cas de toute primitive qui duplique
			// ses sommets par face (un cube en a 24 pour 8 positions, une sphere UV en
			// a sur ses coutures et ses poles) : les copies heritent naturellement de
			// la meme superposition. Ce n'est pas une ambiguite, c'est la soudure
			// ordinaire, et elle ne cree aucune cle orientee en double.
			// Le drapeau `local` du harnais l'a montre tout de suite : UN SEUL des dix
			// cas prenait le chemin local. Sans lui, « 295 vertes » aurait voulu dire
			// « le chemin qu'on vient d'ecrire ne sert jamais ».
			//
			// CE QUI EST VRAIMENT AMBIGU : une copie posee sur un sommet qui EXISTAIT
			// DEJA (indice < nv0). C'est alors -- et alors seulement -- que la cle
			// orientee d'un capuchon devient identique a celle d'une paroi, et que
			// l'appariement depend de l'ordre de parcours de tout le maillage.
			// C'est exactement `offset = 0`.
			for (uint32 k = 0; k < (uint32)copies.Size(); ++k) {
				const uint32 c = copies[k];
				if (c >= nc) {
					canonOf.Clear();
					return false;
				}
				const uint32 r = canonOf[c];
				if (r != c && r < nv0) { // pose sur un sommet PREEXISTANT
					canonOf.Clear();
					return false;
				}
			}
			auto C = [&](uint32 v) -> uint64 { return (uint64)((v < nc) ? canonOf[v] : v); };

			// Fermeture de l'ensemble, en un pas.
			NkVector<NkEmId> A;
			NkEmFlatMap dansA((uint32)touchees.Size() * 4u + 16u);
			auto ajouter = [&](NkEmId h) {
				if (h == NK_EM_INVALID || h >= (NkEmId)hedges.Size())
					return;
				if (dansA.Find((uint64)h))
					return;
				dansA.Insert((uint64)h, 1u);
				A.PushBack(h);
			};
			for (uint32 k = 0; k < (uint32)touchees.Size(); ++k) {
				ajouter(touchees[k]);
				if (touchees[k] < (NkEmId)hedges.Size())
					ajouter(hedges[touchees[k]].twin);
			}
			for (uint32 k = 0; k < (uint32)A.Size(); ++k)
				hedges[A[k]].twin = NK_EM_INVALID;

			// ⚠ ORDRE CROISSANT DES INDICES, comme le parcours global. `A` est
			// construit dans l'ordre des faces touchees, pas des indices : apparier
			// dans cet ordre-la donnerait le meme ENSEMBLE de paires sur un maillage
			// sans ambiguite, mais rien ne le garantit des que deux candidats existent.
			// Tri par insertion : `A` compte quelques dizaines d'elements pour une
			// edition locale, et un tri general couterait plus a ecrire qu'a executer.
			for (uint32 i = 1; i < (uint32)A.Size(); ++i) {
				const NkEmId x = A[i];
				uint32 j = i;
				while (j > 0 && A[j - 1] > x) {
					A[j] = A[j - 1];
					--j;
				}
				A[j] = x;
			}

			NkEmFlatMap map((uint32)A.Size() * 2u + 16u);
			for (uint32 k = 0; k < (uint32)A.Size(); ++k) {
				const NkEmId h = A[k];
				if (!hedges[h].alive || hedges[h].next == NK_EM_INVALID)
					continue;
				const uint64 o = C(hedges[h].origin);
				const uint64 d = C(hedges[hedges[h].next].origin);
				if (o == d)
					continue; // arete degeneree
				const uint32 *trouve = map.Find((d << 32) | o);
				if (trouve && hedges[*trouve].twin == NK_EM_INVALID) {
					hedges[h].twin = (NkEmId)*trouve;
					hedges[*trouve].twin = h;
				} else if (!trouve) {
					map.Insert((o << 32) | d, (uint32)h);
				}
			}
			canonOf.Clear(); // meme post-condition que le chemin global
			return true;
		}

		// ── EXTRUSION DE FACES, EN PLACE ────────────────────────────────────────
		// ETAPE 2 du chantier « cycles chaines ». La branche REGION (le defaut de
		// NkExtrudeParams) est reproduite a l'identique, mais SANS l'aller-retour par
		// la soupe de polygones : le maillage n'est plus detruit puis refait, il est
		// MUTE.
		//
		// ⚠ CE QUI DOIT ETRE REPRODUIT AU SOMMET PRES, ET POURQUOI
		// Le harnais compare V, F, E, bord, non-manifold, aire et centre. E et le
		// non-manifold sont recalcules PAR LE HARNAIS depuis la liste de faces et les
		// POSITIONS (soudure positionnelle), jamais depuis le cablage interne. Il
		// suffit donc de produire les MEMES faces sur les MEMES positions — mais il le
		// faut exactement, y compris dans les cas degeneres :
		//   `offset = 0` fait retomber la nappe extrudee SUR l'originale. La soudure
		//   fusionne alors les deux, et le cube rend `E=20 nonmanif=20`, la grille
		//   `nonmanif=16`. Une implantation « propre » qui refuserait la degenerescence
		//   donnerait un maillage plus sain — et une reference differente, sans qu'aucun
		//   defaut n'existe. On ACCEPTE la degenerescence, comme le chemin actuel.
		//
		// L'ORDRE DE DUPLICATION EST UN CONTRAT. Les sommets dupliques sont ajoutes a
		// la fin, dans l'ordre « faces selectionnees par indice croissant, puis coins
		// dans l'ordre du cycle de face » — exactement celui du chemin actuel. Un autre
		// ordre donnerait les memes formes avec d'autres numeros ; rien ne tomberait
		// aujourd'hui, et le premier temoin qui regarde un indice tomberait demain.
		bool NkEditMesh::ExtrudeSelectedFacesInPlace(const NkExtrudeParams &p, uint32 *outTwinsLocaux) {
			// ⚠ `outTwinsLocaux` N'EST PAS UN CONFORT DE DEBOGAGE. Le re-appariement
			// local retombe SILENCIEUSEMENT sur le chemin global quand il detecte une
			// ambiguite. Sans ce drapeau, une condition trop prudente ferait retomber
			// TOUS les cas, le resultat resterait juste, la mesure resterait bonne, et
			// personne ne saurait que le chemin qu'on vient d'ecrire ne sert jamais.
			if (outTwinsLocaux)
				*outTwinsLocaux = 0u;
			const uint32 nv0 = (uint32)verts.Size();
			const uint32 nf0 = (uint32)faces.Size();

			// -- 1. Faces selectionnees, dans l'ORDRE DES INDICES ----------------
			// Le meme ordre que `ToPolygons` : il saute les mortes et les aretes fil
			// (moins de 3 sommets), et c'est lui qui fixe la numerotation des copies.
			NkVector<NkEmId> selFaces;
			NkVector<uint32> selStart, selVerts; // boucles SAUVEGARDEES avant mutation
			selStart.PushBack(0);
			NkVec3f avgN{0.f, 0.f, 0.f};
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < nf0; ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts((NkEmId)f, loop);
				const uint32 n = (uint32)loop.Size();
				if (n < 3)
					continue;
				bool sel = true;
				for (uint32 k = 0; k < n && sel; ++k)
					sel = (loop[k] < (NkEmId)nv0) && verts[loop[k]].sel != 0;
				if (!sel)
					continue;
				selFaces.PushBack((NkEmId)f);
				for (uint32 k = 0; k < n; ++k)
					selVerts.PushBack(loop[k]);
				selStart.PushBack((uint32)selVerts.Size());
				avgN = avgN + NkEmFaceCross(verts[loop[0]].pos, verts[loop[1]].pos, verts[loop[2]].pos);
			}
			if (selFaces.Empty())
				return false;
			{
				const float32 l = avgN.Len();
				avgN = (l > 1e-6f) ? avgN * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
			}

			// -- 2. Offset AUTO : bbox de TOUS les sommets ------------------------
			// Tous, y compris ceux qu'aucune face ne touche : c'est ce que fait le
			// chemin actuel (`pv` contient l'integralite du tableau de sommets), et une
			// bbox restreinte donnerait un autre offset, donc une autre geometrie.
			float32 off = p.offset;
			if (off < 0.f) {
				NkVec3f bmn{1e30f, 1e30f, 1e30f}, bmx{-1e30f, -1e30f, -1e30f};
				for (uint32 i = 0; i < nv0; ++i) {
					const NkVec3f q = verts[i].pos;
					bmn.x = (q.x < bmn.x ? q.x : bmn.x);
					bmn.y = (q.y < bmn.y ? q.y : bmn.y);
					bmn.z = (q.z < bmn.z ? q.z : bmn.z);
					bmx.x = (q.x > bmx.x ? q.x : bmx.x);
					bmx.y = (q.y > bmx.y ? q.y : bmx.y);
					bmx.z = (q.z > bmx.z ? q.z : bmx.z);
				}
				off = (bmx - bmn).Len() * 0.08f;
			}

			// -- 3. Normales par sommet, restreintes a la SELECTION ---------------
			NkVector<NkVec3f> vertN;
			if (p.direction == NkExtrudeParams::AlongNormals) {
				NkVector<uint32> cano;
				BuildVertexMerge(cano);
				auto cn = [&](uint32 v) { return (v < (uint32)cano.Size()) ? cano[v] : v; };
				NkVector<NkVec3f> acc;
				acc.Resize(nv0);
				for (uint32 i = 0; i < nv0; ++i)
					acc[i] = {0.f, 0.f, 0.f};
				for (uint32 s = 0; s < (uint32)selFaces.Size(); ++s) {
					const uint32 b = selStart[s], e = selStart[s + 1];
					const NkVec3f fn =
						NkEmFaceCross(verts[selVerts[b]].pos, verts[selVerts[b + 1]].pos, verts[selVerts[b + 2]].pos);
					for (uint32 k = b; k < e; ++k) {
						const uint32 r = cn(selVerts[k]);
						if (r < nv0)
							acc[r] = acc[r] + fn;
					}
				}
				vertN.Resize(nv0);
				for (uint32 i = 0; i < nv0; ++i) {
					const uint32 r = cn(i);
					NkVec3f n = (r < nv0) ? acc[r] : NkVec3f{0.f, 0.f, 0.f};
					const float32 l = n.Len();
					vertN[i] = (l > 1e-6f) ? n * (1.f / l) : avgN;
				}
			}
			auto extrudePos = [&](const NkVec3f &base, uint32 srcIdx) -> NkVec3f {
				if (p.direction == NkExtrudeParams::ToCursor) {
					const NkVec3f d = p.target - base;
					const float32 t = (p.offset > 0.f) ? p.offset : 1.f;
					return base + d * (t > 1.f ? 1.f : t);
				}
				if (p.direction == NkExtrudeParams::AlongNormals && srcIdx < (uint32)vertN.Size())
					return base + vertN[srcIdx] * off;
				return base + avgN * off;
			};

			// -- 4. Copies des sommets de la region -------------------------------
			// Copies creees et demi-aretes touchees : les deux entrees du
			// re-appariement local. Collectees ICI parce que c'est le seul endroit qui
			// les voit -- les rededuire ensuite reviendrait a re-parcourir le maillage,
			// c'est-a-dire a payer ce qu'on cherche a eviter.
			NkVector<uint32> copies;
			NkVector<NkEmId> touchees;
			NkVector<int32> vmap;
			vmap.Resize(nv0);
			for (uint32 i = 0; i < nv0; ++i)
				vmap[i] = -1;
			NkVector<uint8> vsel;
			vsel.Resize(nv0);
			for (uint32 i = 0; i < nv0; ++i)
				vsel[i] = 0;
			// ⚠ CAPACITE RESERVEE AVANT LES AJOUTS, ET C'EST LE MEME PIEGE POUR LA
			// TROISIEME FOIS DE LA NUIT. `PushBack` sur un tableau plein RELOGE tout :
			// 66 049 sommets, 262 144 demi-aretes, 131 072 faces. Un seul relogement
			// coute plus que l'operation entiere, et il est en O(MAILLAGE) alors que
			// l'extrusion est en O(REGION) — c'est-a-dire qu'il annule exactement ce
			// que le chantier cherche a obtenir.
			// MESURE (sonde de phases, unites de temoin) : « capuchon et parois »
			// pesait 1,496 sur 6,464 — 23 % d'une operation qui, elle, ne touche que
			// 3 faces.
			// Les bornes sont SUPERIEURES et exactes : au plus un sommet copie et une
			// paroi par coin selectionne. Sur-reserver un peu ne coute qu'une fois ;
			// sous-reserver rendrait la reservation inutile sans le dire.
			{
				const uint32 marge = (uint32)selVerts.Size();
				verts.Reserve(nv0 + marge);
				hedges.Reserve((uint32)hedges.Size() + 4u * marge);
				faces.Reserve((uint32)faces.Size() + marge);
			}
			for (uint32 k = 0; k < (uint32)selVerts.Size(); ++k) {
				const uint32 vi = selVerts[k];
				if (vi >= nv0 || vmap[vi] >= 0)
					continue;
				vmap[vi] = (int32)verts.Size();
				Vert nvv = verts[vi];
				nvv.pos = extrudePos(verts[vi].pos, vi);
				// MEME REPLI DE TANGENTE QUE ToPolygons : une tangente degeneree y est
				// remplacee par une orthogonale construite depuis la normale. Ne pas le
				// faire ici laisserait aux copies une tangente que l'ancien chemin
				// n'aurait jamais produite — invisible sur la topologie, visible au rendu.
				nvv.tangent = (verts[vi].tangent.Dot(verts[vi].tangent) > 1e-12f)
								  ? verts[vi].tangent
								  : NkEmOrthoTangent(verts[vi].normal);
				nvv.hedge = NK_EM_INVALID;
				nvv.diskEdge = NK_EM_INVALID;
				nvv.sel = 1;
				nvv.selOrder = 0;
				verts.PushBack(nvv);
				vsel.PushBack(1);
				copies.PushBack((uint32)verts.Size() - 1u);
			}

			// -- 5. Aretes ORIENTEES de la region ---------------------------------
			// Une arete est INTERIEURE a la region si son opposee (b,a) appartient
			// aussi a une face selectionnee. Seules les autres engendrent une paroi.
			NkEmFlatMap selDir((uint32)selVerts.Size());
			for (uint32 s = 0; s < (uint32)selFaces.Size(); ++s) {
				const uint32 b = selStart[s], e = selStart[s + 1], n = e - b;
				for (uint32 k = 0; k < n; ++k)
					selDir.Insert(((uint64)selVerts[b + k] << 32) | (uint64)selVerts[b + (k + 1u) % n], 1u);
			}

			// -- 6. LE CAPUCHON : la face selectionnee MONTE ----------------------
			// Elle garde son identite, son materiau et son ombrage ; seules ses
			// demi-aretes changent de sommet d'origine. C'est tout ce que « en place »
			// veut dire — et c'est ce que l'aller-retour obtenait en reconstruisant
			// l'integralite du maillage.
			for (uint32 s = 0; s < (uint32)selFaces.Size(); ++s) {
				const NkEmId f = selFaces[s];
				const NkEmId start = faces[f].hedge;
				NkEmId h = start;
				uint32 garde = 0;
				do {
					const uint32 o = hedges[h].origin;
					if (o < nv0 && vmap[o] >= 0)
						hedges[h].origin = (NkEmId)vmap[o];
					touchees.PushBack(h); // son sommet d'origine a change
					h = hedges[h].next;
				} while (h != start && h != NK_EM_INVALID && ++garde < 100000u);
			}

			// -- 7. LES PAROIS : un quad par arete de BORD de region --------------
			for (uint32 s = 0; s < (uint32)selFaces.Size(); ++s) {
				const NkEmId f = selFaces[s];
				// Lus AVANT tout PushBack sur `faces` : une reference y survivrait mal
				// a une reallocation, et le piege a deja ete paye ailleurs.
				const uint16 mat = faces[f].material;
				const uint8 sm = faces[f].smooth;
				const uint32 b = selStart[s], e = selStart[s + 1], n = e - b;
				for (uint32 k = 0; k < n; ++k) {
					const uint32 a = selVerts[b + k], c = selVerts[b + (k + 1u) % n];
					if (selDir.Find(((uint64)c << 32) | (uint64)a))
						continue; // arete interieure : deux faces selectionnees la portent
					if (a >= nv0 || c >= nv0 || vmap[a] < 0 || vmap[c] < 0)
						continue;
					const uint32 quad[4] = {a, c, (uint32)vmap[c], (uint32)vmap[a]};
					const NkEmId h0 = (NkEmId)hedges.Size();
					const NkEmId gf = (NkEmId)faces.Size();
					for (uint32 q = 0; q < 4u; ++q) {
						Hedge he;
						he.origin = (NkEmId)quad[q];
						he.next = h0 + ((q + 1u) % 4u);
						he.face = gf;
						he.alive = 1;
						hedges.PushBack(he);
						touchees.PushBack(h0 + (NkEmId)q); // neuve
					}
					Face fa;
					fa.hedge = h0;
					fa.alive = 1;
					fa.material = mat; // heritage de la face MERE, comme le chemin actuel
					fa.smooth = sm;
					faces.PushBack(fa);
				}
			}

			// -- 8. Remise en etat, et rien de plus -------------------------------
			// ⚠ ETAPE 2, PHASE A : `LinkTwins` et `RecomputeNormals` restent GLOBAUX.
			// Ils sont en O(maillage), donc l'operation ne l'est pas encore ; ce qui a
			// disparu, c'est l'aller-retour par la soupe de polygones et la
			// reconstruction integrale des sommets et des demi-aretes. Les localiser
			// est la phase suivante — separee, pour qu'une divergence sache dire
			// laquelle des deux l'a causee.
			// ⚠ LA COMPACTION D'ABORD, ET ELLE REMAPPE `touchees`.
			// Premiere version : j'avais deplace `CompactDead` APRES l'appariement,
			// parce qu'elle renumerote et invalidait mes indices. Mesure : la region
			// GLOBALE passait de x0,47 a x1,04 -- apparier sur le tableau NON compacte,
			// c'est traiter autant de demi-aretes mortes que de vivantes. La solution
			// n'etait pas de deplacer la compaction mais de lui faire remapper la liste.
			CompactDead(&touchees);
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i)
				verts[i].hedge = NK_EM_INVALID;
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h) {
				if (!hedges[h].alive)
					continue;
				const NkEmId o = hedges[h].origin;
				if (o < (NkEmId)verts.Size() && verts[o].hedge == NK_EM_INVALID)
					verts[o].hedge = (NkEmId)h;
			}
			edges.Clear();
			canonOf.Clear();
			// ⚠ SEUIL DE RENTABILITE, ET IL EST MESURE, PAS DEVINE.
			// Le chemin local construit son voisinage, le trie et le hache : c'est
			// GAGNANT quand le voisinage est petit devant le maillage, et PERDANT quand
			// la region le couvre -- il fait alors strictement plus de travail que le
			// balayage global qu'il remplace. Mesure sur une region couvrant tout :
			// x1,04 au lieu de x0,47. Un quart des demi-aretes est la limite ou les
			// deux se valent.
			// Le drapeau `local` du harnais rend ce choix VISIBLE cas par cas : sans
			// lui, un seuil trop prudent eteindrait le chemin local partout et rien
			// ne le dirait.
			const bool vautLaPeine = ((uint32)touchees.Size() * 4u < (uint32)hedges.Size());
			if (!vautLaPeine || !LinkTwinsLocal(touchees, copies, nv0))
				LinkTwins(); // region trop large, ou ambiguite positionnelle
			else if (outTwinsLocaux)
				*outTwinsLocaux = 1u;
			RecomputeNormals();
			ApplyVertSel(vsel);
			return true;
		}

		bool NkEditMesh::ExtrudeSelectedFaces(const NkExtrudeParams &p) {
			// ⚠ LE BRANCHEMENT VERS `ExtrudeSelectedFacesInPlace` A ETE RETIRE, ET
			// C'EST UNE DECISION QUI REPOSE SUR UNE MESURE, PAS SUR UN DOUTE.
			// La version en place est JUSTE — le harnais l'atteste ligne a ligne
			// (famille `enplace/`) — mais elle est aujourd'hui PLUS LENTE sur une
			// edition locale : x1,16 a x1,40, mesure en normalisant par un temoin du
			// meme lancement. Elle ne gagne (x1,25) que lorsque la region couvre tout
			// le maillage.
			// La raison est entierement connue : quatre passes de remise en etat
			// heritees du chemin par la soupe de polygones restent en O(MAILLAGE) —
			// reconstruction globale de `Vert::hedge`, `CompactDead`, `LinkTwins`,
			// `RecomputeNormals`. Sonde a 65 536 faces : les desactiver toutes fait
			// tomber l'operation de 35,9 a 13,9 ms, et les 13,9 restants sont la
			// seule reconstruction de `Vert::hedge`.
			// ⚠ Supprimer l'aller-retour n'etait donc PAS l'endroit du gain. Tant que
			// ces quatre passes ne sont pas localisees, brancher ce chemin
			// RALENTIRAIT l'editeur. On garde le code, prouve, et on branche quand il
			// gagnera.
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// MATERIAU PAR FACE : les faces non touchees gardent leur index, la face
			// extrudee le transmet a son CAPUCHON et a toutes ses PAROIS laterales.
			// Regle de Blender : les faces neuves d'une extrusion heritent de la face
			// d'origine, pas du slot 0.
			NkVector<NkEditMesh::FaceAttrib> fm;
			NkVector<NkEditMesh::FaceAttrib> nfm;
			ToPolygons(pv, fs, fv, &fm);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			NkVec3f avgN{0.f, 0.f, 0.f};
			int32 selCount = 0;
			NkVector<uint8> faceSel;
			faceSel.Resize(fc);
			for (uint32 f = 0; f < fc; f++) {
				const uint32 s = fs[f], e = fs[f + 1];
				// Les arêtes FIL (2 sommets) ne sont pas des faces extrudables.
				const bool sel = (e - s >= 3) && PolyFaceSelected(fv, s, e);
				faceSel[f] = sel ? 1 : 0;
				if (sel) {
					selCount++;
					avgN = avgN + NkEmFaceCross(pv[fv[s]].pos, pv[fv[s + 1]].pos, pv[fv[s + 2]].pos);
				}
			}
			if (selCount == 0)
				return false;
			{
				float32 l = avgN.Len();
				avgN = (l > 1e-6f) ? avgN * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
			}
			float32 off = p.offset;
			if (off < 0.f) {
				NkVec3f bmn{1e30f, 1e30f, 1e30f}, bmx{-1e30f, -1e30f, -1e30f};
				for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
					NkVec3f q = pv[i].pos;
					bmn.x = (q.x < bmn.x ? q.x : bmn.x);
					bmn.y = (q.y < bmn.y ? q.y : bmn.y);
					bmn.z = (q.z < bmn.z ? q.z : bmn.z);
					bmx.x = (q.x > bmx.x ? q.x : bmx.x);
					bmx.y = (q.y > bmx.y ? q.y : bmx.y);
					bmx.z = (q.z > bmx.z ? q.z : bmx.z);
				}
				off = (bmx - bmn).Len() * 0.08f;
			}

			// ── NORMALES PAR SOMMET, restreintes aux faces SELECTIONNEES ────────
			// Necessaires a AlongNormals. Restreintes a la selection : inclure les
			// faces voisines NON extrudees ferait pencher la direction vers
			// l'exterieur de la region et tordrait le bord.
			// Accumulees sur l'identite SOUDEE puis redistribuees : sans cela, un coin
			// duplique par face partirait dans plusieurs directions et le maillage se
			// dechirerait le long des coutures.
			NkVector<NkVec3f> vertN;
			if (p.direction == NkExtrudeParams::AlongNormals) {
				NkVector<uint32> canon;
				BuildVertexMerge(canon);
				auto cn = [&](uint32 v) { return (v < (uint32)canon.Size()) ? canon[v] : v; };
				NkVector<NkVec3f> acc;
				acc.Resize((uint32)pv.Size());
				for (uint32 i = 0; i < (uint32)acc.Size(); i++)
					acc[i] = {0.f, 0.f, 0.f};
				for (uint32 f = 0; f < fc; f++) {
					if (!faceSel[f])
						continue;
					const uint32 s = fs[f];
					// Normale NON normalisee = ponderation par l'aire : une grande face
					// doit peser plus qu'un triangle degenere.
					const NkVec3f fn = NkEmFaceCross(pv[fv[s]].pos, pv[fv[s + 1]].pos, pv[fv[s + 2]].pos);
					for (uint32 k = fs[f]; k < fs[f + 1]; k++) {
						const uint32 r = cn(fv[k]);
						if (r < (uint32)acc.Size())
							acc[r] = acc[r] + fn;
					}
				}
				vertN.Resize((uint32)pv.Size());
				for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
					const uint32 r = cn(i);
					NkVec3f n = (r < (uint32)acc.Size()) ? acc[r] : NkVec3f{0.f, 0.f, 0.f};
					const float32 l = n.Len();
					// Repli sur la normale de region si l'accumulation s'annule (faces
					// opposees de part et d'autre du sommet) : mieux vaut la direction
					// commune qu'un deplacement nul silencieux.
					vertN[i] = (l > 1e-6f) ? n * (1.f / l) : avgN;
				}
			}

			// Deplacement d'UN sommet duplique, selon la variante demandee.
			auto extrudePos = [&](const NkVec3f &base, uint32 srcIdx) -> NkVec3f {
				if (p.direction == NkExtrudeParams::ToCursor) {
					// Chacun rejoint le point cible : les sommets convergent, la region
					// se ferme en pointe. offset sert de FRACTION du chemin (1 = au point).
					const NkVec3f d = p.target - base;
					const float32 t = (p.offset > 0.f) ? p.offset : 1.f;
					return base + d * (t > 1.f ? 1.f : t);
				}
				if (p.direction == NkExtrudeParams::AlongNormals && srcIdx < (uint32)vertN.Size())
					return base + vertN[srcIdx] * off;
				return base + avgN * off;
			};

			if (p.individual) {
				NkVector<uint32> nfs, nfv;
				nfs.PushBack(0);
				NkVector<uint8> vsel;
				vsel.Resize((uint32)pv.Size());
				for (uint32 i = 0; i < (uint32)vsel.Size(); i++)
					vsel[i] = 0;
				for (uint32 f = 0; f < fc; f++) {
					if (faceSel[f])
						continue;
					for (uint32 k = fs[f]; k < fs[f + 1]; k++)
						nfv.PushBack(fv[k]);
					nfs.PushBack((uint32)nfv.Size());
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
				}
				for (uint32 f = 0; f < fc; f++) {
					if (!faceSel[f])
						continue;
					const uint32 s = fs[f], e = fs[f + 1], n = e - s;
					NkVec3f fn = NkEmFaceCross(pv[fv[s]].pos, pv[fv[s + 1]].pos, pv[fv[s + 2]].pos);
					{
						float32 l = fn.Len();
						fn = (l > 1e-6f) ? fn * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
					}
					NkVector<uint32> dup;
					dup.Resize(n);
					for (uint32 k = 0; k < n; k++) {
						uint32 vi = fv[s + k];
						NkVertex3D nv = pv[vi];
						// ToCursor primes sur la normale de face : la cible est absolue.
						nv.pos = (p.direction == NkExtrudeParams::ToCursor) ? extrudePos(nv.pos, vi)
																						   : nv.pos + fn * off;
						dup[k] = (uint32)pv.Size();
						pv.PushBack(nv);
						vsel.PushBack(1);
					}
					for (uint32 k = 0; k < n; k++)
						nfv.PushBack(dup[k]);
					nfs.PushBack((uint32)nfv.Size()); // cap
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					for (uint32 k = 0; k < n; k++) {
						uint32 a = fv[s + k], b = fv[s + (k + 1) % n], na = dup[k], nb = dup[(k + 1) % n];
						nfv.PushBack(a);
						nfv.PushBack(b);
						nfv.PushBack(nb);
						nfv.PushBack(na);
						nfs.PushBack((uint32)nfv.Size());
						nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					}
				}
				BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
				  nfm.Data());
				ApplyVertSel(vsel);
				return true;
			}

			NkHashMap<uint64, uint8> selDir;
			for (uint32 f = 0; f < fc; f++) {
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				for (uint32 k = 0; k < n; k++) {
					uint32 a = fv[s + k], b = fv[s + (k + 1) % n];
					selDir.InsertOrAssign(((uint64)a << 32) | (uint64)b, (uint8)1);
				}
			}
			NkVector<int32> vmap;
			vmap.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)vmap.Size(); i++)
				vmap[i] = -1;
			NkVector<uint8> vsel;
			vsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)vsel.Size(); i++)
				vsel[i] = 0;
			for (uint32 f = 0; f < fc; f++) {
				if (!faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; k++) {
					uint32 vi = fv[k];
					if (vmap[vi] < 0) {
						vmap[vi] = (int32)pv.Size();
						NkVertex3D nv = pv[vi];
						nv.pos = extrudePos(nv.pos, vi); // Region / AlongNormals / ToCursor
						pv.PushBack(nv);
						vsel.PushBack(1);
					}
				}
			}
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			for (uint32 f = 0; f < fc; f++) {
				if (faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; k++)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
				nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
			}
			for (uint32 f = 0; f < fc; f++) {
				if (!faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; k++)
					nfv.PushBack((uint32)vmap[fv[k]]);
				nfs.PushBack((uint32)nfv.Size());
				nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
			}
			for (uint32 f = 0; f < fc; f++) {
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				for (uint32 k = 0; k < n; k++) {
					uint32 a = fv[s + k], b = fv[s + (k + 1) % n];
					if (selDir.Find(((uint64)b << 32) | (uint64)a))
						continue; // arête intérieure (2 faces sél.)
					uint32 na = (uint32)vmap[a], nb = (uint32)vmap[b];
					nfv.PushBack(a);
					nfv.PushBack(b);
					nfv.PushBack(nb);
					nfv.PushBack(na);
					nfs.PushBack((uint32)nfv.Size());
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
				}
			}
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
			  nfm.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// EXTRUDE SOMMETS (mode VERTEX) : chaque sommet sélectionné est DUPLIQUÉ et relié à
		// son original par une ARÊTE FIL (« face » à 2 sommets : pas de surface, mais une
		// vraie arête de la topologie, affichée dans la cage et éditable). La sélection passe
		// sur les NOUVEAUX sommets — à offset 0 ils sont confondus avec les originaux, comme
		// dans Blender : c'est l'utilisateur qui les déplace ensuite.
		bool NkEditMesh::ExtrudeSelectedVertices(const NkExtrudeParams &p) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// MATERIAU PAR FACE : transporte a travers le round-trip. Les faces conservees gardent leur index.
			NkVector<NkEditMesh::FaceAttrib> fm;
			ToPolygons(pv, fs, fv, &fm);
			const uint32 baseVerts = (uint32)pv.Size();
			NkVector<uint32> src; // sommets sélectionnés
			for (uint32 i = 0; i < (uint32)verts.Size() && i < baseVerts; i++)
				if (verts[i].sel)
					src.PushBack(i);
			if (src.Empty())
				return false;
			const float32 off = (p.offset > 0.f) ? p.offset : 0.f;
			NkVector<uint8> vsel;
			vsel.Resize(baseVerts);
			for (uint32 i = 0; i < baseVerts; i++)
				vsel[i] = 0;
			for (uint32 k = 0; k < (uint32)src.Size(); k++) {
				const uint32 a = src[k];
				NkVertex3D nv = pv[a];
				nv.pos = nv.pos + verts[a].normal * off;
				const uint32 b = (uint32)pv.Size();
				pv.PushBack(nv);
				vsel.PushBack(1);
				fv.PushBack(a); // arête fil a-b
				fv.PushBack(b);
				fs.PushBack((uint32)fv.Size());
				// Une ARETE FIL n'est pas une surface : elle ne porte pas de
				// materiau, donc slot 0. Mais elle DOIT avoir son entree, sinon
				// `fm` et `fs` se desalignent et toutes les faces suivantes
				// changeraient de materiau en silence.
				fm.PushBack(NkEditMesh::FaceAttrib{});
			}
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), fs.Data(), (uint32)fs.Size() - 1, fv.Data(),
							  fm.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// EXTRUDE ARÊTES (mode EDGE) : chaque arête dont les 2 extrémités sont sélectionnées
		// engendre une NOUVELLE arête (sommets dupliqués, partagés entre arêtes voisines) et
		// une FACE quad reliante (a,b,b',a'). Sélection = les nouveaux sommets. Offset 0 par
		// défaut (comportement Blender : le quad est plat tant que l'utilisateur n'a pas bougé).
		bool NkEditMesh::ExtrudeSelectedEdges(const NkExtrudeParams &p, uint32 *outMaterialChanged) {
			if (outMaterialChanged)
				*outMaterialChanged = 0;
			NkVector<uint32> pairs;
			GetUniqueEdges(pairs);
			NkVector<uint32> selA, selB;
			for (uint32 e = 0; e + 1 < (uint32)pairs.Size(); e += 2) {
				const uint32 a = pairs[e], b = pairs[e + 1];
				if (a >= (uint32)verts.Size() || b >= (uint32)verts.Size())
					continue;
				if (verts[a].sel && verts[b].sel) {
					selA.PushBack(a);
					selB.PushBack(b);
				}
			}
			if (selA.Empty())
				return false;
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// ATTRIBUTS PAR FACE : les faces EXISTANTES gardent les leurs. Sans ce
			// `&fa`, cette operation les remettait toutes a zero — une extrusion
			// d'arete repeignait le maillage entier en slot 0 et le rendait FLAT.
			NkVector<NkEditMesh::FaceAttrib> fa;
			ToPolygons(pv, fs, fv, &fa);
			const uint32 faceCountAvant = (fs.Size() > 0) ? (uint32)fs.Size() - 1u : 0u;
			// Faces incidentes a chaque arete (par INDICE de sommet, comme selA/selB).
			// Construit en UNE passe sur les faces emises par ToPolygons : c'est la
			// meme numerotation que `fa`, donc aucun decalage possible.
			NkHashMap<uint64, uint64> edgeFaces; // cle arete -> (f0+1) | ((f1+1) << 32)
			{
				auto ek = [](uint32 a, uint32 b) -> uint64 {
					const uint32 lo = (a < b) ? a : b, hi = (a < b) ? b : a;
					return ((uint64)lo << 32) | (uint64)hi;
				};
				for (uint32 f = 0; f < faceCountAvant; ++f) {
					const uint32 s0 = fs[f], e0 = fs[f + 1u], n0 = e0 - s0;
					if (n0 < 2u)
						continue;
					for (uint32 k = 0; k < n0; ++k) {
						const uint32 a = fv[s0 + k], b = fv[s0 + ((k + 1u) % n0)];
						if (a == b)
							continue;
						const uint64 key = ek(a, b);
						uint64 *q = edgeFaces.Find(key);
						const uint64 tag = (uint64)(f + 1u);
						if (!q)
							edgeFaces.InsertOrAssign(key, tag);
						else if ((*q & 0xFFFFFFFFull) != tag && (*q >> 32) == 0ull)
							edgeFaces.InsertOrAssign(key, *q | (tag << 32));
					}
				}
			}
			const uint32 baseVerts = (uint32)pv.Size();
			const float32 off = (p.offset > 0.f) ? p.offset : 0.f;
			NkVector<int32> dup; // sommet d'origine -> son duplicata (partagé)
			dup.Resize(baseVerts);
			for (uint32 i = 0; i < baseVerts; i++)
				dup[i] = -1;
			NkVector<uint8> vsel;
			vsel.Resize(baseVerts);
			for (uint32 i = 0; i < baseVerts; i++)
				vsel[i] = 0;
			auto dupOf = [&](uint32 v) -> uint32 {
				if (dup[v] >= 0)
					return (uint32)dup[v];
				NkVertex3D nv = pv[v];
				nv.pos = nv.pos + verts[v].normal * off;
				const uint32 id = (uint32)pv.Size();
				pv.PushBack(nv);
				vsel.PushBack(1);
				dup[v] = (int32)id;
				return id;
			};
			uint32 perdus = 0;
			for (uint32 k = 0; k < (uint32)selA.Size(); k++) {
				const uint32 a = selA[k], b = selB[k];
				const uint32 na = dupOf(a), nb = dupOf(b);
				fv.PushBack(a);
				fv.PushBack(b);
				fv.PushBack(nb);
				fv.PushBack(na);
				fs.PushBack((uint32)fv.Size());
				// LE QUAD CREE N'A PAS DE MERE : il herite de ses VOISINES, celles qui
				// touchent l'arete (a,b). Poids = longueur de contour partage — ici la
				// meme pour les deux, puisque c'est le MEME segment. L'egalite tranche
				// donc toujours, par l'indice le plus bas ; on passe quand meme par la
				// regle commune plutot que d'ecrire « prends le plus petit index », qui
				// serait un second enonce a maintenir.
				{
					const uint32 lo = (a < b) ? a : b, hi = (a < b) ? b : a;
					const uint64 key = ((uint64)lo << 32) | (uint64)hi;
					uint64 *q = edgeFaces.Find(key);
					uint16 mats[2];
					uint8 sms[2];
					float32 poids[2];
					uint32 nn = 0;
					const float32 lg = (pv[a].pos - pv[b].pos).Len();
					if (q) {
						const uint32 f0 = (uint32)((*q) & 0xFFFFFFFFull);
						const uint32 f1 = (uint32)((*q) >> 32);
						if (f0 > 0u && (f0 - 1u) < (uint32)fa.Size()) {
							mats[nn] = fa[f0 - 1u].material;
							sms[nn] = fa[f0 - 1u].smooth;
							poids[nn] = lg;
							++nn;
						}
						if (f1 > 0u && (f1 - 1u) < (uint32)fa.Size()) {
							mats[nn] = fa[f1 - 1u].material;
							sms[nn] = fa[f1 - 1u].smooth;
							poids[nn] = lg;
							++nn;
						}
					}
					fa.PushBack(EM_AttribFromNeighbours(mats, sms, poids, nn, &perdus));
				}
			}
			const uint32 nfc = (uint32)fs.Size() - 1u;
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), fs.Data(), nfc, fv.Data(),
							  (fa.Size() == nfc) ? fa.Data() : nullptr);
			ApplyVertSel(vsel);
			if (outMaterialChanged)
				*outMaterialChanged = perdus;
			return true;
		}

		// DELETE : supprime les faces sélectionnées, compacte les sommets orphelins.
		bool NkEditMesh::DeleteSelectedFaces() {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// MATERIAU PAR FACE : transporte a travers le round-trip. Les faces survivantes gardent leur index ; la face supprimee ne laisse rien.
			NkVector<NkEditMesh::FaceAttrib> fm;
			NkVector<NkEditMesh::FaceAttrib> nfm;
			ToPolygons(pv, fs, fv, &fm);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			NkVector<int32> remap;
			remap.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)remap.Size(); i++)
				remap[i] = -1;
			NkVector<NkVertex3D> nv2;
			NkVector<uint8> vsel;
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			uint32 removed = 0;
			for (uint32 f = 0; f < fc; f++) {
				const uint32 s = fs[f], e = fs[f + 1];
				if (PolyFaceSelected(fv, s, e)) {
					removed++;
					continue;
				} // face supprimée
				for (uint32 k = s; k < e; k++) {
					uint32 vi = fv[k];
					if (remap[vi] < 0) {
						remap[vi] = (int32)nv2.Size();
						nv2.PushBack(pv[vi]);
						vsel.PushBack(verts[vi].sel);
					}
					nfv.PushBack((uint32)remap[vi]);
				}
				nfs.PushBack((uint32)nfv.Size());
				nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
			}
			if (removed == 0)
				return false;
			BuildFromPolygons(nv2.Data(), (uint32)nv2.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
							  nfm.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// MERGE : soude les sommets sélectionnés en un (centroïde / premier / dernier),
		// retire les faces dégénérées (<3 sommets distincts), compacte.
		// ── CATMULL-CLARK ───────────────────────────────────────────────────────
		// Une passe. Voir NkEditMesh.h pour les regles et le pourquoi de chacune.
		static NkVertex3D NkEmMixVerts(const NkVertex3D *v, const uint32 *idx, uint32 n) {
			NkVertex3D o{};
			if (n == 0)
				return o;
			const float32 inv = 1.f / (float32)n;
			NkVec3f nrm{0.f, 0.f, 0.f}, tan{0.f, 0.f, 0.f};
			NkVec2f uv{0.f, 0.f}, uv2{0.f, 0.f};
			float32 cr = 0.f, cg = 0.f, cb = 0.f, ca = 0.f;
			for (uint32 k = 0; k < n; ++k) {
				const NkVertex3D &s = v[idx[k]];
				nrm = nrm + s.normal;
				tan = tan + s.tangent;
				uv = uv + s.uv;
				uv2 = uv2 + s.uv2;
				cr += (float32)((s.color >> 0) & 0xFFu);
				cg += (float32)((s.color >> 8) & 0xFFu);
				cb += (float32)((s.color >> 16) & 0xFFu);
				ca += (float32)((s.color >> 24) & 0xFFu);
			}
			const NkVec3f an = nrm * inv, at = tan * inv;
			const float32 ln = an.Len(), lt = at.Len();
			o.normal = (ln > 1e-6f) ? an * (1.f / ln) : NkVec3f{0.f, 1.f, 0.f};
			o.tangent = (lt > 1e-6f) ? at * (1.f / lt) : NkEmOrthoTangent(o.normal);
			o.uv = uv * inv;
			o.uv2 = uv2 * inv;
			// Couleur moyennee canal par canal : melanger les 32 bits d'un coup
			// melangerait le rouge d'un sommet avec le vert d'un autre.
			auto q = [](float32 x, float32 k) {
				const float32 r = x * k + 0.5f;
				const int32 i = (int32)(r < 0.f ? 0.f : (r > 255.f ? 255.f : r));
				return (uint32)i;
			};
			o.color = q(cr, inv) | (q(cg, inv) << 8) | (q(cb, inv) << 16) | (q(ca, inv) << 24);
			return o;
		}

		bool NkEditMesh::SubdivideCatmullClark(int32 levels) {
			if (levels < 1)
				return false;
			bool any = false;
			for (int32 lvl = 0; lvl < levels; ++lvl) {
				NkVector<NkVertex3D> pv;
				NkVector<uint32> fs, fv;
				// MATERIAU PAR FACE : meme transport que la subdivision lineaire.
				NkVector<NkEditMesh::FaceAttrib> fm;
				NkVector<NkEditMesh::FaceAttrib> nfm;
				ToPolygons(pv, fs, fv, &fm);
				const uint32 vc = (uint32)pv.Size();
				const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
				if (vc == 0 || fc == 0)
					return any;

				NkVector<uint32> canon;
				BuildVertexMerge(canon);
				auto CN = [&](uint32 v) { return (v < (uint32)canon.Size()) ? canon[v] : v; };

				// ── 1) POINT DE FACE : barycentre des sommets de la face ────────
				NkVector<NkVec3f> facePt;
				facePt.Resize(fc);
				for (uint32 f = 0; f < fc; ++f) {
					NkVec3f c{0.f, 0.f, 0.f};
					const uint32 s0 = fs[f], s1 = fs[f + 1], n = s1 - s0;
					for (uint32 k = s0; k < s1; ++k)
						c = c + pv[fv[k]].pos;
					facePt[f] = (n > 0) ? c * (1.f / (float32)n) : c;
				}

				// ── 2) ARETES SOUDEES : milieu, somme des points de face, degre ──
				struct EdgeAcc {
						NkVec3f pa{0.f, 0.f, 0.f}, pb{0.f, 0.f, 0.f}, fsum{0.f, 0.f, 0.f};
						uint32 nface = 0;
						// ⚠ LA CLE EST RANGEE ICI, ET C'EST LE CORRECTIF.
						// L'accumulation ci-dessous parcourait `edgeOf` — donc l'ORDRE DES
						// SEAUX d'une table de hachage — pour sommer `sumMid[v] += mid` en
						// FLOTTANTS. L'addition flottante n'est pas associative : changer la
						// fonction de hachage changeait les derniers bits des positions
						// lissees, et les heuristiques gloutonnes d'aval (decimation QEM,
						// retopologie) basculaient sur les quasi-egalites. Mesure : decim
						// 768->168 devenait 768->166, retopo 108 quads devenait 104.
						//   > Le defaut n'etait pas dans la table : il etait ici, dans du
						//   > code qui s'appuyait sans le dire sur un ordre que personne
						//   > n'avait jamais garanti.
						// En portant (ka,kb) sur l'accumulateur, la boucle parcourt
						// `edgeAcc` PAR INDICE — l'ordre du balayage des faces, qui, lui,
						// ne depend que du maillage.
						uint32 ka = 0, kb = 0;
				};
				NkHashMap<uint64, uint32> edgeOf;
				NkVector<EdgeAcc> edgeAcc;
				auto edgeKey = [](uint32 a, uint32 b) {
					const uint64 lo = a < b ? a : b, hi = a < b ? b : a;
					return (lo << 32) | hi;
				};
				for (uint32 f = 0; f < fc; ++f) {
					const uint32 s0 = fs[f], s1 = fs[f + 1], n = s1 - s0;
					for (uint32 k = 0; k < n; ++k) {
						const uint32 ia = fv[s0 + k], ib = fv[s0 + (k + 1) % n];
						const uint32 ca = CN(ia), cb = CN(ib);
						if (ca == cb)
							continue;
						const uint64 key = edgeKey(ca, cb);
						uint32 *ex = edgeOf.Find(key);
						uint32 ei;
						if (ex) {
							ei = *ex;
						} else {
							ei = (uint32)edgeAcc.Size();
							EdgeAcc e;
							e.pa = pv[ia].pos;
							e.pb = pv[ib].pos;
							e.ka = (uint32)(key >> 32);
							e.kb = (uint32)(key & 0xFFFFFFFFu);
							edgeAcc.PushBack(e);
							edgeOf.InsertOrAssign(key, ei);
						}
						edgeAcc[ei].fsum = edgeAcc[ei].fsum + facePt[f];
						edgeAcc[ei].nface++;
					}
				}

				// ── 3) SOMMETS DEPLACES, par identite soudee ────────────────────
				NkVector<NkVec3f> sumF, sumMid, bnd1, bnd2;
				NkVector<uint32> degF, degE, nBnd;
				sumF.Resize(vc);
				sumMid.Resize(vc);
				bnd1.Resize(vc);
				bnd2.Resize(vc);
				degF.Resize(vc);
				degE.Resize(vc);
				nBnd.Resize(vc);
				for (uint32 i = 0; i < vc; ++i) {
					sumF[i] = sumMid[i] = bnd1[i] = bnd2[i] = NkVec3f{0.f, 0.f, 0.f};
					degF[i] = degE[i] = nBnd[i] = 0;
				}
				for (uint32 f = 0; f < fc; ++f) {
					const uint32 s0 = fs[f], s1 = fs[f + 1];
					// Chaque identite soudee ne doit compter la face QU'UNE fois, meme si
					// plusieurs de ses copies apparaissent dans le meme polygone.
					for (uint32 k = s0; k < s1; ++k) {
						const uint32 c = CN(fv[k]);
						bool seen = false;
						for (uint32 j = s0; j < k; ++j)
							if (CN(fv[j]) == c) {
								seen = true;
								break;
							}
						if (seen)
							continue;
						sumF[c] = sumF[c] + facePt[f];
						degF[c]++;
					}
				}
				// PAR INDICE, jamais par l'ordre des seaux : cf. EdgeAcc::ka/kb.
				for (uint32 ei = 0; ei < (uint32)edgeAcc.Size(); ++ei) {
					const EdgeAcc &e = edgeAcc[ei];
					const uint32 a = e.ka, b = e.kb;
					const NkVec3f mid = (e.pa + e.pb) * 0.5f;
					for (int32 side = 0; side < 2; ++side) {
						const uint32 v = side ? b : a;
						if (v >= vc)
							continue;
						sumMid[v] = sumMid[v] + mid;
						degE[v]++;
						if (e.nface < 2) {
							// Bord : on retient les DEUX premiers milieux — la regle de
							// bordure ne fait intervenir qu'eux.
							if (nBnd[v] == 0)
								bnd1[v] = mid;
							else if (nBnd[v] == 1)
								bnd2[v] = mid;
							nBnd[v]++;
						}
					}
				}
				NkVector<NkVec3f> newPos;
				newPos.Resize(vc);
				for (uint32 i = 0; i < vc; ++i)
					newPos[i] = pv[i].pos;
				for (uint32 i = 0; i < vc; ++i) {
					const uint32 c = CN(i);
					if (c >= vc)
						continue;
					const uint32 n = degE[c];
					if (n < 2) {
						newPos[i] = pv[c].pos; // sommet isole ou pendant : inchange
						continue;
					}
					if (nBnd[c] >= 2) {
						// BORD : (M1 + 6V + M2) / 8. N'utilise que la bordure, donc le bord
						// reste franc au lieu d'etre aspire vers l'interieur.
						newPos[i] = (bnd1[c] + pv[c].pos * 6.f + bnd2[c]) * (1.f / 8.f);
						continue;
					}
					const float32 fn = (float32)n;
					const NkVec3f F = (degF[c] > 0) ? sumF[c] * (1.f / (float32)degF[c]) : pv[c].pos;
					const NkVec3f R = sumMid[c] * (1.f / fn);
					newPos[i] = (F + R * 2.f + pv[c].pos * (fn - 3.f)) * (1.f / fn);
				}

				// ── 4) RECONSTRUCTION : n quads par face de n coins ─────────────
				// Les attributs restent PAR COIN. Un point d'arete est indexe par la
				// paire de coins D'ORIGINE (et non par l'arete soudee) : deux faces de
				// part et d'autre d'une couture d'UV gardent ainsi chacune le leur.
				NkVector<NkVertex3D> ov;
				ov.Resize(vc);
				for (uint32 i = 0; i < vc; ++i) {
					ov[i] = pv[i];
					ov[i].pos = newPos[i];
				}
				NkHashMap<uint64, uint32> cornerEdge;
				NkVector<uint32> nfs, nfv;
				nfs.PushBack(0);
				uint32 pair[2];
				for (uint32 f = 0; f < fc; ++f) {
					const uint32 s0 = fs[f], s1 = fs[f + 1], n = s1 - s0;
					if (n < 3)
						continue;
					// Point de face (position issue du barycentre, attributs moyens).
					NkVertex3D fvtx = NkEmMixVerts(pv.Data(), &fv[s0], n);
					fvtx.pos = facePt[f];
					const uint32 fIdx = (uint32)ov.Size();
					ov.PushBack(fvtx);
					// Points d'arete du contour.
					NkVector<uint32> ep;
					ep.Resize(n);
					for (uint32 k = 0; k < n; ++k) {
						const uint32 ia = fv[s0 + k], ib = fv[s0 + (k + 1) % n];
						const uint64 ck = edgeKey(ia, ib);
						uint32 *ex = cornerEdge.Find(ck);
						if (ex) {
							ep[k] = *ex;
							continue;
						}
						pair[0] = ia;
						pair[1] = ib;
						NkVertex3D e = NkEmMixVerts(pv.Data(), pair, 2);
						const uint32 ca = CN(ia), cb = CN(ib);
						uint32 *sei = (ca != cb) ? edgeOf.Find(edgeKey(ca, cb)) : nullptr;
						if (sei) {
							const EdgeAcc &ea = edgeAcc[*sei];
							e.pos = (ea.nface >= 2)
										? (ea.pa + ea.pb + ea.fsum * (1.f / (float32)ea.nface) * 2.f) * 0.25f
										: (ea.pa + ea.pb) * 0.5f;
						} else {
							e.pos = (pv[ia].pos + pv[ib].pos) * 0.5f;
						}
						ep[k] = (uint32)ov.Size();
						ov.PushBack(e);
						cornerEdge.InsertOrAssign(ck, ep[k]);
					}
					for (uint32 k = 0; k < n; ++k) {
						const uint32 prev = (k + n - 1) % n;
						nfv.PushBack(fv[s0 + k]); // sommet d'origine, DEPLACE
						nfv.PushBack(ep[k]);	  // arete suivante
						nfv.PushBack(fIdx);		  // centre de face
						nfv.PushBack(ep[prev]);	  // arete precedente
						nfs.PushBack((uint32)nfv.Size());
						// Chaque quad fille herite de sa face mere, comme en subdivision lineaire.
						nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					}
				}
				if (nfs.Size() < 2)
					return any;
				NkVector<uint8> vsel;
				vsel.Resize((uint32)ov.Size());
				for (uint32 i = 0; i < (uint32)ov.Size(); ++i)
					vsel[i] = (i < vc && i < (uint32)verts.Size()) ? verts[i].sel : (uint8)0;
				BuildFromPolygons(ov.Data(), (uint32)ov.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
						  nfm.Data());
				ApplyVertSel(vsel);
				RecomputeNormals();
				RebuildEdges();
				any = true;
			}
			return any;
		}

		bool NkEditMesh::MergeSelectedVerts(const NkMergeParams &p) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// MATERIAU PAR FACE : transporte a travers la soudure.
			NkVector<NkEditMesh::FaceAttrib> fm;
			NkVector<NkEditMesh::FaceAttrib> nfm;
			ToPolygons(pv, fs, fv, &fm);
			NkVec3f c{0.f, 0.f, 0.f};
			int32 n = 0;
			for (uint32 i = 0; i < (uint32)pv.Size(); i++)
				if (i < (uint32)verts.Size() && verts[i].sel) {
					c = c + pv[i].pos;
					n++;
				}
			// PREMIER / DERNIER SELECTIONNE au sens des GESTES, pas des indices.
			// L'indice reflete l'ordre de construction du maillage ; sur un cube dont
			// les sommets sont dupliques par face, « plus petit indice » designait une
			// copie arbitraire, sans rapport avec le premier coin clique.
			int32 first = FirstSelected(), last = LastSelected();
			if (first < 0 || first >= (int32)pv.Size())
				first = 0;
			if (last < 0 || last >= (int32)pv.Size())
				last = first;
			if (n < 2)
				return false;
			c = c * (1.f / (float32)n);
			NkVector<int32> map;
			map.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)map.Size(); i++)
				map[i] = (int32)i;

			if (p.mode == NkMergeParams::Collapse) {
				// COLLAPSE : un merge PAR ILOT CONNEXE de la selection, chacun vers son
				// propre centre — c'est ce qui le distingue de Center (merge global).
				// Connexite etablie sur l'identite SOUDEE via les aretes : union-find.
				NkVector<uint32> canon;
				BuildVertexMerge(canon);
				auto cn = [&](uint32 v) { return (v < (uint32)canon.Size()) ? canon[v] : v; };
				NkVector<uint32> parent;
				parent.Resize((uint32)pv.Size());
				for (uint32 i = 0; i < (uint32)parent.Size(); i++)
					parent[i] = i;
				auto find = [&](uint32 x) {
					while (parent[x] != x) {
						parent[x] = parent[parent[x]];
						x = parent[x];
					}
					return x;
				};
				NkVector<uint32> pairs;
				GetUniqueEdges(pairs);
				auto selC = [&](uint32 v) { return v < (uint32)verts.Size() && verts[v].sel != 0; };
				for (uint32 e = 0; e + 1 < (uint32)pairs.Size(); e += 2) {
					const uint32 a = pairs[e], b = pairs[e + 1];
					if (selC(a) && selC(b)) {
						const uint32 ra = find(cn(a)), rb = find(cn(b));
						if (ra != rb)
							parent[ra] = rb;
					}
				}
				// Centre par ilot : chaque identite soudee comptee UNE fois (les copies
				// coincidentes fausseraient la moyenne).
				NkHashMap<uint32, uint32> repOf;   // racine -> sommet representant
				NkHashMap<uint64, uint8> counted;  // (racine<<32|canonId) deja compte
				NkHashMap<uint32, NkVec3f> sum;
				NkHashMap<uint32, uint32> cnt;
				for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
					if (!selC(i))
						continue;
					const uint32 r = find(cn(i));
					if (!repOf.Find(r))
						repOf.InsertOrAssign(r, i);
					const uint64 key = ((uint64)r << 32) | cn(i);
					if (!counted.Find(key)) {
						counted.InsertOrAssign(key, (uint8)1);
						NkVec3f *s = sum.Find(r);
						if (s)
							*s = *s + pv[i].pos;
						else
							sum.InsertOrAssign(r, pv[i].pos);
						uint32 *k2 = cnt.Find(r);
						if (k2)
							(*k2)++;
						else
							cnt.InsertOrAssign(r, 1u);
					}
				}
				for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
					if (!selC(i))
						continue;
					const uint32 r = find(cn(i));
					const uint32 rep2 = *repOf.Find(r);
					map[i] = (int32)rep2;
					pv[rep2].pos = *sum.Find(r) * (1.f / (float32)(*cnt.Find(r)));
				}
			} else if (p.mode == NkMergeParams::ByDistance) {
				// BY DISTANCE (« Remove Doubles ») : grappes de sommets selectionnes plus
				// proches que le seuil, chaque grappe vers son centre. Quantification
				// spatiale au pas du seuil — meme technique que BuildVertexMerge, mais au
				// seuil UTILISATEUR et restreinte a la selection.
				float32 eps = p.distance;
				if (eps <= 0.f) {
					NkVec3f mn = pv[0].pos, mx = pv[0].pos;
					for (uint32 i = 1; i < (uint32)pv.Size(); i++) {
						mn.x = NkMin(mn.x, pv[i].pos.x); mn.y = NkMin(mn.y, pv[i].pos.y); mn.z = NkMin(mn.z, pv[i].pos.z);
						mx.x = NkMax(mx.x, pv[i].pos.x); mx.y = NkMax(mx.y, pv[i].pos.y); mx.z = NkMax(mx.z, pv[i].pos.z);
					}
					eps = (mx - mn).Len() * 0.001f; // 0,1 % de la diagonale
					if (eps <= 0.f)
						eps = 1e-4f;
				}
				const float32 inv = 1.f / eps;
				NkHashMap<uint64, uint32> cell; // cle spatiale -> representant
				NkHashMap<uint64, NkVec3f> csum;
				NkHashMap<uint64, uint32> ccnt;
				bool merged = false;
				for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
					if (!(i < (uint32)verts.Size() && verts[i].sel))
						continue;
					const NkVec3f q = pv[i].pos;
					const int64 qx = (int64)(q.x * inv + (q.x >= 0.f ? 0.5f : -0.5f));
					const int64 qy = (int64)(q.y * inv + (q.y >= 0.f ? 0.5f : -0.5f));
					const int64 qz = (int64)(q.z * inv + (q.z >= 0.f ? 0.5f : -0.5f));
					const uint64 key = ((uint64)(qx & 0x1FFFFF)) | (((uint64)(qy & 0x1FFFFF)) << 21) |
									   (((uint64)(qz & 0x1FFFFF)) << 42);
					uint32 *rep2 = cell.Find(key);
					if (rep2) {
						map[i] = (int32)(*rep2);
						merged = true;
						NkVec3f *s = csum.Find(key);
						*s = *s + q;
						(*ccnt.Find(key))++;
					} else {
						cell.InsertOrAssign(key, i);
						csum.InsertOrAssign(key, q);
						ccnt.InsertOrAssign(key, 1u);
					}
				}
				if (!merged)
					return false; // aucun couple sous le seuil : rien a faire
				for (auto it = cell.Begin(); it != cell.End(); ++it)
					pv[it->Second].pos = (*csum.Find(it->First)) * (1.f / (float32)(*ccnt.Find(it->First)));
			} else {
				// Center / First / Last / AtCursor : UN seul representant global.
				const int32 rep = (p.mode == NkMergeParams::Last) ? last : first;
				NkVec3f target = (p.mode == NkMergeParams::First)	   ? pv[(uint32)first].pos
								 : (p.mode == NkMergeParams::Last)	   ? pv[(uint32)last].pos
								 : (p.mode == NkMergeParams::AtCursor) ? p.point
																	   : c;
				pv[(uint32)rep].pos = target;
				for (uint32 i = 0; i < (uint32)pv.Size(); i++)
					if (i < (uint32)verts.Size() && verts[i].sel)
						map[i] = rep;
			}
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			NkVector<int32> remap;
			remap.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)remap.Size(); i++)
				remap[i] = -1;
			NkVector<NkVertex3D> nv2;
			NkVector<uint8> vsel;
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint32> loop;
			for (uint32 f = 0; f < fc; f++) {
				loop.Clear();
				for (uint32 k = fs[f]; k < fs[f + 1]; k++) {
					uint32 vi = (uint32)map[fv[k]];
					if (loop.Empty() || loop[loop.Size() - 1] != vi)
						loop.PushBack(vi);
				} // retire doublons consécutifs
				if (loop.Size() >= 2 && loop[0] == loop[loop.Size() - 1])
					loop.Resize((uint32)loop.Size() - 1);
				if (loop.Size() < 3)
					continue; // face dégénérée
				for (uint32 k = 0; k < (uint32)loop.Size(); k++) {
					uint32 vi = loop[k];
					if (remap[vi] < 0) {
						remap[vi] = (int32)nv2.Size();
						nv2.PushBack(pv[vi]);
						vsel.PushBack(vi < (uint32)verts.Size() ? verts[vi].sel : (uint8)0);
					}
					nfv.PushBack((uint32)remap[vi]);
				}
				nfs.PushBack((uint32)nfv.Size());
				// SOUDURE : la face survivante garde SON index. Une face degeneree par la
				// fusion est sautee juste au-dessus, donc `nfm` reste aligne sur `nfs`.
				nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
			}
			BuildFromPolygons(nv2.Data(), (uint32)nv2.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
				  nfm.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// MAKE FACE : ajoute UNE face n-gon depuis les sommets sélectionnés (ordre d'index).
		// F (« Make Face ») façon Blender : crée UNE face à partir des sommets sélectionnés.
		// DEUX PIÈGES corrigés ici, tous deux invisibles en SOLIDE et flagrants en FIL DE FER :
		//  1. COPIES COÏNCIDENTES. Les primitives dupliquent leurs coins PAR FACE (un coin de
		//     cube = 3 sommets distincts au même endroit) et la sélection se propage aux
		//     copies (« flushing »). Prendre bêtement tous les `verts[i].sel` transformait donc
		//     « 4 sommets » en 12 -> face à 12 côtés. On ne garde qu'UN représentant par
		//     sommet TOPOLOGIQUE (identité canonique BuildVertexMerge).
		//  2. ORDRE DU CONTOUR. Les sommets étaient poussés dans l'ORDRE DES INDICES, qui n'a
		//     aucune raison de suivre le contour : le polygone zigzaguait entre les coins et
		//     ses arêtes traversaient la face — exactement l'aspect « la face est faite de deux
		//     triangles » signalé. On les ORDONNE maintenant angulairement autour de leur
		//     barycentre, dans le plan de meilleur ajustement -> contour simple, non croisé.

		// ── ARETES DE PREMIER PLAN (etape 1 BMesh) ──────────────────────────────
		void NkEditMesh::RebuildEdges() {
			// Les aretes FILAIRES sont conservees : elles ne sont incidentes a aucune
			// face, donc aucune reconstruction depuis les demi-aretes ne pourrait les
			// retrouver. C'est toute la raison d'etre de cette liste.
			NkVector<Edge> wires;
			for (uint32 i = 0; i < (uint32)edges.Size(); ++i)
				if (edges[i].alive && edges[i].faceCount == 0 && edges[i].hedge == NK_EM_INVALID)
					wires.PushBack(edges[i]);

			// Identite SOUDEE : deux sommets exactement au meme endroit sont une seule
			// identite topologique. Sans cela, un cube (24 sommets dupliques par face)
			// donnerait 24 aretes distinctes la ou il n'y en a que 12.
			// L'identite soudee est CONSERVEE (membre `canonOf`) et non plus jetee en
			// sortant : EdgeBetween et AddWireEdge la recalculaient chacun de leur
			// cote, sur tous les sommets, pour repondre a une question locale.
			BuildVertexMerge(canonOf);
			auto cn = [&](uint32 v) { return (v < (uint32)canonOf.Size()) ? canonOf[v] : v; };

			edges.Clear();
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h)
				hedges[h].edge = NK_EM_INVALID;
			// TAILLE ANNONCEE, ET NON DEVINEE. Le nombre d'aretes vaut au plus le
			// nombre de demi-aretes ; la moitie en est une borne serree pour une
			// variete. La table plate ne grandit donc jamais ici — la dimensionner
			// juste n'etait PAS ce qui rendait la fonction super-quadratique
			// (c'etait la repartition dans les seaux, corrigee en Q73), c'est ce qui
			// reste une fois la classe redressee.
			// ⚠ La mesure « sans Reserve 27,3 ms / avec 18,9 ms » qui figurait ici
			// portait sur `NkHashMap`, qui n'est plus la table employee : elle a ete
			// retiree plutot que laissee decrire un objet disparu.
			NkEmFlatMap seen((uint32)hedges.Size() / 2u + 16u);
			NkVector<NkEmId> loop;
			// ⚠ LE TABLEAU DE TABLEAUX A DISPARU. Il collectait les demi-aretes de
			// chaque arete AVANT de les aplatir, parce qu'une tranche contigue exige
			// de connaitre sa taille d'avance. Un cycle chaine ne l'exige pas : on
			// branche chaque incidence au moment ou on la voit. Le banc mesurait ce
			// tableau a 18,3 ms pour 131 072 entrees ; il n'est plus alloue du tout.
			for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts((NkEmId)f, loop);
				if (loop.Size() < 3)
					continue;
				const NkEmId start = faces[f].hedge;
				NkEmId hh = start;
				uint32 guard = 0;
				do {
					const uint32 o = cn(hedges[hh].origin);
					const uint32 d = cn(hedges[hedges[hh].next].origin);
					if (o != d) {
						const uint64 lo = o < d ? o : d, hi = o < d ? d : o;
						const uint64 key = (lo << 32) | hi;
						const uint32 *ex = seen.Find(key);
						uint32 ei;
						if (ex) {
							ei = *ex;
						} else {
							Edge e{};
							e.v0 = (NkEmId)lo;
							e.v1 = (NkEmId)hi;
							e.hedge = NK_EM_INVALID; // pose par RadialAppend, cf. ci-dessous
							e.faceCount = 0;
							e.alive = 1;
							ei = (uint32)edges.Size();
							seen.Insert(key, ei);
							edges.PushBack(e);
						}
						// LIEN DEMI-ARETE -> ARETE et CYCLE RADIAL, poses ici parce que
						// c'est le seul endroit qui voit chaque incidence exactement une
						// fois. Les recalculer ailleurs reviendrait a re-deduire ce que ce
						// balayage vient d'etablir.
						// ⚠ RadialAppend branche EN FIN de cycle. Parcourir depuis la tete
						// rend donc les demi-aretes dans leur ORDRE D'APPARITION, celui-la
						// meme que le reservoir aplati produisait. Brancher en tete
						// donnerait le meme ENSEMBLE dans l'ordre INVERSE, et toutes les
						// lignes `bmesh2/` et `acces/` tomberaient sans qu'aucune
						// topologie n'ait bouge.
						hedges[hh].edge = (NkEmId)ei;
						RadialAppend((NkEmId)ei, hh);
						edges[ei].radialCount++;
					}
					hh = hedges[hh].next;
				} while (hh != start && hh != NK_EM_INVALID && ++guard < 100000u);
			}

			// Reinsere les filaires, sauf si une face les a entre-temps recouvertes
			// (une arete filaire qui devient bord d'une face n'est plus filaire).
			for (uint32 i = 0; i < (uint32)wires.Size(); ++i) {
				const uint32 o = cn(wires[i].v0), d = cn(wires[i].v1);
				if (o == d)
					continue;
				const uint64 lo = o < d ? o : d, hi = o < d ? d : o;
				if (seen.Find((lo << 32) | hi))
					continue;
				Edge e{};
				e.v0 = (NkEmId)lo;
				e.v1 = (NkEmId)hi;
				e.hedge = NK_EM_INVALID;
				e.faceCount = 0;
				e.alive = 1;
				edges.PushBack(e); // filaire : cycle radial VIDE (hedge INVALID, count 0)
			}

			// ── PLUS RIEN A APLATIR ─────────────────────────────────────────────
			// Le cycle radial est deja pose, incidence par incidence, pendant le
			// balayage. Il ne reste qu'a realigner `faceCount` sur `radialCount` :
			// deux compteurs qui divergent finissent toujours par se contredire, et
			// c'est alors le mauvais qui est lu.
			for (uint32 i = 0; i < (uint32)edges.Size(); ++i) {
				const uint32 k = edges[i].radialCount;
				edges[i].faceCount = (uint8)(k > 255u ? 255u : k);
			}

			// ── CYCLE DISQUE ────────────────────────────────────────────────────
			// Renseigne sur le sommet REPRESENTANT de l'identite soudee ; les copies
			// coincidentes partagent la meme tranche. Sinon, sur une primitive qui
			// duplique ses sommets par face, chaque copie ne verrait qu'une partie de
			// ses aretes — exactement le piege deja rencontre sur le comptage d'aretes.
			{
				const uint32 nv = (uint32)verts.Size();
				// Toutes les tetes remises a vide : les copies coincidentes restent
				// vides POUR TOUJOURS et sont resolues a la lecture (VertOwner).
				// Une seule ecriture par identite, donc une seule verite.
				for (uint32 i = 0; i < nv; ++i)
					verts[i].diskEdge = NK_EM_INVALID;
				// Branchement dans l'ORDRE DES INDICES d'arete, en FIN de cycle. Le
				// tri par comptage qu'on remplace produisait exactement cet ordre —
				// et `acces/`, `bmesh2/` et les empreintes `aretes/` le lisent.
				// ⚠ `edges[i].v0` et `v1` sont deja des REPRESENTANTS (poses via `cn`),
				// donc on branche bien sur l'identite soudee et pas sur une copie.
				for (uint32 i = 0; i < (uint32)edges.Size(); ++i) {
					if (!edges[i].alive)
						continue;
					if (edges[i].v0 < nv)
						DiskAppend(edges[i].v0, (NkEmId)i);
					if (edges[i].v1 < nv)
						DiskAppend(edges[i].v1, (NkEmId)i);
				}
			}
		}

		// ── ACCES AUX DEUX CYCLES ───────────────────────────────────────────────
		NkEmId NkEditMesh::EdgeBetween(uint32 a, uint32 b) const {
			// ⚠ CETTE FONCTION APPELAIT BuildVertexMerge A CHAQUE APPEL : une table
			// de hachage sur TOUS les sommets pour repondre a une question que le
			// cycle disque rend locale. MESURE : 20 appels coutaient 0,2 ms sur 1 089
			// sommets et 41 ms sur 66 049 — un cout qui suit la taille du MAILLAGE
			// alors que la reponse ne depend que du VOISINAGE. Un editeur en emet un
			// par clic ; le cas ne se voit sur aucun cube.
			// L'identite soudee est desormais celle que RebuildEdges a posee.
			const uint32 ca = VertOwner(a), cb = VertOwner(b);
			if (ca == cb)
				return NK_EM_INVALID;
			const uint32 lo = ca < cb ? ca : cb, hi = ca < cb ? cb : ca;
			// Balayage du DISQUE du sommet plutot que de toutes les aretes : c'est
			// precisement ce que le cycle disque apporte.
			if (lo < (uint32)verts.Size()) {
				const NkEmId tete = verts[lo].diskEdge;
				NkEmId e = tete;
				uint32 garde = 0;
				while (e != NK_EM_INVALID) {
					if (e < (NkEmId)edges.Size() && edges[e].alive && edges[e].v0 == lo && edges[e].v1 == hi)
						return e;
					e = DiskNext(e, lo);
					// Garde-fou : un cycle rompu ferait tourner l'editeur sans fin,
					// ce qui est pire qu'une mauvaise reponse — on sort.
					if (e == tete || ++garde > (uint32)edges.Size() + 4u)
						break;
				}
			}
			return NK_EM_INVALID;
		}

		uint32 NkEditMesh::EdgeHedges(NkEmId e, NkVector<NkEmId> &out) const {
			out.Clear();
			if (e >= (NkEmId)edges.Size() || !edges[e].alive)
				return 0;
			const NkEmId tete = edges[e].hedge;
			NkEmId h = tete;
			uint32 garde = 0;
			while (h != NK_EM_INVALID && h < (NkEmId)hedges.Size()) {
				out.PushBack(h);
				h = hedges[h].rNext;
				if (h == tete || ++garde > (uint32)hedges.Size() + 4u)
					break;
			}
			return (uint32)out.Size();
		}

		uint32 NkEditMesh::EdgeFaces(NkEmId e, NkVector<NkEmId> &out) const {
			out.Clear();
			if (e >= (NkEmId)edges.Size() || !edges[e].alive)
				return 0;
			const NkEmId tete = edges[e].hedge;
			NkEmId h = tete;
			uint32 garde = 0;
			while (h != NK_EM_INVALID) {
				if (h >= (NkEmId)hedges.Size())
					break;
				const NkEmId f = hedges[h].face;
				if (f != NK_EM_INVALID) {
					bool dup = false;
					for (uint32 j = 0; j < (uint32)out.Size(); ++j)
						if (out[j] == f) {
							dup = true;
							break;
						}
					if (!dup)
						out.PushBack(f);
				}
				h = hedges[h].rNext;
				if (h == tete || ++garde > (uint32)hedges.Size() + 4u)
					break;
			}
			return (uint32)out.Size();
		}

		NkEmId NkEditMesh::EdgeOtherFace(NkEmId e, NkEmId f) const {
			// REFUS EXPLICITE sur une arete non manifold : « la face d'en face » n'y
			// existe pas. En retourner une au hasard donnerait un parcours qui semble
			// marcher et qui suit une branche arbitraire — le defaut precis que le
			// cycle radial existe pour rendre visible.
			if (e >= (NkEmId)edges.Size() || !edges[e].alive || edges[e].radialCount != 2)
				return NK_EM_INVALID;
			// Cycle a deux elements : la tete, puis sa suivante. On garde le meme
			// ORDRE de consultation que la tranche aplatie rendait.
			NkEmId h = edges[e].hedge;
			for (uint32 k = 0; k < 2u && h != NK_EM_INVALID; ++k) {
				if (h >= (NkEmId)hedges.Size())
					break;
				const NkEmId ff = hedges[h].face;
				if (ff != f)
					return ff;
				h = hedges[h].rNext;
			}
			return NK_EM_INVALID;
		}

		uint32 NkEditMesh::VertEdges(uint32 v, NkVector<NkEmId> &out) const {
			out.Clear();
			if (v >= (uint32)verts.Size())
				return 0;
			// La tranche est portee par le REPRESENTANT ; une copie coincidente n'a
			// pas la sienne, elle a la meme. Resoudre a la lecture plutot que de
			// recopier la tranche sur chaque copie, c'est ce qui permet a
			// AddWireEdge de la deplacer sans avoir a retrouver les 24 copies d'un
			// coin de cube.
			const uint32 r = VertOwner(v);
			const uint32 rr = (r < (uint32)verts.Size()) ? r : v;
			const NkEmId tete = verts[rr].diskEdge;
			NkEmId e = tete;
			uint32 garde = 0;
			while (e != NK_EM_INVALID && e < (NkEmId)edges.Size()) {
				out.PushBack(e);
				e = DiskNext(e, rr);
				if (e == tete || ++garde > (uint32)edges.Size() + 4u)
					break;
			}
			return (uint32)out.Size();
		}

		uint32 NkEditMesh::NonManifoldEdgeCount() const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)edges.Size(); ++i)
				if (edges[i].alive && edges[i].radialCount > 2)
					n++;
			return n;
		}

		uint32 NkEditMesh::EdgeCount() const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)edges.Size(); ++i)
				if (edges[i].alive)
					n++;
			return n;
		}

		// ── GREFFE SUR UN CYCLE, A LA PLACE D'UN RELOGEMENT DE TRANCHE ─────────
		// AVANT (reservoir CSR) : la tranche d'un sommet etait CONTIGUE, donc on ne
		// pouvait pas y inserer. Ajouter une arete recopiait toute la tranche a la
		// fin de `diskPool` et laissait l'ancienne en espace MORT — 30 entrees
		// devenaient 48 sur un cube apres trois ajouts.
		// MAINTENANT : deux ecritures de voisinage. Rien n'est deplace, rien n'est
		// perdu, et le cout ne depend plus du degre du sommet.
		//
		// ⚠ GREFFE EN FIN DE CYCLE (juste AVANT la tete), pas en tete. Parcourir
		// depuis la tete rend alors les aretes dans leur ordre d'insertion, donc
		// dans l'ordre CROISSANT des indices — exactement ce que produisait le tri
		// par comptage qu'on remplace. Greffer en tete donnerait le meme ENSEMBLE
		// dans l'ordre inverse : la topologie serait juste et la moitie des lignes
		// du harnais tomberaient, sans que rien n'ait bouge.
		void NkEditMesh::DiskAppend(uint32 r, NkEmId e) {
			if (r >= (uint32)verts.Size() || e >= (NkEmId)edges.Size())
				return;
			const NkEmId tete = verts[r].diskEdge;
			if (tete == NK_EM_INVALID) {
				verts[r].diskEdge = e;
				DiskSetNext(e, r, e);
				DiskSetPrev(e, r, e);
				return;
			}
			const NkEmId queue = DiskPrev(tete, r);
			DiskSetNext(e, r, tete);
			DiskSetPrev(e, r, queue);
			DiskSetNext(queue, r, e);
			DiskSetPrev(tete, r, e);
		}

		// Meme greffe, meme raison d'ordre, pour le cycle radial. La TETE est
		// `Edge::hedge` : le champ existait deja et designait « une demi-arete
		// porteuse ». Il n'a pas change de sens, il a gagné un cycle derriere lui.
		void NkEditMesh::RadialAppend(NkEmId e, NkEmId h) {
			if (e >= (NkEmId)edges.Size() || h >= (NkEmId)hedges.Size())
				return;
			const NkEmId tete = edges[e].hedge;
			if (tete == NK_EM_INVALID) {
				edges[e].hedge = h;
				hedges[h].rNext = h;
				hedges[h].rPrev = h;
				return;
			}
			const NkEmId queue = hedges[tete].rPrev;
			hedges[h].rNext = tete;
			hedges[h].rPrev = queue;
			hedges[queue].rNext = h;
			hedges[tete].rPrev = h;
		}

		NkEmId NkEditMesh::AddWireEdge(uint32 a, uint32 b) {
			if (a >= (uint32)verts.Size() || b >= (uint32)verts.Size())
				return NK_EM_INVALID;
			// La structure doit exister avant qu'on la mette a jour. `canon` de la
			// bonne taille est la condition : un `canon` du maillage PRECEDENT aurait
			// la bonne forme et le mauvais contenu.
			if (edges.Empty() || (uint32)canonOf.Size() != (uint32)verts.Size())
				RebuildEdges();
			const uint32 ca = VertOwner(a), cb = VertOwner(b);
			if (ca == cb)
				return NK_EM_INVALID; // meme sommet topologique : pas d'arete a creer
			const uint32 lo = ca < cb ? ca : cb, hi = ca < cb ? cb : ca;
			// DEJA PRESENTE ? Par le DISQUE du sommet, pas par un balayage de toutes
			// les aretes. C'est precisement ce que le cycle disque apporte, et ce
			// balayage etait le second cout lineaire de la fonction.
			{
				const NkEmId tete = verts[lo].diskEdge;
				NkEmId x = tete;
				uint32 garde = 0;
				while (x != NK_EM_INVALID && x < (NkEmId)edges.Size()) {
					if (edges[x].alive && edges[x].v0 == lo && edges[x].v1 == hi)
						return x; // deja presente (bord de face ou filaire)
					x = DiskNext(x, lo);
					if (x == tete || ++garde > (uint32)edges.Size() + 4u)
						break;
				}
			}
			// ── MISE A JOUR INCREMENTALE, ET RIEN DE PLUS ───────────────────────
			// AVANT : la fonction rappelait RebuildEdges() EN ENTIER apres avoir
			// empile l'arete, parce qu'empiler ne renseignait pas le cycle disque.
			// MESURE : 20 appels coutaient 11 ms sur 1 024 faces et 959 ms sur
			// 65 536 — soit ~48 ms par clic, et une croissance en x4,6 par x4 faces.
			// Tracer k aretes coutait k x n.
			// Or ni les faces ni les demi-aretes ne bougent quand on ajoute un
			// filaire : le seul etat a corriger est l'arete elle-meme et les deux
			// cycles disque de ses extremites. C'est le SEUL endroit de cette classe
			// ou la structure survit a l'operation ; partout ailleurs elle est
			// detruite et refaite (ToPolygons -> BuildFromPolygons).
			Edge e{};
			e.v0 = (NkEmId)lo;
			e.v1 = (NkEmId)hi;
			e.hedge = NK_EM_INVALID; // FILAIRE : aucune face incidente
			e.faceCount = 0;
			e.alive = 1;
			// Cycle radial VIDE : c'est exactement ce que RebuildEdges donne a un
			// filaire. Le piege d'un `radialStart` laisse a 0 — qui pointait sur la
			// tranche d'une AUTRE arete, inoffensif tant que le compte vaut 0 et faux
			// le jour ou quelqu'un lit le depart sans lire le compte — n'existe plus :
			// la tete vaut INVALID, et INVALID ne designe rien.
			e.radialCount = 0;
			const NkEmId ei = (NkEmId)edges.Size();
			edges.PushBack(e);
			DiskAppend(lo, ei);
			DiskAppend(hi, ei);
			return ei;
		}


		// ── LOT 5 : PROPORTIONAL EDITING + SYMETRIE ─────────────────────────────
		float32 NkEditMesh::ProportionalWeight(float32 d, float32 r, int32 falloff) {
			if (r <= 0.f)
				return d <= 0.f ? 1.f : 0.f;
			float32 t = 1.f - (d / r); // 1 au centre, 0 au bord
			if (t <= 0.f)
				return 0.f;
			if (t > 1.f)
				t = 1.f;
			switch (falloff) {
				case NkProportionalParams::Sphere: return sqrtf(1.f - (1.f - t) * (1.f - t));
				case NkProportionalParams::Root: return sqrtf(t);
				case NkProportionalParams::Sharp: return t * t;
				case NkProportionalParams::Linear: return t;
				case NkProportionalParams::Constant: return 1.f;
				case NkProportionalParams::Smooth:
				default:
					// Hermite 3t^2-2t^3 : tangente NULLE aux deux bouts, donc aucune
					// cassure ni au sommet tire ni a la limite du rayon. C'est ce qui
					// distingue une bosse propre d'un cone.
					return t * t * (3.f - 2.f * t);
			}
		}

		bool NkEditMesh::MoveSelected(const NkVec3f &delta, const NkProportionalParams &prop,
									  const NkSymmetryParams &sym) {
			const uint32 nv = (uint32)verts.Size();
			if (nv == 0)
				return false;

			// Identite SOUDEE : un coin duplique par face doit se deplacer d'un seul
			// bloc, sinon le maillage s'ouvre le long des coutures.
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			auto cn = [&](uint32 v) { return (v < (uint32)canon.Size()) ? canon[v] : v; };

			// Poids par sommet SOUDE. 1 = selectionne, 0 < w < 1 = entraine.
			NkVector<float32> w;
			w.Resize(nv);
			for (uint32 i = 0; i < nv; i++)
				w[i] = 0.f;
			bool any = false;
			for (uint32 i = 0; i < nv; i++)
				if (verts[i].sel) {
					w[cn(i)] = 1.f;
					any = true;
				}
			if (!any)
				return false;

			if (prop.enabled) {
				float32 r = prop.radius;
				if (r <= 0.f) {
					NkVec3f mn = verts[0].pos, mx = verts[0].pos;
					for (uint32 i = 1; i < nv; i++) {
						const NkVec3f q = verts[i].pos;
						mn.x = NkMin(mn.x, q.x); mn.y = NkMin(mn.y, q.y); mn.z = NkMin(mn.z, q.z);
						mx.x = NkMax(mx.x, q.x); mx.y = NkMax(mx.y, q.y); mx.z = NkMax(mx.z, q.z);
					}
					r = (mx - mn).Len() * 0.25f; // 25 % de la diagonale
				}
				// Distance EUCLIDIENNE au sommet selectionne le plus proche (mode par
				// defaut de Blender). O(n x s) : suffisant pour une selection d'edition,
				// et sans structure a maintenir — une accélération spatiale serait une
				// optimisation prematuree ici.
				NkVector<uint32> selIdx;
				for (uint32 i = 0; i < nv; i++)
					if (verts[i].sel)
						selIdx.PushBack(i);
				for (uint32 i = 0; i < nv; i++) {
					const uint32 ci = cn(i);
					if (w[ci] >= 1.f)
						continue; // deja plein poids
					float32 best = 1e30f;
					const NkVec3f q = verts[i].pos;
					for (uint32 k = 0; k < (uint32)selIdx.Size(); k++) {
						const float32 d = (verts[selIdx[k]].pos - q).Len();
						if (d < best)
							best = d;
					}
					const float32 ww = ProportionalWeight(best, r, prop.falloff);
					if (ww > w[ci])
						w[ci] = ww;
				}
			}

			// Positions AVANT deplacement : l'appariement miroir doit se faire sur le
			// maillage d'origine. Le faire au fur et a mesure ferait apparier des
			// sommets deja bouges et la symetrie deriverait.
			NkVector<NkVec3f> before;
			before.Resize(nv);
			for (uint32 i = 0; i < nv; i++)
				before[i] = verts[i].pos;

			// Deplacement direct.
			for (uint32 i = 0; i < nv; i++) {
				const float32 ww = w[cn(i)];
				if (ww > 0.f)
					verts[i].pos = verts[i].pos + delta * ww;
			}

			// ── SYMETRIE ────────────────────────────────────────────────────────
			if (sym.Any()) {
				// Un axe actif -> 1 miroir ; deux -> 3 ; trois -> 7 (toutes les
				// combinaisons non nulles de reflexions), comme Blender qui cumule les
				// cases cochees.
				const int32 combos[7][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0},
											{1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
				for (int32 ci = 0; ci < 7; ci++) {
					const bool useX = combos[ci][0] != 0, useY = combos[ci][1] != 0, useZ = combos[ci][2] != 0;
					if ((useX && !sym.x) || (useY && !sym.y) || (useZ && !sym.z))
						continue;
					auto mirror = [&](NkVec3f p) {
						NkVec3f d2 = p - sym.center;
						if (useX) d2.x = -d2.x;
						if (useY) d2.y = -d2.y;
						if (useZ) d2.z = -d2.z;
						return sym.center + d2;
					};
					for (uint32 i = 0; i < nv; i++) {
						const float32 ww = w[cn(i)];
						if (ww <= 0.f)
							continue;
						const NkVec3f src = before[i];
						const NkVec3f tgt = mirror(src);
						// Sommet SUR le plan de symetrie : il est son propre miroir. Son
						// deplacement doit etre PROJETE dans le plan, sinon il quitte
						// l'axe et casse la symetrie qu'on cherche a maintenir.
						if ((tgt - src).Len() <= sym.tolerance) {
							NkVec3f d3 = delta * ww;
							if (useX) d3.x = 0.f;
							if (useY) d3.y = 0.f;
							if (useZ) d3.z = 0.f;
							verts[i].pos = src + d3;
							continue;
						}
						// Sinon : trouver le sommet a la position miroir dans le maillage
						// AVANT deplacement, et lui appliquer le delta reflechi.
						NkVec3f dm = delta;
						if (useX) dm.x = -dm.x;
						if (useY) dm.y = -dm.y;
						if (useZ) dm.z = -dm.z;
						for (uint32 j = 0; j < nv; j++) {
							if (w[cn(j)] > 0.f)
								continue; // deja deplace par la selection elle-meme
							if ((before[j] - tgt).Len() <= sym.tolerance)
								verts[j].pos = before[j] + dm * ww;
						}
					}
				}
			}

			RecomputeNormals();
			return true;
		}

		bool NkEditMesh::MakeEdgeFromSelected() {
			// Un seul REPRESENTANT par sommet topologique : les primitives dupliquent
			// leurs sommets par face, donc « deux sommets selectionnes » peut vouloir
			// dire six indices bruts pointant deux positions.
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			NkHashMap<uint32, uint8> taken;
			NkVector<uint32> sel;
			for (uint32 i = 0; i < (uint32)verts.Size(); ++i) {
				if (!verts[i].sel)
					continue;
				const uint32 cc = (i < (uint32)canon.Size()) ? canon[i] : i;
				if (taken.Find(cc))
					continue;
				taken.InsertOrAssign(cc, (uint8)1);
				sel.PushBack(i);
			}
			if (sel.Size() != 2)
				return false;
			return AddWireEdge(sel[0], sel[1]) != NK_EM_INVALID;
		}

		bool NkEditMesh::MakeFaceFromSelected() {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// MATERIAU PAR FACE : transporte a travers le round-trip. Les faces conservees gardent leur index.
			NkVector<NkEditMesh::FaceAttrib> fm;
			ToPolygons(pv, fs, fv, &fm);
			// ── 1) UN SEUL REPRÉSENTANT PAR SOMMET TOPOLOGIQUE ──────────────────────
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const uint32 ncv = (uint32)canon.Size();
			NkHashMap<uint32, uint8> taken;
			NkVector<uint32> sel;
			for (uint32 i = 0; i < (uint32)verts.Size() && i < (uint32)pv.Size(); i++) {
				if (!verts[i].sel)
					continue;
				const uint32 c = (i < ncv) ? canon[i] : i;
				if (taken.Find(c) != nullptr)
					continue; // copie coïncidente du même sommet -> ignorée
				taken.InsertOrAssign(c, (uint8)1);
				sel.PushBack(i);
			}
			if (sel.Size() < 3)
				return false;
			// ── 2) ORDRE DU CONTOUR : tri angulaire dans le plan de meilleur ajustement ──
			const uint32 sn = (uint32)sel.Size();
			NkVec3f ctr{0.f, 0.f, 0.f};
			for (uint32 k = 0; k < sn; k++)
				ctr = ctr + pv[sel[k]].pos;
			ctr = ctr * (1.f / (float32)sn);
			// Normale du plan = le plus GRAND produit vectoriel trouvé entre deux rayons
			// (robuste : une somme « à la Newell » sur un ordre arbitraire peut s'annuler).
			// Recherche bornée aux 32 premiers sommets -> coût constant même sur « tout
			// sélectionner puis F ».
			NkVec3f nrm{0.f, 0.f, 0.f};
			float32 bestA = 0.f;
			const uint32 scan = (sn > 32u) ? 32u : sn;
			for (uint32 a = 0; a < scan; a++)
				for (uint32 b = a + 1; b < scan; b++) {
					const NkVec3f cr = (pv[sel[a]].pos - ctr).Cross(pv[sel[b]].pos - ctr);
					const float32 l = cr.Len();
					if (l > bestA) {
						bestA = l;
						nrm = cr;
					}
				}
			if (bestA > 1e-12f) {
				nrm = nrm * (1.f / bestA);
				// Base orthonormée du plan.
				NkVec3f u = pv[sel[0]].pos - ctr;
				u = u - nrm * u.Dot(nrm);
				float32 ul = u.Len();
				if (ul < 1e-8f) { // le 1er sommet est au barycentre : on prend n'importe quel axe
					u = (fabsf(nrm.x) < 0.9f) ? NkVec3f{1.f, 0.f, 0.f} : NkVec3f{0.f, 1.f, 0.f};
					u = u - nrm * u.Dot(nrm);
					ul = u.Len();
				}
				if (ul > 1e-8f) {
					u = u * (1.f / ul);
					const NkVec3f v = nrm.Cross(u);
					// Tri par insertion sur l'angle (zéro STL, zéro allocation).
					NkVector<float32> ang;
					ang.Resize(sn);
					for (uint32 k = 0; k < sn; k++) {
						const NkVec3f d = pv[sel[k]].pos - ctr;
						ang[k] = atan2f(d.Dot(v), d.Dot(u));
					}
					for (uint32 k = 1; k < sn; k++) {
						const float32 av = ang[k];
						const uint32 iv = sel[k];
						uint32 j = k;
						while (j > 0 && ang[j - 1] > av) {
							ang[j] = ang[j - 1];
							sel[j] = sel[j - 1];
							j--;
						}
						ang[j] = av;
						sel[j] = iv;
					}
				}
			}
			for (uint32 k = 0; k < (uint32)sel.Size(); k++)
				fv.PushBack(sel[k]);
			fs.PushBack((uint32)fv.Size());
			// LA FACE NEUVE N'HERITE DE PERSONNE : elle n'a pas de face mere, donc
			// slot 0. Son entree reste OBLIGATOIRE pour que `fm` suive `fs`.
			fm.PushBack(NkEditMesh::FaceAttrib{});
			NkVector<uint8> keep;
			keep.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)keep.Size(); i++)
				keep[i] = (i < (uint32)verts.Size() ? verts[i].sel : (uint8)0);
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), fs.Data(), (uint32)fs.Size() - 1, fv.Data(),
							  fm.Data());
			ApplyVertSel(keep);
			return true;
		}

		// SUBDIVIDE (Catmull-Clark) : chaque face sélectionnée -> n sous-quads (centre de
		// face + milieux d'arête PARTAGÉS). p.cuts itère la passe. Rien de sélectionné => TOUT.
		bool NkEditMesh::SubdivideSelectedFaces(const NkSubdivideParams &p) {
			bool changed = false;
			const int32 cuts = (p.cuts < 1) ? 1 : p.cuts;
			for (int32 c = 0; c < cuts; c++) {
				if (SubdivideSelectedOnce())
					changed = true;
				else
					break;
			}
			return changed;
		}

		bool NkEditMesh::SubdivideSelectedOnce() {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// MATERIAU PAR FACE : recupere l'index de chaque face SOURCE, et le repose
			// sur chaque face FILLE. Sans ce transport, BuildFromPolygons reconstruit des
			// Face neuves et toutes retombent sur le slot 0 -- mesure prise avant le
			// correctif : un cube dont deux faces portaient le slot 1 tombait a
			// « slot1 2 -> 0 » des la premiere subdivision.
			NkVector<NkEditMesh::FaceAttrib> fm;
			NkVector<NkEditMesh::FaceAttrib> nfm;
			ToPolygons(pv, fs, fv, &fm);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			if (fc == 0)
				return false;
			NkVector<uint8> faceSel;
			faceSel.Resize(fc);
			int32 selCount = 0;
			for (uint32 f = 0; f < fc; f++) {
				bool s = PolyFaceSelected(fv, fs[f], fs[f + 1]);
				faceSel[f] = s ? 1 : 0;
				if (s)
					selCount++;
			}
			if (selCount == 0) {
				for (uint32 f = 0; f < fc; f++)
					faceSel[f] = 1;
				selCount = (int32)fc;
			} // rien -> TOUT le modèle
			NkHashMap<uint64, uint32> emid;
			auto lerp = [&](uint32 a, uint32 b) {
				NkVertex3D r = pv[a];
				r.pos = (pv[a].pos + pv[b].pos) * 0.5f;
				r.uv = (pv[a].uv + pv[b].uv) * 0.5f;
				return r;
			};
			auto edgeMid = [&](uint32 a, uint32 b) -> uint32 {
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				uint64 key = ((uint64)lo << 32) | hi;
				uint32 *q = emid.Find(key);
				if (q)
					return *q;
				uint32 idx = (uint32)pv.Size();
				pv.PushBack(lerp(a, b));
				emid.InsertOrAssign(key, idx);
				return idx;
			};
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint8> vsel;
			vsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)vsel.Size(); i++)
				vsel[i] = 0;
			for (uint32 f = 0; f < fc; f++) {
				if (faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; k++)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
				nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
			}
			for (uint32 f = 0; f < fc; f++) {
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				if (n < 3) {
					for (uint32 k = s; k < e; k++)
						nfv.PushBack(fv[k]);
					nfs.PushBack((uint32)nfv.Size());
					// Chaque fille herite de sa mere : regle de Blender, et la seule qui
					// garde l'affectation stable sur plusieurs subdivisions de suite.
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					continue;
				}
				NkVertex3D ctr{};
				NkVec3f cp{0, 0, 0};
				NkVec2f cuv{0, 0};
				for (uint32 k = s; k < e; k++) {
					cp = cp + pv[fv[k]].pos;
					cuv = cuv + pv[fv[k]].uv;
				}
				ctr = pv[fv[s]];
				ctr.pos = cp * (1.f / (float32)n);
				ctr.uv = cuv * (1.f / (float32)n);
				uint32 cidx = (uint32)pv.Size();
				pv.PushBack(ctr);
				if ((uint32)vsel.Size() <= cidx)
					vsel.Resize(cidx + 1);
				for (uint32 k = 0; k < n; k++) {
					uint32 v0 = fv[s + k], v1 = fv[s + (k + 1) % n], vp = fv[s + (k + n - 1) % n];
					uint32 m1 = edgeMid(v0, v1), m0 = edgeMid(vp, v0);
					{
						uint32 mx = cidx;
						if (m1 > mx)
							mx = m1;
						if (m0 > mx)
							mx = m0;
						if ((uint32)vsel.Size() <= mx)
							vsel.Resize(mx + 1);
					}
					nfv.PushBack(v0);
					nfv.PushBack(m1);
					nfv.PushBack(cidx);
					nfv.PushBack(m0);
					nfs.PushBack((uint32)nfv.Size());
					// Chaque fille herite de sa mere : regle de Blender, et la seule qui
					// garde l'affectation stable sur plusieurs subdivisions de suite.
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					vsel[cidx] = 1;
					vsel[m1] = 1;
					vsel[m0] = 1;
					if (v0 < (uint32)vsel.Size())
						vsel[v0] = 1;
				}
			}
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
					  nfm.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// LOOP CUT : depuis une ARÊTE sélectionnée, traverse l'ANNEAU de quads et insère
		// p.cuts boucles d'arêtes RÉGULIÈREMENT ESPACÉES (sommets PARTAGÉS entre quads
		// voisins de l'anneau). Maillages quad (façon Blender, Ctrl+R).
		// Limite assumée : pas d'aperçu au survol ni de « slide » modal — les coupes sont
		// posées aux fractions k/(cuts+1) de l'anneau, comme un Ctrl+R validé sans slide.
		bool NkEditMesh::LoopCutFromSelectedEdge(const NkLoopCutParams &p) {
			const int32 cuts = (p.cuts < 1) ? 1 : ((p.cuts > 32) ? 32 : p.cuts);
			// SLIDE (edge slide de Blender) : glisse les boucles insérées le long de l'anneau.
			// 0 = position médiane (comportement historique, strictement inchangé).
			const float32 slide = (p.slide < -1.f) ? -1.f : ((p.slide > 1.f) ? 1.f : p.slide);
			// Arête de départ = 1re demi-arête vivante dont les 2 extrémités sont sélectionnées.
			NkEmId h0 = NK_EM_INVALID;
			for (uint32 h = 0; h < (uint32)hedges.Size(); ++h) {
				if (!hedges[h].alive)
					continue;
				uint32 o = hedges[h].origin, d = hedges[hedges[h].next].origin;
				if (o < (uint32)verts.Size() && d < (uint32)verts.Size() && verts[o].sel && verts[d].sel) {
					h0 = h;
					break;
				}
			}
			if (h0 == NK_EM_INVALID)
				return false;
			// L'anneau est identifié sur l'IDENTITÉ TOPOLOGIQUE (sommets soudés) : deux faces
			// voisines n'utilisent pas les mêmes INDICES pour l'arête qu'elles partagent
			// (attributs par coin), mais bien le même représentant canonique.
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			const uint32 ncv = (uint32)canon.Size();
			auto CV = [&](uint32 v) -> uint32 { return (v < ncv) ? canon[v] : v; };
			// Valeur stockée = ORIENTATION de l'arête dans l'anneau : 1 => le sens « positif »
			// (celui du SLIDE) va de `lo` vers `hi` ; 2 => il va de `hi` vers `lo`.
			// Nécessaire parce que l'ordre canonique lo->hi est arbitraire : sans cette
			// mémoire, une arête sur deux glisserait à contresens et la boucle se tordrait.
			NkHashMap<uint64, uint8> ring;
			auto addE = [&](uint32 a0, uint32 b0, bool posIsAtoB) {
				const uint32 a = CV(a0), b = CV(b0);
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				const bool posLoHi = posIsAtoB ? (a < b) : (b < a);
				ring.InsertOrAssign(((uint64)lo << 32) | hi, (uint8)(posLoHi ? 1 : 2));
			};
			NkEmId h = h0;
			uint32 guard = 0;
			do {
				uint32 o = hedges[h].origin, d = hedges[hedges[h].next].origin;
				// Convention : le sens positif suit la demi-arête courante (o -> d). Le quad
				// étant parcouru q0->q1->q2->q3 avec h = (q0,q1) et hOpp = (q2,q3), le sommet
				// de coupe de `h` près de q0 fait face à celui de `hOpp` près de q3 : le sens
				// positif sur hOpp est donc q3 -> q2, soit l'INVERSE de (origin -> dest).
				addE(o, d, true);
				if (FaceSize(hedges[h].face) != 4)
					break;								   // anneau uniquement à travers des quads
				NkEmId hOpp = hedges[hedges[h].next].next; // arête opposée du quad
				addE(hedges[hOpp].origin, hedges[hedges[hOpp].next].origin, false);
				NkEmId tw = hedges[hOpp].twin;
				if (tw == NK_EM_INVALID)
					break; // bord -> anneau ouvert
				h = tw;
				// (la prochaine itération réécrit l'orientation de hOpp via son twin, avec
				//  EXACTEMENT la même valeur : twin(hOpp) va bien de q3 vers q2.)
			} while (h != h0 && ++guard < 100000u);
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// MATERIAU PAR FACE : transporte a travers le round-trip. Chaque bande issue d un quad herite du quad d origine.
			NkVector<NkEditMesh::FaceAttrib> fm;
			NkVector<NkEditMesh::FaceAttrib> nfm;
			ToPolygons(pv, fs, fv, &fm);
			auto isRing = [&](uint32 a0, uint32 b0) -> bool {
				const uint32 a = CV(a0), b = CV(b0);
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				return ring.Find(((uint64)lo << 32) | hi) != nullptr;
			};
			// Chaque arête de l'anneau reçoit `cuts` sommets, créés d'un bloc et INDEXÉS
			// DE `lo` VERS `hi` (ordre canonique) -> les 2 quads voisins d'une même arête
			// retrouvent EXACTEMENT les mêmes sommets : la boucle est soudée, pas dédoublée.
			NkHashMap<uint64, uint32> emid;
			auto edgeCutBase = [&](uint32 a0, uint32 b0) -> uint32 {
				// Clé CANONIQUE : les deux faces voisines qui partagent l'arête retrouvent les
				// MÊMES sommets de coupe -> la boucle insérée est soudée, pas dédoublée.
				const uint32 a = CV(a0), b = CV(b0);
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				uint64 key = ((uint64)lo << 32) | hi;
				uint32 *q = emid.Find(key);
				if (q)
					return *q;
				// SLIDE : le sens positif de CETTE arête vient de la table `ring` (établie en
				// parcourant l'anneau). `sp` est le glissement ramené dans le repère lo->hi.
				const uint8 *ro = ring.Find(key);
				const bool posLoHi = (ro == nullptr) || (*ro != 2u);
				const float32 sp = posLoHi ? slide : -slide;
				const uint32 base = (uint32)pv.Size();
				for (int32 c = 0; c < cuts; c++) {
					float32 t = (float32)(c + 1) / (float32)(cuts + 1);
					// Glissement vers la boucle bordante `hi` (sp > 0) ou `lo` (sp < 0) :
					// t' = t + |sp| * (cible - t) -> à |sp| = 1 toutes les coupes se rabattent
					// exactement sur la boucle visée, comme le edge slide de Blender.
					if (sp != 0.f) {
						const float32 target = (sp > 0.f) ? 1.f : 0.f;
						const float32 a2 = (sp < 0.f) ? -sp : sp;
						t = t + a2 * (target - t);
						t = (t < 0.001f) ? 0.001f : ((t > 0.999f) ? 0.999f : t);
					}
					NkVertex3D nv = pv[lo];
					nv.pos = pv[lo].pos + (pv[hi].pos - pv[lo].pos) * t;
					nv.uv = pv[lo].uv + (pv[hi].uv - pv[lo].uv) * t;
					pv.PushBack(nv);
				}
				emid.InsertOrAssign(key, base);
				return base;
			};
			// Les `cuts` sommets de l'arête (a,b) RANGÉS DANS LE SENS a -> b.
			auto edgeCutsDir = [&](uint32 a, uint32 b, NkVector<uint32> &out) {
				out.Clear();
				const uint32 base = edgeCutBase(a, b);
				const bool fwd = (CV(a) < CV(b)); // les sommets sont stockés de lo vers hi
				for (int32 c = 0; c < cuts; c++)
					out.PushBack(base + (uint32)(fwd ? c : (cuts - 1 - c)));
			};
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint8> vsel;
			vsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)vsel.Size(); i++)
				vsel[i] = 0;
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			bool changed = false;
			for (uint32 f = 0; f < fc; f++) {
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				int32 re0 = -1, re1 = -1;
				if (n == 4) {
					for (uint32 k = 0; k < 4; k++)
						if (isRing(fv[s + k], fv[s + (k + 1) % 4])) {
							if (re0 < 0)
								re0 = (int32)k;
							else
								re1 = (int32)k;
						}
				}
				if (n == 4 && re0 >= 0 && re1 >= 0 && (re1 - re0) == 2) { // 2 arêtes opposées
					uint32 k0 = (uint32)re0;
					uint32 q0 = fv[s + k0], q1 = fv[s + (k0 + 1) % 4], q2 = fv[s + (k0 + 2) % 4],
						   q3 = fv[s + (k0 + 3) % 4];
					// A = coupes de l'arête (q0,q1) dans le sens q0->q1 ;
					// B = coupes de l'arête opposée (q2,q3) dans le sens q2->q3.
					// La boucle du quad étant q0->q1->q2->q3, A[i] fait face à B[cuts-1-i].
					NkVector<uint32> A, B;
					edgeCutsDir(q0, q1, A);
					edgeCutsDir(q2, q3, B);
					uint32 mx = 0;
					for (int32 c = 0; c < cuts; c++) {
						if (A[(uint32)c] > mx)
							mx = A[(uint32)c];
						if (B[(uint32)c] > mx)
							mx = B[(uint32)c];
					}
					if ((uint32)vsel.Size() <= mx)
						vsel.Resize(mx + 1);
					for (int32 c = 0; c < cuts; c++) {
						vsel[A[(uint32)c]] = 1;
						vsel[B[(uint32)c]] = 1;
					}
					changed = true;
					// Bande 0 : q0, A0, B(cuts-1), q3
					nfv.PushBack(q0);
					nfv.PushBack(A[0]);
					nfv.PushBack(B[(uint32)(cuts - 1)]);
					nfv.PushBack(q3);
					nfs.PushBack((uint32)nfv.Size());
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					// Bandes intermédiaires : Ai, Ai+1, B(cuts-2-i), B(cuts-1-i)
					for (int32 c = 0; c + 1 < cuts; c++) {
						nfv.PushBack(A[(uint32)c]);
						nfv.PushBack(A[(uint32)(c + 1)]);
						nfv.PushBack(B[(uint32)(cuts - 2 - c)]);
						nfv.PushBack(B[(uint32)(cuts - 1 - c)]);
						nfs.PushBack((uint32)nfv.Size());
						nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					}
					// Bande finale : A(cuts-1), q1, q2, B0
					nfv.PushBack(A[(uint32)(cuts - 1)]);
					nfv.PushBack(q1);
					nfv.PushBack(q2);
					nfv.PushBack(B[0]);
					nfs.PushBack((uint32)nfv.Size());
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
				} else {
					for (uint32 k = s; k < e; k++)
						nfv.PushBack(fv[k]);
					nfs.PushBack((uint32)nfv.Size());
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
				}
			}
			if (!changed)
				return false;
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
							  nfm.Data());
			ApplyVertSel(vsel);
			return true;
		}

		// =====================================================================
		// OUTILS COMMUNS AUX OPÉRATIONS « TOPOLOGIE SOUDÉE » (bevel, inset, split, spin)
		// ---------------------------------------------------------------------
		// Ces opérations ont besoin d'une VRAIE adjacence : savoir quelles faces bordent
		// une arête, tourner autour d'un sommet. Or les primitives/imports dupliquent les
		// coins PAR FACE (cube = 24 sommets pour 8 positions) : dans l'espace des INDICES
		// bruts, deux faces voisines ne partagent aucun sommet. On travaille donc sur une
		// copie SOUDÉE (un sommet par position, cf. BuildVertexMerge) — exactement le
		// modèle Blender : maillage soudé, attributs portés par les coins.
		// =====================================================================
		static inline NkVec3f EM_Norm(const NkVec3f &v) {
			const float32 l = v.Len();
			return (l > 1e-8f) ? v * (1.f / l) : NkVec3f{0.f, 0.f, 0.f};
		}

		// Diagonale de la boîte englobante = ÉCHELLE du maillage (offsets AUTO).
		static float32 EM_BBoxDiag(const NkVector<NkVertex3D> &pv) {
			if (pv.Empty())
				return 0.f;
			NkVec3f mn = pv[0].pos, mx = pv[0].pos;
			for (uint32 i = 1; i < (uint32)pv.Size(); ++i) {
				const NkVec3f q = pv[i].pos;
				mn.x = (q.x < mn.x) ? q.x : mn.x;
				mn.y = (q.y < mn.y) ? q.y : mn.y;
				mn.z = (q.z < mn.z) ? q.z : mn.z;
				mx.x = (q.x > mx.x) ? q.x : mx.x;
				mx.y = (q.y > mx.y) ? q.y : mx.y;
				mx.z = (q.z > mx.z) ? q.z : mx.z;
			}
			return (mx - mn).Len();
		}

		// Polygones SOUDÉS : un sommet par position (représentant du groupe coïncident).
		// vsel = sélection soudée (OU logique du groupe) ; wmap[i] = indice soudé de i.
		// `ofm`, quand il est fourni, recoit l'index de materiau de chaque face
		// EMISE -- donc apres le saut des faces effondrees par la soudure. C'est
		// pour ca qu'il est rempli au meme endroit que `fs`, et pas dans une boucle
		// separee : les deux tableaux doivent sauter les memes faces.
		static void EM_ToWeldedPolygons(const NkEditMesh &m, NkVector<NkVertex3D> &pv, NkVector<uint32> &fs,
										NkVector<uint32> &fv, NkVector<uint8> &vsel, NkVector<uint32> &wmap,
										NkVector<NkEditMesh::FaceAttrib> *ofm = nullptr) {
			NkVector<NkVertex3D> rv;
			NkVector<uint32> rfs, rfv;
			NkVector<NkEditMesh::FaceAttrib> rfm;
			m.ToPolygons(rv, rfs, rfv, ofm ? &rfm : nullptr);
			if (ofm)
				ofm->Clear();
			NkVector<uint32> canon;
			m.BuildVertexMerge(canon);
			const uint32 n = (uint32)rv.Size();
			NkVector<int32> newIdx;
			newIdx.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				newIdx[i] = -1;
			pv.Clear();
			vsel.Clear();
			wmap.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				const uint32 c = (i < (uint32)canon.Size()) ? canon[i] : i;
				if (newIdx[c] < 0) {
					newIdx[c] = (int32)pv.Size();
					pv.PushBack(rv[c]);
					vsel.PushBack(0);
				}
				wmap[i] = (uint32)newIdx[c];
				if (i < m.VertCount() && m.verts[i].sel)
					vsel[wmap[i]] = 1;
			}
			fs.Clear();
			fv.Clear();
			fs.PushBack(0);
			const uint32 fc = (rfs.Size() > 0) ? (uint32)rfs.Size() - 1 : 0;
			for (uint32 f = 0; f < fc; ++f) {
				const uint32 s = rfs[f], e = rfs[f + 1];
				const uint32 st = (uint32)fv.Size();
				const bool wire = (e - s) < 3u; // arête FIL : à préserver telle quelle
				for (uint32 k = s; k < e; ++k) {
					const uint32 w = wmap[rfv[k]];
					if ((uint32)fv.Size() > st && fv[(uint32)fv.Size() - 1] == w)
						continue; // doublon consécutif né de la soudure
					fv.PushBack(w);
				}
				while ((uint32)fv.Size() > st + 1u && fv[(uint32)fv.Size() - 1] == fv[st])
					fv.Resize((uint32)fv.Size() - 1);
				const uint32 got = (uint32)fv.Size() - st;
				if (got < 2u || (!wire && got < 3u)) {
					fv.Resize(st);
					continue;
				} // face effondrée
				fs.PushBack((uint32)fv.Size());
				if (ofm)
					ofm->PushBack(f < (uint32)rfm.Size() ? rfm[f] : NkEditMesh::FaceAttrib{});
			}
		}

		// =====================================================================
		// BEVEL / CHANFREIN (Ctrl+B · Ctrl+Shift+B) — algorithme PAR COIN
		// ---------------------------------------------------------------------
		// Idée : chaque COIN (face, sommet) est remplacé par 1 ou 2 points, selon que ses
		// deux arêtes sont chanfreinées ou non :
		//   • aucune des deux, sommet non touché      -> 1 point : le sommet lui-même ;
		//   • aucune des deux, sommet TOUCHÉ          -> 2 points, reculés le long des deux
		//                                                arêtes (c'est ce qui transforme la
		//                                                face « du bout » en n-gon) ;
		//   • une seule chanfreinée                   -> 1 point, reculé le long de l'AUTRE ;
		//   • les deux chanfreinées                   -> 1 point INTÉRIEUR à la face.
		// Les points « reculés le long d'une arête » sont MÉMORISÉS par clé (sommet, arête) :
		// les deux faces qui partagent une arête non chanfreinée obtiennent donc le MÊME
		// point -> aucune fissure. Ensuite :
		//   (a) chaque face d'origine est ré-émise avec ses coins remplacés ;
		//   (b) chaque arête chanfreinée engendre une BANDE de `segments` quads ;
		//   (c) chaque sommet touché engendre une face de RACCORD si l'anneau de points
		//       autour de lui n'est pas dégénéré (coin où >= 3 arêtes sont chanfreinées,
		//       bevel de sommet, ou bevel arrondi dont l'arc creuse le coin).
		// Le bevel de SOMMET est le MÊME code avec « aucune arête chanfreinée, sommets
		// sélectionnés touchés » : chaque face incidente gagne un point, l'anneau devient
		// la petite face de coin.
		// =====================================================================
		bool NkEditMesh::BevelSelected(const NkBevelParams &p, uint32 *outMaterialChanged) {
			if (outMaterialChanged)
				*outMaterialChanged = 0;
			NkVector<NkVertex3D> wv;
			NkVector<uint32> wfs, wfv;
			NkVector<uint8> wsel;
			NkVector<uint32> wmap;
			// Les attributs par face voyagent jusqu'au maillage soude : c'est LUI qui
			// porte l'adjacence dont les faces creees vont heriter.
			NkVector<NkEditMesh::FaceAttrib> wfa;
			EM_ToWeldedPolygons(*this, wv, wfs, wfv, wsel, wmap, &wfa);
			const uint32 wfc = (wfs.Size() > 0) ? (uint32)wfs.Size() - 1 : 0;
			if (wfc == 0)
				return false;
			NkEditMesh W;
			W.BuildFromPolygons(wv.Data(), (uint32)wv.Size(), wfs.Data(), wfc, wfv.Data(),
								(wfa.Size() == wfc) ? wfa.Data() : nullptr);
			uint32 perdus = 0;
			const uint32 NV = W.VertCount();
			for (uint32 i = 0; i < NV && i < (uint32)wsel.Size(); ++i)
				W.verts[i].sel = wsel[i];
			const uint32 HC = (uint32)W.hedges.Size();
			if (HC == 0 || NV == 0)
				return false;

			// prevOf[h] : demi-arête précédente dans la boucle de face. Le COIN « h » est
			// délimité par prevOf[h] (arête entrante) et h (arête sortante).
			NkVector<NkEmId> prevOf;
			prevOf.Resize(HC);
			for (uint32 i = 0; i < HC; ++i)
				prevOf[i] = NK_EM_INVALID;
			for (uint32 f = 0; f < (uint32)W.faces.Size(); ++f) {
				if (!W.faces[f].alive || W.faces[f].hedge == NK_EM_INVALID)
					continue;
				const NkEmId s = W.faces[f].hedge;
				NkEmId h = s;
				uint32 g = 0;
				do {
					const NkEmId nx = W.hedges[h].next;
					if (nx == NK_EM_INVALID)
						break;
					prevOf[nx] = h;
					h = nx;
				} while (h != s && ++g < 100000u);
			}

			auto dstOf = [&](NkEmId h) -> uint32 {
				const NkEmId nx = W.hedges[h].next;
				return (nx == NK_EM_INVALID) ? W.hedges[h].origin : W.hedges[nx].origin;
			};
			auto ekey = [](uint32 a, uint32 b) -> uint64 {
				const uint32 lo = (a < b) ? a : b, hi = (a < b) ? b : a;
				return ((uint64)lo << 32) | (uint64)hi;
			};

			// Arêtes CHANFREINÉES (les deux extrémités sélectionnées + un jumeau) et
			// sommets TOUCHÉS (mode sommet : simplement les sommets sélectionnés).
			NkHashMap<uint64, uint8> selE;
			NkVector<uint8> touched;
			touched.Resize(NV);
			for (uint32 i = 0; i < NV; ++i)
				touched[i] = 0;
			if (p.vertexOnly) {
				for (uint32 i = 0; i < NV; ++i)
					touched[i] = W.verts[i].sel ? (uint8)1 : (uint8)0;
			} else {
				for (uint32 h = 0; h < HC; ++h) {
					if (!W.hedges[h].alive || W.hedges[h].twin == NK_EM_INVALID)
						continue; // arête de BORD -> non chanfreinable (limite assumée)
					const uint32 a = W.hedges[h].origin, b = dstOf((NkEmId)h);
					if (a == b || a >= NV || b >= NV)
						continue;
					if (!W.verts[a].sel || !W.verts[b].sel)
						continue;
					selE.InsertOrAssign(ekey(a, b), (uint8)1);
					touched[a] = 1;
					touched[b] = 1;
				}
			}
			bool anyTouched = false;
			for (uint32 i = 0; i < NV && !anyTouched; ++i)
				anyTouched = (touched[i] != 0);
			if (!anyTouched)
				return false;

			float32 off = p.offset;
			if (off <= 0.f)
				off = EM_BBoxDiag(wv) * 0.06f;
			if (off <= 1e-7f)
				return false;
			int32 seg = (p.segments < 1) ? 1 : ((p.segments > 16) ? 16 : p.segments);
			if (p.vertexOnly)
				seg = 1; // un bevel de sommet produit UNE face de coin

			// ── Nuage de points de sortie (mémorisation par clé -> pas de fissure) ──
			NkVector<NkVertex3D> np;
			NkVector<uint8> nsel;
			NkHashMap<uint64, uint32> origPt, edgePt;
			auto pushPt = [&](const NkVertex3D &v, uint8 s) -> uint32 {
				const uint32 id = (uint32)np.Size();
				np.PushBack(v);
				nsel.PushBack(s);
				return id;
			};
			auto tAlong = [&](uint32 v, uint32 w) -> float32 {
				const float32 lim = (W.verts[w].pos - W.verts[v].pos).Len() * 0.45f;
				return (off > lim) ? lim : off; // jamais plus de 45 % de l'arête
			};
			auto getOrig = [&](uint32 v) -> uint32 {
				uint32 *q = origPt.Find((uint64)v);
				if (q)
					return *q;
				const uint32 id = pushPt(wv[v], W.verts[v].sel);
				origPt.InsertOrAssign((uint64)v, id);
				return id;
			};
			auto getEdgePt = [&](uint32 v, uint32 w) -> uint32 {
				const uint64 k = ((uint64)v << 32) | (uint64)w;
				uint32 *q = edgePt.Find(k);
				if (q)
					return *q;
				NkVertex3D nv = wv[v];
				nv.pos = W.verts[v].pos + EM_Norm(W.verts[w].pos - W.verts[v].pos) * tAlong(v, w);
				const uint32 id = pushPt(nv, (uint8)1);
				edgePt.InsertOrAssign(k, id);
				return id;
			};

			NkVector<uint32> ptPrev, ptNext;
			ptPrev.Resize(HC);
			ptNext.Resize(HC);
			for (uint32 h = 0; h < HC; ++h) {
				ptPrev[h] = 0;
				ptNext[h] = 0;
			}
			for (uint32 h = 0; h < HC; ++h) {
				if (!W.hedges[h].alive || W.hedges[h].face == NK_EM_INVALID)
					continue;
				const uint32 v = W.hedges[h].origin;
				const NkEmId hp = prevOf[h];
				if (v >= NV || hp == NK_EM_INVALID)
					continue;
				const uint32 pv2 = W.hedges[hp].origin, nv2 = dstOf((NkEmId)h);
				if (!touched[v]) {
					const uint32 id = getOrig(v);
					ptPrev[h] = id;
					ptNext[h] = id;
					continue;
				}
				const bool sp = (selE.Find(ekey(pv2, v)) != nullptr);
				const bool sn = (selE.Find(ekey(v, nv2)) != nullptr);
				if (!sp && !sn) {
					ptPrev[h] = getEdgePt(v, pv2);
					ptNext[h] = getEdgePt(v, nv2);
				} else if (sp && !sn) {
					const uint32 id = getEdgePt(v, nv2);
					ptPrev[h] = id;
					ptNext[h] = id;
				} else if (!sp && sn) {
					const uint32 id = getEdgePt(v, pv2);
					ptPrev[h] = id;
					ptNext[h] = id;
				} else {
					// Les deux arêtes du coin reculent : le point est l'INTERSECTION des deux
					// droites décalées. Pour un coin droit, cela vaut v + t1*u1 + t2*u2.
					const NkVec3f u1 = EM_Norm(W.verts[pv2].pos - W.verts[v].pos);
					const NkVec3f u2 = EM_Norm(W.verts[nv2].pos - W.verts[v].pos);
					float32 s = u1.Cross(u2).Len(); // sin de l'angle du coin
					if (s < 0.2f)
						s = 0.2f; // coin très aigu : on borne l'étirement
					NkVertex3D nvx = wv[v];
					nvx.pos = W.verts[v].pos + (u1 * tAlong(v, pv2) + u2 * tAlong(v, nv2)) * (1.f / s);
					const uint32 id = pushPt(nvx, (uint8)1);
					ptPrev[h] = id;
					ptNext[h] = id;
				}
			}

			// ── ARCS (segments > 1) : points intermédiaires du profil arrondi. Calculés
			// UNE SEULE FOIS par demi-arête chanfreinée, donc PARTAGÉS entre la bande et la
			// face de raccord -> pas de fissure. Slerp autour du sommet = arc de cercle.
			NkVector<int32> arcBase;
			arcBase.Resize(HC);
			for (uint32 h = 0; h < HC; ++h)
				arcBase[h] = -1;
			NkVector<uint32> arcData;
			if (seg > 1) {
				for (uint32 h = 0; h < HC; ++h) {
					if (!W.hedges[h].alive || W.hedges[h].face == NK_EM_INVALID)
						continue;
					const NkEmId tw = W.hedges[h].twin;
					if (tw == NK_EM_INVALID)
						continue;
					const uint32 v = W.hedges[h].origin, w2 = dstOf((NkEmId)h);
					if (!selE.Find(ekey(v, w2)))
						continue;
					const NkEmId rot = W.hedges[tw].next; // demi-arête suivante autour de v
					if (rot == NK_EM_INVALID)
						continue;
					const uint32 iA = ptNext[h], iB = ptPrev[rot];
					// CENTRE de l'arc : surtout PAS le sommet lui-même (le profil bomberait
					// HORS de la surface). C'est le « coin intérieur » : le point d'où les deux
					// extrémités du profil sont à la MÊME distance = la largeur du chanfrein.
					// Les deux extrémités valent v + Σ t*u sur des sous-ensembles d'arêtes ; leur
					// centre commun est v + Σ t*u sur l'UNION, soit P0 + P1 - v, moins la part
					// COMMUNE (la direction de l'arête chanfreinée) quand les deux extrémités
					// sont des points intérieurs de face.
					const NkEmId hp2 = prevOf[h];
					const bool p0Int = (hp2 != NK_EM_INVALID) && (selE.Find(ekey(W.hedges[hp2].origin, v)) != nullptr);
					const bool p1Int = (selE.Find(ekey(v, dstOf(rot))) != nullptr);
					NkVec3f c = np[iA].pos + np[iB].pos - W.verts[v].pos;
					if (p0Int && p1Int)
						c = c - EM_Norm(W.verts[w2].pos - W.verts[v].pos) * tAlong(v, w2);
					const NkVec3f r0 = np[iA].pos - c, r1 = np[iB].pos - c;
					const float32 l0 = r0.Len(), l1 = r1.Len();
					const NkVec3f e0 = EM_Norm(r0), e1 = EM_Norm(r1);
					NkVec3f ax = e0.Cross(e1);
					const float32 sn = ax.Len();
					float32 cs = e0.Dot(e1);
					cs = (cs > 1.f) ? 1.f : ((cs < -1.f) ? -1.f : cs);
					const bool arcOk = (l0 > 1e-6f && l1 > 1e-6f && sn > 1e-5f);
					if (arcOk)
						ax = ax * (1.f / sn);
					const float32 ang = atan2f(sn, cs);
					arcBase[h] = (int32)arcData.Size();
					for (int32 j = 1; j < seg; ++j) {
						const float32 t = (float32)j / (float32)seg;
						NkVertex3D nvx = np[iA];
						if (arcOk) { // Rodrigues : rotation de e0 autour de ax
							const float32 a = ang * t, ca = cosf(a), sa = sinf(a);
							const NkVec3f er = e0 * ca + ax.Cross(e0) * sa + ax * (ax.Dot(e0) * (1.f - ca));
							nvx.pos = c + er * (l0 + (l1 - l0) * t);
						} else {
							nvx.pos = np[iA].pos + (np[iB].pos - np[iA].pos) * t;
						}
						arcData.PushBack(pushPt(nvx, (uint8)1));
					}
				}
			}

			// ── Faces de sortie ──────────────────────────────────────────────
			NkVector<uint32> nfs, nfv;
			// UNE entree d'attribut par face EMISE. Poussee dans `endFace`, sous la
			// meme condition d'acceptation que `nfs` : une face refusee (anneau
			// degenere) ne doit pas laisser d'attribut orphelin derriere elle, sinon
			// TOUTES les faces suivantes changent de couleur en silence.
			NkVector<NkEditMesh::FaceAttrib> nfa;
			nfs.PushBack(0);
			auto pushCorner = [&](uint32 st, uint32 id) {
				if ((uint32)nfv.Size() > st && nfv[(uint32)nfv.Size() - 1] == id)
					return; // doublon consécutif
				nfv.PushBack(id);
			};
			auto endFace = [&](uint32 st, uint32 minN, const NkEditMesh::FaceAttrib &at) -> bool {
				while ((uint32)nfv.Size() > st + 1u && nfv[(uint32)nfv.Size() - 1] == nfv[st])
					nfv.Resize((uint32)nfv.Size() - 1);
				if ((uint32)nfv.Size() - st < minN) {
					nfv.Resize(st); // anneau dégénéré -> pas de face
					return false;
				}
				nfs.PushBack((uint32)nfv.Size());
				nfa.PushBack(at);
				return true;
			};
			// Attribut d'une face incidente de `W`, sous une forme directement
			// utilisable par la regle d'heritage.
			auto attribDe = [&](NkEmId f) -> NkEditMesh::FaceAttrib {
				NkEditMesh::FaceAttrib a;
				if (f != NK_EM_INVALID && f < (NkEmId)W.faces.Size() && W.faces[f].alive) {
					a.material = W.faces[f].material;
					a.smooth = W.faces[f].smooth;
				}
				return a;
			};
			// (a) faces d'origine, coins remplacés
			for (uint32 f = 0; f < (uint32)W.faces.Size(); ++f) {
				if (!W.faces[f].alive || W.faces[f].hedge == NK_EM_INVALID)
					continue;
				const uint32 minN = (W.FaceSize((NkEmId)f) >= 3u) ? 3u : 2u; // 2 = arête FIL
				const NkEmId s = W.faces[f].hedge;
				const uint32 st = (uint32)nfv.Size();
				NkEmId h = s;
				uint32 g = 0;
				do {
					pushCorner(st, ptPrev[h]);
					pushCorner(st, ptNext[h]);
					h = W.hedges[h].next;
				} while (h != s && h != NK_EM_INVALID && ++g < 100000u);
				// (a) CETTE FACE A UNE MERE : c'est la face d'origine elle-meme, dont on
				// n'a remplace que les coins. Aucune regle a arbitrer, aucune perte.
				endFace(st, minN, attribDe((NkEmId)f));
			}
			// (b) BANDES de chanfrein (une par arête chanfreinée, `seg` quads chacune)
			NkVector<uint32> A, B;
			for (uint32 h = 0; h < HC; ++h) {
				if (!W.hedges[h].alive || W.hedges[h].face == NK_EM_INVALID)
					continue;
				const NkEmId tw = W.hedges[h].twin;
				if (tw == NK_EM_INVALID || (uint32)tw < h)
					continue; // une seule fois par arête
				const uint32 a = W.hedges[h].origin, b = dstOf((NkEmId)h);
				if (!selE.Find(ekey(a, b)))
					continue;
				const NkEmId rotA = W.hedges[tw].next, rotB = W.hedges[h].next;
				if (rotA == NK_EM_INVALID || rotB == NK_EM_INVALID)
					continue;
				A.Clear();
				B.Clear();
				A.PushBack(ptNext[h]); // côté A : a0 -> … -> a1
				if (arcBase[h] >= 0)
					for (int32 j = 0; j < seg - 1; ++j)
						A.PushBack(arcData[(uint32)arcBase[h] + (uint32)j]);
				A.PushBack(ptPrev[rotA]);
				B.PushBack(ptNext[tw]); // côté B : b1 -> … -> b0 (sens du tour autour de b)
				if (arcBase[tw] >= 0)
					for (int32 j = 0; j < seg - 1; ++j)
						B.PushBack(arcData[(uint32)arcBase[tw] + (uint32)j]);
				B.PushBack(ptPrev[rotB]);
				if ((int32)A.Size() != seg + 1 || (int32)B.Size() != seg + 1)
					continue;
				// (b) LA BANDE N'A PAS DE MERE. Ses deux voisines sont les faces de part
				// et d'autre de l'arete chanfreinee ; le poids de chacune est la longueur
				// du contour partage LE LONG de cette arete — cote face(h) entre A[0] et
				// B[seg], cote face(tw) entre A[seg] et B[0]. Les deux different des que
				// les reculs des deux cotes different, ce qui est le cas general.
				//
				// ⚠ UN SEUL ATTRIBUT POUR TOUTE LA BANDE, calcule ici et non par quad :
				// seuls le premier et le dernier quad touchent une voisine, ceux du
				// milieu ne touchent que l'arc. Leur donner un attribut « propre »
				// ferait apparaitre une couture de couleur au milieu d'un chanfrein,
				// la ou la geometrie est continue.
				NkEditMesh::FaceAttrib atBande;
				{
					const uint16 mats[2] = {W.faces[W.hedges[h].face].material,
											W.faces[W.hedges[tw].face].material};
					const uint8 sms[2] = {W.faces[W.hedges[h].face].smooth,
										  W.faces[W.hedges[tw].face].smooth};
					const float32 poids[2] = {(np[A[0]].pos - np[B[(uint32)seg]].pos).Len(),
											  (np[A[(uint32)seg]].pos - np[B[0]].pos).Len()};
					atBande = EM_AttribFromNeighbours(mats, sms, poids, 2u, &perdus);
				}
				for (int32 j = 0; j < seg; ++j) {
					const uint32 st = (uint32)nfv.Size();
					pushCorner(st, B[(uint32)(seg - j)]);
					pushCorner(st, A[(uint32)j]);
					pushCorner(st, A[(uint32)(j + 1)]);
					pushCorner(st, B[(uint32)(seg - j - 1)]);
					endFace(st, 3u, atBande);
				}
			}
			// (c) faces de RACCORD aux sommets (anneau des points autour du sommet, parcouru
			//     à l'ENVERS pour que la face regarde vers l'extérieur).
			NkVector<uint32> ring, rr;
			for (uint32 v = 0; v < NV; ++v) {
				if (!touched[v] || W.verts[v].hedge == NK_EM_INVALID)
					continue;
				ring.Clear();
				// (c) LA FACE DE RACCORD N'A PAS DE MERE NON PLUS. Ses voisines sont
				// TOUTES les faces incidentes au sommet, et le poids de chacune est la
				// longueur du morceau d'anneau pose sur elle : le segment ptPrev[h] ->
				// ptNext[h] de la demi-arete qui lui appartient.
				// Collecte faite DANS le meme tour que l'anneau, sous les memes sorties
				// (`open`) : un second parcours pourrait s'arreter ailleurs et ponderer
				// des faces qui ne sont pas celles de l'anneau retenu.
				NkVector<uint16> cmats;
				NkVector<uint8> csms;
				NkVector<float32> cpoids;
				bool open = false;
				const NkEmId h0 = W.verts[v].hedge;
				NkEmId h = h0;
				uint32 g = 0;
				do {
					ring.PushBack(ptPrev[h]);
					ring.PushBack(ptNext[h]);
					{
						const NkEmId fh = W.hedges[h].face;
						if (fh != NK_EM_INVALID && fh < (NkEmId)W.faces.Size() && W.faces[fh].alive) {
							cmats.PushBack(W.faces[fh].material);
							csms.PushBack(W.faces[fh].smooth);
							// ⚠ VAUT ZERO quand les deux aretes du coin sont chanfreinees
							// (ptPrev == ptNext, le recul est un point unique). Toutes les
							// ponderations sont alors nulles et l'indice le plus bas
							// tranche — c'est la clause d'egalite de la regle, pas une
							// exception : le cas est frequent (chanfreiner TOUTES les
							// aretes d'un cube le produit a chaque coin).
							cpoids.PushBack((np[ptPrev[h]].pos - np[ptNext[h]].pos).Len());
						}
					}
					if (arcBase[h] >= 0)
						for (int32 j = 0; j < seg - 1; ++j)
							ring.PushBack(arcData[(uint32)arcBase[h] + (uint32)j]);
					const NkEmId tw = W.hedges[h].twin;
					if (tw == NK_EM_INVALID) {
						open = true;
						break;
					}
					h = W.hedges[tw].next;
					if (h == NK_EM_INVALID) {
						open = true;
						break;
					}
				} while (h != h0 && ++g < 4096u);
				if (open)
					continue; // sommet de BORD : pas de raccord (limite assumée)
				// Anneau parcouru à l'ENVERS (orientation sortante), compacté.
				rr.Clear();
				for (uint32 k = (uint32)ring.Size(); k > 0; --k) {
					const uint32 id = ring[k - 1];
					if (!rr.Empty() && rr[(uint32)rr.Size() - 1] == id)
						continue;
					rr.PushBack(id);
				}
				while (rr.Size() > 1u && rr[(uint32)rr.Size() - 1] == rr[0])
					rr.Resize((uint32)rr.Size() - 1);
				const uint32 rn = (uint32)rr.Size();
				if (rn < 3u)
					continue; // anneau dégénéré -> le coin est déjà fermé par les faces voisines
				// UN SEUL attribut pour tout le raccord, eventail compris : un coin de
				// chanfrein est UNE surface, comme la bande. Les triangles de l'eventail
				// ne sont qu'une triangulation, pas des faces distinctes pour l'oeil.
				const NkEditMesh::FaceAttrib atCoin =
					EM_AttribFromNeighbours(cmats.Data(), csms.Data(), cpoids.Data(), (uint32)cmats.Size(),
											&perdus);
				if (rn <= 4u) {
					const uint32 st = (uint32)nfv.Size();
					for (uint32 k = 0; k < rn; ++k)
						nfv.PushBack(rr[k]);
					nfs.PushBack((uint32)nfv.Size());
					nfa.PushBack(atCoin);
					continue;
				}
				// COIN ARRONDI (bevel à plusieurs segments) : l'anneau est très NON PLAN.
				// Une seule n-gon serait triangulée en éventail depuis un de ses coins ->
				// bosses visibles. On pose donc un point central sur la sphère du coin et on
				// raccorde en éventail : coin lisse, faces quasi équilatérales.
				{
					// Le « coin intérieur » (centre de la sphère du coin) : v reculé le long de
					// TOUTES ses arêtes. Les points de l'anneau sont à peu près à distance
					// `offset` de ce point — on y pose donc le point central du raccord.
					NkVec3f ic = W.verts[v].pos;
					{
						NkEmId hh = W.verts[v].hedge;
						uint32 gg = 0;
						do {
							const uint32 dv = dstOf(hh);
							ic = ic + EM_Norm(W.verts[dv].pos - W.verts[v].pos) * tAlong(v, dv);
							const NkEmId tw2 = W.hedges[hh].twin;
							if (tw2 == NK_EM_INVALID)
								break;
							hh = W.hedges[tw2].next;
						} while (hh != W.verts[v].hedge && hh != NK_EM_INVALID && ++gg < 4096u);
					}
					NkVec3f cen{0.f, 0.f, 0.f};
					float32 rad = 0.f;
					for (uint32 k = 0; k < rn; ++k) {
						cen = cen + np[rr[k]].pos;
						rad += (np[rr[k]].pos - ic).Len();
					}
					cen = cen * (1.f / (float32)rn);
					rad /= (float32)rn;
					NkVertex3D nvc = wv[v];
					const NkVec3f dir = EM_Norm(cen - ic);
					nvc.pos = (dir.LenSq() > 0.f) ? (ic + dir * rad) : cen;
					const uint32 cid = pushPt(nvc, (uint8)1);
					for (uint32 k = 0; k < rn; ++k) {
						nfv.PushBack(cid);
						nfv.PushBack(rr[k]);
						nfv.PushBack(rr[(k + 1u) % rn]);
						nfs.PushBack((uint32)nfv.Size());
						nfa.PushBack(atCoin);
					}
				}
			}

			if (nfs.Size() < 2u || np.Empty())
				return false;
			const uint32 nfc = (uint32)nfs.Size() - 1u;
			BuildFromPolygons(np.Data(), (uint32)np.Size(), nfs.Data(), nfc, nfv.Data(),
							  (nfa.Size() == nfc) ? nfa.Data() : nullptr);
			ApplyVertSel(nsel);
			if (outMaterialChanged)
				*outMaterialChanged = perdus;
			return true;
		}

		// =====================================================================
		// INSET FACES (I) — face plus petite à l'intérieur + bande de raccord
		// ---------------------------------------------------------------------
		// INDIVIDUAL : chaque face sélectionnée reçoit son propre contour intérieur
		//   (rétréci par bissectrice de coin dans le plan de la face) ; la bande relie
		//   les 4 côtés. Deux faces voisines gardent leur contour extérieur COMMUN
		//   (maillage soudé) mais obtiennent des intérieurs séparés — exactement Blender.
		// REGION : la sélection est un bloc. Seules les arêtes de BORD de la région
		//   (celles qui n'ont qu'UNE face sélectionnée) engendrent la bande ; les arêtes
		//   intérieures restent partagées. Le déplacement d'un sommet de bord est la
		//   SOMME des directions « vers l'intérieur » de ses arêtes de bord (sur un coin
		//   droit, cela recule bien de `thickness` sur chaque côté).
		// =====================================================================
		bool NkEditMesh::InsetSelectedFaces(const NkInsetParams &p) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			NkVector<uint8> vsel;
			NkVector<uint32> wmap;
			// MATERIAU PAR FACE : transporte a travers le round-trip soude.
			NkVector<NkEditMesh::FaceAttrib> fm;
			NkVector<NkEditMesh::FaceAttrib> nfm;
			EM_ToWeldedPolygons(*this, pv, fs, fv, vsel, wmap, &fm);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			if (fc == 0)
				return false;
			NkVector<uint8> faceSel;
			faceSel.Resize(fc);
			uint32 selCount = 0;
			for (uint32 f = 0; f < fc; ++f) {
				const uint32 s = fs[f], e = fs[f + 1];
				bool sel = (e - s) >= 3u;
				for (uint32 k = s; k < e && sel; ++k)
					sel = (fv[k] < (uint32)vsel.Size()) && (vsel[fv[k]] != 0);
				faceSel[f] = sel ? (uint8)1 : (uint8)0;
				selCount += sel ? 1u : 0u;
			}
			if (selCount == 0)
				return false;
			float32 thick = p.thickness;
			if (thick <= 0.f)
				thick = EM_BBoxDiag(pv) * 0.08f;
			if (thick <= 1e-7f && p.depth == 0.f)
				return false;

			// Normale d'une face (convention moteur : cf. NkEmFaceCross).
			auto faceNormal = [&](uint32 f) -> NkVec3f {
				const uint32 s = fs[f], e = fs[f + 1];
				NkVec3f n{0.f, 0.f, 0.f};
				for (uint32 k = s + 1; k + 1 < e; ++k)
					n = n + NkEmFaceCross(pv[fv[s]].pos, pv[fv[k]].pos, pv[fv[k + 1]].pos);
				return EM_Norm(n);
			};

			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint8> nsel;
			nsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)nsel.Size(); ++i)
				nsel[i] = 0;
			for (uint32 f = 0; f < fc; ++f) { // faces NON sélectionnées : recopiées telles quelles
				if (faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; ++k)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
				nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
			}

			if (p.individual) {
				NkVector<uint32> inner;
				for (uint32 f = 0; f < fc; ++f) {
					if (!faceSel[f])
						continue;
					const uint32 s = fs[f], e = fs[f + 1], n = e - s;
					const NkVec3f fn = faceNormal(f);
					inner.Clear();
					for (uint32 k = 0; k < n; ++k) {
						const uint32 v = fv[s + k], pr = fv[s + (k + n - 1u) % n], nx = fv[s + (k + 1u) % n];
						const NkVec3f d1 = pv[pr].pos - pv[v].pos, d2 = pv[nx].pos - pv[v].pos;
						const NkVec3f u1 = EM_Norm(d1), u2 = EM_Norm(d2);
						float32 t1 = thick, t2 = thick;
						const float32 l1 = d1.Len() * 0.45f, l2 = d2.Len() * 0.45f;
						if (t1 > l1)
							t1 = l1;
						if (t2 > l2)
							t2 = l2;
						float32 sn = u1.Cross(u2).Len();
						if (sn < 0.2f)
							sn = 0.2f;
						NkVertex3D nv = pv[v];
						nv.pos = pv[v].pos + (u1 * t1 + u2 * t2) * (1.f / sn) + fn * p.depth;
						inner.PushBack((uint32)pv.Size());
						pv.PushBack(nv);
						nsel.PushBack(1);
					}
					for (uint32 k = 0; k < n; ++k) // face INTÉRIEURE (même winding)
						nfv.PushBack(inner[k]);
					nfs.PushBack((uint32)nfv.Size());
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					for (uint32 k = 0; k < n; ++k) { // BANDE de raccord
						nfv.PushBack(fv[s + k]);
						nfv.PushBack(fv[s + (k + 1u) % n]);
						nfv.PushBack(inner[(k + 1u) % n]);
						nfv.PushBack(inner[k]);
						nfs.PushBack((uint32)nfv.Size());
						nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					}
				}
				if (nfs.Size() < 2u)
					return false;
				BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
								  nfm.Data());
				ApplyVertSel(nsel);
				return true;
			}

			// ── MODE RÉGION ──────────────────────────────────────────────────
			const uint32 baseVC = (uint32)pv.Size();
			NkHashMap<uint64, uint8> dirEdge; // arêtes ORIENTÉES des faces sélectionnées
			for (uint32 f = 0; f < fc; ++f) {
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				for (uint32 k = 0; k < n; ++k)
					dirEdge.InsertOrAssign(((uint64)fv[s + k] << 32) | (uint64)fv[s + (k + 1u) % n], (uint8)1);
			}
			NkVector<NkVec3f> disp, nrm;
			disp.Resize(baseVC);
			nrm.Resize(baseVC);
			for (uint32 i = 0; i < baseVC; ++i) {
				disp[i] = {0.f, 0.f, 0.f};
				nrm[i] = {0.f, 0.f, 0.f};
			}
			NkVector<uint8> inRegion;
			inRegion.Resize(baseVC);
			for (uint32 i = 0; i < baseVC; ++i)
				inRegion[i] = 0;
			bool anyBoundary = false;
			for (uint32 f = 0; f < fc; ++f) {
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				const NkVec3f fn = faceNormal(f);
				NkVec3f cen{0.f, 0.f, 0.f};
				for (uint32 k = 0; k < n; ++k) {
					cen = cen + pv[fv[s + k]].pos;
					inRegion[fv[s + k]] = 1;
					nrm[fv[s + k]] = nrm[fv[s + k]] + fn;
				}
				cen = cen * (1.f / (float32)n);
				for (uint32 k = 0; k < n; ++k) {
					const uint32 a = fv[s + k], b = fv[s + (k + 1u) % n];
					if (dirEdge.Find(((uint64)b << 32) | (uint64)a))
						continue; // arête INTÉRIEURE à la région
					anyBoundary = true;
					// Direction « vers l'intérieur de la face », perpendiculaire à l'arête.
					const NkVec3f d = EM_Norm(pv[b].pos - pv[a].pos);
					const NkVec3f m = (pv[a].pos + pv[b].pos) * 0.5f;
					NkVec3f w = cen - m;
					w = EM_Norm(w - d * w.Dot(d));
					disp[a] = disp[a] + w;
					disp[b] = disp[b] + w;
				}
			}
			if (!anyBoundary && p.depth == 0.f)
				return false; // région fermée sans profondeur -> rien à faire
			NkVector<int32> innerOf;
			innerOf.Resize(baseVC);
			for (uint32 i = 0; i < baseVC; ++i)
				innerOf[i] = -1;
			for (uint32 i = 0; i < baseVC; ++i) {
				if (!inRegion[i])
					continue;
				NkVertex3D nv = pv[i];
				nv.pos = pv[i].pos + disp[i] * thick + EM_Norm(nrm[i]) * p.depth;
				innerOf[i] = (int32)pv.Size();
				pv.PushBack(nv);
				nsel.PushBack(1);
			}
			for (uint32 f = 0; f < fc; ++f) { // faces sélectionnées -> version intérieure
				if (!faceSel[f])
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; ++k)
					nfv.PushBack((uint32)innerOf[fv[k]]);
				nfs.PushBack((uint32)nfv.Size());
				nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
			}
			for (uint32 f = 0; f < fc; ++f) { // BANDE sur les seules arêtes de BORD
				if (!faceSel[f])
					continue;
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				for (uint32 k = 0; k < n; ++k) {
					const uint32 a = fv[s + k], b = fv[s + (k + 1u) % n];
					if (dirEdge.Find(((uint64)b << 32) | (uint64)a))
						continue;
					nfv.PushBack(a);
					nfv.PushBack(b);
					nfv.PushBack((uint32)innerOf[b]);
					nfv.PushBack((uint32)innerOf[a]);
					nfs.PushBack((uint32)nfv.Size());
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
				}
			}
			if (nfs.Size() < 2u)
				return false;
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
							  nfm.Data());
			ApplyVertSel(nsel);
			return true;
		}

		// =====================================================================
		// EDGE SPLIT (V) — dé-soudure locale le long des arêtes sélectionnées
		// ---------------------------------------------------------------------
		// Autour de chaque sommet touché, on parcourt le VENTILATEUR de faces
		// (rot(h) = twin(h).next). Chaque traversée d'une arête SÉLECTIONNÉE ouvre un
		// nouveau GROUPE ; chaque groupe reçoit sa propre copie du sommet, décalée d'un
		// demi-`gap` le long de la normale moyenne de ses faces. Après reconstruction,
		// LinkTwins n'apparie plus les demi-arêtes de part et d'autre (positions
		// différentes) : la déchirure est réelle et les twins restent cohérents.
		// =====================================================================
		bool NkEditMesh::SplitSelectedEdges(const NkEdgeSplitParams &p) {
			NkVector<NkVertex3D> wv;
			NkVector<uint32> wfs, wfv;
			NkVector<uint8> wsel;
			NkVector<uint32> wmap;
			// MATERIAU PAR FACE : transporte a travers le round-trip soude.
			NkVector<NkEditMesh::FaceAttrib> fm;
			NkVector<NkEditMesh::FaceAttrib> nfm;
			EM_ToWeldedPolygons(*this, wv, wfs, wfv, wsel, wmap, &fm);
			const uint32 wfc = (wfs.Size() > 0) ? (uint32)wfs.Size() - 1 : 0;
			if (wfc == 0)
				return false;
			NkEditMesh W;
			W.BuildFromPolygons(wv.Data(), (uint32)wv.Size(), wfs.Data(), wfc, wfv.Data());
			const uint32 NV = W.VertCount(), HC = (uint32)W.hedges.Size();
			if (NV == 0 || HC == 0)
				return false;
			for (uint32 i = 0; i < NV && i < (uint32)wsel.Size(); ++i)
				W.verts[i].sel = wsel[i];

			NkVector<NkEmId> prevOf;
			prevOf.Resize(HC);
			for (uint32 i = 0; i < HC; ++i)
				prevOf[i] = NK_EM_INVALID;
			for (uint32 f = 0; f < (uint32)W.faces.Size(); ++f) {
				if (!W.faces[f].alive || W.faces[f].hedge == NK_EM_INVALID)
					continue;
				const NkEmId s = W.faces[f].hedge;
				NkEmId h = s;
				uint32 g = 0;
				do {
					const NkEmId nx = W.hedges[h].next;
					if (nx == NK_EM_INVALID)
						break;
					prevOf[nx] = h;
					h = nx;
				} while (h != s && ++g < 100000u);
			}
			auto dstOf = [&](NkEmId h) -> uint32 {
				const NkEmId nx = W.hedges[h].next;
				return (nx == NK_EM_INVALID) ? W.hedges[h].origin : W.hedges[nx].origin;
			};
			auto ekey = [](uint32 a, uint32 b) -> uint64 {
				const uint32 lo = (a < b) ? a : b, hi = (a < b) ? b : a;
				return ((uint64)lo << 32) | (uint64)hi;
			};
			NkHashMap<uint64, uint8> selE;
			NkVector<uint8> touched;
			touched.Resize(NV);
			for (uint32 i = 0; i < NV; ++i)
				touched[i] = 0;
			for (uint32 h = 0; h < HC; ++h) {
				if (!W.hedges[h].alive || W.hedges[h].twin == NK_EM_INVALID)
					continue; // arête de BORD : déjà ouverte
				const uint32 a = W.hedges[h].origin, b = dstOf((NkEmId)h);
				if (a == b || a >= NV || b >= NV || !W.verts[a].sel || !W.verts[b].sel)
					continue;
				selE.InsertOrAssign(ekey(a, b), (uint8)1);
				touched[a] = 1;
				touched[b] = 1;
			}
			if (selE.Empty())
				return false;
			float32 gap = p.gap;
			if (gap <= 0.f)
				gap = EM_BBoxDiag(wv) * 0.01f;

			NkVector<NkVertex3D> np = wv;
			NkVector<uint8> nsel;
			nsel.Resize((uint32)np.Size());
			for (uint32 i = 0; i < (uint32)nsel.Size(); ++i)
				nsel[i] = 0;
			NkVector<int32> cornerOf; // coin (demi-arête) -> sommet de sortie
			cornerOf.Resize(HC);
			for (uint32 i = 0; i < HC; ++i)
				cornerOf[i] = -1;
			bool splitAny = false;
			NkVector<NkEmId> fanH;
			NkVector<int32> fanG;
			NkVector<NkVec3f> gN;
			NkVector<int32> gIdx;
			for (uint32 v = 0; v < NV; ++v) {
				if (!touched[v] || W.verts[v].hedge == NK_EM_INVALID)
					continue;
				const NkEmId h0 = W.verts[v].hedge;
				// 1) point de départ : le bord du ventilateur s'il est OUVERT, sinon la
				//    demi-arête qui suit immédiatement une arête sélectionnée (sans quoi le
				//    1er et le dernier groupe du tour seraient comptés deux fois).
				NkEmId start = h0;
				bool closed = true;
				{
					NkEmId x = h0;
					uint32 g = 0;
					while (++g < 4096u) {
						const NkEmId pr = prevOf[x];
						const NkEmId tw = (pr == NK_EM_INVALID) ? NK_EM_INVALID : W.hedges[pr].twin;
						if (tw == NK_EM_INVALID) {
							start = x;
							closed = false;
							break;
						}
						x = tw;
						if (x == h0)
							break;
					}
				}
				if (closed) {
					NkEmId x = h0;
					uint32 g = 0;
					do {
						const NkEmId tw = W.hedges[x].twin;
						if (tw == NK_EM_INVALID)
							break;
						if (selE.Find(ekey(v, dstOf(x)))) {
							start = W.hedges[tw].next;
							break;
						}
						x = W.hedges[tw].next;
					} while (x != h0 && x != NK_EM_INVALID && ++g < 4096u);
				}
				// 2) parcours du ventilateur : groupe incrémenté à chaque arête sélectionnée.
				fanH.Clear();
				fanG.Clear();
				gN.Clear();
				gIdx.Clear();
				gN.PushBack({0.f, 0.f, 0.f});
				gIdx.PushBack((int32)v); // groupe 0 = le sommet d'origine
				int32 grp = 0;
				NkEmId x = start;
				uint32 g = 0;
				while (x != NK_EM_INVALID && ++g < 4096u) {
					fanH.PushBack(x);
					fanG.PushBack(grp);
					if (W.hedges[x].face != NK_EM_INVALID)
						gN[(uint32)grp] = gN[(uint32)grp] + W.faces[W.hedges[x].face].normal;
					const NkEmId tw = W.hedges[x].twin;
					if (tw == NK_EM_INVALID)
						break; // fin d'un ventilateur ouvert
					const NkEmId nx = W.hedges[tw].next;
					if (nx == NK_EM_INVALID || nx == start)
						break; // tour complet
					if (selE.Find(ekey(v, dstOf(x)))) {
						++grp;
						gN.PushBack({0.f, 0.f, 0.f});
						gIdx.PushBack(-1);
					}
					x = nx;
				}
				// 3) un seul groupe -> le sommet reste partagé (arête isolée : cf. limites).
				if (gIdx.Size() > 1u) {
					splitAny = true;
					for (uint32 q = 1; q < (uint32)gIdx.Size(); ++q) {
						gIdx[q] = (int32)np.Size();
						np.PushBack(wv[v]);
						nsel.PushBack(1); // les morceaux DÉTACHÉS deviennent la sélection
					}
					for (uint32 q = 0; q < (uint32)gIdx.Size(); ++q) {
						const uint32 id = (uint32)gIdx[q];
						np[id].pos = np[id].pos + EM_Norm(gN[q]) * (gap * 0.5f);
					}
				}
				for (uint32 q = 0; q < (uint32)fanH.Size(); ++q)
					cornerOf[fanH[q]] = gIdx[(uint32)fanG[q]];
			}
			if (!splitAny)
				return false;

			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			for (uint32 f = 0; f < (uint32)W.faces.Size(); ++f) {
				if (!W.faces[f].alive || W.faces[f].hedge == NK_EM_INVALID)
					continue;
				const NkEmId s = W.faces[f].hedge;
				NkEmId h = s;
				uint32 g = 0;
				do {
					nfv.PushBack((cornerOf[h] >= 0) ? (uint32)cornerOf[h] : W.hedges[h].origin);
					h = W.hedges[h].next;
				} while (h != s && h != NK_EM_INVALID && ++g < 100000u);
				nfs.PushBack((uint32)nfv.Size());
				nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
			}
			BuildFromPolygons(np.Data(), (uint32)np.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
							  nfm.Data());
			ApplyVertSel(nsel);
			return true;
		}

		// =====================================================================
		// SPIN / RÉVOLUTION (J) — le profil sélectionné tourne autour d'un axe
		// ---------------------------------------------------------------------
		// Le centre et l'axe arrivent dans l'espace du CURSEUR 3D (monde éditeur) : on les
		// ramène en local par l'inverse de `localToSpin` (l'axe est une DIRECTION : on le
		// transforme comme une différence de deux points, ce qui reste juste sous une
		// transform à rotation/échelle quelconque).
		// =====================================================================
		bool NkEditMesh::SpinSelected(const NkSpinParams &p, const NkMat4f &localToSpin,
										  uint32 *outMaterialChanged) {
			if (outMaterialChanged)
				*outMaterialChanged = 0;
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			NkVector<uint8> vsel;
			NkVector<uint32> wmap;
			// MATERIAU PAR FACE : transporte a travers la revolution.
			NkVector<NkEditMesh::FaceAttrib> fm;
			NkVector<NkEditMesh::FaceAttrib> nfm;
			EM_ToWeldedPolygons(*this, pv, fs, fv, vsel, wmap, &fm);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			const uint32 baseVC = (uint32)pv.Size();
			if (baseVC == 0)
				return false;
			const NkMat4f inv = localToSpin.Inverse();
			const NkVec3f ctr = inv * p.center;
			NkVec3f ax = EM_Norm((inv * (p.center + p.axis)) - ctr);
			if (ax.LenSq() < 1e-12f)
				ax = {0.f, 1.f, 0.f};
			const int32 steps = (p.steps < 1) ? 1 : ((p.steps > 256) ? 256 : p.steps);

			// Profil : sommets sélectionnés + arêtes dont les DEUX extrémités le sont.
			NkVector<int32> slot;
			slot.Resize(baseVC);
			NkVector<uint32> prof;
			for (uint32 i = 0; i < baseVC; ++i) {
				slot[i] = -1;
				if (i < (uint32)vsel.Size() && vsel[i]) {
					slot[i] = (int32)prof.Size();
					prof.PushBack(i);
				}
			}
			if (prof.Empty())
				return false;
			NkVector<uint32> eA, eB;
			NkVector<uint8> faceSel;
			faceSel.Resize(fc);
			{
				NkHashMap<uint64, uint8> seen;
				for (uint32 f = 0; f < fc; ++f) {
					const uint32 s = fs[f], e = fs[f + 1], n = e - s;
					bool allSel = (n >= 3u);
					for (uint32 k = 0; k < n; ++k) {
						const uint32 a = fv[s + k], b = fv[s + (k + 1u) % n];
						if (slot[a] < 0 || slot[b] < 0) {
							allSel = false;
							continue;
						}
						const uint32 lo = (a < b) ? a : b, hi = (a < b) ? b : a;
						const uint64 key = ((uint64)lo << 32) | (uint64)hi;
						if (seen.Find(key))
							continue;
						seen.InsertOrAssign(key, (uint8)1);
						eA.PushBack(a);
						eB.PushBack(b);
					}
					faceSel[f] = allSel ? (uint8)1 : (uint8)0;
				}
			}
			if (eA.Empty() && !p.duplicate)
				return false; // pas d'arête à balayer

			// FACES INCIDENTES A CHAQUE ARETE DU PROFIL. Une bande laterale nait d une
			// ARETE, pas d une face : elle herite donc de son VOISINAGE, comme toute
			// face creee. Construit sur la MEME numerotation que `fm` (celle de
			// EM_ToWeldedPolygons), donc aucun decalage possible.
			NkHashMap<uint64, uint64> edgeFaces; // cle arete -> (f0+1) | ((f1+1) << 32)
			{
				for (uint32 f = 0; f < fc; ++f) {
					const uint32 s = fs[f], e = fs[f + 1], n = e - s;
					if (n < 2u)
						continue;
					for (uint32 k = 0; k < n; ++k) {
						const uint32 a = fv[s + k], b = fv[s + (k + 1u) % n];
						if (a == b)
							continue;
						const uint32 lo = (a < b) ? a : b, hi = (a < b) ? b : a;
						const uint64 key = ((uint64)lo << 32) | (uint64)hi;
						uint64 *q = edgeFaces.Find(key);
						const uint64 tag = (uint64)(f + 1u);
						if (!q)
							edgeFaces.InsertOrAssign(key, tag);
						else if ((*q & 0xFFFFFFFFull) != tag && (*q >> 32) == 0ull)
							edgeFaces.InsertOrAssign(key, *q | (tag << 32));
					}
				}
			}
			uint32 perdus = 0;

			// Anneaux successifs du balayage : ring[k * pn + j].
			const uint32 pn = (uint32)prof.Size();
			NkVector<uint32> ring;
			ring.Resize((uint32)(steps + 1) * pn);
			for (uint32 j = 0; j < pn; ++j)
				ring[j] = prof[j];
			for (int32 k = 1; k <= steps; ++k) {
				const float32 t = p.angle * (float32)k / (float32)steps;
				const float32 cs = cosf(t), sn = sinf(t);
				for (uint32 j = 0; j < pn; ++j) {
					const NkVec3f r = pv[prof[j]].pos - ctr;
					NkVertex3D nv = pv[prof[j]];
					nv.pos = ctr + r * cs + ax.Cross(r) * sn + ax * (ax.Dot(r) * (1.f - cs)); // Rodrigues
					ring[(uint32)k * pn + j] = (uint32)pv.Size();
					pv.PushBack(nv);
				}
			}

			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			for (uint32 f = 0; f < fc; ++f) { // la géométrie d'origine est CONSERVÉE
				for (uint32 k = fs[f]; k < fs[f + 1]; ++k)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
				nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
			}
			if (p.duplicate) { // copies ISOLÉES des faces sélectionnées à chaque pas
				for (int32 k = 1; k <= steps; ++k)
					for (uint32 f = 0; f < fc; ++f) {
						if (!faceSel[f])
							continue;
						for (uint32 q = fs[f]; q < fs[f + 1]; ++q)
							nfv.PushBack(ring[(uint32)k * pn + (uint32)slot[fv[q]]]);
						nfs.PushBack((uint32)nfv.Size());
						// Chaque copie herite de la face dont elle est la copie.
						nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					}
			} else { // bandes reliant les anneaux consécutifs
				for (int32 k = 0; k < steps; ++k) {
					for (uint32 e = 0; e < (uint32)eA.Size(); ++e) {
						const uint32 a0 = ring[(uint32)k * pn + (uint32)slot[eA[e]]];
						const uint32 b0 = ring[(uint32)k * pn + (uint32)slot[eB[e]]];
						const uint32 a1 = ring[(uint32)(k + 1) * pn + (uint32)slot[eA[e]]];
						const uint32 b1 = ring[(uint32)(k + 1) * pn + (uint32)slot[eB[e]]];
						// ORIENTATION : la normale du quad doit FUIR l'axe (surface de
						// révolution vue de l'extérieur) ; sinon on inverse la boucle.
						const NkVec3f n4 = NkEmFaceCross(pv[a0].pos, pv[b0].pos, pv[b1].pos);
						const NkVec3f cq = (pv[a0].pos + pv[b0].pos + pv[a1].pos + pv[b1].pos) * 0.25f;
						NkVec3f rad = cq - ctr;
						rad = rad - ax * rad.Dot(ax);
						if ((rad.LenSq() > 1e-12f) && (n4.Dot(rad) < 0.f)) {
							nfv.PushBack(a1);
							nfv.PushBack(b1);
							nfv.PushBack(b0);
							nfv.PushBack(a0);
						} else {
							nfv.PushBack(a0);
							nfv.PushBack(b0);
							nfv.PushBack(b1);
							nfv.PushBack(a1);
						}
						nfs.PushBack((uint32)nfv.Size());
						// ⚠️ UNE BANDE LATERALE N'A PAS DE FACE MERE : elle nait d'une
						// ARETE du profil, pas d'une face.
						//
						// ⚠️ CORRIGE LE 2026-08-23. Cette ligne posait le SLOT 0, avec
						// pour raison ecrite qu'heriter d'une des deux faces adjacentes
						// « privilegierait arbitrairement un cote ». L'arbitrage de
						// Rodolf a retire cette raison : une face creee herite de son
						// VOISINAGE, dominance par la longueur de contour partage,
						// egalite par l'indice le plus bas. Ce n'est plus arbitraire,
						// et le slot 0 est une couleur comme une autre, pas un
						// « sans materiau » : le poser repeignait la bande en silence.
						//   > Une regle qui traine une exception que personne ne se
						//   > rappelle est pire que pas de regle : elle fait croire
						//   > qu on peut predire le comportement.
						{
							const uint32 va = eA[e], vb = eB[e];
							const uint32 lo = (va < vb) ? va : vb, hi = (va < vb) ? vb : va;
							const uint64 key = ((uint64)lo << 32) | (uint64)hi;
							uint64 *q = edgeFaces.Find(key);
							uint16 mats[2];
							uint8 sms[2];
							float32 poids[2];
							uint32 nn = 0;
							// Les deux voisines partagent le MEME segment : longueurs
							// egales, donc c'est l'indice le plus bas qui tranche. On passe
							// quand meme par la regle commune plutot que de recrire
							// « prends le plus petit », qui serait un second enonce.
							const float32 lg = (pv[a0].pos - pv[b0].pos).Len();
							if (q) {
								const uint32 f0 = (uint32)((*q) & 0xFFFFFFFFull);
								const uint32 f1 = (uint32)((*q) >> 32);
								if (f0 > 0u && (f0 - 1u) < (uint32)fm.Size()) {
									mats[nn] = fm[f0 - 1u].material;
									sms[nn] = fm[f0 - 1u].smooth;
									poids[nn] = lg;
									++nn;
								}
								if (f1 > 0u && (f1 - 1u) < (uint32)fm.Size()) {
									mats[nn] = fm[f1 - 1u].material;
									sms[nn] = fm[f1 - 1u].smooth;
									poids[nn] = lg;
									++nn;
								}
							}
							nfm.PushBack(EM_AttribFromNeighbours(mats, sms, poids, nn, &perdus));
						}
					}
				}
			}
			NkVector<uint8> nsel;
			nsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)nsel.Size(); ++i)
				nsel[i] = 0;
			for (uint32 j = 0; j < pn; ++j) // sélection = DERNIER anneau (façon Blender)
				nsel[ring[(uint32)steps * pn + j]] = 1;
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
							  nfm.Data());
			ApplyVertSel(nsel);
			if (outMaterialChanged)
				*outMaterialChanged = perdus;
			return true;
		}

		// =====================================================================
		// DISSOLVE (Ctrl+X) — retire des éléments SANS trouer : les faces voisines
		// fusionnent en n-gon. C'est l'opposé de « supprimer » (X).
		// ---------------------------------------------------------------------
		// UN SEUL algorithme pour les trois modes : on marque les arêtes à RETIRER, puis
		// on reparcourt les contours. Pour une demi-arête de contour h, la suivante est
		// obtenue en avançant dans la face et, TANT QUE l'arête rencontrée est retirée, en
		// passant chez le voisin par le jumeau :  x = next(h) ; while(retiree(x)) x = next(twin(x)).
		// Ce parcours saute naturellement les sommets devenus intérieurs à la région —
		// c'est ce qui rend le dissolve de SOMMET et de FACE identiques à celui d'ARÊTE.
		// =====================================================================
		bool NkEditMesh::DissolveSelected(const NkDissolveParams &p, uint32 *outMaterialChanged) {
			if (outMaterialChanged)
				*outMaterialChanged = 0;
			NkVector<NkVertex3D> wv;
			NkVector<uint32> wfs, wfv;
			NkVector<uint8> wsel;
			NkVector<uint32> wmap;
			// MATERIAU PAR FACE : recupere DES la soudure, donc dans le meme ordre
			// que les faces de `W`. Le lire plus tard supposerait que cet ordre est
			// conserve — une hypothese qu'on n'a pas a prendre.
			NkVector<NkEditMesh::FaceAttrib> wfm;
			EM_ToWeldedPolygons(*this, wv, wfs, wfv, wsel, wmap, &wfm);
			const uint32 wfc = (wfs.Size() > 0) ? (uint32)wfs.Size() - 1 : 0;
			if (wfc == 0)
				return false;
			NkEditMesh W;
			W.BuildFromPolygons(wv.Data(), (uint32)wv.Size(), wfs.Data(), wfc, wfv.Data(),
								(wfm.Size() == wfc) ? wfm.Data() : nullptr);
			// (wfm porte materiau ET ombrage depuis l'arbitrage du 2026-08-22 : le
			// maillage soude est donc une copie fidele des attributs, pas seulement
			// de la topologie.)
			const uint32 NV = W.VertCount(), HC = (uint32)W.hedges.Size();
			if (NV == 0 || HC == 0)
				return false;
			for (uint32 i = 0; i < NV && i < (uint32)wsel.Size(); ++i)
				W.verts[i].sel = wsel[i];
			auto dstOf = [&](NkEmId h) -> uint32 {
				const NkEmId nx = W.hedges[h].next;
				return (nx == NK_EM_INVALID) ? W.hedges[h].origin : W.hedges[nx].origin;
			};

			// ── 1) Arêtes à RETIRER, selon le mode ───────────────────────────
			NkVector<uint8> gone; // par demi-arête (symétrique avec son jumeau)
			gone.Resize(HC);
			for (uint32 i = 0; i < HC; ++i)
				gone[i] = 0;
			const int32 mode = (p.mode < 0 || p.mode > 2) ? 1 : p.mode;
			NkVector<uint8> faceSel;
			if (mode == 2) {
				faceSel.Resize((uint32)W.faces.Size());
				for (uint32 f = 0; f < (uint32)W.faces.Size(); ++f)
					faceSel[f] = (W.faces[f].alive && W.FaceIsSelected((NkEmId)f)) ? (uint8)1 : (uint8)0;
			}
			uint32 removedCount = 0;
			for (uint32 h = 0; h < HC; ++h) {
				const NkEmId tw = W.hedges[h].twin;
				if (!W.hedges[h].alive || tw == NK_EM_INVALID)
					continue; // arête de BORD : rien à fusionner
				if (W.hedges[h].face == NK_EM_INVALID || W.hedges[tw].face == NK_EM_INVALID)
					continue;
				if (W.hedges[h].face == W.hedges[tw].face)
					continue; // même face des deux côtés : dégénéré
				const uint32 a = W.hedges[h].origin, b = dstOf((NkEmId)h);
				if (a >= NV || b >= NV)
					continue;
				bool kill = false;
				if (mode == 0)
					kill = (W.verts[a].sel != 0) || (W.verts[b].sel != 0); // Verts
				else if (mode == 1)
					kill = (W.verts[a].sel != 0) && (W.verts[b].sel != 0); // Edges
				else
					kill = (faceSel[W.hedges[h].face] != 0) && (faceSel[W.hedges[tw].face] != 0); // Faces
				if (!kill)
					continue;
				if (!gone[h])
					++removedCount;
				gone[h] = 1;
				gone[tw] = 1;
			}
			if (removedCount == 0)
				return false;

			// -- 1bis) REGIONS FUSIONNEES ET MATERIAU RETENU PAR REGION --------
			// ⚠ LE CONTOUR NE SUFFIT PAS A CONNAITRE LES CONTRIBUTEURS. Une face
			// entierement INTERIEURE a la region (toutes ses aretes retirees) n'est
			// jamais visitee par le parcours du contour -- son materiau et son aire
			// disparaitraient du calcul sans que rien ne le signale. On regroupe donc
			// les faces par COMPOSANTE CONNEXE des aretes retirees, ce qui les tient
			// toutes, contour ou pas.
			const uint32 WFC = (uint32)W.faces.Size();
			NkVector<uint32> parent;
			parent.Resize(WFC);
			for (uint32 i = 0; i < WFC; ++i)
				parent[i] = i;
			auto trouver = [&](uint32 x) {
				while (parent[x] != x) {
					parent[x] = parent[parent[x]]; // compression de chemin
					x = parent[x];
				}
				return x;
			};
			for (uint32 h = 0; h < HC; ++h) {
				if (!gone[h])
					continue;
				const NkEmId tw = W.hedges[h].twin;
				if (tw == NK_EM_INVALID)
					continue;
				const NkEmId fa2 = W.hedges[h].face, fb2 = W.hedges[tw].face;
				if (fa2 == NK_EM_INVALID || fb2 == NK_EM_INVALID || fa2 >= WFC || fb2 >= WFC)
					continue;
				const uint32 ra = trouver(fa2), rb = trouver(fb2);
				if (ra != rb)
					parent[ra] = rb;
			}
			// Regroupement en CSR : une seule passe par face, jamais un balayage par
			// region (qui serait quadratique sur un gros maillage).
			NkVector<uint32> cnt, deb, fill;
			cnt.Resize(WFC);
			deb.Resize(WFC + 1u);
			for (uint32 i = 0; i < WFC; ++i)
				cnt[i] = 0;
			for (uint32 f = 0; f < WFC; ++f)
				if (W.faces[f].alive)
					cnt[trouver(f)]++;
			deb[0] = 0;
			for (uint32 i = 0; i < WFC; ++i)
				deb[i + 1u] = deb[i] + cnt[i];
			fill = deb;
			NkVector<uint16> mflat;
			NkVector<uint8> sflat;
			NkVector<float32> aflat;
			mflat.Resize(deb[WFC]);
			sflat.Resize(deb[WFC]);
			aflat.Resize(deb[WFC]);
			for (uint32 f = 0; f < WFC; ++f) {
				if (!W.faces[f].alive)
					continue;
				const uint32 r = trouver(f), k = fill[r]++;
				// Les DEUX attributs sont ranges par la MEME table de parente et au
				// meme rang : aucun des deux ne peut se retrouver aligne sur une autre
				// face que l'autre.
				mflat[k] = W.faces[f].material;
				sflat[k] = W.faces[f].smooth;
				aflat[k] = W.FaceArea((NkEmId)f);
			}
			NkVector<NkEditMesh::FaceAttrib> attribRegion;
			attribRegion.Resize(WFC);
			uint32 materiauxPerdus = 0;
			for (uint32 r = 0; r < WFC; ++r) {
				attribRegion[r] = NkEditMesh::FaceAttrib{};
				if (cnt[r] == 0)
					continue;
				const uint16 retenu = EM_MaterialDominant(&mflat[deb[r]], &aflat[deb[r]], cnt[r]);
				attribRegion[r].material = retenu;
				attribRegion[r].smooth = EM_SmoothMerged(&sflat[deb[r]], cnt[r]);
				// Une region d'UNE seule face n'est pas une fusion : EM_MaterialLost y
				// rend 0 de lui-meme, sans qu'on ait a le supposer.
				materiauxPerdus += EM_MaterialLost(&mflat[deb[r]], cnt[r], retenu);
			}

			// ── 2) Contours des régions fusionnées ───────────────────────────
			NkVector<uint8> seen;
			seen.Resize(HC);
			for (uint32 i = 0; i < HC; ++i)
				seen[i] = 0;
			NkVector<uint32> nfs, nfv;
			NkVector<NkEditMesh::FaceAttrib> nfm; // materiau de chaque face EMISE (une entree par nfs)
			nfs.PushBack(0);
			NkVector<uint8> touchedV; // sommets du contour d'une région fusionnée -> sélection
			touchedV.Resize(NV);
			for (uint32 i = 0; i < NV; ++i)
				touchedV[i] = 0;
			for (uint32 h0 = 0; h0 < HC; ++h0) {
				if (seen[h0] || gone[h0] || !W.hedges[h0].alive || W.hedges[h0].face == NK_EM_INVALID)
					continue;
				const uint32 st = (uint32)nfv.Size();
				bool merged = false;
				NkEmId h = (NkEmId)h0;
				uint32 g = 0;
				bool bad = false;
				do {
					seen[h] = 1;
					nfv.PushBack(W.hedges[h].origin);
					NkEmId x = W.hedges[h].next;
					uint32 g2 = 0;
					while (x != NK_EM_INVALID && gone[x] && ++g2 < 100000u) {
						merged = true;
						const NkEmId tx = W.hedges[x].twin;
						if (tx == NK_EM_INVALID) {
							bad = true;
							break;
						}
						x = W.hedges[tx].next;
					}
					if (bad || x == NK_EM_INVALID)
						break;
					h = x;
				} while (h != (NkEmId)h0 && ++g < 100000u);
				const uint32 n = (uint32)nfv.Size() - st;
				if (bad || n < 3u) {
					nfv.Resize(st); // contour dégénéré -> abandonné
					continue;
				}
				if (merged)
					for (uint32 k = st; k < (uint32)nfv.Size(); ++k)
						touchedV[nfv[k]] = 1;
				// La face emise appartient a la region de la face d'ou part son
				// contour : elle prend le materiau retenu pour cette region.
				{
					const NkEmId fh = W.hedges[h0].face;
					nfm.PushBack((fh != NK_EM_INVALID && fh < WFC) ? attribRegion[trouver(fh)]
																	 : NkEditMesh::FaceAttrib{});
				}
				nfs.PushBack((uint32)nfv.Size());
			}
			if (nfs.Size() < 2u)
				return false;

			// ── 3) COMPACTAGE : on ne garde que les sommets réellement utilisés ──
			NkVector<int32> remap;
			remap.Resize(NV);
			for (uint32 i = 0; i < NV; ++i)
				remap[i] = -1;
			NkVector<NkVertex3D> np;
			NkVector<uint8> nsel;
			for (uint32 k = 0; k < (uint32)nfv.Size(); ++k) {
				const uint32 v = nfv[k];
				if (remap[v] < 0) {
					remap[v] = (int32)np.Size();
					np.PushBack(wv[v]);
					nsel.PushBack(touchedV[v]);
				}
				nfv[k] = (uint32)remap[v];
			}
			const uint32 nfc = (uint32)nfs.Size() - 1u;
			BuildFromPolygons(np.Data(), (uint32)np.Size(), nfs.Data(), nfc, nfv.Data(),
							  (nfm.Size() == nfc) ? nfm.Data() : nullptr);
			ApplyVertSel(nsel);
			if (outMaterialChanged)
				*outMaterialChanged = materiauxPerdus;
			return true;
		}

		// BISECT / KNIFE : coupe le maillage par un PLAN. Chaque arête traversante reçoit un
		// sommet d'intersection (partagé) et chaque face traversée est coupée en 2. planePoint/
		// planeNormal dans l'espace de `xform` (modèle->monde éditeur, ou identité IA).
		bool NkEditMesh::BisectByPlane(const NkVec3f &pPoint, const NkVec3f &pNormal, const NkMat4f &xform) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// MATERIAU PAR FACE : transporte a travers le round-trip. Les morceaux d une face coupee heritent de la face d origine.
			NkVector<NkEditMesh::FaceAttrib> fm;
			NkVector<NkEditMesh::FaceAttrib> nfm;
			ToPolygons(pv, fs, fv, &fm);
			NkVector<float32> sd;
			sd.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)pv.Size(); i++) {
				NkVec3f w = xform * pv[i].pos;
				sd[i] = (w - pPoint).Dot(pNormal);
			}
			NkHashMap<uint64, uint32> cross;
			auto crossV = [&](uint32 a, uint32 b) -> int32 {
				if (sd[a] * sd[b] >= 0.f)
					return -1; // même côté (ou sur le plan)
				uint32 lo = a < b ? a : b, hi = a < b ? b : a;
				uint64 key = ((uint64)lo << 32) | hi;
				uint32 *q = cross.Find(key);
				if (q)
					return (int32)*q;
				float32 t = sd[a] / (sd[a] - sd[b]);
				NkVertex3D nv = pv[a];
				nv.pos = pv[a].pos + (pv[b].pos - pv[a].pos) * t;
				nv.uv = pv[a].uv + (pv[b].uv - pv[a].uv) * t;
				uint32 idx = (uint32)pv.Size();
				pv.PushBack(nv);
				cross.InsertOrAssign(key, idx);
				return (int32)idx;
			};
			NkVector<uint32> nfs, nfv;
			nfs.PushBack(0);
			NkVector<uint32> selCross;
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			NkVector<uint32> loop;
			NkVector<uint32> cpos;
			bool changed = false;
			for (uint32 f = 0; f < fc; f++) {
				const uint32 s = fs[f], e = fs[f + 1], n = e - s;
				loop.Clear();
				cpos.Clear();
				for (uint32 k = 0; k < n; k++) {
					loop.PushBack(fv[s + k]);
					int32 cv = crossV(fv[s + k], fv[s + (k + 1) % n]);
					if (cv >= 0) {
						cpos.PushBack((uint32)loop.Size());
						loop.PushBack((uint32)cv);
						selCross.PushBack((uint32)cv);
					}
				}
				if (cpos.Size() == 2) { // face traversée -> 2 sous-faces
					uint32 c0 = cpos[0], c1 = cpos[1], L = (uint32)loop.Size();
					changed = true;
					for (uint32 i = c0; i <= c1; i++)
						nfv.PushBack(loop[i]);
					nfs.PushBack((uint32)nfv.Size());
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
					for (uint32 i = c1; i < L; i++)
						nfv.PushBack(loop[i]);
					for (uint32 i = 0; i <= c0; i++)
						nfv.PushBack(loop[i]);
					nfs.PushBack((uint32)nfv.Size());
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
				} else {
					for (uint32 i = 0; i < (uint32)loop.Size(); i++)
						nfv.PushBack(loop[i]);
					nfs.PushBack((uint32)nfv.Size());
					nfm.PushBack(f < (uint32)fm.Size() ? fm[f] : NkEditMesh::FaceAttrib{});
				}
			}
			if (!changed)
				return false;
			BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), (uint32)nfs.Size() - 1, nfv.Data(),
							  nfm.Data());
			NkVector<uint8> vsel;
			vsel.Resize((uint32)pv.Size());
			for (uint32 i = 0; i < (uint32)vsel.Size(); i++)
				vsel[i] = 0;
			for (uint32 i = 0; i < (uint32)selCross.Size(); i++)
				if (selCross[i] < (uint32)vsel.Size())
					vsel[selCross[i]] = 1;
			ApplyVertSel(vsel);
			return true;
		}

		// =====================================================================
		// HISTORIQUE UNDO/REDO (mémento : snapshots complets de NkEditMesh)
		// =====================================================================

		// Empile un snapshot en respectant le plafond (retire le plus ancien si dépassé).
		static void EM_PushCapped(NkVector<NkEditMesh> &stack, const NkEditMesh &m, uint32 limit) {
			stack.PushBack(m);
			while ((uint32)stack.Size() > limit) { // retire le plus ancien (décalage)
				for (uint32 i = 1; i < (uint32)stack.Size(); ++i)
					stack[i - 1] = stack[i];
				stack.Resize((uint32)stack.Size() - 1);
			}
		}

		void NkEditHistory::Clear() {
			mUndo.Clear();
			mRedo.Clear();
		}

		void NkEditHistory::Commit(const NkEditMesh &preState) {
			EM_PushCapped(mUndo, preState, mLimit);
			mRedo.Clear(); // nouvelle branche -> redo invalidé
		}

		bool NkEditHistory::Undo(NkEditMesh &mesh) {
			if (mUndo.Empty())
				return false;
			mRedo.PushBack(mesh);					// sauve l'état courant pour redo
			mesh = mUndo[(uint32)mUndo.Size() - 1]; // restaure le précédent
			mUndo.Resize((uint32)mUndo.Size() - 1);
			return true;
		}

		bool NkEditHistory::Redo(NkEditMesh &mesh) {
			if (mRedo.Empty())
				return false;
			mUndo.PushBack(mesh);
			mesh = mRedo[(uint32)mRedo.Size() - 1];
			mRedo.Resize((uint32)mRedo.Size() - 1);
			return true;
		}

		// =====================================================================
		// COMMANDE D'ÉDITION SÉRIALISABLE — pose la sélection puis dispatch l'op.
		// C'est ce qui rend la couche de commandes SCRIPTABLE (modificateurs + IA).
		// =====================================================================
		// =====================================================================
		// TO SPHERE / SHRINK-FATTEN — deformations RADIALES (façon Blender)
		// =====================================================================
		// Toutes deux operent sur l'IDENTITE SOUDEE (BuildVertexMerge) : les copies
		// coincidentes d'un meme coin recoivent EXACTEMENT le meme deplacement, sinon
		// la soudure (donc les jumeaux de demi-aretes) serait rompue au premier appel.
		bool NkEditMesh::ToSphereSelected(const NkToSphereParams &p) {
			const uint32 nv = (uint32)verts.Size();
			if (nv == 0 || fabsf(p.factor) < 1e-6f)
				return false;
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			NkVector<NkVec3f> target;
			NkVector<uint32> hits;
			target.Resize(nv);
			hits.Resize(nv);
			for (uint32 i = 0; i < nv; ++i) {
				target[i] = {0.f, 0.f, 0.f};
				hits[i] = 0;
			}
			if (!p.individual) {
				// RAYON MOYEN : moyenne des distances au centre, comptee UNE fois par
				// sommet soude (sinon les coins dupliques pesent 3x et le rayon derive).
				float64 sum = 0.0;
				uint32 cnt = 0;
				NkVector<uint8> seen;
				seen.Resize(nv);
				for (uint32 i = 0; i < nv; ++i)
					seen[i] = 0;
				for (uint32 i = 0; i < nv; ++i) {
					if (!verts[i].sel)
						continue;
					const uint32 cv = canon[i];
					if (seen[cv])
						continue;
					seen[cv] = 1;
					sum += (float64)(verts[i].pos - p.center).Len();
					cnt++;
				}
				if (cnt == 0)
					return false;
				const float32 R = (float32)(sum / (float64)cnt);
				for (uint32 i = 0; i < nv; ++i) {
					if (!verts[i].sel)
						continue;
					const NkVec3f d = verts[i].pos - p.center;
					const float32 l = d.Len();
					if (l < 1e-8f)
						continue;
					target[i] = p.center + d * (R / l);
					hits[i] = 1;
				}
			} else {
				// PAR ILOT : chaque face entierement selectionnee est spherisee autour de
				// SON barycentre ; un sommet partage prend la MOYENNE de ses cibles.
				NkVector<NkEmId> loop;
				NkVector<NkVec3f> acc;
				NkVector<uint32> acn;
				acc.Resize(nv);
				acn.Resize(nv);
				for (uint32 i = 0; i < nv; ++i) {
					acc[i] = {0.f, 0.f, 0.f};
					acn[i] = 0;
				}
				for (uint32 f = 0; f < (uint32)faces.Size(); ++f) {
					if (!faces[f].alive || !FaceIsSelected(f))
						continue;
					loop.Clear();
					GetFaceVerts(f, loop);
					const uint32 fn = (uint32)loop.Size();
					if (fn < 3)
						continue;
					NkVec3f fc = {0.f, 0.f, 0.f};
					for (uint32 k = 0; k < fn; ++k)
						fc = fc + verts[loop[k]].pos;
					fc = fc * (1.f / (float32)fn);
					float64 sum = 0.0;
					for (uint32 k = 0; k < fn; ++k)
						sum += (float64)(verts[loop[k]].pos - fc).Len();
					const float32 R = (float32)(sum / (float64)fn);
					for (uint32 k = 0; k < fn; ++k) {
						const uint32 vi = canon[loop[k]];
						const NkVec3f d = verts[loop[k]].pos - fc;
						const float32 l = d.Len();
						if (l < 1e-8f)
							continue;
						acc[vi] = acc[vi] + (fc + d * (R / l));
						acn[vi]++;
					}
				}
				for (uint32 i = 0; i < nv; ++i) {
					const uint32 cv = canon[i];
					if (!verts[i].sel || acn[cv] == 0)
						continue;
					target[i] = acc[cv] * (1.f / (float32)acn[cv]);
					hits[i] = 1;
				}
			}
			bool changed = false;
			for (uint32 i = 0; i < nv; ++i) {
				if (!hits[i])
					continue;
				const NkVec3f np = verts[i].pos + (target[i] - verts[i].pos) * p.factor;
				if ((np - verts[i].pos).Len() > 1e-7f)
					changed = true;
				verts[i].pos = np;
			}
			if (changed)
				RecomputeNormals();
			return changed;
		}

		bool NkEditMesh::ShrinkFattenSelected(const NkShrinkFattenParams &p) {
			const uint32 nv = (uint32)verts.Size();
			const uint32 nf = (uint32)faces.Size();
			if (nv == 0 || fabsf(p.offset) < 1e-7f)
				return false;
			NkVector<uint32> canon;
			BuildVertexMerge(canon);
			// Normale par sommet SOUDE = somme des normales de face NON normalisees
			// (donc ponderees par l'aire), accumulee sur le representant du groupe.
			NkVector<NkVec3f> acc;
			acc.Resize(nv);
			for (uint32 i = 0; i < nv; ++i)
				acc[i] = {0.f, 0.f, 0.f};
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < nf; ++f) {
				if (!faces[f].alive)
					continue;
				loop.Clear();
				GetFaceVerts(f, loop);
				if (loop.Size() < 3)
					continue;
				const NkVec3f n = NkEmFaceCross(verts[loop[0]].pos, verts[loop[1]].pos, verts[loop[2]].pos);
				for (uint32 k = 0; k < (uint32)loop.Size(); ++k) {
					const uint32 cv = canon[loop[k]];
					acc[cv] = acc[cv] + n;
				}
			}
			bool changed = false;
			for (uint32 i = 0; i < nv; ++i) {
				if (!verts[i].sel)
					continue;
				const NkVec3f n = acc[canon[i]];
				const float32 l = n.Len();
				if (l < 1e-10f)
					continue;
				verts[i].pos = verts[i].pos + n * (p.offset / l);
				changed = true;
			}
			if (changed)
				RecomputeNormals();
			return changed;
		}

		bool NkMeshEditCommand::Apply(NkEditMesh &m) const {
			// Rejoue la sélection enregistrée sur le maillage courant.
			for (uint32 i = 0; i < m.VertCount(); ++i)
				m.verts[i].sel = 0;
			for (uint32 k = 0; k < (uint32)selection.Size(); ++k) {
				const uint32 vi = selection[k];
				if (vi < m.VertCount())
					m.verts[vi].sel = 1;
			}
			switch (op) {
				case NkMeshEditOp::Extrude:
					return m.ExtrudeSelectedFaces(extrude);
				case NkMeshEditOp::ExtrudeVerts:
					return m.ExtrudeSelectedVertices(extrude);
				case NkMeshEditOp::ExtrudeEdges:
					return m.ExtrudeSelectedEdges(extrude);
				case NkMeshEditOp::Delete:
					return m.DeleteSelectedFaces();
				case NkMeshEditOp::Merge:
					return m.MergeSelectedVerts(merge);
				case NkMeshEditOp::MakeFace:
					// F facon Blender : la MEME touche cree une ARETE avec deux sommets
					// selectionnes, et une FACE a partir de trois. Cette bascule est le
					// comportement de Blender, pas une commodite : avec deux sommets il
					// n'y a pas de face a creer, il y a un segment.
					// C'est ce que l'ancienne structure ne savait pas faire (une arete
					// n'existait qu'a travers ses faces) — cf. Edge / AddWireEdge.
					if (m.MakeEdgeFromSelected())
						return true;
					return m.MakeFaceFromSelected();
				case NkMeshEditOp::Subdivide:
					return m.SubdivideSelectedFaces(subdiv);
				case NkMeshEditOp::LoopCut:
					return m.LoopCutFromSelectedEdge(loopcut);
				case NkMeshEditOp::Bisect:
					return m.BisectByPlane(planePoint, planeNormal, bisectXform);
				case NkMeshEditOp::Bevel:
					return m.BevelSelected(bevel);
				case NkMeshEditOp::Inset:
					return m.InsetSelectedFaces(inset);
				case NkMeshEditOp::EdgeSplit:
					return m.SplitSelectedEdges(esplit);
				case NkMeshEditOp::Spin:
					return m.SpinSelected(spin, spinXform);
				case NkMeshEditOp::Dissolve:
					return m.DissolveSelected(dissolve);
				case NkMeshEditOp::ToSphere:
					return m.ToSphereSelected(tosphere);
				case NkMeshEditOp::ShrinkFatten:
					return m.ShrinkFattenSelected(shrinkfatten);
				case NkMeshEditOp::Move: {
					bool changed = false;
					for (uint32 k = 0; k < (uint32)selection.Size() && k < (uint32)moveDeltas.Size(); ++k) {
						const uint32 vi = selection[k];
						if (vi < m.VertCount()) {
							m.verts[vi].pos = m.verts[vi].pos + moveDeltas[k];
							changed = true;
						}
					}
					if (changed)
						m.RecomputeNormals();
					return changed;
				}
				default:
					return false;
			}
		}

		uint32 NkMeshEditRecorder::ReplayOnto(NkEditMesh &mesh) const {
			uint32 applied = 0;
			for (uint32 i = 0; i < (uint32)mCommands.Size(); ++i)
				if (mCommands[i].Apply(mesh))
					++applied;
			return applied;
		}

		// ── Sérialisation binaire (petit lecteur/écriveur d'octets, little-endian) ──
		namespace {
			struct EmW {
					NkVector<uint8> &b;

					void U8(uint8 v) {
						b.PushBack(v);
					}

					void U32(uint32 v) {
						b.PushBack((uint8)(v & 0xFF));
						b.PushBack((uint8)((v >> 8) & 0xFF));
						b.PushBack((uint8)((v >> 16) & 0xFF));
						b.PushBack((uint8)((v >> 24) & 0xFF));
					}

					void I32(int32 v) {
						U32((uint32)v);
					}

					void F32(float32 v) {
						union {
								float32 f;
								uint32 u;
						} c;

						c.f = v;
						U32(c.u);
					}
			};

			struct EmR {
					const uint8 *d;
					uint32 n;
					uint32 p;
					bool ok;

					EmR(const uint8 *dd, uint32 nn) : d(dd), n(nn), p(0), ok(true) {
					}

					uint8 U8() {
						if (p + 1 > n) {
							ok = false;
							return 0;
						}
						return d[p++];
					}

					uint32 U32() {
						if (p + 4 > n) {
							ok = false;
							return 0;
						}
						uint32 v = (uint32)d[p] | ((uint32)d[p + 1] << 8) | ((uint32)d[p + 2] << 16) |
								   ((uint32)d[p + 3] << 24);
						p += 4;
						return v;
					}

					int32 I32() {
						return (int32)U32();
					}

					float32 F32() {
						union {
								float32 f;
								uint32 u;
						} c;

						c.u = U32();
						return c.f;
					}
			};

			static const uint32 NK_EMREC_MAGIC = 0x4E4D4543u; // "NMEC"
		} // namespace

		void NkMeshEditRecorder::Serialize(NkVector<uint8> &out) const {
			out.Clear();
			EmW w{out};
			w.U32(NK_EMREC_MAGIC);
			w.U32(9u); // v9 : + loopcut.slide (v8 : ToSphere/ShrinkFatten · v7 : dissolve · v6 : spin
					   //       v5 : split · v4 : inset · v3 : bevel · v2 : loopcut.cuts)
			w.U32((uint32)mCommands.Size());
			for (uint32 i = 0; i < (uint32)mCommands.Size(); ++i) {
				const NkMeshEditCommand &c = mCommands[i];
				w.U8((uint8)c.op);
				w.U32((uint32)c.selection.Size());
				for (uint32 k = 0; k < (uint32)c.selection.Size(); ++k)
					w.U32(c.selection[k]);
				w.U8((uint8)(c.extrude.individual ? 1 : 0));
				w.F32(c.extrude.offset);
				w.I32(c.merge.mode);
				w.I32(c.subdiv.cuts);
				w.F32(c.planePoint.x);
				w.F32(c.planePoint.y);
				w.F32(c.planePoint.z);
				w.F32(c.planeNormal.x);
				w.F32(c.planeNormal.y);
				w.F32(c.planeNormal.z);
				for (int32 col = 0; col < 4; ++col)
					for (int32 row = 0; row < 4; ++row)
						w.F32(c.bisectXform[col][row]);
				w.U32((uint32)c.moveDeltas.Size());
				for (uint32 k = 0; k < (uint32)c.moveDeltas.Size(); ++k) {
					w.F32(c.moveDeltas[k].x);
					w.F32(c.moveDeltas[k].y);
					w.F32(c.moveDeltas[k].z);
				}
				w.I32(c.loopcut.cuts); // v2
				w.F32(c.bevel.offset); // v3
				w.I32(c.bevel.segments);
				w.U8((uint8)(c.bevel.vertexOnly ? 1 : 0));
				w.F32(c.inset.thickness); // v4
				w.F32(c.inset.depth);
				w.U8((uint8)(c.inset.individual ? 1 : 0));
				w.F32(c.esplit.gap); // v5
				w.F32(c.spin.center.x); // v6
				w.F32(c.spin.center.y);
				w.F32(c.spin.center.z);
				w.F32(c.spin.axis.x);
				w.F32(c.spin.axis.y);
				w.F32(c.spin.axis.z);
				w.F32(c.spin.angle);
				w.I32(c.spin.steps);
				w.U8((uint8)(c.spin.duplicate ? 1 : 0));
				for (int32 col = 0; col < 4; ++col)
					for (int32 row = 0; row < 4; ++row)
						w.F32(c.spinXform[col][row]);
				w.I32(c.dissolve.mode); // v7
				w.F32(c.tosphere.center.x); // v8
				w.F32(c.tosphere.center.y);
				w.F32(c.tosphere.center.z);
				w.F32(c.tosphere.factor);
				w.U8((uint8)(c.tosphere.individual ? 1 : 0));
				w.F32(c.shrinkfatten.offset);
				w.F32(c.loopcut.slide); // v9
			}
		}

		// =====================================================================
		// STACK DE MODIFICATEURS — Mirror / Array / Subsurf (non-destructif)
		// =====================================================================
		// ── OUTILS COMMUNS AUX MODIFICATEURS ────────────────────────────────────
		// Un modificateur travaille sur le maillage ENTIER. Ceux qui reutilisent une
		// operation d'edition (qui, elle, agit sur la SELECTION) doivent donc tout
		// selectionner puis restaurer — sans quoi le resultat dependrait de ce que
		// l'utilisateur avait clique avant, ce qui n'aurait aucun sens pour une pile
		// non destructive rejouee a chaque frame.
		static NkVector<uint8> NkEmSaveSel(const NkEditMesh &m) {
			NkVector<uint8> s;
			s.Resize(m.VertCount());
			for (uint32 i = 0; i < m.VertCount(); ++i)
				s[i] = m.verts[i].sel;
			return s;
		}
		static void NkEmRestoreSel(NkEditMesh &m, const NkVector<uint8> &s) {
			// La topologie a pu changer : on ne restaure que si la taille correspond,
			// sinon on repart d'une selection vide plutot que de reaffecter au hasard.
			if ((uint32)s.Size() == m.VertCount())
				m.SetVertSelection(s.Data(), (uint32)s.Size());
			else
				m.SelectNone();
		}

		// SOLIDIFIER : construit une SECONDE coque decalee le long des normales, et
		// (option) referme le BORD entre les deux. Sans la bordure, une surface
		// ouverte donnerait deux nappes separees — visuellement une epaisseur, mais un
		// maillage non ferme, ce qui casse tout ce qui suit (booleen, impression 3D).
		static void NkEmModSolidify(NkEditMesh &m, const NkMeshModifier &p) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// ATTRIBUTS PAR FACE. Solidify DOUBLE une surface : la coque externe et la
			// coque interne sont deux copies de la meme face, elles en heritent donc par
			// IDENTITE. Seule la tranche de bord est une face NEUVE.
			NkVector<NkEditMesh::FaceAttrib> fa;
			m.ToPolygons(pv, fs, fv, &fa);
			const uint32 vc = (uint32)pv.Size();
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			if (vc == 0 || fc == 0 || p.solidifyThickness <= 0.f)
				return;
			// offset -1 = tout vers l'interieur, +1 = tout vers l'exterieur, 0 = moitie
			// de chaque cote (convention Blender).
			const float32 o = (p.solidifyOffset < -1.f) ? -1.f : (p.solidifyOffset > 1.f ? 1.f : p.solidifyOffset);
			const float32 outAmt = p.solidifyThickness * (1.f + o) * 0.5f;
			const float32 inAmt = p.solidifyThickness * (1.f - o) * 0.5f;
			NkVector<NkVertex3D> ov;
			ov.Resize(vc * 2u);
			for (uint32 i = 0; i < vc; ++i) {
				ov[i] = pv[i];
				ov[i].pos = pv[i].pos + pv[i].normal * outAmt;
				ov[vc + i] = pv[i];
				ov[vc + i].pos = pv[i].pos - pv[i].normal * inAmt;
				ov[vc + i].normal = pv[i].normal * -1.f; // la coque interne regarde dedans
			}
			NkVector<uint32> nfs, nfv;
			NkVector<NkEditMesh::FaceAttrib> nfa;
			nfs.PushBack(0);
			auto attrDe = [&](uint32 f) -> NkEditMesh::FaceAttrib {
				return (f < (uint32)fa.Size()) ? fa[f] : NkEditMesh::FaceAttrib{};
			};
			for (uint32 f = 0; f < fc; ++f) { // coque externe
				for (uint32 k = fs[f]; k < fs[f + 1]; ++k)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
				nfa.PushBack(attrDe(f));
			}
			for (uint32 f = 0; f < fc; ++f) { // coque interne, winding INVERSE
				const uint32 s0 = fs[f], s1 = fs[f + 1];
				for (uint32 k = s1; k > s0; --k)
					nfv.PushBack(vc + fv[k - 1]);
				nfs.PushBack((uint32)nfv.Size());
				// La coque interne DOUBLE la face externe : meme materiau, meme ombrage.
				nfa.PushBack(attrDe(f));
			}
			if (p.solidifyRim) {
				// BORD = arete portee par UNE seule face (identite soudee). C'est la
				// seule facon de savoir ou la surface s'arrete.
				NkVector<uint32> canon;
				m.BuildVertexMerge(canon);
				auto CN = [&](uint32 v) { return (v < (uint32)canon.Size()) ? canon[v] : v; };
				NkHashMap<uint64, uint32> cnt;
				for (uint32 f = 0; f < fc; ++f) {
					const uint32 s0 = fs[f], s1 = fs[f + 1], n = s1 - s0;
					for (uint32 k = 0; k < n; ++k) {
						const uint32 a = CN(fv[s0 + k]), b = CN(fv[s0 + (k + 1) % n]);
						if (a == b)
							continue;
						const uint64 lo = a < b ? a : b, hi = a < b ? b : a;
						const uint64 key = (lo << 32) | hi;
						uint32 *e = cnt.Find(key);
						if (e)
							(*e)++;
						else
							cnt.InsertOrAssign(key, 1u);
					}
				}
				for (uint32 f = 0; f < fc; ++f) {
					const uint32 s0 = fs[f], s1 = fs[f + 1], n = s1 - s0;
					for (uint32 k = 0; k < n; ++k) {
						const uint32 ia = fv[s0 + k], ib = fv[s0 + (k + 1) % n];
						const uint32 a = CN(ia), b = CN(ib);
						if (a == b)
							continue;
						const uint64 lo = a < b ? a : b, hi = a < b ? b : a;
						const uint32 *e = cnt.Find((lo << 32) | hi);
						if (!e || *e != 1u)
							continue; // arete interieure : pas de bordure a creer
						nfv.PushBack(ib);
						nfv.PushBack(ia);
						nfv.PushBack(vc + ia);
						nfv.PushBack(vc + ib);
						nfs.PushBack((uint32)nfv.Size());
						// TRANCHE DE BORD : une face NEUVE, donc la regle des faces sans mere.
						// Ses deux voisines sont la coque externe et la coque interne de la MEME
						// face d'origine, ponderees par le meme segment de contour.
						// ⚠ AUCUN COMPTEUR DE PERTE N'EST EXPOSE ICI, et c'est deliberé : les
						// deux voisines portent le meme materiau PAR CONSTRUCTION, donc la perte
						// ne peut que valoir zero. Un compteur qui ne peut pas etre non nul
						// n'atteste rien — on ecrit la raison plutot qu'un zero rassurant.
						{
							const NkEditMesh::FaceAttrib at = attrDe(f);
							const uint16 mats[2] = {at.material, at.material};
							const uint8 sms[2] = {at.smooth, at.smooth};
							const float32 lg = (ov[ia].pos - ov[ib].pos).Len();
							const float32 poids[2] = {lg, lg};
							nfa.PushBack(EM_AttribFromNeighbours(mats, sms, poids, 2u, nullptr));
						}
					}
				}
			}
			{
				const uint32 nfc = (uint32)nfs.Size() - 1u;
				m.BuildFromPolygons(ov.Data(), (uint32)ov.Size(), nfs.Data(), nfc, nfv.Data(),
							    (nfa.Size() == nfc) ? nfa.Data() : nullptr);
			}
			m.RecomputeNormals();
			m.RebuildEdges();
		}

		// BUILD (proportion de faces) et MASK (faces selectionnees) : meme mecanique,
		// seul le CRITERE de conservation change. Les mettre en commun evite deux
		// reconstructions de polygones qui divergeraient a la premiere correction.
		static void NkEmModFaceSubset(NkEditMesh &m, const NkMeshModifier &p, bool byRatio) {
			NkVector<NkVertex3D> pv;
			NkVector<uint32> fs, fv;
			// ATTRIBUTS PAR FACE : Build et Mask SELECTIONNENT des faces, ils n'en
			// creent aucune. Heritage par IDENTITE, comme Mirror et Array.
			NkVector<NkEditMesh::FaceAttrib> fa;
			m.ToPolygons(pv, fs, fv, &fa);
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			if (fc == 0)
				return;
			uint32 keepN = fc;
			if (byRatio) {
				float32 r = p.buildRatio;
				r = (r < 0.f) ? 0.f : (r > 1.f ? 1.f : r);
				keepN = (uint32)((float32)fc * r + 0.5f);
			}
			NkVector<uint32> nfs, nfv;
			NkVector<NkEditMesh::FaceAttrib> nfa;
			nfs.PushBack(0);
			for (uint32 f = 0; f < fc; ++f) {
				bool keep;
				if (byRatio) {
					keep = (f < keepN);
				} else {
					// MASK : une face est retenue si TOUS ses sommets sont selectionnes
					// — meme convention que le remplissage orange de l'editeur, pour que
					// ce qu'on voit selectionne soit exactement ce qui reste.
					keep = true;
					for (uint32 k = fs[f]; k < fs[f + 1]; ++k)
						if (fv[k] >= m.VertCount() || !m.verts[fv[k]].sel) {
							keep = false;
							break;
						}
					if (p.maskInvert)
						keep = !keep;
				}
				if (!keep)
					continue;
				for (uint32 k = fs[f]; k < fs[f + 1]; ++k)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
				nfa.PushBack(f < (uint32)fa.Size() ? fa[f] : NkEditMesh::FaceAttrib{});
			}
			if (nfs.Size() < 2) {
				// Tout retirer donnerait un maillage vide et une scene qui semble avoir
				// disparu ; on prefere ne rien faire, l'utilisateur voit alors que le
				// reglage est a l'extreme.
				return;
			}
			{
				const uint32 nfc = (uint32)nfs.Size() - 1u;
				m.BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), nfc, nfv.Data(),
							    (nfa.Size() == nfc) ? nfa.Data() : nullptr);
			}
			m.RebuildEdges();
		}

		// DEFORMATIONS PURES : Cast / SimpleDeform / Smooth / Wave. Elles ne touchent
		// QUE les positions — la topologie est intacte, donc pas de reconstruction.
		static void NkEmModDeform(NkEditMesh &m, const NkMeshModifier &p) {
			const uint32 vc = m.VertCount();
			if (vc == 0)
				return;
			// Repere : centre et rayon moyen du maillage. Un rayon impose a zero ferait
			// imploser le modele, ce qui n'est jamais l'intention -> repli automatique.
			NkVec3f c{0.f, 0.f, 0.f};
			for (uint32 i = 0; i < vc; ++i)
				c = c + m.verts[i].pos;
			c = c * (1.f / (float32)vc);
			float32 rAvg = 0.f, ymin = 1e30f, ymax = -1e30f;
			const int32 ax = (p.type == NkModifierType::Wave) ? p.waveAxis : p.deformAxis;
			auto comp = [&](const NkVec3f &v, int32 a) { return (a == 0) ? v.x : (a == 1 ? v.y : v.z); };
			for (uint32 i = 0; i < vc; ++i) {
				rAvg += (m.verts[i].pos - c).Len();
				const float32 t = comp(m.verts[i].pos - c, ax);
				if (t < ymin)
					ymin = t;
				if (t > ymax)
					ymax = t;
			}
			rAvg /= (float32)vc;
			const float32 span = (ymax - ymin) > 1e-6f ? (ymax - ymin) : 1.f;

			if (p.type == NkModifierType::Smooth) {
				// Relaxation laplacienne sur l'identite SOUDEE : sans soudure, un cube
				// aux sommets dupliques n'aurait aucun voisin et rien ne bougerait.
				NkVector<uint32> pairs;
				m.GetUniqueEdges(pairs);
				NkVector<uint32> canon;
				m.BuildVertexMerge(canon);
				auto CN = [&](uint32 v) { return (v < (uint32)canon.Size()) ? canon[v] : v; };
				const int32 rep = (p.smoothRepeat < 1) ? 1 : p.smoothRepeat;
				for (int32 it = 0; it < rep; ++it) {
					NkVector<NkVec3f> sum;
					NkVector<uint32> deg;
					sum.Resize(vc);
					deg.Resize(vc);
					for (uint32 i = 0; i < vc; ++i) {
						sum[i] = NkVec3f{0.f, 0.f, 0.f};
						deg[i] = 0;
					}
					for (uint32 e = 0; e + 1 < (uint32)pairs.Size(); e += 2) {
						const uint32 a = CN(pairs[e]), b = CN(pairs[e + 1]);
						if (a >= vc || b >= vc)
							continue;
						sum[a] = sum[a] + m.verts[b].pos;
						deg[a]++;
						sum[b] = sum[b] + m.verts[a].pos;
						deg[b]++;
					}
					NkVector<NkVec3f> np;
					np.Resize(vc);
					for (uint32 i = 0; i < vc; ++i) {
						const uint32 r = CN(i);
						np[i] = (r < vc && deg[r] > 0)
									? m.verts[i].pos + (sum[r] * (1.f / (float32)deg[r]) - m.verts[r].pos) * p.smoothFactor
									: m.verts[i].pos;
					}
					for (uint32 i = 0; i < vc; ++i)
						m.verts[i].pos = np[i];
				}
				m.RecomputeNormals();
				return;
			}

			for (uint32 i = 0; i < vc; ++i) {
				NkVec3f v = m.verts[i].pos - c;
				if (p.type == NkModifierType::Cast) {
					const float32 R = (p.castRadius > 1e-6f) ? p.castRadius : rAvg;
					NkVec3f tgt = v;
					if (p.castType == 0) { // SPHERE
						const float32 l = v.Len();
						tgt = (l > 1e-6f) ? v * (R / l) : v;
					} else if (p.castType == 1) { // CYLINDRE : rayon dans le plan normal a l'axe
						NkVec3f r = v;
						if (ax == 0)
							r.x = 0.f;
						else if (ax == 1)
							r.y = 0.f;
						else
							r.z = 0.f;
						const float32 l = r.Len();
						if (l > 1e-6f) {
							const NkVec3f s = r * (R / l);
							tgt = v + (s - r);
						}
					} else { // CUBE : projection sur la face dominante
						const float32 axv = v.x < 0.f ? -v.x : v.x, ayv = v.y < 0.f ? -v.y : v.y,
									  azv = v.z < 0.f ? -v.z : v.z;
						const float32 mx = (axv > ayv ? (axv > azv ? axv : azv) : (ayv > azv ? ayv : azv));
						if (mx > 1e-6f)
							tgt = v * (R / mx);
					}
					v = v + (tgt - v) * p.castFactor;
				} else if (p.type == NkModifierType::SimpleDeform) {
					const float32 t = (comp(v, ax) - ymin) / span; // 0..1 le long de l'axe
					if (p.deformMode == 0) { // TORSION
						const float32 a = p.deformAngle * 3.14159265f / 180.f * t;
						const float32 ca = cosf(a), sa = sinf(a);
						if (ax == 1) {
							const float32 x = v.x * ca - v.z * sa, z = v.x * sa + v.z * ca;
							v.x = x;
							v.z = z;
						} else if (ax == 0) {
							const float32 y = v.y * ca - v.z * sa, z = v.y * sa + v.z * ca;
							v.y = y;
							v.z = z;
						} else {
							const float32 x = v.x * ca - v.y * sa, y = v.x * sa + v.y * ca;
							v.x = x;
							v.y = y;
						}
					} else if (p.deformMode == 1) { // COURBURE
						const float32 a = p.deformAngle * 3.14159265f / 180.f * (t - 0.5f);
						const float32 ca = cosf(a), sa = sinf(a);
						if (ax == 1) {
							const float32 x = v.x * ca - v.y * sa, y = v.x * sa + v.y * ca;
							v.x = x;
							v.y = y;
						} else {
							const float32 y = v.y * ca - v.z * sa, z = v.y * sa + v.z * ca;
							v.y = y;
							v.z = z;
						}
					} else if (p.deformMode == 2) { // EFFILEMENT
						const float32 k = 1.f + p.deformFactor * (t - 0.5f) * 2.f;
						if (ax == 1) {
							v.x *= k;
							v.z *= k;
						} else if (ax == 0) {
							v.y *= k;
							v.z *= k;
						} else {
							v.x *= k;
							v.y *= k;
						}
					} else { // ETIREMENT : allonge sur l'axe, retrecit autour (volume ~ conserve)
						const float32 k = 1.f + p.deformFactor;
						const float32 inv = (k > 1e-4f) ? 1.f / sqrtf(k) : 1.f;
						if (ax == 1) {
							v.y *= k;
							v.x *= inv;
							v.z *= inv;
						} else if (ax == 0) {
							v.x *= k;
							v.y *= inv;
							v.z *= inv;
						} else {
							v.z *= k;
							v.x *= inv;
							v.y *= inv;
						}
					}
				} else if (p.type == NkModifierType::Wave) {
					// Onde RADIALE dans le plan perpendiculaire a l'axe : c'est la forme
					// de Blender (rides concentriques), pas une sinusoide le long d'un axe.
					NkVec3f r = v;
					if (ax == 0)
						r.x = 0.f;
					else if (ax == 1)
						r.y = 0.f;
					else
						r.z = 0.f;
					const float32 d = r.Len();
					const float32 w = (p.waveWidth > 1e-4f) ? p.waveWidth : 1e-4f;
					const float32 h = p.waveHeight * sinf(d / w - p.wavePhase);
					if (ax == 0)
						v.x += h;
					else if (ax == 1)
						v.y += h;
					else
						v.z += h;
				}
				m.verts[i].pos = c + v;
			}
			m.RecomputeNormals();
		}

		// OMBRAGE PAR ANGLE (« Auto Smooth ») : une face est LISSE si toutes ses
		// aretes partagees font un angle diedre inferieur au seuil. Au-dela, l'arete
		// doit rester franche — c'est ce qui distingue un cylindre (cotes lisses,
		// couvercles francs) d'une capsule.
		static void NkEmModAutoSmooth(NkEditMesh &m, const NkMeshModifier &p) {
			const float32 cosLim = cosf(p.autoSmoothAngle * 3.14159265f / 180.f);
			// On passe par le CYCLE RADIAL (BMesh etape 2) et non par GetUniqueEdges +
			// EdgeFaces(a, b, ...). Mesure du defaut de la premiere version : sur un
			// cube, toutes les faces ressortaient LISSES quel que soit le seuil — la
			// recherche par paire de sommets ne retrouvait pas les deux faces (sommets
			// dupliques par face), donc aucune arete n'etait jamais jugee vive et le
			// reglage n'avait aucun effet. Le cycle radial, lui, PORTE les faces.
			m.RebuildEdges();
			const uint32 fc = (uint32)m.faces.Size();
			NkVector<uint8> sharp;
			sharp.Resize(fc);
			for (uint32 f = 0; f < fc; ++f)
				sharp[f] = 0;
			NkVector<NkEmId> fs;
			for (uint32 e = 0; e < (uint32)m.edges.Size(); ++e) {
				if (!m.edges[e].alive)
					continue;
				if (m.EdgeFaces((NkEmId)e, fs) != 2)
					continue; // bord, filaire ou non manifold : pas d angle diedre defini
				const NkEmId f0 = fs[0], f1 = fs[1];
				if (f0 >= fc || f1 >= fc)
					continue;
				if (m.faces[f0].normal.Dot(m.faces[f1].normal) < cosLim) {
					sharp[f0] = 1;
					sharp[f1] = 1;
				}
			}
			for (uint32 f = 0; f < fc; ++f)
				if (m.faces[f].alive)
					m.faces[f].smooth = sharp[f] ? (uint8)0 : (uint8)1;
			m.RecomputeNormals();
		}

		void NkMeshModifier::Apply(NkEditMesh &m) const {
			if (!enabled)
				return;
			// ── LOT DE MODIFICATEURS ────────────────────────────────────────────
			// Chacun traite le maillage ENTIER : un modificateur n'a pas de notion de
			// selection utilisateur (sauf Mask, dont c'est justement l'objet). Ceux
			// qui reutilisent une operation d'edition selectionnent donc TOUT d'abord,
			// puis restaurent — sinon le resultat dependrait de ce que l'utilisateur
			// avait clique avant d'ajouter le modificateur, ce qui serait imprevisible.
			switch (type) {
				case NkModifierType::Triangulate: {
					NkVector<NkVertex3D> tv;
					NkVector<uint32> ti;
					NkVector<NkEmId> tf;
					m.Triangulate(tv, ti, tf);
					// minVerts : les faces plus PETITES sont laissees telles quelles.
					// Sans ce garde, « trianguler » a partir de 5 cotes decouperait
					// quand meme les quads — le reglage n'aurait aucun effet.
					if (triangulateMinVerts > 3) {
						NkVector<NkVertex3D> pv;
						NkVector<uint32> fs, fv;
						// IDENTITE : trianguler DECOUPE une face, ca n'en cree pas de neuve. Les
						// triangles d'un eventail sortent tous du MEME n-gon et en heritent.
						NkVector<NkEditMesh::FaceAttrib> fa;
						m.ToPolygons(pv, fs, fv, &fa);
						const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
						NkVector<uint32> nfs, nfv;
						NkVector<NkEditMesh::FaceAttrib> nfa;
						nfs.PushBack(0);
						for (uint32 f = 0; f < fc; ++f) {
							const uint32 s0 = fs[f], s1 = fs[f + 1], n = s1 - s0;
							const NkEditMesh::FaceAttrib at = (f < (uint32)fa.Size()) ? fa[f] : NkEditMesh::FaceAttrib{};
							if ((int32)n < triangulateMinVerts) {
								for (uint32 k = s0; k < s1; ++k)
									nfv.PushBack(fv[k]);
								nfs.PushBack((uint32)nfv.Size());
								nfa.PushBack(at);
								continue;
							}
							for (uint32 k = 1; k + 1 < n; ++k) { // eventail
								nfv.PushBack(fv[s0]);
								nfv.PushBack(fv[s0 + k]);
								nfv.PushBack(fv[s0 + k + 1]);
								nfs.PushBack((uint32)nfv.Size());
								nfa.PushBack(at);
							}
						}
						if (nfs.Size() > 1) {
							const uint32 nfc = (uint32)nfs.Size() - 1u;
							m.BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), nfc, nfv.Data(),
												(nfa.Size() == nfc) ? nfa.Data() : nullptr);
						}
					} else if (!tv.Empty() && !ti.Empty()) {
						// IDENTITE, par une parente qui existait DEJA : `tf` donne le n-gon
						// d'origine de chaque triangle - elle sert au picking. Il n'y avait
						// qu'a la lire. Sans ca, ce chemin repeignait tout en slot 0.
						NkVector<uint16> tm;
						const uint32 triN = (uint32)ti.Size() / 3u;
						tm.Resize(triN);
						for (uint32 t = 0; t < triN; ++t) {
							const NkEmId sf = (t < (uint32)tf.Size()) ? tf[t] : NK_EM_INVALID;
							tm[t] = (sf != NK_EM_INVALID && sf < (NkEmId)m.faces.Size()) ? m.faces[sf].material
																					 : (uint16)0;
						}
						m.BuildFromIndexed(tv.Data(), (uint32)tv.Size(), ti.Data(), (uint32)ti.Size(), false,
									   tm.Data());
					}
					m.RebuildEdges();
					return;
				}
				case NkModifierType::Weld: {
					const NkVector<uint8> keep = NkEmSaveSel(m);
					m.SelectAll();
					NkMergeParams mp;
					mp.mode = NkMergeParams::ByDistance;
					mp.distance = (weldDistance > 0.f) ? weldDistance : 1e-4f;
					m.MergeSelectedVerts(mp);
					NkEmRestoreSel(m, keep);
					m.RebuildEdges();
					return;
				}
				case NkModifierType::Bevel: {
					const NkVector<uint8> keep = NkEmSaveSel(m);
					m.SelectAll();
					NkBevelParams bp;
					bp.offset = bevelWidth;
					bp.segments = (bevelSegments < 1) ? 1 : bevelSegments;
					m.BevelSelected(bp);
					NkEmRestoreSel(m, keep);
					m.RebuildEdges();
					return;
				}
				case NkModifierType::EdgeSplit: {
					const NkVector<uint8> keep = NkEmSaveSel(m);
					m.SelectAll();
					NkEdgeSplitParams ep;
					m.SplitSelectedEdges(ep);
					NkEmRestoreSel(m, keep);
					m.RebuildEdges();
					return;
				}
				case NkModifierType::Decimate: {
					const NkVector<uint8> keep = NkEmSaveSel(m);
					m.SelectAll();
					NkDissolveParams dp;
					m.DissolveSelected(dp);
					NkEmRestoreSel(m, keep);
					m.RebuildEdges();
					return;
				}
				case NkModifierType::Screw: {
					const NkVector<uint8> keep = NkEmSaveSel(m);
					m.SelectAll();
					NkSpinParams sp;
					sp.steps = (screwSteps < 2) ? 2 : screwSteps;
					sp.angle = screwAngle * 3.14159265f / 180.f;
					sp.duplicate = true;
					sp.axis = (screwAxis == 0) ? NkVec3f{1.f, 0.f, 0.f}
								: (screwAxis == 2) ? NkVec3f{0.f, 0.f, 1.f}
												   : NkVec3f{0.f, 1.f, 0.f};
					sp.center = {0.f, 0.f, 0.f};
					m.SpinSelected(sp, NkMat4f::Identity());
					NkEmRestoreSel(m, keep);
					m.RebuildEdges();
					return;
				}
				case NkModifierType::Solidify: NkEmModSolidify(m, *this); return;
				case NkModifierType::Build: NkEmModFaceSubset(m, *this, true); return;
				case NkModifierType::Mask: NkEmModFaceSubset(m, *this, false); return;
				case NkModifierType::Cast:
				case NkModifierType::SimpleDeform:
				case NkModifierType::Smooth:
				case NkModifierType::Wave: NkEmModDeform(m, *this); return;
				case NkModifierType::SmoothByAngle: NkEmModAutoSmooth(m, *this); return;
				default: break;
			}
			if (type == NkModifierType::Subsurf) {
				const int32 lv = (subsurfLevels < 1) ? 1 : subsurfLevels;
				if (!subsurfSimple) {
					// CATMULL-CLARK : le vrai lissage. C'etait la lacune — le
					// modificateur appelait SubdivideSelectedFaces, une subdivision
					// LINEAIRE : elle densifie le maillage sans DEPLACER un seul sommet,
					// donc un cube subdivise restait un cube. Ce n'est pas le
					// modificateur « Subdivision Surface » de Blender, c'est son mode
					// « Simple » — desormais accessible par subsurfSimple.
					m.SubdivideCatmullClark(lv);
					return;
				}
				for (uint32 i = 0; i < m.VertCount(); ++i)
					m.verts[i].sel = 0; // aucune sél. -> TOUT
				NkSubdivideParams p;
				p.cuts = lv;
				m.SubdivideSelectedFaces(p);
				return;
			}
			// Mirror & Array travaillent en représentation polygones (CSR).
			NkVector<NkVertex3D> base;
			NkVector<uint32> fs, fv;
			// ATTRIBUTS PAR FACE : Mirror et Array COPIENT des faces, ils n'en creent
			// aucune. L'heritage est donc l'IDENTITE : chaque copie garde le materiau
			// et l'ombrage de son originale. Sans ce `&fa`, appliquer un modificateur
			// repeignait TOUT le maillage en slot 0, en silence, jusqu'au rendu.
			NkVector<NkEditMesh::FaceAttrib> fa;
			m.ToPolygons(base, fs, fv, &fa);
			const uint32 baseVC = (uint32)base.Size();
			const uint32 fc = (fs.Size() > 0) ? (uint32)fs.Size() - 1 : 0;
			if (baseVC == 0 || fc == 0)
				return;

			if (type == NkModifierType::Mirror) {
				NkVector<NkVertex3D> pv = base; // sortie sommets (base + miroir)
				NkVector<int32> mir;
				mir.Resize(baseVC);
				for (uint32 i = 0; i < baseVC; i++) {
					const float32 co = (mirrorAxis == 0)   ? base[i].pos.x
									   : (mirrorAxis == 1) ? base[i].pos.y
														   : base[i].pos.z;
					const float32 aco = (co < 0.f) ? -co : co;
					if (mirrorMerge && aco <= mirrorMergeDist) {
						mir[i] = (int32)i;
					} // sur le plan -> soudé
					else {
						NkVertex3D v = base[i];
						if (mirrorAxis == 0) {
							v.pos.x = -v.pos.x;
							v.normal.x = -v.normal.x;
						} else if (mirrorAxis == 1) {
							v.pos.y = -v.pos.y;
							v.normal.y = -v.normal.y;
						} else {
							v.pos.z = -v.pos.z;
							v.normal.z = -v.normal.z;
						}
						mir[i] = (int32)pv.Size();
						pv.PushBack(v);
					}
				}
				NkVector<uint32> nfs, nfv;
				NkVector<NkEditMesh::FaceAttrib> nfa;
				nfs.PushBack(0);
				auto attrDe = [&](uint32 f) -> NkEditMesh::FaceAttrib {
					return (f < (uint32)fa.Size()) ? fa[f] : NkEditMesh::FaceAttrib{};
				};
				for (uint32 f = 0; f < fc; f++) {
					for (uint32 k = fs[f]; k < fs[f + 1]; k++)
						nfv.PushBack(fv[k]);
					nfs.PushBack((uint32)nfv.Size());
					nfa.PushBack(attrDe(f));
				}
				for (uint32 f = 0; f < fc; f++) {
					const uint32 s = fs[f], e = fs[f + 1]; // faces miroir : winding inversé
					for (uint32 k = e; k > s; --k)
						nfv.PushBack((uint32)mir[fv[k - 1]]);
					nfs.PushBack((uint32)nfv.Size());
					// La face miroir est la MEME face, retournee : meme materiau.
					nfa.PushBack(attrDe(f));
				}
				{
					const uint32 nfc = (uint32)nfs.Size() - 1u;
					m.BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), nfc, nfv.Data(),
								    (nfa.Size() == nfc) ? nfa.Data() : nullptr);
				}
				return;
			}

			// Array
			int32 cnt = (arrayCount < 1) ? 1 : arrayCount;
			if (cnt < 2)
				return;
			NkVector<NkVertex3D> pv = base;
			NkVector<uint32> nfs, nfv;
			NkVector<NkEditMesh::FaceAttrib> nfa;
			nfs.PushBack(0);
			auto attrDe = [&](uint32 f) -> NkEditMesh::FaceAttrib {
				return (f < (uint32)fa.Size()) ? fa[f] : NkEditMesh::FaceAttrib{};
			};
			for (uint32 f = 0; f < fc; f++) {
				for (uint32 k = fs[f]; k < fs[f + 1]; k++)
					nfv.PushBack(fv[k]);
				nfs.PushBack((uint32)nfv.Size());
				nfa.PushBack(attrDe(f));
			}
			for (int32 c = 1; c < cnt; c++) {
				const uint32 voff = (uint32)pv.Size();
				for (uint32 i = 0; i < baseVC; i++) {
					NkVertex3D v = base[i];
					v.pos = v.pos + arrayOffset * (float32)c;
					pv.PushBack(v);
				}
				for (uint32 f = 0; f < fc; f++) {
					for (uint32 k = fs[f]; k < fs[f + 1]; k++)
						nfv.PushBack(fv[k] + voff);
					nfs.PushBack((uint32)nfv.Size());
					// Chaque exemplaire est une COPIE : meme materiau que son originale.
					nfa.PushBack(attrDe(f));
				}
			}
			{
				const uint32 nfc = (uint32)nfs.Size() - 1u;
				m.BuildFromPolygons(pv.Data(), (uint32)pv.Size(), nfs.Data(), nfc, nfv.Data(),
							    (nfa.Size() == nfc) ? nfa.Data() : nullptr);
			}
		}

		// ── TABLES DE PARAMETRES ────────────────────────────────────────────────
		// Les noms sont des CLES : ne jamais les renommer une fois publies, une courbe
		// d'animation ou un fichier enregistre les designerait encore.
		static const NkModParam kParamsMirror[] = {
			{"mirror_axis", "Axe", NkModParamType::Int, offsetof(NkMeshModifier, mirrorAxis), 0.f, 2.f},
			{"mirror_merge", "Souder au plan", NkModParamType::Bool, offsetof(NkMeshModifier, mirrorMerge), 0.f, 1.f},
			{"mirror_merge_dist", "Distance de soudure", NkModParamType::Float,
			 offsetof(NkMeshModifier, mirrorMergeDist), 0.f, 1.f},
		};
		static const NkModParam kParamsArray[] = {
			{"array_count", "Nombre", NkModParamType::Int, offsetof(NkMeshModifier, arrayCount), 1.f, 256.f},
			{"array_offset", "Decalage", NkModParamType::Vec3, offsetof(NkMeshModifier, arrayOffset), 0.f, 0.f},
		};
		static const NkModParam kParamsSubsurf[] = {
			{"subsurf_levels", "Niveaux", NkModParamType::Int, offsetof(NkMeshModifier, subsurfLevels), 0.f, 6.f},
			{"subsurf_simple", "Simple (lineaire)", NkModParamType::Bool, offsetof(NkMeshModifier, subsurfSimple), 0.f,
			 1.f},
		};

		static const NkModParam kParamsSolidify[] = {
			{"solidify_thickness", "Epaisseur", NkModParamType::Float, offsetof(NkMeshModifier, solidifyThickness),
			 0.f, 2.f},
			{"solidify_offset", "Decalage", NkModParamType::Float, offsetof(NkMeshModifier, solidifyOffset), -1.f, 1.f},
			{"solidify_rim", "Fermer le bord", NkModParamType::Bool, offsetof(NkMeshModifier, solidifyRim), 0.f, 1.f},
		};
		static const NkModParam kParamsTriangulate[] = {
			{"triangulate_min_verts", "Cotes minimum", NkModParamType::Int,
			 offsetof(NkMeshModifier, triangulateMinVerts), 3.f, 16.f},
		};
		static const NkModParam kParamsWeld[] = {
			{"weld_distance", "Distance", NkModParamType::Float, offsetof(NkMeshModifier, weldDistance), 0.f, 1.f},
		};
		static const NkModParam kParamsBevel[] = {
			{"bevel_width", "Largeur", NkModParamType::Float, offsetof(NkMeshModifier, bevelWidth), 0.f, 1.f},
			{"bevel_segments", "Segments", NkModParamType::Int, offsetof(NkMeshModifier, bevelSegments), 1.f, 12.f},
		};
		static const NkModParam kParamsScrew[] = {
			{"screw_steps", "Pas", NkModParamType::Int, offsetof(NkMeshModifier, screwSteps), 2.f, 128.f},
			{"screw_angle", "Angle", NkModParamType::Float, offsetof(NkMeshModifier, screwAngle), -720.f, 720.f},
			{"screw_height", "Hauteur", NkModParamType::Float, offsetof(NkMeshModifier, screwHeight), -10.f, 10.f},
			{"screw_axis", "Axe", NkModParamType::Int, offsetof(NkMeshModifier, screwAxis), 0.f, 2.f},
		};
		static const NkModParam kParamsEdgeSplit[] = {
			{"edge_split_angle", "Angle", NkModParamType::Float, offsetof(NkMeshModifier, edgeSplitAngle), 0.f, 180.f},
		};
		static const NkModParam kParamsDecimate[] = {
			{"decimate_angle", "Angle planaire", NkModParamType::Float, offsetof(NkMeshModifier, decimateAngle), 0.f,
			 90.f},
		};
		static const NkModParam kParamsBuild[] = {
			{"build_ratio", "Proportion", NkModParamType::Float, offsetof(NkMeshModifier, buildRatio), 0.f, 1.f},
		};
		static const NkModParam kParamsMask[] = {
			{"mask_invert", "Inverser", NkModParamType::Bool, offsetof(NkMeshModifier, maskInvert), 0.f, 1.f},
		};
		static const NkModParam kParamsCast[] = {
			{"cast_type", "Forme", NkModParamType::Int, offsetof(NkMeshModifier, castType), 0.f, 2.f},
			{"cast_factor", "Facteur", NkModParamType::Float, offsetof(NkMeshModifier, castFactor), -2.f, 2.f},
			{"cast_radius", "Rayon", NkModParamType::Float, offsetof(NkMeshModifier, castRadius), 0.f, 10.f},
		};
		static const NkModParam kParamsSimpleDeform[] = {
			{"deform_mode", "Mode", NkModParamType::Int, offsetof(NkMeshModifier, deformMode), 0.f, 3.f},
			{"deform_angle", "Angle", NkModParamType::Float, offsetof(NkMeshModifier, deformAngle), -360.f, 360.f},
			{"deform_factor", "Facteur", NkModParamType::Float, offsetof(NkMeshModifier, deformFactor), -2.f, 2.f},
			{"deform_axis", "Axe", NkModParamType::Int, offsetof(NkMeshModifier, deformAxis), 0.f, 2.f},
		};
		static const NkModParam kParamsSmooth[] = {
			{"smooth_factor", "Facteur", NkModParamType::Float, offsetof(NkMeshModifier, smoothFactor), 0.f, 1.f},
			{"smooth_repeat", "Repetitions", NkModParamType::Int, offsetof(NkMeshModifier, smoothRepeat), 1.f, 20.f},
		};
		static const NkModParam kParamsWave[] = {
			{"wave_height", "Hauteur", NkModParamType::Float, offsetof(NkMeshModifier, waveHeight), -2.f, 2.f},
			{"wave_width", "Largeur", NkModParamType::Float, offsetof(NkMeshModifier, waveWidth), 0.01f, 10.f},
			{"wave_phase", "Phase", NkModParamType::Float, offsetof(NkMeshModifier, wavePhase), -100.f, 100.f},
			{"wave_axis", "Axe", NkModParamType::Int, offsetof(NkMeshModifier, waveAxis), 0.f, 2.f},
		};
		static const NkModParam kParamsAutoSmooth[] = {
			{"auto_smooth_angle", "Angle", NkModParamType::Float, offsetof(NkMeshModifier, autoSmoothAngle), 0.f,
			 180.f},
		};

		const char *NkModifierTypeName(NkModifierType t) {
			switch (t) {
				case NkModifierType::Mirror: return "Miroir";
				case NkModifierType::Array: return "Tableau";
				case NkModifierType::Subsurf: return "Subdivision de surface";
				case NkModifierType::Solidify: return "Solidifier";
				case NkModifierType::Triangulate: return "Trianguler";
				case NkModifierType::Weld: return "Souder";
				case NkModifierType::Bevel: return "Chanfrein";
				case NkModifierType::Screw: return "Vis (revolution)";
				case NkModifierType::EdgeSplit: return "Separer les aretes";
				case NkModifierType::Decimate: return "Simplifier";
				case NkModifierType::Build: return "Construction";
				case NkModifierType::Mask: return "Masque";
				case NkModifierType::Cast: return "Projeter";
				case NkModifierType::SimpleDeform: return "Deformation simple";
				case NkModifierType::Smooth: return "Lisser";
				case NkModifierType::Wave: return "Onde";
				case NkModifierType::SmoothByAngle: return "Ombrage par angle";
			}
			return "?";
		}

		const NkModParam *NkModifierParams(NkModifierType t, uint32 &count) {
			switch (t) {
				case NkModifierType::Mirror:
					count = (uint32)(sizeof(kParamsMirror) / sizeof(kParamsMirror[0]));
					return kParamsMirror;
				case NkModifierType::Array:
					count = (uint32)(sizeof(kParamsArray) / sizeof(kParamsArray[0]));
					return kParamsArray;
				case NkModifierType::Subsurf:
					count = (uint32)(sizeof(kParamsSubsurf) / sizeof(kParamsSubsurf[0]));
					return kParamsSubsurf;
#define NK_MOD_TABLE(T, A)                                                                                             \
	case NkModifierType::T: count = (uint32)(sizeof(A) / sizeof(A[0])); return A
				NK_MOD_TABLE(Solidify, kParamsSolidify);
				NK_MOD_TABLE(Triangulate, kParamsTriangulate);
				NK_MOD_TABLE(Weld, kParamsWeld);
				NK_MOD_TABLE(Bevel, kParamsBevel);
				NK_MOD_TABLE(Screw, kParamsScrew);
				NK_MOD_TABLE(EdgeSplit, kParamsEdgeSplit);
				NK_MOD_TABLE(Decimate, kParamsDecimate);
				NK_MOD_TABLE(Build, kParamsBuild);
				NK_MOD_TABLE(Mask, kParamsMask);
				NK_MOD_TABLE(Cast, kParamsCast);
				NK_MOD_TABLE(SimpleDeform, kParamsSimpleDeform);
				NK_MOD_TABLE(Smooth, kParamsSmooth);
				NK_MOD_TABLE(Wave, kParamsWave);
				NK_MOD_TABLE(SmoothByAngle, kParamsAutoSmooth);
#undef NK_MOD_TABLE
			}
			count = 0;
			return nullptr;
		}

		uint32 NkMeshModifier::ParamCount() const {
			uint32 n = 0;
			NkModifierParams(type, n);
			return n;
		}

		const NkModParam *NkMeshModifier::ParamAt(uint32 i) const {
			uint32 n = 0;
			const NkModParam *p = NkModifierParams(type, n);
			return (p && i < n) ? &p[i] : nullptr;
		}

		static bool NkEmStrEq(const char *a, const char *b) {
			if (!a || !b)
				return false;
			while (*a && *b) {
				if (*a != *b)
					return false;
				++a;
				++b;
			}
			return *a == *b;
		}

		const NkModParam *NkMeshModifier::FindParam(const char *name) const {
			uint32 n = 0;
			const NkModParam *p = NkModifierParams(type, n);
			for (uint32 i = 0; i < n; ++i)
				if (NkEmStrEq(p[i].name, name))
					return &p[i];
			return nullptr;
		}

		bool NkMeshModifier::GetParam(const char *name, float32 &out) const {
			const NkModParam *p = FindParam(name);
			if (!p || p->type == NkModParamType::Vec3)
				return false;
			const uint8 *base = (const uint8 *)this + p->offset;
			switch (p->type) {
				case NkModParamType::Bool: out = (*(const bool *)base) ? 1.f : 0.f; return true;
				case NkModParamType::Int: out = (float32)(*(const int32 *)base); return true;
				default: out = *(const float32 *)base; return true;
			}
		}

		bool NkMeshModifier::SetParam(const char *name, float32 v) {
			const NkModParam *p = FindParam(name);
			if (!p || p->type == NkModParamType::Vec3)
				return false;
			// Ecretage sur les bornes PUBLIEES : une courbe d'animation depasse
			// facilement (interpolation, rebond), et un arrayCount negatif ou un niveau
			// de subdivision a 30 ne sont pas des reglages, ce sont des plantages.
			if (p->maxV > p->minV) {
				if (v < p->minV)
					v = p->minV;
				if (v > p->maxV)
					v = p->maxV;
			}
			uint8 *base = (uint8 *)this + p->offset;
			switch (p->type) {
				case NkModParamType::Bool: *(bool *)base = (v >= 0.5f); return true;
				// Arrondi au plus proche et non troncature : une courbe qui passe par
				// 2,999 vise 3, pas 2.
				case NkModParamType::Int: *(int32 *)base = (int32)(v < 0.f ? v - 0.5f : v + 0.5f); return true;
				default: *(float32 *)base = v; return true;
			}
		}

		bool NkMeshModifier::GetParamVec3(const char *name, NkVec3f &out) const {
			const NkModParam *p = FindParam(name);
			if (!p || p->type != NkModParamType::Vec3)
				return false;
			out = *(const NkVec3f *)((const uint8 *)this + p->offset);
			return true;
		}

		bool NkMeshModifier::SetParamVec3(const char *name, const NkVec3f &v) {
			const NkModParam *p = FindParam(name);
			if (!p || p->type != NkModParamType::Vec3)
				return false;
			*(NkVec3f *)((uint8 *)this + p->offset) = v;
			return true;
		}

		// ── GESTION DE LA PILE ──────────────────────────────────────────────────
		bool NkModifierStack::Remove(uint32 index) {
			if (index >= (uint32)modifiers.Size())
				return false;
			for (uint32 i = index; i + 1 < (uint32)modifiers.Size(); ++i)
				modifiers[i] = modifiers[i + 1];
			modifiers.Resize((uint32)modifiers.Size() - 1);
			return true;
		}

		bool NkModifierStack::MoveUp(uint32 index) {
			if (index == 0 || index >= (uint32)modifiers.Size())
				return false;
			const NkMeshModifier t = modifiers[index - 1];
			modifiers[index - 1] = modifiers[index];
			modifiers[index] = t;
			return true;
		}

		bool NkModifierStack::MoveDown(uint32 index) {
			if (index + 1 >= (uint32)modifiers.Size())
				return false;
			return MoveUp(index + 1);
		}

		bool NkModifierStack::SetEnabled(uint32 index, bool on) {
			if (index >= (uint32)modifiers.Size())
				return false;
			modifiers[index].enabled = on;
			return true;
		}

		bool NkModifierStack::Duplicate(uint32 index) {
			if (index >= (uint32)modifiers.Size())
				return false;
			NkMeshModifier c = modifiers[index];
			c.id = mNextId++; // identifiant NEUF : animable independamment de l'original
			modifiers.PushBack(c);
			// Inseree JUSTE APRES l'original, comme Blender : la pile est un ordre
			// d'evaluation, une copie ajoutee a la fin n'aurait pas le meme effet.
			for (uint32 i = (uint32)modifiers.Size() - 1; i > index + 1; --i) {
				const NkMeshModifier t = modifiers[i - 1];
				modifiers[i - 1] = modifiers[i];
				modifiers[i] = t;
			}
			return true;
		}

		int32 NkModifierStack::IndexOfId(uint32 id) const {
			for (uint32 i = 0; i < (uint32)modifiers.Size(); ++i)
				if (modifiers[i].id == id)
					return (int32)i;
			return -1;
		}

		NkMeshModifier *NkModifierStack::FindById(uint32 id) {
			const int32 i = IndexOfId(id);
			return (i >= 0) ? &modifiers[(uint32)i] : nullptr;
		}

		const NkMeshModifier *NkModifierStack::FindById(uint32 id) const {
			const int32 i = IndexOfId(id);
			return (i >= 0) ? &modifiers[(uint32)i] : nullptr;
		}

		bool NkModifierStack::ApplyToBase(uint32 index, NkEditMesh &base, bool *outWarnNotFirst) {
			if (index >= (uint32)modifiers.Size())
				return false;
			if (outWarnNotFirst)
				*outWarnNotFirst = (index != 0);
			// On applique meme s'il est desactive ? NON : un modificateur eteint ne
			// participe pas au resultat affiche, le cuire produirait une geometrie que
			// l'utilisateur n'a jamais vue. On le retire simplement, comme Blender.
			if (modifiers[index].enabled)
				modifiers[index].Apply(base);
			return Remove(index);
		}

		void NkModifierStack::Evaluate(const NkEditMesh &base, NkEditMesh &out) const {
			out = base;
			for (uint32 i = 0; i < (uint32)modifiers.Size(); ++i)
				if (modifiers[i].enabled)
					modifiers[i].Apply(out);
		}

		bool NkMeshEditRecorder::Deserialize(const uint8 *data, uint32 size) {
			mCommands.Clear();
			EmR r(data, size);
			if (r.U32() != NK_EMREC_MAGIC)
				return false;
			const uint32 ver = r.U32(); // 1 = sans loopcut.cuts, 2 = avec
			const uint32 count = r.U32();
			for (uint32 i = 0; i < count && r.ok; ++i) {
				NkMeshEditCommand c;
				c.op = (NkMeshEditOp)r.U8();
				const uint32 sc = r.U32();
				for (uint32 k = 0; k < sc && r.ok; ++k)
					c.selection.PushBack(r.U32());
				c.extrude.individual = (r.U8() != 0);
				c.extrude.offset = r.F32();
				c.merge.mode = r.I32();
				c.subdiv.cuts = r.I32();
				{
					float32 x = r.F32(), y = r.F32(), z = r.F32();
					c.planePoint = {x, y, z};
				}
				{
					float32 x = r.F32(), y = r.F32(), z = r.F32();
					c.planeNormal = {x, y, z};
				}
				for (int32 col = 0; col < 4; ++col)
					for (int32 row = 0; row < 4; ++row)
						c.bisectXform[col][row] = r.F32();
				const uint32 mc = r.U32();
				for (uint32 k = 0; k < mc && r.ok; ++k) {
					float32 x = r.F32(), y = r.F32(), z = r.F32();
					NkVec3f d = {x, y, z};
					c.moveDeltas.PushBack(d);
				}
				if (ver >= 2u)
					c.loopcut.cuts = r.I32();
				if (ver >= 3u) {
					c.bevel.offset = r.F32();
					c.bevel.segments = r.I32();
					c.bevel.vertexOnly = (r.U8() != 0);
				}
				if (ver >= 4u) {
					c.inset.thickness = r.F32();
					c.inset.depth = r.F32();
					c.inset.individual = (r.U8() != 0);
				}
				if (ver >= 5u)
					c.esplit.gap = r.F32();
				if (ver >= 6u) {
					float32 cx = r.F32(), cy = r.F32(), cz = r.F32();
					c.spin.center = {cx, cy, cz};
					float32 axx = r.F32(), axy = r.F32(), axz = r.F32();
					c.spin.axis = {axx, axy, axz};
					c.spin.angle = r.F32();
					c.spin.steps = r.I32();
					c.spin.duplicate = (r.U8() != 0);
					for (int32 col = 0; col < 4; ++col)
						for (int32 row = 0; row < 4; ++row)
							c.spinXform[col][row] = r.F32();
				}
				if (ver >= 7u)
					c.dissolve.mode = r.I32();
				if (ver >= 8u) {
					float32 sx = r.F32(), sy = r.F32(), sz = r.F32();
					c.tosphere.center = {sx, sy, sz};
					c.tosphere.factor = r.F32();
					c.tosphere.individual = (r.U8() != 0);
					c.shrinkfatten.offset = r.F32();
				}
				if (ver >= 9u)
					c.loopcut.slide = r.F32();
				if (r.ok)
					mCommands.PushBack(c);
			}
			return r.ok;
		}

	} // namespace renderer
} // namespace nkentseu
