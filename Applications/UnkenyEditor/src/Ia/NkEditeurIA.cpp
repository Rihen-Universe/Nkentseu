// =============================================================================
// NkEditeurIA.cpp — la BOUCLE de l'IA, son PANNEAU, la fenetre des REGLAGES
//
// LA BOUCLE D'UNE DEMANDE
//   1. Entree dans le composeur : un bloc « demande », un message utilisateur.
//   2. UN TOUR : le message systeme est ENGENDRE (NkEditeurIADescription), la
//      requete part sur un fil de travail (NkEnvoiChatIA) ; le texte arrive EN
//      FLUX et le bloc de prose GRANDIT a chaque image.
//   3. La reponse dit des APPELS D'OUTILS (natifs, ou blocs <outil> du texte) :
//      un par un, chacun devient un bloc « outil » (arguments / resultat) et,
//      s'il a change quelque chose, un bloc « effet » (« Ctrl+Z la defait »).
//      ⚠️ UN APPEL IRREVERSIBLE S'ARRETE LA : le bloc porte « Confirmer » et
//         « Refuser », et la boucle ATTEND le clic. Rien n'est execute avant.
//   4. Les RESULTATS repartent au modele (un tour de plus), qui conclut en une
//      phrase -- ou appelle d'autres outils. Huit tours d'outils au plus par
//      demande : un modele qui tourne en rond ne bloque pas l'editeur.
//
// ⚠️ LES OUTILS S'EXECUTENT SUR LE FIL DE L'INTERFACE, jamais sur celui du
//    modele : la scene n'est pas partagee entre deux fils. Le fil de travail ne
//    fait que PARLER au service ; c'est la trame qui agit.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Ia/NkEditeurIA.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurInterface.h"

