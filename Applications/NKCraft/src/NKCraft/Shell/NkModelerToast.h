#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerToast.h
// @Brief   LE RESULTAT D'UNE ACTION, DIT A L'ECRAN -- une pile de messages
//          peinte DANS LA COUCHE OVERLAY, donc au-dessus de tout, avec sa
//          severite en couleur ET en mot, et une croix pour la retirer.
//
//          🔴 POURQUOI CE FICHIER EXISTE. Le 2026-09-05, Rodolf a rapporte pour
//          la seconde fois « l'import de XBot a refuse ». Son propre journal de
//          23h48 disait la raison en clair :
//
//            [WRN] [import] Importer REFUSE : ouvrez une SCENE (l'import cree
//                  des models, et un model n'en contient pas)
//
//          Ce refus etait JUSTE. Il etait aussi ecrit dans `st.hierNote` -- la
//          derniere ligne du panneau Hierarchie, en petit, sous la liste, en
//          concurrence avec le decompte des objets, coupee a 96 caracteres --
//          et dans un journal que personne n'ouvre en travaillant. Une
//          correction precedente avait ajoute le JOURNAL ; elle n'avait pas
//          ajoute l'ECRAN.
//
//          La regle du depot le disait deja : « un refus qu'on peut ne pas
//          regarder se confond avec un bouton casse ». Une note qu'on peut ne
//          pas regarder EST un refus qu'on peut ne pas regarder.
//
//          OU CE COMPOSANT DEVRAIT VIVRE : dans NKEditorKit, avec les modales et
//          les infobulles -- toute application de la maison a le meme besoin.
//          Il ne s'y trouve pas ce soir pour deux raisons DITES, pas subies :
//          (1) mesure faite, ni NKEditorKit ni NKGui ne portent aujourd'hui la
//              moindre notion de notification (aucun `toast`, `notification`,
//              `bandeau`) -- il n'y a donc rien a reutiliser, et rien a etendre ;
//          (2) le kit est refondu EN CE MOMENT dans un autre arbre
//              (`Nkentseu-noge`), et sa surface de dessin (`NkEditorContext`)
//              n'est pas celle que NK3DModeler emploie pour sa couche overlay
//              (`NkModelerPainter` sur `ui.dlOverlay`). L'y porter ce soir
//              serait ecrire contre une interface qu'un autre deplace.
//          => DETTE NOMMEE, inscrite dans `Applications/NK3DModeler/ROADMAP.md`.
//
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h" // NkOvPainter : la couche peinte EN DERNIER
#include "NKEditorKit/NkScreenLogSink.h" // (25/09) LE NEUVIEME PUITS : ce qu il depose arrive ici

namespace nkentseu {
	namespace nk3d {

		/// La severite decide la couleur, le mot, ET si le message part tout
		/// seul. Un REFUS ne part jamais tout seul : il demande un geste, et un
		/// message qui s'efface avant qu'on l'ait lu ne vaut pas mieux qu'une
		/// note dans un coin -- c'est le defaut qu'on repare, on ne le refait pas
		/// avec un compte a rebours.
		enum class NkToastKind : uint8 {
			Succes = 0,	 ///< l'action a eu lieu -- ce qui a ete ajoute, combien
			Partiel = 1, ///< elle a eu lieu EN PARTIE -- ce qui a ete saute, pourquoi
			Refus = 2	 ///< elle n'a pas eu lieu -- la raison ET ce qu'il faut faire
		};

		inline constexpr int32 kToastMax = 6; ///< au-dela, le plus ANCIEN cede sa place
		inline constexpr uint32 kToastTexte = 256;

		struct NkToast {
				char texte[kToastTexte] = {};
				NkToastKind kind = NkToastKind::Succes;
				float32 restant = 0.f; ///< secondes restantes ; <= 0 = permanent
				bool ferme = false;	   ///< la croix a ete cliquee
				/// (25/09) D'OU VIENT CE BANDEAU. Le puits d'ecran de NKLogger fait
				/// remonter TOUT ce qui est journalise au-dessus de l'avertissement --
				/// y compris l'echo d'un message que le code produit a DEJA pose
				/// lui-meme. `NkImportNote` fait exactement cela : `NkToastPush(buf)`
				/// puis `logger.Warnf("[import] %s", buf)`. Sans cette marque, le meme
				/// refus s'afficherait DEUX FOIS -- et comme un refus n'expire pas,
				/// les deux resteraient. Mesure faite avant d'ecrire ce champ.
				bool venuDuJournal = false;
				/// (25/09) FUSION DES REPETITIONS. Le puits d'ecran de NKEditorKit
				/// fait remonter des messages qui peuvent se repeter des dizaines
				/// de fois par seconde (une boucle de rendu qui refuse un maillage).
				/// Douze bandeaux identiques chassent tout le reste de l'ecran ;
				/// un bandeau et « x 12 » disent la meme chose et laissent la place.
				uint32 repetitions = 1;
		};

