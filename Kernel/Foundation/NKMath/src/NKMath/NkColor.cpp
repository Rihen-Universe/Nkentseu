// -----------------------------------------------------------------------------
// FICHIER: NKMath\NkColor.cpp
// DESCRIPTION: Implémentation des fonctions non-inline de NkColor et NkColorF
// AUTEUR: Rihen
// DATE: 2026-04-26
// VERSION: 2.1.0
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

// -------------------------------------------------------------------------
// PRECOMPILED HEADER (requis pour tous les fichiers .cpp du projet)
// -------------------------------------------------------------------------
#include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES DU MODULE
// -------------------------------------------------------------------------
#include "NKMath/NkColor.h"
#include "NKMath/NkRandom.h"
#include "NKContainers/String/NkString.h"
#include <ostream>

// -------------------------------------------------------------------------
// ESPACE DE NOMS PRINCIPAL
// -------------------------------------------------------------------------

namespace nkentseu {

	// ====================================================================
	// NAMESPACE : MATH (IMPLÉMENTATIONS NON-INLINE)
	// ====================================================================

	namespace math {

		// ====================================================================
		// DÉFINITIONS DES CONSTANTES DE COULEUR NOMMÉES
		// ====================================================================

		const NkColor NkColor::White = NkColor::RGBf(1.00f, 1.00f, 1.00f);
		const NkColor NkColor::Black = NkColor::RGBf(0.00f, 0.00f, 0.00f);
		const NkColor NkColor::Transparent = NkColor::RGBAf(0.00f, 0.00f, 0.00f, 0.0f);
		const NkColor NkColor::Gray = NkColor::RGBf(0.50f, 0.50f, 0.50f);
		const NkColor NkColor::Red = NkColor::RGBf(1.00f, 0.00f, 0.00f);
		const NkColor NkColor::Green = NkColor::RGBf(0.00f, 1.00f, 0.00f);
		const NkColor NkColor::Blue = NkColor::RGBf(0.00f, 0.00f, 1.00f);
		const NkColor NkColor::Yellow = NkColor::RGBf(1.00f, 1.00f, 0.00f);
		const NkColor NkColor::Cyan = NkColor::RGBf(0.00f, 1.00f, 1.00f);
		const NkColor NkColor::Magenta = NkColor::RGBf(1.00f, 0.00f, 1.00f);
		const NkColor NkColor::Orange = NkColor::RGBf(1.00f, 0.50f, 0.00f);
		const NkColor NkColor::Pink = NkColor::RGBf(1.00f, 0.75f, 0.80f);
		const NkColor NkColor::Purple = NkColor::RGBf(0.50f, 0.00f, 0.50f);
		const NkColor NkColor::DarkGray = NkColor::RGBf(0.10f, 0.10f, 0.10f);
		const NkColor NkColor::Lime = NkColor::RGBf(0.75f, 1.00f, 0.00f);
		const NkColor NkColor::Teal = NkColor::RGBf(0.00f, 0.50f, 0.50f);
		const NkColor NkColor::Brown = NkColor::RGBf(0.65f, 0.16f, 0.16f);
		const NkColor NkColor::SaddleBrown = NkColor::RGBf(0.55f, 0.27f, 0.07f);
		const NkColor NkColor::Olive = NkColor::RGBf(0.50f, 0.50f, 0.00f);
		const NkColor NkColor::Maroon = NkColor::RGBf(0.50f, 0.00f, 0.00f);
		const NkColor NkColor::Navy = NkColor::RGBf(0.00f, 0.00f, 0.50f);
		const NkColor NkColor::Indigo = NkColor::RGBf(0.29f, 0.00f, 0.51f);
		const NkColor NkColor::Turquoise = NkColor::RGBf(0.25f, 0.88f, 0.82f);
		const NkColor NkColor::Silver = NkColor::RGBf(0.75f, 0.75f, 0.75f);
		const NkColor NkColor::Gold = NkColor::RGBf(1.00f, 0.84f, 0.00f);
		const NkColor NkColor::SkyBlue = NkColor::RGBf(0.53f, 0.81f, 0.98f);
		const NkColor NkColor::ForestGreen = NkColor::RGBf(0.13f, 0.55f, 0.13f);
		const NkColor NkColor::SteelBlue = NkColor::RGBf(0.27f, 0.51f, 0.71f);
		const NkColor NkColor::DarkSlateGray = NkColor::RGBf(0.18f, 0.31f, 0.31f);
		const NkColor NkColor::Chocolate = NkColor::RGBf(0.82f, 0.41f, 0.12f);
		const NkColor NkColor::HotPink = NkColor::RGBf(1.00f, 0.41f, 0.71f);
		const NkColor NkColor::SlateBlue = NkColor::RGBf(0.42f, 0.35f, 0.80f);
		const NkColor NkColor::RoyalBlue = NkColor::RGBf(0.25f, 0.41f, 0.88f);
		const NkColor NkColor::Tomato = NkColor::RGBf(1.00f, 0.39f, 0.28f);
		const NkColor NkColor::MediumSeaGreen = NkColor::RGBf(0.24f, 0.70f, 0.44f);
		const NkColor NkColor::DarkOrange = NkColor::RGBf(1.00f, 0.55f, 0.00f);
		const NkColor NkColor::MediumPurple = NkColor::RGBf(0.58f, 0.44f, 0.86f);
		const NkColor NkColor::CornflowerBlue = NkColor::RGBf(0.39f, 0.58f, 0.93f);
		const NkColor NkColor::DarkGoldenrod = NkColor::RGBf(0.72f, 0.53f, 0.04f);
		const NkColor NkColor::DodgerBlue = NkColor::RGBf(0.12f, 0.56f, 1.00f);
		const NkColor NkColor::MediumVioletRed = NkColor::RGBf(0.78f, 0.08f, 0.52f);
		const NkColor NkColor::Peru = NkColor::RGBf(0.80f, 0.52f, 0.25f);
		const NkColor NkColor::MediumAquamarine = NkColor::RGBf(0.40f, 0.80f, 0.67f);
		const NkColor NkColor::DarkTurquoise = NkColor::RGBf(0.00f, 0.81f, 0.82f);
		const NkColor NkColor::MediumSlateBlue = NkColor::RGBf(0.48f, 0.41f, 0.93f);
		const NkColor NkColor::YellowGreen = NkColor::RGBf(0.60f, 0.80f, 0.20f);
		const NkColor NkColor::LightCoral = NkColor::RGBf(0.94f, 0.50f, 0.50f);
		const NkColor NkColor::DarkSlateBlue = NkColor::RGBf(0.28f, 0.24f, 0.55f);
		const NkColor NkColor::DarkOliveGreen = NkColor::RGBf(0.33f, 0.42f, 0.18f);
		const NkColor NkColor::Firebrick = NkColor::RGBf(0.70f, 0.13f, 0.13f);
		const NkColor NkColor::MediumOrchid = NkColor::RGBf(0.73f, 0.33f, 0.83f);
		const NkColor NkColor::RosyBrown = NkColor::RGBf(0.74f, 0.56f, 0.56f);
		const NkColor NkColor::DarkCyan = NkColor::RGBf(0.00f, 0.55f, 0.55f);
		const NkColor NkColor::CadetBlue = NkColor::RGBf(0.37f, 0.62f, 0.63f);
		const NkColor NkColor::PaleVioletRed = NkColor::RGBf(0.86f, 0.44f, 0.58f);
		const NkColor NkColor::DeepPink = NkColor::RGBf(1.00f, 0.08f, 0.58f);
		const NkColor NkColor::LawnGreen = NkColor::RGBf(0.49f, 0.99f, 0.00f);
		const NkColor NkColor::MediumSpringGreen = NkColor::RGBf(0.00f, 0.98f, 0.60f);
		const NkColor NkColor::MediumTurquoise = NkColor::RGBf(0.28f, 0.82f, 0.80f);
		const NkColor NkColor::PaleGreen = NkColor::RGBf(0.60f, 0.98f, 0.60f);
		const NkColor NkColor::DarkKhaki = NkColor::RGBf(0.74f, 0.72f, 0.42f);
		const NkColor NkColor::MediumBlue = NkColor::RGBf(0.00f, 0.00f, 0.80f);
		const NkColor NkColor::MidnightBlue = NkColor(25, 25, 112);
		const NkColor NkColor::NavajoWhite = NkColor::RGBf(1.00f, 0.87f, 0.68f);
		const NkColor NkColor::DarkSalmon = NkColor::RGBf(0.91f, 0.59f, 0.48f);
		const NkColor NkColor::MediumCoral = NkColor::RGBf(0.81f, 0.36f, 0.36f);
		const NkColor NkColor::DefaultBackground = NkColor(0, 162, 232);
		const NkColor NkColor::CharcoalBlack = NkColor(31, 31, 31);
		const NkColor NkColor::SlateGray = NkColor(46, 46, 46);
		const NkColor NkColor::SkyBlueRef = NkColor(50, 130, 246);
		const NkColor NkColor::DuckBlue = NkColor(0, 162, 232);

