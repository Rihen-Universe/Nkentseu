// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// ExportCli.cpp — voir ExportCli.h. TU séparé : n'inclut NI NKRenderer NI l'Editor
// Kit, seulement NKAnima (Foundation), donc aucun risque du conflit de types
// NKRenderer/NKCanvas décrit en tête d'AnimBridge.h.
// =============================================================================
#include "ExportCli.h"
#include "AnimBridge.h"
#include "NKAnima/Clip/NkAnimation.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include <cstdlib>
#include <cstring>

namespace nkanima {

	using nkentseu::NkString;
	using nkentseu::anim::NkAnimationClip;
	using nkentseu::anim::NkAnimationTrack;
	using nkentseu::math::NkMat4f;

	namespace {

		// ── Écriture des NOMBRES ──────────────────────────────────────────────
		// Les flottants sortent en HEXA de leur motif binaire : deux valeurs sont
		// égales dans l'empreinte si et seulement si elles sont égales au bit près.
		// Un %f à six décimales aurait fait passer pour identiques deux poses qui
		// ne le sont pas — c'est exactement l'erreur que ce lot doit éviter.
		void HexU32(NkString &s, nkentseu::uint32 v) {
			const char *d = "0123456789ABCDEF";
			for (int i = 7; i >= 0; --i)
				s += d[(v >> (i * 4)) & 0xFu];
		}

		void HexF32(NkString &s, nkentseu::float32 v) {
			nkentseu::uint32 bits = 0;
			memcpy(&bits, &v, 4);
			HexU32(s, bits);
		}

		void DecU32(NkString &s, nkentseu::uint32 v) {
			char buf[16];
			int n = 0;
			if (v == 0)
				buf[n++] = '0';
			while (v > 0 && n < 15) {
				buf[n++] = (char)('0' + (v % 10u));
				v /= 10u;
			}
			while (n > 0)
				s += buf[--n];
		}

		void DecI32(NkString &s, nkentseu::int32 v) {
			if (v < 0) {
				s += '-';
				DecU32(s, (nkentseu::uint32)(-v));
			} else {
				DecU32(s, (nkentseu::uint32)v);
			}
		}

		void Mat(NkString &s, const NkMat4f &m) {
			const nkentseu::float32 *f = (const nkentseu::float32 *)m.data;
			for (int i = 0; i < 16; ++i) {
				s += ' ';
				HexF32(s, f[i]);
			}
		}

		// FNV-1a 64 bits sur le texte de l'empreinte : une seule ligne à comparer
		// à l'œil quand on ne veut pas diffusionner 200 Ko. La comparaison qui fait
		// FOI reste `cmp` sur le fichier entier.
		nkentseu::uint64 Fnv1a(const char *p, nkentseu::usize n) {
			nkentseu::uint64 h = 1469598103934665603ull;
			for (nkentseu::usize i = 0; i < n; ++i) {
				h ^= (nkentseu::uint64)(unsigned char)p[i];
				h *= 1099511628211ull;
			}
			return h;
		}

