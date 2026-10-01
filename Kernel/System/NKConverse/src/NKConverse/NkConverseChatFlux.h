#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverseChatFlux.h
// @Brief   LA CONVERSATION QUI NE GELE PAS L'INTERFACE, LUE EN FLUX ; et la
//          SONDE (lister les modeles, tester la connexion) qui ne la gele pas
//          non plus.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// MEME PATRON QUE NkConverseChatAsync.h (NkTacheIA / NkEnvoiAsync), ET C'EST
// VOULU : un objet partage, DEUX proprietaires comptes, l'interface qui LACHE
// sa part au lieu d'attendre (« LACHER, C'EST OUBLIER »), le travailleur qui
// ecrit puis PUBLIE l'etat en RELEASE. Ce qui s'ajoute :
//   - le TEXTE EN COURS, ecrit morceau par morceau par le fil de travail et lu
//     a chaque image par l'interface -- sous un verrou, parce que c'est une
//     chaine qui grandit (une lecture pendant une reallocation lirait de la
//     memoire rendue) ;
//   - l'ANNULATION LUE PENDANT LE FLUX : la requete est coupee au paquet
//     suivant, pas a la fin d'une generation de deux minutes.
// -----------------------------------------------------------------------------

#include "NKConverse/NkConverseFournisseurs.h"
#include "NKCore/NkAtomic.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"
#include "NKThreading/NkThread.h"
#include "NKTime/NkChrono.h"

namespace nkentseu::converse {

	enum class NkEtatChatIA : nkentseu::uint8 { NK_EN_COURS = 0, NK_FINI, NK_ANNULE };

	struct NkTacheChatIA {
			// ── poses par l'INTERFACE avant le lancement, jamais retouches ──
			NkReglagesFournisseur reglages;
			NkString cle; ///< ⚠️ en memoire seulement, le temps de la requete
			NkRequeteIA requete;
			// ── ecrits par le TRAVAILLEUR ──
			threading::NkMutex verrou;
			NkString texte;					 ///< le texte recu jusqu'ici (sous `verrou`)
			nkentseu::uint32 morceaux = 0u;	 ///< combien de morceaux (sous `verrou`)
			NkReponseIA reponse;			 ///< lu APRES l'etat publie
			// ── les atomiques ──
			nkentseu::NkAtomic<nkentseu::uint8> etat{static_cast<nkentseu::uint8>(NkEtatChatIA::NK_EN_COURS)};
			nkentseu::NkAtomicBool annule{false};
			nkentseu::NkAtomic<nkentseu::uint32> parts{2u};

			NkEtatChatIA Etat() const {
				return static_cast<NkEtatChatIA>(etat.Load(nkentseu::NkMemoryOrder::NK_ACQUIRE));
			}
			void Lacher() {
				if (parts.FetchSub(1u) == 1u)
					delete this;
			}
	};

	inline void NkTravailChatIA(NkTacheChatIA *t) {
		if (!t)
			return;
		NkReponseIA rep;
		const nkentseu::NkFunction<void(const NkString &)> sur = [t](const NkString &m) {
			threading::NkLockGuard g(t->verrou);
			t->texte.Append(m);
			++t->morceaux;
		};
		NkIaDiscuter(t->reglages, t->cle, t->requete, rep, sur, &t->annule);
		t->cle = NkString(); // la cle ne survit pas a la requete
		t->reponse = rep;
		const NkEtatChatIA fin = t->annule.Load(nkentseu::NkMemoryOrder::NK_ACQUIRE) ? NkEtatChatIA::NK_ANNULE
																					  : NkEtatChatIA::NK_FINI;
		t->etat.Store(static_cast<nkentseu::uint8>(fin), nkentseu::NkMemoryOrder::NK_RELEASE);
		t->Lacher();
	}

