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
//       1. EQUIVALENCE  -- relire ce qu'on a ecrit redonne le meme document
//                          (`NkGEqual`). C'est le critere qui fait foi ;
//       2. IDENTITE OCTET -- le texte reemis est exactement le fichier d'origine.
//                          C'est plus fort, et ca depend de la mise en forme ;
//   - le style d'ecriture du fichier SOURCE est detecte et repris (largeur
//     d'indentation, fins de ligne). Un editeur qui reformate le fichier de
//     quelqu'un d'autre a chaque enregistrement produit des diffs illisibles.
//
// POURQUOI DEUX MESURES, ET POURQUOI L'ORDRE COMPTE :
//    l'identite octet seule punirait une difference d'indentation comme une
//    perte de donnee. L'equivalence seule laisserait passer un ecrivain qui
//    ecrit du charabia, du moment que son propre lecteur le relit pareil.
//    **Les deux ensemble ne laissent passer ni l'un ni l'autre.**
//
// =============================================================================
//  LA SORTIE PASSE PAR NKLOGGER  (Rodolf, 2026-08-21)
// =============================================================================
//  La version precedente de ce banc ecrivait son rapport avec `fputs`/`WriteFile`.
//  Rodolf l'a releve : **le depot journalise par NKLogger, le banc aussi.** Un
//  outil qui se fabrique sa propre sortie echappe au niveau de journalisation, au
//  fichier `logs/app.log` et aux sinks ajoutes -- et il faut se souvenir qu'il
//  existe pour aller chercher son resultat.
//
//  `NkConsoleSink` est actif par defaut en Debug (`NkLog::NkLog`) et ecrit sur
//  `stdout` par `fwrite`, ce qui marche sur une console, un tuyau ET une
//  redirection. Le detournement de `stdout` par `AllocConsole()` -- corrige le
//  2026-08-21 dans `NKWindow/EntryPoints/NkWindowsDesktop.h` -- ne s'applique
//  plus : la console n'est prise que si l'appelant n'en fournit pas.
//
//  Le rapport reste ECRIT EN FICHIER en plus, et son chemin absolu est
//  journalise : un banc dont il faut deviner ou est le resultat ne sert qu'a
//  celui qui l'a ecrit.
//
// =============================================================================
//  BASCULE DU 2026-08-22 : LE MOTEUR A CHANGE, LE FORMAT NON
// =============================================================================
//  `NkGuiFormat.h` (4185 lignes, un modele par section du langage) a ete RETIRE.
//  Ce banc mesure exactement le meme format, a travers
//  `NKSerialization/NkGui/NkGuiArchive.h` -- une couche purement syntaxique.
//
//  Le critere de bascule etait celui-la meme qui a servi a ecrire la couche :
//  **les dix fichiers du corpus, charges et reenregistres octet pour octet**.
//  Tant qu'il n'etait pas tenu, `NkGuiFormat.h` restait ; des qu'il l'a ete, il
//  est parti -- deux chemins de lecture pour un meme format finissent toujours
//  par diverger, et c'est le genre de divergence qu'on decouvre chez un
//  utilisateur.
//
//  ⚠️ TROIS CONTROLES ONT CHANGE DE SENS, ET AUCUN N'A ETE SUPPRIME :
//     - le 3 se scinde. Cinq documents fautifs sont toujours REFUSES A LA
//       LECTURE (ce que la couche ne sait pas representer) ; quatre autres --
//       `#12345`, la virgule finale, la cle de dictionnaire qui n'en est pas
//       une, la section inconnue -- sont devenus des fautes de VALIDATION. La
//       frontiere a bouge, elle n'a pas disparu, et le controle 3b la mesure ;
//     - le 20c disait « la meme source estampillee 0.3 est REFUSEE ». Un lecteur
//       syntaxique n'a aucune raison de la refuser : c'est la validation qui dit
//       desormais « section inconnue » ;
//     - le 10b garde `NkGSplitPath`, descendu dans `NkGuiValidate.h`.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKUIDESIGN_NKGUIROUNDTRIP_H__
#define __NKENTSEU_NKUIDESIGN_NKGUIROUNDTRIP_H__

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

#include "NKSerialization/NkGui/NkGuiArchive.h"

#include "NkGuiValidate.h"

namespace nkuidesign {
	namespace guifmt {

		using nkentseu::NkArchive;
		using nkentseu::NkArchiveNode;
		using nkentseu::NkDirectory;
		using nkentseu::NkFile;
		using nkentseu::NkGuiArchive;
		using nkentseu::NkGuiDiag;
		using nkentseu::NkGuiStyle;
		using nkentseu::NkGuiValueKind;

		// ====================================================================
		//  PUBLIER LE RAPPORT
		// ====================================================================

		/// Journalise le rapport LIGNE PAR LIGNE. Une ligne de journal par ligne de
		/// rapport, parce qu'un sink formate par MESSAGE : envoyer le rapport entier
		/// en un seul appel donnerait un pave sans horodatage ni niveau, et le
		/// filtrage habituel cesserait de marcher.
		inline void NkGLogReport(const NkString &rep) {
			const char *p = rep.Data();
			const uint32 n = (uint32)rep.Size();
			uint32 begin = 0;
			for (uint32 i = 0; i <= n; ++i) {
				if (i == n || p[i] == '\n') {
					uint32 e = i;
					if (e > begin && p[e - 1] == '\r') {
						--e;
					}
					const NkString line(p + begin, e - begin);
					// `{0}` et pas la ligne en format direct : le rapport contient des
					// accolades (messages de refus, extraits de source) et elles
					// seraient prises pour des marqueurs de substitution.
					logger.Info("{0}", line.Data());
					begin = i + 1;
				}
			}
		}

		inline void NkGPublish(NkString &rep, const char *fileName) {
			const nkentseu::NkPath cwd = NkDirectory::GetCurrentDirectory();
			rep.Append("\nRapport ecrit dans : ");
			rep.Append(cwd.ToString());
			rep.Append('/');
			rep.Append(fileName);
			rep.Append('\n');

			NkFile::WriteAllText(fileName, rep.Data());
			NkGLogReport(rep);
			// Le banc rend la main tout de suite apres : sans vidage explicite, les
			// dernieres lignes resteraient dans les tampons des sinks.
			logger.Flush();
		}

		/// Le style d'ecriture LU dans le fichier source. Ce n'est pas de la
		/// devinette de confort : sans lui, reecrire un document indente a deux
		/// espaces le rendrait indente a quatre, et le diff porterait sur chaque
		/// ligne du fichier au lieu de porter sur ce qui a change.
		///
		/// CE QUI A DISPARU EN v0.3 : la detection des lignes vides. Elles sont
		/// desormais LUES et conservees avec le reste de la trivia -- il n'y a plus
		/// d'heuristique a caler, donc plus d'heuristique a se tromper.
		/// LA DETECTION DE STYLE EST DESCENDUE DANS LA COUCHE. Elle y a sa place :
		/// c'est le lecteur qui sait ce qu'il a lu. Ce banc n'en garde qu'un alias,
		/// pour que les controles se relisent comme avant.
		inline NkGuiStyle NkGDetectStyle(const char *text, uint32 length) {
			return NkGuiArchive::DetectStyle(text, length);
		}

		struct NkGRoundTripResult {
				NkString file;
				bool parsed = false;
				bool equivalent = false;	 ///< le critere qui fait foi
				bool byteIdentical = false;	 ///< la mesure plus forte
				uint32 nodeCount = 0;
				uint32 firstDiffOffset = 0;	 ///< si !byteIdentical
				NkGuiDiag diag;
		};

