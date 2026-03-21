/**
 * @File    NkOutline.cpp
 * @Brief   Implémentation NkOutline — flattening et construction des arêtes.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#include "pch.h"
#include "NKFont/NkOutline.h"
#include <cstring>

namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  NkEdgeList::SortByY — tri insertion (liste courte, < 4096 arêtes)
// ─────────────────────────────────────────────────────────────────────────────

void NkEdgeList::SortByY() noexcept {
    for (uint32 i = 1; i < numEdges; ++i) {
        NkEdge key = edges[i];
        const int32 keyY = key.y0 < key.y1 ? key.y0 : key.y1;
        int32 j = static_cast<int32>(i) - 1;
        while (j >= 0) {
            const int32 cmpY = edges[j].y0 < edges[j].y1 ? edges[j].y0 : edges[j].y1;
            if (cmpY <= keyY) break;
            edges[j+1] = edges[j];
            --j;
        }
        edges[j+1] = key;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkOutline::AddEdge — ajoute une arête non-horizontale
// ─────────────────────────────────────────────────────────────────────────────

void NkOutline::AddEdge(NkEdgeList& edges, NkPoint26_6 p0, NkPoint26_6 p1) noexcept {
    // Conversion F26Dot6 → pixels (round)
    const int32 x0 = p0.x.Round(), y0 = p0.y.Round();
    const int32 x1 = p1.x.Round(), y1 = p1.y.Round();
    if (y0 == y1) return; // arête horizontale — ignorée

    NkEdge e;
    if (y0 < y1) {
        e.x0 = x0; e.y0 = y0; e.x1 = x1; e.y1 = y1;
        e.winding = +1;
    } else {
        e.x0 = x1; e.y0 = y1; e.x1 = x0; e.y1 = y0;
        e.winding = -1;
    }
    e.dx = e.x1 - e.x0;
    e.dy = e.y1 - e.y0;

    // Calcule xStep = dx/dy en F26Dot6 pour l'interpolation scanline
    if (e.dy != 0)
        e.xStep = F26Dot6::FromRaw((e.dx * F26Dot6::ONE) / e.dy);
    else
        e.xStep = F26Dot6::Zero();

    // Position X au début de la première scanline
    e.xCurrent = F26Dot6::FromInt(e.x0);

    edges.Add(e);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Callback pour NkBezier::Flatten
// ─────────────────────────────────────────────────────────────────────────────

void NkOutline::OnSegment(NkPoint26_6 p0, NkPoint26_6 p1, void* ud) noexcept {
    FlattenCtx* ctx = static_cast<FlattenCtx*>(ud);
    AddEdge(*ctx->edges, p0, p1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkOutline::FlattenContour
//  Reconstruit les courbes TrueType (points implicites) et génère les arêtes.
// ─────────────────────────────────────────────────────────────────────────────

void NkOutline::FlattenContour(
    uint16 start, uint16 end,
    NkEdgeList& edges, int32 tolerance) const noexcept
{
    const uint16 count = end - start + 1;
    if (count < 2) return;

    FlattenCtx ctx{&edges, tolerance};

    // Cherche le premier point on-curve pour initialiser la position courante
    uint16 firstOnCurve = start;
    for (uint16 i = start; i <= end; ++i) {
        if (tags[i] & NK_TAG_ON_CURVE) { firstOnCurve = i; break; }
    }

    NkPoint26_6 contourStart = points[firstOnCurve];
    NkPoint26_6 cur          = contourStart;
    uint16       i            = firstOnCurve;

    auto next = [&](uint16 idx) -> uint16 {
        return (idx >= end) ? start : idx + 1;
    };

    uint16 processed = 0;
    while (processed < count) {
        const uint16 ni = next(i);
        const uint8  tagCur  = tags[i];
        const uint8  tagNext = tags[ni];
        (void)tagCur;

        if (tags[ni] & NK_TAG_ON_CURVE) {
            // Ligne droite
            AddEdge(edges, cur, points[ni]);
            cur = points[ni];
            i = ni;
            ++processed;
        } else if (!(tags[ni] & NK_TAG_CUBIC)) {
            // Quadratique conic (TrueType)
            // Cherche le point d'arrivée
            const uint16 nni = next(ni);
            NkPoint26_6 ctrl = points[ni];
            NkPoint26_6 end_pt;

            if (tags[nni] & NK_TAG_ON_CURVE) {
                end_pt = points[nni];
                NkBezierQuad q(cur, ctrl, end_pt);
                q.Flatten(OnSegment, &ctx, tolerance);
                cur = end_pt;
                i = nni;
                processed += 2;
            } else {
                // Deux points off-curve consécutifs → on-curve implicite au milieu
                end_pt = ctrl.MidPoint(points[nni]);
                NkBezierQuad q(cur, ctrl, end_pt);
                q.Flatten(OnSegment, &ctx, tolerance);
                cur = end_pt;
                i = ni;
                ++processed;
            }
        } else {
            // Cubique (CFF outline dans glyf — rare mais possible)
            const uint16 ni2 = next(ni);
            const uint16 ni3 = next(ni2);
            if (ni3 <= end && (tags[ni3] & NK_TAG_ON_CURVE)) {
                NkBezierCubic c(cur, points[ni], points[ni2], points[ni3]);
                c.Flatten(OnSegment, &ctx, tolerance);
                cur = points[ni3];
                i = ni3;
                processed += 3;
            } else {
                // Fallback : ligne droite
                AddEdge(edges, cur, points[ni]);
                cur = points[ni];
                i = ni;
                ++processed;
            }
        }
    }

    // Ferme le contour
    if (!(cur == contourStart))
        AddEdge(edges, cur, contourStart);
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkOutline::Flatten
// ─────────────────────────────────────────────────────────────────────────────

bool NkOutline::Flatten(
    NkEdgeList& edges, NkMemArena& arena, int32 tolerance) const noexcept
{
    if (numPoints == 0 || numContours == 0) return true;

    // Capacité maximale : chaque segment produit au plus 1 arête,
    // chaque courbe au plus ~32 arêtes (profondeur 5 × 2 = 32).
    const uint32 maxEdges = static_cast<uint32>(numPoints) * 32u;
    edges.Init(arena, maxEdges);
    if (!edges.edges) return false;

    uint16 start = 0;
    for (uint16 c = 0; c < numContours; ++c) {
        const uint16 end = contourEnds[c];
        if (end >= numPoints) return false;
        FlattenContour(start, end, edges, tolerance);
        start = end + 1;
    }

    edges.SortByY();
    return true;
}

} // namespace nkentseu
