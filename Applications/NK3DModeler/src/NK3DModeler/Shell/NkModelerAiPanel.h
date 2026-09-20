#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Shell/NkModelerAiPanel.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  (b9) L'ASSISTANT — LA FORME TRANCHEE PAR RODOLF LE 17/09
// =============================================================================
//  « que ce soit pour nkcode, nk3dmodeler ou nkuidesign ou les futures, ils
//    doivent ressembler a celui de VSCode, sauf si tu as mieux a proposer »
//  La forme est ecrite dans `echanges/PANNEAU_IA_SPEC.md` : panneau ancre a
//  droite sur toute la hauteur, une barre d'onglets PAR FOURNISSEUR, une ligne
//  de titre, un fil de BLOCS TYPES replies par defaut, un composeur en bas, et
//  sous lui une ligne d'etat.
//
//  ⚠️ CE FICHIER EST REECRIT, ET LA RAISON EST UN DEFAUT, PAS UN GOUT.
//     La premiere version (17/09, 18h09) peignait le panneau DANS la pastille
//     de proprietes -- c'est-a-dire a l'interieur d'un panneau hote. C'est
//     exactement le piege que la specification interdit : deux menus de
//     NKUIDesign dessines ainsi laissaient passer les clics une image sur deux.
//     Le panneau vit desormais sur la COUCHE OVERLAY (`NkOvPainter()`), sur sa
//     propre couche de registre.
//
//  CE QUI N'A PAS CHANGE, ET QUI EST L'ESSENTIEL :
//    - `NkAiSoumettre` reste LE POINT D'ENTREE UNIQUE. Le bouton l'appelle, et
//      le crochet de mesure `NK_AI_DEMANDE` entre exactement la.
//    - Le panneau N'APPELLE PAS LE PONT. Il ecrit dans `st.aiPending` ; la
//      boucle execute. Un panneau qui appellerait `NkVpPoserAction` lui-meme
//      serait un second chemin vers le meme etat.
//
//  CE QUE LA FORME AJOUTE, ET QUI NE VIENT PAS DE LA CAPTURE :
//    1. CHAQUE OPERATION PORTE SON EFFET **MESURE** ET SON ANNULATION. La ligne
//       dit « faces 6 -> 384 », lu dans les compteurs de l'hote avant et apres.
//    2. LE REFUS EST UN BLOC A PART, avec son motif. C'est une reponse, pas une
//       panne : une IA invente des verbes, c'est le cas normal.
//    3. LA LIGNE D'ETAT DIT LE DORSAL ET SON COUT -- et elle dit **non charge**
//       tant qu'il ne l'est pas. Afficher « 4 444 Mo » comme si le modele
//       occupait la carte serait faux aujourd'hui, et ce genre de chiffre finit
//       par etre cru.
//
//  CE QUE CE PANNEAU EST DEPUIS LE 19/09 : une phrase francaise y entre, un
//  verbe du contrat en sort. `NkModelerIA.h` parle a NKConverse en asynchrone
//  et main.cpp recolte a chaque image. Mesure : « subdivise le cube deux fois »
//  -> `subdivide:2`, 0,91 s a chaud et 10,35 s A FROID -- avec 1478 images
//  passees pendant l attente, donc sans gel.
//
//  ⚠️ CE PARAGRAPHE DISAIT « rien n est branche » JUSQU AU 20/09, ET C ETAIT
//     FAUX DEPUIS LA VEILLE. Un commentaire perime ne vieillit pas comme un
//     chiffre : il se lit comme une CONSIGNE, et le lecteur suivant renonce a
//     chercher ce qui existe. Il avait deja failli faire reecrire ce branchement.
//
//  ⚠️ CE QUE LE PANNEAU N EST TOUJOURS PAS : une CONVERSATION. Un tour, une
//     demande, un verbe -- aucune memoire d un echange a l autre. Le fil ne
//     porte donc AUCUN bloc de « reflexion » : en fabriquer un serait du
//     theatre, personne ne pense derriere.
//
//  DEUX DETTES NOMMEES, AVEC LEUR CONDITION DE RETRAIT :
//    - La specification demande l'entree et la sortie **a chasse fixe**. Ce
//      peintre n'a qu'une police. Le bloc est distingue par son FOND. A
//      corriger le jour ou le shell charge une seconde police.
//    - La specification demande que le panneau soit monte depuis un document
//      `.nkgui`. Le mecanisme de zone hote vient d'arriver par transit ; le
//      panneau doit d'abord exister et se mesurer. Le jour venu, seule
//      l'ORIGINE des rectangles change, pas ce qu'ils montrent.
// -----------------------------------------------------------------------------

