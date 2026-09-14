// =============================================================================
// NKGuiInteractTest — LE BANC QUI FAIT AGIR LE FORMAT `.nkgui`.
//
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// Le monteur (`NKGuiMonteTest`, 162 criteres) prouve que le document DESSINE.
// Ce banc-ci prouve qu'il AGIT, et il reprend l'ordre du canal :
//
//   (p0) LES ZEROS, prouves avant tout le reste -- un compteur de pixels, un
//        compteur de peintures, un compteur d'instructions.
//   (p1) LA FORME REELLE de `behavior` dans l'archive -- ce que j'avais DEDUIT
//        de la lecture du parseur, mesure au lieu d'etre suppose.
//   (b1) L'INTERACTION, et les quatre etats qu'elle debloque. La couleur ne doit
//        pas « changer » : elle doit devenir CELLE QUE LE FICHIER DECLARE.
//   (b2) LE CALLBACK APPELE, avec son ARGUMENT.
//   (b3) LE COMPORTEMENT EVALUE, attendu calcule a la main AVANT la mesure.
//   (b4) LA LIAISON `bind` a une donnee VIVANTE, dans les deux sens.
//
// ⚠️ AUCUNE FENETRE N'EST OUVERTE, AUCUNE ENTREE N'EST INJECTEE. Le rendu passe
//    par `NkGuiDrawListRaster` (ni fenetre, ni device, ni GPU) et la position du
//    pointeur est POSEE dans `ctx.input.mousePos` -- le champ que
//    `Core/NkGuiInput.h` declare « brut, pose par l'app ». C'est la fonction que
//    la boucle d'evenements appelle. Aucune API systeme n'est touchee.
//
// ⚠️ LE SURVOL A UNE IMAGE DE RETARD, ET CE N'EST PAS UN DEFAUT.
//    `NkGuiContext::ItemHoverable` finit par `return hotIdPrev == id` : le
//    survol resolu est celui de l'image PRECEDENTE (c'est ce qui resout le
//    z-ordre entre panneaux). Un banc qui pose le pointeur et mesure la meme
//    image conclurait donc « le survol ne marche pas » sur une machine
//    parfaitement correcte. Chaque pose est suivie de DEUX images, et le banc
//    le mesure explicitement -- critere (b1.0).
//
// Sortie : `n/n` + code de sortie (0 = tout passe, 1 = au moins un echec).
// =============================================================================

#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Doc/NkGuiInteraction.h"
#include "NKGui/Doc/NkGuiMonteur.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKImage/Core/NkImage.h"
#include "NKMemory/NkAllocator.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

#include <cstdio>
#include <cstdlib>

using namespace nkentseu;
using namespace nkentseu::nkgui;

static int g_pass = 0, g_fail = 0;

static void Check(bool ok, const char *nom) {
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s\n", ok ? "OK" : "KO", nom);
}

static void CheckEqU(uint32 lu, uint32 attendu, const char *nom) {
	const bool ok = (lu == attendu);
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s  (attendu %u, lu %u)\n", ok ? "OK" : "KO", nom, attendu, lu);
}

static void CheckEqF(float32 lu, float32 attendu, float32 tol, const char *nom) {
	const float32 d = (lu > attendu) ? (lu - attendu) : (attendu - lu);
	const bool ok = (d <= tol);
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s  (attendu %.4f +-%.4f, lu %.4f)\n", ok ? "OK" : "KO", nom,
		   (double)attendu, (double)tol, (double)lu);
}

// ── Lecture de fichier ──────────────────────────────────────────────────────
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

// ── Les compteurs, et leur definition exacte ────────────────────────────────
static const uint32 kFond = 0x101418FFu;

/// Pixels EXACTEMENT de cette couleur. C'est le compteur de (b1) : « la couleur
/// devient celle que le fichier declare » ne se mesure pas par « l'image a
/// change », elle se mesure par la PRESENCE de cette couleur-la.
/// ⚠️ SON ZERO SE PROUVE EN (p0) : une cible effacee au fond ne contient aucun
///    pixel des cinq couleurs du fichier 10. Un compteur de pixels a deja rendu
///    40 sans aucune cible dans ce depot.
static uint32 ComptePixelsCouleur(const NkGuiDrawListRaster &r, uint32 couleur) {
	uint32 n = 0;
	for (int32 y = 0; y < r.Hauteur(); ++y)
		for (int32 x = 0; x < r.Largeur(); ++x)
			if (r.Pixel(x, y) == couleur)
				++n;
	return n;
}

/// La boite englobante des pixels d'une couleur. Rend faux si la couleur est
/// absente -- une boite « vide » a des bornes qui n'ont aucun sens, et un
/// appelant qui les lirait mesurerait un rectangle imaginaire.
static bool BoiteCouleur(const NkGuiDrawListRaster &r, uint32 couleur, NkRect &out) {
	int32 x0 = r.Largeur(), y0 = r.Hauteur(), x1 = -1, y1 = -1;
	for (int32 y = 0; y < r.Hauteur(); ++y)
		for (int32 x = 0; x < r.Largeur(); ++x)
			if (r.Pixel(x, y) == couleur) {
				if (x < x0)
					x0 = x;
				if (x > x1)
					x1 = x;
				if (y < y0)
					y0 = y;
				if (y > y1)
					y1 = y;
			}
	if (x1 < 0)
		return false;
	out.x = (float32)x0;
	out.y = (float32)y0;
	out.w = (float32)(x1 - x0 + 1);
	out.h = (float32)(y1 - y0 + 1);
	return true;
}

/// Les pixels d'un rectangle qui ne sont NI l'aplat NI le bord. Sur un bouton
/// re-peint par le document, c'est exactement son LIBELLE.
/// ⚠️ IL EXISTE PARCE QU'UN BOUTON RE-PEINT PEUT ETRE MUET. Quand l'application
///    reprend la main via `ctx.styleFn`, NKGui saute TOUT son dessin par defaut
///    -- le texte compris. Un aplat de la bonne couleur, sans une lettre dessus,
///    passe tous les criteres de couleur du monde.
static uint32 ComptePixelsAutres(const NkGuiDrawListRaster &r, const NkRect &zone, uint32 aplat,
								 uint32 bord) {
	uint32 n = 0;
	const int32 x0 = (int32)zone.x, y0 = (int32)zone.y;
	const int32 x1 = (int32)(zone.x + zone.w), y1 = (int32)(zone.y + zone.h);
	for (int32 y = y0; y < y1 && y < r.Hauteur(); ++y)
		for (int32 x = x0; x < x1 && x < r.Largeur(); ++x) {
			if (x < 0 || y < 0)
				continue;
			const uint32 p = r.Pixel(x, y);
			if (p != aplat && p != bord)
				++n;
		}
	return n;
}

/// Empreinte de TOUS les pixels : deux images identiques AU BIT ont la meme.
static uint32 Empreinte(const NkGuiDrawListRaster &r) {
	uint32 h = 2166136261u;
	const uint8 *p = r.Pixels();
	const usize n = (usize)r.Largeur() * (usize)r.Hauteur() * 4u;
	for (usize i = 0; i < n; ++i) {
		h ^= (uint32)p[i];
		h *= 16777619u;
	}
	return h;
}

static NkGuiFont g_font;
static bool g_fontOk = false;
static const char *g_pngDir = nullptr;

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
	memory::NkFree(out);
	return ok;
}

