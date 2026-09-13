// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// ExportCli.cpp — voir ExportCli.h. TU séparé : n'inclut NI NKRenderer NI l'Editor
// Kit, seulement NKAnima (Foundation), donc aucun risque du conflit de types
// NKRenderer/NKCanvas décrit en tête d'AnimBridge.h.
// =============================================================================
#include "ExportCli.h"
#include "AnimBridge.h"
#include "Commands.h" // on appelle LES fonctions du bouton, jamais un clic
#include "NKAnima/Clip/NkAnimation.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include <cstdlib>
#include <cstring>
#include <cmath>

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

		bool WriteBody(const NkString &body, const char *path) {
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

		bool WriteDigest(const NkAnimationClip &c, const char *path) {
			NkString body;
			BuildDigest(c, body);
			return WriteBody(body, path);
		}

		// Empreinte d'une POSE : 16 flottants par joint (matrice monde), en hexa.
		// Pas les positions seules — une rotation pure d'un os ne déplace pas sa
		// propre position, et une empreinte de positions la laisserait passer.
		bool WritePoseDigest(const char *path, const char *mode, nkentseu::float32 t) {
			NkVector<nkentseu::float32> m;
			AnimGetPoseMatrices(m);
			if (m.Empty()) {
				logger.Errorf("[ExportCli] pose vide : rien a empreindre (%s)\n", mode);
				return false;
			}
			// ⚠️ NI LE MODE NI L'INSTANT n'entrent dans l'empreinte : le journal les
			// dit, le corps comparé ne porte QUE la pose. Mesuré en chemin le
			// 2026-09-13 : avec `tHex` dans le corps, deux scrubs à des instants
			// différents rendaient des empreintes différentes — par leur ÉTIQUETTE,
			// alors que les 19 lignes d'os étaient identiques au bit. Le témoin
			// disait « le curseur bouge » exactement quand il ne bougeait pas.
			logger.Info("[ExportCli] empreinte de pose : mode={0} t={1}\n", mode, t);
			NkString body;
			body += "NKANIM-POSE 1 joints=";
			DecU32(body, (nkentseu::uint32)(m.Size() / 16u));
			body += '\n';
			for (nkentseu::uint32 j = 0; j < (nkentseu::uint32)(m.Size() / 16u); ++j) {
				body += "j ";
				DecU32(body, j);
				for (nkentseu::uint32 e = 0; e < 16u; ++e) {
					body += ' ';
					HexF32(body, m[j * 16u + e]);
				}
				body += '\n';
			}
			return WriteBody(body, path);
		}

		// Decimal a virgule fixe : le rapport d'equilibre se lit a l'oeil (« de
		// combien le COM se deplace, en centimetres »), et l'hexa ne se lit pas.
		// L'hexa reste pour ce qui doit etre compare au bit ; ici on veut un nombre
		// que Rodolf peut opposer a son intuition.
		void DecF(NkString &s, nkentseu::float32 v, nkentseu::uint32 dec) {
			bool neg = (v < 0.f);
			if (neg)
				v = -v;
			nkentseu::uint32 mul = 1;
			for (nkentseu::uint32 i = 0; i < dec; ++i)
				mul *= 10u;
			const nkentseu::uint64 scaled = (nkentseu::uint64)((double)v * (double)mul + 0.5);
			if (neg)
				s += '-';
			DecU32(s, (nkentseu::uint32)(scaled / mul));
			if (dec > 0) {
				s += '.';
				const nkentseu::uint32 frac = (nkentseu::uint32)(scaled % mul);
				nkentseu::uint32 p = mul / 10u;
				while (p > 0) {
					s += (char)('0' + ((frac / p) % 10u));
					p /= 10u;
				}
			}
		}

		void Vec3(NkString &s, const nkentseu::float32 *v) {
			s += '(';
			for (int i = 0; i < 3; ++i) {
				if (i)
					s += ' ';
				DecF(s, v[i], 4);
			}
			s += ") hex";
			for (int i = 0; i < 3; ++i) {
				s += ' ';
				HexF32(s, v[i]);
			}
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
		if (StartsWith(arg, "--save-button=", &v)) {
			out.saveButtonPath = v;
			return true;
		}
		if (StartsWith(arg, "--save-as=", &v)) {
			out.saveAsPath = v;
			return true;
		}
		if (StartsWith(arg, "--balance=", &v)) {
			out.balance = true;
			out.balanceTime = (float)atof(v);
			return true;
		}
		if (StartsWith(arg, "--lean=", &v)) {
			out.lean = (float)atof(v);
			return true;
		}
		if (StartsWith(arg, "--foot=", &v)) {
			out.footPrint = (float)atof(v);
			return true;
		}
		if (StartsWith(arg, "--scrub=", &v)) {
			out.scrubTime = (float)atof(v);
			return true;
		}
		if (StartsWith(arg, "--play-to=", &v)) {
			out.playToTime = (float)atof(v);
			return true;
		}
		return false;
	}

	bool ExportCliWanted(const NkExportCliArgs &a) {
		return a.exportPath != nullptr || a.verifyPath != nullptr || a.saveButtonPath != nullptr ||
			   a.saveAsPath != nullptr || a.scrubTime >= 0.f || a.playToTime >= 0.f || a.balance;
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

		// ── Tous les autres modes ont besoin du MODELE ────────────────────────
		if (!AnimInit(a.modelPath)) {
			logger.Errorf("[ExportCli] modele non charge : %s\n", a.modelPath ? a.modelPath : "(nul)");
			return 2;
		}

		// ── L'ÉQUILIBRE : ce que le viewport calcule, en NOMBRES ──────────────
		if (a.balance) {
			if (a.footPrint >= 0.f)
				AnimSetFootPrintFraction(a.footPrint);
			AnimSeek(a.balanceTime);
			int32 leaned = -1;
			if (a.lean != 0.f) {
				leaned = AnimLeanTorso(a.lean);
				if (leaned < 0) {
					logger.Errorf("[ExportCli] --lean : aucun joint de tronc NOMME dans ce rig\n");
					return 12;
				}
			}
			NkAnimBalanceReport r;
			if (!AnimComputeBalance(r)) {
				logger.Errorf("[ExportCli] equilibre : rien a calculer (pose vide)\n");
				return 11;
			}
			NkString b;
			b += "NKANIM-BALANCE 1\n";
			b += "joints=";
			DecU32(b, (nkentseu::uint32)r.jointCount);
			b += " regime=";
			b += (r.regime == 1) ? "anthropometrique" : (r.regime == 0 ? "uniforme" : "aucun");
			b += " upAxis=";
			DecU32(b, (nkentseu::uint32)r.upAxis);
			b += " sol=";
			DecF(b, r.floorLevel, 4);
			b += " penche=";
			DecF(b, a.lean, 3);
			b += "rad joint=";
			DecI32(b, leaned);
			b += '\n';
			b += "comUniforme= ";
			Vec3(b, r.comUniform);
			b += '\n';
			b += "comCourant=  ";
			Vec3(b, r.comCurrent);
			b += '\n';
			// L'ECART, en centimetres et par axe : c'est la question posee.
			const nkentseu::float32 d[3] = {r.comCurrent[0] - r.comUniform[0], r.comCurrent[1] - r.comUniform[1],
											r.comCurrent[2] - r.comUniform[2]};
			const nkentseu::float32 norme = (nkentseu::float32)sqrt((double)(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
			b += "ecart_cm= (";
			for (int i = 0; i < 3; ++i) {
				if (i)
					b += ' ';
				DecF(b, d[i] * 100.f, 2);
			}
			b += ") norme_cm= ";
			DecF(b, norme * 100.f, 2);
			b += '\n';
			b += "appuisNommes=";
			DecU32(b, (nkentseu::uint32)r.footCount);
			b += " contacts=";
			DecU32(b, (nkentseu::uint32)r.contactCount);
			b += " sommetsPolygone=";
			DecU32(b, (nkentseu::uint32)r.supportCount);
			b += " demiEmpreinte_cm=";
			DecF(b, r.footHalfSize * 100.f, 2);
			b += " tolerance_cm=";
			DecF(b, r.contactThreshold * 100.f, 2);
			b += '\n';
			b += "verdict=";
			b += (r.verdict == 1) ? "EQUILIBRE" : (r.verdict == 0 ? "DESEQUILIBRE" : "INDETERMINE");
			b += " marge_cm=";
			DecF(b, r.margin * 100.f, 2);
			b += '\n';
			// Les NOMS qui ont ete reconnus comme appuis : sans eux, « 0 appui » ne
			// dit pas si le rig n'a pas de pieds ou si nos mots-cles les ont ratés.
			const NkAnimationClip *cl = (const NkAnimationClip *)AnimClipHandle();
			if (cl) {
				b += "noms=";
				DecU32(b, (nkentseu::uint32)cl->jointNames.Size());
				b += '\n';
				for (nkentseu::uint32 j = 0; j < (nkentseu::uint32)cl->jointNames.Size(); ++j) {
					b += "  joint ";
					DecU32(b, j);
					b += ' ';
					b += cl->jointNames[j];
					b += '\n';
				}
			}
			if (!a.digestPath) {
				logger.Errorf("[ExportCli] --balance exige --digest=\n");
				return 10;
			}
			return WriteBody(b, a.digestPath) ? 0 : 9;
		}

		// ── LE CURSEUR : scrub SANS lecture, ou lecture jusqu'à t ─────────────
		// `AnimSeek` est exactement ce que le glissé de la timeline appelle
		// (Panels.h l.135 et l.141) ; `AnimUpdate` est ce que le panneau appelle à
		// chaque image. Aucun chemin parallèle, encore une fois.
		if (a.scrubTime >= 0.f || a.playToTime >= 0.f) {
			const bool scrub = (a.scrubTime >= 0.f);
			const nkentseu::float32 t = scrub ? a.scrubTime : a.playToTime;
			if (scrub) {
				AnimSeek(t);
			} else {
				// UNE seule avance de t depuis 0 : `mTime` vaut t AU BIT, pas la
				// somme de soixante pas de 1/60 s qui n'y tomberait jamais.
				AnimSetPlaying(true);
				AnimUpdate(t);
			}
			if (!a.digestPath) {
				logger.Errorf("[ExportCli] --scrub/--play-to exigent --digest=\n");
				return 10;
			}
			return WritePoseDigest(a.digestPath, scrub ? "scrub" : "lecture", t) ? 0 : 9;
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

		// ── LE GESTE DE RODOLF, sans main : on appelle LA fonction du bouton ──
		// `CmdSave` est littéralement ce que la commande « Fichier: Enregistrer »
		// (Ctrl+S) et le bouton de la barre d'outils appellent ; `CmdSaveAsConfirmed`
		// est la branche de confirmation du sélecteur. Aucun clic n'est simulé : on
		// entre par la même porte, un cran en dessous du dessin.
		// ⚠️ Et on ne regarde PAS leur valeur de retour — le bouton ne la regarde pas
		// non plus. Ce qui fait foi, c'est le fichier écrit, relu par --verify.
		if (a.saveButtonPath) {
			AnimSetSavePath(a.saveButtonPath);
			CmdSave(nullptr);
			logger.Info("[ExportCli] bouton Enregistrer : {0} poses-cles modifiees -> {1}\n", changed,
						a.saveButtonPath);
			return 0;
		}
		if (a.saveAsPath) {
			CmdSaveAsConfirmed(a.saveAsPath);
			logger.Info("[ExportCli] Enregistrer sous (confirme) : {0} poses-cles modifiees -> {1}\n", changed,
						a.saveAsPath);
			return 0;
		}
		if (!AnimExportClip(a.exportPath)) {
			logger.Errorf("[ExportCli] ecriture .nkanim echouee : %s\n", a.exportPath ? a.exportPath : "(nul)");
			return 5;
		}
		logger.Info("[ExportCli] export OK : {0} poses-cles modifiees -> {1}\n", changed, a.exportPath);
		return 0;
	}

} // namespace nkanima