		// ====================================================================
		// CONVERSION HSV → RGB (NKCOLOR)
		// ====================================================================

		/**
		 * @brief Convertit une structure HSV en couleur NkColor (algorithme standard)
		 * @param hsv Structure NkHSV avec teinte/saturation/valeur
		 * @return Couleur NkColor équivalente en RGB
		 * @note Implémentation de l'algorithme standard HSV→RGB
		 * @note Gère correctement les cas limites (saturation=0, valeur=0)
		 */
		NkColor NkColor::FromHSV(const NkHSV &hsv) noexcept {
			float32 h = hsv.hue / 360.0f;
			float32 s = hsv.saturation / 100.0f;
			float32 v = hsv.value / 100.0f;

			int32 i = static_cast<int32>(h * 6.0f);
			float32 f = h * 6.0f - static_cast<float32>(i);
			float32 p = v * (1.0f - s);
			float32 q = v * (1.0f - f * s);
			float32 t = v * (1.0f - (1.0f - f) * s);

			float32 cr = 0.0f, cg = 0.0f, cb = 0.0f;
			switch (i % 6) {
				case 0:
					cr = v;
					cg = t;
					cb = p;
					break;
				case 1:
					cr = q;
					cg = v;
					cb = p;
					break;
				case 2:
					cr = p;
					cg = v;
					cb = t;
					break;
				case 3:
					cr = p;
					cg = q;
					cb = v;
					break;
				case 4:
					cr = t;
					cg = p;
					cb = v;
					break;
				case 5:
					cr = v;
					cg = p;
					cb = q;
					break;
			}

			return {static_cast<uint8>(cr * 255.0f), static_cast<uint8>(cg * 255.0f), static_cast<uint8>(cb * 255.0f),
					255};
		}

		// ====================================================================
		// CONVERSION HSV → RGB (NKCOLORF)
		// ====================================================================

		/**
		 * @brief Convertit une structure HSV en couleur NkColorF (algorithme standard)
		 * @param hsv Structure NkHSV avec teinte/saturation/valeur
		 * @return Couleur NkColorF équivalente en RGB flottant
		 * @note Implémentation de l'algorithme standard HSV→RGB
		 * @note Plus précis que FromHSV() car pas de quantification 8-bit intermédiaire
		 */
		NkColorF NkColor::FromHSVf(const NkHSV &hsv) noexcept {
			float32 h = hsv.hue / 360.0f;
			float32 s = hsv.saturation / 100.0f;
			float32 v = hsv.value / 100.0f;

			int32 i = static_cast<int32>(h * 6.0f);
			float32 f = h * 6.0f - static_cast<float32>(i);
			float32 p = v * (1.0f - s);
			float32 q = v * (1.0f - f * s);
			float32 t = v * (1.0f - (1.0f - f) * s);

			float32 cr = 0.0f, cg = 0.0f, cb = 0.0f;
			switch (i % 6) {
				case 0:
					cr = v;
					cg = t;
					cb = p;
					break;
				case 1:
					cr = q;
					cg = v;
					cb = p;
					break;
				case 2:
					cr = p;
					cg = v;
					cb = t;
					break;
				case 3:
					cr = p;
					cg = q;
					cb = v;
					break;
				case 4:
					cr = t;
					cg = p;
					cb = v;
					break;
				case 5:
					cr = v;
					cg = p;
					cb = q;
					break;
			}

			return {cr, cg, cb, 1.0f};
		}

		// ====================================================================
		// CONVERSION RGB → HSV (NKCOLOR)
		// ====================================================================

