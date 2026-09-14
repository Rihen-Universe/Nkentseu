// =============================================================================
// Applications/NKFoldTest/src/main.cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// POURQUOI CE PROGRAMME EXISTE
// -----------------------------------------------------------------------------
// Le panneau de proprietes du modeleur portait l'etat de pliage de ses groupes
// dans un `uint32`, UN BIT par groupe, choisi a la main a chaque appel. Mesure du
// 14/09 : 27 groupes pour 16 bits distincts, SIX bits partages par 17 groupes, et
// un `prop.g.cam` portant le bit 3 — pas une puissance de deux — dont le XOR
// bascule sept groupes d'un coup.
//
// Effet visible aujourd'hui : plier « SSAO » plie aussi GI, PostFX, Ombres et
// Transformation. Ce banc exerce la REGLE D'ETAT qui remplace ce masque, et le
// critere qu'il verifie n'est PAS « ca marche » mais **chacun se plie seul**.
//
// L'ATTENDU EST DERIVE, PAS RECOPIE
// -----------------------------------------------------------------------------
// Les 27 cles ci-dessous ont ete EXTRAITES du fichier source (les appels a
// `PaintPropGroup`), pas retapees. Et le banc ne compare a aucun nombre en dur :
// il verifie une PROPRIETE — plier l'un ne change aucun des autres — qui reste
// vraie quel que soit le nombre de groupes a venir.
//
// SANS FENETRE
// -----------------------------------------------------------------------------
// `NkModelerFold.h` n'inclut que `NKCore/NkTypes.h`. La regle a ete sortie du
// panneau pour etre atteignable, exactement comme `NkVpEditTarget.h` avait ete
// sorti de NkDemo3D.cpp — un fichier de 18 000 lignes qui exige un device.
//
// CE QUI N'EST PAS COUVERT — a dire plutot que de laisser croire
// -----------------------------------------------------------------------------
//   * `PaintPropGroup` lui-meme, le chevron, le clic, la hauteur du panneau ;
//   * le negatif « deplie puis replie rend EXACTEMENT la hauteur de depart » :
//     il se verifie a l'image, pas ici.
// =============================================================================
#include "NK3DModeler/Shell/NkModelerFold.h"

#include <stdio.h>

using namespace nkentseu;
using namespace nkentseu::nk3d;

namespace {

	int32 gPass = 0, gFail = 0;

	void Cas(bool ok, const char *quoi) {
		if (ok) {
			gPass++;
			printf("  OK    %s\n", quoi);
		} else {
			gFail++;
			printf("  FAIL  %s\n", quoi);
		}
	}

	// LES 27 CLES REELLES, extraites des appels a `PaintPropGroup`.
	// Si un groupe s'ajoute demain sans etre repris ici, P3 ne le verra pas —
	// mais P1 continuera de tester ce qu'il y a, et c'est la propriete qui compte.
	const char *const kCles[] = {
		"prop.g.view",	  "prop.g.unit",	"prop.g.amb",	  "prop.g.floor",
		"prop.g.fog",	  "prop.g.ssao",	"prop.g.gi",	  "prop.g.postfx",
		"prop.g.shadow",  "prop.g.matp",	"prop.g.out",	  "prop.g.outdst",
		"prop.g.outmod",  "prop.g.outaid",	"prop.g.outvid",  "prop.g.outins",
		"prop.g.xform",	  "prop.g.dim",		"prop.g.rel",	  "prop.g.lit",
		"prop.g.cam",	  "prop.g.edsel",	"prop.g.edtools", "prop.g.edgeo",
		"prop.g.tool",	  "prop.g.edt",		"prop.g.mod",
	};
	const int32 kN = (int32)(sizeof(kCles) / sizeof(kCles[0]));

	// ── LE ZERO, AVANT TOUT LE RESTE ────────────────────────────────────────
	void Test_Zero() {
		printf("\n-- LE ZERO DU COMPTEUR --\n");
		NkFoldTable t;
		Cas(t.count == 0 && t.refuses == 0, "Z0 table neuve : 0 groupe connu, 0 refus");
		Cas(t.EstPlie("prop.g.jamais.vu", false) == false,
			"Z1 groupe inconnu, defaut DEPLIE -> rend deplie");
		Cas(t.EstPlie("prop.g.jamais.vu", true) == true,
			"Z2 groupe inconnu, defaut PLIE -> rend plie SANS aucun clic");
		Cas(t.NbDeplies(true) == 0, "Z3 aucun groupe connu, defaut plie -> 0 deplie");
	}

	// ── P3 : les cles sont distinctes (sinon P1 ne voudrait rien dire) ──────
	void Test_ClesDistinctes() {
		printf("\n-- LES CLES SONT-ELLES DISTINCTES ? --\n");
		bool toutes = true;
		for (int32 i = 0; i < kN && toutes; ++i)
			for (int32 j = i + 1; j < kN; ++j)
				if (NkFoldTable::Eq(kCles[i], kCles[j])) {
					printf("        doublon : %s == %s\n", kCles[i], kCles[j]);
					toutes = false;
					break;
				}
		char q[96];
		snprintf(q, sizeof(q), "P3 les %d cles du modeleur sont distinctes deux a deux", (int)kN);
		Cas(toutes, q);
	}

