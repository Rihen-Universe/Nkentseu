#pragma once
// -----------------------------------------------------------------------------
// @File    NkNodeGraph.inl
// @Brief   Implantation du coeur de graphe. Incluse par NkNodeGraph.h — le module
//          reste EN-TETE PUR tant qu'aucun consommateur ne le lie.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

namespace nkentseu {
	namespace graph {

		namespace detail {
			inline bool GraphStrEq(const NkString &a, const char *b) {
				const char *p = a.CStr();
				if (!p || !b)
					return false;
				while (*p && *b) {
					if (*p != *b)
						return false;
					++p;
					++b;
				}
				return *p == *b;
			}
		} // namespace detail

		inline const char *NkLinkErrorName(NkLinkError e) {
			switch (e) {
				case NkLinkError::Ok:
					return "ok";
				case NkLinkError::UnknownNode:
					return "noeud-inconnu";
				case NkLinkError::UnknownSocket:
					return "socket-inconnu";
				case NkLinkError::SameNode:
					return "meme-noeud";
				case NkLinkError::DirectionMismatch:
					return "sens-invalide";
				case NkLinkError::TypeMismatch:
					return "type-incompatible";
				case NkLinkError::WouldCycle:
					return "cycle";
			}
			return "?";
		}

		inline int32 NkNode::FindSocket(const char *name, NkSocketDir dir) const {
			for (uint32 i = 0; i < (uint32)sockets.Size(); ++i)
				if (sockets[i].dir == dir && detail::GraphStrEq(sockets[i].name, name))
					return (int32)i;
			return -1;
		}

		// ── TYPES ───────────────────────────────────────────────────────────────
		inline NkTypeId NkNodeGraph::RegisterType(const char *name) {
			const NkTypeId existing = FindType(name);
			if (existing != NK_TYPE_INVALID)
				return existing; // idempotent : deux consommateurs peuvent declarer le meme
			if (mTypeNames.Empty())
				mTypeNames.PushBack(NkString("")); // l'index 0 reste « invalide »
			mTypeNames.PushBack(NkString(name ? name : ""));
			return (NkTypeId)(mTypeNames.Size() - 1);
		}

		inline NkTypeId NkNodeGraph::FindType(const char *name) const {
			for (uint32 i = 1; i < (uint32)mTypeNames.Size(); ++i)
				if (detail::GraphStrEq(mTypeNames[i], name))
					return (NkTypeId)i;
			return NK_TYPE_INVALID;
		}

		inline const NkString *NkNodeGraph::TypeName(NkTypeId t) const {
			return (t != NK_TYPE_INVALID && t < (NkTypeId)mTypeNames.Size()) ? &mTypeNames[t] : nullptr;
		}

		inline void NkNodeGraph::AllowConversion(NkTypeId from, NkTypeId to) {
			if (from == NK_TYPE_INVALID || to == NK_TYPE_INVALID || from == to)
				return;
			const uint64 k = ((uint64)from << 32) | (uint64)to;
			for (uint32 i = 0; i < (uint32)mConversions.Size(); ++i)
				if (mConversions[i] == k)
					return;
			mConversions.PushBack(k);
		}

		inline bool NkNodeGraph::Accepts(NkTypeId socketType, NkTypeId valueType) const {
			if (socketType == valueType)
				return true;
			// La conversion est DIRIGEE : autoriser flottant -> vecteur n'autorise pas
			// vecteur -> flottant. Une conversion symetrique perdrait de l'information
			// dans un sens sans le dire.
			const uint64 k = ((uint64)valueType << 32) | (uint64)socketType;
			for (uint32 i = 0; i < (uint32)mConversions.Size(); ++i)
				if (mConversions[i] == k)
					return true;
			return false;
		}

		// ── NOEUDS ──────────────────────────────────────────────────────────────
		inline NkNodeId NkNodeGraph::AddNode(const char *type, const char *label) {
			NkNode n;
			n.id = mNextNode++;
			n.type = NkString(type ? type : "");
			n.label = NkString(label ? label : (type ? type : ""));
			n.alive = true;
			mNodes.PushBack(n);
			return n.id;
		}