#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerCommon.h"
#include "NKEditorKit/NkEditorKit.h"

namespace nkentseu {
	namespace nk3d {

		// Copie bornee, sans dependre de strncpy : le depot ecrit ses propres
		// primitives de chaine (zero-STL), et une troncature doit etre VOULUE.
		inline void NkAiCopie(char *dst, uint32 cap, const char *src) {
			if (!dst || cap == 0u)
				return;
			uint32 i = 0;
			if (src)
				for (; src[i] && i + 1u < cap; ++i)
					dst[i] = src[i];
			dst[i] = 0;
		}

		inline bool NkAiVide(const char *s) {
			if (!s)
				return true;
			for (uint32 i = 0; s[i]; ++i)
				if (s[i] != ' ' && s[i] != '\t')
					return false; // au moins un caractere qui n'est pas du blanc
			return true;
		}

		// ── LES FOURNISSEURS ───────────────────────────────────────────────────
		// LE LOCAL EST LE PREMIER ET LE DEFAUT. Les deux autres existent dans la
		// barre parce qu'ils existent dans la maison (PV3DE parle a Claude et a
		// Ollama), mais rien ne les branche AU MODELEUR : leur onglet le DIT et
		// eteint le composeur, au lieu d'envoyer au pont un texte en laissant
		// croire qu'un modele l'a produit.
		inline const char *NkAiFournisseur(int32 i) {
			switch (i) {
				case 0:
					return "Local";
				case 1:
					return "Claude";
				case 2:
					return "Ollama";
				default:
					return "?";
			}
		}
		static const int32 kAiFournisseurs = 3;

		// ── LE FIL : POSER UN BLOC ─────────────────────────────────────────────
		// Fenetre glissante : on perd le plus ancien, jamais le plus recent.
		inline int32 NkAiPousser(NkModelerState &st, NkModelerState::AiType t, const char *ligne) {
			if (st.aiFilN >= NkModelerState::kAiFil) {
				for (int32 i = 1; i < NkModelerState::kAiFil; ++i)
					st.aiFil[i - 1] = st.aiFil[i];
				st.aiFilN = NkModelerState::kAiFil - 1;
				// ⚠️ LE BLOC EN COURS DE MESURE GLISSE AVEC LE FIL. Sans ce
				//    decalage, l'effet mesure irait se poser sur le bloc du
				//    voisin : un chiffre juste, ecrit sur la mauvaise ligne.
				if (st.aiEnCours > 0)
					--st.aiEnCours;
				else if (st.aiEnCours == 0)
					st.aiEnCours = -1; // celui qu'on attendait vient d'etre jete
			}
			NkModelerState::AiBloc &b = st.aiFil[st.aiFilN];
			b = NkModelerState::AiBloc{};
			b.type = (uint8)t;
			// La demande de l'utilisateur est ENCADREE EN TETE, jamais repliee :
			// c'est la question, on doit toujours la voir.
			b.replie = (t == NkModelerState::AiType::Demande) ? 0u : 1u;
			NkAiCopie(b.ligne, sizeof(b.ligne), ligne);
			return st.aiFilN++;
		}

