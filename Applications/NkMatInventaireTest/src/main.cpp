// =============================================================================
// NkMatInventaireTest — rend un APERCU par fichier .nkmat, sans souris.
//
// POURQUOI CE BANC EXISTE
// -----------------------
// L'inventaire des materiaux demande, pour chaque materiau : ce qu'il devrait
// rendre, ce qu'il rend, l'ecart, une capture. La premiere passe s'est faite SUR
// PIECES (lecture des .nkmat) et a classe six materiaux sur dix comme des
// defauts jamais edites. Les quatre autres ne peuvent pas etre departages entre
// « juste » et « trahi par la chaine de rendu » sans les VOIR.
//
// CE BANC NE PILOTE PAS LA SOURIS. C'est sa raison d'etre : il se rejoue apres
// chaque correctif, il ne mobilise pas la machine de l'utilisateur, et il donne
// deux fois le meme resultat.
//
// ── LE REGIME QU'IL COUVRE, ET CELUI QU'IL NE COUVRE PAS ────────────────────
// L'apercu est rendu par le meme chemin que la vignette du panneau Materiaux :
// eclairage D'APERCU, pas celui de la scene. Un materiau juste en apercu et faux
// en scene RESTERAIT INVISIBLE ICI. Ce n'est pas un defaut du banc, c'est son
// perimetre — et il est ecrit DANS SA SORTIE (regime.txt), pas seulement ici :
// dans trois semaines ces PNG seront lus comme des captures de scene, et cette
// reserve aura disparu au moment ou elle protege.
//
// ── POURQUOI IL PASSE PAR L'HOTE DU VISEUR ─────────────────────────────────
// Charger un .nkmat n'a pas de forme autonome : NkAsMatRestore ECRIT dans l'hote
// du viewport (42 points d'entree Demo3DHostProjMat*). On aurait pu ecrire un
// second lecteur du format, plus leger — mais alors le banc rendrait ce QU'IL
// lit et non ce que le modeleur affiche, et il deviendrait aveugle exactement
// la ou l'inventaire a besoin de lui (un materiau juste que le modeleur affiche
// faux). Cf. la dette « charger un .nkmat ecrit dans l'hote » dans la ROADMAP.
//
// ── POURQUOI UNE FENETRE ───────────────────────────────────────────────────
// Il n'existe pas de creation de device sans surface : le device vient d'un
// renderer attache a une fenetre. On en ouvre donc une, et on la CACHE. C'est
// aussi pour ca que ce banc est une application et non un test unitest() :
// aucun des 26 dossiers tests/ du depot n'ouvre de device.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKLogger/NkLogger.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKImage/Core/NkImage.h"
#include "NKFileSystem/NkDirectory.h"

#include "NK3DModeler/Viewport/NkDemo3DHost.h"
#include "NK3DModeler/Project/NkModelerAssets.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace nkentseu;
// Les fonctions d'assets du modeleur (NkAsRead / NkAsNatureIs / NkAsMatRestore)
// vivent dans nk3d, comme l'hote du viseur.
using namespace nkentseu::nk3d;

namespace {

	// Taille de l'apercu. 256 : au-dela on ne lit rien de plus sur une sphere,
	// en deca les liserets de contour du type Toon disparaissent.
	constexpr int32 kPrev = 256;

	// FRAMES RENDUES AVANT LECTURE. Le depot a deja paye ce piege sur la sortie
	// d'image : redimensionner une cible et lire ses pixels ne peuvent pas avoir
	// lieu dans la meme image, le GPU doit avoir rendu entre les deux. On laisse
	// donc respirer, et ce nombre est ECRIT DANS LA SORTIE : le jour ou un PNG
	// sort noir, c'est le premier suspect, et sans lui quelqu'un accuserait le
	// materiau.
	constexpr int32 kFramesAvantLecture = 4;

	struct Trouve {
			std::string rel; // chemin relatif a la racine du projet
	};

