// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkSequenceCheck — le banc du séquenceur : des clés entrent, des images sortent.
// -----------------------------------------------------------------------------
// QUATRE CRITÈRES, CHACUN AVEC SON ATTENDU ÉCRIT AVANT LA MESURE ET SON NÉGATIF.
//
//   f1  une séquence tient des clés et les relit — y compris HORS de l'intervalle
//   f2  le temps avance et la pose change — et à vitesse nulle elle ne bouge PAS
//   f3  la sortie existe sur le disque — 48 fichiers PNG numérotés
//   f4  frame_0001 et frame_0024 DIFFÈRENT — ce qui sépare un film d'un diaporama
//
// ⚠️ CE QUE CE BANC PROUVE, ET CE QU'IL NE PROUVE PAS.
//
// Les pixels écrits ici sont PEINTS PAR LE PROCESSEUR, pas rendus par NKRenderer :
// un fond uni, et un carré dont la position vient de `NkTransform::localPosition`
// APRÈS évaluation de la séquence. Ce n'est donc pas une preuve du moteur de rendu
// — c'est une preuve du PILOTE TEMPOREL, qui est ce qui manquait. Le chaînage
// prouvé est exactement : clés -> NkSequence::Evaluate -> pose ECS -> pixels ->
// fichiers numérotés. Le rendu hors écran (NkOffscreenTarget) est une deuxième
// question, volontairement non mélangée à celle-ci.
//
// Si le carré ne dépendait pas de la pose, f4 passerait au vert sans rien prouver.
// C'est le seul endroit où ce banc pourrait se mentir à lui-même, et c'est pour ça
// que la position du carré est lue DANS LE MONDE ECS, après Evaluate, jamais
// recalculée à côté.
//
// Aucune fenêtre, aucune souris, aucun clavier : que des appels de fonctions.
// =============================================================================
// ⚠️ L'ORDRE DE CES INCLUSIONS N'EST PAS UN DETAIL DE STYLE — IL EST OBLIGATOIRE.
//
// `NkSequencer.h:50` ouvre `using namespace ecs;`. Tout en-tete compile APRES lui
// voit donc `nkentseu::ecs::*` comme s'il etait dans `nkentseu`. Or Noge declare
// `ecs::NkRect2D` (NkRenderComponents.h:68) et NKRHI declare `NkRect2D`
// (NkTypes.h:455, un alias de `math::NkIntRect`). Resultat mesure le 2026-09-13,
// en mettant Noge en premier :
//
//   NKRHI/Core/NkTypes.h:457:20: error: reference to 'NkRect2D' is ambiguous
//     using NkScissor = NkRect2D;
//     candidate : nkentseu::NkRect2D        (NkTypes.h:455)
//     candidate : nkentseu::ecs::NkRect2D   (NkRenderComponents.h:68)
//   NKRHI/Commands/NkICommandBuffer.h:46: idem, sur BeginRenderPass lui-meme
//
// LE DEFAUT N'EST PAS DANS CE FICHIER : c'est NKRHI qui cesse de compiler, dans
// ses propres en-tetes, parce qu'un en-tete de Noge a ete lu avant lui. Un
// en-tete public qui ouvre un `using namespace` empoisonne tout ce qui le suit.
//
// Mettre NKRHI et NKRenderer EN PREMIER est un CONTOURNEMENT, pas une correction.
// La correction est de retirer `using namespace ecs;` de `NkSequencer.h` — elle
// touche tout Noge, elle appartient a Rodolf, et elle est signalee au canal.
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"

#include "Noge/Sequencer/NkSequencer.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "NKECS/World/NkWorld.h"
#include "NKMedia/Video/NkImageSequenceWriter.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKMemory/NKMemory.h"

#include <cstdio>

#if defined(_WIN32)
#	include <windows.h>
#endif

namespace {

	using nkentseu::float32;
	using nkentseu::uint32;
	using nkentseu::uint8;
	using nkentseu::usize;

	int gPass = 0;
	int gFail = 0;

	void Verdict(const char *nom, bool ok, const char *detail) {
		if (ok)
			gPass++;
		else
			gFail++;
		std::printf("  [%s] %-46s %s\n", ok ? "OK " : "NON", nom, detail);
	}

	bool Proche(float32 a, float32 b, float32 tol) {
		const float32 d = (a > b) ? (a - b) : (b - a);
		return d <= tol;
	}

