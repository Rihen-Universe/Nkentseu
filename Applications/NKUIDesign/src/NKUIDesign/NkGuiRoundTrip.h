//
// NkGuiRoundTrip.h
// =============================================================================
// Description :
//   L'ALLER-RETOUR : le temoin du lecteur/ecrivain `.nkgui`. Un document analyse
//   puis reemis doit redonner un fichier equivalent a l'original. C'est le
//   critere d'acceptation du format, et il ne depend d'aucun jugement.
//
// Caracteristiques :
//   - deux mesures, PAS une, et elles ne disent pas la meme chose :
//       1. EQUIVALENCE  — relire ce qu'on a ecrit redonne le meme document
//                         (`NkGEqual`). C'est le critere qui fait foi ;
//       2. IDENTITE OCTET — le texte reemis est exactement le fichier d'origine.
//                         C'est plus fort, et ca depend de la mise en forme ;
//   - le style d'ecriture du fichier SOURCE est detecte et repris (largeur
//     d'indentation, fins de ligne). Un editeur qui reformate le fichier de
//     quelqu'un d'autre a chaque enregistrement produit des diffs illisibles.
//
// ⚠️ POURQUOI DEUX MESURES, ET POURQUOI L'ORDRE COMPTE :
//    l'identite octet seule punirait une difference d'indentation comme une
//    perte de donnee. L'equivalence seule laisserait passer un ecrivain qui
//    ecrit du charabia, du moment que son propre lecteur le relit pareil.
//    **Les deux ensemble ne laissent passer ni l'un ni l'autre.**
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKUIDESIGN_NKGUIROUNDTRIP_H__
#define __NKENTSEU_NKUIDESIGN_NKGUIROUNDTRIP_H__

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKPlatform/NkPlatformDetect.h"

#include "NkGuiFormat.h"

#include <cstdio>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include <windows.h>
#endif

namespace nkuidesign {
	namespace guifmt {

		using nkentseu::NkDirectory;
		using nkentseu::NkFile;

		// ════════════════════════════════════════════════════════════════════════
		//  PUBLIER LE RAPPORT — et le probleme, releve le 2026-08-21
		// ════════════════════════════════════════════════════════════════════════
		//
		//  ⚠️ `fputs(rep, stdout)` NE SORT NULLE PART DANS CETTE APPLICATION, et il a
		//     fallu qu'un relecteur lance la commande pour s'en apercevoir.
		//
		//     `NkWindowsDesktop.h` (le point d'entree Windows du moteur) fait, en
		//     Debug, juste avant d'appeler `nkmain` :
		//
		//         AllocConsole();
		//         freopen_s(..., "CONOUT$", "w", stdout);
		//         freopen_s(..., "CONOUT$", "w", stderr);
		//
		//     Le flux C `stdout` est donc reattache a un TAMPON DE CONSOLE. Tout ce
		//     qu'on y ecrit part dans cette console-la — pas dans le terminal
		//     appelant, et **surtout pas dans une redirection `> fichier`**, qui est
		//     court-circuitee. Le programme rend 0, n'affiche rien, et a pourtant
		//     tout ecrit. `--probe` a le meme comportement depuis toujours.
		//
		//     ⚠️ `NkConsoleStream` (NKStream) ne repond pas au besoin : il ecrit
		//        avec `WriteConsoleA`, qui ECHOUE sur un tuyau ou un fichier. Il sert
		//        a peindre une console, pas a alimenter une sortie standard.
		//
		//     Le handle systeme, lui, est intact : `freopen_s` rebranche le flux du
		//     CRT, il ne touche pas a `GetStdHandle(STD_OUTPUT_HANDLE)`. On ecrit
		//     donc **directement dessus**, avec `WriteFile` — qui marche sur une
		//     console, un tuyau ET un fichier redirige, la ou `WriteConsoleA` n'en
		//     couvre qu'un des trois.
		//
		//  Le fichier reste ecrit en plus, et son chemin ABSOLU est affiche : un banc
		//  dont il faut deviner ou est le resultat ne sert qu'a celui qui l'a ecrit.
		inline void NkGPublish(NkString &rep, const char *fileName) {
			const nkentseu::NkPath cwd = NkDirectory::GetCurrentDirectory();
			rep.Append("\nRapport ecrit dans : ");
			rep.Append(cwd.ToString());
			rep.Append('/');
			rep.Append(fileName);
			rep.Append('\n');

			NkFile::WriteAllText(fileName, rep.Data());

#if defined(NKENTSEU_PLATFORM_WINDOWS)
			const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
			if (out && out != INVALID_HANDLE_VALUE) {
				DWORD written = 0;
				WriteFile(out, rep.Data(), (DWORD)rep.Size(), &written, nullptr);
			}
#else
			// ⚠️ L'UN OU L'AUTRE, JAMAIS LES DEUX. Sur Windows, le flux `stdout` du
			//    CRT et le handle systeme designent desormais la meme sortie : y
			//    ecrire par les deux chemins imprimerait le rapport en double, et un
			//    banc qui affiche deux fois « 14 / 14 » fait douter des deux.
			fputs(rep.Data(), stdout);
			fflush(stdout);
#endif
		}

