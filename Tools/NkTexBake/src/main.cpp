// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkTexBake — LE FOUR
//
// Prend une image (PNG, JPEG, BMP, TGA, HDR, QOI, WebP…) et produit un actif
// `.nktex` : pixels DEJA dans la disposition que le GPU accepte, mipmaps
// precalculees, en-tete decrivant format, dimensions, espace colorimetrique et
// mode d'adressage. Le moteur le televerse alors sans decoder quoi que ce soit.
//
//   NkTexBake <entree> [-o sortie.nktex] [--nom /Textures/Bois]
//             [--usage couleur|donnee] [--cible <plateforme>]
//             [--sans-mips] [--adressage repeat|clamp|mirror]
//
// `--usage` est le reglage qui compte, et il n'a rien d'esthetique : une carte
// de COULEUR (albedo, emission) s'echantillonne en sRGB, une DONNEE (normale,
// rugosite, metallique, occlusion) doit rester lineaire — la deliner l'abime.
// Par defaut on devine depuis le nom du fichier, et on DIT ce qu'on a devine.
//
// ⚠️ `--cible` : ce que chaque plateforme accepterait, et ce que ce four sait
// faire aujourd'hui, ne sont pas la meme chose. Voir `--cible aide`.
// =============================================================================
#include "NKImage/Core/NkTextureOven.h"
#include "NKImage/Core/NkBlockCompress.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKSerialization/Asset/NkTextureAssetFormat.h"

#include <cstdio>
#include <cstring>

using namespace nkentseu;

namespace {

	// -------------------------------------------------------------------------
	// Ce que les cibles accepteraient, et ce que ce four livre.
	//
	// La colonne « accepte » est MESUREE dans le depot, pas devinee :
	//   - `NkDeviceCaps::textureCompressionBC` est mis a `true` en dur par
	//     NkDirectX11Device.cpp:1747 et NkDirectX12Device.cpp:3011, interroge
	//     par NkOpenglDevice.cpp:1230-1232 et par NkVulkanDevice.cpp:2470 ;
	//   - `textureCompressionETC2` et `textureCompressionASTC` ne sont
	//     renseignes QUE par le dorsal Vulkan (NkVulkanDevice.cpp:2471-2472).
	//     Aucun autre dorsal ne les annonce.
	//
	// La colonne « livre » est la meme partout, et c'est le point du lot : le
	// palier 1 donne le gain de DECODAGE (brut + mipmaps) ; la compression par
	// blocs donnera le gain de VRAM et n'est pas faite — `NkFormatBytesPerPixel`
	// rend 0 pour tous les formats par blocs, donc le RHI ne sait pas encore
	// televerser un seul d'entre eux.
	// -------------------------------------------------------------------------
	struct Cible {
			const char *nom;
			const char *accepterait;
			const char *pourquoi;
	};

	const Cible kCibles[] = {
		{"windows", "BC (BC1/BC3/BC5/BC7)", "DX11 et DX12 annoncent textureCompressionBC = true en dur"},
		{"linux", "BC si le pilote l'annonce", "OpenGL interroge l'extension, Vulkan interroge la fonctionnalite"},
		{"macos", "BC si Vulkan/MoltenVK l'annonce", "aucun dorsal du depot n'annonce ASTC hors Vulkan"},
		{"android", "ASTC / ETC2", "renseignes uniquement par le dorsal Vulkan"},
		{"ios", "ASTC si Vulkan l'annonce", "le dorsal Metal ne renseigne aucune de ces trois capacites"},
		{"web", "ETC2 / BC selon l'extension WebGL", "aucun dorsal du depot ne l'annonce"},
		{"harmonyos", "ASTC / ETC2 si Vulkan l'annonce", "meme dorsal que Android"},
	};

