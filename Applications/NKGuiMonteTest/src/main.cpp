// =============================================================================
// NKGuiMonteTest — LE BANC QUI FAIT MONTER LE FORMAT `.nkgui`.
//
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// Quatre criteres, chacun avec son NEGATIF, sur le corpus ecrit a la main de
// `Applications/NKUIDesign/exemples/` :
//
//   (m0) LE ZERO DU COMPTEUR DE PIXELS, prouve AVANT tout le reste.
//   (m1) l'analyseur lit les huit sections -- attendus ecrits avant la mesure.
//   (m2) les onze fautifs sont refuses, ET LA FAMILLE COMPTE.
//   (m3) ca dessine -- rendu hors ecran, pixels comptes.
//   (m4) l'agencement est celui du fichier, pas un hasard.
//
// ⚠️ AUCUNE FENETRE N'EST OUVERTE. Le rendu passe par `NkGuiDrawListRaster`, le
//    rasteriseur logiciel de NKGui : ni fenetre, ni device, ni GPU. La garde
//    « aucune capture de l'ecran, aucune injection d'entree » est donc tenue par
//    CONSTRUCTION, pas par discipline -- il n'y a rien a ouvrir ni a cliquer.
//
// Sortie : `n/n` + code de sortie (0 = tout passe, 1 = au moins un echec).
// =============================================================================

#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Doc/NkGuiMonteur.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
#include "NKMemory/NkAllocator.h"
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKImage/Core/NkImage.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"
#include "NKUIDesign/NkGuiValidate.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::nkgui;

static int g_pass = 0, g_fail = 0;

static void Check(bool ok, const char *name) {
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s\n", ok ? "OK" : "KO", name);
}

static void CheckEq(uint32 lu, uint32 attendu, const char *name) {
	const bool ok = (lu == attendu);
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s  (attendu %u, lu %u)\n", ok ? "OK" : "KO", name, attendu, lu);
}

// ── Lecture de fichier, sans dependre d'un module de plus ───────────────────
struct Fichier {
		char *data = nullptr;
		uint32 taille = 0;
		bool ok = false;
};

static Fichier Lire(const char *chemin) {
	Fichier f;
	FILE *h = fopen(chemin, "rb");
	if (!h)
		return f;
	fseek(h, 0, SEEK_END);
	const long n = ftell(h);
	fseek(h, 0, SEEK_SET);
	if (n < 0) {
		fclose(h);
		return f;
	}
	f.data = (char *)malloc((size_t)n + 1u);
	if (!f.data) {
		fclose(h);
		return f;
	}
	const size_t lu = fread(f.data, 1, (size_t)n, h);
	fclose(h);
	f.data[lu] = '\0';
	f.taille = (uint32)lu;
	f.ok = true;
	return f;
}

static void Liberer(Fichier &f) {
	if (f.data)
		free(f.data);
	f.data = nullptr;
	f.ok = false;
}

static void Joindre(char *out, uint32 taille, const char *a, const char *b) {
	uint32 i = 0;
	for (; a[i] && i + 1u < taille; ++i)
		out[i] = a[i];
	uint32 j = 0;
	for (; b[j] && i + 1u < taille; ++j, ++i)
		out[i] = b[j];
	out[i] = '\0';
}

// ── Le compteur de pixels, et sa definition exacte ──────────────────────────
// Compte les pixels DIFFERENTS du fond pose avant le rendu. C'est la seule
// definition qui a un zero prouvable : « non transparent » n'en a pas, une page
// au fond opaque etant entierement non transparente avant le premier widget.
static uint32 ComptePixelsPeints(const NkGuiDrawListRaster &r, uint32 fond) {
	uint32 n = 0;
	for (int32 y = 0; y < r.Hauteur(); ++y)
		for (int32 x = 0; x < r.Largeur(); ++x)
			if (r.Pixel(x, y) != fond)
				++n;
	return n;
}

// La couleur la PLUS FREQUENTE de l'image -- l'aplat de fond d'un panneau qui
// couvre la region.
//
// ⚠️ LA PREMIERE VERSION PRENAIT `Pixel(0, 0)`, ET ELLE ETAIT VERTE POUR RIEN.
//    `PanelBackground` peint « un rectangle theme + UN BORD » : le coin
//    haut-gauche porte donc la couleur du BORD, pas celle de l'aplat. Le
//    compteur ne retranchait rien, et rendait `contenu == pixels` (239980 des
//    deux cotes) -- exactement le temoin muet qu'il pretendait remplacer.
//    La couleur dominante, elle, est l'aplat par construction.
static uint32 CouleurDominante(const NkGuiDrawListRaster &r) {
	// Une interface a peu de couleurs distinctes : une petite table suffit, et
	// ce qui deborde ne peut pas etre dominant (il faudrait plus de 64 aplats).
	uint32 couleurs[64];
	uint32 comptes[64];
	uint32 n = 0;
	for (int32 y = 0; y < r.Hauteur(); ++y)
		for (int32 x = 0; x < r.Largeur(); ++x) {
			const uint32 p = r.Pixel(x, y);
			uint32 i = 0;
			for (; i < n; ++i)
				if (couleurs[i] == p) {
					++comptes[i];
					break;
				}
			if (i == n && n < 64u) {
				couleurs[n] = p;
				comptes[n] = 1u;
				++n;
			}
		}
	uint32 best = 0, bestN = 0;
	for (uint32 i = 0; i < n; ++i)
		if (comptes[i] > bestN) {
			bestN = comptes[i];
			best = couleurs[i];
		}
	return best;
}

