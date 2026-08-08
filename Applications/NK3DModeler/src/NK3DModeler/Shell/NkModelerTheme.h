#pragma once
// =============================================================================
// NkModelerTheme.h — themes de NK3DModeler : roles propres, chargement disque.
//
// CE QUE CE FICHIER FAIT, ET POURQUOI IL EST ICI ET NON DANS NKEditorKit
//   1. Il ENREGISTRE les roles de couleur qui n'appartiennent qu'a ce produit.
//      L'anneau de brosse du mode sculpt n'a aucun sens pour Nogee ou NkAnima ;
//      les mettre dans l'enumeration commune de NKEditorKit ferait grossir cette
//      enumeration a chaque application, et Nogee trainerait des roles de sculpt.
//      C'est le garde-fou n.1 de NKGraph, applique aux themes.
//
//   2. Il LIT LES FICHIERS. NkThemeLibrary n'ouvre volontairement aucun fichier :
//      dependre de NKFileSystem lui ferait perdre sa propriete d'etre testable
//      sans rien lier. L'application, elle, sait ou sont ses dossiers.
//
// OU SONT LES THEMES — meme convention que les icones (data/icons.cfg), le
// premier trouve gagne :
//   1. <utilisateur>/NK3DModeler/themes/<nom>.nktheme   (surcharge personnelle)
//   2. <app>/data/themes/<nom>.nktheme                  (livre)
// Un theme utilisateur qui reprend le nom d'un theme livre le REMPLACE dans la
// liste : c'est ce qu'attend quelqu'un qui personnalise « Sombre ».
// =============================================================================

