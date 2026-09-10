// =============================================================================
// Unkeny.h — l'en-tete parapluie du moteur de jeu 2D
//
// Inclure ce fichier donne acces a tout Unkeny. Pour un jeu, c'est le seul
// include necessaire.
//
// CE QUE UNKENY EST
//   Un moteur de jeu 2D bati sur NKCanvas. Il COMPOSE ce que le depot porte
//   deja — NKECS pour les entites, NKPhysics et NKCollision pour la simulation,
//   NKCanvas pour le rendu et la coquille d'application — et il n'en reecrit
//   rien.
//
// CE QU'IL N'EST PAS
//   Une variante de Noge. NKCanvas et NKRenderer sont EXCLUSIFS par regle du
//   depot : une fenetre utilise l'un ou l'autre. Noge est le moteur 3D sur
//   NKRenderer ; Unkeny est le moteur 2D sur NKCanvas. Deux piles, deux noms.
//
// POUR QUEL GENRE DE JEU — TOUS
//   Unkeny n'est pas un moteur de jeux a tours. Les trois jeux de plateau
//   ecrits avant lui ont servi a MESURER ce qui se repete ; ils n'ont pas
//   defini sa portee. Un RPG, un jeu de plateforme, un shoot, un puzzle, un
//   roguelike y trouvent leur compte :
//     tuiles et parallaxe  -> Monde/    (RPG, plateforme, roguelike)
//     animation par images -> Anim/     (tout ce qui bouge)
//     actions d'entree     -> Entree/   (tout ce qui se joue en temps reel)
//     vues et miniatures   -> Vues/     (ecran partage, minicarte, vignette)
//     physique et collision-> Scene/    (facultative, voir NkSceneConfig)
//   Le module Jeu/ porte du vocabulaire de GENRE (sieges humain/IA d'un jeu a
//   tours). Il est facultatif comme le reste : un RPG ne l'inclut jamais.
//
// LES ETAGES, du plus bas au plus haut
//   Scene/     entites, composants, vue, monde physique
//   Monde/     cartes de tuiles, couches, parallaxe
//   Rendu/     dessin des sprites, des collisionneurs, de la grille
//   Vues/      plusieurs vues d'une meme scene, miniatures hors ecran
//   Entree/    actions — le jeu parle d'actions, jamais de touches
//   Anim/      images de sprite, et interpolation de deplacement
//   Ui/        mise en page ancree sur la zone sure, widgets
//   Jeu/       vocabulaire de GENRE (jeux a tours) — facultatif
//
// L'ORDRE D'UNE TRAME, et il n'est pas indifferent
//   1. entrees        -> l'application decide
//   2. scene.Pas(dt)  -> physique a pas FIXE, puis recopie vers les transforms
//   3. NkDessinerScene -> lit, ne modifie rien
//   Dessiner avant de simuler afficherait toujours la trame precedente ; c'est
//   invisible a l'oeil et ca decale l'interaction d'une trame.
// =============================================================================
#pragma once

#include "Unkeny/Anim/NkUnkenyChemin.h"
#include "Unkeny/Anim/NkUnkenySpriteAnim.h"
#include "Unkeny/Entree/NkUnkenyActions.h"
#include "Unkeny/Jeu/NkUnkenySieges.h"
#include "Unkeny/Monde/NkUnkenyTuiles.h"
#include "Unkeny/Vues/NkUnkenyVues.h"
#include "Unkeny/Rendu/NkUnkenyRendu.h"
#include "Unkeny/Scene/NkUnkenyCamera.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"
#include "Unkeny/Scene/NkUnkenyScene.h"
#include "Unkeny/Ui/NkUnkenyGeometrie.h"
#include "Unkeny/Ui/NkUnkenyTheme.h"
#include "Unkeny/Ui/NkUnkenyWidgets.h"

// Les aides de texte vivent UN ETAGE PLUS BAS, dans NKCanvas : elles servent
// aussi aux applications qui n'utilisent pas Unkeny. On les re-exporte pour que
// `#include "Unkeny/Unkeny.h"` suffise vraiment.
#include "NKCanvas/App/NkCanvasGuiApp.h"
#include "NKCanvas/App/NkCanvasTexte.h"
