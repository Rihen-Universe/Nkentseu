//
// NkColor.cpp
// Définition des constantes de couleur nommées et implémentations HSV/Random.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "NkColor.h"
#include "NkRandom.h"

namespace nkentseu {

    namespace math {

        // =====================================================================
        // Définitions des constantes de couleur nommées
        //
        // Chaque couleur est initialisée via RGBf / RGBAf (valeurs [0,1]).
        // Certaines couleurs "exactes" utilisent le constructeur (r,g,b) direct.
        // =====================================================================

        const NkColor NkColor::Transparent      = NkColor::RGBAf(0.00f, 0.00f, 0.00f, 0.00f);
        const NkColor NkColor::Black            = NkColor::RGBf( 0.00f, 0.00f, 0.00f);
        const NkColor NkColor::White            = NkColor::RGBf( 1.00f, 1.00f, 1.00f);
        const NkColor NkColor::Red              = NkColor::RGBf( 1.00f, 0.00f, 0.00f);
        const NkColor NkColor::Green            = NkColor::RGBf( 0.00f, 1.00f, 0.00f);
        const NkColor NkColor::Blue             = NkColor::RGBf( 0.00f, 0.00f, 1.00f);
        const NkColor NkColor::Yellow           = NkColor::RGBf( 1.00f, 1.00f, 0.00f);
        const NkColor NkColor::Cyan             = NkColor::RGBf( 0.00f, 1.00f, 1.00f);
        const NkColor NkColor::Magenta          = NkColor::RGBf( 1.00f, 0.00f, 1.00f);
        const NkColor NkColor::Orange           = NkColor::RGBf( 1.00f, 0.50f, 0.00f);
        const NkColor NkColor::Pink             = NkColor::RGBf( 1.00f, 0.75f, 0.80f);
        const NkColor NkColor::Purple           = NkColor::RGBf( 0.50f, 0.00f, 0.50f);
        const NkColor NkColor::Gray             = NkColor::RGBf( 0.50f, 0.50f, 0.50f);
        const NkColor NkColor::DarkGray         = NkColor::RGBf( 0.10f, 0.10f, 0.10f);
        const NkColor NkColor::Lime             = NkColor::RGBf( 0.75f, 1.00f, 0.00f);
        const NkColor NkColor::Teal             = NkColor::RGBf( 0.00f, 0.50f, 0.50f);
        const NkColor NkColor::Brown            = NkColor::RGBf( 0.65f, 0.16f, 0.16f);
        const NkColor NkColor::SaddleBrown      = NkColor::RGBf( 0.55f, 0.27f, 0.07f);
        const NkColor NkColor::Olive            = NkColor::RGBf( 0.50f, 0.50f, 0.00f);
        const NkColor NkColor::Maroon           = NkColor::RGBf( 0.50f, 0.00f, 0.00f);
        const NkColor NkColor::Navy             = NkColor::RGBf( 0.00f, 0.00f, 0.50f);
        const NkColor NkColor::Indigo           = NkColor::RGBf( 0.29f, 0.00f, 0.51f);
        const NkColor NkColor::Turquoise        = NkColor::RGBf( 0.25f, 0.88f, 0.82f);
        const NkColor NkColor::Silver           = NkColor::RGBf( 0.75f, 0.75f, 0.75f);
        const NkColor NkColor::Gold             = NkColor::RGBf( 1.00f, 0.84f, 0.00f);
        const NkColor NkColor::SkyBlue          = NkColor::RGBf( 0.53f, 0.81f, 0.98f);
        const NkColor NkColor::ForestGreen      = NkColor::RGBf( 0.13f, 0.55f, 0.13f);
        const NkColor NkColor::SteelBlue        = NkColor::RGBf( 0.27f, 0.51f, 0.71f);
        const NkColor NkColor::DarkSlateGray    = NkColor::RGBf( 0.18f, 0.31f, 0.31f);
        const NkColor NkColor::Chocolate        = NkColor::RGBf( 0.82f, 0.41f, 0.12f);
        const NkColor NkColor::HotPink          = NkColor::RGBf( 1.00f, 0.41f, 0.71f);
        const NkColor NkColor::SlateBlue        = NkColor::RGBf( 0.42f, 0.35f, 0.80f);
        const NkColor NkColor::RoyalBlue        = NkColor::RGBf( 0.25f, 0.41f, 0.88f);
        const NkColor NkColor::Tomato           = NkColor::RGBf( 1.00f, 0.39f, 0.28f);
        const NkColor NkColor::MediumSeaGreen   = NkColor::RGBf( 0.24f, 0.70f, 0.44f);
        const NkColor NkColor::DarkOrange       = NkColor::RGBf( 1.00f, 0.55f, 0.00f);
        const NkColor NkColor::MediumPurple     = NkColor::RGBf( 0.58f, 0.44f, 0.86f);
        const NkColor NkColor::CornflowerBlue   = NkColor::RGBf( 0.39f, 0.58f, 0.93f);
        const NkColor NkColor::DarkGoldenrod    = NkColor::RGBf( 0.72f, 0.53f, 0.04f);
        const NkColor NkColor::DodgerBlue       = NkColor::RGBf( 0.12f, 0.56f, 1.00f);
        const NkColor NkColor::MediumVioletRed  = NkColor::RGBf( 0.78f, 0.08f, 0.52f);
        const NkColor NkColor::Peru             = NkColor::RGBf( 0.80f, 0.52f, 0.25f);
        const NkColor NkColor::MediumAquamarine = NkColor::RGBf( 0.40f, 0.80f, 0.67f);
        const NkColor NkColor::DarkTurquoise    = NkColor::RGBf( 0.00f, 0.81f, 0.82f);
        const NkColor NkColor::MediumSlateBlue  = NkColor::RGBf( 0.48f, 0.41f, 0.93f);
        const NkColor NkColor::YellowGreen      = NkColor::RGBf( 0.60f, 0.80f, 0.20f);
        const NkColor NkColor::LightCoral       = NkColor::RGBf( 0.94f, 0.50f, 0.50f);
        const NkColor NkColor::DarkSlateBlue    = NkColor::RGBf( 0.28f, 0.24f, 0.55f);
        const NkColor NkColor::DarkOliveGreen   = NkColor::RGBf( 0.33f, 0.42f, 0.18f);
        const NkColor NkColor::Firebrick        = NkColor::RGBf( 0.70f, 0.13f, 0.13f);
        const NkColor NkColor::MediumOrchid     = NkColor::RGBf( 0.73f, 0.33f, 0.83f);
        const NkColor NkColor::RosyBrown        = NkColor::RGBf( 0.74f, 0.56f, 0.56f);
        const NkColor NkColor::DarkCyan         = NkColor::RGBf( 0.00f, 0.55f, 0.55f);
        const NkColor NkColor::CadetBlue        = NkColor::RGBf( 0.37f, 0.62f, 0.63f);
        const NkColor NkColor::PaleVioletRed    = NkColor::RGBf( 0.86f, 0.44f, 0.58f);
        const NkColor NkColor::DeepPink         = NkColor::RGBf( 1.00f, 0.08f, 0.58f);
        const NkColor NkColor::LawnGreen        = NkColor::RGBf( 0.49f, 0.99f, 0.00f);
        const NkColor NkColor::MediumSpringGreen= NkColor::RGBf( 0.00f, 0.98f, 0.60f);
        const NkColor NkColor::MediumTurquoise  = NkColor::RGBf( 0.28f, 0.82f, 0.80f);
        const NkColor NkColor::PaleGreen        = NkColor::RGBf( 0.60f, 0.98f, 0.60f);
        const NkColor NkColor::DarkKhaki        = NkColor::RGBf( 0.74f, 0.72f, 0.42f);
        const NkColor NkColor::MediumBlue       = NkColor::RGBf( 0.00f, 0.00f, 0.80f);
        const NkColor NkColor::NavajoWhite      = NkColor::RGBf( 1.00f, 0.87f, 0.68f);
        const NkColor NkColor::DarkSalmon       = NkColor::RGBf( 0.91f, 0.59f, 0.48f);
        const NkColor NkColor::MediumCoral      = NkColor::RGBf( 0.81f, 0.36f, 0.36f);
        const NkColor NkColor::MidnightBlue     = NkColor(25,  25,  112);
        const NkColor NkColor::DefaultBackground= NkColor(0,   162, 232);
        const NkColor NkColor::CharcoalBlack    = NkColor(31,  31,  31);
        const NkColor NkColor::SlateGray        = NkColor(46,  46,  46);
        const NkColor NkColor::SkyBlueRef       = NkColor(50,  130, 246);
        const NkColor NkColor::DuckBlue         = NkColor(0,   162, 232);

