#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerGeom.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// =============================================================================
// NkModelerGeom.h — LA GEOMETRIE DES OBJETS IMPORTES, ECRITE A COTE DE SON ASSET.
//
// 🔴 POURQUOI CE FICHIER EXISTE — LE DEFAUT DES CUBES BLANCS (Rodolf, 06/09).
//   « Les models que j'avais charges precedemment, une fois rouvert le projet,
//   elles sont devenues des cubes. »
//
//   La cause, LUE DANS LE FICHIER de Rodolf et pas devinee : un objet importe
//   naissait par `Demo3DHostCreateMeshNode`, qui l'alloue en NATURE 2 -- la
//   famille CUBE -- et lui donne sa geometrie dans `nkvpUserMesh`. Le format
//   ecrivait la nature, le sous-type, la transform, les materiaux... et RIEN de
//   la geometrie. A la relecture, `Demo3DHostAddNode(2, 0)` recreait donc un
//   cube parfaitement valide. Le dessin etait juste ; c'est l'attribut qui
//   manquait, et il manquait ENTIEREMENT : ni sommets, ni chemin vers le
//   fichier d'origine. Rien a rattraper.
//
//   Verifiable dans son projet : `AgentTest/042082ea....nkmesh`, 833 octets pour
//   un mannequin de 249 906 sommets. Le fichier ne porte pas un seul sommet.
//
// ── POURQUOI UN FICHIER BINAIRE A COTE, ET PAS DU JSON DANS L'ASSET ─────────
//   250 000 sommets en JSON, c'est ~40 Mo de texte pour 12 Mo de donnees, une
//   relecture par analyse lexicale, et un `.nkmesh` qu'un humain ne peut plus
//   ouvrir pour comprendre ce qu'il contient. Le `.nkmesh` reste LISIBLE et
//   court -- il decrit la structure ; son `.nkgeo` frere porte la matiere.
//   Meme partage que le `.nkmat` et sa texture : le petit fichier designe, le
//   gros contient.
//
// ── CE QUI EST ECRIT, ET CE QUI NE L'EST PAS ────────────────────────────────
//   Ecrit : la geometrie des noeuds qui portent LEUR PROPRE maillage avec une
//   copie CPU -- c'est exactement l'ensemble des objets importes, et des
//   maillages internes d'un model.
//   Pas ecrit : les primitives du menu (sphere, cylindre, cone, plan, cube).
//   Elles se REGENERENT depuis leur nature et leurs parametres de creation ;
//   les ecrire couterait des megaoctets par projet pour reproduire ce que trois
//   entiers disent deja.
//
//   ⚠️ LE FILTRE EST `Demo3DHostMeshParams`, PAS `Demo3DHostNodeGeometry`.
//   Une sphere, un cylindre, un cone et un plan portent bel et bien LEUR PROPRE
//   maillage, avec sa copie CPU -- `HostRegenUserMesh` le leur fabrique --, donc
//   `Demo3DHostNodeGeometry` rend VRAI pour eux. Seul le cube nu n'a pas de
//   maillage a lui. La bonne question n'est pas « ce noeud a-t-il un maillage »
//   mais « ce maillage se REGENERE-t-il », et c'est le bloc « creation » qui y
//   repond -- le meme test des deux cotes, une seule reponse.
//
// ── LE RANG, PAS LE NUMERO DE NOEUD ─────────────────────────────────────────
//   Une entree designe son noeud par son RANG dans le tableau « noeuds » de
//   l'asset frere -- la meme convention que la parente et les materiaux. Les
//   emplacements de noeud se recyclent d'une session a l'autre : s'y fier
//   rendrait la geometrie a un autre objet, ce qui est pire que de la perdre.
//
// ── L'ORDRE DES OCTETS ET LE PAS DE SOMMET ──────────────────────────────────
//   Ecriture NATIVE, sans conversion. Ce format n'est pas un format d'echange :
//   il vit A COTE d'un projet, sur la machine qui l'a ecrit. Le PAS DE SOMMET
//   est ecrit dans chaque entree, et la relecture REFUSE un pas different du
//   layout courant plutot que de reinterpreter les octets -- une geometrie
//   fausse sans message serait pire que le cube, parce qu'un cube se voit.
//   Le jour ou un projet doit voyager entre machines d'endianness differentes,
//   c'est ici que la conversion se pose, et la version du format monte.
//   Dette NOMMEE, pas subie.
// =============================================================================