	// RECURSIF, et ce n'est pas un detail : sur les dix materiaux d'AgentTest,
	// TROIS vivent dans des sous-dossiers (Bob/, Dossier/). Un balayage a plat
	// en manquerait un tiers — et le resultat aurait l'air complet.
	std::vector<Trouve> Balayer(const std::string &racine) {
		std::vector<Trouve> out;
		const NkVector<NkString> abs = NkDirectory::GetFiles(
			racine.c_str(), "*.nkmat", NkSearchOption::NK_ALL_DIRECTORIES);
		for (size_t i = 0; i < abs.size(); ++i) {
			std::string p = abs[i].CStr();
			for (size_t k = 0; k < p.size(); ++k)
				if (p[k] == '\\')
					p[k] = '/';
			std::string r = racine;
			for (size_t k = 0; k < r.size(); ++k)
				if (r[k] == '\\')
					r[k] = '/';
			// GetFiles rend des chemins COMPLETS ; NkAsRead attend un chemin
			// RELATIF a la racine du projet. On retranche donc le prefixe.
			Trouve t;
			t.rel = (p.size() > r.size() && p.compare(0, r.size(), r) == 0)
						? p.substr(r.size() + 1)
						: p;
			out.push_back(t);
		}
		return out;
	}

} // namespace

