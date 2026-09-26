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

// Le centre de gravite en X des pixels d'une bande horizontale qui ne sont ni
// le fond ni l'aplat -- sert a LOCALISER en pixels la poignee d'un curseur.
// Rend -1 quand la bande est vide.
static float32 CentreXBande(const NkGuiDrawListRaster &r, int32 y0, int32 y1, int32 x0, int32 x1,
							uint32 fond, uint32 aplat) {
	float64 somme = 0.0;
	uint32 n = 0;
	for (int32 y = y0; y < y1; ++y)
		for (int32 x = x0; x < x1; ++x) {
			const uint32 p = r.Pixel(x, y);
			if (p != fond && p != aplat) {
				somme += (float64)x;
				++n;
			}
		}
	return n == 0u ? -1.f : (float32)(somme / (float64)n);
}

// Le centre en X des pixels d'une COULEUR PRECISE. Mesurer une poignee sur toute
// une bande la noie dans le rail : c'est sa couleur qui la designe, pas sa zone.
static float32 CentreXCouleur(const NkGuiDrawListRaster &r, uint32 couleur, uint32 *outN) {
	float64 somme = 0.0;
	uint32 n = 0;
	for (int32 y = 0; y < r.Hauteur(); ++y)
		for (int32 x = 0; x < r.Largeur(); ++x)
			if (r.Pixel(x, y) == couleur) {
				somme += (float64)x;
				++n;
			}
	if (outN)
		*outN = n;
	return n == 0u ? -1.f : (float32)(somme / (float64)n);
}

static float32 Luminance(uint32 rgba) {
	const float32 rr = (float32)((rgba >> 24) & 0xFFu);
	const float32 gg = (float32)((rgba >> 16) & 0xFFu);
	const float32 bb = (float32)((rgba >> 8) & 0xFFu);
	return 0.2126f * rr + 0.7152f * gg + 0.0722f * bb;
}

// La luminance MAXIMALE parmi les pixels qui ne sont ni le fond ni l'aplat. Le
// coeur d'un glyphe atteint la couleur pleine du texte : ce maximum est donc la
// couleur avec laquelle le texte a ete PEINT, antialiasing mis de cote.
static float32 LuminanceMaxContenu(const NkGuiDrawListRaster &r, uint32 fond, uint32 aplat,
								   int32 x0 = 0, int32 x1 = 0) {
	float32 best = -1.f;
	if (x1 <= x0) {
		x0 = 0;
		x1 = r.Largeur();
	}
	for (int32 y = 0; y < r.Hauteur(); ++y)
		for (int32 x = x0; x < x1 && x < r.Largeur(); ++x) {
			const uint32 p = r.Pixel(x, y);
			if (p == fond || p == aplat)
				continue;
			const float32 l = Luminance(p);
			if (l > best)
				best = l;
		}
	return best;
}

