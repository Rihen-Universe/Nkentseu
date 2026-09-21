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
				NkString detail;
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
				/// Le panneau a PRIS un clic de la souris cette image : l'hote ne doit
				/// pas le relayer a ce qui est dessous.
				bool clicPris = false;
		};

		enum class NkAiMenu : uint8 { Aucun = 0, Fournisseurs, Modeles, Modes, Commandes, Historique, AjouterIa };

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
						modele = fa->nom;
						if (fa->modele >= 0 && fa->modele < (int32)fa->modeles.Size()) {
							modele.Append(" · ");
							modele.Append(fa->modeles[(usize)fa->modele].nom.CStr());
							modeleDetail = fa->modeles[(usize)fa->modele].detail;
						}
					}
					const NkAiModeDesc *md = (mode >= 0 && mode < (int32)modes.Size()) ? &modes[(usize)mode] : nullptr;
					NkAiComposeurDecl cd;
					cd.texte = mSaisie;
					cd.invite = (md && md->invite.Length() > 0) ? md->invite.CStr() : invite.CStr();
					cd.caret = focus ? mCaret : -1;
					cd.portePlus = portePlus;
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

					// ── 3. LES MENUS, PUBLIES PAR-DESSUS ──
					PublierMenu(m, r, yComposeur, hEntete, p);

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
				void PublierMenu(const NkAiMetriques &m, const NkPaintRect &r, float32 yComposeur, float32 hEntete,
								 NkComponentPaint &p) {
					mMenuTextes.Clear();
					mMenuDetails.Clear();
					mMenuActif.Clear();
					mMenuEteint.Clear();
					mMenuPtr.Clear();
					mMenuDetPtr.Clear();
					if (mMenu == NkAiMenu::Aucun)
						return;
					auto ligne = [&](const char *t, const char *d, bool actif, bool eteint) {
						mMenuTextes.PushBack(NkString(t ? t : ""));
						mMenuDetails.PushBack(NkString(d ? d : ""));
						mMenuActif.PushBack(actif ? 1u : 0u);
						mMenuEteint.PushBack(eteint ? 1u : 0u);
					};
					switch (mMenu) {
						case NkAiMenu::Fournisseurs:
							for (usize i = 0; i < fournisseurs.Size(); ++i) {
								const NkAiFournisseurDesc &f = fournisseurs[i];
								NkString d;
								if (!f.pret)
									d = f.motif.Length() > 0 ? f.motif : NkString("indisponible");
								else
									d = NkString(f.distant ? "distant" : "local");
								if ((int32)i == mRefusFournisseur && mRefus.Length() > 0)
									d = mRefus;
								ligne(f.nom.CStr(), d.CStr(), (int32)i == mActif, false);
							}
							ligne("Ajouter une IA…", "declarer, sans cle", false, false);
							break;
						case NkAiMenu::Modeles: {
							const int32 fi = mMenuFournisseur;
							if (fi >= 0 && fi < (int32)fournisseurs.Size()) {
								const NkAiFournisseurDesc &f = fournisseurs[(usize)fi];
								NkString t("‹  ");
								t.Append(f.nom.CStr());
								ligne(t.CStr(), "", false, false);
								for (usize k = 0; k < f.modeles.Size(); ++k)
									ligne(f.modeles[k].nom.CStr(), f.modeles[k].detail.CStr(),
										  fi == mActif && (int32)k == f.modele, false);
							}
							break;
						}
						case NkAiMenu::Modes:
							for (usize i = 0; i < modes.Size(); ++i)
								ligne(modes[i].nom.CStr(), modes[i].detail.CStr(), (int32)i == mode, false);
							break;
						case NkAiMenu::Commandes:
							for (usize i = 0; i < commandes.Size(); ++i)
								ligne(commandes[i].nom.CStr(), commandes[i].detail.CStr(), false, false);
							break;
						case NkAiMenu::Historique: {
							const NkAiConversation &c = mConv[(usize)mActif];
							for (usize i = c.archives.Size(); i > 0; --i) {
								char d[32];
								snprintf(d, sizeof(d), "%u bloc(s)", (unsigned)c.archives[i - 1].fil.Taille());
								ligne(c.archives[i - 1].sujet.CStr(), d, false, false);
							}
							if (c.archives.Size() == 0)
								ligne("Aucune conversation archivee", "", false, true);
							else
								ligne("Vider l'historique", "", false, false);
							ligne("Les conversations ne survivent pas a la fermeture.", "", false, true);
							break;
						}
						case NkAiMenu::AjouterIa:
							ligne("Ajouter une IA = la DECLARER.", "", false, true);
							ligne("Aucune cle n'est saisie ici : le panneau lit", "", false, true);
							ligne("une variable d'environnement, ou le CLI", "", false, true);
							ligne("s'authentifie lui-meme.", "", false, true);
							ligne(declaration.Length() > 0 ? declaration.CStr() : "(ou declarer : non precise par l'hote)",
								  "", false, true);
							break;
						default: break;
					}
					for (usize i = 0; i < mMenuTextes.Size(); ++i) {
						mMenuPtr.PushBack(mMenuTextes[i].CStr());
						mMenuDetPtr.PushBack(mMenuDetails[i].CStr());
					}
					const uint32 n = (uint32)mMenuTextes.Size();
					if (n == 0)
						return;
					// LA LARGEUR : ce qui s'y lit, bornee au panneau.
					float32 w = 220.f * echelle;
					for (uint32 i = 0; i < n; ++i) {
						const float32 wt = Mesure(&p, NkAiPolice::Normale, mMenuPtr[i]) +
										   Mesure(&p, NkAiPolice::Normale, mMenuDetPtr[i]) + 60.f * echelle;
						if (wt > w)
							w = wt;
					}
					const float32 wMax = r.w - m.margeComposeur * 2.f;
					if (w > wMax)
						w = wMax;
					const float32 h = (float32)n * m.menuLigne + 8.f;
					float32 x = m.margeComposeur, y = yComposeur - 4.f - h;
					if (mMenu == NkAiMenu::Historique) {
						x = r.w - w - 12.f;
						y = hEntete + 4.f;
					} else {
						// sous la pastille qui l'a ouvert, sans sortir du panneau
						NkAiRectPublie a;
						const NkAiPiece ancre = (mMenu == NkAiMenu::Modes)		   ? NkAiPiece::PastilleMode
												: (mMenu == NkAiMenu::Commandes) ? NkAiPiece::BoutonCommandes
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
					for (uint32 i = 0; i < n; ++i) {
						const float32 ly = y + 4.f + (float32)i * m.menuLigne;
						NkAiRectPublie l;
						l.piece = NkAiPiece::MenuLigne;
						l.role = NkRole::Text;
						l.x = x + 4.f;
						l.y = ly;
						l.w = w - 8.f;
						l.h = m.menuLigne;
						l.debut = i;
						l.drapeaux = (uint8)((mMenuActif[i] ? kAiActif : 0u) | (mMenuEteint[i] ? kAiEteint : 0u));
						planChrome.Ajouter(l);
						const float32 wd = mMenuDetPtr[i][0] ? Mesure(&p, NkAiPolice::Normale, mMenuDetPtr[i]) : 0.f;
						const float32 wtexte = w - 8.f - 20.f - (wd > 0.f ? wd + 12.f : 0.f) - 22.f;
						NkAiRectPublie t;
						t.piece = NkAiPiece::ChromeTexte;
						t.source = NkAiSource::MenuTexte;
						t.role = mMenuEteint[i] ? NkRole::TextMuted : NkRole::Text;
						t.x = x + 14.f;
						t.y = ly + (m.menuLigne - m.ligne) * 0.5f;
						t.w = wtexte > 10.f ? wtexte : 10.f;
						t.h = m.ligne;
						t.debut = i;
						planChrome.Ajouter(t);
						if (wd > 0.f) {
							NkAiRectPublie dd = t;
							dd.source = NkAiSource::MenuDetail;
							dd.role = NkRole::TextMuted;
							dd.x = x + w - 8.f - 22.f - wd;
							dd.w = wd;
							planChrome.Ajouter(dd);
						}
					}
				}

				/// La piece interactive du chrome sous (px, py) -- coordonnees du
				/// panneau -- ou -1. Les menus d'abord : ils sont par-dessus.
				int32 PieceSous(float32 px, float32 py) const {
					for (uint32 i = planChrome.Pieces(); i > 0; --i) {
						const NkAiRectPublie &q = planChrome.Piece(i - 1);
						switch (q.piece) {
							case NkAiPiece::MenuLigne:
							case NkAiPiece::MenuFond:
							case NkAiPiece::IconeHistorique:
							case NkAiPiece::IconeNouvelle:
							case NkAiPiece::BoutonPlus:
							case NkAiPiece::BoutonCommandes:
							case NkAiPiece::PastilleModele:
							case NkAiPiece::PastilleMode:
							case NkAiPiece::PastilleLieu:
							case NkAiPiece::Envoi:
							case NkAiPiece::ComposeurCadre: {
								// les icones se cliquent un peu au-dela de leur trait
								const float32 marge = (q.w < 30.f) ? 4.f : 0.f;
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
						const bool surMenu = k >= 0 && (planChrome.Piece((uint32)k).piece == NkAiPiece::MenuLigne ||
														planChrome.Piece((uint32)k).piece == NkAiPiece::MenuFond);
						if (!surMenu) {
							mMenu = NkAiMenu::Aucun;
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
								ChoisirLigne((int32)q.debut, out);
								return;
							case NkAiPiece::MenuFond:
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
								out.plus = true;
								return;
							case NkAiPiece::BoutonCommandes:
								if (commandesParHote)
									out.commandes = true;
								else
									mMenu = NkAiMenu::Commandes;
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

				void ChoisirLigne(int32 i, NkAiSorties &out) {
					switch (mMenu) {
						case NkAiMenu::Fournisseurs: {
							if (i == (int32)fournisseurs.Size()) {
								mMenu = NkAiMenu::AjouterIa;
								out.ajouterIa = true;
								return;
							}
							if (i < 0 || i >= (int32)fournisseurs.Size())
								return;
							// UN FOURNISSEUR A PLUSIEURS MODELES OUVRE SON SECOND NIVEAU ;
							// un seul : on le choisit tout de suite.
							if (fournisseurs[(usize)i].modeles.Size() > 1u) {
								mMenuFournisseur = i;
								mMenu = NkAiMenu::Modeles;
								return;
							}
							Basculer(i, out);
							return;
						}
						case NkAiMenu::Modeles: {
							if (i == 0) {
								mMenu = NkAiMenu::Fournisseurs;
								return;
							}
							const int32 fi = mMenuFournisseur;
							if (fi < 0 || fi >= (int32)fournisseurs.Size())
								return;
							if (!Basculer(fi, out))
								return;
							NkAiFournisseurDesc &f = fournisseurs[(usize)fi];
							if (i - 1 >= 0 && i - 1 < (int32)f.modeles.Size() && f.modele != i - 1) {
								f.modele = i - 1;
								out.modeleChange = true;
							}
							mMenu = NkAiMenu::Aucun;
							return;
						}
						case NkAiMenu::Modes:
							if (i >= 0 && i < (int32)modes.Size() && i != mode) {
								mode = i;
								out.modeChange = true;
							}
							mMenu = NkAiMenu::Aucun;
							return;
						case NkAiMenu::Commandes:
							if (i >= 0 && i < (int32)commandes.Size()) {
								const NkString &s = commandes[(usize)i].insertion;
								int32 len = 0;
								while (mSaisie[len])
									++len;
								for (usize k = 0; k < s.Length() && len + 2 < (int32)sizeof(mSaisie); ++k)
									mSaisie[len++] = s.CStr()[k];
								mSaisie[len] = 0;
								mCaret = len;
							}
							mMenu = NkAiMenu::Aucun;
							return;
						case NkAiMenu::Historique: {
							NkAiConversation &c = mConv[(usize)mActif];
							const int32 n = (int32)c.archives.Size();
							if (i >= 0 && i < n) {
								if (!occupe)
									Rouvrir(n - 1 - i); // la liste est la plus recente en tete
							} else if (i == n && n > 0)
								c.archives.Clear(); // « Vider l'historique » : ATTEIGNABLE
							mMenu = NkAiMenu::Aucun;
							return;
						}
						default:
							mMenu = NkAiMenu::Aucun;
							return;
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
				NkVector<NkString> mMenuTextes, mMenuDetails;
				NkVector<uint8> mMenuActif, mMenuEteint;
				NkVector<const char *> mMenuPtr, mMenuDetPtr;
				uint32 mEpingle = 0u;

			public:
				/// La demande EPINGLEE en tete du fil cette image (0 = aucune).
				uint32 Epingle() const {
					return mEpingle;
				}
		};

	} // namespace editorkit
} // namespace nkentseu
