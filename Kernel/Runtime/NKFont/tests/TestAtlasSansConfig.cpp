// =============================================================================
// @File    TestAtlasSansConfig.cpp
// @Brief   UNE POLICE AJOUTEE SANS CONFIGURATION DOIT SE DESSINER.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (13/10/2026) — L'ATLAS ENTIEREMENT TRANSPARENT
// =============================================================================
//  Un etudiant ecrit ce que l'en-tete lui propose :
//
//      atlas.AddFontFromFile(chemin, 18.f);   // le troisieme argument vaut nullptr
//      atlas.Build();
//
//  et obtient une texture dont TOUS les octets valent zero, sans une erreur.
//  La cause tenait en une ligne du constructeur : `memset(configs, 0, ...)`
//  effacait les valeurs par defaut que `NkFontConfig` declare, et l'ajout sans
//  configuration ne les reposait pas. Trois champs en mouraient :
//
//    - `rasterizerMultiply` = 0 au lieu de 1 : chaque glyphe, une fois dessine,
//      etait multiplie par zero ;
//    - `glyphMaxAdvanceX`   = 0 au lieu de 1e9 : chaque lettre avancait de zero,
//      tout le texte se serait empile sur lui-meme ;
//    - `glyphFallback`      = 0 au lieu de '?'.
//
//  Les polices embarquees (`NkFontEmbedded::AddToAtlas`) n'en souffraient pas :
//  elles construisent leur propre `NkFontConfig` et la passent. C'est pourquoi
//  NKGui, NKCode et les demos n'ont jamais rien montre.
//
// =============================================================================
//  CE QUE CE BANC MESURE
// =============================================================================
//  L'encre : la somme de tous les octets de l'atlas, et le nombre d'octets non
//  nuls -- comptes sur TOUT l'atlas, pas sur un echantillon.
//
//  ⚠️ LE NEGATIF EST DANS LE MEME BINAIRE : un multiplicateur nul demande EXPRES
//     doit donner zero octet non nul. Sans lui, « l'atlas porte de l'encre »
//     pourrait etre vrai pour une raison etrangere aux glyphes, et le banc ne
//     saurait pas rougir.
//
//  ⚠️ LE CRITERE FORT EST UNE EGALITE, PAS UN SEUIL : sans configuration, l'atlas
//     doit etre IDENTIQUE, a l'octet pres, a celui que donne `NkFontConfig()`.
//     Un seuil (« plus de mille octets non nuls ») laisserait passer un atlas a
//     moitie eteint.
// =============================================================================

#include "NKFont/Embedded/NkFontEmbedded.h"
#include "NKFont/NkFont.h"

#include <cstdio>

using namespace nkentseu;

/// (14/10) Les temoins de l'atlas SDF (tests/TestAtlasSdf.cpp).
void NkTestsAtlasSdf(void (*temoin)(bool, const char *), const nkft_uint8 *octets, nkft_uint32 taille);

namespace {

	/// Ce qu'on lit d'un atlas construit.
	struct Mesure {
			bool construit = false;
			unsigned long long somme = 0; ///< Somme de tous les octets de l'atlas.
			unsigned nonNuls = 0;		  ///< Octets non nuls, sur tout l'atlas.
			int largeur = 0, hauteur = 0;
			float avanceA = 0.f, avanceI = 0.f;
			unsigned repli = 0; ///< Point de code du glyphe de repli ; 0 = aucun.
	};

	int gVerts = 0, gRouges = 0;

	void Temoin(bool ok, const char *nom) {
		std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", nom);
		if (ok)
			++gVerts;
		else
			++gRouges;
	}

	/// La premiere police vectorielle embarquee disponible : le banc ne depend d'aucun fichier.
	const NkEmbeddedFontData *Police() {
		const NkEmbeddedFontId ordre[] = {NkEmbeddedFontId::Inter, NkEmbeddedFontId::DejaVuSansMono,
										  NkEmbeddedFontId::Default};
		for (NkEmbeddedFontId id : ordre)
			if (NkFontEmbedded::IsAvailable(id))
				return NkFontEmbedded::GetData(id);
		return nullptr;
	}

	Mesure Lire(NkFontAtlas &atlas, NkFont *police) {
		Mesure m;
		if (!police || !atlas.Build())
			return m;
		nkft_uint8 *px = nullptr;
		nkft_int32 l = 0, h = 0;
		atlas.GetTexDataAsAlpha8(&px, &l, &h);
		if (!px || l <= 0 || h <= 0)
			return m;
		m.construit = true;
		m.largeur = l;
		m.hauteur = h;
		const long total = static_cast<long>(l) * static_cast<long>(h);
		for (long i = 0; i < total; ++i) {
			m.somme += px[i];
			if (px[i] != 0)
				++m.nonNuls;
		}
		if (const NkFontGlyph *g = police->FindGlyphNoFallback('A'))
			m.avanceA = g->advanceX;
		if (const NkFontGlyph *g = police->FindGlyphNoFallback('i'))
			m.avanceI = g->advanceX;
		if (police->fallbackGlyph)
			m.repli = static_cast<unsigned>(police->fallbackGlyph->codepoint);
		return m;
	}

	/// Un atlas neuf, une police, la configuration donnee (nullptr = aucune).
	Mesure Construire(const nkft_uint8 *octets, nkft_uint32 taille, const NkFontConfig *cfg) {
		NkFontAtlas *atlas = new NkFontAtlas();
		const Mesure m = Lire(*atlas, atlas->AddFontFromMemory(octets, taille, 18.f, cfg));
		delete atlas;
		return m;
	}