		/// Le style d'ecriture LU dans le fichier source. Ce n'est pas de la
		/// devinette de confort : sans lui, reecrire un document indente a deux
		/// espaces le rendrait indente a quatre, et le diff porterait sur chaque
		/// ligne du fichier au lieu de porter sur ce qui a change.
		inline NkGWriteOptions NkGDetectStyle(const char *text, uint32 length) {
			NkGWriteOptions opt;
			opt.crlf = false;
			for (uint32 i = 0; i + 1 < length; ++i) {
				if (text[i] == '\r' && text[i + 1] == '\n') {
					opt.crlf = true;
					break;
				}
				if (text[i] == '\n') {
					break;
				}
			}
			// La premiere ligne qui commence par des espaces donne la largeur d'un
			// cran : c'est forcement un cran, jamais deux, parce qu'un fichier
			// commence par une section au niveau zero.
			opt.indent = 4;
			for (uint32 i = 0; i < length; ++i) {
				if (text[i] != '\n') {
					continue;
				}
				uint32 j = i + 1;
				uint32 spaces = 0;
				while (j < length && text[j] == ' ') {
					++spaces;
					++j;
				}
				if (spaces > 0 && j < length && text[j] != '\r' && text[j] != '\n') {
					opt.indent = spaces;
					break;
				}
			}

			// La ligne vide apres l'en-tete (et, par la meme convention, entre les
			// sections). Le convertisseur du corpus Camrail en met une, les
			// exemples du document 2 n'en mettent pas.
			//
			// ⚠️ UNE SEULE OBSERVATION SERT AUX DEUX REGLAGES, et c'est une
			//    HEURISTIQUE assumee : rien ne garantit qu'un auteur qui aere son
			//    en-tete aere aussi ses sections. Elle ne porte que sur la mise en
			//    forme — se tromper coute une ligne vide, jamais une donnee.
			opt.blankLineAfterHeader = false;
			for (uint32 i = 0; i < length; ++i) {
				if (text[i] != '\n') {
					continue;
				}
				uint32 j = i + 1;
				// Les lignes `include` font encore partie de l'en-tete.
				if (j + 7 <= length && text[j] == 'i' && text[j + 1] == 'n' && text[j + 2] == 'c'
					&& text[j + 3] == 'l' && text[j + 4] == 'u' && text[j + 5] == 'd'
					&& text[j + 6] == 'e') {
					continue;
				}
				opt.blankLineAfterHeader = (j < length && (text[j] == '\n' || text[j] == '\r'));
				break;
			}
			opt.blankLineBetweenSections = opt.blankLineAfterHeader;
			return opt;
		}

		struct NkGRoundTripResult {
				NkString file;
				bool parsed = false;
				bool equivalent = false;	///< le critere qui fait foi
				bool byteIdentical = false;	///< la mesure plus forte
				uint32 nodeCount = 0;
				uint32 firstDiffOffset = 0;	///< si !byteIdentical
				NkGDiag diag;
		};

