// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NKRenderer/Tools/Realiste/NkArgileReglages.cpp (12/11/2026, rendu-realiste) : les reglages du mode argile (voir
// l'en-tete). (13/11/2026) DEPLACE depuis Applications/NKCraft/src/NKCraft/Render/ (espace nk3d -> renderer).
#include "NKRenderer/Tools/Realiste/NkArgileReglages.h"
#include "NKRenderer/Tools/PathTracer/NkTraceurChemins.h"
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKImage/Core/NkImage.h"
#include "NKContainers/String/NkString.h"
#include "NKPlatform/NkEnv.h"
#include "NKCore/Text/NkSortie.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			bool Egal(const char *a, const char *b) {
				if (!a || !b)
					return false;
				while (*a && *a == *b) {
					++a;
					++b;
				}
				return *a == *b;
			}
			const char *Blancs(const char *s) {
				while (s && (*s == ' ' || *s == '\t' || *s == ',' || *s == ';'))
					++s;
				return s;
			}
			const char *Mot(const char *s, char *out, int32 cap) {
				s = Blancs(s);
				int32 n = 0;
				while (s && *s && *s != ' ' && *s != '\t' && *s != ',' && *s != ';' && *s != '\r' && *s != '\n') {
					if (n + 1 < cap)
						out[n++] = *s;
					++s;
				}
				out[n] = 0;
				return s;
			}
			// un reel decimal (signe, partie entiere, virgule ou point) ; faux s'il n'y en a pas
			bool Reel(const char *&s, float32 &v) {
				s = Blancs(s);
				bool neg = false;
				if (*s == '-' || *s == '+') {
					neg = *s == '-';
					++s;
				}
				if (!((*s >= '0' && *s <= '9') || *s == '.'))
					return false;
				float64 x = 0.0;
				while (*s >= '0' && *s <= '9')
					x = x * 10.0 + (float64)(*s++ - '0');
				if (*s == '.') {
					++s;
					float64 p = 0.1;
					while (*s >= '0' && *s <= '9') {
						x += p * (float64)(*s++ - '0');
						p *= 0.1;
					}
				}
				v = (float32)(neg ? -x : x);
				return true;
			}
			float32 Lin(uint8 c) {
				const float32 x = (float32)c / 255.f;
				return x <= 0.04045f ? x / 12.92f : (float32)math::NkPow((float64)((x + 0.055f) / 1.055f), 2.4);
			}
			uint8 Srgb(float32 c) {
				c = c < 0.f ? 0.f : (c > 1.f ? 1.f : c);
				const float32 s = c <= 0.0031308f ? 12.92f * c : 1.055f * (float32)math::NkPow((float64)c, 1.0 / 2.4) - 0.055f;
				return (uint8)(int32)(s * 255.f + 0.5f);
			}
			float32 Lum(float32 r, float32 g, float32 b) {
				return 0.2126f * r + 0.7152f * g + 0.0722f * b;
			}
			bool Mutation() {
				const char *m = env::GetEnvVar("NK_ARGILE_MUTATION");
				return m && m[0] == '1';
			}
		} // namespace

		NkArgileReglages NkArgilePrereglage(int32 k) {
			NkArgileReglages a;
			if (k == kNkArgileChaude) {
				a.eclairage = 1;
				a.balance = 2;
				a.exposition = 0;
				a.cle = 0.16f;
				a.gris = false;
			} else {
				a.eclairage = 0;
				a.balance = 0;
				a.exposition = 0;
				a.cle = kNkArgileCleReference; // (13/11) 0,13 -> 0,24 : la mesure est dans NkArgileReglages.h
				a.gris = true;
			}
			return a;
		}

		float32 NkArgileCleTempsReel(const NkArgileReglages &a) {
			const float32 k = (a.cle > 0.f ? a.cle : kNkArgileCleReference) * kNkArgileCleTempsReel;
			return k > 1.f ? 1.f : k;
		}
		const char *NkArgilePrereglageNom(int32 k) {
			return k == kNkArgileChaude ? "Argile chaude" : "Argile de reference";
		}
		bool NkArgileEgaux(const NkArgileReglages &a, const NkArgileReglages &b) {
			auto pres = [](float32 x, float32 y) { return math::NkFabs(x - y) < 1e-4f; };
			return a.eclairage == b.eclairage && a.balance == b.balance && pres(a.pipette[0], b.pipette[0]) &&
				   pres(a.pipette[1], b.pipette[1]) && pres(a.temperatureK, b.temperatureK) && a.exposition == b.exposition &&
				   pres(a.cle, b.cle) && pres(a.ev, b.ev) && a.gris == b.gris;
		}
		int32 NkArgileQuelPrereglage(const NkArgileReglages &a) {
			for (int32 k = 0; k < kNkArgilePrereglages; ++k)
				if (NkArgileEgaux(a, NkArgilePrereglage(k)))
					return k;
			return -1;
		}

		bool NkArgileLireLigne(NkArgileReglages &a, const char *mot, const char *reste) {
			char m[48];
			const char *q = Mot(reste, m, 48);
			if (Egal(mot, "argile_prereglage")) {
				a = NkArgilePrereglage(Egal(m, "chaude") ? kNkArgileChaude : kNkArgileReference);
				return true;
			}
			if (Egal(mot, "argile_eclairage")) {
				a.eclairage = Egal(m, "ambiance") ? 1 : 0;
				return true;
			}
			if (Egal(mot, "argile_balance")) {
				if (Egal(m, "aucune")) {
					a.balance = 2;
				} else if (Egal(m, "auto") || Egal(m, "automatique")) {
					a.balance = 0;
					float32 x = -1.f, y = -1.f;
					if (Reel(q, x) && Reel(q, y)) {
						a.pipette[0] = x;
						a.pipette[1] = y;
					} else {
						a.pipette[0] = a.pipette[1] = -1.f;
					}
				} else {
					const char *r = m;
					float32 k = 0.f;
					if (Reel(r, k) && k > 500.f) {
						a.balance = 1;
						a.temperatureK = k;
					}
				}
				return true;
			}
			if (Egal(mot, "argile_exposition")) {
				float32 v = 0.f;
				if (Egal(m, "manuelle")) {
					a.exposition = 1;
					if (Reel(q, v))
						a.ev = v;
				} else {
					a.exposition = 0;
					if (Reel(q, v) && v > 0.f)
						a.cle = v;
				}
				return true;
			}
			if (Egal(mot, "argile_couleur")) {
				a.gris = Egal(m, "gris");
				return true;
			}
			return false;
		}

		void NkArgileLireEnv(NkArgileReglages &a) {
			static const char *const kVars[5][2] = {{"NK_REALISTE_ARGILE_PREREGLAGE", "argile_prereglage"},
													{"NK_REALISTE_ARGILE_ECLAIRAGE", "argile_eclairage"},
													{"NK_REALISTE_ARGILE_BALANCE", "argile_balance"},
													{"NK_REALISTE_ARGILE_EXPOSITION", "argile_exposition"},
													{"NK_REALISTE_ARGILE_COULEUR", "argile_couleur"}};
			for (int32 i = 0; i < 5; ++i)
				if (const char *v = env::GetEnvVar(kVars[i][0]))
					if (v[0])
						(void)NkArgileLireLigne(a, kVars[i][1], v);
		}

		void NkArgileCapturer(const NkArgileReglages &a, bool actif, NkArchive &out) {
			NkArchive o;
			o.SetInt32("version", 1);
			o.SetBool("actif", actif);
			o.SetInt32("eclairage", a.eclairage);
			o.SetInt32("balance", a.balance);
			o.SetFloat32("pipetteX", a.pipette[0]);
			o.SetFloat32("pipetteY", a.pipette[1]);
			o.SetFloat32("temperatureK", a.temperatureK);
			o.SetInt32("exposition", a.exposition);
			if (!Mutation()) // contre-epreuve du banc : la cle n'est plus ecrite
				o.SetFloat32("cle", a.cle);
			o.SetFloat32("ev", a.ev);
			o.SetBool("gris", a.gris);
			out.SetObject("argile", o);
		}

		bool NkArgileRestaurer(const NkArchive &in, NkArgileReglages &a, bool &actif) {
			NkArchive o;
			if (!in.GetObject("argile", o))
				return false;
			NkArgileReglages d;
			nk_bool b = false;
			actif = o.GetBool("actif", b) ? (bool)b : false;
			nk_int32 i = 0;
			a.eclairage = o.GetInt32("eclairage", i) ? (int32)i : d.eclairage;
			a.balance = o.GetInt32("balance", i) ? (int32)i : d.balance;
			a.exposition = o.GetInt32("exposition", i) ? (int32)i : d.exposition;
			nk_float32 f = 0.f;
			a.pipette[0] = o.GetFloat32("pipetteX", f) ? (float32)f : d.pipette[0];
			a.pipette[1] = o.GetFloat32("pipetteY", f) ? (float32)f : d.pipette[1];
			a.temperatureK = o.GetFloat32("temperatureK", f) ? (float32)f : d.temperatureK;
			a.cle = o.GetFloat32("cle", f) ? (float32)f : d.cle;
			a.ev = o.GetFloat32("ev", f) ? (float32)f : d.ev;
			a.gris = o.GetBool("gris", b) ? (bool)b : d.gris;
			return true;
		}

		void NkArgileVersTraceur(const NkArgileReglages &a, int32 L, int32 H, renderer::NkTcReglages &r) {
			r.balanceK = 0.f;
			r.balanceX = r.balanceY = -1;
			r.balanceMonde = false;
			if (a.balance == 0) {
				if (a.pipette[0] >= 0.f && a.pipette[1] >= 0.f) {
					r.balanceX = (int32)(a.pipette[0] * (float32)L);
					r.balanceY = (int32)(a.pipette[1] * (float32)H);
					r.balanceR = L / 100 > 4 ? L / 100 : 4;
				} else
					r.balanceMonde = true;
			} else if (a.balance == 1)
				r.balanceK = a.temperatureK;
			r.expoAuto = a.exposition == 0;
			r.cleExpo = a.cle > 0.f ? a.cle : kNkArgileCleReference;
			r.evManuel = a.exposition == 1 ? a.ev : -1000.f;
			r.gris = a.gris;
		}

		void NkArgileGainsTemperature(const NkArgileReglages &a, float32 *g) {
			g[0] = g[1] = g[2] = 1.f;
			if (a.balance != 1)
				return;
			renderer::NkLightDesc ld;
			ld.color = {1.f, 1.f, 1.f};
			ld.temperatureK = a.temperatureK;
			const math::NkVec3f t = renderer::NkLightEffectiveColor(ld);
			ld.temperatureK = 6500.f;
			const math::NkVec3f b = renderer::NkLightEffectiveColor(ld);
			const float32 tr = t.x / (b.x > 1e-4f ? b.x : 1e-4f), tg = t.y / (b.y > 1e-4f ? b.y : 1e-4f),
						  tb = t.z / (b.z > 1e-4f ? b.z : 1e-4f);
			const float32 lt = Lum(tr, tg, tb);
			g[0] = lt / (tr > 1e-6f ? tr : 1e-6f);
			g[1] = lt / (tg > 1e-6f ? tg : 1e-6f);
			g[2] = lt / (tb > 1e-6f ? tb : 1e-6f);
		}

		bool NkArgileCorrigerImage(const NkArgileReglages &a, const char *png, float32 *gainsVus) {
			NkImage im;
			if (!im.Load(png, 3) || !im.IsValid())
				return false;
			const int32 W = im.Width(), H = im.Height(), C = im.Channels();
			if (C < 3)
				return false;
			float32 g[3] = {1.f, 1.f, 1.f};
			if (a.balance == 1)
				NkArgileGainsTemperature(a, g);
			else if (a.balance == 0) {
				// la teinte moyenne des pixels ni brules ni noirs (en argile : la lumiere), ou d'une fenetre a la pipette
				float64 s[3] = {0.0, 0.0, 0.0};
				int32 n = 0;
				int32 x0 = 0, x1 = W - 1, y0 = 0, y1 = H - 1;
				if (a.pipette[0] >= 0.f && a.pipette[1] >= 0.f) {
					const int32 cx = (int32)(a.pipette[0] * (float32)W), cy = (int32)(a.pipette[1] * (float32)H), rr = W / 100 > 4 ? W / 100 : 4;
					x0 = cx - rr;
					x1 = cx + rr;
					y0 = cy - rr;
					y1 = cy + rr;
				}
				for (int32 y = (y0 < 0 ? 0 : y0); y <= y1 && y < H; ++y) {
					const uint8 *p = im.RowPtr(y);
					for (int32 x = (x0 < 0 ? 0 : x0); x <= x1 && x < W; ++x) {
						const uint8 *q = p + (usize)x * (usize)C;
						const uint8 mx = q[0] > q[1] ? (q[0] > q[2] ? q[0] : q[2]) : (q[1] > q[2] ? q[1] : q[2]);
						const uint8 mn = q[0] < q[1] ? (q[0] < q[2] ? q[0] : q[2]) : (q[1] < q[2] ? q[1] : q[2]);
						if (mx > 250 || mn < 8)
							continue;
						for (int32 k = 0; k < 3; ++k)
							s[k] += (float64)Lin(q[k]);
						++n;
					}
				}
				if (n > 0) {
					const float32 tr = (float32)(s[0] / n), tg = (float32)(s[1] / n), tb = (float32)(s[2] / n);
					const float32 lt = Lum(tr, tg, tb);
					g[0] = lt / (tr > 1e-6f ? tr : 1e-6f);
					g[1] = lt / (tg > 1e-6f ? tg : 1e-6f);
					g[2] = lt / (tb > 1e-6f ? tb : 1e-6f);
				}
			}
			if (gainsVus)
				for (int32 k = 0; k < 3; ++k)
					gainsVus[k] = g[k];
			const bool rien = g[0] == 1.f && g[1] == 1.f && g[2] == 1.f && !a.gris;
			if (rien)
				return false;
			for (int32 y = 0; y < H; ++y) {
				uint8 *p = im.RowPtr(y);
				for (int32 x = 0; x < W; ++x) {
					uint8 *q = p + (usize)x * (usize)C;
					float32 c[3] = {Lin(q[0]) * g[0], Lin(q[1]) * g[1], Lin(q[2]) * g[2]};
					if (a.gris)
						c[0] = c[1] = c[2] = Lum(c[0], c[1], c[2]);
					for (int32 k = 0; k < 3; ++k)
						q[k] = Srgb(c[k]);
				}
			}
			return im.SavePNG(png);
		}

		int32 NkArgileBancDocument() {
			NkArgileReglages cas[4];
			cas[0] = NkArgileReglages{};
			cas[1] = NkArgilePrereglage(kNkArgileReference);
			cas[2] = NkArgilePrereglage(kNkArgileChaude);
			cas[3].eclairage = 1;
			cas[3].balance = 0;
			cas[3].pipette[0] = 0.62f;
			cas[3].pipette[1] = 0.37f;
			cas[3].temperatureK = 3200.f;
			cas[3].exposition = 1;
			cas[3].cle = 0.21f;
			cas[3].ev = 12.5f;
			cas[3].gris = false;
			int32 echecs = 0;
			for (int32 i = 0; i < 4; ++i) {
				NkArchive rendu;
				NkArgileCapturer(cas[i], (i & 1) != 0, rendu);
				const NkString json = NkJSONWriter::WriteArchive(rendu, true, 2);
				NkArchive relu;
				NkString err;
				const bool lu = NkJSONReader::ReadArchive(NkStringView(json.CStr()), relu, &err);
				NkArgileReglages r;
				bool actif = false;
				const bool ok = lu && NkArgileRestaurer(relu, r, actif) && NkArgileEgaux(r, cas[i]) && actif == ((i & 1) != 0);
				if (!ok)
					++echecs;
				NkPrintfSortie("[argile] BANC DOCUMENT cas %d (%s) : %s\n", (int)i,
							   NkArgileQuelPrereglage(cas[i]) >= 0 ? NkArgilePrereglageNom(NkArgileQuelPrereglage(cas[i])) : "reglages libres",
							   ok ? "IDENTIQUE apres JSON" : "DIFFERENT apres JSON");
			}
			// les lignes de liste (et donc les variables, meme syntaxe)
			{
				NkArgileReglages a;
				(void)NkArgileLireLigne(a, "argile_prereglage", "chaude");
				(void)NkArgileLireLigne(a, "argile_balance", "auto 0.25 0.5");
				(void)NkArgileLireLigne(a, "argile_exposition", "manuelle 13.5");
				(void)NkArgileLireLigne(a, "argile_couleur", "gris");
				const bool ok = a.eclairage == 1 && a.balance == 0 && math::NkFabs(a.pipette[0] - 0.25f) < 1e-4f &&
								a.exposition == 1 && math::NkFabs(a.ev - 13.5f) < 1e-4f && a.gris;
				if (!ok)
					++echecs;
				NkPrintfSortie("[argile] BANC LIGNES DE LISTE : %s\n", ok ? "VERT" : "ROUGE");
			}
			NkPrintfSortie("[argile] BANC DOCUMENT : %d echec(s)%s\n", (int)echecs,
						   Mutation() ? " (NK_ARGILE_MUTATION=1 : la cle n'est pas ecrite -- doit etre ROUGE)" : "");
			NkFflush(NkFlux::Sortie);
			return echecs;
		}

	} // namespace renderer
} // namespace nkentseu
