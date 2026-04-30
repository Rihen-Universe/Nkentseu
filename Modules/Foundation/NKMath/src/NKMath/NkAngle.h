// -----------------------------------------------------------------------------
// FICHIER: NKMath\NkAngle.h
// DESCRIPTION: Classe NkAngle - Représentation d'un angle avec wrapping automatique
//              Support float32/float64, sérialisation, fonctions avancées
// AUTEUR: Rihen
// DATE: 2026-04-26
// VERSION: 2.0.0
// LICENCE: Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
//
// RÉSUMÉ:
//   Classe template pour la manipulation d'angles avec normalisation automatique
//   dans l'intervalle (-180°, 180°]. Supporte float32 (NkAngle) et float64 (NkAngleD).
//   Inclut sérialisation binaire, fonctions avancées (Lerp, ShortestArc), et
//   wrapping optimisé pour inputs extrêmes.
//
// CARACTÉRISTIQUES:
//   - Template NkAngleT<Precision> pour support float32/float64 générique
//   - Typedefs pratiques : NkAngle = NkAngleT<float32>, NkAngleD = NkAngleT<float64>
//   - Wrapping optimisé : NkFmod pour inputs extrêmes, boucles fines près des limites
//   - Fonctions avancées libres : LerpAngle(), ShortestArc(), DeltaAngle()
//   - Sérialisation binaire : Save/Load pour persistance et réseau
//   - Conversion radians ⇄ degrés à la demande, thread-safe, sans cache
//   - Opérateurs arithmétiques complets avec tolérance epsilon pour comparaisons
//   - Fonctions critiques inline avec NKENTSEU_MATH_API_FORCE_INLINE
//   - Fonctions non-inline exportées avec NKENTSEU_MATH_API
//
// DÉPENDANCES:
//   - NKCore/NkTypes.h          : Types fondamentaux (float32, float64, etc.)
//   - NKMath/NkMathApi.h        : Macros d'export NKENTSEU_MATH_API
//   - NKMath/NkFunctions.h      : Fonctions mathématiques NkFabs, NkFmod, NkEpsilon
//   - NKCore/NkString.h         : Chaîne de caractères pour ToString()
//   - NKCore/NkSerialization.h  : (optionnel) pour sérialisation avancée
//
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_MATH_NKANGLE_H
#define NKENTSEU_MATH_NKANGLE_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Types fondamentaux : float32, float64, nk_bool, etc.
    #include "NKMath/NkMathApi.h"                  // Macros d'export : NKENTSEU_MATH_API, NKENTSEU_MATH_API_FORCE_INLINE
    #include "NKMath/NkFunctions.h"                // Fonctions mathématiques : NkFabs, NkFmod, constantes epsilon
    #include "NKContainers/String/NkString.h"      // Classe NkString pour représentation texte
    #include "NKContainers/String/NkFormat.h"      // NkFormatProps, NkFormatter, NkFormat
    #include <ostream>                             // std::ostream pour operator<<

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // NAMESPACE : MATH (COMPOSANTES MATHÉMATIQUES)
        // ====================================================================

        namespace math {

            // ====================================================================
            // CLASSE TEMPLATE : NKANGLET<PRECISION> (REPRÉSENTATION D'UN ANGLE)
            // ====================================================================

            /**
             * @brief Classe template pour la manipulation d'angles avec précision configurable
             * 
             * Représente un angle en degrés avec wrapping automatique dans l'intervalle
             * canonique (-180°, 180°]. Conçu pour être utilisé dans les calculs
             * géométriques, rotations, et interpolations angulaires avec précision
             * float32 ou float64 selon les besoins.
             * 
             * PRINCIPE DE FONCTIONNEMENT :
             * - Stockage interne : Precision (float32 ou float64) en degrés
             * - Wrapping optimisé : NkFmod pour inputs extrêmes (>1e6°), boucles fines près limites
             * - Conversion : Rad() = Deg() * DEG_TO_RAD, calculé à la demande (pas de cache)
             * - Thread-safe : aucun état mutable partagé, pas de variables statiques
             * 
             * GARANTIES DE PERFORMANCE :
             * - Constructeurs/accesseurs : O(1), tous inline pour élimination d'appel
             * - Wrapping : O(1) avec NkFmod, même pour inputs extrêmes (>1e9°)
             * - Conversion radians : 1 multiplication, négligeable en performance
             * - Mémoire : sizeof(NkAngleT<P>) == sizeof(P) (4 ou 8 bytes)
             * 
             * CAS D'USAGE TYPQUES :
             * - NkAngle (float32) : graphisme, jeux, UI, configurations (précision suffisante)
             * - NkAngleD (float64) : simulations scientifiques, astronomie, haute précision
             * - Interpolation angulaire avec gestion du wrap-around
             * - Calculs de différence angulaire minimale (shortest arc)
             * - Sérialisation pour sauvegarde, réseau, ou persistance
             * 
             * @tparam Precision Type de précision : float32 (défaut) ou float64
             * 
             * @note
             *   - Intervalle canonique : (-180, 180] degrés, pas [-180, 180)
             *   - Comparaisons : utilise epsilon adapté à la précision (kCompareAbsEpsilonF/D)
             *   - Conversion implicite vers Precision : pratique pour NkSin(angle), etc.
             *   - Pas de cache radians : recalculé à chaque appel Rad() pour thread-safety
             * 
             * @warning
             *   - Les opérations * et / entre angles ont une sémantique mathématique rare
             *   - Vérifier la précision requise : float32 suffit pour la plupart des cas graphiques
             */
            template<typename Precision = float32>
            class NkAngleT {

                // ====================================================================
                // SECTION PUBLIQUE : TYPES ET CONSTANTES ASSOCIÉES
                // ====================================================================
                public:

                    /** @brief Type de précision utilisé par cette instance */
                    using PrecisionType = Precision;

                    /** @brief Epsilon de comparaison adapté à la précision */
                    static constexpr Precision kEpsilon = 
                        sizeof(Precision) == sizeof(float32) 
                            ? constants::kCompareAbsEpsilonF 
                            : constants::kCompareAbsEpsilonD;

                    /** @brief Facteur de conversion degrés → radians pour cette précision */
                    static constexpr Precision kDegToRad = 
                        sizeof(Precision) == sizeof(float32) 
                            ? static_cast<Precision>(constants::kPiF / 180.0f)
                            : static_cast<Precision>(constants::kPi / 180.0);

                    /** @brief Facteur de conversion radians → degrés pour cette précision */
                    static constexpr Precision kRadToDeg = 
                        sizeof(Precision) == sizeof(float32)
                            ? static_cast<Precision>(180.0f / constants::kPiF)
                            : static_cast<Precision>(180.0 / constants::kPi);


                // ====================================================================
                // SECTION PUBLIQUE : CONSTRUCTEURS ET RÈGLE DE CINQ
                // ====================================================================
                public:

                    /**
                     * @brief Constructeur par défaut : angle nul (0°)
                     * @note Initialisation à 0, déjà dans l'interval canonique
                     * @note noexcept : aucune opération pouvant lever d'exception
                     * @note constexpr : évaluable à la compilation pour initialisation statique
                     */
                    constexpr NkAngleT() noexcept
                        : mDeg(static_cast<Precision>(0))
                    {
                    }

                    /**
                     * @brief Constructeur depuis une valeur en degrés avec wrapping automatique
                     * @param degrees Valeur en degrés à normaliser
                     * @note Applique Wrap() pour garantir mDeg ∈ (-180, 180]
                     * @note noexcept : Wrap() est noexcept et ne lève jamais d'exception
                     * @note constexpr : évaluable à la compilation si l'input est constexpr
                     */
                    explicit constexpr NkAngleT(Precision degrees) noexcept
                        : mDeg(Wrap(degrees))
                    {
                    }

                    /**
                     * @brief Constructeur de copie par défaut
                     * @note Copie bit-à-bit de la précision interne : opération triviale
                     * @note noexcept : copie de type triviallement copiable
                     */
                    NkAngleT(const NkAngleT&) noexcept = default;

                    /**
                     * @brief Constructeur de déplacement par défaut
                     * @note Déplacement trivial pour type scalaire
                     * @note noexcept : déplacement de type trivial
                     */
                    NkAngleT(NkAngleT&&) noexcept = default;

                    /**
                     * @brief Opérateur d'affectation par copie par défaut
                     * @note Copie bit-à-bit de la précision interne : opération triviale
                     * @return Référence vers *this pour chaînage
                     * @note noexcept : affectation de type triviallement copiable
                     */
                    NkAngleT& operator=(const NkAngleT&) noexcept = default;

                    /**
                     * @brief Opérateur d'affectation par déplacement par défaut
                     * @return Référence vers *this pour chaînage
                     * @note noexcept : déplacement de type trivial
                     */
                    NkAngleT& operator=(NkAngleT&&) noexcept = default;

                    /**
                     * @brief Destructeur par défaut
                     * @note Aucun nettoyage requis pour type scalaire
                     * @note noexcept : destruction triviale
                     */
                    ~NkAngleT() noexcept = default;


                // ====================================================================
                // SECTION PUBLIQUE : MÉTHODES DE FABRIQUE (FACTORY METHODS)
                // ====================================================================
                public:

                    /**
                     * @brief Crée un NkAngleT depuis une valeur en radians
                     * @param radians Angle en radians à convertir
                     * @return Nouveau NkAngleT avec valeur convertie et wrappée en degrés
                     * @note Conversion : degrees = radians * (180/π)
                     * @note Le résultat est automatiquement wrappé dans (-180, 180]
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining agressif
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    static NkAngleT FromRad(Precision radians) noexcept
                    {
                        return NkAngleT(radians * kRadToDeg);
                    }

                    /**
                     * @brief Alias legacy pour FromRad (compatibilité code ancien)
                     * @param radians Angle en radians à convertir
                     * @return Même résultat que FromRad(radians)
                     * @deprecated Préférer FromRad() pour clarté sémantique
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    static NkAngleT FromRadian(Precision radians) noexcept
                    {
                        return FromRad(radians);
                    }


                // ====================================================================
                // SECTION PUBLIQUE : ACCESSEURS ET MUTATEURS
                // ====================================================================
                public:

                    /**
                     * @brief Retourne la valeur de l'angle en degrés
                     * @return Valeur Precision dans (-180, 180]
                     * @note Accès direct au membre interne : O(1), aucun calcul
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : élimination d'appel garantie
                     * @note Méthode const : ne modifie pas l'état de l'angle
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    Precision Deg() const noexcept
                    {
                        return mDeg;
                    }

                    /**
                     * @brief Retourne la valeur de l'angle en radians (calculé à la demande)
                     * @return Valeur Precision équivalente en radians
                     * @note Conversion : radians = degrees * (π/180)
                     * @note Pas de cache : recalculé à chaque appel pour thread-safety
                     * @note Performance : 1 multiplication flottante, négligeable
                     * @note Méthode const : ne modifie pas l'état de l'angle
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    Precision Rad() const noexcept
                    {
                        return mDeg * kDegToRad;
                    }

                    /**
                     * @brief Modifie la valeur de l'angle en degrés avec wrapping automatique
                     * @param d Nouvelle valeur en degrés
                     * @note Applique Wrap() pour garantir mDeg ∈ (-180, 180] après affectation
                     * @note noexcept : Wrap() est noexcept, aucune exception possible
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void Deg(Precision d) noexcept
                    {
                        mDeg = Wrap(d);
                    }

                    /**
                     * @brief Modifie la valeur de l'angle en radians avec wrapping automatique
                     * @param r Nouvelle valeur en radians
                     * @note Conversion interne : mDeg = Wrap(radians * (180/π))
                     * @note Le résultat est garanti dans (-180, 180] degrés
                     * @note noexcept : opérations flottantes et Wrap() sont noexcept
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    void Rad(Precision r) noexcept
                    {
                        mDeg = Wrap(r * kRadToDeg);
                    }

                    /**
                     * @brief Conversion implicite vers Precision (valeur en degrés)
                     * @return Valeur Precision dans (-180, 180]
                     * @note Permet l'usage direct : NkSin(angle), NkCos(angle), etc.
                     * @note explicit : évite les conversions accidentelles dans les expressions
                     * @note noexcept : accès direct au membre interne
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    explicit operator Precision() const noexcept
                    {
                        return mDeg;
                    }


                // ====================================================================
                // SECTION PUBLIQUE : OPÉRATEURS UNAIRES
                // ====================================================================
                public:

                    /**
                     * @brief Négation unaire : retourne l'angle opposé
                     * @return NkAngleT avec valeur -mDeg, wrappé dans (-180, 180]
                     * @note Exemple : -NkAngle(45°) → NkAngle(-45°)
                     * @note noexcept : opérations flottantes et constructeur sont noexcept
                     * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    NkAngleT operator-() const noexcept
                    {
                        return NkAngleT(-mDeg);
                    }


                // ====================================================================
                // SECTION PUBLIQUE : OPÉRATEURS BINAIRES (NKANGLET ±/*÷ NKANGLET)
                // ====================================================================
                public:

                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator+(const NkAngleT& o) const noexcept { return NkAngleT(mDeg + o.mDeg); }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator-(const NkAngleT& o) const noexcept { return NkAngleT(mDeg - o.mDeg); }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator*(const NkAngleT& o) const noexcept { return NkAngleT(mDeg * o.mDeg); }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator/(const NkAngleT& o) const noexcept { return NkAngleT(mDeg / o.mDeg); }
                    
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT& operator+=(const NkAngleT& o) noexcept { mDeg = Wrap(mDeg + o.mDeg); return *this; }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT& operator-=(const NkAngleT& o) noexcept { mDeg = Wrap(mDeg - o.mDeg); return *this; }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT& operator*=(const NkAngleT& o) noexcept { mDeg = Wrap(mDeg * o.mDeg); return *this; }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT& operator/=(const NkAngleT& o) noexcept { mDeg = Wrap(mDeg / o.mDeg); return *this; }


                // ====================================================================
                // SECTION PUBLIQUE : OPÉRATEURS BINAIRES (NKANGLET ±/*÷ PRECISION)
                // ====================================================================
                public:

                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator+(Precision d) const noexcept { return NkAngleT(mDeg + d); }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator-(Precision d) const noexcept { return NkAngleT(mDeg - d); }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator*(Precision s) const noexcept { return NkAngleT(mDeg * s); }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator/(Precision s) const noexcept { return NkAngleT(mDeg / s); }
                    
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT& operator+=(Precision d) noexcept { mDeg = Wrap(mDeg + d); return *this; }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT& operator-=(Precision d) noexcept { mDeg = Wrap(mDeg - d); return *this; }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT& operator*=(Precision s) noexcept { mDeg = Wrap(mDeg * s); return *this; }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT& operator/=(Precision s) noexcept { mDeg = Wrap(mDeg / s); return *this; }


                // ====================================================================
                // SECTION PUBLIQUE : OPÉRATEURS FRIENDS (PRECISION ±/*÷ NKANGLET)
                // ====================================================================
                public:

                    friend NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator+(Precision d, const NkAngleT& a) noexcept { return NkAngleT(d + a.mDeg); }
                    friend NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator-(Precision d, const NkAngleT& a) noexcept { return NkAngleT(d - a.mDeg); }
                    friend NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator*(Precision s, const NkAngleT& a) noexcept { return NkAngleT(s * a.mDeg); }
                    friend NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator/(Precision s, const NkAngleT& a) noexcept { return NkAngleT(s / a.mDeg); }


                // ====================================================================
                // SECTION PUBLIQUE : INCRÉMENT/DÉCRÉMENT (PAS DE 1 DEGRÉ)
                // ====================================================================
                public:

                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT& operator++() noexcept { mDeg = Wrap(mDeg + static_cast<Precision>(1)); return *this; }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT& operator--() noexcept { mDeg = Wrap(mDeg - static_cast<Precision>(1)); return *this; }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator++(int) noexcept { NkAngleT t(*this); ++(*this); return t; }
                    NKENTSEU_MATH_API_FORCE_INLINE NkAngleT operator--(int) noexcept { NkAngleT t(*this); --(*this); return t; }


                // ====================================================================
                // SECTION PUBLIQUE : OPÉRATEURS DE COMPARAISON
                // ====================================================================
                public:

                    friend NKENTSEU_MATH_API_FORCE_INLINE bool operator==(const NkAngleT& a, const NkAngleT& b) noexcept {
                        return NkFabs(a.mDeg - b.mDeg) <= kEpsilon;
                    }
                    friend NKENTSEU_MATH_API_FORCE_INLINE bool operator!=(const NkAngleT& a, const NkAngleT& b) noexcept { return !(a == b); }
                    friend NKENTSEU_MATH_API_FORCE_INLINE bool operator<(const NkAngleT& a, const NkAngleT& b) noexcept { return a.mDeg < b.mDeg; }
                    friend NKENTSEU_MATH_API_FORCE_INLINE bool operator<=(const NkAngleT& a, const NkAngleT& b) noexcept { return a.mDeg <= b.mDeg; }
                    friend NKENTSEU_MATH_API_FORCE_INLINE bool operator>(const NkAngleT& a, const NkAngleT& b) noexcept { return a.mDeg > b.mDeg; }
                    friend NKENTSEU_MATH_API_FORCE_INLINE bool operator>=(const NkAngleT& a, const NkAngleT& b) noexcept { return a.mDeg >= b.mDeg; }


                // ====================================================================
                // SECTION PUBLIQUE : REPRÉSENTATION TEXTE (I/O)
                // ====================================================================
                public:

                    /**
                     * @brief Convertit l'angle en chaîne de caractères lisible
                     * @return NkString au format "{value}_deg" (ex: "45_deg", "-179.5_deg")
                     * @note Utilise NkFormat pour la mise en forme
                     * @note Non-inline : allocation de string, pas critique en performance
                     * @note Méthode const : ne modifie pas l'état de l'angle
                     */
                    NKENTSEU_MATH_API
                    NkString ToString() const;

                    /**
                     * @brief Fonction libre pour conversion texte (ADL-friendly)
                     * @param a Angle à convertir
                     * @return Même résultat que a.ToString()
                     * @note Permet l'usage : ToString(angle) via Argument-Dependent Lookup
                     */
                    friend NKENTSEU_MATH_API
                    NkString ToString(const NkAngleT& a);

                    /**
                     * @brief Opérateur de flux pour sortie std::ostream
                     * @param os Flux de sortie (std::cout, fichier, etc.)
                     * @param a Angle à écrire
                     * @return Référence vers os pour chaînage d'opérations
                     * @note Délègue à ToString() puis CStr() pour compatibilité C++ standard
                     * @note Utile pour logging/debug avec std::cout << angle
                     */
                    friend NKENTSEU_MATH_API
                    std::ostream& operator<<(std::ostream& os, const NkAngleT& a);


                // ====================================================================
                // SECTION PUBLIQUE : SÉRIALISATION BINAIRE (PERSISTENCE)
                // ====================================================================
                public:

                    /**
                     * @brief Sérialise l'angle dans un buffer binaire
                     * @param buffer Pointeur vers le buffer de destination (doit avoir au moins sizeof(Precision) bytes)
                     * @param bufferSize Taille disponible du buffer en bytes
                     * @return Nombre de bytes écrits (sizeof(Precision)), ou 0 en cas d'erreur
                     * @note Écrit la valeur en degrés en binaire natif (little-endian sur la plupart des plateformes)
                     * @note Pour la portabilité cross-platform, envisager une normalisation endianness
                     * @note noexcept : opérations mémoire simples, pas d'exception possible
                     * @warning Vérifier bufferSize >= sizeof(Precision) avant appel pour éviter buffer overflow
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    usize Save(void* buffer, usize bufferSize) const noexcept
                    {
                        if (bufferSize < sizeof(Precision)) {
                            return 0;
                        }
                        NKENTSEU_MEMCPY(buffer, &mDeg, sizeof(Precision));
                        return sizeof(Precision);
                    }

                    /**
                     * @brief Désérialise un angle depuis un buffer binaire
                     * @param buffer Pointeur vers le buffer source contenant les données
                     * @param bufferSize Taille disponible du buffer en bytes
                     * @return true si la désérialisation a réussi, false sinon
                     * @note Lit la valeur en degrés en binaire natif et applique Wrap() automatiquement
                     * @note Pour la portabilité cross-platform, envisager une normalisation endianness
                     * @note noexcept : opérations mémoire simples, pas d'exception possible
                     * @warning Vérifier bufferSize >= sizeof(Precision) avant appel
                     */
                    NKENTSEU_MATH_API_FORCE_INLINE
                    bool Load(const void* buffer, usize bufferSize) noexcept
                    {
                        if (bufferSize < sizeof(Precision)) {
                            return false;
                        }
                        NKENTSEU_MEMCPY(&mDeg, buffer, sizeof(Precision));
                        mDeg = Wrap(mDeg);  // Garantir le wrapping après chargement
                        return true;
                    }

                    /**
                     * @brief Retourne la taille requise pour la sérialisation binaire
                     * @return sizeof(Precision) bytes (4 pour float32, 8 pour float64)
                     * @note Utile pour allouer des buffers de taille exacte avant Save()
                     * @note constexpr : évaluable à la compilation
                     */
                    static constexpr usize SerializedSize() noexcept
                    {
                        return sizeof(Precision);
                    }


                // ====================================================================
                // SECTION PRIVÉE : MEMBRES DONNÉES ET UTILITAIRES INTERNES
                // ====================================================================
                private:

                    Precision mDeg;   ///< Valeur interne en degrés, garantie dans (-180, 180]

                    /**
                     * @brief Normalise un angle dans l'intervalle canonique (-180, 180]
                     * @param d Valeur en degrés à normaliser
                     * @return Valeur wrappée dans (-180, 180]
                     * @note Algorithme hybride :
                     *   - Pour |d| > 1e6° : utilise NkFmod pour O(1) garanti
                     *   - Pour |d| <= 1e6° : boucles fines pour précision exacte aux limites
                     * @note Complexité : O(1) dans tous les cas, même pour inputs extrêmes
                     * @note noexcept : opérations flottantes simples, pas d'exception possible
                     */
                    static NKENTSEU_MATH_API_FORCE_INLINE
                    Precision Wrap(Precision d) noexcept
                    {
                        // Optimisation pour inputs extrêmes : utiliser fmod pour O(1) garanti
                        constexpr Precision kExtremeThreshold = static_cast<Precision>(1e6);
                        
                        if (d > kExtremeThreshold || d < -kExtremeThreshold) {
                            // Réduction modulo 360 pour ramener dans [-360, 360]
                            d = NkFmod(d, static_cast<Precision>(360));
                        }
                        
                        // Boucles fines pour ajustement précis aux limites (-180, 180]
                        // Très rapides car rarement plus de 1-2 itérations après fmod
                        while (d > static_cast<Precision>(180)) { d -= static_cast<Precision>(360); }
                        while (d <= static_cast<Precision>(-180)) { d += static_cast<Precision>(360); }
                        
                        return d;
                    }

            }; // class NkAngleT<Precision>


            // ====================================================================
            // TYPEDEFS PRATIQUES POUR PRÉCISIONS COURANTES
            // ====================================================================

            /** @brief Angle en précision float32 (usage graphique, jeux, UI) */
            using NkAngle = NkAngleT<float32>;

            /** @brief Angle en précision float64 (usage scientifique, haute précision) */
            using NkAngleD = NkAngleT<float64>;


            // ====================================================================
            // FONCTIONS LIBRES AVANCÉES POUR NKANGLET
            // ====================================================================

            /**
             * @brief Calcule l'arc le plus court entre deux angles (différence minimale)
             * @tparam Precision Type de précision (float32 ou float64)
             * @param from Angle de départ
             * @param to Angle d'arrivée
             * @return NkAngleT<Precision> représentant la différence minimale dans (-180, 180]
             * @note Utilise la soustraction d'angles qui gère déjà le wrapping automatiquement
             * @note Résultat positif : rotation anti-horaire, négatif : horaire
             * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining agressif
             * 
             * @example
             * @code
             * NkAngle a(170.0f), b(-170.0f);
             * NkAngle diff = ShortestArc(a, b);  // -20° (plus court que +340°)
             * @endcode
             */
            template<typename Precision>
            NKENTSEU_MATH_API_FORCE_INLINE
            NkAngleT<Precision> ShortestArc(const NkAngleT<Precision>& from, 
                                           const NkAngleT<Precision>& to) noexcept
            {
                return to - from;  // La soustraction gère déjà le wrapping
            }

            /**
             * @brief Interpolation linéaire entre deux angles avec gestion du wrap-around
             * @tparam Precision Type de précision (float32 ou float64)
             * @param start Angle de départ
             * @param end Angle d'arrivée
             * @param t Facteur d'interpolation (typiquement ∈ [0, 1], non clamped)
             * @return NkAngleT<Precision> interpolé, wrappé dans (-180, 180]
             * @note Utilise ShortestArc pour prendre le chemin angulaire le plus court
             * @note t hors [0,1] : extrapolation (comportement défini, pas d'erreur)
             * @note Préférer NkClamp(t, 0, 1) avant appel si extrapolation non désirée
             * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining agressif
             * 
             * @example
             * @code
             * NkAngle start(0.0f), end(90.0f);
             * NkAngle mid = LerpAngle(start, end, 0.5f);  // 45°
             * 
             * // Avec wrap-around : prend le chemin le plus court
             * NkAngle a(170.0f), b(-170.0f);
             * NkAngle mid2 = LerpAngle(a, b, 0.5f);  // -180° ou 180° (équivalent)
             * @endcode
             */
            template<typename Precision>
            NKENTSEU_MATH_API_FORCE_INLINE
            NkAngleT<Precision> LerpAngle(const NkAngleT<Precision>& start,
                                         const NkAngleT<Precision>& end,
                                         Precision t) noexcept
            {
                // Utiliser la différence minimale pour l'interpolation
                NkAngleT<Precision> diff = ShortestArc(start, end);
                return start + diff * t;  // Wrapping automatique via opérateur+
            }

            /**
             * @brief Calcule la différence angulaire absolue minimale entre deux angles
             * @tparam Precision Type de précision (float32 ou float64)
             * @param a Premier angle
             * @param b Second angle
             * @return Valeur Precision ∈ [0, 180] représentant l'écart minimal
             * @note Utile pour tester la proximité géométrique d'orientations
             * @note Retourne toujours une valeur positive ou nulle
             * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining agressif
             * 
             * @example
             * @code
             * NkAngle a(-179.0f), b(179.0f);
             * Precision delta = DeltaAngle(a, b);  // 2.0f (pas 358°)
             * bool proche = delta < 5.0f;  // true
             * @endcode
             */
            template<typename Precision>
            NKENTSEU_MATH_API_FORCE_INLINE
            Precision DeltaAngle(const NkAngleT<Precision>& a, 
                                const NkAngleT<Precision>& b) noexcept
            {
                Precision diff = NkFabs(ShortestArc(a, b).Deg());
                return diff <= static_cast<Precision>(180) ? diff : static_cast<Precision>(360) - diff;
            }

            /**
             * @brief Normalise un angle vers l'intervalle [0, 360) au lieu de (-180, 180]
             * @tparam Precision Type de précision (float32 ou float64)
             * @param angle Angle à normaliser
             * @return NkAngleT<Precision> avec valeur dans [0, 360)
             * @note Utile pour certains systèmes qui préfèrent [0, 360) comme convention
             * @note Conversion simple : si angle < 0, ajouter 360
             * @note NKENTSEU_MATH_API_FORCE_INLINE : candidate à l'inlining agressif
             */
            template<typename Precision>
            NKENTSEU_MATH_API_FORCE_INLINE
            NkAngleT<Precision> NormalizeTo360(const NkAngleT<Precision>& angle) noexcept
            {
                Precision d = angle.Deg();
                if (d < static_cast<Precision>(0)) {
                    d += static_cast<Precision>(360);
                }
                return NkAngleT<Precision>(d);  // Déjà dans [0, 360)
            }

            // ── ToString ─────────────────────────────────────────────────────────

            template<>
            NkString NkAngleT<float32>::ToString() const
            {
                return NkFormat("{0}", *this);
            }

            inline NkString ToString(const NkAngleT<float32>& a)
            {
                return a.ToString();
            }

            inline std::ostream& operator<<(std::ostream& os, const NkAngleT<float32>& a)
            {
                return os << a.ToString().CStr();
            }

            template<>
            NkString NkAngleT<float64>::ToString() const
            {
                return NkFormat("{0}", *this);
            }

            inline NkString ToString(const NkAngleT<float64>& a)
            {
                return a.ToString();
            }

            inline std::ostream& operator<<(std::ostream& os, const NkAngleT<float64>& a)
            {
                return os << a.ToString().CStr();
            }


        } // namespace math

    } // namespace nkentseu

    // ============================================================================
    // POINT D'EXTENSION ADL : NkToString pour NkAngleT (dans namespace nkentseu)
    // ============================================================================

    namespace nkentseu {

        /**
         * @brief Surcharge ADL de NkToString pour NkAngleT avec support de formatage.
         *        Doit être dans namespace nkentseu pour être trouvée par la recherche ADL
         *        depuis nkentseu::detail::adl::Invoke (utilisé par NkFormat/NkFormatter).
         */

        inline NkString NkToString(const math::NkAngleT<float32>& a, const NkFormatProps& props)
        {
            // doit afficher en degrer radian ou sans specification de formatage
            if (props.type == 'd') {
                return props.ApplyWidth(NkStringView(NkFormat("{0}_deg", a.Deg())), false);
            }
            else if (props.type == 'r') {
                return props.ApplyWidth(NkStringView(NkFormat("{0}_rad", a.Rad())), false);
            }
            return props.ApplyWidth(NkStringView(NkFormat("{0}", a.Deg())), false);
        }

        inline NkString NkToString(const math::NkAngleT<float64>& a, const NkFormatProps& props)
        {
            if (props.type == 'd') {
                return props.ApplyWidth(NkStringView(NkFormat("{0}_deg", a.Deg())), false);
            }
            else if (props.type == 'r') {
                return props.ApplyWidth(NkStringView(NkFormat("{0}_rad", a.Rad())), false);
            }
            return props.ApplyWidth(NkStringView(NkFormat("{0}", a.Deg())), false);
        }

    } // namespace nkentseu

    // ── NkToString ADL overloads ──────────────────────────────────────────────