		// ── LA SOUMISSION, ET ELLE EST LA SEULE ────────────────────────────────
		// Le bouton l'appelle, la touche Entree l'appellera, et le crochet de
		// mesure `NK_AI_DEMANDE` entre EXACTEMENT ICI. Ce n'est pas un raccourci
		// pour la sonde : c'est le meme point d'entree, sinon la sonde mesurerait
		// un chemin que Rodolf n'emprunte jamais.
		inline bool NkAiSoumettre(NkModelerState &st, const char *texte) {
			// ── PAS D'ASSISTANT AU LANCEUR (Rodolf, 17/09) ─────────────────────
			// « le chat s'ouvre seulement quand on est EN PROJET, et non depuis le
			// lanceur ». La raison tient en une phrase : l'IA AGIT SUR QUELQUE
			// CHOSE. Sur l'ecran d'accueil il n'y a pas de maillage, donc aucune
			// de ses actions n'a de cible, et la moitie utile du panneau -- l'effet
			// mesure, l'annulation -- n'a plus de sens.
			//
			// ⚠️ ON REFUSE EN LE DISANT, on n'ouvre pas une coquille vide. Un
			//    panneau present mais inerte apprend a l'utilisateur que cette
			//    pastille ne sert a rien, et il ne l'ouvrira plus le jour ou elle
			//    servira.
			if (st.welcome) {
				NkAiCopie(st.aiMotif, sizeof(st.aiMotif),
						  "Aucun projet ouvert : l'assistant agit sur un maillage.");
				st.aiMotifEstRefus = true;
				st.aiOuvert = false; // et surtout : il ne s'ouvre pas
				return false;
			}
			if (NkAiVide(texte)) {
				// LE ZERO. On ne soumet rien, et on DIT pourquoi : un bouton qui
				// ne reagit pas se lit comme un bouton casse.
				NkAiCopie(st.aiMotif, sizeof(st.aiMotif),
						  "Rien a faire : la demande est vide.");
				st.aiMotifEstRefus = true;
				return false;
			}
			NkAiCopie(st.aiPending, sizeof(st.aiPending), texte);
			// LE SUJET EST LA PREMIERE DEMANDE, comme dans la capture.
			if (!st.aiSujet[0])
				NkAiCopie(st.aiSujet, sizeof(st.aiSujet), texte);
			(void)NkAiPousser(st, NkModelerState::AiType::Demande, texte);
			// ── UNE DEMANDE **MARQUE** LE PANNEAU, ELLE NE L'OUVRE PLUS ───────
			// L'ancienne version posait ici `st.aiOuvert = true`, pour une raison qui
			// reste vraie : une demande soumise dans un panneau ferme repondrait sans
			// que personne ne voie la reponse.
			// 
			// Ce que cette raison ne justifiait pas, c'est de PRENDRE L'ECRAN. Rodolf
			// a demande que l'assistant soit « une pastille comme les autres » -- et
			// une pastille ne s'ouvre pas toute seule. *Ouvrir de force repond au
			// besoin de l'APPLICATION ; marquer repond a celui de l'UTILISATEUR.*
			// 
			// `NkAiPousser` vient d'incrementer `aiFilN` : la marque EXISTE DEJA, elle
			// est `aiFilN > aiFilVu`, et la pastille la peint. Il n'y a donc rien a
			// poser ici, et surtout pas un second etat qui dirait la meme chose.
			// 
			// ⚠ ET LE BANC N'Y PERD RIEN, VERIFIE EN LISANT LES DEUX CROCHETS :
			//   `NK_AI_DEMANDE` ne lit pas `aiOuvert` -- il lit le retour de cette
			//   fonction et `st.aiMotif` ; `NK_AI_TRACE` imprime desormais la MARQUE
			//   meme panneau ferme, et `NK_AI_PANNEAU` ouvre pour qui veut mesurer
			//   les rectangles. C'est ce qui autorise a ne garder QU'UN comportement,
			//   au lieu d'un pour l'humain et un pour l'instrument.
			return true;
		}

		// ── LE TOUR, UNE FOIS QUE LA TABLE A REPONDU ───────────────────────────
		// Appelee par la boucle APRES `NkVpPoserAction` : c'est la table qui sait
		// si le verbe existe, et `aiMotifEstRefus` porte deja sa reponse. Les
		// compteurs d'AVANT sont passes par la boucle, seule a voir l'hote -- le
		// panneau ne connait pas le maillage et ne doit pas l'apprendre.
		inline void NkAiTour(NkModelerState &st, const char *demande, int32 v0, int32 e0, int32 f0,
							 int32 frame) {
			if (st.aiMotifEstRefus) {
				const int32 i = NkAiPousser(st, NkModelerState::AiType::Refus, "Demande refusee");
				NkAiCopie(st.aiFil[i].detail, sizeof(st.aiFil[i].detail), st.aiMotif);
				NkAiCopie(st.aiFil[i].in, sizeof(st.aiFil[i].in), demande);
				st.aiEnCours = -1; // rien a mesurer : rien n'a ete pose
				return;
			}
			const int32 i = NkAiPousser(st, NkModelerState::AiType::Operation, demande);
			NkModelerState::AiBloc &b = st.aiFil[i];
			NkAiCopie(b.in, sizeof(b.in), demande);
			b.vA = v0;
			b.eA = e0;
			b.fA = f0;
			b.mesure = 0;
			st.aiEnCours = i;
			st.aiEnCoursFrame = frame;
		}

