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
// Famille 5 — le RAIL du selecteur de fichiers. Le banc vit DANS LE KIT
// (`NkFilePickerNavProbe.h`) : c'est le kit qu'il mesure, et une fusion doit
// l'emporter avec le correctif qu'il garde. Ici, une ligne d'appel.
#include "NKEditorKit/NkFilePickerNavProbe.h"

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
	// Famille 5 — le rail du selecteur. Elle tient son propre compte et rend un
	// BILAN : on additionne les deux nombres, sinon deux echecs vaudraient un.
	{
		const navprobe::Bilan b5 = navprobe::Sonder();
		gPassed += (uint32)b5.ok;
		gFailed += (uint32)(b5.total - b5.ok);
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
