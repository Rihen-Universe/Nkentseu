#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    CasFocusPanneau.h
// @Brief   (b24) LE FOCUS DE PANNEAU : qui a le clavier, et ce qui le deplace.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE ÇA MESURE
// =============================================================================
//  Rodolf, 10/10 : « on doit implementer ce systeme de focus de panneau, c'est
//  important ». NKGui ne savait pas quel panneau a le clavier ; le tableau blanc
//  de NKCode le devinait au survol, et un J tape dans le chat faisait une union.
//
//  Deux panneaux A et B cote a cote, une barre de menus au-dessus. Le pointeur
//  est POSE dans `ctx.input` (comme le reste de ce banc) ; aucune fenetre.
//
//   (f1) clic dans A, puis dans B : B a le focus, A ne l'a plus ;
//   (f2) un clic dans le vide (entre les panneaux) le retire a tous ;
//   (f3) clic droit dans A : il le donne aussi ;
//   (f4) 🔴 UN MENU AU-DESSUS DE A : le clic sur son titre, puis sur son entree
//        posee SUR A, ne donne pas le focus a A et ne le retire pas a B ;
//        et tant que le menu est ouvert, B n'a pas le clavier ;
//   (f5) une surface declaree a la couche 50 sur A, une modale (`appModal`), la
//        barre de titre de l'hote (`titleBarH`) : meme regle ;
//   (f6) 🔴 LA FRAPPE : un champ de NKGui focalise hors de tout panneau, un champ
//        du kit en frappe dans B, un champ en frappe hors panneau -> A a le focus
//        mais PAS le clavier ; un champ en frappe DANS A -> A garde le clavier.
//
//  CONTRE-EPREUVES, une par regle, DANS LE MEME PROCESSUS (NK_FOCUS_MUTATION est
//  relue a chaque clic) : chaque mutation doit faire rougir SON critere --
//  collant (f1), garde (f2), popup (f4), surcouche (f4, f5), frappe (f6),
//  proprietaire (f6).
// -----------------------------------------------------------------------------

/// Deux panneaux, une barre de menus, des champs : une interface minimale vivante.
/// ⚠️ ALLOUEE SUR LE TAS : un `NkGuiContext` pese des dizaines de Ko, et ce banc a
///    deja deborde sa pile une fois (voir l'avertissement de `main`).
struct SceneFocus {
		NkGuiContext ctx;
		NkGuiId idA = NKGUI_ID_NONE, idB = NKGUI_ID_NONE;
		NkRect rA{0.f, 30.f, 190.f, 270.f};
		NkRect rB{210.f, 30.f, 190.f, 270.f};
		bool barre = true;			 ///< dessiner la barre de menus
		bool champDansA = false;	 ///< un champ NKGui (InputText) dans A
		bool frappeKitA = false;	 ///< un champ du kit en frappe dans A (NoterFrappe)
		bool frappeKitB = false;	 ///< ... dans B
		bool frappeKitHors = false;	 ///< ... hors de tout panneau (une palette)
		bool appModalDeclare = false; ///< une modale declaree par l'hote a cette image
		bool surface50 = false;		 ///< une surface de couche 50 declaree sur A
		NkRect rSurface{20.f, 200.f, 120.f, 60.f};
		char champA[64] = {};
		// Ce que les panneaux ont lu PENDANT leur dessin (la question, la ou
		// l'application la pose), et ce que la barre a dessine.
		bool clavierA = false, clavierB = false;
		NkRect itemMenu{0.f, 0.f, 0.f, 0.f};
		NkRect champARect{0.f, 0.f, 0.f, 0.f};
		bool menuOuvert = false;

		void Init() {
			ctx.Init(400, 300);
			if (g_fontOk)
				ctx.font = &g_font;
			idA = ctx.GetId("panneau.A");
			idB = ctx.GetId("panneau.B");
		}