	/// LA POIGNEE, cote interface.
	class NkEnvoiChatIA {
		public:
			~NkEnvoiChatIA() {
				Annuler(); // ⚠️ le destructeur annule, il n'attend pas
			}
			bool EnCours() const {
				return mTache != nullptr;
			}
			/// Rend faux (et `pourquoi`) si un tour est deja en vol : jamais deux
			/// generations a la fois -- la seconde ecraserait la premiere, et la
			/// carte d'un PC de joueur ne tient pas deux modeles.
			bool Lancer(const NkReglagesFournisseur &r, const NkString &cle, const NkRequeteIA &q, NkString &pourquoi) {
				pourquoi = NkString();
				if (mTache) {
					pourquoi = NkString("une reponse est deja attendue");
					return false;
				}
				NkTacheChatIA *t = new NkTacheChatIA();
				t->reglages = r;
				t->cle = cle;
				t->requete = q;
				mTache = t;
				mHorloge = nkentseu::NkChrono();
				mImages = 0u;
				nkentseu::threading::NkThread fil([t](void *) { NkTravailChatIA(t); });
				fil.Detach();
				return true;
			}
			/// Le texte recu jusqu'ici (une COPIE, prise sous le verrou).
			NkString TexteEnCours(nkentseu::uint32 *morceaux = nullptr) {
				if (!mTache)
					return NkString();
				threading::NkLockGuard g(mTache->verrou);
				if (morceaux)
					*morceaux = mTache->morceaux;
				return mTache->texte;
			}
			/// A chaque image. Vrai UNE fois, quand le tour s'acheve.
			bool Recolter(NkReponseIA &out) {
				if (!mTache)
					return false;
				const NkEtatChatIA e = mTache->Etat();
				if (e == NkEtatChatIA::NK_EN_COURS) {
					++mImages;
					return false;
				}
				out = mTache->reponse;
				mSecondes = mHorloge.Elapsed().ToSeconds();
				mTache->Lacher();
				mTache = nullptr;
				return true;
			}
			/// Arreter : le travailleur coupe la requete au paquet suivant.
			void Annuler() {
				if (!mTache)
					return;
				mTache->annule.Store(true, nkentseu::NkMemoryOrder::NK_RELEASE);
				mTache->Lacher();
				mTache = nullptr;
				mSecondes = mHorloge.Elapsed().ToSeconds();
			}
			/// Les images passees pendant l'attente : la PREUVE que la fenetre a vecu.
			nkentseu::uint32 Images() const {
				return mImages;
			}
			nkentseu::float64 Secondes() const {
				return mTache ? mHorloge.Elapsed().ToSeconds() : mSecondes;
			}

		private:
			NkTacheChatIA *mTache = nullptr;
			nkentseu::NkChrono mHorloge;
			nkentseu::uint32 mImages = 0u;
			nkentseu::float64 mSecondes = 0.0;
	};

	// ═══════════════════════════════════════════════════════════════════════
	//  LA SONDE : lister les modeles / tester la connexion, sans geler
	// ═══════════════════════════════════════════════════════════════════════
	struct NkTacheSondeIA {
			NkReglagesFournisseur reglages;
			NkString cle;
			bool tester = false; ///< faux : lister seulement
			NkDiagIA diag = NkDiagIA::NK_ERREUR;
			NkString message;
			nkentseu::NkVector<NkConverseModeleInfo> modeles;
			nkentseu::NkAtomicBool fini{false};
			nkentseu::NkAtomic<nkentseu::uint32> parts{2u};
			void Lacher() {
				if (parts.FetchSub(1u) == 1u)
					delete this;
			}
	};

	class NkSondeIA {
		public:
			~NkSondeIA() {
				Oublier();
			}
			bool EnCours() const {
				return mTache != nullptr;
			}
			void Lancer(const NkReglagesFournisseur &r, const NkString &cle, bool tester) {
				Oublier();
				NkTacheSondeIA *t = new NkTacheSondeIA();
				t->reglages = r;
				t->cle = cle;
				t->tester = tester;
				mTache = t;
				nkentseu::threading::NkThread fil([t](void *) {
					if (t->tester)
						t->diag = NkIaTesterConnexion(t->reglages, t->cle, t->message, &t->modeles);
					else
						t->diag = NkIaListerModeles(t->reglages, t->cle, t->modeles, t->message);
					t->cle = NkString();
					t->fini.Store(true, nkentseu::NkMemoryOrder::NK_RELEASE);
					t->Lacher();
				});
				fil.Detach();
			}
			/// Vrai une fois, quand la sonde a fini ; les sorties sont alors posees.
			bool Recolter(NkDiagIA &diag, NkString &message, nkentseu::NkVector<NkConverseModeleInfo> &modeles,
						  bool &etaitTest) {
				if (!mTache || !mTache->fini.Load(nkentseu::NkMemoryOrder::NK_ACQUIRE))
					return false;
				diag = mTache->diag;
				message = mTache->message;
				modeles = mTache->modeles;
				etaitTest = mTache->tester;
				mTache->Lacher();
				mTache = nullptr;
				return true;
			}
			void Oublier() {
				if (!mTache)
					return;
				mTache->Lacher();
				mTache = nullptr;
			}

		private:
			NkTacheSondeIA *mTache = nullptr;
	};

} // namespace nkentseu::converse
