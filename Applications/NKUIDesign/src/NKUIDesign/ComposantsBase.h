#pragma once
// -----------------------------------------------------------------------------
// @File    ComposantsBase.h
// @Brief   LES COMPOSANTS DE BASE DE L'ATELIER — declares ET dessines.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  🔴 POURQUOI CE FICHIER, ET POURQUOI IL NE CONTIENT PAS 108 ENTREES
// =============================================================================
//  Le compte de la nuit disait : « NKGui expose 108 points d'entree de widgets,
//  le registre en declare 2 ». La conclusion evidente etait d'en declarer 106
//  de plus. **La mesure suivante l'a interdite.**
//
//  Les 108 sont des fonctions de MODE IMMEDIAT : elles prennent un
//  `NkGuiContext`, elles lisent le clavier et la souris, elles rendent un `bool`
//  au clic. Le document, lui, se dessine par `NkComponentPaint` -- une porte
//  SANS contexte, SANS entree, que le peintre enregistreur implemente aussi
//  (c'est ce qui rend les bancs et le temoin possibles).
//
//  Recherche faite dans tout le kit : **DEUX fonctions dessinent par
//  `NkComponentPaint`** (`NkDrawContentBrowser`, `NkDrawTreeView`) -- exactement
//  les deux qui sont declarees. *Le registre n'etait pas incomplet : il etait
//  EXACT vis-a-vis de ce qui savait se dessiner.*
//
//  ⚠️ DECLARER LES 106 AUTRES AURAIT PRODUIT 106 RESERVES. Le peintre du
//     document repond `DrawPlaceholder` -- « declare, dessin non branche » --
//     a tout composant qu'il ne connait pas. La palette aurait affiche cent-huit
//     noms dont cent-six auraient pose un rectangle gris portant son propre
//     aveu. *Un catalogue de 108 dont 106 refusent de se dessiner n'est pas un
//     catalogue, c'est une liste de promesses.*
//
//  Le travail reel n'est donc pas d'ENREGISTRER, c'est de DESSINER. Ce fichier
//  fait ce travail pour les composants les plus courants, un par un, avec le
//  meme vocabulaire de peintre que le reste du document.
//
// =============================================================================
//  CE QUI PEUT ETRE UN COMPOSANT DE DOCUMENT, ET CE QUI NE PEUT PAS
// =============================================================================
//  Un composant de document est un DESSIN DE DONNEES : il se peint entierement
//  a partir du nœud (ses parametres, ses remplissages, son texte) et de son
//  rectangle. Rien d'autre.
//
//  Ne peuvent PAS en etre, et c'est pour ca qu'ils ne sont pas ici :
//    - ce qui exige un ETAT D'INTERACTION (survol, focus, saisie en cours) :
//      un document n'a pas de souris. On dessine l'ETAT NORMAL, et les autres
//      etats vivent dans les blocs `appearance(Etat)` (§9) ;
//    - ce qui exige des DONNEES EXTERNES (une liste de fichiers, un arbre) :
//      ceux-la sont deja les deux composants du kit, avec leur modele ;
//    - les fonctions d'AGENCEMENT (`BeginVBox`, `BeginRow`...) : chez nous
//      l'agencement est une propriete du nœud (`NkLayoutKind`), pas un
//      composant. Les declarer aurait cree deux facons de faire la meme chose.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_COMPOSANTS_BASE_H__
#define __NKENTSEU_NKUIDESIGN_COMPOSANTS_BASE_H__

