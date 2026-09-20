#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkAiThreadLayout.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LE PLAN DU FIL : tous les rectangles que le peintre va peindre, et
//          RIEN D'AUTRE. Publie par le peintre, lu par la sonde.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI LE PEINTRE EST COUPE EN DEUX, ET C'EST LA DECISION DE CE FICHIER
//   Un peintre ordinaire calcule une position puis dessine, dans le meme geste.
//   Il est alors INVERIFIABLE sans fenetre : pour savoir ou il a mis les choses,
//   il faut regarder des pixels.
//
//   Ici la geometrie est un PRODUIT : `NkAiFilMesurer` rend un plan -- la liste
//   des rectangles, chacun nomme, chacun rattache au bloc qui le porte. Le
//   peintre (`NkAiThreadPaint.h`) ne fait que TRANSCRIRE ce plan : il ne calcule
//   aucune position, il n'en a pas le droit.
//
//   ⚠️ CE QUE CA ACHETE, ET C'EST LE POINT : *la sonde compare des rectangles
//      PUBLIES PAR LEUR PEINTRE, jamais recalcules dans la sonde.* Une sonde qui
//      refait le calcul mesure sa propre copie du calcul -- elle verdit sur un
//      peintre casse dont elle partage l'erreur. Ici elle lit le plan, et le
//      plan EST ce qui sera peint.
//
//   ⚠️ ET LA SECONDE FAUTE, INVERSE, QUE CETTE FORME REND IMPOSSIBLE : un temoin
//      place EN AMONT de la passe lit des zeros et conclut « conforme ». Le
//      panneau du modeleur l'a paye le 20/09 -- place avant la peinture, son
//      temoin lisait le panneau a [0..0] et trouvait « atteignable » parce que
//      1571 >= 0. Ici il n'y a pas d'amont possible : le plan n'existe qu'une
//      fois mesure, et un plan vide se voit (`Pieces() == 0`).
//
// ZERO NKGui, ZERO FONTE, ZERO FENETRE
//   Ce fichier ne connait que `NkRole` (pour dire QUELLE couleur, pas laquelle)
//   et une fonction de mesure de texte fournie par l'appelant. Dans
//   l'application c'est la vraie fonte ; au banc c'est une regle deterministe.
//   La GEOMETRIE -- indentation, repli, troncature, ordre -- est la meme dans
//   les deux cas, et c'est elle qu'on eprouve.
//
// ⚠️ TOUT CE QUI DESIGNE, DESIGNE PAR SON NOM.
//    Une piece se retrouve par `(identifiant du bloc, nom de la piece)`, jamais
//    par sa position dans le tableau. Le tableau est un ORDRE DE PEINTURE : il
//    change des qu'un bloc se deplie. Le depot a paye quatre fois en une
//    journee la designation par indice, et ce fichier en a paye une cinquieme
//    la veille (`NkAiFil::Basculer(uint32 i)`, corrige le meme jour).
//
// LES MESURES VIENNENT DE LA CAPTURE, PAS D'UN GOUT
//   `Screenshot 2026-09-20 111040.png`, 695 x 1292, relevee au pixel :
//     - la bulle de la demande : x 24 .. 654 sur 695 -- retrait de 24 a gauche,
//       41 a droite (la barre de defilement du fil vit la) ;
//     - les blocs du fil : x ~55, soit 31 px PLUS INDENTES que la demande ;
//     - le filet vertical du rail : x ~36 ; les puces dessus ;
//     - le compartiment `IN` : gouttiere du bord du bloc a x ~92, contenu apres.
//   Elles sont donc exprimees en RETRAITS depuis les bords, jamais en positions
//   absolues : un panneau de 320 px de large doit rester juste.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkAiThread.h"
#include "NKEditorKit/NkTheme.h"

namespace nkentseu {
	namespace editorkit {

