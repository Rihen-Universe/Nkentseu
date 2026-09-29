// -----------------------------------------------------------------------------
// FICHIER: Applications\NKR32Harness\src\main.cpp
// DESCRIPTION: Banc de R32 — adresses stables, pile C3 rejouee, les cinq cas
//              du §3.2bis, R32.7, et le taux de survie mesure sur trois chaines.
// AUTEUR: TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026-09-29
// VERSION: 0.1.0
// -----------------------------------------------------------------------------
//
// LE CRITERE DE G0 (04 §14) : une corne extrudee A LA MAIN survit a TROIS
// regenerations, et le taux de survie part d'un zero MESURE, pas suppose.
//
// ⚠️ LE JUGE N'EST PAS L'ACCUSE. Une retouche « survit » si la GEOMETRIE est a
//    sa place, mesuree par projection analytique sur l'os du document regenere
//    (NkR32Projeter / NkR32PointAnalytique) — jamais en demandant a R32 s'il
//    pense l'avoir appliquee. Un report faux et silencieux (le cas 4) doit
//    donc etre VU.
// ⚠️ LE JUGE EST LUI-MEME CONTROLE : sur le document d'origine, il doit dire
//    3/3 avec les retouches (temoin positif) et 0/3 sans (temoin negatif).
//
// LES TROIS CHAINES, SUR LES MEMES TROIS REGENERATIONS
//   ecraser  : regenerer = reconstruire depuis le document, sans pile. C'est le
//              comportement ecrit dans ETAT_REPRISE_GENIA3D.md:131-140.
//   indices  : rejouer tel quel le journal classique (NkMeshEditRecorder), dont
//              les cibles sont des NUMEROS de sommets et de faces.
//   R32      : la pile rejouee par designation.
//
// USAGE
//   NKR32Harness            -> imprime chaque critere ; code 1 si l'un rougit
//   NK_R32_MUTE=1..4        -> doit rendre 1 (voir NkMeshR32.h)
// -----------------------------------------------------------------------------

// ============================================================
// INCLUDES
// ============================================================

#include "NKRenderer/Mesh/NkMeshR32.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================
// USING DECLARATIONS
// ============================================================

using namespace nkentseu;
using namespace nkentseu::renderer;

// ============================================================
// ANONYMOUS NAMESPACE
// ============================================================

namespace {

	// ========================================
	// CONSTANTS
	// ========================================

	const uint32 NK_BANC_TEXTE_MAX = 8192u;
	const float32 NK_BANC_UN_SIXIEME = 1.f / 6.f;
	const float32 NK_BANC_UN_TIERS = 1.f / 3.f;
	const float32 NK_BANC_CORNE_LONGUEUR = 0.22f;
	const float32 NK_BANC_BOSSE = 0.04f;
	const float32 NK_BANC_CREUX = 0.04f;

	int32 nkEchecs = 0;
	int32 nkVerifs = 0;

	void Verifier(bool ok, const char *cas, const char *fmt, ...) {
		++nkVerifs;
		if (!ok) {
			++nkEchecs;
		}
		char detail[512];
		va_list args;
		va_start(args, fmt);
		std::vsnprintf(detail, sizeof(detail), fmt, args);
		va_end(args);
		std::printf("%-6s %-44s %s\n", ok ? "OK" : "ROUGE", cas, detail);
	}

	NkVec3f V3(float32 x, float32 y, float32 z) {
		NkVec3f v;
		v.x = x;
		v.y = y;
		v.z = z;
		return v;
	}

