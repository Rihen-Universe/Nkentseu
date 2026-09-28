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
#include "NKGui/Core/NkGuiIcons.h"
#include "NKGui/Doc/NkGuiInteraction.h"
#include "NKGui/Doc/NkGuiMonteur.h"
#include "NKGui/Doc/NkGuiRappels.h"
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

/// Pose (ou retire) une variable d'environnement, pour armer une MUTATION dans le
/// processus courant. `valeur == nullptr` la retire.
///
/// ⚠️ ELLE EXISTE PARCE QU'UN NEGATIF DOIT TENIR DANS LA MEME EXECUTION QUE LE
///    POSITIF. Les mutations plus anciennes de ce depot sont lues par un
///    `static const bool` initialise UNE FOIS par processus : pour les comparer il
///    fallait relancer le banc, et comparer deux images ecrites par deux
///    executions differentes. Deux processus, c'est deux occasions de comparer
///    autre chose que ce qu'on croit. `NkGuiTailleFluxIgnoree` relit donc
///    l'environnement a chaque montage, et ce tampon-ci le change entre deux
///    montages : les deux images sortent du meme banc, a la suite.
static void NkDefinirVariable(const char *nom, const char *valeur) {
#if defined(_WIN32)
	(void)_putenv_s(nom, valeur ? valeur : "");
#else
	if (valeur)
		(void)setenv(nom, valeur, 1);
	else
		(void)unsetenv(nom);
#endif
}

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

