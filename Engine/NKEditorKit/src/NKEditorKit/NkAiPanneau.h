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
#include "NKFileSystem/NkFile.h" // (Q8) la persistance des chats
#include <cstdio>
#include <cstdlib>

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

		/// UN CHAT (21/09, Q8 -- Rodolf : « c'est la creation d'un nouveau chat qui
		/// met une nouvelle page, et on doit pouvoir basculer entre plusieurs chats
		/// sans rien perdre de chacun »).
		/// ⚠️ UN CHAT N'APPARTIENT NI A UN MODELE NI A UN FOURNISSEUR : ce sont SES
		///    REGLAGES, qu'on change en cours de route sans rien perdre. La regle
		///    de Q2.3 (« changer de fournisseur ramene SA conversation ») rangeait
		///    le fil dans la case de l'ancien fournisseur -- choisir un modele
		///    d'un autre fournisseur montrait une page vide : le defaut vu par
		///    Rodolf le 21/09 a 13h42. Elle est RETIREE.
		struct NkAiConversation {
				NkAiFil fil;
				float32 defile = 0.f;
				bool colle = true; ///< suit le bas tant qu'on n'a pas remonte
				float64 debut = -1.0;
				NkString brouillon;		///< le composeur non envoye de CE chat
				NkString fournisseur;	///< la cle du fournisseur choisi (stable)
				NkString modele;		///< le nom du modele choisi
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
				/// (Q8) LES IMAGES qui partent avec `texte` (chemins). L'hote les lit.
				NkVector<NkString> images;
				/// « Joindre une image… » : l'hote ouvre SON selecteur de fichier et
				/// rend le chemin par `JoindreImage`.
				bool joindreImage = false;
				int32 entreePlus = -1;	   ///< l'`id` d'une entree « + »
				uint8 fenetre = 0;		   ///< 1 = Utilisation, 2 = Carte des agents : vient d'ouvrir
				bool fenetreFermee = false;
				// ── Q8 ──
				bool copie = false; ///< quelque chose vient d'etre copie dans le presse-papiers
				NkString copieTexte; ///< ce qui a ete copie (la sonde le compare au presse-papiers)
				/// Le panneau a PRIS un clic de la souris cette image : l'hote ne doit
				/// pas le relayer a ce qui est dessous.
				bool clicPris = false;
		};

		enum class NkAiMenu : uint8 { Aucun = 0, Fournisseurs, Modeles, Modes, Commandes, Historique, AjouterIa, Plus, Contexte };

		class NkAiPanneau {
			private:
				/// (Q8) Un point de selection : (bloc, rang, octet), jamais un indice de plan.
				struct PointSel {
						uint32 bloc = 0u;	 ///< le bloc (stable d'une image a l'autre)
						uint32 ordinal = 0u; ///< le rang du morceau de texte DANS ce bloc
						uint32 off = 0u;	 ///< l'octet dans la tranche du morceau
						bool valide = false;
				};
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
				/// (Q8) L'hote accepte des images jointes : le « + » porte « Joindre une
				/// image… », Ctrl+V d'un chemin d'image et le depot d'un fichier joignent.
				bool accepteImages = false;
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

				/// (Q8) LA PERSISTANCE DES CHATS : non vide = les chats sont relus au
				/// premier affichage et reecrits a chaque changement (contenu, bascule,
				/// reglage). Un fichier texte a lignes, dans le dossier de l'hote.
				NkString cheminChats;

				NkAiPanneau() {
					mConv.Resize(1);
					effortCrans.PushBack(NkString("Bas"));
					effortCrans.PushBack(NkString("Moyen"));
					effortCrans.PushBack(NkString("Haut"));
					effortCrans.PushBack(NkString("Max"));
				}
				// ── LES IMAGES JOINTES (Q8) ─────────────────────────────────────
				/// Joint une image au composeur. Rend faux ET NOMME la raison (fichier
				/// illisible, pas une image) -- rien n'est joint en silence.
				bool JoindreImage(const char *chemin, NkString &pourquoi) {
					if (!accepteImages) {
						pourquoi = NkString("cet assistant ne recoit pas d'image");
						return false;
					}
					const NkAiVignetteImage &v = NkAiVignetteDe(chemin);
					if (!v.ok) {
						pourquoi = NkString("ce fichier ne se lit pas comme une image : ");
						pourquoi.Append(chemin ? chemin : "");
						return false;
					}
					for (usize i = 0; i < mJointes.Size(); ++i)
						if (mJointes[i] == NkString(chemin))
							return true; // deja jointe
					if (mJointes.Size() >= 6u) {
						pourquoi = NkString("six images au plus par demande");
						return false;
					}
					mJointes.PushBack(NkString(chemin));
					return true;
				}
				void RetirerImage(uint32 i) {
					if (i < mJointes.Size())
						mJointes.Erase(mJointes.Begin() + i);
				}
				const NkVector<NkString> &ImagesJointes() const {
					return mJointes;
				}
				/// (Q9) La selection du composeur [a, b), ou faux s'il n'y en a pas.
				bool SelectionComposeur(int32 &a, int32 &b) const {
					if (mSelComp < 0 || mSelComp == mCaret)
						return false;
					a = mSelComp < mCaret ? mSelComp : mCaret;
					b = mSelComp < mCaret ? mCaret : mSelComp;
					return true;
				}
				/// (Q9) UNE LIGNE D'ETAT pour les sondes : ce que le composeur contient,
				/// ce qui y est selectionne, ce que le fil a de selectionne, le menu
				/// ouvert, les images jointes -- et le presse-papiers RELU.
				void TracerEtat(nkgui::NkGuiContext &ctx, const char *qui, int32 image) const {
					int32 a = 0, b = 0;
					const bool sel = SelectionComposeur(a, b);
					const NkString cb = ctx.GetClipboard();
					NkString selTxt;
					if (sel)
						selTxt = NkString(mSaisie + a, (NkString::SizeType)(b - a));
					std::printf("[%s] AI ETAT image=%d saisie=\"%.60s\" selection_composeur=%s\"%.40s\" "
								"selection_fil=%u octets menu=%d images=%u presse_papiers=\"%.60s\"\n",
								qui, (int)image, mSaisie, sel ? "" : "(aucune) ", selTxt.CStr(),
								(unsigned)mTexteSel.Length(), (int)mMenu, (unsigned)mJointes.Size(), cb.CStr());
					std::fflush(stdout);
				}
				/// Le DEPOT de fichiers sur le panneau (glisser-deposer) : l'hote relaie
				/// ce que la fenetre lui donne ; ce qui n'est pas une image est refuse
				/// et le motif est rendu.
				uint32 DeposerFichiers(const char *const *chemins, uint32 n, NkString &pourquoi) {
					uint32 ok = 0;
					for (uint32 i = 0; i < n; ++i)
						if (JoindreImage(chemins[i], pourquoi))
							++ok;
					return ok;
				}

				// ── LA SELECTION DU FIL (Q8) ────────────────────────────────────
				/// Tout le fil, comme Ctrl+A quand le fil a la main. Porte des sondes.
				void ToutSelectionnerFil() {
					mSelTout = true;
					mFilFocus = true;
				}
				bool SelectionFil() const {
					return mSelA.valide && mSelB.valide;
				}
				/// Le texte selectionne dans le fil, tel que Ctrl+C le copierait.
				NkString TexteSelectionne() const {
					return mTexteSel;
				}
				void EffacerSelection() {
					mSelA = mSelB = PointSel{};
					mSelTout = false;
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
					NkAiFil &f = mVivant ? *mVivant : mConv[(usize)mChat].fil;
					f.Declarer(capacites);
					return f;
				}
				const NkAiFil &Fil() const {
					return mVivant ? *mVivant : mConv[(usize)mChat].fil;
				}
				/// Le fil du chat `i` (le vivant si c'est le chat ouvert).
				NkAiFil &FilDe(int32 i) {
					Assurer();
					if (i == mChat || i < 0 || i >= (int32)mConv.Size())
						return Fil();
					mConv[(usize)i].fil.Declarer(capacites);
					return mConv[(usize)i].fil;
				}
				int32 Actif() const {
					return mActif;
				}
				/// Le chat ouvert, et combien il y en a.
				int32 ChatActif() const {
					return mChat;
				}
				uint32 Chats() const {
					return (uint32)mConv.Size();
				}
				const NkAiFournisseurDesc *FournisseurActif() const {
					return (mActif >= 0 && mActif < (int32)fournisseurs.Size()) ? &fournisseurs[(usize)mActif] : nullptr;
				}

				/// LE SUJET : la premiere demande de la conversation, comme la capture.
				const char *Sujet() const {
					return SujetDe(Fil());
				}
				static const char *SujetDe(const NkAiFil &f) {
					for (uint32 i = 0; i < f.Taille(); ++i)
						if (f.At(i).type == NkAiBloc::Demande)
							return f.At(i).texte.CStr();
					return "Nouvelle conversation";
				}

				/// Change de fournisseur. Rend faux ET NOMME la raison.
				/// ⚠️ LE FIL NE BOUGE PAS (Q8) : c'est un reglage du chat ouvert.
				bool Choisir(int32 i, NkString &pourquoi) {
					Assurer();
					if (i < 0 || i >= (int32)fournisseurs.Size()) {
						pourquoi = NkString("aucun assistant a cet indice");
						return false;
					}
					if (i == mActif)
						return true;
					if (occupe) {
						pourquoi = NkString("une reponse est attendue du fournisseur courant : changez apres elle");
						return false;
					}
					mActif = i;
					RetenirReglages();
					return true;
				}

				/// UN CHAT NEUF : une page blanche. L'ancien RESTE, intact, dans
				/// l'historique (l'horloge de l'en-tete). Un chat deja vide n'en cree
				/// pas un second -- deux pages blanches ne se distinguent pas.
				void NouvelleConversation() {
					Assurer();
					if (Fil().Taille() == 0 && Blanc(mSaisie))
						return;
					RangerChat();
					NkAiConversation c;
					c.fil.Declarer(capacites);
					if (plafond > 0u)
						c.fil.PoserPlafond(plafond);
					mConv.PushBack(c);
					mChat = (int32)mConv.Size() - 1;
					PoserChat();
					RetenirReglages();
				}
				/// Ouvre le chat `k` parmi les AUTRES (0 = le plus ancien). Le chat
				/// courant reste ou il est : rien n'est perdu d'une bascule.
				void Rouvrir(int32 k) {
					Assurer();
					const int32 i = AutreVersChat(k);
					if (i >= 0)
						BasculerChat(i);
				}
				/// Bascule sur le chat `i` : son fil, son brouillon, son modele.
				void BasculerChat(int32 i) {
					Assurer();
					if (i < 0 || i >= (int32)mConv.Size() || i == mChat || occupe)
						return;
					RangerChat();
					mChat = i;
					PoserChat();
				}
				/// Supprime les AUTRES chats. ⚠️ ATTEIGNABLE : un historique qu'on ne
				/// peut pas vider est une dette de vie privee. Le chat OUVERT reste.
				void ViderHistorique() {
					Assurer();
					RangerChat();
					NkAiConversation garde = mConv[(usize)mChat];
					mConv.Clear();
					mConv.PushBack(garde);
					mChat = 0;
				}
				/// Le sujet de l'AUTRE chat `k` (0 = le plus ancien).
				const char *SujetArchive(uint32 k) const {
					const int32 i = AutreVersChat((int32)k);
					if (i < 0)
						return "";
					return SujetDe(i == mChat ? Fil() : mConv[(usize)i].fil);
				}
				/// Combien d'AUTRES chats l'historique propose.
				uint32 Archives() const {
					return mConv.Size() > 0 ? (uint32)mConv.Size() - 1u : 0u;
				}

				// ── LA PERSISTANCE DES CHATS (Q8) ─────────────────────────────
				/// Ecrit tous les chats. Rend faux si le fichier n'a pas pu l'etre.
				bool EnregistrerChats(const char *chemin) {
					if (!chemin || !chemin[0])
						return false;
					RangerChat();
					NkString o("nkaichats 1\n");
					char b[64];
					snprintf(b, sizeof(b), "actif\t%d\n", (int)mChat);
					o.Append(b);
					for (usize i = 0; i < mConv.Size(); ++i) {
						const NkAiConversation &c = mConv[i];
						o.Append("chat\t");
						Echapper(c.fournisseur, o);
						o.Append("\t");
						Echapper(c.modele, o);
						o.Append("\t");
						Echapper(c.brouillon, o);
						o.Append("\n");
						const NkAiFil &f = ((int32)i == mChat) ? Fil() : c.fil;
						for (uint32 k = 0; k < f.Taille(); ++k) {
							const NkAiBlocDonnees &d = f.At(k);
							snprintf(b, sizeof(b), "bloc\t%s\t%d", NkAiBlocNom(d.type), d.replie ? 1 : 0);
							o.Append(b);
							const NkString enClair(d.sortieEnClair ? "1" : "0");
							// (Q9) LES IMAGES JOINTES, leurs chemins separes par « | »
							NkString imgs;
							for (usize q = 0; q < d.images.Size(); ++q) {
								if (q)
									imgs.Append("|");
								imgs.Append(d.images[q].CStr());
							}
							// \U0001f534 (25/09) LA VIGNETTE DU RESULTAT SUIT LE CHAT.
							//    Elle etait calculee, affichee... et perdue a la fermeture : le
							//    fichier de chats portait DIX champs, aucun n'etait `vignette`.
							//    Rouvrir une conversation rendait donc le texte des etapes ET
							//    UNE TOILE VIDE a cote -- ce que le bloc disait avoir pose
							//    n'etait plus montrable.
							// FORME : « rapport;x,y,w,h,g|x,y,w,h,g|... », normalise 0..1. Une
							// vignette est une poignee de rectangles, pas une image : elle se
							// serialise en quelques dizaines d'octets, et elle se redessine a
							// n'importe quelle taille.
							NkString vign;
							if (d.vignette.Size() > 0) {
								char vb[64];
								snprintf(vb, sizeof(vb), "%.4f;", (double)d.vignetteRapport);
								vign.Append(vb);
								for (usize q = 0; q < d.vignette.Size(); ++q) {
									if (q)
										vign.Append("|");
									const NkAiBlocDonnees::Vignette &v = d.vignette[q];
									snprintf(vb, sizeof(vb), "%.4f,%.4f,%.4f,%.4f,%u", (double)v.x,
											 (double)v.y, (double)v.w, (double)v.h, (unsigned)v.genre);
									vign.Append(vb);
								}
							}
							const NkString *champs[11] = {&d.titre, &d.texte,  &d.entree,		   &d.sortie,		  &d.motif,
														  &d.effet, &enClair, &d.etiquetteEntree, &d.etiquetteSortie, &imgs,
														  &vign};
							for (int32 j = 0; j < 11; ++j) {
								o.Append("\t");
								Echapper(*champs[j], o);
							}
							o.Append("\n");
						}
					}
					mEmpreinteEcrite = Empreinte();
					return NkFile::WriteAllText(chemin, o.CStr());
				}
				/// Relit les chats. Rend faux si le fichier est absent ou d'un autre
				/// format -- rien n'est alors touche.
				bool ChargerChats(const char *chemin) {
					if (!chemin || !chemin[0] || !NkFile::Exists(chemin))
						return false;
					const NkString t = NkFile::ReadAllText(chemin);
					if (t.Find("nkaichats 1", 0) != 0u)
						return false;
					NkVector<NkAiConversation> lus;
					int32 actif = 0;
					const char *c = t.CStr();
					while (c && *c) {
						const char *fin = c;
						while (*fin && *fin != '\n')
							++fin;
						NkVector<NkString> ch;
						const char *d = c;
						for (const char *q = c;; ++q) {
							if (q == fin || *q == '\t') {
								NkString v;
								Deschapper(d, q, v);
								ch.PushBack(v);
								d = q + 1;
								if (q == fin)
									break;
							}
						}
						if (ch.Size() >= 2 && ch[0] == NkString("actif"))
							actif = (int32)atoi(ch[1].CStr());
						else if (ch.Size() >= 4 && ch[0] == NkString("chat")) {
							NkAiConversation cv;
							cv.fil.Declarer(capacites);
							if (plafond > 0u)
								cv.fil.PoserPlafond(plafond);
							cv.fournisseur = ch[1];
							cv.modele = ch[2];
							cv.brouillon = ch[3];
							lus.PushBack(cv);
						} else if (ch.Size() >= 9 && ch[0] == NkString("bloc") && lus.Size() > 0) {
							NkAiBlocDonnees bd;
							bd.type = TypeDe(ch[1]);
							bd.titre = ch[3];
							bd.texte = ch[4];
							bd.entree = ch[5];
							bd.sortie = ch[6];
							bd.motif = ch[7];
							bd.effet = ch[8];
							if (ch.Size() >= 12) {
								bd.sortieEnClair = ch[9] == NkString("1");
								bd.etiquetteEntree = ch[10];
								bd.etiquetteSortie = ch[11];
							}
							if (ch.Size() >= 13 && ch[12].Length() > 0) {
								const char *q = ch[12].CStr();
								while (*q) {
									const char *f = q;
									while (*f && *f != '|')
										++f;
									bd.images.PushBack(NkString(q, (NkString::SizeType)(f - q)));
									q = *f ? f + 1 : f;
								}
							}
							// (25/09) LA VIGNETTE, si le fichier la porte. Un chat ecrit AVANT
							// ce champ se relit sans erreur -- il n'aura simplement pas de
							// vignette, comme aujourd'hui. On n'invente rien a sa place.
							if (ch.Size() >= 14 && ch[13].Length() > 0) {
								const char *q = ch[13].CStr();
								bd.vignetteRapport = (float32)atof(q);
								while (*q && *q != ';')
									++q;
								if (*q == ';')
									++q;
								while (*q) {
									NkAiBlocDonnees::Vignette v;
									v.x = (float32)atof(q);
									int32 champ = 0;
									const char *f2 = q;
									while (*f2 && *f2 != '|') {
										if (*f2 == ',') {
											++champ;
											const char *val = f2 + 1;
											if (champ == 1)
												v.y = (float32)atof(val);
											else if (champ == 2)
												v.w = (float32)atof(val);
											else if (champ == 3)
												v.h = (float32)atof(val);
											else if (champ == 4)
												v.genre = (uint8)atoi(val);
										}
										++f2;
									}
									// \u26a0\ufe0f UN RECTANGLE DEGENERE N'EST PAS UNE VIGNETTE : le
									//    poser ferait un trait invisible que personne ne saurait
									//    expliquer. On le laisse tomber plutot que de le tracer.
									if (v.w > 0.f && v.h > 0.f)
										bd.vignette.PushBack(v);
									q = *f2 ? f2 + 1 : f2;
								}
							}
							NkString pq;
							NkAiFil &f = lus[lus.Size() - 1].fil;
							if (f.Pousser(bd, pq))
								if (NkAiBlocDonnees *m = f.MutableParId(f.At(f.Taille() - 1).id))
									m->replie = ch[2] == NkString("1");
						}
						c = *fin ? fin + 1 : fin;
					}
					if (lus.Size() == 0)
						return false;
					mConv = lus;
					mChat = (actif >= 0 && actif < (int32)mConv.Size()) ? actif : 0;
					PoserChat();
					mEmpreinteEcrite = Empreinte();
					return true;
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
					mSelComp = -1;
					// 🔴 LE BROUILLON RANGE DOIT PARTIR AVEC (24/09).
					//    Mesure de l'agent de la modelisation : la porte d'evenements a
					//    tape « un vase a fleurs » et l'application a envoye « Je veux un
					//    canar ninja » -- une demande d'une session precedente.
					//    `ViderSaisie` vidait le CHAMP (`mSaisie`) mais laissait
					//    `mConv[mChat].brouillon` intact : le brouillon est cense etre
					//    « ce qui n'a pas ete envoye », et il survivait a l'envoi. Le
					//    prochain `PoserChat` le reinjectait donc dans le composeur, par
					//    dessus ce que l'utilisateur venait de taper.
					// ⚠️ CE N'EST QUE LA MOITIE DU DEFAUT, et je le dis : il reste a
					//    prouver que `PoserChat` ne s'execute jamais ENTRE la frappe et
					//    l'envoi. Cette ligne ferme la voie « pas efface a l'envoi » ;
					//    la voie « relu au mauvais moment » n'est pas mesuree.
					if (mChat >= 0 && (usize)mChat < mConv.Size())
						mConv[(usize)mChat].brouillon = NkString();
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
					// LE PEINTRE DE CETTE IMAGE mesure la selection -- jamais celui d'une
					// image passee (il vivait sur la pile de l'hote : pointeur pendant).
					mMesureur = &p;
					mCtx = &ctx;
					NkAiFil &fil = Fil();
					fil.Declarer(capacites);
					if (plafond > 0u)
						fil.PoserPlafond(plafond);
					// (Q8) LES CHATS PERSISTANTS : relus une fois, puis reecrits a chaque
					// changement -- le contenu, une bascule, un reglage.
					if (!mChatsLus) {
						mChatsLus = true;
						if (cheminChats.Length() > 0)
							(void)ChargerChats(cheminChats.CStr());
					}
					NkAiConversation &conv = mConv[(usize)mChat];
					if (fil.Taille() == 0)
						conv.debut = -1.0;
					else if (conv.debut < 0.0 && maintenant >= 0.0)
						conv.debut = maintenant;

					NkAiMetriques m;
					m.Echelle(echelle);
					// (Q9) 8 PX DE MARGE A TOUTES LES LARGEURS : la regle « sous 560 px »
					// laissait 30 a 55 px ailleurs, et Rodolf les a vus (capture 172651).
					m.Compacter(echelle);
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
					cd.reserveHaut = mJointes.Size() > 0 ? 64.f * echelle : 0.f;
					cd.envoiActif = cd.envoiActif || (mJointes.Size() > 0 && (!fa || fa->pret));
					const float32 yComposeur = NkAiComposeurMesurer(cd, r.w, r.h, m, &Mesure, &p, planChrome);

					// (Q8) LES VIGNETTES DU COMPOSEUR, dans la place reservee en tete du cadre
					mJointesPtr.Clear();
					for (usize i = 0; i < mJointes.Size(); ++i)
						mJointesPtr.PushBack(mJointes[i].CStr());
					if (mJointes.Size() > 0) {
						NkAiRectPublie cad;
						if (planChrome.Trouver(0u, NkAiPiece::ComposeurCadre, cad)) {
							const float32 t = 52.f * echelle;
							for (usize i = 0; i < mJointes.Size(); ++i) {
								NkAiRectPublie im;
								im.piece = NkAiPiece::ImageJointe;
								im.x = cad.x + 12.f + (float32)i * (t + 8.f);
								im.y = cad.y + 8.f;
								im.w = t;
								im.h = t;
								im.debut = (uint32)i;
								planChrome.Ajouter(im);
								NkAiRectPublie x;
								x.piece = NkAiPiece::RetirerImage;
								x.w = x.h = 16.f * echelle;
								x.x = im.x + t - x.w * 0.6f;
								x.y = im.y - x.h * 0.4f;
								x.debut = (uint32)i;
								planChrome.Ajouter(x);
							}
						}
					}
					if (mImagesAAttacher.Size() > 0) {
						for (uint32 k = fil.Taille(); k > 0 && k + 4u > fil.Taille(); --k) {
							const NkAiBlocDonnees &bd = fil.At(k - 1);
							if (bd.type == NkAiBloc::Demande && bd.images.Size() == 0 &&
								(mTexteAAttacher.Length() == 0 || bd.texte.Find(mTexteAAttacher.CStr(), 0) != NkString::npos ||
								 mTexteAAttacher.Find(bd.texte.CStr(), 0) != NkString::npos)) {
								if (NkAiBlocDonnees *mb = fil.MutableParId(bd.id))
									mb->images = mImagesAAttacher;
								mImagesAAttacher.Clear();
								break;
							}
						}
						if (++mAttacheAttente > 120u) { // la demande n'est jamais venue : on n'attache rien
							mImagesAAttacher.Clear();
							mAttacheAttente = 0u;
						}
					}

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

					// ── 4 bis. LA SELECTION (Q8) : le glisser, Ctrl+A / Ctrl+C du fil,
					//    le surlignage et le bouton « copier » du bloc survole ──
					Selection(ctx, focus, p, out);

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
					ch.images = mJointesPtr.Size() ? mJointesPtr.Data() : nullptr;
					ch.imagesN = (uint32)mJointesPtr.Size();
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
					mMesureur = nullptr;
					mCtx = nullptr;
					if (out.modeleChange)
						RetenirReglages();
					// (Q8) UNE BASCULE DE CHAT a pose SES reglages : l'hote les applique
					// (dorsal, modele) comme s'ils avaient ete choisis a la main.
					if (mBasculeReglage) {
						mBasculeReglage = false;
						out.fournisseurChange = true;
						out.ancien = -1;
						out.nouveau = mActif;
						out.modeleChange = true;
					}
					if (cheminChats.Length() > 0 && Empreinte() != mEmpreinteEcrite)
						(void)EnregistrerChats(cheminChats.CStr());
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
					const usize n = fournisseurs.Size() > 0 ? fournisseurs.Size() : 1u;
					if (mConv.Size() == 0)
						mConv.Resize(1);
					if (mChat < 0 || mChat >= (int32)mConv.Size())
						mChat = 0;
					if (mActif >= (int32)n)
						mActif = 0;
				}

				// ── LES CHATS (Q8) ──────────────────────────────────────────────
				/// Le chat ouvert est RANGE : le fil vivant de l'hote, le brouillon.
				void RangerChat() {
					NkAiConversation &c = mConv[(usize)mChat];
					if (mVivant)
						c.fil = *mVivant;
					c.brouillon = NkString(mSaisie);
					RetenirReglages(); // le chat qu'on quitte garde SES reglages
				}
				/// Le chat `mChat` est POSE : son fil chez l'hote, son brouillon dans
				/// le composeur, ses reglages dans la pastille.
				void PoserChat() {
					NkAiConversation &c = mConv[(usize)mChat];
					if (mVivant)
						*mVivant = c.fil;
					PoserSaisie(c.brouillon.CStr());
					for (usize i = 0; i < fournisseurs.Size(); ++i)
						if (c.fournisseur.Length() > 0 && fournisseurs[i].cle == c.fournisseur) {
							if ((int32)i != mActif) {
								mActif = (int32)i;
								mBasculeReglage = true;
							}
							NkAiFournisseurDesc &f = fournisseurs[i];
							for (usize k = 0; k < f.modeles.Size(); ++k)
								if (f.modeles[k].nom == c.modele && f.modele != (int32)k) {
									f.modele = (int32)k;
									mBasculeReglage = true;
								}
						}
					c.colle = true;
				}
				/// Les reglages COURANTS sont ceux du chat ouvert.
				void RetenirReglages() {
					if (mConv.Size() == 0)
						return;
					NkAiConversation &c = mConv[(usize)mChat];
					const NkAiFournisseurDesc *f = FournisseurActif();
					if (!f)
						return;
					c.fournisseur = f->cle;
					c.modele = (f->modele >= 0 && f->modele < (int32)f->modeles.Size())
								   ? f->modeles[(usize)f->modele].nom
								   : NkString();
				}
				/// L'AUTRE chat `k` (0 = le plus ancien, le chat ouvert exclu) -> indice.
				int32 AutreVersChat(int32 k) const {
					if (k < 0)
						return -1;
					int32 n = 0;
					for (int32 i = 0; i < (int32)mConv.Size(); ++i) {
						if (i == mChat)
							continue;
						if (n == k)
							return i;
						++n;
					}
					return -1;
				}
				/// Une empreinte BON MARCHE de tout ce qui se persiste : sans elle,
				/// on ecrirait le fichier a chaque image.
				uint64 Empreinte() const {
					uint64 h = 1469598103934665603ull;
					auto mix = [&](uint64 v) { h = (h ^ v) * 1099511628211ull; };
					mix((uint64)mConv.Size());
					mix((uint64)mChat);
					for (usize i = 0; i < mConv.Size(); ++i) {
						const NkAiConversation &c = mConv[i];
						const NkAiFil &f = ((int32)i == mChat) ? Fil() : c.fil;
						mix((uint64)f.Taille());
						for (uint32 k = 0; k < f.Taille(); ++k) {
							const NkAiBlocDonnees &d = f.At(k);
							mix((uint64)d.id);
							mix((uint64)(d.replie ? 1 : 0));
							mix((uint64)(d.texte.Length() + d.sortie.Length() * 3u + d.effet.Length() * 7u +
										 d.entree.Length() * 11u + d.motif.Length() * 13u + d.titre.Length() * 17u));
						}
						mix((uint64)(((int32)i == mChat) ? Longueur(mSaisie) : c.brouillon.Length()));
						for (usize k = 0; k < c.modele.Length(); ++k)
							mix((uint64)(unsigned char)c.modele.CStr()[k]);
						for (usize k = 0; k < c.fournisseur.Length(); ++k)
							mix((uint64)(unsigned char)c.fournisseur.CStr()[k]);
					}
					return h;
				}
				static uint32 Longueur(const char *s) {
					uint32 n = 0;
					while (s && s[n])
						++n;
					return n;
				}
				static void Echapper(const NkString &v, NkString &o) {
					for (usize i = 0; i < v.Length(); ++i) {
						const char c = v.CStr()[i];
						if (c == '\\')
							o.Append("\\\\");
						else if (c == '\t')
							o.Append("\\t");
						else if (c == '\n')
							o.Append("\\n");
						else if (c == '\r')
							o.Append("\\r");
						else
							o.Append(&c, 1);
					}
				}
				static void Deschapper(const char *a, const char *b, NkString &o) {
					for (const char *q = a; q < b; ++q) {
						if (*q == '\\' && q + 1 < b) {
							++q;
							const char c = *q == 'n' ? '\n' : (*q == 't' ? '\t' : (*q == 'r' ? '\r' : *q));
							o.Append(&c, 1);
						} else if (*q != '\r')
							o.Append(q, 1);
					}
				}
				static NkAiBloc TypeDe(const NkString &nom) {
					for (uint8 t = 0; t < 16; ++t)
						if (nom == NkString(NkAiBlocNom((NkAiBloc)t)))
							return (NkAiBloc)t;
					return NkAiBloc::Prose;
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
					// ── Q8 : LA SELECTION DU COMPOSEUR (Ctrl+A, Ctrl+C, Ctrl+X, et la frappe
					//    ou le collage REMPLACENT ce qui est selectionne) ──
					auto bornes = [&](int32 &a, int32 &b) {
						a = mSelComp < mCaret ? mSelComp : mCaret;
						b = mSelComp < mCaret ? mCaret : mSelComp;
						if (a < 0)
							a = 0;
						if (b > len)
							b = len;
					};
					// UNE ANCRE SANS ETENDUE N'EST PAS UNE SELECTION : laissee en place, la
					// frappe qui suit « selectionnait » tout ce qu'on tapait (mesure Q9).
					if (mSelComp == mCaret && !mCompGlisse) // pas pendant un glisser : l'ancre en EST le debut
						mSelComp = -1;
					auto aSelection = [&]() { return mSelComp >= 0 && mSelComp != mCaret; };
					if (in.wantSelectAll) {
						mSelComp = 0;
						mCaret = len;
						in.wantSelectAll = false;
					}
					if ((in.wantCopy || in.wantCut) && aSelection()) {
						int32 a = 0, b = 0;
						bornes(a, b);
						NkString t(mSaisie + a, (NkString::SizeType)(b - a));
						ctx.SetClipboard(t.CStr());
						out.copie = true;
						out.copieTexte = t;
						if (in.wantCut) {
							effacer(a, b);
							mSelComp = -1;
						}
						in.wantCopy = in.wantCut = false;
					}
					const bool frappe = in.charCount > 0 || in.wantPaste ||
										in.KeyPressedRepeat(nkgui::NkGuiKey::Backspace) ||
										in.KeyPressedRepeat(nkgui::NkGuiKey::Delete);
					if (frappe && aSelection()) {
						int32 a = 0, b = 0;
						bornes(a, b);
						effacer(a, b);
						mSelComp = -1;
						// la touche d'effacement a deja fait son office : on la consomme
						if (in.charCount == 0 && !in.wantPaste) {
							mEffaceDeja = true;
						}
					}
					if (in.KeyPressed(nkgui::NkGuiKey::Left) || in.KeyPressed(nkgui::NkGuiKey::Right) ||
						in.KeyPressed(nkgui::NkGuiKey::Home) || in.KeyPressed(nkgui::NkGuiKey::End)) {
						if (in.shiftDown) {
							if (mSelComp < 0)
								mSelComp = mCaret;
						} else
							mSelComp = -1;
					}
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
					if (in.KeyPressedRepeat(nkgui::NkGuiKey::Backspace) && !mEffaceDeja)
						effacer(precedent(mCaret), mCaret);
					if (in.KeyPressedRepeat(nkgui::NkGuiKey::Delete) && !mEffaceDeja)
						effacer(mCaret, suivant(mCaret));
					mEffaceDeja = false;
					if (in.KeyPressedRepeat(nkgui::NkGuiKey::Left))
						mCaret = precedent(mCaret);
					if (in.KeyPressedRepeat(nkgui::NkGuiKey::Right))
						mCaret = suivant(mCaret);
					if (in.KeyPressed(nkgui::NkGuiKey::Home))
						mCaret = 0;
					if (in.KeyPressed(nkgui::NkGuiKey::End))
						mCaret = len;
					if (in.wantPaste && accepteImages && CollerImage(ctx))
						in.wantPaste = false; // (Q9) un BITMAP copie devient une piece jointe
					if (in.wantPaste && accepteImages) {
						// (Q8) UN CHEMIN D'IMAGE COLLE se joint au lieu de s'ecrire. ⚠️ Le
						//    presse-papiers IMAGE (un bitmap copie) n'est pas lisible : la
						//    fenetre du depot ne rend que du texte (NkWindow::GetClipboardText).
						NkString cb = ctx.GetClipboard();
						cb.Trim();
						if (cb.Length() > 4 && cb.Find("\n", 0) == NkString::npos && EstCheminImage(cb.CStr())) {
							NkString pq;
							if (JoindreImage(cb.CStr(), pq))
								in.wantPaste = false;
						}
					}
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
					if ((occupe && !fileAttente) || (Blanc(mSaisie) && mJointes.Size() == 0))
						return;
					const NkAiFournisseurDesc *fa = FournisseurActif();
					if (fa && !fa->pret)
						return; // l'envoi est ETEINT : la pastille dit pourquoi
					out.envoyer = true;
					out.texte = NkString(mSaisie);
					out.mode = mode;
					// (Q8) LES IMAGES PARTENT AVEC, et se rattachent a la demande que
					// l'hote posera dans le fil (dans les images qui suivent).
					out.images = mJointes;
					if (mJointes.Size() > 0) {
						mImagesAAttacher = mJointes;
						mTexteAAttacher = NkString(mSaisie);
						mAttacheAttente = 0u;
					}
					mJointes.Clear();
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
					QViderHist,
					QImage,
					// (Q9) le menu contextuel
					QCouper,
					QCopierComp,
					QColler,
					QToutComp,
					QCopierSel,
					QToutFil,
					QCopierBloc
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
							if (accepteImages) {
								LigneMenu l;
								l.quoi = QImage;
								l.texte = NkString("Joindre une image…");
								l.detail = NkString("aussi : Ctrl+V d'un chemin, ou deposer le fichier sur le panneau");
								l.deuxLignes = true;
								mLignes.PushBack(l);
							}
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
						case NkAiMenu::Contexte: {
							auto ligneCtx = [&](uint8 quoi, const char *t, const char *raccourci, bool eteint) {
								LigneMenu l;
								l.quoi = quoi;
								l.texte = NkString(t);
								l.droite = NkString(raccourci);
								l.eteint = eteint;
								mLignes.PushBack(l);
							};
							if (mCtxComposeur) {
								const bool sel = mSelComp >= 0 && mSelComp != mCaret;
								ligneCtx(QCouper, "Couper", "Ctrl+X", !sel);
								ligneCtx(QCopierComp, "Copier", "Ctrl+C", !sel);
								ligneCtx(QColler, "Coller", "Ctrl+V", false);
								ligneCtx(QToutComp, "Tout selectionner", "Ctrl+A", mSaisie[0] == 0);
							} else {
								ligneCtx(QCopierSel, "Copier", "Ctrl+C", mTexteSel.Length() == 0);
								ligneCtx(QToutFil, "Tout selectionner", "Ctrl+A", Fil().Taille() == 0);
								ligneCtx(QCopierBloc, "Copier le bloc", "", mCtxBloc == 0u);
							}
							break;
						}
						case NkAiMenu::Historique: {
							// LES AUTRES CHATS, le plus recent en tete : un clic y bascule,
							// sans rien perdre de celui qu'on quitte.
							const uint32 n = Archives();
							for (uint32 k = n; k > 0; --k) {
								const int32 i = AutreVersChat((int32)(k - 1));
								if (i < 0)
									continue;
								const NkAiConversation &cv = mConv[(usize)i];
								LigneMenu l;
								l.quoi = QArchive;
								l.arg = (int32)(k - 1);
								l.texte = NkString(SujetDe(cv.fil));
								char d[128];
								snprintf(d, sizeof(d), "%u bloc(s)%s%s", (unsigned)cv.fil.Taille(),
										 cv.modele.Length() ? " · " : "", cv.modele.CStr());
								l.detail = NkString(d);
								l.deuxLignes = true;
								mLignes.PushBack(l);
							}
							if (n == 0)
								Note("Aucun autre chat : « + » en haut en ouvre un.");
							else {
								LigneMenu l;
								l.quoi = QViderHist;
								l.texte = NkString("Supprimer les autres chats");
								mLignes.PushBack(l);
							}
							if (cheminChats.Length() == 0)
								Note("Les chats ne survivent pas a la fermeture.");
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

				// ══════════════ Q8 : SELECTIONNER ET COPIER ══════════════
				// ⚠️ UN POINT DE SELECTION EST (bloc, rang, octet), JAMAIS un indice de
				//    plan : le plan est reconstruit a chaque image, et un tour qui ecrit
				//    decale les indices -- la selection glisserait sur un autre texte.
				static bool Selectionnable(const NkAiRectPublie &q) {
					return q.blocId != 0u && (q.piece == NkAiPiece::Fragment || q.piece == NkAiPiece::Titre ||
											  q.piece == NkAiPiece::TexteIn || q.piece == NkAiPiece::TexteOut ||
											  q.piece == NkAiPiece::Effet);
				}
				bool Tranche(const NkAiRectPublie &q, const char *&a, const char *&b) const {
					NkAiChromeTextes vide;
					vide.actions = &actions;
					if (!aipaint::TextePiece(Fil(), q, vide, a, b) || !a)
						return false;
					if (!b) {
						b = a;
						while (*b)
							++b;
					}
					return true;
				}
				bool PremiereLigneDuBloc(const NkAiRectPublie &q) const {
					for (uint32 i = 0; i < planFil.Pieces(); ++i) {
						const NkAiRectPublie &o = planFil.Piece(i);
						if (o.blocId == q.blocId && Selectionnable(o))
							return o.y >= q.y - 0.5f;
					}
					return true;
				}
				PointSel VersPoint(int32 k, uint32 off) const {
					PointSel s;
					const NkAiRectPublie &q = planFil.Piece((uint32)k);
					uint32 ord = 0;
					for (int32 i = 0; i < k; ++i)
						if (planFil.Piece((uint32)i).blocId == q.blocId && Selectionnable(planFil.Piece((uint32)i)))
							++ord;
					s.bloc = q.blocId;
					s.ordinal = ord;
					s.off = off;
					s.valide = true;
					return s;
				}
				int32 IndiceDe(const PointSel &s) const {
					if (!s.valide)
						return -1;
					uint32 ord = 0;
					for (uint32 i = 0; i < planFil.Pieces(); ++i) {
						const NkAiRectPublie &q = planFil.Piece(i);
						if (q.blocId != s.bloc || !Selectionnable(q))
							continue;
						if (ord == s.ordinal)
							return (int32)i;
						++ord;
					}
					return -1;
				}
				/// L'octet sous `fx` dans le morceau `q` : on mesure les prefixes, par
				/// point de code, avec la police qui PEINT ce morceau.
				uint32 OctetSous(const NkAiRectPublie &q, float32 fx, NkComponentPaint &p) const {
					const char *a = nullptr, *b = nullptr;
					if (!Tranche(q, a, b))
						return 0u;
					const float32 cible = fx - q.x;
					if (cible <= 0.f)
						return 0u;
					const char *c = a;
					float32 avant = 0.f;
					while (c < b) {
						const char *n = c + aidetail::LongueurCp((unsigned char)*c);
						if (n > b)
							n = b;
						const float32 w = p.LargeurPolice(a, n, (uint8)q.police);
						if (w >= cible)
							return (uint32)((cible - avant < w - cible) ? c - a : n - a);
						avant = w;
						c = n;
					}
					return (uint32)(b - a);
				}
				/// Le morceau de texte sous (fx, fy), ou le plus proche sur sa ligne.
				bool PointSous(float32 fx, float32 fy, int32 &k, uint32 &off) {
					k = -1;
					float32 meilleur = 1.0e30f;
					for (uint32 i = 0; i < planFil.Pieces(); ++i) {
						const NkAiRectPublie &q = planFil.Piece(i);
						if (!Selectionnable(q) || fy < q.y || fy >= q.y + q.h)
							continue;
						const float32 d = fx < q.x ? q.x - fx : (fx > q.x + q.w ? fx - q.x - q.w : 0.f);
						if (d < meilleur) {
							meilleur = d;
							k = (int32)i;
						}
					}
					if (k < 0 || meilleur > 40.f)
						return false;
					off = mMesureur ? OctetSous(planFil.Piece((uint32)k), fx, *mMesureur) : 0u;
					return true;
				}
				void Ordonner(int32 &ia, uint32 &oa, int32 &ib, uint32 &ob) const {
					if (ia > ib || (ia == ib && oa > ob)) {
						const int32 ti = ia;
						ia = ib;
						ib = ti;
						const uint32 to = oa;
						oa = ob;
						ob = to;
					}
				}

				void Selection(nkgui::NkGuiContext &ctx, bool focusComposeur, NkComponentPaint &p, NkAiSorties &out) {
					mMesureur = &p;
					auto &in = ctx.input;
					// LE GLISSER : tant que le bouton est tenu, la fin suit la souris --
					// et au-dela du bord, elle suit la ligne la plus proche.
					if (mSelEnCours) {
						if (in.mouseDown[0]) {
							const float32 fx = in.mousePos.x - filOx, fy = in.mousePos.y - filOy;
							int32 k = -1;
							uint32 off = 0;
							if (PointSous(fx, fy, k, off))
								mSelB = VersPoint(k, off);
						} else
							mSelEnCours = false;
					}
					// (Q9) LE GLISSER DANS LE COMPOSEUR : la fin suit la souris
					if (mCompGlisse) {
						if (in.mouseDown[0])
							mCaret = OctetComposeurSous(in.mousePos.x - rect.x, in.mousePos.y - rect.y);
						else {
							mCompGlisse = false;
							if (mSelComp == mCaret)
								mSelComp = -1; // un clic sans glisser pose le curseur, rien de plus
						}
					}
					// Ctrl+A / Ctrl+C DU FIL : quand le fil a la main (le composeur, lui,
					// les traite dans `Clavier`).
					if (mFilFocus && !focusComposeur) {
						if (in.wantSelectAll) {
							mSelTout = true;
							in.wantSelectAll = false;
						}
						if (in.KeyPressed(nkgui::NkGuiKey::Escape))
							EffacerSelection();
					}
					if (mSelTout) {
						int32 premier = -1, dernier = -1;
						for (uint32 i = 0; i < planFil.Pieces(); ++i)
							if (Selectionnable(planFil.Piece(i))) {
								if (premier < 0)
									premier = (int32)i;
								dernier = (int32)i;
							}
						if (premier >= 0) {
							const char *a = nullptr, *b = nullptr;
							mSelA = VersPoint(premier, 0u);
							mSelB = VersPoint(dernier, Tranche(planFil.Piece((uint32)dernier), a, b) ? (uint32)(b - a) : 0u);
						}
					}
					// LE TEXTE ET LE SURLIGNAGE
					mTexteSel = NkString();
					int32 ia = IndiceDe(mSelA), ib = IndiceDe(mSelB);
					uint32 oa = mSelA.off, ob = mSelB.off;
					if (ia >= 0 && ib >= 0 && !(ia == ib && oa == ob)) {
						Ordonner(ia, oa, ib, ob);
						const char *precB = nullptr;
						uint32 precBloc = 0u;
						float32 precY = -1.f;
						NkAiPiece precPiece = NkAiPiece::Count;
						for (int32 i = ia; i <= ib; ++i) {
							const NkAiRectPublie &q = planFil.Piece((uint32)i);
							if (!Selectionnable(q))
								continue;
							const char *a = nullptr, *b = nullptr;
							if (!Tranche(q, a, b))
								continue;
							const uint32 lon = (uint32)(b - a);
							const uint32 d0 = (i == ia) ? (oa < lon ? oa : lon) : 0u;
							const uint32 d1 = (i == ib) ? (ob < lon ? ob : lon) : lon;
							if (d1 <= d0 && i != ia)
								continue;
							// LA JOINTURE : le texte d'origine entre deux lignes d'un meme
							// paragraphe (l'espace ou le saut que le repli a mange), sinon
							// un saut de ligne entre deux lignes, rien sur une meme ligne.
							if (precB) {
								if (q.blocId == precBloc && q.piece == precPiece && a >= precB && a - precB <= 4)
									mTexteSel.Append(precB, (NkString::SizeType)(a - precB));
								else if (q.y > precY + 0.5f)
									mTexteSel.Append("\n");
							}
							mTexteSel.Append(a + d0, (NkString::SizeType)(d1 - d0));
							precB = a + d1;
							precBloc = q.blocId;
							precY = q.y;
							precPiece = q.piece;
							const float32 x0 = q.x + (d0 ? p.LargeurPolice(a, a + d0, (uint8)q.police) : 0.f);
							float32 x1 = q.x + p.LargeurPolice(a, a + d1, (uint8)q.police);
							if (x1 > q.x + q.w)
								x1 = q.x + q.w; // le code rogne au bord : le surlignage aussi
							NkAiRectPublie h;
							h.blocId = q.blocId;
							h.piece = NkAiPiece::Surlignage;
							h.x = x0;
							h.y = q.y;
							h.w = (x1 - x0) > 2.f ? x1 - x0 : 2.f;
							h.h = q.h;
							planFil.Ajouter(h);
						}
					}
					if (mFilFocus && !focusComposeur && (in.wantCopy || in.wantCut)) {
						if (mTexteSel.Length() > 0) {
							ctx.SetClipboard(mTexteSel.CStr());
							out.copie = true;
							out.copieTexte = mTexteSel;
						}
						in.wantCopy = in.wantCut = false;
					}
					// LE SURLIGNAGE DU COMPOSEUR
					{
						int32 len = 0;
						while (mSaisie[len])
							++len;
						if (mSelComp > len)
							mSelComp = len;
						if (mSelComp >= 0 && mSelComp != mCaret && focusComposeur) {
							const int32 a = mSelComp < mCaret ? mSelComp : mCaret;
							const int32 b = mSelComp < mCaret ? mCaret : mSelComp;
							for (uint32 i = 0; i < planChrome.Pieces(); ++i) {
								const NkAiRectPublie &q = planChrome.Piece(i);
								if (q.piece != NkAiPiece::ComposeurTexte || q.source != NkAiSource::Saisie)
									continue;
								const int32 d0 = (int32)q.debut, d1 = (int32)(q.debut + q.longueur);
								const int32 s0 = a > d0 ? a : d0, s1 = b < d1 ? b : d1;
								if (s1 <= s0)
									continue;
								NkAiRectPublie h;
								h.piece = NkAiPiece::Surlignage;
								h.x = q.x + p.LargeurPolice(mSaisie + d0, mSaisie + s0, 0u);
								h.y = q.y;
								h.w = p.LargeurPolice(mSaisie + s0, mSaisie + s1, 0u);
								h.h = q.h;
								planChrome.Ajouter(h);
							}
						}
					}
					// LE BOUTON « COPIER » : au survol d'un bloc, en haut a droite de lui.
					mCopieBloc = 0u;
					const float32 mx = in.mousePos.x, my = in.mousePos.y;
					if (vueFil.Contains(mx, my) && mMenu == NkAiMenu::Aucun) {
						const float32 fx = mx - filOx, fy = my - filOy;
						uint32 bloc = 0u;
						for (uint32 i = 0; i < planFil.Pieces() && bloc == 0u; ++i) {
							const NkAiRectPublie &q = planFil.Piece(i);
							if (q.blocId != 0u && q.piece != NkAiPiece::Surlignage && fx >= q.x - 4.f &&
								fx < q.x + q.w + 30.f && fy >= q.y && fy < q.y + q.h)
								bloc = q.blocId;
						}
						if (bloc != 0u) {
							float32 x1 = -1.f, y0 = 1.0e30f;
							for (uint32 i = 0; i < planFil.Pieces(); ++i) {
								const NkAiRectPublie &q = planFil.Piece(i);
								if (q.blocId != bloc || q.piece == NkAiPiece::Rail)
									continue;
								if (q.x + q.w > x1)
									x1 = q.x + q.w;
								if (q.y < y0)
									y0 = q.y;
							}
							const float32 c = 20.f * echelle;
							NkAiRectPublie bc;
							bc.blocId = bloc;
							bc.piece = NkAiPiece::BoutonCopier;
							bc.x = x1 + 4.f;
							if (bc.x + c > rect.w - 4.f)
								bc.x = rect.w - 4.f - c;
							bc.y = y0;
							bc.w = c;
							bc.h = c;
							if (fx >= bc.x && fx < bc.x + c && fy >= bc.y && fy < bc.y + c)
								bc.drapeaux |= kAiSurvol;
							planFil.Ajouter(bc);
							mCopieBloc = bloc;
							mCopieRect = {bc.x, bc.y, bc.w, bc.h};
						}
					}
				}

				/// Tout le texte d'un bloc -- ce que le bouton « copier » met au
				/// presse-papiers : la demande, la reponse, ou l'etape entiere.
				void CopierBloc(nkgui::NkGuiContext &ctx, uint32 bloc, NkAiSorties &out) {
					uint32 idx = 0;
					if (!Fil().TrouverParId(bloc, idx))
						return;
					const NkAiBlocDonnees &b = Fil().At(idx);
					NkString t;
					auto ajoute = [&](const NkString &s) {
						if (s.Length() == 0)
							return;
						if (t.Length() > 0)
							t.Append("\n");
						t.Append(s.CStr());
					};
					ajoute(b.titre);
					ajoute(b.texte);
					ajoute(b.entree);
					ajoute(b.sortie);
					ajoute(b.effet);
					ajoute(b.motif);
					ctx.SetClipboard(t.CStr());
					out.copie = true;
					out.copieTexte = t;
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
					if (mMenu == NkAiMenu::Contexte) {
						// SOUS LA SOURIS, sans sortir du panneau
						w = 220.f * echelle;
						x = mCtxX;
						y = mCtxY;
						if (x + w > r.w - 4.f)
							x = r.w - 4.f - w;
						if (y + h > r.h - 4.f)
							y = mCtxY - h;
						if (x < 4.f)
							x = 4.f;
					} else if (mMenu == NkAiMenu::Historique) {
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
							case NkAiPiece::RetirerImage:
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
					// (Q9) LE CLIC DROIT : un menu contextuel, dans le fil ou dans le composeur.
					if (dedans && ctx.input.mouseClicked[1]) {
						NkAiRectPublie cad;
						const bool dansComposeur = planChrome.Trouver(0u, NkAiPiece::ComposeurCadre, cad) &&
												   px >= cad.x && px < cad.x + cad.w && py >= cad.y && py < cad.y + cad.h;
						mCtxComposeur = dansComposeur;
						mCtxBloc = 0u;
						if (!dansComposeur && vueFil.Contains(ctx.input.mousePos.x, ctx.input.mousePos.y)) {
							const float32 fx = ctx.input.mousePos.x - filOx, fy = ctx.input.mousePos.y - filOy;
							for (uint32 i = 0; i < planFil.Pieces() && mCtxBloc == 0u; ++i) {
								const NkAiRectPublie &q = planFil.Piece(i);
								if (q.blocId != 0u && q.piece != NkAiPiece::Surlignage && fx >= q.x && fx < q.x + q.w &&
									fy >= q.y && fy < q.y + q.h)
									mCtxBloc = q.blocId;
							}
						}
						if (dansComposeur) {
							ctx.inputId = IdComposeur(); // le menu agit sur le composeur
							mFilFocus = false;
						}
						mCtxX = px;
						mCtxY = py;
						mMenu = NkAiMenu::Contexte;
						ctx.input.mouseClicked[1] = false;
						out.clicPris = true;
						return;
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
					if (!dedans) {
						mFilFocus = false;
						return;
					}
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
							case NkAiPiece::RetirerImage:
								RetirerImage(q.debut);
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
								if (entreesPlus.Size() > 0 || accepteImages)
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
							case NkAiPiece::ComposeurCadre: {
								// LE FOCUS CLAVIER, et le clic est PRIS : sinon NKGui
								// defocaliserait a la fin de cette meme image.
								mFilFocus = false;
								ctx.inputId = IdComposeur();
								ctx.inputClickConsumed = true;
								// 🔴 (Q9) LE CLIC POSAIT LE CURSEUR EN FIN DE TEXTE, TOUJOURS : on
								//    ne pouvait donc ni selectionner a la souris, ni copier ce
								//    qu'on ne pouvait pas selectionner (Rodolf, 172651). Le banc
								//    passait par Ctrl+A, jamais par la souris.
								const int32 pos = OctetComposeurSous(px, py);
								if (ctx.input.mouseDoubleClicked[0]) {
									SelectionnerMot(pos);
									return;
								}
								if (ctx.input.shiftDown) {
									if (mSelComp < 0)
										mSelComp = mCaret;
								} else
									mSelComp = pos;
								mCaret = pos;
								mCompGlisse = true;
								return;
							}
							default: return;
						}
					}
					if (surFenetre)
						return;
					// ── LE FIL : plier / deplier, et les boutons d'un bloc ──
					if (vueFil.Contains(ctx.input.mousePos.x, ctx.input.mousePos.y)) {
						const float32 fx = ctx.input.mousePos.x - filOx, fy = ctx.input.mousePos.y - filOy;
						// (Q8) LE BOUTON « COPIER » du bloc survole passe AVANT tout le reste
						if (mCopieBloc != 0u && fx >= mCopieRect.x && fx < mCopieRect.x + mCopieRect.w &&
							fy >= mCopieRect.y && fy < mCopieRect.y + mCopieRect.h) {
							CopierBloc(ctx, mCopieBloc, out);
							return;
						}
						// (Q8) UNE SELECTION COMMENCE sur un texte du fil -- sauf sur la
						// ligne-titre d'une etape, qui plie et deplie comme avant.
						{
							int32 k = -1;
							uint32 off = 0;
							if (PointSous(fx, fy, k, off)) {
								const NkAiRectPublie &q = planFil.Piece((uint32)k);
								uint32 idx = 0;
								const bool etape = Fil().TrouverParId(q.blocId, idx) &&
												   (Fil().At(idx).type == NkAiBloc::Outil ||
													Fil().At(idx).type == NkAiBloc::Reflexion);
								const bool ligneTitre =
									q.piece == NkAiPiece::Titre || (q.piece == NkAiPiece::Fragment && etape && fy < q.y + m.ligne &&
																	PremiereLigneDuBloc(q));
								if (!ligneTitre) {
									mSelA = mSelB = VersPoint(k, off);
									mSelTout = false;
									mSelEnCours = true;
									mFilFocus = true;
									if (ctx.inputId == IdComposeur())
										ctx.inputId = nkgui::NKGUI_ID_NONE; // Ctrl+C va au fil
									return;
								}
							}
						}
						EffacerSelection();
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
						case QCouper:
						case QCopierComp:
							if (mCtx && mSelComp >= 0 && mSelComp != mCaret) {
								const int32 a = mSelComp < mCaret ? mSelComp : mCaret;
								const int32 b = mSelComp < mCaret ? mCaret : mSelComp;
								NkString t(mSaisie + a, (NkString::SizeType)(b - a));
								mCtx->SetClipboard(t.CStr());
								out.copie = true;
								out.copieTexte = t;
								if (l.quoi == QCouper) {
									int32 len = 0;
									while (mSaisie[len])
										++len;
									for (int32 k = a; k + (b - a) <= len; ++k)
										mSaisie[k] = mSaisie[k + (b - a)];
									mCaret = a;
									mSelComp = -1;
								}
							}
							FermerMenus();
							return;
						case QColler:
							// LE MEME CHEMIN que Ctrl+V : le drapeau, lu par `Clavier`
							if (mCtx) {
								mCtx->input.wantPaste = true;
								mCtx->inputId = IdComposeur();
							}
							FermerMenus();
							return;
						case QToutComp: {
							int32 len = 0;
							while (mSaisie[len])
								++len;
							mSelComp = 0;
							mCaret = len;
							if (mCtx)
								mCtx->inputId = IdComposeur();
							FermerMenus();
							return;
						}
						case QCopierSel:
							if (mCtx && mTexteSel.Length() > 0) {
								mCtx->SetClipboard(mTexteSel.CStr());
								out.copie = true;
								out.copieTexte = mTexteSel;
							}
							FermerMenus();
							return;
						case QToutFil:
							ToutSelectionnerFil();
							FermerMenus();
							return;
						case QCopierBloc:
							if (mCtx && mCtxBloc != 0u)
								CopierBloc(*mCtx, mCtxBloc, out);
							FermerMenus();
							return;
						case QImage:
							out.joindreImage = true; // l'hote ouvre SON selecteur
							FermerMenus();
							return;
						case QViderHist:
							ViderHistorique(); // ATTEIGNABLE
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
				int32 mChat = 0;
				bool mChatsLus = false;
				bool mBasculeReglage = false;
				uint64 mEmpreinteEcrite = 0u;
				int32 mActif = 0;
				NkAiFil *mVivant = nullptr;
				char mSaisie[8192] = {0};
				int32 mCaret = 0;
				NkAiMenu mMenu = NkAiMenu::Aucun;
				int32 mMenuFournisseur = -1;
				NkString mRefus;
				int32 mRefusFournisseur = -1;
				NkVector<LigneMenu> mLignes;
				// ── Q8 : la selection ──
				PointSel mSelA, mSelB;
				bool mSelEnCours = false, mSelTout = false, mFilFocus = false;
				NkString mTexteSel;
				int32 mSelComp = -1; ///< l'ancre de la selection du composeur (-1 = aucune)
				bool mEffaceDeja = false;
				uint32 mCopieBloc = 0u;
				NkPaintRect mCopieRect;
				float32 mMenuDefile = 0.f;
				NkAiMenu mMenuPrecedent = NkAiMenu::Aucun;
				NkPaintRect mMenuRect;
				NkComponentPaint *mMesureur = nullptr;
				nkgui::NkGuiContext *mCtx = nullptr;
				bool mCompGlisse = false;
				bool mCtxComposeur = false;
				uint32 mCtxBloc = 0u;
				float32 mCtxX = 0.f, mCtxY = 0.f;
				uint32 mCollees = 0u;

				/// (Q9) L'OCTET DU COMPOSEUR sous (px, py) -- coordonnees du panneau --,
				/// lu sur les lignes PUBLIEES du composeur, mesure avec la police qui
				/// les peint. Au-dessus : debut ; en dessous : fin.
				int32 OctetComposeurSous(float32 px, float32 py) const {
					int32 len = 0;
					while (mSaisie[len])
						++len;
					int32 meilleure = -1;
					float32 dMin = 1.0e30f;
					for (uint32 i = 0; i < planChrome.Pieces(); ++i) {
						const NkAiRectPublie &q = planChrome.Piece(i);
						if (q.piece != NkAiPiece::ComposeurTexte || q.source != NkAiSource::Saisie)
							continue;
						const float32 cy = q.y + q.h * 0.5f;
						const float32 d = py > cy ? py - cy : cy - py;
						if (d < dMin) {
							dMin = d;
							meilleure = (int32)i;
						}
					}
					if (meilleure < 0)
						return len;
					const NkAiRectPublie &q = planChrome.Piece((uint32)meilleure);
					const char *a = mSaisie + q.debut, *b = mSaisie + q.debut + q.longueur;
					if (py < q.y - q.h && meilleure >= 0 && q.debut == 0u)
						return 0;
					const float32 cible = px - q.x;
					if (cible <= 0.f || !mMesureur)
						return (int32)q.debut;
					const char *c = a;
					float32 avant = 0.f;
					while (c < b) {
						const char *n = c + aidetail::LongueurCp((unsigned char)*c);
						if (n > b)
							n = b;
						const float32 w = mMesureur->LargeurPolice(a, n, 0u);
						if (w >= cible)
							return (int32)((cible - avant < w - cible) ? c - mSaisie : n - mSaisie);
						avant = w;
						c = n;
					}
					return (int32)(b - mSaisie);
				}
				void SelectionnerMot(int32 pos) {
					int32 len = 0;
					while (mSaisie[len])
						++len;
					auto lettre = [&](int32 i) {
						const char ch = mSaisie[i];
						return ch != ' ' && ch != '\n' && ch != '\t' && ch != 0;
					};
					int32 a = pos, b = pos;
					while (a > 0 && lettre(a - 1))
						--a;
					while (b < len && lettre(b))
						++b;
					mSelComp = a;
					mCaret = b;
				}
				/// (Q9) UN BITMAP COPIE devient une piece jointe : l'hote rend les
				/// pixels (Win32 CF_DIB), le kit les ecrit en PNG avec NOTRE codec et
				/// les joint par la MEME porte qu'un fichier.
				bool CollerImage(nkgui::NkGuiContext &ctx) {
					NkVector<uint8> rgba;
					int32 w = 0, h = 0;
					NkString motif;
					if (!ctx.GetClipboardImage(rgba, w, h, motif) || w <= 0 || h <= 0)
						return false;
					NkImage img;
					if (!img.Create((uint32)w, (uint32)h, math::NkColor(), 4) || !img.Pixels())
						return false;
					for (int32 y = 0; y < h; ++y)
						for (int32 x = 0; x < w; ++x) {
							uint8 *d = img.Pixels() + y * img.Stride() + x * 4;
							const uint8 *s = rgba.Data() + ((usize)y * (usize)w + (usize)x) * 4u;
							d[0] = s[0];
							d[1] = s[1];
							d[2] = s[2];
							d[3] = s[3];
						}
					const char *tmp = std::getenv("TEMP");
					char chemin[512];
					snprintf(chemin, sizeof(chemin), "%s/nkai_presse_papiers_%u_%u.png", (tmp && *tmp) ? tmp : ".",
							 (unsigned)(uintptr_t)this % 100000u, (unsigned)++mCollees);
					if (!img.SavePNG(chemin))
						return false;
					NkString pq;
					return JoindreImage(chemin, pq);
				}
				NkVector<NkString> mJointes, mImagesAAttacher;
				NkVector<const char *> mJointesPtr;
				NkString mTexteAAttacher;
				uint32 mAttacheAttente = 0u;
				static bool EstCheminImage(const char *c) {
					const char *ext[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".webp", ".PNG", ".JPG", ".JPEG"};
					usize n = 0;
					while (c[n])
						++n;
					for (usize e = 0; e < sizeof(ext) / sizeof(ext[0]); ++e) {
						usize m = 0;
						while (ext[e][m])
							++m;
						if (n > m) {
							bool ok = true;
							for (usize k = 0; k < m && ok; ++k)
								ok = c[n - m + k] == ext[e][k];
							if (ok)
								return NkFile::Exists(c);
						}
					}
					return false;
				}
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
