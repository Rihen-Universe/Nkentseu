// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_icone_x11.cpp
//
// L'icone de la fenetre sous X11 (_NET_WM_ICON), eprouvee sur une image dont
// CHAQUE pixel se calcule : l'attendu ne passe pas par le code qu'on eprouve.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (x1) emballage : une image L x H donne L*H + 2 cardinaux ; les deux
//        premiers sont L puis H ; le pixel (x, y) vaut 0xAARRGGBB -- alpha en
//        poids fort, lignes du HAUT vers le bas
//   (x2) deux images : emballees a la suite, dans l'ordre donne
//   (x3) une image invalide (tampon trop court) est sautee, jamais lue
//   (x4) plafond : l'image qui le depasse est laissee, la suivante, plus
//        petite, est emballee quand meme
//   (x5) [serveur X] une fenetre creee avec ces images porte _NET_WM_ICON, de
//        type CARDINAL, format 32, et XGetWindowProperty en rend exactement
//        les cardinaux de (x2)
//   (x6) [serveur X] sans `iconImages`, la propriete N'EXISTE PAS : (x5) ne
//        peut pas reussir sur une propriete que quelqu'un d'autre aurait posee
//
// (x5) et (x6) demandent un serveur X : `xvfb-run -a <le banc>`. La fenetre est
// creee INVISIBLE (jamais affichee, elle ne prend le focus a personne).
// ⚠️ SANS SERVEUR X CES DEUX CAS ECHOUENT, avec le motif. Un banc qui passerait
//    vert faute d'ecran ne dirait rien de l'icone.
//
// Contre-epreuves (08/10, WSL clang 18, Xvfb) -- chacune fait echouer le banc :
//   R et B echanges dans NkX11EmballerIcones            -> (x1) a (x5)
//   le tableau de uint32 passe tel quel a XChangeProperty -> (x5)
//   l'appel NkXLibPoserIcone retire de Create             -> (x5)
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XLIB)

#include "NKWindow/NKWindow.h"
#include "NKWindow/Platform/Common/NkX11Icon.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

// ⚠️ Xlib definit `True`, `False`, `None` et `Success` en MACROS : `TestAssert::True` (derriere
//    ASSERT_TRUE) ne leur survit pas. On garde les VALEURS sous un nom a nous, on rend les noms.
namespace {
	constexpr int kXSucces = Success;
	constexpr Atom kXAucun = None;
	constexpr Bool kXFaux = False;
} // namespace
#undef True
#undef False
#undef None
#undef Success

using namespace nkentseu;

namespace {

	// Les quatre canaux du pixel (x, y). R et B ne sont jamais egaux partout, les
	// lignes et les colonnes ne se ressemblent pas : un echange de canaux, de
	// largeur et de hauteur ou de sens des lignes change la valeur.
	uint8 CanalR(uint32 x, uint32 y) {
		return static_cast<uint8>((x * 37u + y * 11u + 3u) & 0xFFu);
	}
	uint8 CanalG(uint32 x, uint32 y) {
		return static_cast<uint8>((x * 5u + y * 59u + 17u) & 0xFFu);
	}
	uint8 CanalB(uint32 x, uint32 y) {
		return static_cast<uint8>((x * 101u + y * 7u + 200u) & 0xFFu);
	}
	uint8 CanalA(uint32 x, uint32 y) {
		return static_cast<uint8>(255u - ((x + y * 7u) & 0x7Fu));
	}

	/// Le cardinal ATTENDU du pixel (x, y), ecrit depuis la norme EWMH.
	uint32 Attendu(uint32 x, uint32 y) {
		return (static_cast<uint32>(CanalA(x, y)) << 24) | (static_cast<uint32>(CanalR(x, y)) << 16) |
			   (static_cast<uint32>(CanalG(x, y)) << 8) | static_cast<uint32>(CanalB(x, y));
	}

