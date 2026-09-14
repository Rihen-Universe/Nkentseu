#pragma once
// -----------------------------------------------------------------------------
// @File    RecetteEcrivain.h
// @Brief   `--recette-ecrivain` : NKUIDesign ECRIT un `.nkgui`, le monteur le
//          REMONTE, et les deux releves se comparent. Sans fenetre ni GPU.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LES CRITERES, ET LEUR ATTENDU ECRIT AVANT LA MESURE
// =============================================================================
//  (z0) LE ZERO DES DEUX COMPTEURS, prouve AVANT de s'en servir.
//       - le compteur de pixels : une image comparee a elle-meme rend 0, et la
//         meme image dont UN pixel a ete change rend exactement 1 ;
//       - le compteur de widgets : un document VIDE rend 0 widget ecrit -- et
//         un fichier NON VIDE malgre tout (en-tete + `widgets { }`).
//       ⚠️ Un compteur de pixels a rendu 40 sans aucune cible, dans ce depot.
//          Celui-ci prouve son zero ET son un.
//
//  (e2) UN DOCUMENT DESSINE DEVIENT UN `.nkgui` QUI MONTE.
//       attendu : widgets ECRITS == blocs RELUS == widgets MONTES, et aucun
//       refus a la relecture. Le nombre n'est PAS ecrit en dur : il est derive
//       du document charge (`rap.widgetsEcrits`), parce qu'un attendu en dur se
//       perime a la premiere retouche du document d'exemple.
//       negatif : document vide -> fichier valide A ZERO WIDGET, pas un fichier
//       vide, et la relecture doit quand meme reussir.
//
//  (e3) LES DEUX IMAGES.
//       attendu ECRIT AVANT, et il n'est pas « identiques » : voir plus bas,
//       les trois familles d'ecart sont PREDITES avec leur ligne de code.
//       La mesure qui fait foi n'est pas le compteur de pixels : c'est la
//       comparaison des DEUX RELEVES DE RECTANGLES, par identifiant. Elle dit
//       OU est l'ecart ; le compteur ne dit que combien.
//
// =============================================================================
//  L'ATTENDU DE (e3), ECRIT ICI POUR QU'ON NE LE REECRIVE PAS APRES LA MESURE
// =============================================================================
//  Les deux images DIFFERERONT, et voici les trois familles predites, chacune
//  avec la ligne qui la cause :
//
//   1. LA COULEUR. `NkGuiMonteur.h:144` ecarte `appearance` / `fill` / `stroke` ;
//      le seul aplat peint est `PanelBackground(ctx, r)` (l.519), qui prend la
//      couleur du THEME NKGui. Le monteur ne lit AUCUNE couleur du document.
//   2. LE PLACEMENT DES FEUILLES. Le monteur place au CURSEUR. Seuls `Window` et
//      `Panel` honorent `pos`/`size` (`RegionCourante`, l.518 et l.802). Les
//      conteneurs tomberont donc au bon endroit ; les feuilles descendront en
//      pile.
//   3. LA POLICE. L'editeur peint `police_px` / `graisse` par noeud ; le monteur
//      n'a qu'une police.
//
//  Si la mesure montre une QUATRIEME famille, c'est une trouvaille et elle est
//  nommee. Si elle en montre MOINS de trois, l'instrument ne mesure pas ce que
//  je crois, et c'est dit aussi.
//
// ⚠️ AUCUNE FENETRE N'EST OUVERTE et aucune entree n'est injectee. Les deux
//    rendus passent par `NkGuiDrawListRaster`, le rasteriseur LOGICIEL : il n'y
//    a ni fenetre, ni device, ni GPU a piloter. La garde est tenue par
//    construction, pas par discipline.
// -----------------------------------------------------------------------------

#include <cstdio>

#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Doc/NkGuiMonteur.h"
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKImage/Core/NkImage.h"

#include "Export.h"
#include "NkGuiRoundTrip.h" // NkGCountBlocks : deja ecrit, on ne le recopie pas
#include "NkGuiEcrire.h"

namespace nkuidesign {

	namespace ecrivain {

		using namespace nkentseu;
		using nkentseu::nkgui::NkGuiDrawListRaster;
		using nkentseu::nkgui::NkGuiFont;
		using nkentseu::nkgui::NkGuiMonteEtat;
		using nkentseu::nkgui::NkGuiMonteRapport;
		using nkentseu::nkgui::NkGuiMonteur;

		static int g_ok = 0, g_ko = 0;

		inline void Check(bool ok, const char *nom, const char *detail) {
			(ok ? g_ok : g_ko)++;
			printf("  [ %s ] %s\n", ok ? "OK" : "KO", nom);
			if (detail && *detail) {
				printf("         %s\n", detail);
			}
		}

