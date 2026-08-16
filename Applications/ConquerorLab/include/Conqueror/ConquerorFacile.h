#pragma once
// =============================================================================
// ConquerorFacile.h — LE CONTRAT, EN CONFORTABLE.
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// Le contrat (ConquerorRulesABI.h) est fait pour etre STABLE : structures plates,
// pointeurs bruts, aucune allocation, rien qui bouge entre deux versions du
// compilateur. C'est ce qu'il faut pour une frontiere binaire.
//
// C'est aussi penible a lire. Compter « les ennemis autour de mes totems »
// s'ecrit, avec le contrat nu :
//
//     NkcStateView v;
//     std::memset(&v, 0, sizeof(v));
//     rules->GetView(inst, st, &v);
//     for (uint32 i = 0; i < v.cellCount; ++i) {
//         if (v.cells[i].owner != (int8)moi) continue;
//         NkcCoord nb[16];
//         const uint32 n = rules->GetNeighbors(inst, v.coords[i], nb, 16);
//         for (uint32 k = 0; k < n; ++k)
//             for (uint32 j = 0; j < v.cellCount; ++j)      // recherche a la main
//                 if (v.coords[j].q == nb[k].q && v.coords[j].r == nb[k].r) {
//                     if (v.cells[j].owner >= 0 && v.cells[j].owner != (int8)moi) ++c;
//                     break;
//                 }
//     }
//
// Avec ce fichier :
//
//     Plateau p(rules, inst, st);
//     for (Case c : p)
//         if (c.AMoi(moi))
//             contact += p.CompteVoisins(c.ou, Voisin::Ennemi, moi);
//
// TROIS PROMESSES
// ---------------
//   1. RIEN DE NOUVEAU. Chaque fonction ici appelle le contrat, et rien d'autre.
//      Tout ce qu'on peut faire avec ce fichier, on peut le faire sans — en plus
//      long. Il n'y a donc aucun risque a l'utiliser, ni rien a desapprendre.
//
//   2. TOUT EST INLINE. Aucun symbole a lier, aucun cout a l'execution qu'un
//      compilateur en -O2 ne supprime. Vous pouvez l'utiliser dans une boucle
//      chaude d'IA.
//
//   3. AUCUNE ALLOCATION. Tout passe par des tableaux fournis par l'appelant ou
//      par des vues sur la memoire du module. Un `Plateau` fait quelques
//      pointeurs : le construire dans une boucle ne coute rien.
//
// CE QU'IL NE FAIT PAS
// --------------------
// Il ne vous dispense pas de comprendre le contrat. Le jour ou quelque chose ne
// marche pas, c'est ConquerorRulesABI.h qu'il faudra lire — ce fichier n'est
// qu'une facon plus courte de dire la meme chose.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorGeometry.h"

namespace nkentseu {
	namespace conqueror {
		namespace facile {

			// -----------------------------------------------------------------
			/// Une case, AVEC sa coordonnee.
			///
			/// Le contrat les separe en deux tableaux paralleles (`cells` et
			/// `coords`), ce qui est efficace et facile a desynchroniser quand on
			/// debute. Ici elles voyagent ensemble.
			struct Case {
					NkcCoord ou;			 ///< ou elle est
					int8	 proprietaire = kCellEmpty;
					int8	 niveau		  = 0;
					int32	 index		  = -1;	 ///< position dans la vue, si besoin

					bool Vide() const noexcept	  { return proprietaire == kCellEmpty; }
					bool Bloquee() const noexcept { return proprietaire == kCellBlocked; }
					bool Occupee() const noexcept { return proprietaire >= 0; }

					bool AMoi(uint8 moi) const noexcept {
						return proprietaire == static_cast<int8>(moi);
					}
					bool Ennemie(uint8 moi) const noexcept {
						return proprietaire >= 0 && proprietaire != static_cast<int8>(moi);
					}
			};

			/// Ce qu'on cherche dans un voisinage.
			enum class Voisin : uint8 {
				Tous	= 0,
				Vide	= 1,
				Ennemi	= 2,
				Allie	= 3,
				Bloque	= 4
			};

			// =================================================================
			/// LE PLATEAU, en lecture.
			///
			/// Se construit a partir de (vtable, instance, etat) et ne fait que
			/// des appels au contrat. Il ne POSSEDE rien : les pointeurs
			/// appartiennent au module, exactement comme `NkcStateView`. Donc,
			/// meme regle : apres un ApplyMove, reconstruisez-le.
			// =================================================================
			class Plateau {
				public:
					Plateau(const NkcRulesVTable &vt, NkcRules inst, const NkcState st) noexcept
						: mVt(&vt), mInst(inst), mSt(st) {
						// L'equivalent du memset + GetView, fait une fois pour toutes.
						mV = NkcStateView{};
						if (mVt->GetView) mVt->GetView(mInst, mSt, &mV);
					}

