// =============================================================================
// NkUnkenySon.h — le son d'un jeu 2D, sur NKAudio
//
// A QUOI SERT CE FICHIER
//   Charger des sons (WAV, OGG, MP3, FLAC… ce que NKAudio decode) ou les
//   fabriquer par programme, les jouer — a plat (interface, musique) ou A UNE
//   POSITION du monde : le son d'une caisse qui tombe a gauche de l'ecran
//   sort a gauche, et s'eteint quand elle sort du champ.
//
// POURQUOI IL EXISTE — mesure du 2026-09-29
//   NKAudio etait dans les dependances de chaque jeu Unkeny et d'aucun fichier
//   d'Unkeny. Pong et Gemcrush ont chacun ecrit leur gestionnaire (Audio/
//   AudioManager, Ui/NkGemAudio) ; le prochain jeu aurait ecrit le troisieme.
//
// LE COMPOSANT ET LE SYSTEME (comme l'animation)
//   NkSource2D ne porte que des DONNEES : quel son, a quel volume, faut-il le
//   jouer. NkSons2D::Avancer(scene) fait le travail : il lance ce qui doit
//   l'etre, place chaque voix a gauche ou a droite selon la camera, baisse
//   celles qui s'eloignent, et COUPE celles dont l'entite a disparu (sans
//   quoi une boucle de moteur continuerait apres la destruction du vehicule).
//
// ⚠️ SANS CARTE SON, TOUT RESTE APPELABLE
//   Demarrer() peut echouer (pas de peripherique, serveur, banc). Le jeu
//   CONTINUE : Jouer rend 0, Avancer calcule quand meme pan et gain. Un jeu
//   qui refuse de demarrer faute de son est un jeu casse (NkGemAudio.h le
//   disait deja).
//
// ⚠️ LA SPATIALISATION EST CELLE D'UN JEU 2D, pas de NKAudio 3D
//   NKAudio sait faire du HRTF et du Doppler. Un jeu 2D veut autre chose, et
//   de previsible : le panoramique suit la position A L'ECRAN, le volume ne
//   baisse qu'une fois HORS du champ. Un son a l'ecran s'entend toujours.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"
#include "NKECS/NkECSDefines.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace unkeny {

		using math::NkVec2f;
		class NkScene;
		class NkVue2D;

		/// Le composant : un son attache a une entite.
		struct NkSource2D {
				uint32 son = 0;			///< identifiant NkSons2D (0 = aucun)
				float32 volume = 1.f;
				float32 pitch = 1.f;
				/// Distance, en metres HORS du champ, sur laquelle le son s'eteint.
				float32 portee = 8.f;
				bool boucle = false;
				bool auDemarrage = false; ///< se lance seul la premiere fois qu'il est vu
				bool spatial = true;	  ///< false : ni panoramique ni attenuation
				/// Mettre a true pour JOUER une fois ; le systeme le remet a false.
				bool demande = false;
				/// Mettre a true pour ARRETER ; le systeme le remet a false.
				bool arret = false;

				// --- Ecrit par le systeme (a lire, pas a ecrire) ----------------
				uint32 voix = 0;	///< voix NKAudio en cours (0 = silence)
				float32 pan = 0.f;	///< [-1, 1] applique a la derniere trame
				float32 gain = 1.f; ///< [0, 1] attenuation de distance appliquee
				bool lance = false; ///< auDemarrage a deja servi
		};

		class NkSons2D {
			public:
				NkSons2D() = default;
				~NkSons2D();
				NkSons2D(const NkSons2D &) = delete;
				NkSons2D &operator=(const NkSons2D &) = delete;

				/// Demarre NKAudio (si personne ne l'a fait). `sortieNulle` : un
				/// moteur qui mixe sans peripherique — pour les bancs.
				/// false = pas de son ; le reste de l'API reste sans danger.
				bool Demarrer(bool sortieNulle = false);
				void Arreter();
				bool Actif() const noexcept {
					return mActif;
				}

				// --- Les sons -------------------------------------------------
				/// Un fichier (relatif a la racine, comme NkTextures2D). Meme chemin,
				/// meme identifiant. 0 = introuvable.
				uint32 Charger(const char *chemin);
				/// Un son fabrique : `frames` echantillons MONO a `frequence` Hz.
				uint32 Creer(const float32 *mono, usize frames, int32 frequence, const char *nom);
				uint32 Trouver(const char *nom) const noexcept;
				float32 Duree(uint32 son) const noexcept; ///< secondes
				uint32 Nombre() const noexcept {
					return static_cast<uint32>(mSons.Size());
				}
				void PoserRacine(const char *racine);

				// --- Jouer ----------------------------------------------------
				/// A plat. Rend la voix, ou 0 (pas de son, son inconnu).
				uint32 Jouer(uint32 son, float32 volume = 1.f, float32 pitch = 1.f, float32 pan = 0.f, bool boucle = false,
							 const char *bus = "SFX");
				/// A une position du monde, vue par `camera`.
				uint32 JouerA(uint32 son, const NkVec2f &position, const NkVue2D &camera, float32 volume = 1.f,
							  float32 portee = 8.f);
				void Couper(uint32 voix, float32 fondu = 0.f);
				bool Joue(uint32 voix) const;
				void PoserMuet(bool muet) noexcept;
				bool Muet() const noexcept {
					return mMuet;
				}
				float32 volumeGeneral = 1.f; ///< multiplie tout ce qui part d'ici

				// --- Le systeme -----------------------------------------------
				/// Une trame : lance, spatialise, coupe. A appeler APRES scene.Pas.
				void Avancer(NkScene &scene);

				/// Pan et gain d'un son a `position` pour cette camera : le
				/// panoramique suit la place A L'ECRAN (0,8 au bord : jamais tout a
				/// gauche, c'est fatigant) ; le gain reste 1 dans le champ et tombe
				/// en carre sur `portee` metres au-dela.
				static void Spatialiser(const NkVec2f &position, const NkVue2D &camera, float32 portee, float32 &pan,
										float32 &gain) noexcept;

			private:
				struct NkSon {
						char nom[160] = {};
						/// audio::AudioSample*, ALLOUE a part. ⚠️ Pas un membre : NKAudio
						/// garde un POINTEUR vers l'echantillon pendant toute la lecture
						/// (Voice::sample), et un NkVector qui grandit deplace ses
						/// elements — chaque son ajoute aurait casse les voix en cours.
						void *echantillon = nullptr;
						bool duChargeur = false; ///< donnees liberees par AudioLoader::Free
				};
				struct NkSuivi {
						uint64 entite = 0; ///< NkEntityId emballe
						uint32 voix = 0;
				};
				uint32 Voix(uint32 son, float32 volume, float32 pitch, float32 pan, bool boucle, const char *bus);
				void Liberer(NkSon &s);

				NkVector<NkSon> mSons;
				NkVector<NkSuivi> mSuivis;
				char mRacine[128] = {};
				bool mRacinePosee = false;
				bool mActif = false;
				bool mProprietaire = false; ///< c'est NOUS qui avons demarre NKAudio
				bool mMuet = false;
		};

	} // namespace unkeny
} // namespace nkentseu
