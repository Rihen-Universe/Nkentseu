// // StringTest.h
// #pragma once

// #include "Nkentseu/Nkentseu.h"
// #include "String.h"
// #include <Unitest/Unitest.h>

// namespace nkentseu {

// class StringTest {
// public:
//     StringTest() {
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestConstructeurs, "Tests des constructeurs")
//         );
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestAffectation, "Tests des affectations")
//         );
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestAccesseurs, "Tests des accesseurs")
//         );
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestCapacite, "Tests de capacité mémoire")
//         );
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestModificateurs, "Tests des modificateurs")
//         );
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestOperationsChaine, "Tests des opérations chaînées")
//         );
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestComparaisons, "Tests des comparaisons")
//         );
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestConversions, "Tests des conversions")
//         );
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestIterateurs, "Tests des itérateurs")
//         );
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestCasLimites, "Tests des cas limites")
//         );
//         UNITEST_REGISTRY(
//             UnitestRegisterIClass(StringTest::TestUnicode, "Tests Unicode")
//         );
//     }

// private:
//     // Helper pour générer de longues chaînes
//     String GenerateLongString(usize length) {
//         String result;
//         for(usize i = 0; i < length; ++i) {
//             result.Append('A' + (i % 26));
//         }
//         return result;
//     }

//     void TestConstructeurs(const std::string& context) {
//         // 1. Constructeur par défaut
//         String s1;
//         UNITEST_TRUE(s1.Empty(), "Constructeur par défaut");
        
//         // 2. Constructeur depuis C-string
//         String s2("Hello");
//         UNITEST_EQUAL(s2, "Hello", "Constructeur C-string");
        
//         // 3. Constructeur avec répétition
//         String s3(5, 'X');
//         UNITEST_EQUAL(s3, "XXXXX", "Constructeur répétition");
        
//         // 4. Constructeur de copie
//         String s4(s2);
//         UNITEST_EQUAL(s4, "Hello", "Constructeur copie");
        
//         // 5. Constructeur de déplacement
//         String s5(std::move(s4));
//         UNITEST_EQUAL(s5, "Hello", "Constructeur déplacement");
//         UNITEST_TRUE(s4.Empty(), "Original vidé après déplacement");
        
//         // 6. Constructeur depuis std::string
//         std::string stdstr = "Test";
//         String s6(stdstr);
//         UNITEST_EQUAL(s6, "Test", "Constructeur depuis std::string");
//     }

//     void TestAffectation(const std::string& context) {
//         String s1;
        
//         // 1. Affectation depuis C-string
//         s1 = "Hello";
//         UNITEST_EQUAL(s1, "Hello", "Affectation C-string");
        
//         // 2. Affectation par copie
//         String s2;
//         s2 = s1;
//         UNITEST_EQUAL(s2, "Hello", "Affectation copie");
        
//         // 3. Affectation par déplacement
//         String s3;
//         s3 = std::move(s2);
//         UNITEST_EQUAL(s3, "Hello", "Affectation déplacement");
//         UNITEST_TRUE(s2.Empty(), "Original vidé après déplacement");
        
//         // 4. Auto-affectation
//         s3 = s3;
//         UNITEST_EQUAL(s3, "Hello", "Auto-affectation");
        
//         // 5. Affectation longue chaîne (dépassement SSO)
//         String s4;
//         s4 = GenerateLongString(100).Data();
//         UNITEST_EQUAL(s4.Length(), 100, "Affectation longue chaîne");
//     }

//     void TestAccesseurs(const std::string& context) {
//         String s("Hello");
        
//         // 1. Accès index valide
//         UNITEST_EQUAL(s[0], 'H', "Accès index 0");
//         UNITEST_EQUAL(s[4], 'o', "Accès index 4");
        
//         // 2. Front/Back
//         UNITEST_EQUAL(s.Front(), 'H', "Front()");
//         UNITEST_EQUAL(s.Back(), 'o', "Back()");
        
//         // 3. Modification via []
//         s[1] = 'a';
//         UNITEST_EQUAL(s, "Hallo", "Modification via []");
        
//         // 4. Accès const
//         const String cs("Test");
//         UNITEST_EQUAL(cs[2], 's', "Accès const []");
        
//         // 5. Conversion explicite
//         UNITEST_EQUAL(strcmp((const char*)cs, "Test"), 0, "Conversion const char*");
//     }

//     void TestCapacite(const std::string& context) {
//         // 1. SSO capacity
//         String sso = "Short";
//         UNITEST_TRUE(sso.IsSso(), "SSO activé");
        
