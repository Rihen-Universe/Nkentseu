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

namespace nkentseu {
	namespace nk3d {

		// Fond de survol. UN SEUL endroit, pour que tous les elements survolables
		// reagissent pareil : un survol qui change d'aspect d'un bouton a l'autre se
		// lit comme un defaut d'affichage, pas comme une intention.
		inline void HoverFill(NkModelerPainter &p, const NkRect &r, bool on, float32 rounding = 3.f) {
			if (on)
				p.Fill(r, NkRole::PanelBg, rounding);
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

	} // namespace nk3d
} // namespace nkentseu
