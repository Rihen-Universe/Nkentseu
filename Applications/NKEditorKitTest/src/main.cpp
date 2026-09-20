// -----------------------------------------------------------------------------
// @File    main.cpp
// @Brief   BANC DE NKEditorKit — la resolution des roles de theme, et le choix
//          du backend graphique. Sans fenetre, sans GPU, sans souris.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE CE BANC MESURE, ET CE QU'IL NE MESURE PAS
// =============================================================================
//  Il mesure DEUX choses, toutes deux purement calculables :
//    A. `NkResolveRole` / `NkRoleRegistry::Find` — un nom de role declare dans un
//       composant arrive-t-il sur un identifiant de theme, et que se passe-t-il
//       quand il n'y arrive pas ;
//    B. `NkEditorGfxApi` — le vocabulaire du choix de backend : nom textuel,
//       analyse, et disponibilite par plateforme.
//
//  ⚠️ REGIME COUVERT, ecrit avec le resultat : ce banc tourne **sans fenetre et
//     sans contexte GPU**. Il ne dit RIEN de ce qu'un backend fait une fois
//     cree — il dit seulement ce que le kit repond quand on le lui demande. La
//     partie « le magenta a disparu de l'ecran » ne se mesure qu'a l'ecran, et
//     c'est l'agent NkUIDesign qui la tient (temoin visuel, Q64 §4).
//
// =============================================================================
//  POURQUOI IL EXISTE — un resolveur qui dit oui a tout ne peut pas voir un
//  nom faux
// =============================================================================
//  Le 18/08, NkUIDesign a peint son navigateur de contenu en MAGENTA PLEIN
//  ECRAN pendant que sa sonde annonçait 72/72. Sa sonde avait SA propre
//  resolution — un hachage qui rendait un identifiant valide pour n'importe
//  quelle chaine et ne rendait JAMAIS `NK_ROLE_INVALID`. Elle ne pouvait pas
//  voir le defaut : elle ne pouvait voir aucun defaut de ce genre.
//
//  Ce banc n'a donc AUCUNE resolution a lui. Il appelle `NkResolveRole`, la
//  vraie, celle que les applications appellent. C'est la contrainte n.1 de ce
//  fichier, et la seule qui ne se negocie pas.
//
// =============================================================================
//  OU AJOUTER UN ESSAI (le point d'extension, nomme comme l'exige la regle
//  « le code produit doit etre lisible par un humain qui arrive »)
// =============================================================================
//  Les essais sont ranges en FAMILLES numerotees, une fonction par famille.
//    Famille 1 — la canonisation, cas par cas (dont 2 controles negatifs)
//    Famille 2 — les jetons REELS des composants declares, par le registre
//    Famille 3 — le repli franc : compte ET nomme
//    Famille 4 — le vocabulaire de backend graphique, Metal compris
//    Famille 15 — les roles d'ALERTE et le repli DECLARE (14/09). Numerotee 15
//       et non 6 a dessein : le sous-banc du selecteur (familles 5 a 14, dans
//       `NkFilePickerNavProbe.h`) imprime deja des essais « 6a » a « 6l ». Deux
//       « 6k » differents dans un meme rapport, c'est un instrument qui ment --
//       on lit le mauvais verdict sans s'en apercevoir.
//  Pour ajouter un essai : ecrire un `Check(...)` dans la famille concernee.
//  Pour ajouter une famille : ecrire `static void FamilleN()` et l'appeler dans
//  `main`. Rien d'autre a toucher.
//
// =============================================================================
//  CE BANC SAIT TOMBER — MESURE, PAS AFFIRMEE
// =============================================================================
//  Un essai qui passe avant ET apres le correctif ne mesure rien. Trois releves,
//  tous rejouables, et **aucun** ne passe par un interrupteur du banc : ce qui a
//  ete mute, c'est le CODE DU KIT. Un banc qui contient sa propre facon
//  d'echouer ne prouve rien — il se contente de declarer l'echec au lieu de le
//  produire.
//
//  | etat mesure                                        | resultat | code |
//  |----------------------------------------------------|----------|------|
//  | AVANT correctif (origin/main 5af191eb, familles    |  6/13    |  1   |
//  |   1 et 2 seules — 3 et 4 ne compilaient pas,        |          |      |
//  |   l'API qu'elles mesurent n'existait pas)           |          |      |
//  | MUTATION A : canonisation desactivee dans           | 19/28    |  1   |
//  |   `NkRoleRegistry::Find` -> 1a-1f, 2b, 3e, 3h rouges|          |      |
//  | MUTATION B : `NkEditorGfxApiSupported` rend toujours| 26/28    |  1   |
//  |   vrai -> 4e et 4f rouges                           |          |      |
//  | APRES correctif, sans mutation                      | 28/28    |  0   |
//
//  RELEVE DU 2026-09-14, famille 15 (roles d'alerte). Meme discipline : les deux
//  mutations sont posees DANS LE KIT, pas dans le banc.
//
//  | etat mesure                                        | resultat | code |
//  |----------------------------------------------------|----------|------|
//  | MUTATION A : `NkTheme::Get` rend la sentinelle telle| 108/113  |  1   |
//  |   quelle (`return c;` au lieu de `Replier(r)`) --   |          |      |
//  |   c'est LITTERALEMENT l'etat d'avant le 14/09.      |          |      |
//  |   Rouges : 15i, 15j, 15k, 15l, 15m                  |          |      |
//  | MUTATION B : `Light()` repose les trois statuts aux | 111/113  |  1   |
//  |   valeurs Dark (une seule couleur pour les deux     |          |      |
//  |   themes, l'etat d'avant lui aussi).                |          |      |
//  |   Rouges : 15c, 15g -- et 15g NOMME la paire :      |          |      |
//  |   « status_warn sur panel_header = 2.10 (exige 3.00)|          |      |
//  | APRES correctif, sans mutation                      | 113/113  |  0   |
//
//  Pour rejouer une mutation : la poser a la main dans le kit, reconstruire,
//  relancer. C'est deux minutes, et c'est la seule chose qui distingue un banc
//  d'un decor.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentDecl.h"
#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NKEditorKit/NkIEditorRenderer.h"
#include "NKEditorKit/NkTheme.h"
// Le contrat de donnees du fil du panneau IA (20/09). Il n'est PAS dans le
// parapluie `NkEditorKit.h` et ce n'est pas un oubli : tant que le peintre
// n'existe pas, l'y mettre ferait recompiler tous les consommateurs du kit pour
// un en-tete que personne n'appelle. CONDITION D'ENTREE DANS LE PARAPLUIE : le
// jour ou le peintre du fil est livre.
#include "NKEditorKit/NkAiThread.h"
#include "NKEditorKit/NkAiThreadLayout.h"
#include "NkAiPlanProbe.h" // famille 22 : le plan du fil, eprouve sans fenetre
// Famille 5 — le RAIL du selecteur de fichiers. Le banc vit DANS LE KIT
// (`NkFilePickerNavProbe.h`) : c'est le kit qu'il mesure, et une fusion doit
// l'emporter avec le correctif qu'il garde. Ici, une ligne d'appel.
#include "NKEditorKit/NkFilePickerNavProbe.h"
// (o1) LA BANDE D'ONGLETS PARTAGEE et le peintre qui ENREGISTRE ce qui est
// peint : le banc lit le flux, il ne croit pas un resultat rapporte.
#include "NKEditorKit/Components/NkTabStripModel.h"
#include "NKEditorKit/Components/NkRecordingPaint.h"
// (o3) la porte du chrome : les couches de surfaces et PointReachable
#include "NKEditorKit/NkEditorSurface.h"

#include <stdio.h>

using namespace nkentseu;
using namespace nkentseu::editorkit;

// ── LE COMPTEUR ─────────────────────────────────────────────────────────────
// Volontairement minuscule : ce banc n'a pas besoin d'un cadre de test, il a
// besoin d'un code de sortie honnete et d'une ligne par essai.
namespace {

	uint32 gPassed = 0;
	uint32 gFailed = 0;

	void Check(const char *id, bool ok, const char *what) {
		if (ok) {
			++gPassed;
			printf("  [ ok ] %-6s %s\n", id, what);
		} else {
			++gFailed;
			printf("  [FAIL] %-6s %s\n", id, what);
		}
	}

	// Comparaison de chaines locale au banc. Ce n'est PAS une reimplantation de
	// la resolution : c'est de l'outillage d'assertion. La frontiere est nette —
	// aucune fonction de ce banc ne transforme un nom de role.
	bool Same(const char *a, const char *b) {
		if (!a || !b)
			return a == b;
		for (; *a && *b; ++a, ++b)
			if (*a != *b)
				return false;
		return *a == *b;
	}

} // namespace

