/**
* @File NkTime.h
* @Description Définit la classe NkTime pour la gestion précise du temps quotidien avec nanosecondes
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Apache-2.0
*/

#pragma once // Directive de préprocesseur pour éviter les inclusions multiples

// Région des includes
#include "NkTimeExport.h"
#include "NkPlatformDetect.h"
#include "NkTypes.h"
#include <chrono>      // Pour les opérations temporelles
#include <ctime>       // Pour la manipulation du temps C
#include <sstream>     // Pour les flux de chaînes
#include <iomanip>     // Pour le formatage de sortie
#include <stdexcept>   // Pour les exceptions standard

namespace nkentseu { // Début du namespace nkentseu

    /**
    * - NkTime : Représente un temps quotidien avec précision nanoseconde
    *
    * @Description :
    * Cette classe fournit une représentation précise du temps avec gestion des composants
    * horaires jusqu'aux nanosecondes. Elle inclut des fonctionnalités complètes pour :
    * - Les opérations arithmétiques entre objets NkTime
    * - La conversion depuis/dans le système chrono de C++
    * - La validation rigoureuse des valeurs temporelles
    * - Le formatage de sortie selon la norme ISO 8601 étendue
    * - Une implémentation thread-safe
    *
    * @Members :
    * - (int32) mHour : Heure dans l'intervalle [0-23]
    * - (int32) mMinute : Minute dans l'intervalle [0-59]
    * - (int32) mSecond : Seconde dans l'intervalle [0-59]
    * - (int32) mMillisecond : Milliseconde dans l'intervalle [0-999]
    * - (int32) mNanosecond : Nanoseconde dans l'intervalle [0-999999]
    */
    class NKTIME_API NkTime {
    public:
        /// @Region: Constantes ----------------------------------------------------
        /**
        * @Constant NANOSECONDS_PER_MICROSECOND : Conversion nanosecondes -> microsecondes
        */
        static constexpr int32 NANOSECONDS_PER_MICROSECOND = 1000;

        /**
        * @Constant NANOSECONDS_PER_MILLISECOND : Conversion nanosecondes -> millisecondes
        */
        static constexpr int32 NANOSECONDS_PER_MILLISECOND = 1000000;

        /**
        * @Constant MICROSECONDS_PER_MILLISECOND : Conversion microsecondes -> millisecondes
        */
        static constexpr int32 MICROSECONDS_PER_MILLISECOND = 1000;

        /**
        * @Constant MILLISECONDS_PER_SECOND : Conversion millisecondes -> secondes
        */
        static constexpr int32 MILLISECONDS_PER_SECOND = 1000;

        /**
        * @Constant SECONDS_PER_MINUTE : Conversion secondes -> minutes
        */
        static constexpr int32 SECONDS_PER_MINUTE = 60;

        /**
        * @Constant MINUTES_PER_HOUR : Conversion minutes -> heures
        */
        static constexpr int32 MINUTES_PER_HOUR = 60;

        /**
        * @Constant HOURS_PER_DAY : Conversion heures -> jours
        */
        static constexpr int32 HOURS_PER_DAY = 24;

        /**
        * @Constant SECONDS_PER_HOUR : Nombre de secondes dans une heure
        */
        static constexpr int32 SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;

        /**
        * @Constant SECONDS_PER_DAY : Nombre de secondes dans un jour
        */
        static constexpr int32 SECONDS_PER_DAY  = SECONDS_PER_HOUR * HOURS_PER_DAY;

        /**
        * @Constant NANOSECONDS_PER_SECOND : Conversion secondes -> nanosecondes
        */
        static constexpr int64 NANOSECONDS_PER_SECOND = static_cast<int64>(NANOSECONDS_PER_MILLISECOND) * MILLISECONDS_PER_SECOND;

        /// @Region: Constructeurs -------------------------------------------------
        /**
        * @Constructor NkTime par défaut
        * @Description : Initialise tous les composants temporels à zéro
        */
        NkTime() noexcept;