		void Image() {
			ctx.BeginFrame(0.016f);
			ctx.appModal = false; // comme la coquille : une declaration PAR image
			ctx.BeginLayout({0.f, 0.f, 400.f, 300.f});
			if (barre && BeginMenuBar(ctx, {0.f, 0.f, 400.f, 24.f})) {
				if (BeginMenu(ctx, "Fichier")) {
					(void)MenuItem(ctx, "Ouvrir");
					itemMenu = ctx.menuDernierItem;
					EndMenu(ctx);
				}
				EndMenuBar(ctx);
			}
			menuOuvert = ctx.popupDepth > 0;
			if (surface50) {
				NkGuiContext::NkInputLayerScope couche(ctx, 50);
				ctx.PushOcclusion(rSurface, 50);
			}
			if (frappeKitHors)
				ctx.NoterFrappe();
			ctx.PanneauDebut(idA, rA);
			clavierA = ctx.PanneauAuClavier(idA);
			if (champDansA) {
				champARect = {rA.x + 10.f, rA.y + 40.f, 150.f, 22.f};
				ctx.SetNextItemRect(champARect);
				(void)InputText(ctx, "##champA", champA, (int32)sizeof(champA));
			}
			if (frappeKitA)
				ctx.NoterFrappe();
			ctx.PanneauFin();
			ctx.PanneauDebut(idB, rB);
			clavierB = ctx.PanneauAuClavier(idB);
			if (frappeKitB)
				ctx.NoterFrappe();
			ctx.PanneauFin();
			if (appModalDeclare)
				ctx.appModal = true;
			ctx.EndFrame();
		}

		/// Appui sur une image, relache a la suivante (le focus se resout a la fin
		/// de l'image de l'APPUI).
		void Clic(float32 x, float32 y, int32 bouton = 0) {
			ctx.input.mousePos = {x, y};
			ctx.input.mouseDown[bouton] = true;
			Image();
			ctx.input.mouseDown[bouton] = false;
			Image();
		}
		/// Seulement l'appui (pour lire le focus a la fin de cette image-la).
		void Appui(float32 x, float32 y) {
			ctx.input.mousePos = {x, y};
			ctx.input.mouseDown[0] = true;
			Image();
		}
		void Relache() {
			ctx.input.mouseDown[0] = false;
			Image();
		}
		float32 cxA() const {
			return rA.x + rA.w * 0.5f;
		}
		float32 cxB() const {
			return rB.x + rB.w * 0.5f;
		}
		float32 cy() const {
			return rA.y + rA.h * 0.5f;
		}
};

/// La mutation posee DE L'EXTERIEUR (NK_FOCUS_MUTATION=<regle> au lancement) : les
/// criteres positifs tournent avec elle, et le banc entier doit alors ROUGIR. Les
/// contre-epreuves internes l'ecrasent le temps de leur scene, puis la remettent.
static char g_focusExterne[32] = {};
static void ArmerFocus(const char *mutation) {
	NkDefinirVariable("NK_FOCUS_MUTATION", mutation ? mutation : (g_focusExterne[0] ? g_focusExterne : nullptr));
}

static const char *NomFocus(const SceneFocus &s, NkGuiId f) {
	return f == s.idA ? "A" : f == s.idB ? "B" : f == NKGUI_ID_NONE ? "aucun" : "?";
}

// ── (f1) A puis B ─────────────────────────────────────────────────────────
struct ObsAB {
		NkGuiId apresA = NKGUI_ID_NONE, apresB = NKGUI_ID_NONE;
		bool clavierAApresA = false, clavierBApresA = false, clavierAApresB = false, clavierBApresB = false;
};
static ObsAB FocusObsAB() {
	ObsAB o;
	SceneFocus *s = new SceneFocus();
	s->Init();
	s->Image();
	s->Clic(s->cxA(), s->cy());
	o.apresA = s->ctx.panneauFocus;
	s->Image(); // la question posee PENDANT le dessin lit l'etat resolu a l'image d'avant
	o.clavierAApresA = s->clavierA;
	o.clavierBApresA = s->clavierB;
	s->Clic(s->cxB(), s->cy());
	o.apresB = s->ctx.panneauFocus;
	s->Image();
	o.clavierAApresB = s->clavierA;
	o.clavierBApresB = s->clavierB;
	delete s;
	return o;
}