		struct NkToastPile {
				NkToast items[kToastMax];
				int32 count = 0;
		};

		inline NkToastPile &NkToasts() {
			static NkToastPile pile;
			return pile;
		}

		/// Le MOT de la severite. Il double la couleur : un daltonien lit le mot,
		/// et une capture en niveaux de gris reste lisible.
		inline const char *NkToastMot(NkToastKind k) {
			return k == NkToastKind::Refus	   ? "REFUSE"
				   : k == NkToastKind::Partiel ? "PARTIEL"
											   : "REUSSI";
		}

		/// Pose un message a l'ecran. Un REFUS reste tant qu'on ne le ferme pas ;
		/// un PARTIEL douze secondes (il y a plus a lire) ; un succes six.
		/// Deux textes disent-ils LE MEME message ? Egaux, ou bien l'un est le
		/// SUFFIXE de l'autre -- c'est le cas de l'echo du journal, qui prefixe la
		/// source entre crochets (« [import] » ). Le seuil de longueur evite
		/// qu'« Annule » et « Import annule » ne soient pris l'un pour l'autre par
		/// accident : en dessous de douze caracteres, on exige l'egalite stricte.
		inline bool NkToastMemeMessage(const char *a, const char *b) {
			if (!a || !b)
				return false;
			uint32 la = 0, lb = 0;
			while (a[la])
				++la;
			while (b[lb])
				++lb;
			if (la == lb) {
				for (uint32 i = 0; i < la; ++i)
					if (a[i] != b[i])
						return false;
				return true;
			}
			const char *court = (la < lb) ? a : b;
			const char *long_ = (la < lb) ? b : a;
			const uint32 lc = (la < lb) ? la : lb, ll = (la < lb) ? lb : la;
			if (lc < 12u)
				return false;
			for (uint32 i = 0; i < lc; ++i)
				if (court[i] != long_[ll - lc + i])
					return false;
			return true;
		}

		inline void NkToastPush(NkToastKind kind, const char *texte, uint32 repetitions = 1,
								bool venuDuJournal = false) {
			NkToastPile &pl = NkToasts();
			// (25/09) MEME TEXTE, MEME SEVERITE, DEJA A L'ECRAN : on compte, on
			// n'empile pas. Et la duree repart : un message qui se repete est un
			// message encore vrai.
			if (texte) {
				for (int32 i = 0; i < pl.count; ++i) {
					if (pl.items[i].ferme)
						continue;
					// ⚠️ LE `kind` N'ENTRE PLUS DANS LA COMPARAISON quand l'un des deux
					//    vient du journal : `NkImportNote` pose un `Refus` et journalise
					//    en `Warn`, que le puits traduit en `Partiel`. Exiger le meme
					//    verdict ferait echouer la fusion sur le cas meme qu'elle doit
					//    couvrir -- et le refus s'afficherait deux fois, en deux couleurs.
					const bool memeVerdict = (pl.items[i].kind == kind);
					const bool echo = venuDuJournal || pl.items[i].venuDuJournal;
					if (!memeVerdict && !echo)
						continue;
					if (NkToastMemeMessage(pl.items[i].texte, texte)) {
						// L'ECHO NE COMPTE PAS. Un message deja pose par le code produit
						// et re-vu par le journal reste UN message : il voit sa duree
						// repartir, pas son compteur monter.
						if (!(venuDuJournal && !pl.items[i].venuDuJournal))
							pl.items[i].repetitions += repetitions;
						// ⚠️ LA DUREE SE RELIT SUR LE BANDEAU EXISTANT, PAS SUR L'ARRIVANT.
						//    Avec l'arrivant, l'echo d'un REFUS (permanent) -- que le
						//    puits traduit en « Partiel » -- lui posait douze secondes
						//    et le faisait disparaitre. Un refus qui s'efface tout seul
						//    est exactement le defaut qu'on repare.
						pl.items[i].restant = (pl.items[i].kind == NkToastKind::Refus)	  ? 0.f
											  : (pl.items[i].kind == NkToastKind::Partiel) ? 12.f
																						   : 6.f;
						return;
					}
				}
			}
			if (pl.count >= kToastMax) {
				// Le plus ANCIEN cede, jamais le plus recent : c'est le dernier
				// message qui repond au dernier geste.
				for (int32 i = 1; i < kToastMax; ++i)
					pl.items[i - 1] = pl.items[i];
				--pl.count;
			}
			NkToast &t = pl.items[pl.count++];
			snprintf(t.texte, sizeof(t.texte), "%s", texte ? texte : "");
			t.kind = kind;
			t.restant = (kind == NkToastKind::Refus)	 ? 0.f
						: (kind == NkToastKind::Partiel) ? 12.f
														 : 6.f;
			t.ferme = false;
			t.repetitions = repetitions < 1u ? 1u : repetitions;
			t.venuDuJournal = venuDuJournal;
		}

