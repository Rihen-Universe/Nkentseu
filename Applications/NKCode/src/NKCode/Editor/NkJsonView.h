#pragma once
// =============================================================================
// NkJsonView.h — Éditeur d'arbre JSON de NKCode (aperçu ÉDITABLE).
//   Parse le JSON en arbre à plat repliable + coloré, et permet d'ÉDITER
//   directement : clic sur une valeur ou une clé -> champ en place (Entrée
//   valide), boutons [+]/[–] pour ajouter/supprimer des membres/éléments, cycle
//   de type sur les scalaires. Chaque édition réécrit le fichier .json (le doc
//   texte est régénéré ; l'arbre reste la source de vérité tant qu'on édite).
//   Défilement via le scrollbar standard NKEditorKit.
// @Author  Rihen
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKEditorKit/NkEditorTextField.h" // NkOverlayTextField (edition en place)
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkFormat.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::nkgui;

		// Un nœud de l'arbre JSON (liste à plat, ordre pré-fixe).
		struct NkJNode {
				int32 depth = 0;
				int32 type = 0;			// 0 = scalaire, 1 = objet {}, 2 = tableau []
				int32 count = 0;		// nombre d'enfants directs (conteneurs)
				NkString key;			// clé AVEC guillemets ("nom") ou "" (élément de tableau / racine)
				NkString val;			// scalaire : texte brut (chaîne AVEC guillemets) ; conteneurs : ""
				int32 col = 0;			// 0 autre, 1 chaîne(vert), 2 nombre(orange), 3 mot-clé(violet)
				bool hasKey = false;	// membre d'objet
				bool collapsed = false; // conteneur replié
				bool empty = false;		// conteneur vide {} / []
		};

		// ── Utilitaires chaîne JSON ───────────────────────────────────────────────
		inline NkString NkJUnquote(const NkString &s) { // "abc" -> abc (déséchappe minimal)
			const char *p = s.CStr();
			usize n = s.Size();
			if (n >= 2 && p[0] == '"' && p[n - 1] == '"') {
				NkString o;
				for (usize i = 1; i + 1 < n; ++i) {
					if (p[i] == '\\' && i + 2 < n) {
						const char c = p[i + 1];
						if (c == 'n')
							o += '\n';
						else if (c == 't')
							o += '\t';
						else if (c == '"')
							o += '"';
						else if (c == '\\')
							o += '\\';
						else {
							o += p[i];
							o += c;
						}
						++i;
						continue;
					}
					o += p[i];
				}
				return o;
			}
			return s;
		}
		inline NkString NkJQuote(const char *raw) { // abc -> "abc" (échappe " \ \n \t)
			NkString o;
			o += '"';
			for (const char *p = raw; *p; ++p) {
				if (*p == '"' || *p == '\\') {
					o += '\\';
					o += *p;
				} else if (*p == '\n') {
					o += '\\';
					o += 'n';
				} else if (*p == '\t') {
					o += '\\';
					o += 't';
				} else
					o += *p;
			}
			o += '"';
			return o;
		}

		// ── Parseur JSON -> liste de nœuds à plat ─────────────────────────────────
		struct NkJParser {
				const char *p = nullptr, *end = nullptr;
				NkVector<NkJNode> *out = nullptr;
				bool ok = true;

				void Skip() {
					while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
						++p;
				}
				NkString ReadString() {
					NkString s;
					if (p >= end || *p != '"')
						return s;
					s += '"';
					++p;
					while (p < end && *p != '"') {
						if (*p == '\\' && p + 1 < end) {
							s += *p++;
							s += *p++;
							continue;
						}
						s += *p++;
					}
					if (p < end) {
						s += '"';
						++p;
					}
					return s;
				}
				void ParseValue(int32 depth, const NkString &key, bool hasKey) {
					Skip();
					if (p >= end) {
						ok = false;
						return;
					}
					const char ch = *p;
					if (ch == '{' || ch == '[') {
						const bool obj = (ch == '{');
						++p;
						const usize nodeIdx = out->Size();
						NkJNode n;
						n.depth = depth;
						n.type = obj ? 1 : 2;
						n.key = key;
						n.hasKey = hasKey;
						out->PushBack(n);
						int32 count = 0;
						Skip();
						const char close = obj ? '}' : ']';
						if (p < end && *p == close) {
							++p;
							(*out)[nodeIdx].empty = true;
							return;
						}
						while (p < end && ok) {
							Skip();
							NkString memberKey;
							bool memberHasKey = false;
							if (obj) {
								memberKey = ReadString();
								memberHasKey = true;
								Skip();
								if (p < end && *p == ':')
									++p;
							}
							ParseValue(depth + 1, memberKey, memberHasKey);
							++count;
							Skip();
							if (p < end && *p == ',') {
								++p;
								continue;
							}
							if (p < end && *p == close) {
								++p;
								break;
							}
							break;
						}
						(*out)[nodeIdx].count = count;
						return;
					}
					NkJNode n;
					n.depth = depth;
					n.type = 0;
					n.key = key;
					n.hasKey = hasKey;
					if (ch == '"') {
						n.val = ReadString();
						n.col = 1;
					} else if (ch == 't' || ch == 'f' || ch == 'n') {
						const char *s = p;
						while (p < end && (*p >= 'a' && *p <= 'z'))
							++p;
						for (const char *z = s; z < p; ++z)
							n.val += *z;
						n.col = 3;
					} else {
						const char *s = p;
						while (p < end && (*p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E' ||
										   (*p >= '0' && *p <= '9')))
							++p;
						for (const char *z = s; z < p; ++z)
							n.val += *z;
						n.col = 2;
						if (n.val.Empty()) {
							n.val += *p++;
							n.col = 0;
						}
					}
					out->PushBack(n);
				}
		};

		// ── Sérialisation arbre -> texte JSON (pretty, 2 espaces) ──────────────────
		inline usize NkJSer(const NkVector<NkJNode> &nodes, usize i, NkString &out, int32 indent) {
			auto pad = [&](int32 d) {
				for (int32 k = 0; k < d; ++k)
					out += "  ";
			};
			const NkJNode &n = nodes[i];
			if (n.type == 0) {
				out += n.val.Empty() ? NkString("null") : n.val;
				return i + 1;
			}
			const bool obj = (n.type == 1);
			out += obj ? "{" : "[";
			if (n.empty || n.count == 0) {
				out += obj ? "}" : "]";
				return i + 1;
			}
			out += "\n";
			usize j = i + 1;
			for (int32 c = 0; c < n.count; ++c) {
				pad(indent + 1);
				if (obj) {
					out += nodes[j].key.Empty() ? NkString("\"\"") : nodes[j].key;
					out += ": ";
				}
				j = NkJSer(nodes, j, out, indent + 1);
				if (c < n.count - 1)
					out += ",";
				out += "\n";
			}
			pad(indent);
			out += obj ? "}" : "]";
			return j;
		}
		inline NkString NkJSerialize(const NkVector<NkJNode> &nodes) {
			NkString out;
			if (!nodes.Empty())
				NkJSer(nodes, 0, out, 0);
			return out;
		}

		// Fin (exclusive) du sous-arbre du nœud i = 1er nœud suivant de profondeur <= celle de i.
		inline usize NkJSubtreeEnd(const NkVector<NkJNode> &nodes, usize i) {
			const int32 d = nodes[i].depth;
			usize j = i + 1;
			while (j < nodes.Size() && nodes[j].depth > d)
				++j;
			return j;
		}
		// Index du conteneur parent de i (nœud précédent de profondeur = depth-1), ou -1.
		inline int32 NkJParent(const NkVector<NkJNode> &nodes, usize i) {
			const int32 d = nodes[i].depth;
			for (int32 j = (int32)i - 1; j >= 0; --j)
				if (nodes[j].depth == d - 1)
					return j;
			return -1;
		}

		// ── Cache par onglet (état de pliage + édition) ───────────────────────────
		struct NkJsonTree {
				NkVector<NkJNode> nodes;
				usize srcLen = 0;
				uint32 srcHash = 0;
				bool valid = false;
				int32 editIdx = -1; // nœud en cours d'édition
				int32 editKind = 0; // 1 = valeur, 2 = clé
				char editBuf[1024] = {};
		};

		inline uint32 NkJHash(const char *s, usize n) {
			uint32 h = 2166136261u;
			for (usize i = 0; i < n; ++i)
				h = (h ^ (uint8)s[i]) * 16777619u;
			return h;
		}
		inline NkVector<NkJsonTree *> &NkJsonTrees() {
			static NkVector<NkJsonTree *> v;
			return v;
		}
		inline NkJsonTree *NkJsonTreeFor(const void *keyPtr, const char *text) {
			static NkVector<const void *> keys;
			auto &docs = NkJsonTrees();
			NkJsonTree *d = nullptr;
			for (usize i = 0; i < keys.Size(); ++i)
				if (keys[i] == keyPtr) {
					d = docs[i];
					break;
				}
			if (!d) {
				d = new NkJsonTree();
				keys.PushBack(keyPtr);
				docs.PushBack(d);
			}
			usize len = 0;
			while (text[len])
				++len;
			const uint32 h = NkJHash(text, len);
			if (d->srcLen != len || d->srcHash != h) { // le texte a changé (édité en mode code) -> reparse
				d->nodes.Clear();
				d->editIdx = -1;
				NkJParser ps;
				ps.p = text;
				ps.end = text + len;
				ps.out = &d->nodes;
				ps.ParseValue(0, NkString(), false);
				d->valid = ps.ok && !d->nodes.Empty();
				d->srcLen = len;
				d->srcHash = h;
			}
			return d;
		}

		// ── Rendu + édition de l'arbre dans `area`. Si une édition modifie l'arbre,
		//    pose outChanged=true et outNewText = le JSON régénéré (à réécrire dans le doc). ──
		inline void NkDrawJson(NkGuiContext &ctx, const void *key, const char *text, const NkRect &area,
							   float32 &extScroll, NkString &outNewText, bool &outChanged) {
			outChanged = false;
			NkGuiDrawList &dl = ctx.DL();
			const NkGuiFont *font = ctx.font;
			const float32 S = ctx.S(1.f);
			dl.PushClipRect(area, true);
			dl.AddRectFilled(area, NkColor{22, 24, 28, 255});
			if (!font || !font->Valid()) {
				dl.PopClipRect();
				return;
			}
			const float32 lh = font->LineHeight(), asc = font->Ascent();
			const float32 rowH = lh + 8.f * S;
			NkJsonTree *d = NkJsonTreeFor(key, text);
			if (!d->valid) {
				dl.AddText(font->Face(), font->TexId(), {area.x + 16.f * S, area.y + 16.f * S + asc},
						   "JSON invalide ou vide", NkColor{240, 120, 110, 255});
				dl.PopClipRect();
				return;
			}
			auto &in = ctx.input;
			const NkVec2 mp = in.mousePos;
			const float32 sbW = 14.f * S, padX = 12.f * S, padY = 8.f * S, indentW = 16.f * S;

			const NkColor colKey{88, 166, 255, 255}, colStr{126, 231, 135, 255}, colNum{247, 154, 40, 255},
				colKw{210, 168, 255, 255}, colPunct{120, 128, 138, 255}, colTri{140, 148, 158, 255};
			auto scalarColor = [&](int32 c) -> NkColor {
				return c == 1 ? colStr : c == 2 ? colNum : c == 3 ? colKw : NkColor{200, 208, 216, 255};
			};

			bool dirty = false;			  // l'arbre a été modifié cette frame
			int32 pendClickToggle = -1;	  // pliage
			int32 pendStartEditV = -1;	  // démarrer édition valeur
			int32 pendStartEditK = -1;	  // démarrer édition clé
			int32 pendCycleType = -1;	  // cycler le type d'un scalaire
			int32 pendAddInto = -1;		  // ajouter un enfant dans ce conteneur
			int32 pendRemove = -1;		  // supprimer ce nœud

			// Commit de l'édition en cours (Entrée / clic ailleurs).
			auto commitEdit = [&]() {
				if (d->editIdx < 0 || d->editIdx >= (int32)d->nodes.Size())
					return;
				NkJNode &n = d->nodes[d->editIdx];
				if (d->editKind == 2) { // clé
					n.key = NkJQuote(d->editBuf);
					n.hasKey = true;
				} else { // valeur
					if (n.col == 1)
						n.val = NkJQuote(d->editBuf); // chaîne -> re-guillemetée
					else
						n.val = NkString(d->editBuf); // nombre / mot-clé : brut
				}
				d->editIdx = -1;
				d->editKind = 0;
				dirty = true;
			};

			// Visibilité (pliage).
			static NkVector<int32> vis;
			vis.Clear();
			int32 hideDepth = -1;
			for (usize i = 0; i < d->nodes.Size(); ++i) {
				const NkJNode &n = d->nodes[i];
				if (hideDepth >= 0 && n.depth > hideDepth)
					continue;
				hideDepth = -1;
				vis.PushBack((int32)i);
				if ((n.type == 1 || n.type == 2) && n.collapsed && !n.empty)
					hideDepth = n.depth;
			}
			const int32 nVis = (int32)vis.Size();
			const float32 contentH = (float32)nVis * rowH + padY * 2.f;
			const float32 viewH = area.h;
			const bool needV = contentH > viewH + 0.5f;
			const float32 innerW = area.w - (needV ? sbW : 0.f);
			const float32 maxScroll = contentH - viewH > 0.f ? contentH - viewH : 0.f;
			if (extScroll < 0.f)
				extScroll = 0.f;
			if (extScroll > maxScroll)
				extScroll = maxScroll;
			if (NkGuiRectContains(area, mp) && in.wheel != 0.f) {
				extScroll -= in.wheel * rowH * 3.f;
				in.wheel = 0.f;
				extScroll = extScroll < 0.f ? 0.f : (extScroll > maxScroll ? maxScroll : extScroll);
			}

			const NkRect clip = {area.x, area.y, innerW, area.h};
			dl.PushClipRect(clip, true);
			for (int32 r = 0; r < nVis; ++r) {
				const float32 y = area.y + padY - extScroll + (float32)r * rowH;
				if (y + rowH <= area.y || y >= area.y + area.h)
					continue;
				const int32 idx = vis[r];
				const NkJNode &n = d->nodes[idx];
				const bool container = (n.type == 1 || n.type == 2);
				const bool rowHov = NkGuiRectContains({area.x, y, innerW, rowH}, mp) && NkGuiRectContains(clip, mp);
				if (rowHov)
					dl.AddRectFilled({area.x, y, innerW, rowH}, NkColor{255, 255, 255, 12});
				float32 x = area.x + padX + (float32)n.depth * indentW;
				const float32 ty = y + (rowH - lh) * 0.5f;

				// Triangle de pliage.
				if (container && !n.empty) {
					const float32 cx = x + 4.f * S, cy = y + rowH * 0.5f;
					if (n.collapsed)
						dl.AddTriangleFilled({cx, cy - 4.f * S}, {cx, cy + 4.f * S}, {cx + 5.f * S, cy}, colTri);
					else
						dl.AddTriangleFilled({cx - 1.f * S, cy - 2.f * S}, {cx + 8.f * S, cy - 2.f * S},
											 {cx + 3.5f * S, cy + 4.f * S}, colTri);
					if (NkGuiRectContains({x, y, 16.f * S, rowH}, mp) && NkGuiRectContains(clip, mp) &&
						in.mouseClicked[0])
						pendClickToggle = idx;
				}
				x += indentW;

				// Clé (éditable).
				if (n.hasKey) {
					const float32 kw = font->MeasureWidth(NkJUnquote(n.key).CStr()) + 4.f * S;
					const NkRect kr = {x, y + 2.f * S, kw + 8.f * S, rowH - 4.f * S};
					if (d->editIdx == idx && d->editKind == 2) {
						editorkit::NkOverlayTextField(ctx, dl, font, kr, d->editBuf, (int32)sizeof(d->editBuf), true);
						x += kr.w;
					} else {
						dl.AddText(font->Face(), font->TexId(), {x + 2.f * S, ty + asc}, NkJUnquote(n.key).CStr(),
								   colKey);
						if (NkGuiRectContains(kr, mp) && NkGuiRectContains(clip, mp) && in.mouseClicked[0])
							pendStartEditK = idx;
						x += kw + 4.f * S;
					}
					dl.AddText(font->Face(), font->TexId(), {x, ty + asc}, ": ", colPunct);
					x += font->MeasureWidth(": ");
				}

				if (container) {
					const char *op = n.type == 1 ? "{" : "[";
					const char *cl = n.type == 1 ? "}" : "]";
					NkString b = n.empty ? (NkString(op) + cl)
										 : n.collapsed ? NkPrintf("%s … %s  (%d)", op, cl, n.count)
													   : NkPrintf("%s  (%d)", op, n.count);
					dl.AddText(font->Face(), font->TexId(), {x, ty + asc}, b.CStr(), colPunct);
					x += font->MeasureWidth(b.CStr()) + 8.f * S;
					// [+] ajouter un enfant (au survol).
					if (rowHov) {
						const NkRect ab = {x, y + 3.f * S, 20.f * S, rowH - 6.f * S};
						const bool ah = NkGuiRectContains(ab, mp);
						dl.AddRectFilled(ab, ah ? NkColor{40, 120, 60, 255} : NkColor{40, 46, 54, 255}, 4.f * S);
						dl.AddText(font->Face(), font->TexId(),
								   {ab.x + 10.f * S - font->MeasureWidth("+") * 0.5f, ty + asc}, "+", ctx.theme.text);
						if (ah && in.mouseClicked[0])
							pendAddInto = idx;
						x += 24.f * S;
					}
				} else {
					// Valeur scalaire (éditable) + cycle de type + [–].
					const NkString shown = n.col == 1 ? NkJUnquote(n.val) : n.val;
					const float32 vw = font->MeasureWidth(shown.CStr()) + 6.f * S;
					const NkRect vr = {x, y + 2.f * S, (vw < 40.f * S ? 40.f * S : vw) + 8.f * S, rowH - 4.f * S};
					if (d->editIdx == idx && d->editKind == 1) {
						editorkit::NkOverlayTextField(ctx, dl, font, vr, d->editBuf, (int32)sizeof(d->editBuf), true);
						x += vr.w;
					} else {
						dl.AddText(font->Face(), font->TexId(), {x + 2.f * S, ty + asc}, shown.CStr(),
								   scalarColor(n.col));
						if (NkGuiRectContains(vr, mp) && NkGuiRectContains(clip, mp) && in.mouseClicked[0])
							pendStartEditV = idx;
						x += vr.w;
					}
					if (rowHov) {
						// cycle de type (t).
						const NkRect tb = {x, y + 3.f * S, 20.f * S, rowH - 6.f * S};
						const bool th2 = NkGuiRectContains(tb, mp);
						dl.AddRectFilled(tb, th2 ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}, 4.f * S);
						dl.AddText(font->Face(), font->TexId(),
								   {tb.x + 10.f * S - font->MeasureWidth("t") * 0.5f, ty + asc}, "t", colPunct);
						if (th2 && in.mouseClicked[0])
							pendCycleType = idx;
						x += 24.f * S;
					}
				}
				// [–] supprimer (au survol, sauf racine).
				if (rowHov && idx != 0) {
					const NkRect rb = {x, y + 3.f * S, 20.f * S, rowH - 6.f * S};
					const bool rh = NkGuiRectContains(rb, mp);
					dl.AddRectFilled(rb, rh ? NkColor{150, 60, 60, 255} : NkColor{40, 46, 54, 255}, 4.f * S);
					dl.AddText(font->Face(), font->TexId(),
							   {rb.x + 10.f * S - font->MeasureWidth("–") * 0.5f, ty + asc}, "–", ctx.theme.text);
					if (rh && in.mouseClicked[0])
						pendRemove = idx;
				}
			}
			dl.PopClipRect();

			// Entrée -> commit. Échap -> annule.
			if (d->editIdx >= 0) {
				if (in.KeyPressed(nkgui::NkGuiKey::Enter))
					commitEdit();
				else if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
					d->editIdx = -1;
					d->editKind = 0;
				}
			}

			// Actions (après la boucle : ne pas invalider l'itération / le champ).
			if (pendClickToggle >= 0) {
				d->nodes[pendClickToggle].collapsed = !d->nodes[pendClickToggle].collapsed;
				in.mouseClicked[0] = false;
			} else if (pendStartEditK >= 0) {
				commitEdit();
				d->editIdx = pendStartEditK;
				d->editKind = 2;
				const NkString u = NkJUnquote(d->nodes[pendStartEditK].key);
				int32 i = 0;
				for (const char *c = u.CStr(); *c && i < 1023; ++c)
					d->editBuf[i++] = *c;
				d->editBuf[i] = 0;
				in.mouseClicked[0] = false;
			} else if (pendStartEditV >= 0) {
				commitEdit();
				d->editIdx = pendStartEditV;
				d->editKind = 1;
				const NkJNode &n = d->nodes[pendStartEditV];
				const NkString u = n.col == 1 ? NkJUnquote(n.val) : n.val;
				int32 i = 0;
				for (const char *c = u.CStr(); *c && i < 1023; ++c)
					d->editBuf[i++] = *c;
				d->editBuf[i] = 0;
				in.mouseClicked[0] = false;
			} else if (pendCycleType >= 0) {
				NkJNode &n = d->nodes[pendCycleType];
				// chaîne -> nombre -> true -> false -> null -> chaîne
				if (n.col == 1) {
					n.val = "0";
					n.col = 2;
				} else if (n.col == 2) {
					n.val = "true";
					n.col = 3;
				} else if (n.col == 3 && n.val == "true") {
					n.val = "false";
				} else if (n.col == 3 && n.val == "false") {
					n.val = "null";
				} else {
					n.val = "\"\"";
					n.col = 1;
				}
				dirty = true;
				in.mouseClicked[0] = false;
			} else if (pendAddInto >= 0) {
				commitEdit();
				NkJNode &cont = d->nodes[pendAddInto];
				cont.empty = false;
				cont.collapsed = false;
				const usize insAt = NkJSubtreeEnd(d->nodes, pendAddInto); // après le dernier descendant
				NkJNode nn;
				nn.depth = cont.depth + 1;
				nn.type = 0;
				nn.col = 1;
				nn.val = "\"\"";
				if (cont.type == 1) {
					nn.hasKey = true;
					nn.key = "\"nouveau\"";
				}
				d->nodes.Insert(d->nodes.Begin() + insAt, nn);
				cont.count += 1;
				dirty = true;
				in.mouseClicked[0] = false;
			} else if (pendRemove >= 0) {
				commitEdit();
				const int32 par = NkJParent(d->nodes, pendRemove);
				const usize e = NkJSubtreeEnd(d->nodes, pendRemove);
				d->nodes.Erase(d->nodes.Begin() + (usize)pendRemove, d->nodes.Begin() + e); // supprime le sous-arbre
				if (par >= 0) {
					d->nodes[par].count -= 1;
					if (d->nodes[par].count <= 0)
						d->nodes[par].empty = true;
				}
				dirty = true;
				in.mouseClicked[0] = false;
			}

			// Réécriture du document si l'arbre a changé (et on empêche le reparse : on aligne le hash).
			if (dirty) {
				outNewText = NkJSerialize(d->nodes);
				outChanged = true;
				d->srcLen = outNewText.Size();
				d->srcHash = NkJHash(outNewText.CStr(), outNewText.Size());
			}

			if (needV) {
				const NkRect vtrack = {area.x + area.w - sbW, area.y, sbW, area.h};
				editorkit::NkVScrollbar(ctx, dl, vtrack, extScroll, contentH, viewH, 0x510B0001u, rowH);
			}
			dl.PopClipRect();
		}

	} // namespace nkcode
} // namespace nkentseu