// ── (f2) le vide ──────────────────────────────────────────────────────────
static NkGuiId FocusApresClicVide() {
	SceneFocus *s = new SceneFocus();
	s->Init();
	s->Image();
	s->Clic(s->cxA(), s->cy());
	s->Clic(200.f, s->cy()); // entre A (x < 190) et B (x >= 210)
	const NkGuiId f = s->ctx.panneauFocus;
	delete s;
	return f;
}

// ── (f3) clic droit ───────────────────────────────────────────────────────
static NkGuiId FocusApresClicDroit() {
	SceneFocus *s = new SceneFocus();
	s->Init();
	s->Image();
	s->Clic(s->cxB(), s->cy());
	s->Clic(s->cxA(), s->cy(), 1);
	const NkGuiId f = s->ctx.panneauFocus;
	delete s;
	return f;
}

// ── (f4) le menu au-dessus de A ───────────────────────────────────────────
struct ObsMenu {
		NkGuiId apresTitre = NKGUI_ID_NONE, apresItem = NKGUI_ID_NONE, apresRelache = NKGUI_ID_NONE;
		bool ouvert = false, itemSurA = false, clavierBMenuOuvert = true, clavierBMenuFerme = false;
		NkRect item{0.f, 0.f, 0.f, 0.f};
};
static ObsMenu FocusObsMenu() {
	ObsMenu o;
	SceneFocus *s = new SceneFocus();
	s->Init();
	s->Image();
	s->Clic(s->cxB(), s->cy()); // B a le focus
	s->Clic(20.f, 12.f);		// le titre « Fichier » : le menu s'ouvre
	o.apresTitre = s->ctx.panneauFocus;
	// ⚠️ PLUSIEURS IMAGES : un menu se MESURE une image et s'applique a la suivante.
	for (int32 i = 0; i < 3; ++i)
		s->Image();
	o.ouvert = s->menuOuvert;
	o.clavierBMenuOuvert = s->clavierB;
	o.item = s->itemMenu;
	const NkVec2 p{o.item.x + o.item.w * 0.5f, o.item.y + o.item.h * 0.5f};
	// LE CRITERE N'A DE SENS QUE SI L'ENTREE EST VRAIMENT SUR A : sinon « A n'a pas
	// pris le focus » serait vrai pour la mauvaise raison.
	o.itemSurA = o.item.w > 0.f && NkGuiRectContains(s->rA, p) && !NkGuiRectContains({0.f, 0.f, 400.f, 24.f}, p);
	// le pointeur SURVOLE l'entree d'abord (le survol se resout a l'image d'apres),
	// puis appuie : c'est ainsi qu'une main choisit une entree de menu
	s->ctx.input.mousePos = p;
	s->Image();
	s->Image();
	s->Appui(p.x, p.y);
	o.apresItem = s->ctx.panneauFocus;
	s->Relache(); // l'entree est validee au relache : le menu se referme
	o.apresRelache = s->ctx.panneauFocus;
	s->Image();
	s->Image();
	o.clavierBMenuFerme = s->clavierB && !s->menuOuvert;
	delete s;
	return o;
}

// ── (f5) surface de couche 50, modale, barre de titre ────────────────────
static NkGuiId FocusApresSurface50() {
	SceneFocus *s = new SceneFocus();
	s->Init();
	s->Image();
	s->Clic(s->cxB(), s->cy());
	s->surface50 = true;
	s->Image(); // la surface est lue a l'image SUIVANTE (comme l'occultation)
	s->Clic(s->rSurface.x + 30.f, s->rSurface.y + 30.f); // dans la surface, SUR A
	const NkGuiId f = s->ctx.panneauFocus;
	delete s;
	return f;
}
static NkGuiId FocusApresClicSousModale() {
	SceneFocus *s = new SceneFocus();
	s->Init();
	s->Image();
	s->Clic(s->cxA(), s->cy());
	s->appModalDeclare = true;
	s->Image();
	s->Clic(200.f, s->cy()); // dans le vide, mais une modale est ouverte
	const NkGuiId f = s->ctx.panneauFocus;
	delete s;
	return f;
}
static NkGuiId FocusApresClicBarreTitre() {
	SceneFocus *s = new SceneFocus();
	s->Init();
	s->barre = false; // pas de barre de menus : seule la barre de titre de l'hote
	s->ctx.titleBarH = 24.f;
	s->Image();
	s->Clic(s->cxA(), s->cy());
	s->Clic(300.f, 10.f);
	const NkGuiId f = s->ctx.panneauFocus;
	delete s;
	return f;
}