		/**
		 * @brief Convertit une couleur NkColor en structure HSV (algorithme standard)
		 * @return Structure NkHSV équivalente
		 * @note Implémentation de l'algorithme standard RGB→HSV
		 * @note Précision limitée par la quantification 8-bit des composantes RGB
		 */
		NkHSV NkColor::ToHSV() const noexcept {
			float32 fr = static_cast<float32>(r) / 255.0f;
			float32 fg = static_cast<float32>(g) / 255.0f;
			float32 fb = static_cast<float32>(b) / 255.0f;

			float32 maxC = NkMax(NkMax(fr, fg), fb);
			float32 minC = NkMin(NkMin(fr, fg), fb);
			float32 h = 0.0f, s = 0.0f, v = maxC;

			if (maxC > 0.0f) {
				float32 d = maxC - minC;
				s = d / maxC;
				if (maxC == fr) {
					h = (fg - fb) / d + (fg < fb ? 6.0f : 0.0f);
				} else if (maxC == fg) {
					h = (fb - fr) / d + 2.0f;
				} else {
					h = (fr - fg) / d + 4.0f;
				}
				h /= 6.0f;
			}

			return {h * 360.0f, s * 100.0f, v * 100.0f};
		}

		// ====================================================================
		// CONVERSION RGB → HSV (NKCOLORF)
		// ====================================================================

		/**
		 * @brief Convertit une couleur NkColorF en structure HSV (algorithme standard)
		 * @return Structure NkHSV équivalente
		 * @note Implémentation de l'algorithme standard RGB→HSV
		 * @note Plus précis que ToHSV() car pas de quantification 8-bit
		 */
		NkHSV NkColor::ToHSVf() const noexcept {
			// Conversion via NkColorF interne pour réutiliser l'algorithme
			NkColorF self(r * kOneOver255, g * kOneOver255, b * kOneOver255, a * kOneOver255);
			return self.ToHSVf();
		}

		/**
		 * @brief Convertit une couleur NkColorF en structure HSV (implémentation directe)
		 * @return Structure NkHSV équivalente
		 * @note Implémentation de l'algorithme standard RGB→HSV pour flottants
		 */
		NkHSV NkColorF::ToHSVf() const noexcept {
			float32 maxC = NkMax(NkMax(r, g), b);
			float32 minC = NkMin(NkMin(r, g), b);
			float32 h = 0.0f, s = 0.0f, v = maxC;

			if (maxC > 0.0f) {
				float32 d = maxC - minC;
				s = d / maxC;
				if (maxC == r) {
					h = (g - b) / d + (g < b ? 6.0f : 0.0f);
				} else if (maxC == g) {
					h = (b - r) / d + 2.0f;
				} else {
					h = (r - g) / d + 4.0f;
				}
				h /= 6.0f;
			}

			return {h * 360.0f, s * 100.0f, v * 100.0f};
		}

		// ====================================================================
		// COULEURS ALÉATOIRES
		// ====================================================================

		/**
		 * @brief Génère une couleur RGB aléatoire opaque
		 * @return Nouvelle couleur avec r,g,b aléatoires ∈ [0,255], a=255
		 * @note Utilise NkRandom::Instance() pour la génération
		 */
		NkColor NkColor::RandomRGB() noexcept {
			return {static_cast<uint8>(NkRandom::Instance().NextUInt32(256u)),
					static_cast<uint8>(NkRandom::Instance().NextUInt32(256u)),
					static_cast<uint8>(NkRandom::Instance().NextUInt32(256u))};
		}

		/**
		 * @brief Génère une couleur RGBA aléatoire
		 * @return Nouvelle couleur avec r,g,b,a aléatoires ∈ [0,255]
		 * @note Utilise NkRandom::Instance() pour la génération
		 */
		NkColor NkColor::RandomRGBA() noexcept {
			return {static_cast<uint8>(NkRandom::Instance().NextUInt32(256u)),
					static_cast<uint8>(NkRandom::Instance().NextUInt32(256u)),
					static_cast<uint8>(NkRandom::Instance().NextUInt32(256u)),
					static_cast<uint8>(NkRandom::Instance().NextUInt32(256u))};
		}

		// ====================================================================
		// LOOKUP PAR NOM (RECHERCHE LINÉAIRE)
		// ====================================================================

		/**
		 * @brief Trouve une couleur par son nom textuel (recherche linéaire)
		 * @param name Nom de la couleur à rechercher
		 * @return Référence const vers la couleur correspondante, ou Black si non trouvée
		 * @note Recherche linéaire dans ~60 entrées : O(n) mais n petit et usage rare
		 * @note Optimisé pour les noms les plus courants en premier (Transparent, Black, White)
		 */
		const NkColor &NkColor::FromName(const NkString &name) noexcept {
			if (name == "Transparent") {
				return Transparent;
			}
			if (name == "Black") {
				return Black;
			}
			if (name == "White") {
				return White;
			}
			if (name == "Red") {
				return Red;
			}
			if (name == "Green") {
				return Green;
			}
			if (name == "Blue") {
				return Blue;
			}
			if (name == "Yellow") {
				return Yellow;
			}
			if (name == "Cyan") {
				return Cyan;
			}
			if (name == "Magenta") {
				return Magenta;
			}
			if (name == "Orange") {
				return Orange;
			}
			if (name == "Pink") {
				return Pink;
			}
			if (name == "Purple") {
				return Purple;
			}
			if (name == "Gray") {
				return Gray;
			}
			if (name == "DarkGray") {
				return DarkGray;
			}
			if (name == "Lime") {
				return Lime;
			}
			if (name == "Teal") {
				return Teal;
			}
			if (name == "Brown") {
				return Brown;
			}
			if (name == "SaddleBrown") {
				return SaddleBrown;
			}
			if (name == "Olive") {
				return Olive;
			}
			if (name == "Maroon") {
				return Maroon;
			}
			if (name == "Navy") {
				return Navy;
			}
			if (name == "Indigo") {
				return Indigo;
			}
			if (name == "Turquoise") {
				return Turquoise;
			}
			if (name == "Silver") {
				return Silver;
			}
			if (name == "Gold") {
				return Gold;
			}
			if (name == "SkyBlue") {
				return SkyBlue;
			}
			if (name == "ForestGreen") {
				return ForestGreen;
			}
			if (name == "SteelBlue") {
				return SteelBlue;
			}
			if (name == "DarkSlateGray") {
				return DarkSlateGray;
			}
			if (name == "Chocolate") {
				return Chocolate;
			}
			if (name == "HotPink") {
				return HotPink;
			}
			if (name == "SlateBlue") {
				return SlateBlue;
			}
			if (name == "RoyalBlue") {
				return RoyalBlue;
			}
			if (name == "Tomato") {
				return Tomato;
			}
			if (name == "MediumSeaGreen") {
				return MediumSeaGreen;
			}
			if (name == "DarkOrange") {
				return DarkOrange;
			}
			if (name == "MediumPurple") {
				return MediumPurple;
			}
			if (name == "CornflowerBlue") {
				return CornflowerBlue;
			}
			if (name == "DarkGoldenrod") {
				return DarkGoldenrod;
			}
			if (name == "DodgerBlue") {
				return DodgerBlue;
			}
			if (name == "MediumVioletRed") {
				return MediumVioletRed;
			}
			if (name == "Peru") {
				return Peru;
			}
			if (name == "MediumAquamarine") {
				return MediumAquamarine;
			}
			if (name == "DarkTurquoise") {
				return DarkTurquoise;
			}
			if (name == "MediumSlateBlue") {
				return MediumSlateBlue;
			}
			if (name == "YellowGreen") {
				return YellowGreen;
			}
			if (name == "LightCoral") {
				return LightCoral;
			}
			if (name == "DarkSlateBlue") {
				return DarkSlateBlue;
			}
			if (name == "DarkOliveGreen") {
				return DarkOliveGreen;
			}
			if (name == "Firebrick") {
				return Firebrick;
			}
			if (name == "MediumOrchid") {
				return MediumOrchid;
			}
			if (name == "RosyBrown") {
				return RosyBrown;
			}
			if (name == "DarkCyan") {
				return DarkCyan;
			}
			if (name == "CadetBlue") {
				return CadetBlue;
			}
			if (name == "PaleVioletRed") {
				return PaleVioletRed;
			}
			if (name == "DeepPink") {
				return DeepPink;
			}
			if (name == "LawnGreen") {
				return LawnGreen;
			}
			if (name == "MediumSpringGreen") {
				return MediumSpringGreen;
			}
			if (name == "MediumTurquoise") {
				return MediumTurquoise;
			}
			if (name == "PaleGreen") {
				return PaleGreen;
			}
			if (name == "DarkKhaki") {
				return DarkKhaki;
			}
			if (name == "MediumBlue") {
				return MediumBlue;
			}
			if (name == "MidnightBlue") {
				return MidnightBlue;
			}
			if (name == "NavajoWhite") {
				return NavajoWhite;
			}
			if (name == "DarkSalmon") {
				return DarkSalmon;
			}
			if (name == "MediumCoral") {
				return MediumCoral;
			}
			if (name == "DefaultBackground") {
				return DefaultBackground;
			}
			if (name == "CharcoalBlack") {
				return CharcoalBlack;
			}
			if (name == "SlateGray") {
				return SlateGray;
			}
			if (name == "SkyBlueRef") {
				return SkyBlueRef;
			}
			if (name == "DuckBlue") {
				return DuckBlue;
			}

			// Couleur non trouvée → noir par défaut
			static const NkColor fallback;
			return fallback;
		}