					// ---- l'etat de la partie ---------------------------------
					int32 NbCases() const noexcept	  { return static_cast<int32>(mV.cellCount); }
					uint8 NbJoueurs() const noexcept  { return mV.playerCount; }
					uint8 QuiJoue() const noexcept	  { return mV.current; }
					bool  Finie() const noexcept	  { return mV.finished != 0; }
					uint32 Tour() const noexcept	  { return mV.turn; }

					/// -1 = match nul, -2 = partie en cours.
					int32 Vainqueur() const noexcept  { return mV.winner; }

					int32 Totems(uint8 joueur) const noexcept {
						if (!mV.totemCount || joueur >= kMaxPlayers) return 0;
						return mV.totemCount[joueur];
					}
					int32 Energie(uint8 joueur) const noexcept {
						if (!mV.energy || joueur >= kMaxPlayers) return 0;
						return mV.energy[joueur];
					}
					/// En DIXIEMES entiers : 100 == 10,0 points (REGLES §17.1).
					int32 ConqueteDixiemes(uint8 joueur) const noexcept {
						if (!mV.conquestTenths || joueur >= kMaxPlayers) return 0;
						return mV.conquestTenths[joueur];
					}

					/// Totems a moi moins totems a tous les autres. L'evaluation la
					/// plus simple qui ait un sens, ecrite une fois ici plutot que
					/// recopiee dans chaque IA.
					int32 Avantage(uint8 moi) const noexcept {
						int32 miens = 0, autres = 0;
						for (uint8 p = 0; p < mV.playerCount; ++p) {
							if (p == moi) miens += Totems(p);
							else		  autres += Totems(p);
						}
						return miens - autres;
					}

					// ---- acces aux cases -------------------------------------
					Case operator[](int32 i) const noexcept {
						Case c;
						if (i < 0 || i >= NbCases() || !mV.cells || !mV.coords) return c;
						c.ou		   = mV.coords[i];
						c.proprietaire = mV.cells[i].owner;
						c.niveau	   = mV.cells[i].level;
						c.index		   = i;
						return c;
					}

					/// L'index d'une coordonnee, ou -1 si elle n'est pas sur le
					/// plateau. Recherche lineaire : sur quelques dizaines a
					/// quelques centaines de cases, c'est sous le bruit de mesure.
					int32 Index(NkcCoord ou) const noexcept {
						if (!mV.coords) return -1;
						for (uint32 i = 0; i < mV.cellCount; ++i)
							if (mV.coords[i].q == ou.q && mV.coords[i].r == ou.r)
								return static_cast<int32>(i);
						return -1;
					}

					/// La case a cette coordonnee. `index == -1` si elle n'existe
					/// pas — testez-le, c'est le hors-plateau.
					Case A(NkcCoord ou) const noexcept {
						const int32 i = Index(ou);
						return i < 0 ? Case{} : (*this)[i];
					}

					bool Existe(NkcCoord ou) const noexcept	 { return Index(ou) >= 0; }
					bool EstVide(NkcCoord ou) const noexcept { return A(ou).Vide(); }

					// ---- voisinage -------------------------------------------
					/// Ecrit les voisins de `ou` dans `sortie`, renvoie combien.
					///
					/// Passe par `GetNeighbors` si le module le declare — c'est LUI
					/// qui sait. Sinon, repli sur la topologie. Un module a
					/// geometrie libre qui oublie de declarer son voisinage fera
					/// donc repondre faux ici : c'est pourquoi l'atelier le signale
					/// (panneau Modules).
					int32 Voisins(NkcCoord ou, NkcCoord *sortie, int32 cap) const noexcept {
						if (!sortie || cap <= 0) return 0;
						if (mVt->GetNeighbors && mInst)
							return static_cast<int32>(
								mVt->GetNeighbors(mInst, ou, sortie,
												  static_cast<uint32>(cap)));
						const int32 n = NeighborCount(mV.topology);
						int32		w = 0;
						for (int32 i = 0; i < n && w < cap; ++i)
							sortie[w++] = Neighbor(mV.topology, ou, i);
						return w;
					}

					/// Compte les voisins d'un certain genre. C'est la fonction que
					/// toute evaluation finit par ecrire ; autant qu'elle soit juste
					/// une fois pour tout le monde.
					int32 CompteVoisins(NkcCoord ou, Voisin genre, uint8 moi = 0) const noexcept {
						NkcCoord	nb[32];
						const int32 n = Voisins(ou, nb, 32);
						int32		c = 0;
						for (int32 k = 0; k < n; ++k) {
							const Case v = A(nb[k]);
							if (v.index < 0) continue;	// hors plateau : ne compte pas
							switch (genre) {
								case Voisin::Tous:	 ++c; break;
								case Voisin::Vide:	 if (v.Vide()) ++c; break;
								case Voisin::Bloque: if (v.Bloquee()) ++c; break;
								case Voisin::Ennemi: if (v.Ennemie(moi)) ++c; break;
								case Voisin::Allie:	 if (v.AMoi(moi)) ++c; break;
							}
						}
						return c;
					}

