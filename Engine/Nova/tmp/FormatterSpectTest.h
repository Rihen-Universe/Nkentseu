#pragma once

#include "Nkentseu/Nkentseu.h"
#include "Unitest/Unitest.h"

#include <limits>

namespace nkentseu {

    class FormatterSpectTest {
    public:
        FormatterSpectTest() {
            UNITEST_REGISTRY(UnitestRegisterIClass(FormatterSpectTest::BasicFormatTest, "Test basique"));
            UNITEST_REGISTRY(UnitestRegisterIClass(FormatterSpectTest::NumericFormatTest, "Test numérique"));
            UNITEST_REGISTRY(UnitestRegisterIClass(FormatterSpectTest::PrecisionTest, "Test précision"));
            UNITEST_REGISTRY(UnitestRegisterIClass(FormatterSpectTest::AlignmentTest, "Test alignement"));
        }

    private:
        void BasicFormatTest(const std::string&) {
            charb buffer[256];

            FormatterSpect::Format(buffer, "{0}", 42);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "42"), 0, "Entier basique");

            FormatterSpect::Format(buffer, "[0]", "Hello");
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "Hello"), 0, "Chaîne avec délimiteur carré");
        }

        void NumericFormatTest(const std::string&) {
            charb buffer[256];

            FormatterSpect::Format(buffer, "{0:x}", 255);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "ff"), 0, "Hexadécimal");

            FormatterSpect::Format(buffer, "{0:#X}", 255);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "0xFF"), 0, "Hexadécimal alterné");
        }

        void PrecisionTest(const std::string&) {
            charb buffer[256];

            FormatterSpect::Format(buffer, "{0:.3}", 3.14159);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "3.142"), 0, "Précision 3 chiffres");

            FormatterSpect::Format(buffer, "{0:.0}", 2.718);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "3"), 0, "Arrondi à l'entier");
        }

        void AlignmentTest(const std::string&) {
            charb buffer[256];

            FormatterSpect::Format(buffer, "{0:>5}", 123);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "  123"), 0, "Alignement droite");

            FormatterSpect::Format(buffer, "{0:<5}", 123);
            UNITEST_EQUAL(StringUtils::CompareStrings(buffer, "123  "), 0, "Alignement gauche");
        }
    };

} // namespace nkentseu