#pragma once
// =============================================================================
// NkcSession — LA partie en cours. C'est le seul objet que les panneaux
// partagent : le plateau l'affiche, le journal le relit, les regles le
// reconfigurent, les joueurs le pilotent.
//
// TROIS DECISIONS QUI STRUCTURENT CE FICHIER
// ------------------------------------------
//
// 1. L'ATELIER NE CONNAIT AUCUNE REGLE. Il ne sait pas ce qu'est une
//    duplication. Pour montrer « quels ennemis vais-je retourner ? » — la
//    lecture tactique centrale du jeu (NOTE_DESIGN §2) — il ne DEDUIT rien : il
//    CLONE l'etat, joue le coup pour de faux, et lit les evenements emis. Le
//    moteur reste seul juge, et l'apercu ne peut pas mentir, meme quand le
//    stagiaire A1 aura change la regle de transformation.
//
// 2. L'IA REFLECHIT SUR SON PROPRE MOTEUR. `ChooseMove` tourne sur un thread
//    worker (contrainte du contrat, ConquerorAIABI.h). Plutot que de partager
//    l'instance de regles avec le thread de rendu — ou l'utilisateur peut a tout
//    moment bouger un parametre — le thread possede SA PROPRE instance,
//    synchronisee avant chaque reflexion (plateau + parametres), et son propre
//    etat, transfere par SerializeState/DeserializeState. Zero verrou, zero
//    course : les deux mondes ne se touchent jamais.
//
// 3. LES VTABLES SONT COPIEES, PAS POINTEES. Elles vivent dans un NkVector du
//    catalogue, qui REALLOUE quand un module apparait, et dans une DLL qu'un
//    rechargement a chaud peut fermer. On copie donc la table a la selection, et
//    on se relie explicitement (`Rebind`) quand le catalogue bouge.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorAIABI.h"
#include "Conqueror/ConquerorGeometry.h"
#include "ConquerorLab/NkcModuleHost.h"
#include "ConquerorLab/NkcParamSchema.h"
#include "ConquerorLab/NkcBoardLibrary.h"

#include "NKCore/NkAtomic.h"
#include "NKThreading/NkThread.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"

