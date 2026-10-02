// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Noge/Sequencer/NkSequencer.cpp — le CORPS du séquenceur déclaré en 2026 dans
// NkSequencer.h, resté en-tête seul jusqu'ici (16 méthodes sans corps, 16 inline).
// -----------------------------------------------------------------------------
// CE QUE CE FICHIER LIVRE, ET CE QU'IL NE LIVRE PAS.
//
// Livré, avec son banc : les clés et leur relecture (NkKeyframe, NkAnimChannel),
// l'avance du temps (NkPlaybackCtrl::Update), l'évaluation d'une piste Transform
// et de la séquence entière (NkTrack/NkSequence::Evaluate), la durée recalculée,
// et le choix du plan caméra actif.
//
// Livré le soir même : la SÉRIALISATION `.nkseq` (SaveToFile / LoadFromFile).
// Elles rendaient `false` sans toucher au disque tant qu'on ne savait pas ce que
// le séquenceur portait — figer un format qui aurait perdu les caméras et les
// marqueurs aurait coûté des années. Maintenant on le sait, et il est écrit.
//
// PAS livré, et qui le DIT au lieu de faire semblant :
//   - (2026-10-01 soir) NkNLATrack::Evaluate est LIVRE avec un registre de
//     clips : la pile de poses qu'il attendait est NKAnima/Blend/NkAnimMix.h. La
//     piste `Animation` MELANGE aussi ses clips qui se chevauchent (par leurs
//     fondus) au lieu de prendre le dernier.
//
// L'interpolation suit le contrat de bord de NKAnima (NkAnimation.h:108-122) :
// HORS de l'intervalle des clés, on rend la valeur de BORD — jamais d'extrapolation.
// Trois `NkKeyframe` coexistent dans le dépôt (celui-ci, NKAnima, Noge/ECS) ; ce
// fichier n'en fusionne aucun, il s'aligne seulement sur leur comportement commun.
// =============================================================================
#include "Noge/Sequencer/NkSequencer.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Core/NkTag.h" // NkName : BindByName relie les pistes par le nom
#include "NKFileSystem/NkFile.h" // SaveToFile / LoadFromFile : octets bruts, jamais du texte
// La piste `Animation` a besoin du registre ET du lecteur de NKAnima. Ils sont
// inclus ICI et pas dans l'en-tete : celui-ci n'en declare que le nom.
#include "NKAnima/Clip/NkClipRegistry.h"
#include "NKAnima/Blend/NkAnimMix.h" // (01/10 soir) la pile de poses : NLA, fondus

#include <cmath> // asin / atan2 : les angles des canaux de rotation

namespace nkentseu {

	namespace {

		// ── Les courbes, écrites une fois ────────────────────────────────────
		// `a` est TOUJOURS normalisé dans [0,1] avant d'arriver ici : le bornage
		// est fait par l'appelant, qui seul connaît les temps des deux clés.
		inline float32 SeqLerp(float32 v0, float32 v1, float32 a) noexcept {
			return v0 + (v1 - v0) * a;
		}

		inline float32 SeqEase(float32 a, NkInterpolation kind) noexcept {
			switch (kind) {
				case NkInterpolation::EaseIn:
					return a * a;
				case NkInterpolation::EaseOut:
					return 1.f - (1.f - a) * (1.f - a);
				case NkInterpolation::EaseInOut:
					return a * a * (3.f - 2.f * a);
				default:
					return a;
			}
		}

		// Hermite cubique. Les tangentes sont exprimées en unités de valeur par
		// unité de TEMPS : il faut donc les multiplier par dt, sinon la courbe
		// change de forme quand on écarte deux clés — défaut classique et
		// silencieux, la courbe reste « jolie » mais ne passe plus par les
		// pentes demandées.
		inline float32 SeqHermite(float32 v0, float32 v1, float32 out0, float32 in1, float32 dt, float32 a) noexcept {
			const float32 a2 = a * a;
			const float32 a3 = a2 * a;
			const float32 h00 = 2.f * a3 - 3.f * a2 + 1.f;
			const float32 h10 = a3 - 2.f * a2 + a;
			const float32 h01 = -2.f * a3 + 3.f * a2;
			const float32 h11 = a3 - a2;
			return h00 * v0 + h10 * dt * out0 + h01 * v1 + h11 * dt * in1;
		}

		// Comparaison de nom de canal SANS <cstring> (zéro-STL, et l'en-tête ne
		// tire pas la libc). Rend true si `s` vaut exactement `lit`.
		inline bool SeqStrEq(const char *s, const char *lit) noexcept {
			if (s == nullptr || lit == nullptr)
				return false;
			uint32 i = 0;
			while (s[i] != '\0' && lit[i] != '\0') {
				if (s[i] != lit[i])
					return false;
				i++;
			}
			return s[i] == '\0' && lit[i] == '\0';
		}

	} // namespace

	// =========================================================================
	// NkAnimChannel
	// =========================================================================
	void NkAnimChannel::AddKey(float32 time, float32 value, NkInterpolation interp) noexcept {
		nkentseu::NkKeyframe kf;
		kf.time = time;
		kf.value = value;
		kf.interpolation = interp;
		// Insertion À SA PLACE : le canal reste trié en permanence, donc
		// Evaluate n'a jamais à trier ni à supposer. SortByTime reste exposé
		// pour les clés arrivées d'ailleurs (import, désérialisation).
		uint32 i = 0;
		const uint32 n = (uint32)keyframes.Size();
		while (i < n && keyframes[i].time <= time)
			i++;
		if (i >= n)
			keyframes.PushBack(static_cast<nkentseu::NkKeyframe &&>(kf));
		else
			keyframes.Insert(keyframes.Begin() + i, kf);
	}

	void NkAnimChannel::RemoveKey(uint32 idx) noexcept {
		if (idx >= (uint32)keyframes.Size())
			return;
		keyframes.Erase(keyframes.Begin() + idx);
	}

	float32 NkAnimChannel::Evaluate(float32 time) const noexcept {
		const uint32 n = (uint32)keyframes.Size();
		if (n == 0)
			return 0.f;
		// ── LE CONTRAT DE BORD ───────────────────────────────────────────────
		// Hors de l'intervalle : valeur de bord, JAMAIS d'extrapolation. Une
		// extrapolation linéaire ferait partir un os à l'infini dès qu'une piste
		// dépasse sa dernière clé d'une image.
		if (time <= keyframes[0].time)
			return keyframes[0].value;
		if (time >= keyframes[n - 1].time)
			return keyframes[n - 1].value;

		uint32 hi = 1;
		while (hi < n && keyframes[hi].time <= time)
			hi++;
		if (hi >= n)
			return keyframes[n - 1].value;
		return keyframes[hi - 1].Evaluate(keyframes[hi], time);
	}

	void NkAnimChannel::SortByTime() noexcept {
		// Tri par insertion : les canaux ont typiquement quelques dizaines de
		// clés et sont DÉJÀ presque triés (AddKey les insère à leur place). Sur
		// une entrée presque triée l'insertion est linéaire ; un tri rapide y
		// serait plus lent et bien plus long à écrire sans la STL.
		const uint32 n = (uint32)keyframes.Size();
		for (uint32 i = 1; i < n; ++i) {
			nkentseu::NkKeyframe kf = keyframes[i];
			uint32 j = i;
			while (j > 0 && keyframes[j - 1].time > kf.time) {
				keyframes[j] = keyframes[j - 1];
				j--;
			}
			keyframes[j] = kf;
		}
	}

