#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkModelerGeomProbe.h — LA PREMIERE SONDE DE NK3DModeler. Sans fenetre.
//
// POURQUOI ELLE EXISTE. NK3DModeler est la deuxieme application du depot par la
// taille, et la SEULE des cinq a n'avoir eu AUCUNE sonde : ni drapeau, ni test,
// ni banc. Tout s'y verifiait a l'oeil, dans une fenetre, sur la machine de
// Rodolf. Une regression de persistance ne se voyait donc qu'au moment ou l'on
// perdait du travail -- c'est-a-dire trop tard.
//
// CE QU'ELLE EPROUVE, ET CE QU'ELLE N'EPROUVE PAS
//   Elle eprouve l'ALLER-RETOUR DE LA GEOMETRIE PAR LE DISQUE : des sommets ->
//   une archive -> un fichier JSON -> une archive RELUE -> des sommets. C'est
//   la chaine exacte que `.nkmesh` et `.nkscene` empruntent (NkGeomWrite /
//   NkAsWrite / NkAsRead / NkGeomRead), a ceci pres qu'elle n'a besoin NI de
//   fenetre, NI de GPU, NI de l'hote 3D. Elle tourne donc partout, en une
//   seconde, et peut etre rejouee par n'importe qui.
//   Elle N'EPROUVE PAS la vue 3D : « le maillage revient dans le viewport » se
//   prouve dans un PROCESSUS NEUF de l'application, pas ici. Les deux preuves
//   sont complementaires, et aucune ne remplace l'autre.
//
// LA STRUCTURE EST VIDEE, ET C'EST DIT. La relecture part d'une archive NEUVE
// remplie depuis le TEXTE du fichier : rien de l'ecriture ne survit dans
// l'objet compare. Un aller-retour qui passerait parce qu'on relit ce qu'on a
// encore en memoire ne prouverait rien du tout.
//
// SON VOLET NEGATIF EST OBLIGATOIRE. Un temoin qui rend exactement zero doit
// prouver qu'il sait rendre autre chose : la sonde DEPLACE un sommet avant de
// comparer, et exige que la comparaison ECHOUE. Si le negatif passe, la sonde
// se declare elle-meme sans valeur -- et le dit dans son verdict.
//
// Usage :  NK3DModeler.exe --probe=geom
// Sortie : des lignes « [probe geom] ... », un verdict, et RIEN a l'ecran.
// =============================================================================

#include "NK3DModeler/Project/NkModelerGeom.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h" // Demo3DHostVertexBytes : le pas, sans le rendu

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"

#ifdef GetObject
#undef GetObject
#endif

#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstring>

namespace nkentseu {
	namespace nk3d {

		// Tolerance de l'identite spatiale, la regle maison : 1e-4. Le codec est en
		// fait EXACT (les octets du sommet sont recopies), donc l'ecart attendu est
		// zero -- mais le critere reste celui du depot, pas une exigence inventee
		// pour l'occasion.
		static const float32 kGeomProbeEps = 1e-4f;