        /**
        * @Constructor NkTime à partir de composants
        * @Description : Construit un objet NkTime à partir de ses composants individuels
        * @param (int32) hour : Heure [0-23]
        * @param (int32) minute : Minute [0-59]
        * @param (int32) second : Seconde [0-59]
        * @param (int32) millisecond : Milliseconde [0-999] (défaut : 0)
        * @param (int32) nanosecond : Nanoseconde [0-999999] (défaut : 0)
        * @Throws std::invalid_argument : Si un paramètre est hors limites
        */
        NkTime(int32 hour, int32 minute, int32 second, int32 millisecond = 0, int32 nanosecond = 0);

        /**
        * @Constructor NkTime à partir de nanosecondes
        * @Description : Construit un NkTime à partir d'un total de nanosecondes
        * @param (int64) totalNanoseconds : Nombre total de nanosecondes écoulées
        */
        explicit NkTime(int64 totalNanoseconds) noexcept;

        /**
        * @Constructor Copie
        * @Description : Constructeur de copie par défaut
        */
        NkTime(const NkTime&) noexcept;

        /// @Region: Opérations -----------------------------------------------------
        /**
        * @Operator Affectation
        * @Description : Opérateur d'affectation par défaut
        * @return (NkTime&) : Référence à l'objet modifié
        */
        NkTime& operator=(const NkTime&) noexcept;

        /// @Region: Accesseurs -----------------------------------------------------
        /**
        * @Function GetHour
        * @Description : Accesseur pour le composant heure
        * @return (int32) : Valeur actuelle de l'heure [0-23]
        */
        int32 GetHour() const noexcept { return mHour; }

        /**
        * @Function GetMinute
        * @Description : Accesseur pour le composant minute
        * @return (int32) : Valeur actuelle des minutes [0-59]
        */
        int32 GetMinute() const noexcept { return mMinute; }

        /**
        * @Function GetSecond
        * @Description : Accesseur pour le composant seconde
        * @return (int32) : Valeur actuelle des secondes [0-59]
        */
        int32 GetSecond() const noexcept { return mSecond; }

        /**
        * @Function GetMillisecond
        * @Description : Accesseur pour le composant milliseconde
        * @return (int32) : Valeur actuelle des millisecondes [0-999]
        */
        int32 GetMillisecond() const noexcept { return mMillisecond; }

        /**
        * @Function GetNanosecond
        * @Description : Accesseur pour le composant nanoseconde
        * @return (int32) : Valeur actuelle des nanosecondes [0-999999]
        */
        int32 GetNanosecond() const noexcept { return mNanosecond; }

        /// @Region: Mutateurs ------------------------------------------------------
        /**
        * @Function SetHour
        * @Description : Modifie le composant heure
        * @param (int32) hour : Nouvelle valeur de l'heure [0-23]
        * @Throws std::invalid_argument : Si la valeur est hors limites
        */
        void SetHour(int32 hour);

        /**
        * @Function SetMinute
        * @Description : Modifie le composant minute
        * @param (int32) minute : Nouvelle valeur des minutes [0-59]
        * @Throws std::invalid_argument : Si la valeur est hors limites
        */
        void SetMinute(int32 minute);

        /**
        * @Function SetSecond
        * @Description : Modifie le composant seconde
        * @param (int32) second : Nouvelle valeur des secondes [0-59]
        * @Throws std::invalid_argument : Si la valeur est hors limites
        */
        void SetSecond(int32 second);

        /**
        * @Function SetMillisecond
        * @Description : Modifie le composant milliseconde
        * @param (int32) millis : Nouvelle valeur des millisecondes [0-999]
        * @Throws std::invalid_argument : Si la valeur est hors limites
        */
        void SetMillisecond(int32 millis);

        /**
        * @Function SetNanosecond
        * @Description : Modifie le composant nanoseconde
        * @param (int32) nano : Nouvelle valeur des nanosecondes [0-999999]
        * @Throws std::invalid_argument : Si la valeur est hors limites
        */
        void SetNanosecond(int32 nano);

