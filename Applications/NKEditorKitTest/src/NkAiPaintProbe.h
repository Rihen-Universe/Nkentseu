#pragma once
// -----------------------------------------------------------------------------
// @File    Applications/NKEditorKitTest/src/NkAiPaintProbe.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   FAMILLE 23 — la transcription du plan en commandes de dessin.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// CE QU'ELLE MESURE, ET POURQUOI ELLE PEUT LE MESURER SANS FENETRE
//   `NkAiFilPeindre` ecrit dans `NkComponentPaint`, l'interface que le kit
//   portait DEJA, avec son enregistreur headless (`NkRecordingPaint`) et son
//   implementation NKGui. La sonde branche l'enregistreur et lit le flux.
//
// ⚠️ ET ELLE COMPARE LE FLUX AU PLAN, PAS A UN CALCUL A ELLE. C'est la regle
//    posee au lot precedent : *des rectangles publies par leur peintre, jamais
//    recalcules dans la sonde.* Ici il y a DEUX producteurs a confronter -- le
//    plan (`NkAiFilMesurer`) et le flux (`NkAiFilPeindre`) -- et la sonde ne
//    fait que verifier qu'ils disent la meme chose. Elle n'a aucune geometrie
//    de son eu.
//
// ⚠️ L'ENREGISTREUR REND UNE COULEUR PAR ROLE, ET ELLE EST INJECTIVE -- son
//    en-tete le dit : « si deux roles rendaient la meme couleur, une erreur de
//    role serait invisible dans le flux enregistre ». C'est ce qui permet a 23c
//    d'attraper une piece peinte avec le mauvais role, et pas seulement une
//    piece absente.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkAiThreadPaint.h"
#include "NKEditorKit/Components/NkRecordingPaint.h"
#include <stdio.h>

namespace aipaintprobe {

	using namespace nkentseu;
	using namespace nkentseu::editorkit;

	struct Bilan {
			uint32 ok = 0;
			uint32 total = 0;
	};

	inline void Essai(Bilan &b, const char *id, bool cond, const char *quoi) {
		++b.total;
		if (cond)
			++b.ok;
		printf("  [%s] %-6s %s\n", cond ? " ok " : "FAIL", id, quoi);
	}

	inline float32 Mesure(void *, NkAiPolice pol, const char *t) {
		if (!t)
			return 0.f;
		uint32 n = 0;
		while (t[n] != '\0')
			++n;
		const float32 pas = (pol == NkAiPolice::Grasse)		  ? 7.6f
							: (pol == NkAiPolice::ChasseFixe) ? 7.2f
															  : 7.0f;
		return (float32)n * pas;
	}

	/// Le fil de travail des essais : une demande, une etape d'outil depliee.
	inline void Batir(NkAiFil &fil) {
		NkAiCapacites cap = NkAiCapacites::Texte();
		cap.produitOutil = true;
		fil.Declarer(cap);
		NkString pq;
		NkAiBlocDonnees dem;
		dem.type = NkAiBloc::Demande;
		dem.texte = NkString("brancher claude comme on la branche a nkcode");
		(void)fil.Pousser(dem, pq);
		NkAiBlocDonnees outil;
		outil.type = NkAiBloc::Outil;
		outil.titre = NkString("Bash");
		outil.texte = NkString("Deposer une troisieme variante sans recompiler");
		outil.entree = NkString("cd D:/Projets && md5sum Build/Bin/NKSculptHarness");
		outil.sortie = NkString("depose.");
		outil.replie = false;
		(void)fil.Pousser(outil, pq);
	}

	/// Une piece va-t-elle produire une commande ? Les fonds et la puce toujours ;
	/// une piece de texte seulement si elle a du texte.
	inline bool PieceSePeint(const NkAiFil &fil, const NkAiRectPublie &r) {
		switch (r.piece) {
			case NkAiPiece::Cadre:
			case NkAiPiece::FondIn:
			case NkAiPiece::FondOut:
			case NkAiPiece::Estompe:
			case NkAiPiece::Puce:
				return true;
			default: {
				const char *s = aipaint::TextePiece(fil, r.blocId, r.piece);
				return s && s[0] != '\0';
			}
		}
	}

	inline bool Pres(float32 a, float32 b) {
		const float32 d = a - b;
		return d < 0.01f && d > -0.01f;
	}

