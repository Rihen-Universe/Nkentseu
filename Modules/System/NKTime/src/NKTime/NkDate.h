/**
* @File NkDate.h
* @Description Définit la classe NkDate pour la manipulation de dates grégoriennes
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Apache-2.0
*/

#pragma once

#include "NkTimeExport.h"
#include "NkPlatformDetect.h"
#include "NkTypes.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace nkentseu {
    /**
    * - NkDate : Représente une date grégorienne validée
    *
    * @Description :
    * Cette classe permet la manipulation sécurisée de dates avec :
    * - Validation automatique des valeurs
    * - Calcul des années bissextiles
    * - Conversion depuis/dans les types systèmes
    * - Formatage selon différents standards
    *
    * @Members :
    * - (int32) year : Année [1601-30827]
    * - (int32) month : Mois [1-12]
    * - (int32) day : Jour [1-31]
    */
    class NKTIME_API NkDate {
    public:
        /// @Region: Constructeurs -------------------------------------------------

        /**
        * @Constructor NkDate par défaut
        * @Description Initialise avec la date système courante
        */
        NkDate() noexcept;

        /**
        * @Constructor NkDate à partir de composants
        * @Description Crée une date spécifique avec validation
        * @param (int32) year : Année [1601-30827]
        * @param (int32) month : Mois [1-12]
        * @param (int32) day : Jour [1-31]
        * @Throws std::invalid_argument : Si la date est invalide
        */
        NkDate(int32 year, int32 month, int32 day);

        /// @Region: Accesseurs -----------------------------------------------------
        /**
        * @Function GetYear
        * @return (int32) Année actuelle [1601-30827]
        */
        int32 GetYear() const noexcept { return year; }

        /**
        * @Function GetMonth
        * @return (int32) Mois actuel [1-12]
        */
        int32 GetMonth() const noexcept { return month; }

        /**
        * @Function GetDay
        * @return (int32) Jour actuel [1-31]
        */
        int32 GetDay() const noexcept { return day; }

        /// @Region: Mutateurs ------------------------------------------------------
        /**
        * @Function SetYear
        * @Description Modifie l'année avec validation
        * @param (int32) year : Nouvelle année [1601-30827]
        * @Throws std::invalid_argument : Si hors limites
        */
        void SetYear(int32 year);

        /**
        * @Function SetMonth
        * @Description Modifie le mois avec validation
        * @param (int32) month : Nouveau mois [1-12]
        * @Throws std::invalid_argument : Si hors limites
        */
        void SetMonth(int32 month);

        /**
        * @Function SetDay
        * @Description Modifie le jour avec validation
        * @param (int32) day : Nouveau jour [1-31]
        * @Throws std::invalid_argument : Si incompatible avec mois/année
        */
        void SetDay(int32 day);

        /// @Region: Méthodes statiques ---------------------------------------------
        /**
        * @Function GetCurrent
        * @Description Obtient la date système locale thread-safe
        * @return (NkDate) : Date courante
        */
        static NkDate GetCurrent();

        /**
        * @Function DaysInMonth
        * @Description Calcule le nombre de jours dans un mois
        * @param (int32) year : Année pour les bissextiles
        * @param (int32) month : Mois [1-12]
        * @return (int32) : Nombre de jours dans le mois
        * @Throws std::invalid_argument : Si mois invalide
        */
        static int32 DaysInMonth(int32 year, int32 month);

        /**
        * @Function IsLeapYear
        * @Description Détermine si une année est bissextile
        * @param (int32) year : Année à vérifier
        * @return (bool) : true si bissextile
        */
        static bool IsLeapYear(int32 year) noexcept;

        /// @Region: Formatage ------------------------------------------------------
        /**
        * @Function ToString
        * @Description Formatage ISO 8601 (YYYY-MM-DD)
        * @return (std::string) : Date formatée
        */
        std::string ToString() const;

        /**
        * @Function GetMonthName
        * @Description Nom localisé du mois en français
        * @return (std::string) : Nom du mois (ex: "Janvier")
        */
        std::string GetMonthName() const;

        /// @Region: Opérateurs -----------------------------------------------------
        /**
        * @Operator ==
        * @Description Comparaison d'égalité
        * @param (const NkDate&) other : Date à comparer
        * @return (bool) : true si dates identiques
        */
        bool operator==(const NkDate& other) const noexcept;

        /**
        * @Operator !=
        * @Description Comparaison de différence
        * @param (const NkDate&) other : Date à comparer
        * @return (bool) : true si dates différentes
        */
        bool operator!=(const NkDate& other) const noexcept;

        /**
        * @Operator <
        * @Description Comparaison chronologique
        * @param (const NkDate&) other : Date à comparer
        * @return (bool) : true si cette date est antérieure
        */
        bool operator<(const NkDate& other) const noexcept;

        /**
        * @Friend ToString
        * @Description Surcharge de la fonction de formatage
        * @param (const NkDate&) date : Date à formater
        * @return (std::string) : Date formatée
        */
        friend std::string ToString(const NkDate& date) {
            return date.ToString();
        }

    private:
        int32 year;   // Stockage de l'année [1601-30827]
        int32 month;  // Stockage du mois [1-12]
        int32 day;    // Stockage du jour [1-31]

        /// @Region: Validation interne ---------------------------------------------
        /**
        * @Function Validate
        * @Description Valide l'intégrité de la date
        * @Throws std::invalid_argument : Si composants invalides
        */
        void Validate() const;
    };

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.