		/// L'aller-retour sur UN document. `original` est le contenu du fichier, en
		/// octets, tel qu'il est sur le disque.
		inline NkGRoundTripResult NkGRoundTripOne(const char *original, uint32 length) {
			NkGRoundTripResult r;

			NkGDocument doc1;
			if (!NkGParse(original, length, doc1, r.diag)) {
				return r;
			}
			r.parsed = true;
			r.nodeCount = (uint32)doc1.nodes.Size();

			const NkGWriteOptions opt = NkGDetectStyle(original, length);
			const NkString emitted = NkGWrite(doc1, opt);

			// ── Mesure 1 : l'equivalence ────────────────────────────────────
			NkGDiag err2;
			NkGDocument doc2;
			if (NkGParse(emitted.Data(), (uint32)emitted.Size(), doc2, err2)) {
				r.equivalent = NkGEqual(doc1, doc2);
			} else {
				// ⚠️ Ce cas merite d'etre distingue d'une simple inegalite : il dit
				//    que l'ecrivain a produit un fichier que le lecteur REFUSE. Ce
				//    n'est pas une perte de fidelite, c'est une sortie invalide.
				r.diag = err2;
				r.diag.message.Append(" [dans le texte REEMIS, pas dans le fichier source]");
			}

			// ── Mesure 2 : l'identite octet ─────────────────────────────────
			const uint32 n = (uint32)emitted.Size();
			if (n == length) {
				r.byteIdentical = true;
				for (uint32 i = 0; i < n; ++i) {
					if (emitted.Data()[i] != original[i]) {
						r.byteIdentical = false;
						r.firstDiffOffset = i;
						break;
					}
				}
			} else {
				uint32 i = 0;
				const uint32 m = (n < length) ? n : length;
				while (i < m && emitted.Data()[i] == original[i]) {
					++i;
				}
				r.firstDiffOffset = i;
			}
			return r;
		}