// Compte les pixels qui ne sont NI le fond de la cible NI la couleur d'aplat.
// Sans cette seconde exclusion, `01_panneau_reglages` annonce 239980 px sur
// 240000 : le compteur mesure alors le fond du panneau et resterait vert meme si
// aucun widget ne dessinait. Un temoin qui ne peut pas rougir ne temoigne de rien.
static uint32 ComptePixelsContenu(const NkGuiDrawListRaster &r, uint32 fond, uint32 aplat) {
	uint32 n = 0;
	for (int32 y = 0; y < r.Hauteur(); ++y)
		for (int32 x = 0; x < r.Largeur(); ++x) {
			const uint32 p = r.Pixel(x, y);
			if (p != fond && p != aplat)
				++n;
		}
	return n;
}

// Empreinte d'une image : deux images identiques AU BIT ont la meme, et deux
// images qui different en ont une differente (somme de controle FNV-1a 32 bits).
static uint32 Empreinte(const NkGuiDrawListRaster &r) {
	uint32 h = 2166136261u;
	const uint8 *p = r.Pixels();
	const uint32 n = (uint32)(r.Largeur() * r.Hauteur() * 4);
	for (uint32 i = 0; i < n; ++i) {
		h ^= (uint32)p[i];
		h *= 16777619u;
	}
	return h;
}

static float32 Abs(float32 v) {
	return v < 0.f ? -v : v;
}

// ── Le montage complet d'un texte `.nkgui`, de bout en bout ─────────────────
struct Montage {
		bool lu = false;
		NkGuiDiag err;
		NkGuiMonteRapport rap;
		uint32 pixels = 0;	   ///< pixels differents du fond de la cible
		uint32 contenu = 0;	   ///< pixels qui ne sont NI le fond NI l'aplat de panneau
		uint32 empreinte = 0;
		uint32 texInconnues = 0; ///< commandes texturees dont la texture manquait
};

// ── LA POLICE, et pourquoi ce banc en charge une ──────────────────────────────
// Sans police, `Text()` place son rectangle mais ne peint AUCUN glyphe : le
// premier releve de ce banc rendait 0 pixel pour `04_echappements_utf8` et
// `09_indentation_a_la_main`, les deux fichiers dont tout le contenu visible est
// du texte. Ce n'etait pas un defaut du monteur -- c'etait le banc qui mesurait
// une interface sans police et concluait qu'elle ne dessine pas.
// La police est EMBARQUEE (aucun fichier a trouver, aucun chemin fragile), et son
// atlas est declare au rasteriseur : sans ca les commandes texturees se peindraient
// sans texture, et `Rasteriser` le compte -- c'est le controle `texInconnues`.
static NkGuiFont g_font;
static bool g_fontOk = false;

static const uint32 kFond = 0x101418FFu;

// Dossier de sortie des images (option `--png=`), vide = on n'ecrit rien.
static const char *g_pngDir = nullptr;

// Ecrit la cible rasterisee en PNG. Le tampon rendu par `Encode` est alloue par
// NkAlloc : il se libere par `nkentseu::memory::NkFree`, jamais par free() --
// c'est un `c0000374` connu du depot.
static bool EcrirePng(const NkGuiDrawListRaster &r, const char *chemin) {
	NkImage img = NkImage::Alloc(r.Largeur(), r.Hauteur(), NkImagePixelFormat::NK_RGBA32);
	if (!img.Pixels())
		return false;
	const uint8 *src = r.Pixels();
	uint8 *dst = img.Pixels();
	const usize n = (usize)r.Largeur() * (usize)r.Hauteur() * 4u;
	for (usize i = 0; i < n; ++i)
		dst[i] = src[i];

	uint8 *out = nullptr;
	usize taille = 0;
	if (!NkPNGCodec::Encode(img, out, taille) || !out)
		return false;
	FILE *h = fopen(chemin, "wb");
	bool ok = false;
	if (h) {
		ok = (fwrite(out, 1, (size_t)taille, h) == (size_t)taille);
		fclose(h);
	}
	nkentseu::memory::NkFree(out);
	return ok;
}