// =============================================================================
//  LA SCENE -- un document vivant, sur PLUSIEURS images
// =============================================================================
// ⚠️ POURQUOI UNE SCENE PERSISTANTE, ET PAS UN MONTAGE PAR MESURE. Trois
//    mecanismes de NKGui n'existent QU'ENTRE deux images : `hotIdPrev` (le
//    survol resolu), `mouseClicked` (une TRANSITION, pas un etat) et `activeId`
//    (la capture du pointeur par un curseur qu'on glisse). Un banc qui recree
//    son contexte a chaque mesure ne peut en observer aucun -- il mesurerait
//    une interface qui vient de naitre, a chaque fois.
struct Scene {
		NkArchive doc;
		NkGuiContext ctx;
		NkGuiMonteEtat etat;
		NkGuiExecution exe;
		NkGuiMonteRapport rap;
		NkGuiDrawListRaster ras;
		NkGuiDiag err;
		bool lu = false;
		uint32 images = 0;

		bool Charger(const char *src, uint32 len, int32 w, int32 h) {
			if (!NkGuiArchive::Read(src, len, doc, err))
				return false;
			lu = true;
			ctx.viewW = w;
			ctx.viewH = h;
			if (g_fontOk)
				ctx.font = &g_font;
			NkGuiMonteur::Preparer(doc, etat);
			exe.infos.Lire(doc);
			exe.etat = &etat;
			exe.Brancher(ctx);
			return ras.Init(w, h);
		}

		/// Une image complete : entree deja posee, montage, comportement, rendu.
		void Image() {
			rap = NkGuiMonteRapport();
			exe.ReinitialiserCompteurs();
			const NkRect region{0.f, 0.f, (float32)ctx.viewW, (float32)ctx.viewH};
			ctx.BeginFrame(0.016f);
			ctx.BeginLayout(region);
			ctx.DL().Reset();
			NkGuiMonteur::Monter(ctx, doc, etat, rap, &exe);
			// Les comportements APRES le montage : `n1.value` doit etre la valeur
			// que le widget vient d'avoir, pas celle de l'image d'avant.
			exe.ExecuterComportements(doc);
			ctx.EndFrame();
			ras.Effacer(kFond);
			if (g_fontOk && g_font.pixels)
				ras.PoserTexture(g_font.TexId(), g_font.pixels, g_font.atlasW, g_font.atlasH, 1);
			(void)ras.Rasteriser(ctx.DL());
			++images;
		}

		/// Pose le pointeur PUIS rend deux images : la premiere resout `hotId`, la
		/// seconde le LIT (`hotIdPrev`). Voir l'avertissement en tete de fichier.
		void PoserEtStabiliser(float32 x, float32 y) {
			exe.PoserPointeur(ctx, x, y);
			Image();
			Image();
		}

		void Png(const char *nom) {
			if (!g_pngDir || !nom)
				return;
			char sortie[1024];
			Joindre(sortie, sizeof(sortie), g_pngDir, nom);
			if (EcrirePng(ras, sortie))
				printf("        image ecrite : %s\n", sortie);
			else
				printf("        ECHEC d'ecriture : %s\n", sortie);
		}

		/// Le rectangle qu'un widget a REELLEMENT pris, par son id. C'est de la
		/// que sortent toutes les coordonnees de ce banc -- aucune n'est ecrite en
		/// dur : un attendu en dur se perime sans prevenir quand le layout bouge.
		bool Rect(const char *id, NkRect &out) const {
			for (uint32 i = 0; i < (uint32)rap.items.Size(); ++i)
				if (rap.items[i].id.Compare(NkString(id)) == 0 && !rap.items[i].conteneur) {
					out = rap.items[i].rect;
					return true;
				}
			return false;
		}
};

// Le compteur d'appels de l'application -- le bout du fil de (b2).
struct Recepteur {
		uint32 appels = 0;
		NkString dernierNom;
		NkString dernierArg;
		uint32 dernierNbArgs = 0;
		bool dernierArgEstJeton = false;
};

static void SurCallback(const NkGuiAppelCallback &a, void *user) {
	Recepteur *r = (Recepteur *)user;
	if (!r)
		return;
	++r->appels;
	r->dernierNom = a.nom;
	r->dernierNbArgs = a.nbArgs;
	if (a.nbArgs > 0) {
		r->dernierArgEstJeton = (a.args[0].type == NkGuiValeur::Type::Jeton);
		r->dernierArg = a.args[0].jeton;
	}
}

static uint32 Empaquete(const NkColor &c) {
	return ((uint32)c.r << 24) | ((uint32)c.g << 16) | ((uint32)c.b << 8) | (uint32)c.a;
}

