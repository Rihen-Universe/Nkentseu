// =============================================================================
// NkEditeurBancIA.cpp — le banc de l'IA INTEGREE (R18, document 05)
//
// Sans Ollama, sans modele, sans reseau : un FAUX SERVEUR local
// (NKConverse/NkConverseFauxServeur.h) parle les vrais protocoles sur de vrais
// sockets ; l'editeur est hors ecran (NkEditeurBancTrame) ; les gestes passent
// par l'entree de la trame (clic, frappe, Entree, Ctrl+Z).
//
// PRE-ENREGISTREMENT :
//   (ia1)  Ollama : la liste vient du serveur (/api/tags) ; /api/show dit lequel
//          sait les outils
//   (ia2)  Tester la connexion : ok / modele absent (« ollama pull X ») /
//          serveur absent (« Lancez Ollama »)
//   (ia3)  OpenAI : la liste avec la cle ; une mauvaise cle est REFUSEE (401) ;
//          Anthropic sans cle : CLE ABSENTE, sans rien envoyer ; Anthropic en
//          flux : un appel d'outil aux arguments fragmentes est recolle
//   (ia4)  une conversation EN FLUX : le texte grandit a l'ecran en plusieurs
//          images ; le message systeme parti est celui de l'EDITEUR (outils,
//          catalogue)
//   (ia5)  « ajoute une caisse au centre » : l'outil creer_acteur pose une
//          Caisse au centre de la vue ; un bloc d'effet ; le resultat repart
//   (ia6)  Ctrl+Z (clavier de l'editeur) retire la caisse
//   (ia7)  une suppression de fichier ATTEND la confirmation ; « Refuser » :
//          le fichier reste ; « Confirmer » : il part ; Ctrl+Z le rend
//   (ia8)  un modele SANS outils natifs : des blocs <outil> dans le texte
//          agissent (la balle en (2, 3)) ; aucun champ « tools » n'est parti
//   (ia9)  un modele qui REFUSE les outils natifs (400) : l'editeur repasse en
//          texte tout seul
//   (ia10) OpenAI en flux : ecrire_gdd aux arguments fragmentes ecrit le GDD
//   (ia11) un chemin hors du projet est refuse, rien n'est ecrit
//   (ia12) la description est ENGENDREE : elle nomme une entite creee a
//          l'instant, les noeuds Blueprint, les acteurs
//   (ia13) le panneau : un ONGLET du groupe Details | Monde par defaut ;
//          « Detacher » en fait un panneau a part ; son chevron le REPLIE en
//          bande ; la disposition est RETENUE ; « Rattacher » le remet en onglet
//   CONTRE-EPREUVES (une mutation, le temoin doit passer au ROUGE) :
//   (ce1) un nom d'outil faux -> le temoin de creation voit ECHEC
//   (ce2) sans la photo avant l'outil -> le temoin de Ctrl+Z voit ECHEC
//   (ce3) sans la confirmation -> le temoin « le fichier attend » voit ECHEC
//   (ce4) une reponse en UN morceau -> le temoin du flux voit ECHEC
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Ia/NkEditeurHarnaisIA.h"