// =============================================================================
//  FAMILLE 1 — LA CANONISATION, CAS PAR CAS
// =============================================================================
//  Les huit cas viennent de Q64 (canal `noge`), ou ils avaient ete ecrits AVANT
//  le correctif. Six positifs, **deux controles negatifs** — et ce sont les deux
//  negatifs qui font le travail : sans eux, une fonction qui abimerait tout sauf
//  le PascalCase passerait le banc.
//
//  L'essai porte sur la RESOLUTION (`NkResolveRole`), pas sur la fonction de
//  canonisation prise a part : c'est le comportement observable, celui dont le
//  magenta depend.
static void Famille1_Canonisation() {
	printf("\nFamille 1 — la canonisation d'un nom de role\n");

	// Les six graphies PascalCase qui doivent rattraper, avec la forme canonique
	// qu'elles doivent atteindre. On verifie l'IDENTITE d'identifiant avec la
	// forme snake_case : « ca resout » ne suffirait pas, il faut que ce soit vers
	// LE MEME role — sinon deux noms d'un meme role peindraient deux couleurs.
	struct Cas {
			const char *ecrit;
			const char *canonique;
	};
	static const Cas kCas[] = {
		{"PanelBg", "panel_bg"},		   {"PanelHeader", "panel_header"},
		{"TextOnAccent", "text_on_accent"}, {"AccentUi", "accent_ui"},
		{"TypeFolder", "type_folder"},	   {"InputBg", "input_bg"},
	};

	for (uint32 i = 0; i < sizeof(kCas) / sizeof(kCas[0]); ++i) {
		const uint16 attendu = NkResolveRole(kCas[i].canonique);
		const uint16 obtenu = NkResolveRole(kCas[i].ecrit);
		char id[8];
		snprintf(id, sizeof(id), "1%c", (char)('a' + i));
		char msg[160];
		snprintf(msg, sizeof(msg), "« %s » resout vers le MEME role que « %s »", kCas[i].ecrit,
				 kCas[i].canonique);
		Check(id, attendu != NK_ROLE_INVALID && obtenu == attendu, msg);
	}

	// ⚠️ CONTROLE NEGATIF 1 — un nom DEJA canonique ne bouge pas.
	Check("1g", NkResolveRole("panel_bg") == (uint16)NkRole::PanelBg,
		  "controle negatif : « panel_bg » resout toujours, inchange");

	// ⚠️ CONTROLE NEGATIF 2 — un role d'APPLICATION, enregistre sous sa propre
	// graphie, garde son identifiant. C'est ce qui prouve que la canonisation est
	// essayee APRES le nom brut : si elle passait avant, « nk3d.anneau_brosse »
	// resterait juste (il est deja minuscule), mais un role enregistre « MonRole »
	// serait cherche sous « mon_role » et manquerait. On teste donc les deux.
	const uint16 idBrosse = NkRoleRegistry::Register("nk3d.anneau_brosse");
	Check("1h", idBrosse != NK_ROLE_INVALID && NkResolveRole("nk3d.anneau_brosse") == idBrosse,
		  "controle negatif : un role d'extension minuscule garde son identifiant");

	const uint16 idMixte = NkRoleRegistry::Register("MonRoleDApplication");
	Check("1i", idMixte != NK_ROLE_INVALID && NkResolveRole("MonRoleDApplication") == idMixte,
		  "controle negatif : un role d'extension en PascalCase resout sur SON nom brut, "
		  "pas sur sa forme canonisee");

	// ⚠️ ELLE N'INVENTE PAS. Une canonisation qui rattraperait tout serait le
	// hachage permissif deplace d'un cran — exactement le defaut de la sonde de
	// NkUIDesign, qui a laisse 72 essais verts couvrir un magenta plein ecran.
	Check("1j", NkResolveRole("RoleQuiNExistePasDuTout") == NK_ROLE_INVALID,
		  "un role reellement inconnu reste NK_ROLE_INVALID");
}

// =============================================================================
//  FAMILLE 2 — LES JETONS REELS, PAR LE REGISTRE
// =============================================================================
//  ⚠️ ELLE BOUCLE SUR LE REGISTRE, PAS SUR UNE LISTE DE NOMS. C'est la lecon de
//     la famille 33 de NkUIDesign : un composant ajoute demain est couvert sans
//     qu'une ligne de ce banc bouge. Une liste ecrite a la main aurait couvert
//     ce qui existait le jour ou on l'a ecrite, et rien d'autre.
//
//  ⚠️ PERIMETRE, ecrit avec le chiffre — les deux comptes sont vrais, ils ne
//     mesurent pas le meme arbre :
//       - `origin/main` : 1 composant declare (`content_browser`), **10 jetons,
//         10 en PascalCase**, zero exception ;
//       - `feat/noge-inventaire` : + `tree_view`, 13 jetons de plus -> 23/23.
//     Le banc ne compte pas 23 ici et ne doit pas pretendre le contraire : il
//     compte ce que le registre porte DANS CET ARBRE, et il l'affiche.
static void Famille2_JetonsReels() {
	printf("\nFamille 2 — les jetons declares des composants du registre\n");

	// Le registre est peuple par les applications ; ici on y met ce que le kit
	// declare lui-meme, pour que le banc ne depende d'aucune application.
	NkComponentRegistry::Register(NkContentBrowserDecl());

	const uint16 nbComposants = NkComponentRegistry::Count();
	Check("2a", nbComposants > 0, "le registre porte au moins un composant declare");

	uint32 total = 0, resolus = 0, pascal = 0;
	char premierFautif[96] = {0};

	for (uint16 c = 0; c < nbComposants; ++c) {
		const NkComponentDecl *d = NkComponentRegistry::At(c);
		if (!d)
			continue;
		for (uint16 t = 0; t < d->tokenCount; ++t) {
			const char *role = d->tokens[t].defaultRole;
			++total;
			// « porte au moins une majuscule » = la graphie qui ne resolvait pas.
			for (const char *p = role; p && *p; ++p)
				if (*p >= 'A' && *p <= 'Z') {
					++pascal;
					break;
				}
			if (NkResolveRole(role) != NK_ROLE_INVALID)
				++resolus;
			else if (!premierFautif[0])
				snprintf(premierFautif, sizeof(premierFautif), "%s::%s -> « %s »", d->name,
						 d->tokens[t].name, role ? role : "(nul)");
		}
	}

	printf("       perimetre : %u composant(s), %u jeton(s) declare(s), dont %u en PascalCase\n",
		   (uint32)nbComposants, total, pascal);

	char msg[220];
	snprintf(msg, sizeof(msg), "les %u jetons declares resolvent TOUS (%u/%u)%s%s", total, resolus,
			 total, premierFautif[0] ? " — premier fautif : " : "", premierFautif);
	Check("2b", total > 0 && resolus == total, msg);

	// Ce qui rend l'essai 2b non trivial : s'il n'y avait AUCUN PascalCase, il
	// passerait sans que la canonisation ait rien fait. On le dit a voix haute.
	Check("2c", pascal > 0,
		  "au moins un jeton est declare en PascalCase — sinon 2b ne prouverait rien");
}

// =============================================================================
//  FAMILLE 3 — LE REPLI FRANC : COMPTE **ET** NOMME
// =============================================================================
//  (a) la canonisation supprime la classe de defaut ; (b) le repli franc
//  garantit qu'on verra la PROCHAINE, celle que (a) ne couvre pas.
//
//  ⚠️ ET (b) PROTEGE (a). L'audit retient aussi les RATTRAPAGES. Sans cette
//     liste, la canonisation rendrait les declarations fausses invisibles :
//     l'ecran serait juste, personne ne corrigerait la source, et le jour ou la
//     canonisation bougerait, tous les jetons casseraient d'un coup. C'est
//     « une protection qui empeche d'aller verifier », et la seule facon de ne
//     pas la subir est de publier ce qu'elle a masque.
//
//  📌 L'ESSAI QU'ON N'ECRIT PAS, ET POURQUOI. `Check(RescuedCount() > 0)` serait
//     rouge le jour ou quelqu'un corrige les declarations a la source : il
//     punirait le correctif qu'il reclame. L'assertion porte donc sur la FORME
//     de la trace — tout rattrapage porte son nom declare ET la forme qui a
//     resolu — et le nombre est publie a cote, comme diagnostic.
static void Famille3_RepliFranc() {
	printf("\nFamille 3 — le repli franc : un role inconnu se dit\n");

	NkRoleAudit::Reset();
	NkRoleAudit::SetSink(nullptr, nullptr); // le banc ne veut pas la sortie stderr

	Check("3a", NkRoleAudit::FaultCount() == 0 && NkRoleAudit::RescuedCount() == 0,
		  "Reset() vide les deux listes");

	(void)NkResolveRole("UnRoleTotalementInvente");
	Check("3b", NkRoleAudit::FaultCount() == 1, "un role inconnu est COMPTE");
	Check("3c",
		  NkRoleAudit::FaultCount() == 1 && Same(NkRoleAudit::Faults()[0].name.CStr(),
												 "UnRoleTotalementInvente"),
		  "le role inconnu est NOMME, tel qu'il est declare");

	// ⚠️ DEDUPLICATION. Le dessin passe par la resolution a CHAQUE image : sans
	// deduplication, la liste grossirait de N entrees par image et la fuite se
	// presenterait comme un ralentissement, jamais comme un defaut de roles.
	for (int32 i = 0; i < 100; ++i)
		(void)NkResolveRole("UnRoleTotalementInvente");
	Check("3d", NkRoleAudit::FaultCount() == 1,
		  "100 resolutions du meme nom inconnu = 1 seule entree (deduplication)");

	// Le rattrapage porte SA FORME : le nom declare et la forme qui a resolu.
	NkRoleAudit::Reset();
	(void)NkResolveRole("PanelHeader");
	bool formeOk = NkRoleAudit::RescuedCount() >= 1;
	for (uint32 i = 0; i < NkRoleAudit::RescuedCount(); ++i) {
		const NkRoleAudit::Entry &e = NkRoleAudit::Rescued()[i];
		if (e.name.Empty() || e.canon.Empty() || Same(e.name.CStr(), e.canon.CStr()))
			formeOk = false;
	}
	Check("3e", formeOk,
		  "tout rattrapage porte son nom DECLARE et la forme qui a resolu, et les deux different");

	// Un nom qui resout directement n'est NI une faute NI un rattrapage : la
	// canonisation ne doit rien enregistrer quand elle n'a rien fait.
	NkRoleAudit::Reset();
	(void)NkResolveRole("panel_bg");
	Check("3f", NkRoleAudit::FaultCount() == 0 && NkRoleAudit::RescuedCount() == 0,
		  "un nom deja canonique n'entre dans aucune des deux listes");

	// ⚠️ `Register` NE DOIT PAS POLLUER L'AUDIT. Il appelle la recherche pour son
	// idempotence ; si cette recherche comptait une faute, enregistrer un role
	// d'application legitime inscrirait une faute imaginaire — et un audit faux
	// est pire qu'aucun audit.
	NkRoleAudit::Reset();
	(void)NkRoleRegistry::Register("nkbanc.role_tout_neuf");
	Check("3g", NkRoleAudit::FaultCount() == 0,
		  "enregistrer un role neuf n'inscrit AUCUNE faute");

	// La ligne lisible par un humain — celle qui part au journal.
	NkRoleAudit::Reset();
	(void)NkResolveRole("RoleAbsentPourLeResume");
	(void)NkResolveRole("TypeFolder");
	NkString resume;
	NkRoleAudit::Summary(resume);
	printf("       resume : %s\n", resume.CStr());
	Check("3h", !resume.Empty() && NkRoleAudit::FaultCount() == 1 &&
					NkRoleAudit::RescuedCount() == 1,
		  "le resume distingue ce qui est perdu de ce qui est rattrape");

	NkRoleAudit::Reset();
}

