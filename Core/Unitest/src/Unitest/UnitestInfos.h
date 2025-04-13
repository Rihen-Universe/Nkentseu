/**
* @File UnitestInfos.h
* @Description Définit la classe UnitestInfo pour stocker les informations d'un test unitaire.
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2024-04-10
* @License Rihen
*/

#pragma once

#include "Nkentseu/Nkentseu.h"

namespace nkentseu {

    /**
    * - UnitestInfo : Stocke les métadonnées complètes d'une exécution de test unitaire.
    *
    * @Description :
    * Cette classe encapsule toutes les informations générées pendant l'exécution d'un test,
    * permettant le reporting détaillé et le débogage.
    *
    * @Members :
    * - (std::string) m_Name : Identifiant unique du cas de test.
    * - (bool) m_Condition : Résultat booléen de l'exécution.
    * - (std::string) m_File : Chemin complet du fichier source.
    * - (uint32) m_Line : Numéro de ligne dans le code source.
    * - (std::string) m_Function : Nom qualifié de la fonction/méthode testée.
    * - (std::string) m_Message : Message de log ou d'erreur associé.
    * - (UnitestInfo()) : Constructeur par défaut.
    * - (UnitestInfo(...)) : Constructeur paramétré complet.
    * - (GetName/SetName) : Accesseur/Mutateur pour m_Name.
    * - (IsSuccessfull/SetSuccessfull) : Accesseur/Mutateur pour m_Condition.
    * - (GetFile/SetFile) : Accesseur/Mutateur pour m_File.
    * - (GetLine/SetLine) : Accesseur/Mutateur pour m_Line.
    * - (GetFunction/SetFunction) : Accesseur/Mutateur pour m_Function.
    * - (GetMessage/SetMessage) : Accesseur/Mutateur pour m_Message.
    */
    class NKENTSEU_API UnitestInfo {
    public:
        /**
        * @Function UnitestInfo
        * @Description Constructeur par défaut initialisant les membres avec des valeurs par défaut
        */
        UnitestInfo();

        /**
        * @Function UnitestInfo
        * @Description Constructeur paramétré pour initialisation complète
        * @param (const std::string&) name : Nom identifiant du test
        * @param (bool) condition : État de réussite (true = succès)
        * @param (const std::string&) file : Fichier source contenant le test
        * @param (uint32) line : Ligne de définition du test
        * @param (const std::string&) function : Nom complet de la fonction testée
        * @param (const std::string&) message : Message contextuel ou d'erreur
        */
        UnitestInfo(const std::string& name, bool condition, const std::string& file,
            uint32 line, const std::string& function, const std::string& message);

        /// @region Accesseurs et Mutateurs

        /**
        * @Function GetName
        * @Description Retourne l'identifiant du cas de test
        * @return (const std::string&) : Référence constante au nom du test
        */
        const std::string& GetName() const;
        
        /**
        * @Function SetName
        * @Description Définit l'identifiant du cas de test
        * @param (const std::string&) name : Nouveau nom à attribuer
        * @return (void)
        */
        void SetName(const std::string& name);

        /**
        * @Function IsSuccessfull
        * @Description Indique si le test s'est exécuté avec succès
        * @return (bool) : true si le test a réussi, false sinon
        */
        bool IsSuccessfull() const;

        /**
        * @Function SetSuccessfull
        * @Description Définit l'état de réussite du test
        * @param (bool) condition : Nouvel état de réussite
        * @return (void)
        */
        void SetSuccessfull(bool condition);

        /**
        * @Function GetFile
        * @Description Retourne le chemin du fichier source contenant le test
        * @return (const std::string&) : Référence constante au chemin du fichier
        */
        const std::string& GetFile() const;

        /**
        * @Function SetFile
        * @Description Définit le chemin du fichier source du test
        * @param (const std::string&) file : Nouveau chemin de fichier
        * @return (void)
        */
        void SetFile(const std::string& file);

        /**
        * @Function GetLine
        * @Description Retourne le numéro de ligne où le test est défini
        * @return (uint32) : Numéro de ligne dans le fichier source
        */
        uint32 GetLine() const;

        /**
        * @Function SetLine
        * @Description Définit le numéro de ligne du test dans le fichier source
        * @param (uint32) line : Nouveau numéro de ligne
        * @return (void)
        */
        void SetLine(uint32 line);

        /**
        * @Function GetFunction
        * @Description Retourne le nom complet de la fonction testée
        * @return (const std::string&) : Référence constante au nom de fonction
        */
        const std::string& GetFunction() const;

        /**
        * @Function SetFunction
        * @Description Définit le nom de la fonction testée
        * @param (const std::string&) function : Nouveau nom de fonction
        * @return (void)
        */
        void SetFunction(const std::string& function);

        /**
        * @Function GetMessage
        * @Description Retourne le message associé au test
        * @return (const std::string&) : Référence constante au message
        */
        const std::string& GetMessage() const;

        /**
        * @Function SetMessage
        * @Description Définit un message contextuel pour le test
        * @param (const std::string&) message : Nouveau message à associer
        * @return (void)
        */
        void SetMessage(const std::string& message);

    private:
        std::string m_Name;
        bool m_Condition;
        std::string m_File;
        uint32 m_Line;
        std::string m_Function;
        std::string m_Message;
    };

} // namespace nkentseu