static Montage MonterTexte(const char *src, uint32 len, int32 w, int32 h,
						   const char *nomPng = nullptr) {
	Montage m;
	NkArchive doc;
	if (!NkGuiArchive::Read(src, len, doc, m.err))
		return m;
	m.lu = true;

	NkGuiContext ctx;
	ctx.viewW = w;
	ctx.viewH = h;
	if (g_fontOk)
		ctx.font = &g_font;
	const NkRect region{0.f, 0.f, (float32)w, (float32)h};
	ctx.BeginFrame(0.016f);
	ctx.BeginLayout(region);
	ctx.DL().Reset();

	NkGuiMonteEtat etat;
	NkGuiMonteur::Preparer(doc, etat);
	NkGuiMonteur::Monter(ctx, doc, etat, m.rap);

	NkGuiDrawListRaster ras;
	if (ras.Init(w, h)) {
		ras.Effacer(kFond);
		if (g_fontOk && g_font.pixels)
			ras.PoserTexture(g_font.TexId(), g_font.pixels, g_font.atlasW, g_font.atlasH, 1);
		m.texInconnues = ras.Rasteriser(ctx.DL());
		m.pixels = ComptePixelsPeints(ras, kFond);
		// L'aplat est DERIVE de l'image (sa couleur dominante), jamais ecrit en
		// dur et jamais devine sur un coin -- voir CouleurDominante.
		m.contenu = ComptePixelsContenu(ras, kFond, CouleurDominante(ras));
		m.empreinte = Empreinte(ras);
		if (g_pngDir && nomPng) {
			char sortie[1024];
			Joindre(sortie, sizeof(sortie), g_pngDir, nomPng);
			if (EcrirePng(ras, sortie))
				printf("        image ecrite : %s\n", sortie);
			else
				printf("        ECHEC d'ecriture : %s\n", sortie);
		}
	}
	return m;
}

static Montage MonterFichier(const char *chemin, int32 w, int32 h,
							 const char *nomPng = nullptr) {
	Fichier f = Lire(chemin);
	if (!f.ok) {
		Montage m;
		return m;
	}
	Montage m = MonterTexte(f.data, f.taille, w, h, nomPng);
	Liberer(f);
	return m;
}

// ── Le corpus, et MES ATTENDUS -- ecrits AVANT la mesure, comptes a la main ──
struct Attendu {
		const char *nom;
		uint32 widgets;
		uint32 behaviors;
		uint32 animations;
		uint32 apparences;	///< blocs `appearance`, toutes graphies confondues
		uint32 etatsHorsRepos; ///< ceux qui ne sont ni `appearance` nu ni `(Normal)`
};

static const Attendu kValides[] = {
	{"01_panneau_reglages.nkgui", 11u, 0u, 0u, 0u, 0u},
	{"02_bloc_sur_une_ligne.nkgui", 8u, 0u, 0u, 0u, 0u},
	{"03_virgule_vecteur_couleur.nkgui", 4u, 0u, 0u, 0u, 0u},
	{"04_echappements_utf8.nkgui", 4u, 0u, 0u, 0u, 0u},
	{"05_animation_comportement.nkgui", 2u, 1u, 1u, 0u, 0u},
	{"06_version_0_2_alias.nkgui", 1u, 0u, 0u, 0u, 0u},
	// 07 : `appearance` nu + (Hover) + (Disabled) -- le nu EST le repos.
	{"07_apparence.nkgui", 1u, 0u, 0u, 3u, 2u},
	{"08_indentation_mixte.nkgui", 1u, 1u, 0u, 0u, 0u},
	{"09_indentation_a_la_main.nkgui", 2u, 1u, 0u, 0u, 0u},
	// 10 : valider 5, annuler 2 (dont (Normal) = repos), recherche 1.
	{"10_etats_apparence.nkgui", 3u, 0u, 0u, 8u, 6u},
};
static const uint32 kNbValides = 10u;

static const char *kRefusesLecture[] = {
	"r1_echappement_inconnu.nkgui", "r2_accolade_jamais_fermee.nkgui", "r4_entete_nkgui_manquant.nkgui",
	"r5_commentaire_jamais_ferme.nkgui", "r7_valeur_manquante.nkgui",
};
static const uint32 kNbRefusesLecture = 5u;

static const char *kSignalesValidation[] = {
	"v1_couleur_cinq_chiffres.nkgui", "v2_virgule_finale_dans_liste.nkgui",
	"v3_cle_de_dictionnaire_invalide.nkgui", "v4_section_inconnue.nkgui",
	"v5_valeur_bien_formee_mauvais_type.nkgui", "v6_role_inconnu.nkgui",
};
static const uint32 kNbSignalesValidation = 6u;

