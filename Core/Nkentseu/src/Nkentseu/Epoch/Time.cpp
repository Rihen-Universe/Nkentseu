/**
* @File Time.cpp
* @Description Implémentation complète de la classe Time avec gestion nanoseconde
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Apache-2.0
*/

#include "pch.h"
#include "Time.h"
#include <iostream>

namespace nkentseu {

    /// @Region: Constructeurs et opérateurs d'affectation ------------------------

    /**
    * @Constructor Composants temporels
    * @Description Initialise l'objet avec des valeurs spécifiques et normalise
    * @param hour Heure [0-23]
    * @param minute Minute [0-59]
    * @param second Seconde [0-59]
    * @param millisecond Milliseconde [0-999]
    * @param nanosecond Nanoseconde [0-999999]
    */
    Time::Time(int32 hour, int32 minute, int32 second, int32 millisecond, int32 nanosecond) 
        : m_Hour(hour), m_Minute(minute), m_Second(second), 
          m_Millisecond(millisecond), m_Nanosecond(nanosecond) {
        Normalize();
    }

    /**
    * @Constructor Total nanosecondes
    * @Description Construit l'objet à partir d'une durée totale en nanosecondes
    * @param totalNanoseconds Durée absolue en nanosecondes depuis minuit
    */
    Time::Time(int64 totalNanoseconds) noexcept {
        m_Nanosecond = totalNanoseconds % NANOSECONDS_PER_MILLISECOND;
        totalNanoseconds /= NANOSECONDS_PER_MILLISECOND;

        m_Millisecond = totalNanoseconds % MILLISECONDS_PER_SECOND;
        totalNanoseconds /= MILLISECONDS_PER_SECOND;

        m_Second = totalNanoseconds % SECONDS_PER_MINUTE;
        totalNanoseconds /= SECONDS_PER_MINUTE;

        m_Minute = totalNanoseconds % MINUTES_PER_HOUR;
        totalNanoseconds /= MINUTES_PER_HOUR;

        m_Hour = totalNanoseconds % HOURS_PER_DAY;
    }

    /**
    * @Constructor Copie
    * @Description Copie les membres d'une instance existante
    * @param other Instance à copier
    */
    Time::Time(const Time& other) noexcept :
        m_Hour(other.m_Hour),
        m_Minute(other.m_Minute),
        m_Second(other.m_Second),
        m_Millisecond(other.m_Millisecond),
        m_Nanosecond(other.m_Nanosecond) {}

    /**
    * @Operator Affectation
    * @Description Affectation membre par membre avec vérification d'auto-affectation
    * @param other Instance à copier
    * @return Référence à l'instance courante
    */
    Time& Time::operator=(const Time& other) noexcept {
        if (this != &other) {
            m_Hour = other.m_Hour;
            m_Minute = other.m_Minute;
            m_Second = other.m_Second;
            m_Millisecond = other.m_Millisecond;
            m_Nanosecond = other.m_Nanosecond;
        }
        return *this;
    }

    /// @Region: Mutateurs avec validation -----------------------------------------

    /**
    * @Function SetHour
    * @Description Modifie le composant heure avec validation
    * @param hour Nouvelle valeur [0-23]
    * @Throws std::invalid_argument Si valeur hors limites
    */
    void Time::SetHour(int32 hour) { 
        if(hour < 0 || hour >= HOURS_PER_DAY)
            throw std::invalid_argument("Hour must be 0-23");
        m_Hour = hour; 
    }

    /**
    * @Function SetMinute
    * @Description Modifie le composant minute avec validation
    * @param minute Nouvelle valeur [0-59]
    * @Throws std::invalid_argument Si valeur hors limites
    */
    void Time::SetMinute(int32 minute) {
        if(minute < 0 || minute >= MINUTES_PER_HOUR)
            throw std::invalid_argument("Minute must be 0-59");
        m_Minute = minute;
    }

