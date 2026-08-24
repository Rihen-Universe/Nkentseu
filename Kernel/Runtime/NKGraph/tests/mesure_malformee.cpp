// Banc n°3 : une directive CONNUE mais MAL FORMEE — le troisieme cas
// (corruption silencieuse) existe-t-il ?
#include "NKGraph/NkGraphDocument.h"
#include <stdio.h>
#include <string.h>
using namespace nkentseu;
using namespace nkentseu::graph;

static void Essai(const char* titre, const char* texte) {
    printf("\n[%s]\n", titre);
    NkNodeGraph r;
    const bool ok = r.Deserialize(texte);
    printf("  Deserialize -> %s ; noeuds vivants=%u bruts=%u liens=%u\n",
           ok ? "true" : "false", r.NodeCount(), r.RawNodeCount(), r.LinkCount());
    for (uint32 i = 0; i < r.RawNodeCount(); ++i) {
        const NkNode *n = r.RawNodeAt(i);
        printf("    noeud id=%u type='%s' libelle='%s' x=%.1f y=%.1f prises=%u%s\n",
               n->id, n->type.CStr(), n->label.CStr(), n->x, n->y,
               (uint32)n->sockets.Size(), n->id == NK_NODE_INVALID ? "   <-- ID INVALIDE (0)" : "");
    }
    NkString s; r.Serialize(s);
    printf("  reserialise :\n%s", s.CStr());
}

int main() {
    printf("=== MESURE n3 : directive CONNUE mais MAL FORMEE ===\n");

    Essai("1. `noeud` avec deux jetons au lieu de cinq",
        "nkgraph 1\n"
        "compteurs 3 2\n"
        "type 1 float\n"
        "noeud 1 10.000000 20.000000 math.add Addition\n"
        "noeud 2\n"
        "lien 1 1 0 2 0\n");

    Essai("2. `noeud` avec un identifiant NON NUMERIQUE",
        "nkgraph 1\n"
        "compteurs 3 2\n"
        "noeud abc 10.0 20.0 math.add Addition\n");

    Essai("3. `lien` vers un noeud qui n'existe pas",
        "nkgraph 1\n"
        "compteurs 2 2\n"
        "noeud 1 0 0 math.add A\n"
        "lien 1 1 0 99 0\n");

    Essai("4. `sock` rattache a un noeud absent",
        "nkgraph 1\n"
        "compteurs 2 1\n"
        "type 1 float\n"
        "sock 42 0 1 Orpheline\n"
        "noeud 1 0 0 math.add A\n");

    Essai("5. ligne `compteurs` ABSENTE",
        "nkgraph 1\n"
        "type 1 float\n"
        "noeud 7 0 0 math.add A\n");

    Essai("6. deux noeuds portant le MEME identifiant",
        "nkgraph 1\n"
        "compteurs 3 1\n"
        "noeud 1 0 0 math.add Premier\n"
        "noeud 1 5 5 math.mul Second\n");

    printf("\n=== fin n3 ===\n");
    return 0;
}
