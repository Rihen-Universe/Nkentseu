/**
* @File ConsoleLogger.h
* @Description Implémente une cible de journalisation vers la console avec support des couleurs ANSI.
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/

#pragma once

#include "LoggerTarget.h"
#include "LogSeverity.h"
#include <iostream>


namespace nkentseu {

    /**
    * - ConsoleLogger : Cible de journalisation vers la console.
    *
    * @Description :
    * Cette classe permet d'envoyer les messages de log vers la console avec un formatage coloré.
    * Gère automatiquement l'activation des séquences ANSI sur Windows.
    *
    * @Members :
    * - (ColorCode) colors : Codes de couleurs ANSI pour les différents niveaux de log.
    * - (static const char*) GetColor : Récupère le code couleur selon le niveau de sévérité.
    * - (static void) EnableWindowsANSI : Active le support ANSI sur Windows.
    */
    class NKENTSEU_API ConsoleLogger : public LoggerTarget {
    public:
        /**
        * - ColorCode : Stocke les codes de couleurs ANSI.
        *
        * @Description :
        * Structure contenant les séquences d'échappement ANSI pour le coloriage des logs.
        */
        struct ColorCode {
            const char* reset = "\033[0m";        // Réinitialisation
            const char* app = "\033[44m\033[37m"; // Style application
            const char* trace = "\033[36m";       // Cyan
            const char* debug = "\033[34m";       // Bleu
            const char* info = "\033[32m";        // Vert
            const char* warning = "\033[33m";     // Jaune
            const char* error = "\033[31m";       // Rouge
            const char* critical = "\033[35m";    // Magenta
            const char* fatal = "\033[41m\033[97m"; // Fond rouge + texte blanc
        };

        /**
        * @Function GetColor
        * @Description Récupère le code couleur correspondant au niveau de log.
        * @param (LogSeverity::Level) level : Niveau de sévérité du message
        * @param (const ColorCode&) colors : Référence aux codes de couleur
        * @return (const char*) : Séquence ANSI correspondante
        */
        static const char* GetColor(LogSeverity::Level level, const ColorCode& colors);
        
        /**
        * @Function EnableWindowsANSI
        * @Description Active le support des codes ANSI dans la console Windows
        */
        static void EnableWindowsANSI();

    protected:
        /**
        * @Function Write
        * @Description Écrit un message formatté dans la console (override)
        * @param (const LogMessage&) msg : Message de log à écrire
        */
        void Write(const LogMessage& msg) override;

        /**
        * @Function Flush
        * @Description Vide le buffer de sortie de la console (override)
        */
        void Flush() override;

    private:
        const ColorCode colors; // Codes de couleurs configurés
    };

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.