	float32 Dist(const NkVec3f &a, const NkVec3f &b) {
		const float32 dx = a.x - b.x;
		const float32 dy = a.y - b.y;
		const float32 dz = a.z - b.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	NkVec3f Unit(const NkVec3f &a) {
		const float32 n = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
		return V3(a.x / n, a.y / n, a.z / n);
	}

	// ========================================
	// LA CREATURE D'ESSAI
	// ========================================

	NkR32Os Os(const char *nom, const char *parent, NkVec3f racine, NkVec3f dir, float32 l, float32 r0, float32 r1,
			   NkVec3f ref) {
		NkR32Os o;
		std::snprintf(o.nom, sizeof(o.nom), "%s", nom);
		std::snprintf(o.parent, sizeof(o.parent), "%s", parent);
		o.racine = racine;
		o.direction = dir;
		o.longueur = l;
		o.rayon0 = r0;
		o.rayon1 = r1;
		o.reference = ref;
		o.versionReference = 1u;
		return o;
	}

	bool DocDepart(NkR32Document &d) {
		char pq[256];
		d = NkR32Document{};
		const NkVec3f haut = V3(0.f, 1.f, 0.f);
		const NkVec3f avant = V3(0.f, 0.f, 1.f);
		bool ok = d.AjouterOs(Os("torse", "", V3(0.f, 0.f, 0.f), haut, 1.0f, 0.30f, 0.28f, avant), pq, 256u);
		ok = ok && d.AjouterOs(Os("cou", "torse", V3(0.f, 0.f, 0.f), haut, 0.30f, 0.10f, 0.09f, avant), pq, 256u);
		ok = ok && d.AjouterOs(Os("tete", "cou", V3(0.f, 0.f, 0.f), haut, 0.50f, 0.20f, 0.18f, avant), pq, 256u);
		ok = ok && d.AjouterOs(Os("bras_g", "", V3(0.32f, 0.85f, 0.f), V3(1.f, 0.f, 0.f), 0.60f, 0.08f, 0.07f, haut),
							   pq, 256u);
		ok = ok && d.AjouterOs(Os("avant_bras_g", "bras_g", V3(0.f, 0.f, 0.f), V3(1.f, -0.2f, 0.f), 0.50f, 0.065f,
								  0.05f, haut),
							   pq, 256u);
		if (!ok) {
			std::printf("document de depart refuse : %s\n", pq);
		}
		return ok;
	}

	// Les trois regenerations, CUMULEES (chacune part de la precedente).
	void Regenerer(NkR32Document &d, int32 k) {
		if (d.Trouver("tete") < 0 || d.Trouver("bras_g") < 0 || d.Trouver("torse") < 0 || d.Trouver("cou") < 0) {
			std::printf("       regeneration impossible : un os du scenario manque\n");
			return;
		}
		if (k >= 1) {
			// R1 — proportions (C0/C1) : tete plus longue et plus epaisse, bras plus long, torse plus large.
			NkR32Os &t = d.os[(uint32)d.Trouver("tete")];
			t.longueur = 0.60f;
			t.rayon0 = 0.23f;
			t.rayon1 = 0.21f;
			d.os[(uint32)d.Trouver("bras_g")].longueur = 0.70f;
			d.os[(uint32)d.Trouver("torse")].rayon0 = 0.34f;
		}
		if (k >= 2) {
			// R2 — resolution : plus de sommets par anneau, plus d'anneaux.
			d.anneaux = 16u;
			d.densite = 11.f;
		}
		if (k >= 3) {
			// R3 — pose : le cou et la tete s'inclinent, le bras se leve.
			d.os[(uint32)d.Trouver("cou")].direction = Unit(V3(0.3f, 1.f, 0.f));
			d.os[(uint32)d.Trouver("tete")].direction = Unit(V3(0.25f, 1.f, 0.05f));
			d.os[(uint32)d.Trouver("bras_g")].direction = Unit(V3(1.f, 0.5f, 0.2f));
		}
	}

	// ========================================
	// LA MAIN : UNE SELECTION A LA SOURIS
	// ========================================

	// Emule des clics : choisit les faces de base de `os` dont le CENTRE tombe
	// dans la plage. Rend un tableau d'octets par face, comme l'editeur.
	void Cliquer(const NkR32Etat &e, const char *os, float32 s0, float32 s1, float32 a0, float32 a1,
				 NkVector<uint8> &sel) {
		sel.Resize(e.maillage.FaceCount());
		for (uint32 f = 0u; f < e.maillage.FaceCount(); ++f) {
			sel[f] = 0u;
			const uint32 id = e.maillage.faces[f].origine;
			if (id == 0u) {
				continue;
			}
			const NkR32Origine &o = e.origines[id - 1u];
			const NkR32AdresseFace &adr = e.peau.adresses[o.faceBase];
			if (o.role != NkR32Role::Nk_R32Role_Base || adr.lieu != NkR32Lieu::Nk_R32Lieu_Tube) {
				continue;
			}
			if (std::strcmp(e.peau.os[adr.os].nom, os) != 0) {
				continue;
			}
			const float32 sm = 0.5f * (adr.s0 + adr.s1);
			const float32 am = 0.5f * (adr.a0 + adr.a1);
			if (sm > s0 && sm < s1 && am > a0 && am < a1) {
				sel[f] = 1u;
			}
		}
	}

	// ========================================
	// LE JUGE (geometrie analytique, independante de R32)
	// ========================================

	bool DansArc(float32 a, float32 a0, float32 a1) {
		const float32 x = a - std::floor(a);
		const float32 lo = a0 - std::floor(a0);
		const float32 hi = a1 - std::floor(a1);
		if (a1 - a0 >= 1.f) {
			return true;
		}
		if (lo <= hi) {
			return x >= lo && x <= hi;
		}
		return x >= lo || x <= hi;
	}

	struct NkBancHauteur {
			float32 s = 0.f;
			float32 a = 0.f;
			float32 h = 0.f;  // distance radiale au-dessus (+) ou au-dessous (-) de la peau analytique
			bool estValide = false;
	};

	// ── LA HAUTEUR SE MESURE PAR RAPPORT A LA PEAU DE BASE, PAS AU TUBE ANALYTIQUE ──
	// Le juge lance un rayon depuis l'axe de l'os (en s borne a [0, 1]) vers le
	// sommet, et compare sa distance au PREMIER impact sur la peau de BASE du meme
	// document (sans retouche) : h = 0 pour tout sommet non retouche, quel que
	// soit le generateur.
	// ⚠️ LA PREMIERE VERSION COMPARAIT AU TUBE ANALYTIQUE (rayon0 -> rayon1) : juste
	//    pour G0, FAUX pour la peau G1. Mesure du 29/09 (NK_R32_GENERATEUR=2, R3) :
	//    l'anneau du joint torse -> cou, moyenne des deux sections, est 0,087 SOUS
	//    le tube analytique du torse ; ses 5 sommets comptaient comme le creux, et
	//    le juge rougissait (creux decale de 0,13) sans aucun defaut de R32.
	//    La peau de base est construite par le generateur, pas par R32 : le juge
	//    reste independant de ce qu'il juge.
	struct NkBancBase {
			bool active = false;
			NkVector<NkVec3f> triangles;  // 3 sommets par triangle
			NkVector<uint32> osTriangle;  // l'os de la face de base d'ou vient le triangle
			NkVector<NkR32Repere> reps;
	};
	NkBancBase gBase;

	void PoserBase(const NkR32Document &doc) {
		gBase.active = false;
		gBase.triangles.Clear();
		gBase.osTriangle.Clear();
		char pq[256];
		NkR32Peau peau;
		if (!NkR32ConstruirePeau(doc, peau, pq, 256u) || !NkR32CalculerReperes(doc, gBase.reps, pq, 256u)) {
			return;
		}
		const NkEditMesh &m = peau.maillage;
		NkVector<NkEmId> fv;
		for (uint32 f = 0u; f < m.FaceCount(); ++f) {
			if (!m.faces[f].alive) {
				continue;
			}
			m.GetFaceVerts((NkEmId)f, fv);
			for (uint32 k = 1u; k + 1u < (uint32)fv.Size(); ++k) {
				gBase.triangles.PushBack(m.verts[fv[0]].pos);
				gBase.triangles.PushBack(m.verts[fv[k]].pos);
				gBase.triangles.PushBack(m.verts[fv[k + 1u]].pos);
				gBase.osTriangle.PushBack((uint32)peau.adresses[f].os);
			}
		}
		gBase.active = true;
	}

	// Moller-Trumbore : la plus petite distance t > 0 le long de `d` (unitaire),
	// sur les seuls triangles de l'os `os`.
	// ⚠️ SUR TOUTE LA PEAU, le rayon touchait d'abord un AUTRE os la ou les tubes
	//    G0 s'interpenetrent (le bras leve entre dans le torse en R3) : la bosse
	//    ramassait 11 sommets au lieu de 9 (mesure du 29/09).
	bool Rayon(const NkVec3f &o, const NkVec3f &d, uint32 os, float32 &tMin) {
		tMin = 1e30f;
		for (uint32 i = 0u; i + 2u < (uint32)gBase.triangles.Size(); i += 3u) {
			if (gBase.osTriangle[i / 3u] != os) {
				continue;
			}
			const NkVec3f &a = gBase.triangles[i];
			const NkVec3f &b = gBase.triangles[i + 1u];
			const NkVec3f &c = gBase.triangles[i + 2u];
			const NkVec3f e1 = V3(b.x - a.x, b.y - a.y, b.z - a.z);
			const NkVec3f e2 = V3(c.x - a.x, c.y - a.y, c.z - a.z);
			const NkVec3f pv = V3(d.y * e2.z - d.z * e2.y, d.z * e2.x - d.x * e2.z, d.x * e2.y - d.y * e2.x);
			const float32 det = e1.x * pv.x + e1.y * pv.y + e1.z * pv.z;
			// Rayon PARALLELE au triangle (seuil RELATIF) : un rayon parti du centre
			// d'un capuchon plat G0 vers son bord glissait dans le plan du capuchon,
			// |det| ~ 1e-9 passait un seuil absolu, et t valait ~0 : 7 sommets du
			// bord du capuchon de la tete comptaient comme la corne (mesure, G0 R3).
			const float32 l1 = std::sqrt(e1.x * e1.x + e1.y * e1.y + e1.z * e1.z);
			const float32 l2 = std::sqrt(e2.x * e2.x + e2.y * e2.y + e2.z * e2.z);
			if (std::fabs(det) <= 1e-4f * l1 * l2) {
				continue;
			}
			const float32 inv = 1.f / det;
			const NkVec3f tv = V3(o.x - a.x, o.y - a.y, o.z - a.z);
			const float32 u = (tv.x * pv.x + tv.y * pv.y + tv.z * pv.z) * inv;
			if (u < -1e-6f || u > 1.f + 1e-6f) {
				continue;
			}
			const NkVec3f qv = V3(tv.y * e1.z - tv.z * e1.y, tv.z * e1.x - tv.x * e1.z, tv.x * e1.y - tv.y * e1.x);
			const float32 v = (d.x * qv.x + d.y * qv.y + d.z * qv.z) * inv;
			if (v < -1e-6f || u + v > 1.f + 1e-6f) {
				continue;
			}
			const float32 t = (e2.x * qv.x + e2.y * qv.y + e2.z * qv.z) * inv;
			if (t > 1e-6f && t < tMin) {
				tMin = t;
			}
		}
		return tMin < 1e29f;
	}

	NkBancHauteur Hauteur(const NkR32Document &doc, const char *os, const NkVec3f &p) {
		NkBancHauteur r;
		float32 rho = 0.f;
		if (!NkR32Projeter(doc, os, p, r.s, r.a, rho)) {
			return r;
		}
		const float32 sc = r.s < 0.f ? 0.f : (r.s > 1.f ? 1.f : r.s);
		const int32 io = doc.Trouver(os);
		if (gBase.active && io >= 0 && (uint32)io < (uint32)gBase.reps.Size()) {
			const NkR32Repere &rp = gBase.reps[(uint32)io];
			const NkVec3f o = V3(rp.racine.x + rp.axe.x * sc * rp.longueur, rp.racine.y + rp.axe.y * sc * rp.longueur,
								 rp.racine.z + rp.axe.z * sc * rp.longueur);
			const float32 dist = Dist(p, o);
			float32 t = 0.f;
			if (dist > 1e-9f && Rayon(o, Unit(V3(p.x - o.x, p.y - o.y, p.z - o.z)), (uint32)io, t)) {
				r.h = dist - t;
				r.estValide = true;
				return r;
			}
		}
		NkVec3f q;
		NkVec3f n;
		float32 rayon = 0.f;
		NkR32PointAnalytique(doc, os, sc, r.a, q, n, &rayon);
		r.h = rho - rayon;
		r.estValide = true;
		return r;
	}

	// L'os le plus proche d'un point : celui dont la peau de base est la plus
	// proche, en comptant le depassement hors de [0, 1] le long de l'os.
	// ⚠️ SANS CE FILTRE LE JUGE SE TROMPAIT DE CORPS : en R3 le bras leve passe
	//    pres de la tete, et ses 154 sommets, projetes sur l'axe de la tete,
	//    comptaient comme une corne -- meme sur la chaine « ecraser ».
	// ⚠️ ET LE FILTRE « s dans [0, 1] » NE SUFFIT PAS : le capuchon bombe de la
	//    peau G1 se projette a s < 0 sur son propre os, qui etait alors ecarte --
	//    la tete « gagnait » 9 sommets du bras leve (mesure du 29/09, G1, R3).
	bool EstLePlusProche(const NkR32Document &doc, const char *os, const NkEditMesh &m, uint32 v) {
		float32 meilleur = 1e30f;
		int32 lequel = -1;
		for (uint32 i = 0u; i < (uint32)doc.os.Size(); ++i) {
			const NkBancHauteur h = Hauteur(doc, doc.os[i].nom, m.verts[v].pos);
			if (!h.estValide) {
				continue;
			}
			const float32 depasse = (h.s < 0.f ? -h.s : (h.s > 1.f ? h.s - 1.f : 0.f)) * doc.os[i].longueur;
			const float32 d = (h.h < 0.f ? -h.h : h.h) + depasse;
			if (d < meilleur) {
				meilleur = d;
				lequel = (int32)i;
			}
		}
		return lequel >= 0 && std::strcmp(doc.os[(uint32)lequel].nom, os) == 0;
	}

	// ── L'ECART BORNE (04 §10.7) ────────────────────────────────────────────
	// Quand la resolution change, une retouche est REECHANTILLONNEE sur la
	// nouvelle grille : elle ne peut pas etre plus precise qu'une face. La
	// tolerance du juge ajoute donc une demi-face de la resolution COURANTE --
	// et elle reste plus petite qu'un placement faux, qui decale d'au moins une
	// face entiere.
	void TailleFace(const NkR32Document &doc, const char *os, float32 &ds, float32 &da, float32 &diag) {
		const int32 i = doc.Trouver(os);
		if (i < 0) {
			ds = 1.f;
			da = 1.f;
			diag = 0.f;
			return;
		}
		const NkR32Os &o = doc.os[(uint32)i];
		uint32 pas = (uint32)(o.longueur * doc.densite + 0.5f);
		if (pas < 1u) {
			pas = 1u;
		}
		ds = 1.f / (float32)pas;
		da = 1.f / (float32)doc.anneaux;
		const float32 rayon = o.rayon0 > o.rayon1 ? o.rayon0 : o.rayon1;
		const float32 ls = o.longueur * ds;
		const float32 la = 2.f * 3.14159265f * rayon * da;
		diag = std::sqrt(ls * ls + la * la);
	}

	// ── L'ECART BORNE, PAR DIRECTION (04 §10.7) ─────────────────────────────
	// Un relief reechantillonne ne peut pas etre plus precis qu'une demi-face
	// du maillage JUGE, dans chaque direction. L'erreur est donc decomposee :
	//   le long de l'os   <= 0,6 x cote de face en s    + 10 % du relief
	//   autour de l'os    <= 0,6 x cote de face en a    + 10 % du relief
	//   selon la normale  AFFICHE, pas juge : un placement faux est TANGENT a la
	//                     surface (les trois temoins rougissent tous par `a`), et
	//                     la hauteur est jugee a part (moyenne a 30 % pres). Le
	//                     fond d'un creux insere est une CORDE sous la peau
	//                     courbe : son centre y est plus bas sans aucun defaut
	//                     (mesure : 0,0164 sur le torse en R2).
	// Un placement faux d'UNE face depasse 0,6 face : il rougit (temoins
	// « juge-rougit-a-une-face » pour chaque retouche).
	// ⚠️ LA VERSION PRECEDENTE plafonnait une erreur LE LONG de l'os par le cote
	//    de face AUTOUR de l'os, et prenait la resolution du document d'ORIGINE
	//    au lieu de celle du maillage juge : elle rougissait sur une coupe a
	//    t = 0,3, ou la bosse reechantillonnee est a un tiers de face.
	struct NkBancResolution {
			const NkR32Document *doc = nullptr;
			const char *os = nullptr;
	};

	bool JugerPosition(const NkR32Document &doc, const char *os, float32 sc, float32 ac, float32 hauteur,
					   const NkVec3f &trouve, const NkBancResolution &res, char *detail, uint32 cap) {
		NkVec3f p;
		NkVec3f n;
		NkVec3f p0;
		NkVec3f p1;
		NkVec3f n0;
		NkR32PointAnalytique(doc, os, sc, ac, p, n);
		NkR32PointAnalytique(doc, os, sc - 0.01f, ac, p0, n0);
		NkR32PointAnalytique(doc, os, sc + 0.01f, ac, p1, n0);
		const NkVec3f t = Unit(V3(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z));
		const NkVec3f u = Unit(V3(n.y * t.z - n.z * t.y, n.z * t.x - n.x * t.z, n.x * t.y - n.y * t.x));
		const NkVec3f e = V3(trouve.x - p.x - n.x * hauteur, trouve.y - p.y - n.y * hauteur,
							 trouve.z - p.z - n.z * hauteur);
		const float32 es = std::fabs(e.x * t.x + e.y * t.y + e.z * t.z);
		const float32 ea = std::fabs(e.x * u.x + e.y * u.y + e.z * u.z);
		const float32 en = std::fabs(e.x * n.x + e.y * n.y + e.z * n.z);
		float32 ds = 0.f;
		float32 da = 0.f;
		float32 diag = 0.f;
		const NkR32Document &rd = res.doc ? *res.doc : doc;
		const char *ro = res.os ? res.os : os;
		TailleFace(rd, ro, ds, da, diag);
		const int32 io = rd.Trouver(ro);
		const NkR32Os &o = rd.os[(uint32)(io < 0 ? 0 : io)];
		const float32 rayon = o.rayon0 > o.rayon1 ? o.rayon0 : o.rayon1;
		const float32 ls = o.longueur * ds;
		const float32 la = 2.f * 3.14159265f * rayon * da;
		const float32 absH = hauteur < 0.f ? -hauteur : hauteur;
		const float32 ts = 0.6f * ls + 0.1f * absH;
		const float32 ta = 0.6f * la + 0.1f * absH;
		std::snprintf(detail, cap, "ecart s %.4f/%.4f a %.4f/%.4f (n %.4f)", (double)es, (double)ts, (double)ea,
					  (double)ta, (double)en);
		return es <= ts && ea <= ta;
	}

	// La corne : son BOUT doit etre a P(sc, ac) + n * longueur.
	bool JugerCorne(const NkR32Document &doc, const char *os, const NkEditMesh &m, char *detail, uint32 cap,
					const NkBancResolution &res = NkBancResolution{}) {
		const float32 sc = 0.625f;
		const float32 ac = 0.25f;
		PoserBase(doc);
		if (doc.Trouver(os) < 0) {
			std::snprintf(detail, cap, "os `%s` absent du document", os);
			return false;
		}
		NkVec3f c = V3(0.f, 0.f, 0.f);
		uint32 nb = 0u;
		for (uint32 v = 0u; v < m.VertCount(); ++v) {
			const NkBancHauteur h = Hauteur(doc, os, m.verts[v].pos);
			if (!h.estValide || h.s < -0.05f || h.s > 1.05f || h.h <= 0.75f * NK_BANC_CORNE_LONGUEUR) {
				continue;
			}
			if (!EstLePlusProche(doc, os, m, v)) {
				continue;
			}
			c = V3(c.x + m.verts[v].pos.x, c.y + m.verts[v].pos.y, c.z + m.verts[v].pos.z);
			++nb;
		}
		if (nb < 3u) {
			std::snprintf(detail, cap, "corne ABSENTE (%u sommets au-dessus de 75 %% de sa longueur)", nb);
			return false;
		}
		c = V3(c.x / (float32)nb, c.y / (float32)nb, c.z / (float32)nb);
		char pos[160];
		const bool ok = JugerPosition(doc, os, sc, ac, NK_BANC_CORNE_LONGUEUR, c, res, pos, 160u);
		std::snprintf(detail, cap, "bout : %s, %u sommets", pos, nb);
		return ok;
	}

	// Un relief (bosse, creux) : le CENTRE des sommets souleves (ou enfonces) de
	// plus de la moitie de `hauteur` doit etre a P(sc, ac) + n * hauteur, et leur
	// hauteur moyenne a 30 % pres de `hauteur`.
	// ⚠️ LA PREMIERE VERSION COMPTAIT LES SOMMETS D'UNE BOITE ELARGIE, et la
	//    relecture du 29/09 l'a prise en defaut : une bosse ou un creux decale
	//    d'UNE FACE passait. Le centre, lui, se decale d'une face -- et rougit.
	bool JugerRelief(const NkR32Document &doc, const char *os, float32 sc, float32 ac, float32 hauteur,
					 const NkEditMesh &m, char *detail, uint32 cap, const NkBancResolution &res) {
		if (doc.Trouver(os) < 0) {
			std::snprintf(detail, cap, "os `%s` absent du document", os);
			return false;
		}
		PoserBase(doc);
		const float32 signe = hauteur < 0.f ? -1.f : 1.f;
		const float32 absH = hauteur * signe;
		NkVec3f c = V3(0.f, 0.f, 0.f);
		float32 somme = 0.f;
		uint32 nb = 0u;
		for (uint32 v = 0u; v < m.VertCount(); ++v) {
			const NkBancHauteur h = Hauteur(doc, os, m.verts[v].pos);
			if (!h.estValide || h.s < 0.02f || h.s > 0.98f) {
				continue;
			}
			const float32 hs = h.h * signe;
			if (hs <= 0.5f * absH || hs > 3.f * absH) {
				continue;
			}
			if (!EstLePlusProche(doc, os, m, v)) {
				continue;
			}
			c = V3(c.x + m.verts[v].pos.x, c.y + m.verts[v].pos.y, c.z + m.verts[v].pos.z);
			somme += h.h;
			++nb;
		}
		if (nb == 0u) {
			std::snprintf(detail, cap, "relief ABSENT sur `%s`", os);
			return false;
		}
		c = V3(c.x / (float32)nb, c.y / (float32)nb, c.z / (float32)nb);
		const float32 moyenne = somme / (float32)nb;
		const float32 rapport = moyenne / hauteur;
		char pos[160];
		const bool place = JugerPosition(doc, os, sc, ac, moyenne, c, res, pos, 160u);
		std::snprintf(detail, cap, "centre : %s ; hauteur %.4f (attendu %.2f), %u sommets", pos, (double)moyenne,
					  (double)hauteur, nb);
		return place && rapport >= 0.7f && rapport <= 1.3f;
	}

	bool JugerBosse(const NkR32Document &doc, const NkEditMesh &m, char *detail, uint32 cap,
					const NkBancResolution &res = NkBancResolution{}) {
		return JugerRelief(doc, "bras_g", 0.5f, 0.25f, NK_BANC_BOSSE, m, detail, cap, res);
	}

	bool JugerCreux(const NkR32Document &doc, const NkEditMesh &m, char *detail, uint32 cap,
					const NkBancResolution &res = NkBancResolution{}) {
		return JugerRelief(doc, "torse", 0.5f, 1.f / 12.f, -NK_BANC_CREUX, m, detail, cap, res);
	}

	uint32 Juger3(const NkR32Document &doc, const NkEditMesh &m, bool ok[3], char det[3][160]) {
		ok[0] = JugerCorne(doc, "tete", m, det[0], 160u);
		ok[1] = JugerBosse(doc, m, det[1], 160u);
		ok[2] = JugerCreux(doc, m, det[2], 160u);
		return (ok[0] ? 1u : 0u) + (ok[1] ? 1u : 0u) + (ok[2] ? 1u : 0u);
	}

	// ========================================
	// LA PILE : TROIS RETOUCHES A LA MAIN
	// ========================================

	NkR32Operation Op(NkR32Verbe verbe, const char *cible, float32 valeur, const char *groupe) {
		NkR32Operation o;
		o.auteur = NkR32Auteur::Nk_R32Auteur_Humain;
		o.verbe = verbe;
		std::snprintf(o.cible, sizeof(o.cible), "%s", cible);
		std::snprintf(o.groupe, sizeof(o.groupe), "%s", groupe ? groupe : "");
		o.valeur = valeur;
		return o;
	}

	bool Convertir(const NkR32Etat &e, const char *os, float32 s0, float32 s1, float32 a0, float32 a1, char *cible) {
		NkVector<uint8> sel;
		Cliquer(e, os, s0, s1, a0, a1, sel);
		char pq[256];
		const bool ok = NkR32ConvertirSelection(e, sel.Data(), (uint32)sel.Size(), cible, NK_R32_CIBLE_MAX, pq, 256u);
		if (!ok) {
			std::printf("       conversion refusee : %s\n", pq);
		}
		return ok;
	}

	// La corne : extruder, affiner, extruder, affiner — le geste d'un artiste.
	bool PileCorne(const NkR32Document &doc, const char *cibleCorne, NkR32Pile &pile) {
		char pq[256];
		bool ok = pile.Ajouter(doc, Op(NkR32Verbe::Nk_R32Verbe_Extruder, cibleCorne, 0.12f, "corne_g"), pq, 256u);
		ok = ok && pile.Ajouter(doc, Op(NkR32Verbe::Nk_R32Verbe_Echelle, "groupe:corne_g", 0.6f, nullptr), pq, 256u);
		ok = ok && pile.Ajouter(doc, Op(NkR32Verbe::Nk_R32Verbe_Extruder, "groupe:corne_g", 0.10f, "corne_g"), pq, 256u);
		ok = ok && pile.Ajouter(doc, Op(NkR32Verbe::Nk_R32Verbe_Echelle, "groupe:corne_g", 0.4f, nullptr), pq, 256u);
		if (!ok) {
			std::printf("       pile refusee : %s\n", pq);
		}
		return ok;
	}

	bool PileBosse(const NkR32Document &doc, const char *cible, NkR32Pile &pile) {
		char pq[256];
		NkR32Operation o = Op(NkR32Verbe::Nk_R32Verbe_Deplacer, cible, 0.f, nullptr);
		o.local = V3(NK_BANC_BOSSE, 0.f, 0.f);
		return pile.Ajouter(doc, o, pq, 256u);
	}

	bool PileCreux(const NkR32Document &doc, const char *cible, NkR32Pile &pile) {
		char pq[256];
		NkR32Operation o = Op(NkR32Verbe::Nk_R32Verbe_Inserer, cible, 0.03f, "creux");
		o.valeur2 = -NK_BANC_CREUX;
		return pile.Ajouter(doc, o, pq, 256u);
	}

	struct NkBancCibles {
			char corne[NK_R32_CIBLE_MAX] = {0};
			char bosse[NK_R32_CIBLE_MAX] = {0};
			char creux[NK_R32_CIBLE_MAX] = {0};
	};

	bool Rejouer(const NkR32Document &doc, const NkR32Pile &pile, NkR32Etat &e, const NkR32Confirmation *c = nullptr,
				 uint32 nc = 0u) {
		char pq[256];
		const bool ok = NkR32Rejouer(doc, pile, c, nc, e, pq, 256u);
		if (!ok) {
			std::printf("       rejeu refuse : %s\n", pq);
		}
		return ok;
	}

	uint32 Nombre(const NkR32Etat &e, NkR32Statut s) {
		uint32 n = 0u;
		for (uint32 i = 0u; i < (uint32)e.resultats.Size(); ++i) {
			if (e.resultats[i].statut == s) {
				++n;
			}
		}
		return n;
	}

	void Montrer(const NkR32Etat &e) {
		for (uint32 i = 0u; i < (uint32)e.resultats.Size(); ++i) {
			const NkR32Resultat &r = e.resultats[i];
			std::printf("       [%u] %-11s %-30s %s\n", i, NkR32StatutNom(r.statut), NkR32CauseNom(r.cause), r.message);
		}
	}

	// ========================================
	// BATTERIES
	// ========================================

	void BancBase(const NkR32Document &doc0) {
		std::printf("\n== peau C2 de base ==\n");
		NkR32Peau peau;
		char pq[256];
		const bool ok = NkR32ConstruirePeau(doc0, peau, pq, 256u);
		Verifier(ok, "base/construite", "%s", ok ? "" : pq);
		if (!ok) {
			return;
		}
		const NkEditMesh &m = peau.maillage;
		uint32 nonManif = 0u;
		uint32 bord = 0u;
		for (uint32 k = 0u; k < (uint32)m.edges.Size(); ++k) {
			if (!m.edges[k].alive) {
				continue;
			}
			if (m.edges[k].radialCount > 2u) {
				++nonManif;
			}
			if (m.edges[k].radialCount == 1u) {
				++bord;
			}
		}
		uint32 sortantes = 0u;
		uint32 tubes = 0u;
		for (uint32 f = 0u; f < m.FaceCount(); ++f) {
			const NkR32AdresseFace &adr = peau.adresses[f];
			if (adr.lieu != NkR32Lieu::Nk_R32Lieu_Tube) {
				continue;
			}
			++tubes;
			NkVec3f p;
			NkVec3f n;
			NkR32PointAnalytique(doc0, peau.os[adr.os].nom, 0.5f * (adr.s0 + adr.s1), 0.5f * (adr.a0 + adr.a1), p, n);
			const NkVec3f fn = m.faces[f].normal;
			if (fn.x * n.x + fn.y * n.y + fn.z * n.z > 0.5f) {
				++sortantes;
			}
		}
		Verifier(nonManif == 0u && bord == 0u, "base/variete", "faces=%u sommets=%u aretes-non-manifold=%u bord=%u",
				 m.FaceCount(), m.VertCount(), nonManif, bord);
		Verifier(sortantes == tubes, "base/normales-sortantes", "%u/%u faces de tube", sortantes, tubes);
		bool origines = true;
		for (uint32 f = 0u; f < m.FaceCount(); ++f) {
			if (m.faces[f].origine != f + 1u) {
				origines = false;
			}
		}
		Verifier(origines, "base/origine=f+1", "chaque face porte son adresse");
		NkR32Peau peau2;
		NkR32ConstruirePeau(doc0, peau2, pq, 256u);
		const uint64 h1 = NkR32Empreinte(peau.maillage);
		const uint64 h2 = NkR32Empreinte(peau2.maillage);
		Verifier(h1 == h2, "base/deterministe", "empreinte %016llx / %016llx", (unsigned long long)h1,
				 (unsigned long long)h2);
		NkR32Document impair = doc0;
		impair.anneaux = 11u;
		Verifier(!NkR32ConstruirePeau(impair, peau2, pq, 256u), "base/refus-anneaux-impairs", "%s", pq);
		NkR32Document parallele = doc0;
		parallele.os[(uint32)parallele.Trouver("tete")].reference = V3(0.f, 1.f, 0.f);
		Verifier(!NkR32ConstruirePeau(parallele, peau2, pq, 256u), "base/refus-reference-parallele", "%s", pq);
	}

	// Le coeur de G0 : le taux de survie, sur trois chaines.
	void BancSurvie(const NkR32Document &doc0, const NkR32Pile &pile, NkBancCibles &cibles) {
		std::printf("\n== taux de survie : 3 retouches x 3 regenerations ==\n");
		NkR32Etat e0;
		if (!Rejouer(doc0, pile, e0)) {
			Verifier(false, "survie/rejeu-origine", "le rejeu sur le document d'origine echoue");
			return;
		}
		bool ok[3];
		char det[3][160];
		const uint32 temoinPlus = Juger3(doc0, e0.maillage, ok, det);
		Verifier(temoinPlus == 3u, "survie/temoin-positif(doc0+pile)", "%u/3 | %s | %s | %s", temoinPlus, det[0], det[1],
				 det[2]);
		{
			// LE JUGE DOIT ROUGIR SUR UNE CORNE POSEE UNE FACE A COTE (a + 1/12).
			NkR32Pile decalee;
			PileCorne(doc0, "adresse:(tete, 0.5..0.75, 0.25..0.416667)", decalee);
			NkR32Etat ed;
			Rejouer(doc0, decalee, ed);
			char dc[160];
			const bool accepte = JugerCorne(doc0, "tete", ed.maillage, dc, 160u);
			Verifier(!accepte, "survie/juge-rougit-a-une-face(corne)", "corne decalee d'un cran : %s", dc);
			NkR32Pile bd;
			PileBosse(doc0, "adresse:(bras_g, 0.4..0.6, 0.25..0.416667)", bd);
			Rejouer(doc0, bd, ed);
			const bool bosseAcceptee = JugerBosse(doc0, ed.maillage, dc, 160u);
			Verifier(!bosseAcceptee, "survie/juge-rougit-a-une-face(bosse)", "bosse decalee d'un cran : %s", dc);
			NkR32Pile cd;
			PileCreux(doc0, "adresse:(torse, 0.375..0.625, 0.083333..0.25)", cd);
			Rejouer(doc0, cd, ed);
			const bool creuxAccepte = JugerCreux(doc0, ed.maillage, dc, 160u);
			Verifier(!creuxAccepte, "survie/juge-rougit-a-une-face(creux)", "creux decale d'un cran : %s", dc);
		}
		const uint32 temoinMoins = Juger3(doc0, e0.peau.maillage, ok, det);
		Verifier(temoinMoins == 0u, "survie/temoin-negatif(doc0 sans pile)", "%u/3 : le juge ne voit rien la ou rien n'est",
				 temoinMoins);
		std::printf("       journal classique : %u commandes, cibles en INDICES\n", e0.journal.Count());

		uint32 total[3] = {0u, 0u, 0u};
		uint32 corneR32 = 0u;
		uint32 fauxRejeu = 0u;
		const char *noms[3] = {"ecraser", "indices", "R32"};
		const char *regen[3] = {"R1 proportions", "R2 resolution", "R3 pose"};
		NkR32Document d = doc0;
		for (int32 k = 1; k <= 3; ++k) {
			Regenerer(d, k);
			NkR32Peau peau;
			char pq[256];
			NkR32ConstruirePeau(d, peau, pq, 256u);
			// ecraser
			const uint32 n0 = Juger3(d, peau.maillage, ok, det);
			std::printf("       %-15s %-8s %u/3 | corne: %s\n", regen[k - 1], noms[0], n0, det[0]);
			total[0] += n0;
			// indices
			NkEditMesh mi = peau.maillage;
			const uint32 appliquees = e0.journal.ReplayOnto(mi);
			const uint32 n1 = Juger3(d, mi, ok, det);
			std::printf("       %-15s %-8s %u/3 | %u/%u commandes « appliquees » | corne: %s\n", regen[k - 1], noms[1],
						n1, appliquees, e0.journal.Count(), det[0]);
			if (appliquees == e0.journal.Count() && n1 < 3u) {
				fauxRejeu += 3u - n1;
			}
			total[1] += n1;
			// R32
			NkR32Etat e;
			Rejouer(d, pile, e);
			const uint32 n2 = Juger3(d, e.maillage, ok, det);
			std::printf("       %-15s %-8s %u/3 | corne: %s | bosse: %s | creux: %s\n", regen[k - 1], noms[2], n2, det[0],
						det[1], det[2]);
			total[2] += n2;
			if (ok[0]) {
				++corneR32;
			}
		}
		std::printf("       TAUX DE SURVIE  ecraser %u/9   indices %u/9   R32 %u/9\n", total[0], total[1], total[2]);
		std::printf("       rejeu par indices : %u retouche(s) perdue(s) ou deplacee(s) SANS AUCUNE ERREUR signalee\n",
					fauxRejeu);
		Verifier(total[0] == 0u, "survie/zero-mesure(ecraser)", "%u/9 : c'est le point de depart, MESURE", total[0]);
		Verifier(total[2] == 9u, "survie/R32", "%u/9", total[2]);
		Verifier(corneR32 == 3u, "G0/corne-survit-a-3-regenerations", "%u/3", corneR32);
		(void)cibles;
	}

	void BancR32_7(const NkR32Document &doc0, const NkR32Pile &pile, const NkBancCibles &cibles) {
		std::printf("\n== R32.7 ==\n");
		char pq[256];
		NkR32Pile vide;
		NkR32Etat ev;
		Rejouer(doc0, vide, ev);
		const uint64 hBase = NkR32Empreinte(ev.peau.maillage);
		const uint64 hVide = NkR32Empreinte(ev.maillage);
		Verifier(hBase == hVide, "R32.7/pile-vide=base-au-bit", "%016llx / %016llx", (unsigned long long)hBase,
				 (unsigned long long)hVide);
		NkR32Etat e1;
		NkR32Etat e2;
		Rejouer(doc0, pile, e1);
		NkR32Document copie = doc0;
		Rejouer(copie, pile, e2);
		const uint64 a = NkR32Empreinte(e1.maillage);
		const uint64 b = NkR32Empreinte(e2.maillage);
		Verifier(a == b && a != hBase, "R32.7/deterministe", "%016llx / %016llx (et differente de la base)",
				 (unsigned long long)a, (unsigned long long)b);

		NkR32Pile p = pile;
		NkR32Operation z = Op(NkR32Verbe::Nk_R32Verbe_Extruder, "zone:orbite_g", 0.1f, nullptr);
		NkR32Operation q = Op(NkR32Verbe::Nk_R32Verbe_Extruder, "n'importe quoi", 0.1f, nullptr);
		NkR32Pile avant;
		avant.ops.PushBack(z);
		avant.ops.PushBack(q);
		avant.ops.PushBack(Op(NkR32Verbe::Nk_R32Verbe_Extruder, "groupe:fantome", 0.1f, nullptr));
		for (uint32 i = 0u; i < (uint32)p.ops.Size(); ++i) {
			avant.ops.PushBack(p.ops[i]);
		}
		NkR32Etat e;
		Rejouer(doc0, avant, e);
		Montrer(e);
		Verifier(e.resultats[0].cause == NkR32Cause::Nk_R32Cause_DesignationPasEnG0 && e.resultats[0].message[0] != 0,
				 "R32.7/refus-nomme(zone:)", "%s", e.resultats[0].message);
		Verifier(e.resultats[1].cause == NkR32Cause::Nk_R32Cause_DesignationInconnue, "R32.7/refus-nomme(inconnue)",
				 "%s", e.resultats[1].message);
		Verifier(e.resultats[2].cause == NkR32Cause::Nk_R32Cause_GroupeInconnu, "R32.7/refus-nomme(groupe)", "%s",
				 e.resultats[2].message);
		bool ok[3];
		char det[3][160];
		const uint32 n = Juger3(doc0, e.maillage, ok, det);
		Verifier(n == 3u, "R32.7/orpheline-n'annule-pas-les-autres", "3 orphelines en tete de pile, retouches %u/3", n);

		// La pile en texte : lisible, et l'aller-retour ne change rien.
		static char t1[NK_BANC_TEXTE_MAX];
		static char t2[NK_BANC_TEXTE_MAX];
		NkR32PileEcrire(pile, t1, NK_BANC_TEXTE_MAX);
		NkR32Pile relue;
		const bool lu = NkR32PileLire(t1, relue, pq, 256u);
		NkR32PileEcrire(relue, t2, NK_BANC_TEXTE_MAX);
		NkR32Etat er;
		Rejouer(doc0, relue, er);
		Verifier(lu && std::strcmp(t1, t2) == 0 && NkR32Empreinte(er.maillage) == a, "R32.7/pile-texte-aller-retour",
				 "%u operations, texte identique, meme maillage", (uint32)relue.ops.Size());
		std::printf("%s", t1);

		// Une selection qui n'est pas une boite : refus nomme, pas d'adresse fausse.
		NkVector<uint8> sel;
		Cliquer(ev, "tete", 0.5f, 0.75f, 0.16f, 0.34f, sel);
		NkVector<uint8> l = sel;
		Cliquer(ev, "tete", 0.25f, 0.5f, 0.16f, 0.25f, sel);
		for (uint32 i = 0u; i < (uint32)sel.Size(); ++i) {
			l[i] = (uint8)(l[i] | sel[i]);
		}
		char cible[NK_R32_CIBLE_MAX];
		const bool conv = NkR32ConvertirSelection(ev, l.Data(), (uint32)l.Size(), cible, NK_R32_CIBLE_MAX, pq, 256u);
		Verifier(!conv, "R32.7/selection-en-L-refusee", "%s", pq);

		// Toute face nee d'une operation porte une origine (sauf les faces SANS mere).
		uint32 sans = 0u;
		for (uint32 f = 0u; f < e1.maillage.FaceCount(); ++f) {
			if (e1.maillage.faces[f].alive && e1.maillage.faces[f].origine == 0u) {
				++sans;
			}
		}
		std::printf("       mesure : %u faces sans origine apres la pile (faces nees SANS mere : bandes d'insertion)\n",
					sans);
		(void)cibles;
	}

	void BancCinqCas(const NkR32Document &doc0, const NkBancCibles &cibles) {
		std::printf("\n== les cinq cas du 3.2bis ==\n");
		char pq[256];
		bool ok[3];
		char det[3][160];

		// ── 1. RENOMME ──────────────────────────────────────────────
		{
			NkR32Pile pile;
			PileCorne(doc0, cibles.corne, pile);
			NkR32Document d = doc0;
			d.RenommerOs("tete", "crane", pq, 256u);
			NkR32Etat e;
			Rejouer(d, pile, e);
			char dc[160];
			const bool corne = JugerCorne(d, "crane", e.maillage, dc, 160u);
			Verifier(e.resultats[0].statut == NkR32Statut::Nk_R32Statut_AConfirmer && !corne,
					 "cas1/renomme-sans-confirmation", "%s -- %s", NkR32StatutNom(e.resultats[0].statut),
					 e.resultats[0].message);
			NkR32Confirmation c;
			std::snprintf(c.ancien, sizeof(c.ancien), "tete");
			std::snprintf(c.nouveau, sizeof(c.nouveau), "crane");
			c.estAccepte = true;
			Rejouer(d, pile, e, &c, 1u);
			const bool corneOui = JugerCorne(d, "crane", e.maillage, dc, 160u);
			Verifier(e.resultats[0].statut == NkR32Statut::Nk_R32Statut_Reportee && corneOui,
					 "cas1/renomme-confirme", "%s -- %s", NkR32StatutNom(e.resultats[0].statut), dc);
			c.estAccepte = false;
			Rejouer(d, pile, e, &c, 1u);
			const bool corneNon = JugerCorne(d, "crane", e.maillage, dc, 160u);
			Verifier(e.resultats[0].statut == NkR32Statut::Nk_R32Statut_Orpheline &&
						 e.resultats[0].cause == NkR32Cause::Nk_R32Cause_RenommageRefuse && !corneNon,
					 "cas1/renomme-refuse=orpheline", "%s", e.resultats[0].message);
		}

		// ── 2. COUPE : la bosse est A CHEVAL sur la coupe ─────────
		{
			NkR32Pile pile;
			PileBosse(doc0, cibles.bosse, pile);
			NkR32Document d = doc0;
			d.CouperOs("bras_g", 0.5f, "bras_g_haut", "bras_g_bas", pq, 256u);
			NkR32Etat e;
			Rejouer(d, pile, e);
			// Le bras coupe a la meme geometrie que l'original : on juge sur doc0.
			NkBancResolution rc;
			rc.doc = &d;
			rc.os = "bras_g_bas";
			const bool bosse = JugerBosse(doc0, e.maillage, det[1], 160u, rc);
			Verifier(e.resultats[0].statut == NkR32Statut::Nk_R32Statut_Reportee && bosse, "cas2/coupe-report-calcule",
					 "%s -- %s", e.resultats[0].message, det[1]);
			// Une coupe a t = 0,3 : la bosse tombe entierement dans le morceau bas et
			// doit y etre RENORMALISEE ; c'est la geometrie qui juge, pas le statut.
			NkR32Document d3 = doc0;
			d3.CouperOs("bras_g", 0.3f, "bras_g_haut", "bras_g_bas", pq, 256u);
			NkR32Etat e3;
			Rejouer(d3, pile, e3);
			NkBancResolution r3;
			r3.doc = &d3;
			r3.os = "bras_g_bas";
			const bool bosse3 = JugerBosse(doc0, e3.maillage, det[1], 160u, r3);
			Verifier(e3.resultats[0].statut == NkR32Statut::Nk_R32Statut_Reportee && bosse3, "cas2/coupe-a-0.3",
					 "%s -- %s", e3.resultats[0].message, det[1]);
		}

		// ── 3. FUSION : les deux morceaux se ressoudent, et une collision se voit ─
		{
			NkR32Document d = doc0;
			NkR32Pile pile;
			// A : faite avant la coupe, sur bras_g, au milieu.
			NkR32Operation a = Op(NkR32Verbe::Nk_R32Verbe_Deplacer, "adresse:(bras_g, 0.45..0.55, 0.166667..0.333333)",
								  0.f, nullptr);
			a.local = V3(0.02f, 0.f, 0.f);
			pile.Ajouter(d, a, pq, 256u);
			d.CouperOs("bras_g", 0.5f, "bras_g_haut", "bras_g_bas", pq, 256u);
			// B : faite apres la coupe, sur le morceau du haut, AU MEME ENDROIT que la moitie de A.
			NkR32Operation b = Op(NkR32Verbe::Nk_R32Verbe_Deplacer,
								  "adresse:(bras_g_haut, 0.9..1.0, 0.166667..0.333333)", 0.f, nullptr);
			b.local = V3(0.02f, 0.f, 0.f);
			pile.Ajouter(d, b, pq, 256u);
			const bool fus = d.FusionnerOs("bras_g_haut", "bras_g_bas", "bras_g", pq, 256u);
			NkR32Etat e;
			Rejouer(d, pile, e);
			Montrer(e);
			const bool appliquees = Nombre(e, NkR32Statut::Nk_R32Statut_Reportee) == 2u;
			const bool collision = (e.resultats[0].alertes & NK_R32_ALERTE_COLLISION) &&
								   (e.resultats[1].alertes & NK_R32_ALERTE_COLLISION);
			Verifier(fus && appliquees && collision, "cas3/fusion-report+collision-signalee", "%s",
					 e.resultats[1].message);
			// Une fusion, UN report : la bosse tombe a sa place, sans alerte.
			{
				NkR32Pile p2;
				NkR32Document d2 = doc0;
				d2.CouperOs("bras_g", 0.5f, "bras_g_haut", "bras_g_bas", pq, 256u);
				NkR32Operation c = Op(NkR32Verbe::Nk_R32Verbe_Deplacer,
									  "adresse:(bras_g_haut, 0.8..1.0, 0.166667..0.333333)", 0.f, nullptr);
				c.local = V3(NK_BANC_BOSSE, 0.f, 0.f);
				p2.Ajouter(d2, c, pq, 256u);
				d2.FusionnerOs("bras_g_haut", "bras_g_bas", "bras_g", pq, 256u);
				NkR32Etat e2;
				Rejouer(d2, p2, e2);
				const bool bosse = JugerBosse(d2, e2.maillage, det[1], 160u);
				const bool sansAlerte = e2.resultats[0].alertes == 0u;
				Verifier(bosse && sansAlerte && e2.resultats[0].statut == NkR32Statut::Nk_R32Statut_Reportee,
						 "cas3/fusion-report-a-sa-place", "%s -- %s", e2.resultats[0].message, det[1]);
			}
			// Deux retouches ADJACENTES sur les deux morceaux : a la resolution de
			// l'os fusionne elles visent la MEME face. Ca ne se resout pas en silence.
			{
				NkR32Pile p3;
				NkR32Document d3 = doc0;
				d3.CouperOs("bras_g", 0.5f, "bras_g_haut", "bras_g_bas", pq, 256u);
				NkR32Operation c = Op(NkR32Verbe::Nk_R32Verbe_Deplacer,
									  "adresse:(bras_g_haut, 0.8..1.0, 0.166667..0.333333)", 0.f, nullptr);
				c.local = V3(NK_BANC_BOSSE, 0.f, 0.f);
				p3.Ajouter(d3, c, pq, 256u);
				NkR32Operation c2 = c;
				std::snprintf(c2.cible, sizeof(c2.cible), "adresse:(bras_g_bas, 0.0..0.2, 0.166667..0.333333)");
				p3.Ajouter(d3, c2, pq, 256u);
				d3.FusionnerOs("bras_g_haut", "bras_g_bas", "bras_g", pq, 256u);
				NkR32Etat e3;
				Rejouer(d3, p3, e3);
				Montrer(e3);
				const bool signalee = (e3.resultats[0].alertes & NK_R32_ALERTE_COLLISION) &&
									  (e3.resultats[1].alertes & NK_R32_ALERTE_COLLISION);
				Verifier(signalee, "cas3/fusion-collision-par-reechantillonnage", "%s", e3.resultats[1].message);
			}
		}

		// ── 4. LA REFERENCE TOURNE — le cas dangereux ─────────────────
		{
			NkR32Pile pile;
			PileCorne(doc0, cibles.corne, pile);
			const NkVec3f tournee = V3(1.f, 0.f, 0.f);  // +Z -> +X : un quart de tour autour de la tete
			// (a) versionnee : reportee au BON endroit, et l'artiste est PREVENU.
			NkR32Document d = doc0;
			d.ReorienterOs("tete", tournee, pq, 256u);
			NkR32Etat e;
			Rejouer(d, pile, e);
			char dc[160];
			const bool corne = JugerCorne(doc0, "tete", e.maillage, dc, 160u);
			const bool prevenu = (e.resultats[0].alertes & NK_R32_ALERTE_REORIENTATION) != 0;
			Verifier(corne && prevenu, "cas4/reorientation-versionnee", "%s -- %s", dc, e.resultats[0].message);
			// (a') coupe PUIS reorientation des deux morceaux : chaque morceau a sa
			// propre histoire de reference (defaut trouve en relecture le 29/09).
			NkR32Pile pb;
			PileBosse(doc0, cibles.bosse, pb);
			NkR32Document dc2 = doc0;
			dc2.CouperOs("bras_g", 0.5f, "bras_g_haut", "bras_g_bas", pq, 256u);
			dc2.ReorienterOs("bras_g_haut", V3(0.f, 0.f, 1.f), pq, 256u);
			dc2.ReorienterOs("bras_g_bas", V3(0.f, 0.f, 1.f), pq, 256u);
			NkR32Etat ec2;
			Rejouer(dc2, pb, ec2);
			NkBancResolution r4;
			r4.doc = &dc2;
			r4.os = "bras_g_bas";
			const bool bosse = JugerBosse(doc0, ec2.maillage, det[1], 160u, r4);
			Verifier(bosse && (ec2.resultats[0].alertes & NK_R32_ALERTE_REORIENTATION) != 0,
					 "cas4/coupe-puis-double-reorientation", "%s -- %s", det[1], ec2.resultats[0].message);
			// (b) hors versionnement (le vecteur edite a la main) : REFUSE, rien n'est pose.
			NkR32Document dm = doc0;
			dm.os[(uint32)dm.Trouver("tete")].reference = tournee;
			NkR32Etat em;
			Rejouer(dm, pile, em);
			float32 hMax = 0.f;
			PoserBase(dm);
			for (uint32 v = 0u; v < em.maillage.VertCount(); ++v) {
				const NkBancHauteur h = Hauteur(dm, "tete", em.maillage.verts[v].pos);
				if (h.estValide && h.s > -0.05f && h.s < 1.05f && h.h > hMax) {
					hMax = h.h;
				}
			}
			const uint32 posees = Nombre(em, NkR32Statut::Nk_R32Statut_Appliquee) +
								  Nombre(em, NkR32Statut::Nk_R32Statut_Reportee);
			Verifier(em.resultats[0].cause == NkR32Cause::Nk_R32Cause_ReferenceChangeeHorsVersion && posees == 0u &&
						 hMax < 0.05f,
					 "cas4/reference-hors-version=REFUS", "aucune corne posee (%u appliquees, relief max %.4f) -- %s",
					 posees, (double)hMax, em.resultats[0].message);
		}

		// ── 5. DISPARU ─────────────────────────────────────────────
		{
			NkR32Pile pile;
			NkR32Operation o = Op(NkR32Verbe::Nk_R32Verbe_Deplacer, "adresse:(avant_bras_g, 0.4..0.6, 0.0..0.25)", 0.f,
								  nullptr);
			o.local = V3(0.02f, 0.f, 0.f);
			pile.Ajouter(doc0, o, pq, 256u);
			PileCorne(doc0, cibles.corne, pile);
			NkR32Document d = doc0;
			d.SupprimerOs("avant_bras_g", pq, 256u);
			NkR32Etat e;
			Rejouer(d, pile, e);
			char dc[160];
			const bool corne = JugerCorne(d, "tete", e.maillage, dc, 160u);
			Verifier(e.resultats[0].cause == NkR32Cause::Nk_R32Cause_OsDisparu && (uint32)e.resultats.Size() == 5u &&
						 (uint32)pile.ops.Size() == 5u && corne,
					 "cas5/disparu=orpheline-gardee", "%s ; la corne reste : %s", e.resultats[0].message, dc);
			// Retire A LA MAIN (sans lignee) : meme verdict, jamais un report ailleurs.
			NkR32Document dm = doc0;
			NkVector<NkR32Os> reste;
			for (uint32 i = 0u; i < (uint32)dm.os.Size(); ++i) {
				if (std::strcmp(dm.os[i].nom, "avant_bras_g") != 0) {
					reste.PushBack(dm.os[i]);
				}
			}
			dm.os = reste;
			Rejouer(dm, pile, e);
			Verifier(e.resultats[0].cause == NkR32Cause::Nk_R32Cause_OsDisparu, "cas5/disparu-hors-lignee", "%s",
					 e.resultats[0].message);
		}
	}

}  // namespace

// ============================================================
// POINT D'ENTREE
// ============================================================

int main() {
	std::printf("NKR32Harness -- R32 (G0, NKCraft doc 04). NK_R32_MUTE=%s\n",
				std::getenv("NK_R32_MUTE") ? std::getenv("NK_R32_MUTE") : "(aucune)");
	NkR32Document doc0;
	if (!DocDepart(doc0)) {
		return 1;
	}
	// NK_R32_GENERATEUR=2 : le MEME banc sur la peau G1 (NkCreaturePeau : tube
	// continu, arcs de conge aux joints, capuchons en grille). R32 ne doit pas
	// dependre du generateur de la base.
	if (std::getenv("NK_R32_GENERATEUR") && std::getenv("NK_R32_GENERATEUR")[0] == '2') {
		doc0.generateur = 2u;
	}
	std::printf("generateur de la base : %u\n", (unsigned)doc0.generateur);
	BancBase(doc0);

	// Les retouches A LA MAIN : des clics sur la base, convertis en adresses.
	std::printf("\n== selection libre -> adresse (04 §10.3) ==\n");
	NkR32Pile vide;
	NkR32Etat e0;
	if (!Rejouer(doc0, vide, e0)) {
		return 1;
	}
	NkBancCibles cibles;
	const bool c1 = Convertir(e0, "tete", 0.5f, 0.75f, NK_BANC_UN_SIXIEME, NK_BANC_UN_TIERS, cibles.corne);
	const bool c2 = Convertir(e0, "bras_g", 0.4f, 0.6f, NK_BANC_UN_SIXIEME, NK_BANC_UN_TIERS, cibles.bosse);
	const bool c3 = Convertir(e0, "torse", 0.375f, 0.625f, 0.f, NK_BANC_UN_SIXIEME, cibles.creux);
	Verifier(c1 && c2 && c3, "selection/convertie", "corne `%s`", cibles.corne);
	std::printf("       bosse `%s`\n       creux `%s`\n", cibles.bosse, cibles.creux);
	if (!(c1 && c2 && c3)) {
		std::printf("\n%d critere(s) ROUGE(S) sur %d\n", nkEchecs, nkVerifs);
		return 1;
	}
	NkR32Pile pile;
	const bool p1 = PileCorne(doc0, cibles.corne, pile);
	const bool p2 = PileBosse(doc0, cibles.bosse, pile);
	const bool p3 = PileCreux(doc0, cibles.creux, pile);
	Verifier(p1 && p2 && p3, "pile/construite", "%u operations", (uint32)pile.ops.Size());

	BancSurvie(doc0, pile, cibles);
	BancR32_7(doc0, pile, cibles);
	BancCinqCas(doc0, cibles);

	std::printf("\n%d critere(s) ROUGE(S) sur %d\n", nkEchecs, nkVerifs);
	return nkEchecs == 0 ? 0 : 1;
}

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Creation Date: 2026-09-29
// ============================================================
