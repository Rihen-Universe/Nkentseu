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
//  La directive de Rodolf (2026-08-18) demande que chaque application expose le
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
#include "NKFileSystem/NkFile.h"

namespace nkuidesign {

	using namespace nkentseu;
	using namespace nkentseu::editorkit;

	// D'ou vient la valeur. Sert au journal : « demande X, source Y ».
	// L'ordre de priorite croissante est celui de l'enumeration.
	// ⚠️ L'ORDRE DE L'ENUMERATION EST L'ORDRE DE PRIORITE CROISSANTE, et
	//    `FichierDeConfig` s'insere ENTRE la detection et l'environnement : le
	//    fichier est DURABLE (il survit a la session) mais il est le moins local
	//    des trois reglages explicites. Une variable posee pour une session doit
	//    pouvoir passer devant lui, une option de ligne de commande devant les deux.
	enum class NkGfxSource : uint8 {
		DetectionAuto = 0,
		FichierDeConfig,
		Environnement,
		LigneDeCommande
	};

	inline const char *NkGfxSourceName(NkGfxSource s) {
		switch (s) {
			case NkGfxSource::LigneDeCommande:
				return "ligne de commande --gfx";
			case NkGfxSource::Environnement:
				return "variable d'environnement NK_GFX_API";
			case NkGfxSource::FichierDeConfig:
				return "fichier de configuration nkuidesign.cfg (cle gfx)";
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

			// ⚠️ LE REPLI QUI NE VERROUILLE PAS L'UTILISATEUR DEHORS (regle du
			//    18/08). Deux fautes symetriques a eviter : remplacer en SILENCE un
			//    reglage indisponible, et REFUSER DE DEMARRER parce que la config
			//    en nomme un. La seconde est la pire : l'utilisateur ne peut plus
			//    atteindre les Preferences pour corriger sa propre erreur — il est
			//    enferme dehors par le reglage qu'il voulait changer.
			//
			//    Donc : quand c'est le FICHIER qui nomme l'indisponible, on demarre
			//    sur le repli, on le CRIE, et **on ne reecrit pas la config** — le
			//    reglage fautif reste visible la ou il a ete pose.
			//
			//    ⚠️ ET SEULEMENT POUR LE FICHIER. Une option `--gfx` ou une variable
			//    d'environnement ne verrouillent personne : on relance sans elles.
			//    Y appliquer le repli ferait mesurer sur autre chose que ce qu'on a
			//    demande, ce que la regle 3 de Rodolf interdit. Deux sources, deux
			//    conduites, et c'est la portee qui les separe — pas le confort.
			bool fellBack = false;		 ///< a-t-on demarre sur le repli ?
			const char *refused = "";	 ///< ce que la config demandait, et qu'on n'a pas
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

	// ── LE FICHIER DE CONFIGURATION ─────────────────────────────────────────
	// Directive de Rodolf (2026-08-18) : *« par defaut, sans avoir a faire ce
	// probe, meme si c'est une option, l'application peut lire le fichier de
	// config »*. La chaine complete est **Preferences (interface) -> fichier de
	// config -> lu par defaut au demarrage**, et le fichier est la source
	// DURABLE, la seule des quatre qui survive a la session.
	//
	// ⚠️⚠️ LA SONDE LIT LE MEME FICHIER QUE L'APPLICATION, et ce n'est pas une
	//      precaution de style : c'est la lecon du magenta, transposee. Si
	//      `--probe` gardait sa propre resolution pendant que `main` lit un
	//      fichier, on aurait **deux sources de verite pour une meme chose** — et
	//      la sonde passerait verte sur un programme qui se lance sur autre chose.
	//      La resolution reste donc UNE fonction pure, `NkGfxResolve`, a qui l'on
	//      PASSE la valeur lue ; le fichier entre par un argument, jamais par un
	//      acces cache au disque au milieu de la resolution.
	//
	// ⚠️ FORMAT : une cle par ligne, `cle = valeur`, `#` en commentaire. Le meme
	//    esprit que les fichiers de theme du kit — lisible, comparable par
	//    `git diff`, editable a la main. Ce n'est pas un format neuf a apprendre.
	//
	// ⚠️ OU IL VIT, ET CE QU'IL NE TOUCHE PAS. `nkuidesign.cfg`, a cote de
	//    l'executable. **Il n'ecrit PAS dans `~/.nkcode_*.cfg`** : la coquille lit
	//    deja ces fichiers-la inconditionnellement (`NkLoadTheme`,
	//    `NkLoadFontPrefs`), donc NkUIDesign en HERITE — mais NKCode est en pause
	//    depuis des semaines et il fonctionne. Ecrire dedans pour regler une autre
	//    application casserait l'etat d'une application au repos : un cout pur.
	//    Arbitrage de Rodolf : « pas besoin de nom descriptif », on ne renomme
	//    rien et la direction « configs globales par type d'application » se
	//    tranchera plus tard.
	inline const char *NkGfxConfigPath() {
		return "nkuidesign.cfg";
	}

	/// Lit la valeur d'une cle dans un texte de configuration. PURE : le texte est
	/// passe, pas lu ici — c'est ce qui permet a la sonde d'exercer l'analyse sans
	/// toucher au disque, et donc de tester les cas tordus (cle absente, valeur
	/// vide, commentaire, espaces) qu'un fichier reel ne contiendrait jamais tous.
	///
	/// Rend `nullptr` si la cle est absente ou sa valeur vide. ⚠️ « absente » et
	/// « vide » rendent la MEME chose, et c'est voulu : une cle posee sans valeur
	/// ne doit pas ecraser la detection automatique par une chaine vide.
	inline bool NkGfxConfigValue(const char *text, const char *key, char *out, uint32 cap) {
		if (!text || !key || !out || cap == 0)
			return false;
		out[0] = 0;
		const char *p = text;
		while (*p) {
			// Debut de ligne : on saute les blancs.
			while (*p == ' ' || *p == '\t')
				++p;
			if (*p == '#') { // commentaire : ligne ignoree
				while (*p && *p != '\n')
					++p;
				if (*p)
					++p;
				continue;
			}
			// La cle, comparee caractere a caractere.
			const char *k = key;
			const char *q = p;
			while (*k && *q && *q == *k) {
				++k;
				++q;
			}
			bool match = (*k == 0);
			if (match) {
				while (*q == ' ' || *q == '\t')
					++q;
				match = (*q == '=');
			}
			if (match) {
				++q; // le '='
				while (*q == ' ' || *q == '\t')
					++q;
				uint32 n = 0;
				while (*q && *q != '\n' && *q != '\r' && *q != '#' && n + 1 < cap)
					out[n++] = *q++;
				// On retire les blancs de fin : « opengl   » et « opengl » sont la
				// meme valeur, et un espace invisible qui change le backend serait
				// exactement le genre de defaut qu'on ne relie pas a sa cause.
				while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t'))
					--n;
				out[n] = 0;
				return n > 0;
			}
			while (*p && *p != '\n')
				++p;
			if (*p)
				++p;
		}
		return false;
	}

	// ── UN FICHIER QU'ON N'A PAS SU LIRE SE DIT, ET NE S'ECRASE PAS ─────────
	// ⚠️ TROIS ETATS, ET C'EST LE TROISIEME QUI COMPTE. « Fichier absent » et
	//    « fichier present dont on ne tire rien » se comportaient pareil —
	//    repli silencieux sur la detection — et c'est un repli MUET, celui que
	//    la regle 2 de Rodolf interdit. Or les deux situations n'ont rien a voir :
	//    l'absence est normale, l'illisible veut dire que **quelqu'un a ecrit
	//    quelque chose et que le programme n'en a rien fait**.
	//
	// ⚠️ ET SURTOUT : ON NE REECRIT PAS un fichier qu'on n'a pas su lire. Il a pu
	//    etre edite a la main ; le « reparer » en le regenerant detruirait le
	//    travail de quelqu'un pour corriger une erreur qu'il verra tout seul des
	//    qu'on la lui aura dite.
	enum class NkGfxConfigState : uint8 {
		Absent = 0,		 ///< pas de fichier : cas normal, rien a signaler
		CleLue,			 ///< fichier present, cle `gfx` lue
		PresentSansCle	 ///< fichier present et INEXPLOITABLE : a dire, jamais a reparer
	};

	inline NkGfxConfigState NkGfxConfigClassify(bool exists, const char *text, char *out,
											   uint32 cap) {
		if (out && cap)
			out[0] = 0;
		if (!exists)
			return NkGfxConfigState::Absent;
		return NkGfxConfigValue(text, "gfx", out, cap) ? NkGfxConfigState::CleLue
													  : NkGfxConfigState::PresentSansCle;
	}

	inline const char *NkGfxConfigStateMessage(NkGfxConfigState st) {
		switch (st) {
			case NkGfxConfigState::PresentSansCle:
				return "[NKUIDesign] le fichier nkuidesign.cfg existe mais n'a pas livre de cle "
					   "'gfx' lisible -- demarrage sur la detection automatique. Le fichier n'a PAS "
					   "ete modifie : s'il a ete edite a la main, rien n'est perdu.";
			case NkGfxConfigState::CleLue:
				return "";
			default:
				return "";
		}
	}

	// ── ECRIRE LA CONFIGURATION ─────────────────────────────────────────────
	// Directive de Rodolf : *« il faut pouvoir la changer depuis l'interface
	// aussi »*. La chaine se referme ici — **Preferences -> fichier -> demarrage**
	// — et l'ecriture est la moitie qui manquait.
	//
	// ⚠️ TROIS EXIGENCES, ET AUCUNE N'EST DU CONFORT :
	//
	//  1. **NE JAMAIS DETRUIRE CE QU'ON N'A PAS ECRIT.** Le fichier peut avoir ete
	//     edite a la main. On ne le REGENERE pas : on remplace **la ligne de la
	//     cle**, on garde commentaires, ordre, et toutes les autres cles. Un
	//     `Enregistrer` qui reecrirait le fichier entier effacerait le travail de
	//     quelqu'un sans qu'un seul message le signale.
	//  2. **NE JAMAIS LAISSER UN FICHIER A MOITIE ECRIT.** Ecriture dans un
	//     temporaire, puis **renommage**. Une coupure de courant pendant un
	//     `Enregistrer` doit couter le REGLAGE, jamais la CONFIGURATION.
	//     ⚠️ Et le renommage n'est atomique que si les deux chemins sont sur le
	//     meme volume : le temporaire est donc **a cote du fichier**, jamais dans
	//     un dossier temporaire du systeme.
	//  3. **LA PRODUCTION DU TEXTE EST PURE.** `NkGfxConfigSet` ne touche pas au
	//     disque : elle prend l'ancien texte et rend le nouveau. C'est ce qui
	//     permet a la sonde d'exercer les cas qu'un fichier reel ne contiendrait
	//     jamais tous — cle absente, cle en double, fichier vide, texte tronque —
	//     sans ecrire un octet. Meme discipline que la resolution.

	/// Produit le NOUVEAU texte de configuration : `key = value`, en preservant
	/// tout le reste. Remplace la **premiere** occurrence non commentee de la cle ;
	/// si elle est absente, ajoute la ligne a la fin.
	///
	/// ⚠️ « premiere occurrence » et pas « toutes » : c'est la premiere que
	///    `NkGfxConfigValue` lit, donc la seule qui compte. En reecrire d'autres
	///    ferait diverger ce qu'on ecrit de ce qu'on relira.
	inline void NkGfxConfigSet(const char *text, const char *key, const char *value, NkString &out) {
		out = NkString("");
		bool remplace = false;
		const char *p = text ? text : "";
		while (*p) {
			const char *debut = p;
			const char *fin = p;
			while (*fin && *fin != '\n')
				++fin;
			const char *apres = *fin ? fin + 1 : fin;

			// La ligne porte-t-elle la cle, hors commentaire ?
			const char *q = debut;
			while (q < fin && (*q == ' ' || *q == '\t'))
				++q;
			bool porteLaCle = false;
			if (q < fin && *q != '#') {
				const char *k = key;
				const char *r = q;
				while (*k && r < fin && *r == *k) {
					++k;
					++r;
				}
				if (*k == 0) {
					while (r < fin && (*r == ' ' || *r == '\t'))
						++r;
					porteLaCle = (r < fin && *r == '=');
				}
			}
			if (porteLaCle && !remplace) {
				out.Append(key);
				out.Append(" = ");
				out.Append(value ? value : "");
				out.Append('\n');
				remplace = true;
			} else {
				for (const char *c = debut; c < apres; ++c)
					out.Append(*c);
				// Derniere ligne sans saut final : on le pose, sinon l'ajout
				// ci-dessous se collerait a elle.
				if (apres == fin && fin > debut)
					out.Append('\n');
			}
			p = apres;
		}
		if (!remplace) {
			out.Append(key);
			out.Append(" = ");
			out.Append(value ? value : "");
			out.Append('\n');
		}
	}

	/// Ecrit le texte de facon ATOMIQUE : temporaire a cote, puis renommage.
	/// Rend `false` sans rien detruire si l'une des deux etapes echoue.
	inline bool NkGfxConfigWriteAtomic(const char *path, const char *text) {
		if (!path || !*path || !text)
			return false;
		NkString tmp(path);
		tmp.Append(".tmp");
		if (!nkentseu::NkFile::WriteAllText(tmp.Data(), text))
			return false;
		// `Move` echoue si la cible existe : on retire l'ancienne APRES avoir
		// ecrit le temporaire, donc la fenetre ou rien n'existe est aussi courte
		// que possible — et si la machine s'arrete pile la, le `.tmp` porte encore
		// le contenu complet.
		if (nkentseu::NkFile::Exists(path))
			nkentseu::NkFile::Delete(path);
		if (nkentseu::NkFile::Move(tmp.Data(), path))
			return true;
		nkentseu::NkFile::Delete(tmp.Data());
		return false;
	}

	/// Le geste complet : relire, remplacer la cle, reecrire atomiquement.
	/// ⚠️ IL RELIT JUSTE AVANT D'ECRIRE, il ne se sert pas d'un texte garde en
	///    memoire au demarrage : entre les deux, quelqu'un a pu editer le fichier
	///    a la main. Ecraser sa version avec la notre serait exactement la
	///    destruction que la regle 1 interdit.
	inline bool NkGfxConfigSetKey(const char *path, const char *key, const char *value) {
		const NkString ancien =
			nkentseu::NkFile::Exists(path) ? nkentseu::NkFile::ReadAllText(path) : NkString("");
		NkString nouveau;
		NkGfxConfigSet(ancien.Data(), key, value, nouveau);
		return NkGfxConfigWriteAtomic(path, nouveau.Data());
	}

	// LA resolution complete : detection automatique, puis environnement, puis
	// ligne de commande. L'ordre est celui qu'on attend, et il est verifie dans
	// LES DEUX SENS par l'essai 31e — sans quoi « la ligne de commande gagne »
	// passerait aussi si l'environnement etait purement ignore.
	inline NkGfxChoice NkGfxResolve(const char *cfg, const char *env, const char *const *args,
									uint32 argCount) {
		NkGfxChoice c;
		c.effective = NkGfxResolveEffective(c.api);
		if (NkGfxParse(cfg, c))
			c.source = NkGfxSource::FichierDeConfig;
		if (NkGfxParse(env, c))
			c.source = NkGfxSource::Environnement;
		for (uint32 i = 0; i < argCount; ++i) {
			const char *v = NkGfxArgValue(args ? args[i] : nullptr);
			if (v && NkGfxParse(v, c))
				c.source = NkGfxSource::LigneDeCommande;
		}
		if (!c.supported && c.source == NkGfxSource::FichierDeConfig) {
			c.refused = c.requested;
			c.fellBack = true;
			c.requested = "auto";
			c.api = NkEditorGfxApi::Auto;
			c.effective = NkGfxResolveEffective(c.api);
			c.supported = true;
			c.source = NkGfxSource::DetectionAuto;
		}
		return c;
	}

	// LA LIGNE DE JOURNAL, assemblee sans formateur.
	// ⚠️ C'est delibere : le defaut d'origine etait un formateur qui n'a pas
	//    substitue et l'a taire. Une concatenation ne peut pas echouer en
	//    silence — et l'essai 31a verifie qu'il ne reste AUCUNE accolade dans la
	//    sortie, ce qui rattraperait un retour au formateur.
	inline NkString NkGfxJournalLine(const NkGfxChoice &c) {
		NkString s("[NKUIDesign] backend graphique -- ");
		if (c.fellBack) {
			// ⚠️ EN TETE DE LIGNE, PAS EN FIN : une annonce de repli placee apres
			//    quatre champs se lit apres coup, quand on cherche deja pourquoi
			//    ca ne marche pas. « Crier » veut dire « en premier ».
			s.Append("!! LA CONFIGURATION DEMANDE '");
			s.Append(c.refused);
			s.Append("', INDISPONIBLE -- demarrage sur le repli, la config n'a PAS ete "
					 "reecrite (corrigez-la dans Preferences) !!  |  ");
		}
		s.Append("demande : ");
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