		// ── LES PIECES D'UN BLOC ────────────────────────────────────────────────
		// Le NOM d'un rectangle. C'est par lui que la sonde interroge le plan.
		// APPEND-ONLY, comme tout le reste de ce kit.
		enum class NkAiPiece : uint8 {
			/// Le cadre d'un bloc (la bulle de la demande, le cadre d'un outil).
			Cadre = 0,
			/// La puce ronde du rail. Verte pour un outil, grise sinon.
			Puce,
			/// Le nom en gras (`Bash`), ou le libelle d'un refus.
			Titre,
			/// La phrase courante : prose, phrase courte d'outil, motif, effet.
			Texte,
			/// La gouttiere `IN` (le libelle), sur le fond du BLOC.
			GouttiereIn,
			/// Le fond du compartiment d'entree. Porte `CodeBg`.
			FondIn,
			/// Le contenu d'entree, a chasse fixe.
			TexteIn,
			/// La gouttiere `OUT`.
			GouttiereOut,
			/// Le fond du compartiment de sortie. Porte `CodeOutBg`.
			FondOut,
			/// Le contenu de sortie, a chasse fixe.
			TexteOut,
			/// L'EFFET MESURE, sur la MEME LIGNE que le texte du bloc.
			/// ⚠️ AJOUTEE LE 20/09, ET C'EST L'INTEGRATION QUI L'A REVELEE. Le
			///    panneau de NK3DModeler affiche « <demande>   .   faces 6 -> 384 » :
			///    le texte ET l'effet, sur une seule ligne, parce que sa regle est
			///    « L'EFFET EST SUR LA LIGNE, pas cache dans le repli ». Mon contrat
			///    ne savait le rendre pour AUCUN type : `Outil` ignorait `effet`, et
			///    un bloc `Effet` n'affiche QUE l'effet, sans le texte. Aucun des
			///    deux ne reproduisait ce qui est a l'ecran aujourd'hui.
			///    *Une integration faite tot paie en montrant ce qu'un contrat ne
			///    sait pas dire* -- ici avant meme la premiere ligne de migration.
			Effet,

			/// La bande d'estompe en bas d'un compartiment TRONQUE. ⚠️ Elle n'est
			/// publiee QUE s'il y a vraiment eu troncature : une estompe permanente
			/// dirait « il y a la suite » sur un bloc complet.
			Estompe,

			Count
		};

		inline const char *NkAiPieceNom(NkAiPiece p) {
			switch (p) {
				case NkAiPiece::Cadre:		  return "cadre";
				case NkAiPiece::Puce:		  return "puce";
				case NkAiPiece::Titre:		  return "titre";
				case NkAiPiece::Texte:		  return "texte";
				case NkAiPiece::GouttiereIn:  return "gouttiere_in";
				case NkAiPiece::FondIn:		  return "fond_in";
				case NkAiPiece::TexteIn:	  return "texte_in";
				case NkAiPiece::GouttiereOut: return "gouttiere_out";
				case NkAiPiece::FondOut:	  return "fond_out";
				case NkAiPiece::TexteOut:	  return "texte_out";
				case NkAiPiece::Effet:				  return "effet";
				case NkAiPiece::Estompe:	  return "estompe";
				default:					  return "";
			}
		}

		/// Quelle police le peintre doit prendre. La chasse fixe est une DETTE
		/// NOMMEE tant que la coquille ne charge qu'une fonte : le panneau du
		/// modeleur la porte deja par ecrit. Le plan la demande quand meme --
		/// c'est le peintre qui se rabat, pas le plan qui ment.
		enum class NkAiPolice : uint8 { Normale = 0, Grasse, ChasseFixe };

		// ── UN RECTANGLE PUBLIE ─────────────────────────────────────────────────
		struct NkAiRectPublie {
				/// L'identifiant STABLE du bloc porteur. Jamais son indice.
				uint32 blocId = 0;
				NkAiPiece piece = NkAiPiece::Cadre;
				/// La couleur est dite par son ROLE, jamais par sa valeur : c'est ce
				/// qui fait que le theme clair suit sans qu'on y retouche.
				NkRole role = NkRole::Text;
				NkAiPolice police = NkAiPolice::Normale;
				float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
		};

