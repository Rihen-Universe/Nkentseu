// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_texture_asset.cpp — LE TELEVERSEMENT SANS DECODAGE, EXECUTE
//
// Le banc de NKImage prouve le FORMAT (aller-retour, mipmaps, additivite, refus).
// Celui-ci prouve le CHARGEMENT : qu'un `.nktex` cuit arrive dans une texture du
// RHI, niveau par niveau, sans qu'aucun codec ne tourne et sans que le RHI
// regenere la chaine.
//
// ── POURQUOI LE DORSAL LOGICIEL, ET PAS LE GPU ───────────────────────────────
// `NkSoftwareDevice` est un vrai dorsal du RHI : il implemente `CreateTexture`
// et `WriteTextureRegion` comme les autres, et il garde ses niveaux en memoire
// (`NkSWTexture::mips`). On peut donc RELIRE ce qui a ete televerse et le
// comparer octet par octet au fichier — ce qu'aucun dorsal GPU ne permet sans
// une lecture arriere. Et ça n'occupe pas la carte.
//
// ⚠️ Ce que ce banc NE prouve PAS, et il faut le dire : que Vulkan, DX11, DX12,
// OpenGL et Metal acceptent le meme chemin. Ils partagent l'interface
// `WriteTextureRegion(..., mipLevel, ...)`, mais « ça compile » ne prouve rien
// dans ce depot. Le verdict sur un vrai GPU reste a prendre.
// =============================================================================
#include "NKImage/Core/NkTextureOven.h"
#include "NKRHI/Software/NkSoftwareDevice.h"
#include "NKRenderer/Core/NkTextureAsset.h"
#include "NKRenderer/Core/NkTextureCache.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKSerialization/Asset/NkTextureAssetFormat.h"

#include <cstdio>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	int g_echecs = 0;

	void Verdict(const char *nom, bool ok, const char *detail) {
		std::printf("[%s] %-52s %s\n", ok ? "VERT " : "ROUGE", nom, detail);
		if (!ok)
			++g_echecs;
	}

	NkImage ImageReference(int32 w, int32 h) {
		NkImage img = NkImage::Alloc(w, h, NkImagePixelFormat::NK_RGBA32);
		if (!img.IsValid())
			return img;
		for (int32 y = 0; y < h; ++y) {
			uint8 *r = img.RowPtr(y);
			for (int32 x = 0; x < w; ++x) {
				uint8 *p = r + usize(x) * 4;
				p[0] = uint8((x * 255) / (w - 1));
				p[1] = uint8((y * 255) / (h - 1));
				p[2] = uint8(((x ^ y) & 0x0F) * 17);
				p[3] = uint8(255 - ((x + y) & 0x3F));
			}
		}
		return img;
	}

} // namespace