// ── (f6) la frappe ────────────────────────────────────────────────────────
struct ObsFrappe {
		bool champHorsFocusA = false, champHorsClavierA = true;
		bool kitBImage1 = true, kitBImage2 = true, kitBFini = false;
		bool kitHorsClavierA = true;
		bool champDansAFocus = false, champDansAClavierA = false, champDansAClavierB = true;
		bool kitAClavierA = false;
};
static ObsFrappe FocusObsFrappe() {
	ObsFrappe o;
	{ // un champ de NKGui focalise HORS de tout panneau (le raccourci d'une palette)
		SceneFocus *s = new SceneFocus();
		s->Init();
		s->Image();
		s->Clic(s->cxA(), s->cy());
		s->ctx.inputId = s->ctx.GetId("##palette");
		s->Image();
		s->Image();
		o.champHorsFocusA = s->ctx.PanneauAFocus(s->idA);
		o.champHorsClavierA = s->clavierA;
		delete s;
	}
	{ // un champ du KIT en frappe dans B (son focus est tenu par l'application)
		SceneFocus *s = new SceneFocus();
		s->Init();
		s->Image();
		s->Clic(s->cxA(), s->cy());
		s->frappeKitB = true;
		s->Image();
		o.kitBImage1 = s->clavierA; // B est dessine APRES A : rien a voir encore
		s->Image();
		o.kitBImage2 = s->clavierA; // l'image d'avant l'a vu
		s->frappeKitB = false;
		s->Image();
		s->Image();
		o.kitBFini = s->clavierA; // la frappe finie, A retrouve le clavier
		delete s;
	}
	{ // un champ du kit en frappe HORS de tout panneau
		SceneFocus *s = new SceneFocus();
		s->Init();
		s->Image();
		s->Clic(s->cxA(), s->cy());
		s->frappeKitHors = true;
		s->Image();
		s->Image();
		o.kitHorsClavierA = s->clavierA;
		delete s;
	}
	{ // un champ de NKGui DANS A : cliquer dedans donne le focus a A, et la frappe est chez lui
		SceneFocus *s = new SceneFocus();
		s->Init();
		s->champDansA = true;
		s->Image();
		s->Clic(s->champARect.x + 20.f, s->champARect.y + 10.f);
		s->Image();
		o.champDansAFocus = s->ctx.inputId != NKGUI_ID_NONE && s->ctx.PanneauAFocus(s->idA);
		o.champDansAClavierA = s->clavierA;
		o.champDansAClavierB = s->clavierB;
		delete s;
	}
	{ // un champ du kit en frappe DANS A
		SceneFocus *s = new SceneFocus();
		s->Init();
		s->Image();
		s->Clic(s->cxA(), s->cy());
		s->frappeKitA = true;
		s->Image();
		s->Image();
		o.kitAClavierA = s->clavierA;
		delete s;
	}
	return o;
}