#include "NK3DModeler/Project/NkModelerScene.h" // NkScToAbs / NkScNorm

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKLogger/NkLog.h"

#include <cstring>

namespace nkentseu {
	namespace nk3d {

		/// Version du format de geometrie, DISTINCTE de celle de l'asset : le
		/// jour ou le layout de sommet change, c'est ce nombre qui monte, et un
		/// `.nkmesh` inchange n'a pas a etre reecrit pour autant.
		static const uint32 kGeoFormatVersion = 1;
		static const uint32 kGeoHeaderBytes = 16;
		static const uint32 kGeoEntryBytes = 16;

		/// Chemin du `.nkgeo` frere d'un asset : meme dossier, meme nom, autre
		/// extension. Un asset renomme ou deplace emporte donc sa geometrie par
		/// le meme geste -- il n'y a pas de second chemin a tenir a jour.
		inline NkString NkGeoRelFor(const NkString &assetRel) {
			if (assetRel.Empty())
				return NkString();
			const NkString::SizeType dot = assetRel.RFind('.');
			const NkString::SizeType sl = assetRel.RFind('/');
			NkString out;
			if (dot != NkString::npos && (sl == NkString::npos || dot > sl))
				out = NkString(assetRel.CStr(), dot);
			else
				out = assetRel;
			out += ".nkgeo";
			return out;
		}

		// ── ECRITURE ────────────────────────────────────────────────────────────
		/// Un tampon qu'on remplit noeud par noeud pendant la capture, puis qu'on
		/// verse en une fois. Ecrire au fil de l'eau ouvrirait le fichier avant de
		/// savoir s'il aura un seul octet a porter.
		struct NkGeoBuilder {
				NkVector<uint8> body;
				uint32 count = 0;
				uint64 bytes = 0; ///< pour le journal : ce que la geometrie coute
		};

		/// Un bloc d'octets s'ajoute par UN agrandissement et UNE recopie. Un
		/// `PushBack` par octet ferait douze millions d'appels pour un mannequin,
		/// et le cout ne se verrait qu'a l'enregistrement -- au pire moment.
		inline void NkGeoPutBytes(NkVector<uint8> &out, const void *src, usize n) {
			if (!src || n == 0)
				return;
			const usize base = out.Size();
			out.Resize(base + n);
			std::memcpy(out.Data() + base, src, n);
		}

		inline void NkGeoPutU32(NkVector<uint8> &out, uint32 v) {
			NkGeoPutBytes(out, &v, sizeof(v));
		}

		/// Ajoute la geometrie d'un noeud, sous son RANG dans l'asset frere.
		inline void NkGeoAdd(NkGeoBuilder &g, int32 rank, const void *verts, uint32 vcount,
							 uint32 stride, const uint32 *indices, uint32 icount) {
			if (rank < 0 || !verts || !indices || vcount == 0 || icount == 0 || stride == 0)
				return;
			NkGeoPutU32(g.body, (uint32)rank);
			NkGeoPutU32(g.body, vcount);
			NkGeoPutU32(g.body, stride);
			NkGeoPutU32(g.body, icount);
			NkGeoPutBytes(g.body, verts, (usize)vcount * (usize)stride);
			NkGeoPutBytes(g.body, indices, (usize)icount * sizeof(uint32));
			++g.count;
			g.bytes += (uint64)vcount * (uint64)stride + (uint64)icount * 4u;
		}