		// ── L'EFFET, LU DANS LES COMPTEURS ET NULLE PART AILLEURS ──────────────
		inline void NkAiEffet(NkModelerState &st, int32 v1, int32 e1, int32 f1) {
			if (st.aiEnCours < 0 || st.aiEnCours >= st.aiFilN)
				return;
			NkModelerState::AiBloc &b = st.aiFil[st.aiEnCours];
			b.vB = v1;
			b.eB = e1;
			b.fB = f1;
			const bool bouge = (v1 != b.vA) || (e1 != b.eA) || (f1 != b.fA);
			b.mesure = bouge ? 1u : 2u;
			if (bouge)
				snprintf(b.out, sizeof(b.out), "sommets %d -> %d   aretes %d -> %d   faces %d -> %d",
						 b.vA, v1, b.eA, e1, b.fA, f1);
			else
				// ⚠️ « RIEN N'A CHANGE » N'EST PAS UN ECHEC, et il ne faut pas
				//    l'ecrire comme tel : changer de sous-mode, cadrer la vue ou
				//    basculer le rayon X ne touche aucun compteur. Le dire est plus
				//    honnete que d'afficher « 6 -> 6 » comme un resultat.
				snprintf(b.out, sizeof(b.out), "les comptes n'ont pas bouge (%d/%d/%d)", b.vA, b.eA,
						 b.fA);
			// -- L EFFET MESURE, LISIBLE PAR UN BANC (NK_AI_TRACE) --
			// Il existait deja, mais seulement DANS le bloc du fil : pour le lire il
			// fallait ouvrir le panneau et regarder. Un verdict qui n existe que la ou
			// personne ne le cherche ne sert a personne.
			//
			// [!] CETTE LIGNE N A JAMAIS TEMOIGNE, ET JE L ECRIS PLUTOT QUE DE LA
			//     LIVRER COMME ACQUISE. Son appelant (main.cpp, « l effet de la demande
			//     precedente ») ne l atteint que si `Demo3DHostStats` rend true, et cette
			//     fonction n est renseignee QU EN MODE EDITION. Quatre essais avec
			//     `NK_EDIT_MODE=1` ont donne « AI RESULTAT : acceptee » et un fil a deux
			//     blocs -- donc `aiEnCours` etait bien pose -- sans jamais une seule ligne
			//     AI EFFET : le mode edition n etait pas actif.
			//     Reste a etablir : POURQUOI le crochet ne met-il pas le mode ? Tant que
			//     ce n est pas fait, ne pas lire l ABSENCE de cette ligne comme
			//     « l operation n a rien change » -- c est une sonde muette, pas un zero.
			static const bool sEffetOn = (std::getenv("NK_AI_TRACE") != nullptr);
			if (sEffetOn) {
				std::printf("[nk3d] AI EFFET : %s -> %s\n", b.in, b.out);
				std::fflush(stdout);
			}
			st.aiEnCours = -1;
		}

		/// L'index du DERNIER bloc d'operation du fil, ou -1.
		/// ⚠️ SEUL CELUI-LA PEUT PORTER « Annuler cette action ». Notre pile
		///    d'annulation a UN CRAN : un bouton actif sur une ligne ancienne
		///    annulerait LA DERNIERE en affichant le texte D'UNE AUTRE, et
		///    l'utilisateur verrait le mauvais effet disparaitre.
		inline int32 NkAiDerniereOperation(const NkModelerState &st) {
			for (int32 i = st.aiFilN - 1; i >= 0; --i)
				if (st.aiFil[i].type == (uint8)NkModelerState::AiType::Operation)
					return i;
			return -1;
		}

