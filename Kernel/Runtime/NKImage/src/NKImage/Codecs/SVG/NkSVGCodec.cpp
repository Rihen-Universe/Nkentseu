// =============================================================================
// NkSVGCodec.cpp
// -----------------------------------------------------------------------------
// Reecriture complete from scratch 2026-05-19, inspiree de NanoSVG par
// Mikko Mononen (sans inclure la lib). Single-file rasterizer SVG -> NkImage RGBA.
//
// Architecture :
//   1. Parser XML stream-based avec stack pour <g> (push/pop style + transform).
//   2. Chaque shape produit une Shape interne (path polyligne apres flatten,
//      style cumule, CTM cumule deja applique aux points).
//   3. Rasterizer scanline avec supersample 2x AA, fill-rule nonzero/evenodd,
//      alpha blending sur l'image RGBA32 destination.
//
// Features supportees :
//   - <svg> avec viewBox + width/height
//   - <g> avec transform cascade correcte (BUG fixe vs ancienne version)
//   - <path> M L H V C S Q T A Z (absolu/relatif) avec text-to-path 24+ KB OK
//   - <rect> <circle> <ellipse> <line> <polyline> <polygon>
//   - Cubic + quadratic Bezier flatten adaptatif
//   - Arc elliptique conversion W3C SVG 1.1 Appendix F.6
//   - fill, opacity, fill-opacity, fill-rule
//   - STROKE RASTERISE : stroke, stroke-width, stroke-opacity, stroke-linecap
//     (butt/round/square), stroke-linejoin (miter/round/bevel), stroke-miterlimit.
//     Le trait est converti en contour plein (BuildStrokeShape) puis rempli.
//   - Gradients lineaires et radiaux (objectBoundingBox / userSpaceOnUse)
//   - transform: translate, scale, rotate, matrix
//   - Couleurs : #RGB #RRGGBB rgb() rgba() + 148 noms CSS
//   - Attribut style="..." CSS inline
//
// Non implemente :
//   - <text> <tspan> (necessite font system)
//   - <defs><style> avec class CSS
//   - stroke-dasharray (le trait est toujours continu)
//   - patterns, masks, clip-path, filters
//   - <use> <symbol> <image>
//
// ATTENTION : cette liste a menti pendant un temps. Elle annoncait « Stroke
// (uniquement fill rendu pour Phase 1) » et « Gradients » comme absents, alors
// que les deux etaient implementes -- au point de faire dessiner des icones
// entieres en contours evides pour contourner une limite qui n'existait plus
// (13 aout 2026). Une capacite qu'on ajoute se declare ICI dans le meme
// changement, sans quoi personne ne s'en sert.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================

#include "NKImage/Codecs/SVG/NkSVGCodec.h"
#include "NKImage/Core/NkImage.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NkAllocator.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/Encoding/NkBase64.h"
#include "NKCore/Text/NkIGlyphSource.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <new>

namespace nkentseu {

	using namespace nkentseu::memory;

	namespace { // file-local helpers (anonymous namespace)

		// ─────────────────────────────────────────────────────────────────────────────
		// SECTION 1 — Helpers parsing texte
		// ─────────────────────────────────────────────────────────────────────────────

		inline bool IsSpace(char c) noexcept {
			return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
		}

		inline bool IsDigit(char c) noexcept {
			return c >= '0' && c <= '9';
		}

		inline const char *SkipWS(const char *p) noexcept {
			while (*p && IsSpace(*p))
				++p;
			return p;
		}

		inline const char *SkipWSComma(const char *p) noexcept {
			while (*p && (IsSpace(*p) || *p == ','))
				++p;
			return p;
		}

		/// Parse un float style strtod (sans locale). Avance @p end si fourni.
		float32 ParseFloat(const char *s, const char **endOut = nullptr) noexcept {
			const char *p = s ? SkipWS(s) : nullptr;
			if (!p) {
				if (endOut)
					*endOut = nullptr;
				return 0.f;
			}
			float32 sign = 1.f;
			if (*p == '+')
				++p;
			else if (*p == '-') {
				sign = -1.f;
				++p;
			}
			float64 v = 0.0;
			while (IsDigit(*p)) {
				v = v * 10.0 + (float64)(*p - '0');
				++p;
			}
			if (*p == '.') {
				++p;
				float64 f = 0.1;
				while (IsDigit(*p)) {
					v += (float64)(*p - '0') * f;
					f *= 0.1;
					++p;
				}
			}
			if (*p == 'e' || *p == 'E') {
				++p;
				int32 esign = 1, e = 0;
				if (*p == '+')
					++p;
				else if (*p == '-') {
					esign = -1;
					++p;
				}
				while (IsDigit(*p)) {
					e = e * 10 + (*p - '0');
					++p;
				}
				v *= std::pow(10.0, (float64)(esign * e));
			}
			if (endOut)
				*endOut = p;
			return (float32)(sign * v);
		}

		/// Strcasecmp portable (les noms CSS sont insensibles a la casse).
		int32 StrCaseCmp(const char *a, const char *b) noexcept {
			while (*a && *b) {
				const char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
				const char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
				if (ca != cb)
					return ca - cb;
				++a;
				++b;
			}
			return *a - *b;
		}

		/// Decode UN point de code UTF-8 et avance @p p. 0 en fin ou sur octet
		/// invalide. (Il venait de NKFont ; le codec ne depend plus de NKFont, et
		/// douze lignes valent mieux qu'une arete entre deux modules.)
		uint32 DecoderUTF8(const char *&p, const char *end) noexcept {
			if (p >= end)
				return 0u;
			const uint8 c0 = (uint8)*p++;
			if (c0 < 0x80u)
				return c0;
			int32 suite = 0;
			uint32 cp = 0;
			if ((c0 & 0xE0u) == 0xC0u) {
				cp = c0 & 0x1Fu;
				suite = 1;
			} else if ((c0 & 0xF0u) == 0xE0u) {
				cp = c0 & 0x0Fu;
				suite = 2;
			} else if ((c0 & 0xF8u) == 0xF0u) {
				cp = c0 & 0x07u;
				suite = 3;
			} else {
				return 0xFFFDu; // octet de continuation isole : le caractere de remplacement
			}
			for (int32 i = 0; i < suite; ++i) {
				if (p >= end)
					return 0xFFFDu;
				const uint8 c = (uint8)*p;
				if ((c & 0xC0u) != 0x80u)
					return 0xFFFDu;
				cp = (cp << 6) | (uint32)(c & 0x3Fu);
				++p;
			}
			return cp;
		}

		/// Encode un point de code en UTF-8. @return le nombre d'octets ecrits.
		int32 EncoderUTF8(uint32 cp, char *out, int32 outSz) noexcept {
			if (cp < 0x80u && outSz >= 1) {
				out[0] = (char)cp;
				return 1;
			}
			if (cp < 0x800u && outSz >= 2) {
				out[0] = (char)(0xC0u | (cp >> 6));
				out[1] = (char)(0x80u | (cp & 0x3Fu));
				return 2;
			}
			if (cp < 0x10000u && outSz >= 3) {
				out[0] = (char)(0xE0u | (cp >> 12));
				out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
				out[2] = (char)(0x80u | (cp & 0x3Fu));
				return 3;
			}
			if (outSz >= 4) {
				out[0] = (char)(0xF0u | (cp >> 18));
				out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
				out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
				out[3] = (char)(0x80u | (cp & 0x3Fu));
				return 4;
			}
			return 0;
		}

		int32 HexDigit(char c) noexcept {
			if (c >= '0' && c <= '9')
				return c - '0';
			if (c >= 'a' && c <= 'f')
				return c - 'a' + 10;
			if (c >= 'A' && c <= 'F')
				return c - 'A' + 10;
			return -1;
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// SECTION 2 — Table des noms de couleurs CSS (148 noms standards)
		// ─────────────────────────────────────────────────────────────────────────────

		struct NamedColor {
				const char *name;
				uint8 r, g, b;
		};

		const NamedColor kNamedColors[] = {
			{"aliceblue", 240, 248, 255},
			{"antiquewhite", 250, 235, 215},
			{"aqua", 0, 255, 255},
			{"aquamarine", 127, 255, 212},
			{"azure", 240, 255, 255},
			{"beige", 245, 245, 220},
			{"bisque", 255, 228, 196},
			{"black", 0, 0, 0},
			{"blanchedalmond", 255, 235, 205},
			{"blue", 0, 0, 255},
			{"blueviolet", 138, 43, 226},
			{"brown", 165, 42, 42},
			{"burlywood", 222, 184, 135},
			{"cadetblue", 95, 158, 160},
			{"chartreuse", 127, 255, 0},
			{"chocolate", 210, 105, 30},
			{"coral", 255, 127, 80},
			{"cornflowerblue", 100, 149, 237},
			{"cornsilk", 255, 248, 220},
			{"crimson", 220, 20, 60},
			{"cyan", 0, 255, 255},
			{"darkblue", 0, 0, 139},
			{"darkcyan", 0, 139, 139},
			{"darkgoldenrod", 184, 134, 11},
			{"darkgray", 169, 169, 169},
			{"darkgreen", 0, 100, 0},
			{"darkgrey", 169, 169, 169},
			{"darkkhaki", 189, 183, 107},
			{"darkmagenta", 139, 0, 139},
			{"darkolivegreen", 85, 107, 47},
			{"darkorange", 255, 140, 0},
			{"darkorchid", 153, 50, 204},
			{"darkred", 139, 0, 0},
			{"darksalmon", 233, 150, 122},
			{"darkseagreen", 143, 188, 143},
			{"darkslateblue", 72, 61, 139},
			{"darkslategray", 47, 79, 79},
			{"darkslategrey", 47, 79, 79},
			{"darkturquoise", 0, 206, 209},
			{"darkviolet", 148, 0, 211},
			{"deeppink", 255, 20, 147},
			{"deepskyblue", 0, 191, 255},
			{"dimgray", 105, 105, 105},
			{"dimgrey", 105, 105, 105},
			{"dodgerblue", 30, 144, 255},
			{"firebrick", 178, 34, 34},
			{"floralwhite", 255, 250, 240},
			{"forestgreen", 34, 139, 34},
			{"fuchsia", 255, 0, 255},
			{"gainsboro", 220, 220, 220},
			{"ghostwhite", 248, 248, 255},
			{"gold", 255, 215, 0},
			{"goldenrod", 218, 165, 32},
			{"gray", 128, 128, 128},
			{"green", 0, 128, 0},
			{"greenyellow", 173, 255, 47},
			{"grey", 128, 128, 128},
			{"honeydew", 240, 255, 240},
			{"hotpink", 255, 105, 180},
			{"indianred", 205, 92, 92},
			{"indigo", 75, 0, 130},
			{"ivory", 255, 255, 240},
			{"khaki", 240, 230, 140},
			{"lavender", 230, 230, 250},
			{"lavenderblush", 255, 240, 245},
			{"lawngreen", 124, 252, 0},
			{"lemonchiffon", 255, 250, 205},
			{"lightblue", 173, 216, 230},
			{"lightcoral", 240, 128, 128},
			{"lightcyan", 224, 255, 255},
			{"lightgoldenrodyellow", 250, 250, 210},
			{"lightgray", 211, 211, 211},
			{"lightgreen", 144, 238, 144},
			{"lightgrey", 211, 211, 211},
			{"lightpink", 255, 182, 193},
			{"lightsalmon", 255, 160, 122},
			{"lightseagreen", 32, 178, 170},
			{"lightskyblue", 135, 206, 250},
			{"lightslategray", 119, 136, 153},
			{"lightslategrey", 119, 136, 153},
			{"lightsteelblue", 176, 196, 222},
			{"lightyellow", 255, 255, 224},
			{"lime", 0, 255, 0},
			{"limegreen", 50, 205, 50},
			{"linen", 250, 240, 230},
			{"magenta", 255, 0, 255},
			{"maroon", 128, 0, 0},
			{"mediumaquamarine", 102, 205, 170},
			{"mediumblue", 0, 0, 205},
			{"mediumorchid", 186, 85, 211},
			{"mediumpurple", 147, 112, 219},
			{"mediumseagreen", 60, 179, 113},
			{"mediumslateblue", 123, 104, 238},
			{"mediumspringgreen", 0, 250, 154},
			{"mediumturquoise", 72, 209, 204},
			{"mediumvioletred", 199, 21, 133},
			{"midnightblue", 25, 25, 112},
			{"mintcream", 245, 255, 250},
			{"mistyrose", 255, 228, 225},
			{"moccasin", 255, 228, 181},
			{"navajowhite", 255, 222, 173},
			{"navy", 0, 0, 128},
			{"oldlace", 253, 245, 230},
			{"olive", 128, 128, 0},
			{"olivedrab", 107, 142, 35},
			{"orange", 255, 165, 0},
			{"orangered", 255, 69, 0},
			{"orchid", 218, 112, 214},
			{"palegoldenrod", 238, 232, 170},
			{"palegreen", 152, 251, 152},
			{"paleturquoise", 175, 238, 238},
			{"palevioletred", 219, 112, 147},
			{"papayawhip", 255, 239, 213},
			{"peachpuff", 255, 218, 185},
			{"peru", 205, 133, 63},
			{"pink", 255, 192, 203},
			{"plum", 221, 160, 221},
			{"powderblue", 176, 224, 230},
			{"purple", 128, 0, 128},
			{"red", 255, 0, 0},
			{"rosybrown", 188, 143, 143},
			{"royalblue", 65, 105, 225},
			{"saddlebrown", 139, 69, 19},
			{"salmon", 250, 128, 114},
			{"sandybrown", 244, 164, 96},
			{"seagreen", 46, 139, 87},
			{"seashell", 255, 245, 238},
			{"sienna", 160, 82, 45},
			{"silver", 192, 192, 192},
			{"skyblue", 135, 206, 235},
			{"slateblue", 106, 90, 205},
			{"slategray", 112, 128, 144},
			{"slategrey", 112, 128, 144},
			{"snow", 255, 250, 250},
			{"springgreen", 0, 255, 127},
			{"steelblue", 70, 130, 180},
			{"tan", 210, 180, 140},
			{"teal", 0, 128, 128},
			{"thistle", 216, 191, 216},
			{"tomato", 255, 99, 71},
			{"turquoise", 64, 224, 208},
			{"violet", 238, 130, 238},
			{"wheat", 245, 222, 179},
			{"white", 255, 255, 255},
			{"whitesmoke", 245, 245, 245},
			{"yellow", 255, 255, 0},
			{"yellowgreen", 154, 205, 50},
		};
		constexpr int32 kNamedColorsCount = (int32)(sizeof(kNamedColors) / sizeof(kNamedColors[0]));

	} // anonymous namespace

	// ─────────────────────────────────────────────────────────────────────────────
	// SECTION 3 — NkSVGColor / NkSVGTransform (API publique du header)
	// ─────────────────────────────────────────────────────────────────────────────

	NkSVGColor NkSVGColor::Parse(const char *str) noexcept {
		if (!str)
			return Black();
		while (*str && IsSpace(*str))
			++str;
		if (!*str)
			return Black();
		if (StrCaseCmp(str, "none") == 0)
			return None();
		if (StrCaseCmp(str, "transparent") == 0)
			return Transparent();
		if (StrCaseCmp(str, "currentColor") == 0)
			return Black(); // pas de cascade currentColor

		// #RGB / #RRGGBB / #RRGGBBAA
		if (*str == '#') {
			++str;
			int32 len = 0;
			while (str[len] && HexDigit(str[len]) >= 0)
				++len;
			NkSVGColor c;
			c.a = 255;
			if (len == 3) {
				c.r = (uint8)(HexDigit(str[0]) * 17);
				c.g = (uint8)(HexDigit(str[1]) * 17);
				c.b = (uint8)(HexDigit(str[2]) * 17);
			} else if (len == 6 || len == 8) {
				c.r = (uint8)(HexDigit(str[0]) * 16 + HexDigit(str[1]));
				c.g = (uint8)(HexDigit(str[2]) * 16 + HexDigit(str[3]));
				c.b = (uint8)(HexDigit(str[4]) * 16 + HexDigit(str[5]));
				if (len == 8) {
					c.a = (uint8)(HexDigit(str[6]) * 16 + HexDigit(str[7]));
				}
			}
			return c;
		}

		// rgb(r,g,b) / rgba(r,g,b,a)
		if (str[0] == 'r' && str[1] == 'g' && str[2] == 'b') {
			const char *p = str + 3;
			if (*p == 'a')
				++p;
			while (*p && *p != '(')
				++p;
			if (*p == '(')
				++p;
			const float32 r = ParseFloat(p, &p);
			p = SkipWSComma(p);
			const float32 g = ParseFloat(p, &p);
			p = SkipWSComma(p);
			const float32 b = ParseFloat(p, &p);
			p = SkipWSComma(p);
			float32 a = 1.f;
			if (*p && *p != ')')
				a = ParseFloat(p, &p);
			const auto clamp255 = [](float32 v) -> uint8 {
				if (v < 0.f)
					return 0;
				if (v > 255.f)
					return 255;
				return (uint8)v;
			};
			NkSVGColor c;
			c.r = clamp255(r);
			c.g = clamp255(g);
			c.b = clamp255(b);
			c.a = (a <= 1.f) ? clamp255(a * 255.f) : clamp255(a);
			return c;
		}

		// Nom CSS (recherche lineaire, ~150 entrees).
		for (int32 i = 0; i < kNamedColorsCount; ++i) {
			if (StrCaseCmp(str, kNamedColors[i].name) == 0) {
				return {kNamedColors[i].r, kNamedColors[i].g, kNamedColors[i].b, 255, false};
			}
		}
		return Black();
	}

	NkSVGTransform NkSVGTransform::Rotate(float32 deg) noexcept {
		const float32 r = deg * 0.017453292519943295f;
		const float32 c = std::cos(r), s = std::sin(r);
		return {c, s, -s, c, 0, 0};
	}

	NkSVGTransform NkSVGTransform::operator*(const NkSVGTransform &o) const noexcept {
		// (*this) * o : applique o d'abord, puis (*this).
		// En forme matricielle :
		//   [a c e]   [oa oc oe]
		//   [b d f] x [ob od of]
		//   [0 0 1]   [0  0  1 ]
		NkSVGTransform r;
		r.a = a * o.a + c * o.b;
		r.b = b * o.a + d * o.b;
		r.c = a * o.c + c * o.d;
		r.d = b * o.c + d * o.d;
		r.e = a * o.e + c * o.f + e;
		r.f = b * o.e + d * o.f + f;
		return r;
	}

	NkSVGTransform NkSVGTransform::Parse(const char *str) noexcept {
		NkSVGTransform result = Identity();
		if (!str)
			return result;
		const char *p = str;
		while (*p) {
			p = SkipWS(p);
			if (!*p)
				break;
			// Lit le nom de l'op (matrix/translate/scale/rotate/skewX/skewY).
			char op[16] = {0};
			int32 oi = 0;
			while (*p && *p != '(' && !IsSpace(*p) && oi < 15)
				op[oi++] = *p++;
			op[oi] = 0;
			p = SkipWS(p);
			if (*p != '(') {
				if (*p)
					++p;
				continue;
			}
			++p;
			// Lit jusqu'a 6 floats.
			float32 args[6] = {0};
			int32 nargs = 0;
			while (*p && *p != ')' && nargs < 6) {
				p = SkipWSComma(p);
				if (*p == ')')
					break;
				args[nargs++] = ParseFloat(p, &p);
			}
			if (*p == ')')
				++p;

			NkSVGTransform t = Identity();
			if (std::strcmp(op, "matrix") == 0 && nargs >= 6) {
				t.a = args[0];
				t.b = args[1];
				t.c = args[2];
				t.d = args[3];
				t.e = args[4];
				t.f = args[5];
			} else if (std::strcmp(op, "translate") == 0) {
				t = Translate(args[0], nargs > 1 ? args[1] : 0.f);
			} else if (std::strcmp(op, "scale") == 0) {
				t = Scale(args[0], nargs > 1 ? args[1] : args[0]);
			} else if (std::strcmp(op, "rotate") == 0) {
				if (nargs >= 3) {
					// rotate(angle cx cy) = T(cx,cy) * R(angle) * T(-cx,-cy)
					t = Translate(args[1], args[2]) * Rotate(args[0]) * Translate(-args[1], -args[2]);
				} else {
					t = Rotate(args[0]);
				}
			} else if (std::strcmp(op, "skewX") == 0) {
				const float32 rd = args[0] * 0.017453292519943295f;
				t.a = 1.f;
				t.b = 0.f;
				t.c = std::tan(rd);
				t.d = 1.f;
			} else if (std::strcmp(op, "skewY") == 0) {
				const float32 rd = args[0] * 0.017453292519943295f;
				t.a = 1.f;
				t.b = std::tan(rd);
				t.c = 0.f;
				t.d = 1.f;
			}
			// SVG : "transform=A B C" applique A puis B puis C, lecture
			// gauche-droite. Donc result = result * t.
			result = result * t;
		}
		return result;
	}

	// ═════════════════════════════════════════════════════════════════════════════
	// SECTION 4 — Structures internes : Shape + PathBuilder + ParseState
	// ═════════════════════════════════════════════════════════════════════════════

	namespace {

		// ── Gradients (linear / radial) ───────────────────────────────────────────────
		enum class GradKind : uint8 { Linear = 0, Radial };
		enum class GradSpread : uint8 { Pad = 0, Reflect, Repeat };

		struct GradStop {
				float32 offset = 0.f; // 0..1
				NkSVGColor color;	  // color.a inclut deja stop-opacity
		};

		struct Gradient {
				char id[64] = {0};
				char href[64] = {0}; // gradient reference (stops herites)
				GradKind kind = GradKind::Linear;
				GradSpread spread = GradSpread::Pad;
				bool userSpace = false;							   // false = objectBoundingBox (defaut)
				NkSVGTransform xform = NkSVGTransform::Identity(); // gradientTransform
				float32 x1 = 0.f, y1 = 0.f, x2 = 1.f, y2 = 0.f;	   // linear (defaut bbox 0..1)
				float32 cx = 0.5f, cy = 0.5f, r = 0.5f;			   // radial
				float32 fx = 0.5f, fy = 0.5f;					   // radial : le FOYER (defaut = centre)
				bool hasFocal = false;							   // fx/fy ecrits par l'auteur ?
				// CE QUI A ETE ECRIT, attribut par attribut. Un gradient qui en
				// reference un autre (href) n'herite QUE ce qu'il n'a pas dit lui-meme :
				// sans ces drapeaux on ne peut pas distinguer « absent » de « pose a la
				// valeur par defaut », et l'heritage devient faux dans un cas sur deux.
				bool hasX1 = false, hasY1 = false, hasX2 = false, hasY2 = false;
				bool hasCx = false, hasCy = false, hasR = false;
				bool hasUnits = false, hasSpread = false, hasXform = false;
				NkVector<GradStop> stops;

				Gradient() = default;
				Gradient(Gradient &&) noexcept = default;
				Gradient &operator=(Gradient &&) noexcept = default;
				Gradient(const Gradient &) = delete;
				Gradient &operator=(const Gradient &) = delete;
		};

		// ── L'AJUSTEMENT D'UNE IMAGE DANS SA BOITE (preserveAspectRatio) ──────
		enum class FitKind : uint8 {
			Meet = 0, ///< tient EN ENTIER dans la boite, des bandes vides restent
			Slice,	  ///< COUVRE la boite, ce qui depasse est coupe
			None	  ///< etire aux deux dimensions, le rapport est perdu
		};

		/// L'ALIGNEMENT de preserveAspectRatio : `xMinYMax`, `xMidYMid`... Neuf
		/// combinaisons, et elles comptent -- une icone calee en haut a gauche et la
		/// meme centree ne sont pas au meme endroit. Ne gerer que « Mid » revenait a
		/// recentrer silencieusement ce que le fichier voulait caler dans un coin.
		enum class Align : uint8 { Min = 0, Mid, Max };

		// ═════════════════════════════════════════════════════════════════════════════
		// <filter> — UN GRAPHE, PAS UNE LISTE DE CAS PARTICULIERS
		// -----------------------------------------------------------------------------
		// Un filtre SVG est un petit graphe de flots : chaque primitive lit une ou
		// deux images (`in`, `in2` -- nommees, ou implicitement la sortie de la
		// precedente), en produit une, et peut la nommer (`result`) pour qu'une autre
		// la reprenne. `feDropShadow` n'est qu'un raccourci pour flou + decalage +
		// teinte + composition.
		//
		// ECRIRE LE GRAPHE PLUTOT QUE DES CAS : une implementation « si c'est une
		// ombre, faire ceci » traite le cas frequent et laisse les autres muets pour
		// toujours -- et le jour ou deux primitives se suivent, elle n'a rien a dire.
		// Avec le graphe, chaque primitive ajoutee vaut pour toutes les combinaisons.
		// ═════════════════════════════════════════════════════════════════════════════
		enum class PrimType : uint8 {
			Inconnue = 0,
			Flou,		 ///< feGaussianBlur
			Decalage,	 ///< feOffset
			Aplat,		 ///< feFlood
			Composition, ///< feComposite
			Matrice,	 ///< feColorMatrix
			Fusion,		 ///< feBlend
			Assemblage,	 ///< feMerge (+ feMergeNode)
			Ombre		 ///< feDropShadow (raccourci)
		};

		/// L'operateur de feComposite (Porter-Duff) ou de feBlend.
		enum class OpComposite : uint8 { Over = 0, In, Out, Atop, Xor, Arithmetique };
		enum class OpFusion : uint8 { Normal = 0, Multiplier, Ecran, Assombrir, Eclaircir };

		struct Primitive {
				PrimType type = PrimType::Inconnue;
				char in[32] = {0};	   ///< vide = la sortie de la primitive precedente
				char in2[32] = {0};
				char result[32] = {0}; ///< vide = anonyme (seule la suivante la lit)
				float32 ecartX = 0.f, ecartY = 0.f;
				float32 dx = 0.f, dy = 0.f;
				NkSVGColor couleur = NkSVGColor::Black();
				OpComposite op = OpComposite::Over;
				OpFusion fusion = OpFusion::Normal;
				float32 k1 = 0.f, k2 = 0.f, k3 = 0.f, k4 = 0.f;
				float32 mat[20] = {};
				bool aMatrice = false;
				char merges[8][32] = {};
				int32 nMerges = 0;
		};

		struct Filtre {
				char id[64] = {0};
				NkVector<Primitive> prims;

				Filtre() = default;
				Filtre(Filtre &&) noexcept = default;
				Filtre &operator=(Filtre &&) noexcept = default;
				Filtre(const Filtre &) = delete;
				Filtre &operator=(const Filtre &) = delete;

				void Vider() noexcept {
					id[0] = 0;
					while (!prims.IsEmpty())
						prims.PopBack();
				}
		};

		/// Shape interne = un path flatten en polyligne(s), + style cumule + CTM applique.
		/// Les xs/ys sont en COORDONNEES DESTINATION (apres ctm + view->out scaling).
		/// Une shape peut aussi porter une IMAGE (<image>) : ses quatre coins sont
		/// alors le contour, et `img` les pixels a poser dedans. Les deux vivent dans
		/// LE MEME vecteur, dans l'ordre du document -- c'est ce qui garde l'ordre de
		/// peinture entre formes et images (deux listes l'auraient perdu).
		struct Shape {
				NkSVGStyle style;
				NkVector<float32> xs;
				NkVector<float32> ys;
				NkVector<int32> contourStart; // index dans xs/ys
				NkVector<int32> contourLen;
				NkSVGTransform ctm = NkSVGTransform::Identity(); // CTM applique (gradients userSpace)
				char fillRef[64] = {0};							 // id de gradient si fill="url(#id)"
				char strokeRef[64] = {0};						 // id de gradient si stroke="url(#id)"

				// ── <image> ──────────────────────────────────────────────────
				NkImage img;						 ///< pixels decodes ; invalide = ce n'est pas une image
				float32 ix = 0, iy = 0, iw = 0, ih = 0; ///< la boite, en espace UTILISATEUR (avant ctm)
				FitKind fit = FitKind::Meet;
				Align alignX = Align::Mid, alignY = Align::Mid;

				// ── le filtre du groupe qui porte cette forme ────────────────
				char filterRef[64] = {0};
				int32 filterInst = 0; ///< numero D'INSTANCE du groupe filtre (0 = aucun)

				// ── les decoupages herites, du plus exterieur au plus interieur ──
				//    Plusieurs, parce qu'ils s'INTERSECTENT : un <g clip-path> dans un
				//    autre <g clip-path> ne garde que ce que les deux gardent.
				static constexpr int32 kMaxClips = 4;
				char clipRefs[kMaxClips][64] = {};
				int32 nClips = 0;

				bool EstImage() const noexcept {
					return img.IsValid();
				}

				Shape() = default;
				// Move uniquement (les NkVector peuvent etre couteux a copier).
				Shape(Shape &&) noexcept = default;
				Shape &operator=(Shape &&) noexcept = default;
				Shape(const Shape &) = delete;
				Shape &operator=(const Shape &) = delete;
		};

		// ─────────────────────────────────────────────────────────────────────────────
		// <clipPath> — UN DECOUPAGE EST UNE MULTIPLICATION D'ALPHA, PAS UN CALQUE
		// -----------------------------------------------------------------------------
		// Contrairement a une ombre (qui a besoin de voir le GROUPE entier avant de
		// pouvoir etre calculee), un decoupage se decide PIXEL PAR PIXEL et il est
		// idempotent. Il n'a donc besoin d'aucun calque intermediaire : on multiplie
		// la couverture de chaque forme par un masque, au moment ou on la peint.
		// Consequence agreable : les clips IMBRIQUES se composent tout seuls (leur
		// intersection est le produit des masques), sans pile de calques a tenir.
		// ─────────────────────────────────────────────────────────────────────────────
		// ─────────────────────────────────────────────────────────────────────────────
		// <pattern> — UNE TUILE, RASTERISEE UNE FOIS, ECHANTILLONNEE EN MODULO
		// -----------------------------------------------------------------------------
		// Un motif se peint comme un degrade : on ne transporte pas la geometrie vers
		// les pixels, on ramene CHAQUE PIXEL dans l'espace du motif (matrice inverse),
		// puis on prend le reste de la division par la periode. Une seule tuile est
		// rasterisee, quelle que soit la surface a couvrir -- carreler en repetant le
		// rendu des formes couterait le nombre de repetitions.
		// ─────────────────────────────────────────────────────────────────────────────
		struct Motif {
				char id[64] = {0};
				char href[64] = {0};   ///< un pattern peut heriter d'un autre
				float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
				bool userSpace = false;			  ///< patternUnits (defaut objectBoundingBox)
				bool contenuUserSpace = true;	  ///< patternContentUnits (defaut userSpaceOnUse)
				bool aViewBox = false;
				float32 vbX = 0.f, vbY = 0.f, vbW = 0.f, vbH = 0.f;
				NkSVGTransform xform = NkSVGTransform::Identity(); ///< patternTransform
				NkVector<Shape> formes;

				Motif() = default;
				Motif(Motif &&) noexcept = default;
				Motif &operator=(Motif &&) noexcept = default;
				Motif(const Motif &) = delete;
				Motif &operator=(const Motif &) = delete;

				void Vider() noexcept {
					id[0] = 0;
					href[0] = 0;
					x = y = w = h = 0.f;
					userSpace = false;
					contenuUserSpace = true;
					aViewBox = false;
					xform = NkSVGTransform::Identity();
					while (!formes.IsEmpty())
						formes.PopBack();
				}
		};

		// <mask> EST LE MEME MECANISME, a une chose pres : un decoupage ne connait
		// que DEDANS ou DEHORS (la couverture des formes), tandis qu'un masque prend
		// une valeur CONTINUE -- la LUMINANCE de ce qu'on y peint (SVG 1.1) ou son
		// alpha (mask-type="alpha"). Un degrade du noir au blanc dans un <mask> fait
		// donc un fondu, ce qu'un <clipPath> ne peut pas exprimer. Les deux se
		// multiplient de la meme facon, et se cumulent : une seule liste les porte.
		struct ClipPath {
				char id[64] = {0};
				bool userSpace = true; ///< clipPathUnits ; objectBoundingBox = false
				bool masque = false;   ///< <mask> plutot que <clipPath>
				bool typeAlpha = false; ///< mask-type="alpha" (defaut : luminance)
				NkVector<Shape> formes;

				ClipPath() = default;
				ClipPath(ClipPath &&) noexcept = default;
				ClipPath &operator=(ClipPath &&) noexcept = default;
				ClipPath(const ClipPath &) = delete;
				ClipPath &operator=(const ClipPath &) = delete;

				/// Remise a zero SANS affectation par deplacement : `NkVector<Shape>`
				/// porte des elements move-only et n'expose pas `operator=(&&)`.
				void Vider() noexcept {
					id[0] = 0;
					userSpace = true;
					masque = false;
					typeAlpha = false;
					while (!formes.IsEmpty())
						formes.PopBack();
				}
		};

		// ─────────────────────────────────────────────────────────────────────────────
		// CE QUE LE CODEC SAUTE SE DIT — UNE FOIS PAR NOM, JAMAIS EN SILENCE.
		// Un decodeur qui ignore sans le dire fait croire que le fichier est rendu.
		// Le nom est garde ici (relisible par NkSVGImage::SkippedAt) ET journalise :
		// le test lit la liste, l'humain lit le journal.
		// ─────────────────────────────────────────────────────────────────────────────
		struct SkipList {
				static constexpr int32 kMax = 32;
				char noms[kMax][40] = {};
				int32 nb = 0;

