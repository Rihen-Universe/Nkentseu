// =============================================================================
// Applications/NkMsaaContractCheck/src/main.cpp
// =============================================================================
// POURQUOI CE PROGRAMME EXISTE
// -----------------------------------------------------------------------------
// Une des neuf capacites annoncees et creuses : « le nombre d'echantillons
// impossible qui rend 0 sans erreur ».
//
// C'est une propriete d'EXECUTION. Aucun motif de texte ne la voit, et tout
// detecteur statique qui pretendrait la voir serait un generateur de faux
// positifs. Ce n'est donc pas une regle : c'est un BANC.
//
// LE DEFAUT MESURE, ET IL EST REEL
// -----------------------------------------------------------------------------
//   NkVulkanDevice::ToVkSamples() est un switch sur NkSampleCount dont le
//   `default:` rend VK_SAMPLE_COUNT_1_BIT.
//
//   NkSampleCount declare NK_S32 et NK_S64. Ils sont EXPRIMABLES et aucun
//   backend ne les honore. Et NkRendererConfig::msaaSamples est un uint32 nu :
//   un 3 venu d'un fichier de configuration devient
//   static_cast<NkSampleCount>(3) sans que rien ne proteste.
//
//   Dans les trois cas l'appelant obtient UN echantillon. Sans erreur, sans
//   journal, sans retour. Il croit avoir du MSAA et n'en a pas. C'est
//   « un repli qui preserve success n'est pas un repli, c'est un mensonge »
//   dans sa forme la plus dangereuse : le repli produit un resultat PLAUSIBLE.
//
// CE QUE CE BANC MESURE, ET CE QU'IL NE MESURE PAS
// -----------------------------------------------------------------------------
// COUVERT — le CONTRAT, sans peripherique, donc partout et par tout le monde :
//   NkDeviceCaps::SupportsSamples() rend la question DICIBLE avant qu'elle
//   n'atteigne un backend. Le banc verifie que ce contrat tient sur les SEIZE
//   combinaisons de drapeaux MSAA — pas sur trois exemples choisis.
//
// NON COUVERT ICI, MAIS PLUS NON COUVERT DU TOUT (2026-08-23)
//   La creation reelle d'une texture multi-echantillon demande un peripherique
//   GPU. Ce banc-ci ne la couvre toujours pas — mais NkMsaaDeviceCheck, en mode
//   `complet`, la couvre desormais, et il a TROUVE le defaut.
//   Mesure du 23/08 sur DX11 : demander 3, 7, 16, 32 ou 64 echantillons rend une
//   texture VALIDE. NkDirectX11Device.cpp:969-977 appelle
//   CheckMultisampleQualityLevels(), et quand la carte repond « impossible » il
//   pose SampleDesc.Count = 1 puis cree la texture QUAND MEME. Meme defaut que le
//   `default:` de ToVkSamples, dans un autre backend, et ecrit explicitement.
//   ⚠️ Ce que ce banc-la n'a PAS mesure : ToVkSamples lui-meme. Il retient le
//   premier peripherique disponible, et c'etait DX11. Le chemin Vulkan reste une
//   deduction par lecture, pas une mesure.
//   Ce banc-ci garde donc sa limite, et la dit — un banc qui tait sa limite est
//   pire qu'un banc absent : on croit la question reglee.
//
// POURQUOI DES RELATIONS ET PAS DES EXEMPLES
// -----------------------------------------------------------------------------
// « Un compte fige dans un controle qui mesure une collection qui grandit par
// conception est une dette a echeance. » Les quatre drapeaux MSAA ne grandissent
// pas par conception — mais la REGLE doit tenir quelles que soient leurs valeurs.
// Le banc enumere donc les 2^4 = 16 combinaisons et verifie une RELATION sur
// chacune. Un cinquieme drapeau ajoute sans `case` correspondant fait tomber le
// cas 4, il ne passe pas en silence.
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"

#include <cstdio>

using namespace nkentseu;

namespace {

	int32 gPass = 0;
	int32 gFail = 0;

	void Verifier(bool condition, const char *libelle) {
		if (condition) {
			++gPass;
			printf("  [OK]   %s\n", libelle);
		} else {
			++gFail;
			printf("  [FAIL] %s\n", libelle);
		}
	}

	// Les caps sont de la DONNEE PURE : on les fabrique, aucun peripherique n'est
	// cree. C'est ce qui permet a ce banc de tourner dans un arbre neuf, chez tout
	// agent, et sur une machine sans carte.
	NkDeviceCaps CapsAvec(bool m2, bool m4, bool m8, bool m16) {
		NkDeviceCaps c{};
		c.msaa2x = m2;
		c.msaa4x = m4;
		c.msaa8x = m8;
		c.msaa16x = m16;
		return c;
	}

