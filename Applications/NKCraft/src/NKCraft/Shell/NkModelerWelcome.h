#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkModelerWelcome.h — l'ECRAN D'ACCUEIL (demande de Rihen).
//
// Affiche au lancement TANT QU'AUCUN PROJET N'EST OUVERT, en plein ecran DANS
// la fenetre du modeleur : pas une seconde fenetre, pas un splash. Il disparait
// des qu'un projet est ouvert ou cree.
//
// CE QU'IL MONTRE, DANS L'ORDRE D'IMPORTANCE VOULU
//   1. les PROJETS RECENTS, avec leur image de couverture -- c'est la raison
//      d'etre de l'ecran : neuf ouvertures sur dix reprennent un projet ;
//   2. les deux actions, Nouveau et Ouvrir ;
//   3. les liens externes.
//
// LA MARQUE : l'araignee, logo PROVISOIRE choisi par Rihen le 29/09/2026 en
// attendant le designer (voir NkModelerBrand.h). Elle est dessinee au painter,
// suivie du mot en deux poids, avec les ROLES DE COULEUR du theme, jamais des
// valeurs en dur : un theme clair la repeint toute seule. Le jour ou le logo
// definitif arrive, c'est NkModelerBrand.h qu'on remplace.
//
// ⚠ LES URL N'EXISTENT PAS ENCORE. Elles sont declarees vides ci-dessous, en
// tete de fichier, marquees « a renseigner ». Un lien dont l'URL est vide
// s'affiche GRISE ET INACTIF -- jamais un lien mort : la regle du depot est
// qu'une commande affichee fait ce qu'elle annonce.
// =============================================================================

#include "NKCraft/Shell/NkModelerUI.h"
#include "NKCraft/Shell/NkModelerBrand.h" // la marque : l'araignee (PaintBrandMark)
#include "NKCraft/Shell/NkModelerInput.h"
#include "NKCraft/Shell/NkModelerWidgets.h"
#include "NKCraft/Project/NkModelerProject.h"
#include "NKCraft/Project/NkModelerScene.h"	 // lecteur HERITE (tout dans le .nk3dm)
#include "NKCraft/Project/NkModelerAssets.h" // un fichier par asset (disposition 3)
#include "NKWindow/Core/NkLauncher.h" // ouvre le navigateur du systeme
#include "NKWindow/Core/NkDialogs.h"  // selecteurs natifs DEJA presents dans le depot
#include "NKEditorKit/NkIEditorRenderer.h"
#include "NKEditorKit/NkProjectLauncherHost.h" // (01/10) le lanceur de projets partage
#include "NKCraft/Shell/NkModelerCommon.h"     // NkPickerOuvrirImport / NkPickerOuvrirImage (modeles)
// OU SONT LES DONNEES LIVREES : une seule convention (cf. son en-tete).
#include "NKCraft/NkModelerData.h"
#include "NKCraft/NkCraftMigration.h" // (29/09) le dossier de projets de l'ancien nom
#include "NKImage/NKImage.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"

#include <cstdio>

namespace nkentseu {
	namespace nk3d {

		// ── LIENS EXTERNES — A RENSEIGNER ───────────────────────────────────────
		// Les adresses n'existent pas encore. Tant qu'une chaine est VIDE, son lien
		// est grise et ne repond pas : c'est le seul comportement honnete. Remplir
		// la chaine suffit a l'activer, il n'y a rien d'autre a toucher.
		static const char *const kUrlSite = ""; // A RENSEIGNER — site NKCraft
		static const char *const kUrlTutos = ""; // A RENSEIGNER — tutoriels
		static const char *const kUrlCommu = ""; // A RENSEIGNER — communaute
		static const char *const kUrlDocs = ""; // A RENSEIGNER — documentation

		// ── IMAGE DE VERSION (façon Blender) — decision de Rihen, 5 aout ────────
		// Chaque version du logiciel est accompagnee d'une IMAGE realisee avec
		// lui, creditee a son auteur. Elle ne demande AUCUN serveur : elle est
		// livree dans `data/splash/`, ce qui la distingue de tout le reste du
		// contenu « editorial » (nouveautes, communaute, marketplace) qui, lui,
		// suppose un service en ligne inexistant. C'est le seul contenu de ce
		// genre qu'on puisse honnetement afficher aujourd'hui.
		//
		// Le fichier de credit `data/splash/splash.txt` porte DEUX lignes :
		//   1. le titre de l'oeuvre
		//   2. le nom de son auteur
		// Sans image, la bande ne s'affiche PAS DU TOUT -- pas de cadre vide,
		// pas d'image d'emprunt.
		// ⚠️ CES DEUX CHEMINS SONT RELATIFS ET NE SE LISENT PAS TELS QUELS : ils
		//    passent par `NkDataFile` (NkModelerData.h), qui essaie les trois
		//    racines. Ecrits tels quels ils ne designaient AUCUN fichier possible
		//    -- il n'existe pas de `data/` a la racine de l'arbre, d'ou
		//    l'application se lance. C'etait donc un piege monte : le
		//    `data/splash/LISEZMOI.md` dit a Rodolf de deposer `splash.png` dans
		//    `Applications/NKCraft/data/splash/`, et le code regardait
		//    ailleurs. L'image n'aurait jamais paru, et RIEN ne l'aurait dit --
		//    l'absence d'image etant un etat normal, l'echec se confondait avec
		//    le cas ou il n'y a simplement rien a montrer.
		static const char *const kSplashImage = "data/splash/splash.png";
		static const char *const kSplashCredits = "data/splash/splash.txt";
		static const uint32 kSplashTexId = 4599u; ///< juste sous les couvertures
		// Version affichee. Elle DOIT suivre `appversion(...)` du .jenga : deux
		// numeros differents sur le meme binaire feraient mentir l'un des deux.
		static const char *const kAppVersion = "0.1.0";

		// Plage d'identifiants de texture des VIGNETTES de couverture. Elle suit
		// celle des matcaps (4300+) sans la recouvrir : les identifiants sont
		// attribues a la main dans cette application, une collision passerait
		// inapercue jusqu'a ce qu'une image en remplace une autre.
		static const uint32 kCoverTexBase = 4600u;
		static const int32 kCoverMax = 16; ///< au-dela, la liste defile sans vignette

		// Etat de l'image de version, resolu UNE fois au demarrage (decoder un
		// PNG a chaque image couterait plus cher que tout l'ecran).
		struct NkSplashArt {
				bool tried = false;   ///< tentative faite (succes ou non)
				bool valid = false;   ///< image chargee et publiee
				uint32 w = 0, h = 0;  ///< dimensions natives, pour le rapport
				char title[96] = {};  ///< 1re ligne de splash.txt
				char author[96] = {}; ///< 2e ligne
		};

