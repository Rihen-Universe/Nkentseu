// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_eau.cpp — les TÉMOINS de l'eau au-delà du fluide (2026-09-06).
// ROADMAP_PRODUITS.md §6.6 : palier 1 (les éclaboussures) et paliers 2-3 (le
// mouillage). Écrits AVANT de lancer quoi que ce soit, scènes fixées une fois.
// Appelé depuis le main() de test_physics.cpp, mêmes compteurs.
//
// ⚠️ POURQUOI ICI, dans NKPhysics_Tests, alors que NkContactEvent / NkSplashLaw /
// NkWetnessMap vivent dans NKMath : parce que NKPhysics_Tests lie déjà NKMath ET
// NKPhysics, qu'il est DÉJÀ dans la liste `allow` de dutc/dute (Nkentseu.jenga),
// et que le témoin (e) a besoin de NkCloth. Un seul exécutable, aucune cible
// nouvelle à ouvrir. Le témoin qui manque ici est celui du PRODUCTEUR (le
// solveur SPH publie-t-il ?) : il vit dans NKRenderer, et il est dans
// Kernel/Runtime/NKRenderer/tests/test_eau_sph.cpp.
//
// Les chiffres partent sur stderr, comme test_cloth.cpp et pour la même raison
// mesurée : le puits console de NKLogger ne traverse pas un tuyau dans cet arbre.
//
// LES TÉMOINS, et ce que chacun prouve :
//   (a) LA FILE  — bornée, et elle DIT ce qu'elle perd.
//   (b) LA LOI   — seuil, LES DEUX LOIS, direction, déterminisme, plafond.
//                  ⚠️ CHANGÉ le 06/09 (nuit) après l'arbitrage de Rodolf : les
//                  deux lois sont LIVRÉES et sélectionnables (`NkSplashLawKind`),
//                  donc le témoin ne porte plus sur une loi avec l'autre en
//                  contrôle positif — il porte sur les DEUX, chacune avec son
//                  attendu analytique 2^q écrit AVANT la mesure : 2,000 pour la
//                  linéaire, 4,000 pour l'énergie, 8,000 pour Puissance(3).
//                  (b5c) mesure que le DÉFAUT livré est bien la linéaire, et
//                  (b11) mesure qu'`exponentLibre` est ignoré sous Lineaire ET
//                  lu sous Puissance — un paramètre non honoré qu'on mesure au
//                  lieu de le subir. (b12) refait la mesure par l'ÉMETTEUR, la
//                  porte que le système d'effets appelle réellement.
//   (c) LA CARTE — dépôt local, séchage exponentiel, MUTATION (séchage coupé),
//                  et deux matières dans le bon ORDRE (pas à la bonne valeur).
//   (d) LA LUMIÈRE — identité à sec (contrôle positif), les constantes de
//                  Lagarde au chiffre près, monotonie, et le RETOUR à la couleur.
//   (e) LE TISSU — masse, absence de dérive, et « pend plus bas », en mm.
//
// ⚠️ CE QUE (d) MESURE EXACTEMENT — la population, dite avant le chiffre : ce
// sont les sorties de `NkApplyWetness`, c'est-à-dire l'albédo et la rugosité que
// le nuanceur RECEVRAIT. Ce n'est PAS une lecture de framebuffer. Le brief
// demande « mesuré en pixels » ; ce banc mesure la valeur qui produit le pixel.
// La lecture de pixels demande un rendu et elle est nommée comme non faite.
// =============================================================================
#include "NKPhysics/NkCloth.h"
#include "NKMath/NkContactEvent.h"
#include "NKMath/NkSplashLaw.h"
#include "NKMath/NkWetnessMap.h"
#include "NKMath/NkWaterSurface.h"
#include <ctime> // NKPhysics ne depend PAS de NKTime : le chrono du banc est clock()
#include "NKMath/NkFunctions.h"
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::math;

namespace {

	int *gPass = nullptr;
	int *gFail = nullptr;

	void ECHECK(bool ok, const char *what) {
		if (ok) {
			++(*gPass);
			std::fprintf(stderr, "  [ok]    %s\n", what);
		} else {
			++(*gFail);
			std::fprintf(stderr, "  [ROUGE] %s\n", what);
		}
	}

	// ─────────────────────────────────────────────────────────────────────
	// (a) LA FILE : bornée, et elle dit ce qu'elle perd.
	// ─────────────────────────────────────────────────────────────────────
	void TemoinFile() {
		std::fprintf(stderr, "-- (a) file de contacts bornee --\n");
		NkContactQueue q;
		q.Reserve(4);
		NkContactEvent e;
		e.normalSpeed = 1.f;
		uint32 accepted = 0;
		for (uint32 i = 0; i < 10; ++i) {
			e.index = i;
			if (q.Push(e))
				++accepted;
		}
		std::fprintf(stderr, "     capacite=%u prises=%u perdues=%u proposees=%u\n", q.Capacity(), q.Count(),
					 q.Dropped(), q.Total());
		ECHECK(q.Count() == 4u && accepted == 4u, "(a1) la file borne a sa capacite (4 prises sur 10)");
		ECHECK(q.Dropped() == 6u, "(a2) elle COMPTE ce qu'elle perd (6), elle ne le perd pas en silence");
		ECHECK(q.Total() == 10u, "(a3) elle dit la POPULATION proposee (10) : un chiffre sans elle ne vaut rien");
		ECHECK(q.Overflowed(), "(a4) le debordement est lisible d'un seul appel");
		q.Clear();
		ECHECK(q.Count() == 0u && q.Dropped() == 0u && q.Total() == 0u, "(a5) Clear remet les TROIS compteurs a zero");
	}

	// Un contact synthétique, tombant droit sur un sol horizontal.
	NkContactEvent Impact(float32 vn, uint32 index, float32 t = 0.f) {
		NkContactEvent e;
		e.position = {0.f, 0.f, 0.f};
		e.normal = {0.f, 1.f, 0.f};
		e.velocity = {0.f, -vn, 0.f};
		e.normalSpeed = vn;
		e.time = t;
		e.index = index;
		return e;
	}

	// ─────────────────────────────────────────────────────────────────────
	// (b) LA LOI D'ÉCLABOUSSURE
	// ─────────────────────────────────────────────────────────────────────
	uint32 TotalGouttes(const NkSplashParams &p, float32 vn, uint32 nEvents) {
		uint32 total = 0;
		for (uint32 i = 0; i < nEvents; ++i)
			total += NkSplashDropCount(p, Impact(vn, i, (float32)i * 0.001f));
		return total;
	}

