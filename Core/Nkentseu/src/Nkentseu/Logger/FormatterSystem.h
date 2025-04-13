/**
* @File FormatterSystem.h
* @Description Système avancé de formatage de chaînes avec styles personnalisables
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/
#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>

#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/Platform.h"


namespace nkentseu {

    /**
    * @Function ToString
    * @Description Convertit une valeur générique en chaîne de caractères
    * @Template (typename T) : Type de la valeur à convertir
    * @param (const T&) t : Valeur à convertir
    * @return (std::string) : Représentation textuelle de la valeur
    */
    template <typename T>
    std::string ToString(const T& t) {
        std::stringstream ss;
        ss << t;
        return ss.str();
    }

    /**
    * @Function ToString
    * @Description Spécialisation pour le type booléen
    * @param (bool) value : Valeur booléenne à convertir
    * @return (std::string) : "true" ou "false"
    */
    std::string NKENTSEU_API ToString(bool value);

    /**
    * - FormatterSystem : Système de formatage de chaînes avancé
    *
    * @Description :
    * Classe singleton permettant le formatage de chaînes avec différents styles
    * de placeholder. Supporte les styles curly {}, square [], angle <> et parenthèses ().
    * Gère automatiquement la détection de style et la conversion des arguments.
    */
    class NKENTSEU_API FormatterSystem {
    public:
        /**
        * @Enum Style
        * @Description Styles de délimiteurs supportés
        * @Values :
        * - Curly : Utilise {0}, {1} comme placeholders
        * - Square : Utilise [0], [1] comme placeholders
        * - Angle : Utilise <0>, <1> comme placeholders
        * - Parenthese : Utilise (0), (1) comme placeholders
        */
        enum class Style { Curly, Square, Angle, Parenthese };

        /**
        * @Function Instance
        * @Description Accès à l'instance singleton
        * @return (FormatterSystem&) : Référence à l'instance unique
        */
        static FormatterSystem& Instance();

        /**
        * @Function Format
        * @Description Formatage générique avec style spécifique
        * @Template (typename... Args) : Types des arguments à formater
        * @param (Style) style : Style de délimiteur à utiliser
        * @param (bool) anyStyle : Autorise le mélange de styles si true
        * @param (const std::string&) fmt : Chaîne de format avec placeholders
        * @param (Args&&...) args : Arguments à injecter dans les placeholders
        * @return (std::string) : Chaîne formatée
        * @throws std::runtime_error Si le nombre d'arguments ne correspond pas
        */
        template<typename... Args>
        std::string Format(Style style, bool anyStyle, const std::string& fmt, Args&&... args) {
            std::vector<std::string> params = {AnyToString(args)...};
            return Format(fmt, params, style, anyStyle);
        }

        /**
        * @Function FormatAny
        * @Description Formatage avec détection automatique du style
        * @Template (typename... Args) : Types des arguments à formater
        * @param (const std::string&) fmt : Chaîne de format avec placeholders
        * @param (Args&&...) args : Arguments à injecter
        * @return (std::string) : Chaîne formatée
        */
        template<typename... Args>
        std::string FormatAny(const std::string& fmt, Args&&... args) {
            std::vector<std::string> params = {AnyToString(args)...};
            return FormatAny(fmt, params, Style::Angle, true);
        }

        /**
        * @Function FormatCurly
        * @Description Formatage avec style curly {}
        * @Template (typename... Args) : Types des arguments
        * @param (const std::string&) fmt : Chaîne de format
        * @param (Args&&...) args : Arguments à formater
        * @return (std::string) : Chaîne formatée
        */
        template<typename... Args>
        std::string FormatCurly(const std::string& fmt, Args&&... args) {
            std::vector<std::string> params = {AnyToString(args)...};
            return Format(fmt, params, Style::Curly, false);
        }

        /**
        * @Function FormatSquare
        * @Description Formatage avec style square []
        * @Template (typename... Args) : Types des arguments
        * @param (const std::string&) fmt : Chaîne de format
        * @param (Args&&...) args : Arguments à formater
        * @return (std::string) : Chaîne formatée
        */
        template<typename... Args>
        std::string FormatSquare(const std::string& fmt, Args&&... args) {
            std::vector<std::string> params = {AnyToString(args)...};
            return Format(fmt, params, Style::Square, false);
        }

        /**
        * @Function FormatAngle
        * @Description Formatage avec style angle <>
        * @Template (typename... Args) : Types des arguments
        * @param (const std::string&) fmt : Chaîne de format
        * @param (Args&&...) args : Arguments à formater
        * @return (std::string) : Chaîne formatée
        */
        template<typename... Args>
        std::string FormatAngle(const std::string& fmt, Args&&... args) {
            std::vector<std::string> params = {AnyToString(args)...};
            return Format(fmt, params, Style::Angle, false);
        }

        /**
        * @Function FormatParenthese
        * @Description Formatage avec style parenthèses ()
        * @Template (typename... Args) : Types des arguments
        * @param (const std::string&) fmt : Chaîne de format
        * @param (Args&&...) args : Arguments à formater
        * @return (std::string) : Chaîne formatée
        */
        template<typename... Args>
        std::string FormatParenthese(const std::string& fmt, Args&&... args) {
            std::vector<std::string> params = {AnyToString(args)...};
            return Format(fmt, params, Style::Parenthese, false);
        }

        /**
        * @Function FormatCurly
        * @Description Formatage avec style curly {} et paramètres prétraités
        * @param (const std::string&) fmt : Chaîne de format
        * @param (const std::vector<std::string>&) params : Arguments formatés
        * @return (std::string) : Chaîne formatée
        */
        std::string FormatCurly(const std::string& fmt, const std::vector<std::string>& params) {
            return Format(fmt, params, Style::Curly, false);
        }

        /**
        * @Function FormatSquare
        * @Description Formatage avec style square [] et paramètres prétraités
        * @param (const std::string&) fmt : Chaîne de format
        * @param (const std::vector<std::string>&) params : Arguments formatés
        * @return (std::string) : Chaîne formatée
        */
        std::string FormatSquare(const std::string& fmt, const std::vector<std::string>& params) {
            return Format(fmt, params, Style::Square, false);
        }

        /**
        * @Function FormatAngle
        * @Description Formatage avec style angle <> et paramètres prétraités
        * @param (const std::string&) fmt : Chaîne de format
        * @param (const std::vector<std::string>&) params : Arguments formatés
        * @return (std::string) : Chaîne formatée
        */
        std::string FormatAngle(const std::string& fmt, const std::vector<std::string>& params) {
            return Format(fmt, params, Style::Angle, false);
        }

        /**
        * @Function FormatParenthese
        * @Description Formatage avec style parenthèses et paramètres prétraités
        * @param (const std::string&) fmt : Chaîne de format
        * @param (const std::vector<std::string>&) params : Arguments formatés
        * @return (std::string) : Chaîne formatée
        */
        std::string FormatParenthese(const std::string& fmt, const std::vector<std::string>& params) {
            return Format(fmt, params, Style::Parenthese, false);
        }

        /**
        * @Function Format
        * @Description Méthode principale de formatage
        * @param (const std::string&) formatStr : Chaîne de format
        * @param (const std::vector<std::string>&) args : Arguments convertis
        * @param (Style) style : Style de délimiteur principal
        * @param (bool) anyStyle : Autorise différents styles dans la même chaîne
        * @return (std::string) : Chaîne formatée
        */
        std::string Format(const std::string& formatStr, const std::vector<std::string>& args, Style style, bool anyStyle);

        /**
        * @Function FormatAny
        * @Description Formatage avec détection automatique du style
        * @param (const std::string&) formatStr : Chaîne de format
        * @param (const std::vector<std::string>&) args : Arguments convertis
        * @param (Style) style : Style par défaut
        * @return (std::string) : Chaîne formatée
        */
        std::string FormatAny(const std::string& formatStr, const std::vector<std::string>& args, Style style);

    private:
        /**
        * @Function ReplacePlaceholders
        * @Description Remplace les placeholders par les valeurs
        * @param (bool) anyStyle : Autorise différents styles
        * @param (Style) style : Style principal
        * @param (const std::string&) fmt : Chaîne à traiter
        * @param (const std::vector<std::string>&) params : Valeurs à injecter
        * @return (std::string) : Chaîne remplie
        */
        static std::string ReplacePlaceholders(bool anyStyle, Style style, const std::string& fmt, const std::vector<std::string>& params);

        /**
        * @Function GetDelimiters
        * @Description Retourne les délimiteurs selon le style
        * @param (Style) style : Style à utiliser
        * @return (pair<char, char>) : Paire [ouvrant, fermant]
        */
        static std::pair<char, char> GetDelimiters(Style style);

        /**
        * @Function DetectStyle
        * @Description Détecte le style dominant dans la chaîne
        * @param (const std::string&) fmt : Chaîne à analyser
        * @return (Style) : Style détecté
        */
        static Style DetectStyle(const std::string& fmt);

        /**
        * @Function ExtractIndex
        * @Description Extrait l'index d'un placeholder
        * @param (const std::string&) token : Placeholder à analyser
        * @return (int32) : Index numérique extrait
        */
        static int32 ExtractIndex(const std::string& token);

        /**
        * @Function ProcessFormat
        * @Description Traite le formatage avec les paramètres donnés
        * @param (bool) anyStyle : Mix de styles autorisé
        * @param (std::string) format : Chaîne de format
        * @param (const std::vector<std::string>&) values : Valeurs à injecter
        * @param (const std::string&) open : Délimiteur ouvrant
        * @param (const std::string&) close : Délimiteur fermant
        * @return (std::string) : Chaîne traitée
        */
        static std::string ProcessFormat(bool anyStyle, std::string format, const std::vector<std::string>& values, const std::string& open, const std::string& close);

        /**
        * @Function AnyToString
        * @Description Convertit n'importe quel type en chaîne
        * @Template (typename T) : Type de la valeur
        * @param (const T&) value : Valeur à convertir
        * @return (std::string) : Chaîne convertie
        */
        template<typename T>
        static std::string AnyToString(const T& value) {
            return ToString(value);
        }

        // Constructeurs privés pour singleton
        FormatterSystem() = default;
        FormatterSystem(const FormatterSystem&) = delete;
        FormatterSystem& operator=(const FormatterSystem&) = delete;
    };

} // namespace nkentseu

/**
* @Macro Formatter
* @Description Raccourci pour accéder à l'instance du formatter
* @Example Formatter.FormatCurly("Hello {0}", "World");
*/
#define Formatter ::nkentseu::FormatterSystem::Instance()

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.