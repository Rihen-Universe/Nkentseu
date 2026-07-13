#pragma once
// =============================================================================
// NkJsonView.h — Éditeur d'arbre JSON de NKCode (aperçu ÉDITABLE).
//   Arbre à plat repliable + coloré. Clic sur un nœud -> POPUP à 2 champs
//   (Clé + Valeur, monoligne) avec sélecteur de type ; [+]/[–] ajout/suppr de
//   nœuds. Chaque édition réécrit le .json (via callback -> Checkpoint+SetText,
//   donc undo/redo Ctrl+Z câblé côté Panels). L'arbre reste la source de vérité
//   pendant l'édition (pas de reparse qui perdrait le pliage).
// @Author  Rihen
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKEditorKit/NkEditorTextField.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkFormat.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::nkgui;

		struct NkJNode {
				int32 depth = 0;
				int32 type = 0;			// 0 = scalaire, 1 = objet {}, 2 = tableau []
				int32 count = 0;
				NkString key;			// clé AVEC guillemets ou ""
				NkString val;			// scalaire brut (chaîne AVEC guillemets) ; conteneurs : ""
				int32 col = 0;			// 0 autre, 1 chaîne, 2 nombre, 3 mot-clé
				bool hasKey = false;
				bool collapsed = false;
				bool empty = false;
		};

		inline NkString NkJUnquote(const NkString &s) {
			const char *p = s.CStr();
			usize n = s.Size();
			if (n >= 2 && p[0] == '"' && p[n - 1] == '"') {
				NkString o;
				for (usize i = 1; i + 1 < n; ++i) {
					if (p[i] == '\\' && i + 2 < n) {
						const char c = p[i + 1];
						o += (c == 'n') ? '\n' : (c == 't') ? '\t' : c;
						++i;
						continue;
					}
					o += p[i];
				}
				return o;
			}
			return s;
		}
		inline NkString NkJQuote(const char *raw) {
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

		// ── Parseur JSON -> nœuds à plat ──────────────────────────────────────────
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
						const usize idx = out->Size();
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
							(*out)[idx].empty = true;
							return;
						}
						while (p < end && ok) {
							Skip();
							NkString mk;
							bool mh = false;
							if (obj) {
								mk = ReadString();
								mh = true;
								Skip();
								if (p < end && *p == ':')
									++p;
							}
							ParseValue(depth + 1, mk, mh);
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
						(*out)[idx].count = count;
						return;
					}
					NkJNode n;
					n.depth = depth;
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
		inline usize NkJSubtreeEnd(const NkVector<NkJNode> &nodes, usize i) {
			const int32 d = nodes[i].depth;
			usize j = i + 1;
			while (j < nodes.Size() && nodes[j].depth > d)
				++j;
			return j;
		}
		inline int32 NkJParent(const NkVector<NkJNode> &nodes, usize i) {
			const int32 d = nodes[i].depth;
			for (int32 j = (int32)i - 1; j >= 0; --j)
				if (nodes[j].depth == d - 1)
					return j;
			return -1;
		}

		struct NkJsonTree {
				NkVector<NkJNode> nodes;
				usize srcLen = 0;
				uint32 srcHash = 0;
				bool valid = false;
				int32 popupIdx = -1; // nœud édité via popup
				int32 popField = 0;	 // 0 = champ Clé, 1 = champ Valeur
				int32 popCol = 1;	 // type de la valeur en cours : 1 str, 2 num, 3 kw
				char popKey[512] = {};
				char popVal[1024] = {};
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
			if (d->srcLen != len || d->srcHash != h) {
				d->nodes.Clear();
				d->popupIdx = -1;
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

		// Petit bouton texte. Retourne true si cliqué.
		inline bool NkJBtn(NkGuiContext &ctx, NkGuiDrawList &dl, const NkGuiFont *f, const NkRect &r, const char *lbl,
						   bool on) {
			const NkVec2 mp = ctx.input.mousePos;
			const bool hov = NkGuiRectContains(r, mp);
			dl.AddRectFilled(r, on ? NkColor{15, 115, 213, 255} : (hov ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}),
							 4.f);
			if (f && f->Valid())
				dl.AddText(f->Face(), f->TexId(),
						   {r.x + (r.w - f->MeasureWidth(lbl)) * 0.5f, r.y + (r.h - f->LineHeight()) * 0.5f + f->Ascent()},
						   lbl, on ? NkColor{255, 255, 255, 255} : ctx.theme.text);
			return hov && ctx.input.mouseClicked[0];
		}

		// ── Rendu + édition ; outChanged/outNewText = le JSON à réécrire (Panels fait Checkpoint+SetText). ──
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
			const bool popOpen = d->popupIdx >= 0 && d->popupIdx < (int32)d->nodes.Size();
			const bool treeClick = in.mouseClicked[0] && !popOpen; // tree interactions gelées si popup ouvert
			const float32 sbW = 14.f * S, padX = 12.f * S, padY = 8.f * S, indentW = 16.f * S;
			const NkColor colKey{88, 166, 255, 255}, colStr{126, 231, 135, 255}, colNum{247, 154, 40, 255},
				colKw{210, 168, 255, 255}, colPunct{120, 128, 138, 255}, colTri{140, 148, 158, 255};
			auto scalarColor = [&](int32 c) -> NkColor {
				return c == 1 ? colStr : c == 2 ? colNum : c == 3 ? colKw : NkColor{200, 208, 216, 255};
			};

			bool dirty = false;
			int32 pendToggle = -1, pendEdit = -1, pendEditField = 1, pendAdd = -1, pendRemove = -1;

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
			extScroll = extScroll < 0.f ? 0.f : (extScroll > maxScroll ? maxScroll : extScroll);
			if (NkGuiRectContains(area, mp) && in.wheel != 0.f && !popOpen) {
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
				const bool rowHov = !popOpen && NkGuiRectContains({area.x, y, innerW, rowH}, mp) &&
									NkGuiRectContains(clip, mp);
				if (rowHov)
					dl.AddRectFilled({area.x, y, innerW, rowH}, NkColor{255, 255, 255, 12});
				float32 x = area.x + padX + (float32)n.depth * indentW;
				const float32 ty = y + (rowH - lh) * 0.5f;

				if (container && !n.empty) {
					const float32 cx = x + 4.f * S, cy = y + rowH * 0.5f;
					if (n.collapsed)
						dl.AddTriangleFilled({cx, cy - 4.f * S}, {cx, cy + 4.f * S}, {cx + 5.f * S, cy}, colTri);
					else
						dl.AddTriangleFilled({cx - 1.f * S, cy - 2.f * S}, {cx + 8.f * S, cy - 2.f * S},
											 {cx + 3.5f * S, cy + 4.f * S}, colTri);
					if (treeClick && NkGuiRectContains({x, y, 16.f * S, rowH}, mp) && NkGuiRectContains(clip, mp))
						pendToggle = idx;
				}
				x += indentW;

				// Clé (cliquable -> popup, champ Clé).
				if (n.hasKey) {
					const NkString ku = NkJUnquote(n.key);
					const float32 kw = font->MeasureWidth(ku.CStr());
					dl.AddText(font->Face(), font->TexId(), {x, ty + asc}, ku.CStr(), colKey);
					const NkRect kr = {x, y, kw + 6.f * S, rowH};
					if (treeClick && NkGuiRectContains(kr, mp) && NkGuiRectContains(clip, mp)) {
						pendEdit = idx;
						pendEditField = 0;
					}
					x += kw + 4.f * S;
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
					if (rowHov) {
						const NkRect ab = {x, y + 3.f * S, 20.f * S, rowH - 6.f * S};
						if (NkJBtn(ctx, dl, font, ab, "+", false))
							pendAdd = idx;
						x += 24.f * S;
					}
				} else {
					const NkString shown = n.col == 1 ? NkJUnquote(n.val) : n.val;
					const float32 vw = font->MeasureWidth(shown.CStr());
					dl.AddText(font->Face(), font->TexId(), {x, ty + asc}, shown.CStr(), scalarColor(n.col));
					const NkRect vr = {x, y, (vw < 30.f * S ? 30.f * S : vw) + 6.f * S, rowH};
					if (treeClick && NkGuiRectContains(vr, mp) && NkGuiRectContains(clip, mp)) {
						pendEdit = idx;
						pendEditField = 1;
					}
					x += vr.w;
				}
				if (rowHov && idx != 0) {
					const NkRect rb = {x, y + 3.f * S, 20.f * S, rowH - 6.f * S};
					if (NkJBtn(ctx, dl, font, rb, "–", false))
						pendRemove = idx;
				}
			}
			dl.PopClipRect();

			// ── Actions arbre (hors popup) ──
			if (pendToggle >= 0) {
				d->nodes[pendToggle].collapsed = !d->nodes[pendToggle].collapsed;
				in.mouseClicked[0] = false;
			} else if (pendEdit >= 0) {
				NkJNode &n = d->nodes[pendEdit];
				d->popupIdx = pendEdit;
				d->popField = n.hasKey ? pendEditField : 1;
				d->popCol = n.type == 0 ? (n.col == 0 ? 1 : n.col) : 1;
				const NkString ku = NkJUnquote(n.key), vu = n.col == 1 ? NkJUnquote(n.val) : n.val;
				int32 i = 0;
				for (const char *c = ku.CStr(); *c && i < 511; ++c)
					d->popKey[i++] = *c;
				d->popKey[i] = 0;
				i = 0;
				for (const char *c = vu.CStr(); *c && i < 1023; ++c)
					d->popVal[i++] = *c;
				d->popVal[i] = 0;
				in.mouseClicked[0] = false;
			} else if (pendAdd >= 0) {
				NkJNode &cont = d->nodes[pendAdd];
				cont.empty = false;
				cont.collapsed = false;
				const usize insAt = NkJSubtreeEnd(d->nodes, pendAdd);
				NkJNode nn;
				nn.depth = cont.depth + 1;
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
				const int32 par = NkJParent(d->nodes, pendRemove);
				const usize e = NkJSubtreeEnd(d->nodes, pendRemove);
				d->nodes.Erase(d->nodes.Begin() + (usize)pendRemove, d->nodes.Begin() + e);
				if (par >= 0) {
					d->nodes[par].count -= 1;
					if (d->nodes[par].count <= 0)
						d->nodes[par].empty = true;
				}
				dirty = true;
				in.mouseClicked[0] = false;
			}

			// ── POPUP d'édition (2 champs + type) ──
			if (d->popupIdx >= 0 && d->popupIdx < (int32)d->nodes.Size()) {
				NkJNode &n = d->nodes[d->popupIdx];
				const bool hasKey = n.hasKey;
				const float32 pw = 380.f * S;
				const float32 ph = (hasKey ? 176.f : 138.f) * S;
				NkRect pop = {area.x + (area.w - pw) * 0.5f, area.y + 34.f * S, pw, ph};
				if (pop.x < area.x + 8.f * S)
					pop.x = area.x + 8.f * S;
				// Voile + boîte.
				dl.AddRectFilled(area, NkColor{0, 0, 0, 90});
				dl.AddRectFilled(pop, NkColor{30, 33, 40, 255}, 8.f * S);
				dl.AddRect(pop, NkColor{70, 78, 90, 255}, 1.f);
				float32 py = pop.y + 12.f * S;
				const float32 fx = pop.x + 14.f * S, fw = pw - 28.f * S, fh = 26.f * S;

				auto commit = [&]() {
					if (hasKey)
						n.key = NkJQuote(d->popKey);
					if (d->popCol == 1)
						n.val = NkJQuote(d->popVal);
					else
						n.val = NkString(d->popVal);
					n.col = d->popCol;
					n.type = 0;
					dirty = true;
					d->popupIdx = -1;
				};

				// Tab -> bascule Clé/Valeur.
				if (hasKey && in.KeyPressed(nkgui::NkGuiKey::Tab))
					d->popField ^= 1;

				if (hasKey) {
					dl.AddText(font->Face(), font->TexId(), {fx, py + asc}, "Clé", colPunct);
					const NkRect kr = {fx, py + lh + 2.f * S, fw, fh};
					if (in.mouseClicked[0] && NkGuiRectContains(kr, mp))
						d->popField = 0;
					editorkit::NkOverlayTextField(ctx, dl, font, kr, d->popKey, (int32)sizeof(d->popKey),
												  d->popField == 0);
					py += lh + fh + 8.f * S;
				}
				dl.AddText(font->Face(), font->TexId(), {fx, py + asc}, "Valeur", colPunct);
				const NkRect vr = {fx, py + lh + 2.f * S, fw, fh};
				if (in.mouseClicked[0] && NkGuiRectContains(vr, mp))
					d->popField = 1;
				editorkit::NkOverlayTextField(ctx, dl, font, vr, d->popVal, (int32)sizeof(d->popVal),
											  d->popField == 1 || !hasKey);
				py += lh + fh + 8.f * S;

				// Sélecteur de type.
				const char *tlabs[5] = {"\"abc\"", "123", "true", "false", "null"};
				const int32 tcols[5] = {1, 2, 3, 3, 3};
				const char *tvals[5] = {nullptr, nullptr, "true", "false", "null"};
				float32 tx = fx;
				const float32 tbw = (fw - 4.f * 4.f * S) / 5.f;
				for (int32 t = 0; t < 5; ++t) {
					const bool on = (d->popCol == tcols[t]) &&
									(tvals[t] == nullptr || NkString(d->popVal) == NkString(tvals[t]) ||
									 (t < 2 && d->popCol == tcols[t]));
					const NkRect tb = {tx, py, tbw, fh};
					if (NkJBtn(ctx, dl, font, tb, tlabs[t], on && (t >= 2 ? NkString(d->popVal) == tvals[t] : true))) {
						d->popCol = tcols[t];
						if (tvals[t]) {
							int32 i = 0;
							for (const char *c = tvals[t]; *c && i < 1023; ++c)
								d->popVal[i++] = *c;
							d->popVal[i] = 0;
						}
					}
					tx += tbw + 4.f * S;
				}
				py += fh + 10.f * S;

				// Boutons OK / Annuler.
				const float32 bw = 90.f * S;
				const NkRect okB = {pop.x + pw - bw * 2.f - 22.f * S, py, bw, fh};
				const NkRect caB = {pop.x + pw - bw - 14.f * S, py, bw, fh};
				if (NkJBtn(ctx, dl, font, okB, "Valider", true))
					commit();
				if (NkJBtn(ctx, dl, font, caB, "Annuler", false))
					d->popupIdx = -1;

				// Entrée = valider ; Échap = annuler ; clic hors boîte = annuler.
				if (in.KeyPressed(nkgui::NkGuiKey::Enter))
					commit();
				else if (in.KeyPressed(nkgui::NkGuiKey::Escape))
					d->popupIdx = -1;
				else if (in.mouseClicked[0] && !NkGuiRectContains(pop, mp))
					d->popupIdx = -1;
				in.mouseClicked[0] = false; // le popup capture le clic (modal)
			}

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