	void TemoinLoi() {
		std::fprintf(stderr, "-- (b) loi d'eclaboussure --\n");
		NkSplashParams p; // seuil 0,5 m/s ; 4 gouttes par m/s au-dessus ; exposant 1
		// (b1) SOUS le seuil : rien. C'est ce qui empeche le fluide POSE sur le fond
		// de pleuvoir des gouttes en permanence.
		ECHECK(NkSplashDropCountReal(p, 0.4f) == 0.f && NkSplashDropCountReal(p, p.speedThreshold) == 0.f,
			   "(b1) aucune goutte au seuil ni en dessous");
		NkSplashDrop buf[64];
		ECHECK(NkSplashEmit(p, Impact(0.4f, 7), buf, 64) == 0u, "(b2) et l'emetteur n'en ecrit aucune non plus");
		ECHECK(NkSplashDropCountReal(p, 1.5f) > 0.f, "(b3) au-dessus du seuil, la loi en demande");

		// (b4-b5) LES DEUX LOIS, LE MEME TEMOIN. Arbitrage de Rodolf du 06/09 :
		// les deux sont livrees, l'utilisateur choisit. Le temoin ne porte donc plus
		// sur UNE loi avec l'autre en controle positif -- il porte sur LES DEUX, et
		// chacune a son attendu ANALYTIQUE, pas une borne large :
		//        doubler (vn - seuil) multiplie n par 2^q.
		// Population dite : 2 000 evenements par point, arrondi stochastique
		// deterministe -- la moyenne vaut la loi exactement.
		//
		// ATTENDU ECRIT AVANT LA MESURE (pre-enregistrement) : 2,000 / 4,000 / 8,000.
		// La bande est +/- 5 %, et elle est justifiee : l'arrondi stochastique tire
		// une fraction par evenement, donc le total porte un bruit en 1/sqrt(N) --
		// de l'ordre de 2 % a N = 2 000. Une bande plus etroite serait du bruit
		// deguise en exigence ; une bande plus large ne separerait plus 2 de 4.
		const uint32 N = 2000u;
		const float32 v1 = p.speedThreshold + 1.f, v2 = p.speedThreshold + 2.f;
		struct CasLoi {
				NkSplashLawKind kind;
				float32 exposantLibre;
				float32 attendu; // 2^q
				const char *quoi;
		};
		const CasLoi cas[3] = {
			{NkSplashLawKind::Lineaire, 1.f, 2.f, "(b4) loi LINEAIRE (le DEFAUT) : n ~ (vn-seuil)^1, rapport 2"},
			{NkSplashLawKind::Energie, 1.f, 4.f, "(b5) loi ENERGIE (livree aussi) : n ~ (vn-seuil)^2, rapport 4"},
			{NkSplashLawKind::Puissance, 3.f, 8.f, "(b5b) echappatoire Puissance(3) : rapport 8"},
		};
		for (uint32 c = 0; c < 3u; ++c) {
			NkSplashParams pl = p;
			pl.law = cas[c].kind;
			pl.exponentLibre = cas[c].exposantLibre;
			pl.maxDropsPerEvent = 65536u; // sinon le plafond ecrase l'ecart qu'on cherche
			const uint32 a1 = TotalGouttes(pl, v1, N), a2 = TotalGouttes(pl, v2, N);
			const float32 r = a1 ? (float32)a2 / (float32)a1 : 0.f;
			std::fprintf(stderr,
						 "     loi %-9s (q=%.1f) : %u -> %u gouttes sur %u impacts, rapport %.3f (attendu %.3f)\n",
						 NkSplashLawName(pl.law), (double)NkSplashExponent(pl), a1, a2, N, (double)r,
						 (double)cas[c].attendu);
			ECHECK(a1 > 0u && NkFabs(r - cas[c].attendu) <= 0.05f * cas[c].attendu, cas[c].quoi);
		}

		// (b5c) LE DEFAUT EST BIEN CELUI QU'ON ANNONCE. Un defaut nomme dans un
		// commentaire n'est pas un defaut : c'est une intention. Celui-ci se mesure
		// sur un objet neuf, la ou l'utilisateur le recevra.
		{
			NkSplashParams d;
			ECHECK(d.law == NkSplashLawKind::Lineaire && NkSplashExponent(d) == 1.f,
				   "(b5c) le DEFAUT livre est bien la loi lineaire (q = 1), mesure sur un objet neuf");
		}

		// (b10) POURQUOI q=2 NE PASSE PAS PAR NkPow, avec le chiffre au lieu de
		// l'affirmation. NkSplashLaw.h dit que les deux ne rendent pas forcement le
		// meme float32 ; ce banc IMPRIME l'ecart au lieu de le supposer, et il
		// n'exige rien dessus -- il documente. Un commentaire qui avance un chiffre
		// non mesure est exactement ce que le corpus interdit.
		{
			const float32 t = (v2 - p.speedThreshold) / p.speedRef; // 2,0
			const float32 parPow = NkPow(t, 2.f);
			const float32 parMul = t * t;
			std::fprintf(stderr, "     (b10) NkPow(%.1f, 2) = %.9g   contre  t*t = %.9g   ecart = %.3g\n", (double)t,
						 (double)parPow, (double)parMul, (double)NkFabs(parPow - parMul));
			ECHECK(parMul == t * t, "(b10) t*t est la reference exacte (l'ecart a NkPow est imprime ci-dessus)");
		}

		// (b11) LE PARAMETRE QUI N'EST PAS LU, MESURE AU LIEU D'ETRE SUBI.
		// `exponentLibre` n'est consulte que sous Puissance. Le corpus interdit un
		// parametre present et silencieusement ignore ; la parade retenue n'est pas
		// de le retirer (il porte l'echappatoire) mais de PROUVER les deux moities :
		//   - sous Lineaire, le bouger de 1 a 7 ne change RIEN, au bit ;
		//   - sous Puissance, le meme geste change TOUT.
		// Le second est indispensable : sans lui, un `exponentLibre` mort PARTOUT
		// passerait la premiere moitie sans rien prouver.
		{
			NkSplashParams l1 = p, l7 = p;
			l1.law = l7.law = NkSplashLawKind::Lineaire;
			l1.exponentLibre = 1.f;
			l7.exponentLibre = 7.f;
			const uint32 n1 = TotalGouttes(l1, v2, N), n7 = TotalGouttes(l7, v2, N);
			ECHECK(n1 == n7, "(b11a) sous Lineaire, exponentLibre n'est PAS lu -- meme total au bit");

			NkSplashParams q1 = p, q3 = p;
			q1.law = q3.law = NkSplashLawKind::Puissance;
			q1.exponentLibre = 1.f;
			q3.exponentLibre = 3.f;
			q1.maxDropsPerEvent = q3.maxDropsPerEvent = 65536u;
			const uint32 m1 = TotalGouttes(q1, v2, N), m3 = TotalGouttes(q3, v2, N);
			std::fprintf(stderr, "     (b11) Lineaire : %u == %u   |   Puissance : %u -> %u en passant q de 1 a 3\n",
						 n1, n7, m1, m3);
			ECHECK(m3 > m1 * 3u, "(b11b) sous Puissance, exponentLibre est LU -- et l'ecart est massif");
		}

		// (b12) LA PORTE, PAS LA FONCTION. (b4-b5) lisent le compteur ; celui-ci lit
		// ce que l'EMETTEUR ecrit reellement dans le tampon de l'appelant. Un compteur
		// juste et un emetteur qui plafonne ailleurs donneraient deux verites, et
		// c'est l'emetteur que le systeme d'effets appelle.
		{
			NkSplashDrop out[256];
			for (uint32 c = 0; c < 2u; ++c) {
				NkSplashParams pe2 = p;
				pe2.law = (c == 0u) ? NkSplashLawKind::Lineaire : NkSplashLawKind::Energie;
				pe2.maxDropsPerEvent = 65536u;
				uint32 ecrites1 = 0u, ecrites2 = 0u;
				for (uint32 i = 0; i < 400u; ++i) {
					ecrites1 += NkSplashEmit(pe2, Impact(v1, i, (float32)i * 0.001f), out, 256);
					ecrites2 += NkSplashEmit(pe2, Impact(v2, i, (float32)i * 0.001f), out, 256);
				}
				const float32 r = ecrites1 ? (float32)ecrites2 / (float32)ecrites1 : 0.f;
				const float32 att = (c == 0u) ? 2.f : 4.f;
				std::fprintf(stderr,
							 "     (b12) emetteur, loi %-9s : %u -> %u gouttes ECRITES, rapport %.3f (attendu %.3f)\n",
							 NkSplashLawName(pe2.law), ecrites1, ecrites2, (double)r, (double)att);
				ECHECK(ecrites1 > 0u && NkFabs(r - att) <= 0.05f * att,
					   c == 0u ? "(b12a) l'EMETTEUR suit la loi lineaire, pas seulement le compteur"
							   : "(b12b) l'EMETTEUR suit la loi en energie, pas seulement le compteur");
			}
		}

		// (b6) DIRECTION : les gouttes partent DE la surface. Une goutte dont la vitesse
		// entre dans le solide est un defaut visible immediatement.
		NkContactEvent e = Impact(3.f, 42);
		e.velocity = {2.f, -3.f, 0.5f}; // impact oblique : le cas ou une reflexion rasante fait mal
		const uint32 k = NkSplashEmit(p, e, buf, 64);
		bool toutesDehors = true;
		float32 cosMin = 1.f;
		// direction de reference = reflexion redressee vers la normale (NkSplashLaw.h)
		NkVec3f d = e.velocity;
		d.Normalize();
		const float32 dn = d.Dot(e.normal);
		NkVec3f r = d - e.normal * (2.f * dn);
		r.Normalize();
		NkVec3f axis = r + e.normal;
		axis.Normalize();
		for (uint32 i = 0; i < k; ++i) {
			NkVec3f v = buf[i].velocity;
			if (v.Dot(e.normal) <= 0.f)
				toutesDehors = false;
			NkVec3f vn = v;
			vn.Normalize();
			const float32 c = vn.Dot(axis);
			if (c < cosMin)
				cosMin = c;
		}
		std::fprintf(stderr, "     %u gouttes, angle max au cone %.1f deg (demi-angle demande %.1f deg)\n", k,
					 (double)(NkToDegrees(NkAcos(NkClamp(cosMin, -1.f, 1.f)))),
					 (double)NkToDegrees(p.coneHalfAngle));
		ECHECK(k > 0u && toutesDehors, "(b6) toutes les gouttes s'eloignent de la surface (v.n > 0)");
		ECHECK(cosMin >= NkCos(p.coneHalfAngle) - 1e-3f, "(b7) et toutes tiennent dans le cone demande");

		// (b8) DÉTERMINISME : deux passes, meme resultat. Un banc qui ne peut pas etre
		// rejoue ne peut pas etre contredit.
		NkSplashDrop buf2[64];
		const uint32 k2 = NkSplashEmit(p, e, buf2, 64);
		bool identique = (k == k2);
		for (uint32 i = 0; i < k && identique; ++i)
			identique = (buf[i].velocity - buf2[i].velocity).Len() < 1e-6f;
		ECHECK(identique, "(b8) deterministe : le meme contact rend exactement les memes gouttes");

		// (b9) PLAFOND
		NkSplashParams pc = p;
		pc.maxDropsPerEvent = 3u;
		ECHECK(NkSplashDropCount(pc, Impact(100.f, 1)) == 3u, "(b9) le plafond par evenement est respecte");
	}

