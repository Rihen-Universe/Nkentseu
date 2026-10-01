// =============================================================================
// NkEditeurIAOutils.cpp — LES OUTILS DE L'IA, et la description de l'editeur
//
// A QUOI SERT CE FICHIER
//   Le modele branche agit sur l'editeur par des OUTILS : chacun est une
//   commande de l'editeur (NkEditeurActions.h, NkEditeurContenu.h, les
//   scripts), avec un nom, une phrase et le schema JSON de ses parametres. Et
//   le message systeme qui lui fait « comprendre Unkeny des la connexion » est
//   ENGENDRE ici, depuis l'editeur lui-meme : le catalogue des acteurs vient de
//   NkActeurSimInfo, les composants de NkComposantEditeurNom, les noeuds
//   Blueprint de NkBpProtos, les types d'assets de NkEditeurNatureFichier, la
//   scene de la scene. Rien n'est recopie a la main : un acteur ou un noeud
//   ajoute demain apparait a l'IA sans une ligne ici.
//
// ⚠️ LES GARDE-FOUS VIVENT ICI, PAS DANS LA CONSIGNE AU MODELE
//   Dire au modele « ne supprime rien sans demander » ne protege rien : un
//   modele local de 7 milliards de parametres l'oubliera. Ce qui protege :
//     - la PHOTO prise avant tout outil qui modifie (NkEditeurIAExecuter) : Ctrl+Z
//       la rend, et les fichiers ecrits sont dans le journal de l'historique ;
//     - NkEditeurIAIrreversible, que la boucle consulte AVANT d'executer : la
//       confirmation est demandee par l'editeur, le modele n'a pas a s'en souvenir ;
//     - les chemins : RELATIFS au projet, sans « .. », jamais absolus -- un
//       modele ne peut pas ecrire dans C:/Windows en se trompant de dossier ;
//     - en JEU, rien ne se modifie (l'historique est celui de l'edition).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Ia/NkEditeurIA.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurInterface.h"
#include "Script/NkBpCatalogue.h"
#include "Script/NkEditeurScripts.h"