		// ── LES RETRAITS, RELEVES SUR LA CAPTURE ────────────────────────────────
		struct NkAiMetriques {
				/// Retrait de la bulle de la demande, a gauche. Capture : 24.
				float32 retraitDemande = 24.f;
				/// Place reservee a droite pour la barre de defilement. Capture : 41.
				float32 margeDroite = 41.f;
				/// Retrait des blocs du fil. Capture : ~55, soit 31 de plus.
				float32 retraitFil = 55.f;
				/// Abscisse du filet vertical du rail. Capture : ~36.
				float32 railX = 36.f;
				/// Rayon de la puce.
				float32 puceR = 3.5f;
				/// Largeur de la gouttiere `IN`/`OUT`, depuis le bord du bloc.
				float32 gouttiere = 37.f;
				/// Hauteur d'une ligne de texte.
				float32 ligne = 18.f;
				/// Respiration verticale entre deux blocs.
				float32 entreBlocs = 14.f;
				/// Marge interne d'un cadre.
				float32 padding = 8.f;
				/// ⚠️ PLAFOND DE LIGNES D'UN COMPARTIMENT. Au-dela, on TRONQUE et on
				///    publie une estompe. La capture tronque : ses blocs `IN`
				///    s'arretent net et le bas s'efface.
				uint32 lignesMax = 4;
				/// Hauteur de la bande d'estompe.
				float32 estompeH = 10.f;
		};

		/// Mesure la largeur d'un texte. Fournie par l'appelant : la vraie fonte
		/// dans l'application, une regle deterministe au banc.
		typedef float32 (*NkAiMesureTexte)(void *ctx, NkAiPolice police, const char *texte);

		// ── LE PLAN ─────────────────────────────────────────────────────────────
		class NkAiPlan {
			public:
				void Vider() {
					mPieces.Clear();
					mHauteur = 0.f;
				}
				void Ajouter(const NkAiRectPublie &r) {
					mPieces.PushBack(r);
				}
				uint32 Pieces() const {
					return (uint32)mPieces.Size();
				}
				const NkAiRectPublie &Piece(uint32 i) const {
					return mPieces[i];
				}
				float32 Hauteur() const {
					return mHauteur;
				}
				void PoserHauteur(float32 h) {
					mHauteur = h;
				}

				/// ⚠️ LA SEULE FACON DE RETROUVER UN RECTANGLE : par le NOM du bloc
				///    et le NOM de la piece. Il n'existe volontairement AUCUNE
				///    surcharge par indice -- l'ordre du tableau est un ordre de
				///    PEINTURE, il change des qu'un bloc se deplie, et s'en servir
				///    pour designer serait la faute que ce depot a payee quatre fois
				///    le 20/09.
				bool Trouver(uint32 blocId, NkAiPiece p, NkAiRectPublie &out) const {
					for (usize i = 0; i < mPieces.Size(); ++i)
						if (mPieces[i].blocId == blocId && mPieces[i].piece == p) {
							out = mPieces[i];
							return true;
						}
					return false;
				}
				bool Possede(uint32 blocId, NkAiPiece p) const {
					NkAiRectPublie tmp;
					return Trouver(blocId, p, tmp);
				}

			private:
				NkVector<NkAiRectPublie> mPieces;
				float32 mHauteur = 0.f;
		};

		namespace aidetail {

