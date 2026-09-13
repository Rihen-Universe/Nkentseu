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
#include "NKRenderer/Shader/NkShaderLibrary.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"

#include "Noge/Sequencer/NkSequencer.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "NKECS/World/NkWorld.h"
#include "NKMedia/Video/NkImageSequenceWriter.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKMemory/NKMemory.h"

#include <cstdio>
#include <cstdlib> // system() : f7 relance CE MEME exe pour obtenir un processus NEUF

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

	// `s` commence-t-il par `p` ? Sans <cstring>, comme le reste du depot.
	bool Prefixe(const char *s, const char *p) {
		uint32 i = 0;
		while (p[i] != '\0') {
			if (s[i] != p[i])
				return false;
			i++;
		}
		return true;
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

	// =========================================================================
	// f6 — LE GPU DESSINE UN MAILLAGE, il n'efface plus seulement
	// =========================================================================
	// C'est la premiere fois que ces 48 images prouvent quelque chose du PIPELINE
	// de rendu : un nuanceur compile, une geometrie est transmise, un tirage a
	// lieu. f5 n'exercait que la passe et la relecture.
	//
	// LA GEOMETRIE : un quad (deux triangles) dont le CENTRE vient de la pose.
	// Ses sommets sont reecrits a chaque image dans un tampon DYNAMIQUE — pas
	// d'uniforme, pas de matrice, pas de descripteur : moins de pieces, donc un
	// rouge qui designe une seule cause.
	//
	// ⚠️ LE PIEGE DU COMPTEUR DE PIXELS, ET COMMENT IL EST DESAMORCE.
	// Un autre chantier a mesure ce soir un compteur qui rendait 40 pixels SANS
	// AUCUNE CIBLE a l'ecran : la teinte cherchee tombait dans l'anticrenelage
	// des glyphes. Trois parades ici, et la troisieme est la seule qui vaille :
	//   1. le fond est MAGENTA pur, une couleur que l'objet (blanc) ne produit
	//      jamais — motif repris de NkMatGraphDemo, ou un fond noir se serait
	//      confondu avec « le graphe a rendu du noir » ;
	//   2. la cible est en UNORM et non en sRGB, donc l'effacement (1, 0, 1)
	//      revient exactement en (255, 0, 255) — un attendu calculable, pas un
	//      « ca ressemble » ;
	//   3. ON COMPTE D'ABORD SUR LA SCENE SANS L'OBJET, et on exige ZERO. Sans
	//      ce controle, un compteur qui compterait n'importe quoi passerait tous
	//      les autres criteres.
	//
	// Un seul dorsal (OpenGL) : la divergence sRGB mesuree par NkOffscreenProbe
	// entre OpenGL et les trois autres n'est pas tranchee, et comparer des images
	// entre dorsaux comparerait aussi cet ecart-la sans le dire.

	// Un sommet : sa position EST deja en espace de clip.
	struct SommetQuad {
			float32 pos[3];
	};

	// ⚠️ UN SEUL ATTRIBUT, ET IL S'APPELLE `aPos`. Le generateur HLSL deduit la
	// SEMANTIQUE d'un attribut de son NOM DE VARIABLE : « apos » donne POSITION.
	// Un autre nom retomberait sur TEXCOORD<location>, le layout C++ ne
	// correspondrait plus au nuanceur, et RIEN ne le dirait. (Regle etablie par
	// NkMatGraphDemo, reprise telle quelle.)
	const char *const kVS = R"NKSL(
@location(0) in vec3 aPos;

@stage(vertex)
@entry
void main() {
    gl_Position = vec4(aPos, 1.0);
}
)NKSL";

	const char *const kFS = R"NKSL(
@location(0) out vec4 fragColor;

