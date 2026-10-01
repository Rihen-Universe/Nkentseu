// -----------------------------------------------------------------------------
// @File    NkTimelineModel.cpp
// @Brief   Le MODELE de la frise : pistes, cles, selection, temps, annuler.
//          Aucun dessin ici (NkTimelineDraw.cpp) : ce fichier se teste sans
//          peintre ni fenetre (NKEditorKitTest, famille 31).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkTimelineModel.h"

#include <cmath>

namespace nkentseu {
	namespace editorkit {

		const char *NkTimelineInterpName(uint8 interp) {
			static const char *const kNoms[(uint8)NkTimelineInterp::Count] = {
				"Palier", "Lineaire", "Courbe", "Entree", "Sortie", "Entree-sortie", "Rebond", "Elastique", "Recul"};
			return interp < (uint8)NkTimelineInterp::Count ? kNoms[interp] : "?";
		}

		namespace {
			float32 Abs(float32 v) {
				return v < 0.f ? -v : v;
			}

			/// Trie les cles d'une piste par temps (insertion : stable, et les
			/// pistes tiennent en quelques dizaines de cles).
			void Trier(NkTimelineTrack &tr) {
				for (uint32 i = 1; i < (uint32)tr.keys.Size(); ++i) {
					NkTimelineKey k = tr.keys[i];
					uint32 j = i;
					while (j > 0 && tr.keys[j - 1].time > k.time) {
						tr.keys[j] = tr.keys[j - 1];
						--j;
					}
					tr.keys[j] = k;
				}
			}
		} // namespace

		// ── Les pistes ──────────────────────────────────────────────────────────
		NkTimelineTrack *NkTimelineModel::Track(nk_uint64 id) {
			const int32 i = TrackIndex(id);
			return i >= 0 ? &tracks[(uint32)i] : nullptr;
		}

		const NkTimelineTrack *NkTimelineModel::Track(nk_uint64 id) const {
			const int32 i = TrackIndex(id);
			return i >= 0 ? &tracks[(uint32)i] : nullptr;
		}

		int32 NkTimelineModel::TrackIndex(nk_uint64 id) const {
			for (uint32 i = 0; id != 0 && i < (uint32)tracks.Size(); ++i) {
				if (tracks[i].id == id) {
					return (int32)i;
				}
			}
			return -1;
		}

		NkTimelineTrack &NkTimelineModel::AddTrack(nk_uint64 id, const NkString &object, const NkString &property,
												   NkTimelineValueKind kind, uint8 channels) {
			if (NkTimelineTrack *t = Track(id)) {
				return *t;
			}
			PushUndo();
			NkTimelineTrack t;
			t.id = id;
			t.object = object;
			t.property = property;
			t.kind = kind;
			t.channels = channels < 1 ? (uint8)1 : (channels > 4 ? (uint8)4 : channels);
			// Les pistes d'un meme objet se SUIVENT : la nouvelle va apres la
			// derniere de son objet (sinon l'en-tete d'objet se repeterait).
			int32 apres = -1;
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				if (tracks[i].object == object) {
					apres = (int32)i;
				}
			}
			if (apres < 0) {
				tracks.PushBack(t);
			} else {
				tracks.Insert(tracks.Begin() + (apres + 1), t);
			}
			Touch();
			return *Track(id);
		}

		bool NkTimelineModel::RemoveTrack(nk_uint64 id) {
			const int32 i = TrackIndex(id);
			if (i < 0) {
				return false;
			}
			PushUndo();
			tracks.Erase(tracks.Begin() + i);
			if (activeTrack == id) {
				activeTrack = 0;
			}
			Touch();
			return true;
		}

		bool NkTimelineModel::IsCollapsed(const NkString &object) const {
			for (uint32 i = 0; i < (uint32)collapsed.Size(); ++i) {
				if (collapsed[i] == object) {
					return true;
				}
			}
			return false;
		}

		void NkTimelineModel::ToggleCollapsed(const NkString &object) {
			for (uint32 i = 0; i < (uint32)collapsed.Size(); ++i) {
				if (collapsed[i] == object) {
					collapsed.Erase(collapsed.Begin() + i);
					return;
				}
			}
			collapsed.PushBack(object);
		}

		// ── Le temps ────────────────────────────────────────────────────────────
		float32 NkTimelineModel::Snap(float32 t) const {
			if (!snap) {
				return t;
			}
			const float32 f = FrameDuration();
			return std::floor(t / f + 0.5f) * f;
		}

		int32 NkTimelineModel::FrameOf(float32 t) const {
			return (int32)std::floor(t / FrameDuration() + 0.5f);
		}

		void NkTimelineModel::SetCursor(float32 t) {
			if (t < 0.f) {
				t = 0.f;
			}
			if (t > duration) {
				t = duration;
			}
			cursor = t;
		}

		bool NkTimelineModel::Advance(float32 dt) {
			if (!playing || dt <= 0.f) {
				return false;
			}
			float32 t = cursor + dt * speed;
			if (t > duration) {
				if (loop && duration > 1e-6f) {
					t = std::fmod(t, duration);
				} else {
					t = duration;
					playing = false;
				}
			}
			cursor = t;
			return true;
		}

