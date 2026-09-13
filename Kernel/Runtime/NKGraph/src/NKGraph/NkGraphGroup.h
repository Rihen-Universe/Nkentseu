#pragma once
// -----------------------------------------------------------------------------
// @File    NkGraphGroup.h
// @Brief   REGROUPER / DEGROUPER — l'operation qui fabrique un groupe.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// CE QUE RODOLF A DEMANDE (2026-08-22, R8 et R9 de design.reponses.md)
//   « un groupe est un groupement de noeuds que l'utilisateur peut empaqueter
//   pour reutiliser a volonte comme des fonctions », et « le groupe resultat a
//   des entrees et des sorties » — qui ne se DECLARENT pas : elles se DEDUISENT
//   des fils qui traversent la frontiere de la selection.
//
// POURQUOI C'EST DANS LE COEUR, ET PAS CHEZ LE CONSOMMATEUR
//   Regrouper est une operation d'AUTORAT, pas de semantique metier : elle ne
//   regarde que des noeuds, des prises et des liens. Elle ne demande jamais ce
//   qu'un type SIGNIFIE — seulement s'il est le meme. Le garde-fou n°1 tient
//   donc entierement : pas un `if (type == …)` metier ici.
//
// POURQUOI UN FICHIER A PART, et pas une methode de plus dans NkGraphDocument.h
//   Ce fichier est le SEUL a savoir ce qu'est « grouper ». NkGraphDocument sait
//   instancier et aplatir ; il n'a pas a savoir d'ou viennent les groupes. Et
//   pratiquement : quatre agents travaillent sur ce depot, un fichier neuf se
//   fusionne, une methode inseree au milieu d'un en-tete partage se conflit.
//
// ⚠️ LE PIEGE QUI N'EST PAS DANS L'ENONCE, et qui casserait tout en silence :
//   CHAQUE GRAPHE TIENT SON PROPRE REGISTRE DE TYPES ET SES PROPRES
//   CONVERSIONS. Un sous-graphe cree vide refuserait, a l'interieur, un lien
//   reel -> couleur que le parent acceptait — et le regroupement perdrait un fil
//   sans que rien ne le dise, puisque `Connect` rend une erreur que personne ne
//   lit. Le registre ET les conversions sont donc RECOPIES avant tout le reste.
//
// ZERO-STL : NkVector/NkString.
// -----------------------------------------------------------------------------

#include "NKGraph/NkGraphDocument.h"

namespace nkentseu {
	namespace graph {

		// Raison d'un refus. On REND une raison plutot qu'un booleen, meme regle
		// que NkLinkError : l'interface doit pouvoir DIRE pourquoi elle refuse.
		enum class NkGroupError : uint8 {
			Ok = 0,
			EmptySelection, ///< rien a empaqueter
			UnknownNode,	///< un identifiant de la selection n'est pas dans ce graphe
			UnknownGraph,	///< l'index de graphe n'existe pas dans le document
			NameTaken,		///< un graphe porte deja ce nom
			NotAnInstance,	///< Degrouper appele sur un noeud qui n'en est pas une
			UnknownSubgraph, ///< l'instance nomme un graphe absent
			// ⚠️ REFUS A L'INSERTION, decide par Rodolf le 2026-08-22. Le refus
			// existait deja a l'APLATISSEMENT, et il suffisait a empecher la
			// boucle -- mais il sortait loin de sa cause. La mesure qui a
			// tranche : sans le controle de recursion, la borne de profondeur
			// arrete quand meme, et rend « trop-profond ». Ca s'arrete, donc ca a
			// l'air de marcher, ET CA ACCUSE LE MAUVAIS COUPABLE. Ici,
			// l'utilisateur vient de poser l'instance : il sait de quoi on parle.
			Recursive,
		};

		inline const char *NkGroupErrorName(NkGroupError e) {
			switch (e) {
				case NkGroupError::Ok:
					return "ok";
				case NkGroupError::EmptySelection:
					return "selection-vide";
				case NkGroupError::UnknownNode:
					return "noeud-inconnu";
				case NkGroupError::UnknownGraph:
					return "graphe-inconnu";
				case NkGroupError::NameTaken:
					return "nom-deja-pris";
				case NkGroupError::NotAnInstance:
					return "pas-une-instance";
				case NkGroupError::UnknownSubgraph:
					return "sousgraphe-inconnu";
				case NkGroupError::Recursive:
					return "recursion-refusee";
			}
			return "?";
		}

