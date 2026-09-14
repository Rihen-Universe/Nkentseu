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
//  FAMILLE 15 — LA BANDE D'ONGLETS PARTAGEE (canal `onglets.questions.md`, o1)
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
			printf("  [FAIL] 15.a la bande n'a pas emis DEUX onglets (%u et %u)\n",
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
				printf("  [FAIL] 15.b largeurs %0.2f/%0.2f, attendu %0.2f/%0.2f\n", ws1[0], ws1[1],
					   attendu0, attendu1);
		} else {
			printf("  [FAIL] 15.b pas de rectangle a mesurer\n");
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
				printf("  [FAIL] 15.c NEGATIF : deux peintres differents rendent la meme bande "
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
				printf("  [FAIL] 15.d la mutation partagee decale de %0.2f/%0.2f, attendu 36/36\n",
					   d0, d1);
		} else {
			printf("  [FAIL] 15.d pas de rectangle a comparer\n");
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
				printf("  [FAIL] 15.e NEGATIF : une metrique INUTILISEE par la variante "
					   "Documents a quand meme deplace la bande (%0.2f -> %0.2f)\n",
					   ws1[0], ws4[0]);
		} else {
			printf("  [FAIL] 15.e comptes d'onglets differents\n");
		}

		// ── (f) LE CLIP EST EQUILIBRE ───────────────────────────────────────
		// Un `PushClip` sans son `PopClip` laisse tout ce qui suit rogne a la
		// bande — un defaut qui ne se voit pas dans la bande elle-meme, mais
		// dans le panneau d'a cote.
		++total;
		if (p1.ClipBalanced())
			++ok;
		else
			printf("  [FAIL] 15.f PushClip / PopClip desequilibres\n");
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
	// Famille 5 — le rail du selecteur. Elle tient son propre compte et rend un
	// BILAN : on additionne les deux nombres, sinon deux echecs vaudraient un.
	{
		const navprobe::Bilan b5 = navprobe::Sonder();
		gPassed += (uint32)b5.ok;
		gFailed += (uint32)(b5.total - b5.ok);
	}
	// Famille 15 — la bande d'onglets PARTAGEE (canal onglets, o1). Meme forme de
	// bilan que la 5 : on additionne les deux nombres, sinon deux echecs
	// vaudraient un.
	{
		printf("\n--- Famille 15 : la bande d'onglets partagee ---\n");
		uint32 ok15 = 0, total15 = 0;
		tabprobe::Sonder(ok15, total15);
		printf("  famille 15 : %u/%u\n", ok15, total15);
		gPassed += ok15;
		gFailed += (total15 - ok15);
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

	printf("\n---------------------------------------------\n");
	printf("RESULTAT : %u/%u\n", gPassed, gPassed + gFailed);
	if (gFailed) {
		printf("%u ESSAI(S) EN ECHEC.\n", gFailed);
		return 1;
	}
	printf("Tout est vert.\n");
	return 0;
}