	void NkAnimChannel::AutoSmooth() noexcept {
		const uint32 n = (uint32)keyframes.Size();
		if (n == 0)
			return;
		if (n == 1) {
			keyframes[0].inTangent = 0.f;
			keyframes[0].outTangent = 0.f;
			return;
		}
		for (uint32 i = 0; i < n; ++i) {
			float32 slope;
			if (i == 0) {
				const float32 dt = keyframes[1].time - keyframes[0].time;
				slope = (dt > 1e-6f) ? (keyframes[1].value - keyframes[0].value) / dt : 0.f;
			} else if (i == n - 1) {
				const float32 dt = keyframes[n - 1].time - keyframes[n - 2].time;
				slope = (dt > 1e-6f) ? (keyframes[n - 1].value - keyframes[n - 2].value) / dt : 0.f;
			} else {
				// Catmull-Rom : la pente au point i est celle de la corde qui
				// joint ses deux voisins. C'est ce qui donne une courbe lisse
				// qui passe EXACTEMENT par toutes les clés.
				const float32 dt = keyframes[i + 1].time - keyframes[i - 1].time;
				slope = (dt > 1e-6f) ? (keyframes[i + 1].value - keyframes[i - 1].value) / dt : 0.f;
			}
			keyframes[i].inTangent = slope;
			keyframes[i].outTangent = slope;
		}
	}

	// =========================================================================
	// NkClipOnTrack
	// =========================================================================
	float32 NkClipOnTrack::GetWeight(float32 t) const noexcept {
		if (!IsActive(t))
			return 0.f;
		float32 w = weight;
		if (blendIn > 1e-6f) {
			const float32 a = (t - startTime) / blendIn;
			if (a < 1.f)
				w *= (a < 0.f) ? 0.f : a;
		}
		if (blendOut > 1e-6f) {
			const float32 a = (EndTime() - t) / blendOut;
			if (a < 1.f)
				w *= (a < 0.f) ? 0.f : a;
		}
		return w;
	}

	// =========================================================================
	// NkTrack
	// =========================================================================
	namespace {

		// Applique un canal nommé au composant Transform. Rend true si le nom a
		// été RECONNU — la distinction compte : un canal non reconnu doit rester
		// visible comme tel, pas se confondre avec un canal appliqué à 0.
		bool SeqApplyToTransform(const NkAnimChannel &ch, float32 v, ecs::NkTransform &tr) noexcept {
			const char *p = ch.propertyName;
			if (SeqStrEq(p, "localPosition.x")) {
				tr.localPosition.x = v;
			} else if (SeqStrEq(p, "localPosition.y")) {
				tr.localPosition.y = v;
			} else if (SeqStrEq(p, "localPosition.z")) {
				tr.localPosition.z = v;
			} else if (SeqStrEq(p, "localScale.x")) {
				tr.localScale.x = v;
			} else if (SeqStrEq(p, "localScale.y")) {
				tr.localScale.y = v;
			} else if (SeqStrEq(p, "localScale.z")) {
				tr.localScale.z = v;
			} else {
				return false;
			}
			// Le monde est recalculé par NkTransformSystem ; sans ce drapeau la
			// pose change en mémoire et RIEN ne bouge à l'écran.
			tr.worldDirty = true;
			return true;
		}

		// ── LA PISTE `Animation`, ET POURQUOI IL FAUT DEUX APPELS ────────────
		//
		// ⚠️ LIS CECI AVANT D'« ALLÉGER » EN SUPPRIMANT LE `Update(0.f)`.
		//
		// On veut la pose À UN INSTANT ABSOLU `t`. Le réflexe est d'écrire :
		//
		//     lecteur.SeekTo(t);
		//     const NkAnimationState &s = lecteur.GetState();   // ← FAUX
		//
		// et c'est faux SANS AUCUN MESSAGE. Mesuré le 2026-09-13 :
		//
		//     NkAnimation.cpp:539   void NkAnimationPlayer::SeekTo(float32 t) {
		//     NkAnimation.cpp:540       mTime = t;
		//     NkAnimation.cpp:541   }
		//
		// `SeekTo` POSE LE TEMPS ET N'ÉVALUE RIEN. `GetState()` rend alors l'état
		// de l'évaluation PRÉCÉDENTE : une pose plausible, simplement en retard.
		//
		// L'évaluation vit dans `Update` — et, détail qui sauve tout, elle est
		// HORS du test de lecture :
		//
		//     NkAnimation.cpp:601   void Update(float32 dt) {
		//                             if (!mClip) return;
		//                             if (mPlaying) { mTime += dt * mSpeed; ... }
		//     NkAnimation.cpp:631       Evaluate(mClip, mTime, mState);   ← toujours
		//
		// Donc `Update(0.f)` évalue SANS faire avancer le temps, que la lecture
		// soit en cours ou non. D'où les deux appels, dans cet ordre.
		//
		// Et non, on ne peut pas appeler `Evaluate(clip, t, out)` directement :
		// elle est PRIVÉE (`NkAnimation.h:445` porte `private:`, la déclaration est
		// l.466). Si NKAnima reçoit un jour un `EvaluateAt(t)` public, ces deux
		// lignes deviendront une seule — et ce commentaire devra partir avec elles.
		// ── (2026-10-01 soir) Le transform d'OBJET d'un clip, et son melange ─────
		struct ObjetTRS {
				NkVec3f t = {0.f, 0.f, 0.f};
				NkQuatf r = NkQuatf::Identity();
				NkVec3f s = {1.f, 1.f, 1.f};
		};

		/// Le transform d'objet d'un clip au temps local : `Evaluate` du lecteur est
		/// privee (voir plus bas), on passe par SeekTo + Update(0).
		ObjetTRS EchantillonObjet(const anim::NkAnimationClip &clip, float32 local) {
			anim::NkAnimationPlayer lecteur;
			lecteur.SetClip(&clip);
			lecteur.SeekTo(local);
			lecteur.Update(0.f);
			const anim::NkAnimationState &etat = lecteur.GetState();
			ObjetTRS o;
			o.t = etat.position;
			o.s = etat.scale;
			// `etat.rotation` est un NkVec4f (x, y, z, w) : reconstruit composante par
			// composante (les deux types ne se convertissent pas, et c'est voulu).
			o.r = NkQuatf(etat.rotation.x, etat.rotation.y, etat.rotation.z, etat.rotation.w);
			return o;
		}

		ObjetTRS LireObjet(const ecs::NkTransform &tr) {
			ObjetTRS o;
			o.t = tr.localPosition;
			o.r = tr.localRotation;
			o.s = tr.localScale;
			return o;
		}

		void EcrireObjet(ecs::NkTransform &tr, const ObjetTRS &o) {
			tr.localPosition = o.t;
			tr.localRotation = o.r;
			tr.localScale = o.s;
			// Sans ce drapeau, la pose change en mémoire et RIEN ne bouge à l'écran.
			tr.worldDirty = true;
		}

		NkQuatf NlerpQ(const NkQuatf &a, const NkQuatf &b, float32 w) {
			const float32 d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
			const float32 sg = d < 0.f ? -1.f : 1.f;
			return NkQuatf(a.x + (b.x * sg - a.x) * w, a.y + (b.y * sg - a.y) * w, a.z + (b.z * sg - a.z) * w,
						   a.w + (b.w * sg - a.w) * w)
				.Normalized();
		}

