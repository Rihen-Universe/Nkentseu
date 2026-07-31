#pragma once
// -----------------------------------------------------------------------------
// @File    NkGraphDocument.inl
// @Brief   Aplatissement des sous-graphes et serialisation du document.
// @Author  Rihen
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------

namespace nkentseu {
	namespace graph {

		inline const char *NkPlanErrorName(NkPlanError e) {
			switch (e) {
				case NkPlanError::Ok:
					return "ok";
				case NkPlanError::EmptyDocument:
					return "document-vide";
				case NkPlanError::UnknownSubgraph:
					return "sousgraphe-inconnu";
				case NkPlanError::RecursiveSubgraph:
					return "sousgraphe-recursif";
				case NkPlanError::Cycle:
					return "cycle";
				case NkPlanError::TooDeep:
					return "trop-profond";
				case NkPlanError::InterfaceMismatch:
					return "interface-incompatible";
			}
			return "?";
		}

		namespace detail {

			inline bool GraphStrEqS(const NkString &a, const NkString &b) {
				if (a.Size() != b.Size())
					return false;
				const char *p = a.CStr();
				const char *q = b.CStr();
				if (!p || !q)
					return p == q;
				for (uint32 i = 0; i < (uint32)a.Size(); ++i)
					if (p[i] != q[i])
						return false;
				return true;
			}

			// Source resolue d'un socket, telle qu'elle traverse les frontieres.
			struct GraphBinding {
					NkString name;
					uint32 step = NK_EVAL_NO_SOURCE;
					int32 socket = -1;
			};

			// Correspondance noeud -> etape emise, dans le cadre courant.
			struct GraphNodeStep {
					NkNodeId node = NK_NODE_INVALID;
					uint32 step = 0;
			};

			// Sortie d'un noeud d'instance, une fois le groupe developpe : elle
			// pointe sur l'etape REELLE qui produit la valeur a l'interieur.
			struct GraphInstOut {
					NkNodeId node = NK_NODE_INVALID;
					int32 socket = -1;
					uint32 step = NK_EVAL_NO_SOURCE;
					int32 srcSocket = -1;
			};

			// Verifie que les sockets du noeud d'instance correspondent EXACTEMENT a
			// l'interface du groupe : meme noms, memes types, ni manquant ni en trop.
			//
			// LA COMPARAISON PORTE SUR LES NOMS DE TYPE, JAMAIS SUR LEURS
			// IDENTIFIANTS. Chaque graphe tient son propre registre : le numero 1
			// peut designer « flux » ici et « maillage » la. Un controle par
			// identifiant laisserait donc passer le cas le PLUS dangereux — celui ou
			// les deux cotes portent le meme numero et croient s'accorder.
			//
			// C'est ce controle qui manquait, et non un registre partage : partager
			// les identifiants rendrait la comparaison moins couteuse, mais sans
			// verification il n'y aurait toujours rien pour refuser.
			inline bool GraphCheckInterface(const NkNodeGraph &parent, const NkNode &inst, const NkNodeGraph &child,
											NkString &detail) {
				struct Expected {
						NkString name;
						NkString typeName;
						NkSocketDir dirOnInstance;
				};
				NkVector<Expected> expected;

				// L'interface du groupe = les sockets de ses noeuds frontiere. On
				// accepte plusieurs noeuds d'entree ou de sortie : leur union forme
				// l'interface, comme chez Blender.
				for (uint32 gi = 0; gi < child.RawNodeCount(); ++gi) {
					const NkNode *n = child.RawNodeAt(gi);
					if (!n || !n->alive)
						continue;
					const bool isIn = GraphStrEq(n->type, NK_NODE_GROUP_IN);
					const bool isOut = GraphStrEq(n->type, NK_NODE_GROUP_OUT);
					if (!isIn && !isOut)
						continue;
					for (uint32 s = 0; s < (uint32)n->sockets.Size(); ++s) {
						// Le noeud d'ENTREE du groupe porte des sockets de SORTIE : il
						// alimente l'interieur. Sur l'instance, ce sont des ENTREES.
						if (isIn && n->sockets[s].dir != NkSocketDir::Output)
							continue;
						if (isOut && n->sockets[s].dir != NkSocketDir::Input)
							continue;
						Expected e;
						e.name = n->sockets[s].name;
						const NkString *tn = child.TypeName(n->sockets[s].type);
						e.typeName = tn ? *tn : NkString("");
						e.dirOnInstance = isIn ? NkSocketDir::Input : NkSocketDir::Output;
						expected.PushBack(e);
					}
				}

				// a) tout ce que le groupe attend doit exister sur l'instance, au bon
				//    nom ET au bon type.
				for (uint32 i = 0; i < (uint32)expected.Size(); ++i) {
					const int32 si = inst.FindSocket(expected[i].name.CStr(), expected[i].dirOnInstance);
					if (si < 0) {
						detail = NkString("socket absent sur l'instance : ");
						detail.Append(expected[i].name);
						return false;
					}
					const NkString *pn = parent.TypeName(inst.sockets[(uint32)si].type);
					const NkString instType = pn ? *pn : NkString("");
					if (!GraphStrEqS(instType, expected[i].typeName)) {
						detail = NkString("type different sur ");
						detail.Append(expected[i].name);
						detail.Append(" : groupe=");
						detail.Append(expected[i].typeName);
						detail.Append(" instance=");
						detail.Append(instType);
						return false;
					}
				}

				// b) et reciproquement : un socket EN TROP sur l'instance serait un
				//    fil branche sur rien. Sans ce controle, une entree se retrouverait
				//    silencieusement debranchee apres modification du groupe -- le
				//    calcul continuerait, avec une valeur manquante.
				for (uint32 s = 0; s < (uint32)inst.sockets.Size(); ++s) {
					bool found = false;
					for (uint32 i = 0; i < (uint32)expected.Size() && !found; ++i)
						if (expected[i].dirOnInstance == inst.sockets[s].dir
							&& GraphStrEqS(expected[i].name, inst.sockets[s].name))
							found = true;
					if (!found) {
						detail = NkString("socket en trop sur l'instance : ");
						detail.Append(inst.sockets[s].name);
						return false;
					}
				}
				return true;
			}