@stage(fragment)
@entry
void main() {
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)NKSL";

	// Compte les pixels qui NE SONT PAS le fond magenta exact.
	uint32 ComptePixelsObjet(const uint8 *px, uint32 w, uint32 h) {
		uint32 n = 0;
		for (usize i = 0; i < (usize)w * h; ++i) {
			const uint8 r = px[i * 4 + 0], g = px[i * 4 + 1], b = px[i * 4 + 2];
			if (!(r == 255 && g == 0 && b == 255))
				n++;
		}
		return n;
	}

	void F6_LeGpuDessine() {
		std::printf("\nf6 — LE GPU DESSINE UN MAILLAGE (le pipeline entre en jeu)\n");
		std::printf("  attendu ECRIT AVANT LA MESURE :\n");
		std::printf("     quad de demi-cote 0,25 en NDC sur une cible 320x180 en UNORM\n");
		std::printf("       -> 0,5 x 320/2 = 80 px de large, 0,5 x 180/2 = 45 px de haut = 3600 px\n");
		std::printf("     centre pilote par la pose : cx = x/3 va de -1 a +1, le quad sort donc\n");
		std::printf("       a moitie du cadre aux deux extremites -> environ 1800 px\n");
		std::printf("     CONTROLE D'ABORD : la meme scene SANS l'objet doit compter EXACTEMENT 0\n");
		std::printf("     48 images, compte au milieu > compte au debut, et min != max\n");
		std::printf("     negatif : sequence figee -> images identiques ET compte CONSTANT\n");
		std::printf("  Un seul dorsal (OpenGL) : la divergence sRGB entre dorsaux n'est pas\n");
		std::printf("  tranchee, comparer des images entre eux comparerait aussi cet ecart.\n\n");

		nkentseu::NkDeviceInitInfo di;
		di.api = nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL;
		di.context.software.threading = true;
		nkentseu::NkIDevice *dev = nkentseu::NkDeviceFactory::Create(di);
		if (dev == nullptr || !dev->IsValid()) {
			std::printf("  [IGN] aucun peripherique graphique — f6 n'est ni vert ni rouge.\n");
			if (dev)
				nkentseu::NkDeviceFactory::Destroy(dev);
			return;
		}

		char d[300];
		const uint32 W = 320, H = 180;
		nkentseu::renderer::NkTextureLibrary texlib;
		nkentseu::renderer::NkShaderLibrary shaders;
		nkentseu::renderer::NkOffscreenTarget cible;
		bool monte = ((nkentseu::int32)texlib.Init(dev, nullptr) >= 0) &&
					 shaders.Init(dev, dev->GetApi(), /*useNkSL=*/true);

		nkentseu::renderer::NkOffscreenDesc od;
		od.width = W;
		od.height = H;
		// PAS DE PROFONDEUR : un quad, rien a trier. Une passe sans attachement de
		// profondeur evite d'avoir a accorder l'etat du pipeline avec la passe.
		od.hasDepth = false;
		// ⚠️ UNORM, PAS sRGB : sans cela le magenta d'effacement ne reviendrait pas
		// en (255, 0, 255) et le compteur n'aurait plus d'attendu calculable.
		od.colorFmt = nkentseu::NkGPUFormat::NK_RGBA8_UNORM;
		od.readable = true;
		od.readback = true;
		od.name = "SequenceGeometrie";
		monte = monte && cible.Init(dev, &texlib, od) && cible.IsValid();
		if (!monte) {
			Verdict("f6 device + nuanceurs + cible", false, "montage refuse");
			cible.Shutdown();
			shaders.Shutdown();
			texlib.Shutdown();
			nkentseu::NkDeviceFactory::Destroy(dev);
			return;
		}

		nkentseu::NkBufferHandle vbo = dev->CreateBuffer(nkentseu::NkBufferDesc::VertexDynamic(6 * sizeof(SommetQuad)));
		nkentseu::NkShaderHandle prog = shaders.CompileVF(nkentseu::NkString(kVS), nkentseu::NkString(kFS),
														 nkentseu::NkString("SequenceQuad"));
		nkentseu::NkShaderHandle rhi = shaders.GetRHIHandle(prog);
		if (!vbo.IsValid() || !rhi.IsValid()) {
			Verdict("f6 le nuanceur NkSL compile et la geometrie se cree", false,
					!rhi.IsValid() ? "CompileVF refuse" : "tampon de sommets refuse");
			cible.Shutdown();
			shaders.Shutdown();
			texlib.Shutdown();
			nkentseu::NkDeviceFactory::Destroy(dev);
			return;
		}
		Verdict("f6 le nuanceur NkSL compile et la geometrie se cree", true, "vertex + fragment, tampon dynamique");

		nkentseu::NkGraphicsPipelineDesc pd;
		pd.shader = rhi;
		pd.vertexLayout.AddBinding(0, (uint32)sizeof(SommetQuad))
			.AddAttribute(0, 0, nkentseu::NkGPUFormat::NK_RGB32_FLOAT, 0, "POSITION", 0);
		// Aucun cull : un enroulement mal oriente rendrait une image UNIFORME sans
		// le moindre message, et le compteur tomberait a zero pour une raison qui
		// n'a rien a voir avec la sequence.
		pd.rasterizer.cullMode = nkentseu::NkCullMode::NK_NONE;
		pd.depthStencil = nkentseu::NkDepthStencilDesc::NoDepth();
		pd.renderPass = cible.GetRP();
		pd.debugName = "SequenceQuad";
		nkentseu::NkPipelineHandle pipe = dev->CreateGraphicsPipeline(pd);
		if (!pipe.IsValid()) {
			Verdict("f6 le pipeline graphique se cree", false, "CreateGraphicsPipeline refuse");
			cible.Shutdown();
			shaders.Shutdown();
			texlib.Shutdown();
			nkentseu::NkDeviceFactory::Destroy(dev);
			return;
		}
		Verdict("f6 le pipeline graphique se cree", true, "sans cull, sans profondeur, UNORM");

		const usize octets = (usize)W * H * 4u;
		uint8 *px = (uint8 *)nkentseu::memory::NkAlloc(octets);
		if (px == nullptr) {
			Verdict("f6 tampon de relecture", false, "allocation refusee");
			dev->DestroyPipeline(pipe);
			cible.Shutdown();
			shaders.Shutdown();
			texlib.Shutdown();
			nkentseu::NkDeviceFactory::Destroy(dev);
			return;
		}

		// Rend une image. `avecObjet == false` = la scene SANS le quad.
		auto RendreUne = [&](float32 cx, float32 cy, bool avecObjet) -> bool {
			if (avecObjet) {
				const float32 k = 0.25f;
				const SommetQuad s[6] = {
					{{cx - k, cy - k, 0.f}}, {{cx + k, cy - k, 0.f}}, {{cx + k, cy + k, 0.f}},
					{{cx - k, cy - k, 0.f}}, {{cx + k, cy + k, 0.f}}, {{cx - k, cy + k, 0.f}},
				};
				dev->WriteBuffer(vbo, s, sizeof(s));
			}
			nkentseu::NkICommandBuffer *cmd = dev->CreateCommandBuffer();
			if (cmd == nullptr || !cmd->Begin())
				return false;
			// Fond MAGENTA, jamais noir : la couleur de repli doit etre une couleur
			// que l'objet ne peut pas produire.
			cible.BeginCapture(cmd, true, nkentseu::math::NkVec4f(1.f, 0.f, 1.f, 1.f), false);
			if (avecObjet) {
				cmd->BindGraphicsPipeline(pipe);
				cmd->BindVertexBuffer(0, vbo);
				cmd->Draw(6);
			}
			cible.EndCapture(cmd);
			cmd->End();
			dev->Submit(&cmd, 1);
			dev->WaitIdle();
			for (usize i = 0; i < octets; ++i)
				px[i] = 0;
			return cible.ReadbackPixels(px, W * 4u);
		};

		// ── LE CONTROLE, AVANT TOUT LE RESTE ─────────────────────────────────
		// « Compte tes pixels sur un cas ou tu connais la reponse d'avance. »
		// Ici la reponse est ZERO, et elle est exigee EXACTE — pas « faible ».
		const bool luVide = RendreUne(0.f, 0.f, false);
		const uint32 nVide = luVide ? ComptePixelsObjet(px, W, H) : 0xFFFFFFFFu;
		std::snprintf(d, sizeof(d), "attendu 0 EXACT  mesure %u sur %u pixels", (unsigned)nVide, (unsigned)(W * H));
		Verdict("f6 CONTROLE : la scene SANS objet compte 0", luVide && nVide == 0u, d);
		if (!luVide || nVide != 0u) {
			std::printf("       ⚠️ LE COMPTEUR MESURE AUTRE CHOSE QUE L'OBJET. Tout critere\n"
						"          bati dessus serait faux. On s'arrete ici.\n");
			nkentseu::memory::NkFree(px);
			dev->DestroyPipeline(pipe);
			cible.Shutdown();
			shaders.Shutdown();
			texlib.Shutdown();
			nkentseu::NkDeviceFactory::Destroy(dev);
			return;
		}

		// ── Les 48 images, puis les 48 du volet negatif ──────────────────────
		const char *dossier = "Sortie_NkSequenceCheck_geo";
		const char *dossierFige = "Sortie_NkSequenceCheck_geo_fige";
		nkentseu::NkDirectory::CreateRecursive(dossier);
		nkentseu::NkDirectory::CreateRecursive(dossierFige);

		uint32 compte[2][48] = {{0}, {0}};
		int ecrites[2] = {0, 0};
		for (int passe = 0; passe < 2; ++passe) {
			const bool fige = (passe == 1);
			PorteScene porte;
			Scene &sc = *porte;
			BatirScene(sc);
			nkentseu::media::NkImageSequenceWriter sw;
			if (!sw.Open(fige ? dossierFige : dossier, "geo", (nkentseu::int32)W, (nkentseu::int32)H,
						 nkentseu::media::NkImageSeqFormat::PNG, 4))
				break;
			for (int i = 0; i < 48; ++i) {
				const float32 t = fige ? 0.f : ((float32)i / 24.f);
				sc.seq.Evaluate(t, sc.world);
				const nkentseu::ecs::NkTransform *tr =
					sc.world.Get<nkentseu::ecs::NkTransform>(sc.seq.tracks[0].entity);
				const nkentseu::math::NkVec3f p =
					tr ? tr->localPosition : nkentseu::math::NkVec3f(0.f, 0.f, 0.f);
				// LA GEOMETRIE EST LA POSE, lue dans le monde ECS apres Evaluate.
				if (!RendreUne(p.x / 3.f, p.y / 2.f - 0.5f, true))
					break;
				compte[passe][i] = ComptePixelsObjet(px, W, H);
				if (sw.WriteFrame(px, nkentseu::media::NkVideoInputFormat::RGBA32))
					ecrites[passe]++;
			}
			sw.Close();
		}

		std::snprintf(d, sizeof(d), "attendu 48  ecrites %d", ecrites[0]);
		Verdict("f6 48 images dessinees par le GPU", ecrites[0] == 48, d);

		// Le compte doit VARIER : un objet qui bouge ne couvre pas toujours la
		// meme surface. C'est ce critere, et non la difference d'empreintes, qui
		// distingue « la geometrie a bouge » de « la couleur a change ».
		uint32 mini = 0xFFFFFFFFu, maxi = 0;
		for (int i = 0; i < 48; ++i) {
			if (compte[0][i] < mini)
				mini = compte[0][i];
			if (compte[0][i] > maxi)
				maxi = compte[0][i];
		}
		std::snprintf(d, sizeof(d), "img1 %u | img24 %u | img48 %u | min %u max %u", (unsigned)compte[0][0],
					  (unsigned)compte[0][23], (unsigned)compte[0][47], (unsigned)mini, (unsigned)maxi);
		Verdict("f6 la surface couverte VARIE au cours de la sequence", maxi > mini, d);

		std::snprintf(d, sizeof(d), "attendu ~3600 au milieu, ~1800 aux bords  mesure max %u min %u", (unsigned)maxi,
					  (unsigned)mini);
		Verdict("f6 les comptes tombent dans l'attendu ecrit d'avance", maxi >= 3000u && maxi <= 4200u &&
																			mini >= 1300u && mini <= 2400u,
				d);

		std::snprintf(d, sizeof(d), "img24 %u > img1 %u", (unsigned)compte[0][23], (unsigned)compte[0][0]);
		Verdict("f6 le quad est PLUS visible au milieu qu'au depart", compte[0][23] > compte[0][0], d);

		char c1[512], c24[512];
		std::snprintf(c1, sizeof(c1), "%s/geo_0001.png", dossier);
		std::snprintf(c24, sizeof(c24), "%s/geo_0024.png", dossier);
		nkentseu::uint64 h1 = 0, h24 = 0, t1 = 0, t24 = 0;
		const bool lu = EmpreinteFichier(c1, h1, t1) && EmpreinteFichier(c24, h24, t24);
		std::snprintf(d, sizeof(d), "0x%016llx vs 0x%016llx", (unsigned long long)h1, (unsigned long long)h24);
		Verdict("f6 geo_0001 DIFFERE de geo_0024", lu && h1 != h24, d);

		// ── LE NEGATIF : images identiques ET compte constant ────────────────
		bool compteConstant = true;
		for (int i = 1; i < 48; ++i)
			if (compte[1][i] != compte[1][0])
				compteConstant = false;
		char g1[512], g24[512];
		std::snprintf(g1, sizeof(g1), "%s/geo_0001.png", dossierFige);
		std::snprintf(g24, sizeof(g24), "%s/geo_0024.png", dossierFige);
		nkentseu::uint64 f1h = 0, f24h = 0, f1t = 0, f24t = 0;
		const bool lug = EmpreinteFichier(g1, f1h, f1t) && EmpreinteFichier(g24, f24h, f24t);
		std::snprintf(d, sizeof(d), "empreintes %s, compte fige %u constant=%s",
					  (lug && f1h == f24h) ? "EGALES" : "DIFFERENTES", (unsigned)compte[1][0],
					  compteConstant ? "oui" : "NON");
		Verdict("f6 NEGATIF : figee -> images identiques ET compte constant", lug && f1h == f24h && compteConstant, d);

		nkentseu::memory::NkFree(px);
		dev->DestroyPipeline(pipe);
		cible.Shutdown();
		shaders.Shutdown();
		texlib.Shutdown();
		nkentseu::NkDeviceFactory::Destroy(dev);
	}

	// =========================================================================
	// f7 — LA SEQUENCE S'ENREGISTRE ET SE RELIT (format .nkseq)
	// =========================================================================
	// Le critere n'est pas « le fichier se relit sans planter » : c'est
	// ecrire -> relire DANS UN PROCESSUS NEUF -> reecrire -> les deux fichiers
	// identiques OCTET A OCTET. Un aller-retour dans le meme processus prouve
	// beaucoup moins : la memoire encore chaude masque les champs oublies.
	//
	// ⚠️ LA MESURE PASSE PAR `ReadAllBytes`, JAMAIS PAR UN ALLER-RETOUR TEXTE.
	// `NkFile::WriteAllText` ecrit en CRLF et `ReadAllText` renormalise : l'ecart
	// est masque des DEUX cotes a la fois, si bien qu'une comparaison de chaines
	// declare « identiques » deux fichiers qui ne le sont pas. Un chantier voisin
	// a vu deux journaux annoncer 1159 et 1110 octets pour le meme fichier.
	//
	// Et le critere qui separe « j'ai relu des donnees » de « j'ai relu une
	// SEQUENCE » : la sequence relue doit rendre LES 48 MEMES IMAGES, empreintes
	// comprises. Des nombres qui reviennent ne prouvent pas qu'ils sont a leur
	// place.

	// Une sequence RICHE : tout ce que le format pretend porter, y compris ce que
	// le reste du banc n'emploie pas (clips, plans camera, marqueurs, piste NLA,
	// renderOutput non par defaut). Un format ne se teste pas sur le
	// sous-ensemble qu'on utilise.
	void EnrichirPourSerialisation(Scene &sc) {
		nkentseu::NkClipOnTrack &cl = sc.seq.tracks[0].AddClip(0x0123456789ABCDEFull, 0.25f, 1.5f);
		cl.blendIn = 0.1f;
		cl.blendOut = 0.2f;
		cl.loop = true;

		sc.seq.cameraTrack.AddShot(sc.sujet, 0.f, 1.f, nkentseu::NkCutType::Cut);
		sc.seq.cameraTrack.AddShot(sc.sujet, 1.f, 1.f, nkentseu::NkCutType::Blend);
		sc.seq.cameraTrack.shots[1].blendDuration = 0.33f;

		sc.seq.AddMarker(0.5f, "debut du plan", nkentseu::NkMarker::Type::Scene);
		sc.seq.AddMarker(1.75f, "fin", nkentseu::NkMarker::Type::Note);
		sc.seq.AddNLATrack(sc.sujet, "nla");

		sc.seq.renderOutput.width = 320;
		sc.seq.renderOutput.height = 180;
		sc.seq.renderOutput.fps = 24.f;
		sc.seq.renderOutput.startTime = 0.f;
		sc.seq.renderOutput.endTime = 2.f;
		sc.seq.renderOutput.outputDirectory = "un/dossier/quelconque";
		sc.seq.renderOutput.jpegQuality = 77;
		sc.seq.renderOutput.motionBlurSamples = 9;
		sc.seq.renderOutput.renderDOF = true;
		sc.seq.renderOutput.renderSSAO = false;
	}

	// Le mode « processus neuf ». Il relit, reecrit, rend les 48 images — et RIEN
	// d'autre : aucun autre critere, aucune scene batie en memoire. C'est ce qui
	// en fait une preuve : tout ce qu'il produit vient du FICHIER.
	int ModeRelecture(const char *entree, const char *sortie, const char *dossierImages) {
		nkentseu::NkSequence seq;
		if (!seq.LoadFromFile(entree)) {
			std::printf("[relecture] REFUS : %s\n", nkentseu::NkSequenceDernierRefus());
			return 2;
		}
		if (sortie != nullptr && !seq.SaveToFile(sortie)) {
			std::printf("[relecture] reecriture refusee : %s\n", nkentseu::NkSequenceDernierRefus());
			return 3;
		}
		if (dossierImages != nullptr) {
			nkentseu::NkDirectory::CreateRecursive(dossierImages);
			PorteScene porte;
			Scene &sc = *porte;
			// Le monde est NEUF. On y cree une entite et on verifie qu'elle porte
			// le MEME identifiant que celui ecrit dans la sequence : sinon la
			// piste viserait une entite inexistante, Evaluate ne toucherait rien,
			// les 48 images seraient identiques, et l'echec ressemblerait a un
			// probleme de rendu alors qu'il serait un probleme d'identite.
			const nkentseu::NkEntityId e = sc.world.CreateEntity();
			sc.world.Add<nkentseu::ecs::NkTransform>(e, nkentseu::ecs::NkTransform{});
			if (seq.tracks.Size() > 0 && seq.tracks[0].entity != e) {
				std::printf("[relecture] entite relue (%u:%u) != entite neuve (%u:%u)\n",
							(unsigned)seq.tracks[0].entity.index, (unsigned)seq.tracks[0].entity.gen, (unsigned)e.index,
							(unsigned)e.gen);
				return 4;
			}
			const int n = RendreSequence(seq.renderOutput.endTime > 0.f ? seq : seq, sc.world, dossierImages, false);
			std::printf("[relecture] %d images rendues depuis le fichier\n", n);
			if (n != 48)
				return 5;
		}
		std::printf("[relecture] OK\n");
		return 0;
	}

	// Compare deux fichiers OCTET A OCTET. Rend -1 s'ils font la meme taille et
	// le meme contenu ; sinon l'indice du premier ecart, ou -2 si une taille
	// differe (et alors `tailleA`/`tailleB` le disent).
	int PremierEcart(const char *a, const char *b, nkentseu::uint64 &tailleA, nkentseu::uint64 &tailleB) {
		nkentseu::NkVector<nkentseu::uint8> da = nkentseu::NkFile::ReadAllBytes(a);
		nkentseu::NkVector<nkentseu::uint8> db = nkentseu::NkFile::ReadAllBytes(b);
		tailleA = (nkentseu::uint64)da.Size();
		tailleB = (nkentseu::uint64)db.Size();
		if (tailleA != tailleB)
			return -2;
		for (nkentseu::uint32 i = 0; i < da.Size(); ++i)
			if (da[i] != db[i])
				return (int)i;
		return -1;
	}

	void F7_LaSequenceSEnregistre(const char *exe) {
		std::printf("\nf7 — LA SEQUENCE S'ENREGISTRE ET SE RELIT (format .nkseq)\n");
		std::printf("  attendu ECRIT AVANT LA MESURE :\n");
		std::printf("     ecrire A -> relire A DANS UN PROCESSUS NEUF -> reecrire B\n");
		std::printf("     A et B identiques OCTET A OCTET, mesures par ReadAllBytes\n");
		std::printf("     (jamais par ReadAllText : WriteAllText ecrit en CRLF et ReadAllText\n");
		std::printf("      renormalise, l'ecart serait masque des DEUX cotes a la fois)\n");
		std::printf("     la sequence relue rend LES 48 MEMES IMAGES, empreintes comprises\n");
		std::printf("     negatif : un octet abime AU MILIEU -> refus NOMME, jamais une\n");
		std::printf("               sequence a moitie lue\n\n");

		char d[400];
		const char *fA = "seq_A.nkseq";
		const char *fB = "seq_B.nkseq";
		const char *fC = "seq_C_abime.nkseq";
		const char *dImg = "Sortie_NkSequenceCheck_relue";

		PorteScene porte;
		Scene &sc = *porte;
		BatirScene(sc);
		EnrichirPourSerialisation(sc);
		sc.seq.RecalcDuration();

		const bool ecrit = sc.seq.SaveToFile(fA);
		const nkentseu::nk_int64 tA = nkentseu::NkFile::GetFileSize(fA);
		std::snprintf(d, sizeof(d), "%lld octets%s%s", (long long)tA, ecrit ? "" : " — refus : ",
					  ecrit ? "" : nkentseu::NkSequenceDernierRefus());
		Verdict("f7 la sequence s'ecrit sur le disque", ecrit && tA > 24, d);
		if (!ecrit)
			return;

		// ── LE PROCESSUS NEUF ────────────────────────────────────────────────
		char cmd[1200];
		std::snprintf(cmd, sizeof(cmd), "\"\"%s\" --relire=%s --reecrire=%s --images=%s\" > relecture.log 2>&1", exe,
					  fA, fB, dImg);
		const int codeRelecture = std::system(cmd);
		std::snprintf(d, sizeof(d), "code %d (0 attendu) — voir relecture.log", codeRelecture);
		Verdict("f7 un PROCESSUS NEUF relit le fichier et le reecrit", codeRelecture == 0, d);

		nkentseu::uint64 ta = 0, tb = 0;
		const int ecart = PremierEcart(fA, fB, ta, tb);
		std::snprintf(d, sizeof(d), "A %llu o, B %llu o, %s", (unsigned long long)ta, (unsigned long long)tb,
					  ecart == -1 ? "aucun ecart" : (ecart == -2 ? "TAILLES DIFFERENTES" : "ecart au milieu"));
		Verdict("f7 A et B identiques OCTET A OCTET (ReadAllBytes)", ecart == -1 && ta > 24, d);
		if (ecart >= 0)
			std::printf("       premier octet different a l'offset %d\n", ecart);

		// ── LE CRITERE QUI COMPTE : les memes 48 images ──────────────────────
		char r1[512], r24[512], o1[512], o24[512];
		std::snprintf(r1, sizeof(r1), "%s/frame_0001.png", dImg);
		std::snprintf(r24, sizeof(r24), "%s/frame_0024.png", dImg);
		std::snprintf(o1, sizeof(o1), "Sortie_NkSequenceCheck/frame_0001.png");
		std::snprintf(o24, sizeof(o24), "Sortie_NkSequenceCheck/frame_0024.png");
		nkentseu::uint64 hr1 = 0, hr24 = 0, ho1 = 0, ho24 = 0, z = 0;
		const bool lus = EmpreinteFichier(r1, hr1, z) && EmpreinteFichier(r24, hr24, z) &&
						 EmpreinteFichier(o1, ho1, z) && EmpreinteFichier(o24, ho24, z);
		std::snprintf(d, sizeof(d), "relue 0x%016llx / 0x%016llx  contre  memoire 0x%016llx / 0x%016llx",
					  (unsigned long long)hr1, (unsigned long long)hr24, (unsigned long long)ho1,
					  (unsigned long long)ho24);
		Verdict("f7 la sequence RELUE rend LES MEMES 48 images", lus && hr1 == ho1 && hr24 == ho24, d);

		// ── LE NEGATIF : un octet abime AU MILIEU ────────────────────────────
		// Au milieu, jamais dans l'en-tete : abimer la magie prouverait seulement
		// qu'on sait lire quatre octets. Le seul controle capable de voir un
		// octet change au coeur des donnees est l'empreinte du corps.
		nkentseu::NkVector<nkentseu::uint8> corps = nkentseu::NkFile::ReadAllBytes(fA);
		bool negatifPose = false;
		if (corps.Size() > 40) {
			const nkentseu::uint32 milieu = corps.Size() / 2u;
			corps[milieu] = (nkentseu::uint8)(corps[milieu] ^ 0xFFu);
			negatifPose = nkentseu::NkFile::WriteAllBytes(fC, corps);
		}
		std::snprintf(cmd, sizeof(cmd), "\"\"%s\" --relire=%s\" > relecture_negatif.log 2>&1", exe, fC);
		const int codeNeg = negatifPose ? std::system(cmd) : -1;
		std::snprintf(d, sizeof(d), "octet %u inverse, code %d (2 attendu = refus)",
					  (unsigned)(corps.Size() / 2u), codeNeg);
		Verdict("f7 NEGATIF : un octet abime AU MILIEU -> refus", negatifPose && codeNeg == 2, d);

		// Le refus doit etre NOMME, pas un `false` nu.
		nkentseu::NkSequence abimee;
		const bool refuse = !abimee.LoadFromFile(fC);
		const char *raison = nkentseu::NkSequenceDernierRefus();
		std::snprintf(d, sizeof(d), "raison : \"%s\"", raison);
		Verdict("f7 le refus est NOMME, pas un faux nu", refuse && raison[0] != '\0', d);

		// Et il ne laisse pas une sequence a moitie lue : `abimee` doit etre restee
		// vierge. Un chargement qui viderait les pistes avant d'echouer laisserait
		// l'appelant sans rien a quoi revenir.
		std::snprintf(d, sizeof(d), "%u piste(s), %u marqueur(s) apres le refus", (unsigned)abimee.tracks.Size(),
					  (unsigned)abimee.markers.Size());
		Verdict("f7 le refus ne laisse PAS une sequence a moitie lue", abimee.tracks.Empty() && abimee.markers.Empty(),
				d);
	}

} // namespace

