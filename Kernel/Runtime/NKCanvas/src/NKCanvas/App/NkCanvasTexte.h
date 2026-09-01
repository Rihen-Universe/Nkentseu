// =============================================================================
// NkCanvasTexte.h — ecrire du texte AILLEURS QU'A LA LIGNE DE BASE
//
// A QUOI SERT CE FICHIER
//   `NkGuiDrawList::AddText` ne sait ecrire qu'a la LIGNE DE BASE : ni centrer,
//   ni caler a droite, ni tronquer proprement, ni mesurer une hauteur de ligne.
//   C'est ce manque — et lui seul — qui fait que chaque application reecrit les
//   memes quinze lignes.
//
// ⚠️ POURQUOI DES FONCTIONS LIBRES, ET PAS DES METHODES
//   Elles ont d'abord ete ecrites en METHODES PROTEGEES de NkCanvasGuiApp. Le
//   resultat s'est mesure tout seul : les trois jeux de plateau dessinent depuis
//   des fonctions libres (leur fichier d'ecran ne connait pas la classe de
//   l'application), donc aucun des trois n'a pu s'en servir — et les trois les
//   ont RE-ECRITES. Une aide rangee dans une classe n'est disponible que pour
//   ce qui herite de cette classe ; c'est une portee, pas un detail de style.
//
//   Compte au 2026-09-01 : SIX copies dans le depot (MouDraw.h, Nkoung,
//   NkGemHud.cpp, puis NkDames, NkEchecs, NkLudo). Les trois dernieres sont de
//   moi, et elles datent du jour ou j'ai range les aides au mauvais endroit.
//
// ⚠️ AUCUN .cpp — VOIR NkCanvasGuiApp.h
//   Un `.cpp` dans un module cree une dependance dure pour TOUS ses dependants.
//   Ce fichier n'en a pas, et ne doit pas en avoir.
//
// 📌 SA PLACE DEFINITIVE RESTE NKGUI, pas NKCanvas : une application qui utilise
//   NKGui SANS NKCanvas (NKCode, NK3DModeler) ne les voit toujours pas. Le jour
//   ou quelqu'un ouvre NKGui pour autre chose, ce fichier y descend sous le nom
//   NkGuiDrawText.h et celui-ci le re-exporte. Tant que ce n'est pas fait, il
//   evite au moins que le compte monte.
//
// LA CONVENTION QUI COMPTE
//   `topY` est le HAUT du texte, pas sa ligne de base. C'est la difference qui
//   oblige a ajouter Ascent(), et c'est exactement ce que les six copies
//   corrigeaient chacune de leur cote.
//   Toutes ces fonctions tolerent une police nulle : une application sans police
//   doit rester dessinable, pas planter.
// =============================================================================
#pragma once

#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
	namespace renderer {

		/// Largeur d'une chaine, en pixels. 0 si la police ou le texte manque.
		inline float32 NkTexteLargeur(nkgui::NkGuiFont *f, const char *s) noexcept {
			return (f != nullptr && s != nullptr) ? f->MeasureWidth(s) : 0.f;
		}

		/// Hauteur d'une ligne. `secours` sert quand la police n'est pas chargee —
		/// une mise en page qui divise par zero est pire qu'une mise en page fausse.
		inline float32 NkTexteHauteurLigne(nkgui::NkGuiFont *f, float32 secours) noexcept {
			return (f != nullptr && f->Face() != nullptr) ? f->LineHeight() : secours;
		}

		/// Texte cale a gauche, `topY` etant le HAUT du texte.
		/// `maxWidth >= 0` tronque proprement au lieu de deborder.
		inline void NkTexte(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 x, float32 topY, const char *s,
							const nkgui::NkColor &c, float32 maxWidth = -1.f) noexcept {
			if (f != nullptr && f->Face() != nullptr && s != nullptr) {
				dl.AddText(f->Face(), f->TexId(), math::NkVec2f(x, topY + f->Ascent()), s, c, maxWidth);
			}
		}

		/// Texte centre horizontalement sur `cx`.
		inline void NkTexteCentre(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 cx, float32 topY,
								  const char *s, const nkgui::NkColor &c) noexcept {
			NkTexte(dl, f, cx - NkTexteLargeur(f, s) * 0.5f, topY, s, c);
		}

		/// Texte cale a droite de `droite`.
		inline void NkTexteADroite(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 droite, float32 topY,
								   const char *s, const nkgui::NkColor &c) noexcept {
			NkTexte(dl, f, droite - NkTexteLargeur(f, s), topY, s, c);
		}

		/// Centre dans une boite, horizontalement ET verticalement.
		/// C'est la forme d'un libelle de bouton — celle qu'on veut neuf fois sur
		/// dix, et que personne n'ecrit juste du premier coup.
		inline void NkTexteDansBoite(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, const nkgui::NkRect &box,
									 const char *s, const nkgui::NkColor &c) noexcept {
			if (f == nullptr || f->Face() == nullptr) {
				return;
			}
			const float32 topY = box.y + (box.h - f->LineHeight()) * 0.5f;
			NkTexte(dl, f, box.x + (box.w - NkTexteLargeur(f, s)) * 0.5f, topY, s, c);
		}

	} // namespace renderer
} // namespace nkentseu
