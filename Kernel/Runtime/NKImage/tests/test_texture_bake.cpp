// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_texture_bake.cpp — banc du FORMAT D'ACTIF texture (.nktex)
//
// Deux choses dans ce fichier, et dans cet ordre :
//
//   ETAPE 0 — LA MESURE. Ce que coute AUJOURD'HUI le decodage PNG/JPEG au
//   chargement d'un jeu de textures reel du depot. C'est ce chiffre — et lui
//   seul — qui justifie ou non le format d'actif.
//
//   ETAPE 1 — LES TEMOINS DU FORMAT. Aller-retour au bit pres, mipmaps
//   verifiees comme des reductions 2x, additivite (un champ inconnu se relit et
//   se reemet intact), refus dit sur un en-tete tronque, et le GAIN chiffre.
//
// Regle du depot : un instrument neuf se calibre par un CONTROLE POSITIF avant
// qu'on croie son chiffre. Le chronometre est donc verifie contre une duree
// connue (sommeil) ET contre une duree nulle, avant toute mesure de decodage.
// =============================================================================
#include "NKImage/NKImage.h"
#include "NKSerialization/Asset/NkTextureAssetFormat.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstring>
#include <utility>

using namespace nkentseu;

namespace {

	int g_echecs = 0;

	void Verdict(const char *nom, bool ok, const char *detail) {
		std::printf("[%s] %-46s %s\n", ok ? "VERT " : "ROUGE", nom, detail);
		if (!ok)
			++g_echecs;
	}

	// Taille d'un fichier sur disque, sans dependre de NKFileSystem.
	long long TailleFichier(const char *chemin) {
		std::FILE *f = std::fopen(chemin, "rb");
		if (!f)
			return -1;
		std::fseek(f, 0, SEEK_END);
		long long n = std::ftell(f);
		std::fclose(f);
		return n;
	}

	bool EcrireFichier(const char *chemin, const nk_uint8 *data, nk_size n) {
		std::FILE *f = std::fopen(chemin, "wb");
		if (!f)
			return false;
		const bool ok = (std::fwrite(data, 1, n, f) == n);
		std::fclose(f);
		return ok;
	}

	bool LireFichier(const char *chemin, NkVector<nk_uint8> &out) {
		std::FILE *f = std::fopen(chemin, "rb");
		if (!f)
			return false;
		std::fseek(f, 0, SEEK_END);
		const long n = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
		out.Clear();
		out.Resize(nk_size(n));
		const bool ok = (std::fread(out.Data(), 1, nk_size(n), f) == nk_size(n));
		std::fclose(f);
		return ok;
	}

	// =========================================================================
	// CONTROLE POSITIF DU CHRONOMETRE
	//
	// Deux bornes, dans les deux sens :
	//   - une duree CONNUE (200 ms de sommeil) doit se lire dans [150, 300] ms.
	//     Un chrono bloque a zero, ou dont l'unite est fausse d'un facteur 1000,
	//     rougit ici.
	//   - une duree NULLE (bloc vide) doit se lire sous 1 ms. Un chrono qui rend
	//     toujours la meme grande valeur rougit ici.
	// =========================================================================
	void ControlePositifChrono() {
		NkChrono::BeginPreciseTiming();

		NkChrono c;
		NkChrono::SleepMilliseconds(200);
		const float64 msConnu = c.Elapsed().ToMilliseconds();

		NkChrono c2;
		const float64 msNul = c2.Elapsed().ToMilliseconds();

		NkChrono::EndPreciseTiming();

		char d[160];
		std::snprintf(d, sizeof(d), "sommeil 200 ms lu %.2f ms (attendu 150..300)", msConnu);
		Verdict("chrono / controle positif (duree connue)", msConnu >= 150.0 && msConnu <= 300.0, d);

		std::snprintf(d, sizeof(d), "bloc vide lu %.4f ms (attendu < 1)", msNul);
		Verdict("chrono / controle nul (duree nulle)", msNul >= 0.0 && msNul < 1.0, d);
	}

