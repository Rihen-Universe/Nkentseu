#pragma once
// -----------------------------------------------------------------------------
// @File    RecettePlacement.h
// @Brief   `--recette-placement` : le PLACEMENT PAR WIDGET, les familles de
//          conteneur, les `Window` imbriques et la section `geometry`.
//          Sans fenetre, sans GPU, sans aucune entree injectee.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE RODOLF A TRANCHE, ET CE QUE CE BANC MESURE
// =============================================================================
//  « on dois pouvoir avoir du placement absolut comme non absolut ca va
//    dependre d elutilisateur et pour certain widget utiliser. »
//  puis, le meme jour :
//  « au vu de son parent, un conteneur ne pourra jamais porter les deux. »
//
//  LA REGLE QUI EN SORT, en une phrase : **le placement est une propriete du
//  CONTENEUR.** Un conteneur ecrit `placement = absolute`, et alors SES
//  enfants directs portent `pos`. Sans cette declaration il est en FLUX --
//  le defaut, inchange -- et un enfant qui porterait des coordonnees est
//  REFUSE par le validateur, pas ignore en silence.
//
//  Les enfants d'un conteneur sont donc TOUS en flux ou TOUS poses. La
//  question « un pose laisse-t-il un trou dans le flux de ses freres ? »
//  n'existe plus : le cas ne peut pas se produire. Les deux modes ne se
//  rencontrent QUE par IMBRICATION -- et c'est donc la, et seulement la,
//  qu'il faut mesurer (critere (g1h), trois niveaux).
//
// =============================================================================
//  CE BANC A ETE EXECUTE **AVANT** LE CORRECTIF, ET C'EST SON INTERET
// =============================================================================
//  Les criteres (g0b) a (g4c) ont d'abord ete lances sur le code d'avant. Leur
//  ROUGE est le ZERO -- la preuve que le compteur ne vaut pas N depuis toujours.
//  Le releve de ce premier passage vit dans `scratchpad/agent-ecrivain/`.
//
//  ⚠️ (g0b) EXISTE PARCE QUE LE VERT PRECEDENT ETAIT CIRCULAIRE. Le critere
//     (e3b) de `RecetteEcrivain` annoncait « 22 widgets au MEME endroit » : le
//     monteur NOTAIT le rectangle d'un `Window` en recopiant le `pos` qu'on
//     venait d'ecrire (`RegionCourante`), sans jamais deplacer le curseur. Le
//     releve comparait donc un nombre a lui-meme. (g0b) regarde l'ENFANT du
//     conteneur pose -- le seul temoin qu'une recopie ne peut pas fabriquer.
// -----------------------------------------------------------------------------

#include <cstdio>

#include "RecetteEcrivain.h" // MonterEtPeindre, EcrirePng : deja ecrits, on ne les recopie pas

namespace nkuidesign {

	namespace placement {

		using namespace nkentseu;
		using nkentseu::nkgui::NkGuiFont;
		using nkentseu::nkgui::NkGuiMonteItem;
		using nkentseu::nkgui::NkGuiMonteRapport;
		using nkentseu::nkgui::NkRect;

		static int g_ok = 0, g_ko = 0;

		inline void Check(bool ok, const char *nom, const char *detail) {
			(ok ? g_ok : g_ko)++;
			printf("  [ %s ] %s\n", ok ? "OK" : "KO", nom);
			if (detail && *detail) {
				printf("         %s\n", detail);
			}
		}

		/// Une empreinte du tampon RGBA. FNV-1a 32 bits -- elle sert a comparer
		/// DEUX CONSTRUCTIONS, pas a juger une image : c'est pourquoi elle est
		/// imprimee en plus d'etre comparee.
		inline uint32 Empreinte(const uint8 *p, usize n) {
			uint32 h = 2166136261u;
			for (usize i = 0; i < n; ++i) {
				h ^= (uint32)p[i];
				h *= 16777619u;
			}
			return h;
		}

		/// Le releve d'un item par son identifiant, ou nullptr. Chercher par ID
		/// et non par rang : un lot qui ajoute un widget decalerait tous les rangs.
		inline const NkGuiMonteItem *Trouver(const NkGuiMonteRapport &r, const char *id) {
			for (uint32 i = 0; i < (uint32)r.items.Size(); ++i) {
				if (r.items[i].id.Compare(NkString(id)) == 0) {
					return &r.items[i];
				}
			}
			return nullptr;
		}

		/// Le pixel (x, y) d'un tampon RGBA, empaquete 0xAABBGGRR tel que le
		/// rasteriseur l'ecrit. Rend 0 hors du cadre -- et l'appelant doit donc
		/// verifier ses bornes lui-meme plutot que de lire un zero pour du noir.
		inline uint32 Pixel(const NkVector<uint8> &px, int32 W, int32 H, int32 x, int32 y) {
			if (x < 0 || y < 0 || x >= W || y >= H) {
				return 0u;
			}
			const usize i = ((usize)y * (usize)W + (usize)x) * 4u;
			if (i + 3u >= (usize)px.Size()) {
				return 0u;
			}
			return (uint32)px[(uint32)i] | ((uint32)px[(uint32)i + 1u] << 8)
				   | ((uint32)px[(uint32)i + 2u] << 16) | ((uint32)px[(uint32)i + 3u] << 24);
		}

		/// Combien de pixels NON BLANCS dans un rectangle. Le banc efface a blanc
		/// (`ras.Effacer(0xFFFFFFFF)`), donc « non blanc » = « quelque chose a ete
		/// peint ici ».
		inline uint32 PeintsDans(const NkVector<uint8> &px, int32 W, int32 H, int32 x0, int32 y0,
								 int32 x1, int32 y1) {
			uint32 n = 0;
			for (int32 y = y0; y < y1; ++y) {
				for (int32 x = x0; x < x1; ++x) {
					if (x < 0 || y < 0 || x >= W || y >= H) {
						continue;
					}
					if (Pixel(px, W, H, x, y) != 0xFFFFFFFFu) {
						++n;
					}
				}
			}
			return n;
		}