		/// Verse le tampon dans `root/rel`. Un tampon VIDE ne laisse pas un
		/// fichier vide derriere lui : il EFFACE le `.nkgeo` precedent, sinon un
		/// asset dont on a supprime le dernier import garderait sa geometrie
		/// morte et la rendrait a un rang qui designe autre chose.
		inline bool NkGeoWrite(const NkString &root, const NkString &rel, const NkGeoBuilder &g,
							   NkString *err) {
			const NkString abs = NkScToAbs(root, rel.CStr());
			if (abs.Empty())
				return true;
			if (g.count == 0) {
				if (NkFile::Exists(abs.CStr()))
					(void)NkFile::Delete(abs.CStr());
				return true;
			}
			const NkString::SizeType s = abs.RFind('/');
			if (s != NkString::npos) {
				const NkString dir(abs.CStr(), s);
				if (!dir.Empty() && !NkDirectory::Exists(dir.CStr()) &&
					!NkDirectory::CreateRecursive(dir.CStr())) {
					if (err)
						*err = NkString("dossier impossible a creer : ") + dir;
					return false;
				}
			}
			NkVector<uint8> file;
			file.Reserve(g.body.Size() + (usize)kGeoHeaderBytes);
			const uint8 magic[8] = {'N', 'K', 'G', 'E', 'O', '1', 0, 0};
			NkGeoPutBytes(file, magic, sizeof(magic));
			NkGeoPutU32(file, kGeoFormatVersion);
			NkGeoPutU32(file, g.count);
			NkGeoPutBytes(file, g.body.Data(), g.body.Size());
			if (!NkFile::WriteAllBytes(abs.CStr(), file)) {
				if (err)
					*err = NkString("ecriture impossible : ") + abs;
				return false;
			}
			return true;
		}

		// ── LECTURE ─────────────────────────────────────────────────────────────
		/// Le fichier entier en memoire, plus la table de ses entrees. On lit tout
		/// d'un coup parce que la restauration touche TOUS les rangs d'un asset :
		/// rouvrir le fichier par noeud coutrait un aller-retour disque par objet.
		struct NkGeoEntry {
				uint32 rank = 0;
				uint32 vcount = 0;
				uint32 stride = 0;
				uint32 icount = 0;
				usize vOff = 0; ///< offset des sommets DANS `bytes`
				usize iOff = 0; ///< offset des indices DANS `bytes`
		};

		struct NkGeoFile {
				NkVector<uint8> bytes;
				NkVector<NkGeoEntry> entries;
				bool present = false; ///< le fichier existe et s'est laisse lire
		};

		inline uint32 NkGeoGetU32(const NkVector<uint8> &b, usize off) {
			uint32 v = 0;
			if (off + 4u <= b.Size())
				std::memcpy(&v, b.Data() + off, sizeof(v));
			return v;
		}

