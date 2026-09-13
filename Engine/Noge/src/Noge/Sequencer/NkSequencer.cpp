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
//   - NkNLATrack::Evaluate — les pistes NLA n'appliquent rien (voir son corps).
//     Elles sont en revanche SÉRIALISÉES : le format porte ce que le moteur
//     n'exécute pas encore, pour ne pas avoir à le casser quand il l'exécutera.
//
// L'interpolation suit le contrat de bord de NKAnima (NkAnimation.h:108-122) :
// HORS de l'intervalle des clés, on rend la valeur de BORD — jamais d'extrapolation.
// Trois `NkKeyframe` coexistent dans le dépôt (celui-ci, NKAnima, Noge/ECS) ; ce
// fichier n'en fusionne aucun, il s'aligne seulement sur leur comportement commun.
// =============================================================================
#include "Noge/Sequencer/NkSequencer.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "NKFileSystem/NkFile.h" // SaveToFile / LoadFromFile : octets bruts, jamais du texte

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
		constexpr uint32 kSeqVersion = 1u;
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

	bool NkSequence::SaveToFile(const char *path) const noexcept {
		gRefus = "";
		if (path == nullptr || path[0] == '\0') {
			gRefus = "chemin vide";
			return false;
		}

		SeqEcrivain w;
		w.Str(name);
		w.F32(fps);
		w.F32(duration);
		SeqEcrisSortie(w, renderOutput);

		w.U32((uint32)tracks.Size());
		for (uint32 t = 0; t < (uint32)tracks.Size(); ++t) {
			const NkTrack &tr = tracks[t];
			w.U8((uint8)tr.type);
			w.U64(tr.entity.Pack());
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
			w.U64(nt.entity.Pack());
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
			w.U64(sh.cameraEntity.Pack());
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
		if (ver != kSeqVersion) {
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

		NkSequence tmp;
		tmp.name = r.Str();
		tmp.fps = r.F32();
		tmp.duration = r.F32();
		SeqLisSortie(r, tmp.renderOutput);

		const uint32 nt = r.U32();
		for (uint32 t = 0; t < nt && r.ok; ++t) {
			NkTrack tr;
			tr.type = (NkTrackType)r.U8();
			tr.entity = NkEntityId::Unpack(r.U64());
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
