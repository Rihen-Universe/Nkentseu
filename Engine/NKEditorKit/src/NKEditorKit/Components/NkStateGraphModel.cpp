// -----------------------------------------------------------------------------
// @File    NkStateGraphModel.cpp
// @Brief   Le MODELE du graphe d'etats : etats, transitions, conditions,
//          parametres, annuler. Aucun dessin ici (NkStateGraphDraw.cpp).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkStateGraphModel.h"

#include <cstdio>

namespace nkentseu {
	namespace editorkit {

		const char *NkGraphParamKindName(uint8 kind) {
			static const char *const kNoms[(uint8)NkGraphParamKind::Count] = {"Booleen", "Reel", "Declencheur"};
			return kind < (uint8)NkGraphParamKind::Count ? kNoms[kind] : "?";
		}

		const char *NkGraphCondOpName(uint8 op) {
			static const char *const kNoms[(uint8)NkGraphCondOp::Count] = {"vrai", ">", "<", "faux", "tire", "depuis >="};
			return op < (uint8)NkGraphCondOp::Count ? kNoms[op] : "?";
		}

		// ── Les noeuds ──────────────────────────────────────────────────────────
		NkGraphNode *NkStateGraphModel::Node(nk_uint64 id) {
			const int32 i = NodeIndex(id);
			return i >= 0 ? &nodes[(uint32)i] : nullptr;
		}

		const NkGraphNode *NkStateGraphModel::Node(nk_uint64 id) const {
			const int32 i = NodeIndex(id);
			return i >= 0 ? &nodes[(uint32)i] : nullptr;
		}

		int32 NkStateGraphModel::NodeIndex(nk_uint64 id) const {
			for (uint32 i = 0; id != 0 && i < (uint32)nodes.Size(); ++i) {
				if (nodes[i].id == id) {
					return (int32)i;
				}
			}
			return -1;
		}

		const NkGraphNode *NkStateGraphModel::FindByName(const NkString &name, nk_uint64 parent) const {
			for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
				if (nodes[i].parent == parent && nodes[i].name == name) {
					return &nodes[i];
				}
			}
			return nullptr;
		}

		NkString NkStateGraphModel::UniqueName(const NkString &base, nk_uint64 parent) const {
			const NkString b = base.Empty() ? NkString("Etat") : base;
			if (FindByName(b, parent) == nullptr) {
				return b;
			}
			for (int32 k = 2; k < 10000; ++k) {
				char suffixe[16];
				std::snprintf(suffixe, sizeof(suffixe), " %d", k);
				NkString n = b;
				n.Append(suffixe);
				if (FindByName(n, parent) == nullptr) {
					return n;
				}
			}
			return b;
		}

		nk_uint64 NkStateGraphModel::AddState(const NkString &name, nk_uint64 parent, float32 x, float32 y,
											  const NkString &clip) {
			if (parent != 0) {
				const NkGraphNode *p = Node(parent);
				if (p == nullptr || !p->subMachine) {
					return 0;
				}
			}
			PushUndo();
			NkGraphNode n;
			n.id = nextId++;
			n.name = UniqueName(name, parent);
			n.parent = parent;
			n.clip = clip;
			n.x = x;
			n.y = y;
			nodes.PushBack(n);
			Touch();
			return n.id;
		}

		nk_uint64 NkStateGraphModel::AddSubMachine(const NkString &name, nk_uint64 parent, float32 x, float32 y) {
			const nk_uint64 id = AddState(name.Empty() ? NkString("Sous-machine") : name, parent, x, y);
			if (NkGraphNode *n = Node(id)) {
				n->subMachine = true;
			}
			return id;
		}

		bool NkStateGraphModel::IsUnder(nk_uint64 id, nk_uint64 machine) const {
			if (machine == 0) {
				return Node(id) != nullptr;
			}
			const NkGraphNode *n = Node(id);
			for (int32 garde = 0; n != nullptr && garde < 64; ++garde) {
				if (n->parent == machine) {
					return true;
				}
				n = Node(n->parent);
			}
			return false;
		}

		nk_uint64 NkStateGraphModel::RepresentativeAt(nk_uint64 id, nk_uint64 lvl) const {
			const NkGraphNode *n = Node(id);
			for (int32 garde = 0; n != nullptr && garde < 64; ++garde) {
				if (n->parent == lvl) {
					return n->id;
				}
				n = Node(n->parent);
			}
			return 0;
		}