		/// UN CUBE A LA CONVENTION DE LA MAISON : notre Vert EST un coin, donc un
		/// cube fait 24 sommets et 36 indices -- pas 8. Les positions sont
		/// arbitraires mais DISTINCTES l'une de l'autre : deux sommets identiques
		/// laisseraient passer une permutation, et une permutation est une
		/// geometrie changee.
		inline void NkGeomProbeCube(NkVector<uint8> &bytes, uint32 stride, uint32 *vcount,
									NkVector<uint32> &idx) {
			const uint32 kV = 24u;
			bytes.Clear();
			bytes.Resize((usize)kV * (usize)stride);
			for (usize b = 0; b < bytes.Size(); ++b)
				bytes[b] = 0u;
			for (uint32 v = 0; v < kV; ++v) {
				const uint32 face = v / 4u, corner = v % 4u;
				float32 p[3];
				// Une valeur par sommet, jamais deux fois la meme : le .25 et le .0625
				// separent les coins d'une meme face et les faces entre elles.
				p[0] = ((corner & 1u) ? 0.5f : -0.5f) + (float32)face * 0.0625f;
				p[1] = ((corner & 2u) ? 0.5f : -0.5f) + (float32)v * 0.001f;
				p[2] = ((face & 1u) ? 0.5f : -0.5f) - (float32)corner * 0.25f;
				// Les octets du sommet commencent par sa POSITION (NkVertex3D::pos) :
				// c'est le seul champ dont la sonde a besoin de connaitre la place.
				for (uint32 a = 0; a < 3u; ++a) {
					uint8 raw[sizeof(float32)];
					std::memcpy(raw, &p[a], sizeof(float32));
					for (uint32 k = 0; k < sizeof(float32); ++k)
						bytes[(usize)v * stride + a * sizeof(float32) + k] = raw[k];
				}
			}
			if (vcount)
				*vcount = kV;
			idx.Clear();
			for (uint32 f = 0; f < 6u; ++f) {
				const uint32 o = f * 4u;
				const uint32 t[6] = {o, o + 1u, o + 2u, o + 2u, o + 1u, o + 3u};
				for (uint32 k = 0; k < 6u; ++k)
					idx.PushBack(t[k]);
			}
		}

		/// Ecart MAXIMAL entre deux jeux de sommets, sur la position seule.
		/// Rend -1 quand les comptes different : « pas comparable » n'est pas
		/// « identique », et rendre 0 sur deux tailles differentes serait le genre
		/// de temoin muet qu'on paie plus tard.
		inline float32 NkGeomProbeMaxDelta(const NkVector<uint8> &a, uint32 na,
										   const NkVector<uint8> &b, uint32 nb, uint32 stride) {
			if (na != nb || a.Size() != b.Size())
				return -1.f;
			float32 worst = 0.f;
			for (uint32 v = 0; v < na; ++v)
				for (uint32 c = 0; c < 3u; ++c) {
					float32 x = 0.f, y = 0.f;
					std::memcpy(&x, &a[(usize)v * stride + c * sizeof(float32)], sizeof(float32));
					std::memcpy(&y, &b[(usize)v * stride + c * sizeof(float32)], sizeof(float32));
					const float32 d = std::fabs(x - y);
					if (d > worst)
						worst = d;
				}
			return worst;
		}

		/// Horloge de la sonde, en microsecondes. Un temps s'annonce avec ce qui le
		/// mesure : `steady_clock`, pas l'heure du mur.
		inline uint64 NkGeomProbeMicros() {
			return (uint64)std::chrono::duration_cast<std::chrono::microseconds>(
					   std::chrono::steady_clock::now().time_since_epoch())
				.count();
		}

