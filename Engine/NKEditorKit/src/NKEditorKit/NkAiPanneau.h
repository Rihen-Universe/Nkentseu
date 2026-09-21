#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkAiPanneau.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LE PANNEAU IA, UN SEUL, pour NKCode, NK3DModeler, NKUIDesign et les
//          suivantes : en-tete, fil, composeur, menus, et UNE conversation par
//          assistant. Les applications le REMPLISSENT ; elles ne le dessinent pas.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// LA DEMANDE (Rodolf, 20/09 puis 21/09 a 04h)
//   « qu'il s'agisse de NKCode, NK3DModeler, NKUIDesign ou n'importe laquelle de
//     nos applications qui utilise une IA, je veux que le panneau soit
//     exactement comme ceci : meme design, meme emplacement dans sa pastille,
//     meme comportement »
//   Et le constat du 21/09 : l'en-tete et le composeur du kit existaient avec
//   ZERO appelant ; chaque application gardait son propre panneau (onglets
//   Local/Claude/Ollama et boutons Envoyer/Fermer dans le modeleur, « Passer a
//   Claude » et « Outils avances » dans NKUIDesign). Trois panneaux conformes
//   sur le papier, trois ecrans differents.
//
// CE QUE CE FICHIER FAIT, ET POURQUOI IL EXISTE EN PLUS DU PLAN
//   `NkAiThreadLayout.h` sait OU vont les choses ; `NkAiThreadPaint.h` sait les
//   PEINDRE. Il manquait celui qui TIENT le panneau : quel assistant est
//   choisi, quelle conversation il a, ce qui est tape, quel menu est ouvert, ce
//   qu'un clic veut dire. Sans lui, chaque application le reecrivait -- et
//   c'est exactement ainsi que trois panneaux ont diverge.
//
// ⚠️ UNE CONVERSATION PAR ASSISTANT (Rodolf, 21/09 : « quitter d'un assistant
//    a l'autre affiche le meme contenu, ce n'est pas normal »). Chaque
//    fournisseur a SON fil, SON sujet, SON historique. Changer de fournisseur
//    ramene SA conversation.
//    ⚠️ ET ON NE CHANGE PAS PENDANT UN TOUR. Une reponse attendue arriverait
//       dans le fil de l'assistant qu'on vient de choisir -- une reponse juste,
//       rangee chez le mauvais interlocuteur, et rien ne le dirait. Le menu le
//       REFUSE en le disant (le motif s'affiche sur la ligne).
//
// ⚠️ PAS D'ONGLETS (§6 de la spec, 20/09). Le choix du fournisseur PUIS du
//    modele vit dans la pastille de la barre du bas -- la ou la capture met
//    `Opus 5 (1M) High` -- avec « Ajouter une IA… » en bas. Ajouter = DECLARER :
//    le panneau ne saisit AUCUNE cle (aucun coffre ; une cle passee en ligne de
//    commande serait lisible dans la table des processus).
//
// ⚠️ « RIEN NE QUITTE CETTE MACHINE » EST UNE PROPRIETE DU FOURNISSEUR, PLUS
//    UNE PHRASE EN DUR. La pastille « lieu » dit `local` (point vert) ou
//    `distant` (point orange) d'apres le descripteur ACTIF -- le negatif de la
//    spec (distant choisi + « locale » affichee) est impossible a ecrire ici.
//
// CE QUE LA CAPTURE MONTRE ET QUI N'A PAS DE SENS CHEZ NOUS -- DIT, PAS OMIS
//   - « 3 agents » : nous n'avons jamais plus d'un tour en vol. A sa place, la
//     pastille LIEU (local / distant), qui dit une chose vraie et utile.
//   - le micro : aucune reconnaissance vocale dans le depot. Absent.
//   - « Auto » : chez nous la pastille de MODE ; elle n'existe que si l'hote
//     declare des modes (NKUIDesign : generer / proposer / discuter, §7).
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkAiThreadLayout.h"
#include "NKEditorKit/NkAiThreadPaint.h"
#include "NKGui/NKGui.h"
#include <cstdio>

namespace nkentseu {
	namespace editorkit {

		/// Un modele d'un fournisseur (« qwen2.5-coder 7B », detail « Q4 »).
		struct NkAiModeleDesc {
				NkString nom;
				NkString detail; ///< UNE ligne : taille, lieu, a quoi il sert (065128)
				/// ⚠️ UNE PROPRIETE SANS OBJET POUR CE MODELE : son MOTIF, non vide. Le
				///    panneau GRISE la ligne et ecrit ce motif ; il ne la cache pas (Q5).
				NkString motifEffort;
				NkString motifPensee;
		};

		/// UN FOURNISSEUR, tel que l'hote le declare. ⚠️ `pret` et `motif` sont
		/// l'AVIS DE L'HOTE, relu a chaque image : le panneau ne redecide pas si
		/// un dorsal est joignable -- deux avis finissent par diverger.
		struct NkAiFournisseurDesc {
				NkString cle; ///< identifiant STABLE (« local », « claude ») -- jamais le nom affiche
				NkString nom; ///< ce qui s'affiche
				bool distant = false;
				bool pret = true;
				NkString motif; ///< pourquoi il n'est pas pret, avec le geste qui repare
				NkVector<NkAiModeleDesc> modeles;
				int32 modele = 0;
		};

		/// UN MODE (le geste de NKUIDesign, l'autorisation de NKCode).
		/// ⚠️ `produit` : ce geste PRODUIT-il un document ? C'est l'exigence liante
		///    du §7 : la surface dit, SANS QU'ON L'ESSAIE, quel geste produit et
		///    lequel discute. Le bouton d'envoi est orange s'il produit, bleu s'il
		///    discute, et l'invite du composeur le dit en toutes lettres.
		struct NkAiModeDesc {
				NkString nom;
				NkString detail;
				NkString invite;
				bool produit = true;
		};

		/// Une commande du bouton « / » : ce qu'elle insere dans le composeur.
		struct NkAiCommandeDesc {
				NkString nom;
				NkString detail;
				NkString insertion;
				/// La section du menu « / » (065041) : « Contexte », « Modele », « Aide »
				/// ont leur place fixe ; toute autre devient sa propre section.
				NkString section;
				/// >= 0 : l'HOTE execute (joindre un fichier, revenir en arriere) et le
				/// panneau le lui DIT par `NkAiSorties::commande` au lieu d'inserer.
				int32 id = -1;
				NkString motif; ///< non vide = grisee, et voila pourquoi
		};

		/// Une entree du menu « + » (065237) : ce que l'hote sait joindre.
		struct NkAiEntreeDesc {
				NkString nom;
				NkString detail;
				int32 id = 0;
				NkString motif; ///< non vide = grisee, et voila pourquoi
		};

		/// UNE LIGNE D'UNE FENETRE (Utilisation, Carte des agents).
		/// genre : 0 section, 1 libelle + valeur, 2 libelle + valeur + barre, 3 note.
		struct NkAiFenetreLigne {
				uint8 genre = 1;
				NkString libelle;
				NkString valeur;
				float32 part = 0.f; ///< 0..1, pour une barre
				NkRole role = NkRole::AccentUi;
		};

		/// UNE FENETRE posee sur le fil (065201, 065228). L'hote la REMPLIT tant
		/// qu'elle est ouverte ; le panneau la met en page et la ferme.
		struct NkAiFenetre {
				NkString titre;
				NkVector<NkAiFenetreLigne> lignes;
				void Vider() {
					lignes.Clear();
				}
				void Section(const char *t) {
					NkAiFenetreLigne l;
					l.genre = 0;
					l.libelle = NkString(t);
					lignes.PushBack(l);
				}
				void Valeur(const char *l1, const char *v) {
					NkAiFenetreLigne l;
					l.libelle = NkString(l1);
					l.valeur = NkString(v ? v : "");
					lignes.PushBack(l);
				}
				void Barre(const char *l1, const char *v, float32 part, NkRole role = NkRole::AccentUi) {
					NkAiFenetreLigne l;
					l.genre = 2;
					l.libelle = NkString(l1);
					l.valeur = NkString(v ? v : "");
					l.part = part;
					l.role = role;
					lignes.PushBack(l);
				}
				void Note(const char *t) {
					NkAiFenetreLigne l;
					l.genre = 3;
					l.libelle = NkString(t);
					lignes.PushBack(l);
				}
		};

		struct NkAiArchive {
				NkAiFil fil;
				NkString sujet;
		};

		/// LA CONVERSATION D'UN FOURNISSEUR.
		struct NkAiConversation {
				NkAiFil fil;
				float32 defile = 0.f;
				bool colle = true; ///< suit le bas tant qu'on n'a pas remonte
				float64 debut = -1.0;
				NkVector<NkAiArchive> archives;
		};

		/// CE QUE LE PANNEAU DEMANDE A L'HOTE, cette image. Le panneau n'appelle
		/// AUCUN dorsal : il DIT ce que l'utilisateur a voulu, l'hote execute.
		struct NkAiSorties {
				bool envoyer = false; ///< `texte` part, avec le mode `mode`
				NkString texte;
				int32 mode = 0;
				bool arreter = false; ///< le bouton d'arret pendant un tour
				uint32 actionBloc = 0;
				int32 actionIndice = -1; ///< le bouton `actionIndice` du bloc `actionBloc`
				bool plus = false;		 ///< le « + » (l'hote ouvre ce qu'il sait joindre)
				bool commandes = false;	 ///< le « / » quand l'hote tient sa propre liste
				bool historique = false; ///< l'horloge quand l'hote tient son propre historique
				bool nouvelle = false;	 ///< une conversation neuve vient d'etre ouverte
				bool fournisseurChange = false;
				int32 ancien = -1, nouveau = -1;
				bool modeleChange = false;
				bool modeChange = false;
				bool ajouterIa = false;
				uint32 bascule = 0; ///< le bloc qui vient d'etre plie / deplie
				// ── 21/09, Q5 ──
				bool effortChange = false; ///< `effort` a change : la PROCHAINE requete le porte
				bool penserChange = false; ///< `penser` a change
				int32 commande = -1;	   ///< l'`id` d'une commande « / » que l'hote execute
				int32 entreePlus = -1;	   ///< l'`id` d'une entree « + »
				uint8 fenetre = 0;		   ///< 1 = Utilisation, 2 = Carte des agents : vient d'ouvrir
				bool fenetreFermee = false;
				/// Le panneau a PRIS un clic de la souris cette image : l'hote ne doit
				/// pas le relayer a ce qui est dessous.
				bool clicPris = false;
		};

		enum class NkAiMenu : uint8 { Aucun = 0, Fournisseurs, Modeles, Modes, Commandes, Historique, AjouterIa, Plus };

