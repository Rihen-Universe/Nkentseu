/**
* @File TimeSpan.cpp
* @Description Implémentation de la classe TimeSpan
*/

#include "pch.h"
#include "TimeSpan.h"

namespace nkentseu {

    TimeSpan::TimeSpan(int64 days, int64 hours, int64 minutes, int64 seconds, 
                      int64 milliseconds, int64 nanoseconds) {
        m_TotalNanoseconds = days * 86400LL * 1'000'000'000LL +
                            hours * 3600LL * 1'000'000'000LL +
                            minutes * 60LL * 1'000'000'000LL +
                            seconds * 1'000'000'000LL +
                            milliseconds * 1'000'000LL +
                            nanoseconds;
    }

    // Implémentation des factory methods
    TimeSpan TimeSpan::FromDays(int64 days) noexcept {
        return TimeSpan(days, 0, 0, 0);
    }

    // Implémentation des accesseurs
    int64 TimeSpan::GetDays() const noexcept {
        return m_TotalNanoseconds / (86400LL * 1'000'000'000LL);
    }

    // Implémentation des opérations mathématiques
    TimeSpan& TimeSpan::Add(const TimeSpan& other) noexcept {
        m_TotalNanoseconds += other.m_TotalNanoseconds;
        return *this;
    }

    // Implémentation des opérateurs
    TimeSpan TimeSpan::operator+(const TimeSpan& other) const noexcept {
        return TimeSpan(m_TotalNanoseconds + other.m_TotalNanoseconds);
    }

    // Implémentation des comparaisons
    bool TimeSpan::operator==(const TimeSpan& other) const noexcept {
        return m_TotalNanoseconds == other.m_TotalNanoseconds;
    }

    std::string TimeSpan::ToString() const {
        const int64 nanoseconds = std::abs(m_TotalNanoseconds);
        
        // Calcul des composants temporels
        const int64 days = nanoseconds / (86400LL * 1'000'000'000LL);
        int64 remainder = nanoseconds % (86400LL * 1'000'000'000LL);
        
        const int64 hours = remainder / (3600LL * 1'000'000'000LL);
        remainder %= 3600LL * 1'000'000'000LL;
        
        const int64 minutes = remainder / (60LL * 1'000'000'000LL);
        remainder %= 60LL * 1'000'000'000LL;
        
        const int64 seconds = remainder / 1'000'000'000LL;
        remainder %= 1'000'000'000LL;
        
        const int64 milliseconds = remainder / 1'000'000LL;
        remainder %= 1'000'000LL;
        
        const int64 nanosecondsPart = remainder;
    
        // Formatage avec gestion du signe
        std::ostringstream ss;
        if(m_TotalNanoseconds < 0) ss << "-";
        
        // Construction de la chaîne
        if(days > 0) ss << days << "j ";
        if(hours > 0 || !ss.str().empty()) ss << hours << "h ";
        if(minutes > 0 || !ss.str().empty()) ss << minutes << "m ";
        ss << seconds << "s ";
        
        // Affichage des fractions seulement si nécessaires
        if(milliseconds > 0 || nanosecondsPart > 0) {
            ss << std::setfill('0') 
               << std::setw(3) << milliseconds << "."
               << std::setw(6) << nanosecondsPart << "ms";
        }
    
        std::string result = ss.str();
        
        // Nettoyage des espaces en fin de chaîne
        if(!result.empty() && result.back() == ' ') {
            result.erase(result.size() - 1);
        }
        
        return result;
    }
    
    Time TimeSpan::GetTime() const noexcept {
        constexpr int64 NS_PER_DAY = 86400LL * 1'000'000'000LL;
        int64 adjusted_ns = m_TotalNanoseconds % NS_PER_DAY;
        
        // Gestion des valeurs négatives
        if(adjusted_ns < 0) {
            adjusted_ns += NS_PER_DAY;
        }
        
        // Calcul des composants
        const int32 hours = static_cast<int32>(adjusted_ns / (3600LL * 1'000'000'000LL));
        adjusted_ns %= 3600LL * 1'000'000'000LL;
        
        const int32 minutes = static_cast<int32>(adjusted_ns / (60LL * 1'000'000'000LL));
        adjusted_ns %= 60LL * 1'000'000'000LL;
        
        const int32 seconds = static_cast<int32>(adjusted_ns / 1'000'000'000LL);
        adjusted_ns %= 1'000'000'000LL;
        
        const int32 milliseconds = static_cast<int32>(adjusted_ns / 1'000'000LL);
        adjusted_ns %= 1'000'000LL;
        
        const int32 nanoseconds = static_cast<int32>(adjusted_ns);
    
        return nkentseu::Time(
            hours,
            minutes,
            seconds,
            milliseconds,
            nanoseconds
        );
    }
}  // namespace nkentseu (fin du namespace)


// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.