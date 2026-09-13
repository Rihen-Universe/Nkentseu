// =============================================================================
// Applications/NkMsaaDeviceCheck/src/main.cpp
// =============================================================================
// LE CONTRAT DIT-IL LA VERITE SUR UNE VRAIE CARTE ? (mode complet)
// -----------------------------------------------------------------------------
// NkMsaaContractCheck mesure le CONTRAT sans peripherique : il prouve que
// demander 3 echantillons est REFUSABLE. Il ne prouve pas que le refus
// correspond a ce que la carte fait vraiment.
//
// Ce banc-ci ferme la moitie manquante. Pour chaque nombre d'echantillons :
//   attendu = caps.SupportsSamples(s)
//   obtenu  = CreateTexture(desc avec samples = s).IsValid()
//   exigence : obtenu == attendu
//
// LES DEUX DIRECTIONS D'ERREUR N'ONT PAS LE MEME SENS
// -----------------------------------------------------------------------------
//   attendu=NON, obtenu=OUI  -> LE MENSONGE VISE. La carte a accepte une valeur
//     que le contrat refuse : elle a tres probablement degrade en silence, et
//     l'appelant croit avoir du MSAA. C'est le defaut des neuf capacites.
//   attendu=OUI, obtenu=NON  -> le contrat est trop optimiste : il promet ce que
//     la carte refuse. Moins dangereux, faux quand meme.
// Les deux sont des echecs. Ils sont NOMMES separement.
//
// IGNORE N'EST PAS VERT, ET C'EST LA RAISON D'ETRE DU CODE 77
// -----------------------------------------------------------------------------
// Un banc qui exige un GPU sera saute sur une machine qui n'en a pas, ou dont le
// pilote refuse. UN BANC IGNORE QUI RAPPORTE VERT EST EXACTEMENT LE MENSONGE QUE
// CE CHANTIER TRAQUE : on croit avoir mesure.
//   0  le contrat dit la verite sur cette carte
//   1  le contrat MENT -- voir les lignes [FAIL]
//   77 IGNORE : rien n'a pu etre mesure. NI succes, NI echec. Le verificateur le
//      compte a part et le NOMME sur sa ligne de verdict.
//
// Et un precedent a ne pas suivre : NkGpuProbe rend 1 quand aucun device n'est
// disponible -- il confond " pas de carte " avec " la carte est cassee ". C'est
// le symetrique du meme defaut : un faux ECHEC au lieu d'un faux vert.
//
// LE TEMOIN, ET POURQUOI IL PASSE AVANT TOUTE ACCUSATION
// -----------------------------------------------------------------------------
// " Un defaut injecte qui n'en est pas un fait croire que le controle est
// aveugle. Avant de conclure qu'un controle ne voit rien, prouver que ce qu'on
// lui a montre etait visible. " Le symetrique s'applique ici, et il mord plus
// fort : avant de conclure que le contrat MENT, prouver que la question POUVAIT
// etre posee.
//
// Si une texture 64x64 RGBA8 render-target a UN SEUL echantillon ne peut pas
// etre creee sur ce peripherique, alors AUCUNE ne le peut, et les huit autres
// valeurs vont toutes rendre " le contrat PROMET, la carte REFUSE " -- huit
// accusations, zero mesure. Ce banc dirait " le contrat ment " alors qu'il n'a
// rien mesure du contrat. C'est " une supposition consignee comme mesure se
// propage avec l'autorite d'une mesure ", en huit exemplaires.
//
// Donc : le temoin a 1 echantillon tourne EN PREMIER, et son echec est un
// IGNORE, jamais un ECHEC. Un peripherique qui refuse la texture la plus banale
// qui soit ne permet pas de conclure sur le MSAA ; le banc le DIT et se tait sur
// le reste. Sa ligne [IGNORE] porte la raison, distincte de " aucune carte ".
// =============================================================================
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Core/NkIDevice.h"

#include <cstdio>

using namespace nkentseu;

namespace {

	// Code de sortie " rien mesure ". Il est DECLARE au verificateur dans
	// config/bancs.list (verdict " code+ignore:77 ") : le nombre n'est pas devine
	// par l'outil, il est une donnee de la ligne du banc. Un code special que
	// seul le banc connaitrait serait un accord tacite entre deux fichiers.
	const int kIgnore = 77;

	int32 gPass = 0;
	int32 gFail = 0;

	NkSampleCount Brut(uint32 n) {
		return static_cast<NkSampleCount>(n);
	}

