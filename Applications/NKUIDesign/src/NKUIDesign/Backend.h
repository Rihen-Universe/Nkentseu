#pragma once
// -----------------------------------------------------------------------------
// @File    Backend.h
// @Brief   LE CHOIX DU BACKEND GRAPHIQUE, sorti de `main` pour devenir MESURABLE.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE — un defaut qui s'est vu dans un journal reel
// =============================================================================
//  La directive de Rodolf (2026-08-19) demande que chaque application expose le
//  choix de son backend et **le journalise au demarrage : demande / retenu /
//  pourquoi**. La premiere version vivait entierement dans `main.cpp`. Elle a
//  tourne une fois, sur GPU, et a produit exactement ceci :
//
//      [NKUIDesign] backend graphique demande : {} (source : {})
//      [NKUIDesign] coquille initialisee, backend ? {} ?, fenetre {}x{}.
//
//  **La trace existait et ne disait RIEN.** Deux causes cumulees :
//    1. `logger.Infof` est la famille **printf** (`%s`), pas la famille a
//       accolades — cf. `NkLogger.h`, section « Style printf ... via NkFormatf » ;
//    2. meme du bon cote, les accolades NUES `{}` ne sont pas un jeton : le
//       formatage est **indexe**, `{0}` `{1}`, et un jeton non resolu est
//       **laisse tel quel** — c'est un comportement TESTE en propre
//       (`test_indexed_format.cpp`, `KeepsUnknownPlaceholderWhenIndexMissing`).
//
//  C'est le defaut que la regle 2 vise exactement : on croit mesurer un backend
//  et on ne mesure rien. Il n'etait invisible que parce que la sonde ne peut pas
//  ouvrir de fenetre — donc le seul code que le GPU touchait etait le seul code
//  sans temoin. **La reponse n'est pas de mieux ecrire la ligne : c'est de la
//  rendre calculable SANS fenetre**, pour qu'un essai headless la lise.
//
//  D'ou ce fichier : la resolution est une fonction PURE (pas d'entree/sortie,
//  pas de GPU), `main` ne fait plus que l'appeler et journaliser son resultat,
//  et `--probe` verifie la MEME fonction. La ligne de journal est desormais
//  assemblee ici, sans formateur du tout — une concatenation ne peut pas
//  « ne pas substituer ».
//
// =============================================================================
//  CE QUE « RETENU » VEUT DIRE, ET POURQUOI CE N'EST PAS « DEMANDE »
// =============================================================================
//  `auto` n'est pas une API : c'est une DELEGATION. Sur Windows, la coquille la
//  resout en **DX11** (`NkEditorCanvasRenderer::Init`, branche `default`),
//  ailleurs en OpenGL. Journaliser « demande : auto » et s'arreter la laisse
//  croire qu'on a mesure « auto » — alors qu'on a mesure DX11. Le champ
//  `effective` porte donc le nom CONCRET, toujours, et il n'est jamais `auto`.
//
//  ⚠️ PORTEE — ce que ce fichier ne fait PAS, et qui part au canal :
//    - il ne rend pas le choix UNIFORME entre applications (regle 1). Le nom
//      d'option et la variable d'environnement reprennent le patron NKXR, mais
//      tant que le code vit DANS une application, la suivante le reecrira. Sa
//      place est NKEditorKit ou NKWindow — pas a moi de deplacer ;
//    - `metal` est accepte a l'ANALYSE et refuse a la RESOLUTION : l'enumeration
//      `NkEditorGfxApi` (`NkIEditorRenderer.h`) n'a **pas d'entree Metal**. Le
//      taire lancerait silencieusement autre chose sur macOS — exactement le
//      repli muet que la regle 3 interdit. Manque porte au canal.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentDecl.h"
#include "NKEditorKit/NkIEditorRenderer.h"

namespace nkuidesign {

	using namespace nkentseu;
	using namespace nkentseu::editorkit;

	// D'ou vient la valeur. Sert au journal : « demande X, source Y ».
	// L'ordre de priorite croissante est celui de l'enumeration.
	enum class NkGfxSource : uint8 { DetectionAuto = 0, Environnement, LigneDeCommande };

	inline const char *NkGfxSourceName(NkGfxSource s) {
		switch (s) {
			case NkGfxSource::LigneDeCommande:
				return "ligne de commande --gfx";
			case NkGfxSource::Environnement:
				return "variable d'environnement NK_GFX_API";
			default:
				return "detection automatique (aucune option, aucune variable)";
		}
	}

	// Le resultat de la resolution. `supported == false` => on ne lance PAS.
	struct NkGfxChoice {
			const char *requested = "auto";			   // ce que l'humain a ecrit
			NkGfxSource source = NkGfxSource::DetectionAuto;
			NkEditorGfxApi api = NkEditorGfxApi::Auto; // ce que la coquille recevra
			const char *effective = "";				   // le nom CONCRET, jamais "auto"
			bool supported = true;
			const char *reason = ""; // rempli seulement si `supported == false`
	};

	// Le nom concret d'une API resolue. `Auto` n'en est pas une : elle est
	// resolue par `NkGfxResolveEffective`, pas ici.
	inline const char *NkGfxApiName(NkEditorGfxApi api) {
		switch (api) {
			case NkEditorGfxApi::OpenGL:
				return "opengl";
			case NkEditorGfxApi::Vulkan:
				return "vulkan";
			case NkEditorGfxApi::DX11:
				return "dx11";
			case NkEditorGfxApi::DX12:
				return "dx12";
			case NkEditorGfxApi::Software:
				return "software";
			default:
				return "auto";
		}
	}