int main(int argc, char **argv) {
#if defined(_WIN32)
	// GARDE : cette fenetre n'est pas le produit. Elle doit le dire dans son
	// titre, pour qu'une capture prise par erreur ne puisse pas etre confondue
	// avec l'application de Rodolf.
	SetConsoleTitleA("*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT ***");
#endif

	// ── LE MODE « PROCESSUS NEUF » DE f7 ─────────────────────────────────────
	// Lu AVANT toute autre chose : ce mode ne doit rien faire d'autre que relire
	// un fichier, le reecrire, et rendre ses images. S'il executait la moindre
	// partie du banc, il pourrait reconstruire en memoire ce qu'il pretend avoir
	// relu du disque — et la preuve s'effondrerait sans bruit.
	//
	// Les drapeaux sont lus dans argv : AUCUNE injection de souris ni de clavier,
	// jamais. Meme dispositif que `ExportCli` de NkAnimaEditor.
	{
		const char *rel = nullptr, *reecrire = nullptr, *images = nullptr;
		for (int i = 1; i < argc; ++i) {
			const char *a = argv[i];
			if (Prefixe(a, "--relire="))
				rel = a + 9;
			else if (Prefixe(a, "--reecrire="))
				reecrire = a + 11;
			else if (Prefixe(a, "--images="))
				images = a + 9;
		}
		if (rel != nullptr)
			return ModeRelecture(rel, reecrire, images);
	}
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
	F6_LeGpuDessine();
	F7_LaSequenceSEnregistre(argv[0]);

	std::printf("\n-----------------------------------------------------------------------------\n");
	std::printf(" BILAN : %d verts, %d rouges\n", gPass, gFail);
	std::printf(" Status: %s\n", (gFail == 0) ? "SUCCESS" : "FAILURE");
	std::printf("-----------------------------------------------------------------------------\n");
	return (gFail == 0) ? 0 : 1;
}
