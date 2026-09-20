// =============================================================================
// Applications/NKAiFilTest/src/main.cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// POURQUOI CE PROGRAMME EXISTE
// -----------------------------------------------------------------------------
// Le fil du panneau IA de NK3DModeler designe ses blocs PAR LEUR POSITION :
//
//     snprintf(cle, sizeof(cle), "ai.b%d", (int)i);   // NkModelerAiPanel.h
//
// et `NkAiPousser` est une FENETRE GLISSANTE -- « on perd le plus ancien, jamais
// le plus recent ». Des qu'un bloc entre dans un fil plein, `aiFil[i-1] =
// aiFil[i]` decale TOUT, et la cle `ai.b10` ne designe plus le meme bloc.
//
// CE QUE RODOLF PEUT VIVRE AUJOURD'HUI : il deplie un bloc, une reponse arrive,
// un AUTRE bloc se deplie. Rien ne le dit. Il croira avoir mal clique.
//
// ⚠️ ET LE DECALAGE ETAIT DEJA CONNU -- A UN SEUL SITE. `NkAiPousser` porte
//    ceci, en propre :
//      « LE BLOC EN COURS DE MESURE GLISSE AVEC LE FIL. Sans ce decalage,
//        l'effet mesure irait se poser sur le bloc du voisin : un chiffre juste,
//        ecrit sur la mauvaise ligne. »
//    `st.aiEnCours` a donc ete corrige. La cle du registre de clics, non. C'est
//    *le meme calcul a deux sites, garde a un seul* -- et le site oublie est
//    celui que l'utilisateur touche avec sa souris.
//
// CE QUE CE BANC MESURE, ET COMMENT
// -----------------------------------------------------------------------------
// Il n'INJECTE RIEN et n'ouvre AUCUNE fenetre. Il appelle le VRAI producteur,
// `NkAiPousser`, sur un VRAI `NkModelerState`, et pose une seule question :
//
//     apres qu'un bloc soit entre dans un fil plein, l'indice k designe-t-il
//     toujours le meme bloc qu'avant ?
//
// ⚠️ IL NE RECONSTRUIT PAS LA FORMULE DE LA CLE. Une sonde qui refabriquerait
//    `"ai.b%d"` de son cote mesurerait SA copie du defaut. Ce qui est interroge
//    est l'IDENTITE DU BLOC A L'INDICE k -- c'est-a-dire exactement ce que la
//    cle resout. Le piege n.4 de la grille du corpus (« un banc monte sur les
//    memes primitives les recable correctement par construction ») est evite
//    parce que le glissement vient de `NkAiPousser` et de rien d'autre.
//
// LE MEME BANC PORTE LE VERDICT D'APRES. `NkAiFil` (NKEditorKit) subit le MEME
// scenario, et lui designe par identifiant stable. Rouge d'un cote, vert de
// l'autre, dans le meme processus : c'est ce qui distingue « j'ai corrige »
// de « j'ai deplace du code ».
//
// PRECEDENT DE LA MAISON : `NKFoldTest`, 14/09 -- 27 groupes de proprietes pour
// 16 bits distincts, six bits partages par 17 groupes. Meme famille de defaut
// (une identite portee par une position), meme remede (designer par une cle), et
// meme forme de banc : un programme console par defaut mesure.
// =============================================================================

#include "NK3DModeler/Shell/NkModelerAiPanel.h"
#include "NKEditorKit/NkAiThread.h"
#include <stdio.h>

using namespace nkentseu;
using namespace nkentseu::editorkit;
using namespace nkentseu::nk3d; // NkModelerState, NkAiPousser

static uint32 gOk = 0, gTotal = 0;

static void Essai(const char *id, bool cond, const char *quoi) {
	++gTotal;
	if (cond)
		++gOk;
	printf("  [%s] %-6s %s\n", cond ? " ok " : "FAIL", id, quoi);
}

/// Les deux fils portent-ils la meme ligne au meme endroit logique ?
static bool MemeLigne(const char *a, const char *b) {
	if (!a || !b)
		return a == b;
	for (; *a && *b; ++a, ++b)
		if (*a != *b)
			return false;
	return *a == *b;
}

