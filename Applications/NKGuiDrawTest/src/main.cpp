// =============================================================================
// NKGuiDrawTest — banc TEMOIN NON VISUEL des primitives de dessin NKGui.
//
// Ce qu'il fait : appelle les primitives de NkGuiDrawList et VERIFIE LA SORTIE
// (nombre de sommets, d'indices, de commandes, et des proprietes GEOMETRIQUES
// re-calculees depuis les triangles emis). Aucune fenetre, aucun device, aucun
// GPU — donc exercable pendant qu'une campagne d'entrainement occupe la carte.
//
// Ce qu'il NE fait PAS : il ne prouve pas qu'un backend dessine correctement ce
// flux a l'ecran. Il prouve que le flux EST celui qu'on croit. Le rendu reel
// reste a confirmer par un temoin visuel, differe et nomme (voir ROADMAP).
//
// Sortie : `n/n` + code de sortie (0 = tout passe, 1 = au moins un echec).
// =============================================================================
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiIcons.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::nkgui;

static int g_pass = 0, g_fail = 0;

static void Check(bool ok, const char *name) {
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s\n", ok ? "OK" : "KO", name);
}

// ── Outils de mesure sur la geometrie EMISE ─────────────────────────────────

// Le point p est-il couvert par au moins un triangle de la liste ? C'est la
// mesure qui distingue un ANNEAU d'un DISQUE sans regarder une image.
static bool Covers(const NkGuiDrawList &dl, float32 px, float32 py) {
	for (uint32 i = 0; i + 2 < dl.idx.Size(); i += 3) {
		const NkVec2 &a = dl.vtx[dl.idx[i]].pos;
		const NkVec2 &b = dl.vtx[dl.idx[i + 1]].pos;
		const NkVec2 &c = dl.vtx[dl.idx[i + 2]].pos;
		const float32 d1 = (px - b.x) * (a.y - b.y) - (a.x - b.x) * (py - b.y);
		const float32 d2 = (px - c.x) * (b.y - c.y) - (b.x - c.x) * (py - c.y);
		const float32 d3 = (px - a.x) * (c.y - a.y) - (c.x - a.x) * (py - a.y);
		const bool neg = (d1 < 0.f) || (d2 < 0.f) || (d3 < 0.f);
		const bool pos = (d1 > 0.f) || (d2 > 0.f) || (d3 > 0.f);
		if (!(neg && pos))
			return true; // meme signe (ou sur une arete) => dedans
	}
	return false;
}

// Tous les sommets tiennent-ils dans `r` (tolerance eps) ?
static bool AllInside(const NkGuiDrawList &dl, const NkRect &r, float32 eps) {
	for (uint32 i = 0; i < dl.vtx.Size(); ++i) {
		const NkVec2 &p = dl.vtx[i].pos;
		if (p.x < r.x - eps || p.y < r.y - eps || p.x > r.x + r.w + eps || p.y > r.y + r.h + eps)
			return false;
	}
	return true;
}

static float32 Abs(float32 v) {
	return v < 0.f ? -v : v;
}

