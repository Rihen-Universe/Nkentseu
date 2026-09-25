#include "pch.h"
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/MeshSculpt/NkMeshSculpt.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   Application d'un trait de brosse aux sommets d'un NkEditMesh.
// -----------------------------------------------------------------------------

#include "NKRenderer/Tools/MeshSculpt/NkMeshSculpt.h"

#include "NKContainers/Sequential/NkVector.h"

#include <cmath>
#include <cstdlib> // getenv : la mutation du masque vit dans le MEME binaire

namespace nkentseu {
	namespace renderer {

		namespace {
			inline float32 Dot3(const NkVec3f &a, const NkVec3f &b) noexcept {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			}
		} // namespace

		float32 NkSculptSignedVolume(const NkEditMesh &mesh) noexcept {
			// Somme des produits mixtes sur un eventail par face. Pour une surface
			// fermee, c'est SIX fois le volume signe ; on divise donc par 6. Pour
			// une surface ouverte la valeur n'est pas un volume, mais elle reste
			// une fonctionnelle CONTINUE et ORIENTEE des positions -- ce qui suffit
			// a dire si un trait a pousse la matiere dehors ou dedans.
			float64 acc = 0.0;
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < mesh.FaceCount(); ++f) {
				if (!mesh.faces[f].alive)
					continue;
				loop.Clear();
				mesh.GetFaceVerts((NkEmId)f, loop);
				if (loop.Size() < 3)
					continue;
				const NkVec3f &p0 = mesh.verts[loop[0]].pos;
				for (uint32 k = 1; k + 1 < (uint32)loop.Size(); ++k) {
					const NkVec3f &p1 = mesh.verts[loop[k]].pos;
					const NkVec3f &p2 = mesh.verts[loop[k + 1]].pos;
					acc += (float64)Dot3(p0, p1.Cross(p2));
				}
			}
			return (float32)(acc / 6.0);
		}

