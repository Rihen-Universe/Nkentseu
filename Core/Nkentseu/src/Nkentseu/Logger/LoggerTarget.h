#pragma once

/**
* @File LoggerTarget.h
* @Description Définit les structures et interfaces pour le système de logging
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/

#include "Nkentseu/Platform/Export.h"
#include "LogSeverity.h"
#include "Nkentseu/Epoch/Date.h"
#include "Nkentseu/Epoch/Time.h"
#include "Nkentseu/Platform/Types.h"
#include <string>

namespace nkentseu {

    /**
    * @Struct LogMessage
    * @Description Conteneur complet pour un message de log
    * 
    * @Members :
    * - (Date) date : Date du jour au format grégorien
    * - (Time) time : Heure précise avec nanosecondes
    * - (LogSeverity::Level) severity : Niveau de criticité
    * - (std::string) loggerName : Identifiant du logger émetteur
    * - (std::string) message : Contenu du message
    * - (std::string) sourceFile : Fichier source d'origine (optionnel)
    * - (int32) sourceLine : Ligne dans le code source (optionnel)
    * - (std::string) functionName : Fonction/méthode d'origine (optionnel)
    */
    struct NKENTSEU_API LogMessage {
        Date date;                 
        Time time;                 
        LogSeverity::Level severity; 
        std::string loggerName;    
        std::string message;       
        std::string sourceFile;    
        int32 sourceLine;            
        std::string functionName;  

        /**
        * @Constructor LogMessage
        * @Description Construit un message de log complet
        * @Param logger Nom du logger (doit être non vide)
        * @Param severityLevel Niveau de sévérité
        * @Param msg Message principal (doit être non vide)
        * @Param date Date de génération (défaut=date courante)
        * @Param time Heure de génération (défaut=heure courante)
        * @Param file Fichier source (défaut=vide)
        * @Param line Ligne source (défaut=0)
        * @Param func Fonction appelante (défaut=vide)
        */
        LogMessage(
            const std::string& logger,
            LogSeverity::Level severityLevel,
            const std::string& msg,
            const Date& date = Date::GetCurrent(),
            const Time& time = Time::GetCurrent(),
            const std::string& file = "",
            int32 line = 0,
            const std::string& func = ""
        );
    };

    /**
    * @Class LoggerTarget
    * @Description Interface abstraite pour les cibles de logging
    * 
    * @Fonctionnalités :
    * - Définit le contrat pour les implémentations concrètes
    * - Gère l'écriture et le flush des logs
    * - Permet l'extension via héritage
    */
    class NKENTSEU_API LoggerTarget {
    public:
        virtual ~LoggerTarget() = default;

    protected:
        /**
        * @Function Write
        * @Description Écriture effective du message (à implémenter)
        * @Param message Message de log complet
        */
        virtual void Write(const LogMessage& message) = 0;

        /**
        * @Function Flush
        * @Description Force le vidage des buffers (implémentation vide par défaut)
        */
        virtual void Flush() {} 

    public:
        /**
        * @Function Log
        * @Description Transmet le message à la cible
        * @Param message Message à logger
        */
        void Log(const LogMessage& message) { Write(message); }

        /**
        * @Function ForceFlush
        * @Description Appelle le vidage d'urgence des buffers
        */
        void ForceFlush() { Flush(); }
    };

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.