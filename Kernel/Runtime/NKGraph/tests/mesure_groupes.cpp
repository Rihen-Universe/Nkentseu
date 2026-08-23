// Banc de mesure n°2 — R8 point 4 :
//  (a) le registre de types accepte-t-il un type enregistre A L'EXECUTION ?
//  (b) le « groupe » (definition + instances) existe-t-il deja dans le modele ?
//  (c) la definition est-elle ecrite UNE SEULE FOIS a la serialisation ?
//  (d) que se passe-t-il quand on modifie la definition APRES instanciation ?
#include "NKGraph/NkGraphDocument.h"
#include <stdio.h>
#include <string.h>

using namespace nkentseu;
using namespace nkentseu::graph;

static int gFail = 0;
#define CHECK(cond, msg) do { if(!(cond)) { printf("  [ECHEC] %s\n", msg); ++gFail; } else { printf("  [ok]    %s\n", msg); } } while(0)

static uint32 Compte(const char* hay, const char* needle) {
    uint32 n = 0; const char* p = hay; size_t l = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) { ++n; p += l; }
    return n;
}

int main() {
    printf("=== MESURE n2 : types a l'execution et GROUPES ===\n\n");

    // ---------------------------------------------------------------- (a) ---
    printf("[A] enregistrement d'un type APRES coup, sur un graphe deja peuple\n");
    {
        NkNodeGraph g;
        const NkTypeId t1 = g.RegisterType("float");
        NkNodeId a = g.AddNode("math.add", "A");
        g.AddSocket(a, "In", t1, NkSocketDir::Input);
        // type enregistre APRES que le graphe existe et porte deja des noeuds
        const NkTypeId t2 = g.RegisterType("monType.utilisateur");
        NkNodeId b = g.AddNode("user.node", "B");
        const bool okSock = g.AddSocket(b, "Out", t2, NkSocketDir::Output);
        const NkLinkError le = g.Connect(b, "Out", a, "In", NULL);
        printf("  t1=%u t2=%u ; AddSocket=%s ; Connect(t2->t1)=%s\n",
               t1, t2, okSock ? "true" : "false", NkLinkErrorName(le));
        CHECK(t2 != NK_TYPE_INVALID && t2 != t1, "A1 : un type enregistre a l'execution obtient un identifiant neuf");
        CHECK(okSock, "A2 : une prise peut porter ce type");
        CHECK(le == NkLinkError::TypeMismatch, "A3 : le type neuf est bien DISTINCT (refus de connexion attendu)");
        g.AllowConversion(t2, t1);
        const NkLinkError le2 = g.Connect(b, "Out", a, "In", NULL);
        printf("  apres AllowConversion : Connect=%s\n", NkLinkErrorName(le2));
        CHECK(le2 == NkLinkError::Ok, "A4 : une conversion declaree a l'execution est prise en compte");

        NkString s; g.Serialize(s);
        NkNodeGraph r; r.Deserialize(s.CStr());
        NkString s2; r.Serialize(s2);
        CHECK(strcmp(s.CStr(), s2.CStr()) == 0, "A5 : le type cree a l'execution SURVIT a l aller-retour");
        const NkTypeId retrouve = r.FindType("monType.utilisateur");
        printf("  FindType apres rechargement -> %u (avant : %u)\n", retrouve, t2);
        CHECK(retrouve == t2, "A6 : il conserve le MEME identifiant apres rechargement");
    }

    // ---------------------------------------------------------------- (b) ---
    printf("\n[B] un GROUPE : definition unique, deux instances\n");
    NkGraphDocument doc;
    const uint32 gRacine = doc.AddGraph("racine");
    const uint32 gGroupe = doc.AddGraph("MonGroupe");
    doc.SetRoot(gRacine);

    NkTypeId tf = 0;
    {   // --- interieur du groupe : entree -> traitement -> sortie
        NkNodeGraph &G = doc.GraphAt(gGroupe);
        tf = G.RegisterType("float");
        NkNodeId in  = G.AddNode(NK_NODE_GROUP_IN,  "Entrees du groupe");
        NkNodeId mul = G.AddNode("math.mul",        "Multiplier");
        NkNodeId out = G.AddNode(NK_NODE_GROUP_OUT, "Sorties du groupe");
        G.AddSocket(in,  "V", tf, NkSocketDir::Output); // l'interface se declare DE L'INTERIEUR
        G.AddSocket(mul, "A", tf, NkSocketDir::Input);
        G.AddSocket(mul, "R", tf, NkSocketDir::Output);
        G.AddSocket(out, "R", tf, NkSocketDir::Input);
        const NkLinkError e1 = G.Connect(in,  "V", mul, "A", NULL);
        const NkLinkError e2 = G.Connect(mul, "R", out, "R", NULL);
        printf("  interieur : %s / %s\n", NkLinkErrorName(e1), NkLinkErrorName(e2));
        CHECK(e1 == NkLinkError::Ok && e2 == NkLinkError::Ok, "B1 : l'interface se declare par deux noeuds ORDINAIRES");
    }
    {   // --- racine : DEUX instances du meme groupe
        NkNodeGraph &G = doc.GraphAt(gRacine);
        const NkTypeId t = G.RegisterType("float");
        NkNodeId src = G.AddNode("const.float", "Source");
        G.AddSocket(src, "R", t, NkSocketDir::Output);
        for (int i = 0; i < 2; ++i) {
            NkNodeId inst = G.AddNode(NK_NODE_INSTANCE, i == 0 ? "Instance 1" : "Instance 2");
            G.Find(inst)->subgraph = NkString("MonGroupe"); // reference, pas copie
            G.AddSocket(inst, "V", t, NkSocketDir::Input);
            G.AddSocket(inst, "R", t, NkSocketDir::Output);
            const NkLinkError e = G.Connect(src, "R", inst, "V", NULL);
            CHECK(e == NkLinkError::Ok, i == 0 ? "B2 : instance 1 branchee" : "B3 : instance 2 branchee");
        }
    }

    // ---------------------------------------------------------------- (c) ---
    printf("\n[C] la definition est-elle ecrite UNE SEULE FOIS ?\n");
    NkString ser; doc.Serialize(ser);
    const uint32 nDef  = Compte(ser.CStr(), "graphe MonGroupe");
    const uint32 nRef  = Compte(ser.CStr(), "sousgraphe ");
    printf("  `graphe MonGroupe` : %u fois ; `sousgraphe ` : %u fois\n", nDef, nRef);
    CHECK(nDef == 1, "C1 : la definition du groupe est ecrite UNE fois");
    CHECK(nRef == 2, "C2 : chaque instance ne porte qu'une REFERENCE");
    {
        NkGraphDocument r;
        const bool ok = r.Deserialize(ser.CStr());
        NkString s2; r.Serialize(s2);
        CHECK(ok && strcmp(ser.CStr(), s2.CStr()) == 0, "C3 : aller-retour du document identique");
        if (!ok || strcmp(ser.CStr(), s2.CStr()) != 0) printf("----\n%s----\n%s----\n", ser.CStr(), s2.CStr());
    }

    // --- le PLAN APLATI : les frontieres disparaissent-elles ?
    printf("\n[D] plan aplati\n");
    {
        NkEvalPlan plan;
        const NkPlanError pe = doc.BuildPlan(plan);
        printf("  BuildPlan -> %s ; etapes=%u\n", NkPlanErrorName(pe), plan.Size());
        CHECK(pe == NkPlanError::Ok, "D1 : le plan se construit");
        bool frontiere = false;
        for (uint32 i = 0; i < plan.Size(); ++i) {
            const NkEvalStep &st = plan.steps[i];
            const NkNode *n = doc.GraphAt(st.graph).Find(st.node);
            const char *ty = n ? n->type.CStr() : "?";
            printf("    etape %u : graphe=%u type=%-14s profondeur=%u chemin=%s\n",
                   i, st.graph, ty, st.depth, st.path.CStr());
            if (n && (strcmp(ty, NK_NODE_INSTANCE) == 0 || strcmp(ty, NK_NODE_GROUP_IN) == 0
                   || strcmp(ty, NK_NODE_GROUP_OUT) == 0)) frontiere = true;
        }
        CHECK(!frontiere, "D2 : instances et frontieres ONT DISPARU du plan");
        // 1 source + 2 x (1 multiplication) = 3 etapes utiles
        CHECK(plan.Size() == 3, "D3 : 3 etapes — le corps du groupe est instancie DEUX fois");
    }

    // ---------------------------------------------------------------- (e) ---
    printf("\n[E] on MODIFIE la definition APRES instanciation (ajout d'une entree)\n");
    {
        NkNodeGraph &G = doc.GraphAt(gGroupe);
        NkNodeId in = NK_NODE_INVALID;
        for (uint32 i = 0; i < G.RawNodeCount(); ++i) {
            const NkNode *n = G.RawNodeAt(i);
            if (n && n->alive && strcmp(n->type.CStr(), NK_NODE_GROUP_IN) == 0) in = n->id;
        }
        G.AddSocket(in, "V2", tf, NkSocketDir::Output); // interface elargie, instances PAS mises a jour
        NkEvalPlan plan;
        const NkPlanError pe = doc.BuildPlan(plan);
        printf("  BuildPlan -> %s ; detail = %s\n", NkPlanErrorName(pe), plan.errorDetail.CStr());
        CHECK(pe == NkPlanError::InterfaceMismatch,
              "E1 : la desynchronisation definition/instance est DETECTEE, pas silencieuse");
        CHECK(plan.errorDetail.CStr()[0] != '\0', "E2 : et elle DIT lequel");
    }

    // ---------------------------------------------------------------- (f) ---
    printf("\n[F] un groupe qui s'instancie lui-meme\n");
    {
        NkGraphDocument d2;
        const uint32 r0 = d2.AddGraph("racine");
        d2.SetRoot(r0);
        NkNodeGraph &G = d2.GraphAt(r0);
        NkNodeId inst = G.AddNode(NK_NODE_INSTANCE, "Moi-meme");
        G.Find(inst)->subgraph = NkString("racine");
        NkEvalPlan plan;
        const NkPlanError pe = d2.BuildPlan(plan);
        printf("  BuildPlan -> %s\n", NkPlanErrorName(pe));
        CHECK(pe == NkPlanError::RecursiveSubgraph || pe == NkPlanError::TooDeep,
              "F1 : la recursion est refusee, pas explosee");
    }

    // ---------------------------------------------------------------- (g) ---
    printf("\n[G] instance qui nomme un groupe ABSENT\n");
    {
        NkGraphDocument d3;
        const uint32 r0 = d3.AddGraph("racine");
        d3.SetRoot(r0);
        NkNodeGraph &G = d3.GraphAt(r0);
        NkNodeId inst = G.AddNode(NK_NODE_INSTANCE, "Fantome");
        G.Find(inst)->subgraph = NkString("GroupeQuiNexistePas");
        NkEvalPlan plan;
        const NkPlanError pe = d3.BuildPlan(plan);
        printf("  BuildPlan -> %s ; detail = %s\n", NkPlanErrorName(pe), plan.errorDetail.CStr());
        CHECK(pe == NkPlanError::UnknownSubgraph, "G1 : un groupe absent est nomme, pas ignore");
    }

    printf("\n=== BILAN n2 : %d echec(s) ===\n", gFail);
    return gFail == 0 ? 0 : 1;
}