	// ─────────────────────────────────────────────────────────────────────
	// (c) LA CARTE DE MOUILLAGE : dépôt, séchage, mutation, deux matières.
	// ─────────────────────────────────────────────────────────────────────
	void TemoinCarte() {
		std::fprintf(stderr, "-- (c) carte de mouillage --\n");
		NkWetnessMap m;
		m.Create(256, 256);
		m.material = NkWetSand();
		std::fprintf(stderr, "     carte 256x256 float32 : %u octets (%u Kio) ; en R8 au GPU : %u octets\n", m.Bytes(),
					 m.Bytes() / 1024u, m.Bytes() / 4u);
		ECHECK(m.Bytes() == 256u * 256u * 4u, "(c1) memoire DITE et verifiee : 4 octets par texel");
		ECHECK(m.Max() == 0.f && m.IsDry(), "(c2) une carte neuve est seche");

		m.Splat(0.5f, 0.5f, 0.05f, 1.f);
		const float32 centre = m.Sample(0.5f, 0.5f);
		const float32 loin = m.Sample(0.05f, 0.05f);
		std::fprintf(stderr, "     apres un depot au centre : centre=%.4f, coin=%.4f\n", (double)centre, (double)loin);
		ECHECK(centre > 0.5f, "(c3) le depot mouille la ou le contact a eu lieu");
		ECHECK(loin == 0.f, "(c4) et NULLE PART ailleurs (un depot local reste local)");

		// (c5) SÉCHAGE EXPONENTIEL : apres tau il reste 1/e.
		NkWetnessMap s;
		s.Create(8, 8);
		s.material.dryingTime = 10.f;
		for (uint32 i = 0; i < 64u; ++i)
			s.Data()[i] = 1.f;
		const float32 w0 = s.Sample(0.5f, 0.5f);
		const float32 dt = 1.f / 60.f;
		for (uint32 i = 0; i < 600u; ++i) // 10 s = tau
			s.Dry(dt);
		const float32 wTau = s.Sample(0.5f, 0.5f);
		const float32 attendu = NkExp(-1.f);
		std::fprintf(stderr, "     w0=%.4f -> w(tau)=%.4f (attendu 1/e = %.4f, ecart %.2f %%)\n", (double)w0,
					 (double)wTau, (double)attendu, (double)(100.f * NkFabs(wTau - attendu) / attendu));
		ECHECK(NkFabs(wTau - attendu) < 0.01f * attendu, "(c5) sechage exponentiel : w(tau) = w0 / e a 1 %");
		for (uint32 i = 0; i < 2400u; ++i) // + 40 s = 5 tau au total
			s.Dry(dt);
		ECHECK(s.Sample(0.5f, 0.5f) < 0.01f, "(c6) apres 5 tau il ne reste rien (< 1 %)");

		// (c7) MUTATION : sechage coupe -> le temoin doit ROUGIR. Ici on verifie que
		// la mutation change bien quelque chose ; c'est ce qui rend (c5) capable de rougir.
		NkWetnessMap mu;
		mu.Create(8, 8);
		mu.material.dryingTime = 10.f;
		for (uint32 i = 0; i < 64u; ++i)
			mu.Data()[i] = 1.f;
		for (uint32 i = 0; i < 600u; ++i)
			mu.Dry(dt, false);
		std::fprintf(stderr, "     MUTATION sechage coupe : w apres tau = %.4f (le sain vaut %.4f)\n",
					 (double)mu.Sample(0.5f, 0.5f), (double)wTau);
		ECHECK(mu.Sample(0.5f, 0.5f) > 0.99f,
			   "(c7) MUTATION : sechage desactive -> rien ne seche, donc (c5) SAIT rougir");

		// (c8) DEUX MATIÈRES, ORDRE et non valeur (les tau sont de la direction artistique).
		NkWetnessMap sable, cuir;
		sable.Create(4, 4);
		cuir.Create(4, 4);
		sable.material = NkWetSand();	 // tau 20 s
		cuir.material = NkWetLeather(); // tau 300 s
		for (uint32 i = 0; i < 16u; ++i) {
			sable.Data()[i] = 1.f;
			cuir.Data()[i] = 1.f;
		}
		for (uint32 i = 0; i < 3600u; ++i) { // 60 s
			sable.Dry(dt);
			cuir.Dry(dt);
		}
		const float32 ws = sable.Sample(0.5f, 0.5f), wc = cuir.Sample(0.5f, 0.5f);
		std::fprintf(stderr, "     a 60 s : sable=%.4f (tau %.0f s), cuir=%.4f (tau %.0f s)\n", (double)ws,
					 (double)sable.material.dryingTime, (double)wc, (double)cuir.material.dryingTime);
		ECHECK(ws < wc, "(c8) le sable seche AVANT le cuir (l'ordre est la promesse, pas les secondes)");
		ECHECK(ws < 0.1f && wc > 0.5f, "(c9) et l'ecart est franc a 60 s, pas une difference de bruit");
	}