		/// Compte les BLOCS d'une archive, en descendant.
		inline uint32 NkGCountBlocks(const NkArchive &ar) {
			const NkArchiveNode *b = ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
			if (!b || !b->IsArray()) {
				return 0;
			}
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)b->array.Size(); ++i) {
				if (b->array[i].IsObject() && b->array[i].object) {
					++n;
					n += NkGCountBlocks(*b->array[i].object);
				}
			}
			return n;
		}

		/// L'aller-retour sur UN document. `original` est le contenu du fichier, en
		/// octets, tel qu'il est sur le disque.
		///
		/// ⚠️ LES DEUX MESURES SONT DES CONTROLES DE SYMETRIE, ET AUCUNE DES DEUX NE
		///    VOIT UNE FAUTE SYMETRIQUE. Ecrire puis relire annule toute faute qui
		///    traverse l'aller ET le retour -- une tranche gardee verbatim ressort
		///    verbatim meme si l'archive l'a mal decoupee. Les controles qui
		///    regardent DANS l'archive (banc `SandboxNKArchive`, L14) sont les seuls
		///    a pouvoir le voir. Ne pas confondre « le fichier revient » avec « le
		///    document a ete compris ».
		inline NkGRoundTripResult NkGRoundTripOne(const char *original, uint32 length) {
			NkGRoundTripResult r;

			NkArchive doc1;
			if (!NkGuiArchive::Read(original, length, doc1, r.diag)) {
				return r;
			}
			r.parsed = true;
			r.nodeCount = NkGCountBlocks(doc1);

			const NkGuiStyle opt = NkGuiArchive::DetectStyle(original, length);
			const NkString emitted = NkGuiArchive::Write(doc1, opt);

			// -- Mesure 1 : l'equivalence ---------------------------------
			NkGuiDiag err2;
			NkArchive doc2;
			if (NkGuiArchive::Read(emitted.Data(), (uint32)emitted.Size(), doc2, err2)) {
				r.equivalent = NkGuiArchive::Equal(doc1, doc2);
			} else {
				// Ce cas merite d'etre distingue d'une simple inegalite : il dit que
				// l'ecrivain a produit un fichier que le lecteur REFUSE. Ce n'est pas
				// une perte de fidelite, c'est une sortie invalide.
				r.diag = err2;
				r.diag.message.Append(" [dans le texte REEMIS, pas dans le fichier source]");
			}

			// -- Mesure 2 : l'identite octet ------------------------------
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

		// ====================================================================
		//  LA COLLECTE DES FICHIERS -- UN SEUL POINT, POUR LES DEUX GESTES
		// ====================================================================
		//
		// ⚠️ LE BALAYAGE DESCEND DANS LES SOUS-DOSSIERS. Jusqu'au 2026-08-27 il
		//    ne descendait pas, et le corpus `exemples/` vit entierement dans les
		//    siens : viser le dossier racine -- LE GESTE NATUREL, celui que le
		//    README invite a faire -- rendait « TOTAL : 0 erreur(s) », un
		//    certificat de bonne sante pour quatorze fichiers dont pas un n'avait
		//    ete ouvert. Meme famille que le drapeau inconnu qui ouvrait la
		//    fenetre : **un chemin qui ne fait rien et qui en donne l'apparence.**
		//
		// ⚠️ ET LE MESSAGE D'ECHEC NE SUFFISAIT PAS -- il expliquait a
		//    l'utilisateur comment contourner l'outil, dossier par dossier. Un
		//    diagnostic qui remplace une capacite est une dette, pas une parade.
		//
		// ⚠️ UNE SEULE FONCTION POUR LES DEUX GESTES, et ce n'est pas de
		//    l'economie : `--roundtrip` et `--valider` portaient DEJA le meme
		//    message d'echec, recopie mot pour mot. Deux copies d'un mecanisme
		//    derivent, et la mesure du 23/08 (V6) a montre qu'on n'en corrige
		//    qu'une. Le controle 25b tient qu'elles ne se separent plus.
		inline NkVector<NkString> NkGCollecter(const char *directory) {
			return NkDirectory::GetFiles(directory, "*.nkgui",
										 nkentseu::NkSearchOption::NK_ALL_DIRECTORIES);
		}

		/// L'aller-retour sur tout un dossier, sous-dossiers compris.
		inline int NkGRunRoundTrip(const char *directory) {
			NkString rep("=== ALLER-RETOUR .nkgui -- le temoin du lecteur/ecrivain ===\n");
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

			NkVector<NkString> files = NkGCollecter(directory);
			if (files.Empty()) {
				rep.Append("ECHEC : aucun fichier .nkgui dans ce dossier. Rien n'a ete mesure.\n");
				rep.Append("       Le balayage DESCEND dans les sous-dossiers : il n'y a\n");
				rep.Append("       vraiment aucun .nkgui sous ce chemin. Le corpus du depot\n");
				rep.Append("       vit dans Applications/NKUIDesign/exemples.\n");
				NkGPublish(rep, "nkuidesign_roundtrip.txt");
				return 2;
			}
			uint32 total = 0;
			uint32 equivalent = 0;
			uint32 identical = 0;

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
					rep.Append("  [ECHEC ANALYSE] ");
					rep.Append(files[i]);
					rep.Append("\n      ");
					rep.Append(r.diag.code);
					rep.Append(" ligne ");
					rep.Append(NkGU32(r.diag.line));
					rep.Append(" colonne ");
					rep.Append(NkGU32(r.diag.column));
					rep.Append(" : ");
					rep.Append(r.diag.message);
					rep.Append('\n');
				} else if (!r.equivalent) {
					rep.Append("  [NON EQUIVALENT] ");
					rep.Append(files[i]);
					rep.Append(" (");
					rep.Append(NkGU32(r.nodeCount));
					rep.Append(" noeuds) -- ");
					rep.Append(r.diag.message.Empty()
								   ? NkString("le document relu differe de l'original")
								   : r.diag.message);
					rep.Append('\n');
				} else if (!r.byteIdentical) {
					rep.Append("  [OK, mise en forme differente] ");
					rep.Append(files[i]);
					rep.Append(" (");
					rep.Append(NkGU32(r.nodeCount));
					rep.Append(" noeuds), premier ecart a l'octet ");
					rep.Append(NkGU32(r.firstDiffOffset));
					rep.Append('\n');
				} else {
					rep.Append("  [OK, octet pour octet] ");
					rep.Append(files[i]);
					rep.Append(" (");
					rep.Append(NkGU32(r.nodeCount));
					rep.Append(" noeuds)\n");
				}
			}

			rep.Append("\n=== TAUX D'ALLER-RETOUR : ");
			rep.Append(NkGU32(equivalent));
			rep.Append(" / ");
			rep.Append(NkGU32(total));
			rep.Append(" equivalents -- ");
			rep.Append(NkGU32(identical));
			rep.Append(" / ");
			rep.Append(NkGU32(total));
			rep.Append(" identiques octet pour octet ===\n");

			NkGPublish(rep, "nkuidesign_roundtrip.txt");
			return (total > 0 && equivalent == total) ? 0 : 1;
		}

		// ====================================================================
		//  LA VALIDATION D'UN DOSSIER
		// ====================================================================
		//
		//  Mode SEPARE, et c'est le point : lire et juger sont deux gestes. Un
		//  dossier peut etre a 10/10 d'aller-retour ET plein de roles hors
		//  vocabulaire -- ce sont deux mesures differentes, et les melanger ferait
		//  croire qu'un fichier fidele est un fichier juste.
		inline int NkGRunValidate(const char *directory) {
			NkString rep("=== VALIDATION .nkgui -- roles et types (vocabulaire du doc 7) ===\n");
			rep.Append("dossier : ");
			rep.Append(directory ? directory : "(aucun)");
			rep.Append("\n\n");
			if (!directory || !NkDirectory::Exists(directory)) {
				rep.Append("ECHEC : le dossier n'existe pas. Rien n'a ete mesure.\n");
				NkGPublish(rep, "nkuidesign_validation.txt");
				return 2;
			}
			NkVector<NkString> files = NkGCollecter(directory);
			if (files.Empty()) {
				rep.Append("ECHEC : aucun fichier .nkgui dans ce dossier. Rien n'a ete mesure.\n");
				rep.Append("       Le balayage DESCEND dans les sous-dossiers : il n'y a\n");
				rep.Append("       vraiment aucun .nkgui sous ce chemin. Le corpus du depot\n");
				rep.Append("       vit dans Applications/NKUIDesign/exemples.\n");
				NkGPublish(rep, "nkuidesign_validation.txt");
				return 2;
			}
			uint32 totalErr = 0;
			uint32 totalWarn = 0;
			for (uint32 i = 0; i < (uint32)files.Size(); ++i) {
				NkVector<nkentseu::uint8> bytes = NkFile::ReadAllBytes(files[i].Data());
				NkArchive doc;
				NkGuiDiag err;
				if (!NkGuiArchive::Read((const char *)bytes.Data(), (uint32)bytes.Size(), doc, err)) {
					rep.Append("  [ILLISIBLE] ");
					rep.Append(files[i]);
					rep.Append(" : ");
					rep.Append(err.message);
					rep.Append('\n');
					++totalErr;
					continue;
				}
				NkVector<NkGuiDiag> diags;
				const NkGValidateResult vr = NkGValidate(doc, diags);
				totalErr += vr.errors;
				totalWarn += vr.warnings;
				rep.Append(vr.errors == 0 ? "  [OK] " : "  [FAUTES] ");
				rep.Append(files[i]);
				rep.Append(" -- ");
				rep.Append(NkGU32(vr.errors));
				rep.Append(" erreur(s), ");
				rep.Append(NkGU32(vr.warnings));
				rep.Append(" avertissement(s)\n");
				for (uint32 d = 0; d < (uint32)diags.Size() && d < 20; ++d) {
					rep.Append("      ");
					rep.Append(diags[d].code);
					rep.Append(" ligne ");
					rep.Append(NkGU32(diags[d].line));
					rep.Append(" : ");
					rep.Append(diags[d].message);
					rep.Append('\n');
				}
			}
			rep.Append("\n=== TOTAL : ");
			rep.Append(NkGU32(totalErr));
			rep.Append(" erreur(s), ");
			rep.Append(NkGU32(totalWarn));
			rep.Append(" avertissement(s) ===\n");
			NkGPublish(rep, "nkuidesign_validation.txt");
			return (totalErr == 0) ? 0 : 1;
		}

		// ====================================================================
		//  LES CONTROLES -- sans eux, « 10 / 10 » ne veut rien dire
		// ====================================================================
		//
		//  UN BANC QUI NE SAIT DIRE QUE « OUI » NE MESURE RIEN. Le taux
		//  d'aller-retour du corpus est un chiffre flatteur tant que personne n'a
		//  montre que ce banc SAIT ECHOUER. Quatre familles :
		//
		//   1. TEMOIN DE BRUIT -- la meme mesure repetee sans rien changer. Le
		//      serialiseur est deterministe : le plancher attendu est EXACTEMENT
		//      zero difference, et on le verifie au lieu de le supposer ;
		//   2. CONTROLES POSITIFS -- un changement connu DOIT etre vu. Sans eux,
		//      `NkGEqual` pourrait rendre `true` en toutes circonstances et le
		//      corpus passerait a 10/10 sans rien prouver ;
		//   3. CONTROLES NEGATIFS -- ce que le format ne dit pas doit etre REFUSE
		//      avec un message, jamais devine ;
		//   4. LES NOUVEAUTES DE LA v0.3 -- listes, dictionnaires, chemins pointes,
		//      commentaires, apparence, animation, polices, validation, et surtout
		//      **la preservation d'une section inconnue d'un fichier plus recent**.
		//      Ce dernier est le temoin de la regle (d) : sans lui, rien ne prouve
		//      qu'une evolution future ne detruira pas les documents.

		inline int NkGRunControls() {
			NkString rep("=== CONTROLES du lecteur/ecrivain .nkgui v0.3 ===\n");
			uint32 pass = 0;
			uint32 total = 0;

			auto check = [&](const char *label, bool ok, const char *detail) {
				++total;
				if (ok) {
					++pass;
				}
				rep.Append(ok ? "  [OK ] " : "  [NON] ");
				rep.Append(label);
				if (detail && *detail) {
					rep.Append(" -- ");
					rep.Append(detail);
				}
				rep.Append('\n');
			};
			auto len = [](const char *s) {
				uint32 n = 0;
				while (s[n]) {
					++n;
				}
				return n;
			};
			auto parse = [&](const char *src, NkArchive &d, NkGuiDiag &e) {
				return NkGuiArchive::Read(src, len(src), d, e);
			};
			auto rejects = [&](const char *src, const char *why) {
				NkArchive d;
				NkGuiDiag e;
				const bool ko = !parse(src, d, e);
				rep.Append("      refus attendu (");
				rep.Append(why);
				rep.Append(") : ");
				rep.Append(ko ? e.message : NkString("*** ACCEPTE ***"));
				rep.Append('\n');
				return ko;
			};
			/// L'aller-retour COMPLET sur une source litterale : equivalent ET
			/// identique octet pour octet. C'est le geste repete par presque tous les
			/// controles de la v0.3, il merite un nom.
			auto roundtrip = [&](const char *src, bool &equivalent, bool &identical,
								 NkString &why) {
				equivalent = false;
				identical = false;
				NkArchive d;
				NkGuiDiag e;
				if (!parse(src, d, e)) {
					why = NkString("refuse a la lecture : ");
					why.Append(e.message);
					return;
				}
				const NkString out = NkGuiArchive::Write(d, NkGDetectStyle(src, len(src)));
				NkArchive d2;
				NkGuiDiag e2;
				if (!NkGuiArchive::Read(out.Data(), (uint32)out.Size(), d2, e2)) {
					why = NkString("le texte REEMIS n'est pas relisible : ");
					why.Append(e2.message);
					return;
				}
				equivalent = NkGuiArchive::Equal(d, d2);
				identical = (out.Compare(NkString(src)) == 0);
				if (!identical) {
					why = NkString("mise en forme differente a la reemission");
				}
			};

			/// L'aller-retour qui exige les MEMES OCTETS et MONTRE ce qu'il a emis
			/// quand il echoue. Sans la sortie sous les yeux, un ecart de mise en
			/// forme se diagnostique a l'aveugle.
			///
			/// ⚠️ IL VIT ICI, ET NON DANS LA FAMILLE QUI L'A FAIT NAITRE (24), PARCE
			///    QUE LA FAMILLE 26 EN A EU BESOIN LE JOUR MEME. Le recopier aurait
			///    refait le piege que V6 a puni : un mecanisme present a deux endroits
			///    n'est corrige qu'a un seul.

			// L'aller-retour qui MONTRE ce qu'il a emis quand il echoue. Sans
			// la sortie sous les yeux, un ecart d'indentation se diagnostique
			// a l'aveugle.
			auto memeOctets = [&](const char *label, const char *src) {
				NkArchive d;
				NkGuiDiag e;
				if (!parse(src, d, e)) {
					check(label, false, "refuse a la lecture");
					rep.Append("      message : ");
					rep.Append(e.message);
					rep.Append('\n');
					return;
				}
				const NkString out =
					NkGuiArchive::Write(d, NkGDetectStyle(src, len(src)));
				const bool ok = (out.Compare(NkString(src)) == 0);
				check(label, ok, ok ? "" : "reemis autrement");
				if (!ok) {
					rep.Append("      --- attendu (la source, ecrite a la main) ---\n");
					rep.Append(src);
					rep.Append("      --- reemis ---\n");
					rep.Append(out);
					rep.Append("      ---\n");
				}
			};

			// 1. Le temoin de bruit.
			//
			// ⚠️ IL A ETE VERT SUR UN ECRIVAIN MORT jusqu'au 2026-08-27, et c'est
			//    la relecture d'audit qui l'a trouve, pas un echec. `a` et `b`
			//    sortent de **la meme fonction** : si `Write` rendait du vide,
			//    `a.Compare(b) == 0` compare deux chaines vides et **passe**.
			//    Mesure : `NkGuiArchive::Write` neutralisee -> **37 / 65, et le
			//    controle 1 reste VERT**.
			//
			//    C'est **mot pour mot le defaut de T5** (`SandboxNKArchive`,
			//    corrige le 2026-08-22) : la lecon avait ete appliquee au banc ou
			//    elle avait ete trouvee, **pas au banc voisin**. Regle :
			//    **au moins un cote de chaque egalite doit venir d'ailleurs que du
			//    code teste.**
			{
				NkArchive d;
				NkGuiDiag e;
				parse("nkgui 0.2\nwidgets {\n  Button \"a\" { label = \"x\" }\n}\n", d, e);
				const NkString a = NkGuiArchive::Write(d, NkGuiStyle());
				const NkString b = NkGuiArchive::Write(d, NkGuiStyle());
				// L'ancrage : du texte ECRIT A LA MAIN, qui ne peut pas sortir du
				// code teste. Sans lui, le determinisme se prouve sur du vide.
				const bool ancre = a.Size() > 0 && a.Contains("nkgui 0.2")
								   && a.Contains("Button") && a.Contains("label");
				check("1. temoin de bruit : deux ecritures donnent le meme texte -- ET "
					  "ce texte existe (sinon on prouve le determinisme du vide)",
					  ancre && a.Compare(b) == 0, "");
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
					{"2g. un COMMENTAIRE en moins est DETECTE (la trivia entre dans "
					 "l'egalite)",
					 "nkgui 0.3\nwidgets {\n // note\n B \"a\" { }\n}\n",
					 "nkgui 0.3\nwidgets {\n B \"a\" { }\n}\n"},
					{"2h. une LIGNE VIDE en moins est DETECTEE",
					 "nkgui 0.3\nwidgets {\n\n B \"a\" { }\n}\n",
					 "nkgui 0.3\nwidgets {\n B \"a\" { }\n}\n"},
					{"2i. un element de LISTE different est DETECTE",
					 "nkgui 0.3\nwidgets {\n Dropdown \"d\" { items = [\"a\", \"b\"] }\n}\n",
					 "nkgui 0.3\nwidgets {\n Dropdown \"d\" { items = [\"a\", \"c\"] }\n}\n"},
					{"2j. une CLE de dictionnaire differente est DETECTEE",
					 "nkgui 0.3\nwidgets {\n B \"a\" { m = { x = 1 } }\n}\n",
					 "nkgui 0.3\nwidgets {\n B \"a\" { m = { y = 1 } }\n}\n"},
				};
				for (uint32 i = 0; i < 10; ++i) {
					NkArchive d1;
					NkArchive d2;
					NkGuiDiag e;
					const bool ok1 = parse(kPairs[i][1], d1, e);
					const bool ok2 = parse(kPairs[i][2], d2, e);
					check(kPairs[i][0], ok1 && ok2 && !NkGuiArchive::Equal(d1, d2), "");
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

			const bool r1 = rejects(badEsc.Data(), "echappement hors des trois du doc 2 §2");
			const bool r2 = rejects("nkgui 0.2\nwidgets {\n Button \"a\" { label = \"x\" \n}\n",
									"accolade jamais fermee");
			// ⚠️ r3, r6, r8 et r9 ONT QUITTE CETTE LISTE. Ils sont mesures par le
			//    controle 3b : un lecteur purement syntaxique les LIT (ce sont des
			//    jetons nus bien formes pour lui), c'est la validation qui les juge.
			const bool r4 = rejects("widgets { }\n", "en-tete nkgui manquant");
			const bool r5 =
				rejects("nkgui 0.2\n/* jamais ferme\nwidgets { }\n", "commentaire de bloc ouvert");

			const bool r7 =
				rejects("nkgui 0.2\nwidgets {\n B \"a\" { p = }\n}\n", "valeur manquante");
			rep.Append('\n');
			const uint32 refusesLecture =
				(uint32)(r1 ? 1 : 0) + (uint32)(r2 ? 1 : 0) + (uint32)(r4 ? 1 : 0)
				+ (uint32)(r5 ? 1 : 0) + (uint32)(r7 ? 1 : 0);
			check("3. les 5 documents que la couche ne sait pas REPRESENTER sont refuses "
				  "a la lecture",
				  refusesLecture == 5, "");

			// =============================================================
			// 3b -- LES QUATRE REFUS QUI ONT CHANGE DE DOMICILE
			// =============================================================
			// ⚠️ SANS CE CONTROLE, QUATRE DIAGNOSTICS DISPARAITRAIENT SANS QUE RIEN
			//    NE TOMBE. Le fichier se lirait, se reecrirait a l'octet, et personne
			//    ne dirait que la couleur a cinq chiffres. C'est exactement la forme
			//    de perte qu'une bascule produit quand on ne compte que ce qui reste.
			{
				struct Cas {
						const char *quoi;
						const char *src;
						const char *code;
				};
				static const Cas kCas[] = {
					{"couleur a 5 chiffres",
					 "nkgui 0.3\nwidgets {\n Button \"a\" { color = #12345 }\n}\n", "E-VALEUR"},
					{"virgule finale dans une liste",
					 "nkgui 0.3\nwidgets {\n Dropdown \"d\" { items = [\"a\",] }\n}\n",
					 "E-VALEUR"},
					{"cle de dictionnaire ni identifiant ni chaine",
					 "nkgui 0.3\nwidgets {\n Table \"t\" { style = { 1 = 2 } }\n}\n",
					 "E-VALEUR"},
					{"section inconnue dans un fichier de NOTRE version",
					 "nkgui 0.3\ninconnue { }\n", "E-SECTION-INCONNUE"},
				};
				bool tous = true;
				uint32 signalesValidation = 0;
				for (uint32 i = 0; i < 4; ++i) {
					NkArchive dc;
					NkGuiDiag ec;
					const bool lu = parse(kCas[i].src, dc, ec);
					NkVector<NkGuiDiag> dg;
					NkGValidate(dc, dg);
					bool vu = false;
					for (uint32 k = 0; k < (uint32)dg.Size(); ++k) {
						if (dg[k].code.Compare(kCas[i].code) == 0) {
							vu = true;
						}
					}
					rep.Append("      faute deplacee (");
					rep.Append(kCas[i].quoi);
					rep.Append(") : ");
					rep.Append(!lu ? NkString("*** REFUSEE A LA LECTURE ***")
								   : (vu ? NkString(kCas[i].code)
										 : NkString("*** NON SIGNALEE ***")));
					rep.Append('\n');
					// LE FICHIER DOIT SE LIRE **ET** LA FAUTE ETRE VUE. Un refus a la
					// lecture serait ici un echec : il rendrait la faute incorrigible.
					tous = tous && lu && vu;
					if (lu && vu) {
						++signalesValidation;
					}
				}
				check("3b. les 4 refus qui ont change de domicile : le fichier se LIT, et la "
					  "VALIDATION nomme la faute",
					  tous, "");

				// =========================================================
				// 3c -- LE COMPTE DES REFUS, ET C'EST UNE FAMILLE ENTIERE
				// =========================================================
				// ⚠️ UNE MIGRATION CONSERVE CE QUE LES CONTROLES MESURENT, ET PERD
				//    SILENCIEUSEMENT CE QU'ILS NE MESURENT PAS -- EN PARTICULIER LA
				//    CAPACITE A REFUSER.
				//
				//    C'est ce qui a failli arriver le 2026-08-22. L'ancien lecteur
				//    refusait NEUF documents fautifs ; le nouveau en refuse CINQ.
				//    Aucun banc ne pouvait le voir : les quatre autres se lisent, se
				//    reecrivent a l'octet, et tous les controles restaient verts. **Le
				//    moteur avait juste cesse de savoir dire que la couleur a cinq
				//    chiffres**, et un refus qu'on ne fait plus ne se mesure nulle
				//    part -- il n'a pas de trace, pas de sortie, pas de ligne rouge.
				//
				//    C'est PIRE qu'un controle qui rend un faux vert : c'est une
				//    CAPACITE QUI DISPARAIT SANS LAISSER DE TRACE.
				//
				// >>> LA PARADE, ET ELLE SE POSE AVANT DE COMPARER CE QU'ON ACCEPTE :
				//     COMPTER LES REFUS DE L'ANCIEN ET DU NOUVEAU, ET EXIGER
				//     L'EGALITE. Ce controle fige ce compte a NEUF. Il ne dit pas
				//     « les fautes sont bien vues » -- 3 et 3b le disent deja -- il
				//     dit **combien de fautes ce systeme sait encore nommer**. Le
				//     jour ou ce nombre baissera, ce sera une DECISION, pas une
				//     consequence.
				check("3c. LE COMPTE DES REFUS : 9 fautes nommees, autant qu'avant la "
					  "bascule -- 5 a la lecture + 4 a la validation",
					  refusesLecture + signalesValidation == 9, "");
			}

			// 4. Ce qui DOIT passer : les trois echappements du document 2, l'UTF-8 et
			//    la chaine vide. C'est le seul endroit du serialiseur qui REGENERE une
			//    valeur -- donc le seul qui puisse la perdre.
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

				NkArchive d;
				NkGuiDiag e;
				const bool ok = NkGuiArchive::Read(src.Data(), (uint32)src.Size(), d, e);
				const NkString out = NkGuiArchive::Write(d, NkGDetectStyle(src.Data(), (uint32)src.Size()));
				NkArchive d2;
				NkGuiDiag e2;
				const bool ok2 = NkGuiArchive::Read(out.Data(), (uint32)out.Size(), d2, e2);
				check("4. les trois echappements, l'UTF-8 et la chaine vide survivent",
					  ok && ok2 && NkGuiArchive::Equal(d, d2), ok ? "" : e.message.Data());
				check("4b. et le texte reemis est identique octet pour octet",
					  ok && out.Compare(src) == 0, "");
			}

			// 5. LES COMMENTAIRES ET LES LIGNES VIDES -- decision 3 de Rodolf.
			//    C'etait la LIMITE nommee du 2026-08-21 (« ce n'est pas normal ») ;
			//    c'est maintenant un controle. Le fichier melange volontairement les
			//    six endroits ou un commentaire peut se poser : en-tete, avant un
			//    membre, en fin de ligne, sur plusieurs lignes, en fin de bloc et en
			//    pied de fichier.
			{
				const char *src = "nkgui 0.3\n"
								  "// en-tete : ce fichier est le temoin des commentaires\n"
								  "\n"
								  "widgets {\n"
								  "    // le bouton principal\n"
								  "    Button \"valider\" {\n"
								  "        label = \"Valider\"   // le libelle vu par l'usager\n"
								  "\n"
								  "        /* un commentaire\n"
								  "           sur deux lignes */\n"
								  "        on Click -> Callback \"F\"()\n"
								  "    }\n"
								  "\n"
								  "    // fin des widgets\n"
								  "}\n"
								  "\n"
								  "// pied de page\n";
				bool eq = false;
				bool id = false;
				NkString why;
				roundtrip(src, eq, id, why);
				check("5. un fichier COMMENTE fait l'aller-retour", eq, why.Data());
				check("5b. et il revient IDENTIQUE octet pour octet -- commentaires, lignes "
					  "vides et pied de fichier compris",
					  id, why.Data());
			}

			// 6. Les exemples du document 2 (§4.1, §5, §6.3, §10) tels qu'ils sont
			//    ecrits. Un format dont la specification ne se relit pas elle-meme n'a
			//    pas de reference.
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
				bool eq = false;
				bool id = false;
				NkString why;
				roundtrip(src, eq, id, why);
				check("6. les exemples du document 2 (§4.1, §5, §6.3, §10) font l'aller-retour",
					  eq, why.Data());
				check("6b. et ils reviennent identiques octet pour octet", id, why.Data());
			}

			// 7. La precedence des operateurs. Une expression peut se reecrire JUSTE
			//    et se calculer FAUX : `a + b * c` reemis en `(a + b) * c` est un
			//    fichier valide, relisible, et qui ne fait plus la meme chose. C'est
			//    le defaut le plus difficile a voir d'un aller-retour.
			{
				const char *src = "nkgui 0.2\nbehavior \"P\" {\n"
								  "    set r = a + b * c\n"
								  "    set s = (a + b) * c\n"
								  "    set t = a - -3\n"
								  "}\n";
				bool eq = false;
				bool id = false;
				NkString why;
				roundtrip(src, eq, id, why);
				check("7. precedence, parentheses et nombre negatif survivent", eq && id,
					  why.Data());
			}

			// 8-9. LES LISTES ET LES DICTIONNAIRES -- decision 1 de Rodolf. Les cinq
			//      roles qui les exigeaient (doc 9 §6.1) sont tous representes ici :
			//      `columns[]`, `items[]`, `tabs[]`, `values[]`, `sizes[]`.
			{
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "    Table \"t\" {\n"
								  "        columns = [\"Nom\", \"Type\", \"Valeur\"]\n"
								  "    }\n"
								  "    Dropdown \"d\" {\n"
								  "        items = [\"Rouge\", \"Vert\", \"Bleu\"]\n"
								  "    }\n"
								  "    TabBar \"tb\" {\n"
								  "        tabs = [\"Un\", \"Deux\"]\n"
								  "    }\n"
								  "    Chart \"c\" {\n"
								  "        values = [0, 0.5, 1, 0.25]\n"
								  "    }\n"
								  "    Grid \"g\" {\n"
								  "        sizes = [1, 2, 1]\n"
								  "        vide = []\n"
								  "        imbriquee = [[1, 2], [3, 4]]\n"
								  "    }\n"
								  "}\n";
				bool eq = false;
				bool id = false;
				NkString why;
				roundtrip(src, eq, id, why);
				check("8. LISTES : les cinq roles du doc 9 §6.1 s'ecrivent enfin, et "
					  "reviennent identiques",
					  eq && id, why.Data());
			}
			{
				const char *src =
					"nkgui 0.3\n"
					"widgets {\n"
					"    Table \"t\" {\n"
					"        entetes = { nom = \"Nom\", largeur = 120 }\n"
					"        style = { \"couleur de fond\" = #101418, marge = (4, 2) }\n"
					"        vide = { }\n"
					"        imbrique = { a = { b = [1, 2] } }\n"
					"    }\n"
					"}\n";
				bool eq = false;
				bool id = false;
				NkString why;
				roundtrip(src, eq, id, why);
				check("9. DICTIONNAIRES : cles identifiant ET cles chaine, imbrication, "
					  "aller-retour identique",
					  eq && id, why.Data());
			}

			// 10. LES CHEMINS POINTES -- decision 2. Le document 2 les emploie dans
			//     ses propres exemples et son lecteur les refusait.
			{
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "    Slider \"s\" {\n"
								  "        on Commit(value) -> Callback \"F\"(Enum.X, n1.value, "
								  "a.b.c)\n"
								  "    }\n"
								  "}\n"
								  "behavior \"G\" graph {\n"
								  "    node n2 Multiply { a = n1.value, b = 100 }\n"
								  "    node n4 Compare { a = n1.value, op = \">\", b = 0.8 }\n"
								  "    wire n1.exec -> n3.exec\n"
								  "}\n"
								  "behavior \"S\" {\n"
								  "    set r = n1.value * 100\n"
								  "    if n1.value > 0.8 {\n"
								  "        Callback \"W\"(Enum.Y)\n"
								  "    }\n"
								  "}\n";
				bool eq = false;
				bool id = false;
				NkString why;
				roundtrip(src, eq, id, why);
				check("10. `n1.value`, `Enum.X` et `a.b.c` se lisent en argument, en pin et "
					  "en expression",
					  eq && id, why.Data());
				const NkVector<NkString> segs = NkGSplitPath(NkString("a.b.c"));
				check("10b. et le chemin se decoupe en segments pour qui doit le resoudre",
					  segs.Size() == 3 && segs[0].Compare("a") == 0 && segs[2].Compare("c") == 0,
					  "");
			}

			// 11-13. L'APPARENCE, L'ANIMATION ET LES POLICES (doc 9 §3, §4, §5).
			{
				const char *src =
					"nkgui 0.3\n"
					"widgets {\n"
					"    Button \"valider\" {\n"
					"        label = \"Valider\"\n"
					"        appearance {\n"
					"            radius = 6\n"
					"            font = \"Inter\"\n"
					"            fill { color = #2F6F7A }\n"
					"            shadow \"portee\" { offset = (0, 2), blur = 6, color = "
					"#0000003A }\n"
					"        }\n"
					"        appearance(Hover) {\n"
					"            fill { color = #3A8894 }\n"
					"        }\n"
					"    }\n"
					"}\n";
				bool eq = false;
				bool id = false;
				NkString why;
				roundtrip(src, eq, id, why);
				check("11. APPARENCE : surcharges par etat, effets nommes, forme en ligne "
					  "conservee",
					  eq && id, why.Data());
			}
			{
				const char *src = "nkgui 0.3\n"
								  "animation \"bouton_valider\" {\n"
								  "    transition \"appui\" {\n"
								  "        target = \"valider\"\n"
								  "        on = Click\n"
								  "        duration = 0.12\n"
								  "        track \"scale\" {\n"
								  "            key 0.0 -> 1.0\n"
								  "            key 1.0 -> 0.96, curve = EaseOut\n"
								  "        }\n"
								  "    }\n"
								  "    ambience \"respiration\" {\n"
								  "        state = Idle\n"
								  "        repeat = 0\n"
								  "    }\n"
								  "    continuous \"reflet\" {\n"
								  "        rest = 0.0\n"
								  "        map {\n"
								  "            source = PointerX\n"
								  "            property = \"fill.angle\"\n"
								  "        }\n"
								  "    }\n"
								  "}\n";
				bool eq = false;
				bool id = false;
				NkString why;
				roundtrip(src, eq, id, why);
				check("12. ANIMATION : les trois familles, les pistes et les cles", eq && id,
					  why.Data());
			}
			{
				const char *src = "nkgui 0.3\n"
								  "fonts {\n"
								  "    font \"titre\" {\n"
								  "        family = \"Inter\"\n"
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
								  "            glyph \"A\" -> 1366\n"
								  "            glyph \"M\" -> 1774\n"
								  "        }\n"
								  "    }\n"
								  "}\n";
				bool eq = false;
				bool id = false;
				NkString why;
				roundtrip(src, eq, id, why);
				check("13. POLICES : source, chaine de repli ordonnee et empreinte de glyphes",
					  eq && id, why.Data());
			}

			// 14-17. LA VALIDATION PAR ROLE ET PAR TYPE -- decision 4.
			{
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "    VBox \"v\" {\n"
								  "        Text \"t\" { text = \"bonjour\" }\n"
								  "        TextField \"f\" { bind = email }\n"
								  "        Dropdown \"d\" { items = [\"a\"] }\n"
								  "        Item \"i\" { label = \"x\" }\n"
								  "        Progress \"p\" { bind = avancement }\n"
								  "        Button \"b\" { label = \"ok\" tooltip = \"aide\" }\n"
								  "    }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool ok = parse(src, d, e);
				NkVector<NkGuiDiag> diags;
				const NkGValidateResult vr = NkGValidate(d, diags);
				check("14. le VOCABULAIRE DU DOCUMENT 7 passe la validation sans une faute",
					  ok && vr.errors == 0 && vr.warnings == 0, ok ? "" : e.message.Data());

				// ⚠️ 14 SEUL NE PEUT PAS ECHOUER POUR UNE RAISON QUI LUI APPARTIENNE.
				//    « 0 erreur » veut dire « ce vocabulaire est accepte » OU « rien
				//    n'a ete examine », et rien dans sa ligne ne separe les deux.
				//    Mesure : `NkGValidate` qui rend `r` sans rien traverser ->
				//    **51 / 65, et 14 reste VERT**.
				//
				//    Meme forme que 25b, meme forme que T5 : **exiger une absence,
				//    c'est etre satisfait par le vide.** D'ou le temoin ci-dessous --
				//    la construction que 20c/20d et 23a/23b emploient deja, et qui
				//    manquait ici : **un positif et un negatif sur le MEME fichier.**
				const char *casse = "nkgui 0.3\n"
									 "widgets {\n"
									 "    VBox \"v\" {\n"
									 "        Mystere \"t\" { text = \"bonjour\" }\n"
									 "    }\n"
									 "}\n";
				NkArchive dk;
				NkGuiDiag ek;
				const bool okk = parse(casse, dk, ek);
				NkVector<NkGuiDiag> diagsk;
				const NkGValidateResult vrk = NkGValidate(dk, diagsk);
				check("14b. TEMOIN -- le MEME document, un seul role remplace par un nom "
					  "qui n'existe pas : la validation le VOIT. Sans lui, le vert de 14 "
					  "dirait aussi bien \"rien n'a ete examine\"",
					  okk && vrk.errors == 1, "");
			}
			{
				// UN ROLE INCONNU : ERREUR NOMMEE, ET LE FICHIER RESTE LISIBLE. C'est
				// la moitie de la decision 4 -- « jamais un rejet muet du fichier
				// entier ».
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "    Zorglub \"z\" {\n"
								  "        bidule = 1\n"
								  "    }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool lu = parse(src, d, e);
				NkVector<NkGuiDiag> diags;
				NkGValidate(d, diags);
				bool nomme = false;
				for (uint32 i = 0; i < (uint32)diags.Size(); ++i) {
					if (diags[i].code.Compare("E-ROLE-INCONNU") == 0
						&& diags[i].message.Contains("Zorglub")) {
						nomme = true;
					}
				}
				const NkString out = NkGuiArchive::Write(d, NkGDetectStyle(src, len(src)));
				check("15. un ROLE INCONNU produit une erreur NOMMEE...", lu && nomme, "");
				check("15b. ...et le document reste lisible ET reenregistrable a l'identique "
					  "(on doit pouvoir CORRIGER la faute qu'on signale)",
					  lu && out.Compare(NkString(src)) == 0, "");
			}
			{
				const char *src = "nkgui 0.3\nwidgets {\n    Combo \"c\" { items = [\"a\"] }\n}\n";
				NkArchive d;
				NkGuiDiag e;
				parse(src, d, e);
				NkVector<NkGuiDiag> diags;
				const NkGValidateResult vr = NkGValidate(d, diags);
				bool alias = false;
				for (uint32 i = 0; i < (uint32)diags.Size(); ++i) {
					if (diags[i].code.Compare("W-ROLE-ALIAS") == 0
						&& diags[i].message.Contains("Dropdown")) {
						alias = true;
					}
				}
				check("16. un ANCIEN nom (doc 7 §5) est lu, signale, et le nouveau est nomme",
					  alias && vr.errors == 0, "");
			}
			{
				const char *src = "nkgui 0.3\nwidgets {\n"
								  "    Slider \"s\" { min = \"zero\" }\n"
								  "    Text \"t\" { couleur = #FF0000 }\n"
								  "    Dropdown \"d\" { items = \"pas une liste\" }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				parse(src, d, e);
				NkVector<NkGuiDiag> diags;
				const NkGValidateResult vr = NkGValidate(d, diags);
				check("17. VALIDATION PAR TYPE : nombre attendu, propriete hors schema, liste "
					  "attendue -- 3 fautes vues",
					  vr.errors == 3, "");
			}

			// 18-20. LA VERSION -- regles (a) (b) (c) (d).
			{
				NkArchive d;
				NkGuiDiag e;
				const bool lu02 = parse("nkgui 0.2\nwidgets { }\n", d, e);
				const nkentseu::NkSchemaVersion v = NkGuiArchive::VersionOf(d);
				const bool garde02 = lu02 && v.major == 0 && v.minor == 2;
				const NkString out = NkGuiArchive::Write(d, NkGuiStyle());
				check("18. (a)(b) un fichier 0.2 se lit et se REECRIT en 0.2, jamais "
					  "reestampille",
					  garde02 && out.StartsWith("nkgui 0.2"), "");
			}
			{
				rep.Append("\n  -- refus de version --\n");
				const bool refuse =
					rejects("nkgui 1.0\nwidgets { }\n", "MAJEURE plus recente -- regle (c)");
				NkArchive d;
				NkGuiDiag e;
				parse("nkgui 1.0\nwidgets { }\n", d, e);
				rep.Append('\n');
				check("19. (c) une MAJEURE plus recente est REFUSEE, avec « ce fichier est trop "
					  "recent pour moi »",
					  refuse && e.code.Compare("E-VERSION-INCOMPATIBLE") == 0
						  && e.message.Contains("trop recent"),
					  "");
			}
			{
				// LE TEMOIN DE LA REGLE (d), ET C'EST LE CONTROLE LE PLUS IMPORTANT DU
				// FICHIER. Un document 0.4 fictif contient une section que ce lecteur
				// ne connait pas, ET un membre de widget qu'il ne connait pas non plus.
				// Les deux doivent revenir a l'octet pres.
				//
				// Sans ce controle, rien ne prouve qu'une version future du format ne
				// sera pas silencieusement effacee par un outil d'aujourd'hui qui se
				// contente d'ouvrir et d'enregistrer.
				const char *src = "nkgui 0.4\n"
								  "\n"
								  "widgets {\n"
								  "    Button \"a\" {\n"
								  "        label = \"ok\"\n"
								  "        futurMembre(x) {\n"
								  "            profondeur = 3\n"
								  "        }\n"
								  "    }\n"
								  "}\n"
								  "\n"
								  "// une section que la 0.4 a ajoutee et que je ne connais pas\n"
								  "theme \"sombre\" {\n"
								  "    fond = #101418\n"
								  "    roles {\n"
								  "        // un commentaire a l'interieur du bloc inconnu\n"
								  "        accent = #F79A28\n"
								  "    }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool lu = parse(src, d, e);
				// ⚠️ CE QUE CE CONTROLE MESURE A CHANGE DE NATURE, ET C'EST TOUT
				//    L'INTERET DE LA BASCULE. L'ancien lecteur rangeait `theme` dans
				//    une section de type `Raw` -- un mecanisme d'EXCEPTION. Le lecteur
				//    syntaxique n'a pas d'exception a declencher : `theme "sombre"` est
				//    un bloc comme un autre, avec son type, son identifiant et ses
				//    membres. La regle (d) n'est plus un mecanisme, c'est le regime
				//    normal -- alors on mesure la PRESENCE du bloc, pas celle d'un
				//    fourre-tout.
				bool sectionRaw = false;
				const NkArchiveNode *corps =
					d.FindNode(NkStringView(NkGuiArchive::KeyBody()));
				if (corps && corps->IsArray()) {
					for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
						if (corps->array[i].IsObject() && corps->array[i].object
							&& NkString(NkGuiArchive::TypeOf(*corps->array[i].object))
								   .Compare("theme")
								   == 0) {
							sectionRaw = true;
						}
					}
				}
				// Et le MEMBRE inconnu (`futurMembre(x) { ... }`), lui, n'a aucune
				// forme de bloc : il est bien garde en TRANCHE BRUTE, c'est-a-dire un
				// element SCALAIRE du corps du bouton. C'est le seul endroit ou la
				// preservation verbatim joue encore, et il faut le voir.
				bool membreBrut = false;
				if (corps && corps->IsArray() && corps->array.Size() > 0
					&& corps->array[0].object) {
					const NkArchiveNode *w =
						corps->array[0].object->FindNode(NkStringView(NkGuiArchive::KeyBody()));
					if (w && w->IsArray() && w->array.Size() > 0 && w->array[0].object) {
						const NkArchiveNode *b = w->array[0].object->FindNode(
							NkStringView(NkGuiArchive::KeyBody()));
						if (b && b->IsArray()) {
							for (uint32 k = 0; k < (uint32)b->array.Size(); ++k) {
								if (b->array[k].IsScalar()) {
									membreBrut = true;
								}
							}
						}
					}
				}
				bool eq = false;
				bool id = false;
				NkString why;
				roundtrip(src, eq, id, why);
				check("20. (d) un fichier 0.4 se LIT, sa section inconnue devient un bloc "
					  "ordinaire et son membre inconnu une tranche brute",
					  lu && sectionRaw && membreBrut, lu ? "" : e.message.Data());
				check("20b. (d) LE TEMOIN : la section inconnue et le membre inconnu sont "
					  "PRESERVES et reemis a l'octet pres, commentaires interieurs compris",
					  eq && id, why.Data());
				// Et la preuve par la negative : la meme source en 0.3 doit ECHOUER.
				// Sinon la preservation ne serait pas liee a la version, elle serait une
				// tolerance permanente -- c'est-a-dire un trou.
				// ⚠️ CE CONTROLE A CHANGE DE DOMICILE, IL N'A PAS DISPARU. Il disait
				//    « la meme source estampillee 0.3 est REFUSEE A LA LECTURE ». Un
				//    lecteur purement syntaxique n'a aucune raison de la refuser : il
				//    ne connait aucune section, donc il ne peut pas en trouver une
				//    inconnue. **C'est la validation qui doit le dire, et elle le
				//    dit** -- sinon « preserver » deviendrait « tout accepter », et une
				//    section mal orthographiee passerait sans un mot.
				NkString v03("nkgui 0.3");
				v03.Append(NkString(src).SubStr(9));
				NkArchive d3;
				NkGuiDiag e3;
				const bool lu3 = NkGuiArchive::Read(v03.Data(), (uint32)v03.Size(), d3, e3);
				NkVector<NkGuiDiag> dg3;
				NkGValidate(d3, dg3);
				bool signalee = false;
				for (uint32 i = 0; i < (uint32)dg3.Size(); ++i) {
					if (dg3[i].code.Compare("E-SECTION-INCONNUE") == 0) {
						signalee = true;
					}
				}
				check("20c. (d) la MEME source estampillee 0.3 se LIT, et sa section "
					  "inconnue est SIGNALEE par la validation : preserver n'est pas "
					  "tout accepter",
					  lu3 && signalee, lu3 ? "" : e3.message.Data());
				// Et la contre-epreuve, sans laquelle le signalement pourrait etre
				// permanent : en 0.4 -- une mineure plus recente que la mienne -- la
				// meme section ne doit PAS etre signalee. C'est la regle (d).
				NkVector<NkGuiDiag> dg4;
				NkGValidate(d, dg4);
				bool signalee04 = false;
				for (uint32 i = 0; i < (uint32)dg4.Size(); ++i) {
					if (dg4[i].code.Compare("E-SECTION-INCONNUE") == 0) {
						signalee04 = true;
					}
				}
				check("20d. (d) LE TEMOIN : en 0.4, la MEME section n'est PAS signalee -- "
					  "la compatibilite ascendante n'est pas une faute",
					  !signalee04, "");
			}

			// =============================================================
			// 21 -- LE CANARI DE L'ETAPE 4 : `$` N'EST PAS UN IDENTIFIANT
			// =============================================================
			// Ce controle ne mesure RIEN pour le lecteur `.nkgui` lui-meme. Il
			// garde une hypothese que la couche `.nkgui` sur `NkArchive`
			// (etape 4, `SandboxNKArchive` T10) EMPRUNTE a ce lexeur.
			//
			// Cette couche represente l'identite syntaxique d'un noeud
			// (`VBox "v"`) par des entrees a cle RESERVEE `$type` / `$id`, que
			// l'ecrivain consomme pour composer l'en-tete du bloc au lieu de les
			// emettre comme des lignes `cle = valeur`. Le procede n'est sur que
			// parce qu'une cle reservee ne peut jamais entrer en collision avec
			// un vrai nom de propriete -- et ca, ce n'est pas l'archive qui le
			// garantit, c'est CE lexeur : un identifiant est
			// `[A-Za-z_][A-Za-z0-9_]*` (`NkGIsAlpha`), donc `$` en est exclu.
			//
			// ⚠️ UNE GARANTIE EMPRUNTEE DOIT ETRE GARDEE LA OU ELLE EST PRODUITE.
			//    Le jour ou quelqu'un ajoutera `$` aux identifiants -- pour des
			//    variables, pour une interpolation, peu importe -- il le fera
			//    ici, dans ce fichier, et il n'aura aucune raison de penser a une
			//    couche d'archive qui vit ailleurs. C'est ce controle qui l'en
			//    avertira, a l'endroit exact ou il travaille.
			//
			//    Sans lui, la collision serait SILENCIEUSE : une propriete
			//    legitimement nommee `$type` serait avalee par l'en-tete du bloc
			//    et disparaitrait du fichier reecrit.
			{
				check("21. (etape 4) `$` est refuse comme debut d'identifiant : la cle "
					  "reservee $type ne peut pas entrer en collision avec une propriete",
					  rejects("nkgui 0.3\nwidgets {\n  VBox \"v\" {\n    $type = 1\n  }\n}\n",
							  "`$` n'est pas un caractere d'identifiant"),
					  "");
				// La contre-epreuve, sinon le refus ci-dessus pourrait venir de
				// n'importe quelle autre faute de cette source : le MEME fichier
				// avec un nom legal doit passer.
				NkArchive dOk;
				NkGuiDiag eOk;
				const bool ok = parse("nkgui 0.3\nwidgets {\n  VBox \"v\" {\n    type = 1\n  }\n}\n",
									  dOk, eOk);
				check("21b. (temoin) le MEME fichier avec un nom legal est ACCEPTE : le refus "
					  "de 21 vient bien du `$`",
					  ok, ok ? "" : eOk.message.Data());
			}

			// =============================================================
			// 22 -- UN DIAGNOSTIC DIT **OU ALLER**, PAS SEULEMENT **QUOI**
			// =============================================================
			// ATTENTION -- CE CONTROLE EXISTE PARCE QUE J'AVAIS LIVRE LE CHEMIN SEUL.
			//    A la bascule, les diagnostics ont perdu leur numero de ligne et j'ai
			//    presente ca comme un echange acceptable : le chemin
			//    (`widgets / Button "x" . color`) designe le noeud et survit aux
			//    modifications, la ligne non.
			//
			//    C'etait vrai et c'etait insuffisant. **Le chemin dit QUOI, la ligne
			//    dit OU ALLER**, et celui qui corrige un `.nkgui` l'a ouvert dans un
			//    editeur de texte. Ce sont deux questions differentes, donc l'une ne
			//    remplace pas l'autre.
			//
			//    ⚠️ Et l'information n'etait pas PERDUE, elle n'etait pas TRANSPORTEE :
			//       le lecteur connait la ligne au moment ou il analyse. Cout chiffre
			//       avant d'ecrire, puis MESURE : le champ tient dans le rembourrage
			//       que la trivia portait deja -- `sizeof` 200 avec, 200 sans.
			//       **Zero octet.** Quand une information utile ne coute rien,
			//       « c'est un echange » est une facon de ne pas la porter.
			{
				//  1: nkgui 0.3
				//  2: widgets {
				//  3:   Button "ok" {
				//  4:     label = "x"
				//  5:
				//  6:     // un commentaire
				//  7:     color = #12345      <-- la faute
				//  8:   }
				//  9:   Inconnu "z" { }       <-- role inconnu
				// 10: }
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "  Button \"ok\" {\n"
								  "    label = \"x\"\n"
								  "\n"
								  "    // un commentaire\n"
								  "    color = #12345\n"
								  "  }\n"
								  "  Inconnu \"z\" { }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool lu = parse(src, d, e);
				NkVector<NkGuiDiag> dg;
				NkGValidate(d, dg);

				uint32 ligneValeur = 0;
				uint32 ligneRole = 0;
				bool cheminValeur = false;
				for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
					if (dg[i].code.Compare("E-VALEUR") == 0) {
						ligneValeur = dg[i].line;
						cheminValeur = dg[i].message.Contains("Button \"ok\" . color");
					}
					if (dg[i].code.Compare("E-ROLE-INCONNU") == 0) {
						ligneRole = dg[i].line;
					}
				}
				// LES LIGNES SONT ECRITES A LA MAIN, sinon ce controle mesurerait le
				// code teste avec lui-meme. Ni la ligne vide ni le commentaire ne
				// comptent pour la propriete : ils appartiennent a sa trivia de tete.
				check("22. un diagnostic de PROPRIETE porte la ligne exacte (7) ET le chemin",
					  lu && ligneValeur == 7 && cheminValeur, "");
				check("22b. un diagnostic de BLOC porte la ligne du bloc (9)",
					  lu && ligneRole == 9, "");
			}

			// =============================================================
			// 22c -- LA VALEUR **BIEN FORMEE MAIS DU MAUVAIS TYPE**
			// =============================================================
			// ⚠️ CE CONTROLE EXISTE PARCE QU'UNE MUTATION A SURVECU. « V6 -- le
			//    diagnostic d'une PROPRIETE perd sa ligne » restait VERTE : 22 la
			//    croyait couverte, elle ne l'etait pas.
			//
			//    La raison est qu'il y a DEUX portes vers un diagnostic de propriete,
			//    et 22 n'en franchit qu'une. `color = #12345` est mal formee : elle
			//    sort par la porte « aucune forme de valeur du format ». Une valeur
			//    BIEN formee du MAUVAIS type -- `maxLines = "trois"` la ou un nombre
			//    est attendu -- sort par l'autre, celle de `NkGValueMatches`. C'est
			//    celle-la que la mutation visait, et aucun controle n'y passait.
			//
			// >>> LA LECON, ET ELLE VAUT AU-DELA D'ICI : **UN MECANISME PRESENT A
			//     PLUSIEURS ENDROITS DOIT ETRE MESURE A CHACUN.** Un seul site
			//     verifie donne la sensation d'avoir couvert la fonction, pas la
			//     couverture. La ligne se pose a cinq endroits dans ce fichier ;
			//     22, 22b et 22c en franchissent trois -- les deux autres
			//     (`W-ROLE-ALIAS` et `E-SECTION-INCONNUE`) partagent leur ligne avec
			//     un site deja mesure.
			{
				//  1: nkgui 0.3
				//  2: widgets {
				//  3:   Text "t" {
				//  4:     text = "ok"
				//  5:     maxLines = "trois"   <-- bien formee, mais un nombre est attendu
				//  6:   }
				//  7: }
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "  Text \"t\" {\n"
								  "    text = \"ok\"\n"
								  "    maxLines = \"trois\"\n"
								  "  }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool lu = parse(src, d, e);
				NkVector<NkGuiDiag> dg;
				NkGValidate(d, dg);

				uint32 ligneType = 0;
				bool cheminType = false;
				uint32 combien = 0;
				for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
					if (dg[i].code.Compare("E-TYPE") == 0) {
						++combien;
						ligneType = dg[i].line;
						cheminType = dg[i].message.Contains("Text \"t\" . maxLines");
					}
				}
				// LA LIGNE EST ECRITE A LA MAIN, et elle vaut 5 -- ni 7 ni 9, pour
				// qu'un compteur bloque sur une constante ne puisse pas passer les
				// trois controles a la fois.
				check("22c. une valeur BIEN FORMEE mais du MAUVAIS TYPE porte sa ligne (5) "
					  "ET son chemin -- l'autre porte vers un diagnostic de propriete",
					  lu && combien == 1 && ligneType == 5 && cheminType, "");
			}


			// =============================================================
			// 23 -- L'APPARENCE EST JUGEE DANS **SON** VOCABULAIRE
			// =============================================================
			// ⚠️ CES CONTROLES EXISTENT PARCE QUE LA VALIDATION SE TROMPAIT, ET QUE
			//    LE LECTEUR AVAIT RAISON. `appearance`, `fill` et `shadow` sont des
			//    constructions du document 9 §3, pas des roles de widget. La
			//    validation les traversait comme des roles et rendait
			//    `E-ROLE-INCONNU` sur chacun -- trois faux positifs sur un fichier
			//    parfaitement legal, dont l'aller-retour etait deja octet pour octet.
			//
			// >>> ET LE PIEGE DE CE CORRECTIF EST PLUS DANGEREUX QUE LE DEFAUT : un
			//     validateur d'apparence qui ne ferait RIEN ferait disparaitre les
			//     trois faux positifs tout aussi bien, et 23a serait VERT. C'est
			//     exactement la forme du 2026-08-22 -- **une capacite de refuser qui
			//     disparait sans laisser de trace**. 23b, 23c et 23f sont donc des
			//     controles POSITIFS du refus : ils ne demandent pas « le faux positif
			//     a-t-il disparu », ils demandent **« que sait encore refuser ce
			//     vocabulaire-la ».**
			{
				//  1: nkgui 0.3
				//  2: widgets {
				//  3:   Button "valider" {
				//  4:     label = "Valider"
				//  5:     appearance {
				//  6:       radius = 6
				//  7:       font = "Inter"
				//  8:       fill { color = #2F6F7A }
				//  9:       shadow "portee" { offset = (0, 2), blur = 6 }
				// 10:     }
				// 11:     appearance(Hover) {
				// 12:       fill { color = #3A8894 }
				// 13:     }
				// 14:   }
				// 15: }
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "  Button \"valider\" {\n"
								  "    label = \"Valider\"\n"
								  "    appearance {\n"
								  "      radius = 6\n"
								  "      font = \"Inter\"\n"
								  "      fill { color = #2F6F7A }\n"
								  "      shadow \"portee\" { offset = (0, 2), blur = 6 }\n"
								  "    }\n"
								  "    appearance(Hover) {\n"
								  "      fill { color = #3A8894 }\n"
								  "    }\n"
								  "  }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool lu = parse(src, d, e);
				NkVector<NkGuiDiag> dg;
				NkGValidate(d, dg);
				const NkString out = NkGuiArchive::Write(
					d, NkGDetectStyle(src, (uint32)NkString(src).Size()));

				// LE FICHIER EST LEGAL : aucun diagnostic, et il revient a l'octet.
				// Les deux comptent -- une validation muette sur un document que le
				// lecteur abime ne vaudrait rien.
				check("23a. `appearance`, `fill` et `shadow` ne sont plus pris pour des "
					  "roles : 0 diagnostic sur un fichier legal, et l'octet est rendu",
					  lu && dg.Size() == 0 && out.Compare(NkString(src)) == 0,
					  lu ? "" : e.message.Data());
			}

			{
				//  1: nkgui 0.3
				//  2: widgets {
				//  3:   Button "b" {
				//  4:     appearance {
				//  5:       rayon = 6            <-- nom hors schema d'appearance
				//  6:       fill { colour = #FFFFFF }   <-- nom hors schema de fill
				//  7:       shadow { offset = 4 }       <-- Vec2 attendu, nombre lu
				//  8:       glow { }                    <-- effet hors liste fermee
				//  9:     }
				// 10:   }
				// 11: }
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "  Button \"b\" {\n"
								  "    appearance {\n"
								  "      rayon = 6\n"
								  "      fill { colour = #FFFFFF }\n"
								  "      shadow { offset = 4 }\n"
								  "      glow { }\n"
								  "    }\n"
								  "  }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool lu = parse(src, d, e);
				NkVector<NkGuiDiag> dg;
				NkGValidate(d, dg);

				// LES LIGNES SONT ECRITES A LA MAIN -- 5, 6, 7, 8 -- sinon ce controle
				// mesurerait le code teste avec lui-meme. Le mecanisme de la ligne a
				// CHANGE DE DOMICILE (il se pose desormais dans l'apparence aussi),
				// donc il change de harnais avec lui.
				bool p5 = false, p6 = false, p7 = false, p8 = false;
				for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
					const NkGuiDiag &g = dg[i];
					if (g.code.Compare("E-TYPE") == 0 && g.line == 5
						&& g.message.Contains("appearance . rayon")) {
						p5 = true;
					}
					if (g.code.Compare("E-TYPE") == 0 && g.line == 6
						&& g.message.Contains("appearance / fill . colour")) {
						p6 = true;
					}
					if (g.code.Compare("E-TYPE") == 0 && g.line == 7
						&& g.message.Contains("appearance / shadow . offset")) {
						p7 = true;
					}
					if (g.code.Compare("E-EFFET-INCONNU") == 0 && g.line == 8
						&& g.message.Contains("appearance / glow")) {
						p8 = true;
					}
				}
				check("23b. LE REFUS EXISTE : nom hors schema d'`appearance` (5), hors "
					  "schema de `fill` (6), mauvais type dans `shadow` (7), effet hors "
					  "liste fermee (8) -- chacun avec sa ligne ET son chemin",
					  lu && dg.Size() == 4 && p5 && p6 && p7 && p8, "");
			}

			{
				//  1: nkgui 0.3
				//  2: widgets {
				//  3:   Button "b" {
				//  4:     appearance {
				//  5:       Button "dedans" { }     <-- un widget dans une apparence
				//  6:       fill { shadow { } }     <-- un bloc dans un effet
				//  7:     }
				//  8:   }
				//  9:   Mystere "z" { }             <-- un role VRAIMENT inconnu
				// 10: }
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "  Button \"b\" {\n"
								  "    appearance {\n"
								  "      Button \"dedans\" { }\n"
								  "      fill { shadow { } }\n"
								  "    }\n"
								  "  }\n"
								  "  Mystere \"z\" { }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool lu = parse(src, d, e);
				NkVector<NkGuiDiag> dg;
				NkGValidate(d, dg);

				bool widgetRefuse = false, blocRefuse = false, roleRefuse = false;
				for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
					const NkGuiDiag &g = dg[i];
					// ⚠️ UN WIDGET DANS UNE APPARENCE SORT EN `E-EFFET-INCONNU`, PAS EN
					//    `E-ROLE-INCONNU`. La grammaire du document 9 §3.1 dit
					//    `appearance_member := prop_decl | effect_blk` : `Button` est un
					//    role parfaitement connu, il est simplement AU MAUVAIS ENDROIT.
					//    Se plaindre du vocabulaire des roles ici serait refaire, en
					//    plus discret, l'erreur que ce chantier corrige.
					if (g.code.Compare("E-EFFET-INCONNU") == 0 && g.line == 5
						&& g.message.Contains("appearance / Button")) {
						widgetRefuse = true;
					}
					if (g.code.Compare("E-EFFET-INCONNU") == 0 && g.line == 6
						&& g.message.Contains("fill / shadow")) {
						blocRefuse = true;
					}
					if (g.code.Compare("E-ROLE-INCONNU") == 0 && g.line == 9
						&& g.message.Contains("Mystere")) {
						roleRefuse = true;
					}
				}
				check("23c. LA CAPACITE A REFUSER N'A PAS DEMENAGE : un widget dans une "
					  "apparence (5) et un bloc dans un effet (6) sont refuses, et un "
					  "role VRAIMENT inconnu rend TOUJOURS `E-ROLE-INCONNU` (9)",
					  lu && dg.Size() == 3 && widgetRefuse && blocRefuse && roleRefuse, "");
			}

			{
				// LES CAPACITES TRANSVERSALES NE FUIENT PAS DANS L'APPARENCE.
				// `tooltip` et `enabled` sont admis sur TOUT ROLE (document 7 §4) --
				// c'est une propriete du widget, pas du dessin. Les laisser passer
				// dans un `fill` aurait rendu la table d'apparence plus permissive que
				// le document, et personne ne l'aurait vu : un contournement de schema
				// ne produit aucune sortie.
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "  Button \"b\" {\n"
								  "    tooltip = \"admis ici\"\n"
								  "    appearance {\n"
								  "      fill { tooltip = \"pas ici\" }\n"
								  "    }\n"
								  "  }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool lu = parse(src, d, e);
				NkVector<NkGuiDiag> dg;
				NkGValidate(d, dg);

				bool refuseDansFill = false;
				for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
					if (dg[i].code.Compare("E-TYPE") == 0 && dg[i].line == 6
						&& dg[i].message.Contains("fill . tooltip")) {
						refuseDansFill = true;
					}
				}
				check("23d. `tooltip` reste admis sur le WIDGET et refuse dans un `fill` : "
					  "les capacites transversales du document 7 §4 ne fuient pas dans le "
					  "vocabulaire d'apparence",
					  lu && dg.Size() == 1 && refuseDansFill, "");
			}

			{
				// ⚠️ LA LIMITE EST FERMEE -- ET CE CONTROLE EST L'ANCIEN, RETOURNE.
				//    Jusqu'au 2026-08-27 il FIGEAIT l'asymetrie : la meme faute,
				//    ecrite dans `appearance` et dans `appearance(Hover)`, sortait
				//    **une fois et une seule**, parce que la seconde restait une
				//    tranche verbatim que la validation ne regardait pas.
				//
				//    Il exige maintenant **DEUX** diagnostics. Le meme fichier, la
				//    meme faute, le chiffre inverse : c'est la mesure de la fermeture,
				//    pas une nouvelle affirmation.
				//
				// ⚠️ ET IL VERIFIE QUE LES DEUX PORTENT LEUR LIGNE **ET** LEUR ETAT.
				//    Deux diagnostics au chemin identique sur un widget qui a trois
				//    apparences obligeraient a compter les lignes pour savoir lequel
				//    est en cause -- on aurait ferme la limite en creant une gene.
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "  Button \"b\" {\n"
								  "    appearance { glow { } }\n"
								  "    appearance(Hover) { glow { } }\n"
								  "  }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool lu = parse(src, d, e);
				NkVector<NkGuiDiag> dg;
				NkGValidate(d, dg);

				uint32 effets = 0;
				bool surLeNu = false;
				bool surLEtat = false;
				for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
					if (dg[i].code.Compare("E-EFFET-INCONNU") != 0) {
						continue;
					}
					++effets;
					if (dg[i].line == 4 && !dg[i].message.Contains("appearance(")) {
						surLeNu = true;
					}
					if (dg[i].line == 5 && dg[i].message.Contains("appearance(Hover)")) {
						surLEtat = true;
					}
				}
				check("23e. LIMITE FERMEE (2026-08-27) : la meme faute est vue DES DEUX "
					  "COTES -- `appearance` (4) et `appearance(Hover)` (5) -- chacune avec "
					  "sa ligne ET son etat dans le chemin. 2 diagnostics, plus 1",
					  lu && effets == 2 && surLeNu && surLEtat && dg.Size() == 2, "");
			}

			{
				// =========================================================
				// 23f -- LE COMPTE DES REFUS DU VOCABULAIRE D'APPARENCE
				// =========================================================
				// Meme parade que le controle 3c, sur le vocabulaire neuf. 3c fige a
				// NEUF ce que le format sait refuser depuis la bascule ; celui-ci fige
				// a SIX ce que l'apparence sait refuser depuis aujourd'hui.
				//
				// Il ne dit pas « les fautes sont bien vues » -- 23b, 23c et 23d le
				// disent deja. Il dit **combien de fautes ce vocabulaire sait nommer**,
				// et le jour ou ce nombre baissera, ce sera une DECISION, pas une
				// consequence. Un schema qu'on elargit « parce qu'un fichier reel ne
				// passait pas » se relache toujours d'un cran de plus que necessaire.
				const char *src = "nkgui 0.3\n"
								  "widgets {\n"
								  "  Button \"b\" {\n"
								  "    appearance {\n"
								  "      rayon = 6\n"
								  "      fill { color = #2F6F7A, colour = #FFFFFF }\n"
								  "      shadow { offset = 4 }\n"
								  "      glow { color = #FFFFFF }\n"
								  "      fill { shadow { blur = 2 } }\n"
								  "      Button \"dedans\" { label = \"x\" }\n"
								  "    }\n"
								  "  }\n"
								  "}\n";
				NkArchive d;
				NkGuiDiag e;
				const bool lu = parse(src, d, e);
				NkVector<NkGuiDiag> dg;
				NkGValidate(d, dg);

				uint32 bloquants = 0;
				for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
					if (dg[i].code.StartsWith("E-")) {
						++bloquants;
					}
				}
				check("23f. LE COMPTE DES REFUS D'APPARENCE : 6 fautes nommees sur un "
					  "fichier qui les cumule -- le jour ou ce nombre baisse, c'est une "
					  "decision",
					  lu && bloquants == 6, "");
			}

			// ================================================================
			//  24. L'INDENTATION -- ce que le fichier disait, et que le modele
			//      n'a pas a savoir. La derniere face des trois manques.
			// ================================================================
			//
			//  ⚠️ AUCUN DES 50 CONTROLES PRECEDENTS NE POUVAIT VOIR CE DEFAUT,
			//     ET LE CORPUS NON PLUS : ils indentent tous partout pareil.
			//     Un corpus de dix fichiers du meme moule mesure le moule, pas
			//     le format. Chaque source ci-dessous est donc ecrite A LA MAIN
			//     et VOLONTAIREMENT irreguliere -- c'est elle qui fait foi, et
			//     elle ne peut pas venir du code teste.
			//
			//  Sept chemins DIFFERENTS, un par controle, parce que le meme
			//  mecanisme vit a plusieurs endroits (`OpenMember` et `CloseLine`)
			//  et qu'un seul controle en aurait laisse la moitie dans le noir.
			//
			//  ⚠️ ET DEUX TEMOINS QUI TIENNENT L'AUTRE MOITIE DE LA REGLE
			//     (24h, 24i). Sans eux, un ecrivain qui n'indenterait PLUS RIEN
			//     serait vert de 24a a 24g. « une ligne qui vient du fichier
			//     garde ses octets » ne vaut que collee a « une ligne creee par
			//     l'editeur recoit une indentation generee ».
			{

				// [A] Une largeur qui n'est pas `profondeur x cran`. Le cran deduit
				//     vaut 2 (la ligne `  VBox`), la propriete en demande 8.
				memeOctets("24a. [A] une propriete indentee AUTREMENT que "
						   "`profondeur x cran` garde ses octets",
						   "nkgui 0.3\n"
						   "widgets {\n"
						   "  VBox \"v\" {\n"
						   "        gap = 4\n"
						   "  }\n"
						   "}\n");

				// [B] Une indentation qui n'est meme pas faite d'espaces.
				memeOctets("24b. [B] une indentation au TABULATEUR garde ses octets",
						   "nkgui 0.3\n"
						   "widgets {\n"
						   "  VBox \"v\" {\n"
						   "\tgap = 4\n"
						   "  }\n"
						   "}\n");

				// [E] La trivia de tete porte DEJA des lignes completes (une ligne
				//     vide, un commentaire). Ce n'est pas le meme chemin que [A] :
				//     la trivia finit par un saut de ligne dans un cas, par de
				//     l'espace dans l'autre.
				memeOctets("24c. [E] une indentation qui SUIT une ligne vide et un "
						   "commentaire garde ses octets",
						   "nkgui 0.3\n"
						   "widgets {\n"
						   "  VBox \"v\" {\n"
						   "\n"
						   "      // une note\n"
						   "      gap = 4\n"
						   "  }\n"
						   "}\n");

				// [F] L'ACCOLADE FERMANTE. Elle ne passe pas par `OpenMember` mais
				//     par `CloseLine` -- le meme mecanisme, un second domicile.
				//     C'est la lecon de V6 : n'en corriger qu'un donne un vert a
				//     moitie merite.
				memeOctets("24d. [F] l'ACCOLADE FERMANTE indentee autrement garde "
						   "ses octets (`CloseLine`, pas `OpenMember`)",
						   "nkgui 0.3\n"
						   "widgets {\n"
						   "  VBox \"v\" {\n"
						   "    gap = 4\n"
						   "     }\n"
						   "}\n");

				// [G] Une TRANCHE BRUTE : elle ne passe pas par le modele, mais
				//     elle passe bien par `OpenMember`.
				memeOctets("24e. [G] une TRANCHE BRUTE indentee autrement que le "
						   "reste du document garde ses octets",
						   "nkgui 0.3\n"
						   "widgets {\n"
						   "  Slider \"n1\" { min = 0 }\n"
						   "}\n"
						   "\n"
						   "behavior \"seuil\" {\n"
						   "      set r = 1\n"
						   "}\n");

				// [D] ZERO indentation dans un bloc imbrique. ⚠️ Sa trivia de tete
				//     est VIDE -- exactement celle d'un noeud que l'editeur vient
				//     de creer. Les deux cas sont indiscernables dans la trivia,
				//     et 24i exige justement que le second soit indente.
				memeOctets("24f. [D] une ligne COLLEE A LA MARGE dans un bloc "
						   "imbrique garde ses octets",
						   "nkgui 0.3\n"
						   "widgets {\n"
						   "  VBox \"v\" {\n"
						   "gap = 4\n"
						   "  }\n"
						   "}\n");

				// Le meme cas pour la fermante : `}` colle a la marge.
				memeOctets("24g. [D+F] une ACCOLADE FERMANTE collee a la marge "
						   "garde ses octets",
						   "nkgui 0.3\n"
						   "widgets {\n"
						   "  VBox \"v\" {\n"
						   "    gap = 4\n"
						   "}\n"
						   "}\n");

				// ⚠️ TEMOIN 1 : L'ACQUIS. Un document indente uniformement doit
				//    rester octet pour octet. C'est ce que les 50 controles
				//    precedents mesuraient deja, mais il faut le redire ICI :
				//    c'est le seul acquis que le correctif d'indentation peut
				//    casser, et il doit pouvoir rougir dans la meme page.
				memeOctets("24h. TEMOIN -- un document indente UNIFORMEMENT reste "
						   "octet pour octet (l'acquis que ce correctif peut casser)",
						   "nkgui 0.3\n"
						   "widgets {\n"
						   "  VBox \"v\" {\n"
						   "    gap = 4\n"
						   "    align = Start\n"
						   "  }\n"
						   "}\n");

				// ⚠️ TEMOIN 2 : L'AUTRE MOITIE DE LA REGLE, et le seul controle de
				//    la famille dont l'attendu N'EST PAS la source.
				//
				//    `{gap = 4` : la trivia de tete de `gap` est VIDE. C'est le cas
				//    du noeud NEUF -- celui que l'editeur pose et qui n'a jamais
				//    connu de fichier. Il DOIT recevoir une indentation generee.
				//
				//    Sans ce controle, « ne plus jamais indenter » passerait la
				//    famille entiere. C'est la forme exacte du piege de V6 et du
				//    22/08 : une capacite qui s'en va sans laisser de trace.
				{
					const char *src = "nkgui 0.3\n"
									  "widgets {\n"
									  "  VBox \"v\" {gap = 4\n"
									  "  }\n"
									  "}\n";
					// Attendu ECRIT A LA MAIN. Il ne sort pas du code teste.
					const char *attendu = "nkgui 0.3\n"
										  "widgets {\n"
										  "  VBox \"v\" {\n"
										  "    gap = 4\n"
										  "  }\n"
										  "}\n";
					NkArchive d;
					NkGuiDiag e;
					const bool lu = parse(src, d, e);
					NkString out;
					if (lu) {
						out = NkGuiArchive::Write(d, NkGDetectStyle(src, len(src)));
					}
					const bool ok = lu && (out.Compare(NkString(attendu)) == 0);
					check("24i. TEMOIN -- une trivia de tete VIDE (le noeud NEUF) "
						  "recoit bien une indentation GENEREE",
						  ok, "");
					if (!ok) {
						rep.Append("      --- attendu (ecrit a la main) ---\n");
						rep.Append(attendu);
						rep.Append("      --- reemis ---\n");
						rep.Append(out);
						rep.Append("      ---\n");
					}
				}

				// ⚠️ LE TERMINATEUR DE LIGNE N'EST PLUS DEDUIT, IL EST TRANSPORTE.
				//    Avant le 2026-08-27 le `\\r` etait REGENERE depuis
				//    `style.crlf` -- une deduction GLOBALE (la premiere ligne du fichier)
				//    appliquee a un fait LOCAL. Le `lead` porte desormais les octets
				//    exacts du terminateur, donc la deduction ne sert plus qu'aux lignes
				//    NEUVES. Ces deux controles sont la contrepartie de cette promesse ;
				//    sans eux, elle n'existerait que dans un commentaire.
				memeOctets("24j. le TERMINATEUR : un document en CRLF, indente a la main, "
						   "revient a l'octet",
						   "nkgui 0.3\r\n"
						   "widgets {\r\n"
						   "  VBox \"v\" {\r\n"
						   "        gap = 4\r\n"
						   "     }\r\n"
						   "}\r\n");

				// ⚠️ MIXTE. Aucun fichier sain ne devrait l'etre -- mais une fusion mal
				//    reglee en fabrique, et c'est PRECISEMENT le cas qu'une deduction
				//    globale ne peut pas rendre : elle en choisit UN pour tout le
				//    document. Un outil qui « repare » les fins de ligne d'un fichier
				//    qu'on lui demandait juste d'ouvrir rend tout diagnostic impossible,
				//    et fait exploser un diff qui n'aurait du porter qu'une ligne.
				memeOctets("24k. le TERMINATEUR : un document aux fins de ligne MIXTES "
						   "n'est pas \"repare\" en douce",
						   "nkgui 0.3\n"
						   "widgets {\r\n"
						   "  VBox \"v\" {\n"
						   "    gap = 4\r\n"
						   "    align = Start\n"
						   "  }\r\n"
						   "}\n");
			}

			// ================================================================
			//  25. LE BALAYAGE DESCEND DANS LES SOUS-DOSSIERS
			// ================================================================
			//
			//  ⚠️ CELUI-LA TOUCHE LE DISQUE, ET C'EST LE SEUL DU BANC. Il ne
			//     peut pas faire autrement : ce qu'il mesure EST un parcours de
			//     systeme de fichiers. Le mesurer en memoire reviendrait a
			//     tester une imitation de `NkDirectory` -- exactement la forme
			//     « la meme fonction produit l'attendu et l'observe » qui avait
			//     rendu T5 vert sur un ecrivain mort.
			//
			//     Il fabrique donc son propre arbre dans le dossier courant, le
			//     mesure, et le retire. **Il ne lit pas le corpus du depot** :
			//     un banc qui depend d'un chemin relatif ne tourne plus des
			//     qu'on le lance d'ailleurs.
			{
				const char *racine = "nkuidesign_ctl25";
				const char *sous = "nkuidesign_ctl25/a/b";
				NkDirectory::Delete(racine, true);
				const bool arbre = NkDirectory::CreateRecursive(sous);
				const char *doc = "nkgui 0.3\nwidgets {\n  Group \"g\" { }\n}\n";
				// ⚠️ LE FICHIER DU FOND EST DELIBEREMENT ILLISIBLE (pas d'en-tete
				//    `nkgui`). C'est LUI qui ancre 25b, et la raison est au §25b.
				const char *casse = "widgets { }\n";
				const bool ecrit =
					NkFile::WriteAllText("nkuidesign_ctl25/racine.nkgui", doc)
					&& NkFile::WriteAllText("nkuidesign_ctl25/a/milieu.nkgui", doc)
					&& NkFile::WriteAllText("nkuidesign_ctl25/a/b/fond.nkgui", casse)
					// ⚠️ UN INTRUS QUI N'EST PAS UN `.nkgui`. Sans lui, un
					//    balayage qui rendrait TOUS les fichiers serait vert :
					//    « il en a trouve 3 » ne dit pas « il a filtre ».
					&& NkFile::WriteAllText("nkuidesign_ctl25/a/b/intrus.txt", doc);

				const NkVector<NkString> trouves = NkGCollecter(racine);
				check("25. le balayage DESCEND : 3 fichiers sur 3 niveaux, trouves "
					  "depuis la racine",
					  arbre && ecrit && trouves.Size() == 3, "");
				if (arbre && ecrit && trouves.Size() != 3) {
					rep.Append("      trouves : ");
					rep.Append(NkGU32((uint32)trouves.Size()));
					rep.Append(" au lieu de 3\n");
					for (uint32 i = 0; i < (uint32)trouves.Size(); ++i) {
						rep.Append("        ");
						rep.Append(trouves[i]);
						rep.Append('\n');
					}
				}

				// ⚠️ 25b : LES DEUX GESTES BALAIENT PAREIL. `--roundtrip` et
				//    `--valider` portaient DEUX copies du meme appel et DEUX
				//    copies du meme message d'echec. C'est le motif que V6 a
				//    puni : un mecanisme a deux domiciles n'est corrige qu'a un
				//    seul.
				//
				// ⚠️ ET CE CONTROLE ETAIT MORT A SA PREMIERE ECRITURE. Il exigeait
				//    `rt == 0 && vd == 0` -- LE SUCCES DES DEUX GESTES. La mutation
				//    M7 (la validation reprend son appel non recursif) lui est passee
				//    sous le nez, 65 / 65 : un balayage qui lit MOINS reussit tout
				//    autant. **Exiger un succes, c'est etre satisfait par le vide** --
				//    la meme famille que les deux `Find(...) == npos` qui rendaient T5
				//    vert sur un ecrivain JSON mort.
				//
				//    D'ou l'ancrage : le fichier du FOND (`a/b/`) est illisible. Les
				//    deux gestes doivent donc REFUSER (1), chacun de son cote. Un
				//    balayage qui ne descend pas rend 0 (il ne voit que la racine,
				//    saine) ; un balayage qui ne lit rien rend 2. **Seul un balayage
				//    qui a vraiment atteint le fond rend 1.**
				const int rt = NkGRunRoundTrip(racine);
				const int vd = NkGRunValidate(racine);
				check("25b. LES DEUX GESTES atteignent le FOND : chacun refuse le "
					  "fichier casse enterre a trois niveaux, pas seulement celui "
					  "qu'on a corrige",
					  rt == 1 && vd == 1, "");
				if (rt != 1 || vd != 1) {
					rep.Append("      aller-retour -> ");
					rep.Append(NkGU32((uint32)rt));
					rep.Append(", validation -> ");
					rep.Append(NkGU32((uint32)vd));
					rep.Append(" (1 attendu des deux cotes ; 0 = le fond n'a pas ete lu, "
							   "2 = rien du tout)\n");
				}

				// ⚠️ 25c : ET IL SAIT ENCORE DIRE NON. Un balayage qui rendrait
				//    tout, partout, serait vert de 25 a 25b. Le sous-dossier
				//    `a/b` ne contient qu'un seul `.nkgui` : le compte y tombe a
				//    1, et un arbre sans aucun `.nkgui` reste un ECHEC (code 2),
				//    pas un « 0 erreur » flatteur.
				const NkVector<NkString> feuille = NkGCollecter(sous);
				NkDirectory::CreateRecursive("nkuidesign_ctl25/vide/rien");
				const int nul = NkGRunRoundTrip("nkuidesign_ctl25/vide");
				check("25c. LE BALAYAGE SAIT ENCORE DIRE NON : 1 seul fichier sous la "
					  "feuille, et un arbre sans .nkgui reste un ECHEC (2)",
					  feuille.Size() == 1 && nul == 2, "");

				NkDirectory::Delete(racine, true);
				check("25d. le controle 25 ne laisse rien derriere lui",
					  !NkDirectory::Exists(racine), "");
			}

			// ================================================================
			//  26. LA LISTE FERMEE DES ETATS -- tranchee par Rodolf le 2026-08-27
			// ================================================================
			//
			//  `Normal` · `Hover` · `Pressed` · `Focus` · `Disabled`.
			//
			//  ⚠️ CE QUE CES CONTROLES TIENNENT N'EST PAS « la liste est bonne » --
			//     ca, c'est une decision. Ils tiennent qu'elle est **FERMEE** : que
			//     le format sait dire NON a ce qui n'y est pas. Une liste qu'on
			//     documente sans la faire respecter est un commentaire.
			{
				auto valide = [&](const char *src, NkVector<NkGuiDiag> &dg) {
					NkArchive d;
					NkGuiDiag e;
					if (!parse(src, d, e)) {
						return false;
					}
					NkGValidate(d, dg);
					return true;
				};

				// 26a. LES CINQ PASSENT. Un a un, dans un seul fichier.
				{
					const char *src = "nkgui 0.3\n"
									  "widgets {\n"
									  "  Button \"b\" {\n"
									  "    appearance(Normal) { radius = 1 }\n"
									  "    appearance(Hover) { radius = 2 }\n"
									  "    appearance(Pressed) { radius = 3 }\n"
									  "    appearance(Focus) { radius = 4 }\n"
									  "    appearance(Disabled) { radius = 5 }\n"
									  "  }\n"
									  "}\n";
					NkVector<NkGuiDiag> dg;
					const bool lu = valide(src, dg);
					check("26a. LES CINQ ETATS passent : Normal, Hover, Pressed, Focus, "
						  "Disabled -- 0 diagnostic",
						  lu && dg.Size() == 0, "");
				}

				// ⚠️ 26b. LE TEMOIN DE 26a, ET IL EST INDISPENSABLE. « 0 diagnostic »
				//     est une ABSENCE : une liste qui accepterait TOUT rendrait 26a
				//     vert. C'est la lecon du 27/08 au matin appliquee le jour meme.
				{
					const char *src = "nkgui 0.3\n"
									  "widgets {\n"
									  "  Button \"b\" {\n"
									  "    appearance(Mystere) { radius = 1 }\n"
									  "  }\n"
									  "}\n";
					NkVector<NkGuiDiag> dg;
					const bool lu = valide(src, dg);
					bool nomme = false;
					for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
						if (dg[i].code.Compare("E-ETAT-INCONNU") == 0 && dg[i].line == 4
							&& dg[i].message.Contains("Mystere")
							&& dg[i].message.Contains("Normal")) {
							nomme = true;
						}
					}
					check("26b. TEMOIN -- un etat hors liste est REFUSE, avec sa ligne et "
						  "la liste dans le message (sinon 26a serait vert sur une liste "
						  "qui accepte tout)",
						  lu && dg.Size() == 1 && nomme, "");
				}

				// ⚠️ 26c. LES ECARTES SONT ECARTES, ET C'EST LA MOITIE DU TRAVAIL.
				//     Une liste fermee se justifie par ses exclusions autant que par
				//     ses membres. Ces quatre-la sont les plus tentants -- `Idle`
				//     (la transposition evidente du doc 3), `Active` (quatre sens
				//     incompatibles), `Default` (le bouton par defaut en CSS et Qt),
				//     `Checked` (une DONNEE du composant). Le jour ou l'un d'eux
				//     entre, ce sera une decision, et ce controle la reclamera.
				{
					static const char *kEcartes[] = {"Idle", "Active", "Default", "Checked"};
					uint32 refuses = 0;
					for (uint32 i = 0; i < 4; ++i) {
						NkString src("nkgui 0.3\nwidgets {\n  Button \"b\" {\n    appearance(");
						src.Append(kEcartes[i]);
						src.Append(") { radius = 1 }\n  }\n}\n");
						NkVector<NkGuiDiag> dg;
						if (valide(src.Data(), dg) && dg.Size() == 1
							&& dg[0].code.Compare("E-ETAT-INCONNU") == 0) {
							++refuses;
						}
					}
					check("26c. LES QUATRE ECARTES sont refuses : Idle, Active, Default, "
						  "Checked -- une liste fermee se justifie par ses exclusions",
						  refuses == 4, "");
				}

				// ⚠️ 26d. `appearance { }` ET `appearance(Normal) { }` SONT LE MEME
				//     ETAT -- la question que le mot `Normal` a ouverte, et la seule
				//     reponse que le releve autorise : chez TOUS ceux qui nomment le
				//     repos (Unity, Godot, WPF, Figma), **le repos nomme EST le",
				//     socle**. Aucun n'a a la fois un socle et un repos distincts.
				{
					const char *src = "nkgui 0.3\n"
									  "widgets {\n"
									  "  Button \"b\" {\n"
									  "    appearance { radius = 1 }\n"
									  "    appearance(Normal) { radius = 2 }\n"
									  "  }\n"
									  "}\n";
					NkVector<NkGuiDiag> dg;
					const bool lu = valide(src, dg);
					bool dit = false;
					for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
						if (dg[i].code.Compare("W-ETAT-DOUBLE") == 0 && dg[i].line == 5
							&& dg[i].message.Contains("Normal")) {
							dit = true;
						}
					}
					check("26d. `appearance` et `appearance(Normal)` sont LE MEME ETAT : "
						  "les cumuler est signale (W-ETAT-DOUBLE), pas ignore",
						  lu && dit, "");
				}

				// ⚠️ 26e. LE TEMOIN DE 26d -- SANS LUI, UN AVERTISSEMENT POSE SUR
				//     CHAQUE APPARENCE passerait. Cinq etats DIFFERENTS ne doivent
				//     produire AUCUN doublon (c'est 26a), et deux `Hover` doivent en
				//     produire un : le doublon suit l'ETAT, pas le nombre de blocs.
				{
					const char *src = "nkgui 0.3\n"
									  "widgets {\n"
									  "  Button \"b\" {\n"
									  "    appearance(Hover) { radius = 1 }\n"
									  "    appearance(Hover) { radius = 2 }\n"
									  "  }\n"
									  "  Button \"c\" {\n"
									  "    appearance(Hover) { radius = 3 }\n"
									  "  }\n"
									  "}\n";
					NkVector<NkGuiDiag> dg;
					const bool lu = valide(src, dg);
					check("26e. TEMOIN -- deux `Hover` sur UN widget = 1 doublon ; le `Hover` "
						  "du widget VOISIN n'en est pas un (la liste est locale au widget)",
						  lu && dg.Size() == 1
							  && dg[0].code.Compare("W-ETAT-DOUBLE") == 0 && dg[0].line == 5,
						  "");
				}

				// ⚠️ 26f. L'ALLER-RETOUR DES DEUX GRAPHIES, espaces interieurs
				//     compris. C'est la contrepartie de « le modele ne canonise pas » :
				//     si l'ecrivain REGENERAIT l'etat au lieu de reemettre son lexeme,
				//     tous les documents ecrits dans l'autre graphie cesseraient de
				//     revenir a l'octet. La promesse centrale decide seule.
				memeOctets("26f. les DEUX graphies reviennent a l'octet, y compris "
						   "`appearance(  Normal  )` -- l'etat est un lexeme, pas une "
						   "valeur regeneree",
						   "nkgui 0.3\n"
						   "widgets {\n"
						   "  Button \"b\" {\n"
						   "    appearance { radius = 1 }\n"
						   "    appearance(  Normal  ) { radius = 2 }\n"
						   "    appearance(Hover) { radius = 3 }\n"
						   "  }\n"
						   "}\n");

				// ⚠️ 26g. LA REGLE (d) N'A PAS ETE SACRIFIEE POUR FERMER LA LIMITE.
				//     La premiere version acceptait `Ident ( Ident ) {` PARTOUT, et le
				//     controle 20 l'a attrapee : `futurMembre(x) { }` -- le membre
				//     inconnu d'un fichier 0.4 -- cessait d'etre conserve verbatim.
				//     **On aurait invente une structure pour une construction qu'on ne
				//     connait pas.** Le document 9 §7 est net : seul `appearance` porte
				//     un `(Etat)`. Ce controle tient la restriction a sa place.
				memeOctets("26g. un en-tete parenthese sur un AUTRE mot que `appearance` "
						   "reste une tranche verbatim (regle (d)) -- et revient a l'octet",
						   "nkgui 0.3\n"
						   "widgets {\n"
						   "  Button \"b\" {\n"
						   "    futurMembre(x) {\n"
						   "      profondeur = 3\n"
						   "    }\n"
						   "  }\n"
						   "}\n");

				// ⚠️ 26h. L'ORDRE DE LA TABLE **EST** LA PRIORITE, donc il se tient
				//     par un controle et non par un commentaire. Une table qu'on
				//     reordonne « pour ranger » changerait ce qui s'affiche a
				//     l'ecran, en silence et sans qu'aucun fichier ne bouge.
				//
				//     Disabled > Pressed > Hover > FocusVisible > Focus > Normal
				//
				//     Le compte figé est ici LEGITIME, au sens de la note du 23/08 :
				//     la liste est FERMEE, donc sa taille EST la regle. Le jour ou
				//     elle grandit, c'est une decision -- et ce controle la reclame.
				{
					static const char *kAttendu[] = {"Disabled", "Pressed",	 "Hover",
													 "FocusVisible", "Focus", "Normal"};
					uint32 n = 0;
					const char *const *t = nkuidesign::guifmt::NkGEtats(n);
					bool memeOrdre = (n == 6);
					for (uint32 i = 0; memeOrdre && i < n; ++i) {
						if (NkString(t[i]).Compare(kAttendu[i]) != 0) {
							memeOrdre = false;
						}
					}
					check("26h. LA PRIORITE EST L'ORDRE DE LA TABLE : Disabled > Pressed "
						  "> Hover > FocusVisible > Focus > Normal -- six noms, cet ordre",
						  memeOrdre, "");
					if (!memeOrdre) {
						rep.Append("      table lue : ");
						for (uint32 i = 0; i < n; ++i) {
							rep.Append(t[i]);
							rep.Append(" ");
						}
						rep.Append("\n");
					}
				}

				// ⚠️ 26i. `Focus` ET `FocusVisible` SONT DEUX ETATS, PAS UN PREFIXE.
				//     Deux noms dont l'un commence par l'autre : si une comparaison
				//     quelque part devenait un `StartsWith`, `FocusVisible` serait lu
				//     comme `Focus` et les deux blocs deviendraient un DOUBLON. Ce
				//     controle tient qu'ils restent distincts partout.
				{
					const char *src = "nkgui 0.3\n"
									  "widgets {\n"
									  "  Button \"b\" {\n"
									  "    appearance(Focus) { fill { color = #111111 } }\n"
									  "    appearance(FocusVisible) { radius = 2 }\n"
									  "  }\n"
									  "}\n";
					NkVector<NkGuiDiag> dg;
					const bool lu = valide(src, dg);
					check("26i. `Focus` et `FocusVisible` sont DEUX etats distincts : les "
						  "poser tous les deux n'est pas un doublon (pas de collision de "
						  "prefixe)",
						  lu && dg.Size() == 0, "");
				}

				// ⚠️ 26j. L'AVERTISSEMENT QUI GUIDE -- et c'est lui qui rend tenable
				//     le choix d'avoir SIX noms dont deux voisins. Sans lui, on ecrit
				//     `Focus` parce que c'est le mot court, et l'anneau apparait aussi
				//     a la souris -- exactement ce que `:focus-visible` evite.
				//
				//     ⚠️ LE PREMIER DOCUMENT QU'IL A ATTRAPE ETAIT LE NOTRE :
				//     `valides/10_etats_apparence.nkgui`, ecrit deux heures plus tot,
				//     posait l'anneau de son BOUTON sur `Focus`. Le fichier a ete
				//     corrige ; l'avertissement n'est pas decoratif.
				{
					const char *src = "nkgui 0.3\n"
									  "widgets {\n"
									  "  Button \"b\" {\n"
									  "    appearance(Focus) {\n"
									  "      stroke { color = #F79A28, width = 2 }\n"
									  "    }\n"
									  "  }\n"
									  "}\n";
					NkVector<NkGuiDiag> dg;
					const bool lu = valide(src, dg);
					bool guide = false;
					for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
						if (dg[i].code.Compare("W-FOCUS-ANNEAU") == 0 && dg[i].line == 5
							&& dg[i].message.Contains("FocusVisible")) {
							guide = true;
						}
					}
					check("26j. un ANNEAU (`stroke`) pose sur `Focus` est signale, avec "
						  "`FocusVisible` suggere dans le message -- la distinction est "
						  "posee au bon moment, pas laissee dans une doc",
						  lu && dg.Size() == 1 && guide, "");
				}

				// ⚠️ 26k. LES DEUX TEMOINS DE 26j, ET ILS SONT INDISPENSABLES : un
				//     avertissement pose sur CHAQUE apparence, ou sur CHAQUE effet,
				//     rendrait 26j vert. Deux cas doivent rester SILENCIEUX :
				//       - un `fill` dans `Focus` : ce n'est pas un anneau ;
				//       - un `stroke` dans `FocusVisible` : c'est justement le bon
				//         endroit, il ne faut surtout pas s'en plaindre.
				//
				//     Le second est le plus important : un avertissement qui se
				//     declencherait AUSSI sur la forme qu'il recommande enverrait
				//     l'utilisateur en rond, et c'est le genre de parade qu'on prend
				//     l'habitude d'ignorer -- desarmee pour de bon.
				{
					const char *src = "nkgui 0.3\n"
									  "widgets {\n"
									  "  Button \"b\" {\n"
									  "    appearance(Focus) { fill { color = #111111 } }\n"
									  "    appearance(FocusVisible) {\n"
									  "      stroke { color = #F79A28, width = 2 }\n"
									  "    }\n"
									  "  }\n"
									  "}\n";
					NkVector<NkGuiDiag> dg;
					const bool lu = valide(src, dg);
					check("26k. TEMOIN -- un `fill` dans `Focus` et un `stroke` dans "
						  "`FocusVisible` ne disent RIEN : l'avertissement vise l'anneau "
						  "mal place, pas l'etat ni l'effet",
						  lu && dg.Size() == 0, "");
				}

				// 26l. Le corpus du depot exerce les deux focus -- et son
				//      avertissement est DELIBERE : un champ de saisie doit se voir
				//      focalise quelle que soit l'origine. On verifie ici que ce cas
				//      reste LEGAL, sans quoi l'avertissement serait un refus deguise.
				{
					const char *src = "nkgui 0.3\n"
									  "widgets {\n"
									  "  TextField \"f\" {\n"
									  "    appearance(Focus) {\n"
									  "      stroke { color = #F79A28, width = 2 }\n"
									  "    }\n"
									  "  }\n"
									  "}\n";
					NkVector<NkGuiDiag> dg;
					const bool lu = valide(src, dg);
					uint32 erreurs = 0;
					for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
						if (dg[i].code.StartsWith("E-")) {
							++erreurs;
						}
					}
					check("26l. le cas LEGITIME de `Focus` (un champ de saisie) reste "
						  "legal : un avertissement, ZERO erreur -- on pose la question, "
						  "on ne tranche pas a la place du concepteur",
						  lu && erreurs == 0 && dg.Size() == 1, "");
				}
			}

			rep.Append("\n=== CONTROLES : ");
			rep.Append(NkGU32(pass));
			rep.Append(" / ");
			rep.Append(NkGU32(total));
			rep.Append(" ===\n");
			NkGPublish(rep, "nkuidesign_roundtrip_controles.txt");
			return (pass == total) ? 0 : 1;
		}

	}  // namespace guifmt
}  // namespace nkuidesign

#endif	// __NKENTSEU_NKUIDESIGN_NKGUIROUNDTRIP_H__