	bool Identiques(const Mesure &a, const Mesure &b) {
		return a.construit && b.construit && a.somme == b.somme && a.nonNuls == b.nonNuls && a.largeur == b.largeur &&
			   a.hauteur == b.hauteur && a.avanceA == b.avanceA && a.avanceI == b.avanceI && a.repli == b.repli;
	}

	void Dire(const char *nom, const Mesure &m) {
		std::printf("  %-34s : %s, atlas %dx%d, somme %llu, %u octets non nuls, avance A %.2f, i %.2f, repli U+%04X\n", nom,
					m.construit ? "construit" : "NON construit", m.largeur, m.hauteur, m.somme, m.nonNuls,
					static_cast<double>(m.avanceA), static_cast<double>(m.avanceI), m.repli);
	}

} // namespace

int main() {
	std::printf("===== NKFont_Tests : une police sans configuration =====\n");
	const NkEmbeddedFontData *police = Police();
	if (!police) {
		std::printf("  [FAIL] aucune police embarquee disponible : le banc ne peut rien mesurer\n");
		return 1;
	}
	nkft_uint32 taille = 0;
	nkft_uint8 *octets = NkFontEmbedded::DecompressData(*police, &taille);
	if (!octets || taille == 0) {
		std::printf("  [FAIL] la police « %s » ne se decompresse pas\n", police->name);
		return 1;
	}
	std::printf("  police : %s, %u octets\n", police->name, static_cast<unsigned>(taille));

	// ── la reference : la configuration par defaut, ecrite en toutes lettres ──
	const NkFontConfig parDefaut;
	const Mesure reference = Construire(octets, taille, &parDefaut);
	Dire("configuration par defaut", reference);

	// ── le cas de l'etudiant : aucune configuration ──
	const Mesure sans = Construire(octets, taille, nullptr);
	Dire("sans configuration", sans);

	// ── le negatif : un multiplicateur nul, demande expres ──
	NkFontConfig eteinte;
	eteinte.rasterizerMultiply = 0.f;
	const Mesure nulle = Construire(octets, taille, &eteinte);
	Dire("multiplicateur nul (negatif)", nulle);

	// ── un emplacement reutilise : une police reglee, Clear(), puis une police sans configuration ──
	Mesure reutilise;
	{
		NkFontAtlas *atlas = new NkFontAtlas();
		NkFontConfig reglee;
		reglee.rasterizerMultiply = 0.5f;
		reglee.glyphMaxAdvanceX = 3.f;
		reglee.glyphFallback = 'X';
		(void)Lire(*atlas, atlas->AddFontFromMemory(octets, taille, 18.f, &reglee));
		atlas->Clear();
		reutilise = Lire(*atlas, atlas->AddFontFromMemory(octets, taille, 18.f, nullptr));
		delete atlas;
	}
	Dire("sans configuration, apres Clear()", reutilise);

	// ── le repli CHOISI : `glyphFallback` doit etre honore, seul puis avec une police fusionnee ──
	NkFontConfig repliX;
	repliX.glyphFallback = 'X';
	const Mesure choisi = Construire(octets, taille, &repliX);
	Dire("repli choisi : X", choisi);
	Mesure fusionne;
	{
		NkFontAtlas *atlas = new NkFontAtlas();
		NkFont *base = atlas->AddFontFromMemory(octets, taille, 18.f, &repliX);
		NkFontConfig fusion; // repli par defaut : '?'
		fusion.mergeMode = true;
		(void)atlas->AddFontFromMemory(octets, taille, 18.f, &fusion);
		fusionne = Lire(*atlas, base);
		delete atlas;
	}
	Dire("repli X + une police fusionnee", fusionne);

	std::printf("\n");
	Temoin(reference.construit && reference.nonNuls > 0 && reference.avanceA > 0.f,
		   "la reference se construit, porte de l'encre et fait avancer la lettre A");
	Temoin(nulle.construit && nulle.nonNuls == 0,
		   "NEGATIF : un multiplicateur nul demande expres donne un atlas sans un octet non nul");
	Temoin(sans.construit && sans.nonNuls > 0, "sans configuration : l'atlas porte de l'encre");
	Temoin(sans.avanceA > 0.f && sans.avanceI > 0.f, "sans configuration : les lettres A et i avancent");
	Temoin(sans.repli == 0x3Fu, "sans configuration : le glyphe de repli est le point d'interrogation");
	Temoin(Identiques(sans, reference),
		   "sans configuration = configuration par defaut, a l'octet pres (somme, compte, taille, avances, repli)");
	Temoin(Identiques(reutilise, reference),
		   "un emplacement reutilise apres Clear() ne garde rien de la police d'avant (identique a la reference)");
	Temoin(reference.repli == 0x3Fu, "la reference a pour glyphe de repli le point d'interrogation");
	Temoin(choisi.repli == 'X', "un repli choisi (X) est honore");
	Temoin(fusionne.repli == 'X', "une police fusionnee ne remplace pas le repli de sa base");

	NkTestsAtlasSdf(&Temoin, octets, taille); // (14/10) l'atlas SDF
	NkFontEmbedded::FreeDecompressedData(octets);

	std::printf("\n===== NKFont_Tests : %d / %d — %s =====\n", gVerts, gVerts + gRouges, gRouges == 0 ? "VERT" : "ROUGE");
	return gRouges == 0 ? 0 : 1;
}
