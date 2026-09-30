// -----------------------------------------------------------------------------
// FICHIER: Kernel\Runtime\NKRenderer\src\NKRenderer\Mesh\NkCreaturePeau.cpp
// DESCRIPTION: Implementation de la peau G1 (voir NkCreaturePeau.h).
// AUTEUR: TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026-09-29
// VERSION: 0.1.0
// -----------------------------------------------------------------------------

// ============================================================
// INCLUDES
// ============================================================

// Header correspondant (TOUJOURS EN PREMIER)
#include "NKRenderer/Mesh/NkCreaturePeau.h"

// Standard library
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

// ============================================================
// ANONYMOUS NAMESPACE (helpers internes)
// ============================================================

namespace nkentseu {
	namespace renderer {
		namespace {

			void Dire(char *out, uint32 cap, const char *fmt, ...) {
				if (!out || cap == 0u) {
					return;
				}
				va_list args;
				va_start(args, fmt);
				std::vsnprintf(out, cap, fmt, args);
				va_end(args);
			}

			bool Egal(const char *a, const char *b) {
				return a && b && std::strcmp(a, b) == 0;
			}

			NkVec3f V3(float32 x, float32 y, float32 z) {
				NkVec3f v;
				v.x = x;
				v.y = y;
				v.z = z;
				return v;
			}

			NkVec3f Plus(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.x + b.x, a.y + b.y, a.z + b.z);
			}

			NkVec3f Moins(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.x - b.x, a.y - b.y, a.z - b.z);
			}

			NkVec3f Fois(const NkVec3f &a, float32 k) {
				return V3(a.x * k, a.y * k, a.z * k);
			}

			float32 Scal(const NkVec3f &a, const NkVec3f &b) {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			}

			NkVec3f Vect(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
			}

			float32 Norme(const NkVec3f &a) {
				return std::sqrt(Scal(a, a));
			}

			NkVec3f Unitaire(const NkVec3f &a) {
				const float32 n = Norme(a);
				if (n < 1e-12f) {
					return V3(0.f, 0.f, 0.f);
				}
				return Fois(a, 1.f / n);
			}

			// Un anneau de la chaine : l'os qui le porte, son s, et sa nature.
			struct NkCreatureAnneau {
					uint32 os = 0u;
					float32 s = 0.f;
					/** Anneau du joint (au milieu de l'arc de conge) : l'os est le PARENT, s = 1. */
					bool estJoint = false;
					/** Boucle d'articulation : les deux bouts de l'arc et son milieu. */
					bool estBoucle = false;
					/** Anneau pose sur l'arc de conge du joint dont `arc` est l'os ENFANT (-1 : tube droit). */
					int32 arc = -1;
					/** Position le long de l'arc : 0 au debut (cote parent), 0,5 au joint, 1 a la fin. */
					float32 u = 0.f;
					uint32 premier = 0u;  // indice du sommet j = 0
			};

			// L'ARC DE CONGE d'un joint (04 §5.1, la reponse du tuyau cintre). La ligne
			// centrale quitte l'os parent a une distance t avant le joint J, tourne sur
			// un arc de cercle de rayon R tangent aux deux os, et rejoint l'enfant a t
			// apres J : t = R tan(theta/2). Les anneaux de l'arc sont PERPENDICULAIRES a
			// la ligne centrale -- le trapeze entre une boucle droite et l'anneau
			// bissecteur (rapport de cotes 4,05 sur la cheville de la patte de
			// validation) n'existe plus.
			struct NkCreatureConge {
					bool existe = false;
					/** Joint deja droit (theta ~ 0) : le troncon est un segment. */
					bool droit = true;
					uint32 p = 0u;
					uint32 c = 0u;
					float32 theta = 0.f;
					float32 R = 0.f;
					float32 t = 0.f;
					/** Le R minimal accepte si l'os est trop court pour 2 r_max (voir plus bas). */
					float32 tMin = 0.f;
					float32 sDebut = 1.f;  // sur le parent
					float32 sFin = 0.f;    // sur l'enfant
					uint32 nDemi = 1u;     // segments par demi-arc
					NkVec3f aP{};
					NkVec3f aC{};
					NkVec3f nP{};
					NkVec3f axe{};
					NkVec3f S{};
					NkVec3f E{};
					NkVec3f O{};
			};