#include "NKEditorKit/NkTheme.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace nk3d {

		using namespace nkentseu::editorkit;

		// ── ROLES PROPRES A NK3DMODELER ─────────────────────────────────────────
		// Prefixes « nk3d. » : un theme portant ces lignes se charge sans erreur
		// dans une autre application, qui les comptera simplement comme inconnues.
		struct NkModelerRoles {
				uint16 brushRing = NK_ROLE_INVALID;	  ///< anneau de brosse, mode sculpt
				uint16 brushFalloff = NK_ROLE_INVALID; ///< cercle interieur d'attenuation
				uint16 maskTint = NK_ROLE_INVALID;	  ///< voile de masquage
				uint16 modifierOn = NK_ROLE_INVALID;  ///< pastille d'un modificateur actif
				uint16 modifierOff = NK_ROLE_INVALID; ///< le meme, desactive
				uint16 animatable = NK_ROLE_INVALID;  ///< parametre marque pour animation

				// ── FICHIERS PROCEDURAUX (demande de Rihen, 8 aout 2026) ────
				// Un fichier procedural ne decrit pas un resultat mais la RECETTE
				// qui le produit. C'est une nature a part entiere, et elle doit se
				// voir : les quatre graphes portaient jusqu'ici la couleur du
				// maillage, donc se confondaient avec un Model -- qui est
				// exactement ce qu'ils NE sont pas.
				//
				// UNE SEULE FAMILLE, le VIOLET, quatre nuances dedans : la famille
				// dit « c'est procedural » d'un coup d'oeil, la nuance dit lequel.
				// Le violet est le seul creneau libre du produit -- cyan, vert,
				// rose, rouge, bleu et ambre sont deja pris par maillage, materiau,
				// texture, model, scene et dossier.
				uint16 procMesh = NK_ROLE_INVALID;	 ///< graphe de modelisation
				uint16 procTex = NK_ROLE_INVALID;	 ///< graphe de texturing
				uint16 procMat = NK_ROLE_INVALID;	 ///< graphe de materiau
				uint16 procMotion = NK_ROLE_INVALID; ///< graphe de motion

				// Idempotent : appelable plusieurs fois sans consequence.
				void Register() {
					brushRing = NkRoleRegistry::Register("nk3d.anneau_brosse");
					brushFalloff = NkRoleRegistry::Register("nk3d.attenuation_brosse");
					maskTint = NkRoleRegistry::Register("nk3d.voile_masque");
					modifierOn = NkRoleRegistry::Register("nk3d.modificateur_actif");
					modifierOff = NkRoleRegistry::Register("nk3d.modificateur_inactif");
					animatable = NkRoleRegistry::Register("nk3d.parametre_animable");
					procMesh = NkRoleRegistry::Register("nk3d.procedural_modelisation");
					procTex = NkRoleRegistry::Register("nk3d.procedural_texturing");
					procMat = NkRoleRegistry::Register("nk3d.procedural_materiau");
					procMotion = NkRoleRegistry::Register("nk3d.procedural_motion");
				}

				// Valeurs par defaut, a poser sur CHAQUE theme charge. Sans elles, un
				// theme livre par NKEditorKit rendrait le magenta de « role oublie »
				// pour ces six roles -- ce qui est le comportement voulu (ca saute aux
				// yeux) mais pas ce qu'on veut voir a l'ecran.
				void ApplyDefaults(NkTheme &t) const {
					const bool dark = t.IsDark();
					t.Set(brushRing, NkTheme::FromHex(dark ? "#FFFFFFB3" : "#101010B3"));
					t.Set(brushFalloff, NkTheme::FromHex(dark ? "#FFFFFF59" : "#10101059"));
					t.Set(maskTint, NkTheme::FromHex(dark ? "#0A545E99" : "#0A545E66"));
					// L'actif reprend le BLEU d'etat d'interface, pas l'ambre : un
					// modificateur allume est un etat de l'interface, pas une selection
					// 3D. C'est la regle des deux familles (UI_SPEC 10bis.2).
					t.Set(modifierOn, t.Get(NkRole::AccentUi));
					t.Set(modifierOff, t.Get(NkRole::TextMuted));
					// L'animable reprend la couleur du type « animation » du navigateur
					// de projet : c'est la meme notion vue a deux endroits.
					t.Set(animatable, t.Get(NkRole::TypeAnim));
					// LE VIOLET DES PROCEDURAUX. En clair il faut descendre en
					// luminosite, sinon la nuance disparait sur un fond blanc --
					// meme regle que pour les axes.
					t.Set(procMesh, NkTheme::FromHex(dark ? "#A371F7" : "#7B3FD4"));
					t.Set(procTex, NkTheme::FromHex(dark ? "#8957E5" : "#5B34A8"));
					t.Set(procMat, NkTheme::FromHex(dark ? "#6E5BE8" : "#4535A8"));
					t.Set(procMotion, NkTheme::FromHex(dark ? "#C48AFF" : "#8E4FD0"));
				}
		};

		// ── CHARGEMENT ──────────────────────────────────────────────────────────
		// Remplit la bibliotheque : les deux themes livres, puis les fichiers
		// trouves. Renvoie le nombre de themes ajoutes depuis le disque.
		inline uint32 LoadThemes(NkThemeLibrary &lib, const NkModelerRoles &roles, const char *appDataDir,
								 const char *userDir) {
			// Les deux themes livres, AVEC les roles du produit poses dessus. Sans ce
			// passage, l'anneau de brosse et les cinq autres sortiraient en magenta de
			// « role oublie » : le comportement est correct, l'ecran ne l'est pas.
			{
				NkTheme d = NkTheme::Dark();
				roles.ApplyDefaults(d);
				lib.AddOrReplace(d);
				NkTheme l = NkTheme::Light();
				roles.ApplyDefaults(l);
				lib.AddOrReplace(l);
			}

			uint32 loaded = 0;
			// Les livres d'abord, la surcharge utilisateur ENSUITE : a nom egal le
			// second remplace le premier, donc l'ORDRE de ce tableau porte la priorite.
			const char *dirs[2] = {appDataDir, userDir};
			for (int32 d = 0; d < 2; ++d) {
				if (!dirs[d] || !*dirs[d] || !NkDirectory::Exists(dirs[d]))
					continue;
				// Le filtrage par motif revient a NkDirectory : le refaire a la main
				// serait l'occasion d'oublier la casse ou les fichiers sans extension.
				const NkVector<NkString> files = NkDirectory::GetFiles(dirs[d], "*.nktheme");
				for (uint32 i = 0; i < (uint32)files.Size(); ++i) {
					const NkString text = NkFile::ReadAllText(files[i].CStr());
					if (text.Size() == 0)
						continue;
					// L'ORDRE DES TROIS LIGNES SUIVANTES EST LE POINT DELICAT :
					//   base complete -> defauts du produit -> fichier.
					// Poser les defauts APRES le chargement ecraserait ce que l'auteur du
					// theme a explicitement choisi pour l'anneau de brosse. Les poser
					// avant lui laisse le dernier mot, ce qui est le sens d'un theme.
					NkTheme t = NkTheme::Dark();
					roles.ApplyDefaults(t);
					if (!t.Load(text.CStr()))
						continue;
					lib.AddOrReplace(t);
					loaded++;
				}
			}
			return loaded;
		}

	} // namespace nk3d
} // namespace nkentseu
