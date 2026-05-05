//
// NkMath.h
// =============================================================================
// Description :
//   En-tête maître du module mathématique NkMath.
//   Ce fichier unique permet d'accéder à tous les types et utilitaires
//   mathématiques du projet via une inclusion centralisée.
//
// Architecture modulaire :
//   - Noyau scalaires et constantes : NkFunctions.h
//   - Types angulaires : NkAngle.h, NkEulerAngle.h
//   - Vecteurs génériques : NkVec.h (Vec2/3/4 × float32/float64/int32/uint32)
//   - Matrices génériques : NkMat.h (Mat2/3/4 × float32/float64)
//   - Quaternions : NkQuat.h (rotations 3D sans gimbal lock)
//   - Utilitaires géométriques : NkRange.h, NkRectangle.h, NkSegment.h
//   - Couleurs : NkColor.h (RGB, RGBA, HSV)
//   - Génération aléatoire : NkRandom.h
//   - SIMD optionnel : NkSIMD.h (activé via NK_ENABLE_SSE42 / NK_ENABLE_NEON)
//
// Usage recommandé :
//   #include "NKMath/NkMath.h"  // Inclure ce fichier uniquement
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef NKENTSEU_MATH_H
#define NKENTSEU_MATH_H

    // =====================================================================
    // Section 1 : Noyau mathématique — scalaires et constantes
    // =====================================================================
    // Fonctions utilitaires mathématiques et constantes globales :
    //   - NkSin, NkCos, NkTan, NkSqrt, NkClamp, NkLerp, etc.
    //   - Constantes : NkPi, NkPis2, NkEpsilon, NkVectorEpsilon, etc.
    // =====================================================================
    #include "NKMath/NkFunctions.h"

    // =====================================================================
    // Section 2 : Types angulaires — gestion robuste des angles
    // =====================================================================
    // NkAngle : type wrapper pour angles avec wrapping automatique (-180°, 180°]
    // NkEulerAngle : triplet pitch/yaw/roll pour orientations 3D
    // =====================================================================
    #include "NKMath/NkAngle.h"
    #include "NKMath/NkEulerAngle.h"

    // =====================================================================
    // Section 3 : Vecteurs génériques — Vec2/3/4 pour tous types scalaires
    // =====================================================================
    // Template unique NkVecT<T> avec aliases pour variantes de type :
    //   - Float : NkVec2f, NkVec3f, NkVec4f (float32)
    //   - Double : NkVec2d, NkVec3d, NkVec4d (float64)
    //   - Entiers : NkVec2i/u, NkVec3i/u, NkVec4i/u (int32/uint32)
    //
    // Fonctionnalités :
    //   - Arithmétique composante par composante
    //   - Produit scalaire, vectoriel (3D), longueur, normalisation
    //   - Interpolation : Lerp, NLerp, SLerp
    //   - Swizzling : accès flexible aux composantes (xy, xyz, etc.)
    // =====================================================================
    #include "NKMath/NkVec.h"

    // =====================================================================
    // Section 4 : Matrices génériques — transformations linéaires 2D/3D/4D
    // =====================================================================
    // Template unique NkMatT<T> avec aliases pour variantes de type :
    //   - Float : NkMat2f, NkMat3f, NkMat4f (float32)
    //   - Double : NkMat2d, NkMat3d, NkMat4d (float64)
    //
    // Convention de stockage :
    //   - Column-major (compatible OpenGL/Vulkan)
    //   - Méthode ToRowMajor() pour conversion DirectX/HLSL si nécessaire
    //
    // Fonctionnalités :
    //   - Produit matriciel, transposition, inversion, déterminant
    //   - Fabriques : Translation, Rotation, Scaling, Perspective, LookAt
    //   - Extraction TRS robuste via quaternion (évite gimbal lock)
    // =====================================================================
    #include "NKMath/NkMat.h"

    // =====================================================================
    // Section 5 : Quaternions — rotations 3D robustes sans singularités
    // =====================================================================
    // Template NkQuatT<T> pour rotations unitaires :
    //   - NkQuatf (float32) — recommandé pour la plupart des usages
    //   - NkQuatd (float64) — pour précision numérique élevée
    //
    // Avantages vs angles d'Euler :
    //   - Pas de gimbal lock
    //   - Interpolation sphérique naturelle (SLerp)
    //   - Composition de rotations stable numériquement
    //   - Stockage compact (4 scalaires vs 9/16 pour matrices)
    //
    // Fonctionnalités :
    //   - Construction : angle-axe, Euler, matrice, from-to
    //   - Application : rotation de vecteurs via formule de Rodrigues
    //   - Interpolation : Lerp, NLerp, SLerp (chemin court garanti)
    //   - Conversions : ↔ matrice, ↔ Euler (avec détection de singularité)
    // =====================================================================
    #include "NKMath/NkQuat.h"

    // =====================================================================
    // Section 6 : Utilitaires géométriques 1D et 2D
    // =====================================================================
    // NkRangeT<T> : intervalle fermé [min, max] générique
    //   - Opérations : Contains, Overlaps, Intersect, Union, Split
    //   - Mutations : Shift, Expand, Clamp
    //   - Alias : NkRange (float32), NkRangeInt, NkRangeUInt, etc.
    //
    // NkRectangle : rectangle axis-aligned (AABB) 2D float32
    //   - Représentation : corner (haut-gauche) + size (largeur, hauteur)
    //   - Opérations : Clamp, Corner, Enlarge, AABB (enveloppe de groupe)
    //   - Tests : SeparatingAxis pour détection de collision SAT
    //
    // NkSegment : segment de droite 2D défini par deux points
    //   - Accès flexible via union : vecteurs, scalaires, tableau brut
    //   - Opérations : Length, Project, Equivalent
    // =====================================================================
    #include "NKMath/NkRange.h"
    #include "NKMath/NkRectangle.h"
    #include "NKMath/NkSegment.h"

    // =====================================================================
    // Section 7 : Couleurs — gestion des espaces RGB/RGBA/HSV
    // =====================================================================
    // NkColor : couleur 8-bit par composante (RGBA)
    //   - Constructeurs : depuis hex, depuis float normalisé, depuis HSV
    //   - Conversions : ↔ NkHSV, ↔ float normalisé, ↔ entier 0-255
    //   - Opérations : interpolation, modulation, premultiplied alpha
    //
    // NkHSV : couleur en espace Hue/Saturation/Value
    //   - Plus intuitif pour les variations de teinte/saturation/luminosité
    //   - Conversion bidirectionnelle avec NkColor
    // =====================================================================
    #include "NKMath/NkColor.h"

    // =====================================================================
    // Section 8 : Génération de nombres pseudo-aléatoires
    // =====================================================================
    // NkRandom : singleton global pour génération aléatoire
    //   - Backend : rand() standard (suffisant pour usage jeu vidéo)
    //   - API riche : NextFloat32, NextInt32, NextVec3, NextColor, etc.
    //   - Accès concis via alias global : NkRand.NextFloat32()
    //
    // Note thread-safety :
    //   Ce singleton n'est PAS thread-safe. Pour usage multi-thread :
    //   - Utiliser un générateur thread-local par thread
    //   - Ou protéger les appels par mutex externe
    // =====================================================================
    #include "NKMath/NkRandom.h"

    // =====================================================================
    // Section 9 : SIMD optionnel — accélérations vectorielles matérielles
    // =====================================================================
    // NkSIMD : helpers pour opérations batch sur vecteurs/matrices
    //   - Activation conditionnelle via macros de compilation :
    //     • NK_ENABLE_SSE42 : pour processeurs x86/x64 avec SSE4.2
    //     • NK_ENABLE_NEON  : pour processeurs ARM avec NEON
    //
    // Fonctionnalités (si activé) :
    //   - Produits matriciels 4×4 vectorisés
    //   - Transformations de lots de vecteurs (batch transform)
    //   - Interpolations SIMD pour particules/animations
    //
    // Si non activé : fallback automatique sur implémentation scalaire
    // =====================================================================
    #include "NKMath/NkSIMD.h"

    // =====================================================================
    // Section 10 : Formatage — intégration avec le système NkFormat
    // =====================================================================
    // NkMathFormat : spécialisations NkFormatter<T> pour tous les types NkMath.
    //   - Permet NkFormat("{0}", vec3), NkFormat("{0:v}", vec4), etc.
    //   - NkVec4 : modes v/p/c/n pour vecteur, position, couleur, normalisé
    //   - NkAngle : modes d/r pour degrés/radians
    //   - Tous les autres types délèguent à leur ToString()
    // =====================================================================
    #include "NKMath/NkMathFormat.h"