#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKConverse/NkConverseFournisseurs.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using converse::NkAppelOutil;
		using converse::NkMessageIA;
		using converse::NkReglagesFournisseur;
		using converse::NkRoleIA;
		using editorkit::NkAiBloc;
		using editorkit::NkAiBlocDonnees;

		namespace {

			NkReglagesFournisseur &Actif(NkEditeurIA &ia) {
				if (ia.actif < 0 || ia.actif >= static_cast<int32>(ia.fournisseurs.Size()))
					ia.actif = 0;
				return ia.fournisseurs[static_cast<usize>(ia.actif)];
			}

			NkString Cle(NkEditeurIA &ia, const NkReglagesFournisseur &r, NkString *source = nullptr) {
				NkString c;
				converse::NkIaLireCle(ia.dossier, r, c, source);
				return c;
			}

			/// Le serveur a-t-il dit que CE modele sait les outils ? 1 oui, 0 non, -1 inconnu.
			int32 OutilsModele(NkEditeurIA &ia, int32 i) {
				if (i < 0 || i >= static_cast<int32>(ia.modeles.Size()))
					return -1;
				const NkReglagesFournisseur &r = ia.fournisseurs[static_cast<usize>(i)];
				const NkVector<converse::NkConverseModeleInfo> &l = ia.modeles[static_cast<usize>(i)];
				for (usize k = 0; k < l.Size(); ++k)
					if (l[k].nom == r.modele || l[k].nom == r.modele + ":latest")
						return l[k].outils ? 1 : 0;
				return -1;
			}

			void Tailles(NkEditeurIA &ia) {
				const usize n = ia.fournisseurs.Size();
				while (ia.modeles.Size() < n)
					ia.modeles.PushBack(NkVector<converse::NkConverseModeleInfo>());
				while (ia.motifs.Size() < n)
					ia.motifs.PushBack(NkString());
				while (ia.forceTexte.Size() < n)
					ia.forceTexte.PushBack(0u);
				while (ia.modeles.Size() > n)
					ia.modeles.PopBack();
				while (ia.motifs.Size() > n)
					ia.motifs.PopBack();
				while (ia.forceTexte.Size() > n)
					ia.forceTexte.PopBack();
			}

			/// Le panneau du kit reflete les fournisseurs (nom, lieu, modeles listes).
			void DeclarerFournisseurs(NkEditeurIA &ia) {
				Tailles(ia);
				ia.pan.fournisseurs.Clear();
				for (usize i = 0; i < ia.fournisseurs.Size(); ++i) {
					const NkReglagesFournisseur &r = ia.fournisseurs[i];
					editorkit::NkAiFournisseurDesc d;
					d.cle = r.id;
					d.nom = r.nom;
					d.distant = r.Distant();
					d.pret = ia.motifs[i].Empty();
					d.motif = ia.motifs[i];
					const NkVector<converse::NkConverseModeleInfo> &l = ia.modeles[i];
					for (usize k = 0; k < l.Size(); ++k) {
						editorkit::NkAiModeleDesc md;
						md.nom = l[k].nom;
						md.detail = converse::NkConverseDecrireModele(l[k], r.Distant());
						md.motifEffort = NkString("l'effort n'est pas un réglage d'Unkeny");
						d.modeles.PushBack(md);
						if (l[k].nom == r.modele)
							d.modele = static_cast<int32>(k);
					}
					if (!r.modele.Empty() && d.modeles.Empty()) {
						editorkit::NkAiModeleDesc md;
						md.nom = r.modele;
						md.detail = NkString("choisi dans les réglages (liste non lue : Réglages > Actualiser)");
						d.modeles.PushBack(md);
						d.modele = 0;
					}
					ia.pan.fournisseurs.PushBack(d);
				}
				NkString pq;
				ia.pan.Choisir(ia.actif, pq);
			}

			NkVector<NkMessageIA> &Echange(NkEditeurIA &ia) {
				const int32 k = ia.pan.ChatActif();
				while (ia.echanges.Size() <= static_cast<usize>(k < 0 ? 0 : k))
					ia.echanges.PushBack(NkVector<NkMessageIA>());
				return ia.echanges[static_cast<usize>(k < 0 ? 0 : k)];
			}

			/// Pousse un bloc dans le fil, rend son identifiant (0 : refuse, et dit pourquoi).
			uint32 Pousser(NkEditeurIA &ia, const NkAiBlocDonnees &b) {
				NkString pq;
				editorkit::NkAiFil &f = ia.pan.Fil();
				if (!f.Pousser(b, pq)) {
					std::printf("[ia] bloc refuse par le fil : %s\n", pq.CStr());
					return 0u;
				}
				return f.At(f.Taille() - 1u).id;
			}
			uint32 Bloc(NkEditeurIA &ia, NkAiBloc type, const NkString &texte, const NkString &motif = NkString()) {
				NkAiBlocDonnees b;
				b.type = type;
				b.texte = texte;
				b.motif = motif;
				if (type == NkAiBloc::Refus || type == NkAiBloc::Echec)
					b.replie = false;
				if (type == NkAiBloc::Prose)
					b.replie = false;
				return Pousser(ia, b);
			}
			NkString Borne(const NkString &s, usize n) {
				return s.Length() <= n ? s : s.SubStr(0, n) + "…";
			}

			void LancerTour(NkEditeurCadre &c, NkEditeurIA &ia);

			/// Une action EXECUTEE : son bloc d'outil, son effet, son resultat au modele.
			void Executer(NkEditeurCadre &c, NkEditeurIA &ia, const NkAppelOutil &ap, bool natif) {
				const NkResultatOutilIA r = NkEditeurIAExecuter(c, ap);
				NkAiBlocDonnees b;
				b.type = NkAiBloc::Outil;
				b.titre = ap.nom;
				b.texte = r.ok ? (r.effet.Empty() ? NkString("fait") : r.effet) : NkString("refusé : ") + Borne(r.texte, 160);
				b.entree = Borne(ap.arguments, 900);
				b.sortie = Borne(r.texte, 1600);
				b.etiquetteEntree = NkString("Arguments");
				b.etiquetteSortie = NkString("Résultat");
				Pousser(ia, b);
				if (r.ok && r.modifie && !r.effet.Empty()) {
					NkAiBlocDonnees e;
					e.type = NkAiBloc::Effet;
					e.effet = r.effet + " — Ctrl+Z la défait";
					e.texte = r.effet;
					e.replie = false;
					ia.blocEffet = Pousser(ia, e);
					ia.historiqueApresEffet = c.m.historique.annuler.Size();
					ia.dernierEffet = r.effet;
					++ia.actionsFaites;
				}
				if (!r.ok)
					++ia.actionsRefusees;
				NkMessageIA m;
				m.role = natif ? NkRoleIA::NK_OUTIL : NkRoleIA::NK_UTILISATEUR;
				m.idAppel = ap.id;
				m.nomOutil = ap.nom;
				m.texte = natif ? r.texte : NkString("[resultat de l'outil ") + ap.nom + "]\n" + r.texte;
				ia.resultats.PushBack(m);
			}

			/// Les appels du tour, un par un ; s'arrete sur un IRREVERSIBLE.
			void ExecuterSuivants(NkEditeurCadre &c, NkEditeurIA &ia) {
				while (ia.prochain < ia.aExecuter.Size()) {
					const NkAppelOutil ap = ia.aExecuter[ia.prochain];
					NkString pourquoi;
					if (NkEditeurIAIrreversible(c, ap, pourquoi)) {
						NkAiBlocDonnees b;
						b.type = NkAiBloc::Outil;
						b.titre = ap.nom;
						b.texte = NkString("attend votre confirmation");
						b.entree = Borne(ap.arguments, 900);
						b.etiquetteEntree = NkString("Arguments");
						// LA QUESTION ENTIERE, en clair, dans le compartiment du bas :
						// sur la ligne du titre, elle serait tronquee.
						b.sortie = pourquoi;
						b.etiquetteSortie = NkString("Question");
						b.sortieEnClair = true;
						b.replie = false;
						ia.blocConfirmation = Pousser(ia, b);
						ia.motifConfirmation = pourquoi;
						ia.phase = NkPhaseIA::NK_CONFIRMATION;
						return;
					}
					Executer(c, ia, ap, ia.outilsNatifs && !ap.id.StartsWith("texte_"));
					++ia.prochain;
				}
				// Tous faits : les resultats repartent au modele.
				NkVector<NkMessageIA> &e = Echange(ia);
				for (usize i = 0; i < ia.resultats.Size(); ++i)
					e.PushBack(ia.resultats[i]);
				ia.resultats.Clear();
				ia.aExecuter.Clear();
				ia.prochain = 0u;
				++ia.etapes;
				if (ia.etapes >= ia.etapesMax) {
					char b[160];
					std::snprintf(b, sizeof(b), "La demande s'arrête après %d tours d'outils (un modèle qui tourne en rond ne bloque pas l'éditeur). Relancez si besoin.",
								  static_cast<int>(ia.etapesMax));
					Bloc(ia, NkAiBloc::Refus, NkString(), NkString(b));
					ia.phase = NkPhaseIA::NK_REPOS;
					return;
				}
				LancerTour(c, ia);
			}

			void TraiterReponse(NkEditeurCadre &c, NkEditeurIA &ia, const converse::NkReponseIA &rep) {
				if (!rep.ok) {
					if (rep.diag == converse::NkDiagIA::NK_OUTILS_REFUSES && ia.outilsNatifs) {
						ia.forceTexte[static_cast<usize>(ia.actif)] = 1u;
						Bloc(ia, NkAiBloc::Refus, NkString(),
							 NkString("Ce modèle ne sait pas les outils natifs : ils passent en blocs <outil> (format texte). Nouvel essai."));
						LancerTour(c, ia);
						return;
					}
					if (!rep.texte.Empty()) {
						const NkString vis = ia.outilsNatifs ? rep.texte : converse::NkIaTexteSansOutils(rep.texte);
						if (!vis.Empty() && ia.blocFlux == 0u)
							Bloc(ia, NkAiBloc::Prose, vis);
					}
					ia.derniereErreur = rep.erreur;
					if (rep.diag == converse::NkDiagIA::NK_ANNULE)
						Bloc(ia, NkAiBloc::Refus, NkString(), NkString("Arrêté à votre demande."));
					else {
						Bloc(ia, NkAiBloc::Echec, NkString(), rep.erreur.Empty() ? NkString("le fournisseur n'a rien rendu") : rep.erreur);
						if (rep.diag == converse::NkDiagIA::NK_SERVEUR_ABSENT || rep.diag == converse::NkDiagIA::NK_CLE_ABSENTE ||
							rep.diag == converse::NkDiagIA::NK_CLE_REFUSEE || rep.diag == converse::NkDiagIA::NK_TRANSPORT_ABSENT) {
							ia.motifs[static_cast<usize>(ia.actif)] = rep.erreur;
							DeclarerFournisseurs(ia);
						}
					}
					ia.phase = NkPhaseIA::NK_REPOS;
					return;
				}
				ia.motifs[static_cast<usize>(ia.actif)] = NkString();
				// Le texte FINAL (les blocs <outil> retires de ce qui se lit).
				const NkString vis = ia.outilsNatifs ? converse::NkIaTexteSansOutils(rep.texte) : converse::NkIaTexteSansOutils(rep.texte);
				if (!vis.Empty()) {
					if (ia.blocFlux != 0u) {
						if (NkAiBlocDonnees *b = ia.pan.Fil().MutableParId(ia.blocFlux))
							b->texte = vis;
					} else
						Bloc(ia, NkAiBloc::Prose, vis);
				}
				ia.blocFlux = 0u;
				// Les APPELS : natifs, sinon ceux du texte (un modele local, ou un
				// modele a outils natifs qui a prefere les ecrire).
				NkVector<NkAppelOutil> appels = rep.appels;
				const bool natifs = ia.outilsNatifs && !appels.Empty();
				if (appels.Empty())
					converse::NkIaOutilsDuTexte(rep.texte, appels);
				NkMessageIA m;
				m.role = NkRoleIA::NK_ASSISTANT;
				m.texte = rep.texte;
				if (natifs)
					m.appels = appels;
				Echange(ia).PushBack(m);
				if (appels.Empty()) {
					ia.phase = NkPhaseIA::NK_REPOS;
					ia.etapes = 0;
					return;
				}
				ia.aExecuter = appels;
				ia.prochain = 0u;
				ia.resultats.Clear();
				ExecuterSuivants(c, ia);
			}

			void LancerTour(NkEditeurCadre &c, NkEditeurIA &ia) {
				if (ia.fournisseurs.Empty()) {
					Bloc(ia, NkAiBloc::Echec, NkString(), NkString("Déclarez un fournisseur : Fenêtre > Réglages de l'IA."));
					ia.phase = NkPhaseIA::NK_REPOS;
					return;
				}
				const NkReglagesFournisseur &r = Actif(ia);
				const bool natif = ia.forceTexte[static_cast<usize>(ia.actif)] == 0u &&
								   converse::NkIaOutilsNatifs(r, OutilsModele(ia, ia.actif));
				converse::NkRequeteIA q;
				q.outilsNatifs = natif;
				NkMessageIA s;
				s.role = NkRoleIA::NK_SYSTEME;
				s.texte = NkEditeurIADescription(c.m, !natif);
				q.messages.PushBack(s);
				const NkVector<NkMessageIA> &e = Echange(ia);
				for (usize i = 0; i < e.Size(); ++i)
					q.messages.PushBack(e[i]);
				NkEditeurIAOutilsConverse(q.outils);
				NkString pq;
				if (!ia.envoi.Lancer(r, Cle(ia, r), q, pq)) {
					Bloc(ia, NkAiBloc::Echec, NkString(), pq);
					ia.phase = NkPhaseIA::NK_REPOS;
					return;
				}
				ia.outilsNatifs = natif;
				ia.phase = NkPhaseIA::NK_ATTENTE;
				ia.blocFlux = 0u;
				ia.morceauxVus = 0u;
			}

			// --- La fenetre des reglages : la forme <-> les reglages -------------------
			void VersForme(NkEditeurIA &ia, int32 i) {
				editorkit::NkAiReglagesVue &v = ia.vue;
				Tailles(ia);
				v.fournisseurs.Clear();
				for (usize k = 0; k < ia.fournisseurs.Size(); ++k)
					v.fournisseurs.PushBack(ia.fournisseurs[k].nom);
				if (v.genres.Empty())
					for (uint8 g = 0; g < static_cast<uint8>(converse::NkGenreFournisseur::NK_COUNT); ++g)
						v.genres.PushBack(NkString(converse::NkGenreFournisseurNom(static_cast<converse::NkGenreFournisseur>(g))));
				if (v.modesOutils.Empty()) {
					v.modesOutils.PushBack(NkString("Auto (natifs si le modèle les annonce)"));
					v.modesOutils.PushBack(NkString("Natifs (champ « tools » de l'API)"));
					v.modesOutils.PushBack(NkString("Texte (blocs <outil>, tout modele)"));
				}
				if (i < 0 || i >= static_cast<int32>(ia.fournisseurs.Size()))
					return;
				ia.vueIndex = i;
				v.choisi = i;
				v.actif = ia.actif;
				const NkReglagesFournisseur &r = ia.fournisseurs[static_cast<usize>(i)];
				v.genre = static_cast<int32>(r.genre);
				v.PoserChamp(v.nom, sizeof(v.nom), r.nom.CStr());
				v.PoserChamp(v.adresse, sizeof(v.adresse), r.adresse.CStr());
				v.PoserChamp(v.modele, sizeof(v.modele), r.modele.CStr());
				std::snprintf(v.temperature, sizeof(v.temperature), "%.2f", static_cast<double>(r.temperature));
				std::snprintf(v.contexte, sizeof(v.contexte), "%d", static_cast<int>(r.contexte));
				std::snprintf(v.sortie, sizeof(v.sortie), "%d", static_cast<int>(r.sortieMax));
				v.modeOutils = static_cast<int32>(r.outils);
				v.aide = NkString(converse::NkGenreFournisseurAide(r.genre));
				v.cleUtile = r.genre == converse::NkGenreFournisseur::NK_ANTHROPIC || r.genre == converse::NkGenreFournisseur::NK_OPENAI;
				v.adresseUtile = r.genre != converse::NkGenreFournisseur::NK_CLAUDE_CLI;
				NkString source;
				const NkString cle = Cle(ia, r, &source);
				v.cleInfo = cle.Empty() ? NkString() : converse::NkIaMasquerCle(cle) + "  (" + source + ")";
				v.cleSaisie = NkString();
				v.modeles.Clear();
				v.modelesDetail.Clear();
				const NkVector<converse::NkConverseModeleInfo> &l = ia.modeles[static_cast<usize>(i)];
				for (usize k = 0; k < l.Size(); ++k) {
					v.modeles.PushBack(l[k].nom);
					v.modelesDetail.PushBack(converse::NkConverseDecrireModele(l[k], r.Distant()));
				}
				v.message = ia.motifs[static_cast<usize>(i)];
				v.ton = v.message.Empty() ? 0u : 2u;
				v.liste = 0;
				v.focus = -1;
			}
			NkReglagesFournisseur DepuisForme(NkEditeurIA &ia) {
				editorkit::NkAiReglagesVue &v = ia.vue;
				NkReglagesFournisseur r = ia.fournisseurs[static_cast<usize>(ia.vueIndex)];
				r.genre = static_cast<converse::NkGenreFournisseur>(v.genre);
				r.nom = NkString(v.nom);
				r.adresse = NkString(v.adresse);
				r.modele = NkString(v.modele);
				r.nom.Trim();
				r.adresse.Trim();
				r.modele.Trim();
				r.temperature = static_cast<float32>(std::atof(v.temperature));
				r.contexte = static_cast<int32>(std::atoi(v.contexte));
				r.sortieMax = static_cast<int32>(std::atoi(v.sortie));
				r.outils = static_cast<converse::NkModeOutils>(v.modeOutils);
				return r;
			}
			void Enregistrer(NkEditeurIA &ia) {
				if (!ia.persister)
					return;
				converse::NkIaEcrireReglages(ia.dossier, ia.fournisseurs, Actif(ia).id);
			}
			void LancerSonde(NkEditeurIA &ia, int32 i, const NkReglagesFournisseur &r, const NkString &cle, bool tester) {
				ia.sondeIndex = i;
				ia.sonde.Lancer(r, cle, tester);
				if (ia.reglagesOuverts && ia.vueIndex == i) {
					ia.vue.occupe = true;
					ia.vue.message = NkString(tester ? "Test de la connexion…" : "Lecture de la liste des modèles…");
					ia.vue.ton = 3u;
				}
			}

		} // namespace

		// =====================================================================
		// DEMARRER
		// =====================================================================
		void NkEditeurIADemarrer(NkEditeurIA &ia, NkEditeurModele &m, const char *dossier) {
			ia.dossier = dossier && *dossier ? NkString(dossier) : converse::NkIaDossier();
			NkString actifId;
			converse::NkIaLireReglages(ia.dossier, ia.fournisseurs, actifId);
			ia.actif = 0;
			for (usize i = 0; i < ia.fournisseurs.Size(); ++i)
				if (ia.fournisseurs[i].id == actifId)
					ia.actif = static_cast<int32>(i);
			Tailles(ia);
			editorkit::NkAiPanneau &p = ia.pan;
			editorkit::NkAiCapacites cap = editorkit::NkAiCapacites::Texte();
			cap.produitOutil = true;
			cap.produitRefus = true;
			cap.produitEffet = true;
			p.capacites = cap;
			p.plafond = 200u;
			p.reglagesParHote = true;
			p.effortCrans.Clear(); // aucun « effort » : le budget est un reglage du fournisseur
			p.invite = NkString("Demandez à l'IA : « ajoute une caisse au centre », « écris le GDD »…");
			p.indication = NkString("L'assistant VOIT la scène et AGIT dessus : créer et régler des entités, écrire des scripts C++ "
									"et des Blueprints, organiser le Contenu, écrire le GDD. Chaque action se défait par Ctrl+Z ; "
									"supprimer ou écraser un fichier attend votre confirmation.");
			p.declaration = NkString("Fenêtre > Réglages de l'IA : Ollama, serveur compatible OpenAI, Claude (API ou CLI), modèle local.");
			p.commandes.Clear();
			{
				editorkit::NkAiCommandeDesc d;
				d.nom = NkString("Réglages des fournisseurs");
				d.detail = NkString("adresse, modèle, clé (hors du dépôt), tester la connexion");
				d.section = NkString("Modele");
				d.id = 1;
				p.commandes.PushBack(d);
			}
			{
				editorkit::NkAiCommandeDesc d;
				d.nom = NkString("Tester la connexion");
				d.detail = NkString("le fournisseur actif : serveur, modèle, clé");
				d.section = NkString("Modele");
				d.id = 2;
				p.commandes.PushBack(d);
			}
			{
				editorkit::NkAiCommandeDesc d;
				d.nom = NkString("Ce que l'IA voit");
				d.detail = NkString("le message système engendré par l'éditeur, maintenant");
				d.section = NkString("Contexte");
				d.id = 3;
				p.commandes.PushBack(d);
			}
			ia.echanges.Clear();
			ia.echanges.PushBack(NkVector<NkMessageIA>());
			DeclarerFournisseurs(ia);
			m.ia = &ia;
		}

		void NkEditeurIAChoisir(NkEditeurIA &ia, int32 i, const char *modele) {
			if (i < 0 || i >= static_cast<int32>(ia.fournisseurs.Size()))
				return;
			ia.actif = i;
			if (modele && *modele)
				ia.fournisseurs[static_cast<usize>(i)].modele = NkString(modele);
			DeclarerFournisseurs(ia);
			Enregistrer(ia);
		}

		void NkEditeurIAOuvrirReglages(NkEditeurIA &ia, int32 i) {
			ia.reglagesOuverts = true;
			VersForme(ia, i < 0 ? ia.actif : i);
		}

		bool NkEditeurReglagesIAOuverts(const NkEditeurModele &m) {
			return m.ia != nullptr && m.ia->reglagesOuverts;
		}

		// =====================================================================
		// ENVOYER, CONFIRMER
		// =====================================================================
		bool NkEditeurIAEnvoyer(NkEditeurCadre &c, const char *texte) {
			if (!c.m.ia || !texte || !*texte)
				return false;
			NkEditeurIA &ia = *c.m.ia;
			if (ia.phase != NkPhaseIA::NK_REPOS)
				return false;
			NkAiBlocDonnees d;
			d.type = NkAiBloc::Demande;
			d.texte = NkString(texte);
			Pousser(ia, d);
			NkMessageIA m;
			m.role = NkRoleIA::NK_UTILISATEUR;
			m.texte = NkString(texte);
			Echange(ia).PushBack(m);
			ia.etapes = 0;
			LancerTour(c, ia);
			return true;
		}

		void NkEditeurIAConfirmer(NkEditeurCadre &c, bool accepte) {
			if (!c.m.ia)
				return;
			NkEditeurIA &ia = *c.m.ia;
			if (ia.phase != NkPhaseIA::NK_CONFIRMATION || ia.prochain >= ia.aExecuter.Size())
				return;
			const NkAppelOutil ap = ia.aExecuter[ia.prochain];
			if (NkAiBlocDonnees *b = ia.pan.Fil().MutableParId(ia.blocConfirmation))
				b->texte = NkString(accepte ? "confirmé par vous" : "refusé par vous");
			ia.blocConfirmation = 0u;
			ia.phase = NkPhaseIA::NK_ATTENTE;
			const bool natif = ia.outilsNatifs && !ap.id.StartsWith("texte_");
			if (accepte)
				Executer(c, ia, ap, natif);
			else {
				++ia.actionsRefusees;
				NkMessageIA m;
				m.role = natif ? NkRoleIA::NK_OUTIL : NkRoleIA::NK_UTILISATEUR;
				m.idAppel = ap.id;
				m.nomOutil = ap.nom;
				const NkString t("REFUS : l'utilisateur a REFUSÉ cette action. Ne la refais pas sans qu'il la redemande.");
				m.texte = natif ? t : NkString("[resultat de l'outil ") + ap.nom + "]\n" + t;
				ia.resultats.PushBack(m);
			}
			++ia.prochain;
			ExecuterSuivants(c, ia);
		}

		// =====================================================================
		// LA TRAME
		// =====================================================================
		void NkEditeurIATrame(NkEditeurCadre &c) {
			if (!c.m.ia)
				return;
			NkEditeurIA &ia = *c.m.ia;
			Tailles(ia);
			// ── LA SONDE (liste, test) ──
			{
				converse::NkDiagIA d;
				NkString msg;
				NkVector<converse::NkConverseModeleInfo> l;
				bool test = false;
				if (ia.sonde.Recolter(d, msg, l, test) && ia.sondeIndex >= 0 && ia.sondeIndex < static_cast<int32>(ia.fournisseurs.Size())) {
					const usize i = static_cast<usize>(ia.sondeIndex);
					if (!l.Empty())
						ia.modeles[i] = l;
					ia.motifs[i] = d == converse::NkDiagIA::NK_OK ? NkString() : msg;
					// Aucun modele choisi : le premier que le serveur liste.
					if (ia.fournisseurs[i].modele.Empty() && !l.Empty() && !(ia.reglagesOuverts && ia.vueIndex == static_cast<int32>(i)))
						ia.fournisseurs[i].modele = l[0].nom;
					if (ia.reglagesOuverts && ia.vueIndex == static_cast<int32>(i)) {
						editorkit::NkAiReglagesVue &v = ia.vue;
						v.occupe = false;
						v.modeles.Clear();
						v.modelesDetail.Clear();
						for (usize k = 0; k < l.Size(); ++k) {
							v.modeles.PushBack(l[k].nom);
							v.modelesDetail.PushBack(converse::NkConverseDecrireModele(l[k], ia.fournisseurs[i].Distant()));
						}
						if (v.modele[0] == 0 && !l.Empty())
							v.PoserChamp(v.modele, sizeof(v.modele), l[0].nom.CStr());
						if (d == converse::NkDiagIA::NK_OK && !test) {
							char b[96];
							std::snprintf(b, sizeof(b), "%u modèle(s) lu(s) sur le serveur : choisissez-le dans la liste",
										  static_cast<unsigned>(l.Size()));
							v.message = NkString(b);
						} else
							v.message = msg;
						v.ton = d == converse::NkDiagIA::NK_OK ? 1u : 2u;
					}
					DeclarerFournisseurs(ia);
				}
			}
			// La liste du fournisseur actif, une fois, a la premiere ouverture du panneau.
			if (NkEditeurIAVisible(c.ui) && !ia.listeDemandee && !ia.sonde.EnCours() && !ia.fournisseurs.Empty()) {
				ia.listeDemandee = true;
				const NkReglagesFournisseur &r = Actif(ia);
				if (r.genre != converse::NkGenreFournisseur::NK_CLAUDE_CLI)
					LancerSonde(ia, ia.actif, r, Cle(ia, r), false);
			}
			// ── LE FLUX ──
			if (ia.phase == NkPhaseIA::NK_ATTENTE && ia.envoi.EnCours()) {
				uint32 morceaux = 0u;
				const NkString brut = ia.envoi.TexteEnCours(&morceaux);
				if (morceaux != ia.morceauxVus) {
					const NkString vis = converse::NkIaTexteSansOutils(brut);
					if (!vis.Empty()) {
						if (ia.blocFlux == 0u)
							ia.blocFlux = Bloc(ia, NkAiBloc::Prose, vis);
						else if (NkAiBlocDonnees *b = ia.pan.Fil().MutableParId(ia.blocFlux))
							b->texte = vis;
						++ia.imagesFlux;
					}
					ia.morceauxVus = morceaux;
				}
				converse::NkReponseIA rep;
				if (ia.envoi.Recolter(rep))
					TraiterReponse(c, ia, rep);
			}
			ia.pan.occupe = ia.phase != NkPhaseIA::NK_REPOS;
			// La disposition choisie (onglet, a part, replie) est RETENUE.
			NkEditeurIARetenirDisposition(ia, c.ui);
		}

		// =====================================================================
		// LA DISPOSITION : onglet, panneau a part, replie -- et RETENUE
		// =====================================================================
		namespace {
			NkString CheminDisposition(const NkEditeurIA &ia) {
				return (NkPath(ia.dossier.CStr()) / "unkeny_panneau.txt").ToString();
			}
			NkString TexteDisposition(const NkEditeurInterface &ui) {
				char b[160];
				std::snprintf(b, sizeof(b), "nkpanneauia 1\nplace = %s\nreplie = %d\nvoir = %d\nlargeur = %d\n",
							  ui.iaPlace == 1 ? "panneau" : "onglet", ui.iaReplie ? 1 : 0, ui.voirIA ? 1 : 0,
							  static_cast<int>(ui.largeurIA));
				return NkString(b);
			}
		} // namespace

		void NkEditeurIALireDisposition(NkEditeurIA &ia, NkEditeurInterface &ui) {
			const NkString f = CheminDisposition(ia);
			if (!NkFile::Exists(f.CStr()))
				return; // rien de retenu : le defaut (l'onglet du groupe Details | Monde)
			const NkString t = NkFile::ReadAllText(f.CStr());
			auto Valeur = [&](const char *cle) -> NkString {
				const NkString motif = NkString(cle) + " = ";
				const NkString::SizeType i = t.Find(motif.CStr());
				if (i == NkString::npos)
					return NkString();
				NkString::SizeType j = t.Find('\n', i);
				NkString v = t.SubStr(i + motif.Length(), (j == NkString::npos ? t.Length() : j) - i - motif.Length());
				v.Trim();
				return v;
			};
			ui.iaPlace = Valeur("place") == "panneau" ? 1 : 0;
			ui.iaReplie = Valeur("replie") == "1";
			ui.voirIA = Valeur("voir") != "0";
			const int32 l = Valeur("largeur").ToInt32(0);
			if (l >= 200)
				ui.largeurIA = static_cast<float32>(l);
			ia.dispositionEcrite = TexteDisposition(ui);
		}

		void NkEditeurIARetenirDisposition(NkEditeurIA &ia, const NkEditeurInterface &ui) {
			if (!ia.persister)
				return;
			const NkString t = TexteDisposition(ui);
			if (t == ia.dispositionEcrite)
				return;
			NkDirectory::CreateRecursive(ia.dossier.CStr());
			if (NkFile::WriteAllText(CheminDisposition(ia).CStr(), t.CStr()))
				ia.dispositionEcrite = t;
		}

		bool NkEditeurIAVisible(const NkEditeurInterface &ui) {
			return ui.iaPlace == 0 ? (ui.voirDetails && ui.ongletDroite == NK_ONGLET_IA) : (ui.voirIA && !ui.iaReplie);
		}

		// =====================================================================
		// LES ACTIONS DU MENU
		// =====================================================================
		bool NkEditeurActionIA(NkEditeurCadre &c, int32 action) {
			NkEditeurInterface &ui = c.ui;
			if (action == NK_A_VOIR_IA) {
				// En ONGLET : l'onglet IA vient au premier plan (et y renvoie a
				// Details s'il y etait deja). A PART : il s'ouvre, ou se deplie ;
				// ouvert et deplie, il se ferme.
				if (ui.iaPlace == 0) {
					ui.voirDetails = true;
					ui.ongletDroite = ui.ongletDroite == NK_ONGLET_IA ? 0 : NK_ONGLET_IA;
				} else if (!ui.voirIA || ui.iaReplie) {
					ui.voirIA = true;
					ui.iaReplie = false;
				} else
					ui.voirIA = false;
				return true;
			}
			if (action == NK_A_IA_PLACE) {
				if (ui.iaPlace == 0) {
					ui.iaPlace = 1; // DETACHER : un panneau a part, ouvert et deplie
					ui.voirIA = true;
					ui.iaReplie = false;
					if (ui.ongletDroite == NK_ONGLET_IA)
						ui.ongletDroite = 0;
				} else {
					ui.iaPlace = 0; // RATTACHER : l'onglet du groupe, au premier plan
					ui.voirDetails = true;
					ui.ongletDroite = NK_ONGLET_IA;
				}
				return true;
			}
			if (action == NK_A_IA_REPLIER) {
				if (ui.iaPlace == 1) {
					ui.voirIA = true;
					ui.iaReplie = !ui.iaReplie;
				}
				return true;
			}
			if (action == NK_A_REGLAGES_IA) {
				if (c.m.ia)
					NkEditeurIAOuvrirReglages(*c.m.ia, -1);
				else
					NkEditeurAnnoncer(c.m, "L'IA n'est pas démarrée dans cet éditeur");
				return true;
			}
			return false;
		}

		// =====================================================================
		// LE PANNEAU (son contenu, quel que soit l'endroit ou il vit)
		// =====================================================================
		namespace {
			/// Le chevron d'un panneau qui se replie vers la DROITE (« › ») ou se
			/// deplie vers la GAUCHE (« ‹ »), TRACE (la police n'a pas ces fleches
			/// en gras lisible).
			void ChevronHorizontal(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, bool versDroite, const nkgui::NkColor &col) {
				const float32 d = versDroite ? 1.f : -1.f;
				dl.AddLine(nkgui::NkVec2{cx - 2.5f * d, cy - 5.f}, nkgui::NkVec2{cx + 2.5f * d, cy}, col, 1.8f);
				dl.AddLine(nkgui::NkVec2{cx + 2.5f * d, cy}, nkgui::NkVec2{cx - 2.5f * d, cy + 5.f}, col, 1.8f);
			}

			/// Le CONTENU du panneau IA dans `r` : le panneau du kit, et ses gestes.
			void DessinerContenu(NkEditeurCadre &c, const nkgui::NkRect &zone, const nkgui::NkRect &r) {
				NkEditeurInterface &ui = c.ui;
				if (!c.m.ia) {
					nkgui::NkGuiFont *f = c.police;
					if (f && f->Valid())
						c.ctx.dl.AddText(f->Face(), f->TexId(), {r.x + 12.f, r.y + 24.f}, "L'IA n'est pas démarrée dans cet éditeur.",
										 c.pal.attenue);
					return;
				}
				NkEditeurIA &ia = *c.m.ia;
				editorkit::NkAiPanneau &pan = ia.pan;
				// LES BOUTONS D'UN BLOC : la confirmation en attente d'abord ; sinon
				// « Annuler cette action » sur le dernier effet, TANT QUE rien d'autre
				// n'a ete retenu depuis (sinon il defairait le geste de quelqu'un d'autre).
				pan.actions = editorkit::NkAiActionsFil{};
				if (ia.phase == NkPhaseIA::NK_CONFIRMATION && ia.blocConfirmation != 0u) {
					pan.actions.blocId = ia.blocConfirmation;
					pan.actions.libelle[0] = "Confirmer";
					pan.actions.libelle[1] = "Refuser";
				} else if (ia.blocEffet != 0u && c.m.historique.annuler.Size() == ia.historiqueApresEffet &&
						   !c.m.historique.annuler.Empty()) {
					pan.actions.blocId = ia.blocEffet;
					pan.actions.libelle[0] = "Annuler cette action (Ctrl+Z)";
				}
				pan.echelle = 1.f;
				pan.maintenant = static_cast<float64>(ui.temps);
				pan.phase = ui.temps - static_cast<float32>(static_cast<int32>(ui.temps));
				// UN CLIC AILLEURS REND LE CLAVIER A L'EDITEUR : sans cela, apres une
				// demande, Ctrl+Z dans la vue partait au composeur (et n'annulait pas
				// l'action de l'IA qu'on voulait defaire).
				if (c.ctx.input.mouseClicked[0] && !NkEditeurDans(zone, c.ctx.input.mousePos) && pan.ComposeurActif(c.ctx))
					c.ctx.inputId = nkgui::NKGUI_ID_NONE;
				editorkit::NkGuiComponentPaint pc(c.ctx, c.theme);
				pc.PoserPolices(nullptr, nullptr, nullptr);
				const editorkit::NkAiSorties out = pan.Dessiner(c.ctx, pc, {r.x, r.y, r.w, r.h}, true);

				if (out.envoyer && !out.texte.Empty()) {
					if (NkEditeurIAEnvoyer(c, out.texte.CStr()))
						pan.ViderSaisie();
				}
				if (out.arreter) {
					if (ia.phase == NkPhaseIA::NK_ATTENTE) {
						ia.envoi.Annuler();
						Bloc(ia, NkAiBloc::Refus, NkString(), NkString("Arrêté à votre demande."));
					} else if (ia.phase == NkPhaseIA::NK_CONFIRMATION) {
						Bloc(ia, NkAiBloc::Refus, NkString(), NkString("Arrêté : l'action en attente n'a pas été faite."));
						ia.aExecuter.Clear();
						ia.resultats.Clear();
						ia.prochain = 0u;
						ia.blocConfirmation = 0u;
					}
					ia.phase = NkPhaseIA::NK_REPOS;
				}
				if (out.actionBloc != 0u) {
					if (out.actionBloc == ia.blocConfirmation && ia.phase == NkPhaseIA::NK_CONFIRMATION)
						NkEditeurIAConfirmer(c, out.actionIndice == 0);
					else if (out.actionBloc == ia.blocEffet && out.actionIndice == 0) {
						NkEditeurExecuter(c, NK_A_ANNULER);
						if (NkAiBlocDonnees *b = pan.Fil().MutableParId(ia.blocEffet))
							b->effet = b->texte + " — ANNULÉE (Ctrl+Y la refait)";
						ia.blocEffet = 0u;
					}
				}
				if (out.ajouterIa)
					NkEditeurIAOuvrirReglages(ia, -1);
				if (out.fournisseurChange && out.nouveau >= 0)
					NkEditeurIAChoisir(ia, out.nouveau);
				if (out.modeleChange) {
					const int32 a = pan.Actif();
					if (a >= 0 && a < static_cast<int32>(pan.fournisseurs.Size())) {
						const editorkit::NkAiFournisseurDesc &f = pan.fournisseurs[static_cast<usize>(a)];
						if (f.modele >= 0 && f.modele < static_cast<int32>(f.modeles.Size()))
							NkEditeurIAChoisir(ia, a, f.modeles[static_cast<usize>(f.modele)].nom.CStr());
					}
				}
				if (out.nouvelle)
					(void)Echange(ia); // le chat neuf a son echange vide
				if (out.commande == 1)
					NkEditeurIAOuvrirReglages(ia, -1);
				if (out.commande == 2 && !ia.fournisseurs.Empty()) {
					NkEditeurIAOuvrirReglages(ia, -1);
					const NkReglagesFournisseur &rf = Actif(ia);
					LancerSonde(ia, ia.actif, rf, Cle(ia, rf), true);
				}
				if (out.commande == 3) {
					const bool natif = ia.fournisseurs.Empty() ? true
															   : (ia.forceTexte[static_cast<usize>(ia.actif)] == 0u &&
																  converse::NkIaOutilsNatifs(Actif(ia), OutilsModele(ia, ia.actif)));
					NkAiBlocDonnees b;
					b.type = NkAiBloc::Outil;
					b.titre = NkString("Ce que l'IA voit");
					b.texte = NkString(natif ? "le message système (outils natifs)" : "le message système (outils en texte)");
					b.entree = Borne(NkEditeurIADescription(c.m, !natif), 6000);
					b.etiquetteEntree = NkString("Système");
					Pousser(ia, b);
				}
			}
		} // namespace

		// --- EN ONGLET, dans le groupe Details | Monde -----------------------------
		void NkEditeurDessinerIADans(NkEditeurCadre &c, const nkgui::NkRect &zone) {
			// Une barre fine : de quoi DETACHER le panneau, et ses reglages.
			// ⚠️ PAS DE « ⚙ » NI DE « ✕ » : la police de l'editeur ne les a pas (la
			//    premiere capture les montrait en « ? »). Des mots, et le « × » latin.
			const float32 hBarre = 26.f;
			c.ctx.dl.AddRectFilled(nkgui::NkRect{zone.x, zone.y, zone.w, hBarre}, c.pal.entete);
			c.ui.iaBoutonPlace = nkgui::NkRect{zone.x + zone.w - 160.f, zone.y + 2.f, 86.f, 22.f};
			if (NkEditeurBouton(c, c.ui.iaBoutonPlace, "Détacher"))
				NkEditeurExecuter(c, NK_A_IA_PLACE);
			if (NkEditeurBouton(c, nkgui::NkRect{zone.x + zone.w - 70.f, zone.y + 2.f, 66.f, 22.f}, "Réglages") && c.m.ia)
				NkEditeurIAOuvrirReglages(*c.m.ia, -1);
			if (c.m.ia && c.police && c.police->Valid()) {
				const NkEditeurIA &ia = *c.m.ia;
				const NkString lieu = ia.fournisseurs.Empty() ? NkString("aucun fournisseur")
															  : ia.fournisseurs[static_cast<usize>(ia.actif < 0 ? 0 : ia.actif)].nom;
				const float32 y = zone.y + hBarre * 0.5f - c.police->LineHeight() * 0.5f + c.police->Ascent();
				c.ctx.dl.PushClipRect(nkgui::NkRect{zone.x + 8.f, zone.y, zone.w - 176.f, hBarre}, true);
				c.ctx.dl.AddText(c.police->Face(), c.police->TexId(), {zone.x + 10.f, y}, lieu.CStr(), c.pal.attenue);
				c.ctx.dl.PopClipRect();
			}
			DessinerContenu(c, zone, nkgui::NkRect{zone.x, zone.y + hBarre, zone.w, zone.h - hBarre});
		}

		// --- A PART, a droite de tout (deplie ou replie) ----------------------------
		void NkEditeurDessinerIA(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			if (ui.iaPlace != 1 || !ui.voirIA || ui.ia.w < 8.f || ui.ia.h < 80.f)
				return;
			const nkgui::NkRect zone = ui.ia;
			c.ctx.dl.AddRectFilled(zone, c.pal.panneau);
			const float32 hOnglet = 26.f;
			if (ui.iaReplie) {
				// REPLIE : une bande, son chevron « ‹ » en tete, « IA » ecrit lettre a
				// lettre ; un clic n'importe ou sur la bande le deplie. Un point
				// d'accent dit qu'un tour est en vol (ou qu'une confirmation attend).
				c.ctx.dl.AddRectFilled(nkgui::NkRect{zone.x, zone.y, zone.w, zone.h}, c.pal.entete);
				c.ctx.dl.AddRectFilled(nkgui::NkRect{zone.x, zone.y, 1.f, zone.h}, c.pal.bord);
				ui.iaBoutonRepli = nkgui::NkRect{zone.x + 2.f, zone.y + 2.f, zone.w - 4.f, 22.f};
				const bool survol = NkEditeurDans(zone, c.ctx.input.mousePos);
				if (survol)
					c.ctx.dl.AddRectFilled(ui.iaBoutonRepli, c.pal.boutonSurvol, 2.f);
				ChevronHorizontal(c.ctx.dl, zone.x + zone.w * 0.5f, zone.y + 13.f, false, survol ? c.pal.texte : c.pal.attenue);
				if (c.police && c.police->Valid()) {
					static const char *kLettres[2] = {"I", "A"};
					for (int32 k = 0; k < 2; ++k) {
						const float32 w = c.police->MeasureWidth(kLettres[k]);
						c.ctx.dl.AddText(c.police->Face(), c.police->TexId(),
										 {zone.x + (zone.w - w) * 0.5f, zone.y + 48.f + static_cast<float32>(k) * 16.f}, kLettres[k],
										 c.pal.texte);
					}
				}
				if (c.m.ia && c.m.ia->phase != NkPhaseIA::NK_REPOS)
					c.ctx.dl.AddCircleFilled(nkgui::NkVec2{zone.x + zone.w * 0.5f, zone.y + 92.f}, 4.f,
											 c.m.ia->phase == NkPhaseIA::NK_CONFIRMATION ? c.pal.selection : c.pal.accent);
				if (survol && c.ctx.input.mouseClicked[0]) {
					c.ctx.input.mouseClicked[0] = false;
					NkEditeurExecuter(c, NK_A_IA_REPLIER);
				}
				return;
			}
			// DEPLIE : l'onglet du panneau ; Rattacher (en onglet), Reglages, le
			// chevron « › » qui le replie, la croix qui le ferme.
			static const char *kOnglet[1] = {"Assistant IA"};
			int32 seul = 0;
			const float32 wBoutons = 92.f + 70.f + 26.f + 26.f;
			NkEditeurOnglets(c, nkgui::NkRect{zone.x, zone.y, zone.w - wBoutons, hOnglet}, kOnglet, 1, seul);
			c.ctx.dl.AddRectFilled(nkgui::NkRect{zone.x + zone.w - wBoutons, zone.y, wBoutons, hOnglet}, c.pal.entete);
			float32 x = zone.x + zone.w - wBoutons + 2.f;
			ui.iaBoutonPlace = nkgui::NkRect{x, zone.y + 2.f, 88.f, 22.f};
			if (NkEditeurBouton(c, ui.iaBoutonPlace, "Rattacher"))
				NkEditeurExecuter(c, NK_A_IA_PLACE);
			x += 92.f;
			if (NkEditeurBouton(c, nkgui::NkRect{x, zone.y + 2.f, 66.f, 22.f}, "Réglages") && c.m.ia)
				NkEditeurIAOuvrirReglages(*c.m.ia, -1);
			x += 70.f;
			ui.iaBoutonRepli = nkgui::NkRect{x, zone.y + 2.f, 23.f, 22.f};
			if (NkEditeurBouton(c, ui.iaBoutonRepli, ""))
				NkEditeurExecuter(c, NK_A_IA_REPLIER);
			ChevronHorizontal(c.ctx.dl, ui.iaBoutonRepli.x + ui.iaBoutonRepli.w * 0.5f, ui.iaBoutonRepli.y + 11.f, true, c.pal.texte);
			x += 26.f;
			if (NkEditeurBouton(c, nkgui::NkRect{x, zone.y + 2.f, 23.f, 22.f}, "×")) {
				ui.voirIA = false;
				return;
			}
			DessinerContenu(c, zone, nkgui::NkRect{zone.x, zone.y + hOnglet, zone.w, zone.h - hOnglet});
		}

		// =====================================================================
		// LA FENETRE DES REGLAGES
		// =====================================================================
		void NkEditeurDessinerReglagesIA(NkEditeurCadre &c) {
			if (!c.m.ia || !c.m.ia->reglagesOuverts)
				return;
			NkEditeurIA &ia = *c.m.ia;
			if (ia.fournisseurs.Empty()) {
				converse::NkIaFournisseursParDefaut(ia.fournisseurs);
				VersForme(ia, 0);
			}
			if (ia.vueIndex >= static_cast<int32>(ia.fournisseurs.Size()))
				VersForme(ia, 0);
			editorkit::NkAiReglagesCouleurs k;
			k.fond = c.pal.fond;
			k.panneau = c.pal.panneau;
			k.entete = c.pal.entete;
			k.bord = c.pal.bord;
			k.champ = c.pal.champ;
			k.bouton = c.pal.bouton;
			k.boutonSurvol = c.pal.boutonSurvol;
			k.texte = c.pal.texte;
			k.attenue = c.pal.attenue;
			k.accent = c.pal.accent;
			k.surAccent = c.pal.surAccent;
			// Le voile sur l'editeur : la fenetre est modale.
			c.ctx.dlOverlay.AddRectFilled(c.ui.ecran, nkgui::NkColor{0, 0, 0, 90});
			ia.vue.actif = ia.actif;
			const editorkit::NkAiReglagesSorties out = editorkit::NkAiDessinerReglages(c.ctx, c.ctx.dlOverlay, c.police, ia.vue, c.ui.ecran, k);
			c.ui.toucheChamp = c.ui.toucheChamp || out.champFocus;
			if (out.genreChange) {
				const converse::NkGenreFournisseur g = static_cast<converse::NkGenreFournisseur>(ia.vue.genre);
				NkReglagesFournisseur &r = ia.fournisseurs[static_cast<usize>(ia.vueIndex)];
				const NkString ancienDefaut(converse::NkAdresseParDefaut(r.genre));
				const NkString adresse(ia.vue.adresse);
				if (adresse.Empty() || adresse == ancienDefaut)
					ia.vue.PoserChamp(ia.vue.adresse, sizeof(ia.vue.adresse), converse::NkAdresseParDefaut(g));
				ia.vue.aide = NkString(converse::NkGenreFournisseurAide(g));
				ia.vue.cleUtile = g == converse::NkGenreFournisseur::NK_ANTHROPIC || g == converse::NkGenreFournisseur::NK_OPENAI;
				ia.vue.adresseUtile = g != converse::NkGenreFournisseur::NK_CLAUDE_CLI;
				ia.vue.modeles.Clear();
				ia.vue.modelesDetail.Clear();
				ia.vue.message = NkString("Genre changé : « Actualiser » lit la liste des modèles de ce serveur.");
				ia.vue.ton = 0u;
			}
			const int32 i = ia.vueIndex;
			if (out.choisir >= 0) {
				ia.fournisseurs[static_cast<usize>(i)] = DepuisForme(ia);
				VersForme(ia, out.choisir);
				return;
			}
			if (out.actualiser || out.tester) {
				const NkReglagesFournisseur r = DepuisForme(ia);
				const NkString cle = ia.vue.cleSaisie.Empty() ? Cle(ia, r) : ia.vue.cleSaisie;
				LancerSonde(ia, i, r, cle, out.tester);
			}
			if (out.effacerCle) {
				NkString pq;
				converse::NkIaEcrireCle(ia.dossier, ia.fournisseurs[static_cast<usize>(i)].id, NkString(), pq);
				NkString source;
				const NkString cle = Cle(ia, ia.fournisseurs[static_cast<usize>(i)], &source);
				ia.vue.cleInfo = cle.Empty() ? NkString() : converse::NkIaMasquerCle(cle) + "  (" + source + ")";
				ia.vue.message = cle.Empty() ? NkString("Clé effacée du dossier de l'utilisateur.")
											 : NkString("Le fichier est effacé, mais une variable d'environnement fournit encore une clé.");
				ia.vue.ton = 1u;
			}
			if (out.enregistrer || out.utiliser) {
				NkReglagesFournisseur r = DepuisForme(ia);
				if (r.id.Empty())
					r.id = converse::NkIaIdSur(r.nom.CStr());
				ia.fournisseurs[static_cast<usize>(i)] = r;
				NkString pq;
				bool cleOk = true;
				if (!ia.vue.cleSaisie.Empty() && ia.persister) {
					cleOk = converse::NkIaEcrireCle(ia.dossier, r.id, ia.vue.cleSaisie, pq);
					ia.vue.cleSaisie = NkString();
				}
				if (out.utiliser)
					ia.actif = i;
				Enregistrer(ia);
				ia.motifs[static_cast<usize>(i)] = NkString();
				DeclarerFournisseurs(ia);
				VersForme(ia, i);
				ia.vue.message = cleOk ? NkString(out.utiliser ? "Fournisseur utilisé par le panneau ; réglages enregistrés dans " : "Enregistré dans ") +
											 ia.dossier + " (AUCUNE clé dans ce fichier : elles sont dans cles/)."
									   : pq;
				ia.vue.ton = cleOk ? 1u : 2u;
			}
			if (out.ajouter) {
				ia.fournisseurs[static_cast<usize>(i)] = DepuisForme(ia);
				NkReglagesFournisseur n;
				char id[48];
				std::snprintf(id, sizeof(id), "fournisseur-%u", static_cast<unsigned>(ia.fournisseurs.Size() + 1u));
				n.id = NkString(id);
				n.genre = converse::NkGenreFournisseur::NK_OLLAMA;
				n.nom = NkString("Ollama (autre PC du réseau)");
				n.adresse = NkString("http://192.168.1.20:11434");
				ia.fournisseurs.PushBack(n);
				Tailles(ia);
				VersForme(ia, static_cast<int32>(ia.fournisseurs.Size()) - 1);
				ia.vue.message = NkString("Corrigez l'adresse (l'IP de l'autre PC), puis « Actualiser ».");
				ia.vue.ton = 0u;
			}
			if (out.supprimer && ia.fournisseurs.Size() > 1u) {
				NkString pq;
				converse::NkIaEcrireCle(ia.dossier, ia.fournisseurs[static_cast<usize>(i)].id, NkString(), pq);
				ia.fournisseurs.Erase(ia.fournisseurs.Begin() + i);
				ia.modeles.Erase(ia.modeles.Begin() + i);
				ia.motifs.Erase(ia.motifs.Begin() + i);
				ia.forceTexte.Erase(ia.forceTexte.Begin() + i);
				if (ia.actif >= static_cast<int32>(ia.fournisseurs.Size()) || ia.actif == i)
					ia.actif = 0;
				else if (ia.actif > i)
					--ia.actif;
				Enregistrer(ia);
				DeclarerFournisseurs(ia);
				VersForme(ia, i > 0 ? i - 1 : 0);
			}
			if (out.fermer) {
				ia.fournisseurs[static_cast<usize>(ia.vueIndex)] = DepuisForme(ia);
				ia.reglagesOuverts = false;
				ia.vue.cleSaisie = NkString(); // une cle tapee et non enregistree ne survit pas
				DeclarerFournisseurs(ia);
			}
		}

	} // namespace editeur
} // namespace nkentseu