		// Charge l'image de version et ses credits. Idempotent : la deuxieme
		// tentative ne fait rien, meme si la premiere a echoue -- une image
		// absente est un etat normal, pas une erreur a reessayer chaque frame.
		inline void NkSplashLoad(editorkit::NkIEditorRenderer &renderer, NkSplashArt &art) {
			if (art.tried)
				return;
			art.tried = true;
			// LES TROIS RACINES, par la porte commune. Sans elle, ce chargeur
			// cherchait un fichier a un endroit qui ne peut pas exister.
			const NkString chemin = NkDataFile(kSplashImage);
			if (chemin.Empty()) {
				// ⚠️ ON LE DIT, MEME SI C'EST NORMAL. « Pas d'image livree » et
				//    « image livree au bon endroit mais cherchee au mauvais » sont
				//    le meme silence, et c'est precisement ce silence qui a laisse
				//    ce chemin mort passer. La ligne nomme les racines essayees :
				//    Rodolf depose son PNG, relit le journal, et sait tout de
				//    suite si le produit est alle le chercher la ou il l'a mis.
				// ⚠️ `attendu = true` : SON ABSENCE EST L'ETAT NORMAL, et le
				//    `LISEZMOI.md` de ce dossier le dit. Le fichier n'existe dans
				//    aucune branche : contenu editorial, pas actif oublie. Le
				//    journaliser en AVERTISSEMENT le faisait monter a l'ecran de
				//    tout utilisateur depuis que le puits d'ecran est branche --
				//    Rodolf l'a photographie le 26/09 a 02:12.
				NkDataRefus("image de version (ecran d'accueil)", kSplashImage, /*attendu*/ true);
				return; // pas d'image livree : la bande n'existera pas
			}
			NkImage img;
			if (!img.Load(chemin.CStr(), 4) || !img.IsValid()) {
				NkLog::Instance().Warn(
					"[nk3d-data] image de version TROUVEE mais illisible : {0}\n", chemin.CStr());
				return;
			}
			art.w = (uint32)img.Width();
			art.h = (uint32)img.Height();
			renderer.UploadImageRGBA(kSplashTexId, (const uint8 *)img.Pixels(), (int32)art.w,
									 (int32)art.h);
			art.valid = true;
			// ⚠️ LE SUCCES SE DIT AUSSI, et pas seulement le refus. Sans cette
			//    ligne, « pas de ligne au journal » voudrait dire a la fois
			//    « trouvee » et « jamais tentee » : on ne pourrait pas prouver
			//    qu'une image deposee a ete PRISE, seulement qu'elle n'a pas ete
			//    refusee -- et une absence ne prouve rien. Elle dit le chemin
			//    RETENU, donc laquelle des trois racines a repondu.
			NkLog::Instance().Info("[nk3d-data] image de version chargee : {0} ({1}x{2})\n",
								   chemin.CStr(), art.w, art.h);
			// Credits : deux lignes, facultatives. Une image sans credit
			// s'affiche quand meme -- mais un credit vide ne s'invente pas.
			// Le credit suit la MEME resolution que l'image : il vit a cote
			// d'elle, donc dans la meme racine. Le resoudre autrement ferait
			// afficher une oeuvre avec le credit d'une autre.
			const NkString cheminCredits = NkDataFile(kSplashCredits);
			if (cheminCredits.Empty())
				return; // image sans credit : elle s'affiche quand meme
			const NkString txt = NkFile::ReadAllText(NkPath(cheminCredits.CStr()));
			if (txt.Empty())
				return;
			const char *s = txt.CStr();
			int32 line = 0, k = 0;
			char *dst = art.title;
			for (usize i = 0; s[i] && line < 2; ++i) {
				if (s[i] == '\n' || s[i] == '\r') {
					if (k == 0)
						continue; // saute les fins de ligne consecutives
					dst[k] = 0;
					++line;
					k = 0;
					dst = art.author;
					continue;
				}
				if (k < 94)
					dst[k++] = s[i];
			}
			dst[k] = 0;
		}

		// ── VIGNETTES : CHARGEMENT ──────────────────────────────────────────────
		// Appele par la boucle principale quand la liste change (`texDirty`), pas
		// a chaque image : decoder un PNG par frame couterait plus cher que tout
		// le reste de l'ecran.
		/// Les polices du lanceur, chargees et televersees par le renderer de
		/// l'application (cf. NkWelcomeUploadCovers, qui le recoit chaque image).
		inline editorkit::NkLanceurPolices &NkWelcomePolices() {
			static editorkit::NkLanceurPolices p;
			return p;
		}

		inline void NkWelcomeUploadCovers(editorkit::NkIEditorRenderer &renderer,
										  NkRecentList &rec) {
			// (01/10) LES POLICES DU LANCEUR passent par la meme porte : c'est ici
			// que l'accueil recoit le renderer, a chaque image. Rechargees si
			// l'echelle d'interface change (un ecran a l'autre DPI).
			{
				editorkit::NkLanceurPolices &pol = NkWelcomePolices();
				if (pol.echelle != gUiScale)
					(void)pol.Charger(gUiScale, &renderer);
			}
			if (!rec.texDirty)
				return;
			rec.texDirty = false;
			const int32 n = (int32)rec.items.Size();
			for (int32 i = 0; i < n; ++i) {
				rec.items[(usize)i].tex = 0;
				if (i >= kCoverMax)
					continue;
				const NkString abs = rec.items[(usize)i].CoverAbs();
				if (abs.Empty() || !NkFile::Exists(abs.CStr()))
					continue;
				NkImage img;
				if (!img.Load(abs.CStr(), 4) || !img.IsValid())
					continue;
				// Reduite AVANT le televersement : une couverture 4K occuperait
				// 32 Mo de VRAM pour une vignette de 160 px.
				NkImage small = img.Resize(256, 144);
				// Une reference, pas un pointeur : si le redimensionnement echoue,
				// `small` est invalide et on retombe sur l'originale.
				const NkImage &use = small.IsValid() ? small : img;
				const uint32 tex = kCoverTexBase + (uint32)i;
				if (renderer.UploadImageRGBA(tex, use.Pixels(), use.Width(), use.Height()))
					rec.items[(usize)i].tex = tex;
				// `small` libere ses pixels toute seule en fin d'iteration.
			}
		}

		// La marque (PaintBrandMark) vit dans NkModelerBrand.h : l'accueil et la
		// barre de menus dessinent la meme.

		// ── UN CHAMP DE SAISIE DE BOITE MODALE ──────────────────────────────────
		// Le champ en place de NkModelerWidgets s'ouvre au DOUBLE-clic, ce qui a du
		// sens dans une liste (le simple clic y selectionne) mais pas dans une
		// boite ou il n'y a rien d'autre a faire que taper. Meme brique de saisie
		// (celle de NKEditorKit), simple clic pour entrer dedans.
		inline void WelcomeField(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
								 const nkgui::NkGuiInput &in, const char *key, const NkRect &r,
								 char *buf, uint32 cap) {
			const bool over = hit.Add(key, r);
			if (!ws.IsEditing(key)) {
				p.Outline(r, over ? NkRole::AccentUi : NkRole::Border, NkRole::InputBg, 3.f);
				p.Clip({r.x + S(4.f), r.y, r.w - S(8.f), r.h});
				p.TextV(r.x + S(6.f), r.y, r.h, buf, buf[0] ? NkRole::Text : NkRole::TextMuted);
				p.Unclip();
				if (hit.Clicked(key))
					ws.BeginEdit(key, buf);
				return;
			}
			p.Outline(r, NkRole::AccentUi, NkRole::InputBg, 3.f);
			if (nkgui::NkGuiContext *gc = NkUiCtx()) {
				editorkit::NkOverlayTextField(*gc, gc->dl, p.FontPtr(), r, ws.editBuf,
											  (int32)(cap < 259u ? cap : 259u), true);
				uint32 n = 0;
				while (ws.editBuf[n])
					++n;
				ws.editLen = n;
			}
			// RECOPIE CONTINUE, et c'est voulu : le bouton « Creer » et la ligne
			// « sera cree ici » doivent montrer ce qui est tape MAINTENANT. Attendre
			// une validation ferait un bouton qui agit sur un nom perime.
			{
				uint32 i = 0;
				for (; ws.editBuf[i] && i + 1u < cap; ++i)
					buf[i] = ws.editBuf[i];
				buf[i] = 0;
			}
			if (in.KeyPressed(nkgui::NkGuiKey::Enter) || in.KeyPressed(nkgui::NkGuiKey::Escape))
				ws.EndEdit();
			else if (hit.AnyClick() && !hit.IsHovered(key))
				ws.EndEdit();
		}

