/**
* @File TimeZone.h
* @Description Gestion des fuseaux horaires et conversions UTC/local
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Apache-2.0
*/

#pragma once

#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/Platform.h"
#include "Date.h"
#include "Time.h"
#include "TimeSpan.h"
#include <string>

namespace nkentseu {

    /**
    * - TimeZone : Représente un fuseau horaire avec règles de DST
    *
    * @Description :
    * Fournit des fonctionnalités de conversion entre temps local et UTC
    * avec prise en compte des changements d'heure saisonniers
    */
    // class NKENTSEU_API TimeZone {
    // public:
    //     /// @Region: Factory methods ----------------------------------------------
    //     static TimeZone GetLocal();
    //     static TimeZone GetUtc();
    //     static TimeZone FromName(const std::string& ianaName);

    //     /// @Region: Conversions --------------------------------------------------
    //     Time ToLocal(const Time& utcTime) const;
    //     Time ToUtc(const Time& localTime) const;
    //     Date ToLocal(const Date& utcDate) const;
    //     Date ToUtc(const Date& localDate) const;

    //     /// @Region: Informations -------------------------------------------------
    //     std::string GetName() const;
    //     TimeSpan GetUtcOffset(const Date& date) const;
    //     bool IsDaylightSavingTime(const Date& date) const;

    //     /// @Region: Comparaisons -------------------------------------------------
    //     bool operator==(const TimeZone& other) const;
    //     bool operator!=(const TimeZone& other) const;

    // private:
    //     TimeZone(const std::string& name);
    //     std::string m_Name;
    //     // Membres supplémentaires pour stocker les règles DST
    // };

} // namespace nkentseu (fin du namespace)


// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.