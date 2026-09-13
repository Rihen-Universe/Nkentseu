// =============================================================================
// Applications/NkMsaaVulkanCheck/src/main.cpp
// =============================================================================
// LE DERNIER MORCEAU DU MENSONGE : ToVkSamples() EXERCE, PAS DEDUIT
// -----------------------------------------------------------------------------
// Toute l'affaire du contrat d'echantillonnage part d'UNE ligne, lue et jamais
// exercee :
//
//     NkVulkanDevice.cpp:2787  ToVkSamples(NkSampleCount s)
//         case NK_S2/NK_S4/NK_S8/NK_S16 -> le bit correspondant
//         default:                      -> VK_SAMPLE_COUNT_1_BIT
//
//     NkVulkanDevice.cpp:1271  ici.samples = ToVkSamples(desc.samples);
//         C'est le SEUL traitement du nombre d'echantillons dans CreateTexture.
//         Aucun plafonnement, aucun refus, aucun journal avant ni apres.
//
// Deux bancs existaient, et ni l'un ni l'autre n'a jamais fait tourner ce code :
//   NkMsaaContractCheck  mesure le CONTRAT (CPU pur, aucun peripherique). Il
//     prouve que la question est REFUSABLE avant d'atteindre un backend. Il rend
//     le mensonge EVITABLE ; il ne le supprime pas.
//   NkMsaaDeviceCheck    mesure une VRAIE carte -- mais sa liste de backends est
//     une CHAINE DE REPLI, pas un balayage : DX11, puis DX12, puis OpenGL, et il
//     s'arrete au premier qui s'ouvre (break). Sur la machine de mesure DX11
//     s'ouvre. Vulkan n'a jamais ete parcouru. Le 23/08 il a trouve un defaut
//     REEL -- mais dans NkDirectX11Device.cpp, pas ici.
//
// CE BANC-CI N'A PAS DE CHAINE DE REPLI, ET C'EST DELIBERE
// -----------------------------------------------------------------------------
// Il ouvre Vulkan ou il rend IGNORE. Se replier sur un autre backend
// reproduirait trait pour trait le defaut qui a empeche la mesure pendant deux
// jours : on croirait avoir mesure Vulkan en ayant mesure DX11. Un banc qui
// mesure autre chose que ce qu'il annonce est pire qu'un banc absent.
//
// CE QU'IL MESURE, ET CE QU'IL NE MESURE PAS -- LA DISTINCTION EST LE TRAVAIL
// -----------------------------------------------------------------------------
// MESURE (observable, reproductible, sans supposition) :
//   pour chaque nombre d'echantillons n, sur un peripherique VULKAN,
//     attendu = caps.SupportsSamples(n)     (le contrat)
//     obtenu  = CreateTexture(samples=n).IsValid()   (la carte, via ToVkSamples)
//   L'exigence est obtenu == attendu. Un `obtenu=OUI` pour 3, 7, 32 ou 64 est LA
//   FORME OBSERVABLE DU MENSONGE : l'appelant recoit une ressource valide, sans
//   erreur, sans journal, et rien ne lui dit combien d'echantillons elle porte.
//
// NON MESURE, ET DIT PLUTOT QUE TU :
//   ce banc ne LIT PAS le VkImage cree. Il ne peut donc pas affirmer par la
//   mesure « la texture porte 1 echantillon au lieu de 32 ». Que ce soit le
//   `default:` de ToVkSamples qui l'ait ramenee a 1 est une LECTURE DU CODE
//   (une seule ligne, citee ci-dessus), pas une mesure de ce banc.
//   « Une supposition consignee comme mesure se propage avec l'autorite d'une
//   mesure » -- elle est donc consignee comme lecture, et etiquetee [LECTURE].
//
// LE DISCRIMINANT, ET POURQUOI IL EST UNE INFORMATION ET NON UN VERDICT
// -----------------------------------------------------------------------------
// Si la carte REFUSE une valeur que ToVkSamples HONORE (2, 4, 8 ou 16 : le
// switch a un `case` pour chacune, le nombre atteint le pilote tel quel) et
// ACCEPTE 32 et 64, la comparaison des deux est parlante : le pilote sait dire
// non a une demande d'echantillonnage, et il n'a pas dit non a 32.
// ⚠️ Elle n'est PAS une preuve. Vulkan ne garantit pas que le masque
// framebufferColorSampleCounts soit monotone : rien dans la specification
// n'interdit formellement a un pilote de refuser 16 et d'accepter 32. Aucun
// pilote reel ne fait ca -- mais « aucun pilote reel » est une croyance, pas une
// mesure. Le discriminant est donc IMPRIME COMME INFORMATION, avec sa supposition
// NOMMEE, et il ne compte ni en OK ni en FAIL. C'est le meme dessin que D3 dans
// verif_capacites.sh : ca ne prouve rien, ca montre ou regarder.
//
// IGNORE EST UN TROISIEME ETAT (code 77)
// -----------------------------------------------------------------------------
//   0  le contrat dit la verite sur cette carte Vulkan
//   1  le contrat MENT -- voir les lignes [FAIL]
//   77 IGNORE : rien n'a pu etre mesure. NI succes, NI echec.
// Deux causes distinctes d'IGNORE, et elles sont NOMMEES separement :
//   (1) aucun peripherique Vulkan headless n'a pu etre cree ;
//   (2) le TEMOIN a 1 echantillon a ete refuse.
// Le temoin passe AVANT toute accusation, et pour la raison que ce chantier a
// payee : si une texture 64x64 RGBA8 render-target a UN echantillon ne peut pas
// etre creee, alors aucune ne le peut, les huit autres valeurs vont toutes
// rendre « le contrat PROMET, la carte REFUSE », et le banc produirait huit
// accusations pour zero mesure. « Un defaut injecte qui n'en est pas un fait
// croire que le controle est aveugle » -- le symetrique mord plus fort : avant
// de conclure que le contrat ment, prouver que la question POUVAIT etre posee.
// =============================================================================
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Core/NkIDevice.h"