	// ── P1 : LE CRITERE QUI COMPTE ──────────────────────────────────────────
	// Pour chaque cle : la plier, puis exiger que les 26 autres soient EXACTEMENT
	// dans l'etat ou elles etaient. C'est ce critere, et lui seul, qui attrape le
	// defaut mesure (plier ssao pliait gi, postfx, shadow et xform).
	void Test_ChacunSePlieSeul() {
		printf("\n-- CHACUN SE PLIE SEUL (le critere du lot) --\n");
		int32 fautes = 0, comparaisons = 0;
		for (int32 i = 0; i < kN; ++i) {
			NkFoldTable t;
			// On INTERNE tout le monde d'abord : sans ca, « l'etat des autres »
			// serait le defaut et non un etat reellement enregistre, et le test
			// serait vrai pour une mauvaise raison.
			for (int32 k = 0; k < kN; ++k)
				t.Poser(kCles[k], false);
			bool avant[64];
			for (int32 k = 0; k < kN; ++k)
				avant[k] = t.EstPlie(kCles[k], false);

			t.Basculer(kCles[i], false);

			if (!t.EstPlie(kCles[i], false)) {
				printf("        %s n'a PAS ete plie\n", kCles[i]);
				++fautes;
			}
			for (int32 k = 0; k < kN; ++k) {
				if (k == i)
					continue;
				++comparaisons;
				if (t.EstPlie(kCles[k], false) != avant[k]) {
					printf("        plier %-16s a change %s\n", kCles[i], kCles[k]);
					++fautes;
				}
			}
		}
		char q[140];
		snprintf(q, sizeof(q),
				 "P1 %d x %d = %d comparaisons : plier un groupe n'en change aucun autre (%d faute(s))",
				 (int)kN, (int)(kN - 1), (int)comparaisons, (int)fautes);
		Cas(fautes == 0, q);
	}

	// ── P2 : plier puis replier rend l'etat de depart ───────────────────────
	// C'est lui qui rattrape une bascule qui n'ecrirait RIEN : P1 serait vert
	// (aucun groupe ne bougeant, aucun n'en entraine un autre) et pourtant le
	// pliage serait mort. Les deux criteres ne sont donc pas redondants.
	void Test_BasculerEstSonInverse() {
		printf("\n-- PLIER PUIS REPLIER REND L'ETAT DE DEPART --\n");
		int32 fautes = 0;
		for (int32 i = 0; i < kN; ++i) {
			NkFoldTable t;
			for (int32 k = 0; k < kN; ++k)
				t.Poser(kCles[k], false);
			const bool avant = t.EstPlie(kCles[i], false);
			t.Basculer(kCles[i], false);
			const bool milieu = t.EstPlie(kCles[i], false);
			t.Basculer(kCles[i], false);
			const bool apres = t.EstPlie(kCles[i], false);
			if (milieu == avant) {
				printf("        %s : la bascule n'a rien change\n", kCles[i]);
				++fautes;
			}
			if (apres != avant) {
				printf("        %s : deux bascules ne rendent pas l'etat de depart\n", kCles[i]);
				++fautes;
			}
		}
		char q[120];
		snprintf(q, sizeof(q), "P2 sur %d groupes : la bascule agit ET est son propre inverse (%d faute(s))",
				 (int)kN, (int)fautes);
		Cas(fautes == 0, q);
	}

	// ── LE DEFAUT « PLIE » ET SON PREMIER CLIC ──────────────────────────────
	void Test_PlieParDefaut() {
		printf("\n-- PLIE PAR DEFAUT (la demande de Rodolf) --\n");
		NkFoldTable t;
		// UN SEUL CAS POUR LES 27, et non 27 cas dont 26 sans libelle : un banc qui
		// imprime des lignes vides gonfle son compte sans rien dire de plus, et
		// « 13 OK » cesse alors de vouloir dire quelque chose. La boucle compte les
		// fautes et NOMME celles qu'elle trouve.
		int32 pasPlies = 0;
		for (int32 k = 0; k < kN; ++k)
			if (t.EstPlie(kCles[k], true) != true) {
				printf("        %s n'est PAS plie par defaut\n", kCles[k]);
				++pasPlies;
			}
		char qf[120];
		snprintf(qf, sizeof(qf), "F1 les %d groupes sont plies sans aucun clic (%d faute(s))",
				 (int)kN, (int)pasPlies);
		Cas(pasPlies == 0, qf);
		Cas(t.NbDeplies(true) == 0, "F2 le nombre de blocs DEPLIES au premier affichage vaut 0");
		// Premier clic sur un groupe PLIE par defaut : il doit se DEPLIER, et non
		// se plier une seconde fois. C'est la faute classique d'une bascule qui
		// part de l'etat stocke au lieu de l'etat EFFECTIF.
		t.Basculer(kCles[0], true);
		Cas(t.EstPlie(kCles[0], true) == false,
			"F3 premier clic sur un groupe plie par defaut -> il se DEPLIE");
		Cas(t.NbDeplies(true) == 1, "F4 et le compteur de deplies passe a exactement 1");
	}

	// ── LE DEPASSEMENT SE DIT, il n'est pas ignore ──────────────────────────
	void Test_Depassement() {
		printf("\n-- LE DEPASSEMENT EST BORNE ET SE COMPTE --\n");
		NkFoldTable t;
		char k[kFoldKeyCap];
		for (int32 i = 0; i < kFoldMaxGroups + 5; ++i) {
			snprintf(k, sizeof(k), "prop.g.g%04d", (int)i);
			t.Poser(k, true);
		}
		Cas(t.count == kFoldMaxGroups, "D1 la table sature a son plafond, sans le depasser");
		Cas(t.refuses == 5, "D2 les 5 refus sont COMPTES, pas ignores");
	}

} // namespace

int main() {
	printf("=== NKFoldTest : chaque groupe de proprietes se plie-t-il SEUL ? ===\n");
	printf("    (console, sans fenetre ; la regle est dans NkModelerFold.h)\n");
	Test_Zero();
	Test_ClesDistinctes();
	Test_ChacunSePlieSeul();
	Test_BasculerEstSonInverse();
	Test_PlieParDefaut();
	Test_Depassement();
	printf("\n=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	return gFail == 0 ? 0 : 1;
}
