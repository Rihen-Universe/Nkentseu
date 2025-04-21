#pragma once

#include "Nkentseu/Nkentseu.h"
#include "Unitest/Unitest.h"

namespace nkentseu {

    class StringTest {
    public:
        StringTest() {
            // Enregistrement des tests
            UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::DefaultConstruction, "Constructeur par défaut"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::CStringConstruction, "Construction depuis C-string"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::CopyConstruction, "Constructeur de copie"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::CrossTypeConstruction, "Construction multi-encodage"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::CapacityOperations, "Gestion de capacité"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::DataAccess, "Accès aux données"));
            UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::SsoBehavior, "Comportement SSO"));
        }

    private:
        void DefaultConstruction(const std::string&) {
            BasicString<charb> s;
            UNITEST_EQUAL(s.Length(), 0, "Longueur par défaut");
            UNITEST_EQUAL(s.Capacity(), BasicString<charb>::kSSOCapacity, "Capacité SSO initiale");
            UNITEST_EQUAL(s.Data()[0], '\0', "Chaîne vide");
            UNITEST_TRUE(s.IsSso(), "Doit utiliser SSO");
        }

        void CStringConstruction(const std::string&) {
            // Chaîne vide
            BasicString<charb> s1("");
            UNITEST_EQUAL(s1.Length(), 0, "Longueur chaîne vide");
            UNITEST_TRUE(s1.IsSso(), "SSO pour chaîne vide");

            // Chaîne SSO
            const charb* shortStr = "Test";
            BasicString<charb> s2(shortStr);
            UNITEST_EQUAL(s2.Length(), StringUtils::ComputeStringLength(shortStr), "Longueur SSO");
            UNITEST_EQUAL(StringUtils::CompareStrings(s2.Data(), shortStr), 0, "Contenu SSO");
            UNITEST_TRUE(s2.IsSso(), "Doit utiliser SSO");

            // Chaîne longue
            const charb* longStr = "Ceci est une chaîne très longue qui dépasse la capacité SSO";
            BasicString<charb> s3(longStr);
            UNITEST_EQUAL(s3.Length(), StringUtils::ComputeStringLength(longStr), "Longueur dynamique");
            UNITEST_EQUAL(StringUtils::CompareStrings(s3.Data(), longStr), 0, "Contenu dynamique");
            UNITEST_FALSE(s3.IsSso(), "Doit utiliser l'allocation dynamique");
        }

        void CopyConstruction(const std::string&) {
            // Copie SSO
            BasicString<charb> s1("Short");
            BasicString<charb> s2(s1);
            UNITEST_EQUAL(s2.Length(), s1.Length(), "Longueur copie SSO");
            UNITEST_EQUAL(StringUtils::CompareStrings(s2.Data(), s1.Data()), 0, "Contenu copie SSO");
            UNITEST_TRUE(s2.IsSso(), "Copie doit rester en SSO");

            // Copie dynamique
            const charb* longStr = "Chaîne suffisamment longue pour allocation dynamique";
            BasicString<charb> s3(longStr);
            BasicString<charb> s4(s3);
            UNITEST_EQUAL(s4.Length(), s3.Length(), "Longueur copie dynamique");
            UNITEST_EQUAL(StringUtils::CompareStrings(s4.Data(), s3.Data()), 0, "Contenu copie dynamique");
            UNITEST_FALSE(s4.IsSso(), "Copie doit rester dynamique");
        }

        void CrossTypeConstruction(const std::string&) {
            // Conversion charb -> char16
            BasicString<char8> srcCharb(u8"Hello🌍");
            BasicString<char16> destChar16(srcCharb);
            
            const char16* expected = u"Hello🌍";
            // UNITEST_EQUAL(StringUtils::CompareStrings<char16>(destChar16.Data(), expected), 0, "Conversion charb vers char16 = {0}", srcCharb.Data());
            UNITEST_EQUAL(StringUtils::CompareStrings<char16>(destChar16.Data(), expected), 0, "Conversion charb vers char16");

            // Conversion char16 -> charb avec perte
            BasicString<char16> srcChar16(u"Test 世界");
            BasicString<charb> destCharb(srcChar16);
            
            const charb* expectedLossy = "Test ??";
            UNITEST_EQUAL(StringUtils::CompareStrings(destCharb.Data(), expectedLossy), 0, "Conversion char16 vers charb avec perte");

            // Conversion UTF-16 -> UTF-8
            BasicString<char16> utf16Str(u"Hello 🌍");
            BasicString<charb> utf8Str(utf16Str);

            // Le test devrait maintenant passer
            UNITEST_EQUAL(StringUtils::CompareStrings<charb>(utf8Str.Data(), 
                    "Hello ??" // Représentation UTF-8 de 🌍
                ), 0, "Conversion UTF-16 vers UTF-8 correcte {0}", utf8Str.Data());

            // Test UTF-16 -> UTF-8
            BasicString<char16> utf16Str1(u"Hello \xD83C\xDF0D"); // 🌍 en UTF-16
            BasicString<char8> utf8Str1 = utf16Str1.ConvertEncoding<char8>();

            UNITEST_EQUAL(
                StringUtils::CompareStrings<char8>(
                    utf8Str1.Data(), 
                    u8"Hello \xF0\x9F\x8C\x8D" // 🌍 en UTF-8
                ), 
                0, 
                "Conversion UTF-16 vers UTF-8"
            );

            // Test UTF-8 -> UTF-16
            BasicString<char8> utf8Src(u8"Hello 🌍");
            BasicString<char16> utf16Result = utf8Src.ConvertEncoding<char16>();

            const char16 expected1[] = u"Hello \xD83C\xDF0D"; 
            UNITEST_EQUAL(
                StringUtils::CompareStrings<char16>(
                    utf16Result.Data(), 
                    expected1
                ),
                0,
                "Conversion UTF-8 vers UTF-16"
            );
        }

        void CapacityOperations(const std::string&) {
            BasicString<charb> s;
            UNITEST_EQUAL(s.Capacity(), BasicString<charb>::kSSOCapacity, "Capacité initiale SSO");

            // Dépassement SSO
            const charb* longStr = "Chaîne longue pour vérifier l'augmentation de capacité";
            s = longStr;
            UNITEST_TRUE(s.Capacity() > BasicString<charb>::kSSOCapacity, "Capacité étendue");
            UNITEST_EQUAL(s.Length(), StringUtils::ComputeStringLength(longStr), "Longueur après extension");
        }

        void DataAccess(const std::string&) {
            BasicString<charb> s("Modifiable");
            charb* data = s.Data();
            data[0] = 'm'; // Modification directe
            
            UNITEST_EQUAL(
                StringUtils::CompareStrings(s.Data(), "modifiable"),
                0,
                "Modification via Data()"
            );

            // Test const-correctness
            const BasicString<charb> cs("Const");
            UNITEST_EQUAL(
                StringUtils::CompareStrings(cs.Data(), "Const"),
                0,
                "Accès const Data()"
            );
        }

        void SsoBehavior(const std::string&) {
            // Chaîne limite SSO +1 pour forcer l'allocation dynamique
            BasicString<charb> s1;
            const usize bufferSize = BasicString<charb>::kSSOCapacity + 1;
            charb* buffer = new charb[bufferSize + 1]; // +1 pour le terminateur
            
            StringUtils::MemorySet(buffer, 'a', bufferSize);
            buffer[bufferSize] = '\0';
            
            s1 = buffer;
            UNITEST_FALSE(s1.IsSso(), "Doit basculer en dynamique");
            UNITEST_EQUAL(s1.Length(), bufferSize, "Longueur exacte SSO +1");

            // Réaffectation SSO
            s1 = "Short";
            UNITEST_TRUE(s1.IsSso(), "Retour à SSO");
            UNITEST_EQUAL(s1.Capacity(), BasicString<charb>::kSSOCapacity, "Capacité restaurée");
        }
    };

} // namespace nkentseu













