		class NkAiPanneau {
			public:
				// ════════════ CE QUE L'HOTE DECLARE ════════════
				NkVector<NkAiFournisseurDesc> fournisseurs;
				NkVector<NkAiModeDesc> modes;
				NkVector<NkAiCommandeDesc> commandes;
				/// Ce que l'hote sait produire -- pose dans CHAQUE fil (voir `NkAiCapacites`).
				NkAiCapacites capacites = NkAiCapacites::Texte();
				/// La fenetre glissante du fil. 0 = celle du kit (200). ⚠️ DECLAREE, pas
				/// heritee : le modeleur en a paye le silence (16 -> 200, le 20/09).
				uint32 plafond = 0;
				bool portePlus = false;			///< l'hote sait joindre quelque chose
				bool historiqueParHote = false; ///< l'hote tient son historique (NKCode, persistant)
				bool commandesParHote = false;	///< l'hote tient sa liste de commandes
				/// L'hote sait METTRE EN FILE une demande tapee pendant un tour (NKCode).
				/// Sans elle, pendant un tour, l'envoi devient un bouton d'ARRET -- une
				/// fleche qui refuserait serait un geste qui ment.
				bool fileAttente = false;
				NkString invite = NkString("Posez votre question…");
				NkString indication; ///< ce qu'un fil vide dit de ce que l'assistant sait faire
				/// Ou DECLARER une IA de plus. Affiche par « Ajouter une IA… ».
				NkString declaration;
				/// Le menu « + ». Vide = le « + » rend `NkAiSorties::plus` a l'hote.
				NkVector<NkAiEntreeDesc> entreesPlus;

				// ════════════ LES PROPRIETES DU MODELE (Q5) ════════════
				// ⚠️ ELLES DOIVENT AGIR. Le panneau ne connait aucun dorsal : il TIENT
				//    le reglage et le SIGNALE (`effortChange`, `penserChange`) ; l'hote
				//    le traduit dans SA requete (num_predict, think…). Un reglage qui
				//    ne changerait pas les octets envoyes serait pire qu'absent.
				int32 effort = 3;				 ///< le cran courant de `effortCrans`
				NkVector<NkString> effortCrans; ///< vide ou 1 cran = pas de curseur
				bool penser = true;				 ///< montrer le raisonnement (modele qui l'annonce)
				NkAiFenetre utilisation;		 ///< remplie par l'hote tant qu'elle est ouverte
				NkAiFenetre carte;				 ///< idem : les etapes du tour

				// ════════════ CE QUE L'HOTE POSE A CHAQUE IMAGE ════════════
				bool occupe = false;	   ///< un tour est en vol
				float64 maintenant = -1.0; ///< secondes, horloge de l'hote ; < 0 = pas de duree
				float32 phase = 0.f;	   ///< l'indicateur d'activite (0..1), horloge de l'hote
				float32 echelle = 1.f;	   ///< l'echelle d'interface de l'hote
				NkAiActionsFil actions;	   ///< les boutons d'UN bloc

				// ════════════ CE QUE LE PANNEAU PUBLIE ════════════
				NkAiPlan planChrome; ///< en-tete, composeur, menus : coordonnees du PANNEAU
				NkAiPlan planFil;	 ///< le fil : coordonnees du CONTENU (0 = haut du fil)
				NkPaintRect rect;	 ///< le panneau, tel que peint la derniere fois
				NkPaintRect vueFil;	 ///< la fenetre du fil a l'ecran
				float32 filOx = 0.f, filOy = 0.f; ///< l'origine du plan du fil a l'ecran

				NkAiPanneau() {
					mConv.Resize(1);
					effortCrans.PushBack(NkString("Bas"));
					effortCrans.PushBack(NkString("Moyen"));
					effortCrans.PushBack(NkString("Haut"));
					effortCrans.PushBack(NkString("Max"));
				}
				/// La fenetre ouverte : 0 aucune, 1 Utilisation, 2 Carte des agents.
				uint8 FenetreOuverte() const {
					return mFenetre;
				}
				void OuvrirFenetre(uint8 f) {
					mFenetre = f;
				}
				const char *Filtre() const {
					return mFiltre;
				}
				/// Pose le filtre du « / » SANS clavier : la porte des sondes.
				void PoserFiltre(const char *t) {
					uint32 n = 0;
					if (t)
						for (; t[n] && n + 1u < (uint32)sizeof(mFiltre); ++n)
							mFiltre[n] = t[n];
					mFiltre[n] = 0;
				}

				// ── LA CONVERSATION VIVANTE ─────────────────────────────────────
				/// ⚠️ LE FIL VIVANT PEUT APPARTENIR A L'HOTE. NK3DModeler ecrit dans
				///    `st.aiFil` depuis quarante sites (boucle, recolte, sondes) : le
				///    deplacer ici toucherait le code d'un autre chantier. `Lier` dit
				///    au panneau « la conversation ACTIVE vit la » ; changer d'assistant
				///    RANGE ce fil dans la case de l'ancien et y POSE celui du nouveau.
				void Lier(NkAiFil *vivant) {
					mVivant = vivant;
				}
				/// ⚠️ LES CAPACITES SONT POSEES A L'ACCES, PAS SEULEMENT A LA PEINTURE.
				///    Un hote qui pousse un bloc AVANT la premiere image (une reponse
				///    recoltee panneau ferme) le verrait refuse par un fil qui ne sait
				///    encore rien produire -- et le bloc serait perdu sans bruit.
				NkAiFil &Fil() {
					Assurer();
					NkAiFil &f = mVivant ? *mVivant : mConv[(usize)mActif].fil;
					f.Declarer(capacites);
					return f;
				}
				const NkAiFil &Fil() const {
					return mVivant ? *mVivant : mConv[(usize)mActif].fil;
				}
				/// Le fil du fournisseur `i` (le vivant si c'est l'actif).
				NkAiFil &FilDe(int32 i) {
					Assurer();
					if (i == mActif || i < 0 || i >= (int32)mConv.Size())
						return Fil();
					mConv[(usize)i].fil.Declarer(capacites);
					return mConv[(usize)i].fil;
				}
				int32 Actif() const {
					return mActif;
				}
				const NkAiFournisseurDesc *FournisseurActif() const {
					return (mActif >= 0 && mActif < (int32)fournisseurs.Size()) ? &fournisseurs[(usize)mActif] : nullptr;
				}

				/// LE SUJET : la premiere demande de la conversation, comme la capture.
				const char *Sujet() const {
					const NkAiFil &f = Fil();
					for (uint32 i = 0; i < f.Taille(); ++i)
						if (f.At(i).type == NkAiBloc::Demande)
							return f.At(i).texte.CStr();
					return "Nouvelle conversation";
				}

				/// Change d'assistant. Rend faux ET NOMME la raison.
				bool Choisir(int32 i, NkString &pourquoi) {
					Assurer();
					if (i < 0 || i >= (int32)fournisseurs.Size()) {
						pourquoi = NkString("aucun assistant a cet indice");
						return false;
					}
					if (i == mActif)
						return true;
					if (occupe) {
						pourquoi = NkString("une reponse est attendue : elle arriverait dans la conversation de "
											"l'autre assistant");
						return false;
					}
					if (mVivant) {
						mConv[(usize)mActif].fil = *mVivant;
						*mVivant = mConv[(usize)i].fil;
					}
					mActif = i;
					return true;
				}

				/// Une conversation NEUVE pour l'assistant courant. L'ancienne est
				/// ARCHIVEE d'abord : rien ne disparait sans recours.
				void NouvelleConversation() {
					Assurer();
					NkAiFil &f = Fil();
					NkAiConversation &c = mConv[(usize)mActif];
					if (f.Taille() > 0) {
						if (c.archives.Size() >= 8u)
							c.archives.Erase(c.archives.Begin());
						NkAiArchive a;
						a.fil = f;
						a.sujet = NkString(Sujet());
						c.archives.PushBack(a);
					}
					f.Vider();
					c.defile = 0.f;
					c.colle = true;
					c.debut = -1.0;
				}
				/// Rouvre l'archive `k` de l'assistant courant (la courante est
				/// archivee a sa place : rouvrir ne fait rien perdre).
				void Rouvrir(int32 k) {
					Assurer();
					NkAiConversation &c = mConv[(usize)mActif];
					if (k < 0 || k >= (int32)c.archives.Size())
						return;
					NkAiArchive a = c.archives[(usize)k];
					c.archives.Erase(c.archives.Begin() + k);
					NouvelleConversation();
					Fil() = a.fil;
					c.colle = true;
				}
				/// Vide l'historique de l'assistant courant. ⚠️ ATTEIGNABLE : un historique
				/// qu'on ne peut pas vider est une dette de vie privee. La conversation
				/// OUVERTE n'est pas touchee.
				void ViderHistorique() {
					Assurer();
					mConv[(usize)mActif].archives.Clear();
				}
				/// Le sujet de l'archive `k` (0 = la plus ancienne).
				const char *SujetArchive(uint32 k) const {
					if (mActif < 0 || mActif >= (int32)mConv.Size() || k >= mConv[(usize)mActif].archives.Size())
						return "";
					return mConv[(usize)mActif].archives[k].sujet.CStr();
				}
				uint32 Archives() const {
					return (mActif >= 0 && mActif < (int32)mConv.Size()) ? (uint32)mConv[(usize)mActif].archives.Size() : 0u;
				}

				// ── LE COMPOSEUR ────────────────────────────────────────────────
				char *Saisie() {
					return mSaisie;
				}
				uint32 CapaciteSaisie() const {
					return (uint32)sizeof(mSaisie);
				}
				void ViderSaisie() {
					mSaisie[0] = 0;
					mCaret = 0;
				}
				void PoserSaisie(const char *t) {
					uint32 n = 0;
					if (t)
						for (; t[n] && n + 1u < (uint32)sizeof(mSaisie); ++n)
							mSaisie[n] = t[n];
					mSaisie[n] = 0;
					mCaret = (int32)n;
				}
				/// L'identifiant de focus du composeur dans NKGui (`ctx.inputId`).
				nkgui::NkGuiId IdComposeur() const {
					return nkgui::NkGuiHashPtr(this, 0x41495041u);
				}
				bool ComposeurActif(const nkgui::NkGuiContext &ctx) const {
					return ctx.inputId == IdComposeur();
				}
				NkAiMenu MenuOuvert() const {
					return mMenu;
				}
				void FermerMenus() {
					mMenu = NkAiMenu::Aucun;
					mFiltre[0] = 0;
				}
				/// Ouvre un menu SANS souris : la porte des sondes, qui ne peuvent
				/// injecter aucune entree sur la machine de Rodolf. Elle ecrit
				/// l'etat qu'un clic ecrirait, pas un second comportement.
				void OuvrirMenu(NkAiMenu m, int32 fournisseur = -1) {
					mMenu = m;
					if (fournisseur >= 0)
						mMenuFournisseur = fournisseur;
				}

