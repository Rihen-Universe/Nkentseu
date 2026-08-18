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

	printf("\n=== %d/%d ===\n", g_pass, g_pass + g_fail);
	return g_fail == 0 ? 0 : 1;
}