#include <cstring>
#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		inline constexpr uint32 kMoveCap	= 2048;	 ///< coups legaux retenus par tour
		inline constexpr int32	kMaxFlips	= 12;	 ///< totems retournes montres par coup
		inline constexpr float32 kEchoTime	= 1.2f;	 ///< duree de l'anneau du dernier coup (s)
		inline constexpr float32 kCascadeTime = 0.9f; ///< duree du bandeau « CASCADE xN » (s)

		inline bool NkcMoveEqual(const NkcMove &a, const NkcMove &b) noexcept {
			return std::memcmp(&a, &b, sizeof(NkcMove)) == 0;
		}

		// =====================================================================
		// Outillage d'instances de regles
		// =====================================================================

		/// Recopie plateau + parametres de `src` vers `dst`. Passe par le contrat
		/// seul : aucune connaissance de la representation interne du module.
		inline void NkcSyncRules(const NkcRulesVTable &vt, NkcRules dst, NkcRules src) noexcept {
			if (!dst || !src) return;
			if (vt.GetBoardJson && vt.LoadBoardJson) {
				if (const char *board = vt.GetBoardJson(src)) vt.LoadBoardJson(dst, board);
			}
			if (!vt.GetParamsSchemaJson || !vt.SetParam || !vt.GetParam) return;
			NkVector<NkcParam> schema;
			if (!NkcParseParamsSchema(vt.GetParamsSchemaJson(src), schema)) return;
			for (usize i = 0; i < schema.Size(); ++i)
				vt.SetParam(dst, schema[i].key.CStr(), vt.GetParam(src, schema[i].key.CStr()));
		}

		// =====================================================================
		// Journal et retours visuels
		// =====================================================================

		struct NkcJournalEntry {
				NkcMove	 move;
				uint64	 hash	 = 0;	///< empreinte APRES le coup (diagnostic de determinisme)
				uint32	 turn	 = 0;
				uint8	 player	 = 0;
				uint8	 byAi	 = 0;
				int16	 flipped = 0;	///< totems retournes par ce coup
		};

		/// Ce que le plateau doit MONTRER du dernier coup. Rempli par les
		/// evenements du moteur, jamais devine.
		struct NkcMoveEcho {
				NkcCoord to;
				NkcCoord from;
				NkcCoord flips[kMaxFlips];
				int32	 flipCount = 0;
				int32	 cascade   = 0;
				uint8	 player	   = 0;
				uint8	 valid	   = 0;
				float32	 age	   = 1000.f;   ///< secondes depuis le coup
		};

		/// Apercu d'un coup candidat : sa destination et les totems qu'il
		/// retournerait. Produit par simulation, pas par deduction.
		struct NkcPreview {
				NkcCoord to;
				NkcCoord flips[kMaxFlips];
				int32	 flipCount = 0;
				uint32	 moveIndex = 0;	 ///< index dans la liste des coups legaux
		};

		struct NkcPlayerCfg {
				int32		  aiModule = -1;   ///< -1 = humain, sinon index dans host.Ais()
				NkcDifficulty diff	   = NkcDifficulty::Normal;
				uint32		  budgetMs = 250;
		};

		// =====================================================================
		// Le travail confie au thread de reflexion. Rien ici n'est touche par le
		// thread de rendu tant que `done` vaut 0.
		// =====================================================================
		struct NkcThinkJob {
				NkcRulesVTable rvt{};
				NkcAIVTable	   avt{};
				NkcRules	   rules  = nullptr;   ///< instance PRIVEE du thread
				NkcState	   state  = nullptr;
				NkcAI		   ai	  = nullptr;
				NkcAIResult	   result{};
				NkAtomicInt32  done{0};
				NkAtomicInt32  ok{0};
		};

		// =====================================================================
		class NkcSession {
			public:
				// ---- cycle de vie ------------------------------------------
				void Init(NkcModuleHost *host, const NkString &boardsDir,
						  const NkString &boardsSeed = NkString()) noexcept {
					mHost = host;
					// REGLAGE D'OUVERTURE : le joueur 0 est HUMAIN, les autres sont
					// des IA, et RIEN NE TOURNE tant qu'on n'a pas appuye sur Lecture.
					//
					// Deux exigences qui se contredisent en apparence :
					//   - « la simulation ne doit pas se lancer seule, c'est l'etudiant
					//     qui la lance »  -> mAutoPlay = false ;
					//   - « l'humain doit pouvoir jouer »                   -> siege 0 humain.
					// Elles tiennent ensemble parce que ce sont DEUX reglages distincts :
					// QUI tient un siege, et SI la partie avance toute seule. Le premier
					// jet les confondait, d'ou l'impression que rien ne marchait.
					//
					// Un clic sur « IA vs IA » (barre du plateau) bascule tous les sieges
					// en mode mesure, quand on veut faire tourner l'instrument.
					const bool hasAi	 = host && !host->Ais().Empty();
					mPlayers[0].aiModule = -1;	 // humain
					for (uint32 p = 1; p < kMaxPlayers; ++p) mPlayers[p].aiModule = hasAi ? 0 : -1;
					mAutoPlay = false;			 // l'etudiant lance la partie

					mBoards.Init(boardsDir, boardsSeed);
					UseRules(0);
					// L'exemple ne peut etre ecrit qu'APRES : il est exporte du moteur.
					if (Ready()) mBoards.EnsureExample(mRulesVt, mRules);
				}

				NkcModuleHost	*Host() const noexcept { return mHost; }
				NkcBoardLibrary &Boards() noexcept { return mBoards; }

				// ---- forme des cellules (PRESENTATION, pas voisinage) -------
				// Trois sources, dans cet ordre de priorite :
				//   1. le module, s'il fournit GetCellShape — traite dans le
				//      projecteur : il a le dernier mot sur SA geometrie ;
				//   2. le choix explicite de l'utilisateur (panneau Regles) ;
				//   3. ce que declare le fichier de plateau.
				NkcCellShape CellShape() const noexcept {
					return mShapeOverride != NkcCellShape::Auto ? mShapeOverride
															   : mBoards.CellShape();
				}
				NkcCellShape ShapeOverride() const noexcept { return mShapeOverride; }
				void SetShapeOverride(NkcCellShape s) noexcept { mShapeOverride = s; }

				// ---- grilles (donnees, pas code) ----------------------------
				/// Charge la grille `idx` de la bibliotheque et repart d'une partie
				/// neuve : changer de plateau invalide toute position en cours.
				bool LoadBoard(usize idx) noexcept {
					if (!Ready()) return false;
					WaitThinking();
					if (!mBoards.LoadInto(mRulesVt, mRules, idx)) return false;
					mBoardIdx = static_cast<int32>(idx);
					mThinkerDirty = true;
					NewGame();
					return true;
				}

				bool ExportBoard(const char *name) noexcept {
					return Ready() && mBoards.Export(mRulesVt, mRules, name);
				}

				// ---- pourquoi rien ne bouge ---------------------------------
				/// L'atelier doit dire ce qu'il attend. Une interface qui reste
				/// immobile sans rien expliquer se lit comme une panne — c'est
				/// exactement ce qui s'est passe avec le reglage « humain » par defaut.
				const char *IdleReason() const noexcept {
					if (!Ready())			return "aucun moteur de regles jouable";
					if (mThinking)			return "l'IA reflechit";
					if (mCursor >= 0)		return "rejeu en pause — revenir a la position vivante";
					if (mView.finished)		return "partie terminee";
					if (IsHumanTurn())		return "au tour du joueur humain — clique un totem";
					if (!mAutoPlay)			return "en pause — Lecture ou Pas a pas";
					if (!mLastStartOk && mStartFail[0]) return mStartFail;
					return "";
				}

				void Shutdown() noexcept {
					WaitThinking();
					DestroyThinker();
					DestroyMain();
					mHost = nullptr;
				}

				// ---- catalogue ---------------------------------------------
				/// Selectionne le moteur de regles n° `idx` et repart d'une partie
				/// neuve. Tout le reste (IA, journal, selection) est remis a plat :
				/// un journal issu d'un autre moteur n'a aucun sens.
				void UseRules(usize idx) noexcept {
					if (!mHost) return;
					const NkVector<NkcRulesEntry> &all = mHost->Rules();
					if (all.Empty()) return;
					if (idx >= all.Size()) idx = 0;
					// Un module casse ne doit pas rendre l'atelier inutilisable :
					// on retombe sur le premier module jouable.
					if (!all[idx].Usable()) {
						usize fallback = idx;
						for (usize i = 0; i < all.Size(); ++i)
							if (all[i].Usable()) { fallback = i; break; }
						if (!all[fallback].Usable()) return;
						idx = fallback;
					}

					WaitThinking();
					DestroyThinker();
					DestroyMain();

					mRulesIdx = idx;
					mRulesVt  = all[idx].factory.vtable;
					mRulesOk  = mRulesVt.Create && mRulesVt.CreateState && mRulesVt.Setup &&
							   mRulesVt.GetView && mRulesVt.GenerateLegalMoves && mRulesVt.ApplyMove;
					if (!mRulesOk) {
						logger.Error("[lab] moteur de regles incomplet : vtable partielle");
						return;
					}
					mRules = mRulesVt.Create();
					mState = mRulesVt.CreateState(mRules);
					mProbe = mRulesVt.CreateState(mRules);
					mReplay = mRulesVt.CreateState(mRules);
					NewGame();
				}

				usize				  RulesIndex() const noexcept { return mRulesIdx; }

				// ---- de QUOI vient ce resultat ? ----------------------------
				// Trois libelles, pour qu'une trace copiee se suffise a elle-meme.
				// Un journal qui ne nomme ni le moteur ni les IA qui l'ont produit
				// fait chercher un bug la ou il n'y en a pas — et avec deux
				// stagiaires en parallele, il ne dit meme pas de qui vient le code
				// incrimine.
				const char *RulesLabel() const noexcept {
					if (!mHost) return "?";
					const NkVector<NkcRulesEntry> &all = mHost->Rules();
					if (mRulesIdx >= all.Size()) return "?";
					return all[mRulesIdx].label.CStr();
				}

				/// Le plateau charge. « par defaut du moteur » tant qu'aucun fichier
				/// n'a ete choisi : le moteur en fournit toujours un.
				const char *BoardLabel() const noexcept {
					const NkVector<NkcBoardFile> &f = mBoards.Files();
					if (mBoardIdx < 0 || static_cast<usize>(mBoardIdx) >= f.Size())
						return "(par defaut du moteur)";
					return f[static_cast<usize>(mBoardIdx)].name.CStr();
				}

				const char *SeatLabel(uint8 seat) const noexcept {
					if (seat >= kMaxPlayers) return "?";
					const int32 m = mPlayers[seat].aiModule;
					if (m < 0) return "Humain";
					if (!mHost) return "?";
					const NkVector<NkcAIEntry> &all = mHost->Ais();
					if (static_cast<usize>(m) >= all.Size()) return "?";
					return all[static_cast<usize>(m)].label.CStr();
				}

				bool				  Ready() const noexcept { return mRulesOk && mRules && mState; }
				const NkcRulesVTable &Vt() const noexcept { return mRulesVt; }
				NkcRules			  RulesInst() const noexcept { return mRules; }

				/// Le catalogue a bouge (compilation, rechargement a chaud) : les
				/// pointeurs de fonction de l'ancienne DLL sont morts. On se relie.
				void Rebind() noexcept { UseRules(mRulesIdx); }

				/// Scrute les dossiers de modules, mais JAMAIS pendant une
				/// reflexion : recharger une DLL sous les pieds du thread worker
				/// est le plus sur moyen de planter sans trace.
				///
				/// La compilation est SYNCHRONE : quand le stagiaire sauvegarde,
				/// l'atelier se fige une seconde ou deux, puis son module apparait.
				/// C'est assume — un compilateur pilote depuis un thread annexe
				/// demanderait une file de travaux et un verrou sur le catalogue,
				/// pour gagner deux secondes toutes les cinq minutes.
				void PollModules(float32 dt) noexcept {
					if (!mHost || mThinking || !mAutoScan) return;
					mScanTimer += dt;
					if (mScanTimer < 1.0f) return;
					mScanTimer = 0.f;
					ScanModules();
				}

				/// Scrutation immediate (bouton « Recompiler » du panneau Modules).
				void ScanModules() noexcept {
					if (!mHost || mThinking) return;
					if (mHost->Refresh() > 0) Rebind();
				}

				bool AutoScan() const noexcept { return mAutoScan; }
				void SetAutoScan(bool v) noexcept { mAutoScan = v; }

				// ---- parametres --------------------------------------------
				const char *ParamsSchemaJson() const noexcept {
					if (!Ready() || !mRulesVt.GetParamsSchemaJson) return "";
					return mRulesVt.GetParamsSchemaJson(mRules);
				}

				void SetParam(const char *key, float64 value) noexcept {
					if (!Ready() || !mRulesVt.SetParam) return;
					mRulesVt.SetParam(mRules, key, value);
					mThinkerDirty = true;
					mParamsTouched = true;
				}

				float64 GetParam(const char *key) const noexcept {
					if (!Ready() || !mRulesVt.GetParam) return 0.0;
					return mRulesVt.GetParam(mRules, key);
				}

				/// Vrai depuis le dernier NewGame si un parametre a change : le
				/// panneau Regles s'en sert pour dire « relance la partie ».
				bool ParamsTouched() const noexcept { return mParamsTouched; }

				/// Repart d'un moteur neuf, valeurs par defaut du module.
				void ResetRulesInstance() noexcept { UseRules(mRulesIdx); }

				// ---- partie -------------------------------------------------
				void NewGame() noexcept {
					if (!Ready()) return;
					WaitThinking();
					mRulesVt.Setup(mRules, mState, mPlayerCount, mSeed);
					mJournal.Clear();
					mCursor		   = -1;
					mHasSel		   = false;
					mEcho		   = NkcMoveEcho{};
					mParamsTouched = false;
					mAiTimer	   = 0.f;
					mThinkerDirty  = true;
					RefreshView();
					RefreshLegal();
					RefreshPreviews();
					for (uint8 p = 0; p < kMaxPlayers; ++p) ResetAiMemory(p);
					CheckCoherence();
				}

				/// Ce que l'atelier reproche au moteur courant, ou "" si rien.
				/// Affiche en clair par le panneau Modules — voir CheckCoherence.
				const char *Coherence() const noexcept { return mCoherence; }

				// -------------------------------------------------------------
				// COHERENCE : rendre BRUYANTE une divergence qui etait muette.
				//
				// LE PROBLEME
				// Un moteur declare son voisinage (GetNeighbors) et genere ses coups
				// (GenerateLegalMoves). Rien n'oblige les deux a s'accorder. Quand ils
				// divergent, RIEN NE SE PLAINT :
				//   - l'atelier dessine des liens d'adjacence faux ;
				//   - une IA qui compte « combien d'ennemis touche cette case » se
				//     trompe silencieusement, et joue simplement moins bien.
				//
				// Une IA affaiblie sans message d'erreur, c'est une semaine perdue a
				// chercher dans l'evaluation un bug qui est dans la geometrie.
				//
				// CE QU'ON VERIFIE
				// Un coup DUPLIQUER va toujours vers une case voisine de sa source —
				// c'est la definition meme de l'action. Si `to` n'est pas dans
				// GetNeighbors(from), l'un des deux ment. On ne dit pas lequel : on
				// signale la contradiction, ce qui suffit a la faire chercher.
				//
				// ON NE VERIFIE QUE CE QUI EST VERIFIABLE. Sans GetNeighbors declare,
				// il n'y a rien a confronter — et si le moteur declare en plus sa
				// propre geometrie d'ecran, c'est le cas le PLUS a risque : la
				// topologie ne peut plus servir de repli honnete. On le dit.
				void CheckCoherence() noexcept {
					mCoherence[0] = '\0';
					if (!Ready()) return;

					if (!mRulesVt.GetNeighbors) {
						if (mRulesVt.GetCellCenter) {
							std::snprintf(mCoherence, sizeof(mCoherence),
										  "Ce moteur dessine sa propre geometrie mais ne declare pas "
										  "GetNeighbors. Une IA ne peut alors PAS connaitre l'adjacence : "
										  "elle la devinera depuis la topologie, et se trompera en silence.");
						}
						return;
					}

					NkcCoord	 nb[64];
					int32		 bad = 0;
					NkcCoord	 badFrom{}, badTo{};
					const usize	 n = mLegal.Size();
					for (usize i = 0; i < n; ++i) {
						const NkcMove &m = mLegal[i];
						if (m.kind != NkcMoveKind::Duplicate) continue;
						const uint32 cnt = mRulesVt.GetNeighbors(mRules, m.from, nb, 64);
						bool		 ok	 = false;
						for (uint32 k = 0; k < cnt && k < 64; ++k)
							if (CoordEqual(nb[k], m.to)) { ok = true; break; }
						if (!ok) {
							if (bad == 0) { badFrom = m.from; badTo = m.to; }
							++bad;
						}
					}
					if (bad > 0) {
						std::snprintf(mCoherence, sizeof(mCoherence),
									  "INCOHERENCE : %d coup(s) DUPLIQUER visent une case que "
									  "GetNeighbors ne declare pas voisine — p.ex. (%d,%d) -> (%d,%d). "
									  "Ton generateur de coups et ton voisinage ne disent pas la meme "
									  "chose ; toute IA qui evalue l'adjacence se trompera.",
									  bad, badFrom.q, badFrom.r, badTo.q, badTo.r);
					}
				}

				void   SetSeed(uint64 s) noexcept { mSeed = s ? s : 1ull; }
				uint64 Seed() const noexcept { return mSeed; }

				void SetPlayerCount(uint8 n) noexcept {
					if (n < 2) n = 2;
					if (n > static_cast<uint8>(kMaxPlayers)) n = static_cast<uint8>(kMaxPlayers);
					mPlayerCount = n;
				}
				uint8 PlayerCount() const noexcept { return mPlayerCount; }

				NkcPlayerCfg	   &Player(uint8 p) noexcept { return mPlayers[p < kMaxPlayers ? p : 0]; }
				const NkcPlayerCfg &Player(uint8 p) const noexcept { return mPlayers[p < kMaxPlayers ? p : 0]; }

				bool AutoPlay() const noexcept { return mAutoPlay; }
				void SetAutoPlay(bool v) noexcept { mAutoPlay = v; }
				float32 &Speed() noexcept { return mSpeed; }   ///< secondes entre deux coups d'IA

				bool Thinking() const noexcept { return mThinking; }

				/// Force UN coup d'IA, meme en pause. C'est le bouton « pas a pas ».
				void StepOnce() noexcept { mStepRequested = true; }

				// ---- boucle -------------------------------------------------
				void Tick(float32 dt) noexcept {
					mEcho.age += dt;
					if (!Ready()) return;

					// Une reflexion terminee est recoltee AVANT tout le reste :
					// sinon le coup arrive avec une frame de retard visible.
					if (mThinking && mJob.done.Load() != 0) CollectThinking();
					if (mThinking) return;
					if (mCursor >= 0) return;			  // on relit le passe : le jeu attend
					if (mView.finished) return;
					if (IsHumanTurn()) return;

					mAiTimer += dt;
					const bool due = mAutoPlay && mAiTimer >= mSpeed;
					if (!due && !mStepRequested) return;
					mAiTimer	   = 0.f;
					mStepRequested = false;
					StartThinking();
				}

				// ---- interaction humaine ------------------------------------
				bool IsHumanTurn() const noexcept {
					if (!Ready() || mView.finished) return false;
					return mPlayers[mView.current & 3].aiModule < 0;
				}

				bool HasSelection() const noexcept { return mHasSel; }
				NkcCoord Selection() const noexcept { return mSel; }

				/// Le joueur a clique une cellule. Selection puis destination ; un
				/// clic sur un autre totem allie deplace la selection, un clic
				/// ailleurs l'annule.
				void ClickCell(NkcCoord c) noexcept {
					if (!Ready() || !IsHumanTurn() || mCursor >= 0) return;

					if (mHasSel) {
						for (usize i = 0; i < mLegal.Size(); ++i) {
							const NkcMove &m = mLegal[i];
							if (m.kind == NkcMoveKind::Pass) continue;
							if (CoordEqual(m.from, mSel) && CoordEqual(m.to, c)) {
								PlayMove(m, false);
								mHasSel = false;
								RefreshPreviews();
								return;
							}
						}
					}
					if (HasMoveFrom(c)) {
						mSel	= c;
						mHasSel = true;
					} else {
						mHasSel = false;
					}
					RefreshPreviews();
				}

				/// Vrai quand PASSER est le seul coup legal — le panneau Plateau
				/// affiche alors un bouton, plutot que de laisser le joueur devant
				/// un plateau qui ne repond plus.
				bool OnlyPass() const noexcept {
					return mLegal.Size() == 1 && mLegal[0].kind == NkcMoveKind::Pass;
				}

				void PlayPass() noexcept {
					if (!Ready() || !OnlyPass() || mCursor >= 0) return;
					PlayMove(mLegal[0], false);
				}

				// ---- vues ---------------------------------------------------
				const NkcStateView		 &View() const noexcept { return mView; }
				const NkVector<NkcMove>	 &Legal() const noexcept { return mLegal; }
				const NkVector<NkcPreview> &Previews() const noexcept { return mPreviews; }
				const NkcMoveEcho		 &Echo() const noexcept { return mEcho; }
				const NkVector<NkcJournalEntry> &Journal() const noexcept { return mJournal; }
				uint64 Hash() const noexcept {
					return (Ready() && mRulesVt.HashState) ? mRulesVt.HashState(mRules, mState) : 0ull;
				}

				// ---- rejeu ---------------------------------------------------
				/// -1 = position vivante. Sinon : nombre de coups rejoues depuis le
				/// debut. Le rejeu est RECONSTRUIT (Setup + N coups), jamais
				/// « defait » : annuler un coup demanderait au moteur une operation
				/// inverse que le contrat ne lui impose pas.
				int32 Cursor() const noexcept { return mCursor; }

				void SetCursor(int32 c) noexcept {
					if (!Ready()) return;
					const int32 n = static_cast<int32>(mJournal.Size());
					if (c < 0 || c >= n) { mCursor = -1; RefreshView(); return; }
					mCursor = c;
					mRulesVt.Setup(mRules, mReplay, mPlayerCount, mSeed);
					for (int32 i = 0; i <= c; ++i)
						mRulesVt.ApplyMove(mRules, mReplay, &mJournal[static_cast<usize>(i)].move,
										   nullptr, nullptr);
					mRulesVt.GetView(mRules, mReplay, &mReplayView);
				}

				void LiveView() noexcept { SetCursor(-1); }

				/// L'etat A AFFICHER : la partie vivante, ou la position rejouee.
				const NkcStateView &DisplayView() const noexcept {
					return mCursor >= 0 ? mReplayView : mView;
				}

				// ---- statistiques d'usage (panneau Metriques) ----------------
				const uint32 *ActionUsage() const noexcept { return mActionUsage; }

			private:
				// ---- destruction --------------------------------------------
				void DestroyMain() noexcept {
					if (!mRules) { mState = mProbe = mReplay = nullptr; return; }
					if (mRulesVt.DestroyState) {
						if (mState)	 mRulesVt.DestroyState(mRules, mState);
						if (mProbe)	 mRulesVt.DestroyState(mRules, mProbe);
						if (mReplay) mRulesVt.DestroyState(mRules, mReplay);
					}
					if (mRulesVt.Destroy) mRulesVt.Destroy(mRules);
					mRules = nullptr;
					mState = mProbe = mReplay = nullptr;
					mRulesOk = false;
					mLegal.Clear();
					mPreviews.Clear();
					mView = NkcStateView{};
				}

				void DestroyThinker() noexcept {
					if (mJob.ai && mJob.avt.Destroy) mJob.avt.Destroy(mJob.ai);
					mJob.ai = nullptr;
					if (mJob.rules) {
						if (mJob.state && mJob.rvt.DestroyState) mJob.rvt.DestroyState(mJob.rules, mJob.state);
						if (mJob.rvt.Destroy) mJob.rvt.Destroy(mJob.rules);
					}
					mJob.rules	  = nullptr;
					mJob.state	  = nullptr;
					mJob.avt	  = NkcAIVTable{};
					mAiBound	  = -1;
					mThinkerDirty = true;
				}

				void WaitThinking() noexcept {
					if (!mThinking) return;
					if (mThread.Joinable()) mThread.Join();
					mThinking = false;
				}

				// ---- rafraichissements --------------------------------------
				void RefreshView() noexcept {
					if (!Ready()) return;
					mRulesVt.GetView(mRules, mState, &mView);
				}

				void RefreshLegal() noexcept {
					mLegal.Clear();
					if (!Ready() || mView.finished) return;
					mMoveBuf.Resize(kMoveCap);
					const uint32 total = mRulesVt.GenerateLegalMoves(mRules, mState, mMoveBuf.Data(), kMoveCap);
					const uint32 n	   = total < kMoveCap ? total : kMoveCap;
					for (uint32 i = 0; i < n; ++i) mLegal.PushBack(mMoveBuf[i]);
					mLegalTotal = total;
				}

				bool HasMoveFrom(NkcCoord c) const noexcept {
					for (usize i = 0; i < mLegal.Size(); ++i)
						if (mLegal[i].kind != NkcMoveKind::Pass && CoordEqual(mLegal[i].from, c)) return true;
					return false;
				}

				struct FlipCollect {
						NkcCoord cells[kMaxFlips];
						int32	 count	 = 0;
						int32	 cascade = 0;
				};

				static void FlipSink(void *user, const NkcEvent *ev) noexcept {
					FlipCollect *fc = static_cast<FlipCollect *>(user);
					if (!fc || !ev) return;
					if (ev->kind == NkcEventKind::TotemTransformed) {
						if (fc->count < kMaxFlips) fc->cells[fc->count++] = ev->a;
					} else if (ev->kind == NkcEventKind::Cascade) {
						fc->cascade = ev->value;
					}
				}

				/// Simule chaque coup partant de la selection et retient ce que le
				/// MOTEUR dit avoir retourne. Recalcule seulement a la selection ou
				/// apres un coup : jamais par frame.
				void RefreshPreviews() noexcept {
					mPreviews.Clear();
					if (!Ready() || !mHasSel || !mProbe || !mRulesVt.CloneState) return;
					for (usize i = 0; i < mLegal.Size(); ++i) {
						const NkcMove &m = mLegal[i];
						if (m.kind == NkcMoveKind::Pass) continue;
						if (!CoordEqual(m.from, mSel)) continue;

						mRulesVt.CloneState(mRules, mProbe, mState);
						FlipCollect fc;
						if (!mRulesVt.ApplyMove(mRules, mProbe, &m, &NkcSession::FlipSink, &fc)) continue;

						NkcPreview pv;
						pv.to		 = m.to;
						pv.moveIndex = static_cast<uint32>(i);
						pv.flipCount = fc.count;
						for (int32 k = 0; k < fc.count; ++k) pv.flips[k] = fc.cells[k];
						mPreviews.PushBack(pv);
					}
				}

				// ---- application d'un coup ----------------------------------
				bool PlayMove(const NkcMove &mv, bool byAi) noexcept {
					if (!Ready() || mView.finished) return false;

					FlipCollect fc;
					if (!mRulesVt.ApplyMove(mRules, mState, &mv, &NkcSession::FlipSink, &fc)) {
						logger.Error("[lab] coup refuse par le moteur — coup ignore");
						return false;
					}

					NkcJournalEntry e;
					e.move	  = mv;
					e.turn	  = mView.turn;
					e.player  = mv.player;
					e.byAi	  = byAi ? 1 : 0;
					e.flipped = static_cast<int16>(fc.count);
					RefreshView();
					e.hash = mRulesVt.HashState ? mRulesVt.HashState(mRules, mState) : 0ull;
					mJournal.PushBack(e);

					const uint32 k = static_cast<uint32>(mv.kind);
					if (k < 5) ++mActionUsage[k];

					mEcho			= NkcMoveEcho{};
					mEcho.valid		= 1;
					mEcho.from		= mv.from;
					mEcho.to		= mv.to;
					mEcho.player	= mv.player;
					mEcho.cascade	= fc.cascade;
					mEcho.flipCount = fc.count;
					for (int32 i = 0; i < fc.count; ++i) mEcho.flips[i] = fc.cells[i];
					mEcho.age = 0.f;

					RefreshLegal();
					RefreshPreviews();

					// Reutilisation d'arbre (MCTS) : on previent l'IA du coup REEL.
					if (mJob.ai && mJob.avt.OnMovePlayed) mJob.avt.OnMovePlayed(mJob.ai, &mv);
					return true;
				}

				// ---- reflexion sur thread -----------------------------------
				void ResetAiMemory(uint8) noexcept {
					if (mJob.ai && mJob.avt.Reset) mJob.avt.Reset(mJob.ai);
				}

				/// Prepare l'instance privee du thread : meme moteur, memes regles,
				/// meme plateau. Appelee seulement quand rien ne tourne.
				bool EnsureThinker() noexcept {
					if (!Ready()) return false;
					if (!mJob.rules) {
						mJob.rvt   = mRulesVt;
						mJob.rules = mJob.rvt.Create ? mJob.rvt.Create() : nullptr;
						if (!mJob.rules) return false;
						mJob.state	  = mJob.rvt.CreateState(mJob.rules);
						mThinkerDirty = true;
					}
					if (mThinkerDirty) {
						NkcSyncRules(mRulesVt, mJob.rules, mRules);
						mThinkerDirty = false;
					}
					return mJob.state != nullptr;
				}

				bool BindAi(int32 aiModule) noexcept {
					if (!mHost || aiModule < 0) return false;
					const NkVector<NkcAIEntry> &all = mHost->Ais();
					if (static_cast<usize>(aiModule) >= all.Size()) return false;
					if (!all[static_cast<usize>(aiModule)].Usable()) return false;
					if (mAiBound == aiModule && mJob.ai) return true;

					if (mJob.ai && mJob.avt.Destroy) mJob.avt.Destroy(mJob.ai);
					mJob.avt = all[static_cast<usize>(aiModule)].factory.vtable;
					mJob.ai	 = mJob.avt.Create ? mJob.avt.Create() : nullptr;
					mAiBound = mJob.ai ? aiModule : -1;
					return mJob.ai != nullptr;
				}

				/// Un echec de demarrage est TRACE, pas avale : sans cela l'atelier
				/// retente en silence toutes les `mSpeed` secondes et paraît en panne.
				bool Fail(const char *why) noexcept {
					mLastStartOk = false;
					std::snprintf(mStartFail, sizeof(mStartFail), "%s", why ? why : "raison inconnue");
					logger.Errorf("[lab] reflexion non demarree : %s", mStartFail);
					return false;
				}

				void StartThinking() noexcept {
					const NkcPlayerCfg &cfg = mPlayers[mView.current & 3];
					if (cfg.aiModule < 0) { Fail("ce siege est humain"); return; }
					if (!EnsureThinker()) { Fail("instance de regles du thread impossible a creer"); return; }
					if (!BindAi(cfg.aiModule)) { Fail("IA introuvable ou refusee au chargement"); return; }
					if (!mRulesVt.SerializeState || !mJob.rvt.DeserializeState) {
						Fail("le moteur n'implemente pas Serialize/DeserializeState");
						return;
					}

					// Transfert de l'etat par le contrat : le thread ne voit jamais
					// la memoire du moteur principal.
					const uint32 need = mRulesVt.SerializeState(mRules, mState, nullptr, 0);
					if (need == 0) { Fail("SerializeState annonce une taille nulle"); return; }
					mBlob.Resize(need);
					if (mRulesVt.SerializeState(mRules, mState, mBlob.Data(), need) != need) {
						Fail("SerializeState n'a pas ecrit la taille annoncee");
						return;
					}
					if (!mJob.rvt.DeserializeState(mJob.rules, mJob.state, mBlob.Data(), need)) {
						Fail("DeserializeState a refuse l'etat transfere");
						return;
					}
					mLastStartOk = true;
					mStartFail[0] = '\0';

					NkcAIConfig c;
					c.difficulty = cfg.diff;
					c.budgetMs	 = cfg.budgetMs;
					// Graine derivee de (partie, tour, joueur) : deux parties de meme
					// graine rejouent a l'identique, et deux tours ne partagent pas
					// le meme flux.
					c.seed = mSeed ^ (static_cast<uint64>(mView.turn) * 0x9E3779B97F4A7C15ull) ^
							 (static_cast<uint64>(mView.current) + 1ull);
					if (mJob.avt.Configure) mJob.avt.Configure(mJob.ai, &c);

					mThinkFor = mView.current;
					mJob.done.Store(0);
					mJob.ok.Store(0);
					mThinking = true;

					NkcThinkJob *job = &mJob;
					mThread			 = threading::NkThread([job](void *) {
						 NkcAIResult r;
						 std::memset(&r, 0, sizeof(r));
						 const int32 ok = job->avt.ChooseMove
											  ? job->avt.ChooseMove(job->ai, &job->rvt, job->rules, job->state, &r)
											  : 0;
						 job->result = r;
						 job->ok.Store(ok);
						 job->done.Store(1);	// PUBLIE tout ce qui precede
					 });
					if (mThread.Joinable()) {
						mThread.SetName("nkc-ai");
					} else {   // creation refusee : on ne fige pas l'atelier
						mThinking = false;
						Fail("thread de reflexion refuse par le systeme");
					}
				}

				void CollectThinking() noexcept {
					if (mThread.Joinable()) mThread.Join();
					mThinking = false;

					mLastResult	 = mJob.result;
					mLastResultOk = mJob.ok.Load() != 0;

					if (!mLastResultOk) {			  // l'IA declare n'avoir aucun coup
						if (OnlyPass()) PlayMove(mLegal[0], true);
						return;
					}
					if (mThinkFor != mView.current) return;   // la position a change sous elle

					// Le contrat exige que le coup soit legal : on VERIFIE, et on le
					// dit. C'est ce controle qui rend le module du stagiaire debuggable.
					if (mRulesVt.IsLegalMove && !mRulesVt.IsLegalMove(mRules, mState, &mLastResult.move)) {
						++mIllegalCount;
						logger.Errorf("[lab] l'IA a propose un coup ILLEGAL (joueur %u, tour %u)",
									  static_cast<unsigned>(mView.current), mView.turn);
						if (!mLegal.Empty()) PlayMove(mLegal[0], true);   // la partie continue
						return;
					}
					PlayMove(mLastResult.move, true);
				}

			public:
				const NkcAIResult &LastAiResult() const noexcept { return mLastResult; }
				bool			   LastAiOk() const noexcept { return mLastResultOk; }
				uint32			   IllegalCount() const noexcept { return mIllegalCount; }
				uint32			   LegalTotal() const noexcept { return mLegalTotal; }

			private:
				NkcModuleHost *mHost = nullptr;

				// moteur principal (thread de rendu)
				NkcRulesVTable mRulesVt{};
				NkcRules	   mRules  = nullptr;
				NkcState	   mState  = nullptr;
				NkcState	   mProbe  = nullptr;   ///< bac a sable des apercus
				NkcState	   mReplay = nullptr;   ///< position reconstruite du rejeu
				usize		   mRulesIdx = 0;
				bool		   mRulesOk	 = false;

				NkcStateView	   mView{};
				NkcStateView	   mReplayView{};
				NkVector<NkcMove>  mMoveBuf;
				NkVector<NkcMove>  mLegal;
				NkVector<NkcPreview> mPreviews;
				NkVector<NkcJournalEntry> mJournal;
				NkVector<uint8>	   mBlob;			///< tampon de transfert d'etat
				uint32			   mLegalTotal = 0;

				NkcMoveEcho mEcho;
				NkcCoord	mSel;
				bool		mHasSel = false;
				int32		mCursor = -1;

				uint64		 mSeed		  = 12345ull;
				uint8		 mPlayerCount = 2;
				NkcPlayerCfg mPlayers[kMaxPlayers];

				bool	mAutoPlay	   = false;	  ///< cf. Init : l'etudiant lance
				bool	mStepRequested = false;
				float32 mSpeed		   = 0.35f;
				float32 mAiTimer	   = 0.f;
				float32 mScanTimer	   = 0.f;
				bool	mParamsTouched = false;
				bool	mAutoScan	   = true;

				// reflexion
				NkcThinkJob			 mJob;
				threading::NkThread	 mThread;
				bool				 mThinking	   = false;
				bool				 mThinkerDirty = true;
				uint8				 mThinkFor	   = 0;
				int32				 mAiBound	   = -1;
				NkcAIResult			 mLastResult{};
				bool				 mLastResultOk = false;
				uint32				 mIllegalCount = 0;
				bool				 mLastStartOk  = true;
				char				 mStartFail[160] = {};

				NkcBoardLibrary mBoards;
				NkcCellShape	mShapeOverride = NkcCellShape::Auto;
				int32			mBoardIdx = -1;
				char			mCoherence[400] = {};

				uint32 mActionUsage[5] = {};   ///< None / Duplicate / Fuse / Power / Pass
		};

	} // namespace conqueror
} // namespace nkentseu