		// Bouton plein / bouton contour, la paire de l'ecran. Renvoie true au clic.
		inline bool WelcomeButton(NkModelerPainter &p, NkHitRegistry &hit, const char *key,
								  const NkRect &r, const char *label, NkIcon ic, bool primary,
								  bool enabled = true) {
			const bool over = enabled && hit.Add(key, r);
			if (!enabled) {
				// GRISE ET INACTIF : la zone n'est meme pas declaree, donc il n'y a
				// rien a cliquer -- pas seulement rien qui se passe.
				p.Outline(r, NkRole::Border, NkRole::PanelBg, 4.f);
				if (ic != NkIcon::None)
					p.IconV(r.x + S(12.f), r.y, r.h, ic, NkRole::TextMuted, 14.f);
				p.TextV(r.x + (ic != NkIcon::None ? S(34.f) : S(14.f)), r.y, r.h, label,
						NkRole::TextMuted);
				return false;
			}
			if (primary)
				p.Fill(r, over ? NkRole::AccentSel : NkRole::AccentUi, 4.f);
			else
				p.Outline(r, over ? NkRole::AccentUi : NkRole::Border,
						  over ? NkRole::PanelHeader : NkRole::PanelBg, 4.f);
			const NkRole txt = primary ? NkRole::TextOnAccent : NkRole::Text;
			if (ic != NkIcon::None)
				p.IconV(r.x + S(12.f), r.y, r.h, ic, txt, 14.f);
			p.TextV(r.x + (ic != NkIcon::None ? S(34.f) : S(14.f)), r.y, r.h, label, txt);
			if (over)
				hit.WantCursor(NkCursorWant::Hand);
			return hit.Clicked(key);
		}

		/// Les modeles de nouveau projet de NKCraft. ⚠️ CHAQUE CARTE DISPONIBLE FAIT
		/// CE QU'ELLE ANNONCE : elle cree le projet par la boite habituelle, puis
		/// ouvre ce qu'elle promet (selecteur d'import, generateur, assistant) --
		/// par les portes que l'application a deja (pickerAction 2 et 3, aiOuvert).
		/// Ce qui n'est pas encore branche se montre « a venir ».
		inline const NkVector<editorkit::NkLanceurModele> &NkWelcomeModeles() {
			static NkVector<editorkit::NkLanceurModele> v;
			if (v.Empty()) {
				using G = editorkit::NkLanceurGlyphe;
				auto ajoute = [&](const char *nom, const char *cat, const char *desc, G g, uint32 teinte, bool dispo) {
					editorkit::NkLanceurModele m;
					m.nom = NkString(nom);
					m.categorie = NkString(cat);
					m.description = NkString(desc);
					m.glyphe = g;
					m.couleur = teinte;
					m.disponible = dispo;
					v.PushBack(m);
				};
				ajoute("Scene de depart", "MODELISATION",
					   "Sol, lumieres et camera deja en place : on ajoute ses objets et on modele.", G::Cube,
					   0u, true);
				ajoute("Importer un modele", "IMPORT",
					   "Cree le projet puis ouvre le selecteur : glTF, OBJ, FBX... arrivent editables.",
					   G::Ouvrir, math::NkColor(0x2E, 0xA0, 0x6B).ToUint32A(), true);
				ajoute("Image vers objet 3D", "GENERATION",
					   "Cree le projet puis demande une image : le generateur en tire un objet editable.",
					   G::Pinceau, math::NkColor(0xB0, 0x5C, 0xE0).ToUint32A(), true);
				ajoute("Avec l'assistant IA", "ASSISTANT",
					   "Cree le projet et ouvre l'assistant : decrivez ce que vous voulez modeler.", G::Etoile,
					   math::NkColor(0xF2, 0x98, 0x0E).ToUint32A(), true);
				ajoute("Creature", "PERSONNAGE",
					   "Une creature articulee generee par l'outil de creatures de NKCraft.", G::Os,
					   math::NkColor(0xD9, 0x4F, 0x4F).ToUint32A(), false);
			}
			return v;
		}

		// ── BOITE « NOUVEAU PROJET » ────────────────────────────────────────────
		// Un projet = un DOSSIER + un .nk3dm a sa racine : creer demande donc un
		// emplacement ET un nom. Le chemin exact qui sera cree est affiche en
		// toutes lettres -- sans quoi personne ne sait ou son travail atterrit.
		inline void PaintNewProjectDialog(NkModelerPainter &p, float32 W, float32 H,
										  NkModelerState &st, NkHitRegistry &hit,
										  NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			if (!st.newProjOpen)
				return;
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150});
			hit.Add("np.veil", {0.f, 0.f, W, H});