//         // 2. Heap allocation
//         String heap = GenerateLongString(100);
//         UNITEST_FALSE(heap.IsSso(), "Allocation heap");
        
//         // 3. Reserve
//         String s;
//         s.Reserve(50);
//         UNITEST_TRUE(s.Capacity() >= 50, "Reserve capacité");
        
//         // 4. Shrink to fit
//         String s2 = GenerateLongString(100);
//         s2.Resize(10);
//         s2.Reserve(0);
//         UNITEST_TRUE(s2.Capacity() <= 15, "Shrink to fit");
        
//         // 5. Resize avec remplissage
//         String s3;
//         s3.Resize(5, 'Z');
//         UNITEST_EQUAL(s3, "ZZZZZ", "Resize avec remplissage");
//     }

//     void TestModificateurs(const std::string& context) {
//         String s;
        
//         // 1. Append caractère
//         s.Append('H');
//         UNITEST_EQUAL(s, "H", "Append caractère");
        
//         // 2. Append C-string
//         s.Append("ello");
//         UNITEST_EQUAL(s, "Hello", "Append C-string");
        
//         // 3. Insert milieu
//         s.Insert(3, "p");
//         UNITEST_EQUAL(s, "Helplo", "Insert milieu");
        
//         // 4. Insert début
//         s.Insert(0, "Start ");
//         UNITEST_EQUAL(s, "Start Helplo", "Insert début");
        
//         // 5. Erase plage
//         s.Erase(5, 4);
//         UNITEST_EQUAL(s, "Start plo", "Erase plage");
        
//         // 6. Replace
//         s.Replace(6, 2, "lo World");
//         UNITEST_EQUAL(s, "Start Hello World", "Replace");
//     }

//     void TestOperationsChaine(const std::string& context) {
//         String s = "Hello world, welcome to C++";
        
//         // 1. Find sous-chaîne
//         UNITEST_EQUAL(s.Find("world"), 6, "Find sous-chaîne");
        
//         // 2. RFind caractère
//         UNITEST_EQUAL(s.RFind('o'), 22, "RFind caractère");
        
//         // 3. Substr
//         UNITEST_EQUAL(s.Substr(7, 5), "world", "Substr");
        
//         // 4. StartsWith/EndsWith
//         UNITEST_TRUE(s.StartsWith("Hello"), "StartsWith");
//         UNITEST_TRUE(s.EndsWith("C++"), "EndsWith");
        
//         // 5. Contains
//         UNITEST_TRUE(s.Contains("welcome"), "Contains");
        
//         // 6. Compare
//         String s2 = "Hello";
//         UNITEST_EQUAL(s2.Compare("Hello"), 0, "Compare égalité");
//         UNITEST_EQUAL(s2.Compare("Helln"), 1, "Compare supérieur");
//     }

//     void TestComparaisons(const std::string& context) {
//         // 1. Égalité
//         UNITEST_TRUE(String("Test") == "Test", "Opérateur == vrai");
//         UNITEST_FALSE(String("Test") == "test", "Opérateur == faux");
        
//         // 2. Inégalité
//         UNITEST_TRUE(String("A") != "B", "Opérateur != vrai");
//         UNITEST_FALSE(String("A") != "A", "Opérateur != faux");
        
//         // 3. Inférieur
//         UNITEST_TRUE(String("Apple") < "Banana", "Opérateur <");
        
//         // 4. Supérieur
//         UNITEST_TRUE(String("Zebra") > "Apple", "Opérateur >");
        
//         // 5. Comparaison sensible à la casse
//         UNITEST_TRUE(String("abc") < "abd", "Comparaison lexicographique");
//         UNITEST_TRUE(String("ABC") < "abc", "Sensibilité à la casse");
//     }

//     void TestConversions(const std::string& context) {
//         // 1. UTF-8 vers UTF-16
//         String utf8 = u8"Élément €";
//         String16 utf16 = utf8.ConvertTo<char16>();
//         UNITEST_EQUAL(utf16.Length(), 9, "Conversion UTF-8 → UTF-16");
        
//         // 2. UTF-16 vers UTF-8
//         String utf8bis = utf16.ConvertTo<char8>();
//         UNITEST_EQUAL(utf8bis, utf8, "Round-trip UTF-8");
        
//         // 3. Vers std::string
//         std::string stdstr = utf8;
//         UNITEST_EQUAL(stdstr, "Élément €", "Conversion vers std::string");
        