					// ---- parcours : for (Case c : plateau) -------------------
					class Iter {
						public:
							Iter(const Plateau *p, int32 i) noexcept : mP(p), mI(i) {}
							Case operator*() const noexcept { return (*mP)[mI]; }
							Iter &operator++() noexcept { ++mI; return *this; }
							bool operator!=(const Iter &o) const noexcept { return mI != o.mI; }
						private:
							const Plateau *mP;
							int32		   mI;
					};
					Iter begin() const noexcept { return Iter(this, 0); }
					Iter end() const noexcept	{ return Iter(this, NbCases()); }

					/// La vue brute, pour ce que cette classe ne couvre pas. On ne
					/// vous enferme pas : le contrat reste accessible.
					const NkcStateView &Vue() const noexcept { return mV; }

				private:
					const NkcRulesVTable *mVt;
					NkcRules			  mInst;
					NkcState			  mSt;
					NkcStateView		  mV;
			};

			// =================================================================
			/// LES COUPS LEGAUX, en une ligne.
			///
			///     Coups liste(rules, inst, st);
			///     for (const NkcMove &m : liste) { ... }
			// =================================================================
			template <int32 CAP = 512>
			class Coups {
				public:
					Coups(const NkcRulesVTable &vt, NkcRules inst, const NkcState st) noexcept {
						mTotal = vt.GenerateLegalMoves ? vt.GenerateLegalMoves(inst, st, mBuf, CAP) : 0;
						mN	   = static_cast<int32>(mTotal < static_cast<uint32>(CAP)
													? mTotal : static_cast<uint32>(CAP));
					}

					int32 Nb() const noexcept	 { return mN; }
					bool  Aucun() const noexcept { return mN == 0; }

					/// Le nombre TOTAL de coups legaux, qui peut depasser la
					/// capacite. Si `Total() > Nb()`, votre plafond est trop bas et
					/// vous ne voyez pas tous les coups — mieux vaut le savoir.
					uint32 Total() const noexcept { return mTotal; }

					const NkcMove &operator[](int32 i) const noexcept { return mBuf[i]; }
					const NkcMove *begin() const noexcept { return mBuf; }
					const NkcMove *end() const noexcept	  { return mBuf + mN; }

				private:
					NkcMove mBuf[CAP];
					uint32	mTotal = 0;
					int32	mN	   = 0;
			};

			// =================================================================
			/// JOUER UN COUP « POUR DE FAUX ».
			///
			/// C'est le geste central de toute IA : essayer, regarder, oublier.
			/// Le contrat impose de cloner l'etat — ne JAMAIS muter celui qu'on
			/// recoit. Cette classe s'en charge et libere toute seule.
			///
			///     Essai e(rules, inst);
			///     if (e.Joue(st, coup)) {
			///         Plateau apres = e.Plateau();
			///         note = apres.Avantage(moi);
			///     }
			///
			/// EN BOUCLE CHAUDE : construisez l'Essai UNE FOIS, hors de la boucle,
			/// et rappelez `Joue` — creer un etat a chaque coup essaye couterait
			/// une allocation par candidat.
			// =================================================================
			class Essai {
				public:
					Essai(const NkcRulesVTable &vt, NkcRules inst) noexcept
						: mVt(&vt), mInst(inst) {
						if (mVt->CreateState) mEtat = mVt->CreateState(mInst);
					}

					~Essai() noexcept {
						if (mEtat && mVt->DestroyState) mVt->DestroyState(mInst, mEtat);
					}

					Essai(const Essai &)			= delete;
					Essai &operator=(const Essai &) = delete;

					bool Pret() const noexcept { return mEtat != nullptr; }

					/// Clone `depuis`, applique `coup`. Renvoie false si le moteur
					/// refuse — il a toujours le dernier mot, et un coup refuse
					/// n'est pas une erreur : c'est une reponse.
					bool Joue(const NkcState depuis, const NkcMove &coup) noexcept {
						if (!mEtat || !mVt->CloneState || !mVt->ApplyMove) return false;
						mVt->CloneState(mInst, mEtat, depuis);
						return mVt->ApplyMove(mInst, mEtat, &coup, nullptr, nullptr) != 0;
					}

					/// L'etat obtenu, en lecture confortable.
					facile::Plateau Plateau() const noexcept {
						return facile::Plateau(*mVt, mInst, mEtat);
					}

					/// L'etat brut, pour le passer a une recherche recursive.
					NkcState Etat() const noexcept { return mEtat; }

				private:
					const NkcRulesVTable *mVt;
					NkcRules			  mInst;
					NkcState			  mEtat = nullptr;
			};

		} // namespace facile
	} // namespace conqueror
} // namespace nkentseu