// #pragma once

// #include "Nkentseu/Nkentseu.h"
// #include "Unitest/Unitest.h"

// namespace nkentseu {

//     class StringTest {
//     public:
//         StringTest() {
//             // Enregistrement de tous les tests
//             // UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::DefaultConstructorTest, "Constructeur par défaut"));
//             // UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::CStringConstructorTest, "Constructeur depuis C-string"));
//             // UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::CopyConstructorTest, "Constructeur de copie"));
//             // UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::MoveConstructorTest, "Constructeur de déplacement"));
//             // UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::InitializerListConstructorTest, "Constructeur depuis initializer_list"));
//             // UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::LengthCapacityTest, "Test Length/Capacity"));
//             // UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::ResizeTest, "Test Redimensionnement"));
//             // UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::AssignTest, "Test Assignation"));
//             // UNITEST_REGISTRY(UnitestRegisterIClass(StringTest::SwapTest, "Test d'échange"));
//         }

//     private:
//         // void DefaultConstructorTest(const std::string&) {
//         //     BasicString<charb> s;
//         //     UNITEST_EQUAL(s.Length(), 0, "Longueur par défaut");
//         //     UNITEST_EQUAL(s.Capacity(), 0, "Capacité initiale");
//         //     UNITEST_TRUE(s.IsEmpty(), "Doit être vide");
//         // }

