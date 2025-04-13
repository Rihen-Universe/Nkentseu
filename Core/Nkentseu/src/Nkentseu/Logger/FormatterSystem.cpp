/**
* @File FormatterSystem.cpp
* @Description Implémentation du système de formatage de chaînes
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/
#include "pch.h"
#include "FormatterSystem.h"

#include <cctype>
#include <string>
#include <stdexcept>
#include <limits>
#include <iostream>


namespace nkentseu {

    /**
    * @Function ToString
    * @brief Convertit une valeur booléenne en chaîne localisée
    * @param value Valeur booléenne à convertir
    * @return "True" si vrai, "False" sinon
    */
    std::string ToString(bool value) {
        std::stringstream ss;
        ss << ((value) ? "True" : "False");
        return ss.str();
    }

    /**
    * @Function Instance
    * @brief Accède à l'instance unique du formatter (singleton)
    * @return Référence à l'instance FormatterSystem
    */
    FormatterSystem& FormatterSystem::Instance() {
        static FormatterSystem instance;
        return instance;
    }

    /**
    * @Function ExtractIndex
    * @brief Extrait un index numérique d'une chaîne de placeholder
    * @param token Chaîne contenant l'index à parser
    * @return Index numérique valide ou -1 si invalide
    * @throws Aucune exception directe, retourne -1 pour les cas d'erreur :
    * - Caractères non numériques
    - Overflow numérique
    * - Zéros non significatifs
    */
    int32 FormatterSystem::ExtractIndex(const std::string& token) {
        if (token.empty() || (token.size() > 1 && token[0] == '0')) 
            return -1;

        int32 value = 0;
        constexpr int32 max = std::numeric_limits<int32>::max();

        for (char c : token) {
            if (c < '0' || c > '9') 
                return -1;

            const int digit = c - '0';
            if (value > (max - digit) / 10) 
                return -1;

            value = value * 10 + digit;
        }
        return (value <= max) ? value : -1;
    }

    /**
    * @Function ReplacePlaceholders
    * @brief Remplace récursivement les placeholders par les valeurs
    * @param anyStyle Autorise le mix de styles dans la chaîne
    * @param style Style de placeholder principal
    * @param fmt Chaîne de format originale
    * @param values Valeurs à injecter
    * @return Chaîne formatée avec valeurs insérées
    * @throws std::runtime_error Si index de placeholder invalide
    * 
    * @Details :
    * - Gère l'échappement des délimiteurs avec '/'
    * - Supporte l'imbrication récursive des placeholders
    * - Détection automatique de style si anyStyle=true
    */
    std::string FormatterSystem::ReplacePlaceholders(
        bool anyStyle, 
        Style style, 
        const std::string& fmt, 
        const std::vector<std::string>& values
    ) {
        std::string format = fmt;
        usize searchStart = 0;
    
        while (true) {
            Style currentStyle = (anyStyle) ? DetectStyle(format) : style;
            const auto [open, close] = GetDelimiters(currentStyle);
    
            usize openPos = format.find(open, searchStart);
            if (openPos == std::string::npos) break;
    
            // Gestion des délimiteurs échappés
            if (openPos > 0 && format[openPos - 1] == '/') {
                format.replace(openPos - 1, 1, "");
                searchStart = openPos + 1;
                continue;
            }
    
            usize closePos = format.find(close, openPos + 1);
            if (closePos == std::string::npos) {
                searchStart = openPos + 1;
                continue;
            }
    
            std::string content = format.substr(openPos + 1, closePos - openPos - 1);
            int32 number = ExtractIndex(content);
    
            if (number >= 0 && number < static_cast<int32>(values.size())) {
                format.replace(openPos, closePos - openPos + 1, values[number]);
                searchStart = openPos + values[number].size();
            } else if (content.find(open) == std::string::npos && content.find(close) == std::string::npos) {
                searchStart = closePos + 1;
            } else {
                // Traitement récursif pour l'imbrication
                std::string resolved = ReplacePlaceholders(anyStyle, style, content, values);
                format.replace(openPos + 1, closePos - openPos - 1, resolved);
                searchStart = closePos + (resolved.size() - content.size()) + 1;
            }
        }
    
        return format;
    }

    /**
    * @Function GetDelimiters
    * @brief Retourne la paire de délimiteurs pour un style donné
    * @param style Style de délimiteur à utiliser
    * @return Paire de caractères [ouvrant, fermant]
    */
    std::pair<char, char> FormatterSystem::GetDelimiters(Style style) {
        switch (style) {
            case Style::Square:    return {'[', ']'};
            case Style::Angle:     return {'<', '>'};
            case Style::Parenthese:return {'(', ')'};
            default:               return {'{', '}'};
        }
    }

    /**
    * @Function DetectStyle
    * @brief Détecte le style dominant dans la chaîne
    * @param fmt Chaîne à analyser
    * @return Style détecté (Curly par défaut)
    * 
    * @Details :
    * - Analyse la première occurrence de chaque type de délimiteur
    * - Retourne le style correspondant au délimiteur le plus tôt
    */
    FormatterSystem::Style FormatterSystem::DetectStyle(const std::string& fmt) {
        size_t positions[] = {
            fmt.find('<'),  // Angle
            fmt.find('['),  // Square
            fmt.find('('),  // Parenthese
            fmt.find('{')   // Curly
        };
    
        size_t minPos = std::string::npos;
        Style detectedStyle = Style::Curly;
    
        for (int i = 0; i < 4; ++i) {
            if (positions[i] != std::string::npos && positions[i] < minPos) {
                minPos = positions[i];
                detectedStyle = static_cast<Style>(i);
            }
        }
    
        return detectedStyle;
    }

    /**
    * @Function Format
    * @brief Méthode principale de formatage avec gestion d'erreurs
    * @param formatStr Chaîne de format avec placeholders
    * @param args Valeurs à injecter
    * @param style Style de délimiteur principal
    * @param anyStyle Autorise les styles multiples
    * @return Chaîne formatée
    * @throws std::runtime_error Si erreur pendant le remplacement
    */
    std::string FormatterSystem::Format(
        const std::string& formatStr,
        const std::vector<std::string>& args,
        Style style,
        bool anyStyle
    ) {
        try {
            return ReplacePlaceholders(anyStyle, style, formatStr, args);
        } catch (const std::exception& e) {
            throw std::runtime_error("Erreur de formatage : " + std::string(e.what()));
        }
    }

    /**
    * @Function FormatAny
    * @brief Formatage avec détection automatique de style
    * @param formatStr Chaîne à formater
    * @param args Valeurs à injecter
    * @param style Style de fallback
    * @return Chaîne formatée
    * @throws std::runtime_error Si erreur pendant le remplacement
    */
    std::string FormatterSystem::FormatAny(
        const std::string& formatStr,
        const std::vector<std::string>& args,
        Style style
    ) {
        try {
            return ReplacePlaceholders(true, style, formatStr, args);
        } catch (const std::exception& e) {
            throw std::runtime_error("Erreur de formatage : " + std::string(e.what()));
        }
    }

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.