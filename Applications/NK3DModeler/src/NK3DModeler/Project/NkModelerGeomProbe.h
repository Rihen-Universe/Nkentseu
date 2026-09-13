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

			std::printf("[probe geom] VERDICT : %s (%d critere(s) rouge(s))\n",
						rouges == 0 ? "VERT" : "ROUGE", (int)rouges);
			return rouges;
		}

	} // namespace nk3d
} // namespace nkentseu