	// ── L'empreinte d'un fichier ─────────────────────────────────────────────
	// FNV-1a 64 bits sur TOUS les octets. On rend aussi la taille : deux fichiers
	// de tailles différentes sont différents quoi qu'en dise un hachage, et le
	// dire évite de faire reposer un verdict sur la seule absence de collision.
	bool EmpreinteFichier(const char *chemin, nkentseu::uint64 &hors, nkentseu::uint64 &tailleOut) {
		nkentseu::NkFile f;
		if (!f.Open(chemin, nkentseu::NkFileMode::NK_READ_BINARY))
			return false;
		const nkentseu::nk_int64 taille = f.GetSize();
		if (taille <= 0) {
			f.Close();
			return false;
		}
		nkentseu::uint64 h = 1469598103934665603ull; // offset basis FNV-1a 64
		uint8 tampon[8192];
		nkentseu::uint64 lus = 0;
		for (;;) {
			const usize n = f.Read(tampon, sizeof(tampon));
			if (n == 0)
				break;
			for (usize i = 0; i < n; ++i) {
				h ^= (nkentseu::uint64)tampon[i];
				h *= 1099511628211ull; // prime FNV-1a 64
			}
			lus += (nkentseu::uint64)n;
		}
		f.Close();
		hors = h;
		tailleOut = lus;
		return lus > 0;
	}