		namespace detail {

			// ── L'ORDRE DES PRISES, ET POURQUOI IL EST ECRIT ICI ──────────────
			// R9, piege 3 : « grouper deux fois la meme selection donne deux
			// noeuds de formes differentes » si l'ordre emerge du parcours. Il
			// doit donc venir d'une propriete DU GRAPHE, jamais de l'ordre dans
			// lequel la main a clique.
			//
			// L'ordre retenu : position VERTICALE du noeud interne concerne,
			// puis horizontale, puis l'indice de la prise, puis l'identifiant du
			// noeud. Les trois derniers ne sont pas de la coquetterie : deux
			// noeuds peuvent partager une position au pixel pres, et il faut
			// quand meme que deux executions s'accordent.
			struct GroupRang {
					float32 y = 0.f;
					float32 x = 0.f;
					int32 socket = 0;
					NkNodeId node = NK_NODE_INVALID;
			};

			inline bool GroupRangAvant(const GroupRang &a, const GroupRang &b) {
				if (a.y != b.y)
					return a.y < b.y;
				if (a.x != b.x)
					return a.x < b.x;
				if (a.socket != b.socket)
					return a.socket < b.socket;
				return a.node < b.node;
			}

			// Une prise de l'interface en construction.
			struct GroupPrise {
					// Le cote EXTERIEUR : pour une entree, la prise source hors
					// de la selection ; pour une sortie, la prise source dans la
					// selection. C'est LA CLE DE DEDUPLICATION (R9, pieges 1
					// et 2) : deux fils partant de la meme prise ne font qu'une
					// prise de groupe.
					NkNodeId srcNode = NK_NODE_INVALID;
					int32 srcSocket = -1;
					NkString name;	  ///< nom retenu, desambiguise
					NkTypeId type = NK_TYPE_INVALID; ///< dans le registre du PARENT
					GroupRang rang;
			};

			inline bool GroupDansSelection(const NkNodeId *sel, uint32 n, NkNodeId id) {
				for (uint32 i = 0; i < n; ++i)
					if (sel[i] == id)
						return true;
				return false;
			}

			// Cherche une prise deja retenue pour cette source. Rend son index,
			// ou -1.
			inline int32 GroupTrouvePrise(const NkVector<GroupPrise> &v, NkNodeId n, int32 s) {
				for (uint32 i = 0; i < (uint32)v.Size(); ++i)
					if (v[i].srcNode == n && v[i].srcSocket == s)
						return (int32)i;
				return -1;
			}

			// Le nom est celui de la prise INTERNE (R9, piege 4). Deux prises
			// internes homonymes exigent une desambiguisation, et elle doit etre
			// stable — elle l'est, parce qu'elle suit l'ordre deja fige.
			inline NkString GroupNomUnique(const NkVector<GroupPrise> &deja, const NkString &voulu) {
				bool libre = true;
				for (uint32 i = 0; i < (uint32)deja.Size(); ++i)
					if (deja[i].name == voulu) {
						libre = false;
						break;
					}
				if (libre)
					return voulu;
				for (uint32 k = 2; k < 1000u; ++k) {
					NkString essai = voulu;
					essai.Append("_");
					// pas de formateur ici : le coeur ne depend que de
					// NkString/NkVector.
					char buf[8];
					uint32 v = k, len = 0;
					char tmp[8];
					while (v && len < 7) {
						tmp[len++] = (char)('0' + (v % 10u));
						v /= 10u;
					}
					for (uint32 i = 0; i < len; ++i)
						buf[i] = tmp[len - 1 - i];
					buf[len] = 0;
					essai.Append(buf);
					bool pris = false;
					for (uint32 i = 0; i < (uint32)deja.Size(); ++i)
						if (deja[i].name == essai) {
							pris = true;
							break;
						}
					if (!pris)
						return essai;
				}
				return voulu;
			}

			inline void GroupTriPrises(NkVector<GroupPrise> &v) {
				for (uint32 i = 1; i < (uint32)v.Size(); ++i) {
					GroupPrise cle = v[i];
					uint32 j = i;
					while (j > 0 && GroupRangAvant(cle.rang, v[j - 1].rang)) {
						v[j] = v[j - 1];
						--j;
					}
					v[j] = cle;
				}
			}