//         // 4. Depuis wchar_t
//         String ws = L"WideString";
//         UNITEST_EQUAL(ws.Length(), 10, "Conversion depuis wchar_t");
        
//         // 5. Caractères spéciaux
//         String emoji = u8"😊⭐🎉";
//         String32 emoji32 = emoji.ConvertTo<char32>();
//         UNITEST_EQUAL(emoji32.Length(), 3, "Conversion émoticônes");
//     }

//     void TestIterateurs(const std::string& context) {
//         String s = "ABCDE";
        
//         // 1. Itération avant
//         usize sum = 0;
//         for(auto c : s) sum += c;
//         UNITEST_EQUAL(sum, 'A'+'B'+'C'+'D'+'E', "Itération avant");
        
//         // 2. Itération inverse
//         String reversed;
//         for(auto it = s.rbegin(); it != s.rend(); ++it) {
//             reversed.Append(*it);
//         }
//         UNITEST_EQUAL(reversed, "EDCBA", "Itération inverse");
        
//         // 3. Modification via itérateur
//         auto it = s.begin();
//         *it = 'X';
//         UNITEST_EQUAL(s, "XBCDE", "Modification via itérateur");
        
//         // 4. Comparaison d'itérateurs
//         auto it1 = s.begin();
//         auto it2 = s.begin();
//         UNITEST_TRUE(it1 == it2, "Comparaison itérateurs égaux");
        
//         // 5. Distance d'itérateurs
//         UNITEST_EQUAL(s.end() - s.begin(), 5, "Distance itérateurs");
//     }

//     void TestCasLimites(const std::string& context) {
//         // 1. Chaîne vide
//         String s;
//         UNITEST_TRUE(s.Empty(), "Chaîne vide");
//         s.Append("test");
//         UNITEST_EQUAL(s.Length(), 4, "Ajout à chaîne vide");
        
//         // 2. Capacité maximale SSO
//         String ssoMax(String::SSO_CAPACITY, 'x');
//         UNITEST_TRUE(ssoMax.IsSso(), "SSO à capacité max");
//         ssoMax.Append('y');
//         UNITEST_FALSE(ssoMax.IsSso(), "Dépassement SSO");
        
//         // 3. Positions invalides
//         String s2 = "test";
//         UNITEST_EQUAL(s2.Find('z'), String::npos, "Recherche échouée");
//         UNITEST_EQUAL(s2.Substr(10, 2), "", "Substr hors limites");
        
//         // 4. Auto-remplacement
//         s2.Replace(0, 4, s2);
//         UNITEST_EQUAL(s2, "test", "Auto-remplacement");
        
//         // 5. Performances grandes chaînes
//         String big = GenerateLongString(100000);
//         UNITEST_EQUAL(big.Length(), 100000, "Chaîne de 100k caractères");
//         UNITEST_TRUE(big.Find("XYZ") == String::npos, "Recherche grande chaîne");
//     }

//     void TestUnicode(const std::string& context) {
//         // 1. Validation UTF-8
//         String valid = u8"café";
//         String invalid("\xF0\x82\x82"); // Séquence invalide
//         UNITEST_TRUE(valid.IsValidUtf8(), "UTF-8 valide");
//         UNITEST_FALSE(invalid.IsValidUtf8(), "UTF-8 invalide");
        
//         // 2. Normalisation
//         String accented = u8"éèêàù";
//         String16 accented16 = accented.ConvertTo<char16>();
//         UNITEST_EQUAL(accented16.Length(), 5, "Conversion caractères accentués");
        
//         // 3. Caractères multi-octets
//         String emoji = u8"😊";
//         UNITEST_EQUAL(emoji.Length(), 4, "Longueur émoticône UTF-8");
        
//         // 4. Combinaison de caractères
//         String combining = u8"A\u0308"; // Ä combiné
//         String normalized = u8"Ä";
//         // Note: Implémenter une logique de normalisation si nécessaire
//         UNITEST_EQUAL(combining.ConvertTo<char32>(), normalized.ConvertTo<char32>(),
//                      "Équivalence de normalisation");
        
//         // 5. Performances Unicode
//         String longUnicode = GenerateLongString(50000) + u8"éèçà";
//         String16 longUnicode16 = longUnicode.ConvertTo<char16>();
//         UNITEST_EQUAL(longUnicode16.Length(), 50004, "Conversion longue chaîne Unicode");
//     }
// };

// } // namespace nkentseu