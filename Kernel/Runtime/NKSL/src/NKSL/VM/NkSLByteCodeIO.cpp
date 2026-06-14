// =============================================================================
// NkSLByteCodeIO.cpp — Sérialisation binaire du bytecode NkSL (.nkbc)
// =============================================================================
#include "NKSL/VM/NkSLByteCodeIO.h"
#include "NKFileSystem/NkFile.h"
#include <cstring>

namespace nkentseu {

    static const uint32 kMagic   = 0x4342424E; // 'N','K','B','C' little-endian
    static const uint32 kVersion = 1;

    // ── Writer ───────────────────────────────────────────────────────────────
    struct BcWriter {
        NkVector<uint8>& buf;
        explicit BcWriter(NkVector<uint8>& b) : buf(b) {}
        void Bytes(const void* p, usize n) {
            const uint8* s = (const uint8*)p;
            for (usize i = 0; i < n; ++i) buf.PushBack(s[i]);
        }
        void U8 (uint8 v)  { buf.PushBack(v); }
        void U16(uint16 v) { Bytes(&v, 2); }
        void U32(uint32 v) { Bytes(&v, 4); }
        void I32(int32 v)  { Bytes(&v, 4); }
        void F32(float v)  { Bytes(&v, 4); }
        void Str(const NkString& s) { U32((uint32)s.Size()); Bytes(s.CStr(), s.Size()); }
        void Sym(const NkSLVMSymbol& s) { Str(s.name); U32(s.offset); U8(s.count); U32(s.location); }
    };

    // ── Reader ───────────────────────────────────────────────────────────────
    struct BcReader {
        const uint8* p; usize n; usize off = 0; bool ok = true;
        BcReader(const uint8* d, usize sz) : p(d), n(sz) {}
        bool Need(usize k) { if (off + k > n) { ok = false; return false; } return true; }
        void Bytes(void* dst, usize k) { if (!Need(k)) return; std::memcpy(dst, p + off, k); off += k; }
        uint8  U8 () { uint8 v=0;  Bytes(&v,1); return v; }
        uint16 U16() { uint16 v=0; Bytes(&v,2); return v; }
        uint32 U32() { uint32 v=0; Bytes(&v,4); return v; }
        int32  I32() { int32 v=0;  Bytes(&v,4); return v; }
        float  F32() { float v=0;  Bytes(&v,4); return v; }
        NkString Str() {
            uint32 len = U32(); NkString s;
            if (!Need(len)) return s;
            s = NkString((const char*)(p + off), (usize)len); off += len; return s;
        }
        NkSLVMSymbol Sym() { NkSLVMSymbol s; s.name=Str(); s.offset=U32(); s.count=U8(); s.location=U32(); return s; }
    };

    // =============================================================================
    bool NkSLByteCodeSerialize(const NkSLByteProgram& prog, NkVector<uint8>& out) {
        out.Clear();
        BcWriter w(out);
        w.U32(kMagic); w.U32(kVersion);
        w.U32((uint32)prog.stage);
        w.U32(prog.regCount);
        w.U32(prog.inputFloats);
        w.U32(prog.outputFloats);

        // code
        w.U32((uint32)prog.code.Size());
        for (const auto& in : prog.code) {
            w.U16((uint16)in.op); w.U16(in.a); w.U16(in.b); w.U16(in.c); w.I32(in.imm); w.U8(in.aux);
        }
        // constants
        w.U32((uint32)prog.constants.Size());
        for (const auto& k : prog.constants) {
            w.U8(k.count);
            for (uint8 i = 0; i < k.count; ++i) w.F32(k.v[i]);
        }
        // inputs / outputs / uniforms
        w.U32((uint32)prog.inputs.Size());  for (const auto& s : prog.inputs)  w.Sym(s);
        w.U32((uint32)prog.outputs.Size()); for (const auto& s : prog.outputs) w.Sym(s);
        w.U32((uint32)prog.uniforms.Size());for (const auto& s : prog.uniforms)w.Sym(s);
        // samplers
        w.U32((uint32)prog.samplers.Size());
        for (const auto& s : prog.samplers) { w.Str(s.name); w.U32(s.index); w.U8(s.isShadow?1:0); }
        return true;
    }

    bool NkSLByteCodeDeserialize(const uint8* data, usize size, NkSLByteProgram& prog) {
        BcReader r(data, size);
        if (r.U32() != kMagic)   return false;
        if (r.U32() != kVersion) return false;
        prog = NkSLByteProgram{};
        prog.stage        = (NkSLStage)r.U32();
        prog.regCount     = r.U32();
        prog.inputFloats  = r.U32();
        prog.outputFloats = r.U32();

        uint32 nc = r.U32();
        for (uint32 i = 0; i < nc && r.ok; ++i) {
            NkSLInstr in;
            in.op=(NkSLOp)r.U16(); in.a=r.U16(); in.b=r.U16(); in.c=r.U16(); in.imm=r.I32(); in.aux=r.U8();
            prog.code.PushBack(in);
        }
        uint32 nk = r.U32();
        for (uint32 i = 0; i < nk && r.ok; ++i) {
            NkSLValue v; v.count = r.U8();
            for (uint8 j = 0; j < v.count; ++j) v.v[j] = r.F32();
            prog.constants.PushBack(v);
        }
        uint32 ni = r.U32(); for (uint32 i=0;i<ni&&r.ok;++i) prog.inputs.PushBack(r.Sym());
        uint32 no = r.U32(); for (uint32 i=0;i<no&&r.ok;++i) prog.outputs.PushBack(r.Sym());
        uint32 nu = r.U32(); for (uint32 i=0;i<nu&&r.ok;++i) prog.uniforms.PushBack(r.Sym());
        uint32 ns = r.U32();
        for (uint32 i=0;i<ns&&r.ok;++i) { NkSLVMSampler s; s.name=r.Str(); s.index=r.U32(); s.isShadow=r.U8()!=0; prog.samplers.PushBack(s); }
        return r.ok;
    }

    // ── Fichier ──────────────────────────────────────────────────────────────
    bool NkSLByteCodeSaveFile(const NkSLByteProgram& prog, const char* path) {
        NkVector<uint8> blob;
        if (!NkSLByteCodeSerialize(prog, blob)) return false;
        NkFile f(path, NkFileMode::NK_WRITE_BINARY);
        if (!f.IsOpen()) return false;
        return f.Write(blob.Data(), blob.Size()) == blob.Size();
    }

    bool NkSLByteCodeLoadFile(const char* path, NkSLByteProgram& prog) {
        NkFile f(path, NkFileMode::NK_READ_BINARY);
        if (!f.IsOpen()) return false;
        nk_int64 sz = f.GetSize();
        if (sz <= 0) return false;
        NkVector<uint8> blob; blob.Resize((usize)sz);
        if (f.Read(blob.Data(), (usize)sz) != (usize)sz) return false;
        return NkSLByteCodeDeserialize(blob.Data(), (usize)sz, prog);
    }

} // namespace nkentseu