	// Ce que `Auto` DEVIENT reellement, en miroir de la branche `default` de
	// `NkEditorCanvasRenderer::Init`.
	// ⚠️ CE MIROIR EST UNE DEPENDANCE : si la coquille change son defaut, cette
	//    ligne ment. L'essai 31f l'ancre a une valeur concrete et non vide ;
	//    l'accord exact avec la coquille ne se verifie qu'a l'ecran, et c'est dit
	//    plutot que suppose.
	inline const char *NkGfxResolveEffective(NkEditorGfxApi api) {
		if (api != NkEditorGfxApi::Auto)
			return NkGfxApiName(api);
#if defined(NKENTSEU_PLATFORM_WINDOWS)
		return "dx11";
#else
		return "opengl";
#endif
	}

	// Analyse UNE valeur. Rend `false` si la chaine est vide/nulle (« rien
	// demande »), ce qui n'est pas la meme chose qu'une valeur refusee : une
	// variable d'environnement vide ne doit pas ecraser quoi que ce soit.
	inline bool NkGfxParse(const char *value, NkGfxChoice &out) {
		if (!value || !*value)
			return false;
		out.requested = value;
		out.supported = true;
		out.reason = "";
		if (NkComponentDecl::StrEq(value, "auto"))
			out.api = NkEditorGfxApi::Auto;
		else if (NkComponentDecl::StrEq(value, "opengl"))
			out.api = NkEditorGfxApi::OpenGL;
		else if (NkComponentDecl::StrEq(value, "vulkan"))
			out.api = NkEditorGfxApi::Vulkan;
		else if (NkComponentDecl::StrEq(value, "dx11"))
			out.api = NkEditorGfxApi::DX11;
		else if (NkComponentDecl::StrEq(value, "dx12"))
			out.api = NkEditorGfxApi::DX12;
		else if (NkComponentDecl::StrEq(value, "software"))
			out.api = NkEditorGfxApi::Software;
		else if (NkComponentDecl::StrEq(value, "metal")) {
			out.supported = false;
			out.reason = "l'enumeration NkEditorGfxApi de NKEditorKit n'a pas d'entree Metal ; "
						 "lancer autre chose a sa place serait un repli silencieux";
		} else {
			out.supported = false;
			out.reason = "valeur inconnue (attendu : auto|opengl|vulkan|dx11|dx12|metal|software)";
		}
		out.effective = out.supported ? NkGfxResolveEffective(out.api) : "";
		return true;
	}

	// Extrait la valeur d'un argument `--gfx=X`. Rend nullptr si ce n'est pas
	// cet argument. Sortie separee de l'analyse pour que l'essai puisse verifier
	// la RECONNAISSANCE de l'option independamment de la validite de sa valeur.
	inline const char *NkGfxArgValue(const char *arg) {
		static const char kPrefix[] = "--gfx=";
		if (!arg)
			return nullptr;
		for (uint32 i = 0; i < 6; ++i)
			if (arg[i] != kPrefix[i])
				return nullptr;
		return arg + 6;
	}

	// LA resolution complete : detection automatique, puis environnement, puis
	// ligne de commande. L'ordre est celui qu'on attend, et il est verifie dans
	// LES DEUX SENS par l'essai 31e — sans quoi « la ligne de commande gagne »
	// passerait aussi si l'environnement etait purement ignore.
	inline NkGfxChoice NkGfxResolve(const char *env, const char *const *args, uint32 argCount) {
		NkGfxChoice c;
		c.effective = NkGfxResolveEffective(c.api);
		if (NkGfxParse(env, c))
			c.source = NkGfxSource::Environnement;
		for (uint32 i = 0; i < argCount; ++i) {
			const char *v = NkGfxArgValue(args ? args[i] : nullptr);
			if (v && NkGfxParse(v, c))
				c.source = NkGfxSource::LigneDeCommande;
		}
		return c;
	}

	// LA LIGNE DE JOURNAL, assemblee sans formateur.
	// ⚠️ C'est delibere : le defaut d'origine etait un formateur qui n'a pas
	//    substitue et l'a taire. Une concatenation ne peut pas echouer en
	//    silence — et l'essai 31a verifie qu'il ne reste AUCUNE accolade dans la
	//    sortie, ce qui rattraperait un retour au formateur.
	inline NkString NkGfxJournalLine(const NkGfxChoice &c) {
		NkString s("[NKUIDesign] backend graphique -- demande : ");
		s.Append(c.requested);
		s.Append("  |  source : ");
		s.Append(NkGfxSourceName(c.source));
		if (c.supported) {
			s.Append("  |  retenu : ");
			s.Append(c.effective);
			s.Append("  |  pourquoi : ");
			s.Append(NkComponentDecl::StrEq(c.requested, "auto")
						 ? "aucune API forcee, la coquille applique son defaut de plateforme"
						 : "API forcee explicitement, aucun repli n'est autorise");
		} else {
			s.Append("  |  RETENU : AUCUN, lancement refuse  |  pourquoi : ");
			s.Append(c.reason);
		}
		return s;
	}

} // namespace nkuidesign