	// =========================================================================
	// ETAPE 0 — le cout actuel
	// =========================================================================
	struct Lot {
			const char *nom;
			const char *fichiers[8];
	};

	struct Bilan {
			long long octetsDisque = 0;
			float64 ms = 0.0;
			long long octetsRam = 0;
			int nbFichiers = 0;
	};

	// Un jeu de textures est mesure ENTIER : c'est ainsi qu'un materiau se
	// charge (albedo + normale + metallique + rugosite + occlusion).
	Bilan MesurerLot(const Lot &lot) {
		Bilan b;
		long long octetsAvecMips = 0;
		int nbManquants = 0;

		std::printf("\n--- lot « %s » ---\n", lot.nom);
		for (int i = 0; i < 8 && lot.fichiers[i]; ++i) {
			const char *chemin = lot.fichiers[i];
			const long long taille = TailleFichier(chemin);
			if (taille < 0) {
				std::printf("    (absent) %s\n", chemin);
				++nbManquants;
				continue;
			}

			NkImage img;
			NkChrono c;
			const bool ok = img.Load(chemin);
			const float64 ms = c.Elapsed().ToMilliseconds();

			if (!ok || !img.IsValid()) {
				std::printf("    (echec decodage) %s\n", chemin);
				++nbManquants;
				continue;
			}

			const long long brut =
				(long long)img.Width() * (long long)img.Height() * (long long)BytesPerPixelOf(img.Format());
			// Chaine de mips complete = 1 + 1/4 + 1/16 + ... ≈ 4/3 du niveau 0.
			octetsAvecMips += brut + brut / 3;

			b.octetsDisque += taille;
			b.octetsRam += brut;
			b.ms += ms;
			++b.nbFichiers;

			std::printf("    %-52s %5dx%-5d %2d o/px  %7.2f Mo disque  %7.2f ms\n", chemin, img.Width(), img.Height(),
						(int)BytesPerPixelOf(img.Format()), (float64)taille / 1048576.0, ms);
		}

		std::printf("  => %d fichier(s), %d absent(s)\n", b.nbFichiers, nbManquants);
		std::printf("  => DISQUE lu      : %8.2f Mo\n", (float64)b.octetsDisque / 1048576.0);
		std::printf("  => DECODAGE       : %8.2f ms\n", b.ms);
		std::printf("  => RAM mip 0      : %8.2f Mo\n", (float64)b.octetsRam / 1048576.0);
		std::printf("  => VRAM + mips    : %8.2f Mo  (mip 0 x 4/3)\n", (float64)octetsAvecMips / 1048576.0);
		return b;
	}

	// =========================================================================
	// ETAPE 1 — le four, en une fonction : image decodee -> payload .nktex
	// =========================================================================
	nk_uint32 CodeFormatDepuis(NkImagePixelFormat f, bool srgb) {
		switch (f) {
			case NkImagePixelFormat::NK_GRAY8:
				return NKTEXFMT_R8_UNORM;
			case NkImagePixelFormat::NK_GRAY_A16:
				return NKTEXFMT_RG8_UNORM;
			case NkImagePixelFormat::NK_RGB24:
				return srgb ? NKTEXFMT_RGB8_SRGB : NKTEXFMT_RGB8_UNORM;
			case NkImagePixelFormat::NK_RGBA32:
				return srgb ? NKTEXFMT_RGBA8_SRGB : NKTEXFMT_RGBA8_UNORM;
			case NkImagePixelFormat::NK_RGB96F:
				return NKTEXFMT_RGB32_FLOAT;
			case NkImagePixelFormat::NK_RGBA128F:
				return NKTEXFMT_RGBA32_FLOAT;
			default:
				return NKTEXFMT_INCONNU;
		}
	}

	// Recopie les lignes de `img` SERREES : l'image aligne son stride sur 4
	// octets, le fichier porte un pas de ligne explicite et sans trou.
	void SerrerLignes(const NkImage &img, NkVector<nk_uint8> &out) {
		const nk_size pas = nk_size(img.Width()) * nk_size(BytesPerPixelOf(img.Format()));
		out.Clear();
		out.Resize(pas * nk_size(img.Height()));
		for (int32 y = 0; y < img.Height(); ++y)
			memcpy(out.Data() + nk_size(y) * pas, img.RowPtr(y), pas);
	}