	// La description mesuree. UNE SEULE fabrique, pour que le temoin et les cas
	// ne different QUE par le nombre d'echantillons -- sinon le temoin ne
	// temoigne de rien.
	NkTextureDesc Decrire(uint32 n) {
		NkTextureDesc d;
		d.type = NkTextureType::NK_TEX2D;
		d.format = NkGPUFormat::NK_RGBA8_UNORM;
		d.width = 64;
		d.height = 64;
		d.depth = 1;
		d.arrayLayers = 1;
		d.mipLevels = 1;
		d.samples = Brut(n);
		d.bindFlags = NkBindFlags::NK_RENDER_TARGET;
		d.usage = NkResourceUsage::NK_DEFAULT;
		d.debugName = "NkMsaaDeviceCheck";
		return d;
	}

	// Demande la texture, rend si elle a ete obtenue, et ne laisse rien derriere.
	bool Obtenir(NkIDevice *dev, uint32 n) {
		NkTextureDesc d = Decrire(n);
		NkTextureHandle h = dev->CreateTexture(d);
		const bool obtenu = h.IsValid();
		if (obtenu)
			dev->DestroyTexture(h);
		return obtenu;
	}

	// Une seule valeur mesuree : on demande, on regarde, on confronte au contrat.
	void Mesurer(NkIDevice *dev, uint32 n) {
		const NkDeviceCaps &caps = dev->GetCaps();
		const bool attendu = caps.SupportsSamples(Brut(n));
		const bool obtenu = Obtenir(dev, n);

		if (obtenu == attendu) {
			++gPass;
			printf("  [OK]   %2u echantillons : contrat=%s, carte=%s\n", n, attendu ? "OUI" : "NON",
				   obtenu ? "OUI" : "NON");
			return;
		}
		++gFail;
		if (!attendu && obtenu) {
			printf("  [FAIL] %2u echantillons : le contrat REFUSE, la carte ACCEPTE.\n", n);
			printf("         C'est le mensonge vise : la ressource existe, et rien ne dit\n");
			printf("         combien d'echantillons elle porte reellement.\n");
		} else {
			printf("  [FAIL] %2u echantillons : le contrat PROMET, la carte REFUSE.\n", n);
			printf("         Le contrat est trop optimiste -- il annonce ce qui n'existe pas.\n");
		}
	}

	NkIDevice *Ouvrir(NkGraphicsApi api) {
		NkDeviceInitInfo di;
		di.api = api; // pas de di.surface -> headless
		di.width = 0;
		di.height = 0;
		di.context.software.threading = true;

		NkIDevice *dev = NkDeviceFactory::Create(di);
		if (dev == nullptr)
			return nullptr;
		if (!dev->IsValid()) {
			NkDeviceFactory::Destroy(dev);
			return nullptr;
		}
		return dev;
	}

} // namespace