    /**
    * @Function SetSecond
    * @Description Modifie le composant seconde avec validation
    * @param second Nouvelle valeur [0-59]
    * @Throws std::invalid_argument Si valeur hors limites
    */
    void Time::SetSecond(int32 second) {
        if(second < 0 || second >= SECONDS_PER_MINUTE)
            throw std::invalid_argument("Second must be 0-59");
        m_Second = second;
    }

    /**
    * @Function SetMillisecond
    * @Description Modifie le composant milliseconde avec validation
    * @param millis Nouvelle valeur [0-999]
    * @Throws std::invalid_argument Si valeur hors limites
    */
    void Time::SetMillisecond(int32 millis) {
        if(millis < 0 || millis >= MILLISECONDS_PER_SECOND)
            throw std::invalid_argument("Millisecond must be 0-999");
        m_Millisecond = millis;
    }

    /**
    * @Function SetNanosecond
    * @Description Modifie le composant nanoseconde avec validation
    * @param nano Nouvelle valeur [0-999999]
    * @Throws std::invalid_argument Si valeur hors limites
    */
    void Time::SetNanosecond(int32 nano) {
        if(nano < 0 || nano >= NANOSECONDS_PER_MILLISECOND)
            throw std::invalid_argument("Nanosecond must be 0-999999");
        m_Nanosecond = nano;
    }

    /// @Region: Opérateurs arithmétiques ------------------------------------------

    /**
    * @Operator +=
    * @Description Additionne un temps à l'instance courante
    * @param other Temps à ajouter
    * @return Référence à l'instance modifiée
    */
    Time& Time::operator+=(const Time& other) noexcept {
        *this = Time(static_cast<int64>(*this) + static_cast<int64>(other));
        return *this;
    }

    /**
    * @Operator -=
    * @Description Soustrait un temps de l'instance courante
    * @param other Temps à soustraire
    * @return Référence à l'instance modifiée
    */
    Time& Time::operator-=(const Time& other) noexcept {
        *this = Time(static_cast<int64>(*this) - static_cast<int64>(other));
        return *this;
    }

    /**
    * @Operator +
    * @Description Crée un nouveau temps résultant de l'addition
    * @param other Temps à ajouter
    * @return Nouvelle instance Time
    */
    Time Time::operator+(const Time& other) const noexcept {
        return Time(static_cast<int64>(*this) + static_cast<int64>(other));
    }

    /**
    * @Operator -
    * @Description Crée un nouveau temps résultant de la soustraction
    * @param other Temps à soustraire
    * @return Nouvelle instance Time
    */
    Time Time::operator-(const Time& other) const noexcept {
        return Time(static_cast<int64>(*this) - static_cast<int64>(other));
    }

    /// @Region: Opérateurs de comparaison ----------------------------------------

    /**
    * @Operator ==
    * @Description Compare l'égalité via la représentation en nanosecondes
    * @param other Temps à comparer
    * @return true si les temps sont identiques
    */
    bool Time::operator==(const Time& other) const noexcept {
        return static_cast<int64>(*this) == static_cast<int64>(other);
    }

    /**
    * @Operator !=
    * @Description Compare la différence via l'opérateur d'égalité
    * @param other Temps à comparer
    * @return true si les temps sont différents
    */
    bool Time::operator!=(const Time& other) const noexcept {
        return !(*this == other);
    }

    /**
    * @Operator <
    * @Description Compare l'ordre chronologique strict
    * @param other Temps de comparaison
    * @return true si ce temps est antérieur
    */
    bool Time::operator<(const Time& other) const noexcept {
        return static_cast<int64>(*this) < static_cast<int64>(other);
    }

    /// @Region: Conversions de type ----------------------------------------------

    /**
    * @Operator float
    * @Description Conversion en secondes avec précision réduite
    * @return Valeur en secondes (précision milliseconde)
    */
    Time::operator float() const noexcept {
        return static_cast<float>(static_cast<double>(*this));
    }

