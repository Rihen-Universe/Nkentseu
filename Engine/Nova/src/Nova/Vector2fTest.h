// Vector2fTest.h
#pragma once

#include <Nkentseu/Nkentseu.h>
#include "Vector2f.h"
#include <Unitest/Unitest.h>

namespace nkentseu {

    class Vector2fTest {
    public:
        Vector2fTest() {
            // Enregistrement des tests
            UNITEST_REGISTRY(
                UnitestRegisterIClass(Vector2fTest::TestAll, "Tests complets pour Vector2f")
            );
        }

        void TestAll(const std::string& context) {
            TestAddition();
            TestSubtraction();
            TestScalarMultiplication();
            TestDotProduct();
            TestMagnitude();
            TestNormalization();
            TestEdgeCases();
        }

    private:
        void TestAddition() {
            Vector2f v1(2.0f, 3.0f);
            Vector2f v2(1.0f, 2.0f);
            Vector2f sum = v1 + v2;

            UNITEST_EQUAL(sum.x, 3.0f, "Addition composant X");
            UNITEST_EQUAL(sum.y, 5.0f, "Addition composant Y");
        }

        void TestSubtraction() {
            Vector2f v1(5.0f, 4.0f);
            Vector2f v2(2.0f, 1.0f);
            Vector2f diff = v1 - v2;

            UNITEST_EQUAL(diff.x, 3.0f, "Soustraction composant X");
            UNITEST_EQUAL(diff.y, 3.0f, "Soustraction composant Y");
        }

        void TestScalarMultiplication() {
            Vector2f v(2.0f, 3.0f);
            Vector2f scaled = v * 1.5f;

            UNITEST_APPROX(scaled.x, 3.0f, 0.001f, "Multiplication scalaire X");
            UNITEST_APPROX(scaled.y, 4.5f, 0.001f, "Multiplication scalaire Y");
        }

        void TestDotProduct() {
            Vector2f v1(1.0f, 0.0f);
            Vector2f v2(0.0f, 1.0f);
            Vector2f v3(2.0f, 3.0f);

            UNITEST_EQUAL(v1.Dot(v2), 0.0f, "Produit scalaire perpendiculaire");
            UNITEST_EQUAL(v3.Dot(v3), 13.0f, "Produit scalaire même vecteur");
        }

        void TestMagnitude() {
            Vector2f v(3.0f, 4.0f);
            UNITEST_APPROX(v.Magnitude(), 5.0f, 0.001f, "Magnitude normale");
            
            Vector2f zero(0.0f, 0.0f);
            UNITEST_EQUAL(zero.Magnitude(), 0.0f, "Magnitude nulle");
        }

        void TestNormalization() {
            Vector2f v(3.0f, 4.0f);
            Vector2f norm = v.Normalized();
            const float epsilon = 0.0001f;

            UNITEST_APPROX(norm.Magnitude(), 1.0f, epsilon, "Magnitude normalisée");
            UNITEST_APPROX(norm.x, 0.6f, epsilon, "Normalisation X");
            UNITEST_APPROX(norm.y, 0.8f, epsilon, "Normalisation Y");
        }

        void TestEdgeCases() {
            // Test de la normalisation du vecteur nul
            Vector2f zero(0.0f, 0.0f);
            Vector2f zeroNorm = zero.Normalized();
            
            UNITEST_TRUE(zeroNorm.x == 0.0f && zeroNorm.y == 0.0f, 
                       "Normalisation du vecteur nul");
            
            // Test d'égalité approximative avec valeurs limites
            Vector2f v1(0.00001f, -0.00001f);
            UNITEST_APPROX(v1.x, 0.0f, 0.0001f, "Valeur limite X");
            UNITEST_APPROX(v1.y, 0.0f, 0.0001f, "Valeur limite Y");
        }
    };

    // Instance globale pour l'enregistrement automatique
    // Vector2fTest vector2fTest;

} // namespace nkentseu