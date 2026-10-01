#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverseFournisseurs.h
// @Brief   LES FOURNISSEURS DE MODELES, UNE SEULE COUCHE : Ollama, tout serveur
//          compatible OpenAI, l'API d'Anthropic, le CLI Claude, un modele local
//          lance comme un processus. Reglages, cles, liste des modeles, test de
//          connexion, conversation EN FLUX avec OUTILS.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// LA DEMANDE (Rihen, 01/10/2026, R18 de la feuille de route d'Unkeny)
//   « Ne pas oublier le panneau IA dans Unkeny, et trouver comment brancher des
//     modeles comme Ollama, etc. ; sur mon autre PC je vais installer Qwen juste
//     pour les tests en local. »
//
// POURQUOI ICI, ET PAS DANS UNE APPLICATION
//   Trois applications parlaient deja a des modeles, chacune a sa facon :
//   NKCode (curl + cle EN LIGNE DE COMMANDE, Ollama en dur sur `llama3.2`),
//   NKCraft (NKConverse, mais l'onglet Ollama « n'est pas cable au modeleur »),
//   NKUIDesign (NKConverse, sans outils). Une quatrieme ecriture pour Unkeny
//   aurait ete la divergence de trop. Tout ce qui sait PARLER a un fournisseur
//   vit donc ici ; ce qui sait ce qu'est une SCENE reste chez Unkeny (regle 3
//   du module : le contrat reste pauvre, le vocabulaire du metier n'entre pas).
//
// CE QUE CE FICHIER DONNE
//   1. NkReglagesFournisseur : adresse, modele, temperature, contexte, outils.
//      Ecrits dans le dossier de l'UTILISATEUR (hors de tout depot), partages
//      par toutes les applications : un Ollama declare dans Unkeny l'est aussi
//      pour NKCraft.
//   2. LES CLES : saisies par l'utilisateur, rangees dans
//      `<AppData>/Nkentseu/IA/cles/<id>.cle` -- JAMAIS dans un depot, jamais en
//      ligne de commande (NkConverseTransport.h), jamais dans un journal. Une
//      variable d'environnement passe AVANT le fichier (ANTHROPIC_API_KEY...).
//   3. NkIaListerModeles : ce que le SERVEUR dit avoir (/api/tags, /v1/models),
//      jamais une liste en dur -- un modele tire demain apparait sans une ligne.
//   4. NkIaTesterConnexion : UN diagnostic qui dit LEQUEL des cas, avec le
//      geste qui repare EN TETE (une ligne d'etat tronque, et ce qui survit
//      doit etre ce qu'on peut FAIRE) : serveur absent, modele absent, cle
//      absente, cle refusee, HTTPS et curl absents.
//   5. NkIaDiscuter : des messages (systeme, utilisateur, assistant, resultat
//      d'outil) et des OUTILS ; la reponse arrive EN FLUX ; les appels d'outils
//      sont rendus, NON executes -- c'est l'hote qui sait agir, annuler et
//      demander confirmation.
//   6. LE FORMAT TEXTE des outils, pour les modeles locaux sans appel d'outils
//      natif : un bloc `<outil nom="...">{...}</outil>` que l'hote analyse
//      (NkIaOutilsDuTexte). Le format Hermes `<tool_call>{...}</tool_call>`,
//      qu'ecrit Qwen quand son gabarit d'outils n'est pas branche, est lu aussi.
// -----------------------------------------------------------------------------