        // =====================================================================
        // Conversion HSV → RGB
        // =====================================================================

        NkColor NkColor::FromHSV(const NkHSV& hsv) noexcept {
            float32 h = hsv.hue        / 360.0f;
            float32 s = hsv.saturation / 100.0f;
            float32 v = hsv.value      / 100.0f;

            int32   i = static_cast<int32>(h * 6.0f);
            float32 f = h * 6.0f - static_cast<float32>(i);
            float32 p = v * (1.0f - s);
            float32 q = v * (1.0f - f * s);
            float32 t = v * (1.0f - (1.0f - f) * s);

            float32 cr = 0.0f, cg = 0.0f, cb = 0.0f;
            switch (i % 6) {
                case 0: cr = v; cg = t; cb = p; break;
                case 1: cr = q; cg = v; cb = p; break;
                case 2: cr = p; cg = v; cb = t; break;
                case 3: cr = p; cg = q; cb = v; break;
                case 4: cr = t; cg = p; cb = v; break;
                case 5: cr = v; cg = p; cb = q; break;
            }

            return {
                static_cast<uint8>(cr * 255.0f),
                static_cast<uint8>(cg * 255.0f),
                static_cast<uint8>(cb * 255.0f)
            };
        }

        // =====================================================================
        // Conversion RGB → HSV
        // =====================================================================

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
                if      (maxC == fr) { h = (fg - fb) / d + (fg < fb ? 6.0f : 0.0f); }
                else if (maxC == fg) { h = (fb - fr) / d + 2.0f; }
                else                 { h = (fr - fg) / d + 4.0f; }
                h /= 6.0f;
            }