	// Le four : une image decodee -> le payload d'un `.nktex`, mipmaps comprises.
	bool Cuire(const NkImage &source, bool srgb, bool avecMips, NkVector<nk_uint8> &payload, NkString *err) {
		const nk_uint32 code = CodeFormatDepuis(source.Format(), srgb);
		if (code == NKTEXFMT_INCONNU) {
			if (err)
				*err = NkString("format de pixel non cuisinable");
			return false;
		}

		// La chaine de niveaux, chacun serre. On ne garde que les octets : les
		// descripteurs pointent dedans, il faut donc que le tampon ne bouge plus
		// (d'ou le Resize prealable, jamais un PushBack apres coup).
		const nk_uint32 nMax = avecMips ? NkTexturePayload::CompteMipsComplet(nk_uint32(source.Width()),
																			 nk_uint32(source.Height()))
										: 1u;
		NkVector<NkVector<nk_uint8>> octets;
		octets.Resize(nk_size(nMax));
		NkVector<nk_uint32> largeurs, hauteurs;

		{
			NkImage courant = source.Copy();
			if (!courant.IsValid()) {
				if (err)
					*err = NkString("copie de l'image source impossible");
				return false;
			}
			for (nk_uint32 i = 0; i < nMax; ++i) {
				SerrerLignes(courant, octets[i]);
				largeurs.PushBack(nk_uint32(courant.Width()));
				hauteurs.PushBack(nk_uint32(courant.Height()));
				if (i + 1u == nMax)
					break;
				NkImage suivant = courant.ReduceHalf();
				if (!suivant.IsValid())
					break;
				courant = std::move(suivant);
			}
		}

		const nk_size nNiveaux = largeurs.Size();
		const nk_uint32 bpp = nk_uint32(BytesPerPixelOf(source.Format()));

		NkTexCuisson cuisson;
		cuisson.formatCode = code;
		cuisson.width = nk_uint32(source.Width());
		cuisson.height = nk_uint32(source.Height());
		cuisson.depth = 1u;
		cuisson.arrayLayers = 1u;
		cuisson.flags = (srgb ? nk_uint32(NKTEXFLAG_SRGB) : 0u) |
						(nNiveaux > 1u ? nk_uint32(NKTEXFLAG_MIPS_PRECALCULES) : 0u);
		cuisson.rowAlignment = 1u;
		for (nk_size i = 0; i < nNiveaux; ++i) {
			NkTexNiveauSource n;
			n.width = largeurs[i];
			n.height = hauteurs[i];
			n.depth = 1u;
			n.rowPitch = largeurs[i] * bpp;
			n.data = octets[i].Data();
			n.size = nk_uint32(octets[i].Size());
			cuisson.levels.PushBack(n);
		}
		return NkTexturePayload::Encode(cuisson, payload, err);
	}

	// -------------------------------------------------------------------------
	// Une image de reference deterministe : degrades croises + damier, pour que
	// la reduction 2x ait quelque chose a moyenner (une image plate passerait
	// n'importe quel filtre, y compris un filtre faux).
	// -------------------------------------------------------------------------
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

