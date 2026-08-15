#pragma once
// =============================================================================
// NkSystemClock.h — L'heure MURALE : celle des horodatages et des noms de
//                   fichiers, par opposition à l'horloge monotone de NkChrono.
//
// POURQUOI CE FICHIER EXISTE (2026-08-16) :
//   NKTime mesurait des DURÉES (`NkChrono`, monotone) et savait REPRÉSENTER des
//   dates (`NkDate`, `NkTimes`) — mais rien ne donnait **l'heure qu'il est**.
//   Résultat : quatre endroits du dépôt appelaient `std::time` / `localtime`
//   chacun dans leur coin — `NkStringUtils.cpp`, `NkDirectory.cpp`,
//   `NkUIFileBrowser.cpp`, `NkCameraSystem.cpp`. Le même manque résolu quatre
//   fois localement, avec de la STL, dans un moteur qui n'en veut pas.
//
//   Les en-têtes du module renvoyaient d'ailleurs vers « NkDateTime (module
//   séparé) » — qui n'existe pas. Une promesse d'API sans implémentation, du
//   même genre que les réglages fantômes retirés de NKCamera cette semaine.
//
// LA DISTINCTION QUI COMPTE, et elle n'est pas cosmétique :
//   - `NkChrono` est MONOTONE : il ne recule jamais, il ignore les changements
//     d'heure système, et c'est le seul valable pour MESURER une durée ;
//   - `NkSystemClock` est MURAL : il donne l'heure du calendrier, il PEUT
//     reculer (fuseau, synchronisation réseau, heure d'été), et c'est le seul
//     valable pour DATER un événement ou nommer un fichier.
//   Mesurer une durée avec l'horloge murale est un défaut classique : un
//   ajustement d'horloge pendant la mesure rend une durée négative.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#ifndef __NKENTSEU_NKSYSTEMCLOCK_H__
#define __NKENTSEU_NKSYSTEMCLOCK_H__

#include "NKCore/NkTypes.h"
#include "NKTime/NkTimeApi.h"

namespace nkentseu {

	/// Instant du calendrier, décomposé. Champs déjà séparés parce que c'est
	/// sous cette forme qu'on s'en sert : horodater, nommer, afficher.
	struct NkSystemDateTime {
			int32 year = 0;        ///< année pleine (2026)
			int32 month = 0;       ///< 1..12
			int32 day = 0;         ///< 1..31
			int32 hour = 0;        ///< 0..23
			int32 minute = 0;      ///< 0..59
			int32 second = 0;      ///< 0..59
			int32 millisecond = 0; ///< 0..999

			/// Vrai si la lecture a abouti. Un `false` doit se voir : dater un
			/// fichier avec une date nulle produit des noms qui se collisionnent
			/// en silence.
			bool valid = false;
	};

	class NKENTSEU_TIME_CLASS_EXPORT NkSystemClock {
		public:
			/// Secondes écoulées depuis le 1er janvier 1970, UTC.
			/// Pour horodater, comparer, ou semer un générateur.
			static int64 UnixSeconds() noexcept;

			/// Millisecondes depuis la même époque, quand la seconde est trop
			/// grossière (séquences d'images, journaux).
			static int64 UnixMilliseconds() noexcept;

			/// Heure LOCALE décomposée — celle qu'attend un utilisateur qui lit
			/// un nom de fichier.
			static NkSystemDateTime LocalNow() noexcept;

			/// Heure UTC décomposée — celle qu'il faut dès que deux machines
			/// comparent leurs traces.
			static NkSystemDateTime UtcNow() noexcept;

			/// Horodatage compact `AAAAMMJJ_HHMMSS`, écrit dans `out`.
			/// Rend false si le tampon est trop petit ou l'heure illisible ;
			/// dans ce cas `out` reçoit une chaîne vide plutôt qu'un contenu
			/// partiel — une date à moitié écrite est pire qu'aucune.
			/// `outSize` doit valoir au moins 16 octets.
			static bool StampCompact(char *out, usize outSize, bool utc = false) noexcept;
	};

} // namespace nkentseu

#endif // __NKENTSEU_NKSYSTEMCLOCK_H__
