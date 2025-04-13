/**
* @File UnitestCase.cpp
* @Description Implémentation des méthodes de gestion des cas de test unitaires
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2024-04-10
* @License Rihen
*/

#include "pch.h"
#include "UnitestCase.h"
#include <optional>

namespace nkentseu {

    /**
    * @Function UnitestCase::UnitestCase
    * @description Initialise un nouveau cas de test avec ses métadonnées
    * @param name Identifiant unique du test
    * @param function Fonction exécutable du test
    * @param description Information contextuelle optionnelle
    */
    UnitestCase::UnitestCase(const std::string& name, UnitestCaseFunction function, const std::string& description)
        : m_Name(name), m_Function(function), m_Description(description) {}

    /**
    * @Function UnitestCase::Reset
    * @description Réinitialise l'état d'exécution du test
    */
    void UnitestCase::Reset() {
        m_Passeds = 0;
        m_Faileds = 0;
        m_UnitestInfos.clear();
    }

    /**
    * @Function UnitestCase::Run
    * @description Lance l'exécution du test si le contexte correspond
    * @param context Filtre d'exécution basé sur le nom du test
    * @return (bool) Indique si l'exécution a eu lieu
    */
    bool UnitestCase::Run(const std::string& context) {
        if (m_Function != nullptr && context == this->GetName()) {
            m_Function(context);
            return true;
        }
        return false;
    }

    /**
    * @Function UnitestCase::AddUnitestInfo
    * @description Enregistre un nouveau résultat de test
    * @param unitestInfo Résultat à ajouter à l'historique
    */
    void UnitestCase::AddUnitestInfo(const UnitestInfo& unitestInfo) {
        m_UnitestInfos.push_back(unitestInfo);
        unitestInfo.IsSuccessfull() ? m_Passeds++ : m_Faileds++;
    }

    /**
    * @Function UnitestCase::GetTotal
    * @description Calcule le nombre total de tests effectués
    * @return (uint32) Total des exécutions réussies et échouées
    */
    uint32 UnitestCase::GetTotal() const {
        return m_Passeds + m_Faileds;
    }

    /// @region Implémentation des accesseurs

    const std::string& UnitestCase::GetName() const {
        return m_Name;
    }

    const std::string& UnitestCase::GetDescription() const {
        return m_Description;
    }

    const std::vector<UnitestInfo>& UnitestCase::GetUnitestInfos() const {
        return m_UnitestInfos;
    }

    UnitestInfo& UnitestCase::GetUnitestInfo(uint32 index) {
        if (index < GetUnitestInfoCount()) {
            return m_UnitestInfos[index];
        }
        throw std::out_of_range("Index hors limites pour UnitestInfos");
    }

    uint32 UnitestCase::GetUnitestInfoCount() const {
        return (uint32)m_UnitestInfos.size();
    }

    uint32 UnitestCase::GetPassedCount() const {
        return m_Passeds;
    }

    uint32 UnitestCase::GetFailedCount() const {
        return m_Faileds;
    }

    const UnitestCaseFunction& UnitestCase::GetFunction() const {
        return m_Function;
    }

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2024 - Tous droits réservés.