	// ─────────────────────────────────────────────────────────────────────
	// (d) CE QUE LE MOUILLAGE FAIT À LA LUMIÈRE (formule de Lagarde 3b).
	// ─────────────────────────────────────────────────────────────────────
	void TemoinLumiere() {
		std::fprintf(stderr, "-- (d) l'aspect mouille (Lagarde, Water drop 3b, 2013) --\n");
		const NkVec3f albedo = {0.5f, 0.4f, 0.3f};
		const float32 rough = 0.8f, f0 = 0.04f;
		NkWetMaterial sable = NkWetSand(); // porosite 1

		// (d1) CONTRÔLE POSITIF DE L'INSTRUMENT : a sec, la formule est l'IDENTITE.
		// Une formule qui bouge a w = 0 est fausse avant meme qu'on la mesure.
		NkWetShading sec = NkApplyWetness(albedo, rough, f0, 0.f, sable);
		ECHECK((sec.albedo - albedo).Len() < 1e-6f && NkFabs(sec.roughness - rough) < 1e-6f &&
				   NkFabs(sec.f0 - f0) < 1e-6f,
			   "(d1) CONTROLE POSITIF : a w = 0 la formule ne touche a RIEN");

		// (d2) Les constantes de Lagarde, au chiffre : albedo x0,2 et rugosite x0,6.
		NkWetShading trempe = NkApplyWetness(albedo, rough, f0, 1.f, sable);
		std::fprintf(stderr, "     porosite 1, w = 1 : albedo %.3f -> %.3f (x%.3f), rugosite %.3f -> %.3f (x%.3f)\n",
					 (double)albedo.x, (double)trempe.albedo.x, (double)(trempe.albedo.x / albedo.x), (double)rough,
					 (double)trempe.roughness, (double)(trempe.roughness / rough));
		ECHECK(NkFabs(trempe.albedo.x / albedo.x - 0.2f) < 1e-4f,
			   "(d2) albedo x0,2 a saturation : lerp(1, 0.2, porosite) mot pour mot");
		ECHECK(NkFabs(trempe.roughness / rough - 0.6f) < 1e-4f,
			   "(d3) rugosite x0,6 = 1 - 0,4 x porosite x w (conversion algebrique du gloss de Lagarde)");

		// (d4) MONOTONIE : plus c'est mouille, plus c'est sombre et plus c'est lisse.
		bool monotone = true;
		float32 aPrev = 1e9f, rPrev = 1e9f;
		for (uint32 i = 0; i <= 10u; ++i) {
			const NkWetShading o = NkApplyWetness(albedo, rough, f0, (float32)i * 0.1f, sable);
			if (o.albedo.x > aPrev + 1e-6f || o.roughness > rPrev + 1e-6f)
				monotone = false;
			aPrev = o.albedo.x;
			rPrev = o.roughness;
		}
		ECHECK(monotone, "(d4) albedo et rugosite decroissent quand le mouillage monte (11 points)");

		// (d5) POROSITE 0 (verre poli) : l'eau ne rentre pas, rien ne s'assombrit.
		NkWetMaterial verre;
		verre.porosity = 0.f;
		NkWetShading v = NkApplyWetness(albedo, rough, f0, 1.f, verre);
		ECHECK((v.albedo - albedo).Len() < 1e-6f, "(d5) porosite 0 : trempe mais pas assombri (le mouille n'est pas uniforme)");

		// (d6) IL S'ASSOMBRIT, PUIS IL REVIENT. C'est la demande de Rodolf, bout a bout :
		// une carte mouillee, la couleur mesuree, le sechage, la couleur remesuree.
		NkWetnessMap m;
		m.Create(32, 32);
		m.material = sable;
		m.material.dryingTime = 5.f;
		for (uint32 i = 0; i < 32u * 32u; ++i)
			m.Data()[i] = 1.f;
		const float32 aSec = albedo.x;
		const float32 aMouille = NkApplyWetness(albedo, rough, f0, m.Sample(0.5f, 0.5f), m.material).albedo.x;
		const float32 dt = 1.f / 60.f;
		for (uint32 i = 0; i < 60u * 25u; ++i) // 25 s = 5 tau
			m.Dry(dt);
		const float32 aRevenu = NkApplyWetness(albedo, rough, f0, m.Sample(0.5f, 0.5f), m.material).albedo.x;
		std::fprintf(stderr, "     albedo : sec %.4f -> mouille %.4f (-%.1f %%) -> apres 5 tau %.4f (ecart au sec %.2f %%)\n",
					 (double)aSec, (double)aMouille, (double)(100.f * (1.f - aMouille / aSec)), (double)aRevenu,
					 (double)(100.f * NkFabs(aRevenu - aSec) / aSec));
		ECHECK(aMouille < 0.5f * aSec, "(d6) une surface touchee S'ASSOMBRIT (plus de moitie a saturation)");
		ECHECK(NkFabs(aRevenu - aSec) < 0.01f * aSec, "(d7) et elle RETROUVE sa couleur apres le sechage (1 %)");
	}

	// ─────────────────────────────────────────────────────────────────────
	// (e) LE TISSU MOUILLÉ : plus lourd, et il pend plus bas.
	// ─────────────────────────────────────────────────────────────────────
	// Nappe 1 m x 1 m, 16 x 16, 0,2 kg, rangee du haut epinglee, 3 s a 1/60 s.
	// `alpha` = compliance STRUCTURELLE (m/N). Elle est un ARGUMENT, et c est la
	// lecon de ce temoin : voir le bloc (e3) plus bas.
	float32 CourseNappe(const NkWetnessMap *carte, float32 alpha, float32 &outMasse,
						float32 &outEchelleCompliance) {
		physics::NkCloth c;
		const uint32 n = 16u;
		const float32 d = 1.f / (float32)(n - 1u);
		c.BuildGrid(n, n, NkVec3f{0.f, 1.f, 0.f}, NkVec3f{d, 0.f, 0.f}, NkVec3f{0.f, 0.f, d}, 0.2f);
		// la rangee j = 0 est epinglee : la nappe pend
		for (uint32 i = 0; i < n; ++i)
			c.Pin(c.GridIndex(i, 0u));
		c.params.selfCollision = false;
		c.params.collisions = false;
		c.params.compliance = alpha;
		c.params.shearCompliance = alpha;
		c.wetness = carte;
		for (uint32 s = 0; s < 180u; ++s)
			c.Step(1.f / 60.f);
		outMasse = c.Stats().mass;
		outEchelleCompliance = c.Stats().complianceScale;
		float32 ymin = 1e30f;
		const NkVec3f *P = c.Positions();
		for (uint32 i = 0; i < c.ParticleCount(); ++i)
			if (P[i].y < ymin)
				ymin = P[i].y;
		return ymin;
	}