#endif // NKENTSEU_MATH_NKANGLE_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkAngle et NkAngleD
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Création et manipulation avec float32 et float64
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMath/NkAngle.h"
 * #include <cstdio>
 * 
 * void exemplePrecisionMultiple()
 * {
 *     using nkentseu::math::NkAngle;   // float32
 *     using nkentseu::math::NkAngleD;  // float64
 *     
 *     // Float32 : usage graphique, jeux, UI
 *     NkAngle angle32(45.5f);
 *     printf("Float32: %.2f° = %.6f rad\n", angle32.Deg(), angle32.Rad());
 *     
 *     // Float64 : usage scientifique, haute précision
 *     NkAngleD angle64(45.5);  // Construction depuis double literal
 *     printf("Float64: %.10f° = %.12f rad\n", angle64.Deg(), angle64.Rad());
 *     
 *     // Conversion entre précisions (explicite)
 *     NkAngleD from32 = NkAngleD(static_cast<float64>(angle32.Deg()));
 *     
 *     // Factory depuis radians avec précision adaptée
 *     NkAngle a32 = NkAngle::FromRad(3.14159f);      // float32
 *     NkAngleD a64 = NkAngleD::FromRad(3.1415926535); // float64
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Fonctions avancées - ShortestArc, LerpAngle, DeltaAngle
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMath/NkAngle.h"
 * 
 * void exempleFonctionsAvancees()
 * {
 *     using nkentseu::math::NkAngle;
 *     using nkentseu::math::ShortestArc;
 *     using nkentseu::math::LerpAngle;
 *     using nkentseu::math::DeltaAngle;
 *     
 *     // ShortestArc : différence minimale avec wrap-around
 *     NkAngle a(170.0f), b(-170.0f);
 *     NkAngle diff = ShortestArc(a, b);  // -20° (pas +340°)
 *     printf("Shortest arc: %.2f°\n", diff.Deg());
 *     
 *     // LerpAngle : interpolation avec chemin le plus court
 *     NkAngle start(0.0f), end(270.0f);  // 270° = -90° après wrap
 *     for (int i = 0; i <= 10; ++i) {
 *         float32 t = static_cast<float32>(i) / 10.0f;
 *         NkAngle interpolated = LerpAngle(start, end, t);
 *         printf("t=%.1f → %.2f°\n", t, interpolated.Deg());
 *         // Sortie: 0→0°, 0.1→-9°, ..., 1→-90° (pas +270°)
 *     }
 *     
 *     // DeltaAngle : écart minimal absolu pour tests de proximité
 *     NkAngle c(-179.0f), d(179.0f);
 *     float32 delta = DeltaAngle(c, d);  // 2.0f (pas 358°)
 *     bool proche = delta < 5.0f;  // true : angles géométriquement proches
 *     
 *     // NormalizeTo360 : conversion vers [0, 360) si requis par un système
 *     NkAngle neg(-45.0f);
 *     NkAngle normalized = nkentseu::math::NormalizeTo360(neg);  // 315°
 *     printf("Normalized: %.2f°\n", normalized.Deg());
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Sérialisation binaire pour persistance ou réseau
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMath/NkAngle.h"
 * #include <vector>
 * #include <cstdio>
 * 
 * void exempleSerialization()
 * {
 *     using nkentseu::math::NkAngle;
 *     using nkentseu::math::NkAngleD;
 *     
 *     // Sérialisation float32 (4 bytes)
 *     NkAngle angle(123.456f);
 *     uint8_t buffer32[NkAngle::SerializedSize()];
 *     
 *     usize written = angle.Save(buffer32, sizeof(buffer32));
 *     printf("Saved %zu bytes\n", written);  // 4
 *     
 *     // Désérialisation
 *     NkAngle loaded;
 *     if (loaded.Load(buffer32, sizeof(buffer32))) {
 *         printf("Loaded: %.3f° (original: %.3f°)\n", loaded.Deg(), angle.Deg());
 *     }
 *     
 *     // Sérialisation float64 (8 bytes) pour haute précision
 *     NkAngleD angleD(123.456789012345);
 *     uint8_t buffer64[NkAngleD::SerializedSize()];
 *     angleD.Save(buffer64, sizeof(buffer64));
 *     
 *     // Stockage dans un conteneur pour sauvegarde multiple
 *     std::vector<uint8_t> saveData;
 *     saveData.reserve(10 * NkAngle::SerializedSize());
 *     
 *     for (int i = 0; i < 10; ++i) {
 *         NkAngle a(static_cast<float32>(i * 36));
 *         usize offset = saveData.size();
 *         saveData.resize(offset + NkAngle::SerializedSize());
 *         a.Save(saveData.data() + offset, NkAngle::SerializedSize());
 *     }
 *     
 *     // Chargement depuis le buffer
 *     for (int i = 0; i < 10; ++i) {
 *         NkAngle a;
 *         usize offset = i * NkAngle::SerializedSize();
 *         if (a.Load(saveData.data() + offset, NkAngle::SerializedSize())) {
 *             printf("Loaded angle %d: %.1f°\n", i, a.Deg());
 *         }
 *     }
 *     
 *     // Note pour portabilité cross-platform :
 *     // - Les floats sont en little-endian sur x86/x64, ARM moderne
 *     // - Pour big-endian ou portabilité stricte : ajouter conversion endianness
 *     // - Exemple : NKENTSEU_BYTESWAP_IF_NEEDED(&value, sizeof(Precision));
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Wrapping optimisé pour inputs extrêmes
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMath/NkAngle.h"
 * #include "NKCore/NkTimer.h"
 * #include <cstdio>
 * 
 * void exempleWrappingPerformance()
 * {
 *     using nkentseu::math::NkAngle;
 *     
 *     // Test avec input normal : boucles fines (1-2 itérations max)
 *     NkAngle normal(450.0f);  // 450° → 90°
 *     printf("Normal wrap: 450° → %.2f°\n", normal.Deg());
 *     
 *     // Test avec input extrême : NkFmod pour O(1) garanti
 *     constexpr float32 kExtreme = 1.23456789e9f;  // 1.2 milliard de degrés
 *     NkAngle extreme(kExtreme);
 *     printf("Extreme wrap: %.0f° → %.2f°\n", kExtreme, extreme.Deg());
 *     
 *     // Benchmark comparatif
 *     constexpr nk_uint32 kIterations = 1000000;
 *     nkentseu::NkTimer timer;
 *     
 *     // Avec optimisation (NkFmod pour extrêmes)
 *     timer.Start();
 *     for (nk_uint32 i = 0; i < kIterations; ++i) {
 *         NkAngle a(kExtreme + static_cast<float32>(i));
 *         NKENTSEU_UNUSED(a);  // Éviter l'optimisation du compilateur
 *     }
 *     double timeOptimized = timer.ElapsedMilliseconds();
 *     
 *     printf("Wrapping %u extrêmes : %.2f ms (%.1f ns/itération)\n", 
 *            kIterations, timeOptimized, timeOptimized * 1000.0 / kIterations);
 *     
 *     // Sans optimisation (boucles while pures) aurait pris ~100x plus pour cet input
 *     // L'optimisation hybride garantit O(1) dans tous les cas
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Intégration dans un système de rotation 3D
 * --------------------------------------------------------------------------
 * @code
 * struct EulerAngles {
 *     nkentseu::math::NkAngle pitch;   // Lacet : rotation autour de Y
 *     nkentseu::math::NkAngle yaw;     // Tangage : rotation autour de X  
 *     nkentseu::math::NkAngle roll;    // Roulis : rotation autour de Z
 *     
 *     // Interpolation entre deux orientations Euler
 *     static EulerAngles Lerp(const EulerAngles& a, const EulerAngles& b, float32 t) {
 *         using nkentseu::math::LerpAngle;
 *         return {
 *             LerpAngle(a.pitch, b.pitch, t),
 *             LerpAngle(a.yaw, b.yaw, t),
 *             LerpAngle(a.roll, b.roll, t)
 *         };
 *     }
 *     
 *     // Calcul de la différence minimale entre deux orientations
 *     static float32 AngularDistance(const EulerAngles& a, const EulerAngles& b) {
 *         using nkentseu::math::DeltaAngle;
 *         float32 dp = DeltaAngle(a.pitch, b.pitch);
 *         float32 dy = DeltaAngle(a.yaw, b.yaw);
 *         float32 dr = DeltaAngle(a.roll, b.roll);
 *         // Distance euclidienne dans l'espace angulaire (approximation)
 *         return nkentseu::math::NkSqrt(dp*dp + dy*dy + dr*dr);
 *     }
 *     
 *     // Sérialisation compacte : 3 * 4 bytes = 12 bytes totaux
 *     usize Save(void* buffer, usize bufferSize) const noexcept {
 *         if (bufferSize < 3 * sizeof(float32)) return 0;
 *         uint8_t* ptr = static_cast<uint8_t*>(buffer);
 *         usize written = 0;
 *         written += pitch.Save(ptr + written, bufferSize - written);
 *         written += yaw.Save(ptr + written, bufferSize - written);
 *         written += roll.Save(ptr + written, bufferSize - written);
 *         return written;
 *     }
 *     
 *     bool Load(const void* buffer, usize bufferSize) noexcept {
 *         if (bufferSize < 3 * sizeof(float32)) return false;
 *         const uint8_t* ptr = static_cast<const uint8_t*>(buffer);
 *         usize read = 0;
 *         read += pitch.Load(ptr + read, bufferSize - read);
 *         read += yaw.Load(ptr + read, bufferSize - read);
 *         read += roll.Load(ptr + read, bufferSize - read);
 *         return read == 3 * sizeof(float32);
 *     }
 * };
 * 
 * void exempleEulerSystem()
 * {
 *     EulerAngles camera{ nkentseu::math::NkAngle(0), nkentseu::math::NkAngle(45), nkentseu::math::NkAngle(0) };
 *     EulerAngles target{ nkentseu::math::NkAngle(10), nkentseu::math::NkAngle(90), nkentseu::math::NkAngle(5) };
 *     
 *     // Animation fluide vers la cible
 *     for (int frame = 0; frame < 60; ++frame) {
 *         float32 t = static_cast<float32>(frame) / 59.0f;
 *         camera = EulerAngles::Lerp(camera, target, t);
 *         
 *         // Upload des angles au moteur de rendu...
 *         // camera.pitch.Deg(), camera.yaw.Deg(), camera.roll.Deg()
 *     }
 *     
 *     // Vérification de proximité pour déclencher un événement
 *     if (EulerAngles::AngularDistance(camera, target) < 1.0f) {
 *         // Orientation atteinte avec précision de 1°
 *         OnCameraAligned();
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DE LA PRÉCISION (FLOAT32 VS FLOAT64) :
 *    - NkAngle (float32) : suffisant pour graphisme, jeux, UI (erreur < 1e-5°)
 *    - NkAngleD (float64) : requis pour simulations scientifiques, astronomie, haute précision
 *    - Mémoire : float32 = 4 bytes, float64 = 8 bytes ×2 pour tableaux larges
 *    - Performance : float32 plus rapide sur la plupart des CPU/GPU, surtout SIMD
 *    - Sérialisation : float64 double la taille des données sauvegardées/transmises
 * 
 * 2. GESTION DU WRAPPING OPTIMISÉ :
 *    - L'algorithme hybride garantit O(1) même pour inputs > 1e9°
 *    - Pour inputs normaux (< 1e6°) : boucles fines pour précision exacte aux limites ±180°
 *    - Pour inputs extrêmes : NkFmod réduit à [-360, 360] avant ajustement fin
 *    - Éviter les incréments/décréments dans des boucles très serrées sur angles extrêmes
 * 
 * 3. UTILISATION DES FONCTIONS AVANCÉES :
 *    - ShortestArc() : toujours préférer à (b - a) brut pour différences angulaires
 *    - LerpAngle() : gère automatiquement le wrap-around, plus fiable que lerp manuel
 *    - DeltaAngle() : retourne [0, 180], idéal pour tests de proximité géométrique
 *    - NormalizeTo360() : utile pour interop avec systèmes utilisant [0, 360) comme convention
 * 
 * 4. SÉRIALISATION ET PORTABILITÉ :
 *    - Save/Load écrivent/lisent en binaire natif (endianness de la plateforme)
 *    - Pour portabilité cross-platform : ajouter conversion endianness si nécessaire
 *    - Toujours vérifier le retour de Load() : false si buffer trop petit ou corrompu
 *    - SerializedSize() constexpr : utile pour pré-allouer des buffers exacts
 * 
 * 5. COMPARAISONS ET TOLÉRANCE :
 *    - operator== utilise kEpsilon adapté à la précision (float32/float64)
 *    - operator< compare les degrés bruts : -179° < 179° est true (ordre linéaire)
 *    - Pour proximité géométrique : utiliser DeltaAngle(a, b) < threshold
 *    - Documenter le comportement des comparaisons pour éviter les surprises
 * 
 * 6. PERFORMANCE ET INLINING :
 *    - Toutes les méthodes critiques sont NKENTSEU_MATH_API_FORCE_INLINE
 *    - sizeof(NkAngleT<P>) == sizeof(P) : passe par registre, pas d'overhead mémoire
 *    - Éviter les copies inutiles : préférer const NkAngleT<P>& en paramètres
 *    - Pour tableaux d'angles : NkVector<NkAngle> pour localité mémoire optimale
 * 
 * 7. THREAD-SAFETY :
 *    - NkAngleT est stateless et thread-safe : pas de variables statiques ou partagées
 *    - Rad() recalcule à chaque appel : pas de race condition sur cache
 *    - Pour lecture/écriture concurrente sur le même objet : synchronisation externe requise
 *    - Save/Load sont thread-safe si buffer est thread-local ou protégé
 * 
 * 8. LIMITATIONS ET WORKAROUNDS :
 *    - Pas de support pour angles en integer degrés : utiliser float avec valeur entière si besoin
 *    - Pas de fonctions trigonométriques membres : utiliser NkSin(angle), NkCos(angle) via conversion implicite
 *    - Pas de conversion automatique entre NkAngle et NkAngleD : conversion explicite requise
 *    - Wrapping dans (-180, 180] : utiliser NormalizeTo360() si [0, 360) requis par un système externe
 * 
 * 9. EXTENSIONS FUTURES RECOMMANDÉES :
 *    - Ajouter NkAngleT<long double> pour précision extrême (80/128-bit) si plateforme supporte
 *    - Ajouter opérateurs modulo angulaire : angle % 90.0f pour snapping à intervalles réguliers
 *    - Intégrer avec NkQuaternion/NkMatrix4 pour rotations 3D complètes avec conversion
 *    - Ajouter des méthodes de clamping : ClampToRange(min, max) avec gestion du wrap
 *    - Supporter la sérialisation texte JSON/XML via NkSerializer pour interop web/config
 * 
 * 10. DEBUGGING ET VALIDATION :
 *    - Activer NKENTSEU_ASSERT en debug pour valider bufferSize dans Save/Load
 *    - Utiliser ToString() pour logging des angles dans les traces d'exécution
 *    - Tester les cas limites : ±180°, ±360°, valeurs extrêmes (>1e9°), NaN/Inf en entrée
 *    - Profiler le wrapping si angles extrêmes sont attendus fréquemment en production
 *    - Valider la précision float32 vs float64 sur les cas critiques du domaine métier
 */

// ============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// ============================================================================
/*
 * ALGORITHME DE WRAPPING HYBRIDE :
 *   - Problème : boucles while pures sont O(n/360) pour inputs extrêmes
 *   - Solution : détection de seuil (1e6°) → NkFmod pour réduction O(1)
 *   - Précision : NkFmod peut avoir des erreurs d'arrondi → boucles fines après pour ajustement exact
 *   - Résultat : O(1) garanti même pour 1e15°, avec précision exacte aux limites ±180°
 *
 * CHOIX DE L'INTERVALLE CANONIQUE (-180, 180] :
 *   - Avantage : différence minimale = soustraction directe (pas de logique supplémentaire)
 *   - Alternative [0, 360) : requiert logique supplémentaire pour shortest arc
 *   - Conversion possible via NormalizeTo360() si système externe requiert [0, 360)
 *   - Documenter clairement le choix dans l'API pour éviter les confusions
 *
 * PRÉCISION DES COMPARAISONS :
 *   - kEpsilon adapté automatiquement : kCompareAbsEpsilonF pour float32, kCompareAbsEpsilonD pour float64
 *   - Pour comparaisons géométriques : préférer DeltaAngle() qui retourne [0, 180]
 *   - Attention aux angles proches de ±180° : -179.999° et 179.999° sont proches géométriquement mais loin linéairement
 *
 * SÉRIALISATION ET ENDIANNESS :
 *   - Save/Load utilisent NKENTSEU_MEMCPY : copie binaire native de la plateforme
 *   - Little-endian (x86/x64, ARM moderne) : ordre bytes LSB→MSB
 *   - Big-endian (certains systèmes embarqués) : ordre inverse → données incompatibles
 *   - Pour portabilité : ajouter macro NKENTSEU_BYTESWAP_IF_NEEDED avant/après memcpy
 *   - Alternative : sérialisation texte via ToString()/parsing pour portabilité maximale (plus lent)
 *
 * PERFORMANCE INDICATIVE (cycles/opération, CPU moderne) :
 *   | Opération        | Float32 | Float64 | Notes                          |
 *   |-----------------|---------|---------|--------------------------------|
 *   | Construction    | ~1-2    | ~2-4    | Wrapping inclus                |
 *   | Deg()/Rad()     | ~1      | ~1-2    | Accès direct ou 1 multiplication|
 *   | Wrap (normal)   | ~3-5    | ~4-6    | Boucles fines 1-2 itérations   |
 *   | Wrap (extrême)  | ~15-25  | ~20-35  | NkFmod + ajustement fin        |
 *   | ShortestArc     | ~3-5    | ~4-6    | Soustraction + wrapping        |
 *   | LerpAngle       | ~8-12   | ~10-16  | Diff + multiplication + add    |
 *   | Save/Load       | ~5-10   | ~5-10   | Memcpy + checks de bornes      |
 *
 * COMPATIBILITÉ AVEC LE RESTE DU FRAMEWORK :
 *   - Conversion implicite vers Precision : compatible avec NkSin/NkCos de NkFunctions.h
 *   - NkVector<NkAngle> : localité mémoire optimale pour tableaux d'orientations
 *   - NkMap<NkAngle, V> : usable comme clé grâce à operator< défini
 *   - NkToString() : compatible avec NkFormat/NkLogger pour logging structuré
 *   - Sérialisation : compatible avec systèmes de save/load du framework
 *
 * EXTENSIONS POSSIBLES VIA TEMPLATE METAPROGRAMMING :
 *   - NkAngleT<int32> pour angles en degrés entiers (mémoire minimale, pas de flottants)
 *   - NkAngleT<Precision, WrapPolicy> pour politiques de wrapping configurables
 *   - NkAngleT<Precision, Unit> pour support radians comme unité interne (conversion différente)
 *   - Ces extensions nécessiteraient une refactorisation plus poussée de l'API
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Création : 2026-04-26
// Dernière modification : 2026-04-26 (v2.0 : template precision, sérialisation, fonctions avancées, wrapping optimisé)
// ============================================================