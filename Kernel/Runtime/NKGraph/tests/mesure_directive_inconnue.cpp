// Banc de mesure : que fait Deserialize d'une directive INCONNUE ?
// Trois cas possibles : ignoree / refusee / silencieusement corrompue.
#include "NKGraph/NkGraphDocument.h"
#include <stdio.h>
#include <string.h>

using namespace nkentseu;
using namespace nkentseu::graph;

static int gFail = 0;
#define CHECK(cond, msg) do { if(!(cond)) { printf("  [ECHEC] %s\n", msg); ++gFail; } else { printf("  [ok]    %s\n", msg); } } while(0)

// Insere une ligne juste apres la premiere ligne dont le mot-cle est `apres`.
static NkString InsererApres(const NkString& src, const char* apres, const char* ligne) {
    NkString out;
    const char* p = src.CStr();
    size_t la = strlen(apres);
    bool done = false;
    while (*p) {
        const char* fin = p; while (*fin && *fin != '\n') ++fin;
        for (const char* q = p; q < fin; ++q) out.Append(*q);
        out.Append('\n');
        bool match = !done && (size_t)(fin - p) >= la && strncmp(p, apres, la) == 0
                     && (p[la] == ' ' || p + la == fin);
        if (match) { out.Append(ligne); out.Append('\n'); done = true; }
        p = (*fin == '\n') ? fin + 1 : fin;
    }
    return out;
}

static void Dump(const char* titre, const NkString& s) {
    printf("---- %s ----\n%s----\n", titre, s.CStr());
}