	void AfficherCibles() {
		std::printf("Cibles connues (les noms sont ceux de `jenga build --platform`) :\n\n");
		std::printf("  %-12s %-34s %s\n", "cible", "accepterait (mesure)", "d'ou vient la mesure");
		for (const Cible &c : kCibles)
			std::printf("  %-12s %-34s %s\n", c.nom, c.accepterait, c.pourquoi);
		std::printf("\n  CE QUE CE FOUR LIVRE, pour TOUTES les cibles : pixels bruts au format du\n"
					"  GPU + mipmaps precalculees. C'est le gain de DECODAGE, entier.\n"
					"  CE QU'IL NE FAIT PAS : la compression par blocs. Elle donnerait le gain de\n"
					"  VRAM, et elle n'est pas un travail de format mais un travail de RHI :\n"
					"  `NkFormatBytesPerPixel` rend 0 pour BC, ETC2 et ASTC, donc le pas de ligne\n"
					"  et la taille de televersement seraient nuls dans les quatre dorsaux.\n"
					"  Les codes sont deja reserves dans le format : le jour ou le RHI saura les\n"
					"  televerser, un actif ecrit aujourd'hui n'aura pas a changer de version.\n");
	}

	bool Contient(const char *s, const char *motif) {
		return s && motif && std::strstr(s, motif) != nullptr;
	}

	// Devine l'usage depuis le nom du fichier — et le DIT, parce qu'une
	// supposition silencieuse sur l'espace colorimetrique produit des normales
	// delinees que personne ne relie a leur cause.
	bool DevinerCouleur(const char *chemin, const char **raison) {
		static const char *kDonnees[] = {"normal", "_nrm", "rough", "metal", "_orm", "occlusion",
										 "_ao",	   "ao.",  "gloss", "spec",	 "height",	 "displac",
										 "bump",   "mask", "data"};
		for (const char *m : kDonnees) {
			if (Contient(chemin, m)) {
				*raison = m;
				return false;
			}
		}
		*raison = nullptr;
		return true;
	}

	void Usage() {
		std::printf("NkTexBake — le four a textures de Nkentseu\n\n"
					"  NkTexBake <entree> [options]\n\n"
					"  -o, --sortie <f>        fichier de sortie (defaut : entree + .nktex)\n"
					"      --nom <chemin>      chemin logique de l'actif (defaut : /Textures/<nom>)\n"
					"      --usage <u>         couleur | donnee   (defaut : devine depuis le nom)\n"
					"      --adressage <a>     repeat | clamp | mirror   (defaut : repeat)\n"
					"      --filtre <f>        aniso | lineaire | proche   (defaut : aniso, comme\n"
					"                          NkLoadOptions ; il ENTRE dans l'empreinte du cache)\n"
					"      --sans-mips         n'ecrit que le niveau 0\n"
					"      --cible <c>         plateforme visee ; `--cible aide` explique ce que\n"
					"                          chacune accepte et ce que ce four livre\n"
					"      --cache             ecrit dans Build/Cache/Assets/ sous l'empreinte que\n"
					"                          le MOTEUR calculera : c'est la pre-cuisson d'une\n"
					"                          distribution (le jeu n'a plus rien a cuire)\n"
					"      --dossier <d>       parcourt un dossier et cuit toutes ses images\n"
					"                          (implique --cache)\n"
					"      --forcer            recuit meme si l'actif existe deja dans le cache\n"
					"      --comparer <a> <b>  compare deux images (PSNR, ecart max) : pour juger\n"
					"                          l'IMAGE RENDUE, pas seulement que l'appel passe\n"
					"      --perte             ne cuit rien : compresse en BC1 et DIT la perte\n"
					"                          (PSNR et ecart max) — pour decider par la mesure\n"
					"                          plutot que par une regle generale\n"
					"  -h, --help\n");
	}

	// Extension reconnue comme image d'entree. On ne cuit pas un `.nktex` ni un
	// `.txt` trouve dans le dossier.
	bool EstImage(const char *nom) {
		static const char *kExt[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr",
									 ".qoi", ".gif", ".ppm",  ".pgm", ".webp"};
		if (!nom)
			return false;
		const char *point = std::strrchr(nom, '.');
		if (!point)
			return false;
		char bas[16] = {};
		nk_size i = 0;
		for (; i < 15u && point[i]; ++i)
			bas[i] = (point[i] >= 'A' && point[i] <= 'Z') ? char(point[i] - 'A' + 'a') : point[i];
		for (const char *e : kExt)
			if (std::strcmp(bas, e) == 0)
				return true;
		return false;
	}