	// =========================================================================
	// TEMOIN 1 — aller-retour : les pixels du niveau 0 sont IDENTIQUES AU BIT
	// a ceux du decodage direct du PNG.
	// =========================================================================
	void TemoinAllerRetour() {
		const char *pngRef = "Build/nktex_reference.png";
		const char *actif = "Build/nktex_reference.nktexp";

		NkImage ref = ImageReference(64, 48);
		if (!ref.IsValid() || !ref.SavePNG(pngRef)) {
			Verdict("aller-retour / preparation", false, "impossible d'ecrire l'image de reference");
			return;
		}

		// Chemin A : le decodage direct, celui d'aujourd'hui.
		NkImage direct;
		if (!direct.Load(pngRef)) {
			Verdict("aller-retour / preparation", false, "relecture PNG impossible");
			return;
		}

		// Chemin B : le four, puis la lecture de l'actif.
		NkVector<nk_uint8> payload;
		NkString err;
		if (!Cuire(direct, /*srgb*/ true, /*avecMips*/ true, payload, &err)) {
			Verdict("aller-retour / cuisson", false, err.CStr());
			return;
		}
		if (!EcrireFichier(actif, payload.Data(), payload.Size())) {
			Verdict("aller-retour / ecriture", false, "ecriture de l'actif impossible");
			return;
		}
		NkVector<nk_uint8> relu;
		if (!LireFichier(actif, relu)) {
			Verdict("aller-retour / lecture", false, "relecture de l'actif impossible");
			return;
		}
		NkTexVue vue;
		if (!NkTexturePayload::Decode(relu.Data(), relu.Size(), vue, &err)) {
			Verdict("aller-retour / decodage", false, err.CStr());
			return;
		}

		char d[220];

		const bool dims = (vue.width == nk_uint32(direct.Width()) && vue.height == nk_uint32(direct.Height()) &&
						   vue.formatCode == nk_uint32(NKTEXFMT_RGBA8_SRGB) && vue.EstSrgb());
		std::snprintf(d, sizeof(d), "%ux%u format=%s srgb=%d", vue.width, vue.height, NkTexFormatNom(vue.formatCode),
					  vue.EstSrgb() ? 1 : 0);
		Verdict("aller-retour / en-tete relu", dims, d);

		// Les pixels du niveau 0, octet par octet.
		const nk_size pas = nk_size(direct.Width()) * nk_size(BytesPerPixelOf(direct.Format()));
		nk_uint64 differents = 0;
		if (vue.levels.Size() > 0 && vue.levels[0].data) {
			for (int32 y = 0; y < direct.Height(); ++y) {
				const nk_uint8 *a = direct.RowPtr(y);
				const nk_uint8 *b = vue.levels[0].data + nk_size(y) * nk_size(vue.levels[0].rowPitch);
				for (nk_size k = 0; k < pas; ++k)
					if (a[k] != b[k])
						++differents;
			}
		} else {
			differents = 1;
		}
		std::snprintf(d, sizeof(d), "%llu octet(s) different(s) sur %llu (attendu 0 — au BIT)",
					  (unsigned long long)differents, (unsigned long long)(nk_uint64(pas) * nk_uint64(direct.Height())));
		Verdict("aller-retour / pixels du niveau 0 au bit", differents == 0, d);
	}