int main() {
	std::printf("=============================================================\n");
	std::printf(" Actif cuit -> RHI : televersement SANS decodage\n");
	std::printf("=============================================================\n\n");

	char d[300];
	const char *cheminActif = "Build/nktex_temoin.nktex";
	const NkString logique("/Textures/TemoinCuit");

	// ── 1. Cuire, puis ECRIRE un vrai fichier d'actif (conteneur compris) ──
	NkImage ref = ImageReference(64, 48);
	NkVector<nk_uint8> payload;
	NkString err;
	NkTexOvenReglages reglages;
	reglages.sRGB = true;
	reglages.genererMips = true;
	if (!ref.IsValid() || !NkTextureOven::Cuire(ref, reglages, payload, &err)) {
		Verdict("preparation / cuisson", false, err.CStr());
		return 1;
	}
	if (!NkTextureAssetIO::SaveBaked(payload.Data(), payload.Size(), NkString(cheminActif), logique,
									 NkString("(procedural)"))) {
		Verdict("preparation / ecriture de l'actif", false, "SaveBaked a refuse");
		return 1;
	}
	std::snprintf(d, sizeof(d), "%s ecrit, payload %llu o", cheminActif, (unsigned long long)payload.Size());
	Verdict("preparation / actif .nktex ecrit sur disque", true, d);

	// La verite de reference : ce que le fichier contient.
	NkTexVue attendu;
	if (!NkTexturePayload::Decode(payload.Data(), payload.Size(), attendu, &err)) {
		Verdict("preparation / relecture du payload", false, err.CStr());
		return 1;
	}

	// ── 2. Un dorsal logiciel, et la bibliotheque de textures dessus ──
	NkSoftwareDevice dev;
	NkDeviceInitInfo init{};
	if (!dev.Initialize(init)) {
		Verdict("dorsal logiciel / initialisation", false, "NkSoftwareDevice::Initialize a echoue");
		return 1;
	}
	NkTextureLibrary lib;
	if (lib.Init(&dev, nullptr) != NkRResult::NK_OK) {
		Verdict("bibliotheque de textures / initialisation", false, "NkTextureLibrary::Init a echoue");
		return 1;
	}
	Verdict("dorsal logiciel / pret", true, "NkSoftwareDevice + NkTextureLibrary");

	// ── 3. Charger l'actif cuit ──
	NkTexHandle h = NkTextureAssetIO::LoadBaked(NkString(cheminActif), &lib);
	Verdict("chargement / LoadBaked rend une texture", h.IsValid(), h.IsValid() ? "handle valide" : "handle NUL");
	if (!h.IsValid()) {
		lib.Shutdown();
		dev.Shutdown();
		return 1;
	}

	// ── 4. Relire ce qui a ete televerse, niveau par niveau ──
	NkTextureHandle rhi = lib.GetRHIHandle(h);
	NkSWTexture *tex = dev.GetTex(rhi.id);
	if (!tex) {
		Verdict("televersement / texture presente dans le dorsal", false, "GetTex a rendu nullptr");
		lib.Shutdown();
		dev.Shutdown();
		return 1;
	}

	std::snprintf(d, sizeof(d), "%u niveaux dans le dorsal, %u dans le fichier", (unsigned)tex->mips.Size(),
				  attendu.mipCount);
	Verdict("televersement / la texture porte TOUS les niveaux", nk_size(tex->mips.Size()) == nk_size(attendu.mipCount),
			d);

	// Chaque niveau, octet par octet, contre le fichier. C'est ici que se juge
	// « televerse sans decodage » : si le RHI avait regenere la chaine lui-meme,
	// ou si un niveau n'avait pas ete ecrit, les octets differeraient.
	nk_uint64 differents = 0;
	nk_uint64 total = 0;
	nk_size niveauxCompares = 0;
	for (nk_size m = 0; m < attendu.levels.Size() && m < nk_size(tex->mips.Size()); ++m) {
		const NkTexNiveauVue &n = attendu.levels[m];
		const NkVector<uint8> &gpu = tex->mips[m];
		if (gpu.Size() < nk_size(n.size)) {
			differents += n.size;
			total += n.size;
			continue;
		}
		for (nk_uint32 k = 0; k < n.size; ++k) {
			if (gpu[k] != n.data[k])
				++differents;
			++total;
		}
		++niveauxCompares;
	}
	std::snprintf(d, sizeof(d), "%llu octet(s) different(s) sur %llu, %llu niveau(x) compare(s)",
				  (unsigned long long)differents, (unsigned long long)total, (unsigned long long)niveauxCompares);
	Verdict("televersement / chaque niveau est celui du fichier, au bit", differents == 0 && total > 0 &&
																			 niveauxCompares == attendu.levels.Size(),
			d);

	// ── 5. CONTRE-EPREUVE du repli : un actif qui ne porte PAS de pixels ──
	// Sans elle, un `LoadBaked` qui accepterait n'importe quoi passerait le
	// temoin 3 sans rien prouver.
	{
		NkTextureAsset reglagesSeuls;
		reglagesSeuls.sourceFilePath = NkString("Resources/NKRenderer/Textures/Defaults/test_pattern.png");
		reglagesSeuls.sRGB = true;
		const char *cheminReglages = "Build/nktex_temoin_reglages.nktex";
		NkAssetId id;
		const bool ecrit = NkTextureAssetIO::Save(reglagesSeuls, NkString(cheminReglages),
												  NkString("/Textures/TemoinReglages"), &id);
		if (!ecrit) {
			Verdict("repli / preparation", false, "impossible d'ecrire l'actif de reglages");
		} else {
			NkTexHandle rien = NkTextureAssetIO::LoadBaked(NkString(cheminReglages), &lib);
			std::snprintf(d, sizeof(d), "LoadBaked rend %s sur un actif de reglages",
						  rien.IsValid() ? "UNE TEXTURE (faux)" : "un handle nul (juste)");
			Verdict("repli / un actif sans pixels n'est PAS pris pour cuit", !rien.IsValid(), d);

			// Et `Load` doit alors retomber sur le codec, en le disant une fois.
			const nk_uint32 avant = NkTextureAssetIO::CompteRepliDit();
			NkTexHandle parCodec = NkTextureAssetIO::Load(NkString(cheminReglages), &lib);
			NkTexHandle parCodec2 = NkTextureAssetIO::Load(NkString(cheminReglages), &lib);
			std::snprintf(d, sizeof(d), "%s / %s", parCodec.IsValid() ? "1er chargement ok" : "1er chargement ECHOUE",
						  parCodec2.IsValid() ? "2e ok" : "2e ECHOUE");
			Verdict("repli / le codec prend le relais", parCodec.IsValid() && parCodec2.IsValid(), d);

			// « Dit une fois » : deux replis, UN seul message. Un journal ne prouve
			// rien a un banc — c'est le compteur qui le prouve.
			const nk_uint32 apres = NkTextureAssetIO::CompteRepliDit();
			std::snprintf(d, sizeof(d), "2 replis -> %u message(s) emis (attendu 1)", apres - avant);
			Verdict("repli / le message est dit UNE seule fois", (apres - avant) == 1u, d);
		}
	}

	// ── 6. Ce qui est REFUSE, et qui doit l'etre ──
	{
		// Un format par blocs : reserve dans le format, pas televersable par le
		// RHI aujourd'hui. Le refus doit venir d'ici, pas d'un pas de ligne nul
		// calcule trois couches plus bas.
		const NkGPUFormat f = NkTextureLibrary::FormatGpuDepuisCode(NKTEXFMT_BC7_UNORM);
		std::snprintf(d, sizeof(d), "BC7 -> %s", f == NkGPUFormat::NK_UNDEFINED ? "NK_UNDEFINED (refuse)" : "un format");
		Verdict("refus / un format par blocs est refuse a l'entree", f == NkGPUFormat::NK_UNDEFINED, d);

		const NkGPUFormat g = NkTextureLibrary::FormatGpuDepuisCode(NKTEXFMT_RGBA8_SRGB);
		std::snprintf(d, sizeof(d), "RGBA8_SRGB -> %s",
					  g == NkGPUFormat::NK_RGBA8_SRGB ? "NK_RGBA8_SRGB (accepte)" : "REFUSE (faux)");
		Verdict("refus / contre-epreuve : un format livre est accepte", g == NkGPUFormat::NK_RGBA8_SRGB, d);
	}

	// -- 7. DE BOUT EN BOUT, sur un vrai PNG du depot, par le chemin EXACT du
	//    four (`CuireFichier` + `NkEcrireActifTexture`) : c'est ce que fait
	//    `Tools/NkTexBake`, ligne pour ligne. Sans ce temoin, le banc ne
	//    prouverait que son propre aller-retour en memoire.
	{
		const char *png = "Resources/NKRenderer/Textures/Defaults/test_pattern.png";
		const char *actifPng = "Build/nktex_test_pattern.nktex";
		NkVector<nk_uint8> p2;
		NkString e2;
		NkTexOvenReglages r2;
		r2.sRGB = true;
		r2.genererMips = true;
		if (!NkTextureOven::CuireFichier(png, r2, p2, &e2)) {
			Verdict("bout en bout / cuisson d'un PNG du depot", false, e2.CStr());
		} else if (!NkEcrireActifTexture(p2.Data(), p2.Size(), actifPng, NkStringView("/Textures/TestPattern"),
										 NkStringView(png), nullptr, &e2)) {
			Verdict("bout en bout / ecriture de l'actif", false, e2.CStr());
		} else {
			NkTexHandle h2 = NkTextureAssetIO::LoadBaked(NkString(actifPng), &lib);
			NkTextureHandle rhi2 = lib.GetRHIHandle(h2);
			NkSWTexture *t2 = h2.IsValid() ? dev.GetTex(rhi2.id) : nullptr;

			// La verite : le PNG decode directement.
			NkImage direct;
			const bool decode = direct.Load(png, 0);
			NkImage rgba;
			const NkImage *att = &direct;
			if (decode && direct.Format() != NkImagePixelFormat::NK_RGBA32) {
				rgba = direct.Convert(NkImagePixelFormat::NK_RGBA32);
				att = &rgba;
			}

			nk_uint64 diff = 0;
			nk_uint64 tot = 0;
			if (t2 && decode && att->IsValid() && t2->mips.Size() > 0) {
				const nk_size pas = nk_size(att->Width()) * 4u;
				for (int32 y = 0; y < att->Height(); ++y) {
					const uint8 *a = att->RowPtr(y);
					const uint8 *b = t2->mips[0].Data() + nk_size(y) * pas;
					for (nk_size k = 0; k < pas; ++k) {
						if (a[k] != b[k])
							++diff;
						++tot;
					}
				}
			} else {
				diff = 1;
			}
			std::snprintf(d, sizeof(d), "%s : %llu octet(s) different(s) sur %llu du decodage direct", png,
						  (unsigned long long)diff, (unsigned long long)tot);
			Verdict("bout en bout / PNG -> four -> actif -> RHI, au bit", diff == 0 && tot > 0, d);
		}
	}

	// -- 7ter. ACCORD FOUR / MOTEUR ------------------------------------------
	//
	// 🔴 Le defaut le plus silencieux de tout ce lot serait ici : le four
	// (`Tools/NkTexBake --dossier`) pre-cuit sous une empreinte, le moteur en
	// cherche une autre, et **rien ne le signale** — chacun cherche un fichier
	// que l'autre n'ecrit pas. Le symptome serait « la pre-cuisson ne sert a
	// rien », jamais une erreur. Ce temoin est le garde-fou : les quatre champs
	// qui entrent dans l'empreinte doivent etre IDENTIQUES des deux cotes.
	{
		const NkTexOvenReglages moteur = NkTextureLibrary::ReglagesDepuisOptions(NkLoadOptions{});
		const NkTexOvenReglages four; // les defauts du four
		const bool accord = (moteur.sRGB == four.sRGB) && (moteur.genererMips == four.genererMips) &&
							(moteur.addressMode == four.addressMode) && (moteur.filterMode == four.filterMode);
		std::snprintf(d, sizeof(d), "moteur[srgb=%d mips=%d addr=%u filtre=%u] four[srgb=%d mips=%d addr=%u filtre=%u]",
					  moteur.sRGB ? 1 : 0, moteur.genererMips ? 1 : 0, moteur.addressMode, moteur.filterMode,
					  four.sRGB ? 1 : 0, four.genererMips ? 1 : 0, four.addressMode, four.filterMode);
		Verdict("accord four/moteur : memes reglages par defaut", accord, d);

		// Et la consequence, mesuree sur une empreinte reelle : les deux cotes
		// nomment le MEME fichier. Sans cette ligne, l'egalite des champs
		// pourrait etre vraie et le nommage quand meme diverger.
		const char *tmoin = "Resources/NKRenderer/Textures/Defaults/test_pattern.png";
		const nk_uint64 e1 = NkTexCacheNommage::Empreinte(tmoin, moteur);
		const nk_uint64 e2 = NkTexCacheNommage::Empreinte(tmoin, four);
		std::snprintf(d, sizeof(d), "%016llX vs %016llX", (unsigned long long)e1, (unsigned long long)e2);
		Verdict("accord four/moteur : meme fichier de cache vise", e1 != 0u && e1 == e2, d);

		// Contre-epreuve : un reglage VRAIMENT different doit donner un autre
		// nom. Sinon les deux lignes ci-dessus passeraient meme si l'empreinte
		// ignorait les reglages.
		NkTexOvenReglages autre = four;
		autre.filterMode = NKTEXFILTER_NEAREST;
		const nk_uint64 e3 = NkTexCacheNommage::Empreinte(tmoin, autre);
		std::snprintf(d, sizeof(d), "filtre change -> %016llX (etait %016llX)", (unsigned long long)e3,
					  (unsigned long long)e1);
		Verdict("accord four/moteur : contre-epreuve, un reglage different separe", e3 != e1 && e3 != 0u, d);
	}

	// -- 7bis. L'EMPREINTE PORTE LE CONTENU, pas seulement la taille ----------
	//
	// 🔴 Ce temoin existe parce que le precedent ne suffisait pas, et la lecon
	// vaut d'etre gardee : le temoin « source modifiee » du bloc 8 reste VERT
	// meme si on retire le contenu de l'empreinte — les deux PNG qu'il ecrit
	// n'ont pas la meme taille compressee, donc le seul terme de taille suffit
	// a les distinguer. La sonde n'exerçait pas le regime risque.
	//
	// Ici on l'exerce : deux fichiers de MEME longueur, un seul octet different.
	// C'est le cas qu'un cache par taille — ou par date — sert a tort.
	{
		const char *fa = "Build/nktex_empreinte_a.bin";
		const char *fb = "Build/nktex_empreinte_b.bin";
		nk_uint8 tampon[4096];
		for (int i = 0; i < 4096; ++i)
			tampon[i] = nk_uint8(i * 7);

		auto Ecrire = [](const char *chemin, const nk_uint8 *o, nk_size n) -> bool {
			std::FILE *f = std::fopen(chemin, "wb");
			if (!f)
				return false;
			const bool ok = (std::fwrite(o, 1, n, f) == n);
			std::fclose(f);
			return ok;
		};

		bool prets = Ecrire(fa, tampon, sizeof(tampon));
		tampon[2048] = nk_uint8(tampon[2048] ^ 0xFF); // UN octet, meme longueur
		prets = prets && Ecrire(fb, tampon, sizeof(tampon));

		if (!prets) {
			Verdict("empreinte / preparation", false, "ecriture des deux fichiers impossible");
		} else {
			NkTexOvenReglages r;
			const nk_uint64 ea = NkTextureCache::Empreinte(fa, r);
			const nk_uint64 eb = NkTextureCache::Empreinte(fb, r);
			std::snprintf(d, sizeof(d), "%016llX vs %016llX, meme longueur (4096 o), 1 octet different",
						  (unsigned long long)ea, (unsigned long long)eb);
			Verdict("empreinte / un seul octet change l'empreinte", ea != 0u && eb != 0u && ea != eb, d);

			// Contre-epreuve : le MEME fichier doit rendre la MEME empreinte.
			// Sans elle, une empreinte aleatoire passerait la ligne ci-dessus.
			const nk_uint64 ea2 = NkTextureCache::Empreinte(fa, r);
			std::snprintf(d, sizeof(d), "%016llX relu %016llX", (unsigned long long)ea, (unsigned long long)ea2);
			Verdict("empreinte / contre-epreuve : stable sur le meme contenu", ea == ea2 && ea != 0u, d);

			// Et les OPTIONS comptent : changer l'usage doit produire un autre
			// actif, pas ecraser le precedent.
			NkTexOvenReglages r2 = r;
			r2.sRGB = !r.sRGB;
			const nk_uint64 eo = NkTextureCache::Empreinte(fa, r2);
			std::snprintf(d, sizeof(d), "sRGB inverse -> %016llX (etait %016llX)", (unsigned long long)eo,
						  (unsigned long long)ea);
			Verdict("empreinte / une option changee change l'empreinte", eo != ea && eo != 0u, d);
		}
	}

	// -- 8. LE CACHE : cuisson paresseuse, et AUCUN actif perime --------------
	//
	// Trois choses a prouver, et la troisieme est celle qui compte :
	//   a) premier chargement = manque, l'actif est ecrit dans le cache ;
	//   b) second chargement (bibliotheque neuve) = touche, servi par le cache ;
	//   c) source MODIFIEE = nouvelle empreinte = les pixels televerses sont ceux
	//      du NOUVEAU contenu. C'est la propriete que l'empreinte par contenu
	//      achete, et qu'un cache par date n'aurait pas : `git checkout` d'une
	//      ancienne texture lui donne une date NEUVE.
	{
		const char *pngCache = "Build/nktex_cache_source.png";

		auto EcrirePng = [&](uint8 teinte) -> bool {
			NkImage im = NkImage::Alloc(32, 32, NkImagePixelFormat::NK_RGBA32);
			if (!im.IsValid())
				return false;
			for (int32 y = 0; y < 32; ++y) {
				uint8 *r = im.RowPtr(y);
				for (int32 x = 0; x < 32; ++x) {
					uint8 *q = r + usize(x) * 4;
					q[0] = teinte;
					q[1] = uint8(x * 8);
					q[2] = uint8(y * 8);
					q[3] = 255;
				}
			}
			return im.SavePNG(pngCache);
		};

		// Le premier pixel televerse, relu dans le dorsal : c'est lui qui dit
		// quel CONTENU a ete servi.
		auto TeinteTeleversee = [&](NkTextureLibrary &l, const NkString &chemin) -> int {
			NkLoadOptions o;
			o.genMipmaps = false;
			o.useAnisotropic = false;
			NkTexHandle t = l.Load(chemin, o);
			if (!t.IsValid())
				return -1;
			NkSWTexture *sw = dev.GetTex(l.GetRHIHandle(t).id);
			if (!sw || sw->mips.Size() == 0 || sw->mips[0].Size() < 4)
				return -1;
			return int(sw->mips[0][0]);
		};

		if (!NkTextureCache::Actif()) {
			Verdict("cache / actif", false, "NK_TEX_CACHE=0 : ce temoin ne peut rien prouver");
		} else if (!EcrirePng(11)) {
			Verdict("cache / preparation", false, "ecriture du PNG de travail impossible");
		} else {
			NkTextureCache::RemettreCompteursAZero();

			// (a) premier chargement : manque, puis cuisson.
			NkTextureLibrary l1;
			l1.Init(&dev, nullptr);
			const int t1 = TeinteTeleversee(l1, NkString(pngCache));
			const nk_uint32 manques1 = NkTextureCache::Manques();
			const nk_uint32 touches1 = NkTextureCache::Touches();
			std::snprintf(d, sizeof(d), "teinte %d, %u manque(s), %u touche(s) (attendu 1 et 0)", t1, manques1,
						  touches1);
			Verdict("cache / 1er chargement : manque, puis cuisson", t1 == 11 && manques1 == 1u && touches1 == 0u, d);

			// (b) second chargement, bibliotheque NEUVE : servi par le cache.
			NkTextureLibrary l2;
			l2.Init(&dev, nullptr);
			const int t2 = TeinteTeleversee(l2, NkString(pngCache));
			const nk_uint32 touches2 = NkTextureCache::Touches();
			std::snprintf(d, sizeof(d), "teinte %d, %u touche(s) (attendu 1)", t2, touches2);
			Verdict("cache / 2e chargement : servi par le cache", t2 == 11 && touches2 == 1u, d);

			// (c) LA source change : l'empreinte change, donc le contenu servi
			//     aussi. Un cache par date servirait ici l'ancien actif.
			if (!EcrirePng(222)) {
				Verdict("cache / modification de la source", false, "reecriture du PNG impossible");
			} else {
				NkTextureLibrary l3;
				l3.Init(&dev, nullptr);
				const int t3 = TeinteTeleversee(l3, NkString(pngCache));
				std::snprintf(d, sizeof(d), "teinte servie %d (attendu 222, PAS 11)", t3);
				Verdict("cache / source modifiee : jamais l'actif perime", t3 == 222, d);
				l3.Shutdown();
			}
			l2.Shutdown();
			l1.Shutdown();
		}
	}

	lib.Shutdown();
	dev.Shutdown();

	std::printf("\n=============================================================\n");
	std::printf(" %s\n", g_echecs == 0 ? "TOUT VERT." : "ROUGE — voir les lignes ci-dessus.");
	std::printf("=============================================================\n");
	return g_echecs == 0 ? 0 : 1;
}
