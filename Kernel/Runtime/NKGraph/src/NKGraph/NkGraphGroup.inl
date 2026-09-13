#pragma once
// -----------------------------------------------------------------------------
// @File    NkGraphGroup.inl
// @Brief   Implantation de REGROUPER / DEGROUPER.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

namespace nkentseu {
	namespace graph {

		namespace detail {

			// Un fil qui traverse la frontiere de la selection.
			struct GroupCroisement {
					NkNodeId from = NK_NODE_INVALID;
					int32 fromSocket = -1;
					NkNodeId to = NK_NODE_INVALID;
					int32 toSocket = -1;
					int32 prise = -1; ///< index dans `entrees` ou `sorties`
			};

			// Le rang d'un noeud, pour l'ordre deterministe des prises.
			inline GroupRang GroupRangDe(const NkNodeGraph &g, NkNodeId n, int32 socket) {
				GroupRang r;
				const NkNode *p = g.Find(n);
				if (p) {
					r.y = p->y;
					r.x = p->x;
				}
				r.socket = socket;
				r.node = n;
				return r;
			}

		} // namespace detail

		// ── POSER UNE INSTANCE, EN REFUSANT LA RECURSION TOUT DE SUITE ───────
		//
		// C'est LA PORTE par laquelle une instance doit entrer. Le champ
		// `NkNode::subgraph` reste public -- on peut donc toujours fabriquer une
		// instance a la main et contourner ce controle. ⚠️ C'EST EXACTEMENT
		// POURQUOI LE CONTROLE A L'APLATISSEMENT RESTE EN PLACE, et il ne doit
		// pas etre retire sous pretexte que celui-ci existe : un graphe peut
		// arriver par un FICHIER sans jamais passer par une insertion. Deux
		// filets a deux endroits, pour deux chemins differents.

		namespace detail {

			// Existe-t-il une chaine d'instances de `depart` jusqu'a `cible` ?
			// C'est la question qui distingue la boucle a deux maillons -- A
			// instancie B, B instancie A -- de la simple auto-instanciation. Un
			// controle qui ne regarderait que le voisin immediat laisserait
			// passer la premiere.
			inline bool GroupAtteint(const NkGraphDocument &doc, uint32 depart, uint32 cible, NkVector<uint32> &vus) {
				if (depart == cible)
					return true;
				for (uint32 i = 0; i < (uint32)vus.Size(); ++i)
					if (vus[i] == depart)
						return false; // deja explore : pas de boucle infinie ICI non plus
				vus.PushBack(depart);
				if (depart >= doc.GraphCount())
					return false;
				const NkNodeGraph &g = doc.GraphAt(depart);
				for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
					const NkNode *n = g.RawNodeAt(i);
					if (!n || !n->alive || !(n->type == NkString(NK_NODE_INSTANCE)))
						continue;
					const int32 suivant = doc.FindGraph(n->subgraph.CStr());
					if (suivant < 0)
						continue; // sous-graphe absent : c'est un AUTRE defaut, pas le notre
					if (GroupAtteint(doc, (uint32)suivant, cible, vus))
						return true;
				}
				return false;
			}

		} // namespace detail

