#pragma once
// -----------------------------------------------------------------------------
// @File    DesignChatAsync.h
// @Brief   L'IA REPOND PENDANT QUE LA FENETRE VIT — un fil, un objet partage,
//          et UNE regle de partage ecrite avant la premiere ligne de code.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE CA FERME
// =============================================================================
//  `NkDesignConversation::Envoyer` appelle `NkIDesignBackend::Complete`, qui
//  BLOQUE. Avec le dorsal fichier c'est instantane ; avec le dorsal par
//  processus qui charge un modele local, la mesure du 14/09 donne
//  **12,3 s de chargement + 150,3 s de prefill + 818 ms par jeton** — une course
//  complete a dure **7 min 24 s, fenetre figee**. Ce n'est pas un plantage, mais
//  c'est inutilisable, et Rodolf l'a redemande deux fois.
//
// =============================================================================
//  LA REGLE DE PARTAGE — ecrite AVANT le code, et il n'y en a qu'une
// =============================================================================
//  Les deux fils ne partagent QU'UN objet, `NkTacheIA`, et **chaque champ a UN
//  SEUL ECRIVAIN**. C'est toute la discipline ; tout le reste en decoule.
//
//      champ     ecrit par        lu par           la lecture est sure...
//      ------    -------------    -------------    --------------------------
//      etat      le TRAVAILLEUR   l'INTERFACE      toujours (atomique)
//      sortie    le TRAVAILLEUR   l'INTERFACE      SEULEMENT si etat != EnCours
//      erreur    le TRAVAILLEUR   l'INTERFACE      SEULEMENT si etat != EnCours
//      annule    l'INTERFACE      le TRAVAILLEUR   toujours (atomique)
//      invite    l'INTERFACE      le TRAVAILLEUR   ecrit AVANT le lancement
//      dorsal    l'INTERFACE      le TRAVAILLEUR   ecrit AVANT le lancement
//
//  ⚠️ `etat` EST LA BARRIERE, et c'est la seule piece subtile. Le travailleur
//     ecrit `sortie` **puis** pose `etat` en RELEASE ; l'interface lit `etat` en
//     ACQUIRE **puis** lit `sortie`. La paire release/acquire garantit que tout
//     ce qui a ete ecrit avant le Store est visible apres le Load. Sans elle,
//     l'interface pourrait voir `etat = Finie` et une `sortie` a moitie ecrite —
//     un defaut qui ne se reproduit pas, ne se journalise pas, et se corrige a
//     coups de « ca remarche ».
//
//  ⚠️ AUCUN VERROU, ET C'EST UN CHOIX MOTIVE. Un mutex pris a chaque image pour
//     lire un booleen serait une contention gratuite — mais surtout il ne dirait
//     PAS quelle donnee est prete. La barriere, elle, le dit.
//
//  ⚠️ LA PROPRIETE EST PARTAGEE, PAR UN COMPTEUR A DEUX. C'est ce qui rend
//     l'annulation SURE alors que `Complete()` NE PEUT PAS ETRE INTERROMPU : il
//     est, chez le dorsal par processus, un `WaitForSingleObject(INFINITE)`.
//     Annuler ne tue donc rien :
//        1. l'interface pose `annule`, LACHE sa part et ne touche plus jamais
//           l'objet — pas meme pour le lire ;
//        2. le travailleur finit sa generation (on ne peut pas l'en empecher),
//           voit `annule`, **jette sa reponse**, lache sa part ;
//        3. le dernier des deux detruit.
//     C'est ce qui tient la promesse « apres annulation, pas de reponse qui
//     arrive apres coup dans la conversation suivante » : la reponse n'est pas
//     ignoree plus tard, elle n'est **jamais posee**.
//
//  ⚠️ CE QUI N'EST PAS LA, ET QUI EST NOMME : **LE FLUX DES JETONS**.
//     `NkIDesignBackend::Complete(requete, reponse)` rend le texte ENTIER, d'un
//     bloc. Afficher les jetons au fur et a mesure demanderait un rappel de
//     progression dans le contrat — c'est-a-dire une capacite propre aux moteurs
//     qui savent diffuser. Q6 l'interdit explicitement : *« le contrat doit
//     rester assez pauvre pour qu'un autre modele le remplisse »*. **Je livre
//     donc le NON-BLOCAGE SANS LE FLUX**, et le panneau montre le temps ecoule
//     au lieu d'un texte qui grandit. Le jour ou un dorsal sait diffuser, le
//     rappel s'ajoutera comme une option que les autres ignorent — pas comme une
//     obligation.
//
//  ⚠️ ET CE QUI RESTE VRAI : le modele n'est PAS ralenti pour rendre la fenetre
//     fluide. Le fil de generation ne cede rien, ne dort pas, n'est pas
//     tranche ; il fait exactement ce que faisait l'appel synchrone. Le banc
//     mesure les deux — les images de l'interface ET le temps du travail — parce
//     qu'un seul des deux chiffres se truque en abimant l'autre.
// -----------------------------------------------------------------------------

#include "DesignChat.h"

#include "NKCore/NkAtomic.h"
#include "NKThreading/NkThread.h"
#include "NKTime/NkChrono.h"

namespace nkuidesign {

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
			NkIDesignBackend *dorsal = nullptr;
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
		NkDesignRequest req;
		req.prompt = t->invite;
		NkDesignReply rep;
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
			bool Lancer(NkIDesignBackend *dorsal, const NkString &invite, NkString &pourquoi) {
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
	class NkDorsalLent final : public NkIDesignBackend {
		public:
			nkentseu::int64 millisecondes = 1000;
			NkString reponse = NkString("nkuidoc 1\n");

			bool Complete(const NkDesignRequest &, NkDesignReply &out) override {
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

} // namespace nkuidesign