		/// a <- a vers b au poids w.
		void MelangerObjet(ObjetTRS &a, const ObjetTRS &b, float32 w) {
			w = w < 0.f ? 0.f : (w > 1.f ? 1.f : w);
			a.t = {a.t.x + (b.t.x - a.t.x) * w, a.t.y + (b.t.y - a.t.y) * w, a.t.z + (b.t.z - a.t.z) * w};
			a.s = {a.s.x + (b.s.x - a.s.x) * w, a.s.y + (b.s.y - a.s.y) * w, a.s.z + (b.s.z - a.s.z) * w};
			a.r = NlerpQ(a.r, b.r, w);
		}

		/// Les os d'une pose melangee dans le NkSkeleton de l'entite (s'il en a un
		/// et que les comptes concordent), fondus depuis sa pose a `w`.
		void EcrireOs(NkWorld &world, NkEntityId e, const anim::NkAnimPose &pose, float32 w) {
			if (pose.bones.Empty() || w <= 0.f)
				return;
			ecs::NkSkeleton *sk = world.Get<ecs::NkSkeleton>(e);
			if (sk == nullptr || sk->pose.Size() != pose.bones.Size())
				return;
			for (uint32 j = 0; j < (uint32)pose.bones.Size(); ++j) {
				ecs::NkBonePose &b = sk->Pose(j);
				ObjetTRS cur;
				cur.t = b.localPosition;
				cur.r = b.localRotation;
				cur.s = b.localScale;
				ObjetTRS cible;
				cible.t = pose.bones[j].t;
				cible.r = pose.bones[j].r;
				cible.s = pose.bones[j].s;
				MelangerObjet(cur, cible, w);
				b.localPosition = cur.t;
				b.localRotation = cur.r;
				b.localScale = cur.s;
			}
		}

		void SeqEvalueAnimation(const NkTrack &piste, float32 time, NkWorld &world,
								const anim::NkClipRegistry *clips) noexcept {
			// Sans registre, une piste `Animation` ne peut pas résoudre le clip
			// qu'elle désigne. Elle n'applique donc RIEN — elle ne dégrade jamais
			// la pose existante, et l'appelant qui a oublié le registre voit une
			// scène immobile plutôt qu'une scène fausse.
			if (clips == nullptr || piste.clips.Empty())
				return;
			ecs::NkTransform *tr = world.Get<ecs::NkTransform>(piste.entity);
			if (tr == nullptr)
				return;

			// (2026-10-01 soir) Les clips qui se CHEVAUCHENT se MELANGENT par leurs
			// poids (fondus d'entree et de sortie, GetWeight) : la moyenne ponderee
			// des transforms (rotation par NLERP), puis, si la somme des poids reste
			// sous 1 (un clip seul a mi-fondu), un fondu depuis la pose presente.
			// Les os suivent le meme chemin si l'entite a un squelette.
			ObjetTRS acc;
			anim::NkAnimPose os, p;
			float32 somme = 0.f;
			bool premier = true;
			for (uint32 i = 0; i < (uint32)piste.clips.Size(); ++i) {
				const NkClipOnTrack &c = piste.clips[i];
				const float32 w = c.IsActive(time) ? c.GetWeight(time) : 0.f;
				if (w <= 0.f)
					continue;
				const anim::NkAnimationClip *clip = clips->Resolve(c.clipHandle);
				if (clip == nullptr)
					continue; // refus nommé, lisible par clips->DernierRefus()
				const float32 local = c.GetLocalTime(time);
				const ObjetTRS o = EchantillonObjet(*clip, local);
				somme += w;
				if (premier) {
					acc = o;
				} else {
					MelangerObjet(acc, o, w / somme);
				}
				if (!clip->boneTracks.Empty()) {
					anim::NkSampleClip(*clip, local, p);
					if (premier) {
						os = p;
					} else {
						anim::NkBlendPose(os, p, w / somme);
					}
				}
				premier = false;
			}
			if (premier)
				return;
			const float32 couverture = (somme > 1.f ? 1.f : somme) * piste.weight;
			ObjetTRS cur = LireObjet(*tr);
			MelangerObjet(cur, acc, couverture);
			EcrireObjet(*tr, cur);
			EcrireOs(world, piste.entity, os, couverture);
		}

	} // namespace

	void NkTrack::Evaluate(float32 time, NkWorld &world, const anim::NkClipRegistry *clips) const noexcept {
		if (muted || weight <= 0.f)
			return;
		if (entity == NkEntityId::Invalid())
			return;

		// ── LA PISTE `Animation` — elle AGIT enfin ───────────────────────────
		if (type == NkTrackType::Animation) {
			SeqEvalueAnimation(*this, time, world, clips);
			return;
		}

		if (channels.Empty())
			return;

		// ── CE QUI EST BRANCHÉ, ET CE QUI NE L'EST PAS ───────────────────────
		// `Animation` est traitée ci-dessus ; `Transform` et `Property` écrivent
		// dans NkTransform. `Camera`, `Audio`, `Event`, `FacialAnim`,
		// `BlendShape`, `Light`, `PostProcess`, `Particle`, `Visibility` et `NLA`
		// sont déclarées dans l'en-tête et N'AGISSENT PAS encore : elles sortent
		// d'ici sans rien toucher, et ce commentaire est le seul endroit où c'est
		// écrit. Dix sur treize, contre onze hier.
		if (type != NkTrackType::Transform && type != NkTrackType::Property)
			return;

		ecs::NkTransform *tr = world.Get<ecs::NkTransform>(entity);
		if (tr == nullptr)
			return;

		// (2026-10-02) LA ROTATION, en angles d'Euler EN DEGRES : `localRotation.x`
		// (tangage), `.y` (lacet), `.z` (roulis) -- les trois que montrent les
		// Details de la famille. Elles se composent EN UNE FOIS, apres les autres
		// canaux : un quaternion ne se modifie pas composante par composante.
		// Une composante sans canal garde la valeur de la pose presente.
		bool tourne = false;
		float32 euler[3] = {0.f, 0.f, 0.f};
		bool eulerPose[3] = {false, false, false};
		const uint32 n = (uint32)channels.Size();
		for (uint32 i = 0; i < n; ++i) {
			const NkAnimChannel &ch = channels[i];
			if (ch.muted || ch.keyframes.Empty())
				continue;
			const char *p = ch.propertyName;
			const int32 axe = SeqStrEq(p, "localRotation.x") ? 0 : SeqStrEq(p, "localRotation.y") ? 1 : SeqStrEq(p, "localRotation.z") ? 2 : -1;
			if (axe >= 0) {
				euler[axe] = ch.Evaluate(time);
				eulerPose[axe] = true;
				tourne = true;
				continue;
			}
			SeqApplyToTransform(ch, ch.Evaluate(time), *tr);
		}
		if (tourne) {
			if (!eulerPose[0] || !eulerPose[1] || !eulerPose[2]) {
				float32 actuel[3];
				NkSequenceDegreesFromRotation(tr->localRotation, actuel[0], actuel[1], actuel[2]);
				for (int32 a = 0; a < 3; ++a) {
					euler[a] = eulerPose[a] ? euler[a] : actuel[a];
				}
			}
			tr->localRotation = NkSequenceRotationFromDegrees(euler[0], euler[1], euler[2]);
			tr->worldDirty = true;
		}
	}

	// =========================================================================
	// NkCameraTrack
	// =========================================================================
	const NkCameraShot *NkCameraTrack::GetActiveShot(float32 t) const noexcept {
		if (muted)
			return nullptr;
		const uint32 n = (uint32)shots.Size();
		// DERNIER plan qui contient t, pas le premier : deux plans qui se
		// chevauchent sont un montage volontaire, et c'est celui du dessus qui
		// doit gagner — comme dans toute table de montage.
		const NkCameraShot *found = nullptr;
		for (uint32 i = 0; i < n; ++i) {
			if (t >= shots[i].startTime && t <= shots[i].EndTime())
				found = &shots[i];
		}
		return found;
	}