#include "NKConverse/NkConverseJson.h"
#include "NKConverse/NkConverseModeles.h"
#include "NKConverse/NkConverseTransport.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu::converse {

	// ═══════════════════════════════════════════════════════════════════════
	//  LES GENRES DE FOURNISSEUR
	// ═══════════════════════════════════════════════════════════════════════
	/// AJOUTES A LA FIN : le fichier de reglages porte la CLE (« ollama »), pas
	/// la valeur, mais une valeur inseree au milieu fausserait une table.
	enum class NkGenreFournisseur : nkentseu::uint8 {
		NK_OLLAMA = 0,	///< http://127.0.0.1:11434, /api/tags, /api/chat en NDJSON
		NK_OPENAI,		///< /v1/models, /v1/chat/completions en SSE (LM Studio, llama.cpp, vLLM...)
		NK_ANTHROPIC,	///< https://api.anthropic.com/v1/messages en SSE, cle x-api-key
		NK_CLAUDE_CLI,	///< le CLI de Claude Code (NkConverseClaude.h), sans cle a saisir
		NK_PROCESSUS,	///< un modele local lance comme un processus (NKDesignLLM, GGUF)
		NK_COUNT
	};

	inline const char *NkGenreFournisseurCle(NkGenreFournisseur g) {
		switch (g) {
			case NkGenreFournisseur::NK_OLLAMA: return "ollama";
			case NkGenreFournisseur::NK_OPENAI: return "openai";
			case NkGenreFournisseur::NK_ANTHROPIC: return "anthropic";
			case NkGenreFournisseur::NK_CLAUDE_CLI: return "claude-cli";
			case NkGenreFournisseur::NK_PROCESSUS: return "processus";
			default: return "";
		}
	}
	inline const char *NkGenreFournisseurNom(NkGenreFournisseur g) {
		switch (g) {
			case NkGenreFournisseur::NK_OLLAMA: return "Ollama";
			case NkGenreFournisseur::NK_OPENAI: return "Serveur compatible OpenAI";
			case NkGenreFournisseur::NK_ANTHROPIC: return "Claude (API Anthropic)";
			case NkGenreFournisseur::NK_CLAUDE_CLI: return "Claude (CLI Claude Code)";
			case NkGenreFournisseur::NK_PROCESSUS: return "Modele local (processus GGUF)";
			default: return "?";
		}
	}
	/// Ce que l'utilisateur doit savoir du genre, en une ligne.
	inline const char *NkGenreFournisseurAide(NkGenreFournisseur g) {
		switch (g) {
			case NkGenreFournisseur::NK_OLLAMA:
				return "Ollama sur ce PC ou un PC du reseau (http://IP:11434) ; « ollama pull qwen2.5:7b » installe un modele.";
			case NkGenreFournisseur::NK_OPENAI:
				return "LM Studio (http://127.0.0.1:1234/v1), llama.cpp server (:8080/v1), vLLM (:8000/v1), ou un service en ligne.";
			case NkGenreFournisseur::NK_ANTHROPIC:
				return "La cle se saisit ici et se range HORS du depot ; l'invite part chez Anthropic.";
			case NkGenreFournisseur::NK_CLAUDE_CLI:
				return "Le CLI « claude » deja connecte (NKCode > Comptes) : aucune cle a saisir.";
			case NkGenreFournisseur::NK_PROCESSUS:
				return "L'executable (NKDesignLLM.exe) et le modele (.gguf ou nom Ollama) ; sans serveur.";
			default: return "";
		}
	}
	inline NkGenreFournisseur NkGenreFournisseurDeCle(const char *c) {
		for (nkentseu::uint8 i = 0; i < static_cast<nkentseu::uint8>(NkGenreFournisseur::NK_COUNT); ++i)
			if (c && std::strcmp(c, NkGenreFournisseurCle(static_cast<NkGenreFournisseur>(i))) == 0)
				return static_cast<NkGenreFournisseur>(i);
		return NkGenreFournisseur::NK_OLLAMA;
	}
	inline const char *NkAdresseParDefaut(NkGenreFournisseur g) {
		switch (g) {
			case NkGenreFournisseur::NK_OLLAMA: return "http://127.0.0.1:11434";
			case NkGenreFournisseur::NK_OPENAI: return "http://127.0.0.1:1234/v1";
			case NkGenreFournisseur::NK_ANTHROPIC: return "https://api.anthropic.com";
			default: return "";
		}
	}

	/// Comment parler d'outils a CE modele.
	enum class NkModeOutils : nkentseu::uint8 {
		NK_AUTO = 0, ///< natifs si le fournisseur et le modele les annoncent, texte sinon
		NK_NATIFS,	 ///< le champ `tools` de l'API
		NK_TEXTE	 ///< des blocs <outil> dans le texte (tout modele qui suit une consigne)
	};
	inline const char *NkModeOutilsCle(NkModeOutils m) {
		return m == NkModeOutils::NK_NATIFS ? "natifs" : (m == NkModeOutils::NK_TEXTE ? "texte" : "auto");
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  LES REGLAGES D'UN FOURNISSEUR
	// ═══════════════════════════════════════════════════════════════════════
	struct NkReglagesFournisseur {
			NkString id; ///< STABLE et sur pour un nom de fichier (« ollama-local ») : la cle s'y range
			NkGenreFournisseur genre = NkGenreFournisseur::NK_OLLAMA;
			NkString nom;	  ///< ce qui s'affiche (« Ollama (autre PC) »)
			NkString adresse; ///< l'URL du service ; NK_PROCESSUS : l'executable
			NkString modele;  ///< vide = le premier que le serveur liste
			/// < 0 : non envoyee (le modele garde la sienne). Anthropic la borne a 1.
			float32 temperature = 0.7f;
			/// La fenetre de contexte (Ollama : options.num_ctx ; NK_PROCESSUS :
			/// --contexte). 0 = non envoyee. Les serveurs OpenAI la fixent au
			/// chargement du modele : ils l'ignorent.
			nkentseu::int32 contexte = 8192;
			/// Le budget de la REPONSE (num_predict, max_tokens). Anthropic l'exige.
			nkentseu::int32 sortieMax = 4096;
			NkModeOutils outils = NkModeOutils::NK_AUTO;
			NkString compte; ///< NK_CLAUDE_CLI : le compte de NKCode (vide = celui du CLI)

			/// L'invite quitte-t-elle CE PC ? Un Ollama sur un autre PC du reseau :
			/// oui -- vers la maison, mais elle part. Le panneau le dit (orange).
			bool Distant() const {
				if (genre == NkGenreFournisseur::NK_ANTHROPIC || genre == NkGenreFournisseur::NK_CLAUDE_CLI)
					return true;
				if (genre == NkGenreFournisseur::NK_PROCESSUS)
					return false;
				const char *a = adresse.CStr();
				const char *h = std::strstr(a, "://");
				h = h ? h + 3 : a;
				return !(std::strncmp(h, "127.", 4) == 0 || std::strncmp(h, "localhost", 9) == 0 ||
						 std::strncmp(h, "[::1]", 5) == 0);
			}
	};

	// ═══════════════════════════════════════════════════════════════════════
	//  OU VIVENT LES REGLAGES ET LES CLES (hors de tout depot)
	// ═══════════════════════════════════════════════════════════════════════
	/// `<AppData>/Nkentseu/IA` (Windows : %APPDATA%). `NK_IA_DOSSIER` le
	/// deplace -- les bancs y posent un dossier temporaire et n'ecrivent RIEN
	/// chez l'utilisateur.
	inline NkString NkIaDossier() {
		if (const char *d = std::getenv("NK_IA_DOSSIER"))
			if (*d)
				return NkString(d);
		const nkentseu::NkPath base = nkentseu::NkDirectory::GetAppDataDirectory();
		NkString b = base.ToString();
		if (b.Empty())
			b = nkentseu::NkDirectory::GetHomeDirectory().ToString();
		return (nkentseu::NkPath(b.CStr()) / "Nkentseu" / "IA").ToString();
	}

	/// Un identifiant ne garde que [a-z0-9-] : il devient un nom de fichier.
	inline NkString NkIaIdSur(const char *s) {
		NkString o;
		for (const char *p = s ? s : ""; *p; ++p) {
			char c = *p;
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
			if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')
				o.Append(c);
			else if (c == ' ' || c == '_' || c == '.')
				o.Append('-');
		}
		return o.Empty() ? NkString("fournisseur") : o;
	}

	inline NkString NkIaCheminCle(const NkString &dossier, const NkString &id) {
		return (nkentseu::NkPath(dossier.CStr()) / "cles" / (NkIaIdSur(id.CStr()) + ".cle").CStr()).ToString();
	}

	/// LA CLE d'un fournisseur, et D'OU elle vient (`source`, pour l'afficher :
	/// « variable ANTHROPIC_API_KEY » ou « fichier hors du depot »). Faux = aucune.
	inline bool NkIaLireCle(const NkString &dossier, const NkReglagesFournisseur &r, NkString &cle,
							NkString *source = nullptr) {
		cle = NkString();
		// 1. Une variable PROPRE a ce fournisseur : NK_IA_CLE_<ID> (tirets -> _).
		{
			NkString v("NK_IA_CLE_");
			const NkString id = NkIaIdSur(r.id.CStr());
			for (NkString::SizeType i = 0; i < id.Length(); ++i) {
				char c = id[i];
				if (c >= 'a' && c <= 'z')
					c = static_cast<char>(c - 'a' + 'A');
				v.Append(c == '-' ? '_' : c);
			}
			if (const char *e = std::getenv(v.CStr()))
				if (*e) {
					cle = NkString(e);
					if (source)
						*source = NkString("variable ") + v;
					return true;
				}
		}
		// 2. Les variables d'usage du genre (les memes que NKCode et les SDK).
		const char *vars[3] = {nullptr, nullptr, nullptr};
		if (r.genre == NkGenreFournisseur::NK_ANTHROPIC) {
			vars[0] = "ANTHROPIC_API_KEY";
			vars[1] = "NKCODE_ANTHROPIC_KEY";
		} else if (r.genre == NkGenreFournisseur::NK_OPENAI) {
			vars[0] = "OPENAI_API_KEY";
		}
		for (int k = 0; k < 3 && vars[k]; ++k)
			if (const char *e = std::getenv(vars[k]))
				if (*e) {
					cle = NkString(e);
					if (source)
						*source = NkString("variable ") + vars[k];
					return true;
				}
		// 3. Le fichier, hors du depot.
		const NkString f = NkIaCheminCle(dossier, r.id);
		if (nkentseu::NkFile::Exists(f.CStr())) {
			cle = nkentseu::NkFile::ReadAllText(f.CStr());
			cle.Trim();
			if (!cle.Empty()) {
				if (source)
					*source = NkString("fichier hors du depot (") + f + ")";
				return true;
			}
		}
		return false;
	}

	/// Range la cle saisie (vide = l'efface). Rend faux ET NOMME la raison.
	inline bool NkIaEcrireCle(const NkString &dossier, const NkString &id, const NkString &cle, NkString &pourquoi) {
		const NkString f = NkIaCheminCle(dossier, id);
		if (cle.Empty()) {
			if (nkentseu::NkFile::Exists(f.CStr()))
				nkentseu::NkFile::Delete(f.CStr());
			return true;
		}
		nkentseu::NkDirectory::CreateRecursive((nkentseu::NkPath(dossier.CStr()) / "cles").ToString().CStr());
		NkString propre = cle;
		propre.Trim();
		if (!nkentseu::NkFile::WriteAllText(f.CStr(), propre.CStr())) {
			pourquoi = NkString("impossible d'ecrire ") + f;
			return false;
		}
		return true;
	}

	/// LA CLE MASQUEE pour l'affichage : « sk-a…9f2c » -- jamais la cle.
	inline NkString NkIaMasquerCle(const NkString &cle) {
		if (cle.Length() <= 8u)
			return cle.Empty() ? NkString() : NkString("••••");
		NkString m(cle.CStr(), 4);
		m.Append("…");
		m.Append(cle.CStr() + cle.Length() - 4u);
		return m;
	}

	/// Les fournisseurs proposes quand aucun n'est encore declare.
	inline void NkIaFournisseursParDefaut(nkentseu::NkVector<NkReglagesFournisseur> &out) {
		out.Clear();
		NkReglagesFournisseur o;
		o.id = NkString("ollama-local");
		o.genre = NkGenreFournisseur::NK_OLLAMA;
		o.nom = NkString("Ollama (ce PC)");
		o.adresse = NkString(NkAdresseParDefaut(o.genre));
		out.PushBack(o);
		NkReglagesFournisseur l;
		l.id = NkString("openai-local");
		l.genre = NkGenreFournisseur::NK_OPENAI;
		l.nom = NkString("Serveur OpenAI local (LM Studio, llama.cpp)");
		l.adresse = NkString(NkAdresseParDefaut(l.genre));
		out.PushBack(l);
		NkReglagesFournisseur a;
		a.id = NkString("claude-api");
		a.genre = NkGenreFournisseur::NK_ANTHROPIC;
		a.nom = NkString("Claude (API, cle)");
		a.adresse = NkString(NkAdresseParDefaut(a.genre));
		out.PushBack(a);
		NkReglagesFournisseur c;
		c.id = NkString("claude-cli");
		c.genre = NkGenreFournisseur::NK_CLAUDE_CLI;
		c.nom = NkString("Claude (CLI Claude Code)");
		out.PushBack(c);
	}

	/// Lit `<dossier>/fournisseurs.txt`. Absent -> les fournisseurs par defaut
	/// (et `actif` = 0). Le format est celui de la maison : une ligne d'en-tete,
	/// des `cle = valeur`, une section par fournisseur.
	inline void NkIaLireReglages(const NkString &dossier, nkentseu::NkVector<NkReglagesFournisseur> &out,
								 NkString &actif) {
		out.Clear();
		actif = NkString();
		const NkString f = (nkentseu::NkPath(dossier.CStr()) / "fournisseurs.txt").ToString();
		if (!nkentseu::NkFile::Exists(f.CStr())) {
			NkIaFournisseursParDefaut(out);
			return;
		}
		const NkString t = nkentseu::NkFile::ReadAllText(f.CStr());
		const char *l = t.CStr();
		NkReglagesFournisseur *cur = nullptr;
		while (*l) {
			const char *fin = l;
			while (*fin && *fin != '\n' && *fin != '\r')
				++fin;
			NkString ligne(l, static_cast<NkString::SizeType>(fin - l));
			ligne.Trim();
			if (ligne == "[fournisseur]") {
				out.PushBack(NkReglagesFournisseur());
				cur = &out[out.Size() - 1];
			} else {
				const NkString::SizeType eg = ligne.Find(" = ");
				if (eg != NkString::npos) {
					const NkString k = ligne.SubStr(0, eg);
					const NkString v = ligne.SubStr(eg + 3);
					if (k == "actif")
						actif = v;
					else if (cur) {
						if (k == "id")
							cur->id = v;
						else if (k == "genre")
							cur->genre = NkGenreFournisseurDeCle(v.CStr());
						else if (k == "nom")
							cur->nom = v;
						else if (k == "adresse")
							cur->adresse = v;
						else if (k == "modele")
							cur->modele = v;
						else if (k == "temperature")
							cur->temperature = static_cast<float32>(std::atof(v.CStr()));
						else if (k == "contexte")
							cur->contexte = static_cast<nkentseu::int32>(std::atoi(v.CStr()));
						else if (k == "sortie")
							cur->sortieMax = static_cast<nkentseu::int32>(std::atoi(v.CStr()));
						else if (k == "outils")
							cur->outils = v == "natifs" ? NkModeOutils::NK_NATIFS
													  : (v == "texte" ? NkModeOutils::NK_TEXTE : NkModeOutils::NK_AUTO);
						else if (k == "compte")
							cur->compte = v;
					}
				}
			}
			l = fin;
			while (*l == '\n' || *l == '\r')
				++l;
		}
		if (out.Empty())
			NkIaFournisseursParDefaut(out);
	}

	/// Ecrit `<dossier>/fournisseurs.txt`. ⚠️ AUCUNE CLE n'y entre : elles ont
	/// leur fichier a part (NkIaEcrireCle), qu'on peut effacer sans perdre le reste.
	inline bool NkIaEcrireReglages(const NkString &dossier, const nkentseu::NkVector<NkReglagesFournisseur> &fs,
								   const NkString &actif) {
		nkentseu::NkDirectory::CreateRecursive(dossier.CStr());
		NkString t("nkfournisseurs 1\n");
		t.Append("# Les fournisseurs de modeles de Nkentseu (Unkeny, NKCraft, NKCode...).\n");
		t.Append("# AUCUNE CLE ICI : elles sont dans cles/<id>.cle, a cote.\n");
		t.Append("actif = ");
		t.Append(actif);
		t.Append("\n");
		for (nkentseu::usize i = 0; i < fs.Size(); ++i) {
			const NkReglagesFournisseur &r = fs[i];
			char b[64];
			t.Append("\n[fournisseur]\nid = ");
			t.Append(NkIaIdSur(r.id.CStr()));
			t.Append("\ngenre = ");
			t.Append(NkGenreFournisseurCle(r.genre));
			t.Append("\nnom = ");
			t.Append(r.nom);
			t.Append("\nadresse = ");
			t.Append(r.adresse);
			t.Append("\nmodele = ");
			t.Append(r.modele);
			std::snprintf(b, sizeof(b), "\ntemperature = %.2f", static_cast<double>(r.temperature));
			t.Append(b);
			std::snprintf(b, sizeof(b), "\ncontexte = %d\nsortie = %d", static_cast<int>(r.contexte), static_cast<int>(r.sortieMax));
			t.Append(b);
			t.Append("\noutils = ");
			t.Append(NkModeOutilsCle(r.outils));
			if (!r.compte.Empty()) {
				t.Append("\ncompte = ");
				t.Append(r.compte);
			}
			t.Append("\n");
		}
		return nkentseu::NkFile::WriteAllText((nkentseu::NkPath(dossier.CStr()) / "fournisseurs.txt").ToString().CStr(),
											  t.CStr());
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  MESSAGES, OUTILS, REPONSES
	// ═══════════════════════════════════════════════════════════════════════
	enum class NkRoleIA : nkentseu::uint8 { NK_SYSTEME = 0, NK_UTILISATEUR, NK_ASSISTANT, NK_OUTIL };

	struct NkAppelOutil {
			NkString id;		///< celui du fournisseur (« call_1 », « toolu_... ») ; pose s'il manque
			NkString nom;
			NkString arguments; ///< un OBJET JSON, en texte (« {} » s'il n'y en a pas)
	};

	struct NkMessageIA {
			NkRoleIA role = NkRoleIA::NK_UTILISATEUR;
			NkString texte;
			nkentseu::NkVector<NkAppelOutil> appels; ///< NK_ASSISTANT : les outils qu'il a appeles
			NkString idAppel;	///< NK_OUTIL : l'appel auquel ce resultat repond
			NkString nomOutil;	///< NK_OUTIL : son nom (Ollama le veut, le format texte aussi)
	};

	/// UN OUTIL que l'hote propose. `schema` est le schema JSON de ses
	/// PARAMETRES (un objet : {"type":"object","properties":{...}}).
	struct NkOutilIA {
			NkString nom;
			NkString description;
			NkString schema;
			/// IRREVERSIBLE : l'hote demandera confirmation AVANT de l'executer.
			/// Le fournisseur n'en fait rien ; c'est ecrit ici pour que la
			/// description generee le DISE au modele.
			bool irreversible = false;
	};

	struct NkRequeteIA {
			nkentseu::NkVector<NkMessageIA> messages;
			nkentseu::NkVector<NkOutilIA> outils;
			/// Faux : les outils sont decrits dans le message systeme (format texte)
			/// et le champ `tools` n'est PAS envoye.
			bool outilsNatifs = true;
	};

	/// LE DIAGNOSTIC : lequel des cas, pas « ca n'a pas marche ».
	enum class NkDiagIA : nkentseu::uint8 {
		NK_OK = 0,
		NK_SERVEUR_ABSENT,	 ///< personne ne repond a l'adresse
		NK_MODELE_ABSENT,	 ///< le serveur repond, le modele n'y est pas
		NK_CLE_ABSENTE,		 ///< aucune cle pour un fournisseur qui en exige une
		NK_CLE_REFUSEE,		 ///< 401 / 403
		NK_TRANSPORT_ABSENT, ///< https sans TLS compile ni curl ; CLI absent ; executable absent
		NK_OUTILS_REFUSES,	 ///< le modele ne sait pas les outils natifs (l'hote repasse en texte)
		NK_DELAI,			 ///< NOTRE plafond d'attente, pas une panne du modele
		NK_ANNULE,			 ///< l'utilisateur a arrete
		NK_ILLISIBLE,		 ///< 200, mais rien a lire
		NK_ERREUR			 ///< un autre refus du service, avec son message
	};
	inline const char *NkDiagIANom(NkDiagIA d) {
		switch (d) {
			case NkDiagIA::NK_OK: return "ok";
			case NkDiagIA::NK_SERVEUR_ABSENT: return "serveur absent";
			case NkDiagIA::NK_MODELE_ABSENT: return "modele absent";
			case NkDiagIA::NK_CLE_ABSENTE: return "cle absente";
			case NkDiagIA::NK_CLE_REFUSEE: return "cle refusee";
			case NkDiagIA::NK_TRANSPORT_ABSENT: return "transport absent";
			case NkDiagIA::NK_OUTILS_REFUSES: return "outils refuses";
			case NkDiagIA::NK_DELAI: return "delai";
			case NkDiagIA::NK_ANNULE: return "annule";
			case NkDiagIA::NK_ILLISIBLE: return "illisible";
			default: return "erreur";
		}
	}

	struct NkReponseIA {
			bool ok = false;
			NkString texte; ///< ce que le modele a ECRIT (blocs <outil> compris en mode texte)
			nkentseu::NkVector<NkAppelOutil> appels; ///< ses appels NATIFS
			NkDiagIA diag = NkDiagIA::NK_ERREUR;
			NkString erreur;		 ///< le motif, le geste qui repare en tete
			nkentseu::uint32 statut = 0u;
			NkString corpsEnvoye;	 ///< LES OCTETS envoyes (cle exclue : elle est en en-tete)
			nkentseu::uint64 jetonsEntree = 0u, jetonsSortie = 0u;
			NkString finRaison;
			bool viaCurl = false;
	};

	// ═══════════════════════════════════════════════════════════════════════
	//  LE FORMAT TEXTE DES OUTILS
	// ═══════════════════════════════════════════════════════════════════════
	/// La consigne qui l'enseigne, a mettre dans le message systeme.
	inline void NkIaConsigneOutilsTexte(const nkentseu::NkVector<NkOutilIA> &outils, NkString &out) {
		out.Append("\n## Comment agir\n");
		out.Append("Pour agir sur l'editeur, ecris un ou plusieurs blocs, chacun ainsi :\n");
		out.Append("<outil nom=\"NOM_DE_L_OUTIL\">{\"parametre\": valeur}</outil>\n");
		out.Append("Le contenu est un OBJET JSON (des guillemets droits). L'editeur execute chaque bloc et te rend "
				   "son resultat dans le message suivant ; attends-le avant de conclure. N'invente pas d'outil.\n");
		out.Append("\n## Outils\n");
		for (nkentseu::usize i = 0; i < outils.Size(); ++i) {
			out.Append("- ");
			out.Append(outils[i].nom);
			if (outils[i].irreversible)
				out.Append(" [IRREVERSIBLE : l'utilisateur confirme]");
			out.Append(" : ");
			out.Append(outils[i].description);
			out.Append("\n  parametres : ");
			out.Append(outils[i].schema.Empty() ? NkString("{}") : outils[i].schema);
			out.Append("\n");
		}
	}

	/// Les blocs `<outil nom="X">{...}</outil>` (et `<tool_call>{"name":..,
	/// "arguments":{..}}</tool_call>`) d'un texte. Un bloc dont le JSON ne se
	/// lit pas est rendu QUAND MEME, avec `arguments` = son texte brut : c'est
	/// l'hote qui le refuse en le nommant -- un appel avale en silence ferait
	/// croire au modele qu'il a agi.
	inline void NkIaOutilsDuTexte(const NkString &t, nkentseu::NkVector<NkAppelOutil> &out) {
		out.Clear();
		NkString::SizeType p = 0;
		nkentseu::uint32 n = 0u;
		for (;;) {
			const NkString::SizeType a = t.Find("<outil", p);
			const NkString::SizeType b = t.Find("<tool_call>", p);
			if (a == NkString::npos && b == NkString::npos)
				break;
			const bool hermes = a == NkString::npos || (b != NkString::npos && b < a);
			NkAppelOutil ap;
			char id[32];
			std::snprintf(id, sizeof(id), "texte_%u", static_cast<unsigned>(++n));
			ap.id = NkString(id);
			if (!hermes) {
				const NkString::SizeType ferme = t.Find(">", a);
				if (ferme == NkString::npos)
					break;
				const NkString entete = t.SubStr(a, ferme - a);
				const NkString::SizeType q = entete.Find("nom=\"");
				if (q != NkString::npos) {
					const NkString::SizeType q2 = entete.Find("\"", q + 5);
					if (q2 != NkString::npos)
						ap.nom = entete.SubStr(q + 5, q2 - q - 5);
				}
				const NkString::SizeType fin = t.Find("</outil>", ferme);
				NkString corps = t.SubStr(ferme + 1, (fin == NkString::npos ? t.Length() : fin) - ferme - 1);
				corps.Trim();
				ap.arguments = corps.Empty() ? NkString("{}") : corps;
				p = fin == NkString::npos ? t.Length() : fin + 8;
			} else {
				const NkString::SizeType fin = t.Find("</tool_call>", b);
				NkString corps = t.SubStr(b + 11, (fin == NkString::npos ? t.Length() : fin) - b - 11);
				corps.Trim();
				NkJsonDoc d;
				if (d.Lire(corps)) {
					ap.nom = d.TexteDe(d.Racine(), "name");
					const int32 args = d.Membre(d.Racine(), "arguments");
					ap.arguments = d.EstTexte(args) ? d.Texte(args) : d.Ecrire(args);
				} else
					ap.arguments = corps;
				p = fin == NkString::npos ? t.Length() : fin + 12;
			}
			if (!ap.nom.Empty())
				out.PushBack(ap);
		}
	}

	/// Le texte A MONTRER : les blocs d'outils retires (meme un bloc en cours
	/// d'ecriture, pendant le flux), les blancs de bord resserres.
	inline NkString NkIaTexteSansOutils(const NkString &t) {
		NkString o;
		NkString::SizeType p = 0;
		for (;;) {
			NkString::SizeType a = t.Find("<outil", p);
			NkString::SizeType b = t.Find("<tool_call>", p);
			const NkString::SizeType debut = a == NkString::npos ? b : (b == NkString::npos ? a : (a < b ? a : b));
			if (debut == NkString::npos) {
				o.Append(t.CStr() + p);
				break;
			}
			o.Append(t.CStr() + p, debut - p);
			const bool hermes = debut == b;
			const NkString::SizeType fin = t.Find(hermes ? "</tool_call>" : "</outil>", debut);
			if (fin == NkString::npos)
				break; // un bloc en cours : on ne montre pas sa moitie
			p = fin + (hermes ? 12 : 8);
		}
		// Un fragment « <out » au bout du flux n'est pas encore un bloc : on le tait.
		const NkString::SizeType lt = o.RFind("<");
		if (lt != NkString::npos && o.Find(">", lt) == NkString::npos && o.Length() - lt < 12u)
			o = o.SubStr(0, lt);
		o.Trim();
		return o;
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  LES DETAILS DES PROTOCOLES
	// ═══════════════════════════════════════════════════════════════════════
	namespace protocoles {

		inline NkString BaseOpenAI(const NkString &adresse) {
			NkString a = adresse;
			while (a.EndsWith("/"))
				a.PopBack();
			// « http://h:1234 » -> « http://h:1234/v1 » : un chemin vide est le cas
			// de l'utilisateur qui a recopie l'adresse affichee par LM Studio.
			const NkString::SizeType s = a.Find("://");
			const NkString::SizeType chemin = a.Find("/", s == NkString::npos ? 0 : s + 3);
			if (chemin == NkString::npos)
				a.Append("/v1");
			return a;
		}
		inline NkString BaseSimple(const NkString &adresse, const char *defaut) {
			NkString a = adresse.Empty() ? NkString(defaut) : adresse;
			while (a.EndsWith("/"))
				a.PopBack();
			return a;
		}

		/// Les arguments d'un appel, en OBJET JSON valide (« {} » sinon).
		inline NkString ArgumentsObjet(const NkString &a) {
			NkJsonDoc d;
			if (d.Lire(a) && d.EstObjet(d.Racine()))
				return a;
			return NkString("{}");
		}

		inline void OutilsOpenAI(const nkentseu::NkVector<NkOutilIA> &outils, NkString &j) {
			j.Append(",\"tools\":[");
			for (nkentseu::usize i = 0; i < outils.Size(); ++i) {
				if (i)
					j.Append(',');
				j.Append("{\"type\":\"function\",\"function\":{\"name\":");
				NkJsonChaine(outils[i].nom, j);
				j.Append(",\"description\":");
				NkJsonChaine(outils[i].description, j);
				j.Append(",\"parameters\":");
				j.Append(outils[i].schema.Empty() ? NkString("{\"type\":\"object\",\"properties\":{}}") : outils[i].schema);
				j.Append("}}");
			}
			j.Append(']');
		}

		/// Le message d'un resultat d'outil quand les outils sont en TEXTE.
		inline NkString ResultatEnTexte(const NkMessageIA &m) {
			NkString t("[resultat de l'outil ");
			t.Append(m.nomOutil);
			t.Append("]\n");
			t.Append(m.texte);
			return t;
		}

		inline NkString CorpsOllama(const NkReglagesFournisseur &r, const NkRequeteIA &q, bool flux) {
			NkString j("{\"model\":");
			NkJsonChaine(r.modele, j);
			j.Append(",\"messages\":[");
			bool premier = true;
			for (nkentseu::usize i = 0; i < q.messages.Size(); ++i) {
				const NkMessageIA &m = q.messages[i];
				if (!premier)
					j.Append(',');
				premier = false;
				if (m.role == NkRoleIA::NK_OUTIL && !q.outilsNatifs) {
					j.Append("{\"role\":\"user\",\"content\":");
					NkJsonChaine(ResultatEnTexte(m), j);
					j.Append('}');
					continue;
				}
				j.Append("{\"role\":\"");
				j.Append(m.role == NkRoleIA::NK_SYSTEME	   ? "system"
						 : m.role == NkRoleIA::NK_ASSISTANT ? "assistant"
						 : m.role == NkRoleIA::NK_OUTIL		? "tool"
															: "user");
				j.Append("\",\"content\":");
				NkJsonChaine(m.texte, j);
				if (m.role == NkRoleIA::NK_ASSISTANT && q.outilsNatifs && m.appels.Size() > 0) {
					j.Append(",\"tool_calls\":[");
					for (nkentseu::usize k = 0; k < m.appels.Size(); ++k) {
						if (k)
							j.Append(',');
						j.Append("{\"function\":{\"name\":");
						NkJsonChaine(m.appels[k].nom, j);
						j.Append(",\"arguments\":");
						j.Append(ArgumentsObjet(m.appels[k].arguments));
						j.Append("}}");
					}
					j.Append(']');
				}
				if (m.role == NkRoleIA::NK_OUTIL) {
					j.Append(",\"tool_name\":");
					NkJsonChaine(m.nomOutil, j);
				}
				j.Append('}');
			}
			j.Append(']');
			j.Append(flux ? ",\"stream\":true" : ",\"stream\":false");
			char b[160];
			j.Append(",\"options\":{");
			bool v = false;
			if (r.temperature >= 0.f) {
				std::snprintf(b, sizeof(b), "\"temperature\":%.3f", static_cast<double>(r.temperature));
				j.Append(b);
				v = true;
			}
			if (r.contexte > 0) {
				std::snprintf(b, sizeof(b), "%s\"num_ctx\":%d", v ? "," : "", static_cast<int>(r.contexte));
				j.Append(b);
				v = true;
			}
			if (r.sortieMax > 0) {
				std::snprintf(b, sizeof(b), "%s\"num_predict\":%d", v ? "," : "", static_cast<int>(r.sortieMax));
				j.Append(b);
			}
			j.Append('}');
			if (q.outilsNatifs && q.outils.Size() > 0)
				OutilsOpenAI(q.outils, j); // Ollama prend la forme d'OpenAI
			j.Append('}');
			return j;
		}

		inline NkString CorpsOpenAI(const NkReglagesFournisseur &r, const NkRequeteIA &q, bool flux) {
			NkString j("{\"model\":");
			NkJsonChaine(r.modele, j);
			j.Append(",\"messages\":[");
			for (nkentseu::usize i = 0; i < q.messages.Size(); ++i) {
				const NkMessageIA &m = q.messages[i];
				if (i)
					j.Append(',');
				if (m.role == NkRoleIA::NK_OUTIL && !q.outilsNatifs) {
					j.Append("{\"role\":\"user\",\"content\":");
					NkJsonChaine(ResultatEnTexte(m), j);
					j.Append('}');
					continue;
				}
				j.Append("{\"role\":\"");
				j.Append(m.role == NkRoleIA::NK_SYSTEME	   ? "system"
						 : m.role == NkRoleIA::NK_ASSISTANT ? "assistant"
						 : m.role == NkRoleIA::NK_OUTIL		? "tool"
															: "user");
				j.Append("\",\"content\":");
				NkJsonChaine(m.texte, j);
				if (m.role == NkRoleIA::NK_ASSISTANT && q.outilsNatifs && m.appels.Size() > 0) {
					j.Append(",\"tool_calls\":[");
					for (nkentseu::usize k = 0; k < m.appels.Size(); ++k) {
						if (k)
							j.Append(',');
						j.Append("{\"id\":");
						NkJsonChaine(m.appels[k].id, j);
						j.Append(",\"type\":\"function\",\"function\":{\"name\":");
						NkJsonChaine(m.appels[k].nom, j);
						j.Append(",\"arguments\":");
						NkJsonChaine(ArgumentsObjet(m.appels[k].arguments), j); // une CHAINE chez OpenAI
						j.Append("}}");
					}
					j.Append(']');
				}
				if (m.role == NkRoleIA::NK_OUTIL) {
					j.Append(",\"tool_call_id\":");
					NkJsonChaine(m.idAppel, j);
				}
				j.Append('}');
			}
			j.Append(']');
			j.Append(flux ? ",\"stream\":true" : ",\"stream\":false");
			char b[96];
			if (r.temperature >= 0.f) {
				std::snprintf(b, sizeof(b), ",\"temperature\":%.3f", static_cast<double>(r.temperature));
				j.Append(b);
			}
			if (r.sortieMax > 0) {
				std::snprintf(b, sizeof(b), ",\"max_tokens\":%d", static_cast<int>(r.sortieMax));
				j.Append(b);
			}
			if (q.outilsNatifs && q.outils.Size() > 0)
				OutilsOpenAI(q.outils, j);
			j.Append('}');
			return j;
		}

		inline NkString CorpsAnthropic(const NkReglagesFournisseur &r, const NkRequeteIA &q, bool flux) {
			NkString j("{\"model\":");
			NkJsonChaine(r.modele, j);
			char b[96];
			std::snprintf(b, sizeof(b), ",\"max_tokens\":%d", static_cast<int>(r.sortieMax > 0 ? r.sortieMax : 4096));
			j.Append(b);
			// LE SYSTEME vit a part chez Anthropic ; les messages alternent
			// utilisateur / assistant, et des resultats d'outils consecutifs
			// partent dans UN SEUL message utilisateur.
			NkString systeme;
			for (nkentseu::usize i = 0; i < q.messages.Size(); ++i)
				if (q.messages[i].role == NkRoleIA::NK_SYSTEME) {
					if (!systeme.Empty())
						systeme.Append("\n\n");
					systeme.Append(q.messages[i].texte);
				}
			if (!systeme.Empty()) {
				j.Append(",\"system\":");
				NkJsonChaine(systeme, j);
			}
			j.Append(",\"messages\":[");
			bool premier = true;
			for (nkentseu::usize i = 0; i < q.messages.Size(); ++i) {
				const NkMessageIA &m = q.messages[i];
				if (m.role == NkRoleIA::NK_SYSTEME)
					continue;
				if (!premier)
					j.Append(',');
				premier = false;
				if (m.role == NkRoleIA::NK_OUTIL) {
					j.Append("{\"role\":\"user\",\"content\":[");
					nkentseu::usize k = i;
					for (; k < q.messages.Size() && q.messages[k].role == NkRoleIA::NK_OUTIL; ++k) {
						if (k > i)
							j.Append(',');
						if (q.outilsNatifs) {
							j.Append("{\"type\":\"tool_result\",\"tool_use_id\":");
							NkJsonChaine(q.messages[k].idAppel, j);
							j.Append(",\"content\":");
							NkJsonChaine(q.messages[k].texte, j);
							j.Append('}');
						} else {
							j.Append("{\"type\":\"text\",\"text\":");
							NkJsonChaine(ResultatEnTexte(q.messages[k]), j);
							j.Append('}');
						}
					}
					j.Append("]}");
					i = k - 1;
					continue;
				}
				if (m.role == NkRoleIA::NK_ASSISTANT) {
					j.Append("{\"role\":\"assistant\",\"content\":[");
					bool v = false;
					if (!m.texte.Empty()) {
						j.Append("{\"type\":\"text\",\"text\":");
						NkJsonChaine(m.texte, j);
						j.Append('}');
						v = true;
					}
					if (q.outilsNatifs)
						for (nkentseu::usize k = 0; k < m.appels.Size(); ++k) {
							if (v)
								j.Append(',');
							v = true;
							j.Append("{\"type\":\"tool_use\",\"id\":");
							NkJsonChaine(m.appels[k].id, j);
							j.Append(",\"name\":");
							NkJsonChaine(m.appels[k].nom, j);
							j.Append(",\"input\":");
							j.Append(ArgumentsObjet(m.appels[k].arguments));
							j.Append('}');
						}
					if (!v)
						j.Append("{\"type\":\"text\",\"text\":\"(rien)\"}");
					j.Append("]}");
					continue;
				}
				j.Append("{\"role\":\"user\",\"content\":");
				NkJsonChaine(m.texte, j);
				j.Append('}');
			}
			j.Append(']');
			j.Append(flux ? ",\"stream\":true" : ",\"stream\":false");
			if (r.temperature >= 0.f) {
				std::snprintf(b, sizeof(b), ",\"temperature\":%.3f",
							  static_cast<double>(r.temperature > 1.f ? 1.f : r.temperature));
				j.Append(b);
			}
			if (q.outilsNatifs && q.outils.Size() > 0) {
				j.Append(",\"tools\":[");
				for (nkentseu::usize i = 0; i < q.outils.Size(); ++i) {
					if (i)
						j.Append(',');
					j.Append("{\"name\":");
					NkJsonChaine(q.outils[i].nom, j);
					j.Append(",\"description\":");
					NkJsonChaine(q.outils[i].description, j);
					j.Append(",\"input_schema\":");
					j.Append(q.outils[i].schema.Empty() ? NkString("{\"type\":\"object\",\"properties\":{}}")
														: q.outils[i].schema);
					j.Append('}');
				}
				j.Append(']');
			}
			j.Append('}');
			return j;
		}

		/// LE LECTEUR DE FLUX : des octets entrent, du texte et des appels
		/// sortent. Une ligne a la fois (NDJSON d'Ollama, SSE d'OpenAI et
		/// d'Anthropic) ; une ligne coupee entre deux paquets attend la suite.
		struct Decodeur {
				NkGenreFournisseur genre = NkGenreFournisseur::NK_OLLAMA;
				NkString reste;
				NkString texte;
				nkentseu::NkVector<NkAppelOutil> appels;
				nkentseu::NkVector<nkentseu::int32> indices; ///< OpenAI/Anthropic : l'indice de chaque appel
				NkString erreur;
				NkString finRaison;
				nkentseu::uint64 jetonsEntree = 0u, jetonsSortie = 0u;
				bool fini = false;
				nkentseu::uint32 lignes = 0u;
				/// Appele a chaque morceau de TEXTE lisible (le flux montre).
				nkentseu::NkFunction<void(const NkString &)> surTexte;

				void Pousser(const char *d, nkentseu::uint32 n) {
					reste.Append(d, static_cast<NkString::SizeType>(n));
					for (;;) {
						const NkString::SizeType nl = reste.Find('\n');
						if (nl == NkString::npos)
							break;
						NkString l = reste.SubStr(0, nl);
						reste = reste.SubStr(nl + 1);
						Ligne(l);
					}
				}
				void Terminer() {
					if (!reste.Empty()) {
						NkString l = reste;
						reste = NkString();
						Ligne(l);
					}
				}

				nkentseu::int32 AppelPourIndice(nkentseu::int32 idx) {
					for (nkentseu::usize k = 0; k < indices.Size(); ++k)
						if (indices[k] == idx)
							return static_cast<nkentseu::int32>(k);
					appels.PushBack(NkAppelOutil());
					indices.PushBack(idx);
					return static_cast<nkentseu::int32>(appels.Size()) - 1;
				}
				void AjouterTexte(const NkString &t) {
					if (t.Empty())
						return;
					texte.Append(t);
					if (surTexte != nullptr)
						surTexte(t);
				}

				void Ligne(NkString l) {
					l.Trim();
					if (l.Empty())
						return;
					++lignes;
					if (genre == NkGenreFournisseur::NK_OLLAMA) {
						NkJsonDoc d;
						if (!d.Lire(l))
							return;
						const int32 r = d.Racine();
						const int32 e = d.Membre(r, "error");
						if (e >= 0) {
							erreur = d.Texte(e);
							return;
						}
						AjouterTexte(d.TexteDe(r, "message.content"));
						const int32 tc = d.Chemin(r, "message.tool_calls");
						for (uint32 k = 0; k < d.Taille(tc); ++k) {
							const int32 c = d.Element(tc, k);
							NkAppelOutil a;
							a.nom = d.TexteDe(c, "function.name");
							const int32 args = d.Chemin(c, "function.arguments");
							a.arguments = d.EstTexte(args) ? d.Texte(args) : d.Ecrire(args);
							char id[32];
							std::snprintf(id, sizeof(id), "appel_%u", static_cast<unsigned>(appels.Size() + 1u));
							a.id = NkString(id);
							if (!a.nom.Empty())
								appels.PushBack(a);
						}
						if (d.Booleen(d.Membre(r, "done"))) {
							fini = true;
							finRaison = d.TexteDe(r, "done_reason");
							jetonsEntree = static_cast<nkentseu::uint64>(d.Nombre(d.Membre(r, "prompt_eval_count")));
							jetonsSortie = static_cast<nkentseu::uint64>(d.Nombre(d.Membre(r, "eval_count")));
						}
						return;
					}
					// SSE : seules les lignes « data: » portent quelque chose.
					if (!l.StartsWith("data:"))
						return;
					NkString p = l.SubStr(5);
					p.Trim();
					if (p == "[DONE]") {
						fini = true;
						return;
					}
					NkJsonDoc d;
					if (!d.Lire(p))
						return;
					const int32 r = d.Racine();
					if (genre == NkGenreFournisseur::NK_OPENAI) {
						const int32 e = d.Membre(r, "error");
						if (e >= 0) {
							erreur = d.EstObjet(e) ? d.TexteDe(e, "message") : d.Texte(e);
							return;
						}
						const int32 ch = d.Chemin(r, "choices.0");
						AjouterTexte(d.TexteDe(ch, "delta.content"));
						if (d.Membre(ch, "message") >= 0)
							AjouterTexte(d.TexteDe(ch, "message.content")); // un serveur qui ne fait pas de delta
						const int32 tc = d.Chemin(ch, "delta.tool_calls");
						for (uint32 k = 0; k < d.Taille(tc); ++k) {
							const int32 c = d.Element(tc, k);
							const int32 idxN = d.Membre(c, "index");
							const nkentseu::int32 idx = idxN >= 0 ? static_cast<nkentseu::int32>(d.Nombre(idxN)) : static_cast<nkentseu::int32>(k);
							NkAppelOutil &a = appels[static_cast<nkentseu::usize>(AppelPourIndice(idx))];
							const NkString id = d.TexteDe(c, "id");
							if (!id.Empty())
								a.id = id;
							a.nom.Append(d.TexteDe(c, "function.name"));
							a.arguments.Append(d.TexteDe(c, "function.arguments"));
						}
						const NkString fr = d.TexteDe(ch, "finish_reason");
						if (!fr.Empty())
							finRaison = fr;
						const int32 u = d.Membre(r, "usage");
						if (u >= 0) {
							jetonsEntree = static_cast<nkentseu::uint64>(d.Nombre(d.Membre(u, "prompt_tokens")));
							jetonsSortie = static_cast<nkentseu::uint64>(d.Nombre(d.Membre(u, "completion_tokens")));
						}
						return;
					}
					// ANTHROPIC
					const NkString type = d.TexteDe(r, "type");
					if (type == "error") {
						erreur = d.TexteDe(r, "error.message");
						return;
					}
					if (type == "message_start") {
						jetonsEntree = static_cast<nkentseu::uint64>(d.Nombre(d.Chemin(r, "message.usage.input_tokens")));
						return;
					}
					if (type == "content_block_start") {
						const int32 cb = d.Membre(r, "content_block");
						if (d.TexteDe(cb, "type") == "tool_use") {
							NkAppelOutil &a = appels[static_cast<nkentseu::usize>(
								AppelPourIndice(static_cast<nkentseu::int32>(d.Nombre(d.Membre(r, "index")))))];
							a.id = d.TexteDe(cb, "id");
							a.nom = d.TexteDe(cb, "name");
						} else
							AjouterTexte(d.TexteDe(cb, "text"));
						return;
					}
					if (type == "content_block_delta") {
						const int32 dl = d.Membre(r, "delta");
						const NkString dt = d.TexteDe(dl, "type");
						if (dt == "text_delta")
							AjouterTexte(d.TexteDe(dl, "text"));
						else if (dt == "input_json_delta") {
							NkAppelOutil &a = appels[static_cast<nkentseu::usize>(
								AppelPourIndice(static_cast<nkentseu::int32>(d.Nombre(d.Membre(r, "index")))))];
							a.arguments.Append(d.TexteDe(dl, "partial_json"));
						}
						return;
					}
					if (type == "message_delta") {
						const NkString sr = d.TexteDe(r, "delta.stop_reason");
						if (!sr.Empty())
							finRaison = sr;
						jetonsSortie = static_cast<nkentseu::uint64>(d.Nombre(d.Chemin(r, "usage.output_tokens")));
						return;
					}
					if (type == "message_stop")
						fini = true;
				}

				/// Apres la fin : des arguments vides deviennent « {} », un appel
				/// sans nom est retire (un fragment, pas un appel).
				void Achever() {
					Terminer();
					for (nkentseu::usize k = appels.Size(); k > 0; --k) {
						NkAppelOutil &a = appels[k - 1];
						a.arguments.Trim();
						if (a.arguments.Empty())
							a.arguments = NkString("{}");
						if (a.id.Empty()) {
							char id[32];
							std::snprintf(id, sizeof(id), "appel_%u", static_cast<unsigned>(k));
							a.id = NkString(id);
						}
						if (a.nom.Empty())
							appels.Erase(appels.Begin() + (k - 1));
					}
				}
		};

		/// Le message d'erreur d'un corps de refus, quel que soit le fournisseur.
		inline NkString MessageDeRefus(const NkString &corps) {
			NkJsonDoc d;
			if (!d.Lire(corps))
				return corps.Length() > 240u ? corps.SubStr(0, 240) : corps;
			const int32 r = d.Racine();
			const int32 e = d.Membre(r, "error");
			if (d.EstTexte(e))
				return d.Texte(e);
			if (d.EstObjet(e))
				return d.TexteDe(e, "message");
			const NkString m = d.TexteDe(r, "message");
			return m.Empty() ? d.Ecrire(r) : m;
		}

	} // namespace protocoles

	// ═══════════════════════════════════════════════════════════════════════
	//  LE DIAGNOSTIC D'UNE REPONSE HTTP
	// ═══════════════════════════════════════════════════════════════════════
	inline NkDiagIA NkIaDiagnostiquer(const NkReglagesFournisseur &r, const NkResultatHttpIA &h, NkString &motif) {
		if (h.annule) {
			motif = NkString("Arrete : la reponse a ete interrompue a votre demande.");
			return NkDiagIA::NK_ANNULE;
		}
		if (h.statut == 0u) {
			const NkString &e = h.erreur;
			if (e.Find("timeout") != NkString::npos || e.Find("Timeout") != NkString::npos ||
				e.Find("timed out") != NkString::npos) {
				motif = NkString("Reessayez : pas de reponse dans le delai. C'est NOTRE plafond d'attente, pas une "
								 "panne du modele (le premier appel le charge depuis le disque).");
				return NkDiagIA::NK_DELAI;
			}
			if (e.Find("ne s'est pas lance") != NkString::npos) {
				motif = NkString("Construisez avec NK_ENABLE_TLS=1, ou installez curl : ce depot n'a pas HTTPS et curl "
								 "est introuvable (") +
						e + ")";
				return NkDiagIA::NK_TRANSPORT_ABSENT;
			}
			if (r.genre == NkGenreFournisseur::NK_OLLAMA)
				motif = NkString("Lancez Ollama (« ollama serve », ou l'application) ou corrigez l'adresse : personne "
								 "ne repond a ") +
						r.adresse;
			else if (r.genre == NkGenreFournisseur::NK_OPENAI)
				motif = NkString("Lancez le serveur (LM Studio : Developer > Start Server ; llama.cpp : llama-server) "
								 "ou corrigez l'adresse : personne ne repond a ") +
						r.adresse;
			else
				motif = NkString("Verifiez la connexion internet ou l'adresse : personne ne repond a ") + r.adresse;
			if (!e.Empty()) {
				motif.Append(" (");
				motif.Append(e);
				motif.Append(")");
			}
			return NkDiagIA::NK_SERVEUR_ABSENT;
		}
		if (h.statut == 401u || h.statut == 403u) {
			char b[96];
			std::snprintf(b, sizeof(b), "Verifiez la cle dans Reglages : le service la refuse (%u : ",
						  static_cast<unsigned>(h.statut));
			motif = NkString(b) + protocoles::MessageDeRefus(h.corps) + ")";
			return NkDiagIA::NK_CLE_REFUSEE;
		}
		const NkString msg = protocoles::MessageDeRefus(h.corps);
		if (h.statut == 400u && (msg.Find("does not support tools") != NkString::npos ||
								 msg.Find("tools is not supported") != NkString::npos ||
								 (msg.Find("tool") != NkString::npos && msg.Find("support") != NkString::npos))) {
			motif = NkString("Le modele ne sait pas les outils natifs : les outils passent en texte (") + msg + ")";
			return NkDiagIA::NK_OUTILS_REFUSES;
		}
		if (h.statut == 404u && (msg.Find("model") != NkString::npos || msg.Find("modele") != NkString::npos)) {
			if (r.genre == NkGenreFournisseur::NK_OLLAMA)
				motif = NkString("Installez le modele : « ollama pull ") + r.modele +
						" » -- le serveur repond, mais ce modele n'y est pas (" + msg + ")";
			else
				motif = NkString("Choisissez un modele de la liste (Reglages > Actualiser) : « ") + r.modele +
						" » est inconnu du serveur (" + msg + ")";
			return NkDiagIA::NK_MODELE_ABSENT;
		}
		char b[64];
		std::snprintf(b, sizeof(b), "Le service refuse (%u) : ", static_cast<unsigned>(h.statut));
		motif = NkString(b) + msg;
		return NkDiagIA::NK_ERREUR;
	}

	namespace protocoles {
		inline void EntetesCle(const NkReglagesFournisseur &r, const NkString &cle, NkRequeteHttpIA &rq) {
			if (r.genre == NkGenreFournisseur::NK_ANTHROPIC) {
				NkEnteteIA k;
				k.cle = NkString("x-api-key");
				k.valeur = cle;
				rq.entetes.PushBack(k);
				NkEnteteIA v;
				v.cle = NkString("anthropic-version");
				v.valeur = NkString("2023-06-01");
				rq.entetes.PushBack(v);
			} else if (!cle.Empty()) {
				NkEnteteIA k;
				k.cle = NkString("Authorization");
				k.valeur = NkString("Bearer ") + cle;
				rq.entetes.PushBack(k);
			}
		}
	} // namespace protocoles

	// ═══════════════════════════════════════════════════════════════════════
	//  LISTER LES MODELES
	// ═══════════════════════════════════════════════════════════════════════
	/// Ce que le SERVEUR dit avoir. Rend le diagnostic ; `motif` le dit.
	inline NkDiagIA NkIaListerModeles(const NkReglagesFournisseur &r, const NkString &cle,
									  nkentseu::NkVector<NkConverseModeleInfo> &out, NkString &motif) {
		out.Clear();
		motif = NkString();
		switch (r.genre) {
			case NkGenreFournisseur::NK_OLLAMA: {
				const NkString base = protocoles::BaseSimple(r.adresse, NkAdresseParDefaut(r.genre));
				// D'abord la question courte : NkConverseOllamaModeles nomme mal un
				// serveur absent (« Lancez ollama serve » pour une adresse fausse).
				NkRequeteHttpIA rq;
				rq.post = false;
				rq.url = base + "/api/tags";
				rq.delaiMs = 4000u;
				NkResultatHttpIA h;
				NkHttpIA(rq, h);
				if (h.statut != 200u)
					return NkIaDiagnostiquer(r, h, motif);
				NkString m2;
				if (!NkConverseOllamaModeles(base.CStr(), out, m2)) {
					motif = m2.Empty() ? NkString("Aucun modele installe : « ollama pull qwen2.5:7b » en installe un.") : m2;
					return NkDiagIA::NK_MODELE_ABSENT;
				}
				return NkDiagIA::NK_OK;
			}
			case NkGenreFournisseur::NK_OPENAI:
			case NkGenreFournisseur::NK_ANTHROPIC: {
				if (r.genre == NkGenreFournisseur::NK_ANTHROPIC && cle.Empty()) {
					motif = NkString("Saisissez la cle dans Reglages (ou la variable ANTHROPIC_API_KEY) : aucune cle "
									 "pour ce fournisseur.");
					return NkDiagIA::NK_CLE_ABSENTE;
				}
				NkRequeteHttpIA rq;
				rq.post = false;
				rq.url = (r.genre == NkGenreFournisseur::NK_OPENAI ? protocoles::BaseOpenAI(r.adresse)
																	: protocoles::BaseSimple(r.adresse, NkAdresseParDefaut(r.genre)) + "/v1") +
						 "/models";
				rq.delaiMs = 8000u;
				protocoles::EntetesCle(r, cle, rq);
				NkResultatHttpIA h;
				NkHttpIA(rq, h);
				if (h.statut != 200u)
					return NkIaDiagnostiquer(r, h, motif);
				NkJsonDoc d;
				if (!d.Lire(h.corps)) {
					motif = NkString("Le serveur a repondu, mais pas une liste de modeles lisible.");
					return NkDiagIA::NK_ILLISIBLE;
				}
				const int32 data = d.Membre(d.Racine(), "data");
				for (uint32 k = 0; k < d.Taille(data); ++k) {
					NkConverseModeleInfo m;
					m.nom = d.TexteDe(d.Element(data, k), "id");
					m.famille = d.TexteDe(d.Element(data, k), "display_name");
					m.outils = true; // ces API annoncent les outils pour tous leurs modeles de conversation
					if (!m.nom.Empty())
						out.PushBack(m);
				}
				if (out.Empty()) {
					motif = NkString("Chargez un modele dans le serveur : il n'en liste aucun.");
					return NkDiagIA::NK_MODELE_ABSENT;
				}
				return NkDiagIA::NK_OK;
			}
			case NkGenreFournisseur::NK_CLAUDE_CLI: {
				if (!NkConverseClaudeModeles(out, motif))
					return NkDiagIA::NK_TRANSPORT_ABSENT;
				return NkDiagIA::NK_OK;
			}
			case NkGenreFournisseur::NK_PROCESSUS: {
				// Un modele local se DESIGNE (chemin .gguf ou nom Ollama) : on ne
				// fouille pas le disque de l'utilisateur pour en deviner.
				if (!r.modele.Empty()) {
					NkConverseModeleInfo m;
					m.nom = r.modele;
					out.PushBack(m);
				}
				motif = NkString("Saisissez le modele : le chemin d'un .gguf, ou un nom Ollama installe.");
				return out.Empty() ? NkDiagIA::NK_MODELE_ABSENT : NkDiagIA::NK_OK;
			}
			default: break;
		}
		motif = NkString("genre de fournisseur inconnu");
		return NkDiagIA::NK_ERREUR;
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  TESTER LA CONNEXION
	// ═══════════════════════════════════════════════════════════════════════
	/// Le bouton « Tester la connexion ». ⚠️ AUCUNE GENERATION, rien de facture :
	/// la liste des modeles suffit a prouver l'adresse, la cle et le modele.
	inline NkDiagIA NkIaTesterConnexion(const NkReglagesFournisseur &r, const NkString &cle, NkString &message,
										nkentseu::NkVector<NkConverseModeleInfo> *modeles = nullptr) {
		nkentseu::NkVector<NkConverseModeleInfo> liste;
		if (r.genre == NkGenreFournisseur::NK_PROCESSUS) {
			if (r.adresse.Empty() || !nkentseu::NkFile::Exists(r.adresse.CStr())) {
				message = NkString("Indiquez l'executable du modele local (NKDesignLLM.exe) : « ") + r.adresse +
						  " » n'existe pas.";
				return NkDiagIA::NK_TRANSPORT_ABSENT;
			}
		}
		const NkDiagIA d = NkIaListerModeles(r, cle, liste, message);
		if (modeles)
			*modeles = liste;
		if (d != NkDiagIA::NK_OK)
			return d;
		if (!r.modele.Empty() && r.genre != NkGenreFournisseur::NK_PROCESSUS) {
			bool trouve = false;
			for (nkentseu::usize i = 0; i < liste.Size(); ++i)
				trouve = trouve || liste[i].nom == r.modele ||
						 (r.genre == NkGenreFournisseur::NK_OLLAMA && liste[i].nom == r.modele + ":latest");
			if (!trouve) {
				NkString installes;
				for (nkentseu::usize i = 0; i < liste.Size() && i < 6u; ++i) {
					if (i)
						installes.Append(", ");
					installes.Append(liste[i].nom);
				}
				if (r.genre == NkGenreFournisseur::NK_OLLAMA)
					message = NkString("Installez le modele : « ollama pull ") + r.modele +
							  " » -- le serveur repond, mais il ne l'a pas (installes : " + installes + ")";
				else
					message = NkString("Choisissez un modele de la liste : « ") + r.modele +
							  " » est inconnu du serveur (il propose : " + installes + ")";
				return NkDiagIA::NK_MODELE_ABSENT;
			}
		}
		char b[160];
		std::snprintf(b, sizeof(b), "Connecte : %u modele(s) disponible(s)", static_cast<unsigned>(liste.Size()));
		message = NkString(b);
		if (!r.modele.Empty()) {
			message.Append(", « ");
			message.Append(r.modele);
			message.Append(" » pret");
		}
		message.Append(r.Distant() ? " -- l'invite QUITTE ce PC." : " -- rien ne quitte ce PC.");
		return NkDiagIA::NK_OK;
	}

	/// Les outils NATIFS conviennent-ils a ce fournisseur et ce modele ?
	/// `outilsModele` : ce que le serveur a dit du modele (Ollama : la capacite
	/// « tools » de /api/show) ; -1 = inconnu.
	inline bool NkIaOutilsNatifs(const NkReglagesFournisseur &r, nkentseu::int32 outilsModele) {
		if (r.outils == NkModeOutils::NK_NATIFS)
			return r.genre != NkGenreFournisseur::NK_CLAUDE_CLI && r.genre != NkGenreFournisseur::NK_PROCESSUS;
		if (r.outils == NkModeOutils::NK_TEXTE)
			return false;
		switch (r.genre) {
			case NkGenreFournisseur::NK_OLLAMA: return outilsModele != 0; // inconnu : on essaie (refus -> texte)
			case NkGenreFournisseur::NK_OPENAI:
			case NkGenreFournisseur::NK_ANTHROPIC: return true;
			default: return false;
		}
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  DISCUTER
	// ═══════════════════════════════════════════════════════════════════════
	/// UNE TRANSCRIPTION pour les dorsaux sans messages (CLI, processus) : le
	/// systeme, puis l'echange, puis la consigne de repondre au dernier tour.
	inline NkString NkIaTranscription(const NkRequeteIA &q) {
		NkString t;
		for (nkentseu::usize i = 0; i < q.messages.Size(); ++i) {
			const NkMessageIA &m = q.messages[i];
			if (m.role == NkRoleIA::NK_SYSTEME) {
				t.Append(m.texte);
				t.Append("\n\n## Echange\n");
				continue;
			}
			if (m.role == NkRoleIA::NK_OUTIL) {
				t.Append("resultat> ");
				t.Append(protocoles::ResultatEnTexte(m));
			} else {
				t.Append(m.role == NkRoleIA::NK_ASSISTANT ? "assistant> " : "utilisateur> ");
				t.Append(m.texte);
			}
			t.Append("\n");
		}
		t.Append("\nReponds au dernier message, en francais. Pour agir, ecris les blocs <outil> decrits plus haut.\n");
		return t;
	}

	/// LA CONVERSATION. Bloquante : l'appeler sur un fil de travail
	/// (NkConverseChatFlux.h). `surTexte` recoit chaque morceau de texte au fil
	/// de l'eau ; `annuler` (lu entre deux paquets) arrete la lecture.
	inline bool NkIaDiscuter(const NkReglagesFournisseur &r, const NkString &cle, const NkRequeteIA &q, NkReponseIA &rep,
							 const nkentseu::NkFunction<void(const NkString &)> &surTexte,
							 const nkentseu::NkAtomicBool *annuler = nullptr) {
		rep = NkReponseIA();
		if (r.modele.Empty() && r.genre != NkGenreFournisseur::NK_CLAUDE_CLI) {
			rep.diag = NkDiagIA::NK_MODELE_ABSENT;
			rep.erreur = NkString("Choisissez un modele (Reglages > Actualiser la liste) : aucun n'est choisi.");
			return false;
		}
		if (r.genre == NkGenreFournisseur::NK_CLAUDE_CLI || r.genre == NkGenreFournisseur::NK_PROCESSUS) {
			// Ces dorsaux rendent la reponse ENTIERE : le flux la recoit d'un coup.
			NkConverseRequest req;
			req.prompt = NkIaTranscription(q);
			NkConverseReply out;
			bool ok = false;
			if (r.genre == NkGenreFournisseur::NK_CLAUDE_CLI) {
				NkConverseBackendClaude c;
				c.compte = r.compte;
				if (!r.modele.Empty())
					c.modele = r.modele;
				ok = c.Complete(req, out);
				rep.corpsEnvoye = c.DerniereInvite();
			} else {
				NkConverseBackendProcessus p;
				p.nom = NkString("local");
				NkString g("\"");
				g.Append(r.adresse);
				g.Append("\" --invite \"{invite}\" --sortie \"{sortie}\" --modele=\"");
				g.Append(r.modele);
				g.Append("\"");
				char b[64];
				if (r.contexte > 0) {
					std::snprintf(b, sizeof(b), " --contexte=%d", static_cast<int>(r.contexte));
					g.Append(b);
				}
				if (r.sortieMax > 0) {
					std::snprintf(b, sizeof(b), " --tokens=%d", static_cast<int>(r.sortieMax));
					g.Append(b);
				}
				p.gabarit = g;
				p.invitePath = transportdetail::FichierTemporaire("invite.txt");
				p.sortiePath = transportdetail::FichierTemporaire("sortie.txt");
				ok = p.Complete(req, out);
				rep.corpsEnvoye = p.DerniereInvite();
				nkentseu::NkFile::Delete(p.invitePath.CStr());
				nkentseu::NkFile::Delete(p.sortiePath.CStr());
			}
			if (!ok) {
				rep.diag = NkDiagIA::NK_TRANSPORT_ABSENT;
				rep.erreur = out.error;
				return false;
			}
			rep.texte = out.text;
			if (surTexte != nullptr)
				surTexte(out.text);
			rep.ok = true;
			rep.diag = NkDiagIA::NK_OK;
			return true;
		}
		if (r.genre == NkGenreFournisseur::NK_ANTHROPIC && cle.Empty()) {
			rep.diag = NkDiagIA::NK_CLE_ABSENTE;
			rep.erreur = NkString("Saisissez la cle dans Reglages (ou la variable ANTHROPIC_API_KEY) : aucune cle pour "
								  "ce fournisseur.");
			return false;
		}
		NkRequeteHttpIA rq;
		rq.post = true;
		switch (r.genre) {
			case NkGenreFournisseur::NK_OLLAMA:
				rq.url = protocoles::BaseSimple(r.adresse, NkAdresseParDefaut(r.genre)) + "/api/chat";
				rq.corps = protocoles::CorpsOllama(r, q, true);
				break;
			case NkGenreFournisseur::NK_OPENAI:
				rq.url = protocoles::BaseOpenAI(r.adresse) + "/chat/completions";
				rq.corps = protocoles::CorpsOpenAI(r, q, true);
				break;
			default:
				rq.url = protocoles::BaseSimple(r.adresse, NkAdresseParDefaut(r.genre)) + "/v1/messages";
				rq.corps = protocoles::CorpsAnthropic(r, q, true);
				break;
		}
		protocoles::EntetesCle(r, cle, rq);
		rep.corpsEnvoye = rq.corps;
		protocoles::Decodeur dec;
		dec.genre = r.genre;
		dec.surTexte = surTexte;
		nkentseu::uint32 statutVu = 0u;
		NkString corpsRefus;
		const NkFluxHttpIA flux = [&](nkentseu::uint32 st, const char *d, nkentseu::uint32 n) {
			statutVu = st;
			if (annuler && annuler->Load())
				return false;
			if (st != 200u) {
				corpsRefus.Append(d, static_cast<NkString::SizeType>(n));
				return true;
			}
			dec.Pousser(d, n);
			return true;
		};
		NkResultatHttpIA h;
		NkHttpIA(rq, h, &flux);
		rep.statut = h.statut;
		rep.viaCurl = h.viaCurl;
		if (h.statut != 200u || h.annule) {
			if (h.statut != 0u && h.corps.Empty())
				h.corps = corpsRefus;
			rep.diag = NkIaDiagnostiquer(r, h, rep.erreur);
			rep.texte = dec.texte; // ce qui a ete lu avant l'arret reste a l'ecran
			return false;
		}
		// Un serveur qui n'a pas fait de flux (corps entier d'un coup, sans
		// decoupe en lignes) : le decodeur le lit quand meme a la fin.
		if (dec.lignes == 0u && !h.corps.Empty())
			dec.Pousser(h.corps.CStr(), static_cast<nkentseu::uint32>(h.corps.Length()));
		dec.Achever();
		rep.texte = dec.texte;
		rep.appels = dec.appels;
		rep.finRaison = dec.finRaison;
		rep.jetonsEntree = dec.jetonsEntree;
		rep.jetonsSortie = dec.jetonsSortie;
		if (!dec.erreur.Empty()) {
			NkResultatHttpIA faux = h;
			faux.statut = 400u;
			faux.corps = NkString("{\"error\":");
			NkJsonChaine(dec.erreur, faux.corps);
			faux.corps.Append("}");
			rep.diag = NkIaDiagnostiquer(r, faux, rep.erreur);
			if (rep.diag == NkDiagIA::NK_ERREUR && dec.erreur.Find("not found") != NkString::npos) {
				rep.diag = NkDiagIA::NK_MODELE_ABSENT;
				rep.erreur = NkString("Installez le modele : « ollama pull ") + r.modele + " » (" + dec.erreur + ")";
			}
			return false;
		}
		if (rep.texte.Empty() && rep.appels.Empty()) {
			rep.diag = NkDiagIA::NK_ILLISIBLE;
			rep.erreur = NkString("Le modele a repondu 200 sans texte ni outil -- ce n'est pas un succes.");
			return false;
		}
		(void)statutVu;
		rep.ok = true;
		rep.diag = NkDiagIA::NK_OK;
		return true;
	}

} // namespace nkentseu::converse