int main() {
    printf("=== MESURE : directive inconnue dans .nkgraph ===\n\n");

    // --- 1. Graphe temoin non trivial ---------------------------------------
    NkNodeGraph g;
    const NkTypeId tFloat = g.RegisterType("float");
    const NkTypeId tColor = g.RegisterType("color");
    g.AllowConversion(tFloat, tColor);

    NkNodeId a = g.AddNode("math.add", "Addition");
    NkNodeId b = g.AddNode("out.color", "Sortie");
    g.Find(a)->x = 10.0f;  g.Find(a)->y = 20.0f;
    g.Find(b)->x = 300.0f; g.Find(b)->y = 40.0f;
    g.AddSocket(a, "A",  tFloat, NkSocketDir::Input);
    g.AddSocket(a, "B",  tFloat, NkSocketDir::Input);
    g.AddSocket(a, "R",  tFloat, NkSocketDir::Output);
    g.AddSocket(b, "In", tColor, NkSocketDir::Input);
    NkLinkId lid = 0;
    const NkLinkError le = g.Connect(a, "R", b, "In", &lid);
    printf("temoin : Connect -> %s ; lien_id=%u ; noeuds=%u liens=%u\n",
           NkLinkErrorName(le), (uint32)lid, g.NodeCount(), g.LinkCount());
    if (le != NkLinkError::Ok) { printf("ARRET : le temoin ne se construit pas\n"); return 2; }

    NkString ref; g.Serialize(ref);
    Dump("SERIALISE DE REFERENCE", ref);

    // --- 2. Aller-retour SANS directive inconnue ----------------------------
    {
        NkNodeGraph r;
        const bool ok = r.Deserialize(ref.CStr());
        NkString again; r.Serialize(again);
        CHECK(ok, "temoin : Deserialize rend true");
        CHECK(strcmp(again.CStr(), ref.CStr()) == 0, "temoin : aller-retour identique octet pour octet");
        if (strcmp(again.CStr(), ref.CStr()) != 0) Dump("temoin relu", again);
    }

    // --- 3. CAS A : directive inconnue au milieu ----------------------------
    printf("\n[CAS A] directive inconnue inseree apres la premiere ligne `noeud`\n");
    {
        NkString mod = InsererApres(ref, "noeud", "cadre 7 0 0 400 300 Mon Cadre");
        NkNodeGraph r;
        const bool ok = r.Deserialize(mod.CStr());
        NkString again; r.Serialize(again);
        printf("  Deserialize -> %s\n", ok ? "true" : "false");
        CHECK(ok, "CAS A : Deserialize rend true (non refusee)");
        CHECK(strcmp(again.CStr(), ref.CStr()) == 0, "CAS A : graphe relu IDENTIQUE au temoin (ignoree, non corrompue)");
        if (strcmp(again.CStr(), ref.CStr()) != 0) Dump("CAS A relu", again);
    }

    // --- 4. CAS B : directive inconnue juste apres l'entete -----------------
    printf("\n[CAS B] directive inconnue juste apres l'entete `nkgraph`\n");
    {
        NkString mod = InsererApres(ref, "nkgraph", "commentaire 1 50 50 Ceci est un commentaire libre");
        NkNodeGraph r;
        const bool ok = r.Deserialize(mod.CStr());
        NkString again; r.Serialize(again);
        printf("  Deserialize -> %s\n", ok ? "true" : "false");
        CHECK(ok, "CAS B : Deserialize rend true");
        CHECK(strcmp(again.CStr(), ref.CStr()) == 0, "CAS B : graphe identique");
        if (strcmp(again.CStr(), ref.CStr()) != 0) Dump("CAS B relu", again);
    }

    // --- 5. CAS C : directive inconnue AVANT l'entete -----------------------
    printf("\n[CAS C] directive inconnue AVANT la ligne `nkgraph`\n");
    {
        NkString mod("cadre 7 0 0 400 300 Mon Cadre\n");
        mod.Append(ref);
        NkNodeGraph r;
        const bool ok = r.Deserialize(mod.CStr());
        printf("  Deserialize -> %s\n", ok ? "true" : "false");
        CHECK(!ok, "CAS C : refusee (l entete doit rester la premiere ligne)");
    }

    // --- 6. CAS D : mot-cle inconnu prefixe d un mot-cle connu --------------
    printf("\n[CAS D] mot-cle `noeudcadre` (prefixe du mot-cle connu `noeud`)\n");
    {
        NkString mod = InsererApres(ref, "noeud", "noeudcadre 7 0 0 400 300 X");
        NkNodeGraph r;
        const bool ok = r.Deserialize(mod.CStr());
        NkString again; r.Serialize(again);
        printf("  Deserialize -> %s\n", ok ? "true" : "false");
        CHECK(ok && strcmp(again.CStr(), ref.CStr()) == 0, "CAS D : prefixe non confondu avec `noeud`");
        if (strcmp(again.CStr(), ref.CStr()) != 0) Dump("CAS D relu", again);
    }

    // --- 7. CAS E : ligne vide ---------------------------------------------
    printf("\n[CAS E] ligne vide inseree\n");
    {
        NkString mod = InsererApres(ref, "noeud", "");
        NkNodeGraph r;
        const bool ok = r.Deserialize(mod.CStr());
        NkString again; r.Serialize(again);
        printf("  Deserialize -> %s\n", ok ? "true" : "false");
        CHECK(ok && strcmp(again.CStr(), ref.CStr()) == 0, "CAS E : ligne vide sans effet");
        if (strcmp(again.CStr(), ref.CStr()) != 0) Dump("CAS E relu", again);
    }

    // --- 8. CAS F : survie a annuler/refaire --------------------------------
    printf("\n[CAS F] survie de la directive inconnue a un annuler/refaire\n");
    {
        NkGraphHistory h;
        NkNodeGraph r;
        NkString mod = InsererApres(ref, "noeud", "cadre 7 0 0 400 300 Mon Cadre");
        r.Deserialize(mod.CStr());
        h.Reset(r);
        r.AddNode("x.y", "Ajout");
        h.Commit(r);
        const bool ok = h.Undo(r);
        NkString apres; r.Serialize(apres);
        const bool present = strstr(apres.CStr(), "cadre ") != NULL;
        printf("  Undo -> %s ; directive `cadre` encore presente ? %s\n",
               ok ? "true" : "false", present ? "OUI" : "NON");
        CHECK(!present, "CAS F : la directive inconnue est PERDUE des le premier annuler");
    }

    // --- 9. CAS G : au niveau DOCUMENT --------------------------------------
    printf("\n[CAS G] directive inconnue au niveau document\n");
    {
        NkGraphDocument doc;
        const uint32 gi = doc.AddGraph("principal");
        doc.GraphAt(gi).RegisterType("float");
        doc.GraphAt(gi).AddNode("math.add", "Add");
        NkString dref; doc.Serialize(dref);
        Dump("DOC REFERENCE", dref);

        NkString m1 = InsererApres(dref, "racine", "cadres 3");
        NkGraphDocument d1;
        const bool ok1 = d1.Deserialize(m1.CStr());
        NkString s1; d1.Serialize(s1);
        printf("  [niveau doc] Deserialize -> %s\n", ok1 ? "true" : "false");
        CHECK(ok1 && strcmp(s1.CStr(), dref.CStr()) == 0, "CAS G1 : inconnue au niveau document ignoree");
        if (strcmp(s1.CStr(), dref.CStr()) != 0) Dump("G1 relu", s1);

        NkString m2 = InsererApres(dref, "noeud", "cadre 7 0 0 400 300 Mon Cadre");
        NkGraphDocument d2;
        const bool ok2 = d2.Deserialize(m2.CStr());
        NkString s2; d2.Serialize(s2);
        printf("  [corps]      Deserialize -> %s\n", ok2 ? "true" : "false");
        CHECK(ok2 && strcmp(s2.CStr(), dref.CStr()) == 0, "CAS G2 : inconnue dans le corps ignoree");
        if (strcmp(s2.CStr(), dref.CStr()) != 0) Dump("G2 relu", s2);

        // CAS G3 : une directive inconnue qui commence par `graphe...`
        NkString m3 = InsererApres(dref, "racine", "graphecadre 3");
        NkGraphDocument d3;
        const bool ok3 = d3.Deserialize(m3.CStr());
        NkString s3; d3.Serialize(s3);
        printf("  [prefixe graphe] Deserialize -> %s ; graphes=%u (attendu %u)\n",
               ok3 ? "true" : "false", (uint32)d3.GraphCount(), (uint32)doc.GraphCount());
        CHECK(ok3 && strcmp(s3.CStr(), dref.CStr()) == 0, "CAS G3 : `graphecadre` non confondu avec `graphe`");
        if (strcmp(s3.CStr(), dref.CStr()) != 0) Dump("G3 relu", s3);
    }

    printf("\n=== BILAN : %d echec(s) ===\n", gFail);
    return gFail == 0 ? 0 : 1;
}