			inline const GraphBinding *FindBinding(const NkVector<GraphBinding> &v, const NkString &name) {
				for (uint32 i = 0; i < (uint32)v.Size(); ++i)
					if (GraphStrEqS(v[i].name, name))
						return &v[i];
				return nullptr;
			}

		} // namespace detail

		// ── DOCUMENT ────────────────────────────────────────────────────────────
		inline uint32 NkGraphDocument::AddGraph(const char *name) {
			const int32 existing = FindGraph(name);
			if (existing >= 0)
				return (uint32)existing;
			mGraphs.PushBack(NkNodeGraph());
			mNames.PushBack(NkString(name ? name : ""));
			return (uint32)mGraphs.Size() - 1u;
		}

		inline int32 NkGraphDocument::FindGraph(const char *name) const {
			for (uint32 i = 0; i < (uint32)mNames.Size(); ++i)
				if (detail::GraphStrEq(mNames[i], name))
					return (int32)i;
			return -1;
		}

		inline void NkGraphDocument::Clear() {
			mGraphs.Clear();
			mNames.Clear();
			mRoot = 0;
		}

		// ── APLATISSEMENT ───────────────────────────────────────────────────────
		namespace detail {

			struct GraphExpandCtx {
					const NkGraphDocument *doc = nullptr;
					NkEvalPlan *plan = nullptr;
					NkVector<uint32> active; ///< pile des graphes en cours : detecte la recursion
			};

			inline NkPlanError GraphExpand(GraphExpandCtx &ctx, uint32 graphIdx, const NkVector<GraphBinding> &inBind,
										   NkVector<GraphBinding> &outBind, const NkString &path, uint32 depth);

			// Resout la source d'une entree DANS LE CADRE COURANT, en traversant les
			// frontieres : une entree alimentee par le noeud d'entree du groupe
			// remonte au cadre appelant ; une entree alimentee par un noeud
			// d'instance descend vers l'etape reelle qui produit la valeur.
			inline void GraphResolve(const NkNodeGraph &g, NkNodeId node, int32 socketIdx,
									 const NkVector<GraphBinding> &inBind, const NkVector<GraphNodeStep> &emitted,
									 const NkVector<GraphInstOut> &instOuts, uint32 &outStep, int32 &outSocket) {
				outStep = NK_EVAL_NO_SOURCE;
				outSocket = -1;
				const NkLink *l = g.IncomingOf(node, socketIdx);
				if (!l)
					return; // entree libre : la sentinelle reste NK_EVAL_NO_SOURCE
				const NkNode *src = g.Find(l->fromNode);
				if (!src)
					return;

				if (GraphStrEq(src->type, NK_NODE_GROUP_IN)) {
					// La valeur vient de l'exterieur du groupe.
					if (l->fromSocket >= 0 && l->fromSocket < (int32)src->sockets.Size()) {
						const GraphBinding *b = FindBinding(inBind, src->sockets[(uint32)l->fromSocket].name);
						if (b) {
							outStep = b->step;
							outSocket = b->socket;
						}
					}
					return;
				}
				if (GraphStrEq(src->type, NK_NODE_INSTANCE)) {
					// La valeur vient de l'interieur d'un groupe deja developpe.
					for (uint32 i = 0; i < (uint32)instOuts.Size(); ++i)
						if (instOuts[i].node == l->fromNode && instOuts[i].socket == l->fromSocket) {
							outStep = instOuts[i].step;
							outSocket = instOuts[i].srcSocket;
							return;
						}
					return;
				}
				// Noeud ordinaire : il a produit une etape.
				for (uint32 i = 0; i < (uint32)emitted.Size(); ++i)
					if (emitted[i].node == l->fromNode) {
						outStep = emitted[i].step;
						outSocket = l->fromSocket;
						return;
					}
			}