		NkSculptApply NkSculptApplyStroke(NkEditMesh &mesh, const NkBrushDesc &brush,
										  const NkSculptPoint *points, uint32 count) noexcept {
			NkSculptApply out;

			// ── LE ZERO : SORTIR AVANT D'ECRIRE QUOI QUE CE SOIT ────────────────
			// ⚠️ Y compris les normales. Un `RecomputeNormals()` de precaution
			//    suffirait a faire bouger des octets sur un trait qui ne deforme
			//    rien, et la comparaison au bit deviendrait impossible -- on aurait
			//    perdu le seul critere qu'un bug ne peut pas satisfaire par hasard.
			if (!brush.valid || !points || count == 0)
				return out;
			if (mesh.VertCount() == 0)
				return out;
			const float32 amp = brush.strength * brush.dir;
			if (amp == 0.f)
				return out;

			// ── SOUDURE LOGIQUE ─────────────────────────────────────────────────
			// Deux sommets EXACTEMENT au meme endroit sont un seul coin. Sans ce
			// regroupement, les 3 copies d'un coin de cube partent chacune le long
			// de SA normale et le cube se dechire. (Meme raison que dans la
			// signature du harnais : « une primitive dont les faces dupliquent
			// leurs sommets n'aurait aucune arete partagee ».)
			NkVector<uint32> canon;
			mesh.BuildVertexMerge(canon);
			const uint32 vc = mesh.VertCount();
			if (canon.Size() < vc)
				return out; // instrument incoherent : on refuse plutot que deviner

			// Position et normale MOYENNES par groupe.
			NkVector<NkVec3f> gPos, gNrm;
			NkVector<uint32> gCnt;
			gPos.Resize(vc);
			gNrm.Resize(vc);
			gCnt.Resize(vc);
			for (uint32 i = 0; i < vc; ++i) {
				gPos[i] = NkVec3f{0.f, 0.f, 0.f};
				gNrm[i] = NkVec3f{0.f, 0.f, 0.f};
				gCnt[i] = 0;
			}
			for (uint32 i = 0; i < vc; ++i) {
				const uint32 c = canon[i];
				if (c >= vc)
					continue;
				gPos[c] = gPos[c] + mesh.verts[i].pos;
				gNrm[c] = gNrm[c] + mesh.verts[i].normal;
				gCnt[c]++;
			}
			for (uint32 i = 0; i < vc; ++i)
				if (gCnt[i] > 1) {
					const float32 inv = 1.f / (float32)gCnt[i];
					gPos[i] = gPos[i] * inv;
					gNrm[i] = gNrm[i] * inv;
				}

			// -- LE VOISINAGE, ET POURQUOI IL EST CONSTRUIT ICI ET PAS PLUS HAUT --
			// Seul le lissage en a besoin. Le batir pour tout le monde ferait payer a
			// `dessiner` un parcours de toutes les faces a chaque tampon, pour rien.
			//
			// Il est bati sur l'IDENTITE SOUDEE (`canon`), comme le reste de cette
			// fonction : sur un cube, les 3 copies d'un coin sont UN sommet, et leurs
			// voisins doivent se rejoindre. Batir l'adjacence sur les indices bruts
			// donnerait a chaque copie 2 voisins au lieu de 3, et le lissage tirerait
			// chaque coin dans une direction differente -- le cube se dechirerait, la
			// meme faute que la soudure vient d'empecher dix lignes plus haut.
			//
			// Doublons acceptes : une arete partagee par deux faces inscrit deux fois
			// la meme paire. C'est SANS EFFET sur une moyenne ponderee uniformement --
			// chaque voisin compte autant de fois des deux cotes de la somme.
			NkVector<uint32> nbrStart, nbrList;
			const bool needNbr = (brush.op == NkSculptOp::NK_SCULPT_OP_SMOOTH);
			if (needNbr) {
				NkVector<uint32> deg;
				deg.Resize(vc);
				for (uint32 i = 0; i < vc; ++i)
					deg[i] = 0;
				NkVector<NkEmId> loop;
				for (uint32 f = 0; f < mesh.FaceCount(); ++f) {
					if (!mesh.faces[f].alive)
						continue;
					loop.Clear();
					mesh.GetFaceVerts((NkEmId)f, loop);
					const uint32 n = (uint32)loop.Size();
					if (n < 3)
						continue;
					for (uint32 k = 0; k < n; ++k) {
						const uint32 a = canon[(uint32)loop[k]];
						const uint32 b = canon[(uint32)loop[(k + 1u) % n]];
						if (a >= vc || b >= vc || a == b)
							continue;
						deg[a]++;
						deg[b]++;
					}
				}
			// Somme prefixe, puis remplissage : une liste contigue plutot qu'un
			// vecteur de vecteurs -- sur 72 000 sommets, la difference n'est pas
			// une elegance.
				nbrStart.Resize(vc + 1u);
				uint32 acc2 = 0;
				for (uint32 i = 0; i < vc; ++i) {
					nbrStart[i] = acc2;
					acc2 += deg[i];
				}
				nbrStart[vc] = acc2;
				nbrList.Resize(acc2 > 0 ? acc2 : 1u);
				NkVector<uint32> cur;
				cur.Resize(vc);
				for (uint32 i = 0; i < vc; ++i)
					cur[i] = nbrStart[i];
				for (uint32 f = 0; f < mesh.FaceCount(); ++f) {
					if (!mesh.faces[f].alive)
						continue;
					loop.Clear();
					mesh.GetFaceVerts((NkEmId)f, loop);
					const uint32 n = (uint32)loop.Size();
					if (n < 3)
						continue;
					for (uint32 k = 0; k < n; ++k) {
						const uint32 a = canon[(uint32)loop[k]];
						const uint32 b = canon[(uint32)loop[(k + 1u) % n]];
						if (a >= vc || b >= vc || a == b)
							continue;
						nbrList[cur[a]++] = b;
						nbrList[cur[b]++] = a;
					}
				}
			}
			
			// ── LA BROSSE MASQUE : ELLE N'ECRIT AUCUNE POSITION ────────────────
			// Troisieme primitive, et la seule qui ne deforme rien : elle pose un
			// POIDS par sommet. Elle sort donc AVANT tout le calcul de deplacement --
			// y compris le volume signe, qui n'aurait aucun sens ici (le maillage ne
			// bouge pas, la mesure vaudrait « pas de changement » sur un geste qui a
			// pourtant agi).
			//
			// ⚠️ LE MEME CHEMIN QUE LES AUTRES JUSQU'ICI : soudure en groupes, zone,
			//    attenuation, pression. Un masque calcule sur les indices BRUTS
			//    donnerait a un coin de cube trois poids differents selon la copie,
			//    et la protection serait partielle la ou l'utilisateur l'a voulue
			//    pleine. Le poids se calcule par GROUPE, puis se pose sur TOUTES les
			//    copies du groupe -- exactement comme le deplacement.
			//
			// ⚠️ `sens = -1` EFFACE : le poids se retranche au lieu de s'ajouter.
			//    `amp` porte deja force x sens, donc il n'y a rien a decider ici.
			if (brush.op == NkSculptOp::NK_SCULPT_OP_MASK) {
				NkVector<float32> add;
				add.Resize(vc);
				for (uint32 i = 0; i < vc; ++i)
					add[i] = 0.f;
				uint32 groupesTouches = 0;
				for (uint32 g = 0; g < vc; ++g) {
					if (gCnt[g] == 0)
						continue;
					float32 acc = 0.f;
					bool touche = false;
					for (uint32 p = 0; p < count; ++p) {
						const NkSculptPoint &pt = points[p];
						const float32 r = (pt.radius > 0.f) ? pt.radius : brush.radius;
						if (r <= 0.f)
							continue;
						const NkVec3f d = gPos[g] - pt.pos;
						const float32 dist = sqrtf(Dot3(d, d));
						if (dist > r)
							continue;
						float32 pr = pt.pressure;
						if (pr < 0.f)
							pr = 0.f;
						if (pr > 1.f)
							pr = 1.f;
						const float32 w = NkBrushFalloff(dist / r, brush.falloff, brush.hardness) * amp * pr;
						if (w == 0.f)
							continue;
						acc += w;
						touche = true;
					}
					if (touche) {
						add[g] = acc;
						++groupesTouches;
					}
				}
				out.groupsInRadius = groupesTouches;
				if (groupesTouches == 0)
					return out; // aucun octet ecrit, masque compris
				mesh.MaskEnsure();
				uint32 poses = 0;
				float32 maxDelta = 0.f;
				for (uint32 i = 0; i < vc; ++i) {
					const uint32 c = canon[i];
					if (c >= vc || add[c] == 0.f)
						continue;
					const float32 avant = mesh.vertMask[i];
					float32 apres = avant + add[c];
					if (apres < 0.f)
						apres = 0.f;
					if (apres > 1.f)
						apres = 1.f;
					if (apres == avant)
						continue; // deja sature : ne pas compter un sommet qui n'a pas change
					mesh.vertMask[i] = apres;
					++poses;
					const float32 m = (apres > avant) ? (apres - avant) : (avant - apres);
					if (m > maxDelta)
						maxDelta = m;
				}
				if (poses == 0)
					return out;
				// `vertsMoved` compte ici les sommets dont le POIDS a change : le nom
				// dit « ce que le trait a touche », et c'est ce que l'appelant lit
				// pour savoir si le geste a agi. `maxDisplacement` porte le plus grand
				// ecart de poids -- sans unite de longueur, et c'est dit.
				out.vertsMoved = poses;
				out.maxDisplacement = maxDelta;
				out.applied = true;
				return out;
			}

			// -- DEPLACEMENT PAR GROUPE --
			NkVector<NkVec3f> disp;
			disp.Resize(vc);
			for (uint32 i = 0; i < vc; ++i)
				disp[i] = NkVec3f{0.f, 0.f, 0.f};

			uint32 groupsTouched = 0;
			for (uint32 g = 0; g < vc; ++g) {
				if (gCnt[g] == 0)
					continue; // pas un representant de groupe
				bool touched = false;
				NkVec3f acc{0.f, 0.f, 0.f};

				for (uint32 p = 0; p < count; ++p) {
					const NkSculptPoint &pt = points[p];
					const float32 r = (pt.radius > 0.f) ? pt.radius : brush.radius;
					if (r <= 0.f)
						continue;
					const NkVec3f d = gPos[g] - pt.pos;
					const float32 dist = sqrtf(Dot3(d, d));
					// ⚠️ LA ZONE : strictement au-dela du rayon, on ne touche pas.
					if (dist > r)
						continue;
					float32 pr = pt.pressure;
					if (pr < 0.f)
						pr = 0.f;
					if (pr > 1.f)
						pr = 1.f;
					const float32 w = NkBrushFalloff(dist / r, brush.falloff, brush.hardness) * amp * pr;
					if (w == 0.f)
						continue;
					// -- LES DEUX PRIMITIVES SE SEPARENT ICI, ET SEULEMENT ICI --
					// Tout ce qui precede -- soudure, groupes, zone, attenuation, pression --
					// leur est commun. Ce qui les distingue tient en une direction : l une
					// IMPOSE la sienne (la normale), l autre la DEDUIT de l entourage.
					if (brush.op == NkSculptOp::NK_SCULPT_OP_SMOOTH) {
					//
					//   LISSER : on vise la MOYENNE DES VOISINS (Laplacien uniforme).
					//   Le deplacement est une FRACTION du chemin vers cette moyenne, et
					//   non une longueur : c est ce qui rend l operation convergente et
					//   bornee. Pousser d une distance fixe vers la moyenne ferait
					//   osciller un sommet autour d elle au lieu de s y poser.
					//
					//   [!] PAS DE MISE A L ECHELLE PAR LE RAYON ICI, contrairement a
					//       l autre branche. `w * r` a un sens pour un DEPLACEMENT (une
					//       longueur) ; il n en a aucun pour une FRACTION deja sans unite,
					//       et multiplier par r ferait qu une grosse brosse lisserait plus
					//       fort au centre -- un effet que personne n a demande.
						const uint32 b0 = nbrStart[g];
						const uint32 b1 = nbrStart[g + 1u];
						if (b1 <= b0)
							continue; // sommet isole : rien a moyenner
						NkVec3f moy{0.f, 0.f, 0.f};
						for (uint32 k = b0; k < b1; ++k)
							moy = moy + gPos[nbrList[k]];
						const float32 invN = 1.f / (float32)(b1 - b0);
						moy = moy * invN;
					//   `w` porte deja force x sens x attenuation x pression. Un `sens`
					//   negatif ELOIGNE de la moyenne : le relief se durcit au lieu de
					//   s adoucir, et c est la meme formule prise a rebours.
						float32 f = w;
					//   Le pas est borne a 1 : au-dela, le sommet DEPASSE la moyenne et
					//   le lissage se met a osciller. La borne n est pas une precaution
					//   de confort, c est la condition de convergence du schema.
						if (f > 1.f)
							f = 1.f;
						if (f < -1.f)
							f = -1.f;
						acc = acc + (moy - gPos[g]) * f;
					} else {
						// Deplacement le long de la normale du GROUPE, mis a l'echelle
						// par le rayon : la force reste ainsi sans unite, et une meme
						// brosse se comporte pareil sur un objet de 1 cm et de 10 m.
						acc = acc + gNrm[g] * (w * r);
					}
					touched = true;
				}

				if (touched) {
					disp[g] = acc;
					++groupsTouched;
				}
			}
			out.groupsInRadius = groupsTouched;

			// ── RIEN A FAIRE ? ALORS RIEN N'EST ECRIT ───────────────────────────
			if (groupsTouched == 0)
				return out;

			out.volumeBefore = NkSculptSignedVolume(mesh);

			uint32 moved = 0;
			float32 maxDisp = 0.f;
			for (uint32 i = 0; i < vc; ++i) {
				const uint32 c = canon[i];
				if (c >= vc)
					continue;
				const NkVec3f &dBrut = disp[c];
				if (dBrut.x == 0.f && dBrut.y == 0.f && dBrut.z == 0.f)
					continue;
				// ── LE MASQUE PROTEGE, ET IL PROTEGE **TOUTES** LES BROSSES ────────
				// Ici, au SEUL point d'ecriture des positions : toute primitive --
				// celle d'aujourd'hui, celle de demain -- y passe. Une attenuation
				// posee dans chaque branche aurait couvert la premiere et oublie la
				// suivante ; c'est le defaut que le coordinateur annonce comme « le
				// point qui se rate ».
				// Poids 1 = intact, 0 = plein effet, entre les deux = lineaire. Il est
				// lu par SOMMET (et non par groupe) parce que le tableau est par
				// sommet ; les copies d'un coin portent le meme poids, la brosse
				// masque les ayant toutes ecrites ensemble.
				// MUTATION DANS LE MEME BINAIRE : NK_MASQUE_IGNORE=1 rend l'etat
				// d'AVANT ce lot -- les brosses ignorent le masque. Les criteres du
				// banc qui disent « le masque protege » DOIVENT alors rougir ; sans
				// ce negatif, ils pourraient etre verts pour une autre raison (un
				// trait qui rate le maillage rend aussi « rien n'a bouge »).
				static const bool sIgnoreMasque = []() {
					const char *v = std::getenv("NK_MASQUE_IGNORE");
					return v && v[0] && v[0] != '0';
				}();
				const float32 protege = sIgnoreMasque ? 0.f : mesh.MaskAt(i);
				const NkVec3f d = (protege > 0.f) ? dBrut * (1.f - protege) : dBrut;
				if (d.x == 0.f && d.y == 0.f && d.z == 0.f)
					continue; // entierement protege : ce sommet ne compte pas comme deplace
				mesh.verts[i].pos = mesh.verts[i].pos + d;
				++moved;
				const float32 m = sqrtf(Dot3(d, d));
				if (m > maxDisp)
					maxDisp = m;
			}

			if (moved == 0)
				return out; // volumeBefore lu, mais aucun octet ecrit : cohérent

			mesh.RecomputeNormals();
			out.vertsMoved = moved;
			out.maxDisplacement = maxDisp;
			out.volumeAfter = NkSculptSignedVolume(mesh);
			out.applied = true;
			return out;
		}

	} // namespace renderer
} // namespace nkentseu