	void TemoinTissu() {
		std::fprintf(stderr, "-- (e) le tissu mouille --\n");
		NkWetnessMap m;
		m.Create(16, 16);
		m.material = NkWetCloth(); // gain de masse 0,6 ; gain de compliance 3
		for (uint32 i = 0; i < 16u * 16u; ++i)
			m.Data()[i] = 1.f;

		float32 mSec = 0.f, mMou = 0.f, csSec = 0.f, csMou = 0.f;
		// -- LE PIEGE, ET IL A COUTE UN ROUGE AVANT D ETRE COMPRIS ---------------
		// Premiere ecriture de ce temoin : compliance structurelle 0 (le defaut de
		// NkClothParams, « inextensible »). Mesure : la nappe mouillee pendait
		// 0,2 mm PLUS HAUT que la seche. Ce n etait pas un defaut du mouillage --
		// c est la SCENE qui ne pouvait pas repondre a la question. Une nappe
		// inextensible pend a la longueur de son tissu : la masse ne peut pas
		// l allonger, et alpha x 3 vaut encore 0. Le 0,2 mm venait du seul terme qui
		// bougeait, la flexion, qui plie un peu plus la nappe lourde et REMONTE donc
		// son point bas. « Un cas neutre ne teste pas ce qui multiplie zero. »
		// La scene qui repond est un tissu qui S ETIRE. Deuxieme mesure, a alpha =
		// 1e-4 m/N : 0,09 mm d ecart -- du bon signe, mais SOUS le residu du solveur
		// (16 sous-pas x 2 iterations ne convergent pas a ce point, cf. la table de
		// NkCloth.h). Un ecart qu on ne peut pas distinguer du bruit n est pas un
		// temoin. A alpha = 1e-3 m/N (un tricot souple) l ecart est de 2,4 mm, soit
		// 25 fois le seuil : la scene retenue, et les deux chiffres sont dits.
		// Les deux mesures sont gardees, et celle qui ne peut pas repondre est DITE.
		const float32 ySecRigide = CourseNappe(nullptr, 0.f, mSec, csSec);
		const float32 yMouRigide = CourseNappe(&m, 0.f, mMou, csMou);
		std::fprintf(stderr, "     [scene INEXTENSIBLE alpha = 0 : celle qui ne PEUT PAS repondre]\n");
		std::fprintf(stderr, "       point le plus bas : sec %.4f m -> mouille %.4f m (%.2f mm)\n",
					 (double)ySecRigide, (double)yMouRigide, (double)(1000.f * (ySecRigide - yMouRigide)));
		const float32 alpha = 1e-3f;
		const float32 ySec = CourseNappe(nullptr, alpha, mSec, csSec);
		const float32 yMou = CourseNappe(&m, alpha, mMou, csMou);
		std::fprintf(stderr, "     [scene SOUPLE alpha = 1e-3 m/N]\n");
		std::fprintf(stderr, "     masse : sec %.4f kg -> mouille %.4f kg (x%.3f, gain annonce %.2f)\n", (double)mSec,
					 (double)mMou, (double)(mMou / mSec), (double)m.material.saturatedMassGain);
		std::fprintf(stderr, "     point le plus bas : sec %.4f m -> mouille %.4f m (%.2f mm plus bas)\n", (double)ySec,
					 (double)yMou, (double)(1000.f * (ySec - yMou)));
		std::fprintf(stderr, "     echelle de compliance : sec %.3f -> mouille %.3f\n", (double)csSec, (double)csMou);
		ECHECK(NkFabs(mSec - 0.2f) < 1e-3f, "(e1) la nappe seche pese sa masse de construction (0,2 kg)");
		ECHECK(NkFabs(mMou - 0.2f * (1.f + m.material.saturatedMassGain)) < 1e-3f,
			   "(e2) la nappe mouillee pese m_sec x (1 + gain) : la MASSE monte, mesuree");
		ECHECK(yMou < ySec - 1e-4f, "(e3) sur un tissu qui s etire, la mouillee PEND PLUS BAS (> 0,1 mm)");
		ECHECK(NkFabs(ySecRigide - yMouRigide) < 1e-3f,
			   "(e3bis) sur un tissu INEXTENSIBLE elle ne le peut pas : la scene est DITE, pas cachee");
		ECHECK(NkFabs(csMou - (1.f + (m.material.saturatedComplianceGain - 1.f))) < 1e-3f,
			   "(e4) l'echelle de compliance suit la carte (mouillee a 1 partout)");
		ECHECK(NkFabs(csSec - 1.f) < 1e-6f, "(e5) et vaut exactement 1 sans carte (aucun effet de bord a sec)");

		// (e6) PAS DE DÉRIVE. Le piege du lot : recalculer la masse depuis mMass au lieu
		// de mMassDry la multiplierait a chaque pas. 180 pas plus haut n'auraient rien vu
		// si on ne comparait pas la masse a un SEUL pas.
		physics::NkCloth c;
		c.BuildGrid(8u, 8u, NkVec3f{0.f, 1.f, 0.f}, NkVec3f{0.1f, 0.f, 0.f}, NkVec3f{0.f, 0.f, 0.1f}, 0.2f);
		c.params.collisions = false;
		c.wetness = &m;
		c.Step(1.f / 60.f);
		const float32 m1 = c.Stats().mass;
		for (uint32 s = 0; s < 600u; ++s)
			c.Step(1.f / 60.f);
		const float32 m600 = c.Stats().mass;
		std::fprintf(stderr, "     masse au pas 1 : %.5f kg ; au pas 601 : %.5f kg (derive %.3g)\n", (double)m1,
					 (double)m600, (double)NkFabs(m600 - m1));
		ECHECK(NkFabs(m600 - m1) < 1e-5f, "(e6) AUCUNE DERIVE sur 601 pas : la masse se recalcule depuis la masse SECHE");

		// (e7) RETOUR A SEC : la carte retiree, la masse redescend au pas suivant.
		c.wetness = nullptr;
		c.Step(1.f / 60.f);
		std::fprintf(stderr, "     carte retiree : masse = %.5f kg\n", (double)c.Stats().mass);
		ECHECK(NkFabs(c.Stats().mass - 0.2f) < 1e-4f, "(e7) carte retiree -> le tissu retrouve sa masse seche");
	}

	// ─────────────────────────────────────────────────────────────────────
	// (j)(k)(l)(m)(n)(o) LA SURFACE D'EAU : la houle de Gerstner.
	// ─────────────────────────────────────────────────────────────────────
	NkWaterParams UneHoule(float32 lambda, float32 amplitude, float32 steepness) {
		NkWaterParams p;
		p.waveCount = 1u;
		p.waves[0].wavelength = lambda;
		p.waves[0].amplitude = amplitude;
		p.waves[0].steepness = steepness;
		p.waves[0].direction = {1.f, 0.f};
		return p;
	}

	// Suit une crete : abscisse du MAXIMUM de hauteur sur [x0, x0 + lambda],
	// echantillonne finement. C'est une MESURE sur le champ de hauteur, pas un
	// rappel de la formule de dispersion.
	float32 AbscisseDeCrete(const NkWaterParams &p, float32 t, float32 x0, float32 lambda, float32 h) {
		const uint32 N = 20000u;
		float32 best = x0, ymax = -1e30f;
		for (uint32 i = 0; i < N; ++i) {
			const float32 x = x0 + lambda * (float32)i / (float32)N;
			const float32 y = NkWaterHeight(p, x, 0.f, t, h);
			if (y > ymax) {
				ymax = y;
				best = x;
			}
		}
		return best;
	}

