#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorInspectorFrame.h
// @Brief   LA charpente d'inspecteur : en-tete, onglets, sections. Sans etat.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE, ET IL A UNE HISTOIRE PRECISE
// =============================================================================
// L'auteur de NkUIDesign l'avait ecrit, en toutes lettres, DANS son fichier :
//
//   « Signale au canal : il manque au kit une CHARPENTE d'inspecteur (en-tete +
//     onglets + sections nommees) independante de NKReflection. C'est elle que
//     les quatre editeurs partageront ; elle n'existe pas. »
//
// Le signalement est reste. La charpente a ete ecrite chez lui quand meme -- et
// une deuxieme fois chez Nogee, et une troisieme. Mesure du 2026-08-29 :
// QUATRE charpentes, dans TROIS applications.
//
//   NkUIDesign  InspectorPanel                  en-tete + 3 onglets + 7 sections
//   Nogee       DetailsPanel         (146 l.)   sections sur un acteur ECS
//   Nogee       InspectorPanel        (54 l.)   second inspecteur du meme editeur
//   Nogee       Model/NkInspectorModel.h (56)   un modele deja abstrait, chez lui
//
// ⚠️ ET C'EST UNE CAUSE DIFFERENTE DE CELLE DE LA CONVERSION DE THEME. On a
//    ecrit ailleurs, le meme jour, que « la deuxieme copie n'est presque jamais
//    un caprice, c'est la seule porte restee ouverte » -- une conversion qu'il
//    fallait une COQUILLE pour appeler. ICI CETTE EXPLICATION NE TIENT PAS : la
//    premiere copie n'existait nulle part, rien n'etait ferme. Deux causes
//    distinctes au meme motif, et savoir laquelle on a change le remede :
//
//      (1) la chose EXISTE mais on ne peut pas l'appeler   -> il faut l'OUVRIR ;
//      (2) la chose N'EXISTE PAS et personne ne peut se permettre d'attendre
//          qu'elle existe                                  -> il faut l'ECRIRE.
//
//    Un signalement n'est pas un remede. Celui-ci a tenu des semaines pendant
//    que trois applications payaient.
//
// =============================================================================
//  LA PORTEE, ASSUMEE : LE MONDE `NkGuiContext`
// =============================================================================
// Cette charpente dessine avec `nkgui::CollapsingHeader`, `nkgui::TabBarEx`,
// `nkgui::Text` et `nkgui::Separator`. Elle sert donc **les applications qui
// dessinent avec le vocabulaire de widgets de NKGui** : NkUIDesign et Nogee.
//
// ⚠️ NK3DModeler N'EST PAS SERVI, ET CE N'EST PAS UN OUBLI. Mesure du
//    2026-08-29 -- et elle corrige une idee fausse qui circulait :
//
//      il PASSE bien par NkGuiContext : 33 occurrences dans 12 fichiers sur 36
//      (NkGuiKey 38, NkGuiInput 35, glisser-deposer 30 appels, NkVec2, NkRect,
//       NkGuiFont, NkGuiDrawList...)
//      mais il n'appelle AUCUN widget : zero Button, zero Text, zero
//      CollapsingHeader, zero TabBar, zero DragFloat.
//
//    **La barriere n'est donc pas le CONTEXTE, c'est le VOCABULAIRE.** Il peint
//    avec `NkModelerPainter(NkGuiDrawList&, NkGuiFont&, NkTheme&, ...)`,
//    c'est-a-dire *dans une liste de dessin NKGui* : le substrat est commun, les
//    widgets ne le sont pas. Son panneau de proprietes fait 7 914 lignes, avec
//    son propre `SectionHeader` et son propre `DragFloat`.
//
//    CE QU'IL FAUDRAIT POUR L'Y FAIRE ENTRER -- et ce n'est pas un travail de
//    charpente : que `NkModelerPainter` devienne une implementation de
//    `NkComponentPaint` (l'interface abstraite du kit, 213 l., qui a deja deux
//    implementations : `NkGuiComponentPaint` et `NkRecordingPaint`). Il ne la
//    cite nulle part aujourd'hui. C'est un portage, il se decide dans son
//    chantier, et il paierait d'un coup l'arbre, le navigateur et l'inspecteur.
//
// ⚠️ ET ON N'A PAS ABSTRAIT DU PEINTRE POUR L'ANTICIPER. L'abstraction existe
//    deja (`NkComponentPaint`) et il n'y est pas : en inventer une seconde
//    donnerait **trois interfaces de peintre pour deux besoins**, et
//    dessinerait le joint pour un consommateur qui ne peut pas l'eprouver.
//
//    > **Une charpente se valide sur deux consommateurs QUI PARTAGENT SON
//    > VOCABULAIRE, et elle NOMME dans son en-tete ceux qui ne le partagent
//    > pas, avec ce qu'il faudrait pour les faire entrer. Un consommateur hors
//    > vocabulaire ne valide rien -- il ne peut ni l'appeler, ni la
//    > contredire.**
//
// =============================================================================
//  LE JOINT : LA STRUCTURE ICI, LES LIGNES CHEZ L'APPELANT
// =============================================================================
// La charpente dessine EXACTEMENT quatre choses -- le nom, un supplement
// d'en-tete, les onglets, la boucle de sections -- et appelle un rappel pour le
// corps de chaque section. Elle ne connait ni le type de l'objet inspecte, ni
// la nature des champs.
//
// ⚠️ ELLE NE PORTE PAS LES LIGNES, ET C'EST DELIBERE. Une ligne
//    « libelle / valeur » n'a pas la meme forme dans les trois applications :
//      - NkUIDesign l'affiche en LECTURE SEULE -- regle de Rodolf du 18/08,
//        « la position est un RESULTAT, pas une donnee » ;
//      - NK3DModeler a besoin de champs EDITABLES ;
//      - Nogee affiche la rotation en lecture seule PARCE QU'une conversion
//        quaternion vers euler produirait des valeurs fausses -- c'est ecrit
//        chez lui.
//    Trancher ici imposerait un des trois usages aux deux autres, et cette
//    charpente deviendrait la CINQUIEME copie au lieu de la premiere
//    mutualisation. Les usages different dans les LIGNES, pas dans la
//    STRUCTURE : le joint passe la.
//
// ⚠️ ET ELLE NE PORTE AUCUN ETAT. Ni l'onglet courant, ni les sections
//    ouvertes. `nkgui::TabBarEx` garde l'onglet (persistant par son id) et
//    `nkgui::CollapsingHeader` garde l'ouverture. Mesure du 2026-08-29 :
//    `Nogee/Panels/Model/NkInspectorModel.h` porte `IsSectionOpen` /
//    `SetSectionOpen` alors que `CollapsingHeader` tient deja cet etat -- et son
//    auteur l'avait note lui-meme (« ne servent QU'au chemin NKUI »). Deux
//    verites sur « cette section est-elle ouverte », dont une sur un chemin
//    mort. **Remonter ce doublon d'un etage l'aurait rendu permanent.**
//
// ⚠️ EN-TETE SEUL, ET SANS COQUILLE. `NkEditorShell` n'est pas inclus. Une
//    charpente qui exigerait une instance de coquille serait inappelable par
//    qui n'en a pas -- exactement le verrou qu'on vient de retirer sur la
//    conversion de theme, et la lecon vaut plus large : *une capacite peut etre
//    inaccessible sans etre cachee ; exiger un objet que l'appelant n'a pas
//    suffit.* Aucun `.jenga` a modifier pour l'appeler.
//
// IDIOME : `void *user` + pointeurs de fonction, comme `NkTreeViewHooks`,
// `dockHeaderFn`, `mAppMenuFn` et `clipboardGetFn`. Rien d'invente pour
// l'occasion.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKEditorKit/NkEditorScrollbar.h" // la barre STANDARD (une seule pour toute l'UI)
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		/// Une section : un titre, et le rappel qui dessine son corps.
		struct NkInspectorSection {
				const char *titre = nullptr;

				/// Appele UNIQUEMENT si la section est visible (depliee, ou non
				/// repliable). Une section fermee ne doit rien couter.
				void (*corps)(void *user, nkgui::NkGuiContext &ctx) = nullptr;

				/// `false` -> pas de chevron, le corps est TOUJOURS dessine.
				///
				/// ⚠️ CE DRAPEAU N'EST PAS UN CONFORT, IL VIENT D'UNE MESURE. La
				///    section « Transform » de `DetailsPanel` (Nogee) est
				///    specifiee « toujours en haut, NON repliable » (son §8). Sans
				///    ce drapeau, Nogee ne pouvait pas entrer dans la charpente --
				///    et « repliable ou non » est une propriete de la STRUCTURE,
				///    pas du contenu d'une ligne : elle est du bon cote du joint.
				///    Un titre nul avec `repliable = false` ne dessine que le
				///    corps, sans en-tete du tout.
				bool repliable = true;
		};

		/// La description d'un inspecteur. Aucune donnee ne survit a l'appel.
		struct NkInspectorCharpente {
				void *user = nullptr;

				/// Le nom de l'element inspecte, TOUJOURS visible (§12.2 de la
				/// specification NkUIDesign). Vide ou nullptr -> `enteteVide`.
				const char *entete = nullptr;
				const char *enteteVide = "Aucune sélection";

				/// Dessine APRES le nom et son filet, AVANT les onglets.
				///
				/// ⚠️ MESURE, PAS ANTICIPATION. `DetailsPanel` (Nogee) porte un
				///    champ de filtre (`mFilterBuf[64]`) et un menu « Ajouter un
				///    composant » dans cette zone. Sans ce rappel, son en-tete ne
				///    rentrait pas et la charpente n'aurait servi qu'un seul
				///    consommateur -- donc n'aurait ete validee par personne.
				///    C'est la POSITION qui est structurelle ; ce qu'on y dessine
				///    reste a l'appelant.
				void (*enteteSupplement)(void *user, nkgui::NkGuiContext &ctx) = nullptr;

				/// Onglets. `ongletCount == 0` -> aucune barre dessinee, et
				/// `sectionsDe` est appele avec l'onglet 0. C'est le cas de
				/// `DetailsPanel`, qui a des sections sans onglets.
				const char *const *onglets = nullptr;

				/// Onglets grises et NON selectionnables (`TabBarEx`). nullptr =
				/// tous actifs.
				/// ⚠️ PREFERER CECI A UN ONGLET QUI S'OUVRE SUR UNE PHRASE
				///    D'EXCUSE, quand l'onglet n'a rien : un onglet grise dit
				///    « pas encore » sans faire cliquer pour rien.
				const bool *ongletsActifs = nullptr;
				int32 ongletCount = 0;

				/// Identite NKGui de la barre : c'est ELLE qui porte l'onglet
				/// courant d'une image a l'autre. Deux inspecteurs dans la meme
				/// application doivent donc avoir deux id differents.
				const char *idOnglets = "inspecteur.onglets";

				/// Rend la table des sections de l'onglet `onglet`, et pose
				/// `count`. Rendre `count == 0` est LEGITIME : voir
				/// `messageOngletVide`.
				const NkInspectorSection *(*sectionsDe)(void *user, int32 onglet, int32 &count) = nullptr;

				/// Ce qui s'affiche quand un onglet n'a aucune section.
				/// ⚠️ UNE SECTION QUI MANQUE DOIT LE DIRE. Un onglet vide fait
				///    croire a des proprietes disparues ; sept sections vides
				///    aussi. La regle vient de la specification NkUIDesign (§12.2)
				///    et elle vaut pour tout le monde. nullptr -> une phrase par
				///    defaut, jamais le silence.
				const char *(*messageOngletVide)(void *user, int32 onglet) = nullptr;

				// ── COSTUME (opt-in, remandat Banani 2026-08-31) ─────────────────
				// Trois rappels de DESSIN, nuls par defaut = rendu historique
				// (Text/Separator, TabBarEx, CollapsingHeader). La STRUCTURE reste
				// celle de la charpente ; seul le TRAIT change — c'est le meme
				// joint que les lignes : le costume est chez l'appelant.
				/// Remplace « nom + filet » (l'en-tete 34 px a icone de la maquette).
				void (*dessineEntete)(void *user, nkgui::NkGuiContext &ctx, const char *entete) = nullptr;
				/// Remplace la barre TabBarEx. REND l'onglet courant — l'appelant
				/// porte alors lui-meme cet etat (il a remplace le widget qui le
				/// tenait, la verite reste unique).
				int32 (*dessineOnglets)(void *user, nkgui::NkGuiContext &ctx) = nullptr;
				/// Remplace le titre d'une section NON repliable (les MAJUSCULES
				/// 9 px + filet de la maquette). Les sections repliables gardent
				/// CollapsingHeader.
				void (*dessineTitreSection)(void *user, nkgui::NkGuiContext &ctx,
											const char *titre) = nullptr;
		};

		/// Dessine la charpente. Rend l'index de l'onglet courant (0 sans onglets).
		NKENTSEU_FORCE_INLINE int32 NkInspectorDessiner(nkgui::NkGuiContext &ctx,
														const NkInspectorCharpente &c) noexcept {
			// ── LE NOM, toujours visible ─────────────────────────────────────
			const bool aUnNom = c.entete && *c.entete;
			const char *nom = aUnNom ? c.entete : (c.enteteVide ? c.enteteVide : "Aucune sélection");
			if (c.dessineEntete)
				c.dessineEntete(c.user, ctx, nom); // costume : l'appelant dessine
			else {
				nkgui::Text(ctx, nom);
				nkgui::Separator(ctx);
			}

			// ── LE SUPPLEMENT D'EN-TETE (filtre, actions...) ─────────────────
			if (c.enteteSupplement)
				c.enteteSupplement(c.user, ctx);

			// ── LES ONGLETS, s'il y en a ─────────────────────────────────────
			int32 onglet = 0;
			if (c.dessineOnglets)
				onglet = c.dessineOnglets(c.user, ctx); // costume : idem
			else if (c.onglets && c.ongletCount > 0)
				onglet = nkgui::TabBarEx(ctx, c.idOnglets, c.onglets, c.ongletCount, c.ongletsActifs);

			// ── LES SECTIONS ─────────────────────────────────────────────────
			int32 n = 0;
			const NkInspectorSection *sections = c.sectionsDe ? c.sectionsDe(c.user, onglet, n) : nullptr;

			if (!sections || n <= 0) {
				const char *m = c.messageOngletVide ? c.messageOngletVide(c.user, onglet) : nullptr;
				nkgui::Text(ctx, m ? m : "Rien a afficher ici pour le moment.");
				return onglet;
			}

			// ── LA ZONE DES SECTIONS DEFILE, L'EN-TETE RESTE (Rodolf, 01/09 :
			//    « le panneau proprietes doit aussi avoir un scrollbar
			//    vertical ») ────────────────────────────────────────────────────
			// L'ancien defilement etait celui du CADRE DE DOCK : il emportait
			// l'en-tete et les onglets avec les sections — c'est la barre que
			// Rodolf a fait RETIRER la veille. Ici : nom + onglets FIXES, la zone
			// des sections defile seule, avec la barre STANDARD du kit
			// (NkEditorScrollbar — le meme code que les sections de la
			// Hierarchie, trois consommateurs, une geometrie).
			// ⚠️ L'etat de defilement vit dans le magasin PUBLIC du contexte
			//    (scrollKeys/scrollVals) — la charpente reste SANS ETAT.
			const bool cadre = ctx.childDepth > 0;
			nkgui::NkRect vis{0.f, 0.f, 0.f, 0.f};
			float32 defY = 0.f;
			nkgui::NkGuiId idDef = 0;
			nkgui::NkGuiLayout disposAvant;
			bool defile = false;
			if (cadre) {
				const nkgui::NkRect &a = ctx.childStack[ctx.childDepth - 1].area;
				const float32 sbw = NkScrollbarWidth();
				vis = {ctx.layout.region.x, ctx.layout.cursor.y,
					   a.x + a.w - sbw - ctx.layout.region.x, a.y + a.h - ctx.layout.cursor.y};
				if (vis.h > 40.f && vis.w > 40.f) {
					idDef = ctx.GetId(c.idOnglets ? c.idOnglets : "insp.sections") ^ 0x5EC7104u;
					// l'etat persistant, par id (le magasin des zones defilables)
					int32 trouve = -1;
					for (uint32 k = 0; k < (uint32)ctx.scrollKeys.Size(); ++k)
						if (ctx.scrollKeys[k] == idDef) {
							trouve = (int32)k;
							break;
						}
					float32 maxPrec = 0.f;
					if (trouve >= 0) {
						defY = ctx.scrollVals[(uint32)trouve].y;
						maxPrec = ctx.scrollVals[(uint32)trouve].maxY;
					}
					// la molette au survol de la zone (le pouce et elle pilotent
					// LA MEME variable)
					if (ctx.popupDepth == 0 && nkgui::NkGuiRectContains(vis, ctx.input.mousePos))
						defY -= ctx.input.wheel * 36.f;
					// 🔴 L'ECRETAGE EST ICI, AVANT LA DISPOSITION — ET C'EST LA
					//    CORRECTION DU 02/09 (Rodolf : « lorsqu'on scrolle et qu'on
					//    depasse les limites, ca clignote ou ca dandine »).
					//
					//    Le commentaire qui etait a cette place disait « le clamp est
					//    dans NkVScrollbar », et c'etait VRAI — mais `NkVScrollbar`
					//    est appele TRENTE LIGNES PLUS BAS, une fois le contenu deja
					//    dispose et peint a `vis.y - defY`. En butee, la molette
					//    poussait `defY` de 36 px hors bornes, l'image entiere etait
					//    DESSINEE decalee de ces 36 px, puis la valeur etait ramenee
					//    dans les bornes et rangee. A l'image suivante elle repartait
					//    de la valeur propre, se faisait repousser, et redessinait
					//    decalee : **une oscillation d'exactement un cran de molette,
					//    tant que la roue tourne.** Ce n'est pas un defaut de dessin,
					//    c'est une correction qui arrive UNE IMAGE TROP TARD.
					//
					// ⚠️ ON ECRETE AVEC L'ETENDUE DE L'IMAGE PRECEDENTE, et c'est le
					//    seul chiffre honnete disponible ici : la vraie etendue ne se
					//    connait qu'apres avoir dispose le contenu, c'est-a-dire trop
					//    tard pour ce qu'on est en train de peindre. `NkVScrollbar`
					//    ecrete a nouveau en fin de zone avec l'etendue FRAICHE, donc
					//    une etendue qui change reste rattrapee en une image — sans
					//    jamais peindre hors bornes.
					// ⚠️ ET LA PREMIERE IMAGE EST LE CAS LIMITE : `maxPrec` y vaut 0,
					//    donc `defY` est epingle a 0. C'est exact — rien n'a encore
					//    ete defile — et l'etendue reelle arrive a la fin de cette
					//    meme image.
					if (defY < 0.f)
						defY = 0.f;
					if (defY > maxPrec)
						defY = maxPrec;
					ctx.DL().PushClipRect({vis.x, vis.y, vis.w + sbw, vis.h}, true);
					disposAvant = ctx.layout;
					ctx.BeginLayout({vis.x, vis.y - defY, vis.w, 1.0e6f});
					defile = true;
				}
			}

			for (int32 i = 0; i < n; ++i) {
				const NkInspectorSection &s = sections[i];
				if (!s.corps)
					continue;

				if (!s.repliable) {
					// Non repliable : le titre est un simple intitule, sans chevron
					// -- promettre un chevron qui ne replie rien serait pire que
					// pas de chevron du tout.
					if (s.titre && *s.titre) {
						if (c.dessineTitreSection)
							c.dessineTitreSection(c.user, ctx, s.titre); // costume
						else
							nkgui::Text(ctx, s.titre);
					}
					s.corps(c.user, ctx);
					continue;
				}

				if (!s.titre)
					continue; // une section repliable sans titre n'a rien a cliquer
				// ⚠️ L'ETAT D'OUVERTURE APPARTIENT A `CollapsingHeader`, pas a
				//    nous. On ne le lit pas, on ne le garde pas, on ne le restaure
				//    pas.
				if (nkgui::CollapsingHeader(ctx, s.titre))
					s.corps(c.user, ctx);
			}

			// ── LA FERMETURE DE LA ZONE : etendue mesuree, barre, etat range ──
			if (defile) {
				const float32 contentH = ctx.layout.cursor.y - (vis.y - defY);
				ctx.DL().PopClipRect();
				ctx.layout = disposAvant;
				// la zone consomme TOUT le reste visible : le cadre de dock n'a
				// plus de debordement, la molette ne lui vole plus les crans
				ctx.layout.cursor.y = vis.y + vis.h;
				const float32 sbw = NkScrollbarWidth();
				// GEOMETRIE : pouce en haut = premiere section a vis.y ; pouce en
				// butee basse = derniere section entiere (defY = etendue -
				// fenetre) — le meme contrat que hier.defile.*.
				NkVScrollbar(ctx, ctx.DL(), {vis.x + vis.w, vis.y, sbw, vis.h}, defY, contentH,
							 vis.h, idDef, 24.f);
				// ranger l'etat (creer l'entree si c'est la premiere image)
				int32 trouve = -1;
				for (uint32 k = 0; k < (uint32)ctx.scrollKeys.Size(); ++k)
					if (ctx.scrollKeys[k] == idDef) {
						trouve = (int32)k;
						break;
					}
				if (trouve < 0) {
					ctx.scrollKeys.PushBack(idDef);
					nkgui::NkGuiScrollState s0;
					ctx.scrollVals.PushBack(s0);
					trouve = (int32)ctx.scrollKeys.Size() - 1;
				}
				ctx.scrollVals[(uint32)trouve].y = defY;
				ctx.scrollVals[(uint32)trouve].maxY =
					contentH - vis.h > 0.f ? contentH - vis.h : 0.f;
			}
			return onglet;
		}

		// =====================================================================
		//  L'EPREUVE SUR UN SECOND CONSOMMATEUR -- `DetailsPanel` (Nogee, 146 l.)
		// =====================================================================
		// « Une forme validee sur le seul composant pour lequel elle a ete ecrite
		//   n'est pas validee » (`NkComponentDecl.h`). Confrontation ligne a
		//   ligne, faite AVANT de figer l'interface -- et deux de ses besoins ont
		//   change le dessin :
		//
		//   CE QUI ENTRE TEL QUEL
		//     - le nom de l'entite selectionnee            -> `entete`
		//     - sections par composant ECS                 -> `sectionsDe`
		//     - aucune barre d'onglets                     -> `ongletCount = 0`
		//     - « rien de selectionne »                    -> `messageOngletVide`
		//
		//   CE QUI A FAIT CHANGER LE DESSIN (et c'est le but d'une epreuve)
		//     - `RenderTransform` : section « toujours en haut, NON repliable »
		//       (son §8)                                   -> `repliable = false`
		//     - `mFilterBuf` (filtre de propriete) et le menu « Ajouter un
		//       composant », tous deux au-dessus des sections
		//                                                  -> `enteteSupplement`
		//
		//   CE QUI N'ENTRE PAS, ET QUI NE DOIT PAS ENTRER
		//     - `RenderVec3Row` (ligne X/Y/Z a libelles colores, cadenas
		//       d'echelle uniforme) : c'est une LIGNE. NkUIDesign n'en a pas
		//       l'usage et l'afficherait en lecture seule ; NK3DModeler la veut
		//       editable. Elle reste chez Nogee.
		//     - `PassesFilter` : une regle de filtrage propre a la cible. La
		//       charpente ne filtre pas -- `sectionsDe` rend deja la table que
		//       l'appelant veut montrer, filtrage compris.
		//     - `mLockScale`, `mWorld`, `mSel`, `mHist` : l'etat de la cible.
		//
		// ⚠️ CE QUE CETTE EPREUVE A COUTE, ET POURQUOI ELLE VALAIT LE COUP : sans
		//    elle, la charpente n'aurait eu ni `repliable` ni `enteteSupplement`,
		//    et Nogee n'aurait pas pu l'utiliser -- elle aurait ete « validee »
		//    sur son unique auteur, ce qui ne valide rien.

	} // namespace editorkit
} // namespace nkentseu
