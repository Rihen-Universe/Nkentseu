// =============================================================================
// Sandbox/System/NKArchive/src/NkGuiDocCorpus.h
// Le SECOND corpus : les exemples des documents de reference du langage.
// Genere le 2026-08-22 depuis les blocs `nkgui` des documents 2 et 9.
// =============================================================================

#pragma once

// =============================================================================
// L13 -- LE SECOND CORPUS
// =============================================================================
// ATTENTION -- POURQUOI LE CORPUS DES 10 NE SUFFIT PAS, ET C'EST MESURE.
//    Les dix sources de `CorpusUI` sont TOUTES du meme moule : une seule section
//    (`widgets`), des valeurs qui sont des chaines ou `false`, aucun commentaire,
//    aucune ligne vide au milieu, aucun entrelacement autre que « proprietes
//    puis enfants ». Elles ne touchent ni `behavior`, ni `animation`, ni `fonts`,
//    ni `controller`, ni `appearance`, ni `include`, ni une liste, ni un
//    dictionnaire, ni une couleur, ni un vecteur. Passer a 10/10 sur elles ne
//    dit RIEN de ces constructions-la.
//
//    Ces neuf-ci viennent des DOCUMENTS DE REFERENCE du langage
//    (`2_NkUIDesign_Langage_Description_NodeBlueprint.md` et
//    `design/9_Grammaire_complete.md`), recopies tels quels. Ils n'ont ete
//    ecrits ni pour cette couche ni par elle : c'est la seule facon d'avoir un
//    corpus qui ne soit pas d'accord avec l'implementation par construction.
//
//    Le `nkgui X.Y` a ete ajoute en tete des extraits qui n'en avaient pas --
//    c'est la SEULE retouche, et elle est necessaire : un extrait de
//    documentation n'est pas un fichier.
static void L13_CorpusDesDocuments() {
	printf("[L13] Le second corpus : les exemples des documents de grammaire\n");

	{
		const char *src =
		"nkgui 0.2\n"
		"\n"
		"widgets {\n"
		"    Window \"Inspecteur\" {\n"
		"        pos = (40, 40)\n"
		"        size = (320, 480)\n"
		"        flags = Resizable | Closable\n"
		"\n"
		"        Panel \"Transform\" {\n"
		"            SliderFloat \"X\" {\n"
		"                bind = x\n"
		"                min = -100\n"
		"                max = 100\n"
		"                on Commit(value) -> Callback \"TransformInspector.OnPositionChanged\"(Enum.X, value)\n"
		"            }\n"
		"            Button \"Reset\" {\n"
		"                on Click -> Callback \"TransformInspector.OnResetClicked\"()\n"
		"            }\n"
		"        }\n"
		"    }\n"
		"}\n";
		NkString out;
		NkGuiDiag err;
		const char *nom = "doc_2_Nk_05.nkgui";
		if (!NkGRoundTrip(src, out, err)) {
			printf("  FAIL  %s refuse : %s %s (l.%u)\n", nom, err.code.CStr(),
				   err.message.CStr(), err.line);
			++s_fail;
		} else if (NkGFirstDiff(out, NkString(src)) >= 0) {
			printf("  FAIL  %s : difference a l'octet %ld\n", nom,
				   NkGFirstDiff(out, NkString(src)));
			printf("    obtenu  : <<%s>>\n", out.CStr());
			++s_fail;
		} else {
			++s_pass;
		}
	}
	{
		const char *src =
		"nkgui 0.3\n"
		"behavior \"PreviewOpacity\" {\n"
		"    set opacityPreview = value * 100\n"
		"    if value > 0.8 {\n"
		"        Callback \"WarnHighValue\"()\n"
		"    } else {\n"
		"        set opacityPreview = value * 50\n"
		"    }\n"
		"}\n";
		NkString out;
		NkGuiDiag err;
		const char *nom = "doc_2_Nk_06.nkgui";
		if (!NkGRoundTrip(src, out, err)) {
			printf("  FAIL  %s refuse : %s %s (l.%u)\n", nom, err.code.CStr(),
				   err.message.CStr(), err.line);
			++s_fail;
		} else if (NkGFirstDiff(out, NkString(src)) >= 0) {
			printf("  FAIL  %s : difference a l'octet %ld\n", nom,
				   NkGFirstDiff(out, NkString(src)));
			printf("    obtenu  : <<%s>>\n", out.CStr());
			++s_fail;
		} else {
			++s_pass;
		}
	}
	{
		const char *src =
		"nkgui 0.3\n"
		"behavior \"PreviewOpacity\" graph {\n"
		"    node n1 EventChanged\n"
		"    node n2 Multiply     { a = n1.value, b = 100 }\n"
		"    node n3 SetVariable  { name = \"opacityPreview\", value = n2.result }\n"
		"    node n4 Compare      { a = n1.value, op = \">\", b = 0.8 }\n"
		"    node n5 Branch       { cond = n4.result }\n"
		"    node n6 CallCallback { name = \"WarnHighValue\" }\n"
		"\n"
		"    wire n1.exec -> n3.exec -> n5.exec\n"
		"    wire n5.true -> n6.exec\n"
		"}\n";
		NkString out;
		NkGuiDiag err;
		const char *nom = "doc_2_Nk_07.nkgui";
		if (!NkGRoundTrip(src, out, err)) {
			printf("  FAIL  %s refuse : %s %s (l.%u)\n", nom, err.code.CStr(),
				   err.message.CStr(), err.line);
			++s_fail;
		} else if (NkGFirstDiff(out, NkString(src)) >= 0) {
			printf("  FAIL  %s : difference a l'octet %ld\n", nom,
				   NkGFirstDiff(out, NkString(src)));
			printf("    obtenu  : <<%s>>\n", out.CStr());
			++s_fail;
		} else {
			++s_pass;
		}
	}
	{
		const char *src =
		"nkgui 0.2\n"
		"include \"Theme.nkgui\"\n"
		"include \"Widgets/Inspecteur.nkgui\"\n";
		NkString out;
		NkGuiDiag err;
		const char *nom = "doc_2_Nk_08.nkgui";
		if (!NkGRoundTrip(src, out, err)) {
			printf("  FAIL  %s refuse : %s %s (l.%u)\n", nom, err.code.CStr(),
				   err.message.CStr(), err.line);
			++s_fail;
		} else if (NkGFirstDiff(out, NkString(src)) >= 0) {
			printf("  FAIL  %s : difference a l'octet %ld\n", nom,
				   NkGFirstDiff(out, NkString(src)));
			printf("    obtenu  : <<%s>>\n", out.CStr());
			++s_fail;
		} else {
			++s_pass;
		}
	}
	{
		const char *src =
		"nkgui 0.3\n"
		"controller \"TransformInspector\" {\n"
		"    callback OnPositionChanged(axis: Enum[X,Y,Z], value: Float) -> Void\n"
		"    callback OnResetClicked() -> Void\n"
		"    callback OnTintChanged(color: Color) -> Void\n"
		"}\n"
		"\n"
		"callback WarnHighValue() -> Void\n";
		NkString out;
		NkGuiDiag err;
		const char *nom = "doc_2_Nk_09.nkgui";
		if (!NkGRoundTrip(src, out, err)) {
			printf("  FAIL  %s refuse : %s %s (l.%u)\n", nom, err.code.CStr(),
				   err.message.CStr(), err.line);
			++s_fail;
		} else if (NkGFirstDiff(out, NkString(src)) >= 0) {
			printf("  FAIL  %s : difference a l'octet %ld\n", nom,
				   NkGFirstDiff(out, NkString(src)));
			printf("    obtenu  : <<%s>>\n", out.CStr());
			++s_fail;
		} else {
			++s_pass;
		}
	}
	{
		const char *src =
		"nkgui 0.3\n"
		"widgets {\n"
		"    VBox \"formulaire\" {\n"
		"        Text \"etiquette_email\" {\n"
		"            text = \"Adresse e-mail\"\n"
		"            align = start\n"
		"            maxLines = 1\n"
		"            overflow = ellipsis\n"
		"            for = \"champ_email\"\n"
		"        }\n"
		"        InputText \"champ_email\" {\n"
		"            bind = email\n"
		"        }\n"
		"        Spacer \"respiration\" {\n"
		"            size = 24\n"
		"        }\n"
		"    }\n"
		"}\n";
		NkString out;
		NkGuiDiag err;
		const char *nom = "doc_9_Gr_01.nkgui";
		if (!NkGRoundTrip(src, out, err)) {
			printf("  FAIL  %s refuse : %s %s (l.%u)\n", nom, err.code.CStr(),
				   err.message.CStr(), err.line);
			++s_fail;
		} else if (NkGFirstDiff(out, NkString(src)) >= 0) {
			printf("  FAIL  %s : difference a l'octet %ld\n", nom,
				   NkGFirstDiff(out, NkString(src)));
			printf("    obtenu  : <<%s>>\n", out.CStr());
			++s_fail;
		} else {
			++s_pass;
		}
	}
	{
		const char *src =
		"nkgui 0.3\n"
		"widgets {\n"
		"    Button \"valider\" {\n"
		"        label = \"Valider\"\n"
		"\n"
		"        appearance {\n"
		"            radius = 6\n"
		"            opacity = 1.0\n"
		"            font = \"Inter\"\n"
		"            weight = 600\n"
		"            size = 14\n"
		"            fill { color = #2F6F7A }\n"
		"            shadow { offset = (0, 2), blur = 6, color = #0000003A }\n"
		"        }\n"
		"        appearance(Hover) {\n"
		"            fill { color = #3A8894 }\n"
		"        }\n"
		"    }\n"
		"}\n";
		NkString out;
		NkGuiDiag err;
		const char *nom = "doc_9_Gr_02.nkgui";
		if (!NkGRoundTrip(src, out, err)) {
			printf("  FAIL  %s refuse : %s %s (l.%u)\n", nom, err.code.CStr(),
				   err.message.CStr(), err.line);
			++s_fail;
		} else if (NkGFirstDiff(out, NkString(src)) >= 0) {
			printf("  FAIL  %s : difference a l'octet %ld\n", nom,
				   NkGFirstDiff(out, NkString(src)));
			printf("    obtenu  : <<%s>>\n", out.CStr());
			++s_fail;
		} else {
			++s_pass;
		}
	}
	{
		const char *src =
		"nkgui 0.3\n"
		"animation \"bouton_valider\" {\n"
		"\n"
		"    transition \"appui\" {\n"
		"        target = \"valider\"\n"
		"        on = Click\n"
		"        duration = 0.12\n"
		"        curve = EaseOut\n"
		"        reduceMotion = shorten\n"
		"        track \"scale\" {\n"
		"            key 0.0 -> 1.0\n"
		"            key 1.0 -> 0.96, curve = EaseOut\n"
		"        }\n"
		"    }\n"
		"\n"
		"    ambience \"respiration\" {\n"
		"        target = \"valider\"\n"
		"        state = Idle\n"
		"        duration = 2.4\n"
		"        repeat = 0\n"
		"        direction = alternate\n"
		"        jitter = 0.3\n"
		"        reduceMotion = stop\n"
		"        track \"scale\" {\n"
		"            key 0.0 -> 1.0\n"
		"            key 1.0 -> 1.02\n"
		"        }\n"
		"    }\n"
		"\n"
		"    continuous \"reflet\" {\n"
		"        target = \"valider\"\n"
		"        rest = 0.0\n"
		"        smoothing = 0.18\n"
		"        reduceMotion = stop\n"
		"        map {\n"
		"            source = PointerX\n"
		"            sourceFrom = -1.0\n"
		"            sourceTo = 1.0\n"
		"            property = \"fill.angle\"\n"
		"            targetFrom = -6.0\n"
		"            targetTo = 6.0\n"
		"            curve = Linear\n"
		"        }\n"
		"    }\n"
		"}\n";
		NkString out;
		NkGuiDiag err;
		const char *nom = "doc_9_Gr_03.nkgui";
		if (!NkGRoundTrip(src, out, err)) {
			printf("  FAIL  %s refuse : %s %s (l.%u)\n", nom, err.code.CStr(),
				   err.message.CStr(), err.line);
			++s_fail;
		} else if (NkGFirstDiff(out, NkString(src)) >= 0) {
			printf("  FAIL  %s : difference a l'octet %ld\n", nom,
				   NkGFirstDiff(out, NkString(src)));
			printf("    obtenu  : <<%s>>\n", out.CStr());
			++s_fail;
		} else {
			++s_pass;
		}
	}
	{
		const char *src =
		"nkgui 0.3\n"
		"fonts {\n"
		"    font \"titre\" {\n"
		"        family = \"Inter\"\n"
		"        weight = 600\n"
		"        style = normal\n"
		"        kind = text\n"
		"        source {\n"
		"            mode = embedded\n"
		"            path = \"Fonts/Inter-SemiBold.ttf\"\n"
		"        }\n"
		"        fallback {\n"
		"            family = \"Noto Sans\"\n"
		"            family = \"DejaVu Sans\"\n"
		"        }\n"
		"        metrics {\n"
		"            unitsPerEm = 2048\n"
		"            lineHeight = 1.21\n"
		"            ascent = 1984\n"
		"            descent = -494\n"
		"            glyph \"A\" -> 1366\n"
		"            glyph \"M\" -> 1774\n"
		"            glyph \"i\" -> 569\n"
		"        }\n"
		"    }\n"
		"\n"
		"    font \"icones\" {\n"
		"        family = \"Rihen Icons\"\n"
		"        kind = icons\n"
		"        source {\n"
		"            mode = embedded\n"
		"            path = \"Fonts/RihenIcons.ttf\"\n"
		"        }\n"
		"    }\n"
		"}\n";
		NkString out;
		NkGuiDiag err;
		const char *nom = "doc_9_Gr_04.nkgui";
		if (!NkGRoundTrip(src, out, err)) {
			printf("  FAIL  %s refuse : %s %s (l.%u)\n", nom, err.code.CStr(),
				   err.message.CStr(), err.line);
			++s_fail;
		} else if (NkGFirstDiff(out, NkString(src)) >= 0) {
			printf("  FAIL  %s : difference a l'octet %ld\n", nom,
				   NkGFirstDiff(out, NkString(src)));
			printf("    obtenu  : <<%s>>\n", out.CStr());
			++s_fail;
		} else {
			++s_pass;
		}
	}
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