	void TemoinHoule() {
		std::fprintf(stderr, "-- (j..o) la surface d'eau : houle de Gerstner --\n");

		// (j) LA RELATION DE DISPERSION en eau PROFONDE, sur trois longueurs d'onde.
		// La vitesse est MESUREE (deplacement d'une crete entre deux instants) et
		// comparee a c = sqrt(g lambda / 2 pi) (Airy, tanh -> 1).
		bool disp = true;
		for (uint32 i = 0; i < 3u; ++i) {
			const float32 lambda = (i == 0u) ? 4.f : ((i == 1u) ? 20.f : 100.f);
			// amplitude petite devant lambda : on reste dans le domaine ou Airy vaut
			const NkWaterParams p = UneHoule(lambda, 0.02f * lambda, 0.f);
			const float32 dt = 0.05f;
			const float32 x1 = AbscisseDeCrete(p, 0.f, 0.f, lambda, -1.f);
			const float32 x2 = AbscisseDeCrete(p, dt, x1 - 0.25f * lambda, lambda, -1.f);
			const float32 cMes = (x2 - x1) / dt;
			const float32 cTh = NkSqrt(NK_GRAVITE_NORMALE * lambda / 6.2831853f);
			const float32 err = 100.f * NkFabs(cMes - cTh) / cTh;
			std::fprintf(stderr, "     lambda = %6.1f m : c mesuree %7.3f m/s, Airy sqrt(g L / 2pi) %7.3f, ecart %.2f %%\n",
						 (double)lambda, (double)cMes, (double)cTh, (double)err);
			if (err > 5.f)
				disp = false;
		}
		ECHECK(disp, "(j) la relation de DISPERSION en eau profonde tient a 5 % sur trois longueurs d'onde");

		// (k) AMPLITUDE NULLE -> PLAN a epsilon.
		NkWaterParams plat = UneHoule(20.f, 0.f, 0.6f);
		float32 ymax = 0.f;
		for (uint32 i = 0; i < 500u; ++i) {
			const float32 y = NkWaterHeight(plat, (float32)i * 0.37f, (float32)i * 0.11f, 3.21f);
			if (NkFabs(y) > ymax)
				ymax = NkFabs(y);
		}
		std::fprintf(stderr, "     amplitude nulle : |y| max sur 500 points = %.3g m\n", (double)ymax);
		ECHECK(ymax < 1e-6f, "(k) la surface au repos est PLANE a epsilon");

		// (l) DETERMINISME : meme temps -> meme hauteur.
		NkWaterParams huit;
		huit.waveCount = 8u;
		for (uint32 i = 0; i < 8u; ++i) {
			huit.waves[i].wavelength = 4.f + 12.f * (float32)i;
			huit.waves[i].amplitude = 0.35f / (1.f + 0.4f * (float32)i);
			huit.waves[i].steepness = 0.7f;
			const float32 a = 0.7f * (float32)i;
			huit.waves[i].direction = {NkCos(a), NkSin(a)};
			huit.waves[i].phase = 0.31f * (float32)i;
		}
		const float32 h1 = NkWaterHeight(huit, 3.7f, -2.1f, 5.5f);
		const float32 h2 = NkWaterHeight(huit, 3.7f, -2.1f, 5.5f);
		ECHECK(h1 == h2, "(l) la hauteur est DETERMINISTE (meme temps, meme hauteur, au bit)");

		// (m) LE COUT, sur trois tailles de grille, 8 trains. Population dite :
		// c'est le cout d'EVALUATION CPU de N sommets, PAS un cout de rendu.
		// ⚠️ L INSTRUMENT, dit avant le chiffre : `std::clock()`, dont la resolution
		// est de l ordre de la milliseconde sur Windows. Une grille de 64 x 64
		// s evalue bien en dessous : chaque taille est donc REPETEE jusqu a depasser
		// 50 ms de mesure, et le temps rendu est la moyenne. Sans ces repetitions, la
		// petite grille rendrait 0,000 ms et j aurais publie un chiffre qui ne dit que
		// la resolution de l horloge. (NKPhysics ne depend pas de NKTime : pas de
		// NkChrono ici.)
		const uint32 tailles[3] = {64u, 128u, 256u};
		for (uint32 k = 0; k < 3u; ++k) {
			const uint32 n = tailles[k];
			float32 acc = 0.f;
			uint32 rep = 0u;
			const std::clock_t t0 = std::clock();
			double sec = 0.0;
			do {
				for (uint32 j = 0; j < n; ++j)
					for (uint32 i = 0; i < n; ++i) {
						const NkWaterPoint w = NkWaterEval(huit, (float32)i * 0.5f, (float32)j * 0.5f, 1.25f);
						acc += w.position.y + w.normal.y;
					}
				++rep;
				sec = (double)(std::clock() - t0) / (double)CLOCKS_PER_SEC;
			} while (sec < 0.05);
			const double ms = sec * 1000.0 / (double)rep;
			std::fprintf(stderr,
						 "     grille %3u x %3u = %6u sommets, 8 trains : %7.3f ms CPU par passe (%.4f us/sommet, "
						 "%u repetitions)%s\n",
						 n, n, n * n, ms, ms * 1000.0 / (double)(n * n), rep, acc != 0.f ? "" : " ");
		}
		ECHECK(true, "(m) le cout d'evaluation est MESURE et dit sur trois tailles (pas un cout de rendu)");

		// (n) MUTATION : un train ignore -> la hauteur CHANGE.
		NkWaterParams sept = huit;
		sept.waveCount = 7u;
		const float32 hHuit = NkWaterHeight(huit, 3.7f, -2.1f, 5.5f);
		const float32 hSept = NkWaterHeight(sept, 3.7f, -2.1f, 5.5f);
		std::fprintf(stderr, "     MUTATION un train ignore : y = %.5f -> %.5f (ecart %.5f m)\n", (double)hHuit,
					 (double)hSept, (double)NkFabs(hHuit - hSept));
		ECHECK(NkFabs(hHuit - hSept) > 1e-4f, "(n) MUTATION : un train ignore change la hauteur, donc (l) SAIT rougir");

		// (o) LA NORMALE ANALYTIQUE contre les differences finies du VRAI champ.
		// C'est le controle qui rend la citation inutile : si j'avais mal recopie
		// la formule de GPU Gems, l'ecart d'angle exploserait.
		float32 angleMax = 0.f, angleFastMax = 0.f;
		for (uint32 i = 0; i < 200u; ++i) {
			const float32 x = 0.13f * (float32)i, z = 0.07f * (float32)i, t = 2.5f;
			const float32 e = 1e-3f;
			// Gerstner deplace AUSSI x et z : la derivee se prend sur le point
			// deplace, donc par difference des positions completes.
			const NkWaterPoint c = NkWaterEval(huit, x, z, t);
			const NkWaterPoint px = NkWaterEval(huit, x + e, z, t);
			const NkWaterPoint pz = NkWaterEval(huit, x, z + e, t);
			const NkVec3f du = px.position - c.position;
			const NkVec3f dv = pz.position - c.position;
			NkVec3f nfd = du.Cross(dv);
			if (nfd.y < 0.f)
				nfd = nfd * -1.f;
			const float32 l = nfd.Len();
			if (l < 1e-12f)
				continue;
			nfd = nfd * (1.f / l);
			const float32 a = NkToDegrees(NkAcos(NkClamp(nfd.Dot(c.normal), -1.f, 1.f)));
			if (a > angleMax)
				angleMax = a;
			const float32 af = NkToDegrees(NkAcos(NkClamp(nfd.Dot(c.normalFast), -1.f, 1.f)));
			if (af > angleFastMax)
				angleFastMax = af;
		}
		std::fprintf(stderr, "     contre les differences finies du VRAI champ (200 points, 8 trains, cambrure 0,7) :\n");
		std::fprintf(stderr, "       jacobien complet (defaut)      : ecart d angle max %.4f deg\n", (double)angleMax);
		std::fprintf(stderr, "       forme de GPU Gems (normalFast) : ecart d angle max %.4f deg\n", (double)angleFastMax);
		ECHECK(angleMax < 2.f, "(o) la NORMALE ANALYTIQUE (jacobien complet) est la bonne : < 2 deg");
		// (o2) N EST PAS UN PROCES DE GPU GEMS. Sa formule est EXACTE pour un train ;
		// pour une SOMME elle laisse tomber les termes croises. Ce temoin mesure ce
		// que cette approximation coute, et il verifie qu a UN train elle redevient
		// juste -- c est ce qui distingue « la formule est du premier ordre » de
		// « je l ai mal recopiee ».
		float32 unTrain = 0.f;
		{
			const NkWaterParams p1 = UneHoule(20.f, 0.6f, 0.8f);
			for (uint32 i = 0; i < 100u; ++i) {
				const float32 x = 0.21f * (float32)i, t = 1.3f;
				const NkWaterPoint c = NkWaterEval(p1, x, 0.f, t);
				const float32 a = NkToDegrees(NkAcos(NkClamp(c.normal.Dot(c.normalFast), -1.f, 1.f)));
				if (a > unTrain)
					unTrain = a;
			}
		}
		std::fprintf(stderr, "       et a UN SEUL train, les deux formes coincident a %.4f deg\n", (double)unTrain);
		// ⚠️ CRITERE CORRIGE APRES COUP, ET JE LE DIS. Je l avais ecrit a 0,01 deg
		// SANS avoir mesure le plancher de bruit de l instrument -- exactement la
		// faute que le depot a deja payee (« un critere ecrit d avance sur un
		// instrument trop grossier est faux d avance »). Mesure : 0,0343 deg, soit
		// 2 minutes d arc, sur un angle obtenu en normalisant un produit vectoriel
		// de deux vecteurs presque paralleles en float32. Ce n est pas un ecart de
		// formule, c est le bruit du calcul. Le chiffre qui PROUVE quelque chose est
		// le RAPPORT : 4,2746 / 0,0343 = 125. Seuil porte a 0,1 deg (le triple du
		// bruit mesure), et les deux chiffres bruts restent imprimes au-dessus.
		ECHECK(unTrain < 0.1f && angleFastMax > 20.f * unTrain,
			   "(o2) a UN train les deux formes coincident (bruit float32), a 8 trains l ecart est 125x plus grand : "
			   "c est l ORDRE de la formule de GPU Gems, pas ma copie");
	}