			/// Combien de lignes occupe `texte` dans `largeur`. Bornee par
			/// `lignesMax` ; `tronque` dit si on a coupe.
			inline uint32 Lignes(const char *texte, float32 largeur, NkAiPolice pol,
								 NkAiMesureTexte mes, void *ctx, uint32 lignesMax,
								 bool &tronque) {
				tronque = false;
				if (!texte || texte[0] == '\0' || largeur <= 1.f)
					return 0;
				// Une ligne par saut de ligne, plus le repli sur la largeur. On ne
				// coupe pas les mots ici : le peintre le fera, et la HAUTEUR est ce
				// qui nous interesse.
				uint32 n = 0;
				const char *debut = texte;
				for (const char *p = texte;; ++p) {
					if (*p == '\n' || *p == '\0') {
						NkString seg;
						for (const char *q = debut; q < p; ++q) {
							const char d[2] = {*q, '\0'};
							seg.Append(d);
						}
						const float32 w = mes(ctx, pol, seg.CStr());
						uint32 sousLignes = 1;
						if (w > largeur) {
							sousLignes = (uint32)(w / largeur);
							if (w > (float32)sousLignes * largeur)
								++sousLignes;
							if (sousLignes < 1)
								sousLignes = 1;
						}
						n += sousLignes;
						if (*p == '\0')
							break;
						debut = p + 1;
					}
				}
				if (n > lignesMax) {
					n = lignesMax;
					tronque = true;
				}
				return n;
			}

		} // namespace aidetail