	// =========================================================================
	// TEMOIN 2 — les mipmaps : compte attendu, et chaque niveau EST la moyenne
	// des blocs 2x2 du precedent.
	// =========================================================================
	void TemoinMipmaps() {
		const int32 W = 64, H = 48;
		NkImage ref = ImageReference(W, H);
		NkVector<nk_uint8> payload;
		NkString err;
		if (!ref.IsValid() || !Cuire(ref, true, true, payload, &err)) {
			Verdict("mipmaps / cuisson", false, err.CStr());
			return;
		}
		NkTexVue vue;
		if (!NkTexturePayload::Decode(payload.Data(), payload.Size(), vue, &err)) {
			Verdict("mipmaps / decodage", false, err.CStr());
			return;
		}

		char d[220];

		// ── Le COMPTE attendu : 64x48 -> 32x24 -> 16x12 -> 8x6 -> 4x3 -> 2x1
		//    -> 1x1, soit 7 niveaux.
		const nk_uint32 attendu = NkTexturePayload::CompteMipsComplet(nk_uint32(W), nk_uint32(H));
		std::snprintf(d, sizeof(d), "%u niveaux, attendu %u pour %dx%d", vue.mipCount, attendu, W, H);
		Verdict("mipmaps / compte de niveaux", vue.mipCount == attendu && vue.levels.Size() == nk_size(attendu), d);
		if (vue.levels.Size() != nk_size(attendu))
			return;

		// ── Chaque niveau est-il la reduction 2x du precedent ? On recalcule la
		//    moyenne des blocs 2x2 et on exige l'egalite exacte : l'arrondi est
		//    deterministe, il n'y a pas de flottant, donc epsilon = 0.
		nk_uint64 ecartMax = 0;
		nk_uint64 composantes = 0;
		for (nk_size l = 1; l < vue.levels.Size(); ++l) {
			const NkTexNiveauVue &prec = vue.levels[l - 1];
			const NkTexNiveauVue &cur = vue.levels[l];

			const nk_uint32 wAttendu = (prec.width > 1u) ? (prec.width / 2u) : 1u;
			const nk_uint32 hAttendu = (prec.height > 1u) ? (prec.height / 2u) : 1u;
			if (cur.width != wAttendu || cur.height != hAttendu) {
				std::snprintf(d, sizeof(d), "niveau %llu : %ux%u, attendu %ux%u", (unsigned long long)l, cur.width,
							  cur.height, wAttendu, hAttendu);
				Verdict("mipmaps / dimension d'un niveau", false, d);
				return;
			}

			for (nk_uint32 y = 0; y < cur.height; ++y) {
				const nk_uint32 y0 = (prec.height > 1u) ? (y * 2u) : 0u;
				const nk_uint32 y1 = (y0 + 1u < prec.height) ? (y0 + 1u) : y0;
				for (nk_uint32 x = 0; x < cur.width; ++x) {
					const nk_uint32 x0 = (prec.width > 1u) ? (x * 2u) : 0u;
					const nk_uint32 x1 = (x0 + 1u < prec.width) ? (x0 + 1u) : x0;
					for (nk_uint32 k = 0; k < 4u; ++k) {
						const nk_uint32 a = prec.data[y0 * prec.rowPitch + x0 * 4u + k];
						const nk_uint32 b = prec.data[y0 * prec.rowPitch + x1 * 4u + k];
						const nk_uint32 c = prec.data[y1 * prec.rowPitch + x0 * 4u + k];
						const nk_uint32 e = prec.data[y1 * prec.rowPitch + x1 * 4u + k];
						const nk_uint32 moyenne = (a + b + c + e + 2u) / 4u;
						const nk_uint32 lu = cur.data[y * cur.rowPitch + x * 4u + k];
						const nk_uint64 ecart = (lu > moyenne) ? (lu - moyenne) : (moyenne - lu);
						if (ecart > ecartMax)
							ecartMax = ecart;
						++composantes;
					}
				}
			}
		}
		std::snprintf(d, sizeof(d), "ecart max %llu sur %llu composantes (attendu 0)", (unsigned long long)ecartMax,
					  (unsigned long long)composantes);
		Verdict("mipmaps / chaque niveau = moyenne des blocs 2x2", ecartMax == 0 && composantes > 0, d);

		// ── Le drapeau doit DIRE que les mips sont dans le fichier.
		Verdict("mipmaps / drapeau MIPS_PRECALCULES pose", (vue.flags & nk_uint32(NKTEXFLAG_MIPS_PRECALCULES)) != 0u,
				"le fichier annonce porter ses niveaux");
	}