        /// @Region: Opérations -----------------------------------------------------
        /**
        * @Operator +=
        * @Description : Ajoute un temps à l'instance courante
        * @param (const NkTime&) other : Temps à ajouter
        * @return (NkTime&) : Référence à l'instance modifiée
        */
        NkTime& operator+=(const NkTime& other) noexcept;

        /**
        * @Operator -=
        * @Description : Soustrait un temps de l'instance courante
        * @param (const NkTime&) other : Temps à soustraire
        * @return (NkTime&) : Référence à l'instance modifiée
        */
        NkTime& operator-=(const NkTime& other) noexcept;

        /**
        * @Operator +
        * @Description : Addition de deux temps
        * @param (const NkTime&) other : Temps à ajouter
        * @return (NkTime) : Nouvel objet NkTime résultant de l'addition
        */
        NkTime operator+(const NkTime& other) const noexcept;

        /**
        * @Operator -
        * @Description : Soustraction de deux temps
        * @param (const NkTime&) other : Temps à soustraire
        * @return (NkTime) : Nouvel objet NkTime résultant de la soustraction
        */
        NkTime operator-(const NkTime& other) const noexcept;

        /// @Region: Comparaisons ---------------------------------------------------
        /**
        * @Operator ==
        * @Description : Comparaison d'égalité
        * @param (const NkTime&) other : Temps à comparer
        * @return (bool) : true si les temps sont identiques
        */
        bool operator==(const NkTime& other) const noexcept;

        /**
        * @Operator !=
        * @Description : Comparaison de différence
        * @param (const NkTime&) other : Temps à comparer
        * @return (bool) : true si les temps sont différents
        */
        bool operator!=(const NkTime& other) const noexcept;

        /**
        * @Operator <
        * @Description : Comparaison inférieur à
        * @param (const NkTime&) other : Temps à comparer
        * @return (bool) : true si ce temps est antérieur à other
        */
        bool operator<(const NkTime& other) const noexcept;

        /// @Region: Conversions ----------------------------------------------------
        /**
        * @Operator float
        * @Description : Conversion en secondes avec précision simple
        * @return (float) : Temps total en secondes (précision milliseconde)
        */
        explicit operator float() const noexcept;

        /**
        * @Operator double
        * @Description : Conversion en secondes avec haute précision
        * @return (double) : Temps total en secondes (précision nanoseconde)
        */
        explicit operator double() const noexcept;

        /**
        * @Operator int64
        * @Description : Conversion en nanosecondes
        * @return (int64) : Temps total en nanosecondes
        */
        explicit operator int64() const noexcept;

        /// @Region: Utilitaires ----------------------------------------------------
        /**
        * @Function GetCurrent
        * @Description : Obtient l'heure système actuelle avec précision nanoseconde
        * @return (NkTime) : Instance NkTime représentant l'heure courante
        */
        static NkTime GetCurrent();

        /**
        * @Function ToString
        * @Description : Formatage selon ISO 8601 étendu (HH:MM:SS.sss.nnn)
        * @return (std::string) : Représentation formatée du temps
        */
        std::string ToString() const;

        /**
        * @Friend ToString
        * @Description : Surcharge amie de la fonction ToString
        * @param (const NkTime&) time : Temps à formater
        * @return (std::string) : Représentation formatée du temps
        */
        friend std::string ToString(const NkTime& time) {
            return time.ToString();
        }

    private:
        // Membres privés avec commentaires inline
        int32 mHour = 0;        // Heure [0-23]
        int32 mMinute = 0;      // Minute [0-59]
        int32 mSecond = 0;      // Seconde [0-59]
        int32 mMillisecond = 0; // Milliseconde [0-999]
        int32 mNanosecond = 0;  // Nanoseconde [0-999999]

        /// @Region: Normalisation --------------------------------------------------
        /**
        * @Function Normalize
        * @Description : Normalise les composants temporels dans leurs plages valides
        */
        void Normalize() noexcept;

        /**
        * @Function Validate
        * @Description : Valide l'intégrité des composants temporels
        * @Throws std::invalid_argument : Si un composant est invalide
        */
        void Validate() const;
    };

} // namespace nkentseu (fin du namespace)


// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.