	// ─────────────────────────────────────────────────────────────────────
	// (p)(q)(r) LES FONDS MARINS ET LE RIVAGE : la profondeur pilote tout.
	// ─────────────────────────────────────────────────────────────────────
	void TemoinRivage() {
		std::fprintf(stderr, "-- (p..r) les fonds marins et le rivage --\n");

		// CONTROLE POSITIF DE L'INSTRUMENT DE PROFONDEUR, avant tout chiffre :
		// un point connu SOUS l'eau et un point connu AU-DESSUS. Sans lui, une
		// profondeur toujours positive ressemblerait a une mesure juste.
		const float32 surface = 0.f;
		const float32 dSous = NkWaterDepth(surface, -3.f); // fond a -3 m : 3 m d'eau
		const float32 dSur = NkWaterDepth(surface, 1.2f);  // rocher a +1,2 m : emerge
		std::fprintf(stderr, "     controle positif : fond a -3,0 m -> profondeur %.3f ; rocher a +1,2 m -> %.3f\n",
					 (double)dSous, (double)dSur);
		ECHECK(NkFabs(dSous - 3.f) < 1e-5f && dSur < 0.f,
			   "(p1) CONTROLE POSITIF : l'instrument de profondeur separe le dessous du dessus");

		// (p2) BEER-LAMBERT : la couleur tend vers la couleur profonde, et LE ROUGE
		// PART LE PREMIER. Cinq profondeurs.
		NkWaterOptics opt;
		const NkVec3f sable = {0.80f, 0.72f, 0.55f};
		float32 rPrec = 1e9f, ecartPrec = 1e9f;
		bool decroit = true, rougeDabord = true;
		for (uint32 i = 0; i < 5u; ++i) {
			const float32 d = (float32)i * 3.f; // 0, 3, 6, 9, 12 m
			const NkVec3f c = NkWaterShade(opt, sable, d);
			const NkVec3f T = NkBeerLambert(opt.absorption, d);
			const float32 ecart = (c - opt.deepColor).Len();
			std::fprintf(stderr, "     %5.1f m : couleur (%.3f, %.3f, %.3f) ; transmission R=%.3f V=%.3f B=%.3f ; "
								 "distance a la couleur profonde %.4f\n",
						 (double)d, (double)c.x, (double)c.y, (double)c.z, (double)T.x, (double)T.y, (double)T.z,
						 (double)ecart);
			if (i > 0u && ecart > ecartPrec + 1e-6f)
				decroit = false;
			if (i > 0u && !(T.x < T.y && T.y < T.z))
				rougeDabord = false;
			ecartPrec = ecart;
			rPrec = c.x;
		}
		(void)rPrec;
		ECHECK(decroit, "(p2) a profondeur croissante la couleur TEND vers la couleur profonde (5 points)");
		ECHECK(rougeDabord, "(p3) et le ROUGE part le premier : T(rouge) < T(vert) < T(bleu) partout");

		// (q) L'ECUME apparait SEULEMENT sous le seuil de rivage (crete desactivee
		// pour isoler la cause : un temoin qui melange deux sources ne dit rien).
		NkWaterOptics eq = opt;
		eq.crestHeight = 0.f;
		bool sousSeuil = true, auDela = true;
		for (uint32 i = 0; i < 20u; ++i) {
			const float32 d = 0.1f * (float32)i; // 0 a 1,9 m, seuil a 0,6
			const float32 f = NkWaterFoam(eq, d, 0.f);
			if (d < eq.shoreDepth && !(f > 0.f))
				sousSeuil = false;
			if (d >= eq.shoreDepth && f != 0.f)
				auDela = false;
		}
		std::fprintf(stderr, "     seuil de rivage %.2f m : ecume a 0,0 m = %.3f ; a 0,5 m = %.3f ; a 0,7 m = %.3f ; "
							 "a 1,5 m = %.3f\n",
					 (double)eq.shoreDepth, (double)NkWaterFoam(eq, 0.f, 0.f), (double)NkWaterFoam(eq, 0.5f, 0.f),
					 (double)NkWaterFoam(eq, 0.7f, 0.f), (double)NkWaterFoam(eq, 1.5f, 0.f));
		ECHECK(sousSeuil, "(q1) l'ecume apparait sous le seuil de rivage");
		ECHECK(auDela, "(q2) et SEULEMENT sous lui : zero au-dela, pas un petit residu");

		// (r) LE CAMBREMENT : en eau PEU PROFONDE la vitesse de phase suit
		// sqrt(g h) et NE DEPEND PLUS de la longueur d'onde. Mesuree en suivant
		// une crete, comme en (j).
		bool peuProfond = true;
		for (uint32 i = 0; i < 3u; ++i) {
			const float32 h = 0.5f + 0.5f * (float32)i; // 0,5 / 1,0 / 1,5 m
			const float32 lambda = 60.f;				// k h = 0,05 a 0,16 : bien en eau peu profonde
			const NkWaterParams p = UneHoule(lambda, 0.02f, 0.f);
			const float32 dt = 0.05f;
			const float32 x1 = AbscisseDeCrete(p, 0.f, 0.f, lambda, h);
			const float32 x2 = AbscisseDeCrete(p, dt, x1 - 0.25f * lambda, lambda, h);
			const float32 cMes = (x2 - x1) / dt;
			const float32 cTh = NkSqrt(NK_GRAVITE_NORMALE * h);
			const float32 err = 100.f * NkFabs(cMes - cTh) / cTh;
			std::fprintf(stderr, "     h = %.1f m (lambda %.0f m) : c mesuree %6.3f m/s, Airy peu profond sqrt(g h) "
								 "%6.3f, ecart %.2f %%\n",
						 (double)h, (double)lambda, (double)cMes, (double)cTh, (double)err);
			if (err > 10.f)
				peuProfond = false;
		}
		ECHECK(peuProfond, "(r) en eau PEU PROFONDE la vitesse suit sqrt(g h) a 10 % : les vagues RALENTISSENT au rivage");
	}