	NkDeviceCaps CapsDeLaCombinaison(uint32 m) {
		return CapsAvec((m & 1u) != 0, (m & 2u) != 0, (m & 4u) != 0, (m & 8u) != 0);
	}

	NkSampleCount Brut(uint32 n) {
		return static_cast<NkSampleCount>(n);
	}

	const uint32 kNbCombinaisons = 16u; // 2^4 drapeaux MSAA : exhaustif, pas un echantillon

	// -------------------------------------------------------------------------
	// 1. Un echantillon est TOUJOURS honorable, meme sans aucun MSAA.
	//    Sinon un peripherique sans MSAA ne pourrait plus rien rendre du tout.
	bool CasUnEchantillonToujoursHonorable() {
		bool toutes = true;
		for (uint32 m = 0; m < kNbCombinaisons; ++m) {
			if (!CapsDeLaCombinaison(m).SupportsSamples(NkSampleCount::NK_S1)) {
				printf("         combinaison %u : 1 echantillon REFUSE\n", m);
				toutes = false;
			}
		}
		return toutes;
	}

	// -------------------------------------------------------------------------
	// 2. Une valeur qui n'est pas une puissance de deux est REFUSEE.
	//    C'est le cas du mandat : « demander 3 (ou 7) echantillons ». Le contrat
	//    doit dire NON quelles que soient les capacites de la carte.
	bool CasNonPuissanceDeDeuxRefusee() {
		const uint32 impossibles[] = {0u, 3u, 5u, 6u, 7u, 9u, 12u, 15u, 24u, 100u};
		const uint32 nb = (uint32)(sizeof(impossibles) / sizeof(impossibles[0]));
		bool toutes = true;
		for (uint32 m = 0; m < kNbCombinaisons; ++m) {
			NkDeviceCaps c = CapsDeLaCombinaison(m);
			for (uint32 k = 0; k < nb; ++k) {
				if (c.SupportsSamples(Brut(impossibles[k]))) {
					printf("         combinaison %u : %u echantillons ACCEPTES\n", m, impossibles[k]);
					toutes = false;
				}
			}
		}
		return toutes;
	}

	// -------------------------------------------------------------------------
	// 3. EXPRIMABLE n'est pas HONORABLE. NK_S32 et NK_S64 EXISTENT dans l'enum ;
	//    aucun backend ne les rend. Sans ce controle elles arrivent chez
	//    ToVkSamples(), qui les ramene a 1 en silence.
	bool CasExprimableNestPasHonorable() {
		bool toutes = true;
		for (uint32 m = 0; m < kNbCombinaisons; ++m) {
			NkDeviceCaps c = CapsDeLaCombinaison(m);
			if (c.SupportsSamples(NkSampleCount::NK_S32) || c.SupportsSamples(NkSampleCount::NK_S64)) {
				printf("         combinaison %u : NK_S32 ou NK_S64 ACCEPTE\n", m);
				toutes = false;
			}
		}
		return toutes;
	}

	// -------------------------------------------------------------------------
	// 4. Le contrat SUIT les drapeaux, il ne les devine pas.
	//    Relation exacte : SupportsSamples(n) <=> le drapeau de n, verifiee sur
	//    les 16 combinaisons. Un `case` oublie tombe ici.
	bool CasContratSuitLesDrapeaux() {
		bool toutes = true;
		for (uint32 m = 0; m < kNbCombinaisons; ++m) {
			const bool m2 = (m & 1u) != 0;
			const bool m4 = (m & 2u) != 0;
			const bool m8 = (m & 4u) != 0;
			const bool m16 = (m & 8u) != 0;
			NkDeviceCaps c = CapsAvec(m2, m4, m8, m16);
			if (c.SupportsSamples(NkSampleCount::NK_S2) != m2 || c.SupportsSamples(NkSampleCount::NK_S4) != m4 ||
				c.SupportsSamples(NkSampleCount::NK_S8) != m8 || c.SupportsSamples(NkSampleCount::NK_S16) != m16) {
				printf("         combinaison %u : reponse discordante avec les drapeaux\n", m);
				toutes = false;
			}
		}
		return toutes;
	}