	inline Bilan Sonder() {
		Bilan b;
		const NkAiMetriques M;
		const float32 W = 695.f;

		NkAiFil fil;
		Batir(fil);
		NkAiPlan plan;
		NkAiFilMesurer(fil, W, M, Mesure, nullptr, plan);

		NkRecordingPaint rec;
		NkAiFilPeindre(rec, fil, plan, 0.f, 0.f);

		// 23a — AUTANT DE COMMANDES QUE DE PIECES QUI DOIVENT SE PEINDRE. Ni plus
		// (une commande de trop est un rectangle que le plan n'a pas prevu, donc
		// invisible a la sonde du plan), ni moins.
		{
			uint32 attendues = 0;
			for (uint32 i = 0; i < plan.Pieces(); ++i)
				if (PieceSePeint(fil, plan.Piece(i)))
					++attendues;
			Essai(b, "23a", (uint32)rec.cmds.Size() == attendues,
				  "une commande par piece qui doit se peindre -- ni plus, ni moins");
			printf("         plan : %u pieces | peintes : %u | commandes : %u\n", plan.Pieces(),
				   attendues, (uint32)rec.cmds.Size());
		}

		// 23b — LA GEOMETRIE PEINTE EST EXACTEMENT CELLE DU PLAN. C'est l'essai
		// qui interdit a la transcription de recalculer quoi que ce soit : le
		// moindre ajustement, meme d'un pixel, rougit ici.
		{
			bool identique = true;
			uint32 c = 0;
			for (uint32 i = 0; i < plan.Pieces() && identique; ++i) {
				const NkAiRectPublie &r = plan.Piece(i);
				if (!PieceSePeint(fil, r))
					continue;
				if (c >= (uint32)rec.cmds.Size()) {
					identique = false;
					break;
				}
				const NkPaintCmd &k = rec.cmds[c++];
				if (!Pres(k.x, r.x) || !Pres(k.y, r.y) || !Pres(k.w, r.w) || !Pres(k.h, r.h))
					identique = false;
			}
			Essai(b, "23b", identique && c > 0,
				  "chaque commande porte EXACTEMENT le rectangle publie par le plan");
		}

		// 23c — LA COULEUR SUIT LE ROLE, ET LES DEUX FONDS NE SE CONFONDENT PAS.
		// L'enregistreur est injectif : un fond peint avec le mauvais role se voit.
		// C'est par ce chemin -- et lui seul -- que les trois surfaces posees le
		// 20/09 suivent le theme clair.
		{
			NkAiRectPublie in, out;
			uint32 idOutil = 0;
			for (uint32 i = 0; i < fil.Taille(); ++i)
				if (fil.At(i).type == NkAiBloc::Outil)
					idOutil = fil.At(i).id;
			// Controle de depart : les deux pieces existent bien dans le plan.
			const bool trouve = plan.Trouver(idOutil, NkAiPiece::FondIn, in) &&
								plan.Trouver(idOutil, NkAiPiece::FondOut, out);
			// ⚠️ COMPTE PAR ROLE, SANS REGARDER LA POSITION -- ET C'EST UNE
			//    CORRECTION DE CE TEMOIN, TROUVEE PAR UNE MUTATION. Ma premiere
			//    version appariait le remplissage a sa piece PAR SON `y`. La
			//    mutation F (la transcription ajoute 1 px) l'a fait rougir alors
			//    que les ROLES etaient justes : il ne savait pas distinguer
			//    « mauvais role » de « mauvaise position ». Un essai qui rougit
			//    pour la faute du voisin n'est pas un controle, c'est un doublon
			//    de 23b. Decouple, il ne rougit plus que pour SA faute a lui.
			uint32 nIn = 0, nOut = 0;
			for (usize i = 0; i < rec.cmds.Size(); ++i) {
				const NkPaintCmd &k = rec.cmds[i];
				if (k.op != NkPaintOp::Fill)
					continue;
				if (k.role == (uint16)NkRole::CodeBg)
					++nIn;
				if (k.role == (uint16)NkRole::CodeOutBg)
					++nOut;
			}
			const bool bonIn = (nIn == 1), bonOut = (nOut == 1);
			Essai(b, "23c",
				  trouve && bonIn && bonOut &&
					  rec.ColorOf((uint16)NkRole::CodeBg) != rec.ColorOf((uint16)NkRole::CodeOutBg),
				  "l'entree est peinte en code_bg, la sortie en code_out_bg, et les deux different");
		}

		// 23d — UNE PIECE DE TEXTE SANS TEXTE NE PEINT RIEN. Peindre une chaine
		// vide poserait une commande que le banc compte et que l'ecran ne montre
		// pas : les deux mesures cesseraient de dire la meme chose.
		{
			NkAiFil f;
			NkAiCapacites cap = NkAiCapacites::Texte();
			cap.produitOutil = true;
			f.Declarer(cap);
			NkString p;
			NkAiBlocDonnees o;
			o.type = NkAiBloc::Outil;
			o.titre = NkString("Bash");
			o.texte = NkString(""); // la phrase courte est VIDE
			o.entree = NkString("x");
			o.replie = false;
			(void)f.Pousser(o, p);
			NkAiPlan pl;
			NkAiFilMesurer(f, W, M, Mesure, nullptr, pl);
			NkRecordingPaint r2;
			NkAiFilPeindre(r2, f, pl, 0.f, 0.f);
			bool texteVide = false;
			for (usize i = 0; i < r2.cmds.Size(); ++i)
				if (r2.cmds[i].op == NkPaintOp::Text) {
					const char *s = r2.cmds[i].text.Data();
					if (!s || s[0] == '\0')
						texteVide = true;
				}
			Essai(b, "23d", !texteVide,
				  "aucune commande de texte VIDE : le banc et l'ecran comptent pareil");
		}

		// 23e — LES GOUTTIERES DISENT « IN » ET « OUT ». C'est ce que la capture
		// montre, et c'est le seul texte du fil que le porteur ne fournit pas.
		{
			bool vuIn = false, vuOut = false;
			for (usize i = 0; i < rec.cmds.Size(); ++i) {
				if (rec.cmds[i].op != NkPaintOp::Text)
					continue;
				const char *s = rec.cmds[i].text.Data();
				if (!s)
					continue;
				if (s[0] == 'I' && s[1] == 'N' && s[2] == '\0')
					vuIn = true;
				if (s[0] == 'O' && s[1] == 'U' && s[2] == 'T' && s[3] == '\0')
					vuOut = true;
			}
			Essai(b, "23e", vuIn && vuOut, "les gouttieres portent bien « IN » et « OUT »");
		}

		// 23f — TRANSLATION NULLE = IDENTITE. Le controle de depart de 23g : sans
		// lui, une transcription qui ignorerait l'origine passerait 23g.
		{
			NkRecordingPaint z;
			NkAiFilPeindre(z, fil, plan, 0.f, 0.f);
			bool pareil = z.cmds.Size() == rec.cmds.Size();
			for (usize i = 0; pareil && i < z.cmds.Size(); ++i)
				pareil = z.cmds[i].SameAs(rec.cmds[i]);
			Essai(b, "23f", pareil, "translation nulle : le flux est identique");
		}

		// 23g — TRANSLATION (10, 20) : TOUT se decale d'exactement (10, 20), et
		// RIEN d'autre ne change. C'est la seule arithmetique que la transcription
		// a le droit de faire, et on verifie qu'elle ne fait que celle-la.
		{
			NkRecordingPaint t;
			NkAiFilPeindre(t, fil, plan, 10.f, 20.f);
			bool bon = t.cmds.Size() == rec.cmds.Size();
			for (usize i = 0; bon && i < t.cmds.Size(); ++i) {
				const NkPaintCmd &a = rec.cmds[i], &c = t.cmds[i];
				bon = a.op == c.op && a.role == c.role && a.role2 == c.role2 &&
					  Pres(c.x, a.x + 10.f) && Pres(c.y, a.y + 20.f) && Pres(c.w, a.w) &&
					  Pres(c.h, a.h);
			}
			Essai(b, "23g", bon,
				  "translation (10,20) : tout se decale d'exactement ca, largeurs intactes");
		}

		// 23h — LA TRANSCRIPTION EST PURE. Elle ne lit ni la souris ni l'horloge :
		// deux appels rendent le meme flux. Le depot a deja paye des sondes qui
		// rougissaient parce que Rodolf avait bouge sa souris.
		{
			NkRecordingPaint a, c;
			NkAiFilPeindre(a, fil, plan, 3.f, 7.f);
			NkAiFilPeindre(c, fil, plan, 3.f, 7.f);
			bool pareil = a.cmds.Size() == c.cmds.Size() && a.cmds.Size() > 0;
			for (usize i = 0; pareil && i < a.cmds.Size(); ++i)
				pareil = a.cmds[i].SameAs(c.cmds[i]);
			Essai(b, "23h", pareil, "deux transcriptions des memes entrees rendent le meme flux");
		}

		// 23i — LE CADRE DE LA DEMANDE EST UN CONTOUR, PAS UN APLAT. `Outline`
		// prend DEUX roles -- bordure et interieur -- et l'en-tete du peintre
		// raconte deux sites de NkUIDesign qui lui passaient 0 en croyant ecrire
		// « pas d'interieur ». On verifie que les deux sont nommes.
		{
			bool bon = false;
			for (usize i = 0; i < rec.cmds.Size(); ++i) {
				const NkPaintCmd &k = rec.cmds[i];
				if (k.op == NkPaintOp::Outline && k.role == (uint16)NkRole::Border &&
					k.role2 == (uint16)NkRole::PanelBg)
					bon = true;
			}
			Essai(b, "23i", bon, "le cadre de la demande nomme SES DEUX roles (bordure, interieur)");
		}

		return b;
	}

} // namespace aipaintprobe