			// Le pas le long d'un os : celui de la densite si elle est donnee, sinon le
			// pas AUTOUR de l'os au milieu (perimetre approche / M) -- des quads carres.
			float32 PasDe(const NkR32Document &doc, const NkR32Os &o) {
				if (doc.densite > 0.f) {
					return 1.f / doc.densite;
				}
				float32 a = 0.f;
				float32 b = 0.f;
				NkR32Section(o, 0.5f, a, b);
				const float32 perimetre = 3.14159265f * (a + b);
				return perimetre / (float32)doc.anneaux;
			}

			void PousserSommet(NkVector<NkVertex3D> &sommets, NkVector<NkR32AdresseSommet> &adresses,
							   const NkVec3f &p, const NkVec3f &n, const NkVec3f &t, float32 u, float32 v,
							   const NkR32AdresseSommet &adr) {
				NkVertex3D x{};
				x.pos = p;
				x.normal = n;
				x.tangent = t;
				x.uv = NkVec2f{u, v};
				x.uv2 = NkVec2f{u, v};
				x.color = 0xFFFFFFFFu;
				sommets.PushBack(x);
				adresses.PushBack(adr);
			}

			// Rotation de v autour de l'axe UNITAIRE `axe` (Rodrigues).
			NkVec3f Tourner(const NkVec3f &v, const NkVec3f &axe, float32 angle) {
				const float32 c = std::cos(angle);
				const float32 sn = std::sin(angle);
				return Plus(Plus(Fois(v, c), Fois(Vect(axe, v), sn)), Fois(axe, Scal(axe, v) * (1.f - c)));
			}