// Comparaison de noms sans <cstring> (le depot est zero-STL).
static bool NameEq(const char *a, const char *b) {
	while (*a && *b && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

int main() {
	printf("=== NKGuiDrawTest : geometrie des primitives NKGui (sans GPU) ===\n\n");

	const NkColor col{200, 100, 50, 255};
	const NkRect r{10.f, 20.f, 120.f, 80.f};

	// ── 1. AddRect DROIT : le chemin historique ne bouge pas ────────────────
	printf("-- AddRect, coins droits (chemin historique, non regresse)\n");
	{
		NkGuiDrawList dl;
		dl.AddRect(r, col, 2.f);
		// 4 bords x (4 sommets + 6 indices) = 16 / 24.
		Check(dl.vtx.Size() == 16u, "4 bords -> 16 sommets");
		Check(dl.idx.Size() == 24u, "4 bords -> 24 indices");
		Check(dl.cmds.Size() == 1u, "une seule commande (meme clip, meme texture)");
		Check(AllInside(dl, r, 0.001f), "trait STRICTEMENT a l'interieur du rect");
	}

	// ── 2. AddRect ARRONDI : le nouveau chemin ──────────────────────────────
	printf("\n-- AddRect, coins arrondis (l'ajout)\n");
	{
		NkGuiDrawList dl;
		dl.AddRect(r, col, 2.f, 12.f);
		Check(dl.vtx.Size() > 0u && dl.idx.Size() > 0u, "produit de la geometrie");
		Check(dl.idx.Size() % 3u == 0u, "indices multiples de 3 (triangles complets)");
		Check(AllInside(dl, r, 0.001f), "trait STRICTEMENT a l'interieur du rect");

		// LA propriete : c'est un ANNEAU. Le centre doit rester VIDE.
		const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
		Check(!Covers(dl, cx, cy), "le centre n'est PAS couvert (anneau, pas disque)");
		// ... et le trait, lui, EST couvert : sinon le test ci-dessus passerait
		// pour la mauvaise raison (liste vide, geometrie ailleurs).
		Check(Covers(dl, r.x + 1.f, cy), "controle positif : le bord gauche EST couvert");
		Check(Covers(dl, cx, r.y + 1.f), "controle positif : le bord haut EST couvert");
	}

	// ── 3. Bornes et degenerescences ────────────────────────────────────────
	printf("\n-- AddRect : bornes\n");
	{
		NkGuiDrawList dl;
		dl.AddRect(r, col, 2.f, 1000.f); // arrondi absurde -> borne a min(w,h)/2
		Check(AllInside(dl, r, 0.001f), "arrondi 1000 borne : rien ne sort du rect");
		Check(dl.idx.Size() > 0u, "arrondi 1000 : geometrie non vide");
	}
	{
		NkGuiDrawList dl;
		dl.AddRect(r, col, 60.f, 10.f); // trait plus epais que la moitie -> plein
		const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
		Check(Covers(dl, cx, cy), "trait plus epais que la moitie -> devient plein");
	}
	{
		NkGuiDrawList dl;
		dl.AddRect({0.f, 0.f, 0.f, 0.f}, col, 2.f, 4.f);
		Check(dl.idx.Size() == 0u, "rect de taille nulle -> rien (zero)");
		dl.AddRect(r, col, 0.f, 4.f);
		Check(dl.idx.Size() == 0u, "epaisseur nulle -> rien (zero)");
		dl.AddRect(r, col, 2.f, 4.f);
		Check(dl.idx.Size() > 0u, "controle positif du zero : appel valide -> non vide");
	}

	// ── 4. AddCircle (contour) ──────────────────────────────────────────────
	printf("\n-- AddCircle, contour (remplace 3 emulations : Mou, Nkoung, ConquerorLab)\n");
	{
		const NkVec2 c{100.f, 100.f};
		const float32 rad = 30.f, th = 4.f;
		NkGuiDrawList dl;
		dl.AddCircle(c, rad, col, th);
		Check(dl.idx.Size() > 0u && dl.idx.Size() % 3u == 0u, "produit des triangles complets");
		Check(!Covers(dl, c.x, c.y), "le centre n'est PAS couvert (anneau)");
		Check(Covers(dl, c.x + rad, c.y), "controle positif : la ligne mediane EST couverte");
		// `rad` est la ligne MEDIANE : tous les sommets dans [rad-th/2, rad+th/2].
		bool band = true;
		const float32 lo = (rad - th * 0.5f) * (rad - th * 0.5f) - 0.01f;
		const float32 hi = (rad + th * 0.5f) * (rad + th * 0.5f) + 0.01f;
		for (uint32 i = 0; i < dl.vtx.Size(); ++i) {
			const float32 dx = dl.vtx[i].pos.x - c.x, dy = dl.vtx[i].pos.y - c.y;
			const float32 d2 = dx * dx + dy * dy;
			if (d2 < lo || d2 > hi) {
				band = false;
				break;
			}
		}
		Check(band, "rayon = ligne MEDIANE : sommets dans [r-th/2, r+th/2]");
	}
	{
		NkGuiDrawList dl;
		dl.AddCircle({0.f, 0.f}, 0.f, col, 2.f);
		Check(dl.idx.Size() == 0u, "rayon nul -> rien (zero)");
		dl.AddCircle({0.f, 0.f}, 10.f, col, 0.f);
		Check(dl.idx.Size() == 0u, "epaisseur nulle -> rien (zero)");
		dl.AddCircle({0.f, 0.f}, 10.f, col, 2.f);
		Check(dl.idx.Size() > 0u, "controle positif du zero : appel valide -> non vide");
	}
	{
		// Contre-epreuve : le plein, lui, DOIT couvrir son centre.
		NkGuiDrawList dl;
		dl.AddCircleFilled({100.f, 100.f}, 30.f, col);
		Check(Covers(dl, 100.f, 100.f), "contre-epreuve : AddCircleFilled couvre son centre");
	}

	// ── 5. Polygones ────────────────────────────────────────────────────────
	printf("\n-- AddConvexPolyFilled / AddPolyline (remplacent NkcPoly* de ConquerorLab)\n");
	{
		const NkVec2 pts[5] = {{0.f, 0.f}, {40.f, 0.f}, {50.f, 30.f}, {20.f, 50.f}, {-10.f, 30.f}};
		NkGuiDrawList dl;
		dl.AddConvexPolyFilled(pts, 5, col);
		Check(dl.vtx.Size() == 5u, "5 points -> 5 sommets (eventail, pas de doublon)");
		Check(dl.idx.Size() == 9u, "5 points -> 3 triangles (n-2)");
		Check(Covers(dl, 20.f, 20.f), "un point interieur EST couvert");

		NkGuiDrawList z;
		z.AddConvexPolyFilled(pts, 2, col);
		Check(z.idx.Size() == 0u, "moins de 3 points -> rien (zero)");
		z.AddConvexPolyFilled(nullptr, 5, col);
		Check(z.idx.Size() == 0u, "pointeur nul -> rien (zero)");
		z.AddConvexPolyFilled(pts, 3, col);
		Check(z.idx.Size() == 3u, "controle positif du zero : 3 points -> 1 triangle");
	}
	{
		const NkVec2 pts[4] = {{0.f, 0.f}, {40.f, 0.f}, {40.f, 40.f}, {0.f, 40.f}};
		NkGuiDrawList open_;
		open_.AddPolyline(pts, 4, col, 2.f, false);
		Check(open_.vtx.Size() == 12u, "ligne OUVERTE : 3 segments -> 12 sommets");
		NkGuiDrawList closed;
		closed.AddPolyline(pts, 4, col, 2.f, true);
		Check(closed.vtx.Size() == 16u, "ligne FERMEE : 4 segments -> 16 sommets");
	}

	// ── 6. Le clip s'applique aux nouvelles primitives ──────────────────────
	printf("\n-- Clip : les ajouts respectent la pile de decoupe\n");
	{
		NkGuiDrawList dl;
		dl.PushClipRect({0.f, 0.f, 50.f, 50.f});
		dl.AddRect(r, col, 2.f, 8.f);
		dl.AddCircle({20.f, 20.f}, 10.f, col, 2.f);
		bool clipped = true;
		for (uint32 i = 0; i < dl.cmds.Size(); ++i)
			if (Abs(dl.cmds[i].clipRect.w - 50.f) > 0.001f)
				clipped = false;
		dl.PopClipRect();
		Check(dl.cmds.Size() > 0u && clipped, "commandes marquees du clip courant");
	}

	// ── 7. Jetons de theme : ils s'ENUMERENT (socle de NKUIEditor) ──────────
	printf("\n-- Jetons de theme : table de description\n");
	{
		int32 n = 0;
		const NkGuiTokenDesc *t = NkGuiThemeTokens(&n);
		Check(t != nullptr && n > 0, "la table existe et n'est pas vide");
		printf("     (%d jetons decrits)\n", n);

		bool names = true, offs = true, dup = false;
		for (int32 i = 0; i < n; ++i) {
			if (!t[i].name || !t[i].name[0] || !t[i].group || !t[i].group[0])
				names = false;
			if (t[i].offset >= (uint16)sizeof(NkGuiTheme))
				offs = false;
			for (int32 j = i + 1; j < n; ++j)
				if (NameEq(t[i].name, t[j].name))
					dup = true;
		}
		Check(names, "tout jeton a un nom et un groupe non vides");
		Check(offs, "tout decalage tombe DANS NkGuiTheme");
		Check(!dup, "aucun nom en double");

		NkGuiTheme th;
		// Chaque jeton decrit doit etre atteignable par son nom.
		bool reach = true;
		for (int32 i = 0; i < n; ++i) {
			const bool ok = (t[i].type == NkGuiTokenType::Color) ? (NkGuiThemeColor(th, t[i].name) != nullptr)
																 : (NkGuiThemeScalar(th, t[i].name) != nullptr);
			if (!ok)
				reach = false;
		}
		Check(reach, "chaque jeton decrit est atteignable par son nom");

		// Aller-retour : ecrire par nom modifie bien le champ.
		NkColor *acc = NkGuiThemeColor(th, "accent");
		Check(acc == &th.accent, "le nom accent pointe sur le vrai champ");
		if (acc)
			acc->r = 7;
		Check(th.accent.r == 7, "ecrire par nom modifie le theme");

		float32 *rnd = NkGuiThemeScalar(th, "rounding");
		Check(rnd == &th.rounding, "le nom rounding pointe sur le vrai champ");

		// Zeros — chacun double d'un controle positif.
		Check(NkGuiThemeColor(th, "nexistepas") == nullptr, "nom inconnu -> nullptr");
		Check(NkGuiThemeColor(th, "panel") != nullptr, "controle positif : panel -> non nul");
		Check(NkGuiThemeColor(th, "rounding") == nullptr, "mauvais type (scalaire lu en couleur) -> nullptr");
		Check(NkGuiThemeScalar(th, "accent") == nullptr, "mauvais type (couleur lue en scalaire) -> nullptr");
		Check(NkGuiThemeColor(th, "onAccent") != nullptr, "le jeton le plus attendu (onAccent) existe");
	}


	// ── 8. Jeu d'icones : atlas rasterise (source 1) ────────────────────────
	printf("\n-- Icones, source BITMAP : la geometrie echantillonne-t-elle la bonne region ?\n");
	{
		NkGuiIconSet set;
		set.Reset(7u, 128, 64); // texture 7, atlas 128x64
		const NkGuiIconHandle a = set.AddBitmap("a", 0, 0, 16, 16);
		const NkGuiIconHandle b = set.AddBitmap("b", 16, 0, 16, 16);
		Check(a.Valid() && b.Valid() && a != b, "deux glyphes -> deux poignees distinctes et valides");
		Check(set.Find("a") == a, "resolution par nom : retrouve la meme poignee");
		Check(!set.Find("inconnu").Valid(), "nom inconnu -> poignee invalide");
		Check(set.Find("b").Valid(), "controle positif : un nom connu -> poignee valide");

		// LA mesure : les UV emises couvrent EXACTEMENT la region declaree.
		NkGuiDrawList dl;
		const NkRect dst{10.f, 10.f, 32.f, 32.f};
		const NkGuiIconDraw res = AddIcon(dl, set, a, dst, col);
		Check(res == NkGuiIconDraw::Glyph, "poignee valide -> le glyphe demande");
		Check(dl.vtx.Size() == 4u && dl.idx.Size() == 6u, "un quad : 4 sommets, 6 indices");
		Check(dl.cmds.Size() == 1u && dl.cmds[0].texId == 7u, "commande TEXTUREE sur la texture du jeu");
		bool uvOk = true;
		{
			float32 u0 = 1.f, v0 = 1.f, u1 = 0.f, v1 = 0.f;
			for (uint32 i = 0; i < dl.vtx.Size(); ++i) {
				const NkVec2 &uv = dl.vtx[i].uv;
				if (uv.x < u0)
					u0 = uv.x;
				if (uv.y < v0)
					v0 = uv.y;
				if (uv.x > u1)
					u1 = uv.x;
				if (uv.y > v1)
					v1 = uv.y;
			}
			// region (0,0,16,16) dans 128x64 -> u [0, 0.125], v [0, 0.25]
			uvOk = Abs(u0 - 0.f) < 1e-5f && Abs(v0 - 0.f) < 1e-5f && Abs(u1 - 0.125f) < 1e-5f &&
				   Abs(v1 - 0.25f) < 1e-5f;
		}
		Check(uvOk, "UV = EXACTEMENT la region declaree (0,0,16,16) / atlas 128x64");

		// Contre-epreuve : une AUTRE region donne d'AUTRES UV. Sans ca, le test
		// ci-dessus passerait meme si le code renvoyait toujours le meme quad.
		NkGuiDrawList db;
		AddIcon(db, set, b, dst, col);
		float32 bu0 = 1.f;
		for (uint32 i = 0; i < db.vtx.Size(); ++i)
			if (db.vtx[i].uv.x < bu0)
				bu0 = db.vtx[i].uv.x;
		Check(Abs(bu0 - 0.125f) < 1e-5f, "contre-epreuve : la region voisine donne u0 = 16/128");

		// La teinte traverse : aucune couleur en dur dans le mecanisme.
		Check(dl.vtx[0].col == NkGuiPackColor(col), "la teinte de l'appelant traverse (jeton de theme)");
	}

	// ── 9. Icones : ce qui doit ECHOUER, et le DIRE ─────────────────────────
	printf("\n-- Icones : une demande qui n'aboutit pas doit se CONSTATER\n");
	{
		NkGuiIconSet set;
		set.Reset(7u, 128, 64);
		const NkGuiIconHandle a = set.AddBitmap("a", 0, 0, 16, 16);
		const NkRect dst{0.f, 0.f, 32.f, 32.f};

		NkGuiDrawList d1;
		Check(AddIcon(d1, set, NkGuiIconHandle{}, dst, col) == NkGuiIconDraw::None,
			  "poignee invalide sans secours -> None");
		Check(d1.idx.Size() == 0u, "... et rien n'est dessine");

		set.SetFallback(a);
		NkGuiDrawList d2;
		Check(AddIcon(d2, set, NkGuiIconHandle{}, dst, col) == NkGuiIconDraw::Fallback,
			  "poignee invalide AVEC secours -> Fallback (et il le DIT)");
		Check(d2.idx.Size() > 0u, "... et le glyphe de secours est bien dessine");

		// LA propriete de securite : une poignee d'un AUTRE jeu ne doit jamais
		// dessiner un glyphe de celui-ci comme si de rien n'etait.
		NkGuiIconSet other;
		other.Reset(9u, 64, 64);
		const NkGuiIconHandle foreign = other.AddBitmap("x", 0, 0, 8, 8);
		Check(foreign.Valid(), "controle positif : la poignee etrangere est valide DANS SON jeu");
		Check(set.Glyph(foreign) == nullptr, "poignee etrangere -> rejetee par l'autre jeu");
		NkGuiIconSet noFb;
		noFb.Reset(7u, 128, 64);
		noFb.AddBitmap("a", 0, 0, 16, 16);
		NkGuiDrawList d3;
		Check(AddIcon(d3, noFb, foreign, dst, col) == NkGuiIconDraw::None,
			  "poignee etrangere sans secours -> None, PAS un mauvais glyphe");

		// Declarations refusees, chacune doublee d'un controle positif.
		Check(!set.AddBitmap("", 0, 0, 16, 16).Valid(), "nom vide -> refuse");
		Check(!set.AddBitmap("hors", 120, 0, 16, 16).Valid(), "region hors de l'atlas -> refusee");
		Check(!set.AddBitmap("nul", 0, 0, 0, 16).Valid(), "region de largeur nulle -> refusee");
		Check(set.AddBitmap("bon", 0, 16, 16, 16).Valid(), "controle positif : region valide -> acceptee");

		// Recharger ne doit pas invalider ce que l'application tient.
		const NkGuiIconHandle again = set.AddBitmap("a", 32, 0, 16, 16);
		Check(again == a, "re-declarer un nom conserve la poignee (rechargement sur)");
	}

	// ── 10. Jeu d'icones : contours VECTORIELS (source 2) ───────────────────
	printf("\n-- Icones, source VECTORIELLE : editable, recolorable, nette a tout DPI\n");
	{
		NkGuiIconSet set;
		set.Reset(); // aucun atlas : jeu 100 % vectoriel
		const NkGuiIconHandle tri = set.AddPath("triangle");
		Check(tri.Valid(), "un glyphe vectoriel s'ouvre");
		// Triangle dans la boite unite.
		const NkVec2 pts[3] = {{0.5f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f}};
		Check(set.AddContour(tri, pts, 3, true), "un contour rempli s'ajoute");

		NkGuiDrawList dl;
		const NkRect dst{100.f, 200.f, 40.f, 40.f};
		Check(AddIcon(dl, set, tri, dst, col) == NkGuiIconDraw::Glyph, "le glyphe vectoriel se dessine");
		Check(dl.vtx.Size() == 3u && dl.idx.Size() == 3u, "3 points -> 1 triangle (aucune rasterisation)");
		Check(AllInside(dl, dst, 0.001f), "la geometrie tient dans le rect demande");
		Check(dl.cmds.Size() == 1u && dl.cmds[0].texId == 0u,
			  "commande NON texturee : c'est de la geometrie, pas un echantillonnage");
		// Mise a l'echelle exacte de la boite unite : (0.5,0.1) -> (120, 204).
		bool mapped = false;
		for (uint32 i = 0; i < dl.vtx.Size(); ++i)
			if (Abs(dl.vtx[i].pos.x - 120.f) < 0.001f && Abs(dl.vtx[i].pos.y - 204.f) < 0.001f)
				mapped = true;
		Check(mapped, "boite unite -> rect : (0.5, 0.1) tombe bien sur (120, 204)");
		Check(dl.vtx[0].col == NkGuiPackColor(col), "recoloration : la teinte traverse (pas d'atlas par couleur)");

		// LA propriete DPI : le meme glyphe a deux echelles produit une geometrie
		// PROPORTIONNELLE. Rien n'est echantillonne, donc rien ne peut flouter.
		NkGuiDrawList p1, p4;
		AddIcon(p1, set, tri, {0.f, 0.f, 16.f, 16.f}, col);
		AddIcon(p4, set, tri, {0.f, 0.f, 64.f, 64.f}, col);
		bool prop = (p1.vtx.Size() == p4.vtx.Size());
		if (prop)
			for (uint32 i = 0; i < p1.vtx.Size(); ++i)
				if (Abs(p4.vtx[i].pos.x - 4.f * p1.vtx[i].pos.x) > 0.001f ||
					Abs(p4.vtx[i].pos.y - 4.f * p1.vtx[i].pos.y) > 0.001f)
					prop = false;
		Check(prop, "x4 en taille -> geometrie x4 exactement (nette a tout facteur DPI)");

		// Un contour en TRAIT : l'epaisseur suit l'echelle elle aussi.
		const NkGuiIconHandle box = set.AddPath("cadre");
		const NkVec2 sq[4] = {{0.2f, 0.2f}, {0.8f, 0.2f}, {0.8f, 0.8f}, {0.2f, 0.8f}};
		Check(set.AddContour(box, sq, 4, false, 0.1f, true), "un contour en TRAIT s'ajoute");
		NkGuiDrawList dbox;
		Check(AddIcon(dbox, set, box, {0.f, 0.f, 40.f, 40.f}, col) == NkGuiIconDraw::Glyph,
			  "le contour en trait se dessine");
		Check(dbox.vtx.Size() == 16u, "trait ferme de 4 points -> 4 segments -> 16 sommets");

		// Zeros, chacun double d'un controle positif.
		const NkGuiIconHandle vide = set.AddPath("vide");
		NkGuiDrawList dv;
		Check(AddIcon(dv, set, vide, dst, col) == NkGuiIconDraw::None, "glyphe vectoriel SANS contour -> None");
		Check(!set.AddContour(tri, pts, 3, true), "contour sur un glyphe qui n'est plus ouvert -> refuse");
		Check(set.AddContour(vide, pts, 3, true), "controle positif : sur le glyphe ouvert -> accepte");
		Check(!set.AddContour(vide, pts, 2, true), "remplissage a 2 points -> refuse");
		Check(!set.AddContour(vide, nullptr, 3, true), "pointeur nul -> refuse");

		// Le jeu s'ENUMERE — un futur NkUIDesign doit pouvoir lister et editer.
		Check(set.Count() == 3, "le jeu s'enumere (3 glyphes declares)");
		Check(set.GlyphAt(0) != nullptr && set.GlyphAt(99) == nullptr, "enumeration bornee");
		Check(set.PointCount() > 0, "les points des contours sont lisibles (editables)");
	}


	// ── 11. PLACEMENT EXPLICITE — le manque qui debloquait 102 fonctions ────
	printf("\n-- SetNextItemRect : un rectangle POSE l'emporte sur le placement automatique\n");
	{
		// Mesure a l'origine : sur 114 fonctions declarees dans NkGuiWidgets.h,
		// 12 acceptent un NkRect et 102 se placent elles-memes. Une interface
		// pilotee par rectangles ne pouvait en appeler que 12.
		NkGuiContext ctx;
		ctx.viewW = 800;
		ctx.viewH = 600;
		const NkRect region{0.f, 0.f, 400.f, 400.f};
		const NkRect posed{50.f, 60.f, 120.f, 30.f};

		// (a) Placement AUTOMATIQUE — le comportement historique.
		ctx.BeginLayout(region);
		ctx.DL().Reset();
		Button(ctx, "auto");
		const NkRect autoR = ctx.lastItemRect;
		Check(autoR.w > 0.f && autoR.h > 0.f, "sans rectangle pose : le widget se place tout seul");
		Check(!(Abs(autoR.x - posed.x) < 0.001f && Abs(autoR.y - posed.y) < 0.001f),
			  "contre-epreuve : le placement automatique ne tombe PAS sur le rect pose");

		// (b) Rectangle POSE — la geometrie doit tomber EXACTEMENT dedans.
		ctx.BeginLayout(region);
		ctx.DL().Reset();
		ctx.SetNextItemRect(posed);
		Button(ctx, "pose");
		Check(Abs(ctx.lastItemRect.x - posed.x) < 0.001f && Abs(ctx.lastItemRect.y - posed.y) < 0.001f &&
				  Abs(ctx.lastItemRect.w - posed.w) < 0.001f && Abs(ctx.lastItemRect.h - posed.h) < 0.001f,
			  "le widget prend EXACTEMENT le rectangle pose");
		Check(ctx.DL().idx.Size() > 0u, "il dessine bien quelque chose");
		Check(AllInside(ctx.DL(), posed, 0.001f), "toute la geometrie produite tient dans le rectangle pose");

		// (c) Le rectangle pose ne vaut QU'UNE FOIS.
		ctx.DL().Reset();
		Button(ctx, "suivant");
		Check(!(Abs(ctx.lastItemRect.x - posed.x) < 0.001f && Abs(ctx.lastItemRect.y - posed.y) < 0.001f),
			  "le widget SUIVANT reprend le placement automatique (pose = un seul appel)");

		// (d) Un rectangle pose et jamais consomme ne survit pas a la frame.
		ctx.BeginLayout(region);
		ctx.SetNextItemRect(posed);
		ctx.BeginFrame(0.016f);
		ctx.BeginLayout(region);
		ctx.DL().Reset();
		Button(ctx, "apres frame");
		Check(!(Abs(ctx.lastItemRect.x - posed.x) < 0.001f && Abs(ctx.lastItemRect.y - posed.y) < 0.001f),
			  "un rect pose non consomme ne survit pas a la frame suivante");

		// (e) L'etendue du contenu inclut le rectangle pose — sinon un panneau
		//     defilable ne verrait pas ce qu'on y a place.
		ctx.BeginLayout(region);
		const float32 before = ctx.layout.maxY;
		ctx.SetNextItemRect({10.f, 300.f, 50.f, 40.f});
		Button(ctx, "bas");
		Check(ctx.layout.maxY >= 340.f && ctx.layout.maxY > before,
			  "l'etendue du contenu (maxY) inclut le rectangle pose");

		// (f) Le curseur, lui, ne bouge PAS : celui qui pose place lui-meme.
		ctx.BeginLayout(region);
		const NkVec2 cur0 = ctx.layout.cursor;
		ctx.SetNextItemRect(posed);
		Button(ctx, "curseur");
		Check(Abs(ctx.layout.cursor.x - cur0.x) < 0.001f && Abs(ctx.layout.cursor.y - cur0.y) < 0.001f,
			  "le curseur de mise en page ne bouge pas apres un rect pose");
	}


	// ── 12. SEPARATEUR : dette recuperee de NKUI avant son extinction ───────
	printf("\n-- Splitter : ratio + zone de prehension elargie (reprises de NkUILayout::DrawSplitter)\n");
	{
		const NkRect area{0.f, 0.f, 200.f, 100.f};
		NkRect vis{}, grab{};

		SplitterRects(area, true, 0.5f, 4.f, 12.f, &vis, &grab);
		Check(Abs(vis.x - 98.f) < 0.001f && Abs(vis.w - 4.f) < 0.001f, "vertical, ratio 0.5 : trait fin centre en x=100");
		Check(Abs(grab.x - 94.f) < 0.001f && Abs(grab.w - 12.f) < 0.001f, "prehension elargie a 12 px, meme centre");
		Check(grab.w > vis.w, "on dessine FIN, on attrape LARGE");
		Check(Abs((grab.x + grab.w * 0.5f) - (vis.x + vis.w * 0.5f)) < 0.001f,
			  "les deux rectangles partagent exactement le meme centre");

		// Le ratio suit la zone : c'est ce qu'une position en pixels ne sait pas.
		NkRect v2{};
		SplitterRects({0.f, 0.f, 400.f, 100.f}, true, 0.5f, 4.f, 12.f, &v2, nullptr);
		Check(Abs(v2.x - 198.f) < 0.001f, "zone deux fois plus large : le ratio 0.5 suit (x=200)");

		SplitterRects(area, false, 0.25f, 4.f, 12.f, &vis, &grab);
		Check(Abs(vis.y - 23.f) < 0.001f && Abs(vis.h - 4.f) < 0.001f, "horizontal, ratio 0.25 : trait en y=25");
		Check(Abs(grab.h - 12.f) < 0.001f, "prehension elargie aussi a l'horizontale");

		// Bornes et controles positifs.
		SplitterRects(area, true, 2.f, 4.f, 12.f, &vis, nullptr);
		Check(Abs(vis.x - 198.f) < 0.001f, "ratio > 1 borne a 1 (le trait reste dans la zone)");
		SplitterRects(area, true, -1.f, 4.f, 12.f, &vis, nullptr);
		Check(Abs(vis.x + 2.f) < 0.001f, "ratio < 0 borne a 0");
		SplitterRects(area, true, 0.5f, 4.f, 0.f, &vis, &grab);
		Check(Abs(grab.w - vis.w) < 0.001f, "prehension 0 -> elle vaut le visuel (comportement historique)");
		SplitterRects(area, true, 0.5f, 4.f, 20.f, &vis, &grab);
		Check(Abs(grab.w - 20.f) < 0.001f, "controle positif : une prehension demandee est bien appliquee");
	}

	printf("\n=== %d/%d ===\n", g_pass, g_pass + g_fail);
	return g_fail == 0 ? 0 : 1;
}