// =============================================================================
//  FAMILLE 4 — LE VOCABULAIRE DU BACKEND GRAPHIQUE
// =============================================================================
//  Directive de Rodolf (2026-08-18) : « pour toutes nos applications, on doit
//  pouvoir choisir le backend graphique entre ceux disponibles : OpenGL, Vulkan,
//  DX11, DX12 et Metal. » Metal manquait a `NkEditorGfxApi`.
//
//  ⚠️ CE QUE CE BANC EXIGE EN PLUS DE L'ENTREE : qu'une API indisponible sur la
//     plateforme courante se DISE, avec sa raison. Une entree qui plante ou qui
//     retombe en silence sur OpenGL est pire que son absence — c'est le repli
//     muet que la regle 3 de la directive interdit nommement.
static void Famille4_BackendGraphique() {
	printf("\nFamille 4 — le vocabulaire du backend graphique\n");

	// 4a / 4b — l'entree existe, et elle porte un nom textuel stable.
	NkEditorGfxApi api = NkEditorGfxApi::Auto;
	const bool connu = NkEditorGfxApiFromName("metal", api);
	Check("4a", connu && api == NkEditorGfxApi::Metal, "« metal » est un nom reconnu");
	Check("4b", Same(NkEditorGfxApiName(NkEditorGfxApi::Metal), "metal"),
		  "l'aller-retour nom <-> valeur est stable pour Metal");

	// 4c — les CINQ APIs de la directive sont toutes nommables, aucune exception.
	static const char *const kDirective[] = {"opengl", "vulkan", "dx11", "dx12", "metal"};
	uint32 reconnues = 0;
	for (uint32 i = 0; i < 5; ++i) {
		NkEditorGfxApi a = NkEditorGfxApi::Auto;
		if (NkEditorGfxApiFromName(kDirective[i], a) && a != NkEditorGfxApi::Auto)
			++reconnues;
	}
	Check("4c", reconnues == 5, "les 5 APIs nommees par la directive sont toutes reconnues");

	// 4d — une valeur inconnue est REFUSEE, pas silencieusement ramenee a Auto.
	NkEditorGfxApi poubelle = NkEditorGfxApi::DX12;
	Check("4d", !NkEditorGfxApiFromName("directx7", poubelle) && poubelle == NkEditorGfxApi::DX12,
		  "une valeur inconnue est refusee et ne touche pas la sortie");

	// 4e / 4f — LA DISPONIBILITE PAR PLATEFORME, avec sa raison.
	const char *raison = nullptr;
	const bool metalIci = NkEditorGfxApiSupported(NkEditorGfxApi::Metal, &raison);
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
	Check("4e", metalIci, "Metal est disponible sur cette plateforme (Apple)");
#else
	Check("4e", !metalIci, "Metal est REFUSE sur une plateforme qui ne le porte pas");
	Check("4f", raison != nullptr && raison[0] != 0,
		  "le refus porte un message qui dit POURQUOI");
	printf("       raison donnee : %s\n", raison ? raison : "(aucune)");
#endif

	// 4g — le refus est SPECIFIQUE. Si tout etait refuse, 4e passerait sans rien
	// prouver : c'est le controle negatif de la disponibilite.
	const char *r2 = nullptr;
	Check("4g", NkEditorGfxApiSupported(NkEditorGfxApi::OpenGL, &r2) && (r2 == nullptr || !r2[0]),
		  "controle negatif : OpenGL, lui, est disponible et sans raison de refus");
}