		// ====================================================================
		// MÉTHODES DE REPRÉSENTATION TEXTE (NON-INLINE)
		// ====================================================================

		// -------------------------------------------------------------------------
		// NKCOLOR
		// -------------------------------------------------------------------------

		/**
		 * @brief Convertit la couleur en chaîne de caractères lisible
		 * @return NkString au format "(r, g, b, a)"
		 */
		NkString NkColor::ToString() const {
			return NkFormat("{0}", *this);
		}

		/**
		 * @brief Fonction libre pour conversion texte (ADL-friendly)
		 * @param c Couleur à convertir
		 * @return Même résultat que c.ToString()
		 */
		NkString ToString(const NkColor &c) {
			return c.ToString();
		}

		/**
		 * @brief Opérateur de flux pour sortie std::ostream
		 * @param os Flux de sortie
		 * @param c Couleur à écrire
		 * @return Référence vers os pour chaînage
		 */
		std::ostream &operator<<(std::ostream &os, const NkColor &c) {
			return os << c.ToString().CStr();
		}

		// -------------------------------------------------------------------------
		// NKCOLORF
		// -------------------------------------------------------------------------

		/**
		 * @brief Convertit la couleur flottante en chaîne de caractères lisible
		 * @return NkString au format "(r, g, b, a)" avec précision flottante
		 */
		NkString NkColorF::ToString() const {
			return NkFormat("{0}", *this);
		}

		/**
		 * @brief Fonction libre pour conversion texte (ADL-friendly)
		 * @param c Couleur flottante à convertir
		 * @return Même résultat que c.ToString()
		 */
		NkString ToString(const NkColorF &c) {
			return c.ToString();
		}

		/**
		 * @brief Opérateur de flux pour sortie std::ostream
		 * @param os Flux de sortie
		 * @param c Couleur flottante à écrire
		 * @return Référence vers os pour chaînage
		 */
		std::ostream &operator<<(std::ostream &os, const NkColorF &c) {
			return os << c.ToString().CStr();
		}

		// ====================================================================
		// ESPACES COLORIMÉTRIQUES ÉTENDUS (FUSION 2026-07-25)
		// ====================================================================
		// Portage des capacités de l'ancienne classe dupliquée `nkentseu::NkColor`
		// (Engine/Noge/src/Noge/Color/NkColorManager.h, supprimée) sur NkColorF.
		// Contrairement à l'original Noge (dont seuls FromSRGB/SRGBToLinear/
		// LinearToSRGB avaient un corps -- tout le reste était déclaré sans
		// implémentation, cf. NkColorManager.cpp historique), toutes les méthodes
		// ci-dessous sont réellement implémentées.

		namespace {

			// ---- HSL ----

			float32 Hue2Rgb(float32 p, float32 q, float32 t) noexcept {
				if (t < 0.0f) {
					t += 1.0f;
				}
				if (t > 1.0f) {
					t -= 1.0f;
				}
				if (t < 1.0f / 6.0f) {
					return p + (q - p) * 6.0f * t;
				}
				if (t < 0.5f) {
					return q;
				}
				if (t < 2.0f / 3.0f) {
					return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
				}
				return p;
			}

			// ---- sRGB linéaire <-> CIE XYZ (D65) ----

			void LinearRGBToXYZ(float32 r, float32 g, float32 b, float32 &x, float32 &y, float32 &z) noexcept {
				x = 0.4124564f * r + 0.3575761f * g + 0.1804375f * b;
				y = 0.2126729f * r + 0.7151522f * g + 0.0721750f * b;
				z = 0.0193339f * r + 0.1191920f * g + 0.9503041f * b;
			}

			void XYZToLinearRGB(float32 x, float32 y, float32 z, float32 &r, float32 &g, float32 &b) noexcept {
				r = 3.2404542f * x - 1.5371385f * y - 0.4985314f * z;
				g = -0.9692660f * x + 1.8760108f * y + 0.0415560f * z;
				b = 0.0556434f * x - 0.2040259f * y + 1.0572252f * z;
			}

			// ---- CIE XYZ <-> LAB (D65) ----