/// ⚠️ ELLE POSE LE SEPARATEUR QUAND IL MANQUE, et ca n'a pas toujours ete le
///    cas. Avant le 27/09 c'etait une concatenation nue : juste pour `racine`
///    (qui finit par `/`), collante pour `--png=dossier`. Les douze captures
///    d'un lot sont ainsi parties dans `Build/` sous le nom
///    `Preuves_UI_27-09b1_a_hover.png`. La trace « image ecrite : ... » disait
///    VRAI -- c'est l'attente de l'appelant qui etait fausse, et une interface
///    qui n'accepte qu'une seule graphie du meme chemin la fabrique.
static void Joindre(char *out, uint32 taille, const char *a, const char *b) {
	uint32 i = 0;
	for (; a[i] && i + 1u < taille; ++i)
		out[i] = a[i];
	const bool aFinitParSep = (i > 0u && (out[i - 1u] == '/' || out[i - 1u] == '\\'));
	const bool bCommenceParSep = (b[0] == '/' || b[0] == '\\');
	if (i > 0u && !aFinitParSep && !bCommenceParSep && i + 1u < taille)
		out[i++] = '/';
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
/// Les pixels qui ne sont PAS le fond efface. C'est la mesure la plus large
/// possible : « quelque chose de plus est arrive a l'image ».
///
/// ⚠️ ELLE EXISTE POUR CE QUI N'A PAS DE COULEUR CONNUE D'AVANCE. Une infobulle
///    prend la couleur du theme, une icone celle du texte : les chercher par
///    egalite obligerait a recopier le theme dans le banc, et ce chiffre se
///    perimerait au premier changement de theme.
static uint32 ComptePixelsPeints(const NkGuiDrawListRaster &r, uint32 fond) {
	uint32 n = 0;
	for (int32 y = 0; y < r.Hauteur(); ++y)
		for (int32 x = 0; x < r.Largeur(); ++x)
			if (r.Pixel(x, y) != fond)
				++n;
	return n;
}

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

/// Les pixels d'un rectangle PLUS PROCHES d'une couleur que d'une autre.
///
/// ⚠️ IL EXISTE PARCE QU'UNE ENCRE NE SE COMPTE PAS A L'EGALITE. Un libelle de
///    15 px est ANTICRENELE : ses pixels sont des melanges de l'encre et du
///    fond, et la couleur demandee n'apparait presque jamais telle quelle.
///    Mesure du 26/09 sur `panneau_outils` : l'encre #10222B posee sur l'orange
///    a donne #15252b, #1b282b, #1e2a2b... et **zero** pixel exactement egal.
///    Exiger l'egalite aurait rendu un critere toujours rouge sur un rendu
///    parfaitement juste.
///
/// ⚠️ ET CE N'EST PAS « PROCHE DE LA CIBLE », C'EST « PLUS PROCHE D'ELLE QUE DE
///    SA RIVALE ». Un seuil absolu demanderait de choisir une distance, donc un
///    nombre que personne n'a ecrit. La question posee ici est celle qui compte
///    vraiment : des deux encres possibles -- celle du document et celle du
///    theme -- laquelle ce pixel a-t-il servie ?
///
/// 🔴 ET IL COMPTE LE LIBELLE SEUL, PAS LE BOUTON. Premiere version, le 26/09 :
///    elle comparait TOUS les pixels du rectangle. L'aplat orange (#FFB055) est
///    plus proche d'une encre brune (#7A2E00) que d'une encre bleu-noir
///    (#10222B) -- il remplit le bouton, il ecrase le vote, et le critere
///    repondait « l'encre du survol est la » quelle que soit l'encre du libelle.
///    La mutation qui coupait l'encre d'etat est restee VERTE. *Un critere qui
///    mesure l'aplat ne dit rien du texte pose dessus.*
///    L'aplat et le bord sont donc EXCLUS, exactement comme dans
///    `ComptePixelsAutres` -- ce qui reste est le libelle.
static uint32 ComptePlusProchesDe(const NkGuiDrawListRaster &r, const NkRect &zone, NkColor cible,
								  NkColor rivale, uint32 aplat, uint32 bord) {
	uint32 n = 0;
	const int32 x0 = (int32)zone.x, y0 = (int32)zone.y;
	const int32 x1 = (int32)(zone.x + zone.w), y1 = (int32)(zone.y + zone.h);
	for (int32 y = y0; y < y1 && y < r.Hauteur(); ++y)
		for (int32 x = x0; x < x1 && x < r.Largeur(); ++x) {
			if (x < 0 || y < 0)
				continue;
			const uint32 p = r.Pixel(x, y);
			if (p == aplat || p == bord)
				continue;
			const int32 pr = (int32)((p >> 24) & 0xFFu), pv = (int32)((p >> 16) & 0xFFu),
						pb = (int32)((p >> 8) & 0xFFu);
			const int32 dc = (pr - (int32)cible.r) * (pr - (int32)cible.r)
							 + (pv - (int32)cible.g) * (pv - (int32)cible.g)
							 + (pb - (int32)cible.b) * (pb - (int32)cible.b);
			const int32 dv = (pr - (int32)rivale.r) * (pr - (int32)rivale.r)
							 + (pv - (int32)rivale.g) * (pv - (int32)rivale.g)
							 + (pb - (int32)rivale.b) * (pb - (int32)rivale.b);
			// L'egalite ne compte pour personne : elle ne departage rien.
			if (dc < dv)
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
		///
		/// ⚠️ `hooks` REMPLACE `&exe`, IL NE S'AJOUTE PAS. `NkGuiMonteur::Monter` ne
		///    prend qu'UN objet de crochets, et `NkGuiExecution` en est un : c'est lui
		///    que ce banc passe d'ordinaire. Un cas qui fournit son propre hote (pour
		///    `MenuContextuelOuvre`, pour `ZoneAncree`) perd donc les crochets de
		///    l'execution — ce qui est sans effet pour un document SANS `behavior` ni
		///    `bind`, et faux pour tout autre. Le cas qui s'en sert doit le dire.
		void Image(NkGuiMonteHooks *hooks = nullptr) {
			rap = NkGuiMonteRapport();
			exe.ReinitialiserCompteurs();
			const NkRect region{0.f, 0.f, (float32)ctx.viewW, (float32)ctx.viewH};
			ctx.BeginFrame(0.016f);
			ctx.BeginLayout(region);
			ctx.DL().Reset();
			NkGuiMonteur::Monter(ctx, doc, etat, rap, hooks ? hooks : (NkGuiMonteHooks *)&exe);
			// Les comportements APRES le montage : `n1.value` doit etre la valeur
			// que le widget vient d'avoir, pas celle de l'image d'avant.
			exe.ExecuterComportements(doc);
			ctx.EndFrame();
			ras.Effacer(kFond);
			if (g_fontOk && g_font.pixels)
				ras.PoserTexture(g_font.TexId(), g_font.pixels, g_font.atlasW, g_font.atlasH, 1);
			// 🔴 LES DEUX COUCHES, ET LA SECONDE MANQUAIT (27/09). Ce banc ne
			//    rasterisait que `ctx.dl` : tout ce qui vit dans la SURIMPRESSION —
			//    infobulles, menus deroules, popups — lui etait INVISIBLE. Un
			//    critere sur une infobulle ne pouvait donc rien prouver, quoi qu'il
			//    compte. La sonde de NkAnimaEditor avait deja paye cette lecon le
			//    26/09 : « n'en rasteriser qu'une donnait 27 000 pixels — l'aire
			//    exacte de la barre, et rien du menu qu'on cherchait a prouver ».
			(void)ras.Rasteriser(ctx.dl);
			(void)ras.Rasteriser(ctx.dlOverlay);
			++images;
		}

		/// Une image qui ne monte QU'UNE racine nommee du document.
		///
		/// ⚠️ ELLE RECOPIE `Image()` AU LIEU DE L'APPELER, ET C'EST LE SEUL ENDROIT
		///    OU CE BANC LE FAIT. La difference tient a UNE ligne — quel `Monter` on
		///    appelle — et tout le reste (les compteurs remis a zero, la region, les
		///    deux couches rasterisees) doit rester identique, sinon la comparaison
		///    entre « tout monte » et « une racine montee » mesurerait autre chose que
		///    la racine. Un parametre de plus sur `Image()` aurait ete plus propre ;
		///    il aurait aussi donne a tous les appels existants une branche qu'ils ne
		///    prennent jamais. **Condition de retrait :** si un troisieme mode
		///    apparait, les trois fusionnent.
		void ImageRacine(const char *racine, bool &trouveeOut) {
			rap = NkGuiMonteRapport();
			exe.ReinitialiserCompteurs();
			const NkRect region{0.f, 0.f, (float32)ctx.viewW, (float32)ctx.viewH};
			ctx.BeginFrame(0.016f);
			ctx.BeginLayout(region);
			ctx.DL().Reset();
			trouveeOut = NkGuiMonteur::Monter(ctx, doc, racine, etat, rap, &exe);
			exe.ExecuterComportements(doc);
			ctx.EndFrame();
			ras.Effacer(kFond);
			if (g_fontOk && g_font.pixels)
				ras.PoserTexture(g_font.TexId(), g_font.pixels, g_font.atlasW, g_font.atlasH, 1);
			(void)ras.Rasteriser(ctx.dl);
			(void)ras.Rasteriser(ctx.dlOverlay);
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

		/// Le meme, CONTENEURS COMPRIS. `Rect` les ecarte -- c'est ce qu'il faut
		/// quand on vise un widget de saisie, et c'est faux quand on vise un
		/// accordeon : sa barre cliquable appartient au conteneur lui-meme.
		bool RectTout(const char *id, NkRect &out) const {
			for (uint32 i = 0; i < (uint32)rap.items.Size(); ++i)
				if (rap.items[i].id.Compare(NkString(id)) == 0) {
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
// =============================================================================
//  (b19) LE MONTAGE PAR RACINE NOMMEE — dans SA fonction, et voici pourquoi
// =============================================================================
// 🔴 LE BANC A DEBORDE LA PILE EN AJOUTANT CE CAS : `0xC00000FD`, code de
//    sortie -1073741571, et AUCUNE ligne imprimee — pas meme la premiere. Ce
//    n'etait pas une recursion : `main()` portait dix-neuf cas, chacun avec ses
//    objets `Scene` en variables locales, et en Debug le compilateur ne reutilise
//    pas les emplacements des portees imbriquees. Le cadre de `main` depassait
//    simplement la pile.
//
// ⚠️ ET LE SYMPTOME ACCUSAIT LE MAUVAIS COUPABLE. Un binaire neuf qui ne dit RIEN
//    et rend 127 sous git-bash ressemble a une DLL manquante ; c'est PowerShell qui
//    a rendu le vrai code. *Un code de sortie traduit par un shell n'est pas le code
//    de sortie.*
//
//    D'ou cette fonction : un cas de banc qui grandit se sort de `main`, et le
//    prochain fera de meme. Aucun critere n'a change.
// =============================================================================
//  (b20) `Padding`, `Aspect`, `Overlay`, `Center` — quatre conteneurs de plus
// =============================================================================
// (b18) LA BARRE DE MENUS QUI CONNAIT SA LIMITE — dans son propre fichier.
//
// ⚠️ IL S'INCLUT ICI, ET PAS PARMI LES `#include` DU DEBUT. Un cas lit `Scene`,
//    `Check`, `CheckEqU` et `CheckEqF`, declares plus haut : les remonter parmi
//    les includes le ferait referencer du code pas encore declare. La position
//    choisie est la premiere ou tout ce dont il depend existe.
#include "CasBarreMenus.h"
#include "CasOngletsVerticaux.h"
#include "CasHoteDansMenu.h"
#include "CasDefilementFondMouvant.h"

// Rodolf, 27/09 : « je pense qu'il y a encore plein de conteneurs qu'on peut
// ajouter, donc integre-les. »
//
// ⚠️ CHAQUE CAS COMPARE AVEC ET SANS LE CONTENEUR, SUR LE MEME ENFANT. Un critere
//    du genre « le panneau est la » serait vert meme si le conteneur ne faisait
//    RIEN : ce qu'on exige, c'est qu'il DEPLACE quelque chose, et de combien.
static void CasQuatreConteneursDePlus() {
	const uint32 kBleu = 0x0969DAFFu;
	// L'enfant est le meme partout : seul le conteneur change.
	const char *kEnfant = "    Panel \"cible\" { size = (80, 20)\n"
						  "      appearance { fill { color = #0969DA } }\n"
						  "    }\n";
	char doc[1024];

	// ── LA REFERENCE : l'enfant seul, dans une VBox ─────────────────────
	NkRect rRef{0.f, 0.f, 0.f, 0.f};
	{
		snprintf(doc, sizeof(doc), "nkgui 0.3\nwidgets {\n  VBox \"pile\" {\n%s  }\n}\n",
				 kEnfant);
		Scene s;
		Check(s.Charger(doc, (uint32)__builtin_strlen(doc), 300, 200),
			  "(b20) la reference (sans conteneur) se charge");
		s.Image();
		Check(BoiteCouleur(s.ras, kBleu, rRef), "(b20) la reference a une boite");
		printf("        [reference] (%.0f, %.0f, %.0f x %.0f)\n", rRef.x, rRef.y, rRef.w,
			   rRef.h);
		s.exe.Debrancher(s.ctx);
	}

	// ── `Padding` : il RENTRE le contenu, exactement ────────────────────
	{
		snprintf(doc, sizeof(doc),
				 "nkgui 0.3\nwidgets {\n  VBox \"pile\" {\n"
				 "    Padding \"marge\" { padding = 20\n%s    }\n  }\n}\n",
				 kEnfant);
		Scene s;
		Check(s.Charger(doc, (uint32)__builtin_strlen(doc), 300, 200),
			  "(b20) [Padding] le document se charge");
		s.Image();
		NkRect r{0.f, 0.f, 0.f, 0.f};
		Check(BoiteCouleur(s.ras, kBleu, r), "(b20) [Padding] la cible est peinte");
		printf("        [Padding 20] (%.0f, %.0f) — reference (%.0f, %.0f)\n", r.x, r.y,
			   rRef.x, rRef.y);
		// ⚠️ LE DECALAGE EXACT, PAS « IL A BOUGE ». Une marge de 20 deplace de 20 ;
		//    exiger « > 0 » aurait accepte une marge de 3 px.
		CheckEqF(r.x - rRef.x, 20.f, 1.f, "(b20) [Padding] DECALE DE 20 EN X, exactement");
		CheckEqF(r.y - rRef.y, 20.f, 1.f, "(b20) [Padding] et de 20 en Y");
		s.exe.Debrancher(s.ctx);
	}

	// ── `Aspect` : la hauteur SUIT la largeur ───────────────────────────
	{
		snprintf(doc, sizeof(doc),
				 "nkgui 0.3\nwidgets {\n  VBox \"pile\" {\n"
				 "    Aspect \"vue\" { ratio = 2.0, size = (200, 0)\n%s    }\n"
				 "    Panel \"apres\" { size = (60, 10)\n"
				 "      appearance { fill { color = #CF222E } }\n"
				 "    }\n  }\n}\n",
				 kEnfant);
		Scene s;
		Check(s.Charger(doc, (uint32)__builtin_strlen(doc), 300, 260),
			  "(b20) [Aspect] le document se charge");
		s.Image();
		NkRect rV{0.f, 0.f, 0.f, 0.f};
		const bool a = s.RectTout("vue", rV);
		Check(a, "(b20) [Aspect] le conteneur a un rectangle");
		if (a) {
			printf("        [Aspect 2.0] (%.0f x %.0f) — rapport %.2f\n", rV.w, rV.h,
				   rV.h > 0.f ? rV.w / rV.h : 0.f);
			// 200 de large, rapport 2 -> 100 de haut. C'est le seul role qui sache
			// dire « garde ce rapport » : aucun autre ne calcule une hauteur.
			CheckEqF(rV.w, 200.f, 1.f, "(b20) [Aspect] la largeur est celle demandee");
			CheckEqF(rV.h, 100.f, 1.f, "(b20) [Aspect] ET LA HAUTEUR SUIT LE RAPPORT");
		}
		// ⚠️ ET LE FRERE D'APRES DOIT PARTIR SOUS LUI. Un conteneur qui calcule sa
		//    hauteur sans la CONSOMMER laisserait le suivant se peindre dessus —
		//    exactement le defaut corrige ce matin sur `size` en flux.
		NkRect rApres{0.f, 0.f, 0.f, 0.f};
		if (s.RectTout("apres", rApres) && a)
			Check(rApres.y >= rV.y + rV.h - 1.f,
				  "(b20) [Aspect] et le frere suivant part SOUS lui");
		s.exe.Debrancher(s.ctx);
	}

	// ── `Overlay` : il peint dans l'AUTRE couche ────────────────────────
	{
		// Deux documents identiques a un mot pres : `VBox` contre `Overlay`. La
		// surimpression est une COUCHE, pas une region — donc les pixels sont les
		// memes, et c'est le COMPTEUR et la couche qui doivent changer.
		Scene sSans, sAvec;
		char d2[1024];
		snprintf(doc, sizeof(doc), "nkgui 0.3\nwidgets {\n  VBox \"couche\" {\n%s  }\n}\n",
				 kEnfant);
		snprintf(d2, sizeof(d2), "nkgui 0.3\nwidgets {\n  Overlay \"couche\" {\n%s  }\n}\n",
				 kEnfant);
		Check(sSans.Charger(doc, (uint32)__builtin_strlen(doc), 300, 200)
				  && sAvec.Charger(d2, (uint32)__builtin_strlen(d2), 300, 200),
			  "(b20) [Overlay] les deux documents se chargent");
		sSans.Image();
		sAvec.Image();
		printf("        [Overlay] surimpressions sans=%u avec=%u ; px sans=%u avec=%u\n",
			   sSans.rap.surimpressions, sAvec.rap.surimpressions,
			   ComptePixelsCouleur(sSans.ras, kBleu), ComptePixelsCouleur(sAvec.ras, kBleu));
		CheckEqU(sSans.rap.surimpressions, 0u, "(b20) [Overlay] sans le role, aucune couche");
		CheckEqU(sAvec.rap.surimpressions, 1u, "(b20) [Overlay] LE ROLE OUVRE LA COUCHE");
		// ⚠️ ET LE CONTENU ARRIVE QUAND MEME DANS L'IMAGE. Un `PushOverlay` sans
		//    `PopOverlay`, ou une couche que le banc ne rasterise pas, donnerait un
		//    compteur a 1 et un ecran vide — le compteur vert sur le rien.
		Check(ComptePixelsCouleur(sAvec.ras, kBleu) > 1000u,
			  "(b20) ET LE CONTENU EST PEINT — la couche n'avale pas ses enfants");
		sSans.exe.Debrancher(sSans.ctx);
		sAvec.exe.Debrancher(sAvec.ctx);
	}

	// ── `Center` : il centre ce que le document DECLARE ─────────────────
	{
		snprintf(doc, sizeof(doc),
				 "nkgui 0.3\nwidgets {\n"
				 "  Center \"milieu\" { size = (280, 180)\n%s  }\n}\n",
				 kEnfant);
		Scene s;
		Check(s.Charger(doc, (uint32)__builtin_strlen(doc), 300, 200),
			  "(b20) [Center] le document se charge");
		s.Image();
		NkRect r{0.f, 0.f, 0.f, 0.f}, rc{0.f, 0.f, 0.f, 0.f};
		const bool ab = BoiteCouleur(s.ras, kBleu, r);
		const bool ac = s.RectTout("milieu", rc);
		printf("        [Center] centrages=%u impossibles=%u ; cible (%.0f, %.0f, %.0f x "
			   "%.0f) dans (%.0f, %.0f, %.0f x %.0f)\n",
			   s.rap.centrages, s.rap.centragesImpossibles, r.x, r.y, r.w, r.h, rc.x, rc.y,
			   rc.w, rc.h);
		CheckEqU(s.rap.centrages, 1u, "(b20) [Center] un centrage a eu lieu");
		CheckEqU(s.rap.centragesImpossibles, 0u, "(b20) [Center] aucun enfant sans taille");
		Check(ab && ac, "(b20) [Center] la cible et le conteneur ont un rectangle");
		if (ab && ac) {
			// Le critere qui compte : les DEUX marges sont egales, a un pixel pres.
			const float32 gauche = r.x - rc.x;
			const float32 droite = (rc.x + rc.w) - (r.x + r.w);
			const float32 haut = r.y - rc.y;
			const float32 bas = (rc.y + rc.h) - (r.y + r.h);
			printf("        marges : gauche=%.0f droite=%.0f haut=%.0f bas=%.0f\n", gauche,
				   droite, haut, bas);
			CheckEqF(gauche, droite, 2.f, "(b20) [Center] LES MARGES GAUCHE ET DROITE SONT EGALES");
			CheckEqF(haut, bas, 2.f, "(b20) et les marges haut et bas aussi");
		}
		s.exe.Debrancher(s.ctx);
	}

	// ── LE NEGATIF DE `Center` : un enfant qui ne dit pas sa taille ─────
	{
		// 🔴 IL EXISTE PARCE QUE `Center` PEUT PARFAITEMENT NE RIEN CENTRER. Une
		//    interface immediate ne connait la taille d'un enfant qu'APRES l'avoir
		//    monte ; ce role centre donc ce que le document DECLARE. Si les deux
		//    compteurs n'en faisaient qu'un, un document entier pourrait etre colle
		//    en haut a gauche pendant qu'un chiffre dirait « centre ».
		static const char kSansTaille[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Center \"milieu\" { size = (280, 180)\n"
			"    Text \"libre\" { text = \"sans taille declaree\" }\n"
			"  }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kSansTaille, (uint32)(sizeof(kSansTaille) - 1u), 300, 200),
			  "(b20) [Center sans taille] le document se charge");
		s.Image();
		printf("        [Center sans taille] centrages=%u impossibles=%u\n", s.rap.centrages,
			   s.rap.centragesImpossibles);
		CheckEqU(s.rap.centrages, 0u, "(b20) AUCUN centrage — on n'invente pas la taille");
		CheckEqU(s.rap.centragesImpossibles, 1u,
				 "(b20) ET C'EST COMPTE — un centrage qui n'a pas eu lieu ne se tait pas");
		s.exe.Debrancher(s.ctx);
	}
}

static void CasRacineNommee() {
// =====================================================================
printf("\n-- (b19) `Monter(ctx, doc, \"racine\")` — UN document, PLUSIEURS regions\n");
// =====================================================================
// 🔴 LE MANQUE QUE TROIS APPLICATIONS PAYAIENT. `Monter` montait TOUTES les
//    sections `widgets` au curseur courant, et `MonterCorps` est privee : une
//    coquille qui pose sa barre de menu, sa barre d'outils et sa barre d'etat a
//    trois instants ne pouvait pas les servir depuis un seul fichier. Le prix se
//    comptait en fichiers — NkAntenne : 25 documents la ou 5 auraient suffi.
//
// ⚠️ LE CRITERE N'EST PAS « CA MONTE », C'EST « CA NE MONTE QUE CA ». Un
//    montage par racine qui monterait tout rendrait exactement les memes
//    compteurs qu'avant sur un document a une seule section : il faut un
//    document a TROIS racines et exiger que les deux autres restent absentes.
{
	static const char kDoc[] =
		"nkgui 0.3\n"
		"widgets \"barreMenu\" {\n"
		"  Panel \"p.menu\" { size = (200, 24)\n"
		"    appearance { fill { color = #1A7F37 } }\n"
		"  }\n"
		"}\n"
		"widgets \"barreEtat\" {\n"
		"  Panel \"p.etat\" { size = (200, 24)\n"
		"    appearance { fill { color = #8250DF } }\n"
		"  }\n"
		"}\n"
		"widgets \"corps\" {\n"
		"  Panel \"p.corps\" { size = (200, 24)\n"
		"    appearance { fill { color = #CF222E } }\n"
		"  }\n"
		"}\n";
	const uint32 kVert = 0x1A7F37FFu, kViolet = 0x8250DFFFu, kRouge = 0xCF222EFFu;

	// ── LE MONTAGE ENTIER : les trois sont la ───────────────────────────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b19) le document a trois racines se charge");
		s.Image();
		printf("        [tout] vert=%u violet=%u rouge=%u, sections=%u\n",
			   ComptePixelsCouleur(s.ras, kVert), ComptePixelsCouleur(s.ras, kViolet),
			   ComptePixelsCouleur(s.ras, kRouge), s.rap.sections);
		// La reference : `Monter` sans racine monte TOUT. Sans elle, « une seule
		// racine est montee » pourrait vouloir dire « le document n'en a qu'une ».
		Check(ComptePixelsCouleur(s.ras, kVert) > 3000u
				  && ComptePixelsCouleur(s.ras, kViolet) > 3000u
				  && ComptePixelsCouleur(s.ras, kRouge) > 3000u,
			  "(b19) [tout] les TROIS racines sont montees — c'est le comportement d'hier");
		s.exe.Debrancher(s.ctx);
	}

	// ── UNE RACINE NOMMEE : elle seule ──────────────────────────────────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b19) [racine] le document se charge");
		bool trouvee = false;
		s.ImageRacine("barreEtat", trouvee);
		const uint32 v = ComptePixelsCouleur(s.ras, kVert);
		const uint32 x = ComptePixelsCouleur(s.ras, kViolet);
		const uint32 r = ComptePixelsCouleur(s.ras, kRouge);
		printf("        [\"barreEtat\"] vert=%u violet=%u rouge=%u, montees=%u, "
			   "introuvables=%u\n",
			   v, x, r, s.rap.racinesMontees, s.rap.racinesIntrouvables);
		Check(trouvee, "(b19) la racine nommee est TROUVEE");
		CheckEqU(s.rap.racinesMontees, 1u, "(b19) une racine montee");
		CheckEqU(s.rap.racinesIntrouvables, 0u, "(b19) aucune introuvable");
		Check(x > 3000u, "(b19) LA RACINE DEMANDEE EST LA");
		// 🔴 LES DEUX MOITIES. « Elle est la » sans « les autres n'y sont pas »
		//    serait vert sur un montage qui monte tout.
		CheckEqU(v, 0u, "(b19) ET LA RACINE D'AVANT N'Y EST PAS");
		CheckEqU(r, 0u, "(b19) ni celle d'apres — on ne monte QUE ce qui est nomme");
		s.Png("b19_racine_nommee.png");
		s.exe.Debrancher(s.ctx);
	}

	// ── UN WIDGET DE PREMIER NIVEAU, par son identifiant ────────────────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b19) [widget] le document se charge");
		bool trouvee = false;
		s.ImageRacine("p.corps", trouvee);
		printf("        [\"p.corps\"] rouge=%u, montees=%u\n",
			   ComptePixelsCouleur(s.ras, kRouge), s.rap.racinesMontees);
		Check(trouvee, "(b19) un WIDGET de premier niveau sert aussi de racine");
		Check(ComptePixelsCouleur(s.ras, kRouge) > 3000u, "(b19) et il est monte");
		CheckEqU(ComptePixelsCouleur(s.ras, kVert), 0u, "(b19) lui SEUL");
		s.exe.Debrancher(s.ctx);
	}

	// ── LE NEGATIF : une racine qui n'existe pas ────────────────────────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b19) [faute] le document se charge");
		bool trouvee = true;
		s.ImageRacine("barreEtatt", trouvee); // la faute de frappe la plus probable
		printf("        [faute] montees=%u, introuvables=%u (« %s »), peints=%u\n",
			   s.rap.racinesMontees, s.rap.racinesIntrouvables,
			   s.rap.derniereRacineIntrouvable.CStr(), ComptePixelsPeints(s.ras, kFond));
		Check(!trouvee, "(b19) [faute] elle rend FAUX");
		CheckEqU(s.rap.racinesMontees, 0u, "(b19) [faute] rien n'est monte");
		CheckEqU(s.rap.racinesIntrouvables, 1u, "(b19) [faute] et c'est COMPTE");
		// 🔴 LE CHIFFRE NE SUFFIT PAS. Un appelant qui ignore le retour monterait
		//    une region vide sans erreur ; le NOM demande est ce qui transforme
		//    « il manque quelque chose » en « tu as ecrit barreEtatt ».
		Check(s.rap.derniereRacineIntrouvable.Compare(NkString("barreEtatt")) == 0,
			  "(b19) ET LE NOM DEMANDE EST GARDE — sans lui, la faute se cherche a la main");
		s.exe.Debrancher(s.ctx);
	}
}

}

// =============================================================================
//  LES CAS RECENTS, CHACUN DANS SA FONCTION — et ce n'est pas du rangement
// =============================================================================
// 🔴 LE BANC A DEBORDE LA PILE. `sizeof(Scene)` vaut 37 064 octets, et `main`
//    en portait plus de vingt-cinq en variables locales : en Debug le compilateur
//    ne reutilise pas les emplacements des portees imbriquees, donc le cadre
//    cumulait ~900 Ko et le processus mourait sur `0xC00000FD` pendant (b18).
//
// ⚠️ ET LE SYMPTOME ACCUSAIT LE MAUVAIS COUPABLE, DEUX FOIS. Sous git-bash le
//    code rendu est 127 — celui d'une commande introuvable, donc on cherche une
//    DLL manquante ; c'est PowerShell qui a rendu `-1073741571`. Puis la sortie
//    redirigee, mise en tampon par blocs, s'est perdue a l'abandon : « aucune
//    ligne imprimee » m'a fait conclure au PROLOGUE de `main`, alors que le banc
//    tournait jusqu'a (b18). *Un code de sortie traduit par un shell n'est pas le
//    code de sortie, et une sortie tamponnee perdue n'est pas une sortie absente.*
//
//    Le chiffre qui a tranche est `sizeof(Scene)`, imprime en premiere ligne.
//    Chaque cas neuf se sort desormais de `main`.
// =============================================================================
static void CasConteneursFreres() {
// =====================================================================
printf("\n-- (b12) DEUX CONTENEURS FRERES NE SE SUPERPOSENT PLUS — `size` en flux\n");
// =====================================================================
// 🔴 LE DEFAUT QUE RODOLF A DEMANDE DE CORRIGER LE 27/09. `NkGuiLirePlacement`
//    ne rend `pose` que s'il a lu un `pos` ; en flux il n'y en a pas, donc la
//    branche `pos`+`size` de `RegionCourante` ne pouvait JAMAIS s'executer.
//    `size` sur un conteneur en flux etait donc lu par PERSONNE, et les deux
//    `Panel` ci-dessous prenaient tous les deux TOUTE la region : le second
//    repeignait le premier, exactement.
//
// ⚠️ ET LE CRITERE NE PEUT PAS ETRE « L'IMAGE A CHANGE ». Il faut la PRESENCE
//    de chacune des deux couleurs : c'est la seule chose qu'un recouvrement
//    total fait tomber a ZERO. Un compteur de pixels peints, lui, rendait le
//    meme total dans les deux mondes — la region est remplie de toute facon.
//    C'est la troisieme fois en deux jours qu'un total masque un remplacement.
{
	static const char kDoc[] =
		"nkgui 0.3\n"
		"widgets {\n"
		"  Window \"racine\" {\n"
		"    VBox \"pile\" {\n"
		"      Panel \"haut\" {\n"
		"        size = (180, 50)\n"
		"        appearance { fill { color = #1A7F37 } }\n"
		"      }\n"
		"      Panel \"bas\" {\n"
		"        size = (180, 50)\n"
		"        appearance { fill { color = #8250DF } }\n"
		"      }\n"
		"    }\n"
		"  }\n"
		"}\n";
	const uint32 vert = 0x1A7F37FFu;	// le Panel du HAUT
	const uint32 violet = 0x8250DFFFu;	// le Panel du BAS

	// ── LE NEGATIF D'ABORD : la mutation remet le defaut ────────────────
	// ⚠️ ON LE MESURE AVANT LE CORRECTIF, ET DANS LE MEME PROCESSUS. Un negatif
	//    qu'on garde « pour plus tard » ne se fait jamais ; et la mutation n'est
	//    pas mise en cache precisement pour que les deux mondes tiennent dans
	//    une seule execution, donc comparables pixel a pixel.
	NkDefinirVariable("NK_TAILLE_MUTATION", "flux");
	uint32 vMute = 0u, xMute = 0u, tailleFluxMute = 0u;
	{
		Scene sM;
		Check(sM.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b12) [mute] le document se charge");
		sM.Image();
		sM.Image();
		vMute = ComptePixelsCouleur(sM.ras, vert);
		xMute = ComptePixelsCouleur(sM.ras, violet);
		tailleFluxMute = sM.rap.conteneursTailleFlux;
		printf("        [mute]    vert = %u, violet = %u, tailleFlux = %u\n", vMute, xMute,
			   tailleFluxMute);
		sM.Png("b12_mute.png");
		sM.exe.Debrancher(sM.ctx);
	}
	NkDefinirVariable("NK_TAILLE_MUTATION", nullptr);

	// ── LE MONDE CORRIGE ───────────────────────────────────────────────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b12) le document se charge");
		s.Image();
		s.Image();
		const uint32 v = ComptePixelsCouleur(s.ras, vert);
		const uint32 x = ComptePixelsCouleur(s.ras, violet);
		printf("        [corrige] vert = %u, violet = %u, tailleFlux = %u, sansTaille = %u\n", v,
			   x, s.rap.conteneursTailleFlux, s.rap.conteneursSansTaille);

		// Le negatif rougit-il ? Les deux moities, parce qu'un recouvrement a
		// DEUX signes : la couleur du dessous disparait, ET celle du dessus
		// deborde de sa taille declaree.
		CheckEqU(tailleFluxMute, 0u, "(b12) [mute] `size` n'est lu par personne");
		CheckEqU(vMute, 0u, "(b12) [mute] LE PANNEAU DU HAUT A DISPARU — recouvert");
		Check(xMute > 30000u,
			  "(b12) [mute] et celui du bas prend TOUTE la region, pas ses 180x50");

		// Le correctif. 180 x 50 = 9 000 px par panneau ; le seuil laisse la
		// place aux coins arrondis du theme et au contour, sans laisser passer
		// un panneau qui deborderait (la region entiere fait 64 000).
		CheckEqU(s.rap.conteneursTailleFlux, 2u, "(b12) les deux `size` sont LUS");
		Check(v > 6000u && v < 12000u,
			  "(b12) LE PANNEAU DU HAUT EST VISIBLE, a sa taille declaree");
		Check(x > 6000u && x < 12000u,
			  "(b12) celui du bas aussi — ils ne se recouvrent plus");

		// ⚠️ ET LA PREUVE GEOMETRIQUE, PARCE QUE DEUX COMPTES JUSTES NE DISENT
		//    PAS « L'UN SOUS L'AUTRE ». Deux panneaux cote a cote, ou decales de
		//    trois pixels, donneraient exactement les memes deux chiffres.
		NkRect bH{0.f, 0.f, 0.f, 0.f}, bB{0.f, 0.f, 0.f, 0.f};
		const bool aH = BoiteCouleur(s.ras, vert, bH);
		const bool aB = BoiteCouleur(s.ras, violet, bB);
		Check(aH && aB, "(b12) les deux couleurs ont une boite englobante");
		if (aH && aB) {
			printf("        haut = (%.0f, %.0f, %.0f x %.0f), bas = (%.0f, %.0f, %.0f x %.0f)\n",
				   bH.x, bH.y, bH.w, bH.h, bB.x, bB.y, bB.w, bB.h);
			Check(bH.y + bH.h <= bB.y + 1.f,
				  "(b12) LE HAUT EST AU-DESSUS DU BAS — les boites ne se croisent pas");
			Check(bH.x == bB.x, "(b12) et ils partagent leur bord gauche : c'est bien une pile");
		}

		// La racine, elle, ne dit pas sa taille : elle prend ce qui reste. Ce
		// n'est pas un defaut — c'est le sens de « tout le reste » pour le
		// dernier — mais le rapport doit le NOMMER, sinon un document ambigu
		// passe sans qu'on puisse le voir.
		CheckEqU(s.rap.conteneursSansTaille, 1u,
				 "(b12) le `Window` racine est compte SANS TAILLE, pas oublie");
		s.Png("b12_corrige.png");
		s.exe.Debrancher(s.ctx);
	}
}
}

static void CasTableRappels() {
// =====================================================================
printf("\n-- (b13) LA TABLE DE RAPPELS — lambda capturante, METHODE, et le silence compte\n");
// =====================================================================
// Rodolf, 27/09 : « est-ce que le systeme pour brancher une fonction ou
// methode ou lambda sur les evenements callback est deja en place ? »
//
// La reponse mesuree etait : le FIL oui, le BRANCHEMENT non. `NkGuiCallbackFn`
// est un pointeur de fonction C nu — une lambda CAPTURANTE ne s'y convertit
// pas, une METHODE non plus. Ce banc lui-meme ecrivait SIX fois le meme
// trampoline plus un `this` deguise en `void *`.
//
// ⚠️ LE CRITERE NE PEUT PAS ETRE « ca compile ». Une table qui accepte un
//    appelable et ne l'appelle jamais compile parfaitement. Ce qui est exige
//    ici, c'est que la CAPTURE ait ete vue (un compteur exterieur a la lambda
//    a bouge) et que l'objet ait recu l'appel SUR LUI (son propre champ a
//    change, pas une variable globale).
{
	static const char kDoc[] =
		"nkgui 0.3\n"
		"widgets {\n"
		"  Button \"ok\" { label = \"Valider\" }\n"
		"}\n"
		"behavior \"b\" {\n"
		"  Callback \"sauver\"(7)\n"
		"  Callback \"ouvrir\"(\"scene.nk\")\n"
		"  Callback \"personne\"()\n"
		"}\n";
	Scene s;
	Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 120),
		  "(b13) le document a trois `Callback` se charge");

	// Un objet, pour prouver la METHODE. Il compte sur LUI-MEME : une variable
	// globale ne dirait pas que l'instance a bien ete retrouvee.
	struct Panneau {
			uint32 recus = 0u;
			NkString dernier;
			void SurOuvrir(const NkGuiAppelCallback &a) {
				++recus;
				// ⚠️ UNE CHAINE CITEE ARRIVE EN `Jeton`, PAS EN « TEXTE ».
				//    `NkGuiValeur` n'a que trois types (Nombre, Booleen, Jeton)
				//    et le texte d'un `Callback "x"("scene.nk")` vit dans
				//    `.jeton` — c'est ce que (b2) lisait deja. Ecrire `.texte`
				//    ne compilait pas, et c'est tant mieux : un champ qui aurait
				//    existe et rendu du vide aurait fait un critere vert sur un
				//    argument perdu.
				if (a.nbArgs > 0u && a.args[0].type == NkGuiValeur::Type::Jeton)
					dernier = a.args[0].jeton;
			}
	};
	Panneau panneau;

	// Une capture par reference : c'est precisement ce que le pointeur de
	// fonction nu ne savait pas porter.
	uint32 sauves = 0u;
	float32 argVu = -1.f;

	NkGuiRappels rappels;
	Check(rappels.Brancher(NkStringView("sauver"),
						   NkGuiRappels::Rappel([&](const NkGuiAppelCallback &a) {
							   ++sauves;
							   if (a.nbArgs > 0u)
								   argVu = a.args[0].nombre;
						   })),
		  "(b13) une LAMBDA CAPTURANTE se branche");
	Check(rappels.Brancher(NkStringView("ouvrir"),
						   NkGuiRappels::Rappel(&panneau, &Panneau::SurOuvrir)),
		  "(b13) une METHODE se branche, sur SON instance");
	CheckEqU(rappels.Nombre(), 2u, "(b13) la table porte deux noms");
	CheckEqU(rappels.branches, 2u, "(b13) deux branchements NEUFS");
	CheckEqU(rappels.remplaces, 0u, "(b13) aucun remplacement");
	Check(rappels.EstBranche(NkStringView("sauver")), "(b13) `sauver` est branche");
	Check(!rappels.EstBranche(NkStringView("personne")),
		  "(b13) `personne` ne l'est pas — c'est le negatif de la mesure d'apres");

	// Une seule ligne au lieu des deux que ce banc ecrit six fois ailleurs.
	rappels.BrancherSur(s.exe.eval);
	s.Image();

	printf("        servis = %u, sansDestinataire = %u (« %s »), sauves = %u, arg = %.1f\n",
		   rappels.servis, rappels.sansDestinataire,
		   rappels.dernierSansDestinataire.CStr(), sauves, argVu);

	CheckEqU(rappels.servis, 2u, "(b13) DEUX appels ont atteint un destinataire");
	// ⚠️ ET LA CAPTURE, PAS SEULEMENT L'APPEL. `servis` monterait aussi si la
	//    table appelait un appelable VIDE : c'est le compteur exterieur a la
	//    lambda qui prouve que la fermeture a ete portee jusqu'au bout.
	CheckEqU(sauves, 1u, "(b13) LA CAPTURE A ETE VUE — le compteur du dehors a bouge");
	CheckEqF(argVu, 7.f, 0.001f, "(b13) et l'argument du document est arrive");
	CheckEqU(panneau.recus, 1u, "(b13) L'OBJET a recu l'appel sur LUI");
	Check(panneau.dernier.Compare(NkString("scene.nk")) == 0,
		  "(b13) avec son argument texte");

	// 🔴 LE CHIFFRE POUR LEQUEL CETTE CLASSE EXISTE. `Callback "personne"` n'a
	//    aucun destinataire : sans ce compteur il disparaitrait sans un mot,
	//    exactement comme les sept abandons silencieux du 27/09.
	CheckEqU(rappels.sansDestinataire, 1u,
			 "(b13) LE NOM SANS DESTINATAIRE EST COMPTE, pas perdu");
	Check(rappels.dernierSansDestinataire.Compare(NkString("personne")) == 0,
		  "(b13) ET NOMME — un compteur seul dirait qu'il manque quelque chose sans dire quoi");

	// Re-brancher un nom existant : legitime, et ce n'est PAS un ajout.
	Check(rappels.Brancher(NkStringView("sauver"),
						   NkGuiRappels::Rappel([&](const NkGuiAppelCallback &) { ++sauves; })),
		  "(b13) re-brancher un nom deja la reussit");
	CheckEqU(rappels.Nombre(), 2u, "(b13) et n'ajoute PAS de ligne");
	CheckEqU(rappels.remplaces, 1u, "(b13) le remplacement est compte a part de l'ajout");

	Check(rappels.Debrancher(NkStringView("ouvrir")), "(b13) `ouvrir` se debranche");
	Check(!rappels.Debrancher(NkStringView("ouvrir")),
		  "(b13) et une seconde fois rend FAUX — « rien a retirer » n'est pas « retire »");
	rappels.ReinitialiserCompteursAppel();
	s.Image();
	CheckEqU(rappels.sansDestinataire, 2u,
			 "(b13) debranche, `ouvrir` rejoint `personne` dans les sans-destinataire");
	s.exe.Debrancher(s.ctx);
}
}