// =============================================================================
//  FAMILLE 15 — LES ROLES D'ALERTE, ET LE REPLI QUI NE SE TAIT PLUS (14/09)
// =============================================================================
//  DEUX CHOSES, et elles ne se prouvent pas de la meme facon.
//
//  A. LE ROLE. `StatusWarn` existe, porte une couleur DIFFERENTE dans les deux
//     themes, et ne se confond ni avec l'erreur, ni avec l'ambre de selection
//     3D. Le negatif est ecrit AVANT : un role qui n'existe pas doit etre
//     REFUSE par son nom, jamais rendu noir (6f).
//
//  B. LE REPLI. Un theme charge depuis un FICHIER qui ne porte pas la ligne
//     `status_warn` ne doit PAS peindre transparent : il doit prendre la
//     couleur du role de repli ET LE DIRE dans `NkRoleAudit::Replis()`. C'est
//     l'essai 15i, et c'est le seul du lot qui mesure le defaut d'origine --
//     avant le correctif il sortait 0x00000000 sans une ligne de trace.
//
//  ⚠️ CE QUE CETTE FAMILLE NE MESURE PAS : que la couleur arrive a l'ecran.
//     Aucun banc sans fenetre ne peut le dire. C'est la sonde `NK_THEME_PROBE`
//     de NK3DModeler qui le tient, au pixel, et son releve est dans le canal.
static void Famille15_RolesAlerte() {
	printf("\n[Famille 6] roles d'alerte et repli declare\n");

	const NkTheme d = NkTheme::Dark();
	const NkTheme l = NkTheme::Light();

	// 6a — le role existe, et se nomme comme les deux autres de sa triade.
	Check("15a", Same(NkRoleName(NkRole::StatusWarn), "status_warn"),
		  "StatusWarn porte la cle `status_warn`, famille de status_ok/status_err");

	// 6b — il resout PAR NOM, le chemin que prennent 46 des 51 lectures.
	Check("15b", NkResolveRole("status_warn") == (uint16)NkRole::StatusWarn,
		  "`status_warn` resout par nom sur le bon identifiant");

	// 6c — ATTENDU DERIVE, pas constate : chaque statut doit DIFFERER entre les
	// deux themes. Ecrit avant la mesure, et il tombait avant le correctif --
	// StatusOk et StatusErr etaient poses UNE SEULE FOIS pour les deux.
	const bool tousDifferents =
		d.Get(NkRole::StatusOk) != l.Get(NkRole::StatusOk) &&
		d.Get(NkRole::StatusWarn) != l.Get(NkRole::StatusWarn) &&
		d.Get(NkRole::StatusErr) != l.Get(NkRole::StatusErr);
	Check("15c", tousDifferents,
		  "les TROIS statuts rendent une couleur differente en sombre et en clair");

	// 6e — et ils different ENTRE EUX dans chaque theme. Sans ce controle, un
	// correctif qui poserait la meme couleur partout passerait 6d.
	const bool distinctsEntreEux =
		d.Get(NkRole::StatusOk) != d.Get(NkRole::StatusWarn) &&
		d.Get(NkRole::StatusWarn) != d.Get(NkRole::StatusErr) &&
		l.Get(NkRole::StatusOk) != l.Get(NkRole::StatusWarn) &&
		l.Get(NkRole::StatusWarn) != l.Get(NkRole::StatusErr);
	Check("15d", distinctsEntreEux,
		  "les trois statuts sont distincts entre eux, dans CHAQUE theme");

	// 6f — L'AVERTISSEMENT N'EST PAS L'AMBRE DE SELECTION 3D. C'est la regle
	// 10bis.2, et c'est precisement ce que le site emprunteur violait.
	Check("15e",
		  d.Get(NkRole::StatusWarn) != d.Get(NkRole::AccentSel) &&
			  l.Get(NkRole::StatusWarn) != l.Get(NkRole::AccentSel),
		  "StatusWarn != AccentSel : l'alerte n'emprunte plus la selection 3D");

	// 6g — LE CONTRASTE, contre le seuil que le kit s'impose lui-meme. Les trois
	// paires sont dans `ContrastPairs` : `Validate` echoue si l'une tombe.
	NkThemeIssue pireD, pireL;
	Check("15f", d.Validate(&pireD) == 0,
		  "theme SOMBRE : aucune paire sous son seuil (les 3 statuts compris)");
	Check("15g", l.Validate(&pireL) == 0,
		  "theme CLAIR : aucune paire sous son seuil -- c'est lui qui tombait");
	if (l.Validate(nullptr) != 0)
		printf("         pire paire claire : %s sur %s = %.2f (exige %.2f)\n",
			   NkRoleName(pireL.fg), NkRoleName(pireL.bg), (double)pireL.ratio,
			   (double)pireL.required);

	// ── B. LE REPLI ─────────────────────────────────────────────────────────
	// Un theme de FICHIER ecrit avant le 14/09 : il ne porte pas `status_warn`.
	// On le fabrique ici a la main -- c'est le cas reel, pas une hypothese.
	NkRoleAudit::Reset();
	NkTheme ancien;                       // tout magenta, sentinelles posees
	ancien.Set(NkRole::AccentSel, 0xF2980EFFu); // le repli, lui, est pose
	Check("15h", ancien.GetBrut(NkRole::StatusWarn) == NkThemeNonDefini,
		  "controle de depart : dans ce theme, status_warn n'a PAS ete pose");

	const NkThemeColor peint = ancien.Get(NkRole::StatusWarn);
	// 6k — LE DEFAUT D'ORIGINE, exactement. Avant le correctif, `Get` rendait la
	// sentinelle : 0x00000000, un noir a alpha nul. Le role disparaissait.
	Check("15i", peint != NkThemeNonDefini,
		  "un role non pose ne rend PLUS la sentinelle transparente");
	Check("15j", peint == 0xF2980EFFu,
		  "il rend la couleur de son repli declare (AccentSel), pas du noir");

	// 6m — ET IL LE DIT. Un repli muet est interdit : c'est la regle, et sans
	// cet essai le correctif serait « une couleur de plus », pas une politique.
	Check("15k", NkRoleAudit::RepliCount() == 1,
		  "le repli est ANNONCE : une entree dans NkRoleAudit::Replis()");
	const bool nomme = NkRoleAudit::RepliCount() == 1 &&
					   Same(NkRoleAudit::Replis()[0].name.CStr(), "status_warn") &&
					   Same(NkRoleAudit::Replis()[0].canon.CStr(), "accent_sel");
	Check("15l", nomme, "l'annonce NOMME le role absent ET celui dont il a pris la couleur");

	// 6o — DEDUPLICATION. Le dessin lit le role a chaque image ; sans elle, la
	// liste grossirait de 60 entrees par seconde et la fuite se presenterait
	// comme un ralentissement, jamais comme un defaut de theme.
	for (int32 i = 0; i < 200; ++i)
		(void)ancien.Get(NkRole::StatusWarn);
	Check("15m", NkRoleAudit::RepliCount() == 1,
		  "200 lectures de plus n'ajoutent pas une seule entree (deduplication)");

	// 6p — CONTROLE NEGATIF DU REPLI. Un role OBLIGATOIRE n'a pas de repli : il
	// doit crier en magenta, pas emprunter la couleur du voisin. Sans cet essai,
	// une table qui replierait TOUT passerait 6l sans rien prouver.
	Check("15n", NkRoleRepli(NkRole::Text) == NkRole::Count &&
					NkRoleRepli(NkRole::WindowBg) == NkRole::Count,
		  "controle negatif : Text et WindowBg n'ont AUCUN repli -- ils sont dus");

	// 6q — et le repli ne se declenche pas quand la couleur EST posee. Sans lui,
	// un `Get` qui replierait toujours passerait 6l et 6m.
	NkRoleAudit::Reset();
	(void)d.Get(NkRole::StatusWarn);
	(void)d.Get(NkRole::StatusErr);
	(void)d.Get(NkRole::DocMuted);
	Check("15o", NkRoleAudit::RepliCount() == 0,
		  "controle negatif : sur un theme complet, AUCUN repli n'est annonce");
	NkRoleAudit::Reset();
}

