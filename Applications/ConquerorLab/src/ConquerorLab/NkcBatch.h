#pragma once
// =============================================================================
// NkcBatch — campagne IA contre IA sur plusieurs threads.
//
// C'EST L'INSTRUMENT, PAS UN GADGET
// ---------------------------------
// Les regles de Conqueror ne se tranchent pas au debat : « toute valeur est un
// parametre, une regle est refutee ou confirmee par simulation de masse »
// (REGLES §1). Ce fichier produit les dix mille parties qui repondent.
//
// TROIS PRECAUTIONS QUI CHANGENT LA VALEUR DES CHIFFRES
//
//  1. Chaque worker possede SA PROPRE instance de moteur, clonee sur le thread
//     principal AVANT le depart. Aucun etat partage, donc aucune section
//     critique, donc une mise a l'echelle lineaire — et surtout aucun risque
//     qu'un reglage bouge en cours de campagne et melange deux populations.
//
//  2. LES COTES SONT INVERSEES une partie sur deux. Sans cela, tout ecart
//     mesure melange « cette IA est meilleure » et « le premier joueur est
//     avantage » — et REGLES §15 pose justement l'avantage au premier joueur
//     comme LA question du palier 0. On mesure les deux separement.
//
//  3. LES PARTIES COUPEES PAR max_tours SONT COMPTEES A PART. « Un taux non nul
//     signale une pathologie des regles, pas un reglage a monter » (REGLES
//     §12.3). Les noyer dans les nuls reviendrait a effacer le symptome.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorAIABI.h"
#include "ConquerorLab/NkcSession.h"

