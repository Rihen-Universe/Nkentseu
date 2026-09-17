#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverseChatAsync.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// L'APPEL QUI NE GELE PAS L'INTERFACE.
//
// ⚠️ REGLE 2 DU MODULE, ET ELLE S'APPLIQUE ICI EN PREMIER : ce fichier utilise
//    NKThreading et NKTime, donc il les INCLUT lui-meme et `NKConverse.jenga`
//    les DECLARE. Un en-tete qui compte sur un autre pour tirer ses
//    dependances compile par chance chez ses consommateurs actuels, et casse
//    chez le premier qui ne l'a pas — 20 erreurs dans NKPA, la semaine
//    derniere, pour un chronometre sans son include.
// -----------------------------------------------------------------------------

#include "NKConverse/NkConverseChat.h"
#include "NKCore/NkAtomic.h"
#include "NKThreading/NkThread.h"
#include "NKTime/NkChrono.h"

namespace nkentseu::converse {


	enum class NkEtatTache : nkentseu::uint8 {
		EnCours = 0,
		Finie,
		Echouee,
		Annulee
	};

	inline const char *NkEtatTacheNom(NkEtatTache e) {
		switch (e) {
			case NkEtatTache::EnCours:
				return "en cours";
			case NkEtatTache::Finie:
				return "finie";
			case NkEtatTache::Echouee:
				return "echouee";
			default:
				return "annulee";
		}
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  L'OBJET PARTAGE — le SEUL. Voir la regle en tete de fichier.
	// ═══════════════════════════════════════════════════════════════════════
	struct NkTacheIA {
			// ── poses par l'INTERFACE, AVANT le lancement, jamais retouches ──
			NkIConverseBackend *dorsal = nullptr;
			NkString invite;

			// ── ecrits par le TRAVAILLEUR seul ──────────────────────────────
			NkString sortie;
			NkString erreur;

			// ── les deux atomiques, un ecrivain chacun ──────────────────────
			nkentseu::NkAtomic<nkentseu::uint8> etat{
				(nkentseu::uint8)NkEtatTache::EnCours}; ///< travailleur -> interface
			nkentseu::NkAtomicBool annule{false};	   ///< interface -> travailleur

			/// Le compteur de proprietaires : 2 a la creation (un par fil).
			nkentseu::NkAtomic<nkentseu::uint32> parts{2u};

			NkEtatTache Etat() const {
				return (NkEtatTache)etat.Load(nkentseu::NkMemoryOrder::NK_ACQUIRE);
			}

			/// ⚠️ LACHER, C'EST OUBLIER. Apres cet appel, celui qui appelle n'a
			///    plus le droit de toucher l'objet — ni d'ecrire, ni de LIRE.
			///    C'est la seule facon de detruire sans se demander « l'autre a-t-il
			///    fini ? », question a laquelle on repond toujours trop tot.
			void Lacher() {
				if (parts.FetchSub(1u) == 1u)
					delete this;
			}
	};

	/// LE TRAVAIL, hors de toute classe : il ne doit rien voir d'autre que sa
	/// tache. Une lambda capturant le panneau aurait mis le panneau a portee du
	/// second fil — et un jour quelqu'un s'en serait servi.
	inline void NkTravailIA(NkTacheIA *t) {
		if (!t)
			return;
		NkConverseRequest req;
		req.prompt = t->invite;
		NkConverseReply rep;
		const bool ok = t->dorsal && t->dorsal->Complete(req, rep) && rep.text.Length() > 0;

		// ⚠️ L'ORDRE EST LA MOITIE DU CORRECTIF : on ecrit la donnee, PUIS on
		//    publie l'etat en RELEASE. L'inverse laisserait voir un etat « finie »
		//    devant une sortie a moitie ecrite.
		NkEtatTache fin;
		if (t->annule.Load(nkentseu::NkMemoryOrder::NK_ACQUIRE)) {
			// ANNULEE : on ne publie RIEN. L'interface a deja lache sa part et ne
			// lira jamais ces champs ; les remplir serait du travail pour personne,
			// et surtout ce serait laisser croire qu'une reponse annulee existe
			// quelque part.
			fin = NkEtatTache::Annulee;
		} else if (ok) {
			t->sortie = rep.text;
			fin = NkEtatTache::Finie;
		} else {
			t->erreur = rep.error.Length() > 0 ? rep.error
											   : NkString("le dorsal n'a rien rendu");
			fin = NkEtatTache::Echouee;
		}
		t->etat.Store((nkentseu::uint8)fin, nkentseu::NkMemoryOrder::NK_RELEASE);
		t->Lacher();
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  LA POIGNEE, COTE INTERFACE
	// ═══════════════════════════════════════════════════════════════════════
	//  Elle ne sait rien du contenu : elle lance, elle compte les images, elle
	//  recolte une fois, elle annule. Tout ce qui juge la reponse reste dans la
	//  conversation.
	class NkEnvoiAsync {
		public:
			~NkEnvoiAsync() {
				// ⚠️ LE DESTRUCTEUR ANNULE, il n'attend pas. Attendre ici ferait
				//    d'une fermeture de fenetre une attente de sept minutes.
				Annuler();
			}

			bool EnCours() const {
				return mTache != nullptr;
			}

			/// Lancer. Rend faux (et `pourquoi` rempli) si une tache tourne deja ou
			/// s'il n'y a pas de dorsal — jamais deux generations en vol : la
			/// seconde ecraserait la reponse de la premiere, et le materiel ne
			/// tient de toute facon pas deux modeles.
			bool Lancer(NkIConverseBackend *dorsal, const NkString &invite, NkString &pourquoi) {
				pourquoi = NkString("");
				if (mTache) {
					pourquoi = NkString("une generation est deja en cours");
					return false;
				}
				if (!dorsal) {
					pourquoi = NkString("aucun dorsal branche");
					return false;
				}
				NkTacheIA *t = new NkTacheIA();
				t->dorsal = dorsal;
				t->invite = invite;
				mTache = t;
				mImages = 0;
				mHorloge = nkentseu::NkChrono();
				// Le fil est DETACHE : on ne le rejoindra jamais. La propriete
				// partagee est ce qui rend ce detachement sur.
				// ⚠️ LA SIGNATURE DU FIL EST `void(void*)`, PAS `void()`. Une lambda
				//    sans parametre compile jusqu'a l'appel, puis echoue dans
				//    NkFunction -- une erreur qui ne nomme pas son site.
				nkentseu::threading::NkThread fil([t](void *) { NkTravailIA(t); });
				fil.Detach();
				return true;
			}

			/// A APPELER A CHAQUE IMAGE. Rend vrai UNE SEULE FOIS, quand la tache
			/// s'acheve ; `texte` et `erreur` sont alors remplis et la poignee
			/// redevient libre.
			///
			/// ⚠️ C'EST AUSSI LE COMPTEUR DE PREUVE : chaque passage pendant
			///    l'attente incremente `mImages`. Si la fenetre etait figee, ce
			///    nombre vaudrait 1. C'est la mesure que le banc lit — pas une
			///    impression de fluidite.
			bool Recolter(NkString &texte, NkString &erreur, bool &reussi) {
				if (!mTache)
					return false;
				const NkEtatTache e = mTache->Etat();
				if (e == NkEtatTache::EnCours) {
					++mImages;
					return false;
				}
				reussi = (e == NkEtatTache::Finie);
				texte = reussi ? mTache->sortie : NkString("");
				erreur = reussi ? NkString("") : mTache->erreur;
				mSecondes = mHorloge.Elapsed().ToSeconds();
				mTache->Lacher();
				mTache = nullptr;
				return true;
			}

			/// Annuler. Ne tue rien — voir la regle en tete de fichier.
			void Annuler() {
				if (!mTache)
					return;
				mTache->annule.Store(true, nkentseu::NkMemoryOrder::NK_RELEASE);
				mTache->Lacher(); // on LACHE : on ne lira plus jamais cet objet
				mTache = nullptr;
				mSecondes = mHorloge.Elapsed().ToSeconds();
			}

			/// Le nombre d'images passees pendant l'attente. La PREUVE que la
			/// fenetre a vecu.
			nkentseu::uint32 Images() const {
				return mImages;
			}
			/// Secondes ecoulees : pendant l'attente, la mesure vive ; apres, la
			/// duree de la derniere course.
			nkentseu::float64 Secondes() const {
				return mTache ? mHorloge.Elapsed().ToSeconds() : mSecondes;
			}

		private:
			NkTacheIA *mTache = nullptr;
			nkentseu::NkChrono mHorloge;
			nkentseu::uint32 mImages = 0;
			nkentseu::float64 mSecondes = 0.0;
	};

	// ═══════════════════════════════════════════════════════════════════════
	//  UN DORSAL QUI PREND DU TEMPS, POUR LE BANC
	// ═══════════════════════════════════════════════════════════════════════
	//  ⚠️ CE N'EST PAS UN BOUCHON DE CONFORT : c'est l'instrument qui permet de
	//     mesurer le non-blocage SANS occuper la carte graphique pendant sept
	//     minutes, et SANS dependre de ce qu'un modele repond. Il dort — donc il
	//     reproduit exactement ce que fait le fil pendant un appel bloquant :
	//     rien, longtemps.
	//  ⚠️ Et il rend la mesure REPRODUCTIBLE : un banc cale sur le vrai modele
	//     dependrait de la VRAM libre, du pilote et de la longueur de l'invite.
	class NkDorsalLent final : public NkIConverseBackend {
		public:
			nkentseu::int64 millisecondes = 1000;
			NkString reponse = NkString("nkuidoc 1\n");

			bool Complete(const NkConverseRequest &, NkConverseReply &out) override {
				nkentseu::NkChrono::Sleep((nkentseu::int64)millisecondes);
				out.text = reponse;
				out.success = out.text.Length() > 0;
				if (!out.success)
					out.error = NkString("dorsal lent : rien a rendre");
				return out.success;
			}
			bool IsAvailable() const override {
				return true;
			}
			const char *Name() const override {
				return "lent (banc)";
			}
	};

} // namespace nkentseu::converse
