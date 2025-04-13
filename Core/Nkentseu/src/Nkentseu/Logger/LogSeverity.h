/**
* @File LogSeverity.h
* @Description Définit les niveaux de sévérité des logs
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/

#pragma once


#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/Platform.h"
#include <string>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace nkentseu {

    /**
    * @Class LogSeverity
    * @Description Gestion des niveaux de criticité des messages de log
    * 
    * @Fonctionnalités :
    * - Conversion enum <-> chaîne
    * - Comparaison insensible à la casse
    * - Validation des niveaux
    */
    class NKENTSEU_API LogSeverity {
    public:
        /**
        * @Enum Level
        * @Description Niveaux de sévérité des logs
        * 
        * @Valeurs :
        * - Trace : Suivi détaillé du flux d'exécution
        * - Debug : Informations de débogage
        * - Info : Événements normaux
        * - Warning : Comportements anormaux non critiques
        * - Error : Erreurs critiques
        * - Critical : Erreurs système graves
        * - Fatal : Erreurs irrécupérables
        * - Off : Désactivation complète des logs
        */
        enum class Level : int8 {
            Trace,    // Suivi détaillé du flux d'exécution
            Debug,    // Informations de débogage
            Info,     // Événements normaux
            Warning,  // Comportements anormaux non critiques
            Error,    // Erreurs critiques
            Critical, // Erreurs système graves
            Fatal,    // Erreurs irrécupérables
            Off       // Désactive tous les logs
        };

        /**
        * @Constructor LogSeverity
        * @Param severity Niveau de sévérité initial
        */
        explicit LogSeverity(Level severity);
        
        /**
        * @Function ToString
        * @Description Convertit le niveau en chaîne lisible
        * @return (std::string) Libellé formaté en majuscules
        */
        std::string ToString() const;

        /**
        * @Friend ToString
        * @Description : Surcharge amie de la fonction ToString
        * @param (const LogSeverity&) severity : LogSeverity à formater
        * @return (std::string) : Représentation formatée de la severity
        */
        friend std::string ToString(const LogSeverity& severity) {
            return severity.ToString();
        }
        
        /**
        * @Function FromString
        * @Description Convertit une chaîne en niveau de sévérité
        * @Param severity Chaîne insensible à la casse
        * @Throws std::invalid_argument Si chaîne non reconnue
        * @return (Level) Niveau correspondant
        */
        static Level FromString(const std::string& severity);
        
        /**
        * @Getter GetSeverity
        * @return (Level) Niveau de sévérité actuel
        */
        Level GetSeverity() const { return m_Severity; }

    private:
        Level m_Severity;
        
        /**
        * @Helper ToUpper
        * @Description Convertit une chaîne en majuscules
        * @Param str Chaîne d'entrée
        * @return (std::string) Chaîne en majuscules
        */
        static std::string ToUpper(const std::string& str);
    };

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.