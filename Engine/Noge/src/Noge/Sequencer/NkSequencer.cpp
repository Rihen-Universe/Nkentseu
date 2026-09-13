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
// PAS livré dans ce lot, et qui le DIT au lieu de faire semblant :
//   - NkNLATrack::Evaluate    — les pistes NLA n'appliquent rien (voir son corps)
//   - SaveToFile / LoadFromFile — retournent false. Jamais un faux succès, jamais
//     un fichier vide sur le disque : un format de sérialisation qui perdrait les
//     caméras et les marqueurs serait pire que pas de format du tout.
//
// L'interpolation suit le contrat de bord de NKAnima (NkAnimation.h:108-122) :
// HORS de l'intervalle des clés, on rend la valeur de BORD — jamais d'extrapolation.
// Trois `NkKeyframe` coexistent dans le dépôt (celui-ci, NKAnima, Noge/ECS) ; ce
// fichier n'en fusionne aucun, il s'aligne seulement sur leur comportement commun.
// =============================================================================
#include "Noge/Sequencer/NkSequencer.h"
#include "Noge/ECS/Components/Core/NkTransform.h"

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

	} // namespace

	void NkTrack::Evaluate(float32 time, NkWorld &world) const noexcept {
		if (muted || weight <= 0.f)
			return;
		if (entity == NkEntityId::Invalid())
			return;
		if (channels.Empty())
			return;

		// ── CE QUI EST BRANCHÉ, ET CE QUI NE L'EST PAS ───────────────────────
		// Transform et Property écrivent dans NkTransform. Animation, Camera,
		// Audio, Event, FacialAnim, BlendShape, Light, PostProcess, Particle,
		// Visibility et NLA sont déclarés dans l'en-tête et N'AGISSENT PAS
		// encore : ils sortent d'ici sans rien toucher, et ce commentaire est
		// le seul endroit où c'est écrit.
		if (type != NkTrackType::Transform && type != NkTrackType::Property)
			return;

		ecs::NkTransform *tr = world.Get<ecs::NkTransform>(entity);
		if (tr == nullptr)
			return;

		const uint32 n = (uint32)channels.Size();
		for (uint32 i = 0; i < n; ++i) {
			const NkAnimChannel &ch = channels[i];
			if (ch.muted || ch.keyframes.Empty())
				continue;
			SeqApplyToTransform(ch, ch.Evaluate(time), *tr);
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
		// Ce corps existe pour que l'édition de liens réussisse, et pour que
		// l'absence soit ÉCRITE plutôt que découverte. Le blend NLA (Replace /
		// Add / Multiply entre clips squelettiques) demande une pile de poses
		// qui n'existe pas encore côté Noge. Tant qu'elle n'existe pas, une
		// piste NLA n'applique RIEN — elle ne dégrade donc jamais la pose.
		(void)time;
		(void)world;
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
	void NkSequence::Evaluate(float32 time, NkWorld &world) const noexcept {
		const uint32 nt = (uint32)tracks.Size();
		for (uint32 i = 0; i < nt; ++i)
			tracks[i].Evaluate(time, world);
		const uint32 nn = (uint32)nlaTracks.Size();
		for (uint32 i = 0; i < nn; ++i)
			nlaTracks[i].Evaluate(time, world);
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

	// ── Sérialisation : PAS DANS CE LOT, et ça se voit ───────────────────────
	// Elles rendent false sans toucher au disque. Un format qui écrirait les
	// pistes en perdant les caméras, les marqueurs et les NLA produirait des
	// fichiers qu'il faudrait ensuite supporter éternellement. Mieux vaut un
	// refus franc qu'un format qu'on regrette.
	bool NkSequence::SaveToFile(const char *path) const noexcept {
		(void)path;
		return false;
	}

	bool NkSequence::LoadFromFile(const char *path) noexcept {
		(void)path;
		return false;
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