	NkWindowIconImage Fabriquer(uint32 largeur, uint32 hauteur) {
		NkWindowIconImage image;
		image.width = largeur;
		image.height = hauteur;
		image.pixels.Resize(static_cast<usize>(largeur) * hauteur * 4u);
		for (uint32 y = 0; y < hauteur; ++y) {
			for (uint32 x = 0; x < largeur; ++x) {
				uint8 *p = image.pixels.Data() + (static_cast<usize>(y) * largeur + x) * 4u;
				p[0] = CanalR(x, y);
				p[1] = CanalG(x, y);
				p[2] = CanalB(x, y);
				p[3] = CanalA(x, y);
			}
		}
		return image;
	}

	/// Compte les cardinaux qui different de l'attendu pour UNE image posee a
	/// `debut` ; -1 si l'en-tete (largeur, hauteur) est faux.
	template <typename Mot> int32 Ecarts(const Mot *mots, usize debut, uint32 largeur, uint32 hauteur) {
		if (static_cast<uint32>(mots[debut]) != largeur || static_cast<uint32>(mots[debut + 1]) != hauteur)
			return -1;
		int32 ecarts = 0;
		for (uint32 y = 0; y < hauteur; ++y)
			for (uint32 x = 0; x < largeur; ++x)
				if (static_cast<uint32>(mots[debut + 2u + static_cast<usize>(y) * largeur + x]) != Attendu(x, y))
					++ecarts;
		return ecarts;
	}

	// Deux tailles, la premiere NON carree (largeur != hauteur).
	constexpr uint32 kL1 = 5, kH1 = 3, kL2 = 16, kH2 = 16;
	constexpr usize kN1 = 2u + kL1 * kH1;
	constexpr usize kN2 = 2u + kL2 * kH2;

} // namespace

TEST_CASE(NKWindowIconeX11, X1_X2_Emballage) {
	NkVector<NkWindowIconImage> images;
	images.PushBack(Fabriquer(kL1, kH1));
	images.PushBack(Fabriquer(kL2, kH2));
	NkVector<uint32> cardinaux;

	ASSERT_EQUAL(2u, NkX11EmballerIcones(images, 1u << 20, cardinaux));
	ASSERT_EQUAL(static_cast<uint32>(kN1 + kN2), static_cast<uint32>(cardinaux.Size()));
	if (cardinaux.Size() == kN1 + kN2) {
		ASSERT_EQUAL(0, Ecarts(cardinaux.Data(), 0u, kL1, kH1));
		ASSERT_EQUAL(0, Ecarts(cardinaux.Data(), kN1, kL2, kH2));
	}
}

TEST_CASE(NKWindowIconeX11, X3_ImageInvalideSautee) {
	NkVector<NkWindowIconImage> images;
	NkWindowIconImage courte = Fabriquer(8u, 8u);
	courte.pixels.Resize(8u * 8u * 4u - 4u); // un pixel de moins que ce qu'elle annonce
	images.PushBack(courte);
	images.PushBack(Fabriquer(kL1, kH1));
	NkVector<uint32> cardinaux;

	ASSERT_EQUAL(1u, NkX11EmballerIcones(images, 1u << 20, cardinaux));
	ASSERT_EQUAL(static_cast<uint32>(kN1), static_cast<uint32>(cardinaux.Size()));
	if (cardinaux.Size() == kN1) {
		ASSERT_EQUAL(0, Ecarts(cardinaux.Data(), 0u, kL1, kH1));
	}
}

TEST_CASE(NKWindowIconeX11, X4_Plafond) {
	NkVector<NkWindowIconImage> images;
	images.PushBack(Fabriquer(kL2, kH2)); // 258 cardinaux : depasse
	images.PushBack(Fabriquer(kL1, kH1)); // 17 cardinaux : tient
	NkVector<uint32> cardinaux;

	ASSERT_EQUAL(1u, NkX11EmballerIcones(images, 100u, cardinaux));
	ASSERT_EQUAL(static_cast<uint32>(kN1), static_cast<uint32>(cardinaux.Size()));
	if (cardinaux.Size() == kN1) {
		ASSERT_EQUAL(0, Ecarts(cardinaux.Data(), 0u, kL1, kH1));
	}
	// Rien ne tient : rien n'est emballe, et le tableau est vide.
	ASSERT_EQUAL(0u, NkX11EmballerIcones(images, 10u, cardinaux));
	ASSERT_EQUAL(0u, static_cast<uint32>(cardinaux.Size()));
}