		void BuildDigest(const NkAnimationClip &c, NkString &out) {
			out += "NKANIM-DIGEST 1\n";
			out += "clip name=";
			out += c.name;
			out += " durationHex=";
			HexF32(out, c.duration);
			out += " fpsHex=";
			HexF32(out, c.fps);
			out += " loop=";
			DecU32(out, c.loop ? 1u : 0u);
			out += " boneCount=";
			DecU32(out, c.boneCount);
			out += " tracks=";
			DecU32(out, (nkentseu::uint32)c.boneTracks.Size());
			out += '\n';
			for (nkentseu::uint32 b = 0; b < (nkentseu::uint32)c.boneTracks.Size(); ++b) {
				const NkAnimationTrack<NkMat4f> &tr = c.boneTracks[b];
				out += "bone ";
				DecU32(out, b);
				out += " name=";
				out += tr.name;
				out += " enabled=";
				DecU32(out, tr.enabled ? 1u : 0u);
				out += " keys=";
				DecU32(out, tr.KeyCount());
				out += '\n';
				for (nkentseu::uint32 k = 0; k < tr.KeyCount(); ++k) {
					const nkentseu::anim::NkKeyframe<NkMat4f> &kf = tr.GetKey(k);
					out += "k ";
					DecU32(out, b);
					out += ' ';
					DecU32(out, k);
					out += ' ';
					HexF32(out, kf.time);
					Mat(out, kf.value);
					out += ' ';
					DecU32(out, (nkentseu::uint32)kf.interp);
					out += '\n';
				}
			}
			out += "skel local=";
			DecU32(out, c.skeletalLocal ? 1u : 0u);
			out += " parents=";
			DecU32(out, (nkentseu::uint32)c.jointParent.Size());
			out += " invbind=";
			DecU32(out, (nkentseu::uint32)c.jointInverseBind.Size());
			out += " topo=";
			DecU32(out, (nkentseu::uint32)c.jointTopo.Size());
			// jointNames : COMPTÉ ici, et chaque nom ÉCRIT plus bas. C'est cette ligne
			// qui a attrapé la perte du 2026-09-13 — le format v2 n'écrivait pas les
			// noms, et un aller-retour rendait un clip qui ressemblait à l'original
			// avec `jointNames=0`. Compter ne suffirait d'ailleurs pas : 19 chaînes
			// VIDES passeraient le compte. On compare donc les noms eux-mêmes.
			out += " jointNames=";
			DecU32(out, (nkentseu::uint32)c.jointNames.Size());
			out += '\n';
			for (nkentseu::uint32 j = 0; j < (nkentseu::uint32)c.jointParent.Size(); ++j) {
				out += "parent ";
				DecU32(out, j);
				out += ' ';
				DecI32(out, c.jointParent[j]);
				out += '\n';
			}
			for (nkentseu::uint32 j = 0; j < (nkentseu::uint32)c.jointInverseBind.Size(); ++j) {
				out += "ib ";
				DecU32(out, j);
				Mat(out, c.jointInverseBind[j]);
				out += '\n';
			}
			for (nkentseu::uint32 j = 0; j < (nkentseu::uint32)c.jointTopo.Size(); ++j) {
				out += "topo ";
				DecU32(out, j);
				out += ' ';
				DecU32(out, c.jointTopo[j]);
				out += '\n';
			}
			for (nkentseu::uint32 j = 0; j < (nkentseu::uint32)c.jointNames.Size(); ++j) {
				out += "jname ";
				DecU32(out, j);
				out += ' ';
				out += c.jointNames[j];
				out += '\n';
			}
		}

		bool WriteDigest(const NkAnimationClip &c, const char *path) {
			NkString body;
			BuildDigest(c, body);
			nkentseu::uint64 h = Fnv1a(body.CStr(), body.Size());
			NkString all;
			all += "FNV1A64 ";
			HexU32(all, (nkentseu::uint32)(h >> 32));
			HexU32(all, (nkentseu::uint32)(h & 0xFFFFFFFFull));
			all += '\n';
			all += body;
			if (!nkentseu::NkFile::WriteAllText(path, all.CStr())) {
				logger.Errorf("[ExportCli] empreinte non ecrite : %s\n", path);
				return false;
			}
			logger.Info("[ExportCli] empreinte '{0}' : {1} octets\n", path, (nkentseu::uint32)all.Size());
			return true;
		}

		bool StartsWith(const char *s, const char *p, const char **rest) {
			nkentseu::usize n = strlen(p);
			if (strncmp(s, p, n) != 0)
				return false;
			*rest = s + n;
			return true;
		}

	} // namespace

	bool ExportCliParseArg(const char *arg, NkExportCliArgs &out) {
		if (!arg)
			return false;
		const char *v = nullptr;
		if (StartsWith(arg, "--export=", &v)) {
			out.exportPath = v;
			return true;
		}
		if (StartsWith(arg, "--verify=", &v)) {
			out.verifyPath = v;
			return true;
		}
		if (StartsWith(arg, "--digest=", &v)) {
			out.digestPath = v;
			return true;
		}
		if (StartsWith(arg, "--edits=", &v)) {
			int n = atoi(v);
			out.edits = (n > 0) ? (unsigned int)n : 0u;
			return true;
		}
		if (StartsWith(arg, "--amp=", &v)) {
			out.amp = (float)atof(v);
			return true;
		}
		if (StartsWith(arg, "--mutate=", &v)) {
			out.mutateKey = atoi(v);
			return true;
		}
		return false;
	}