		/// UN MAILLAGE D'ESSAI QUI RESSEMBLE A LA VRAIE DONNEE, et c'est tout
		/// l'enjeu d'une mesure de compression : un tampon de zeros se compresserait
		/// cent fois et ne prouverait rien, un tampon aleatoire ne se compresserait
		/// pas et ne prouverait rien non plus. Celui-ci reproduit ce que la mesure a
		/// trouve dans les maillages reels : des positions qui varient partout, des
		/// normales prises dans un petit jeu de valeurs, une tangente et une couleur
		/// CONSTANTES, un second jeu d'UV a zero -- douze a trente-huit colonnes
		/// d'octets constantes sur cinquante-six.
		inline void NkGeomProbeMaillage(NkVector<uint8> &bytes, uint32 stride, uint32 nverts,
										uint32 *vcount, NkVector<uint32> &idx) {
			bytes.Clear();
			bytes.Resize((usize)nverts * (usize)stride);
			for (usize b = 0; b < bytes.Size(); ++b)
				bytes[b] = 0u;
			static const float32 kNorm[6][3] = {{1.f, 0.f, 0.f},  {-1.f, 0.f, 0.f},
												{0.f, 1.f, 0.f},  {0.f, -1.f, 0.f},
												{0.f, 0.f, 1.f},  {0.f, 0.f, -1.f}};
			for (uint32 v = 0; v < nverts; ++v) {
				const float32 f = (float32)v;
				const float32 val[11] = {
					// position : varie sur les trois axes
					f * 0.013f, f * 0.007f - 3.f, f * 0.019f + 1.f,
					// normale : une des six
					kNorm[v % 6u][0], kNorm[v % 6u][1], kNorm[v % 6u][2],
					// tangente : CONSTANTE
					1.f, 0.f, 0.f,
					// uv : varie ; uv2 reste a zero (deja mis a zero plus haut)
					(float32)(v % 64u) / 64.f, (float32)((v / 64u) % 64u) / 64.f};
				for (uint32 a = 0; a < 11u; ++a) {
					uint8 raw[sizeof(float32)];
					std::memcpy(raw, &val[a], sizeof(float32));
					for (uint32 k = 0; k < sizeof(float32); ++k)
						bytes[(usize)v * stride + a * sizeof(float32) + k] = raw[k];
				}
				// couleur : CONSTANTE (0xFFFFFFFF), aux quatre derniers octets
				if (stride >= 4u)
					for (uint32 k = 0; k < 4u; ++k)
						bytes[(usize)v * stride + (stride - 4u) + k] = 0xFFu;
			}
			if (vcount)
				*vcount = nverts;
			idx.Clear();
			for (uint32 v = 0; v + 2u < nverts; v += 3u) {
				idx.PushBack(v);
				idx.PushBack(v + 1u);
				idx.PushBack(v + 2u);
			}
		}