			float32 LabForwardF(float32 t) noexcept {
				constexpr float32 kEpsilon = 0.008856f;
				constexpr float32 kKappa = 903.3f;
				if (t > kEpsilon) {
					return NkCbrt(t);
				}
				return (kKappa * t + 16.0f) / 116.0f;
			}

			float32 LabInverseF(float32 t) noexcept {
				constexpr float32 kEpsilon = 0.008856f;
				constexpr float32 kKappa = 903.3f;
				float32 t3 = t * t * t;
				if (t3 > kEpsilon) {
					return t3;
				}
				return (116.0f * t - 16.0f) / kKappa;
			}

			void XYZToLAB(float32 x, float32 y, float32 z, float32 &L, float32 &A, float32 &B) noexcept {
				constexpr float32 kXn = 0.95047f;
				constexpr float32 kYn = 1.00000f;
				constexpr float32 kZn = 1.08883f;

				float32 fx = LabForwardF(x / kXn);
				float32 fy = LabForwardF(y / kYn);
				float32 fz = LabForwardF(z / kZn);

				L = 116.0f * fy - 16.0f;
				A = 500.0f * (fx - fy);
				B = 200.0f * (fy - fz);
			}

			void LABToXYZ(float32 L, float32 A, float32 B, float32 &x, float32 &y, float32 &z) noexcept {
				constexpr float32 kXn = 0.95047f;
				constexpr float32 kYn = 1.00000f;
				constexpr float32 kZn = 1.08883f;

				float32 fy = (L + 16.0f) / 116.0f;
				float32 fx = fy + A / 500.0f;
				float32 fz = fy - B / 200.0f;

				x = kXn * LabInverseF(fx);
				y = kYn * LabInverseF(fy);
				z = kZn * LabInverseF(fz);
			}

			// ---- sRGB linéaire <-> OKLab (Björn Ottosson) ----

			void LinearRGBToOKLab(float32 r, float32 g, float32 b, float32 &L, float32 &A, float32 &B) noexcept {
				float32 l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
				float32 m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
				float32 s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;

				float32 l_ = NkCbrt(l);
				float32 m_ = NkCbrt(m);
				float32 s_ = NkCbrt(s);

				L = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_;
				A = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_;
				B = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_;
			}

			void OKLabToLinearRGB(float32 L, float32 A, float32 B, float32 &r, float32 &g, float32 &b) noexcept {
				float32 l_ = L + 0.3963377774f * A + 0.2158037573f * B;
				float32 m_ = L - 0.1055613458f * A - 0.0638541728f * B;
				float32 s_ = L - 0.0894841775f * A - 1.2914855480f * B;

				float32 l = l_ * l_ * l_;
				float32 m = m_ * m_ * m_;
				float32 s = s_ * s_ * s_;

				r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
				g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
				b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
			}

			// ---- Modes de fusion (par canal) ----

			float32 OverlayChannel(float32 base, float32 blend) noexcept {
				if (base <= 0.5f) {
					return 2.0f * base * blend;
				}
				return 1.0f - 2.0f * (1.0f - base) * (1.0f - blend);
			}

			float32 HardLightChannel(float32 base, float32 blend) noexcept {
				if (blend <= 0.5f) {
					return 2.0f * base * blend;
				}
				return 1.0f - 2.0f * (1.0f - base) * (1.0f - blend);
			}

			float32 SoftLightChannel(float32 base, float32 blend) noexcept {
				if (blend <= 0.5f) {
					return base - (1.0f - 2.0f * blend) * base * (1.0f - base);
				}
				float32 d = base <= 0.25f ? ((16.0f * base - 12.0f) * base + 4.0f) * base : NkSqrt(base);
				return base + (2.0f * blend - 1.0f) * (d - base);
			}

		} // namespace (anonyme, helpers internes à ce fichier)

		// ---- Helpers privés NkColorF (sRGB <-> Linéaire, hex) ----

		float32 NkColorF::SRGBToLinearChannel(float32 x) noexcept {
			// IEC 61966-2-1 : segment linéaire sous 0.04045, gamma 2.4 au-dessus.
			if (x <= 0.04045f) {
				return x / 12.92f;
			}
			return NkPow((x + 0.055f) / 1.055f, 2.4f);
		}

		float32 NkColorF::LinearToSRGBChannel(float32 x) noexcept {
			// Inverse exact de SRGBToLinearChannel.
			if (x <= 0.0031308f) {
				return x * 12.92f;
			}
			return 1.055f * NkPow(x, 1.0f / 2.4f) - 0.055f;
		}

		int32 NkColorF::HexNibble(char c) noexcept {
			if (c >= '0' && c <= '9') {
				return static_cast<int32>(c - '0');
			}
			if (c >= 'a' && c <= 'f') {
				return static_cast<int32>(c - 'a') + 10;
			}
			if (c >= 'A' && c <= 'F') {
				return static_cast<int32>(c - 'A') + 10;
			}
			return 0;
		}

		// ---- sRGB <-> Linéaire ----

		NkColorF NkColorF::ToLinearRGB() const noexcept {
			return {SRGBToLinearChannel(r), SRGBToLinearChannel(g), SRGBToLinearChannel(b), a};
		}

		NkColorF NkColorF::FromLinearRGB(float32 r, float32 g, float32 b, float32 a) noexcept {
			return {LinearToSRGBChannel(r), LinearToSRGBChannel(g), LinearToSRGBChannel(b), a};
		}

		// ---- HSL ----

		NkColorF NkColorF::FromHSL(float32 h, float32 s, float32 l, float32 a) noexcept {
			float32 hh = h;
			while (hh < 0.0f) {
				hh += 360.0f;
			}
			while (hh >= 360.0f) {
				hh -= 360.0f;
			}
			hh /= 360.0f;

			float32 ss = NkClamp(s, 0.0f, 1.0f);
			float32 ll = NkClamp(l, 0.0f, 1.0f);

			if (ss <= 0.0f) {
				return {ll, ll, ll, a};
			}

			float32 q = ll < 0.5f ? ll * (1.0f + ss) : ll + ss - ll * ss;
			float32 p = 2.0f * ll - q;

			float32 rr = Hue2Rgb(p, q, hh + 1.0f / 3.0f);
			float32 gg = Hue2Rgb(p, q, hh);
			float32 bb = Hue2Rgb(p, q, hh - 1.0f / 3.0f);

			return {rr, gg, bb, a};
		}