#include "NKEditorKit/Components/NkComponentDecl.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkuidesign {
	namespace basiques {

		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;
		using nkentseu::editorkit::NkComponentDecl;
		using nkentseu::editorkit::NkComponentPaint;
		using nkentseu::editorkit::NkPaintRect;
		using nkentseu::editorkit::NkParamDecl;
		using nkentseu::editorkit::NkParamKind;
		using nkentseu::editorkit::NkTextAlign;
		using nkentseu::editorkit::NkVariantDecl;

		// ═══════════════════════════════════════════════════════════════════════
		//  LES FAMILLES — des ETIQUETTES, posees des maintenant
		// ═══════════════════════════════════════════════════════════════════════
		// 🔑 Elles sont posees AVEC la declaration, pas apres : les reclasser plus
		//    tard obligerait a rouvrir chaque entree. Et ce sont les memes
		//    etiquettes que le chapitre ③ (types et filiation) emploiera -- un
		//    composant de document qui derive d'un `bouton` heritera de « widget »
		//    par la filiation, sans qu'on le lui redise.
		enum class NkFamille : nkentseu::uint8 { Widget, Conteneur, Affichage };

		inline const char *NkNomFamille(NkFamille f) {
			switch (f) {
				case NkFamille::Conteneur: return "conteneur";
				case NkFamille::Affichage: return "affichage";
				default: return "widget";
			}
		}

		/// La table des composants de base : nom stable, famille, resume.
		/// ⚠️ UNE SEULE TABLE, lue par la declaration, par le dessin ET par le
		///    banc. Trois listes se seraient desaccordees au premier ajout.
		struct NkBasique {
				const char *nom;
				const char *titre;
				NkFamille famille;
				const char *resume;
				/// LA TAILLE NATURELLE, en unites document.
				/// 🔑 ELLE COMBLE UN MANQUE NOMME. `NkUIDocument::InitNode` refuse
				///    d'inventer une taille -- a juste titre : elle n'etait declaree
				///    nulle part. Elle l'est desormais ICI, par l'auteur de ces
				///    composants, ce qui n'est pas la meme chose qu'un nombre pose
				///    « parce que ca rend bien » dans le code de pose.
				/// ⚠️ SANS ELLE, UN COMPOSANT POSE DANS UN PARENT LIBRE EST
				///    INVISIBLE : il nait en `expand`, et `expand` dans une toile
				///    libre n'a rien contre quoi s'etendre -- il resout a zero.
				///    Mesure a la souris le 03/09 (position 0,0, taille nulle).
				float32 larg;
				float32 haut;
		};

		inline const NkBasique *NkTableBasiques(uint32 &nb) {
			static const NkBasique k[] = {
				{"bouton", "Bouton", NkFamille::Widget,
				 "une boîte et un libellé centré — la variante porte l'accent",
				 120.f, 36.f},
				{"champ_texte", "Champ de texte", NkFamille::Widget,
				 "une boîte creuse et son texte indicatif",
				 180.f, 36.f},
				{"case_a_cocher", "Case à cocher", NkFamille::Widget,
				 "un carré, sa coche quand elle est mise, et son libellé",
				 160.f, 24.f},
				{"interrupteur", "Interrupteur", NkFamille::Widget,
				 "une pilule et son galet — à gauche ou à droite selon l'état",
				 44.f, 24.f},
				{"barre_progression", "Barre de progression", NkFamille::Affichage,
				 "une piste et sa portion remplie (paramètre « valeur », 0..100)",
				 180.f, 8.f},
				{"etiquette", "Étiquette", NkFamille::Affichage,
				 "du texte seul, sans boîte",
				 120.f, 20.f},
				{"separateur", "Séparateur", NkFamille::Affichage,
				 "un filet horizontal à mi-hauteur",
				 180.f, 1.f},
				{"carte", "Carte", NkFamille::Conteneur,
				 "une surface bordée et arrondie qui reçoit d'autres nœuds",
				 240.f, 160.f},
			};
			nb = (uint32)(sizeof(k) / sizeof(k[0]));
			return k;
		}

		/// L'entree de la table pour ce nom, ou nul.
		inline const NkBasique *NkBasiqueDe(const char *nom) {
			if (!nom || !*nom)
				return nullptr;
			uint32 nb = 0;
			const NkBasique *t = NkTableBasiques(nb);
			for (uint32 i = 0; i < nb; ++i)
				if (NkComponentDecl::StrEq(t[i].nom, nom))
					return &t[i];
			return nullptr;
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LES DECLARATIONS
		// ═══════════════════════════════════════════════════════════════════════
		// ⚠️ LES PARAMETRES SONT PARTAGES quand ils veulent dire la meme chose :
		//    `valeur` sert a la barre comme il servira au curseur. Un second
		//    `progression` a cote aurait double le vocabulaire pour rien.
		inline const NkParamDecl *NkParamsBasique(const char *nom, nkentseu::uint16 &nb) {
			static const NkParamDecl kValeur[] = {
				{"valeur", "Valeur", NkParamKind::Float, 40.f, 0.f, 100.f, nullptr, 0},
			};
			static const NkParamDecl kCoche[] = {
				{"coche", "Cochee", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
			};
			static const NkParamDecl kActif[] = {
				{"actif", "Actif", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
			};
			if (NkComponentDecl::StrEq(nom, "barre_progression")) {
				nb = 1;
				return kValeur;
			}
			if (NkComponentDecl::StrEq(nom, "case_a_cocher")) {
				nb = 1;
				return kCoche;
			}
			if (NkComponentDecl::StrEq(nom, "interrupteur")) {
				nb = 1;
				return kActif;
			}
			nb = 0;
			return nullptr;
		}

		inline const NkVariantDecl *NkVariantesBasique(const char *nom, nkentseu::uint16 &nb) {
			static const NkVariantDecl kBouton[] = {
				{"primaire", "Primaire", "fond d'accent, texte sur accent"},
				{"secondaire", "Secondaire", "fond de surface, bord visible"},
				{"fantome", "Fantome", "aucun fond, le libelle seul"},
			};
			if (NkComponentDecl::StrEq(nom, "bouton")) {
				nb = 3;
				return kBouton;
			}
			nb = 0;
			return nullptr;
		}

		/// La declaration complete du composant `i` de la table.
		///
		/// 🔴 ELLE REND UNE REFERENCE VERS UN STATIQUE, ET C'EST OBLIGATOIRE :
		///    `NkComponentRegistry::Register` fait `gItems[gCount++] = &d;` --
		///    **il garde un POINTEUR, pas une copie.** Ma premiere version rendait
		///    la declaration PAR VALEUR : le registre pointait donc sur un
		///    temporaire mort a la ligne suivante, et toute lecture ulterieure
		///    tombait sur de la memoire liberee. Symptome : `--recette-gestes` en
		///    **defaut de segmentation**, a un endroit qui BOUGEAIT d'une
		///    execution a l'autre -- la signature d'une duree de vie, jamais
		///    celle d'une logique fausse.
		///
		/// ⚠️ ET LA SIGNATURE NE LE DIT PAS : `Register(const NkComponentDecl &)`
		///    a l'air de prendre une copie. Les deux composants du kit rendent
		///    des references statiques (`NkTreeViewDecl`), donc le contrat tenait
		///    par l'exemple et non par le type. *Un contrat qui ne vit que dans
		///    l'exemple se casse au premier qui n'a pas lu l'exemple.* Signale au
		///    canal ; ici, on s'y conforme.
		inline const NkComponentDecl &NkDeclBasique(uint32 i) {
			static NkComponentDecl kDecls[16];
			static bool kPret = false;
			uint32 nbT = 0;
			const NkBasique *tt = NkTableBasiques(nbT);
			if (!kPret) {
				kPret = true;
				for (uint32 j = 0; j < nbT && j < 16u; ++j) {
					NkComponentDecl &e = kDecls[j];
					e.name = tt[j].nom;
					e.title = tt[j].titre;
					e.summary = tt[j].resume;
					e.role = NkNomFamille(tt[j].famille);
					nkentseu::uint16 n16 = 0;
					e.params = NkParamsBasique(tt[j].nom, n16);
					e.paramCount = n16;
					e.variants = NkVariantesBasique(tt[j].nom, n16);
					e.variantCount = n16;
				}
			}
			static const NkComponentDecl kVide;
			if (i >= nbT || i >= 16u)
				return kVide;
			return kDecls[i];
		}

		/// LA TAILLE NATURELLE d'un composant de base. Rend `false` si le nom
		/// n'est pas des notres -- un composant du kit n'en declare pas, et
		/// l'appelant doit alors se debrouiller AUTREMENT que par un nombre
		/// invente (voir le site de pose : il derive du parent).
		inline bool NkTailleNaturelle(const char *nom, float32 &w, float32 &h) {
			const NkBasique *b = NkBasiqueDe(nom);
			if (!b || b->larg <= 0.f || b->haut <= 0.f)
				return false;
			w = b->larg;
			h = b->haut;
			return true;
		}

		/// La valeur du PREMIER parametre declare, ou 0.
		/// 🔑 ELLE SE LIT DE LA DECLARATION, elle ne se recopie pas : le defaut
		///    d'un composant est ecrit une fois, dans `NkParamsBasique`. Un second
		///    nombre ici aurait diverge au premier ajustement -- c'est le defaut
		///    que ce chantier a paye tout le week-end.
		inline float32 NkValeurParDefaut(const char *nom) {
			nkentseu::uint16 nb = 0;
			const NkParamDecl *p = NkParamsBasique(nom, nb);
			return (p && nb > 0u) ? p[0].defVal : 0.f;
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LE DESSIN — par `NkComponentPaint`, la porte du document
		// ═══════════════════════════════════════════════════════════════════════
		// ⚠️ AUCUN ETAT D'INTERACTION ICI : on dessine l'etat NORMAL. Le survol,
		//    le focus et le presse vivent dans les blocs `appearance(Etat)` du
		//    nœud (§9) — c'est deja le mecanisme du format, et le peintre du
		//    document n'a ni souris ni clavier a interroger.
		//
		/// @param valeur     le parametre principal (0..100, ou 0/1 pour un booleen)
		/// @param libelle    le texte du nœud, deja resolu par l'appelant
		/// @param variante   l'index de variante du nœud
		/// @return vrai si `nom` est un composant de base et a ete dessine.
		inline bool NkDessinerBasique(NkComponentPaint &p, const NkPaintRect &r, const char *nom,
									  float32 valeur, const char *libelle, uint32 variante,
									  nkentseu::uint16 rAccent, nkentseu::uint16 rSurAccent,
									  nkentseu::uint16 rSurface, nkentseu::uint16 rBord,
									  nkentseu::uint16 rTexte, nkentseu::uint16 rAttenue,
									  float32 rayon, float32 echelle) {
			// LE CORPS DU TEXTE, EN UNITES DOCUMENT MISES A L'ECHELLE.
			// ⚠️ 12 px est le corps par defaut du document (le meme que le repli
			//    de `Renderers.h`, l. ~1179) -- pas un nombre choisi ici.
			const float32 corps = 12.f * (echelle > 0.f ? echelle : 1.f);
			if (!NkBasiqueDe(nom) || r.w <= 0.f || r.h <= 0.f)
				return false;

			if (NkComponentDecl::StrEq(nom, "bouton")) {
				// variante 0 = primaire (fond accent), 1 = secondaire, 2 = fantome
				if (variante == 0u)
					p.Fill(r, rAccent, rayon);
				else if (variante == 1u)
					// ⚠️ UN SEUL APPEL, ET IL SUIT LE RAYON. C'etait
					//    `Fill(rayon)` + `OutlineSharp` : le fond s'arrondissait,
					//    le trait restait carre -- le defaut meme que Rodolf a
					//    photographie, que j'ai reproduit ici le lendemain.
					p.Outline(r, rBord, rSurface, rayon);
				const nkentseu::uint16 encre = (variante == 0u) ? rSurAccent : rTexte;
				p.TextHex(r, (libelle && *libelle) ? libelle : "Bouton", p.ColorOf(encre), encre,
						  NkTextAlign::Center, corps, 0.f);
				return true;
			}
			if (NkComponentDecl::StrEq(nom, "champ_texte")) {
				p.Outline(r, rBord, rSurface, rayon);
				NkPaintRect t = r;
				t.x += 8.f;
				t.w -= 16.f;
				p.TextHex(t, (libelle && *libelle) ? libelle : "Texte…", p.ColorOf(rAttenue),
						  rAttenue, NkTextAlign::Left, corps, 0.f);
				return true;
			}
			if (NkComponentDecl::StrEq(nom, "case_a_cocher")) {
				const float32 c = r.h < 16.f ? r.h : 16.f;
				const NkPaintRect b{r.x, r.y + (r.h - c) * 0.5f, c, c};
				if (valeur > 0.5f) {
					p.Fill(b, rAccent, 3.f);
					// la coche : deux traits, comme partout ailleurs dans l'atelier
					p.Line(b.x + c * 0.24f, b.y + c * 0.52f, b.x + c * 0.44f, b.y + c * 0.72f,
						   rSurAccent, 1.6f);
					p.Line(b.x + c * 0.44f, b.y + c * 0.72f, b.x + c * 0.78f, b.y + c * 0.28f,
						   rSurAccent, 1.6f);
				} else
					// La boite VIDE : un anneau, pas un rectangle net. Le rayon
					// de la case suit celui du noeud, borne par sa propre taille.
					p.Outline(b, rBord, rSurface,
							  rayon < c * 0.5f ? rayon : c * 0.5f);
				NkPaintRect t = r;
				t.x += c + 8.f;
				t.w -= c + 8.f;
				if (t.w > 0.f)
					p.TextHex(t, (libelle && *libelle) ? libelle : "Case à cocher",
							  p.ColorOf(rTexte), rTexte, NkTextAlign::Left, corps, 0.f);
				return true;
			}
			if (NkComponentDecl::StrEq(nom, "interrupteur")) {
				const float32 h = r.h < 20.f ? r.h : 20.f;
				const float32 w = h * 1.8f;
				const NkPaintRect piste{r.x, r.y + (r.h - h) * 0.5f, w, h};
				const bool actif = valeur > 0.5f;
				p.Fill(piste, actif ? rAccent : rSurface, h * 0.5f);
				if (!actif)
					// La piste est deja peinte en demi-hauteur : son contour doit
					// l'etre aussi, sinon le trait coupe les deux bouts ronds.
					p.Outline(piste, rBord, rSurface, h * 0.5f);
				const float32 d = h - 4.f;
				const NkPaintRect galet{actif ? (piste.x + w - d - 2.f) : (piste.x + 2.f),
										piste.y + 2.f, d, d};
				p.Fill(galet, actif ? rSurAccent : rAttenue, d * 0.5f);
				return true;
			}
			if (NkComponentDecl::StrEq(nom, "barre_progression")) {
				const float32 h = r.h < 8.f ? r.h : 8.f;
				const NkPaintRect piste{r.x, r.y + (r.h - h) * 0.5f, r.w, h};
				p.Fill(piste, rSurface, h * 0.5f);
				float32 k = valeur * 0.01f;
				if (k < 0.f)
					k = 0.f;
				if (k > 1.f)
					k = 1.f;
				if (k > 0.f) {
					const NkPaintRect rempli{piste.x, piste.y, piste.w * k, h};
					p.Fill(rempli, rAccent, h * 0.5f);
				}
				return true;
			}
			if (NkComponentDecl::StrEq(nom, "etiquette")) {
				p.TextHex(r, (libelle && *libelle) ? libelle : "Étiquette", p.ColorOf(rTexte), rTexte,
						  NkTextAlign::Left, corps, 0.f);
				return true;
			}
			if (NkComponentDecl::StrEq(nom, "separateur")) {
				p.HLine(r.x, r.y + r.h * 0.5f, r.w, rBord);
				return true;
			}
			if (NkComponentDecl::StrEq(nom, "carte")) {
				p.Outline(r, rBord, rSurface, rayon > 0.f ? rayon : 8.f);
				return true;
			}
			return false;
		}

	} // namespace basiques
} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_COMPOSANTS_BASE_H__
