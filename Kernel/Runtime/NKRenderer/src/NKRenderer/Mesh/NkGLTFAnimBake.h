// =============================================================================
// NKRenderer/Mesh/NkGLTFAnimBake.h
// -----------------------------------------------------------------------------
// Cuisson d'une animation glTF en clip editable : FONCTION LIBRE, du cote glTF.
//
// POURQUOI CE FICHIER EXISTE. `NkAnimationClip::BakeFromGLTF` etait une METHODE
// du clip, et c'etait la SEULE chose qui liait le modele d'animation au chargeur
// glTF : un `NkGLTFMeshData` en parametre, dans un en-tete qui, sinon, ne connait
// que Foundation. Ce lien unique suffisait a empecher l'animation de sortir du
// renderer (extraction du 2026-08-14, bloc de decision « substrats animation »).
//
// LE REMEDE EST UNE FONCTION LIBRE, PAS UNE ABSTRACTION. Introduire une interface
// ou une inversion de dependance pour UN SEUL symbole couterait un virtuel a
// maintenir pour rien. La fonction vit du cote qui connait le format ; le clip
// n'a plus a savoir que glTF existe. Tous les membres qu'elle touche sont publics,
// donc aucun `friend`, aucun accesseur ajoute.
//
// C'est le motif deja impose dans ce depot le 2026-07-26, quand la seconde
// structure demi-arete a ete supprimee au profit de `NkEditMesh` : les operations
// manquantes s'ajoutent en FONCTIONS LIBRES sur le type existant, jamais via une
// couche concurrente. Un precedent maison vaut mieux qu'un patron generique.
//
// AUTEUR : Rihen — LICENCE : usage regi par le fichier LICENSE a la racine du depot
// =============================================================================
#pragma once

#ifndef __NKENTSEU_NKRENDERER_GLTFANIMBAKE_H__
#define __NKENTSEU_NKRENDERER_GLTFANIMBAKE_H__

#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"

namespace nkentseu {
	namespace renderer {

		// Echantillonne la pose squelettique de `data` (animation `animIdx`) a `fps`
		// sur toute sa duree et remplit `out` : keyframes par os en transforms
		// bone-LOCAL + squelette (parents, inverseBind, ordre topologique), de sorte
		// que le clip soit editable, sauvegardable en .nkanim et rejouable par
		// NkAnimationPlayer sans que rien de tout cela ne connaisse glTF.
		// Renvoie false si le modele n'est pas skinne.
		bool BakeClipFromGLTF(const NkGLTFMeshData &data, int32 animIdx, float32 fps, NkAnimationClip &out);

	} // namespace renderer
} // namespace nkentseu

#endif // __NKENTSEU_NKRENDERER_GLTFANIMBAKE_H__