		bool NkStateGraphModel::RemoveNode(nk_uint64 id) {
			if (Node(id) == nullptr) {
				return false;
			}
			PushUndo();
			// Le noeud et TOUT ce qu'il contient.
			NkVector<nk_uint64> partants;
			partants.PushBack(id);
			for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
				if (nodes[i].id != id && IsUnder(nodes[i].id, id)) {
					partants.PushBack(nodes[i].id);
				}
			}
			auto part = [&](nk_uint64 x) {
				for (uint32 k = 0; k < (uint32)partants.Size(); ++k) {
					if (partants[k] == x) {
						return true;
					}
				}
				return false;
			};
			for (int32 t = (int32)transitions.Size() - 1; t >= 0; --t) {
				const NkGraphTransition &tr = transitions[(uint32)t];
				if ((!tr.any && part(tr.from)) || part(tr.to) || (tr.any && tr.scope != 0 && part(tr.scope))) {
					transitions.Erase(transitions.Begin() + t);
				}
			}
			for (int32 i = (int32)nodes.Size() - 1; i >= 0; --i) {
				if (part(nodes[(uint32)i].id)) {
					nodes.Erase(nodes.Begin() + i);
				}
			}
			for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
				if (part(nodes[i].entry)) {
					nodes[i].entry = 0;
				}
			}
			if (part(rootEntry)) {
				rootEntry = 0;
			}
			if (part(selectedNode)) {
				selectedNode = 0;
			}
			if (part(level)) {
				level = 0;
			}
			Touch();
			return true;
		}

		bool NkStateGraphModel::RenameNode(nk_uint64 id, const NkString &name) {
			NkGraphNode *n = Node(id);
			if (n == nullptr || name.Empty()) {
				return false;
			}
			const NkGraphNode *autre = FindByName(name, n->parent);
			if (autre != nullptr && autre->id != id) {
				return false;
			}
			PushUndo();
			Node(id)->name = name;
			Touch();
			return true;
		}

		nk_uint64 NkStateGraphModel::EntryOf(nk_uint64 machine) const {
			nk_uint64 explicite = 0;
			if (machine == 0) {
				explicite = rootEntry;
			} else if (const NkGraphNode *m = Node(machine)) {
				explicite = m->entry;
			}
			if (explicite != 0) {
				const NkGraphNode *e = Node(explicite);
				if (e != nullptr && e->parent == machine) {
					return explicite;
				}
			}
			for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
				if (nodes[i].parent == machine) {
					return nodes[i].id;
				}
			}
			return 0;
		}

		void NkStateGraphModel::SetEntry(nk_uint64 machine, nk_uint64 state) {
			const NkGraphNode *s = Node(state);
			if (s == nullptr || s->parent != machine) {
				return;
			}
			PushUndo();
			if (machine == 0) {
				rootEntry = state;
			} else if (NkGraphNode *m = Node(machine)) {
				m->entry = state;
			}
			Touch();
		}

		NkString NkStateGraphModel::PathOf(nk_uint64 id) const {
			const NkGraphNode *chaine[64];
			int32 n = 0;
			for (const NkGraphNode *x = Node(id); x != nullptr && n < 64; x = Node(x->parent)) {
				chaine[n++] = x;
			}
			NkString r;
			for (int32 k = n - 1; k >= 0; --k) {
				r.Append(chaine[k]->name);
				if (k > 0) {
					r.Append("/");
				}
			}
			return r;
		}

		// ── Les transitions ─────────────────────────────────────────────────────
		NkGraphTransition *NkStateGraphModel::Transition(nk_uint64 id) {
			for (uint32 i = 0; id != 0 && i < (uint32)transitions.Size(); ++i) {
				if (transitions[i].id == id) {
					return &transitions[i];
				}
			}
			return nullptr;
		}

		const NkGraphTransition *NkStateGraphModel::Transition(nk_uint64 id) const {
			for (uint32 i = 0; id != 0 && i < (uint32)transitions.Size(); ++i) {
				if (transitions[i].id == id) {
					return &transitions[i];
				}
			}
			return nullptr;
		}

		nk_uint64 NkStateGraphModel::AddTransition(nk_uint64 from, nk_uint64 to) {
			if (from == to || Node(from) == nullptr || Node(to) == nullptr) {
				return 0;
			}
			PushUndo();
			NkGraphTransition t;
			t.id = nextId++;
			t.from = from;
			t.to = to;
			transitions.PushBack(t);
			Touch();
			return t.id;
		}

		nk_uint64 NkStateGraphModel::AddAnyTransition(nk_uint64 scope, nk_uint64 to) {
			if (Node(to) == nullptr || (scope != 0 && (Node(scope) == nullptr || !Node(scope)->subMachine))) {
				return 0;
			}
			PushUndo();
			NkGraphTransition t;
			t.id = nextId++;
			t.any = true;
			t.scope = scope;
			t.to = to;
			transitions.PushBack(t);
			Touch();
			return t.id;
		}

		bool NkStateGraphModel::RemoveTransition(nk_uint64 id) {
			for (uint32 i = 0; i < (uint32)transitions.Size(); ++i) {
				if (transitions[i].id == id) {
					PushUndo();
					transitions.Erase(transitions.Begin() + i);
					if (selectedTransition == id) {
						selectedTransition = 0;
					}
					Touch();
					return true;
				}
			}
			return false;
		}

		bool NkStateGraphModel::AddCondition(nk_uint64 transition, const NkString &param, uint8 op, float32 threshold) {
			if (Transition(transition) == nullptr) {
				return false;
			}
			NkGraphCondition c;
			c.param = param;
			if (c.param.Empty() && !params.Empty()) {
				c.param = params[0].name;
			}
			const int32 pi = ParamIndex(c.param);
			const uint8 kind = pi >= 0 ? params[(uint32)pi].kind : (uint8)NkGraphParamKind::Float;
			c.op = op < (uint8)NkGraphCondOp::Count ? op : DefaultOp(kind);
			c.threshold = threshold;
			PushUndo();
			Transition(transition)->conditions.PushBack(c);
			Touch();
			return true;
		}

		bool NkStateGraphModel::RemoveCondition(nk_uint64 transition, uint32 index) {
			NkGraphTransition *t = Transition(transition);
			if (t == nullptr || index >= (uint32)t->conditions.Size()) {
				return false;
			}
			PushUndo();
			t = Transition(transition);
			t->conditions.Erase(t->conditions.Begin() + index);
			Touch();
			return true;
		}

		NkString NkStateGraphModel::Summary(const NkGraphTransition &t) const {
			NkString r;
			for (uint32 k = 0; k < (uint32)t.conditions.Size(); ++k) {
				const NkGraphCondition &c = t.conditions[k];
				if (k > 0) {
					r.Append(" ET ");
				}
				char txt[32];
				if (c.op == (uint8)NkGraphCondOp::TimeInState) {
					// Ne depend d'aucun parametre : « apres 0.50 s ».
					std::snprintf(txt, sizeof(txt), "apres %.2f s", (double)c.threshold);
					r.Append(txt);
					continue;
				}
				r.Append(c.param.Empty() ? NkString("?") : c.param);
				switch ((NkGraphCondOp)c.op) {
					case NkGraphCondOp::Greater:
					case NkGraphCondOp::Less:
						std::snprintf(txt, sizeof(txt), " %s %.2f", NkGraphCondOpName(c.op), (double)c.threshold);
						r.Append(txt);
						break;
					case NkGraphCondOp::IsFalse:
						r.Append(" faux");
						break;
					default:
						break;
				}
			}
			return r;
		}

		// ── Les parametres ──────────────────────────────────────────────────────
		int32 NkStateGraphModel::ParamIndex(const NkString &name) const {
			for (uint32 i = 0; i < (uint32)params.Size(); ++i) {
				if (params[i].name == name) {
					return (int32)i;
				}
			}
			return -1;
		}

		int32 NkStateGraphModel::AddParam(const NkString &name, uint8 kind, float32 defaultValue) {
			NkString n = name.Empty() ? NkString(NkGraphParamKindName(kind)) : name;
			if (ParamIndex(n) >= 0) {
				for (int32 k = 2; k < 10000; ++k) {
					char suffixe[16];
					std::snprintf(suffixe, sizeof(suffixe), " %d", k);
					NkString e = n;
					e.Append(suffixe);
					if (ParamIndex(e) < 0) {
						n = e;
						break;
					}
				}
			}
			PushUndo();
			NkGraphParam p;
			p.name = n;
			p.kind = kind < (uint8)NkGraphParamKind::Count ? kind : (uint8)NkGraphParamKind::Float;
			p.defaultValue = defaultValue;
			p.value = defaultValue;
			params.PushBack(p);
			Touch();
			return (int32)params.Size() - 1;
		}

		bool NkStateGraphModel::RemoveParam(uint32 index) {
			if (index >= (uint32)params.Size()) {
				return false;
			}
			PushUndo();
			const NkString nom = params[index].name;
			params.Erase(params.Begin() + index);
			for (uint32 t = 0; t < (uint32)transitions.Size(); ++t) {
				NkVector<NkGraphCondition> &cs = transitions[t].conditions;
				for (int32 k = (int32)cs.Size() - 1; k >= 0; --k) {
					if (cs[(uint32)k].param == nom) {
						cs.Erase(cs.Begin() + k);
					}
				}
			}
			Touch();
			return true;
		}

		bool NkStateGraphModel::RenameParam(uint32 index, const NkString &name) {
			if (index >= (uint32)params.Size() || name.Empty()) {
				return false;
			}
			const int32 autre = ParamIndex(name);
			if (autre >= 0 && autre != (int32)index) {
				return false;
			}
			PushUndo();
			const NkString ancien = params[index].name;
			params[index].name = name;
			for (uint32 t = 0; t < (uint32)transitions.Size(); ++t) {
				for (uint32 k = 0; k < (uint32)transitions[t].conditions.Size(); ++k) {
					if (transitions[t].conditions[k].param == ancien) {
						transitions[t].conditions[k].param = name;
					}
				}
			}
			Touch();
			return true;
		}

		uint8 NkStateGraphModel::DefaultOp(uint8 kind) {
			switch ((NkGraphParamKind)kind) {
				case NkGraphParamKind::Bool:
					return (uint8)NkGraphCondOp::IsTrue;
				case NkGraphParamKind::Trigger:
					return (uint8)NkGraphCondOp::Trigger;
				default:
					return (uint8)NkGraphCondOp::Greater;
			}
		}

		uint8 NkStateGraphModel::NextOp(uint8 kind, uint8 op) {
			// Les operateurs VALABLES par genre, dans l'ordre du clic. « depuis >= »
			// ne depend d'aucun parametre : il est propose a tous.
			static const uint8 kBool[] = {(uint8)NkGraphCondOp::IsTrue, (uint8)NkGraphCondOp::IsFalse,
										  (uint8)NkGraphCondOp::TimeInState};
			static const uint8 kReel[] = {(uint8)NkGraphCondOp::Greater, (uint8)NkGraphCondOp::Less,
										  (uint8)NkGraphCondOp::TimeInState};
			static const uint8 kDecl[] = {(uint8)NkGraphCondOp::Trigger, (uint8)NkGraphCondOp::TimeInState};
			const uint8 *liste = kReel;
			uint32 n = 3;
			if (kind == (uint8)NkGraphParamKind::Bool) {
				liste = kBool;
			} else if (kind == (uint8)NkGraphParamKind::Trigger) {
				liste = kDecl;
				n = 2;
			}
			for (uint32 i = 0; i < n; ++i) {
				if (liste[i] == op) {
					return liste[(i + 1) % n];
				}
			}
			return liste[0];
		}

		// ── Les pseudo-noeuds ───────────────────────────────────────────────────
		NkGraphPseudo &NkStateGraphModel::PseudoOf(nk_uint64 machine) {
			for (uint32 i = 0; i < (uint32)pseudos.Size(); ++i) {
				if (pseudos[i].machine == machine) {
					return pseudos[i];
				}
			}
			PlacePseudos(machine);
			return pseudos[pseudos.Size() - 1];
		}

		void NkStateGraphModel::PlacePseudos(nk_uint64 machine) {
			for (uint32 i = 0; i < (uint32)pseudos.Size(); ++i) {
				if (pseudos[i].machine == machine) {
					return;
				}
			}
			float32 x0 = 0.f, y0 = 0.f;
			bool premier = true;
			for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
				if (nodes[i].parent != machine) {
					continue;
				}
				if (premier || nodes[i].x < x0) {
					x0 = nodes[i].x;
				}
				if (premier || nodes[i].y < y0) {
					y0 = nodes[i].y;
				}
				premier = false;
			}
			NkGraphPseudo p;
			p.machine = machine;
			p.entryX = x0 - 220.f;
			p.entryY = y0;
			p.anyX = x0 - 220.f;
			p.anyY = y0 + 110.f;
			pseudos.PushBack(p);
		}

		// ── Annuler / Refaire ───────────────────────────────────────────────────
		void NkStateGraphModel::PushUndo() {
			NkStateGraphSnapshot s;
			s.nodes = nodes;
			s.transitions = transitions;
			s.params = params;
			s.pseudos = pseudos;
			s.rootEntry = rootEntry;
			undoStack.PushBack(s);
			while ((uint32)undoStack.Size() > undoLimit && !undoStack.Empty()) {
				undoStack.Erase(undoStack.Begin());
			}
			redoStack.Clear();
		}

		namespace {
			void Echanger(NkStateGraphModel &m, NkVector<NkStateGraphSnapshot> &depuis, NkVector<NkStateGraphSnapshot> &vers) {
				NkStateGraphSnapshot cur;
				cur.nodes = m.nodes;
				cur.transitions = m.transitions;
				cur.params = m.params;
				cur.pseudos = m.pseudos;
				cur.rootEntry = m.rootEntry;
				vers.PushBack(cur);
				const NkStateGraphSnapshot &s = depuis[depuis.Size() - 1];
				m.nodes = s.nodes;
				m.transitions = s.transitions;
				m.params = s.params;
				m.pseudos = s.pseudos;
				m.rootEntry = s.rootEntry;
				depuis.PopBack();
				if (m.Node(m.selectedNode) == nullptr && m.selectedNode != kNkGraphEntryId && m.selectedNode != kNkGraphAnyId) {
					m.selectedNode = 0;
				}
				if (m.Transition(m.selectedTransition) == nullptr) {
					m.selectedTransition = 0;
				}
				if (m.level != 0 && m.Node(m.level) == nullptr) {
					m.level = 0;
				}
				m.Touch();
			}
		} // namespace

		bool NkStateGraphModel::Undo() {
			if (undoStack.Empty()) {
				return false;
			}
			Echanger(*this, undoStack, redoStack);
			return true;
		}

		bool NkStateGraphModel::Redo() {
			if (redoStack.Empty()) {
				return false;
			}
			Echanger(*this, redoStack, undoStack);
			return true;
		}

		void NkStateGraphModel::FrameLevel(float32 canvasW, float32 canvasH) {
			float32 x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
			const NkGraphPseudo &p = PseudoOf(level);
			const float32 px[2] = {p.entryX, p.anyX}, py[2] = {p.entryY, p.anyY};
			for (int32 k = 0; k < 2; ++k) {
				x0 = px[k] < x0 ? px[k] : x0;
				y0 = py[k] < y0 ? py[k] : y0;
				x1 = px[k] > x1 ? px[k] : x1;
				y1 = py[k] > y1 ? py[k] : y1;
			}
			for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
				if (nodes[i].parent != level) {
					continue;
				}
				x0 = nodes[i].x < x0 ? nodes[i].x : x0;
				y0 = nodes[i].y < y0 ? nodes[i].y : y0;
				x1 = nodes[i].x > x1 ? nodes[i].x : x1;
				y1 = nodes[i].y > y1 ? nodes[i].y : y1;
			}
			panX = (x0 + x1) * 0.5f;
			panY = (y0 + y1) * 0.5f;
			// Une marge d'un etat de chaque cote (les positions sont des CENTRES).
			const float32 w = x1 - x0 + 360.f, h = y1 - y0 + 160.f;
			float32 z = 1.f;
			if (canvasW > 1.f && canvasH > 1.f) {
				const float32 zx = canvasW / w, zy = canvasH / h;
				z = zx < zy ? zx : zy;
			}
			zoom = z < 0.35f ? 0.35f : (z > 1.25f ? 1.25f : z);
		}

	} // namespace editorkit
} // namespace nkentseu