	// Cuit une image et l'ecrit LA OU LE MOTEUR IRA LA CHERCHER.
	// Rend : 0 = ecrit, 1 = deja present (rien a faire), -1 = refuse.
	int CuireVersCache(const char *entree, const NkTexOvenReglages &reglages, bool forcer, bool bavard) {
		const nk_uint64 empreinte = NkTexCacheNommage::Empreinte(entree, reglages);
		if (empreinte == 0u) {
			std::printf("[NkTexBake] REFUS : source illisible (« %s »)\n", entree);
			return -1;
		}
		const NkString chemin = NkTexCacheNommage::Chemin(empreinte);
		if (!forcer && NkFile::Exists(chemin.CStr())) {
			if (bavard)
				std::printf("[NkTexBake] deja cuit : %s -> %s\n", entree, chemin.CStr());
			return 1;
		}

		NkVector<nk_uint8> payload;
		NkString err;
		if (!NkTextureOven::CuireFichier(entree, reglages, payload, &err)) {
			std::printf("[NkTexBake] REFUS : %s (« %s »)\n", err.CStr(), entree);
			return -1;
		}
		if (!NkDirectory::Exists(NkTexCacheNommage::Racine()) &&
			!NkDirectory::CreateRecursive(NkTexCacheNommage::Racine())) {
			std::printf("[NkTexBake] REFUS : impossible de creer %s\n", NkTexCacheNommage::Racine());
			return -1;
		}
		NkString logique = NkString("/Cache/");
		logique.Append(NkString::Fmtf("%016llX", (unsigned long long)empreinte).View());
		if (!NkEcrireActifTexture(payload.Data(), payload.Size(), chemin.CStr(), logique.View(),
								  NkStringView(entree), nullptr, &err)) {
			std::printf("[NkTexBake] REFUS a l'ecriture : %s\n", err.CStr());
			return -1;
		}
		if (bavard)
			std::printf("[NkTexBake] %s -> %s (%.2f Mo)\n", entree, chemin.CStr(),
						double(payload.Size()) / 1048576.0);
		return 0;
	}

} // namespace