namespace {

	/// Une fenetre de banc, INVISIBLE. `avecIcone` : les deux images de (x2).
	struct NkFenetreIcone {
			NkWindow fenetre;
			bool ok = false;

			explicit NkFenetreIcone(bool avecIcone) {
				NkWESystem::Instance().Initialise();
				NkWindowConfig cfg;
				cfg.title = "NKWindow_Tests icone";
				cfg.width = 320;
				cfg.height = 200;
				cfg.visible = false; // jamais affichee : elle ne prend le focus a personne
				if (avecIcone) {
					cfg.iconImages.PushBack(Fabriquer(kL1, kH1));
					cfg.iconImages.PushBack(Fabriquer(kL2, kH2));
				}
				ok = fenetre.Create(cfg);
			}

			~NkFenetreIcone() {
				if (ok)
					fenetre.Close();
			}
	};

	/// Lit _NET_WM_ICON. Rend le nombre d'elements, 0 si la propriete n'existe pas.
	/// `mots` est a rendre par XFree.
	unsigned long LireIcone(NkWindow &fenetre, Atom &type, int &format, unsigned long &reste, unsigned char *&mots) {
		::Display *d = fenetre.mData.mDisplay;
		const Atom atome = XInternAtom(d, "_NET_WM_ICON", kXFaux);
		unsigned long n = 0;
		type = kXAucun;
		format = 0;
		reste = 0;
		mots = nullptr;
		XSync(d, kXFaux);
		if (XGetWindowProperty(d, fenetre.mData.mXid, atome, 0, 1L << 20, kXFaux, AnyPropertyType, &type, &format, &n,
							   &reste, &mots) != kXSucces)
			return 0;
		return n;
	}

} // namespace

TEST_CASE(NKWindowIconeX11, X5_LaFenetrePorteSonIcone) {
	NkFenetreIcone f(true);
	ASSERT_TRUE_MSG(f.ok, "fenetre non creee : pas de serveur X ? Lancer sous xvfb-run -a");
	if (!f.ok)
		return;

	Atom type = kXAucun;
	int format = 0;
	unsigned long reste = 0;
	unsigned char *donnees = nullptr;
	const unsigned long n = LireIcone(f.fenetre, type, format, reste, donnees);

	ASSERT_TRUE_MSG(type == XA_CARDINAL, "_NET_WM_ICON absente ou d'un autre type que CARDINAL");
	ASSERT_EQUAL(32, format);
	ASSERT_EQUAL(0u, static_cast<uint32>(reste));
	ASSERT_EQUAL(static_cast<uint32>(kN1 + kN2), static_cast<uint32>(n));
	if (donnees && type == XA_CARDINAL && format == 32 && n == kN1 + kN2) {
		// Xlib rend un format 32 en `long` (8 octets sous LP64).
		const unsigned long *mots = reinterpret_cast<const unsigned long *>(donnees);
		ASSERT_EQUAL(0, Ecarts(mots, 0u, kL1, kH1));
		ASSERT_EQUAL(0, Ecarts(mots, kN1, kL2, kH2));
	}
	if (donnees)
		XFree(donnees);
}

TEST_CASE(NKWindowIconeX11, X6_SansImagesPasDePropriete) {
	NkFenetreIcone f(false);
	ASSERT_TRUE_MSG(f.ok, "fenetre non creee : pas de serveur X ? Lancer sous xvfb-run -a");
	if (!f.ok)
		return;

	Atom type = kXAucun;
	int format = 0;
	unsigned long reste = 0;
	unsigned char *donnees = nullptr;
	const unsigned long n = LireIcone(f.fenetre, type, format, reste, donnees);

	ASSERT_EQUAL(0u, static_cast<uint32>(n));
	ASSERT_TRUE(type == kXAucun);
	if (donnees)
		XFree(donnees);
}

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XLIB
