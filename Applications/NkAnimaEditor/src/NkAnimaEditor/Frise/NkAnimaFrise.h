#pragma once
// =============================================================================
// NkAnimaFrise.h — LA FRISE PARTAGEE dans NkAnimaEditor (2026-10-01 au soir).
//
// Rihen : « la frise faite dans Unkeny est meilleure que celle de
// NkAnimaEditor ». L'ancienne (Panels.h, TimelinePanel : une rangee de ronds,
// 900 x 64 px) laisse la place a LA frise de NKEditorKit (NkTimelineModel.h,
// facon Sequencer d'UE5 / Dope Sheet de Blender), la meme que la page
// Animation d'UnkenyEditor :
//   - piste « Poses-cles » : les poses-cles du clip (NkAnimationEditor) --
//     deplacer, supprimer, inserer, copier / coller, interpoler, annuler ;
//   - un ARBRE des os (« Hips/Spine/Chest »), replie : par os, Position et
//     Rotation (degres), leurs cles et leurs COURBES (verrouillees : une pose
//     s'edite dans la vue 3D, comme avant) ;
//   - transport, boucle, plage de lecture, marqueurs, zoom, theme.
// AUCUNE FONCTION D'AVANT N'EST PERDUE : lecture / pause, inserer une cle,
// supprimer, annuler, refaire, frotter le curseur, choisir et glisser une cle,
// la boucle, le compteur (temps, image, nombre de cles).
//
// ⚠️ CE FICHIER N'INCLUT PAS NKAnima (AnimBridge.h l'explique : NKRenderer et
//    NKEditorKit ne cohabitent pas dans une unite) : tout passe par Anim*.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/Components/NkTimelineModel.h"

namespace nkanima {

	/// Dessine la frise dans la place restante du panneau courant (et y joue ses
	/// gestes). Appelle AnimUpdate(dt) : une seule fois par image, comme l'ancienne.
	void NkAnimaDessinerFrise(nkentseu::editorkit::NkEditorFrameContext &ec);
	/// (02/10) La meme, dans un rectangle IMPOSE (l'onglet « Frise » du tiroir de la
	/// face d'UE5, NkAnimaFace.cpp : ACCROCHE FRISE).
	void NkAnimaDessinerFriseZone(nkentseu::editorkit::NkEditorFrameContext &ec, nkentseu::float32 x, nkentseu::float32 y,
								  nkentseu::float32 w, nkentseu::float32 h);

	/// Le theme de la frise (celui de la coquille ; sombre par defaut, clair si
	/// NKANIMA_THEME=light).
	void NkAnimaThemeFrise(const nkentseu::editorkit::NkTheme &theme);

	/// Pour un banc ou une capture : le dernier resultat de la frise, et son modele.
	const nkentseu::editorkit::NkTimelineResult &NkAnimaFriseResultat();
	nkentseu::editorkit::NkTimelineModel &NkAnimaFriseModele();

	/// `--captures-frise=DOSSIER [modele]` : le panneau de la frise HORS ECRAN, en
	/// PNG (sans fenetre ni GPU). Rend 0 si toutes les images sont ecrites.
	nkentseu::int32 NkAnimaCapturesFrise(const char *dossier, const char *modele);

} // namespace nkanima
