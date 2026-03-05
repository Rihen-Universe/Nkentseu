/**
* @File FileLogger.cpp
* @Description Implémentation du système de journalisation dans des fichiers avec rotation
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/
#include "pch.h"
#include "FileLogger.h"

#include "Nkentseu/Epoch/Date.h"
#include "Nkentseu/Epoch/Time.h"
#include <fstream>
#include <filesystem> // C++17

namespace fs = std::filesystem;


namespace nkentseu {

    /**
    * @Function FileLogger
    * @Description Constructeur initialisant le système de fichiers
    * @param (const std::string&) basePath : Chemin de base pour le stockage des logs
    */
    FileLogger::FileLogger(const std::string& basePath, const std::string& baseName) : basePath(basePath), baseName(baseName) {
        fs::create_directories(basePath);
        RotateFile();
    }

    /**
    * @Function ~FileLogger
    * @Description Destructeur assurant la fermeture propre du fichier courant
    */
    FileLogger::~FileLogger() {
        if(currentFile.is_open()) {
            currentFile.close();
        }
    }

    /**
    * @Function Write
    * @Description Écrit un message dans le fichier de log courant
    * @param (const LogMessage&) msg : Message de log à enregistrer
    */
    void FileLogger::Write(const LogMessage& msg) {
        // Vérification de la taille du fichier avant écriture
        if(static_cast<usize>(currentFile.tellp()) > maxSizeMB * 1024 * 1024) {
            RotateFile(msg.date, msg.time);
        }
        
        // Formatage et écriture du message
        currentFile << "[" << msg.loggerName << "]"
                    << "[" << LogSeverity(msg.severity).ToString() << "]"
                    << "[" << msg.date.ToString() << " " << msg.time.ToString() << "] "
                    << "[" << msg.sourceFile << "-> (" << msg.sourceLine << ") -> " << msg.functionName << "]"
                    << " >> " << msg.message << std::endl;
    }

    /**
    * @Function Flush
    * @Description Force l'écriture des données en mémoire dans le fichier
    */
    void FileLogger::Flush() {
        if(currentFile.is_open()) {
            currentFile.flush();
        }
    }

    /**
    * @Function RotateFile
    * @Description Effectue une rotation du fichier de log selon la politique configurée
    * @param (const Date&) date : Date de référence pour le nom de fichier
    * @param (const Time&) time : Heure de référence pour le nom de fichier
    * @throws std::runtime_error Si l'ouverture du fichier échoue
    */
    void FileLogger::RotateFile(const Date& date, const Time& time) {
        if (currentFile.is_open()) {
            currentFile.close();
        }

        std::string filename = GenerateFilename(date, time);
        fs::path filePath(filename);

        // Création récursive des répertoires
        fs::create_directories(filePath.parent_path());
        
        currentFile = std::ofstream(
            filename.c_str(), std::ios::out | std::ios::app
        );

        if (!currentFile.is_open()) {
            throw std::runtime_error("Échec de création du fichier de log: " + filename);
        }
    }

    /**
    * @Function GenerateFilename
    * @Description Génère un nom de fichier unique basé sur la date/heure
    * @return (std::string) : Chemin complet du fichier formaté
    */
    std::string FileLogger::GenerateFilename(const Date& date, const Time& time) const {
        return Formatter.Format(FormatterSystem::Style::Curly, false,
            "{0}/{1}_{2}-{3}-{4}_{5}-{6}-{7}.log",
            basePath,
            baseName,
            date.GetYear(),
            date.GetMonth(),
            date.GetDay(),
            time.GetHour(),
            time.GetMinute(),
            time.GetSecond()
        );
    }

    /**
    * @Function SetMaxFileSize
    * @Description Définit la taille maximale des fichiers avant rotation
    * @param (usize) mb : Taille maximale en mégaoctets
    */
    void FileLogger::SetMaxFileSize(usize mb) {
        maxSizeMB = mb;
    }

    /**
    * @Function SetRotationPolicy
    * @Description Définit la durée de conservation des fichiers
    * @param (usize) days : Nombre maximal de jours de conservation
    */
    void FileLogger::SetRotationPolicy(usize days) {
        rotationDays = days;
    }

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.