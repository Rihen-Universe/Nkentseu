//
// NkRectangle.h
// =============================================================================
// Description :
//   Définition des classes NkRectangle (spécialisée float32) et NkRectT<T>
//   (générique) représentant des rectangles axis-aligned (AABB) en 2D.
//
// Fonctionnalités principales :
//   - NkRectangle : rectangle flottant avec opérations géométriques avancées
//     (Clamp, Corner, SeparatingAxis, Enlarge, AABB, Center)
//   - NkRectT<T> : template générique pour rectangles avec containment,
//     centre, comparaison et sérialisation
//
// Conception :
//   - NkRectangle : méthodes non-inline dans NkRectangle.cpp (règle projet)
//   - NkRectT<T> : template inline dans le header pour instanciation
//   - Union de membres pour accès flexible (x/y/width/height ou position/size)
//
// Auteur   : TEUGUIA TADJUIDJE Rodolf Séderis
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_RECTANGLE_H__
#define __NKENTSEU_RECTANGLE_H__

    // =====================================================================
    // Inclusions des dépendances du projet
    // =====================================================================
    #include "NKMath/NkLegacySystem.h"      // Types fondamentaux : float32, int32, etc.
    #include "NKMath/NkVec.h"               // Types NkVector2f et NkVec2T<T>
    #include "NKMath/NkSegment.h"           // Type NkSegment pour les tests d'axe

    // =====================================================================
    // Namespace principal du projet
    // =====================================================================
    namespace nkentseu {

        // =================================================================
        // Sous-namespace mathématique : types et opérations géométriques 2D
        // =================================================================
        namespace math {

            // =================================================================
            // Classe : NkRectangle
            // =================================================================
            // Rectangle axis-aligned (AABB) spécialisé float32 avec opérations
            // géométriques avancées pour collision, culling et layout.
            //
            // Représentation :
            //   - corner : coin supérieur gauche (origine du rectangle)
            //   - size   : dimensions (largeur, hauteur) positives
            //
            // Note : Les coordonnées Y croissent vers le bas (convention écran)
            // =================================================================
            class NkRectangle {

                // -----------------------------------------------------------------
                // Section : Membres publics (interface)
                // -----------------------------------------------------------------
                public:

                    // -----------------------------------------------------------------
                    // Sous-section : Données membres
                    // -----------------------------------------------------------------
                    NkVector2f corner;  ///< Coin supérieur gauche du rectangle (origine)
                    NkVector2f size;    ///< Dimensions du rectangle (largeur, hauteur)

                    // -----------------------------------------------------------------
                    // Sous-section : Constructeurs
                    // -----------------------------------------------------------------
                    // Constructeur par défaut : rectangle à l'origine de taille (1, 1)
                    NkRectangle();

                    // Constructeur par vecteurs : coin et taille explicites
                    NkRectangle(
                        const NkVector2f& cornerPoint,
                        const NkVector2f& dimensions
                    );

                    // Constructeur par coordonnées scalaires : x, y, largeur, hauteur
                    NkRectangle(
                        float32 x,
                        float32 y,
                        float32 width,
                        float32 height
                    );

                    // Constructeur mixte : coin vectoriel, taille scalaire
                    NkRectangle(
                        const NkVector2f& cornerPoint,
                        float32 width,
                        float32 height
                    );

                    // Constructeur mixte : coin scalaire, taille vectorielle
                    NkRectangle(
                        float32 x,
                        float32 y,
                        const NkVector2f& dimensions
                    );

                    // -----------------------------------------------------------------
                    // Sous-section : Opérateur d'affectation
                    // -----------------------------------------------------------------
                    // Affectation d'un rectangle à un autre (retour par référence)
                    NkRectangle& operator=(const NkRectangle& other);

                    // -----------------------------------------------------------------
                    // Sous-section : Méthodes géométriques (définies dans .cpp)
                    // -----------------------------------------------------------------
                    // Clamp : projette un point dans les limites du rectangle
                    NkVector2f Clamp(const NkVector2f& point);

                    // Corner : retourne le Nième coin du rectangle (0-3, cyclique)
                    // 0: droit, 1: bas-droit, 2: bas, 3: gauche (origine)
                    NkVector2f Corner(int cornerIndex);

                    // SeparatingAxis : test de séparation SAT pour collision
                    // Retourne true si l'axe sépare ce rectangle d'un autre
                    bool SeparatingAxis(const NkSegment& axis);

                    // Enlarge (point) : étend le rectangle pour inclure un point
                    NkRectangle Enlarge(const NkVector2f& point);

                    // Enlarge (rectangle) : étend ce rectangle pour inclure un autre
                    NkRectangle Enlarge(const NkRectangle& extender);

                    // AABB : calcule l'enveloppe axis-aligned de plusieurs rectangles
                    static NkRectangle AABB(
                        const NkRectangle* rectangleArray,
                        int rectangleCount
                    );

                    // Center : retourne le point central du rectangle
                    NkVector2f Center();

                    // -----------------------------------------------------------------
                    // Sous-section : Sérialisation et E/S
                    // -----------------------------------------------------------------
                    // ToString : conversion en chaîne formatée pour débogage
                    NkString ToString() const;

                    // Surcharge globale de ToString pour appel fonctionnel libre
                    friend NkString ToString(const NkRectangle& rectangle);

                    // Opérateur de flux pour affichage dans std::ostream
                    friend std::ostream& operator<<(
                        std::ostream& outputStream,
                        const NkRectangle& rectangle
                    );

            };  // class NkRectangle

            // =================================================================
            // Classe template : NkRectT<T>
            // =================================================================
            // Rectangle axis-aligned générique pour tout type arithmétique T.
            //
            // Accès aux membres via union :
            //   - position/size : accès sémantique vectoriel
            //   - x/y/width/height : accès scalaire nommé
            //   - left/top/w/h : alias sémantiques alternatifs
            //
            // Conçu pour : UI layout, grille de tuiles, collision discrète
            // =================================================================
            template <typename T>
            class NkRectT {

                // -----------------------------------------------------------------
                // Section : Membres publics (interface)
                // -----------------------------------------------------------------
                public:

                    // -----------------------------------------------------------------
                    // Sous-section : Union de données (accès flexible)
                    // -----------------------------------------------------------------
                    union {

                        // Vue scalaire avec alias sémantiques multiples
                        struct {
                            union { T x; T left; };       ///< Position X ou bord gauche
                            union { T y; T top; };        ///< Position Y ou bord haut
                            union { T width; T w; };      ///< Largeur du rectangle
                            union { T height; T h; };     ///< Hauteur du rectangle
                        };

                        // Vue vectorielle : position et taille comme vecteurs 2D
                        struct {
                            NkVec2T<T> position;  ///< Position (x, y) du coin supérieur gauche
                            NkVec2T<T> size;      ///< Dimensions (width, height)
                        };

                    };  // union

                    // -----------------------------------------------------------------
                    // Sous-section : Constructeurs
                    // -----------------------------------------------------------------
                    // Constructeur par défaut : rectangle nul à l'origine
                    NkRectT()
                        : position(T(0), T(0))
                        , size(T(0), T(0))
                    {
                    }

                    // Constructeur par coordonnées scalaires
                    NkRectT(
                        T posX,
                        T posY,
                        T rectWidth,
                        T rectHeight
                    )
                        : position(posX, posY)
                        , size(rectWidth, rectHeight)
                    {
                    }

                    // Constructeur par vecteurs de position et taille
                    NkRectT(
                        const NkVec2T<T>& rectPosition,
                        const NkVec2T<T>& rectSize
                    )
                        : position(rectPosition)
                        , size(rectSize)
                    {
                    }

                    // Constructeur de copie pour même type T
                    NkRectT(const NkRectT& source)
                        : position(source.position)
                        , size(source.size)
                    {
                    }

                    // Constructeur de conversion cross-type (U → T)
                    template <typename U>
                    NkRectT(
                        const NkVec2T<U>& sourcePosition,
                        const NkVec2T<U>& sourceSize
                    )
                        : position(sourcePosition)
                        , size(sourceSize)
                    {
                    }

                    // Constructeur de conversion cross-type depuis NkRectT<U>
                    template <typename U>
                    NkRectT(const NkRectT<U>& source)
                        : position(static_cast<NkVec2T<T>>(source.position))
                        , size(static_cast<NkVec2T<T>>(source.size))
                    {
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Opérateur d'affectation
                    // -----------------------------------------------------------------
                    // Affectation depuis un NkRectT de même type T
                    NkRectT& operator=(const NkRectT& source)
                    {
                        x = source.x;
                        y = source.y;
                        width = source.width;
                        height = source.height;
                        return *this;
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Accesseurs géométriques
                    // -----------------------------------------------------------------
                    // GetCenter : retourne le point central du rectangle
                    NkVec2T<T> GetCenter() const
                    {
                        return NkVec2T<T>(
                            x + width / static_cast<T>(2),
                            y + height / static_cast<T>(2)
                        );
                    }

                    // GetCenterX : retourne la coordonnée X du centre
                    T GetCenterX() const
                    {
                        return x + width / static_cast<T>(2);
                    }

                    // GetCenterY : retourne la coordonnée Y du centre
                    T GetCenterY() const
                    {
                        return y + height / static_cast<T>(2);
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Requêtes de containment
                    // -----------------------------------------------------------------
                    // Contains (scalaire) : teste si un point (px, py) est dans le rectangle
                    // Note : bord droit et bas exclusifs (convention demi-ouvert)
                    bool Contains(T pointX, T pointY) const
                    {
                        return (pointX >= x)
                            && (pointX < (x + width))
                            && (pointY >= y)
                            && (pointY < (y + height));
                    }

                    // Contains (vectoriel) : teste si un point vectoriel est dans le rectangle
                    template <typename U>
                    bool Contains(const NkVec2T<U>& point) const
                    {
                        return Contains(
                            static_cast<T>(point.x),
                            static_cast<T>(point.y)
                        );
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Opérateurs de comparaison
                    // -----------------------------------------------------------------
                    // Égalité : comparaison stricte des quatre composantes
                    friend bool operator==(
                        const NkRectT& left,
                        const NkRectT& right
                    ) noexcept {
                        return (left.x == right.x)
                            && (left.y == right.y)
                            && (left.width == right.width)
                            && (left.height == right.height);
                    }

                    // Inégalité : déléguée à l'opérateur d'égalité (principe DRY)
                    friend bool operator!=(
                        const NkRectT& left,
                        const NkRectT& right
                    ) noexcept {
                        return !(left == right);
                    }

                    // -----------------------------------------------------------------
                    // Sous-section : Sérialisation et E/S
                    // -----------------------------------------------------------------
                    // ToString : conversion en chaîne formatée pour débogage
                    NkString ToString() const
                    {
                        return NkFormat(
                            "NkRectT[pos({0}, {1}); size({2}, {3})]",
                            x,
                            y,
                            width,
                            height
                        );
                    }

                    // Opérateur de flux pour affichage dans std::ostream
                    // Note : template spécifique pour NkRectT<T>
                    friend std::ostream& operator<<(
                        std::ostream& outputStream,
                        const NkRectT& rectangle
                    ) {
                        return outputStream << rectangle.ToString().CStr();
                    }

            };  // class NkRectT<T>

            // =================================================================
            // Aliases de types pour NkRectT<T>
            // =================================================================
            using NkIntRect = NkRectT<int32>;        // Rectangle entier pour UI/grid
            using NkFloatRect = NkRectT<float32>;    // Rectangle float pour géométrie
            using NkRectF = NkRectT<float32>;
            using NkRectD = NkRectT<float64>;
            using NkRectI = NkRectT<int32>;
            using NkRectU = NkRectT<uint32>;
            using NkRectf = NkRectT<float32>;
            using NkRectd = NkRectT<float64>;
            using NkRecti = NkRectT<int32>;
            using NkRectu = NkRectT<uint32>;

        }  // namespace math

        // =====================================================================
        // Spécialisations de NkToString pour intégration au système de formatage
        // =====================================================================
        // Permet d'utiliser NkRectT<T> et NkRectangle avec NkFormat() et les
        // utilitaires d'E/S du projet, avec support des options de formatage.
        // =====================================================================
        template <typename T>
        inline NkString NkToString(
            const math::NkRectT<T>& rectangle,
            const NkFormatProps& formatProperties = {}
        ) {
            return formatProperties.ApplyWidth(NkStringView(rectangle.ToString()), false);
        }

        inline NkString NkToString(
            const math::NkRectangle& rectangle,
            const NkFormatProps& formatProperties = {}
        ) {
            return formatProperties.ApplyWidth(NkStringView(rectangle.ToString()), false);
        }

    }  // namespace nkentseu

#endif  // __NKENTSEU_RECTANGLE_H__

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Création de rectangles avec différents constructeurs
    // ---------------------------------------------------------------------
    #include "NKMath/NkRectangle.h"
    using namespace nkentseu::math;

    // Rectangle par défaut : origine (0,0), taille (1,1)
    NkRectangle defaultRect;

    // Rectangle via vecteurs : coin et taille
    NkVector2f topLeft(10.0f, 20.0f);
    NkVector2f dimensions(100.0f, 50.0f);
    NkRectangle vectorRect(topLeft, dimensions);  // [10,20] -> [110,70]

    // Rectangle via coordonnées scalaires
    NkRectangle scalarRect(0.0f, 0.0f, 1920.0f, 1080.0f);  // Écran Full HD

    // Rectangle mixte : coin vectoriel, taille scalaire
    NkRectangle mixedRect1(topLeft, 200.0f, 150.0f);

    // Rectangle mixte : coin scalaire, taille vectorielle
    NkRectangle mixedRect2(50.0f, 50.0f, dimensions);

    // ---------------------------------------------------------------------
    // Exemple 2 : Template NkRectT<T> pour différents types
    // ---------------------------------------------------------------------
    // Rectangle entier pour système de grille UI
    NkIntRect uiButton(100, 50, 200, 40);  // x=100, y=50, w=200, h=40

    // Rectangle float pour collision physique
    NkFloatRect hitbox(1.5f, 2.3f, 0.5f, 1.8f);

    // Conversion cross-type : int → float
    NkFloatRect floatFromInt(uiButton);  // Copie avec cast implicite

    // Accès via alias sémantiques
    int buttonLeft = uiButton.left;      // 100
    int buttonTop = uiButton.top;        // 50
    int buttonW = uiButton.w;            // 200

    // ---------------------------------------------------------------------
    // Exemple 3 : Requêtes géométriques de base
    // ---------------------------------------------------------------------
    NkRectangle viewport(0.0f, 0.0f, 800.0f, 600.0f);

    // Test d'appartenance d'un point
    bool isInside = viewport.Contains(NkVector2f(400.0f, 300.0f));  // true
    bool isOutside = viewport.Contains(NkVector2f(-10.0f, 0.0f));   // false

    // Calcul du centre
    NkVector2f center = viewport.Center();  // (400, 300)

    // Accès aux coins (index cyclique 0-3)
    NkVector2f corner0 = viewport.Corner(0);  // Coin droit
    NkVector2f corner1 = viewport.Corner(1);  // Coin bas-droit
    NkVector2f corner2 = viewport.Corner(2);  // Coin bas
    NkVector2f corner3 = viewport.Corner(3);  // Coin gauche (origine)

    // ---------------------------------------------------------------------
    // Exemple 4 : Clamping de positions dans un rectangle
    // ---------------------------------------------------------------------
    NkRectangle gameArea(0.0f, 0.0f, 100.0f, 100.0f);
    NkVector2f playerPos(150.0f, -20.0f);  // Hors limites

    // Clamp : projette la position dans les limites du rectangle
    NkVector2f clampedPos = gameArea.Clamp(playerPos);
    // Résultat : (100.0f, 0.0f) - coin inférieur droit

    // Utilisation typique : maintenir un sprite dans l'écran
    void KeepSpriteInViewport(NkVector2f& spritePos, const NkRectangle& viewport)
    {
        spritePos = viewport.Clamp(spritePos);
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Enveloppe AABB de plusieurs rectangles
    // ---------------------------------------------------------------------
    NkRectangle rects[3] = {
        {0.0f, 0.0f, 10.0f, 10.0f},
        {5.0f, 5.0f, 20.0f, 15.0f},
        {15.0f, 10.0f, 5.0f, 5.0f}
    };

    // Calcul de l'enveloppe englobante axis-aligned
    NkRectangle boundingBox = NkRectangle::AABB(rects, 3);
    // Résultat approximatif : [0,0] -> [25,20]

    // Utilisation : culling de groupe, calcul de bounds hiérarchiques
    NkRectangle ComputeSceneBounds(const std::vector<NkRectangle>& objects)
    {
        if (objects.empty()) {
            return NkRectangle();
        }
        return NkRectangle::AABB(objects.data(), static_cast<int>(objects.size()));
    }

    // ---------------------------------------------------------------------
    // Exemple 6 : Test de séparation d'axe (SAT) pour collision
    // ---------------------------------------------------------------------
    NkRectangle rectA(0.0f, 0.0f, 10.0f, 10.0f);
    NkRectangle rectB(15.0f, 5.0f, 10.0f, 10.0f);  // Décalé sur X

    // Création d'un axe de test (ex: normale d'une arête)
    NkSegment testAxis(NkVector2f(1.0f, 0.0f), NkVector2f(0.0f, 0.0f));

    // Test SAT : true si l'axe sépare les rectangles (pas de collision)
    bool isSeparated = rectA.SeparatingAxis(testAxis);

    // Note : pour une collision complète, tester tous les axes potentiels
    // (normales des arêtes des deux rectangles)

    // ---------------------------------------------------------------------
    // Exemple 7 : Expansion dynamique de rectangle
    // ---------------------------------------------------------------------
    NkRectangle initial(10.0f, 10.0f, 20.0f, 20.0f);  // [10,10] -> [30,30]

    // Enlarge avec un point : étend pour inclure le point
    NkRectangle expanded1 = initial.Enlarge(NkVector2f(50.0f, 15.0f));
    // Résultat : [10,10] -> [50,30]

    // Enlarge avec un rectangle : union des deux rectangles
    NkRectangle other(40.0f, 40.0f, 10.0f, 10.0f);  // [40,40] -> [50,50]
    NkRectangle expanded2 = initial.Enlarge(other);
    // Résultat : [10,10] -> [50,50]

    // Utilisation : mise à jour incrémentale des bounds d'un groupe
    void UpdateGroupBounds(NkRectangle& groupBounds, const NkRectangle& newMember)
    {
        groupBounds = groupBounds.Enlarge(newMember);
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Affichage et débogage
    // ---------------------------------------------------------------------
    NkRectangle debugRect(3.14f, 2.71f, 9.99f, 1.41f);

    // Via méthode membre ToString()
    NkString rectStr = debugRect.ToString();
    // Résultat : "NkRectT[pos(3.14, 2.71); size(9.99, 1.41)]"

    // Via flux de sortie standard
    std::cout << "Rectangle : " << debugRect << std::endl;

    // Via fonction globale NkToString avec formatage personnalisé
    NkString formatted = NkToString(
        debugRect,
        NkFormatProps().SetPrecision(2)
    );  // "NkRectT[pos(3.14, 2.71); size(9.99, 1.41)]"

    // Pour NkRectT<int>
    NkIntRect intRect(10, 20, 100, 50);
    std::cout << "UI Rect : " << intRect << std::endl;
    // Résultat : "NkRectT[pos(10, 20); size(100, 50)]"

    // ---------------------------------------------------------------------
    // Exemple 9 : Utilisation dans un système de layout UI
    // ---------------------------------------------------------------------
    class UIWidget {
    public:
        NkIntRect bounds;
        bool IsPointInWidget(int mouseX, int mouseY) const {
            return bounds.Contains(mouseX, mouseY);
        }
        NkVector2i GetWidgetCenter() const {
            return bounds.GetCenter();
        }
    };

    // ---------------------------------------------------------------------
    // Bonnes pratiques d'utilisation
    // ---------------------------------------------------------------------
    // ✓ Préférer NkIntRect pour l'UI (pas de flottants inutiles)
    // ✓ Utiliser Contains() plutôt que des comparaisons manuelles
    // ✓ Vérifier que size.x et size.y sont positifs pour éviter des bugs
    // ✓ Utiliser AABB() pour calculer des bounds hiérarchiques efficacement
    // ✗ Attention : Corner() utilise un modulo 4 - indexer avec précaution
    // ✗ Contains() est demi-ouvert [min, max) - adapter si besoin de fermé
*/