static void CasQuatreConteneurs() {
// =====================================================================
printf("\n-- (b14) VBox, HBox, Group, Stack : `size` les CONFINE aussi\n");
// =====================================================================
// Rodolf, 27/09 : « on doit avoir plusieurs conteneur vbox hbox stack et tout
// ce que tu juge fonctionnel ».
//
// ⚠️ CES QUATRE-LA NE PEIGNENT RIEN, DONC ON NE PEUT PAS LES MESURER
//    DIRECTEMENT. Le critere passe par un `Panel` colore DANS chacun : si le
//    conteneur s'est confine a la bande que `size` demande, son panneau tombe
//    dans cette bande ; s'il a pris toute la region, son panneau part d'ailleurs
//    et les bandes se croisent. Lire les rectangles du RAPPORT aurait mesure ce
//    que le monteur CROIT avoir fait — ici on mesure ce qui est arrive a l'image.
{
	static const char kDoc[] =
		"nkgui 0.3\n"
		"widgets {\n"
		"  Window \"racine\" {\n"
		"    VBox \"pile\" {\n"
		"      VBox \"ca\" { size = (200, 44)\n"
		"        Panel \"pa\" { size = (160, 24)\n"
		"          appearance { fill { color = #1A7F37 } }\n"
		"        }\n"
		"      }\n"
		"      HBox \"cb\" { size = (200, 44)\n"
		"        Panel \"pb\" { size = (160, 24)\n"
		"          appearance { fill { color = #8250DF } }\n"
		"        }\n"
		"      }\n"
		"      Group \"cc\" { size = (200, 44)\n"
		"        Panel \"pc\" { size = (160, 24)\n"
		"          appearance { fill { color = #CF222E } }\n"
		"        }\n"
		"      }\n"
		"      Stack \"cd\" { size = (200, 44)\n"
		"        Panel \"pd\" { size = (160, 24)\n"
		"          appearance { fill { color = #0969DA } }\n"
		"        }\n"
		"      }\n"
		"    }\n"
		"  }\n"
		"}\n";
	const uint32 couleurs[4] = {0x1A7F37FFu, 0x8250DFFFu, 0xCF222EFFu, 0x0969DAFFu};
	const char *noms[4] = {"VBox", "HBox", "Group", "Stack"};
	Scene s;
	Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 220),
		  "(b14) le document a quatre conteneurs de taille se charge");
	s.Image();
	s.Image();
	printf("        tailleFlux = %u, sansTaille = %u\n", s.rap.conteneursTailleFlux,
		   s.rap.conteneursSansTaille);
	// Quatre conteneurs PLUS leurs quatre panneaux : huit `size` honores. Un
	// total de quatre voudrait dire que les panneaux interieurs, eux, ne le
	// sont pas — et le cas serait vert sur une moitie du travail.
	CheckEqU(s.rap.conteneursTailleFlux, 8u,
			 "(b14) HUIT `size` honores — les quatre conteneurs ET leurs quatre panneaux");
	NkRect boites[4];
	bool toutes = true;
	for (uint32 k = 0; k < 4u; ++k) {
		const uint32 n = ComptePixelsCouleur(s.ras, couleurs[k]);
		const bool a = BoiteCouleur(s.ras, couleurs[k], boites[k]);
		if (!a)
			toutes = false;
		printf("        %-6s : %u px", noms[k], n);
		if (a)
			printf(", boite (%.0f, %.0f, %.0f x %.0f)", boites[k].x, boites[k].y,
				   boites[k].w, boites[k].h);
		printf("\n");
		// 160 x 24 = 3 840 px ; la borne haute exclut un panneau qui aurait
		// pris la region (320 x 220 = 70 400).
		Check(n > 2500u && n < 5000u, noms[k]);
	}
	Check(toutes, "(b14) les quatre couleurs sont PRESENTES — aucune n'est recouverte");
	if (toutes) {
		// ⚠️ ET L'ORDRE, PAS SEULEMENT LA PRESENCE. Trois comptes justes ne
		//    disent pas « l'un sous l'autre » : trois bandes empilees a l'envers,
		//    ou trois bandes qui se chevauchent de deux pixels, rendraient
		//    exactement les memes trois chiffres.
		bool ordonnees = true;
		for (uint32 k = 0; k + 1u < 4u; ++k)
			if (boites[k].y + boites[k].h > boites[k + 1].y)
				ordonnees = false;
		Check(ordonnees, "(b14) LES QUATRE BANDES SONT EMPILEES DANS L'ORDRE DU DOCUMENT");
		// Chaque conteneur demande 44 px ; le pas attendu est donc 44 plus
		// l'espacement du theme, jamais la hauteur de la bande peinte (24).
		bool pasJuste = true;
		printf("        pas entre bandes :");
		for (uint32 k = 0; k + 1u < 4u; ++k) {
			const float32 pas = boites[k + 1].y - boites[k].y;
			printf(" %.0f", pas);
			if (pas < 40.f || pas > 56.f)
				pasJuste = false;
		}
		printf(" px (conteneurs de 44 px + espacement du theme)\n");
		Check(pasJuste, "(b14) ET LE PAS VAUT LA TAILLE DEMANDEE — chacun a pris ses 44 px");

		// ⚠️ LE CRITERE QUE LE PREMIER RELEVE A RECLAME. Les quatre bandes
		//    etaient empilees et le pas tenait dans l'intervalle — mais celle du
		//    `Stack` sortait a x = 10 quand les trois autres etaient a x = 20, et
		//    son pas valait 40 au lieu de 50. Un intervalle assez large pour
		//    accepter les deux ne distingue rien : *different ne veut pas dire
		//    visible, et un intervalle qui accepte tout ne refute rien*.
		//
		//    Ce qu'on exige donc ici : le MEME decalage de l'enfant dans son
		//    conteneur, pour les quatre. C'est ce qui fait qu'un document ecrit
		//    pour une `VBox` se relit pareil dans un `Stack`.
		const char *ids[4] = {"ca", "cb", "cc", "cd"};
		float32 dx[4] = {0.f, 0.f, 0.f, 0.f}, dy[4] = {0.f, 0.f, 0.f, 0.f};
		bool tousRects = true;
		printf("        decalage de l'enfant dans son conteneur :");
		for (uint32 k = 0; k < 4u; ++k) {
			NkRect rc{0.f, 0.f, 0.f, 0.f};
			if (!s.RectTout(ids[k], rc)) {
				tousRects = false;
				continue;
			}
			dx[k] = boites[k].x - rc.x;
			dy[k] = boites[k].y - rc.y;
			printf("  %s (%.0f, %.0f)", noms[k], dx[k], dy[k]);
		}
		printf("\n");
		Check(tousRects, "(b14) les quatre conteneurs ont un rectangle au rapport");
		if (tousRects) {
			bool memeDecalage = true;
			for (uint32 k = 1; k < 4u; ++k)
				if (dx[k] != dx[0] || dy[k] != dy[0])
					memeDecalage = false;
			Check(memeDecalage,
				  "(b14) LES QUATRE PLACENT LEUR ENFANT AU MEME ENDROIT — un document "
				  "ecrit pour l'un se relit dans l'autre");
		}
	}
	s.Png("b14_conteneurs.png");
	s.exe.Debrancher(s.ctx);
}
}

static void CasDockable() {
// =====================================================================
printf("\n-- (b15) LE DOCKABLE : la section `layout` PLACE les zones\n");
// =====================================================================
// Rodolf, 27/09 : « on doit aussi avoir le dockable ».
//
// 🔴 P10 ETAIT LU PAR PERSONNE. `NkGuiLireDispositions` existait depuis ce matin,
//    valide et compte — et son SEUL appelant etait le banc `NKGuiMonteTest`. Le
//    monteur ne l'appelait JAMAIS : un document qui ecrivait `dock "outils" left
//    0.16` etait analyse puis ignore, et les zones restaient partagees en bandes
//    verticales egales. *Un banc qui prouve son propre lecteur ne prouve pas que
//    le produit s'en sert*, et je l'avais annonce comme livre.
//
// ⚠️ LE CRITERE EXIGE LES QUATRE COTES, PAS UN. Une disposition qui ne saurait
//    que le `left` rendrait exactement le meme resultat que l'ancien partage
//    horizontal sur un document qui n'ecrit qu'un `left`.
{
	// Les zones ne sont servies par personne : chacune se marque de hachures et
	// ECRIT SON NOM. C'est ce qui les rend reperables sans hote.
	static const char kZones[] =
		"  DockSpace \"espace\" {\n"
		"    Host \"outils\" {}\n"
		"    Host \"scene\" {}\n"
		"    Host \"props\" {}\n"
		"    Host \"barre\" {}\n"
		"    Host \"journal\" {}\n"
		"  }\n";
	char sans[1024], avec[2048];
	// ⚠️ LES DEUX DOCUMENTS PARTAGENT LEUR SECTION `widgets`, AU CARACTERE. Deux
	//    arbres ecrits a la main auraient pu differer par autre chose que la
	//    section `layout`, et la comparaison aurait mesure cette difference-la.
	snprintf(sans, sizeof(sans), "nkgui 0.3\nwidgets {\n%s}\n", kZones);
	snprintf(avec, sizeof(avec),
			 "nkgui 0.3\nwidgets {\n%s}\n"
			 "layout \"defaut\" {\n"
			 "  dock \"outils\" left 0.2\n"
			 "  dock \"props\" right 0.25\n"
			 "  dock \"barre\" top 0.1\n"
			 "  dock \"journal\" bottom 0.15\n"
			 "  dock \"scene\" center\n"
			 "}\n",
			 kZones);

	NkRect rSans[5], rAvec[5];
	const char *noms[5] = {"outils", "scene", "props", "barre", "journal"};
	uint32 amarreesSans = 0u, amarreesAvec = 0u;
	bool lusSans = true, lusAvec = true;

	{
		Scene s;
		Check(s.Charger(sans, (uint32)__builtin_strlen(sans), 400, 300),
			  "(b15) [sans layout] le document se charge");
		s.Image();
		amarreesSans = s.rap.zonesAmarrees;
		for (uint32 k = 0; k < 5u; ++k)
			if (!s.RectTout(noms[k], rSans[k]))
				lusSans = false;
		s.exe.Debrancher(s.ctx);
	}
	{
		Scene s;
		Check(s.Charger(avec, (uint32)__builtin_strlen(avec), 400, 300),
			  "(b15) [avec layout] le document se charge");
		s.Image();
		amarreesAvec = s.rap.zonesAmarrees;
		printf("        dispositions lues = %u, amarrages = %u, zonesAmarrees = %u\n",
			   s.etat.rapportDispositions.dispositions,
			   s.etat.rapportDispositions.amarrages, amarreesAvec);
		Check(s.etat.rapportDispositions.Propre(),
			  "(b15) la disposition est PROPRE — aucun cote inconnu, aucune fraction hors bornes");
		for (uint32 k = 0; k < 5u; ++k)
			if (!s.RectTout(noms[k], rAvec[k]))
				lusAvec = false;
		s.Png("b15_dock.png");
		s.exe.Debrancher(s.ctx);
	}

	Check(lusSans && lusAvec, "(b15) les cinq zones ont un rectangle dans les deux cas");
	// Le negatif : sans section `layout`, RIEN n'est amarre — c'est l'etat d'avant,
	// et c'est ce qui prouve que le chemin d'hier n'a pas bouge.
	CheckEqU(amarreesSans, 0u, "(b15) [sans layout] AUCUNE zone amarree — chemin d'hier intact");
	CheckEqU(amarreesAvec, 4u, "(b15) [avec layout] les QUATRE cotes sont amarres");

	if (lusSans && lusAvec) {
		for (uint32 k = 0; k < 5u; ++k)
			printf("        %-8s sans (%.0f, %.0f, %.0f x %.0f)  avec (%.0f, %.0f, %.0f x %.0f)\n",
				   noms[k], rSans[k].x, rSans[k].y, rSans[k].w, rSans[k].h, rAvec[k].x,
				   rAvec[k].y, rAvec[k].w, rAvec[k].h);
		// Sans disposition : cinq bandes verticales de meme hauteur. C'est le
		// partage d'hier, et il doit rester exactement celui-la.
		bool bandes = true;
		for (uint32 k = 0; k < 5u; ++k)
			if (rSans[k].h != rSans[0].h)
				bandes = false;
		Check(bandes, "(b15) [sans layout] cinq bandes de MEME hauteur — le partage horizontal");

		// Avec disposition, chaque cote a sa place. Les fractions se rapportent a la
		// zone entiere (400 x 300) : outils 80 de large, props 100, barre 30 de haut,
		// journal 45.
		CheckEqF(rAvec[0].w, 80.f, 1.5f, "(b15) `outils` fait 20 % de la LARGEUR");
		CheckEqF(rAvec[2].w, 100.f, 1.5f, "(b15) `props` fait 25 % de la largeur");
		CheckEqF(rAvec[3].h, 30.f, 1.5f, "(b15) `barre` fait 10 % de la HAUTEUR");
		CheckEqF(rAvec[4].h, 45.f, 1.5f, "(b15) `journal` fait 15 % de la hauteur");
		// ⚠️ ET LES COTES, PAS SEULEMENT LES TAILLES. Quatre largeurs justes
		//    n'excluent pas que `props` soit a gauche : il faut dire OU.
		Check(rAvec[0].x < rAvec[1].x, "(b15) `outils` est A GAUCHE de la scene");
		Check(rAvec[2].x > rAvec[1].x, "(b15) `props` est A DROITE de la scene");
		Check(rAvec[3].y < rAvec[1].y, "(b15) `barre` est AU-DESSUS de la scene");
		Check(rAvec[4].y > rAvec[1].y, "(b15) `journal` est AU-DESSOUS de la scene");
		// Le centre prend ce qui reste, et il ne reste AUCUN trou : c'est la regle
		// que ce `case` s'etait donnee le 17/09 (« un dock qui laisse un trou n'est
		// pas un dock »), et elle doit tenir par ce chemin-ci aussi.
		CheckEqF(rAvec[1].x + rAvec[1].w, rAvec[2].x, 1.5f,
				 "(b15) la scene touche `props` — aucune bande orpheline a droite");
		CheckEqF(rAvec[1].y + rAvec[1].h, rAvec[4].y, 1.5f,
				 "(b15) et elle touche `journal` — aucune bande orpheline en bas");
	}
}
}

