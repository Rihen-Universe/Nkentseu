// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Viewport/NkCreaFamilles.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// L'ADAPTATEUR : de la GEOMETRIE des familles aux NOEUDS de la scene.
//
// ⚠️ LES CONSTRUCTEURS NE SONT PLUS ICI (22/09, Q10.1). Ils vivent dans
//    `Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkMeshFamilles.{h,cpp}`, pour
//    que Noge, Nogee et NKScena puissent construire une table sans passer par le
//    modeleur. Ce fichier ne fait plus qu'une chose, et c'est la SEULE que
//    l'application doit posseder : transformer des maillages en noeuds de scene.
//
// Ce qu'il reste tient en trois gestes : appeler la bibliotheque, creer un noeud
// par piece, rendre la liste. Si un jour il en fait plus, la coupure aura ete
// refaite dans le mauvais sens.
// -----------------------------------------------------------------------------
#include "NK3DModeler/Viewport/NkCreaFamilles.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h"
#include "NKRenderer/Mesh/NkMeshFamilles.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace nk3d {

		bool NkFamConnue(const char *f) {
			return renderer::NkFamilleConnue(f);
		}

		const char *NkFamNom(int32 i) {
			return renderer::NkFamilleNom(i);
		}

		bool NkFamFormulaire(const char *famille, char *out, uint32 cap) {
			return renderer::NkFamilleFormulaire(famille, out, cap);
		}

		bool NkFamValeurAutorisee(const char *famille, const char *champ, const char *valeur, char *remplacement,
								  uint32 cap) {
			return renderer::NkFamilleValeurAutorisee(famille, champ, valeur, remplacement, cap);
		}

		int32 NkFamConstruire(const NkFamParams &p, NkFamPiece *out, int32 cap, char *pourquoi, uint32 capPourquoi) {
			renderer::NkFamilleParams fp;
			snprintf(fp.famille, sizeof(fp.famille), "%s", p.famille);
			snprintf(fp.style, sizeof(fp.style), "%s", p.style);
			fp.largeur = p.largeur;
			fp.hauteur = p.hauteur;
			fp.profondeur = p.profondeur;
			fp.nombre = p.nombre;
			fp.fenetres = p.fenetres;
			fp.detaille = p.detaille;

			NkVector<renderer::NkFamillePiece> pieces;
			const int32 n = renderer::NkFamilleConstruire(fp, pieces, pourquoi, capPourquoi);
			if (n < 0)
				return -1;

			int32 nes = 0;
			uint32 faces = 0;
			const float32 zero[3] = {0.f, 0.f, 0.f};
			int32 noeudDe[64]; ///< indice de piece -> noeud, pour le parentage
			for (int32 k = 0; k < 64; ++k)
				noeudDe[k] = -1;
			for (int32 i = 0; i < n && nes < cap; ++i) {
				renderer::NkFamillePiece &q = pieces[(usize)i];
				const int32 nd = demo::Demo3DHostCreateMeshNode(-1, q.verts.Data(), (uint32)q.verts.Size(),
															   q.indices.Data(), (uint32)q.indices.Size(), zero, q.nom);
				if (nd < 0) {
					// ⚠️ ON DEFAIT CE QU'ON A FAIT. Une famille a moitie posee est pire
					//    qu'une famille refusee : l'utilisateur voit un objet ampute et rien
					//    ne lui dit qu'il manque des pieces.
					if (pourquoi)
						snprintf(pourquoi, capPourquoi,
								 "la famille « %s » n'a pas pu etre posee : plus d'emplacement de noeud libre "
								 "(%d piece(s) sur %d)",
								 p.famille, (int)nes, (int)n);
					for (int32 k = 0; k < nes; ++k)
						demo::Demo3DHostDeleteNode(out[k].noeud, false);
					return -1;
				}
				demo::Demo3DHostSetNodeIsMesh(nd, false);
				if (i < 64)
					noeudDe[i] = nd;
				// ── (Q18.1/18.2) LE PARENT ET LE PIVOT, POSES PAR LES PORTES EXISTANTES ──
				// ⚠️ AUCUNE MECANIQUE NEUVE : `Demo3DHostSetNodeOrigin` est le pivot que
				//    le modeleur applique deja a tout noeud (« point autour duquel il
				//    tourne ; la deplacer NE DEPLACE PAS la matiere »), et
				//    `Demo3DHostSetNodeParent` la hierarchie qu'il a depuis toujours.
				//    La famille ne fait que REMPLIR ce que ces deux portes attendent.
				// ⚠️ ET L'ORIGINE SE POSE AVANT LE PARENT : la reparenter apres
				//    deplacerait le pivot avec la transformation du parent, et l'arete
				//    des gonds ne serait plus sur les gonds.
				// ⚠️ LE PIVOT EST ETEINT PAR DEFAUT, ET C'EST MESURE (25/09).
				//    `Demo3DHostSetNodeOrigin` dit « la deplacer NE DEPLACE PAS la
				//    matiere -- LES ENFANTS SONT RECULES D'AUTANT ». C'est la seconde
				//    moitie de la phrase qui mord : la rosace et la poignee sont des
				//    ENFANTS du battant, et poser le pivot du battant les a poussees.
				//    Mutation NK_ARTIC_PIVOT, sur « modelise moi cette porte » :
				//        avec pivot : boite 4,46 x 3,20 m, 4 pieces FLOTTANTES
				//                     (rosace, poignee, rosace_2, poignee_2)
				//        sans pivot : boite 2,40 x 3,20 m, 0 flottante
				//    Une porte de 4,46 m est pire que pas d'articulation du tout : on
				//    laisse donc le pivot derriere une porte nommee jusqu'a ce que
				//    l'ordre soit juste (poser l'origine APRES avoir reparent les
				//    enfants, ou compenser leur recul). LE PARENTAGE, LUI, RESTE : il
				//    ne deplace rien et il est ce que Rodolf veut voir dans la
				//    hierarchie.
				//    ⚠️ CONDITION DE RETRAIT : cette porte disparait le jour ou la
				//    boite de la porte simple reste a 2,40 m et le compte de flottantes
				//    a 0 avec le pivot allume.
				const char *avecPivot = std::getenv("NK_ARTIC_PIVOT");
				if (avecPivot && avecPivot[0] && avecPivot[0] != '0' &&
					(q.pivot[0] != 0.f || q.pivot[1] != 0.f || q.pivot[2] != 0.f))
					demo::Demo3DHostSetNodeOrigin(nd, q.pivot);
				if (q.parent >= 0 && q.parent < i && q.parent < 64 && noeudDe[q.parent] >= 0)
					(void)demo::Demo3DHostSetNodeParent(nd, noeudDe[q.parent]);
				NkFamPiece &P = out[nes++];
				P.noeud = nd;
				snprintf(P.nom, sizeof(P.nom), "%s", q.nom);
				snprintf(P.matiere, sizeof(P.matiere), "%s", q.matiere);
				P.faces = q.faces;
				faces += q.faces;
			}
			int32 nLiaisons = 0;
			for (int32 i = 0; i < n && i < 64; ++i)
				if (pieces[(usize)i].liaison != renderer::NkFamilleLiaison::Fixe)
					++nLiaisons;
			NkLog::Instance().Infof("[famille] %s style=%s detail=%s : %d pieces, %u faces, %d liaison(s) "
									"(bibliotheque NKRenderer)",
									p.famille, p.style, p.detaille ? "detaille" : "simple", (int)nes, (unsigned)faces,
									(int)nLiaisons);
			std::printf("[famille] %s : %d piece(s), %d liaison(s) declaree(s) ; parent pose, pivot SOUS NK_ARTIC_PIVOT\n", p.famille,
						(int)nes, (int)nLiaisons);
			std::fflush(stdout);
			return nes;
		}

	} // namespace nk3d
} // namespace nkentseu