int main(int argc, char **argv) {
	const char *entree = nullptr;
	const char *sortie = nullptr;
	const char *nomLogique = nullptr;
	const char *cible = nullptr;
	int usageCouleur = -1; // -1 = deviner
	bool sansMips = false;
	bool perteSeule = false;
	const char *comparerA = nullptr;
	const char *comparerB = nullptr;
	bool versCache = false;
	bool forcer = false;
	const char *dossier = nullptr;
	nk_uint32 adressage = NKTEXADDR_REPEAT;
	// Le defaut SUIT `NkLoadOptions::useAnisotropic == true`. Ce champ entre dans
	// l'empreinte : s'il divergeait, le moteur ne trouverait jamais ce que ce
	// four a pre-cuit, et rien ne le dirait.
	nk_uint32 filtre = NKTEXFILTER_ANISO;

	for (int i = 1; i < argc; ++i) {
		const char *a = argv[i];
		auto suivant = [&](void) -> const char * { return (i + 1 < argc) ? argv[++i] : nullptr; };

		if (std::strcmp(a, "-h") == 0 || std::strcmp(a, "--help") == 0) {
			Usage();
			return 0;
		} else if (std::strcmp(a, "-o") == 0 || std::strcmp(a, "--sortie") == 0) {
			sortie = suivant();
		} else if (std::strcmp(a, "--nom") == 0) {
			nomLogique = suivant();
		} else if (std::strcmp(a, "--usage") == 0) {
			const char *u = suivant();
			if (u && std::strcmp(u, "couleur") == 0)
				usageCouleur = 1;
			else if (u && std::strcmp(u, "donnee") == 0)
				usageCouleur = 0;
			else {
				std::printf("[NkTexBake] --usage attend « couleur » ou « donnee ».\n");
				return 2;
			}
		} else if (std::strcmp(a, "--adressage") == 0) {
			const char *m = suivant();
			if (m && std::strcmp(m, "repeat") == 0)
				adressage = NKTEXADDR_REPEAT;
			else if (m && std::strcmp(m, "clamp") == 0)
				adressage = NKTEXADDR_CLAMP;
			else if (m && std::strcmp(m, "mirror") == 0)
				adressage = NKTEXADDR_MIRROR;
			else {
				std::printf("[NkTexBake] --adressage attend « repeat », « clamp » ou « mirror ».\n");
				return 2;
			}
		} else if (std::strcmp(a, "--filtre") == 0) {
			const char *m = suivant();
			if (m && std::strcmp(m, "aniso") == 0)
				filtre = NKTEXFILTER_ANISO;
			else if (m && std::strcmp(m, "lineaire") == 0)
				filtre = NKTEXFILTER_LINEAR;
			else if (m && std::strcmp(m, "proche") == 0)
				filtre = NKTEXFILTER_NEAREST;
			else {
				std::printf("[NkTexBake] --filtre attend « aniso », « lineaire » ou « proche ».\n");
				return 2;
			}
		} else if (std::strcmp(a, "--sans-mips") == 0) {
			sansMips = true;
		} else if (std::strcmp(a, "--cache") == 0) {
			versCache = true;
		} else if (std::strcmp(a, "--comparer") == 0) {
			comparerA = suivant();
			comparerB = suivant();
		} else if (std::strcmp(a, "--perte") == 0) {
			perteSeule = true;
		} else if (std::strcmp(a, "--forcer") == 0) {
			forcer = true;
		} else if (std::strcmp(a, "--dossier") == 0) {
			dossier = suivant();
			versCache = true;
		} else if (std::strcmp(a, "--cible") == 0) {
			cible = suivant();
			if (cible && (std::strcmp(cible, "aide") == 0 || std::strcmp(cible, "?") == 0)) {
				AfficherCibles();
				return 0;
			}
		} else if (a[0] == '-') {
			std::printf("[NkTexBake] option inconnue : %s\n", a);
			Usage();
			return 2;
		} else if (!entree) {
			entree = a;
		} else {
			std::printf("[NkTexBake] une seule entree a la fois (recu « %s » puis « %s »).\n", entree, a);
			return 2;
		}
	}

	// ── Mode COMPARER ────────────────────────────────────────────────────────
	if (comparerA && comparerB) {
		NkImage ia, ib;
		if (!ia.Load(comparerA, 0) || !ib.Load(comparerB, 0)) {
			std::printf("[NkTexBake] image illisible\n");
			return 1;
		}
		NkImage ra = (ia.Format() != NkImagePixelFormat::NK_RGBA32) ? ia.Convert(NkImagePixelFormat::NK_RGBA32)
																	: ia.Copy();
		NkImage rb = (ib.Format() != NkImagePixelFormat::NK_RGBA32) ? ib.Convert(NkImagePixelFormat::NK_RGBA32)
																	: ib.Copy();
		if (!ra.IsValid() || !rb.IsValid() || ra.Width() != rb.Width() || ra.Height() != rb.Height()) {
			std::printf("[NkTexBake] dimensions differentes ou conversion impossible : %dx%d contre %dx%d\n",
						ia.Width(), ia.Height(), ib.Width(), ib.Height());
			return 1;
		}
		NkVector<nk_uint8> sa, sb;
		NkTextureOven::SerrerLignes(ra, sa);
		NkTextureOven::SerrerLignes(rb, sb);
		const float64 psnr = NkBlockCompress::PSNR(sa.Data(), sb.Data(), nk_uint32(ra.Width()), nk_uint32(ra.Height()));
		const nk_uint32 pire =
			NkBlockCompress::EcartMax(sa.Data(), sb.Data(), nk_uint32(ra.Width()), nk_uint32(ra.Height()));
		// Combien de pixels NOIRS de chaque cote : une texture refusee par le
		// pilote laisse du noir, et un PSNR seul ne le dirait pas assez fort.
		nk_uint64 noirsA = 0, noirsB = 0;
		for (nk_size i = 0; i < sa.Size(); i += 4) {
			if (sa[i] == 0 && sa[i + 1] == 0 && sa[i + 2] == 0)
				++noirsA;
			if (sb[i] == 0 && sb[i + 1] == 0 && sb[i + 2] == 0)
				++noirsB;
		}
		const float64 n = double(nk_size(ra.Width()) * nk_size(ra.Height()));
		std::printf("%s\n  contre %s\n  %dx%d  PSNR %6.2f dB  ecart max %3u  noirs %.1f%% contre %.1f%%\n",
					comparerA, comparerB, ra.Width(), ra.Height(), psnr, pire, 100.0 * double(noirsA) / n,
					100.0 * double(noirsB) / n);
		return 0;
	}

	if (!entree && !dossier) {
		Usage();
		return 2;
	}

	// ── Mode PERTE : mesurer avant de decider ────────────────────────────────
	// Une regle generale (« BC1 abime les normales ») est un souvenir de lecture.
	// Le chiffre sur LA carte qu'on a sous la main est une mesure. Cette option
	// existe pour que le defaut du moteur soit choisi par la seconde, pas par la
	// premiere.
	if (perteSeule) {
		if (!entree) {
			std::printf("[NkTexBake] --perte attend une image.\n");
			return 2;
		}
		NkImage img;
		if (!img.Load(entree, 0)) {
			std::printf("[NkTexBake] image illisible : %s\n", entree);
			return 1;
		}
		NkImage rgba;
		const NkImage *src = &img;
		if (img.Format() != NkImagePixelFormat::NK_RGBA32) {
			rgba = img.Convert(NkImagePixelFormat::NK_RGBA32);
			src = &rgba;
		}
		if (!src->IsValid()) {
			std::printf("[NkTexBake] conversion RGBA impossible : %s\n", entree);
			return 1;
		}
		NkVector<nk_uint8> serre;
		NkTextureOven::SerrerLignes(*src, serre);

		// L'alpha est-il UTILE ? BC1 n'en a pas : le dire est aussi important que
		// le PSNR, parce qu'une texture a alpha perdrait sa decoupe en silence.
		bool alphaUtile = false;
		for (nk_size i = 3; i < serre.Size() && !alphaUtile; i += 4)
			if (serre[i] != 255u)
				alphaUtile = true;

		NkVector<nk_uint8> blocs, decode;
		NkBlockCompress::EncoderBC1(serre.Data(), nk_uint32(src->Width()), nk_uint32(src->Height()), blocs);
		NkBlockCompress::DecoderBC1(blocs.Data(), nk_uint32(src->Width()), nk_uint32(src->Height()), decode);
		const float64 psnr =
			NkBlockCompress::PSNR(serre.Data(), decode.Data(), nk_uint32(src->Width()), nk_uint32(src->Height()));
		const nk_uint32 pire =
			NkBlockCompress::EcartMax(serre.Data(), decode.Data(), nk_uint32(src->Width()), nk_uint32(src->Height()));

		const bool normale = NkTextureOven::RessembleAUneNormale(*src);
		std::printf("%-52s %5dx%-5d %dc alpha=%-5s normale=%-3s PSNR %6.2f dB  ecart %3u  %7.2f -> %6.2f Mo\n",
					entree, src->Width(), src->Height(), img.Channels(), alphaUtile ? "UTILE" : "plein",
					normale ? "OUI" : "non", psnr, pire, double(serre.Size()) / 1048576.0,
					double(blocs.Size()) / 1048576.0);
		return 0;
	}

	// ── Mode DOSSIER : pre-cuisson d'une distribution ────────────────────────
	// Le jeu livre n'a alors plus rien a cuire au premier lancement — c'est la
	// difference entre « la premiere scene met une seconde de plus » et « elle
	// ne la met pas ». L'empreinte est celle que le moteur calculera : on ecrit
	// la ou il ira chercher.
	if (dossier) {
		NkVector<NkString> fichiers;
		if (!NkDirectory::Exists(dossier)) {
			std::printf("[NkTexBake] dossier introuvable : %s\n", dossier);
			return 1;
		}
		fichiers = NkDirectory::GetFiles(dossier, "*", NkSearchOption::NK_ALL_DIRECTORIES);

		int ecrits = 0, deja = 0, refuses = 0;
		for (nk_size i = 0; i < fichiers.Size(); ++i) {
			const NkString &f = fichiers[i];
			if (!EstImage(f.CStr()))
				continue;
			const char *raison = nullptr;
			NkTexOvenReglages r;
			r.sRGB = (usageCouleur >= 0) ? (usageCouleur == 1) : DevinerCouleur(f.CStr(), &raison);
			r.genererMips = !sansMips;
			r.addressMode = adressage;
			r.filterMode = filtre;
			const int e = CuireVersCache(f.CStr(), r, forcer, false);
			if (e == 0)
				++ecrits;
			else if (e == 1)
				++deja;
			else
				++refuses;
		}
		std::printf("[NkTexBake] dossier « %s » : %d cuit(s), %d deja present(s), %d refuse(s) -> %s\n", dossier,
					ecrits, deja, refuses, NkTexCacheNommage::Racine());
		// Un refus n'est pas une panne du four : un `.hdr` flottant n'est pas
		// encore cuisinable, et il est DIT ligne par ligne au-dessus.
		return 0;
	}

	// ── La cible : on DIT ce qu'elle changerait, et ce qu'elle ne change pas ──
	if (cible) {
		bool connue = false;
		for (const Cible &c : kCibles)
			if (std::strcmp(c.nom, cible) == 0)
				connue = true;
		if (!connue) {
			std::printf("[NkTexBake] cible inconnue « %s ». `--cible aide` liste celles que je connais.\n", cible);
			return 2;
		}
		std::printf("[NkTexBake] cible « %s » : la sortie est IDENTIQUE pour toutes les cibles "
					"aujourd'hui (brut + mipmaps). La compression par blocs, qui serait le seul "
					"ecart entre elles, n'est pas livree — `--cible aide` dit pourquoi.\n",
					cible);
	}

	// ── L'usage : couleur ou donnee ──
	bool couleur;
	if (usageCouleur >= 0) {
		couleur = (usageCouleur == 1);
	} else {
		const char *raison = nullptr;
		couleur = DevinerCouleur(entree, &raison);
		if (!couleur)
			std::printf("[NkTexBake] « %s » contient « %s » : traite comme une DONNEE (lineaire, pas de sRGB). "
						"Force avec --usage couleur si c'est faux.\n",
						entree, raison);
		else
			std::printf("[NkTexBake] « %s » traite comme une COULEUR (sRGB). Force avec --usage donnee si c'est "
						"faux.\n",
						entree);
	}

	// ── Cuisson ──
	NkTexOvenReglages reglages;
	reglages.sRGB = couleur;
	reglages.genererMips = !sansMips;
	reglages.addressMode = adressage;
	reglages.filterMode = filtre;

	if (versCache) {
		const int e = CuireVersCache(entree, reglages, forcer, true);
		return (e < 0) ? 1 : 0;
	}

	NkVector<nk_uint8> payload;
	NkString err;
	if (!NkTextureOven::CuireFichier(entree, reglages, payload, &err)) {
		std::printf("[NkTexBake] REFUS : %s (« %s »)\n", err.CStr(), entree);
		return 1;
	}

	NkTexVue vue;
	if (!NkTexturePayload::Decode(payload.Data(), payload.Size(), vue, &err)) {
		std::printf("[NkTexBake] REFUS : le payload produit est illisible (%s)\n", err.CStr());
		return 1;
	}

	// ── Chemins ──
	NkString cheminSortie;
	if (sortie) {
		cheminSortie = NkString(sortie);
	} else {
		cheminSortie = NkString(entree);
		const nk_size point = cheminSortie.RFind('.');
		if (point != NkString::npos)
			cheminSortie = cheminSortie.SubStr(0, point);
		cheminSortie.Append(".");
		cheminSortie.Append(NkAssetExtensionFor(NkAssetType::Texture2D));
	}

	NkString logique;
	if (nomLogique) {
		logique = NkString(nomLogique);
	} else {
		NkString base(entree);
		nk_size barre = base.RFind('/');
		const nk_size antiBarre = base.RFind('\\');
		if (antiBarre != NkString::npos && (barre == NkString::npos || antiBarre > barre))
			barre = antiBarre;
		if (barre != NkString::npos)
			base = base.SubStr(barre + 1);
		const nk_size point = base.RFind('.');
		if (point != NkString::npos)
			base = base.SubStr(0, point);
		logique = NkString("/Textures/");
		logique.Append(base.View());
	}

	NkAssetId id;
	if (!NkEcrireActifTexture(payload.Data(), payload.Size(), cheminSortie.CStr(), logique.View(),
							  NkStringView(entree), &id, &err)) {
		std::printf("[NkTexBake] REFUS a l'ecriture : %s\n", err.CStr());
		return 1;
	}

	const nk_uint64 pixels = NkTexturePayload::OctetsPixels(vue);
	std::printf("[NkTexBake] %s -> %s\n", entree, cheminSortie.CStr());
	std::printf("            %ux%u, format %s, %u niveau(x), %s\n", vue.width, vue.height,
				NkTexFormatNom(vue.formatCode), vue.mipCount, vue.EstSrgb() ? "sRGB" : "lineaire");
	std::printf("            %.2f Mo de pixels, actif %.2f Mo, id %s\n", double(pixels) / 1048576.0,
				double(payload.Size()) / 1048576.0, id.ToString().CStr());
	return 0;
}