	// =========================================================================
	// NkNLATrack — DÉCLARÉ, PAS LIVRÉ
	// =========================================================================
	void NkNLATrack::Evaluate(float32 time, NkWorld &world) const noexcept {
		// Sans registre, aucun clip ne se resout : rien n'est applique (la pose
		// n'est jamais degradee). La surcharge a registre est celle qui joue.
		(void)time;
		(void)world;
	}

	void NkNLATrack::Evaluate(float32 time, NkWorld &world, const anim::NkClipRegistry *clips) const noexcept {
		// (2026-10-01 soir) LIVRE. La pile de poses est celle de NKAnima
		// (Blend/NkAnimMix.h) : les clips sont poses dans l'ordre, chacun sur ce
		// qui est dessous, a son influence x le poids de la piste.
		//   Replace  : fond vers le clip ;
		//   Add      : ajoute la difference du clip a SA premiere image (additif) ;
		//   Multiply : compose (positions et echelles multipliees, rotations
		//              composees), a l'influence.
		if (muted || weight <= 0.f || clips == nullptr || entity == NkEntityId::Invalid())
			return;
		ecs::NkTransform *tr = world.Get<ecs::NkTransform>(entity);
		if (tr == nullptr)
			return;
		ObjetTRS cur = LireObjet(*tr);
		anim::NkAnimPose os, p, ref, delta;
		ecs::NkSkeleton *sk = world.Get<ecs::NkSkeleton>(entity);
		if (sk != nullptr) {
			os.bones.Resize(sk->pose.Size());
			for (uint32 j = 0; j < (uint32)sk->pose.Size(); ++j) {
				os.bones[j].t = sk->Pose(j).localPosition;
				os.bones[j].r = sk->Pose(j).localRotation;
				os.bones[j].s = sk->Pose(j).localScale;
			}
		}
		bool agi = false;
		for (uint32 i = 0; i < (uint32)this->clips.Size(); ++i) {
			const NkNLAClip &c = this->clips[i];
			if (c.muted || c.duration <= 0.f || time < c.startTime || time > c.startTime + c.duration)
				continue;
			const anim::NkAnimationClip *clip = clips->Resolve(c.clipHandle);
			if (clip == nullptr)
				continue;
			// Le temps du clip : decalage, vitesse, repetitions, a rebours.
			const float32 d = clip->duration > 1e-6f ? clip->duration : 1.f;
			float32 local = c.clipOffset + (time - c.startTime) * c.speed;
			if (c.repeat) {
				const float32 tours = (float32)(c.repeatCount > 0 ? c.repeatCount : 1u);
				local = local > d * tours ? d * tours : local;
				local = std::fmod(local, d);
			} else {
				local = local < 0.f ? 0.f : (local > d ? d : local);
			}
			if (c.reverse)
				local = d - local;
			const float32 w = c.influence * weight;
			if (w <= 0.f)
				continue;
			const ObjetTRS o = EchantillonObjet(*clip, local);
			const bool aOs = !clip->boneTracks.Empty() && !os.bones.Empty();
			if (aOs)
				anim::NkSampleClip(*clip, local, p);
			switch (c.blendType) {
				case NkNLAClip::BlendType::Add: {
					const ObjetTRS o0 = EchantillonObjet(*clip, c.reverse ? d : 0.f);
					cur.t = {cur.t.x + (o.t.x - o0.t.x) * w, cur.t.y + (o.t.y - o0.t.y) * w, cur.t.z + (o.t.z - o0.t.z) * w};
					cur.r = (cur.r * NlerpQ(NkQuatf::Identity(), (o0.r.Conjugate() * o.r).Normalized(), w)).Normalized();
					auto div = [](float32 a, float32 b) { return (b > 1e-6f || b < -1e-6f) ? a / b : 1.f; };
					cur.s = {cur.s.x * (1.f + (div(o.s.x, o0.s.x) - 1.f) * w), cur.s.y * (1.f + (div(o.s.y, o0.s.y) - 1.f) * w),
							 cur.s.z * (1.f + (div(o.s.z, o0.s.z) - 1.f) * w)};
					if (aOs) {
						anim::NkSampleClip(*clip, c.reverse ? d : 0.f, ref);
						anim::NkMakeAdditive(p, ref, delta);
						anim::NkApplyAdditive(os, delta, w);
					}
					break;
				}
				case NkNLAClip::BlendType::Multiply: {
					ObjetTRS m = cur;
					m.t = {cur.t.x * o.t.x, cur.t.y * o.t.y, cur.t.z * o.t.z};
					m.s = {cur.s.x * o.s.x, cur.s.y * o.s.y, cur.s.z * o.s.z};
					m.r = (cur.r * o.r).Normalized();
					MelangerObjet(cur, m, w);
					break;
				}
				default:
					MelangerObjet(cur, o, w);
					if (aOs)
						anim::NkBlendPose(os, p, w);
					break;
			}
			agi = true;
		}
		if (!agi)
			return;
		EcrireObjet(*tr, cur);
		EcrireOs(world, entity, os, 1.f);
	}

	// =========================================================================
	// NkPlaybackCtrl
	// =========================================================================
	bool NkPlaybackCtrl::Update(float32 dt, float32 sequenceDuration) noexcept {
		if (!playing)
			return false;
		const float32 end = (loopEnd > 0.f) ? loopEnd : sequenceDuration;
		time += dt * speed;

		if (speed >= 0.f) {
			if (end > loopStart && time >= end) {
				if (loop) {
					// Modulo, pas remise à loopStart : à 24 i/s un dt qui
					// dépasse la fin de 3 images doit rendre 3 images après le
					// début, sinon la boucle « saute » et la cadence dérive.
					const float32 span = end - loopStart;
					float32 over = time - loopStart;
					while (over >= span)
						over -= span;
					time = loopStart + over;
					return false;
				}
				time = end;
				playing = false;
				return true;
			}
			return false;
		}

		// Rebours : la borne franchie est le DÉBUT.
		if (time <= loopStart) {
			if (loop && end > loopStart) {
				const float32 span = end - loopStart;
				float32 under = loopStart - time;
				while (under >= span)
					under -= span;
				time = end - under;
				return false;
			}
			time = loopStart;
			playing = false;
			return true;
		}
		return false;
	}

	// =========================================================================
	// NkSequence
	// =========================================================================
	void NkSequence::Evaluate(float32 time, NkWorld &world, const anim::NkClipRegistry *clips) const noexcept {
		const uint32 nt = (uint32)tracks.Size();
		for (uint32 i = 0; i < nt; ++i)
			tracks[i].Evaluate(time, world, clips);
		const uint32 nn = (uint32)nlaTracks.Size();
		for (uint32 i = 0; i < nn; ++i)
			nlaTracks[i].Evaluate(time, world, clips); // (01/10 soir) les pistes NLA jouent
		// La caméra n'est PAS appliquée ici : GetActiveCameraAt rend l'entité,
		// et c'est au rendu de choisir quoi en faire. Écrire la caméra active
		// dans le monde depuis une méthode `const` serait un effet de bord caché.
	}

	NkEntityId NkSequence::GetActiveCameraAt(float32 time) const noexcept {
		const NkCameraShot *shot = cameraTrack.GetActiveShot(time);
		return shot ? shot->cameraEntity : NkEntityId::Invalid();
	}

