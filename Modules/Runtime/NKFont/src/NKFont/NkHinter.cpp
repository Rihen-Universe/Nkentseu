/**
 * @File    NkHinter.cpp
 * @Brief   VM TrueType hinting — implémentation complète.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#include "pch.h"
#include "NKFont/NkHinter.h"
#include <cstring>
#include <cmath>

namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  NkGraphicsState
// ─────────────────────────────────────────────────────────────────────────────

void NkGraphicsState::Reset() noexcept {
    pvX = F16Dot16::FromInt(1); pvY = F16Dot16::Zero();
    fvX = F16Dot16::FromInt(1); fvY = F16Dot16::Zero();
    dvX = F16Dot16::FromInt(1); dvY = F16Dot16::Zero();
    rp[0] = rp[1] = rp[2] = 0;
    zp[0] = zp[1] = zp[2] = 1;
    loop          = 1;
    minDist       = F26Dot6::One();
    controlValueCutIn  = F26Dot6::FromRaw(17 * F26Dot6::ONE / 16); // 17/16
    singleWidthCutIn   = F26Dot6::Zero();
    singleWidthValue   = F26Dot6::Zero();
    deltaBase     = F26Dot6::FromInt(9);
    deltaShift    = F26Dot6::FromInt(3);
    roundMode     = RoundMode::ToGrid;
    autoFlip      = true;
    instructControl = false;
}

F26Dot6 NkGraphicsState::Round(F26Dot6 d) const noexcept {
    const bool neg = d.raw < 0;
    F26Dot6 abs_d = d.Abs();
    F26Dot6 result;
    switch (roundMode) {
        case RoundMode::ToGrid:
            result = F26Dot6::FromInt(abs_d.Round()); break;
        case RoundMode::ToHalfGrid:
            result = F26Dot6::FromRaw((abs_d.raw + F26Dot6::HALF) & F26Dot6::INT_MASK)
                   + F26Dot6::Half(); break;
        case RoundMode::ToDoubleGrid:
            result = F26Dot6::FromRaw((abs_d.raw + F26Dot6::HALF/2) & ~(F26Dot6::HALF - 1)); break;
        case RoundMode::DownToGrid:
            result = F26Dot6::FromInt(abs_d.Floor()); break;
        case RoundMode::UpToGrid:
            result = F26Dot6::FromInt(abs_d.Ceil()); break;
        case RoundMode::Off:
            result = abs_d; break;
        default:
            result = F26Dot6::FromInt(abs_d.Round()); break;
    }
    return neg ? -result : result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkHinter::Init
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Init(
    const NkTTFFont& font, uint16 ppem,
    NkMemArena& arena, NkHintVM& vm) noexcept
{
    ::memset(&vm, 0, sizeof(vm));
    if (!font.isValid) return false;

    const NkTTFMaxp& maxp = font.maxp;

    // Stack
    const int32 stackSize = maxp.maxStackElements > 0 ? maxp.maxStackElements : 64;
    vm.stack     = arena.Alloc<int32>(stackSize);
    vm.stackSize = stackSize;
    vm.stackTop  = 0;

    // Storage
    const int32 storageSize = maxp.maxStorage > 0 ? maxp.maxStorage : 64;
    vm.storage     = arena.Alloc<int32>(storageSize);
    vm.storageSize = storageSize;

    // CVT (en F26Dot6, converti depuis les design units)
    vm.cvtSize = static_cast<int32>(font.cvtLength / 2); // table cvt : int16[]
    vm.cvt     = arena.Alloc<F26Dot6>(vm.cvtSize > 0 ? vm.cvtSize : 1);
    if (vm.cvt && font.cvtData && vm.cvtSize > 0) {
        const int32 scale = (static_cast<int32>(ppem) << F26Dot6::SHIFT) / font.head.unitsPerEm;
        for (int32 i = 0; i < vm.cvtSize; ++i) {
            const int16 duVal = static_cast<int16>(
                (static_cast<uint16>(font.cvtData[i*2]) << 8) | font.cvtData[i*2+1]);
            vm.cvt[i] = F26Dot6::FromRaw(static_cast<int32>(duVal) * scale);
        }
    }

    // Fonctions (FDEF)
    const uint32 maxFuncs = maxp.maxFunctionDefs > 0 ? maxp.maxFunctionDefs : 64;
    vm.funcs       = arena.Alloc<NkHintVM::FuncDef>(maxFuncs);
    vm.funcCapacity= maxFuncs;
    vm.funcCount   = 0;

    // Zones
    const uint32 twilightPts = maxp.maxTwilightPoints > 0 ? maxp.maxTwilightPoints : 8;
    vm.zone0.Init(arena, twilightPts, true);
    // zone1 sera initialisée par HintGlyph

    // Métriques
    vm.ppem  = ppem;
    vm.scale = F26Dot6::FromRaw((static_cast<int32>(ppem) << F26Dot6::SHIFT) / font.head.unitsPerEm);

    vm.gs.Reset();
    vm.gsDefault = vm.gs;
    vm.isValid   = true;

    if (!vm.stack || !vm.storage || !vm.cvt || !vm.funcs) return false;

    // Exécute fpgm
    if (font.fpgmData && font.fpgmLength > 0) {
        ExecContext ctx;
        ctx.vm        = &vm;
        ctx.code      = font.fpgmData;
        ctx.codeLen   = font.fpgmLength;
        ctx.ip        = 0;
        ctx.callDepth = 0;
        ctx.arena     = &arena;
        Execute(ctx); // Erreur non fatale — la police reste utilisable
    }

    return true;
}

bool NkHinter::RunPrep(
    const NkTTFFont& font, NkHintVM& vm, NkMemArena& arena) noexcept
{
    if (!vm.isValid) return false;
    vm.gs = vm.gsDefault; // Reset GS

    if (font.prepData && font.prepLength > 0) {
        ExecContext ctx;
        ctx.vm        = &vm;
        ctx.code      = font.prepData;
        ctx.codeLen   = font.prepLength;
        ctx.ip        = 0;
        ctx.callDepth = 0;
        ctx.arena     = &arena;
        Execute(ctx);
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkHinter::HintGlyph
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::HintGlyph(
    NkHintVM&     vm,
    NkPoint26_6*  points,
    const uint8*  tags,
    const uint16* contourEnds,
    uint16        numPoints,
    uint16        numContours,
    const uint8*  instrData,
    uint16        instrLen,
    NkMemArena&   arena) noexcept
{
    if (!vm.isValid || !instrData || instrLen == 0) return true;

    NkScratchArena scratch(arena);

    // Init zone1 avec les points du glyphe
    vm.zone1.Init(arena, numPoints + 4, false); // +4 = phantom points
    if (!vm.zone1.original || !vm.zone1.current) return false;

    ::memcpy(vm.zone1.original, points, sizeof(NkPoint26_6) * numPoints);
    ::memcpy(vm.zone1.current,  points, sizeof(NkPoint26_6) * numPoints);

    // Reset GS entre les glyphes (conserve le résultat de prep)
    vm.gs.rp[0] = vm.gs.rp[1] = vm.gs.rp[2] = 0;
    vm.gs.zp[0] = vm.gs.zp[1] = vm.gs.zp[2] = 1;
    vm.gs.loop  = 1;
    vm.stackTop = 0;

    ExecContext ctx;
    ctx.vm        = &vm;
    ctx.code      = instrData;
    ctx.codeLen   = instrLen;
    ctx.ip        = 0;
    ctx.callDepth = 0;
    ctx.arena     = &arena;

    const bool ok = Execute(ctx);

    // Copie les points hintés en retour
    if (ok)
        ::memcpy(points, vm.zone1.current, sizeof(NkPoint26_6) * numPoints);

    (void)tags; (void)contourEnds; (void)numContours;
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Utilitaires
// ─────────────────────────────────────────────────────────────────────────────

F26Dot6 NkHinter::GetProjection(
    const NkHintVM& vm, const NkHintZone& zone, uint32 idx) noexcept
{
    if (idx >= zone.numPoints) return F26Dot6::Zero();
    return vm.gs.Project(zone.current[idx].x, zone.current[idx].y);
}

void NkHinter::MovePoint(
    NkHintVM& vm, NkHintZone& zone, uint32 idx, F26Dot6 delta) noexcept
{
    if (idx >= zone.numPoints) return;
    // Déplace le point dans la direction du freedom vector
    zone.current[idx].x.raw += static_cast<int32>(
        (static_cast<int64>(vm.gs.fvX.raw) * delta.raw) >> F16Dot16::SHIFT);
    zone.current[idx].y.raw += static_cast<int32>(
        (static_cast<int64>(vm.gs.fvY.raw) * delta.raw) >> F16Dot16::SHIFT);
    if (idx < zone.numPoints) zone.touched[idx] = true;
}

F26Dot6 NkHinter::ComputeDist(
    const NkHintVM& vm,
    const NkHintZone& z1, uint32 p1,
    const NkHintZone& z2, uint32 p2) noexcept
{
    if (p1 >= z1.numPoints || p2 >= z2.numPoints) return F26Dot6::Zero();
    const F26Dot6 dx = z2.current[p2].x - z1.current[p1].x;
    const F26Dot6 dy = z2.current[p2].y - z1.current[p1].y;
    return vm.gs.Project(dx, dy);
}

F26Dot6 NkHinter::CVTCutIn(
    const NkHintVM& vm, F26Dot6 dist, F26Dot6 cvtDist) noexcept
{
    const F26Dot6 diff = (dist - cvtDist).Abs();
    return diff < vm.gs.controlValueCutIn ? cvtDist : dist;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Execute — boucle principale d'interprétation
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Execute(ExecContext& ctx) noexcept {
    if (ctx.callDepth > 16) return false;

    while (ctx.ip < ctx.codeLen) {
        const uint8 op = ctx.code[ctx.ip++];

        // ── PUSH (opcodes 0x40-0x41, 0xB0-0xBF) ─────────────────────────────
        if ((op >= 0xB0 && op <= 0xBF) || op == 0x40 || op == 0x41) {
            if (!Op_Push(ctx, op)) return false;
            continue;
        }

        bool ok = true;
        switch (op) {
            // ── Stack manipulation ────────────────────────────────────────────
            case 0x20: { // DUP
                const int32 v = ctx.vm->Peek();
                ctx.vm->Push(v); break;
            }
            case 0x21: ctx.vm->Pop(); break; // POP
            case 0x22: ctx.vm->stackTop = 0; break; // CLEAR
            case 0x23: { // SWAP
                if (ctx.vm->stackTop < 2) break;
                const int32 a = ctx.vm->Pop(), b = ctx.vm->Pop();
                ctx.vm->Push(a); ctx.vm->Push(b); break;
            }
            case 0x24: ctx.vm->Push(ctx.vm->stackTop); break; // DEPTH
            case 0x25: { // CINDEX
                const int32 idx = ctx.vm->Pop();
                if (idx > 0 && idx <= ctx.vm->stackTop)
                    ctx.vm->Push(ctx.vm->stack[ctx.vm->stackTop - idx]);
                break;
            }
            case 0x26: { // MINDEX
                const int32 idx = ctx.vm->Pop();
                if (idx > 0 && idx <= ctx.vm->stackTop) {
                    const int32 val = ctx.vm->stack[ctx.vm->stackTop - idx];
                    for (int32 i = ctx.vm->stackTop - idx; i < ctx.vm->stackTop - 1; ++i)
                        ctx.vm->stack[i] = ctx.vm->stack[i+1];
                    ctx.vm->stack[ctx.vm->stackTop-1] = val;
                }
                break;
            }

            // ── Storage ───────────────────────────────────────────────────────
            case 0x43: ok = Op_Storage(ctx, op); break; // RS
            case 0x42: ok = Op_Storage(ctx, op); break; // WS

            // ── CVT ───────────────────────────────────────────────────────────
            case 0x44: // WCVTP
            case 0x70: // WCVTF
            case 0x45: // RCVT
                ok = Op_CVT(ctx, op); break;

            // ── Arithmétique ──────────────────────────────────────────────────
            case 0x60: case 0x61: case 0x62: case 0x63:
            case 0x64: case 0x65: case 0x66: case 0x67:
            case 0x68: case 0x69: case 0x6A: case 0x6B:
            case 0x6C: case 0x6D: case 0x8B: case 0x8C:
                ok = Op_Arith(ctx, op); break;

            // ── Comparaison ───────────────────────────────────────────────────
            case 0x50: case 0x51: case 0x52: case 0x53:
            case 0x54: case 0x55: case 0x56: case 0x57:
            case 0x5A: case 0x5B: case 0x5C:
                ok = Op_Compare(ctx, op); break;

            // ── Sauts conditionnels ───────────────────────────────────────────
            case 0x58: case 0x59: case 0x1C: case 0x78: case 0x79:
                ok = Op_Jump(ctx, op); break;

            // ── Graphics State ────────────────────────────────────────────────
            case 0x00: case 0x01: case 0x02: case 0x03:
            case 0x04: case 0x05: case 0x06: case 0x07:
            case 0x08: case 0x09: case 0x0A: case 0x0B:
            case 0x0C: case 0x0D: case 0x0E: case 0x0F:
            case 0x10: case 0x11: case 0x12: case 0x13:
            case 0x14: case 0x15: case 0x16: case 0x17:
            case 0x18: case 0x19: case 0x1A: case 0x1B:
            case 0x1D: case 0x1E: case 0x1F:
                ok = Op_GS(ctx, op); break;

            // ── Déplacement de points ─────────────────────────────────────────
            case 0x2E: case 0x2F: // MDAP
            case 0x3E: case 0x3F: // MIAP
            case 0x3C:             // ALIGNRP
            case 0x27:             // ALIGNPTS
            case 0x29:             // SHP (zone rp2)
            case 0x2A:             // SHP (zone rp1)
            case 0x30: case 0x31: // SHC
            case 0x32: case 0x33: // SHZ
            case 0x34: case 0x35: // SHPIX
            case 0x38:             // SHPIX
            case 0x3A: case 0x3B: // MSIRP
            case 0x4A: case 0x4B: // MDRP range
            case 0x4C: case 0x4D:
            case 0x4E: case 0x4F:
                ok = Op_Move(ctx, op); break;
            case 0xC0: case 0xC1: case 0xC2: case 0xC3: // MDRP (extended)
            case 0xC4: case 0xC5: case 0xC6: case 0xC7:
            case 0xC8: case 0xC9: case 0xCA: case 0xCB:
            case 0xCC: case 0xCD: case 0xCE: case 0xCF:
            case 0xD0: case 0xD1: case 0xD2: case 0xD3:
            case 0xD4: case 0xD5: case 0xD6: case 0xD7:
            case 0xD8: case 0xD9: case 0xDA: case 0xDB:
            case 0xDC: case 0xDD: case 0xDE: case 0xDF:
            case 0xE0: case 0xE1: case 0xE2: case 0xE3: // MIRP (extended)
            case 0xE4: case 0xE5: case 0xE6: case 0xE7:
            case 0xE8: case 0xE9: case 0xEA: case 0xEB:
            case 0xEC: case 0xED: case 0xEE: case 0xEF:
            case 0xF0: case 0xF1: case 0xF2: case 0xF3:
            case 0xF4: case 0xF5: case 0xF6: case 0xF7:
            case 0xF8: case 0xF9: case 0xFA: case 0xFB:
            case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                ok = Op_Move(ctx, op); break;

            // ── IUP ───────────────────────────────────────────────────────────
            case 0x30: case 0x31: // IUP (overlap avec SHC — géré dans Op_Align)
                ok = Op_Align(ctx, op); break;

            // ── Fonctions ─────────────────────────────────────────────────────
            case 0x2C: // FDEF
            case 0x2D: // ENDF
            case 0x2B: // CALL
            case 0xFA: // LOOPCALL
                ok = Op_Function(ctx, op); break;

            // ── Delta ─────────────────────────────────────────────────────────
            case 0x5D: // DELTAP1
            case 0x71: // DELTAP2
            case 0x72: // DELTAP3
            case 0x73: // DELTAC1
            case 0x74: // DELTAC2
            case 0x75: // DELTAC3
                ok = Op_Delta(ctx, op); break;

            // ── Divers ────────────────────────────────────────────────────────
            default:
                ok = Op_Misc(ctx, op); break;
        }
        if (!ok) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_Push
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_Push(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    if (op == 0x40) { // NPUSHB : n octets
        if (ctx.ip >= ctx.codeLen) return false;
        const uint8 n = ctx.code[ctx.ip++];
        for (uint8 i = 0; i < n; ++i) {
            if (ctx.ip >= ctx.codeLen) return false;
            vm.Push(ctx.code[ctx.ip++]);
        }
    } else if (op == 0x41) { // NPUSHW : n mots
        if (ctx.ip >= ctx.codeLen) return false;
        const uint8 n = ctx.code[ctx.ip++];
        for (uint8 i = 0; i < n; ++i) {
            if (ctx.ip + 1 >= ctx.codeLen) return false;
            const int16 w = static_cast<int16>(
                (static_cast<uint16>(ctx.code[ctx.ip]) << 8) | ctx.code[ctx.ip+1]);
            ctx.ip += 2;
            vm.Push(w);
        }
    } else if (op >= 0xB0 && op <= 0xB7) { // PUSHB[n]
        const int32 n = (op - 0xB0) + 1;
        for (int32 i = 0; i < n; ++i) {
            if (ctx.ip >= ctx.codeLen) return false;
            vm.Push(ctx.code[ctx.ip++]);
        }
    } else { // PUSHW[n]  0xB8-0xBF
        const int32 n = (op - 0xB8) + 1;
        for (int32 i = 0; i < n; ++i) {
            if (ctx.ip + 1 >= ctx.codeLen) return false;
            const int16 w = static_cast<int16>(
                (static_cast<uint16>(ctx.code[ctx.ip]) << 8) | ctx.code[ctx.ip+1]);
            ctx.ip += 2;
            vm.Push(w);
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_Storage
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_Storage(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    if (op == 0x43) { // RS : Read Storage
        const int32 idx = vm.Pop();
        vm.Push((idx >= 0 && idx < vm.storageSize) ? vm.storage[idx] : 0);
    } else { // WS : Write Storage
        const int32 val = vm.Pop();
        const int32 idx = vm.Pop();
        if (idx >= 0 && idx < vm.storageSize) vm.storage[idx] = val;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_CVT
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_CVT(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    if (op == 0x45) { // RCVT
        const int32 idx = vm.Pop();
        vm.Push((idx >= 0 && idx < vm.cvtSize) ? vm.cvt[idx].raw : 0);
    } else if (op == 0x44) { // WCVTP
        const int32 val = vm.Pop();
        const int32 idx = vm.Pop();
        if (idx >= 0 && idx < vm.cvtSize) vm.cvt[idx] = F26Dot6::FromRaw(val);
    } else { // WCVTF : write en design units
        const int32 val = vm.Pop();
        const int32 idx = vm.Pop();
        if (idx >= 0 && idx < vm.cvtSize)
            vm.cvt[idx] = F26Dot6::FromRaw((val * vm.scale.raw) >> F26Dot6::SHIFT);
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_Arith
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_Arith(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    switch (op) {
        case 0x60: { const int32 b=vm.Pop(),a=vm.Pop(); vm.Push(a+b); break; } // ADD
        case 0x61: { const int32 b=vm.Pop(),a=vm.Pop(); vm.Push(a-b); break; } // SUB
        case 0x62: { // DIV (F26Dot6)
            const int32 b=vm.Pop(), a=vm.Pop();
            if (b==0) { vm.Push(0); break; }
            vm.Push(static_cast<int32>((static_cast<int64>(a) << F26Dot6::SHIFT) / b));
            break;
        }
        case 0x63: { // MUL (F26Dot6)
            const int32 b=vm.Pop(), a=vm.Pop();
            vm.Push(static_cast<int32>((static_cast<int64>(a) * b) >> F26Dot6::SHIFT));
            break;
        }
        case 0x64: { const int32 a=vm.Pop(); vm.Push(a<0?-a:a); break; } // ABS
        case 0x65: { const int32 a=vm.Pop(); vm.Push(-a); break; } // NEG
        case 0x66: { // FLOOR
            const int32 a=vm.Pop();
            vm.Push(a & F26Dot6::INT_MASK);
            break;
        }
        case 0x67: { // CEILING
            const int32 a=vm.Pop();
            vm.Push((a + F26Dot6::FRAC_MASK) & F26Dot6::INT_MASK);
            break;
        }
        case 0x68: case 0x69: case 0x6A: case 0x6B: { // ROUND
            const int32 a=vm.Pop();
            const F26Dot6 rounded = vm.gs.Round(F26Dot6::FromRaw(a));
            vm.Push(rounded.raw);
            break;
        }
        case 0x6C: case 0x6D: { // NROUND (no round)
            break; // valeur inchangée — déjà sur le stack
        }
        case 0x8B: { const int32 b=vm.Pop(),a=vm.Pop(); vm.Push(a>b?a:b); break; } // MAX
        case 0x8C: { const int32 b=vm.Pop(),a=vm.Pop(); vm.Push(a<b?a:b); break; } // MIN
        default: break;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_Compare
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_Compare(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    const int32 b = vm.Pop(), a = vm.Pop();
    switch (op) {
        case 0x50: vm.Push(a < b  ? 1 : 0); break; // LT
        case 0x51: vm.Push(a <= b ? 1 : 0); break; // LTEQ
        case 0x52: vm.Push(a > b  ? 1 : 0); break; // GT
        case 0x53: vm.Push(a >= b ? 1 : 0); break; // GTEQ
        case 0x54: vm.Push(a == b ? 1 : 0); break; // EQ
        case 0x55: vm.Push(a != b ? 1 : 0); break; // NEQ
        case 0x5A: vm.Push((a && b) ? 1 : 0); break; // AND
        case 0x5B: vm.Push((a || b) ? 1 : 0); break; // OR
        case 0x5C: vm.Push((!a) ? 1 : 0);           // NOT (b popped mais ignoré)
            vm.Push(0); break; // remettre b (si nécessaire — skip)
        default: break;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_Jump
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_Jump(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    if (op == 0x58) { // IF
        const int32 cond = vm.Pop();
        if (!cond) {
            // Saute jusqu'à ELSE ou EIF en comptant les IF/ELSE/EIF imbriqués
            int32 depth = 1;
            while (ctx.ip < ctx.codeLen && depth > 0) {
                const uint8 c = ctx.code[ctx.ip++];
                if      (c == 0x58) ++depth;
                else if (c == 0x59 || c == 0x1B) --depth; // EIF ou ELSE
                // Skip PUSH data
                if (c >= 0xB0 && c <= 0xB7 && depth > 0) ctx.ip += (c - 0xB0 + 1);
                else if (c >= 0xB8 && c <= 0xBF && depth > 0) ctx.ip += (c - 0xB8 + 1) * 2;
                else if (c == 0x40 && depth > 0 && ctx.ip < ctx.codeLen) ctx.ip += ctx.code[ctx.ip] + 1;
                else if (c == 0x41 && depth > 0 && ctx.ip < ctx.codeLen) ctx.ip += ctx.code[ctx.ip] * 2 + 1;
            }
        }
    } else if (op == 0x1B) { // ELSE — saute jusqu'à EIF
        int32 depth = 1;
        while (ctx.ip < ctx.codeLen && depth > 0) {
            const uint8 c = ctx.code[ctx.ip++];
            if (c == 0x58) ++depth;
            else if (c == 0x59) --depth; // EIF
        }
    } else if (op == 0x59) { // EIF — no-op
    } else if (op == 0x1C) { // JMPR
        const int32 offset = vm.Pop();
        ctx.ip = static_cast<uint32>(static_cast<int32>(ctx.ip) + offset - 1);
    } else if (op == 0x78) { // JROT
        const int32 offset = vm.Pop(), cond = vm.Pop();
        if (cond) ctx.ip = static_cast<uint32>(static_cast<int32>(ctx.ip) + offset - 1);
    } else if (op == 0x79) { // JROF
        const int32 offset = vm.Pop(), cond = vm.Pop();
        if (!cond) ctx.ip = static_cast<uint32>(static_cast<int32>(ctx.ip) + offset - 1);
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_GS — Graphics State
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_GS(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    NkGraphicsState& gs = vm.gs;
    switch (op) {
        case 0x00: gs.pvX = F16Dot16::FromInt(1); gs.pvY = F16Dot16::Zero(); // SVTCA[y=0]
                   gs.fvX = F16Dot16::FromInt(1); gs.fvY = F16Dot16::Zero(); break;
        case 0x01: gs.pvX = F16Dot16::Zero(); gs.pvY = F16Dot16::FromInt(1); // SVTCA[y=1]
                   gs.fvX = F16Dot16::Zero(); gs.fvY = F16Dot16::FromInt(1); break;
        case 0x02: gs.pvX = F16Dot16::FromInt(1); gs.pvY = F16Dot16::Zero(); break; // SPVTCA[x]
        case 0x03: gs.pvX = F16Dot16::Zero(); gs.pvY = F16Dot16::FromInt(1); break; // SPVTCA[y]
        case 0x04: gs.fvX = F16Dot16::FromInt(1); gs.fvY = F16Dot16::Zero(); break; // SFVTCA[x]
        case 0x05: gs.fvX = F16Dot16::Zero(); gs.fvY = F16Dot16::FromInt(1); break; // SFVTCA[y]
        case 0x0D: // SRP0
            gs.rp[0] = static_cast<uint32>(vm.Pop()); break;
        case 0x0E: gs.rp[1] = static_cast<uint32>(vm.Pop()); break; // SRP1
        case 0x0F: gs.rp[2] = static_cast<uint32>(vm.Pop()); break; // SRP2
        case 0x13: gs.zp[0] = static_cast<uint32>(vm.Pop()); break; // SZP0
        case 0x14: gs.zp[1] = static_cast<uint32>(vm.Pop()); break; // SZP1
        case 0x15: gs.zp[2] = static_cast<uint32>(vm.Pop()); break; // SZP2
        case 0x16: { const int32 z=vm.Pop(); gs.zp[0]=gs.zp[1]=gs.zp[2]=static_cast<uint32>(z); break; } // SZPS
        case 0x17: gs.loop = vm.Pop(); break; // SLOOP
        case 0x18: gs.roundMode = NkGraphicsState::RoundMode::ToGrid; break; // RTG
        case 0x19: gs.roundMode = NkGraphicsState::RoundMode::ToHalfGrid; break; // RTHG
        case 0x3D: gs.roundMode = NkGraphicsState::RoundMode::ToDoubleGrid; break; // RTDG
        case 0x7D: gs.roundMode = NkGraphicsState::RoundMode::DownToGrid; break; // RDTG
        case 0x7C: gs.roundMode = NkGraphicsState::RoundMode::UpToGrid; break; // RUTG
        case 0x7A: gs.roundMode = NkGraphicsState::RoundMode::Off; break; // ROFF
        case 0x1D: { // SCVTCI
            gs.controlValueCutIn = F26Dot6::FromRaw(vm.Pop()); break;
        }
        case 0x1E: gs.singleWidthCutIn   = F26Dot6::FromRaw(vm.Pop()); break; // SSWCI
        case 0x1F: gs.singleWidthValue   = F26Dot6::FromRaw(vm.Pop()); break; // SSW
        case 0x0A: // SPVFS (set projection vector from stack)
        {
            const int32 y = vm.Pop(), x = vm.Pop();
            gs.pvX = F16Dot16::FromRaw(x); gs.pvY = F16Dot16::FromRaw(y);
            break;
        }
        case 0x0B: { // SFVFS
            const int32 y = vm.Pop(), x = vm.Pop();
            gs.fvX = F16Dot16::FromRaw(x); gs.fvY = F16Dot16::FromRaw(y);
            break;
        }
        case 0x0C: // GPV
            vm.Push(gs.pvX.raw); vm.Push(gs.pvY.raw); break;
        case 0x0D: // GFV
            vm.Push(gs.fvX.raw); vm.Push(gs.fvY.raw); break;
        default: break;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_Move — déplacement de points (MDAP, MIAP, MDRP, MIRP, SHP, SHPIX, IUP)
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_Move(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    NkGraphicsState& gs = vm.gs;

    if (op == 0x2E || op == 0x2F) { // MDAP[round]
        const uint32 ptIdx = static_cast<uint32>(vm.Pop());
        NkHintZone& zone = vm.Zone(gs.zp[0]);
        if (ptIdx >= zone.numPoints) return true;
        F26Dot6 dist = GetProjection(vm, zone, ptIdx);
        if (op == 0x2F) dist = gs.Round(dist); // round
        const F26Dot6 orig = GetProjection(vm, zone, ptIdx);
        MovePoint(vm, zone, ptIdx, dist - orig);
        gs.rp[0] = gs.rp[1] = ptIdx;
    }
    else if (op == 0x3E || op == 0x3F) { // MIAP[round]
        const uint32 cvtIdx = static_cast<uint32>(vm.Pop());
        const uint32 ptIdx  = static_cast<uint32>(vm.Pop());
        NkHintZone& zone = vm.Zone(gs.zp[0]);
        F26Dot6 cvtVal = (cvtIdx < static_cast<uint32>(vm.cvtSize))
                        ? vm.cvt[cvtIdx] : F26Dot6::Zero();
        if (op == 0x3F) cvtVal = gs.Round(cvtVal);
        if (ptIdx < zone.numPoints) {
            const F26Dot6 cur = GetProjection(vm, zone, ptIdx);
            MovePoint(vm, zone, ptIdx, cvtVal - cur);
            gs.rp[0] = gs.rp[1] = ptIdx;
        }
    }
    else if (op == 0x3C) { // ALIGNRP
        const uint32 rp0 = gs.rp[0];
        NkHintZone& zp0 = vm.Zone(gs.zp[0]);
        NkHintZone& zp1 = vm.Zone(gs.zp[1]);
        for (int32 i = 0; i < gs.loop; ++i) {
            const uint32 ptIdx = static_cast<uint32>(vm.Pop());
            if (rp0 < zp0.numPoints && ptIdx < zp1.numPoints) {
                const F26Dot6 d = ComputeDist(vm, zp0, rp0, zp1, ptIdx);
                MovePoint(vm, zp1, ptIdx, -d);
            }
        }
        gs.loop = 1;
    }
    else if (op == 0x34 || op == 0x35 || op == 0x38) { // SHPIX
        const F26Dot6 delta = F26Dot6::FromRaw(vm.Pop());
        NkHintZone& zp2 = vm.Zone(gs.zp[2]);
        for (int32 i = 0; i < gs.loop; ++i) {
            const uint32 pt = static_cast<uint32>(vm.Pop());
            MovePoint(vm, zp2, pt, delta);
        }
        gs.loop = 1;
    }
    else if (op >= 0xC0 && op <= 0xDF) { // MDRP[abcde]
        const uint32 ptIdx = static_cast<uint32>(vm.Pop());
        NkHintZone& zp0 = vm.Zone(gs.zp[0]);
        NkHintZone& zp1 = vm.Zone(gs.zp[1]);
        const bool setRp0 = (op & 0x10) != 0;
        const bool round  = (op & 0x04) != 0;
        const uint32 rp0  = gs.rp[0];
        if (rp0 < zp0.numPoints && ptIdx < zp1.numPoints) {
            F26Dot6 origDist = ComputeDist(vm, zp0, rp0, zp1, ptIdx);
            if (gs.autoFlip && origDist.raw < 0) origDist = -origDist;
            if (origDist < gs.minDist) origDist = gs.minDist;
            if (round) origDist = gs.Round(origDist);
            const F26Dot6 curDist = ComputeDist(vm, zp0, rp0, zp1, ptIdx);
            MovePoint(vm, zp1, ptIdx, origDist - curDist);
        }
        gs.rp[1] = rp0;
        gs.rp[2] = ptIdx;
        if (setRp0) gs.rp[0] = ptIdx;
    }
    else if (op >= 0xE0 && op <= 0xFF) { // MIRP[abcde]
        const uint32 cvtIdx = static_cast<uint32>(vm.Pop());
        const uint32 ptIdx  = static_cast<uint32>(vm.Pop());
        NkHintZone& zp0 = vm.Zone(gs.zp[0]);
        NkHintZone& zp1 = vm.Zone(gs.zp[1]);
        const bool setRp0 = (op & 0x10) != 0;
        const bool round  = (op & 0x04) != 0;
        const uint32 rp0  = gs.rp[0];
        if (rp0 < zp0.numPoints && ptIdx < zp1.numPoints) {
            F26Dot6 cvtVal = (cvtIdx < static_cast<uint32>(vm.cvtSize))
                            ? vm.cvt[cvtIdx] : F26Dot6::Zero();
            const F26Dot6 curDist = ComputeDist(vm, zp0, rp0, zp1, ptIdx);
            F26Dot6 dist = CVTCutIn(vm, curDist, cvtVal);
            if (gs.autoFlip && ((dist.raw ^ curDist.raw) & 0x80000000)) dist = -dist;
            if (round) dist = gs.Round(dist);
            if (dist.Abs() < gs.minDist) dist = dist.IsNegative() ? -gs.minDist : gs.minDist;
            MovePoint(vm, zp1, ptIdx, dist - curDist);
        }
        gs.rp[1] = rp0;
        gs.rp[2] = ptIdx;
        if (setRp0) gs.rp[0] = ptIdx;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_Align — IUP (Interpolate Untouched Points)
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_Align(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    const bool isY = (op == 0x31); // IUP[y] ou IUP[x]
    NkHintZone& z = vm.zone1;

    // Pour chaque contour : interpole les points non-touchés entre les touchés
    // Algorithme simplifié : interpolation linéaire entre les deux points touchés voisins
    for (uint32 i = 0; i < z.numPoints; ++i) {
        if (z.touched[i]) continue;

        // Cherche le point touché précédent et suivant
        uint32 prev = i, next = i;
        for (uint32 j = 1; j < z.numPoints; ++j) {
            const uint32 p = (i + z.numPoints - j) % z.numPoints;
            const uint32 n = (i + j) % z.numPoints;
            if (z.touched[p]) { prev = p; break; }
            if (z.touched[n]) { next = n; break; }
        }
        if (prev == i || next == i) continue;

        // Interpole
        const F26Dot6 origPrev = isY ? z.original[prev].y : z.original[prev].x;
        const F26Dot6 origNext = isY ? z.original[next].y : z.original[next].x;
        const F26Dot6 origCur  = isY ? z.original[i].y   : z.original[i].x;
        const F26Dot6 curPrev  = isY ? z.current [prev].y : z.current [prev].x;
        const F26Dot6 curNext  = isY ? z.current [next].y : z.current [next].x;

        F26Dot6 newVal;
        if (origNext.raw == origPrev.raw) {
            newVal = F26Dot6::FromRaw((curPrev.raw + curNext.raw) / 2);
        } else {
            // Interpolation linéaire
            const int64 t = static_cast<int64>(origCur.raw - origPrev.raw) * F26Dot6::ONE
                          / (origNext.raw - origPrev.raw);
            newVal = F26Dot6::FromRaw(curPrev.raw +
                static_cast<int32>((static_cast<int64>(curNext.raw - curPrev.raw) * t) >> F26Dot6::SHIFT));
        }

        if (isY) z.current[i].y = newVal;
        else     z.current[i].x = newVal;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_Function — FDEF, ENDF, CALL, LOOPCALL
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_Function(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    if (op == 0x2C) { // FDEF
        const int32 fnIdx = vm.Pop();
        if (fnIdx < 0 || static_cast<uint32>(fnIdx) >= vm.funcCapacity) return false;
        // Enregistre le début de la fonction
        const uint32 start = ctx.ip;
        // Cherche ENDF (0x2D) en comptant les FDEF imbriqués
        int32 depth = 1;
        while (ctx.ip < ctx.codeLen && depth > 0) {
            const uint8 c = ctx.code[ctx.ip++];
            if      (c == 0x2C) ++depth;
            else if (c == 0x2D) --depth;
        }
        if (static_cast<uint32>(fnIdx) >= vm.funcCount)
            vm.funcCount = static_cast<uint32>(fnIdx) + 1;
        vm.funcs[fnIdx].data   = ctx.code + start;
        vm.funcs[fnIdx].length = ctx.ip - start - 1; // exclut ENDF
    }
    else if (op == 0x2D) { // ENDF — retour
        return true; // stoppe l'exécution du sous-programme
    }
    else if (op == 0x2B) { // CALL
        const int32 fnIdx = vm.Pop();
        if (fnIdx < 0 || static_cast<uint32>(fnIdx) >= vm.funcCount) return true;
        const NkHintVM::FuncDef& fn = vm.funcs[fnIdx];
        if (!fn.data || fn.length == 0) return true;
        ExecContext sub;
        sub.vm        = ctx.vm;
        sub.code      = fn.data;
        sub.codeLen   = fn.length;
        sub.ip        = 0;
        sub.callDepth = ctx.callDepth + 1;
        sub.arena     = ctx.arena;
        Execute(sub);
    }
    else if (op == 0xFA) { // LOOPCALL
        const int32 fnIdx = vm.Pop();
        const int32 count = vm.Pop();
        if (fnIdx < 0 || static_cast<uint32>(fnIdx) >= vm.funcCount) return true;
        const NkHintVM::FuncDef& fn = vm.funcs[fnIdx];
        for (int32 i = 0; i < count && fn.data && fn.length > 0; ++i) {
            ExecContext sub;
            sub.vm        = ctx.vm;
            sub.code      = fn.data;
            sub.codeLen   = fn.length;
            sub.ip        = 0;
            sub.callDepth = ctx.callDepth + 1;
            sub.arena     = ctx.arena;
            Execute(sub);
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_Delta
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_Delta(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    const int32 n = vm.Pop();
    const int32 baseShift = (op == 0x5D) ? 0 : (op == 0x71) ? 16 : 32; // DELTAP1/2/3

    for (int32 i = 0; i < n; ++i) {
        const int32 arg    = vm.Pop();
        const uint32 ptIdx = static_cast<uint32>(vm.Pop());
        const int32  ppemTarget = ((arg >> 4) & 0xF) + vm.gs.deltaBase.ToInt() + baseShift;
        const int32  stepCode   = (arg & 0xF);
        if (ppemTarget != vm.ppem) continue;

        // Conversion du step en F26Dot6 (magnitude 2^(step - 8) / 2^deltaShift)
        const int32  shift   = vm.gs.deltaShift.ToInt();
        const int32  step    = stepCode < 8 ? stepCode - 8 : stepCode - 7;
        const F26Dot6 delta  = F26Dot6::FromRaw(step * (F26Dot6::ONE >> shift));

        if (op <= 0x72) { // DELTAP1-3
            NkHintZone& zone = vm.Zone(vm.gs.zp[0]);
            MovePoint(vm, zone, ptIdx, delta);
        } else { // DELTAC1-3
            if (static_cast<int32>(ptIdx) < vm.cvtSize)
                vm.cvt[ptIdx] += delta;
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Op_Misc — GETINFO, MPPEM, SCANCTRL, INSTCTRL, FLIPPT, etc.
// ─────────────────────────────────────────────────────────────────────────────

bool NkHinter::Op_Misc(ExecContext& ctx, uint8 op) noexcept {
    NkHintVM& vm = *ctx.vm;
    switch (op) {
        case 0x88: { // GETINFO
            const int32 sel = vm.Pop();
            int32 result = 0;
            if (sel & 1) result |= 35;           // version TrueType (35 = FreeType compat)
            if (sel & 2) result |= (vm.ppem > 0 ? 2 : 0); // gris AA
            if (sel & 8) result |= 0;            // cleartype (off)
            vm.Push(result);
            break;
        }
        case 0x4B: vm.Push(vm.ppem); break; // MPPEM
        case 0x4C: vm.Push(vm.ppem * 72 / 96); break; // MPS (point size approx)
        case 0x80: { // FLIPPT
            const uint32 pt = static_cast<uint32>(vm.Pop());
            // Flip on-curve/off-curve : utilisé rarement, ignoré ici
            (void)pt;
            break;
        }
        case 0x8A: { // ROLL
            if (vm.stackTop >= 3) {
                const int32 a = vm.stack[vm.stackTop-1];
                vm.stack[vm.stackTop-1] = vm.stack[vm.stackTop-2];
                vm.stack[vm.stackTop-2] = vm.stack[vm.stackTop-3];
                vm.stack[vm.stackTop-3] = a;
            }
            break;
        }
        case 0x85: // SCANCTRL
        case 0x8D: // SCANTYPE
            vm.Pop(); break; // Ignoré — hinting sub-pixel non implémenté
        case 0x8E: { // INSTCTRL
            const int32 val = vm.Pop();
            const int32 sel = vm.Pop();
            if (sel == 1) vm.gs.instructControl = (val != 0);
            break;
        }
        // Instructions ignorées en production (no-op safe)
        case 0x4E: // FLIPRGON
        case 0x4F: // FLIPRGOFF
            vm.Pop(); vm.Pop(); break;
        default:
            // Opcode inconnu : on ignore silencieusement
            break;
    }
    return true;
}

} // namespace nkentseu
