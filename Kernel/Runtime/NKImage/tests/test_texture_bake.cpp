// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_texture_bake.cpp — banc du FORMAT D'ACTIF texture (.nktex)
//
// ETAPE 0 (mesure) : ce que coute AUJOURD'HUI le decodage PNG/JPEG au
// chargement d'un jeu de textures reel du depot. C'est ce chiffre — et lui
// seul — qui justifie ou non le format d'actif.
//
// Regle du depot : un instrument neuf se calibre par un CONTROLE POSITIF avant
// qu'on croie son chiffre. Le chronometre est donc verifie contre une duree
// connue (sommeil) ET contre une duree nulle, avant toute mesure de decodage.
// =============================================================================
#include "NKImage/NKImage.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstring>

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

	// -------------------------------------------------------------------------
	// CONTROLE POSITIF DU CHRONOMETRE
	//
	// Deux bornes, dans les deux sens :
	//   - une duree CONNUE (200 ms de sommeil) doit se lire dans [150, 300] ms.
	//     Un chrono bloque a zero, ou dont l'unite est fausse d'un facteur 1000,
	//     rougit ici.
	//   - une duree NULLE (bloc vide) doit se lire sous 1 ms. Un chrono qui rend
	//     toujours la meme grande valeur rougit ici.
	// -------------------------------------------------------------------------
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

	struct Lot {
			const char *nom;
			const char *fichiers[8];
	};

	// Un jeu de textures est mesure ENTIER : c'est ainsi qu'un materiau se
	// charge (albedo + normale + metallique + rugosite + occlusion).
	void MesurerLot(const Lot &lot) {
		long long octetsDisque = 0;
		long long octetsDecodes = 0;
		long long octetsAvecMips = 0;
		float64 msTotal = 0.0;
		int nbFichiers = 0;
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
			const long long avecMips = brut + brut / 3;

			octetsDisque += taille;
			octetsDecodes += brut;
			octetsAvecMips += avecMips;
			msTotal += ms;
			++nbFichiers;

			std::printf("    %-52s %5dx%-5d %2d o/px  %7.2f Mo disque  %7.2f ms\n", chemin, img.Width(), img.Height(),
						(int)BytesPerPixelOf(img.Format()), (float64)taille / 1048576.0, ms);
		}

		std::printf("  => %d fichier(s), %d absent(s)\n", nbFichiers, nbManquants);
		std::printf("  => DISQUE lu      : %8.2f Mo\n", (float64)octetsDisque / 1048576.0);
		std::printf("  => DECODAGE       : %8.2f ms\n", msTotal);
		std::printf("  => RAM mip 0      : %8.2f Mo\n", (float64)octetsDecodes / 1048576.0);
		std::printf("  => VRAM + mips    : %8.2f Mo  (mip 0 x 4/3)\n", (float64)octetsAvecMips / 1048576.0);
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
	MesurerLot(lotPBR);
	MesurerLot(lotModele);

	std::printf("\n=============================================================\n");
	std::printf(" %s\n", g_echecs == 0 ? "Instrument calibre." : "INSTRUMENT NON CALIBRE — chiffres non recevables.");
	std::printf("=============================================================\n");
	return g_echecs == 0 ? 0 : 1;
}