	void NkSequence::RecalcDuration() noexcept {
		float32 d = 0.f;
		const uint32 nt = (uint32)tracks.Size();
		for (uint32 i = 0; i < nt; ++i) {
			const NkTrack &tr = tracks[i];
			const uint32 nc = (uint32)tr.clips.Size();
			for (uint32 c = 0; c < nc; ++c)
				if (tr.clips[c].EndTime() > d)
					d = tr.clips[c].EndTime();
			const uint32 nch = (uint32)tr.channels.Size();
			for (uint32 c = 0; c < nch; ++c) {
				const NkAnimChannel &ch = tr.channels[c];
				const uint32 nk = (uint32)ch.keyframes.Size();
				if (nk > 0 && ch.keyframes[nk - 1].time > d)
					d = ch.keyframes[nk - 1].time;
			}
		}
		const uint32 ns = (uint32)cameraTrack.shots.Size();
		for (uint32 i = 0; i < ns; ++i)
			if (cameraTrack.shots[i].EndTime() > d)
				d = cameraTrack.shots[i].EndTime();
		const uint32 nm = (uint32)markers.Size();
		for (uint32 i = 0; i < nm; ++i)
			if (markers[i].time > d)
				d = markers[i].time;
		duration = d;
	}

	uint32 NkSequence::BindByName(NkWorld &world) noexcept {
		// Le nom -> l'entite, UNE requete par cible (les sequences ont quelques
		// pistes ; une table serait plus longue a ecrire qu'a parcourir).
		auto Trouver = [&](const NkString &nom) {
			NkEntityId r = NkEntityId::Invalid();
			world.Query<const ecs::NkName>().ForEach([&](NkEntityId id, const ecs::NkName &n) {
				if (!r.IsValid() && SeqStrEq(n.value, nom.CStr()))
					r = id;
			});
			return r;
		};
		uint32 perdues = 0;
		for (uint32 i = 0; i < (uint32)tracks.Size(); ++i) {
			if (tracks[i].entityName.Empty())
				continue;
			tracks[i].entity = Trouver(tracks[i].entityName);
			perdues += tracks[i].entity.IsValid() ? 0u : 1u;
		}
		for (uint32 i = 0; i < (uint32)nlaTracks.Size(); ++i) {
			if (nlaTracks[i].entityName.Empty())
				continue;
			nlaTracks[i].entity = Trouver(nlaTracks[i].entityName);
			perdues += nlaTracks[i].entity.IsValid() ? 0u : 1u;
		}
		for (uint32 i = 0; i < (uint32)cameraTrack.shots.Size(); ++i) {
			if (cameraTrack.shots[i].cameraName.Empty())
				continue;
			cameraTrack.shots[i].cameraEntity = Trouver(cameraTrack.shots[i].cameraName);
			perdues += cameraTrack.shots[i].cameraEntity.IsValid() ? 0u : 1u;
		}
		return perdues;
	}

	// =========================================================================
	// SÉRIALISATION — le format .nkseq
	// =========================================================================
	// Le 2026-09-13 au matin, ces deux méthodes rendaient `false` sans toucher au
	// disque : on ne savait pas encore ce que le séquenceur portait, et figer un
	// format qui aurait perdu les caméras et les marqueurs aurait coûté des
	// années. Maintenant on le sait, et le format est écrit.
	//
	// ── CE QUI A ÉTÉ CHERCHÉ : L'ÉCONOMIE ────────────────────────────────────
	// Binaire, séquentiel, aucun index, aucune table de symboles, aucun champ
	// réservé. Un champ réservé est un octet qu'on paie sans savoir pourquoi ; la
	// version en tête suffit à faire évoluer le format.
	//
	//   en-tête, 24 octets exactement
	//     "NKSQ"          4    magie
	//     version u32     4    = 1
	//     tailleCorps u64 8    ce que le corps DOIT faire
	//     empreinte u64   8    FNV-1a 64 du corps
	//
	// L'empreinte est le seul octet de redondance, et elle n'est pas décorative :
	// **sans elle, un octet abîmé au milieu du fichier passe inaperçu** — il
	// change une valeur, et rien ne le dit. C'est elle qui permet le refus.
	//
	// ── CE QU'ON N'ÉCRIT PAS, DÉLIBÉRÉMENT ───────────────────────────────────
	// Les drapeaux `selected` (piste, canal, clé). C'est un état d'INTERFACE, pas
	// du contenu : un fichier de séquence n'a pas à figer ce qui était surligné
	// quand on l'a enregistré. Comme `Load` construit des objets neufs où
	// `selected` vaut `false`, et que `Save` réécrit `false`, l'aller-retour reste
	// identique OCTET À OCTET — la perte est nulle, l'économie est réelle.
	//
	// On réutilise `NkEntityId::Pack()` / `Unpack()` (NkECSDefines.h:63 et 67)
	// plutôt que d'inventer un encodage d'entité : il existait déjà.
	// =========================================================================
	namespace {

		constexpr uint32 kSeqMagie = 0x5153934Eu; // "NKSQ" en little-endian
		// (2026-10-02) Version 2 : la scene visee et les noms des cibles. La
		// version 1 reste LUE (kSeqVersionMin) : « les anciennes se lisent pour
		// toujours » (CONVENTIONS_FICHIERS.md §6).
		constexpr uint32 kSeqVersion = 2u;
		constexpr uint32 kSeqVersionMin = 1u;
		constexpr uint32 kSeqEnTete = 24u;

		// Le refus doit être NOMMÉ : un `false` nu oblige l'appelant à deviner, et
		// il devine mal. Un seul emplacement, écrit avant chaque retour négatif.
		const char *gRefus = "";

		inline uint64 SeqEmpreinte(const uint8 *p, usize n) noexcept {
			uint64 h = 1469598103934665603ull;
			for (usize i = 0; i < n; ++i) {
				h ^= (uint64)p[i];
				h *= 1099511628211ull;
			}
			return h;
		}

		// ── L'écrivain ───────────────────────────────────────────────────────
		struct SeqEcrivain {
				NkVector<uint8> o;

				void Octets(const void *src, usize n) {
					const uint8 *p = (const uint8 *)src;
					for (usize i = 0; i < n; ++i)
						o.PushBack(p[i]);
				}
				void U8(uint8 v) {
					o.PushBack(v);
				}
				void U32(uint32 v) {
					Octets(&v, 4);
				}
				void U64(uint64 v) {
					Octets(&v, 8);
				}
				// Les bits bruts, pas une conversion décimale : un flottant qui
				// passe par du texte ne revient pas au dernier chiffre.
				void F32(float32 v) {
					Octets(&v, 4);
				}
				void Str(const NkString &s) {
					const uint32 n = (uint32)s.Length();
					U32(n);
					if (n)
						Octets(s.Data(), n);
				}
				// ── L'ÉCONOMIE, MESURÉE ──────────────────────────────────────
				// Les champs `char[N]` du modèle (label 128, eventFunction 128,
				// propertyName 64) étaient d'abord écrits BRUTS, capacité pleine.
				// Mesure sur une séquence de deux pistes, deux marqueurs et cinq
				// clés : corps de 1023 octets dont **832 nuls, soit 81 %** — dont
				// 640 pour ces seuls champs.
				//
				// On écrit donc la longueur utile puis les octets utiles. Le
				// MODÈLE NE CHANGE PAS (les structures gardent leurs `char[N]`),
				// et l'aller-retour reste identique octet à octet : `LisCap` met
				// toute la capacité à zéro avant de recopier, donc `Save` relit
				// exactement ce qu'il avait écrit.
				void Cap(const char *s, uint32 capacite) {
					uint32 n = 0;
					while (n + 1u < capacite && s[n] != '\0')
						n++;
					U32(n);
					if (n)
						Octets(s, n);
				}
		};

