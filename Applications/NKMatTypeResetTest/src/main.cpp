// =============================================================================
// Applications/NKMatTypeResetTest/src/main.cpp
// =============================================================================
// POURQUOI CE PROGRAMME EXISTE
// -----------------------------------------------------------------------------
// Rodolf a signale DEUX FOIS le meme defaut — le 14 aout et le 21 aout :
// changer le type d'un materiau laissait passer les parametres communes a
// l'ancien et au nouveau type. La correction du 14 aout etait deux `if` ecrits a
// la main. Elle n'a pas tenu parce que **rien ne pouvait dire qu'il en
// manquait** : aucun controle ne parcourait les champs.
//
// Ce banc est ce controle. Il tourne en console, sans fenetre et sans device.
//
// CE QUE MESURE LA SIGNATURE
// -----------------------------------------------------------------------------
// Une ligne par cas. Les valeurs affichees sont celles de `NkVpMatParams` apres
// application de la regle — donc ce que le materiau porte reellement.
//
// ⚠️ REGIMES COUVERTS, ET CELUI QUI NE L'EST PAS
// COUVERT : la REGLE (`NkVpMatTypeDefaultsFor` / `NkVpMatAppliquerRegle`) et la
//   COUVERTURE des champs par la table. C'est la que vivait le defaut.
// NON COUVERT : la recopie de la ligne dans `NkVpProjMat` a l'interieur de
//   `Demo3DHostProjMatSetType`. Ce symbole vit dans `NkDemo3D.cpp` (18 636
//   lignes, device requis) et n'est PAS linkable depuis une console — c'est le
//   mur des 118 symboles `Demo3DHost*` qui a fait retirer `NkMatInventaireTest`
//   du workspace le 17/08. Cette recopie est donc gardee autrement : par un
//   `static_assert` sur `sizeof(NkVpProjMat)` pose a cote d'elle, qui casse le
//   BUILD si la struct grandit sans que la recopie suive. Un echec de build est
//   un cran plus fort qu'un banc rouge — il ne peut pas etre ignore.
// =============================================================================
#include "NK3DModeler/Viewport/NkVpMatTypeDefaults.h"

#include <cstdio>
#include <cstring>

using namespace nkentseu;

namespace {

	int32 gFail = 0;
	int32 gPass = 0;

	void Cas(bool ok, const char *quoi) {
		if (ok) {
			++gPass;
			printf("  [OK]   %s\n", quoi);
		} else {
			++gFail;
			printf("  [FAIL] %s\n", quoi);
		}
	}

	bool Pres(float32 a, float32 b) {
		const float32 d = a - b;
		return (d < 0.f ? -d : d) <= 0.0001f;
	}

	// Un materiau « touche partout » : aucune valeur ne vaut son defaut. C'est la
	// seule facon de voir une fuite — si un champ garde sa valeur d'origine apres
	// un changement de type, il a traverse.
	NkVpMatParams TouchePartout() {
		NkVpMatParams m = NkVpMatBaseDefaults();
		m.albedo[0] = 0.11f;
		m.albedo[1] = 0.22f;
		m.albedo[2] = 0.33f;
		m.rough = 0.123f;
		m.metal = 0.456f;
		m.clearcoat = 0.77f;
		m.ccRough = 0.66f;
		m.subsurface = 0.55f;
		m.nrmStrength = 1.9f;
		m.emiStrength = 7.7f;
		m.emissive[0] = 0.9f;
		m.emissive[1] = 0.8f;
		m.emissive[2] = 0.7f;
		m.parallax = 0.15f;
		m.shadowMode = 2;
		m.alpha = 0.30f;
		m.aniso = 0.44f;
		m.sheenV = 0.88f;
		m.toonThresh = 0.91f;
		m.toonSmooth = 0.92f;
		m.toonShadow[0] = 0.93f;
		m.toonShadow[1] = 0.94f;
		m.toonShadow[2] = 0.95f;
		m.outlineW = 9.f;
		m.outlineCol[0] = 0.96f;
		m.outlineCol[1] = 0.97f;
		m.outlineCol[2] = 0.98f;
		m.rimI = 0.99f;
		m.rimCol[0] = 0.51f;
		m.rimCol[1] = 0.52f;
		m.rimCol[2] = 0.53f;
		m.specHard = 77.f;
		m.emiEclaire = 1;
		return m;
	}

	// Compare octet a octet — aucun champ ne peut echapper a la comparaison,
	// contrairement a une liste de `if` ecrite a la main. C'est la meme idee que
	// le controle de couverture : ne pas dependre de ce a quoi on a pense.
	bool Identique(const NkVpMatParams &a, const NkVpMatParams &b) {
		return memcmp(&a, &b, sizeof(NkVpMatParams)) == 0;
	}

