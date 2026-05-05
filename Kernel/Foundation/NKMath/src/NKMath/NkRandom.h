//
// NkRandom.h
// =============================================================================
// Description :
//   Singleton de génération de nombres pseudo-aléatoires pour le moteur.
//   Fournit une API unifiée pour générer des valeurs aléatoires de types
//   variés : entiers (signés/non-signés), flottants, vecteurs, matrices,
//   couleurs, quaternions, etc.
//
// Backend :
//   Utilise la fonction standard rand() du C, suffisante pour la plupart
//   des usages jeux vidéo (non-cryptographiques).
//
// Thread-safety :
//   Ce singleton n'est PAS thread-safe. Pour un usage multi-thread :
//   - Créer un générateur thread-local par thread
//   - Ou utiliser un mutex externe pour protéger les appels
//
// Usage recommandé :
//   - Via le singleton : NkRandom::Instance().NextFloat32()
//   - Via l'alias global : NkRand.NextFloat32() (plus concis)
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_RANDOM_H__
#define __NKENTSEU_RANDOM_H__

    // =====================================================================
    // Inclusions des dépendances du projet
    // =====================================================================
    #include "NKMath/NkLegacySystem.h"      // Types fondamentaux : uint8, int32, float32, etc.
    #include "NKMath/NkRange.h"             // Type NkRangeT pour les intervalles
    #include "NKMath/NkVec.h"               // Types vectoriels : NkVec2T, NkVec3T, NkVec4T
    #include "NKMath/NkMat.h"               // Types matriciels : NkMat2T, NkMat3T, NkMat4T
    #include "NKMath/NkQuat.h"              // Type NkQuatT pour les rotations
    #include "NKMath/NkColor.h"             // Types NkColor et NkHSV pour les couleurs

    // Inclusions des bibliothèques standard C/C++
    #include <cstdlib>                      // Fonctions rand(), srand(), RAND_MAX
    #include <ctime>                        // Fonction time() pour le seeding

    // =====================================================================
    // Namespace principal du projet
    // =====================================================================
    namespace nkentseu {

        // =================================================================
        // Sous-namespace mathématique : utilitaires mathématiques
        // =================================================================
        namespace math {

            // =================================================================
            // Classe : NkRandom
            // =================================================================
            // Singleton global pour la génération de nombres pseudo-aléatoires.
            //
            // Caractéristiques :
            //   - Seed initialisé automatiquement à la construction (time(nullptr))
            //   - API riche pour générer tous les types mathématiques du projet
            //   - Méthodes surchargées pour flexibilité d'usage (min/max, range, limit)
            //
            // Attention :
            //   rand() produit une séquence déterministe pour un seed donné.
            //   Pour reproduire un comportement, appeler srand(seed) manuellement
            //   avant tout usage de NkRandom.
            // =================================================================
            class NkRandom {

                // -----------------------------------------------------------------
                // Section : Membres publics (interface du singleton)
                // -----------------------------------------------------------------
                public:

                    // -----------------------------------------------------------------
                    // Sous-section : Accès au singleton (Meyers' Singleton)
                    // -----------------------------------------------------------------
                    // Instance : retourne la référence unique vers le singleton
                    // Implémentation thread-safe en C++11+ (initialisation statique locale)
                    static NkRandom& Instance() noexcept
                    {
                        static NkRandom singletonInstance;
                        return singletonInstance;
                    }

                    // Suppression du constructeur de copie (singleton non copiable)
                    NkRandom(const NkRandom&) = delete;

                    // Suppression de l'opérateur d'affectation (singleton non assignable)
                    NkRandom& operator=(const NkRandom&) = delete;

                    // -----------------------------------------------------------------
                    // Sous-section : Génération d'entiers non-signés 8 bits (uint8)
                    // -----------------------------------------------------------------
                    // NextUInt8() : retourne un uint8 aléatoire dans [0, 255]
                    uint8 NextUInt8() noexcept
                    {
                        return static_cast<uint8>(rand()) % 256;
                    }

                    // NextUInt8(max) : retourne un uint8 dans [0, max[
                    uint8 NextUInt8(uint8 maximum) noexcept
                    {
                        if (maximum != 0) {
                            return NextUInt8() % maximum;
                        }
                        return 0;
                    }

                    // NextUInt8(min, max) : retourne un uint8 dans [min, max[
                    uint8 NextUInt8(uint8 minimum, uint8 maximum) noexcept
                    {
                        return NextUInt8(NkRangeUInt(minimum, maximum));
                    }

                    // NextUInt8(range) : retourne un uint8 dans l'intervalle donné
                    uint8 NextUInt8(const NkRangeUInt& range) noexcept
                    {
                        uint32 rangeLength = range.Len();
                        if (rangeLength != 0) {
                            return static_cast<uint8>((NextUInt8() % rangeLength) + range.GetMin());
                        }
                        return range.GetMin();
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Génération d'entiers non-signés 32 bits (uint32)
                    // -----------------------------------------------------------------
                    // NextUInt32() : retourne un uint32 aléatoire dans [0, RAND_MAX]
                    uint32 NextUInt32() noexcept
                    {
                        return static_cast<uint32>(rand());
                    }

                    // NextUInt32(max) : retourne un uint32 dans [0, max[
                    uint32 NextUInt32(uint32 maximum) noexcept
                    {
                        if (maximum != 0u) {
                            return NextUInt32() % maximum;
                        }
                        return 0u;
                    }

                    // NextUInt32(min, max) : retourne un uint32 dans [min, max[
                    uint32 NextUInt32(uint32 minimum, uint32 maximum) noexcept
                    {
                        return NextUInt32(NkRangeUInt(minimum, maximum));
                    }

                    // NextUInt32(range) : retourne un uint32 dans l'intervalle donné
                    uint32 NextUInt32(const NkRangeUInt& range) noexcept
                    {
                        uint32 rangeLength = range.Len();
                        if (rangeLength != 0u) {
                            return (NextUInt32() % rangeLength) + range.GetMin();
                        }
                        return range.GetMin();
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Génération d'entiers signés 32 bits (int32)
                    // -----------------------------------------------------------------
                    // NextInt32() : retourne un int32 dans [-RAND_MAX/2, RAND_MAX/2]
                    int32 NextInt32() noexcept
                    {
                        uint32 rawValue = NextUInt32() % static_cast<uint32>(RAND_MAX + 1u);
                        return static_cast<int32>(rawValue) - (RAND_MAX / 2);
                    }

                    // NextInt32(limit) : retourne un int32 dans [-limit/2, limit/2[
                    int32 NextInt32(int32 limit) noexcept
                    {
                        int32 value = NextInt32();
                        if (limit != 0) {
                            return value % limit;
                        }
                        return 0;
                    }

                    // NextInt32(min, max) : retourne un int32 dans [min, max[
                    int32 NextInt32(int32 minimum, int32 maximum) noexcept
                    {
                        return NextInt32(NkRangeInt(minimum, maximum));
                    }

                    // NextInt32(range) : retourne un int32 dans l'intervalle donné
                    int32 NextInt32(const NkRangeInt& range) noexcept
                    {
                        int32 rangeLength = range.Len();
                        if (rangeLength != 0) {
                            int32 offset = static_cast<int32>(NextUInt32()) % rangeLength;
                            return offset + range.GetMin();
                        }
                        return range.GetMin();
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Génération de flottants 32 bits (float32)
                    // -----------------------------------------------------------------
                    // NextFloat32() : retourne un float32 dans [0.0, 1.0[
                    float32 NextFloat32() noexcept
                    {
                        return static_cast<float32>(rand()) / static_cast<float32>(RAND_MAX);
                    }

                    // NextFloat32(limit) : retourne un float32 dans [0.0, limit[
                    float32 NextFloat32(float32 limit) noexcept
                    {
                        return NextFloat32() * limit;
                    }

                    // NextFloat32(min, max) : retourne un float32 dans [min, max[
                    float32 NextFloat32(float32 minimum, float32 maximum) noexcept
                    {
                        return NextFloat32(NkRangeFloat(minimum, maximum));
                    }

                    // NextFloat32(range) : retourne un float32 dans l'intervalle donné
                    float32 NextFloat32(const NkRangeFloat& range) noexcept
                    {
                        return NextFloat32() * range.Len() + range.GetMin();
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Requêtes sur les limites du générateur
                    // -----------------------------------------------------------------
                    // MaxUInt : retourne la valeur maximale générable pour uint32
                    uint32 MaxUInt() const noexcept
                    {
                        return static_cast<uint32>(RAND_MAX);
                    }

                    // MaxInt : retourne la valeur maximale générable pour int32
                    int32 MaxInt() const noexcept
                    {
                        return (RAND_MAX / 2) + 1;
                    }

                    // MinInt : retourne la valeur minimale générable pour int32
                    int32 MinInt() const noexcept
                    {
                        return -(RAND_MAX / 2);
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Méthodes templates génériques
                    // -----------------------------------------------------------------
                    // NextInRange : génère une valeur de type T dans un NkRangeT<T>
                    // Fonctionne pour tout type arithmétique supportant + et *
                    template <typename T>
                    T NextInRange(const NkRangeT<T>& range) noexcept
                    {
                        T rangeLength = range.Len();
                        T randomFactor = static_cast<T>(NextFloat32());
                        return range.GetMin() + (randomFactor * rangeLength);
                    }

                    // NextVec2 : génère un vecteur 2D avec composantes dans [0, 1[
                    template <typename T>
                    NkVec2T<T> NextVec2() noexcept
                    {
                        T componentX = static_cast<T>(NextFloat32());
                        T componentY = static_cast<T>(NextFloat32());
                        return { componentX, componentY };
                    }

                    // NextVec3 : génère un vecteur 3D avec composantes dans [0, 1[
                    template <typename T>
                    NkVec3T<T> NextVec3() noexcept
                    {
                        T componentX = static_cast<T>(NextFloat32());
                        T componentY = static_cast<T>(NextFloat32());
                        T componentZ = static_cast<T>(NextFloat32());
                        return { componentX, componentY, componentZ };
                    }

                    // NextVec4 : génère un vecteur 4D avec composantes dans [0, 1[
                    template <typename T>
                    NkVec4T<T> NextVec4() noexcept
                    {
                        T componentX = static_cast<T>(NextFloat32());
                        T componentY = static_cast<T>(NextFloat32());
                        T componentZ = static_cast<T>(NextFloat32());
                        T componentW = static_cast<T>(NextFloat32());
                        return { componentX, componentY, componentZ, componentW };
                    }

                    // NextMat2 : génère une matrice 2×2 avec colonnes aléatoires
                    template <typename T>
                    NkMat2T<T> NextMat2() noexcept
                    {
                        NkVec2T<T> column0 = NextVec2<T>();
                        NkVec2T<T> column1 = NextVec2<T>();
                        return { column0, column1 };
                    }

                    // NextMat3 : génère une matrice 3×3 avec colonnes aléatoires
                    template <typename T>
                    NkMat3T<T> NextMat3() noexcept
                    {
                        NkVec3T<T> column0 = NextVec3<T>();
                        NkVec3T<T> column1 = NextVec3<T>();
                        NkVec3T<T> column2 = NextVec3<T>();
                        return { column0, column1, column2 };
                    }

                    // NextMat4 : génère une matrice 4×4 avec colonnes aléatoires
                    template <typename T>
                    NkMat4T<T> NextMat4() noexcept
                    {
                        NkVec4T<T> column0 = NextVec4<T>();
                        NkVec4T<T> column1 = NextVec4<T>();
                        NkVec4T<T> column2 = NextVec4<T>();
                        NkVec4T<T> column3 = NextVec4<T>();
                        return { column0, column1, column2, column3 };
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Génération de couleurs
                    // -----------------------------------------------------------------
                    // NextColor : génère une couleur RGB opaque (alpha = 255)
                    NkColor NextColor() noexcept
                    {
                        uint8 red = NextUInt8();
                        uint8 green = NextUInt8();
                        uint8 blue = NextUInt8();
                        return { red, green, blue, 255 };
                    }

                    // NextColorA : génère une couleur RGBA avec alpha aléatoire
                    NkColor NextColorA() noexcept
                    {
                        uint8 red = NextUInt8();
                        uint8 green = NextUInt8();
                        uint8 blue = NextUInt8();
                        uint8 alpha = NextUInt8();
                        return { red, green, blue, alpha };
                    }

                    // NextHSV : génère une couleur en espace HSV (H,S,V dans [0,1[)
                    NkHSV NextHSV() noexcept
                    {
                        float32 hue = NextFloat32();
                        float32 saturation = NextFloat32();
                        float32 value = NextFloat32();
                        return { hue, saturation, value };
                    }

                // -----------------------------------------------------------------
                // Section : Membres privés (implémentation interne)
                // -----------------------------------------------------------------
                private:

                    // Constructeur privé : initialisation du seed aléatoire
                    // Utilise time(nullptr) pour un seed basé sur l'heure courante
                    NkRandom() noexcept
                    {
                        uint32 seedValue = static_cast<uint32>(time(nullptr));
                        srand(seedValue);
                    }

            };  // class NkRandom

            // =================================================================
            // Alias global de commodité : NkRand
            // =================================================================
            // Référence statique vers le singleton NkRandom pour un accès concis.
            //
            // Usage recommandé :
            //   NkRand.NextFloat32()  // Plus lisible que NkRandom::Instance().NextFloat32()
            //
            // Note :
            //   Ce n'est PAS une macro — le nom de classe NkRandom reste utilisable
            //   pour les cas où un contrôle explicite du singleton est nécessaire.
            // =================================================================
            static inline NkRandom& NkRand = NkRandom::Instance();

        }  // namespace math

    }  // namespace nkentseu

#endif  // __NKENTSEU_RANDOM_H__

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Accès au singleton et génération de base
    // ---------------------------------------------------------------------
    #include "NKMath/NkRandom.h"
    using namespace nkentseu::math;

    // Via l'alias global (recommandé pour la concision)
    float32 randomValue = NkRand.NextFloat32();  // [0.0, 1.0[

    // Via le singleton explicite (pour plus de clarté dans certains contextes)
    float32 anotherValue = NkRandom::Instance().NextFloat32();

    // ---------------------------------------------------------------------
    // Exemple 2 : Génération d'entiers avec différentes surcharges
    // ---------------------------------------------------------------------
    // uint8 dans [0, 255]
    uint8 byteValue = NkRand.NextUInt8();

    // uint8 dans [0, 10[
    uint8 smallValue = NkRand.NextUInt8(10);

    // uint8 dans [50, 100[
    uint8 rangedValue = NkRand.NextUInt8(50, 100);

    // uint8 via NkRangeUInt
    NkRangeUInt byteRange(128, 255);
    uint8 highByte = NkRand.NextUInt8(byteRange);

    // int32 dans [-100, 100[
    int32 signedValue = NkRand.NextInt32(-100, 100);

    // uint32 dans [0, 1000[
    uint32 largeValue = NkRand.NextUInt32(1000);

    // ---------------------------------------------------------------------
    // Exemple 3 : Génération de flottants et utilisation dans un Range
    // ---------------------------------------------------------------------
    // float32 dans [0.0, 1.0[
    float32 unitRandom = NkRand.NextFloat32();

    // float32 dans [0.0, 5.0[
    float32 scaledRandom = NkRand.NextFloat32(5.0f);

    // float32 dans [-1.0, 1.0[
    float32 signedFloat = NkRand.NextFloat32(-1.0f, 1.0f);

    // Utilisation avec NkRangeFloat pour plus de lisibilité
    NkRangeFloat damageRange(10.0f, 25.5f);
    float32 randomDamage = NkRand.NextFloat32(damageRange);

    // ---------------------------------------------------------------------
    // Exemple 4 : Génération de vecteurs aléatoires
    // ---------------------------------------------------------------------
    // Vecteur 2D float avec composantes dans [0, 1[
    NkVec2f randomVec2 = NkRand.NextVec2<float32>();

    // Vecteur 3D float pour une direction aléatoire (à normaliser)
    NkVec3f randomDir = NkRand.NextVec3<float32>().Normalized();

    // Vecteur 4D pour des coordonnées homogènes ou couleurs
    NkVec4f randomVec4 = NkRand.NextVec4<float32>();

    // Vecteur 3D int pour des coordonnées de grille aléatoires
    NkVec3i gridPos = {
        NkRand.NextInt32(0, 100),
        NkRand.NextInt32(0, 50),
        NkRand.NextInt32(0, 10)
    };

    // ---------------------------------------------------------------------
    // Exemple 5 : Génération de matrices aléatoires (attention : non orthogonales)
    // ---------------------------------------------------------------------
    // Matrice 2×2 float avec valeurs dans [0, 1[
    NkMat2f randomMat2 = NkRand.NextMat2<float32>();

    // Matrice 4×4 pour des tests de transformation aléatoires
    // Note : cette matrice n'est pas une transformation valide (pas orthogonale)
    // Utiliser plutôt NkQuat + NkVec3 pour des transformations réalistes
    NkMat4f randomMat4 = NkRand.NextMat4<float32>();

    // ---------------------------------------------------------------------
    // Exemple 6 : Génération de couleurs aléatoires
    // ---------------------------------------------------------------------
    // Couleur RGB opaque (alpha = 255)
    NkColor randomOpaque = NkRand.NextColor();

    // Couleur RGBA avec alpha aléatoire (pour effets de transparence)
    NkColor randomTransparent = NkRand.NextColorA();

    // Couleur en espace HSV pour un contrôle plus intuitif de la teinte
    NkHSV randomHSV = NkRand.NextHSV();
    // Conversion vers RGB si nécessaire (via NkColor::FromHSV)
    NkColor randomRGB = NkColor::FromHSV(randomHSV);

    // ---------------------------------------------------------------------
    // Exemple 7 : Utilisation dans un système de jeu
    // ---------------------------------------------------------------------
    // Génération de dégâts aléatoires avec variation
    float32 CalculateRandomDamage(float32 baseDamage, float32 variance)
    {
        NkRangeFloat damageRange(
            baseDamage - variance,
            baseDamage + variance
        );
        return NkRand.NextFloat32(damageRange);
    }

    // Position de spawn aléatoire dans une zone rectangulaire
    NkVec2f GetRandomSpawnPosition(NkRangeFloat xRange, NkRangeFloat yRange)
    {
        return {
            NkRand.NextFloat32(xRange),
            NkRand.NextFloat32(yRange)
        };
    }

    // Rotation aléatoire pour un objet (autour de l'axe Y uniquement)
    NkQuatf GetRandomYawRotation()
    {
        float32 randomYaw = NkRand.NextFloat32() * 360.0f;  // [0, 360[ degrés
        return NkQuatf::RotateY(NkAngle::FromDeg(randomYaw));
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Reproductibilité des séquences aléatoires
    // ---------------------------------------------------------------------
    // Pour reproduire exactement la même séquence (debug, tests, replay) :
    void SeedRandomGenerator(uint32 fixedSeed)
    {
        // Ré-initialisation manuelle du seed via srand
        // Note : cela affecte tous les futurs appels à rand(), donc à NkRand
        srand(fixedSeed);
    }

    // Usage typique :
    // SeedRandomGenerator(12345);  // Seed fixe pour tests reproductibles
    // float32 a = NkRand.NextFloat32();  // Toujours la même valeur
    // float32 b = NkRand.NextFloat32();  // Toujours la même valeur

    // Pour revenir à un comportement non-déterministe :
    // SeedRandomGenerator(static_cast<uint32>(time(nullptr)));

    // ---------------------------------------------------------------------
    // Exemple 9 : Génération dans des intervalles génériques (template)
    // ---------------------------------------------------------------------
    // NextInRange fonctionne avec tout type arithmétique supportant + et *

    // Avec double pour plus de précision
    double preciseValue = NkRand.NextInRange(NkRangeT<double>(0.0, 1.0));

    // Avec int16 pour des valeurs compactes
    int16 smallInt = NkRand.NextInRange(NkRangeT<int16>(-100, 100));

    // Avec uint64 pour de grandes plages (attention aux limites de rand())
    // Note : rand() ne génère que jusqu'à RAND_MAX (~32767 sur certaines plateformes)
    // Pour de vrais uint64 aléatoires, envisager un backend différent

    // ---------------------------------------------------------------------
    // Exemple 10 : Bonnes pratiques et pièges à éviter
    // ---------------------------------------------------------------------
    // ✓ Utiliser NkRange pour la lisibilité : NextFloat32(range) vs NextFloat32(min, max)
    // ✓ Normaliser les vecteurs directionnels après génération aléatoire
    // ✓ Seed fixe pour les tests unitaires reproductibles
    // ✓ Documenter quand une valeur aléatoire influence le gameplay (pour le balancing)

    // ✗ Ne pas appeler srand() à chaque appel de génération (casse la séquence)
    // ✗ Ne pas utiliser rand() % n pour des distributions uniformes sur de grands n
    //    (biais de modulo) — préférer les méthodes NextXXX(range) qui gèrent ce cas
    // ✗ Ne pas partager NkRand entre threads sans synchronisation externe
    // ✗ Ne pas compter sur rand() pour de la cryptographie ou de la sécurité

    // ---------------------------------------------------------------------
    // Exemple 11 : Alternative thread-safe (si besoin)
    // ---------------------------------------------------------------------
    // Pour un usage multi-thread, créer un générateur par thread :

    #if defined(ENABLE_THREAD_LOCAL_RANDOM)
    thread_local NkRandom* g_ThreadLocalRandom = nullptr;

    NkRandom& GetThreadLocalRandom()
    {
        if (g_ThreadLocalRandom == nullptr) {
            // Initialisation lazy avec seed basé sur thread ID + time
            g_ThreadLocalRandom = new NkRandom();  // Attention : pas de delete prévu
            // En production, utiliser un gestionnaire de cycle de vie approprié
        }
        return *g_ThreadLocalRandom;
    }

    // Usage : GetThreadLocalRandom().NextFloat32();
    #endif

    // Note : Cette approche est simplifiée. Pour une vraie solution thread-safe,
    // envisager std::mt19937 avec std::random_device pour le seeding par thread.
*/