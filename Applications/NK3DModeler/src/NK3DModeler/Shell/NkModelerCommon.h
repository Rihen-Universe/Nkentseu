#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerCommon.h
// @Brief   Utilitaires PARTAGES par plusieurs surfaces de l'interface : fond de
//          survol commun, et lecture du nom / de la visibilite d'un noeud de
//          scene.
//
//          Ils vivaient au milieu de NkModelerScreens.h ; le decoupage les a
//          revele comme communs -- la vue 3D, la hierarchie et les proprietes
//          les appellent toutes. Un utilitaire partage a besoin d'un endroit a
//          lui, sinon il retient le fichier dont on veut le sortir.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerTables.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h"
#include <cstdio>
#include <cstring> // strcmp : comparaison des noms de materiaux

namespace nkentseu {
	namespace nk3d {

		// Fond de survol. UN SEUL endroit, pour que tous les elements survolables
		// reagissent pareil : un survol qui change d'aspect d'un bouton a l'autre se
		// lit comme un defaut d'affichage, pas comme une intention.
		inline void HoverFill(NkModelerPainter &p, const NkRect &r, bool on, float32 rounding = 3.f) {
			if (on)
				p.Fill(r, NkRole::PanelBg, rounding);
		}

		// ── OUVRIR / FERMER LA MODALE « AJOUTER UN MATERIAU » ───────────────────
		// UNE SEULE PORTE, parce que deux etats decrivent la meme chose : le
		// drapeau de l'application (`matAddOpen`) et le cadre du kit
		// (`matAddModal`). Les fermer separement laisse le cadre arme.
		//
		// CE QUE COUTAIT LA PORTE DEROBEE (Rihen, 13 aout : « je dois appuyer
		// 2 fois sur + ») : le bouton « Nouveau » posait `matAddOpen = false` sans
		// toucher au cadre. Or le kit se defend du clic qui l'OUVRE grace a
		// `posInit` -- tant qu'il est faux, il ignore le clic tombant hors de la
		// boite. En sortant par la porte derobee, `posInit` restait VRAI : a la
		// reouverture, le clic sur « + » comptait comme un clic « a cote » et la
		// refermait dans la meme frame. Le premier clic n'ouvrait donc rien.
		inline void NkMatAddSetOpen(NkModelerState &st, bool ouvert) {
			st.matAddOpen = ouvert;
			st.matAddJustOpened = ouvert;
			if (!ouvert) {
				st.matAddModal.open = false;
				st.matAddModal.posInit = false; // re-arme la garde du clic d'ouverture
				st.matAddModal.dragging = false;
				st.matAddSel = -1;
			}
		}

		// Ligne a SAUTER dans la hierarchie : noeud supprime ou slot libre.
		inline bool NkHierNodeSkip(int32 node) {
			if (demo::Demo3DHostNodeDeleted(node))
				return true;
			if (demo::Demo3DHostNodeScene(node) != demo::Demo3DHostActiveScene())
				return true; // appartient a un autre document (scene/editeur)
			// Les MESH INTERNES d'un model sont sa matiere, pas des objets de
			// scene : ils ne figurent que dans la hierarchie du MODEL (Rihen).
			if (!demo::Demo3DHostDocIsModel() && demo::Demo3DHostNodeIsMesh(node))
				return true;
			return node >= 96 && demo::Demo3DHostUserKind(node) == 0;
		}