	// -------------------------------------------------------------------------
	// 5. MaxSamples est le PLUS GRAND honorable, et il est honorable.
	//    Deux relations, pas deux nombres :
	//      (a) SupportsSamples(MaxSamples()) est vrai ;
	//      (b) aucune valeur strictement superieure n'est honorable.
	//    Elles restent vraies si un drapeau s'ajoute. Un nombre fige, non.
	bool CasMaxSamplesCoherent() {
		bool toutes = true;
		for (uint32 m = 0; m < kNbCombinaisons; ++m) {
			NkDeviceCaps c = CapsDeLaCombinaison(m);
			const uint32 mx = c.MaxSamples();
			if (!c.SupportsSamples(Brut(mx))) {
				printf("         combinaison %u : MaxSamples()=%u n'est pas honorable\n", m, mx);
				toutes = false;
			}
			for (uint32 n = mx + 1u; n <= 64u; ++n) {
				if (c.SupportsSamples(Brut(n))) {
					printf("         combinaison %u : %u > MaxSamples()=%u et pourtant honore\n", m, n, mx);
					toutes = false;
				}
			}
		}
		return toutes;
	}

} // namespace

int main() {
	printf("=== NkMsaaContractCheck — le contrat d'echantillonnage ===\n");
	printf("Aucun peripherique n'est cree : ce banc mesure une REGLE, pas une carte.\n");
	printf("%u combinaisons de drapeaux MSAA parcourues (2^4, exhaustif).\n\n", kNbCombinaisons);

	printf("-- 1. Un echantillon est toujours honorable --\n");
	Verifier(CasUnEchantillonToujoursHonorable(), "NK_S1 accepte sur les 16 combinaisons");

	printf("\n-- 2. Une valeur qui n'est pas une puissance de deux est refusee --\n");
	Verifier(CasNonPuissanceDeDeuxRefusee(),
			 "10 valeurs impossibles refusees sur les 16 combinaisons (dont 3 et 7)");

	printf("\n-- 3. EXPRIMABLE n'est pas HONORABLE : NK_S32 et NK_S64 --\n");
	Verifier(CasExprimableNestPasHonorable(),
			 "NK_S32 et NK_S64 refuses sur les 16 combinaisons (exprimables, jamais honorables)");

	printf("\n-- 4. Le contrat suit les drapeaux, il ne les devine pas --\n");
	Verifier(CasContratSuitLesDrapeaux(),
			 "SupportsSamples(2/4/8/16) == le drapeau correspondant, 16 sur 16");

	printf("\n-- 5. MaxSamples est le plus grand honorable, et il est honorable --\n");
	Verifier(CasMaxSamplesCoherent(),
			 "MaxSamples() honorable, et rien au-dessus ne l'est (16 combinaisons)");

	printf("\n-- 6. Le cas du mandat, ecrit tel qu'il a ete pose --\n");
	// « Demander 3 (ou 7) echantillons a NKRHI et exiger SOIT une ressource
	//   valide, SOIT une erreur — jamais un 0 muet. »
	// Sans peripherique on mesure la moitie CPU de cette exigence : la demande
	// est REFUSABLE, donc l'appelant peut savoir. Le refus est la reponse juste —
	// une carte qui ne sait pas faire 3 echantillons ne doit pas en faire 1 en
	// pretendant avoir obei.
	{
		NkDeviceCaps c = CapsAvec(true, true, true, true); // la carte la plus capable possible
		Verifier(!c.SupportsSamples(Brut(3u)), "3 echantillons refuses meme sur une carte qui fait 2/4/8/16");
		Verifier(!c.SupportsSamples(Brut(7u)), "7 echantillons refuses meme sur une carte qui fait 2/4/8/16");
		Verifier(c.SupportsSamples(NkSampleCount::NK_S4), "4 echantillons acceptes sur cette meme carte");
	}

	printf("\n=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	printf("\n[LIMITE] dite plutot que tue :\n");
	printf("   ce banc mesure le CONTRAT (CPU pur, aucun peripherique). Il ne mesure\n");
	printf("   PAS ce que le backend fait de la valeur demandee.\n");
	printf("   Ce chemin-la a maintenant son banc : NkMsaaDeviceCheck, mode complet.\n");
	printf("   Il a mesure le 23/08, sur DX11, que demander 3, 7, 16, 32 ou 64\n");
	printf("   echantillons rend une texture VALIDE, ramenee a 1 en silence\n");
	printf("   (NkDirectX11Device.cpp:969-977). Le contrat rend la chose EVITABLE ;\n");
	printf("   le defaut, lui, est toujours dans le backend.\n");
	printf("   Non mesure a ce jour : ToVkSamples lui-meme — ce banc-la a retenu DX11.\n");

	return gFail == 0 ? 0 : 1;
}