			inline NkPlanError GraphExpand(GraphExpandCtx &ctx, uint32 graphIdx, const NkVector<GraphBinding> &inBind,
										   NkVector<GraphBinding> &outBind, const NkString &path, uint32 depth) {
				if (depth > 32u)
					return NkPlanError::TooDeep;
				// Un groupe qui s'instancie lui-meme, directement ou par un
				// intermediaire, ferait boucler l'expansion jusqu'a epuisement de la
				// pile. On le REFUSE en le nommant, plutot que de planter.
				for (uint32 i = 0; i < (uint32)ctx.active.Size(); ++i)
					if (ctx.active[i] == graphIdx)
						return NkPlanError::RecursiveSubgraph;
				ctx.active.PushBack(graphIdx);

				const NkNodeGraph &g = ctx.doc->GraphAt(graphIdx);
				NkVector<NkNodeId> order;
				if (!g.TopoSort(order)) {
					ctx.active.PopBack();
					return NkPlanError::Cycle;
				}

				NkVector<GraphNodeStep> emitted;
				NkVector<GraphInstOut> instOuts;

				for (uint32 oi = 0; oi < (uint32)order.Size(); ++oi) {
					const NkNode *n = g.Find(order[oi]);
					if (!n)
						continue;

					// Frontiere d'entree : rien a emettre, elle sert seulement de
					// point de raccord et disparait du plan.
					if (GraphStrEq(n->type, NK_NODE_GROUP_IN))
						continue;

					// Frontiere de sortie : on note vers quoi chacune de ses entrees
					// pointe REELLEMENT, pour que l'appelant s'y raccroche.
					if (GraphStrEq(n->type, NK_NODE_GROUP_OUT)) {
						for (uint32 s = 0; s < (uint32)n->sockets.Size(); ++s) {
							if (n->sockets[s].dir != NkSocketDir::Input)
								continue;
							GraphBinding b;
							b.name = n->sockets[s].name;
							GraphResolve(g, n->id, (int32)s, inBind, emitted, instOuts, b.step, b.socket);
							outBind.PushBack(b);
						}
						continue;
					}

					// Noeud d'instance : on developpe le groupe SUR PLACE. Chaque
					// instance produit ses PROPRES etapes -- deux instances du meme
					// groupe sont deux calculs distincts, pas un calcul partage.
					if (GraphStrEq(n->type, NK_NODE_INSTANCE)) {
						const int32 target = ctx.doc->FindGraph(n->subgraph.CStr());
						if (target < 0) {
							ctx.active.PopBack();
							return NkPlanError::UnknownSubgraph;
						}
						// L'interface est verifiee AVANT de developper : un groupe
						// modifie apres avoir ete instancie doit etre signale, pas
						// developpe a moitie.
						{
							NkString why;
							if (!GraphCheckInterface(g, *n, ctx.doc->GraphAt((uint32)target), why)) {
								ctx.plan->errorDetail = path;
								ctx.plan->errorDetail.Append('/');
								ctx.plan->errorDetail.Append(n->label);
								ctx.plan->errorDetail.Append(" : ");
								ctx.plan->errorDetail.Append(why);
								ctx.active.PopBack();
								return NkPlanError::InterfaceMismatch;
							}
						}
						NkVector<GraphBinding> childIn;
						for (uint32 s = 0; s < (uint32)n->sockets.Size(); ++s) {
							if (n->sockets[s].dir != NkSocketDir::Input)
								continue;
							GraphBinding b;
							b.name = n->sockets[s].name;
							GraphResolve(g, n->id, (int32)s, inBind, emitted, instOuts, b.step, b.socket);
							childIn.PushBack(b);
						}
						NkString childPath = path;
						childPath.Append('/');
						childPath.Append(n->label);
						NkVector<GraphBinding> childOut;
						const NkPlanError e =
							GraphExpand(ctx, (uint32)target, childIn, childOut, childPath, depth + 1u);
						if (e != NkPlanError::Ok) {
							ctx.active.PopBack();
							return e;
						}
						// Les sorties de l'instance pointent desormais sur les etapes
						// reelles produites a l'interieur.
						for (uint32 s = 0; s < (uint32)n->sockets.Size(); ++s) {
							if (n->sockets[s].dir != NkSocketDir::Output)
								continue;
							const GraphBinding *b = FindBinding(childOut, n->sockets[s].name);
							GraphInstOut io;
							io.node = n->id;
							io.socket = (int32)s;
							io.step = b ? b->step : NK_EVAL_NO_SOURCE;
							io.srcSocket = b ? b->socket : -1;
							instOuts.PushBack(io);
						}
						continue;
					}

					// Noeud ordinaire : une etape du plan.
					NkEvalStep st;
					st.graph = graphIdx;
					st.node = n->id;
					st.depth = depth;
					st.path = path;
					for (uint32 s = 0; s < (uint32)n->sockets.Size(); ++s) {
						if (n->sockets[s].dir != NkSocketDir::Input)
							continue;
						NkEvalInput in;
						in.dstSocket = (int32)s;
						GraphResolve(g, n->id, (int32)s, inBind, emitted, instOuts, in.srcStep, in.srcSocket);
						st.inputs.PushBack(in);
					}
					ctx.plan->steps.PushBack(st);
					GraphNodeStep ns;
					ns.node = n->id;
					ns.step = (uint32)ctx.plan->steps.Size() - 1u;
					emitted.PushBack(ns);
				}

				ctx.active.PopBack();
				return NkPlanError::Ok;
			}

		} // namespace detail