int main(int argc, char **argv) {
	NkLog::Instance().Info("[matinv] banc d'apercu des materiaux");

	// Racine du projet a inventorier. Par defaut le projet AgentTest, qui porte
	// les dix materiaux de l'inventaire.
	std::string racine = (argc > 1) ? argv[1]
									: "C:/Users/Rihen/NK3DModeler/AgentTest";
	std::string sortie = (argc > 2) ? argv[2] : "apercus_materiaux";

	std::vector<Trouve> mats = Balayer(racine);
	if (mats.empty()) {
		// On dit OU l'on a cherche : un « aucun materiau » sans sa racine est
		// une rumeur, pas un resultat.
		NkLog::Instance().Error("[matinv] aucun .nkmat sous « {0} »", racine.c_str());
		return 2;
	}
	NkLog::Instance().Info("[matinv] {0} materiaux sous « {1} »", (int32)mats.size(),
						   racine.c_str());

	// ── Fenetre CACHEE ──────────────────────────────────────────────────────
	NkWindowConfig wcfg;
	wcfg.title = "NkMatInventaireTest";
	wcfg.width = 640;
	wcfg.height = 480;
	wcfg.centered = true;
	wcfg.resizable = false;

	NkWindow window;
	if (!window.Create(wcfg)) {
		NkLog::Instance().Error("[matinv] creation de fenetre impossible");
		return 3;
	}
	window.SetVisible(false);

	NkSurfaceDesc surface = window.GetSurfaceDesc();
	NkDeviceInitInfo devInfo;
	devInfo.surface = surface;
	devInfo.width = (uint32)wcfg.width;
	devInfo.height = (uint32)wcfg.height;
	devInfo.context.vulkan.appName = "NkMatInventaireTest";
	devInfo.context.vulkan.engineName = "Nkentseu";
	devInfo.context.swapchainFormat = NkSwapchainFormat::NK_SWAPCHAIN_BGRA8_UNORM;

	NkIDevice *device = NkDeviceFactory::Create(devInfo);
	if (!device || !device->IsValid()) {
		NkLog::Instance().Error("[matinv] device indisponible");
		window.Close();
		return 4;
	}

	// ── L'hote du viseur : device + taille, rien d'autre ────────────────────
	// Ni projet, ni scene, ni navigateur : mesure faite avant d'ecrire ce banc.
	demo::Demo3DHostSetDevice(device);
	demo::Demo3DHostResize((uint32)kPrev, (uint32)kPrev);

	// ── Un emplacement par materiau, puis rendu et lecture ──────────────────
	std::vector<uint8> rgba((size_t)kPrev * kPrev * 4u);
	int32 rendus = 0, echecs = 0;

	for (size_t i = 0; i < mats.size(); ++i) {
		const Trouve &m = mats[i];

		NkArchive a;
		if (!NkAsRead(racine.c_str(), m.rel.c_str(), a)) {
			NkLog::Instance().Error("[matinv] illisible : {0}", m.rel.c_str());
			++echecs;
			continue;
		}
		// L'EN-TETE FAIT FOI : on ne lit pas un mesh de travers parce qu'il
		// porte la bonne extension.
		if (!NkAsNatureIs(a, "materiau")) {
			NkLog::Instance().Error("[matinv] nature != materiau : {0}", m.rel.c_str());
			++echecs;
			continue;
		}

		const int32 slot = demo::Demo3DHostProjMatCreate();
		if (slot < 0) {
			NkLog::Instance().Error("[matinv] plus d'emplacement pour {0}", m.rel.c_str());
			++echecs;
			continue;
		}
		int32 texMiss = 0;
		NkAsMatRestore(a, racine.c_str(), slot, &texMiss);
		if (texMiss > 0)
			NkLog::Instance().Warn("[matinv] {0} : {1} texture(s) manquante(s)",
								   m.rel.c_str(), texMiss);

		// Laisser le GPU rendre AVANT de lire : poser, laisser rendre, lire.
		for (int32 f = 0; f < kFramesAvantLecture; ++f)
			demo::Demo3DHostMatPreviewFrame(nullptr, slot, kPrev, kPrev);

		if (!demo::Demo3DHostProjMatPreviewTake(slot, rgba.data(), (uint32)kPrev,
												(uint32)kPrev)) {
			NkLog::Instance().Error("[matinv] lecture d'apercu refusee : {0}",
									m.rel.c_str());
			++echecs;
			continue;
		}

		// `LoadFromMemory` decode un FICHIER en memoire, pas des pixels bruts :
		// on alloue donc l'image puis on recopie les pixels lus du GPU.
		NkImage img;
		if (!img.Create((uint32)kPrev, (uint32)kPrev, math::NkColor(0, 0, 0, 255), 4)) {
			NkLog::Instance().Error("[matinv] allocation d'image refusee : {0}",
									m.rel.c_str());
			++echecs;
			continue;
		}
		std::memcpy(img.Pixels(), rgba.data(), rgba.size());
		char chemin[512];
		std::string plat = m.rel;
		for (size_t k = 0; k < plat.size(); ++k)
			if (plat[k] == '/' || plat[k] == '\\')
				plat[k] = '_';
		std::snprintf(chemin, sizeof(chemin), "%s/%s.png", sortie.c_str(), plat.c_str());
		if (img.SaveToFile(chemin)) {
			NkLog::Instance().Info("[matinv] {0} -> {1}", m.rel.c_str(), chemin);
			++rendus;
		} else {
			NkLog::Instance().Error("[matinv] ecriture refusee : {0}", chemin);
			++echecs;
		}
	}

	// ── LE REGIME EST ECRIT AVEC LES IMAGES, PAS DANS UN COMMENTAIRE ────────
	// Sans ce fichier, ces PNG seront lus dans trois semaines comme des captures
	// de scene, et la reserve qui les accompagne aura disparu au moment ou elle
	// protege.
	{
		char rp[512];
		std::snprintf(rp, sizeof(rp), "%s/regime.txt", sortie.c_str());
		if (FILE *f = std::fopen(rp, "w")) {
			std::fprintf(f, "REGIME DE CES CAPTURES — a lire avant de les interpreter\n");
			std::fprintf(f, "--------------------------------------------------------\n");
			std::fprintf(f, "rendu        : NkMatPreview3D, via l'hote du viseur\n");
			std::fprintf(f, "eclairage    : celui de l'APERCU, PAS celui de la scene\n");
			std::fprintf(f, "consequence  : un materiau juste en apercu et faux en scene\n");
			std::fprintf(f, "               n'apparait PAS ici. Ce banc ne couvre pas la\n");
			std::fprintf(f, "               famille « la chaine trahit un materiau juste ».\n");
			std::fprintf(f, "fenetre      : CACHEE (SetVisible(false))\n");
			std::fprintf(f, "frames avant lecture : %d\n", (int)kFramesAvantLecture);
			std::fprintf(f, "               (si un PNG sort noir ou incomplet, c'est le\n");
			std::fprintf(f, "                premier suspect — pas le materiau)\n");
			std::fprintf(f, "taille       : %dx%d\n", (int)kPrev, (int)kPrev);
			std::fprintf(f, "racine lue   : %s\n", racine.c_str());
			std::fprintf(f, "rendus       : %d\n", (int)rendus);
			std::fprintf(f, "echecs       : %d\n", (int)echecs);
			std::fclose(f);
		}
	}

	NkLog::Instance().Info("[matinv] {0} apercu(s) ecrit(s), {1} echec(s)", rendus, echecs);
	window.Close();
	return (echecs == 0) ? 0 : 1;
}