		/// Fait vieillir la pile. Appele UNE fois par image avec le vrai `dt` de
		/// la boucle -- jamais un compteur d'images : a 20 images/s, un message
		/// de « six secondes » compte en images en durerait dix-huit.
		inline void NkToastTick(float32 dt) {
			NkToastPile &pl = NkToasts();
			int32 k = 0;
			for (int32 i = 0; i < pl.count; ++i) {
				NkToast &t = pl.items[i];
				if (t.ferme)
					continue;
				if (t.restant > 0.f) {
					t.restant -= dt;
					if (t.restant <= 0.f)
						continue; // expire : il n'est pas recopie
				}
				if (k != i)
					pl.items[k] = t;
				++k;
			}
			pl.count = k;
		}

		inline void NkToastClear() { NkToasts().count = 0; }

		// ── CE QUE LE PUITS D'ECRAN A DEPOSE DEVIENT DES BANDEAUX (25/09) ───────
		// Le neuvieme puits de NKLogger (`NKEditorKit/NkScreenLogSink.h`) depose
		// sur le fil de l'emetteur ; cette fonction RELIT, et elle n'est appelee
		// que depuis le fil d'affichage.
		//
		// ⚠️ ELLE EST UNE FONCTION, ET PAS DIX LIGNES DANS LA BOUCLE, POUR UNE
		//    SEULE RAISON : la sonde `--sonde-messages` appelle EXACTEMENT ce que
		//    le produit appelle. Une sonde qui reecrirait ce drainage de son cote
		//    ne pourrait voir aucun defaut de drainage -- c'est le piege du 18/08,
		//    ou une sonde annoncait 72/72 devant un ecran magenta.
		inline uint32 NkToastDrainerJournal() {
			editorkit::NkEcranLogSink *puits = editorkit::NkEcranLogPuits();
			if (!puits)
				return 0;
			// LE ZERO SE DIT : ce qu'une rafale a fait tomber du tampon est annonce,
			// sinon l'ecran affirme une completude qu'il n'a pas.
			const uint32 perdus = puits->ReprendrePerdus();
			editorkit::NkEcranLogEntree lot[editorkit::kEcranLogMax];
			const uint32 n = puits->Drain(lot, editorkit::kEcranLogMax);
			for (uint32 i = 0; i < n; ++i) {
				// AVERTISSEMENT -> « PARTIEL » (ambre), ERREUR et au-dela -> « REFUSE »
				// (rouge). Les trois verdicts du bandeau disent une ACTION ; les trois
				// niveaux du journal disent une GRAVITE. La correspondance est ecrite
				// ici, une fois.
				const NkToastKind k = (lot[i].niveau >= NkLogLevel::NK_ERROR) ? NkToastKind::Refus
																			  : NkToastKind::Partiel;
				// ⚠️ LE QUATRIEME ARGUMENT DIT « JE VIENS DU JOURNAL », et il compte :
				//    `NkImportNote` pose deja son bandeau ET journalise le meme texte
				//    prefixe de « [import] ». Sans cette marque, chaque refus d'import
				//    s'afficherait DEUX FOIS -- et comme un refus n'expire pas, les deux
				//    resteraient a l'ecran. Mesure faite avant d'ecrire la ligne.
				NkToastPush(k, lot[i].texte, lot[i].repetitions, /*venuDuJournal*/ true);
			}
			if (perdus > 0) {
				char b[80];
				snprintf(b, sizeof(b), "%u message(s) perdu(s) : rafale plus longue que le tampon",
						 (unsigned)perdus);
				NkToastPush(NkToastKind::Partiel, b, 1u, true);
			}
			return n;
		}