	// Les huit octets de signature d'un PNG. Vérifier l'extension ne prouve rien :
	// un fichier vide nommé .png reste un fichier vide.
	bool EstUnPNG(const char *chemin) {
		nkentseu::NkFile f;
		if (!f.Open(chemin, nkentseu::NkFileMode::NK_READ_BINARY))
			return false;
		uint8 sig[8] = {0};
		const usize n = f.Read(sig, 8);
		f.Close();
		if (n != 8)
			return false;
		static const uint8 attendu[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
		for (int i = 0; i < 8; ++i)
			if (sig[i] != attendu[i])
				return false;
		return true;
	}

	// =========================================================================
	// f1 — une séquence tient des clés et les relit
	// =========================================================================
	void F1_ClesEtRelecture() {
		std::printf("\nf1 — DES CLES, ET LEUR RELECTURE A UN INSTANT INTERMEDIAIRE\n");
		std::printf("  attendu ECRIT AVANT LA MESURE : trois cles LINEAIRES\n");
		std::printf("     (0 s, 0)  (1 s, 10)  (2 s, 30)\n");
		std::printf("     v(t) = v0 + (v1-v0)*(t-t0)/(t1-t0), tolerance 1e-5\n");
		std::printf("     t=0,5 s -> 5,0     t=1,5 s -> 20,0\n");
		std::printf("     negatif : t=-1 s -> 0,0 EXACT   t=5 s -> 30,0 EXACT (pas d'extrapolation)\n\n");

		nkentseu::NkAnimChannel ch;
		ch.AddKey(0.f, 0.f, nkentseu::NkInterpolation::Linear);
		ch.AddKey(1.f, 10.f, nkentseu::NkInterpolation::Linear);
		ch.AddKey(2.f, 30.f, nkentseu::NkInterpolation::Linear);

		char d[160];

		std::snprintf(d, sizeof(d), "%u cles posees", (unsigned)ch.keyframes.Size());
		Verdict("le canal tient les trois cles", ch.keyframes.Size() == 3, d);

		const float32 a = ch.Evaluate(0.5f);
		std::snprintf(d, sizeof(d), "attendu 5,000000  mesure %.6f", (double)a);
		Verdict("t = 0,5 s au milieu du premier segment", Proche(a, 5.f, 1e-5f), d);

		const float32 b = ch.Evaluate(1.5f);
		std::snprintf(d, sizeof(d), "attendu 20,000000  mesure %.6f", (double)b);
		Verdict("t = 1,5 s au milieu du second segment", Proche(b, 20.f, 1e-5f), d);

		// ── LE NEGATIF ───────────────────────────────────────────────────────
		// Il ne suffit pas que la valeur soit « raisonnable » hors bornes : une
		// extrapolation lineaire donnerait -10 a t=-1 et 60 a t=5. On exige donc
		// l'egalite EXACTE avec la valeur de bord, pas une approximation.
		const float32 avant = ch.Evaluate(-1.f);
		std::snprintf(d, sizeof(d), "attendu 0,000000 exact  mesure %.6f", (double)avant);
		Verdict("NEGATIF : avant la premiere cle -> bord", avant == 0.f, d);

		const float32 apres = ch.Evaluate(5.f);
		std::snprintf(d, sizeof(d), "attendu 30,000000 exact  mesure %.6f", (double)apres);
		Verdict("NEGATIF : apres la derniere cle -> bord", apres == 30.f, d);

		// Un canal vide ne doit pas lire hors du tableau.
		nkentseu::NkAnimChannel vide;
		const float32 rienDu = vide.Evaluate(1.f);
		std::snprintf(d, sizeof(d), "mesure %.6f", (double)rienDu);
		Verdict("NEGATIF : canal sans aucune cle -> 0, sans plantage", rienDu == 0.f, d);
	}

	// =========================================================================
	// La scène du banc : une entité, un transform, deux canaux animés.
	// =========================================================================
	// ⚠️ ON N'Y GARDE AUCUNE REFERENCE. `AddTrack` et `AddChannel` rendent une
	// reference dans un NkVector qui va GROSSIR : ajouter un second canal peut
	// reallouer le premier et la reference precedente devient un pointeur fou.
	// Le banc adresse donc tout par INDICE. Ce n'est pas de la prudence gratuite :
	// c'est un piege reel de l'API declaree dans NkSequencer.h, signale au canal.
	//
	// ⚠️ ET ELLE NE VIT JAMAIS SUR LA PILE. Premiere execution du banc :
	// 0xC00000FD, STACK_OVERFLOW, avant la moindre ligne de sortie. Ce n'est pas
	// une recursion — c'est `NkWorld` qui est un gros objet par valeur, et le banc
	// en instanciait trois. La taille exacte est IMPRIMEE au demarrage plutot que
	// supposee : un « c'est sans doute gros » n'aurait pas ete une mesure.
	// Les scenes sont donc allouees sur le TAS, partout, sans exception.
	struct Scene {
			nkentseu::ecs::NkWorld world;
			nkentseu::NkSequence seq;
			nkentseu::NkEntityId sujet = nkentseu::NkEntityId::Invalid();
	};

	// Porte une Scene sur le TAS et la rend a la sortie du bloc, y compris sur un
	// `return` precoce. Sans destructeur, les trois `return` de garde du banc
	// fuiraient une scene chacun -- et un banc qui fuit est un banc dont on doute.
	struct PorteScene {
			Scene *p;
			PorteScene() : p(new Scene()) {}
			~PorteScene() {
				delete p;
			}
			PorteScene(const PorteScene &) = delete;
			PorteScene &operator=(const PorteScene &) = delete;
			Scene &operator*() const {
				return *p;
			}
	};

	void BatirScene(Scene &s) {
		s.sujet = s.world.CreateEntity();
		s.world.Add<nkentseu::ecs::NkTransform>(s.sujet, nkentseu::ecs::NkTransform{});

		s.seq.name = "BancFilm";
		s.seq.fps = 24.f;
		s.seq.AddTrack(s.sujet, nkentseu::NkTrackType::Transform, "sujet");

		// x : traverse l'image de gauche a droite en 2 s.
		s.seq.tracks[0].AddChannel("localPosition.x");
		s.seq.tracks[0].channels[0].AddKey(0.f, -3.f, nkentseu::NkInterpolation::Linear);
		s.seq.tracks[0].channels[0].AddKey(2.f, 3.f, nkentseu::NkInterpolation::Linear);

		// y : monte puis redescend — trois cles, pour que le mouvement ne soit pas
		// une simple translation uniforme qu'un decalage constant imiterait.
		s.seq.tracks[0].AddChannel("localPosition.y");
		s.seq.tracks[0].channels[1].AddKey(0.f, 0.f, nkentseu::NkInterpolation::Linear);
		s.seq.tracks[0].channels[1].AddKey(1.f, 2.f, nkentseu::NkInterpolation::Linear);
		s.seq.tracks[0].channels[1].AddKey(2.f, 0.f, nkentseu::NkInterpolation::Linear);

		s.seq.RecalcDuration();
	}

	// =========================================================================
	// f2 — le temps avance et la pose change
	// =========================================================================
	void F2_LeTempsChangeLaPose() {
		std::printf("\nf2 — LE TEMPS AVANCE, ET LA POSE CHANGE\n");
		std::printf("  attendu ECRIT AVANT LA MESURE :\n");
		std::printf("     piste Transform, canaux localPosition.x (-3 -> +3 en 2 s) et .y (0 -> 2 -> 0)\n");
		std::printf("     duree recalculee = 2,000000 s\n");
		std::printf("     ecart de position entre t=0 et t=1 s : > 1e-3\n");
		std::printf("     x(1 s) = 0,0 exactement (milieu du segment)\n");
		std::printf("     negatif : vitesse 0 -> le temps ne bouge pas -> pose identique AU BIT\n\n");

		PorteScene porte_s;
		Scene &s = *porte_s;
		BatirScene(s);
		char d[200];

		std::snprintf(d, sizeof(d), "attendu 2,000000  mesure %.6f", (double)s.seq.duration);
		Verdict("la duree se deduit des cles", Proche(s.seq.duration, 2.f, 1e-5f), d);

		s.seq.Evaluate(0.f, s.world);
		const nkentseu::ecs::NkTransform *tr = s.world.Get<nkentseu::ecs::NkTransform>(s.sujet);
		if (tr == nullptr) {
			Verdict("le sujet porte un NkTransform", false, "composant absent");
			return;
		}
		const nkentseu::math::NkVec3f p0 = tr->localPosition;

		s.seq.Evaluate(1.f, s.world);
		const nkentseu::math::NkVec3f p1 = tr->localPosition;

		const float32 dx = p1.x - p0.x, dy = p1.y - p0.y, dz = p1.z - p0.z;
		const float32 d2 = dx * dx + dy * dy + dz * dz;
		std::snprintf(d, sizeof(d), "t=0 (%.3f, %.3f)  t=1 (%.3f, %.3f)  ecart^2 %.6f", (double)p0.x, (double)p0.y,
					  (double)p1.x, (double)p1.y, (double)d2);
		Verdict("la pose a bouge entre t=0 et t=1 s", d2 > 1e-6f, d);

		std::snprintf(d, sizeof(d), "attendu 0,000000  mesure %.6f", (double)p1.x);
		Verdict("x(1 s) tombe au milieu, valeur exacte", Proche(p1.x, 0.f, 1e-5f), d);

		std::snprintf(d, sizeof(d), "attendu 2,000000  mesure %.6f", (double)p1.y);
		Verdict("y(1 s) est au sommet de sa courbe", Proche(p1.y, 2.f, 1e-5f), d);

		Verdict("worldDirty pose : le monde SAIT qu'il doit recalculer", tr->worldDirty, "drapeau leve");

		// ── LE NEGATIF : vitesse nulle ───────────────────────────────────────
		// On passe par NkPlaybackCtrl, c'est-a-dire par la MEME porte que la
		// lecture normale. Un negatif qui court-circuiterait le controleur ne
		// prouverait rien sur le controleur.
		nkentseu::NkPlaybackCtrl ctrl;
		ctrl.time = 0.7f;
		ctrl.speed = 0.f;
		ctrl.Play();
		s.seq.Evaluate(ctrl.time, s.world);
		const nkentseu::math::NkVec3f avant = tr->localPosition;
		for (int i = 0; i < 24; ++i)
			ctrl.Update(1.f / 24.f, s.seq.duration);
		s.seq.Evaluate(ctrl.time, s.world);
		const nkentseu::math::NkVec3f apres = tr->localPosition;

		const bool auBit = (avant.x == apres.x) && (avant.y == apres.y) && (avant.z == apres.z);
		std::snprintf(d, sizeof(d), "24 Update a vitesse 0 : t reste %.6f", (double)ctrl.time);
		Verdict("NEGATIF : vitesse 0 -> pose identique AU BIT", auBit, d);

		// Et le controle POSITIF du negatif : a vitesse 1, le meme controleur
		// DOIT faire bouger le temps. Sans lui, un Update qui ne ferait jamais
		// rien passerait le negatif haut la main.
		nkentseu::NkPlaybackCtrl vif;
		vif.time = 0.f;
		vif.speed = 1.f;
		vif.loop = false;
		vif.Play();
		for (int i = 0; i < 12; ++i)
			vif.Update(1.f / 24.f, s.seq.duration);
		std::snprintf(d, sizeof(d), "attendu 0,500000  mesure %.6f", (double)vif.time);
		Verdict("le meme controleur AVANCE a vitesse 1", Proche(vif.time, 0.5f, 1e-4f), d);
	}

	// =========================================================================
	// Le peintre — processeur seul, et la position vient DU MONDE
	// =========================================================================
	struct Toile {
			uint32 w = 320, h = 180;
			nkentseu::uint8 *px = nullptr; // RGB24
	};

	void PeindreDepuisLaPose(Toile &t, const nkentseu::math::NkVec3f &pos) {
		// Fond : un dégradé vertical fixe. Il ne varie PAS avec le temps — donc
		// il ne peut pas, à lui seul, faire différer deux images.
		for (uint32 y = 0; y < t.h; ++y) {
			const uint8 f = (uint8)(20u + (60u * y) / t.h);
			for (uint32 x = 0; x < t.w; ++x) {
				uint8 *p = t.px + ((usize)y * t.w + x) * 3u;
				p[0] = f;
				p[1] = f;
				p[2] = (uint8)(f + 25u);
			}
		}
		// Le carré : SA position vient de la pose évaluée. C'est le seul lien
		// entre le temps et les pixels, et c'est celui qu'on veut prouver.
		// Monde [-4, +4] en x, [-1, +3] en y -> pixels.
		const float32 cx = (pos.x + 4.f) / 8.f * (float32)t.w;
		const float32 cy = (float32)t.h - (pos.y + 1.f) / 4.f * (float32)t.h;
		const int demi = 14;
		const int ix = (int)cx, iy = (int)cy;
		for (int y = iy - demi; y <= iy + demi; ++y) {
			if (y < 0 || y >= (int)t.h)
				continue;
			for (int x = ix - demi; x <= ix + demi; ++x) {
				if (x < 0 || x >= (int)t.w)
					continue;
				uint8 *p = t.px + ((usize)y * t.w + (uint32)x) * 3u;
				p[0] = 247; // orange Rihen
				p[1] = 154;
				p[2] = 40;
			}
		}
	}

	// Rend le nombre d'images écrites. `fige` = volet négatif de f4 : le temps
	// n'avance pas, toutes les images sont peintes depuis la MÊME pose.
	int RendreSequence(nkentseu::NkSequence &seq, nkentseu::ecs::NkWorld &world, const char *dossier, bool fige) {
		const nkentseu::NkRenderOutput &out = seq.renderOutput;
		const float32 span = out.endTime - out.startTime;
		if (span <= 0.f || out.fps <= 0.f)
			return 0; // endTime == startTime -> AUCUN fichier, pas un fichier vide
		const int n = (int)(span * out.fps + 0.5f);
		if (n <= 0)
			return 0;

		nkentseu::media::NkImageSequenceWriter sw;
		if (!sw.Open(dossier, "frame", (nkentseu::int32)out.width, (nkentseu::int32)out.height,
					 nkentseu::media::NkImageSeqFormat::PNG, 4))
			return 0;

		Toile toile;
		toile.w = out.width;
		toile.h = out.height;
		const usize octets = (usize)toile.w * toile.h * 3u;
		toile.px = (uint8 *)nkentseu::memory::NkAlloc(octets);
		if (toile.px == nullptr) {
			sw.Close();
			return 0;
		}

		int ecrites = 0;
		for (int i = 0; i < n; ++i) {
			const float32 t = fige ? out.startTime : (out.startTime + (float32)i / out.fps);
			seq.Evaluate(t, world);
			const nkentseu::ecs::NkTransform *tr = world.Get<nkentseu::ecs::NkTransform>(seq.tracks[0].entity);
			const nkentseu::math::NkVec3f pos = tr ? tr->localPosition : nkentseu::math::NkVec3f{0.f, 0.f, 0.f};
			PeindreDepuisLaPose(toile, pos);
			if (sw.WriteFrame(toile.px, nkentseu::media::NkVideoInputFormat::RGB24))
				ecrites++;
		}
		nkentseu::memory::NkFree(toile.px);
		sw.Close();
		return ecrites;
	}

	int CompterFichiers(const char *dossier, const char *prefixe, int max) {
		int n = 0;
		char chemin[512];
		for (int i = 1; i <= max; ++i) {
			std::snprintf(chemin, sizeof(chemin), "%s/%s_%04d.png", dossier, prefixe, i);
			if (nkentseu::NkFile::GetFileSize(chemin) > 0)
				n++;
		}
		return n;
	}

	// =========================================================================
	// f3 et f4
	// =========================================================================
	void F3F4_LesImagesSurLeDisque() {
		std::printf("\nf3 — LA SORTIE EXISTE SUR LE DISQUE\n");
		std::printf("  attendu ECRIT AVANT LA MESURE :\n");
		std::printf("     NkRenderOutput{ fps = 24, startTime = 0, endTime = 2 } -> 48 fichiers\n");
		std::printf("     nommes frame_0001.png .. frame_0048.png, en 320x180\n");
		std::printf("     le premier : taille > 0 ET signature 89 50 4E 47 0D 0A 1A 0A\n");
		std::printf("     negatif : endTime = startTime -> 0 fichier, et AUCUN fichier vide\n\n");

		const char *dossier = "Sortie_NkSequenceCheck";
		nkentseu::NkDirectory::CreateRecursive(dossier);

		PorteScene porte_s;
		Scene &s = *porte_s;
		BatirScene(s);
		s.seq.renderOutput.width = 320;
		s.seq.renderOutput.height = 180;
		s.seq.renderOutput.fps = 24.f;
		s.seq.renderOutput.startTime = 0.f;
		s.seq.renderOutput.endTime = 2.f;

		char d[300];
		const int ecrites = RendreSequence(s.seq, s.world, dossier, false);
		std::snprintf(d, sizeof(d), "attendu 48  ecrites %d", ecrites);
		Verdict("48 images ecrites par le sequenceur", ecrites == 48, d);

		const int surDisque = CompterFichiers(dossier, "frame", 60);
		std::snprintf(d, sizeof(d), "attendu 48  trouvees %d (recherche jusqu'a 60)", surDisque);
		Verdict("48 fichiers NON VIDES sur le disque", surDisque == 48, d);

		char premier[512];
		std::snprintf(premier, sizeof(premier), "%s/frame_0001.png", dossier);
		const nkentseu::nk_int64 taille1 = nkentseu::NkFile::GetFileSize(premier);
		std::snprintf(d, sizeof(d), "%lld octets", (long long)taille1);
		Verdict("frame_0001.png n'est pas vide", taille1 > 0, d);
		Verdict("frame_0001.png porte la signature PNG", EstUnPNG(premier), "8 octets d'en-tete");

		// ── LE NEGATIF de f3 ─────────────────────────────────────────────────
		const char *dossierVide = "Sortie_NkSequenceCheck_negatif";
		nkentseu::NkDirectory::CreateRecursive(dossierVide);
		PorteScene porte_sn;
		Scene &sn = *porte_sn;
		BatirScene(sn);
		sn.seq.renderOutput.width = 320;
		sn.seq.renderOutput.height = 180;
		sn.seq.renderOutput.fps = 24.f;
		sn.seq.renderOutput.startTime = 1.f;
		sn.seq.renderOutput.endTime = 1.f; // endTime == startTime
		const int rien = RendreSequence(sn.seq, sn.world, dossierVide, false);
		const int rienSurDisque = CompterFichiers(dossierVide, "frame", 60);
		std::snprintf(d, sizeof(d), "ecrites %d, fichiers %d", rien, rienSurDisque);
		Verdict("NEGATIF : endTime = startTime -> 0 fichier", rien == 0 && rienSurDisque == 0, d);

		// =====================================================================
		std::printf("\nf4 — DEUX IMAGES DE LA SUITE DIFFERENT\n");
		std::printf("  attendu ECRIT AVANT LA MESURE :\n");
		std::printf("     empreinte(frame_0001) != empreinte(frame_0024)   [FNV-1a 64 sur tous les octets]\n");
		std::printf("     negatif : sequence FIGEE (le temps n'avance pas) -> les deux empreintes EGALES\n\n");

		char c1[512], c24[512];
		std::snprintf(c1, sizeof(c1), "%s/frame_0001.png", dossier);
		std::snprintf(c24, sizeof(c24), "%s/frame_0024.png", dossier);

		nkentseu::uint64 h1 = 0, h24 = 0, t1 = 0, t24 = 0;
		const bool lu1 = EmpreinteFichier(c1, h1, t1);
		const bool lu24 = EmpreinteFichier(c24, h24, t24);
		Verdict("les deux images se relisent", lu1 && lu24, "frame_0001 et frame_0024");

		std::snprintf(d, sizeof(d), "0x%016llx (%llu o) vs 0x%016llx (%llu o)", (unsigned long long)h1,
					  (unsigned long long)t1, (unsigned long long)h24, (unsigned long long)t24);
		Verdict("frame_0001 DIFFERE de frame_0024", lu1 && lu24 && h1 != h24, d);

		// ── LE NEGATIF de f4 : la sequence figee ─────────────────────────────
		// Même code, même peintre, même écrivain — seule la marche du temps est
		// coupée. Si les empreintes différaient quand même, c'est que quelque
		// chose d'autre que le temps fait varier les images, et f4 ne prouverait
		// plus rien.
		const char *dossierFige = "Sortie_NkSequenceCheck_fige";
		nkentseu::NkDirectory::CreateRecursive(dossierFige);
		PorteScene porte_sf;
		Scene &sf = *porte_sf;
		BatirScene(sf);
		sf.seq.renderOutput.width = 320;
		sf.seq.renderOutput.height = 180;
		sf.seq.renderOutput.fps = 24.f;
		sf.seq.renderOutput.startTime = 0.f;
		sf.seq.renderOutput.endTime = 2.f;
		const int figees = RendreSequence(sf.seq, sf.world, dossierFige, true);

		char g1[512], g24[512];
		std::snprintf(g1, sizeof(g1), "%s/frame_0001.png", dossierFige);
		std::snprintf(g24, sizeof(g24), "%s/frame_0024.png", dossierFige);
		nkentseu::uint64 f1h = 0, f24h = 0, f1t = 0, f24t = 0;
		const bool lg1 = EmpreinteFichier(g1, f1h, f1t);
		const bool lg24 = EmpreinteFichier(g24, f24h, f24t);
		std::snprintf(d, sizeof(d), "%d images figees, 0x%016llx vs 0x%016llx", figees, (unsigned long long)f1h,
					  (unsigned long long)f24h);
		Verdict("NEGATIF : sequence figee -> empreintes EGALES", lg1 && lg24 && f1h == f24h, d);
	}

	// =========================================================================
	// f5 — LES MEMES 48 IMAGES, MAIS PEINTES PAR LE GPU
	// =========================================================================
	// Ce bloc ne remplace pas f3/f4, il se pose A COTE : deux chemins qui donnent
	// le meme verdict valent mieux qu'un seul, et le chemin processeur tourne sur
	// une machine sans carte graphique.
	//
	// ⚠️ CE QUE LE GPU FAIT ICI, EXACTEMENT : il EFFACE la cible avec une couleur
	// DERIVEE DE LA POSE evaluee. Il ne dessine aucune geometrie, et c'est
	// delibere — `NkOffscreenProbe` a etabli que l'effacement suffit a prouver
	// qu'une passe s'execute et se relit, sur les quatre dorsaux. Ajouter un
	// maillage melerait deux questions (le sequenceur pilote-t-il le rendu ? le
	// pipeline compile-t-il ?) et un rouge ne dirait plus laquelle des deux a
	// cede. La geometrie est l'etape suivante, pas celle-ci.
	//
	// Le chainage prouve reste entier, et il passe cette fois par la carte :
	//   cles -> NkSequence::Evaluate -> pose ECS -> couleur -> GPU
	//        -> ReadbackPixels -> NkImageSequenceWriter -> gpu_NNNN.png
	void F5_LesImagesViennentDuGpu() {
		std::printf("\nf5 — LES MEMES IMAGES, PEINTES PAR LE GPU (chemin hors ecran)\n");
		std::printf("  attendu ECRIT AVANT LA MESURE :\n");
		std::printf("     un NkIDevice SANS surface, une NkOffscreenTarget 320x180 avec readback\n");
		std::printf("     48 fichiers gpu_0001.png .. gpu_0048.png, le premier PNG valide\n");
		std::printf("     empreinte(gpu_0001) != empreinte(gpu_0024)\n");
		std::printf("     negatif : sequence figee -> les deux empreintes EGALES\n");
		std::printf("  Le GPU EFFACE avec une couleur derivee de la pose ; il ne dessine aucune\n");
		std::printf("  geometrie. C'est le pilote temporel qu'on mesure, pas le pipeline.\n\n");

		nkentseu::NkDeviceInitInfo di;
		di.api = nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL;
		di.context.software.threading = true;
		nkentseu::NkIDevice *dev = nkentseu::NkDeviceFactory::Create(di);
		if (dev == nullptr || !dev->IsValid()) {
			// ── NI VERT NI ROUGE ─────────────────────────────────────────────
			// Une machine sans carte graphique ne doit pas faire rougir un banc
			// qui mesure un sequenceur. On le dit, on ne compte pas, et le bilan
			// reste lisible.
			std::printf("  [IGN] aucun peripherique graphique — f5 n'est ni vert ni rouge.\n");
			std::printf("        (le chemin processeur a deja rendu son verdict ci-dessus)\n");
			if (dev)
				nkentseu::NkDeviceFactory::Destroy(dev);
			return;
		}

		char d[300];
		nkentseu::renderer::NkTextureLibrary texlib;
		if ((nkentseu::int32)texlib.Init(dev) < 0) {
			Verdict("f5 la bibliotheque de textures s'initialise", false, "Init refuse");
			nkentseu::NkDeviceFactory::Destroy(dev);
			return;
		}

		nkentseu::renderer::NkOffscreenDesc od;
		od.width = 320;
		od.height = 180;
		od.hasDepth = true;
		od.readable = true;
		od.readback = true;
		od.name = "SequenceFilm";

		nkentseu::renderer::NkOffscreenTarget cible;
		if (!cible.Init(dev, &texlib, od) || !cible.IsValid()) {
			Verdict("f5 la cible hors ecran s'initialise", false, "Init refuse");
			texlib.Shutdown();
			nkentseu::NkDeviceFactory::Destroy(dev);
			return;
		}
		Verdict("f5 device + cible hors ecran, sans fenetre", true, "OpenGL, 320x180, readback");

		const char *dossier = "Sortie_NkSequenceCheck_gpu";
		const char *dossierFige = "Sortie_NkSequenceCheck_gpu_fige";
		nkentseu::NkDirectory::CreateRecursive(dossier);
		nkentseu::NkDirectory::CreateRecursive(dossierFige);

		const usize octets = (usize)od.width * od.height * 4u;
		uint8 *px = (uint8 *)nkentseu::memory::NkAlloc(octets);
		if (px == nullptr) {
			Verdict("f5 tampon de relecture", false, "allocation refusee");
			cible.Shutdown();
			texlib.Shutdown();
			nkentseu::NkDeviceFactory::Destroy(dev);
			return;
		}

		int ecrites[2] = {0, 0};
		for (int passe = 0; passe < 2; ++passe) {
			const bool fige = (passe == 1);
			PorteScene porte;
			Scene &sc = *porte;
			BatirScene(sc);

			nkentseu::media::NkImageSequenceWriter sw;
			if (!sw.Open(fige ? dossierFige : dossier, "gpu", (nkentseu::int32)od.width, (nkentseu::int32)od.height,
						 nkentseu::media::NkImageSeqFormat::PNG, 4))
				break;

			for (int i = 0; i < 48; ++i) {
				const float32 t = fige ? 0.f : ((float32)i / 24.f);
				sc.seq.Evaluate(t, sc.world);
				const nkentseu::ecs::NkTransform *tr =
					sc.world.Get<nkentseu::ecs::NkTransform>(sc.seq.tracks[0].entity);
				const nkentseu::math::NkVec3f p =
					tr ? tr->localPosition : nkentseu::math::NkVec3f(0.f, 0.f, 0.f);
				// LA COULEUR EST LA POSE. x dans [-3,+3] -> rouge, y dans [0,2] ->
				// vert. Lue dans le monde ECS APRES Evaluate, jamais recalculee a
				// cote : meme verrou que le carre du chemin processeur. Sans ce
				// lien, f5 passerait au vert sans rien prouver.
				const float32 r = (p.x + 3.f) / 6.f;
				const float32 g = p.y / 2.f;
				nkentseu::NkICommandBuffer *cmd = dev->CreateCommandBuffer();
				if (cmd == nullptr || !cmd->Begin())
					break;
				cible.BeginCapture(cmd, true, nkentseu::math::NkVec4f(r, g, 0.35f, 1.f), true);
				cible.EndCapture(cmd);
				cmd->End();
				dev->Submit(&cmd, 1);
				dev->WaitIdle();
				if (!cible.ReadbackPixels(px, od.width * 4u))
					break;
				if (sw.WriteFrame(px, nkentseu::media::NkVideoInputFormat::RGBA32))
					ecrites[passe]++;
			}
			sw.Close();
		}

		std::snprintf(d, sizeof(d), "attendu 48  ecrites %d", ecrites[0]);
		Verdict("f5 48 images rendues par le GPU", ecrites[0] == 48, d);

		const int surDisque = CompterFichiers(dossier, "gpu", 60);
		std::snprintf(d, sizeof(d), "attendu 48  trouvees %d", surDisque);
		Verdict("f5 48 fichiers NON VIDES sur le disque", surDisque == 48, d);

		char c1[512], c24[512];
		std::snprintf(c1, sizeof(c1), "%s/gpu_0001.png", dossier);
		std::snprintf(c24, sizeof(c24), "%s/gpu_0024.png", dossier);
		Verdict("f5 gpu_0001.png porte la signature PNG", EstUnPNG(c1), "8 octets d'en-tete");

		nkentseu::uint64 h1 = 0, h24 = 0, t1 = 0, t24 = 0;
		const bool lu = EmpreinteFichier(c1, h1, t1) && EmpreinteFichier(c24, h24, t24);
		std::snprintf(d, sizeof(d), "0x%016llx (%llu o) vs 0x%016llx (%llu o)", (unsigned long long)h1,
					  (unsigned long long)t1, (unsigned long long)h24, (unsigned long long)t24);
		Verdict("f5 gpu_0001 DIFFERE de gpu_0024", lu && h1 != h24, d);

		char g1[512], g24[512];
		std::snprintf(g1, sizeof(g1), "%s/gpu_0001.png", dossierFige);
		std::snprintf(g24, sizeof(g24), "%s/gpu_0024.png", dossierFige);
		nkentseu::uint64 f1h = 0, f24h = 0, f1t = 0, f24t = 0;
		const bool lug = EmpreinteFichier(g1, f1h, f1t) && EmpreinteFichier(g24, f24h, f24t);
		std::snprintf(d, sizeof(d), "%d images figees, 0x%016llx vs 0x%016llx", ecrites[1], (unsigned long long)f1h,
					  (unsigned long long)f24h);
		Verdict("f5 NEGATIF : sequence figee -> empreintes EGALES", lug && f1h == f24h, d);

		nkentseu::memory::NkFree(px);
		cible.Shutdown();
		texlib.Shutdown();
		nkentseu::NkDeviceFactory::Destroy(dev);
	}

} // namespace

int main() {
#if defined(_WIN32)
	// GARDE : cette fenetre n'est pas le produit. Elle doit le dire dans son
	// titre, pour qu'une capture prise par erreur ne puisse pas etre confondue
	// avec l'application de Rodolf.
	SetConsoleTitleA("*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT ***");
#endif
	std::printf("=============================================================================\n");
	std::printf(" *** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT ***\n");
	std::printf(" NkSequenceCheck — le sequenceur de Noge : des cles entrent, des images sortent\n");
	std::printf("-----------------------------------------------------------------------------\n");
	std::printf(" DEUX CHEMINS, VOLONTAIREMENT GARDES COTE A COTE :\n");
	std::printf("   f1-f4 : les pixels sont PEINTS PAR LE PROCESSEUR. Aucun GPU requis — ce\n");
	std::printf("           chemin tourne sur une machine sans carte graphique.\n");
	std::printf("   f5    : les MEMES images, effacees par le GPU via NkOffscreenTarget, sans\n");
	std::printf("           fenetre. Si aucun peripherique n'est disponible, f5 est IGNORE —\n");
	std::printf("           ni vert ni rouge, et le bilan le dit.\n");
	std::printf(" Dans les deux cas la couleur/position vient de la POSE lue dans le monde ECS\n");
	std::printf(" APRES Evaluate. Sans ce lien, les criteres passeraient au vert sans rien\n");
	std::printf(" prouver. Aucun des deux chemins ne dessine de geometrie : c'est le PILOTE\n");
	std::printf(" TEMPOREL qu'on mesure, pas le pipeline de rendu.\n");
	std::printf("=============================================================================\n");

	// La taille qui a fait deborder la pile, MESUREE et affichee. La premiere
	// execution est morte en 0xC00000FD avant sa premiere ligne de sortie ; dire
	// « NkWorld est sans doute gros » n'aurait pas ete une mesure.
	std::printf(" sizeof(NkWorld) = %llu o   sizeof(NkSequence) = %llu o   sizeof(Scene) = %llu o\n",
				(unsigned long long)sizeof(nkentseu::ecs::NkWorld), (unsigned long long)sizeof(nkentseu::NkSequence),
				(unsigned long long)sizeof(Scene));
	std::printf(" -> les scenes vivent sur le TAS, jamais sur la pile.\n");
	std::printf("=============================================================================\n");

	F1_ClesEtRelecture();
	F2_LeTempsChangeLaPose();
	F3F4_LesImagesSurLeDisque();
	F5_LesImagesViennentDuGpu();

	std::printf("\n-----------------------------------------------------------------------------\n");
	std::printf(" BILAN : %d verts, %d rouges\n", gPass, gFail);
	std::printf(" Status: %s\n", (gFail == 0) ? "SUCCESS" : "FAILURE");
	std::printf("-----------------------------------------------------------------------------\n");
	return (gFail == 0) ? 0 : 1;
}
