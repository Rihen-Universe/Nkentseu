#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamille.h
// @Brief   LA FAMILLE « Unreal 5 » des editeurs Nkentseu : tout ce qu'un editeur
//          inclut pour ressembler EXACTEMENT a UnkenyEditor.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// Document de reference : Applications/UnkenyEditor/design/02-fidelite-ue5.md
// (§0 : « la famille partage ses composants par NKEditorKit, chacun avec sa
// touche ») et la feuille de route R32 (« Nogee ressemble exactement a
// UnkenyEditor »).
//
//   NkFamilleStyle.h    palette lue dans le theme, texte, bouton, onglets, recherche
//   NkFamilleEntree.h   evenement de la fenetre -> entree NKGui (sans clic perdu)
//   NkFamilleCadre.h    plan d'UE5, barre de titre avec LOGO au coin (deux lignes),
//                       onglets de scene, barre d'outils, barre d'etat, cloisons,
//                       bords d'une fenetre sans cadre, menus deroulants
//   NkFamillePlacer.h   « Placer des acteurs » (onglets verticaux, recherche, glisser)
//   NkFamilleOutliner.h l'Outliner (oeil, cadenas, Nom | Type, renommage, rattacher)
//   NkFamilleDetails.h  le panneau Details (en-tete, arbre des composants, pastilles,
//                       CARTES, liseres X/Y/Z, verrou, remise)
//   NkFamilleVue.h      barre de vue, barre flottante du viseur, repere d'axes
//   NkFamilleContenu.h  le tiroir « Contenu » : le Content Browser Unreal du kit sur un dossier
//   NkFamilleJournal.h  le tiroir « Journal » (Output Log d'Unreal)
//   NkFamilleEditeur.h  LA TRAME d'un editeur de la famille, prete a deriver :
//                       polices, theme, entree, plan, ordre de dessin d'UnkenyEditor,
//                       menus, fenetre modale (NkAnimaEditor ; NKScena, PV3DE ensuite)
//   (le tiroir « Terminal » est NKEditorKit/Terminal/NkTerminalPanneau.h)
//
// Qui s'en sert : Nogee (le premier, 01/10/2026), NkAnimaEditor (par NkFamilleEditeur) ; UnkenyEditor a
// ses copies d'origine et basculera sur celles-ci (R32).
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleStyle.h"
#include "NKEditorKit/Famille/NkFamilleEntree.h"
#include "NKEditorKit/Famille/NkFamilleCadre.h"
#include "NKEditorKit/Famille/NkFamillePlacer.h"
#include "NKEditorKit/Famille/NkFamilleOutliner.h"
#include "NKEditorKit/Famille/NkFamilleDetails.h"
#include "NKEditorKit/Famille/NkFamilleVue.h"
#include "NKEditorKit/Famille/NkFamilleContenu.h"
#include "NKEditorKit/Famille/NkFamilleJournal.h"
#include "NKEditorKit/Famille/NkFamilleEditeur.h"