			const float32 bw = S(560.f), bh = S(260.f);
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 6.f);
			hit.Add("np.box", box);
			p.TextV(box.x + S(20.f), box.y + S(12.f), S(24.f), "Nouveau projet");
			// (01/10) Le modele choisi sur le lanceur, dit dans la boite : on sait
			// ce que « Creer » va faire en plus du dossier et du .nk3dm.
			if (st.newProjModele > 0 && (usize)st.newProjModele < NkWelcomeModeles().Size()) {
				char lm[120];
				snprintf(lm, sizeof(lm), "Modele : %s", NkWelcomeModeles()[(usize)st.newProjModele].nom.CStr());
				p.TextV(box.x + bw - S(20.f) - p.TextW(lm), box.y + S(12.f), S(24.f), lm, NkRole::TextMuted);
			}

			const float32 lx = box.x + S(20.f), fx = box.x + S(120.f);
			const float32 fw = bw - S(140.f) - S(20.f);
			float32 y = box.y + S(52.f);

			p.TextV(lx, y, S(26.f), "Nom", NkRole::TextMuted);
			WelcomeField(p, hit, ws, in, "np.name", {fx, y, fw, S(26.f)}, st.newProjName,
						 (uint32)sizeof(st.newProjName));
			y += S(36.f);

			p.TextV(lx, y, S(26.f), "Emplacement", NkRole::TextMuted);
			const float32 browW = S(96.f);
			WelcomeField(p, hit, ws, in, "np.dir", {fx, y, fw - browW - S(8.f), S(26.f)},
						 st.newProjDir, (uint32)sizeof(st.newProjDir));
			if (WelcomeButton(p, hit, "np.browse", {fx + fw - browW, y, browW, S(26.f)},
							  "Parcourir", NkIcon::Folder, false))
				st.projPending = 5; // selecteur de dossier : apres la frame
			y += S(40.f);

			// LE CHEMIN REEL, ecrit noir sur blanc. C'est la seule facon de ne pas
			// se tromper de dossier -- et ca rend visible la regle « un projet est
			// un dossier », au lieu de la faire deviner.
			char full[512];
			snprintf(full, sizeof(full), "%s/%s/%s.nk3dm", st.newProjDir, st.newProjName,
					 st.newProjName);
			p.TextV(lx, y, S(20.f), "Sera cree :", NkRole::TextMuted);
			p.Clip({lx, y + S(20.f), bw - S(40.f), S(20.f)});
			p.TextV(lx, y + S(20.f), S(20.f), full, NkRole::Text);
			p.Unclip();

			if (st.projError[0]) {
				p.Clip({lx, box.y + bh - S(78.f), bw - S(40.f), S(20.f)});
				p.TextV(lx, box.y + bh - S(78.f), S(20.f), st.projError, NkRole::AccentSel);
				p.Unclip();
			}

			// ── UN NOM DE PROJET EST UN NOM DE DOSSIER ──────────────────────
			// Ni espace, ni caractere interdit par le systeme de fichiers : le
			// projet portera ce nom sur le disque (Rihen, 13 aout). On le dit
			// PENDANT la saisie, et le bouton reste eteint -- plutot que d'echouer
			// a la creation, quand l'utilisateur croit son projet fait.
			const char *nomErr = nullptr;
			for (const char *q = st.newProjName; *q && !nomErr; ++q) {
				const char c = *q;
				if (c == ' ')
					nomErr = "Le nom ne peut pas contenir d'espace";
				else if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
						 c == '"' || c == '<' || c == '>' || c == '|' || (unsigned char)c < 32u)
					nomErr = "Caractere interdit dans un nom de dossier";
			}
			if (!nomErr && st.newProjName[0]) {
				const char *fin = st.newProjName;
				while (*fin)
					++fin;
				if (fin[-1] == '.')
					nomErr = "Le nom ne peut pas finir par un point";
			}
			if (nomErr)
				p.TextV(fx, box.y + bh - S(72.f), kRowH, nomErr, NkRole::AccentSel);

			const float32 by = box.y + bh - S(46.f);
			const bool canCreate =
				st.newProjName[0] != 0 && st.newProjDir[0] != 0 && nomErr == nullptr;
			if (WelcomeButton(p, hit, "np.cancel", {box.x + bw - S(240.f), by, S(100.f), S(30.f)},
							  "Annuler", NkIcon::None, false)) {
				st.newProjOpen = false;
				st.projError[0] = 0;
				ws.EndEdit();
			}
			if (WelcomeButton(p, hit, "np.create", {box.x + bw - S(130.f), by, S(110.f), S(30.f)},
							  "Creer", NkIcon::None, true, canCreate)) {
				ws.EndEdit();
				st.projPending = 6; // creation reelle : apres la frame, comme le reste
			}
		}

		// ── L'ECRAN : LE LANCEUR DE PROJETS PARTAGE (2026-10-01) ────────────────
		// Rihen : l'ancien accueil (deux colonnes, boutons empiles, petites
		// vignettes) « ne plait pas » ; il le veut BEAU, facon Unreal Engine 5 /
		// Unity Hub. Il est donc devenu le COMPOSANT du kit
		// (NKEditorKit/Components/NkProjectLauncherModel.h), le meme pour toute la
		// famille ; NKCraft n'y met que SA TOUCHE : l'araignee, ses modeles, ses
		// pages, son extension .nk3dm. Tout ce que faisait l'ancien ecran est garde
		// et passe par les MEMES portes (projPending 1, 2, 7 ; TogglePin, Remove ;
		// la purge des projets morts ; les liens « a venir » ; l'image de version ;
		// les astuces ; la barre de titre dessinee de la fenetre sans cadre).

		/// Ce que promet le modele choisi, fait APRES la creation reussie du projet
		/// (action 6) -- par les portes de l'application, jamais un second chemin.
		inline void NkWelcomeAppliquerModele(NkModelerState &st) {
			const int32 i = st.newProjModele;
			st.newProjModele = 0;
			switch (i) {
				case 1: // Importer un modele : le selecteur d'import du navigateur
					NkPickerOuvrirImport(st);
					st.pickerAction = 2;
					break;
				case 2: // Image vers objet 3D : le meme geste que « Generer » (GENIA)
					NkPickerOuvrirImage(st);
					st.pickerAction = 3;
					break;
				case 3: // Avec l'assistant IA : le panneau deploye
					st.aiOuvert = true;
					break;
				default:
					break;
			}
			if (i > 0 && (usize)i < NkWelcomeModeles().Size()) {
				std::printf("[nk3d] nouveau projet : modele « %s » applique\n", NkWelcomeModeles()[(usize)i].nom.CStr());
				std::fflush(stdout);
			}
		}

		/// Le MODELE du lanceur : son ETAT (page, recherche, vue, defilement) dure
		/// d'une image a l'autre ; ses DONNEES sont rafraichies a chaque image
		/// depuis la liste des recents -- une seule verite, celle du fichier.
		inline editorkit::NkProjectLauncherModel &NkWelcomeLanceur() {
			static editorkit::NkProjectLauncherModel m;
			static bool pret = false;
			if (!pret) {
				pret = true;
				using G = editorkit::NkLanceurGlyphe;
				m.identite.nom = NkString("NKCraft");
				m.identite.prefixe = NkString("NK");
				m.identite.sousTitre = NkString("Modelisation 3D — Nkentseu");
				m.identite.version = NkString(kAppVersion);
				m.identite.extensions = NkString(".nk3dm");
				m.identite.glyphe = G::Cube;
				m.chromeFenetre = true;
				m.themeBasculable = true;
				m.modeles = NkWelcomeModeles();
				auto page = [&](const char *lib, const char *titre, const char *st, G g, editorkit::NkLanceurPageType t) {
					editorkit::NkLanceurPage p;
					p.libelle = NkString(lib);
					p.titre = NkString(titre);
					p.sousTitre = NkString(st);
					p.glyphe = g;
					p.type = t;
					m.pages.PushBack(p);
					return (usize)(m.pages.Size() - 1u);
				};
				auto lien = [&](usize pg, const char *titre, const char *desc, const char *url, G g) {
					editorkit::NkLanceurLien l;
					l.titre = NkString(titre);
					l.description = NkString(desc);
					l.url = NkString(url ? url : "");
					l.glyphe = g;
					// ⚠️ UNE URL VIDE EST « A VENIR », jamais un lien mort (regle de
					//    l'ancien ecran, gardee) : remplir kUrl* suffit a l'activer.
					l.disponible = url && *url;
					l.badge = NkString(l.disponible ? "" : "a venir");
					m.pages[pg].liens.PushBack(l);
				};
				(void)page("Projets", "Projets", "", G::Projets, editorkit::NkLanceurPageType::Projets);
				const usize pa = page("Apprendre", "Apprendre", "Tutoriels et documentation de NKCraft.", G::Apprendre,
									  editorkit::NkLanceurPageType::Liens);
				lien(pa, "Tutoriels", "Modeler, texturer, eclairer : les gestes de base pas a pas.", kUrlTutos,
					 G::Apprendre);
				lien(pa, "Documentation", "Toutes les commandes, les raccourcis et le format .nk3dm.", kUrlDocs,
					 G::Document);
				const usize pc = page("Communaute", "Communaute", "Partager ses modeles, poser ses questions.",
									  G::Communaute, editorkit::NkLanceurPageType::Liens);
				lien(pc, "Communaute", "Le forum des utilisateurs de NKCraft et de Nkentseu.", kUrlCommu, G::Communaute);
				lien(pc, "Site officiel", "Les nouveautes, les versions et les galeries.", kUrlSite, G::Lien);
				const usize pi = page("Installations", "Installations", "Ce qui est installe sur cette machine.",
									  G::Installations, editorkit::NkLanceurPageType::Liens);
				{
					editorkit::NkLanceurLien l;
					l.titre = NkString("NKCraft ") + NkString(kAppVersion);
					l.description = NkString("Installe dans ") + NkPath::GetExecutableDirectory().ToString();
					l.glyphe = G::Cube;
					l.badge = NkString("installee");
					l.disponible = true;
					m.pages[pi].liens.PushBack(l);
					editorkit::NkLanceurLien u;
					u.titre = NkString("Mises a jour");
					u.description = NkString("La recherche de nouvelles versions de NKCraft.");
					u.glyphe = G::Installations;
					u.disponible = false;
					u.badge = NkString("a venir");
					m.pages[pi].liens.PushBack(u);
				}
			}
			return m;
		}

		/// Le logo du lanceur : l'araignee, par le peintre du kit.
		inline void NkWelcomeLogo(void *, editorkit::NkComponentPaint &p, const editorkit::NkPaintRect &r) {
			PaintBrandMarkKit(p, r, (uint16)NkRole::PanelBg);
		}

		inline void PaintWelcome(NkModelerPainter &p, float32 W, float32 H, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in,
								 NkRecentList &rec, const NkSplashArt &art) {
			if (!st.welcome)
				return;
			nkgui::NkGuiContext *gc = NkUiCtx();
			editorkit::NkLanceurPolices &pol = NkWelcomePolices();
			// Le lanceur PREND toute la fenetre : rien de l'application dessous ne
			// recoit un clic (meme role que l'ancien « wel.bg »).
			hit.Add("wel.bg", {0.f, 0.f, W, H});
			if (!gc || !pol.Pretes()) {
				p.Fill({0.f, 0.f, W, H}, NkRole::WindowBg);
				return;
			}

			editorkit::NkProjectLauncherModel &m = NkWelcomeLanceur();
			m.fenetreMaximisee = st.maximized;
			m.themeSombre = p.Theme().IsDark();

			// ── Les recents -> les cartes. L'etat du fichier (introuvable / vide)
			//    est mesure UNE fois par entree, comme avant (etatFichier). ──
			m.projets.Clear();
			for (usize i = 0; i < rec.items.Size(); ++i) {
				NkRecentEntry &e = rec.items[i];
				if (e.etatFichier == 0) {
					const NkString dossier = NkPath(e.path.CStr()).GetParent().ToString();
					const bool absent = !NkFile::Exists(e.path.CStr());
					bool vide = false;
					if (!absent) {
						const NkVector<NkString> sc =
							NkDirectory::GetFiles(dossier.CStr(), "*.nkscene", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
						vide = sc.Empty();
					}
					e.etatFichier = absent ? 2u : (vide ? 3u : 1u);
				}
				editorkit::NkLanceurProjet pr;
				pr.nom = e.name;
				pr.chemin = e.path;
				pr.date = e.date;
				pr.epingle = e.pinned;
				pr.image = e.tex;
				pr.imageW = e.tex ? 256 : 0; // les couvertures sont televersees en 256 x 144
				pr.imageH = e.tex ? 144 : 0;
				pr.etat = e.etatFichier == 2u ? 1u : (e.etatFichier == 3u ? 2u : 0u);
				if (pr.etat == 2u)
					pr.etatTexte = NkString("vide : aucune scene");
				pr.hote = (uint32)i;
				m.projets.PushBack(pr);
			}

			// ── L'image de version, ses credits, le pied de page. ──
			m.banniere = editorkit::NkLanceurBanniere();
			if (art.valid) {
				m.banniere.image = kSplashTexId;
				m.banniere.w = (int32)art.w;
				m.banniere.h = (int32)art.h;
				m.banniere.legende = NkString("NKCraft ") + NkString(kAppVersion);
				if (art.title[0] && art.author[0])
					m.banniere.credit = NkString(art.title) + NkString(" — ") + NkString(art.author);
				else if (art.title[0] || art.author[0])
					m.banniere.credit = NkString(art.title[0] ? art.title : art.author);
			}
			static const char *const kTips[6] = {
				"Astuce : Tab bascule entre le mode Objet et le mode Edition.",
				"Astuce : Ctrl pendant un deplacement inverse l'aimantation.",
				"Astuce : la molette sur un champ numerique l'ajuste finement.",
				"Astuce : G, R et S transforment tout de suite, sans changer d'outil.",
				"Astuce : Maj+D duplique ; Ctrl+C / Ctrl+V copie entre les scenes.",
				"Astuce : le point-virgule ouvre les cibles d'aimantation."};
			m.astuce = NkString(kTips[rec.items.Size() % 6u]);
			m.piedDePage = NkString("Enregistre : objets, geometrie (sommets et faces), materiaux, lumieres, cameras, "
									"scenes. Pas encore : modificateurs.");
			// L'erreur d'une action projet reste affichee jusqu'a la prochaine
			// action : une erreur qui disparait toute seule n'a servi a personne.
			m.erreur = (!st.newProjOpen && st.projError[0]) ? NkString(st.projError) : NkString();

			editorkit::NkProjectLauncherHooks hk;
			hk.peindreLogo = &NkWelcomeLogo;
			const editorkit::NkProjectLauncherResult res =
				editorkit::NkLanceurPeindre(*gc, p.Theme(), pol, editorkit::NkPaintRect{0.f, 0.f, W, H}, m,
											editorkit::NkProjectLauncherStyle(), hk, gUiScale, !st.newProjOpen);
			if (res.curseurMain)
				hit.WantCursor(NkCursorWant::Hand);
			if (std::getenv("NK_ACCUEIL_SONDE")) {
				static bool sDit = false;
				if (!sDit) {
					sDit = true;
					std::printf("[nk3d] ACCUEIL lanceur W=%.0f H=%.0f projets=%d visibles=%d contenu=%.0f page=%d\n",
								(double)W, (double)H, (int)m.projets.Size(), (int)res.projetsVisibles,
								(double)res.contenuH, (int)m.page);
					std::fflush(stdout);
				}
			}

			// ── CE QUE L'UTILISATEUR A DEMANDE -> les portes de toujours. ──
			using A = editorkit::NkLanceurAction;
			const int32 hote = (res.index >= 0 && (usize)res.index < m.projets.Size())
								   ? (int32)m.projets[(usize)res.index].hote
								   : -1;
			switch (res.action) {
				case A::NouveauProjet:
					st.newProjModele = 0;
					st.projPending = 1; // la boite « Nouveau projet »
					break;
				case A::NouveauDepuisModele:
					st.newProjModele = res.index;
					st.projPending = 1;
					break;
				case A::Ouvrir:
					st.projPending = 2;
					break;
				case A::OuvrirRecent:
					if (hote >= 0) {
						st.projRecent = hote;
						st.projPending = 7;
					}
					break;
				case A::Epingler:
					if (hote >= 0)
						rec.TogglePin((usize)hote);
					break;
				case A::Retirer:
					if (hote >= 0)
						rec.Remove((usize)hote);
					break;
				case A::Purger: {
					int32 retires = 0;
					for (isize k = (isize)rec.items.Size() - 1; k >= 0; --k)
						if (rec.items[(usize)k].etatFichier == 2u || rec.items[(usize)k].etatFichier == 3u) {
							rec.Remove((usize)k);
							++retires;
						}
					std::printf("[nk3d] ACCUEIL %d projet(s) vide(s) ou introuvable(s) retire(s) de la liste des "
								"recents (le disque n'est pas touche)\n",
								(int)retires);
					std::fflush(stdout);
					break;
				}
				case A::OuvrirLien:
					if (res.url && *res.url)
						NkLauncher::OpenURL(res.url);
					break;
				case A::BasculerTheme:
					st.themeBascule = true;
					break;
				case A::FenetreReduire:
					st.wantMinimize = true;
					break;
				case A::FenetreAgrandir:
					st.wantMaxRestore = true;
					break;
				case A::FenetreFermer:
					st.running = false;
					break;
				case A::FenetreGlisser:
					st.wantDragMove = true;
					st.dragFracX = W > 1.f ? (in.mousePos.x / W) : 0.5f;
					break;
				default:
					break;
			}

			PaintNewProjectDialog(p, W, H, st, hit, ws, in);
		}

		// ── EXECUTION DES ACTIONS PROJET, APRES LA FRAME ────────────────────────
		// Appelee par la boucle principale une fois la peinture terminee. C'est le
		// SEUL endroit ou l'on ouvre un selecteur de fichiers : ces dialogues
		// entrent dans une boucle modale de l'OS, et les appeler depuis la peinture
		// reentrerait dans la frame en cours (meme piege que BeginDragMove).
		//
		// Les selecteurs viennent de NKWindow (`NkDialogs`) et non d'un Win32 ecrit
		// ici : le depot en a deja un, portable (Win32, zenity, osascript, stubs).
		inline void NkProjectHandlePending(NkModelerState &st, NkProjectState &proj,
										   NkRecentList &rec) {
			// ── SUPPRESSIONS COTE DISQUE ────────────────────────────────────
			// Supprimer une carte doit retirer SON fichier (demande de Rihen).
			// C'est fait ICI, apres la frame : c'est le seul endroit ou la racine
			// du projet est connue, et on ne touche pas au disque en peignant.
			//
			// CORBEILLE D'ABORD, effacement seulement si elle n'est pas disponible.
			// Un fichier detruit sans retour serait la pire des surprises -- c'est
			// la meme regle que « fermer un onglet ne supprime rien ».
			if (st.delPendCount > 0) {
				if (proj.open && !proj.root.Empty()) {
					for (int32 i = 0; i < st.delPendCount; ++i) {
						const NkString rel(st.delPendFile[i]);
						if (rel.Empty())
							continue;
						const bool isDir = rel[rel.Size() - 1u] == '/';
						const NkString abs = NkScToAbs(proj.root, rel.CStr());
						if (isDir) {
							// Un dossier ne part que s'il est VIDE : un fichier
							// qu'on n'a pas mis la n'a pas a disparaitre avec lui.
							if (NkDirectory::Exists(abs.CStr()) &&
								!NkDirectory::MoveToTrash(abs.CStr()))
								(void)NkDirectory::Delete(abs.CStr(), false);
						} else if (NkFile::Exists(abs.CStr()) &&
								   !NkFile::MoveToTrash(abs.CStr())) {
							(void)NkFile::Delete(abs.CStr());
						}
					}
				}
				// La file se vide dans TOUS les cas : la garder ferait retenter la
				// suppression a chaque frame, y compris sans projet ouvert.
				st.delPendCount = 0;
			}

			// ── BASCULE D'ONGLET : reglages Rendu PAR SCENE (Rihen, 10 aout).
			// Requete posee par NkActivateTab, consommee ICI en differe : au
			// moment ou l'on capture, l'etat vivant est encore celui du
			// document quitte (il est global a la vue, la bascule de scene ne
			// l'a pas touche) ; on applique ensuite l'instantane de l'active.
			// Un instantane VIDE n'applique rien : le document herite des
			// reglages courants, puis les possede a la premiere bascule.
			if (st.renduSwitchFrom >= 0 || st.renduSwitchTo >= 0) {
				if (st.renduSwitchFrom >= 0 && st.renduSwitchFrom < 32) {
					NkArchive snap;
					NkAsRenduCapture(snap, proj.root);
					st.docRendu[st.renduSwitchFrom] = snap;
				}
				if (st.renduSwitchTo >= 0 && st.renduSwitchTo < 32 &&
					st.renduSwitchTo != st.renduSwitchFrom)
					NkAsRenduRestore(st.docRendu[st.renduSwitchTo], proj.root, st);
				st.renduSwitchFrom = -1;
				st.renduSwitchTo = -1;
			}

			const int32 action = st.projPending;
			if (action == 0)
				return;
			st.projPending = 0;

			auto put = [](char *dst, uint32 cap, const char *src) {
				uint32 i = 0;
				for (; src && src[i] && i + 1u < cap; ++i)
					dst[i] = src[i];
				dst[i] = 0;
			};
			// Une erreur se dit LA OU L'ON REGARDE : dans la boite « Nouveau » si
			// elle est ouverte, sinon dans une boite systeme -- jamais seulement
			// dans le journal, que personne ne lit au moment ou ca rate.
			auto fail = [&](const NkString &e) {
				put(st.projError, (uint32)sizeof(st.projError), e.CStr());
				if (!st.newProjOpen)
					NkDialogs::OpenMessageBox(e.Empty() ? NkString("action impossible") : e,
											  "NKCraft", 2);
			};
			auto opened = [&]() {
				rec.Touch(proj);
				st.welcome = false;
				st.newProjOpen = false;
				st.projError[0] = 0;
				// Un projet qu'on vient d'ouvrir ou de creer n'a rien de modifie.
				NkClearDirty(st);
			};

			// ── OUVRIR UN PROJET : DEUX DISPOSITIONS, UN SEUL POINT DE PASSAGE ──
			// Disposition 3 : un fichier par asset, le .nk3dm ne porte que l'arbre.
			// Dispositions 1 et 2 : tout etait dans le .nk3dm. On les RELIT encore
			// -- le projet de quelqu'un ne devient pas illisible parce que la
			// disposition a change -- et le prochain enregistrement l'ecrit en
			// fichiers separes. C'est une migration, pas une rupture.
			auto restore = [&](const NkArchive &sc, NkString &e) -> bool {
				bool ok;
				if (NkScInt(sc, "disposition", 0) >= 3) {
					ok = NkProjectTreeRestore(sc, proj.root, st, &e);
				} else {
					ok = NkSceneRestore(sc, proj.root, st, &e);
					if (ok)
						NkBrowserSyncScenes(st); // l'ancien format ne portait pas les cartes
				}
				// LE DISQUE FAIT FOI SUR CE QUI EXISTE : l'arbre relu dit l'ordre,
				// le balayage dit ce qui est reellement la. Un fichier ajoute ou
				// efface pendant que l'application etait fermee est donc pris en
				// compte a l'ouverture, sans attendre le surveillant.
				if (ok)
					(void)NkProjectRescan(proj.root, st);
				return ok;
			};

			NkString err;
			switch (action) {
				case 1: // ouvrir la boite « Nouveau projet »
					if (!st.newProjDir[0]) {
						const NkString home = NkDirectory::GetHomeDirectory().ToString();
						// (29/09) NK3DModeler S'APPELLE NKCRAFT. Le dossier PROPOSE
						// reste `~/NK3DModeler` pour qui y range deja ses projets :
						// un nouveau projet n'atterrit pas a cote des autres sans
						// qu'on l'ait demande. Ce n'est qu'une proposition, rien
						// n'est cree ni deplace ici.
						put(st.newProjDir, (uint32)sizeof(st.newProjDir),
							NkCraftDossierOuAncien(home + "/NKCraft", home + "/NK3DModeler").CStr());
					}
					if (!st.newProjName[0])
						put(st.newProjName, (uint32)sizeof(st.newProjName), "MonProjet");
					st.projError[0] = 0;
					st.newProjOpen = true;
					break;

				case 5: { // « Parcourir » : le dossier PARENT du futur projet
					const NkDialogResult r =
						NkDialogs::OpenFolderDialog("Emplacement du nouveau projet");
					if (r.confirmed && !r.path.Empty())
						put(st.newProjDir, (uint32)sizeof(st.newProjDir), r.path.CStr());
					break;
				}

				case 6: // creation reelle
					if (NkProjectCreate(st.newProjDir, st.newProjName, proj, &err)) {
						opened();
						NkWelcomeAppliquerModele(st); // (01/10) le modele choisi sur le lanceur
					} else
						fail(err);
					break;

				case 2: { // Ouvrir...
					// FILTRE « TOUS LES FICHIERS », a regret et pour une raison
					// precise : `Win32PrepareFilter` (NKWindow/NkDialogs.cpp)
					// construit sa chaine avec `result += ")\0"`, or un littoral
					// C s'arrete au premier zero -- les separateurs nuls que
					// OPENFILENAME attend ne sont jamais ecrits, et le motif
					// arrive VIDE. Passer un filtre reel ferait donc un
					// selecteur qui ne montre rien. La nature est dite dans le
					// titre, et NkProjectLoad refuse proprement un fichier qui
					// n'est pas un projet. (Bug NKWindow a corriger a part.)
					const NkDialogResult r = NkDialogs::OpenFileDialog(
						"*.*", "Ouvrir un projet NKCraft (.nk3dm)");
					if (!r.confirmed || r.path.Empty())
						break;
					NkArchive sc;
					if (NkProjectLoad(r.path.CStr(), proj, &err, &sc)) {
						// La scene est restituee APRES le chargement : `proj.root`
						// doit deja etre a jour, c'est elle qui resout les chemins
						// relatifs. Une scene absente n'est PAS une erreur (projet
						// vide, ou ecrit par une version qui ne la sauvait pas).
						if (!restore(sc, err) && !err.Empty())
							fail(err);
						else {
							opened();
							// UN CHARGEMENT PARTIEL SE DIT QUAND MEME. Il reussit --
							// donc pas de boite d'erreur -- mais taire « 2 textures
							// introuvables » ou « 1 scene orpheline recuperee »
							// laisserait l'utilisateur decouvrir l'ecart tout seul,
							// bien plus tard.
							if (!err.Empty())
								put(st.projError, (uint32)sizeof(st.projError), err.CStr());
						}
					} else
						fail(err);
					break;
				}

				case 7: { // une carte de l'ecran d'accueil
					if (st.projRecent >= 0 && (usize)st.projRecent < rec.items.Size()) {
						const NkString path = rec.items[(usize)st.projRecent].path;
						NkArchive sc;
						if (NkProjectLoad(path.CStr(), proj, &err, &sc)) {
							if (!restore(sc, err) && !err.Empty())
								fail(err);
							else {
								opened();
								if (!err.Empty())
									put(st.projError, (uint32)sizeof(st.projError), err.CStr());
							}
						} else
							// On NE RETIRE PAS l'entree : un disque externe
							// debranche n'est pas un projet supprime, et la
							// retirer d'office ferait perdre l'epingle.
							fail(err);
					}
					st.projRecent = -1;
					break;
				}

				case 9: { // un projet DESIGNE PAR SON CHEMIN (NK_PROJECT)
					// LE MEME POINT DE PASSAGE QUE LE DOUBLE-CLIC : NkProjectLoad,
					// puis `restore`, puis `opened`. Ce qui change n'est pas le
					// chemin de code, c'est la maniere de DESIGNER le projet -- un
					// chemin au lieu d'un rang dans la liste des recents.
					//
					// ET IL LE CREE S'IL N'EXISTE PAS. Sans cela, mesurer un
					// aller-retour de persistance exigerait de cliquer « Nouveau »
					// a la main avant chaque mesure, ce qui rendrait la mesure non
					// rejouable. La creation passe par NkProjectCreate, exactement
					// comme le bouton (action 6) : aucun second createur.
					const NkString path(st.projOpenPath);
					st.projOpenPath[0] = 0;
					if (path.Empty())
						break;
					if (!NkFile::Exists(path.CStr())) {
						// Dossier parent et nom se deduisent du chemin demande :
						// NkProjectCreate fabrique `<parent>/<nom>/<nom>.nk3dm`, on
						// lui donne donc le grand-parent du fichier et son nom nu.
						NkString norm = NkScNorm(path.CStr());
						NkString::SizeType s = norm.RFind('/');
						NkString dir = (s == NkString::npos) ? NkString(".")
															 : NkString(norm.CStr(), s);
						NkString base = (s == NkString::npos)
											? norm
											: NkString(norm.CStr() + s + 1,
													   (NkString::SizeType)(norm.Size() - s - 1));
						const NkString::SizeType d = base.RFind('.');
						if (d != NkString::npos)
							base = NkString(base.CStr(), d);
						// `<parent>/<nom>/<nom>.nk3dm` : le dossier du projet porte
						// deja le nom, il faut donc remonter d'un cran.
						const NkString::SizeType s2 = dir.RFind('/');
						const NkString grand =
							(s2 == NkString::npos) ? NkString(".") : NkString(dir.CStr(), s2);

						// ── LE CHEMIN DOIT DEJA ETRE CONFORME, SINON ON REFUSE ────────
						// ⚠️ CE SILENCE A COUTE UNE NUIT DE FAUX DIAGNOSTIC (20/09).
						//    `NkProjectCreate` prend un PARENT et un NOM, jamais un chemin de
						//    fichier : un projet vit dans un dossier a son nom. La convention
						//    est assumee, et remonter d'un cran la respecte.
						//    Mais quand le chemin recu n'est PAS deja de cette forme, le
						//    segment de dossier etait JETE et remplace par le nom du fichier :
						//    on demandait `mesures/p512.nk3dm`, on obtenait `p512/p512.nk3dm`.
						//    Le projet existait -- ailleurs. Rouvrir le chemin DEMANDE tombait
						//    sur un dossier vide, et l'application attendait sans rien dire.
						//    J'en ai conclu qu'un gros maillage bloquait le modeleur. C'etait
						//    faux, et rien ne me contredisait.
						//
						// ⚠️ ON NE CORRIGE NI LA CONVENTION NI LA FORME DE L'ENTREE. Un chemin
						//    deja conforme se comporte EXACTEMENT comme avant -- les scripts
						//    existants ne changent pas d'un octet. Seul le cas qui etait
						//    reecrit en silence devient un REFUS NOMME.
						//    Des deux facons de manquer, crier ou effacer, celle qui efface est
						//    la pire : un refus aurait coute trente secondes.
						{
							const NkString dossier =
								(s2 == NkString::npos)
									? dir
									: NkString(dir.CStr() + s2 + 1,
										   (NkString::SizeType)(dir.Size() - s2 - 1));
							if (dossier != base) {
								NkString m = "chemin de projet non conforme. Attendu "
									  "<parent>/<nom>/<nom>.nk3dm (un projet vit dans un "
									  "dossier a son nom) ; recu un dossier \"";
								m += dossier;
								m += "\" pour le projet \"";
								m += base;
								m += "\". Le projet aurait ete cree dans \"";
								m += base;
								m += "/\", pas dans le dossier demande.";
							// ⚠️ LE REFUS DOIT SE LIRE. `fail` le pose dans l'etat de l'accueil,
							//    donc a l'ECRAN -- invisible pour un script ou un agent, qui ne
							//    verrait qu'un projet manquant. Les deux autres issues de ce
							//    crochet s'impriment (« projet CREE », « projet OUVERT ») : un
							//    refus muet a cote de deux succes bavards se lit comme un
							//    silence, et c'est ce silence qu'on corrige ici.
							std::printf("[nk3d] NK_PROJECT REFUSE : %s\n", m.CStr());
							fail(m);
							break;
						}
						}
						if (!NkProjectCreate(grand.CStr(), base.CStr(), proj, &err)) {
							fail(err);
							break;
						}
						opened();
						std::printf("[nk3d] NK_PROJECT : projet CREE -> %s\n",
									proj.file.CStr());
						break;
					}
					NkArchive sc;
					if (NkProjectLoad(path.CStr(), proj, &err, &sc)) {
						if (!restore(sc, err) && !err.Empty())
							fail(err);
						else {
							opened();
							if (!err.Empty())
								put(st.projError, (uint32)sizeof(st.projError), err.CStr());
							std::printf("[nk3d] NK_PROJECT : projet OUVERT -> %s\n",
										proj.file.CStr());
						}
					} else
						fail(err);
					break;
				}

				case 3:	  // Enregistrer — LE FICHIER ACTIF
				case 8: { // Enregistrer tout — TOUT LE PROJET
					if (!proj.open || proj.file.Empty()) {
						// Jamais enregistre : « Enregistrer » DOIT se comporter comme
						// « Enregistrer sous... », sinon il echoue sans rien dire.
						st.projPending = 4;
						return;
					}
					// CTRL+S ENREGISTRE CE QU'ON REGARDE, pas tout le projet (Rihen).
					// Depuis qu'un asset est un fichier, « Enregistrer » doit se
					// comporter comme partout ailleurs ; « Enregistrer tout » existe
					// a cote pour le reste.
					const int32 card = (action == 3) ? NkActiveCard(st) : -1;
					{
						// LES ASSETS D'ABORD, chacun dans SON fichier ; le .nk3dm
						// ensuite, qui n'en porte que les liens. L'ordre compte :
						// c'est l'ecriture des assets qui fixe le chemin de chaque
						// carte, et c'est ce chemin que l'arbre enregistre.
						//
						// Tout est ecrit RELATIVEMENT A LA RACINE OU CA S'ECRIT : un
						// projet dont les chemins visent un autre dossier s'ouvre vide.
						if (!NkProjectWriteAssets(proj.root, st, &err, card)) {
							fail(err);
							st.quitAfterSave = false;
							break;
						}
						// L'ARBRE EST TOUJOURS REECRIT, meme pour un seul fichier :
						// il porte l'ordre, les dossiers et le chemin des cartes, et
						// un fichier ecrit dont l'arbre ignorerait le chemin serait
						// introuvable a la reouverture.
						NkArchive sc;
						NkProjectTreeCapture(sc, st);
						if (!NkProjectSave(proj, &err, &sc)) {
							fail(err);
							st.quitAfterSave = false;
							break;
						}
					}
					// MINIATURE de la scene regardee : elle date du meme geste que
					// le fichier — la carte du navigateur dira ce qu'on a enregistre.
					NkAsSceneThumbCapture(proj.root, st);
					rec.Touch(proj);
					if (action == 8)
						NkClearDirty(st);
					else
						NkClearDirtyDoc(st, st.TabDoc(st.activeTab));
					// QUITTER enregistre TOUT : partir en n'ayant ecrit qu'un fichier
					// laisserait le reste du travail derriere soi.
					if (st.quitAfterSave) {
						if (action == 3) {
							st.quitAfterSave = true;
							st.projPending = 8;
							return; // on repasse ici, cette fois pour tout
						}
						st.running = false;
					}
					st.quitAfterSave = false;
					break;
				}

				case 4: { // Enregistrer sous...
					const NkDialogResult r =
						NkDialogs::SaveFileDialog("nk3dm", "Enregistrer le projet sous...");
					if (r.confirmed && !r.path.Empty()) {
						// Racine de DESTINATION, pas l'actuelle : « Enregistrer
						// sous... » deplace le projet, et les fichiers d'assets
						// doivent atterrir dans le NOUVEAU dossier. NkProjectRootFor
						// applique la meme normalisation que l'ecriture elle-meme.
						const NkString dest = NkProjectRootFor(r.path.CStr());
						if (!NkProjectWriteAssets(dest, st, &err)) {
							fail(err);
							st.quitAfterSave = false;
							break;
						}
						NkArchive sc;
						NkProjectTreeCapture(sc, st);
						if (NkProjectSaveAs(proj, r.path.CStr(), &err, &sc)) {
							// La NOUVELLE racine n'a pas d'apercus : celui de la
							// scene active nait avec elle.
							NkAsSceneThumbCapture(dest, st);
							opened();
							if (st.quitAfterSave)
								st.running = false;
						} else
							fail(err);
					}
					st.quitAfterSave = false;
					break;
				}

				default:
					break;
			}
		}

	} // namespace nk3d
} // namespace nkentseu
