/**
* @File Unitest.h
* @Description Définit la classe Unitest pour la gestion centralisée de tests unitaires.
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2024-04-10
* @License Rihen
*/

#pragma once

#include <memory>
#include <unordered_map>
#include <source_location>
#include "Nkentseu/Nkentseu.h"
#include "UnitestCase.h"
#include "UnitestState.h"

namespace nkentseu {

    /**
    * @typedef FunctionRegistryName
    * @brief Alias pour le type des noms de fonctions enregistrées
    * @Description : Utilisé comme clé dans le registre des tests
    */
    using FunctionRegistryName = std::string;

    // Macros d'enregistrement
    /// @region Macros d'enregistrement des tests
    
    /**
    * @macro UnitestRegisterIClass
    * @description Enregistre une méthode de classe comme test unitaire
    * @param name Nom de la méthode
    * @param description Description du scénario
    */
    #define UnitestRegisterIClass(name, description) #name, std::bind(&name, this, std::placeholders::_1), description
    
    /**
    * @macro UnitestRegisterClass
    * @description Enregistre une méthode de classe comme test unitaire
    * @param name Nom de la méthode
    * @param description Description du scénario
    */
    #define UnitestRegisterClass(object, name, description) #name, std::bind(&name, &object, std::placeholders::_1), description
    
    /**
    * @macro UnitestRegisterFunction
    * @description Enregistre une fonction libre comme test unitaire
    * @param name Nom de la fonction
    * @param description Description du scénario
    */
    #define UnitestRegisterFunction(name, description) #name, name, description
    
    /**
    * @macro UnitestRegisterLambda
    * @description Enregistre une lambda comme test unitaire
    * @param name Nom du test
    * @param lambda Expression lambda
    * @param description Description du scénario
    */
    #define UnitestRegisterLambda(name, lambda, description) #name, lambda, description

    /// @endregion

    /**
    * @class UnitestSystem
    * @brief Gestionnaire central de tests unitaires (pattern Singleton)
    *
    * @Description :
    * Classe centrale du framework de test qui gère :
    * - L'enregistrement des cas de test
    * - L'exécution des tests
    * - La collecte des résultats
    * - Le reporting détaillé
    *
    * @DesignPattern : Singleton (une seule instance possible)
    * @ThreadSafety : Non thread-safe (utilisation mono-thread)
    */
    class NKENTSEU_API UnitestSystem {
    public:
        /**
        * @Function Instance
        * @description Accès à l'instance unique du gestionnaire
        * @return (UnitestSystem&) Référence à l'instance singleton
        * @note Premier appel initialise l'instance
        */
        static UnitestSystem& Instance();

        /// @region Gestion du cycle de vie des tests
        
        /**
        * @Function Register
        * @description Enregistre un nouveau cas de test dans le registre
        * @param name Identifiant unique du test (doit être non vide)
        * @param function Fonction/méthode à exécuter
        * @param description [Optionnel] Description lisible du test
        * @throws std::invalid_argument Si le nom est vide ou existe déjà
        * @warning Les noms en double écrasent les précédents enregistrements
        */
        void Register(const std::string& name, UnitestCaseFunction function, const std::string& description = "");

        /**
        * @Function Run
        * @description Exécute l'ensemble des tests enregistrés
        * @return (int32) Nombre total d'assertions échouées
        * @process :
        * 1. Réinitialisation des compteurs
        * 2. Exécution séquentielle des tests
        * 3. Collecte des résultats
        * 4. Génération des rapports
        */
        int32 Run();

        /// @endregion

        /// @region Système de vérification
        