// Les pixels de TEXTE d'une bande : ceux qui different de la couleur dominante
// DE CETTE BANDE -- c'est-a-dire du fond du champ, et non du fond de l'image.
//
// ⚠️ POURQUOI PAS `ComptePixelsContenu` : il compte les pixels non-fond, et le
//    texte d'un champ est peint DANS le champ, sur des pixels deja comptes.
//    Champ nu, champ a invite et champ a saisie rendaient tous 9773 -- un
//    compteur parfaitement stable, et parfaitement aveugle a ce qu'on mesure.
static uint32 PixelsTexteBande(const NkGuiDrawListRaster &r, int32 x0, int32 x1) {
	if (x1 <= x0)
		return 0u;
	uint32 couleurs[64];
	uint32 comptes[64];
	uint32 nc = 0;
	for (int32 y = 0; y < r.Hauteur(); ++y)
		for (int32 x = x0; x < x1 && x < r.Largeur(); ++x) {
			const uint32 p = r.Pixel(x, y);
			uint32 i = 0;
			for (; i < nc; ++i)
				if (couleurs[i] == p) {
					++comptes[i];
					break;
				}
			if (i == nc && nc < 64u) {
				couleurs[nc] = p;
				comptes[nc] = 1u;
				++nc;
			}
		}
	uint32 dom = 0, domN = 0, total = 0;
	for (uint32 i = 0; i < nc; ++i) {
		total += comptes[i];
		if (comptes[i] > domN) {
			domN = comptes[i];
			dom = couleurs[i];
		}
	}
	(void)dom;
	(void)total;
	// Le fond de la bande est la couleur dominante ; tout le reste est du dessin.
	uint32 n = 0;
	for (int32 y = 0; y < r.Hauteur(); ++y)
		for (int32 x = x0; x < x1 && x < r.Largeur(); ++x)
			if (r.Pixel(x, y) != dom)
				++n;
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

// Les bornes du premier `Slider` du document. L'attendu de (i1) se LIT dans le
// fichier : un `0.5` recopie dans le banc se perimerait a la premiere retouche.
static void TrouverBornesSlider(const NkArchive &bloc, float32 &vmin, float32 &vmax) {
	const NkArchiveNode *c = NkGMonteCorps(bloc);
	if (!c)
		return;
	for (uint32 i = 0; i < (uint32)c->array.Size(); ++i) {
		if (!c->array[i].IsObject() || !c->array[i].object)
			continue;
		const NkArchive &n = *c->array[i].object;
		if (NkGMotEgal(NkGuiArchive::TypeOf(n), "Slider")) {
			vmin = NkGNombre(n, "min", -1.f);
			vmax = NkGNombre(n, "max", -1.f);
			return;
		}
		TrouverBornesSlider(n, vmin, vmax);
		if (vmin >= 0.f)
			return;
	}
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
		float32 bandeCentreX = -1.f; ///< centre en X des pixels d'une bande (option)
		float32 poigneeX = -1.f;	 ///< centre en X des pixels de la couleur ciblee
		uint32 poigneeN = 0;		 ///< combien de pixels portaient cette couleur
		float32 lumMaxContenu = -1.f; ///< luminance du texte reellement peint
		uint32 pixelsTexteBande = 0; ///< pixels de dessin dans la bande demandee
		/// Copie des pixels, quand l'appelant l'a demandee (`g_garderPixels`).
		/// Sert aux comparaisons image contre image -- la seule mesure qui voit un
		/// texte peint SUR un aplat.
		NkVector<uint8> px;
};

// Bande a mesurer (option posee par l'appelant juste avant un montage).
static bool g_mesureBande = false;
static int32 g_bandeY0 = 0, g_bandeY1 = 0, g_bandeX0 = 0, g_bandeX1 = 0;
/// Couleur a localiser (0 = aucune). Posee juste avant un montage.
static uint32 g_couleurCible = 0u;
/// Bande horizontale ou mesurer la luminance du texte (x1 <= x0 = toute l'image).
static int32 g_lumX0 = 0, g_lumX1 = 0;
/// Garder une copie des pixels du prochain montage (pour comparer deux images).
static bool g_garderPixels = false;

// ── LA ZONE HOTE : le remplisseur de l'application d'essai (m5) ──────────────
// ⚠️ LA SIGNATURE EST UNE COULEUR QUE RIEN D'AUTRE NE PEINT. Un remplisseur qui
//    peindrait dans les tons du theme serait indistinguable du marqueur de zone
//    vide, et le banc mesurerait « des pixels » au lieu de mesurer SON contenu.
static const uint32 kSignatureHote = 0xFF00FFFFu; // magenta opaque, R<<24|G<<16|B<<8|A

static bool NomEgal(const char *a, const char *b) {
	if (!a || !b)
		return false;
	while (*a && *b && *a == *b) {
		++a;
		++b;
	}
	return *a == '\0' && *b == '\0';
}

struct RemplisseurHote : nkentseu::nkgui::NkGuiMonteHooks {
		const char *cible = nullptr; ///< le seul nom que cet hote sait remplir
		NkRect derniere{};
		uint32 remplis = 0;
		bool RemplirHote(NkGuiContext &ctx, const char *nom, const NkRect &zone) noexcept override {
			// UN HOTE QUI NE CONNAIT PAS LE NOM REPOND NON. C'est le contrat : repondre
			// oui sans peindre ferait disparaitre la zone en silence.
			if (!NomEgal(nom, cible))
				return false;
			ctx.DL().AddRectFilled(zone, NkColor{255, 0, 255, 255});
			derniere = zone;
			++remplis;
			return true;
		}
};
/// Pose juste avant un montage, comme les autres options de ce banc.
static nkentseu::nkgui::NkGuiMonteHooks *g_hooks = nullptr;

/// LE TEMOIN D'UNE ZONE NON REMPLIE : il note le rectangle et repond NON.
///
/// ⚠️ IL EXISTE PARCE QU'UN COMPTEUR SATURE NE DISTINGUE RIEN. Le premier critere
///    du marqueur etait « l'image peint des pixels » : le Panel en peint 239 980 a
///    lui seul, et le compte etait IDENTIQUE avec et sans marqueur. Un critere qui
///    rend le meme nombre dans les deux cas ne teste rien. On compte donc DANS LE
///    RECTANGLE DE LA ZONE, et on garde le rectangle par ce temoin.
struct TemoinHote : nkentseu::nkgui::NkGuiMonteHooks {
		const char *cible = nullptr;
		NkRect zone{};
		bool vu = false;
		bool RemplirHote(NkGuiContext &ctx, const char *nom, const NkRect &r) noexcept override {
			(void)ctx;
			if (NomEgal(nom, cible)) {
				zone = r;
				vu = true;
			}
			return false; // il REGARDE, il ne remplit pas
		}
};

/// Combien de pixels, DANS ce rectangle, S'ECARTENT EN LUMINANCE de la dominante du
/// rectangle lui-meme. Sur une zone vide, la dominante est l'aplat du panneau :
/// tout ce qui compte ici est donc ce que le marqueur a trace.
static uint32 ComptePixelsDansRect(const NkVector<uint8> &px, int32 W, int32 H, const NkRect &r) {
	if (px.Size() == 0u || r.w < 1.f || r.h < 1.f)
		return 0u;
	const int32 x0 = (int32)(r.x + 1.f), y0 = (int32)(r.y + 1.f);
	const int32 x1 = (int32)(r.x + r.w - 1.f), y1 = (int32)(r.y + r.h - 1.f);
	// La dominante du rectangle, comptee sur lui et non sur l'image entiere.
	uint32 couleurs[64] = {};
	uint32 comptes[64] = {};
	uint32 n = 0;
	for (int32 y = y0; y < y1; ++y)
		for (int32 x = x0; x < x1; ++x) {
			const usize i = ((usize)y * (usize)W + (usize)x) * 4u;
			if (i + 3u >= px.Size())
				continue;
			const uint32 c = ((uint32)px[(uint32)i] << 24) | ((uint32)px[(uint32)i + 1u] << 16)
							 | ((uint32)px[(uint32)i + 2u] << 8) | (uint32)px[(uint32)i + 3u];
			uint32 k = 0;
			for (; k < n; ++k)
				if (couleurs[k] == c) {
					++comptes[k];
					break;
				}
			if (k == n && n < 64u) {
				couleurs[n] = c;
				comptes[n] = 1u;
				++n;
			}
		}
	uint32 best = 0u, bestN = 0u;
	for (uint32 k = 0; k < n; ++k)
		if (comptes[k] > bestN) {
			bestN = comptes[k];
			best = couleurs[k];
		}
	uint32 autres = 0u;
	for (int32 y = y0; y < y1; ++y)
		for (int32 x = x0; x < x1; ++x) {
			const usize i = ((usize)y * (usize)W + (usize)x) * 4u;
			if (i + 3u >= px.Size())
				continue;
			const uint32 c = ((uint32)px[(uint32)i] << 24) | ((uint32)px[(uint32)i + 1u] << 16)
							 | ((uint32)px[(uint32)i + 2u] << 8) | (uint32)px[(uint32)i + 3u];
			// ⚠️ « DIFFERENT » NE VEUT PAS DIRE « VISIBLE ». La premiere version comptait
			//    toute couleur autre que la dominante : le marqueur trace en `theme.border`
			//    rendait 3 778 pixels, le critere passait au vert, et l'image ne montrait
			//    RIEN -- la bordure est a 9 de luminance de l'aplat. On exige donc un ecart
			//    de luminance qu'un oeil distingue.
			const float32 dl = Luminance(c) - Luminance(best);
			if (dl > 20.f || dl < -20.f)
				++autres;
		}
	(void)H;
	return autres;
}

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

// Combien de pixels DIFFERENT entre deux montages. Zero quand les deux images
// sont identiques au bit -- c'est le zero prouvable de ce compteur, et il est
// verifie juste avant de s'en servir.
static uint32 PixelsQuiDifferent(const Montage &a, const Montage &b) {
	if (a.px.Size() == 0u || a.px.Size() != b.px.Size())
		return 0xFFFFFFFFu; // tailles incomparables : ce n'est pas « aucune difference »
	uint32 n = 0;
	for (uint32 i = 0; i + 3u < (uint32)a.px.Size(); i += 4u)
		if (a.px[i] != b.px[i] || a.px[i + 1u] != b.px[i + 1u] || a.px[i + 2u] != b.px[i + 2u]
			|| a.px[i + 3u] != b.px[i + 3u])
			++n;
	return n;
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
	NkGuiMonteur::Monter(ctx, doc, etat, m.rap, g_hooks);

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
		if (g_mesureBande) {
			m.bandeCentreX = CentreXBande(ras, g_bandeY0, g_bandeY1, g_bandeX0, g_bandeX1, kFond,
										  CouleurDominante(ras));
		}
		if (g_couleurCible != 0u)
			m.poigneeX = CentreXCouleur(ras, g_couleurCible, &m.poigneeN);
		// ⚠️ La luminance se mesure DANS LA ZONE DEMANDEE quand il y en a une :
		//    un champ de saisie peint AUSSI son libelle, toujours en couleur
		//    normale, et un maximum pris sur toute l'image tombe dessus.
		m.lumMaxContenu = LuminanceMaxContenu(ras, kFond, CouleurDominante(ras), g_lumX0, g_lumX1);
		if (g_lumX1 > g_lumX0)
			m.pixelsTexteBande = PixelsTexteBande(ras, g_lumX0, g_lumX1);
		if (g_garderPixels) {
			const usize nb = (usize)ras.Largeur() * (usize)ras.Hauteur() * 4u;
			m.px.Resize(nb);
			const uint8 *sp = ras.Pixels();
			for (usize i = 0; i < nb; ++i)
				m.px[(uint32)i] = sp[i];
		}
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

// =============================================================================
//  `--monter=<fichier.nkgui>` -- MONTER UN FICHIER QUELCONQUE, ET RENDRE UN
//  VERDICT CHIFFRE. C'est le TROISIEME NIVEAU du jeu d'epreuve de l'IA : le
//  document produit par un modele ne compte comme « ouvert » que si ce binaire-ci
//  -- qui ne partage AUCUNE ligne avec celui qui l'a ecrit -- le lit, le valide et
//  le monte sans role inconnu, sans zone hachuree et sans hote vide.
//
//  Le code de sortie EST le verdict : 0 monte proprement, 1 refus.
// =============================================================================
static int MonterUnFichier(const char *chemin) {
	printf("=== MONTAGE D'UN FICHIER : %s ===\n", chemin);
	const Fichier f = Lire(chemin);
	if (!f.data) {
		printf("  [ REFUS ] illisible ou vide\n");
		return 1;
	}
	// 1. l'ANALYSE et la VALIDATION -- le meme validateur que le corpus.
	NkArchive doc;
	NkVector<NkGuiDiag> diags;
	NkGuiDiag err;
	if (!NkGuiArchive::Read(f.data, f.taille, doc, err)) {
		printf("  [ REFUS ] l'analyseur refuse le fichier\n");
		return 1;
	}
	const nkuidesign::guifmt::NkGValidateResult v =
		nkuidesign::guifmt::NkGValidate(doc, diags);
	printf("  validation : %u erreur(s), %u avertissement(s)\n", v.errors, v.warnings);
	for (uint32 i = 0; i < (uint32)diags.Size() && i < 8u; ++i)
		printf("      %s\n", diags[i].message.CStr());
	// 2. LE MONTAGE, en widgets reels, rasterise en logiciel.
	// ⚠️ ET SON IMAGE, quand `--png=<dossier>` est pose. Les nombres de la ligne
	//    suivante ne peuvent PAS voir un widget monte qui ne peint rien -- c'est
	//    la troisieme fois de la semaine que l'image tranche ce que les compteurs
	//    validaient. Le nom est fixe (`monte.png`) : une seule course, un seul
	//    fichier, jamais une trace ecrasee par une autre course sans le dire.
	const Montage m = MonterTexte(f.data, f.taille, 1000, 700, g_pngDir ? "monte.png" : nullptr);
	printf("  montage    : %u monte(s), %u role(s) inconnu(s), %u hote(s) non rempli(s), "
		   "%u zone(s) sans panneau, %u liste(s) liee(s) non remplie(s)\n",
		   m.rap.montes, m.rap.rolesInconnus, m.rap.hotesNonRemplis, m.rap.zonesSansPanneau,
		   m.rap.listesNonRemplies);
	// ⚠️ LE RECENSEMENT DES ROLES, ET IL N'EST PAS DECORATIF. Un document fait de
	//    `Group` vides passe TOUS les compteurs ci-dessus : il se lit, il se
	//    valide, il monte, et il ne montre RIEN. C'est le meme piege que la bande
	//    noire de 197 px du 17/09 -- un vert qui ne regarde pas ce qu'il compte.
	//    On publie donc le nombre de widgets qui ne sont PAS de simples groupes.
	uint32 nonGroupes = 0u;
	for (uint32 i = 0; i < (uint32)m.rap.items.Size(); ++i) {
		const NkString &r = m.rap.items[i].role;
		// ⚠️ `Scroll` EST UN CONTENEUR, ET IL EST ARRIVE DANS CETTE LISTE APRES COUP.
		//    Le 17/09, le monteur a appris a le monter, et ce compteur a saute de 4 a
		//    5 sans qu'aucun widget utile n'apparaisse. Le but ecrit de la ligne est
		//    de refuser de compter ce qui ne MONTRE rien : une zone defilante vide
		//    est exactement le meme piege qu'un `Group` vide. *Une liste de noms
		//    ecrite en dur se perime des qu'un role naît ; celle-ci se corrige ici, et
		//    le jour ou elle se perimera encore, ce commentaire dit pourquoi.*
		// ⚠️ ET ELLE S'EST PERIMEE UNE SECONDE FOIS, LE 26/09, EXACTEMENT COMME
		//    LA LIGNE AU-DESSUS L'AVAIT PREDIT. Six roles de CONTENEUR sont nes ce
		//    jour-la : `MenuBar`, `Menu`, `Flow`, `Grid`, et `Row`/`Column` (ces
		//    deux derniers devenus synonymes de `HBox`/`VBox`). Le compteur les
		//    prenait pour des widgets utiles.
		//
		//    MESURE QUI L'A REVELE : le MEME document, ecrit en `HBox`/`VBox` puis
		//    en `Row`/`Column`, montait 7 widgets dans les deux cas -- mais rendait
		//    **4 roles utiles contre 6**. Le montage etait identique ; c'est le
		//    RELEVE qui differait.
		//
		//    `MenuItem` N'EST PAS dans la liste, et c'est voulu : une entree de menu
		//    MONTRE quelque chose, elle n'est pas un simple conteneur.
		if (r.Compare("Group") != 0 && r.Compare("VBox") != 0 && r.Compare("HBox") != 0
			&& r.Compare("Panel") != 0 && r.Compare("Window") != 0 && r.Compare("Scroll") != 0
			&& r.Compare("Row") != 0 && r.Compare("Column") != 0 && r.Compare("Flow") != 0
			&& r.Compare("Grid") != 0 && r.Compare("MenuBar") != 0 && r.Compare("Menu") != 0)
			++nonGroupes;
	}
	printf("  roles utiles: %u widget(s) qui ne sont pas un simple conteneur\n", nonGroupes);
	// ⚠️ DEUX COMPTEURS QUE PERSONNE N'IMPRIMAIT N'AURAIENT SERVI A PERSONNE.
	//    `apparencesLues` disait combien de blocs le document ecrit ; il ne disait
	//    pas combien ont ete HONORES. Depuis le 17/09 le monteur peint l'apparence
	//    au repos des quatre roles qui ont une surface, et compte a part ce qu'il
	//    ne sait pas rendre. Les deux se lisent ici, cote a cote, parce qu'un
	//    compteur qu'on n'imprime pas est un compteur que personne ne verifie.
	printf("  debordement: %u widget(s) sortent de la region de leur conteneur ; \n"
		   "               %u espace(s) flexible(s) non honore(s)\n",
		   m.rap.debordements, m.rap.espacesFlexibles);
	// ⚠️ UN COMPTE N'ACCUSE PERSONNE. On nomme les quatre premiers, avec leur
	//    role : c'est ce qui a evite d'attribuer au `Spacer` trois debordements
	//    que le temoin SANS `Spacer` rendait deja.
	for (uint32 i = 0, vus = 0; i < (uint32)m.rap.items.Size() && vus < 4u; ++i)
		if (m.rap.items[i].deborde) {
			printf("      deborde : %s \"%s\" rect (%.0f,%.0f %.0fx%.0f)\n",
				   m.rap.items[i].role.CStr(), m.rap.items[i].id.CStr(),
				   m.rap.items[i].rect.x, m.rap.items[i].rect.y,
				   m.rap.items[i].rect.w, m.rap.items[i].rect.h);
			++vus;
		}
	printf("  apparence  : %u lue(s), %u peinte(s), %u NON peinte(s)\n",
		   m.rap.apparencesLues, m.rap.apparencesPeintes, m.rap.apparencesNonPeintes);
	const bool ok = m.lu && v.errors == 0u && m.rap.rolesInconnus == 0u
					&& m.rap.hotesNonRemplis == 0u && m.rap.zonesSansPanneau == 0u
					&& m.rap.montes > 0u;
	// ⚠️ UN HOTE NON REMPLI N'EST PAS UNE FAUTE DU DOCUMENT quand l'hote est ce
	//    banc, qui n'en remplit aucun : c'est pourquoi le verdict le compte a part
	//    et que la ligne ci-dessus le dit. Pour le jeu d'epreuve de l'IA, les
	//    documents produits ne declarent ni `Host` ni `DockSpace` -- si un jour ils
	//    en declarent, ce verdict devra recevoir un hote, et non etre assoupli.
	printf("  VERDICT    : %s\n", ok ? "MONTE" : "REFUSE");
	return ok ? 0 : 1;
}

int main(int argc, char **argv) {
	const char *racine = "Applications/NKUIDesign/exemples";
	// ⚠️ `--png=` SE LIT AVANT `--monter=`, ET CE N'EST PAS UN DETAIL D'ORDRE :
	//    `--monter=` REND. Une boucle qui le traitait en premier faisait sortir le
	//    programme avant d'avoir lu `--png=`, et l'image demandee n'etait jamais
	//    ecrite -- sans un mot. Une option silencieusement ignoree est la forme la
	//    moins visible du « declarer n'est pas livrer ».
	for (int32 a = 1; a < (int32)argc; ++a) {
		// `--png=<dossier>` : ecrire une image par fichier monte. OPT-IN.
		if (argv[a][0] == '-' && argv[a][1] == '-' && argv[a][2] == 'p' && argv[a][3] == 'n'
			&& argv[a][4] == 'g' && argv[a][5] == '=')
			g_pngDir = argv[a] + 6;
		else if (argv[a][0] != '-')
			racine = argv[a];
	}
	for (int32 a = 1; a < (int32)argc; ++a) {
		if (argv[a][0] == '-' && argv[a][1] == '-' && argv[a][2] == 'm' && argv[a][3] == 'o'
			&& argv[a][4] == 'n' && argv[a][5] == 't' && argv[a][6] == 'e' && argv[a][7] == 'r'
			&& argv[a][8] == '=')
			return MonterUnFichier(argv[a] + 9);
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
	// =====================================================================
	// (i1)(i2)(i3) LES TROIS INFIDELITES TROUVEES PAR LES IMAGES
	//
	// Elles n'ont ete vues par AUCUN des 138 criteres precedents. Chacune recoit
	// ici le critere qui l'aurait attrapee, et chacun a ete verifie ROUGE sur le
	// code d'avant (campagne de mutations i1/i2/i3).
	// =====================================================================
	printf("\n-- (i1) le curseur part de la valeur du fichier, DANS ses bornes\n");
	{
		// Le fichier ecrit `min = 0.5, max = 3.0` et AUCUNE `value`. La valeur
		// montee doit donc etre 0.5 -- la borne basse ECRITE -- et non 0.
		float32 vue = -1.f;
		bool trouve = false;
		for (uint32 i = 0; i < (uint32)m01.rap.items.Size(); ++i) {
			const NkGuiMonteItem &it = m01.rap.items[i];
			if (it.role.Compare(NkString("Slider")) == 0 && it.aValeur) {
				vue = it.valeur;
				trouve = true;
				break;
			}
		}
		Check(trouve, "   le curseur du fichier est monte et porte une valeur");
		// L'attendu est LU dans le fichier, pas recopie ici.
		float32 vmin = -1.f, vmax = -1.f;
		{
			Joindre(dossier, sizeof(dossier), racine, "/valides/");
			Joindre(chemin, sizeof(chemin), dossier, "01_panneau_reglages.nkgui");
			Fichier f = Lire(chemin);
			if (f.ok) {
				NkArchive doc;
				NkGuiDiag err;
				if (NkGuiArchive::Read(f.data, f.taille, doc, err))
					TrouverBornesSlider(doc, vmin, vmax);
				Liberer(f);
			}
		}
		printf("        bornes LUES dans le fichier : [%.2f, %.2f] ; valeur montee : %.2f\n",
			   (double)vmin, (double)vmax, (double)vue);
		Check(vmin >= 0.f && vmax > vmin, "   les bornes se lisent dans le fichier");
		Check(trouve && vue >= vmin - 0.001f && vue <= vmax + 0.001f,
			  "   la valeur montee est DANS les bornes ecrites");
		Check(trouve && Abs(vue - vmin) < 0.001f,
			  "   sans `value`, elle vaut EXACTEMENT la borne basse du fichier");

		// ⚠️ ET LA MESURE EN PIXELS, QUI NE SUFFIT PAS -- c'est dit, pas cache.
		//    `SliderFloat` borne t = (v - min)/(max - min) a [0,1] : 0.0 et 0.5
		//    tombent tous deux a l'extremite gauche du rail. Une mesure de la
		//    poignee sur CE fichier rend le meme pixel avant et apres le
		//    correctif. Elle est donc faite sur un document ou elle SEPARE.
		{
			const char *doc1 =
				"nkgui 0.3\nwidgets {\n  Slider \"s\" { min = 0, max = 10, value = 5 }\n}\n";
			const char *doc2 =
				"nkgui 0.3\nwidgets {\n  Slider \"s\" { min = 0, max = 10, value = 2.5 }\n}\n";
			uint32 n1 = 0, n2 = 0;
			while (doc1[n1]) ++n1;
			while (doc2[n2]) ++n2;

			// ⚠️ LA POIGNEE SE REPERE PAR SA COULEUR, PAS PAR SA ZONE. Un premier
			//    releve prenait le centre de gravite de toute la bande -- rail,
			//    remplissage et libelle compris -- et la poignee y pesait si peu
			//    que 25 % de course ne deplacait le chiffre que de 2,91 px. Le
			//    critere passait, et il serait passe aussi pour 1 px. La couleur
			//    de la poignee au repos est `theme.buttonHover` ; on la prend au
			//    theme, on ne la recopie pas.
			NkGuiContext ref;
			const NkColor cp = ref.theme.buttonHover;
			g_couleurCible = ((uint32)cp.r << 24) | ((uint32)cp.g << 16) | ((uint32)cp.b << 8)
							 | (uint32)cp.a;
			const Montage a = MonterTexte(doc1, n1, 400, 200);
			const Montage b = MonterTexte(doc2, n2, 400, 200);
			g_couleurCible = 0u;

			printf("        poignee : value=5 -> X %.2f (%u px) ;  value=2.5 -> X %.2f (%u px)\n",
				   (double)a.poigneeX, a.poigneeN, (double)b.poigneeX, b.poigneeN);
			Check(a.lu && b.lu, "   les deux documents de mesure se lisent");
			Check(a.poigneeN > 0u && b.poigneeN > 0u,
				  "   la poignee est localisable par sa couleur dans les deux");

			// L'ECART ATTENDU EST DERIVE, pas recopie : la course vaut
			// `track.w = max(largeur * 0.55, 40)` et les deux documents ecrivent
			// `min = 0, max = 10` avec `value` 5 puis 2.5, soit un quart de course.
			const float32 largeurItem = 400.f - 2.f * ref.layout.padding;
			const float32 trackW = (largeurItem * 0.55f > 40.f) ? largeurItem * 0.55f : 40.f;
			const float32 attendu = trackW * 0.25f;
			const float32 mesure = a.poigneeX - b.poigneeX;
			printf("        ecart mesure %.2f px, attendu ~%.2f (quart de course sur %.1f)\n",
				   (double)mesure, (double)attendu, (double)trackW);
			Check(mesure > 0.f, "   NEGATIF : une valeur plus basse met la poignee A GAUCHE");
			Check(Abs(mesure - attendu) < 6.f,
				  "   et elle se deplace de la FRACTION DE COURSE attendue, pas d'un pixel");
		}
	}

	printf("\n-- (i2) un `placeholder` ne se peint PAS comme une saisie\n");
	{
		// Le plus trompeur des trois : rien ne distinguait une invite d'une
		// valeur reelle. Le critere compare les COULEURS peintes.
		const char *invite =
			"nkgui 0.3\nwidgets {\n  TextField \"t\" { placeholder = \"Filtrer...\" }\n}\n";
		const char *saisie =
			"nkgui 0.3\nwidgets {\n  TextField \"t\" { value = \"Filtrer...\" }\n}\n";
		uint32 n1 = 0, n2 = 0;
		while (invite[n1]) ++n1;
		while (saisie[n2]) ++n2;
		// La bande du CHAMP : le texte commence a `field.x + 6`, c'est-a-dire au
		// padding de la region plus le padding interne du champ. On s'arrete bien
		// avant le libelle, qui est peint APRES la fin du champ.
		NkGuiContext refZ;
		g_lumX0 = (int32)refZ.layout.padding;
		g_lumX1 = g_lumX0 + 140;
		g_garderPixels = true;
		const Montage a = MonterTexte(invite, n1, 400, 120);
		const Montage b = MonterTexte(saisie, n2, 400, 120);
		printf("        bande de mesure : x de %d a %d (le champ, PAS son libelle)\n",
			   (int32)refZ.layout.padding, (int32)refZ.layout.padding + 140);
		Check(a.lu && b.lu, "   les deux documents se lisent");

		// ⚠️ LE TIERS TEMOIN, ET IL MANQUAIT. « contenu > 0 » etait satisfait par le
		//    CADRE du champ : un champ vide en peint deja un. Le critere restait donc
		//    vert quand l'invite n'etait pas peinte DU TOUT -- la deuxieme forme du
		//    defaut (mutation i2b). La reference est un champ SANS placeholder ni
		//    valeur : c'est la seule chose qui dise si l'invite a ete peinte.
		//    Meme forme que le negatif de (i3), ou `title = ""` sert de reference.
		const char *nu =
			"nkgui 0.3\nwidgets {\n  TextField \"t\" { }\n}\n";
		uint32 n3 = 0;
		while (nu[n3]) ++n3;
		g_lumX0 = (int32)refZ.layout.padding;
		g_lumX1 = g_lumX0 + 140;
		const Montage z = MonterTexte(nu, n3, 400, 120);
		// Le meme document, monte une seconde fois : c'est le ZERO du comparateur.
		const Montage z2 = MonterTexte(nu, n3, 400, 120);
		g_garderPixels = false;
		g_lumX0 = 0;
		g_lumX1 = 0;
		Check(z.lu && z2.lu, "   le champ nu se lit");
		// ⚠️ LE ZERO DU COMPARATEUR, PROUVE AVANT DE S'EN SERVIR. Deux montages du
		//    MEME document ne doivent differer d'aucun pixel. Sans ce controle, un
		//    comparateur qui rendrait n'importe quoi ferait passer les deux
		//    suivants pour des preuves.
		CheckEq(PixelsQuiDifferent(z, z2), 0u, "   ZERO : deux montages du champ nu ne different pas");
		const uint32 dInvite = PixelsQuiDifferent(z, a);
		const uint32 dSaisie = PixelsQuiDifferent(z, b);
		printf("        pixels changes par rapport au champ nu : invite %u | saisie %u\n",
			   dInvite, dSaisie);
		// ⚠️ POURQUOI UNE COMPARAISON D'IMAGES ET PAS UN COMPTE. Deux mesures ont
		//    echoue avant celle-ci, et pour la MEME raison structurelle : elles
		//    comptaient les pixels differant d'une couleur de reference dans une
		//    zone. Or le texte d'un champ est peint DANS le champ, sur des pixels
		//    qui differaient deja de cette reference -- champ nu, invite et saisie
		//    rendaient tous 9773, puis tous 3770. Repeindre un pixel deja compte
		//    ne change aucun compte. Seule la comparaison de deux images voit un
		//    changement de COULEUR au lieu d'un changement de SURFACE.
		Check(dInvite > 0u, "   l'invite du fichier PEINT quelque chose qu'un champ nu n'a pas");
		Check(dSaisie > 0u, "   une saisie aussi (contre-epreuve)");
		// MEME texte, MEMES pixels de fond : si les deux images sont identiques,
		// c'est que l'invite est peinte comme une saisie -- le defaut d'origine.
		Check(a.empreinte != b.empreinte,
			  "   et elle ne se peint PAS comme une saisie (images differentes)");
		// ⚠️ « IMAGES DIFFERENTES » NE DIT PAS POURQUOI. Le releve le montre
		//    crument : l'invite et la saisie peignent EXACTEMENT le meme nombre
		//    de pixels, au meme endroit. **Seule la couleur change**, et c'est
		//    justement ce qu'il faut mesurer. La luminance du texte peint doit
		//    valoir celle de `textDisabled` pour l'invite, celle de `theme.text`
		//    pour la saisie -- les deux prises AU THEME.
		NkGuiContext ref2;
		const float32 lumGrise = Luminance(((uint32)ref2.theme.textDisabled.r << 24)
										   | ((uint32)ref2.theme.textDisabled.g << 16)
										   | ((uint32)ref2.theme.textDisabled.b << 8) | 255u);
		const float32 lumNormale = Luminance(((uint32)ref2.theme.text.r << 24)
											 | ((uint32)ref2.theme.text.g << 16)
											 | ((uint32)ref2.theme.text.b << 8) | 255u);
		printf("        invite : %u px, luminance %.1f  |  saisie : %u px, luminance %.1f\n",
			   a.contenu, (double)a.lumMaxContenu, b.contenu, (double)b.lumMaxContenu);
		printf("        theme : textDisabled %.1f, text %.1f\n", (double)lumGrise,
			   (double)lumNormale);
		Check(lumNormale > lumGrise, "   le theme distingue bien les deux couleurs");
		Check(a.lumMaxContenu < b.lumMaxContenu,
			  "   l'invite est peinte PLUS SOMBRE que la saisie");
		Check(Abs(a.lumMaxContenu - lumGrise) < Abs(a.lumMaxContenu - lumNormale),
			  "   et sa couleur est celle du texte GRISE, pas celle d'une saisie");
		Check(Abs(b.lumMaxContenu - lumNormale) < Abs(b.lumMaxContenu - lumGrise),
			  "   NEGATIF : la vraie saisie, elle, porte la couleur du texte normal");
		// Le champ est declare VIDE : ce qu'on voit dedans est une invite.
		bool videVu = false, pleinVu = false;
		for (uint32 i = 0; i < (uint32)a.rap.items.Size(); ++i)
			if (a.rap.items[i].role.Compare(NkString("TextField")) == 0)
				videVu = a.rap.items[i].champVide;
		for (uint32 i = 0; i < (uint32)b.rap.items.Size(); ++i)
			if (b.rap.items[i].role.Compare(NkString("TextField")) == 0)
				pleinVu = !b.rap.items[i].champVide;
		Check(videVu, "   le champ a invite est declare VIDE");
		Check(pleinVu, "   NEGATIF : le champ a `value` est declare NON vide");
	}

	printf("\n-- (i3) le `title` du Panel est peint\n");
	{
		// Attendu : le titre ajoute des pixels. Negatif : `title` vide -> retour
		// au compte d'avant, AU BIT.
		const char *avec =
			"nkgui 0.3\nwidgets {\n  Panel \"p\" { title = \"Reglages\" }\n}\n";
		const char *sans =
			"nkgui 0.3\nwidgets {\n  Panel \"p\" { title = \"\" }\n}\n";
		uint32 n1 = 0, n2 = 0;
		while (avec[n1]) ++n1;
		while (sans[n2]) ++n2;
		const Montage a = MonterTexte(avec, n1, 400, 200);
		const Montage b = MonterTexte(sans, n2, 400, 200);
		Check(a.lu && b.lu, "   les deux documents se lisent");
		printf("        contenu : avec titre %u px ;  sans titre %u px\n", a.contenu,
			   b.contenu);
		Check(a.contenu > b.contenu, "   le titre AJOUTE des pixels de texte");
		Check(a.empreinte != b.empreinte, "   et l'image change");
		// NEGATIF au bit : un `title` vide rend EXACTEMENT l'image d'un Panel sans
		// `title` du tout. Un monteur qui reserverait la place du titre meme vide
		// rougirait ici.
		const char *aucun = "nkgui 0.3\nwidgets {\n  Panel \"p\" { }\n}\n";
		uint32 n3 = 0;
		while (aucun[n3]) ++n3;
		const Montage c = MonterTexte(aucun, n3, 400, 200);
		CheckEq(b.empreinte, c.empreinte,
				"   NEGATIF : `title` vide == pas de `title`, AU BIT");
	}

	printf("\n-- (m5) LA ZONE HOTE : l'application remplit, et ce que personne ne remplit SE VOIT\n");
	{
		// ⚠️ LA FRONTIERE DU FORMAT, ET ELLE SE MESURE. Un `Host` est un rectangle que
		//    l'APPLICATION peint. Trois choses doivent etre vraies a la fois :
		//      1. ce que l'hote peint est LA, a la place que le document lui a donnee ;
		//      2. ce que personne ne remplit NE PEINT PAS de faux contenu -- zero pixel
		//         de la signature -- et NE DISPARAIT PAS : un marqueur, et un compteur ;
		//      3. la zone se compte dans les deux cas.
		Joindre(dossier, sizeof(dossier), racine, "/valides/");
		Joindre(chemin, sizeof(chemin), dossier, "12_zone_hote.nkgui");

		// ── (a) L'HOTE REMPLIT « viseur3d », personne ne remplit « apercu_materiau »
		RemplisseurHote hote;
		hote.cible = "viseur3d";
		g_hooks = &hote;
		g_couleurCible = kSignatureHote;
		const Montage r = MonterFichier(chemin, 400, 600, "12_zone_hote_rempli");
		g_hooks = nullptr;
		g_couleurCible = 0u;
		Check(r.lu, "   le document se lit");
		CheckEq(r.rap.rolesInconnus, 0u, "   `Host` est du vocabulaire (0 role inconnu)");
		CheckEq(r.rap.hotes, 2u, "   DEUX zones hotes comptees");
		CheckEq(r.rap.hotesNonRemplis, 1u, "   UNE seule est restee non remplie");
		CheckEq(hote.remplis, 1u, "   l'hote n'a rempli QUE le nom qu'il connait");
		// Ce que l'hote a peint est a la place que le DOCUMENT lui a donnee, et sa
		// surface est celle du rectangle -- pas « des pixels quelque part ».
		const uint32 aire = (uint32)(hote.derniere.w * hote.derniere.h + 0.5f);
		printf("        zone rendue a l'hote : %.0fx%.0f a (%.0f, %.0f) -> %u px attendus ; %u peints\n",
			   (double)hote.derniere.w, (double)hote.derniere.h, (double)hote.derniere.x,
			   (double)hote.derniere.y, aire, r.poigneeN);
		Check(hote.derniere.w > 1.f && hote.derniere.h > 1.f, "   la zone rendue a une surface");
		Check(r.poigneeN > 0u, "   la signature de l'hote EST dans l'image");
		// Tolerance : le rasteriseur peut perdre une bordure de pixels sur les bords,
		// et le texte du Panel peut recouvrir une partie de l'aplat.
		Check(r.poigneeN <= aire + 4u, "   et elle ne DEBORDE pas du rectangle du document");
		Check(r.poigneeN * 10u >= aire * 9u, "   elle en couvre au moins les neuf dixiemes");

		// ── (b) LE NEGATIF : personne ne remplit rien
		g_couleurCible = kSignatureHote;
		const Montage v = MonterFichier(chemin, 400, 600, "12_zone_hote_vide");
		g_couleurCible = 0u;
		Check(v.lu, "   le meme document, sans aucun hote, se lit");
		CheckEq(v.rap.hotes, 2u, "   les DEUX zones se comptent quand meme");
		CheckEq(v.rap.hotesNonRemplis, 2u, "   et les DEUX sont declarees non remplies");
		CheckEq(v.poigneeN, 0u, "   ZERO pixel de la signature : aucun faux contenu");
		// Et elles ne disparaissent pas : le marqueur peint, donc l'image change.
		printf("        pixels peints : avec hote %u ; sans hote %u\n", r.pixels, v.pixels);
		Check(v.pixels > 0u, "   la zone vide PEINT quelque chose (son marqueur)");
		Check(v.empreinte != r.empreinte, "   et les deux images different");

		// ── (c) LE MARQUEUR SE MESURE DANS SA ZONE, PAS SUR L'IMAGE ENTIERE
		// Le temoin note le rectangle de la zone que personne ne remplit, puis on
		// recompte DANS ce rectangle. Le compteur d'image entiere, lui, rendait
		// 239 980 dans les deux cas : sature par l'aplat du Panel.
		TemoinHote temoin;
		temoin.cible = "apercu_materiau";
		g_hooks = &temoin;
		g_garderPixels = true;
		const Montage t = MonterFichier(chemin, 400, 600);
		g_hooks = nullptr;
		g_garderPixels = false;
		Check(temoin.vu, "   le temoin a bien vu passer la zone non remplie");
		const uint32 marque = ComptePixelsDansRect(t.px, 400, 600, temoin.zone);
		printf("        zone non remplie : %.0fx%.0f a (%.0f, %.0f) -> %u px de marqueur\n",
			   (double)temoin.zone.w, (double)temoin.zone.h, (double)temoin.zone.x,
			   (double)temoin.zone.y, marque);
		// ⚠️ CE CRITERE EST CELUI QUE LA MUTATION DOIT FAIRE ROUGIR.
		//    `NK_HOTE_MUTATION=sansmarqueur` retire le trace du marqueur : ce compte
		//    doit alors tomber a zero, et cette ligne doit ECHOUER. Une mutation qui
		//    survit dirait que le critere ne teste rien.
		Check(marque > 0u, "   le marqueur TRACE dans la zone (mutation : sansmarqueur)");
		Check(temoin.zone.w > 1.f && temoin.zone.h > 1.f, "   et la zone a une surface");
	}

	printf("\n-- (m6) LES TAILLES RELATIVES : le MEME document, DEUX fenetres, DEUX dispositions justes\n");
	{
		// ⚠️ C'EST LE MANQUE STRUCTUREL DE L'INVENTAIRE DU 17/09. `pos` et `size` sont en
		//    pixels absolus : un document qui decrirait NK3DModeler avec eux le figerait a
		//    UNE taille de fenetre, alors que sa disposition reelle est en fractions
		//    (0,16 et 0,29). Le critere est donc : le meme fichier, deux fenetres, et les
		//    largeurs suivent -- avec le PLANCHER qui mord dans la petite.
		Joindre(dossier, sizeof(dossier), racine, "/valides/");
		Joindre(chemin, sizeof(chemin), dossier, "13_tailles_relatives.nkgui");

		const Montage grand = MonterFichier(chemin, 1200, 800, "13_tailles_grand");
		const Montage petit = MonterFichier(chemin, 800, 600, "13_tailles_petit");
		Check(grand.lu && petit.lu, "   le document se lit dans les deux fenetres");
		CheckEq(grand.rap.rolesInconnus, 0u, "   aucun role hors vocabulaire");

		// Les rectangles REELLEMENT montes, lus dans le releve -- jamais recalcules ici.
		float32 gG = -1.f, dG = -1.f, gP = -1.f, dP = -1.f;
		for (uint32 i = 0; i < (uint32)grand.rap.items.Size(); ++i) {
			const NkGuiMonteItem &it = grand.rap.items[i];
			if (it.id.Compare("gauche") == 0) gG = it.rect.w;
			if (it.id.Compare("droite") == 0) dG = it.rect.w;
		}
		for (uint32 i = 0; i < (uint32)petit.rap.items.Size(); ++i) {
			const NkGuiMonteItem &it = petit.rap.items[i];
			if (it.id.Compare("gauche") == 0) gP = it.rect.w;
			if (it.id.Compare("droite") == 0) dP = it.rect.w;
		}
		printf("        1200x800 : gauche %.0f px (attendu 192)   droite %.0f px (attendu 348)\n",
			   (double)gG, (double)dG);
		printf("         800x600 : gauche %.0f px (attendu 180, le PLANCHER mord)   droite %.0f px (attendu 232)\n",
			   (double)gP, (double)dP);
		Check(gG > 191.f && gG < 193.f, "   1200 : la colonne gauche fait 16 % (192 px)");
		Check(dG > 347.f && dG < 349.f, "   1200 : la colonne droite fait 29 % (348 px)");
		Check(gP > 179.f && gP < 181.f, "   800 : le PLANCHER de 180 px mord (128 -> 180)");
		Check(dP > 231.f && dP < 233.f, "   800 : la colonne droite suit (232 px)");
		// Et les deux dispositions sont DIFFERENTES : un document fige rendrait la meme.
		Check(gG != gP || dG != dP, "   les deux fenetres donnent deux dispositions differentes");

		// ── LE NEGATIF DE L'ABSOLU : `pos`+`size` ne suit PAS la fenetre ──
		const char *abs =
			"nkgui 0.3\nwidgets {\n  Panel \"racine\" { placement = absolute\n"
			"    Panel \"fixe\" { pos = (10, 10), size = (200, 100) }\n  }\n}\n";
		uint32 na = 0;
		while (abs[na]) ++na;
		const Montage a1 = MonterTexte(abs, na, 1200, 800);
		const Montage a2 = MonterTexte(abs, na, 800, 600);
		float32 f1 = -1.f, f2 = -1.f;
		for (uint32 i = 0; i < (uint32)a1.rap.items.Size(); ++i)
			if (a1.rap.items[i].id.Compare("fixe") == 0) f1 = a1.rap.items[i].rect.w;
		for (uint32 i = 0; i < (uint32)a2.rap.items.Size(); ++i)
			if (a2.rap.items[i].id.Compare("fixe") == 0) f2 = a2.rap.items[i].rect.w;
		printf("        NEGATIF absolu : %.0f px dans les deux fenetres\n", (double)f1);
		Check(f1 > 199.f && f1 < 201.f, "   NEGATIF : un `size` absolu vaut 200 px");
		Check(f1 == f2, "   NEGATIF : et il vaut le MEME dans une autre fenetre");
	}
	printf("\n-- (m7) LA BANDE D'ONGLETS : montee, et c'est le SOULIGNEMENT de l'onglet actif qui le prouve\n");
	{
		// ⚠️ LE CRITERE EST UNE COULEUR QUE SEULE LA SELECTION PEINT. `TabBar` souligne
		//    l'onglet actif sur 3 px en `theme.accent` (NkGuiWidgets.cpp:2249). Compter
		//    « des pixels » ne dirait rien -- le Panel en peint des dizaines de milliers.
		//    Compter l'ACCENT dit qu'un onglet est monte ET selectionne.
		const uint32 kAccent = 0x60A5FAFFu; // theme.accent par defaut : (96, 165, 250, 255)
		Joindre(dossier, sizeof(dossier), racine, "/valides/");
		Joindre(chemin, sizeof(chemin), dossier, "14_onglets.nkgui");

		g_couleurCible = kAccent;
		const Montage t3 = MonterFichier(chemin, 400, 300, "14_onglets");
		g_couleurCible = 0u;
		Check(t3.lu, "   le document se lit");
		CheckEq(t3.rap.rolesInconnus, 0u, "   `TabBar` est du vocabulaire (0 role inconnu)");
		float32 largeur = -1.f;
		for (uint32 i = 0; i < (uint32)t3.rap.items.Size(); ++i)
			if (t3.rap.items[i].id.Compare("onglets") == 0)
				largeur = t3.rap.items[i].rect.w;
		printf("        bande montee : largeur %.0f px ; pixels d'accent %u ; texte %u px\n",
			   (double)largeur, t3.poigneeN, t3.contenu);
		Check(largeur > 0.f, "   la bande est MONTEE (un rectangle non vide au releve)");
		Check(t3.poigneeN > 100u, "   l'onglet actif est SOULIGNE (accent peint)");

		// ── (c) TROIS onglets et non un : moins de texte avec un seul
		const char *un = "nkgui 0.3\nwidgets {\n  Panel \"f\" {\n    TabBar \"onglets\" { tabs = [Scene] }\n  }\n}\n";
		uint32 nu = 0;
		while (un[nu]) ++nu;
		g_couleurCible = kAccent;
		const Montage t1 = MonterTexte(un, nu, 400, 300);
		g_couleurCible = 0u;
		printf("        un seul onglet : texte %u px ; accent %u px\n", t1.contenu, t1.poigneeN);
		Check(t1.contenu < t3.contenu, "   TROIS onglets peignent plus de texte qu'UN");
		Check(t1.empreinte != t3.empreinte, "   et les deux images different");
		Check(t1.poigneeN > 100u, "   un seul onglet reste souligne (il est actif)");

		// ── (d) NEGATIF : une liste VIDE ne souligne rien
		const char *vide = "nkgui 0.3\nwidgets {\n  Panel \"f\" {\n    TabBar \"onglets\" { tabs = [] }\n  }\n}\n";
		uint32 nv = 0;
		while (vide[nv]) ++nv;
		g_couleurCible = kAccent;
		const Montage t0 = MonterTexte(vide, nv, 400, 300);
		g_couleurCible = 0u;
		printf("        liste vide : accent %u px\n", t0.poigneeN);
		CheckEq(t0.poigneeN, 0u, "   NEGATIF : `tabs = []` ne souligne RIEN");
	}
	printf("\n-- (m8) L'INSPECTEUR A BLOCS PLIABLES : ce qui compte n'est pas le triangle, c'est ce qu'il CACHE\n");
	{
		// ⚠️ LE CRITERE EST L'ABSENCE D'UN ENFANT DANS LE RELEVE. Un bloc replie qui
		//    monterait quand meme ses enfants « en les cachant » serait un titre, pas un
		//    pliable -- et un compteur de pixels ne verrait pas la difference si le contenu
		//    tombait hors clip. On lit donc le RELEVE, qui nomme chaque bloc monte.
		Joindre(dossier, sizeof(dossier), racine, "/valides/");
		Joindre(chemin, sizeof(chemin), dossier, "15_inspecteur.nkgui");
		const Montage m = MonterFichier(chemin, 400, 300, "15_inspecteur");
		Check(m.lu, "   le document se lit");
		CheckEq(m.rap.rolesInconnus, 0u, "   `Expander` est du vocabulaire (0 role inconnu)");

		bool blocOuvert = false, blocFerme = false, enfantVisible = false, enfantCache = false;
		for (uint32 i = 0; i < (uint32)m.rap.items.Size(); ++i) {
			const NkGuiMonteItem &it = m.rap.items[i];
			if (it.id.Compare("transformation") == 0) blocOuvert = true;
			if (it.id.Compare("materiau") == 0) blocFerme = true;
			if (it.id.Compare("position_visible") == 0) enfantVisible = true;
			if (it.id.Compare("couleur_cachee") == 0) enfantCache = true;
		}
		printf("        releve : bloc ouvert %s ; bloc ferme %s ; enfant du ouvert %s ; enfant du ferme %s\n",
			   blocOuvert ? "oui" : "NON", blocFerme ? "oui" : "NON",
			   enfantVisible ? "oui" : "NON", enfantCache ? "OUI" : "non");
		Check(blocOuvert && blocFerme, "   les DEUX blocs sont montes");
		Check(enfantVisible, "   l'enfant du bloc OUVERT est monte");
		Check(!enfantCache, "   NEGATIF : l'enfant du bloc FERME n'est PAS monte (mutation : toujours)");

		// ── (d) L'IMAGE : deux blocs fermes peignent MOINS que un ouvert + un ferme
		const char *deuxFermes =
			"nkgui 0.3\nwidgets {\n  Panel \"inspecteur\" { title = \"Proprietes\"\n"
			"    Expander \"transformation\" { label = \"Transformation\", expanded = false\n"
			"      Text \"position_visible\" { text = \"Position X Y Z\" }\n    }\n"
			"    Expander \"materiau\" { label = \"Materiau\", expanded = false\n"
			"      Text \"couleur_cachee\" { text = \"Couleur de base\" }\n    }\n  }\n}\n";
		uint32 nd = 0;
		while (deuxFermes[nd]) ++nd;
		const Montage f = MonterTexte(deuxFermes, nd, 400, 300);
		printf("        contenu peint : un ouvert %u px ; deux fermes %u px\n", m.contenu, f.contenu);
		Check(f.contenu < m.contenu, "   deux blocs fermes peignent MOINS de contenu");
		Check(f.empreinte != m.empreinte, "   et les deux images different");
	}
	printf("\n-- (m9) LE SEPARATEUR : le critere n'est pas qu'il se dessine, c'est qu'il DEPLACE la frontiere\n");
	{
		// ⚠️ UN TRAIT GRIS DE QUATRE PIXELS SE DESSINE SANS RIEN SEPARER. On monte donc
		//    DEUX FOIS dans le MEME contexte et le MEME etat, avec un GESTE entre les deux,
		//    et on lit les rectangles des deux voisins dans le releve. Ils doivent bouger
		//    EN SENS CONTRAIRE : un seul qui bouge serait un redimensionneur, pas un
		//    separateur.
		// ⚠️ AUCUNE ENTREE MACHINE : la souris est posee dans NOTRE contexte, comme partout
		//    ailleurs dans ce depot.
		Joindre(dossier, sizeof(dossier), racine, "/valides/");
		Joindre(chemin, sizeof(chemin), dossier, "16_separateur.nkgui");
		Fichier f = Lire(chemin);
		Check(f.ok, "   le document se lit depuis le disque");
		if (f.ok) {
			NkArchive doc;
			NkGuiDiag err;
			const bool lu = NkGuiArchive::Read(f.data, f.taille, doc, err);
			Check(lu, "   il s'analyse");
			if (lu) {
				NkGuiContext ctx;
				ctx.viewW = 1000;
				ctx.viewH = 600;
				if (g_fontOk)
					ctx.font = &g_font;
				const NkRect region{0.f, 0.f, 1000.f, 600.f};
				NkGuiMonteEtat etat;
				NkGuiMonteur::Preparer(doc, etat);

				// ── PASSE 1 : AU REPOS, aucune souris
				NkGuiMonteRapport r1;
				ctx.BeginFrame(0.016f);
				ctx.BeginLayout(region);
				ctx.DL().Reset();
				NkGuiMonteur::Monter(ctx, doc, etat, r1);
				ctx.EndFrame();
				float32 g1 = -1.f, d1 = -1.f;
				for (uint32 i = 0; i < (uint32)r1.items.Size(); ++i) {
					if (r1.items[i].id.Compare("gauche") == 0) g1 = r1.items[i].rect.w;
					if (r1.items[i].id.Compare("droite") == 0) d1 = r1.items[i].rect.w;
				}
				// ⚠️ MON ATTENDU ETAIT FAUX, ET LE CODE AVAIT RAISON. J'avais predit 160 et 836 ;
				//    `SplitterRects` CENTRE la poignee sur le point de coupe : le trait de 4 px va
				//    de 158 a 162, donc le voisin gauche fait 158 et le droit 838. La coupe est bien
				//    a 0,16 x 1000 = 160 -- c'est la CONDITION que ma prediction supposait (poignee
				//    posee APRES la coupe) qui etait fausse, pas la valeur.
				printf("        AU REPOS  : gauche %.0f px (attendu 158)   droite %.0f px (attendu 838)\n",
					   (double)g1, (double)d1);
				Check(g1 > 157.f && g1 < 159.f, "   le defaut du DOCUMENT est applique (coupe a 160, poignee centree)");
				Check(d1 > 837.f && d1 < 839.f, "   et le voisin prend le reste (1000 - 158 - 4)");

				// ── PASSE 2 : LE GESTE. Souris pressee sur la poignee, puis tiree a x = 400.
				//    Deux images : la premiere prend la poignee, la seconde la deplace --
				//    c'est le contrat de `SplitterRatio`, qui lit un appui deja actif.
				// 🔴 TROIS IMAGES, ET PAS DEUX : `ItemHoverable` finit par `return hotIdPrev == id`,
				//    donc un widget n'est survolable qu'apres avoir ete le plus en avant a l'image
				//    PRECEDENTE. Un appui pose des la premiere image ne trouve personne de survole
				//    et ne prend jamais la main : ma premiere sonde faisait deux images, le geste
				//    n'avait aucun effet, et j'ai failli accuser le separateur.
				//    Ordre obligatoire : SURVOLER, puis APPUYER, puis DEPLACER.
				for (int32 img = 0; img < 3; ++img) {
					NkGuiMonteRapport rg;
					ctx.BeginFrame(0.016f);
					ctx.input.mousePos = (img < 2) ? NkVec2{160.f, 300.f} : NkVec2{400.f, 300.f};
					ctx.input.mouseDown[0] = (img >= 1);
					ctx.input.mouseClicked[0] = (img == 1);
					ctx.BeginLayout(region);
					ctx.DL().Reset();
					NkGuiMonteur::Monter(ctx, doc, etat, rg);
					ctx.EndFrame();
				}
				// 🔴 ON RELACHE LA OU L'ON EST, ET C'EST UNE CORRECTION DE MA SONDE.
				//    Ma premiere version relachait ET teleportait la souris hors ecran dans la
				//    MEME image : `SplitterRatio` rapporte la position a la zone, donc
				//    (-10000 - 0) / 1000 = -10, borne a `min` = 0,1 -- la colonne tombait a 98 px
				//    et j'ai failli conclure que le geste ne survivait pas. Un utilisateur
				//    relache ou son curseur est ; le teleporter etait une condition que le
				//    produit ne rencontre pas.
				// ⚠️ CONSTAT GARDE, NON CORRIGE : si la souris sort VRAIMENT de la fenetre
				//    pendant le glisser, le ratio saute au minimum. Condition de reouverture :
				//    le jour ou un glisser reel devra survivre a une sortie de fenetre.
				// ── PASSE 3 : on relache, et on relit la disposition
				NkGuiMonteRapport r2;
				ctx.BeginFrame(0.016f);
				ctx.input.mouseDown[0] = false;
				ctx.input.mouseClicked[0] = false;
				ctx.input.mousePos = NkVec2{400.f, 300.f}; // ON RELACHE SUR PLACE (voir ci-dessous)
				ctx.BeginLayout(region);
				ctx.DL().Reset();
				NkGuiMonteur::Monter(ctx, doc, etat, r2);
				ctx.EndFrame();
				float32 g2 = -1.f, d2 = -1.f;
				for (uint32 i = 0; i < (uint32)r2.items.Size(); ++i) {
					if (r2.items[i].id.Compare("gauche") == 0) g2 = r2.items[i].rect.w;
					if (r2.items[i].id.Compare("droite") == 0) d2 = r2.items[i].rect.w;
				}
				printf("        APRES LE GESTE : gauche %.0f px   droite %.0f px   (somme %.0f, etait %.0f)\n",
					   (double)g2, (double)d2, (double)(g2 + d2), (double)(g1 + d1));
				Check(g2 > g1, "   la frontiere a BOUGE : le voisin gauche a grandi");
				Check(d2 < d1, "   ET le voisin droit a diminue -- les DEUX se recalculent");
				const float32 somme1 = g1 + d1, somme2 = g2 + d2;
				Check(somme2 > somme1 - 1.f && somme2 < somme1 + 1.f,
					  "   leur somme est conservee (le trait garde son epaisseur)");

				// ── (c) LE DOCUMENT NE REECRIT PAS LE GESTE : un montage de plus, sans
				//    souris, garde la valeur tiree. C'est `initialise` qui le garantit.
				NkGuiMonteRapport r3;
				ctx.BeginFrame(0.016f);
				ctx.BeginLayout(region);
				ctx.DL().Reset();
				NkGuiMonteur::Monter(ctx, doc, etat, r3);
				ctx.EndFrame();
				float32 g3 = -1.f;
				for (uint32 i = 0; i < (uint32)r3.items.Size(); ++i)
					if (r3.items[i].id.Compare("gauche") == 0) g3 = r3.items[i].rect.w;
				printf("        MONTAGE SUIVANT, sans geste : gauche %.0f px\n", (double)g3);
				Check(g3 > g2 - 1.f && g3 < g2 + 1.f,
					  "   le geste SURVIT au montage suivant (le document ne le reecrit pas)");
			}
			Liberer(f);
		}

		// ── (e) NEGATIF : sans aucun geste, deux montages rendent la MEME disposition
		const Montage s1 = MonterFichier(chemin, 1000, 600, "16_separateur");
		const Montage s2 = MonterFichier(chemin, 1000, 600);
		CheckEq(s1.empreinte, s2.empreinte, "   NEGATIF : sans geste, deux montages sont identiques AU BIT");
	}
	printf("\n-- (m10) UNE SENTINELLE N'EST PAS UNE POSITION : un geste ne suit pas une souris « nulle part »\n");
	{
		// ⚠️ LE DEFAUT, TROUVE EN ME TROMPANT LE 17/09 : un separateur tire a 398 px puis
		//    relache pendant que la souris est repoussee hors ecran voyait sa colonne tomber
		//    a 98 px -- la valeur minimale. La coquille repousse la souris a (-100000,
		//    -100000) quand elle masque l'entree des panneaux ; quinze endroits de NKGui
		//    derivent une valeur de la position ABSOLUE, aucun n'avait de garde.
		// ⚠️ LE ZERO D'ABORD : la regle ne doit RIEN changer quand la souris reste dedans.
		Joindre(dossier, sizeof(dossier), racine, "/valides/");
		Joindre(chemin, sizeof(chemin), dossier, "16_separateur.nkgui");
		Fichier f2 = Lire(chemin);
		Check(f2.ok, "   le document se lit");
		if (f2.ok) {
			NkArchive doc;
			NkGuiDiag err;
			if (NkGuiArchive::Read(f2.data, f2.taille, doc, err)) {
				// Un glisser identique a celui de (m9), mais relache AVEC LA SENTINELLE.
				NkGuiContext ctx;
				ctx.viewW = 1000;
				ctx.viewH = 600;
				if (g_fontOk)
					ctx.font = &g_font;
				const NkRect region{0.f, 0.f, 1000.f, 600.f};
				NkGuiMonteEtat etat;
				NkGuiMonteur::Preparer(doc, etat);
				NkGuiMonteRapport rz;
				// survol, appui, deplacement -- l'ordre qu'impose `hotIdPrev`
				for (int32 img = 0; img < 3; ++img) {
					ctx.BeginFrame(0.016f);
					ctx.input.mousePos = (img < 2) ? NkVec2{160.f, 300.f} : NkVec2{400.f, 300.f};
					ctx.input.mouseDown[0] = (img >= 1);
					ctx.input.mouseClicked[0] = (img == 1);
					ctx.BeginLayout(region);
					ctx.DL().Reset();
					NkGuiMonteRapport tmp;
					NkGuiMonteur::Monter(ctx, doc, etat, tmp);
					ctx.EndFrame();
				}
				// LA SENTINELLE : la souris part « nulle part » alors que le geste est vif.
				ctx.BeginFrame(0.016f);
				ctx.input.mousePos = NkVec2{-100000.f, -100000.f};
				ctx.input.mouseDown[0] = true;
				ctx.BeginLayout(region);
				ctx.DL().Reset();
				NkGuiMonteur::Monter(ctx, doc, etat, rz);
				ctx.EndFrame();
				// puis on relache, souris toujours nulle part, et on relit
				NkGuiMonteRapport rf;
				ctx.BeginFrame(0.016f);
				ctx.input.mouseDown[0] = false;
				ctx.BeginLayout(region);
				ctx.DL().Reset();
				NkGuiMonteur::Monter(ctx, doc, etat, rf);
				ctx.EndFrame();
				float32 gf = -1.f;
				for (uint32 i = 0; i < (uint32)rf.items.Size(); ++i)
					if (rf.items[i].id.Compare("gauche") == 0)
						gf = rf.items[i].rect.w;
				printf("        apres la SENTINELLE : gauche %.0f px (attendu ~398, et non 98)\n", (double)gf);
				Check(gf > 390.f, "   la colonne NE S'EFFONDRE PAS (mutation : NK_SOURIS_MUTATION=libre)");
			}
			Liberer(f2);
		}
	}
	printf("\n-- (m11) LA HIERARCHIE : une liste ECRITE est un widget, une liste LIEE est une donnee\n");
	{
		// ⚠️ LA FRONTIERE, ET ELLE EST LA MEME QUE CELLE DE LA ZONE HOTE. Le format sait
		//    ECRIRE une liste (`items`, ou des enfants `Item`/`TreeItem`) : c'est statique,
		//    donc un widget. Il sait aussi NOMMER une source (`bind`), et la rien ne peut la
		//    fournir -- l'etat du montage ne porte qu'un booleen, un flottant et un texte.
		//    Une liste liee que personne ne remplit doit donc SE SIGNALER, comme la zone
		//    hachuree : sinon un arbre vide se fait prendre pour un arbre sans elements.
		Joindre(dossier, sizeof(dossier), racine, "/valides/");
		Joindre(chemin, sizeof(chemin), dossier, "17_hierarchie.nkgui");
		const Montage h = MonterFichier(chemin, 400, 500, "17_hierarchie.png");
		Check(h.lu, "   le document se lit");
		CheckEq(h.rap.rolesInconnus, 0u, "   `ListBox`, `TreeItem` et `Item` sont du vocabulaire");

		bool listeEcrite = false, listeLiee = false, arbre = false, cube = false, lumiere = false;
		NkRect rectLiee{};
		for (uint32 i = 0; i < (uint32)h.rap.items.Size(); ++i) {
			const NkGuiMonteItem &it = h.rap.items[i];
			if (it.id.Compare("ecrite") == 0) listeEcrite = true;
			if (it.id.Compare("liee") == 0) { listeLiee = true; rectLiee = it.rect; }
			if (it.id.Compare("racine") == 0) arbre = true;
			if (it.id.Compare("cube") == 0) cube = true;
			if (it.id.Compare("lumiere") == 0) lumiere = true;
		}
		printf("        releve : liste ecrite %s ; arbre %s ; cube %s ; lumiere %s ; liste liee %s\n",
			   listeEcrite ? "oui" : "NON", arbre ? "oui" : "NON", cube ? "oui" : "NON",
			   lumiere ? "oui" : "NON", listeLiee ? "oui" : "NON");
		Check(listeEcrite && listeLiee, "   les DEUX listes sont montees");
		Check(arbre && cube && lumiere, "   la liste ECRITE monte son arbre et ses deux elements");
		printf("        listes liees et jamais remplies : %u (attendu 1)\n", h.rap.listesNonRemplies);
		CheckEq(h.rap.listesNonRemplies, 1u, "   UNE SEULE liste est declaree non remplie");

		// ── (c) LE MARQUEUR EST VISIBLE, et on le mesure DANS son rectangle
		g_garderPixels = true;
		const Montage h2 = MonterFichier(chemin, 400, 500);
		g_garderPixels = false;
		const uint32 marqueL = ComptePixelsDansRect(h2.px, 400, 500, rectLiee);
		printf("        zone de la liste liee : %.0fx%.0f a (%.0f,%.0f) -> %u px de marqueur\n",
			   (double)rectLiee.w, (double)rectLiee.h, (double)rectLiee.x, (double)rectLiee.y, marqueL);
		// 🔴 LE SEUIL EST DERIVE DU TRACE, ET PAS « PLUS DE ZERO ». Premiere version :
		//    `marque > 0`. La MUTATION A SURVECU -- elle rendait 12 pixels (le cadre de la
		//    `ListBox` elle-meme) au lieu de 4 249, et le critere passait quand meme. Un
		//    critere qu'une mutation traverse ne teste rien.
		//    Les hachures posent un segment tous les 12 px sur (largeur + hauteur) : meme a
		//    dix pixels de long en moyenne, cela fait (w + h) / 12 * 10. Le seuil vient donc
		//    de la geometrie du trace, pas d'un nombre choisi.
		const uint32 seuilHachures = (uint32)((rectLiee.w + rectLiee.h) / 12.f * 10.f);
		printf("        seuil derive des hachures : %u px\n", seuilHachures);
		Check(marqueL > seuilHachures, "   le marqueur de la liste liee est VISIBLE (mutation : muette)");

		// ── (d) NEGATIF : une liste ECRITE ne compte pas comme non remplie
		const char *ecrite =
			"nkgui 0.3\nwidgets {\n  Panel \"f\" {\n    ListBox \"l\" { bind = ui.scene\n"
			"      Item \"a\" { label = \"A\" }\n    }\n  }\n}\n";
		uint32 ne = 0;
		while (ecrite[ne]) ++ne;
		const Montage he = MonterTexte(ecrite, ne, 400, 500);
		printf("        liste LIEE mais REMPLIE : %u non remplie(s)\n", he.rap.listesNonRemplies);
		CheckEq(he.rap.listesNonRemplies, 0u,
				"   NEGATIF : une liste liee ET remplie n'est PAS signalee");
	}
	printf("\n-- (m12) L'ANCRAGE : le document dit QUELLES ZONES, et une zone que personne ne fournit SE SIGNALE\n");
	{
		// ⚠️ LE TROISIEME PIRE RESULTAT DU FORMAT est ici : un document dont les noms de
		//    zone ne correspondent a rien de ce que l'application peut fournir. Il doit se
		//    signaler comme la zone hote et la liste liee -- visible, NOMME, jamais muet.
		// L'hote de ce banc fournit deux panneaux, « hierarchie » et « apercu », et pas le
		// troisieme : c'est exactement le cas a mesurer.
		struct HoteAncrage : nkentseu::nkgui::NkGuiMonteHooks {
				uint32 servies = 0;
				NkRect derniere{};
				// L'INVENTAIRE DE L'HOTE : il possede TROIS panneaux, dont « console »,
				// que le document ne nomme nulle part. C'est le cas INVERSE.
				uint32 nbDemandes = 0;
				bool consoleDemandee = false;
				bool ZoneAncree(const char *nom, const NkRect &zone) noexcept override {
					++nbDemandes;
					if (NomEgal(nom, "console"))
						consoleDemandee = true;
					if (NomEgal(nom, "hierarchie") || NomEgal(nom, "apercu")) {
						++servies;
						derniere = zone;
						return true;
					}
					return false; // « zone_absente » : cet hote ne la fournit pas
				}
		};
		Joindre(dossier, sizeof(dossier), racine, "/valides/");
		Joindre(chemin, sizeof(chemin), dossier, "18_ancrage.nkgui");
		HoteAncrage hote;
		g_hooks = &hote;
		g_garderPixels = true;
		const Montage d1 = MonterFichier(chemin, 1000, 600, "18_ancrage.png");
		g_hooks = nullptr;
		g_garderPixels = false;
		Check(d1.lu, "   le document se lit");
		CheckEq(d1.rap.rolesInconnus, 0u, "   `DockSpace` est du vocabulaire (0 role inconnu)");
		CheckEq(d1.rap.zones, 3u, "   TROIS zones declarees par le document");
		CheckEq(hote.servies, 2u, "   l'hote en sert DEUX (celles dont il a le panneau)");
		CheckEq(d1.rap.zonesSansPanneau, 1u, "   UNE zone ne correspond a AUCUN panneau fourni");

		// Les proportions viennent du DOCUMENT : 0,16 x 1000 = 160 pour la premiere.
		float32 largHier = -1.f, rectAbsenteW = -1.f;
		NkRect rectAbsente{};
		for (uint32 i = 0; i < (uint32)d1.rap.items.Size(); ++i) {
			const NkGuiMonteItem &it = d1.rap.items[i];
			if (it.id.Compare("hierarchie") == 0) largHier = it.rect.w;
			if (it.id.Compare("zone_absente") == 0) { rectAbsente = it.rect; rectAbsenteW = it.rect.w; }
		}
		printf("        zone « hierarchie » : %.0f px (attendu 180 : le plancher mord)   zone absente : %.0f px (attendu 290)\n",
			   (double)largHier, (double)rectAbsenteW);
		// ⚠️ 180 ET NON 160, ET C'EST MON ATTENDU QUI AVAIT TORT -- pour la troisieme fois
		//    cette semaine, j'avais ecrit la VALEUR sans la CONDITION qu'elle suppose. Le
		//    document declare `sizeRel = (0.16, 0)` ET `minSize = (180, 0)` : 0,16 x 1000
		//    fait 160, le plancher de 180 MORD, et 180 est le bon resultat. Le zero de ce
		//    critere est ailleurs : une zone SANS plancher, qui doit rendre sa fraction nue.
		Check(largHier > 179.f && largHier < 181.f, "   la proportion vient du DOCUMENT (16 %, plancher 180 px)");
		// LE ZERO DE LA FRACTION : la troisieme zone declare 0,29 SANS plancher -> 290 px nus.
		Check(rectAbsenteW > 289.f && rectAbsenteW < 291.f, "   une zone SANS plancher rend sa fraction nue (29 %)");

		// ⚠️ CE CRITERE EST NE DE L'IMAGE, PAS DES NOMBRES : les onze criteres precedents
		//    etaient verts et la capture montrait une BANDE NOIRE de ~197 px a droite, qui
		//    n'appartenait a aucune zone. Cause : une zone sans `sizeRel` prenait « une part
		//    egale » (1000/3 = 333) au lieu du RESTE. Un dock qui laisse un trou n'est pas
		//    un dock. La regle est donc : les fractions declarees d'abord, le RESTE partage
		//    entre celles qui n'en declarent pas.
		float32 sommeZones = 0.f, droiteMax = 0.f;
		for (uint32 i = 0; i < (uint32)d1.rap.items.Size(); ++i) {
			const NkGuiMonteItem &it = d1.rap.items[i];
			if (it.profondeur != 1u)
				continue;
			sommeZones += it.rect.w;
			if (it.rect.x + it.rect.w > droiteMax)
				droiteMax = it.rect.x + it.rect.w;
		}
		printf("        largeur couverte par les zones : %.0f px sur 1000 ; bord droit : %.0f\n",
			   (double)sommeZones, (double)droiteMax);
		Check(droiteMax > 999.f, "   LES ZONES COUVRENT TOUT LE DOCK -- aucune bande orpheline");

		// LE MARQUEUR de la zone non fournie, mesure DANS son rectangle, avec le seuil
		// derive des hachures -- pas « plus de zero », qui laisserait passer un cadre.
		const uint32 marqueZ = ComptePixelsDansRect(d1.px, 1000, 600, rectAbsente);
		const uint32 seuilZ = (uint32)((rectAbsente.w + rectAbsente.h) / 12.f * 10.f);
		printf("        marqueur de la zone absente : %u px (seuil derive %u)\n", marqueZ, seuilZ);
		Check(marqueZ > seuilZ, "   la zone non fournie SE SIGNALE (mutation : NK_DOCK_MUTATION=muet)");

		// ── LE CAS INVERSE, celui que le coordinateur demande par ecrit : UN PANNEAU QUE
		//    L'APPLICATION FOURNIT ET QUE LE DOCUMENT NE MENTIONNE PAS.
		//    MON ATTENDU, ecrit avant de lire le compteur : le monteur ne parcourt que les
		//    zones DU DOCUMENT ; il ne connait pas l'inventaire de l'hote et ne peut donc
		//    ni l'ouvrir, ni le fermer, ni le deplacer. Le panneau doit rester EXACTEMENT
		//    ou l'application l'avait mis -- ni flottant, ni ferme : INTOUCHE. La preuve
		//    est que le crochet n'est JAMAIS appele pour lui : exactement 3 demandes pour
		//    3 zones declarees, et « console » n'en fait pas partie.
		//    ⚠️ Ce n'est PAS la reponse de l'autre format : la coquille, elle, FERME. Voir
		//    `LoadUiState` (NkEditorShell.cpp) -- `if (sawPanel) ... SetOpen(false)` ferme
		//    TOUS les panneaux avant de rouvrir les seuls qui sont nommes. Les deux formats
		//    ont donc des politiques OPPOSEES sur le panneau non mentionne, et c'est mesure
		//    des deux cotes (ici, et section 6 de `--recette-identite` dans NKUIDesign).
		printf("        crochets demandes : %u (zones du document : %u) ; « console » demandee : %s\n",
			   hote.nbDemandes, d1.rap.zones, hote.consoleDemandee ? "OUI" : "non");
		CheckEq(hote.nbDemandes, d1.rap.zones, "   le monteur ne demande QUE les zones du document");
		Check(!hote.consoleDemandee,
			  "   LE CAS INVERSE : un panneau non mentionne n'est JAMAIS demande -- il reste INTOUCHE");

		// ── NEGATIF : un hote qui sert TOUT ne doit rien signaler
		struct HoteComplet : nkentseu::nkgui::NkGuiMonteHooks {
				bool ZoneAncree(const char *, const NkRect &) noexcept override { return true; }
		};
		HoteComplet complet;
		g_hooks = &complet;
		const Montage d2 = MonterFichier(chemin, 1000, 600);
		g_hooks = nullptr;
		printf("        hote qui sert TOUT : %u zone(s) sans panneau\n", d2.rap.zonesSansPanneau);
		CheckEq(d2.rap.zonesSansPanneau, 0u, "   NEGATIF : un hote complet ne signale RIEN");
		Check(d2.empreinte != d1.empreinte, "   et les deux images different");
	}
	printf("\n=== %d / %d ===\n", g_pass, g_pass + g_fail);
	if (g_fail > 0)
		printf("    %d ECHEC(S)\n", g_fail);
	return g_fail > 0 ? 1 : 0;
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
