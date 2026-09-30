//
// NkUnkenyEclairage.h
// =============================================================================
// Description :
//   L'ECLAIRAGE 2D d'une scene, facultatif : une lumiere ambiante, des
//   lumieres ponctuelles, en cone et directionnelles (NkLumiere2D), les
//   lumieres liees aux emetteurs (un feu eclaire), et des OMBRES portees par
//   les collisionneurs. Il se compose sur ce qui est DEJA dessine dans le
//   viseur de la camera.
//
// Caracteristiques :
//   - ETEINT PAR DEFAUT (NkEclairage2D::actif) : alors NkDessinerEclairage ne
//     touche pas la liste d'affichage. Une scene sans lumiere se dessine au
//     pixel pres comme avant ce fichier (temoin L1 du banc).
//   - AUCUNE CIBLE HORS ECRAN, AUCUN SHADER. La carte de lumiere est calculee
//     sur le PROCESSEUR, aux sommets d'une maille d'ecran (8 px par defaut),
//     puis posee sur l'image en mode de melange MULTIPLY (DST_COLOR / ZERO) :
//     image x (ambiante + lumieres). C'est la seule chose que ce rendu demande
//     au dorsal, et les cinq dorsaux de NKCanvas la font, comme le
//     rasteriseur logiciel de NKGui (NkGuiDrawListRaster) qui sert au banc.
//   - La maille S'AFFINE (8 -> 4 -> 2 px) la ou une ombre ou un bord de cone
//     passe, et se FUSIONNE en longues bandes la ou tout est ambiant : une nuit
//     sans lumiere coute quelques dizaines de quadrilateres, pas vingt mille.
//   - Repli NK_VOILE : un voile sombre en melange alpha (exact pour une
//     lumiere blanche) et des halos additifs pour la couleur. Jamais un ecran
//     noir, meme la ou le MULTIPLY manquerait.
//
// Algorithmes implementes :
//   - decroissance (1 - d / portee)^k, cone en smoothstep (la loi de
//     ComputeLighting2D, render2d.frag.nksl de NKRenderer, portee au CPU) ;
//   - ombres : pour chaque lumiere, les aretes des occulteurs sont rangees en
//     SEAUX ANGULAIRES (directionnelle : seaux le long de la perpendiculaire).
//     Un point est a l'ombre si le segment point -> lumiere coupe une arete
//     d'un occulteur qui ne CONTIENT pas le point : la face d'un mur reste
//     eclairee, ce qui est derriere ne l'est pas. Une lumiere placee DANS un
//     occulteur (une lanterne et sa boite) l'ignore.
//
// ⚠️ LIMITES ASSUMEES (mesurees, dites) :
//   - La carte est bornee a [0, 1] par canal : une lumiere REVELE la couleur
//     d'un objet, elle ne la rend pas plus claire que lui. Le « plus clair »
//     passe par NkLumiere2D::halo (additif) et par les particules additives.
//   - Une ombre plus fine que la maille de base (8 px) peut passer entre deux
//     sommets et n'etre pas vue. La maille se regle (NkEclairage2D::maille).
//   - Pas de normal map : il y faudrait un calcul PAR PIXEL, donc un shader,
//     et les shaders personnalises de NKCanvas n'existent que sous OpenGL.
//   - Ombres DURES (a la maille pres) : pas de penombre calculee.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYECLAIRAGE_H__
#define __NKENTSEU_UNKENY_NKUNKENYECLAIRAGE_H__

#include "NKGui/Core/NkGuiDrawList.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		/// Ce que la derniere carte a coute : sans compteur, « pourquoi c'est
		/// lent » n'a pas de reponse.
		struct NkStatsEclairage2D {
				int32 lumieres = 0;	  ///< lumieres qui touchent la vue (composants + emetteurs)
				int32 ignorees = 0;	  ///< au-dela du budget (NK_ECLAIRAGE_MAX_LUMIERES)
				int32 occulteurs = 0; ///< collisionneurs retenus comme occulteurs
				int32 mailles = 0;	  ///< quadrilateres emis
				int32 points = 0;	  ///< evaluations de la carte
		};

		/// Le budget de lumieres par image. Au-dela, les plus lointaines du centre
		/// de la vue sont ignorees, et COMPTEES.
		constexpr int32 NK_ECLAIRAGE_MAX_LUMIERES = 48;

		/// Pose la carte de lumiere de la scene sur ce qui est deja dessine dans
		/// le viseur de sa camera. A appeler APRES la scene (formes, sprites,
		/// matiere, effets alpha) et AVANT les effets emissifs et les surcouches
		/// d'editeur. Ne fait RIEN si l'eclairage de la scene est eteint.
		NkStatsEclairage2D NkDessinerEclairage(nkgui::NkGuiDrawList &dl, NkScene &scene);

		/// La lumiere recue en un point du MONDE, par le MEME calcul que la carte
		/// (ambiante comprise, avant bornage). Pour les bancs, et pour un jeu :
		/// « le joueur est-il dans l'ombre ? ». Rend (1, 1, 1) si l'eclairage est
		/// eteint : rien n'est alors multiplie, tout se voit comme en plein jour.
		void NkLumiereAuPoint(NkScene &scene, const NkVec2f &monde, float32 rgb[3]);

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYECLAIRAGE_H__
