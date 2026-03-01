/**
* @File NkTimeSpan.h
* @Description Représente une durée temporelle avec précision nanoseconde
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/

#pragma once

#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/Platform.h"
#include "Nkentseu/Platform/Types.h"
#include "NkTime.h"
#include "NkDate.h"
#include <chrono>
#include <ctime>
#include <string>

namespace nkentseu {

    /**
    * - NkTimeSpan : Durée temporelle signée avec composants détaillés
    *
    * @Description :
    * Représente une intervalle de temps pouvant être positive ou négative.
    * Gère les conversions entre différentes unités et les opérations arithmétiques.
    *
    * @Members :
    * - (int64) mTotalNanoseconds : Durée totale en nanosecondes
    */
    class NKENTSEU_API NkTimeSpan {
    public:
        /// @Region: Constructeurs -------------------------------------------------
        NkTimeSpan() noexcept;
        NkTimeSpan(int64 days, int64 hours, int64 minutes, int64 seconds,
                int64 milliseconds = 0, int64 nanoseconds = 0);
        explicit NkTimeSpan(int64 totalNanoseconds) noexcept {
            mTotalNanoseconds = totalNanoseconds;
        }

        /// @Region: Factory methods -----------------------------------------------
        static NkTimeSpan FromDays(int64 days) noexcept;
        static NkTimeSpan FromHours(int64 hours) noexcept;
        static NkTimeSpan FromMinutes(int64 minutes) noexcept;
        static NkTimeSpan FromSeconds(int64 seconds) noexcept;
        static NkTimeSpan FromMilliseconds(int64 milliseconds) noexcept;
        static NkTimeSpan FromNanoseconds(int64 nanoseconds) noexcept;

        /// @Region: Composants temporels ------------------------------------------
        int64 GetDays() const noexcept;
        int64 GetHours() const noexcept;
        int64 GetMinutes() const noexcept;
        int64 GetSeconds() const noexcept;
        int64 GetMilliseconds() const noexcept;
        int64 GetNanoseconds() const noexcept;

        /// @Region: Opérations mathématiques --------------------------------------
        NkTimeSpan& Add(const NkTimeSpan& other) noexcept;
        NkTimeSpan& Subtract(const NkTimeSpan& other) noexcept;
        NkTimeSpan& Multiply(double factor) noexcept;
        NkTimeSpan& Divide(double divisor) noexcept;

        /// @Region: Opérateurs ----------------------------------------------------
        NkTimeSpan operator+(const NkTimeSpan& other) const noexcept;
        NkTimeSpan operator-(const NkTimeSpan& other) const noexcept;
        NkTimeSpan operator*(double factor) const noexcept;
        NkTimeSpan operator/(double divisor) const noexcept;
        NkTimeSpan& operator+=(const NkTimeSpan& other) noexcept;
        NkTimeSpan& operator-=(const NkTimeSpan& other) noexcept;
        NkTimeSpan& operator*=(double factor) noexcept;
        NkTimeSpan& operator/=(double divisor) noexcept;

        /// @Region: Comparaisons --------------------------------------------------
        bool operator==(const NkTimeSpan& other) const noexcept;
        bool operator!=(const NkTimeSpan& other) const noexcept;
        bool operator<(const NkTimeSpan& other) const noexcept;
        bool operator<=(const NkTimeSpan& other) const noexcept;
        bool operator>(const NkTimeSpan& other) const noexcept;
        bool operator>=(const NkTimeSpan& other) const noexcept;

        NkTime GetTime() const noexcept;
        NkDate GetDate() const noexcept;

        /// @Region: Conversions ---------------------------------------------------
        std::string ToString() const;
        int64 ToNanoseconds() const noexcept { return mTotalNanoseconds; }
        double ToSeconds() const noexcept;

    private:
        int64 mTotalNanoseconds = 0;
    };

}  // namespace nkentseu (fin du namespace)


// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.