		NkVector4f NkColorF::ToHSL() const noexcept {
			float32 maxC = NkMax(NkMax(r, g), b);
			float32 minC = NkMin(NkMin(r, g), b);
			float32 l = (maxC + minC) * 0.5f;
			float32 h = 0.0f;
			float32 s = 0.0f;

			if (maxC != minC) {
				float32 d = maxC - minC;
				s = l > 0.5f ? d / (2.0f - maxC - minC) : d / (maxC + minC);

				if (maxC == r) {
					h = (g - b) / d + (g < b ? 6.0f : 0.0f);
				} else if (maxC == g) {
					h = (b - r) / d + 2.0f;
				} else {
					h = (r - g) / d + 4.0f;
				}
				h *= 60.0f;
			}

			return {h, s, l, a};
		}

		// ---- HSV (variante [0,1], parité d'API Noge) ----

		NkColorF NkColorF::FromHSV(float32 h, float32 s, float32 v, float32 a) noexcept {
			NkHSV hsv(h, s * 100.0f, v * 100.0f);
			NkColorF result = NkColor::FromHSVf(hsv);
			result.a = a;
			return result;
		}

		NkVector4f NkColorF::ToHSV() const noexcept {
			NkHSV hsv = ToHSVf();
			return {hsv.hue, hsv.saturation * 0.01f, hsv.value * 0.01f, a};
		}

		// ---- CIE XYZ ----

		NkVector4f NkColorF::ToXYZ() const noexcept {
			float32 lr = SRGBToLinearChannel(r);
			float32 lg = SRGBToLinearChannel(g);
			float32 lb = SRGBToLinearChannel(b);

			float32 x = 0.0f;
			float32 y = 0.0f;
			float32 z = 0.0f;
			LinearRGBToXYZ(lr, lg, lb, x, y, z);

			return {x, y, z, a};
		}

		NkColorF NkColorF::FromXYZ(float32 x, float32 y, float32 z, float32 a) noexcept {
			float32 lr = 0.0f;
			float32 lg = 0.0f;
			float32 lb = 0.0f;
			XYZToLinearRGB(x, y, z, lr, lg, lb);

			return {LinearToSRGBChannel(lr), LinearToSRGBChannel(lg), LinearToSRGBChannel(lb), a};
		}

		// ---- LAB / LCH ----

		NkVector4f NkColorF::ToLAB() const noexcept {
			NkVector4f xyz = ToXYZ();

			float32 L = 0.0f;
			float32 A = 0.0f;
			float32 B = 0.0f;
			XYZToLAB(xyz.r, xyz.g, xyz.b, L, A, B);

			return {L, A, B, a};
		}

		NkColorF NkColorF::FromLAB(float32 L, float32 a, float32 b, float32 alpha) noexcept {
			float32 x = 0.0f;
			float32 y = 0.0f;
			float32 z = 0.0f;
			LABToXYZ(L, a, b, x, y, z);

			return FromXYZ(x, y, z, alpha);
		}

		NkVector4f NkColorF::ToLCH() const noexcept {
			NkVector4f lab = ToLAB();

			float32 c = NkSqrt(lab.g * lab.g + lab.b * lab.b);
			float32 h = NkDegreesFromRadians(NkAtan2(lab.b, lab.g));
			if (h < 0.0f) {
				h += 360.0f;
			}

			return {lab.r, c, h, a};
		}

		NkColorF NkColorF::FromLCH(float32 L, float32 C, float32 H, float32 a) noexcept {
			float32 hr = NkRadiansFromDegrees(H);
			float32 la = C * NkCos(hr);
			float32 lb = C * NkSin(hr);

			return FromLAB(L, la, lb, a);
		}

		// ---- OKLab / OKLch ----

		NkVector4f NkColorF::ToOKLab() const noexcept {
			float32 lr = SRGBToLinearChannel(r);
			float32 lg = SRGBToLinearChannel(g);
			float32 lb = SRGBToLinearChannel(b);

			float32 L = 0.0f;
			float32 A = 0.0f;
			float32 B = 0.0f;
			LinearRGBToOKLab(lr, lg, lb, L, A, B);

			return {L, A, B, a};
		}

		NkColorF NkColorF::FromOKLab(float32 L, float32 a, float32 b, float32 alpha) noexcept {
			float32 lr = 0.0f;
			float32 lg = 0.0f;
			float32 lb = 0.0f;
			OKLabToLinearRGB(L, a, b, lr, lg, lb);

			return {LinearToSRGBChannel(lr), LinearToSRGBChannel(lg), LinearToSRGBChannel(lb), alpha};
		}

		NkVector4f NkColorF::ToOKLch() const noexcept {
			NkVector4f lab = ToOKLab();

			float32 c = NkSqrt(lab.g * lab.g + lab.b * lab.b);
			float32 h = NkDegreesFromRadians(NkAtan2(lab.b, lab.g));
			if (h < 0.0f) {
				h += 360.0f;
			}

			return {lab.r, c, h, a};
		}

		NkColorF NkColorF::FromOKLch(float32 L, float32 C, float32 H, float32 a) noexcept {
			float32 hr = NkRadiansFromDegrees(H);
			float32 la = C * NkCos(hr);
			float32 lb = C * NkSin(hr);

			return FromOKLab(L, la, lb, a);
		}

		// ---- CMYK (conversion naïve, sans profil ICC) ----

		NkVector4f NkColorF::ToCMYK() const noexcept {
			float32 k = 1.0f - NkMax(NkMax(r, g), b);

			if (k >= 1.0f) {
				return {0.0f, 0.0f, 0.0f, k};
			}

			float32 invK = 1.0f - k;
			float32 c = (invK - r) / invK;
			float32 m = (invK - g) / invK;
			float32 y = (invK - b) / invK;

			return {c, m, y, k};
		}

		NkColorF NkColorF::FromCMYK(float32 c, float32 m, float32 y, float32 k, float32 a) noexcept {
			float32 invK = 1.0f - NkClamp(k, 0.0f, 1.0f);
			float32 rr = (1.0f - NkClamp(c, 0.0f, 1.0f)) * invK;
			float32 gg = (1.0f - NkClamp(m, 0.0f, 1.0f)) * invK;
			float32 bb = (1.0f - NkClamp(y, 0.0f, 1.0f)) * invK;

			return {rr, gg, bb, a};
		}

		// ---- Hexadécimal ----

		NkColorF NkColorF::FromHex(const char *hex) noexcept {
			if (hex == nullptr) {
				return NkColorF();
			}

			const char *p = hex;
			if (*p == '#') {
				++p;
			}

			int32 len = 0;
			while (p[len] != '\0') {
				++len;
			}

			float32 rr = 0.0f;
			float32 gg = 0.0f;
			float32 bb = 0.0f;
			float32 aa = 1.0f;

			if (len >= 6) {
				rr = static_cast<float32>((HexNibble(p[0]) << 4) | HexNibble(p[1])) * kOneOver255;
				gg = static_cast<float32>((HexNibble(p[2]) << 4) | HexNibble(p[3])) * kOneOver255;
				bb = static_cast<float32>((HexNibble(p[4]) << 4) | HexNibble(p[5])) * kOneOver255;
			}
			if (len >= 8) {
				aa = static_cast<float32>((HexNibble(p[6]) << 4) | HexNibble(p[7])) * kOneOver255;
			}

			return {rr, gg, bb, aa};
		}