#include "NKConverse/NkConverseJson.h"
#include "NKEditorKit/Components/NkContentBrowserDisque.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Script/NkUnkenyScript.h"
#include "Unkeny/Script/NkUnkenyScriptABI.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using converse::NkAppelOutil;
		using converse::NkJsonDoc;
		using converse::NkJsonChaine;

		namespace {

			// =================================================================
			// LES NOMS : « Caisse », « caisse », « CAISSE », « Gelée », « gelee »
			// designent la meme chose. Un modele local ecrit comme il veut.
			// =================================================================
			NkString Normaliser(const char *s) {
				NkString o;
				const unsigned char *p = reinterpret_cast<const unsigned char *>(s ? s : "");
				while (*p) {
					unsigned char c = *p;
					if (c == 0xC3u && p[1]) {
						const unsigned char d = p[1];
						char r = 0;
						if ((d >= 0xA0u && d <= 0xA5u) || (d >= 0x80u && d <= 0x85u))
							r = 'a';
						else if (d == 0xA7u || d == 0x87u)
							r = 'c';
						else if ((d >= 0xA8u && d <= 0xABu) || (d >= 0x88u && d <= 0x8Bu))
							r = 'e';
						else if ((d >= 0xACu && d <= 0xAFu) || (d >= 0x8Cu && d <= 0x8Fu))
							r = 'i';
						else if ((d >= 0xB2u && d <= 0xB6u) || (d >= 0x92u && d <= 0x96u))
							r = 'o';
						else if ((d >= 0xB9u && d <= 0xBCu) || (d >= 0x99u && d <= 0x9Cu))
							r = 'u';
						if (r)
							o.Append(r);
						p += 2;
						continue;
					}
					if (c == 0xC5u && (p[1] == 0x93u || p[1] == 0x92u)) {
						o.Append("oe");
						p += 2;
						continue;
					}
					if (c >= 0x80u) { // un autre caractere non ASCII : on le saute
						++p;
						continue;
					}
					if (c >= 'A' && c <= 'Z')
						c = static_cast<unsigned char>(c - 'A' + 'a');
					if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
						o.Append(static_cast<char>(c));
					else if ((c == ' ' || c == '_' || c == '-' || c == '.') && !o.Empty() && !o.EndsWith("-"))
						o.Append('-');
					++p;
				}
				while (o.EndsWith("-"))
					o.PopBack();
				return o;
			}

			/// L'acteur du catalogue dont le nom correspond (exact, puis prefixe).
			bool ActeurParNom(const char *nom, NkActeurSim &out) {
				const NkString v = Normaliser(nom);
				if (v.Empty())
					return false;
				int32 prefixe = -1;
				for (int32 i = 0; i < static_cast<int32>(NkActeurSim::NK_COUNT); ++i) {
					const NkString k = Normaliser(NkActeurSimInfo(static_cast<NkActeurSim>(i)).nom);
					if (k == v) {
						out = static_cast<NkActeurSim>(i);
						return true;
					}
					if (prefixe < 0 && (k.StartsWith(v.CStr()) || v.StartsWith(k.CStr())))
						prefixe = i;
				}
				if (prefixe >= 0) {
					out = static_cast<NkActeurSim>(prefixe);
					return true;
				}
				return false;
			}

			bool ComposantParNom(const char *nom, NkComposantEditeur &out) {
				const NkString v = Normaliser(nom);
				struct Alias {
						const char *mot;
						NkComposantEditeur c;
				};
				static const Alias kAlias[] = {
					{"corps", NkComposantEditeur::NK_CORPS},		   {"rigide", NkComposantEditeur::NK_CORPS},
					{"corps-rigide", NkComposantEditeur::NK_CORPS},	   {"mou", NkComposantEditeur::NK_CORPS_MOU},
					{"son", NkComposantEditeur::NK_SOURCE},			   {"audio", NkComposantEditeur::NK_SOURCE},
					{"source", NkComposantEditeur::NK_SOURCE},		   {"lumiere", NkComposantEditeur::NK_LUMIERE},
					{"particules", NkComposantEditeur::NK_EMETTEUR},   {"emetteur", NkComposantEditeur::NK_EMETTEUR},
					{"collision", NkComposantEditeur::NK_COLLISIONNEUR}, {"boite", NkComposantEditeur::NK_COLLISIONNEUR},
				};
				for (int32 i = 0; i < static_cast<int32>(NkComposantEditeur::NK_COUNT); ++i)
					if (Normaliser(NkComposantEditeurNom(static_cast<NkComposantEditeur>(i))) == v) {
						out = static_cast<NkComposantEditeur>(i);
						return true;
					}
				for (const Alias &a : kAlias)
					if (v == a.mot) {
						out = a.c;
						return true;
					}
				return false;
			}

			// =================================================================
			// LES ARGUMENTS
			// =================================================================
			struct Args {
					NkJsonDoc d;
					int32 r = -1;
					bool A(const char *k) const {
						return d.Membre(r, k) >= 0 && d.GenreDe(d.Membre(r, k)) != NkJsonDoc::NUL;
					}
					NkString T(const char *k, const char *defaut = "") const {
						return d.Texte(d.Membre(r, k), defaut);
					}
					float32 F(const char *k, float32 defaut = 0.f) const {
						return static_cast<float32>(d.Nombre(d.Membre(r, k), static_cast<double>(defaut)));
					}
					bool B(const char *k, bool defaut = false) const {
						return d.Booleen(d.Membre(r, k), defaut);
					}
					uint64 U(const char *k) const {
						const int32 i = d.Membre(r, k);
						const NkString s = d.Texte(i);
						uint64 v = 0u;
						for (NkString::SizeType j = 0; j < s.Length() && s[j] >= '0' && s[j] <= '9'; ++j)
							v = v * 10u + static_cast<uint64>(s[j] - '0');
						return v;
					}
			};

			NkString Nombre(float32 v) {
				char b[32];
				std::snprintf(b, sizeof(b), "%.3g", static_cast<double>(v));
				return NkString(b);
			}
			NkString Pos(const NkVec2f &p) {
				char b[64];
				std::snprintf(b, sizeof(b), "(%.2f ; %.2f)", static_cast<double>(p.x), static_cast<double>(p.y));
				return NkString(b);
			}

			NkResultatOutilIA Refus(const char *pourquoi) {
				NkResultatOutilIA r;
				r.ok = false;
				r.texte = NkString("REFUS : ") + pourquoi;
				return r;
			}
			NkResultatOutilIA Refus(const NkString &pourquoi) {
				return Refus(pourquoi.CStr());
			}

			// =================================================================
			// LES ENTITES
			// =================================================================
			const char *NomDe(NkEditeurModele &m, ecs::NkEntityId id) {
				const NkEtiquette *e = m.scene.Monde().Get<NkEtiquette>(id);
				return e != nullptr && e->nom[0] != '\0' ? e->nom : "(sans nom)";
			}
			NkVector<ecs::NkEntityId> Entites(NkEditeurModele &m) {
				NkVector<ecs::NkEntityId> v;
				m.scene.Monde().Query<NkTransform2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &) { v.PushBack(id); });
				return v;
			}
			/// L'entite d'un uid ; un refus nomme sinon.
			bool EntiteDe(NkEditeurModele &m, uint64 uid, ecs::NkEntityId &id, NkString &pourquoi) {
				if (uid == 0u) {
					pourquoi = NkString("uid manquant : lire_scene donne l'uid de chaque entite");
					return false;
				}
				id = m.scene.EntiteParUid(uid);
				if (!id.IsValid() || !m.scene.Monde().IsAlive(id)) {
					char b[96];
					std::snprintf(b, sizeof(b), "aucune entite d'uid %llu (lire_scene les liste)", static_cast<unsigned long long>(uid));
					pourquoi = NkString(b);
					return false;
				}
				return true;
			}
			NkString ComposantsDe(NkEditeurModele &m, ecs::NkEntityId id) {
				NkString s;
				for (int32 c = 0; c < static_cast<int32>(NkComposantEditeur::NK_COUNT); ++c)
					if (NkEditeurAUnComposant(m, id, static_cast<NkComposantEditeur>(c))) {
						if (!s.Empty())
							s.Append(", ");
						s.Append(Normaliser(NkComposantEditeurNom(static_cast<NkComposantEditeur>(c))));
					}
				return s;
			}
			void Selectionner(NkEditeurModele &m, ecs::NkEntityId id) {
				m.selection = id;
				m.aSelection = id.IsValid();
			}

			// =================================================================
			// LES CHEMINS : RELATIFS au projet, sans « .. »
			// =================================================================
			bool CheminDuProjet(NkEditeurModele &m, const NkString &relBrut, NkString &abs, NkString &rel, NkString &pourquoi) {
				rel = relBrut;
				rel.Trim();
				for (NkString::SizeType i = 0; i < rel.Length(); ++i)
					if (rel[i] == '\\')
						rel[i] = '/';
				while (rel.StartsWith("./"))
					rel = rel.SubStr(2);
				if (rel.Empty()) {
					pourquoi = NkString("chemin vide");
					return false;
				}
				if (rel.StartsWith("/") || rel.Find(':') != NkString::npos || rel.Find("..") != NkString::npos) {
					pourquoi = NkString("« ") + rel + " » sort du projet : les chemins sont RELATIFS au projet, sans « .. »";
					return false;
				}
				abs = NkEditeurDossierProjet(m) + rel;
				return true;
			}
			NkString DossierDe(const NkString &abs) {
				const NkString::SizeType p = abs.RFind("/");
				const NkString::SizeType q = abs.RFind("\\");
				NkString::SizeType k = p == NkString::npos ? q : (q == NkString::npos ? p : (p > q ? p : q));
				return k == NkString::npos ? NkString() : abs.SubStr(0, k);
			}

			// =================================================================
			// LES OUTILS
			// =================================================================
			const char *kSchemaVide = "{\"type\":\"object\",\"properties\":{}}";
			const char *kSchemaUid = "{\"type\":\"object\",\"properties\":{\"uid\":{\"type\":\"integer\",\"description\":\"l'identifiant stable de l'entite (lire_scene)\"}},\"required\":[\"uid\"]}";

			NkVector<NkOutilEditeurIA> &Table() {
				static NkVector<NkOutilEditeurIA> t;
				if (!t.Empty())
					return t;
				auto O = [&](const char *nom, const char *desc, const char *schema, bool modifie, bool irreversible = false) {
					NkOutilEditeurIA o;
					o.nom = nom;
					o.description = desc;
					o.schema = schema;
					o.modifie = modifie;
					o.irreversible = irreversible;
					t.PushBack(o);
				};
				O("lire_scene", "Liste les entites de la scene ouverte (uid, nom, type, position, composants), la selection et la vue.",
				  kSchemaVide, false);
				O("lire_entite", "Rend TOUTES les valeurs d'une entite, dans la forme de sauvegarde de la scene (celle que modifier_valeurs accepte).",
				  kSchemaUid, false);
				O("creer_acteur",
				  "Pose un acteur du catalogue (caisse, balle, blob, eau, tissu...) en (x, y) ; sans x/y : au centre de la vue. Il devient la selection.",
				  "{\"type\":\"object\",\"properties\":{\"acteur\":{\"type\":\"string\",\"description\":\"un acteur du catalogue\"},"
				  "\"x\":{\"type\":\"number\"},\"y\":{\"type\":\"number\"},\"nom\":{\"type\":\"string\"}},\"required\":[\"acteur\"]}",
				  true);
				O("creer_entite", "Cree une entite VIDE (un transform et un nom), puis ses composants si on les donne.",
				  "{\"type\":\"object\",\"properties\":{\"nom\":{\"type\":\"string\"},\"x\":{\"type\":\"number\"},\"y\":{\"type\":\"number\"},"
				  "\"composants\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},\"required\":[\"nom\"]}",
				  true);
				O("modifier_entite",
				  "Change le nom, la position (m), la rotation (degres), l'echelle, l'activite ou le parent (uid, 0 = racine) d'une entite.",
				  "{\"type\":\"object\",\"properties\":{\"uid\":{\"type\":\"integer\"},\"nom\":{\"type\":\"string\"},\"x\":{\"type\":\"number\"},"
				  "\"y\":{\"type\":\"number\"},\"rotation\":{\"type\":\"number\"},\"echelle_x\":{\"type\":\"number\"},\"echelle_y\":{\"type\":\"number\"},"
				  "\"active\":{\"type\":\"boolean\"},\"parent\":{\"type\":\"integer\"}},\"required\":[\"uid\"]}",
				  true);
				O("modifier_valeurs",
				  "Change N'IMPORTE QUELLE valeur d'une entite (sprite, collisionneur, corps, lumiere, emetteur, composants du jeu...) : "
				  "un objet JSON fusionne dans la forme que lire_entite rend. Une couleur peut s'ecrire \"#RRGGBB\".",
				  "{\"type\":\"object\",\"properties\":{\"uid\":{\"type\":\"integer\"},\"valeurs\":{\"type\":\"object\"}},\"required\":[\"uid\",\"valeurs\"]}",
				  true);
				O("ajouter_composant", "Ajoute un composant a une entite (pour un corps mou : sa matiere).",
				  "{\"type\":\"object\",\"properties\":{\"uid\":{\"type\":\"integer\"},\"composant\":{\"type\":\"string\"},\"matiere\":{\"type\":\"string\"}},"
				  "\"required\":[\"uid\",\"composant\"]}",
				  true);
				O("retirer_composant", "Retire un composant d'une entite.",
				  "{\"type\":\"object\",\"properties\":{\"uid\":{\"type\":\"integer\"},\"composant\":{\"type\":\"string\"}},\"required\":[\"uid\",\"composant\"]}",
				  true);
				O("supprimer_entite", "Supprime une entite (Ctrl+Z la rend).", kSchemaUid, true);
				O("dupliquer_entite", "Copie une entite et ses composants, decalee de 0,5 m.", kSchemaUid, true);
				O("selectionner", "Choisit l'entite (uid 0 : rien) ; les Details la montrent.",
				  "{\"type\":\"object\",\"properties\":{\"uid\":{\"type\":\"integer\"}},\"required\":[\"uid\"]}", false);
				O("poser_script", "Pose un script sur une entite : un Blueprint du projet (\"Contenu/Scripts/X.nkbp\") ou une classe C++ (\"cpp:X\").",
				  "{\"type\":\"object\",\"properties\":{\"uid\":{\"type\":\"integer\"},\"script\":{\"type\":\"string\"}},\"required\":[\"uid\",\"script\"]}",
				  true);
				O("commande", "Execute une commande de l'editeur (jouer, arreter, enregistrer, annuler, refaire, cadrer...).",
				  "{\"type\":\"object\",\"properties\":{\"nom\":{\"type\":\"string\"}},\"required\":[\"nom\"]}", false);
				O("lister_contenu", "Liste un dossier du projet (\"\" : la racine du projet ; \"Contenu/Textures\"...), avec la nature de chaque fichier.",
				  "{\"type\":\"object\",\"properties\":{\"dossier\":{\"type\":\"string\"}}}", false);
				O("lire_fichier", "Lit un fichier TEXTE du projet (script, GDD, .json...).",
				  "{\"type\":\"object\",\"properties\":{\"chemin\":{\"type\":\"string\"}},\"required\":[\"chemin\"]}", false);
				O("ecrire_fichier", "Ecrit un fichier texte du projet (le cree, ou l'ECRASE apres confirmation).",
				  "{\"type\":\"object\",\"properties\":{\"chemin\":{\"type\":\"string\"},\"texte\":{\"type\":\"string\"}},\"required\":[\"chemin\",\"texte\"]}",
				  true);
				O("supprimer_fichier", "Supprime un fichier du projet. IRREVERSIBLE hors de cette session : l'utilisateur confirme.",
				  "{\"type\":\"object\",\"properties\":{\"chemin\":{\"type\":\"string\"}},\"required\":[\"chemin\"]}", true, true);
				O("creer_dossier", "Cree un dossier du projet (\"Contenu/Niveaux\").",
				  "{\"type\":\"object\",\"properties\":{\"chemin\":{\"type\":\"string\"}},\"required\":[\"chemin\"]}", true);
				O("ecrire_script_cpp",
				  "Ecrit un script C++ Contenu/Scripts/<nom>.cpp (sans code : le modele commente, qui liste toute l'API). Recompile et recharge a chaud.",
				  "{\"type\":\"object\",\"properties\":{\"nom\":{\"type\":\"string\"},\"code\":{\"type\":\"string\"}},\"required\":[\"nom\"]}", true);
				O("ecrire_blueprint",
				  "Ecrit un Blueprint Contenu/Scripts/<nom>.nkbp : des noeuds du catalogue (id, type, x, y, valeurs par defaut) et des liens "
				  "(de, sortie, vers, entree). Il est COMPILE ; une erreur designe son noeud.",
				  "{\"type\":\"object\",\"properties\":{\"nom\":{\"type\":\"string\"},\"noeuds\":{\"type\":\"array\",\"items\":{\"type\":\"object\","
				  "\"properties\":{\"id\":{\"type\":\"string\"},\"type\":{\"type\":\"string\"},\"x\":{\"type\":\"number\"},\"y\":{\"type\":\"number\"},"
				  "\"valeurs\":{\"type\":\"object\"}}}},\"liens\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{"
				  "\"de\":{\"type\":\"string\"},\"sortie\":{\"type\":\"string\"},\"vers\":{\"type\":\"string\"},\"entree\":{\"type\":\"string\"}}}}},"
				  "\"required\":[\"nom\",\"noeuds\"]}",
				  true);
				O("ecrire_gdd",
				  "Ecrit dans le document de conception du jeu (Documents/GDD.md) : ajoute une section, ou remplace tout (confirmation).",
				  "{\"type\":\"object\",\"properties\":{\"texte\":{\"type\":\"string\"},\"section\":{\"type\":\"string\"},"
				  "\"mode\":{\"type\":\"string\",\"enum\":[\"ajouter\",\"remplacer\"]}},\"required\":[\"texte\"]}",
				  true);
				return t;
			}

			const NkOutilEditeurIA *Outil(const char *nom) {
				const NkString n = Normaliser(nom);
				for (const NkOutilEditeurIA &o : Table()) {
					NkString k = Normaliser(o.nom);
					if (k == n)
						return &o;
				}
				return nullptr;
			}

			// --- Les commandes de l'editeur ------------------------------------------
			struct Commande {
					const char *nom;
					const char *quoi;
					int32 action; ///< NkActionEditeur, ou -1 : traitee a part
			};
			const Commande kCommandes[] = {
				{"jouer", "lance la scene (etat Jeu)", NK_A_JOUER},
				{"pause", "met le jeu en pause", NK_A_PAUSE},
				{"arreter", "arrete le jeu et rend la scene d'avant Jouer", NK_A_ARRETER},
				{"pas", "avance d'un pas de simulation", NK_A_PAS},
				{"enregistrer", "enregistre la scene (ECRASE le fichier : confirmation)", -1},
				{"annuler", "Ctrl+Z : defait le dernier geste", NK_A_ANNULER},
				{"refaire", "Ctrl+Y : refait le geste defait", NK_A_REFAIRE},
				{"cadrer", "la vue montre toute la scene", NK_A_CADRER},
				{"cadrer_selection", "la vue se centre sur la selection", NK_A_CADRER_SELECTION},
				{"grille", "montre / cache la grille", NK_A_GRILLE},
				{"collisionneurs", "montre / cache les collisionneurs", NK_A_COLLISIONNEURS},
				{"creer_prefab", "fait un prefab (.nkprefab) de la selection", NK_A_CREER_PREFAB},
				{"nouvelle_scene", "une scene neuve (confirmation si la courante est modifiee)", -2},
			};
			const Commande *CommandeDe(const char *nom) {
				const NkString n = Normaliser(nom);
				for (const Commande &c : kCommandes) {
					NkString k = Normaliser(c.nom);
					if (k == n)
						return &c;
				}
				return nullptr;
			}

			// =================================================================
			// LES EXECUTANTS
			// =================================================================
			NkResultatOutilIA LireScene(NkEditeurModele &m) {
				NkResultatOutilIA r;
				r.ok = true;
				NkString j("{\"etat\":\"");
				j.Append(m.etat == NkEtatJeu::NK_EDITION ? "edition" : (m.etat == NkEtatJeu::NK_JEU ? "jeu" : "pause"));
				j.Append("\",\"scene\":");
				NkJsonChaine(m.chemin.Empty() ? "(non enregistree)" : m.chemin.CStr(), j);
				const NkVue2D &cam = m.scene.Camera();
				char b[160];
				std::snprintf(b, sizeof(b), ",\"vue\":{\"centre\":[%.3f,%.3f],\"zoom\":%.3f}", static_cast<double>(cam.Centre().x),
							  static_cast<double>(cam.Centre().y), static_cast<double>(cam.Zoom()));
				j.Append(b);
				std::snprintf(b, sizeof(b), ",\"selection\":%llu,\"entites\":[",
							  static_cast<unsigned long long>(m.aSelection ? m.scene.Uid(m.selection) : 0u));
				j.Append(b);
				const NkVector<ecs::NkEntityId> es = Entites(m);
				for (usize i = 0; i < es.Size(); ++i) {
					const ecs::NkEntityId id = es[i];
					const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(id);
					if (i)
						j.Append(',');
					std::snprintf(b, sizeof(b), "{\"uid\":%llu,\"nom\":", static_cast<unsigned long long>(m.scene.AssurerUid(id)));
					j.Append(b);
					NkJsonChaine(NomDe(m, id), j);
					j.Append(",\"type\":");
					NkJsonChaine(NkEditeurTypeDe(m.scene, id), j);
					std::snprintf(b, sizeof(b), ",\"position\":[%.3f,%.3f],\"rotation\":%.2f,\"active\":%s",
								  static_cast<double>(t ? t->position.x : 0.f), static_cast<double>(t ? t->position.y : 0.f),
								  static_cast<double>(t ? t->rotation * 57.2957795f : 0.f), m.scene.EstActiveSoi(id) ? "true" : "false");
					j.Append(b);
					const ecs::NkEntityId p = m.scene.Parent(id);
					if (p.IsValid()) {
						std::snprintf(b, sizeof(b), ",\"parent\":%llu", static_cast<unsigned long long>(m.scene.Uid(p)));
						j.Append(b);
					}
					j.Append(",\"composants\":[");
					bool v = false;
					for (int32 c = 0; c < static_cast<int32>(NkComposantEditeur::NK_COUNT); ++c)
						if (NkEditeurAUnComposant(m, id, static_cast<NkComposantEditeur>(c))) {
							if (v)
								j.Append(',');
							v = true;
							NkJsonChaine(Normaliser(NkComposantEditeurNom(static_cast<NkComposantEditeur>(c))), j);
						}
					j.Append("]}");
				}
				j.Append("]}");
				r.texte = j;
				return r;
			}

			/// L'objet JSON d'une entite, tel que la sauvegarde l'ecrit.
			bool EntiteJson(NkEditeurModele &m, uint64 uid, NkJsonDoc &d, int32 &e, NkString &pourquoi) {
				NkString json;
				if (!NkSauverSceneJSON(m.scene, json, m.RessourcesScene())) {
					pourquoi = NkString("la scene ne s'ecrit pas en JSON");
					return false;
				}
				if (!d.Lire(json, &pourquoi))
					return false;
				const int32 ents = d.Membre(d.Racine(), "entites");
				char u[32];
				std::snprintf(u, sizeof(u), "%llu", static_cast<unsigned long long>(uid));
				for (uint32 k = 0; k < d.Taille(ents); ++k) {
					const int32 x = d.Element(ents, k);
					if (d.Texte(d.Membre(x, "uid")) == u) {
						e = x;
						return true;
					}
				}
				pourquoi = NkString("entite absente de la sauvegarde");
				return false;
			}

			/// « #RRGGBB » / « #RRGGBBAA » -> l'entier RGBA de la sauvegarde.
			void CouleursEnEntiers(NkJsonDoc &p, int32 o, uint32 profondeur = 0u) {
				if (profondeur > 16u)
					return;
				if (p.EstObjet(o)) {
					for (uint32 k = 0; k < p.Taille(o); ++k) {
						const int32 v = p.Element(o, k);
						const NkString cle(p.Cle(o, k));
						if (p.EstTexte(v) && cle.Find("couleur") != NkString::npos && p.Texte(v).StartsWith("#")) {
							const NkString h = p.Texte(v).SubStr(1);
							uint32 x = static_cast<uint32>(std::strtoul(h.CStr(), nullptr, 16));
							if (h.Length() == 6u)
								x = (x << 8) | 0xFFu;
							char b[24];
							std::snprintf(b, sizeof(b), "%u", static_cast<unsigned>(x));
							// le lexeme exact (pas un %.9g arrondi)
							NkJsonDoc t;
							t.Lire(b, std::strlen(b));
							p.Poser(o, cle.CStr(), p.Copier(t, t.Racine()));
						} else
							CouleursEnEntiers(p, v, profondeur + 1u);
					}
				}
			}

			NkResultatOutilIA ModifierValeurs(NkEditeurModele &m, const Args &a) {
				ecs::NkEntityId id;
				NkString pq;
				const uint64 uid = a.U("uid");
				if (!EntiteDe(m, uid, id, pq))
					return Refus(pq);
				const int32 val = a.d.Membre(a.r, "valeurs");
				if (!a.d.EstObjet(val))
					return Refus("« valeurs » doit etre un OBJET JSON (la forme que lire_entite rend)");
				NkJsonDoc d;
				int32 e = -1;
				if (!EntiteJson(m, uid, d, e, pq))
					return Refus(pq);
				NkJsonDoc patch;
				const int32 pr = patch.Copier(a.d, val);
				CouleursEnEntiers(patch, pr);
				// L'IDENTITE ne se change pas : elle tient les parents et les references.
				NkJsonDoc propre;
				const int32 po = propre.NouvelObjet();
				for (uint32 k = 0; k < patch.Taille(pr); ++k) {
					const NkString cle(patch.Cle(pr, k));
					if (cle == "uid")
						continue;
					propre.Poser(po, cle.CStr(), propre.Copier(patch, patch.Element(pr, k)));
				}
				d.Fusionner(e, propre, po);
				const NkString nouveau = d.Ecrire(d.Racine());
				const uint64 selUid = m.aSelection ? m.scene.Uid(m.selection) : 0u;
				NkString err;
				if (!NkChargerSceneJSON(m.scene, nouveau.View(), m.RessourcesScene(), &err))
					return Refus(NkString("la scene refuse ces valeurs (rien n'a change) : ") + err);
				// Ce que NkEditeurOuvrir aligne apres une lecture : la gravite des rigides.
				if (m.scene.MondePhysique() != nullptr && m.scene.Particules() != nullptr) {
					const NkVec2f g = m.scene.Particules()->reglages.gravite;
					m.scene.MondePhysique()->SetGravity(math::NkVec3f(g.x, g.y, 0.f));
				}
				const ecs::NkEntityId s = selUid != 0u ? m.scene.EntiteParUid(selUid) : ecs::NkEntityId::Invalid();
				Selectionner(m, s);
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				const ecs::NkEntityId apres = m.scene.EntiteParUid(uid);
				Selectionner(m, apres);
				r.texte = NkString("{\"ok\":true,\"entite\":") + d.Ecrire(e) + "}";
				r.effet = NkString("valeurs de « ") + NomDe(m, apres) + "» changees : " + propre.Ecrire(po);
				if (r.effet.Length() > 160u)
					r.effet = r.effet.SubStr(0, 157) + "...";
				return r;
			}

			NkResultatOutilIA CreerActeur(NkEditeurModele &m, const Args &a) {
				NkActeurSim acteur;
				const NkString nom = a.T("acteur");
				if (!ActeurParNom(nom.CStr(), acteur)) {
					NkString l;
					for (int32 i = 0; i < static_cast<int32>(NkActeurSim::NK_COUNT); ++i) {
						if (i)
							l.Append(", ");
						l.Append(Normaliser(NkActeurSimInfo(static_cast<NkActeurSim>(i)).nom));
					}
					return Refus(NkString("acteur inconnu « ") + nom + " » ; le catalogue : " + l);
				}
				const NkVec2f centre = m.scene.Camera().Centre();
				const NkVec2f p(a.A("x") ? a.F("x") : centre.x, a.A("y") ? a.F("y") : centre.y);
				const NkActeurSim arme = m.acteur;
				const bool simple = m.acteurSimple;
				m.acteur = acteur;
				m.acteurSimple = false;
				const ecs::NkEntityId e = NkEditeurPoser(m, p);
				m.acteur = arme;
				m.acteurSimple = simple;
				if (!e.IsValid())
					return Refus("l'acteur n'a pas pu etre pose (le monde de particules manque-t-il ?)");
				if (a.A("nom") && !a.T("nom").Empty())
					NkEditeurRenommer(m, e, a.T("nom").CStr());
				Selectionner(m, e);
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				char b[160];
				std::snprintf(b, sizeof(b), "{\"ok\":true,\"uid\":%llu,\"nom\":", static_cast<unsigned long long>(m.scene.AssurerUid(e)));
				r.texte = NkString(b);
				NkJsonChaine(NomDe(m, e), r.texte);
				std::snprintf(b, sizeof(b), ",\"position\":[%.3f,%.3f]}", static_cast<double>(p.x), static_cast<double>(p.y));
				r.texte.Append(b);
				r.effet = NkString("+1 entite « ") + NomDe(m, e) + " » en " + Pos(p);
				return r;
			}

			NkResultatOutilIA CreerEntite(NkEditeurModele &m, const Args &a) {
				const NkVec2f centre = m.scene.Camera().Centre();
				const NkVec2f p(a.A("x") ? a.F("x") : centre.x, a.A("y") ? a.F("y") : centre.y);
				const NkString nom = a.T("nom", "Entite");
				const ecs::NkEntityId e = NkEditeurCreerEntite(m, nom.CStr(), p);
				if (!e.IsValid())
					return Refus("l'entite n'a pas pu etre creee");
				NkString ajoutes, refuses;
				const int32 cs = a.d.Membre(a.r, "composants");
				for (uint32 k = 0; k < a.d.Taille(cs); ++k) {
					const NkString cn = a.d.Texte(a.d.Element(cs, k));
					NkComposantEditeur c;
					if (ComposantParNom(cn.CStr(), c) && NkEditeurPeutAjouter(m, e, c) && NkEditeurAjouterComposant(m, e, c)) {
						ajoutes.Append(ajoutes.Empty() ? "" : ", ");
						ajoutes.Append(cn);
					} else {
						refuses.Append(refuses.Empty() ? "" : ", ");
						refuses.Append(cn);
					}
				}
				Selectionner(m, e);
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				char b[96];
				std::snprintf(b, sizeof(b), "{\"ok\":true,\"uid\":%llu", static_cast<unsigned long long>(m.scene.AssurerUid(e)));
				r.texte = NkString(b);
				r.texte.Append(",\"composants\":");
				NkJsonChaine(ComposantsDe(m, e), r.texte);
				if (!refuses.Empty()) {
					r.texte.Append(",\"refuses\":");
					NkJsonChaine(refuses, r.texte);
				}
				r.texte.Append("}");
				r.effet = NkString("+1 entite « ") + NomDe(m, e) + " » en " + Pos(p) + (ajoutes.Empty() ? NkString() : NkString(" (") + ajoutes + ")");
				return r;
			}

			NkResultatOutilIA ModifierEntite(NkEditeurModele &m, const Args &a) {
				ecs::NkEntityId id;
				NkString pq;
				if (!EntiteDe(m, a.U("uid"), id, pq))
					return Refus(pq);
				NkString fait;
				Selectionner(m, id);
				if (a.A("nom")) {
					NkEditeurRenommer(m, id, a.T("nom").CStr());
					fait.Append("nom ");
				}
				if (a.A("x") || a.A("y")) {
					NkVec2f c(0.f, 0.f);
					NkEditeurCentreSelection(m, c);
					const NkVec2f cible(a.A("x") ? a.F("x") : c.x, a.A("y") ? a.F("y") : c.y);
					NkEditeurDeplacer(m, cible);
					fait.Append("position ");
				}
				if (a.A("rotation")) {
					const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(id);
					const float32 voulu = a.F("rotation") / 57.2957795f;
					NkEditeurTourner(m, voulu - (t ? t->rotation : 0.f));
					fait.Append("rotation ");
				}
				if (a.A("echelle_x") || a.A("echelle_y")) {
					const NkVec2f e0 = NkEditeurEchelle(m, id);
					NkEditeurPoserEchelle(m, id, NkVec2f(a.A("echelle_x") ? a.F("echelle_x") : e0.x, a.A("echelle_y") ? a.F("echelle_y") : e0.y));
					fait.Append("echelle ");
				}
				if (a.A("active")) {
					m.scene.Activer(id, a.B("active"));
					fait.Append("activite ");
				}
				if (a.A("parent")) {
					const uint64 pu = a.U("parent");
					if (pu == 0u)
						NkEditeurDetacher(m, id);
					else {
						ecs::NkEntityId p;
						if (!EntiteDe(m, pu, p, pq))
							return Refus(NkString("parent : ") + pq);
						if (!NkEditeurRattacher(m, id, p))
							return Refus("ce parent ferait une boucle (une entite ne peut pas etre sous sa descendance)");
					}
					fait.Append("parent ");
				}
				if (fait.Empty())
					return Refus("rien a changer : donnez nom, x, y, rotation, echelle_x, echelle_y, active ou parent");
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(id);
				r.texte = NkString("{\"ok\":true,\"change\":");
				NkJsonChaine(fait, r.texte);
				char b[96];
				std::snprintf(b, sizeof(b), ",\"position\":[%.3f,%.3f]}", static_cast<double>(t ? t->position.x : 0.f),
							  static_cast<double>(t ? t->position.y : 0.f));
				r.texte.Append(b);
				r.effet = NkString("« ") + NomDe(m, id) + " » : " + fait;
				return r;
			}

			NkResultatOutilIA AjouterRetirer(NkEditeurModele &m, const Args &a, bool ajouter) {
				ecs::NkEntityId id;
				NkString pq;
				if (!EntiteDe(m, a.U("uid"), id, pq))
					return Refus(pq);
				NkComposantEditeur c;
				const NkString cn = a.T("composant");
				if (!ComposantParNom(cn.CStr(), c)) {
					NkString l;
					for (int32 i = 0; i < static_cast<int32>(NkComposantEditeur::NK_COUNT); ++i) {
						if (i)
							l.Append(", ");
						l.Append(Normaliser(NkComposantEditeurNom(static_cast<NkComposantEditeur>(i))));
					}
					return Refus(NkString("composant inconnu « ") + cn + " » ; les composants : " + l);
				}
				if (ajouter) {
					if (!NkEditeurPeutAjouter(m, id, c))
						return Refus(NkString("« ") + NkComposantEditeurNom(c) +
									 " » ne s'ajoute pas ici (deja present, ou corps rigide ET corps mou, ou pas de monde de particules)");
					NkActeurSim mat = NkActeurSim::NK_BLOB;
					if (a.A("matiere") && !ActeurParNom(a.T("matiere").CStr(), mat))
						return Refus(NkString("matiere inconnue « ") + a.T("matiere") + " »");
					if (!NkEditeurAjouterComposant(m, id, c, mat))
						return Refus("l'ajout a echoue");
				} else {
					if (!NkEditeurAUnComposant(m, id, c))
						return Refus(NkString("l'entite n'a pas de « ") + NkComposantEditeurNom(c) + " »");
					if (!NkEditeurRetirerComposant(m, id, c))
						return Refus("le retrait a echoue");
				}
				Selectionner(m, id);
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				r.texte = NkString("{\"ok\":true,\"composants\":");
				NkJsonChaine(ComposantsDe(m, id), r.texte);
				r.texte.Append("}");
				r.effet = NkString(ajouter ? "+ " : "- ") + NkComposantEditeurNom(c) + " sur « " + NomDe(m, id) + " »";
				return r;
			}

			NkResultatOutilIA SupprimerEntite(NkEditeurModele &m, const Args &a) {
				ecs::NkEntityId id;
				NkString pq;
				if (!EntiteDe(m, a.U("uid"), id, pq))
					return Refus(pq);
				const NkString nom(NomDe(m, id));
				Selectionner(m, id);
				NkEditeurSupprimerSelection(m);
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				r.texte = NkString("{\"ok\":true}");
				r.effet = NkString("-1 entite « ") + nom + " » (Ctrl+Z la rend)";
				return r;
			}

			NkResultatOutilIA DupliquerEntite(NkEditeurModele &m, const Args &a) {
				ecs::NkEntityId id;
				NkString pq;
				if (!EntiteDe(m, a.U("uid"), id, pq))
					return Refus(pq);
				Selectionner(m, id);
				const ecs::NkEntityId e = NkEditeurDupliquer(m);
				if (!e.IsValid())
					return Refus("cette entite ne se duplique pas (la matiere d'un corps mou ne se recopie pas)");
				Selectionner(m, e);
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				char b[64];
				std::snprintf(b, sizeof(b), "{\"ok\":true,\"uid\":%llu}", static_cast<unsigned long long>(m.scene.AssurerUid(e)));
				r.texte = NkString(b);
				r.effet = NkString("+1 copie de « ") + NomDe(m, e) + " »";
				return r;
			}

			NkResultatOutilIA PoserScript(NkEditeurModele &m, const Args &a) {
				ecs::NkEntityId id;
				NkString pq;
				if (!EntiteDe(m, a.U("uid"), id, pq))
					return Refus(pq);
				if (m.scripts == nullptr)
					return Refus("les scripts ne sont pas demarres dans cet editeur");
				const NkString voulu = a.T("script");
				NkVector<NkString> proposes;
				NkEditeurScriptsProposes(*m.scripts, proposes);
				bool connu = false;
				for (usize i = 0; i < proposes.Size(); ++i)
					connu = connu || proposes[i] == voulu;
				if (!connu) {
					NkString l;
					for (usize i = 0; i < proposes.Size(); ++i) {
						if (i)
							l.Append(", ");
						l.Append(proposes[i]);
					}
					return Refus(NkString("script inconnu « ") + voulu + " » ; ceux du projet : " + (l.Empty() ? NkString("(aucun)") : l));
				}
				if (!m.scene.Monde().Has<unkeny::NkScript2D>(id))
					m.scene.Monde().Add<unkeny::NkScript2D>(id, unkeny::NkScript2D());
				unkeny::NkScript2D *sc = m.scene.Monde().Get<unkeny::NkScript2D>(id);
				if (sc == nullptr || unkeny::NkScriptTrouver(*sc, voulu.CStr()) >= 0)
					return Refus("ce script est deja sur l'entite");
				if (unkeny::NkScriptAjouter(*sc, voulu.CStr()) < 0)
					return Refus("plus de place (4 scripts au plus) ou nom trop long");
				const unkeny::NkDefinitionScript *d = m.scripts->registre.Definition(m.scripts->registre.Trouver(voulu.CStr()));
				if (d != nullptr && d->classe != nullptr)
					for (uint32 v = 0; v < d->classe->nbVariables; ++v) {
						const NkUnkVariableV1 &x = d->classe->variables[v];
						unkeny::NkScriptPoserVariable(*sc, static_cast<uint32>(sc->nombre - 1u), x.nom, static_cast<unkeny::NkTypeVarScript>(x.type),
													  NkVec2f(x.x, x.y));
					}
				Selectionner(m, id);
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				r.texte = NkString("{\"ok\":true}");
				r.effet = NkString("script ") + voulu + " pose sur « " + NomDe(m, id) + " »";
				return r;
			}

			NkResultatOutilIA ListerContenu(NkEditeurModele &m, const Args &a) {
				NkString rel = a.T("dossier");
				NkString abs;
				NkString pq;
				if (rel.Empty() || rel == "." || rel == "/")
					abs = NkEditeurDossierProjet(m);
				else {
					NkString r2;
					if (!CheminDuProjet(m, rel, abs, r2, pq))
						return Refus(pq);
				}
				if (!NkDirectory::Exists(abs.CStr()))
					return Refus(NkString("dossier absent : ") + (rel.Empty() ? NkString("(projet)") : rel));
				const NkVector<NkDirectoryEntry> e = NkDirectory::GetEntries(abs.CStr());
				NkString j("{\"dossier\":");
				NkJsonChaine(rel, j);
				j.Append(",\"elements\":[");
				uint32 n = 0;
				for (usize i = 0; i < e.Size() && n < 200u; ++i) {
					if (e[i].Name.StartsWith(".") || e[i].Name == "Intermediaire")
						continue;
					if (n++)
						j.Append(',');
					j.Append("{\"nom\":");
					NkJsonChaine(e[i].Name, j);
					if (e[i].IsDirectory)
						j.Append(",\"dossier\":true}");
					else {
						j.Append(",\"nature\":");
						NkJsonChaine(NkEditeurNatureFichier(e[i].Name.CStr()).libelle, j);
						j.Append('}');
					}
				}
				j.Append("]}");
				NkResultatOutilIA r;
				r.ok = true;
				r.texte = j;
				return r;
			}

			NkResultatOutilIA LireFichier(NkEditeurModele &m, const Args &a) {
				NkString abs, rel, pq;
				if (!CheminDuProjet(m, a.T("chemin"), abs, rel, pq))
					return Refus(pq);
				if (!NkFile::Exists(abs.CStr()))
					return Refus(NkString("fichier absent : ") + rel);
				const nk_int64 taille = NkFile::GetFileSize(abs.CStr());
				if (taille > 262144)
					return Refus("fichier trop grand pour etre lu en entier (plus de 256 Kio)");
				const NkString t = NkFile::ReadAllText(abs.CStr());
				for (NkString::SizeType i = 0; i < t.Length() && i < 4096u; ++i)
					if (t[i] == '\0')
						return Refus("ce fichier n'est pas du texte (un asset binaire)");
				NkResultatOutilIA r;
				r.ok = true;
				r.texte = t;
				return r;
			}

			/// Ecrit `texte` dans `abs` en le RETENANT d'abord (Ctrl+Z le rend).
			bool EcrireRetenu(NkEditeurModele &m, const NkString &abs, const NkString &texte) {
				NkEditeurRetenirFichier(m, abs.CStr());
				NkDirectory::CreateRecursive(DossierDe(abs).CStr());
				return NkFile::WriteAllText(abs.CStr(), texte.CStr());
			}

			NkResultatOutilIA EcrireFichier(NkEditeurModele &m, NkEditeurInterface *ui, const Args &a) {
				NkString abs, rel, pq;
				if (!CheminDuProjet(m, a.T("chemin"), abs, rel, pq))
					return Refus(pq);
				const bool existait = NkFile::Exists(abs.CStr());
				if (!EcrireRetenu(m, abs, a.T("texte")))
					return Refus(NkString("ecriture impossible : ") + rel);
				if (ui)
					ui->contenuPerime = true;
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				r.texte = NkString("{\"ok\":true,\"chemin\":");
				NkJsonChaine(rel, r.texte);
				r.texte.Append("}");
				char b[48];
				std::snprintf(b, sizeof(b), " (%u octets)", static_cast<unsigned>(a.T("texte").Length()));
				r.effet = NkString(existait ? "fichier ecrase : " : "fichier cree : ") + rel + b;
				return r;
			}

			NkResultatOutilIA SupprimerFichier(NkEditeurModele &m, NkEditeurInterface *ui, const Args &a) {
				NkString abs, rel, pq;
				if (!CheminDuProjet(m, a.T("chemin"), abs, rel, pq))
					return Refus(pq);
				if (!NkFile::Exists(abs.CStr()))
					return Refus(NkString("fichier absent : ") + rel);
				NkEditeurRetenirFichier(m, abs.CStr());
				if (!NkFile::Delete(abs.CStr()))
					return Refus(NkString("suppression impossible : ") + rel);
				if (ui)
					ui->contenuPerime = true;
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				r.texte = NkString("{\"ok\":true}");
				r.effet = NkString("fichier supprime : ") + rel + " (Ctrl+Z le rend pendant cette session)";
				return r;
			}

			NkResultatOutilIA CreerDossier(NkEditeurModele &m, NkEditeurInterface *ui, const Args &a) {
				NkString abs, rel, pq;
				if (!CheminDuProjet(m, a.T("chemin"), abs, rel, pq))
					return Refus(pq);
				if (NkDirectory::Exists(abs.CStr()))
					return Refus(NkString("ce dossier existe deja : ") + rel);
				if (!NkDirectory::CreateRecursive(abs.CStr()))
					return Refus(NkString("creation impossible : ") + rel);
				if (ui)
					ui->contenuPerime = true;
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = false; // un dossier vide ne se retient pas (et ne gene personne)
				r.texte = NkString("{\"ok\":true}");
				r.effet = NkString("dossier cree : ") + rel;
				return r;
			}

			NkString IdentifiantCpp(const NkString &nom) {
				NkString c;
				for (NkString::SizeType i = 0; i < nom.Length(); ++i) {
					const char ch = nom[i];
					const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
					c.Append(ok ? ch : '_');
				}
				if (c.Empty() || (c[0] >= '0' && c[0] <= '9'))
					c = NkString("Script_") + c;
				return c;
			}

			NkResultatOutilIA EcrireScriptCpp(NkEditeurModele &m, NkEditeurInterface *ui, const Args &a) {
				NkString nom = a.T("nom");
				if (nom.EndsWith(".cpp"))
					nom = nom.SubStr(0, nom.Length() - 4);
				const NkString classe = IdentifiantCpp(nom);
				const NkString rel = NkString(NK_SCRIPTS_DOSSIER) + "/" + classe + ".cpp";
				NkString abs, r2, pq;
				if (!CheminDuProjet(m, rel, abs, r2, pq))
					return Refus(pq);
				NkString code = a.T("code");
				const bool existait = NkFile::Exists(abs.CStr());
				if (code.Empty()) {
					// LE MODELE COMMENTE de l'editeur (il liste toute l'API des scripts) :
					// ecrit par la MEME fonction que « + Ajouter > Script C++ ».
					const NkString nav = NkEditeurNouveauScriptCpp(m, "Contenu/Scripts");
					if (nav.Empty())
						return Refus("le modele de script ne s'ecrit pas");
					const NkString tmp = NkEditeurCheminContenu(m, nav.CStr());
					NkString t = NkFile::ReadAllText(tmp.CStr());
					NkFile::Delete(tmp.CStr());
					// Le nom de classe du modele (« NouveauScript ») devient le notre.
					const NkString ancien = IdentifiantCpp(editorkit::NkDisqueNom(tmp.CStr()).SubStr(0, editorkit::NkDisqueNom(tmp.CStr()).Length() - 4));
					for (NkString::SizeType p = t.Find(ancien.CStr()); p != NkString::npos; p = t.Find(ancien.CStr(), p + classe.Length())) {
						t.Erase(p, ancien.Length());
						t.Insert(p, classe);
					}
					code = t;
				}
				if (!EcrireRetenu(m, abs, code))
					return Refus(NkString("ecriture impossible : ") + rel);
				if (ui)
					ui->contenuPerime = true;
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				r.texte = NkString("{\"ok\":true,\"chemin\":");
				NkJsonChaine(rel, r.texte);
				r.texte.Append(",\"classe\":");
				NkJsonChaine(NkString("cpp:") + classe, r.texte);
				r.texte.Append(",\"note\":\"l'editeur le recompile et le recharge a chaud ; les erreurs arrivent au Journal\"}");
				r.effet = NkString(existait ? "script ecrase : " : "script C++ ecrit : ") + rel;
				return r;
			}

			NkResultatOutilIA EcrireBlueprint(NkEditeurModele &m, NkEditeurInterface *ui, const Args &a) {
				NkString nom = a.T("nom");
				if (nom.EndsWith(".nkbp"))
					nom = nom.SubStr(0, nom.Length() - 5);
				const NkString rel = NkString(NK_SCRIPTS_DOSSIER) + "/" + IdentifiantCpp(nom) + ".nkbp";
				NkString abs, r2, pq;
				if (!CheminDuProjet(m, rel, abs, r2, pq))
					return Refus(pq);
				graph::NkNodeGraph g;
				g.Clear();
				NkBpEnregistrerTypes(g);
				NkVector<NkString> ids;
				NkVector<graph::NkNodeId> noeuds;
				const int32 ns = a.d.Membre(a.r, "noeuds");
				if (a.d.Taille(ns) == 0u)
					return Refus("aucun noeud : un Blueprint commence par un evenement (bp.ev.debut, bp.ev.tick...)");
				for (uint32 k = 0; k < a.d.Taille(ns); ++k) {
					const int32 n = a.d.Element(ns, k);
					const NkString type = a.d.TexteDe(n, "type");
					const NkString id = a.d.TexteDe(n, "id", "");
					const float32 x = static_cast<float32>(a.d.Nombre(a.d.Membre(n, "x"), 260.0 * k));
					const float32 y = static_cast<float32>(a.d.Nombre(a.d.Membre(n, "y"), 0.0));
					const graph::NkNodeId nid = NkBpCreerNoeud(g, type.CStr(), x, y);
					if (nid == graph::NK_NODE_INVALID)
						return Refus(NkString("type de noeud inconnu « ") + type + " » (le catalogue est dans la description de l'editeur)");
					const int32 vals = a.d.Membre(n, "valeurs");
					for (uint32 v = 0; v < a.d.Taille(vals); ++v) {
						const NkString prise(a.d.Cle(vals, v));
						const int32 val = a.d.Element(vals, v);
						NkString texte = a.d.GenreDe(val) == NkJsonDoc::TABLEAU ? NkString() : a.d.Texte(val);
						if (a.d.GenreDe(val) == NkJsonDoc::BOOLEEN)
							texte = a.d.Booleen(val) ? "vrai" : "faux";
						if (a.d.GenreDe(val) == NkJsonDoc::TABLEAU)
							for (uint32 q = 0; q < a.d.Taille(val); ++q) {
								if (q)
									texte.Append(' ');
								texte.Append(a.d.Texte(a.d.Element(val, q)));
							}
						if (!NkBpPoserDefaut(g, nid, prise.CStr(), texte.CStr()))
							return Refus(NkString("noeud « ") + id + " » : la valeur « " + texte + " » ne convient pas a la prise « " + prise + " »");
					}
					ids.PushBack(id.Empty() ? NkString::Format("n%u", static_cast<unsigned>(k)) : id);
					noeuds.PushBack(nid);
				}
				const int32 ls = a.d.Membre(a.r, "liens");
				auto Noeud = [&](const NkString &id) -> graph::NkNodeId {
					for (usize i = 0; i < ids.Size(); ++i)
						if (ids[i] == id)
							return noeuds[i];
					return graph::NK_NODE_INVALID;
				};
				for (uint32 k = 0; k < a.d.Taille(ls); ++k) {
					const int32 l = a.d.Element(ls, k);
					const NkString de = a.d.TexteDe(l, "de"), vers = a.d.TexteDe(l, "vers");
					const graph::NkNodeId nde = Noeud(de), nvers = Noeud(vers);
					if (nde == graph::NK_NODE_INVALID || nvers == graph::NK_NODE_INVALID)
						return Refus(NkString("lien vers un noeud inconnu : « ") + de + " » -> « " + vers + " »");
					const graph::NkLinkError e =
						g.Connect(nde, a.d.TexteDe(l, "sortie", "suite").CStr(), nvers, a.d.TexteDe(l, "entree", "exec").CStr());
					if (e != graph::NkLinkError::Ok)
						return Refus(NkString::Format("le lien %s.%s -> %s.%s est refuse (prise inconnue, sens ou type)", de.CStr(),
													  a.d.TexteDe(l, "sortie", "suite").CStr(), vers.CStr(), a.d.TexteDe(l, "entree", "exec").CStr()));
				}
				NkEditeurRetenirFichier(m, abs.CStr());
				NkDirectory::CreateRecursive(DossierDe(abs).CStr());
				NkErreurBp err;
				const bool compile = NkBpEnregistrer(abs.CStr(), g, err);
				if (ui)
					ui->contenuPerime = true;
				NkResultatOutilIA r;
				r.modifie = NkFile::Exists(abs.CStr());
				if (!compile) {
					NkString quel;
					for (usize i = 0; i < noeuds.Size(); ++i)
						if (noeuds[i] == err.noeud)
							quel = ids[i];
					r.ok = false;
					r.texte = NkString("REFUS : le graphe est ecrit mais NE COMPILE PAS -- ") + err.message +
							  (quel.Empty() ? NkString() : NkString(" (noeud « ") + quel + " »)");
					r.effet = NkString("Blueprint ecrit, sans module : ") + rel;
					return r;
				}
				r.ok = true;
				r.texte = NkString("{\"ok\":true,\"chemin\":");
				NkJsonChaine(rel, r.texte);
				r.texte.Append(",\"compile\":true}");
				r.effet = NkString("Blueprint compile et ecrit : ") + rel;
				return r;
			}

			NkString CheminGdd(NkEditeurModele &m) {
				return NkEditeurDossierProjet(m) + "Documents/GDD.md";
			}

			NkResultatOutilIA EcrireGdd(NkEditeurModele &m, NkEditeurInterface *ui, const Args &a) {
				const NkString abs = CheminGdd(m);
				const bool remplacer = Normaliser(a.T("mode").CStr()) == "remplacer";
				NkString t = remplacer || !NkFile::Exists(abs.CStr()) ? NkString() : NkFile::ReadAllText(abs.CStr());
				if (t.Empty() && !remplacer)
					t = NkString("# Document de conception du jeu (GDD)\n");
				if (!t.Empty() && !t.EndsWith("\n"))
					t.Append("\n");
				if (!a.T("section").Empty()) {
					t.Append("\n## ");
					t.Append(a.T("section"));
					t.Append("\n\n");
				}
				t.Append(a.T("texte"));
				if (!t.EndsWith("\n"))
					t.Append("\n");
				if (!EcrireRetenu(m, abs, t))
					return Refus("le GDD ne s'ecrit pas (Documents/GDD.md)");
				if (ui)
					ui->contenuPerime = true;
				NkResultatOutilIA r;
				r.ok = true;
				r.modifie = true;
				r.texte = NkString("{\"ok\":true,\"chemin\":\"Documents/GDD.md\"}");
				r.effet = NkString(remplacer ? "GDD remplace" : "GDD complete") + (a.T("section").Empty() ? NkString() : NkString(" : ") + a.T("section"));
				return r;
			}

			NkResultatOutilIA ExecuterCommande(NkEditeurCadre &c, const Args &a) {
				const Commande *k = CommandeDe(a.T("nom").CStr());
				if (!k) {
					NkString l;
					for (const Commande &x : kCommandes) {
						if (!l.Empty())
							l.Append(", ");
						l.Append(x.nom);
					}
					return Refus(NkString("commande inconnue « ") + a.T("nom") + " » ; les commandes : " + l);
				}
				NkResultatOutilIA r;
				r.ok = true;
				if (k->action == -1) {
					r.ok = NkEditeurSauver(c.m);
					if (r.ok)
						NkEditeurRetenirEmpreinte(c.m, c.ui);
				} else if (k->action == -2) {
					NkEditeurNouvelleScene(c.m);
					c.m.chemin = NkString();
					c.ui.cadrageEnAttente = true;
					NkEditeurRetenirEmpreinte(c.m, c.ui);
				} else
					NkEditeurExecuter(c, k->action);
				r.texte = NkString(r.ok ? "{\"ok\":true,\"annonce\":" : "{\"ok\":false,\"annonce\":");
				NkJsonChaine(c.m.message, r.texte);
				r.texte.Append("}");
				r.effet = NkString("commande « ") + k->nom + " » : " + c.m.message;
				return r;
			}

		} // namespace

		// =====================================================================
		// LES MUTATIONS DES CONTRE-EPREUVES (jamais posees hors du banc)
		// =====================================================================
		namespace {
			uint32 gMutation = 0u;
		}
		void NkEditeurIAMutation(uint32 bits) {
			gMutation = bits;
		}
		uint32 NkEditeurIAMutationActive() {
			return gMutation;
		}

		// =====================================================================
		// L'API
		// =====================================================================
		const NkVector<NkOutilEditeurIA> &NkEditeurIAOutils() {
			return Table();
		}

		void NkEditeurIAOutilsConverse(NkVector<converse::NkOutilIA> &sortie) {
			sortie.Clear();
			for (const NkOutilEditeurIA &o : Table()) {
				converse::NkOutilIA x;
				x.nom = NkString(o.nom);
				x.description = NkString(o.description);
				x.schema = NkString(o.schema);
				x.irreversible = o.irreversible;
				sortie.PushBack(x);
			}
		}

		// =====================================================================
		// CE QUI ATTEND LA CONFIRMATION
		// =====================================================================
		bool NkEditeurIAIrreversible(NkEditeurCadre &c, const NkAppelOutil &appel, NkString &pourquoi) {
			if ((gMutation & 2u) != 0u)
				return false; // CONTRE-EPREUVE : le garde-fou est coupe
			const NkOutilEditeurIA *o = Outil(appel.nom.CStr());
			if (!o)
				return false; // inconnu : il sera REFUSE, pas confirme
			Args a;
			if (!a.d.Lire(appel.arguments))
				return false;
			a.r = a.d.Racine();
			const NkString n = Normaliser(o->nom);
			NkString abs, rel, pq;
			if (n == "supprimer-fichier") {
				if (CheminDuProjet(c.m, a.T("chemin"), abs, rel, pq) && NkFile::Exists(abs.CStr())) {
					char b[64];
					std::snprintf(b, sizeof(b), " (%lld octets)", static_cast<long long>(NkFile::GetFileSize(abs.CStr())));
					pourquoi = NkString("Supprimer ") + rel + b + " ? Ctrl+Z le rend pendant cette session, plus apres.";
					return true;
				}
				return false; // un chemin faux sera refuse, sans rien demander
			}
			if (n == "ecrire-fichier" || n == "ecrire-script-cpp" || n == "ecrire-blueprint") {
				NkString r = a.T(n == "ecrire-fichier" ? "chemin" : "nom");
				if (n == "ecrire-script-cpp") {
					if (r.EndsWith(".cpp"))
						r = r.SubStr(0, r.Length() - 4);
					r = NkString(NK_SCRIPTS_DOSSIER) + "/" + IdentifiantCpp(r) + ".cpp";
				} else if (n == "ecrire-blueprint") {
					if (r.EndsWith(".nkbp"))
						r = r.SubStr(0, r.Length() - 5);
					r = NkString(NK_SCRIPTS_DOSSIER) + "/" + IdentifiantCpp(r) + ".nkbp";
				}
				if (CheminDuProjet(c.m, r, abs, rel, pq) && NkFile::Exists(abs.CStr())) {
					char b[64];
					std::snprintf(b, sizeof(b), " (%lld octets)", static_cast<long long>(NkFile::GetFileSize(abs.CStr())));
					pourquoi = NkString("Ecraser ") + rel + b + " ? Ctrl+Z rend l'ancien contenu pendant cette session.";
					return true;
				}
				return false;
			}
			if (n == "ecrire-gdd" && Normaliser(a.T("mode").CStr()) == "remplacer" && NkFile::Exists(CheminGdd(c.m).CStr())) {
				pourquoi = NkString("Remplacer TOUT le GDD (Documents/GDD.md) ?");
				return true;
			}
			if (n == "commande") {
				const Commande *k = CommandeDe(a.T("nom").CStr());
				if (k && k->action == -1 && NkFile::Exists(NkEditeurChemin(c.m))) {
					pourquoi = NkString("Enregistrer par-dessus ") + NkEditeurChemin(c.m) + " ?";
					return true;
				}
				if (k && k->action == -2 && c.ui.modifiee) {
					pourquoi = NkString("Une scene neuve PERD les modifications non enregistrees (et l'historique). Continuer ?");
					return true;
				}
			}
			return o->irreversible;
		}

		// =====================================================================
		// EXECUTER
		// =====================================================================
		NkResultatOutilIA NkEditeurIAExecuter(NkEditeurCadre &c, const NkAppelOutil &appel) {
			NkEditeurModele &m = c.m;
			const NkOutilEditeurIA *o = Outil(appel.nom.CStr());
			if (!o) {
				NkString l;
				for (const NkOutilEditeurIA &x : Table()) {
					if (!l.Empty())
						l.Append(", ");
					l.Append(x.nom);
				}
				return Refus(NkString("outil inconnu « ") + appel.nom + " » ; les outils sont : " + l);
			}
			Args a;
			NkString err;
			if (!a.d.Lire(appel.arguments, &err) || !a.d.EstObjet(a.d.Racine()))
				return Refus(NkString("arguments illisibles (attendu : un OBJET JSON) : ") + (err.Empty() ? appel.arguments : err));
			a.r = a.d.Racine();
			const NkString n = Normaliser(o->nom);
			if (o->modifie && m.etat != NkEtatJeu::NK_EDITION)
				return Refus("en JEU, rien ne se modifie (l'historique est celui de l'edition) : appelez d'abord commande {\"nom\":\"arreter\"}");

			// LA PHOTO AVANT : Ctrl+Z rendra la scene (et les fichiers retenus).
			NkHistoriqueEditeur &h = m.historique;
			const usize avant = h.annuler.Size();
			const NkVector<NkScene::NkPhoto> refaireAvant = o->modifie ? h.refaire : NkVector<NkScene::NkPhoto>();
			const NkVector<NkVector<NkFichierRetenu>> refaireFichiersAvant =
				o->modifie ? h.refaireFichiers : NkVector<NkVector<NkFichierRetenu>>();
			if (o->modifie && (gMutation & 1u) == 0u)
				NkEditeurRetenir(m);

			NkResultatOutilIA r;
			if (n == "lire-scene")
				r = LireScene(m);
			else if (n == "lire-entite") {
				ecs::NkEntityId id;
				NkString pq;
				NkJsonDoc d;
				int32 e = -1;
				if (!EntiteDe(m, a.U("uid"), id, pq) || !EntiteJson(m, a.U("uid"), d, e, pq))
					r = Refus(pq);
				else {
					r.ok = true;
					r.texte = d.Ecrire(e);
				}
			} else if (n == "creer-acteur")
				r = CreerActeur(m, a);
			else if (n == "creer-entite")
				r = CreerEntite(m, a);
			else if (n == "modifier-entite")
				r = ModifierEntite(m, a);
			else if (n == "modifier-valeurs")
				r = ModifierValeurs(m, a);
			else if (n == "ajouter-composant")
				r = AjouterRetirer(m, a, true);
			else if (n == "retirer-composant")
				r = AjouterRetirer(m, a, false);
			else if (n == "supprimer-entite")
				r = SupprimerEntite(m, a);
			else if (n == "dupliquer-entite")
				r = DupliquerEntite(m, a);
			else if (n == "selectionner") {
				const uint64 u = a.U("uid");
				if (u == 0u) {
					Selectionner(m, ecs::NkEntityId::Invalid());
					r.ok = true;
					r.texte = NkString("{\"ok\":true}");
				} else {
					ecs::NkEntityId id;
					NkString pq;
					if (!EntiteDe(m, u, id, pq))
						r = Refus(pq);
					else {
						Selectionner(m, id);
						r.ok = true;
						r.texte = NkString("{\"ok\":true}");
						r.effet = NkString("selection : « ") + NomDe(m, id) + " »";
					}
				}
			} else if (n == "poser-script")
				r = PoserScript(m, a);
			else if (n == "commande")
				r = ExecuterCommande(c, a);
			else if (n == "lister-contenu")
				r = ListerContenu(m, a);
			else if (n == "lire-fichier")
				r = LireFichier(m, a);
			else if (n == "ecrire-fichier")
				r = EcrireFichier(m, &c.ui, a);
			else if (n == "supprimer-fichier")
				r = SupprimerFichier(m, &c.ui, a);
			else if (n == "creer-dossier")
				r = CreerDossier(m, &c.ui, a);
			else if (n == "ecrire-script-cpp")
				r = EcrireScriptCpp(m, &c.ui, a);
			else if (n == "ecrire-blueprint")
				r = EcrireBlueprint(m, &c.ui, a);
			else if (n == "ecrire-gdd")
				r = EcrireGdd(m, &c.ui, a);
			else
				r = Refus(NkString("outil declare sans executant : ") + o->nom);

			// RIEN N'A CHANGE : la photo s'en va, et « refaire » revient tel qu'il
			// etait -- un Ctrl+Z qui ne defait rien serait un mensonge.
			if (o->modifie && !r.modifie && h.annuler.Size() > avant) {
				h.annuler.PopBack();
				if (h.annulerFichiers.Size() > h.annuler.Size())
					h.annulerFichiers.PopBack();
				h.refaire = refaireAvant;
				h.refaireFichiers = refaireFichiersAvant;
			}
			if (r.modifie)
				NkEditeurAnnoncer(m, (NkString("IA : ") + r.effet + " (Ctrl+Z la defait)").CStr());
			return r;
		}

		// =====================================================================
		// LA DESCRIPTION DE L'EDITEUR, ENGENDREE
		// =====================================================================
		NkString NkEditeurIADescription(NkEditeurModele &m, bool outilsTexte) {
			NkString t;
			t.Append("Tu es l'assistant integre d'UnkenyEditor, l'editeur du moteur de jeu 2D Unkeny (Nkentseu). "
					 "Tu VOIS l'editeur et tu AGIS sur lui par des OUTILS : tout ce qui est decrit ici, tu peux le lire et le "
					 "changer -- creer et regler des entites, ecrire des scripts C++ et des Blueprints, organiser le Contenu, "
					 "ecrire le document de conception (GDD), discuter du jeu. Reponds en francais, brievement. Quand une demande "
					 "est claire, AGIS (appelle l'outil) au lieu d'expliquer ce qu'il faudrait faire ; apres l'action, dis en une "
					 "phrase ce qui a change.\n");
			t.Append("\n## Garde-fous\n");
			t.Append("- Chaque action passe par l'historique de l'editeur : l'utilisateur la defait par Ctrl+Z.\n");
			t.Append("- Supprimer ou ecraser un fichier, enregistrer par-dessus la scene, une scene neuve sur des modifications non "
					 "enregistrees : l'editeur demande la CONFIRMATION de l'utilisateur. S'il refuse, n'insiste pas.\n");
			t.Append("- Les chemins sont RELATIFS au projet (\"Contenu/Scripts/Porte.cpp\") ; rien hors du projet.\n");
			t.Append("- En JEU, rien ne se modifie : commande arreter d'abord.\n");
			t.Append("\n## Reperes\n");
			t.Append("- 2D, en metres ; x vers la droite, y vers le HAUT ; rotation en degres. « Au centre » = le centre de la vue.\n");
			t.Append("- Une entite se designe par son uid (lire_scene les donne).\n");

			t.Append("\n## L'editeur maintenant\n");
			const NkString projet = NkEditeurDossierProjet(m);
			t.Append("- Projet : ");
			t.Append(projet);
			t.Append("\n- Scene : ");
			t.Append(m.chemin.Empty() ? NkString("(neuve, non enregistree)") : m.chemin);
			t.Append(" ; etat : ");
			t.Append(m.etat == NkEtatJeu::NK_EDITION ? "edition" : (m.etat == NkEtatJeu::NK_JEU ? "jeu" : "pause"));
			const NkVue2D &cam = m.scene.Camera();
			char b[256];
			std::snprintf(b, sizeof(b), "\n- Vue : centre (%.2f ; %.2f), zoom %.1f px/m\n", static_cast<double>(cam.Centre().x),
						  static_cast<double>(cam.Centre().y), static_cast<double>(cam.Zoom()));
			t.Append(b);
			t.Append("- Selection : ");
			if (m.aSelection && m.scene.Monde().IsAlive(m.selection)) {
				std::snprintf(b, sizeof(b), "« %s » (uid %llu)\n", NomDe(m, m.selection),
							  static_cast<unsigned long long>(m.scene.AssurerUid(m.selection)));
				t.Append(b);
			} else
				t.Append("aucune\n");
			const NkVector<ecs::NkEntityId> es = Entites(m);
			std::snprintf(b, sizeof(b), "- Entites (%u) :\n", static_cast<unsigned>(es.Size()));
			t.Append(b);
			for (usize i = 0; i < es.Size() && i < 60u; ++i) {
				const NkTransform2D *tr = m.scene.Monde().Get<NkTransform2D>(es[i]);
				std::snprintf(b, sizeof(b), "  uid %llu « %s » [%s] en (%.2f ; %.2f)%s : ",
							  static_cast<unsigned long long>(m.scene.AssurerUid(es[i])), NomDe(m, es[i]), NkEditeurTypeDe(m.scene, es[i]),
							  static_cast<double>(tr ? tr->position.x : 0.f), static_cast<double>(tr ? tr->position.y : 0.f),
							  m.scene.EstActiveSoi(es[i]) ? "" : " (inactive)");
				t.Append(b);
				t.Append(ComposantsDe(m, es[i]));
				t.Append("\n");
			}
			if (es.Size() > 60u)
				t.Append("  ... (lire_scene les donne toutes)\n");

			t.Append("\n## Catalogue\n- Acteurs (creer_acteur) : ");
			for (int32 i = 0; i < static_cast<int32>(NkActeurSim::NK_COUNT); ++i) {
				const NkInfoActeurSim &inf = NkActeurSimInfo(static_cast<NkActeurSim>(i));
				if (i)
					t.Append(" ; ");
				t.Append(Normaliser(inf.nom));
				t.Append(" -- ");
				t.Append(inf.description);
			}
			t.Append("\n- Composants (ajouter_composant / retirer_composant) : ");
			for (int32 i = 0; i < static_cast<int32>(NkComposantEditeur::NK_COUNT); ++i) {
				if (i)
					t.Append(", ");
				t.Append(Normaliser(NkComposantEditeurNom(static_cast<NkComposantEditeur>(i))));
			}
			t.Append(" ; la matiere d'un corps-mou est un acteur mou ou fluide (blob-visqueux, gelee, eau...).\n");
			t.Append("- Commandes (commande) : ");
			for (usize i = 0; i < sizeof(kCommandes) / sizeof(kCommandes[0]); ++i) {
				if (i)
					t.Append(" ; ");
				t.Append(kCommandes[i].nom);
				t.Append(" -- ");
				t.Append(kCommandes[i].quoi);
			}
			t.Append("\n- Types d'assets (lister_contenu) : ");
			static const char *const kExt[] = {".nkscene", ".nkprefab", ".nkanim", ".nkanimctl", ".nkbp", ".cpp", ".png", ".wav", ".ogg", ".ttf", ".json", ".md"};
			for (usize i = 0; i < sizeof(kExt) / sizeof(kExt[0]); ++i) {
				if (i)
					t.Append(", ");
				t.Append(kExt[i]);
				t.Append(" (");
				t.Append(NkEditeurNatureFichier((NkString("x") + kExt[i]).CStr()).libelle);
				t.Append(")");
			}
			t.Append(" ; importables : ");
			t.Append(NkEditeurFiltreImport());
			t.Append("\n- Scripts C++ : Contenu/Scripts/<Nom>.cpp, une classe derivee de nkunk::Script declaree par "
					 "NK_UNKENY_CLASSE_VARIABLES ; l'editeur les recompile et les recharge a chaud. ecrire_script_cpp SANS code "
					 "ecrit le modele commente (toute l'API : Debut, Tick, ContactDebut, ZoneEntree, Position, Teleporter, Impulsion, "
					 "JouerClip, Afficher...) : lis-le avec lire_fichier avant d'ecrire du code. poser_script le met sur une entite.\n");
			if (m.scripts != nullptr) {
				NkVector<NkString> proposes;
				NkEditeurScriptsProposes(*m.scripts, proposes);
				t.Append("- Scripts du projet (poser_script) : ");
				if (proposes.Empty())
					t.Append("aucun");
				for (usize i = 0; i < proposes.Size(); ++i) {
					if (i)
						t.Append(", ");
					t.Append(proposes[i]);
				}
				t.Append("\n");
			}
			t.Append("- Blueprints (ecrire_blueprint) : des noeuds relies par des fils ; un fil d'execution va d'une sortie "
					 "« suite » (ou « vrai »/« faux ») vers l'entree « exec ». Les noeuds : ");
			const NkVector<NkProtoBp> &protos = NkBpProtos();
			for (usize i = 0; i < protos.Size() && i < 90u; ++i) {
				const NkProtoBp &p = protos[i];
				if (i)
					t.Append(" ; ");
				t.Append(p.type);
				t.Append(" « ");
				t.Append(p.libelle);
				t.Append(" » (");
				for (uint8 k = 0; k < p.nbEntrees; ++k) {
					if (k)
						t.Append(",");
					t.Append(p.entrees[k].nom);
					t.Append(":");
					t.Append(p.entrees[k].type);
				}
				t.Append(" -> ");
				for (uint8 k = 0; k < p.nbSorties; ++k) {
					if (k)
						t.Append(",");
					t.Append(p.sorties[k].nom);
					t.Append(":");
					t.Append(p.sorties[k].type);
				}
				t.Append(")");
			}
			t.Append("\n- GDD : Documents/GDD.md (lire_fichier pour le lire, ecrire_gdd pour le completer).\n");
			if (outilsTexte) {
				NkVector<converse::NkOutilIA> outils;
				NkEditeurIAOutilsConverse(outils);
				converse::NkIaConsigneOutilsTexte(outils, t);
			}
			return t;
		}

	} // namespace editeur
} // namespace nkentseu