		inline NkGroupError NkPoseInstance(NkGraphDocument &doc, uint32 graphIdx, const char *nomDuSousGraphe,
										   NkNodeId *outInstance = nullptr) {
			if (outInstance)
				*outInstance = NK_NODE_INVALID;
			if (graphIdx >= doc.GraphCount())
				return NkGroupError::UnknownGraph;
			const int32 ci = doc.FindGraph(nomDuSousGraphe);
			if (ci < 0)
				return NkGroupError::UnknownSubgraph;
			const uint32 childIdx = (uint32)ci;

			// LE CONTROLE, ET IL EST POSE AVANT TOUTE MODIFICATION. Un refus qui
			// aurait deja cree le noeud laisserait une instance orpheline
			// derriere lui -- meme piege que « prototype-inconnu », ou le code de
			// retour seul ne suffisait pas.
			{
				NkVector<uint32> vus;
				if (detail::GroupAtteint(doc, childIdx, graphIdx, vus))
					return NkGroupError::Recursive;
			}

			// Les types du sous-graphe doivent exister chez le parent, sinon les
			// prises de l'instance naitraient invalides -- et le controle
			// d'interface de `BuildPlan` les refuserait plus tard, loin d'ici.
			detail::GroupCopieRegistre(doc.GraphAt(childIdx), doc.GraphAt(graphIdx));

			const NkNodeId inst = doc.GraphAt(graphIdx).AddNode(NK_NODE_INSTANCE, nomDuSousGraphe);
			if (NkNode *p = doc.GraphAt(graphIdx).Find(inst))
				p->subgraph = NkString(nomDuSousGraphe);

			// L'interface se DEDUIT de la frontiere du sous-graphe (R9) : le
			// noeud d'entree porte des prises de SORTIE, qui deviennent les
			// ENTREES de l'instance. Recopier le sens tel quel mettrait toutes
			// les prises a l'envers.
			for (uint32 i = 0; i < doc.GraphAt(childIdx).RawNodeCount(); ++i) {
				const NkNode *nd = doc.GraphAt(childIdx).RawNodeAt(i);
				if (!nd || !nd->alive)
					continue;
				const bool estIn = nd->type == NkString(NK_NODE_GROUP_IN);
				const bool estOut = nd->type == NkString(NK_NODE_GROUP_OUT);
				if (!estIn && !estOut)
					continue;
				for (uint32 k = 0; k < (uint32)nd->sockets.Size(); ++k) {
					const NkSocket &sk = nd->sockets[k];
					if (estIn && sk.dir != NkSocketDir::Output)
						continue;
					if (estOut && sk.dir != NkSocketDir::Input)
						continue;
					const NkTypeId t =
						detail::GroupTraduitType(doc.GraphAt(childIdx), doc.GraphAt(graphIdx), sk.type);
					doc.GraphAt(graphIdx).AddSocket(inst, sk.name.CStr(), t,
													estIn ? NkSocketDir::Input : NkSocketDir::Output);
				}
			}

			if (outInstance)
				*outInstance = inst;
			return NkGroupError::Ok;
		}

