// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// @File    TestLectureFichier.cpp
// @Brief   AddFontFromFile LIT LE FICHIER ENTIER, EN BINAIRE, PAR NkFile.
// @License Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (27/11/2026, ludo-24)
// =============================================================================
//  Sur Android, les polices de NKORA (Lilita One, Nunito) sont rangees dans
//  l'APK (assets/Fonts/). AddFontFromFile les ouvrait par std::ifstream puis
//  fopen, qui ne lisent pas un asset : le jeu se rabattait sur Inter embarquee.
//  AddFontFromFile lit maintenant par NkFile, qui essaie le disque puis
//  l'AAssetManager. Sur le bureau, ce banc prouve que le fichier est lu ENTIER
//  et EN BINAIRE : la police ecrite sur le disque, relue par AddFontFromFile,
//  doit donner un atlas IDENTIQUE, a l'octet pres, a la meme police donnee en
//  memoire. La lecture des assets, elle, se prouve sur l'emulateur (journal de
//  NKORA : « TTF lu: Fonts/... »).
//
//  ⚠️ LE BANC SAIT ROUGIR : la police contient des octets 0x0D 0x0A et 0x1A
//     (temoin dedie). Une ouverture en mode texte (« r » au lieu de « rb »)
//     traduirait les CRLF et s'arreterait au 0x1A sous Windows : la lecture
//     rendrait moins d'octets, et l'egalite tomberait.
// =============================================================================

#include "NKFont/NkFont.h"
#include "NKFileSystem/NkFile.h"
#include "NKCore/Text/NkSnprintf.h"

using namespace nkentseu;

namespace {

	/// L'empreinte d'un atlas construit : taille, somme et compte des octets non nuls, avance de A.
	struct Empreinte {
			bool construit = false;
			nkft_int32 largeur = 0, hauteur = 0;
			unsigned long long somme = 0;
			unsigned nonNuls = 0;
			float avanceA = 0.f;
	};

	Empreinte Mesurer(NkFontAtlas &atlas, NkFont *police) {
		Empreinte e;
		if (!police || !atlas.Build())
			return e;
		nkft_uint8 *px = nullptr;
		atlas.GetTexDataAsAlpha8(&px, &e.largeur, &e.hauteur);
		if (!px || e.largeur <= 0 || e.hauteur <= 0)
			return e;
		e.construit = true;
		const long total = static_cast<long>(e.largeur) * static_cast<long>(e.hauteur);
		for (long i = 0; i < total; ++i) {
			e.somme += px[i];
			if (px[i] != 0)
				++e.nonNuls;
		}
		if (const NkFontGlyph *g = police->FindGlyphNoFallback('A'))
			e.avanceA = g->advanceX;
		return e;
	}

	bool Egales(const Empreinte &a, const Empreinte &b) {
		return a.construit && b.construit && a.largeur == b.largeur && a.hauteur == b.hauteur && a.somme == b.somme &&
			   a.nonNuls == b.nonNuls && a.avanceA == b.avanceA;
	}

} // namespace

/// (27/11/2026) Les temoins de la lecture d'une police depuis un fichier (AddFontFromFile par NkFile).
void NkTestsLectureFichier(void (*temoin)(bool, const char *), const nkft_uint8 *octets, nkft_uint32 taille) {
	// Le banc se lance depuis la racine de Nkentseu (comme toutes les applications) : Build/Tests/ existe.
	const char *chemin = "Build/Tests/NKFont_Tests_lecture_fichier.ttf";
	const char *absent = "Build/Tests/NKFont_Tests_ce_fichier_n_existe_pas.ttf";

	// La condition qui rend le banc capable de rougir : des CRLF et un 0x1A dans la police.
	unsigned crlf = 0, sub = 0;
	for (nkft_uint32 i = 0; i < taille; ++i) {
		if (octets[i] == 0x1Au)
			++sub;
		if (i + 1 < taille && octets[i] == 0x0Du && octets[i + 1] == 0x0Au)
			++crlf;
	}
	char nom[256];
	NkSnprintf(nom, sizeof(nom),
			   "la police du banc contient %u paires 0x0D 0x0A et %u octets 0x1A (un mode texte la tronquerait)", crlf,
			   sub);
	temoin(crlf > 0 && sub > 0, nom);

	bool ecrit = false;
	{
		NkFile f;
		if (f.Open(chemin, NkFileMode::NK_WRITE_BINARY) && f.IsOpen()) {
			ecrit = f.Write(octets, static_cast<usize>(taille)) == static_cast<usize>(taille);
			f.Close();
		}
	}
	NkSnprintf(nom, sizeof(nom), "le banc ecrit la police (%u octets) dans %s", static_cast<unsigned>(taille), chemin);
	temoin(ecrit && NkFile::GetFileSize(chemin) == static_cast<nk_int64>(taille), nom);

	Empreinte memoire, fichier;
	bool lue = false;
	{
		NkFontAtlas *atlas = new NkFontAtlas();
		memoire = Mesurer(*atlas, atlas->AddFontFromMemory(octets, taille, 18.f, nullptr));
		delete atlas;
	}
	{
		NkFontAtlas *atlas = new NkFontAtlas();
		NkFont *police = atlas->AddFontFromFile(chemin, 18.f, nullptr);
		lue = police != nullptr;
		fichier = Mesurer(*atlas, police);
		delete atlas;
	}
	temoin(lue, "AddFontFromFile lit la police ecrite sur le disque (une police, pas nullptr)");
	NkSnprintf(nom, sizeof(nom),
			   "police lue du fichier = la meme donnee en memoire, a l'octet pres (atlas %dx%d, somme %llu, %u non nuls)",
			   static_cast<int>(fichier.largeur), static_cast<int>(fichier.hauteur), fichier.somme, fichier.nonNuls);
	temoin(Egales(fichier, memoire) && memoire.nonNuls > 0, nom);

	{
		NkFontAtlas *atlas = new NkFontAtlas();
		NkFont *police = atlas->AddFontFromFile(absent, 18.f, nullptr);
		temoin(police == nullptr && !NkFile::Exists(absent), "NEGATIF : un fichier absent rend nullptr, sans police");
		delete atlas;
	}

	// Le fichier du banc est le sien : il le retire (Build/ n'est pas suivi par git).
	NkFile::Delete(chemin);
	temoin(!NkFile::Exists(chemin), "le banc retire son fichier");
}