int main() {
	printf("=== BANC NKAiFilTest — designer un bloc du fil IA ===\n\n");

	// ─────────────────────────────────────────────────────────────────────────
	//  A. LE FIL DE NK3DModeler, TEL QU'IL EST AUJOURD'HUI
	// ─────────────────────────────────────────────────────────────────────────
	printf("[A] le fil du modeleur, designe par la POSITION\n");
	{
		// ⚠️ SUR LE TAS. `NkModelerState` porte seize blocs de plusieurs centaines
		//    d'octets chacun, plus tout le reste de l'etat du modeleur : le depot
		//    a deja paye « NkWorld ne vit pas sur la pile, 791 640 octets ».
		NkModelerState *pst = new NkModelerState();
		NkModelerState &st = *pst;

		// On remplit la fenetre EXACTEMENT, par le vrai producteur.
		for (int32 i = 0; i < NkModelerState::kAiFil; ++i) {
			char l[64];
			snprintf(l, sizeof(l), "etape %d", (int)i);
			(void)nk3d::NkAiPousser(st, NkModelerState::AiType::Note, l);
		}
		Essai("A1", st.aiFilN == NkModelerState::kAiFil,
			  "controle de depart : la fenetre est pleine (16 blocs)");

		// L'utilisateur deplie le bloc de l'indice 10. C'est ce que fait le
		// panneau : `if (hit.Clicked(cle)) b.replie = ...` sur `aiFil[i]`.
		const int32 k = 10;
		st.aiFil[k].replie = 0u;
		char ligneVisee[96];
		snprintf(ligneVisee, sizeof(ligneVisee), "%s", st.aiFil[k].ligne);
		Essai("A2", MemeLigne(ligneVisee, "etape 10"),
			  "controle de depart : l'indice 10 designe bien « etape 10 », et il est deplie");

		// LA REPONSE ARRIVE. Un bloc entre dans un fil plein : tout glisse.
		(void)nk3d::NkAiPousser(st, NkModelerState::AiType::Note, "la reponse");

		// ⚠️ LE CRITERE. Il ne regarde PAS la cle : il regarde ce que la cle
		//    resout, c'est-a-dire le bloc a l'indice 10.
		const bool memeBloc = MemeLigne(st.aiFil[k].ligne, ligneVisee);
		Essai("A3", memeBloc,
			  "apres un bloc de plus, l'indice 10 designe TOUJOURS « etape 10 »");
		printf("         indice %d : avant « %s » | apres « %s »\n", (int)k, ligneVisee,
			   st.aiFil[k].ligne);

		// Et la consequence visible pour Rodolf : ce n'est plus SON bloc qui est
		// deplie. Le sien l'est encore, mais ailleurs ; celui que sa souris
		// designe, non.
		const bool bonDeplie = (st.aiFil[k].replie == 0u);
		Essai("A4", bonDeplie,
			  "et le bloc que la souris designe est toujours celui qui est deplie");

		// ⚠️ LE SITE QUI, LUI, A ETE CORRIGE. `NkAiPousser` decale `aiEnCours`
		//    avec le fil, et son commentaire dit pourquoi. On le mesure pour
		//    montrer que le defaut n'est pas « personne n'y avait pense » mais
		//    « on y a pense A UN SEUL DES DEUX SITES ».
		{
			NkModelerState *p2 = new NkModelerState();
			NkModelerState &s2 = *p2;
			for (int32 i = 0; i < NkModelerState::kAiFil; ++i)
				(void)nk3d::NkAiPousser(s2, NkModelerState::AiType::Note, "x");
			s2.aiEnCours = 10;
			(void)nk3d::NkAiPousser(s2, NkModelerState::AiType::Note, "la reponse");
			Essai("A5", s2.aiEnCours == 9,
				  "le bloc EN COURS DE MESURE, lui, suit le glissement (deja corrige)");
			delete p2;
		}

		delete pst;
	}

	// ─────────────────────────────────────────────────────────────────────────
	//  B. LE MEME SCENARIO SUR `NkAiFil` — LE FIL DU KIT
	// ─────────────────────────────────────────────────────────────────────────
	printf("\n[B] le fil du kit, designe par l'IDENTIFIANT\n");
	{
		NkAiFil fil;
		fil.Declarer(NkAiCapacites::Texte());
		fil.PoserPlafond(16);
		NkString pq;
		for (int32 i = 0; i < 16; ++i) {
			char l[64];
			snprintf(l, sizeof(l), "etape %d", (int)i);
			NkAiBlocDonnees b;
			b.type = NkAiBloc::Prose;
			b.texte = NkString(l);
			(void)fil.Pousser(b, pq);
		}
		Essai("B1", fil.Taille() == 16, "controle de depart : la fenetre est pleine");

		// L'utilisateur deplie le bloc de l'indice 10 -- mais ce qu'on RETIENT
		// est son identifiant, pas sa position.
		const uint32 idVise = fil.At(10).id;
		fil.BasculerParId(idVise);
		uint32 idx = 0;
		const bool trouveAvant = fil.TrouverParId(idVise, idx);
		Essai("B2", trouveAvant && idx == 10 && !fil.At(idx).replie,
			  "controle de depart : l'identifiant vise l'indice 10, et il est deplie");

		// LA MEME REPONSE ARRIVE.
		{
			NkAiBlocDonnees b;
			b.type = NkAiBloc::Prose;
			b.texte = NkString("la reponse");
			(void)fil.Pousser(b, pq);
		}

		uint32 idx2 = 0;
		const bool trouveApres = fil.TrouverParId(idVise, idx2);
		Essai("B3", trouveApres && MemeLigne(fil.At(idx2).texte.CStr(), "etape 10"),
			  "apres un bloc de plus, l'identifiant designe TOUJOURS « etape 10 »");
		printf("         id %u : indice %u avant, %u apres -- meme bloc\n", idVise, idx, idx2);

		Essai("B4", trouveApres && !fil.At(idx2).replie,
			  "et c'est toujours CE bloc-la qui est deplie");

		// ⚠️ CONTROLE NEGATIF. Sans lui, un `TrouverParId` qui rendrait toujours
		//    le premier bloc passerait B3 et B4 sans rien prouver.
		const uint32 idAutre = fil.At(0).id;
		uint32 idxAutre = 0;
		const bool autre = fil.TrouverParId(idAutre, idxAutre);
		Essai("B5", autre && idxAutre == 0 && idAutre != idVise,
			  "controle negatif : deux identifiants distincts ne designent pas le meme bloc");
	}

	printf("\n---------------------------------------------\n");
	printf("RESULTAT : %u/%u\n", gOk, gOk + (gTotal - gOk));
	if (gOk != gTotal) {
		printf("%u ESSAI(S) EN ECHEC.\n", gTotal - gOk);
		return 1;
	}
	printf("Tout est vert.\n");
	return 0;
}