		inline void NkHierNodeName(NkModelerState &st, int32 node, char *out, uint32 cap) {
			if (node >= 0 && node < 160 && st.customNames[node][0]) {
				snprintf(out, cap, "%s", st.customNames[node]);
				return;
			}
			if (node >= 96) {
				// OBJET UTILISATEUR : nom par nature + numero de slot.
				static const char *const kUK[11] = {"Objet", "Sphere", "Cube",
													"Plan",  "Empty",  "Lumiere",
													"Texte", "Courbe", "Surface",
													"Metaball", "Cercle"};
				int32 k2 = demo::Demo3DHostUserKind(node);
				if (k2 < 0 || k2 > 10)
					k2 = 0;
				const char *bn = kUK[k2];
				if (k2 == 3) {
					// les PLANS a sous-type portent leur vrai nom
					const int32 sb = demo::Demo3DHostUserSub(node);
					if (sb == 2)
						bn = "Plan maille";
					else if (sb == 3)
						bn = "Plan infini";
				}
				if (k2 == 4) {
					// les VIDES a sous-type portent leur vrai nom
					const int32 sb = demo::Demo3DHostUserSub(node);
					if (sb == 10)
						bn = "Camera";
					else if (sb == 11)
						bn = "Reference";
					else if (sb == 12)
						bn = "Arriere-plan";
					else if (sb == 13)
						bn = "Image";
				}
				if (k2 == 5) {
					// nom par TYPE de lumiere (Rihen)
					static const char *const kLT[4] = {"Soleil", "Point light", "Spot",
													   "Area"};
					bn = kLT[demo::Demo3DHostUserSub(node) & 3];
				}
				snprintf(out, cap, "%s.%03d", bn, node - 96);
				return;
			}
			if (node >= 90)
				snprintf(out, cap, "%s", kNkEmptyNames[node - 90]);
			else if (node >= 86)
				demo::Demo3DHostLightName(node - 86, out, cap);
			else
				demo::Demo3DHostObjectName(node, out, cap);
		}

		// ── UN NOM DE MATERIAU EST UNIQUE DANS TOUT LE PROJET ───────────────────
		// Regle de Rihen : « peu importe le dossier, deux materiaux ne doivent pas
		// porter le meme nom ». La raison est concrete : la pastille et les listes
		// affichent le NOM SEUL -- deux « Bois » ranges dans deux dossiers y seraient
		// indiscernables. C'est aussi ce que fait Blender, qui impose l'unicite par
		// type de donnee.
		//
		// ON RENOMME, ON NE REFUSE PAS (Rihen, 13 aout). Un refus sec bloque
		// l'utilisateur sans lui dire quel fichier lointain porte deja le nom ; le
		// suffixe numerote laisse le travail avancer. Meme convention que Blender :
		// « Bois », puis « Bois.001 », « Bois.002 »...
		//
		// `exclu` = emplacement a ignorer dans la comparaison (celui qu'on renomme),
		// pour qu'un materiau ne se declare pas en conflit avec lui-meme.
		inline void NkMatUniqueName(const char *voulu, int32 exclu, char *out, uint32 cap) {
			if (!out || cap == 0u)
				return;
			const char *base = (voulu && *voulu) ? voulu : "Materiau";
			auto pris = [&](const char *nom) -> bool {
				const int32 maxM = demo::Demo3DHostProjMatMax();
				for (int32 m = 0; m < maxM; ++m) {
					if (m == exclu)
						continue;
					char nm[64];
					float32 alb[3];
					float32 rough = 0.f, metal = 0.f;
					if (!demo::Demo3DHostProjMatInfo(m, nm, (uint32)sizeof(nm), alb, &rough,
													 &metal))
						continue;
					if (strcmp(nm, nom) == 0)
						return true;
				}
				return false;
			};
			snprintf(out, cap, "%s", base);
			if (!pris(out))
				return;
			// Le nom est pris : on numerote jusqu'a trouver libre. La borne haute
			// n'est pas une limite de projet, seulement un garde-fou de boucle.
			for (int32 n = 1; n < 1000; ++n) {
				char essai[80];
				snprintf(essai, sizeof(essai), "%s.%03d", base, n);
				if (!pris(essai)) {
					snprintf(out, cap, "%s", essai);
					return;
				}
			}
		}