		/// Charge `root/rel`. Un fichier ABSENT n'est pas une erreur (l'asset n'a
		/// peut-etre aucune geometrie propre) : `present` reste faux et l'appelant
		/// decide. Un fichier PRESENT MAIS ILLISIBLE, lui, est dit -- c'est le cas
		/// ou du travail a ete perdu, et il ne doit pas se confondre avec l'autre.
		inline bool NkGeoRead(const NkString &root, const NkString &rel, NkGeoFile &out) {
			out.entries.Clear();
			out.present = false;
			const NkString abs = NkScToAbs(root, rel.CStr());
			if (abs.Empty() || !NkFile::Exists(abs.CStr()))
				return false;
			out.bytes = NkFile::ReadAllBytes(abs.CStr());
			if (out.bytes.Size() < (usize)kGeoHeaderBytes) {
				NkLog::Instance().Warnf("[geom] « %s » ILLISIBLE : %u octets, l'en-tete en demande %u.",
										rel.CStr(), (unsigned)out.bytes.Size(),
										(unsigned)kGeoHeaderBytes);
				return false;
			}
			const uint8 *p = out.bytes.Data();
			if (p[0] != 'N' || p[1] != 'K' || p[2] != 'G' || p[3] != 'E' || p[4] != 'O' ||
				p[5] != '1') {
				NkLog::Instance().Warnf("[geom] « %s » ILLISIBLE : ce n'est pas un fichier NKGEO.",
										rel.CStr());
				return false;
			}
			const uint32 ver = NkGeoGetU32(out.bytes, 8);
			if (ver > kGeoFormatVersion) {
				NkLog::Instance().Warnf("[geom] « %s » ecrit par une version plus recente "
										"(format %u, connu %u) : NON LU.",
										rel.CStr(), (unsigned)ver, (unsigned)kGeoFormatVersion);
				return false;
			}
			const uint32 n = NkGeoGetU32(out.bytes, 12);
			usize off = (usize)kGeoHeaderBytes;
			for (uint32 k = 0; k < n; ++k) {
				if (off + (usize)kGeoEntryBytes > out.bytes.Size()) {
					NkLog::Instance().Warnf("[geom] « %s » TRONQUE a l'entree %u/%u : les objets "
											"suivants n'ont plus leur maillage.",
											rel.CStr(), (unsigned)k, (unsigned)n);
					break;
				}
				NkGeoEntry e;
				e.rank = NkGeoGetU32(out.bytes, off + 0u);
				e.vcount = NkGeoGetU32(out.bytes, off + 4u);
				e.stride = NkGeoGetU32(out.bytes, off + 8u);
				e.icount = NkGeoGetU32(out.bytes, off + 12u);
				off += (usize)kGeoEntryBytes;
				const usize vb = (usize)e.vcount * (usize)e.stride;
				const usize ib = (usize)e.icount * sizeof(uint32);
				if (e.vcount == 0 || e.stride == 0 || e.icount == 0 ||
					off + vb + ib > out.bytes.Size()) {
					NkLog::Instance().Warnf("[geom] « %s » TRONQUE a l'entree %u/%u (rang %u) : "
											"les objets suivants n'ont plus leur maillage.",
											rel.CStr(), (unsigned)k, (unsigned)n,
											(unsigned)e.rank);
					break;
				}
				e.vOff = off;
				e.iOff = off + vb;
				off += vb + ib;
				out.entries.PushBack(e);
			}
			out.present = true;
			return !out.entries.Empty();
		}

		/// L'entree d'un rang, ou nullptr. Les indices sont RECOPIES par
		/// l'appelant s'il en a besoin alignes ; les sommets se lisent en place
		/// (le systeme de maillages les memcpy).
		inline const NkGeoEntry *NkGeoFind(const NkGeoFile &f, int32 rank) {
			if (rank < 0)
				return nullptr;
			for (usize i = 0; i < f.entries.Size(); ++i)
				if (f.entries[i].rank == (uint32)rank)
					return &f.entries[i];
			return nullptr;
		}