// =============================================================================
int main(int argc, char **argv) {
	const char *racine = "Applications/NKUIDesign/exemples/valides/";
	for (int i = 1; i < argc; ++i) {
		const char *a = argv[i];
		if (a[0] == '-' && a[1] == '-' && a[2] == 'p' && a[3] == 'n' && a[4] == 'g' && a[5] == '=')
			g_pngDir = a + 6;
		else if (a[0] != '-')
			racine = a;
	}

	printf("=== NKGuiInteractTest — le document qui AGIT ===\n");
	printf("    corpus : %s\n", racine);
	g_fontOk = g_font.LoadEmbedded(NkEmbeddedFontId::DroidSans, 15.f, false);
	printf("    police embarquee : %s\n\n", g_fontOk ? "chargee" : "ABSENTE");

	// =====================================================================
	printf("-- (p0) LES ZEROS, avant tout le reste\n");
	// =====================================================================
	{
		NkGuiDrawListRaster vide;
		Check(vide.Init(200, 120), "la cible hors ecran s'alloue (200x120)");
		vide.Effacer(kFond);
		// Les cinq couleurs que `10_etats_apparence.nkgui` declare. Leur compte
		// doit etre ZERO sur une image ou rien n'a ete peint.
		const uint32 kCouleurs[] = {0x2F6F7AFFu, 0x3A8894FFu, 0x24565EFFu, 0xF79A28FFu,
									0x6B7B7EFFu};
		uint32 total = 0;
		for (uint32 i = 0; i < 5u; ++i)
			total += ComptePixelsCouleur(vide, kCouleurs[i]);
		CheckEqU(total, 0u, "le compteur de pixels rend ZERO sans rien peindre");
		CheckEqU(ComptePixelsCouleur(vide, kFond), 200u * 120u,
				 "et il rend TOUTE l'image pour la couleur du fond (il compte vraiment)");

		NkGuiExecution exe;
		CheckEqU(exe.peints, 0u, "le compteur de peintures du document part de ZERO");
		uint32 s = 0;
		for (uint32 i = 0; i < kNkGuiEtatCompte; ++i)
			s += exe.peintsParEtat[i];
		CheckEqU(s, 0u, "et aucun etat n'est compte avant le premier montage");

		NkGuiEvaluateur ev;
		CheckEqU(ev.rapport.instructions, 0u, "l'evaluateur part a ZERO instruction");
		CheckEqU((uint32)ev.appels.Size(), 0u, "et a ZERO appel");
	}

	// =====================================================================
	printf("\n-- (p1) LA FORME REELLE de `behavior` dans l'archive\n");
	printf("        (ce que j'avais DEDUIT de `SpanEnd` + `LooksLikeBlock`)\n");
	// =====================================================================
	{
		char chemin[1024];
		Joindre(chemin, sizeof(chemin), racine, "05_animation_comportement.nkgui");
		Fichier f = Lire(chemin);
		Check(f.ok, "05_animation_comportement.nkgui se lit du disque");
		if (f.ok) {
			NkArchive doc;
			NkGuiDiag err;
			Check(NkGuiArchive::Read(f.data, f.taille, doc, err), "il s'analyse");
			const NkArchiveNode *corps = NkGMonteCorps(doc);
			const NkArchive *beh = nullptr;
			if (corps)
				for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i)
					if (corps->array[i].IsObject() && corps->array[i].object
						&& NkGMotEgal(NkGuiArchive::TypeOf(*corps->array[i].object), "behavior"))
						beh = corps->array[i].object;
			Check(beh != nullptr, "la section `behavior` est un BLOC de premier niveau");
			if (beh) {
				const NkArchiveNode *b = NkGMonteCorps(*beh);
				Check(b != nullptr, "elle a un corps");
				if (b) {
					// ATTENDU, ecrit avant la mesure : DEUX elements, tous deux
					// SCALAIRES (des tranches verbatim) -- `set ...` et le `if`
					// ENTIER, corps compris.
					CheckEqU((uint32)b->array.Size(), 2u,
							 "elle porte DEUX instructions");
					uint32 scalaires = 0;
					for (uint32 i = 0; i < (uint32)b->array.Size(); ++i)
						if (b->array[i].IsScalar())
							++scalaires;
					CheckEqU(scalaires, 2u,
							 "et les deux sont des TRANCHES VERBATIM, pas des blocs");
					if (b->array.Size() >= 2u && b->array[1].IsScalar()) {
						const NkString lex(b->array[1].Lexeme());
						bool aCallback = false, aAccolade = false;
						for (uint32 i = 0; i + 8u <= (uint32)lex.Size(); ++i) {
							const char *m = "Callback";
							bool ok = true;
							for (uint32 j = 0; j < 8u; ++j)
								if (lex.Data()[i + j] != m[j])
									ok = false;
							if (ok)
								aCallback = true;
						}
						for (uint32 i = 0; i < (uint32)lex.Size(); ++i)
							if (lex.Data()[i] == '{')
								aAccolade = true;
						Check(aCallback && aAccolade,
							  "la 2e tranche contient le `if` ENTIER, son `Callback` compris");
						printf("        tranche 2 : %s\n", lex.CStr());
					}
				}
			}
			Liberer(f);
		}
	}

	// =====================================================================
	printf("\n-- (b1) L'INTERACTION, et les quatre etats qu'elle debloque\n");
	// =====================================================================
	{
		char chemin[1024];
		Joindre(chemin, sizeof(chemin), racine, "10_etats_apparence.nkgui");
		Fichier f = Lire(chemin);
		Check(f.ok, "10_etats_apparence.nkgui se lit du disque");
		if (f.ok) {
			Scene s;
			Check(s.Charger(f.data, f.taille, 420, 220), "il se charge et la cible s'alloue");

			// --- les apparences sont LUES, et elles valent ce que le fichier ecrit
			const NkGuiInfoWidget *valider = s.exe.infos.Trouver(NkStringView("valider"));
			const NkGuiInfoWidget *annuler = s.exe.infos.Trouver(NkStringView("annuler"));
			const NkGuiInfoWidget *champ = s.exe.infos.Trouver(NkStringView("recherche"));
			Check(valider && annuler && champ, "les trois widgets du fichier sont retrouves");
			CheckEqU(s.exe.infos.EtatsHorsRepos(), 6u,
					 "SIX apparences hors repos dans CE fichier (4 + 1 + 1)");
			if (valider) {
				// ATTENDU ECRIT AVANT : les couleurs du fichier, chiffre par chiffre.
				CheckEqU(Empaquete(valider->etats[(uint32)NkGuiEtatApp::Normal].fond), 0x2F6F7AFFu,
						 "valider/Normal   = #2F6F7A");
				CheckEqU(Empaquete(valider->etats[(uint32)NkGuiEtatApp::Hover].fond), 0x3A8894FFu,
						 "valider/Hover    = #3A8894");
				CheckEqU(Empaquete(valider->etats[(uint32)NkGuiEtatApp::Pressed].fond), 0x24565EFFu,
						 "valider/Pressed  = #24565E");
				CheckEqU(Empaquete(valider->etats[(uint32)NkGuiEtatApp::Disabled].fond), 0x6B7B7EFFu,
						 "valider/Disabled = #6B7B7E");
				CheckEqU(Empaquete(valider->etats[(uint32)NkGuiEtatApp::FocusVisible].contour),
						 0xF79A28FFu, "valider/FocusVisible = contour #F79A28");
				Check(!valider->etats[(uint32)NkGuiEtatApp::Focus].declare,
					  "et le fichier NE declare PAS `Focus` sur le bouton (seulement FocusVisible)");
			}
			if (champ)
				CheckEqU(Empaquete(champ->etats[(uint32)NkGuiEtatApp::Focus].contour), 0xF79A28FFu,
						 "recherche/Focus  = contour #F79A28");

			// --- REPOS : le pointeur est dans le vide (coin bas-droit, hors widgets)
			const float32 vide_x = 410.f, vide_y = 210.f;
			s.PoserEtStabiliser(vide_x, vide_y);
			printf("        pointeur POSE dans le vide : (%.1f, %.1f)\n", (double)vide_x,
				   (double)vide_y);
			const uint32 empreinteRepos = Empreinte(s.ras);
			const uint32 normalRepos = ComptePixelsCouleur(s.ras, 0x2F6F7AFFu);
			const uint32 hoverRepos = ComptePixelsCouleur(s.ras, 0x3A8894FFu);
			const uint32 pressedRepos = ComptePixelsCouleur(s.ras, 0x24565EFFu);
			const uint32 anneauRepos = ComptePixelsCouleur(s.ras, 0xF79A28FFu);
			const uint32 grisRepos = ComptePixelsCouleur(s.ras, 0x6B7B7EFFu);
			s.Png("b1_0_repos.png");
			printf("        repos : Normal %u px · Hover %u · Pressed %u · anneau %u · Disabled %u\n",
				   normalRepos, hoverRepos, pressedRepos, anneauRepos, grisRepos);
			Check(normalRepos > 0u, "AU REPOS le bouton porte la couleur Normal du fichier");
			CheckEqU(hoverRepos, 0u, "et ZERO pixel de la couleur Hover");
			CheckEqU(pressedRepos, 0u, "et ZERO pixel de la couleur Pressed");
			CheckEqU(anneauRepos, 0u, "et ZERO pixel de l'anneau de focus");
			CheckEqU(grisRepos, 0u, "et ZERO pixel de la couleur Disabled");
			CheckEqU(s.exe.peintsParEtat[(uint32)NkGuiEtatApp::Hover], 0u,
					 "aucun widget peint en Hover");
			CheckEqU(s.exe.peints, 2u, "DEUX widgets peints par le document (les deux boutons)");

			// --- le rectangle de `valider`, DERIVE du montage (jamais ecrit en dur)
			NkRect rValider{0.f, 0.f, 0.f, 0.f};
			NkRect rAnnuler{0.f, 0.f, 0.f, 0.f};
			NkRect rChamp{0.f, 0.f, 0.f, 0.f};
			const bool aRects = s.Rect("valider", rValider) && s.Rect("annuler", rAnnuler)
								&& s.Rect("recherche", rChamp);
			Check(aRects, "les rectangles des trois widgets sortent du montage");
			printf("        valider   : x=%.1f y=%.1f w=%.1f h=%.1f\n", (double)rValider.x,
				   (double)rValider.y, (double)rValider.w, (double)rValider.h);

			// ── (b1.0) LE SURVOL A UNE IMAGE DE RETARD, et on le MESURE ────
			if (aRects) {
				const float32 cx = rValider.x + rValider.w * 0.5f;
				const float32 cy = rValider.y + rValider.h * 0.5f;
				s.exe.PoserPointeur(s.ctx, cx, cy);
				s.Image(); // UNE seule image apres la pose
				const uint32 hover1 = ComptePixelsCouleur(s.ras, 0x3A8894FFu);
				s.Image(); // la seconde
				const uint32 hover2 = ComptePixelsCouleur(s.ras, 0x3A8894FFu);
				printf("        pointeur POSE au centre de `valider` : (%.1f, %.1f)\n", (double)cx,
					   (double)cy);
				printf("        Hover : image 1 -> %u px ; image 2 -> %u px\n", hover1, hover2);
				CheckEqU(hover1, 0u,
						 "(b1.0) a la 1re image le survol n'est PAS encore resolu (hotIdPrev)");
				Check(hover2 > 0u, "(b1.0) il l'est a la 2e -- le banc mesure sa propre condition");

				// ── (b1.a) HOVER : la couleur devient CELLE DU FICHIER ──────
				s.Png("b1_a_hover.png");
				CheckEqU(s.exe.peintsParEtat[(uint32)NkGuiEtatApp::Hover], 1u,
						 "(b1.a) UN widget peint en Hover");
				Check(ComptePixelsCouleur(s.ras, 0x3A8894FFu) > 0u,
					  "(b1.a) la couleur Hover du fichier (#3A8894) est PRESENTE");
				Check(ComptePixelsCouleur(s.ras, 0x3A8894FFu) > hoverRepos,
					  "(b1.a) et elle ne l'etait pas au repos");
				CheckEqU(ComptePixelsCouleur(s.ras, 0x2F6F7AFFu), 0u,
						 "(b1.a) NEGATIF : la couleur Normal a DISPARU du bouton survole");
				// L'autre bouton, lui, n'a pas bouge : un survol qui repeindrait tout
				// serait vert au compteur et faux a l'image.
				Check(ComptePixelsCouleur(s.ras, 0x3A3F44FFu) > 0u,
					  "(b1.a) `annuler` garde SON repos (#3A3F44) -- le survol est cible");
				CheckEqU(ComptePixelsCouleur(s.ras, 0x4A5056FFu), 0u,
						 "(b1.a) et ZERO pixel du Hover de `annuler`");
				// 🔴 UN BOUTON RE-PEINT PEUT ETRE MUET. Quand le document reprend la
				//    main, NKGui saute TOUT son defaut -- le libelle compris. Un aplat
				//    de la bonne couleur sans une lettre dessus passe tous les
				//    criteres de couleur. Celui-ci compte ce qui n'est ni l'aplat ni
				//    le bord DANS le rectangle du bouton : c'est le texte.
				{
					NkGuiContext ref;
					const uint32 bord = Empaquete(ref.theme.border);
					const uint32 texte = ComptePixelsAutres(s.ras, rValider, 0x3A8894FFu, bord);
					printf("        pixels de libelle dans `valider` survole : %u\n", texte);
					Check(texte > 0u, "(b1.a) et le bouton re-peint N'EST PAS MUET : son "
									  "libelle est peint");
				}

				// ── (b1.b) PRESSED ──────────────────────────────────────────
				s.exe.PoserBouton(s.ctx, 0, true);
				s.Image();
				s.Png("b1_b_pressed.png");
				CheckEqU(s.exe.peintsParEtat[(uint32)NkGuiEtatApp::Pressed], 1u,
						 "(b1.b) UN widget peint en Pressed");
				Check(ComptePixelsCouleur(s.ras, 0x24565EFFu) > 0u,
					  "(b1.b) la couleur Pressed du fichier (#24565E) est PRESENTE");
				CheckEqU(ComptePixelsCouleur(s.ras, 0x3A8894FFu), 0u,
						 "(b1.b) NEGATIF : plus un pixel de Hover -- UN SEUL etat s'applique");
				s.exe.PoserBouton(s.ctx, 0, false);
				s.Image();
				s.Image();

				// ── (b1.c) FOCUS VISIBLE (clavier) vs FOCUS SOURIS ──────────
				s.exe.PoserPointeur(s.ctx, vide_x, vide_y);
				s.Image();
				s.Image();
				s.exe.DonnerFocus(NkStringView("valider"), true);
				s.Image();
				s.Png("b1_c_focusvisible.png");
				CheckEqU(s.exe.peintsParEtat[(uint32)NkGuiEtatApp::FocusVisible], 1u,
						 "(b1.c) UN widget peint en FocusVisible");
				Check(ComptePixelsCouleur(s.ras, 0xF79A28FFu) > 0u,
					  "(b1.c) l'anneau orange (#F79A28) est PRESENT");
				// 🔴 LE CRITERE QUE L'IMAGE A EXIGE. `appearance(FocusVisible)` ne
				//    declare QU'UN `stroke` : le fond doit rester celui du repos.
				//    La premiere version peignait l'etat SEUL, le bouton perdait son
				//    teal et retombait sur le gris du theme -- et les onze criteres
				//    ci-dessus restaient VERTS, parce qu'ils ne regardaient que
				//    l'anneau. Ce critere-la aurait rougi ; il n'existait pas.
				Check(ComptePixelsCouleur(s.ras, 0x2F6F7AFFu) > 0u,
					  "(b1.c) et le bouton GARDE son fond de repos -- le repos est le SOCLE");

				s.exe.DonnerFocus(NkStringView("valider"), false); // focus pris a la SOURIS
				s.Image();
				s.Png("b1_c_focus_souris.png");
				CheckEqU(s.exe.peintsParEtat[(uint32)NkGuiEtatApp::FocusVisible], 0u,
						 "(b1.c) NEGATIF : un focus pris A LA SOURIS ne peint PAS FocusVisible");
				CheckEqU(ComptePixelsCouleur(s.ras, 0xF79A28FFu), 0u,
						 "(b1.c) NEGATIF : et l'anneau orange a disparu");
				s.exe.RetirerFocus();
				s.Image();

				// ── (b1.d) FOCUS sur le CHAMP : un `stroke`, pas un aplat ───
				s.exe.DonnerFocus(NkStringView("recherche"), true);
				s.Image();
				s.Png("b1_d_champ_focus.png");
				CheckEqU(s.exe.peintsParEtat[(uint32)NkGuiEtatApp::Focus], 1u,
						 "(b1.d) le CHAMP est peint en Focus (le fichier declare `Focus`, pas FocusVisible)");
				Check(ComptePixelsCouleur(s.ras, 0xF79A28FFu) > 0u,
					  "(b1.d) son contour orange est PRESENT");
				// 🔴 SECOND CRITERE NE DE L'IMAGE. L'anneau debordait sur le libelle
				//    « recherche » ecrit a droite du champ : je peignais la RANGEE
				//    (`layout.prevItem`) au lieu du CHAMP. Un compteur de pixels
				//    orange etait vert dans les deux cas -- il faut mesurer OU ils
				//    sont, pas seulement qu'il y en a.
				//    ATTENDU, DERIVE du meme calcul que `InputTextEx` :
				//        largeur du champ = largeur de la rangee - (texte du libelle + 14)
				{
					NkRect boite{0.f, 0.f, 0.f, 0.f};
					const bool aBoite = BoiteCouleur(s.ras, 0xF79A28FFu, boite);
					Check(aBoite, "(b1.d) la boite englobante de l'anneau se mesure");
					const char *lbl = "recherche";
					const float32 largeurLbl =
						(g_fontOk && LabelEnd(lbl) != lbl)
							? g_font.MeasureWidth(lbl, LabelEnd(lbl)) + 14.f
							: 0.f;
					const float32 attendueW = rChamp.w - largeurLbl;
					printf("        anneau : x=%.1f w=%.1f · champ attendu w=%.1f"
						   " (rangee %.1f - libelle %.1f)\n",
						   (double)boite.x, (double)boite.w, (double)attendueW, (double)rChamp.w,
						   (double)largeurLbl);
					if (aBoite)
						CheckEqF(boite.w, attendueW, 3.f,
								 "(b1.d) et il epouse le CHAMP, pas la rangee entiere");
				}
				s.exe.RetirerFocus();
				s.Image();

				// ── (b1.e) NEGATIF D'ENSEMBLE : le vide rend l'image du repos,
				//           AU BIT. C'est le negatif que le canal demande.
				s.exe.PoserPointeur(s.ctx, vide_x, vide_y);
				s.Image();
				s.Image();
				s.Png("b1_e_retour_repos.png");
				CheckEqU(Empreinte(s.ras), empreinteRepos,
						 "(b1.e) NEGATIF : pointeur dans le vide -> l'image du repos, AU BIT");
			}
			s.exe.Debrancher(s.ctx);
			Liberer(f);
		}
	}

	// =====================================================================
	printf("\n-- (b1.f) DISABLED -- le seul etat qui ne vient pas du pointeur\n");
	// =====================================================================
	{
		// ⚠️ AUCUN FICHIER DU CORPUS N'ECRIT `enabled = false`. Le fichier 10
		//    DECLARE `appearance(Disabled)` sans jamais desactiver le bouton :
		//    l'etat est donc inatteignable tel quel, et le dire vaut mieux que
		//    de le peindre sans raison. Le document ci-dessous est le MEME,
		//    plus la ligne que le format prevoit deja (`{"enabled", 'b'}`,
		//    NkGuiValidate.h l. 187). Le corpus n'est pas touche.
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Button \"valider\" {\n"
			"    label = \"Valider\"\n"
			"    enabled = false\n"
			"    appearance { radius = 6, fill { color = #2F6F7A } }\n"
			"    appearance(Hover) { fill { color = #3A8894 } }\n"
			"    appearance(Disabled) { fill { color = #6B7B7E } }\n"
			"  }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 120),
			  "(b1.f) le document `enabled = false` se charge");
		const NkGuiInfoWidget *w = s.exe.infos.Trouver(NkStringView("valider"));
		Check(w && !w->actif, "(b1.f) le widget est lu comme DESACTIVE");
		NkRect r{0.f, 0.f, 0.f, 0.f};
		s.Image();
		s.Image();
		const bool aR = s.Rect("valider", r);
		Check(aR, "(b1.f) son rectangle sort du montage");
		if (aR) {
			// On pose le pointeur DESSUS : un widget desactive doit rester
			// Disabled quoi qu'il arrive -- c'est le mot du canal.
			s.PoserEtStabiliser(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
			s.Png("b1_f_disabled.png");
			CheckEqU(s.exe.peintsParEtat[(uint32)NkGuiEtatApp::Disabled], 1u,
					 "(b1.f) peint en Disabled");
			Check(ComptePixelsCouleur(s.ras, 0x6B7B7EFFu) > 0u,
				  "(b1.f) la couleur Disabled du fichier (#6B7B7E) est PRESENTE");
			CheckEqU(ComptePixelsCouleur(s.ras, 0x3A8894FFu), 0u,
					 "(b1.f) NEGATIF : le pointeur est DESSUS et il n'y a AUCUN pixel de Hover");
			CheckEqU(ComptePixelsCouleur(s.ras, 0x2F6F7AFFu), 0u,
					 "(b1.f) NEGATIF : ni de Normal");
			// 🔴 CE QUE LA CAPTURE A MONTRE, ET CE QU'ELLE NE PROUVE PAS.
			//    Sur `b1_f_disabled.png` le libelle « Valider » est INVISIBLE. Deux
			//    causes possibles, et elles n'appellent pas la meme reponse : ou il
			//    n'est pas peint (defaut du montage), ou il est peint dans une
			//    couleur trop proche du fond (choix du document croise avec le
			//    theme). Le compteur tranche au lieu de supposer.
			{
				NkGuiContext ref;
				const uint32 bord = Empaquete(ref.theme.border);
				const uint32 texte = ComptePixelsAutres(s.ras, r, 0x6B7B7EFFu, bord);
				printf("        pixels de libelle dans le bouton desactive : %u\n", texte);
				Check(texte > 0u,
					  "(b1.f) le libelle EST peint -- il est illisible, pas absent");
				printf("        ⚠️  il est illisible : le document choisit #6B7B7E et le "
					   "theme peint\n            le texte grise en #%02X%02X%02X. Ce n'est "
					   "pas un defaut du montage,\n            c'est une COLLISION que le "
					   "document ne resout pas -- il n'ecrit\n            aucune couleur de "
					   "texte pour `Disabled`, et le format le permet.\n",
					   (unsigned)ref.theme.textDisabled.r, (unsigned)ref.theme.textDisabled.g,
					   (unsigned)ref.theme.textDisabled.b);
			}
		}
		s.exe.Debrancher(s.ctx);
	}

	// =====================================================================
	printf("\n-- (b2)+(b3) LE CALLBACK ET LE COMPORTEMENT\n");
	// =====================================================================
	{
		char chemin[1024];
		Joindre(chemin, sizeof(chemin), racine, "05_animation_comportement.nkgui");
		Fichier f = Lire(chemin);
		Check(f.ok, "05_animation_comportement.nkgui se lit du disque");
		if (f.ok) {
			// ── ATTENDUS, CALCULES A LA MAIN, ECRITS AVANT LA MESURE ────────
			//   Le fichier : Slider "n1" { bind = modele.valeur, min = 0, max = 1 }
			//                behavior "seuil" { set r = n1.value * 100
			//                                   if n1.value > 0.8 {
			//                                       Callback "alerte"(Enum.Haut) } }
			//
			//   CAS HAUT  : je pose modele.valeur = 0.90
			//               -> r = 0.90 * 100 = 90.0
			//               -> 0.90 > 0.8 : VRAI  -> 1 appel, argument Enum.Haut
			//   CAS BAS   : je pose modele.valeur = 0.50
			//               -> r = 0.50 * 100 = 50.0
			//               -> 0.50 > 0.8 : FAUX  -> 0 appel
			const float32 kHaut = 0.90f;
			const float32 kBas = 0.50f;
			const float32 kAttenduRHaut = kHaut * 100.f; // derive du parametre, pas recopie
			const float32 kAttenduRBas = kBas * 100.f;
			printf("        ATTENDU ECRIT AVANT : v=%.2f -> r=%.1f, condition VRAIE, 1 appel\n",
				   (double)kHaut, (double)kAttenduRHaut);
			printf("        ATTENDU ECRIT AVANT : v=%.2f -> r=%.1f, condition FAUSSE, 0 appel\n",
				   (double)kBas, (double)kAttenduRBas);
			// Le calcul a la main, verifie contre le litteral : si l'un des deux
			// se perime, les deux ne peuvent pas mentir ensemble.
			CheckEqF(kAttenduRHaut, 90.f, 0.001f, "        (l'attendu haut vaut bien 90.0)");
			CheckEqF(kAttenduRBas, 50.f, 0.001f, "        (l'attendu bas vaut bien 50.0)");

			// ── CAS HAUT ───────────────────────────────────────────────────
			{
				Scene s;
				Recepteur rec;
				Check(s.Charger(f.data, f.taille, 420, 160), "(b2) le document se charge");
				s.exe.eval.rappel = &SurCallback;
				s.exe.eval.rappelUser = &rec;
				s.exe.modele.Poser(NkStringView("modele.valeur"), kHaut);
				s.Image();
				s.Png("b2_haut.png");

				// ⚠️ MON PREMIER ATTENDU ETAIT FAUX, ET C'EST LE BANC QUI ME L'A DIT.
				//    J'avais ecrit DEUX (« le `set` et le `if` »). La mesure a rendu
				//    TROIS, et elle a raison : `call_stmt := callback_call` est une
				//    INSTRUCTION du langage au meme titre que les deux autres
				//    (document 2 §4). Le `Callback` du corps du `if` en est donc une
				//    troisieme. Je corrige l'attendu, pas le compteur -- et je laisse
				//    la trace, parce qu'un attendu qu'on change en silence est un
				//    attendu qui suivra le code au lieu de le juger.
				CheckEqU(s.exe.eval.rapport.instructions, 3u,
						 "(b3) TROIS instructions executees (`set`, `if`, et le `Callback`)");
				CheckEqU(s.exe.eval.rapport.appels, 1u,
						 "(b3) dont UN `Callback` -- 1 + 1 + 1, le compte se referme");
				CheckEqU(s.exe.eval.rapport.affectations, 1u, "(b3) une affectation");
				CheckEqU(s.exe.eval.rapport.conditions, 1u, "(b3) une condition");
				CheckEqU(s.exe.eval.rapport.branchesPrises, 1u,
						 "(b3) la branche EST prise pour 0.90 > 0.8");
				CheckEqU(s.exe.eval.rapport.refusees, 0u,
						 "(b3) et AUCUNE instruction refusee -- rien n'a ete devine");
				NkGuiValeur vr;
				Check(s.exe.eval.Variable(NkStringView("r"), vr), "(b3) la variable `r` existe");
				CheckEqF(vr.EnNombre(), kAttenduRHaut, 0.01f,
						 "(b3) et elle vaut `n1.value * 100`");

				CheckEqU(rec.appels, 1u, "(b2) l'APPLICATION a ete appelee UNE fois");
				Check(rec.dernierNom.Compare(NkString("alerte")) == 0,
					  "(b2) sous le nom `alerte`");
				CheckEqU(rec.dernierNbArgs, 1u, "(b2) avec UN argument");
				Check(rec.dernierArgEstJeton, "(b2) l'argument est un JETON (pas un nombre)");
				Check(rec.dernierArg.Compare(NkString("Enum.Haut")) == 0,
					  "(b2) et cet argument est `Enum.Haut` -- le texte, pas un 0 silencieux");
				printf("        recu : %s(%s)\n", rec.dernierNom.CStr(), rec.dernierArg.CStr());
				s.exe.Debrancher(s.ctx);
			}

			// ── CAS BAS (le NEGATIF des deux criteres) ─────────────────────
			{
				Scene s;
				Recepteur rec;
				Check(s.Charger(f.data, f.taille, 420, 160), "(b2) le meme document, recharge");
				s.exe.eval.rappel = &SurCallback;
				s.exe.eval.rappelUser = &rec;
				s.exe.modele.Poser(NkStringView("modele.valeur"), kBas);
				s.Image();
				s.Png("b2_bas.png");
				CheckEqU(rec.appels, 0u,
						 "(b2) NEGATIF : condition fausse -> ZERO appel a l'application");
				CheckEqU(s.exe.eval.rapport.branchesPrises, 0u,
						 "(b3) NEGATIF : la branche n'est PAS prise");
				CheckEqU(s.exe.eval.rapport.conditions, 1u,
						 "(b3) mais la condition a bien ete EVALUEE (le banc n'a pas rien fait)");
				NkGuiValeur vr;
				Check(s.exe.eval.Variable(NkStringView("r"), vr),
					  "(b3) `r` existe quand meme : le `set` est HORS du `if`");
				CheckEqF(vr.EnNombre(), kAttenduRBas, 0.01f, "(b3) et il vaut 50.0");
				s.exe.Debrancher(s.ctx);
			}
			Liberer(f);
		}
	}

	// =====================================================================
	printf("\n-- (b3bis) L'EVALUATEUR sur ce que le corpus NE contient PAS\n");
	printf("           (la grammaire du document 2 §4 va plus loin que le corpus)\n");
	// =====================================================================
	{
		// Ces quatre documents sont ECRITS ICI, pas ajoutes au corpus. Ils
		// exercent les constructions que la grammaire declare et que les trois
		// blocs `behavior` du depot n'emploient pas : `else`, les parentheses,
		// `&&` / `||`, la priorite des operateurs. Si l'evaluateur les ratait,
		// le corpus resterait vert -- c'est exactement le trou qu'un banc doit
		// couvrir lui-meme.
		struct Cas {
				const char *nom;
				const char *instruction;
				float32 attendu;
				uint32 else_;
		};
		static const Cas kCas[] = {
			{"priorite : 2 + 3 * 4 = 14 (pas 20)", "set r = 2 + 3 * 4", 14.f, 0u},
			{"parentheses : (2 + 3) * 4 = 20", "set r = (2 + 3) * 4", 20.f, 0u},
			{"soustraction : 10 - 4 - 3 = 3 (associativite a gauche)", "set r = 10 - 4 - 3", 3.f, 0u},
			{"division : 12 / 4 / 3 = 1", "set r = 12 / 4 / 3", 1.f, 0u},
			{"negatif litteral : set r = -7", "set r = -7", -7.f, 0u},
		};
		for (uint32 i = 0; i < 5u; ++i) {
			NkGuiEvaluateur ev;
			ev.Reinitialiser();
			ev.ExecuterTranche(NkStringView(kCas[i].instruction));
			NkGuiValeur v;
			const bool a = ev.Variable(NkStringView("r"), v);
			Check(a, kCas[i].nom);
			if (a)
				CheckEqF(v.EnNombre(), kCas[i].attendu, 0.001f, "        -> valeur");
			CheckEqU(ev.rapport.refusees, 0u, "        -> rien de refuse");
		}
		// `else` : la grammaire l'ecrit, le corpus ne l'emploie pas.
		{
			NkGuiEvaluateur ev;
			ev.Reinitialiser();
			ev.ExecuterTranche(NkStringView("if 1 > 2 { set r = 1 } else { set r = 2 }"));
			NkGuiValeur v;
			Check(ev.Variable(NkStringView("r"), v), "`else` : la branche alternative EXISTE");
			CheckEqF(v.EnNombre(), 2.f, 0.001f, "        -> c'est elle qui s'execute");
			CheckEqU(ev.rapport.branchesElse, 1u, "        -> et elle est comptee");
			CheckEqU(ev.rapport.branchesPrises, 0u, "        -> la branche vraie ne l'est pas");
		}
		// `if` imbriques : le saut de la branche non prise ne doit pas sauter le
		// mauvais bloc.
		{
			NkGuiEvaluateur ev;
			ev.Reinitialiser();
			ev.ExecuterTranche(
				NkStringView("if 1 > 2 { if 1 > 0 { set a = 9 } set b = 8 } set r = 5"));
			NkGuiValeur v, va;
			Check(!ev.Variable(NkStringView("a"), va),
				  "`if` imbrique : RIEN du bloc non pris ne s'execute");
			Check(ev.Variable(NkStringView("r"), v), "et l'instruction d'APRES s'execute");
			CheckEqF(v.EnNombre(), 5.f, 0.001f, "        -> r = 5");
		}
		// Une instruction hors grammaire est COMPTEE, jamais devinee.
		{
			NkGuiEvaluateur ev;
			ev.Reinitialiser();
			ev.ExecuterTranche(NkStringView("while x { set r = 1 }"));
			CheckEqU(ev.rapport.refusees, 1u,
					 "NEGATIF : `while` n'est pas du langage -> compte comme REFUSE");
			CheckEqU(ev.rapport.instructions, 0u, "NEGATIF : et rien n'a ete execute");
			NkGuiValeur v;
			Check(!ev.Variable(NkStringView("r"), v), "NEGATIF : `r` n'a pas ete pose");
		}
		// Une variable non resolue rend un JETON de son nom -- pas un zero muet.
		{
			NkGuiEvaluateur ev;
			ev.Reinitialiser();
			ev.ExecuterTranche(NkStringView("set r = inconnu"));
			NkGuiValeur v;
			Check(ev.Variable(NkStringView("r"), v), "une variable non resolue se pose quand meme");
			Check(v.type == NkGuiValeur::Type::Jeton,
				  "        -> en JETON, pas en zero silencieux");
			Check(v.jeton.Compare(NkString("inconnu")) == 0, "        -> et il porte son nom");
		}
	}

	// =====================================================================
	printf("\n-- (b4) LA LIAISON `bind` a une donnee VIVANTE, dans les DEUX sens\n");
	// =====================================================================
	{
		char chemin[1024];
		Joindre(chemin, sizeof(chemin), racine, "01_panneau_reglages.nkgui");
		Fichier f = Lire(chemin);
		Check(f.ok, "01_panneau_reglages.nkgui se lit du disque");
		if (f.ok) {
			// Le fichier : Slider "echelle" { bind = ui.echelle, min = 0.5, max = 3.0 }
			// La barre remplie du curseur est peinte en `theme.accent` : sa
			// largeur est proportionnelle a (valeur - min) / (max - min). C'est
			// le compteur de pixels de (b4), et il est DERIVE du fichier.
			NkGuiContext ref;
			const uint32 kAccent = Empaquete(ref.theme.accent);

			// ── SANS LIAISON (le NEGATIF, mesure EN PREMIER) ───────────────
			uint32 accentSansModele = 0;
			{
				Scene s;
				Check(s.Charger(f.data, f.taille, 460, 360),
					  "(b4) le panneau se charge, modele VIDE");
				CheckEqU(s.exe.modele.Taille(), 0u, "(b4) le modele est vide : ZERO chemin");
				s.Image();
				s.Image();
				accentSansModele = ComptePixelsCouleur(s.ras, kAccent);
				s.Png("b4_a_sans_modele.png");
				NkGuiMonteEtat::Entree *e = s.etat.Get(NkStringView("ui.echelle"));
				Check(e != nullptr, "(b4) la cle d'etat du curseur est bien son `bind`");
				if (e)
					CheckEqF(e->f, 0.5f, 0.001f,
							 "(b4) NEGATIF : sans liaison, le widget garde la valeur du FICHIER (min = 0.5)");
				s.exe.Debrancher(s.ctx);
			}
			printf("        barre du curseur sans modele : %u px de theme.accent\n",
				   accentSansModele);

			// ── LA DONNEE BOUGE -> LE WIDGET BOUGE ─────────────────────────
			{
				Scene s;
				Check(s.Charger(f.data, f.taille, 460, 360),
					  "(b4) le meme panneau, avec un modele");
				// ATTENDU ECRIT AVANT : min=0.5, max=3.0. Je pose 3.0 (le maximum)
				// -> la barre doit remplir TOUTE la piste, donc STRICTEMENT plus de
				// pixels d'accent qu'a 0.5 (ou elle est vide).
				s.exe.modele.Poser(NkStringView("ui.echelle"), 3.0f);
				s.Image();
				s.Image();
				const uint32 accentMax = ComptePixelsCouleur(s.ras, kAccent);
				s.Png("b4_b_modele_max.png");
				printf("        barre a ui.echelle = 3.0 : %u px de theme.accent\n", accentMax);
				NkGuiMonteEtat::Entree *e = s.etat.Get(NkStringView("ui.echelle"));
				Check(e != nullptr, "(b4) l'entree d'etat existe");
				if (e)
					CheckEqF(e->f, 3.0f, 0.001f,
							 "(b4) LA DONNEE A BOUGE -> la valeur du widget suit");
				Check(accentMax > accentSansModele,
					  "(b4) et ca se VOIT : la barre remplie est plus longue qu'a 0.5");

				// ⚠️ DEUX EXTREMES NE PROUVENT PAS UNE LIAISON, ILS PROUVENT DEUX
				//    BUTEES. A 0.5 la barre est VIDE (0 px) et a 3.0 elle est PLEINE :
				//    un montage qui ne ferait que « vide ou plein » passerait les deux.
				//    Le milieu, lui, demande que la barre suive la valeur.
				//    ATTENDU, DERIVE des bornes du fichier : a 1.75,
				//    t = (1.75 - 0.5) / (3.0 - 0.5) = 0.5, donc la MOITIE de la piste.
				s.exe.modele.Poser(NkStringView("ui.echelle"), 1.75f);
				s.Image();
				s.Image();
				const uint32 accentMi = ComptePixelsCouleur(s.ras, kAccent);
				s.Png("b4_b2_modele_milieu.png");
				printf("        barre a ui.echelle = 1.75 : %u px (plein = %u px)\n", accentMi,
					   accentMax);
				const float32 part = accentMax > 0u ? (float32)accentMi / (float32)accentMax : 0.f;
				CheckEqF(part, 0.5f, 0.06f,
						 "(b4) a mi-course la barre fait la MOITIE de la piste");

				// Et on la rebouge, dans l'autre sens, sur la MEME scene : une
				// liaison qui ne marcherait qu'a l'initialisation serait verte
				// au premier essai et morte ensuite.
				s.exe.modele.Poser(NkStringView("ui.echelle"), 0.5f);
				s.Image();
				s.Image();
				const uint32 accentRetour = ComptePixelsCouleur(s.ras, kAccent);
				s.Png("b4_c_modele_min.png");
				printf("        barre revenue a 0.5 : %u px de theme.accent\n", accentRetour);
				CheckEqU(accentRetour, accentSansModele,
						 "(b4) la donnee revient au minimum -> la barre aussi, au pixel");
				s.exe.Debrancher(s.ctx);
			}

			// ── LE WIDGET BOUGE -> LA DONNEE BOUGE ─────────────────────────
			{
				Scene s;
				Check(s.Charger(f.data, f.taille, 460, 360),
					  "(b4) le meme panneau, pour le sens inverse");
				s.exe.modele.Poser(NkStringView("ui.echelle"), 0.5f);
				s.Image();
				s.Image();
				const uint32 ecrituresAvant = s.exe.modele.Ecritures(NkStringView("ui.echelle"));
				NkRect rSlider{0.f, 0.f, 0.f, 0.f};
				const bool aR = s.Rect("echelle", rSlider);
				Check(aR, "(b4) le rectangle du curseur sort du montage");
				if (aR) {
					// La piste occupe 55% de la largeur du rang (`SliderFloat`).
					// Je pose le pointeur a 75% de cette piste, et j'ENFONCE --
					// `mouseClicked` est une TRANSITION, il faut donc une image
					// pointeur pose bouton relache, puis une image bouton enfonce.
					const float32 pisteW = (rSlider.w * 0.55f > 40.f) ? rSlider.w * 0.55f : 40.f;
					const float32 px = rSlider.x + pisteW * 0.75f;
					const float32 py = rSlider.y + rSlider.h * 0.5f;
					printf("        pointeur POSE sur la piste du curseur : (%.1f, %.1f)"
						   " — 75%% d'une piste de %.1f px\n",
						   (double)px, (double)py, (double)pisteW);
					s.exe.PoserPointeur(s.ctx, px, py);
					s.Image();
					s.Image();
					s.exe.PoserBouton(s.ctx, 0, true);
					s.Image();
					s.Png("b4_d_widget_vers_donnee.png");
					// ATTENDU, DERIVE des bornes du fichier : t = 0.75 sur [0.5, 3.0]
					// -> 0.5 + 0.75 * 2.5 = 2.375
					const float32 attendu = 0.5f + 0.75f * (3.0f - 0.5f);
					float32 lu = 0.f;
					Check(s.exe.modele.Lire(NkStringView("ui.echelle"), lu),
						  "(b4) le modele porte toujours le chemin");
					CheckEqF(lu, attendu, 0.02f,
							 "(b4) LE WIDGET A BOUGE -> la donnee vaut 0.5 + 0.75*(3.0-0.5)");
					Check(s.exe.modele.Ecritures(NkStringView("ui.echelle")) > ecrituresAvant,
						  "(b4) et le modele a bien ete ECRIT (pas une valeur egale par hasard)");
					s.exe.PoserBouton(s.ctx, 0, false);
					s.Image();
				}
				s.exe.Debrancher(s.ctx);
			}
			Liberer(f);
		}
	}

	// =====================================================================
	printf("\n-- (m) MUTATIONS : je casse expres, et j'exige qu'un critere rougisse\n");
	// =====================================================================
	{
		// Une garde verte peut ne rien garder. Trois mutations, chacune sur un
		// mecanisme different, chacune lancee contre le critere qui doit
		// l'attraper. Elles ne modifient AUCUN fichier : le document mute est
		// ecrit ici.
		// MUTATION 1 : la couleur Hover du document est changee. Si le banc
		//              mesurait « l'image a change » au lieu de « la couleur du
		//              fichier est presente », il resterait vert.
		static const char kMute[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Button \"valider\" {\n"
			"    label = \"Valider\"\n"
			"    appearance { fill { color = #2F6F7A } }\n"
			"    appearance(Hover) { fill { color = #FF0000 } }\n"
			"  }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kMute, (uint32)(sizeof(kMute) - 1u), 320, 120),
			  "(m1) le document mute se charge");
		s.Image();
		s.Image();
		NkRect r{0.f, 0.f, 0.f, 0.f};
		if (s.Rect("valider", r)) {
			s.PoserEtStabiliser(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
			Check(ComptePixelsCouleur(s.ras, 0xFF0000FFu) > 0u,
				  "(m1) le survol peint la NOUVELLE couleur (#FF0000)");
			CheckEqU(ComptePixelsCouleur(s.ras, 0x3A8894FFu), 0u,
					 "(m1) et PLUS AUCUN pixel de l'ancienne (#3A8894) -- le critere (b1.a) "
					 "aurait rougi");
		}
		s.exe.Debrancher(s.ctx);
	}
	{
		// MUTATION 2 : le seuil du `if` est deplace sous la valeur. Le callback
		//              doit alors partir pour une valeur qui ne le declenchait pas.
		static const char kMute[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Slider \"n1\" { bind = modele.valeur\n    min = 0, max = 1 }\n"
			"}\n"
			"behavior \"seuil\" {\n"
			"  set r = n1.value * 100\n"
			"  if n1.value > 0.3 {\n"
			"    Callback \"alerte\"(Enum.Haut)\n"
			"  }\n"
			"}\n";
		Scene s;
		Recepteur rec;
		Check(s.Charger(kMute, (uint32)(sizeof(kMute) - 1u), 320, 120),
			  "(m2) le document a seuil deplace se charge");
		s.exe.eval.rappel = &SurCallback;
		s.exe.eval.rappelUser = &rec;
		s.exe.modele.Poser(NkStringView("modele.valeur"), 0.50f);
		s.Image();
		CheckEqU(rec.appels, 1u,
				 "(m2) 0.50 > 0.30 -> l'appel PART (il ne partait pas avec le seuil a 0.8)");
		s.exe.Debrancher(s.ctx);
	}
	{
		// MUTATION 3 : le `bind` du curseur est renomme. La liaison doit alors
		//              CESSER de suivre `ui.echelle` -- une liaison qui marcherait
		//              quel que soit le nom ne lierait rien.
		static const char kMute[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Slider \"echelle\" { bind = ui.autreChose\n    min = 0.5, max = 3.0 }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kMute, (uint32)(sizeof(kMute) - 1u), 460, 120),
			  "(m3) le document a `bind` renomme se charge");
		s.exe.modele.Poser(NkStringView("ui.echelle"), 3.0f);
		s.Image();
		s.Image();
		NkGuiMonteEtat::Entree *e = s.etat.Get(NkStringView("ui.autreChose"));
		Check(e != nullptr, "(m3) la cle d'etat suit le NOUVEAU `bind`");
		if (e)
			CheckEqF(e->f, 0.5f, 0.001f,
					 "(m3) et le curseur IGNORE `ui.echelle` -- la liaison est bien nominative");
		Check(s.etat.Get(NkStringView("ui.echelle")) == nullptr,
			  "(m3) l'ancienne cle n'existe plus du tout");
		s.exe.Debrancher(s.ctx);
	}

	printf("\n=== %d / %d ===\n", g_pass, g_pass + g_fail);
	return g_fail == 0 ? 0 : 1;
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
