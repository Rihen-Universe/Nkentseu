#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkAiThreadPaint.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LA TRANSCRIPTION : le plan du fil devient des commandes de dessin,
//          et RIEN de plus. Aucune geometrie ne vit ici.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ LA REGLE DE CE FICHIER, ET C'EST LA SEULE : IL NE CALCULE AUCUNE POSITION.
//    Il lit les rectangles que `NkAiFilMesurer` a publies et les rend au
//    peintre. La seule arithmetique autorisee est la TRANSLATION d'origine du
//    panneau (`ox`, `oy`), appliquee uniformement a tout le plan -- et elle est
//    mesuree (translation nulle = identite ; translation quelconque = decalage
//    exact, essais 23f/23g).
//
//    C'est ce qui fait que le temoin du plan couvre vraiment ce qui est peint.
//    Si cette transcription se remettait a calculer, le plan deviendrait une
//    opinion et la sonde mesurerait autre chose que l'ecran.
//
// IL N'INVENTE PAS NON PLUS DE COULEUR
//   Chaque piece porte son ROLE ; la couleur est demandee au peintre
//   (`ColorOf`). Une valeur en dur est litteralement inecrivable ici, et c'est
//   par ce chemin -- et lui seul -- que les trois surfaces posees le 20/09
//   (`code_bg`, `code_out_bg`, `inline_code_bg`) suivent le theme clair.
//
// ⚠️ POURQUOI `NkComponentPaint` ET PAS `NkGuiDrawList` DIRECTEMENT.
//    Le kit porte deja l'interface, son implementation NKGui
//    (`NkGuiComponentPaint.h`) ET un enregistreur headless
//    (`Components/NkRecordingPaint.h`). Ecrire contre `NkGuiDrawList` aurait
//    rendu cette transcription INEPROUVABLE sans fenetre -- c'est-a-dire
//    exactement le defaut que la coupe plan/peintre existe pour eviter. Je ne
//    l'ai pas devine : le kit l'avait deja, il fallait regarder avant d'ecrire.
//
//    Et l'enregistreur rend une couleur par role, INJECTIVE -- son propre
//    en-tete le dit : « si deux roles rendaient la meme couleur, une erreur de
//    role serait invisible dans le flux enregistre ». Une piece peinte avec le
//    mauvais role se voit donc au banc.
//
// ⚠️ LE TEXTE N'EST PAS DANS LE PLAN, ET C'EST DELIBERE.
//    Le plan ne porte que de la geometrie. Mettre dans chaque rectangle un
//    `const char *` vers la chaine du bloc aurait fait vivre un POINTEUR au-dela
//    de son proprietaire -- le depot a deja paye « le registre garde un
//    pointeur : declarer par valeur = segfault mouvant ». La transcription
//    retrouve donc le texte dans le fil, PAR L'IDENTIFIANT DU BLOC. Plus lent,
//    et sans pointeur suspendu.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkAiThreadLayout.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		namespace aipaint {

			/// Le texte que porte une piece, ou `nullptr` si elle n'en porte pas
			/// (les fonds, la puce, l'estompe).
			/// ⚠️ PAR L'IDENTIFIANT DU BLOC, jamais par sa position dans le fil :
			///    entre la mesure et la peinture, rien ne glisse dans une image,
			///    mais la regle ne souffre pas d'exception locale -- c'est ainsi
			///    qu'elle se perd.
			inline const char *TextePiece(const NkAiFil &fil, uint32 blocId, NkAiPiece p) {
				uint32 i = 0;
				if (!fil.TrouverParId(blocId, i))
					return nullptr;
				const NkAiBlocDonnees &b = fil.At(i);
				switch (p) {
					case NkAiPiece::Titre:
						return b.titre.CStr();
					case NkAiPiece::Texte:
						// Le bloc dit lui-meme ce qui va sur sa ligne : le motif pour
						// un refus ou un echec, la mesure pour un effet. Le plan a
						// deja choisi le ROLE correspondant ; ici on ne rechoisit
						// rien, on prend la meme regle -- deux regles ecrites
						// separement finissent toujours par diverger.
						if (b.type == NkAiBloc::Refus || b.type == NkAiBloc::Echec)
							return b.motif.CStr();
						if (b.type == NkAiBloc::Effet)
							return b.effet.CStr();
						return b.texte.CStr();
					case NkAiPiece::GouttiereIn:
						return "IN";
					case NkAiPiece::GouttiereOut:
						return "OUT";
					case NkAiPiece::TexteIn:
						return b.entree.CStr();
					case NkAiPiece::Effet:
						return b.effet.CStr();
					case NkAiPiece::TexteOut:
						return b.sortie.CStr();
					default:
						return nullptr;
				}
			}

			/// L'arrondi d'une piece. Une table, pas un `if` disperse : un arrondi
			/// choisi au site de dessin se met a differer d'un bloc a l'autre.
			inline float32 Arrondi(NkAiPiece p) {
				switch (p) {
					case NkAiPiece::Cadre:
						return 6.f;
					case NkAiPiece::FondIn:
					case NkAiPiece::FondOut:
						return 4.f;
					default:
						return 0.f;
				}
			}

		} // namespace aipaint

		/// Ce que le CHROME affiche : il n appartient a aucun bloc du fil, donc
		/// `TextePiece` ne peut pas le trouver. Passe a part, et facultatif.
		struct NkAiChromeTextes {
			const char *titre = nullptr;
			const char *composeur = nullptr;
		};

		namespace aipaint {
			/// L horloge de l historique, TRACEE : le kit n a aucun atlas.
			/// ⚠️ LE PEINTRE DESSINE DANS UN RECTANGLE PUBLIE, il ne decide pas ou
			///    ce rectangle se trouve. La regle du fichier tient : aucune position
			///    n est calculee ici, seulement des proportions DU rectangle recu.
			inline void Horloge(NkComponentPaint &p, const NkPaintRect &r, uint16 role) {
				p.Ellipse(r, role);
				const NkPaintRect creux = {r.x + 1.5f, r.y + 1.5f, r.w - 3.f, r.h - 3.f};
				p.Fill(creux, (uint16)NkRole::PanelBg, creux.w * 0.5f);
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
				(void)p.Line(cx, cy, cx, cy - r.h * 0.28f, role, 1.f);
				(void)p.Line(cx, cy, cx + r.w * 0.22f, cy, role, 1.f);
			}
			/// La bulle portant un +, pour la conversation neuve.
			inline void BullePlus(NkComponentPaint &p, const NkPaintRect &r, uint16 role) {
				p.Outline(r, role, (uint16)NkRole::PanelBg, r.w * 0.28f);
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
				const float32 b = r.w * 0.22f;
				(void)p.Line(cx - b, cy, cx + b, cy, role, 1.f);
				(void)p.Line(cx, cy - b, cx, cy + b, role, 1.f);
			}
		} // namespace aipaint

		/// Transcrit le plan. `ox`, `oy` : l'origine du panneau a l'ecran.
		/// ⚠️ ELLE NE LIT NI LA SOURIS NI L'HORLOGE : deux appels avec les memes
		///    entrees produisent le meme flux de commandes, ce que le banc verifie
		///    (23h). Un peintre qui depend de l'etat du bureau rend des sondes qui
		///    rougissent parce que quelqu'un a bouge sa souris.
		inline void NkAiFilPeindre(NkComponentPaint &p, const NkAiFil &fil, const NkAiPlan &plan,
								   float32 ox, float32 oy,
								   const NkAiChromeTextes &chrome = NkAiChromeTextes{}) {
			for (uint32 i = 0; i < plan.Pieces(); ++i) {
				const NkAiRectPublie &r = plan.Piece(i);
				// ⚠️ LA SEULE ARITHMETIQUE DU FICHIER, et elle est uniforme.
				const NkPaintRect rect = {r.x + ox, r.y + oy, r.w, r.h};
				const uint16 role = (uint16)r.role;

				switch (r.piece) {
					case NkAiPiece::Cadre:
						// Plein PUIS creuse d'un pixel : `Outline` prend deux roles,
						// et le second n'est pas decoratif -- creuser suppose de
						// savoir quoi remettre a l'interieur. L'en-tete du peintre
						// raconte deux sites de NkUIDesign qui lui passaient 0 en
						// croyant ecrire « pas d'interieur ».
						p.Outline(rect, (uint16)NkRole::Border, role, aipaint::Arrondi(r.piece));
						break;
					case NkAiPiece::FondIn:
					case NkAiPiece::FondOut:
					case NkAiPiece::Estompe:
						p.Fill(rect, role, aipaint::Arrondi(r.piece));
						break;
					case NkAiPiece::Puce:
						p.Ellipse(rect, role);
						break;
					case NkAiPiece::Filet:
						p.Fill(rect, role, 0.f);
						break;
					case NkAiPiece::ComposeurCadre:
						p.Outline(rect, (uint16)NkRole::Border, role, 6.f);
						break;
					case NkAiPiece::IconeHistorique:
						aipaint::Horloge(p, rect, role);
						break;
					case NkAiPiece::IconeNouvelle:
						aipaint::BullePlus(p, rect, role);
						break;
					case NkAiPiece::TitreConversation:
						if (chrome.titre && chrome.titre[0])
							p.Text(rect, chrome.titre, role, NkTextAlign::Left);
						break;
					case NkAiPiece::ComposeurTexte:
						if (chrome.composeur && chrome.composeur[0])
							p.Text(rect, chrome.composeur, role, NkTextAlign::Left);
						break;
					default: {
						const char *s = aipaint::TextePiece(fil, r.blocId, r.piece);
						// ⚠️ UNE PIECE DE TEXTE SANS TEXTE NE SE PEINT PAS. Peindre
						//    une chaine vide poserait une commande que le banc
						//    compterait et que l'ecran ne montrerait pas : les deux
						//    mesures cesseraient de dire la meme chose.
						if (s && s[0] != '\0')
							p.Text(rect, s, role, NkTextAlign::Left);
						break;
					}
				}
			}
		}

	} // namespace editorkit
} // namespace nkentseu