#include "NKCore/NkAtomic.h"
#include "NKThreading/NkThread.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace conqueror {

		inline constexpr int32 kBatchMaxThreads = 16;
		inline constexpr int32 kLenBuckets		= 16;   ///< histogramme de duree, par tranche de 10 coups
		inline constexpr uint32 kBatchHardCap	= 20000u; ///< garde-fou d'anti-boucle, jamais une regle

		struct NkcBatchConfig {
				uint32		  games		  = 200;
				uint8		  playerCount = 2;
				uint64		  seed		  = 1ull;
				uint32		  threads	  = 4;
				uint32		  budgetMs	  = 40;
				bool		  swapSides	  = true;
				int32		  aiModule[kMaxPlayers] = {0, 0, 0, 0};
				NkcDifficulty diff[kMaxPlayers] = {NkcDifficulty::Normal, NkcDifficulty::Normal,
												   NkcDifficulty::Normal, NkcDifficulty::Normal};
		};

		/// Compteurs partages. Tout est atomique : lus a chaque frame par
		/// l'interface pendant que les workers ecrivent.
		struct NkcBatchStats {
				NkAtomicUInt32 played{0};
				NkAtomicUInt32 wins[kMaxPlayers];
				NkAtomicUInt32 draws{0};
				NkAtomicUInt32 guardStops{0};	///< coupees par max_tours
				NkAtomicUInt32 illegal{0};		///< coups illegaux proposes par une IA
				NkAtomicUInt32 totalMoves{0};
				NkAtomicUInt32 action[5];		///< usage par NkcMoveKind
				NkAtomicUInt32 lens[kLenBuckets];
				NkAtomicUInt32 firstPlayerWins{0};  ///< victoires du joueur au trait initial

				void Reset() noexcept {
					played.Store(0);
					draws.Store(0);
					guardStops.Store(0);
					illegal.Store(0);
					totalMoves.Store(0);
					firstPlayerWins.Store(0);
					for (uint32 i = 0; i < kMaxPlayers; ++i) wins[i].Store(0);
					for (int32 i = 0; i < 5; ++i) action[i].Store(0);
					for (int32 i = 0; i < kLenBuckets; ++i) lens[i].Store(0);
				}
		};

		// =====================================================================
		class NkcBatch {
			public:
				~NkcBatch() { Cancel(); Join(); Release(); }

				bool Running() const noexcept { return mRunning; }
				const NkcBatchStats &Stats() const noexcept { return mStats; }
				const NkcBatchConfig &Config() const noexcept { return mCfg; }
				uint32 Target() const noexcept { return mCfg.games; }

				float32 Progress() const noexcept {
					if (mCfg.games == 0) return 1.f;
					const float32 p = static_cast<float32>(mStats.played.Load()) /
									  static_cast<float32>(mCfg.games);
					return p > 1.f ? 1.f : p;
				}

				/// Prepare et lance. `templateInst` sert de modele : plateau et
				/// parametres sont recopies dans chaque instance de worker, ici, sur
				/// le thread appelant. Renvoie false si rien n'est jouable.
				bool Start(const NkcRulesVTable &rvt, NkcRules templateInst, NkcModuleHost *host,
						   const NkcBatchConfig &cfg) noexcept {
					if (mRunning) return false;
					if (!host || !templateInst || !rvt.Create || !rvt.CreateState) return false;

					Cancel();
					Join();
					Release();

					mCfg = cfg;
					if (mCfg.games == 0) mCfg.games = 1;
					if (mCfg.threads < 1) mCfg.threads = 1;
					if (mCfg.threads > static_cast<uint32>(kBatchMaxThreads))
						mCfg.threads = static_cast<uint32>(kBatchMaxThreads);
					if (mCfg.playerCount < 2) mCfg.playerCount = 2;

					mRvt = rvt;
					mStats.Reset();
					mNext.Store(0);
					mCancel.Store(0);
					mAlive.Store(0);
					mLog.Clear();

					// Tables d'IA copiees UNE FOIS : le NkVector du catalogue peut
					// realloouer, et un rechargement a chaud fermerait la DLL.
					for (uint32 p = 0; p < kMaxPlayers; ++p) {
						const int32 idx = mCfg.aiModule[p];
						mHasAi[p]		= false;
						if (idx < 0 || static_cast<usize>(idx) >= host->Ais().Size()) continue;
						if (!host->Ais()[static_cast<usize>(idx)].Usable()) continue;
						mAvt[p]	  = host->Ais()[static_cast<usize>(idx)].factory.vtable;
						mHasAi[p] = mAvt[p].Create != nullptr && mAvt[p].ChooseMove != nullptr;
					}
					for (uint32 p = 0; p < mCfg.playerCount; ++p)
						if (!mHasAi[p]) {
							mLog = "Chaque joueur doit etre pilote par une IA chargee.";
							return false;
						}

					// Instances de worker, creees ICI (thread principal) pour que le
					// modele ne soit jamais lu depuis deux threads a la fois.
					for (uint32 w = 0; w < mCfg.threads; ++w) {
						Worker &k = mWorkers[w];
						k.owner	  = this;
						k.rules	  = mRvt.Create();
						if (!k.rules) { mLog = "Creation d'instance de regles refusee."; Release(); return false; }
						NkcSyncRules(mRvt, k.rules, templateInst);
						k.state = mRvt.CreateState(k.rules);
						k.moves.Resize(kMoveCap);
						for (uint32 p = 0; p < mCfg.playerCount; ++p)
							k.ai[p] = mAvt[p].Create();
					}

					mRunning = true;
					mAlive.Store(static_cast<int32>(mCfg.threads));
					for (uint32 w = 0; w < mCfg.threads; ++w) {
						Worker *k = &mWorkers[w];
						mThreads[w] = threading::NkThread([k](void *) { k->owner->Run(*k); });
						// Creation refusee par l'OS : on decompte tout de suite, sinon
						// `mAlive` n'atteint jamais zero et la campagne reste « en
						// cours » pour toujours.
						if (mThreads[w].Joinable()) mThreads[w].SetName("nkc-batch");
						else						mAlive.FetchAdd(-1);
					}
					return true;
				}

				void Cancel() noexcept { mCancel.Store(1); }

				/// A appeler chaque frame : recolte les threads quand tout est fini.
				/// Non bloquant tant que la campagne tourne.
				void Poll() noexcept {
					if (!mRunning) return;
					if (mAlive.Load() > 0) return;
					Join();
					mRunning = false;
				}

				const NkString &Message() const noexcept { return mLog; }

			private:
				struct Worker {
						NkcBatch		 *owner = nullptr;
						NkcRules		  rules = nullptr;
						NkcState		  state = nullptr;
						NkcAI			  ai[kMaxPlayers] = {};
						NkVector<NkcMove> moves;
				};

				void Join() noexcept {
					for (int32 w = 0; w < kBatchMaxThreads; ++w)
						if (mThreads[w].Joinable()) mThreads[w].Join();
				}

				void Release() noexcept {
					for (int32 w = 0; w < kBatchMaxThreads; ++w) {
						Worker &k = mWorkers[w];
						for (uint32 p = 0; p < kMaxPlayers; ++p) {
							if (k.ai[p] && mAvt[p].Destroy) mAvt[p].Destroy(k.ai[p]);
							k.ai[p] = nullptr;
						}
						if (k.rules) {
							if (k.state && mRvt.DestroyState) mRvt.DestroyState(k.rules, k.state);
							if (mRvt.Destroy) mRvt.Destroy(k.rules);
						}
						k.rules = nullptr;
						k.state = nullptr;
					}
				}

				// -------------------------------------------------------------
				void Run(Worker &k) noexcept {
					for (;;) {
						if (mCancel.Load() != 0) break;
						const uint32 g = static_cast<uint32>(mNext.FetchAdd(1));
						if (g >= mCfg.games) break;
						PlayOne(k, g);
					}
					mAlive.FetchAdd(-1);
				}

				void PlayOne(Worker &k, uint32 gameIndex) noexcept {
					// Inversion des cotes : la partie 2n et la partie 2n+1 partagent
					// la meme graine et n'echangent QUE les sieges. C'est cet
					// appariement qui isole l'avantage de position.
					const bool swapped = mCfg.swapSides && (gameIndex & 1u) != 0u;
					const uint64 seed  = mCfg.seed + static_cast<uint64>(gameIndex >> 1);

					// Un Setup refuse doit quand meme etre COMPTE : sinon la barre de
					// progression n'atteint jamais 100 % et l'on croit a un blocage.
					if (!mRvt.Setup(k.rules, k.state, mCfg.playerCount, seed ? seed : 1ull)) {
						mStats.played.FetchAdd(1);
						return;
					}

					for (uint32 p = 0; p < mCfg.playerCount; ++p) {
						const uint32 src = SeatToConfig(p, swapped);
						NkcAIConfig	 c;
						c.difficulty = mCfg.diff[src];
						c.budgetMs	 = mCfg.budgetMs;
						c.seed		 = seed ^ (static_cast<uint64>(p + 1) * 0x9E3779B97F4A7C15ull);
						if (mAvt[src].Configure) mAvt[src].Configure(k.ai[src], &c);
						if (mAvt[src].Reset) mAvt[src].Reset(k.ai[src]);
					}

					uint32 plies	= 0;
					bool   hitGuard = true;
					for (; plies < kBatchHardCap; ++plies) {
						if (mCancel.Load() != 0) return;
						if (mRvt.IsFinished(k.rules, k.state)) { hitGuard = false; break; }

						NkcStateView v;
						mRvt.GetView(k.rules, k.state, &v);
						const uint32 seat = v.current < mCfg.playerCount ? v.current : 0u;
						const uint32 who  = SeatToConfig(seat, swapped);

						NkcMove chosen;
						std::memset(&chosen, 0, sizeof(chosen));
						bool have = false;

						NkcAIResult r;
						std::memset(&r, 0, sizeof(r));
						if (mAvt[who].ChooseMove(k.ai[who], &mRvt, k.rules, k.state, &r)) {
							if (!mRvt.IsLegalMove || mRvt.IsLegalMove(k.rules, k.state, &r.move)) {
								chosen = r.move;
								have   = true;
							} else {
								mStats.illegal.FetchAdd(1);
							}
						}
						if (!have) {
							// Repli : le premier coup legal. Une IA muette ne doit pas
							// geler la campagne, mais l'incident est compte.
							const uint32 n = mRvt.GenerateLegalMoves(k.rules, k.state, k.moves.Data(), kMoveCap);
							if (n == 0) { hitGuard = false; break; }
							chosen = k.moves[0];
						}

						const uint32 kind = static_cast<uint32>(chosen.kind);
						if (kind < 5) mStats.action[kind].FetchAdd(1);
						if (!mRvt.ApplyMove(k.rules, k.state, &chosen, nullptr, nullptr)) {
							hitGuard = false;
							break;
						}
						if (mAvt[who].OnMovePlayed) mAvt[who].OnMovePlayed(k.ai[who], &chosen);
					}

					// Verdict. `winner` est un SIEGE : on le ramene au camp, sinon
					// l'inversion des cotes fausserait tout le decompte.
					const int32 winnerSeat = mRvt.GetWinner ? mRvt.GetWinner(k.rules, k.state) : -1;
					if (winnerSeat >= 0) {
						const uint32 camp = SeatToConfig(static_cast<uint32>(winnerSeat), swapped);
						if (camp < kMaxPlayers) mStats.wins[camp].FetchAdd(1);
						if (winnerSeat == 0) mStats.firstPlayerWins.FetchAdd(1);
					} else {
						mStats.draws.FetchAdd(1);
					}
					if (hitGuard) mStats.guardStops.FetchAdd(1);

					mStats.totalMoves.FetchAdd(plies);
					int32 bucket = static_cast<int32>(plies / 10u);
					if (bucket >= kLenBuckets) bucket = kLenBuckets - 1;
					mStats.lens[bucket].FetchAdd(1);
					mStats.played.FetchAdd(1);
				}

				/// Siege sur le plateau -> camp de la configuration. A 2 joueurs
				/// l'inversion echange 0 et 1 ; au-dela on ne l'applique pas (une
				/// permutation cyclique a 3-4 joueurs ne s'apparie plus deux a deux).
				uint32 SeatToConfig(uint32 seat, bool swapped) const noexcept {
					if (!swapped || mCfg.playerCount != 2) return seat;
					return seat == 0 ? 1u : 0u;
				}

			private:
				NkcRulesVTable mRvt{};
				NkcAIVTable	   mAvt[kMaxPlayers]{};
				bool		   mHasAi[kMaxPlayers] = {};

				NkcBatchConfig mCfg;
				NkcBatchStats  mStats;
				Worker		   mWorkers[kBatchMaxThreads];
				threading::NkThread mThreads[kBatchMaxThreads];

				NkAtomicInt32 mNext{0};
				NkAtomicInt32 mCancel{0};
				NkAtomicInt32 mAlive{0};
				bool		  mRunning = false;
				NkString	  mLog;
		};

	} // namespace conqueror
} // namespace nkentseu