		/// LA PILE, PEINTE. En BAS AU CENTRE, juste au-dessus de la barre d'etat :
		/// c'est la zone que l'oeil balaie apres une action, et elle ne recouvre
		/// ni la vue 3D utile ni les panneaux lateraux.
		///
		/// ⚠️ Elle se peint dans la couche OVERLAY (`NkOvPainter`), soumise EN
		/// DERNIER -- l'incrustation se peint en dernier, sinon un panneau peint
		/// apres elle la recouvrirait et le message existerait sans se voir :
		/// exactement le defaut qu'on repare.
		///
		/// Rend le rectangle occupe (vide si rien) pour que l'appelant en
		/// interdise l'entree a ce qui est dessous.
		inline NkRect NkToastPaint(NkHitRegistry &hit, float32 W, float32 H, float32 statusH) {
			NkModelerPainter *po = NkOvPainter();
			NkToastPile &pl = NkToasts();
			if (!po || pl.count <= 0)
				return {};
			NkModelerPainter &p = *po;

			const float32 pad = S(10.f), gap = S(8.f), croixW = S(28.f), bande = S(4.f);
			const float32 lh = S(20.f);
			float32 largeur = W * 0.60f;
			if (largeur < S(380.f))
				largeur = S(380.f);
			if (largeur > W - S(24.f))
				largeur = W - S(24.f);
			const float32 x = (W - largeur) * 0.5f;
			const float32 dispo = largeur - bande - pad * 2.f - croixW;

			// Hauteur de chaque bandeau : le texte est REPLIE, jamais tronque. Un
			// message tronque est un message a moitie dit, et la moitie qui saute
			// est toujours la fin -- c'est-a-dire « ce qu'il faut faire ».
			float32 hauteurs[kToastMax] = {};
			float32 total = 0.f;
			for (int32 i = 0; i < pl.count; ++i) {
				float32 ht = p.TextWrapMeasure(dispo, pl.items[i].texte);
				if (ht < lh)
					ht = lh;
				hauteurs[i] = ht + lh + pad * 2.f; // + la ligne du MOT de severite
				total += hauteurs[i] + gap;
			}
			float32 y = H - statusH - S(10.f) - total;
			if (y < S(4.f))
				y = S(4.f);
			const NkRect emprise{x, y, largeur, total};

			for (int32 i = 0; i < pl.count; ++i) {
				NkToast &t = pl.items[i];
				const NkRect r{x, y, largeur, hauteurs[i] - S(0.f)};
				// Trois couleurs, trois verdicts, tirees des ROLES du theme : une
				// couleur ecrite en dur serait illisible dans l'autre theme, et ce
				// depot a deja paye ce prix.
				//
				// ⚠️ 2026-09-14 — `Partiel` NE PREND PLUS `AccentSel`. Il l'empruntait
				//    faute d'un role d'avertissement dans le theme ; `AccentSel` est
				//    l'ambre de la SELECTION 3D (NkTheme.h, 10bis.2 : « le BLEU dit
				//    l'etat de l'INTERFACE, l'AMBRE dit la selection 3D »). Une
				//    pastille « PARTIEL » posee par-dessus la vue peignait donc,
				//    exactement, la couleur reservee a un element selectionne --
				//    et les deux COEXISTENT a l'ecran. `StatusWarn` existe depuis
				//    ce jour : trois verdicts, trois roles de la MEME famille.
				const NkRole accent = (t.kind == NkToastKind::Refus)	 ? NkRole::StatusErr
									  : (t.kind == NkToastKind::Partiel) ? NkRole::StatusWarn
																		 : NkRole::StatusOk;
				p.Fill(r, NkRole::PanelHeader, S(6.f));
				p.OutlineSharp(r, accent);
				// La bande de gauche redit la severite SANS dependre de la lecture.
				p.Fill({r.x, r.y, bande, r.h}, accent, 0.f);

				const float32 tx = r.x + bande + pad;
				// Ligne 1 : le MOT. Ligne 2+ : la phrase, repliee.
				p.TextV(tx, r.y + pad * 0.5f, lh, NkToastMot(t.kind), accent);
				// « x 12 » A DROITE DU MOT. Il n'apparait qu'a partir de DEUX :
				// « x 1 » sur chaque message serait du bruit permanent.
				if (t.repetitions > 1) {
					char rep[24];
					snprintf(rep, sizeof(rep), "x %u", (unsigned)t.repetitions);
					p.TextV(tx + p.TextW(NkToastMot(t.kind)) + S(10.f), r.y + pad * 0.5f, lh, rep,
							accent);
				}
				p.Clip({r.x, r.y, r.w - croixW, r.h});
				(void)p.TextWrap(tx, r.y + pad * 0.5f + lh, dispo, t.texte, NkRole::Text);
				p.Unclip();

				// LA CROIX. Un refus n'expire pas : sans elle il resterait pour
				// toujours, et un message qu'on ne peut pas retirer devient un
				// meuble qu'on cesse de voir -- le defaut d'origine, en plus gros.
				char key[32];
				snprintf(key, sizeof(key), "toast.x.%d", i);
				const NkRect rx{r.x + r.w - croixW, r.y, croixW, lh + pad};
				const bool ov = hit.Add(key, rx);
				p.IconV(rx.x + S(7.f), rx.y, rx.h, NkIcon::WinClose,
						ov ? NkRole::Text : NkRole::TextMuted);
				if (hit.Clicked(key))
					t.ferme = true;
				y += hauteurs[i] + gap;
			}
			return emprise;
		}

	} // namespace nk3d
} // namespace nkentseu