        /**
        * @Function Verify
        * @description Vérifie une condition avec formatage de message
        * @tparam Args Types des arguments variables pour le formatage
        * @param condition Expression booléenne à valider
        * @param file Fichier source (__FILE__)
        * @param line Numéro de ligne (__LINE__)
        * @param function Fonction appelante (__FUNCTION__)
        * @param errorString Message d'erreur de base
        * @param pitch Indentation/niveau hiérarchique
        * @param format Format de message style printf
        * @param args Arguments à formater
        * @return (UnitestState) Résultat de la vérification
        * @note Utilise Formatter pour le formatage avancé
        */
        template<typename... Args>
        UnitestState Verify(bool condition, const std::source_location& loc, const Date& date, const Time& time, const std::string& errorString, 
            const std::string& pitch, const std::string& format = "", Args&&... args) {
            std::string message = Formatter.FormatCurly(format, args...);
            message = Formatter.FormatCurly("[{0}{1}]; {2}", pitch, errorString, message);
            return VerifyInternal(condition, loc, date, time, message);
        }

        /// @endregion

        /// @region Configuration du reporting
        
        /**
        * @Function IsPassedDetailPrint
        * @description Indique si l'affichage des succès est activé
        * @return (bool) État actuel du flag d'affichage
        */
        bool IsPassedDetailPrint() const;
        
        /**
        * @Function PrintPassedDetails
        * @description Active/désactive l'affichage des assertions réussies
        * @param print Nouvel état de l'affichage
        */
        void PrintPassedDetails(bool print);
        
        /**
        * @Function IsFailedDetailPrint
        * @description Indique si l'affichage des échecs est activé
        * @return (bool) État actuel du flag d'affichage
        */
        bool IsFailedDetailPrint() const;
        
        /**
        * @Function PrintFailedDetails
        * @description Active/désactive l'affichage des assertions échouées
        * @param print Nouvel état de l'affichage
        */
        void PrintFailedDetails(bool print);
        
        /**
        * @Function IsDetailPrint
        * @description Indique si l'affichage global est activé
        * @return (bool) État global de l'affichage
        */
        bool IsDetailPrint() const;
        
        /**
        * @Function PrintDetails
        * @description Active/désactive tous les affichages détaillés
        * @param print Nouvel état global
        */
        void PrintDetails(bool print);

        /**
        * @Function Get
        * @description Accède au système de journalisation
        * @return (Logger&) Référence au logger interne
        */
        Logger& Get();

        /// @endregion

    private:
        Logger m_Logger; ///< Système de journalisation intégré
        using UnitestEntryPtr = SharedPtr<UnitestCase>;
        UnitestSystem(); ///< Constructeur privé (Singleton)
        
        FunctionRegistryName m_CurrentRegister; ///< Nom du test en cours d'enregistrement
        std::unordered_map<FunctionRegistryName, UnitestEntryPtr> m_UnitestList; ///< Registre des tests
        
        // Flags de configuration d'affichage
        bool m_PrintFailedDetails = true; ///< Affiche les détails des échecs
        bool m_PrintPassedDetails = true; ///< Affiche les détails des succès
        bool m_PrintDetails = true;       ///< Active/désactive tous les détails

        /**
        * @Function VerifyInternal
        * @description Implémentation interne de la vérification
        * @param condition Résultat booléen à enregistrer
        * @param file Fichier source de l'assertion
        * @param line Ligne du code source
        * @param function Fonction parente
        * @param message Message formaté complet
        * @return (UnitestState) Résultat de la vérification
        * @warning Ne pas appeler directement (utiliser les macros)
        */
        UnitestState VerifyInternal(bool condition, const std::source_location& loc = std::source_location::current(), const Date& date = Date::GetCurrent(), const Time& time = Time::GetCurrent(), const std::string& message = "");
    };

    /**
    * @Function RunTest
    * @description Point d'entrée standard pour l'exécution des tests
    * @return (int32) Code de retour (0 = succès total)
    * @note Affiche un bandeau de démarrage/arrêt stylisé
    */
    int32 NKENTSEU_API RunTest();

    #define Unitest UnitestSystem::Instance() ///< Alias pour l'instance unique de UnitestSystem

    /// @region Macros de vérification avancées
    
    /**
    * @defgroup TestMacros Macros de vérification
    * @brief Ensemble de macros prédéfinies pour les assertions
    * @{
    */

