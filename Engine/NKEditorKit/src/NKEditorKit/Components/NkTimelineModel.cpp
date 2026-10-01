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
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		const char *NkTimelineInterpName(uint8 interp) {
			static const char *const kNoms[(uint8)NkTimelineInterp::Count] = {
				"Palier", "Lineaire", "Courbe", "Entree", "Sortie", "Entree-sortie", "Rebond", "Elastique", "Recul"};
			return interp < (uint8)NkTimelineInterp::Count ? kNoms[interp] : "?";
		}

		const char *NkTimelineTangentName(uint8 tangent) {
			static const char *const kNoms[(uint8)NkTimelineTangent::Count] = {"Auto", "Unifiee", "Cassee", "Plate"};
			return tangent < (uint8)NkTimelineTangent::Count ? kNoms[tangent] : "?";
		}

		namespace {
			float32 Abs(float32 v) {
				return v < 0.f ? -v : v;
			}

			/// Les DOUCEURS de NKAnima (NkAnimationTrack::EaseAlpha), recopiees pour
			/// que le repli du kit trace la meme courbe que le jeu quand l'hote ne
			/// donne pas son `evaluate`.
			float32 Douceur(float32 a, uint8 m) {
				switch ((NkTimelineInterp)m) {
					case NkTimelineInterp::Palier:
						return 0.f;
					case NkTimelineInterp::Entree:
						return a * a;
					case NkTimelineInterp::Sortie:
						return 1.f - (1.f - a) * (1.f - a);
					case NkTimelineInterp::EntreeSortie:
						return a < 0.5f ? 2.f * a * a : 1.f - (-2.f * a + 2.f) * (-2.f * a + 2.f) * 0.5f;
					case NkTimelineInterp::Rebond: {
						float32 n = 1.f - a;
						const float32 d = 2.75f, b = 7.5625f;
						if (n < 1.f / d) {
							return 1.f - b * n * n;
						}
						if (n < 2.f / d) {
							n -= 1.5f / d;
							return 1.f - (b * n * n + 0.75f);
						}
						if (n < 2.5f / d) {
							n -= 2.25f / d;
							return 1.f - (b * n * n + 0.9375f);
						}
						n -= 2.625f / d;
						return 1.f - (b * n * n + 0.984375f);
					}
					case NkTimelineInterp::Elastique: {
						const float32 c4 = (2.f * 3.14159f) / 3.f;
						if (a <= 0.f) {
							return 0.f;
						}
						if (a >= 1.f) {
							return 1.f;
						}
						return std::pow(2.f, -10.f * a) * std::sin((a * 10.f - 0.75f) * c4) + 1.f;
					}
					case NkTimelineInterp::Recul: {
						const float32 c1 = 1.70158f, c3 = c1 + 1.f;
						return 1.f + c3 * (a - 1.f) * (a - 1.f) * (a - 1.f) + c1 * (a - 1.f) * (a - 1.f);
					}
					default:
						return a;
				}
			}

			void TrierClips(NkTimelineTrack &tr) {
				for (uint32 i = 1; i < (uint32)tr.clips.Size(); ++i) {
					NkTimelineClip c = tr.clips[i];
					uint32 j = i;
					while (j > 0 && tr.clips[j - 1].start > c.start) {
						tr.clips[j] = tr.clips[j - 1];
						--j;
					}
					tr.clips[j] = c;
				}
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

		// ── (01/10 soir) L'arbre, muet / solo / verrou ─────────────────────────
		NkString NkTimelineModel::ParentObject(const NkString &object) {
			if (object.Empty()) {
				return NkString();
			}
			const char *s = object.CStr();
			int32 dernier = -1;
			for (int32 i = 0; s[i] != '\0'; ++i) {
				if (s[i] == '/') {
					dernier = i;
				}
			}
			return dernier < 0 ? NkString() : NkString(s, (usize)dernier);
		}

		uint32 NkTimelineModel::ObjectDepth(const NkString &object) {
			if (object.Empty()) {
				return 0;
			}
			uint32 n = 1;
			for (const char *s = object.CStr(); *s != '\0'; ++s) {
				n += *s == '/' ? 1u : 0u;
			}
			return n;
		}

		bool NkTimelineModel::IsUnderObject(const NkString &object, const NkString &ancestor) {
			if (ancestor.Empty() || object == ancestor) {
				return true;
			}
			const usize n = ancestor.Length();
			return object.Length() > n && object.CStr()[n] == '/' && std::strncmp(object.CStr(), ancestor.CStr(), n) == 0;
		}

		bool NkTimelineModel::IsHidden(const NkString &object) const {
			for (uint32 i = 0; i < (uint32)collapsed.Size(); ++i) {
				if (IsUnderObject(object, collapsed[i])) {
					return true;
				}
			}
			return false;
		}

		bool NkTimelineModel::AnySolo() const {
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				if (tracks[i].solo) {
					return true;
				}
			}
			return false;
		}

		bool NkTimelineModel::TrackActive(const NkTimelineTrack &track) const {
			if (track.muted) {
				return false;
			}
			return !AnySolo() || track.solo;
		}

		void NkTimelineModel::ToggleMute(nk_uint64 id) {
			if (NkTimelineTrack *t = Track(id)) {
				PushUndo();
				t->muted = !t->muted;
				Touch();
			}
		}

		void NkTimelineModel::ToggleSolo(nk_uint64 id) {
			if (NkTimelineTrack *t = Track(id)) {
				PushUndo();
				t->solo = !t->solo;
				Touch();
			}
		}

		void NkTimelineModel::ToggleLock(nk_uint64 id) {
			if (NkTimelineTrack *t = Track(id)) {
				PushUndo();
				t->locked = !t->locked;
				if (t->locked) {
					for (uint32 k = 0; k < (uint32)t->keys.Size(); ++k) {
						t->keys[k].selected = false;
					}
					for (uint32 k = 0; k < (uint32)t->clips.Size(); ++k) {
						t->clips[k].selected = false;
					}
				}
				Touch();
			}
		}

		void NkTimelineModel::SetObjectFlag(const NkString &object, uint8 flag, bool on) {
			PushUndo();
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				NkTimelineTrack &t = tracks[i];
				if (!IsUnderObject(t.object, object)) {
					continue;
				}
				bool &champ = flag == 0 ? t.muted : (flag == 1 ? t.solo : t.locked);
				champ = on;
			}
			Touch();
		}

		bool NkTimelineModel::ObjectFlag(const NkString &object, uint8 flag) const {
			bool une = false;
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				const NkTimelineTrack &t = tracks[i];
				if (!IsUnderObject(t.object, object)) {
					continue;
				}
				une = true;
				const bool champ = flag == 0 ? t.muted : (flag == 1 ? t.solo : t.locked);
				if (!champ) {
					return false;
				}
			}
			return une;
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
			// (01/10 soir) La lecture tient dans la PLAGE DE LECTURE (toute la duree
			// tant qu'on ne l'a pas reglee : le comportement d'avant).
			const float32 debut = PlayStart();
			const float32 fin = PlayEnd();
			float32 t = cursor + dt * speed;
			if (t > fin) {
				if (loop && fin - debut > 1e-6f) {
					t = debut + std::fmod(t - debut, fin - debut);
				} else {
					t = fin;
					playing = false;
				}
			}
			cursor = t;
			return true;
		}

		float32 NkTimelineModel::PlayStart() const {
			const float32 d = rangeStart < 0.f ? 0.f : rangeStart;
			return d < duration ? d : 0.f;
		}

		float32 NkTimelineModel::PlayEnd() const {
			if (rangeEnd < 0.f || rangeEnd > duration) {
				return duration;
			}
			return rangeEnd > PlayStart() ? rangeEnd : duration;
		}

		void NkTimelineModel::SetPlayRange(float32 start, float32 end) {
			PushUndo();
			start = start < 0.f ? 0.f : (start > duration ? duration : start);
			end = end < 0.f ? 0.f : (end > duration ? duration : end);
			if (end < start) {
				const float32 x = start;
				start = end;
				end = x;
			}
			rangeStart = start;
			rangeEnd = end;
			Touch();
		}

		void NkTimelineModel::StepFrame(int32 frames) {
			playing = false;
			SetCursor((float32)(FrameOf(cursor) + frames) * FrameDuration());
		}

		void NkTimelineModel::ZoomTimeSmooth(float32 pivot, float32 factor) {
			if (!viewGlide) {
				viewTargetStart = viewStart;
				viewTargetEnd = viewEnd;
			}
			if (factor <= 0.f) {
				return;
			}
			const float32 a = pivot - (pivot - viewTargetStart) * factor;
			const float32 b = pivot + (viewTargetEnd - pivot) * factor;
			const float32 minSpan = FrameDuration() * 4.f;
			const float32 maxSpan = (duration > 1.f ? duration : 1.f) * 100.f;
			if (b - a < minSpan || b - a > maxSpan) {
				return;
			}
			viewTargetStart = a;
			viewTargetEnd = b;
			viewGlide = true;
		}

		bool NkTimelineModel::GlideStep(float32 k) {
			if (!viewGlide) {
				return false;
			}
			k = k <= 0.f ? 1.f : (k > 1.f ? 1.f : k);
			viewStart += (viewTargetStart - viewStart) * k;
			viewEnd += (viewTargetEnd - viewEnd) * k;
			const float32 eps = (viewTargetEnd - viewTargetStart) * 1e-3f;
			if (Abs(viewTargetStart - viewStart) <= eps && Abs(viewTargetEnd - viewEnd) <= eps) {
				viewStart = viewTargetStart;
				viewEnd = viewTargetEnd;
				viewGlide = false;
			}
			return true;
		}

		void NkTimelineModel::FollowCursor() {
			const float32 span = viewEnd - viewStart;
			if (span <= 1e-6f) {
				return;
			}
			// La page suivante quand le curseur sort a droite ; retour au debut quand
			// la boucle le ramene a gauche -- le suivi de page d'UE5.
			if (cursor > viewEnd || cursor < viewStart) {
				viewStart = cursor - span * 0.05f;
				viewEnd = viewStart + span;
				viewGlide = false;
			}
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
			if (tr == nullptr || tr->kind == NkTimelineValueKind::Clips) {
				return -1; // (01/10 soir) une piste de clips n'a pas de cles
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
			uint32 marques = 0;
			for (uint32 i = 0; i < (uint32)markers.Size(); ++i) {
				marques += markers[i].selected ? 1u : 0u;
			}
			if (SelectionCount() == 0 && ClipSelectionCount() == 0 && marques == 0) {
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
				// (01/10 soir) les clips choisis partent aussi...
				NkVector<NkTimelineClip> &cs = tracks[i].clips;
				for (int32 k = (int32)cs.Size() - 1; k >= 0; --k) {
					if (cs[(uint32)k].selected) {
						if (activeClip == cs[(uint32)k].id) {
							activeClip = 0;
						}
						cs.Erase(cs.Begin() + k);
					}
				}
			}
			// ...et les marqueurs choisis.
			for (int32 k = (int32)markers.Size() - 1; k >= 0; --k) {
				if (markers[(uint32)k].selected) {
					markers.Erase(markers.Begin() + k);
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
			if (tr != nullptr && !tr->locked && key >= 0 && key < (int32)tr->keys.Size()) {
				NkTimelineKey &k = tr->keys[(uint32)key];
				k.selected = additive ? !k.selected : true;
			}
		}

		void NkTimelineModel::ClearSelection() {
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					tracks[i].keys[k].selected = false;
				}
				// (01/10 soir) les clips et les marqueurs aussi.
				for (uint32 k = 0; k < (uint32)tracks[i].clips.Size(); ++k) {
					tracks[i].clips[k].selected = false;
				}
			}
			for (uint32 k = 0; k < (uint32)markers.Size(); ++k) {
				markers[k].selected = false;
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
				if (tracks[(uint32)i].locked) {
					continue; // (01/10 soir) une piste verrouillee ne se choisit pas
				}
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
				} else if (a->interp == (uint8)NkTimelineInterp::Courbe) {
					// (01/10 soir) La COURBE : Hermite cubique sur les pentes des deux
					// cles (posees, plates ou automatiques) -- NkPropertyTrack::Evaluate.
					const uint32 ia = hi - 1, ib = hi;
					const float32 u2 = u * u, u3 = u2 * u;
					const float32 h00 = 2.f * u3 - 3.f * u2 + 1.f, h10 = u3 - 2.f * u2 + u;
					const float32 h01 = -2.f * u3 + 3.f * u2, h11 = u3 - u2;
					for (uint32 c = 0; c < 4; ++c) {
						float32 inA, outA, inB, outB;
						KeySlopes(tr, ia, c, inA, outA);
						KeySlopes(tr, ib, c, inB, outB);
						out[c] = h00 * a->v[c] + h10 * d * outA + h01 * b->v[c] + h11 * d * inB;
					}
					return true;
				} else {
					// Les douceurs de NKAnima (le repli trace ce que le jeu jouera).
					u = Douceur(u, a->interp);
				}
			}
			for (uint32 c = 0; c < 4; ++c) {
				out[c] = a->v[c] + (b->v[c] - a->v[c]) * u;
			}
			return true;
		}

		// ── (01/10 soir) Les tangentes ──────────────────────────────────────────
		void NkTimelineModel::KeySlopes(const NkTimelineTrack &tr, uint32 k, uint32 ch, float32 &in, float32 &out) const {
			in = out = 0.f;
			const uint32 n = (uint32)tr.keys.Size();
			if (k >= n || ch >= 4) {
				return;
			}
			const NkTimelineKey &key = tr.keys[k];
			switch ((NkTimelineTangent)key.tangent) {
				case NkTimelineTangent::Unifiee:
					in = out = key.tanOut[ch];
					return;
				case NkTimelineTangent::Cassee:
					in = key.tanIn[ch];
					out = key.tanOut[ch];
					return;
				case NkTimelineTangent::Plate:
					return;
				default:
					break;
			}
			// AUTO, « bornee » comme l'Auto Clamped de Blender : Catmull-Rom sur
			// les temps, plate aux extremites et sur un sommet ou un creux (la
			// courbe ne deborde jamais de ses cles). NKAnima calcule la meme.
			if (k == 0 || k + 1 >= n) {
				return;
			}
			const NkTimelineKey &p = tr.keys[k - 1];
			const NkTimelineKey &s = tr.keys[k + 1];
			const float32 dp = key.v[ch] - p.v[ch], ds = s.v[ch] - key.v[ch];
			if (dp * ds <= 0.f) {
				return;
			}
			const float32 dt = s.time - p.time;
			in = out = dt > 1e-6f ? (s.v[ch] - p.v[ch]) / dt : 0.f;
		}

		void NkTimelineModel::SetSelectionTangent(uint8 tangent) {
			if (tangent >= (uint8)NkTimelineTangent::Count || SelectionCount() == 0) {
				return;
			}
			PushUndo();
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				NkTimelineTrack &tr = tracks[i];
				for (uint32 k = 0; k < (uint32)tr.keys.Size(); ++k) {
					NkTimelineKey &key = tr.keys[k];
					if (!key.selected) {
						continue;
					}
					// Passer a la main GARDE la forme : les pentes de depart sont les
					// effectives (le geste d'UE5 : « User » ne fait pas sauter la courbe).
					if (tangent == (uint8)NkTimelineTangent::Unifiee || tangent == (uint8)NkTimelineTangent::Cassee) {
						for (uint32 c = 0; c < 4; ++c) {
							float32 a, b;
							KeySlopes(tr, k, c, a, b);
							key.tanIn[c] = a;
							key.tanOut[c] = tangent == (uint8)NkTimelineTangent::Unifiee ? a : b;
						}
					}
					key.tangent = tangent;
					// Une tangente n'a de sens que sur une courbe.
					if (key.interp != (uint8)NkTimelineInterp::Courbe && tr.kind != NkTimelineValueKind::Palier) {
						key.interp = (uint8)NkTimelineInterp::Courbe;
					}
				}
			}
			Touch();
		}

		uint8 NkTimelineModel::SelectionTangent() const {
			uint8 r = 255;
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					const NkTimelineKey &key = tracks[i].keys[k];
					if (!key.selected) {
						continue;
					}
					if (r == 255) {
						r = key.tangent;
					} else if (r != key.tangent) {
						return 255;
					}
				}
			}
			return r;
		}

		// ── (01/10 soir) Echelle, copier / coller ───────────────────────────────
		void NkTimelineModel::ScaleSelection(float32 pivot, float32 factor) {
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					NkTimelineKey &key = tracks[i].keys[k];
					if (key.selected) {
						const float32 t = Snap(pivot + (key.time - pivot) * factor);
						key.time = t < 0.f ? 0.f : t;
					}
				}
				Trier(tracks[i]);
			}
			Touch();
		}

		bool NkTimelineModel::SelectionTimeRange(float32 &t0, float32 &t1) const {
			t0 = 1e30f;
			t1 = -1e30f;
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					const NkTimelineKey &key = tracks[i].keys[k];
					if (key.selected) {
						t0 = key.time < t0 ? key.time : t0;
						t1 = key.time > t1 ? key.time : t1;
					}
				}
			}
			return t0 <= t1;
		}

		uint32 NkTimelineModel::CopySelection() {
			float32 t0, t1;
			if (!SelectionTimeRange(t0, t1)) {
				return 0;
			}
			clipboard.Clear();
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].keys.Size(); ++k) {
					const NkTimelineKey &key = tracks[i].keys[k];
					if (key.selected) {
						NkTimelineClipboardKey c;
						c.track = tracks[i].id;
						c.key = key;
						c.key.time = key.time - t0;
						c.key.selected = false;
						clipboard.PushBack(c);
					}
				}
			}
			return (uint32)clipboard.Size();
		}

		bool NkTimelineModel::CutSelection() {
			return CopySelection() > 0 && DeleteSelection();
		}

		bool NkTimelineModel::PasteAt(float32 t) {
			if (clipboard.Empty()) {
				return false;
			}
			PushUndo();
			ClearSelection();
			bool une = false;
			for (uint32 i = 0; i < (uint32)clipboard.Size(); ++i) {
				const NkTimelineClipboardKey &c = clipboard[i];
				NkTimelineTrack *tr = Track(c.track);
				if (tr == nullptr || tr->locked) {
					continue;
				}
				const int32 k = SetKey(c.track, t + c.key.time, c.key.v, c.key.interp, false);
				if (k >= 0) {
					NkTimelineKey &dst = tr->keys[(uint32)k];
					dst.tangent = c.key.tangent;
					for (uint32 ch = 0; ch < 4; ++ch) {
						dst.tanIn[ch] = c.key.tanIn[ch];
						dst.tanOut[ch] = c.key.tanOut[ch];
					}
					dst.selected = true;
					une = true;
				}
			}
			Touch();
			return une;
		}

		// ── (01/10 soir) Les marqueurs ──────────────────────────────────────────
		nk_uint64 NkTimelineModel::AddMarker(float32 t, const NkString &name) {
			PushUndo();
			NkTimelineMarker mk;
			mk.id = nextMarkerId++;
			mk.time = Snap(t < 0.f ? 0.f : t);
			mk.name = name.Empty() ? NkString::Format("M%u", (unsigned)mk.id) : name;
			uint32 j = 0;
			while (j < (uint32)markers.Size() && markers[j].time <= mk.time) {
				++j;
			}
			markers.Insert(markers.Begin() + j, mk);
			Touch();
			return mk.id;
		}

		bool NkTimelineModel::RemoveMarker(nk_uint64 id) {
			for (uint32 i = 0; i < (uint32)markers.Size(); ++i) {
				if (markers[i].id == id) {
					PushUndo();
					markers.Erase(markers.Begin() + i);
					Touch();
					return true;
				}
			}
			return false;
		}

		NkTimelineMarker *NkTimelineModel::Marker(nk_uint64 id) {
			for (uint32 i = 0; i < (uint32)markers.Size(); ++i) {
				if (markers[i].id == id) {
					return &markers[i];
				}
			}
			return nullptr;
		}

		// ── (01/10 soir) Les clips (NLA) ────────────────────────────────────────
		nk_uint64 NkTimelineModel::AddClip(nk_uint64 trackId, const NkString &name, float32 start, float32 length,
										   float32 sourceLength) {
			NkTimelineTrack *tr = Track(trackId);
			if (tr == nullptr || tr->kind != NkTimelineValueKind::Clips || length <= 0.f) {
				return 0;
			}
			PushUndo();
			NkTimelineClip c;
			c.id = nextClipId++;
			c.name = name;
			c.start = Snap(start < 0.f ? 0.f : start);
			c.length = length;
			c.sourceLength = sourceLength > 0.f ? sourceLength : length;
			tr->clips.PushBack(c);
			TrierClips(*tr);
			if (c.End() > duration) {
				duration = c.End();
			}
			Touch();
			return c.id;
		}

		NkTimelineClip *NkTimelineModel::Clip(nk_uint64 trackId, nk_uint64 clip) {
			NkTimelineTrack *tr = Track(trackId);
			for (uint32 k = 0; tr != nullptr && k < (uint32)tr->clips.Size(); ++k) {
				if (tr->clips[k].id == clip) {
					return &tr->clips[k];
				}
			}
			return nullptr;
		}

		NkTimelineClip *NkTimelineModel::FindClip(nk_uint64 clip, nk_uint64 *track) {
			for (uint32 i = 0; clip != 0 && i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].clips.Size(); ++k) {
					if (tracks[i].clips[k].id == clip) {
						if (track != nullptr) {
							*track = tracks[i].id;
						}
						return &tracks[i].clips[k];
					}
				}
			}
			return nullptr;
		}

		nk_uint64 NkTimelineModel::ClipAt(const NkTimelineTrack &tr, float32 t) const {
			nk_uint64 r = 0;
			for (uint32 k = 0; k < (uint32)tr.clips.Size(); ++k) {
				if (t >= tr.clips[k].start && t <= tr.clips[k].End()) {
					r = tr.clips[k].id;
				}
			}
			return r;
		}

		void NkTimelineModel::SelectClip(nk_uint64 clip, bool additive) {
			if (!additive) {
				ClearSelection();
			}
			nk_uint64 piste = 0;
			if (NkTimelineClip *c = FindClip(clip, &piste)) {
				const NkTimelineTrack *tr = Track(piste);
				if (tr != nullptr && !tr->locked) {
					c->selected = additive ? !c->selected : true;
					activeClip = c->selected ? c->id : activeClip;
				}
			}
		}

		uint32 NkTimelineModel::ClipSelectionCount() const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				for (uint32 k = 0; k < (uint32)tracks[i].clips.Size(); ++k) {
					n += tracks[i].clips[k].selected ? 1u : 0u;
				}
			}
			return n;
		}

		void NkTimelineModel::SortClips() {
			for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
				TrierClips(tracks[i]);
			}
		}

		float32 NkTimelineModel::ClipWeightAt(const NkTimelineClip &c, float32 t) {
			if (t < c.start || t > c.End() || c.length <= 0.f) {
				return 0.f;
			}
			float32 w = c.weight;
			if (c.blendIn > 1e-6f && t < c.start + c.blendIn) {
				w *= (t - c.start) / c.blendIn;
			}
			if (c.blendOut > 1e-6f && t > c.End() - c.blendOut) {
				w *= (c.End() - t) / c.blendOut;
			}
			return w < 0.f ? 0.f : w;
		}

		float32 NkTimelineModel::ClipLocalTime(const NkTimelineClip &c, float32 t) {
			float32 local = c.offset + (t - c.start) * c.rate;
			const float32 d = c.sourceLength;
			if (d <= 1e-6f) {
				return 0.f;
			}
			if (c.loop) {
				local = std::fmod(local, d);
				if (local < 0.f) {
					local += d;
				}
				return local;
			}
			return local < 0.f ? 0.f : (local > d ? d : local);
		}

		// ── Annuler / Refaire ───────────────────────────────────────────────────
		void NkTimelineModel::PushUndo() {
			NkTimelineSnapshot s;
			s.tracks = tracks;
			s.duration = duration;
			s.markers = markers;
			s.rangeStart = rangeStart;
			s.rangeEnd = rangeEnd;
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
			cur.markers = markers;
			cur.rangeStart = rangeStart;
			cur.rangeEnd = rangeEnd;
			redoStack.PushBack(cur);
			const NkTimelineSnapshot &s = undoStack[undoStack.Size() - 1];
			tracks = s.tracks;
			duration = s.duration;
			markers = s.markers;
			rangeStart = s.rangeStart;
			rangeEnd = s.rangeEnd;
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
			cur.markers = markers;
			cur.rangeStart = rangeStart;
			cur.rangeEnd = rangeEnd;
			undoStack.PushBack(cur);
			const NkTimelineSnapshot &s = redoStack[redoStack.Size() - 1];
			tracks = s.tracks;
			duration = s.duration;
			markers = s.markers;
			rangeStart = s.rangeStart;
			rangeEnd = s.rangeEnd;
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
				TrierClips(tr); // (01/10 soir)
			}
			Touch();
		}

	} // namespace editorkit
} // namespace nkentseu