		/// Passe TOUT le projet en revue et renomme les doublons deja presents.
		/// Appele a l'ouverture : les projets d'avant cette regle portent des noms
		/// en double (Rihen en avait deux « Materiau »), et rien ne les corrigeait.
		/// Rend le nombre de renommages effectues.
		inline int32 NkMatFixDuplicates() {
			int32 corriges = 0;
			const int32 maxM = demo::Demo3DHostProjMatMax();
			for (int32 m = 0; m < maxM; ++m) {
				char nm[64];
				float32 alb[3];
				float32 rough = 0.f, metal = 0.f;
				if (!demo::Demo3DHostProjMatInfo(m, nm, (uint32)sizeof(nm), alb, &rough, &metal))
					continue;
				// Un doublon n'est detecte que face aux emplacements PRECEDENTS :
				// sinon le premier « Materiau » se renommerait a cause du second, et
				// l'ordre deviendrait arbitraire. Le premier garde son nom.
				bool doublon = false;
				for (int32 k = 0; k < m && !doublon; ++k) {
					char nk[64];
					float32 a2[3];
					float32 r2 = 0.f, g2 = 0.f;
					if (demo::Demo3DHostProjMatInfo(k, nk, (uint32)sizeof(nk), a2, &r2, &g2))
						doublon = (strcmp(nk, nm) == 0);
				}
				if (!doublon)
					continue;
				char libre[80];
				NkMatUniqueName(nm, m, libre, (uint32)sizeof(libre));
				demo::Demo3DHostProjMatSetName(m, libre);
				++corriges;
			}
			return corriges;
		}

		// ── LACHER SUR LA VUE 3D : LES DEUX GESTES QUE main.cpp APPLIQUE ────────
		// Ici et pas dans main.cpp parce que le menu « enfant ou independant »
		// (peint dans NkModelerHierarchy.h) fait EXACTEMENT le meme travail :
		// deux copies auraient diverge des le premier correctif, et un modele
		// depose dans le vide n'aurait plus rien eu a voir avec le meme modele
		// depose sur un objet.

		/// Fait naitre le modele du jeton et rend son noeud, ou -1.
		/// Le jeton porte son noeud SOURCE fige : c'est lui qu'on duplique.
		inline int32 NkDropSpawnModel(NkModelerState &st) {
			int32 nn = -1;
			if (st.dropSrcNode > 0) {
				// LES ASSETS ANCIENS SE REPARENT ICI. Ceux crees avant le 16 aout
				// portent une origine loin de leur matiere ; la fonction etant
				// idempotente, l'appeler avant chaque copie les remet d'aplomb sans
				// jamais rien deranger a ceux qui le sont deja.
				//
				// ⚠️ ACCOLADES OBLIGATOIRES, et elles ont manque. Le 16 aout
				// (4dd4ed21) le premier appel a ete ajoute a un `if` SANS accolades :
				// l'indentation faisait croire que les deux lignes etaient gardees
				// alors que seule la premiere l'etait, et le duplicata partait sur
				// TOUS les lachers, y compris ceux sans source -- donc avec -1.
				// Une garde qui a l'air de proteger et qui ne protege rien.
				demo::Demo3DHostRecenterModel(st.dropSrcNode - 1);
				nn = demo::Demo3DHostDuplicateNode(st.dropSrcNode - 1);
			}
			if (nn < 0) // asset sans source : cube par defaut, comme avant
				nn = demo::Demo3DHostAddNode(2, 0);
			return nn;
		}

		/// REFUS NOMME. Aucune nature ne reste muette sur un objet : un refus
		/// silencieux est indistinguable d'un glisser-deposer casse, et le depot
		/// a deja paye ce silence une fois (regle « tout refus silencieux d'une
		/// action utilisateur doit dire pourquoi », vague 27).
		///
		/// Le message dit CE QU'IL AURAIT FALLU FAIRE, pas seulement que c'est
		/// refuse : « une texture ne se depose pas ici » laisse l'utilisateur
		/// devant la meme question.
		inline void NkDropRefuse(NkModelerState &st, uint8 kind) {
			const char *why = nullptr;
			switch (kind) {
				case 3:
					why = "Une texture se depose dans un canal du materiau, pas sur l'objet";
					break;
				case 5: why = "Une scene s'ouvre, elle ne se depose pas sur un objet"; break;
				case 1: why = "Un dossier ne se depose pas sur un objet"; break;
				case 0: why = "Un graphe ne se depose pas sur un objet"; break;
				case 4: why = "Un dataset IA ne se depose pas sur un objet"; break;
				default: why = "Cet element ne se depose pas sur un objet"; break;
			}
			snprintf(st.hierNote, sizeof(st.hierNote), "%s", why);
		}

	} // namespace nk3d
} // namespace nkentseu