static void CasMenuContextuel() {
// =====================================================================
printf("\n-- (b16) `ContextMenu` : le refus est LEVE par le crochet qu'il reclamait\n");
// =====================================================================
// L'ancien refus, dans l'enumeration des roles : « NKGui a bien `BeginPopupMenu`,
// mais il ne rend vrai que si quelqu'un a appele `ctx.OpenPopupAt(...)` AU CLIC
// DROIT -- et ce monteur monte l'etat au REPOS : il n'a aucun clic droit a offrir.
// [...] CE QU'IL FAUDRAIT POUR LE LEVER : un crochet d'ouverture cote hote. »
//
// ⚠️ ET LE RAISONNEMENT DU REFUS EST DEVENU LE CRITERE. Il disait qu'un role
//    invisible par construction serait PIRE qu'un role refuse, « parce qu'il se
//    compterait parmi les montes et que personne n'irait chercher pourquoi rien
//    n'apparait ». Ce cas exige donc les DEUX moities : que le menu s'ouvre quand
//    l'hote le dit, ET qu'il se compte A PART quand personne ne l'ouvre.
{
	static const char kDoc[] =
		"nkgui 0.3\n"
		"widgets {\n"
		"  Button \"cible\" { label = \"Clic droit ici\" }\n"
		"  ContextMenu \"menu.scene\" {\n"
		"    MenuItem \"m.copier\" { label = \"Copier\", shortcut = \"Ctrl+C\" }\n"
		"    MenuItem \"m.coller\" { label = \"Coller\" }\n"
		"    MenuItem \"m.suppr\" { label = \"Supprimer\" }\n"
		"  }\n"
		"}\n";
	// L'hote : il ouvre le menu qu'il connait, a la position qu'il choisit — et
	// SEULEMENT celui-la. Un hote qui repondrait vrai a tout nom prouverait que le
	// monteur appelle le crochet, pas qu'il ecoute sa reponse.
	struct HoteMenu : public NkGuiMonteHooks {
			bool ouvrir = false;
			uint32 demandes = 0u;
			NkString dernierNom;
			bool MenuContextuelOuvre(const char *nom, NkVec2 &posOut) noexcept override {
				++demandes;
				dernierNom = NkString(nom);
				if (!ouvrir)
					return false;
				posOut = NkVec2{40.f, 60.f};
				return true;
			}
	};

	// ── SANS HOTE DU TOUT : le role est monte, et le menu COMPTE ────────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b16) le document a `ContextMenu` se charge");
		s.Image();
		printf("        [sans hote]  inconnus = %u, ouverts = %u, sansHote = %u, "
			   "itemsHorsMenu = %u\n",
			   s.rap.rolesInconnus, s.rap.menusContextuelsOuverts,
			   s.rap.menusContextuelsSansHote, s.rap.elementsMenuHorsMenu);
		// 🔴 LA MOITIE QUI PROUVE QUE LE REFUS EST LEVE. Avant aujourd'hui, ce
		//    document rendait `rolesInconnus = 1` : le role n'existait pas.
		CheckEqU(s.rap.rolesInconnus, 0u,
				 "(b16) `ContextMenu` N'EST PLUS UN ROLE INCONNU");
		CheckEqU(s.rap.menusContextuelsOuverts, 0u, "(b16) [sans hote] rien ne s'ouvre");
		CheckEqU(s.rap.menusContextuelsSansHote, 1u,
				 "(b16) [sans hote] ET LE MENU EST COMPTE — pas un role invisible qui se dit monte");
		// 🔴 CE CRITERE ETAIT FAUX, ET C'EST MOI QUI L'ETAIS. J'attendais 3 :
		//    « les trois entrees sont comptees hors menu ». Il rendait 0, parce
		//    qu'un menu FERME ne monte pas son contenu du tout — et c'est la
		//    politique que le role `Menu` s'etait deja donnee, mot pour mot :
		//    « LE CONTENU D'UN MENU FERME NE SE MONTE PAS, et ce n'est pas une
		//    perte : NKGui ne dessine ses entrees que dans le popup ouvert. [...]
		//    d'ou `menusOuverts`, qui separe les deux. » Monter les entrees en flux
		//    pour pouvoir les compter aurait fait exactement ce que ce role refuse :
		//    de faux boutons hors de leur menu.
		//
		//    Ce qui dit qu'il y a un menu invisible, ce n'est donc PAS un compte
		//    d'entrees — c'est `menusContextuelsSansHote`, verifie juste au-dessus.
		//    `elementsMenuHorsMenu` garde son sens d'origine : un `MenuItem` ecrit
		//    en dehors de toute chaine de menus, ce que ce document ne fait pas.
		CheckEqU(s.rap.elementsMenuHorsMenu, 0u,
				 "(b16) [sans hote] AUCUNE entree montee — un menu ferme ne monte pas son contenu");
		s.exe.Debrancher(s.ctx);
	}

	// ── L'HOTE EXISTE MAIS N'OUVRE PAS : le crochet est bien APPELE ─────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b16) [hote ferme] le document se charge");
		HoteMenu h;
		h.ouvrir = false;
		// Ce document n'a ni `behavior` ni `bind` : remplacer les crochets de
		// l'execution par cet hote-ci est sans effet sur ce qu'il monte.
		s.Image(&h);
		printf("        [hote ferme] demandes = %u (« %s »), ouverts = %u, sansHote = %u\n",
			   h.demandes, h.dernierNom.CStr(), s.rap.menusContextuelsOuverts,
			   s.rap.menusContextuelsSansHote);
		// ⚠️ SANS CE CRITERE, « rien ne s'est ouvert » serait ambigu : le monteur
		//    pourrait ne JAMAIS appeler le crochet et rendre le meme resultat.
		CheckEqU(h.demandes, 1u, "(b16) LE CROCHET EST APPELE, avec le nom du document");
		Check(h.dernierNom.Compare(NkString("menu.scene")) == 0,
			  "(b16) et c'est bien le nom du `ContextMenu`, pas un autre");
		CheckEqU(s.rap.menusContextuelsOuverts, 0u,
				 "(b16) [hote ferme] il a dit non, donc rien ne s'ouvre");
		s.exe.Debrancher(s.ctx);
	}

	// ── L'HOTE OUVRE : le menu apparait, AVEC SES ENTREES ──────────────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b16) [hote ouvre] le document se charge");
		HoteMenu h;
		h.ouvrir = true;
		// La reference se prend SANS hote, sur la meme scene : c'est l'image a
		// laquelle le menu doit ajouter des pixels.
		s.Image();
		const uint32 peintAvant = ComptePixelsPeints(s.ras, kFond);
		// Deux images avec l'hote : NKGui garde son popup d'une image sur l'autre,
		// et c'est la SECONDE qui le dessine — le survol de ce banc a la meme
		// latence, pour la meme raison.
		s.Image(&h);
		s.Image(&h);
		const uint32 peintApres = ComptePixelsPeints(s.ras, kFond);
		printf("        [hote ouvre] ouverts = %u, sansHote = %u, itemsHorsMenu = %u ; "
			   "pixels %u -> %u\n",
			   s.rap.menusContextuelsOuverts, s.rap.menusContextuelsSansHote,
			   s.rap.elementsMenuHorsMenu, peintAvant, peintApres);
		CheckEqU(s.rap.menusContextuelsOuverts, 1u, "(b16) [hote ouvre] LE MENU S'OUVRE");
		CheckEqU(s.rap.menusContextuelsSansHote, 0u,
				 "(b16) et il n'est plus compte comme sans hote");
		// Les entrees sont DANS le popup : elles ne sont plus « hors menu ».
		CheckEqU(s.rap.elementsMenuHorsMenu, 0u,
				 "(b16) LES TROIS ENTREES SONT DANS LE MENU, plus hors de lui");
		// ⚠️ ET LE CRITERE QUI COMPTE EST EN PIXELS. Trois compteurs justes ne
		//    disent pas qu'un menu est APPARU : le popup vit dans la surimpression,
		//    et ce banc ne la rasterisait meme pas avant ce matin.
		Check(peintApres > peintAvant + 300u,
			  "(b16) ET IL EST PEINT — l'image gagne plus de 300 pixels");
		s.Png("b16_menu_contextuel.png");
		s.exe.Debrancher(s.ctx);
	}
}
}

static void CasCourbe() {
// =====================================================================
printf("\n-- (b17) P7 `CurveField` : une courbe qu'on TIRE, pas seulement qu'on voit\n");
// =====================================================================
// Doc 09 §P7 : « la vitesse le long d'une trajectoire, la part de physique dans le
// temps, l'intensite d'un vent — une courbe editable EN PLACE. `Chart` n'est pas
// editable. »
//
// ⚠️ C'EST DONC L'EDITION QUI EST LE CRITERE, PAS LE DESSIN. Un champ de courbe qui
//    se peint joliment et qu'on ne peut pas toucher serait un `Chart` sous un autre
//    nom : il ferait monter `courbes`, il aurait ses pixels, et il ne rendrait
//    AUCUN des services que ce role existe pour rendre.
{
	static const char kDoc[] =
		"nkgui 0.3\n"
		"widgets {\n"
		"  CurveField \"vitesse\" {\n"
		"    bind = \"traj.vitesse\"\n"
		"    min = 0\n"
		"    max = 200\n"
		"    height = 90\n"
		"    xLabel = \"avancement\"\n"
		"    yLabel = \"km/h\"\n"
		"  }\n"
		"}\n";
	// L'hote possede les points — c'est le partage que ce role a choisi. Il note
	// aussi ce qu'on lui renvoie : sans cela, « le point a bouge a l'ecran » ne
	// dirait pas que l'application l'a SU.
	struct HoteCourbe : public NkGuiMonteHooks {
			NkVec2 pts[4] = {{0.f, 20.f}, {0.33f, 120.f}, {0.66f, 60.f}, {1.f, 180.f}};
			bool servir = true;
			uint32 lectures = 0u;
			uint32 notifications = 0u;
			NkString dernierBind;
			NkVec2 recus[4];
			uint32 nRecus = 0u;
			bool CourbeLiee(const char *bind, NkVec2 *out, uint32 maxP,
							uint32 &nOut) noexcept override {
				++lectures;
				dernierBind = NkString(bind);
				if (!servir || maxP < 4u)
					return false;
				for (uint32 i = 0; i < 4u; ++i)
					out[i] = pts[i];
				nOut = 4u;
				return true;
			}
			void CourbeModifiee(const char *bind, const NkVec2 *p,
								uint32 n) noexcept override {
				++notifications;
				dernierBind = NkString(bind);
				nRecus = n < 4u ? n : 4u;
				for (uint32 i = 0; i < nRecus; ++i) {
					recus[i] = p[i];
					pts[i] = p[i]; // l'hote garde ce qu'on lui rend
				}
			}
	};

	// ── SANS HOTE : le role existe, et l'ABSENCE de courbe se voit ──────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 160),
			  "(b17) le document a `CurveField` se charge");
		s.Image();
		printf("        [sans hote] inconnus = %u, courbes = %u, sansHote = %u\n",
			   s.rap.rolesInconnus, s.rap.courbes, s.rap.courbesSansHote);
		CheckEqU(s.rap.rolesInconnus, 0u, "(b17) `CurveField` n'est plus un role inconnu");
		CheckEqU(s.rap.courbes, 1u, "(b17) la courbe est rencontree");
		// ⚠️ ET ELLE N'INVENTE PAS DE DROITE DE REPLI. Une ligne plate tracee faute
		//    de donnees aurait ete indiscernable d'une vraie courbe plate — un
		//    reglage faux qui a l'air juste. Meme politique qu'une zone `Host`.
		CheckEqU(s.rap.courbesSansHote, 1u,
				 "(b17) [sans hote] comptee SANS HOTE — aucune courbe inventee");
		s.exe.Debrancher(s.ctx);
	}

	// ── AVEC HOTE : elle est LUE et PEINTE ──────────────────────────────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 160),
			  "(b17) [avec hote] le document se charge");
		HoteCourbe h;
		// La reference sans hote, sur la meme scene : c'est l'image a laquelle la
		// courbe doit ajouter ses pixels.
		s.Image();
		const uint32 sansCourbe = ComptePixelsPeints(s.ras, kFond);
		s.Image(&h);
		const uint32 avecCourbe = ComptePixelsPeints(s.ras, kFond);
		const uint32 accent = Empaquete(NkGuiContext().theme.accent);
		const uint32 pxAccent = ComptePixelsCouleur(s.ras, accent);
		printf("        [avec hote] lectures = %u (« %s »), sansHote = %u ; pixels %u -> %u, "
			   "accent = %u\n",
			   h.lectures, h.dernierBind.CStr(), s.rap.courbesSansHote, sansCourbe,
			   avecCourbe, pxAccent);
		CheckEqU(h.lectures, 1u, "(b17) LE CROCHET EST LU, une fois par image");
		Check(h.dernierBind.Compare(NkString("traj.vitesse")) == 0,
			  "(b17) et avec le `bind` du document, pas l'identifiant du widget");
		CheckEqU(s.rap.courbesSansHote, 0u, "(b17) [avec hote] elle n'est plus sans hote");
		// 🔴 LE CRITERE EST EN PIXELS DE LA COULEUR D'ACCENT, ET LE TOTAL AURAIT
		//    RENDU L'INVERSE DE LA VERITE. Mesure : 27 000 pixels non-fond SANS la
		//    courbe, 26 980 AVEC — il DESCEND de vingt. Parce que sans hote le champ
		//    se couvre de hachures (le marqueur « personne ne sert ceci »), qui
		//    peignent plus de pixels que la courbe elle-meme. Un critere « l'image
		//    gagne des pixels » aurait donc declare la courbe absente alors qu'elle
		//    est la, et un critere « l'image a change » l'aurait declaree presente
		//    quoi qu'on dessine. Il faut NOMMER la couleur cherchee — quatrieme fois
		//    aujourd'hui qu'un total ne tranche pas.
		Check(pxAccent > 100u, "(b17) LA COURBE EST TRACEE — la couleur d'accent est presente");
		s.Png("b17_courbe.png");
		s.exe.Debrancher(s.ctx);
	}

	// ── LE GESTE : ON TIRE UN POINT, ET L'HOTE L'APPREND ────────────────
	{
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 160),
			  "(b17) [geste] le document se charge");
		HoteCourbe h;
		s.Image(&h);
		// Le rectangle du champ sort du MONTAGE : aucune coordonnee de ce banc n'est
		// ecrite en dur — un attendu en dur se perime quand la mise en page bouge.
		NkRect cadre{0.f, 0.f, 0.f, 0.f};
		const bool aCadre = s.Rect("vitesse", cadre);
		Check(aCadre, "(b17) le rectangle du champ sort du montage");
		if (aCadre) {
			// Le deuxieme point : x = 0,33 de la largeur utile (marge de 4 px), et sa
			// valeur 120 sur [0, 200].
			const float32 marge = 4.f;
			const float32 x0 = cadre.x + marge, larg = cadre.w - 2.f * marge;
			const float32 y0 = cadre.y + marge, hUtile = cadre.h - 2.f * marge;
			const float32 px = x0 + larg * 0.33f;
			const float32 py = y0 + hUtile * (1.f - 120.f / 200.f);
			const float32 avant = h.pts[1].y;
			printf("        point 1 avant = %.1f, poignee a (%.0f, %.0f)\n", avant, px, py);

			// Survoler d'abord : la poignee doit REPONDRE au survol avant qu'on
			// l'appuie. Ce banc a une image de retard sur le survol (hotIdPrev).
			s.exe.PoserPointeur(s.ctx, px, py);
			s.Image(&h);
			s.Image(&h);
			const uint32 blanc = Empaquete(NkGuiContext().theme.onAccent);
			const uint32 pxSurvol = ComptePixelsCouleur(s.ras, blanc);
			printf("        pixels blancs au survol = %u\n", pxSurvol);
			Check(pxSurvol > 10u,
				  "(b17) LA POIGNEE REPOND AU SURVOL — elle passe au blanc du theme");

			// Puis appuyer et tirer VERS LE HAUT (y decroit a l'ecran = valeur monte).
			// 🔴 LE COMPTEUR SE CUMULE, ET C'EST UNE LECON PAYEE ICI MEME. `rap` est
			//    remis a zero a CHAQUE image : lire `pointsCourbeDeplaces` apres la
			//    derniere rendait 0, parce qu'a cette image-la le point avait deja
			//    atteint sa cible et ne bougeait plus. Le code etait juste, le critere
			//    regardait la mauvaise image. *Un compteur par image ne se lit pas
			//    apres un geste qui dure plusieurs images.*
			uint32 deplacesVus = 0u;
			s.ctx.input.mouseDown[0] = true;
			s.ctx.input.mouseClicked[0] = true;
			s.Image(&h);
			deplacesVus += s.rap.pointsCourbeDeplaces;
			s.ctx.input.mouseClicked[0] = false;
			const float32 cible = y0 + hUtile * (1.f - 180.f / 200.f);
			s.exe.PoserPointeur(s.ctx, px, cible);
			s.ctx.input.mouseDown[0] = true;
			s.Image(&h);
			deplacesVus += s.rap.pointsCourbeDeplaces;
			s.Image(&h);
			deplacesVus += s.rap.pointsCourbeDeplaces;
			printf("        point 1 apres = %.1f (cible ~180), notifications = %u, "
				   "deplaces (cumul) = %u\n",
				   h.pts[1].y, h.notifications, deplacesVus);
			s.ctx.input.mouseDown[0] = false;

			// 🔴 LES TROIS MOITIES DE « C'EST EDITABLE ». La valeur a change, l'hote
			//    l'a APPRISE, et le rapport le dit. Sans la deuxieme, un widget qui
			//    bougerait son point sans prevenir personne passerait : ce serait un
			//    reglage que l'utilisateur croit avoir fait.
			Check(h.pts[1].y > avant + 30.f,
				  "(b17) LE POINT A ETE TIRE — sa valeur a monte de plus de 30");
			Check(h.notifications > 0u, "(b17) ET L'HOTE L'A APPRIS — `CourbeModifiee` appelee");
			Check(deplacesVus > 0u, "(b17) et le rapport le dit");
			CheckEqF(h.pts[1].y, 180.f, 6.f,
					 "(b17) la valeur suit la SOURIS, pas un pas arbitraire");
			// Les voisins n'ont pas bouge : un geste qui deplacerait toute la courbe
			// passerait les criteres ci-dessus.
			CheckEqF(h.pts[0].y, 20.f, 0.5f, "(b17) le point 0 n'a PAS bouge");
			CheckEqF(h.pts[2].y, 60.f, 0.5f, "(b17) ni le point 2");
			s.Png("b17_courbe_tiree.png");
		}
		s.exe.Debrancher(s.ctx);
	}
}
}