	bool ExportCliWanted(const NkExportCliArgs &a) {
		return a.exportPath != nullptr || a.verifyPath != nullptr;
	}

	int ExportCliRun(const NkExportCliArgs &a) {
		// ── Mode VERIFY : processus NEUF, rien d'autre que la relecture ────────
		// Aucun modèle n'est chargé, aucun bake n'est fait : ce que l'empreinte
		// décrit ne peut venir QUE du fichier. C'est ce qui distingue cette preuve
		// d'un aller-retour qui relirait le même objet resté en mémoire.
		if (a.verifyPath) {
			NkAnimationClip fresh;
			if (!fresh.LoadBinary(NkString(a.verifyPath))) {
				logger.Errorf("[ExportCli] verify : relecture impossible (%s)\n", a.verifyPath);
				return 6;
			}
			if (a.mutateKey >= 0) {
				// Volet NÉGATIF : on abîme une clé APRÈS relecture. L'empreinte doit
				// alors différer — sinon la comparaison ne mesure rien.
				bool done = false;
				for (nkentseu::uint32 b = 0; b < (nkentseu::uint32)fresh.boneTracks.Size() && !done; ++b) {
					NkAnimationTrack<NkMat4f> &tr = fresh.boneTracks[b];
					if ((nkentseu::uint32)a.mutateKey < tr.KeyCount()) {
						nkentseu::float32 t0 = tr.GetKey((nkentseu::uint32)a.mutateKey).time;
						tr.MoveKey((nkentseu::uint32)a.mutateKey, t0 + 0.001f);
						logger.Info("[ExportCli] MUTATION volontaire : os {0}, cle {1}\n", b,
									(nkentseu::uint32)a.mutateKey);
						done = true;
					}
				}
				if (!done) {
					logger.Errorf("[ExportCli] mutation demandee mais cle %d absente\n", a.mutateKey);
					return 7;
				}
			}
			if (a.digestPath && !WriteDigest(fresh, a.digestPath))
				return 8;
			logger.Info("[ExportCli] verify OK : {0} os, {1} cles sur l'os 0\n",
						(nkentseu::uint32)fresh.boneTracks.Size(),
						fresh.boneTracks.Empty() ? 0u : fresh.boneTracks[0].KeyCount());
			return 0;
		}

		// ── Mode EXPORT : charger, éditer sans main, écrire ────────────────────
		if (!AnimInit(a.modelPath)) {
			logger.Errorf("[ExportCli] modele non charge : %s\n", a.modelPath ? a.modelPath : "(nul)");
			return 2;
		}
		nkentseu::uint32 changed = AnimScriptedEdit(a.edits, a.amp);
		if (a.edits > 0 && changed == 0) {
			logger.Errorf("[ExportCli] aucune pose-cle MODIFIEE (edits=%u) : rien a ecrire qui prouve l'edition\n",
						  a.edits);
			return 3;
		}
		const NkAnimationClip *clip = (const NkAnimationClip *)AnimClipHandle();
		if (!clip) {
			logger.Errorf("[ExportCli] clip courant absent\n");
			return 4;
		}
		// L'empreinte est écrite AVANT le fichier : elle décrit l'objet ÉDITÉ en
		// mémoire, jamais une relecture. C'est le terme de gauche de la comparaison.
		if (a.digestPath && !WriteDigest(*clip, a.digestPath))
			return 4;
		if (!AnimExportClip(a.exportPath)) {
			logger.Errorf("[ExportCli] ecriture .nkanim echouee : %s\n", a.exportPath ? a.exportPath : "(nul)");
			return 5;
		}
		logger.Info("[ExportCli] export OK : {0} poses-cles modifiees -> {1}\n", changed, a.exportPath);
		return 0;
	}

} // namespace nkanima