		// ── Le lecteur ───────────────────────────────────────────────────────
		// Il ne lit JAMAIS au-delà de la fin : chaque accès vérifie d'abord. Un
		// lecteur qui déborde sur un fichier tronqué produit des valeurs
		// plausibles, et c'est pire qu'un plantage.
		struct SeqLecteur {
				const uint8 *p = nullptr;
				usize n = 0, i = 0;
				bool ok = true;

				bool Reste(usize k) const noexcept {
					return ok && (i + k) <= n;
				}
				void Octets(void *dst, usize k) {
					if (!Reste(k)) {
						ok = false;
						return;
					}
					uint8 *d = (uint8 *)dst;
					for (usize j = 0; j < k; ++j)
						d[j] = p[i + j];
					i += k;
				}
				uint8 U8() {
					uint8 v = 0;
					Octets(&v, 1);
					return v;
				}
				uint32 U32() {
					uint32 v = 0;
					Octets(&v, 4);
					return v;
				}
				uint64 U64() {
					uint64 v = 0;
					Octets(&v, 8);
					return v;
				}
				float32 F32() {
					float32 v = 0.f;
					Octets(&v, 4);
					return v;
				}
				NkString Str() {
					const uint32 k = U32();
					if (!Reste(k)) {
						ok = false;
						return NkString();
					}
					NkString s((const char *)(p + i), (NkString::SizeType)k);
					i += k;
					return s;
				}
				// Pendant de `SeqEcrivain::Cap`. Met TOUTE la capacité à zéro avant
				// de recopier : sans cela, un champ relu garderait les octets du
				// précédent au-delà du terminateur, et l'aller-retour cesserait
				// d'être identique octet à octet pour une raison invisible.
				void LisCap(char *dst, uint32 capacite) {
					for (uint32 j = 0; j < capacite; ++j)
						dst[j] = '\0';
					const uint32 k = U32();
					// Une longueur qui dépasse la capacité déclarée est un fichier
					// hostile ou corrompu : on refuse au lieu de tronquer. Tronquer
					// donnerait une séquence plausible et fausse.
					if (k + 1u > capacite || !Reste(k)) {
						ok = false;
						return;
					}
					if (k)
						Octets(dst, k);
				}
		};

		void SeqEcrisCanal(SeqEcrivain &w, const NkAnimChannel &c) {
			w.Cap(c.propertyName, NkAnimChannel::kMaxName);
			w.U32(c.componentOffset);
			w.U8(c.locked ? 1u : 0u);
			w.U8(c.muted ? 1u : 0u);
			const uint32 nk = (uint32)c.keyframes.Size();
			w.U32(nk);
			for (uint32 k = 0; k < nk; ++k) {
				const nkentseu::NkKeyframe &kf = c.keyframes[k];
				w.F32(kf.time);
				w.F32(kf.value);
				w.F32(kf.inTangent);
				w.F32(kf.outTangent);
				w.U8((uint8)kf.interpolation);
			}
		}

		void SeqLisCanal(SeqLecteur &r, NkAnimChannel &c) {
			r.LisCap(c.propertyName, NkAnimChannel::kMaxName);
			c.componentOffset = r.U32();
			c.locked = (r.U8() != 0u);
			c.muted = (r.U8() != 0u);
			const uint32 nk = r.U32();
			for (uint32 k = 0; k < nk && r.ok; ++k) {
				nkentseu::NkKeyframe kf;
				kf.time = r.F32();
				kf.value = r.F32();
				kf.inTangent = r.F32();
				kf.outTangent = r.F32();
				kf.interpolation = (NkInterpolation)r.U8();
				if (r.ok)
					c.keyframes.PushBack(static_cast<nkentseu::NkKeyframe &&>(kf));
			}
		}

		void SeqEcrisClip(SeqEcrivain &w, const NkClipOnTrack &c) {
			w.U64(c.clipHandle);
			w.F32(c.startTime);
			w.F32(c.duration);
			w.F32(c.clipOffset);
			w.F32(c.speed);
			w.F32(c.blendIn);
			w.F32(c.blendOut);
			w.F32(c.weight);
			w.U8(c.loop ? 1u : 0u);
			w.U8(c.reverse ? 1u : 0u);
		}

		void SeqLisClip(SeqLecteur &r, NkClipOnTrack &c) {
			c.clipHandle = r.U64();
			c.startTime = r.F32();
			c.duration = r.F32();
			c.clipOffset = r.F32();
			c.speed = r.F32();
			c.blendIn = r.F32();
			c.blendOut = r.F32();
			c.weight = r.F32();
			c.loop = (r.U8() != 0u);
			c.reverse = (r.U8() != 0u);
		}

		void SeqEcrisSortie(SeqEcrivain &w, const NkRenderOutput &o) {
			w.U32(o.width);
			w.U32(o.height);
			w.F32(o.fps);
			w.F32(o.startTime);
			w.F32(o.endTime);
			w.U8((uint8)o.format);
			w.U32(o.jpegQuality);
			w.U8(o.exrHDR ? 1u : 0u);
			w.Str(o.outputDirectory);
			w.Str(o.filePrefix);
			w.U8(o.renderMotionBlur ? 1u : 0u);
			w.U32(o.motionBlurSamples);
			w.U8(o.renderDOF ? 1u : 0u);
			w.U8(o.renderSSAO ? 1u : 0u);
			w.U8(o.renderShadows ? 1u : 0u);
		}

		void SeqLisSortie(SeqLecteur &r, NkRenderOutput &o) {
			o.width = r.U32();
			o.height = r.U32();
			o.fps = r.F32();
			o.startTime = r.F32();
			o.endTime = r.F32();
			o.format = (NkRenderOutput::Format)r.U8();
			o.jpegQuality = r.U32();
			o.exrHDR = (r.U8() != 0u);
			o.outputDirectory = r.Str();
			o.filePrefix = r.Str();
			o.renderMotionBlur = (r.U8() != 0u);
			o.motionBlurSamples = r.U32();
			o.renderDOF = (r.U8() != 0u);
			o.renderSSAO = (r.U8() != 0u);
			o.renderShadows = (r.U8() != 0u);
		}

	} // namespace

	const char *NkSequenceDernierRefus() noexcept {
		return gRefus;
	}

	NkQuatf NkSequenceRotationFromDegrees(float32 pitch, float32 yaw, float32 roll) noexcept {
		return (NkQuatf::RotateY(math::NkAngle(yaw)) * NkQuatf::RotateX(math::NkAngle(pitch)) *
				NkQuatf::RotateZ(math::NkAngle(roll)))
			.Normalized();
	}

