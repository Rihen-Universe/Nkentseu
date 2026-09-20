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
/// La mesure de texte du banc : deterministe, comme dans la sonde du kit.
static float32 MesureBanc(void *, editorkit::NkAiPolice, const char *t) {
	uint32 n = 0;
	while (t && t[n]) ++n;
	return (float32)n * 7.f;
}

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
	printf("[A] le fil du modeleur, MIGRE -- designe par l'IDENTIFIANT\n");
	{
		// ⚠️ CETTE FAMILLE A CHANGE AVEC LA MIGRATION, ET C'EST VOULU.
		//    Sa version d'AVANT (commit 116cd79c5) interrogeait `st.aiFil[k]` et
		//    sortait ROUGE : « indice 10 : avant etape 10 | apres etape 11 ».
		//    Le stockage a change, donc le banc ne peut plus poser la meme
		//    question dans les memes termes -- la preuve du defaut vit dans
		//    l'histoire, pas dans un banc qu'on garderait rouge.
		//
		//    Ce qu'elle mesure MAINTENANT : les VRAIS producteurs du modeleur
		//    (`NkAiPousser`, `NkAiTour`, `NkAiEffet`) sur un VRAI etat. La
		//    famille B eprouve le kit seul ; celle-ci eprouve le chemin que
		//    Rodolf emprunte.
		NkModelerState *pst = new NkModelerState();
		NkModelerState &st = *pst;
		for (int32 i = 0; i < 16; ++i) {
			char l[64];
			snprintf(l, sizeof(l), "etape %d", (int)i);
			(void)nk3d::NkAiPousser(st, NkModelerState::AiType::Note, l);
		}
		Essai("A1", st.aiFil.Taille() == 16, "controle de depart : 16 blocs dans le fil");
		const uint32 idVise = st.aiFil.At(10).id;
		st.aiFil.BasculerParId(idVise);
		uint32 idx = 0;
		Essai("A2", st.aiFil.TrouverParId(idVise, idx) && idx == 10 &&
				  !st.aiFil.At(idx).replie,
			  "controle de depart : l'identifiant vise l'indice 10, et il est deplie");
		
		// LA REPONSE ARRIVE : un bloc de plus dans un fil plein.
		(void)nk3d::NkAiPousser(st, NkModelerState::AiType::Note, "la reponse");
		uint32 idx2 = 0;
		const bool trouve = st.aiFil.TrouverParId(idVise, idx2);
		Essai("A3", trouve && MemeLigne(st.aiFil.At(idx2).texte.CStr(), "etape 10"),
			  "apres un bloc de plus, l'identifiant designe TOUJOURS « etape 10 »");
		printf("         id %u : indice %u avant, %u apres -- meme bloc\n", idVise, idx, idx2);
		Essai("A4", trouve && !st.aiFil.At(idx2).replie,
			  "et c'est toujours CE bloc-la qui est deplie");
		
		// ⚠️ A5 A CHANGE DE SENS, ET C'EST LE MEILLEUR RESULTAT DU LOT.
		//    Avant : « le bloc en cours de mesure suit le glissement » -- il
		//    mesurait un RATTRAPAGE (`if (aiEnCours > 0) --aiEnCours;`) ecrit a
		//    la main a un seul des deux sites qui en avaient besoin.
		//    Ces quatre lignes N'EXISTENT PLUS : un identifiant ne glisse pas.
		//    On ne propage pas une correction au site qui l'avait oubliee, on
		//    supprime la RAISON d'en avoir une.
		//    A5 mesure donc ce qui compte vraiment : l'effet atterrit-il sur LE
		//    BON bloc quand le fil a glisse entre la pose et la mesure ?
		{
			NkModelerState *p2 = new NkModelerState();
			NkModelerState &s2 = *p2;
			for (int32 i = 0; i < 16; ++i)
				(void)nk3d::NkAiPousser(s2, NkModelerState::AiType::Note, "remplissage");
			s2.aiMotifEstRefus = false;
			nk3d::NkAiTour(s2, "subdivide:2", 8, 12, 6, 1); // pose l'operation, compteurs d'avant
			const uint32 idOp = s2.aiEnCoursId;
			// Le fil GLISSE entre la pose et la mesure.
			(void)nk3d::NkAiPousser(s2, NkModelerState::AiType::Note, "un bloc qui s'intercale");
			nk3d::NkAiEffet(s2, 386, 768, 384);
			const editorkit::NkAiBlocDonnees *b = s2.aiFil.MutableParId(idOp);
			const bool surLeBon = b && MemeLigne(b->titre.CStr(), "subdivide:2") &&
				  b->sortie.Length() > 0;
			Essai("A5", surLeBon,
				  "l'effet mesure atterrit sur LE BON bloc, meme si le fil a glisse entre-temps");
			if (b) printf("         %s -> %s\n", b->titre.CStr(), b->sortie.CStr());
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

	printf("\n[C] l historique des conversations\n");
	{
		// ⚠️ CETTE FAMILLE EXISTE PARCE QU UN BOUTON MORT LA PROMETTAIT.
		//    `ai.hist` etait dessine, son survol LU pour colorer l icone, et
		//    `Clicked("ai.hist")` n existait nulle part.
		//    Critere qui distingue un survol qui MENT d une emprise honnete :
		//    `ai.box` fait `(void)hit.Add(...)` -- il ne LIT pas son survol, donc
		//    il ne promet rien. `ai.hist` lisait le sien.
		NkModelerState *pst = new NkModelerState();
		NkModelerState &st = *pst;
		nk3d::NkAiCopie(st.aiSujet, sizeof(st.aiSujet), "premier sujet");
		(void)nk3d::NkAiPousser(st, NkModelerState::AiType::Note, "un echange");
		nk3d::NkAiArchiver(st);
		st.aiFil.Vider();
		Essai("C1", st.aiArchivesN == 1 && st.aiFil.Taille() == 0,
			  "archiver met la conversation de cote et laisse le fil vide");
		
		// ⚠️ ON N ARCHIVE PAS DU VIDE : sinon la liste se remplit de lignes
		//    sans contenu des qu on clique deux fois sur « nouvelle ».
		nk3d::NkAiArchiver(st);
		Essai("C2", st.aiArchivesN == 1,
			  "controle negatif : un fil vide ne cree PAS d archive");
		
		// Une seconde conversation, puis on rouvre la premiere.
		nk3d::NkAiCopie(st.aiSujet, sizeof(st.aiSujet), "second sujet");
		(void)nk3d::NkAiPousser(st, NkModelerState::AiType::Note, "autre echange");
		nk3d::NkAiRouvrir(st, 0);
		Essai("C3", MemeLigne(st.aiSujet, "premier sujet") && st.aiFil.Taille() == 1,
			  "rouvrir restaure le sujet ET le fil de l archive");
		// ⚠️ ET LA COURANTE N EST PAS PERDUE : rouvrir ne doit jamais jeter ce
		//    qui etait a l ecran.
		Essai("C4", st.aiArchivesN == 1 && MemeLigne(st.aiArchivesSujet[0], "second sujet"),
			  "la conversation courante est archivee a la place de celle qu on rouvre");
		
		nk3d::NkAiViderHistorique(st);
		Essai("C5", st.aiArchivesN == 0 && st.aiFil.Taille() == 1,
			  "vider l historique n efface QUE l historique, pas la conversation ouverte");
		delete pst;
	}

	printf("\n[D] de la phrase tapee au bloc affiche -- sans fenetre\n");
	{
		// ⚠️ LE CRITERE DE LA NUIT : « une phrase tapee ressort en reponse
		//    AFFICHEE ». Pas « ca compile », pas « le pont repond ». Ce banc suit
		//    la chaine ENTIERE, par les vraies fonctions, et finit sur le PLAN --
		//    c est-a-dire sur ce que le peintre publie, pas sur un etat interne.
		//
		//    Maillons, dans l ordre : NkAiSoumettre (le point d entree UNIQUE, la
		//    ou `NK_AI_DEMANDE` entre) -> NkAiTour (la table a repondu) ->
		//    NkAiEffet (les compteurs de l hote) -> NkAiFilMesurer (le plan).
		NkModelerState *pst = new NkModelerState();
		NkModelerState &st = *pst;
		st.welcome = false; // en projet : sans ca l assistant REFUSE, et il a raison
		
		// 1. L utilisateur tape et soumet.
		const bool soumis = nk3d::NkAiSoumettre(st, "subdivise le cube deux fois");
		Essai("D1", soumis && st.aiFil.Taille() == 1 &&
			  st.aiFil.At(0).type == editorkit::NkAiBloc::Demande,
			  "la phrase soumise entre dans le fil comme DEMANDE");
		
		// 2. La boucle execute et note le tour. `dem` porte le verbe du contrat :
		//    la traduction a eu lieu en amont, comme dans main.cpp.
		st.aiMotifEstRefus = false;
		nk3d::NkAiTour(st, "subdivide:2", 8, 12, 6, 1);
		Essai("D2", st.aiFil.Taille() == 2 &&
			  st.aiFil.At(1).type == editorkit::NkAiBloc::Outil &&
			  st.aiEnCoursId != 0u,
			  "le tour pose un bloc OUTIL et retient son identifiant a mesurer");
		
		// 3. L effet, lu dans les compteurs de l hote a l image suivante.
		nk3d::NkAiEffet(st, 386, 768, 384);
		const editorkit::NkAiBlocDonnees &b = st.aiFil.At(1);
		Essai("D3", b.effet.Length() > 0 && b.sortie.Length() > 0 && st.aiEnCoursId == 0u,
			  "l effet mesure atterrit dans le bloc, et la mesure est refermee");
		printf("         effet : %s | sortie : %s\n", b.effet.CStr(), b.sortie.CStr());
		
		// 4. ⚠️ LE MAILLON QUI MANQUAIT : le bloc est-il PEINT ? On mesure le
		//    PLAN -- ce que le peintre publie -- et pas un drapeau interne.
		editorkit::NkAiMetriques metr;
		editorkit::NkAiPlan plan;
		editorkit::NkAiFilMesurer(st.aiFil, 380.f, metr, MesureBanc, nullptr, plan);
		const uint32 idDem = st.aiFil.At(0).id;
		const uint32 idOp = st.aiFil.At(1).id;
		editorkit::NkAiRectPublie rd, rt, re;
		const bool vuDem = plan.Trouver(idDem, editorkit::NkAiPiece::Texte, rd);
		const bool vuOp = plan.Trouver(idOp, editorkit::NkAiPiece::Titre, rt);
		const bool vuEff = plan.Trouver(idOp, editorkit::NkAiPiece::Effet, re);
		Essai("D4", vuDem && vuOp && rd.w > 0.f && rt.w > 0.f,
			  "la DEMANDE et la REPONSE sont toutes deux publiees par le peintre");
		Essai("D5", vuEff && re.w > 0.f && re.y == rt.y,
			  "et l effet mesure est publie SUR LA LIGNE de la reponse");
		printf("         demande a y=%.0f | reponse a y=%.0f | effet a y=%.0f\n",
			   (double)rd.y, (double)rt.y, (double)re.y);
		
		// 6. ⚠️ CONTROLE NEGATIF : un REFUS ne fabrique pas de reponse. Sans lui,
		//    un banc qui verdirait sur n importe quelle soumission passerait D1-D5.
		const uint32 avant = st.aiFil.Taille();
		nk3d::NkAiCopie(st.aiMotif, sizeof(st.aiMotif), "verbe inconnu du contrat");
		st.aiMotifEstRefus = true;
		nk3d::NkAiTour(st, "fais un cafe", 386, 768, 384, 2);
		const editorkit::NkAiBlocDonnees &r2 = st.aiFil.At(st.aiFil.Taille() - 1);
		Essai("D6", st.aiFil.Taille() == avant + 1 &&
			  r2.type == editorkit::NkAiBloc::Refus && r2.motif.Length() > 0 &&
			  st.aiEnCoursId == 0u,
			  "controle negatif : un verbe inconnu donne un REFUS avec motif, rien a mesurer");
		delete pst;
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
