#pragma once
// -----------------------------------------------------------------------------
// @File    NkNodeGraphIO.inl
// @Brief   Serialisation `.nkgraph` et historique annuler/refaire.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// FORMAT TEXTE, une directive par ligne. Le choix du texte n'est pas de la
// paresse : un graphe se relit, se compare (`git diff`), et se repare a la main
// quand une version future casse quelque chose. Un format binaire ferait gagner
// des octets sur des fichiers qui pesent quelques kilo-octets.
//
//   nkgraph 1
//   compteurs <prochainNoeud> <prochainLien>
//   type <id> <nom...>
//   conv <de> <vers>
//   noeud <id> <x> <y> <cleType> <libelle...>
//   sock <idNoeud> <0=entree|1=sortie> <idType> <nom...>
//   lien <id> <deNoeud> <deSock> <versNoeud> <versSock>
//
// LA LIGNE `compteurs` EST LA PLUS IMPORTANTE DU FICHIER. Sans elle, un graphe
// recharge REATTRIBUERAIT les identifiants liberes par une suppression — et une
// courbe d'animation ou une reference sauvegardee ailleurs pointerait
// silencieusement sur un autre noeud. C'est un defaut qui ne se voit qu'au
// resultat, longtemps apres.
// -----------------------------------------------------------------------------

#include <stdio.h>	// snprintf : formatage des nombres, pas de flux
#include <stdlib.h> // strtod

namespace nkentseu {
	namespace graph {

		namespace detail {

			inline void PutU32(NkString &s, uint32 v) {
				char b[16];
				snprintf(b, sizeof(b), "%u", v);
				s.Append(b);
			}

			inline void PutF32(NkString &s, float32 v) {
				char b[32];
				snprintf(b, sizeof(b), "%.6f", (double)v);
				s.Append(b);
			}

			// Avance jusqu'au prochain jeton, renvoie sa longueur. `p` pointe dessus.
			inline const char *NextToken(const char *p, uint32 &len) {
				while (*p == ' ' || *p == '\t')
					++p;
				const char *start = p;
				while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
					++p;
				len = (uint32)(p - start);
				return start;
			}

			inline uint32 TokenU32(const char *&p) {
				uint32 len = 0;
				const char *t = NextToken(p, len);
				uint32 v = 0;
				for (uint32 i = 0; i < len; ++i) {
					if (t[i] < '0' || t[i] > '9')
						break;
					v = v * 10u + (uint32)(t[i] - '0');
				}
				p = t + len;
				return v;
			}

			inline float32 TokenF32(const char *&p) {
				uint32 len = 0;
				const char *t = NextToken(p, len);
				char b[40];
				const uint32 n = len < 39 ? len : 39;
				for (uint32 i = 0; i < n; ++i)
					b[i] = t[i];
				b[n] = 0;
				p = t + len;
				return (float32)strtod(b, nullptr);
			}

			// Jeton simple (une cle, jamais d'espace).
			inline void TokenStr(const char *&p, NkString &out) {
				uint32 len = 0;
				const char *t = NextToken(p, len);
				out = NkString("");
				for (uint32 i = 0; i < len; ++i)
					out.Append(t[i]);
				p = t + len;
			}

			// RESTE de la ligne : les libelles sont traduisibles, donc ils
			// contiennent des espaces. Ils sont toujours places en DERNIER pour que
			// ce cas reste trivial, sans guillemets ni echappement a mal gerer.
			inline void RestOfLine(const char *&p, NkString &out) {
				while (*p == ' ' || *p == '\t')
					++p;
				out = NkString("");
				while (*p && *p != '\n' && *p != '\r')
					out.Append(*p++);
			}

			inline const char *NextLine(const char *p) {
				while (*p && *p != '\n')
					++p;
				return *p ? p + 1 : p;
			}

			inline bool LineIs(const char *p, const char *kw) {
				while (*p == ' ' || *p == '\t')
					++p;
				while (*kw) {
					if (*p != *kw)
						return false;
					++p;
					++kw;
				}
				return *p == ' ' || *p == '\t';
			}

		} // namespace detail