static void CasPlateformes() {
// =====================================================================
printf("\n-- (b18) P27 : UN document, TROIS plateformes — et l'apercu depuis le PC\n");
// =====================================================================
// Rodolf, 27/09 : « le systeme doit pouvoir avoir des design specifiques par
// plateforme — web, mobile et PC, vu que Nkentseu est multiplateforme […] et comme
// en plus le responsive est defini, ca pourrait encore apporter plus. »
//
// ⚠️ LA SELECTION EST A L'EXECUTION, PAS A LA COMPILATION, ET CE CAS EST CE QUI LE
//    PROUVE : il monte le design MOBILE alors qu'il tourne sur un PC. Une
//    compilation conditionnelle aurait rendu cette mesure impossible — et, plus
//    grave, aurait empeche NKUIDesign de MONTRER le design mobile sans recompiler.
{
	static const char kDoc[] =
		"nkgui 0.3\n"
		"widgets {\n"
		"  Window \"racine\" {\n"
		"    VBox \"pile\" {\n"
		"      Panel \"commun\" { size = (200, 30)\n"
		"        appearance { fill { color = #1A7F37 } }\n"
		"      }\n"
		"      Panel \"colonne_pc\" { platform = Bureau, size = (200, 30)\n"
		"        appearance { fill { color = #8250DF } }\n"
		"      }\n"
		"      Panel \"barre_tel\" { platform = Mobile, size = (200, 30)\n"
		"        appearance { fill { color = #CF222E } }\n"
		"      }\n"
		"      Panel \"ecran_tactile\" { platform = [Mobile, Web], size = (200, 30)\n"
		"        appearance { fill { color = #0969DA } }\n"
		"      }\n"
		"      Panel \"faute\" { platform = Mobil, size = (200, 30)\n"
		"        appearance { fill { color = #E0B05A } }\n"
		"      }\n"
		"    }\n"
		"  }\n"
		"}\n";
	const uint32 kCommun = 0x1A7F37FFu, kPC = 0x8250DFFFu, kTel = 0xCF222EFFu,
				 kTactile = 0x0969DAFFu, kFaute = 0xE0B05AFFu;

	struct Attendu {
			NkGuiCible cible;
			const char *nom;
			bool pc, tel, tactile;
	};
	// Le meme document, lu par trois cibles. Ce que chacune doit voir est ecrit
	// AVANT la mesure — un attendu calcule apres coup n'est pas un attendu.
	const Attendu kCas[] = {
		{NkGuiCible::Bureau, "Bureau", true, false, false},
		{NkGuiCible::Mobile, "Mobile", false, true, true},
		{NkGuiCible::Web, "Web", false, false, true},
	};

	for (uint32 c = 0; c < 3u; ++c) {
		NkGuiPoserCible(kCas[c].cible);
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 220),
			  "(b18) le document multiplateforme se charge");
		s.Image();
		s.Image();
		const uint32 nCommun = ComptePixelsCouleur(s.ras, kCommun);
		const uint32 nPC = ComptePixelsCouleur(s.ras, kPC);
		const uint32 nTel = ComptePixelsCouleur(s.ras, kTel);
		const uint32 nTactile = ComptePixelsCouleur(s.ras, kTactile);
		const uint32 nFaute = ComptePixelsCouleur(s.ras, kFaute);
		printf("        [%-7s] commun=%u pc=%u tel=%u tactile=%u faute=%u | ecartes=%u "
			   "inconnues=%u\n",
			   kCas[c].nom, nCommun, nPC, nTel, nTactile, nFaute,
			   s.rap.ecartesParPlateforme, s.rap.ciblesInconnues);
		// Le panneau sans `platform` est la sur les trois : c'est le defaut, et sans
		// lui on ne saurait pas distinguer « ecarte » de « jamais monte ».
		Check(nCommun > 3000u, "(b18) le panneau SANS `platform` est la");
		Check((nPC > 3000u) == kCas[c].pc, "(b18) la colonne PC n'est la QUE sur Bureau");
		Check((nTel > 3000u) == kCas[c].tel, "(b18) la barre mobile QUE sur Mobile");
		// ⚠️ LA LISTE EST LE CAS QUI SEPARE UN ATTRIBUT D'UN QUALIFICATEUR. Un
		//    `Panel(Mobile)` n'aurait pu nommer qu'UNE cible ; `platform =
		//    [Mobile, Web]` en nomme deux, et c'est ce que « web ET mobile » demande.
		Check((nTactile > 3000u) == kCas[c].tactile,
			  "(b18) l'ecran tactile sur Mobile ET Web — la LISTE est lue");
		// 🔴 LA FAUTE DE FRAPPE NE FAIT PAS DISPARAITRE UN PANNEAU. `platform =
		//    Mobil` est monte PARTOUT et COMPTE : ecarter sur une faute aurait donne
		//    un document juste qui perd un panneau sans qu'aucun chiffre ne le dise.
		Check(nFaute > 3000u,
			  "(b18) `platform = Mobil` (faute) est MONTE — on n'ecarte pas sur une faute");
		CheckEqU(s.rap.ciblesInconnues, 1u, "(b18) et la faute est COMPTEE, pas avalee");
		// Le compte des ecartes se derive de l'attendu, pas releve puis recopie.
		const uint32 attendus = (kCas[c].pc ? 0u : 1u) + (kCas[c].tel ? 0u : 1u)
								+ (kCas[c].tactile ? 0u : 1u);
		CheckEqU(s.rap.ecartesParPlateforme, attendus,
				 "(b18) LE COMPTE DES ECARTES est celui qu'on a calcule d'avance");
		s.exe.Debrancher(s.ctx);
	}

	// ── LE PIRE DES SILENCES : un document juste qui n'affiche rien ─────
	{
		static const char kVide[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Window \"racine\" {\n"
			"    Panel \"tout_mobile\" { platform = Mobile, size = (200, 30)\n"
			"      appearance { fill { color = #CF222E } }\n"
			"    }\n"
			"  }\n"
			"}\n";
		NkGuiPoserCible(NkGuiCible::Bureau);
		Scene s;
		Check(s.Charger(kVide, (uint32)(sizeof(kVide) - 1u), 320, 120),
			  "(b18) [vide] le document se charge");
		s.Image();
		printf("        [vide sur Bureau] refus = %u, ecartes = %u, montes = %u\n",
			   s.rap.rolesInconnus, s.rap.ecartesParPlateforme, s.rap.montes);
		// ⚠️ C'EST LE CAS QUI JUSTIFIE LE COMPTEUR. Rien n'est invalide : le
		//    document est juste, il ne s'affiche simplement pas sur cette cible.
		//    Aucun message d'erreur n'existera jamais pour ca. Sans
		//    `ecartesParPlateforme` au verdict, « mon interface a disparu » se
		//    cherche a la main.
		CheckEqU(s.rap.rolesInconnus, 0u, "(b18) [vide] AUCUNE erreur — le document est juste");
		CheckEqU(s.rap.ecartesParPlateforme, 1u,
				 "(b18) ET POURTANT le panneau a disparu — seul ce compteur le dit");
		s.exe.Debrancher(s.ctx);
	}

	// ── LES DISPOSITIONS PAR CIBLE : la colonne devient une barre ───────
	{
		static const char kDock[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  DockSpace \"espace\" {\n"
			"    Host \"outils\" {}\n"
			"    Host \"scene\" {}\n"
			"  }\n"
			"}\n"
			"layout \"defaut\" {\n"
			"  dock \"outils\" left 0.25\n"
			"  dock \"scene\" center\n"
			"}\n"
			"layout \"defaut\" {\n"
			"  platform = Mobile\n"
			"  dock \"outils\" bottom 0.2\n"
			"  dock \"scene\" center\n"
			"}\n";
		NkRect rOutils[2];
		const NkGuiCible cibles[2] = {NkGuiCible::Bureau, NkGuiCible::Mobile};
		const char *noms[2] = {"Bureau", "Mobile"};
		bool lus = true;
		for (uint32 k = 0; k < 2u; ++k) {
			NkGuiPoserCible(cibles[k]);
			Scene s;
			Check(s.Charger(kDock, (uint32)(sizeof(kDock) - 1u), 400, 300),
				  "(b18) [dock] le document a DEUX dispositions se charge");
			s.Image();
			if (!s.RectTout("outils", rOutils[k]))
				lus = false;
			printf("        [dock %-7s] dispositions = %u, propre = %s, outils = "
				   "(%.0f, %.0f, %.0f x %.0f)\n",
				   noms[k], s.etat.rapportDispositions.dispositions,
				   s.etat.rapportDispositions.Propre() ? "oui" : "NON", rOutils[k].x,
				   rOutils[k].y, rOutils[k].w, rOutils[k].h);
			// 🔴 CE CRITERE A UNE RAISON PRECISE D'EXISTER. La ligne
			//    `platform = Mobile` vit dans le MEME tableau que les lignes `dock`,
			//    en tranche brute : si le lecteur ne l'attrapait pas, elle tomberait
			//    dans l'analyseur de `dock` et la disposition serait declaree NON
			//    PROPRE pour avoir declare correctement sa cible.
			Check(s.etat.rapportDispositions.Propre(),
				  "(b18) [dock] les deux dispositions sont PROPRES — `platform` n'est pas pris pour un `dock`");
			CheckEqU(s.etat.rapportDispositions.dispositions, 2u,
					 "(b18) [dock] les deux `layout` de MEME NOM coexistent");
			s.exe.Debrancher(s.ctx);
		}
		Check(lus, "(b18) [dock] la zone `outils` a un rectangle dans les deux cas");
		if (lus) {
			// Sur Bureau : une COLONNE a gauche (haute et etroite, collee au bord).
			// Sur Mobile : une BARRE en bas (large et basse).
			// ⚠️ ET C'EST LA FORME QU'ON COMPARE, PAS UNE TAILLE. `sizeRel` sait
			//    changer une taille ; il ne sait pas deplacer un panneau d'un bord a
			//    l'autre. C'est precisement ce que la cible apporte en plus du
			//    responsif, et le critere doit porter sur ca.
			Check(rOutils[0].h > rOutils[0].w,
				  "(b18) [Bureau] `outils` est une COLONNE — plus haute que large");
			Check(rOutils[1].w > rOutils[1].h,
				  "(b18) [Mobile] c'est une BARRE — plus large que haute");
			Check(rOutils[1].y > rOutils[0].y,
				  "(b18) et elle est passee EN BAS — pas seulement redimensionnee");
		}
	}
	// ⚠️ ON REMET LA CIBLE. Elle est globale au processus : la laisser sur `Mobile`
	//    ferait mentir tous les cas qui suivent, et le defaut `Toutes` est celui
	//    qui rend la plateforme compilee.
	NkGuiPoserCible(NkGuiCible::Toutes);
}
}

