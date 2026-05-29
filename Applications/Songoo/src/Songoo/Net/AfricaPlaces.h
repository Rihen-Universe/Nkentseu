#pragma once
// =============================================================================
// AfricaPlaces.h
// -----------------------------------------------------------------------------
// Data table compile-time des 54 pays de l'Union Africaine + leurs principales
// villes (capitales, grandes villes, villes notables). Utilise pour generer
// des identifiants memorables de session multijoueur Pong :
//
//     "Cameroun/Douala"   "Senegal/Dakar"   "Egypte/Le Caire"
//
// L'objectif est que chaque joueur soit identifiable visuellement dans le
// lobby reseau (cf [[pong_multijoueur_lobby_pays_ville]]). Combinaison
// aleatoire generee au demarrage de l'app via NkRandom (cf
// [[feedback_nk_time_random]]).
//
// Format compact : table statique de structs { country, cities[], count }.
// L'alourdissement de l'exe est negligeable (~5 KB de chaines, accepte par
// l'user). Aucune allocation dynamique, pas de chargement de fichier.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu
{
    namespace songoo
    {
        namespace africa
        {

            // ─────────────────────────────────────────────────────────────────
            // Une entree pays = nom + tableau de noms de villes (UTF-8, sans
            // accents ASCII-safe pour eviter les soucis d'encodage sur les
            // fonts atlas ASCII-only). Le caller fait le pick aleatoire.
            // ─────────────────────────────────────────────────────────────────
            struct CountryEntry
            {
                const char*  country;      ///< Nom du pays (UTF-8, ASCII-only ici)
                const char* const* cities; ///< Tableau ptr de villes (longueur cityCount)
                int                cityCount;
            };

            /// Nombre de pays dans la table (54 UA + Sahara occidental = 55, etc.).
            /// Garanti >= 1 a compile time via static_assert dans le .cpp.
            int          CountryCount() noexcept;

            /// Acces a la i-eme entree (0 <= i < CountryCount()).
            /// Pointeur valable pour toute la duree du processus.
            const CountryEntry& GetCountry(int idx) noexcept;

            // ─────────────────────────────────────────────────────────────────
            // Generation d'un identifiant aleatoire "Pays/Ville" via NkRandom.
            // outBuf doit pouvoir contenir au moins 64 octets (terminator inclus).
            // Format : "Pays/Ville" sans accents. Retourne le nombre de caracteres
            // ecrits (hors terminator), 0 en cas d'erreur (buf trop petit).
            //
            // Note : la randomisation utilise une seed time-based + machine-id
            // (cf NkRandom default), donc deux clients sur le meme reseau ont
            // 99.99% chance d'obtenir des identifiants differents.
            // ─────────────────────────────────────────────────────────────────
            int PickRandomPlace(char* outBuf, int bufSize) noexcept;

            // ─────────────────────────────────────────────────────────────────
            // Variante avec separation pays/ville pour usage UI (afficher en
            // 2 lignes par ex). Ecrit "Pays" dans countryBuf et "Ville" dans
            // cityBuf. Retourne true en cas de succes.
            // ─────────────────────────────────────────────────────────────────
            bool PickRandomCountryCity(char* countryBuf, int countryBufSize,
                                       char* cityBuf, int cityBufSize) noexcept;

            // ─────────────────────────────────────────────────────────────────
            // Version COMPLETE : pays + ville + code a 9 chiffres aleatoire
            // (range [0..999999999], format "%09u" zero-padded). Avec ~550
            // combinaisons pays/ville et 1 milliard de codes, la probabilite
            // de collision dans une session multi devient negligeable (Pong
            // ne montera jamais au million de joueurs simultanes). codeBuf
            // doit pouvoir contenir au moins 10 chars + null.
            //
            // Identifiant affichable construit par le caller : "Pays/Ville-XXXXXXXXX".
            // ─────────────────────────────────────────────────────────────────
            bool PickRandomCountryCityCode(char* countryBuf, int countryBufSize,
                                           char* cityBuf,    int cityBufSize,
                                           char* codeBuf,    int codeBufSize) noexcept;

        } // namespace africa
    }     // namespace songoo
} // namespace nkentseu