				bool Connu(const char *nom) const noexcept {
					for (int32 i = 0; i < nb; ++i)
						if (std::strcmp(noms[i], nom) == 0)
							return true;
					return false;
				}

				/// @return true si c'est la PREMIERE fois qu'on voit ce nom.
				bool Noter(const char *nom) noexcept {
					if (!nom || !*nom || Connu(nom))
						return false;
					if (nb >= kMax)
						return false;
					std::strncpy(noms[nb], nom, 39);
					noms[nb][39] = 0;
					++nb;
					return true;
				}
		};

		/// State courant pendant le parsing (push/pop sur <g>).
		struct ParseState {
				NkSVGStyle style = {};
				NkSVGTransform xform = NkSVGTransform::Identity();
				char fillRef[64] = {0}; // herite via cascade <g>
				char strokeRef[64] = {0};
				// Le filtre s'herite comme le reste, MAIS avec un numero d'instance :
				// deux groupes freres qui portent le MEME filtre sont deux ombres
				// distinctes, pas une ombre sur leur union.
				char filterRef[64] = {0};
				int32 filterInst = 0;
				char clipRefs[4][64] = {};
				int32 nClips = 0;
		};

		/// Paire (nom, valeur) d'attribut XML. value pointe dans le pool partage.
		struct AttrPair {
				const char *name;
				const char *value;
		};