int main(int argc, char **argv) {
	const char *racine = "Applications/NKUIDesign/exemples";
	for (int32 a = 1; a < (int32)argc; ++a) {
		// `--png=<dossier>` : ecrire une image par fichier monte. OPT-IN.
		if (argv[a][0] == '-' && argv[a][1] == '-' && argv[a][2] == 'p' && argv[a][3] == 'n'
			&& argv[a][4] == 'g' && argv[a][5] == '=')
			g_pngDir = argv[a] + 6;
		else if (argv[a][0] != '-')
			racine = argv[a];
	}
	printf("=== NKGuiMonteTest : le format .nkgui MONTE et DESSINE (sans fenetre, sans GPU) ===\n");
	printf("    corpus : %s\n", racine);

	char chemin[1024];
	char dossier[1024];

	// =====================================================================
	// (m0) LE ZERO DU COMPTEUR DE PIXELS -- AVANT TOUT LE RESTE
	// =====================================================================
	// La police, chargee AVANT toute mesure.
	g_fontOk = g_font.LoadEmbedded(NkEmbeddedFontId::DroidSans, 15.f, false);
	printf("    police embarquee : %s (atlas %dx%d)\n", g_fontOk ? "chargee" : "ABSENTE",
		   g_font.atlasW, g_font.atlasH);

	printf("\n-- (m0) le compteur de pixels prouve son ZERO, puis son ECHELLE\n");
	{
		NkGuiDrawListRaster ras;
		Check(g_fontOk && g_font.Valid(),
			  "la police embarquee est chargee (sans elle, un Text ne peint rien)");
		Check(ras.Init(64, 64), "la cible hors ecran s'alloue (64x64)");
		ras.Effacer(kFond);

		// ZERO : une liste de dessin VIDE ne peint rien. Un compteur qui rend
		// autre chose que 0 ici rendrait n'importe quoi partout ailleurs.
		NkGuiDrawList vide;
		vide.Reset();
		(void)ras.Rasteriser(vide);
		CheckEq(ComptePixelsPeints(ras, kFond), 0u, "ZERO : liste vide -> 0 pixel peint");

		// ECHELLE : un rectangle de 10x10 pose a (5,5) peint EXACTEMENT 100 px.
		// Un compteur juste sur zero peut encore etre faux d'un facteur.
		NkGuiDrawList un;
		un.Reset();
		un.AddRectFilled(NkRect{5.f, 5.f, 10.f, 10.f}, NkColor{255, 128, 0, 255});
		(void)ras.Rasteriser(un);
		CheckEq(ComptePixelsPeints(ras, kFond), 100u, "ECHELLE : un rect 10x10 -> exactement 100 px");

		// ET LE FOND N'EST PAS LA COULEUR DU RECT : sinon le compteur mesurerait
		// la couleur, pas la couverture.
		Check(ras.Pixel(6, 6) != kFond, "le pixel interieur a bien change");
		Check(ras.Pixel(60, 60) == kFond, "le pixel exterieur est reste le fond");
	}

	// =====================================================================
	// (m1) L'ANALYSEUR LIT LES HUIT SECTIONS
	// =====================================================================
	printf("\n-- (m1) les dix valides s'analysent, et le compte est celui du fichier\n");
	uint32 totW = 0, totB = 0, totA = 0, totMontes = 0;
	uint32 totApp = 0, totEtats = 0, totInconnus = 0;
	for (uint32 i = 0; i < kNbValides; ++i) {
		Joindre(dossier, sizeof(dossier), racine, "/valides/");
		Joindre(chemin, sizeof(chemin), dossier, kValides[i].nom);
		const Montage m = MonterFichier(chemin, 400, 600);
		printf("   %s\n", kValides[i].nom);
		Check(m.lu, "   se lit");
		if (!m.lu) {
			printf("        refus : %s %s\n", m.err.code.CStr(), m.err.message.CStr());
			continue;
		}
		CheckEq(m.rap.widgets, kValides[i].widgets, "   widgets");
		CheckEq(m.rap.behaviors, kValides[i].behaviors, "   behaviors");
		CheckEq(m.rap.animations, kValides[i].animations, "   animations");
		CheckEq(m.rap.apparencesLues, kValides[i].apparences, "   blocs d'apparence");
		CheckEq(m.rap.etatsNonAppliques, kValides[i].etatsHorsRepos,
				"   etats hors repos (lus, comptes, JAMAIS peints)");
		// ⚠️ CE CONTROLE-CI EXISTE PARCE QU'UNE MUTATION A SURVECU. Faire compter
		//    les blocs `appearance` comme des widgets ne faisait rougir personne :
		//    ils tombaient dans `rolesInconnus`, un compteur que le banc ne lisait
		//    nulle part. Un compteur que personne ne regarde ne garde rien.
		CheckEq(m.rap.rolesInconnus, 0u, "   aucun role hors vocabulaire");
		totW += m.rap.widgets;
		totB += m.rap.behaviors;
		totA += m.rap.animations;
		totMontes += m.rap.montes;
		totApp += m.rap.apparencesLues;
		totEtats += m.rap.etatsNonAppliques;
		totInconnus += m.rap.rolesInconnus;
	}
	printf("\n   TOTAL corpus valide\n");
	CheckEq(totW, 37u, "   37 widgets sur les dix fichiers");
	CheckEq(totB, 3u, "   3 behaviors");
	CheckEq(totA, 1u, "   1 animation");
	CheckEq(totApp, 11u, "   11 blocs d'apparence (07 en porte 3, 10 en porte 8)");
	CheckEq(totEtats, 8u, "   8 etats hors repos, lus et comptes, aucun peint");
	CheckEq(totInconnus, 0u, "   zero role hors vocabulaire sur tout le corpus");

	// NEGATIF de (m1) : un fichier VIDE (l'en-tete seul) est un document VALIDE
	// a zero widget -- pas un plantage, pas un refus.
	printf("\n   negatif (m1) : un document sans widget\n");
	{
		const char *vide = "nkgui 0.3\n";
		const Montage m = MonterTexte(vide, 10u, 200, 200);
		Check(m.lu, "   l'en-tete seul se lit (document valide)");
		CheckEq(m.rap.widgets, 0u, "   zero widget");
		CheckEq(m.pixels, 0u, "   et il ne peint rien");
	}

	// =====================================================================
	// (m2) LES ONZE FAUTIFS SONT REFUSES, ET LA FAMILLE COMPTE
	// =====================================================================
	printf("\n-- (m2) `refuses_a_la_lecture/` : l'analyse ECHOUE, et elle NOMME sa raison\n");
	uint32 refusesOk = 0;
	for (uint32 i = 0; i < kNbRefusesLecture; ++i) {
		Joindre(dossier, sizeof(dossier), racine, "/fautifs/refuses_a_la_lecture/");
		Joindre(chemin, sizeof(chemin), dossier, kRefusesLecture[i]);
		Fichier f = Lire(chemin);
		if (!f.ok) {
			Check(false, kRefusesLecture[i]);
			continue;
		}
		NkArchive doc;
		NkGuiDiag err;
		const bool lu = NkGuiArchive::Read(f.data, f.taille, doc, err);
		const bool raisonNommee = !lu && err.code.Size() > 0 && err.message.Size() > 0;
		printf("   %s\n", kRefusesLecture[i]);
		Check(!lu, "   REFUSE a la lecture");
		Check(raisonNommee, "   la raison est nommee");
		if (raisonNommee) {
			printf("        %s ligne %u colonne %u : %s\n", err.code.CStr(), err.line, err.column,
				   err.message.CStr());
			++refusesOk;
		}
		Liberer(f);
	}
	CheckEq(refusesOk, kNbRefusesLecture, "   les 5 refus a la lecture, chacun avec sa raison");

	printf("\n-- (m2) `signales_par_la_validation/` : ils SE LISENT, et la faute est SIGNALEE\n");
	printf("        (confondre les deux familles serait aussi faux que de les accepter)\n");
	uint32 signalesOk = 0;
	for (uint32 i = 0; i < kNbSignalesValidation; ++i) {
		Joindre(dossier, sizeof(dossier), racine, "/fautifs/signales_par_la_validation/");
		Joindre(chemin, sizeof(chemin), dossier, kSignalesValidation[i]);
		Fichier f = Lire(chemin);
		if (!f.ok) {
			Check(false, kSignalesValidation[i]);
			continue;
		}
		NkArchive doc;
		NkGuiDiag err;
		const bool lu = NkGuiArchive::Read(f.data, f.taille, doc, err);
		printf("   %s\n", kSignalesValidation[i]);
		Check(lu, "   SE LIT (il doit rester ouvrable pour etre reparable)");
		if (lu) {
			NkVector<NkGuiDiag> diags;
			const nkuidesign::guifmt::NkGValidateResult r = nkuidesign::guifmt::NkGValidate(doc, diags);
			Check(r.errors > 0u, "   et la validation le SIGNALE");
			if (r.errors > 0u) {
				for (uint32 k = 0; k < (uint32)diags.Size(); ++k)
					printf("        %s ligne %u : %s\n", diags[k].code.CStr(), diags[k].line,
						   diags[k].message.CStr());
				++signalesOk;
			}
		}
		Liberer(f);
	}
	CheckEq(signalesOk, kNbSignalesValidation, "   les 6 signalements, chacun avec sa raison");

	// NEGATIF de (m2) : un fichier VALIDE ne doit declencher AUCUNE erreur. Sans
	// cette moitie, une validation qui crierait sur tout serait verte ci-dessus.
	printf("\n   negatif (m2) : les dix valides ne declenchent AUCUNE erreur\n");
	{
		uint32 erreursSurValides = 0, avertissements = 0;
		for (uint32 i = 0; i < kNbValides; ++i) {
			Joindre(dossier, sizeof(dossier), racine, "/valides/");
			Joindre(chemin, sizeof(chemin), dossier, kValides[i].nom);
			Fichier f = Lire(chemin);
			if (!f.ok)
				continue;
			NkArchive doc;
			NkGuiDiag err;
			if (NkGuiArchive::Read(f.data, f.taille, doc, err)) {
				NkVector<NkGuiDiag> diags;
				const nkuidesign::guifmt::NkGValidateResult r =
					nkuidesign::guifmt::NkGValidate(doc, diags);
				if (r.errors > 0u) {
					printf("      %s : %u erreur(s)\n", kValides[i].nom, r.errors);
					for (uint32 k = 0; k < (uint32)diags.Size(); ++k)
						printf("        %s : %s\n", diags[k].code.CStr(), diags[k].message.CStr());
				}
				erreursSurValides += r.errors;
				avertissements += r.warnings;
			}
			Liberer(f);
		}
		CheckEq(erreursSurValides, 0u, "   zero erreur sur le corpus valide");
		// Deux avertissements sont VOULUS et documentes : W-ROLE-ALIAS (06) et
		// W-FOCUS-ANNEAU (10). Les exiger, c'est refuser qu'ils disparaissent en
		// silence -- un avertissement perdu ne se remarque jamais.
		CheckEq(avertissements, 2u, "   exactement 2 avertissements VOULUS (06 alias, 10 focus)");
	}

	// =====================================================================
	// (m3) CA DESSINE
	// =====================================================================
	printf("\n-- (m3) le document monte et PEINT des pixels\n");
	Joindre(dossier, sizeof(dossier), racine, "/valides/");
	Joindre(chemin, sizeof(chemin), dossier, "01_panneau_reglages.nkgui");
	const Montage m01 = MonterFichier(chemin, 400, 600);
	{
		Check(m01.lu, "   01_panneau_reglages se lit");
		CheckEq(m01.rap.montes, 11u, "   les 11 widgets ont TOUS appele une fonction NKGui");
		Check(m01.pixels > 0u, "   il peint des pixels");
		printf("        pixels peints : %u sur %u ;  dont CONTENU (hors aplat) : %u\n",
			   m01.pixels, 400u * 600u, m01.contenu);
		// Le compte brut ne dit presque rien ici : le `Panel` couvre toute la region,
		// donc 239980 px sur 240000 resteraient verts meme si AUCUN widget ne
		// dessinait. C'est le CONTENU -- ce qui n'est ni le fond de la cible ni
		// l'aplat du panneau -- qui temoigne.
		Check(m01.contenu > 0u, "   et du CONTENU se peint PAR-DESSUS l'aplat du panneau");
		CheckEq(m01.texInconnues, 0u, "   aucune commande texturee sans sa texture");
	}

	// Les dix fichiers montent. Un seul qui dessine ne prouve que lui.
	printf("\n   les dix fichiers du corpus valide montent\n");
	{
		uint32 dessinent = 0, texManquantes = 0;
		for (uint32 i = 0; i < kNbValides; ++i) {
			Joindre(dossier, sizeof(dossier), racine, "/valides/");
			Joindre(chemin, sizeof(chemin), dossier, kValides[i].nom);
			char nomPng[256];
			Joindre(nomPng, sizeof(nomPng), kValides[i].nom, ".png");
			const Montage m = MonterFichier(chemin, 400, 600, nomPng);
			if (m.lu && m.contenu > 0u)
				++dessinent;
			texManquantes += m.texInconnues;
			printf("      %-36s %7u px  contenu %6u  %2u/%2u montes\n", kValides[i].nom,
				   m.pixels, m.contenu, m.rap.montes, m.rap.widgets);
		}
		CheckEq(dessinent, kNbValides, "   les 10 peignent du CONTENU");
		CheckEq(texManquantes, 0u, "   et aucune n'a peint une texture absente");
	}

	// =====================================================================
	// (m4) L'AGENCEMENT EST CELUI DU FICHIER
	// =====================================================================
	// NEGATIF du compteur de CONTENU : un panneau SEUL, sans un widget dedans.
	//
	// ⚠️ L'ATTENDU A ETE CORRIGE PAR LA MESURE, ET C'EST L'ATTENDU QUI AVAIT TORT.
	//    Il exigeait ZERO ; le banc a rendu 1988. 1988 n'est pas un bruit : c'est
	//    le PERIMETRE du panneau (2 x (400 + 600) = 2000, moins les coins).
	//    `PanelBackground` peint « un rectangle theme + UN BORD », et ce bord
	//    n'est ni le fond de la cible ni l'aplat -- il EST du contenu, a juste
	//    titre. L'attendu est donc derive de la geometrie : un panneau vide peint
	//    son contour, et rien de la surface.
	printf("\n   negatif (m3) : un Panel VIDE peint son aplat et son CONTOUR, rien de plus\n");
	{
		const char *seul = "nkgui 0.3\nwidgets {\n  Panel \"p\" { }\n}\n";
		uint32 n = 0;
		while (seul[n])
			++n;
		const Montage m = MonterTexte(seul, n, 400, 600);
		Check(m.lu, "   le document se lit");
		CheckEq(m.rap.widgets, 1u, "   un seul widget");
		Check(m.pixels > 0u, "   l'aplat du panneau EST peint");
		const uint32 perimetre = 2u * (400u + 600u);
		printf("        contenu d'un panneau vide : %u px (perimetre attendu ~%u)\n",
			   m.contenu, perimetre);
		Check(m.contenu > 0u && m.contenu <= perimetre,
			  "   le contenu d'un panneau VIDE ne depasse pas son CONTOUR");
		// Et la moitie qui compte : le document complet en peint bien davantage.
		// Sans elle, un compteur bloque sur une petite valeur passerait aussi.
		Check(m01.contenu > 5u * m.contenu,
			  "   le document COMPLET peint 5x plus de contenu qu'un panneau vide");
	}

	printf("\n-- (m4) l'agencement vient du fichier : la VBox `colonne` a gap = 8\n");
	{
		// L'attendu est DERIVE, pas recopie : on relit le `gap` dans le fichier et
		// on exige que l'ecart entre deux rangees consecutives de la VBox le
		// contienne. Un attendu en dur se perimerait a la premiere retouche du
		// theme (ItemHeight n'est pas une constante de ce banc).
		Joindre(dossier, sizeof(dossier), racine, "/valides/");
		Joindre(chemin, sizeof(chemin), dossier, "01_panneau_reglages.nkgui");
		Fichier f = Lire(chemin);
		Check(f.ok, "   le fichier se lit");
		if (f.ok) {
			NkArchive doc;
			NkGuiDiag err;
			if (NkGuiArchive::Read(f.data, f.taille, doc, err)) {
				// le `gap` ECRIT dans le fichier, retrouve dans l'archive
				float32 gapEcrit = -1.f;
				const NkArchiveNode *corps = NkGMonteCorps(doc);
				if (corps)
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const NkArchiveNode *w = NkGMonteCorps(*corps->array[i].object);
						if (!w)
							continue;
						for (uint32 k = 0; k < (uint32)w->array.Size(); ++k) {
							if (!w->array[k].IsObject() || !w->array[k].object)
								continue;
							const NkArchiveNode *p = NkGMonteCorps(*w->array[k].object);
							if (!p)
								continue;
							for (uint32 j = 0; j < (uint32)p->array.Size(); ++j)
								if (p->array[j].IsObject() && p->array[j].object)
									gapEcrit = NkGNombre(*p->array[j].object, "gap", -1.f);
						}
					}
				printf("        gap lu DANS le fichier : %.1f\n", (double)gapEcrit);
				Check(Abs(gapEcrit - 8.f) < 0.001f, "   le gap lu dans le fichier vaut bien 8");
			}
			Liberer(f);
		}

		// Les rangees sont empilees, dans l'ordre du fichier, sans se chevaucher.
		// ⚠️ L'EMPILEMENT NE SE MESURE QUE SOUS UN AXE VERTICAL. Le premier releve
		//    comptait « 1 chevauchement » sur un document correct : les deux
		//    boutons de la `HBox "actions"` PARTAGENT leur y par construction,
		//    c'est ce qu'une rangee horizontale veut dire. Mesurer un empilement
		//    dessus, c'est mesurer la mauvaise chose et appeler defaut ce qui est
		//    le comportement demande par le fichier.
		uint32 empilees = 0, chevauchements = 0;
		float32 precBas = -1.f;
		for (uint32 i = 0; i < (uint32)m01.rap.items.Size(); ++i) {
			const NkGuiMonteItem &it = m01.rap.items[i];
			if (it.conteneur || it.rect.h <= 0.f || it.axeHorizontal)
				continue;
			if (precBas >= 0.f) {
				if (it.rect.y + 0.001f < precBas)
					++chevauchements;
				else
					++empilees;
			}
			precBas = it.rect.y + it.rect.h;
		}
		printf("        rangees empilees : %u, chevauchements : %u\n", empilees, chevauchements);
		Check(empilees > 0u, "   les rangees s'empilent dans l'ordre du fichier");
		CheckEq(chevauchements, 0u, "   et AUCUNE ne chevauche la precedente");

		// ── (a) LA LOI D'EMPILEMENT, ENONCEE EN ENTIER ────────────────────────
		// y(i+1) - (y(i) + h(i)) == gap, pour toutes les rangees consecutives de
		// la colonne -- pas seulement pour la premiere paire. Le `gap` vient du
		// fichier (lu ci-dessus), la profondeur vient du rapport : rien en dur.
		{
			uint32 paires = 0, conformes = 0;
			const NkGuiMonteItem *prec = nullptr;
			for (uint32 i = 0; i < (uint32)m01.rap.items.Size(); ++i) {
				const NkGuiMonteItem &it = m01.rap.items[i];
				if (it.conteneur || it.axeHorizontal || it.rect.h < 0.f)
					continue;
				if (prec && prec->profondeur == it.profondeur) {
					++paires;
					const float32 ecart = it.rect.y - (prec->rect.y + prec->rect.h);
					if (Abs(ecart - 8.f) < 0.01f)
						++conformes;
					else
						printf("        rangee '%s' : ecart %.2f (attendu 8.00 = le gap du fichier)\n",
							   it.id.CStr(), (double)ecart);
				}
				prec = &it;
			}
			printf("        loi d'empilement : %u paires conformes sur %u\n", conformes, paires);
			Check(paires > 0u, "   la colonne a des rangees consecutives a comparer");
			CheckEq(conformes, paires, "   TOUTES respectent y(i+1) = y(i) + h(i) + gap");
		}

		// ── (b) LE SPACER PREND LA TAILLE ECRITE ──────────────────────────────
		// `Spacer "vide" { size = 12 }`. La mutation M7 (le monteur ignore `size`)
		// ne faisait rougir personne avant ce controle.
		{
			bool trouve = false;
			float32 hauteur = -1.f;
			for (uint32 i = 0; i < (uint32)m01.rap.items.Size(); ++i) {
				const NkGuiMonteItem &it = m01.rap.items[i];
				if (it.role.Compare(NkString("Spacer")) == 0) {
					trouve = true;
					hauteur = it.rect.h;
					break;
				}
			}
			Check(trouve, "   le Spacer du fichier est bien monte");
			printf("        Spacer 'vide' : hauteur %.2f (le fichier ecrit size = 12)\n",
				   (double)hauteur);
			Check(trouve && Abs(hauteur - 12.f) < 0.01f,
				  "   et il prend EXACTEMENT les 12 px ecrits dans le fichier");
		}

		// LA MUTATION DEMANDEE : une seule valeur change dans le fichier
		// (`gap = 8` -> `gap = 0`), et la mesure doit BOUGER de la quantite
		// attendue -- 8 px de moins entre deux rangees de la VBox.
		printf("\n   negatif (m4) : `gap = 8` -> `gap = 0`, la mesure DOIT bouger\n");
		Fichier f2 = Lire(chemin);
		if (f2.ok) {
			// substitution du seul `gap = 8` du document
			for (uint32 i = 0; i + 8u < f2.taille; ++i) {
				if (f2.data[i] == 'g' && f2.data[i + 1] == 'a' && f2.data[i + 2] == 'p'
					&& f2.data[i + 3] == ' ' && f2.data[i + 4] == '=' && f2.data[i + 5] == ' '
					&& f2.data[i + 6] == '8') {
					f2.data[i + 6] = '0';
					break;
				}
			}
			const Montage mut = MonterTexte(f2.data, f2.taille, 400, 600);
			Check(mut.lu, "   le fichier mute se lit encore");
			CheckEq(mut.rap.widgets, 11u, "   il porte toujours 11 widgets (seul le gap a change)");
			Check(mut.empreinte != m01.empreinte, "   l'IMAGE a change (empreinte differente)");

			// L'ecart entre deux rangees consecutives a diminue d'exactement 8 px.
			float32 ecart0 = -1.f, ecart1 = -1.f;
			float32 prec = -1.f;
			for (uint32 i = 0; i < (uint32)m01.rap.items.Size(); ++i) {
				const NkGuiMonteItem &it = m01.rap.items[i];
				if (it.conteneur || it.rect.h <= 0.f || it.axeHorizontal)
					continue;
				if (prec >= 0.f && ecart0 < 0.f)
					ecart0 = it.rect.y - prec;
				prec = it.rect.y;
			}
			prec = -1.f;
			for (uint32 i = 0; i < (uint32)mut.rap.items.Size(); ++i) {
				const NkGuiMonteItem &it = mut.rap.items[i];
				if (it.conteneur || it.rect.h <= 0.f || it.axeHorizontal)
					continue;
				if (prec >= 0.f && ecart1 < 0.f)
					ecart1 = it.rect.y - prec;
				prec = it.rect.y;
			}
			printf("        ecart entre rangees : gap=8 -> %.2f ;  gap=0 -> %.2f ;  delta %.2f\n",
				   (double)ecart0, (double)ecart1, (double)(ecart0 - ecart1));
			Check(ecart0 > 0.f && ecart1 > 0.f && Abs((ecart0 - ecart1) - 8.f) < 0.01f,
				  "   l'ecart a diminue d'EXACTEMENT 8 px, la quantite ecrite dans le fichier");
			Liberer(f2);
		}

		// NEGATIF DE L'EMPREINTE : deux montages du MEME texte doivent rendre
		// deux images identiques AU BIT. Sans ce controle, l'empreinte pourrait
		// differer a chaque passage et le test ci-dessus serait vert pour rien.
		printf("\n   negatif (m4) : deux montages du meme fichier -> identiques AU BIT\n");
		{
			const Montage a = MonterFichier(chemin, 400, 600);
			const Montage b = MonterFichier(chemin, 400, 600);
			Check(a.lu && b.lu, "   les deux montages se lisent");
			CheckEq(b.empreinte, a.empreinte, "   meme empreinte au bit");
			CheckEq(b.pixels, a.pixels, "   meme nombre de pixels peints");
		}
	}

	// =====================================================================
	printf("\n=== %d / %d ===\n", g_pass, g_pass + g_fail);
	if (g_fail > 0)
		printf("    %d ECHEC(S)\n", g_fail);
	return g_fail > 0 ? 1 : 0;
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
