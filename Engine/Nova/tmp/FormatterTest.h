#pragma once

#include "Nkentseu/Nkentseu.h"
#include "Unitest/Unitest.h"
#include <limits>

namespace nkentseu {

    class FormatSpectTest {
    public:
        FormatSpectTest() {
            UNITEST_REGISTRY(UnitestRegisterIClass(FormatSpectTest::IntegerFormatTest, "Test formatage d'entiers"));
            UNITEST_REGISTRY(UnitestRegisterIClass(FormatSpectTest::FloatFormatTest, "Test formatage de flottants"));
            UNITEST_REGISTRY(UnitestRegisterIClass(FormatSpectTest::StringFormatTest, "Test formatage de chaînes"));
            UNITEST_REGISTRY(UnitestRegisterIClass(FormatSpectTest::EscapeSequenceTest, "Test séquences d'échappement"));
            UNITEST_REGISTRY(UnitestRegisterIClass(FormatSpectTest::DelimiterTest, "Test différents délimiteurs"));
            UNITEST_REGISTRY(UnitestRegisterIClass(FormatSpectTest::AdvancedFormatTest, "Test formatage avancé"));
        }

    private:
        void IntegerFormatTest(const std::string&) {
            charb buffer[256];

            FormatSpect::Format<charb>(buffer, "{0}", 42);
            UNITEST_EQUAL(StringUtils::CompareStrings<charb>(buffer, "42"), 0, "Format entier simple");

            FormatSpect::Format(buffer, "{0:x}", 255);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "ff"), 0, "Hexadécimal minuscule");

            FormatSpect::Format(buffer, "{0:X}", 255);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "FF"), 0, "Hexadécimal majuscule");

            FormatSpect::Format(buffer, "{0:0>5}", 123);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "00123"), 0, "Remplissage zéro à gauche");

            FormatSpect::Format(buffer, "{0:+}", 567);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "+567"), 0, "Affichage signe positif");
        }

        void FloatFormatTest(const std::string&) {
            charb buffer[256];

            FormatSpect::Format(buffer, "{0:.3}", 3.14159);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "3.142"), 0, "Précision 3 chiffres");

            double nan = std::numeric_limits<double>::quiet_NaN();
            FormatSpect::Format(buffer, "{0}", nan);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "nan"), 0, "NaN handling");

            double inf = std::numeric_limits<double>::infinity();
            FormatSpect::Format(buffer, "{0}", inf);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "inf"), 0, "Infini positif");

            FormatSpect::Format(buffer, "{0}", -inf);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "-inf"), 0, "Infini négatif");
        }

        void StringFormatTest(const std::string&) {
            charb buffer[256];
            const charb* str = "Test";

            FormatSpect::Format(buffer, "{0}", str);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "Test"), 0, "Chaîne simple");

            FormatSpect::Format(buffer, "{0:10}", str);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "      Test"), 0, "Alignement droite avec espace");
        }

        void EscapeSequenceTest(const std::string&) {
            char8 buffer[256];

            FormatSpect::Format(buffer, u8"Escapé \\{{0}\\}", 100);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, u8"Escapé {100}"), 0, "Délimiteurs échappés");

            FormatSpect::Format(buffer, u8"\\\\ Chemin \\\\", nullptr);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, u8"\\ Chemin \\"), 0, "Backslash échappé");
        }

        void DelimiterTest(const std::string&) {
            charb buffer[256];

            FormatSpect::Format(buffer, "[0]", 123);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "123"), 0, "Délimiteur carré");

            FormatSpect::Format(buffer, "(0:.2f)", 5.678);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "5.68"), 0, "Délimiteur parenthèse avec précision");
        }

        void AdvancedFormatTest(const std::string&) {
            charb buffer[256];

            FormatSpect::Format(buffer, "{0} {1} {2}", 1, 2.3, "Test");
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "1 2.3 Test"), 0, "Format multiple arguments");

            FormatSpect::Format(buffer, "{2} {1} {0}", "Premier", 2, 3.0f);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "3.0 2 Premier"), 0, "Ordre des arguments");
        }
    };

} // namespace nkentseu