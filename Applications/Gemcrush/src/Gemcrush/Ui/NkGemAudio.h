// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemAudio.h
// DESCRIPTION: Bruitages et musique de GemCrush — SYNTHÉTISÉS, aucun fichier.
//
//              POURQUOI SYNTHÉTISÉ ET PAS DES .WAV : le jeu n'embarque aucun
//              asset (cf. GemCrush.jenga). Ajouter des fichiers de son
//              réintroduirait toute la chaîne de copie d'assets — dossier
//              `assets/`, `androidassets`, chemins relatifs, lecture par
//              AAssetManager sur Android — pour une poignée de bruitages
//              courts. Les échantillons sont donc CALCULÉS au démarrage
//              (~40 ms, mesuré) et joués par NKAudio comme n'importe quels
//              autres.
//
//              ⚠️ CE QUE C'EST, ET CE QUE CE N'EST PAS : ce sont de vrais
//              bruitages jouables, pas une bande-son composée. La musique est
//              une boucle générative d'ambiance — elle tient sa place, elle ne
//              remplace pas un morceau écrit. `LoadMusicFile()` existe
//              justement pour la remplacer le jour où il y en a un, SANS
//              toucher au reste du jeu.
//
//              POINT D'EXTENSION : un nouveau bruitage s'ajoute dans
//              NkGemSfx + sa recette dans BuildSample() (NkGemAudio.cpp).
//
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_UI_NKGEMAUDIO_H
#define NKENTSEU_GAME_UI_NKGEMAUDIO_H

#include "NKCore/NkTypes.h"
#include "NKAudio/NkAudio.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			/// @brief Les bruitages du jeu. L'ordre fixe l'indice dans le tableau
			///        d'échantillons — ne pas réordonner sans relire BuildSample().
			enum class NkGemSfx : uint8 {
				Swap,	   ///< échange de deux gemmes
				Invalid,   ///< échange refusé (aucun alignement)
				Match,	   ///< alignement résolu (hauteur = rang de cascade)
				Special,   ///< gemme spéciale créée ou déclenchée
				Star,	   ///< étoile de progression gagnée
				Button,	   ///< appui sur un bouton d'interface
				Fail,	   ///< fin de partie perdue
				Win,	   ///< niveau réussi
				Tick,	   ///< dernières secondes du chronomètre
				Count
			};

			// =============================================================
			// NkGemAudio — une instance, tenue par l'application.
			// =============================================================
			class NkGemAudio {
				public:
					NkGemAudio() = default;
					~NkGemAudio();

					NkGemAudio(const NkGemAudio &) = delete;
					NkGemAudio &operator=(const NkGemAudio &) = delete;

					/// @brief Démarre le moteur audio et calcule les échantillons.
					/// @return false si le moteur audio est indisponible — le jeu
					///         DOIT continuer sans son. Un jeu qui refuse de
					///         démarrer faute de carte son est un jeu cassé.
					bool Init();
					void Shutdown();

					bool IsReady() const noexcept {
						return mReady;
					}

					void PlaySfx(NkGemSfx sfx, float32 pitch = 1.f);

					/// @brief Bruitage d'alignement dont la HAUTEUR monte avec le rang
					///        de cascade. C'est ce qui fait entendre une réaction en
					///        chaîne au lieu de répéter le même son.
					void PlayMatch(int32 cascadeRank);

					// -- Musique ------------------------------------------------
					/// @brief Lance la boucle d'ambiance générée.
					void StartMusic();
					void StopMusic();

					/// @brief Remplace la musique générée par un vrai morceau.
					///        Le jour où un fichier existe, c'est la SEULE ligne à
					///        changer côté application.
					/// @return false si le fichier est absent ou illisible — la
					///         boucle générée reste alors en place.
					bool LoadMusicFile(const char *path);

					// -- Réglages -----------------------------------------------
					void SetMusicVolume(float32 volume01);
					void SetSfxVolume(float32 volume01);

					float32 GetMusicVolume() const noexcept {
						return mMusicVolume;
					}

					float32 GetSfxVolume() const noexcept {
						return mSfxVolume;
					}

					/// @brief Coupe tout (bouton haut-parleur du menu).
					void SetMuted(bool muted);

					/// @brief Suspend TOUTES les voix — perte de focus, passage en
					///        arrière-plan. Distinct de SetMuted : le réglage du
					///        joueur n'est pas touché, on reprend exactement où on
					///        en était.
					void Suspend();
					void Resume();

					bool IsSuspended() const noexcept {
						return mSuspended;
					}

					bool IsMuted() const noexcept {
						return mMuted;
					}

				private:
					bool mReady = false;
					bool mMuted = false;
					float32 mMusicVolume = 0.45f;
					float32 mSfxVolume = 0.85f;

					audio::AudioSample mSfxSamples[static_cast<usize>(NkGemSfx::Count)];
					audio::AudioSample mMusicSample;
					audio::AudioHandle mMusicHandle;
					bool mMusicPlaying = false;
					bool mMusicFromFile = false; ///< true : mMusicSample vient d'AudioLoader, pas de la synthèse
					bool mSuspended = false;	 ///< arrière-plan / focus perdu

					void BuildSample(NkGemSfx sfx);
					void BuildMusic();
					void FreeSample(audio::AudioSample &sample, bool fromLoader);
			};

		} // namespace ui
	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_UI_NKGEMAUDIO_H