#endif  // NKENTSEU_MATH_H

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Inclusion unique et accès aux types de base
    // ---------------------------------------------------------------------
    #include "NKMath/NkMath.h"
    using namespace nkentseu::math;

    // Constantes mathématiques
    float32 halfPi = NkPis2;              // π/2 ≈ 1.5708
    float32 epsilon = NkEpsilon;          // Tolérance flottante ~1e-7

    // Fonctions utilitaires
    float32 clamped = NkClamp(1.5f, 0.0f, 1.0f);   // 1.0f
    float32 lerped = NkLerp(10.0f, 20.0f, 0.3f);   // 13.0f

    // ---------------------------------------------------------------------
    // Exemple 2 : Angles et orientations
    // ---------------------------------------------------------------------
    // NkAngle : gestion automatique du wrapping
    NkAngle angle90 = NkAngle::FromDeg(90.0f);
    NkAngle angleRad = NkAngle::FromRad(1.5708f);
    float32 degrees = angle90.Deg();  // 90.0f

    // NkEulerAngle : triplet pitch/yaw/roll pour orientations 3D
    NkEulerAngle orientation(
        NkAngle::FromDeg(30.0f),  // pitch : tangage haut/bas
        NkAngle::FromDeg(45.0f),  // yaw   : lacet gauche/droite
        NkAngle::FromDeg(10.0f)   // roll  : roulis inclinaison
    );

    // ---------------------------------------------------------------------
    // Exemple 3 : Vecteurs — création et opérations de base
    // ---------------------------------------------------------------------
    // Vecteur 3D float (le plus courant)
    NkVec3f position(1.0f, 2.5f, -3.0f);
    NkVec3f velocity(0.0f, 9.81f, 0.0f);

    // Arithmétique composante par composante
    NkVec3f newPosition = position + velocity * 0.016f;  // dt = 16ms

    // Produit scalaire et vectoriel
    NkVec3f forward(0.0f, 0.0f, 1.0f);
    NkVec3f right(1.0f, 0.0f, 0.0f);
    float32 alignment = forward.Dot(right);        // 0.0f (orthogonaux)
    NkVec3f up = forward.Cross(right);             // (0, 1, 0)

    // Normalisation et longueur
    NkVec3f direction = NkVec3f(3.0f, 4.0f, 0.0f);
    float32 length = direction.Len();              // 5.0f
    NkVec3f unitDir = direction.Normalized();      // (0.6, 0.8, 0)

    // ---------------------------------------------------------------------
    // Exemple 4 : Matrices — transformations 3D complètes
    // ---------------------------------------------------------------------
    // Matrice de transformation modèle : Scale → Rotate → Translate
    NkMat4f scale = NkMat4f::Scaling(NkVec3f(2.0f, 1.0f, 1.0f));
    NkMat4f rotation = NkMat4f::RotationY(NkAngle::FromDeg(45.0f));
    NkMat4f translation = NkMat4f::Translation(NkVec3f(10.0f, 0.0f, 0.0f));

    // Composition (ordre important : rightmost appliqué en premier)
    NkMat4f modelMatrix = translation * rotation * scale;

    // Transformation d'un point
    NkVec3f localPoint(1.0f, 0.0f, 0.0f);
    NkVec3f worldPoint = modelMatrix * localPoint;

    // Matrice de vue LookAt
    NkMat4f viewMatrix = NkMat4f::LookAt(
        NkVec3f(0.0f, 0.0f, 10.0f),  // position caméra
        NkVec3f(0.0f, 0.0f, 0.0f),   // point regardé
        NkVec3f(0.0f, 1.0f, 0.0f)    // up mondial
    );

    // Matrice de projection perspective
    NkMat4f projMatrix = NkMat4f::Perspective(
        NkAngle::FromDeg(60.0f),  // FOV vertical
        16.0f / 9.0f,             // aspect ratio
        0.1f, 1000.0f,            // near/far planes
        false                     // depth range [-1,1] pour OpenGL
    );

    // Matrice MVP complète pour vertex shader
    NkMat4f mvpMatrix = projMatrix * viewMatrix * modelMatrix;

    // ---------------------------------------------------------------------
    // Exemple 5 : Quaternions — rotations robustes et interpolation
    // ---------------------------------------------------------------------
    // Création depuis angle-axe
    NkQuatf quarterTurn = NkQuatf(
        NkAngle::FromDeg(90.0f),
        NkVec3f(0.0f, 1.0f, 0.0f)  // axe Y
    );

    // Application à un vecteur
    NkVec3f original(1.0f, 0.0f, 0.0f);
    NkVec3f rotated = quarterTurn * original;  // ~(0, 0, -1)

    // Interpolation sphérique (SLerp) pour animation fluide
    NkQuatf start = NkQuatf::Identity();
    NkQuatf end = NkQuatf::RotateY(NkAngle::FromDeg(180.0f));
    NkQuatf interpolated = start.SLerp(end, 0.5f);  // 90° de rotation

    // Conversion ↔ Euler (avec gestion du gimbal lock)
    NkEulerAngle euler = static_cast<NkEulerAngle>(quarterTurn);
    NkQuatf rebuilt(euler);

    // ---------------------------------------------------------------------
    // Exemple 6 : Utilitaires géométriques — intervalles et rectangles
    // ---------------------------------------------------------------------
    // NkRange : intervalle 1D pour clamping et tests d'appartenance
    NkRangeFloat healthRange(0.0f, 100.0f);
    float32 clampedHealth = healthRange.Clamp(150.0f);  // 100.0f
    bool isAlive = healthRange.Contains(clampedHealth); // true

    // NkRectangle : AABB 2D pour collision et culling
    NkRectangle viewport(0.0f, 0.0f, 1920.0f, 1080.0f);
    NkRectangle sprite(100.0f, 200.0f, 50.0f, 50.0f);

    // Test de chevauchement
    bool isVisible = viewport.Overlaps(sprite);

    // Clamping de position dans les limites de l'écran
    NkVec2f spritePos(2000.0f, -50.0f);
    NkVec2f clampedPos = viewport.Clamp(spritePos);  // (1920, 0)

    // Enveloppe AABB de plusieurs rectangles (culling de groupe)
    NkRectangle objects[3] = {};  // initialisé ailleurs
    NkRectangle groupBounds = NkRectangle::AABB(objects, 3);

    // ---------------------------------------------------------------------
    // Exemple 7 : Couleurs — espaces RGB et HSV
    // ---------------------------------------------------------------------
    // NkColor : couleur RGBA 8-bit par composante
    NkColor red(255, 0, 0, 255);                    // Rouge opaque
    NkColor fromHex = NkColor::FromHex(0x00FF00);   // Vert depuis hex
    NkColor fromFloat = NkColor::FromNormalized(1.0f, 0.5f, 0.0f);  // Orange

    // Interpolation de couleurs (pour effets de fade, lerp UI)
    NkColor startColor(255, 0, 0, 255);   // Rouge
    NkColor endColor(0, 0, 255, 255);     // Bleu
    NkColor midColor = startColor.Lerp(endColor, 0.5f);  // Violet ~

    // NkHSV : espace plus intuitif pour variations de teinte
    NkHSV rainbow(0.0f, 1.0f, 1.0f);  // Rouge saturé plein
    rainbow.hue = NkClamp(rainbow.hue + 0.01f, 0.0f, 1.0f);  // Décalage de teinte
    NkColor rainbowRGB = NkColor::FromHSV(rainbow);  // Conversion vers RGB

    // ---------------------------------------------------------------------
    // Exemple 8 : Génération aléatoire via NkRandom
    // ---------------------------------------------------------------------
    // Accès via alias global concis
    float32 randomUnit = NkRand.NextFloat32();           // [0.0, 1.0[
    int32 randomInt = NkRand.NextInt32(-100, 100);       // [-100, 100[
    NkVec3f randomDir = NkRand.NextVec3<float32>().Normalized();

    // Couleur aléatoire pour effets visuels
    NkColor randomColor = NkRand.NextColor();            // RGB opaque
    NkColor randomTransparent = NkRand.NextColorA();     // RGBA avec alpha

    // Reproductibilité pour tests/debug : seed fixe
    srand(12345);  // Ré-initialisation manuelle si besoin
    float32 deterministic = NkRand.NextFloat32();  // Même valeur à chaque run

    // ---------------------------------------------------------------------
    // Exemple 9 : Intégration dans une boucle de jeu typique
    // ---------------------------------------------------------------------
    void UpdateGameEntity(
        NkVec3f& position,
        NkQuatf& orientation,
        float32 deltaTime
    ) {
        // Mouvement aléatoire léger (dérive)
        NkVec3f drift = NkRand.NextVec3<float32>() * 0.1f;
        position += drift * deltaTime;

        // Rotation progressive vers une cible
        NkQuatf targetRot = NkQuatf::LookAt(
            position,
            NkVec3f(0.0f, 0.0f, 0.0f),  // regarder l'origine
            NkVec3f(0.0f, 1.0f, 0.0f)   // up mondial
        );
        orientation = orientation.SLerp(targetRot, 2.0f * deltaTime);

        // Clamp de position dans une zone de jeu
        NkRangeFloat playArea(-100.0f, 100.0f);
        position.x = playArea.Clamp(position.x);
        position.z = playArea.Clamp(position.z);
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Upload vers GPU — gestion column-major vs row-major
    // ---------------------------------------------------------------------
    NkMat4f modelViewProj = {};  // matrice MVP calculée

    // Pour OpenGL / Vulkan (column-major natif) :
    // glUniformMatrix4fv(location, 1, GL_FALSE, modelViewProj.data);

    // Pour DirectX / HLSL (row-major attendu) :
    float32 rowMajorData[16];
    modelViewProj.ToRowMajor(rowMajorData);
    // Puis upload : context->UpdateSubresource(..., rowMajorData, ...);

    // Alternative : utiliser AsRowMajor() pour obtenir la transposée
    NkMat4f rowMajorMatrix = modelViewProj.AsRowMajor();
    // Puis upload : context->UpdateSubresource(..., rowMajorMatrix.data, ...);

    // ---------------------------------------------------------------------
    // Bonnes pratiques d'utilisation
    // ---------------------------------------------------------------------
    // ✓ Inclure uniquement "NKMath/NkMath.h" — pas besoin d'inclure les sous-fichiers
    // ✓ Utiliser les alias courts (NkVec3f, NkMat4f) pour la lisibilité
    // ✓ Normaliser les vecteurs directionnels avant utilisation dans Dot/Cross
    // ✓ Utiliser SLerp pour les interpolations de rotation (vitesse constante)
    // ✓ Vérifier les bounds avec NkRange avant accès tableau/indexation
    // ✗ Éviter les conversions Euler↔Quaternion en boucle (perte de précision)
    // ✗ Ne pas partager NkRand entre threads sans synchronisation externe
    // ✗ Attention à l'ordre de multiplication matricielle : T*R*S ≠ S*R*T
*/