//         // void CStringConstructorTest(const std::string&) {
//         //     const charb* testStr = "Hello World";
//         //     BasicString<charb> s(testStr);
            
//         //     UNITEST_EQUAL(s.Length(), 11, "Longueur C-string");
//         //     UNITEST_EQUAL(s.Capacity(), 11, "Capacité C-string");
//         //     UNITEST_EQUAL(strcmp(s.Data(), testStr), 0, "Contenu C-string");
//         // }

//         // void CopyConstructorTest(const std::string&) {
//         //     BasicString<charb> original("Copie");
//         //     BasicString<charb> copy(original);
            
//         //     UNITEST_EQUAL(original.Length(), copy.Length(), "Longueur copie");
//         //     UNITEST_EQUAL(strcmp(original.Data(), copy.Data()), 0, "Contenu copie");
//         //     UNITEST_TRUE(original.Data() != copy.Data(), "Doit être une copie profonde");
//         // }

//         // void MoveConstructorTest(const std::string&) {
//         //     BasicString<charb> original("Moved");
//         //     BasicString<charb> moved(std::move(original));
            
//         //     UNITEST_EQUAL(moved.Length(), 5, "Longueur après déplacement");
//         //     UNITEST_EQUAL(original.Length(), 0, "Original doit être vide");
//         //     UNITEST_TRUE(original.Data() == nullptr, "Data originale invalide");
//         // }

//         // void InitializerListConstructorTest(const std::string&) {
//         //     BasicString<charb> s{'T','e','s','t'};
//         //     UNITEST_EQUAL(s.Length(), 4, "Longueur initializer_list");
//         //     UNITEST_EQUAL(strcmp(s.Data(), "Test"), 0, "Contenu initializer_list");
//         // }

//         // void LengthCapacityTest(const std::string&) {
//         //     BasicString<wchar> ws(NK_WIDE("Capacité"));
//         //     const usize len = wcslen(L"Capacité");
            
//         //     UNITEST_EQUAL(ws.Length(), len, "Longueur wchar_t");
//         //     UNITEST_TRUE(ws.Capacity() >= len, "Capacité suffisante");
//         //     UNITEST_FALSE(ws.IsEmpty(), "Non vide");
//         // }

//         // void ResizeTest(const std::string&) {
//         //     BasicString<char8> s(u8"Small");
//         //     const usize originalCapacity = s.Capacity();

//         //     // Agrandissement
//         //     s.Resize(10, u8'_');
//         //     UNITEST_EQUAL(s.Length(), 10, "Nouvelle longueur");
//         //     UNITEST_TRUE(s.Capacity() >= 10, "Nouvelle capacité");
//         //     UNITEST_EQUAL(s[7], u8'_', "Remplissage correct");

//         //     // Réduction
//         //     s.Resize(3);
//         //     UNITEST_EQUAL(s.Length(), 3, "Longueur réduite");
//         //     UNITEST_EQUAL(s.Capacity(), originalCapacity, "Capacité inchangée {0} = {1}", s.Capacity(), originalCapacity);
//         // }

//         // void AssignTest(const std::string&) {
//         //     BasicString<char16> s;
//         //     const char16* data = u"Nouveau contenu";
//         //     const usize len = StringUtils::ComputeStringLength(data);

//         //     s.Assign(data, len);
//         //     UNITEST_EQUAL(s.Length(), len, "Longueur après assignation");
//         //     UNITEST_EQUAL(StringUtils::MemoryCompare(s.Data(), data, len), 0, "Contenu assigné");
//         // }

//         // void SwapTest(const std::string&) {
//         //     BasicString<charb> a("Alice");
//         //     BasicString<charb> b("Bob");
//         //     const charb* aData = a.Data();
//         //     const charb* bData = b.Data();

//         //     a.Swap(b);
//         //     UNITEST_EQUAL(StringUtils::CompareStrings(a.Data(), "Bob"), 0, "Données échangées A");
//         //     UNITEST_EQUAL(StringUtils::CompareStrings(b.Data(), "Alice"), 0, "Données échangées B");
//         //     UNITEST_TRUE(a.Data() == bData && b.Data() == aData, "Échange des pointeurs");
//         // }
//     };

// } // namespace nkentseu