// ─────────────────────────────────────────────────────────────────────────────
//  FAMILLE 20 — LES TROIS SURFACES DU PANNEAU IA (20/09/2026)
//
//  CE QU'ELLE MESURE, ET CE QU'ELLE NE PEUT PAS MESURER
//    Elle juge le THEME : trois roles existent, se nomment, different entre les
//    deux themes, et portent un texte lisible sur les deux. Elle ne dit RIEN de
//    ce qui arrive a l'ecran -- aucun banc sans fenetre ne le peut, et le
//    prochain lot (le peintre du fil) apportera son propre temoin, qui comparera
//    des rectangles PUBLIES PAR LEURS PEINTRES, en AVAL de la passe.
//
//  ⚠️ POURQUOI ELLE VIT ICI ET PAS DANS UN BANC NEUF. `Famille15_RolesAlerte`
//     mesure exactement la meme classe de defaut, trouvee six jours plus tot.
//     Un second harnais aurait deux tables de seuils qui divergeraient -- le
//     depot a deja paye « deux listes ecrites separement finissent toujours par
//     diverger » (l'en-tete de NKConverse le raconte).
//
//  ⚠️ LES QUATRE ESSAIS QUI PEUVENT DIRE NON, ET CE QU'ILS REFUTENT :
//     20c refute « j'ai pose la meme couleur pour les deux themes » -- la faute
//         exacte payee le 31/08 (CanvasBg) PUIS le 14/09 (StatusOk/Err).
//     20d refute « trois roles pour une seule couleur » : un lot qui aurait
//         recopie #161B22 trois fois passerait 20c et tomberait ici.
//     20f refute « les valeurs sont jolies mais le texte ne s'y lit pas » --
//         c'est `Validate()`, seuils du kit, les quatre paires neuves comprises.
//     20g refute « le role est fictif » : un theme qui ne le porte pas doit
//         tomber sur son repli ET L'ANNONCER, pas peindre transparent.
static void Famille20_SurfacesCode() {
	printf("\n[Famille 20] les trois surfaces du panneau IA\n");

	const NkTheme d = NkTheme::Dark();
	const NkTheme l = NkTheme::Light();

	// 20a — les trois existent et se nomment par la SURFACE, pas par l'appelant.
	Check("20a", Same(NkRoleName(NkRole::CodeBg), "code_bg") &&
					 Same(NkRoleName(NkRole::CodeOutBg), "code_out_bg") &&
					 Same(NkRoleName(NkRole::InlineCodeBg), "inline_code_bg"),
		  "les trois portent des cles de SURFACE (code_bg/code_out_bg/inline_code_bg)");

	// 20b — ils resolvent PAR NOM : c'est le chemin d'un theme de fichier.
	Check("20b", NkResolveRole("code_bg") == (uint16)NkRole::CodeBg &&
					 NkResolveRole("code_out_bg") == (uint16)NkRole::CodeOutBg &&
					 NkResolveRole("inline_code_bg") == (uint16)NkRole::InlineCodeBg,
		  "les trois cles resolvent sur les bons identifiants");

	// 20c — ATTENDU ECRIT AVANT LA MESURE. Chacun doit DIFFERER entre sombre et
	// clair. C'est l'essai qui tombe si quelqu'un pose la valeur une seule fois,
	// et c'est la faute que ce fichier a deja vue deux fois.
	const bool troisDifferents =
		d.Get(NkRole::CodeBg) != l.Get(NkRole::CodeBg) &&
		d.Get(NkRole::CodeOutBg) != l.Get(NkRole::CodeOutBg) &&
		d.Get(NkRole::InlineCodeBg) != l.Get(NkRole::InlineCodeBg);
	Check("20c", troisDifferents,
		  "les TROIS surfaces rendent une couleur differente en sombre et en clair");

	// 20d — et distinctes ENTRE ELLES dans chaque theme. Trois roles qui
	// rendraient la meme couleur seraient un inventaire, pas une palette.
	const bool distinctes =
		d.Get(NkRole::CodeBg) != d.Get(NkRole::CodeOutBg) &&
		d.Get(NkRole::CodeBg) != d.Get(NkRole::InlineCodeBg) &&
		d.Get(NkRole::CodeOutBg) != d.Get(NkRole::InlineCodeBg) &&
		l.Get(NkRole::CodeBg) != l.Get(NkRole::CodeOutBg) &&
		l.Get(NkRole::CodeBg) != l.Get(NkRole::InlineCodeBg) &&
		l.Get(NkRole::CodeOutBg) != l.Get(NkRole::InlineCodeBg);
	Check("20d", distinctes,
		  "les trois sont distinctes entre elles, dans CHAQUE theme");

	// 20e — LE POINT DE LA CAPTURE : la sortie est plus claire que l'entree en
	// SOMBRE, et plus sombre qu'elle en CLAIR. C'est l'ORDRE qui porte le sens
	// (« ce qui est RENDU se detache de ce qui a ete RECU »), et il s'inverse
	// avec le theme. Un lot qui recopierait la valeur sombre en clair passerait
	// 20c et 20d, et tomberait ici.
	const float32 cSombre = NkTheme::Contrast(d.Get(NkRole::CodeOutBg), d.Get(NkRole::PanelBg));
	const float32 cSombreIn = NkTheme::Contrast(d.Get(NkRole::CodeBg), d.Get(NkRole::PanelBg));
	const float32 cClair = NkTheme::Contrast(l.Get(NkRole::CodeOutBg), l.Get(NkRole::PanelBg));
	const float32 cClairIn = NkTheme::Contrast(l.Get(NkRole::CodeBg), l.Get(NkRole::PanelBg));
	Check("20e", cSombre > cSombreIn && cClair > cClairIn,
		  "la SORTIE s'ecarte du fond de panneau plus que l'ENTREE, dans les deux themes");
	printf("         ecart au PanelBg : sombre in=%.3f out=%.3f | clair in=%.3f out=%.3f\n",
		   (double)cSombreIn, (double)cSombre, (double)cClairIn, (double)cClair);

	// 20f — LE CONTRASTE DU TEXTE, contre le seuil que le kit s'impose. Les
	// QUATRE paires neuves sont dans `ContrastPairs` : `Validate` echoue si
	// l'une tombe. La plus serree est {TextMuted, CodeBg} en clair -- 4,66 pour
	// 4,50 exiges, et seulement parce que `Validate` COMPOSITE le noir
	// translucide au lieu de le lire comme du noir pur.
	NkThemeIssue pireD, pireL;
	Check("20f", d.Validate(&pireD) == 0 && l.Validate(&pireL) == 0,
		  "les deux themes passent leurs seuils, les 4 paires neuves comprises");
	if (l.Validate(nullptr) != 0)
		printf("         pire paire claire : %s sur %s = %.2f (exige %.2f)\n",
			   NkRoleName(pireL.fg), NkRoleName(pireL.bg), (double)pireL.ratio,
			   (double)pireL.required);

	// 20g — LE REPLI, sur le cas REEL : un theme de fichier ecrit avant
	// aujourd'hui ne porte aucune des trois lignes.
	NkRoleAudit::Reset();
	NkTheme ancien;                                 // tout magenta, sentinelles posees
	ancien.Set(NkRole::LabelCol, 0x161B22FFu);      // le repli de CodeBg est pose
	ancien.Set(NkRole::Border, 0x30363DFFu);        // celui d'InlineCodeBg aussi
	Check("20g", ancien.GetBrut(NkRole::CodeBg) == NkThemeNonDefini &&
					 ancien.GetBrut(NkRole::CodeOutBg) == NkThemeNonDefini,
		  "controle de depart : dans ce theme, les surfaces n'ont PAS ete posees");
	const NkThemeColor pCode = ancien.Get(NkRole::CodeBg);
	const NkThemeColor pOut = ancien.Get(NkRole::CodeOutBg);
	const NkThemeColor pInline = ancien.Get(NkRole::InlineCodeBg);
	Check("20h", pCode == 0x161B22FFu && pInline == 0x30363DFFu,
		  "un role non pose prend la couleur de son repli, pas la sentinelle");
	// 20i — LA CHAINE A DEUX SAUTS : CodeOutBg -> CodeBg -> LabelCol. Aucun des
	// deux premiers n'est pose ; sans le chainage, `pOut` serait transparent.
	Check("20i", pOut == 0x161B22FFu,
		  "CodeOutBg remonte DEUX crans (-> CodeBg -> LabelCol) au lieu de disparaitre");
	// 20j — et le repli s'ANNONCE. Un repli muet est exactement le defaut du
	// 14/09 : la couleur etait plausible et personne ne savait qu'elle etait
	// empruntee.
	Check("20j", NkRoleAudit::RepliCount() >= 3,
		  "les trois emprunts sont ANNONCES dans NkRoleAudit, pas silencieux");

	// 20k — CONTROLE NEGATIF. Sur un theme complet, aucun des trois ne doit
	// declencher de repli. Sans lui, un `Get` qui replierait toujours passerait
	// 20h et 20i sans rien prouver.
	NkRoleAudit::Reset();
	(void)d.Get(NkRole::CodeBg);
	(void)d.Get(NkRole::CodeOutBg);
	(void)d.Get(NkRole::InlineCodeBg);
	(void)l.Get(NkRole::CodeBg);
	(void)l.Get(NkRole::CodeOutBg);
	(void)l.Get(NkRole::InlineCodeBg);
	Check("20k", NkRoleAudit::RepliCount() == 0,
		  "controle negatif : sur les themes livres, AUCUN des trois n'emprunte");
	NkRoleAudit::Reset();
}