		inline NkNode *NkNodeGraph::Find(NkNodeId id) {
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i)
				if (mNodes[i].alive && mNodes[i].id == id)
					return &mNodes[i];
			return nullptr;
		}

		inline const NkNode *NkNodeGraph::Find(NkNodeId id) const {
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i)
				if (mNodes[i].alive && mNodes[i].id == id)
					return &mNodes[i];
			return nullptr;
		}

		inline uint32 NkNodeGraph::NodeCount() const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i)
				if (mNodes[i].alive)
					n++;
			return n;
		}

		inline bool NkNodeGraph::AddSocket(NkNodeId id, const char *name, NkTypeId type, NkSocketDir dir) {
			NkNode *n = Find(id);
			if (!n || !name || type == NK_TYPE_INVALID)
				return false;
			if (n->FindSocket(name, dir) >= 0)
				return false; // deux sockets homonymes dans la meme direction : ambigu
			NkSocket s;
			s.name = NkString(name);
			s.type = type;
			s.dir = dir;
			n->sockets.PushBack(s);
			return true;
		}

		inline bool NkNodeGraph::RemoveNode(NkNodeId id) {
			NkNode *n = Find(id);
			if (!n)
				return false;
			n->alive = false;
			// Les liens incidents meurent AVEC le noeud. Un lien pendant ferait
			// paraitre le graphe valide alors qu'il s'evaluerait faux — exactement le
			// genre de defaut qui ne se voit qu'au resultat.
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive && (mLinks[i].fromNode == id || mLinks[i].toNode == id))
					mLinks[i].alive = false;
			return true;
		}

		// ── CONNEXIONS ──────────────────────────────────────────────────────────
		inline uint32 NkNodeGraph::LinkCount() const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive)
					n++;
			return n;
		}

		inline const NkLink *NkNodeGraph::LinkAt(uint32 idx) const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i) {
				if (!mLinks[i].alive)
					continue;
				if (n++ == idx)
					return &mLinks[i];
			}
			return nullptr;
		}

		inline const NkLink *NkNodeGraph::IncomingOf(NkNodeId id, int32 socketIndex) const {
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive && mLinks[i].toNode == id && mLinks[i].toSocket == socketIndex)
					return &mLinks[i];
			return nullptr;
		}

		inline bool NkNodeGraph::WouldCreateCycle(NkNodeId from, NkNodeId to) const {
			// Existe-t-il DEJA un chemin de `to` vers `from` ? Si oui, ajouter
			// from -> to fermerait la boucle. Parcours en profondeur avec garde.
			if (from == to)
				return true;
			NkVector<NkNodeId> stack;
			NkVector<NkNodeId> seen;
			stack.PushBack(to);
			uint32 guard = 0;
			while (!stack.Empty() && guard++ < 100000u) {
				const NkNodeId cur = stack[(uint32)stack.Size() - 1];
				stack.PopBack();
				if (cur == from)
					return true;
				bool already = false;
				for (uint32 i = 0; i < (uint32)seen.Size(); ++i)
					if (seen[i] == cur) {
						already = true;
						break;
					}
				if (already)
					continue;
				seen.PushBack(cur);
				for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
					if (mLinks[i].alive && mLinks[i].fromNode == cur)
						stack.PushBack(mLinks[i].toNode);
			}
			return false;
		}

		inline NkLinkError NkNodeGraph::Connect(NkNodeId from, const char *fromSocket, NkNodeId to,
												const char *toSocket, NkLinkId *outId) {
			if (outId)
				*outId = 0;
			NkNode *a = Find(from);
			NkNode *b = Find(to);
			if (!a || !b)
				return NkLinkError::UnknownNode;
			if (from == to)
				return NkLinkError::SameNode;
			const int32 si = a->FindSocket(fromSocket, NkSocketDir::Output);
			const int32 di = b->FindSocket(toSocket, NkSocketDir::Input);
			// Un socket cherche dans la MAUVAISE direction est introuvable ; on
			// distingue tout de meme les deux cas, parce que « ce socket n'existe
			// pas » et « vous branchez une entree sur une entree » ne se corrigent
			// pas de la meme facon.
			if (si < 0 || di < 0) {
				const bool aHasIn = a->FindSocket(fromSocket, NkSocketDir::Input) >= 0;
				const bool bHasOut = b->FindSocket(toSocket, NkSocketDir::Output) >= 0;
				if (aHasIn || bHasOut)
					return NkLinkError::DirectionMismatch;
				return NkLinkError::UnknownSocket;
			}
			if (!Accepts(b->sockets[(uint32)di].type, a->sockets[(uint32)si].type))
				return NkLinkError::TypeMismatch;
			if (WouldCreateCycle(from, to))
				return NkLinkError::WouldCycle;

			// Une ENTREE n'accepte qu'une source : l'ancienne est remplacee.
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive && mLinks[i].toNode == to && mLinks[i].toSocket == di)
					mLinks[i].alive = false;

			NkLink l;
			l.id = mNextLink++;
			l.fromNode = from;
			l.fromSocket = si;
			l.toNode = to;
			l.toSocket = di;
			l.alive = true;
			mLinks.PushBack(l);
			if (outId)
				*outId = l.id;
			return NkLinkError::Ok;
		}

		inline bool NkNodeGraph::Disconnect(NkLinkId id) {
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive && mLinks[i].id == id) {
					mLinks[i].alive = false;
					return true;
				}
			return false;
		}

		// ── ORDRE D'EVALUATION ──────────────────────────────────────────────────
		inline bool NkNodeGraph::TopoSort(NkVector<NkNodeId> &out) const {
			out.Clear();
			NkVector<NkNodeId> ids;
			NkVector<uint32> indeg;
			NkVector<uint8> done;
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i)
				if (mNodes[i].alive) {
					ids.PushBack(mNodes[i].id);
					indeg.PushBack(0);
					done.PushBack(0);
				}
			auto indexOf = [&](NkNodeId id) -> int32 {
				for (uint32 i = 0; i < (uint32)ids.Size(); ++i)
					if (ids[i] == id)
						return (int32)i;
				return -1;
			};
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i) {
				if (!mLinks[i].alive)
					continue;
				const int32 t = indexOf(mLinks[i].toNode);
				if (t >= 0)
					indeg[(uint32)t]++;
			}
			const uint32 total = (uint32)ids.Size();
			uint32 emitted = 0;
			uint32 guard = 0;
			// Passes successives : on emet ce qui n'a plus de dependance. Si une passe
			// n'emet RIEN alors qu'il reste des noeuds, c'est un cycle — on le DIT au
			// lieu de rendre un ordre arbitraire.
			while (emitted < total && guard++ <= total + 1) {
				const uint32 before = emitted;
				for (uint32 i = 0; i < total; ++i) {
					if (done[i] || indeg[i] != 0)
						continue;
					out.PushBack(ids[i]);
					done[i] = 1;
					emitted++;
					for (uint32 k = 0; k < (uint32)mLinks.Size(); ++k) {
						if (!mLinks[k].alive || mLinks[k].fromNode != ids[i])
							continue;
						const int32 t = indexOf(mLinks[k].toNode);
						if (t >= 0 && indeg[(uint32)t] > 0)
							indeg[(uint32)t]--;
					}
				}
				if (emitted == before)
					break;
			}
			if (emitted != total) {
				out.Clear();
				return false;
			}
			return true;
		}

		inline bool NkNodeGraph::HasCycle() const {
			NkVector<NkNodeId> tmp;
			return !TopoSort(tmp);
		}

		inline void NkNodeGraph::Clear() {
			mNodes.Clear();
			mLinks.Clear();
			mConversions.Clear();
			mTypeNames.Clear();
			mNextNode = 1;
			mNextLink = 1;
		}

	} // namespace graph
} // namespace nkentseu