			// Recopie le registre de types ET les conversions dirigees du parent
			// vers l'enfant. Voir l'avertissement en tete de fichier : sans ca,
			// l'enfant refuse a l'interieur ce que le parent acceptait.
			//
			// ⚠️ Les IDENTIFIANTS ne sont pas garantis egaux — c'est voulu, et le
			// controle d'interface de NkGraphDocument compare deja les NOMS pour
			// cette raison exacte. On rend donc une table de correspondance.
			inline void GroupCopieRegistre(const NkNodeGraph &src, NkNodeGraph &dst) {
				for (NkTypeId t = 1;; ++t) {
					const NkString *n = src.TypeName(t);
					if (!n)
						break;
					if (dst.FindType(n->CStr()) == NK_TYPE_INVALID)
						dst.RegisterType(n->CStr());
				}
				for (uint32 i = 0; i < src.ConversionCount(); ++i) {
					NkTypeId from = NK_TYPE_INVALID, to = NK_TYPE_INVALID;
					if (!src.ConversionAt(i, &from, &to))
						continue;
					const NkString *fn = src.TypeName(from);
					const NkString *tn = src.TypeName(to);
					if (!fn || !tn)
						continue;
					dst.AllowConversion(dst.FindType(fn->CStr()), dst.FindType(tn->CStr()));
				}
			}

			// Traduit un type d'un registre vers l'autre PAR SON NOM.
			inline NkTypeId GroupTraduitType(const NkNodeGraph &src, const NkNodeGraph &dst, NkTypeId t) {
				const NkString *n = src.TypeName(t);
				return n ? dst.FindType(n->CStr()) : NK_TYPE_INVALID;
			}

			inline NkGraphValue GroupTraduitValeur(const NkNodeGraph &src, const NkNodeGraph &dst,
												   const NkGraphValue &v) {
				NkGraphValue o = v;
				if (v.IsSet())
					o.type = GroupTraduitType(src, dst, v.type);
				return o;
			}

			// Recopie un noeud d'un graphe vers un autre : type, libelle,
			// position, prises (avec leurs defauts) et proprietes. On ne recopie
			// PAS l'identifiant — il appartient au graphe qui accueille.
			inline NkNodeId GroupCopieNoeud(const NkNodeGraph &src, NkNodeGraph &dst, const NkNode &n) {
				const NkNodeId id = dst.AddNode(n.type.CStr(), n.label.CStr());
				if (id == NK_NODE_INVALID)
					return id;
				if (NkNode *p = dst.Find(id)) {
					p->x = n.x;
					p->y = n.y;
					p->subgraph = n.subgraph;
				}
				for (uint32 i = 0; i < (uint32)n.sockets.Size(); ++i) {
					const NkSocket &s = n.sockets[i];
					dst.AddSocket(id, s.name.CStr(), GroupTraduitType(src, dst, s.type), s.dir);
					if (s.defaultValue.IsSet())
						dst.SetSocketDefault(id, s.name.CStr(), s.dir, GroupTraduitValeur(src, dst, s.defaultValue));
				}
				for (uint32 i = 0; i < (uint32)n.props.Size(); ++i)
					dst.SetProp(id, n.props[i].name.CStr(), GroupTraduitValeur(src, dst, n.props[i].value));
				return id;
			}

			struct GroupCorresp {
					NkNodeId avant = NK_NODE_INVALID;
					NkNodeId apres = NK_NODE_INVALID;
			};

			inline NkNodeId GroupSuit(const NkVector<GroupCorresp> &m, NkNodeId a) {
				for (uint32 i = 0; i < (uint32)m.Size(); ++i)
					if (m[i].avant == a)
						return m[i].apres;
				return NK_NODE_INVALID;
			}

			inline const char *GroupNomPrise(const NkNode *n, int32 s) {
				if (!n || s < 0 || s >= (int32)n->sockets.Size())
					return nullptr;
				return n->sockets[(uint32)s].name.CStr();
			}

		} // namespace detail

	} // namespace graph
} // namespace nkentseu

#include "NKGraph/NkGraphGroup.inl"