		NkString NkColorF::ToHexString(bool withAlpha) const noexcept {
			int32 ir = static_cast<int32>(NkClamp(r, 0.0f, 1.0f) * k255);
			int32 ig = static_cast<int32>(NkClamp(g, 0.0f, 1.0f) * k255);
			int32 ib = static_cast<int32>(NkClamp(b, 0.0f, 1.0f) * k255);

			if (withAlpha) {
				int32 ia = static_cast<int32>(NkClamp(a, 0.0f, 1.0f) * k255);
				return NkFormat("#%02X%02X%02X%02X", ir, ig, ib, ia);
			}
			return NkFormat("#%02X%02X%02X", ir, ig, ib);
		}

		// ---- U8 (parité d'API Noge) ----

		NkColorF NkColorF::FromU8(uint8 r, uint8 g, uint8 b, uint8 a) noexcept {
			return {static_cast<float32>(r) * kOneOver255, static_cast<float32>(g) * kOneOver255,
					static_cast<float32>(b) * kOneOver255, static_cast<float32>(a) * kOneOver255};
		}

		NkVector4f NkColorF::ToU8() const noexcept {
			return {NkClamp(r, 0.0f, 1.0f) * k255, NkClamp(g, 0.0f, 1.0f) * k255, NkClamp(b, 0.0f, 1.0f) * k255,
					NkClamp(a, 0.0f, 1.0f) * k255};
		}

		// ---- Ajustements (via HSL) ----

		NkColorF NkColorF::WithHue(float32 h) const noexcept {
			NkVector4f hsl = ToHSL();
			return FromHSL(h, hsl.g, hsl.b, a);
		}

		NkColorF NkColorF::WithSaturation(float32 s) const noexcept {
			NkVector4f hsl = ToHSL();
			return FromHSL(hsl.r, NkClamp(s, 0.0f, 1.0f), hsl.b, a);
		}

		NkColorF NkColorF::WithLightness(float32 l) const noexcept {
			NkVector4f hsl = ToHSL();
			return FromHSL(hsl.r, hsl.g, NkClamp(l, 0.0f, 1.0f), a);
		}

		NkColorF NkColorF::WithValue(float32 v) const noexcept {
			NkVector4f hsv = ToHSV();
			return FromHSV(hsv.r, hsv.g, NkClamp(v, 0.0f, 1.0f), a);
		}

		NkColorF NkColorF::Saturate(float32 amount) const noexcept {
			NkVector4f hsl = ToHSL();
			return FromHSL(hsl.r, NkClamp(hsl.g + amount, 0.0f, 1.0f), hsl.b, a);
		}

		NkColorF NkColorF::Desaturate(float32 amount) const noexcept {
			return Saturate(-amount);
		}

		NkColorF NkColorF::Complement() const noexcept {
			NkVector4f hsl = ToHSL();
			float32 h = hsl.r + 180.0f;
			if (h >= 360.0f) {
				h -= 360.0f;
			}
			return FromHSL(h, hsl.g, hsl.b, a);
		}

		// ---- Métriques perceptuelles ----

		float32 NkColorF::Luminance() const noexcept {
			float32 lr = SRGBToLinearChannel(r);
			float32 lg = SRGBToLinearChannel(g);
			float32 lb = SRGBToLinearChannel(b);
			return 0.2126f * lr + 0.7152f * lg + 0.0722f * lb;
		}

		float32 NkColorF::DeltaE(const NkColorF &other) const noexcept {
			NkVector4f lab1 = ToLAB();
			NkVector4f lab2 = other.ToLAB();

			float32 dL = lab1.r - lab2.r;
			float32 dA = lab1.g - lab2.g;
			float32 dB = lab1.b - lab2.b;

			return NkSqrt(dL * dL + dA * dA + dB * dB);
		}

		float32 NkColorF::DeltaE2000(const NkColorF &other) const noexcept {
			NkVector4f lab1 = ToLAB();
			NkVector4f lab2 = other.ToLAB();

			float32 L1 = lab1.r;
			float32 a1 = lab1.g;
			float32 b1 = lab1.b;
			float32 L2 = lab2.r;
			float32 a2 = lab2.g;
			float32 b2 = lab2.b;

			float32 c1 = NkSqrt(a1 * a1 + b1 * b1);
			float32 c2 = NkSqrt(a2 * a2 + b2 * b2);
			float32 avgC = (c1 + c2) * 0.5f;

			constexpr float32 k25Pow7 = 6103515625.0f; // 25^7
			float32 avgC7 = NkPow(avgC, 7.0f);
			float32 g = 0.5f * (1.0f - NkSqrt(avgC7 / (avgC7 + k25Pow7)));

			float32 a1p = a1 * (1.0f + g);
			float32 a2p = a2 * (1.0f + g);

			float32 c1p = NkSqrt(a1p * a1p + b1 * b1);
			float32 c2p = NkSqrt(a2p * a2p + b2 * b2);

			float32 h1p = (a1p == 0.0f && b1 == 0.0f) ? 0.0f : NkDegreesFromRadians(NkAtan2(b1, a1p));
			if (h1p < 0.0f) {
				h1p += 360.0f;
			}
			float32 h2p = (a2p == 0.0f && b2 == 0.0f) ? 0.0f : NkDegreesFromRadians(NkAtan2(b2, a2p));
			if (h2p < 0.0f) {
				h2p += 360.0f;
			}

			float32 deltaLp = L2 - L1;
			float32 deltaCp = c2p - c1p;

			float32 cp1cp2 = c1p * c2p;

			float32 deltahp = 0.0f;
			if (cp1cp2 != 0.0f) {
				deltahp = h2p - h1p;
				if (deltahp > 180.0f) {
					deltahp -= 360.0f;
				} else if (deltahp < -180.0f) {
					deltahp += 360.0f;
				}
			}
			float32 deltaHp = 2.0f * NkSqrt(cp1cp2) * NkSin(NkRadiansFromDegrees(deltahp) * 0.5f);

			float32 avgLp = (L1 + L2) * 0.5f;
			float32 avgCp = (c1p + c2p) * 0.5f;

			float32 avghp = 0.0f;
			if (cp1cp2 == 0.0f) {
				avghp = h1p + h2p;
			} else if (NkFabs(h1p - h2p) > 180.0f) {
				avghp = ((h1p + h2p) < 360.0f) ? (h1p + h2p + 360.0f) * 0.5f : (h1p + h2p - 360.0f) * 0.5f;
			} else {
				avghp = (h1p + h2p) * 0.5f;
			}

			float32 t = 1.0f - 0.17f * NkCos(NkRadiansFromDegrees(avghp - 30.0f)) +
						0.24f * NkCos(NkRadiansFromDegrees(2.0f * avghp)) +
						0.32f * NkCos(NkRadiansFromDegrees(3.0f * avghp + 6.0f)) -
						0.20f * NkCos(NkRadiansFromDegrees(4.0f * avghp - 63.0f));

			float32 deltaThetaArg = (avghp - 275.0f) / 25.0f;
			float32 deltaTheta = 30.0f * NkExp(-(deltaThetaArg * deltaThetaArg));

			float32 avgCp7 = NkPow(avgCp, 7.0f);
			float32 rc = 2.0f * NkSqrt(avgCp7 / (avgCp7 + k25Pow7));

			float32 avgLpDelta = avgLp - 50.0f;
			float32 sl = 1.0f + (0.015f * avgLpDelta * avgLpDelta) / NkSqrt(20.0f + avgLpDelta * avgLpDelta);
			float32 sc = 1.0f + 0.045f * avgCp;
			float32 sh = 1.0f + 0.015f * avgCp * t;

			float32 rt = -NkSin(NkRadiansFromDegrees(2.0f * deltaTheta)) * rc;

			float32 termL = deltaLp / sl;
			float32 termC = deltaCp / sc;
			float32 termH = deltaHp / sh;

			return NkSqrt(termL * termL + termC * termC + termH * termH + rt * termC * termH);
		}