	// Combien de champs DIFFERENT de la reference — et lequel, pour le premier.
	uint32 ChampsQuiDiffere(const NkVpMatParams &a, const NkVpMatParams &b, const char **premier) {
		uint32 n = 0;
		*premier = "(aucun)";
		uint32 count = 0;
		const NkVpMatFieldDesc *f = NkVpMatFields(count);
		for (uint32 i = 0; i < count; ++i) {
			const char *pa = (const char *)&a + f[i].offset;
			const char *pb = (const char *)&b + f[i].offset;
			if (memcmp(pa, pb, f[i].taille) != 0) {
				if (n == 0)
					*premier = f[i].nom;
				++n;
			}
		}
		return n;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 1. ⭐ LE CONTROLE QUI PROTEGE LE CODE DE DANS SIX MOIS
	// Les autres cas verifient le comportement d'aujourd'hui. Celui-ci verifie
	// qu'un champ ajoute DEMAIN a `NkVpMatParams` sans entree dans la table rend
	// le banc rouge. C'est exactement ce qui a manque aux deux `if` du 14 aout :
	// la liste etait incomplete et rien ne pouvait le dire.
	// ─────────────────────────────────────────────────────────────────────────
	void Test_CouvertureDesChamps() {
		printf("-- type/couverture-des-champs --\n");
		uint32 count = 0;
		const NkVpMatFieldDesc *f = NkVpMatFields(count);

		// (a) La somme des tailles doit faire EXACTEMENT sizeof. `emiEclaire` est
		//     un int32 et non un bool precisement pour qu'il n'y ait aucun octet
		//     de bourrage ou un champ oublie pourrait se cacher.
		uint32 somme = 0;
		for (uint32 i = 0; i < count; ++i)
			somme += f[i].taille;
		printf("     champs=%u somme=%u sizeof=%u\n", count, somme, (uint32)sizeof(NkVpMatParams));
		Cas(somme == (uint32)sizeof(NkVpMatParams),
			"type/couverture-des-champs : la table couvre TOUS les octets de NkVpMatParams");

		// (b) Aucun trou, aucun chevauchement : les descripteurs doivent se
		//     suivre exactement. Une somme juste avec un trou et un doublon
		//     passerait le test (a) — pas celui-ci.
		bool contigus = (count > 0) && (f[0].offset == 0);
		for (uint32 i = 1; i < count && contigus; ++i)
			contigus = (f[i].offset == f[i - 1].offset + f[i - 1].taille);
		Cas(contigus, "type/couverture-des-champs : descripteurs contigus (ni trou ni chevauchement)");
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 2. ⭐ LE CAS QUI PROUVE QUE LA LISTE DU 14 AOUT NE SUFFISAIT PAS
	// On prend `rough` et `clearcoat` — volontairement ABSENTS des deux `if` —
	// et on montre qu'ils traversent l'ancienne regle.
	// ─────────────────────────────────────────────────────────────────────────
	void Test_AncienneRegleFuit() {
		printf("-- type/ancienne-regle-fuit --\n");
		NkVpMatParams m = TouchePartout();
		const float32 roughAvant = m.rough, ccAvant = m.clearcoat;
		NkVpMatAppliquerAncienneRegle(m, 5 /* verre */, 0 /* Standard */);
		printf("     rough %.3f -> %.3f | clearcoat %.3f -> %.3f\n", (double)roughAvant, (double)m.rough,
			   (double)ccAvant, (double)m.clearcoat);
		// Ce cas AFFIRME la fuite : c'est un TEMOIN de l'ancien comportement.
		Cas(Pres(m.rough, roughAvant) && Pres(m.clearcoat, ccAvant),
			"type/ancienne-regle-fuit : rough et clearcoat TRAVERSENT l'ancienne liste (temoin)");

		const NkVpMatParams ref = NkVpMatTypeDefaultsFor(0);
		const char *premier = "";
		const uint32 n = ChampsQuiDiffere(m, ref, &premier);
		printf("     champs encore differents des defauts Standard : %u (premier : %s)\n", n, premier);
		Cas(n > 0, "type/ancienne-regle-fuit : l'ancienne regle NE reset PAS au nouveau type");
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 3. La nouvelle regle : verre a 0,30 -> Standard, TOUT vaut les defauts
	// ─────────────────────────────────────────────────────────────────────────
	void Test_VerreVersStandard() {
		printf("-- type/verre-vers-standard --\n");
		NkVpMatParams m = TouchePartout();
		m.alpha = 0.30f;
		NkVpMatAppliquerRegle(m, 0);
		const char *premier = "";
		const uint32 n = ChampsQuiDiffere(m, NkVpMatTypeDefaultsFor(0), &premier);
		printf("     champs differents des defauts Standard : %u (premier : %s) | rough=%.3f alpha=%.3f\n", n,
			   premier, (double)m.rough, (double)m.alpha);
		Cas(n == 0, "type/verre-vers-standard : TOUS les numeriques valent les defauts Standard");
		Cas(Identique(m, NkVpMatTypeDefaultsFor(0)), "type/verre-vers-standard : egalite octet a octet");
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 4. Standard modifie partout -> Verre : alpha = 0,12 MEME SI l'opacite
	//    avait ete touchee. C'est le « sauf si » que Rodolf a fait retirer.
	// ─────────────────────────────────────────────────────────────────────────
	void Test_VersVerreSansSaufSi() {
		printf("-- type/vers-verre-sans-sauf-si --\n");
		NkVpMatParams m = TouchePartout();
		m.alpha = 0.77f; // opacite DEJA touchee : l'ancien code n'aurait rien pose
		NkVpMatAppliquerRegle(m, 5);
		printf("     alpha=%.3f (0,120 attendu) rough=%.3f (0,850 attendu)\n", (double)m.alpha, (double)m.rough);
		Cas(Pres(m.alpha, 0.12f), "type/vers-verre-sans-sauf-si : alpha=0,12 malgre une opacite deja touchee");
		Cas(Identique(m, NkVpMatTypeDefaultsFor(5)), "type/vers-verre-sans-sauf-si : egalite octet a octet");
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 5. ⚠️ A -> B -> A NE REND PAS LES VALEURS D'ORIGINE, et c'est VOULU.
	// Consequence assumee de la regle. Ecrite ici pour que personne ne la prenne
	// un jour pour un bug et ne « repare » en reintroduisant une memoire.
	// ─────────────────────────────────────────────────────────────────────────
	void Test_AllerRetour() {
		printf("-- type/aller-retour-perd-les-reglages --\n");
		NkVpMatParams m = TouchePartout();
		const float32 roughOrigine = m.rough;
		NkVpMatAppliquerRegle(m, 5); // A -> B
		NkVpMatAppliquerRegle(m, 0); // B -> A
		printf("     rough origine=%.3f, apres aller-retour=%.3f (defaut Standard 0,850)\n", (double)roughOrigine,
			   (double)m.rough);
		Cas(Pres(m.rough, 0.85f) && !Pres(m.rough, roughOrigine),
			"type/aller-retour : le retour rend les DEFAUTS de A, pas les valeurs d'origine (VOULU)");
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 6. Un type SANS gabarit ne laisse rien de l'ancien. 16 des 33 types
	//    declares n'ont aucun gabarit cote moteur : c'est le cas frequent, pas
	//    le cas limite.
	// ─────────────────────────────────────────────────────────────────────────
	void Test_TypeSansGabarit() {
		printf("-- type/sans-gabarit --\n");
		const int32 kInconnus[3] = {7 /* carrosserie */, 30 /* non declare */, 250 /* hors bornes */};
		uint32 pires = 0;
		for (int32 k = 0; k < 3; ++k) {
			NkVpMatParams m = TouchePartout();
			NkVpMatAppliquerRegle(m, kInconnus[k]);
			const char *premier = "";
			const uint32 n = ChampsQuiDiffere(m, NkVpMatBaseDefaults(), &premier);
			printf("     type %-3d : champs restes de l'ancien = %u (premier : %s)\n", kInconnus[k], n, premier);
			if (n > pires)
				pires = n;
		}
		Cas(pires == 0, "type/sans-gabarit : aucun champ de l'ancien ne survit (base ENTIERE, jamais partielle)");
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 7. L'emissif s'allume. Le defaut du 14 aout : sphere NOIRE parce que
	//    l'emission partait a zero.
	// ─────────────────────────────────────────────────────────────────────────
	void Test_EmissifSAllume() {
		printf("-- type/emissif-s-allume --\n");
		NkVpMatParams m = NkVpMatBaseDefaults();
		NkVpMatAppliquerRegle(m, 11);
		printf("     emissive=(%.2f,%.2f,%.2f)\n", (double)m.emissive[0], (double)m.emissive[1],
			   (double)m.emissive[2]);
		Cas(m.emissive[0] > 0.5f && m.emissive[1] > 0.5f && m.emissive[2] > 0.5f,
			"type/emissif-s-allume : emission non nulle (une lampe allumee, pas une sphere noire)");
	}

} // namespace

int main() {
	printf("=== NKMatTypeResetTest : changer le type RESET le materiau (console, sans UI) ===\n");
	Test_CouvertureDesChamps();
	Test_AncienneRegleFuit();
	Test_VerreVersStandard();
	Test_VersVerreSansSaufSi();
	Test_AllerRetour();
	Test_TypeSansGabarit();
	Test_EmissifSAllume();
	printf("=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	return gFail == 0 ? 0 : 1;
}