	// =========================================================================
	// TEMOIN 3 — ADDITIVITE : un champ que cette version ne comprend pas se
	// relit et se REEMET INTACT. Sans ce temoin, le format n'est additif que
	// sur le papier.
	// =========================================================================
	void TemoinAdditivite() {
		NkImage ref = ImageReference(16, 16);
		NkVector<nk_uint8> v1;
		NkString err;
		if (!Cuire(ref, false, false, v1, &err)) {
			Verdict("additivite / cuisson", false, err.CStr());
			return;
		}

		const nk_uint8 kQueue[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};

		NkTexVue vueV1;
		if (!NkTexturePayload::Decode(v1.Data(), v1.Size(), vueV1, &err)) {
			Verdict("additivite / decodage v1", false, err.CStr());
			return;
		}

		// Ce qu'ecrirait une version FUTURE : huit octets de plus dans
		// l'en-tete, `headerSize` qui les annonce, et les offsets decales.
		NkTexCuisson recuit;
		recuit.formatCode = vueV1.formatCode;
		recuit.width = vueV1.width;
		recuit.height = vueV1.height;
		recuit.depth = vueV1.depth;
		recuit.arrayLayers = vueV1.arrayLayers;
		recuit.flags = vueV1.flags;
		recuit.addressMode = vueV1.addressMode;
		recuit.filterMode = vueV1.filterMode;
		recuit.rowAlignment = vueV1.rowAlignment;
		for (int i = 0; i < 8; ++i)
			recuit.enTeteInconnu.PushBack(kQueue[i]);
		for (nk_size i = 0; i < vueV1.levels.Size(); ++i) {
			NkTexNiveauSource n;
			n.width = vueV1.levels[i].width;
			n.height = vueV1.levels[i].height;
			n.depth = vueV1.levels[i].depth;
			n.rowPitch = vueV1.levels[i].rowPitch;
			n.data = vueV1.levels[i].data;
			n.size = vueV1.levels[i].size;
			recuit.levels.PushBack(n);
		}

		NkVector<nk_uint8> avecQueue;
		if (!NkTexturePayload::Encode(recuit, avecQueue, &err)) {
			Verdict("additivite / reencodage", false, err.CStr());
			return;
		}

		// Le lecteur v1 doit LIRE ce fichier — pas le refuser.
		NkTexVue vue2;
		if (!NkTexturePayload::Decode(avecQueue.Data(), avecQueue.Size(), vue2, &err)) {
			Verdict("additivite / un lecteur v1 lit un en-tete plus long", false, err.CStr());
			return;
		}

		char d[220];
		bool queueIntacte = (vue2.enTeteInconnu.Size() == 8u);
		for (nk_size i = 0; queueIntacte && i < 8u; ++i)
			queueIntacte = (vue2.enTeteInconnu[i] == kQueue[i]);
		std::snprintf(d, sizeof(d), "%llu octet(s) inconnus rendus, %s", (unsigned long long)vue2.enTeteInconnu.Size(),
					  queueIntacte ? "identiques" : "ALTERES");
		Verdict("additivite / champ inconnu relu intact", queueIntacte, d);

		nk_uint64 diff = 0;
		if (vue2.levels.Size() == vueV1.levels.Size() && vue2.levels.Size() > 0) {
			for (nk_uint32 k = 0; k < vue2.levels[0].size; ++k)
				if (vue2.levels[0].data[k] != vueV1.levels[0].data[k])
					++diff;
		} else {
			diff = 1;
		}
		std::snprintf(d, sizeof(d), "%llu octet(s) different(s) apres le detour", (unsigned long long)diff);
		Verdict("additivite / pixels intacts apres reemission", diff == 0, d);
	}

	// =========================================================================
	// TEMOIN 4 — un en-tete abime se REFUSE en le disant, il ne plante pas.
	// =========================================================================
	void TemoinRefus() {
		NkImage ref = ImageReference(16, 16);
		NkVector<nk_uint8> bon;
		NkString err;
		if (!Cuire(ref, false, true, bon, &err)) {
			Verdict("refus / cuisson", false, err.CStr());
			return;
		}

		char d[260];
		NkTexVue vue;

		// (a) tronque au milieu de l'en-tete
		{
			NkString e;
			const bool ok = NkTexturePayload::Decode(bon.Data(), 20u, vue, &e);
			std::snprintf(d, sizeof(d), "refuse=%d, raison dite : « %s »", ok ? 0 : 1, e.CStr());
			Verdict("refus / en-tete tronque", !ok && e.Length() > 0, d);
		}

		// (b) magie abimee
		{
			NkVector<nk_uint8> abime;
			abime.Resize(bon.Size());
			memcpy(abime.Data(), bon.Data(), bon.Size());
			abime[2] = nk_uint8('X');
			NkString e;
			const bool ok = NkTexturePayload::Decode(abime.Data(), abime.Size(), vue, &e);
			std::snprintf(d, sizeof(d), "refuse=%d, raison dite : « %s »", ok ? 0 : 1, e.CStr());
			Verdict("refus / magie abimee", !ok && e.Length() > 0, d);
		}

		// (c) fichier coupe apres l'en-tete : les niveaux debordent
		{
			NkString e;
			const nk_size coupe = nk_size(kNkTexHeaderSizeV1) + 40u;
			const bool ok = (coupe < bon.Size()) ? NkTexturePayload::Decode(bon.Data(), coupe, vue, &e) : true;
			std::snprintf(d, sizeof(d), "refuse=%d, raison dite : « %s »", ok ? 0 : 1, e.CStr());
			Verdict("refus / donnees tronquees", !ok && e.Length() > 0, d);
		}

		// (d) CONTRE-EPREUVE : un payload sain doit TOUJOURS passer. Sans elle,
		//     un decodeur qui refuse tout ferait verdir les trois lignes
		//     ci-dessus sans rien prouver.
		{
			NkString e;
			const bool ok = NkTexturePayload::Decode(bon.Data(), bon.Size(), vue, &e);
			std::snprintf(d, sizeof(d), "accepte=%d (le refus n'est donc pas systematique)", ok ? 1 : 0);
			Verdict("refus / contre-epreuve : un payload sain est ACCEPTE", ok, d);
		}
	}