		// ── LA MESURE ───────────────────────────────────────────────────────────
		/// Produit le plan du fil pour une largeur de panneau donnee.
		/// ⚠️ ELLE NE DESSINE RIEN et n'a aucun effet de bord : deux appels avec
		///    les memes entrees rendent le meme plan. C'est ce qui permet a la
		///    sonde de la faire tourner sans coquille, sans fenetre et sans GPU.
		inline void NkAiFilMesurer(const NkAiFil &fil, float32 largeurPanneau,
								   const NkAiMetriques &m, NkAiMesureTexte mes, void *ctx,
								   NkAiPlan &plan) {
			plan.Vider();
			if (largeurPanneau <= m.retraitFil + m.margeDroite + 4.f)
				return; // panneau trop etroit : un plan VIDE, pas un plan faux

			float32 y = m.entreBlocs;
			const float32 droite = largeurPanneau - m.margeDroite;

			auto Pousser = [&](uint32 id, NkAiPiece p, NkRole role, NkAiPolice pol,
							   float32 x, float32 yy, float32 w, float32 h) {
				NkAiRectPublie r;
				r.blocId = id;
				r.piece = p;
				r.role = role;
				r.police = pol;
				r.x = x;
				r.y = yy;
				r.w = w;
				r.h = h;
				plan.Ajouter(r);
			};

			for (uint32 i = 0; i < fil.Taille(); ++i) {
				const NkAiBlocDonnees &b = fil.At(i);

				// ── LA DEMANDE : pleine largeur, HORS du rail, jamais repliee ────
				// C'est la question. Elle n'a pas de puce et n'est pas indentee
				// comme le reste : la capture la met a 24, le fil a 55.
				if (b.type == NkAiBloc::Demande) {
					const float32 x = m.retraitDemande;
					const float32 w = droite - x;
					bool tronque = false;
					const uint32 n = aidetail::Lignes(b.texte.CStr(), w - 2.f * m.padding,
													  NkAiPolice::Normale, mes, ctx,
													  64u /* la question ne se tronque pas */,
													  tronque);
					const float32 h = (float32)(n == 0 ? 1 : n) * m.ligne + 2.f * m.padding;
					Pousser(b.id, NkAiPiece::Cadre, NkRole::PanelBg, NkAiPolice::Normale, x, y, w, h);
					Pousser(b.id, NkAiPiece::Texte, NkRole::Text, NkAiPolice::Normale,
							x + m.padding, y + m.padding, w - 2.f * m.padding,
							(float32)(n == 0 ? 1 : n) * m.ligne);
					y += h + m.entreBlocs;
					continue;
				}

				// ── LES AUTRES : sur le rail, avec leur puce ─────────────────────
				const float32 x = m.retraitFil;
				const float32 w = droite - x;
				// La puce, centree sur la premiere ligne du bloc.
				const NkRole rolePuce = (b.type == NkAiBloc::Outil) ? NkRole::StatusOk
																	: NkRole::TextMuted;
				Pousser(b.id, NkAiPiece::Puce, rolePuce, NkAiPolice::Normale,
						m.railX - m.puceR, y + m.ligne * 0.5f - m.puceR, m.puceR * 2.f,
						m.puceR * 2.f);

				// La premiere ligne : un titre gras (s'il y en a un) puis le texte.
				float32 xTexte = x;
				if (b.titre.Length() > 0) {
					const float32 wT = mes(ctx, NkAiPolice::Grasse, b.titre.CStr());
					Pousser(b.id, NkAiPiece::Titre, NkRole::Text, NkAiPolice::Grasse, x, y, wT,
							m.ligne);
					xTexte = x + wT + 8.f;
				}
				// Le texte de la premiere ligne. Pour un refus ou un echec, c'est le
				// MOTIF -- il est sur la ligne, jamais cache dans le repli : un motif
				// qu'il faut deplier pour lire ne sert a personne.
				const char *surLaLigne = b.texte.CStr();
				NkRole roleTexte = NkRole::Text;
				if (b.type == NkAiBloc::Refus) {
					surLaLigne = b.motif.CStr();
					roleTexte = NkRole::StatusWarn; // un refus n'est PAS une erreur
				} else if (b.type == NkAiBloc::Echec) {
					surLaLigne = b.motif.CStr();
					roleTexte = NkRole::StatusErr;
				} else if (b.type == NkAiBloc::Effet) {
					surLaLigne = b.effet.CStr();
					roleTexte = NkRole::TextMuted;
				} else if (b.titre.Length() > 0) {
					roleTexte = NkRole::TextMuted; // la phrase courte d'un outil
				}
				// ⚠️ L'EFFET RESERVE SA PLACE AVANT LE TEXTE, ET C'EST UNE REGLE.
				//    Quand la place manque, c'est la PROSE qui se tronque, jamais la
				//    mesure : « faces 6 -> 384 » est le fait, la phrase est le
				//    commentaire. Le panneau du modeleur pose deja cette regle sous une
				//    autre forme (« l'effet est sur la ligne, pas cache dans le repli ») ;
				//    elle monte ici pour valoir dans les trois applications.
				float32 wEffet = 0.f;
				const bool aEffet = (b.type != NkAiBloc::Effet) && b.effet.Length() > 0;
				if (aEffet)
					wEffet = mes(ctx, NkAiPolice::Normale, b.effet.CStr()) + 12.f;
				float32 wTexte = droite - xTexte - wEffet;
				if (wTexte < 24.f) {
					// Trop etroit pour les deux : la mesure passe, la prose non.
					wTexte = 0.f;
					wEffet = droite - xTexte;
				}
				bool tronque = false;
				const uint32 nTexte = aidetail::Lignes(surLaLigne, wTexte, NkAiPolice::Normale, mes,
							   ctx, b.replie ? 1u : m.lignesMax, tronque);
				const float32 hTexte = (float32)(nTexte == 0 ? 1 : nTexte) * m.ligne;
				if (wTexte > 0.f)
					Pousser(b.id, NkAiPiece::Texte, roleTexte, NkAiPolice::Normale, xTexte, y, wTexte,
						  hTexte);
				// L'effet, cale a DROITE de la premiere ligne. Role attenue : c'est un fait
				// mesure, pas une alerte, et il ne doit pas voler la lecture de la phrase
				// qu'il complete.
				if (aEffet)
					Pousser(b.id, NkAiPiece::Effet, NkRole::TextMuted, NkAiPolice::Normale,
						  droite - wEffet, y, wEffet, m.ligne);
				float32 yb = y + hTexte;

				// ── LES COMPARTIMENTS : SEULEMENT DEPLIE ─────────────────────────
				// ⚠️ Un bloc replie ne publie AUCUN rectangle de compartiment. Pas un
				//    rectangle de hauteur nulle, pas un rectangle invisible : RIEN.
				//    Un rectangle de hauteur nulle serait reclame au registre de
				//    clics et volerait le survol du bloc voisin.
				if (!b.replie && b.type == NkAiBloc::Outil) {
					const float32 xc = x + m.gouttiere;
					const float32 wc = droite - xc;
					if (b.entree.Length() > 0) {
						bool tr = false;
						const uint32 n = aidetail::Lignes(b.entree.CStr(), wc,
														  NkAiPolice::ChasseFixe, mes, ctx,
														  m.lignesMax, tr);
						const float32 h = (float32)n * m.ligne + m.padding;
						Pousser(b.id, NkAiPiece::FondIn, NkRole::CodeBg, NkAiPolice::Normale, x,
								yb, droite - x, h);
						Pousser(b.id, NkAiPiece::GouttiereIn, NkRole::TextMuted,
								NkAiPolice::Normale, x + 4.f, yb + m.padding * 0.5f,
								m.gouttiere - 8.f, m.ligne);
						Pousser(b.id, NkAiPiece::TexteIn, NkRole::Text, NkAiPolice::ChasseFixe,
								xc, yb + m.padding * 0.5f, wc, (float32)n * m.ligne);
						if (tr)
							Pousser(b.id, NkAiPiece::Estompe, NkRole::CodeBg,
									NkAiPolice::Normale, x, yb + h - m.estompeH, droite - x,
									m.estompeH);
						yb += h;
					}
					if (b.sortie.Length() > 0) {
						bool tr = false;
						const uint32 n = aidetail::Lignes(b.sortie.CStr(), wc,
														  NkAiPolice::ChasseFixe, mes, ctx,
														  m.lignesMax, tr);
						const float32 h = (float32)n * m.ligne + m.padding;
						Pousser(b.id, NkAiPiece::FondOut, NkRole::CodeOutBg,
								NkAiPolice::Normale, x, yb, droite - x, h);
						Pousser(b.id, NkAiPiece::GouttiereOut, NkRole::TextMuted,
								NkAiPolice::Normale, x + 4.f, yb + m.padding * 0.5f,
								m.gouttiere - 8.f, m.ligne);
						Pousser(b.id, NkAiPiece::TexteOut, NkRole::Text,
								NkAiPolice::ChasseFixe, xc, yb + m.padding * 0.5f, wc,
								(float32)n * m.ligne);
						if (tr)
							Pousser(b.id, NkAiPiece::Estompe, NkRole::CodeOutBg,
									NkAiPolice::Normale, x, yb + h - m.estompeH, droite - x,
									m.estompeH);
						yb += h;
					}
				}
				y = yb + m.entreBlocs;
			}
			plan.PoserHauteur(y);
		}

		/// Quel bloc se trouve sous `(px, py)` ? Rend son IDENTIFIANT, pas sa
		/// position -- c'est ce que le panneau rendra a `BasculerParId`.
		/// ⚠️ Rendre un indice ici aurait suffi a ramener la faute : entre le clic
		///    et la bascule, une reponse peut arriver et faire glisser le fil.
		inline bool NkAiFilBlocSous(const NkAiPlan &plan, float32 px, float32 py,
									uint32 &blocIdOut) {
			// Parcours a l'envers : les pieces publiees en dernier sont peintes en
			// dernier, donc au-dessus.
			for (uint32 i = plan.Pieces(); i > 0; --i) {
				const NkAiRectPublie &r = plan.Piece(i - 1);
				if (r.w <= 0.f || r.h <= 0.f)
					continue;
				if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h) {
					blocIdOut = r.blocId;
					return true;
				}
			}
			return false;
		}

	} // namespace editorkit
} // namespace nkentseu