		/// Monte un texte `.nkgui` -- meme porte que la recette ecrivain.
		inline bool Monter(const char *src, int32 W, int32 H, NkGuiFont *police,
						   NkGuiMonteRapport &rap, NkVector<uint8> &px) {
			NkGuiDiag err;
			uint32 tex = 0;
			uint32 len = 0;
			while (src[len]) {
				++len;
			}
			return ecrivain::MonterEtPeindre(src, len, W, H, police, rap, px, err, tex);
		}

		/// Lit un fichier du corpus, en essayant les deux racines (l'executable ne
		/// vit pas a la racine du depot).
		inline NkVector<uint8> LireCorpus(const char *relatif) {
			char a[512], b[512];
			snprintf(a, sizeof(a), "Applications/NKUIDesign/exemples/%s", relatif);
			snprintf(b, sizeof(b), "../../../../Applications/NKUIDesign/exemples/%s", relatif);
			NkVector<uint8> o = NkFile::ReadAllBytes(a);
			if (o.Empty()) {
				o = NkFile::ReadAllBytes(b);
			}
			return o;
		}

		/// Les 21 fichiers du depot, nommes -- pas un balayage de repertoire : un
		/// balayage rendrait le compteur muet le jour ou le chemin change.
		inline const char *const *Corpus21(uint32 &n) {
			static const char *k[] = {
				"valides/01_panneau_reglages.nkgui",
				"valides/02_bloc_sur_une_ligne.nkgui",
				"valides/03_virgule_vecteur_couleur.nkgui",
				"valides/04_echappements_utf8.nkgui",
				"valides/05_animation_comportement.nkgui",
				"valides/06_version_0_2_alias.nkgui",
				"valides/07_apparence.nkgui",
				"valides/08_indentation_mixte.nkgui",
				"valides/09_indentation_a_la_main.nkgui",
				"valides/10_etats_apparence.nkgui",
				"fautifs/refuses_a_la_lecture/r1_echappement_inconnu.nkgui",
				"fautifs/refuses_a_la_lecture/r2_accolade_jamais_fermee.nkgui",
				"fautifs/refuses_a_la_lecture/r4_entete_nkgui_manquant.nkgui",
				"fautifs/refuses_a_la_lecture/r5_commentaire_jamais_ferme.nkgui",
				"fautifs/refuses_a_la_lecture/r7_valeur_manquante.nkgui",
				"fautifs/signales_par_la_validation/v1_couleur_cinq_chiffres.nkgui",
				"fautifs/signales_par_la_validation/v2_virgule_finale_dans_liste.nkgui",
				"fautifs/signales_par_la_validation/v3_cle_de_dictionnaire_invalide.nkgui",
				"fautifs/signales_par_la_validation/v4_section_inconnue.nkgui",
				"fautifs/signales_par_la_validation/v5_valeur_bien_formee_mauvais_type.nkgui",
				"fautifs/signales_par_la_validation/v6_role_inconnu.nkgui",
			};
			n = (uint32)(sizeof(k) / sizeof(k[0]));
			return k;
		}

		/// Les dix VALIDES, ceux dont l'image doit rester identique au pixel.
		inline const char *const *CorpusValides(uint32 &n) {
			uint32 t = 0;
			const char *const *k = Corpus21(t);
			n = 10u; // les dix premiers : voir la table ci-dessus
			return k;
		}

		// =====================================================================
		//  LES DOCUMENTS D'ESSAI -- ecrits ICI, lisibles d'un coup d'oeil
		// =====================================================================
		// ⚠️ LA REGLE QU'ILS EXERCENT, en une phrase : **un conteneur declare
		//    `placement = absolute`, et alors SES enfants directs portent `pos`.**
		//    Sans cette declaration il est en FLUX -- le defaut, inchange -- et un
		//    enfant qui porterait des coordonnees est REFUSE, pas ignore.
		//
		// ⚠️ LA RACINE DE `widgets` EST ABSOLUE PAR NATURE : rien ne la contient,
		//    donc aucun conteneur ne peut y declarer son mode. C'est pourquoi
		//    `DocPose` pose un bouton directement a la racine.
		
		inline const char *DocPose() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  Button \"pose\" { label = \"P\", pos = (500, 300), size = (120, 40) }\n"
				   "}\n";
		}
		