	// =========================================================================
	// TEMOIN 5 — LE GAIN, CHIFFRE. Sur les MEMES fichiers que l'etape 0 :
	// decodage PNG/JPEG d'un cote, lecture de l'actif cuit de l'autre.
	// =========================================================================
	void TemoinGain(const Lot &lot, const Bilan &avant) {
		if (avant.nbFichiers == 0) {
			Verdict("gain / lot mesurable", false, "aucun fichier lu a l'etape 0");
			return;
		}

		// ── Cuisson : le travail que l'on DEPLACE, paye une seule fois ──
		NkVector<NkString> actifs;
		long long octetsActifs = 0;
		float64 msCuisson = 0.0;
		for (int i = 0; i < 8 && lot.fichiers[i]; ++i) {
			NkImage img;
			if (!img.Load(lot.fichiers[i]))
				continue;
			NkVector<nk_uint8> payload;
			NkString err;
			NkChrono c;
			const bool ok = Cuire(img, true, true, payload, &err);
			msCuisson += c.Elapsed().ToMilliseconds();
			if (!ok) {
				std::printf("    (cuisson refusee) %s : %s\n", lot.fichiers[i], err.CStr());
				continue;
			}
			NkString sortie = NkString::Fmtf("Build/nktex_gain_%d.nktexp", i);
			if (!EcrireFichier(sortie.CStr(), payload.Data(), payload.Size()))
				continue;
			octetsActifs += (long long)payload.Size();
			actifs.PushBack(std::move(sortie));
		}

		// ── Chargement : lecture + analyse, SANS AUCUN DECODAGE ──
		float64 msApres = 0.0;
		long long octetsPixels = 0;
		nk_uint32 niveaux = 0;
		bool tousLus = (actifs.Size() > 0);
		for (nk_size i = 0; i < actifs.Size(); ++i) {
			NkVector<nk_uint8> brut;
			NkString err;
			NkChrono c;
			const bool ok = LireFichier(actifs[i].CStr(), brut);
			NkTexVue vue;
			const bool ok2 = ok && NkTexturePayload::Decode(brut.Data(), brut.Size(), vue, &err);
			msApres += c.Elapsed().ToMilliseconds();
			if (!ok2) {
				tousLus = false;
				continue;
			}
			octetsPixels += (long long)NkTexturePayload::OctetsPixels(vue);
			niveaux += vue.mipCount;
		}

		std::printf("\n  == GAIN sur « %s » ==\n", lot.nom);
		std::printf("     AVANT (decodage PNG/JPEG)    : %8.2f ms   %7.2f Mo lus\n", avant.ms,
					(float64)avant.octetsDisque / 1048576.0);
		std::printf("     APRES (lecture de l'actif)   : %8.2f ms   %7.2f Mo lus   %u niveaux, %.2f Mo de pixels\n",
					msApres, (float64)octetsActifs / 1048576.0, niveaux, (float64)octetsPixels / 1048576.0);
		std::printf("     cuisson (payee UNE fois)     : %8.2f ms\n", msCuisson);
		if (msApres > 0.0)
			std::printf("     rapport de temps             : x%.1f\n", avant.ms / msApres);

		char d[260];
		std::snprintf(d, sizeof(d), "%.2f ms -> %.2f ms (x%.1f), disque %.1f -> %.1f Mo", avant.ms, msApres,
					  msApres > 0.0 ? avant.ms / msApres : 0.0, (float64)avant.octetsDisque / 1048576.0,
					  (float64)octetsActifs / 1048576.0);
		// Le seuil est bas EXPRES : un facteur 2 ne se discute pas, alors qu'un
		// banc qui exigerait le facteur mesure aujourd'hui rougirait sur une
		// machine plus lente sans qu'aucune ligne de code n'ait change.
		Verdict("gain / le chargement est au moins 2x plus rapide", tousLus && msApres > 0.0 && avant.ms > 2.0 * msApres,
				d);

		// Ce que le format NE gagne PAS : la place sur disque. Un PNG est
		// compresse, l'actif ne l'est pas encore. On le DIT, plutot que de le
		// laisser decouvrir.
		std::printf("     [DIT  ] l'actif brut pese %.1f Mo contre %.1f Mo de PNG/JPEG — la compression par blocs\n"
					"             est le PALIER SUIVANT, non livre (le RHI ne sait pas televerser un format bloc).\n",
					(float64)octetsActifs / 1048576.0, (float64)avant.octetsDisque / 1048576.0);
	}

} // namespace