#include <cstdio>

using namespace nkentseu;

namespace {

	// Code de sortie « rien mesure ». Il est DECLARE au verificateur dans
	// config/bancs.list (verdict « code+ignore:77 ») : le nombre n'est pas devine
	// par l'outil, il est une donnee de la ligne du banc.
	const int kIgnore = 77;

	int32 gPass = 0;
	int32 gFail = 0;

	NkSampleCount Brut(uint32 n) {
		return static_cast<NkSampleCount>(n);
	}

	// UNE SEULE fabrique de description, pour que le temoin et les cas ne
	// different QUE par le nombre d'echantillons -- sinon le temoin ne temoigne
	// de rien. (Identique a NkMsaaDeviceCheck : deux bancs qui comparent leurs
	// resultats doivent demander la meme chose.)
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
		d.debugName = "NkMsaaVulkanCheck";
		return d;
	}

	bool Obtenir(NkIDevice *dev, uint32 n) {
		NkTextureDesc d = Decrire(n);
		NkTextureHandle h = dev->CreateTexture(d);
		const bool obtenu = h.IsValid();
		if (obtenu)
			dev->DestroyTexture(h);
		return obtenu;
	}

	// Les 9 valeurs mesurees, et ce que chacune est.
	//   1        le temoin, remesure ici pour qu'il compte dans le tableau
	//   2 4 8 16 EXPRIMABLES, et ToVkSamples a un `case` pour chacune : le nombre
	//            atteint le pilote tel quel. Honorables si le drapeau est la.
	//   3 7      INEXPRIMABLES -- absentes de l'enum, atteignables par un cast
	//            depuis un entier de configuration (NkRendererConfig::msaaSamples
	//            est un uint32 nu). Elles tombent dans le `default:`.
	//   32 64    EXPRIMABLES (NK_S32, NK_S64) et JAMAIS honorables. Elles tombent
	//            dans le `default:` elles aussi : c'est la paire qui distingue
	//            EXPRIMABLE de HONORABLE.
	const uint32 kValeurs[] = {1u, 2u, 3u, 4u, 7u, 8u, 16u, 32u, 64u};
	const uint32 kNbValeurs = (uint32)(sizeof(kValeurs) / sizeof(kValeurs[0]));

	// Les valeurs pour lesquelles ToVkSamples a un `case` explicite. Ce n'est pas
	// une heuristique : c'est la lecture du switch, et le discriminant en depend.
	bool CasExpliciteDansToVkSamples(uint32 n) {
		return n == 2u || n == 4u || n == 8u || n == 16u;
	}

	// Resultat par valeur, garde pour le discriminant.
	// ⚠️ Il n'y a PAS de tableau gAttendu a cote : la premiere version en avait un,
	// ecrit a chaque mesure et lu par personne. C'est exactement « un champ qu'on
	// regle et qui ne fait rien » -- le defaut que ce banc existe pour traquer,
	// dans le banc lui-meme. Retire.
	bool gObtenu[kNbValeurs];

	void Mesurer(NkIDevice *dev, uint32 i) {
		const uint32 n = kValeurs[i];
		const NkDeviceCaps &caps = dev->GetCaps();
		const bool attendu = caps.SupportsSamples(Brut(n));
		const bool obtenu = Obtenir(dev, n);
		gObtenu[i] = obtenu;

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
			if (!CasExpliciteDansToVkSamples(n)) {
				printf("         [LECTURE] %u n'a pas de `case` dans ToVkSamples : il tombe dans\n", n);
				printf("                   le `default:` (NkVulkanDevice.cpp:2787), seul traitement\n");
				printf("                   du nombre d'echantillons de CreateTexture (ligne 1271).\n");
				printf("                   Ceci est une lecture du code, PAS une mesure de ce banc :\n");
				printf("                   il ne relit pas le VkImage cree.\n");
			}
		} else {
			printf("  [FAIL] %2u echantillons : le contrat PROMET, la carte REFUSE.\n", n);
			printf("         Le contrat est trop optimiste -- il annonce ce qui n'existe pas.\n");
		}
	}

	// ---- LE DISCRIMINANT : information, jamais verdict -----------------------
	void Discriminant() {
		printf("\n-- discriminant (INFORMATION, ne compte ni en OK ni en FAIL) --\n");

		// (a) une valeur qui a un `case` et que la carte a REFUSEE
		int refuseeAvecCase = -1;
		for (uint32 i = 0; i < kNbValeurs; ++i) {
			if (CasExpliciteDansToVkSamples(kValeurs[i]) && !gObtenu[i]) {
				refuseeAvecCase = (int)i;
				break;
			}
		}
		// (b) 32 et 64 acceptees
		bool grandesAcceptees = true;
		for (uint32 i = 0; i < kNbValeurs; ++i)
			if ((kValeurs[i] == 32u || kValeurs[i] == 64u) && !gObtenu[i])
				grandesAcceptees = false;

		if (refuseeAvecCase < 0) {
			printf("  NON CONCLUANT : cette carte n'a refuse aucune des valeurs que\n");
			printf("  ToVkSamples honore (2, 4, 8, 16). Sans un refus a comparer, la\n");
			printf("  comparaison n'a rien a dire. Ce n'est pas un succes.\n");
			return;
		}
		if (!grandesAcceptees) {
			printf("  NON CONCLUANT : 32 ou 64 a ete refusee. Le rapprochement ne se pose pas.\n");
			return;
		}
		printf("  Cette carte a REFUSE %u echantillons -- une valeur que ToVkSamples\n", kValeurs[refuseeAvecCase]);
		printf("  transmet telle quelle au pilote -- et a ACCEPTE 32 et 64.\n");
		printf("  Le pilote sait donc dire non a une demande d'echantillonnage, et il n'a\n");
		printf("  pas dit non a 32.\n");
		printf("  !! CE N'EST PAS UNE PREUVE. Vulkan ne garantit pas que le masque\n");
		printf("  framebufferColorSampleCounts soit monotone : rien dans la specification\n");
		printf("  n'interdit formellement de refuser 16 et d'accepter 32. Aucun pilote reel\n");
		printf("  ne fait cela -- mais 'aucun pilote reel' est une croyance, pas une\n");
		printf("  mesure. D'ou : information, pas verdict.\n");
	}

	// Vulkan et RIEN D'AUTRE. Pas de chaine de repli : voir l'en-tete.
	NkIDevice *OuvrirVulkan() {
		NkDeviceInitInfo di;
		di.api = NkGraphicsApi::NK_GFX_API_VULKAN; // pas de di.surface -> headless
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
	printf("=== NkMsaaVulkanCheck -- ToVkSamples() exerce sur une vraie carte Vulkan ===\n");
	printf("Aucune chaine de repli : ce banc ouvre Vulkan ou il ne mesure rien.\n");
	fflush(stdout);

	printf("  ... tentative %s\n", NkGraphicsApiName(NkGraphicsApi::NK_GFX_API_VULKAN));
	fflush(stdout);
	NkIDevice *dev = OuvrirVulkan();

	// ---- IGNORE (1) : pas de Vulkan sur cette machine ------------------------
	if (dev == nullptr) {
		printf("\n[IGNORE] aucun peripherique VULKAN headless n'a pu etre cree sur cette machine.\n");
		printf("         Causes possibles, et ce banc ne sait pas laquelle : pas de pilote\n");
		printf("         Vulkan, pas de SDK au moment de la construction\n");
		printf("         (NKENTSEU_ENABLE_VULKAN_BACKEND=0), ou refus de l'instance.\n");
		printf("         Ce banc n'a RIEN mesure. Ce n'est pas un succes.\n");
		printf("         !! ET CE N'EST PAS NON PLUS UN REPLI : essayer DX11 ici ferait croire\n");
		printf("         Vulkan mesure alors qu'on aurait mesure autre chose. C'est le defaut\n");
		printf("         exact qui a laisse ToVkSamples non mesure pendant deux jours.\n");
		printf("         Code de sortie %d : le verificateur le compte IGNORE, distinct de OK.\n", kIgnore);
		return kIgnore;
	}

	// ---- TEMOIN D IDENTITE DU BACKEND, AVANT TOUTE MESURE --------------------
	// DEFAUT DE CE BANC, TROUVE LE 27/08 SUR SIGNALEMENT DU CHANTIER DE
	// CARTOGRAPHIE. Il imprimait le nom de l API DEMANDEE, pas de celle OBTENUE.
	// C etait vrai par accident : NkDeviceFactory::Create honore bien init.api,
	// et CreateForApi ne se replie JAMAIS (switch, une classe par cas, nullptr
	// sinon). Mais un banc qui AFFIRME son backend sans le LIRE affirmerait
	// « Vulkan » le jour ou un repli apparaitrait, en mesurant autre chose.
	// « Un banc qui mesure autre chose que ce qu il annonce est pire qu un banc
	// absent. » Il le LIT maintenant, et il REFUSE de mesurer si ca ne
	// correspond pas -- IGNORE, pas ECHEC : ce n est pas un defaut du contrat.
	const NkGraphicsApi obtenue = dev->GetApi();
	if (obtenue != NkGraphicsApi::NK_GFX_API_VULKAN) {
		printf("\n[IGNORE] j ai demande Vulkan et j ai obtenu %s.\n", NkGraphicsApiName(obtenue));
		printf("         Ce banc ne mesure QUE ToVkSamples ; sur un autre backend il\n");
		printf("         mesurerait autre chose en pretendant mesurer Vulkan.\n");
		printf("         Il se tait. Code %d, IGNORE -- ni succes, ni echec.\n", kIgnore);
		NkDeviceFactory::Destroy(dev);
		return kIgnore;
	}
	printf("\n--- peripherique : %s (LU sur le device, pas suppose) ---\n", NkGraphicsApiName(obtenue));
	const NkDeviceCaps &caps = dev->GetCaps();
	printf("    drapeaux MSAA de cette carte : 2x=%s 4x=%s 8x=%s 16x=%s   MaxSamples()=%u\n",
		   caps.msaa2x ? "oui" : "non", caps.msaa4x ? "oui" : "non", caps.msaa8x ? "oui" : "non",
		   caps.msaa16x ? "oui" : "non", caps.MaxSamples());

	// ---- LE TEMOIN, AVANT TOUTE ACCUSATION -----------------------------------
	printf("\n-- temoin : une texture render-target 64x64 RGBA8 a 1 echantillon --\n");
	if (!Obtenir(dev, 1u)) {
		printf("\n[IGNORE] le temoin a 1 echantillon a ete REFUSE par ce peripherique Vulkan.\n");
		printf("         Une texture render-target 64x64 RGBA8 sans MSAA est la demande la\n");
		printf("         plus banale qui soit. Si elle echoue, les huit autres valeurs vont\n");
		printf("         echouer aussi, et ce banc produirait huit accusations du type\n");
		printf("         'le contrat PROMET, la carte REFUSE' pour zero mesure du MSAA.\n");
		printf("         Ce n'est donc PAS un verdict sur le contrat. Code %d, IGNORE.\n", kIgnore);
		printf("         A REGARDER quand meme : un peripherique existe et refuse cette\n");
		printf("         texture-la. C'est anormal, mais ce banc n'est pas celui qui peut\n");
		printf("         dire pourquoi, et le supposer serait une mesure inventee.\n");
		NkDeviceFactory::Destroy(dev);
		return kIgnore;
	}
	printf("  [TEMOIN] obtenue. La question du MSAA est posable sur ce peripherique Vulkan.\n");

	printf("\n-- confrontation contrat / carte, par ToVkSamples --\n");
	for (uint32 i = 0; i < kNbValeurs; ++i)
		Mesurer(dev, i);

	Discriminant();

	NkDeviceFactory::Destroy(dev);

	printf("\n=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	printf("\n[LIMITE] dite plutot que tue :\n");
	printf("   ce banc mesure ce que la carte REND (une ressource valide, ou rien). Il ne\n");
	printf("   relit pas le VkImage cree : il ne peut donc pas mesurer combien\n");
	printf("   d'echantillons la texture porte VRAIMENT. Que 3, 7, 32 et 64 soient ramenes\n");
	printf("   a 1 par le `default:` de ToVkSamples (NkVulkanDevice.cpp:2787, appele en\n");
	printf("   ligne 1271) est une LECTURE du code, marquee comme telle.\n");
	printf("   Ce qu'il mesure, en revanche, tient tout seul : sur une carte Vulkan,\n");
	printf("   demander un nombre d'echantillons que le contrat REFUSE rend une ressource\n");
	printf("   VALIDE, sans erreur ni journal. L'appelant ne peut pas savoir.\n");
	if (gFail == 0) {
		printf("\nSur CETTE carte et ce pilote uniquement. Un autre pilote peut mentir la ou\n");
		printf("celui-ci dit vrai : ce banc mesure une machine, pas une promesse portable.\n");
	}
	return gFail == 0 ? 0 : 1;
}