		/// La sonde. Rend 0 si TOUT passe, le nombre de criteres rouges sinon --
		/// le volet negatif compris : un negatif qui ne rougit pas est un echec.
		inline int32 NkGeomProbeRun() {
			const uint32 stride = demo::Demo3DHostVertexBytes();
			std::printf("[probe geom] pas du sommet : %u octets\n", (unsigned)stride);
			int32 rouges = 0;

			NkVector<uint8> src;
			NkVector<uint32> srcIdx;
			uint32 srcN = 0u;
			NkGeomProbeCube(src, stride, &srcN, srcIdx);
			std::printf("[probe geom] maillage d'essai : %u sommets, %u indices\n",
						(unsigned)srcN, (unsigned)srcIdx.Size());

			// ── ECRITURE : la MEME chaine que le .nkmesh (archive -> JSON -> fichier)
			NkArchive nd;
			nd.SetInt32("nature", 2);
			if (!NkGeomWrite(nd, src.Data(), srcN, stride, srcIdx.Data(),
							 (uint32)srcIdx.Size())) {
				std::printf("[probe geom] ROUGE C1 : l'ecriture a refuse le maillage\n");
				return 1;
			}
			NkDirectory::CreateRecursive("captures");
			const char *path = "captures/nk3d_probe_geom.nkmesh";
			const NkString json = NkJSONWriter::WriteArchive(nd, true, 2);
			if (!NkFile::WriteAllText(path, json.CStr())) {
				std::printf("[probe geom] ROUGE C1 : fichier non ecrit (%s)\n", path);
				return 1;
			}
			std::printf("[probe geom] fichier ecrit : %s (%u octets de texte)\n", path,
						(unsigned)json.Size());

			// ── RELECTURE : archive NEUVE, remplie depuis le TEXTE du fichier.
			// Rien de l'ecriture ne survit ici -- c'est le « structure videe » de la
			// consigne, dit explicitement.
			NkArchive relu;
			{
				const NkString text = NkFile::ReadAllText(path);
				NkString perr;
				if (!NkJSONReader::ReadArchive(NkStringView(text.CStr()), relu, &perr)) {
					std::printf("[probe geom] ROUGE C1 : JSON illisible (%s)\n", perr.CStr());
					return 1;
				}
			}
			NkVector<uint8> back;
			NkVector<uint32> backIdx;
			uint32 backN = 0u;
			NkString why;
			if (!NkGeomRead(relu, stride, back, &backN, backIdx, &why)) {
				std::printf("[probe geom] ROUGE C1 : relecture refusee (%s)\n",
							why.Empty() ? "aucune geometrie" : why.CStr());
				return 1;
			}

			// ── C1 : les comptes et les positions ───────────────────────────────
			const float32 d1 = NkGeomProbeMaxDelta(src, srcN, back, backN, stride);
			bool idxOk = backIdx.Size() == srcIdx.Size();
			for (usize k = 0; idxOk && k < srcIdx.Size(); ++k)
				idxOk = backIdx[k] == srcIdx[k];
			const bool c1 = (backN == srcN) && idxOk && d1 >= 0.f && d1 < kGeomProbeEps;
			std::printf("[probe geom] C1 sommets %u -> %u | indices %u -> %u | ecart max %.9f | %s\n",
						(unsigned)srcN, (unsigned)backN, (unsigned)srcIdx.Size(),
						(unsigned)backIdx.Size(), (double)d1, c1 ? "VERT" : "ROUGE");
			if (!c1)
				++rouges;

			// ── C1n : LE VOLET NEGATIF. Un sommet deplace de 0,01 -- cent fois la
			// tolerance -- doit faire ECHOUER la comparaison. Sans ce pas, « ecart
			// max 0 » ne prouverait que l'immobilite de l'instrument.
			{
				NkVector<uint8> mute = back;
				float32 x = 0.f;
				std::memcpy(&x, &mute[0], sizeof(float32));
				x += 0.01f;
				std::memcpy(&mute[0], &x, sizeof(float32));
				const float32 d2 = NkGeomProbeMaxDelta(src, srcN, mute, backN, stride);
				const bool rouge = !(d2 >= 0.f && d2 < kGeomProbeEps);
				std::printf("[probe geom] C1n sommet 0 deplace de 0,010000 : ecart max %.9f | "
							"la comparaison %s | %s\n",
							(double)d2, rouge ? "ECHOUE (attendu)" : "PASSE (anormal)",
							rouge ? "VERT" : "ROUGE");
				if (!rouge)
					++rouges;
			}

			// ── C2 : LE PAS FAIT FOI. Un fichier qui annonce un autre sommet doit
			// etre REFUSE, jamais lu de travers : un sommet decode avec le mauvais
			// pas donne une geometrie plausible et fausse.
			{
				NkArchive faux;
				NkArchive g;
				(void)relu.GetObject("geometrie", g);
				g.SetInt32("octetsParSommet", (int32)stride + 4);
				faux.SetObject("geometrie", g);
				NkVector<uint8> v2;
				NkVector<uint32> i2;
				uint32 n2 = 0u;
				NkString w2;
				const bool refuse = !NkGeomRead(faux, stride, v2, &n2, i2, &w2);
				std::printf("[probe geom] C2 pas annonce %u au lieu de %u : %s (%s) | %s\n",
							(unsigned)(stride + 4u), (unsigned)stride,
							refuse ? "refuse" : "ACCEPTE", w2.Empty() ? "sans raison" : w2.CStr(),
							(refuse && !w2.Empty()) ? "VERT" : "ROUGE");
				if (!refuse || w2.Empty())
					++rouges;
			}

			// ── C3 : LE COMPTE FAIT FOI. Un `sommets` qui ne retombe pas sur les
			// octets decodes est un fichier tronque : refus, avec sa raison.
			{
				NkArchive faux;
				NkArchive g;
				(void)relu.GetObject("geometrie", g);
				g.SetInt32("sommets", (int32)srcN + 1);
				faux.SetObject("geometrie", g);
				NkVector<uint8> v3;
				NkVector<uint32> i3;
				uint32 n3 = 0u;
				NkString w3;
				const bool refuse = !NkGeomRead(faux, stride, v3, &n3, i3, &w3);
				std::printf("[probe geom] C3 compte annonce %u au lieu de %u : %s (%s) | %s\n",
							(unsigned)(srcN + 1u), (unsigned)srcN, refuse ? "refuse" : "ACCEPTE",
							w3.Empty() ? "sans raison" : w3.CStr(),
							(refuse && !w3.Empty()) ? "VERT" : "ROUGE");
				if (!refuse || w3.Empty())
					++rouges;
			}

			// ── C4 : LA COMPRESSION -- taux REELS, temps REELS, et l'aller-retour
			// qui doit rester BIT A BIT. Trois tailles, parce qu'un taux mesure sur
			// un seul maillage ne dit rien : la structure d'un cube de 24 sommets
			// n'est pas celle d'un maillage de vingt mille.
			{
				static const uint32 kTailles[3] = {24u, 1200u, 20000u};
				for (int32 t = 0; t < 3; ++t) {
					NkVector<uint8> gros;
					NkVector<uint32> grosIdx;
					uint32 grosN = 0u;
					NkGeomProbeMaillage(gros, stride, kTailles[t], &grosN, grosIdx);
					// ⚠️ NkGeomWrite ecrit DANS le noeud, sous la cle « geometrie ».
					// Lui passer le bloc directement produirait `geometrie.geometrie`
					// -- et la sonde serait alors verte ou rouge pour une raison qui
					// n'a rien a voir avec ce qu'elle croit mesurer. Paye une fois.
					NkArchive nd4;
					const uint64 t0 = NkGeomProbeMicros();
					(void)NkGeomWrite(nd4, gros.Data(), grosN, stride, grosIdx.Data(),
									  (uint32)grosIdx.Size());
					const uint64 t1 = NkGeomProbeMicros();
					NkArchive gg;
					(void)nd4.GetObject("geometrie", gg);
					NkVector<uint8> back2;
					NkVector<uint32> backIdx2;
					uint32 backN2 = 0u;
					NkString w4;
					const uint64 t2 = NkGeomProbeMicros();
					const bool ok4 = NkGeomRead(nd4, stride, back2, &backN2, backIdx2, &w4);
					const uint64 t3 = NkGeomProbeMicros();
					// Taille des octets ENCODES, la seule qui compte pour le fichier.
					NkString v64;
					(void)gg.GetString("v", v64);
					NkString cod;
					(void)gg.GetString("codage", cod);
					const usize brut = (usize)grosN * (usize)stride;
					const usize avant = (brut + 2u) / 3u * 4u; // ce que faisait la version 1
					bool exact = ok4 && backN2 == grosN && back2.Size() == gros.Size();
					for (usize k = 0; exact && k < gros.Size(); ++k)
						exact = gros[k] == back2[k];
					std::printf("[probe geom] C4 %5u sommets | brut %7u o | avant (base64) %7u o | "
								"apres %7u o | x%.2f | codage %s | ecrire %.1f ms, relire %.1f ms | "
								"octets identiques %s | %s\n",
								(unsigned)grosN, (unsigned)brut, (unsigned)avant,
								(unsigned)v64.Size(),
								v64.Size() ? (double)avant / (double)v64.Size() : 0.0,
								cod.Empty() ? "?" : cod.CStr(), (double)(t1 - t0) / 1000.0,
								(double)(t3 - t2) / 1000.0, exact ? "oui" : "NON",
								exact ? "VERT" : "ROUGE");
					if (!exact)
						++rouges;
				}
			}

			// ── C5 : LE VOLET NEGATIF DE LA COMPRESSION. Un octet abime dans le
			// flux compresse doit etre DETECTE ou REFUSE, jamais relu en silence.
			// On abime un caractere du base64 au MILIEU du flux, la ou ni le debut
			// ni la fin ne protegent.
			{
				NkVector<uint8> gros;
				NkVector<uint32> grosIdx;
				uint32 grosN = 0u;
				NkGeomProbeMaillage(gros, stride, 1200u, &grosN, grosIdx);
				NkArchive nd5;
				(void)NkGeomWrite(nd5, gros.Data(), grosN, stride, grosIdx.Data(),
								  (uint32)grosIdx.Size());
				NkArchive gg;
				(void)nd5.GetObject("geometrie", gg);
				NkString cod5, v64;
				(void)gg.GetString("codage", cod5);
				(void)gg.GetString("v", v64);
				// Un caractere change = au moins un octet different apres decodage.
				NkString abime;
				const NkString::SizeType mid = v64.Size() / 2u;
				for (NkString::SizeType k = 0; k < v64.Size(); ++k)
					abime += (k == mid) ? (char)(v64[k] == 'A' ? 'B' : 'A') : v64[k];
				gg.SetString("v", abime.CStr());
				nd5.SetObject("geometrie", gg); // remplace le bloc, pas un second niveau
				NkVector<uint8> v5;
				NkVector<uint32> i5;
				uint32 n5 = 0u;
				NkString w5;
				const bool refuse = !NkGeomRead(nd5, stride, v5, &n5, i5, &w5);
				// LE CODAGE EST DIT DANS LA LIGNE : un refus sur un flux BRUT ne
				// prouverait rien de la compression. On doit voir « transpose-rle ».
				std::printf("[probe geom] C5 un caractere du flux %s abime au milieu (sur %u) : "
							"%s (%s) | %s\n",
							cod5.Empty() ? "?" : cod5.CStr(), (unsigned)v64.Size(),
							refuse ? "refuse" : "ACCEPTE EN SILENCE",
							w5.Empty() ? "sans raison" : w5.CStr(),
							(refuse && !w5.Empty()) ? "VERT" : "ROUGE");
				if (!refuse || w5.Empty())
					++rouges;
			}

			// ── C6 : COMPATIBILITE DESCENDANTE. Un bloc de VERSION 1 -- base64 brut,
			// sans `codage`, sans `vBrut`, sans `vSomme`, exactement ce que l'on a
			// ecrit ce matin -- doit se relire a l'identique. La compatibilite ne se
			// promet pas, elle se mesure, et elle se remesure au lot suivant.
			{
				NkArchive g1;
				g1.SetInt32("version", 1);
				g1.SetInt32("octetsParSommet", (int32)stride);
				g1.SetInt32("sommets", (int32)srcN);
				g1.SetInt32("indices", (int32)srcIdx.Size());
				g1.SetString("v", encoding::base64::NkEncode(src.Data(), src.Size()).CStr());
				g1.SetString("i",
							 encoding::base64::NkEncode((const uint8 *)srcIdx.Data(),
														srcIdx.Size() * sizeof(uint32))
								 .CStr());
				NkArchive nd6;
				nd6.SetObject("geometrie", g1);
				NkVector<uint8> v6;
				NkVector<uint32> i6;
				uint32 n6 = 0u;
				NkString w6;
				const bool ok6 = NkGeomRead(nd6, stride, v6, &n6, i6, &w6);
				const float32 d6 = NkGeomProbeMaxDelta(src, srcN, v6, n6, stride);
				bool idx6 = ok6 && i6.Size() == srcIdx.Size();
				for (usize k = 0; idx6 && k < srcIdx.Size(); ++k)
					idx6 = i6[k] == srcIdx[k];
				const bool vert = ok6 && idx6 && d6 >= 0.f && d6 < kGeomProbeEps;
				std::printf("[probe geom] C6 bloc de VERSION 1 (base64 brut, sans codage) : "
							"%u -> %u sommets | ecart max %.9f | indices %s | %s (%s)\n",
							(unsigned)srcN, (unsigned)n6, (double)d6, idx6 ? "oui" : "NON",
							vert ? "VERT" : "ROUGE", w6.Empty() ? "aucun refus" : w6.CStr());
				if (!vert)
					++rouges;
			}

			std::printf("[probe geom] VERDICT : %s (%d critere(s) rouge(s))\n",
						rouges == 0 ? "VERT" : "ROUGE", (int)rouges);
			return rouges;
		}

	} // namespace nk3d
} // namespace nkentseu