		/// LE NEGATIF DE (g1a) : le MEME bouton, sans `pos`.
		inline const char *DocFlux() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  Button \"pose\" { label = \"P\" }\n"
				   "}\n";
		}
		
		/// Des coordonnees SOUS UN CONTENEUR EN FLUX : le validateur doit le NOMMER.
		inline const char *DocPosSousFlux() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  VBox \"col\" {\n"
				   "    Button \"dedans\" { label = \"x\", pos = (50, 60) }\n"
				   "  }\n"
				   "}\n";
		}
		
		/// LE MEME fichier, avec le mot qui manquait sur le PARENT.
		inline const char *DocPosSousAbsolu() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  Panel \"toile\" {\n"
				   "    placement = absolute\n"
				   "    pos = (100, 100)\n"
				   "    size = (400, 300)\n"
				   "    Button \"dedans\" { label = \"x\", pos = (50, 60), size = (80, 30) }\n"
				   "  }\n"
				   "}\n";
		}
		
		/// `placement` SUR UNE BOITE : son nom dit deja son agencement, le format
		/// doit le refuser -- et la faute doit tomber par le mecanisme qui existe.
		inline const char *DocPlacementSurBoite() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  VBox \"col\" { placement = absolute }\n"
				   "  Panel \"p\" { placement = flottant }\n"
				   "}\n";
		}
		
		/// TROIS NIVEAUX, ET LES DEUX MODES SE RENCONTRENT A CHAQUE ETAGE :
		///   `n1` ABSOLU  >  `n2` (une `VBox`, donc EN FLUX, mais POSEE par n1)
		///                >  `n3` ABSOLU a nouveau, lui-meme dans le flux de n2.
		/// C'est le seul endroit ou les deux modes se touchent desormais -- donc
		/// c'est la qu'il faut mesurer.
		inline const char *DocModesImbriques() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  Panel \"n1\" {\n"
				   "    placement = absolute\n"
				   "    pos = (40, 40)\n"
				   "    size = (500, 400)\n"
				   "    VBox \"n2\" {\n"
				   "      pos = (20, 20)\n"
				   "      size = (300, 260)\n"
				   "      gap = 4\n"
				   "      Text \"t1\" { text = \"un\" }\n"
				   "      Text \"t2\" { text = \"deux\" }\n"
				   "      Panel \"n3\" {\n"
				   "        placement = absolute\n"
				   "        Button \"b3\" { label = \"trois\", pos = (10, 10), size = (90, 26) }\n"
				   "      }\n"
				   "    }\n"
				   "  }\n"
				   "}\n";
		}
		

		/// Un conteneur pose AVEC un enfant : le seul temoin qu'une recopie de
		/// `pos` ne peut pas fabriquer.
		/// LE FICHIER `03` DU CORPUS, ET LE MEME SANS SON `title`.
		///
		/// 🔴 C'EST LE SEUL FICHIER DU CORPUS DONT L'IMAGE CHANGE, et il faut
		///    nommer la cause plutot que de la laisser dans une empreinte.
		///    `Window "apercu"` porte `pos` : il est donc POSE, et un conteneur pose
		///    OUVRE SA REGION. Avant, il n'en ouvrait pas : la hauteur de sa barre
		///    de titre etait ajoutee au curseur DU PARENT, et poussait ses trois
		///    freres (`choix`, `logo`, `courbe`) vers le bas. Maintenant elle
		///    s'applique a l'interieur de la fenetre, ou elle a toujours eu sa
		///    place, et les freres ne bougent plus.
		///
		/// ⚠️ CE N'EST PAS UNE REGRESSION, C'EST UNE CONSEQUENCE -- et la
		///    difference se mesure : le titre d'une fenetre POSEE ne doit plus
		///    deplacer ce qui est DEHORS. Le temoin ci-dessous le prouve en
		///    comparant le fichier a lui-meme sans son `title`.
		inline const char *DocApercuAvecTitre() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  Window \"apercu\" { title = \"Apercu\" pos = (120, 80) size = (640, 480) }\n"
				   "  Dropdown \"choix\" { items = [\"un\", \"deux\"] }\n"
				   "}\n";
		}
		
		inline const char *DocApercuSansTitre() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  Window \"apercu\" { pos = (120, 80) size = (640, 480) }\n"
				   "  Dropdown \"choix\" { items = [\"un\", \"deux\"] }\n"
				   "}\n";
		}
		
		inline const char *DocConteneurPose() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  Window \"hote\" {\n"
				   "    title = \"Hote\"\n"
				   "    pos = (500, 300)\n"
				   "    size = (200, 150)\n"
				   "    Button \"dedans\" { label = \"ok\" }\n"
				   "  }\n"
				   "}\n";
		}

		inline const char *DocGeometrie() {
			return "nkgui 0.3\n"
				   "geometry {\n"
				   "  shape \"aplat\" {\n"
				   "    kind = rect\n"
				   "    pos = (600, 400)\n"
				   "    size = (120, 60)\n"
				   "    color = #F79A28\n"
				   "  }\n"
				   "}\n"
				   "widgets {\n"
				   "  Text \"t\" { text = \"au dessus\" }\n"
				   "}\n";
		}

		inline const char *DocSansGeometrie() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  Text \"t\" { text = \"au dessus\" }\n"
				   "}\n";
		}

		inline const char *DocModal() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  Window \"dlg\" {\n"
				   "    title = \"Confirmer\"\n"
				   "    pos = (300, 200)\n"
				   "    size = (240, 120)\n"
				   "    modal = true\n"
				   "    Button \"ok\" { label = \"Enregistrer\" }\n"
				   "  }\n"
				   "}\n";
		}

		inline const char *DocNonModal() {
			return "nkgui 0.3\n"
				   "widgets {\n"
				   "  Window \"dlg\" {\n"
				   "    title = \"Confirmer\"\n"
				   "    pos = (300, 200)\n"
				   "    size = (240, 120)\n"
				   "    modal = false\n"
				   "    Button \"ok\" { label = \"Enregistrer\" }\n"
				   "  }\n"
				   "}\n";
		}

		// =====================================================================
		//  LA RECETTE
		// =====================================================================
		inline int RecettePlacement() {
			g_ok = 0;
			g_ko = 0;
			printf("\n=== RECETTE PLACEMENT -- poser un widget, et garder le flux intact ===\n");
			printf("    aucune fenetre ouverte, aucun GPU, aucune entree injectee.\n");

			NkGuiFont police;
			const bool policeOk =
				police.LoadEmbedded(nkentseu::NkEmbeddedFontId::DroidSans, 15.f, false);
			{
				char d[200];
				snprintf(d, sizeof(d), "police embarquee=%d, atlas %dx%d", policeOk ? 1 : 0,
						 police.atlasW, police.atlasH);
				Check(policeOk && police.Valid(),
					  "(g00) la police est chargee AVANT toute conclusion sur du texte", d);
			}
			const int32 W = 800, H = 600;

			// -----------------------------------------------------------------
			// (g0a) LE ZERO DE `geometry`, MESURE PAR LE LECTEUR
			// -----------------------------------------------------------------
			printf("\n-- (g0) les zeros, avant de mesurer quoi que ce soit\n");
			{
				uint32 n = 0;
				const char *const *k = Corpus21(n);
				uint32 lus = 0, sections = 0, formes = 0;
				for (uint32 i = 0; i < n; ++i) {
					const NkVector<uint8> o = LireCorpus(k[i]);
					if (o.Empty()) {
						continue;
					}
					NkArchive a;
					NkGuiDiag e;
					if (!NkGuiArchive::Read((const char *)o.Data(), (uint32)o.Size(), a, e)) {
						continue; // les cinq refuses a la lecture : c'est leur role
					}
					++lus;
					const NkArchiveNode *corps = guifmt::NkGCorps(a);
					if (!corps) {
						continue;
					}
					for (uint32 s = 0; s < (uint32)corps->array.Size(); ++s) {
						if (!corps->array[s].IsObject() || !corps->array[s].object) {
							continue;
						}
						const NkArchive &sec = *corps->array[s].object;
						const NkString nom(NkGuiArchive::TypeOf(sec));
						if (nom.Compare("geometry") != 0) {
							continue;
						}
						++sections;
						const NkArchiveNode *c2 = guifmt::NkGCorps(sec);
						if (c2) {
							formes += (uint32)c2->array.Size();
						}
					}
				}
				char d[300];
				snprintf(d, sizeof(d),
						 "%u fichier(s) nomme(s), %u lisible(s) ; %u section(s) `geometry`, "
						 "%u bloc(s) dedans -- attendu 0 et 0",
						 n, lus, sections, formes);
				Check(lus > 0u && sections == 0u && formes == 0u,
					  "(g0a) ZERO : `geometry` n'est atteste par AUCUN fichier du depot", d);
			}

			// -----------------------------------------------------------------
			// (g0b) LE VERT CIRCULAIRE, ET SON TEMOIN
			// -----------------------------------------------------------------
			{
				NkGuiMonteRapport r;
				NkVector<uint8> px;
				const bool ok = Monter(DocConteneurPose(), W, H, &police, r, px);
				const NkGuiMonteItem *hote = Trouver(r, "hote");
				const NkGuiMonteItem *dedans = Trouver(r, "dedans");
				bool contenu = false;
				if (hote && dedans) {
					const NkRect &a = hote->rect;
					const NkRect &b = dedans->rect;
					contenu = (b.x >= a.x - 0.5f) && (b.y >= a.y - 0.5f)
							  && (b.x + b.w <= a.x + a.w + 0.5f) && (b.y + b.h <= a.y + a.h + 0.5f);
				}
				char d[400];
				snprintf(d, sizeof(d),
						 "monte=%d ; hote (%.0f, %.0f) %.0f x %.0f ; enfant (%.0f, %.0f) %.0f x %.0f "
						 "-- l'enfant est-il DANS l'hote ? %d",
						 ok ? 1 : 0, hote ? (double)hote->rect.x : -1.0,
						 hote ? (double)hote->rect.y : -1.0, hote ? (double)hote->rect.w : -1.0,
						 hote ? (double)hote->rect.h : -1.0, dedans ? (double)dedans->rect.x : -1.0,
						 dedans ? (double)dedans->rect.y : -1.0, dedans ? (double)dedans->rect.w : -1.0,
						 dedans ? (double)dedans->rect.h : -1.0, contenu ? 1 : 0);
				// ⚠️ LE SEUL TEMOIN QU'UNE RECOPIE NE PEUT PAS FABRIQUER. Le rect de
				//    l'HOTE est le `pos` qu'on vient d'ecrire ; celui de l'ENFANT est
				//    calcule par le layout. Comparer les deux, c'est demander au
				//    monteur s'il a VRAIMENT ouvert une region.
				Check(ok && hote && dedans && contenu,
					  "(g0b) l'enfant d'un conteneur POSE se monte DANS ce conteneur", d);
			}

			// -----------------------------------------------------------------
			// (g1) LE PLACEMENT PAR WIDGET
			// -----------------------------------------------------------------
			printf("\n-- (g1) le placement est une propriete du CONTENEUR\n");
			NkGuiMonteRapport rA, rB;
			NkVector<uint8> pA, pB;
			const bool okA = Monter(DocPose(), W, H, &police, rA, pA);
			const bool okB = Monter(DocFlux(), W, H, &police, rB, pB);
			{
				const NkGuiMonteItem *p = Trouver(rA, "pose");
				const bool exact = p && (p->rect.x > 499.5f && p->rect.x < 500.5f)
							   && (p->rect.y > 299.5f && p->rect.y < 300.5f)
							   && (p->rect.w > 119.5f && p->rect.w < 120.5f)
							   && (p->rect.h > 39.5f && p->rect.h < 40.5f);
				const uint32 peints = PeintsDans(pA, W, H, 500, 300, 620, 340);
				char d[400];
				snprintf(d, sizeof(d),
						"monte=%d ; ecrit (500, 300) 120 x 40 ; monte (%.1f, %.1f) %.1f x %.1f ; "
						"%u pixel(s) peint(s) dans ce rectangle ; %u pose(s), %u honore(s), "
						"%u non consomme(s)",
						okA ? 1 : 0, p ? (double)p->rect.x : -1.0, p ? (double)p->rect.y : -1.0,
						p ? (double)p->rect.w : -1.0, p ? (double)p->rect.h : -1.0, peints, rA.poses,
						rA.posesHonores, rA.posesNonConsommes);
				Check(okA && exact && peints > 0u && rA.posesNonConsommes == 0u,
						"(g1a) un widget POSE se monte a SES coordonnees, au pixel", d);
			}
			{
				// LE NEGATIF : le MEME bouton sans `pos`. S'il tombait quand meme a
				// (500, 300), (g1a) serait vert sans rien mesurer.
				const NkGuiMonteItem *p = Trouver(rB, "pose");
				const bool ailleurs = p && (p->rect.x < 499.5f || p->rect.x > 500.5f
									 || p->rect.y < 299.5f || p->rect.y > 300.5f);
				const uint32 peints = PeintsDans(pB, W, H, 500, 300, 620, 340);
				char d[400];
				snprintf(d, sizeof(d),
						"monte=%d ; sans `pos` le bouton est en (%.1f, %.1f) ; %u pixel(s) peint(s) "
						"dans (500, 300) 120 x 40 -- attendu 0 ; %u pose(s) comptes",
						okB ? 1 : 0, p ? (double)p->rect.x : -1.0, p ? (double)p->rect.y : -1.0, peints,
						rB.poses);
				Check(okB && p && ailleurs && peints == 0u && rB.poses == 0u,
						"(g1b) NEGATIF : le MEME widget SANS `pos` ne va PAS a ces coordonnees", d);
			}
			{
				// ⚠️ LE REFUS EST LA CONTREPARTIE DE LA REGLE. Des coordonnees sous un
				//    conteneur en flux, si elles etaient IGNOREES, se traduiraient par
				//    « j'ecris, rien ne bouge, je ne sais pas pourquoi ».
				NkArchive a;
				NkGuiDiag e;
				NkVector<NkGuiDiag> dg;
				guifmt::NkGValidateResult vr;
				const NkString t(DocPosSousFlux());
				const bool lu = NkGuiArchive::Read(t.Data(), (uint32)t.Size(), a, e);
				if (lu) {
					vr = guifmt::NkGValidate(a, dg);
				}
				bool bonCode = false;
				for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
					if (dg[i].code.Compare("E-PLACEMENT") == 0) {
						bonCode = true;
					}
				}
				char d[400];
				snprintf(d, sizeof(d), "lu=%d ; %u erreur(s), code E-PLACEMENT=%d ; 1re : %s",
						lu ? 1 : 0, vr.errors, bonCode ? 1 : 0,
						dg.Empty() ? "(aucune)" : dg[0].message.Data());
				Check(lu && vr.errors > 0u && bonCode,
						"(g1c) des coordonnees sous un conteneur EN FLUX sont REFUSEES", d);
			}
			{
				// LE POSITIF : le MEME fichier, avec le mot qui manquait sur le PARENT.
				// L'attendu de la place est DERIVE -- l'origine du parent PLUS le `pos`
				// de l'enfant : (100 + 50, 100 + 60). Aucun nombre recopie d'un releve.
				NkArchive a;
				NkGuiDiag e;
				NkVector<NkGuiDiag> dg;
				guifmt::NkGValidateResult vr;
				const NkString t(DocPosSousAbsolu());
				const bool lu = NkGuiArchive::Read(t.Data(), (uint32)t.Size(), a, e);
				if (lu) {
					vr = guifmt::NkGValidate(a, dg);
				}
				NkGuiMonteRapport r;
				NkVector<uint8> px;
				const bool ok = Monter(DocPosSousAbsolu(), W, H, &police, r, px);
				const NkGuiMonteItem *b = Trouver(r, "dedans");
				const bool place = b && (b->rect.x > 149.5f && b->rect.x < 150.5f)
							   && (b->rect.y > 159.5f && b->rect.y < 160.5f);
				char d[400];
				snprintf(d, sizeof(d),
						"validation : %u erreur(s) ; monte=%d ; enfant a (%.1f, %.1f) -- attendu "
						"(100+50, 100+60) = (150, 160) ; %u pose(s), %u honore(s)",
						vr.errors, ok ? 1 : 0, b ? (double)b->rect.x : -1.0, b ? (double)b->rect.y : -1.0,
						r.poses, r.posesHonores);
				Check(lu && vr.errors == 0u && ok && place,
						"(g1d) sous `placement = absolute`, l'enfant se monte a parent + pos", d);
			}
			{
				// ⚠️ DEUX REFUS QUI TOMBENT PAR LE MECANISME QUI EXISTE DEJA : une `VBox`
				//    n'a pas `placement` a son schema (son NOM dit son agencement), et un
				//    mode hors de { flow, absolute } n'est pas un mode. Aucun garde special
				//    n'a ete ecrit pour le premier -- c'est `E-TYPE` qui le prend.
				NkArchive a;
				NkGuiDiag e;
				NkVector<NkGuiDiag> dg;
				guifmt::NkGValidateResult vr;
				const NkString t(DocPlacementSurBoite());
				const bool lu = NkGuiArchive::Read(t.Data(), (uint32)t.Size(), a, e);
				if (lu) {
					vr = guifmt::NkGValidate(a, dg);
				}
				bool surBoite = false, modeInconnu = false;
				for (uint32 i = 0; i < (uint32)dg.Size(); ++i) {
					if (dg[i].code.Compare("E-TYPE") == 0) {
						surBoite = true;
					}
					if (dg[i].code.Compare("E-PLACEMENT") == 0) {
						modeInconnu = true;
					}
				}
				char d[400];
				snprintf(d, sizeof(d),
						"lu=%d ; %u erreur(s) ; `placement` sur une VBox signale=%d, mode inconnu "
						"signale=%d ; 1re : %s",
						lu ? 1 : 0, vr.errors, surBoite ? 1 : 0, modeInconnu ? 1 : 0,
						dg.Empty() ? "(aucune)" : dg[0].message.Data());
				Check(lu && surBoite && modeInconnu,
						"(g1e) NEGATIF : `placement` sur une boite, et un mode inconnu, sont "
						"SIGNALES",
						d);
			}
			{
				// ── (g1h) LES DEUX MODES SE RENCONTRENT PAR IMBRICATION, ET LA SEULEMENT
				// ⚠️ ON MESURE LA CONTENANCE, PAS DES NOMBRES RECOPIES. Le rect de `n1`
				//    vient de son propre `pos` -- le comparer a lui-meme ne prouverait
				//    rien. Les rects de `t1`, `t2` et `b3`, eux, sont CALCULES par le
				//    layout : c'est leur place a EUX qui atteste que chaque region a bien
				//    ete ouverte, et que le flux a bien coule a l'etage du milieu.
				NkGuiMonteRapport r;
				NkVector<uint8> px;
				const bool ok = Monter(DocModesImbriques(), W, H, &police, r, px);
				const NkGuiMonteItem *n1 = Trouver(r, "n1");
				const NkGuiMonteItem *n2 = Trouver(r, "n2");
				const NkGuiMonteItem *n3 = Trouver(r, "n3");
				const NkGuiMonteItem *t1 = Trouver(r, "t1");
				const NkGuiMonteItem *t2 = Trouver(r, "t2");
				const NkGuiMonteItem *b3 = Trouver(r, "b3");
				const bool tous = n1 && n2 && n3 && t1 && t2 && b3;
				// n2 est POSE par n1 : (40 + 20, 40 + 20).
				const bool n2Place = tous && (n2->rect.x > 59.5f && n2->rect.x < 60.5f)
							   && (n2->rect.y > 59.5f && n2->rect.y < 60.5f);
				// n2 est une `VBox` : ses deux textes sont EN FLUX -- meme x, y croissant.
				const bool flux = tous && (t2->rect.y > t1->rect.y + 0.5f)
							   && (t2->rect.x > t1->rect.x - 0.5f)
							   && (t2->rect.x < t1->rect.x + 0.5f);
				// n3 est ABSOLU a nouveau : son bouton est a n3 + (10, 10).
				const bool b3Place = tous
							   && (b3->rect.x > n3->rect.x + 9.5f)
							   && (b3->rect.x < n3->rect.x + 10.5f)
							   && (b3->rect.y > n3->rect.y + 9.5f)
							   && (b3->rect.y < n3->rect.y + 10.5f);
				// Et chaque etage vit DANS le precedent.
				const bool emboite = tous && (n2->rect.x >= n1->rect.x - 0.5f)
							   && (n2->rect.y >= n1->rect.y - 0.5f)
							   && (n3->rect.x >= n2->rect.x - 0.5f)
							   && (n3->rect.y >= n2->rect.y - 0.5f);
				char d[600];
				snprintf(d, sizeof(d),
						"monte=%d ; n1 (%.0f, %.0f) > n2 (%.0f, %.0f) [attendu (60, 60)] > n3 "
						"(%.0f, %.0f) ; t1 y=%.0f, t2 y=%.0f (flux=%d) ; b3 (%.0f, %.0f) = n3 + "
						"(10, 10) -> %d ; emboite=%d",
						ok ? 1 : 0, tous ? (double)n1->rect.x : -1.0, tous ? (double)n1->rect.y : -1.0,
						tous ? (double)n2->rect.x : -1.0, tous ? (double)n2->rect.y : -1.0,
						tous ? (double)n3->rect.x : -1.0, tous ? (double)n3->rect.y : -1.0,
						tous ? (double)t1->rect.y : -1.0, tous ? (double)t2->rect.y : -1.0, flux ? 1 : 0,
						tous ? (double)b3->rect.x : -1.0, tous ? (double)b3->rect.y : -1.0,
						b3Place ? 1 : 0, emboite ? 1 : 0);
				Check(ok && tous && n2Place && flux && b3Place && emboite,
						"(g1h) ABSOLU > FLUX > ABSOLU : trois niveaux, chacun a sa place", d);
			}
			// -----------------------------------------------------------------
			// (g1e) LA NON-REGRESSION DU CORPUS, PAR EMPREINTE
			// -----------------------------------------------------------------
			printf("\n-- (g1e) les dix valides du corpus : leur image doit etre INCHANGEE\n");
			{
				uint32 n = 0;
				const char *const *k = CorpusValides(n);
				uint32 montes = 0;
				for (uint32 i = 0; i < n; ++i) {
					const NkVector<uint8> o = LireCorpus(k[i]);
					if (o.Empty()) {
						printf("  [ .. ] %-44s FICHIER INTROUVABLE\n", k[i]);
						continue;
					}
					NkGuiMonteRapport r;
					NkVector<uint8> px;
					NkGuiDiag e;
					uint32 tex = 0;
					const bool ok = ecrivain::MonterEtPeindre((const char *)o.Data(),
															  (uint32)o.Size(), 600, 700, &police, r,
															  px, e, tex);
					if (!ok || px.Empty()) {
						printf("  [ .. ] %-44s NON MONTE\n", k[i]);
						continue;
					}
					++montes;
					printf("  [ .. ] %-44s empreinte=%08X  %u monte(s)\n", k[i],
						   Empreinte(px.Data(), (usize)px.Size()), r.montes);
				}
				char d[200];
				snprintf(d, sizeof(d), "%u / %u fichier(s) valides montes et empreintes", montes, n);
				// PUBLIE, PAS JUGE ICI : la comparaison se fait ENTRE DEUX
				// CONSTRUCTIONS, elle ne peut pas vivre dans une seule execution.
				// Un critere qui comparerait a une constante ecrite a la main serait
				// un attendu en dur, et il se perimerait a la premiere retouche.
				Check(montes == n, "(g1e) les dix valides se montent tous (empreintes publiees)", d);
			}
			{
				// ── (g1i) LA SEULE DIVERGENCE DU CORPUS, ET SA CAUSE ────────────
				NkGuiMonteRapport ra, rb;
				NkVector<uint8> pa, pb;
				const bool oka = Monter(DocApercuAvecTitre(), W, H, &police, ra, pa);
				const bool okb = Monter(DocApercuSansTitre(), W, H, &police, rb, pb);
				const NkGuiMonteItem *ca = Trouver(ra, "choix");
				const NkGuiMonteItem *cb = Trouver(rb, "choix");
				const bool memeY = ca && cb && (ca->rect.y > cb->rect.y - 0.5f)
							   && (ca->rect.y < cb->rect.y + 0.5f);
				char d[400];
				snprintf(d, sizeof(d),
						"le frere de la fenetre posee : y=%.1f AVEC un titre, y=%.1f SANS -- ils "
						"doivent etre EGAUX (le titre d'une fenetre posee est peint DEDANS)",
						ca ? (double)ca->rect.y : -1.0, cb ? (double)cb->rect.y : -1.0);
				Check(oka && okb && memeY,
						"(g1i) CAUSE NOMMEE : le titre d'une fenetre POSEE ne pousse plus ses "
						"freres",
						d);
			}

			// -----------------------------------------------------------------
			// (g2) LES FAMILLES : `modal` ET `flags` CESSENT D'ETRE ILLISIBLES
			// -----------------------------------------------------------------
			printf("\n-- (g2) le dialogue n'est pas un role : c'est `Window { modal = true }`\n");
			{
				NkGuiMonteRapport rm, rn;
				NkVector<uint8> pm, pn;
				const bool okm = Monter(DocModal(), W, H, &police, rm, pm);
				const bool okn = Monter(DocNonModal(), W, H, &police, rn, pn);
				// LOIN de la fenetre (300,200)-(540,320) : le coin bas-droit.
				const uint32 voileM = PeintsDans(pm, W, H, 700, 500, 780, 580);
				const uint32 voileN = PeintsDans(pn, W, H, 700, 500, 780, 580);
				char d[400];
				snprintf(d, sizeof(d),
						 "hors de la fenetre : %u pixel(s) peint(s) en MODAL, %u en NON MODAL "
						 "-- attendu > 0 et 0 ; modales comptees=%u",
						 voileM, voileN, rm.modales);
				Check(okm && okn && voileM > 0u && voileN == 0u && rm.modales == 1u
						  && rn.modales == 0u,
					  "(g2a) `modal = true` PEINT un voile ; `modal = false` n'en peint aucun", d);
			}

			// -----------------------------------------------------------------
			// (g3) LES `Window` IMBRIQUES, ATTESTES PAR UN FICHIER DU CORPUS
			// -----------------------------------------------------------------
			printf("\n-- (g3) les `Window` imbriques, valides par Rodolf, attestes par un fichier\n");
			{
				const NkVector<uint8> o = LireCorpus("valides/11_fenetres_imbriquees.nkgui");
				NkArchive a;
				NkGuiDiag e;
				const bool lu = !o.Empty()
								&& NkGuiArchive::Read((const char *)o.Data(), (uint32)o.Size(), a, e);
				NkVector<NkGuiDiag> dg;
				guifmt::NkGValidateResult vr;
				if (lu) {
					vr = guifmt::NkGValidate(a, dg);
				}
				char d[400];
				if (o.Empty()) {
					snprintf(d, sizeof(d), "le fichier du corpus est INTROUVABLE");
				} else if (!lu) {
					snprintf(d, sizeof(d), "REFUS a la lecture : %s", e.message.Data());
				} else {
					snprintf(d, sizeof(d), "%u erreur(s), %u avertissement(s) ; 1er : %s", vr.errors,
							 vr.warnings, dg.Empty() ? "(aucun)" : dg[0].message.Data());
				}
				Check(lu && vr.errors == 0u && vr.warnings == 0u,
					  "(g3a) le fichier de `Window` imbriques passe le VALIDATEUR a 0 / 0", d);

				NkGuiMonteRapport r;
				NkVector<uint8> px;
				bool contenu = false, feuilleDedans = false;
				if (!o.Empty()) {
					NkGuiDiag e2;
					uint32 tex = 0;
					(void)ecrivain::MonterEtPeindre((const char *)o.Data(), (uint32)o.Size(), W, H,
													&police, r, px, e2, tex);
				}
				const NkGuiMonteItem *ext = Trouver(r, "dehors");
				const NkGuiMonteItem *inte = Trouver(r, "dedans");
				const NkGuiMonteItem *btn = Trouver(r, "valider");
				if (ext && inte) {
					contenu = (inte->rect.x >= ext->rect.x - 0.5f)
							  && (inte->rect.y >= ext->rect.y - 0.5f)
							  && (inte->rect.x + inte->rect.w <= ext->rect.x + ext->rect.w + 0.5f);
				}
				if (inte && btn) {
					feuilleDedans = (btn->rect.x >= inte->rect.x - 0.5f)
									&& (btn->rect.y >= inte->rect.y - 0.5f)
									&& (btn->rect.x + btn->rect.w
										<= inte->rect.x + inte->rect.w + 0.5f);
				}
				char d2[500];
				snprintf(d2, sizeof(d2),
						 "exterieure (%.0f, %.0f) %.0f x %.0f ; interieure (%.0f, %.0f) %.0f x %.0f "
						 "(dedans=%d) ; le bouton de l'interieure dedans=%d",
						 ext ? (double)ext->rect.x : -1.0, ext ? (double)ext->rect.y : -1.0,
						 ext ? (double)ext->rect.w : -1.0, ext ? (double)ext->rect.h : -1.0,
						 inte ? (double)inte->rect.x : -1.0, inte ? (double)inte->rect.y : -1.0,
						 inte ? (double)inte->rect.w : -1.0, inte ? (double)inte->rect.h : -1.0,
						 contenu ? 1 : 0, feuilleDedans ? 1 : 0);
				// ⚠️ ON MESURE LA CONTENANCE, PAS UNE EGALITE DE NOMBRES RECOPIES. Le
				//    rect de l'interieure vient de son propre `pos` ; celui du BOUTON,
				//    lui, est calcule -- c'est lui qui prouve que la region a ete
				//    ouverte deux fois.
				Check(ext && inte && btn && contenu && feuilleDedans,
					  "(g3b) la fenetre interieure est DANS l'exterieure, et son bouton dans elle",
					  d2);
			}

			// -----------------------------------------------------------------
			// (g4) `geometry` -- un schema, une ecriture, un montage, un pixel
			// -----------------------------------------------------------------
			printf("\n-- (g4) `geometry` : les calques du canvas, derriere les widgets\n");
			{
				NkGuiMonteRapport rg, rs;
				NkVector<uint8> pg, ps;
				const bool okg = Monter(DocGeometrie(), W, H, &police, rg, pg);
				const bool oks = Monter(DocSansGeometrie(), W, H, &police, rs, ps);
				// Le centre du rectangle ecrit : (600, 400) + (120, 60) / 2.
				const uint32 c = Pixel(pg, W, H, 660, 430);
				const uint32 cs = Pixel(ps, W, H, 660, 430);
				// #F79A28 empaquete par le rasteriseur : R=0xF7 G=0x9A B=0x28 A=0xFF.
				const bool orange = ((c & 0xFFu) == 0xF7u) && (((c >> 8) & 0xFFu) == 0x9Au)
									&& (((c >> 16) & 0xFFu) == 0x28u);
				char d[400];
				snprintf(d, sizeof(d),
						 "monte=%d ; %u forme(s) lue(s), %u peinte(s) ; pixel (660, 430) = %08X "
						 "(attendu R=F7 G=9A B=28), sans la section = %08X",
						 okg ? 1 : 0, rg.formes, rg.formesPeintes, c, cs);
				Check(okg && rg.formes == 1u && rg.formesPeintes == 1u && orange,
					  "(g4a) une `shape` posee peint EXACTEMENT ses pixels", d);
				char d2[300];
				snprintf(d2, sizeof(d2),
						 "sans `geometry` : %u forme(s), pixel (660, 430) = %08X (attendu blanc)",
						 rs.formes, cs);
				Check(oks && rs.formes == 0u && cs == 0xFFFFFFFFu,
					  "(g4b) NEGATIF : le MEME fichier sans `geometry` laisse ces pixels au fond",
					  d2);
			}
			{
				// (g4c) LE VALIDATEUR CONNAIT LES FORMES. Sans ce negatif, « 0 erreur
				// sur le fichier de (g4a) » ne prouverait que l'absence de validation.
				static const char *kFaux =
					"nkgui 0.3\n"
					"geometry {\n"
					"  losange \"x\" { kind = rect }\n"
					"  shape \"y\" { kind = trapeze }\n"
					"  shape \"z\" { pos = (0, 0) fill { color = #000000 } }\n"
					"}\n";
				NkArchive a;
				NkGuiDiag e;
				NkVector<NkGuiDiag> dg;
				guifmt::NkGValidateResult vr;
				const bool lu = NkGuiArchive::Read(kFaux, (uint32)NkString(kFaux).Size(), a, e);
				if (lu) {
					vr = guifmt::NkGValidate(a, dg);
				}
				char d[400];
				snprintf(d, sizeof(d),
						 "lu=%d ; %u erreur(s) attendues >= 3 (bloc hors `shape`, nature inconnue, "
						 "bloc dans une forme) ; 1re : %s",
						 lu ? 1 : 0, vr.errors,
						 dg.Empty() ? "(aucune)" : dg[0].message.Data());
				Check(lu && vr.errors >= 3u,
					  "(g4c) NEGATIF : une forme hors catalogue et un bloc dans une forme sont "
					  "SIGNALES",
					  d);
			}
			{
				// LE POSITIF DU VALIDATEUR : le fichier de (g4a) doit passer a 0 / 0.
				NkArchive a;
				NkGuiDiag e;
				NkVector<NkGuiDiag> dg;
				guifmt::NkGValidateResult vr;
				const NkString t(DocGeometrie());
				const bool lu = NkGuiArchive::Read(t.Data(), (uint32)t.Size(), a, e);
				if (lu) {
					vr = guifmt::NkGValidate(a, dg);
				}
				char d[300];
				snprintf(d, sizeof(d), "lu=%d ; %u erreur(s), %u avertissement(s) ; 1re : %s",
						 lu ? 1 : 0, vr.errors, vr.warnings,
						 dg.Empty() ? "(aucune)" : dg[0].message.Data());
				Check(lu && vr.errors == 0u && vr.warnings == 0u,
					  "(g4d) le fichier `geometry` de (g4a) passe le validateur a 0 / 0", d);
			}
			{
				// ⚠️ LE VALIDATEUR ET LE MONTEUR, DES DEUX COTES. Le document de (g1a)
				//    se MONTE ; il doit aussi etre JUSTE. C'est la lecon payee le
				//    14/09 : une version qui se montait parfaitement etait refusee
				//    81 fois. Sans ce critere, `pos` serait ajoute d'un seul cote.
				NkArchive a;
				NkGuiDiag e;
				NkVector<NkGuiDiag> dg;
				guifmt::NkGValidateResult vr;
				const NkString t(DocPose());
				const bool lu = NkGuiArchive::Read(t.Data(), (uint32)t.Size(), a, e);
				if (lu) {
					vr = guifmt::NkGValidate(a, dg);
				}
				char d[300];
				snprintf(d, sizeof(d), "lu=%d ; %u erreur(s), %u avertissement(s) ; 1re : %s",
						 lu ? 1 : 0, vr.errors, vr.warnings,
						 dg.Empty() ? "(aucune)" : dg[0].message.Data());
				Check(lu && vr.errors == 0u && vr.warnings == 0u,
					  "(g1f) LE FORMAT AUSSI : `pos` sur un `Button` passe le VALIDATEUR", d);
			}
			{
				// NON-REGRESSION DU LEXIQUE : `Spacer { size = 12 }` du corpus reste
				// valide. C'est le risque exact d'avoir rendu `size` universel.
				static const char *kSpacer = "nkgui 0.3\nwidgets {\n  VBox \"v\" {\n"
											 "    Spacer \"vide\" { size = 12 }\n  }\n}\n";
				NkArchive a;
				NkGuiDiag e;
				NkVector<NkGuiDiag> dg;
				guifmt::NkGValidateResult vr;
				const bool lu = NkGuiArchive::Read(kSpacer, (uint32)NkString(kSpacer).Size(), a, e);
				if (lu) {
					vr = guifmt::NkGValidate(a, dg);
				}
				char d[300];
				snprintf(d, sizeof(d), "lu=%d ; %u erreur(s) -- attendu 0 ; 1re : %s", lu ? 1 : 0,
						 vr.errors, dg.Empty() ? "(aucune)" : dg[0].message.Data());
				Check(lu && vr.errors == 0u,
					  "(g1g) NON-REGRESSION : `Spacer { size = 12 }` reste valide", d);
			}

			printf("\n=== %d / %d ===\n", g_ok, g_ok + g_ko);
			return g_ko == 0 ? 0 : 1;
		}

	} // namespace placement
} // namespace nkuidesign

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