				// ════════════ L'IMAGE ════════════
				/// Mesure, traite la souris et le clavier, peint. `pointeurLibre` : la
				/// souris est a ce panneau cette image (aucun menu de l'hote dessus).
				NkAiSorties Dessiner(nkgui::NkGuiContext &ctx, NkComponentPaint &p, const NkPaintRect &r,
									 bool pointeurLibre) {
					NkAiSorties out;
					Assurer();
					rect = r;
					NkAiFil &fil = Fil();
					fil.Declarer(capacites);
					if (plafond > 0u)
						fil.PoserPlafond(plafond);
					NkAiConversation &conv = mConv[(usize)mActif];
					if (fil.Taille() == 0)
						conv.debut = -1.0;
					else if (conv.debut < 0.0 && maintenant >= 0.0)
						conv.debut = maintenant;

					NkAiMetriques m;
					m.Echelle(echelle);
					const bool focus = ComposeurActif(ctx);

					// ── 1. LE CLAVIER, AVANT LA MESURE : on mesure ce qui est tape ──
					if (focus)
						Clavier(ctx, out);

					// ── 2. LA MESURE ──
					planChrome.Vider();
					NkAiEnteteDecl ed;
					ed.porteHistorique = true;
					ed.porteNouvelle = true;
					const char *sujet = Sujet();
					const float32 hEntete = NkAiEnteteMesurer(sujet, r.w, 0.f, ed, m, planChrome, &Mesure, &p);

					char duree[24];
					duree[0] = 0;
					if (conv.debut >= 0.0 && maintenant >= conv.debut)
						FormaterDuree(maintenant - conv.debut, duree, sizeof(duree));
					const NkAiFournisseurDesc *fa = FournisseurActif();
					NkString modele;
					NkString modeleDetail;
					if (fa) {
						// LA PASTILLE DE LA CAPTURE : « Opus 5 (1M) High » = le MODELE et
						// son EFFORT. Le fournisseur se lit au menu et dans la pastille
						// de lieu ; la description du modele, au menu.
						modele = fa->nom;
						if (fa->modele >= 0 && fa->modele < (int32)fa->modeles.Size()) {
							const NkAiModeleDesc &ma = fa->modeles[(usize)fa->modele];
							modele = ma.nom;
							if (ma.motifEffort.Length() == 0 && effort >= 0 && effort < (int32)effortCrans.Size() &&
								effortCrans.Size() > 1u)
								modeleDetail = effortCrans[(usize)effort];
						}
					}
					const NkAiModeDesc *md = (mode >= 0 && mode < (int32)modes.Size()) ? &modes[(usize)mode] : nullptr;
					NkAiComposeurDecl cd;
					cd.texte = mSaisie;
					cd.invite = (md && md->invite.Length() > 0) ? md->invite.CStr() : invite.CStr();
					cd.caret = focus ? mCaret : -1;
					cd.portePlus = portePlus || entreesPlus.Size() > 0;
					cd.porteCommandes = commandesParHote || commandes.Size() > 0;
					cd.occupe = occupe;
					cd.arretSeul = occupe && (!fileAttente || Blanc(mSaisie));
					cd.duree = duree[0] ? duree : nullptr;
					cd.lieu = fa ? (fa->distant ? "distant" : "local") : nullptr;
					cd.distant = fa && fa->distant;
					cd.modele = modele.Length() > 0 ? modele.CStr() : nullptr;
					cd.modeleDetail = modeleDetail.Length() > 0 ? modeleDetail.CStr() : nullptr;
					cd.mode = md ? md->nom.CStr() : nullptr;
					cd.envoiActif = !Blanc(mSaisie) && (!fa || fa->pret);
					cd.envoiProduit = md ? md->produit : true;
					const float32 yComposeur = NkAiComposeurMesurer(cd, r.w, r.h, m, &Mesure, &p, planChrome);

					// ── LE FIL, DANS SA FENETRE ──
					vueFil = {r.x, r.y + hEntete, r.w, yComposeur - hEntete - 6.f};
					if (vueFil.h < 0.f)
						vueFil.h = 0.f;
					NkAiFilMesurer(fil, r.w, m, &Mesure, &p, planFil, actions.blocId ? &actions : nullptr);
					const float32 hContenu = planFil.Hauteur();
					const float32 maxDefile = hContenu > vueFil.h ? hContenu - vueFil.h : 0.f;
					const float32 mx = ctx.input.mousePos.x, my = ctx.input.mousePos.y;
					const bool dansVue = pointeurLibre && vueFil.Contains(mx, my) && mMenu == NkAiMenu::Aucun;
					if (mMenu != NkAiMenu::Aucun && pointeurLibre && ctx.input.wheel != 0.f &&
						mMenuRect.Contains(mx - r.x, my - r.y))
						mMenuDefile -= ctx.input.wheel * m.menuLigne * 2.f;
					if (dansVue && ctx.input.wheel != 0.f) {
						conv.defile -= ctx.input.wheel * m.ligne * 2.f;
						conv.colle = false;
					}
					if (conv.colle)
						conv.defile = maxDefile;
					if (conv.defile > maxDefile)
						conv.defile = maxDefile;
					if (conv.defile < 0.f)
						conv.defile = 0.f;
					if (conv.defile >= maxDefile - 1.f)
						conv.colle = true;
					filOx = r.x;
					filOy = vueFil.y - conv.defile;
					if (fil.Taille() == 0 && indication.Length() > 0)
						(void)NkAiPublierTexte(planChrome, 0u, indication.CStr(), true, m.retraitFil, hEntete + m.entreBlocs,
											   r.w - m.retraitFil - m.margeDroite, m.ligne, NkRole::TextMuted, 12u, m,
											   &Mesure, &p, NkAiSource::Indication);

					// ── LA DEMANDE EPINGLEE (la capture : la question du tour reste en
					//    tete pendant que ses etapes defilent dessous) ──
					// ⚠️ UNE COPIE DES PIECES PUBLIEES, PAS UN SECOND CALCUL : la bulle
					//    est deplacee d'un bloc, ses fragments gardent leurs tranches.
					{
						float32 meilleurY = -1.f;
						uint32 idDem = 0u;
						for (uint32 i = 0; i < planFil.Pieces(); ++i) {
							const NkAiRectPublie &q = planFil.Piece(i);
							if (q.piece == NkAiPiece::Cadre && q.y < conv.defile && q.y > meilleurY) {
								meilleurY = q.y;
								idDem = q.blocId;
							}
						}
						if (idDem != 0u) {
							const float32 dy = (hEntete + m.entreBlocs * 0.75f) - meilleurY;
							for (uint32 i = 0; i < planFil.Pieces(); ++i) {
								NkAiRectPublie q = planFil.Piece(i);
								if (q.blocId != idDem)
									continue;
								q.y += dy;
								planChrome.Ajouter(q);
							}
							mEpingle = idDem;
						} else
							mEpingle = 0u;
					}

					// ── 3. LA FENETRE, PUIS LES MENUS PAR-DESSUS ──
					PublierFenetre(m, r, hEntete, yComposeur, p);
					PublierMenu(m, r, yComposeur, hEntete, p);
					if (mFiltreValider) {
						// ENTREE dans le filtre = la premiere action qui passe
						mFiltreValider = false;
						for (uint32 i = 0; i < (uint32)mLignes.Size(); ++i)
							if (mLignes[i].genre == 0 && !mLignes[i].eteint && mLignes[i].quoi != QRien) {
								ChoisirLigne((int32)i, 0.f, out);
								break;
							}
					}

					// ── 4. LA SOURIS ──
					Souris(ctx, r, pointeurLibre, m, out);

					// ── 5. LA PEINTURE ──
					NkAiChromeTextes ch;
					ch.titre = sujet;
					ch.saisie = mSaisie;
					ch.invite = cd.invite;
					ch.duree = cd.duree;
					ch.lieu = cd.lieu;
					ch.modele = cd.modele;
					ch.modeleDetail = cd.modeleDetail;
					ch.mode = cd.mode;
					ch.indication = indication.CStr();
					ch.menu = mMenuPtr.Size() ? mMenuPtr.Data() : nullptr;
					ch.menuDetail = mMenuDetPtr.Size() ? mMenuDetPtr.Data() : nullptr;
					ch.menuN = (uint32)mMenuPtr.Size();
					ch.menuDroite = mMenuDroitePtr.Size() ? mMenuDroitePtr.Data() : nullptr;
					ch.filtre = mFiltre;
					ch.filtreInvite = "Filtrer les actions…";
					ch.fenetreTextes = mFenPtr.Size() ? mFenPtr.Data() : nullptr;
					ch.fenetreN = (uint32)(mFenPtr.Size() / 2u);
					ch.actions = &actions;
					p.Fill(r, (uint16)NkRole::PanelBg, 0.f);
					p.PushClip(vueFil);
					NkAiFilPeindre(p, fil, planFil, filOx, filOy, ch, phase);
					p.PopClip();
					p.PushClip(r);
					NkAiFilPeindre(p, fil, planChrome, r.x, r.y, ch, phase);
					p.PopClip();
					// LA MARQUE « non vu » se consomme LA OU LES BLOCS SONT PEINTS.
					fil.MarquerVus();
					out.mode = mode;
					return out;
				}

				int32 mode = 0;

			private:
				// ── LA MESURE, SERVIE PAR LE PEINTRE ─────────────────────────────
				// ⚠️ LE PLAN MESURE AVEC LES POLICES DU PEINTRE : celles qui peindront.
				//    Une mesure faite dans une autre police replierait les lignes a
				//    un autre endroit que la peinture -- des mots coupes au milieu.
				static float32 Mesure(void *ctx, NkAiPolice pol, const char *t) {
					return ctx ? ((NkComponentPaint *)ctx)->LargeurPolice(t, nullptr, (uint8)pol) : 0.f;
				}

				void Assurer() {
					usize n = fournisseurs.Size() > 0 ? fournisseurs.Size() : 1u;
					if (mConv.Size() < n)
						mConv.Resize(n);
					if (mActif >= (int32)n)
						mActif = 0;
				}

				static bool Blanc(const char *s) {
					if (!s)
						return true;
					for (; *s; ++s)
						if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r')
							return false;
					return true;
				}

				static void FormaterDuree(float64 s, char *out, usize cap) {
					const int32 t = (int32)s;
					if (t < 60)
						snprintf(out, cap, "%ds", (int)t);
					else if (t < 3600)
						snprintf(out, cap, "%dm", (int)(t / 60));
					else
						snprintf(out, cap, "%dh%02d", (int)(t / 3600), (int)((t % 3600) / 60));
				}

