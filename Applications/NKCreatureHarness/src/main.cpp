// -----------------------------------------------------------------------------
// FICHIER: Applications\NKCreatureHarness\src\main.cpp
// DESCRIPTION: Banc du generateur de creatures : les criteres du §6 sur le
//              catalogue de chaque palier, chacun avec son temoin qui rougit.
// AUTEUR: TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026-09-29
// VERSION: 0.1.0
// -----------------------------------------------------------------------------
//
// G1 (04 §14) : « un serpent et un bras a 3 articulations passent le §6 ».
//
// ⚠️ UN CRITERE QUI NE PEUT PAS ROUGIR NE PROUVE RIEN. Chaque critere du §6 a ici
//    un temoin construit pour le faire echouer ; si le temoin passe, c'est la
//    MESURE qui est fausse, et le banc rougit.
// ⚠️ LA FIDELITE AUX VOLUMES (§6) n'est pas mesuree : il n'y a pas de volumes
//    avant G3. Le banc le dit au lieu de l'omettre.
// -----------------------------------------------------------------------------

// ============================================================
// INCLUDES
// ============================================================

#include "NKRenderer/Mesh/NkCreatureMesures.h"
#include "NKRenderer/Mesh/NkCreaturePeau.h"
#include "NKRenderer/Mesh/NkCreatureScene.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
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
		std::printf("%-6s %-46s %s\n", ok ? "OK" : "ROUGE", cas, detail);
	}

	// ========================================
	// LE CATALOGUE G1
	// ========================================

	// Un serpent : une chaine de 24 os, courbee, effilee, section aplatie.
	const char *NK_BANC_SERPENT =
		"nkscene 2\n"
		"creature \"Serpent d'essai\"\n"
		"squelette\n"
		"  chaine corps racine segments 24 longueur 3.0 direction 1 0 0 courbure 0 0.3 1.2 rayon 0.08 effilement 0.4\n"
		"forme\n"
		"  profil corps clefs 0:0.12x0.10 0.12:0.17x0.14 0.75:0.13x0.11 1:0.05x0.05\n"
		"  resolution anneaux 12 boucles_articulation 3\n";

	// Un bras a 3 articulations (coude, poignet, doigt), mis en miroir.
	const char *NK_BANC_BRAS =
		"nkscene 2\n"
		"creature \"Bras d'essai\" symetrie X\n"
		"squelette\n"
		"  membre bras_g racine direction 1 0 0\n"
		"    os humerus position 0.2 1.4 0 longueur 0.32 rayon 0.055 0.045\n"
		"    os avant_bras longueur 0.28 direction 1 -0.25 0 rayon 0.045 0.035\n"
		"    os main longueur 0.10 direction 1 -0.3 0\n"
		"    os doigt longueur 0.08 rayon 0.016 0.013\n"
		"  miroir *_g -> *_d\n"
		"forme\n"
		"  profil main_* clefs 0:0.070x0.060 1:0.034x0.030\n"
		"  resolution anneaux 12 boucles_articulation 3\n";

	// ── VALIDATION : une creature qui n'a servi a REGLER aucun parametre ──
	// La bande de pli (1,1 rayon) a ete choisie sur le serpent et le bras, la regle
	// d'angle de repos sur une jambe. Cette patte en zigzag (angulations de chien,
	// quatre os, section aplatie au pied) n'a ete construite qu'APRES : elle dit si
	// les reglages generalisent ou s'ils collent aux exemples.
	const char *NK_BANC_PATTE =
		"nkscene 2\n"
		"creature \"Patte d'essai\" symetrie X\n"
		"squelette\n"
		"  membre patte_g racine\n"
		"    os cuisse position 0.15 0.9 -0.4 longueur 0.30 direction 0 -1 0.35 rayon 0.075 0.05\n"
		"    os jambe longueur 0.28 direction 0 -1 -0.55 rayon 0.05 0.035\n"
		"    os metatarse longueur 0.20 direction 0 -1 0.15 rayon 0.035 0.03\n"
		"    os doigts longueur 0.08 direction 0 -0.3 1 rayon 0.03 0.022\n"
		"  miroir *_g -> *_d\n"
		"forme\n"
		"  profil doigts_* section superellipse largeur 0.075 hauteur 0.04 exposant 2.6\n"
		"  resolution anneaux 14 boucles_articulation 3\n";

	// ── TEMOINS : chacun doit faire rougir UN critere ─────────────────────
	// Doigt trop court : les poles du capuchon tombent pres de la bande de pli.
	const char *NK_TEMOIN_POLE =
		"nkscene 2\n"
		"squelette\n"
		"  os paume racine longueur 0.2 direction 1 0 0 rayon 0.03\n"
		"  os bout depuis paume longueur 0.018 rayon 0.03\n"
		"forme\n"
		"  resolution anneaux 12 boucles_articulation 3\n";

	// Une epingle a cheveux serree. Avant l'arc de conge, le tube se TRAVERSAIT
	// (c'etait le temoin d'auto-intersection) ; depuis, la peau est REFUSEE, nommee :
	// l'os est trop court pour l'arc que son angle exige. Le banc verifie ce refus.
	const char *NK_TEMOIN_EPINGLE =
		"nkscene 2\n"
		"squelette\n"
		"  chaine u racine segments 6 longueur 0.9 direction 1 0 0 courbure -3.0 0 2.2 rayon 0.16\n"
		"forme\n"
		"  resolution anneaux 12 boucles_articulation 3\n";

	// Deux tubes qui se croisent : le detecteur d'auto-intersection doit les voir.
	const char *NK_TEMOIN_INTERSECTION =
		"nkscene 2\n"
		"squelette\n"
		"  os a racine position -0.5 0 0 longueur 1 direction 1 0 0 rayon 0.1\n"
		"  os b racine position 0 -0.5 0 longueur 1 direction 0 1 0 rayon 0.1\n"
		"forme\n"
		"  resolution anneaux 12 boucles_articulation 3\n";

	// Un bras SANS son miroir : la creature n'est pas symetrique.
	const char *NK_TEMOIN_SYMETRIE =
		"nkscene 2\n"
		"squelette\n"
		"  membre bras_g racine direction 1 0 0\n"
		"    os humerus position 0.2 1.4 0 longueur 0.32 rayon 0.055 0.045\n"
		"forme\n"
		"  resolution anneaux 12 boucles_articulation 3\n";

	// Un seul os tres court a 4 sommets par anneau : trop peu de sommets pour
	// diluer les 8 poles (§2.1 : il faut N >= 20 I).
	const char *NK_TEMOIN_VALENCE =
		"nkscene 2\n"
		"squelette\n"
		"  os bloc racine longueur 0.05 rayon 0.05\n"
		"forme\n"
		"  resolution anneaux 4 boucles_articulation 3\n";

	bool Construire(const char *texte, NkR32Document &doc, NkR32Peau &peau, char *pq, uint32 cap) {
		char avert[1024];
		if (!NkCreatureLireScene(texte, doc, pq, cap, avert, sizeof(avert))) {
			return false;
		}
		if (avert[0]) {
			std::printf("       avertissements :\n%s", avert);
		}
		return NkR32ConstruirePeau(doc, peau, pq, cap);
	}

	void Imprimer(const char *nom, const NkCreatureMesure &m) {
		std::printf("       %s : %u faces (%u quads) | subdivisee : %u sommets, %u valence 4 | euler %d, "
					"composantes %u, genre %d\n",
					nom, m.faces, m.quads, m.sommetsSubdivises, m.valence4, m.euler, m.composantes, m.genre);
	}

	// Les criteres du §6 sur une creature du catalogue.
	void JugerCatalogue(const char *nom, const char *texte, bool symetrique) {
		std::printf("\n== %s ==\n", nom);
		char cas[64];
		char pq[512];
		NkR32Document doc;
		NkR32Peau peau;
		if (!Construire(texte, doc, peau, pq, sizeof(pq))) {
			std::snprintf(cas, sizeof(cas), "%s/construit", nom);
			Verifier(false, cas, "%s", pq);
			return;
		}
		NkCreatureMesure m;
		if (!NkCreatureMesurer(doc, peau, m, pq, sizeof(pq))) {
			std::snprintf(cas, sizeof(cas), "%s/mesure", nom);
			Verifier(false, cas, "%s", pq);
			return;
		}
		const NkCreatureCriteres c;
		Imprimer(nom, m);
		std::snprintf(cas, sizeof(cas), "%s/quads", nom);
		Verifier(m.partQuads >= c.partQuads, cas, "%.4f (cible 100 %%)", (double)m.partQuads);
		std::snprintf(cas, sizeof(cas), "%s/valence4-subdivisee", nom);
		Verifier(m.partValence4 >= c.partValence4, cas, "%.2f %% (cible >= 95 %%)", (double)(100.f * m.partValence4));
		std::snprintf(cas, sizeof(cas), "%s/somme(4-val)=8-8g", nom);
		Verifier(m.sommeEcarts == 8 * (int32)m.composantes && m.genre == 0, cas, "%d (attendu %d, genre %d)",
				 m.sommeEcarts, 8 * (int32)m.composantes, m.genre);
		std::snprintf(cas, sizeof(cas), "%s/variete", nom);
		Verifier(m.aretesNonManifold == 0u && m.aretesBord == 0u && m.autoIntersections == 0u, cas,
				 "non-manifold %u, bord %u, auto-intersections %u%s%s%s%s", m.aretesNonManifold, m.aretesBord,
				 m.autoIntersections, m.autoIntersections ? " (" : "", m.intersectionOs[0],
				 m.autoIntersections ? " x " : "", m.autoIntersections ? m.intersectionOs[1] : "");
		std::snprintf(cas, sizeof(cas), "%s/boucles-par-articulation", nom);
		Verifier(m.articulations > 0u && m.bouclesMin >= c.bouclesMin && m.bouclesMax <= c.bouclesMax, cas,
				 "%u articulations, %u a %u boucles", m.articulations, m.bouclesMin, m.bouclesMax);
		std::snprintf(cas, sizeof(cas), "%s/poles-loin-des-plis", nom);
		Verifier(m.polesPresDesPlis == 0u, cas, "%u poles, %u a moins de 2 anneaux d'un pli (distance min %d)",
				 m.poles, m.polesPresDesPlis, m.distancePoleMin);
		if (symetrique) {
			std::snprintf(cas, sizeof(cas), "%s/symetrie", nom);
			Verifier(m.ecartSymetrie <= c.ecartSymetrie, cas, "ecart max %.3g (cible <= 1e-6)", (double)m.ecartSymetrie);
			if (m.ecartSymetrie > c.ecartSymetrie) {
				// DIAGNOSTIC : les premiers sommets sans miroir, avec leur adresse.
				const NkEditMesh &mm = peau.maillage;
				uint32 montres = 0u;
				for (uint32 v = 0u; v < mm.VertCount() && montres < 6u; ++v) {
					NkVec3f q = mm.verts[v].pos;
					q.x = -q.x;
					float32 meilleur = 1e30f;
					for (uint32 w = 0u; w < mm.VertCount(); ++w) {
						const NkVec3f r = mm.verts[w].pos;
						const float32 d = (r.x - q.x) * (r.x - q.x) + (r.y - q.y) * (r.y - q.y) + (r.z - q.z) * (r.z - q.z);
						meilleur = d < meilleur ? d : meilleur;
					}
					if (meilleur > 1e-12f) {
						const NkR32AdresseSommet &a = peau.sommets[v];
						std::printf("       sans miroir : sommet %u os `%s` lieu %d s %.3f a %.3f anneau %d, ecart %.3g\n", v,
									doc.os[a.os].nom, (int)a.lieu, (double)a.s, (double)a.a, a.anneau,
									(double)std::sqrt(meilleur));
						++montres;
					}
				}
			}
		}
		std::snprintf(cas, sizeof(cas), "%s/regularite", nom);
		Verifier(m.partReguliere >= c.partReguliere, cas,
				 "%.2f %% des faces (cotes <= 3, angles 45-135), rapport max %.2f (`%s`)",
				 (double)(100.f * m.partReguliere), (double)m.rapportCotesMax, m.rapportOs);
		// Juge en QUATERNIONS DUAUX (decision du 29/09, NkCreatureMesures.h) ; le
		// lineaire -- le skinning du moteur aujourd'hui -- est affiche, pas juge.
		std::snprintf(cas, sizeof(cas), "%s/pli-90-quaternions-duaux", nom);
		Verifier(m.articulationsPliees == m.articulations && m.perteVolumeLocalePire <= c.perteVolume &&
					 m.facesRetournees == 0u,
				 cas, "%u articulations : perte locale pire %.1f %% (`%s`), totale %.1f %%, %u pliures (pire : `%s`, %u)",
				 m.articulationsPliees, (double)(100.f * m.perteVolumeLocalePire), m.articulationPerte,
				 (double)(100.f * m.perteVolumeTotalePire), m.facesRetournees, m.articulationPire, m.pliuresPire);
		std::printf("       skinning LINEAIRE (le moteur aujourd'hui) : perte locale pire %.1f %%, %u pliures -- affiche, "
					"pas juge\n",
					(double)(100.f * m.perteVolumeLocaleLineaire), m.facesRetourneesLineaire);
		NkR32Peau autre;
		NkR32ConstruirePeau(doc, autre, pq, sizeof(pq));
		const uint64 h1 = NkR32Empreinte(peau.maillage);
		const uint64 h2 = NkR32Empreinte(autre.maillage);
		std::snprintf(cas, sizeof(cas), "%s/deterministe", nom);
		Verifier(h1 == h2, cas, "%016llx / %016llx", (unsigned long long)h1, (unsigned long long)h2);
		std::printf("       fidelite aux volumes (§6) : NON MESUREE -- pas de volumes avant G3\n");
		// Le temoin des poids : tout-ou-rien au joint doit faire MOINS bien.
		NkCreatureMesure rigide;
		NkCreatureMesurer(doc, peau, rigide, pq, sizeof(pq), NkCreaturePoids::Nk_CreaturePoids_Rigide);
		std::printf("       temoin poids rigides : perte locale %.1f %%, %u faces retournees\n",
					(double)(100.f * rigide.perteVolumeLocalePire), rigide.facesRetournees);
	}

	// Chaque temoin doit faire rougir SON critere.
	void Temoins() {
		std::printf("\n== temoins : chaque critere doit savoir rougir ==\n");
		char pq[512];
		NkCreatureMesure m;
		{
			NkR32Document doc;
			NkR32Peau peau;
			const bool ok = Construire(NK_TEMOIN_POLE, doc, peau, pq, sizeof(pq)) &&
							NkCreatureMesurer(doc, peau, m, pq, sizeof(pq));
			Verifier(ok && m.polesPresDesPlis > 0u, "temoin/pole-pres-d'un-pli", "%u poles pres d'un pli (distance min %d)",
					 m.polesPresDesPlis, m.distancePoleMin);
		}
		{
			NkR32Document doc;
			NkR32Peau peau;
			const bool ok = Construire(NK_TEMOIN_INTERSECTION, doc, peau, pq, sizeof(pq)) &&
							NkCreatureMesurer(doc, peau, m, pq, sizeof(pq));
			Verifier(ok && m.autoIntersections > 0u, "temoin/auto-intersection", "%u paires de triangles se traversent%s%s",
					 m.autoIntersections, ok ? "" : " -- REFUSE : ", ok ? "" : pq);
		}
		{
			NkR32Document doc;
			NkR32Peau peau;
			const bool ok = Construire(NK_TEMOIN_EPINGLE, doc, peau, pq, sizeof(pq));
			Verifier(!ok && std::strstr(pq, "trop court") != nullptr, "temoin/epingle-refusee-pas-repliee", "%s",
					 ok ? "ACCEPTEE (la peau se replierait)" : pq);
		}
		{
			NkR32Document doc;
			NkR32Peau peau;
			const bool ok = Construire(NK_TEMOIN_SYMETRIE, doc, peau, pq, sizeof(pq)) &&
							NkCreatureMesurer(doc, peau, m, pq, sizeof(pq));
			Verifier(ok && m.ecartSymetrie > 1e-6f, "temoin/asymetrie", "ecart max %.3g", (double)m.ecartSymetrie);
		}
		{
			NkR32Document doc;
			NkR32Peau peau;
			const bool ok = Construire(NK_TEMOIN_VALENCE, doc, peau, pq, sizeof(pq)) &&
							NkCreatureMesurer(doc, peau, m, pq, sizeof(pq));
			Verifier(ok && m.partValence4 < 0.95f, "temoin/valence(N<20I)", "%.2f %% de valence 4 sur %u sommets",
					 (double)(100.f * m.partValence4), m.sommetsSubdivises);
		}
		{
			// Quads etires : 32 sommets par anneau mais des anneaux tres espaces.
			NkR32Document doc;
			NkR32Peau peau;
			bool ok = Construire(NK_BANC_BRAS, doc, peau, pq, sizeof(pq));
			doc.anneaux = 32u;
			doc.densite = 2.f;
			ok = ok && NkR32ConstruirePeau(doc, peau, pq, sizeof(pq)) && NkCreatureMesurer(doc, peau, m, pq, sizeof(pq));
			Verifier(ok && m.partReguliere < 0.95f, "temoin/quads-etires", "%.2f %% reguliers, rapport max %.1f",
					 (double)(100.f * m.partReguliere), (double)m.rapportCotesMax);
		}
		{
			// La peau G0 ne porte pas l'adresse de ses sommets : la mesure REFUSE au
			// lieu de rendre des chiffres faux.
			NkR32Document doc;
			NkR32Peau peau;
			bool ok = Construire(NK_BANC_BRAS, doc, peau, pq, sizeof(pq));
			doc.generateur = 1u;
			doc.densite = 8.f;
			ok = ok && NkR32ConstruirePeau(doc, peau, pq, sizeof(pq));
			const bool refus = !NkCreatureMesurer(doc, peau, m, pq, sizeof(pq));
			Verifier(ok && refus, "temoin/peau-G0-refusee-par-la-mesure", "%s", pq);
		}
	}

	void Lecteur() {
		std::printf("\n== lecteur .nkscene v2 ==\n");
		char pq[512];
		NkR32Document doc;
		NkR32Peau peau;
		// Les os d'une chaine s'appellent chaine.k ; `depuis chaine` = son dernier os.
		const char *texte =
			"nkscene 2\n"
			"squelette\n"
			"  chaine colonne racine segments 4 longueur 1.2 direction 0 1 0\n"
			"  os tete depuis colonne longueur 0.3 rayon 0.12\n";
		const bool lu = NkCreatureLireScene(texte, doc, pq, sizeof(pq));
		const int32 t = doc.Trouver("tete");
		Verifier(lu && doc.Trouver("colonne.1") >= 0 && doc.Trouver("colonne.4") >= 0 && t >= 0 &&
					 std::strcmp(doc.os[(uint32)t].parent, "colonne.4") == 0,
				 "lecteur/chaine.k-et-depuis-chaine", "%s", lu ? "colonne.1..4, tete depuis colonne.4" : pq);
		// Miroir : bras_g -> bras_d, x change de signe.
		const bool lub = NkCreatureLireScene(NK_BANC_BRAS, doc, pq, sizeof(pq));
		const int32 g = doc.Trouver("humerus_g");
		const int32 d = doc.Trouver("humerus_d");
		Verifier(lub && g >= 0 && d >= 0 && doc.os[(uint32)g].racine.x == -doc.os[(uint32)d].racine.x &&
					 doc.Trouver("doigt_d") >= 0 && std::strcmp(doc.os[(uint32)doc.Trouver("doigt_d")].parent, "main_d") == 0,
				 "lecteur/membre-suffixe-et-miroir", "%u os", (uint32)doc.os.Size());
		// L'exemple du dragon (04 §4.1) : G1 doit s'arreter NOMMEMENT sur `extremite`.
		const char *dragon =
			"nkscene 2\n"
			"creature \"Dragon des Monts\" style \"heroique\" symetrie X\n"
			"squelette\n"
			"  os bassin        racine            longueur 0.60  direction 0 0 1\n"
			"  chaine colonne   depuis bassin     segments 4  longueur 1.40  courbure 0 0.15 0\n"
			"  chaine cou       depuis colonne    segments 5  longueur 1.10  courbure 0 0.40 0\n"
			"  os tete          depuis cou        longueur 0.50\n"
			"  chaine queue     depuis bassin     segments 10 longueur 2.80  effilement 0.12  vers -Z\n"
			"  membre patte_arriere_g depuis bassin\n"
			"    os cuisse longueur 0.70  os jambe longueur 0.60  os metatarse longueur 0.35\n"
			"    extremite pied doigts 4  phalanges 3  griffes oui\n";
		const bool dr = NkCreatureLireScene(dragon, doc, pq, sizeof(pq));
		Verifier(!dr && std::strstr(pq, "ligne 11") && std::strstr(pq, "extremite") && std::strstr(pq, "G2"),
				 "lecteur/dragon-s'arrete-sur-extremite", "%s", pq);
		// Le squelette du dragon SANS extremite se lit, mais sa peau refuse
		// l'embranchement (bassin porte colonne, queue et patte) : c'est G2.
		const char *sansExtremite =
			"nkscene 2\n"
			"squelette\n"
			"  os bassin racine longueur 0.60 direction 0 0 1\n"
			"  chaine colonne depuis bassin segments 4 longueur 1.40 courbure 0 0.15 0\n"
			"  chaine queue depuis bassin segments 10 longueur 2.80 effilement 0.12 vers -Z\n";
		const bool lu2 = NkCreatureLireScene(sansExtremite, doc, pq, sizeof(pq));
		const bool peau2 = lu2 && NkR32ConstruirePeau(doc, peau, pq, sizeof(pq));
		Verifier(lu2 && !peau2 && std::strstr(pq, "JONCTION"), "lecteur/embranchement=G2", "%s", pq);
		const bool vol = NkCreatureLireScene("nkscene 2\nvolumes\n  volume ventre sur colonne.1\n", doc, pq, sizeof(pq));
		Verifier(!vol && std::strstr(pq, "G3"), "lecteur/volumes=G3", "%s", pq);
		const bool tete = NkCreatureLireScene("nkscene 2\nforme\n  tete gabarit \"reptile\"\n", doc, pq, sizeof(pq));
		Verifier(!tete && std::strstr(pq, "G4"), "lecteur/tete=G4", "%s", pq);
		const bool inconnu = NkCreatureLireScene("nkscene 2\nsquelette\n  os a racine longeur 1\n", doc, pq, sizeof(pq));
		Verifier(!inconnu && std::strstr(pq, "longeur"), "lecteur/mot-inconnu-refuse", "%s", pq);
		const bool deux = NkCreatureLireScene(
			"nkscene 2\nsquelette\n  os a racine longueur 1\nforme\n  resolution anneaux 12 boucles_articulation 2\n", doc,
			pq, sizeof(pq));
		const bool peau3 = deux && NkR32ConstruirePeau(doc, peau, pq, sizeof(pq));
		Verifier(deux && !peau3, "lecteur/2-boucles-refusees-en-G1", "%s", pq);
		const bool v1 = NkCreatureLireScene("scene bonhomme\npartie base forme sphere\n", doc, pq, sizeof(pq));
		Verifier(!v1 && std::strstr(pq, "nkscene 2"), "lecteur/v1-refuse", "%s", pq);
	}

}  // namespace

// ============================================================
// POINT D'ENTREE
// ============================================================

int main() {
	std::printf("NKCreatureHarness -- generateur de creatures, criteres du §6 (palier G1)\n");
	JugerCatalogue("serpent", NK_BANC_SERPENT, false);
	JugerCatalogue("bras-3-articulations", NK_BANC_BRAS, true);
	JugerCatalogue("patte-zigzag(validation)", NK_BANC_PATTE, true);
	Temoins();
	Lecteur();
	std::printf("\n%d critere(s) ROUGE(S) sur %d\n", nkEchecs, nkVerifs);
	return nkEchecs == 0 ? 0 : 1;
}

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Creation Date: 2026-09-29
// ============================================================