		/// Combien de pixels different entre deux tampons RGBA de meme taille.
		/// Rend 0xFFFFFFFF quand les tailles ne sont pas comparables -- ce n'est
		/// PAS « aucune difference », et confondre les deux serait exactement le
		/// vert faux qu'on cherche a eviter.
		inline uint32 PixelsQuiDifferent(const uint8 *a, const uint8 *b, usize n) {
			if (!a || !b || n == 0u) {
				return 0xFFFFFFFFu;
			}
			uint32 d = 0;
			for (usize i = 0; i + 3u < n; i += 4u) {
				if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]
					|| a[i + 3] != b[i + 3]) {
					++d;
				}
			}
			return d;
		}

		/// Le document de travail de la recette : celui que l'application charge
		/// elle-meme au demarrage, cherche aux memes endroits (l'executable ne vit
		/// pas a la racine du depot).
		inline bool ChargerDocument(NkUIDocument &d, char *chemin, uint32 cap) {
			static const char *kChemins[] = {
				"nkuidesign_document.nkuidoc",
				"../../../../nkuidesign_document.nkuidoc",
				"Applications/NKUIDesign/design/mises_en_scene/demo_ecran_01.nkuidoc",
				"../../../../Applications/NKUIDesign/design/mises_en_scene/demo_ecran_01.nkuidoc"};
			for (uint32 i = 0; i < 4u; ++i) {
				const NkString t = NkFile::ReadAllText(kChemins[i]);
				if (t.Size() > 0 && d.Load(t.Data())) {
					snprintf(chemin, cap, "%s", kChemins[i]);
					return true;
				}
			}
			return false;
		}

		/// Monter un texte `.nkgui` et le rasteriser. Rend faux si le LECTEUR
		/// refuse le fichier -- un ecrivain qui produit du charabia doit se voir.
		inline bool MonterEtPeindre(const char *src, uint32 len, int32 W, int32 H,
									NkGuiFont *police, NkGuiMonteRapport &rap,
									NkVector<uint8> &pixels, NkGuiDiag &err, uint32 &texInconnues) {
			NkArchive doc;
			if (!NkGuiArchive::Read(src, len, doc, err)) {
				return false;
			}
			nkentseu::nkgui::NkGuiContext ctx;
			ctx.viewW = W;
			ctx.viewH = H;
			if (police && police->Valid()) {
				ctx.font = police;
			}
			const nkentseu::nkgui::NkRect region{0.f, 0.f, (float32)W, (float32)H};
			ctx.BeginFrame(0.016f);
			ctx.BeginLayout(region);
			ctx.DL().Reset();

			NkGuiMonteEtat etat;
			NkGuiMonteur::Preparer(doc, etat);
			NkGuiMonteur::Monter(ctx, doc, etat, rap);

			NkGuiDrawListRaster ras;
			if (!ras.Init(W, H)) {
				return false;
			}
			ras.Effacer(0xFFFFFFFFu); // le meme fond que l'export de l'editeur
			if (police && police->Valid() && police->pixels) {
				ras.PoserTexture(police->TexId(), police->pixels, police->atlasW, police->atlasH, 1);
			}
			texInconnues = ras.Rasteriser(ctx.DL());
			const usize n = (usize)W * (usize)H * 4u;
			pixels.Resize(n);
			const uint8 *sp = ras.Pixels();
			for (usize i = 0; i < n; ++i) {
				pixels[(uint32)i] = sp[i];
			}
			return true;
		}

		/// Ecrit un tampon RGBA en PNG. Le tampon rendu par `Encode` est alloue par
		/// NkAlloc : il se libere par `memory::NkFree`, jamais par free() -- c'est un
		/// `c0000374` connu du depot, ecrit dans son CLAUDE.md.
		inline bool EcrirePng(const uint8 *px, int32 W, int32 H, const char *chemin) {
			if (!px || W <= 0 || H <= 0) {
				return false;
			}
			NkImage img = NkImage::Alloc((uint32)W, (uint32)H, NkImagePixelFormat::NK_RGBA32);
			if (!img.Pixels()) {
				return false;
			}
			const usize n = (usize)W * (usize)H * 4u;
			for (usize i = 0; i < n; ++i) {
				img.Pixels()[i] = px[i];
			}
			uint8 *out = nullptr;
			usize taille = 0;
			if (!NkPNGCodec::Encode(img, out, taille) || !out) {
				return false;
			}
			NkVector<uint8> octets;
			octets.Resize(taille);
			for (usize i = 0; i < taille; ++i) {
				octets[(uint32)i] = out[i];
			}
			nkentseu::memory::NkFree(out);
			return NkFile::WriteAllBytes(chemin, octets);
		}

		/// Combien de fois `motif` apparait dans `texte`. Sert a compter des cles
		/// ECRITES dans le fichier -- on mesure ce qui est sur le disque, pas ce que le
		/// code croit avoir ecrit.
		inline uint32 Occurrences(const NkString &texte, const char *motif) {
			if (!motif || !*motif || texte.Empty()) {
				return 0u;
			}
			uint32 n = 0;
			const char *d = texte.Data();
			const uint32 tt = (uint32)texte.Size();
			uint32 lm = 0;
			while (motif[lm]) {
				++lm;
			}
			if (lm == 0u || lm > tt) {
				return 0u;
			}
			for (uint32 i = 0; i + lm <= tt; ++i) {
				uint32 k = 0;
				while (k < lm && d[i + k] == motif[k]) {
					++k;
				}
				if (k == lm) {
					++n;
				}
			}
			return n;
		}

		/// LA NATURE DU PLACEMENT D'UN DOCUMENT, comptee sur le document CHARGE.
		/// C'est la mesure qui tranche (f1) : un document dont aucun noeud n'emploie un
		/// agencement en FLUX ne peut pas etre reproduit par un moteur qui n'empile que
		/// des flux -- et ce n'est alors pas l'ecrivain qui est en faute.
		inline void CompterAgencements(const NkUIDocument &d, uint32 &absolus, uint32 &flux,
															uint32 &total) {
			absolus = flux = total = 0;
			for (uint32 i = 0; i < (uint32)d.nodes.Size(); ++i) {
				++total;
				const NkLayoutKind k = d.nodes[i].layout.kind;
				if (k == NkLayoutKind::Row || k == NkLayoutKind::Column || k == NkLayoutKind::Grid) {
					++flux;
				} else {
					// None, Free, Anchor : la position est LUE, pas calculee par un flux.
					++absolus;
				}
			}
		}

		// =====================================================================
		//  LA RECETTE
		// =====================================================================
		inline int RecetteEcrivain() {
			g_ok = 0;
			g_ko = 0;
			printf("\n=== RECETTE ECRIVAIN .nkgui -- NKUIDesign ecrit, le monteur remonte ===\n");
			printf("    aucune fenetre ouverte, aucun GPU, aucune entree injectee.\n");

			// -----------------------------------------------------------------
			// (z0) LE ZERO DES COMPTEURS, AVANT TOUT LE RESTE
			// -----------------------------------------------------------------
			printf("\n-- (z0) les compteurs prouvent leur ZERO, puis leur UN\n");
			{
				NkVector<uint8> a, b;
				a.Resize(64u * 4u);
				b.Resize(64u * 4u);
				for (uint32 i = 0; i < 64u * 4u; ++i) {
					a[i] = (uint8)(i * 7u);
					b[i] = a[i];
				}
				const uint32 zero = PixelsQuiDifferent(a.Data(), b.Data(), 64u * 4u);
				char d1[160];
				snprintf(d1, sizeof(d1), "meme tampon -> %u pixel(s) different(s), attendu 0", zero);
				Check(zero == 0u, "(z0a) le compteur de pixels rend ZERO sur deux images egales", d1);

				b[4 * 3 + 1] = (uint8)(b[4 * 3 + 1] ^ 0xFFu); // UN pixel, une composante
				const uint32 un = PixelsQuiDifferent(a.Data(), b.Data(), 64u * 4u);
				char d2[160];
				snprintf(d2, sizeof(d2), "un seul octet change -> %u pixel(s), attendu 1", un);
				Check(un == 1u, "(z0b) le compteur rend UN quand UN pixel change", d2);

				const uint32 incomparable = PixelsQuiDifferent(a.Data(), nullptr, 0u);
				Check(incomparable == 0xFFFFFFFFu,
					  "(z0c) des tailles incomparables ne rendent PAS zero",
					  "sinon « rien a comparer » se lirait « rien ne differe »");
			}

			// -----------------------------------------------------------------
			// (z0d) LE DOCUMENT VIDE -- le negatif de (e2), place avant lui
			// -----------------------------------------------------------------
			{
				NkUIDocument vide;
				vide.NewDocument("Toile", NkAuthor::Humain);
				NkLayoutResult lay;
				NkComputeLayout(vide, NkPaintRect{0.f, 0.f, 800.f, 600.f}, lay);
				guifmt::NkEcritRapport rv;
				const NkString texte = guifmt::NkDocumentVersTexte(vide, lay, rv);
				NkGuiDiag err;
				NkArchive relu;
				const bool lisible =
					NkGuiArchive::Read(texte.Data(), (uint32)texte.Size(), relu, err);
				char d[300];
				snprintf(d, sizeof(d),
						 "%u widget(s) ecrit(s), fichier de %u octet(s), relu=%d ; extrait : %.40s",
						 rv.widgetsEcrits, (uint32)texte.Size(), lisible ? 1 : 0,
						 texte.Data() ? texte.Data() : "");
				Check(rv.widgetsEcrits == 0u && texte.Size() > 0u && lisible,
					  "(z0d) NEGATIF : un document VIDE donne un fichier VALIDE a zero widget", d);
			}

			// -----------------------------------------------------------------
			// LE DOCUMENT REEL
			// -----------------------------------------------------------------
			printf("\n-- (e2) un document dessine devient un .nkgui qui MONTE\n");
			char chemin[512] = {0};
			NkUIDocument doc;
			if (!ChargerDocument(doc, chemin, sizeof(chemin))) {
				Check(false, "(e2a) le document d'exemple se charge",
					  "aucun .nkuidoc trouve : la recette ne mesure RIEN, elle ne passe pas");
				printf("\n=== %d / %d ===\n", g_ok, g_ok + g_ko);
				return 1;
			}
			{
				char d[400];
				snprintf(d, sizeof(d), "%s -- %u noeud(s)", chemin, (uint32)doc.nodes.Size());
				Check(doc.nodes.Size() > 1u, "(e2a) le document d'exemple se charge", d);
			}

			// La disposition : la MEME surface que l'export de l'editeur utilise
			// quand l'application n'est pas a jour (`NkExporterImage`), pour que
			// les deux cotes parlent des memes rectangles.
			NkLayoutResult lay;
			NkComputeLayout(doc, NkPaintRect{0.f, 0.f, 1400.f, 900.f}, lay);

			guifmt::NkEcritRapport rap;
			const NkString texte = guifmt::NkDocumentVersTexte(doc, lay, rap);
			{
				char d[400];
				snprintf(d, sizeof(d),
						 "%u noeud(s) vus, %u widget(s) ecrit(s) (%u conteneur(s), %u feuille(s)), "
						 "%u apparence(s), fichier de %u octet(s)",
						 rap.noeudsVus, rap.widgetsEcrits, rap.conteneurs, rap.feuilles,
						 rap.apparencesEcrites, (uint32)texte.Size());
				Check(rap.widgetsEcrits > 0u && texte.Size() > 0u,
					  "(e2b) l'ecrivain produit un texte .nkgui non vide", d);
			}

			// Le fichier sur le disque, PAR OCTETS. Puis relu PAR OCTETS.
			const char *sortie = "nkuidesign_ecrit.nkgui";
			{
				guifmt::NkEcritRapport r2;
				const bool ecrit = guifmt::NkEcrireNkgui(doc, lay, sortie, r2);
				const NkVector<uint8> relus = NkFile::ReadAllBytes(sortie);
				bool memeOctets = (relus.Size() == (usize)texte.Size());
				for (uint32 i = 0; memeOctets && i < (uint32)texte.Size(); ++i) {
					if ((char)relus[i] != texte.Data()[i]) {
						memeOctets = false;
					}
				}
				char d[300];
				snprintf(d, sizeof(d),
						 "ecrit=%d ; %u octet(s) en memoire, %u relus du disque ; identiques=%d",
						 ecrit ? 1 : 0, (uint32)texte.Size(), (uint32)relus.Size(),
						 memeOctets ? 1 : 0);
				Check(ecrit && memeOctets,
					  "(e2c) le fichier sur le disque est EXACTEMENT le texte produit", d);
			}

			// Le lecteur doit accepter ce que l'ecrivain a produit.
			uint32 blocsRelus = 0;
			{
				NkArchive relu;
				NkGuiDiag err;
				const bool ok = NkGuiArchive::Read(texte.Data(), (uint32)texte.Size(), relu, err);
				blocsRelus = ok ? guifmt::NkGCountBlocks(relu) : 0u;
				char d[400];
				if (ok) {
					snprintf(d, sizeof(d), "%u bloc(s) relu(s) (dont la section `widgets` et les "
										   "blocs `appearance`)",
							 blocsRelus);
				} else {
					snprintf(d, sizeof(d), "REFUS : %s ligne %u colonne %u : %s", err.code.Data(),
							 (uint32)err.line, (uint32)err.column, err.message.Data());
				}
				Check(ok && blocsRelus > 0u,
					  "(e2d) le LECTEUR accepte le fichier que l'ECRIVAIN a produit", d);
			}

			// -----------------------------------------------------------------
			// LE MONTAGE
			// -----------------------------------------------------------------
			const int32 W = 1400, H = 900;
			NkGuiFont police;
			const bool policeOk =
				police.LoadEmbedded(nkentseu::NkEmbeddedFontId::DroidSans, 15.f, false);
			{
				char d[200];
				snprintf(d, sizeof(d), "police embarquee chargee=%d, atlas %dx%d", policeOk ? 1 : 0,
						 police.atlasW, police.atlasH);
				Check(policeOk && police.Valid(),
					  "(e2e) la police du banc est chargee AVANT de conclure sur le texte", d);
			}

			NkGuiMonteRapport mrap;
			NkVector<uint8> pixMonteur;
			NkGuiDiag merr;
			uint32 texInconnues = 0;
			const bool monte = MonterEtPeindre(texte.Data(), (uint32)texte.Size(), W, H, &police,
											   mrap, pixMonteur, merr, texInconnues);
			{
				char d[400];
				snprintf(d, sizeof(d),
						 "monte=%d ; %u section(s), %u widget(s) rencontre(s), %u monte(s), "
						 "%u role(s) inconnu(s), %u apparence(s) lue(s), %u texture(s) inconnue(s)",
						 monte ? 1 : 0, mrap.sections, mrap.widgets, mrap.montes, mrap.rolesInconnus,
						 mrap.apparencesLues, texInconnues);
				Check(monte && mrap.montes > 0u, "(e2f) le monteur MONTE le fichier produit", d);
			}
			{
				// L'attendu est DERIVE : le nombre de widgets ecrits, pas un nombre
				// recopie. Un attendu en dur se perimerait a la premiere retouche du
				// document d'exemple et crierait rouge sur un montage correct.
				char d[300];
				snprintf(d, sizeof(d), "ecrits=%u, rencontres=%u, montes=%u, roles inconnus=%u",
						 rap.widgetsEcrits, mrap.widgets, mrap.montes, mrap.rolesInconnus);
				Check(mrap.widgets == rap.widgetsEcrits && mrap.montes == rap.widgetsEcrits
						  && mrap.rolesInconnus == 0u,
					  "(e2g) widgets ECRITS == widgets RENCONTRES == widgets MONTES", d);
			}

			// -----------------------------------------------------------------
			// (e3) LES DEUX IMAGES
			// -----------------------------------------------------------------
			printf("\n-- (e3) les deux images, et surtout OU elles different\n");

			// L'image de l'editeur, par sa propre porte d'export (sans fenetre).
			static DesignState st;
			st.doc = doc;
			st.layout = lay;
			st.host.SyncTo(st.doc);
			NkExportOptions opts;
			opts.echelle = 1.f;
			opts.page = 0; // la page par defaut du document
			NkExportResultat res;
			NkImage imgEditeur;
			const bool exporte = NkExporterImage(st, opts, imgEditeur, res) && imgEditeur.Pixels();
			{
				char d[400];
				snprintf(d, sizeof(d), "export=%d (%s) ; %d x %d ; %u noeud(s) peint(s)",
						 exporte ? 1 : 0, res.message, res.largeur, res.hauteur, res.noeuds);
				Check(exporte, "(e3a) l'editeur rend son image, sans fenetre", d);
			}

			// LA MESURE QUI FAIT FOI : les deux releves de rectangles, par id.
			{
				uint32 apparies = 0, absents = 0, memePlace = 0;
				char premier[300];
				premier[0] = '\0';
				for (uint32 i = 0; i < (uint32)rap.items.Size(); ++i) {
					const guifmt::NkEcritItem &e = rap.items[i];
					const nkentseu::nkgui::NkGuiMonteItem *m = nullptr;
					for (uint32 j = 0; j < (uint32)mrap.items.Size(); ++j) {
						if (mrap.items[j].id.Compare(e.id) == 0) {
							m = &mrap.items[j];
							break;
						}
					}
					if (!m) {
						++absents;
						continue;
					}
					++apparies;
					const float32 dx = m->rect.x - e.x, dy = m->rect.y - e.y;
					const float32 ax = dx < 0.f ? -dx : dx, ay = dy < 0.f ? -dy : dy;
					if (ax <= 1.f && ay <= 1.f) {
						++memePlace;
					} else if (premier[0] == '\0') {
						snprintf(premier, sizeof(premier),
								 "1er ecart : %s (%s) editeur (%.0f, %.0f) vs monteur (%.0f, %.0f)",
								 e.id.Data(), e.role.Data(), (double)e.x, (double)e.y,
								 (double)m->rect.x, (double)m->rect.y);
					}
				}
				char d[600];
				snprintf(d, sizeof(d),
						 "%u ecrit(s), %u apparie(s) par id, %u absent(s) du montage ; "
						 "%u au MEME endroit (a 1 px) ; %s",
						 (uint32)rap.items.Size(), apparies, absents, memePlace,
						 premier[0] ? premier : "aucun ecart de place");
				// ⚠️ CE CRITERE N'EXIGE PAS L'EGALITE DES PLACES. L'attendu ecrit
				//    avant la mesure dit que les feuilles NE tomberont PAS au meme
				//    endroit. Ce qu'il exige, c'est que CHAQUE widget ecrit se
				//    retrouve dans le montage : un widget qui disparait est une
				//    perte, un widget deplace est une limite connue et nommee.
				Check(apparies == (uint32)rap.items.Size() && absents == 0u,
					  "(e3b) CHAQUE widget ecrit se retrouve dans le montage, par identifiant", d);
			}

			// LES DEUX IMAGES SUR LE DISQUE -- parce qu'un compteur vert n'est pas un
			// rendu juste, et qu'il faut pouvoir les REGARDER.
			if (exporte) {
				const bool a = EcrirePng(imgEditeur.Pixels(), imgEditeur.Width(), imgEditeur.Height(),
									"nkuidesign_image_editeur.png");
				printf("  [ .. ] image de l'EDITEUR : nkuidesign_image_editeur.png (%d x %d) ecrite=%d\n",
						imgEditeur.Width(), imgEditeur.Height(), a ? 1 : 0);
			}
			if (pixMonteur.Size() > 0u) {
				const bool b = EcrirePng(pixMonteur.Data(), W, H, "nkuidesign_image_monteur.png");
				printf("  [ .. ] image du MONTEUR : nkuidesign_image_monteur.png (%d x %d) ecrite=%d\n", W,
						H, b ? 1 : 0);
			}

			// LE COMPTEUR DE PIXELS, ALIGNE SUR LA ZONE EXPORTEE.
			// Les deux images n'ont PAS la meme taille : l'editeur exporte la boite de
			// la page, le monteur peint toute la surface du document. Les comparer sans
			// decalage ne mesurerait que le cadrage. Le decalage est DERIVE de
			// `res.zone`, jamais recopie a la main.
			if (exporte && pixMonteur.Size() > 0u && imgEditeur.Pixels()) {
				const int32 ox = (int32)res.zone.x, oy = (int32)res.zone.y;
				uint32 compares = 0, differents = 0, horsCadre = 0;
				for (int32 y = 0; y < imgEditeur.Height(); ++y) {
					for (int32 x = 0; x < imgEditeur.Width(); ++x) {
						const int32 mx = x + ox, my = y + oy;
						if (mx < 0 || my < 0 || mx >= W || my >= H) {
							++horsCadre;
							continue;
						}
						const uint8 *pa =
								imgEditeur.Pixels() + ((usize)y * (usize)imgEditeur.Width() + (usize)x) * 4u;
						const uint8 *pb = pixMonteur.Data() + ((usize)my * (usize)W + (usize)mx) * 4u;
						++compares;
						if (pa[0] != pb[0] || pa[1] != pb[1] || pa[2] != pb[2] || pa[3] != pb[3]) {
							++differents;
						}
					}
				}
				char d[400];
				snprintf(d, sizeof(d),
							"zone exportee (%.0f, %.0f) %.0f x %.0f ; %u pixel(s) compare(s), %u hors du cadre "
							"du monteur ; %u different(s) (%.1f %%)",
							(double)res.zone.x, (double)res.zone.y, (double)res.zone.w, (double)res.zone.h,
							compares, horsCadre, differents,
							compares ? (double)differents * 100.0 / (double)compares : 0.0);
				// PUBLIE, PAS JUGE : l'attendu ecrit avant dit que les images different.
				// Un critere « diff == 0 » serait faux ; « diff > 0 » ne prouverait rien.
				printf("  [ .. ] (e3c) releve du compteur de pixels, aligne sur la zone\n         %s\n", d);
			}

			// -----------------------------------------------------------------
			// (f3) LE VALIDATEUR, DANS LA RECETTE -- plus jamais un geste a la main
			// -----------------------------------------------------------------
			// 🔴 C'EST MA PROPRE LECON, RENDUE PERMANENTE. Ma premiere version se
			//    MONTAIT parfaitement et etait refusee 81 fois par `--valider` : le
			//    MONTEUR est plus permissif que le FORMAT. Verifier a la main, c'est
			//    verifier tant qu'on y pense. Le critere vit donc ICI.
			printf("\n-- (f3) le fichier produit passe le VALIDATEUR du depot\n");
			{
				NkArchive relu;
				NkGuiDiag err;
				NkVector<NkGuiDiag> diags;
				guifmt::NkGValidateResult vr;
				const bool lu = NkGuiArchive::Read(texte.Data(), (uint32)texte.Size(), relu, err);
				if (lu) {
					vr = guifmt::NkGValidate(relu, diags);
				}
				char d[400];
				if (!lu) {
					snprintf(d, sizeof(d), "le fichier ne se relit meme pas : %s", err.message.Data());
				} else if (vr.errors == 0u && vr.warnings == 0u) {
					snprintf(d, sizeof(d), "0 erreur, 0 avertissement sur %u diagnostic(s) emis",
								(uint32)diags.Size());
				} else {
					snprintf(d, sizeof(d), "%u erreur(s), %u avertissement(s) ; 1re : %s",
								vr.errors, vr.warnings,
								diags.Empty() ? "(aucune listee)" : diags[0].message.Data());
				}
				Check(lu && vr.errors == 0u && vr.warnings == 0u,
					  "(f3a) le fichier produit : 0 erreur de validation", d);
			}
			{
				// NEGATIF : un role hors catalogue DOIT etre signale. Sans lui, (f3a)
				// pourrait etre vert parce que le validateur ne valide rien.
				NkString faux("nkgui 0.3\nwidgets {\n  Bidule \"x\" { truc = 1 }\n}\n");
				NkArchive a;
				NkGuiDiag e;
				NkVector<NkGuiDiag> dg;
				guifmt::NkGValidateResult vr;
				const bool lu = NkGuiArchive::Read(faux.Data(), (uint32)faux.Size(), a, e);
				if (lu) {
					vr = guifmt::NkGValidate(a, dg);
				}
				char d[300];
				snprintf(d, sizeof(d), "role inconnu `Bidule` -> lu=%d, %u erreur(s) ; 1re : %s",
							lu ? 1 : 0, vr.errors,
							dg.Empty() ? "(aucune)" : dg[0].code.Data());
				Check(lu && vr.errors > 0u,
					  "(f3b) NEGATIF : un role hors catalogue EST signale", d);
			}

			// -----------------------------------------------------------------
			// (f2) `size` ET `weight` NE PARTENT PLUS EN SILENCE
			// -----------------------------------------------------------------
			printf("\n-- (f2) le corps et la graisse sont ECRITS, et comptes contre le document\n");
			{
				// Les attendus sont DERIVES du document charge. Un nombre recopie se
				// perimerait a la premiere retouche de la maquette et crierait rouge sur
				// un ecrivain correct.
				uint32 avecCorps = 0, avecGraisse = 0;
				for (uint32 i = 0; i < (uint32)doc.nodes.Size(); ++i) {
					if (doc.nodes[i].fontPx != 0.f) {
						++avecCorps;
					}
					if (doc.nodes[i].fontWeight != 0.f) {
						++avecGraisse;
					}
				}
				// 🔴 `size` NOMME DEUX CHOSES DANS LE FORMAT, et mon premier compteur
				//    les additionnait : `Window.size = (w, h)` est une GEOMETRIE,
				//    `appearance.size = 12` est un CORPS DE POLICE. Le releve annoncait
				//    39 pour 18 attendus -- 22 geometries plus 17 corps. Le compteur
				//    etait faux, pas l'ecrivain. On retranche la forme parenthesee.
				const uint32 ecritsCorps =
					Occurrences(texte, "size = ") - Occurrences(texte, "size = (");
				const uint32 ecritsGraisse = Occurrences(texte, "weight = ");
				char d[400];
				snprintf(d, sizeof(d),
							"document : %u noeud(s) a corps, %u a graisse ; fichier : %u `size = `, "
							"%u `weight = `",
							avecCorps, avecGraisse, ecritsCorps, ecritsGraisse);
				Check(ecritsCorps == avecCorps && ecritsGraisse == avecGraisse,
					  "(f2a) autant de `size`/`weight` ecrits que le document en porte", d);
			}
			{
				// NEGATIF D'ADDITIVITE : sans corps ni graisse, la cle n'existe pas. Une
				// cle ecrite « a zero » ferait grossir tout fichier d'avant.
				NkUIDocument nu;
				nu.NewDocument("Toile", NkAuthor::Humain);
				const int32 pg = nu.AddChild(0, "", NkAuthor::Humain);
				nu.nodes[(uint32)pg].shape = NkString("text");
				nu.nodes[(uint32)pg].text = NkString("sans corps");
				NkLayoutResult l2;
				NkComputeLayout(nu, NkPaintRect{0.f, 0.f, 400.f, 300.f}, l2);
				guifmt::NkEcritRapport r3;
				const NkString t2 = guifmt::NkDocumentVersTexte(nu, l2, r3);
				// 🔴 ET CE COMPTEUR-CI A REFAIT LA FAUTE DE (f2a), UN LOT PLUS TARD.
				//    `size` nomme DEUX choses : `appearance.size` est un CORPS DE POLICE,
				//    `size = (w, h)` est une GEOMETRIE. (f2a) retranchait deja la forme
				//    parenthesee ; (f2b) ne le faisait pas, parce qu'au moment ou il a ete
				//    ecrit AUCUN noeud ne portait de geometrie. Le jour ou le pont s'est mis
				//    a poser tout le monde, ce negatif a crie au rouge sur un ecrivain
				//    correct. **Le compteur etait faux, pas le code mesure** -- et c'est la
				//    deuxieme fois que ce meme mot le provoque.
				const uint32 c2 = Occurrences(t2, "size = ") - Occurrences(t2, "size = (");
				const uint32 g2 = Occurrences(t2, "weight = ");
				char d[300];
				snprintf(d, sizeof(d), "document sans corps ni graisse -> %u `size`, %u `weight` "
							"(attendu 0 et 0), fichier de %u octet(s)",
							c2, g2, (uint32)t2.Size());
				Check(c2 == 0u && g2 == 0u && t2.Size() > 0u,
					  "(f2b) NEGATIF : sans corps ni graisse, AUCUNE cle ajoutee", d);
			}

			// -----------------------------------------------------------------
			// (f1) LA CAUSE DU PLACEMENT -- etablie AVANT tout correctif
			// -----------------------------------------------------------------
			printf("\n-- (f1) pourquoi les feuilles descendent en pile : la CAUSE, mesuree\n");
			{
				uint32 absolus = 0, flux = 0, total = 0;
				CompterAgencements(doc, absolus, flux, total);
				char d[400];
				snprintf(d, sizeof(d),
							"le document place %u noeud(s) sur %u en ABSOLU (None/Free/Anchor) et "
							"%u en FLUX (Row/Column/Grid)",
							absolus, total, flux);
				// Ce n'est pas un critere de qualite : c'est un CONSTAT chiffre. Il est
				// pose en critere pour qu'il rougisse le jour ou la maquette changera de
				// nature -- ce jour-la, la conclusion de (f1) devra etre refaite.
				Check(flux == 0u, "(f1a) le document de l'editeur est a placement ABSOLU, sans aucun flux", d);
			}
			{
				// TEMOIN DE CONTROLE, et c'est lui qui distingue « le monteur est casse »
				// de « les deux modeles ne disent pas la meme chose ». Un fichier du
				// corpus ECRIT EN FLUX doit, lui, s'empiler correctement : des y
				// STRICTEMENT croissants d'un item au suivant.
				static const char *kFlux[] = {
					"Applications/NKUIDesign/exemples/valides/01_panneau_reglages.nkgui",
					"../../../../Applications/NKUIDesign/exemples/valides/01_panneau_reglages.nkgui"};
				NkVector<uint8> octets;
				for (uint32 i = 0; i < 2u && octets.Empty(); ++i) {
					octets = NkFile::ReadAllBytes(kFlux[i]);
				}
				NkGuiMonteRapport fr;
				NkVector<uint8> fp;
				NkGuiDiag fe;
				uint32 ft = 0;
				const bool ok = !octets.Empty()
								&& MonterEtPeindre((const char *)octets.Data(), (uint32)octets.Size(),
												  600, 700, &police, fr, fp, fe, ft);
				// 🔴 LE TEMOIN COMPTAIT UN CHEVAUCHEMENT LA OU IL Y A UNE LIGNE. Mon
				//    premier releve rendait 7 croissants sur 8 feuilles et rougissait un
				//    montage correct : les deux derniers boutons de `01_panneau_reglages`
				//    vivent dans une `HBox "actions"` et PARTAGENT donc leur y. L'auteur
				//    du monteur avait ecrit l'avertissement mot pour mot au-dessus du
				//    champ `axeHorizontal` -- et j'ai quand meme mesure sans le lire.
				//    Sur un axe horizontal, c'est X qui doit croitre, pas Y.
				// 🔴 DEUXIEME VERSION DE CE TEMOIN, ET LA PREMIERE ETAIT ENCORE FAUSSE.
				//    J'avais bien separe les deux axes -- Y en colonne, X en ligne --
				//    mais je mettais a jour LES DEUX precedents a chaque item. Le
				//    PREMIER bouton d'une `HBox` comparait donc son x a celui du dernier
				//    item de la COLONNE au-dessus, qui commence au meme bord : 7 sur 8.
				//    Chaque item se compare au precedent DE SON AXE, et une ligne
				//    REMET A ZERO son propre precedent (une nouvelle ligne repart du
				//    bord). La regle vient du contrat du monteur -- « deux widgets d'une
				//    HBox partagent leur y » -- pas du resultat que je voulais obtenir.
				uint32 feuilles = 0, croissants = 0, yDistincts = 0;
				float32 yPrecV = -1.f, xPrecH = -1.f, yVu = -1e9f;
				for (uint32 i = 0; i < (uint32)fr.items.Size(); ++i) {
					if (fr.items[i].conteneur) {
						continue;
					}
					++feuilles;
					const NkRect &r = fr.items[i].rect;
					bool ok;
					if (fr.items[i].axeHorizontal) {
						ok = (r.x > xPrecH);
						xPrecH = r.x;
					} else {
						ok = (r.y > yPrecV);
						yPrecV = r.y;
						xPrecH = -1.f; // une nouvelle rangee repart du bord
					}
					if (ok) {
						++croissants;
					}
					if (r.y > yVu + 0.5f || r.y < yVu - 0.5f) {
						++yDistincts;
						yVu = r.y;
					}
				}
				char d[400];
				snprintf(d, sizeof(d),
							"corpus EN FLUX : %u widget(s) monte(s), %u feuille(s), %u places dans le "
							"sens de leur boite (Y en colonne, X en ligne), %u hauteur(s) distincte(s) "
							"-- le monteur empile correctement ce qui est ECRIT en flux",
							fr.montes, feuilles, croissants, yDistincts);
				// `yDistincts > 1` est le FALSIFICATEUR : si le monteur empilait tout au
				// meme endroit -- le symptome meme qu'on etudie en (f1) -- il n'y aurait
				// qu'une hauteur, et ce temoin ne pourrait pas etre vert par accident.
				Check(ok && fr.montes > 0u && feuilles > 0u && croissants == feuilles
						&& yDistincts > 1u,
					  "(f1b) TEMOIN : un document ecrit EN FLUX se monte correctement", d);
			}

			printf("\n=== %d / %d ===\n", g_ok, g_ok + g_ko);
			printf("    fichier produit : %s\n", sortie);
			if (rap.rolesHorsCatalogue > 0u) {
				printf("    ROLES HORS CATALOGUE SIGNALES (aucun n'a ete migre) :\n");
				for (uint32 i = 0; i < (uint32)rap.nomsHorsCatalogue.Size(); ++i) {
					printf("      - %s\n", rap.nomsHorsCatalogue[i].Data());
				}
			}
			if (rap.encresPerdues > 0u) {
				printf("    %u noeud(s) ont un FOND et une ENCRE ; une `appearance` n'a qu'un\n"
						   "    `fill`. Le fond est ecrit, l'encre du texte ne l'est pas -- comptee\n"
						   "    ici plutot que perdue en silence.\n",
						   rap.encresPerdues);
			}
			if (rap.textesAbsorbes > 0u) {
				printf("    %u texte(s) absorbe(s) en `label` (avec leur corps et leur graisse) :\n"
						   "    en .nkgui un bouton porte son mot en propriete, pas en enfant.\n",
						   rap.textesAbsorbes);
			}
			if (rap.transfoAbandonnees > 0u) {
				printf("    %u noeud(s) portent une transformation que le format .nkgui ne sait\n"
					   "    pas dire (rotation, miroir, inclinaison, fusion, opacite). Elles ne\n"
					   "    sont pas ecrites, et elles ne sont pas devinees.\n",
					   rap.transfoAbandonnees);
			}
			return g_ko == 0 ? 0 : 1;
		}

	} // namespace ecrivain
} // namespace nkuidesign

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
