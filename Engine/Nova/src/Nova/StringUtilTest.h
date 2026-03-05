#pragma once

#include "Nkentseu/Nkentseu.h"
#include "Unitest/Unitest.h"
#include <cstring>
#include <cwchar> // pour wcscmp
#include <string_view> // Pour std::u8string_view, std::u16string_view, std::u32string_view

#include <string>

namespace nkentseu {

    class StringConvertTest {
    public:
        StringConvertTest() {
            UNITEST_REGISTRY(UnitestRegisterIClass(StringConvertTest::SignedIntConversionTest, "Conversion des entiers signes"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringConvertTest::UnsignedIntConversionTest, "Conversion des entiers non signes"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringConvertTest::FloatingPointConversionTest, "Conversion des nombres flottants"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringConvertTest::BoolConversionTest, "Conversion des booleens"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringConvertTest::StringEncodingConversionTest, "Conversion entre encodages"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringConvertTest::CrossEncodingConversionTest, "Conversion multi-encodage"));
        }

    private:
        void SignedIntConversionTest(const std::string&) {
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<int8>(-128), "-128"), 0, "int8 negatif");
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<int16>(32767), "32767"), 0, "int16 max");
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<int32>(-123456), "-123456"), 0, "int32 valeur negative");
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<int64>(9223372036854775807LL), "9223372036854775807"), 0, "int64 max");
        }

        void UnsignedIntConversionTest(const std::string&) {
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<uint8>(255), "255"), 0, "uint8 max");
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<uint16>(65535), "65535"), 0, "uint16 max");
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<uint32>(4294967295U), "4294967295"), 0, "uint32 max");
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<uint64>(18446744073709551615ULL), "18446744073709551615"), 0, "uint64 max");
        }

        void FloatingPointConversionTest(const std::string&) {
            // StringConverter<charb>::Convert<Type>(value, precision) default_precision = 15
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<float32>(3.14159f, 5), "3.14159"), 0, "float32 π approx = {0}", StringConverter<charb>::Convert<float32>(3.14159f));
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<float64>(2.718281828459045), "2.718281828459045"), 0, "float64 e = {0}", StringConverter<charb>::Convert<float64>(2.718281828459045));
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<float64>(-0.001), "-0.001"), 0, "float64 valeur negative = {0}", StringConverter<charb>::Convert<float64>(-0.001));
        }

        void BoolConversionTest(const std::string&) {
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<bool>(true), "true"), 0, "Conversion booleen true = {0}", StringConverter<charb>::Convert<bool>(true));
            UNITEST_EQUAL(strcmp(StringConverter<charb>::Convert<bool>(false), "false"), 0, "Conversion booleen false = {0}", StringConverter<charb>::Convert<bool>(false));
        }

        void StringEncodingConversionTest(const std::string&) {
            const charb* str = "Nkentseu!";
            
            auto char8Str = StringConverter<charb>::ToChar8(str);
            UNITEST_EQUAL(std::u8string_view(char8Str), u8"Nkentseu!", "Conversion vers char8");
        
            auto char16Str = StringConverter<charb>::ToChar16(str);
            UNITEST_EQUAL(std::u16string_view(char16Str), u"Nkentseu!", "Conversion vers char16");
        
            auto char32Str = StringConverter<charb>::ToChar32(str);
            UNITEST_EQUAL(std::u32string_view(char32Str), U"Nkentseu!", "Conversion vers char32");
        
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            auto wcharStr = StringConverter<charb>::ToWchar(str);
            UNITEST_EQUAL(std::wstring_view(wcharStr), L"Nkentseu!", "Conversion vers wchar");
        #endif
        
            // On peut aussi tester l'inverse si ConvertFrom<charb> existe pour les autres types
            auto backToCharb = StringConverter<char8>::ToCharb(char8Str);
            UNITEST_EQUAL(strcmp(backToCharb, str), 0, "Retour à charb depuis char8");
        }

        void CrossEncodingConversionTest(const std::string&) {
            // Test UTF-8 vers UTF-16
            const char8* utf8Str = u8"🌍 Unicode Test";
            const char16* utf16Result = StringConverter<char8>::ToChar16(utf8Str);
            UNITEST_EQUAL(StringUtils::CompareStrings<char16>(utf16Result, u"🌍 Unicode Test"), 0, "Compare pour se rassurer que utf16Result est egal a utf8Str");
            
            // Test wchar_t vers charb
            const wchar* wideStr = NK_WIDE("Hello 世界");
            const charb* charbResult = StringConverter<wchar>::ToCharb(wideStr);
            UNITEST_EQUAL(StringUtils::CompareStrings<charb>(charbResult, "Hello ??"), 0, "Compare pour se rassurer que {0} est egal a wideStr", charbResult);
            
            // Test des limites de buffer
            char16 smallBuffer[5];
            StringUtils::CopyBoundedString(smallBuffer, u"TestLong", 4);
            UNITEST_EQUAL(StringUtils::CompareStrings<char16>(smallBuffer, u"Test"), 0, "Compare pour se rassurer que smallBuffer est egal au texte brute");
        }
    };

    class StringUtilsTest {
        public:
            StringUtilsTest() {
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::StringLengthComputationTest, "Calcul de la longueur des chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::StringComparisonTest, "Comparaison de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::BoundedStringComparisonTest, "Comparaison bornee de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::StringCopyTest, "Copie de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::BoundedStringCopyTest, "Copie bornee de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::StringConcatenationTest, "Concatenation de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::BoundedStringConcatenationTest, "Concatenation bornee de chaînes"));

                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::TestStringLength, "Test de la longueur de chaîne"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::TestStringComparison, "Test de la comparaison de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::TestStringNComparison, "Test de la comparaison partielle de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::TestStringCopy, "Test de la copie de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::TestStringNCopy, "Test de la copie partielle de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::TestStringConcatenation, "Test de la concatenation de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::TestStringNConcatenation, "Test de la concatenation partielle de chaînes"));

                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::AdvancedSearchTests, "Recherche avancée dans les chaînes"));

                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::MemoryOperationsTests, "Gestion memoire"));

                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::TokenizationTests, "Tests de découpage de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::StringDuplicationTests, "Tests de duplication de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::NumericConversionTests, "Tests de conversion numérique"));

                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::StringManipulationTests, "Manipulation de chaînes"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::CharacterClassificationTests, "Classification de caractères"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::NumericConversionAdvancedTests, "Conversions numériques avancées"));
                UNITEST_REGISTRY(UnitestRegisterIClass(StringUtilsTest::MemoryOperationsAdvancedTests, "Opérations mémoire avancées"));
            }

            void MemoryOperationsTests(const std::string&) {
                TestRawMemoryCopy();
                TestSafeMemoryMove();
                TestMemoryComparison();
                TestMemoryInitialization();
                TestBoundedStringLength();
            }
    
        private:
            // Helper pour comparer les chaînes selon le type
            template<typename CharT>
            static int32 Compare(const CharT* a, const CharT* b) {
                if constexpr (std::is_same_v<CharT, charb>) {
                    return strcmp(a, b);
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                } else if constexpr (std::is_same_v<CharT, wchar>) {
                    return wcscmp(a, b);
                #endif
                } else if constexpr (std::is_same_v<CharT, char8>) {
                    return std::u8string_view(a).compare(b);
                } else if constexpr (std::is_same_v<CharT, char16>) {
                    return std::u16string_view(a).compare(b);
                } else if constexpr (std::is_same_v<CharT, char32>) {
                    return std::u32string_view(a).compare(b);
                } else {
                    static_assert(sizeof(CharT) == 0, "Unsupported character type");
                }
            }
    
            void StringLengthComputationTest(const std::string&) {
                // Test charb (char)
                UNITEST_EQUAL(StringUtils::ComputeStringLength<charb>(""), 0, "Chaîne vide (char)");
                UNITEST_EQUAL(StringUtils::ComputeStringLength<charb>("Hello"), 5, "Chaîne normale (char)");
    
                // Test wchar (wchar_t)
                UNITEST_EQUAL(StringUtils::ComputeStringLength<wchar>(NK_WIDE("")), 0, "Chaîne vide (wchar_t)");
                UNITEST_EQUAL(StringUtils::ComputeStringLength<wchar>(NK_WIDE("Привет")), 6, "Chaîne Unicode (wchar_t)");
            }
    
            void StringComparisonTest(const std::string&) {
                // Test charb
                UNITEST_EQUAL(StringUtils::CompareStrings<charb>("abc", "abc"), 0, "egalite (char)");
                UNITEST_EQUAL(StringUtils::CompareStrings<charb>("abc", "abd"), -1, "Inferiorite (char)");
                UNITEST_EQUAL(StringUtils::CompareStrings<charb>("abd", "abc"), 1, "Superiorite (char)");
    
                // Test wchar
                UNITEST_EQUAL(StringUtils::CompareStrings<wchar>(NK_WIDE("αβγ"), NK_WIDE("αβγ")), 0, "egalite Unicode (wchar_t)");
                UNITEST_EQUAL(Compare(NK_WIDE("αβδ"), NK_WIDE("αβγ")), 1, "Superiorite Unicode (wchar_t)");
            }
    
            void BoundedStringComparisonTest(const std::string&) {
                const charb* str1 = "abcxyz";
                const charb* str2 = "abcdef";
                UNITEST_EQUAL(StringUtils::CompareBoundedStrings(str1, str2, 3), 0, "Comparaison bornee egale");
                UNITEST_DIFFERENT(StringUtils::CompareBoundedStrings(str1, str2, 4), 0, "Comparaison bornee differente");
            }
    
            void StringCopyTest(const std::string&) {
                charb buffer[32];
                StringUtils::CopyString(buffer, "test");
                UNITEST_EQUAL(Compare(buffer, "test"), 0, "Copie complete (char)");
    
                wchar wbuffer[32];
                StringUtils::CopyString(wbuffer, NK_WIDE("wide"));
                UNITEST_EQUAL(Compare(wbuffer, NK_WIDE("wide")), 0, "Copie complete (wchar_t)");
            }
    
            void BoundedStringCopyTest(const std::string&) {
                charb buffer[6] = "xxxxx";
                StringUtils::CopyBoundedString(buffer, "abc", 2);
                UNITEST_EQUAL(Compare(buffer, "abxxx"), 0, "Copie partielle avec remplissage");
            }
    
            void StringConcatenationTest(const std::string&) {
                charb buffer[32] = "Hello";
                StringUtils::ConcatenateStrings(buffer, " World");
                UNITEST_EQUAL(Compare(buffer, "Hello World"), 0, "Concatenation simple (char)");
            }
    
            void BoundedStringConcatenationTest(const std::string&) {
                charb buffer[32] = "Hello";
                StringUtils::ConcatenateBoundedStrings(buffer, " World!", 3);
                UNITEST_EQUAL(Compare(buffer, "Hello Wo"), 0, "Concatenation bornee (char)");
            }

            void TestStringLength(const std::string&) {
                UNITEST_EQUAL(StringUtils::ComputeStringLength<charb>("Hello"), 5, "Longueur de 'Hello'");
                UNITEST_EQUAL(StringUtils::ComputeStringLength<wchar>(NK_WIDE("Bonjour")), 7, "Longueur de 'Bonjour'");
                UNITEST_EQUAL(StringUtils::ComputeStringLength<char8>(u8"Hello"), 5, "Longueur de 'Hello' en UTF-8");
                UNITEST_EQUAL(StringUtils::ComputeStringLength<char16>(u"Hello"), 5, "Longueur de 'Hello' en UTF-16");
                UNITEST_EQUAL(StringUtils::ComputeStringLength<char32>(U"Hello"), 5, "Longueur de 'Hello' en UTF-32");
            }
    
            void TestStringComparison(const std::string&) {
                UNITEST_EQUAL(StringUtils::CompareStrings<charb>("Hello", "Hello"), 0, "Comparaison 'Hello' == 'Hello'");
                UNITEST_EQUAL(StringUtils::CompareStrings<wchar>(NK_WIDE("Bonjour"), NK_WIDE("Bonjour")), 0, "Comparaison 'Bonjour' == 'Bonjour'");
                UNITEST_EQUAL(StringUtils::CompareStrings<char8>(u8"Hello", u8"Hello"), 0, "Comparaison 'Hello' == 'Hello' en UTF-8");
                UNITEST_EQUAL(StringUtils::CompareStrings<char16>(u"Hello", u"Hello"), 0, "Comparaison 'Hello' == 'Hello' en UTF-16");
                UNITEST_EQUAL(StringUtils::CompareStrings<char32>(U"Hello", U"Hello"), 0, "Comparaison 'Hello' == 'Hello' en UTF-32");
            }
    
            void TestStringNComparison(const std::string&) {
                UNITEST_EQUAL(StringUtils::CompareBoundedStrings<charb>("Hello", "HelloWorld", 5), 0, "Comparaison partielle 'Hello' == 'HelloWorld' (5)");
                UNITEST_EQUAL(StringUtils::CompareBoundedStrings<wchar>(NK_WIDE("Bonjour"), NK_WIDE("BonjourMonde"), 7), 0, "Comparaison partielle 'Bonjour' == 'BonjourMonde' (7)");
                UNITEST_EQUAL(StringUtils::CompareBoundedStrings<char8>(u8"Hello", u8"HelloWorld", 5), 0, "Comparaison partielle 'Hello' == 'HelloWorld' (5) en UTF-8");
                UNITEST_EQUAL(StringUtils::CompareBoundedStrings<char16>(u"Hello", u"HelloWorld", 5), 0, "Comparaison partielle 'Hello' == 'HelloWorld' (5) en UTF-16");
                UNITEST_EQUAL(StringUtils::CompareBoundedStrings<char32>(U"Hello", U"HelloWorld", 5), 0, "Comparaison partielle 'Hello' == 'HelloWorld' (5) en UTF-32");
            }
    
            void TestStringCopy(const std::string&) {
                charb dest1[10];
                StringUtils::CopyString(dest1, "Hello");
                UNITEST_EQUAL(Compare(dest1, "Hello"), 0, "Copie de 'Hello'");
    
                wchar dest2[10];
                StringUtils::CopyString(dest2, NK_WIDE("Bonjour"));
                UNITEST_EQUAL(Compare(dest2, NK_WIDE("Bonjour")), 0, "Copie de 'Bonjour'");
    
                char8 dest3[10];
                StringUtils::CopyString<char8>(dest3, u8"Hello");
                UNITEST_EQUAL(Compare<char8>(dest3, u8"Hello"), 0, "Copie de 'Hello' en UTF-8");
    
                char16 dest4[10];
                StringUtils::CopyString(dest4, u"Hello");
                UNITEST_EQUAL(Compare(dest4, u"Hello"), 0, "Copie de 'Hello' en UTF-16");
    
                char32 dest5[10];
                StringUtils::CopyString(dest5, U"Hello");
                UNITEST_EQUAL(Compare(dest5, U"Hello"), 0, "Copie de 'Hello' en UTF-32");
            }
    
            void TestStringNCopy(const std::string&) {
                charb dest1[10];
                StringUtils::CopyBoundedString(dest1, "HelloWorld", 5);
                UNITEST_EQUAL(StringUtils::CompareStrings<charb>(dest1, "Hello"), 0, "Copie partielle de 'HelloWorld' (5): {0} = Hello", dest1);
    
                wchar dest2[10];
                StringUtils::CopyBoundedString(dest2, NK_WIDE("BonjourMonde"), 7);
                UNITEST_EQUAL(StringUtils::CompareStrings<wchar>(dest2, NK_WIDE("Bonjour")), 0, "Copie partielle de 'BonjourMonde' (7)");
    
                char8 dest3[10];
                StringUtils::CopyBoundedString(dest3, u8"HelloWorld", 5);
                UNITEST_EQUAL(StringUtils::CompareStrings<char8>(dest3, u8"Hello"), 0, "Copie partielle de 'HelloWorld' (5) en UTF-8");
    
                char16 dest4[10];
                StringUtils::CopyBoundedString(dest4, u"HelloWorld", 5);
                UNITEST_EQUAL(StringUtils::CompareStrings<char16>(dest4, u"Hello"), 0, "Copie partielle de 'HelloWorld' (5) en UTF-16");
    
                char32 dest5[10];
                StringUtils::CopyBoundedString(dest5, U"HelloWorld", 5);
                UNITEST_EQUAL(Compare(dest5, U"Hello"), 0, "Copie partielle de 'HelloWorld' (5) en UTF-32");
            }
    
            void TestStringConcatenation(const std::string&) {
                charb dest1[20] = "Hello";
                StringUtils::ConcatenateStrings(dest1, " World");
                UNITEST_EQUAL(Compare(dest1, "Hello World"), 0, "Concatenation de 'Hello' et ' World'");
    
                wchar dest2[20] = NK_WIDE("Bonjour");
                StringUtils::ConcatenateStrings(dest2, NK_WIDE(" Monde"));
                UNITEST_EQUAL(Compare(dest2, NK_WIDE("Bonjour Monde")), 0, "Concatenation de 'Bonjour' et ' Monde'");
    
                char8 dest3[20] = u8"Hello";
                StringUtils::ConcatenateStrings(dest3, u8" World");
                UNITEST_EQUAL(Compare(reinterpret_cast<const char*>(dest3), "Hello World"), 0, "Concatenation de 'Hello' et ' World' en UTF-8");
    
                char16 dest4[20] = u"Hello";
                StringUtils::ConcatenateStrings(dest4, u" World");
                UNITEST_EQUAL(Compare(dest4, u"Hello World"), 0, "Concatenation de 'Hello' et ' World' en UTF-16");
    
                char32 dest5[20] = U"Hello";
                StringUtils::ConcatenateStrings(dest5, U" World");
                UNITEST_EQUAL(Compare(dest5, U"Hello World"), 0, "Concatenation de 'Hello' et ' World' en UTF-32");
            }
    
            void TestStringNConcatenation(const std::string&) {
                charb dest1[20] = "Hello";
                StringUtils::ConcatenateBoundedStrings(dest1, " World", 5);
                UNITEST_EQUAL(Compare(dest1, "Hello Worl"), 0, "Concatenation partielle de 'Hello' et ' World' (5) : {0}", dest1);
    
                wchar dest2[20] = NK_WIDE("Bonjour");
                StringUtils::ConcatenateBoundedStrings(dest2, NK_WIDE(" Monde"), 5);
                UNITEST_EQUAL(Compare(dest2, NK_WIDE("Bonjour Mond")), 0, "Concatenation partielle de 'Bonjour' et ' Monde' (5)");
    
                char8 dest3[20] = u8"Hello";
                StringUtils::ConcatenateBoundedStrings(dest3, u8" World", 5);
                UNITEST_EQUAL(Compare<char8>(dest3, u8"Hello Worl"), 0, "Concatenation partielle de 'Hello' et ' World' (5) en UTF-8");
    
                char16 dest4[20] = u"Hello";
                StringUtils::ConcatenateBoundedStrings(dest4, u" World", 5);
                UNITEST_EQUAL(Compare(dest4, u"Hello Worl"), 0, "Concatenation partielle de 'Hello' et ' World' (5) en UTF-16");
    
                char32 dest5[20] = U"Hello";
                StringUtils::ConcatenateBoundedStrings(dest5, U" World", 5);
                UNITEST_EQUAL(Compare(dest5, U"Hello Worl"), 0, "Concatenation partielle de 'Hello' et ' World' (5) en UTF-32");
            }

            void AdvancedSearchTests(const std::string&) {
                TestCharacterSearch<charb>("abcde", 'c', 2);
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    TestCharacterSearch<wchar>(NK_WIDE("unicode🌍"), 0xD83C, 7); // High surrogate
                #else
                    TestCharacterSearch<wchar>(NK_WIDE("unicode🌍"), NK_WIDE('🌍'), 7); // UTF-32
                #endif
                TestStringSearch<char16>(u"Hello 世界", u"世界", 6);
                TestSpanFunctions<charb>("123abc", "0123456789", 3);
            }
        
            template<typename CharT>
            void TestCharacterSearch(const CharT* str, CharT target, usize expectedPos) {
                const CharT* found = StringUtils::FindChar(str, target);
                UNITEST_TRUE(found != nullptr && static_cast<usize>(found - str) == expectedPos, "found at position {0}", expectedPos);
                
                const CharT* lastFound = StringUtils::FindLastChar(str, target);
                UNITEST_TRUE(lastFound != nullptr && static_cast<usize>(lastFound - str) == expectedPos, "last found at position {0}", expectedPos);
            }
        
            template<typename CharT>
            void TestStringSearch(const CharT* haystack, const CharT* needle, usize expectedPos) {
                const CharT* found = StringUtils::FindSubstring(haystack, needle);
                UNITEST_TRUE(found != nullptr && static_cast<usize>(found - haystack) == expectedPos, "'needle' found at position {0}", expectedPos);
            }
        
            template<typename CharT>
            void TestSpanFunctions(const CharT* str, const CharT* set, usize expectedSpan) {
                UNITEST_EQUAL(StringUtils::SpanInclusive(str, set), expectedSpan, "SpanInclusive : {0}", expectedSpan);
                UNITEST_EQUAL(StringUtils::SpanExclusive(str, set), expectedSpan, "SpanExclusive: {0} == {1}", expectedSpan, StringUtils::SpanExclusive(str, set));
            }

            void TestRawMemoryCopy() {
                // Cas standard
                charb source[6] = "Hello";
                charb dest[6];
                StringUtils::RawMemoryCopy(dest, source, 6);
                UNITEST_EQUAL(memcmp(dest, source, 6), 0, "Copie brute complète");
                
                // Copie partielle
                StringUtils::RawMemoryCopy(dest, source, 3);
                UNITEST_EQUAL(memcmp(dest, "Hel", 3), 0, "Copie brute partielle");
                
                // Test avec 0 octets
                StringUtils::RawMemoryCopy(dest, source, 0);
                UNITEST_TRUE(true, "Copie brute avec count=0 ne modifie rien");
            }
        
            void TestSafeMemoryMove() {
                charb buffer[10] = "abcdefghi";
                
                // Chevauchement dest < src
                StringUtils::SafeMemoryMove(buffer, buffer + 3, 4);
                UNITEST_EQUAL(memcmp(buffer, "defgefghi", 10), 0, "Déplacement avant");
                
                // Chevauchement dest > src
                StringUtils::SafeMemoryMove(buffer + 2, buffer, 4);
                UNITEST_EQUAL(memcmp(buffer, "dedefgghi", 10), 0, "Déplacement arrière {0}", buffer);
                
                // Pas de chevauchement
                charb dest[5];
                StringUtils::SafeMemoryMove(dest, buffer, 5);
                UNITEST_EQUAL(memcmp(dest, "dedef", 5), 0, "Déplacement sécurisé simple");
            }
        
            void TestMemoryComparison() {
                charb data1[5] = {1, 2, 3, 4, 5};
                charb data2[5] = {1, 2, 3, 4, 5};
                charb data3[5] = {1, 2, 0, 4, 5};
                
                UNITEST_EQUAL(StringUtils::MemoryCompare(data1, data2, 5), 0, "Mémoire identique");
                UNITEST_TRUE(StringUtils::MemoryCompare(data1, data3, 5) > 0, "Différence mémoire");
                UNITEST_EQUAL(StringUtils::MemoryCompare(data1, data3, 2), 0, "Comparaison partielle");
            }
        
            void TestMemoryInitialization() {
                charb buffer[10];
                
                // Initialisation complète
                StringUtils::MemorySet(buffer, 'A', 10);
                UNITEST_TRUE(std::all_of(buffer, buffer + 10, [](charb c){ return c == 'A'; }), "Remplissage complet");
                
                // Initialisation partielle
                StringUtils::MemorySet(buffer, SLTT(charb), 5);
                UNITEST_EQUAL(memcmp(buffer, "\0\0\0\0\0AAAAA", 10), 0, "Remplissage partiel");
            }
        
            void TestBoundedStringLength() {
                // Chaîne plus courte que max
                UNITEST_EQUAL(StringUtils::BoundedStringLength<charb>("Hello", 10), 5, "Longueur normale");
                
                // Chaîne exactement max
                UNITEST_EQUAL(StringUtils::BoundedStringLength<charb>("12345", 5), 5, "Longueur limite atteinte");
                
                // Chaîne plus longue que max
                UNITEST_EQUAL(StringUtils::BoundedStringLength<charb>("Hello World", 5), 5, "Troncature de longueur");
                
                // Test avec caractère nul avant max
                UNITEST_EQUAL(StringUtils::BoundedStringLength<charb>("Test\0Hidden", 10), 4, "Arrêt sur caractère nul");
            }

            /* Tests de découpage de chaînes */
            void TokenizationTests(const std::string&) {
                TestBasicTokenization();
                TestThreadSafeTokenization();
                TestEdgeCaseTokenization();
            }

            void TestBasicTokenization() {
                charb str[] = "A,B,C;D";
                const charb* delims = ",;";
                
                charb* token = StringUtils::TokenizeString(str, delims);
                UNITEST_EQUAL(Compare(token, "A"), 0, "Premier token");
                
                token = StringUtils::TokenizeString<charb>(nullptr, delims);
                UNITEST_EQUAL(Compare(token, "B"), 0, "Deuxième token");
                
                token = StringUtils::TokenizeString<charb>(nullptr, delims);
                UNITEST_EQUAL(Compare(token, "C"), 0, "Troisième token");
            }

            void TestThreadSafeTokenization() {
                charb str[] = "X/Y/Z";
                charb* context;
                const charb* delims = "/";
                
                charb* token = StringUtils::TokenizeStringSafe(str, delims, &context);
                UNITEST_EQUAL(Compare(token, "X"), 0, "Token safe 1");
                
                token = StringUtils::TokenizeStringSafe<charb>(nullptr, delims, &context);
                UNITEST_EQUAL(Compare(token, "Y"), 0, "Token safe 2");
            }

            void TestEdgeCaseTokenization() {
                // Test avec délimiteurs consécutifs
                charb str[] = "Hello,,World";
                charb* token = StringUtils::TokenizeString(str, ",");
                UNITEST_EQUAL(Compare(token, "Hello"), 0, "Token avec délimiteurs multiples");
                
                // Test chaîne vide
                charb emptyStr[] = "";
                UNITEST_EQUAL(StringUtils::TokenizeString(emptyStr, ","), nullptr, "Chaîne vide");
            }

            /* Tests de duplication de chaînes */
            void StringDuplicationTests(const std::string&) {
                TestFullStringDuplication();
                TestPartialStringDuplication();
                TestSpecialCaseDuplication();
            }

            void TestFullStringDuplication() {
                const charb* src = "Duplicate me!";
                charb* dup = StringUtils::DuplicateString(src);
                UNITEST_EQUAL(Compare(dup, src), 0, "Duplication complète");
                StringUtils::FreeJoinStringsResult(dup);
            }

            void TestPartialStringDuplication() {
                const charb* src = "Partial";
                charb* dup = StringUtils::DuplicateBoundedString(src, 3);
                UNITEST_EQUAL(Compare(dup, "Par"), 0, "Duplication partielle");
                StringUtils::FreeJoinStringsResult(dup);
            }

            void TestSpecialCaseDuplication() {
                // Test nullptr
                UNITEST_EQUAL(StringUtils::DuplicateString<charb>(nullptr), nullptr, "Duplication nullptr");
                
                // Test zéro caractère
                charb* empty = StringUtils::DuplicateBoundedString("Test", 0);
                UNITEST_EQUAL(Compare(empty, ""), 0, "Duplication longueur zéro");
                StringUtils::FreeJoinStringsResult(empty);
            }

            /* Tests de conversion numérique */
            void NumericConversionTests(const std::string&) {
                TestIntegerConversions();
                TestFloatingPointConversions();
                TestBaseConversions();
                TestConversionErrors();
            }

            void TestIntegerConversions() {
                UNITEST_EQUAL(StringUtils::StringToInteger<charb>("-12345"), -12345, "Entier négatif");
                UNITEST_EQUAL(StringUtils::StringToInteger<charb>("+987"), 987, "Entier avec signe +");
            }

            void TestFloatingPointConversions() {
                UNITEST_APPROX(StringUtils::StringToFloat<charb>("3.14159"), 3.14159f, 0.00001f, "Float standard");
                UNITEST_APPROX(StringUtils::StringToDouble<charb>("-123.456e-2", nullptr), -1.23456, 0.00001, "Notation scientifique");
            }

            void TestBaseConversions() {
                charb* end;
                UNITEST_EQUAL(StringUtils::StringToIntegerBase<charb>("1A", &end, 16), 26, "Hexadécimal");
                UNITEST_EQUAL(StringUtils::StringToIntegerBase<charb>("77", &end, 8), 63, "Octal");
            }

            void TestConversionErrors() {
                charb* end;
                StringUtils::StringToIntegerBase<charb>("123abc", &end, 10);
                UNITEST_EQUAL(Compare(end, "abc"), 0, "Détection caractères invalides");
                
                UNITEST_EQUAL(StringUtils::StringToInteger<charb>("Invalid"), 0, "Conversion invalide");
            }

            /* Manipulation de chaînes */
            void StringManipulationTests(const std::string&) {
                TestStringReversal();
                TestCaseConversion();
                TestWhitespaceTrimming();
                TestStringReplacement();
                TestSubstringReplacement();
                TestStringSplitting();
                TestStringJoining();
            }

            void TestStringReversal() {
                charb str[] = "Hello";
                StringUtils::ReverseStringInPlace(str);
                UNITEST_EQUAL(Compare(str, "olleH"), 0, "Inversion basique");
                
                wchar wstr[] = NK_WIDE("Unicode");
                StringUtils::ReverseStringInPlace(wstr);
                UNITEST_EQUAL(Compare(wstr, NK_WIDE("edocinU")), 0, "Inversion Unicode");
            }

            void TestCaseConversion() {
                charb mixed[] = "HeLlO wOrLd";
                StringUtils::ToLower(mixed);
                UNITEST_EQUAL(Compare(mixed, "hello world"), 0, "Conversion minuscule");
                
                StringUtils::ToUpper(mixed);
                UNITEST_EQUAL(Compare(mixed, "HELLO WORLD"), 0, "Conversion majuscule");
            }

            void TestWhitespaceTrimming() {
                charb str[] = "  \tTest\n\r ";
                StringUtils::TrimWhitespace(str);
                UNITEST_EQUAL(Compare(str, "Test"), 0, "Nettoyage espaces");
            }

            void TestStringReplacement() {
                charb str[] = "a-b-c-d";
                StringUtils::ReplaceCharacters(str, '-', '_');
                UNITEST_EQUAL(Compare(str, "a_b_c_d"), 0, "Remplacement caractère");
            }

            void TestSubstringReplacement() {
                charb str[50] = "foo bar foo";
                StringUtils::ReplaceSubstring(str, "foo", "baz");
                UNITEST_EQUAL(Compare(str, "baz bar baz"), 0, "Remplacement sous-chaîne");
            }

            void TestStringSplitting() {
                usize count;
                charb** parts = StringUtils::SplitString("A,B,C", ',', &count);
                UNITEST_EQUAL(count, 3, "Nombre d'éléments split");
                UNITEST_EQUAL(Compare(parts[1], "B"), 0, "Vérification élément 2");
                StringUtils::FreeSplitStringResult(parts, count);
            }

            void TestStringJoining() {
                const charb* parts[] = {"A", "B", "C"};
                charb* joined = StringUtils::JoinStrings(parts, 3, '-');
                UNITEST_EQUAL(Compare(joined, "A-B-C"), 0, "Jonction simple");
                StringUtils::FreeJoinStringsResult(joined);
            }

            /* Classification de caractères */
            void CharacterClassificationTests(const std::string&) {
                TestWhitespaceDetection();
                TestNumericDetection();
                TestAlphaDetection();
                TestCaseDetection();
            }

            void TestWhitespaceDetection() {
                UNITEST_TRUE(StringUtils::IsWhitespace<charb>('\t'), "Tabulation");
                UNITEST_FALSE(StringUtils::IsWhitespace<wchar>(L'é'), "Caractère accentué");
            }

            void TestNumericDetection() {
                UNITEST_TRUE(StringUtils::IsDigit<charb>('5'), "Chiffre décimal");
                UNITEST_FALSE(StringUtils::IsDigit<char16>(u'A'), "Lettre hexa");
            }

            void TestAlphaDetection() {
                UNITEST_TRUE(StringUtils::IsAlpha<char32>(U'β'), "Lettre grecque");
                UNITEST_FALSE(StringUtils::IsAlpha<charb>('3'), "Chiffre");
            }

            void TestCaseDetection() {
                UNITEST_TRUE(StringUtils::IsUpper<wchar>(L'É'), "Majuscule accentuée");
                UNITEST_TRUE(StringUtils::IsLower<charb>('z'), "Minuscule");
            }

            /* Conversions numériques avancées */
            void NumericConversionAdvancedTests(const std::string&) {
                TestUnsignedConversions();
                TestFloatingPointPrecision();
                TestBaseConversions1();
                TestPowerFunctions();
            }

            void TestUnsignedConversions() {
                charb* end;
                UNITEST_EQUAL(StringUtils::StringToUint64<charb>("18446744073709551615", &end, 10), 
                            18446744073709551615ULL, "Max uint64");
            }

            void TestFloatingPointPrecision() {
                charb* end;
                UNITEST_APPROX(StringUtils::StringToDouble("3.141592653589793", &end), 
                            3.141592653589793L, 0.000000000000001L, "Précision étendue");
            }

            void TestBaseConversions1() {
                charb* end;
                UNITEST_EQUAL(StringUtils::CharToDigit('F', 16), 15, "Valeur hexadécimale");
                UNITEST_EQUAL(StringUtils::CharToDigit('7', 8), 7, "Valeur octale");
                NK_UNUSED end;
            }

            void TestPowerFunctions() {
                UNITEST_APPROX(StringUtils::Pow10(5), 100000.0, 0.1, "Puissance de 10");
                UNITEST_APPROX(StringUtils::Pow10L(-3), 0.001L, 0.0001L, "Puissance négative");
            }

            /* Opérations mémoire avancées */
            void MemoryOperationsAdvancedTests(const std::string&) {
                TestValueSwapping();
                TestInPlaceSwapping();
            }

            void TestValueSwapping() {
                int a = 5, b = 10;
                StringUtils::Swap(a, b);
                UNITEST_EQUAL(a, 10, "Swap standard");
            }

            void TestInPlaceSwapping() {
                uint32 x = 0xABCD, y = 0x1234;
                StringUtils::FastSwap(x, y);
                UNITEST_EQUAL(x, 0x1234, "Swap rapide");
            }
        };    

} // namespace nkentseu
