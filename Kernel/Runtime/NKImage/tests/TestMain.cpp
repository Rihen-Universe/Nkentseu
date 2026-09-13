// =============================================================================
// @File    TestMain.cpp
// @Brief   LE point d'entree unique de NKImage_Tests : appelle chaque banc et
//          rend un code de sortie qui dit la verite.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (2026-09-05)
// =============================================================================
//  La suite declare `testownmain()` : elle fournit son `main`, et Jenga 2.6 en
//  REFUSE deux. Ajouter un banc SVG a cote de TestEXR imposait donc de choisir :
//  un second `main` (refuse), ou un point d'entree unique qui appelle les deux.
//  Chaque banc devient une fonction qui rend 0 ou 1 ; ici on les enchaine TOUS
//  (jamais de court-circuit au premier echec : on veut le tableau complet, pas
//  la premiere panne) et on rend 1 des qu'un seul a rougi.
// =============================================================================

#include <cstdio>

int TestEXR_Run();
int TestSVG_Run();

int main() {
	int codes = 0;
	codes |= (TestEXR_Run() != 0) ? 1 : 0;
	codes |= (TestSVG_Run() != 0) ? 1 : 0;
	std::printf("\n===== NKImage_Tests : %s =====\n", codes == 0 ? "VERT" : "ROUGE");
	return codes;
}