		// ═══════════════════════════════════════════════════════════════════
		// LA SONDE DU FORMAT — sans fenetre, sans GPU, sans un clic
		// ═══════════════════════════════════════════════════════════════════
		// `NK3DModeler.exe --sonde-geo` la lance et rend 0 si tout est VERT.
		//
		// POURQUOI ELLE EST ICI ET PAS AILLEURS : elle eprouve exactement le
		// fichier qui la porte. Une sonde dans un autre fichier se met a decrire
		// ce qu'elle croit du format ; celle-ci ne peut pas diverger de lui.
		//
		// ⚠️ CE QU'ELLE NE COUVRE PAS, ET IL FAUT LE LIRE AVANT DE S'Y FIER :
		// elle eprouve la COUCHE FICHIER (ecriture, relecture, refus des
		// fichiers abimes). Elle ne touche NI le systeme de maillages NI la
		// sauvegarde du projet : ceux-la demandent un device, donc une fenetre,
		// donc des gestes. Un aller-retour REEL -- importer, enregistrer,
		// fermer, rouvrir -- reste un test HUMAIN. Cette sonde peut etre verte
		// pendant que la chaine complete est cassee ; elle dit que le format
		// tient, pas que le modeleur s'en sert.
		//
		// CHAQUE LIGNE A ETE VUE ROUGE SOUS SA MUTATION : les quatre controles
		// 2 a 5 abiment le fichier a la main (nombre magique, version, coupure)
		// et exigent un REFUS. Retirer le controle correspondant dans NkGeoRead
		// les fait passer au rouge -- c'est ce qui les rend autre chose qu'un
		// commentaire.
		inline int32 NkGeoSonde(const NkString &dir) {
			NkString root = dir.Empty() ? NkString(".") : dir;
			NkString rap;
			int32 rouges = 0;
			auto dire = [&](const char *nom, bool vert, const char *detail) {
				char l[512];
				snprintf(l, sizeof(l), "%s  %-42s  %s\n", vert ? "VERT " : "ROUGE", nom,
						 detail ? detail : "");
				rap += l;
				if (!vert)
					++rouges;
			};

			// Deux entrees : une minuscule, une assez grosse pour sortir de tout
			// tampon d'essai. Les rangs 0 et 7 ne se suivent pas : un lecteur qui
			// confondrait le rang et la position dans la table passerait le
			// premier controle et echouerait ici.
			const uint32 stride = 48u;
			const uint32 vcA = 3u, icA = 6u;
			const uint32 vcB = 5000u, icB = 30000u;
			NkVector<uint8> vA, vB;
			NkVector<uint32> iA, iB;
			vA.Resize((usize)vcA * stride);
			for (usize i = 0; i < vA.Size(); ++i)
				vA[i] = (uint8)((i * 37u + 11u) & 0xFFu);
			vB.Resize((usize)vcB * stride);
			for (usize i = 0; i < vB.Size(); ++i)
				vB[i] = (uint8)((i * 131u + 7u) & 0xFFu);
			iA.Resize((usize)icA);
			for (usize i = 0; i < iA.Size(); ++i)
				iA[i] = (uint32)(i * 3u);
			iB.Resize((usize)icB);
			for (usize i = 0; i < iB.Size(); ++i)
				iB[i] = (uint32)(i % vcB);

			NkGeoBuilder g;
			NkGeoAdd(g, 0, vA.Data(), vcA, stride, iA.Data(), icA);
			NkGeoAdd(g, 7, vB.Data(), vcB, stride, iB.Data(), icB);
			const NkString rel = "sonde_geo.nkgeo";
			NkString err;

			// (1) ALLER-RETOUR : ce qui sort est OCTET POUR OCTET ce qui entre.
			{
				const bool ecrit = NkGeoWrite(root, rel, g, &err);
				NkGeoFile f;
				const bool lu = ecrit && NkGeoRead(root, rel, f);
				bool ok = lu && f.entries.Size() == 2u;
				const NkGeoEntry *eA = ok ? NkGeoFind(f, 0) : nullptr;
				const NkGeoEntry *eB = ok ? NkGeoFind(f, 7) : nullptr;
				ok = ok && eA && eB && eA->vcount == vcA && eA->icount == icA &&
					 eA->stride == stride && eB->vcount == vcB && eB->icount == icB;
				if (ok)
					ok = std::memcmp(f.bytes.Data() + eA->vOff, vA.Data(), vA.Size()) == 0 &&
						 std::memcmp(f.bytes.Data() + eA->iOff, iA.Data(),
									 (usize)icA * sizeof(uint32)) == 0 &&
						 std::memcmp(f.bytes.Data() + eB->vOff, vB.Data(), vB.Size()) == 0 &&
						 std::memcmp(f.bytes.Data() + eB->iOff, iB.Data(),
									 (usize)icB * sizeof(uint32)) == 0;
				char d[160];
				snprintf(d, sizeof(d), "2 entrees (rangs 0 et 7), %u + %u sommets, octet pour octet",
						 (unsigned)vcA, (unsigned)vcB);
				dire("(1) aller-retour ecriture/relecture", ok, d);
			}

			const NkString abs = NkScToAbs(root, rel.CStr());
			NkVector<uint8> sain = NkFile::ReadAllBytes(abs.CStr());

			// (2) MUTATION : le nombre magique. Un fichier qui n'est pas un
			// NKGEO doit etre REFUSE, jamais lu de travers.
			{
				NkVector<uint8> m = sain;
				if (m.Size() > 2u)
					m[2] = (uint8)'X';
				(void)NkFile::WriteAllBytes(abs.CStr(), m);
				NkGeoFile f;
				const bool lu = NkGeoRead(root, rel, f);
				dire("(2) mutation du nombre magique", !lu, "refuse et journalise ILLISIBLE");
			}

			// (3) MUTATION : une version FUTURE. Lire un format qu'on ne connait
			// pas donnerait une geometrie fausse sans erreur.
			{
				NkVector<uint8> m = sain;
				const uint32 futur = kGeoFormatVersion + 99u;
				if (m.Size() >= 12u)
					std::memcpy(m.Data() + 8, &futur, sizeof(futur));
				(void)NkFile::WriteAllBytes(abs.CStr(), m);
				NkGeoFile f;
				const bool lu = NkGeoRead(root, rel, f);
				dire("(3) mutation de la version (plus recente)", !lu, "refuse, non lu");
			}

			// (4) MUTATION : le fichier est COUPE au milieu de la seconde
			// entree. La premiere reste bonne : on garde ce qui est entier et on
			// DIT ce qui manque -- perdre les deux serait aussi faux que de
			// rendre la seconde a moitie.
			{
				NkVector<uint8> m;
				const usize coupe = sain.Size() - (usize)vcB * stride / 2u;
				m.Resize(coupe);
				std::memcpy(m.Data(), sain.Data(), coupe);
				(void)NkFile::WriteAllBytes(abs.CStr(), m);
				NkGeoFile f;
				const bool lu = NkGeoRead(root, rel, f);
				const bool ok = lu && f.entries.Size() == 1u && f.entries[0].rank == 0u;
				dire("(4) mutation : fichier coupe en deux", ok,
					 "garde l'entree entiere, TRONQUE pour l'autre");
			}

			// (5) MUTATION : le fichier ne fait plus la taille de son en-tete.
			{
				NkVector<uint8> m;
				m.Resize(5u);
				(void)NkFile::WriteAllBytes(abs.CStr(), m);
				NkGeoFile f;
				const bool lu = NkGeoRead(root, rel, f);
				dire("(5) mutation : plus court que l'en-tete", !lu, "refuse");
			}

			// (6) UN TAMPON VIDE EFFACE le fichier precedent. Sans cela, un
			// asset dont on retire le dernier import garderait une geometrie
			// morte, rendue a un rang qui designe autre chose.
			{
				NkGeoBuilder vide;
				const bool ecrit = NkGeoWrite(root, rel, vide, &err);
				const bool parti = !NkFile::Exists(abs.CStr());
				dire("(6) tampon vide : le .nkgeo precedent part", ecrit && parti,
					 "aucun fichier vide laisse derriere");
			}

			// (7) LE NOM DU FRERE : meme dossier, meme nom, autre extension.
			{
				const bool ok = NkGeoRelFor("Dossier/Model.nkmesh") == NkString("Dossier/Model.nkgeo") &&
								NkGeoRelFor("Scene1.nkscene") == NkString("Scene1.nkgeo") &&
								NkGeoRelFor("Dos.sier/Sans") == NkString("Dos.sier/Sans.nkgeo");
				dire("(7) nom du .nkgeo frere", ok, "extension remplacee, point du dossier epargne");
			}

			char tete[512];
			snprintf(tete, sizeof(tete),
					 "SONDE GEOMETRIE — NkModelerGeom.h, format %u\n"
					 "Regime : COUCHE FICHIER seule. Ni maillage, ni GPU, ni sauvegarde de\n"
					 "projet — ceux-la demandent une fenetre, donc des gestes humains.\n"
					 "-----------------------------------------------------------------\n",
					 (unsigned)kGeoFormatVersion);
			NkString sortie = NkString(tete) + rap;
			char pied[128];
			snprintf(pied, sizeof(pied), "-----------------------------------------------------------------\n%s (%d rouge(s))\n",
					 rouges == 0 ? "TOUT VERT" : "ECHEC", rouges);
			sortie += pied;
			(void)NkFile::WriteAllText(NkScToAbs(root, "sonde_geo.txt").CStr(), sortie.CStr());
			NkLog::Instance().Warnf("[sonde-geo]\n%s", sortie.CStr());
			return rouges == 0 ? 0 : 1;
		}

	} // namespace nk3d
} // namespace nkentseu