		// ── LE PANNEAU ─────────────────────────────────────────────────────────
		// `r` est le rectangle COMPLET du panneau (ancre a droite, pleine hauteur).
		// `peutAnnuler` vient de l'hote (`Demo3DHostEditCanUndo`) : le panneau ne
		// connait pas le maillage.
		inline void PaintAiOverlay(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								   nkgui::NkGuiContext *guiCtx, const NkRect &r, bool peutAnnuler) {
			if (!st.aiOuvert)
				return;
			// ── VU, PARCE QU'IL EST PEINT ──────────────────────────────
			// La marque se consomme ICI et nulle part ailleurs : au moment ou les blocs
			// passent sous les yeux. La poser au CLIC de la pastille aurait marque
			// « vu » un panneau que ce meme clic venait peut-etre de FERMER.
			st.aiFilVu = st.aiFilN;
			const float32 kRowH = S(22.f);
			const float32 pad = S(8.f);
			// L'EMPRISE, DECLAREE D'ABORD. Sans elle, un clic dans le vide du
			// panneau traverserait jusqu'au viseur et deselectionnerait -- le
			// defaut « les clics traversent » deja paye sur les surcouches de la vue.
			(void)hit.Add("ai.box", r);
			// Le panneau PUBLIE le rectangle qu il vient de reclamer. Le temoin le
			// compare a celui de la pastille : deux mesures, deux peintres, aucune
			// formule recopiee dans la sonde.
			st.aiPanRect[0] = r.x; st.aiPanRect[1] = r.y;
			st.aiPanRect[2] = r.w; st.aiPanRect[3] = r.h;
			p.Fill(r, NkRole::PanelBg, 0.f);
			p.Fill({r.x, r.y, S(1.f), r.h}, NkRole::Border, 0.f);

			float32 yy = r.y;

			// ── 1. LA BARRE D'ONGLETS DE FOURNISSEUR ───────────────────────────
			{
				const float32 tw = (r.w - pad * 2.f) / (float32)kAiFournisseurs;
				for (int32 i = 0; i < kAiFournisseurs; ++i) {
					const NkRect tr{r.x + pad + tw * (float32)i, yy + S(4.f), tw, kRowH};
					char cle[24];
					snprintf(cle, sizeof(cle), "ai.tab%d", (int)i);
					const bool over = hit.Add(cle, tr);
					const bool actif = (st.aiOnglet == i);
					p.Fill(tr,
						   actif ? NkRole::PanelHeader : (over ? NkRole::InputBg : NkRole::PanelBg),
						   3.f);
					p.TextV(tr.x + (tw - p.TextW(NkAiFournisseur(i))) * 0.5f, tr.y, kRowH,
							NkAiFournisseur(i), actif ? NkRole::Text : NkRole::TextMuted);
					if (actif)
						p.Fill({tr.x, tr.y + kRowH - S(2.f), tw, S(2.f)}, NkRole::AccentUi, 0.f);
					if (hit.Clicked(cle))
						st.aiOnglet = i;
				}
				yy += kRowH + S(8.f);
			}

			// ── 2. LA LIGNE DE TITRE : sujet tronque, et deux boutons discrets ──
			{
				const float32 bw = S(22.f);
				const NkRect nouv{r.x + r.w - pad - bw, yy, bw, kRowH};
				const NkRect hist{nouv.x - bw - S(4.f), yy, bw, kRowH};
				p.TextClipped(r.x + pad, yy + S(4.f), hist.x - r.x - pad * 2.f,
							  st.aiSujet[0] ? st.aiSujet : "Nouvelle conversation", NkRole::Text);
				const bool ovH = hit.Add("ai.hist", hist);
				const bool ovN = hit.Add("ai.nouv", nouv);
				p.IconV(hist.x + S(4.f), yy, kRowH, NkIcon::Journal,
						ovH ? NkRole::Text : NkRole::TextMuted, 12.f);
				p.IconV(nouv.x + S(4.f), yy, kRowH, NkIcon::PlusCircle,
						ovN ? NkRole::Text : NkRole::TextMuted, 12.f);
				if (hit.Clicked("ai.nouv")) {
					// NOUVELLE CONVERSATION : le fil se vide, le sujet aussi.
					// ⚠️ Rien d'autre. Elle ne touche pas au maillage : une
					//    conversation qu'on ferme ne defait pas ce qu'elle a fait.
					st.aiFilN = 0;
					st.aiEnCours = -1;
					st.aiSujet[0] = 0;
					st.aiDefile = 0.f;
				}
				yy += kRowH + S(4.f);
			}
			p.Fill({r.x + pad, yy, r.w - pad * 2.f, S(1.f)}, NkRole::Border, 0.f);
			yy += S(6.f);

			// ── 3. LE COMPOSEUR ET SA LIGNE D'ETAT, RESERVES EN BAS ────────────
			// Reserves AVANT de peindre le fil : c'est ce qui donne au fil sa
			// hauteur exacte. L'inverse -- peindre le fil puis « ce qui reste » --
			// laisse le composeur sortir du panneau des que le fil est long.
			const float32 hComposeur = kRowH * 2.f + S(6.f);
			const float32 hEtat = kRowH;
			const float32 yComposeur = r.y + r.h - pad - hEtat - S(6.f) - hComposeur;
			const NkRect filR{r.x + pad, yy, r.w - pad * 2.f, yComposeur - yy - S(6.f)};

			// ── 4. LE FIL DE BLOCS TYPES ───────────────────────────────────────
			p.Clip(filR);
			float32 fy = filR.y - st.aiDefile;
			for (int32 i = 0; i < st.aiFilN; ++i) {
				NkModelerState::AiBloc &b = st.aiFil[i];
				const NkModelerState::AiType t = (NkModelerState::AiType)b.type;
				char cle[24];
				snprintf(cle, sizeof(cle), "ai.b%d", (int)i);

				if (t == NkModelerState::AiType::Demande) {
					// LA DEMANDE DE L'UTILISATEUR, DANS UN CADRE. C'est la question.
					const float32 h = kRowH + S(6.f);
					p.Outline({filR.x, fy, filR.w, h}, NkRole::Border, NkRole::InputBg, 3.f);
					p.TextClipped(filR.x + S(6.f), fy + S(7.f), filR.w - S(12.f), b.ligne,
								  NkRole::Text);
					fy += h + S(4.f);
					continue;
				}

				// LES AUTRES BLOCS : UNE LIGNE, REPLIEE PAR DEFAUT.
				// Le rectangle est MEMORISE : c'est lui que la sonde cliquera.
				b.rl[0] = filR.x;
				b.rl[1] = fy;
				b.rl[2] = filR.w;
				b.rl[3] = kRowH;
				const bool over = hit.Add(cle, {filR.x, fy, filR.w, kRowH});
				const bool refus = (t == NkModelerState::AiType::Refus);
				const NkRole teinte = refus ? NkRole::AxisX : NkRole::Text;
				if (over)
					p.Fill({filR.x, fy, filR.w, kRowH}, NkRole::InputBg, 3.f);
				p.IconV(filR.x + S(2.f), fy, kRowH,
						b.replie ? NkIcon::ChevronRight : NkIcon::ChevronDown, NkRole::TextMuted,
						10.f);
				// L'EFFET EST SUR LA LIGNE, pas cache dans le repli : on doit lire
				// ce qui a change sans rien ouvrir.
				char ligne[224];
				if (t == NkModelerState::AiType::Operation && b.mesure == 1u)
					snprintf(ligne, sizeof(ligne), "%s   ·   faces %d -> %d", b.ligne, b.fA, b.fB);
				else if (t == NkModelerState::AiType::Operation && b.mesure == 2u)
					snprintf(ligne, sizeof(ligne), "%s   ·   comptes inchanges", b.ligne);
				else if (t == NkModelerState::AiType::Operation)
					snprintf(ligne, sizeof(ligne), "%s   ·   effet en cours de mesure", b.ligne);
				else
					snprintf(ligne, sizeof(ligne), "%s", b.ligne);
				p.TextClipped(filR.x + S(16.f), fy + S(4.f), filR.w - S(22.f), ligne, teinte);
				if (hit.Clicked(cle))
					b.replie = b.replie ? 0u : 1u; // ⚠️ DEPLIER N'EXECUTE RIEN
				fy += kRowH;

				if (!b.replie) {
					// L'ENTREE ET LA SORTIE, dans un bloc a fond distinct.
					// ⚠️ La chasse fixe manque (une seule police dans ce peintre) :
					//    le bloc est distingue par son FOND. Dette nommee en tete.
					const float32 h = kRowH * 2.f;
					p.Fill({filR.x + S(16.f), fy, filR.w - S(16.f), h}, NkRole::InputBg, 3.f);
					char l1[160];
					snprintf(l1, sizeof(l1), "IN    %s", b.in);
					p.TextClipped(filR.x + S(22.f), fy + S(3.f), filR.w - S(28.f), l1,
								  NkRole::TextMuted);
					char l2[224];
					snprintf(l2, sizeof(l2), "OUT   %s", b.out[0] ? b.out : b.detail);
					p.TextClipped(filR.x + S(22.f), fy + S(3.f) + kRowH, filR.w - S(28.f), l2,
								  refus ? NkRole::AxisX : NkRole::TextMuted);
					fy += h + S(2.f);
					if (refus && b.detail[0]) {
						const float32 hw = p.TextWrapMeasure(filR.w - S(28.f), b.detail) + S(4.f);
						p.TextWrap(filR.x + S(22.f), fy, filR.w - S(28.f), b.detail, NkRole::AxisX);
						fy += hw;
					}
				}

				// ── « ANNULER CETTE ACTION », et SEULEMENT sur la derniere ─────
				if (t == NkModelerState::AiType::Operation && b.mesure != 0u) {
					const bool derniere = (i == NkAiDerniereOperation(st));
					const bool actif = derniere && peutAnnuler;
					const float32 bw = S(132.f);
					const NkRect ub{filR.x + filR.w - bw, fy, bw, kRowH - S(2.f)};
					char ucle[24];
					snprintf(ucle, sizeof(ucle), "ai.undo%d", (int)i);
					const bool ovU = hit.Add(ucle, ub);
					b.ru[0] = ub.x;
					b.ru[1] = ub.y;
					b.ru[2] = actif ? ub.w : 0.f; // w=0 quand il est ETEINT : la sonde
					b.ru[3] = ub.h;				  // ne doit pas cliquer un bouton inerte
					p.Fill(ub,
						   actif ? (ovU ? NkRole::AccentUi : NkRole::PanelHeader) : NkRole::InputBg,
						   3.f);
					p.TextV(ub.x + S(6.f), ub.y, ub.h, "Annuler cette action",
							actif ? NkRole::Text : NkRole::TextMuted);
					if (!actif) {
						// LE MOTIF, A COTE DU BOUTON ETEINT. Un bouton gris sans
						// raison se lit comme une panne.
						const char *pourquoi =
							!derniere ? "seule la derniere action s'annule d'un cran"
									  : "rien a annuler dans l'historique du maillage";
						p.TextClipped(filR.x + S(16.f), ub.y + S(3.f), filR.w - bw - S(24.f),
									  pourquoi, NkRole::TextMuted);
					}
					if (actif && hit.Clicked(ucle))
						// PAR LA MEME PORTE QUE TOUT LE RESTE : « undo » est un verbe
						// du pont. Le bouton n'a aucun pouvoir propre.
						(void)NkAiSoumettre(st, "undo");
					fy += kRowH + S(2.f);
				}
				fy += S(4.f);
			}
			if (st.aiFilN == 0) {
				p.TextV(filR.x, fy, kRowH, "Selectionnez, puis demandez.", NkRole::TextMuted);
				fy += kRowH;
				// ⚠️ ON DIT CE QU'ON SAIT FAIRE. Un champ libre sans exemple se
				//    repond par des phrases que rien ne comprend, et l'utilisateur
				//    conclut que l'outil ne marche pas -- alors qu'il n'a jamais su
				//    ce qu'on attendait de lui.
				p.TextV(filR.x, fy, kRowH, "Ex. : subdivide:3  ·  bevel:0.2:4  ·  undo",
						NkRole::TextMuted);
			}
			p.Unclip();

			// ── 5. LE COMPOSEUR ────────────────────────────────────────────────
			// ⚠️ CE N'EST PLUS « LOCAL OU RIEN ». Jusqu'au 20/09 le composeur
			//    n'etait allume que sur l'onglet 0, parce qu'aucun autre n'etait
			//    cable. Il l'est maintenant des que la BOUCLE dit que l'onglet a
			//    un dorsal -- `st.aiOngletPret`, ecrit par `NkIaCanal::DorsalDe`.
			//    Le panneau ne redecide pas : deux avis sur la meme question
			//    finissent toujours par diverger, et l'utilisateur croit celui
			//    qu'il voit.
			const bool ongletActif = st.aiOngletPret;
			{
				const float32 bw = S(74.f);
				const NkRect champ{r.x + pad, yComposeur, r.w - pad * 2.f - bw - S(6.f),
								   hComposeur};
				p.Outline(champ, NkRole::Border, NkRole::InputBg, 3.f);
				if (guiCtx && ongletActif) {
					editorkit::NkOverlayTextField(*guiCtx, guiCtx->dl, p.FontPtr(),
												  {champ.x, champ.y, champ.w, kRowH}, st.aiSaisie,
												  (int32)sizeof(st.aiSaisie) - 1, true);
					// ── CE QUI PART, DIT SOUS LE CHAMP, AVANT D'ENVOYER ───────
					// ⚠️ L'AVERTISSEMENT VIT SOUS LE CURSEUR, PAS DANS UNE LIGNE
					//    D'ETAT EN BAS DE PANNEAU. Rodolf doit pouvoir choisir en
					//    connaissance de cause A CHAQUE USAGE, et le regard est
					//    sur le champ qu'il remplit -- pas sur le bord inferieur.
					if (st.aiOngletDistant)
						p.TextClipped(champ.x + S(4.f), champ.y + S(4.f) + kRowH,
									  champ.w - S(8.f),
									  "⚠ SERVICE DISTANT : cette demande et le contrat des 26 "
									  "verbes QUITTENT cette machine.",
									  NkRole::AxisX);
				} else if (!ongletActif) {
					// LE ZERO, ET IL DIT LEQUEL. Le motif vient du canal, avec le
					// GESTE QUI REPARE en tete : « Installez... », « Connectez-vous... ».
					// ⚠️ Un refus muet a cote de deux onglets bavards se lit comme
					//    un silence -- et on cherche le defaut dans sa demande.
					p.TextWrap(champ.x + S(4.f), champ.y + S(4.f), champ.w - S(8.f),
							   st.aiOngletMotif[0] ? st.aiOngletMotif
												   : "Aucun dorsal pour cet onglet.",
							   NkRole::TextMuted);
				} else {
					// Repli nomme (pas de contexte) : on AFFICHE, on n'edite pas. Un
					// champ muet qui a l'air editable est pire qu'un champ eteint.
					p.TextV(champ.x + S(4.f), champ.y, kRowH, st.aiSaisie, NkRole::TextMuted);
				}
				const NkRect bt{r.x + r.w - pad - bw, yComposeur, bw, kRowH};
				const bool over = hit.Add("ai.envoyer", bt);
				const bool actif = ongletActif && !NkAiVide(st.aiSaisie);
				// Le bouton dit lui-meme s'il fera quelque chose : eteint quand le
				// champ est vide. C'est le ZERO, rendu visible avant d'etre clique.
				p.Fill(bt,
					   actif ? (over ? NkRole::AccentUi : NkRole::PanelHeader) : NkRole::InputBg,
					   3.f);
				p.TextV(bt.x + (bw - p.TextW("Envoyer")) * 0.5f, bt.y, kRowH, "Envoyer",
						actif ? NkRole::Text : NkRole::TextMuted);
				if (actif && hit.Clicked("ai.envoyer")) {
					if (NkAiSoumettre(st, st.aiSaisie))
						st.aiSaisie[0] = 0; // le champ se vide : la demande est PARTIE
				}
				// FERMER : le panneau est refermable, comme celui de la capture.
				const NkRect fb{bt.x, bt.y + kRowH + S(6.f), bw, kRowH};
				const bool ovF = hit.Add("ai.fermer", fb);
				p.Fill(fb, ovF ? NkRole::InputBg : NkRole::PanelBg, 3.f);
				p.TextV(fb.x + (bw - p.TextW("Fermer")) * 0.5f, fb.y, kRowH, "Fermer",
						NkRole::TextMuted);
				if (hit.Clicked("ai.fermer"))
					// ON RECULE UN GESTE, ON NE LE RETIRE PAS : ce bouton reste, mais
					// il n'est plus la SEULE sortie -- la pastille du bord droit en
					// est une, visible que le panneau soit ouvert ou ferme. Les deux
					// ecrivent le MEME etat : aucune des deux portes ne peut produire
					// un resultat que l'autre ne produirait pas.
					st.aiOuvert = false;
			}

			// ── 6. LA LIGNE D'ETAT : LE DORSAL ET SON COUT ─────────────────────
			// ⚠️ « NON CHARGE » EST LA PARTIE IMPORTANTE. Les 4 444 Mo sont une
			//    mesure d'en-tete du 14/09, pas une mesure de l'instant :
			//    l'afficher seul ferait croire que le modele occupe la carte
			//    maintenant. Et la carte est DISPUTEE -- l'entrainement d'Ilyana y
			//    tourne. Rodolf doit pouvoir comprendre un ralentissement sans le
			//    deviner.
			//
			// ⚠️ CETTE LIGNE N'EST PLUS COMPOSEE ICI, ET C'EST LE CORRECTIF DU
			//    20/09. Le panneau ecrivait le texte PUIS le recopiait dans
			//    `st.aiEtat` « ce qui est AFFICHE, pas une copie » -- mais il le
			//    composait a partir du SEUL `aiOnglet`, sans rien savoir du
			//    dorsal. Avec trois onglets dont deux cables, il aurait fallu lui
			//    apprendre l'etat des dorsaux : c'est-a-dire une SECONDE autorite
			//    sur la meme question. `NkIaCanal::LigneEtat` l'ecrit, la boucle
			//    la depose, le panneau la PEINT. Un seul auteur.
			{
				const NkRect er{r.x + pad, r.y + r.h - pad - hEtat, r.w - pad * 2.f, hEtat};
				// LE DISTANT SE VOIT : fond d'alerte, pas le fond d'en-tete ordinaire.
				// Un avertissement de la meme couleur que tout le reste n'avertit pas.
				p.Fill(er, st.aiOngletDistant ? NkRole::InputBg : NkRole::PanelHeader, 3.f);
				p.TextClipped(er.x + S(6.f), er.y + S(3.f), er.w - S(12.f),
							  st.aiEtat[0] ? st.aiEtat : "(etat non publie par la boucle)",
							  st.aiOngletDistant ? NkRole::AxisX : NkRole::TextMuted);
			}
		}

	} // namespace nk3d
} // namespace nkentseu
