/**
* @File UnitestInfos.cpp
* @Description Implémentation des méthodes de la classe UnitestInfo.
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2024-04-10
* @License Rihen
*/

#include "pch.h"
#include "UnitestInfos.h"

namespace nkentseu {

    /**
    * @Function UnitestInfo::UnitestInfo
    * @description Constructeur par défaut initialisant les membres
    */
    UnitestInfo::UnitestInfo() : m_Condition(false) {}

    /**
    * @Function UnitestInfo::UnitestInfo
    * @description Constructeur paramétré complet
    * @param name Identifiant unique du test
    * @param condition Résultat d'exécution
    * @param file Fichier source du test
    * @param line Ligne dans le fichier
    * @param function Fonction testée
    * @param message Message informatif
    */
    UnitestInfo::UnitestInfo(const std::string& name, bool condition, const std::string& file,
        uint32 line, const std::string& function, const std::string& message)
        : m_Name(name), m_Condition(condition), m_File(file), m_Line(line),
        m_Function(function), m_Message(message) {}

    /**
    * @Function UnitestInfo::GetName
    * @description Accède à l'identifiant du test
    * @return Référence constante au nom du test
    */
    const std::string& UnitestInfo::GetName() const {
        return m_Name;
    }

    /**
    * @Function UnitestInfo::SetName
    * @description Modifie l'identifiant du test
    * @param name Nouvel identifiant à attribuer
    */
    void UnitestInfo::SetName(const std::string& name) {
        m_Name = name;
    }

    /**
    * @Function UnitestInfo::IsSuccessfull
    * @description Vérifie le statut d'exécution du test
    * @return État de réussite (true = réussi)
    */
    bool UnitestInfo::IsSuccessfull() const {
        return m_Condition;
    }

    /**
    * @Function UnitestInfo::SetSuccessfull
    * @description Définit le statut d'exécution du test
    * @param condition Nouvel état de réussite
    */
    void UnitestInfo::SetSuccessfull(bool condition) {
        m_Condition = condition;
    }

    /**
    * @Function UnitestInfo::GetFile
    * @description Récupère le fichier source du test
    * @return Référence constante au chemin du fichier
    */
    const std::string& UnitestInfo::GetFile() const {
        return m_File;
    }

    /**
    * @Function UnitestInfo::SetFile
    * @description Modifie le fichier source associé
    * @param file Nouveau chemin de fichier
    */
    void UnitestInfo::SetFile(const std::string& file) {
        m_File = file;
    }

    /**
    * @Function UnitestInfo::GetLine
    * @description Récupère la ligne de définition du test
    * @return Numéro de ligne dans le fichier source
    */
    uint32 UnitestInfo::GetLine() const {
        return m_Line;
    }

    /**
    * @Function UnitestInfo::SetLine
    * @description Modifie la ligne de définition du test
    * @param line Nouveau numéro de ligne
    */
    void UnitestInfo::SetLine(uint32 line) {
        m_Line = line;
    }

    /**
    * @Function UnitestInfo::GetFunction
    * @description Récupère le nom de la fonction testée
    * @return Référence constante au nom de fonction
    */
    const std::string& UnitestInfo::GetFunction() const {
        return m_Function;
    }

    /**
    * @Function UnitestInfo::SetFunction
    * @description Modifie le nom de la fonction testée
    * @param function Nouveau nom de fonction
    */
    void UnitestInfo::SetFunction(const std::string& function) {
        m_Function = function;
    }

    /**
    * @Function UnitestInfo::GetMessage
    * @description Récupère le message associé au test
    * @return Référence constante au message
    */
    const std::string& UnitestInfo::GetMessage() const {
        return m_Message;
    }

    /**
    * @Function UnitestInfo::SetMessage
    * @description Modifie le message associé au test
    * @param message Nouveau message contextuel
    */
    void UnitestInfo::SetMessage(const std::string& message) {
        m_Message = message;
    }

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2024 - Tous droits réservés.