            return { h * 360.0f, s * 100.0f, v * 100.0f };
        }

        // =====================================================================
        // Couleurs aléatoires
        // =====================================================================

        NkColor NkColor::RandomRGB() noexcept {
            return {
                static_cast<uint8>(NkRandom::Instance().NextUInt32(256u)),
                static_cast<uint8>(NkRandom::Instance().NextUInt32(256u)),
                static_cast<uint8>(NkRandom::Instance().NextUInt32(256u))
            };
        }

        NkColor NkColor::RandomRGBA() noexcept {
            return {
                static_cast<uint8>(NkRandom::Instance().NextUInt32(256u)),
                static_cast<uint8>(NkRandom::Instance().NextUInt32(256u)),
                static_cast<uint8>(NkRandom::Instance().NextUInt32(256u)),
                static_cast<uint8>(NkRandom::Instance().NextUInt32(256u))
            };
        }

        // =====================================================================
        // Lookup par nom (recherche linéaire — ~60 entrées, usage rare)
        // =====================================================================

        const NkColor& NkColor::FromName(const NkString& name) noexcept {
            if (name == "Transparent")       { return Transparent;      }
            if (name == "Black")             { return Black;            }
            if (name == "White")             { return White;            }
            if (name == "Red")               { return Red;              }
            if (name == "Green")             { return Green;            }
            if (name == "Blue")              { return Blue;             }
            if (name == "Yellow")            { return Yellow;           }
            if (name == "Cyan")              { return Cyan;             }
            if (name == "Magenta")           { return Magenta;          }
            if (name == "Orange")            { return Orange;           }
            if (name == "Pink")              { return Pink;             }
            if (name == "Purple")            { return Purple;           }
            if (name == "Gray")              { return Gray;             }
            if (name == "DarkGray")          { return DarkGray;         }
            if (name == "Lime")              { return Lime;             }
            if (name == "Teal")              { return Teal;             }
            if (name == "Brown")             { return Brown;            }
            if (name == "SaddleBrown")       { return SaddleBrown;      }
            if (name == "Olive")             { return Olive;            }
            if (name == "Maroon")            { return Maroon;           }
            if (name == "Navy")              { return Navy;             }
            if (name == "Indigo")            { return Indigo;           }
            if (name == "Turquoise")         { return Turquoise;        }
            if (name == "Silver")            { return Silver;           }
            if (name == "Gold")              { return Gold;             }
            if (name == "SkyBlue")           { return SkyBlue;          }
            if (name == "ForestGreen")       { return ForestGreen;      }
            if (name == "SteelBlue")         { return SteelBlue;        }
            if (name == "DarkSlateGray")     { return DarkSlateGray;    }
            if (name == "Chocolate")         { return Chocolate;        }
            if (name == "HotPink")           { return HotPink;          }
            if (name == "SlateBlue")         { return SlateBlue;        }
            if (name == "RoyalBlue")         { return RoyalBlue;        }
            if (name == "Tomato")            { return Tomato;           }
            if (name == "MediumSeaGreen")    { return MediumSeaGreen;   }
            if (name == "DarkOrange")        { return DarkOrange;       }
            if (name == "MediumPurple")      { return MediumPurple;     }
            if (name == "CornflowerBlue")    { return CornflowerBlue;   }
            if (name == "DarkGoldenrod")     { return DarkGoldenrod;    }
            if (name == "DodgerBlue")        { return DodgerBlue;       }
            if (name == "MediumVioletRed")   { return MediumVioletRed;  }
            if (name == "Peru")              { return Peru;             }
            if (name == "MediumAquamarine")  { return MediumAquamarine; }
            if (name == "DarkTurquoise")     { return DarkTurquoise;    }
            if (name == "MediumSlateBlue")   { return MediumSlateBlue;  }
            if (name == "YellowGreen")       { return YellowGreen;      }
            if (name == "LightCoral")        { return LightCoral;       }
            if (name == "DarkSlateBlue")     { return DarkSlateBlue;    }
            if (name == "DarkOliveGreen")    { return DarkOliveGreen;   }
            if (name == "Firebrick")         { return Firebrick;        }
            if (name == "MediumOrchid")      { return MediumOrchid;     }
            if (name == "RosyBrown")         { return RosyBrown;        }
            if (name == "DarkCyan")          { return DarkCyan;         }
            if (name == "CadetBlue")         { return CadetBlue;        }
            if (name == "PaleVioletRed")     { return PaleVioletRed;    }
            if (name == "DeepPink")          { return DeepPink;         }
            if (name == "LawnGreen")         { return LawnGreen;        }
            if (name == "MediumSpringGreen") { return MediumSpringGreen;}
            if (name == "MediumTurquoise")   { return MediumTurquoise;  }
            if (name == "PaleGreen")         { return PaleGreen;        }
            if (name == "DarkKhaki")         { return DarkKhaki;        }
            if (name == "MediumBlue")        { return MediumBlue;       }
            if (name == "MidnightBlue")      { return MidnightBlue;     }
            if (name == "NavajoWhite")       { return NavajoWhite;      }
            if (name == "DarkSalmon")        { return DarkSalmon;       }
            if (name == "MediumCoral")       { return MediumCoral;      }
            if (name == "DefaultBackground") { return DefaultBackground; }
            if (name == "CharcoalBlack")     { return CharcoalBlack;    }
            if (name == "SlateGray")         { return SlateGray;        }
            if (name == "SkyBlueRef")        { return SkyBlueRef;       }
            if (name == "DuckBlue")          { return DuckBlue;         }

            // Couleur non trouvée → noir par défaut
            static const NkColor fallback;
            return fallback;
        }

    } // namespace math

} // namespace nkentseu