// ─────────────────────────────────────────────────────────────────────────────
//  FAMILLE 21 — LE CONTRAT DE DONNEES DU FIL (NkAiThread.h, 20/09/2026)
//
//  CE QU'ELLE MESURE : qu'un bloc SANS PRODUCTEUR REEL ne peut pas entrer dans
//  le fil, et qu'un bloc incomplet non plus. C'est la traduction executable de
//  la regle que deux chantiers ont ecrite separement -- `NkBrushDesc.h`
//  (« aucune primitive declaree sans etre implementee ») et `NkAiPanel.h`
//  (« pas de fausse carte d'outil qui ne ferait rien ») -- et de ce que Rodolf
//  a subi le matin du 20/09 : dix controles dont il ne comprenait pas l'usage.
//
//  ⚠️ CE QU'ELLE NE MESURE PAS, ET QUI VIENDRA AU LOT SUIVANT : que le fil se
//     PEIGNE juste. Aucun banc console ne le dit. Le temoin du peintre comparera
//     des rectangles PUBLIES PAR LEURS PEINTRES, place EN AVAL de la passe --
//     place en amont il lirait des zeros et conclurait « conforme », faute
//     payee le 20/09 meme sur le panneau du modeleur.
static void Famille21_ContratDuFil() {
	printf("\n[Famille 21] le contrat de donnees du fil\n");

	auto Bloc = [](NkAiBloc t, const char *texte) {
		NkAiBlocDonnees b;
		b.type = t;
		b.texte = NkString(texte);
		return b;
	};

	// 21a — un porteur qui n'a RIEN declare n'accepte que la demande de
	// l'utilisateur. Le defaut permissif serait le defaut invisible.
	{
		NkAiFil fil;
		NkString pourquoi;
		const bool dem = fil.Pousser(Bloc(NkAiBloc::Demande, "subdivise le cube"), pourquoi);
		const bool pro = fil.Pousser(Bloc(NkAiBloc::Prose, "voila"), pourquoi);
		Check("21a", dem && !pro && fil.Taille() == 1,
			  "porteur muet : la DEMANDE entre (elle vient de l'utilisateur), la prose non");
		printf("         motif du refus : %s\n", pourquoi.CStr());
	}

	// 21b — LE POINT DU LOT. `Reflexion` n'a aucun producteur aujourd'hui : meme
	// un porteur qui declare TOUT LE RESTE ne peut pas en pousser une.
	// ⚠️ CET ESSAI EST ECRIT POUR TOMBER le jour ou un dorsal emettra vraiment
	//    de la reflexion -- il faudra alors poser `produitReflexion` ET changer
	//    cette ligne. C'est voulu : *un contournement dit quand le supprimer.*
	{
		NkAiCapacites tout;
		tout.produitProse = tout.produitOutil = tout.produitRefus = true;
		tout.produitEchec = tout.produitEffet = true; // tout SAUF la reflexion
		NkAiFil fil;
		fil.Declarer(tout);
		NkString pourquoi;
		const bool refl = fil.Pousser(Bloc(NkAiBloc::Reflexion, "hmm"), pourquoi);
		Check("21b", !refl && fil.Taille() == 0,
			  "un bloc « reflexion » n'entre PAS : aucun dorsal n'en emet aujourd'hui");
		printf("         motif du refus : %s\n", pourquoi.CStr());
	}

	// 21c — CONTROLE NEGATIF DE 21b. Sans lui, un `Pousser` qui refuserait TOUT
	// passerait 21a et 21b sans rien prouver. Le jour ou la reflexion aura une
	// source, ce meme chemin doit l'accepter -- on le verifie des maintenant en
	// posant le drapeau a la main.
	{
		NkAiCapacites c = NkAiCapacites::Texte();
		c.produitReflexion = true; // ce qu'un dorsal fera le jour venu
		NkAiFil fil;
		fil.Declarer(c);
		NkString pourquoi;
		const bool refl = fil.Pousser(Bloc(NkAiBloc::Reflexion, "hmm"), pourquoi);
		Check("21c", refl && fil.Taille() == 1,
			  "controle negatif : DECLAREE, la reflexion entre -- le refus vise le type, pas tout");
	}

	// 21d — un bloc INCOMPLET n'entre pas, meme si son type est declare. Un
	// refus sans motif est le defaut exact que le modeleur a paye le 20/09 au
	// matin : Ollama eteint, annonce comme un fichier manquant.
	{
		NkAiCapacites c = NkAiCapacites::Texte();
		c.produitRefus = true;
		c.produitEffet = true;
		NkAiFil fil;
		fil.Declarer(c);
		NkString p1, p2, p3;
		NkAiBlocDonnees refusMuet;
		refusMuet.type = NkAiBloc::Refus; // motif vide
		NkAiBlocDonnees effetVide;
		effetVide.type = NkAiBloc::Effet; // effet vide
		const bool r1 = fil.Pousser(refusMuet, p1);
		const bool r2 = fil.Pousser(effetVide, p2);
		const bool r3 = fil.Pousser(Bloc(NkAiBloc::Prose, ""), p3);
		Check("21d", !r1 && !r2 && !r3 && fil.Taille() == 0,
			  "refus sans motif, effet sans mesure, prose vide : les trois sont refuses");
		printf("         motifs : %s | %s\n", p1.CStr(), p2.CStr());
	}

	// 21e — LES DEUX RAISONS DE REFUS SE DISTINGUENT. « le porteur ne produit
	// pas ce type » et « ce bloc est incomplet » demandent deux corrections
	// OPPOSEES : declarer, ou remplir. Un booleen seul enverrait chercher la
	// mauvaise, et c'est le genre de message qui fait perdre une heure.
	{
		NkAiCapacites c = NkAiCapacites::Texte();
		NkAiFil fil;
		fil.Declarer(c);
		NkString pType, pVide;
		NkAiBlocDonnees outil;
		outil.type = NkAiBloc::Outil;
		outil.titre = NkString("Bash");
		(void)fil.Pousser(outil, pType);          // type non declare
		(void)fil.Pousser(Bloc(NkAiBloc::Prose, ""), pVide); // type OK, bloc vide
		const bool distincts = pType.Length() > 0 && pVide.Length() > 0 &&
							   !Same(pType.CStr(), pVide.CStr());
		Check("21e", distincts,
			  "les deux raisons de refus rendent DEUX motifs differents, pas un booleen");
	}

	// 21f — LA DEMANDE N'EST JAMAIS REPLIEE, meme poussee repliee. C'est la
	// capture : la question est encadree en tete, lisible sans un geste.
	{
		NkAiFil fil;
		NkString pourquoi;
		NkAiBlocDonnees d = Bloc(NkAiBloc::Demande, "subdivise le cube deux fois");
		d.replie = true; // on essaie de la replier
		(void)fil.Pousser(d, pourquoi);
		Check("21f", fil.Taille() == 1 && !fil.At(0).replie,
			  "la DEMANDE entre depliee, meme poussee repliee");
		fil.BasculerParId(fil.At(0).id); // par le NOM, jamais la position (corrige le 20/09 au soir)
		Check("21g", !fil.At(0).replie,
			  "et elle ne se replie pas non plus au clic : une question repliee n'en est plus une");
	}

	// 21h — LA FENETRE GLISSANTE PERD LE PLUS ANCIEN, JAMAIS LE PLUS RECENT.
	// ⚠️ Ecrit parce que l'implementation naive fait l'inverse : un plafond
	//    applique AVANT l'ajout REFUSE le bloc neuf. La sortie de l'utilisateur
	//    est la meme (« le fil ne grandit plus »), la perte est l'opposee.
	{
		NkAiFil fil;
		fil.Declarer(NkAiCapacites::Texte());
		fil.PoserPlafond(3);
		NkString pourquoi;
		for (int i = 0; i < 6; ++i) {
			char t[16];
			snprintf(t, sizeof(t), "bloc %d", i);
			(void)fil.Pousser(Bloc(NkAiBloc::Prose, t), pourquoi);
		}
		const bool bonneFin = fil.Taille() == 3 && Same(fil.At(2).texte.CStr(), "bloc 5") &&
							  Same(fil.At(0).texte.CStr(), "bloc 3");
		Check("21h", bonneFin,
			  "plafond 3 apres 6 blocs : il reste 3, 4, 5 -- le PLUS RECENT survit");
	}

	// 21i — LA MARQUE. Elle compte ce qui n'a pas ete PEINT, et elle ne se
	// consomme qu'a la peinture. C'est la lecon de `d31c127e9` remontee dans le
	// kit : consommee au clic, elle marquerait « vu » un panneau que ce clic
	// venait peut-etre de fermer.
	{
		NkAiFil fil;
		fil.Declarer(NkAiCapacites::Texte());
		NkString pourquoi;
		(void)fil.Pousser(Bloc(NkAiBloc::Prose, "a"), pourquoi);
		(void)fil.Pousser(Bloc(NkAiBloc::Prose, "b"), pourquoi);
		const uint32 avant = fil.NonVus();
		fil.MarquerVus();
		const uint32 apres = fil.NonVus();
		(void)fil.Pousser(Bloc(NkAiBloc::Prose, "c"), pourquoi);
		Check("21i", avant == 2 && apres == 0 && fil.NonVus() == 1,
			  "la marque compte les blocs non peints : 2, puis 0, puis 1");
	}

	// 21j — ET ELLE NE MENT PAS A LA REMISE A ZERO. Une nouvelle conversation
	// n'a rien a faire voir : la pastille ne doit pas rester marquee pour des
	// blocs qui n'existent plus.
	{
		NkAiFil fil;
		fil.Declarer(NkAiCapacites::Texte());
		NkString pourquoi;
		(void)fil.Pousser(Bloc(NkAiBloc::Prose, "a"), pourquoi);
		fil.Vider();
		Check("21j", fil.Taille() == 0 && fil.NonVus() == 0,
			  "un fil vide ne laisse AUCUN bloc en attente derriere lui");
	}

	// 21k — chaque type porte une cle stable et non vide : c'est l'acces du
	// FICHIER, celui d'un fil enregistre et des journaux.
	{
		bool tous = true;
		for (uint8 i = 0; i < (uint8)NkAiBloc::Count; ++i)
			if (NkAiBlocNom((NkAiBloc)i)[0] == '\0')
				tous = false;
		Check("21k", tous, "les sept types de bloc portent tous une cle stable non vide");
	}
}

//  ⚠️ NUMEROTATION : 16 et 19, avec un TROU en 17-18. La famille 15 est
//     celle du chantier THEME (`Famille15_RolesAlerte`) ; la mienne s'est
//     decalee a la fusion du 14/09 -- deux familles sous le meme numero
//     rendraient les journaux illisibles. Le trou est deliberе : renumeroter
//     19 perimerait un message de commit et des renvois de canal pour une
//     contiguite qui n'apporte rien.
//  FAMILLE 16 — LA BANDE D'ONGLETS PARTAGEE (canal `onglets.questions.md`, o1)
// =============================================================================
//  CE QU'ELLE PROUVE, ET CE QU'ELLE NE PROUVE PAS.
//
//  Rodolf a demande que la bande d'onglets soit COMBLEE DANS LA COQUILLE, pas
//  recopiee chez Nogee. L'attendu derive du canal est ecrit noir sur blanc :
//  « les onglets de Nogee et ceux du modeleur doivent venir du MEME code. Le
//  prouver autrement qu'en le disant : une mutation dans la coquille doit
//  changer les deux, ou le partage est une fiction. »
//
//  ⚠️ CE BANC NE PEUT PAS LANCER LES DEUX APPLICATIONS. Il prouve la propriete
//     qui rend l'affirmation vraie : **la geometrie de la bande ne depend que
//     du peintre et de la table de metriques partagee.** Deux hotes qui
//     appellent `NkDrawTabStrip` avec les memes mesures de texte obtiennent la
//     MEME bande, et une mutation de la metrique les deplace tous les deux.
//
//  ⚠️ ET CE QU'IL NE PROUVE PAS EST DIT : il ne montre aucun pixel. Le temoin
//     visuel de la bande existe ailleurs (`NkOngletSonde`, captures
//     `sonde_onglets/captures/`), et il est pris avec le peintre NKGui de la
//     coquille — pas avec celui de NK3DModeler, dont l'application ne compile
//     pas sur cette branche pour des raisons ANTERIEURES a ce lot (merge
//     `e249d7151` ; 12 erreurs dans `main.cpp`, `NkModelerImport.h` et
//     `NkDemo3D.cpp`, aucune dans un fichier de ce lot). Cf. le canal.
namespace tabprobe {

	using namespace nkentseu::editorkit;

	/// ⚠️ UN SECOND PEINTRE, ET IL DIFFERE DE L'AUTRE LA OU CA COMPTE. Deux
	///    instances de la meme classe ne prouveraient rien : elles partagent
	///    jusqu'aux defauts. Celui-ci REMPLACE la mesure de texte — c'est la
	///    seule entree par laquelle un hote peut faire varier la largeur d'un
	///    onglet. S'il produisait la meme bande que l'autre, ce serait le signe
	///    que la largeur NE depend PAS du texte, donc que le composant ignore
	///    son peintre.
	class PeintreLarge final : public NkRecordingPaint {
		public:
			float32 TextWidth(const char *s) const override {
				usize n = 0;
				while (s && s[n])
					++n;
				return (float32)n * 12.f; // 12 px/caractere au lieu de 7
			}
	};