int main() {
	printf("=== NkMsaaDeviceCheck -- le contrat dit-il la verite sur cette carte ? ===\n");
	fflush(stdout);

	const NkGraphicsApi essais[] = {NkGraphicsApi::NK_GFX_API_DX11, NkGraphicsApi::NK_GFX_API_DX12,
									NkGraphicsApi::NK_GFX_API_OPENGL};
	NkIDevice *dev = nullptr;
	NkGraphicsApi retenue = NkGraphicsApi::NK_GFX_API_NONE;
	for (uint32 i = 0; i < sizeof(essais) / sizeof(essais[0]); ++i) {
		printf("  ... tentative %s\n", NkGraphicsApiName(essais[i]));
		fflush(stdout);
		dev = Ouvrir(essais[i]);
		if (dev != nullptr) {
			retenue = essais[i];
			break;
		}
	}

	// ---- IGNORE (1) : aucune carte -------------------------------------------
	if (dev == nullptr) {
		printf("\n[IGNORE] aucun peripherique headless n'a pu etre cree sur cette machine.\n");
		printf("         DX11, DX12 et OpenGL ont tous ete essayes.\n");
		printf("         Ce banc n'a RIEN mesure. Ce n'est pas un succes.\n");
		printf("         Code de sortie %d : le verificateur le compte IGNORE, distinct de OK,\n", kIgnore);
		printf("         et le nomme sur sa ligne de verdict. Un banc ignore qui rapporterait\n");
		printf("         vert ferait croire la question mesuree -- c'est le mensonge meme que\n");
		printf("         ce banc existe pour traquer.\n");
		return kIgnore;
	}

	// TEMOIN D IDENTITE : on LIT l API du device obtenu au lieu de croire celle
	// qu on a demandee. Meme correctif que dans NkMsaaVulkanCheck le 27/08 --
	// ici le banc BALAIE plusieurs backends, donc l ecart serait encore plus
	// facile a ne pas voir. La valeur LUE fait foi dans tout le rapport.
	const NkGraphicsApi obtenue = dev->GetApi();
	if (obtenue != retenue) {
		printf("\n[IGNORE] j ai retenu %s et le device dit %s.\n", NkGraphicsApiName(retenue), NkGraphicsApiName(obtenue));
		printf("         Un banc qui mesure autre chose que ce qu il annonce est pire\n");
		printf("         qu un banc absent. Il se tait. Code %d, IGNORE.\n", kIgnore);
		NkDeviceFactory::Destroy(dev);
		return kIgnore;
	}
	printf("\n--- peripherique : %s (LU sur le device, pas suppose) ---\n", NkGraphicsApiName(obtenue));
	const NkDeviceCaps &caps = dev->GetCaps();
	printf("    drapeaux MSAA de cette carte : 2x=%s 4x=%s 8x=%s 16x=%s   MaxSamples()=%u\n",
		   caps.msaa2x ? "oui" : "non", caps.msaa4x ? "oui" : "non", caps.msaa8x ? "oui" : "non",
		   caps.msaa16x ? "oui" : "non", caps.MaxSamples());

	// ---- LE TEMOIN, AVANT TOUTE ACCUSATION -----------------------------------
	// Le contrat rend TOUJOURS true pour 1 echantillon (" pas de MSAA : toujours
	// honorable "). Si la carte refuse meme celle-la, la question du MSAA ne peut
	// pas etre posee sur ce peripherique.
	printf("\n-- temoin : une texture render-target 64x64 RGBA8 a 1 echantillon --\n");
	if (!Obtenir(dev, 1u)) {
		printf("\n[IGNORE] le temoin a 1 echantillon a ete REFUSE par ce peripherique.\n");
		printf("         Une texture render-target 64x64 RGBA8 sans MSAA est la demande la\n");
		printf("         plus banale qui soit. Si elle echoue, les huit autres valeurs vont\n");
		printf("         echouer aussi, et ce banc produirait huit accusations du type\n");
		printf("         'le contrat PROMET, la carte REFUSE' pour zero mesure du MSAA.\n");
		printf("         Ce n'est donc PAS un verdict sur le contrat : le banc n'a rien\n");
		printf("         mesure de lui. Code %d, IGNORE.\n", kIgnore);
		printf("         A REGARDER quand meme : un peripherique existe et refuse cette\n");
		printf("         texture-la. C'est anormal, mais ce banc n'est pas celui qui peut\n");
		printf("         dire pourquoi, et le supposer serait une mesure inventee.\n");
		NkDeviceFactory::Destroy(dev);
		return kIgnore;
	}
	printf("  [TEMOIN] obtenue. La question du MSAA est posable sur ce peripherique.\n");

	printf("\n-- confrontation contrat / carte --\n");

	// Les valeurs EXPRIMABLES, plus deux INEXPRIMABLES (le cas du mandat) :
	//   1        le temoin, remesure ici pour qu'il compte dans le tableau
	//   2 4 8 16 exprimables ET honorables si le drapeau correspondant est la
	//   3 7      inexprimables -- absentes de l'enum, atteignables par un cast
	//            depuis un entier de configuration (NkRendererConfig::msaaSamples)
	//   32 64    EXPRIMABLES (NK_S32, NK_S64) et JAMAIS honorables : c'est la
	//            paire qui distingue EXPRIMABLE de HONORABLE
	const uint32 valeurs[] = {1u, 2u, 3u, 4u, 7u, 8u, 16u, 32u, 64u};
	for (uint32 i = 0; i < sizeof(valeurs) / sizeof(valeurs[0]); ++i)
		Mesurer(dev, valeurs[i]);

	NkDeviceFactory::Destroy(dev);

	printf("\n=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	if (gFail == 0) {
		printf("Le contrat dit la verite sur cette carte, pour les 9 valeurs mesurees.\n");
		printf("Sur CETTE carte et ce backend uniquement. Un autre pilote peut mentir la ou\n");
		printf("celui-ci dit vrai : ce banc mesure une machine, pas une promesse portable.\n");
		printf("C'est pour cela qu'il est en mode complet et pas en mode rapide.\n");
	}
	return gFail == 0 ? 0 : 1;
}
