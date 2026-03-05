/**
* @File Date.cpp
* @Description Implémentation complète de la classe Date avec gestion des dates grégoriennes
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Apache-2.0
*/

#include "pch.h"
#include "Date.h"

namespace nkentseu {

    /// @Region: Constructeurs et initialisation ----------------------------------

    /**
    * @Constructor Date par défaut
    * @Description Initialise avec la date système actuelle
    * @Note Utilise GetCurrent() pour l'initialisation thread-safe
    */
    Date::Date() noexcept {
        *this = GetCurrent();
    }

    /**
    * @Constructor Date à partir de composants
    * @Description Construit une date validée
    * @param year Année [1601-30827]
    * @param month Mois [1-12]
    * @param day Jour [1-31]
    * @Throws std::invalid_argument Si validation échoue
    */
    Date::Date(int32 year, int32 month, int32 day) : 
        year(year), month(month), day(day) {
        Validate();
    }

    /// @Region: Méthodes de validation -------------------------------------------

    /**
    * @Function Validate
    * @Description Validation complète des composants de la date
    * @Throws std::invalid_argument Si un composant est invalide
    */
    void Date::Validate() const {
        if(year < 1601 || year > 30827)
            throw std::invalid_argument("Year must be between 1601-30827");
        
        if(month < 1 || month > 12)
            throw std::invalid_argument("Month must be 1-12");
            
        const int32 maxDay = DaysInMonth(year, month);
        if(day < 1 || day > maxDay)
            throw std::invalid_argument("Day invalid for current month/year");
    }

    /// @Region: Mutateurs avec propagation de validation -------------------------

    /**
    * @Function SetYear
    * @Description Modifie l'année avec validation étendue
    * @param year Nouvelle année [1601-30827]
    * @Throws std::invalid_argument Si hors limites
    */
    void Date::SetYear(int32 year) {
        if(year < 1601 || year > 30827)
            throw std::invalid_argument("Year must be between 1601-30827");
        this->year = year;
        Validate();
    }

    /**
    * @Function SetMonth
    * @Description Modifie le mois avec validation
    * @param month Nouveau mois [1-12]
    * @Throws std::invalid_argument Si hors limites
    */
    void Date::SetMonth(int32 month) {
        if(month < 1 || month > 12)
            throw std::invalid_argument("Month must be 1-12");
        this->month = month;
        Validate();
    }

    /**
    * @Function SetDay
    * @Description Modifie le jour avec validation contextuelle
    * @param day Nouveau jour [1-31]
    * @Throws std::invalid_argument Si incompatible avec mois/année
    */
    void Date::SetDay(int32 day) {
        this->day = day;
        Validate();
    }

    /// @Region: Méthodes statiques et utilitaires --------------------------------

    /**
    * @Function GetCurrent
    * @Description Obtient la date système locale thread-safe
    * @Note Gère les spécificités Windows/POSIX
    * @return Date Date courante validée
    */
    Date Date::GetCurrent() {
        std::time_t now = std::time(nullptr);
        
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        std::tm tmStruct;
        localtime_s(&tmStruct, &now);
        return Date(tmStruct.tm_year + 1900, 
                    tmStruct.tm_mon + 1, 
                    tmStruct.tm_mday);
    #else
        std::tm tmStruct;
        localtime_r(&now, &tmStruct);
        return Date(tmStruct.tm_year + 1900, 
                    tmStruct.tm_mon + 1, 
                    tmStruct.tm_mday);
    #endif
    }

    /// @Region: Formatage et conversion ------------------------------------------

    /**
    * @Function ToString
    * @Description Formatage ISO 8601 avec padding zéro
    * @Exemple "2025-01-05"
    * @return string Date formatée
    */
    std::string Date::ToString() const {
        std::ostringstream ss;
        ss << std::setfill('0') 
        << std::setw(4) << year << "-"
        << std::setw(2) << month << "-"
        << std::setw(2) << day;
        return ss.str();
    }

    /**
    * @Function GetMonthName
    * @Description Nom du mois en français
    * @Note L'implémentation actuelle utilise une locale fixe
    * @return string Nom du mois localisé
    */
    std::string Date::GetMonthName() const {
        static const char* months[] = {
            "Janvier", "Février", "Mars", "Avril", "Mai", "Juin",
            "Juillet", "Août", "Septembre", "Octobre", "Novembre", "Décembre"
        };
        return months[month - 1];
    }

    /// @Region: Calculs de date --------------------------------------------------

    /**
    * @Function DaysInMonth
    * @Description Calcule les jours dans un mois donné
    * @param year Année pour gestion bissextile
    * @param month Mois cible [1-12]
    * @return int32 Nombre de jours dans le mois
    * @Throws std::invalid_argument Si mois invalide
    */
    int32 Date::DaysInMonth(int32 year, int32 month) {
        if(month < 1 || month > 12)
            throw std::invalid_argument("Month must be 1-12");
            
        static const int32 days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        
        if(month == 2 && IsLeapYear(year))
            return 29;
            
        return days[month - 1];
    }

    /**
    * @Function IsLeapYear
    * @Description Détermine si une année est bissextile
    * @param year Année à vérifier
    * @return bool true si bissextile selon règles grégoriennes
    */
    bool Date::IsLeapYear(int32 year) noexcept {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    /// @Region: Opérateurs de comparaison ----------------------------------------

    /**
    * @Operator ==
    * @Description Comparaison d'égalité complète
    * @param other Date à comparer
    * @return bool true si dates identiques
    */
    bool Date::operator==(const Date& other) const noexcept {
        return year == other.year 
            && month == other.month 
            && day == other.day;
    }

    /**
    * @Operator !=
    * @Description Comparaison de différence
    * @param other Date à comparer
    * @return bool true si dates différentes
    */
    bool Date::operator!=(const Date& other) const noexcept {
        return !(*this == other);
    }

    /**
    * @Operator <
    * @Description Comparaison chronologique
    * @param other Date de comparaison
    * @return bool true si cette date est antérieure
    */
    bool Date::operator<(const Date& other) const noexcept {
        return std::tie(year, month, day) 
            < std::tie(other.year, other.month, other.day);
    }

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.