		float32 NkColorF::ContrastRatio(const NkColorF &other) const noexcept {
			float32 l1 = Luminance();
			float32 l2 = other.Luminance();
			float32 lighter = NkMax(l1, l2);
			float32 darker = NkMin(l1, l2);
			return (lighter + 0.05f) / (darker + 0.05f);
		}

		NkColorF NkColorF::BestContrast(const NkColorF &dark, const NkColorF &light) const noexcept {
			float32 contrastDark = ContrastRatio(dark);
			float32 contrastLight = ContrastRatio(light);
			return (contrastDark >= contrastLight) ? dark : light;
		}

		// ---- Interpolation perceptuelle ----

		NkColorF NkColorF::LerpLAB(const NkColorF &from, const NkColorF &to, float32 t) noexcept {
			NkVector4f lab0 = from.ToLAB();
			NkVector4f lab1 = to.ToLAB();

			float32 L = lab0.r + (lab1.r - lab0.r) * t;
			float32 A = lab0.g + (lab1.g - lab0.g) * t;
			float32 B = lab0.b + (lab1.b - lab0.b) * t;
			float32 alpha = from.a + (to.a - from.a) * t;

			return FromLAB(L, A, B, alpha);
		}

		NkColorF NkColorF::LerpOKLab(const NkColorF &from, const NkColorF &to, float32 t) noexcept {
			NkVector4f lab0 = from.ToOKLab();
			NkVector4f lab1 = to.ToOKLab();

			float32 L = lab0.r + (lab1.r - lab0.r) * t;
			float32 A = lab0.g + (lab1.g - lab0.g) * t;
			float32 B = lab0.b + (lab1.b - lab0.b) * t;
			float32 alpha = from.a + (to.a - from.a) * t;

			return FromOKLab(L, A, B, alpha);
		}

		// ---- Modes de fusion ----

		NkColorF NkColorF::Overlay(const NkColorF &other) const noexcept {
			return {OverlayChannel(r, other.r), OverlayChannel(g, other.g), OverlayChannel(b, other.b), a};
		}

		NkColorF NkColorF::HardLight(const NkColorF &other) const noexcept {
			return {HardLightChannel(r, other.r), HardLightChannel(g, other.g), HardLightChannel(b, other.b), a};
		}

		NkColorF NkColorF::SoftLight(const NkColorF &other) const noexcept {
			return {SoftLightChannel(r, other.r), SoftLightChannel(g, other.g), SoftLightChannel(b, other.b), a};
		}

	} // namespace math

	// ============================================================================
	// SPÉCIALISATIONS : NKTOSTRING (ESPACE DE NOMS GLOBAL)
	// ============================================================================

	/**
	 * @brief Spécialisation de NkToString pour NkColorF avec support de formatage
	 * @param c Couleur flottante à convertir
	 * @param props Options de formatage optionnelles
	 * @return NkString formaté selon les propriétés spécifiées
	 */
	NkString NkToString(const math::NkColorF &c, const NkFormatProps &props) {
		if (props.type == 'H' || props.type == 'h') {
			// Format hexadécimal : #RRGGBB (en convertissant les flottants en 8-bit)
			return props.ApplyWidth(
				NkStringView(props.type == 'H'
								 ? NkFormat("#%02X%02X%02X", static_cast<int>(c.r * 255.0f),
											static_cast<int>(c.g * 255.0f), static_cast<int>(c.b * 255.0f))
								 : NkFormat("#%02x%02x%02x", static_cast<int>(c.r * 255.0f),
											static_cast<int>(c.g * 255.0f), static_cast<int>(c.b * 255.0f))),
				false);
		}
		return props.ApplyWidth(NkStringView(NkFormat("({0:.4f}, {1:.4f}, {2:.4f}, {3:.4f})", c.r, c.g, c.b, c.a)),
								false);
	}

	/**
	 * @brief Spécialisation de NkToString pour NkColor avec support de formatage
	 * @param c Couleur à convertir
	 * @param props Options de formatage optionnelles
	 * @return NkString formaté selon les propriétés spécifiées
	 */
	NkString NkToString(const math::NkColor &c, const NkFormatProps &props) {
		if (props.type == 'H' || props.type == 'h') {
			// Format hexadécimal : #RRGGBB
			return props.ApplyWidth(NkStringView(props.type == 'H'
													 ? NkFormat("#%02X%02X%02X", (int)c.r, (int)c.g, (int)c.b)
													 : NkFormat("#%02x%02x%02x", (int)c.r, (int)c.g, (int)c.b)),
									false);
		}
		return props.ApplyWidth(NkStringView(NkFormat("({0}, {1}, {2}, {3})", (int)c.r, (int)c.g, (int)c.b, (int)c.a)),
								false);
	}

} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - All Rights Reserved (see LICENSE)
// ============================================================