	void NkSequenceDegreesFromRotation(const NkQuatf &qIn, float32 &pitch, float32 &yaw, float32 &roll) noexcept {
		// R = Ry(lacet) . Rx(tangage) . Rz(roulis) ; ses termes utiles :
		//   R12 = -sin(tangage)
		//   R02 = sin(lacet) cos(tangage)   R22 = cos(lacet) cos(tangage)
		//   R10 = cos(tangage) sin(roulis)  R11 = cos(tangage) cos(roulis)
		const NkQuatf q = qIn.Normalized();
		const float32 x = q.x, y = q.y, z = q.z, w = q.w;
		const float32 r12 = 2.f * (y * z - w * x);
		const float32 r02 = 2.f * (x * z + w * y);
		const float32 r22 = 1.f - 2.f * (x * x + y * y);
		const float32 r10 = 2.f * (x * y + w * z);
		const float32 r11 = 1.f - 2.f * (x * x + z * z);
		const float32 kDeg = 57.29577951f;
		float32 s = -r12;
		s = s > 1.f ? 1.f : (s < -1.f ? -1.f : s);
		pitch = std::asin(s) * kDeg;
		if (s > 0.99999f || s < -0.99999f) {
			// Droit en haut ou en bas : le cap et le roulis se confondent ; le
			// roulis est pose a zero et le cap porte tout.
			const float32 r00 = 1.f - 2.f * (y * y + z * z);
			const float32 r20 = 2.f * (x * z - w * y);
			yaw = std::atan2(-r20, r00) * kDeg;
			roll = 0.f;
			return;
		}
		yaw = std::atan2(r02, r22) * kDeg;
		roll = std::atan2(r10, r11) * kDeg;
	}

	bool NkSequence::SaveToFile(const char *path) const noexcept {
		gRefus = "";
		if (path == nullptr || path[0] == '\0') {
			gRefus = "chemin vide";
			return false;
		}

		SeqEcrivain w;
		w.Str(name);
		w.Str(scene); // v2
		w.F32(fps);
		w.F32(duration);
		SeqEcrisSortie(w, renderOutput);

		w.U32((uint32)tracks.Size());
		for (uint32 t = 0; t < (uint32)tracks.Size(); ++t) {
			const NkTrack &tr = tracks[t];
			w.U8((uint8)tr.type);
			w.U64(tr.entityName.Empty() ? tr.entity.Pack() : NkEntityId::Invalid().Pack());
			w.Str(tr.entityName); // v2
			w.Str(tr.name);
			w.U8(tr.muted ? 1u : 0u);
			w.U8(tr.locked ? 1u : 0u);
			w.F32(tr.weight);
			w.U32((uint32)tr.clips.Size());
			for (uint32 c = 0; c < (uint32)tr.clips.Size(); ++c)
				SeqEcrisClip(w, tr.clips[c]);
			w.U32((uint32)tr.channels.Size());
			for (uint32 c = 0; c < (uint32)tr.channels.Size(); ++c)
				SeqEcrisCanal(w, tr.channels[c]);
		}

		w.U32((uint32)nlaTracks.Size());
		for (uint32 t = 0; t < (uint32)nlaTracks.Size(); ++t) {
			const NkNLATrack &nt = nlaTracks[t];
			w.U64(nt.entityName.Empty() ? nt.entity.Pack() : NkEntityId::Invalid().Pack());
			w.Str(nt.entityName); // v2
			w.Str(nt.name);
			w.U8(nt.muted ? 1u : 0u);
			w.U8(nt.locked ? 1u : 0u);
			w.F32(nt.weight);
			w.U32((uint32)nt.clips.Size());
			for (uint32 c = 0; c < (uint32)nt.clips.Size(); ++c) {
				const NkNLAClip &cl = nt.clips[c];
				w.U64(cl.clipHandle);
				w.F32(cl.startTime);
				w.F32(cl.duration);
				w.F32(cl.clipOffset);
				w.F32(cl.speed);
				w.F32(cl.influence);
				w.U8(cl.repeat ? 1u : 0u);
				w.U32(cl.repeatCount);
				w.U8(cl.reverse ? 1u : 0u);
				w.U8(cl.muted ? 1u : 0u);
				w.U8((uint8)cl.blendType);
			}
		}

		w.U8(cameraTrack.muted ? 1u : 0u);
		w.U8(cameraTrack.locked ? 1u : 0u);
		w.U32((uint32)cameraTrack.shots.Size());
		for (uint32 s = 0; s < (uint32)cameraTrack.shots.Size(); ++s) {
			const NkCameraShot &sh = cameraTrack.shots[s];
			w.U64(sh.cameraName.Empty() ? sh.cameraEntity.Pack() : NkEntityId::Invalid().Pack());
			w.Str(sh.cameraName); // v2
			w.F32(sh.startTime);
			w.F32(sh.duration);
			w.U8((uint8)sh.cutType);
			w.F32(sh.blendDuration);
		}

		w.U32((uint32)markers.Size());
		for (uint32 m = 0; m < (uint32)markers.Size(); ++m) {
			const NkMarker &mk = markers[m];
			w.Cap(mk.label, NkMarker::kMaxLabel);
			w.F32(mk.time);
			w.F32(mk.color.x);
			w.F32(mk.color.y);
			w.F32(mk.color.z);
			w.U8((uint8)mk.type);
			w.Cap(mk.eventFunction, 128u);
		}

		const usize nCorps = (usize)w.o.Size();
		const uint64 emp = SeqEmpreinte(w.o.Data(), nCorps);

		NkVector<uint8> fichier;
		SeqEcrivain h;
		h.U32(kSeqMagie);
		h.U32(kSeqVersion);
		h.U64((uint64)nCorps);
		h.U64(emp);
		for (uint32 i = 0; i < (uint32)h.o.Size(); ++i)
			fichier.PushBack(h.o[i]);
		for (usize i = 0; i < nCorps; ++i)
			fichier.PushBack(w.o[(uint32)i]);

		if (!NkFile::WriteAllBytes(path, fichier)) {
			gRefus = "ecriture disque refusee";
			return false;
		}
		return true;
	}