    #define UNITEST_ABS(x, y) ((x) < (y) ? (y) - (x) : (x) - (y)) ///< Macro d'absorption de valeur
    #define UNITEST_ABS_APPROX(x, y, epsilon) (UNITEST_ABS(x, y) < (epsilon)) ///< Macro d'approximation d'absorption
    
    #define UNITEST_REGISTRY ::nkentseu::Unitest.Register ///< Macro d'enregistrement
    #define UNITEST_VERIFY ::nkentseu::Unitest.Verify     ///< Macro de vérification générique
    #define UNITEST_HEADER std::source_location::current(), ::nkentseu::Date::GetCurrent(), ::nkentseu::Time::GetCurrent() ///< Métadonnées d'appel
    #define STRAUT(a) #a ///< Stringification d'expression

    // Conditions de base
    #define UNITEST(value, ...)                             UNITEST_VERIFY((value), UNITEST_HEADER, "", #value, __VA_OPT__(__VA_ARGS__)) ///< Vérification booléenne
    #define UNITEST_NULL(value, ...)                        UNITEST_VERIFY((value == nullptr), UNITEST_HEADER, " Is Null", #value, __VA_OPT__(__VA_ARGS__)) ///< Vérification nullptr
    #define UNITEST_TRUE(value, ...)                        UNITEST_VERIFY(value, UNITEST_HEADER, " Is True", #value, __VA_OPT__(__VA_ARGS__)) ///< Vérification true
    #define UNITEST_FALSE(value, ...)                       UNITEST_VERIFY(!(value), UNITEST_HEADER, " Is False", #value, __VA_OPT__(__VA_ARGS__)) ///< Vérification false

    // Comparaisons
    #define UNITEST_EQUAL(value1, value2, ...)              UNITEST_VERIFY((value1) == (value2), UNITEST_HEADER, "", STRAUT(value1 == value2), __VA_OPT__(__VA_ARGS__)) ///< Égalité stricte
    #define UNITEST_DIFFERENT(value1, value2, ...)          UNITEST_VERIFY((value1) != (value2), UNITEST_HEADER, "", STRAUT(value1 != value2), __VA_OPT__(__VA_ARGS__)) ///< Différence stricte
    #define UNITEST_LESS(value1, value2, ...)               UNITEST_VERIFY((value1) < (value2), UNITEST_HEADER, "", STRAUT(value1 < value2), __VA_OPT__(__VA_ARGS__)) ///< Comparaison <
    #define UNITEST_GREATER(value1, value2, ...)            UNITEST_VERIFY((value1) > (value2), UNITEST_HEADER, "", STRAUT(value1 > value2), __VA_OPT__(__VA_ARGS__)) ///< Comparaison >
    #define UNITEST_LESS_OR_EQUAL(value1, value2, ...)      UNITEST_VERIFY((value1) <= (value2), UNITEST_HEADER, "", STRAUT(value1 <= value2), __VA_OPT__(__VA_ARGS__)) ///< Comparaison <=
    #define UNITEST_GREATER_OR_EQUAL(value1, value2, ...)   UNITEST_VERIFY((value1) >= (value2), UNITEST_HEADER, "", STRAUT(value1 >= value2), __VA_OPT__(__VA_ARGS__)) ///< Comparaison >=

    // Conditions avancées
    #define UNITEST_APPROX(value, expected, epsilon, ...)   UNITEST_VERIFY(UNITEST_ABS_APPROX(value, expected, epsilon), UNITEST_HEADER, " Approx Check", STRAUT(value ≈ expected), __VA_OPT__(__VA_ARGS__)) ///< Comparaison approximative
    #define UNITEST_EMPTY(container, ...)                   UNITEST_VERIFY((container.Empty()), UNITEST_HEADER, " Is Empty", #container, __VA_OPT__(__VA_ARGS__)) ///< Vérification conteneur vide

    /** @} */ // End of TestMacros

} // namespace nkentseu