		// ── ECRITURE ────────────────────────────────────────────────────────────
		inline void NkNodeGraph::Serialize(NkString &out) const {
			out = NkString("nkgraph 1\n");

			out.Append("compteurs ");
			detail::PutU32(out, mNextNode);
			out.Append(' ');
			detail::PutU32(out, mNextLink);
			out.Append('\n');

			for (uint32 i = 1; i < (uint32)mTypeNames.Size(); ++i) {
				out.Append("type ");
				detail::PutU32(out, i);
				out.Append(' ');
				out.Append(mTypeNames[i]);
				out.Append('\n');
			}

			for (uint32 i = 0; i < (uint32)mConversions.Size(); ++i) {
				out.Append("conv ");
				detail::PutU32(out, (uint32)(mConversions[i] >> 32));
				out.Append(' ');
				detail::PutU32(out, (uint32)(mConversions[i] & 0xFFFFFFFFull));
				out.Append('\n');
			}

			// Les noeuds morts ne sont PAS ecrits — mais la ligne `compteurs`
			// ci-dessus garde leur identifiant hors d'atteinte.
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i) {
				const NkNode &n = mNodes[i];
				if (!n.alive)
					continue;
				out.Append("noeud ");
				detail::PutU32(out, n.id);
				out.Append(' ');
				detail::PutF32(out, n.x);
				out.Append(' ');
				detail::PutF32(out, n.y);
				out.Append(' ');
				out.Append(n.type);
				out.Append(' ');
				out.Append(n.label);
				out.Append('\n');
				// Directive DEDIEE plutot qu'un champ de plus sur la ligne `noeud` :
				// les fichiers ecrits avant les sous-graphes restent lisibles tels
				// quels, et un noeud ordinaire n'ecrit rien du tout.
				if (n.subgraph.Size() > 0) {
					out.Append("sousgraphe ");
					detail::PutU32(out, n.id);
					out.Append(' ');
					out.Append(n.subgraph);
					out.Append('\n');
				}
				// Les sockets suivent leur noeud, dans l'ordre : cet ordre EST leur
				// index, et les liens s'y referent.
				for (uint32 k = 0; k < (uint32)n.sockets.Size(); ++k) {
					const NkSocket &s = n.sockets[k];
					out.Append("sock ");
					detail::PutU32(out, n.id);
					out.Append(s.dir == NkSocketDir::Output ? " 1 " : " 0 ");
					detail::PutU32(out, s.type);
					out.Append(' ');
					out.Append(s.name);
					out.Append('\n');
				}
			}

			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i) {
				const NkLink &l = mLinks[i];
				if (!l.alive)
					continue;
				out.Append("lien ");
				detail::PutU32(out, l.id);
				out.Append(' ');
				detail::PutU32(out, l.fromNode);
				out.Append(' ');
				detail::PutU32(out, (uint32)l.fromSocket);
				out.Append(' ');
				detail::PutU32(out, l.toNode);
				out.Append(' ');
				detail::PutU32(out, (uint32)l.toSocket);
				out.Append('\n');
			}
		}

		// ── LECTURE ─────────────────────────────────────────────────────────────
		inline bool NkNodeGraph::Deserialize(const char *text) {
			if (!text)
				return false;
			Clear();
			if (!detail::LineIs(text, "nkgraph"))
				return false;

			for (const char *line = text; *line; line = detail::NextLine(line)) {
				const char *p = line;
				NkString kw;
				detail::TokenStr(p, kw);

				if (detail::GraphStrEq(kw, "compteurs")) {
					mNextNode = detail::TokenU32(p);
					mNextLink = detail::TokenU32(p);
				} else if (detail::GraphStrEq(kw, "type")) {
					const uint32 id = detail::TokenU32(p);
					NkString name;
					detail::RestOfLine(p, name);
					// On RESPECTE l'identifiant ecrit au lieu d'en attribuer un
					// nouveau : les sockets s'y referent par numero.
					while ((uint32)mTypeNames.Size() <= id)
						mTypeNames.PushBack(NkString(""));
					mTypeNames[id] = name;
				} else if (detail::GraphStrEq(kw, "conv")) {
					const uint32 a = detail::TokenU32(p);
					const uint32 b = detail::TokenU32(p);
					mConversions.PushBack(((uint64)a << 32) | (uint64)b);
				} else if (detail::GraphStrEq(kw, "noeud")) {
					NkNode n;
					n.id = detail::TokenU32(p);
					n.x = detail::TokenF32(p);
					n.y = detail::TokenF32(p);
					detail::TokenStr(p, n.type);
					detail::RestOfLine(p, n.label);
					n.alive = true;
					mNodes.PushBack(n);
				} else if (detail::GraphStrEq(kw, "sousgraphe")) {
					const uint32 nid = detail::TokenU32(p);
					NkNode *n = Find(nid);
					if (n)
						detail::RestOfLine(p, n->subgraph);
				} else if (detail::GraphStrEq(kw, "sock")) {
					const uint32 nid = detail::TokenU32(p);
					const uint32 dir = detail::TokenU32(p);
					NkSocket s;
					s.type = detail::TokenU32(p);
					s.dir = dir ? NkSocketDir::Output : NkSocketDir::Input;
					detail::RestOfLine(p, s.name);
					NkNode *n = Find(nid);
					if (n)
						n->sockets.PushBack(s);
				} else if (detail::GraphStrEq(kw, "lien")) {
					NkLink l;
					l.id = detail::TokenU32(p);
					l.fromNode = detail::TokenU32(p);
					l.fromSocket = (int32)detail::TokenU32(p);
					l.toNode = detail::TokenU32(p);
					l.toSocket = (int32)detail::TokenU32(p);
					l.alive = true;
					mLinks.PushBack(l);
				}
			}
			// Un `mNextNode` absent du fichier laisserait le graphe attribuer 1 au
			// prochain noeud, donc ecraser un identifiant existant.
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i)
				if (mNodes[i].id >= mNextNode)
					mNextNode = mNodes[i].id + 1;
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].id >= mNextLink)
					mNextLink = mLinks[i].id + 1;
			return true;
		}

		// ── HISTORIQUE ──────────────────────────────────────────────────────────
		inline void NkGraphHistory::Reset(const NkNodeGraph &g) {
			mStack.Clear();
			NkString s;
			g.Serialize(s);
			mStack.PushBack(s);
			mCursor = 0;
		}

		inline void NkGraphHistory::Commit(const NkNodeGraph &g) {
			if (mStack.Empty()) {
				Reset(g);
				return;
			}
			// Modifier apres une annulation ABANDONNE la branche refaisable. C'est
			// le comportement de tous les editeurs : garder les deux branches
			// obligerait a montrer un arbre a l'utilisateur.
			while ((uint32)mStack.Size() > mCursor + 1)
				mStack.PopBack();
			NkString s;
			g.Serialize(s);
			mStack.PushBack(s);
			mCursor = (uint32)mStack.Size() - 1;
			while ((uint32)mStack.Size() > mLimit) {
				// On retire le PLUS ANCIEN. NkVector n'offre pas de retrait en tete :
				// on decale, le cout est negligeable devant la profondeur retenue.
				for (uint32 i = 1; i < (uint32)mStack.Size(); ++i)
					mStack[i - 1] = mStack[i];
				mStack.PopBack();
				if (mCursor > 0)
					mCursor--;
			}
		}

		inline bool NkGraphHistory::Undo(NkNodeGraph &g) {
			if (mCursor == 0 || mStack.Empty())
				return false;
			mCursor--;
			return g.Deserialize(mStack[mCursor].CStr());
		}

		inline bool NkGraphHistory::Redo(NkNodeGraph &g) {
			if (mStack.Empty() || mCursor + 1 >= (uint32)mStack.Size())
				return false;
			mCursor++;
			return g.Deserialize(mStack[mCursor].CStr());
		}

		inline uint32 NkGraphHistory::UndoDepth() const {
			return mCursor;
		}

		inline uint32 NkGraphHistory::RedoDepth() const {
			return mStack.Empty() ? 0u : (uint32)mStack.Size() - 1u - mCursor;
		}

	} // namespace graph
} // namespace nkentseu