	bool NkSequence::LoadFromFile(const char *path) noexcept {
		gRefus = "";
		if (path == nullptr || path[0] == '\0') {
			gRefus = "chemin vide";
			return false;
		}
		NkVector<uint8> d = NkFile::ReadAllBytes(path);
		if ((usize)d.Size() < kSeqEnTete) {
			gRefus = "fichier trop court pour porter un en-tete";
			return false;
		}
		SeqLecteur hr;
		hr.p = d.Data();
		hr.n = (usize)d.Size();
		if (hr.U32() != kSeqMagie) {
			gRefus = "magie absente : ce n'est pas un .nkseq";
			return false;
		}
		const uint32 ver = hr.U32();
		if (ver < kSeqVersionMin || ver > kSeqVersion) {
			gRefus = "version de format inconnue";
			return false;
		}
		const uint64 taille = hr.U64();
		const uint64 emp = hr.U64();
		if ((uint64)d.Size() - kSeqEnTete != taille) {
			gRefus = "taille du corps differente de celle annoncee";
			return false;
		}
		// ── LE SEUL CONTRÔLE QUI VOIT UN OCTET ABÎMÉ AU MILIEU ───────────────
		// La magie protège l'en-tête, la taille protège la troncature. Ni l'une
		// ni l'autre ne voit un octet changé au cœur des données : il donnerait
		// simplement une clé décalée, et la séquence serait lue « avec succès ».
		if (SeqEmpreinte(d.Data() + kSeqEnTete, (usize)taille) != emp) {
			gRefus = "empreinte du corps incorrecte : fichier abime";
			return false;
		}

		// À partir d'ici seulement, on touche à `*this`. Un chargement qui
		// échouerait après avoir vidé les pistes laisserait une séquence
		// mutilée, et l'appelant n'aurait plus rien à quoi revenir.
		SeqLecteur r;
		r.p = d.Data() + kSeqEnTete;
		r.n = (usize)taille;

		const bool v2 = ver >= 2u;
		NkSequence tmp;
		tmp.name = r.Str();
		if (v2)
			tmp.scene = r.Str();
		tmp.fps = r.F32();
		tmp.duration = r.F32();
		SeqLisSortie(r, tmp.renderOutput);

		const uint32 nt = r.U32();
		for (uint32 t = 0; t < nt && r.ok; ++t) {
			NkTrack tr;
			tr.type = (NkTrackType)r.U8();
			tr.entity = NkEntityId::Unpack(r.U64());
			if (v2)
				tr.entityName = r.Str();
			tr.name = r.Str();
			tr.muted = (r.U8() != 0u);
			tr.locked = (r.U8() != 0u);
			tr.weight = r.F32();
			const uint32 nc = r.U32();
			for (uint32 c = 0; c < nc && r.ok; ++c) {
				NkClipOnTrack cl;
				SeqLisClip(r, cl);
				if (r.ok)
					tr.clips.PushBack(static_cast<NkClipOnTrack &&>(cl));
			}
			const uint32 nch = r.U32();
			for (uint32 c = 0; c < nch && r.ok; ++c) {
				NkAnimChannel ch;
				SeqLisCanal(r, ch);
				if (r.ok)
					tr.channels.PushBack(static_cast<NkAnimChannel &&>(ch));
			}
			if (r.ok)
				tmp.tracks.PushBack(static_cast<NkTrack &&>(tr));
		}

		const uint32 nn = r.U32();
		for (uint32 t = 0; t < nn && r.ok; ++t) {
			NkNLATrack nt2;
			nt2.entity = NkEntityId::Unpack(r.U64());
			if (v2)
				nt2.entityName = r.Str();
			nt2.name = r.Str();
			nt2.muted = (r.U8() != 0u);
			nt2.locked = (r.U8() != 0u);
			nt2.weight = r.F32();
			const uint32 nc = r.U32();
			for (uint32 c = 0; c < nc && r.ok; ++c) {
				NkNLAClip cl;
				cl.clipHandle = r.U64();
				cl.startTime = r.F32();
				cl.duration = r.F32();
				cl.clipOffset = r.F32();
				cl.speed = r.F32();
				cl.influence = r.F32();
				cl.repeat = (r.U8() != 0u);
				cl.repeatCount = r.U32();
				cl.reverse = (r.U8() != 0u);
				cl.muted = (r.U8() != 0u);
				cl.blendType = (NkNLAClip::BlendType)r.U8();
				if (r.ok)
					nt2.clips.PushBack(static_cast<NkNLAClip &&>(cl));
			}
			if (r.ok)
				tmp.nlaTracks.PushBack(static_cast<NkNLATrack &&>(nt2));
		}

		tmp.cameraTrack.muted = (r.U8() != 0u);
		tmp.cameraTrack.locked = (r.U8() != 0u);
		const uint32 ns = r.U32();
		for (uint32 s = 0; s < ns && r.ok; ++s) {
			NkCameraShot sh;
			sh.cameraEntity = NkEntityId::Unpack(r.U64());
			if (v2)
				sh.cameraName = r.Str();
			sh.startTime = r.F32();
			sh.duration = r.F32();
			sh.cutType = (NkCutType)r.U8();
			sh.blendDuration = r.F32();
			if (r.ok)
				tmp.cameraTrack.shots.PushBack(static_cast<NkCameraShot &&>(sh));
		}

		const uint32 nm = r.U32();
		for (uint32 m = 0; m < nm && r.ok; ++m) {
			NkMarker mk;
			r.LisCap(mk.label, NkMarker::kMaxLabel);
			mk.time = r.F32();
			mk.color.x = r.F32();
			mk.color.y = r.F32();
			mk.color.z = r.F32();
			mk.type = (NkMarker::Type)r.U8();
			r.LisCap(mk.eventFunction, 128u);
			if (r.ok)
				tmp.markers.PushBack(static_cast<NkMarker &&>(mk));
		}

		if (!r.ok) {
			gRefus = "corps incomplet : lecture au-dela de la fin";
			return false;
		}
		if (r.i != r.n) {
			// Tout doit être consommé. Des octets en trop signifient que le
			// lecteur et l'écrivain ne s'accordent pas sur le format — et c'est
			// le genre de désaccord qui ne se voit qu'une fois en production.
			gRefus = "octets en trop apres la fin des donnees";
			return false;
		}

		name = static_cast<NkString &&>(tmp.name);
		scene = static_cast<NkString &&>(tmp.scene);
		fps = tmp.fps;
		duration = tmp.duration;
		renderOutput = static_cast<NkRenderOutput &&>(tmp.renderOutput);
		tracks = static_cast<NkVector<NkTrack> &&>(tmp.tracks);
		nlaTracks = static_cast<NkVector<NkNLATrack> &&>(tmp.nlaTracks);
		cameraTrack = static_cast<NkCameraTrack &&>(tmp.cameraTrack);
		markers = static_cast<NkVector<NkMarker> &&>(tmp.markers);
		return true;
	}

} // namespace nkentseu

// =============================================================================
// NkKeyframe::Evaluate — DÉFINIE HORS DU NAMESPACE, ET VOICI POURQUOI
// -----------------------------------------------------------------------------
// `NkSequencer.h` ouvre `using namespace ecs;` (l.50) puis inclut
// `Noge/ECS/Components/Animation/NkAnimation.h`, qui déclare un AUTRE
// `NkKeyframe` (l.185). Depuis l'intérieur de `namespace nkentseu`, le nom
// `NkKeyframe` a donc DEUX candidats et le compilateur refuse :
//
//     error: reference to 'NkKeyframe' is ambiguous
//       candidate 1: 'struct nkentseu::ecs::NkKeyframe'   NkAnimation.h:185
//       candidate 2: 'struct nkentseu::NkKeyframe'        NkSequencer.h:97
//
// L'en-tête ne s'en aperçoit pas parce qu'il se qualifie lui-même une seule
// fois, l.116 : `NkVector<nkentseu::NkKeyframe> keyframes;`. Un en-tête qui
// compile n'annonce donc PAS que son corps compilera — c'est la mesure qui l'a
// dit, pas la lecture.
//
// Un alias (`using X = nkentseu::NkKeyframe;`) ne sert à rien ici : C++ interdit
// de nommer une classe par un typedef dans la définition hors-classe d'un de ses
// membres. Reste la qualification complète depuis la portée globale, ci-dessous.
//
// ⚠️ La vraie correction est ailleurs et elle n'appartient pas à ce lot :
// `using namespace ecs;` dans un en-tête public impose ses collisions à tous
// ceux qui l'incluent. Le retirer touche tout Noge et se mesure à part.
// =============================================================================
nkentseu::float32 nkentseu::NkKeyframe::Evaluate(const nkentseu::NkKeyframe &next,
												 nkentseu::float32 t) const noexcept {
	const nkentseu::float32 dt = next.time - time;
	// Deux clés au même instant : la seconde gagne. Ne JAMAIS diviser ici —
	// c'est ainsi qu'on récolte des NaN qui se propagent à toute la pose.
	if (dt <= 1e-6f)
		return next.value;
	nkentseu::float32 a = (t - time) / dt;
	if (a < 0.f)
		a = 0.f;
	if (a > 1.f)
		a = 1.f;

	switch (interpolation) {
		case nkentseu::NkInterpolation::Constant:
			// Stepped : la valeur tient jusqu'à la clé suivante EXCLUE.
			return value;
		case nkentseu::NkInterpolation::Bezier:
		case nkentseu::NkInterpolation::Auto:
			return nkentseu::SeqHermite(value, next.value, outTangent, next.inTangent, dt, a);
		case nkentseu::NkInterpolation::Linear:
			return nkentseu::SeqLerp(value, next.value, a);
		default:
			return nkentseu::SeqLerp(value, next.value, nkentseu::SeqEase(a, interpolation));
	}
}