		float32 NkTimelineModel::PreviousKeyTime(float32 t) const {
			const float32 tol = FrameDuration() * 0.5f;
			float32 best = -1e30f;
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					const float32 kt = tracks[i].keys[k].time;
					if (kt < t - tol && kt > best) {
						best = kt;
					}
				}
			}
			return best > -1e29f ? best : t;
		}

		float32 NkTimelineModel::NextKeyTime(float32 t) const {
			const float32 tol = FrameDuration() * 0.5f;
			float32 best = 1e30f;
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					const float32 kt = tracks[i].keys[k].time;
					if (kt > t + tol && kt < best) {
						best = kt;
					}
				}
			}
			return best < 1e29f ? best : t;
		}

		void NkTimelineModel::FrameAll() {
			const float32 d = duration > 1e-3f ? duration : 1.f;
			viewStart = -d * 0.04f;
			viewEnd = d * 1.06f;
			curveAutoFit = true;
		}

		void NkTimelineModel::ZoomTime(float32 pivot, float32 factor) {
			if (factor <= 0.f) {
				return;
			}
			float32 a = pivot - (pivot - viewStart) * factor;
			float32 b = pivot + (viewEnd - pivot) * factor;
			// Bornes : pas plus fin qu'un quart d'image, pas plus large que cent fois
			// la duree -- au-dela, la regle n'a plus rien a dire.
			const float32 minSpan = FrameDuration() * 4.f;
			const float32 maxSpan = (duration > 1.f ? duration : 1.f) * 100.f;
			if (b - a < minSpan || b - a > maxSpan) {
				return;
			}
			viewStart = a;
			viewEnd = b;
		}

		// ── Les cles ────────────────────────────────────────────────────────────
		int32 NkTimelineModel::KeyAt(const NkTimelineTrack &track, float32 t) const {
			const float32 tol = FrameDuration() * 0.5f;
			for (uint32 k = 0; k < (uint32)track.keys.Size(); ++k) {
				if (Abs(track.keys[k].time - t) < tol) {
					return (int32)k;
				}
			}
			return -1;
		}

		int32 NkTimelineModel::SetKey(nk_uint64 trackId, float32 t, const float32 *values, uint8 interp, bool undoable) {
			NkTimelineTrack *tr = Track(trackId);
			if (tr == nullptr) {
				return -1;
			}
			if (undoable) {
				PushUndo();
			}
			t = Snap(t < 0.f ? 0.f : t);
			// L'interpolation par defaut suit le GENRE : une piste Palier ne glisse pas.
			if (interp >= (uint8)NkTimelineInterp::Count) {
				interp = tr->kind == NkTimelineValueKind::Palier ? (uint8)NkTimelineInterp::Palier
																 : (uint8)NkTimelineInterp::Lineaire;
			}
			int32 idx = KeyAt(*tr, t);
			if (idx < 0) {
				NkTimelineKey k;
				k.time = t;
				k.interp = interp;
				uint32 j = 0;
				while (j < (uint32)tr->keys.Size() && tr->keys[j].time < t) {
					++j;
				}
				tr->keys.Insert(tr->keys.Begin() + j, k);
				idx = (int32)j;
			}
			NkTimelineKey &k = tr->keys[(uint32)idx];
			for (uint32 c = 0; c < 4; ++c) {
				k.v[c] = (values != nullptr && c < tr->channels) ? values[c] : 0.f;
			}
			if (t > duration) {
				duration = t;
			}
			Touch();
			return idx;
		}

		bool NkTimelineModel::DeleteSelection() {
			if (SelectionCount() == 0) {
				return false;
			}
			PushUndo();
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				NkVector<NkTimelineKey> &ks = tracks[i].keys;
				for (int32 k = (int32)ks.Size() - 1; k >= 0; --k) {
					if (ks[(uint32)k].selected) {
						ks.Erase(ks.Begin() + k);
					}
				}
			}
			Touch();
			return true;
		}

		void NkTimelineModel::MoveSelection(float32 dt) {
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					NkTimelineKey &key = tracks[i].keys[k];
					if (key.selected) {
						key.time = Snap(key.time + dt);
						if (key.time < 0.f) {
							key.time = 0.f;
						}
					}
				}
				Trier(tracks[i]);
			}
			Touch();
		}

		void NkTimelineModel::SetSelectionInterp(uint8 interp) {
			if (interp >= (uint8)NkTimelineInterp::Count || SelectionCount() == 0) {
				return;
			}
			PushUndo();
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					if (tracks[i].keys[k].selected) {
						tracks[i].keys[k].interp = interp;
					}
				}
			}
			Touch();
		}

		uint8 NkTimelineModel::SelectionInterp() const {
			uint8 r = 255;
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					const NkTimelineKey &key = tracks[i].keys[k];
					if (!key.selected) {
						continue;
					}
					if (r == 255) {
						r = key.interp;
					} else if (r != key.interp) {
						return 255;
					}
				}
			}
			return r;
		}

		// ── La selection ────────────────────────────────────────────────────────
		void NkTimelineModel::SelectKey(nk_uint64 trackId, int32 key, bool additive) {
			if (!additive) {
				ClearSelection();
			}
			NkTimelineTrack *tr = Track(trackId);
			if (tr != nullptr && key >= 0 && key < (int32)tr->keys.Size()) {
				NkTimelineKey &k = tr->keys[(uint32)key];
				k.selected = additive ? !k.selected : true;
			}
		}

		void NkTimelineModel::ClearSelection() {
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					tracks[i].keys[k].selected = false;
				}
			}
		}

		void NkTimelineModel::SelectAll() {
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					tracks[i].keys[k].selected = true;
				}
			}
		}

		void NkTimelineModel::SelectInBox(int32 trackFirst, int32 trackLast, float32 t0, float32 t1, bool additive) {
			if (!additive) {
				ClearSelection();
			}
			if (t0 > t1) {
				const float32 x = t0;
				t0 = t1;
				t1 = x;
			}
			for (int32 i = trackFirst < 0 ? 0 : trackFirst; i <= trackLast && i < (int32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[(uint32)i].keys.Size(); ++k) {
					NkTimelineKey &key = tracks[(uint32)i].keys[k];
					if (key.time >= t0 && key.time <= t1) {
						key.selected = true;
					}
				}
			}
		}

		uint32 NkTimelineModel::SelectionCount() const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					n += tracks[i].keys[k].selected ? 1u : 0u;
				}
			}
			return n;
		}

		// ── L'evaluation de repli ───────────────────────────────────────────────
		bool NkTimelineModel::EvaluateFallback(const NkTimelineTrack &tr, float32 t, float32 out[4]) const {
			const uint32 n = (uint32)tr.keys.Size();
			if (n == 0) {
				return false;
			}
			const NkTimelineKey *a = &tr.keys[0];
			const NkTimelineKey *b = a;
			float32 u = 0.f;
			if (t <= tr.keys[0].time) {
				b = a;
			} else if (t >= tr.keys[n - 1].time) {
				a = &tr.keys[n - 1];
				b = a;
			} else {
				uint32 hi = 1;
				while (hi < n && tr.keys[hi].time <= t) {
					++hi;
				}
				a = &tr.keys[hi - 1];
				b = &tr.keys[hi];
				const float32 d = b->time - a->time;
				u = d > 1e-6f ? (t - a->time) / d : 0.f;
				if (tr.kind == NkTimelineValueKind::Palier || a->interp == (uint8)NkTimelineInterp::Palier) {
					u = 0.f;
				}
			}
			for (uint32 c = 0; c < 4; ++c) {
				out[c] = a->v[c] + (b->v[c] - a->v[c]) * u;
			}
			return true;
		}

		// ── Annuler / Refaire ───────────────────────────────────────────────────
		void NkTimelineModel::PushUndo() {
			NkTimelineSnapshot s;
			s.tracks = tracks;
			s.duration = duration;
			undoStack.PushBack(s);
			while ((uint32)undoStack.Size() > undoLimit && !undoStack.Empty()) {
				undoStack.Erase(undoStack.Begin());
			}
			redoStack.Clear();
		}

		bool NkTimelineModel::Undo() {
			if (undoStack.Empty()) {
				return false;
			}
			NkTimelineSnapshot cur;
			cur.tracks = tracks;
			cur.duration = duration;
			redoStack.PushBack(cur);
			const NkTimelineSnapshot &s = undoStack[undoStack.Size() - 1];
			tracks = s.tracks;
			duration = s.duration;
			undoStack.PopBack();
			if (TrackIndex(activeTrack) < 0) {
				activeTrack = 0;
			}
			Touch();
			return true;
		}

		bool NkTimelineModel::Redo() {
			if (redoStack.Empty()) {
				return false;
			}
			NkTimelineSnapshot cur;
			cur.tracks = tracks;
			cur.duration = duration;
			undoStack.PushBack(cur);
			const NkTimelineSnapshot &s = redoStack[redoStack.Size() - 1];
			tracks = s.tracks;
			duration = s.duration;
			redoStack.PopBack();
			if (TrackIndex(activeTrack) < 0) {
				activeTrack = 0;
			}
			Touch();
			return true;
		}

		void NkTimelineModel::Normalize() {
			const float32 tol = FrameDuration() * 0.5f;
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				NkTimelineTrack &tr = tracks[i];
				Trier(tr);
				// Deux cles sur la meme image : la CHOISIE (celle qu'on vient de
				// deplacer) l'emporte, l'autre est recouverte -- le geste d'Unreal.
				for (int32 k = (int32)tr.keys.Size() - 2; k >= 0; --k) {
					NkTimelineKey &a = tr.keys[(uint32)k];
					NkTimelineKey &b = tr.keys[(uint32)k + 1];
					if (Abs(a.time - b.time) < tol) {
						const int32 perdante = (a.selected && !b.selected) ? k + 1 : k;
						tr.keys.Erase(tr.keys.Begin() + perdante);
					}
				}
			}
			Touch();
		}

	} // namespace editorkit
} // namespace nkentseu