	/// Les rectangles d'onglets EMIS, dans l'ordre. On lit le flux plutot que le
	/// resultat rendu : un composant pourrait rapporter une geometrie et en
	/// peindre une autre — c'est exactement le defaut « declarer n'est pas
	/// livrer » que ce depot a paye. Ici on mesure CE QUI EST PEINT.
	inline void RectsPeints(const NkRecordingPaint &p, NkVector<float32> &xs,
							NkVector<float32> &ws) {
		xs.Clear();
		ws.Clear();
		for (usize i = 0; i < p.cmds.Size(); ++i) {
			const NkPaintCmd &c = p.cmds[i];
			// Les fonds d'onglets : des `Fill` arrondis. Le fond de BANDE, lui,
			// n'est pas arrondi -- c'est ce qui les distingue sans compter sur
			// un ordre d'emission.
			if (c.op == NkPaintOp::Fill && c.rounding > 0.f && c.h > 0.f) {
				xs.PushBack(c.x);
				ws.PushBack(c.w);
			}
		}
	}

	inline void Modele(NkTabStripModel &m) {
		m.tabs.Clear();
		NkTabItem a;
		a.id = 1;
		a.label = NkString("Scene");
		m.tabs.PushBack(a);
		NkTabItem b;
		b.id = 2;
		b.label = NkString("Eclairage");
		m.tabs.PushBack(b);
		m.active = 1;
	}

	inline NkTabStripStyle Style(const NkComponentInstance *inst) {
		NkTabStripStyle s;
		s.bandBg = 1;
		s.border = 2;
		s.tabBg = 3;
		s.tabHoverBg = 4;
		s.tabActiveBg = 5;
		s.text = 6;
		s.textMuted = 7;
		s.accent = 8;
		s.values = inst;
		return s;
	}

	/// Rend le nombre d'essais reussis et le total.
	inline void Sonder(uint32 &ok, uint32 &total) {
		NkComponentInput in; // souris HORS de la bande : aucun survol, aucun clic
		in.mouseX = -1000.f;
		in.mouseY = -1000.f;
		const NkPaintRect rect{0.f, 0.f, 900.f, 28.f};

		NkVector<float32> xs1, ws1, xs2, ws2, xs3, ws3;

		// ── (a) LE MEME CODE, DEUX PEINTRES : la bande suit son peintre ──────
		NkTabStripModel m1;
		Modele(m1);
		NkRecordingPaint p1;
		NkDrawTabStrip(p1, in, rect, m1, Style(nullptr), NkTabStripHooks{});
		RectsPeints(p1, xs1, ws1);

		NkTabStripModel m2;
		Modele(m2);
		PeintreLarge p2;
		NkDrawTabStrip(p2, in, rect, m2, Style(nullptr), NkTabStripHooks{});
		RectsPeints(p2, xs2, ws2);

		++total;
		if (xs1.Size() == 2 && xs2.Size() == 2) {
			++ok;
		} else {
			printf("  [FAIL] 16.a la bande n'a pas emis DEUX onglets (%u et %u)\n",
				   (unsigned)xs1.Size(), (unsigned)xs2.Size());
		}

		// ⚠️ L'ATTENDU EST DERIVE, PAS ECRIT. La largeur d'un onglet vaut
		//    `TextWidth(libelle) + tab_pad_x`. On la recalcule a partir des
		//    parametres du banc (5 et 9 caracteres, 7 puis 12 px) et de la
		//    metrique LUE dans la declaration — jamais d'un nombre recopie ici,
		//    qui se perimerait le jour ou Rodolf change `tab_pad_x`.
		const float32 pad = NkTabStripDecl().Metric("tab_pad_x");
		++total;
		if (xs1.Size() == 2 && ws1[0] > 0.f) {
			const float32 attendu0 = 5.f * 7.f + pad;  // "Scene"
			const float32 attendu1 = 9.f * 7.f + pad;  // "Eclairage"
			const bool bon = (ws1[0] > attendu0 - 0.01f && ws1[0] < attendu0 + 0.01f) &&
							 (ws1[1] > attendu1 - 0.01f && ws1[1] < attendu1 + 0.01f);
			if (bon)
				++ok;
			else
				printf("  [FAIL] 16.b largeurs %0.2f/%0.2f, attendu %0.2f/%0.2f\n", ws1[0], ws1[1],
					   attendu0, attendu1);
		} else {
			printf("  [FAIL] 16.b pas de rectangle a mesurer\n");
		}

		// ⚠️ LE NEGATIF : le SECOND peintre doit donner d'AUTRES largeurs. S'il
		//    donnait les memes, le composant n'ecouterait pas son peintre — et
		//    l'essai (a) serait vert pour une raison qui n'a rien a voir.
		++total;
		if (xs1.Size() == 2 && xs2.Size() == 2) {
			const bool differe = (ws2[0] > ws1[0] + 1.f) && (ws2[1] > ws1[1] + 1.f);
			if (differe)
				++ok;
			else
				printf("  [FAIL] 16.c NEGATIF : deux peintres differents rendent la meme bande "
					   "(%0.2f vs %0.2f) -- le composant ignore son peintre\n",
					   ws1[0], ws2[0]);
		} else {
			++total; // rien a comparer : on ne credite pas
		}

		// ── (d) LA MUTATION PARTAGEE : elle deplace TOUT hote ────────────────
		// C'est la reponse a « une mutation dans la coquille doit changer les
		// deux ». On mute la metrique PARTAGEE et on verifie que la bande du
		// premier peintre bouge de la difference EXACTE.
		NkComponentInstance inst;
		inst.Bind(NkTabStripDecl());
		const float32 padMute = pad + 36.f;
		inst.SetMetric("tab_pad_x", padMute);

		NkTabStripModel m3;
		Modele(m3);
		NkRecordingPaint p3;
		NkDrawTabStrip(p3, in, rect, m3, Style(&inst), NkTabStripHooks{});
		RectsPeints(p3, xs3, ws3);

		++total;
		if (xs1.Size() == 2 && xs3.Size() == 2) {
			const float32 d0 = ws3[0] - ws1[0];
			const float32 d1 = ws3[1] - ws1[1];
			const bool bon = (d0 > 35.99f && d0 < 36.01f) && (d1 > 35.99f && d1 < 36.01f);
			if (bon)
				++ok;
			else
				printf("  [FAIL] 16.d la mutation partagee decale de %0.2f/%0.2f, attendu 36/36\n",
					   d0, d1);
		} else {
			printf("  [FAIL] 16.d pas de rectangle a comparer\n");
		}

		// ⚠️ LE NEGATIF DE LA MUTATION : une metrique que la bande N'UTILISE PAS
		//    ne doit RIEN changer. Sans cet essai, « la mutation a change la
		//    bande » pourrait vouloir dire « toute mutation la change », ce qui
		//    serait un composant qui se redessine au hasard, pas un composant
		//    qui honore ses parametres.
		NkComponentInstance inertie;
		inertie.Bind(NkTabStripDecl());
		inertie.SetMetric("seg_min_w", 999.f); // variante Segmente seulement
		NkTabStripModel m4;
		Modele(m4);
		NkRecordingPaint p4;
		NkDrawTabStrip(p4, in, rect, m4, Style(&inertie), NkTabStripHooks{});
		NkVector<float32> xs4, ws4;
		RectsPeints(p4, xs4, ws4);
		++total;
		if (xs1.Size() == xs4.Size() && xs4.Size() == 2) {
			const bool inchange = (ws4[0] > ws1[0] - 0.01f && ws4[0] < ws1[0] + 0.01f) &&
								  (ws4[1] > ws1[1] - 0.01f && ws4[1] < ws1[1] + 0.01f);
			if (inchange)
				++ok;
			else
				printf("  [FAIL] 16.e NEGATIF : une metrique INUTILISEE par la variante "
					   "Documents a quand meme deplace la bande (%0.2f -> %0.2f)\n",
					   ws1[0], ws4[0]);
		} else {
			printf("  [FAIL] 16.e comptes d'onglets differents\n");
		}

		// ── (f) LE CLIP EST EQUILIBRE ───────────────────────────────────────
		// Un `PushClip` sans son `PopClip` laisse tout ce qui suit rogne a la
		// bande — un defaut qui ne se voit pas dans la bande elle-meme, mais
		// dans le panneau d'a cote.
		++total;
		if (p1.ClipBalanced())
			++ok;
		else
			printf("  [FAIL] 16.f PushClip / PopClip desequilibres\n");
	}

} // namespace tabprobe