    /**
    * @Operator double
    * @Description Conversion en secondes avec haute précision
    * @return Valeur en secondes (précision nanoseconde)
    */
    Time::operator double() const noexcept {
        return static_cast<double>(static_cast<int64>(*this)) / 1e9;
    }

    /**
    * @Operator int64
    * @Description Conversion en durée totale en nanosecondes
    * @return Nombre total de nanosecondes depuis minuit
    */
    Time::operator int64() const noexcept {
        return static_cast<int64>(m_Nanosecond) 
            + NANOSECONDS_PER_MILLISECOND * (
                m_Millisecond 
                + MILLISECONDS_PER_SECOND * (
                    m_Second 
                    + SECONDS_PER_MINUTE * (
                        m_Minute 
                        + MINUTES_PER_HOUR * m_Hour
                    )
                )
            );
    }

    /// @Region: Méthodes utilitaires ---------------------------------------------

    /**
    * @Function GetCurrent
    * @Description Obtient l'heure système locale précise
    * @Note Gère les spécificités Windows et POSIX
    * @return Instance Time actuelle
    */
    Time Time::GetCurrent() {
        using namespace std::chrono;
    
        auto now = system_clock::now();
        time_t time_utc = system_clock::to_time_t(now);
        
        tm local_time;
        #if defined(_WIN32)
            localtime_s(&local_time, &time_utc);
        #else
            localtime_r(&time_utc, &local_time);
        #endif
    
        auto since_epoch = now.time_since_epoch();
        auto secs = duration_cast<seconds>(since_epoch);
        auto subsec = duration_cast<nanoseconds>(since_epoch - secs);
    
        return Time(
            local_time.tm_hour,
            local_time.tm_min,
            local_time.tm_sec,
            static_cast<int32>(subsec.count() / 1'000'000),
            static_cast<int32>(subsec.count() % 1'000'000)
        );
    }

    /**
    * @Function ToString
    * @Description Formatage selon ISO 8601 étendu avec padding zéro
    * @Exemple 14:05:30.123.456789
    * @return Chaîne formatée
    */
    std::string Time::ToString() const {
        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(2) << m_Hour   << ":"
            << std::setw(2) << m_Minute << ":"
            << std::setw(2) << m_Second << "."
            << std::setw(3) << std::to_string(m_Millisecond + 1000).substr(1) << "."
            << std::setw(3) << std::to_string((m_Nanosecond % 1000000) + 1000000).substr(1);
    
        return oss.str();
    }

    /// @Region: Méthodes internes ------------------------------------------------

    /**
    * @Function Normalize
    * @Description Ajuste les composants pour respecter les plages valides
    * @Note Garantit m_Nanosecond ∈ [0,999999], m_Millisecond ∈ [0,999], etc.
    */
    void Time::Normalize() noexcept {
        m_Nanosecond %= NANOSECONDS_PER_MILLISECOND;
        m_Millisecond %= MILLISECONDS_PER_SECOND;
        m_Second %= SECONDS_PER_MINUTE;
        m_Minute %= MINUTES_PER_HOUR;
        m_Hour %= HOURS_PER_DAY;
    }

    /**
    * @Function Validate
    * @Description Valide l'intégrité des composants temporels
    * @Throws std::invalid_argument Si un composant est invalide
    */
    void Time::Validate() const {
        if(m_Hour < 0 || m_Hour >= HOURS_PER_DAY)
            throw std::invalid_argument("Invalid hour");
        if(m_Minute < 0 || m_Minute >= MINUTES_PER_HOUR)
            throw std::invalid_argument("Invalid minute");
        if(m_Second < 0 || m_Second >= SECONDS_PER_MINUTE)
            throw std::invalid_argument("Invalid second");
        if(m_Millisecond < 0 || m_Millisecond >= MILLISECONDS_PER_SECOND)
            throw std::invalid_argument("Invalid millisecond");
        if(m_Nanosecond < 0 || m_Nanosecond >= NANOSECONDS_PER_MILLISECOND)
            throw std::invalid_argument("Invalid nanosecond");
    }

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.