				// ── LE CLAVIER DU COMPOSEUR ─────────────────────────────────────
				// ⚠️ UTF-8 DE BOUT EN BOUT. Le champ du kit (`NkOverlayTextField`)
				//    refuse tout ce qui n'est pas ASCII (cp >= 127) : « modélise une
				//    chaise » y perdait ses accents. Un panneau francais ne peut pas
				//    s'en contenter.
				void Clavier(nkgui::NkGuiContext &ctx, NkAiSorties &out) {
					auto &in = ctx.input;
					if (mMenu == NkAiMenu::Commandes) {
						FiltreClavier(ctx);
						return;
					}
					int32 len = 0;
					while (mSaisie[len])
						++len;
					if (mCaret > len || mCaret < 0)
						mCaret = len;
					const int32 cap = (int32)sizeof(mSaisie);
					auto inserer = [&](const char *s, int32 n) {
						if (len + n + 1 > cap)
							return;
						for (int32 k = len; k >= mCaret; --k)
							mSaisie[k + n] = mSaisie[k];
						for (int32 k = 0; k < n; ++k)
							mSaisie[mCaret + k] = s[k];
						mCaret += n;
						len += n;
					};
					auto precedent = [&](int32 c) {
						if (c <= 0)
							return 0;
						--c;
						while (c > 0 && ((unsigned char)mSaisie[c] & 0xC0u) == 0x80u)
							--c;
						return c;
					};
					auto suivant = [&](int32 c) {
						if (c >= len)
							return len;
						c += (int32)aidetail::LongueurCp((unsigned char)mSaisie[c]);
						return c > len ? len : c;
					};
					auto effacer = [&](int32 a, int32 b) {
						if (b <= a)
							return;
						const int32 n = b - a;
						for (int32 k = a; k + n <= len; ++k)
							mSaisie[k] = mSaisie[k + n];
						len -= n;
						mCaret = a;
					};
					for (int32 i = 0; i < in.charCount; ++i) {
						const uint32 cp = in.chars[i];
						if (cp < 32u || cp == 127u)
							continue;
						// « / » EN TETE D'UNE DEMANDE VIDE ouvre le menu, comme la capture
						if (cp == (uint32)'/' && len == 0 && !commandesParHote) {
							mMenu = NkAiMenu::Commandes;
							mFiltre[0] = 0;
							continue;
						}
						char u[4];
						int32 n = 0;
						if (cp < 0x80u)
							u[n++] = (char)cp;
						else if (cp < 0x800u) {
							u[n++] = (char)(0xC0u | (cp >> 6));
							u[n++] = (char)(0x80u | (cp & 0x3Fu));
						} else if (cp < 0x10000u) {
							u[n++] = (char)(0xE0u | (cp >> 12));
							u[n++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
							u[n++] = (char)(0x80u | (cp & 0x3Fu));
						} else {
							u[n++] = (char)(0xF0u | (cp >> 18));
							u[n++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
							u[n++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
							u[n++] = (char)(0x80u | (cp & 0x3Fu));
						}
						inserer(u, n);
					}
					if (in.KeyPressedRepeat(nkgui::NkGuiKey::Backspace))
						effacer(precedent(mCaret), mCaret);
					if (in.KeyPressedRepeat(nkgui::NkGuiKey::Delete))
						effacer(mCaret, suivant(mCaret));
					if (in.KeyPressedRepeat(nkgui::NkGuiKey::Left))
						mCaret = precedent(mCaret);
					if (in.KeyPressedRepeat(nkgui::NkGuiKey::Right))
						mCaret = suivant(mCaret);
					if (in.KeyPressed(nkgui::NkGuiKey::Home))
						mCaret = 0;
					if (in.KeyPressed(nkgui::NkGuiKey::End))
						mCaret = len;
					if (in.wantPaste) {
						const NkString cb = ctx.GetClipboard();
						for (const char *s = cb.CStr(); s && *s; ++s)
							if (*s != '\r')
								inserer(s, 1);
						in.wantPaste = false;
					}
					if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
						if (mMenu != NkAiMenu::Aucun)
							mMenu = NkAiMenu::Aucun;
						else
							ctx.inputId = nkgui::NKGUI_ID_NONE;
					}
					if (in.KeyPressed(nkgui::NkGuiKey::Enter)) {
						// MAJ+ENTREE = une ligne de plus ; ENTREE = envoyer. C'est la
						// convention de la capture, et celle de NKCode.
						if (in.shiftDown)
							inserer("\n", 1);
						else
							Envoyer(out);
					}
				}

				void FiltreClavier(nkgui::NkGuiContext &ctx) {
					auto &in = ctx.input;
					int32 n = 0;
					while (mFiltre[n])
						++n;
					for (int32 i = 0; i < in.charCount; ++i) {
						const uint32 cp = in.chars[i];
						if (cp < 32u || cp == 127u || cp >= 0x80u)
							continue; // le filtre compare en ASCII
						if (n + 2 < (int32)sizeof(mFiltre)) {
							mFiltre[n++] = (char)cp;
							mFiltre[n] = 0;
						}
					}
					if (in.KeyPressedRepeat(nkgui::NkGuiKey::Backspace) && n > 0)
						mFiltre[--n] = 0;
					if (in.KeyPressed(nkgui::NkGuiKey::Escape))
						FermerMenus();
					if (in.KeyPressed(nkgui::NkGuiKey::Enter))
						mFiltreValider = true;
				}

				void Envoyer(NkAiSorties &out) {
					if ((occupe && !fileAttente) || Blanc(mSaisie))
						return;
					const NkAiFournisseurDesc *fa = FournisseurActif();
					if (fa && !fa->pret)
						return; // l'envoi est ETEINT : la pastille dit pourquoi
					out.envoyer = true;
					out.texte = NkString(mSaisie);
					out.mode = mode;
				}

				// ── LES MENUS ───────────────────────────────────────────────────
				// ⚠️ UNE LIGNE DE MENU DIT CE QU'ELLE FAIT (`quoi`), PAS SON RANG. Le
				//    rang d'une ligne change avec le filtre du « / » et avec les
				//    sections : un clic lu par son indice dans une liste filtree
				//    declencherait la commande d'a cote.
				enum Quoi : uint8 {
					QRien = 0,
					QFournisseur,
					QRetour,
					QModele,
					QAjouterIa,
					QMode,
					QCommande,
					QNouvelle,
					QChangerModele,
					QEffort,
					QPenser,
					QFenetre,
					QPlus,
					QArchive,
					QViderHist
				};
				/// genre : 0 action, 1 titre de section, 2 interrupteur, 3 curseur, 4 note
				struct LigneMenu {
						NkString texte, detail, droite;
						uint8 genre = 0;
						bool actif = false, eteint = false, deuxLignes = false, valeur = false;
						uint8 quoi = QRien;
						int32 arg = 0;
				};

				const NkAiModeleDesc *ModeleActif() const {
					const NkAiFournisseurDesc *f = FournisseurActif();
					if (!f || f->modele < 0 || f->modele >= (int32)f->modeles.Size())
						return nullptr;
					return &f->modeles[(usize)f->modele];
				}

				static bool Contient(const NkString &botte, const char *aiguille) {
					if (!aiguille || !aiguille[0])
						return true;
					const char *h = botte.CStr();
					for (; h && *h; ++h) {
						const char *a = h, *b = aiguille;
						while (*a && *b) {
							char x = *a, y = *b;
							if (x >= 'A' && x <= 'Z')
								x = (char)(x - 'A' + 'a');
							if (y >= 'A' && y <= 'Z')
								y = (char)(y - 'A' + 'a');
							if (x != y)
								break;
							++a;
							++b;
						}
						if (!*b)
							return true;
					}
					return false;
				}

				void LigneEffort(bool avecTitre) {
					// ⚠️ GRISEE AVEC SON MOTIF, JAMAIS CACHEE (Q5) : un modele pour
					//    lequel l'effort ne change rien le DIT sur la ligne.
					LigneMenu l;
					l.genre = 3;
					l.quoi = QEffort;
					l.texte = NkString("Effort");
					const NkAiModeleDesc *ma = ModeleActif();
					l.eteint = ma && ma->motifEffort.Length() > 0;
					const int32 nc = (int32)effortCrans.Size();
					if (l.eteint) {
						// LE MOTIF SE LIT EN ENTIER, sous le libelle : a 276 px il etait
						// coupe a « la boucle » -- un motif tronque ne dit plus rien.
						l.detail = ma->motifEffort;
						l.deuxLignes = true;
					} else if (effort >= 0 && effort < nc) {
						l.droite = NkString("(");
						l.droite.Append(effortCrans[(usize)effort].CStr());
						l.droite.Append(")");
					}
					(void)avecTitre;
					mLignes.PushBack(l);
				}
				void LignePenser() {
					LigneMenu l;
					l.genre = 2;
					l.quoi = QPenser;
					l.texte = NkString("Thinking");
					const NkAiModeleDesc *ma = ModeleActif();
					l.eteint = ma && ma->motifPensee.Length() > 0;
					l.valeur = penser && !l.eteint;
					l.detail = l.eteint ? ma->motifPensee : NkString("montrer le raisonnement du modele");
					l.deuxLignes = l.eteint;
					mLignes.PushBack(l);
				}
				void Section(const char *t) {
					LigneMenu l;
					l.genre = 1;
					l.texte = NkString(t);
					mLignes.PushBack(l);
				}
				void Note(const char *t) {
					LigneMenu l;
					l.genre = 4;
					l.eteint = true;
					l.texte = NkString(t);
					mLignes.PushBack(l);
				}

				void Remplir() {
					mLignes.Clear();
					switch (mMenu) {
						case NkAiMenu::Fournisseurs:
							Section("Assistant");
							for (usize i = 0; i < fournisseurs.Size(); ++i) {
								const NkAiFournisseurDesc &f = fournisseurs[i];
								LigneMenu l;
								l.quoi = QFournisseur;
								l.arg = (int32)i;
								l.texte = f.nom;
								l.deuxLignes = true;
								char d[64];
								snprintf(d, sizeof(d), "%s · %u modele(s)", f.distant ? "distant" : "local",
										 (unsigned)f.modeles.Size());
								l.detail = f.pret ? NkString(d) : (f.motif.Length() ? f.motif : NkString("indisponible"));
								if ((int32)i == mRefusFournisseur && mRefus.Length() > 0)
									l.detail = mRefus;
								l.actif = (int32)i == mActif;
								mLignes.PushBack(l);
							}
							{
								LigneMenu l;
								l.quoi = QAjouterIa;
								l.texte = NkString("Ajouter une IA…");
								l.detail = NkString("declarer, sans cle");
								l.deuxLignes = true;
								mLignes.PushBack(l);
							}
							break;
						case NkAiMenu::Modeles: {
							const int32 fi = mMenuFournisseur;
							if (fi < 0 || fi >= (int32)fournisseurs.Size())
								break;
							const NkAiFournisseurDesc &f = fournisseurs[(usize)fi];
							LigneMenu r;
							r.quoi = QRetour;
							r.texte = NkString("‹  ");
							r.texte.Append(f.nom.CStr());
							mLignes.PushBack(r);
							Section("Choisir un modele");
							for (usize k = 0; k < f.modeles.Size(); ++k) {
								LigneMenu l;
								l.quoi = QModele;
								l.arg = (int32)k;
								l.texte = f.modeles[k].nom;
								l.detail = f.modeles[k].detail;
								l.deuxLignes = true;
								l.actif = fi == mActif && (int32)k == f.modele;
								mLignes.PushBack(l);
							}
							if (f.modeles.Size() == 0)
								Note(f.motif.Length() ? f.motif.CStr() : "Aucun modele declare par ce fournisseur.");
							if (effortCrans.Size() > 1u)
								LigneEffort(true);
							break;
						}
						case NkAiMenu::Modes:
							Section("Mode");
							for (usize i = 0; i < modes.Size(); ++i) {
								LigneMenu l;
								l.quoi = QMode;
								l.arg = (int32)i;
								l.texte = modes[i].nom;
								l.detail = modes[i].detail;
								l.deuxLignes = modes[i].detail.Length() > 0;
								l.actif = (int32)i == mode;
								mLignes.PushBack(l);
							}
							if (effortCrans.Size() > 1u)
								LigneEffort(true);
							break;
						case NkAiMenu::Commandes: {
							// LES SECTIONS DE LA CAPTURE (065041) : Contexte, Modele, ce que
							// l'hote declare, Reglages, Aide. Le FILTRE garde une section
							// tant qu'une de ses lignes passe.
							const char *flt = mFiltre;
							NkVector<LigneMenu> brut;
							auto sec = [&](const char *t) {
								LigneMenu l;
								l.genre = 1;
								l.texte = NkString(t);
								brut.PushBack(l);
							};
							auto hote = [&](const char *s) {
								for (usize i = 0; i < commandes.Size(); ++i) {
									const NkAiCommandeDesc &c = commandes[i];
									const bool ici = s ? (c.section == NkString(s))
													   : !(c.section == NkString("Contexte") || c.section == NkString("Aide") ||
														   c.section == NkString("Modele"));
									if (!ici)
										continue;
									LigneMenu l;
									l.quoi = QCommande;
									l.arg = (int32)i;
									l.texte = c.nom;
									l.detail = c.motif.Length() ? c.motif : c.detail;
									l.eteint = c.motif.Length() > 0;
									brut.PushBack(l);
								}
							};
							sec("Contexte");
							{
								LigneMenu l;
								l.quoi = QNouvelle;
								l.texte = NkString("Nouvelle conversation");
								l.detail = NkString("la courante est archivee");
								l.eteint = occupe;
								if (occupe)
									l.detail = NkString("une reponse est attendue");
								brut.PushBack(l);
							}
							hote("Contexte");
							sec("Modele");
							{
								LigneMenu l;
								l.quoi = QChangerModele;
								l.texte = NkString("Changer de modele");
								const NkAiModeleDesc *ma = ModeleActif();
								if (ma)
									l.droite = ma->nom;
								brut.PushBack(l);
							}
							{
								const usize avant = mLignes.Size();
								if (effortCrans.Size() > 1u)
									LigneEffort(false);
								LignePenser();
								for (usize k = avant; k < mLignes.Size(); ++k)
									brut.PushBack(mLignes[k]);
								mLignes.Clear();
							}
							hote("Modele");
							// les sections propres a l'hote, dans l'ordre de leur declaration
							{
								NkVector<NkString> vues;
								for (usize i = 0; i < commandes.Size(); ++i) {
									const NkString &s = commandes[i].section;
									if (s == NkString("Contexte") || s == NkString("Aide") || s == NkString("Modele"))
										continue;
									bool deja = false;
									for (usize k = 0; k < vues.Size(); ++k)
										deja = deja || vues[k] == s;
									if (deja)
										continue;
									vues.PushBack(s);
									sec(s.Length() ? s.CStr() : "Actions");
									for (usize j = 0; j < commandes.Size(); ++j) {
										if (!(commandes[j].section == s))
											continue;
										LigneMenu l;
										l.quoi = QCommande;
										l.arg = (int32)j;
										l.texte = commandes[j].nom;
										l.detail = commandes[j].motif.Length() ? commandes[j].motif : commandes[j].detail;
										l.eteint = commandes[j].motif.Length() > 0;
										brut.PushBack(l);
									}
								}
							}
							sec("Reglages");
							{
								LigneMenu l;
								l.quoi = QFenetre;
								l.arg = 1;
								l.texte = NkString("Utilisation");
								l.detail = NkString("modele charge, memoire, jetons/s");
								brut.PushBack(l);
								l.arg = 2;
								l.texte = NkString("Carte des agents");
								l.detail = NkString("les etapes du tour en cours");
								brut.PushBack(l);
							}
							{
								bool aide = false;
								for (usize i = 0; i < commandes.Size(); ++i)
									aide = aide || commandes[i].section == NkString("Aide");
								if (aide) {
									sec("Aide");
									hote("Aide");
								}
							}
							// LE FILTRE : les sections vides tombent avec leurs lignes.
							for (usize i = 0; i < brut.Size(); ++i) {
								if (brut[i].genre == 1) {
									bool garde = false;
									for (usize j = i + 1; j < brut.Size() && brut[j].genre != 1; ++j)
										garde = garde || Contient(brut[j].texte, flt) || Contient(brut[j].detail, flt);
									if (garde)
										mLignes.PushBack(brut[i]);
								} else if (Contient(brut[i].texte, flt) || Contient(brut[i].detail, flt))
									mLignes.PushBack(brut[i]);
							}
							if (mLignes.Size() == 0)
								Note("Aucune action ne correspond au filtre.");
							break;
						}
						case NkAiMenu::Plus:
							for (usize i = 0; i < entreesPlus.Size(); ++i) {
								LigneMenu l;
								l.quoi = QPlus;
								l.arg = (int32)i;
								l.texte = entreesPlus[i].nom;
								l.eteint = entreesPlus[i].motif.Length() > 0;
								l.detail = l.eteint ? entreesPlus[i].motif : entreesPlus[i].detail;
								l.deuxLignes = l.detail.Length() > 0;
								mLignes.PushBack(l);
							}
							break;
						case NkAiMenu::Historique: {
							const NkAiConversation &c = mConv[(usize)mActif];
							for (usize i = c.archives.Size(); i > 0; --i) {
								LigneMenu l;
								l.quoi = QArchive;
								l.arg = (int32)(i - 1);
								l.texte = c.archives[i - 1].sujet;
								char d[32];
								snprintf(d, sizeof(d), "%u bloc(s)", (unsigned)c.archives[i - 1].fil.Taille());
								l.detail = NkString(d);
								mLignes.PushBack(l);
							}
							if (c.archives.Size() == 0)
								Note("Aucune conversation archivee");
							else {
								LigneMenu l;
								l.quoi = QViderHist;
								l.texte = NkString("Vider l'historique");
								mLignes.PushBack(l);
							}
							Note("Les conversations ne survivent pas a la fermeture.");
							break;
						}
						case NkAiMenu::AjouterIa:
							Note("Ajouter une IA = la DECLARER.");
							Note("Aucune cle n'est saisie ici : le panneau lit");
							Note("une variable d'environnement, ou le CLI");
							Note("s'authentifie lui-meme.");
							Note(declaration.Length() > 0 ? declaration.CStr() : "(ou declarer : non precise par l'hote)");
							break;
						default: break;
					}
				}

				float32 HauteurLigne(const LigneMenu &l, const NkAiMetriques &m) const {
					switch (l.genre) {
						case 1: return m.menuLigne * 0.9f;
						case 3: return l.deuxLignes ? m.menuLigne + m.ligne - 2.f : m.menuLigne + 2.f * echelle;
						default: return l.deuxLignes ? m.menuLigne + m.ligne - 2.f : m.menuLigne;
					}
				}

				void PublierMenu(const NkAiMetriques &m, const NkPaintRect &r, float32 yComposeur, float32 hEntete,
								 NkComponentPaint &p) {
					mMenuPtr.Clear();
					mMenuDetPtr.Clear();
					mMenuDroitePtr.Clear();
					if (mMenu == NkAiMenu::Aucun) {
						mLignes.Clear();
						return;
					}
					Remplir();
					for (usize i = 0; i < mLignes.Size(); ++i) {
						mMenuPtr.PushBack(mLignes[i].texte.CStr());
						mMenuDetPtr.PushBack(mLignes[i].detail.CStr());
						mMenuDroitePtr.PushBack(mLignes[i].droite.CStr());
					}
					const uint32 n = (uint32)mLignes.Size();
					if (n == 0)
						return;
					const bool filtre = mMenu == NkAiMenu::Commandes;
					// LA LARGEUR : ce qui s'y lit, bornee au panneau.
					const bool riche = mMenu == NkAiMenu::Modeles || mMenu == NkAiMenu::Modes ||
									   mMenu == NkAiMenu::Commandes || mMenu == NkAiMenu::Fournisseurs;
					float32 w = (riche ? 300.f : 220.f) * echelle;
					for (uint32 i = 0; i < n; ++i) {
						const LigneMenu &l = mLignes[i];
						const float32 wt = Mesure(&p, l.deuxLignes ? NkAiPolice::Grasse : NkAiPolice::Normale, mMenuPtr[i]);
						const float32 wd = Mesure(&p, NkAiPolice::Normale, mMenuDetPtr[i]);
						const float32 wr = Mesure(&p, NkAiPolice::Normale, mMenuDroitePtr[i]);
						const float32 v = l.deuxLignes ? (wt > wd ? wt : wd) + wr + 64.f * echelle
													   : wt + wd + wr + 70.f * echelle;
						if (v > w)
							w = v;
					}
					const float32 wMax = r.w - m.margeComposeur * 2.f;
					if (w > wMax)
						w = wMax;
					if (mMenu != NkAiMenu::Historique && (riche || mMenu == NkAiMenu::Plus))
						w = wMax;
					// UN MENU ETROIT (le panneau du modeleur, 276 px) : le detail passe
					// SOUS le nom. Cote a cote, a 45 % chacun, « Nouvelle conversation »
					// devenait « Nouvelle cc » -- ni le nom ni le detail ne se lisaient.
					if (w < 380.f * echelle)
						for (uint32 i = 0; i < n; ++i)
							if (mLignes[i].genre == 0 && mLignes[i].detail.Length() > 0)
								mLignes[i].deuxLignes = true;
					float32 hContenu = 0.f;
					for (uint32 i = 0; i < n; ++i)
						hContenu += HauteurLigne(mLignes[i], m);
					const float32 hTete = 8.f + (filtre ? m.menuLigne + 6.f : 0.f);
					float32 h = hTete + hContenu;
					// LE MENU DEFILE quand il ne tient pas entre l'en-tete et le composeur
					// (le « / » du modeleur : trente-neuf verbes). Il sortait du panneau.
					const float32 hMax = yComposeur - 4.f - (hEntete + 4.f);
					if (mMenu != mMenuPrecedent) {
						mMenuDefile = 0.f;
						mMenuPrecedent = mMenu;
					}
					const float32 visible = (h > hMax ? hMax : h) - hTete;
					const float32 maxDefile = hContenu > visible ? hContenu - visible : 0.f;
					if (mMenuDefile > maxDefile)
						mMenuDefile = maxDefile;
					if (mMenuDefile < 0.f)
						mMenuDefile = 0.f;
					if (h > hMax)
						h = hMax;
					float32 x = m.margeComposeur, y = yComposeur - 4.f - h;
					if (mMenu == NkAiMenu::Historique) {
						x = r.w - w - 12.f;
						y = hEntete + 4.f;
					} else if (riche || mMenu == NkAiMenu::Plus) {
						// LA LARGEUR DU COMPOSEUR (065041, 065128) : le menu se lit comme
						// une extension de la zone de saisie, pas comme une bulle.
					} else {
						// sous la pastille qui l'a ouvert, sans sortir du panneau
						NkAiRectPublie a;
						const NkAiPiece ancre = (mMenu == NkAiMenu::Modes)		   ? NkAiPiece::PastilleMode
												: (mMenu == NkAiMenu::Commandes) ? NkAiPiece::BoutonCommandes
												: (mMenu == NkAiMenu::Plus)		 ? NkAiPiece::BoutonPlus
																				 : NkAiPiece::PastilleModele;
						if (planChrome.Trouver(0u, ancre, a))
							x = a.x;
						if (x + w > r.w - m.margeComposeur)
							x = r.w - m.margeComposeur - w;
						if (x < m.margeComposeur)
							x = m.margeComposeur;
					}
					if (y < 4.f)
						y = 4.f;
					NkAiRectPublie f;
					f.piece = NkAiPiece::MenuFond;
					f.role = NkRole::PanelHeader;
					f.x = x;
					f.y = y;
					f.w = w;
					f.h = h;
					planChrome.Ajouter(f);
					mMenuRect = {x, y, w, h};
					float32 ly = y + 4.f;
					if (filtre) {
						// LE FILTRE DE LA CAPTURE (« Filter actions… ») : ce qui se tape
						// quand le menu est ouvert va ICI, pas dans la demande.
						NkAiRectPublie c;
						c.piece = NkAiPiece::MenuFiltre;
						c.x = x + 6.f;
						c.y = ly + 2.f;
						c.w = w - 12.f;
						c.h = m.menuLigne;
						planChrome.Ajouter(c);
						NkAiRectPublie t;
						t.piece = NkAiPiece::ChromeTexte;
						t.source = NkAiSource::Filtre;
						t.role = mFiltre[0] ? NkRole::Text : NkRole::TextMuted;
						t.x = c.x + 10.f;
						t.y = c.y + (c.h - m.ligne) * 0.5f;
						t.w = c.w - 20.f;
						t.h = m.ligne;
						planChrome.Ajouter(t);
						ly += m.menuLigne + 6.f;
					}
					const float32 droiteX = x + w - 8.f - 22.f; // la coche vit a droite
					const float32 hautLignes = ly, basLignes = y + h - 4.f;
					ly -= mMenuDefile;
					for (uint32 i = 0; i < n; ++i) {
						const LigneMenu &lm = mLignes[i];
						const float32 hl = HauteurLigne(lm, m);
						// HORS DE LA FENETRE DU MENU : rien n'est publie -- ni peint, ni
						// cliquable (une ligne invisible qui prendrait le clic mentirait).
						if (ly < hautLignes - 0.5f || ly + hl > basLignes + 0.5f) {
							ly += hl;
							continue;
						}
						// le filet avant chaque section, et avant l'Effort EN PIED de liste
						if ((lm.genre == 1 || (lm.genre == 3 && mMenu != NkAiMenu::Commandes)) && i > 0u) {
							NkAiRectPublie sep;
							sep.piece = NkAiPiece::Separateur;
							sep.role = NkRole::Border;
							sep.x = x + 1.f;
							sep.y = ly;
							sep.w = w - 2.f;
							sep.h = 1.f;
							planChrome.Ajouter(sep);
						}
						NkAiRectPublie l;
						l.piece = NkAiPiece::MenuLigne;
						l.role = NkRole::Text;
						l.x = x + 4.f;
						l.y = ly;
						l.w = w - 8.f;
						l.h = hl;
						l.debut = i;
						l.drapeaux = (uint8)((lm.actif ? kAiActif : 0u) | ((lm.eteint || lm.genre == 4) ? kAiEteint : 0u) |
											 (lm.genre == 1 ? kAiSection : 0u));
						planChrome.Ajouter(l);
						// ce qui s'aligne a droite : la valeur (Effort : « Haut »), le modele
						// courant, l'interrupteur
						float32 finTexte = droiteX;
						if (lm.genre == 2) {
							NkAiRectPublie s;
							s.piece = NkAiPiece::Interrupteur;
							s.w = 30.f * echelle;
							s.h = 16.f * echelle;
							s.x = x + w - 12.f - s.w;
							s.y = ly + (m.menuLigne - s.h) * 0.5f;
							s.debut = i;
							s.drapeaux = (uint8)((lm.valeur ? kAiActif : 0u) | (lm.eteint ? kAiEteint : 0u));
							planChrome.Ajouter(s);
							finTexte = s.x - 8.f;
						} else if (mMenuDroitePtr[i][0]) {
							float32 wr = Mesure(&p, NkAiPolice::Normale, mMenuDroitePtr[i]);
							const float32 cap = (w - 8.f) * (lm.genre == 3 ? 0.62f : 0.45f);
							if (wr > cap)
								wr = cap;
							NkAiRectPublie d;
							d.piece = NkAiPiece::ChromeTexte;
							d.source = NkAiSource::MenuDroite;
							d.role = NkRole::TextMuted;
							d.x = x + w - 12.f - wr;
							d.y = ly + (m.menuLigne - m.ligne) * 0.5f;
							d.w = wr;
							d.h = m.ligne;
							d.debut = i;
							if (lm.genre == 3) {
								// « Effort (Haut) » : la valeur suit le libelle ; le curseur
								// compact tient la droite.
								d.x = x + 14.f + Mesure(&p, NkAiPolice::Normale, mMenuPtr[i]) + 5.f;
								const float32 lim = x + w - 12.f - 80.f * echelle - 10.f;
								d.w = lim - d.x;
								if (d.w > 10.f)
									planChrome.Ajouter(d);
							} else {
								planChrome.Ajouter(d);
								finTexte = d.x - 10.f;
							}
						}
						if (lm.genre == 3) {
							// LE CURSEUR A CRANS (065128 : Effort en pied de liste)
							NkAiRectPublie c;
							c.piece = NkAiPiece::Curseur;
							c.w = 76.f * echelle;
							c.h = 16.f * echelle;
							c.x = x + w - 12.f - c.w;
							c.y = ly + ((lm.deuxLignes ? m.menuLigne : hl) - c.h) * 0.5f;
							c.debut = (uint32)(effort < 0 ? 0 : effort);
							c.longueur = (uint32)effortCrans.Size();
							c.drapeaux = (uint8)(lm.eteint ? kAiEteint : 0u);
							planChrome.Ajouter(c);
						}
						const float32 tx = x + 14.f;
						NkAiRectPublie t;
						t.piece = NkAiPiece::ChromeTexte;
						t.source = NkAiSource::MenuTexte;
						t.police = (lm.deuxLignes && !lm.eteint) ? NkAiPolice::Grasse : NkAiPolice::Normale;
						t.role = (lm.eteint || lm.genre == 1 || lm.genre == 4) ? NkRole::TextMuted : NkRole::Text;
						t.x = tx;
						t.y = lm.deuxLignes ? ly + 4.f : ly + (m.menuLigne - m.ligne) * 0.5f;
						t.h = m.ligne;
						t.debut = i;
						if (lm.deuxLignes) {
							// NOM EN GRAS, UNE LIGNE DE DESCRIPTION GRISE DESSOUS (065128)
							if (lm.genre == 2 || lm.genre == 3)
								t.y = ly + (m.menuLigne - m.ligne) * 0.5f;
							t.w = (lm.actif ? droiteX : finTexte) - tx;
							if (t.w < 10.f)
								t.w = 10.f;
							planChrome.Ajouter(t);
							if (mMenuDetPtr[i][0]) {
								NkAiRectPublie dd = t;
								dd.source = NkAiSource::MenuDetail;
								dd.police = NkAiPolice::Normale;
								dd.role = NkRole::TextMuted;
								dd.y = (lm.genre == 2 || lm.genre == 3) ? t.y + m.ligne - 1.f : ly + 4.f + m.ligne - 2.f;
								dd.w = x + w - 12.f - tx;
								planChrome.Ajouter(dd);
							}
						} else {
							// LE DETAIL CEDE : au plus 45 % de la ligne, rogne au-dela.
							float32 wd = (lm.genre != 2 && lm.genre != 3 && mMenuDetPtr[i][0])
											 ? Mesure(&p, NkAiPolice::Normale, mMenuDetPtr[i])
											 : 0.f;
							if (wd > (w - 8.f) * 0.45f)
								wd = (w - 8.f) * 0.45f;
							t.w = finTexte - tx - (wd > 0.f ? wd + 12.f : 0.f);
							if (t.w < 10.f)
								t.w = 10.f;
							planChrome.Ajouter(t);
							if (wd > 0.f) {
								NkAiRectPublie dd = t;
								dd.source = NkAiSource::MenuDetail;
								dd.role = NkRole::TextMuted;
								dd.x = finTexte - wd;
								dd.w = wd;
								planChrome.Ajouter(dd);
							}
							// l'interrupteur grise dit son motif SOUS son nom : jamais cache
							if (lm.genre == 2 && mMenuDetPtr[i][0] && lm.eteint) {
								NkAiRectPublie dd = t;
								dd.source = NkAiSource::MenuDetail;
								dd.role = NkRole::TextMuted;
								dd.x = tx + Mesure(&p, NkAiPolice::Normale, mMenuPtr[i]) + 12.f;
								dd.w = finTexte - dd.x;
								if (dd.w > 10.f)
									planChrome.Ajouter(dd);
							}
						}
						ly += hl;
					}
				}

				// ── LES FENETRES (Utilisation, Carte des agents) ────────────────
				void PublierFenetre(const NkAiMetriques &m, const NkPaintRect &r, float32 hEntete, float32 yComposeur,
									NkComponentPaint &p) {
					mFenTextes.Clear();
					mFenPtr.Clear();
					if (mFenetre == 0u)
						return;
					const NkAiFenetre &fe = (mFenetre == 1u) ? utilisation : carte;
					mFenTextes.PushBack(fe.titre.Length() ? fe.titre
														  : NkString(mFenetre == 1u ? "Utilisation" : "Carte des agents"));
					mFenTextes.PushBack(NkString());
					for (usize i = 0; i < fe.lignes.Size(); ++i) {
						mFenTextes.PushBack(fe.lignes[i].libelle);
						mFenTextes.PushBack(fe.lignes[i].valeur);
					}
					if (fe.lignes.Size() == 0) {
						mFenTextes.PushBack(NkString("L'hote n'a rien declare pour cette fenetre."));
						mFenTextes.PushBack(NkString());
					}
					for (usize i = 0; i < mFenTextes.Size(); ++i)
						mFenPtr.PushBack(mFenTextes[i].CStr());
					const uint32 nl = (uint32)(mFenTextes.Size() / 2u);
					const float32 x = 16.f, w = r.w - 32.f;
					float32 h = 12.f + m.menuLigne + 6.f;
					for (uint32 i = 1; i < nl; ++i) {
						const uint8 g = (i - 1 < fe.lignes.Size()) ? fe.lignes[i - 1].genre : 3u;
						h += (g == 2u) ? m.ligne + 14.f : (g == 0u ? m.ligne + 8.f : m.ligne + 4.f);
						// la meme regle que la mise en page : une valeur qui passe dessous
						const char *val = mFenPtr[i * 2u + 1u];
						if (val[0] && Mesure(&p, NkAiPolice::Normale, mFenPtr[i * 2u]) +
											  Mesure(&p, NkAiPolice::Normale, val) + 12.f >
										  (r.w - 32.f) - 28.f)
							h += m.ligne;
					}
					h += 10.f;
					const float32 y = hEntete + 8.f;
					if (h > yComposeur - y - 8.f)
						h = yComposeur - y - 8.f;
					NkAiRectPublie fo;
					fo.piece = NkAiPiece::Fenetre;
					fo.x = x;
					fo.y = y;
					fo.w = w;
					fo.h = h;
					planChrome.Ajouter(fo);
					NkAiRectPublie t;
					t.piece = NkAiPiece::ChromeTexte;
					t.source = NkAiSource::FenetreTexte;
					t.police = NkAiPolice::Grasse;
					t.role = NkRole::Text;
					t.x = x + 14.f;
					t.y = y + 10.f;
					t.w = w - 60.f;
					t.h = m.ligne;
					t.debut = 0u;
					planChrome.Ajouter(t);
					NkAiRectPublie cr;
					cr.piece = NkAiPiece::FenetreFermer;
					cr.w = cr.h = 14.f * echelle;
					cr.x = x + w - 14.f - cr.w;
					cr.y = y + 12.f;
					planChrome.Ajouter(cr);
					float32 ly = y + 12.f + m.menuLigne;
					for (uint32 i = 1; i < nl; ++i) {
						const NkAiFenetreLigne *fl = (i - 1 < fe.lignes.Size()) ? &fe.lignes[i - 1] : nullptr;
						const uint8 g = fl ? fl->genre : 3u;
						const float32 hl = (g == 2u) ? m.ligne + 14.f : (g == 0u ? m.ligne + 8.f : m.ligne + 4.f);
						if (ly + hl > y + h)
							break; // rogne plutot que deborder sur le composeur
						NkAiRectPublie a;
						a.piece = NkAiPiece::ChromeTexte;
						a.source = NkAiSource::FenetreTexte;
						a.debut = i;
						a.longueur = 0u;
						a.role = (g == 0u || g == 3u) ? NkRole::TextMuted : NkRole::Text;
						a.x = x + 14.f;
						a.y = ly + (g == 0u ? 6.f : 0.f);
						a.h = m.ligne;
						const char *val = mFenPtr[i * 2u + 1u];
						float32 wv = val[0] ? Mesure(&p, NkAiPolice::Normale, val) : 0.f;
						const float32 wl = Mesure(&p, NkAiPolice::Normale, mFenPtr[i * 2u]);
						// UNE VALEUR QUI NE TIENT PAS A COTE DE SON LIBELLE PASSE DESSOUS
						// (le modeleur, 276 px : « En memoir | non : charge a la pro »).
						const bool dessous = wv > 0.f && wl + wv + 12.f > w - 28.f;
						if (wv > w - 28.f)
							wv = w - 28.f;
						a.w = dessous ? w - 28.f : w - 28.f - (wv > 0.f ? wv + 12.f : 0.f);
						planChrome.Ajouter(a);
						if (wv > 0.f) {
							NkAiRectPublie v = a;
							v.longueur = 1u;
							v.role = NkRole::TextMuted;
							v.x = dessous ? x + 28.f : x + w - 14.f - wv;
							v.w = wv;
							if (dessous) {
								v.y = a.y + m.ligne;
								ly += m.ligne;
							}
							planChrome.Ajouter(v);
						}
						if (g == 2u && fl) {
							NkAiRectPublie b;
							b.piece = NkAiPiece::Barre;
							b.role = fl->role;
							b.x = x + 14.f;
							b.y = ly + m.ligne + 3.f;
							b.w = w - 28.f;
							b.h = 5.f * echelle;
							const float32 part = fl->part < 0.f ? 0.f : (fl->part > 1.f ? 1.f : fl->part);
							b.longueur = (uint32)(part * 1000.f + 0.5f);
							planChrome.Ajouter(b);
						}
						ly += hl;
					}
				}

				/// La piece interactive du chrome sous (px, py) -- coordonnees du
				/// panneau -- ou -1. Les menus d'abord : ils sont par-dessus.
				int32 PieceSous(float32 px, float32 py) const {
					for (uint32 i = planChrome.Pieces(); i > 0; --i) {
						const NkAiRectPublie &q = planChrome.Piece(i - 1);
						switch (q.piece) {
							case NkAiPiece::Interrupteur:
							case NkAiPiece::Curseur:
							case NkAiPiece::MenuFiltre:
							case NkAiPiece::MenuLigne:
							case NkAiPiece::MenuFond:
							case NkAiPiece::FenetreFermer:
							case NkAiPiece::Fenetre:
							case NkAiPiece::IconeHistorique:
							case NkAiPiece::IconeNouvelle:
							case NkAiPiece::BoutonPlus:
							case NkAiPiece::BoutonCommandes:
							case NkAiPiece::PastilleModele:
							case NkAiPiece::PastilleMode:
							case NkAiPiece::PastilleLieu:
							case NkAiPiece::Envoi:
							case NkAiPiece::ComposeurCadre: {
								// les icones et le curseur se cliquent un peu au-dela de leur trait
								const float32 marge = (q.w < 30.f || q.h < 10.f) ? 5.f : 0.f;
								if (px >= q.x - marge && px < q.x + q.w + marge && py >= q.y - marge &&
									py < q.y + q.h + marge)
									return (int32)(i - 1);
								break;
							}
							default: break;
						}
					}
					return -1;
				}

				static bool DansMenu(NkAiPiece p) {
					return p == NkAiPiece::MenuLigne || p == NkAiPiece::MenuFond || p == NkAiPiece::Interrupteur ||
						   p == NkAiPiece::Curseur || p == NkAiPiece::MenuFiltre;
				}

				void Souris(nkgui::NkGuiContext &ctx, const NkPaintRect &r, bool libre, const NkAiMetriques &m,
							NkAiSorties &out) {
					if (!libre)
						return;
					const float32 px = ctx.input.mousePos.x - r.x, py = ctx.input.mousePos.y - r.y;
					const bool dedans = px >= 0.f && py >= 0.f && px < r.w && py < r.h;
					const bool clic = ctx.input.mouseClicked[0];
					const int32 k = dedans ? PieceSous(px, py) : -1;
					if (k >= 0)
						planChrome.PieceModifiable((uint32)k).drapeaux |= kAiSurvol;
					// le survol d'une ligne se voit aussi quand on est sur son interrupteur
					if (k >= 0 && planChrome.Piece((uint32)k).piece == NkAiPiece::Interrupteur) {
						const uint32 li = planChrome.Piece((uint32)k).debut;
						for (uint32 i = 0; i < planChrome.Pieces(); ++i)
							if (planChrome.Piece(i).piece == NkAiPiece::MenuLigne && planChrome.Piece(i).debut == li)
								planChrome.PieceModifiable(i).drapeaux |= kAiSurvol;
					}
					const bool surFenetre = k >= 0 && (planChrome.Piece((uint32)k).piece == NkAiPiece::Fenetre ||
													   planChrome.Piece((uint32)k).piece == NkAiPiece::FenetreFermer);
					// le survol d'un bouton de bloc
					if (dedans && !(k >= 0) && vueFil.Contains(ctx.input.mousePos.x, ctx.input.mousePos.y)) {
						const float32 fx = ctx.input.mousePos.x - filOx, fy = ctx.input.mousePos.y - filOy;
						for (uint32 i = 0; i < planFil.Pieces(); ++i) {
							NkAiRectPublie &q = planFil.PieceModifiable(i);
							if (q.piece == NkAiPiece::Action && fx >= q.x && fx < q.x + q.w && fy >= q.y &&
								fy < q.y + q.h)
								q.drapeaux |= kAiSurvol;
						}
					}
					if (!clic)
						return;
					// UN CLIC HORS DU MENU LE FERME, et ne fait rien d'autre : sinon le
					// clic qui ferme un menu declencherait aussi ce qui est dessous.
					if (mMenu != NkAiMenu::Aucun) {
						const bool surMenu = k >= 0 && DansMenu(planChrome.Piece((uint32)k).piece);
						if (!surMenu) {
							FermerMenus();
							mRefus = NkString();
							if (dedans)
								out.clicPris = true;
							return;
						}
					}
					if (!dedans)
						return;
					out.clicPris = true;
					if (k >= 0) {
						const NkAiRectPublie &q = planChrome.Piece((uint32)k);
						switch (q.piece) {
							case NkAiPiece::MenuLigne:
								ChoisirLigne((int32)q.debut, px, out);
								return;
							case NkAiPiece::Interrupteur:
								ChoisirLigne((int32)q.debut, px, out);
								return;
							case NkAiPiece::Curseur:
								for (uint32 i = 0; i < (uint32)mLignes.Size(); ++i)
									if (mLignes[i].quoi == QEffort) {
										ChoisirLigne((int32)i, px, out);
										break;
									}
								return;
							case NkAiPiece::MenuFiltre:
								ctx.inputId = IdComposeur();
								ctx.inputClickConsumed = true;
								return;
							case NkAiPiece::MenuFond:
							case NkAiPiece::Fenetre:
								return;
							case NkAiPiece::FenetreFermer:
								mFenetre = 0u;
								out.fenetreFermee = true;
								return;
							case NkAiPiece::IconeNouvelle:
								if (occupe)
									return; // la reponse attendue irait dans la conversation neuve
								NouvelleConversation();
								out.nouvelle = true;
								return;
							case NkAiPiece::IconeHistorique:
								if (historiqueParHote)
									out.historique = true;
								else
									mMenu = NkAiMenu::Historique;
								return;
							case NkAiPiece::BoutonPlus:
								if (entreesPlus.Size() > 0)
									mMenu = NkAiMenu::Plus;
								else
									out.plus = true;
								return;
							case NkAiPiece::BoutonCommandes:
								if (commandesParHote)
									out.commandes = true;
								else {
									mMenu = NkAiMenu::Commandes;
									mFiltre[0] = 0;
									// le filtre recoit la frappe : le composeur prend le focus
									ctx.inputId = IdComposeur();
									ctx.inputClickConsumed = true;
								}
								return;
							case NkAiPiece::PastilleModele:
							case NkAiPiece::PastilleLieu:
								mMenu = NkAiMenu::Fournisseurs;
								mRefus = NkString();
								return;
							case NkAiPiece::PastilleMode:
								if (modes.Size() > 0)
									mMenu = NkAiMenu::Modes;
								return;
							case NkAiPiece::Envoi:
								if (occupe && (!fileAttente || Blanc(mSaisie)))
									out.arreter = true;
								else
									Envoyer(out);
								return;
							case NkAiPiece::ComposeurCadre:
								// LE FOCUS CLAVIER, et le clic est PRIS : sinon NKGui
								// defocaliserait a la fin de cette meme image.
								ctx.inputId = IdComposeur();
								ctx.inputClickConsumed = true;
								{
									int32 n = 0;
									while (mSaisie[n])
										++n;
									mCaret = n;
								}
								return;
							default: return;
						}
					}
					if (surFenetre)
						return;
					// ── LE FIL : plier / deplier, et les boutons d'un bloc ──
					if (vueFil.Contains(ctx.input.mousePos.x, ctx.input.mousePos.y)) {
						const float32 fx = ctx.input.mousePos.x - filOx, fy = ctx.input.mousePos.y - filOy;
						for (uint32 i = planFil.Pieces(); i > 0; --i) {
							const NkAiRectPublie &q = planFil.Piece(i - 1);
							if (q.blocId == 0u || !(fx >= q.x && fx < q.x + q.w && fy >= q.y && fy < q.y + q.h))
								continue;
							if (q.piece == NkAiPiece::Action) {
								if ((q.drapeaux & kAiEteint) == 0u) {
									out.actionBloc = q.blocId;
									out.actionIndice = (int32)q.debut;
								}
								return;
							}
							// une LIGNE de bloc bascule ; l'interieur d'une boite non
							if (q.piece == NkAiPiece::Titre || q.piece == NkAiPiece::Texte ||
								q.piece == NkAiPiece::Puce || (q.piece == NkAiPiece::Fragment && fy < q.y + m.ligne)) {
								uint32 idx = 0;
								if (Fil().TrouverParId(q.blocId, idx)) {
									const NkAiBloc t = Fil().At(idx).type;
									if (t == NkAiBloc::Outil || t == NkAiBloc::Reflexion) {
										Fil().BasculerParId(q.blocId);
										out.bascule = q.blocId;
									}
								}
								return;
							}
						}
					}
				}

				void ChoisirLigne(int32 i, float32 px, NkAiSorties &out) {
					if (i < 0 || i >= (int32)mLignes.Size())
						return;
					const LigneMenu l = mLignes[(usize)i];
					if (l.genre == 1 || l.genre == 4)
						return; // un titre, une note : rien a faire
					if (l.eteint)
						return; // GRISE : le motif est deja ecrit sur la ligne
					switch (l.quoi) {
						case QFournisseur: {
							// LE FOURNISSEUR OUVRE TOUJOURS SES MODELES (065128) : on y lit
							// ce qu'on va choisir, meme quand il n'y en a qu'un.
							if (l.arg < 0 || l.arg >= (int32)fournisseurs.Size())
								return;
							if (fournisseurs[(usize)l.arg].modeles.Size() > 0u) {
								mMenuFournisseur = l.arg;
								mMenu = NkAiMenu::Modeles;
								return;
							}
							Basculer(l.arg, out);
							return;
						}
						case QRetour:
							mMenu = NkAiMenu::Fournisseurs;
							return;
						case QAjouterIa:
							mMenu = NkAiMenu::AjouterIa;
							out.ajouterIa = true;
							return;
						case QModele: {
							const int32 fi = mMenuFournisseur;
							if (fi < 0 || fi >= (int32)fournisseurs.Size())
								return;
							if (!Basculer(fi, out))
								return;
							NkAiFournisseurDesc &f = fournisseurs[(usize)fi];
							if (l.arg >= 0 && l.arg < (int32)f.modeles.Size() && f.modele != l.arg) {
								f.modele = l.arg;
								out.modeleChange = true;
							}
							FermerMenus();
							return;
						}
						case QMode:
							if (l.arg >= 0 && l.arg < (int32)modes.Size() && l.arg != mode) {
								mode = l.arg;
								out.modeChange = true;
							}
							FermerMenus();
							return;
						case QCommande:
							if (l.arg >= 0 && l.arg < (int32)commandes.Size()) {
								const NkAiCommandeDesc &c = commandes[(usize)l.arg];
								if (c.id >= 0)
									out.commande = c.id; // l'HOTE execute : le panneau ne sait pas joindre
								else {
									const NkString &s = c.insertion;
									int32 len = 0;
									while (mSaisie[len])
										++len;
									for (usize k = 0; k < s.Length() && len + 2 < (int32)sizeof(mSaisie); ++k)
										mSaisie[len++] = s.CStr()[k];
									mSaisie[len] = 0;
									mCaret = len;
								}
							}
							FermerMenus();
							return;
						case QNouvelle:
							if (!occupe) {
								NouvelleConversation();
								out.nouvelle = true;
							}
							FermerMenus();
							return;
						case QChangerModele:
							mMenuFournisseur = mActif;
							mMenu = (mActif >= 0 && mActif < (int32)fournisseurs.Size() &&
									 fournisseurs[(usize)mActif].modeles.Size() > 0u)
										? NkAiMenu::Modeles
										: NkAiMenu::Fournisseurs;
							mFiltre[0] = 0;
							return;
						case QEffort: {
							// LE CRAN SOUS LA SOURIS, lu sur la piste PUBLIEE (pas recalculee)
							const uint32 nc = (uint32)effortCrans.Size();
							if (nc < 2u)
								return;
							NkAiRectPublie c;
							bool trouve = false;
							for (uint32 k = 0; k < planChrome.Pieces() && !trouve; ++k)
								if (planChrome.Piece(k).piece == NkAiPiece::Curseur) {
									c = planChrome.Piece(k);
									trouve = true;
								}
							if (!trouve)
								return;
							float32 t = (px - c.x) / (c.w > 1.f ? c.w : 1.f);
							t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
							const int32 cran = (int32)(t * (float32)(nc - 1u) + 0.5f);
							if (cran != effort) {
								effort = cran;
								out.effortChange = true;
							}
							return; // le menu reste ouvert : on regle, on voit, on ferme
						}
						case QPenser:
							penser = !penser;
							out.penserChange = true;
							return;
						case QFenetre:
							mFenetre = (uint8)l.arg;
							out.fenetre = (uint8)l.arg;
							FermerMenus();
							return;
						case QPlus:
							if (l.arg >= 0 && l.arg < (int32)entreesPlus.Size())
								out.entreePlus = entreesPlus[(usize)l.arg].id;
							FermerMenus();
							return;
						case QArchive:
							if (!occupe)
								Rouvrir(l.arg);
							FermerMenus();
							return;
						case QViderHist:
							mConv[(usize)mActif].archives.Clear(); // ATTEIGNABLE
							FermerMenus();
							return;
						default: FermerMenus(); return;
					}
				}

				bool Basculer(int32 i, NkAiSorties &out) {
					const int32 avant = mActif;
					NkString pourquoi;
					if (!Choisir(i, pourquoi)) {
						// LE REFUS SE LIT SUR LA LIGNE, au lieu d'un menu qui se ferme
						// en silence et d'un assistant qui n'a pas change.
						mRefus = pourquoi;
						mRefusFournisseur = i;
						mMenu = NkAiMenu::Fournisseurs;
						return false;
					}
					mRefus = NkString();
					mMenu = NkAiMenu::Aucun;
					if (avant != mActif) {
						out.fournisseurChange = true;
						out.ancien = avant;
						out.nouveau = mActif;
					}
					return true;
				}

				NkVector<NkAiConversation> mConv;
				int32 mActif = 0;
				NkAiFil *mVivant = nullptr;
				char mSaisie[8192] = {0};
				int32 mCaret = 0;
				NkAiMenu mMenu = NkAiMenu::Aucun;
				int32 mMenuFournisseur = -1;
				NkString mRefus;
				int32 mRefusFournisseur = -1;
				NkVector<LigneMenu> mLignes;
				float32 mMenuDefile = 0.f;
				NkAiMenu mMenuPrecedent = NkAiMenu::Aucun;
				NkPaintRect mMenuRect;
				NkVector<const char *> mMenuPtr, mMenuDetPtr, mMenuDroitePtr;
				char mFiltre[128] = {0};
				bool mFiltreValider = false;
				uint8 mFenetre = 0;
				NkVector<NkString> mFenTextes;
				NkVector<const char *> mFenPtr;
				uint32 mEpingle = 0u;

			public:
				/// La demande EPINGLEE en tete du fil cette image (0 = aucune).
				uint32 Epingle() const {
					return mEpingle;
				}
		};

		/// LA CARTE DES AGENTS (065201), chez nous : les ETAPES DU TOUR EN COURS,
		/// lues dans le fil -- ce qui s'y lit s'est passe, rien n'est estime. Une
		/// seule lecture pour toutes les applications : la carte d'une application
		/// ne peut pas dire autre chose que son fil.
		inline void NkAiCarteDepuisFil(const NkAiFil &fil, bool occupe, NkAiFenetre &fe) {
			fe.Vider();
			fe.titre = NkString("Carte des agents");
			int32 debut = -1;
			for (uint32 i = fil.Taille(); i > 0; --i)
				if (fil.At(i - 1).type == NkAiBloc::Demande) {
					debut = (int32)(i - 1);
					break;
				}
			if (debut < 0) {
				fe.Note("Aucun tour : la carte montre les etapes de la demande en cours.");
				return;
			}
			{
				NkString t("Tour : « ");
				const NkString &d = fil.At((uint32)debut).texte;
				t.Append(d.Length() > 60 ? NkString(d.CStr(), 60).CStr() : d.CStr());
				t.Append(d.Length() > 60 ? "… »" : " »");
				fe.Section(t.CStr());
			}
			uint32 n = 0, faites = 0;
			for (uint32 i = (uint32)debut + 1u; i < fil.Taille(); ++i) {
				const NkAiBlocDonnees &b = fil.At(i);
				const bool dernier = i + 1u == fil.Taille();
				char lib[96];
				const char *quoi = "etape";
				NkString etat("fait");
				switch (b.type) {
					case NkAiBloc::Outil:
						quoi = b.titre.Length() ? b.titre.CStr() : "outil";
						if (dernier && occupe && b.sortie.Length() == 0)
							etat = NkString("en cours");
						break;
					case NkAiBloc::Reflexion: quoi = "raisonnement"; break;
					case NkAiBloc::Prose: quoi = "reponse"; break;
					case NkAiBloc::Effet:
						quoi = "effet";
						etat = b.effet.Length() ? b.effet : etat;
						break;
					case NkAiBloc::Refus:
						quoi = "refus";
						etat = b.motif;
						break;
					case NkAiBloc::Echec:
						quoi = "echec";
						etat = b.motif;
						break;
					default: break;
				}
				++n;
				if (etat == NkString("fait"))
					++faites;
				snprintf(lib, sizeof(lib), "%u. %s", (unsigned)n, quoi);
				fe.Valeur(lib, etat.CStr());
			}
			if (n == 0)
				fe.Note(occupe ? "La demande est partie ; aucune etape n'est encore revenue." : "Aucune etape.");
			else {
				char v[48];
				snprintf(v, sizeof(v), "%u / %u", (unsigned)faites, (unsigned)n);
				fe.Barre("Etapes faites", v, (float32)faites / (float32)n, NkRole::StatusOk);
			}
			if (occupe)
				fe.Note("Un tour est en vol.");
		}

	} // namespace editorkit
} // namespace nkentseu
