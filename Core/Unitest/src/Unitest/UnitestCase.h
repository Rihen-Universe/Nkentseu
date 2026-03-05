/**
* @File UnitestCase.h
* @Description Définit la classe UnitestCase pour représenter un cas de test unitaire.
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2024-04-10
* @License Rihen
*/

#pragma once

#include "Nkentseu/Nkentseu.h"
#include <vector>
#include <functional>
#include "UnitestInfos.h"

namespace nkentseu {

    /**
    * @typedef UnitestCaseFunction
    * @brief Alias pour une fonction de test prenant un contexte en paramètre
    * @Description :
    * Représente une fonction exécutable dans un contexte de test spécifique.
    * Le paramètre `context` permet de filtrer les tests à exécuter.
    */
    using UnitestCaseFunction = std::function<void(const std::string&)>;

    /**
    * - UnitestCase : Conteneur pour un cas de test unitaire complet.
    *
    * @Description :
    * Cette classe regroupe toutes les métadonnées et résultats d'un scénario de test,
    * permettant l'exécution, le suivi et le reporting des résultats.
    *
    * @Members :
    * - (std::string) m_Name : Identifiant unique du cas de test
    * - (UnitestCaseFunction) m_Function : Fonction de test à exécuter
    * - (std::string) m_Description : Description optionnelle du scénario
    * - (std::vector<UnitestInfo>) m_UnitestInfos : Historique des exécutions
    * - (uint32) m_Passeds : Nombre de tests réussis
    * - (uint32) m_Faileds : Nombre de tests échoués
    */
    class NKENTSEU_API UnitestCase {
    public:
        /**
        * @Function UnitestCase
        * @description Constructeur initialisant les éléments principaux
        * @param name Identifiant du cas de test
        * @param function Fonction de test à exécuter
        * @param description [Optionnel] Explication du scénario
        */
        UnitestCase(const std::string& name, UnitestCaseFunction function, const std::string& description = "");

        /// @region Gestion du cycle de vie

        /**
        * @Function Reset
        * @description Réinitialise les compteurs et l'historique des exécutions
        */
        void Reset();

        /**
        * @Function Run
        * @description Exécute le test dans un contexte donné
        * @param context Filtre d'exécution basé sur l'identifiant
        * @return (bool) true si l'exécution a été effectuée, false sinon
        */
        bool Run(const std::string& context);

        /**
        * @Function AddUnitestInfo
        * @description Ajoute un résultat d'exécution au journal
        * @param unitestInfo Résultat à enregistrer
        */
        void AddUnitestInfo(const UnitestInfo& unitestInfo);

        /// @region Accesseurs

        /**
        * @Function GetTotal
        * @description Retourne le nombre total de tests exécutés
        * @return (uint32) Somme des tests réussis et échoués
        */
        uint32 GetTotal() const;

        /**
        * @Function GetName
        * @description Retourne l'identifiant du cas de test
        * @return (const std::string&) Référence constante au nom
        */
        const std::string& GetName() const;

        /**
        * @Function GetDescription
        * @description Retourne la description du scénario
        * @return (const std::string&) Référence constante à la description
        */
        const std::string& GetDescription() const;

        /**
        * @Function GetUnitestInfos
        * @description Accède à l'historique complet des exécutions
        * @return (const std::vector<UnitestInfo>&) Référence constante aux résultats
        */
        const std::vector<UnitestInfo>& GetUnitestInfos() const;

        /**
        * @Function GetUnitestInfo
        * @description Accède à un résultat spécifique par index
        * @param index Position dans la liste des résultats
        * @return (UnitestInfo&) Référence au résultat demandé
        * @throws std::out_of_range Si l'index est invalide
        */
        UnitestInfo& GetUnitestInfo(uint32 index);

        /**
        * @Function GetUnitestInfoCount
        * @description Retourne le nombre de résultats enregistrés
        * @return (uint32) Taille de la liste des résultats
        */
        uint32 GetUnitestInfoCount() const;

        /**
        * @Function GetPassedCount
        * @description Retourne le nombre de tests réussis
        * @return (uint32) Valeur du compteur de succès
        */
        uint32 GetPassedCount() const;

        /**
        * @Function GetFailedCount
        * @description Retourne le nombre de tests échoués
        * @return (uint32) Valeur du compteur d'échecs
        */
        uint32 GetFailedCount() const;

        /**
        * @Function GetFunction
        * @description Accède à la fonction de test associée
        * @return (const UnitestCaseFunction&) Référence à la fonction
        */
        const UnitestCaseFunction& GetFunction() const;

    private:
        std::string m_Name;
        UnitestCaseFunction m_Function;
        std::string m_Description;
        std::vector<UnitestInfo> m_UnitestInfos;
        uint32 m_Passeds;
        uint32 m_Faileds;
    };

} // namespace nkentseu