int main() {
	std::printf("=============================================================\n");
	std::printf(" ETAPE 0 — cout ACTUEL du decodage image au chargement\n");
	std::printf("=============================================================\n\n");

	ControlePositifChrono();

	// Le seul PNG que charge reellement `renderdemo --demo=2` aujourd'hui.
	static const Lot lotDemo2 = {"renderdemo --demo=2 (scene reelle d'aujourd'hui)",
								 {"Resources/NKRenderer/Textures/Defaults/test_pattern.png", nullptr}};

	// Un materiau PBR complet du depot — ce que charge une scene du moteur des
	// qu'elle porte un vrai materiau (et ce que NK3DModeler importe).
	static const Lot lotPBR = {"materiau PBR rusted_iron (5 cartes)",
							   {"Resources/Textures/PBR/rusted_iron/albedo.png",
								"Resources/Textures/PBR/rusted_iron/normal.png",
								"Resources/Textures/PBR/rusted_iron/metallic.png",
								"Resources/Textures/PBR/rusted_iron/roughness.png",
								"Resources/Textures/PBR/rusted_iron/ao.png", nullptr}};

	// Un modele importe complet, PNG + JPEG melanges.
	static const Lot lotModele = {"modele backpack (5 cartes, PNG + JPEG)",
								  {"Resources/Models/backpack/diffuse.jpg", "Resources/Models/backpack/normal.png",
								   "Resources/Models/backpack/specular.jpg", "Resources/Models/backpack/roughness.jpg",
								   "Resources/Models/backpack/ao.jpg", nullptr}};

	MesurerLot(lotDemo2);
	const Bilan avantPBR = MesurerLot(lotPBR);
	MesurerLot(lotModele);

	std::printf("\n=============================================================\n");
	std::printf(" ETAPE 1 — temoins du format d'actif .nktex\n");
	std::printf("=============================================================\n\n");

	TemoinAllerRetour();
	TemoinMipmaps();
	TemoinAdditivite();
	TemoinRefus();
	TemoinGain(lotPBR, avantPBR);

	std::printf("\n=============================================================\n");
	std::printf(" %s\n", g_echecs == 0 ? "TOUT VERT." : "ROUGE — voir les lignes ci-dessus.");
	std::printf("=============================================================\n");
	return g_echecs == 0 ? 0 : 1;
}