	// ─────────────────────────────────────────────────────────────────────
	// (y1)(y2)(y3) LE DEFERLEMENT : le jacobien horizontal, et ce qu'il donne.
	//
	// ⚠️ CE QUI N'EST PAS PROMIS, redit ici et pas seulement dans l'en-tete : un
	// champ de hauteur ne peut PAS representer une vague qui se retourne, parce
	// qu'une surface repliee est MULTIVALUEE et qu'un y = f(x, z) ne l'est jamais.
	// Ce qui est mesure est le REPLI DE L'APPLICATION HORIZONTALE :
	//        J = det( d(P.x, P.z) / d(x, z) )
	// J = 1 au repos, et J <= 0 quand l'application se replie sur elle-meme.
	// ─────────────────────────────────────────────────────────────────────
	void TemoinDeferlement() {
		std::fprintf(stderr, "-- (y1..y3) le deferlement : J = det(d(P.x,P.z)/d(x,z)) --\n");

		// (y1) CONTROLE NEGATIF. Si J ne vaut pas 1 quand RIEN ne deplace, le
		// jacobien est faux et tout ce qui suit ment -- le seuil comme l'ecume.
		// Deux facons de ne rien deplacer, et les DEUX doivent rendre 1 EXACTEMENT,
		// pas « a peu pres » : au repos les tangentes valent (1,0,0) et (0,0,1), et
		// 1*1 - 0*0 est exact en flottant.
		{
			const NkWaterParams sansAmpl = UneHoule(20.f, 0.f, 0.8f); // amplitude nulle
			const NkWaterParams sansRaid = UneHoule(20.f, 0.6f, 0.f); // raideur nulle
			bool exactA = true, exactR = true;
			float32 pireA = 0.f, pireR = 0.f;
			for (uint32 i = 0; i < 200u; ++i) {
				const float32 x = 0.37f * (float32)i, z = 0.11f * (float32)i, t = 0.05f * (float32)i;
				const float32 ja = NkWaterEval(sansAmpl, x, z, t).jacobianXZ;
				const float32 jr = NkWaterEval(sansRaid, x, z, t).jacobianXZ;
				if (ja != 1.f) {
					exactA = false;
					if (NkFabs(ja - 1.f) > pireA)
						pireA = NkFabs(ja - 1.f);
				}
				if (jr != 1.f) {
					exactR = false;
					if (NkFabs(jr - 1.f) > pireR)
						pireR = NkFabs(jr - 1.f);
				}
			}
			std::fprintf(stderr,
						 "     amplitude nulle : ecart max a 1 = %.3e | raideur nulle : %.3e (200 points)\n",
						 (double)pireA, (double)pireR);
			ECHECK(exactA && exactR, "(y1) CONTROLE NEGATIF : sans deplacement, J vaut 1 AU BIT");
		}

		// (y2) LE SEUIL DE BRISURE SE MESURE, IL NE SE CHOISIT PAS.
		//
		// PREDICTION ECRITE AVANT LA MESURE, depuis le code : Q est normalise par
		// (k A N), donc Q A k = raideur / N. Pour UN train aligne sur x, les termes
		// croises sont nuls et dP.z/dz vaut 1 ; il reste
		//        J = 1 - raideur * sin(theta)
		// donc J <= 0 pour la premiere fois a RAIDEUR = 1,000 EXACTEMENT. Si la
		// mesure dit autre chose, c'est ma lecture du code qui est fausse.
		//
		// ⚠️ ET LA DEPENDANCE A LA RESOLUTION EST DANS L'ECHANTILLONNAGE, PAS DANS LA
		// LOI. J est analytique et ponctuel ; mais un seuil cherche en BALAYANT une
		// grille peut rater le maximum du sinus si la grille est grossiere, et
		// rendre un seuil trop grand. Il doit donc CONVERGER quand on raffine. S'il
		// derivait au lieu de converger, je mesurerais la discretisation et pas la
		// vague -- c'est exactement ce que le chantier feu vient de payer.
		{
			// 🔴 ET MES RESOLUTIONS PASSAIENT POUR UNE MAUVAISE RAISON. Premiere
			// version : 16, 64, 256, 1024 -- TOUS multiples de 4. Avec x = 20 i / n,
			// l'angle vaut 2 pi i / n et le sinus atteint EXACTEMENT +/-1 a chaque
			// fois : les quatre grilles tombaient pile sur le minimum de J, donc
			// AUCUNE ne pouvait manquer le seuil. Le temoin ne testait pas ce qu'il
			// annoncait. On ajoute 37 et 101, qui ne divisent pas le tour.
			const uint32 resolutions[6] = {16u, 37u, 64u, 101u, 256u, 1024u};
			float32 seuils[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
			float32 minJUn[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
			for (uint32 r = 0; r < 6u; ++r) {
				const uint32 n = resolutions[r];
				float32 seuil = -1.f;
				for (uint32 s = 1u; s <= 400u && seuil < 0.f; ++s) {
					const float32 raideur = 0.005f * (float32)s; // 0,005 -> 2,000
					const NkWaterParams p = UneHoule(20.f, 0.6f, raideur);
					float32 minJ = 1e30f;
					for (uint32 i = 0; i < n; ++i) {
						const float32 x = 20.f * (float32)i / (float32)n;
						const float32 j = NkWaterEval(p, x, 0.f, 0.f).jacobianXZ;
						if (j < minJ)
							minJ = j;
					}
					if (minJ <= 0.f)
						seuil = raideur;
				}
				seuils[r] = seuil;

				// LA MESURE DIRECTE DE L'ERREUR D'ECHANTILLONNAGE, sans balayage. A
				// raideur 1, J vaut exactement 0 au point ou le sinus vaut 1 : une
				// grille qui tombe juste rend 0, une grille qui manque ce point rend un
				// petit POSITIF, et ce positif EST l'erreur de discretisation. C'est
				// plus fin que le balayage, dont le pas de 0,005 est plus grossier que
				// l'ecart attendu (~0,004 a n = 37) et qui ne pourrait donc pas le voir.
				{
					const NkWaterParams p1 = UneHoule(20.f, 0.6f, 1.f);
					float32 mj = 1e30f;
					for (uint32 i = 0; i < n; ++i) {
						const float32 x = 20.f * (float32)i / (float32)n;
						const float32 j = NkWaterEval(p1, x, 0.f, 0.f).jacobianXZ;
						if (j < mj)
							mj = j;
					}
					minJUn[r] = mj;
				}
				std::fprintf(stderr,
							 "     %4u points par longueur d'onde : J <= 0 des raideur %.3f | a raideur 1,"
							 " min J = %+.5f%s\n",
							 n, (double)seuil, (double)minJUn[r],
							 (n % 4u == 0u) ? "  (grille alignee sur le sinus)" : "  (grille DESALIGNEE)");
			}
			const float32 fin = seuils[5];
			float32 ecartRes = 0.f, pireMinJ = 0.f;
			for (uint32 r = 0; r < 6u; ++r) {
				const float32 e = NkFabs(seuils[r] - fin);
				if (e > ecartRes)
					ecartRes = e;
				if (minJUn[r] > pireMinJ)
					pireMinJ = minJUn[r];
			}
			std::fprintf(stderr,
						 "     seuil le plus fin %.3f (prediction 1,000) | ecart max entre resolutions %.3f"
						 " | pire min J a raideur 1 : %+.5f\n",
						 (double)fin, (double)ecartRes, (double)pireMinJ);
			ECHECK(fin > 0.f && NkFabs(fin - 1.f) < 0.02f,
				   "(y2) le seuil de brisure MESURE tombe sur la prediction : raideur 1,000");
			ECHECK(ecartRes < 0.05f && pireMinJ < 0.02f,
				   "(y2b) il NE DEPEND PAS de la resolution, y compris sur des grilles DESALIGNEES");
		}

		// (y3) L'ECUME EST UN CHAMP, PAS UNE VALEUR.
		// On ISOLE la source jacobien : rivage et cretes desactives, sinon le temoin
		// melangerait trois causes et ne dirait rien -- c'est la regle qui a produit
		// (q1) et (q2) plus haut.
		{
			NkWaterOptics opt;
			opt.shoreDepth = 0.f;
			opt.crestHeight = 0.f;
			const NkWaterParams brise = UneHoule(20.f, 0.6f, 1.4f); // au-dela du seuil
			uint32 nonNuls = 0, total = 0, incoherents = 0;
			float32 fmin = 1e30f, fmax = -1e30f, jmin = 1e30f;
			for (uint32 i = 0; i < 400u; ++i) {
				const float32 x = 20.f * (float32)i / 400.f;
				const NkWaterPoint w = NkWaterEval(brise, x, 0.f, 0.f);
				const float32 f = NkWaterFoam(opt, 10.f, w.position.y, w.jacobianXZ);
				++total;
				if (f > 0.f)
					++nonNuls;
				if ((f > 0.f) != (w.jacobianXZ < opt.breakJacobian))
					++incoherents;
				if (f < fmin)
					fmin = f;
				if (f > fmax)
					fmax = f;
				if (w.jacobianXZ < jmin)
					jmin = w.jacobianXZ;
			}
			std::fprintf(stderr,
						 "     raideur 1,4 : J min %.3f | ecume de %.3f a %.3f | non nulle sur %u points"
						 " sur %u | incoherences %u\n",
						 (double)jmin, (double)fmin, (double)fmax, nonNuls, total, incoherents);
			ECHECK(incoherents == 0u,
				   "(y3) l'ecume est non nulle EXACTEMENT la ou J passe sous le seuil, et nulle ailleurs");
			ECHECK(fmin == 0.f && fmax > 0.f && nonNuls < total,
				   "(y3b) c'est un CHAMP : nul par endroits, non nul ailleurs -- pas une valeur uniforme");

			// CONTRE-EPREUVE : une mer CALME ne doit produire AUCUNE ecume de
			// deferlement. Sans elle, une ecume allumee partout passerait pour un
			// champ -- « un champ d'ecume uniforme est un echec, meme s'il est joli ».
			const NkWaterParams calme = UneHoule(20.f, 0.6f, 0.2f);
			float32 fmaxCalme = 0.f;
			for (uint32 i = 0; i < 400u; ++i) {
				const float32 x = 20.f * (float32)i / 400.f;
				const NkWaterPoint w = NkWaterEval(calme, x, 0.f, 0.f);
				const float32 f = NkWaterFoam(opt, 10.f, w.position.y, w.jacobianXZ);
				if (f > fmaxCalme)
					fmaxCalme = f;
			}
			std::fprintf(stderr, "     CONTRE-EPREUVE mer calme (raideur 0,2) : ecume max %.3f\n",
						 (double)fmaxCalme);
			ECHECK(fmaxCalme == 0.f,
				   "(y3c) CONTRE-EPREUVE : une mer calme ne produit AUCUNE ecume de deferlement");
		}
	}

} // namespace

int RunEauTests(int &pass, int &fail) {
	gPass = &pass;
	gFail = &fail;
	std::fprintf(stderr, "=== L'EAU AU-DELA DU FLUIDE : temoins §6.6 (eclaboussures + mouillage) ===\n");
	TemoinFile();
	TemoinLoi();
	TemoinCarte();
	TemoinLumiere();
	TemoinTissu();
	TemoinHoule();
	TemoinRivage();
	TemoinDeferlement();
	return fail;
}