static void CasFocusPanneau() {
	// =====================================================================
	printf("\n-- (b24) LE FOCUS DE PANNEAU : qui a le clavier\n");
	// =====================================================================
	if (const char *e = getenv("NK_FOCUS_MUTATION")) {
		uint32 i = 0;
		for (; e[i] && i + 1 < (uint32)sizeof(g_focusExterne); ++i)
			g_focusExterne[i] = e[i];
		g_focusExterne[i] = 0;
		printf("        ⚠️ NK_FOCUS_MUTATION=%s pose au lancement : les criteres positifs DOIVENT rougir\n",
			   g_focusExterne);
	}
	SceneFocus *pNoms = new SceneFocus(); // les identifiants de A et B, pour lire les resultats
	pNoms->Init();
	const SceneFocus &noms = *pNoms;

	// ── (f1) ─────────────────────────────────────────────────────────────
	const ObsAB ab = FocusObsAB();
	printf("        clic dans A -> %s ; clic dans B -> %s\n", NomFocus(noms, ab.apresA), NomFocus(noms, ab.apresB));
	Check(ab.apresA == noms.idA, "(f1) un clic dans A lui donne le focus");
	Check(ab.clavierAApresA && !ab.clavierBApresA, "(f1) A a le clavier, B ne l'a pas");
	Check(ab.apresB == noms.idB, "(f1) puis un clic dans B le donne a B");
	Check(!ab.clavierAApresB && ab.clavierBApresB, "(f1) et A ne l'a plus");

	// ── (f2) ─────────────────────────────────────────────────────────────
	const NkGuiId vide = FocusApresClicVide();
	printf("        clic dans A puis dans le vide -> %s\n", NomFocus(noms, vide));
	Check(vide == NKGUI_ID_NONE, "(f2) un clic hors de tout panneau RETIRE le focus");

	// ── (f3) ─────────────────────────────────────────────────────────────
	Check(FocusApresClicDroit() == noms.idA, "(f3) un clic DROIT dans A lui donne aussi le focus");

	// ── (f4) ─────────────────────────────────────────────────────────────
	const ObsMenu me = FocusObsMenu();
	printf("        menu ouvert=%d, entree (%.0f,%.0f %.0fx%.0f) sur A=%d ; focus apres le titre %s, apres "
		   "l'entree %s, apres le relache %s\n",
		   (int)me.ouvert, (double)me.item.x, (double)me.item.y, (double)me.item.w, (double)me.item.h,
		   (int)me.itemSurA, NomFocus(noms, me.apresTitre), NomFocus(noms, me.apresItem),
		   NomFocus(noms, me.apresRelache));
	Check(me.ouvert && me.itemSurA, "(f4) le menu est ouvert, et son entree est bien SUR A (sinon rien n'est prouve)");
	Check(me.apresTitre == noms.idB, "(f4) le clic sur le TITRE du menu ne retire pas le focus a B");
	Check(me.apresItem == noms.idB, "(f4) LE CLIC SUR L'ENTREE, AU-DESSUS DE A : ni A ne le prend, ni B ne le perd");
	Check(me.apresRelache == noms.idB, "(f4) et apres le relache (le menu se referme), B l'a toujours");
	Check(!me.clavierBMenuOuvert, "(f4) menu ouvert : B a le focus mais pas le clavier (le menu l'a)");
	Check(me.clavierBMenuFerme, "(f4) menu ferme : B retrouve le clavier");

	// ── (f5) ─────────────────────────────────────────────────────────────
	const NkGuiId s50 = FocusApresSurface50(), modale = FocusApresClicSousModale(), titre = FocusApresClicBarreTitre();
	printf("        surface 50 sur A -> %s ; vide sous modale -> %s ; barre de titre -> %s\n", NomFocus(noms, s50),
		   NomFocus(noms, modale), NomFocus(noms, titre));
	Check(s50 == noms.idB, "(f5) un clic dans une surface de couche 50 posee sur A : B garde le focus");
	Check(modale == noms.idA, "(f5) un clic sous une modale declaree ne retire rien");
	Check(titre == noms.idA, "(f5) un clic dans la barre de titre de l'hote ne retire rien");

	// ── (f6) ─────────────────────────────────────────────────────────────
	const ObsFrappe fr = FocusObsFrappe();
	printf("        champ NKGui hors panneau : focus A=%d clavier A=%d ; kit dans B : image 1 %d, image 2 %d, "
		   "fini %d ; kit hors panneau : %d ; champ dans A : focus=%d clavier A=%d B=%d ; kit dans A : %d\n",
		   (int)fr.champHorsFocusA, (int)fr.champHorsClavierA, (int)fr.kitBImage1, (int)fr.kitBImage2,
		   (int)fr.kitBFini, (int)fr.kitHorsClavierA, (int)fr.champDansAFocus, (int)fr.champDansAClavierA,
		   (int)fr.champDansAClavierB, (int)fr.kitAClavierA);
	Check(fr.champHorsFocusA && !fr.champHorsClavierA,
		  "(f6) un champ NKGui en frappe HORS panneau : A garde le focus, mais n'a PAS le clavier");
	Check(!fr.kitBImage2, "(f6) un champ du kit en frappe dans B : A n'a pas le clavier (des l'image suivante)");
	Check(fr.kitBFini, "(f6) la frappe finie, A retrouve le clavier");
	Check(!fr.kitHorsClavierA, "(f6) un champ du kit en frappe hors de tout panneau : A n'a pas le clavier");
	Check(fr.champDansAFocus && fr.champDansAClavierA && !fr.champDansAClavierB,
		  "(f6) un champ NKGui en frappe DANS A : A garde le clavier (sa frappe est son affaire)");
	Check(fr.kitAClavierA, "(f6) un champ du kit en frappe DANS A : A garde le clavier");

	// =====================================================================
	//  LES CONTRE-EPREUVES : une mutation par regle, chacune doit rougir SON critere
	// =====================================================================
	printf("        -- contre-epreuves (NK_FOCUS_MUTATION), dans ce processus --\n");
	{
		ArmerFocus("collant");
		const ObsAB m = FocusObsAB();
		ArmerFocus(nullptr);
		printf("        [collant] clic dans B -> %s\n", NomFocus(noms, m.apresB));
		Check(m.apresB != noms.idB, "(f1) [contre-epreuve collant] le focus qui ne bouge plus fait rougir (f1)");
	}
	{
		ArmerFocus("garde");
		const NkGuiId m = FocusApresClicVide();
		ArmerFocus(nullptr);
		printf("        [garde] clic dans le vide -> %s\n", NomFocus(noms, m));
		Check(m != NKGUI_ID_NONE, "(f2) [contre-epreuve garde] le clic ailleurs qui ne retire rien fait rougir (f2)");
	}
	{
		ArmerFocus("popup");
		const ObsMenu m = FocusObsMenu();
		ArmerFocus(nullptr);
		printf("        [popup] apres l'entree -> %s\n", NomFocus(noms, m.apresItem));
		Check(m.itemSurA && m.apresItem == noms.idA,
			  "(f4) [contre-epreuve popup] le panneau qui ignore le menu PREND le focus : (f4) rougit");
	}
	{
		ArmerFocus("surcouche");
		const ObsMenu m = FocusObsMenu();
		const NkGuiId m50 = FocusApresSurface50(), mMod = FocusApresClicSousModale(), mTit = FocusApresClicBarreTitre();
		ArmerFocus(nullptr);
		printf("        [surcouche] titre -> %s, entree -> %s ; surface 50 -> %s ; modale -> %s ; barre de titre "
			   "-> %s\n",
			   NomFocus(noms, m.apresTitre), NomFocus(noms, m.apresItem), NomFocus(noms, m50), NomFocus(noms, mMod),
			   NomFocus(noms, mTit));
		Check(m.apresTitre != noms.idB && m.apresItem != noms.idB,
			  "(f4) [contre-epreuve surcouche] le clic dans le menu RETIRE le focus : (f4) rougit");
		Check(m50 != noms.idB && mMod != noms.idA && mTit != noms.idA,
			  "(f5) [contre-epreuve surcouche] surface 50, modale, barre de titre : (f5) rougit trois fois");
	}
	{
		ArmerFocus("frappe");
		const ObsFrappe m = FocusObsFrappe();
		ArmerFocus(nullptr);
		printf("        [frappe] champ hors panneau : clavier A=%d ; kit dans B : %d ; kit hors : %d\n",
			   (int)m.champHorsClavierA, (int)m.kitBImage2, (int)m.kitHorsClavierA);
		Check(m.champHorsClavierA && m.kitBImage2 && m.kitHorsClavierA,
			  "(f6) [contre-epreuve frappe] la frappe ailleurs ignoree : (f6) rougit trois fois");
	}
	{
		ArmerFocus("proprietaire");
		const ObsFrappe m = FocusObsFrappe();
		ArmerFocus(nullptr);
		printf("        [proprietaire] champ dans A : clavier A=%d ; kit dans A : %d\n", (int)m.champDansAClavierA,
			   (int)m.kitAClavierA);
		Check(!m.champDansAClavierA && !m.kitAClavierA,
			  "(f6) [contre-epreuve proprietaire] la frappe chez soi comptee ailleurs : (f6) rougit deux fois");
	}
	ArmerFocus(nullptr);
	delete pNoms;
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
