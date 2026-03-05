/**
* @File NkTimeZone.h
* @Description Gestion des fuseaux horaires et conversions UTC/local
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Apache-2.0
*/

#pragma once

#include "NkTimeExport.h"
#include "NkPlatformDetect.h"
#include "NkTypes.h"
#include "NkDate.h"
#include "NkTime.h"
#include "NkTimeSpan.h"
#include <string>

namespace nkentseu {

    /**
    * - NkTimeZone : Représente un fuseau horaire avec règles de DST
    *
    * @Description :
    * Fournit des fonctionnalités de conversion entre temps local et UTC
    * avec prise en compte des changements d'heure saisonniers
    */
    // class NKTIME_API NkTimeZone {
    // public:
    //     /// @Region: Factory methods ----------------------------------------------
    //     static NkTimeZone GetLocal();
    //     static NkTimeZone GetUtc();
    //     static NkTimeZone FromName(const std::string& ianaName);

    //     /// @Region: Conversions --------------------------------------------------
    //     NkTime ToLocal(const NkTime& utcTime) const;
    //     NkTime ToUtc(const NkTime& localTime) const;
    //     NkDate ToLocal(const NkDate& utcDate) const;
    //     NkDate ToUtc(const NkDate& localDate) const;

    //     /// @Region: Informations -------------------------------------------------
    //     std::string GetName() const;
    //     NkTimeSpan GetUtcOffset(const NkDate& date) const;
    //     bool IsDaylightSavingTime(const NkDate& date) const;

    //     /// @Region: Comparaisons -------------------------------------------------
    //     bool operator==(const NkTimeZone& other) const;
    //     bool operator!=(const NkTimeZone& other) const;

    // private:
    //     NkTimeZone(const std::string& name);
    //     std::string mName;
    //     // Membres supplémentaires pour stocker les règles DST
    // };

} // namespace nkentseu (fin du namespace)


// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.