		/// Trouve l'attribut par nom dans la liste (case sensitive).
		const char *FindAttr(const AttrPair *attrs, int32 n, const char *name) noexcept {
			for (int32 i = 0; i < n; ++i) {
				if (std::strcmp(attrs[i].name, name) == 0)
					return attrs[i].value;
			}
			return nullptr;
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// SECTION 5 — Construction des paths : PathBuilder + Bezier flatten + Arc
		// ─────────────────────────────────────────────────────────────────────────────

		/// Helper qui pousse des segments dans une Shape. Gere les contours (M ouvre,
		/// Z ferme) + curseur courant + reflexion ctrl points (S/T).
		struct PathBuilder {
				Shape &sh;
				float32 curX = 0, curY = 0;		// position du curseur
				float32 startX = 0, startY = 0; // debut du contour courant
				bool inContour = false;

				explicit PathBuilder(Shape &s) noexcept : sh(s) {
				}

				void StartContour(float32 x, float32 y) noexcept {
					if (inContour)
						EndContour();
					sh.contourStart.PushBack((int32)sh.xs.Size());
					sh.contourLen.PushBack(1);
					sh.xs.PushBack(x);
					sh.ys.PushBack(y);
					startX = curX = x;
					startY = curY = y;
					inContour = true;
				}

				void LineTo(float32 x, float32 y) noexcept {
					if (!inContour) {
						StartContour(x, y);
						return;
					}
					sh.xs.PushBack(x);
					sh.ys.PushBack(y);
					sh.contourLen[sh.contourLen.Size() - 1]++;
					curX = x;
					curY = y;
				}

				void EndContour() noexcept {
					inContour = false;
				}

				void Close() noexcept {
					if (!inContour)
						return;
					if (std::fabs(curX - startX) > 1e-5f || std::fabs(curY - startY) > 1e-5f) {
						sh.xs.PushBack(startX);
						sh.ys.PushBack(startY);
						sh.contourLen[sh.contourLen.Size() - 1]++;
					}
					curX = startX;
					curY = startY;
					inContour = false;
				}
		};

		/// Flatten cubique Bezier par subdivision adaptative (De Casteljau).
		/// Tolerance flatness = 0.5 px max d'erreur.
		void FlattenCubic(PathBuilder &pb, float32 x0, float32 y0, float32 x1, float32 y1, float32 x2, float32 y2,
						  float32 x3, float32 y3, int32 depth = 0) noexcept {
			constexpr float32 kTol2 = 0.25f; // (0.5 px)^2
			constexpr int32 kMaxDepth = 12;
			if (depth >= kMaxDepth) {
				pb.LineTo(x3, y3);
				return;
			}
			// Distance des points de controle a la ligne (x0,y0)-(x3,y3).
			const float32 dx = x3 - x0, dy = y3 - y0;
			const float32 d1 = std::fabs((x1 - x3) * dy - (y1 - y3) * dx);
			const float32 d2 = std::fabs((x2 - x3) * dy - (y2 - y3) * dx);
			const float32 len2 = dx * dx + dy * dy;
			if ((d1 + d2) * (d1 + d2) <= kTol2 * len2) {
				pb.LineTo(x3, y3);
				return;
			}
			// Subdivise en 2 par De Casteljau.
			const float32 x01 = (x0 + x1) * 0.5f, y01 = (y0 + y1) * 0.5f;
			const float32 x12 = (x1 + x2) * 0.5f, y12 = (y1 + y2) * 0.5f;
			const float32 x23 = (x2 + x3) * 0.5f, y23 = (y2 + y3) * 0.5f;
			const float32 x012 = (x01 + x12) * 0.5f, y012 = (y01 + y12) * 0.5f;
			const float32 x123 = (x12 + x23) * 0.5f, y123 = (y12 + y23) * 0.5f;
			const float32 xC = (x012 + x123) * 0.5f, yC = (y012 + y123) * 0.5f;
			FlattenCubic(pb, x0, y0, x01, y01, x012, y012, xC, yC, depth + 1);
			FlattenCubic(pb, xC, yC, x123, y123, x23, y23, x3, y3, depth + 1);
		}

		/// Flatten quadratique Bezier (eleve en cubique : c1 = p0+2/3(p1-p0), c2 = p2+2/3(p1-p2)).
		void FlattenQuad(PathBuilder &pb, float32 x0, float32 y0, float32 x1, float32 y1, float32 x2,
						 float32 y2) noexcept {
			const float32 cx1 = x0 + (2.f / 3.f) * (x1 - x0);
			const float32 cy1 = y0 + (2.f / 3.f) * (y1 - y0);
			const float32 cx2 = x2 + (2.f / 3.f) * (x1 - x2);
			const float32 cy2 = y2 + (2.f / 3.f) * (y1 - y2);
			FlattenCubic(pb, x0, y0, cx1, cy1, cx2, cy2, x2, y2);
		}

		/// Convertit un arc elliptique en sequence de Beziers cubiques (W3C SVG 1.1).
		/// @p x1,y1 = position depart, @p x2,y2 = position arrivee.
		void FlattenArc(PathBuilder &pb, float32 x1, float32 y1, float32 rx, float32 ry, float32 angleDeg,
						bool largeArc, bool sweep, float32 x2, float32 y2) noexcept {
			if (rx == 0.f || ry == 0.f) {
				pb.LineTo(x2, y2);
				return;
			}
			rx = std::fabs(rx);
			ry = std::fabs(ry);
			const float32 rad = angleDeg * 0.017453292519943295f;
			const float32 cosA = std::cos(rad), sinA = std::sin(rad);
			// Step 1 : (x1', y1')
			const float32 dx2 = (x1 - x2) * 0.5f, dy2 = (y1 - y2) * 0.5f;
			const float32 x1p = cosA * dx2 + sinA * dy2;
			const float32 y1p = -sinA * dx2 + cosA * dy2;
			// Step 2 : (cx', cy')
			float32 rxs = rx * rx, rys = ry * ry;
			const float32 x1ps = x1p * x1p, y1ps = y1p * y1p;
			const float32 cr = x1ps / rxs + y1ps / rys;
			if (cr > 1.f) {
				const float32 s = std::sqrt(cr);
				rx *= s;
				ry *= s;
				rxs = rx * rx;
				rys = ry * ry;
			}
			float32 sq = (rxs * rys - rxs * y1ps - rys * x1ps) / (rxs * y1ps + rys * x1ps);
			sq = (sq < 0.f) ? 0.f : sq;
			const float32 coef = (largeArc == sweep ? -1.f : 1.f) * std::sqrt(sq);
			const float32 cxp = coef * (rx * y1p) / ry;
			const float32 cyp = -coef * (ry * x1p) / rx;
			// Step 3 : (cx, cy)
			const float32 cx = cosA * cxp - sinA * cyp + (x1 + x2) * 0.5f;
			const float32 cy = sinA * cxp + cosA * cyp + (y1 + y2) * 0.5f;
			// Step 4 : theta1 + dtheta
			auto angleFn = [](float32 ux, float32 uy, float32 vx, float32 vy) {
				const float32 dot = ux * vx + uy * vy;
				const float32 lenSq = (ux * ux + uy * uy) * (vx * vx + vy * vy);
				if (lenSq <= 0.f)
					return 0.f;
				float32 a = std::acos(std::fmax(-1.f, std::fmin(1.f, dot / std::sqrt(lenSq))));
				if (ux * vy - uy * vx < 0.f)
					a = -a;
				return a;
			};
			const float32 ux1 = (x1p - cxp) / rx, uy1 = (y1p - cyp) / ry;
			const float32 ux2 = (-x1p - cxp) / rx, uy2 = (-y1p - cyp) / ry;
			const float32 theta1 = angleFn(1, 0, ux1, uy1);
			float32 dtheta = angleFn(ux1, uy1, ux2, uy2);
			if (!sweep && dtheta > 0)
				dtheta -= 6.283185307179586f;
			else if (sweep && dtheta < 0)
				dtheta += 6.283185307179586f;
			// Step 5 : approxime par 1 Bezier par quart de tour max.
			const int32 nSegs = (int32)std::ceil(std::fabs(dtheta) / 1.5707963267948966f);
			if (nSegs <= 0)
				return;
			const float32 dt = dtheta / (float32)nSegs;
			const float32 t = (4.f / 3.f) * std::tan(dt * 0.25f);
			float32 cxLast = x1, cyLast = y1;
			for (int32 i = 0; i < nSegs; ++i) {
				const float32 a0 = theta1 + dt * (float32)i;
				const float32 a1 = theta1 + dt * (float32)(i + 1);
				const float32 ca0 = std::cos(a0), sa0 = std::sin(a0);
				const float32 ca1 = std::cos(a1), sa1 = std::sin(a1);
				const float32 ex = cosA * (rx * ca1) - sinA * (ry * sa1) + cx;
				const float32 ey = sinA * (rx * ca1) + cosA * (ry * sa1) + cy;
				const float32 c1x = cosA * (rx * (ca0 - t * sa0)) - sinA * (ry * (sa0 + t * ca0)) + cx;
				const float32 c1y = sinA * (rx * (ca0 - t * sa0)) + cosA * (ry * (sa0 + t * ca0)) + cy;
				const float32 c2x = cosA * (rx * (ca1 + t * sa1)) - sinA * (ry * (sa1 - t * ca1)) + cx;
				const float32 c2y = sinA * (rx * (ca1 + t * sa1)) + cosA * (ry * (sa1 - t * ca1)) + cy;
				FlattenCubic(pb, cxLast, cyLast, c1x, c1y, c2x, c2y, ex, ey);
				cxLast = ex;
				cyLast = ey;
			}
		}

		/// Parser de l'attribut d="..." pour <path>.
		/// Decode toutes les commandes SVG, applique relative/absolue, gere les
		/// repetitions implicites (M suivi de coords = ML, M etc.).
		void ParsePathD(const char *d, PathBuilder &pb) noexcept {
			if (!d)
				return;
			const char *p = d;
			float32 prevCtrlX = 0, prevCtrlY = 0;
			char lastCmd = 0;
			while (*p) {
				p = SkipWSComma(p);
				if (!*p)
					break;
				char cmd = *p;
				const bool isCmd = (cmd >= 'A' && cmd <= 'Z') || (cmd >= 'a' && cmd <= 'z');
				if (isCmd) {
					++p;
				} else {
					// Repetition implicite : M -> L (M devient L pour les sets suivants),
					// m -> l. Les autres conservent leur commande.
					cmd = lastCmd;
					if (cmd == 'M')
						cmd = 'L';
					else if (cmd == 'm')
						cmd = 'l';
				}
				p = SkipWSComma(p);
				const bool rel = (cmd >= 'a' && cmd <= 'z');
				auto X = [&](float32 x) { return rel ? pb.curX + x : x; };
				auto Y = [&](float32 y) { return rel ? pb.curY + y : y; };
				switch (cmd) {
					case 'M':
					case 'm': {
						const float32 x = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y = ParseFloat(p, &p);
						pb.StartContour(X(x), Y(y));
						prevCtrlX = pb.curX;
						prevCtrlY = pb.curY;
						break;
					}
					case 'L':
					case 'l': {
						const float32 x = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y = ParseFloat(p, &p);
						pb.LineTo(X(x), Y(y));
						prevCtrlX = pb.curX;
						prevCtrlY = pb.curY;
						break;
					}
					case 'H':
					case 'h': {
						const float32 x = ParseFloat(p, &p);
						pb.LineTo(X(x), pb.curY);
						prevCtrlX = pb.curX;
						prevCtrlY = pb.curY;
						break;
					}
					case 'V':
					case 'v': {
						const float32 y = ParseFloat(p, &p);
						pb.LineTo(pb.curX, Y(y));
						prevCtrlX = pb.curX;
						prevCtrlY = pb.curY;
						break;
					}
					case 'C':
					case 'c': {
						const float32 x1 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y1 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 x2 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y2 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 x3 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y3 = ParseFloat(p, &p);
						const float32 sx = pb.curX, sy = pb.curY;
						FlattenCubic(pb, sx, sy, X(x1), Y(y1), X(x2), Y(y2), X(x3), Y(y3));
						prevCtrlX = X(x2);
						prevCtrlY = Y(y2);
						break;
					}
					case 'S':
					case 's': {
						const float32 x2 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y2 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 x3 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y3 = ParseFloat(p, &p);
						const float32 sx = pb.curX, sy = pb.curY;
						const bool reflect = (lastCmd == 'C' || lastCmd == 'c' || lastCmd == 'S' || lastCmd == 's');
						const float32 c1x = reflect ? (2.f * sx - prevCtrlX) : sx;
						const float32 c1y = reflect ? (2.f * sy - prevCtrlY) : sy;
						FlattenCubic(pb, sx, sy, c1x, c1y, X(x2), Y(y2), X(x3), Y(y3));
						prevCtrlX = X(x2);
						prevCtrlY = Y(y2);
						break;
					}
					case 'Q':
					case 'q': {
						const float32 x1 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y1 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 x2 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y2 = ParseFloat(p, &p);
						const float32 sx = pb.curX, sy = pb.curY;
						FlattenQuad(pb, sx, sy, X(x1), Y(y1), X(x2), Y(y2));
						prevCtrlX = X(x1);
						prevCtrlY = Y(y1);
						break;
					}
					case 'T':
					case 't': {
						const float32 x2 = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y2 = ParseFloat(p, &p);
						const float32 sx = pb.curX, sy = pb.curY;
						const bool reflect = (lastCmd == 'Q' || lastCmd == 'q' || lastCmd == 'T' || lastCmd == 't');
						const float32 c1x = reflect ? (2.f * sx - prevCtrlX) : sx;
						const float32 c1y = reflect ? (2.f * sy - prevCtrlY) : sy;
						FlattenQuad(pb, sx, sy, c1x, c1y, X(x2), Y(y2));
						prevCtrlX = c1x;
						prevCtrlY = c1y;
						break;
					}
					case 'A':
					case 'a': {
						const float32 rx = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 ry = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 ang = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const int32 large = (int32)ParseFloat(p, &p);
						p = SkipWSComma(p);
						const int32 sweep = (int32)ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 x = ParseFloat(p, &p);
						p = SkipWSComma(p);
						const float32 y = ParseFloat(p, &p);
						const float32 sx = pb.curX, sy = pb.curY;
						const float32 ex = X(x), ey = Y(y);
						FlattenArc(pb, sx, sy, rx, ry, ang, large != 0, sweep != 0, ex, ey);
						pb.curX = ex;
						pb.curY = ey;
						if (pb.sh.contourLen.Size() > 0) {
							// Update du contourLen pour le dernier point d'arc.
							// FlattenArc -> FlattenCubic -> LineTo qui incremente contourLen,
							// donc rien a faire ici. Mais on met a jour curX/curY au-dessus
							// pour les commandes relatives suivantes.
						}
						prevCtrlX = pb.curX;
						prevCtrlY = pb.curY;
						break;
					}
					case 'Z':
					case 'z': {
						pb.Close();
						break;
					}
					default:
						// commande inconnue : on saute un caractere et continue
						++p;
						break;
				}
				lastCmd = cmd;
			}
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// SECTION 6 — Style : application des attributs CSS / individuels
		// ─────────────────────────────────────────────────────────────────────────────

		void ApplyAttrToStyle(NkSVGStyle &s, const char *name, const char *value) noexcept {
			if (!name || !value)
				return;
			if (std::strcmp(name, "fill") == 0)
				s.fill = NkSVGColor::Parse(value);
			else if (std::strcmp(name, "stroke") == 0)
				s.stroke = NkSVGColor::Parse(value);
			else if (std::strcmp(name, "stroke-width") == 0)
				s.strokeWidth = ParseFloat(value);
			else if (std::strcmp(name, "opacity") == 0)
				s.opacity = ParseFloat(value);
			else if (std::strcmp(name, "fill-opacity") == 0)
				s.fillOpacity = ParseFloat(value);
			else if (std::strcmp(name, "stroke-opacity") == 0)
				s.strokeOpacity = ParseFloat(value);
			else if (std::strcmp(name, "fill-rule") == 0)
				s.fillEvenOdd = (std::strcmp(value, "evenodd") == 0);
			else if (std::strcmp(name, "display") == 0)
				s.visible = std::strcmp(value, "none") != 0;
			else if (std::strcmp(name, "visibility") == 0)
				s.visible = std::strcmp(value, "hidden") != 0;
			else if (std::strcmp(name, "stroke-linecap") == 0) {
				if (std::strcmp(value, "round") == 0)
					s.strokeLineCap = NkSVGLineCap::Round;
				else if (std::strcmp(value, "square") == 0)
					s.strokeLineCap = NkSVGLineCap::Square;
				else
					s.strokeLineCap = NkSVGLineCap::Butt;
			} else if (std::strcmp(name, "stroke-linejoin") == 0) {
				if (std::strcmp(value, "round") == 0)
					s.strokeLineJoin = NkSVGLineJoin::Round;
				else if (std::strcmp(value, "bevel") == 0)
					s.strokeLineJoin = NkSVGLineJoin::Bevel;
				else
					s.strokeLineJoin = NkSVGLineJoin::Miter;
			} else if (std::strcmp(name, "stroke-dasharray") == 0) {
				s.nDashes = 0;
				if (StrCaseCmp(value, "none") != 0) {
					const char *p = value;
					float32 somme = 0.f;
					while (*p && s.nDashes < NkSVGStyle::kMaxDashes) {
						p = SkipWSComma(p);
						if (!*p)
							break;
						const char *e = nullptr;
						float32 v = ParseFloat(p, &e);
						if (e == p)
							break;
						p = e;
						if (v < 0.f)
							v = 0.f; // une longueur negative invalide le motif (norme)
						s.dashes[s.nDashes++] = v;
						somme += v;
					}
					// UN MOTIF DE SOMME NULLE EST UN TRAIT CONTINU, pas un trait
					// invisible : la norme le dit, et l'oubli ferait disparaitre le
					// trait au lieu de le laisser plein.
					if (somme <= 0.f)
						s.nDashes = 0;
				}
			} else if (std::strcmp(name, "stroke-dashoffset") == 0) {
				s.dashOffset = ParseFloat(value);
			} else if (std::strcmp(name, "mix-blend-mode") == 0) {
				// PEINTS parce qu'ils ne coutent rien : le rasteriseur lit deja la
				// destination pour composer. Les autres modes demanderaient un calque
				// isole par groupe -- ils sont nommes au decodage, pas devines.
				if (std::strcmp(value, "multiply") == 0)
					s.blend = NkSVGBlend::Multiply;
				else if (std::strcmp(value, "screen") == 0)
					s.blend = NkSVGBlend::Screen;
				else
					s.blend = NkSVGBlend::Normal;
			} else if (std::strcmp(name, "stroke-miterlimit") == 0) {
				const float32 ml = ParseFloat(value);
				s.strokeMiterLimit = (ml >= 1.f) ? ml : 1.f;
			}
		}

		/// Parse un bloc de declarations « key:value;key:value ».
		/// @param important  false : n'applique QUE les declarations ordinaires ;
		///                   true  : n'applique QUE celles marquees `!important`.
		///                   Deux passes, parce que `!important` ne se contente pas
		///                   de gagner contre ses voisines : il gagne contre TOUT ce
		///                   qui vient apres, y compris le style inline.
		void ApplyCSSStyle(NkSVGStyle &s, const char *css, bool important = false) noexcept {
			if (!css)
				return;
			const char *p = css;
			while (*p) {
				// Lit le nom de la propriete.
				char name[64];
				int32 ni = 0;
				while (*p && *p != ':' && *p != ';' && ni + 1 < (int32)sizeof(name)) {
					if (!IsSpace(*p))
						name[ni++] = *p;
					++p;
				}
				name[ni] = 0;
				if (*p != ':') {
					while (*p && *p != ';')
						++p;
					if (*p)
						++p;
					continue;
				}
				++p;
				while (IsSpace(*p))
					++p;
				// Lit la valeur (peut etre longue : rgb(...) etc., on prend 255 chars).
				char val[256];
				int32 vi = 0;
				while (*p && *p != ';' && vi + 1 < (int32)sizeof(val))
					val[vi++] = *p++;
				val[vi] = 0;
				// Trim trailing whitespace.
				while (vi > 0 && IsSpace(val[vi - 1]))
					val[--vi] = 0;
				// « !important » : on le RETIRE de la valeur et on note son rang.
				bool estImportante = false;
				{
					// « !important » fait DIX caracteres : son debut est a k-9. Le
					// decalage d'un seul caractere ne provoque aucune erreur -- la
					// declaration passe simplement pour ordinaire, et le style inline
					// la bat. C'est le genre de defaut qu'aucun compilateur ne voit
					// et qu'un banc attrape en une ligne.
					int32 k = vi - 1;
					while (k >= 0 && IsSpace(val[k]))
						--k;
					if (k >= 9) {
						char *deb = val + (k - 9);
						if (StrCaseCmp(deb, "!important") == 0) {
							estImportante = true;
							*deb = 0;
							int32 j = (k - 9) - 1;
							while (j >= 0 && IsSpace(val[j]))
								val[j--] = 0;
						}
					}
				}
				if (estImportante == important)
					ApplyAttrToStyle(s, name, val);
				if (*p == ';')
					++p;
			}
		}

		// ═════════════════════════════════════════════════════════════════════════════
		// LE CSS DE <style> — CE QUI REND LISIBLES LES SVG DES AUTRES OUTILS
		// -----------------------------------------------------------------------------
		// Illustrator, Figma et Inkscape factorisent les styles dans un <style> et ne
		// laissent sur les elements qu'un `class="st0"`. Sauter ce bloc -- ce que le
		// codec faisait -- revient a rendre ces fichiers SANS AUCUNE COULEUR : tout
		// retombe sur le noir par defaut. C'est le palier qui ouvre le plus de
		// fichiers d'un coup, apres <use>.
		//
		// L'ORDRE DE PRIORITE EST CELUI DE LA NORME, et il n'est pas intuitif :
		//   attributs de presentation  <  regles CSS  <  style="" inline  <  !important
		// Autrement dit une regle CSS ECRASE un `fill="red"` ecrit sur l'element. Le
		// codec faisait l'inverse pour le couple attribut / style inline (les attributs
		// gagnaient) : corrige ici, et le banc le fige.
		//
		// CE QUI N'EST PAS FAIT, et qui est dit : les combinateurs (`a b`, `a > b`),
		// les pseudo-classes, les media queries. Un selecteur qu'on ne sait pas lire
		// est IGNORE ET NOMME plutot qu'apparie de travers -- appliquer une regle au
		// mauvais element est pire que ne pas l'appliquer.
		// ═════════════════════════════════════════════════════════════════════════════
		struct RegleCSS {
				char type[32] = {0}; ///< « rect » ; vide = `*`
				char cls[64] = {0};	 ///< « st0 » sans le point ; vide = aucune
				char id[64] = {0};	 ///< sans le dièse ; vide = aucun
				char decl[512] = {0};
				int32 spec = 0;	  ///< 100 par id, 10 par classe, 1 par type
				int32 ordre = 0;  ///< depart l'egalite : la derniere ecrite gagne
		};

		/// Decompose UN selecteur simple (« rect.st0#a ») en ses trois parties.
		/// @return false si on y voit autre chose (combinateur, pseudo-classe...).
		bool LireSelecteur(const char *deb, const char *fin, RegleCSS &r) noexcept {
			int32 n = 0;
			char courant = 't'; // t=type, c=classe, i=id
			char *cible = r.type;
			usize cap = sizeof(r.type);
			for (const char *p = deb; p < fin; ++p) {
				const char c = *p;
				if (IsSpace(c)) {
					// un espace SEPARE deux selecteurs : c'est un combinateur
					const char *q = p;
					while (q < fin && IsSpace(*q))
						++q;
					if (q < fin)
						return false;
					break;
				}
				if (c == '>' || c == '+' || c == '~' || c == ':' || c == '[')
					return false;
				if (c == '.' || c == '#') {
					courant = (c == '.') ? 'c' : 'i';
					cible = (c == '.') ? r.cls : r.id;
					cap = (c == '.') ? sizeof(r.cls) : sizeof(r.id);
					n = 0;
					continue;
				}
				if (c == '*') {
					if (courant == 't')
						continue; // `*` = pas de contrainte de type
					return false;
				}
				if ((usize)n + 1 < cap)
					cible[n++] = c;
				cible[n] = 0;
			}
			r.spec = (r.id[0] ? 100 : 0) + (r.cls[0] ? 10 : 0) + (r.type[0] ? 1 : 0);
			return true;
		}

		/// Collecte toutes les regles de tous les blocs <style> du document. Une
		/// PRE-PASSE, comme l'index des id : une regle peut etre ecrite APRES les
		/// elements qu'elle habille.
		void CollecterCSS(const char *deb, const char *fin, NkVector<RegleCSS> &out, SkipList &skips) noexcept {
			int32 ordre = 0;
			const char *p = deb;
			while (p < fin) {
				// trouver « <style »
				while (p < fin && !(p[0] == '<' && (p + 6) < fin && std::strncmp(p + 1, "style", 5) == 0 &&
									(IsSpace(p[6]) || p[6] == '>')))
					++p;
				if (p >= fin)
					break;
				while (p < fin && *p != '>')
					++p;
				if (p < fin)
					++p;
				const char *corpsDeb = p;
				while (p + 7 < fin && !(p[0] == '<' && p[1] == '/' && std::strncmp(p + 2, "style", 5) == 0))
					++p;
				const char *corpsFin = p;

				// le corps, sans le CDATA ni les commentaires
				const char *c = corpsDeb;
				while (c < corpsFin) {
					if (std::strncmp(c, "<![CDATA[", 9) == 0) {
						c += 9;
						continue;
					}
					if (std::strncmp(c, "]]>", 3) == 0) {
						c += 3;
						continue;
					}
					if (std::strncmp(c, "/*", 2) == 0) {
						c += 2;
						while (c + 1 < corpsFin && !(c[0] == '*' && c[1] == '/'))
							++c;
						c = (c + 1 < corpsFin) ? c + 2 : corpsFin;
						continue;
					}
					if (IsSpace(*c)) {
						++c;
						continue;
					}
					// « selecteurs { declarations } »
					const char *selDeb = c;
					while (c < corpsFin && *c != '{')
						++c;
					if (c >= corpsFin)
						break;
					const char *selFin = c;
					++c;
					const char *declDeb = c;
					while (c < corpsFin && *c != '}')
						++c;
					const char *declFin = c;
					if (c < corpsFin)
						++c;
					if (selDeb >= selFin)
						continue;
					// une @-regle (@media, @font-face...) : dite, pas devinee
					if (*selDeb == '@') {
						if (skips.Noter("css-at-rule"))
							logger.Warn("[SVG] regle CSS « @ » (media, font-face...) non appliquee.");
						continue;
					}
					// les selecteurs sont separes par des virgules
					const char *a = selDeb;
					while (a < selFin) {
						const char *b = a;
						while (b < selFin && *b != ',')
							++b;
						const char *deb2 = a, *fin2 = b;
						while (deb2 < fin2 && IsSpace(*deb2))
							++deb2;
						while (fin2 > deb2 && IsSpace(fin2[-1]))
							--fin2;
						if (deb2 < fin2 && out.Size() < 512u) {
							RegleCSS r;
							if (LireSelecteur(deb2, fin2, r)) {
								usize k = 0;
								for (const char *d = declDeb; d < declFin && k + 1 < sizeof(r.decl); ++d)
									r.decl[k++] = *d;
								r.decl[k] = 0;
								r.ordre = ordre++;
								out.PushBack(r);
							} else if (skips.Noter("css-selecteur")) {
								logger.Warn("[SVG] selecteur CSS non gere (combinateur, pseudo-classe, "
											"attribut) -- la regle est IGNOREE plutot qu'appliquee de travers.");
							}
						}
						a = (b < selFin) ? b + 1 : selFin;
					}
				}
				p = corpsFin;
			}
		}

		/// La regle s'applique-t-elle a cet element ?
		bool CorrespondCSS(const RegleCSS &r, const char *tag, const char *classes, const char *id) noexcept {
			if (r.type[0] && (!tag || std::strcmp(r.type, tag) != 0))
				return false;
			if (r.id[0] && (!id || std::strcmp(r.id, id) != 0))
				return false;
			if (r.cls[0]) {
				if (!classes)
					return false;
				// `class` porte une LISTE de noms separes par des blancs
				const usize n = std::strlen(r.cls);
				const char *p = classes;
				while (*p) {
					while (*p && IsSpace(*p))
						++p;
					const char *deb = p;
					while (*p && !IsSpace(*p))
						++p;
					if ((usize)(p - deb) == n && std::strncmp(deb, r.cls, n) == 0)
						return true;
				}
				return false;
			}
			return true;
		}

		/// Fusionne les attributs d'un element sur un style herite.
		/// Ordre : (1) parent, (2) attribut "style" inline, (3) attributs individuels.
		/// LA CASCADE, dans l'ordre de la norme -- et il n'est pas intuitif :
		///   parent  <  attributs de presentation  <  regles CSS  <  style=""  <  !important
		/// Le codec appliquait `style=""` AVANT les attributs, donc un `fill="red"`
		/// ecrase par un `style="fill:blue"` gagnait quand meme. Corrige ici.
		/// @param regles  les regles du document, ou nullptr (aucun <style>)
		NkSVGStyle MergeStyle(const NkSVGStyle &parent, const AttrPair *attrs, int32 n,
							  const NkVector<RegleCSS> *regles = nullptr, const char *tag = nullptr) noexcept {
			NkSVGStyle s = parent;
			const char *css = FindAttr(attrs, n, "style");

			// 1. les attributs de presentation (le plus faible)
			for (int32 i = 0; i < n; ++i) {
				if (std::strcmp(attrs[i].name, "style") == 0)
					continue;
				ApplyAttrToStyle(s, attrs[i].name, attrs[i].value);
			}

			// 2. les regles CSS, par SPECIFICITE croissante ; a egalite, la derniere
			//    ecrite gagne (l'ordre du fichier, comme dans un navigateur).
			if (regles && !regles->IsEmpty()) {
				const char *cls = FindAttr(attrs, n, "class");
				const char *id = FindAttr(attrs, n, "id");
				for (int32 passe = 0; passe < 2; ++passe) { // 0 = ordinaires, 1 = !important
					// tri par insertion a la volee : on cherche, a chaque tour, la
					// regle applicable de rang immediatement superieur au precedent.
					int32 dernierSpec = -1, dernierOrdre = -1;
					for (;;) {
						const RegleCSS *choisie = nullptr;
						for (uint32 i = 0; i < regles->Size(); ++i) {
							const RegleCSS &r = (*regles)[i];
							const int32 rang = r.spec;
							if (rang < dernierSpec || (rang == dernierSpec && r.ordre <= dernierOrdre))
								continue;
							if (!CorrespondCSS(r, tag, cls, id))
								continue;
							if (!choisie || rang < choisie->spec ||
								(rang == choisie->spec && r.ordre < choisie->ordre))
								choisie = &r;
						}
						if (!choisie)
							break;
						ApplyCSSStyle(s, choisie->decl, passe == 1);
						dernierSpec = choisie->spec;
						dernierOrdre = choisie->ordre;
					}
					// 3. le style inline, plus fort que toute regle -- mais dans la
					//    MEME passe, pour que `!important` d'une regle batte un inline
					//    ordinaire, comme la norme le demande.
					if (css)
						ApplyCSSStyle(s, css, passe == 1);
				}
			} else if (css) {
				ApplyCSSStyle(s, css, false);
				ApplyCSSStyle(s, css, true);
			}
			return s;
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// Helpers gradients : url(#id), props CSS, refs fill/stroke, lecture de <stop>
		// ─────────────────────────────────────────────────────────────────────────────

		/// Extrait l'id d'une valeur "url(#id)" -> out. @return true si match.
		bool ParseUrlRef(const char *v, char *out, usize outSz) noexcept {
			if (!v)
				return false;
			const char *p = SkipWS(v);
			if (std::strncmp(p, "url(", 4) != 0)
				return false;
			p += 4;
			p = SkipWS(p);
			if (*p == '#')
				++p;
			usize i = 0;
			while (*p && *p != ')' && i + 1 < outSz) {
				if (!IsSpace(*p))
					out[i++] = *p;
				++p;
			}
			out[i] = 0;
			return i > 0;
		}

		/// Recupere une propriete d'un attribut style="a:b;c:d". @return true si trouvee.
		bool GetCSSProp(const char *css, const char *prop, char *out, usize outSz) noexcept {
			if (!css)
				return false;
			const usize plen = std::strlen(prop);
			const char *p = css;
			while (*p) {
				const char *ns = p;
				while (*p && *p != ':' && *p != ';')
					++p;
				const char *n0 = ns;
				const char *nend = p;
				while (n0 < nend && IsSpace(*n0))
					++n0;
				while (nend > n0 && IsSpace(nend[-1]))
					--nend;
				const bool match = ((usize)(nend - n0) == plen) && std::strncmp(n0, prop, plen) == 0;
				if (*p == ':') {
					++p;
					while (*p && IsSpace(*p))
						++p;
					const char *vs = p;
					while (*p && *p != ';')
						++p;
					const char *ve = p;
					while (ve > vs && IsSpace(ve[-1]))
						--ve;
					if (match) {
						usize i = 0;
						for (const char *q = vs; q < ve && i + 1 < outSz; ++q)
							out[i++] = *q;
						out[i] = 0;
						return true;
					}
				}
				if (*p == ';')
					++p;
			}
			return false;
		}

		/// Met a jour un ref (fill/stroke) selon une valeur : url() -> set, sinon clear.
		void UpdateRefFromValue(char *ref, const char *value) noexcept {
			char tmp[64];
			if (value && ParseUrlRef(value, tmp, sizeof(tmp))) {
				std::strncpy(ref, tmp, 63);
				ref[63] = 0;
			} else if (value) {
				ref[0] = 0; // couleur solide ou "none" : annule un gradient herite
			}
		}

		/// Met a jour fillRef/strokeRef (deja herites) selon style="" puis attrs individuels.
		void UpdateRefs(char *fillRef, char *strokeRef, const AttrPair *attrs, int32 n) noexcept {
			const char *css = FindAttr(attrs, n, "style");
			if (css) {
				char buf[80];
				if (GetCSSProp(css, "fill", buf, sizeof(buf)))
					UpdateRefFromValue(fillRef, buf);
				if (GetCSSProp(css, "stroke", buf, sizeof(buf)))
					UpdateRefFromValue(strokeRef, buf);
			}
			for (int32 i = 0; i < n; ++i) {
				if (std::strcmp(attrs[i].name, "fill") == 0)
					UpdateRefFromValue(fillRef, attrs[i].value);
				else if (std::strcmp(attrs[i].name, "stroke") == 0)
					UpdateRefFromValue(strokeRef, attrs[i].value);
			}
		}

		/// Parse un nombre pouvant finir par '%' (=> /100). @p def si str nul.
		float32 ParsePct(const char *s, float32 def) noexcept {
			if (!s)
				return def;
			const char *e = nullptr;
			float32 v = ParseFloat(s, &e);
			if (e) {
				while (*e && IsSpace(*e))
					++e;
				if (*e == '%')
					v *= 0.01f;
			}
			return v;
		}

		/// Lit un <stop> -> GradStop (offset + stop-color * stop-opacity).
		GradStop ReadStop(const AttrPair *a, int32 n) noexcept {
			GradStop st;
			const char *css = FindAttr(a, n, "style");
			char cbuf[80], obuf[32];

			const char *sc = FindAttr(a, n, "stop-color");
			if (!sc && css && GetCSSProp(css, "stop-color", cbuf, sizeof(cbuf)))
				sc = cbuf;
			NkSVGColor col = sc ? NkSVGColor::Parse(sc) : NkSVGColor::Black();

			const char *so = FindAttr(a, n, "stop-opacity");
			if (!so && css && GetCSSProp(css, "stop-opacity", obuf, sizeof(obuf)))
				so = obuf;
			if (so) {
				float32 op = ParseFloat(so);
				if (op < 0.f)
					op = 0.f;
				if (op > 1.f)
					op = 1.f;
				col.a = (uint8)((float32)col.a * op);
			}
			col.none = false;
			st.color = col;
			st.offset = ParsePct(FindAttr(a, n, "offset"), 0.f);
			if (st.offset < 0.f)
				st.offset = 0.f;
			if (st.offset > 1.f)
				st.offset = 1.f;
			return st;
		}

		/// Applique une matrice a tous les points d'une Shape (in-place).
		void ApplyTransform(Shape &sh, const NkSVGTransform &m) noexcept {
			for (uint32 i = 0; i < sh.xs.Size(); ++i) {
				float32 x = sh.xs[i], y = sh.ys[i];
				m.Apply(x, y);
				sh.xs[i] = x;
				sh.ys[i] = y;
			}
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// SECTION 7 — Constructeurs de Shape par tag SVG
		// ─────────────────────────────────────────────────────────────────────────────

		void ShapeFromRect(NkVector<Shape> &shapes, const AttrPair *a, int32 n, const ParseState &st) noexcept {
			const char *sw = FindAttr(a, n, "width");
			const char *sh = FindAttr(a, n, "height");
			if (!sw || !sh)
				return;
			const float32 x = ParseFloat(FindAttr(a, n, "x"));
			const float32 y = ParseFloat(FindAttr(a, n, "y"));
			const float32 w = ParseFloat(sw);
			const float32 h = ParseFloat(sh);
			if (w <= 0 || h <= 0)
				return;

			// ── rx / ry : la regle du W3C (SVG 1.1 §9.2), pas une approximation ──
			//   - « auto » ou absent des DEUX cotes : coins droits ;
			//   - un seul des deux donne : l'autre prend sa valeur ;
			//   - chacun borne a la MOITIE du cote correspondant.
			// Le coin est un ARC (quart d'ellipse par une Bezier cubique, k = 4/3
			// tan(pi/8)), jamais une suite de segments : a l'oeil comme au test,
			// un octogone n'est pas un arrondi.
			const char *srx = FindAttr(a, n, "rx");
			const char *sry = FindAttr(a, n, "ry");
			const bool rxAuto = (!srx || StrCaseCmp(srx, "auto") == 0);
			const bool ryAuto = (!sry || StrCaseCmp(sry, "auto") == 0);
			float32 rx = rxAuto ? 0.f : ParseFloat(srx);
			float32 ry = ryAuto ? 0.f : ParseFloat(sry);
			if (rxAuto && !ryAuto)
				rx = ry;
			else if (ryAuto && !rxAuto)
				ry = rx;
			if (rx < 0.f)
				rx = 0.f;
			if (ry < 0.f)
				ry = 0.f;
			if (rx > w * 0.5f)
				rx = w * 0.5f;
			if (ry > h * 0.5f)
				ry = h * 0.5f;

			Shape s2;
			s2.style = st.style;
			PathBuilder pb(s2);
			if (rx > 0.f && ry > 0.f) {
				constexpr float32 k = 0.5522847498307933f;
				const float32 cx = rx * k, cy = ry * k;
				const float32 x0 = x, x1 = x + w, y0 = y, y1 = y + h;
				pb.StartContour(x0 + rx, y0);
				pb.LineTo(x1 - rx, y0);
				FlattenCubic(pb, x1 - rx, y0, x1 - rx + cx, y0, x1, y0 + ry - cy, x1, y0 + ry);
				pb.LineTo(x1, y1 - ry);
				FlattenCubic(pb, x1, y1 - ry, x1, y1 - ry + cy, x1 - rx + cx, y1, x1 - rx, y1);
				pb.LineTo(x0 + rx, y1);
				FlattenCubic(pb, x0 + rx, y1, x0 + rx - cx, y1, x0, y1 - ry + cy, x0, y1 - ry);
				pb.LineTo(x0, y0 + ry);
				FlattenCubic(pb, x0, y0 + ry, x0, y0 + ry - cy, x0 + rx - cx, y0, x0 + rx, y0);
			} else {
				pb.StartContour(x, y);
				pb.LineTo(x + w, y);
				pb.LineTo(x + w, y + h);
				pb.LineTo(x, y + h);
			}
			pb.Close();
			ApplyTransform(s2, st.xform);
			shapes.PushBack(std::move(s2));
		}

		/// Cercle : approxime par 4 Beziers cubiques (k = (4/3)*tan(pi/8) ≈ 0.5523).
		void ShapeFromCircle(NkVector<Shape> &shapes, const AttrPair *a, int32 n, const ParseState &st) noexcept {
			const char *sr = FindAttr(a, n, "r");
			if (!sr)
				return;
			const float32 cx = ParseFloat(FindAttr(a, n, "cx"));
			const float32 cy = ParseFloat(FindAttr(a, n, "cy"));
			const float32 r = ParseFloat(sr);
			if (r <= 0)
				return;
			Shape s2;
			s2.style = st.style;
			PathBuilder pb(s2);
			const float32 k = 0.5522847498307933f;
			pb.StartContour(cx - r, cy);
			FlattenCubic(pb, cx - r, cy, cx - r, cy - r * k, cx - r * k, cy - r, cx, cy - r);
			FlattenCubic(pb, cx, cy - r, cx + r * k, cy - r, cx + r, cy - r * k, cx + r, cy);
			FlattenCubic(pb, cx + r, cy, cx + r, cy + r * k, cx + r * k, cy + r, cx, cy + r);
			FlattenCubic(pb, cx, cy + r, cx - r * k, cy + r, cx - r, cy + r * k, cx - r, cy);
			pb.Close();
			ApplyTransform(s2, st.xform);
			shapes.PushBack(std::move(s2));
		}

		void ShapeFromEllipse(NkVector<Shape> &shapes, const AttrPair *a, int32 n, const ParseState &st) noexcept {
			const char *srx = FindAttr(a, n, "rx");
			const char *sry = FindAttr(a, n, "ry");
			if (!srx || !sry)
				return;
			const float32 cx = ParseFloat(FindAttr(a, n, "cx"));
			const float32 cy = ParseFloat(FindAttr(a, n, "cy"));
			const float32 rx = ParseFloat(srx), ry = ParseFloat(sry);
			if (rx <= 0 || ry <= 0)
				return;
			Shape s2;
			s2.style = st.style;
			PathBuilder pb(s2);
			const float32 k = 0.5522847498307933f;
			pb.StartContour(cx - rx, cy);
			FlattenCubic(pb, cx - rx, cy, cx - rx, cy - ry * k, cx - rx * k, cy - ry, cx, cy - ry);
			FlattenCubic(pb, cx, cy - ry, cx + rx * k, cy - ry, cx + rx, cy - ry * k, cx + rx, cy);
			FlattenCubic(pb, cx + rx, cy, cx + rx, cy + ry * k, cx + rx * k, cy + ry, cx, cy + ry);
			FlattenCubic(pb, cx, cy + ry, cx - rx * k, cy + ry, cx - rx, cy + ry * k, cx - rx, cy);
			pb.Close();
			ApplyTransform(s2, st.xform);
			shapes.PushBack(std::move(s2));
		}

		void ShapeFromLine(NkVector<Shape> &shapes, const AttrPair *a, int32 n, const ParseState &st) noexcept {
			Shape s2;
			s2.style = st.style;
			PathBuilder pb(s2);
			pb.StartContour(ParseFloat(FindAttr(a, n, "x1")), ParseFloat(FindAttr(a, n, "y1")));
			pb.LineTo(ParseFloat(FindAttr(a, n, "x2")), ParseFloat(FindAttr(a, n, "y2")));
			ApplyTransform(s2, st.xform);
			shapes.PushBack(std::move(s2));
		}

		void ShapeFromPolygon(NkVector<Shape> &shapes, const AttrPair *a, int32 n, const ParseState &st,
							  bool closed) noexcept {
			const char *pts = FindAttr(a, n, "points");
			if (!pts)
				return;
			Shape s2;
			s2.style = st.style;
			PathBuilder pb(s2);
			const char *p = pts;
			bool first = true;
			while (*p) {
				p = SkipWSComma(p);
				if (!*p)
					break;
				const float32 x = ParseFloat(p, &p);
				p = SkipWSComma(p);
				if (!*p && first)
					break;
				const float32 y = ParseFloat(p, &p);
				if (first) {
					pb.StartContour(x, y);
					first = false;
				} else
					pb.LineTo(x, y);
			}
			if (closed)
				pb.Close();
			ApplyTransform(s2, st.xform);
			shapes.PushBack(std::move(s2));
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// <image> — LA SOURCE : « data:image/...;base64,... » ou un chemin RELATIF AU
		// FICHIER SVG. Un href relatif ne veut rien dire sans savoir d'ou l'on lit :
		// le dossier du .svg est donc porte jusqu'ici (nullptr = on ne sait pas, et
		// on le DIT au lieu de deviner un chemin depuis le repertoire courant).
		// ─────────────────────────────────────────────────────────────────────────────

		/// Decode « data:<type>;base64,<charge> ». @return image invalide si ce n'est
		/// pas une URL de donnees, ou si la charge est illisible (dit par l'appelant).
		NkImage ImageDepuisDataURL(const char *href, bool &etaitDataURL) noexcept {
			etaitDataURL = false;
			NkImage vide;
			if (!href || std::strncmp(href, "data:", 5) != 0)
				return vide;
			etaitDataURL = true;
			// on saute jusqu'a la virgule ; on exige base64 (le pourcent-encodage
			// d'un SVG imbrique n'est pas lu -- ce serait un autre palier, et il est dit)
			const char *virgule = href;
			while (*virgule && *virgule != ',')
				++virgule;
			if (*virgule != ',')
				return vide;
			bool b64 = false;
			for (const char *p = href + 5; p < virgule; ++p) {
				if (std::strncmp(p, "base64", 6) == 0) {
					b64 = true;
					break;
				}
			}
			if (!b64) {
				logger.Warn("[SVG] <image> : data: sans « base64 » -- charge non lue.");
				return vide;
			}
			const char *charge = virgule + 1;
			const usize nChars = std::strlen(charge);
			if (nChars == 0)
				return vide;
			// 4 caracteres -> 3 octets ; on alloue large, NkDecode rend la taille exacte
			const usize capacite = (nChars / 4u + 2u) * 3u;
			uint8 *brut = (uint8 *)NkAlloc(capacite);
			if (!brut)
				return vide;
			usize ecrits = 0;
			const NkStringView vue(charge, nChars);
			// NkDecode ECHOUE sur un caractere invalide (il ne devine pas) : on relaie
			// cet echec au lieu de peindre du bruit.
			if (!encoding::base64::NkDecode(vue, brut, &ecrits) || ecrits == 0) {
				NkFree(brut);
				logger.Warn("[SVG] <image> : base64 illisible -- image ignoree.");
				return vide;
			}
			NkImage im;
			// 4 canaux exiges : le rasteriseur compose en RGBA, un PNG gris ou sans
			// alpha doit arriver au meme format que les autres.
			if (!im.LoadFromMemory(brut, ecrits, 4)) {
				NkFree(brut);
				logger.Warn("[SVG] <image> : les octets base64 ne sont AUCUN des formats connus de NKImage.");
				return NkImage();
			}
			NkFree(brut);
			return im;
		}

		/// Joint le dossier du .svg et un href relatif. Un href ABSOLU (C:/..., /...)
		/// est pris tel quel.
		void JoindreChemin(char *out, usize outSz, const char *base, const char *href) noexcept {
			out[0] = 0;
			if (!href)
				return;
			const bool absolu = (href[0] == '/' || href[0] == '\\' ||
								 (href[0] && href[1] == ':' && (href[2] == '/' || href[2] == '\\')));
			if (absolu || !base || !*base) {
				std::strncpy(out, href, outSz - 1);
				out[outSz - 1] = 0;
				return;
			}
			std::strncpy(out, base, outSz - 1);
			out[outSz - 1] = 0;
			usize l = std::strlen(out);
			if (l > 0 && out[l - 1] != '/' && out[l - 1] != '\\' && l + 1 < outSz) {
				out[l++] = '/';
				out[l] = 0;
			}
			std::strncat(out, href, outSz - l - 1);
		}

		/// Lit « <align> [meet|slice] » en entier : l'ajustement ET les deux
		/// alignements. Le defaut de la norme est `xMidYMid meet`.
		void LirePreserveAspectRatio(const char *par, FitKind &fit, Align &ax, Align &ay) noexcept {
			fit = FitKind::Meet;
			ax = ay = Align::Mid;
			if (!par)
				return;
			const char *p = SkipWS(par);
			if (std::strncmp(p, "none", 4) == 0) {
				fit = FitKind::None;
				return;
			}
			for (const char *q = p; *q; ++q)
				if (std::strncmp(q, "slice", 5) == 0) {
					fit = FitKind::Slice;
					break;
				}
			// ⚠️ LE JETON, PAS LA SOUS-CHAINE. « xMaxYMid » contient DEUX `x` : celui
			// du debut, et celui de « Max ». Balayer toute la chaine trouvait le
			// second (suivi de « YMid ») et ecrasait l'alignement lu au premier --
			// `xMaxYMid` rendait donc `xMidYMid`, en silence. L'alignement est UN
			// jeton de huit caracteres, en tete : on le lit LA, une fois.
			//   « x » + Min|Mid|Max + « Y » + Min|Mid|Max
			if (p[0] == 'x' && p[1] && p[2] && p[3] && p[4] == 'Y' && p[5] && p[6] && p[7]) {
				if (std::strncmp(p + 1, "Min", 3) == 0) ax = Align::Min;
				else if (std::strncmp(p + 1, "Max", 3) == 0) ax = Align::Max;
				else ax = Align::Mid;
				if (std::strncmp(p + 5, "Min", 3) == 0) ay = Align::Min;
				else if (std::strncmp(p + 5, "Max", 3) == 0) ay = Align::Max;
				else ay = Align::Mid;
			}
		}

		void ShapeFromImage(NkVector<Shape> &shapes, const AttrPair *a, int32 n, const ParseState &st,
							const char *baseDir, SkipList &skips) noexcept {
			const char *href = FindAttr(a, n, "href");
			if (!href)
				href = FindAttr(a, n, "xlink:href");
			if (!href || !*href)
				return;
			const float32 w = ParseFloat(FindAttr(a, n, "width"));
			const float32 h = ParseFloat(FindAttr(a, n, "height"));
			if (w <= 0.f || h <= 0.f)
				return;
			const float32 x = ParseFloat(FindAttr(a, n, "x"));
			const float32 y = ParseFloat(FindAttr(a, n, "y"));

			bool dataURL = false;
			NkImage im = ImageDepuisDataURL(href, dataURL);
			if (!dataURL) {
				char chemin[1024];
				JoindreChemin(chemin, sizeof(chemin), baseDir, href);
				if (!chemin[0])
					return;
				if (!baseDir && href[0] != '/' && !(href[0] && href[1] == ':')) {
					if (skips.Noter("image-href-relatif"))
						logger.Warn("[SVG] <image href=\"{0}\"> : le SVG a ete decode SANS son dossier "
									"(Decode d'un buffer) -- le chemin est resolu depuis le repertoire courant.",
									href);
				}
				if (!im.Load(chemin, 4)) {
					logger.Warn("[SVG] <image> : « {0} » illisible -- image ignoree (le cadre reste vide).", chemin);
					return;
				}
			}
			if (!im.IsValid())
				return;

			const char *par = FindAttr(a, n, "preserveAspectRatio");

			Shape s2;
			s2.style = st.style;
			s2.img = std::move(im);
			s2.ix = x;
			s2.iy = y;
			s2.iw = w;
			s2.ih = h;
			LirePreserveAspectRatio(par, s2.fit, s2.alignX, s2.alignY);
			// les quatre coins de la boite, comme pour toute shape : ils donnent la
			// bbox de balayage, et ils suivent le transform du groupe parent.
			PathBuilder pb(s2);
			pb.StartContour(x, y);
			pb.LineTo(x + w, y);
			pb.LineTo(x + w, y + h);
			pb.LineTo(x, y + h);
			pb.Close();
			ApplyTransform(s2, st.xform);
			shapes.PushBack(std::move(s2));
		}

		void ShapeFromPath(NkVector<Shape> &shapes, const AttrPair *a, int32 n, const ParseState &st) noexcept {
			const char *d = FindAttr(a, n, "d");
			if (!d)
				return;
			Shape s2;
			s2.style = st.style;
			PathBuilder pb(s2);
			ParsePathD(d, pb);
			ApplyTransform(s2, st.xform);
			shapes.PushBack(std::move(s2));
		}

		// ═════════════════════════════════════════════════════════════════════════════
		// SECTION 7bis — <text> : LES CONTOURS DES GLYPHES, PAR UNE SOURCE INJECTEE
		// -----------------------------------------------------------------------------
		// NKImage NE DEPEND PAS DE NKFont, et ce n'est pas un detail de plomberie.
		// Faire dependre NKImage de NKFont reglait le texte d'un trait -- et faisait
		// payer les polices embarquees a TOUT consommateur de NKImage, y compris
		// celui qui ne veut que decoder un PNG. Nkentseu vise `Bare` (une console
		// sans OS) et le Web : ce poids n'y est pas negociable.
		//
		// Arbitrage du 2026-09-05 (sortie 2 de la porte des cycles) : LE BAS DEFINIT,
		// LE HAUT IMPLEMENTE ET INJECTE.
		//   - NKCore declare `NkIGlyphSource` / `NkIGlyphSink` -- le contrat minimal
		//     pour dessiner un glyphe : choisir une fonte, l'avance, les contours ;
		//   - NKFont l'implemente (`NkFontGlyphSource`) ;
		//   - ici on ne connait QUE le contrat. Aucune arete NKImage <-> NKFont dans
		//     les .jenga : c'est verifiable par un grep, et le banc le verifie.
		//
		// SANS SOURCE INJECTEE, le texte est SAUTE ET DIT (comme <use> ou <mask>),
		// jamais rendu vide en silence. L'application choisit, en une ligne :
		//     static NkFontGlyphSource glyphes;                 // NKFont
		//     NkSVGCodec::SetDefaultGlyphSource(&glyphes);
		//
		// POURQUOI LES CONTOURS ET PAS UN ATLAS. Un atlas est rasterise A UNE TAILLE
		// en pixels ; un SVG se re-rasterise a n'importe quelle taille (c'est son
		// interet) et porte des matrices qui tournent. Un atlas etire par une matrice
		// rend flou -- c'est ce que la sonde 81 de NkUIDesign mesure et refuse. Les
		// contours suivent la matrice sans perte, passent par le meme anticrenelage
		// que les formes, et acceptent un degrade.
		//
		// ⚠️ `font-size` EST LE CADRATIN (em) en SVG comme en CSS. NkFontAtlas
		// echelonne par (ascender - descender) : pour Inter les deux different d'un
		// facteur mesure a 1,25. On suit LA NORME -- un .svg doit etre juste pour un
		// navigateur d'abord -- et le temoin croise NOMME l'ecart avec le peintre.
		// ═════════════════════════════════════════════════════════════════════════════

		/// Recoit les contours d'un glyphe et les pousse dans une Shape. Les courbes
		/// sont aplaties ICI, avec la tolerance du rasteriseur qui va les remplir --
		/// c'est pour ca que la source rend des courbes et non des points.
		class GlyphSink final : public NkIGlyphSink {
			public:
				explicit GlyphSink(PathBuilder &pb) noexcept : mPb(pb) {
				}

				void GlyphMoveTo(float32 x, float32 y) noexcept override {
					if (mOuvert)
						mPb.Close();
					mPb.StartContour(x, y);
					mOuvert = true;
					mX = x;
					mY = y;
				}
				void GlyphLineTo(float32 x, float32 y) noexcept override {
					mPb.LineTo(x, y);
					mX = x;
					mY = y;
				}
				void GlyphQuadTo(float32 cx, float32 cy, float32 x, float32 y) noexcept override {
					FlattenQuad(mPb, mX, mY, cx, cy, x, y);
					mX = x;
					mY = y;
				}
				void GlyphCubicTo(float32 c1x, float32 c1y, float32 c2x, float32 c2y, float32 x,
								  float32 y) noexcept override {
					FlattenCubic(mPb, mX, mY, c1x, c1y, c2x, c2y, x, y);
					mX = x;
					mY = y;
				}
				void GlyphClose() noexcept override {
					// UN CONTOUR DE GLYPHE EST TOUJOURS FERME : laisse ouvert, il fait
					// fuir le remplissage nonzero sur toute la ligne.
					if (mOuvert)
						mPb.Close();
					mOuvert = false;
				}

			private:
				PathBuilder &mPb;
				float32 mX = 0.f, mY = 0.f;
				bool mOuvert = false;
		};

		/// Un morceau de texte : le contenu direct d'un <text>, ou celui d'un <tspan>.
		struct TextFragment {
				char txt[512] = {0};
				float32 fontSize = 16.f;
				char famille[64] = {0};
				int32 weight = 400;
				NkSVGStyle style;
				char fillRef[64] = {0};
				bool posX = false, posY = false; ///< le fragment repositionne le curseur
				float32 x = 0.f, y = 0.f;
		};

		/// L'etat d'un <text> en cours : on accumule les fragments jusqu'a </text>,
		/// PARCE QUE `text-anchor` a besoin de la largeur TOTALE, qu'on ne connait
		/// qu'a la fermeture. Poser les glyphes au fil de l'eau aurait interdit
		/// « middle » et « end », ou impose une seconde passe.
		/// Ou poser la ligne de base par rapport a `y` (dominant-baseline).
		enum class Ligne : uint8 {
			Alphabetique = 0, ///< le defaut : y EST la ligne de base
			Milieu,			  ///< middle / central : le milieu de la hauteur d'x
			Suspendue,		  ///< hanging / text-before-edge : y est le HAUT
			Basse			  ///< text-after-edge : y est le BAS
		};

		struct TextState {
				bool actif = false;
				float32 x = 0.f, y = 0.f;
				int32 anchor = 0; ///< 0 start, 1 middle, 2 end
				bool preserve = false;
				Ligne ligne = Ligne::Alphabetique;
				float32 longueurVoulue = 0.f; ///< textLength ; 0 = libre
				bool ajusterGlyphes = false;  ///< lengthAdjust="spacingAndGlyphs"
				char cheminRef[64] = {0};	  ///< <textPath href="#id">
				NkVector<TextFragment> frags;
				NkSVGTransform xform = NkSVGTransform::Identity();
		};

		/// Decode les entites XML et applique la regle des blancs de SVG :
		/// sans xml:space="preserve", les blancs sont reduits a UN espace et les
		/// bords sont rognes -- sans quoi l'INDENTATION du fichier deviendrait du
		/// texte visible.
		void NettoyerTexte(const char *src, usize n, bool preserve, char *out, usize outSz) noexcept {
			usize o = 0;
			bool blancPrec = true; // rogne les blancs de tete
			for (usize i = 0; i < n && o + 1 < outSz; ++i) {
				char c = src[i];
				if (c == '&') { // entites
					if (std::strncmp(src + i, "&amp;", 5) == 0) { c = '&'; i += 4; }
					else if (std::strncmp(src + i, "&lt;", 4) == 0) { c = '<'; i += 3; }
					else if (std::strncmp(src + i, "&gt;", 4) == 0) { c = '>'; i += 3; }
					else if (std::strncmp(src + i, "&quot;", 6) == 0) { c = '"'; i += 5; }
					else if (std::strncmp(src + i, "&apos;", 6) == 0) { c = '\''; i += 5; }
					else if (src[i + 1] == '#') {
						usize j = i + 2;
						int32 code = 0;
						if (src[j] == 'x' || src[j] == 'X') {
							++j;
							while (j < n && HexDigit(src[j]) >= 0)
								code = code * 16 + HexDigit(src[j++]);
						} else {
							while (j < n && IsDigit(src[j]))
								code = code * 10 + (src[j++] - '0');
						}
						if (j < n && src[j] == ';' && code > 0) {
							char buf[8];
							const int32 nb = EncoderUTF8((uint32)code, buf, (int32)sizeof(buf));
							for (int32 k = 0; k < nb && o + 1 < outSz; ++k)
								out[o++] = buf[k];
							i = j;
							blancPrec = false;
							continue;
						}
					}
				}
				if (!preserve && IsSpace(c)) {
					if (blancPrec)
						continue;
					c = ' ';
					blancPrec = true;
				} else {
					blancPrec = false;
				}
				out[o++] = c;
			}
			if (!preserve)
				while (o > 0 && out[o - 1] == ' ')
					--o;
			out[o] = 0;
		}

		/// Choisit la fonte du fragment et DIT le repli, une fois par famille.
		void ChoisirFonte(NkIGlyphSource &src, const TextFragment &fr, SkipList &skips) noexcept {
			// « Inter, sans-serif » -> « Inter » : guillemets et espaces retires.
			char fam[64];
			fam[0] = 0;
			{
				const char *p = SkipWS(fr.famille);
				usize i = 0;
				while (*p && *p != ',' && i + 1 < sizeof(fam)) {
					if (*p != '"' && *p != '\'')
						fam[i++] = *p;
					++p;
				}
				while (i > 0 && IsSpace(fam[i - 1]))
					--i;
				fam[i] = 0;
			}
			if (!src.SelectFace(fam[0] ? fam : nullptr, fr.weight)) {
				char cle[96];
				std::snprintf(cle, sizeof(cle), "police:%s", fam[0] ? fam : "(defaut)");
				if (skips.Noter(cle))
					logger.Warn("[SVG] <text font-family=\"{0}\"> : police indisponible -- repli sur « {1} ». La "
								"forme des glyphes n'est PAS celle demandee.",
								fam[0] ? fam : "(defaut)", src.ActiveFamily());
			}
		}

		/// Le chemin d'un <textPath>, aplati en polyligne, avec ses longueurs cumulees.
		struct CheminTexte {
				NkVector<float32> xs, ys, cumul;
				float32 total = 0.f;

				/// Le point et la TANGENTE a la distance @p d le long du chemin.
				bool Au(float32 d, float32 &x, float32 &y, float32 &tx, float32 &ty) const noexcept {
					if (xs.Size() < 2u)
						return false;
					if (d < 0.f || d > total)
						return false; // hors du chemin : le glyphe n'est pas pose (norme)
					uint32 i = 1;
					while (i + 1u < xs.Size() && cumul[i] < d)
						++i;
					const float32 d0 = cumul[i - 1u], d1 = cumul[i];
					const float32 seg = (d1 - d0) > 1e-6f ? (d1 - d0) : 1e-6f;
					const float32 t = (d - d0) / seg;
					x = xs[i - 1u] + (xs[i] - xs[i - 1u]) * t;
					y = ys[i - 1u] + (ys[i] - ys[i - 1u]) * t;
					tx = (xs[i] - xs[i - 1u]) / seg;
					ty = (ys[i] - ys[i - 1u]) / seg;
					return true;
				}
		};

		/// La largeur d'un fragment, en unites utilisateur (somme des avances).
		/// Le CRENAGE n'est PAS applique : le peintre de NKGui ne l'applique pas non
		/// plus (`x += g->advanceX`), et le temoin croise compare les deux.
		float32 LargeurFragment(NkIGlyphSource &src, const TextFragment &fr) noexcept {
			const char *p = fr.txt;
			const char *end = p + std::strlen(p);
			float32 w = 0.f;
			while (p < end) {
				const uint32 cp = DecoderUTF8(p, end);
				if (cp == 0u)
					break;
				w += src.Advance(cp, fr.fontSize);
			}
			return w;
		}

		/// Pose les contours d'un fragment dans @p sh, a partir de (penX, penY).
		void ContoursDuFragment(Shape &sh, NkIGlyphSource &src, const TextFragment &fr, float32 &penX,
								float32 penY, float32 espaceSup = 0.f) noexcept {
			PathBuilder pb(sh);
			GlyphSink sink(pb);
			const char *p = fr.txt;
			const char *end = p + std::strlen(p);
			while (p < end) {
				const uint32 cp = DecoderUTF8(p, end);
				if (cp == 0u)
					break;
				src.Outline(cp, fr.fontSize, penX, penY, sink); // false = espace : normal
				penX += src.Advance(cp, fr.fontSize) + espaceSup;
			}
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// <textPath> — CHAQUE GLYPHE SUR SA TANGENTE
		// -----------------------------------------------------------------------------
		// Un glyphe est pose au MILIEU de son avance sur le chemin, tourne selon la
		// tangente a cet endroit. Le poser a son bord gauche ferait pivoter les
		// lettres autour d'un point excentre et les ferait « glisser » dans les
		// virages -- le milieu est ce qui donne un texte lisible en courbe.
		// ─────────────────────────────────────────────────────────────────────────────
		void ContoursSurChemin(Shape &sh, NkIGlyphSource &src, const TextFragment &fr, const CheminTexte &chemin,
							   float32 &distance, float32 espaceSup = 0.f) noexcept {
			const char *p = fr.txt;
			const char *end = p + std::strlen(p);
			while (p < end) {
				const uint32 cp = DecoderUTF8(p, end);
				if (cp == 0u)
					break;
				const float32 av = src.Advance(cp, fr.fontSize);
				float32 x = 0.f, y = 0.f, tx = 1.f, ty = 0.f;
				if (chemin.Au(distance + av * 0.5f, x, y, tx, ty)) {
					// on emet le glyphe a l'origine, puis on le TOURNE sur sa tangente :
					// la source ne sait poser qu'un glyphe droit, et c'est tres bien --
					// une affine par glyphe suffit, et elle vit ici.
					const uint32 avant = sh.xs.Size();
					PathBuilder pb(sh);
					GlyphSink sink(pb);
					src.Outline(cp, fr.fontSize, -av * 0.5f, 0.f, sink);
					for (uint32 k = avant; k < sh.xs.Size(); ++k) {
						const float32 gx = sh.xs[k], gy = sh.ys[k];
						sh.xs[k] = x + tx * gx - ty * gy;
						sh.ys[k] = y + ty * gx + tx * gy;
					}
				}
				distance += av + espaceSup;
			}
		}

		/// Ferme un <text> : c'est ICI qu'on connait la largeur totale, donc l'ancrage.
		void FinirTexte(NkVector<Shape> &shapes, TextState &ts, NkIGlyphSource *source, SkipList &skips,
						const CheminTexte *chemin = nullptr) noexcept {
			if (!ts.actif)
				return;
			ts.actif = false;
			if (ts.frags.IsEmpty())
				return;
			if (!source) {
				// PAS DE SOURCE : on ne peint rien, ET ON LE DIT. Un <text> avale en
				// silence donnerait une image incomplete qui a l'air complete.
				if (skips.Noter("text"))
					logger.Warn("[SVG] <text> saute : aucune source de glyphes fournie. Injectez-en une "
								"(NkSVGCodec::SetDefaultGlyphSource) -- NKFont en fournit une avec "
								"NkFontGlyphSource.");
				return;
			}

			// largeur totale (l'ancrage a besoin de la connaitre AVANT de poser)
			float32 largeur = 0.f;
			for (uint32 i = 0; i < ts.frags.Size(); ++i) {
				ChoisirFonte(*source, ts.frags[i], skips);
				largeur += LargeurFragment(*source, ts.frags[i]);
			}
			// ── textLength : le texte est ETIRE ou COMPRIME pour tenir la mesure ──
			//    « spacing » (le defaut) repartit l'ecart entre les glyphes ;
			//    « spacingAndGlyphs » met tout a l'echelle, glyphes compris.
			float32 espaceSup = 0.f, echelleGlyphes = 1.f;
			int32 nbGlyphes = 0;
			for (uint32 i = 0; i < ts.frags.Size(); ++i)
				for (const char *p = ts.frags[i].txt; *p; ++p)
					if (((uint8)*p & 0xC0u) != 0x80u)
						++nbGlyphes;
			if (ts.longueurVoulue > 0.f && largeur > 1e-4f) {
				if (ts.ajusterGlyphes) {
					echelleGlyphes = ts.longueurVoulue / largeur;
				} else if (nbGlyphes > 1) {
					espaceSup = (ts.longueurVoulue - largeur) / (float32)(nbGlyphes - 1);
				}
				largeur = ts.longueurVoulue;
			}

			float32 penX = ts.x;
			if (ts.anchor == 1)
				penX -= largeur * 0.5f;
			else if (ts.anchor == 2)
				penX -= largeur;
			// ── dominant-baseline : `y` ne designe pas toujours la ligne de base ──
			float32 penY = ts.y;
			if (ts.ligne != Ligne::Alphabetique && source) {
				float32 asc = 0.f, desc = 0.f;
				if (source->Metrics(ts.frags[0].fontSize, asc, desc)) {
					if (ts.ligne == Ligne::Milieu)
						penY += (asc - desc) * 0.5f;
					else if (ts.ligne == Ligne::Suspendue)
						penY += asc;
					else if (ts.ligne == Ligne::Basse)
						penY -= desc;
				} else if (skips.Noter("dominant-baseline-metriques")) {
					logger.Warn("[SVG] dominant-baseline demande, mais la source de glyphes ne fournit pas de "
								"metriques -- `y` est traite comme la ligne de base.");
				}
			}

			Shape sh;
			sh.style = ts.frags[0].style;
			// LES GLYPHES SE REMPLISSENT EN NONZERO. Un `fill-rule="evenodd"` herite
			// d'un ancetre creverait les contre-formes (le O deviendrait plein, le e
			// perdrait son oeil) : la regle du texte n'est pas celle du dessin.
			sh.style.fillEvenOdd = false;
			std::strncpy(sh.fillRef, ts.frags[0].fillRef, 63);

			bool grasSimule = false;
			for (uint32 i = 0; i < ts.frags.Size(); ++i) {
				TextFragment &fr = ts.frags[i];
				ChoisirFonte(*source, fr, skips);
				if (fr.posX)
					penX = fr.x;
				if (fr.posY)
					penY = fr.y;
				TextFragment ajuste = fr;
				if (echelleGlyphes != 1.f)
					ajuste.fontSize = fr.fontSize * echelleGlyphes;
				if (chemin)
					ContoursSurChemin(sh, *source, ajuste, *chemin, penX, espaceSup);
				else
					ContoursDuFragment(sh, *source, ajuste, penX, penY, espaceSup);
				if (fr.weight >= 600)
					grasSimule = true;
			}
			if (sh.contourStart.IsEmpty())
				return;
			// FAUSSE GRAISSE : les polices embarquees n'ont qu'une coupe. Un
			// font-weight >= 600 est rendu par un TRAIT de la couleur du remplissage
			// autour du glyphe -- plus d'encre, la meme forme. Ce n'est pas un vrai
			// Bold dessine par un typographe, et c'est dit.
			// LA SIMULATION N'A LIEU QUE SI LA SOURCE N'A PAS DE VRAIE COUPE GRASSE.
			// Quand l'application en declare une, on la prend -- un vrai Bold dessine
			// par un typographe n'a pas les memes formes qu'un contour epaissi, et
			// l'epaissir en plus le rendrait pateux.
			if (grasSimule && source->ActiveWeight() < 600) {
				sh.style.stroke = sh.style.fill;
				sh.style.strokeOpacity = sh.style.fillOpacity;
				sh.style.strokeWidth = ts.frags[0].fontSize * 0.03f;
				if (skips.Noter("font-weight"))
					logger.Warn("[SVG] font-weight >= 600 : la source de glyphes n'offre pas de coupe grasse -- "
								"graisse SIMULEE par un trait (ce n'est pas un vrai Bold). Une application peut "
								"en declarer une (NkFontGlyphSource::AjouterFonteFichier).");
			}
			ApplyTransform(sh, ts.xform);
			sh.ctm = ts.xform;
			shapes.PushBack(std::move(sh));
		}


		// ═════════════════════════════════════════════════════════════════════════════
		// SECTION 7ter — <use> / <symbol> : INSTANCIER PAR REFERENCE
		// -----------------------------------------------------------------------------
		// MESURE QUI A DECIDE DE CE PALIER : les 10 SVG de `Resources/` portent 1691
		// <path>, 1677 <g>... et 824 <use>. Un decodeur qui ignore <use> ne rend PAS
		// ces fichiers -- il en rend le decor.
		//
		// COMMENT, SANS DOM. Le parseur est un flux : il ne peut pas « revenir » sur
		// un element deja passe, ni voir un element pas encore lu (un <use> peut
		// referencer ce qui vient APRES lui). D'ou deux temps :
		//   1. UN INDEX, construit une fois : id -> l'intervalle du sous-arbre dans le
		//      buffer. Une passe lineaire, une pile de profondeurs, aucune copie ;
		//   2. a <use href="#id">, ON RE-PARSE ce fragment -- le meme code, avec un
		//      etat initial (matrice et style du <use>). Une instanciation N'EST QUE
		//      ca : relire le meme texte dans un autre repere.
		// Ecrire un second chemin de rendu pour les elements instancies aurait fait
		// diverger les deux (le depot a deja paye ce prix : « le peintre a ete ecrit
		// deux fois, independamment »).
		//
		// LA RECURSION EST BORNEE ET LE DIT : <use> peut se referencer lui-meme,
		// directement ou en cycle. On s'arrete a une profondeur fixe et on NOMME le
		// cas -- une pile qui explose ne laisse aucun message, un compteur si.
		// ═════════════════════════════════════════════════════════════════════════════

		/// Un element referencable : son id, et OU il vit dans le buffer.
		struct IdRange {
				char id[64] = {0};
				char tag[32] = {0};
				const char *debComplet = nullptr; ///< le '<' du tag ouvrant
				const char *finComplet = nullptr; ///< juste apres le '>' de la fermeture
				const char *debInterieur = nullptr;
				const char *finInterieur = nullptr;
		};

		/// Contexte d'instanciation, partage par tous les niveaux de <use>.
		struct UseCtx {
				NkVector<IdRange> index;
				NkVector<RegleCSS> regles; ///< les regles de tous les <style> du document
				char *nameBuf = nullptr;  ///< buffers PARTAGES : reallouer 1 Mo par <use>
				char *attrPool = nullptr; ///< couterait plus cher que tout le reste
				int32 profondeur = 0;
				bool indexe = false;
		};

		constexpr int32 kProfondeurUseMax = 12;

		/// Construit l'index des id. Une passe, une pile : O(n).
		void IndexerIds(const char *deb, const char *fin, NkVector<IdRange> &out) noexcept {
			struct Ouvert {
					int32 idx;	  ///< entree dans `out`, ou -1 si l'element n'a pas d'id
					char tag[32]; ///< pour apparier la fermeture
			};
			constexpr int32 kMaxPile = 64;
			Ouvert pile[kMaxPile];
			int32 nPile = 0;

			const char *p = deb;
			while (p < fin) {
				while (p < fin && *p != '<')
					++p;
				if (p >= fin)
					break;
				const char *debTag = p;
				++p;
				// commentaire / PI / doctype : sautes en bloc
				if (p + 2 < fin && p[0] == '!' && p[1] == '-' && p[2] == '-') {
					p += 3;
					while (p + 2 < fin && !(p[0] == '-' && p[1] == '-' && p[2] == '>'))
						++p;
					p = (p + 2 < fin) ? p + 3 : fin;
					continue;
				}
				if (p < fin && (*p == '?' || *p == '!')) {
					while (p < fin && *p != '>')
						++p;
					if (p < fin)
						++p;
					continue;
				}
				// fermeture </tag>
				if (p < fin && *p == '/') {
					++p;
					char nom[32];
					int32 ni = 0;
					while (p < fin && *p != '>' && !IsSpace(*p) && ni + 1 < (int32)sizeof(nom))
						nom[ni++] = *p++;
					nom[ni] = 0;
					while (p < fin && *p != '>')
						++p;
					if (p < fin)
						++p;
					if (nPile > 0 && std::strcmp(pile[nPile - 1].tag, nom) == 0) {
						--nPile;
						const int32 idx = pile[nPile].idx;
						if (idx >= 0) {
							out[(uint32)idx].finInterieur = debTag;
							out[(uint32)idx].finComplet = p;
						}
					}
					continue;
				}
				// ouverture
				char nom[32];
				int32 ni = 0;
				while (p < fin && *p != '>' && *p != '/' && !IsSpace(*p) && ni + 1 < (int32)sizeof(nom))
					nom[ni++] = *p++;
				nom[ni] = 0;
				// l'attribut id="..." se lit a la main : on ne veut pas des buffers du
				// parseur complet pour une passe d'indexation
				char id[64];
				id[0] = 0;
				const char *q = p;
				while (q < fin && *q != '>') {
					if ((q[0] == 'i' || q[0] == 'I') && (q[1] == 'd' || q[1] == 'D') && q > p &&
						(IsSpace(q[-1]) || q[-1] == '"' || q[-1] == '\'')) {
						const char *r = q + 2;
						while (r < fin && IsSpace(*r))
							++r;
						if (r < fin && *r == '=') {
							++r;
							while (r < fin && IsSpace(*r))
								++r;
							if (r < fin && (*r == '"' || *r == '\'')) {
								const char quote = *r++;
								int32 ii = 0;
								while (r < fin && *r != quote && ii + 1 < (int32)sizeof(id))
									id[ii++] = *r++;
								id[ii] = 0;
								break;
							}
						}
					}
					++q;
				}
				while (p < fin && *p != '>')
					++p;
				const bool selfClose = (p > debTag && p[-1] == '/');
				if (p < fin)
					++p;

				int32 idx = -1;
				if (id[0] && out.Size() < 4096u) {
					IdRange r;
					std::strncpy(r.id, id, sizeof(r.id) - 1);
					std::strncpy(r.tag, nom, sizeof(r.tag) - 1);
					r.debComplet = debTag;
					r.debInterieur = p;
					r.finInterieur = p;
					r.finComplet = p;
					out.PushBack(r);
					idx = (int32)out.Size() - 1;
				}
				if (!selfClose && nPile < kMaxPile) {
					pile[nPile].idx = idx;
					std::strncpy(pile[nPile].tag, nom, sizeof(pile[nPile].tag) - 1);
					pile[nPile].tag[sizeof(pile[nPile].tag) - 1] = 0;
					++nPile;
				}
			}
		}

		/// Le parseur sait-il quoi faire de ce tag ? Une seule liste, pour que
		/// « connu » veuille dire la meme chose partout -- et pour qu'ajouter un
		/// palier consiste a l'ajouter ICI aussi, sinon il continuerait a se declarer
		/// saute alors qu'il est peint.
		bool TagConnu(const char *t) noexcept {
			static const char *const kConnus[] = {"svg",	 "g",		 "defs",  "symbol",	  "use",	 "rect",
												  "circle",	 "ellipse",	 "line",  "polyline", "polygon", "path",
												  "image",	 "text",	 "tspan", "title",	  "desc",	 "metadata",
												  "style",	 "clipPath", "mask",  "pattern",  "stop",  "filter",  "linearGradient",
												  "radialGradient"};
			for (usize i = 0; i < sizeof(kConnus) / sizeof(kConnus[0]); ++i)
				if (std::strcmp(t, kConnus[i]) == 0)
					return true;
			return std::strncmp(t, "fe", 2) == 0; // les primitives de filtre se disent ailleurs
		}

		/// Retrouve un element par son id (« #id » ou « id »).
		const IdRange *TrouverId(const UseCtx &ctx, const char *href) noexcept {
			if (!href)
				return nullptr;
			const char *id = (*href == '#') ? href + 1 : href;
			if (!*id)
				return nullptr;
			for (uint32 i = 0; i < ctx.index.Size(); ++i)
				if (std::strcmp(ctx.index[i].id, id) == 0)
					return &ctx.index[i];
			return nullptr;
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// SECTION 8 — Parser XML stream-based avec stack <g>
		// ─────────────────────────────────────────────────────────────────────────────

		/// Parse les attributs d'une balise jusqu'a '>'. Les noms et valeurs sont
		/// stockes dans des buffers fournis par le caller (nameBuf + attrPool partage).
		int32 ParseTagAttrs(const char *&p, const char *end, AttrPair *attrs, char *nameBuf, usize &nameOff,
							usize nameBufSize, char *attrPool, usize &poolOff, usize attrPoolSize) noexcept {
			int32 num = 0;
			while (p < end && *p != '>' && *p != '/') {
				while (p < end && IsSpace(*p))
					++p;
				if (p >= end || *p == '>' || *p == '/')
					break;
				// Nom de l'attribut.
				const usize nameStart = nameOff;
				while (p < end && *p != '=' && !IsSpace(*p) && *p != '>' && *p != '/' && nameOff + 1 < nameBufSize) {
					nameBuf[nameOff++] = *p++;
				}
				if (nameOff < nameBufSize)
					nameBuf[nameOff++] = 0;
				while (p < end && IsSpace(*p))
					++p;
				// Valeur.
				const char *valPtr = "";
				if (p < end && *p == '=') {
					++p;
					while (p < end && IsSpace(*p))
						++p;
					const char q = (*p == '"' || *p == '\'') ? *p++ : ' ';
					valPtr = (const char *)(attrPool + poolOff);
					usize vi = 0;
					while (p < end && *p != q && *p != '>' && poolOff + vi + 1 < attrPoolSize) {
						attrPool[poolOff + vi++] = *p++;
					}
					// Si pool sature, skip le reste pour avancer correctement.
					while (p < end && *p != q && *p != '>')
						++p;
					attrPool[poolOff + vi] = 0;
					poolOff += vi + 1;
					if (*p == q)
						++p;
				}
				if (num < 64) {
					attrs[num].name = nameBuf + nameStart;
					attrs[num].value = valPtr;
					++num;
				}
			}
			return num;
		}

		/// Lit le prochain tag dans le flux XML.
		/// @return 0 = fin, 1 = balise ouvrante, 2 = balise self-close, 3 = balise fermante.
		int32 ReadNextTag(const char *&p, const char *end, char *tagBuf, usize tagBufSize, AttrPair *attrs,
						  int32 &outNumAttrs, char *nameBuf, usize &nameOff, usize nameBufSize, char *attrPool,
						  usize &poolOff, usize attrPoolSize) noexcept {
			while (p < end) {
				// Skip texte / espace entre tags.
				while (p < end && *p != '<')
					++p;
				if (p >= end)
					return 0;
				++p;
				// Commentaire <!-- ... -->
				if (p + 2 < end && p[0] == '!' && p[1] == '-' && p[2] == '-') {
					p += 3;
					while (p + 2 < end && !(p[0] == '-' && p[1] == '-' && p[2] == '>'))
						++p;
					if (p + 2 < end)
						p += 3;
					continue;
				}
				// PI <?...?> ou DOCTYPE <!...>
				if (p < end && (*p == '?' || *p == '!')) {
					while (p < end && *p != '>')
						++p;
					if (p < end)
						++p;
					continue;
				}
				// Closing tag </tag>
				if (p < end && *p == '/') {
					++p;
					usize ti = 0;
					while (p < end && *p != '>' && !IsSpace(*p) && ti + 1 < tagBufSize) {
						tagBuf[ti++] = *p++;
					}
					tagBuf[ti] = 0;
					while (p < end && *p != '>')
						++p;
					if (p < end)
						++p;
					outNumAttrs = 0;
					return 3;
				}
				// Opening tag <tag attr=...>
				usize ti = 0;
				while (p < end && *p != '>' && *p != '/' && !IsSpace(*p) && ti + 1 < tagBufSize) {
					tagBuf[ti++] = *p++;
				}
				tagBuf[ti] = 0;
				// On reset les offsets de pool a chaque tag (les valeurs precedentes
				// ne sont plus utilisees apres traitement du tag).
				nameOff = 0;
				poolOff = 0;
				outNumAttrs =
					ParseTagAttrs(p, end, attrs, nameBuf, nameOff, nameBufSize, attrPool, poolOff, attrPoolSize);
				const bool self = (p < end && *p == '/');
				while (p < end && *p != '>')
					++p;
				if (p < end)
					++p;
				return self ? 2 : 1;
			}
			return 0;
		}

		/// Parse l'ensemble du document SVG -> liste de shapes + gradients + viewBox + dim.
		/// @param useCtx    index des id + buffers partages (instanciation par <use>)
		/// @param etatInit  nullptr a la racine ; sinon l'etat herite du <use> qui
		///                  instancie ce fragment (sa matrice, son style).
		void ParseSVGDocument(const char *xml, usize xmlLen, NkVector<Shape> &shapes, NkVector<Gradient> &gradients,
							  NkVector<Filtre> &filtres, NkVector<ClipPath> &clips, NkVector<Motif> &motifs,
							  float32 &vbX, float32 &vbY,
							  float32 &vbW, float32 &vbH,
							  float32 &svgW, float32 &svgH, SkipList &skips, const char *baseDir,
							  NkIGlyphSource *glyphes, UseCtx &useCtx,
							  const ParseState *etatInit = nullptr) noexcept {
			const bool racine = (etatInit == nullptr);
			if (racine) {
				vbX = vbY = 0.f;
				vbW = 0.f;
				vbH = 0.f;
				svgW = 0.f;
				svgH = 0.f;
			}

			constexpr usize kAttrPoolSize = 1024 * 1024; // 1 MB par tag
			constexpr usize kNameBufSize = 64 * 1024;	 // 64 KB noms d'attrs
			// LES BUFFERS SONT PARTAGES par tous les niveaux d'instanciation : en
			// reallouer 1 Mo a chaque <use> couterait plus cher que tout le reste du
			// decodage (824 <use> mesures dans Resources/). C'est sur parce que les
			// valeurs d'attributs sont COPIEES avant toute recursion.
			const bool proprietaireBufs = (useCtx.nameBuf == nullptr);
			if (proprietaireBufs) {
				useCtx.nameBuf = (char *)NkAlloc(kNameBufSize);
				useCtx.attrPool = (char *)NkAlloc(kAttrPoolSize);
			}
			char *nameBuf = useCtx.nameBuf;
			char *attrPool = useCtx.attrPool;
			if (!nameBuf || !attrPool) {
				if (proprietaireBufs) {
					if (nameBuf)
						NkFree(nameBuf);
					if (attrPool)
						NkFree(attrPool);
					useCtx.nameBuf = nullptr;
					useCtx.attrPool = nullptr;
				}
				return;
			}
			// L'INDEX, une seule fois pour tout le document.
			if (!useCtx.indexe) {
				useCtx.indexe = true;
				IndexerIds(xml, xml + xmlLen, useCtx.index);
				// LE CSS AUSSI EST UNE PRE-PASSE : une regle peut etre ecrite APRES
				// les elements qu'elle habille, et un <style> peut vivre n'importe ou.
				CollecterCSS(xml, xml + xmlLen, useCtx.regles, skips);
			}
			usize nameOff = 0, poolOff = 0;

			char tagBuf[64];
			AttrPair attrs[64];

			// Stack <g> avec depth max raisonnable.
			constexpr int32 kMaxDepth = 64;
			ParseState stack[kMaxDepth];
			int32 depth = 0;
			stack[0] = etatInit ? *etatInit : ParseState{};

			int32 numAttrs = 0;
			bool gotSvg = false;
			TextState texte;
			Filtre curFiltre;
			bool dansFiltre = false;
			ClipPath curClip;
			bool dansClip = false;
			int32 defsAvantClip = 0;
			uint32 shapesAvantClip = 0;
			Motif curMotif;
			bool dansMotif = false;
			int32 defsAvantMotif = 0;
			uint32 shapesAvantMotif = 0;
			int32 prochaineInstance = 0;
			int32 defsDepth = 0; // > 0 = on est dans <defs> : shapes non rendues
			bool buildingGrad = false;
			Gradient curGrad; // gradient en cours de construction (stops)
			// BUG FIX : end est calcule UNE seule fois. xml est passe par reference et
			// est avance par le parser ; si on recalcule "xml + xmlLen" a chaque tour,
			// end se decale avec xml et finit par sortir du buffer (SEGV sur memoire
			// non mappee lors du parsing de longs SVG).
			const char *const xmlEnd = xml + xmlLen;
			while (true) {
				const int32 kind =
					ReadNextTag(reinterpret_cast<const char *&>(xml), xmlEnd, tagBuf, sizeof(tagBuf), attrs, numAttrs,
								nameBuf, nameOff, kNameBufSize, attrPool, poolOff, kAttrPoolSize);
				if (kind == 0)
					break;

				if (kind == 3) {
					// Closing tag : depile la stack / la profondeur defs / finalise un gradient.
					if (std::strcmp(tagBuf, "text") == 0) {
						// LE CHEMIN D'UN <textPath> est relu ICI, depuis l'element
						// reference : on a l'index des id, et le `d` d'un <path> se
						// reparse avec le meme PathBuilder que partout ailleurs.
						CheminTexte chemin;
						bool aChemin = false;
						if (texte.cheminRef[0]) {
							const IdRange *cible = TrouverId(useCtx, texte.cheminRef);
							if (cible) {
								char dbuf[4096];
								dbuf[0] = 0;
								// ⚠️ « d= » SE TROUVE AUSSI DANS « id= ». Chercher la
								// sous-chaine rendait « arc » (la valeur de l'id) comme
								// trace, et <textPath> ne peignait rien -- sans erreur,
								// sans message. Un attribut se cherche sur sa FRONTIERE :
								// precede d'un blanc ou du nom du tag.
								const char *q = cible->debComplet;
								while (q + 1 < cible->debInterieur &&
									   !(q[0] == 'd' && q[1] == '=' && q > cible->debComplet && IsSpace(q[-1])))
									++q;
								if (q + 2 < cible->debInterieur) {
									q += 2;
									if (*q == '"' || *q == '\'') {
										const char quote = *q++;
										usize k = 0;
										while (q < cible->debInterieur && *q != quote && k + 1 < sizeof(dbuf))
											dbuf[k++] = *q++;
										dbuf[k] = 0;
									}
								}
								if (dbuf[0]) {
									Shape tmpSh;
									PathBuilder pbc(tmpSh);
									ParsePathD(dbuf, pbc);
									for (uint32 c0 = 0; c0 < tmpSh.contourStart.Size(); ++c0) {
										const int32 st0 = tmpSh.contourStart[c0];
										const int32 ln0 = tmpSh.contourLen[c0];
										for (int32 k = 0; k < ln0; ++k) {
											chemin.xs.PushBack(tmpSh.xs[(uint32)(st0 + k)]);
											chemin.ys.PushBack(tmpSh.ys[(uint32)(st0 + k)]);
										}
									}
									chemin.cumul.PushBack(0.f);
									for (uint32 k = 1; k < chemin.xs.Size(); ++k) {
										const float32 ddx = chemin.xs[k] - chemin.xs[k - 1u];
										const float32 ddy = chemin.ys[k] - chemin.ys[k - 1u];
										chemin.total += std::sqrt(ddx * ddx + ddy * ddy);
										chemin.cumul.PushBack(chemin.total);
									}
									aChemin = (chemin.xs.Size() >= 2u && chemin.total > 1e-4f);
								}
							}
							if (!aChemin && skips.Noter("textPath-cible"))
								logger.Warn("[SVG] <textPath href=\"{0}\"> : chemin introuvable ou vide -- le "
											"texte est pose en ligne droite.",
											texte.cheminRef);
						}
						FinirTexte(shapes, texte, glyphes, skips, aChemin ? &chemin : nullptr);
						continue;
					}
					if (std::strcmp(tagBuf, "tspan") == 0 || std::strcmp(tagBuf, "textPath") == 0)
						continue;
					if (std::strcmp(tagBuf, "filter") == 0) {
						if (dansFiltre) {
							filtres.PushBack(std::move(curFiltre));
							curFiltre.Vider();
							dansFiltre = false;
						}
						continue;
					}
					if (std::strcmp(tagBuf, "pattern") == 0) {
						if (dansMotif) {
							for (uint32 k = shapesAvantMotif; k < shapes.Size(); ++k)
								curMotif.formes.PushBack(std::move(shapes[k]));
							while (shapes.Size() > shapesAvantMotif)
								shapes.PopBack();
							motifs.PushBack(std::move(curMotif));
							curMotif.Vider();
							dansMotif = false;
							defsDepth = defsAvantMotif;
						}
						continue;
					}
					if (std::strcmp(tagBuf, "clipPath") == 0 || std::strcmp(tagBuf, "mask") == 0) {
						if (dansClip) {
							// LES FORMES DU CLIP SORTENT DE LA LISTE A PEINDRE : elles
							// decrivent une decoupe, elles ne se dessinent pas.
							for (uint32 k = shapesAvantClip; k < shapes.Size(); ++k)
								curClip.formes.PushBack(std::move(shapes[k]));
							while (shapes.Size() > shapesAvantClip)
								shapes.PopBack();
							clips.PushBack(std::move(curClip));
							curClip.Vider();
							dansClip = false;
							defsDepth = defsAvantClip;
						}
						continue;
					}
					if (std::strcmp(tagBuf, "g") == 0 && depth > 0) {
						--depth;
					} else if ((std::strcmp(tagBuf, "defs") == 0 || std::strcmp(tagBuf, "symbol") == 0) &&
							   defsDepth > 0) {
						--defsDepth;
					} else if ((std::strcmp(tagBuf, "linearGradient") == 0 ||
								std::strcmp(tagBuf, "radialGradient") == 0) &&
							   buildingGrad) {
						gradients.PushBack(std::move(curGrad));
						curGrad = Gradient();
						buildingGrad = false;
					}
					continue;
				}

				ParseState &cur = stack[depth];

				if (std::strcmp(tagBuf, "svg") == 0 && !gotSvg) {
					const char *vb = FindAttr(attrs, numAttrs, "viewBox");
					if (vb) {
						const char *pp = vb;
						vbX = ParseFloat(pp, &pp);
						pp = SkipWSComma(pp);
						vbY = ParseFloat(pp, &pp);
						pp = SkipWSComma(pp);
						vbW = ParseFloat(pp, &pp);
						pp = SkipWSComma(pp);
						vbH = ParseFloat(pp, &pp);
					}
					const char *w = FindAttr(attrs, numAttrs, "width");
					const char *h = FindAttr(attrs, numAttrs, "height");
					if (w)
						svgW = ParseFloat(w);
					if (h)
						svgH = ParseFloat(h);
					// <svg> peut aussi porter fill/stroke par defaut + transform.
					cur.style = MergeStyle(cur.style, attrs, numAttrs, &useCtx.regles, "svg");
					UpdateRefs(cur.fillRef, cur.strokeRef, attrs, numAttrs);
					const char *tr = FindAttr(attrs, numAttrs, "transform");
					if (tr)
						cur.xform = cur.xform * NkSVGTransform::Parse(tr);
					gotSvg = true;
					continue;
				}

				if (std::strcmp(tagBuf, "g") == 0) {
					// Ouverture <g> : push un nouveau state derive de cur.
					// BUG FIX : on ne pop QU'A la fermeture </g>, pas a la sortie du
					// tag courant. Les <path> enfants heritent ainsi du transform/style.
					if (kind == 1 && depth + 1 < kMaxDepth) {
						ParseState next = cur;
						next.style = MergeStyle(cur.style, attrs, numAttrs, &useCtx.regles, "g");
						UpdateRefs(next.fillRef, next.strokeRef, attrs, numAttrs);
						const char *tr = FindAttr(attrs, numAttrs, "transform");
						if (tr)
							next.xform = cur.xform * NkSVGTransform::Parse(tr);
						// clip-path ET mask se cumulent dans la MEME liste : tous deux
						// sont des masques qui se multiplient, seule leur fabrication
						// differe.
						for (int32 q = 0; q < 2; ++q) {
							const char *clp = FindAttr(attrs, numAttrs, q == 0 ? "clip-path" : "mask");
							char refClip[64];
							if (clp && ParseUrlRef(clp, refClip, sizeof(refClip)) &&
								next.nClips < (int32)(sizeof(next.clipRefs) / sizeof(next.clipRefs[0]))) {
								std::strncpy(next.clipRefs[next.nClips], refClip, 63);
								next.clipRefs[next.nClips][63] = 0;
								++next.nClips;
							}
						}
						const char *flt = FindAttr(attrs, numAttrs, "filter");
						char refFiltre[64];
						if (flt && ParseUrlRef(flt, refFiltre, sizeof(refFiltre))) {
							std::strncpy(next.filterRef, refFiltre, 63);
							next.filterRef[63] = 0;
							next.filterInst = ++prochaineInstance;
						}
						stack[++depth] = next;
					}
					// <g/> self-closed = groupe vide, on ignore.
					continue;
				}

				// ── <pattern> : ses formes sont CAPTEES pour devenir une tuile ───
				if (std::strcmp(tagBuf, "pattern") == 0) {
					curMotif.Vider();
					const char *pid = FindAttr(attrs, numAttrs, "id");
					if (pid) {
						std::strncpy(curMotif.id, pid, 63);
						curMotif.id[63] = 0;
					}
					const char *ph = FindAttr(attrs, numAttrs, "href");
					if (!ph)
						ph = FindAttr(attrs, numAttrs, "xlink:href");
					if (ph) {
						std::strncpy(curMotif.href, (*ph == '#') ? ph + 1 : ph, 63);
						curMotif.href[63] = 0;
					}
					curMotif.x = ParsePct(FindAttr(attrs, numAttrs, "x"), 0.f);
					curMotif.y = ParsePct(FindAttr(attrs, numAttrs, "y"), 0.f);
					curMotif.w = ParsePct(FindAttr(attrs, numAttrs, "width"), 0.f);
					curMotif.h = ParsePct(FindAttr(attrs, numAttrs, "height"), 0.f);
					const char *pu = FindAttr(attrs, numAttrs, "patternUnits");
					curMotif.userSpace = (pu && std::strcmp(pu, "userSpaceOnUse") == 0);
					const char *pcu = FindAttr(attrs, numAttrs, "patternContentUnits");
					curMotif.contenuUserSpace = !(pcu && std::strcmp(pcu, "objectBoundingBox") == 0);
					const char *pt = FindAttr(attrs, numAttrs, "patternTransform");
					if (pt)
						curMotif.xform = NkSVGTransform::Parse(pt);
					const char *pvb = FindAttr(attrs, numAttrs, "viewBox");
					if (pvb) {
						const char *pp = pvb;
						curMotif.vbX = ParseFloat(pp, &pp);
						pp = SkipWSComma(pp);
						curMotif.vbY = ParseFloat(pp, &pp);
						pp = SkipWSComma(pp);
						curMotif.vbW = ParseFloat(pp, &pp);
						pp = SkipWSComma(pp);
						curMotif.vbH = ParseFloat(pp, &pp);
						curMotif.aViewBox = (curMotif.vbW > 0.f && curMotif.vbH > 0.f);
					}
					dansMotif = true;
					shapesAvantMotif = shapes.Size();
					defsAvantMotif = defsDepth;
					defsDepth = 0; // ses formes doivent EXISTER pour devenir la tuile
					if (kind == 2) {
						motifs.PushBack(std::move(curMotif));
						curMotif.Vider();
						dansMotif = false;
						defsDepth = defsAvantMotif;
					}
					continue;
				}

				// ── <clipPath> et <mask> : leurs formes sont CAPTEES, pas peintes ─
				if (std::strcmp(tagBuf, "clipPath") == 0 || std::strcmp(tagBuf, "mask") == 0) {
					curClip.Vider();
					curClip.masque = (tagBuf[0] == 'm');
					const char *cid = FindAttr(attrs, numAttrs, "id");
					if (cid) {
						std::strncpy(curClip.id, cid, 63);
						curClip.id[63] = 0;
					}
					const char *cu = FindAttr(attrs, numAttrs, curClip.masque ? "maskContentUnits" : "clipPathUnits");
					curClip.userSpace = !(cu && std::strcmp(cu, "objectBoundingBox") == 0);
					if (curClip.masque) {
						// mask-type, en attribut ou en propriete de style
						const char *mt = FindAttr(attrs, numAttrs, "mask-type");
						char buf[32];
						const char *css = FindAttr(attrs, numAttrs, "style");
						if (!mt && css && GetCSSProp(css, "mask-type", buf, sizeof(buf)))
							mt = buf;
						curClip.typeAlpha = (mt && std::strcmp(mt, "alpha") == 0);
					}
					dansClip = true;
					shapesAvantClip = shapes.Size();
					// un <clipPath> vit presque toujours dans <defs>, qui bloque la
					// creation des formes : ici on la REACTIVE, puis on les recupere.
					defsAvantClip = defsDepth;
					defsDepth = 0;
					if (kind == 2) {
						clips.PushBack(std::move(curClip));
						curClip.Vider();
						dansClip = false;
						defsDepth = defsAvantClip;
					}
					continue;
				}

				// ── <filter> et ses primitives ───────────────────────────────────
				if (std::strcmp(tagBuf, "filter") == 0) {
					curFiltre.Vider();
					const char *fid = FindAttr(attrs, numAttrs, "id");
					if (fid) {
						std::strncpy(curFiltre.id, fid, 63);
						curFiltre.id[63] = 0;
					}
					dansFiltre = true;
					if (kind == 2) { // <filter/> vide
						filtres.PushBack(std::move(curFiltre));
						curFiltre.Vider();
						dansFiltre = false;
					}
					continue;
				}
				if (std::strncmp(tagBuf, "fe", 2) == 0) {
					if (!dansFiltre)
						continue;
					// feMergeNode n'est pas une primitive : c'est une ENTREE de la
					// derniere feMerge ouverte.
					if (std::strcmp(tagBuf, "feMergeNode") == 0) {
						if (!curFiltre.prims.IsEmpty()) {
							Primitive &p = curFiltre.prims[curFiltre.prims.Size() - 1];
							const char *mi = FindAttr(attrs, numAttrs, "in");
							if (p.type == PrimType::Assemblage && p.nMerges < 8) {
								if (mi) {
									std::strncpy(p.merges[p.nMerges], mi, 31);
									p.merges[p.nMerges][31] = 0;
								} else {
									p.merges[p.nMerges][0] = 0;
								}
								++p.nMerges;
							}
						}
						continue;
					}
					Primitive p;
					if (std::strcmp(tagBuf, "feGaussianBlur") == 0)
						p.type = PrimType::Flou;
					else if (std::strcmp(tagBuf, "feOffset") == 0)
						p.type = PrimType::Decalage;
					else if (std::strcmp(tagBuf, "feFlood") == 0)
						p.type = PrimType::Aplat;
					else if (std::strcmp(tagBuf, "feComposite") == 0)
						p.type = PrimType::Composition;
					else if (std::strcmp(tagBuf, "feColorMatrix") == 0)
						p.type = PrimType::Matrice;
					else if (std::strcmp(tagBuf, "feBlend") == 0)
						p.type = PrimType::Fusion;
					else if (std::strcmp(tagBuf, "feMerge") == 0)
						p.type = PrimType::Assemblage;
					else if (std::strcmp(tagBuf, "feDropShadow") == 0)
						p.type = PrimType::Ombre;
					else {
						if (skips.Noter(tagBuf))
							logger.Warn("[SVG] primitive de filtre non geree, sautee : <{0}> -- le graphe "
										"continue SANS elle (son entree passe telle quelle).",
										tagBuf);
						// on l'ajoute quand meme en « inconnue » : elle laisse passer
						// son entree, ce qui garde le CHAINAGE des `result` intact.
						p.type = PrimType::Inconnue;
					}
					const char *ai = FindAttr(attrs, numAttrs, "in");
					if (ai) {
						std::strncpy(p.in, ai, 31);
						p.in[31] = 0;
					}
					const char *ai2 = FindAttr(attrs, numAttrs, "in2");
					if (ai2) {
						std::strncpy(p.in2, ai2, 31);
						p.in2[31] = 0;
					}
					const char *ar = FindAttr(attrs, numAttrs, "result");
					if (ar) {
						std::strncpy(p.result, ar, 31);
						p.result[31] = 0;
					}
					// stdDeviation accepte « x » ou « x y »
					const char *sd = FindAttr(attrs, numAttrs, "stdDeviation");
					if (sd) {
						const char *pp = sd;
						p.ecartX = ParseFloat(pp, &pp);
						pp = SkipWSComma(pp);
						p.ecartY = (*pp) ? ParseFloat(pp, &pp) : p.ecartX;
					} else if (p.type == PrimType::Ombre) {
						p.ecartX = p.ecartY = 2.f;
					}
					p.dx = ParseFloat(FindAttr(attrs, numAttrs, "dx"));
					p.dy = ParseFloat(FindAttr(attrs, numAttrs, "dy"));
					const char *fc = FindAttr(attrs, numAttrs, "flood-color");
					p.couleur = fc ? NkSVGColor::Parse(fc) : NkSVGColor::Black();
					const char *fo = FindAttr(attrs, numAttrs, "flood-opacity");
					if (fo) {
						float32 o = ParseFloat(fo);
						if (o < 0.f)
							o = 0.f;
						if (o > 1.f)
							o = 1.f;
						p.couleur.a = (uint8)((float32)p.couleur.a * o);
					}
					p.couleur.none = false;
					const char *op = FindAttr(attrs, numAttrs, "operator");
					if (op) {
						if (std::strcmp(op, "in") == 0) p.op = OpComposite::In;
						else if (std::strcmp(op, "out") == 0) p.op = OpComposite::Out;
						else if (std::strcmp(op, "atop") == 0) p.op = OpComposite::Atop;
						else if (std::strcmp(op, "xor") == 0) p.op = OpComposite::Xor;
						else if (std::strcmp(op, "arithmetic") == 0) p.op = OpComposite::Arithmetique;
					}
					const char *mo = FindAttr(attrs, numAttrs, "mode");
					if (mo) {
						if (std::strcmp(mo, "multiply") == 0) p.fusion = OpFusion::Multiplier;
						else if (std::strcmp(mo, "screen") == 0) p.fusion = OpFusion::Ecran;
						else if (std::strcmp(mo, "darken") == 0) p.fusion = OpFusion::Assombrir;
						else if (std::strcmp(mo, "lighten") == 0) p.fusion = OpFusion::Eclaircir;
					}
					p.k1 = ParseFloat(FindAttr(attrs, numAttrs, "k1"));
					p.k2 = ParseFloat(FindAttr(attrs, numAttrs, "k2"));
					p.k3 = ParseFloat(FindAttr(attrs, numAttrs, "k3"));
					p.k4 = ParseFloat(FindAttr(attrs, numAttrs, "k4"));
					// feColorMatrix : type matrix / saturate / hueRotate / luminanceToAlpha
					if (p.type == PrimType::Matrice) {
						const char *ty = FindAttr(attrs, numAttrs, "type");
						const char *va = FindAttr(attrs, numAttrs, "values");
						for (int32 i = 0; i < 20; ++i)
							p.mat[i] = 0.f;
						p.mat[0] = p.mat[6] = p.mat[12] = p.mat[18] = 1.f; // identite
						p.aMatrice = true;
						if (!ty || std::strcmp(ty, "matrix") == 0) {
							if (va) {
								const char *pp = va;
								for (int32 i = 0; i < 20 && *pp; ++i) {
									pp = SkipWSComma(pp);
									if (!*pp)
										break;
									p.mat[i] = ParseFloat(pp, &pp);
								}
							}
						} else if (std::strcmp(ty, "saturate") == 0) {
							const float32 sv = va ? ParseFloat(va) : 1.f;
							p.mat[0] = 0.213f + 0.787f * sv; p.mat[1] = 0.715f - 0.715f * sv; p.mat[2] = 0.072f - 0.072f * sv;
							p.mat[5] = 0.213f - 0.213f * sv; p.mat[6] = 0.715f + 0.285f * sv; p.mat[7] = 0.072f - 0.072f * sv;
							p.mat[10] = 0.213f - 0.213f * sv; p.mat[11] = 0.715f - 0.715f * sv; p.mat[12] = 0.072f + 0.928f * sv;
							p.mat[3] = p.mat[4] = p.mat[8] = p.mat[9] = p.mat[13] = p.mat[14] = 0.f;
							p.mat[18] = 1.f;
						} else if (std::strcmp(ty, "luminanceToAlpha") == 0) {
							for (int32 i = 0; i < 20; ++i)
								p.mat[i] = 0.f;
							p.mat[15] = 0.2125f;
							p.mat[16] = 0.7154f;
							p.mat[17] = 0.0721f;
						} else if (skips.Noter("feColorMatrix-type")) {
							logger.Warn("[SVG] feColorMatrix type=\"{0}\" non gere -- identite appliquee.", ty);
						}
					}
					curFiltre.prims.PushBack(p);
					continue;
				}

				// <defs> : on N'ignore PLUS le bloc -> on entre dedans pour capter les
				// gradients ; un compteur empeche le rendu des shapes qu'il contient.
				if (std::strcmp(tagBuf, "defs") == 0) {
					if (kind == 1)
						++defsDepth;
					continue;
				}
				// <symbol> NE SE REND PAS LA OU IL EST DEFINI -- seulement instancie
				// par <use>, et <use> lui passe alors son INTERIEUR : cette balise
				// n'apparait donc jamais dans un fragment instancie. Une seule regle,
				// sans exception a verifier.
				if (std::strcmp(tagBuf, "symbol") == 0) {
					if (kind == 1)
						++defsDepth;
					continue;
				}

				// Gradients : <linearGradient> / <radialGradient> (+ <stop> enfants).
				if (std::strcmp(tagBuf, "linearGradient") == 0 || std::strcmp(tagBuf, "radialGradient") == 0) {
					curGrad = Gradient();
					curGrad.kind = (tagBuf[0] == 'l') ? GradKind::Linear : GradKind::Radial;
					const char *id = FindAttr(attrs, numAttrs, "id");
					if (id) {
						std::strncpy(curGrad.id, id, 63);
						curGrad.id[63] = 0;
					}
					const char *href = FindAttr(attrs, numAttrs, "xlink:href");
					if (!href)
						href = FindAttr(attrs, numAttrs, "href");
					if (href) {
						const char *hp = (*href == '#') ? href + 1 : href;
						std::strncpy(curGrad.href, hp, 63);
						curGrad.href[63] = 0;
					}
					const char *gu = FindAttr(attrs, numAttrs, "gradientUnits");
					curGrad.userSpace = (gu && std::strcmp(gu, "userSpaceOnUse") == 0);
					curGrad.hasUnits = (gu != nullptr);
					const char *gt = FindAttr(attrs, numAttrs, "gradientTransform");
					if (gt) {
						curGrad.xform = NkSVGTransform::Parse(gt);
						curGrad.hasXform = true;
					}
					const char *sp = FindAttr(attrs, numAttrs, "spreadMethod");
					curGrad.hasSpread = (sp != nullptr);
					if (sp) {
						if (std::strcmp(sp, "reflect") == 0)
							curGrad.spread = GradSpread::Reflect;
						else if (std::strcmp(sp, "repeat") == 0)
							curGrad.spread = GradSpread::Repeat;
					}
					if (curGrad.kind == GradKind::Linear) {
						const char *a1 = FindAttr(attrs, numAttrs, "x1");
						const char *b1 = FindAttr(attrs, numAttrs, "y1");
						const char *a2 = FindAttr(attrs, numAttrs, "x2");
						const char *b2 = FindAttr(attrs, numAttrs, "y2");
						curGrad.x1 = ParsePct(a1, 0.f);
						curGrad.y1 = ParsePct(b1, 0.f);
						curGrad.x2 = ParsePct(a2, 1.f);
						curGrad.y2 = ParsePct(b2, 0.f);
						curGrad.hasX1 = (a1 != nullptr);
						curGrad.hasY1 = (b1 != nullptr);
						curGrad.hasX2 = (a2 != nullptr);
						curGrad.hasY2 = (b2 != nullptr);
					} else {
						const char *ac = FindAttr(attrs, numAttrs, "cx");
						const char *bc = FindAttr(attrs, numAttrs, "cy");
						const char *rr = FindAttr(attrs, numAttrs, "r");
						curGrad.cx = ParsePct(ac, 0.5f);
						curGrad.cy = ParsePct(bc, 0.5f);
						curGrad.r = ParsePct(rr, 0.5f);
						curGrad.hasCx = (ac != nullptr);
						curGrad.hasCy = (bc != nullptr);
						curGrad.hasR = (rr != nullptr);
						// LE FOYER : par defaut il est AU CENTRE (le degrade est alors
						// concentrique) ; fx/fy le deplacent, et la lumiere se decale.
						const char *sfx = FindAttr(attrs, numAttrs, "fx");
						const char *sfy = FindAttr(attrs, numAttrs, "fy");
						curGrad.fx = sfx ? ParsePct(sfx, curGrad.cx) : curGrad.cx;
						curGrad.fy = sfy ? ParsePct(sfy, curGrad.cy) : curGrad.cy;
						curGrad.hasFocal = (sfx != nullptr || sfy != nullptr);
					}
					buildingGrad = true;
					if (kind == 2) { // self-close (ex. gradient referencant un href, sans stops)
						gradients.PushBack(std::move(curGrad));
						curGrad = Gradient();
						buildingGrad = false;
					}
					continue;
				}
				if (std::strcmp(tagBuf, "stop") == 0) {
					if (buildingGrad)
						curGrad.stops.PushBack(ReadStop(attrs, numAttrs));
					continue;
				}

				// Tags structurels ignores (contenu sans shape direct). <style> est
				// SAUTE, pas applique : les classes CSS ne sont pas resolues -> on le dit.
				if (std::strcmp(tagBuf, "title") == 0 || std::strcmp(tagBuf, "desc") == 0 ||
					std::strcmp(tagBuf, "metadata") == 0 || std::strcmp(tagBuf, "style") == 0) {
					// (le contenu de <style> a ete lu par la PRE-PASSE CollecterCSS :
					//  ici on ne fait que sauter le bloc dans le flux)
					// Skip jusqu'au closing equivalent.
					if (kind == 1) {
						int32 nested = 1;
						while (nested > 0) {
							const int32 k2 = ReadNextTag(reinterpret_cast<const char *&>(xml), xml + xmlLen, tagBuf,
														 sizeof(tagBuf), attrs, numAttrs, nameBuf, nameOff,
														 kNameBufSize, attrPool, poolOff, kAttrPoolSize);
							if (k2 == 0)
								break;
							if (k2 == 1)
								++nested;
							else if (k2 == 3)
								--nested;
						}
					}
					continue;
				}

				// Contenu de <defs> : definitions seulement, pas de rendu de shape.
				// ⚠️ MAIS ON DIT QUAND MEME CE QU'ON NE SAIT PAS. Ce `continue` etait
				// place AVANT la branche « element inconnu » : tout ce qui vit dans un
				// <defs> -- et <mask>, <pattern>, <marker> y vivent presque toujours --
				// etait donc saute EN SILENCE. Un decodeur muet sur la moitie du
				// document ou se rangent les definitions est un decodeur qui ment par
				// omission ; le banc l'a attrape en cherchant « mask » dans le registre.
				if (defsDepth > 0) {
					if (!TagConnu(tagBuf) && skips.Noter(tagBuf))
						logger.Warn("[SVG] element non gere dans <defs>, saute : <{0}>", tagBuf);
					continue;
				}

				// ── <use> : INSTANCIER UN ELEMENT DEJA DECRIT AILLEURS ───────────
				if (std::strcmp(tagBuf, "use") == 0) {
					// TOUT CE QU'ON LIT DU <use> EST COPIE MAINTENANT : la recursion
					// reutilise les memes buffers d'attributs et les ecrasera.
					const char *hrefAttr = FindAttr(attrs, numAttrs, "href");
					if (!hrefAttr)
						hrefAttr = FindAttr(attrs, numAttrs, "xlink:href");
					char href[128];
					href[0] = 0;
					if (hrefAttr) {
						std::strncpy(href, hrefAttr, sizeof(href) - 1);
						href[sizeof(href) - 1] = 0;
					}
					const float32 ux = ParseFloat(FindAttr(attrs, numAttrs, "x"));
					const float32 uy = ParseFloat(FindAttr(attrs, numAttrs, "y"));
					const char *wAttr = FindAttr(attrs, numAttrs, "width");
					const char *hAttr = FindAttr(attrs, numAttrs, "height");
					const float32 uw = wAttr ? ParseFloat(wAttr) : 0.f;
					const float32 uh = hAttr ? ParseFloat(hAttr) : 0.f;

					ParseState inst = cur;
					inst.style = MergeStyle(cur.style, attrs, numAttrs, &useCtx.regles, "use");
					UpdateRefs(inst.fillRef, inst.strokeRef, attrs, numAttrs);
					const char *trU = FindAttr(attrs, numAttrs, "transform");
					if (trU)
						inst.xform = cur.xform * NkSVGTransform::Parse(trU);
					// x / y d'un <use> valent une TRANSLATION, appliquee APRES son
					// propre transform (SVG 1.1 §5.6).
					if (ux != 0.f || uy != 0.f)
						inst.xform = inst.xform * NkSVGTransform::Translate(ux, uy);

					const IdRange *cible = TrouverId(useCtx, href);
					if (!cible) {
						if (skips.Noter("use-cible-absente"))
							logger.Warn("[SVG] <use href=\"{0}\"> : cible introuvable -- rien n'est instancie.",
										href);
						continue;
					}
					// LA GARDE DE RECURSION. Un <use> peut se referencer lui-meme, ou
					// deux <use> se referencer mutuellement. Une pile qui explose ne
					// laisse aucun message ; un compteur, si.
					if (useCtx.profondeur >= kProfondeurUseMax) {
						if (skips.Noter("use-recursion"))
							logger.Warn("[SVG] <use> : profondeur d'instanciation {0} atteinte (reference "
										"circulaire ?) -- on s'arrete la.",
										kProfondeurUseMax);
						continue;
					}

					// <symbol> et <svg> : on instancie leur INTERIEUR (leur propre
					// balise ne se rend pas), et leur viewBox devient une echelle si
					// le <use> donne une taille.
					const bool conteneur =
						(std::strcmp(cible->tag, "symbol") == 0 || std::strcmp(cible->tag, "svg") == 0);
					const char *fragDeb = conteneur ? cible->debInterieur : cible->debComplet;
					const char *fragFin = conteneur ? cible->finInterieur : cible->finComplet;
					if (!fragDeb || !fragFin || fragFin <= fragDeb)
						continue;
					if (conteneur && uw > 0.f && uh > 0.f) {
						// la viewBox du symbole, relue a la main dans son tag ouvrant
						float32 sx = 1.f, sy = 1.f;
						const char *vbp = cible->debComplet;
						while (vbp < cible->debInterieur && std::strncmp(vbp, "viewBox", 7) != 0)
							++vbp;
						if (vbp < cible->debInterieur) {
							vbp += 7;
							while (vbp < cible->debInterieur && (*vbp == '=' || *vbp == '"' || *vbp == '\'' ||
																 IsSpace(*vbp)))
								++vbp;
							const float32 bx = ParseFloat(vbp, &vbp);
							vbp = SkipWSComma(vbp);
							const float32 by = ParseFloat(vbp, &vbp);
							vbp = SkipWSComma(vbp);
							const float32 bw = ParseFloat(vbp, &vbp);
							vbp = SkipWSComma(vbp);
							const float32 bh = ParseFloat(vbp, &vbp);
							if (bw > 0.f && bh > 0.f) {
								sx = uw / bw;
								sy = uh / bh;
								inst.xform = inst.xform * NkSVGTransform::Scale(sx, sy) *
											 NkSVGTransform::Translate(-bx, -by);
							}
						}
					} else if (!conteneur && (wAttr || hAttr) && skips.Noter("use-taille")) {
						logger.Warn("[SVG] <use width/height> n'a d'effet que sur un <symbol> ou un <svg> : "
									"ignore ici (norme SVG 1.1 §5.6).");
					}

					++useCtx.profondeur;
					float32 iw = 0.f, ih = 0.f, jw = 0.f, jh = 0.f, kw = 0.f, kh = 0.f;
					ParseSVGDocument(fragDeb, (usize)(fragFin - fragDeb), shapes, gradients, filtres, clips, motifs,
									 iw, ih, jw, jh, kw, kh, skips, baseDir, glyphes, useCtx, &inst);
					--useCtx.profondeur;
					continue;
				}

				// ── <text> / <tspan> : le contenu VIT ENTRE LES BALISES ──────────
				//    Le lecteur de tags saute le texte ; ici on le prend a la source,
				//    depuis la position juste apres le « > » de la balise ouvrante.
				if (std::strcmp(tagBuf, "text") == 0 || std::strcmp(tagBuf, "tspan") == 0 ||
					std::strcmp(tagBuf, "textPath") == 0) {
					const bool estText = (std::strcmp(tagBuf, "text") == 0);
					if (std::strcmp(tagBuf, "textPath") == 0 && texte.actif) {
						const char *hp = FindAttr(attrs, numAttrs, "href");
						if (!hp)
							hp = FindAttr(attrs, numAttrs, "xlink:href");
						if (hp) {
							std::strncpy(texte.cheminRef, (*hp == '#') ? hp + 1 : hp, 63);
							texte.cheminRef[63] = 0;
						}
					}
					ParseState loc = cur;
					loc.style = MergeStyle(cur.style, attrs, numAttrs, &useCtx.regles, tagBuf);
					UpdateRefs(loc.fillRef, loc.strokeRef, attrs, numAttrs);
					const char *tr2 = FindAttr(attrs, numAttrs, "transform");
					if (tr2)
						loc.xform = cur.xform * NkSVGTransform::Parse(tr2);

					if (estText) {
						FinirTexte(shapes, texte, glyphes, skips); // un <text> non ferme
						texte = TextState();
						texte.actif = true;
						texte.x = ParseFloat(FindAttr(attrs, numAttrs, "x"));
						texte.y = ParseFloat(FindAttr(attrs, numAttrs, "y"));
						texte.xform = loc.xform;
						const char *anc = FindAttr(attrs, numAttrs, "text-anchor");
						if (anc && std::strcmp(anc, "middle") == 0)
							texte.anchor = 1;
						else if (anc && (std::strcmp(anc, "end") == 0))
							texte.anchor = 2;
						const char *esp = FindAttr(attrs, numAttrs, "xml:space");
						texte.preserve = (esp && std::strcmp(esp, "preserve") == 0);
						const char *db = FindAttr(attrs, numAttrs, "dominant-baseline");
						if (db) {
							if (std::strcmp(db, "middle") == 0 || std::strcmp(db, "central") == 0)
								texte.ligne = Ligne::Milieu;
							else if (std::strcmp(db, "hanging") == 0 ||
									 std::strcmp(db, "text-before-edge") == 0)
								texte.ligne = Ligne::Suspendue;
							else if (std::strcmp(db, "text-after-edge") == 0 ||
									 std::strcmp(db, "ideographic") == 0)
								texte.ligne = Ligne::Basse;
							else if (std::strcmp(db, "auto") != 0 && std::strcmp(db, "alphabetic") != 0 &&
									 skips.Noter("dominant-baseline-valeur"))
								logger.Warn("[SVG] dominant-baseline=\"{0}\" inconnu -- `y` reste la ligne de "
											"base.",
											db);
						}
						const char *tl = FindAttr(attrs, numAttrs, "textLength");
						if (tl)
							texte.longueurVoulue = ParseFloat(tl);
						const char *la = FindAttr(attrs, numAttrs, "lengthAdjust");
						texte.ajusterGlyphes = (la && std::strcmp(la, "spacingAndGlyphs") == 0);
					}
					if (!texte.actif) {
						// un <tspan> hors de tout <text> : rien a poser
						continue;
					}

					TextFragment fr;
					fr.style = loc.style;
					std::strncpy(fr.fillRef, loc.fillRef, 63);
					const char *fs = FindAttr(attrs, numAttrs, "font-size");
					if (!fs && estText)
						fs = nullptr;
					fr.fontSize = fs ? ParseFloat(fs)
									 : (texte.frags.IsEmpty() ? 16.f : texte.frags[texte.frags.Size() - 1].fontSize);
					const char *ff = FindAttr(attrs, numAttrs, "font-family");
					if (!ff && !texte.frags.IsEmpty())
						ff = texte.frags[texte.frags.Size() - 1].famille;
					if (ff) {
						std::strncpy(fr.famille, ff, sizeof(fr.famille) - 1);
						fr.famille[sizeof(fr.famille) - 1] = 0;
					}
					const char *fw = FindAttr(attrs, numAttrs, "font-weight");
					if (fw) {
						if (StrCaseCmp(fw, "bold") == 0)
							fr.weight = 700;
						else if (StrCaseCmp(fw, "normal") == 0)
							fr.weight = 400;
						else
							fr.weight = (int32)ParseFloat(fw);
					} else if (!texte.frags.IsEmpty()) {
						fr.weight = texte.frags[texte.frags.Size() - 1].weight;
					}
					if (!estText) { // un <tspan> peut repositionner le curseur
						const char *tx = FindAttr(attrs, numAttrs, "x");
						const char *ty = FindAttr(attrs, numAttrs, "y");
						if (tx) {
							fr.posX = true;
							fr.x = ParseFloat(tx);
						}
						if (ty) {
							fr.posY = true;
							fr.y = ParseFloat(ty);
						}
					}
					if (kind == 1) { // le contenu court jusqu'au prochain '<'
						const char *deb = xml;
						const char *fin = deb;
						while (fin < xmlEnd && *fin != '<')
							++fin;
						if (fin > deb)
							NettoyerTexte(deb, (usize)(fin - deb), texte.preserve, fr.txt, sizeof(fr.txt));
					}
					if (fr.txt[0])
						texte.frags.PushBack(fr);
					continue;
				}

				// Element shape : compose le state local = cur + attrs propres.
				ParseState local = cur;
				local.style = MergeStyle(cur.style, attrs, numAttrs, &useCtx.regles, tagBuf);
				UpdateRefs(local.fillRef, local.strokeRef, attrs, numAttrs);
				const char *tr = FindAttr(attrs, numAttrs, "transform");
				if (tr)
					local.xform = cur.xform * NkSVGTransform::Parse(tr);
				for (int32 q = 0; q < 2; ++q) { // clip-path / mask poses sur la forme
					const char *clp = FindAttr(attrs, numAttrs, q == 0 ? "clip-path" : "mask");
					char refClip[64];
					if (clp && ParseUrlRef(clp, refClip, sizeof(refClip)) &&
						local.nClips < (int32)(sizeof(local.clipRefs) / sizeof(local.clipRefs[0]))) {
						std::strncpy(local.clipRefs[local.nClips], refClip, 63);
						local.clipRefs[local.nClips][63] = 0;
						++local.nClips;
					}
				}

				const uint32 shapeBefore = shapes.Size();
				if (std::strcmp(tagBuf, "rect") == 0)
					ShapeFromRect(shapes, attrs, numAttrs, local);
				else if (std::strcmp(tagBuf, "circle") == 0)
					ShapeFromCircle(shapes, attrs, numAttrs, local);
				else if (std::strcmp(tagBuf, "ellipse") == 0)
					ShapeFromEllipse(shapes, attrs, numAttrs, local);
				else if (std::strcmp(tagBuf, "line") == 0)
					ShapeFromLine(shapes, attrs, numAttrs, local);
				else if (std::strcmp(tagBuf, "polyline") == 0)
					ShapeFromPolygon(shapes, attrs, numAttrs, local, false);
				else if (std::strcmp(tagBuf, "polygon") == 0)
					ShapeFromPolygon(shapes, attrs, numAttrs, local, true);
				else if (std::strcmp(tagBuf, "path") == 0)
					ShapeFromPath(shapes, attrs, numAttrs, local);
				else if (std::strcmp(tagBuf, "image") == 0)
					ShapeFromImage(shapes, attrs, numAttrs, local, baseDir, skips);
				else if (skips.Noter(tagBuf)) {
					// L'ELEMENT QU'ON NE SAIT PAS SE DIT, une fois par nom. Le silence
					// d'un decodeur est ce qui fait croire qu'un fichier est rendu.
					logger.Warn("[SVG] element non gere, saute : <{0}>", tagBuf);
				}

				// Propage le CTM + les refs de gradient aux shapes nouvellement creees.
				for (uint32 si = shapeBefore; si < shapes.Size(); ++si) {
					shapes[si].ctm = local.xform;
					std::strncpy(shapes[si].filterRef, local.filterRef, 63);
					shapes[si].filterRef[63] = 0;
					shapes[si].filterInst = local.filterInst;
					shapes[si].nClips = local.nClips;
					for (int32 c = 0; c < local.nClips; ++c) {
						std::strncpy(shapes[si].clipRefs[c], local.clipRefs[c], 63);
						shapes[si].clipRefs[c][63] = 0;
					}
					std::strncpy(shapes[si].fillRef, local.fillRef, 63);
					shapes[si].fillRef[63] = 0;
					std::strncpy(shapes[si].strokeRef, local.strokeRef, 63);
					shapes[si].strokeRef[63] = 0;
				}
			}

			if (dansFiltre)
				filtres.PushBack(std::move(curFiltre)); // un <filter> jamais ferme
			if (dansClip) {
				for (uint32 k = shapesAvantClip; k < shapes.Size(); ++k)
					curClip.formes.PushBack(std::move(shapes[k]));
				while (shapes.Size() > shapesAvantClip)
					shapes.PopBack();
				clips.PushBack(std::move(curClip));
			}
			if (dansMotif) {
				for (uint32 k = shapesAvantMotif; k < shapes.Size(); ++k)
					curMotif.formes.PushBack(std::move(shapes[k]));
				while (shapes.Size() > shapesAvantMotif)
					shapes.PopBack();
				motifs.PushBack(std::move(curMotif));
			}
			FinirTexte(shapes, texte, glyphes, skips); // un <text> jamais ferme se pose quand meme
			if (proprietaireBufs) {
				NkFree(nameBuf);
				NkFree(attrPool);
				useCtx.nameBuf = nullptr;
				useCtx.attrPool = nullptr;
			}
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// SECTION 9 — Rasterizer scanline AA (supersample 2x)
		// ─────────────────────────────────────────────────────────────────────────────

		struct ScanEdge {
				float32 x;
				int32 dir;
		};

		// ─────────────────────────────────────────────────────────────────────────────
		// LA PEINTURE D'UN DEGRADE — ON VA DU PIXEL VERS LE DEGRADE, jamais l'inverse
		// -----------------------------------------------------------------------------
		// L'ancienne version transportait la GEOMETRIE du degrade (deux points, ou un
		// centre et un rayon) vers l'espace des pixels. Ca marche tant que la matrice
		// ne fait que translater et mettre a l'echelle -- et ca ment des qu'elle
		// tourne ou cisaille : un cercle transporte par trois nombres redevient un
		// cercle, alors qu'il doit devenir une ELLIPSE. C'est aussi pour ca que
		// `gradientTransform` etait silencieusement IGNORE en objectBoundingBox.
		//
		// Ici on garde le degrade dans SON espace et on y ramene chaque pixel par une
		// matrice inverse. Consequences, toutes gratuites : gradientTransform vaut
		// dans les DEUX modes d'unites, une ellipse est une ellipse, et le foyer
		// (fx/fy) se pose sans cas particulier.
		// ─────────────────────────────────────────────────────────────────────────────
		struct GradPaint {
				GradKind kind = GradKind::Linear;
				GradSpread spread = GradSpread::Pad;
				const GradStop *stops = nullptr;
				int32 nStops = 0;
				NkSVGTransform inv;						  ///< pixel -> espace du degrade
				float32 x1 = 0, y1 = 0, x2 = 1, y2 = 0;	  // linear, dans SON espace
				float32 cx = 0, cy = 0, r = 1;			  // radial, dans SON espace
				float32 fx = 0, fy = 0;					  // le foyer
		};

		/// Peinture par MOTIF : une tuile deja rasterisee, plus la matrice qui ramene
		/// un pixel dans l'espace du motif. Le carrelage est un simple modulo.
		struct MotifPaint {
				const NkImage *tuile = nullptr;
				NkSVGTransform inv;		 ///< pixel -> espace du motif
				float32 periodeX = 1.f;	 ///< la periode, en pixels de la tuile
				float32 periodeY = 1.f;
		};

		/// Lerp deux couleurs (composantes + alpha).
		NkSVGColor LerpStopColor(const NkSVGColor &a, const NkSVGColor &b, float32 f) noexcept {
			if (f < 0.f)
				f = 0.f;
			if (f > 1.f)
				f = 1.f;
			NkSVGColor c;
			c.r = (uint8)((float32)a.r + ((float32)b.r - (float32)a.r) * f);
			c.g = (uint8)((float32)a.g + ((float32)b.g - (float32)a.g) * f);
			c.b = (uint8)((float32)a.b + ((float32)b.b - (float32)a.b) * f);
			c.a = (uint8)((float32)a.a + ((float32)b.a - (float32)a.a) * f);
			c.none = false;
			return c;
		}

		/// Evalue la couleur du gradient au pixel (px,py).
		NkSVGColor EvalGrad(const GradPaint &gp, float32 px, float32 py) noexcept {
			if (gp.nStops <= 0)
				return NkSVGColor::Transparent();
			// le pixel, ramene dans l'espace ou le degrade est decrit
			gp.inv.Apply(px, py);
			float32 t;
			if (gp.kind == GradKind::Linear) {
				const float32 dx = gp.x2 - gp.x1, dy = gp.y2 - gp.y1;
				const float32 l2 = dx * dx + dy * dy;
				t = (l2 > 1e-9f) ? ((px - gp.x1) * dx + (py - gp.y1) * dy) / l2 : 0.f;
			} else if (gp.r <= 1e-6f) {
				t = 1.f;
			} else {
				// LE FOYER (SVG 1.1 §13.2.3) : le degrade court du foyer F vers le
				// cercle, le long du rayon qui passe par le pixel. On cherche le
				// scalaire k tel que F + k (P - F) touche le cercle ; la valeur du
				// degrade est alors 1/k. Foyer au centre -> on retombe exactement sur
				// la distance normalisee, sans cas particulier.
				const float32 dx = px - gp.fx, dy = py - gp.fy;
				const float32 fcx = gp.fx - gp.cx, fcy = gp.fy - gp.cy;
				const float32 a = dx * dx + dy * dy;
				if (a <= 1e-12f) {
					t = 0.f;
				} else {
					const float32 b = dx * fcx + dy * fcy;
					const float32 c = fcx * fcx + fcy * fcy - gp.r * gp.r;
					float32 disc = b * b - a * c;
					if (disc < 0.f)
						disc = 0.f;
					const float32 k = (-b + std::sqrt(disc)) / a;
					t = (k > 1e-6f) ? (1.f / k) : 1.f;
				}
			}
			// spreadMethod.
			if (gp.spread == GradSpread::Pad) {
				if (t < 0.f)
					t = 0.f;
				if (t > 1.f)
					t = 1.f;
			} else if (gp.spread == GradSpread::Repeat) {
				t = t - std::floor(t);
			} else { // Reflect
				float32 u = std::fabs(t);
				u = u - 2.f * std::floor(u * 0.5f);
				if (u > 1.f)
					u = 2.f - u;
				t = u;
			}
			// Echantillonne les stops (supposes tries par offset croissant).
			if (t <= gp.stops[0].offset)
				return gp.stops[0].color;
			if (t >= gp.stops[gp.nStops - 1].offset)
				return gp.stops[gp.nStops - 1].color;
			for (int32 i = 0; i < gp.nStops - 1; ++i) {
				const float32 o0 = gp.stops[i].offset;
				const float32 o1 = gp.stops[i + 1].offset;
				if (t <= o1) {
					const float32 seg = o1 - o0;
					const float32 f = (seg > 1e-6f) ? (t - o0) / seg : 0.f;
					return LerpStopColor(gp.stops[i].color, gp.stops[i + 1].color, f);
				}
			}
			return gp.stops[gp.nStops - 1].color;
		}

		/// Inverse d'une affine [a c e ; b d f]. @return false si degeneree (det ~ 0).
		bool Inverser(const NkSVGTransform &m, NkSVGTransform &out) noexcept {
			const float32 det = m.a * m.d - m.b * m.c;
			if (std::fabs(det) < 1e-12f)
				return false;
			const float32 inv = 1.f / det;
			out.a = m.d * inv;
			out.b = -m.b * inv;
			out.c = -m.c * inv;
			out.d = m.a * inv;
			out.e = (m.c * m.f - m.d * m.e) * inv;
			out.f = (m.b * m.e - m.a * m.f) * inv;
			return true;
		}

		/// Echantillon BILINEAIRE (bord repete) d'une image RGBA, en pixels source.
		void EchantillonBilineaire(const NkImage &src, float32 fx, float32 fy, float32 out[4]) noexcept {
			const int32 w = src.Width(), h = src.Height(), ch = src.Channels();
			const uint8 *px = src.Pixels();
			float32 sx = fx - 0.5f, sy = fy - 0.5f;
			int32 x0 = (int32)std::floor(sx), y0 = (int32)std::floor(sy);
			const float32 tx = sx - (float32)x0, ty = sy - (float32)y0;
			int32 x1 = x0 + 1, y1 = y0 + 1;
			auto borne = [](int32 v, int32 hi) { return v < 0 ? 0 : (v >= hi ? hi - 1 : v); };
			x0 = borne(x0, w);
			x1 = borne(x1, w);
			y0 = borne(y0, h);
			y1 = borne(y1, h);
			const int32 st = src.Stride();
			auto lire = [&](int32 x, int32 y, float32 c[4]) {
				const uint8 *p = px + (usize)y * (usize)st + (usize)x * (usize)ch;
				c[0] = (float32)p[0];
				c[1] = (float32)(ch > 1 ? p[1] : p[0]);
				c[2] = (float32)(ch > 2 ? p[2] : p[0]);
				c[3] = (float32)(ch > 3 ? p[3] : 255);
			};
			float32 c00[4], c10[4], c01[4], c11[4];
			lire(x0, y0, c00);
			lire(x1, y0, c10);
			lire(x0, y1, c01);
			lire(x1, y1, c11);
			for (int32 k = 0; k < 4; ++k) {
				const float32 h0 = c00[k] + (c10[k] - c00[k]) * tx;
				const float32 h1 = c01[k] + (c11[k] - c01[k]) * tx;
				out[k] = h0 + (h1 - h0) * ty;
			}
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// LA POSE D'UNE <image> : on balaie la BOITE TRANSFORMEE et, pour chaque pixel,
		// on revient dans l'espace utilisateur par l'INVERSE de la matrice totale. Un
		// aller (image -> ecran) aurait laisse des trous des que la matrice tourne ou
		// agrandit ; le retour n'en laisse aucun, et il donne la rotation gratuitement.
		// Les bords sont anticreneles par 2x2 sous-echantillons -- le seul endroit ou
		// une image posee de biais se juge.
		// ─────────────────────────────────────────────────────────────────────────────
		void RasterizeImage(NkImage &dst, const Shape &sh, const NkImage &src,
							const NkSVGTransform &totale) noexcept {
			if (!src.IsValid() || !sh.style.visible)
				return;
			NkSVGTransform inv;
			if (!Inverser(totale, inv))
				return;
			const float32 alphaGlobal = sh.style.opacity * sh.style.fillOpacity;
			if (alphaGlobal <= 0.f)
				return;

			const int32 W = dst.Width(), H = dst.Height();
			// bbox du quad deja transforme (les points de la shape sont en destination)
			float32 minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
			for (uint32 i = 0; i < sh.xs.Size(); ++i) {
				if (sh.xs[i] < minx) minx = sh.xs[i];
				if (sh.xs[i] > maxx) maxx = sh.xs[i];
				if (sh.ys[i] < miny) miny = sh.ys[i];
				if (sh.ys[i] > maxy) maxy = sh.ys[i];
			}
			int32 x0 = (int32)std::floor(minx), x1 = (int32)std::ceil(maxx);
			int32 y0 = (int32)std::floor(miny), y1 = (int32)std::ceil(maxy);
			if (x0 < 0) x0 = 0;
			if (y0 < 0) y0 = 0;
			if (x1 > W) x1 = W;
			if (y1 > H) y1 = H;
			if (x0 >= x1 || y0 >= y1)
				return;

			const float32 nw = (float32)src.Width(), nh = (float32)src.Height();
			if (nw <= 0.f || nh <= 0.f)
				return;
			// preserveAspectRatio : l'echelle image -> boite, et le centrage.
			float32 ech = 1.f, offx = 0.f, offy = 0.f, echY = 1.f;
			if (sh.fit == FitKind::None) {
				ech = sh.iw / nw;
				echY = sh.ih / nh;
			} else {
				const float32 sx = sh.iw / nw, sy = sh.ih / nh;
				ech = (sh.fit == FitKind::Meet) ? (sx < sy ? sx : sy) : (sx > sy ? sx : sy);
				echY = ech;
				// LES NEUF ALIGNEMENTS : Min cale au bord, Max a l'autre, Mid centre.
				const float32 resteX = sh.iw - nw * ech, resteY = sh.ih - nh * echY;
				offx = (sh.alignX == Align::Min) ? 0.f : ((sh.alignX == Align::Max) ? resteX : resteX * 0.5f);
				offy = (sh.alignY == Align::Min) ? 0.f : ((sh.alignY == Align::Max) ? resteY : resteY * 0.5f);
			}

			uint8 *pixels = dst.Pixels();
			const int32 stride = dst.Stride();
			for (int32 py = y0; py < y1; ++py) {
				for (int32 px = x0; px < x1; ++px) {
					// 2x2 sous-echantillons : couverture ET couleur moyennee
					float32 acc[4] = {0.f, 0.f, 0.f, 0.f};
					int32 dedans = 0;
					for (int32 sy2 = 0; sy2 < 2; ++sy2) {
						for (int32 sx2 = 0; sx2 < 2; ++sx2) {
							float32 ux = (float32)px + 0.25f + 0.5f * (float32)sx2;
							float32 uy = (float32)py + 0.25f + 0.5f * (float32)sy2;
							inv.Apply(ux, uy); // -> espace utilisateur
							const float32 bx = ux - sh.ix, by = uy - sh.iy;
							if (bx < 0.f || by < 0.f || bx > sh.iw || by > sh.ih)
								continue; // hors de la boite <image>
							const float32 fx = (bx - offx) / ech;
							const float32 fy = (by - offy) / echY;
							if (fx < 0.f || fy < 0.f || fx > nw || fy > nh)
								continue; // « meet » : la bande vide autour de l'image
							float32 c[4];
							EchantillonBilineaire(src, fx, fy, c);
							for (int32 k = 0; k < 4; ++k)
								acc[k] += c[k];
							++dedans;
						}
					}
					if (dedans == 0)
						continue;
					const float32 inv4 = 1.f / (float32)dedans;
					const float32 couverture = (float32)dedans * 0.25f;
					const float32 sa = (acc[3] * inv4 / 255.f) * alphaGlobal * couverture;
					if (sa <= 0.f)
						continue;
					const int32 isa = (int32)(sa * 255.f + 0.5f);
					if (isa <= 0)
						continue;
					const int32 invA = 255 - isa;
					uint8 *p = pixels + (usize)py * (usize)stride + (usize)px * 4u;
					for (int32 k = 0; k < 3; ++k) {
						const int32 sc = (int32)(acc[k] * inv4 + 0.5f);
						p[k] = (uint8)((sc * isa + (int32)p[k] * invA + 127) / 255);
					}
					const int32 a2 = (int32)p[3] + isa - ((int32)p[3] * isa + 127) / 255;
					p[3] = (uint8)(a2 > 255 ? 255 : (a2 < 0 ? 0 : a2));
				}
			}
		}

		/// @param clip  masque alpha (une valeur par pixel de l'image) ou nullptr.
		///             La couverture de chaque pixel est MULTIPLIEE par lui : c'est
		///             tout ce qu'est un decoupage, et c'est pour ca qu'il n'a pas
		///             besoin de calque.
		void RasterizeShape(NkImage &img, const Shape &sh, const GradPaint *paint = nullptr,
							const uint8 *clip = nullptr, const MotifPaint *motif = nullptr) noexcept {
			if (sh.contourStart.IsEmpty())
				return;
			const NkSVGColor &fill = sh.style.fill;
			if (fill.none)
				return;
			if (!sh.style.visible)
				return;

			// Alpha effectif = fill.a * opacity * fill-opacity (range 0..1).
			// En mode gradient, l'alpha varie par pixel -> on ne pre-calcule que le facteur global.
			const float32 styleA = sh.style.opacity * sh.style.fillOpacity;
			const float32 alphaF = (float32)fill.a / 255.f * styleA;
			if (!paint && !motif && alphaF <= 0.f)
				return;

			const int32 W = img.Width();
			const int32 H = img.Height();
			if (W <= 0 || H <= 0)
				return;

			// BBox du shape pour limiter les scanlines.
			float32 minY = 1e30f, maxY = -1e30f;
			for (uint32 i = 0; i < sh.ys.Size(); ++i) {
				if (sh.ys[i] < minY)
					minY = sh.ys[i];
				if (sh.ys[i] > maxY)
					maxY = sh.ys[i];
			}
			int32 y0 = (int32)std::floor(minY);
			int32 y1 = (int32)std::ceil(maxY);
			if (y0 < 0)
				y0 = 0;
			if (y1 > H)
				y1 = H;
			if (y0 >= y1)
				return;

			// Supersample factor vertical pour AA edges.
			constexpr int32 kSS = 2;

			// Buffer de couverture par pixel (0..255) pour la ligne courante.
			uint8 *covRow = (uint8 *)NkAlloc((usize)W);
			if (!covRow)
				return;

			uint8 *pixels = img.Pixels();
			const int32 stride = img.Stride();
			const bool evenOdd = sh.style.fillEvenOdd;

			constexpr int32 kMaxEdges = 4096;
			ScanEdge edges[kMaxEdges];

			for (int32 py = y0; py < y1; ++py) {
				std::memset(covRow, 0, (usize)W);

				// Pour chaque sub-scanline (2x AA).
				for (int32 ss = 0; ss < kSS; ++ss) {
					const float32 ySub = (float32)py + ((float32)ss + 0.5f) / (float32)kSS;
					int32 nEdges = 0;
					// Collecte les intersections de tous les segments.
					for (uint32 ci = 0; ci < sh.contourStart.Size(); ++ci) {
						const int32 start = sh.contourStart[ci];
						const int32 len = sh.contourLen[ci];
						if (len < 2)
							continue;
						for (int32 k = 0; k < len - 1; ++k) {
							const float32 xA = sh.xs[start + k];
							const float32 yA = sh.ys[start + k];
							const float32 xB = sh.xs[start + k + 1];
							const float32 yB = sh.ys[start + k + 1];
							// Segment cross la sub-scanline ?
							if ((yA <= ySub && yB > ySub) || (yB <= ySub && yA > ySub)) {
								const float32 t = (ySub - yA) / (yB - yA);
								const float32 x = xA + (xB - xA) * t;
								if (nEdges < kMaxEdges) {
									edges[nEdges].x = x;
									edges[nEdges].dir = (yA <= ySub) ? +1 : -1;
									++nEdges;
								}
							}
						}
					}
					if (nEdges < 2)
						continue;
					// Tri par x croissant (insertion sort, nEdges typiquement petit).
					for (int32 i = 1; i < nEdges; ++i) {
						ScanEdge tmp = edges[i];
						int32 j = i - 1;
						while (j >= 0 && edges[j].x > tmp.x) {
							edges[j + 1] = edges[j];
							--j;
						}
						edges[j + 1] = tmp;
					}

					// Remplit selon fill-rule.
					if (evenOdd) {
						for (int32 i = 0; i + 1 < nEdges; i += 2) {
							float32 xL = edges[i].x;
							float32 xR = edges[i + 1].x;
							if (xL > xR)
								std::swap(xL, xR);
							int32 ixL = (int32)std::floor(xL);
							int32 ixR = (int32)std::ceil(xR);
							if (ixL < 0)
								ixL = 0;
							if (ixR > W)
								ixR = W;
							for (int32 x = ixL; x < ixR; ++x) {
								const float32 pL = (float32)x;
								const float32 pR = (float32)(x + 1);
								float32 cov = std::fmin(pR, xR) - std::fmax(pL, xL);
								if (cov <= 0.f)
									continue;
								if (cov > 1.f)
									cov = 1.f;
								const int32 add = (int32)(cov * (255.f / (float32)kSS));
								const int32 s = (int32)covRow[x] + add;
								covRow[x] = (uint8)(s > 255 ? 255 : s);
							}
						}
					} else {
						// Non-zero : winding counter.
						int32 winding = 0;
						float32 spanStart = 0.f;
						bool open = false;
						for (int32 i = 0; i < nEdges; ++i) {
							const int32 prev = winding;
							winding += edges[i].dir;
							if (prev == 0 && winding != 0) {
								spanStart = edges[i].x;
								open = true;
							} else if (prev != 0 && winding == 0 && open) {
								float32 xL = spanStart;
								float32 xR = edges[i].x;
								if (xL > xR)
									std::swap(xL, xR);
								int32 ixL = (int32)std::floor(xL);
								int32 ixR = (int32)std::ceil(xR);
								if (ixL < 0)
									ixL = 0;
								if (ixR > W)
									ixR = W;
								for (int32 x = ixL; x < ixR; ++x) {
									const float32 pL = (float32)x;
									const float32 pR = (float32)(x + 1);
									float32 cov = std::fmin(pR, xR) - std::fmax(pL, xL);
									if (cov <= 0.f)
										continue;
									if (cov > 1.f)
										cov = 1.f;
									const int32 add = (int32)(cov * (255.f / (float32)kSS));
									const int32 s = (int32)covRow[x] + add;
									covRow[x] = (uint8)(s > 255 ? 255 : s);
								}
								open = false;
							}
						}
					}
				}

				// Blend la row dans l'image (premultiplied alpha src-over).
				uint8 *dst = pixels + (usize)py * (usize)stride;
				const uint8 *clipRow = clip ? (clip + (usize)py * (usize)W) : nullptr;
				for (int32 x = 0; x < W; ++x) {
					uint8 c = covRow[x];
					if (c == 0)
						continue;
					if (clipRow) {
						const int32 m = (int32)clipRow[x];
						if (m == 0)
							continue;
						c = (uint8)(((int32)c * m + 127) / 255);
						if (c == 0)
							continue;
					}
					// Couleur source : solide ou echantillonnee dans le gradient.
					int32 srcR, srcG, srcB;
					float32 baseA;
					if (motif && motif->tuile && motif->tuile->IsValid()) {
						// LE PIXEL REVIENT DANS L'ESPACE DU MOTIF, puis modulo : c'est
						// tout le carrelage. Une tuile, quelle que soit la surface.
						float32 mx = (float32)x + 0.5f, my = (float32)py + 0.5f;
						motif->inv.Apply(mx, my);
						float32 u = mx - motif->periodeX * std::floor(mx / motif->periodeX);
						float32 v = my - motif->periodeY * std::floor(my / motif->periodeY);
						const float32 fx = u * (float32)motif->tuile->Width() / motif->periodeX;
						const float32 fy = v * (float32)motif->tuile->Height() / motif->periodeY;
						float32 c4[4];
						EchantillonBilineaire(*motif->tuile, fx, fy, c4);
						srcR = (int32)c4[0];
						srcG = (int32)c4[1];
						srcB = (int32)c4[2];
						baseA = (c4[3] / 255.f) * styleA;
					} else if (paint) {
						const NkSVGColor gc = EvalGrad(*paint, (float32)x + 0.5f, (float32)py + 0.5f);
						srcR = gc.r;
						srcG = gc.g;
						srcB = gc.b;
						baseA = (float32)gc.a / 255.f * styleA;
					} else {
						srcR = fill.r;
						srcG = fill.g;
						srcB = fill.b;
						baseA = alphaF;
					}
					const float32 alpha = baseA * (float32)c / 255.f;
					const int32 sa = (int32)(alpha * 255.f);
					if (sa <= 0)
						continue;
					const int32 invA = 255 - sa;
					uint8 *px = dst + (usize)x * 4;
					// ── mix-blend-mode : la couleur SOURCE est melangee au FOND avant
					//    d'etre composee. Formule de CSS Compositing :
					//    Cs' = (1 - da) Cs + da B(Cb, Cs). A fond transparent (da = 0)
					//    on retombe exactement sur la source : un mode de fusion ne
					//    change rien la ou il n'y a rien, et c'est ce qu'on veut.
					if (sh.style.blend != NkSVGBlend::Normal) {
						const float32 da = (float32)px[3] / 255.f;
						int32 *sc[3] = {&srcR, &srcG, &srcB};
						for (int32 k = 0; k < 3; ++k) {
							const float32 cs = (float32)(*sc[k]) / 255.f;
							const float32 cb = (float32)px[k] / 255.f;
							const float32 b = (sh.style.blend == NkSVGBlend::Multiply) ? (cs * cb)
																					  : (cs + cb - cs * cb);
							float32 v = (1.f - da) * cs + da * b;
							if (v < 0.f)
								v = 0.f;
							if (v > 1.f)
								v = 1.f;
							*sc[k] = (int32)(v * 255.f + 0.5f);
						}
					}
					px[0] = (uint8)((srcR * sa + (int32)px[0] * invA + 127) / 255);
					px[1] = (uint8)((srcG * sa + (int32)px[1] * invA + 127) / 255);
					px[2] = (uint8)((srcB * sa + (int32)px[2] * invA + 127) / 255);
					const int32 a2 = (int32)px[3] + sa - ((int32)px[3] * sa + 127) / 255;
					px[3] = (uint8)(a2 > 255 ? 255 : (a2 < 0 ? 0 : a2));
				}
			}

			NkFree(covRow);
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// Pousse un polygone dans la Shape destination en FORCANT l'orientation CCW puis
		// en fermant le contour. Indispensable : le stroke est rasterise en union de
		// multiples sous-polygones (quads de segment + disques/triangles de jointure) ;
		// si certains etaient CW et d'autres CCW, la regle nonzero les annulerait
		// (winding +1/-1 = 0 -> trous). Tout CCW -> winding >= 1 partout -> union pleine.
		// ─────────────────────────────────────────────────────────────────────────────
		void PushPolyCCW(Shape &dst, const float32 *px, const float32 *py, int32 n) noexcept {
			if (n < 3)
				return;
			float32 area = 0.f;
			for (int32 i = 0; i < n; ++i) {
				const int32 j = (i + 1) % n;
				area += px[i] * py[j] - px[j] * py[i];
			}
			const bool ccw = area > 0.f;
			const int32 cStart = (int32)dst.xs.Size();
			if (ccw) {
				for (int32 i = 0; i < n; ++i) {
					dst.xs.PushBack(px[i]);
					dst.ys.PushBack(py[i]);
				}
			} else {
				for (int32 i = n - 1; i >= 0; --i) {
					dst.xs.PushBack(px[i]);
					dst.ys.PushBack(py[i]);
				}
			}
			// Ferme (re-pousse le premier point pousse).
			dst.xs.PushBack(dst.xs[(uint32)cStart]);
			dst.ys.PushBack(dst.ys[(uint32)cStart]);
			dst.contourStart.PushBack(cStart);
			dst.contourLen.PushBack((int32)dst.xs.Size() - cStart);
		}

		/// Pousse un disque approxime (20 cotes) — pour cap/join "round".
		void PushDisc(Shape &dst, float32 cx, float32 cy, float32 r) noexcept {
			if (r <= 0.f)
				return;
			constexpr int32 N = 20;
			float32 px[N], py[N];
			for (int32 i = 0; i < N; ++i) {
				const float32 a = (float32)i * (6.283185307179586f / (float32)N);
				px[i] = cx + r * std::cos(a);
				py[i] = cy + r * std::sin(a);
			}
			PushPolyCCW(dst, px, py, N);
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// Construit une "shape ruban" autour du chemin source pour rasteriser le stroke.
		// Honore stroke-linecap (butt/round/square) et stroke-linejoin (miter/round/bevel)
		// + stroke-miterlimit. Approche par tampons : un quad par segment, une jointure
		// par vertex interieur, un cap par extremite ouverte. Tout passe par PushPolyCCW
		// (union nonzero correcte). Le resultat est rempli par RasterizeShape avec la
		// couleur stroke en "fill".
		// ─────────────────────────────────────────────────────────────────────────────
		// ─────────────────────────────────────────────────────────────────────────────
		// stroke-dasharray — DECOUPER LE CHEMIN AVANT DE L'EPAISSIR
		// -----------------------------------------------------------------------------
		// Un trait en pointilles n'est pas un trait avec des trous : c'est un chemin
		// COUPE en morceaux, dont chacun est ensuite epaissi normalement. Le faire
		// dans cet ordre donne gratuitement le bon comportement aux extremites --
		// chaque tiret recoit ses propres terminaisons (`stroke-linecap`), ce qu'un
		// effacement a posteriori n'aurait pas su produire.
		//
		// Un motif de longueur IMPAIRE est repete deux fois (norme SVG 1.1) : « 5 »
		// veut dire « 5 pleins, 5 vides », pas « 5 pleins » puis rien.
		// ─────────────────────────────────────────────────────────────────────────────
		void DecouperEnTirets(const Shape &src, const NkSVGStyle &st, float32 echelle, Shape &dst) noexcept {
			// le motif, en pixels de destination, double s'il est impair
			float32 motif[NkSVGStyle::kMaxDashes * 2];
			int32 nm = 0;
			for (int32 r = 0; r < ((st.nDashes % 2 == 1) ? 2 : 1); ++r)
				for (int32 i = 0; i < st.nDashes && nm < (int32)(sizeof(motif) / sizeof(motif[0])); ++i)
					motif[nm++] = st.dashes[i] * echelle;
			float32 periode = 0.f;
			for (int32 i = 0; i < nm; ++i)
				periode += motif[i];
			if (nm <= 0 || periode <= 1e-6f)
				return;

			for (uint32 ci = 0; ci < src.contourStart.Size(); ++ci) {
				const int32 start = src.contourStart[ci];
				const int32 len = src.contourLen[ci];
				if (len < 2)
					continue;
				// ou en est-on dans le motif ? `stroke-dashoffset` decale le depart.
				float32 reste = st.dashOffset * echelle;
				reste = reste - periode * std::floor(reste / periode);
				int32 idx = 0;
				bool plein = true;
				while (reste >= motif[idx]) {
					reste -= motif[idx];
					idx = (idx + 1) % nm;
					plein = !plein;
				}
				float32 restant = motif[idx] - reste;

				bool ouvert = false;
				int32 cStart = 0;
				auto ouvrir = [&](float32 x, float32 y) {
					cStart = (int32)dst.xs.Size();
					dst.xs.PushBack(x);
					dst.ys.PushBack(y);
					ouvert = true;
				};
				auto pousser = [&](float32 x, float32 y) {
					dst.xs.PushBack(x);
					dst.ys.PushBack(y);
				};
				auto fermer = [&]() {
					if (!ouvert)
						return;
					const int32 n = (int32)dst.xs.Size() - cStart;
					if (n >= 2) {
						dst.contourStart.PushBack(cStart);
						dst.contourLen.PushBack(n);
					} else {
						while ((int32)dst.xs.Size() > cStart) {
							dst.xs.PopBack();
							dst.ys.PopBack();
						}
					}
					ouvert = false;
				};

				if (plein)
					ouvrir(src.xs[(uint32)start], src.ys[(uint32)start]);
				for (int32 k = 0; k < len - 1; ++k) {
					float32 ax = src.xs[(uint32)(start + k)], ay = src.ys[(uint32)(start + k)];
					const float32 bx = src.xs[(uint32)(start + k + 1)], by = src.ys[(uint32)(start + k + 1)];
					float32 dx = bx - ax, dy = by - ay;
					float32 L = std::sqrt(dx * dx + dy * dy);
					if (L < 1e-9f)
						continue;
					dx /= L;
					dy /= L;
					while (L > restant) {
						ax += dx * restant;
						ay += dy * restant;
						L -= restant;
						if (plein) {
							pousser(ax, ay);
							fermer();
						} else {
							ouvrir(ax, ay);
						}
						plein = !plein;
						idx = (idx + 1) % nm;
						restant = motif[idx];
						if (restant <= 1e-6f) { // une longueur nulle : on ne boucle pas dessus
							plein = !plein;
							idx = (idx + 1) % nm;
							restant = motif[idx];
							if (restant <= 1e-6f)
								return;
						}
					}
					restant -= L;
					if (plein)
						pousser(bx, by);
				}
				fermer();
			}
		}

		void BuildStrokeShape(const Shape &src, float32 hwPx, NkSVGLineCap cap, NkSVGLineJoin join, float32 miterLimit,
							  Shape &dst) noexcept {
			if (hwPx <= 0.f)
				return;
			if (miterLimit < 1.f)
				miterLimit = 1.f;

			for (uint32 ci = 0; ci < src.contourStart.Size(); ++ci) {
				const int32 start = src.contourStart[ci];
				const int32 len = src.contourLen[ci];
				if (len < 2)
					continue;

				const bool closed = (std::fabs(src.xs[start] - src.xs[start + len - 1]) < 1e-4f &&
									 std::fabs(src.ys[start] - src.ys[start + len - 1]) < 1e-4f);
				const int32 nPts = closed ? (len - 1) : len;
				if (nPts < 2)
					continue;

				auto PX = [&](int32 i) { return src.xs[start + i]; };
				auto PY = [&](int32 i) { return src.ys[start + i]; };

				const int32 segCount = closed ? nPts : (nPts - 1);

				// ── Un quad par segment ───────────────────────────────────────────────
				for (int32 s = 0; s < segCount; ++s) {
					const int32 i0 = s;
					const int32 i1 = (s + 1) % nPts;
					float32 ax = PX(i0), ay = PY(i0);
					float32 bx = PX(i1), by = PY(i1);
					float32 dx = bx - ax, dy = by - ay;
					const float32 l = std::sqrt(dx * dx + dy * dy);
					if (l < 1e-6f)
						continue;
					dx /= l;
					dy /= l;
					const float32 nx = -dy * hwPx, ny = dx * hwPx; // perpendiculaire * demi-largeur

					// Cap "square" : etend le segment de hwPx aux extremites ouvertes.
					if (!closed && cap == NkSVGLineCap::Square) {
						if (s == 0) {
							ax -= dx * hwPx;
							ay -= dy * hwPx;
						}
						if (s == segCount - 1) {
							bx += dx * hwPx;
							by += dy * hwPx;
						}
					}
					const float32 qx[4] = {ax + nx, bx + nx, bx - nx, ax - nx};
					const float32 qy[4] = {ay + ny, by + ny, by - ny, ay - ny};
					PushPolyCCW(dst, qx, qy, 4);
				}

				// ── Jointures aux vertices interieurs (et tous pour un contour ferme) ──
				const int32 jStart = closed ? 0 : 1;
				const int32 jEnd = closed ? nPts : (nPts - 1);
				for (int32 v = jStart; v < jEnd; ++v) {
					const int32 prev = (v - 1 + nPts) % nPts;
					const int32 next = (v + 1) % nPts;
					float32 idx = PX(v) - PX(prev), idy = PY(v) - PY(prev);
					float32 odx = PX(next) - PX(v), ody = PY(next) - PY(v);
					const float32 li = std::sqrt(idx * idx + idy * idy);
					const float32 lo = std::sqrt(odx * odx + ody * ody);
					if (li < 1e-6f || lo < 1e-6f)
						continue;
					idx /= li;
					idy /= li;
					odx /= lo;
					ody /= lo;
					const float32 vx = PX(v), vy = PY(v);

					if (join == NkSVGLineJoin::Round) {
						PushDisc(dst, vx, vy, hwPx);
						continue;
					}
					// Perpendiculaires (unitaires) des deux segments.
					const float32 niX = -idy, niY = idx; // perp entrant
					const float32 noX = -ody, noY = odx; // perp sortant

					bool doMiter = (join == NkSVGLineJoin::Miter);
					float32 apX = 0.f, apY = 0.f, amX = 0.f, amY = 0.f;
					if (doMiter) {
						// Bisectrice des perpendiculaires -> direction du miter.
						float32 bx = niX + noX, by = niY + noY;
						const float32 bl = std::sqrt(bx * bx + by * by);
						if (bl < 1e-4f) {
							doMiter = false; // U-turn : pas de miter, bevel.
						} else {
							bx /= bl;
							by /= bl;
							const float32 cosHalf = bx * niX + by * niY; // dot(bisect, perpIn)
							if (cosHalf < 1e-4f) {
								doMiter = false;
							} else {
								const float32 ratio = 1.f / cosHalf; // miterLength / hw
								if (ratio > miterLimit) {
									doMiter = false; // depasse la limite -> bevel
								} else {
									const float32 mlen = hwPx * ratio;
									apX = vx + bx * mlen;
									apY = vy + by * mlen;
									amX = vx - bx * mlen;
									amY = vy - by * mlen;
								}
							}
						}
					}

					if (doMiter) {
						// Triangles de miter des deux cotes (l'interieur est couvert par les quads).
						const float32 tpX[3] = {vx + niX * hwPx, apX, vx + noX * hwPx};
						const float32 tpY[3] = {vy + niY * hwPx, apY, vy + noY * hwPx};
						PushPolyCCW(dst, tpX, tpY, 3);
						const float32 tmX[3] = {vx - niX * hwPx, amX, vx - noX * hwPx};
						const float32 tmY[3] = {vy - niY * hwPx, amY, vy - noY * hwPx};
						PushPolyCCW(dst, tmX, tmY, 3);
					} else {
						// Bevel : comble le coin par un triangle de chaque cote vers le vertex.
						const float32 tpX[3] = {vx + niX * hwPx, vx + noX * hwPx, vx};
						const float32 tpY[3] = {vy + niY * hwPx, vy + noY * hwPx, vy};
						PushPolyCCW(dst, tpX, tpY, 3);
						const float32 tmX[3] = {vx - niX * hwPx, vx - noX * hwPx, vx};
						const float32 tmY[3] = {vy - niY * hwPx, vy - noY * hwPx, vy};
						PushPolyCCW(dst, tmX, tmY, 3);
					}
				}

				// ── Caps "round" aux extremites ouvertes ──────────────────────────────
				if (!closed && cap == NkSVGLineCap::Round) {
					PushDisc(dst, PX(0), PY(0), hwPx);
					PushDisc(dst, PX(nPts - 1), PY(nPts - 1), hwPx);
				}
			}
		}

		// ─────────────────────────────────────────────────────────────────────────────
		// <feDropShadow> — L'OMBRE EST L'ALPHA DU GROUPE, FLOUTE, DECALE, TEINTE
		// -----------------------------------------------------------------------------
		// Un filtre ne s'applique pas a une forme mais a un GROUPE : deux rectangles
		// qui se chevauchent sous le meme <g filter> portent UNE ombre commune, pas
		// deux. Il faut donc un CALQUE -- rendre le groupe a part, puis composer. La
		// forme de l'ombre ne depend que de l'ALPHA du calque (feDropShadow est
		// monochrome) : on ne floute qu'un canal, pas quatre.
		//
		// Le flou gaussien est SEPARABLE : deux passes a une dimension au lieu d'une
		// convolution carree. Pour un rayon de 3 ecarts-types, ca fait 6σ additions
		// par pixel au lieu de 36σ² -- la difference entre « instantane » et « on
		// attend ».
		// ─────────────────────────────────────────────────────────────────────────────

		/// Flou gaussien separable d'un canal 8 bits, en place (via un tampon).
		void FlouGaussienXY(uint8 *canal, int32 W, int32 H, float32 sigmaX, float32 sigmaY) noexcept {
			if ((sigmaX < 0.05f && sigmaY < 0.05f) || W <= 0 || H <= 0)
				return;
			// DEUX NOYAUX, un par direction : `stdDeviation` accepte « x y » et un
			// flou anisotrope est un usage courant (une ombre ecrasee). Reutiliser le
			// meme sigma dans les deux sens rendrait un flou rond la ou le fichier en
			// demande un ovale -- une erreur qui ne se voit que sur les cas obliques.
			constexpr int32 kRayonMax = 128;
			float32 poidsX[kRayonMax * 2 + 1], poidsY[kRayonMax * 2 + 1];
			int32 rx = 0, ry = 0;
			auto noyau = [](float32 sigma, float32 *poids, int32 &rayon) {
				if (sigma < 0.05f) {
					rayon = 0;
					poids[0] = 1.f;
					return;
				}
				rayon = (int32)std::ceil(sigma * 3.f);
				if (rayon < 1)
					rayon = 1;
				if (rayon > kRayonMax)
					rayon = kRayonMax; // au-dela, le noyau coute plus que le resultat ne montre
				const int32 n = rayon * 2 + 1;
				const float32 deux = 2.f * sigma * sigma;
				float32 somme = 0.f;
				for (int32 i = 0; i < n; ++i) {
					const float32 d = (float32)(i - rayon);
					poids[i] = std::exp(-(d * d) / deux);
					somme += poids[i];
				}
				for (int32 i = 0; i < n; ++i)
					poids[i] /= somme;
			};
			noyau(sigmaX, poidsX, rx);
			noyau(sigmaY, poidsY, ry);

			uint8 *tmp = (uint8 *)NkAlloc((usize)W * (usize)H);
			if (!tmp)
				return;
			// passe horizontale
			for (int32 y = 0; y < H; ++y) {
				const uint8 *ligne = canal + (usize)y * (usize)W;
				uint8 *sortie = tmp + (usize)y * (usize)W;
				for (int32 x = 0; x < W; ++x) {
					if (rx == 0) {
						sortie[x] = ligne[x];
						continue;
					}
					float32 acc = 0.f;
					for (int32 i = 0; i < rx * 2 + 1; ++i) {
						int32 sx = x + i - rx;
						if (sx < 0)
							sx = 0;
						if (sx >= W)
							sx = W - 1;
						acc += (float32)ligne[sx] * poidsX[i];
					}
					sortie[x] = (uint8)(acc + 0.5f);
				}
			}
			// passe verticale
			for (int32 y = 0; y < H; ++y) {
				for (int32 x = 0; x < W; ++x) {
					if (ry == 0) {
						canal[(usize)y * (usize)W + (usize)x] = tmp[(usize)y * (usize)W + (usize)x];
						continue;
					}
					float32 acc = 0.f;
					for (int32 i = 0; i < ry * 2 + 1; ++i) {
						int32 sy = y + i - ry;
						if (sy < 0)
							sy = 0;
						if (sy >= H)
							sy = H - 1;
						acc += (float32)tmp[(usize)sy * (usize)W + (usize)x] * poidsY[i];
					}
					canal[(usize)y * (usize)W + (usize)x] = (uint8)(acc + 0.5f);
				}
			}
			NkFree(tmp);
		}

		/// Compose @p calque (RGBA, alpha droit) sur @p dst, en « dessus » ordinaire.
		void ComposerCalque(NkImage &dst, const NkImage &calque) noexcept {
			const int32 W = dst.Width(), H = dst.Height();
			uint8 *d = dst.Pixels();
			const uint8 *s2 = calque.Pixels();
			if (!d || !s2)
				return;
			for (int32 y = 0; y < H; ++y) {
				for (int32 x = 0; x < W; ++x) {
					const usize o = ((usize)y * (usize)W + (usize)x) * 4u;
					const int32 sa = (int32)s2[o + 3];
					if (sa <= 0)
						continue;
					const int32 invA = 255 - sa;
					for (int32 k = 0; k < 3; ++k)
						d[o + k] = (uint8)(((int32)s2[o + k] * sa + (int32)d[o + k] * invA + 127) / 255);
					const int32 a2 = (int32)d[o + 3] + sa - ((int32)d[o + 3] * sa + 127) / 255;
					d[o + 3] = (uint8)(a2 > 255 ? 255 : a2);
				}
			}
		}

		/// Flou gaussien d'une image RGBA, en PREMULTIPLIE. Flouter des canaux non
		/// premultiplies fait baver la couleur des pixels transparents dans les
		/// voisins (une frange noire autour de tout objet floute) : le premultiplie
		/// est la seule facon d'obtenir un bord propre.
		void FlouRGBA(NkImage &img, float32 sigmaX, float32 sigmaY) noexcept {
			if (sigmaX < 0.05f && sigmaY < 0.05f)
				return;
			const int32 W = img.Width(), H = img.Height();
			uint8 *px = img.Pixels();
			if (!px || W <= 0 || H <= 0)
				return;
			const usize n = (usize)W * (usize)H;
			uint8 *canal = (uint8 *)NkAlloc(n);
			if (!canal)
				return;
			// premultiplier
			for (usize i = 0; i < n; ++i) {
				const int32 a = px[i * 4u + 3u];
				for (int32 k = 0; k < 3; ++k)
					px[i * 4u + (usize)k] = (uint8)(((int32)px[i * 4u + (usize)k] * a + 127) / 255);
			}
			for (int32 k = 0; k < 4; ++k) {
				for (usize i = 0; i < n; ++i)
					canal[i] = px[i * 4u + (usize)k];
				FlouGaussienXY(canal, W, H, sigmaX, sigmaY);
				for (usize i = 0; i < n; ++i)
					px[i * 4u + (usize)k] = canal[i];
			}
			// demultiplier
			for (usize i = 0; i < n; ++i) {
				const int32 a = px[i * 4u + 3u];
				if (a == 0) {
					px[i * 4u] = px[i * 4u + 1u] = px[i * 4u + 2u] = 0;
					continue;
				}
				for (int32 k = 0; k < 3; ++k) {
					int32 v = ((int32)px[i * 4u + (usize)k] * 255 + a / 2) / a;
					px[i * 4u + (usize)k] = (uint8)(v > 255 ? 255 : v);
				}
			}
			NkFree(canal);
		}

		// ═════════════════════════════════════════════════════════════════════════════
		// L'EVALUATION DU GRAPHE — chaque primitive lit des images nommees, en produit une
		// ═════════════════════════════════════════════════════════════════════════════
		struct ResultatFiltre {
				char nom[32] = {0};
				NkImage img;

				ResultatFiltre() = default;
				ResultatFiltre(ResultatFiltre &&) noexcept = default;
				ResultatFiltre &operator=(ResultatFiltre &&) noexcept = default;
				ResultatFiltre(const ResultatFiltre &) = delete;
				ResultatFiltre &operator=(const ResultatFiltre &) = delete;
		};

		/// Copie profonde d'une image RGBA (les primitives ne modifient jamais leur
		/// entree : une meme image nommee peut etre lue par plusieurs primitives).
		NkImage CopierImage(const NkImage &src) noexcept {
			NkImage d = NkImage::Alloc(src.Width(), src.Height(), NkImagePixelFormat::NK_RGBA32);
			if (d.IsValid() && src.Pixels() && d.Pixels())
				std::memcpy(d.Pixels(), src.Pixels(), (usize)src.Width() * (usize)src.Height() * 4u);
			return d;
		}

		/// `SourceAlpha` : la source, dont il ne reste que l'alpha (noir opaque la ou
		/// il y avait quelque chose). C'est l'entree de toute ombre portee.
		NkImage AlphaSeul(const NkImage &src) noexcept {
			NkImage d = CopierImage(src);
			if (!d.IsValid())
				return d;
			uint8 *p = d.Pixels();
			const usize n = (usize)d.Width() * (usize)d.Height();
			for (usize i = 0; i < n; ++i) {
				p[i * 4u] = 0;
				p[i * 4u + 1u] = 0;
				p[i * 4u + 2u] = 0;
			}
			return d;
		}

		/// Compose `a` SUR `b` selon un operateur de Porter-Duff, dans `sortie`.
		void Composer(const NkImage &a, const NkImage &b, OpComposite op, float32 k1, float32 k2, float32 k3,
					  float32 k4, NkImage &sortie) noexcept {
			const int32 W = sortie.Width(), H = sortie.Height();
			uint8 *d = sortie.Pixels();
			const uint8 *pa = a.Pixels();
			const uint8 *pb = b.Pixels();
			if (!d || !pa || !pb)
				return;
			const usize n = (usize)W * (usize)H;
			for (usize i = 0; i < n; ++i) {
				float32 A[4], B[4];
				for (int32 k = 0; k < 4; ++k) {
					A[k] = (float32)pa[i * 4u + (usize)k] / 255.f;
					B[k] = (float32)pb[i * 4u + (usize)k] / 255.f;
				}
				// les operateurs travaillent en PREMULTIPLIE (c'est leur definition)
				for (int32 k = 0; k < 3; ++k) {
					A[k] *= A[3];
					B[k] *= B[3];
				}
				float32 O[4];
				switch (op) {
					case OpComposite::In:
						for (int32 k = 0; k < 4; ++k) O[k] = A[k] * B[3];
						break;
					case OpComposite::Out:
						for (int32 k = 0; k < 4; ++k) O[k] = A[k] * (1.f - B[3]);
						break;
					case OpComposite::Atop:
						for (int32 k = 0; k < 4; ++k) O[k] = A[k] * B[3] + B[k] * (1.f - A[3]);
						break;
					case OpComposite::Xor:
						for (int32 k = 0; k < 4; ++k) O[k] = A[k] * (1.f - B[3]) + B[k] * (1.f - A[3]);
						break;
					case OpComposite::Arithmetique:
						for (int32 k = 0; k < 4; ++k) O[k] = k1 * A[k] * B[k] + k2 * A[k] + k3 * B[k] + k4;
						break;
					default: // Over
						for (int32 k = 0; k < 4; ++k) O[k] = A[k] + B[k] * (1.f - A[3]);
						break;
				}
				for (int32 k = 0; k < 4; ++k) {
					if (O[k] < 0.f) O[k] = 0.f;
					if (O[k] > 1.f) O[k] = 1.f;
				}
				// retour en alpha droit
				if (O[3] > 0.f)
					for (int32 k = 0; k < 3; ++k)
						O[k] = O[k] / O[3] > 1.f ? 1.f : O[k] / O[3];
				else
					O[0] = O[1] = O[2] = 0.f;
				for (int32 k = 0; k < 4; ++k)
					d[i * 4u + (usize)k] = (uint8)(O[k] * 255.f + 0.5f);
			}
		}

		/// feBlend : `a` sur `b` avec un mode de fusion.
		void Fusionner(const NkImage &a, const NkImage &b, OpFusion mode, NkImage &sortie) noexcept {
			const usize n = (usize)sortie.Width() * (usize)sortie.Height();
			uint8 *d = sortie.Pixels();
			const uint8 *pa = a.Pixels();
			const uint8 *pb = b.Pixels();
			if (!d || !pa || !pb)
				return;
			for (usize i = 0; i < n; ++i) {
				const float32 sa = (float32)pa[i * 4u + 3u] / 255.f;
				const float32 da = (float32)pb[i * 4u + 3u] / 255.f;
				const float32 oa = sa + da * (1.f - sa);
				for (int32 k = 0; k < 3; ++k) {
					const float32 cs = (float32)pa[i * 4u + (usize)k] / 255.f;
					const float32 cb = (float32)pb[i * 4u + (usize)k] / 255.f;
					float32 B = cs;
					switch (mode) {
						case OpFusion::Multiplier: B = cs * cb; break;
						case OpFusion::Ecran: B = cs + cb - cs * cb; break;
						case OpFusion::Assombrir: B = cs < cb ? cs : cb; break;
						case OpFusion::Eclaircir: B = cs > cb ? cs : cb; break;
						default: break;
					}
					const float32 cr = (1.f - da) * cs + da * B;
					float32 v = (oa > 0.f) ? (cr * sa + cb * da * (1.f - sa)) / oa : 0.f;
					if (v < 0.f) v = 0.f;
					if (v > 1.f) v = 1.f;
					d[i * 4u + (usize)k] = (uint8)(v * 255.f + 0.5f);
				}
				d[i * 4u + 3u] = (uint8)(oa * 255.f + 0.5f);
			}
		}

		/// Evalue le graphe d'un filtre. @p source = le groupe deja peint.
		/// @return l'image filtree (invalide si rien a faire).
		NkImage EvaluerFiltre(const Filtre &f, const NkImage &source, float32 echX, float32 echY) noexcept {
			const int32 W = source.Width(), H = source.Height();
			NkVector<ResultatFiltre> nommes;
			NkImage precedente = CopierImage(source);
			if (!precedente.IsValid())
				return NkImage();

			auto trouver = [&](const char *nom) -> const NkImage * {
				if (!nom || !nom[0])
					return nullptr;
				if (std::strcmp(nom, "SourceGraphic") == 0)
					return &source;
				for (uint32 i = 0; i < nommes.Size(); ++i)
					if (std::strcmp(nommes[i].nom, nom) == 0)
						return &nommes[i].img;
				return nullptr;
			};

			NkImage sourceAlpha; // fabriquee a la demande : la plupart des filtres l'utilisent
			for (uint32 ip = 0; ip < f.prims.Size(); ++ip) {
				const Primitive &p = f.prims[ip];
				// ── l'entree : nommee, ou la sortie de la primitive precedente ──
				NkImage entree;
				const NkImage *ref = trouver(p.in);
				if (!ref && p.in[0] && std::strcmp(p.in, "SourceAlpha") == 0) {
					if (!sourceAlpha.IsValid())
						sourceAlpha = AlphaSeul(source);
					ref = &sourceAlpha;
				}
				entree = ref ? CopierImage(*ref) : CopierImage(precedente);
				if (!entree.IsValid())
					break;

				NkImage sortie;
				switch (p.type) {
					case PrimType::Flou: {
						sortie = std::move(entree);
						FlouRGBA(sortie, p.ecartX * echX, p.ecartY * echY);
						break;
					}
					case PrimType::Decalage: {
						sortie = NkImage::Alloc(W, H, NkImagePixelFormat::NK_RGBA32);
						if (!sortie.IsValid())
							break;
						const int32 idx = (int32)(p.dx * echX + (p.dx >= 0.f ? 0.5f : -0.5f));
						const int32 idy = (int32)(p.dy * echY + (p.dy >= 0.f ? 0.5f : -0.5f));
						uint8 *d = sortie.Pixels();
						const uint8 *sp = entree.Pixels();
						for (int32 y = 0; y < H; ++y)
							for (int32 x = 0; x < W; ++x) {
								const int32 sx = x - idx, sy = y - idy;
								if (sx < 0 || sy < 0 || sx >= W || sy >= H)
									continue;
								for (int32 k = 0; k < 4; ++k)
									d[((usize)y * (usize)W + (usize)x) * 4u + (usize)k] =
										sp[((usize)sy * (usize)W + (usize)sx) * 4u + (usize)k];
							}
						break;
					}
					case PrimType::Aplat: {
						sortie = NkImage::Alloc(W, H, NkImagePixelFormat::NK_RGBA32);
						if (!sortie.IsValid())
							break;
						uint8 *d = sortie.Pixels();
						const usize n = (usize)W * (usize)H;
						for (usize i = 0; i < n; ++i) {
							d[i * 4u] = p.couleur.r;
							d[i * 4u + 1u] = p.couleur.g;
							d[i * 4u + 2u] = p.couleur.b;
							d[i * 4u + 3u] = p.couleur.a;
						}
						break;
					}
					case PrimType::Matrice: {
						sortie = std::move(entree);
						uint8 *d = sortie.Pixels();
						const usize n = (usize)W * (usize)H;
						for (usize i = 0; i < n; ++i) {
							const float32 r = (float32)d[i * 4u] / 255.f, g = (float32)d[i * 4u + 1u] / 255.f;
							const float32 b = (float32)d[i * 4u + 2u] / 255.f, a = (float32)d[i * 4u + 3u] / 255.f;
							float32 o[4];
							for (int32 k = 0; k < 4; ++k)
								o[k] = p.mat[k * 5 + 0] * r + p.mat[k * 5 + 1] * g + p.mat[k * 5 + 2] * b +
									   p.mat[k * 5 + 3] * a + p.mat[k * 5 + 4];
							for (int32 k = 0; k < 4; ++k) {
								if (o[k] < 0.f) o[k] = 0.f;
								if (o[k] > 1.f) o[k] = 1.f;
								d[i * 4u + (usize)k] = (uint8)(o[k] * 255.f + 0.5f);
							}
						}
						break;
					}
					case PrimType::Composition:
					case PrimType::Fusion: {
						const NkImage *b2 = trouver(p.in2);
						NkImage deux;
						if (!b2 && p.in2[0] && std::strcmp(p.in2, "SourceAlpha") == 0) {
							if (!sourceAlpha.IsValid())
								sourceAlpha = AlphaSeul(source);
							b2 = &sourceAlpha;
						}
						if (!b2)
							b2 = &source;
						sortie = NkImage::Alloc(W, H, NkImagePixelFormat::NK_RGBA32);
						if (!sortie.IsValid())
							break;
						if (p.type == PrimType::Composition)
							Composer(entree, *b2, p.op, p.k1, p.k2, p.k3, p.k4, sortie);
						else
							Fusionner(entree, *b2, p.fusion, sortie);
						break;
					}
					case PrimType::Assemblage: {
						sortie = NkImage::Alloc(W, H, NkImagePixelFormat::NK_RGBA32);
						if (!sortie.IsValid())
							break;
						// feMerge empile ses entrees dans l'ordre : la premiere DESSOUS.
						for (int32 m = 0; m < p.nMerges; ++m) {
							const NkImage *src2 = trouver(p.merges[m]);
							if (!src2 && p.merges[m][0] && std::strcmp(p.merges[m], "SourceAlpha") == 0) {
								if (!sourceAlpha.IsValid())
									sourceAlpha = AlphaSeul(source);
								src2 = &sourceAlpha;
							}
							if (!src2)
								src2 = &precedente;
							ComposerCalque(sortie, *src2);
						}
						break;
					}
					case PrimType::Ombre: {
						// LE RACCOURCI, exprime avec les memes briques : l'alpha de
						// l'entree, floute, decale, teinte, puis l'entree PAR-DESSUS.
						NkImage ombre = AlphaSeul(entree);
						if (!ombre.IsValid())
							break;
						FlouRGBA(ombre, p.ecartX * echX, p.ecartY * echY);
						uint8 *d = ombre.Pixels();
						const usize n = (usize)W * (usize)H;
						for (usize i = 0; i < n; ++i) {
							d[i * 4u] = p.couleur.r;
							d[i * 4u + 1u] = p.couleur.g;
							d[i * 4u + 2u] = p.couleur.b;
							d[i * 4u + 3u] = (uint8)(((int32)d[i * 4u + 3u] * (int32)p.couleur.a + 127) / 255);
						}
						sortie = NkImage::Alloc(W, H, NkImagePixelFormat::NK_RGBA32);
						if (!sortie.IsValid())
							break;
						const int32 idx = (int32)(p.dx * echX + (p.dx >= 0.f ? 0.5f : -0.5f));
						const int32 idy = (int32)(p.dy * echY + (p.dy >= 0.f ? 0.5f : -0.5f));
						uint8 *od = sortie.Pixels();
						const uint8 *op2 = ombre.Pixels();
						for (int32 y = 0; y < H; ++y)
							for (int32 x = 0; x < W; ++x) {
								const int32 sx = x - idx, sy = y - idy;
								if (sx < 0 || sy < 0 || sx >= W || sy >= H)
									continue;
								for (int32 k = 0; k < 4; ++k)
									od[((usize)y * (usize)W + (usize)x) * 4u + (usize)k] =
										op2[((usize)sy * (usize)W + (usize)sx) * 4u + (usize)k];
							}
						ComposerCalque(sortie, entree); // la forme PAR-DESSUS son ombre
						break;
					}
					default: // Inconnue : l'entree passe telle quelle, le chainage tient
						sortie = std::move(entree);
						break;
				}
				if (!sortie.IsValid())
					continue;
				if (p.result[0] && nommes.Size() < 16u) {
					ResultatFiltre r;
					std::strncpy(r.nom, p.result, sizeof(r.nom) - 1);
					r.img = CopierImage(sortie);
					nommes.PushBack(std::move(r));
				}
				precedente = std::move(sortie);
			}
			return precedente;
		}

		/// Donnees opaques d'un NkSVGImage : shapes vectorielles + dimensions natives.
		/// Pointe par NkSVGImage::mImpl (PIMPL).
		struct SVGImageImpl {
				NkVector<Shape> shapes;
				NkVector<Gradient> gradients;
				NkVector<Filtre> filtres;
				NkVector<ClipPath> clips;
				NkVector<Motif> motifs;
				float32 vbX = 0, vbY = 0, vbW = 0, vbH = 0;
				float32 svgW = 0, svgH = 0;
				SkipList skips; ///< ce que le decodage a saute (dit une fois par nom)
		};

		/// Trouve un motif par id.
		const Motif *TrouverMotif(const SVGImageImpl *impl, const char *id) noexcept {
			if (!impl || !id || !id[0])
				return nullptr;
			for (uint32 i = 0; i < impl->motifs.Size(); ++i)
				if (std::strcmp(impl->motifs[i].id, id) == 0)
					return &impl->motifs[i];
			return nullptr;
		}

		/// Trouve un clipPath par id.
		const ClipPath *TrouverClip(const SVGImageImpl *impl, const char *id) noexcept {
			if (!impl || !id || !id[0])
				return nullptr;
			for (uint32 i = 0; i < impl->clips.Size(); ++i)
				if (std::strcmp(impl->clips[i].id, id) == 0)
					return &impl->clips[i];
			return nullptr;
		}

		/// Trouve un filtre par id. nullptr si absent (ou sans feDropShadow).
		const Filtre *TrouverFiltre(const SVGImageImpl *impl, const char *id) noexcept {
			if (!impl || !id || !id[0])
				return nullptr;
			for (uint32 i = 0; i < impl->filtres.Size(); ++i)
				if (std::strcmp(impl->filtres[i].id, id) == 0)
					return impl->filtres[i].prims.IsEmpty() ? nullptr : &impl->filtres[i];
			return nullptr;
		}

		/// Trouve un gradient par id (recherche lineaire). nullptr si absent.
		const Gradient *FindGradient(const SVGImageImpl *impl, const char *id) noexcept {
			if (!impl || !id || !id[0])
				return nullptr;
			for (uint32 i = 0; i < impl->gradients.Size(); ++i) {
				if (std::strcmp(impl->gradients[i].id, id) == 0)
					return &impl->gradients[i];
			}
			return nullptr;
		}

		/// Construit une GradPaint (geometrie en pixels destination) pour un ref donne.
		/// @p localPts = shape deja transformee (pour le bbox objectBoundingBox).
		/// @p ctm/@p mView = transforms pour le mode userSpaceOnUse.
		/// @return false si le gradient/les stops sont introuvables.
		bool BuildGradPaint(const SVGImageImpl *impl, const char *ref, const Shape &localPts, const NkSVGTransform &ctm,
							const NkSVGTransform &mView, GradPaint &gp) noexcept {
			const Gradient *g = FindGradient(impl, ref);
			if (!g)
				return false;

			// ── L'HERITAGE PAR href ──────────────────────────────────────────
			// Un gradient qui en reference un autre herite TOUT ce qu'il n'a pas dit
			// lui-meme : ses arrets, mais aussi sa geometrie, ses unites, sa matrice
			// et son etalement. N'heriter que les arrets (ce qu'on faisait) donne des
			// degrades geometriquement FAUX des qu'un fichier factorise ses
			// definitions -- une pratique courante des exporteurs.
			const Gradient *base = g->href[0] ? FindGradient(impl, g->href) : nullptr;
			const Gradient *sg = g;
			if (g->stops.IsEmpty() && base)
				sg = base;
			if (sg->stops.IsEmpty())
				return false;
			gp.stops = &sg->stops[0];
			gp.nStops = (int32)sg->stops.Size();
			gp.kind = g->kind;
			gp.spread = (g->hasSpread || !base) ? g->spread : base->spread;
			const bool userSpace = (g->hasUnits || !base) ? g->userSpace : base->userSpace;
			const NkSVGTransform gxf = (g->hasXform || !base) ? g->xform : base->xform;
			const float32 gx1 = (g->hasX1 || !base) ? g->x1 : base->x1;
			const float32 gy1 = (g->hasY1 || !base) ? g->y1 : base->y1;
			const float32 gx2 = (g->hasX2 || !base) ? g->x2 : base->x2;
			const float32 gy2 = (g->hasY2 || !base) ? g->y2 : base->y2;
			const float32 gcx = (g->hasCx || !base) ? g->cx : base->cx;
			const float32 gcy = (g->hasCy || !base) ? g->cy : base->cy;
			const float32 gr = (g->hasR || !base) ? g->r : base->r;
			const bool focal = g->hasFocal || (base && base->hasFocal);
			const float32 gfx = g->hasFocal ? g->fx : (base && base->hasFocal ? base->fx : gcx);
			const float32 gfy = g->hasFocal ? g->fy : (base && base->hasFocal ? base->fy : gcy);

			gp.x1 = gx1;
			gp.y1 = gy1;
			gp.x2 = gx2;
			gp.y2 = gy2;
			gp.cx = gcx;
			gp.cy = gcy;
			gp.r = gr;
			gp.fx = focal ? gfx : gcx;
			gp.fy = focal ? gfy : gcy;
			// LE FOYER DOIT RESTER DANS LE CERCLE (la norme le ramene sur le bord
			// sinon) : au-dehors, le rayon ne coupe plus le cercle et la couleur
			// partirait a l'infini.
			{
				const float32 ddx = gp.fx - gp.cx, ddy = gp.fy - gp.cy;
				const float32 d2 = ddx * ddx + ddy * ddy;
				if (gp.r > 1e-6f && d2 > gp.r * gp.r * 0.9801f) { // 0.99 r
					const float32 d = std::sqrt(d2);
					const float32 k = (gp.r * 0.99f) / d;
					gp.fx = gp.cx + ddx * k;
					gp.fy = gp.cy + ddy * k;
				}
			}

			// LA MATRICE : espace du degrade -> pixels. On l'inverse une fois par
			// forme, pas une fois par pixel.
			NkSVGTransform versPixels;
			if (!userSpace) {
				// objectBoundingBox : le degrade est decrit dans le carre unite de la
				// BOITE de la forme. bbox <- points DEJA transformes (destination).
				float32 minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
				for (uint32 k = 0; k < localPts.xs.Size(); ++k) {
					const float32 x = localPts.xs[k], y = localPts.ys[k];
					if (x < minx)
						minx = x;
					if (x > maxx)
						maxx = x;
					if (y < miny)
						miny = y;
					if (y > maxy)
						maxy = y;
				}
				float32 w = maxx - minx, h = maxy - miny;
				if (w <= 0.f)
					w = 1.f;
				if (h <= 0.f)
					h = 1.f;
				// unite -> boite, PUIS gradientTransform (qui s'applique dans l'espace
				// du degrade) : c'est cette composition qui manquait.
				versPixels = NkSVGTransform::Translate(minx, miny) * NkSVGTransform::Scale(w, h) * gxf;
			} else {
				// userSpaceOnUse : user -> dest, gradientTransform comprise.
				versPixels = mView * (ctm * gxf);
			}
			if (!Inverser(versPixels, gp.inv))
				return false;
			return true;
		}

		// LA SOURCE DE GLYPHES PAR DEFAUT. Un pointeur, pose UNE FOIS au demarrage
		// par l'application qui veut du texte (elle seule sait quelles polices elle
		// embarque). Nul par defaut : le texte est alors saute ET DIT. Ce n'est pas
		// un etat mutable en cours de route -- le poser depuis plusieurs fils
		// pendant un decodage n'aurait aucun sens.
		NkIGlyphSource *gSourceParDefaut = nullptr;

	} // anonymous namespace

	// ═════════════════════════════════════════════════════════════════════════════
	// SECTION 9.5 — Triangulation : ear-clipping pour acces mesh 3D / collision
	// ═════════════════════════════════════════════════════════════════════════════

	namespace {

		/// Signed area (algorithm shoelace). Positif = CCW, negatif = CW.
		float32 SignedArea(const float32 *xs, const float32 *ys, int32 n) noexcept {
			float32 s = 0.f;
			for (int32 i = 0; i < n; ++i) {
				const int32 j = (i + 1) % n;
				s += xs[i] * ys[j] - xs[j] * ys[i];
			}
			return s * 0.5f;
		}

		/// Test point-in-triangle via signes des aires (barycentric-like).
		bool PointInTriangle(float32 px, float32 py, float32 ax, float32 ay, float32 bx, float32 by, float32 cx,
							 float32 cy) noexcept {
			const float32 d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
			const float32 d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
			const float32 d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
			const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
			const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
			return !(hasNeg && hasPos);
		}

		/// Triangule un contour simple par ear-clipping. Output : indices triangle-list
		/// dans le repere original (les indices pointent dans @p xs / @p ys, decales
		/// par @p baseIndex). @return nombre de triangles produits.
		/// Algo O(N²) -- OK pour contours < ~500 vertices.
		int32 EarClipContour(const float32 *xs, const float32 *ys, int32 n, uint32 baseIndex,
							 NkVector<uint32> &outIndices) noexcept {
			if (n < 3)
				return 0;
			// Detection orientation. Si CW, on inverse l'ordre (ear-clipping veut CCW).
			const bool isCCW = (SignedArea(xs, ys, n) > 0.f);

			// Liste de vertices restants (indices dans xs/ys).
			NkVector<int32> verts;
			verts.Reserve((usize)n);
			if (isCCW) {
				for (int32 i = 0; i < n; ++i)
					verts.PushBack(i);
			} else {
				for (int32 i = n - 1; i >= 0; --i)
					verts.PushBack(i);
			}

			int32 triCount = 0;
			int32 guard = 2 * n; // protection anti-boucle infinie (cas degenere)
			while (verts.Size() >= 3 && guard-- > 0) {
				const int32 nv = (int32)verts.Size();
				bool earFound = false;
				for (int32 i = 0; i < nv; ++i) {
					const int32 i0 = verts[(i - 1 + nv) % nv];
					const int32 i1 = verts[i];
					const int32 i2 = verts[(i + 1) % nv];
					const float32 ax = xs[i0], ay = ys[i0];
					const float32 bx = xs[i1], by = ys[i1];
					const float32 cx = xs[i2], cy = ys[i2];
					// Convexite (cross product positif en CCW).
					const float32 cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
					if (cross <= 0.f)
						continue;
					// Aucun autre vertex du polygone a l'interieur du triangle ?
					bool inside = false;
					for (int32 k = 0; k < nv && !inside; ++k) {
						const int32 ki = verts[k];
						if (ki == i0 || ki == i1 || ki == i2)
							continue;
						if (PointInTriangle(xs[ki], ys[ki], ax, ay, bx, by, cx, cy)) {
							inside = true;
						}
					}
					if (inside)
						continue;
					// Ear trouve : output triangle + retire i1.
					outIndices.PushBack(baseIndex + (uint32)i0);
					outIndices.PushBack(baseIndex + (uint32)i1);
					outIndices.PushBack(baseIndex + (uint32)i2);
					++triCount;
					verts.RemoveAt((uint32)i);
					earFound = true;
					break;
				}
				if (!earFound)
					break; // Polygone degenere ou non-simple
			}
			return triCount;
		}

		/// Pack RGBA8 -> uint32 (little-endian : R=byte0, G=byte1, B=byte2, A=byte3).
		uint32 PackRGBA(uint8 r, uint8 g, uint8 b, uint8 a) noexcept {
			return ((uint32)r) | ((uint32)g << 8) | ((uint32)b << 16) | ((uint32)a << 24);
		}

	} // anonymous namespace

	// ═════════════════════════════════════════════════════════════════════════════
	// SECTION 9.6 — NkSVGShapeView : implementation
	// ═════════════════════════════════════════════════════════════════════════════

	int32 NkSVGShapeView::ContourCount() const noexcept {
		const Shape *sh = (const Shape *)mShapePtr;
		return sh ? (int32)sh->contourStart.Size() : 0;
	}

	int32 NkSVGShapeView::ContourPointCount(int32 ci) const noexcept {
		const Shape *sh = (const Shape *)mShapePtr;
		if (!sh || ci < 0 || (uint32)ci >= sh->contourLen.Size())
			return 0;
		return sh->contourLen[(uint32)ci];
	}

	const float32 *NkSVGShapeView::ContourXs(int32 ci) const noexcept {
		const Shape *sh = (const Shape *)mShapePtr;
		if (!sh || ci < 0 || (uint32)ci >= sh->contourStart.Size())
			return nullptr;
		return &sh->xs[(uint32)sh->contourStart[(uint32)ci]];
	}

	const float32 *NkSVGShapeView::ContourYs(int32 ci) const noexcept {
		const Shape *sh = (const Shape *)mShapePtr;
		if (!sh || ci < 0 || (uint32)ci >= sh->contourStart.Size())
			return nullptr;
		return &sh->ys[(uint32)sh->contourStart[(uint32)ci]];
	}

	NkSVGColor NkSVGShapeView::FillColor() const noexcept {
		const Shape *sh = (const Shape *)mShapePtr;
		return sh ? sh->style.fill : NkSVGColor::Black();
	}

	NkSVGColor NkSVGShapeView::StrokeColor() const noexcept {
		const Shape *sh = (const Shape *)mShapePtr;
		return sh ? sh->style.stroke : NkSVGColor::None();
	}

	float32 NkSVGShapeView::StrokeWidth() const noexcept {
		const Shape *sh = (const Shape *)mShapePtr;
		return sh ? sh->style.strokeWidth : 1.f;
	}

	float32 NkSVGShapeView::Opacity() const noexcept {
		const Shape *sh = (const Shape *)mShapePtr;
		return sh ? sh->style.opacity : 1.f;
	}

	bool NkSVGShapeView::FillEvenOdd() const noexcept {
		const Shape *sh = (const Shape *)mShapePtr;
		return sh && sh->style.fillEvenOdd;
	}

	int32 NkSVGShapeView::Triangulate(NkVector<float32> &outXs, NkVector<float32> &outYs, NkVector<uint32> &outIndices,
									  uint32 baseIndex) const noexcept {
		const Shape *sh = (const Shape *)mShapePtr;
		if (!sh)
			return 0;

		// Si baseIndex == 0 et que les buffers de sortie sont vides, on peut
		// commencer a 0. Sinon on utilise baseIndex pour chainer (appel par
		// TriangulateAll en boucle sur les shapes).
		uint32 curBase = baseIndex;
		int32 totalTris = 0;
		for (uint32 ci = 0; ci < sh->contourStart.Size(); ++ci) {
			const int32 start = sh->contourStart[ci];
			const int32 len = sh->contourLen[ci];
			if (len < 3)
				continue;
			// Le contour peut etre ferme (dernier point == premier). On detecte
			// et on l'ignore pour l'ear clipping.
			const float32 fx = sh->xs[(uint32)start];
			const float32 fy = sh->ys[(uint32)start];
			const float32 lx = sh->xs[(uint32)(start + len - 1)];
			const float32 ly = sh->ys[(uint32)(start + len - 1)];
			int32 n = len;
			if (std::fabs(fx - lx) < 1e-5f && std::fabs(fy - ly) < 1e-5f) {
				n = len - 1;
			}
			if (n < 3)
				continue;

			// Copie locale des vertices pour ear-clipping (puis push dans outXs/Ys).
			for (int32 k = 0; k < n; ++k) {
				outXs.PushBack(sh->xs[(uint32)(start + k)]);
				outYs.PushBack(sh->ys[(uint32)(start + k)]);
			}
			const uint32 contourBase = curBase;
			totalTris += EarClipContour(&sh->xs[(uint32)start], &sh->ys[(uint32)start], n, contourBase, outIndices);
			curBase += (uint32)n;
		}
		return totalTris;
	}

	// ═════════════════════════════════════════════════════════════════════════════
	// SECTION 10 — NkSVGImage API publique : stockage vectoriel + rasterize ondemand
	// ═════════════════════════════════════════════════════════════════════════════

	NkSVGImage *NkSVGImage::LoadFromMemory(const uint8 *data, usize size) noexcept {
		return LoadFromMemory(data, size, nullptr, gSourceParDefaut);
	}

	NkSVGImage *NkSVGImage::LoadFromMemory(const uint8 *data, usize size, const char *baseDir) noexcept {
		return LoadFromMemory(data, size, baseDir, gSourceParDefaut);
	}

	NkSVGImage *NkSVGImage::LoadFromMemory(const uint8 *data, usize size, const char *baseDir,
										   NkIGlyphSource *glyphes) noexcept {
		if (!data || size < 5)
			return nullptr;
		const char *xml = reinterpret_cast<const char *>(data);

		// Alloc impl + parse.
		SVGImageImpl *impl = (SVGImageImpl *)NkAlloc(sizeof(SVGImageImpl));
		if (!impl)
			return nullptr;
		new (impl) SVGImageImpl();
		UseCtx useCtx; // l'index des id vit le temps du decodage, pas au-dela
		ParseSVGDocument(xml, size, impl->shapes, impl->gradients, impl->filtres, impl->clips, impl->motifs,
						 impl->vbX, impl->vbY, impl->vbW, impl->vbH, impl->svgW, impl->svgH, impl->skips, baseDir,
						 glyphes, useCtx);

		// Si parsing a echoue (aucun shape ET aucune viewBox), on libere.
		if (impl->shapes.IsEmpty() && impl->vbW <= 0.f && impl->svgW <= 0.f) {
			impl->~SVGImageImpl();
			NkFree(impl);
			return nullptr;
		}

		// Defauts raisonnables si l'auteur du SVG a omis viewBox ou width/height.
		if (impl->vbW <= 0.f)
			impl->vbW = (impl->svgW > 0.f) ? impl->svgW : 800.f;
		if (impl->vbH <= 0.f)
			impl->vbH = (impl->svgH > 0.f) ? impl->svgH : 600.f;
		if (impl->svgW <= 0.f)
			impl->svgW = impl->vbW;
		if (impl->svgH <= 0.f)
			impl->svgH = impl->vbH;

		// Alloc l'image vectorielle (objet + impl).
		NkSVGImage *svg = (NkSVGImage *)NkAlloc(sizeof(NkSVGImage));
		if (!svg) {
			impl->~SVGImageImpl();
			NkFree(impl);
			return nullptr;
		}
		new (svg) NkSVGImage();
		svg->mImpl = impl;
		return svg;
	}

	NkSVGImage *NkSVGImage::LoadFromFile(const char *path) noexcept {
		if (!path)
			return nullptr;
		NkFile f;
		if (!f.Open(path, NkFileMode::NK_READ))
			return nullptr;
		const usize sz = (usize)f.Size();
		if (sz == 0) {
			f.Close();
			return nullptr;
		}
		uint8 *buf = (uint8 *)NkAlloc(sz + 1);
		if (!buf) {
			f.Close();
			return nullptr;
		}
		const usize read = f.Read(buf, sz);
		buf[read] = 0;
		f.Close();
		// LE DOSSIER DU FICHIER : un href relatif se resout par rapport au .svg qui
		// le porte, jamais par rapport au repertoire courant du processus.
		char base[1024];
		std::strncpy(base, path, sizeof(base) - 1);
		base[sizeof(base) - 1] = 0;
		usize coupe = 0;
		for (usize i = 0; base[i]; ++i)
			if (base[i] == '/' || base[i] == '\\')
				coupe = i;
		base[coupe] = 0; // "" si le chemin n'a pas de dossier : le courant, et c'est juste
		NkSVGImage *svg = LoadFromMemory(buf, read, base, gSourceParDefaut);
		NkFree(buf);
		return svg;
	}

	NkImage NkSVGImage::Rasterize(int32 outW, int32 outH) const noexcept {
		const SVGImageImpl *impl = (const SVGImageImpl *)mImpl;
		if (!impl)
			return NkImage();

		// Resolution de la taille de sortie.
		if (outW <= 0 && outH <= 0) {
			outW = (int32)impl->svgW;
			outH = (int32)impl->svgH;
		} else if (outW <= 0) {
			outW = (int32)((float32)outH * impl->vbW / impl->vbH);
		} else if (outH <= 0) {
			outH = (int32)((float32)outW * impl->vbH / impl->vbW);
		}
		if (outW <= 0)
			outW = 1;
		if (outH <= 0)
			outH = 1;

		// Mapping viewBox -> taille de sortie.
		const NkSVGTransform mView = NkSVGTransform::Scale((float32)outW / impl->vbW, (float32)outH / impl->vbH) *
									 NkSVGTransform::Translate(-impl->vbX, -impl->vbY);

		// Alloc image RGBA32 (zero-initialise = fond transparent).
		NkImage img = NkImage::Alloc(outW, outH, NkImagePixelFormat::NK_RGBA32);
		if (!img.IsValid())
			return NkImage();

		// Copie locale des shapes + applique le mapping a leurs points, puis
		// rasterise. On NE modifie PAS l'impl source (pour que Rasterize() puisse
		// etre appele plusieurs fois avec des tailles differentes).
		// ── LES MASQUES DE DECOUPE, calcules a la demande et GARDES ────────────
		//    Un meme <clipPath> sert souvent a des dizaines de formes : le rasteriser
		//    une fois par forme serait le travail refait pour rien.
		struct MasqueCache {
				char id[64] = {0};
				NkVector<uint8> px;
		};
		NkVector<MasqueCache> masques;

		auto masqueDe = [&](const char *ref, const Shape &pour) -> const uint8 * {
			if (!ref || !ref[0])
				return nullptr;
			const ClipPath *cp = TrouverClip(impl, ref);
			if (!cp || cp->formes.IsEmpty())
				return nullptr;
			// objectBoundingBox : le clip est decrit dans le carre unite de la boite
			// de CETTE forme -- il ne peut donc pas etre partage entre formes.
			const bool partageable = cp->userSpace;
			if (partageable) {
				for (uint32 i = 0; i < masques.Size(); ++i)
					if (std::strcmp(masques[i].id, ref) == 0)
						return masques[i].px.IsEmpty() ? nullptr : &masques[i].px[0];
			}
			// bbox de la forme a decouper (en destination), pour objectBoundingBox
			NkSVGTransform versDest = mView;
			if (!cp->userSpace) {
				float32 minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
				for (uint32 k = 0; k < pour.xs.Size(); ++k) {
					float32 x = pour.xs[k], y = pour.ys[k];
					mView.Apply(x, y);
					if (x < minx) minx = x;
					if (x > maxx) maxx = x;
					if (y < miny) miny = y;
					if (y > maxy) maxy = y;
				}
				float32 w = maxx - minx, h = maxy - miny;
				if (w <= 0.f) w = 1.f;
				if (h <= 0.f) h = 1.f;
				versDest = NkSVGTransform::Translate(minx, miny) * NkSVGTransform::Scale(w, h);
			}
			// on rasterise les formes du clip en BLANC OPAQUE : leur alpha EST le masque
			NkImage tampon = NkImage::Alloc(outW, outH, NkImagePixelFormat::NK_RGBA32);
			if (!tampon.IsValid())
				return nullptr;
			for (uint32 f = 0; f < cp->formes.Size(); ++f) {
				const Shape &src = cp->formes[f];
				Shape loc;
				loc.style = src.style;
				if (!cp->masque) {
					// UN DECOUPAGE NE CONNAIT QUE DEDANS / DEHORS : la couleur des
					// formes du <clipPath> n'a aucun sens, on peint en blanc opaque et
					// c'est leur COUVERTURE qui devient le masque.
					loc.style.fill = NkSVGColor::White();
					loc.style.stroke = NkSVGColor::None();
					loc.style.opacity = 1.f;
					loc.style.fillOpacity = 1.f;
				}
				// UN MASQUE, lui, garde ses couleurs : c'est leur LUMINANCE qu'on lira.
				loc.style.visible = true;
				// `clip-rule` est le fill-rule des formes du clip : il est deja dans
				// leur style, lu comme n'importe quel attribut.
				for (uint32 k = 0; k < src.xs.Size(); ++k) {
					float32 x = src.xs[k], y = src.ys[k];
					versDest.Apply(x, y);
					loc.xs.PushBack(x);
					loc.ys.PushBack(y);
				}
				for (uint32 k = 0; k < src.contourStart.Size(); ++k) {
					loc.contourStart.PushBack(src.contourStart[k]);
					loc.contourLen.PushBack(src.contourLen[k]);
				}
				// LE DEGRADE D'UN MASQUE DOIT ETRE RESOLU COMME LES AUTRES. Sans ca,
				// `fill="url(#g)"` n'est pas une couleur lisible et retombe sur du
				// NOIR -- or un masque noir cache tout : le fondu, qui est l'usage
				// principal d'un <mask>, rendait une image entierement vide.
				GradPaint gp;
				bool aGrad = false;
				if (cp->masque && src.fillRef[0])
					aGrad = BuildGradPaint(impl, src.fillRef, loc, src.ctm, versDest, gp);
				if (cp->masque && src.fillRef[0] && !aGrad)
					continue; // reference morte : ne rien peindre plutot que du noir
				RasterizeShape(tampon, loc, aGrad ? &gp : nullptr, nullptr);
			}
			MasqueCache mc;
			std::strncpy(mc.id, ref, sizeof(mc.id) - 1);
			mc.px.Resize((usize)outW * (usize)outH);
			const uint8 *tp = tampon.Pixels();
			for (int32 i = 0; i < outW * outH; ++i) {
				const usize o = (usize)i * 4u;
				if (!cp->masque || cp->typeAlpha) {
					mc.px[(uint32)i] = tp[o + 3u];
				} else {
					// LUMINANCE (coefficients de luminosite de sRGB, ceux que la norme
					// SVG donne pour `mask`), ponderee par l'alpha : un blanc opaque
					// laisse tout passer, un noir opaque bloque tout, un transparent
					// aussi.
					const float32 lum = 0.2125f * (float32)tp[o + 0u] + 0.7154f * (float32)tp[o + 1u] +
										0.0721f * (float32)tp[o + 2u];
					const float32 v = lum * ((float32)tp[o + 3u] / 255.f);
					mc.px[(uint32)i] = (uint8)(v > 255.f ? 255.f : (v < 0.f ? 0.f : v));
				}
			}
			masques.PushBack(std::move(mc));
			return &masques[masques.Size() - 1].px[0];
		};

		/// L'INTERSECTION des masques herites. Un seul clip : on rend le sien, sans
		/// copie. Plusieurs : on multiplie -- ne garder que ce que TOUS gardent.
		NkVector<uint8> clipCompose;
		auto clipDe = [&](const Shape &src) -> const uint8 * {
			if (src.nClips <= 0)
				return nullptr;
			if (src.nClips == 1)
				return masqueDe(src.clipRefs[0], src);
			clipCompose.Clear();
			clipCompose.Resize((usize)outW * (usize)outH);
			for (int32 i = 0; i < outW * outH; ++i)
				clipCompose[(uint32)i] = 255u;
			bool un = false;
			for (int32 c = 0; c < src.nClips; ++c) {
				const uint8 *m = masqueDe(src.clipRefs[c], src);
				if (!m)
					continue;
				un = true;
				for (int32 i = 0; i < outW * outH; ++i)
					clipCompose[(uint32)i] = (uint8)(((int32)clipCompose[(uint32)i] * (int32)m[i] + 127) / 255);
			}
			return un ? &clipCompose[0] : nullptr;
		};

		// ── LES TUILES DE MOTIF, rasterisees a la demande et GARDEES ───────────
		struct TuileCache {
				char id[64] = {0};
				NkImage img;
				float32 perX = 1.f, perY = 1.f;
				NkSVGTransform base; ///< motif -> pixels (sans le modulo)
		};
		NkVector<TuileCache> tuiles;

		/// @return false si le motif est inconnu ou vide (l'appelant ne peint alors
		///         RIEN plutot qu'un aplat noir : une reference morte n'est pas une
		///         couleur).
		auto motifPour = [&](const char *ref, const Shape &pour, MotifPaint &mp) -> bool {
			const Motif *m = TrouverMotif(impl, ref);
			if (!m)
				return false;
			// un pattern peut heriter le CONTENU d'un autre (href) : c'est ainsi que
			// les exporteurs declinent un motif en plusieurs couleurs.
			const Motif *contenu = m;
			if (m->formes.IsEmpty() && m->href[0]) {
				const Motif *h = TrouverMotif(impl, m->href);
				if (h)
					contenu = h;
			}
			if (contenu->formes.IsEmpty())
				return false;

			// la bbox de la forme a remplir (objectBoundingBox est le defaut ici)
			float32 minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
			for (uint32 k = 0; k < pour.xs.Size(); ++k) {
				const float32 x = pour.xs[k], y = pour.ys[k];
				if (x < minx) minx = x;
				if (x > maxx) maxx = x;
				if (y < miny) miny = y;
				if (y > maxy) maxy = y;
			}
			float32 bw = maxx - minx, bh = maxy - miny;
			if (bw <= 0.f) bw = 1.f;
			if (bh <= 0.f) bh = 1.f;

			// la periode et l'origine, en PIXELS de destination
			const float32 echX = std::sqrt(mView.a * mView.a + mView.b * mView.b);
			const float32 echY = std::sqrt(mView.c * mView.c + mView.d * mView.d);
			float32 perX, perY, ox, oy;
			if (m->userSpace) {
				perX = m->w * echX;
				perY = m->h * echY;
				float32 px = m->x, py = m->y;
				mView.Apply(px, py);
				ox = px;
				oy = py;
			} else { // objectBoundingBox : fractions de la boite
				perX = m->w * bw;
				perY = m->h * bh;
				ox = minx + m->x * bw;
				oy = miny + m->y * bh;
			}
			if (perX <= 0.5f || perY <= 0.5f)
				return false;

			char cle[80];
			std::snprintf(cle, sizeof(cle), "%s|%d|%d", ref, (int32)(perX * 4.f), (int32)(perY * 4.f));
			TuileCache *tc = nullptr;
			for (uint32 i = 0; i < tuiles.Size(); ++i)
				if (std::strcmp(tuiles[i].id, cle) == 0)
					tc = &tuiles[i];
			if (!tc) {
				int32 tw = (int32)std::ceil(perX), th = (int32)std::ceil(perY);
				if (tw < 1) tw = 1;
				if (th < 1) th = 1;
				if (tw > 2048) tw = 2048;
				if (th > 2048) th = 2048;
				TuileCache nouvelle;
				std::strncpy(nouvelle.id, cle, sizeof(nouvelle.id) - 1);
				nouvelle.img = NkImage::Alloc(tw, th, NkImagePixelFormat::NK_RGBA32);
				if (!nouvelle.img.IsValid())
					return false;
				nouvelle.perX = perX;
				nouvelle.perY = perY;
				// le contenu -> la tuile : viewBox si elle existe, sinon l'echelle
				// des unites utilisateur.
				NkSVGTransform versTuile;
				if (m->aViewBox)
					versTuile = NkSVGTransform::Scale((float32)tw / m->vbW, (float32)th / m->vbH) *
								NkSVGTransform::Translate(-m->vbX, -m->vbY);
				else if (m->contenuUserSpace)
					versTuile = NkSVGTransform::Scale(perX / (m->userSpace ? m->w : (m->w * bw / echX)),
													  perY / (m->userSpace ? m->h : (m->h * bh / echY)));
				else
					versTuile = NkSVGTransform::Scale((float32)tw, (float32)th);
				for (uint32 f = 0; f < contenu->formes.Size(); ++f) {
					const Shape &sf = contenu->formes[f];
					Shape loc;
					loc.style = sf.style;
					loc.style.visible = true;
					for (uint32 k = 0; k < sf.xs.Size(); ++k) {
						float32 x = sf.xs[k], y = sf.ys[k];
						versTuile.Apply(x, y);
						loc.xs.PushBack(x);
						loc.ys.PushBack(y);
					}
					for (uint32 k = 0; k < sf.contourStart.Size(); ++k) {
						loc.contourStart.PushBack(sf.contourStart[k]);
						loc.contourLen.PushBack(sf.contourLen[k]);
					}
					GradPaint gp;
					bool aGrad = false;
					if (sf.fillRef[0])
						aGrad = BuildGradPaint(impl, sf.fillRef, loc, sf.ctm, versTuile, gp);
					if (sf.fillRef[0] && !aGrad)
						continue;
					RasterizeShape(nouvelle.img, loc, aGrad ? &gp : nullptr, nullptr, nullptr);
				}
				tuiles.PushBack(std::move(nouvelle));
				tc = &tuiles[tuiles.Size() - 1];
			}
			// pixel -> espace du motif : on translate a l'origine, puis on defait
			// `patternTransform`.
			NkSVGTransform versPixels = NkSVGTransform::Translate(ox, oy) * m->xform;
			if (!Inverser(versPixels, mp.inv))
				return false;
			mp.tuile = &tc->img;
			mp.periodeX = tc->perX;
			mp.periodeY = tc->perY;
			return true;
		};

		auto peindre = [&](NkImage &cible, const Shape &src) {
			// ── UNE IMAGE SE POSE (pas de remplissage, pas de trait) ───────────
			if (src.EstImage()) {
				Shape poseur;
				poseur.style = src.style;
				poseur.ix = src.ix;
				poseur.iy = src.iy;
				poseur.iw = src.iw;
				poseur.ih = src.ih;
				poseur.fit = src.fit;
				poseur.alignX = src.alignX;
				poseur.alignY = src.alignY;
				for (uint32 k = 0; k < src.xs.Size(); ++k) {
					float32 x = src.xs[k], y = src.ys[k];
					mView.Apply(x, y);
					poseur.xs.PushBack(x);
					poseur.ys.PushBack(y);
				}
				// les pixels sont REFERENCES, jamais copies ni deplaces : Rasterize()
				// reste const et rejouable a plusieurs tailles, comme son contrat le dit.
				RasterizeImage(cible, poseur, src.img, mView * src.ctm);
				return;
			}
			// Construit un Shape transforme localement (sans toucher la source).
			Shape local;
			local.style = src.style;
			local.xs.Reserve(src.xs.Size());
			local.ys.Reserve(src.ys.Size());
			for (uint32 k = 0; k < src.xs.Size(); ++k) {
				float32 x = src.xs[k], y = src.ys[k];
				mView.Apply(x, y);
				local.xs.PushBack(x);
				local.ys.PushBack(y);
			}
			for (uint32 k = 0; k < src.contourStart.Size(); ++k) {
				local.contourStart.PushBack(src.contourStart[k]);
				local.contourLen.PushBack(src.contourLen[k]);
			}
			// ── Rasterise le fill (couleur unie OU gradient) ───────────────────
			GradPaint fillGP;
			MotifPaint fillMP;
			bool hasFillGP = false, hasFillMP = false;
			if (src.fillRef[0]) {
				hasFillGP = BuildGradPaint(impl, src.fillRef, local, src.ctm, mView, fillGP);
				if (!hasFillGP)
					hasFillMP = motifPour(src.fillRef, local, fillMP);
			}
			const bool fillRefUnresolved = (src.fillRef[0] && !hasFillGP && !hasFillMP);
			const uint8 *masque = clipDe(src);
			if (!fillRefUnresolved)
				RasterizeShape(cible, local, hasFillGP ? &fillGP : nullptr, masque,
							   hasFillMP ? &fillMP : nullptr);

			// ── Rasterise le stroke si present ─────────────────────────────────
			// On construit une nouvelle Shape "ruban" autour des contours et on
			// la rasterise comme un fill avec la couleur stroke. strokeWidth est
			// en unites SVG (avant view->out scale) : on le convertit en pixels
			// via la moyenne des facteurs du mView (mView est diagonal en pratique).
			const bool hasStroke = (!src.style.stroke.none || src.strokeRef[0]) && src.style.strokeWidth > 0.f;
			if (hasStroke) {
				const float32 sx = std::sqrt(mView.a * mView.a + mView.b * mView.b);
				const float32 sy = std::sqrt(mView.c * mView.c + mView.d * mView.d);
				const float32 swPx = src.style.strokeWidth * ((sx + sy) * 0.5f);
				if (swPx >= 0.1f) {
					Shape strokeShape;
					strokeShape.style = src.style;
					// RasterizeShape regarde "fill" -> on copie stroke dedans.
					strokeShape.style.fill = src.style.stroke;
					strokeShape.style.fillOpacity = src.style.strokeOpacity;
					strokeShape.style.fillEvenOdd = false;
					// LES TIRETS D'ABORD : on coupe le chemin, puis on epaissit chaque
					// morceau -- l'inverse (effacer des bouts du ruban) ne saurait pas
					// donner ses terminaisons a chaque tiret.
					Shape tirets;
					const Shape *aEpaissir = &local;
					if (src.style.nDashes > 0) {
						const float32 echDash = (sx + sy) * 0.5f;
						DecouperEnTirets(local, src.style, echDash, tirets);
						aEpaissir = &tirets;
					}
					BuildStrokeShape(*aEpaissir, swPx * 0.5f, src.style.strokeLineCap, src.style.strokeLineJoin,
									 src.style.strokeMiterLimit, strokeShape);
					GradPaint strokeGP;
					bool hasStrokeGP = false;
					if (src.strokeRef[0])
						hasStrokeGP = BuildGradPaint(impl, src.strokeRef, strokeShape, src.ctm, mView, strokeGP);
					const bool strokeRefUnresolved = (src.strokeRef[0] && !hasStrokeGP);
					if (strokeShape.contourStart.Size() > 0 && !strokeRefUnresolved) {
						RasterizeShape(cible, strokeShape, hasStrokeGP ? &strokeGP : nullptr, masque);
					}
				}
			}
		};

		// ─────────────────────────────────────────────────────────────────────────
		// L'ORDRE DE PEINTURE : forme par forme, SAUF un groupe filtre, qui part
		// dans un calque. Les formes d'un meme <g filter> se suivent (parcours en
		// profondeur), et chaque OUVERTURE de groupe filtre a son propre numero
		// d'instance : deux freres qui portent le meme filtre font deux ombres, pas
		// une ombre sur leur union.
		// ─────────────────────────────────────────────────────────────────────────
		const float32 echX = std::sqrt(mView.a * mView.a + mView.b * mView.b);
		const float32 echY = std::sqrt(mView.c * mView.c + mView.d * mView.d);
		uint32 i = 0;
		while (i < impl->shapes.Size()) {
			const int32 inst = impl->shapes[i].filterInst;
			if (inst == 0) {
				peindre(img, impl->shapes[i]);
				++i;
				continue;
			}
			uint32 j = i;
			while (j < impl->shapes.Size() && impl->shapes[j].filterInst == inst)
				++j;
			const Filtre *fl = TrouverFiltre(impl, impl->shapes[i].filterRef);
			NkImage calque;
			if (fl)
				calque = NkImage::Alloc(outW, outH, NkImagePixelFormat::NK_RGBA32);
			if (!fl || !calque.IsValid()) {
				// filtre inconnu, sans feDropShadow, ou memoire refusee : on peint le
				// groupe TEL QUEL. Perdre l'ombre est un moindre mal ; perdre le
				// groupe n'en serait pas un.
				for (uint32 k = i; k < j; ++k)
					peindre(img, impl->shapes[k]);
				i = j;
				continue;
			}
			for (uint32 k = i; k < j; ++k)
				peindre(calque, impl->shapes[k]);
			// LE GRAPHE remplace le groupe par son resultat -- l'ombre n'est plus un
			// cas particulier, c'est une primitive parmi d'autres.
			NkImage filtre = EvaluerFiltre(*fl, calque, echX, echY);
			ComposerCalque(img, filtre.IsValid() ? filtre : calque);
			i = j;
		}
		return img;
	}

	int32 NkSVGImage::NaturalWidth() const noexcept {
		const SVGImageImpl *impl = (const SVGImageImpl *)mImpl;
		return impl ? (int32)impl->svgW : 0;
	}

	int32 NkSVGImage::NaturalHeight() const noexcept {
		const SVGImageImpl *impl = (const SVGImageImpl *)mImpl;
		return impl ? (int32)impl->svgH : 0;
	}

	int32 NkSVGImage::SkippedCount() const noexcept {
		const SVGImageImpl *impl = (const SVGImageImpl *)mImpl;
		return impl ? impl->skips.nb : 0;
	}

	const char *NkSVGImage::SkippedAt(int32 idx) const noexcept {
		const SVGImageImpl *impl = (const SVGImageImpl *)mImpl;
		if (!impl || idx < 0 || idx >= impl->skips.nb)
			return nullptr;
		return impl->skips.noms[idx];
	}

	int32 NkSVGImage::ShapeCount() const noexcept {
		const SVGImageImpl *impl = (const SVGImageImpl *)mImpl;
		return impl ? (int32)impl->shapes.Size() : 0;
	}

	NkSVGShapeView NkSVGImage::GetShape(int32 idx) const noexcept {
		NkSVGShapeView v;
		const SVGImageImpl *impl = (const SVGImageImpl *)mImpl;
		if (!impl || idx < 0 || (uint32)idx >= impl->shapes.Size())
			return v;
		v.mShapePtr = &impl->shapes[(uint32)idx];
		return v;
	}

	bool NkSVGImage::TriangulateAll(NkVector<float32> &outXs, NkVector<float32> &outYs, NkVector<uint32> &outIndices,
									NkVector<uint32> &outTriColors) const noexcept {
		const SVGImageImpl *impl = (const SVGImageImpl *)mImpl;
		if (!impl)
			return false;
		outXs.Clear();
		outYs.Clear();
		outIndices.Clear();
		outTriColors.Clear();
		for (uint32 i = 0; i < impl->shapes.Size(); ++i) {
			const Shape &sh = impl->shapes[i];
			if (sh.style.fill.none)
				continue;
			// Triangulate cette shape : chaque triangle produit hérite la couleur fill.
			NkSVGShapeView v;
			v.mShapePtr = &sh;
			const uint32 baseIdx = (uint32)outXs.Size();
			const int32 trisBefore = (int32)(outIndices.Size() / 3);
			v.Triangulate(outXs, outYs, outIndices, baseIdx);
			const int32 trisAfter = (int32)(outIndices.Size() / 3);
			// Couleur fill premultipliee par opacity * fillOpacity.
			const float32 a = (float32)sh.style.fill.a / 255.f * sh.style.opacity * sh.style.fillOpacity;
			const uint32 col = PackRGBA(sh.style.fill.r, sh.style.fill.g, sh.style.fill.b, (uint8)(a * 255.f));
			for (int32 k = trisBefore; k < trisAfter; ++k) {
				outTriColors.PushBack(col);
			}
		}
		return true;
	}

	void NkSVGImage::Free() noexcept {
		SVGImageImpl *impl = (SVGImageImpl *)mImpl;
		if (impl) {
			impl->~SVGImageImpl();
			NkFree(impl);
			mImpl = nullptr;
		}
		this->~NkSVGImage();
		NkFree(this);
	}

	// ═════════════════════════════════════════════════════════════════════════════
	// SECTION 11 — NkSVGCodec API publique
	// ═════════════════════════════════════════════════════════════════════════════

	NkImage NkSVGCodec::Decode(const uint8 *data, usize size, int32 outW, int32 outH) noexcept {
		// Pipe-through : on parse en NkSVGImage puis on rasterise. Une seule passe
		// si l'appelant n'a pas besoin de conserver la representation vectorielle.
		NkSVGImage *svg = NkSVGImage::LoadFromMemory(data, size);
		if (!svg)
			return NkImage();
		// NkSVGImage reste une ressource tas explicite : on la libere ici.
		// Seule l'image rasterisee est un type valeur, rendue par valeur.
		NkImage img = svg->Rasterize(outW, outH);
		svg->Free();
		return img;
	}

	void NkSVGCodec::SetDefaultGlyphSource(NkIGlyphSource *source) noexcept {
		gSourceParDefaut = source;
	}

	NkIGlyphSource *NkSVGCodec::GetDefaultGlyphSource() noexcept {
		return gSourceParDefaut;
	}

	NkImage NkSVGCodec::Decode(const uint8 *data, usize size, int32 outW, int32 outH, const char *baseDir,
							   NkIGlyphSource *glyphes) noexcept {
		NkSVGImage *svg = NkSVGImage::LoadFromMemory(data, size, baseDir, glyphes);
		if (!svg)
			return NkImage();
		NkImage img = svg->Rasterize(outW, outH);
		svg->Free();
		return img;
	}

	NkImage NkSVGCodec::DecodeFromFile(const char *path, int32 outW, int32 outH) noexcept {
		NkSVGImage *svg = NkSVGImage::LoadFromFile(path);
		if (!svg)
			return NkImage();
		NkImage img = svg->Rasterize(outW, outH);
		svg->Free();
		return img;
	}

	// L'encodage SVG (NkImage -> SVG enrobant un PNG base64) n'est pas prioritaire.
	// On le laisse stub pour l'instant ; a implementer si besoin.
	bool NkSVGCodec::Encode(const NkImage &, uint8 *&, usize &) noexcept {
		return false;
	}

	bool NkSVGCodec::EncodeToFile(const NkImage &, const char *) noexcept {
		return false;
	}

} // namespace nkentseu