			// Centre, tangente et rotation de l'arc de conge au parametre u (0..1).
			NkVec3f CentreConge(const NkCreatureConge &cg, float32 u, NkVec3f &tangente) {
				if (cg.droit) {
					tangente = cg.aP;
					return Plus(cg.S, Fois(Moins(cg.E, cg.S), u));
				}
				const float32 phi = cg.theta * u;
				tangente = Plus(Fois(cg.aP, std::cos(phi)), Fois(cg.nP, std::sin(phi)));
				return Plus(cg.O, Fois(Moins(Fois(cg.aP, std::sin(phi)), Fois(cg.nP, std::cos(phi))), cg.R));
			}

		}  // namespace

		// ============================================================
		// IMPLEMENTATIONS
		// ============================================================

		bool NkCreatureConstruirePeau(const NkR32Document &doc, NkR32Peau &out, char *pourquoi, uint32 cap) {
			out.maillage.Clear();
			out.adresses.Clear();
			out.sommets.Clear();
			out.os = doc.os;
			const uint32 M = doc.anneaux;
			if (M < 4u || (M % 2u) != 0u) {
				Dire(pourquoi, cap, "anneaux = %u : il faut un nombre PAIR et au moins 4 (04 §5.4)", M);
				return false;
			}
			if (doc.boucles != 3u) {
				Dire(pourquoi, cap, "boucles_articulation = %u : G1 ne construit que 3 boucles par articulation",
					 doc.boucles);
				return false;
			}
			if (doc.os.Empty()) {
				Dire(pourquoi, cap, "aucun os : pas de peau");
				return false;
			}
			NkVector<NkR32Repere> reps;
			if (!NkR32CalculerReperes(doc, reps, pourquoi, cap)) {
				return false;
			}
			const uint32 nOs = (uint32)doc.os.Size();

			// ── 1. LES CHAINES : un os ne porte qu'un os (sinon : jonction, G2) ──
			NkVector<int32> enfant;
			NkVector<int32> parent;
			enfant.Resize(nOs);
			parent.Resize(nOs);
			for (uint32 i = 0u; i < nOs; ++i) {
				enfant[i] = -1;
				parent[i] = doc.Trouver(doc.os[i].parent);
			}
			for (uint32 i = 0u; i < nOs; ++i) {
				if (parent[i] < 0) {
					continue;
				}
				const uint32 p = (uint32)parent[i];
				if (enfant[p] >= 0) {
					Dire(pourquoi, cap,
						 "`%s` porte `%s` ET `%s` : un embranchement est une JONCTION (04 §5.2), construite en G2",
						 doc.os[p].nom, doc.os[(uint32)enfant[p]].nom, doc.os[i].nom);
					return false;
				}
				enfant[p] = (int32)i;
			}

			// ── 1b. LES ARCS DE CONGE, un par joint (indexe par l'os ENFANT) ──
			// t = max(bande de pose, arc) :
			//   - la bande de pose : largeurPli rayons moyens (1,1 par defaut), pas plus
			//     de 0,3 os -- le poids doit passer d'un os a l'autre sur une distance
			//     comparable a l'epaisseur du membre ;
			//   - l'arc : R >= 2 r_max, r_max = plus grand demi-axe des deux sections
			//     au joint. Sur l'arc, le cote interieur vaut (R - r) dphi et
			//     l'exterieur (R + r) dphi : R = 2 r borne leur rapport a 3, le seuil
			//     de regularite du §6. R = r ferait un point de rebroussement.
			// Un os trop court pour ses deux arcs les resserre jusqu'a R = 1,25 r_max,
			// puis la peau est REFUSEE (nommee) plutot que de se replier.
			NkVector<NkCreatureConge> conges;
			conges.Resize(nOs);
			for (uint32 c = 0u; c < nOs; ++c) {
				if (parent[c] < 0) {
					continue;
				}
				NkCreatureConge &cg = conges[c];
				cg.existe = true;
				cg.p = (uint32)parent[c];
				cg.c = c;
				cg.aP = reps[cg.p].axe;
				cg.aC = reps[c].axe;
				float32 cosT = Scal(cg.aP, cg.aC);
				cosT = cosT > 1.f ? 1.f : (cosT < -1.f ? -1.f : cosT);
				cg.theta = std::acos(cosT);
				if (cg.theta > 3.0f) {
					Dire(pourquoi, cap, "le joint `%s` -> `%s` est replie sur lui-meme (%.0f degres) : pas de tube possible",
						 doc.os[cg.p].nom, doc.os[c].nom, (double)(cg.theta * 180.f / 3.14159265f));
					return false;
				}
				float32 ap = 0.f;
				float32 bp = 0.f;
				float32 ac = 0.f;
				float32 bc = 0.f;
				NkR32Section(doc.os[cg.p], 1.f, ap, bp);
				NkR32Section(doc.os[c], 0.f, ac, bc);
				float32 rmax = ap;
				rmax = bp > rmax ? bp : rmax;
				rmax = ac > rmax ? ac : rmax;
				rmax = bc > rmax ? bc : rmax;
				const float32 lMin = doc.os[cg.p].longueur < doc.os[c].longueur ? doc.os[cg.p].longueur : doc.os[c].longueur;
				const float32 pasMoyen = 0.5f * (PasDe(doc, doc.os[cg.p]) + PasDe(doc, doc.os[c]));
				float32 tBande = 0.5f * pasMoyen;
				if (doc.largeurPli > 0.f) {
					tBande = doc.largeurPli * 0.25f * (ap + bp + ac + bc);
				}
				tBande = tBande < 0.3f * lMin ? tBande : 0.3f * lMin;
				const float32 tanDemi = std::tan(0.5f * cg.theta);
				const float32 tArc = 2.f * rmax * tanDemi;
				cg.tMin = 1.25f * rmax * tanDemi;
				cg.t = tBande > tArc ? tBande : tArc;
			}
			// Le budget de chaque os : ses deux arcs dans 90 % de sa longueur.
			for (uint32 i = 0u; i < nOs; ++i) {
				const float32 tD = conges[i].existe ? conges[i].t : 0.f;
				const float32 tF = (enfant[i] >= 0) ? conges[(uint32)enfant[i]].t : 0.f;
				const float32 budget = 0.9f * doc.os[i].longueur;
				if (tD + tF <= budget) {
					continue;
				}
				const float32 f = budget / (tD + tF);
				if (conges[i].existe) {
					const float32 t = conges[i].t * f;
					conges[i].t = t > conges[i].tMin ? t : conges[i].tMin;
				}
				if (enfant[i] >= 0) {
					NkCreatureConge &cg = conges[(uint32)enfant[i]];
					const float32 t = cg.t * f;
					cg.t = t > cg.tMin ? t : cg.tMin;
				}
			}
			for (uint32 i = 0u; i < nOs; ++i) {
				const float32 tD = conges[i].existe ? conges[i].t : 0.f;
				const float32 tF = (enfant[i] >= 0) ? conges[(uint32)enfant[i]].t : 0.f;
				if (tD + tF > 0.9f * doc.os[i].longueur + 1e-6f) {
					Dire(pourquoi, cap,
						 "l'os `%s` (longueur %.3f) est trop court pour les arcs de ses joints (%.3f + %.3f, au plus 90 %%)",
						 doc.os[i].nom, (double)doc.os[i].longueur, (double)tD, (double)tF);
					return false;
				}
			}
			for (uint32 c = 0u; c < nOs; ++c) {
				NkCreatureConge &cg = conges[c];
				if (!cg.existe) {
					continue;
				}
				const NkR32Repere &rp = reps[cg.p];
				const NkVec3f J = Plus(rp.racine, Fois(rp.axe, rp.longueur));
				cg.S = Moins(J, Fois(cg.aP, cg.t));
				cg.E = Plus(J, Fois(cg.aC, cg.t));
				cg.sDebut = 1.f - cg.t / doc.os[cg.p].longueur;
				cg.sFin = cg.t / doc.os[c].longueur;
				cg.droit = cg.theta < 1e-3f;
				float32 demi = cg.t;
				if (!cg.droit) {
					cg.R = cg.t / std::tan(0.5f * cg.theta);
					cg.nP = Unitaire(Moins(cg.aC, Fois(cg.aP, Scal(cg.aP, cg.aC))));
					cg.axe = Unitaire(Vect(cg.aP, cg.aC));
					cg.O = Plus(cg.S, Fois(cg.nP, cg.R));
					demi = 0.5f * cg.R * cg.theta;
				}
				const float32 pasMoyen = 0.5f * (PasDe(doc, doc.os[cg.p]) + PasDe(doc, doc.os[c]));
				uint32 nd = (uint32)(demi / pasMoyen + 0.5f);
				cg.nDemi = nd < 1u ? 1u : nd;
			}

			NkVector<NkVertex3D> sommets;
			NkVector<NkR32AdresseSommet> adrSommets;
			NkVector<uint32> debut;
			NkVector<uint32> boucles;
			debut.PushBack(0u);
			uint16 numChaine = 0u;

			for (uint32 racine = 0u; racine < nOs; ++racine) {
				if (parent[racine] >= 0) {
					continue;
				}
				// ── 2. LES ANNEAUX DE LA CHAINE, dans l'ordre ───────────────
				// Un os : [arc d'arrivee, cote enfant] tube droit [arc de depart, cote
				// parent, jusqu'au joint]. Les boucles d'articulation sont les deux bouts
				// de l'arc (s = 1 - t/L_parent, s = t/L_enfant) et son milieu (le joint).
				NkVector<NkCreatureAnneau> anneaux;
				for (int32 b = (int32)racine; b >= 0; b = enfant[(uint32)b]) {
					const NkR32Os &o = doc.os[(uint32)b];
					const float32 pas = PasDe(doc, o);
					const bool jointDebut = parent[(uint32)b] >= 0;
					const bool jointFin = enfant[(uint32)b] >= 0;
					const NkCreatureConge *cgDebut = jointDebut ? &conges[(uint32)b] : nullptr;
					const NkCreatureConge *cgFin = jointFin ? &conges[(uint32)enfant[(uint32)b]] : nullptr;
					const float32 a0 = cgDebut ? cgDebut->sFin : 0.f;
					const float32 b0 = cgFin ? cgFin->sDebut : 1.f;
					NkCreatureAnneau an;
					an.os = (uint32)b;
					if (cgDebut) {
						// Seconde moitie de l'arc du joint (parent -> b) : elle appartient a b.
						for (uint32 i = 1u; i <= cgDebut->nDemi; ++i) {
							const float32 f = (float32)i / (float32)cgDebut->nDemi;
							an.arc = b;
							an.u = 0.5f + 0.5f * f;
							an.s = cgDebut->sFin * f;
							an.estBoucle = (i == cgDebut->nDemi);
							anneaux.PushBack(an);
						}
					} else {
						an.arc = -1;
						an.s = 0.f;
						an.estBoucle = false;
						anneaux.PushBack(an);
					}
					uint32 n = (uint32)((b0 - a0) * o.longueur / pas + 0.5f);
					if (n < 1u) {
						n = 1u;
					}
					an.arc = -1;
					an.estBoucle = false;
					for (uint32 i = 1u; i < n; ++i) {
						an.s = a0 + (b0 - a0) * (float32)i / (float32)n;
						anneaux.PushBack(an);
					}
					if (cgFin) {
						// Premiere moitie de l'arc du joint (b -> enfant), jusqu'au joint.
						for (uint32 i = 0u; i <= cgFin->nDemi; ++i) {
							const float32 f = (float32)i / (float32)cgFin->nDemi;
							an.arc = enfant[(uint32)b];
							an.u = 0.5f * f;
							an.s = (i == cgFin->nDemi) ? 1.f : cgFin->sDebut + (1.f - cgFin->sDebut) * f;
							an.estBoucle = (i == 0u || i == cgFin->nDemi);
							an.estJoint = (i == cgFin->nDemi);
							anneaux.PushBack(an);
						}
						an.estJoint = false;
					} else {
						an.arc = -1;
						an.s = 1.f;
						an.estBoucle = false;
						anneaux.PushBack(an);
					}
				}

				// ── 3. LES SOMMETS DES ANNEAUX ───────────────────────────────
				const uint32 nA = (uint32)anneaux.Size();
				for (uint32 k = 0u; k < nA; ++k) {
					NkCreatureAnneau &an = anneaux[k];
					an.premier = (uint32)sommets.Size();
					const NkR32Os &o = doc.os[an.os];
					const NkR32Repere &r = reps[an.os];
					for (uint32 j = 0u; j < M; ++j) {
						const float32 a = (float32)j / (float32)M;
						NkR32AdresseSommet adr;
						adr.os = (uint16)an.os;
						adr.lieu = NkR32Lieu::Nk_R32Lieu_Tube;
						adr.s = an.s;
						adr.a = a;
						adr.anneau = (int32)k;
						adr.chaine = numChaine;
						adr.estBoucle = an.estBoucle ? 1u : 0u;
						if (an.arc < 0) {
							NkVec3f n;
							NkVec3f p = NkR32PointSection(r, o, an.s, a, &n);
							PousserSommet(sommets, adrSommets, p, n, r.axe, a, an.s, adr);
							continue;
						}
						// SUR L'ARC : la section du parent (a son bout d'arc) et celle de
						// l'enfant (a son bout d'arc, ramenee dans le plan du parent par la
						// rotation -theta) sont melangees en u, puis tournees de theta u.
						const NkCreatureConge &cg = conges[(uint32)an.arc];
						const NkR32Repere &rp = reps[cg.p];
						const NkR32Repere &rc = reps[cg.c];
						NkVec3f nP;
						NkVec3f nC;
						const NkVec3f cP = Plus(rp.racine, Fois(rp.axe, cg.sDebut * rp.longueur));
						const NkVec3f cC = Plus(rc.racine, Fois(rc.axe, cg.sFin * rc.longueur));
						NkVec3f vP = Moins(NkR32PointSection(rp, doc.os[cg.p], cg.sDebut, a, &nP), cP);
						NkVec3f vC = Moins(NkR32PointSection(rc, doc.os[cg.c], cg.sFin, a, &nC), cC);
						NkVec3f tangente;
						const NkVec3f centre = CentreConge(cg, an.u, tangente);
						NkVec3f v;
						NkVec3f n;
						if (cg.droit) {
							v = Plus(Fois(vP, 1.f - an.u), Fois(vC, an.u));
							n = Unitaire(Plus(Fois(nP, 1.f - an.u), Fois(nC, an.u)));
						} else {
							vC = Tourner(vC, cg.axe, -cg.theta);
							nC = Tourner(nC, cg.axe, -cg.theta);
							v = Tourner(Plus(Fois(vP, 1.f - an.u), Fois(vC, an.u)), cg.axe, cg.theta * an.u);
							n = Unitaire(Tourner(Plus(Fois(nP, 1.f - an.u), Fois(nC, an.u)), cg.axe, cg.theta * an.u));
						}
						PousserSommet(sommets, adrSommets, Plus(centre, v), n, tangente, a, an.s, adr);
					}
				}

				// ── 4. LES QUADS DU TUBE ─────────────────────────────────────
				// Ordre (k,j) (k+1,j) (k+1,j+1) (k,j+1) : normale SORTANTE dans la
				// convention de NkEditMesh (mesure par le banc, comme en G0).
				for (uint32 k = 0u; k + 1u < nA; ++k) {
					const NkCreatureAnneau &a0 = anneaux[k];
					const NkCreatureAnneau &a1 = anneaux[k + 1u];
					// Le segment appartient a l'os de l'anneau d'ARRIVEE ; s'il part du
					// joint de son parent, il commence a s = 0.
					const uint32 os = a1.os;
					const float32 s0 = (a0.os == os) ? a0.s : 0.f;
					const float32 s1 = a1.s;
					for (uint32 j = 0u; j < M; ++j) {
						const uint32 j1 = (j + 1u) % M;
						boucles.PushBack(a0.premier + j);
						boucles.PushBack(a1.premier + j);
						boucles.PushBack(a1.premier + j1);
						boucles.PushBack(a0.premier + j1);
						debut.PushBack((uint32)boucles.Size());
						NkR32AdresseFace adr;
						adr.os = (uint16)os;
						adr.lieu = NkR32Lieu::Nk_R32Lieu_Tube;
						adr.s0 = s0;
						adr.s1 = s1;
						adr.a0 = (float32)j / (float32)M;
						adr.a1 = (float32)(j + 1u) / (float32)M;
						out.adresses.PushBack(adr);
					}
				}

				// ── 5. LES CAPUCHONS : une grille n1 x n2, 2(n1 + n2) = M ────
				const uint32 n1 = M / 4u;
				const uint32 n2 = M / 2u - n1;
				for (uint32 bout = 0u; bout < 2u; ++bout) {
					const NkCreatureAnneau &an = (bout == 0u) ? anneaux[0] : anneaux[nA - 1u];
					const NkR32Repere &r = reps[an.os];
					const NkVec3f dehors = (bout == 0u) ? Fois(r.axe, -1.f) : r.axe;
					float32 demiA = 0.f;
					float32 demiB = 0.f;
					NkR32Section(doc.os[an.os], an.s, demiA, demiB);
					const float32 hauteur = 0.6f * 0.5f * (demiA + demiB);
					// Indice de sommet de chaque point (x, y) de la grille : le bord EST
					// l'anneau, parcouru dans l'ordre de ses j ; l'interieur est neuf.
					NkVector<uint32> grille;
					grille.Resize((n1 + 1u) * (n2 + 1u));
					auto idx = [&](uint32 x, uint32 y) -> uint32 & {
						return grille[y * (n1 + 1u) + x];
					};
					// LE DEPART DU PARCOURS, choisi pour que le MIROIR soit une symetrie
					// de la grille. Le miroir envoie le sommet j de l'anneau `_g` sur le
					// sommet (M - j) mod M de l'anneau `_d` (e2 = axe x e1 change de
					// signe). En partant de j = o, le parcours k devient (-2o - k) : une
					// symetrie de la grille n1 x n2 exige -2o = n1 (retournement x) ou
					// -2o = 2 n1 + n2 (retournement y), modulo M ; la grille carree a en
					// plus sa diagonale (o = 0). Mesure : avec M = 14 (grille 3 x 4) et
					// o = 0, 6 sommets du capuchon de `cuisse_g` n'avaient pas de miroir.
					uint32 depart = 0u;
					if (n1 != n2) {
						depart = (n1 % 2u == 0u) ? (M - n1) / 2u : n2 / 2u;
					}
					for (uint32 k = 0u; k < M; ++k) {
						uint32 x = 0u;
						uint32 y = 0u;
						if (k < n1) {
							x = k;
							y = 0u;
						} else if (k < n1 + n2) {
							x = n1;
							y = k - n1;
						} else if (k < 2u * n1 + n2) {
							x = n1 - (k - n1 - n2);
							y = n2;
						} else {
							x = 0u;
							y = n2 - (k - 2u * n1 - n2);
						}
						idx(x, y) = an.premier + (k + depart) % M;
					}
					// L'interieur : patch de Coons tendu sur le bord, puis bombe vers dehors.
					for (uint32 y = 1u; y < n2; ++y) {
						for (uint32 x = 1u; x < n1; ++x) {
							const float32 u = (float32)x / (float32)n1;
							const float32 v = (float32)y / (float32)n2;
							const NkVec3f bas = sommets[idx(x, 0u)].pos;
							const NkVec3f haut = sommets[idx(x, n2)].pos;
							const NkVec3f gauche = sommets[idx(0u, y)].pos;
							const NkVec3f droite = sommets[idx(n1, y)].pos;
							const NkVec3f p00 = sommets[idx(0u, 0u)].pos;
							const NkVec3f p10 = sommets[idx(n1, 0u)].pos;
							const NkVec3f p01 = sommets[idx(0u, n2)].pos;
							const NkVec3f p11 = sommets[idx(n1, n2)].pos;
							NkVec3f p = Plus(Plus(Fois(bas, 1.f - v), Fois(haut, v)),
											 Plus(Fois(gauche, 1.f - u), Fois(droite, u)));
							p = Moins(p, Plus(Plus(Fois(p00, (1.f - u) * (1.f - v)), Fois(p10, u * (1.f - v))),
											  Plus(Fois(p01, (1.f - u) * v), Fois(p11, u * v))));
							const float32 du = std::fabs(2.f * u - 1.f);
							const float32 dv = std::fabs(2.f * v - 1.f);
							const float32 d = du > dv ? du : dv;
							p = Plus(p, Fois(dehors, hauteur * (1.f - d * d)));
							uint32 aDuBord = x;
							if (n1 - x < aDuBord) {
								aDuBord = n1 - x;
							}
							if (y < aDuBord) {
								aDuBord = y;
							}
							if (n2 - y < aDuBord) {
								aDuBord = n2 - y;
							}
							NkR32AdresseSommet adr;
							adr.os = (uint16)an.os;
							adr.lieu = (bout == 0u) ? NkR32Lieu::Nk_R32Lieu_BoutDebut : NkR32Lieu::Nk_R32Lieu_BoutFin;
							adr.s = an.s;
							adr.anneau = (bout == 0u) ? -(int32)aDuBord : (int32)(nA - 1u + aDuBord);
							adr.chaine = numChaine;
							idx(x, y) = (uint32)sommets.Size();
							PousserSommet(sommets, adrSommets, p, dehors, r.axe, 0.5f, an.s, adr);
						}
					}
					// LE SENS DES FACES EST MESURE, PAS DEDUIT (lecon de G0) : on regarde
					// la normale du quad central, et on retourne tout s'il rentre.
					const uint32 cx = n1 / 2u;
					const uint32 cy = n2 / 2u;
					const uint32 xc = (cx + 1u <= n1) ? cx : n1 - 1u;
					const uint32 yc = (cy + 1u <= n2) ? cy : n2 - 1u;
					const NkVec3f q0 = sommets[idx(xc, yc)].pos;
					const NkVec3f q1 = sommets[idx(xc + 1u, yc)].pos;
					const NkVec3f q2 = sommets[idx(xc + 1u, yc + 1u)].pos;
					const NkVec3f q3 = sommets[idx(xc, yc + 1u)].pos;
					const NkVec3f nq = Plus(Vect(Moins(q1, q0), Moins(q2, q0)), Vect(Moins(q2, q0), Moins(q3, q0)));
					const bool inverse = Scal(nq, dehors) < 0.f;
					for (uint32 y = 0u; y < n2; ++y) {
						for (uint32 x = 0u; x < n1; ++x) {
							const uint32 a = idx(x, y);
							const uint32 b = idx(x + 1u, y);
							const uint32 c = idx(x + 1u, y + 1u);
							const uint32 d = idx(x, y + 1u);
							if (!inverse) {
								boucles.PushBack(a);
								boucles.PushBack(b);
								boucles.PushBack(c);
								boucles.PushBack(d);
							} else {
								boucles.PushBack(a);
								boucles.PushBack(d);
								boucles.PushBack(c);
								boucles.PushBack(b);
							}
							debut.PushBack((uint32)boucles.Size());
							NkR32AdresseFace adr;
							adr.os = (uint16)an.os;
							adr.lieu = (bout == 0u) ? NkR32Lieu::Nk_R32Lieu_BoutDebut : NkR32Lieu::Nk_R32Lieu_BoutFin;
							adr.s0 = an.s;
							adr.s1 = an.s;
							adr.a0 = 0.f;
							adr.a1 = 1.f;
							out.adresses.PushBack(adr);
						}
					}
				}
				++numChaine;
			}

			// ── 6. LE MAILLAGE, et l'adresse de chaque face (origine = f + 1) ──
			const uint32 nFaces = (uint32)debut.Size() - 1u;
			NkVector<NkEditMesh::FaceAttrib> attribs;
			attribs.Resize(nFaces);
			for (uint32 f = 0u; f < nFaces; ++f) {
				attribs[f] = NkEditMesh::FaceAttrib{};
				attribs[f].origine = f + 1u;
			}
			out.maillage.BuildFromPolygons(sommets.Data(), (uint32)sommets.Size(), debut.Data(), nFaces, boucles.Data(),
										   attribs.Data());
			out.maillage.RecomputeNormals();
			out.sommets = adrSommets;
			if (out.maillage.FaceCount() != nFaces || out.maillage.VertCount() != (uint32)sommets.Size()) {
				Dire(pourquoi, cap, "BuildFromPolygons a rendu %u faces / %u sommets pour %u / %u",
					 out.maillage.FaceCount(), out.maillage.VertCount(), nFaces, (uint32)sommets.Size());
				return false;
			}
			for (uint32 f = 0u; f < nFaces; ++f) {
				if (out.maillage.faces[f].origine != f + 1u) {
					Dire(pourquoi, cap, "la face %u ne porte pas son origine : l'ordre des faces a change", f);
					return false;
				}
			}
			return true;
		}

	}  // namespace renderer
}  // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Creation Date: 2026-09-29
// ============================================================