		// ── REGROUPER ────────────────────────────────────────────────────────
		//
		// L'ordre des etapes n'est pas negociable, et chacune depend de la
		// precedente :
		//   1. valider — AVANT de toucher au document, pour qu'un refus ne
		//      laisse rien derriere lui ;
		//   2. calculer la frontiere sur le graphe INTACT ;
		//   3. creer le sous-graphe et y recopier la selection ;
		//   4. relever les extremites EXTERIEURES pendant qu'elles existent
		//      encore ;
		//   5. seulement alors, supprimer et remplacer par l'instance.
		//
		// ⚠️ `doc.AddGraph()` peut REALLOUER la table des graphes : toute
		// reference obtenue par `GraphAt()` AVANT cet appel devient pendante.
		// C'est pour ca qu'on ne garde aucune reference longue ici et qu'on
		// redemande le graphe apres la creation. Le defaut serait silencieux et
		// dependrait de la capacite du vecteur — donc invisible sur les petits
		// documents, et brutal sur les gros.
		inline NkGroupError NkGrouper(NkGraphDocument &doc, uint32 graphIdx, const NkNodeId *sel, uint32 n,
									  const char *nom, NkNodeId *outInstance = nullptr) {
			if (outInstance)
				*outInstance = NK_NODE_INVALID;
			if (graphIdx >= doc.GraphCount())
				return NkGroupError::UnknownGraph;
			if (!sel || n == 0)
				return NkGroupError::EmptySelection;
			if (!nom || !nom[0] || doc.FindGraph(nom) >= 0)
				return NkGroupError::NameTaken;
			for (uint32 i = 0; i < n; ++i)
				if (!doc.GraphAt(graphIdx).Find(sel[i]))
					return NkGroupError::UnknownNode;

			// ── 2. la frontiere ──────────────────────────────────────────
			NkVector<detail::GroupPrise> entrees, sorties;
			NkVector<detail::GroupCroisement> entrants, sortants;
			{
				const NkNodeGraph &g = doc.GraphAt(graphIdx);
				for (uint32 i = 0; i < g.LinkCount(); ++i) {
					const NkLink *l = g.LinkAt(i);
					if (!l || !l->alive)
						continue;
					const bool dedansFrom = detail::GroupDansSelection(sel, n, l->fromNode);
					const bool dedansTo = detail::GroupDansSelection(sel, n, l->toNode);
					detail::GroupCroisement c;
					c.from = l->fromNode;
					c.fromSocket = l->fromSocket;
					c.to = l->toNode;
					c.toSocket = l->toSocket;
					if (!dedansFrom && dedansTo)
						entrants.PushBack(c);
					else if (dedansFrom && !dedansTo)
						sortants.PushBack(c);
					// dedans->dedans : reste a l'interieur ; dehors->dehors : ne
					// nous concerne pas. Ce sont les deux dernieres lignes du
					// tableau de R9, et elles n'ont rien a faire ici.
				}

				// ENTREES : la cle de deduplication est la PRISE SOURCE
				// EXTERIEURE. Deux fils partant de la meme prise ne font qu'une
				// entree — sans ca, grouper une constante partagee par cinq
				// noeuds creerait cinq entrees identiques.
				for (uint32 i = 0; i < (uint32)entrants.Size(); ++i) {
					detail::GroupCroisement &c = entrants[i];
					// le rang, le nom et le TYPE viennent du cote INTERNE : c'est
					// ce que le groupe consomme.
					const detail::GroupRang r = detail::GroupRangDe(g, c.to, c.toSocket);
					const NkNode *dst = g.Find(c.to);
					int32 idx = detail::GroupTrouvePrise(entrees, c.from, c.fromSocket);
					if (idx < 0) {
						detail::GroupPrise p;
						p.srcNode = c.from;
						p.srcSocket = c.fromSocket;
						p.rang = r;
						if (dst && c.toSocket >= 0 && c.toSocket < (int32)dst->sockets.Size()) {
							p.name = dst->sockets[(uint32)c.toSocket].name;
							p.type = dst->sockets[(uint32)c.toSocket].type;
						}
						entrees.PushBack(p);
						idx = (int32)entrees.Size() - 1;
					} else if (detail::GroupRangAvant(r, entrees[(uint32)idx].rang)) {
						// Le REPRESENTANT est la destination interne la plus
						// haute. Sans cette regle, le nom de la prise
						// dependrait de l'ordre des liens dans la table.
						entrees[(uint32)idx].rang = r;
						if (dst && c.toSocket >= 0 && c.toSocket < (int32)dst->sockets.Size()) {
							entrees[(uint32)idx].name = dst->sockets[(uint32)c.toSocket].name;
							entrees[(uint32)idx].type = dst->sockets[(uint32)c.toSocket].type;
						}
					}
					c.prise = idx;
				}

				// SORTIES : meme raisonnement, la cle est la prise source
				// INTERNE. Une sortie interne alimentant trois noeuds exterieurs
				// ne fait qu'une sortie.
				for (uint32 i = 0; i < (uint32)sortants.Size(); ++i) {
					detail::GroupCroisement &c = sortants[i];
					const detail::GroupRang r = detail::GroupRangDe(g, c.from, c.fromSocket);
					const NkNode *src = g.Find(c.from);
					int32 idx = detail::GroupTrouvePrise(sorties, c.from, c.fromSocket);
					if (idx < 0) {
						detail::GroupPrise p;
						p.srcNode = c.from;
						p.srcSocket = c.fromSocket;
						p.rang = r;
						if (src && c.fromSocket >= 0 && c.fromSocket < (int32)src->sockets.Size()) {
							p.name = src->sockets[(uint32)c.fromSocket].name;
							p.type = src->sockets[(uint32)c.fromSocket].type;
						}
						sorties.PushBack(p);
						idx = (int32)sorties.Size() - 1;
					}
					c.prise = idx;
				}
			}

			// L'ORDRE, puis la desambiguisation des homonymes DANS cet ordre —
			// c'est ce qui la rend stable elle aussi (R9, pieges 3 et 4).
			// ⚠️ Le tri deplace les elements : les index `prise` releves plus haut
			// deviennent faux. On les rattrape par la CLE (prise source), qui,
			// elle, ne bouge pas.
			detail::GroupTriPrises(entrees);
			detail::GroupTriPrises(sorties);
			for (uint32 i = 0; i < (uint32)entrants.Size(); ++i)
				entrants[i].prise = detail::GroupTrouvePrise(entrees, entrants[i].from, entrants[i].fromSocket);
			for (uint32 i = 0; i < (uint32)sortants.Size(); ++i)
				sortants[i].prise = detail::GroupTrouvePrise(sorties, sortants[i].from, sortants[i].fromSocket);
			{
				NkVector<detail::GroupPrise> vus;
				for (uint32 i = 0; i < (uint32)entrees.Size(); ++i) {
					entrees[i].name = detail::GroupNomUnique(vus, entrees[i].name);
					vus.PushBack(entrees[i]);
				}
				NkVector<detail::GroupPrise> vus2;
				for (uint32 i = 0; i < (uint32)sorties.Size(); ++i) {
					sorties[i].name = detail::GroupNomUnique(vus2, sorties[i].name);
					vus2.PushBack(sorties[i]);
				}
			}

			// ── 3. le sous-graphe ────────────────────────────────────────
			// A partir d'ici, plus aucune reference n'est gardee au-dela de
			// l'appel qui l'a produite.
			const uint32 childIdx = doc.AddGraph(nom);
			detail::GroupCopieRegistre(doc.GraphAt(graphIdx), doc.GraphAt(childIdx));

			NkVector<detail::GroupCorresp> corresp;
			float32 minX = 0.f, maxX = 0.f, sumX = 0.f, sumY = 0.f;
			for (uint32 i = 0; i < n; ++i) {
				const NkNode *src = doc.GraphAt(graphIdx).Find(sel[i]);
				if (!src)
					continue;
				if (i == 0) {
					minX = maxX = src->x;
				} else {
					if (src->x < minX)
						minX = src->x;
					if (src->x > maxX)
						maxX = src->x;
				}
				sumX += src->x;
				sumY += src->y;
				detail::GroupCorresp m;
				m.avant = sel[i];
				m.apres = detail::GroupCopieNoeud(doc.GraphAt(graphIdx), doc.GraphAt(childIdx), *src);
				corresp.PushBack(m);
			}

			// les liens PUREMENT INTERNES, recopies tels quels
			{
				const NkNodeGraph &g = doc.GraphAt(graphIdx);
				for (uint32 i = 0; i < g.LinkCount(); ++i) {
					const NkLink *l = g.LinkAt(i);
					if (!l || !l->alive)
						continue;
					if (!detail::GroupDansSelection(sel, n, l->fromNode) ||
						!detail::GroupDansSelection(sel, n, l->toNode))
						continue;
					const char *a = detail::GroupNomPrise(g.Find(l->fromNode), l->fromSocket);
					const char *b = detail::GroupNomPrise(g.Find(l->toNode), l->toSocket);
					if (!a || !b)
						continue;
					doc.GraphAt(childIdx).Connect(detail::GroupSuit(corresp, l->fromNode), a,
												  detail::GroupSuit(corresp, l->toNode), b);
				}
			}

			// les deux noeuds de frontiere. Leurs prises SONT l'interface vue de
			// l'exterieur — c'est la solution de Blender, et elle evite
			// d'inventer un second mecanisme.
			const NkNodeId bordIn = doc.GraphAt(childIdx).AddNode(NK_NODE_GROUP_IN, "Entrees du groupe");
			const NkNodeId bordOut = doc.GraphAt(childIdx).AddNode(NK_NODE_GROUP_OUT, "Sorties du groupe");
			if (NkNode *p = doc.GraphAt(childIdx).Find(bordIn)) {
				p->x = minX - 200.f;
				p->y = sumY / (float32)(n ? n : 1u);
			}
			if (NkNode *p = doc.GraphAt(childIdx).Find(bordOut)) {
				p->x = maxX + 200.f;
				p->y = sumY / (float32)(n ? n : 1u);
			}
			for (uint32 i = 0; i < (uint32)entrees.Size(); ++i) {
				// Le noeud d'ENTREE du groupe porte des prises de SORTIE : il
				// alimente l'interieur. Sur l'instance, ce seront des entrees.
				const NkTypeId t =
					detail::GroupTraduitType(doc.GraphAt(graphIdx), doc.GraphAt(childIdx), entrees[i].type);
				doc.GraphAt(childIdx).AddSocket(bordIn, entrees[i].name.CStr(), t, NkSocketDir::Output);
			}
			for (uint32 i = 0; i < (uint32)sorties.Size(); ++i) {
				const NkTypeId t =
					detail::GroupTraduitType(doc.GraphAt(graphIdx), doc.GraphAt(childIdx), sorties[i].type);
				doc.GraphAt(childIdx).AddSocket(bordOut, sorties[i].name.CStr(), t, NkSocketDir::Input);
			}

			// et le cablage interieur de la frontiere. Un croisement entrant se
			// rebranche sur la prise du groupe ; PLUSIEURS croisements peuvent
			// partager la meme prise, et c'est exactement le but.
			for (uint32 i = 0; i < (uint32)entrants.Size(); ++i) {
				const detail::GroupCroisement &c = entrants[i];
				if (c.prise < 0)
					continue;
				const char *b = detail::GroupNomPrise(doc.GraphAt(graphIdx).Find(c.to), c.toSocket);
				if (!b)
					continue;
				doc.GraphAt(childIdx).Connect(bordIn, entrees[(uint32)c.prise].name.CStr(),
											  detail::GroupSuit(corresp, c.to), b);
			}
			for (uint32 i = 0; i < (uint32)sortants.Size(); ++i) {
				const detail::GroupCroisement &c = sortants[i];
				if (c.prise < 0)
					continue;
				const char *a = detail::GroupNomPrise(doc.GraphAt(graphIdx).Find(c.from), c.fromSocket);
				if (!a)
					continue;
				doc.GraphAt(childIdx).Connect(detail::GroupSuit(corresp, c.from), a, bordOut,
											  sorties[(uint32)c.prise].name.CStr());
			}

			// ── 4. les extremites EXTERIEURES, relevees PAR LEUR NOM pendant
			//       que les noeuds existent encore ────────────────────────
			NkVector<NkString> nomSourceExt; ///< par entree
			NkVector<NkNodeId> noeudSourceExt;
			for (uint32 i = 0; i < (uint32)entrees.Size(); ++i) {
				const char *a = detail::GroupNomPrise(doc.GraphAt(graphIdx).Find(entrees[i].srcNode), entrees[i].srcSocket);
				nomSourceExt.PushBack(NkString(a ? a : ""));
				noeudSourceExt.PushBack(entrees[i].srcNode);
			}
			NkVector<NkString> nomDestExt; ///< par croisement sortant
			NkVector<NkNodeId> noeudDestExt;
			for (uint32 i = 0; i < (uint32)sortants.Size(); ++i) {
				const char *b = detail::GroupNomPrise(doc.GraphAt(graphIdx).Find(sortants[i].to), sortants[i].toSocket);
				nomDestExt.PushBack(NkString(b ? b : ""));
				noeudDestExt.PushBack(sortants[i].to);
			}

			// ── 5. remplacer ─────────────────────────────────────────────
			for (uint32 i = 0; i < n; ++i)
				doc.GraphAt(graphIdx).RemoveNode(sel[i]);

			const NkNodeId inst = doc.GraphAt(graphIdx).AddNode(NK_NODE_INSTANCE, nom);
			if (NkNode *p = doc.GraphAt(graphIdx).Find(inst)) {
				p->subgraph = NkString(nom);
				p->x = sumX / (float32)n;
				p->y = sumY / (float32)n;
			}
			for (uint32 i = 0; i < (uint32)entrees.Size(); ++i)
				doc.GraphAt(graphIdx).AddSocket(inst, entrees[i].name.CStr(), entrees[i].type, NkSocketDir::Input);
			for (uint32 i = 0; i < (uint32)sorties.Size(); ++i)
				doc.GraphAt(graphIdx).AddSocket(inst, sorties[i].name.CStr(), sorties[i].type, NkSocketDir::Output);

			for (uint32 i = 0; i < (uint32)entrees.Size(); ++i) {
				if (nomSourceExt[i].Size() == 0)
					continue;
				doc.GraphAt(graphIdx).Connect(noeudSourceExt[i], nomSourceExt[i].CStr(), inst,
											  entrees[i].name.CStr());
			}
			for (uint32 i = 0; i < (uint32)sortants.Size(); ++i) {
				if (sortants[i].prise < 0 || nomDestExt[i].Size() == 0)
					continue;
				doc.GraphAt(graphIdx).Connect(inst, sorties[(uint32)sortants[i].prise].name.CStr(), noeudDestExt[i],
											  nomDestExt[i].CStr());
			}

			if (outInstance)
				*outInstance = inst;
			return NkGroupError::Ok;
		}

