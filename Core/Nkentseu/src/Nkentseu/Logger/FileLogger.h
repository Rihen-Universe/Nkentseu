/**
* @File FileLogger.h
* @Description Implémente une cible de journalisation vers des fichiers avec rotation automatique.
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/

#pragma once

#include "LoggerTarget.h"
#include "Nkentseu/Memory/Memory.h"
#include "Nkentseu/Memory/SharedPtr.h"
#include "Nkentseu/Platform/Types.h"

#include <fstream>
#include <string>
#include <memory>


namespace nkentseu {

    /**
    * - FileLogger : Journalisation vers fichiers avec rotation.
    *
    * @Description :
    * Cette classe écrit les logs dans des fichiers avec gestion automatique de rotation
    * selon la taille maximale ou la durée de conservation. Supporte le formatage temporel.
    *
    * @Members :
    * - (std::ofstream) currentFile : Fichier de log courant
    * - (std::string) basePath : Chemin de base des fichiers de log
    * - (usize) maxSizeMB : Taille max avant rotation (en Mo)
    * - (usize) rotationDays : Durée max de conservation (en jours)
    */
    class NKENTSEU_API FileLogger : public LoggerTarget {
    public:
        /**
        * @Function FileLogger
        * @Description Constructeur initialisant le système de fichiers
        * @param (const std::string&) basePath : Chemin de base pour les fichiers de log
        */
        explicit FileLogger(const std::string& basePath = "logs", const std::string& baseName = "none");
        
        /**
        * @Function ~FileLogger
        * @Description Destructeur assurant la fermeture propre du fichier
        */
        ~FileLogger();

        /**
        * @Function SetMaxFileSize
        * @Description Définit la taille maximale des fichiers avant rotation
        * @param (usize) mb : Taille maximale en mégaoctets
        */
        void SetMaxFileSize(usize mb);

        /**
        * @Function SetRotationPolicy
        * @Description Définit la durée de conservation des fichiers
        * @param (usize) days : Nombre maximal de jours de conservation
        */
        void SetRotationPolicy(usize days);

    protected:
        /**
        * @Function Write
        * @Description Écrit un message dans le fichier courant (override)
        * @param (const LogMessage&) msg : Message de log à enregistrer
        */
        void Write(const LogMessage& msg) override;

        /**
        * @Function Flush
        * @Description Force l'écriture des données en mémoire dans le fichier (override)
        */
        void Flush() override;

    private:
        /**
        * @Function RotateFile
        * @Description Effectue une rotation des fichiers selon les politiques configurées
        * @param (const Date&) date : Date de référence pour la rotation
        * @param (const Time&) time : Heure de référence pour la rotation
        */
        void RotateFile(const Date& date = Date::GetCurrent(), const Time& time = Time::GetCurrent());

        /**
        * @Function GenerateFilename
        * @Description Génère un nom de fichier basé sur la date/heure
        * @return (std::string) : Nom de fichier formaté
        */
        std::string GenerateFilename(const Date& date, const Time& time) const;

        std::ofstream currentFile;
        std::string basePath;
        std::string baseName;
        usize maxSizeMB = 10;
        usize rotationDays = 1;
    };

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.