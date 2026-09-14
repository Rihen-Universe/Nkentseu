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
		inline void NkToastPush(NkToastKind kind, const char *texte) {
			NkToastPile &pl = NkToasts();
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
				const NkRole accent = (t.kind == NkToastKind::Refus)	 ? NkRole::StatusErr
									  : (t.kind == NkToastKind::Partiel) ? NkRole::AccentSel
																		 : NkRole::StatusOk;
				p.Fill(r, NkRole::PanelHeader, S(6.f));
				p.OutlineSharp(r, accent);
				// La bande de gauche redit la severite SANS dependre de la lecture.
				p.Fill({r.x, r.y, bande, r.h}, accent, 0.f);

				const float32 tx = r.x + bande + pad;
				// Ligne 1 : le MOT. Ligne 2+ : la phrase, repliee.
				p.TextV(tx, r.y + pad * 0.5f, lh, NkToastMot(t.kind), accent);
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
