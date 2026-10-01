//
// NkEditeurReferences.h
// =============================================================================
// Description :
//   Les REFERENCES D'ASSETS des Details, a la maniere d'Unreal (document 02,
//   §5) : la texture d'un sprite se choisit dans une liste filtree par type,
//   se prend de la selection du Content Browser, ou s'y glisse ; « parcourir »
//   saute a l'asset dans le Content Browser.
//
// Caracteristiques :
//   - (2026-10-01, retour 4 de Rihen : « comment mettre une texture sur un
//     sprite ? ») Chaque changement est RETENU (NkEditeurRetenir) : Ctrl+Z le
//     defait, Ctrl+Y le refait.
//   - Les chemins sont ceux du NAVIGATEUR (« Contenu/... ») ; la texture est
//     chargee par son chemin ABSOLU (NkEditeurCheminContenuAbsolu), le meme que
//     celui des vignettes : une image n'est chargee qu'une fois.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURREFERENCES_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURREFERENCES_H__

#include "Editeur/NkEditeurModele.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editeur {

		/// Pose l'image `cheminNav` (du Contenu) sur le sprite de `id` : sa region
		/// redevient l'image entiere, sa taille et sa teinte restent. Retenu
		/// (Ctrl+Z). Rend false (et l'annonce) si ce n'est pas une image lisible
		/// ou si l'entite n'a pas de sprite ; true sans rien retenir si c'est
		/// deja sa texture.
		bool NkEditeurTextureSprite(NkEditeurModele &m, ecs::NkEntityId id, const char *cheminNav);

		/// Retire la texture du sprite de `id` (« Aucune ») : il redevient un
		/// rectangle de sa teinte. Retenu (Ctrl+Z).
		bool NkEditeurSansTexture(NkEditeurModele &m, ecs::NkEntityId id);

		/// Le chemin du navigateur de la texture `texId` (vide : aucune, ou une
		/// texture hors du Contenu, comme celles de la scene de demonstration).
		NkString NkEditeurNavDeTexture(NkEditeurModele &m, uint32 texId);

		/// Les IMAGES du Contenu, recursivement, en chemins du navigateur, dans
		/// l'ordre du rail puis par nom. Borne a `maxi`.
		void NkEditeurImagesDuContenu(NkEditeurModele &m, NkVector<NkString> &sortie, uint32 maxi = 99u);

		/// Un chemin du navigateur designe-t-il une image (par son extension) ?
		bool NkEditeurEstImage(const char *cheminNav) noexcept;

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURREFERENCES_H__