		// ── DEGROUPER ────────────────────────────────────────────────────────
		//
		// L'inverse EXACT, et le critere d'acceptation de R9 en depend :
		// grouper puis degrouper doit rendre le graphe identique, aux
		// identifiants pres.
		//
		// ⚠️ CE QUE `Degrouper` NE FAIT PAS, ecrit ici pour que ce ne soit pas
		// une surprise : il ne SUPPRIME PAS la definition du sous-graphe. Deux
		// raisons, et la seconde est la vraie. D'abord d'autres instances
		// peuvent encore la designer. Ensuite `NkGraphDocument` range ses
		// graphes dans un vecteur indexe : en retirer un DECALERAIT les index de
		// tous les suivants, et chaque `graph` deja range dans un NkEvalStep
		// designerait alors le mauvais graphe. Une definition orpheline est un
		// desagrement ; un index qui glisse est un defaut plausible et faux.
		// La purge des definitions inutilisees est une operation A PART.
		inline NkGroupError NkDegrouper(NkGraphDocument &doc, uint32 graphIdx, NkNodeId instance) {
			if (graphIdx >= doc.GraphCount())
				return NkGroupError::UnknownGraph;
			const NkNode *inst = doc.GraphAt(graphIdx).Find(instance);
			if (!inst)
				return NkGroupError::UnknownNode;
			if (!(inst->type == NkString(NK_NODE_INSTANCE)))
				return NkGroupError::NotAnInstance;
			const int32 ci = doc.FindGraph(inst->subgraph.CStr());
			if (ci < 0)
				return NkGroupError::UnknownSubgraph;
			const uint32 childIdx = (uint32)ci;

			// Les branchements exterieurs de l'instance, releves PAR NOM.
			NkVector<NkString> nomPriseIn, nomPriseOut;
			NkVector<NkNodeId> srcExt;
			NkVector<NkString> srcExtPrise;
			{
				const NkNodeGraph &g = doc.GraphAt(graphIdx);
				const NkNode *p = g.Find(instance);
				for (uint32 i = 0; i < (uint32)p->sockets.Size(); ++i) {
					if (p->sockets[i].dir != NkSocketDir::Input)
						continue;
					nomPriseIn.PushBack(p->sockets[i].name);
					const NkLink *l = g.IncomingOf(instance, (int32)i);
					if (l) {
						srcExt.PushBack(l->fromNode);
						const char *a = detail::GroupNomPrise(g.Find(l->fromNode), l->fromSocket);
						srcExtPrise.PushBack(NkString(a ? a : ""));
					} else {
						srcExt.PushBack(NK_NODE_INVALID);
						srcExtPrise.PushBack(NkString(""));
					}
				}
				for (uint32 i = 0; i < (uint32)p->sockets.Size(); ++i)
					if (p->sockets[i].dir == NkSocketDir::Output)
						nomPriseOut.PushBack(p->sockets[i].name);
			}
			// et les destinataires exterieurs de chaque sortie — il peut y en
			// avoir PLUSIEURS pour une seule prise, c'est tout l'interet de la
			// deduplication faite au regroupement.
			NkVector<NkString> destPrise;  ///< nom de la prise du groupe
			NkVector<NkNodeId> destNoeud;  ///< noeud exterieur
			NkVector<NkString> destSocket; ///< sa prise
			{
				const NkNodeGraph &g = doc.GraphAt(graphIdx);
				for (uint32 i = 0; i < g.LinkCount(); ++i) {
					const NkLink *l = g.LinkAt(i);
					if (!l || !l->alive || l->fromNode != instance)
						continue;
					const char *a = detail::GroupNomPrise(g.Find(instance), l->fromSocket);
					const char *b = detail::GroupNomPrise(g.Find(l->toNode), l->toSocket);
					if (!a || !b)
						continue;
					destPrise.PushBack(NkString(a));
					destNoeud.PushBack(l->toNode);
					destSocket.PushBack(NkString(b));
				}
			}

			// Le registre de l'enfant peut porter des types que le parent
			// n'a pas — un groupe importe, par exemple. On complete, sans
			// jamais retirer.
			detail::GroupCopieRegistre(doc.GraphAt(childIdx), doc.GraphAt(graphIdx));

			// Recopier tout l'interieur SAUF les deux noeuds de frontiere.
			NkVector<detail::GroupCorresp> corresp;
			NkNodeId bordIn = NK_NODE_INVALID, bordOut = NK_NODE_INVALID;
			{
				const uint32 brut = doc.GraphAt(childIdx).RawNodeCount();
				for (uint32 i = 0; i < brut; ++i) {
					const NkNode *nd = doc.GraphAt(childIdx).RawNodeAt(i);
					if (!nd || !nd->alive)
						continue;
					if (nd->type == NkString(NK_NODE_GROUP_IN)) {
						bordIn = nd->id;
						continue;
					}
					if (nd->type == NkString(NK_NODE_GROUP_OUT)) {
						bordOut = nd->id;
						continue;
					}
					detail::GroupCorresp m;
					m.avant = nd->id;
					m.apres = detail::GroupCopieNoeud(doc.GraphAt(childIdx), doc.GraphAt(graphIdx), *nd);
					corresp.PushBack(m);
				}
			}

			// les liens interieurs qui ne touchent pas la frontiere
			{
				const NkNodeGraph &c = doc.GraphAt(childIdx);
				for (uint32 i = 0; i < c.LinkCount(); ++i) {
					const NkLink *l = c.LinkAt(i);
					if (!l || !l->alive)
						continue;
					if (l->fromNode == bordIn || l->toNode == bordOut)
						continue;
					const char *a = detail::GroupNomPrise(c.Find(l->fromNode), l->fromSocket);
					const char *b = detail::GroupNomPrise(c.Find(l->toNode), l->toSocket);
					if (!a || !b)
						continue;
					doc.GraphAt(graphIdx).Connect(detail::GroupSuit(corresp, l->fromNode), a,
												  detail::GroupSuit(corresp, l->toNode), b);
				}
			}

			// la frontiere d'ENTREE : ce que la prise du groupe alimentait a
			// l'interieur est desormais alimente par la source exterieure.
			{
				const NkNodeGraph &c = doc.GraphAt(childIdx);
				for (uint32 i = 0; i < c.LinkCount(); ++i) {
					const NkLink *l = c.LinkAt(i);
					if (!l || !l->alive || l->fromNode != bordIn)
						continue;
					const char *nomPrise = detail::GroupNomPrise(c.Find(bordIn), l->fromSocket);
					const char *b = detail::GroupNomPrise(c.Find(l->toNode), l->toSocket);
					if (!nomPrise || !b)
						continue;
					for (uint32 k = 0; k < (uint32)nomPriseIn.Size(); ++k) {
						if (!(nomPriseIn[k] == NkString(nomPrise)))
							continue;
						if (srcExt[k] == NK_NODE_INVALID || srcExtPrise[k].Size() == 0)
							break;
						doc.GraphAt(graphIdx).Connect(srcExt[k], srcExtPrise[k].CStr(),
													  detail::GroupSuit(corresp, l->toNode), b);
						break;
					}
				}
			}

			// la frontiere de SORTIE : ce qui alimentait la prise du groupe
			// alimente maintenant TOUS les destinataires exterieurs.
			{
				const NkNodeGraph &c = doc.GraphAt(childIdx);
				for (uint32 i = 0; i < c.LinkCount(); ++i) {
					const NkLink *l = c.LinkAt(i);
					if (!l || !l->alive || l->toNode != bordOut)
						continue;
					const char *nomPrise = detail::GroupNomPrise(c.Find(bordOut), l->toSocket);
					const char *a = detail::GroupNomPrise(c.Find(l->fromNode), l->fromSocket);
					if (!nomPrise || !a)
						continue;
					for (uint32 k = 0; k < (uint32)destPrise.Size(); ++k) {
						if (!(destPrise[k] == NkString(nomPrise)))
							continue;
						doc.GraphAt(graphIdx).Connect(detail::GroupSuit(corresp, l->fromNode), a, destNoeud[k],
													  destSocket[k].CStr());
					}
				}
			}

			doc.GraphAt(graphIdx).RemoveNode(instance);
			return NkGroupError::Ok;
		}

	} // namespace graph
} // namespace nkentseu