// =============================================================================
//  FAMILLE 19 — (o3) LA PORTE DU CHROME : un menu par-dessus la bande d'onglets
// =============================================================================
//  LE DEFAUT MESURE. `NkEditorShell::DrawTabStrip` est appelee ligne 906 ; le
//  masquage d'entree du corps vit lignes 923-945, donc APRES. La bande recevait
//  `mUI.input` NON FILTRE. Et la geometrie ne laisse pas de doute : un menu de
//  la barre de titre (y 0..30) se deroule vers le BAS, par-dessus la bande
//  (y 30..58). Un clic destine a « Fichier > Ouvrir » pouvait activer l'onglet
//  du dessous -- ou le FERMER, si la croix tombait sous le pointeur.
//
//  ⚠️ MEME FAMILLE que le defaut signale par Rodolf sur un panneau et traite le
//     meme matin dans `DrawPanels`. Meme cause (un site qui lit la souris sans
//     demander si le point est atteignable), donc MEME PORTE : `PointReachable`.
//     On n'en ecrit pas une seconde.
//
//  CE QUE CETTE FAMILLE PROUVE, ET CE QU'ELLE NE PROUVE PAS
//    * elle prouve que le COMPOSANT agit sur n'importe quel clic qu'on lui
//      donne -- donc que le filtrage ne peut venir que de l'hote ;
//    * elle prouve que la PORTE repond correctement : une surface declaree
//      au-dessus rend le point inatteignable, et rien d'autre ne change ;
//    * elle prouve que la COMPOSITION des deux (l'expression exacte posee dans
//      `DrawTabStrip`) supprime le clic fantome et garde le clic legitime.
//    * ⚠️ Elle ne fait PAS tourner `NkEditorShell` : la coquille veut une
//      fenetre. Que l'expression soit bien CELLE-LA dans `DrawTabStrip` est
//      verifie par lecture, pas par ce banc. C'est dit plutot que tu.
namespace porteprobe {

	using namespace nkentseu::editorkit;
	using nkentseu::nkgui::NkGuiContext;

	/// La bande d'essai et un clic AU CENTRE DU PREMIER ONGLET.
	inline void Poser(NkTabStripModel &m) {
		m.tabs.Clear();
		NkTabItem a;
		a.id = 1;
		a.label = nkentseu::NkString("Scene");
		m.tabs.PushBack(a);
		NkTabItem b;
		b.id = 2;
		b.label = nkentseu::NkString("Eclairage");
		m.tabs.PushBack(b);
		m.active = 2; // l'ACTIF est le second : un clic sur le premier doit changer
	}

	inline NkTabStripStyle Style() {
		NkTabStripStyle s;
		s.bandBg = 1; s.border = 2; s.tabBg = 3; s.tabHoverBg = 4;
		s.tabActiveBg = 5; s.text = 6; s.textMuted = 7; s.accent = 8;
		return s;
	}

	inline void Sonder(uint32 &ok, uint32 &total) {
		const NkPaintRect bande{0.f, 30.f, 900.f, 28.f};
		// Un point DANS le premier onglet : x = 10 (marge) + un peu.
		const float32 px = 40.f, py = 42.f;

		auto clic = [&](bool atteignable) {
			NkComponentInput in;
			in.mouseX = px;
			in.mouseY = py;
			in.mousePressed = true;
			// ⚠️ C'EST L'EXPRESSION EXACTE POSEE DANS `DrawTabStrip`. Si elle
			//    change la-bas, ce banc cesse de mesurer ce qu'il croit -- d'ou le
			//    commentaire, faute de pouvoir la partager sans tirer NKGui ici.
			if (!atteignable) {
				in.mousePressed = false;
				in.mouseReleased = false;
				in.mouseDown = false;
				in.doubleClick = false;
				in.rightPressed = false;
				in.wheel = 0.f;
			}
			return in;
		};

		// ── 19.a LE COMPOSANT AGIT SUR TOUT CLIC QU'ON LUI DONNE ────────────
		// Positif : sans filtrage, le clic active l'onglet 1. C'est la preuve que
		// le composant ne se protege PAS lui-meme -- et il ne le doit pas : il ne
		// sait rien des surfaces qui le recouvrent.
		{
			NkTabStripModel m; Poser(m);
			NkRecordingPaint p;
			const NkTabStripResult r = NkDrawTabStrip(p, clic(true), bande, m, Style(), NkTabStripHooks{});
			++total;
			if (r.selectionChanged && m.active == 1) ++ok;
			else printf("  [FAIL] 19.a un clic non filtre aurait du activer l'onglet 1 (actif=%u)\n",
						(unsigned)m.active);
		}

		// ── 19.b LE CLIC FANTOME EST SUPPRIME ───────────────────────────────
		{
			NkTabStripModel m; Poser(m);
			NkRecordingPaint p;
			const NkTabStripResult r = NkDrawTabStrip(p, clic(false), bande, m, Style(), NkTabStripHooks{});
			++total;
			if (!r.selectionChanged && m.active == 2) ++ok;
			else printf("  [FAIL] 19.b le clic sous une surface a quand meme agi (actif=%u)\n",
						(unsigned)m.active);
		}

		// ── 19.c LA PORTE DU KIT REPOND JUSTE ───────────────────────────────
		// On declare une surface de couche SUPERIEURE par-dessus la bande, et on
		// interroge `PointReachable` -- la vraie, celle que la coquille appelle.
		{
			static NkGuiContext ctx;
			ctx.occlCount = 0;
			ctx.curInputLayer = 0; // la couche du CHROME
			// un menu deroulant : il couvre la bande
			ctx.occlRects[0] = nkentseu::nkgui::NkRect{0.f, 28.f, 200.f, 160.f};
			ctx.occlLayers[0] = (int32)NkCouche::Menu;
			ctx.occlCount = 1;
			const bool sousLeMenu = !ctx.PointReachable({px, py});
			const bool aCote = ctx.PointReachable({700.f, 42.f}); // meme bande, hors du menu
			++total;
			if (sousLeMenu && aCote) ++ok;
			else printf("  [FAIL] 19.c la porte repond mal (sous le menu=%d, a cote=%d ; "
						"attendu 1 et 1)\n", sousLeMenu ? 1 : 0, aCote ? 1 : 0);
		}

		// ── 19.d NEGATIF : SANS MENU, RIEN NE CHANGE ────────────────────────
		// Sans cet essai, « le clic ne passe plus » pourrait vouloir dire « plus
		// aucun clic ne passe », et la bande serait devenue inerte.
		{
			static NkGuiContext ctx;
			ctx.occlCount = 0;
			ctx.curInputLayer = 0;
			const bool libre = ctx.PointReachable({px, py});
			NkTabStripModel m; Poser(m);
			NkRecordingPaint p;
			const NkTabStripResult r = NkDrawTabStrip(p, clic(libre), bande, m, Style(), NkTabStripHooks{});
			++total;
			if (libre && r.selectionChanged && m.active == 1) ++ok;
			else printf("  [FAIL] 19.d NEGATIF : aucune surface declaree et le clic ne passe pas "
						"(atteignable=%d, actif=%u)\n", libre ? 1 : 0, (unsigned)m.active);
		}
	}

} // namespace porteprobe

// =============================================================================
int main(int argc, char **argv) {
	(void)argc;
	(void)argv;
	printf("=== BANC NKEditorKit — roles de theme et backend graphique ===\n");

	Famille1_Canonisation();
	Famille2_JetonsReels();
	Famille3_RepliFranc();
	Famille4_BackendGraphique();
	Famille15_RolesAlerte();
	Famille20_SurfacesCode();
	Famille21_ContratDuFil();
	// Famille 5 — le rail du selecteur. Elle tient son propre compte et rend un
	// BILAN : on additionne les deux nombres, sinon deux echecs vaudraient un.
	{
		const navprobe::Bilan b5 = navprobe::Sonder();
		gPassed += (uint32)b5.ok;
		gFailed += (uint32)(b5.total - b5.ok);
	}
	// Famille 16 — la bande d'onglets PARTAGEE (canal onglets, o1). Meme forme de
	// bilan que la 5 : on additionne les deux nombres, sinon deux echecs
	// vaudraient un.
	{
		printf("\n--- Famille 16 : la bande d'onglets partagee ---\n");
		uint32 ok16 = 0, total16 = 0;
		tabprobe::Sonder(ok16, total16);
		printf("  famille 16 : %u/%u\n", ok16, total16);
		gPassed += ok16;
		gFailed += (total16 - ok16);
	}
	// Famille 19 — (o3) la porte du chrome : un menu par-dessus la bande.
	{
		printf("\n--- Famille 19 : la porte du chrome ---\n");
		uint32 ok19 = 0, total19 = 0;
		porteprobe::Sonder(ok19, total19);
		printf("  famille 19 : %u/%u\n", ok19, total19);
		gPassed += ok19;
		gFailed += (total19 - ok19);
	}

	// Famille 22 - le plan du fil du panneau IA. Meme forme que 5, 16 et 19 :
	// elle tient son propre compte et rend un BILAN, quon ADDITIONNE -- sinon
	// deux echecs vaudraient un.
	{
		printf("\n--- Famille 22 : le plan du fil (panneau IA) ---\n");
		const aiplanprobe::Bilan b22 = aiplanprobe::Sonder();
		printf("  famille 22 : %u/%u\n", b22.ok, b22.total);
		gPassed += b22.ok;
		gFailed += (b22.total - b22.ok);
	}

	printf("\n---------------------------------------------\n");
	printf("RESULTAT : %u/%u\n", gPassed, gPassed + gFailed);
	if (gFailed) {
		printf("%u ESSAI(S) EN ECHEC.\n", gFailed);
		return 1;
	}
	printf("Tout est vert.\n");
	return 0;
}
