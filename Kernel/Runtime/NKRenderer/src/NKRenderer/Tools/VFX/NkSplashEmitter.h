#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSplashEmitter.h — l'ÉMETTEUR qui consomme les contacts du fluide et fait
// naître des gouttes (2026-09-06). ROADMAP_PRODUITS.md §6.6, palier 1.
//
// Ce fichier est un ADAPTATEUR, volontairement mince : la LOI (combien de
// gouttes, dans quelle direction) est dans NKMath/NkSplashLaw.h, avec ses
// témoins, dans le module le moins cher à construire. Ici il ne reste que la
// traduction NkSplashDrop -> NkParticleBirth et la comptabilité de ce qui est
// PERDU -- parce qu'il y a trois endroits où une goutte peut disparaître, et
// qu'aucun ne doit être silencieux :
//   1. la file de contacts a débordé          -> NkContactQueue::Dropped()
//   2. le plafond de gouttes de CE pas est atteint -> Stats::dropsRefused
//   3. le stockage de particules est plein     -> c'est NkParticleStoreCPU::Spawn
//      qui le dit ; l'émetteur rapporte ce qu'il a DEMANDÉ, pas ce qui est né.
//
// Le stockage n'est pas touché ici : `Consume` remplit un tableau de naissances,
// l'appelant le passe à `store.Spawn`. C'est le contrat de NkParticleStore.h
// (« qui décide QUI naît : toujours le CPU »), et ça rend l'émetteur testable
// sans NkIDevice.
// =============================================================================
#include "NkParticleStore.h"
#include "NKMath/NkContactEvent.h"
#include "NKMath/NkSplashLaw.h"

namespace nkentseu {
	namespace renderer {

		class NkSplashEmitter {
			public:
				math::NkSplashParams law;
				// Plafond de gouttes créées par appel à Consume. Un plafond DIT vaut mieux
				// qu'un stockage qui refuse en silence.
				uint32 maxDropsPerStep = 4096u;

				struct Stats {
						uint32 events = 0;		  // événements lus dans la file
						uint32 eventsAbove = 0;	  // ceux qui passent le seuil de vitesse de la loi
						uint32 drops = 0;		  // naissances écrites
						uint32 dropsRefused = 0;  // gouttes que la loi voulait et que le plafond a refusées
						uint32 contactsDropped = 0; // ce que la FILE avait déjà perdu (débordement)
						uint32 contactsTotal = 0;   // la population : contacts proposés à la file
				};

				// Lit la file et écrit des naissances dans `out` (vidé au préalable).
				// Rend le nombre de naissances écrites ; `Last()` dit tout le reste.
				uint32 Consume(const math::NkContactQueue &queue, NkVector<NkParticleBirth> &out) {
					mStats = Stats{};
					mStats.contactsDropped = queue.Dropped();
					mStats.contactsTotal = queue.Total();
					out.Clear();
					math::NkSplashDrop buf[64];
					const uint32 perEvent = law.maxDropsPerEvent < 64u ? law.maxDropsPerEvent : 64u;
					for (uint32 i = 0; i < queue.Count(); ++i) {
						const math::NkContactEvent &e = queue[i];
						++mStats.events;
						const float32 want = math::NkSplashDropCountReal(law, e.normalSpeed);
						if (want <= 0.f)
							continue;
						++mStats.eventsAbove;
						const uint32 k = math::NkSplashEmit(law, e, buf, perEvent);
						for (uint32 d = 0; d < k; ++d) {
							if (mStats.drops >= maxDropsPerStep) {
								mStats.dropsRefused += (k - d);
								return mStats.drops;
							}
							NkParticleBirth b;
							b.pos = buf[d].position;
							b.vel = buf[d].velocity;
							b.life = buf[d].life;
							b.rotation = 0.f;
							b.rotSpeed = 0.f;
							out.PushBack(b);
							++mStats.drops;
						}
					}
					return mStats.drops;
				}

				const Stats &Last() const noexcept {
					return mStats;
				}

			private:
				Stats mStats;
		};

	} // namespace renderer
} // namespace nkentseu