int main(int argc, char **argv) {
	// ⚠️ CE CHIFFRE RESTE, PARCE QUE C'EST LUI QUI A TRANCHE. Le banc est mort une
	//    fois sur `0xC00000FD` en gagnant un cas : `main` portait plus de vingt-cinq
	//    `Scene` locales, et 37 064 octets chacune font ~900 Ko de cadre. Les cas
	//    recents vivent desormais dans leur propre fonction ; ce chiffre est ce qui
	//    rendra la prochaine croissance VISIBLE avant qu'elle ne coute une heure.
	printf("[cadre] sizeof(Scene) = %u octets — un cas par fonction au-dela de ~20\n",
		   (unsigned)sizeof(Scene));
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
	printf("\n-- (b1.g) L'ENCRE : `text { color }` sur un widget re-peint\n");
	// =====================================================================
	{
		// 🔴 CE QUE CE CAS A TROUVE, LE 26/09. `text { color }` etait LU par le
		//    format et honore par le seul `case Text` du monteur. Des qu'un widget
		//    declarait une apparence, c'est le crochet de style qui peignait -- et
		//    il ecrivait le libelle avec l'encre du THEME, quoi que le document
		//    demande. Le bouton orange de NkAnimaEditor reclamait #10222B et rendait
		//    **0 pixel** de sa couleur pour 2 420 pixels d'orange.
		//
		// ⚠️ LE CAS PORTE DEUX ENCRES, ET C'EST DELIBERE. Une seule encre, la meme
		//    au repos et au survol, passerait meme si l'etat ne la transmettait pas :
		//    le socle suffirait. Deux encres DIFFERENTES exigent que l'etat porte la
		//    sienne.
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Button \"ecrire\" {\n"
			"    label = \"Enregistrer\"\n"
			"    appearance { radius = 4, fill { color = #F79A28 }, text { color = #10222B } }\n"
			"    appearance(Hover) { fill { color = #FFB055 }, text { color = #7A2E00 } }\n"
			"  }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 120),
			  "(b1.g) le document a deux encres se charge");
		const NkGuiInfoWidget *w = s.exe.infos.Trouver(NkStringView("ecrire"));
		Check(w != nullptr, "(b1.g) le widget est lu");
		if (w) {
			Check(w->etats[(uint32)NkGuiEtatApp::Normal].aEncre,
				  "(b1.g) l'encre du REPOS est lue depuis le document");
			CheckEqU(Empaquete(w->etats[(uint32)NkGuiEtatApp::Normal].encre), 0x10222BFFu,
					 "(b1.g) ecrire/Normal encre = #10222B");
			CheckEqU(Empaquete(w->etats[(uint32)NkGuiEtatApp::Hover].encre), 0x7A2E00FFu,
					 "(b1.g) ecrire/Hover  encre = #7A2E00");
		}
		s.Image();
		s.Image();
		NkRect r{0.f, 0.f, 0.f, 0.f};
		const bool aR = s.Rect("ecrire", r);
		Check(aR, "(b1.g) son rectangle sort du montage");
		if (aR) {
			NkGuiContext ref; // le theme NU : c'est lui la rivale de l'encre du document
			const uint32 bord = Empaquete(ref.theme.border);
			const NkColor encreRepos{0x10, 0x22, 0x2B, 0xFF};
			const NkColor encreHover{0x7A, 0x2E, 0x00, 0xFF};

			// ── AU REPOS ──────────────────────────────────────────────────
			const uint32 auDoc =
				ComptePlusProchesDe(s.ras, r, encreRepos, ref.theme.text, 0xF79A28FFu, bord);
			const uint32 auTheme =
				ComptePlusProchesDe(s.ras, r, ref.theme.text, encreRepos, 0xF79A28FFu, bord);
			s.Png("g1_encre_repos.png");
			printf("        repos : %u px de libelle plus proches de l'encre du DOCUMENT, %u de "
				   "celle du THEME\n",
				   auDoc, auTheme);
			Check(auDoc > 0u, "(b1.g) le libelle est peint avec l'encre du DOCUMENT");
			Check(auDoc > auTheme,
				  "(b1.g) NEGATIF : et PAS avec celle du theme -- les deux sont comptees");

			// ── SURVOLE : l'encre CHANGE avec l'etat ──────────────────────
			s.PoserEtStabiliser(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
			CheckEqU(s.exe.peintsParEtat[(uint32)NkGuiEtatApp::Hover], 1u,
					 "(b1.g) le bouton est bien peint en Hover");
			const uint32 hoverDoc =
				ComptePlusProchesDe(s.ras, r, encreHover, encreRepos, 0xFFB055FFu, bord);
			const uint32 hoverRepos2 =
				ComptePlusProchesDe(s.ras, r, encreRepos, encreHover, 0xFFB055FFu, bord);
			s.Png("g2_encre_survol.png");
			printf("        survol : %u px de libelle plus proches de l'encre HOVER, %u de celle "
				   "du REPOS\n",
				   hoverDoc, hoverRepos2);
			Check(hoverDoc > 0u, "(b1.g) survole, le libelle prend l'encre de l'ETAT");
			Check(hoverDoc > hoverRepos2,
				  "(b1.g) NEGATIF : l'encre du repos n'a PAS survecu au survol");
		}
		s.exe.Debrancher(s.ctx);
	}

	// =====================================================================
	printf("\n-- (b1.i) L'ENCRE D'UN ETAT SEUL -- le chemin que (b1.g) ne separait pas\n");
	// =====================================================================
	{
		// 🔴 POURQUOI CE CAS EXISTE, ET IL EST NE D'UNE MUTATION VERTE. L'encre du
		//    document est posee par DEUX chemins : le monteur la met dans
		//    `theme.text` autour du widget (`EncreDuDocument`), et le crochet de
		//    style la choisit par etat. Tant que le REPOS en declare une, couper le
		//    second chemin ne change RIEN -- le premier la fournit quand meme.
		//    *Une propriete garantie deux fois est une propriete dont l'echec est
		//    masque*, et c'est exactement ce que (b1.g) seul laissait passer.
		//
		//    Ici le repos n'ecrit AUCUNE encre : le monteur n'a rien a poser, et
		//    seule la resolution d'etat peut donner sa couleur au libelle survole.
		//    Le chemin est isole, donc il est mesurable.
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Button \"ecrire\" {\n"
			"    label = \"Enregistrer\"\n"
			"    appearance { radius = 4, fill { color = #F79A28 } }\n"
			"    appearance(Hover) { fill { color = #FFB055 }, text { color = #7A2E00 } }\n"
			"  }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 120),
			  "(b1.i) le document a encre d'ETAT SEUL se charge");
		const NkGuiInfoWidget *w = s.exe.infos.Trouver(NkStringView("ecrire"));
		Check(w && !w->etats[(uint32)NkGuiEtatApp::Normal].aEncre,
			  "(b1.i) le REPOS ne declare aucune encre -- le monteur n'a rien a poser");
		s.Image();
		s.Image();
		NkRect r{0.f, 0.f, 0.f, 0.f};
		if (s.Rect("ecrire", r)) {
			NkGuiContext ref;
			const uint32 bord = Empaquete(ref.theme.border);
			const NkColor encreHover{0x7A, 0x2E, 0x00, 0xFF};
			// Au repos, le libelle doit etre celui du THEME : rien d'autre n'est
			// declare. Sans cette moitie, le cas ne saurait pas dire que l'encre
			// est ARRIVEE avec l'etat.
			const uint32 reposTheme =
				ComptePlusProchesDe(s.ras, r, ref.theme.text, encreHover, 0xF79A28FFu, bord);
			const uint32 reposDoc =
				ComptePlusProchesDe(s.ras, r, encreHover, ref.theme.text, 0xF79A28FFu, bord);
			printf("        repos  : %u px de libelle au THEME, %u a l'encre de l'etat\n",
				   reposTheme, reposDoc);
			Check(reposTheme > reposDoc,
				  "(b1.i) au repos le libelle porte l'encre du THEME (rien d'autre n'est ecrit)");

			s.PoserEtStabiliser(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
			CheckEqU(s.exe.peintsParEtat[(uint32)NkGuiEtatApp::Hover], 1u,
					 "(b1.i) le bouton est peint en Hover");
			const uint32 survolDoc =
				ComptePlusProchesDe(s.ras, r, encreHover, ref.theme.text, 0xFFB055FFu, bord);
			const uint32 survolTheme =
				ComptePlusProchesDe(s.ras, r, ref.theme.text, encreHover, 0xFFB055FFu, bord);
			printf("        survol : %u px de libelle a l'encre de l'ETAT, %u au THEME\n",
				   survolDoc, survolTheme);
			Check(survolDoc > 0u, "(b1.i) survole, le libelle prend l'encre declaree par l'ETAT");
			Check(survolDoc > survolTheme,
				  "(b1.i) NEGATIF : le theme ne peint plus le libelle -- l'etat a gagne");
		}
		s.exe.Debrancher(s.ctx);
	}

	// =====================================================================
	printf("\n-- (b1.h) L'ENCRE HERITE DU REPOS quand l'etat n'en parle pas\n");
	// =====================================================================
	{
		// ⚠️ SANS CE CAS, LE PRECEDENT SUFFIRAIT A JUSTIFIER UNE MAUVAISE REGLE.
		//    Un etat qui REMPLACE tout (au lieu de se poser sur le repos) passerait
		//    (b1.g) sans faute -- ses deux encres sont declarees. Ici `Hover` ne
		//    parle que du fond : le libelle doit garder l'encre du repos, sinon il
		//    retombe sur le theme et disparait au survol. C'est la meme lecon que
		//    `appearance(FocusVisible)` avait deja donnee pour le FOND.
		//
		// ⚠️ CE CAS MESURE LA PROPRIETE, PAS LE CHEMIN QUI LA TIENT -- et c'est une
		//    limite, pas un oubli. Deux chemins la garantissent : l'heritage dans
		//    `NkGuiPeintureEffective` et l'encre que le monteur pose dans
		//    `theme.text`. Couper l'un des deux laisse ce critere VERT (mesure du
		//    26/09). Les separer demanderait un document ou l'un des deux chemins
		//    voit l'encre et pas l'autre : les deux lisent le meme `text { color }`
		//    du meme document, donc un tel document n'existe pas.
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Button \"ecrire\" {\n"
			"    label = \"Enregistrer\"\n"
			"    appearance { radius = 4, fill { color = #F79A28 }, text { color = #10222B } }\n"
			"    appearance(Hover) { fill { color = #FFB055 } }\n"
			"  }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 120),
			  "(b1.h) le document a UNE seule encre se charge");
		s.Image();
		s.Image();
		NkRect r{0.f, 0.f, 0.f, 0.f};
		if (s.Rect("ecrire", r)) {
			NkGuiContext ref;
			const uint32 bord = Empaquete(ref.theme.border);
			const NkColor encreRepos{0x10, 0x22, 0x2B, 0xFF};
			s.PoserEtStabiliser(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
			CheckEqU(s.exe.peintsParEtat[(uint32)NkGuiEtatApp::Hover], 1u,
					 "(b1.h) le bouton est peint en Hover");
			Check(ComptePixelsCouleur(s.ras, 0xFFB055FFu) > 0u,
				  "(b1.h) le fond du survol est bien celui de l'etat (#FFB055)");
			const uint32 herite =
				ComptePlusProchesDe(s.ras, r, encreRepos, ref.theme.text, 0xFFB055FFu, bord);
			const uint32 auTheme =
				ComptePlusProchesDe(s.ras, r, ref.theme.text, encreRepos, 0xFFB055FFu, bord);
			printf("        survol sans encre d'etat : %u px de libelle a l'encre du REPOS, %u au "
				   "THEME\n",
				   herite, auTheme);
			Check(herite > 0u, "(b1.h) l'encre du repos SURVIT au survol -- elle est le socle");
			Check(herite > auTheme, "(b1.h) NEGATIF : le theme n'a pas repris la main");
		}
		s.exe.Debrancher(s.ctx);
	}

	// =====================================================================
	printf("\n-- (b1.j) L'ACCORDEON SE PLIE ET RESTE PLIE\n");
	// =====================================================================
	{
		// 🔴 CE QUE CE CAS A TROUVE, LE 26/09. Rodolf : « j'espere que on pourra
		//    facilement programmer des accordeon ». Le role `Expander` existait,
		//    montait, et repliait son contenu -- mais il appelait
		//    `SetNodeOpen(id, expanded_du_document)` A CHAQUE IMAGE. Le clic
		//    basculait bien l'etat de NKGui, et l'image SUIVANTE le remettait comme
		//    le document le demande. **L'accordeon se refermait sous le doigt.**
		//
		//    `NkGuiMonteEtat::Entree` porte `initialise` depuis sa creation, et son
		//    commentaire dit deja la regle : « chaque trame ecraserait ce que
		//    l'utilisateur a change ». Le `Splitter` l'applique. Le pliable ne
		//    l'appliquait pas. *Une regle ecrite dans le champ qui la porte n'est
		//    pas une regle appliquee.*
		//
		// ⚠️ LA MESURE PORTE SUR L'ENFANT, PAS SUR L'EN-TETE. Un accordeon ferme
		//    garde son en-tete : compter « des pixels dans la zone » resterait vert
		//    les deux fois. C'est le `Button` de l'interieur, avec SA couleur, qui
		//    dit si le contenu est deplie.
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Expander \"section\" {\n"
			"    label = \"Transport\"\n"
			"    expanded = true\n"
			"    Button \"dedans\" {\n"
			"      label = \"Jouer\"\n"
			"      appearance { fill { color = #1A7F37 } }\n"
			"    }\n"
			"  }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 300, 160),
			  "(b1.j) le document a accordeon se charge");
		s.Image();
		s.Image();
		s.Png("j1_accordeon_deplie.png");
		const uint32 deplie = ComptePixelsCouleur(s.ras, 0x1A7F37FFu);
		printf("        deplie (expanded = true) : %u px de l'enfant\n", deplie);
		Check(deplie > 0u, "(b1.j) l'accordeon ouvert par le document montre son contenu");

		// L'EN-TETE : son rectangle sort du releve, on ne le devine pas.
		NkRect rTete{0.f, 0.f, 0.f, 0.f};
		const bool aTete = s.RectTout("section", rTete);
		Check(aTete, "(b1.j) le rectangle de l'en-tete sort du montage");
		if (aTete) {
			// Un clic : pointeur pose (deux images pour `hotIdPrev`), puis
			// l'enfoncement, puis le relachement -- `mouseClicked` est une
			// TRANSITION, pas un etat.
			// ⚠️ LE RECTANGLE RELEVE EST CELUI DU BLOC ENTIER, PAS DE L'EN-TETE.
			//    Viser son milieu (`y + h/2`) tombe DANS le contenu deplie : premiere
			//    version de ce cas, le clic n'atteignait jamais la barre et les deux
			//    criteres rougissaient sur un correctif JUSTE. L'en-tete est la
			//    premiere rangee : `CollapsingHeader` lui donne `ctx.ItemHeight()`.
			const float32 cx = rTete.x + 12.f;
			const float32 cy = rTete.y + s.ctx.ItemHeight() * 0.5f;
			s.exe.PoserPointeur(s.ctx, cx, cy);
			s.Image();
			s.Image();
			s.exe.PoserBouton(s.ctx, 0, true);
			s.Image();
			s.exe.PoserBouton(s.ctx, 0, false);
			s.Image();
			const uint32 apresClic = ComptePixelsCouleur(s.ras, 0x1A7F37FFu);
			printf("        apres UN clic sur l'en-tete : %u px\n", apresClic);
			CheckEqU(apresClic, 0u, "(b1.j) LE CLIC PLIE l'accordeon");

			// ⚠️ ET LA MOITIE QUI MANQUAIT AVANT LE CORRECTIF : il doit RESTER plie.
			//    Le defaut ne se voyait pas a l'image du clic -- il se voyait a la
			//    suivante, quand le document reimposait son `expanded = true`.
			s.Image();
			s.Image();
			s.Image();
			s.Png("j2_accordeon_plie.png");
			const uint32 troisImagesPlusTard = ComptePixelsCouleur(s.ras, 0x1A7F37FFu);
			printf("        trois images plus tard : %u px\n", troisImagesPlusTard);
			CheckEqU(troisImagesPlusTard, 0u,
					 "(b1.j) IL RESTE PLIE -- le document donne l'etat INITIAL, pas un ordre");

			// Et il se rouvre : un pli qui ne se deplie plus serait l'autre panne.
			s.exe.PoserBouton(s.ctx, 0, true);
			s.Image();
			s.exe.PoserBouton(s.ctx, 0, false);
			s.Image();
			s.Image();
			const uint32 rouvert = ComptePixelsCouleur(s.ras, 0x1A7F37FFu);
			printf("        apres un second clic : %u px\n", rouvert);
			Check(rouvert > 0u, "(b1.j) un second clic le rouvre");
		}
		s.exe.Debrancher(s.ctx);
	}

	// =====================================================================
	printf("\n-- (b1.k) LA BOITE DE DIALOGUE SE TRAINE PAR SA BARRE DE TITRE\n");
	// =====================================================================
	{
		// Troisieme demande de Rodolf (26/09) : « des dialog box deplacable ».
		// Le voile modal existait, le titre se peignait -- et la fenetre etait
		// CLOUEE : `NoMove` etait COMPTE, jamais applique, faute de geste.
		//
		// ⚠️ LA MESURE EST LE RECTANGLE RELEVE, PAS DES PIXELS. Une fenetre
		//    deplacee de 40 px garde exactement les memes couleurs : un compteur
		//    de couleur serait vert avant comme apres. Ce qui bouge, c'est son
		//    ORIGINE, et le montage la donne.
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Window \"dialogue\" {\n"
			"    title = \"Exporter\"\n"
			"    pos = (40, 30)\n"
			"    size = (220, 120)\n"
			"    placement = absolute\n"
			"  }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 400, 240),
			  "(b1.k) le document a fenetre titree se charge");
		s.Image();
		s.Image();
		NkRect r0{0.f, 0.f, 0.f, 0.f};
		const bool a0 = s.RectTout("dialogue", r0);
		Check(a0, "(b1.k) le rectangle de la fenetre sort du montage");
		if (a0) {
			s.Png("k1_fenetre_avant.png");
			printf("        au repos : x=%.1f y=%.1f\n", (double)r0.x, (double)r0.y);
			CheckEqF(r0.x, 40.f, 0.6f, "(b1.k) elle est ou le DOCUMENT la met (x = 40)");
			CheckEqF(r0.y, 30.f, 0.6f, "(b1.k) ... et y = 30");

			// LE GESTE : appui sur la barre de titre, puis deux deplacements.
			// L'attendu est ECRIT AVANT, et DERIVE des deux pas, pas recopie.
			const float32 kDx1 = 25.f, kDy1 = 14.f, kDx2 = 12.f, kDy2 = 7.f;
			const float32 attX = 40.f + kDx1 + kDx2;
			const float32 attY = 30.f + kDy1 + kDy2;
			printf("        ATTENDU ECRIT AVANT : 40+%.0f+%.0f = %.0f ; 30+%.0f+%.0f = %.0f\n",
				   (double)kDx1, (double)kDx2, (double)attX, (double)kDy1, (double)kDy2,
				   (double)attY);
			float32 mx = r0.x + 30.f, my = r0.y + s.ctx.ItemHeight() * 0.5f;
			s.exe.PoserPointeur(s.ctx, mx, my);
			s.Image();
			s.Image();
			s.exe.PoserBouton(s.ctx, 0, true);
			s.Image(); // l'appui PREND la fenetre
			mx += kDx1;
			my += kDy1;
			s.exe.PoserPointeur(s.ctx, mx, my);
			s.Image();
			mx += kDx2;
			my += kDy2;
			s.exe.PoserPointeur(s.ctx, mx, my);
			s.Image();
			NkRect r1{0.f, 0.f, 0.f, 0.f};
			const bool a1 = s.RectTout("dialogue", r1);
			printf("        apres le geste : x=%.1f y=%.1f (images ayant vu un deplacement : %u)\n",
				   (double)r1.x, (double)r1.y, s.rap.fenetresDeplacees);
			Check(a1, "(b1.k) elle est toujours au releve");
			CheckEqF(r1.x, attX, 1.0f, "(b1.k) ELLE A SUIVI le curseur en x");
			CheckEqF(r1.y, attY, 1.0f, "(b1.k) ... et en y");

			// ⚠️ ET ELLE RESTE OU ON L'A LACHEE. Sans cette moitie, un document qui
			//    reimpose son `pos` a chaque image passerait le critere ci-dessus et
			//    ramenerait la fenetre des le relachement.
			s.exe.PoserBouton(s.ctx, 0, false);
			s.Image();
			s.Image();
			s.Image();
			NkRect r2{0.f, 0.f, 0.f, 0.f};
			s.RectTout("dialogue", r2);
			s.Png("k2_fenetre_apres.png");
			printf("        trois images apres le lacher : x=%.1f y=%.1f\n", (double)r2.x,
				   (double)r2.y);
			CheckEqF(r2.x, attX, 1.0f, "(b1.k) ELLE RESTE la ou on l'a lachee");
			CheckEqF(r2.y, attY, 1.0f, "(b1.k) ... en y aussi");
			CheckEqU(s.rap.fenetresDeplacees, 0u,
					 "(b1.k) NEGATIF : bouton relache -> plus aucune image ne la deplace");
		}
		s.exe.Debrancher(s.ctx);
	}

	// =====================================================================
	printf("\n-- (b1.l) CE QUI N'OFFRE AUCUNE PRISE NE SE DEPLACE PAS\n");
	// =====================================================================
	{
		// Deux negatifs dans un seul document, parce qu'ils disent deux choses
		// differentes : `NoMove` est un REFUS du document, l'absence de titre est
		// une absence de PRISE. Les confondre ferait passer un `NoMove` ignore
		// pour un comportement voulu.
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Window \"clouee\" {\n"
			"    title = \"Fixe\"\n"
			"    pos = (20, 20)\n"
			"    size = (150, 80)\n"
			"    placement = absolute\n"
			"    flags = NoMove\n"
			"  }\n"
			"  Window \"sansprise\" {\n"
			"    pos = (200, 20)\n"
			"    size = (150, 80)\n"
			"    placement = absolute\n"
			"  }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 400, 240),
			  "(b1.l) le document a deux fenetres fixes se charge");
		s.Image();
		s.Image();
		const char *noms[2] = {"clouee", "sansprise"};
		const char *pourquoi[2] = {"elle ecrit `NoMove`", "elle n'a pas de barre de titre"};
		for (uint32 k = 0; k < 2u; ++k) {
			NkRect r0{0.f, 0.f, 0.f, 0.f};
			if (!s.RectTout(noms[k], r0))
				continue;
			const float32 mx = r0.x + 30.f, my = r0.y + s.ctx.ItemHeight() * 0.5f;
			s.exe.PoserPointeur(s.ctx, mx, my);
			s.Image();
			s.Image();
			s.exe.PoserBouton(s.ctx, 0, true);
			s.Image();
			s.exe.PoserPointeur(s.ctx, mx + 40.f, my + 25.f);
			s.Image();
			s.exe.PoserBouton(s.ctx, 0, false);
			s.Image();
			NkRect r1{0.f, 0.f, 0.f, 0.f};
			s.RectTout(noms[k], r1);
			printf("        « %s » (%s) : x %.1f -> %.1f\n", noms[k], pourquoi[k], (double)r0.x,
				   (double)r1.x);
			CheckEqF(r1.x, r0.x, 0.6f, "(b1.l) elle n'a PAS bouge en x");
			CheckEqF(r1.y, r0.y, 0.6f, "(b1.l) ni en y");
		}
		CheckEqU(s.rap.fenetresDeplacees, 0u, "(b1.l) et AUCUNE image n'a vu un deplacement");
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

	// \u2550\u2550\u2550 (25/09) DockPruneEmpty : L'ELAGAGE DES FEUILLES VIDES \u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550
	//
	// Pourquoi ce banc existe : une disposition RELUE d'un fichier peut contenir des
	// feuilles auxquelles plus aucune fenetre n'est liee. Une feuille sans fenetre
	// reserve quand meme sa largeur -- a l'ecran, une colonne vide (mesuree dans
	// NKUIDesign le 25/09). `DockPruneEmpty` les collapse en appelant l'elagage qui
	// existait deja (`DockCollapseLeaf`), et qui n'etait pas declare.
	//
	// \u26a0\ufe0f LE NEGATIF EST LA MOITIE QUI COMPTE. Le risque de cette fonction n'est pas
	//    de ne rien elaguer : c'est d'elaguer une feuille QUI PORTE UNE FENETRE, et
	//    ca se paie en panneaux disparus. Un banc qui ne verifierait que le positif
	//    serait vert sur une fonction qui efface tout.
	{
		printf("\\n-- DockPruneEmpty (elagage des feuilles vides) --\\n");
		// Un arbre minimal, ecrit a la main : une separation, deux feuilles.
		//   0 (separation) -> 1 (feuille) et 2 (feuille)
		auto arbre = [](NkGuiContext &c, bool feuille1Pleine, bool feuille2Pleine) {
			c.dockNodes.Clear();
			NkGuiDockNode sep;
			sep.kind = 1;
			sep.vertical = false;
			sep.ratio = 0.5f;
			sep.child0 = 1;
			sep.child1 = 2;
			sep.parent = -1;
			NkGuiDockNode f1;
			f1.kind = 2;
			f1.parent = 0;
			if (feuille1Pleine) {
				f1.windows[0] = (NkGuiId)111;
				f1.winCount = 1;
			}
			NkGuiDockNode f2;
			f2.kind = 2;
			f2.parent = 0;
			if (feuille2Pleine) {
				f2.windows[0] = (NkGuiId)222;
				f2.winCount = 1;
			}
			c.dockNodes.PushBack(sep);
			c.dockNodes.PushBack(f1);
			c.dockNodes.PushBack(f2);
			c.dockRoot = 0;
		};

		// (d1) POSITIF : une feuille vide a cote d'une feuille pleine part, et la
		//      pleine remonte a sa place. L'arbre passe de 3 noeuds a 3 noeuds dont
		//      la racine EST devenue la feuille pleine.
		{
			NkGuiContext c;
			arbre(c, true, false);
			const int32 n = DockPruneEmpty(c);
			CheckEqU((uint32)n, 1u, "(d1) une feuille vide est elaguee, et une seule");
			Check(c.dockNodes[0].kind == 2, "(d1) la racine est devenue la feuille survivante");
			CheckEqU((uint32)c.dockNodes[0].winCount, 1u, "(d1) elle porte toujours SA fenetre");
			Check(c.dockNodes[0].windows[0] == (NkGuiId)111,
				  "(d1) et c'est bien la MEME fenetre -- pas une feuille reconstruite");
		}

		// (d2) NEGATIF, ET C'EST LE CRITERE QUI PROTEGE LES FENETRES : deux feuilles
		//      PLEINES. Rien ne doit bouger. Si cette ligne rougit, la fonction fait
		//      disparaitre des panneaux.
		{
			NkGuiContext c;
			arbre(c, true, true);
			const uint32 avant = (uint32)c.dockNodes.Size();
			const int32 n = DockPruneEmpty(c);
			CheckEqU((uint32)n, 0u, "(d2) NEGATIF : aucune feuille pleine n'est elaguee");
			CheckEqU((uint32)c.dockNodes.Size(), avant, "(d2) l'arbre n'a pas change de taille");
			Check(c.dockNodes[0].kind == 1 && c.dockNodes[1].winCount == 1 &&
					  c.dockNodes[2].winCount == 1,
				  "(d2) la separation et ses DEUX fenetres sont intactes");
		}

		// (d3) LA RACINE VIDE SE GARDE. Un arbre d'une seule feuille, vide : il n'y a
		//      pas de frere a promouvoir, et collapser rendrait l'arbre inutilisable.
		{
			NkGuiContext c;
			c.dockNodes.Clear();
			NkGuiDockNode f;
			f.kind = 2;
			f.parent = -1;
			c.dockNodes.PushBack(f);
			c.dockRoot = 0;
			const int32 n = DockPruneEmpty(c);
			CheckEqU((uint32)n, 0u, "(d3) la racine vide n'est PAS elaguee (aucun frere a promouvoir)");
			CheckEqU((uint32)c.dockNodes.Size(), 1u, "(d3) et l'arbre existe toujours");
		}

		// (d4) DEUX VIDES : les deux partent, et l'appel se termine. Le garde-fou de
		//      256 passes n'est pas une excuse pour une boucle sans fin -- on verifie
		//      que le compte est celui des feuilles vides, pas le plafond.
		{
			NkGuiContext c;
			arbre(c, false, false);
			const int32 n = DockPruneEmpty(c);
			Check(n >= 1 && n <= 2, "(d4) deux feuilles vides : 1 ou 2 elagages, jamais le plafond");
			Check(c.dockNodes[0].winCount == 0, "(d4) ce qui reste ne porte aucune fenetre inventee");
		}
	}
	// =====================================================================
	printf("\n-- (b5) LE BLUEPRINT, LOT 1 : `!=`, `not`, les fonctions, les champs\n");
	// =====================================================================
	//  Rodolf, 27/09 : « le blueprint doit etre fait entierement ». Voici sa
	//  premiere moitie : ce qu'une EXPRESSION sait dire.
	//
	//  ⚠️ CHAQUE AJOUT EST MESURE PAR SON EFFET, PAS PAR SA PRESENCE. Un
	//     comportement ecrit `set r = ...` ; ce qui se verifie, c'est la VALEUR de
	//     `r` apres coup. Un analyseur qui accepte `!=` sans le calculer passerait
	//     un controle qui s'arreterait a « ca compile ».
	{
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  TextField \"nom\"      { value = \"Rodolf\" }\n"
			"  TextField \"vide\"     { value = \"\" }\n"
			"  Checkbox  \"conditions\" { value = true }\n"
			"  Button    \"cache\"    { label = \"Cache\", visible = false }\n"
			"}\n"
			"behavior \"essai\" {\n"
			"  set different   = 3 != 4\n"
			"  set identique   = 3 != 3\n"
			"  set negation    = not (1 == 1)\n"
			"  set negation2   = !(1 == 1)\n"
			"  set nonEgalNie  = not 3 != 4\n"
			"  set videOui     = empty(vide.text)\n"
			"  set videNon     = empty(nom.text)\n"
			"  set longueur    = length(nom.text)\n"
			"  set contientOui = contains(nom.text, \"dol\")\n"
			"  set contientNon = contains(nom.text, \"zzz\")\n"
			"  set egalOui     = matches(nom.text, \"Rodolf\")\n"
			"  set egalNon     = matches(nom.text, \"rodolf\")\n"
			"  set coche       = conditions.checked\n"
			"  set visibleNon  = cache.visible\n"
			"  set actifOui    = nom.enabled\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 240), "(b5) le document se charge");
		s.Image();
		s.Image();

		// ⚠️ LE WIDGET CACHE NE SE MONTE PAS, et c'est mesurable : quatre widgets
		//    ecrits, trois montes. `visible` etait au vocabulaire depuis toujours
		//    et personne ne le lisait.
		printf("        widgets=%u montes=%u invisibles=%u\n", s.rap.widgets, s.rap.montes,
			   s.rap.invisibles);
		CheckEqU(s.rap.invisibles, 1u, "(b5) `visible = false` RETIRE le widget du montage");
		CheckEqU(s.rap.montes, 3u, "(b5) trois widgets montes sur quatre ecrits");

		struct Attendu {
				const char *nom;
				float32 valeur;
				const char *quoi;
		};
		// ⚠️ LES ATTENDUS SONT ECRITS AVANT LA MESURE, et chacun porte son
		//    CONTRAIRE : `3 != 4` vrai ET `3 != 3` faux. Un seul des deux serait
		//    vert sur un `!=` qui rendrait toujours vrai.
		static const Attendu kAttendus[] = {
			{"different", 1.f, "3 != 4 est VRAI"},
			{"identique", 0.f, "3 != 3 est FAUX"},
			{"negation", 0.f, "not (1 == 1) est FAUX"},
			{"negation2", 0.f, "!(1 == 1) est FAUX — `!` et `not` sont la meme chose"},
			// ⚠️ CELUI-CI JUGE LA PRIORITE, et c'est le seul qui la juge : `not`
			//    porte sur la COMPARAISON, pas sur `3`. Si `not` etait dans
			//    `Primaire`, on lirait `(not 3) != 4` — donc `false != 4`, donc
			//    VRAI, et le contraire du sens ecrit.
			{"nonEgalNie", 0.f, "not 3 != 4 est FAUX — `not` porte sur la COMPARAISON"},
			{"videOui", 1.f, "empty(\"\") est VRAI"},
			{"videNon", 0.f, "empty(\"Rodolf\") est FAUX"},
			{"longueur", 6.f, "length(\"Rodolf\") vaut 6"},
			{"contientOui", 1.f, "contains(\"Rodolf\", \"dol\") est VRAI"},
			{"contientNon", 0.f, "contains(\"Rodolf\", \"zzz\") est FAUX"},
			{"egalOui", 1.f, "matches(\"Rodolf\", \"Rodolf\") est VRAI"},
			{"egalNon", 0.f, "matches est une egalite EXACTE : la casse compte"},
			{"coche", 1.f, "`.checked` lit la case"},
			{"visibleNon", 0.f, "`.visible` lit l'etat, et le widget est cache"},
			{"actifOui", 1.f, "`.enabled` lit l'etat, et le champ est actif"},
		};
		for (uint32 k = 0; k < (uint32)(sizeof(kAttendus) / sizeof(Attendu)); ++k) {
			NkGuiValeur val;
			const bool lu = s.exe.eval.Variable(NkStringView(kAttendus[k].nom), val);
			const float32 v = lu ? val.EnNombre() : -1.f;
			char titre[160];
			Joindre(titre, sizeof(titre), "(b5) ", kAttendus[k].quoi);
			if (!lu) {
				Check(false, titre);
				continue;
			}
			CheckEqF(v, kAttendus[k].valeur, 0.001f, titre);
		}
		printf("        expressions refusees : %u\n", s.exe.eval.rapport.refusees);
		CheckEqU(s.exe.eval.rapport.refusees, 0u,
				 "(b5) et AUCUNE expression refusee — tout ce qui est ecrit est compris");
		s.exe.Debrancher(s.ctx);
	}

	// =====================================================================
	printf("\n-- (b6) LE BLUEPRINT, LOT 2 : les instructions d'interface (P25)\n");
	// =====================================================================
	//  « Un comportement sait calculer et appeler l'application ; il ne sait pas
	//  encore AGIR sur l'interface. » C'est le chantier O du document 19.
	//
	//  ⚠️ LE DOCUMENT DE CE CAS EST CELUI DE LA SPECIFICATION, mot pour mot
	//     (doc 2 §5.5) : la condition EN CONTINU qui active ou desactive un
	//     bouton selon l'etat d'un autre. Un cas invente aurait prouve que le
	//     code marche sur ce que le code sait faire.
	{
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Checkbox  \"conditions\" { label = \"J'accepte\", value = false }\n"
			"  TextField \"email\"      { value = \"\" }\n"
			"  Button    \"inscription.suivant\" { label = \"Suivant\" }\n"
			"  Text      \"nom_erreur\" { text = \"\" }\n"
			"}\n"
			"behavior \"inscription.suivant\" {\n"
			"  if conditions.checked && not empty(email.text) {\n"
			"    enable \"inscription.suivant\"\n"
			"    hide \"nom_erreur\"\n"
			"  } else {\n"
			"    disable \"inscription.suivant\" because \"Accepte les conditions\"\n"
			"    set \"nom_erreur\".text = \"Le nom est obligatoire\"\n"
			"    show \"nom_erreur\"\n"
			"  }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 360, 240), "(b6) le document se charge");
		s.Image();
		s.Image();

		printf("        reconnues=%u servies=%u sansHote=%u refusees=%u\n",
			   s.exe.eval.rapport.uiReconnues, s.exe.eval.rapport.uiServies,
			   s.exe.eval.rapport.uiSansHote, s.exe.eval.rapport.refusees);
		// ⚠️ LE PREMIER CRITERE EST QUE RIEN N'EST REFUSE : une instruction que
		//    l'evaluateur ne comprend pas se compte en `refusees`, et un zero dit
		//    que tous les mots du document ont ete lus.
		CheckEqU(s.exe.eval.rapport.refusees, 0u,
				 "(b6) AUCUNE instruction refusee — tout ce qui est ecrit est compris");
		Check(s.exe.eval.rapport.uiReconnues >= 3u, "(b6) les instructions sont RECONNUES");
		CheckEqU(s.exe.eval.rapport.uiSansHote, 0u, "(b6) et l'hote est branche");
		CheckEqU(s.exe.eval.rapport.uiServies, s.exe.eval.rapport.uiReconnues,
				 "(b6) TOUTES servies — reconnues et servies ne sont pas le meme chiffre");

		// ── LA BRANCHE FAUSSE : le bouton est desactive AVEC SA RAISON ───
		const NkGuiInfoWidget *bouton = s.exe.infos.Trouver(NkStringView("inscription.suivant"));
		Check(bouton != nullptr, "(b6) le bouton est au releve");
		if (bouton) {
			NkGuiMonteEtat::Entree *e =
				s.etat.Get(NkStringView(bouton->cle.Data(), (usize)bouton->cle.Size()));
			Check(e != nullptr, "(b6) son etat existe");
			if (e) {
				printf("        etat du bouton : actif=%d raison=\"%s\"\n", (int)e->actif,
					   e->raison);
				Check(!e->actif, "(b6) la condition est FAUSSE -> le bouton est DESACTIVE");
				// ⚠️ « Un element desactive dit pourquoi » (doc 3 §14quater). Sans
				//    ce critere, `disable` serait servi et la raison PERDUE — et
				//    l'utilisateur verrait un bouton gris sans explication.
				Check(e->raison[0] != '\0', "(b6) et il PORTE SA RAISON, pas seulement son gris");
			}
		}

		// ── `set x.text` et `show` ont agi sur un AUTRE widget ───────────
		const NkGuiInfoWidget *err = s.exe.infos.Trouver(NkStringView("nom_erreur"));
		if (err) {
			NkGuiMonteEtat::Entree *e =
				s.etat.Get(NkStringView(err->cle.Data(), (usize)err->cle.Size()));
			if (e) {
				printf("        message d'erreur : visible=%d texte=\"%s\"\n", (int)e->visible,
					   e->texte);
				Check(e->visible, "(b6) `show` a rendu le message visible");
				Check(e->texte[0] != '\0', "(b6) et `set x.text` a ecrit DANS UN AUTRE widget");
			}
		}

		// ── LA BRANCHE VRAIE : on coche, on saisit, le bouton revient ────
		//  ⚠️ C'EST LA MOITIE QUI MANQUERAIT LE PLUS. Un `disable` qui ne se leve
		//     jamais passerait tous les criteres ci-dessus : ils ne regardent
		//     qu'un seul etat du monde.
		if (bouton) {
			const NkGuiInfoWidget *cond = s.exe.infos.Trouver(NkStringView("conditions"));
			const NkGuiInfoWidget *mail = s.exe.infos.Trouver(NkStringView("email"));
			if (cond && mail) {
				NkGuiMonteEtat::Entree *ec =
					s.etat.Get(NkStringView(cond->cle.Data(), (usize)cond->cle.Size()));
				NkGuiMonteEtat::Entree *em =
					s.etat.Get(NkStringView(mail->cle.Data(), (usize)mail->cle.Size()));
				if (ec && em) {
					ec->b = true;
					em->texte[0] = 'a';
					em->texte[1] = '\0';
					s.Image();
					NkGuiMonteEtat::Entree *e =
						s.etat.Get(NkStringView(bouton->cle.Data(), (usize)bouton->cle.Size()));
					printf("        apres avoir coche et saisi : actif=%d\n", (int)(e && e->actif));
					Check(e && e->actif,
						  "(b6) la condition devient VRAIE -> `enable` le rend actif");
					const NkGuiInfoWidget *err2 = s.exe.infos.Trouver(NkStringView("nom_erreur"));
					if (err2) {
						NkGuiMonteEtat::Entree *ee =
							s.etat.Get(NkStringView(err2->cle.Data(), (usize)err2->cle.Size()));
						Check(ee && !ee->visible, "(b6) et `hide` a recache le message");
					}
				}
			}
		}
		s.exe.Debrancher(s.ctx);
	}

	// =====================================================================
	printf("\n-- (b7) CE QUE L'HOTE NE SAIT PAS FAIRE SE COMPTE, ET SE VOIT\n");
	// =====================================================================
	//  ⚠️ C'EST LE CRITERE QUI DONNE SA VALEUR AUX DEUX COMPTEURS. `open`,
	//     `toast`, `emit`, `call`, `theme`, `back` parlent de NAVIGATION, de
	//     DIALOGUES et de SERVICES — que cette couche ne possede pas. Elles sont
	//     RECONNUES et NON SERVIES, et **l'ecart entre les deux chiffres est
	//     exactement ce que l'application ne sait pas encore faire**. Sans lui,
	//     un document plein d'instructions muettes passerait pour un document
	//     qui marche.
	{
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Button \"projet.supprimer\" { label = \"Supprimer\" }\n"
			"}\n"
			"behavior \"essai\" {\n"
			"  open \"reglages\"\n"
			"  open \"confirmer\" as modal\n"
			"  close\n"
			"  back\n"
			"  toast \"Copie\"\n"
			"  theme \"Rihen UE5 Clair\"\n"
			"  emit \"projet.modifie\"()\n"
			"  after 1500 { hide \"bandeau\" }\n"
			"  call \"fichier.ouvrir\"() -> chemin { toast chemin }\n"
			"  message \"Supprimer ?\" \"Definitif.\" buttons [\"Supprimer\", \"Annuler\"] -> r { "
			"toast r }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 160), "(b7) le document se charge");
		s.Image();
		s.Image();
		printf("        reconnues=%u servies=%u refusees=%u\n", s.exe.eval.rapport.uiReconnues,
			   s.exe.eval.rapport.uiServies, s.exe.eval.rapport.refusees);
		CheckEqU(s.exe.eval.rapport.refusees, 0u,
				 "(b7) les DIX instructions sont LUES — aucune refusee par l'analyseur");
		CheckEqU(s.exe.eval.rapport.uiReconnues, 10u, "(b7) et toutes RECONNUES");
		CheckEqU(s.exe.eval.rapport.uiServies, 0u,
				 "(b7) NEGATIF : aucune SERVIE — cette couche n'a ni navigation ni services");
		s.exe.Debrancher(s.ctx);
	}

	// =====================================================================
	printf("\n-- (b8) LE BLUEPRINT NODAL : il se COMPILE vers le script\n");
	// =====================================================================
	//  Decision de Rodolf (doc 2 §6.2) : « le graphe est COMPILE vers la
	//  representation intermediaire du script et execute par LE MEME evaluateur.
	//  *Un Blueprint tourne dans l'application exactement comme le script
	//  equivalent* — pas d'interpreteur de graphe separe, donc pas de divergence
	//  possible. »
	//
	//  ⚠️ LE CRITERE QUI PORTE CETTE PHRASE EST LE DERNIER DE CE BLOC : le
	//     graphe et le script equivalent doivent produire le MEME etat. Tout le
	//     reste ne verifierait que « le graphe fait quelque chose ».
	//
	//  Le graphe est celui du §6.5, mot pour mot.
	{
		static const char kGraphe[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  TextField \"nom\"        { value = \"\" }\n"
			"  Text      \"nom_erreur\" { text = \"\" }\n"
			"}\n"
			"behavior \"fiche.enregistrer\"(Click) graph {\n"
			"  node n1 EventClick\n"
			"  node n2 GetWidgetValue { target = \"nom\", field = text }\n"
			"  node n3 IsEmpty        { value = n2.value }\n"
			"  node n4 Branch         { cond = n3.result }\n"
			"  node n5 SetWidgetProperty { target = \"nom_erreur\", prop = \"text\", value = "
			"\"Le nom est obligatoire\" }\n"
			"  node n6 Show           { target = \"nom_erreur\" }\n"
			"  node n7 Focus          { target = \"nom\" }\n"
			"  node n8 CallCallback   { name = \"fiche.enregistrer\" }\n"
			"  node n9 Hide           { target = \"nom_erreur\" }\n"
			"  wire n1.exec -> n4.exec\n"
			"  wire n4.true -> n5.exec -> n6.exec -> n7.exec\n"
			"  wire n4.false -> n8.exec -> n9.exec\n"
			"}\n";
		// LE MEME COMPORTEMENT, ECRIT EN SCRIPT. C'est la reference.
		static const char kScript[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  TextField \"nom\"        { value = \"\" }\n"
			"  Text      \"nom_erreur\" { text = \"\" }\n"
			"}\n"
			"behavior \"fiche.enregistrer\" {\n"
			"  if empty(\"nom\".text) {\n"
			"    set \"nom_erreur\".text = \"Le nom est obligatoire\"\n"
			"    show \"nom_erreur\"\n"
			"    focus \"nom\"\n"
			"  } else {\n"
			"    Callback \"fiche.enregistrer\"()\n"
			"    hide \"nom_erreur\"\n"
			"  }\n"
			"}\n";

		Scene sg;
		Check(sg.Charger(kGraphe, (uint32)(sizeof(kGraphe) - 1u), 320, 200),
			  "(b8) le document a graphe se charge");
		Recepteur recG;
		sg.exe.eval.rappel = &SurCallback;
		sg.exe.eval.rappelUser = &recG;
		sg.Image();
		sg.Image();

		const NkGuiRapportGraphe &rg = sg.exe.eval.rapport.graphe;
		printf("        graphes=%u noeuds=%u fils=%u compiles=%u refuses=%u\n", rg.graphes,
			   rg.noeuds, rg.fils, rg.compiles, rg.refuses);
		for (uint32 k = 0; k < (uint32)rg.causes.Size(); ++k)
			printf("        cause : %s\n", rg.causes[k].CStr());
		CheckEqU(rg.graphes, 1u, "(b8) le graphe est VU — il arrivait en tranche brute");
		CheckEqU(rg.noeuds, 9u, "(b8) ses NEUF noeuds sont lus");
		// `wire a -> b -> c` est une CHAINE de N references : N-1 liens.
		// Les trois lignes donnent 1 + 3 + 2 = SIX.
		// ⚠️ MON PREMIER ATTENDU DISAIT CINQ, et le commentaire juste au-dessus
		//    ecrivait deja « 1 + 3 + 2 = 6 ». L'arithmetique etait sous mes yeux ;
		//    c'est le banc qui l'a lue, pas moi.
		CheckEqU(rg.fils, 6u, "(b8) et ses SIX fils (une chaine de N donne N-1 liens)");
		CheckEqU(rg.refuses, 0u, "(b8) il COMPILE");
		CheckEqU(rg.compiles, 1u, "(b8) et il est compile UNE fois");
		CheckEqU(sg.exe.eval.rapport.grapheIgnore, 0u,
				 "(b8) il n'est plus IGNORE — c'etait l'etat d'avant");

		Scene ss;
		Check(ss.Charger(kScript, (uint32)(sizeof(kScript) - 1u), 320, 200),
			  "(b8) le document en script se charge");
		Recepteur recS;
		ss.exe.eval.rappel = &SurCallback;
		ss.exe.eval.rappelUser = &recS;
		ss.Image();
		ss.Image();

		// ── L'EQUIVALENCE, ETAT PAR ETAT ────────────────────────────────
		struct Etat {
				bool trouve = false;
				bool visible = false;
				NkString texte;
		};
		auto lire = [](Scene &s, const char *id) {
			Etat e;
			const NkGuiInfoWidget *i = s.exe.infos.Trouver(NkStringView(id));
			if (!i)
				return e;
			NkGuiMonteEtat::Entree *en =
				s.etat.Get(NkStringView(i->cle.Data(), (usize)i->cle.Size()));
			if (!en)
				return e;
			e.trouve = true;
			e.visible = en->visible;
			e.texte = NkString(en->texte);
			return e;
		};
		const Etat gErr = lire(sg, "nom_erreur");
		const Etat sErr = lire(ss, "nom_erreur");
		printf("        graphe : visible=%d texte=\"%s\"\n", (int)gErr.visible, gErr.texte.CStr());
		printf("        script : visible=%d texte=\"%s\"\n", (int)sErr.visible, sErr.texte.CStr());
		Check(gErr.trouve && sErr.trouve, "(b8) les deux documents ont leur widget");
		// ⚠️ D'ABORD L'EFFET, ENSUITE L'EGALITE. Deux etats identiques mais VIDES
		//    passeraient une egalite nue : le champ est vide, donc la branche
		//    VRAIE doit avoir ecrit le message et l'avoir montre.
		Check(gErr.visible, "(b8) le graphe a pris la branche VRAIE : le message est visible");
		Check(gErr.texte.Size() > 0u, "(b8) et son texte a ete ecrit");
		Check(gErr.visible == sErr.visible && gErr.texte.Compare(sErr.texte) == 0,
			  "(b8) GRAPHE ET SCRIPT DONNENT LE MEME ETAT — un seul moteur, une seule verite");
		CheckEqU(recG.appels, recS.appels, "(b8) et le meme nombre d'appels a l'application");

		// ── L'AUTRE BRANCHE, SUR LES DEUX ECRITURES ─────────────────────
		//  ⚠️ SANS ELLE, L'EQUIVALENCE NE PORTE QUE SUR UN CHEMIN. Les deux
		//     documents prennent la branche VRAIE parce que le champ est vide :
		//     un compilateur qui ne saurait traduire QUE cette branche passerait
		//     tous les criteres ci-dessus. On remplit le champ, et on exige que
		//     les DEUX ecritures basculent ensemble.
		const uint32 avantG = recG.appels, avantS = recS.appels;
		for (uint32 k = 0; k < 2u; ++k) {
			Scene &sc = (k == 0u) ? sg : ss;
			const NkGuiInfoWidget *i = sc.exe.infos.Trouver(NkStringView("nom"));
			if (!i)
				continue;
			NkGuiMonteEtat::Entree *e =
				sc.etat.Get(NkStringView(i->cle.Data(), (usize)i->cle.Size()));
			if (!e)
				continue;
			e->texte[0] = 'R';
			e->texte[1] = '\0';
			e->initialise = true;
		}
		sg.Image();
		ss.Image();
		const Etat gErr2 = lire(sg, "nom_erreur");
		const Etat sErr2 = lire(ss, "nom_erreur");
		printf("        champ rempli — graphe : visible=%d appels=+%u | script : visible=%d "
			   "appels=+%u\n",
			   (int)gErr2.visible, recG.appels - avantG, (int)sErr2.visible,
			   recS.appels - avantS);
		Check(!gErr2.visible, "(b8) branche FAUSSE : le graphe a recache le message");
		Check(recG.appels > avantG, "(b8) et il a APPELE l'application");
		Check(gErr2.visible == sErr2.visible && (recG.appels - avantG) == (recS.appels - avantS),
			  "(b8) LES DEUX BRANCHES sont equivalentes, pas seulement la premiere");
		sg.exe.Debrancher(sg.ctx);
		ss.exe.Debrancher(ss.ctx);
	}

	// =====================================================================
	printf("\n-- (b9) UN GRAPHE QUI NE COMPILE PAS N'EST PAS EXECUTE, ET IL LE DIT\n");
	// =====================================================================
	//  §6.2 : « Un graphe qui ne compile pas (fil manquant sur une entree
	//  obligatoire, cycle de noeuds purs, type incompatible) n'est PAS execute et
	//  le dit : `E-GRAPHE` + le noeud en cause. »
	//
	//  ⚠️ LE REFUS PORTE SUR LE GRAPHE ENTIER, PAS SUR LE NOEUD FAUTIF. Executer
	//     la moitie d'un comportement donnerait un etat que personne n'a decrit.
	{
		struct Cas {
				const char *nom;
				const char *doc;
		};
		static const Cas kCas[] = {
			{"un type inconnu",
			 "nkgui 0.3\n"
			 "behavior \"x\" graph {\n"
			 "  node n1 EventClick\n"
			 "  node n2 NoeudQuiNexistePas { a = 1 }\n"
			 "  wire n1.exec -> n2.exec\n"
			 "}\n"},
			{"une entree ni reliee ni ecrite",
			 "nkgui 0.3\n"
			 "behavior \"x\" graph {\n"
			 "  node n1 EventClick\n"
			 "  node n2 Show\n"
			 "  wire n1.exec -> n2.exec\n"
			 "}\n"},
			{"un cycle de noeuds purs",
			 "nkgui 0.3\n"
			 "behavior \"x\" graph {\n"
			 "  node n1 EventClick\n"
			 "  node n2 Not   { a = n3.result }\n"
			 "  node n3 Not   { a = n2.result }\n"
			 "  node n4 Branch { cond = n2.result }\n"
			 "  wire n1.exec -> n4.exec\n"
			 "  wire n2.result -> n3.a\n"
			 "  wire n3.result -> n2.a\n"
			 "}\n"},
			{"aucun noeud d'evenement",
			 "nkgui 0.3\n"
			 "behavior \"x\" graph {\n"
			 "  node n1 Show { target = \"a\" }\n"
			 "}\n"},
			{"deux noeuds d'evenement",
			 "nkgui 0.3\n"
			 "behavior \"x\" graph {\n"
			 "  node n1 EventClick\n"
			 "  node n2 EventHover\n"
			 "}\n"},
		};
		for (uint32 k = 0; k < (uint32)(sizeof(kCas) / sizeof(Cas)); ++k) {
			Scene s;
			const uint32 n = (uint32)__builtin_strlen(kCas[k].doc);
			if (!s.Charger(kCas[k].doc, n, 200, 120))
				continue;
			s.Image();
			const NkGuiRapportGraphe &r = s.exe.eval.rapport.graphe;
			char titre[200];
			Joindre(titre, sizeof(titre), "(b9) REFUSE : ", kCas[k].nom);
			Check(r.refuses == 1u && r.compiles == 0u, titre);
			if (r.causes.Size() > 0u)
				printf("        %s\n", r.causes[0].CStr());
			else
				printf("        (aucune cause nommee !)\n");
			char titre2[200];
			Joindre(titre2, sizeof(titre2), "(b9) ... et la cause est NOMMEE : ", kCas[k].nom);
			Check(r.causes.Size() > 0u, titre2);
			s.exe.Debrancher(s.ctx);
		}
	}

	// =====================================================================
	printf("\n-- (b10) L'INFOBULLE ET L'ICONE SE VOIENT — pas seulement se comptent\n");
	// =====================================================================
	//  Rodolf, 27/09 : « les silences fermes sont donc fonctionnels deja ? sinon
	//  ils doivent etre fonctionnels s'ils ont une utilite. »
	//
	//  🔴 LA QUESTION ETAIT JUSTE POUR DEUX D'ENTRE EUX. Pour `tooltip` et pour
	//     `icon`, je n'avais mesure que le COMPTEUR : « une infobulle a ete
	//     posee », « une icone a ete demandee et le glyphe manque ». Aucun des
	//     deux ne disait que quelque chose ARRIVE A L'ECRAN. *Un compteur vert
	//     n'est pas un rendu juste* — ce depot le sait, et je l'avais oublie sur
	//     ces deux-la.
	{
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Button \"outil.deplacer\" { label = \"Deplacer\", tooltip = \"Deplacer l'objet\","
			" shortcut = \"W\" }\n"
			"}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200), "(b10) le document se charge");

		// ── L'ETAT DE REFERENCE : pointeur LOIN du bouton ────────────────
		s.PoserEtStabiliser(300.f, 190.f);
		const uint32 sansSurvol = ComptePixelsPeints(s.ras, kFond);
		printf("        pointeur loin : %u px peints, infobulles=%u\n", sansSurvol,
			   s.rap.infobullesPosees);
		CheckEqU(s.rap.infobullesPosees, 0u, "(b10) loin du bouton, AUCUNE infobulle posee");

		// ── LE SURVOL ────────────────────────────────────────────────────
		NkRect r{0.f, 0.f, 0.f, 0.f};
		const bool aR = s.Rect("outil.deplacer", r);
		Check(aR, "(b10) le rectangle du bouton sort du montage");
		if (aR) {
			s.PoserEtStabiliser(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
			const uint32 avecSurvol = ComptePixelsPeints(s.ras, kFond);
			printf("        pointeur sur le bouton : %u px peints, infobulles=%u\n", avecSurvol,
				   s.rap.infobullesPosees);
			CheckEqU(s.rap.infobullesPosees, 1u, "(b10) survole, UNE infobulle est posee");
			// ⚠️ LE CRITERE EST EN PIXELS, PAS EN COMPTEUR. Une infobulle posee et
			//    jamais peinte laisserait le compteur a 1 et l'image inchangee —
			//    c'est exactement l'etat que ce banc ne savait pas voir avant que
			//    la SURIMPRESSION soit rasterisee.
			Check(avecSurvol > sansSurvol + 100u,
				  "(b10) L'INFOBULLE EST PEINTE — l'image gagne plus de 100 pixels");

			// ── ET ELLE PORTE LE RACCOURCI (P8) ─────────────────────────
			//  L'infobulle du document dit « Deplacer l'objet » ; avec `shortcut`
			//  elle doit dire « Deplacer l'objet (W) ». La difference se mesure a
			//  sa LARGEUR : un texte plus long fait une bulle plus large.
			static const char kSansRaccourci[] =
				"nkgui 0.3\n"
				"widgets {\n"
				"  Button \"outil.deplacer\" { label = \"Deplacer\", tooltip = \"Deplacer "
				"l'objet\" }\n"
				"}\n";
			Scene s2;
			if (s2.Charger(kSansRaccourci, (uint32)(sizeof(kSansRaccourci) - 1u), 320, 200)) {
				NkRect r2{0.f, 0.f, 0.f, 0.f};
				if (s2.Rect("outil.deplacer", r2) || true) {
					s2.PoserEtStabiliser(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
					const uint32 sansRacc = ComptePixelsPeints(s2.ras, kFond);
					printf("        sans `shortcut` : %u px  |  avec : %u px\n", sansRacc,
						   avecSurvol);
					Check(avecSurvol > sansRacc,
						  "(b10) P8 : le raccourci AGRANDIT l'infobulle — il y est vraiment");
				}
				s2.exe.Debrancher(s2.ctx);
			}
		}
		s.exe.Debrancher(s.ctx);
	}

	// =====================================================================
	printf("\n-- (b11) P12 : une icone POSEE par l'hote est DESSINEE\n");
	// =====================================================================
	//  ⚠️ LE PREMIER TEMOIN NE MESURAIT QUE L'ABSENCE : « sans jeu d'icones,
	//     elle est comptee manquante ». C'est la moitie facile. Celui-ci pose un
	//     jeu et exige que le glyphe ARRIVE DANS L'IMAGE.
	{
		static const char kDoc[] =
			"nkgui 0.3\n"
			"widgets {\n"
			"  Button \"outil.deplacer\" { label = \"Deplacer\", icon = croix }\n"
			"}\n";

		// Un jeu d'icones minimal : une CROIX vectorielle dans la boite unite.
		// ⚠️ Elle est faite de deux traits epais, et c'est deliberе : un glyphe
		//    trop fin se confondrait avec l'anticrenelage du libelle, et le
		//    critere ne saurait plus ce qu'il compte.
		NkGuiIconSet jeu;
		jeu.Reset(0u, 0, 0);
		const NkGuiIconHandle h = jeu.AddPath("croix");
		// Deux traits croises, dans la boite unite, EPAIS : un glyphe trop fin
		// se confondrait avec l'anticrenelage du libelle, et le critere ne
		// saurait plus ce qu'il compte.
		{
			const NkVec2 d1[2] = {{0.15f, 0.15f}, {0.85f, 0.85f}};
			const NkVec2 d2[2] = {{0.85f, 0.15f}, {0.15f, 0.85f}};
			const bool c1 = jeu.AddContour(h, d1, 2, false, 0.22f, false);
			const bool c2 = jeu.AddContour(h, d2, 2, false, 0.22f, false);
			Check(c1 && c2, "(b11) le jeu accepte les deux contours du glyphe");
		}

		Scene sSans;
		Check(sSans.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b11) le document se charge (sans jeu)");
		NkGuiPoserIcones(nullptr);
		sSans.Image();
		sSans.Image();
		const uint32 pxSans = ComptePixelsPeints(sSans.ras, kFond);

		Scene sAvec;
		Check(sAvec.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 320, 200),
			  "(b11) le document se charge (avec jeu)");
		NkGuiPoserIcones(&jeu);
		sAvec.Image();
		sAvec.Image();
		const uint32 pxAvec = ComptePixelsPeints(sAvec.ras, kFond);
		printf("        sans jeu : %u px, manquantes=%u  |  avec jeu : %u px, manquantes=%u\n",
			   pxSans, sSans.rap.iconesManquantes, pxAvec, sAvec.rap.iconesManquantes);
		CheckEqU(sSans.rap.iconesManquantes, 1u, "(b11) sans jeu, l'icone est comptee MANQUANTE");
		CheckEqU(sAvec.rap.iconesDemandees, 1u, "(b11) avec jeu, l'icone est demandee");

		// ⚠️ ON N'EXIGE PAS ENCORE `manquantes == 0` : le glyphe pose ci-dessus
		//    n'a peut-etre aucun contour, et le jeu le dirait par son repli. Ce
		//    que le critere exige, c'est que l'etat CHANGE : poser un jeu ne doit
		//    pas etre sans effet.
		// ⚠️ ET LE CRITERE QUI COMPTE EST EN PIXELS. `manquantes` qui tombe a
		//    zero dit que le GLYPHE A ETE TROUVE ; il ne dit pas qu'il a ete
		//    DESSINE. Les deux images doivent differer, sinon l'icone est un
		//    succes annonce et invisible.
		//
		// 🔴 ET LE COMPTEUR « PIXELS NON-FOND » NE POUVAIT PAS TRANCHER : il rend
		//    2 063 des deux cotes, parce que l'icone REMPLACE des pixels du
		//    libelle au lieu d'en ajouter. C'est la DEUXIEME fois en deux jours
		//    qu'un compteur sature masque un changement — le compteur `contenu` de
		//    la sonde avait fait exactement cela le 26/09, a 53 239 avant comme
		//    apres. *Un compteur qui totalise ne voit pas un remplacement ; il
		//    faut comparer les images.*
		{
			uint32 differents = 0;
			for (int32 y = 0; y < sAvec.ras.Hauteur(); ++y)
				for (int32 x = 0; x < sAvec.ras.Largeur(); ++x)
					if (sAvec.ras.Pixel(x, y) != sSans.ras.Pixel(x, y))
						++differents;
			printf("        pixels qui different avec/sans le jeu : %u\n", differents);
			CheckEqU(sSans.rap.iconesManquantes, 1u,
					 "(b11) sans jeu : le glyphe manque");
			CheckEqU(sAvec.rap.iconesManquantes, 0u,
					 "(b11) avec jeu : le glyphe est TROUVE");
			Check(differents > 20u,
				  "(b11) ET DESSINE — plus de 20 pixels changent");
		}
		NkGuiPoserIcones(nullptr);
		sSans.exe.Debrancher(sSans.ctx);
		sAvec.exe.Debrancher(sAvec.ctx);
	}

	CasConteneursFreres();

	CasTableRappels();

	CasQuatreConteneurs();

	CasDockable();

	CasMenuContextuel();

	CasCourbe();

	CasPlateformes();

	CasRacineNommee();
	CasQuatreConteneursDePlus();

	CasBarreMenus();

	CasOngletsVerticaux();

	CasHoteDansMenu();

	CasDefilementFondMouvant();

	printf("\n=== %d / %d ===\n", g_pass, g_pass + g_fail);
	return g_fail == 0 ? 0 : 1;
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
