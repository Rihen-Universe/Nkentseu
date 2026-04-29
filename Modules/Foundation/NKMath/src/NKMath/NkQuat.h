//
// NkQuat.h
// =============================================================================
// Description :
//   Définition de la structure template NkQuatT<T> représentant un quaternion
//   unitaire générique pour les rotations 3D sans gimbal lock.
//
// Représentation :
//   q = (x, y, z, w) = (v, s) où :
//   - v = partie vectorielle (x, y, z) : axe de rotation × sin(angle/2)
//   - s = partie scalaire (w) : cos(angle/2)
//   Convention : w est le scalaire (compatible XNA, Unity, GLM)
//
// Algorithmes implémentés :
//   - Rotation de vecteur : formule de Rodrigues optimisée (15 multiplications)
//     source: Vince (2011) "Quaternions for Computer Graphics"
//   - Produit Hamilton : (this ⊗ r) — r appliqué APRÈS this (convention GLM)
//   - SLerp : exponentiation q*(q⁻¹*r)^t (Shoemake 1985) avec chemin court
//   - Exponentiation q^t : interpolation angulaire avec clamp [-1,1] pour acos
//   - LookAt : deux rotations successives from-to (forward puis up)
//   - Conversion Euler : extraction avec détection du pôle de Gimbal (±90° Yaw)
//
// Types disponibles :
//   NkQuatf = NkQuatT<float32>  (recommandé pour la plupart des usages)
//   NkQuatd = NkQuatT<float64>  (pour précision numérique élevée)
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_QUATERNION_H__
#define __NKENTSEU_QUATERNION_H__

    // =====================================================================
    // Inclusions des dépendances du projet
    // =====================================================================
    #include "NKMath/NkLegacySystem.h"      // Types fondamentaux : float32, float64, etc.
    #include "NKMath/NkVec.h"               // Types vectoriels : NkVec3T, NkVec4T
    #include "NKMath/NkMat.h"               // Type NkMat4T pour conversions matricielles
    #include "NKMath/NkEulerAngle.h"        // Type NkEulerAngle pour conversions Euler

    // =====================================================================
    // Namespace principal du projet
    // =====================================================================
    namespace nkentseu {

        // =================================================================
        // Sous-namespace mathématique : types et opérations quaternioniques
        // =================================================================
        namespace math {

            // =================================================================
            // Structure template : NkQuatT<T>
            // =================================================================
            // Quaternion unitaire générique pour rotations 3D robustes.
            //
            // Invariant de production : les quaternions sont toujours normalisés.
            // Conséquence : Inverse() == Conjugate() (plus rapide que l'inverse général)
            //
            // Avantages vs matrices/Euler :
            //   - Pas de gimbal lock (contrairement aux angles d'Euler)
            //   - Interpolation sphérique naturelle (SLerp)
            //   - Composition de rotations plus stable numériquement
            //   - Stockage compact (4 scalaires vs 9/16 pour matrices)
            // =================================================================
            template <typename T>
            struct NkQuatT {

                // -----------------------------------------------------------------
                // Section : Union de données (accès flexible)
                // -----------------------------------------------------------------
                union {

                    // Accès nommé par composantes individuelles
                    struct {
                        T x;  ///< Composante X de la partie vectorielle
                        T y;  ///< Composante Y de la partie vectorielle
                        T z;  ///< Composante Z de la partie vectorielle
                        T w;  ///< Composante scalaire (cos(angle/2))
                    };

                    // Accès sémantique : partie vectorielle + scalaire séparés
                    struct {
                        NkVec3T<T> vector;  ///< Partie vectorielle (x, y, z)
                        T scalar;           ///< Partie scalaire (w)
                    };

                    // Accès via vecteur 4D complet pour opérations génériques
                    NkVec4T<T> quat;  ///< Accès NkVec4T pour compatibilité

                    // Accès tableau brut pour opérations bas-niveau/SIMD
                    T data[4];  ///< Tableau linéaire [x, y, z, w]

                };  // union

                // -----------------------------------------------------------------
                // Section : Constructeurs
                // -----------------------------------------------------------------
                // Constructeur par défaut : quaternion identité (pas de rotation)
                // Représente la rotation nulle : w=1, xyz=0
                NkQuatT() noexcept
                    : x(T(0))
                    , y(T(0))
                    , z(T(0))
                    , w(T(1))
                {
                }

                // Constructeur scalaire avec option identité
                // Si identity=true : quaternion identité multiplié par val (w=val, xyz=0)
                // Si identity=false : toutes composantes initialisées à val
                explicit NkQuatT(T value, bool identity = true) noexcept
                    : w(value)
                    , x(identity ? T(0) : value)
                    , y(identity ? T(0) : value)
                    , z(identity ? T(0) : value)
                {
                }

                // Constructeur composant par composant (ordre : x, y, z, w)
                NkQuatT(
                    T componentX,
                    T componentY,
                    T componentZ,
                    T componentW
                ) noexcept
                    : x(componentX)
                    , y(componentY)
                    , z(componentZ)
                    , w(componentW)
                {
                }

                // Constructeur depuis angle-axe : crée quaternion représentant
                // une rotation de 'angle' radians autour de l'axe 'axis'
                // Formule : q = (axis_normalized * sin(angle/2), cos(angle/2))
                NkQuatT(
                    const NkAngle& rotationAngle,
                    const NkVec3T<T>& rotationAxis
                ) noexcept;

                // Constructeur depuis angles d'Euler (convention YXZ : yaw→pitch→roll)
                // Convertit les trois angles en quaternion équivalent
                NkQuatT(const NkEulerAngle& eulerAngles) noexcept;

                // Constructeur depuis partie vectorielle + scalaire explicites
                NkQuatT(
                    const NkVec3T<T>& vectorPart,
                    T scalarPart
                ) noexcept
                    : vector(vectorPart)
                    , scalar(scalarPart)
                {
                }

                // Constructeur depuis matrice de rotation 4×4
                // Extrait la rotation pure de la matrice (ignore translation/scale)
                explicit NkQuatT(const NkMat4T<T>& rotationMatrix) noexcept;

                // Constructeur depuis vecteur 4D (x,y,z,w)
                explicit NkQuatT(const NkVec4T<T>& components) noexcept
                    : quat(components)
                {
                }

                // Constructeur de rotation minimale : crée le quaternion représentant
                // la plus petite rotation amenant le vecteur 'from' vers 'to'
                // Gère le cas dégénéré from == -to (rotation 180°)
                NkQuatT(
                    const NkVec3T<T>& sourceVector,
                    const NkVec3T<T>& targetVector
                ) noexcept;

                // Constructeur de copie : généré par défaut (optimisation compilateur)
                NkQuatT(const NkQuatT&) noexcept = default;

                // Constructeur de déplacement : généré par défaut
                NkQuatT(NkQuatT&&) noexcept = default;

                // Opérateur d'affectation par copie : généré par défaut
                NkQuatT& operator=(const NkQuatT&) noexcept = default;

                // Opérateur d'affectation par déplacement : généré par défaut
                NkQuatT& operator=(NkQuatT&&) noexcept = default;

                // -----------------------------------------------------------------
                // Section : Accesseurs et conversions brutes
                // -----------------------------------------------------------------
                // Conversion implicite vers pointeur mutable sur données brutes
                operator T*() noexcept
                {
                    return data;
                }

                // Conversion implicite vers pointeur constant sur données brutes
                operator const T*() const noexcept
                {
                    return data;
                }

                // Opérateur d'indexation mutable avec masquage de borne (0-3)
                NK_FORCE_INLINE T& operator[](size_t index) noexcept
                {
                    return data[index & 3];
                }

                // Opérateur d'indexation constant avec masquage de borne (0-3)
                NK_FORCE_INLINE const T& operator[](size_t index) const noexcept
                {
                    return data[index & 3];
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs arithmétiques binaires
                // -----------------------------------------------------------------
                // Addition composante par composante (rarement utile pour quaternions)
                NK_FORCE_INLINE NkQuatT operator+(const NkQuatT& other) const noexcept
                {
                    return {
                        x + other.x,
                        y + other.y,
                        z + other.z,
                        w + other.w
                    };
                }

                // Soustraction composante par composante (rarement utile pour quaternions)
                NK_FORCE_INLINE NkQuatT operator-(const NkQuatT& other) const noexcept
                {
                    return {
                        x - other.x,
                        y - other.y,
                        z - other.z,
                        w - other.w
                    };
                }

                // Produit de Hamilton : composition de rotations (this ⊗ other)
                // Convention : 'other' est appliqué APRÈS 'this' (ordre GLM/Unity)
                // Formule : q⊗r = (r.w*q.v + q.w*r.v + q.v×r.v, q.w*r.w - q.v·r.v)
                NkQuatT operator*(const NkQuatT& other) const noexcept;

                // Multiplication par scalaire : scale chaque composante
                NK_FORCE_INLINE NkQuatT operator*(T scalar) const noexcept
                {
                    return {
                        x * scalar,
                        y * scalar,
                        z * scalar,
                        w * scalar
                    };
                }

                // Division par scalaire : divise chaque composante (optimisée par inversion)
                NK_FORCE_INLINE NkQuatT operator/(T scalar) const noexcept
                {
                    T inverse = T(1) / scalar;
                    return {
                        x * inverse,
                        y * inverse,
                        z * inverse,
                        w * inverse
                    };
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs d'affectation composée
                // -----------------------------------------------------------------
                NK_FORCE_INLINE NkQuatT& operator+=(const NkQuatT& other) noexcept
                {
                    *this = *this + other;
                    return *this;
                }

                NK_FORCE_INLINE NkQuatT& operator-=(const NkQuatT& other) noexcept
                {
                    *this = *this - other;
                    return *this;
                }

                NK_FORCE_INLINE NkQuatT& operator*=(const NkQuatT& other) noexcept
                {
                    *this = *this * other;
                    return *this;
                }

                NK_FORCE_INLINE NkQuatT& operator*=(T scalar) noexcept
                {
                    *this = *this * scalar;
                    return *this;
                }

                // -----------------------------------------------------------------
                // Section : Rotation de vecteur (application du quaternion)
                // -----------------------------------------------------------------
                // Applique la rotation représentée par ce quaternion au vecteur v
                // Utilise la formule de Rodrigues optimisée (15 multiplications)
                // Formule : v' = 2(q.v)v + (s²-|q.v|²)v + 2s(q.v×v)
                // Plus efficace que q*[v,0]*q⁻¹ qui nécessite 28 multiplications
                NkVec3T<T> operator*(const NkVec3T<T>& vector) const noexcept;

                // Multiplication scalaire commutative (fonction amie)
                friend NK_FORCE_INLINE NkQuatT operator*(
                    T scalar,
                    const NkQuatT& quaternion
                ) noexcept {
                    return quaternion * scalar;
                }

                // Multiplication vecteur × quaternion (alias pour quaternion * vecteur)
                // Permet la syntaxe : rotated = vector * quaternion
                friend NK_FORCE_INLINE NkVec3T<T> operator*(
                    const NkVec3T<T>& vector,
                    const NkQuatT& quaternion
                ) noexcept {
                    return quaternion * vector;
                }

                // -----------------------------------------------------------------
                // Section : Exponentiation et opérateurs spéciaux
                // -----------------------------------------------------------------
                // Exponentiation scalaire : q^t pour interpolation angulaire
                // Formule : q^t = (sin(acos(w)*t)/sin(acos(w))) * v_n + cos(acos(w)*t)
                // Utilisé en interne par SLerp pour interpolation sphérique
                friend NkQuatT operator^(const NkQuatT& quaternion, T exponent) noexcept;

                // -----------------------------------------------------------------
                // Section : Opérateurs de comparaison
                // -----------------------------------------------------------------
                // Égalité : comparaison stricte des quatre composantes via NkVec4T
                friend NK_FORCE_INLINE bool operator==(
                    const NkQuatT& left,
                    const NkQuatT& right
                ) noexcept {
                    return left.quat == right.quat;
                }

                // Inégalité : déléguée à l'opérateur d'égalité (principe DRY)
                friend NK_FORCE_INLINE bool operator!=(
                    const NkQuatT& left,
                    const NkQuatT& right
                ) noexcept {
                    return !(left == right);
                }

                // -----------------------------------------------------------------
                // Section : Conversions vers autres types
                // -----------------------------------------------------------------
                // Conversion explicite vers matrice de rotation 4×4 column-major
                // Génère la matrice de rotation équivalente au quaternion
                explicit operator NkMat4T<T>() const noexcept;

                // Conversion explicite vers angles d'Euler avec gestion du gimbal lock
                // Détecte les singularités à ±90° de Yaw et applique la résolution adaptée
                explicit operator NkEulerAngle() const noexcept;

                // -----------------------------------------------------------------
                // Section : Requêtes géométriques (const)
                // -----------------------------------------------------------------
                // Produit scalaire (dot product) entre deux quaternions
                // Utile pour mesurer la similarité angulaire entre rotations
                NK_FORCE_INLINE T Dot(const NkQuatT& other) const noexcept
                {
                    return x * other.x
                         + y * other.y
                         + z * other.z
                         + w * other.w;
                }

                // Carré de la norme : évite un sqrt coûteux pour les comparaisons
                NK_FORCE_INLINE T LenSq() const noexcept
                {
                    return x * x + y * y + z * z + w * w;
                }

                // Norme euclidienne : longueur du quaternion dans R⁴
                NK_FORCE_INLINE T Len() const noexcept
                {
                    T lengthSquared = LenSq();
                    if (lengthSquared < T(NkQuatEpsilon)) {
                        return T(0);
                    }
                    return static_cast<T>(
                        NkSqrt(static_cast<float32>(lengthSquared))
                    );
                }

                // Test de normalisation : vérifie si ||q|| ≈ 1 dans la tolérance
                NK_FORCE_INLINE bool IsNormalized() const noexcept
                {
                    return NkFabs(
                        static_cast<float32>(LenSq()) - 1.0f
                    ) < 2.0f * static_cast<float32>(NkEpsilon);
                }

                // Détection du pôle de Gimbal pour conversion Euler
                // Retourne : +1 = pôle Nord (Yaw=+90°), -1 = pôle Sud (Yaw=-90°), 0 = normal
                int GimbalPole() const noexcept;

                // Angle de rotation représenté par ce quaternion (en radians)
                // Formule : angle = 2 * acos(|w|)
                NkAngle Angle() const noexcept;

                // Axe de rotation normalisé extrait du quaternion
                // Retourne un axe arbitraire (0,1,0) si quaternion identité
                NkVec3T<T> Axis() const noexcept;

                // -----------------------------------------------------------------
                // Section : Opérations non-mutantes (retournent une copie)
                // -----------------------------------------------------------------
                // Normalisation en place : modifie ce quaternion pour avoir ||q|| = 1
                void Normalize() noexcept;

                // Normalisation immuable : retourne une copie normalisée
                NkQuatT Normalized() const noexcept
                {
                    NkQuatT copy(*this);
                    copy.Normalize();
                    return copy;
                }

                // Conjugué : inverse la partie vectorielle (x,y,z → -x,-y,-z)
                // Pour un quaternion unitaire : Conjugate() == Inverse()
                NK_FORCE_INLINE NkQuatT Conjugate() const noexcept
                {
                    return {
                        -x,
                        -y,
                        -z,
                        w
                    };
                }

                // Inverse : pour un quaternion unitaire, égal au conjugué
                // Plus rapide que l'inverse général car évite la division par la norme
                NK_FORCE_INLINE NkQuatT Inverse() const noexcept
                {
                    return Conjugate();
                }

                // -----------------------------------------------------------------
                // Section : Interpolation entre rotations
                // -----------------------------------------------------------------
                // Mix linéaire : interpolation linéaire non normalisée
                // Utile comme étape intermédiaire avant normalisation (NLerp)
                NK_FORCE_INLINE NkQuatT Mix(
                    const NkQuatT& target,
                    T interpolationFactor
                ) const noexcept {
                    return (*this) * (T(1) - interpolationFactor)
                         + target * interpolationFactor;
                }

                // Lerp : alias sémantique pour Mix (Linear interpolation)
                NK_FORCE_INLINE NkQuatT Lerp(
                    const NkQuatT& target,
                    T interpolationFactor
                ) const noexcept {
                    return Mix(target, interpolationFactor);
                }

                // NLerp : Lerp suivi d'une normalisation
                // Plus rapide que SLerp mais vitesse angulaire non constante
                NK_FORCE_INLINE NkQuatT NLerp(
                    const NkQuatT& target,
                    T interpolationFactor
                ) const noexcept {
                    return Mix(target, interpolationFactor).Normalized();
                }

                // SLerp : Spherical Linear Interpolation (Shoemake 1985)
                // Interpolation à vitesse angulaire constante sur la sphère unitaire
                // Garantit le chemin le plus court : si Dot < 0, utilise -target
                NkQuatT SLerp(
                    const NkQuatT& target,
                    T interpolationFactor
                ) const noexcept;

                // -----------------------------------------------------------------
                // Section : Directions locales transformées
                // -----------------------------------------------------------------
                // Retourne la direction Forward locale transformée par cette rotation
                // Équivalent à : (*this) * NkVec3T<T>(0, 0, 1)
                NK_FORCE_INLINE NkVec3T<T> Forward() const noexcept
                {
                    return (*this) * NkVec3T<T>(T(0), T(0), T(1));
                }

                // Retourne la direction Back locale transformée
                NK_FORCE_INLINE NkVec3T<T> Back() const noexcept
                {
                    return (*this) * NkVec3T<T>(T(0), T(0), T(-1));
                }

                // Retourne la direction Up locale transformée
                NK_FORCE_INLINE NkVec3T<T> Up() const noexcept
                {
                    return (*this) * NkVec3T<T>(T(0), T(1), T(0));
                }

                // Retourne la direction Down locale transformée
                NK_FORCE_INLINE NkVec3T<T> Down() const noexcept
                {
                    return (*this) * NkVec3T<T>(T(0), T(-1), T(0));
                }

                // Retourne la direction Right locale transformée
                NK_FORCE_INLINE NkVec3T<T> Right() const noexcept
                {
                    return (*this) * NkVec3T<T>(T(1), T(0), T(0));
                }

                // Retourne la direction Left locale transformée
                NK_FORCE_INLINE NkVec3T<T> Left() const noexcept
                {
                    return (*this) * NkVec3T<T>(T(-1), T(0), T(0));
                }

                // -----------------------------------------------------------------
                // Section : Fabriques statiques (méthodes de construction)
                // -----------------------------------------------------------------
                // Quaternion identité statique : pas de rotation
                static NK_FORCE_INLINE NkQuatT Identity() noexcept
                {
                    return { T(0), T(0), T(0), T(1) };
                }

                // Rotation autour de l'axe X (pitch / tangage)
                static NkQuatT RotateX(const NkAngle& pitchAngle) noexcept;

                // Rotation autour de l'axe Y (yaw / lacet)
                static NkQuatT RotateY(const NkAngle& yawAngle) noexcept;

                // Rotation autour de l'axe Z (roll / roulis)
                static NkQuatT RotateZ(const NkAngle& rollAngle) noexcept;

                // LookAt depuis direction + up de référence
                // Crée un quaternion orientant l'objet pour regarder dans 'direction'
                // tout en alignant son axe Up local avec 'up' (autant que possible)
                static NkQuatT LookAt(
                    const NkVec3T<T>& lookDirection,
                    const NkVec3T<T>& referenceUp
                ) noexcept;

                // LookAt depuis position vers cible + up de référence
                // Alias pratique : calcule direction = target - position
                static NkQuatT LookAt(
                    const NkVec3T<T>& eyePosition,
                    const NkVec3T<T>& targetPosition,
                    const NkVec3T<T>& referenceUp
                ) noexcept;

                // Quaternion de réflexion par rapport à un plan de normale donnée
                // Utile pour les miroirs, les rebonds, les effets de symétrie
                static NkQuatT Reflection(const NkVec3T<T>& planeNormal) noexcept;

                // -----------------------------------------------------------------
                // Section : Sérialisation et E/S
                // -----------------------------------------------------------------
                // Conversion en chaîne formatée pour débogage et logging
                // Format : "[x, y, z; w]" avec w séparé pour lisibilité
                NkString ToString() const
                {
                    return NkFormat(
                        "[{0}, {1}, {2}; {3}]",
                        x,
                        y,
                        z,
                        w
                    );
                }

                // Surcharge globale de ToString pour appel fonctionnel libre
                friend NkString ToString(const NkQuatT& quaternion)
                {
                    return quaternion.ToString();
                }

                // Opérateur de flux pour affichage dans std::ostream
                friend std::ostream& operator<<(
                    std::ostream& outputStream,
                    const NkQuatT& quaternion
                ) {
                    return outputStream << quaternion.ToString().CStr();
                }

                // -----------------------------------------------------------------
                // Section : Membres privés (utilitaires internes)
                // -----------------------------------------------------------------
                private:

                    // ClampEpsilon : remplace les composantes quasi-nulles par zéro exact
                    // Évite l'accumulation d'erreurs d'arrondi flottantes
                    // Appelée après les calculs sensibles pour nettoyer le résultat
                    void ClampEpsilon() noexcept;

            };  // struct NkQuatT

            // =================================================================
            // Implémentations des méthodes template (dans header car template)
            // =================================================================

            // -----------------------------------------------------------------
            // Méthode : ClampEpsilon
            // -----------------------------------------------------------------
            // Remplace chaque composante dont la valeur absolue est <= NkEpsilon
            // par zéro exact pour éviter l'accumulation d'erreurs d'arrondi.
            // Utile après des opérations trigonométriques ou des normalisations.
            // -----------------------------------------------------------------
            template <typename T>
            void NkQuatT<T>::ClampEpsilon() noexcept
            {
                const T epsilon = static_cast<T>(NkEpsilon);
                if (NkFabs(static_cast<float32>(x)) <= static_cast<float32>(epsilon)) {
                    x = T(0);
                }
                if (NkFabs(static_cast<float32>(y)) <= static_cast<float32>(epsilon)) {
                    y = T(0);
                }
                if (NkFabs(static_cast<float32>(z)) <= static_cast<float32>(epsilon)) {
                    z = T(0);
                }
                if (NkFabs(static_cast<float32>(w)) <= static_cast<float32>(epsilon)) {
                    w = T(0);
                }
            }

            // -----------------------------------------------------------------
            // Constructeur : angle-axe
            // -----------------------------------------------------------------
            // Crée un quaternion représentant une rotation de 'angle' radians
            // autour de l'axe 'axis' (qui sera normalisé en interne).
            //
            // Formule mathématique :
            //   q = (axis_normalized * sin(angle/2), cos(angle/2))
            //
            // Paramètres :
            //   rotationAngle : angle de rotation (type NkAngle)
            //   rotationAxis  : axe de rotation (sera normalisé)
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T>::NkQuatT(
                const NkAngle& rotationAngle,
                const NkVec3T<T>& rotationAxis
            ) noexcept {
                // Normalisation de l'axe de rotation
                NkVec3T<T> normalizedAxis = rotationAxis.Normalized();

                // Gestion du cas dégénéré : axe nul → retourne identité
                if (normalizedAxis.Len() == T(0)) {
                    *this = Identity();
                    return;
                }

                // Calcul des composantes via formules trigonométriques
                float32 halfAngle = rotationAngle.Rad() * 0.5f;
                vector = normalizedAxis * static_cast<T>(NkSin(halfAngle));
                scalar = static_cast<T>(NkCos(halfAngle));

                // Nettoyage des erreurs d'arrondi infimes
                ClampEpsilon();
            }

            // -----------------------------------------------------------------
            // Constructeur : Euler → Quaternion
            // -----------------------------------------------------------------
            // Convertit des angles d'Euler (convention YXZ : yaw→pitch→roll)
            // en quaternion équivalent.
            //
            // Ordre d'application des rotations :
            //   1. Yaw   (rotation autour de Y) : lacet gauche/droite
            //   2. Pitch (rotation autour de X) : tangage haut/bas
            //   3. Roll  (rotation autour de Z) : roulis inclinaison
            //
            // Formule : q = q_roll ⊗ q_pitch ⊗ q_yaw
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T>::NkQuatT(const NkEulerAngle& euler) noexcept
            {
                // Pré-calcul des sinus et cosinus des demi-angles
                float32 cosYaw   = NkCos(euler.yaw.Rad()   * 0.5f);
                float32 sinYaw   = NkSin(euler.yaw.Rad()   * 0.5f);
                float32 cosPitch = NkCos(euler.pitch.Rad() * 0.5f);
                float32 sinPitch = NkSin(euler.pitch.Rad() * 0.5f);
                float32 cosRoll  = NkCos(euler.roll.Rad()  * 0.5f);
                float32 sinRoll  = NkSin(euler.roll.Rad()  * 0.5f);

                // Composition des trois quaternions élémentaires
                // Formule développée pour éviter les allocations temporaires
                w = static_cast<T>(
                    cosRoll * cosYaw * cosPitch + sinRoll * sinYaw * sinPitch
                );
                x = static_cast<T>(
                    cosRoll * cosYaw * sinPitch - sinRoll * sinYaw * cosPitch
                );
                y = static_cast<T>(
                    cosRoll * sinYaw * cosPitch + sinRoll * cosYaw * sinPitch
                );
                z = static_cast<T>(
                    sinRoll * cosYaw * cosPitch - cosRoll * sinYaw * sinPitch
                );

                // Nettoyage des erreurs d'arrondi infimes
                ClampEpsilon();
            }

            // -----------------------------------------------------------------
            // Constructeur : Matrice → Quaternion
            // -----------------------------------------------------------------
            // Extrait un quaternion de rotation depuis une matrice 4×4.
            // Ignore la translation et le scale, ne conserve que la rotation pure.
            //
            // Algorithme :
            //   1. Extraire et normaliser les axes up et forward de la matrice
            //   2. Reconstruire l'axe right par produit vectoriel
            //   3. Ré-orthogonaliser up pour garantir un repère orthonormé
            //   4. Utiliser LookAt(forward, up) pour construire le quaternion
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T>::NkQuatT(const NkMat4T<T>& matrix) noexcept
            {
                // Extraction et normalisation des axes depuis la matrice
                NkVec3T<T> upAxis = matrix.up.xyz().Normalized();
                NkVec3T<T> forwardAxis = matrix.forward.xyz().Normalized();

                // Reconstruction de l'axe right par produit vectoriel
                NkVec3T<T> rightAxis = upAxis.Cross(forwardAxis);

                // Ré-orthogonalisation de up pour garantir l'orthonormalité
                upAxis = forwardAxis.Cross(rightAxis);

                // Construction du quaternion via LookAt
                *this = LookAt(forwardAxis, upAxis);

                // Nettoyage des erreurs d'arrondi infimes
                ClampEpsilon();
            }

            // -----------------------------------------------------------------
            // Constructeur : from-to (rotation minimale)
            // -----------------------------------------------------------------
            // Crée le quaternion représentant la plus petite rotation amenant
            // le vecteur 'from' à coïncider avec le vecteur 'to'.
            //
            // Cas particuliers gérés :
            //   - from == to      → retourne Identity() (pas de rotation)
            //   - from == -to     → rotation 180° autour d'un axe perpendiculaire
            //
            // Algorithme général :
            //   half = normalize(from + to)
            //   q.v = from × half
            //   q.w = from · half
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T>::NkQuatT(
                const NkVec3T<T>& fromVector,
                const NkVec3T<T>& toVector
            ) noexcept {
                // Normalisation des vecteurs d'entrée
                NkVec3T<T> normalizedFrom = fromVector.Normalized();
                NkVec3T<T> normalizedTo = toVector.Normalized();

                // Cas 1 : vecteurs identiques → pas de rotation nécessaire
                if (normalizedFrom == normalizedTo) {
                    *this = Identity();
                    return;
                }

                // Cas 2 : vecteurs opposés → rotation 180° (cas singulier)
                if (normalizedFrom == normalizedTo * T(-1)) {
                    // Choix d'un axe arbitraire perpendiculaire à from
                    NkVec3T<T> orthogonalAxis = { T(1), T(0), T(0) };
                    if (NkFabs(static_cast<float32>(normalizedFrom.y))
                        < NkFabs(static_cast<float32>(normalizedFrom.x))) {
                        orthogonalAxis = { T(0), T(1), T(0) };
                    }
                    if (NkFabs(static_cast<float32>(normalizedFrom.z))
                        < NkFabs(static_cast<float32>(normalizedFrom.y))
                        && NkFabs(static_cast<float32>(normalizedFrom.z))
                        < NkFabs(static_cast<float32>(normalizedFrom.x))) {
                        orthogonalAxis = { T(0), T(0), T(1) };
                    }
                    // Quaternion 180° : axe normalisé, scalaire = 0
                    vector = normalizedFrom.Cross(orthogonalAxis).Normalized();
                    scalar = T(0);
                } else {
                    // Cas général : rotation minimale via demi-vecteur
                    NkVec3T<T> halfVector = (normalizedFrom + normalizedTo).Normalized();
                    vector = normalizedFrom.Cross(halfVector);
                    scalar = normalizedFrom.Dot(halfVector);
                }

                // Nettoyage des erreurs d'arrondi infimes
                ClampEpsilon();
            }

            // -----------------------------------------------------------------
            // Opérateur : Produit de Hamilton
            // -----------------------------------------------------------------
            // Composition de deux rotations quaternioniques : this ⊗ other
            // Convention : 'other' est appliqué APRÈS 'this' (ordre GLM/Unity)
            //
            // Formule développée :
            //   w = w₁w₂ - x₁x₂ - y₁y₂ - z₁z₂
            //   x = w₁x₂ + x₁w₂ + y₁z₂ - z₁y₂
            //   y = w₁y₂ - x₁z₂ + y₁w₂ + z₁x₂
            //   z = w₁z₂ + x₁y₂ - y₁x₂ + z₁w₂
            //
            // Retour :
            //   Nouveau quaternion représentant la rotation composée
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T> NkQuatT<T>::operator*(const NkQuatT& other) const noexcept
            {
                return {
                    other.x * w + other.y * z - other.z * y + other.w * x,
                    -other.x * z + other.y * w + other.z * x + other.w * y,
                    other.x * y - other.y * x + other.z * w + other.w * z,
                    -other.x * x - other.y * y - other.z * z + other.w * w
                };
            }

            // -----------------------------------------------------------------
            // Opérateur : Rotation de vecteur (Rodrigues optimisé)
            // -----------------------------------------------------------------
            // Applique la rotation représentée par ce quaternion au vecteur v.
            // Utilise la formule de Rodrigues optimisée en 15 multiplications.
            //
            // Formule : v' = 2(q.v)v + (s²-|q.v|²)v + 2s(q.v×v)
            //
            // Avantages vs méthode naïve q*[v,0]*q⁻¹ :
            //   - 15 multiplications au lieu de 28
            //   - Pas d'allocation temporaire de quaternion 4D
            //   - Meilleure stabilité numérique pour les vecteurs courts
            //
            // Paramètre :
            //   vector : vecteur 3D à transformer
            // Retour :
            //   Nouveau vecteur 3D après application de la rotation
            // -----------------------------------------------------------------
            template <typename T>
            NkVec3T<T> NkQuatT<T>::operator*(const NkVec3T<T>& vector) const noexcept
            {
                // Formule de Rodrigues optimisée (Vince 2011)
                return vector * (T(2) * vector.Dot(vector))
                     + vector * (scalar * scalar - vector.Dot(vector))
                     + vector.Cross(vector) * (T(2) * scalar);
            }

            // -----------------------------------------------------------------
            // Opérateur : Exponentiation q^t (fonction amie)
            // -----------------------------------------------------------------
            // Exponentiation scalaire d'un quaternion : q^t
            // Utilisé en interne par SLerp pour interpolation sphérique.
            //
            // Formule :
            //   Soit θ = acos(w), alors :
            //   q^t = (sin(t*θ)/sin(θ)) * v_n + cos(t*θ)
            //   où v_n = v / |v| est l'axe normalisé
            //
            // Protection numérique :
            //   - Clamp de w dans [-1, 1] avant acos pour éviter NaN
            //   - Gestion implicite du cas |v| ≈ 0 via Normalized()
            //
            // Paramètres :
            //   quaternion : quaternion de base
            //   exponent   : exposant scalaire t ∈ [0, 1] typiquement
            // Retour :
            //   Quaternion résultant de l'exponentiation
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T> operator^(const NkQuatT<T>& quaternion, T exponent) noexcept
            {
                // Clamp de w pour protéger acos contre les erreurs d'arrondi
                float32 clampedW = NkClamp(
                    static_cast<float32>(quaternion.scalar),
                    -1.0f,
                    1.0f
                );

                // Calcul de l'angle interpolé
                float32 halfAngle = NkAcos(clampedW) * static_cast<float32>(exponent);

                // Extraction et normalisation de l'axe de rotation
                NkVec3T<T> rotationAxis = quaternion.vector.Normalized();

                // Construction du quaternion résultant
                return {
                    rotationAxis.x * static_cast<T>(NkSin(halfAngle)),
                    rotationAxis.y * static_cast<T>(NkSin(halfAngle)),
                    rotationAxis.z * static_cast<T>(NkSin(halfAngle)),
                    static_cast<T>(NkCos(halfAngle))
                };
            }

            // -----------------------------------------------------------------
            // Conversion : Quaternion → Matrice 4×4
            // -----------------------------------------------------------------
            // Génère la matrice de rotation 4×4 column-major équivalente
            // à ce quaternion unitaire.
            //
            // Formule (optimisée pour éviter les calculs redondants) :
            //   xx=x², yy=y², zz=z², xy=x*y, xz=x*z, yz=y*z
            //   wx=w*x, wy=w*y, wz=w*z
            //
            //   [ 1-2(yy+zz)   2(xy+wz)     2(xz-wy)     0 ]
            //   [ 2(xy-wz)     1-2(xx+zz)   2(yz+wx)     0 ]
            //   [ 2(xz+wy)     2(yz-wx)     1-2(xx+yy)   0 ]
            //   [ 0            0            0            1 ]
            //
            // Retour :
            //   NkMat4T<T> contenant la matrice de rotation équivalente
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T>::operator NkMat4T<T>() const noexcept
            {
                // Pré-calcul des produits pour optimisation
                T xx = x * x;
                T yy = y * y;
                T zz = z * z;
                T xy = x * y;
                T xz = x * z;
                T yz = y * z;
                T wx = w * x;
                T wy = w * y;
                T wz = w * z;

                // Construction de la matrice column-major
                return NkMat4T<T>(
                    // Colonne 0 (axe X)          Colonne 1 (axe Y)          Colonne 2 (axe Z)          Colonne 3 (translation)
                    T(1) - T(2) * (yy + zz),     T(2) * (xy + wz),          T(2) * (xz - wy),          T(0),
                    T(2) * (xy - wz),            T(1) - T(2) * (xx + zz),   T(2) * (yz + wx),          T(0),
                    T(2) * (xz + wy),            T(2) * (yz - wx),          T(1) - T(2) * (xx + yy),   T(0),
                    T(0),                        T(0),                      T(0),                      T(1)
                );
            }

            // -----------------------------------------------------------------
            // Conversion : Quaternion → Euler
            // -----------------------------------------------------------------
            // Extrait les angles d'Euler (pitch/yaw/roll) depuis ce quaternion.
            // Gère les singularités de gimbal lock via détection du pôle.
            //
            // Algorithme :
            //   1. Normaliser le quaternion pour garantir la stabilité
            //   2. Calculer le test de gimbal : t = y*x + z*w
            //   3. Si |t| > 0.499 → singularité Nord/Sud → résolution spéciale
            //   4. Sinon → conversion standard via formules trigonométriques
            //
            // Convention de sortie :
            //   - pitch : rotation autour de X (tangage)
            //   - yaw   : rotation autour de Y (lacet)
            //   - roll  : rotation autour de Z (roulis)
            //
            // Retour :
            //   NkEulerAngle contenant les trois angles extraits
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T>::operator NkEulerAngle() const noexcept
            {
                // Normalisation pour garantir la stabilité numérique
                NkQuatT<T> normalizedQuat = Normalized();
                NkEulerAngle extractedAngles;

                // Test de détection du pôle de Gimbal
                float32 gimbalTest = static_cast<float32>(
                    normalizedQuat.y * normalizedQuat.x
                    + normalizedQuat.z * normalizedQuat.w
                );

                if (gimbalTest > 0.499f) {
                    // -----------------------------------------------------------------
                    // Singularité Nord : Yaw = +90° (pôle +Z)
                    // Roll et Pitch dégénèrent : on fixe Roll à 0 et on calcule Pitch
                    // -----------------------------------------------------------------
                    extractedAngles.yaw = NkAngle::FromRad(
                        static_cast<float32>(NkPis2)
                    );
                    extractedAngles.pitch = NkAngle::FromRad(
                        2.0f * NkAtan2(
                            static_cast<float32>(normalizedQuat.x),
                            static_cast<float32>(normalizedQuat.w)
                        )
                    );
                    extractedAngles.roll = NkAngle(0.0f);

                } else if (gimbalTest < -0.499f) {
                    // -----------------------------------------------------------------
                    // Singularité Sud : Yaw = -90° (pôle -Z)
                    // Roll et Pitch dégénèrent : on fixe Roll à 0 et on calcule Pitch
                    // -----------------------------------------------------------------
                    extractedAngles.yaw = NkAngle::FromRad(
                        -static_cast<float32>(NkPis2)
                    );
                    extractedAngles.pitch = NkAngle::FromRad(
                        -2.0f * NkAtan2(
                            static_cast<float32>(normalizedQuat.x),
                            static_cast<float32>(normalizedQuat.w)
                        )
                    );
                    extractedAngles.roll = NkAngle(0.0f);

                } else {
                    // -----------------------------------------------------------------
                    // Cas général : pas de singularité, conversion standard
                    // -----------------------------------------------------------------
                    // Pitch (rotation autour de X)
                    float32 sinPitch = 2.0f * (
                        static_cast<float32>(normalizedQuat.w * normalizedQuat.x)
                        + static_cast<float32>(normalizedQuat.y * normalizedQuat.z)
                    );
                    float32 cosPitch = 1.0f - 2.0f * (
                        static_cast<float32>(normalizedQuat.x * normalizedQuat.x)
                        + static_cast<float32>(normalizedQuat.y * normalizedQuat.y)
                    );
                    extractedAngles.pitch = NkAngle::FromRad(
                        NkAtan2(sinPitch, cosPitch)
                    );

                    // Yaw (rotation autour de Y) avec clamp pour protéger asin
                    float32 sinYaw = NkClamp(
                        2.0f * static_cast<float32>(
                            normalizedQuat.w * normalizedQuat.y
                            - normalizedQuat.z * normalizedQuat.x
                        ),
                        -1.0f,
                        1.0f
                    );
                    extractedAngles.yaw = NkAngle::FromRad(
                        NkAsin(sinYaw)
                    );

                    // Roll (rotation autour de Z)
                    float32 sinRoll = 2.0f * (
                        static_cast<float32>(normalizedQuat.w * normalizedQuat.z)
                        + static_cast<float32>(normalizedQuat.x * normalizedQuat.y)
                    );
                    float32 cosRoll = 1.0f - 2.0f * (
                        static_cast<float32>(normalizedQuat.y * normalizedQuat.y)
                        + static_cast<float32>(normalizedQuat.z * normalizedQuat.z)
                    );
                    extractedAngles.roll = NkAngle::FromRad(
                        NkAtan2(sinRoll, cosRoll)
                    );
                }

                return extractedAngles;
            }

            // -----------------------------------------------------------------
            // Méthode : GimbalPole
            // -----------------------------------------------------------------
            // Détecte la proximité d'une singularité de gimbal lock.
            // Utilisé pour choisir la branche de conversion Quaternion→Euler.
            //
            // Critère : t = y*x + z*w
            //   t > +0.499 → pôle Nord (Yaw ≈ +90°)
            //   t < -0.499 → pôle Sud (Yaw ≈ -90°)
            //   sinon      → cas normal
            //
            // Retour :
            //   +1 = pôle Nord, -1 = pôle Sud, 0 = pas de singularité
            // -----------------------------------------------------------------
            template <typename T>
            int NkQuatT<T>::GimbalPole() const noexcept
            {
                float32 gimbalTest = static_cast<float32>(y * x + z * w);
                if (gimbalTest > 0.499f) {
                    return 1;   // Pôle Nord
                }
                if (gimbalTest < -0.499f) {
                    return -1;  // Pôle Sud
                }
                return 0;       // Cas normal
            }

            // -----------------------------------------------------------------
            // Méthode : Angle
            // -----------------------------------------------------------------
            // Retourne l'angle de rotation représenté par ce quaternion.
            // Formule : angle = 2 * acos(|w|)
            //
            // Protection numérique :
            //   - Clamp de w dans [-1, 1] avant acos pour éviter NaN
            //   - Utilisation de NkFabs pour gérer q et -q (même rotation)
            //
            // Retour :
            //   NkAngle contenant l'angle de rotation en radians
            // -----------------------------------------------------------------
            template <typename T>
            NkAngle NkQuatT<T>::Angle() const noexcept
            {
                float32 clampedW = NkClamp(
                    static_cast<float32>(w),
                    -1.0f,
                    1.0f
                );
                return NkAngle::FromRad(
                    2.0f * NkAcos(NkFabs(clampedW))
                );
            }

            // -----------------------------------------------------------------
            // Méthode : Axis
            // -----------------------------------------------------------------
            // Extrait l'axe de rotation normalisé depuis ce quaternion.
            // Formule : axis = (x, y, z) / sqrt(1 - w²)
            //
            // Cas particulier :
            //   Si |w| ≈ 1 (quaternion identité ou 360°), l'axe est indéfini.
            //   On retourne alors un axe arbitraire (0, 1, 0) par convention.
            //
            // Retour :
            //   NkVec3T<T> contenant l'axe de rotation normalisé
            // -----------------------------------------------------------------
            template <typename T>
            NkVec3T<T> NkQuatT<T>::Axis() const noexcept
            {
                float32 sinSquared = 1.0f - static_cast<float32>(w * w);
                if (sinSquared < static_cast<float32>(NkEpsilon)) {
                    // Quaternion identité → axe arbitraire par convention
                    return { T(0), T(1), T(0) };
                }
                float32 inverseSin = 1.0f / NkSqrt(sinSquared);
                return {
                    x * static_cast<T>(inverseSin),
                    y * static_cast<T>(inverseSin),
                    z * static_cast<T>(inverseSin)
                };
            }

            // -----------------------------------------------------------------
            // Méthode : Normalize
            // -----------------------------------------------------------------
            // Normalise ce quaternion en place pour garantir ||q|| = 1.
            // Essentiel après des opérations arithmétiques qui peuvent
            // introduire une dérive numérique.
            //
            // Protection :
            //   Si ||q||² < NkQuatEpsilon, retourne sans modifier (évite div/0)
            // -----------------------------------------------------------------
            template <typename T>
            void NkQuatT<T>::Normalize() noexcept
            {
                float32 lengthSquared = static_cast<float32>(LenSq());
                if (lengthSquared < static_cast<float32>(NkQuatEpsilon)) {
                    return;  // Quaternion trop court → pas de normalisation possible
                }
                T inverseLength = T(1) / static_cast<T>(NkSqrt(lengthSquared));
                x *= inverseLength;
                y *= inverseLength;
                z *= inverseLength;
                w *= inverseLength;
            }

            // -----------------------------------------------------------------
            // Méthode : SLerp
            // -----------------------------------------------------------------
            // Spherical Linear Interpolation : interpolation à vitesse angulaire
            // constante sur la sphère unitaire des quaternions.
            //
            // Algorithme (Shoemake 1985) :
            //   1. Garantir le chemin court : si Dot < 0, utiliser -target
            //   2. Si quaternions quasi-identiques → fallback vers NLerp (plus stable)
            //   3. Sinon : q_result = (q_start ⊗ (q_start⁻¹ ⊗ q_end)^t).Normalized()
            //
            // Avantages vs Lerp/NLerp :
            //   - Vitesse angulaire constante (mouvement naturel)
            //   - Trajectoire géodésique sur la sphère (plus courte)
            //   - Évite le "ralentissement" aux extrémités de Lerp
            //
            // Paramètres :
            //   target            : quaternion destination
            //   interpolationFactor : t ∈ [0, 1] (0=start, 1=target)
            // Retour :
            //   Quaternion interpolé sur l'arc de grand cercle
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T> NkQuatT<T>::SLerp(
                const NkQuatT<T>& targetInput,
                T interpolationFactor
            ) const noexcept {
                // Étape 1 : Garantir le chemin le plus court sur la sphère
                // Si le produit scalaire est négatif, les quaternions sont
                // antipodaux : on inverse target pour prendre l'autre demi-sphère
                NkQuatT<T> target = targetInput;
                float32 dotProduct = static_cast<float32>(Dot(target));
                if (dotProduct < 0.0f) {
                    target = { -target.x, -target.y, -target.z, -target.w };
                    dotProduct = -dotProduct;
                }

                // Étape 2 : Fallback vers NLerp si quaternions quasi-identiques
                // Évite les instabilités numériques quand sin(θ) ≈ 0
                if (dotProduct > 1.0f - static_cast<float32>(NkQuatEpsilon)) {
                    return NLerp(target, interpolationFactor);
                }

                // Étape 3 : Interpolation sphérique via exponentiation
                // Formule : result = start ⊗ (start⁻¹ ⊗ target)^t
                NkQuatT<T> deltaQuaternion = Inverse() * target;
                NkQuatT<T> interpolated = (deltaQuaternion ^ interpolationFactor) * (*this);

                // Normalisation finale pour corriger la dérive numérique
                return interpolated.Normalized();
            }

            // -----------------------------------------------------------------
            // Fabrique : RotateX
            // -----------------------------------------------------------------
            // Crée un quaternion représentant une rotation pure autour de l'axe X.
            // Utile pour les rotations de tangage (pitch) en aéronautique.
            //
            // Formule : q = (sin(angle/2), 0, 0, cos(angle/2))
            //
            // Paramètre :
            //   pitchAngle : angle de rotation autour de X
            // Retour :
            //   Quaternion de rotation X pure
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T> NkQuatT<T>::RotateX(const NkAngle& pitchAngle) noexcept
            {
                float32 halfAngle = pitchAngle.Rad() * 0.5f;
                return {
                    static_cast<T>(NkSin(halfAngle)),
                    T(0),
                    T(0),
                    static_cast<T>(NkCos(halfAngle))
                };
            }

            // -----------------------------------------------------------------
            // Fabrique : RotateY
            // -----------------------------------------------------------------
            // Crée un quaternion représentant une rotation pure autour de l'axe Y.
            // Utile pour les rotations de lacet (yaw) en aéronautique.
            //
            // Formule : q = (0, sin(angle/2), 0, cos(angle/2))
            //
            // Paramètre :
            //   yawAngle : angle de rotation autour de Y
            // Retour :
            //   Quaternion de rotation Y pure
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T> NkQuatT<T>::RotateY(const NkAngle& yawAngle) noexcept
            {
                float32 halfAngle = yawAngle.Rad() * 0.5f;
                return {
                    T(0),
                    static_cast<T>(NkSin(halfAngle)),
                    T(0),
                    static_cast<T>(NkCos(halfAngle))
                };
            }

            // -----------------------------------------------------------------
            // Fabrique : RotateZ
            // -----------------------------------------------------------------
            // Crée un quaternion représentant une rotation pure autour de l'axe Z.
            // Utile pour les rotations de roulis (roll) en aéronautique.
            //
            // Formule : q = (0, 0, sin(angle/2), cos(angle/2))
            //
            // Paramètre :
            //   rollAngle : angle de rotation autour de Z
            // Retour :
            //   Quaternion de rotation Z pure
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T> NkQuatT<T>::RotateZ(const NkAngle& rollAngle) noexcept
            {
                float32 halfAngle = rollAngle.Rad() * 0.5f;
                return {
                    T(0),
                    T(0),
                    static_cast<T>(NkSin(halfAngle)),
                    static_cast<T>(NkCos(halfAngle))
                };
            }

            // -----------------------------------------------------------------
            // Fabrique : LookAt (direction + up)
            // -----------------------------------------------------------------
            // Crée un quaternion orientant un objet pour regarder dans 'direction'
            // tout en alignant son axe Up local avec 'referenceUp' autant que possible.
            //
            // Algorithme en deux étapes :
            //   1. Rotation q1 : aligne Forward monde (0,0,1) vers 'direction'
            //   2. Rotation q2 : aligne l'Up résultant vers 'referenceUp'
            //   Résultat : q = (q1 ⊗ q2).Normalized()
            //
            // Paramètres :
            //   lookDirection : direction dans laquelle regarder (sera normalisée)
            //   referenceUp   : vecteur up de référence pour l'orientation (sera normalisé)
            // Retour :
            //   Quaternion représentant l'orientation LookAt
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T> NkQuatT<T>::LookAt(
                const NkVec3T<T>& lookDirection,
                const NkVec3T<T>& referenceUp
            ) noexcept {
                // Normalisation des vecteurs d'entrée
                NkVec3T<T> forwardAxis = lookDirection.Normalized();
                NkVec3T<T> desiredUp = referenceUp.Normalized();

                // Construction d'un repère orthonormé complet
                NkVec3T<T> rightAxis = desiredUp.Cross(forwardAxis);
                NkVec3T<T> upAxis = forwardAxis.Cross(rightAxis);

                // Étape 1 : aligner l'axe Forward monde (0,0,1) vers la direction cible
                NkQuatT<T> rotationToForward(
                    NkVec3T<T>{ T(0), T(0), T(1) },
                    forwardAxis
                );

                // Étape 2 : corriger l'orientation Up après la première rotation
                NkVec3T<T> currentUp = rotationToForward * NkVec3T<T>{ T(0), T(1), T(0) };
                NkQuatT<T> rotationToUp(currentUp, desiredUp);

                // Composition des deux rotations et normalisation finale
                return (rotationToForward * rotationToUp).Normalized();
            }

            // -----------------------------------------------------------------
            // Fabrique : LookAt (position → target + up)
            // -----------------------------------------------------------------
            // Alias pratique de LookAt(direction, up) qui calcule automatiquement
            // la direction depuis une position d'œil vers une cible.
            //
            // Paramètres :
            //   eyePosition   : position de la caméra/objet
            //   targetPosition: point vers lequel regarder
            //   referenceUp   : vecteur up de référence
            // Retour :
            //   Quaternion LookAt équivalent à LookAt(target - eye, up)
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T> NkQuatT<T>::LookAt(
                const NkVec3T<T>& eyePosition,
                const NkVec3T<T>& targetPosition,
                const NkVec3T<T>& referenceUp
            ) noexcept {
                return LookAt(targetPosition - eyePosition, referenceUp);
            }

            // -----------------------------------------------------------------
            // Fabrique : Reflection
            // -----------------------------------------------------------------
            // Crée un quaternion représentant une réflexion par rapport à un plan
            // de normale donnée. Utile pour les effets de miroir, rebonds, etc.
            //
            // Formule : q = (normalizedNormal, 0)
            // C'est un quaternion "pur" (scalaire nul) représentant une rotation
            // de 180° autour de la normale, équivalente à une réflexion.
            //
            // Paramètre :
            //   planeNormal : normale du plan de réflexion (sera normalisée)
            // Retour :
            //   Quaternion de réflexion
            // -----------------------------------------------------------------
            template <typename T>
            NkQuatT<T> NkQuatT<T>::Reflection(const NkVec3T<T>& planeNormal) noexcept
            {
                return {
                    planeNormal.Normalized(),
                    T(0)
                };
            }

            // =================================================================
            // Aliases de types : variantes f=float32, d=float64
            // =================================================================
            using NkQuatf = NkQuatT<float32>;           // Quaternion float 32 bits (recommandé)
            using NkQuatd = NkQuatT<float64>;           // Quaternion float 64 bits (haute précision)
            using NkQuaternion = NkQuatf;               // Alias legacy (float32 par défaut)
            using NkQuat = NkQuatf;                     // Alias court
            using NkQuaternionf = NkQuatf;              // Alias legacy explicite float32
            using NkQuaterniond = NkQuatd;              // Alias legacy explicite float64

        }  // namespace math

        // =====================================================================
        // Spécialisation de NkToString pour intégration au système de formatage
        // =====================================================================
        // Permet d'utiliser NkQuatT<T> avec NkFormat() et les utilitaires d'E/S
        // du projet, avec support des options de formatage via NkFormatProps.
        // =====================================================================
        template <typename T>
        inline NkString NkToString(
            const math::NkQuatT<T>& quaternion,
            const NkFormatProps& formatProperties = {}
        ) {
            return NkApplyFormatProps(quaternion.ToString(), formatProperties);
        }

    }  // namespace nkentseu

#endif  // __NKENTSEU_QUATERNION_H__

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Création de quaternions avec différents constructeurs
    // ---------------------------------------------------------------------
    #include "NKMath/NkQuat.h"
    using namespace nkentseu::math;

    // Quaternion identité (pas de rotation)
    NkQuatf identity = NkQuatf::Identity();

    // Quaternion depuis angle-axe : rotation 90° autour de l'axe Y
    NkQuatf quarterTurnY = NkQuatf(
        NkAngle::FromDeg(90.0f),
        NkVec3f(0.0f, 1.0f, 0.0f)
    );

    // Quaternion depuis angles d'Euler (convention YXZ)
    NkEulerAngle euler(NkAngle::FromDeg(30), NkAngle::FromDeg(45), NkAngle::FromDeg(15));
    NkQuatf fromEuler(euler);

    // Quaternion depuis vecteur 4D explicite
    NkQuatf explicitQuat(0.0f, 0.707f, 0.0f, 0.707f);  // ~90° autour de Y

    // Quaternion de rotation minimale : from → to
    NkVec3f forward(0.0f, 0.0f, 1.0f);
    NkVec3f targetDir(1.0f, 0.0f, 1.0f).Normalized();
    NkQuatf alignRotation(forward, targetDir);

    // ---------------------------------------------------------------------
    // Exemple 2 : Application de rotations à des vecteurs
    // ---------------------------------------------------------------------
    NkQuatf rotation = NkQuatf::RotateY(NkAngle::FromDeg(45.0f));
    NkVec3f originalVector(1.0f, 0.0f, 0.0f);

    // Rotation via opérateur * (quaternion * vecteur)
    NkVec3f rotated = rotation * originalVector;

    // Syntaxe alternative : vecteur * quaternion (fonction amie)
    NkVec3f alsoRotated = originalVector * rotation;

    // Extraction des directions locales après rotation
    NkVec3f localForward = rotation.Forward();  // Direction Forward transformée
    NkVec3f localUp = rotation.Up();            // Direction Up transformée
    NkVec3f localRight = rotation.Right();      // Direction Right transformée

    // ---------------------------------------------------------------------
    // Exemple 3 : Composition de rotations (ordre important !)
    // ---------------------------------------------------------------------
    // Ordre d'application : rightmost appliqué en premier (convention GLM)
    // q_total = q_yaw ⊗ q_pitch ⊗ q_roll  →  roll d'abord, puis pitch, puis yaw

    NkQuatf roll  = NkQuatf::RotateZ(NkAngle::FromDeg(10.0f));
    NkQuatf pitch = NkQuatf::RotateX(NkAngle::FromDeg(20.0f));
    NkQuatf yaw   = NkQuatf::RotateY(NkAngle::FromDeg(30.0f));

    // Composition : yaw appliqué en dernier (le plus "extérieur")
    NkQuatf combined = yaw * pitch * roll;

    // Application à un vecteur
    NkVec3f result = combined * NkVec3f(0.0f, 0.0f, 1.0f);

    // ---------------------------------------------------------------------
    // Exemple 4 : Interpolation entre rotations (animation)
    // ---------------------------------------------------------------------
    NkQuatf startRot = NkQuatf::Identity();
    NkQuatf endRot = NkQuatf::RotateY(NkAngle::FromDeg(180.0f));

    // Lerp : interpolation linéaire simple (vitesse non constante)
    NkQuatf lerped = startRot.Lerp(endRot, 0.5f);  // 50% du chemin

    // NLerp : Lerp + normalisation (meilleur que Lerp seul)
    NkQuatf nlerped = startRot.NLerp(endRot, 0.5f);

    // SLerp : interpolation sphérique (vitesse angulaire constante)
    // Recommandé pour les animations de caméra ou d'objets
    NkQuatf slerped = startRot.SLerp(endRot, 0.5f);

    // Animation en boucle : interpolation progressive dans Update()
    void AnimateRotation(NkQuatf& current, const NkQuatf& target, float32 deltaTime)
    {
        constexpr float32 rotationSpeed = 2.0f;  // radians par seconde
        float32 t = NkClamp(rotationSpeed * deltaTime, 0.0f, 1.0f);
        current = current.SLerp(target, t);
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Conversions entre représentations
    // ---------------------------------------------------------------------
    NkQuatf quaternion = NkQuatf::RotateX(NkAngle::FromDeg(45.0f));

    // Quaternion → Matrice 4×4 (pour upload GPU ou transformation de points)
    NkMat4f rotationMatrix = static_cast<NkMat4f>(quaternion);

    // Matrice → Quaternion (extraction de rotation pure)
    NkQuatf extracted(rotationMatrix);

    // Quaternion → Euler (attention au gimbal lock !)
    NkEulerAngle euler = static_cast<NkEulerAngle>(quaternion);

    // Euler → Quaternion (reconstruction)
    NkQuatf rebuilt(euler);

    // Vérification de cohérence (avec tolérance flottante)
    bool isConsistent = (quaternion == rebuilt);  // Peut être false à cause des erreurs d'arrondi

    // ---------------------------------------------------------------------
    // Exemple 6 : LookAt pour orientation de caméra/objet
    // ---------------------------------------------------------------------
    // Faire regarder un objet vers une cible avec un up de référence
    NkVec3f eye(0.0f, 0.0f, 10.0f);
    NkVec3f target(0.0f, 0.0f, 0.0f);
    NkVec3f worldUp(0.0f, 1.0f, 0.0f);

    NkQuatf lookAtRot = NkQuatf::LookAt(eye, target, worldUp);

    // Application à la matrice de modèle d'un objet
    NkMat4f modelMatrix = NkMat4f::Translation(eye)
                        * static_cast<NkMat4f>(lookAtRot);

    // ---------------------------------------------------------------------
    // Exemple 7 : Requêtes géométriques sur quaternion
    // ---------------------------------------------------------------------
    NkQuatf q(0.0f, 0.0f, 0.383f, 0.924f);  // ~45° autour de Z

    // Angle de rotation extrait
    NkAngle rotationAngle = q.Angle();  // ~0.785 rad = 45°

    // Axe de rotation extrait
    NkVec3f rotationAxis = q.Axis();  // ~(0, 0, 1)

    // Test de normalisation (utile pour le debugging)
    bool isUnit = q.IsNormalized();  // true si ||q|| ≈ 1

    // Produit scalaire pour mesurer la similarité entre rotations
    NkQuatf q2 = NkQuatf::RotateZ(NkAngle::FromDeg(46.0f));
    float32 similarity = q.Dot(q2);  // ~0.999 pour des rotations proches

    // Détection de gimbal pole avant conversion Euler
    int pole = q.GimbalPole();  // 0 = normal, +1 = Nord, -1 = Sud
    if (pole != 0) {
        // Gestion spéciale de la singularité si nécessaire
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Opérations avancées
    // ---------------------------------------------------------------------
    // Conjugaison : inverse la rotation (utile pour transformations inverses)
    NkQuatf inverse = q.Conjugate();  // Pour quaternion unitaire : == Inverse()

    // Exponentiation : q^t pour interpolation angulaire personnalisée
    NkQuatf partial = q ^ 0.3f;  // 30% de la rotation de q

    // Réflexion par rapport à un plan
    NkVec3f mirrorNormal(1.0f, 0.0f, 0.0f);  // Plan YZ
    NkQuatf mirror = NkQuatf::Reflection(mirrorNormal);
    NkVec3f reflected = mirror * NkVec3f(1.0f, 2.0f, 3.0f);  // ~(-1, 2, 3)

    // Normalisation corrective après accumulation d'erreurs
    NkQuatf drifted = /* résultat de nombreuses opérations */;
    drifted.Normalize();  // Remet ||q|| = 1 pour éviter la dérive

    // ---------------------------------------------------------------------
    // Exemple 9 : Affichage et débogage
    // ---------------------------------------------------------------------
    NkQuatf debugQuat = NkQuatf::RotateY(NkAngle::FromDeg(123.456f));

    // Via méthode membre ToString()
    NkString quatStr = debugQuat.ToString();
    // Résultat : "[0.0000, 0.5592, 0.0000; 0.8290]"

    // Via flux de sortie standard
    std::cout << "Rotation : " << debugQuat << std::endl;

    // Via fonction globale NkToString avec formatage personnalisé
    NkString formatted = NkToString(
        debugQuat,
        NkFormatProps().SetPrecision(6)
    );  // Affiche avec 6 décimales au lieu du défaut

    // ---------------------------------------------------------------------
    // Exemple 10 : Intégration dans un système de caméra 3D
    // ---------------------------------------------------------------------
    class Camera {
    public:
        NkQuatf orientation;
        NkVec3f position;

        void Rotate(float32 pitchDelta, float32 yawDelta)
        {
            // Création des quaternions de rotation incrémentale
            NkQuatf pitchRot = NkQuatf::RotateX(NkAngle::FromRad(pitchDelta));
            NkQuatf yawRot = NkQuatf::RotateY(NkAngle::FromRad(yawDelta));

            // Application : yaw d'abord (monde), puis pitch (local)
            orientation = orientation * yawRot * pitchRot;

            // Correction périodique de la dérive numérique
            orientation.Normalize();
        }

        NkMat4f GetViewMatrix() const
        {
            // Matrice de vue = inverse(Translation * Rotation)
            // Pour quaternion unitaire : inverse(R) = conjugate(R)
            NkMat4f rotMatrix = static_cast<NkMat4f>(orientation.Conjugate());
            NkMat4f transMatrix = NkMat4f::Translation(-position);
            return rotMatrix * transMatrix;
        }
    };

    // ---------------------------------------------------------------------
    // Bonnes pratiques d'utilisation
    // ---------------------------------------------------------------------
    // ✓ Toujours normaliser après des opérations arithmétiques accumulées
    // ✓ Utiliser SLerp pour les animations (vitesse constante)
    // ✓ Préférer quaternion * vecteur plutôt que conversion matricielle
    // ✓ Vérifier GimbalPole() avant conversion Euler si précision critique
    // ✓ Utiliser Conjugate() pour l'inverse d'un quaternion unitaire (plus rapide)
    // ✗ Éviter les conversions Euler↔Quaternion en boucle (perte de précision)
    // ✗ Ne pas comparer des quaternions avec == sans tolérance (erreurs d'arrondi)
    // ✗ Attention à l'ordre de multiplication : q1 * q2 ≠ q2 * q1
*/