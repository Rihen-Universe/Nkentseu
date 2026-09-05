#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkTextureCache.h — LE CACHE D'ACTIFS CUITS
//
// Un actif cuit est un DERIVE, comme un `.obj` : il ne va jamais dans le depot,
// jamais a cote de sa source. Il vit dans `Build/Cache/Assets/`, ignore par git,
// et son nom EST son empreinte.
//
// ── L'EMPREINTE PORTE LE CONTENU, PAS LA DATE ────────────────────────────────
// `empreinte = FNV-1a 64 (octets du fichier source) melange a FNV-1a 64 (options
// de cuisson)`. Source modifiee ou option changee -> autre empreinte -> autre
// fichier -> recuisson automatique. Il n'existe donc AUCUN etat « actif perime » :
// un actif perime n'est pas invalide, il est simplement introuvable.
//
// 🔴 Pourquoi le CONTENU et pas l'horodatage : le depot a deja paye
// « un cache qui compare les dates ne voit pas un fichier restaure » (2026-08-22).
// Un `git checkout` d'une texture remet un ancien contenu avec une date NEUVE ;
// un cache par date sert alors l'actif du nouveau contenu pour l'ancien. Le prix
// du contenu est une lecture du fichier (pas un decodage) : ~10 ms pour un PNG de
// 10 Mo, contre ~300 ms de decodage economises.
//
// ── CUISSON PARESSEUSE ───────────────────────────────────────────────────────
// Le premier chargement decode, cuit, ecrit le cache et televerse ; les suivants
// televersent seulement. Aucun geste demande a personne — c'est ce que font
// `Library/` d'Unity et le cache de donnees derivees d'Unreal.
//
// `NK_TEX_CACHE=0` dans l'environnement desactive tout le mecanisme. C'est ce qui
// rend l'avant/apres mesurable sur la MEME application, sans recompiler.
// =============================================================================

#ifndef NKENTSEU_RENDERER_CORE_NKTEXTURECACHE_H
#define NKENTSEU_RENDERER_CORE_NKTEXTURECACHE_H

#include "NKContainers/String/NkString.h"
#include "NKImage/Core/NkTextureOven.h"

namespace nkentseu {
	namespace renderer {

		class NkTextureCache {
			public:
				// Racine du cache. Relative au repertoire courant, comme tout le
				// reste des chemins du moteur.
				// Une seule definition, partagee avec le four : voir
				// `NkTexCacheNommage` dans NKImage/Core/NkTextureOven.h.
				static const char *Racine() noexcept {
					return NkTexCacheNommage::Racine();
				}

				// Le cache est-il actif ? (`NK_TEX_CACHE=0` le coupe.)
				[[nodiscard]] static bool Actif() noexcept;

				// Empreinte du CONTENU du fichier source melangee aux options de
				// cuisson. Rend 0 si le fichier est illisible — l'appelant doit
				// alors se passer du cache, pas inventer une empreinte.
				[[nodiscard]] static nk_uint64 Empreinte(const char *cheminSource,
														 const NkTexOvenReglages &reglages) noexcept;

				// `Build/Cache/Assets/<16 hex>.nktex`
				[[nodiscard]] static NkString Chemin(nk_uint64 empreinte) noexcept;

				[[nodiscard]] static bool Existe(const NkString &chemin) noexcept;

				// Ecrit l'actif cuit dans le cache (cree l'arborescence au besoin).
				// Rend faux et le DIT si l'ecriture echoue — un cache muet qui
				// n'ecrit jamais se confondrait avec un cache toujours froid.
				static bool Ecrire(const NkString &chemin, const nk_uint8 *payload, nk_size taille,
								   const NkString &cheminSource) noexcept;

				// ── Compteurs, pour que la mesure soit possible ───────────────
				// Un cache sature et un cache qui rate ont le meme taux de service
				// (porte du 2026-08-19) : sans ces trois compteurs, on ne saurait
				// pas distinguer « tout en cache » de « cache jamais consulte ».
				[[nodiscard]] static nk_uint32 Touches() noexcept;   // servis par le cache
				[[nodiscard]] static nk_uint32 Manques() noexcept;   // cuits a la volee
				[[nodiscard]] static nk_uint32 Refus() noexcept;     // non cuisinables
				static void RemettreCompteursAZero() noexcept;

				// ── OU PART LE TEMPS, POSTE PAR POSTE ─────────────────────────
				// 🔴 Ces chronometres existent parce qu'un TOTAL NE DESIGNE PERSONNE.
				// Mesure du 2026-09-05 sur `renderdemo --demo=20`, vrai GPU : le cache
				// chaud rendait 1 684 ms contre 1 691 ms sans cache — le gain de
				// decodage, pourtant reel et mesure sur banc CPU (x7 a x10), AVAIT
				// DISPARU dans l'application. Sans separer lecture, decodage et
				// televersement, on ne pouvait qu'en faire des hypotheses.
				[[nodiscard]] static float64 MsLecture() noexcept;       // E/S disque
				[[nodiscard]] static float64 MsDecodage() noexcept;      // codec PNG/JPEG
				[[nodiscard]] static float64 MsTeleversement() noexcept; // vers le GPU
				[[nodiscard]] static nk_uint64 OctetsLus() noexcept;

				static void AjouterMsLecture(float64 ms, nk_uint64 octets) noexcept;
				static void AjouterMsDecodage(float64 ms) noexcept;
				static void AjouterMsTeleversement(float64 ms) noexcept;

				// Incrementes par NkTextureLibrary.
				static void CompterTouche() noexcept;
				static void CompterManque() noexcept;
				static void CompterRefus() noexcept;
		};

	} // namespace renderer
} // namespace nkentseu

#endif // NKENTSEU_RENDERER_CORE_NKTEXTURECACHE_H