		inline NkPlanError NkGraphDocument::BuildPlan(NkEvalPlan &out) const {
			out.Clear();
			if (mGraphs.Empty() || mRoot >= (uint32)mGraphs.Size())
				return NkPlanError::EmptyDocument;
			detail::GraphExpandCtx ctx;
			ctx.doc = this;
			ctx.plan = &out;
			NkVector<detail::GraphBinding> noIn, rootOut;
			const NkString path = mNames[mRoot];
			const NkPlanError e = detail::GraphExpand(ctx, mRoot, noIn, rootOut, path, 0u);
			if (e != NkPlanError::Ok) {
				// On vide les etapes — un plan partiel serait pire qu'aucun plan —
				// mais on GARDE le message : c'est tout ce qui reste a montrer.
				const NkString why = out.errorDetail;
				out.Clear();
				out.errorDetail = why;
			}
			return e;
		}

		// ── SERIALISATION DU DOCUMENT ───────────────────────────────────────────
		// Chaque graphe est precede d'une ligne `graphe <nom>` ; son contenu reste
		// EXACTEMENT le format `.nkgraph` d'un graphe seul, entete comprise. Un
		// document se decoupe donc en graphes autonomes, qu'on peut extraire et
		// relire un par un.
		inline void NkGraphDocument::Serialize(NkString &out) const {
			out = NkString("nkgraphdoc 1\n");
			out.Append("racine ");
			detail::PutU32(out, mRoot);
			out.Append('\n');
			for (uint32 i = 0; i < (uint32)mGraphs.Size(); ++i) {
				out.Append("graphe ");
				out.Append(mNames[i]);
				out.Append('\n');
				NkString body;
				mGraphs[i].Serialize(body);
				out.Append(body);
			}
		}

		inline bool NkGraphDocument::Deserialize(const char *text) {
			if (!text)
				return false;
			Clear();
			if (!detail::LineIs(text, "nkgraphdoc"))
				return false;

			NkString pendingName;
			NkString body;
			bool haveGraph = false;
			auto flush = [&]() {
				if (!haveGraph)
					return;
				const uint32 idx = AddGraph(pendingName.CStr());
				mGraphs[idx].Deserialize(body.CStr());
				haveGraph = false;
				body = NkString("");
			};

			for (const char *line = text; *line; line = detail::NextLine(line)) {
				if (detail::LineIs(line, "nkgraphdoc"))
					continue;
				if (detail::LineIs(line, "racine")) {
					const char *p = line;
					NkString kw;
					detail::TokenStr(p, kw);
					mRoot = detail::TokenU32(p);
					continue;
				}
				if (detail::LineIs(line, "graphe")) {
					flush();
					const char *p = line;
					NkString kw;
					detail::TokenStr(p, kw);
					detail::RestOfLine(p, pendingName);
					haveGraph = true;
					continue;
				}
				if (haveGraph)
					for (const char *q = line; *q && *q != '\n'; ++q)
						body.Append(*q);
				if (haveGraph)
					body.Append('\n');
			}
			flush();
			return true;
		}

	} // namespace graph
} // namespace nkentseu