		/// L'aller-retour sur tout un dossier. Ecrit son rapport sur la sortie
		/// standard ET dans `nkuidesign_roundtrip.txt` — une application fenetree
		/// n'a pas toujours de console attachee, et un resultat qu'on ne peut pas
		/// relire ne prouve rien.
		inline int NkGRunRoundTrip(const char *directory) {
			NkString rep("=== ALLER-RETOUR .nkgui — le temoin du lecteur/ecrivain ===\n");
			rep.Append("dossier : ");
			rep.Append(directory ? directory : "(aucun)");
			rep.Append('\n');
			rep.Append("critere qui fait foi : analyser -> reemettre -> reanalyser rend le meme "
					   "document.\n\n");

			if (!directory || !NkDirectory::Exists(directory)) {
				rep.Append("ECHEC : le dossier n'existe pas. Rien n'a ete mesure.\n");
				NkGPublish(rep, "nkuidesign_roundtrip.txt");
				return 2;
			}

			NkVector<NkString> files = NkDirectory::GetFiles(directory, "*.nkgui");
			uint32 total = 0;
			uint32 equivalent = 0;
			uint32 identical = 0;
			char buf[512];

			for (uint32 i = 0; i < (uint32)files.Size(); ++i) {
				++total;
				NkVector<nkentseu::uint8> bytes = NkFile::ReadAllBytes(files[i].Data());
				const NkGRoundTripResult r =
					NkGRoundTripOne((const char *)bytes.Data(), (uint32)bytes.Size());
				if (r.equivalent) {
					++equivalent;
				}
				if (r.byteIdentical) {
					++identical;
				}

				if (!r.parsed) {
					snprintf(buf, sizeof(buf),
							 "  [ECHEC ANALYSE] %s\n      %s ligne %u colonne %u : %s\n",
							 files[i].Data(), r.diag.code.Data(), r.diag.line, r.diag.column,
							 r.diag.message.Data());
				} else if (!r.equivalent) {
					snprintf(buf, sizeof(buf),
							 "  [NON EQUIVALENT] %s (%u noeuds) — %s\n", files[i].Data(),
							 r.nodeCount,
							 r.diag.message.Empty() ? "le document relu differe de l'original"
													: r.diag.message.Data());
				} else if (!r.byteIdentical) {
					snprintf(buf, sizeof(buf),
							 "  [OK, mise en forme differente] %s (%u noeuds), premier ecart a "
							 "l'octet %u\n",
							 files[i].Data(), r.nodeCount, r.firstDiffOffset);
				} else {
					snprintf(buf, sizeof(buf), "  [OK, octet pour octet] %s (%u noeuds)\n",
							 files[i].Data(), r.nodeCount);
				}
				rep.Append(buf);
			}

			snprintf(buf, sizeof(buf),
					 "\n=== TAUX D'ALLER-RETOUR : %u / %u equivalents — %u / %u identiques octet "
					 "pour octet ===\n",
					 equivalent, total, identical, total);
			rep.Append(buf);

			NkGPublish(rep, "nkuidesign_roundtrip.txt");
			return (total > 0 && equivalent == total) ? 0 : 1;
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LES CONTROLES — sans eux, « 10 / 10 » ne veut rien dire
		// ═══════════════════════════════════════════════════════════════════════
		//
		//  ⚠️ UN BANC QUI NE SAIT DIRE QUE « OUI » NE MESURE RIEN. Le taux
		//     d'aller-retour du corpus est un chiffre flatteur tant que personne
		//     n'a montre que ce banc SAIT ECHOUER. Trois familles, reprises de la
		//     discipline de `Probe.h` :
		//
		//     1. TEMOIN DE BRUIT — la meme mesure repetee sans rien changer. Le
		//        serialiseur est deterministe : le plancher attendu est EXACTEMENT
		//        zero difference, et on le verifie au lieu de le supposer ;
		//     2. CONTROLES POSITIFS — un changement connu DOIT etre vu. Sans eux,
		//        `NkGEqual` pourrait rendre `true` en toutes circonstances et le
		//        corpus passerait a 10/10 sans rien prouver ;
		//     3. CONTROLES NEGATIFS — ce que le format ne dit pas doit etre REFUSE
		//        avec un message, jamais devine. C'est ce qui distingue un lecteur
		//        strict d'un lecteur qui accepte tout et perd la moitie du fichier.
		//
		//  Et une quatrieme famille, qui n'est pas un controle mais une LIMITE
		//  mesuree : les commentaires et les lignes vides ne survivent pas. Mieux
		//  vaut un banc qui l'affiche qu'une documentation qui l'oublie.

		inline int NkGRunControls() {
			NkString rep("=== CONTROLES du lecteur/ecrivain .nkgui ===\n");
			uint32 pass = 0;
			uint32 total = 0;
			char buf[512];

			auto check = [&](const char *label, bool ok, const char *detail) {
				++total;
				if (ok) {
					++pass;
				}
				snprintf(buf, sizeof(buf), "  [%s] %s%s%s\n", ok ? "OK " : "NON", label,
						 (detail && *detail) ? " -- " : "", detail ? detail : "");
				rep.Append(buf);
			};
			auto parse = [](const char *src, NkGDocument &d, NkGDiag &e) {
				uint32 n = 0;
				while (src[n]) {
					++n;
				}
				return NkGParse(src, n, d, e);
			};
			auto rejects = [&](const char *src, const char *why) {
				NkGDocument d;
				NkGDiag e;
				const bool ko = !parse(src, d, e);
				snprintf(buf, sizeof(buf), "      refus attendu (%s) : %s\n", why,
						 ko ? e.message.Data() : "*** ACCEPTE ***");
				rep.Append(buf);
				return ko;
			};

			// 1. Le temoin de bruit.
			{
				NkGDocument d;
				NkGDiag e;
				parse("nkgui 0.2\nwidgets {\n  Button \"a\" { label = \"x\" }\n}\n", d, e);
				const NkString a = NkGWrite(d);
				const NkString b = NkGWrite(d);
				check("1. temoin de bruit : deux ecritures donnent le meme texte",
					  a.Compare(b) == 0, "");
			}

			// 2. Les controles positifs. Chaque paire ne differe QUE par ce que son
			//    libelle annonce.
			{
				static const char *kPairs[][3] = {
					{"2a. une valeur differente est DETECTEE",
					 "nkgui 0.2\nwidgets {\n Button \"a\" { label = \"x\" }\n}\n",
					 "nkgui 0.2\nwidgets {\n Button \"a\" { label = \"y\" }\n}\n"},
					{"2b. un id different est DETECTE",
					 "nkgui 0.2\nwidgets {\n Button \"a\" { }\n}\n",
					 "nkgui 0.2\nwidgets {\n Button \"b\" { }\n}\n"},
					{"2c. l'ORDRE des membres est DETECTE",
					 "nkgui 0.2\nwidgets {\n VBox \"v\" { a = 1\n Text \"t\" { } }\n}\n",
					 "nkgui 0.2\nwidgets {\n VBox \"v\" { Text \"t\" { }\n a = 1 }\n}\n"},
					{"2d. un lexeme numerique reecrit est DETECTE (0.20 != 0.2)",
					 "nkgui 0.2\nwidgets {\n Slider \"s\" { min = 0.20 }\n}\n",
					 "nkgui 0.2\nwidgets {\n Slider \"s\" { min = 0.2 }\n}\n"},
					{"2e. une section en trop est DETECTEE", "nkgui 0.2\nwidgets { }\n",
					 "nkgui 0.2\nwidgets { }\ncallback F() -> Void\n"},
					{"2f. l'ORDRE des drapeaux est DETECTE",
					 "nkgui 0.2\nwidgets {\n B \"a\" { f = X | Y }\n}\n",
					 "nkgui 0.2\nwidgets {\n B \"a\" { f = Y | X }\n}\n"},
				};
				for (uint32 i = 0; i < 6; ++i) {
					NkGDocument d1;
					NkGDocument d2;
					NkGDiag e;
					const bool ok1 = parse(kPairs[i][1], d1, e);
					const bool ok2 = parse(kPairs[i][2], d2, e);
					check(kPairs[i][0], ok1 && ok2 && !NkGEqual(d1, d2), "");
				}
			}

			// 3. Les controles negatifs. L'anti-slash se construit a l'execution :
			//    l'ecrire dans un litteral C++ traverse deux couches d'echappement,
			//    et c'est exactement la qu'un banc finit par mesurer autre chose que
			//    ce qu'il annonce.
			rep.Append("\n  -- refus attendus --\n");
			NkString badEsc("nkgui 0.2\nwidgets {\n T \"t\" { text = \"a");
			badEsc.Append('\\');
			badEsc.Append("z\" }\n}\n");

			const bool r1 =
				rejects("nkgui 0.2\nwidgets {\n Combo \"c\" { items = [\"a\", \"b\"] }\n}\n",
						"litteral de liste, doc 9 §6.1");
			const bool r2 = rejects(badEsc.Data(), "echappement hors des trois du doc 2 §2");
			const bool r3 = rejects("nkgui 0.2\nwidgets {\n Button \"a\" { label = \"x\" \n}\n",
									"accolade jamais fermee");
			const bool r4 = rejects("nkgui 0.2\nwidgets {\n Button \"a\" { c = #12345 }\n}\n",
									"couleur a 5 chiffres");
			const bool r5 = rejects("widgets { }\n", "en-tete nkgui manquant");
			const bool r6 =
				rejects("nkgui 0.2\n/* jamais ferme\nwidgets { }\n", "commentaire de bloc ouvert");
			const bool r7 = rejects("nkgui 0.2\ninconnue { }\n", "section inconnue");
			const bool r8 =
				rejects("nkgui 0.2\nwidgets {\n B \"a\" { p = }\n}\n", "valeur manquante");
			rep.Append("\n");
			check("3. les 8 documents fautifs sont TOUS refuses",
				  r1 && r2 && r3 && r4 && r5 && r6 && r7 && r8, "");

			// 4. Ce qui DOIT passer : les trois echappements du document 2, l'UTF-8
			//    et la chaine vide. C'est le seul endroit du serialiseur qui
			//    REGENERE une valeur — donc le seul qui puisse la perdre.
			{
				NkString src("nkgui 0.2\nwidgets {\n    T \"t\" {\n        text = \"a");
				src.Append('\\');
				src.Append('"');
				src.Append('b');
				src.Append('\\');
				src.Append('\\');
				src.Append('c');
				src.Append('\\');
				src.Append('n');
				src.Append("d \xC3\xA9\xC3\xA8 \xE2\x9C\x93\"\n        vide = \"\"\n    }\n}\n");

				NkGDocument d;
				NkGDiag e;
				const bool ok = NkGParse(src.Data(), (uint32)src.Size(), d, e);
				const NkString out = NkGWrite(d, NkGDetectStyle(src.Data(), (uint32)src.Size()));
				NkGDocument d2;
				NkGDiag e2;
				const bool ok2 = NkGParse(out.Data(), (uint32)out.Size(), d2, e2);
				check("4. les trois echappements, l'UTF-8 et la chaine vide survivent",
					  ok && ok2 && NkGEqual(d, d2), ok ? "" : e.message.Data());
				check("4b. et le texte reemis est identique octet pour octet",
					  ok && out.Compare(src) == 0, "");
			}

			// 5. LA LIMITE, mesuree et non supposee.
			{
				const char *src =
					"nkgui 0.2\nwidgets {\n // un commentaire\n\n Button \"a\" { }\n}\n";
				NkGDocument d;
				NkGDiag e;
				const bool ok = parse(src, d, e);
				uint32 n = 0;
				while (src[n]) {
					++n;
				}
				const NkString out = NkGWrite(d, NkGDetectStyle(src, n));
				check("5. LIMITE : commentaires et lignes vides ne survivent pas",
					  ok && !out.Contains("commentaire"),
					  "equivalent oui, identique octet non — la limite est nommee, pas tue");
			}

			// 6. Les exemples du document 2 (§4.1, §5, §6.3, §10) tels qu'ils sont
			//    ecrits. Un format dont la specification ne se relit pas elle-meme
			//    n'a pas de reference.
			{
				const char *src =
					"nkgui 0.2\n"
					"widgets {\n"
					"    Window \"Inspecteur\" {\n"
					"        pos = (40, 40)\n"
					"        flags = Resizable | Closable\n"
					"        SliderFloat \"X\" {\n"
					"            min = -100\n"
					"            on Commit(value) -> Callback \"T.OnPositionChanged\"(Enum.X, "
					"value)\n"
					"        }\n"
					"    }\n"
					"}\n"
					"behavior \"PreviewOpacity\" {\n"
					"    set opacityPreview = value * 100\n"
					"    if value > 0.8 {\n"
					"        Callback \"WarnHighValue\"()\n"
					"    } else {\n"
					"        set opacityPreview = value * 50\n"
					"    }\n"
					"}\n"
					"behavior \"G\" graph {\n"
					"    node n1 EventChanged\n"
					"    node n2 Multiply { a = n1.value, b = 100 }\n"
					"    wire n1.exec -> n3.exec -> n5.exec\n"
					"}\n"
					"controller \"T\" {\n"
					"    callback OnPositionChanged(axis: Enum[X,Y,Z], value: Float) -> Void\n"
					"}\n"
					"callback WarnHighValue() -> Void\n";
				uint32 n = 0;
				while (src[n]) {
					++n;
				}
				NkGDocument d;
				NkGDiag e;
				const bool ok = parse(src, d, e);
				const NkString out = NkGWrite(d, NkGDetectStyle(src, n));
				NkGDocument d2;
				NkGDiag e2;
				const bool ok2 = NkGParse(out.Data(), (uint32)out.Size(), d2, e2);
				check("6. les exemples du document 2 (§4.1, §5, §6.3, §10) font l'aller-retour",
					  ok && ok2 && NkGEqual(d, d2), ok ? "" : e.message.Data());
				check("6b. et ils reviennent identiques octet pour octet",
					  ok && out.Compare(src) == 0, "");
			}

			// 7. La precedence des operateurs. ⚠️ Une expression peut se reecrire
			//    JUSTE et se calculer FAUX : `a + b * c` reemis en `(a + b) * c` est
			//    un fichier valide, relisible, et qui ne fait plus la meme chose.
			//    C'est le defaut le plus difficile a voir d'un aller-retour.
			{
				const char *src = "nkgui 0.2\nbehavior \"P\" {\n"
								  "    set r = a + b * c\n"
								  "    set s = (a + b) * c\n"
								  "    set t = a - -3\n"
								  "}\n";
				uint32 n = 0;
				while (src[n]) {
					++n;
				}
				NkGDocument d;
				NkGDiag e;
				const bool ok = parse(src, d, e);
				const NkString out = NkGWrite(d, NkGDetectStyle(src, n));
				check("7. precedence, parentheses et nombre negatif survivent",
					  ok && out.Compare(src) == 0, ok ? "" : e.message.Data());
			}

			snprintf(buf, sizeof(buf), "\n=== CONTROLES : %u / %u ===\n", pass, total);
			rep.Append(buf);
			NkGPublish(rep, "nkuidesign_roundtrip_controles.txt");
			return (pass == total) ? 0 : 1;
		}

	} // namespace guifmt
} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_NKGUIROUNDTRIP_H__