#include "NKConverse/NkConverseFournisseurs.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			int32 gR = 0, gE = 0;
			void Temoin(bool ok, const char *quoi, float32 v = 0.f) {
				std::printf("  [%s] %-74s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}
			bool Contient(const NkString &s, const char *m) {
				return s.Find(m) != NkString::npos;
			}
			ecs::NkEntityId ParUid(NkEditeurModele &m, uint64 uid) {
				return uid ? m.scene.EntiteParUid(uid) : ecs::NkEntityId::Invalid();
			}
			/// L'uid d'une entite dont le nom COMMENCE par `nom` (« Caisse » trouve
			/// « Caisse_4 » : le catalogue numerote ce qu'il pose), 0 sinon. Parmi
			/// `exclus` (une photo d'avant), aucune n'est rendue.
			uint64 UidDe(NkEditeurModele &m, const char *nom, const NkVector<uint64> *exclus = nullptr) {
				uint64 trouve = 0u;
				NkVector<ecs::NkEntityId> ids;
				m.scene.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (std::strncmp(e.nom, nom, std::strlen(nom)) == 0)
						ids.PushBack(id);
				});
				for (usize i = 0; i < ids.Size(); ++i) {
					const uint64 u = m.scene.AssurerUid(ids[i]);
					bool exclu = false;
					for (usize k = 0; exclus && k < exclus->Size(); ++k)
						exclu = exclu || (*exclus)[k] == u;
					if (!exclu)
						trouve = u;
				}
				return trouve;
			}
			/// Les identites de TOUTES les entites (posees maintenant si besoin) :
			/// la photo d'avant un geste, pour reconnaitre ce qu'il a cree.
			NkVector<uint64> Uids(NkEditeurModele &m) {
				NkVector<ecs::NkEntityId> ids;
				m.scene.Monde().Query<NkTransform2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &) { ids.PushBack(id); });
				NkVector<uint64> u;
				for (usize i = 0; i < ids.Size(); ++i)
					u.PushBack(m.scene.AssurerUid(ids[i]));
				return u;
			}

			/// LE TEMOIN DE CREATION, ecrit une fois et joue deux fois (ia5, ce1).
			struct Creation {
					bool entiteEnPlus = false, auCentre = false, effet = false, resultatParti = false;
					uint64 uid = 0u;
					uint32 avant = 0u; ///< les entites AVANT le geste
					bool Ok() const {
						return entiteEnPlus && auCentre && effet && resultatParti;
					}
			};
			Creation CreerCaisse(NkHarnaisIA &h, const char *nomOutil) {
				Creation c;
				const uint32 avant = h.NbEntites();
				c.avant = avant;
				const NkVector<uint64> photo = Uids(h.M());
				const NkVec2f centre = h.M().scene.Camera().Centre();
				const uint32 requetes = h.serveur.Requetes();
				h.serveur.PousserOutil(nomOutil, "{\"acteur\":\"caisse\"}");
				const char *fin[] = {"Une caisse est au centre de la vue."};
				h.serveur.PousserTexte(fin, 1);
				h.Dire("ajoute une caisse au centre");
				h.Attendre();
				c.entiteEnPlus = h.NbEntites() == avant + 1u;
				c.uid = UidDe(h.M(), "Caisse", &photo);
				const ecs::NkEntityId e = ParUid(h.M(), c.uid);
				if (e.IsValid()) {
					const NkTransform2D *t = h.M().scene.Monde().Get<NkTransform2D>(e);
					c.auCentre = t != nullptr && std::fabs(t->position.x - centre.x) < 0.05f && std::fabs(t->position.y - centre.y) < 0.05f;
				}
				c.effet = Contient(h.IA().dernierEffet, "Caisse");
				// Le resultat de l'outil (son uid) est REPARTI au modele, dans un
				// message « tool » de la requete suivante.
				c.resultatParti = h.serveur.Requetes() >= requetes + 2u &&
								  Contient(h.serveur.Requete(requetes + 1u), "\"role\":\"tool\"") &&
								  Contient(h.serveur.Requete(requetes + 1u), "uid");
				return c;
			}

			/// LE TEMOIN DE CTRL+Z (ia6, ce2) : il defait EXACTEMENT le geste de
			/// l'IA -- la caisse part, et la scene revient a son compte d'avant,
			/// pas a celui d'un geste plus ancien (une annulation qui remonterait
			/// trop loin retirerait aussi ce qu'on a fait avant).
			bool AnnulationExacte(NkHarnaisIA &h, const Creation &c) {
				h.ToucheEditeur(nkgui::NkGuiKey::Z, true);
				return !ParUid(h.M(), c.uid).IsValid() && h.NbEntites() == c.avant;
			}

			/// LE TEMOIN DE LA CONFIRMATION (ia7, ce3) : le fichier ATTEND.
			bool SuppressionAttend(NkHarnaisIA &h, const NkString &chemin, uint32 &bloc) {
				h.serveur.PousserOutil("supprimer_fichier", "{\"chemin\":\"Contenu/a_supprimer.txt\"}");
				h.Dire("supprime Contenu/a_supprimer.txt");
				h.Attendre();
				for (int32 k = 0; k < 20; ++k)
					h.T().Trame(); // le temps passe : rien ne doit partir sans clic
				bloc = h.IA().blocConfirmation;
				return h.IA().phase == NkPhaseIA::NK_CONFIRMATION && NkFile::Exists(chemin.CStr()) && bloc != 0u;
			}
		} // namespace

		int32 NkEditeurLancerBancIA() {
			gR = gE = 0;
			std::printf("\n=== BANC IA (R18) : fournisseurs, panneau, outils, garde-fous ===\n");
			NkHarnaisIA h;
			if (!h.Ouvrir("unkeny_banc_ia", 1500.f, 860.f)) {
				Temoin(false, "(ia0) le harnais s'ouvre (faux serveur, editeur hors ecran)");
				h.Fermer();
				return 1;
			}
			NkEditeurIA &ia = h.IA();
			const converse::NkReglagesFournisseur ollama = ia.fournisseurs[0];

			// ── (ia1) la liste des modeles vient du serveur ──
			{
				NkVector<converse::NkConverseModeleInfo> l;
				NkString motif;
				const converse::NkDiagIA d = converse::NkIaListerModeles(ollama, NkString(), l, motif);
				bool qwenOutils = false, llamaSans = false;
				for (usize i = 0; i < l.Size(); ++i) {
					qwenOutils = qwenOutils || (l[i].nom == "qwen2.5:7b-instruct" && l[i].outils);
					llamaSans = llamaSans || (l[i].nom == "llama3.2:1b" && !l[i].outils);
				}
				Temoin(d == converse::NkDiagIA::NK_OK && l.Size() == 2u && qwenOutils && llamaSans,
					   "(ia1) Ollama : 2 modeles lus sur /api/tags, la capacite outils par /api/show", static_cast<float32>(l.Size()));
			}
			// ── (ia2) tester la connexion : trois verdicts, trois gestes ──
			{
				NkString msg;
				const converse::NkDiagIA ok = converse::NkIaTesterConnexion(ollama, NkString(), msg);
				Temoin(ok == converse::NkDiagIA::NK_OK && Contient(msg, "Connecté") && Contient(msg, "rien ne quitte"),
					   "(ia2a) Tester la connexion : « Connecte : 2 modele(s)... rien ne quitte ce PC »");
				converse::NkReglagesFournisseur absent = ollama;
				absent.modele = NkString("qwen3:8b");
				const converse::NkDiagIA d2 = converse::NkIaTesterConnexion(absent, NkString(), msg);
				Temoin(d2 == converse::NkDiagIA::NK_MODELE_ABSENT && Contient(msg, "ollama pull qwen3:8b"),
					   "(ia2b) modele absent : le geste « ollama pull qwen3:8b » en tete");
				converse::NkReglagesFournisseur mort = ollama;
				mort.adresse = NkString("http://127.0.0.1:9");
				NkChrono t0;
				const converse::NkDiagIA d3 = converse::NkIaTesterConnexion(mort, NkString(), msg);
				Temoin(d3 == converse::NkDiagIA::NK_SERVEUR_ABSENT && Contient(msg, "Lancez Ollama"),
					   "(ia2c) serveur absent : « Lancez Ollama » (refus de Windows : ~2 s)", static_cast<float32>(t0.Elapsed().ToSeconds()));
			}
			// ── (ia3) OpenAI et Anthropic : les cles ──
			{
				const converse::NkReglagesFournisseur openai = ia.fournisseurs[1];
				NkVector<converse::NkConverseModeleInfo> l;
				NkString motif;
				const converse::NkDiagIA d = converse::NkIaListerModeles(openai, NkString(NkHarnaisIA::kCle), l, motif);
				Temoin(d == converse::NkDiagIA::NK_OK && l.Size() == 2u, "(ia3a) OpenAI : /v1/models avec la cle (Bearer)", static_cast<float32>(l.Size()));
				const converse::NkDiagIA d2 = converse::NkIaListerModeles(openai, NkString("sk-fausse"), l, motif);
				Temoin(d2 == converse::NkDiagIA::NK_CLE_REFUSEE && Contient(motif, "Vérifiez la clé"),
					   "(ia3b) une mauvaise cle : CLE REFUSEE (401), le geste en tete");
				converse::NkReglagesFournisseur claude = ia.fournisseurs[2];
				const uint32 appels = h.serveur.Appels();
				const converse::NkDiagIA d3 = converse::NkIaListerModeles(claude, NkString(), l, motif);
				Temoin(d3 == converse::NkDiagIA::NK_CLE_ABSENTE && h.serveur.Appels() == appels,
					   "(ia3c) Anthropic sans cle : CLE ABSENTE, et RIEN n'est envoye");
				// Anthropic en flux, un outil aux arguments fragmentes (input_json_delta).
				h.serveur.PousserOutil("creer_acteur", "{\"acteur\":\"caisse\",\"x\":1.5}", "Je pose une caisse.");
				claude.modele = NkString("qwen2.5:7b-instruct");
				converse::NkRequeteIA q;
				converse::NkMessageIA u;
				u.texte = NkString("ajoute une caisse");
				q.messages.PushBack(u);
				NkEditeurIAOutilsConverse(q.outils);
				converse::NkReponseIA rep;
				NkString vu;
				const nkentseu::NkFunction<void(const NkString &)> sur = [&](const NkString &m) { vu.Append(m); };
				converse::NkIaDiscuter(claude, NkString(NkHarnaisIA::kCle), q, rep, sur);
				converse::NkJsonDoc args;
				const bool argsOk = rep.appels.Size() == 1u && args.Lire(rep.appels[0].arguments) &&
									args.TexteDe(args.Racine(), "acteur") == "caisse";
				Temoin(rep.ok && argsOk && vu == "Je pose une caisse." && Contient(rep.corpsEnvoye, "\"input_schema\""),
					   "(ia3d) Anthropic en flux : texte + tool_use recolle (input_json_delta)");
			}

			// Quelques trames : le panneau se peint, la liste du fournisseur actif est lue.
			for (int32 k = 0; k < 40 && (ia.sonde.EnCours() || ia.modeles[0].Empty()); ++k) {
				h.T().Trame();
				NkChrono::Sleep(static_cast<int64>(5));
			}

			// ── (ia4) une conversation EN FLUX ──
			{
				const char *morceaux[] = {"Bonjour ! ", "Je vois la scene ", "et ses ", "entites."};
				h.serveur.PousserTexte(morceaux, 4);
				const uint32 r0 = h.serveur.Requetes();
				NkVector<uint32> longueurs;
				const uint32 imagesAvant = ia.imagesFlux;
				h.Dire("Bonjour");
				h.Attendre(15.0, &longueurs);
				const uint32 bloc = h.DernierBloc(editorkit::NkAiBloc::Prose);
				const NkString corps = h.serveur.Requetes() > r0 ? h.serveur.Requete(r0) : NkString();
				Temoin(h.TexteBloc(bloc) == "Bonjour ! Je vois la scene et ses entites.", "(ia4a) le texte final, entier, dans le fil");
				Temoin(longueurs.Size() >= 2u && ia.imagesFlux - imagesAvant >= 2u,
					   "(ia4b) le texte a GRANDI a l'ecran en plusieurs images (flux)", static_cast<float32>(longueurs.Size()));
				Temoin(Contient(corps, "UnkenyEditor") && Contient(corps, "\"tools\"") && Contient(corps, "creer_acteur") &&
						   Contient(corps, "\"stream\":true"),
					   "(ia4c) la requete partie : systeme ENGENDRE (UnkenyEditor), outils natifs, flux");
			}

			// ── (ia5) « ajoute une caisse au centre » ──
			const Creation cr = CreerCaisse(h, "creer_acteur");
			Temoin(cr.entiteEnPlus, "(ia5a) une entite de plus dans la scene");
			Temoin(cr.auCentre, "(ia5b) la Caisse est AU CENTRE de la vue");
			Temoin(cr.effet && h.DernierBloc(editorkit::NkAiBloc::Effet) != 0u, "(ia5c) un bloc d'effet : « +1 entite « Caisse »... Ctrl+Z »");
			Temoin(cr.resultatParti, "(ia5d) le resultat de l'outil (uid) repart au modele (message tool)");

			// ── (ia6) Ctrl+Z la retire ──
			Temoin(AnnulationExacte(h, cr), "(ia6) Ctrl+Z (clavier de l'editeur) retire la caisse, et seulement elle",
				   static_cast<float32>(h.NbEntites()));

			// ── (ia7) une suppression ATTEND la confirmation ──
			{
				const NkString chemin = h.projet + "Contenu/a_supprimer.txt";
				NkFile::WriteAllText(chemin.CStr(), "a garder ?");
				uint32 bloc = 0u;
				const bool attend = SuppressionAttend(h, chemin, bloc);
				Temoin(attend, "(ia7a) supprimer_fichier : le fichier RESTE tant que rien n'est clique");
				const bool refuse = h.CliquerAction(bloc, 1u);
				h.Attendre();
				const NkString derniere = h.serveur.DerniereRequete();
				Temoin(refuse && NkFile::Exists(chemin.CStr()) && Contient(derniere, "a REFUS"),
					   "(ia7b) « Refuser » : le fichier reste, le modele apprend le refus");
				uint32 bloc2 = 0u;
				const bool attend2 = SuppressionAttend(h, chemin, bloc2);
				const bool confirme = attend2 && h.CliquerAction(bloc2, 0u);
				h.Attendre();
				Temoin(confirme && !NkFile::Exists(chemin.CStr()), "(ia7c) « Confirmer » : le fichier part");
				h.ToucheEditeur(nkgui::NkGuiKey::Z, true);
				Temoin(NkFile::Exists(chemin.CStr()) && NkFile::ReadAllText(chemin.CStr()) == "a garder ?",
					   "(ia7d) Ctrl+Z rend le fichier, octet pour octet (journal des fichiers)");
			}

			// ── (ia8) un modele SANS outils natifs : des blocs <outil> ──
			{
				NkEditeurIAChoisir(ia, 0, "llama3.2:1b");
				const uint32 r0 = h.serveur.Requetes();
				const NkVector<uint64> photo = Uids(h.M());
				const char *t[] = {"Je pose une balle. ", "<outil nom=\"creer_acteur\">{\"acteur\":\"balle\",", "\"x\":2,\"y\":3}</outil>"};
				h.serveur.PousserTexte(t, 3);
				const char *fin[] = {"La balle est en (2, 3)."};
				h.serveur.PousserTexte(fin, 1);
				h.Dire("pose une balle en 2, 3");
				h.Attendre();
				const ecs::NkEntityId b = ParUid(h.M(), UidDe(h.M(), "Balle", &photo));
				const NkTransform2D *tr = b.IsValid() ? h.M().scene.Monde().Get<NkTransform2D>(b) : nullptr;
				const NkString corps = h.serveur.Requetes() > r0 ? h.serveur.Requete(r0) : NkString();
				const uint32 prose = h.DernierBloc(editorkit::NkAiBloc::Prose);
				Temoin(tr && std::fabs(tr->position.x - 2.f) < 0.05f && std::fabs(tr->position.y - 3.f) < 0.05f,
					   "(ia8a) format texte : le bloc <outil> pose la Balle en (2, 3)");
				Temoin(!Contient(corps, "\"tools\"") && Contient(corps, "Comment agir") && Contient(corps, "<outil nom="),
					   "(ia8b) aucun champ tools parti ; la consigne <outil> est dans le systeme");
				Temoin(!Contient(h.TexteBloc(prose), "<outil"), "(ia8c) le texte montre ne contient pas le bloc <outil>");
			}
			// ── (ia9) des outils natifs REFUSES (400) : retour au texte, seul ──
			{
				ia.fournisseurs[0].outils = converse::NkModeOutils::NK_NATIFS;
				ia.forceTexte[0] = 0u;
				const char *t[] = {"<outil nom=\"creer_entite\">{\"nom\":\"Repere\",\"x\":-1,\"y\":0}</outil>"};
				h.serveur.PousserTexte(t, 1);
				const char *fin[] = {"Repere cree."};
				h.serveur.PousserTexte(fin, 1);
				h.Dire("cree une entite Repere en -1, 0");
				h.Attendre();
				Temoin(ia.forceTexte[0] == 1u && UidDe(h.M(), "Repere") != 0u,
					   "(ia9) 400 « does not support tools » : l'editeur repasse en texte et agit");
				ia.fournisseurs[0].outils = converse::NkModeOutils::NK_AUTO;
				ia.forceTexte[0] = 0u;
				NkEditeurIAChoisir(ia, 0, "qwen2.5:7b-instruct");
			}
			// ── (ia10) OpenAI en flux : le GDD ──
			{
				NkEditeurIAChoisir(ia, 1, "qwen2.5:7b-instruct");
				h.serveur.PousserOutil("ecrire_gdd", "{\"section\":\"Pitch\",\"texte\":\"Une gelee survit au dernier feu.\"}");
				const char *fin[] = {"Le pitch est dans le GDD."};
				h.serveur.PousserTexte(fin, 1);
				h.Dire("ecris le pitch dans le GDD");
				h.Attendre();
				const NkString gdd = NkFile::ReadAllText((h.projet + "Documents/GDD.md").CStr());
				Temoin(Contient(gdd, "## Pitch") && Contient(gdd, "Une gelee survit au dernier feu."),
					   "(ia10) OpenAI : ecrire_gdd (arguments en 2 fragments SSE) ecrit Documents/GDD.md");
				NkEditeurIAChoisir(ia, 0, "qwen2.5:7b-instruct");
			}
			// ── (ia11) hors du projet : refuse ──
			{
				NkEditeurCadre c = h.T().Cadre();
				converse::NkAppelOutil ap;
				ap.nom = NkString("ecrire_fichier");
				ap.arguments = NkString("{\"chemin\":\"../evade.txt\",\"texte\":\"x\"}");
				const NkResultatOutilIA r = NkEditeurIAExecuter(c, ap);
				Temoin(!r.ok && Contient(r.texte, "sort du projet") && !NkFile::Exists((h.racine + "evade.txt").CStr()),
					   "(ia11) « ../evade.txt » : REFUS nomme, rien n'est ecrit hors du projet");
			}
			// ── (ia12) la description est ENGENDREE ──
			{
				NkEditeurCadre c = h.T().Cadre();
				converse::NkAppelOutil ap;
				ap.nom = NkString("creer_entite");
				ap.arguments = NkString("{\"nom\":\"TemoinUnique\"}");
				(void)NkEditeurIAExecuter(c, ap);
				const NkString d = NkEditeurIADescription(h.M(), false);
				Temoin(Contient(d, "TemoinUnique") && Contient(d, "bp.ev.debut") && Contient(d, "caisse") && Contient(d, "Ctrl+Z") &&
						   Contient(d, "Documents/GDD.md"),
					   "(ia12) la description nomme l'entite creee a l'instant, les noeuds, les acteurs", static_cast<float32>(d.Length()));
			}

			// ── (ia13) ou vit le panneau : onglet (defaut), a part, replie ; RETENU ──
			{
				NkEditeurInterface &ui = h.T().Ui();
				auto Dans = [](const nkgui::NkRect &a, const editorkit::NkPaintRect &b) {
					return b.w > 0.f && b.x >= a.x - 0.5f && b.y >= a.y - 0.5f && b.x + b.w <= a.x + a.w + 0.5f && b.y + b.h <= a.y + a.h + 0.5f;
				};
				h.T().Trame();
				h.T().Trame();
				Temoin(ui.iaPlace == 0 && ui.ia.w == 0.f && Dans(ui.details, ia.pan.rect),
					   "(ia13a) par defaut : un ONGLET « IA » du groupe Details | Monde");
				const nkgui::NkRect bp = ui.iaBoutonPlace;
				h.T().Clic(0, bp.x + bp.w * 0.5f, bp.y + bp.h * 0.5f);
				h.T().Trame();
				const float32 largeurVue = ui.viseur.w;
				Temoin(ui.iaPlace == 1 && ui.ia.w >= 300.f && Dans(ui.ia, ia.pan.rect) && ui.ongletDroite != NK_ONGLET_IA,
					   "(ia13b) « Detacher » : un panneau a part, a droite de tout", ui.ia.w);
				const nkgui::NkRect br = ui.iaBoutonRepli;
				h.T().Clic(0, br.x + br.w * 0.5f, br.y + br.h * 0.5f);
				h.T().Trame();
				const bool replie = ui.iaReplie && ui.ia.w == 28.f && ui.viseur.w > largeurVue + 200.f;
				const nkgui::NkRect bande = ui.ia;
				h.T().Clic(0, bande.x + bande.w * 0.5f, bande.y + 200.f);
				h.T().Trame();
				Temoin(replie && !ui.iaReplie && ui.ia.w >= 300.f, "(ia13c) le chevron le REPLIE en bande (la vue s'elargit) ; un clic le deplie",
					   bande.w);
				// RETENU : ecrit sur le disque, relu dans une interface neuve.
				h.T().Clic(0, ui.iaBoutonRepli.x + 10.f, ui.iaBoutonRepli.y + 10.f); // replie
				h.T().Trame();
				ia.persister = true;
				NkEditeurIARetenirDisposition(ia, ui);
				ia.persister = false;
				memory::NkAllocator &al = memory::NkGetDefaultAllocator();
				NkEditeurInterface *ui2 = al.New<NkEditeurInterface>();
				const bool neuveOnglet = ui2->iaPlace == 0;
				NkEditeurIALireDisposition(ia, *ui2);
				Temoin(neuveOnglet && ui2->iaPlace == 1 && ui2->iaReplie && ui2->voirIA,
					   "(ia13d) la disposition est RETENUE (unkeny_panneau.txt) et relue");
				al.Delete(ui2);
				// Retour au defaut : deplier, puis rattacher en onglet.
				NkEditeurCadre c = h.T().Cadre();
				NkEditeurExecuter(c, NK_A_IA_REPLIER);
				NkEditeurExecuter(c, NK_A_IA_PLACE);
				h.T().Trame();
				Temoin(ui.iaPlace == 0 && ui.ongletDroite == NK_ONGLET_IA && Dans(ui.details, ia.pan.rect),
					   "(ia13e) « Rattacher » : de nouveau l'onglet, au premier plan");
			}

			// ════════════ LES CONTRE-EPREUVES ════════════
			{
				const Creation faux = CreerCaisse(h, "creer_acteurz");
				Temoin(!faux.Ok(), "(ce1) outil au nom faux -> le temoin de creation voit ECHEC");
			}
			{
				NkEditeurIAMutation(1u);
				const Creation sans = CreerCaisse(h, "creer_acteur");
				const bool exacte = AnnulationExacte(h, sans);
				NkEditeurIAMutation(0u);
				Temoin(sans.entiteEnPlus && !exacte, "(ce2) sans photo avant l'outil -> le temoin de Ctrl+Z voit ECHEC",
					   static_cast<float32>(h.NbEntites()));
			}
			{
				const NkString chemin = h.projet + "Contenu/a_supprimer.txt";
				NkFile::WriteAllText(chemin.CStr(), "a garder ?");
				NkEditeurIAMutation(2u);
				uint32 bloc = 0u;
				const bool attend = SuppressionAttend(h, chemin, bloc);
				NkEditeurIAMutation(0u);
				h.Attendre();
				Temoin(!attend && !NkFile::Exists(chemin.CStr()), "(ce3) sans confirmation -> le temoin « le fichier attend » voit ECHEC");
			}
			{
				const char *un[] = {"Une seule phrase, d'un bloc."};
				h.serveur.PousserTexte(un, 1);
				NkVector<uint32> longueurs;
				const uint32 imagesAvant = ia.imagesFlux;
				h.Dire("dis une phrase");
				h.Attendre(15.0, &longueurs);
				Temoin(!(longueurs.Size() >= 2u && ia.imagesFlux - imagesAvant >= 2u),
					   "(ce4) une reponse en UN morceau -> le temoin du flux voit ECHEC", static_cast<float32>(longueurs.Size()));
			}

			h.Fermer();
			std::printf("\nBANC IA %s : %d reussis, %d echec(s)\n", gE == 0 ? "REUSSI" : "ECHOUE", gR, gE);
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
