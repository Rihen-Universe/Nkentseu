// =============================================================================
// NkMetalCommandBuffer.mm
// =============================================================================
#ifdef NK_RHI_METAL_ENABLED
#import "NkMetalCommandBuffer.h"
#import "NkMetalDevice.h"
#import <Metal/Metal.h>

#define RENDER_ENC ((__bridge id<MTLRenderCommandEncoder>)mRenderEncoder)
#define COMPUTE_ENC((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder)
#define BLIT_ENC  ((__bridge id<MTLBlitCommandEncoder>)mBlitEncoder)
#define CMD_BUF   ((__bridge id<MTLCommandBuffer>)mCmdBuf)
#define DEV_MTL   (mDev->MtlDevice())
#define QUEUE_MTL (mDev->MtlQueue())

namespace nkentseu {

NkMetalCommandBuffer::NkMetalCommandBuffer(NkMetalDevice* dev, NkCommandBufferType type)
    : mDev(dev), mType(type) {
    id<MTLCommandBuffer> cmd = [dev->MtlQueue() commandBuffer];
    mCmdBuf = (__bridge_retained void*)cmd;
}

NkMetalCommandBuffer::~NkMetalCommandBuffer() {
    EndCurrentEncoder();
    if (mCmdBuf) CFRelease(mCmdBuf);
}

void NkMetalCommandBuffer::Begin() {
    // Obtenir un nouveau command buffer
    if (mCmdBuf) CFRelease(mCmdBuf);
    id<MTLCommandBuffer> cmd = [mDev->MtlQueue() commandBuffer];
    mCmdBuf = (__bridge_retained void*)cmd;
}

void NkMetalCommandBuffer::End() { EndCurrentEncoder(); }
void NkMetalCommandBuffer::Reset() {
    EndCurrentEncoder();
    if (mCmdBuf) CFRelease(mCmdBuf);
    id<MTLCommandBuffer> cmd = [mDev->MtlQueue() commandBuffer];
    mCmdBuf = (__bridge_retained void*)cmd;
}

void NkMetalCommandBuffer::EndCurrentEncoder() {
    if (mRenderEncoder)  { [RENDER_ENC endEncoding];  CFRelease(mRenderEncoder);  mRenderEncoder=nullptr; }
    if (mComputeEncoder) { [((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder) endEncoding]; CFRelease(mComputeEncoder); mComputeEncoder=nullptr; }
    if (mBlitEncoder)    { [BLIT_ENC endEncoding];    CFRelease(mBlitEncoder);    mBlitEncoder=nullptr; }
}

void NkMetalCommandBuffer::CommitAndWait() {
    EndCurrentEncoder();
    [CMD_BUF commit];
    [CMD_BUF waitUntilCompleted];
}

void NkMetalCommandBuffer::CommitAndPresent(void* drawable) {
    EndCurrentEncoder();
    if (drawable) {
        id<CAMetalDrawable> d = (__bridge id<CAMetalDrawable>)drawable;
        [CMD_BUF presentDrawable:d];
    }
    [CMD_BUF commit];
}

// =============================================================================
// Render Pass
// =============================================================================
void NkMetalCommandBuffer::BeginRenderPass(NkRenderPassHandle rpH,
                                             NkFramebufferHandle fbH,
                                             const NkRect2D& /*area*/) {
    EndCurrentEncoder();

    auto* fb  = mDev->GetFBO(fbH.id);
    auto  rpit = mDev->GetFBO(fbH.id); // même objet

    MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];

    // Color attachments
    if (fb) {
        for (uint32 i = 0; i < fb->colorCount; i++) {
            id<MTLTexture> tex = (__bridge id<MTLTexture>)mDev->GetMTLTexture(fb->colorAttachments[i].id);
            rpd.colorAttachments[i].texture     = tex;
            rpd.colorAttachments[i].loadAction  = MTLLoadActionClear;
            rpd.colorAttachments[i].storeAction = MTLStoreActionStore;
            rpd.colorAttachments[i].clearColor  = MTLClearColorMake(0, 0, 0, 1);
        }
        if (fb->depthAttachment.IsValid()) {
            id<MTLTexture> dtex = (__bridge id<MTLTexture>)mDev->GetMTLTexture(fb->depthAttachment.id);
            rpd.depthAttachment.texture    = dtex;
            rpd.depthAttachment.loadAction = MTLLoadActionClear;
            rpd.depthAttachment.storeAction= MTLStoreActionDontCare;
            rpd.depthAttachment.clearDepth = 1.0;
        }
    }

    id<MTLRenderCommandEncoder> enc = [CMD_BUF renderCommandEncoderWithDescriptor:rpd];
    mRenderEncoder = (__bridge_retained void*)enc;
}

void NkMetalCommandBuffer::EndRenderPass() {
    if (mRenderEncoder) {
        [RENDER_ENC endEncoding];
        CFRelease(mRenderEncoder);
        mRenderEncoder = nullptr;
    }
}

// =============================================================================
// Viewport & Scissor
// =============================================================================
void NkMetalCommandBuffer::SetViewport(const NkViewport& vp) {
    if (!mRenderEncoder) return;
    MTLViewport v{ vp.x, vp.y, vp.width, vp.height, vp.minDepth, vp.maxDepth };
    [RENDER_ENC setViewport:v];
}

void NkMetalCommandBuffer::SetViewports(const NkViewport* vps, uint32 n) {
    if (!mRenderEncoder) return;
    // Metal 3+: setViewports:count:
    MTLViewport v{ vps[0].x, vps[0].y, vps[0].width, vps[0].height, vps[0].minDepth, vps[0].maxDepth };
    [RENDER_ENC setViewport:v];
}

void NkMetalCommandBuffer::SetScissor(const NkRect2D& r) {
    if (!mRenderEncoder) return;
    MTLScissorRect sc{ (NSUInteger)r.x, (NSUInteger)r.y,
                       (NSUInteger)r.width, (NSUInteger)r.height };
    [RENDER_ENC setScissorRect:sc];
}

void NkMetalCommandBuffer::SetScissors(const NkRect2D* r, uint32 /*n*/) {
    SetScissor(r[0]);
}

// =============================================================================
// Pipeline
// =============================================================================
void NkMetalCommandBuffer::BindGraphicsPipeline(NkPipelineHandle p) {
    if (!mRenderEncoder) return;
    auto* pipe = mDev->GetPipeline(p.id);
    if (!pipe) return;
    [RENDER_ENC setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)pipe->rpso];
    if (pipe->dss)
        [RENDER_ENC setDepthStencilState:(__bridge id<MTLDepthStencilState>)pipe->dss];
    [RENDER_ENC setFrontFacingWinding:pipe->frontFaceCCW ? MTLWindingCounterClockwise : MTLWindingClockwise];
    MTLCullMode cull = pipe->cullMode == 0 ? MTLCullModeNone :
                       pipe->cullMode == 1 ? MTLCullModeFront : MTLCullModeBack;
    [RENDER_ENC setCullMode:cull];
    if (pipe->depthBiasConst != 0 || pipe->depthBiasSlope != 0)
        [RENDER_ENC setDepthBias:pipe->depthBiasConst slopeScale:pipe->depthBiasSlope clamp:pipe->depthBiasClamp];
    mPrimitive = MTLPrimitiveTypeTriangle; // défaut
}

void NkMetalCommandBuffer::BindComputePipeline(NkPipelineHandle p) {
    EndCurrentEncoder();
    auto* pipe = mDev->GetPipeline(p.id);
    if (!pipe || !pipe->cpso) return;
    id<MTLComputeCommandEncoder> enc = [CMD_BUF computeCommandEncoder];
    [enc setComputePipelineState:(__bridge id<MTLComputePipelineState>)pipe->cpso];
    mComputeEncoder = (__bridge_retained void*)enc;
}

// =============================================================================
// Descriptor Set
// =============================================================================
void NkMetalCommandBuffer::BindDescriptorSet(NkDescSetHandle set,
                                               uint32 /*idx*/,
                                               uint32* /*off*/, uint32 /*cnt*/) {
    auto* ds = mDev->GetDescSet(set.id);
    if (!ds) return;

    for (auto& b : ds->bindings) {
        UINT slot = b.slot;
        if (b.bufId) {
            id<MTLBuffer> buf = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(b.bufId);
            if (mRenderEncoder) {
                [RENDER_ENC setVertexBuffer:buf offset:0 atIndex:slot];
                [RENDER_ENC setFragmentBuffer:buf offset:0 atIndex:slot];
            }
            if (mComputeEncoder)
                [((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder) setBuffer:buf offset:0 atIndex:slot];
        }
        if (b.texId) {
            id<MTLTexture> tex = (__bridge id<MTLTexture>)mDev->GetMTLTexture(b.texId);
            if (mRenderEncoder) {
                [RENDER_ENC setVertexTexture:tex atIndex:slot];
                [RENDER_ENC setFragmentTexture:tex atIndex:slot];
            }
            if (mComputeEncoder)
                [((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder) setTexture:tex atIndex:slot];
        }
        if (b.sampId) {
            id<MTLSamplerState> ss = (__bridge id<MTLSamplerState>)mDev->GetMTLSampler(b.sampId);
            if (mRenderEncoder) {
                [RENDER_ENC setVertexSamplerState:ss atIndex:slot];
                [RENDER_ENC setFragmentSamplerState:ss atIndex:slot];
            }
            if (mComputeEncoder)
                [((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder) setSamplerState:ss atIndex:slot];
        }
    }
}

// =============================================================================
// Push Constants — via setVertexBytes / setFragmentBytes
// =============================================================================
void NkMetalCommandBuffer::PushConstants(NkShaderStage stages, uint32 /*offset*/,
                                           uint32 size, const void* data) {
    UINT slot = 30; // slot réservé pour les push constants en MSL
    if (!mRenderEncoder && !mComputeEncoder) return;
    bool doVert = (uint32)stages & (uint32)NkShaderStage::NK_VERTEX;
    bool doFrag = (uint32)stages & (uint32)NkShaderStage::NK_FRAGMENT;
    bool doComp = (uint32)stages & (uint32)NkShaderStage::NK_COMPUTE;
    if (mRenderEncoder) {
        if (doVert || !doFrag) [RENDER_ENC setVertexBytes:data length:size atIndex:slot];
        if (doFrag || !doVert) [RENDER_ENC setFragmentBytes:data length:size atIndex:slot];
    }
    if (mComputeEncoder && doComp)
        [((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder) setBytes:data length:size atIndex:slot];
}

// =============================================================================
// Vertex / Index
// =============================================================================
void NkMetalCommandBuffer::BindVertexBuffer(uint32 binding, NkBufferHandle buf, uint64 off) {
    if (!mRenderEncoder) return;
    id<MTLBuffer> b = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(buf.id);
    [RENDER_ENC setVertexBuffer:b offset:(NSUInteger)off atIndex:binding];
}

void NkMetalCommandBuffer::BindVertexBuffers(uint32 first, const NkBufferHandle* bufs,
                                               const uint64* offs, uint32 n) {
    for (uint32 i = 0; i < n; i++) BindVertexBuffer(first + i, bufs[i], offs[i]);
}

void NkMetalCommandBuffer::BindIndexBuffer(NkBufferHandle buf, NkIndexFormat fmt, uint64 off) {
    mCurrentIndexBuffer = mDev->GetMTLBuffer(buf.id);
    mIndexOffset  = off;
    mIndexUint32  = (fmt == NkIndexFormat::NK_UINT32);
}

// =============================================================================
// Draw
// =============================================================================
void NkMetalCommandBuffer::Draw(uint32 vtx, uint32 inst, uint32 firstVtx, uint32 firstInst) {
    if (!mRenderEncoder) return;
    [RENDER_ENC drawPrimitives:mPrimitive
                   vertexStart:firstVtx
                   vertexCount:vtx
                 instanceCount:inst
                  baseInstance:firstInst];
}

void NkMetalCommandBuffer::DrawIndexed(uint32 idx, uint32 inst, uint32 firstIdx,
                                         int32 /*vtxOff*/, uint32 firstInst) {
    if (!mRenderEncoder || !mCurrentIndexBuffer) return;
    id<MTLBuffer> ib = (__bridge id<MTLBuffer>)mCurrentIndexBuffer;
    MTLIndexType  it = mIndexUint32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    NSUInteger stride = mIndexUint32 ? 4 : 2;
    NSUInteger offset = mIndexOffset + firstIdx * stride;

    [RENDER_ENC drawIndexedPrimitives:mPrimitive
                           indexCount:idx
                            indexType:it
                          indexBuffer:ib
                    indexBufferOffset:offset
                        instanceCount:inst
                           baseVertex:0
                         baseInstance:firstInst];
}

void NkMetalCommandBuffer::DrawIndirect(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) {
    if (!mRenderEncoder) return;
    id<MTLBuffer> b = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(buf.id);
    for (uint32 i = 0; i < cnt; i++)
        [RENDER_ENC drawPrimitives:mPrimitive
                    indirectBuffer:b indirectBufferOffset:off + i * stride];
}

void NkMetalCommandBuffer::DrawIndexedIndirect(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) {
    if (!mRenderEncoder || !mCurrentIndexBuffer) return;
    id<MTLBuffer> ib = (__bridge id<MTLBuffer>)mCurrentIndexBuffer;
    id<MTLBuffer> ab = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(buf.id);
    MTLIndexType  it = mIndexUint32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    for (uint32 i = 0; i < cnt; i++)
        [RENDER_ENC drawIndexedPrimitives:mPrimitive
                                indexType:it indexBuffer:ib indexBufferOffset:mIndexOffset
                           indirectBuffer:ab indirectBufferOffset:off + i * stride];
}

// =============================================================================
// Compute
// =============================================================================
void NkMetalCommandBuffer::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    if (!mComputeEncoder) return;
    auto* enc = (__bridge id<MTLComputeCommandEncoder>)mComputeEncoder;
    [enc dispatchThreadgroups:MTLSizeMake(gx,gy,gz)
        threadsPerThreadgroup:MTLSizeMake(1,1,1)];
}

void NkMetalCommandBuffer::DispatchIndirect(NkBufferHandle buf, uint64 off) {
    if (!mComputeEncoder) return;
    auto* enc = (__bridge id<MTLComputeCommandEncoder>)mComputeEncoder;
    id<MTLBuffer> b = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(buf.id);
    [enc dispatchThreadgroupsWithIndirectBuffer:b indirectBufferOffset:off
         threadsPerThreadgroup:MTLSizeMake(1,1,1)];
}

// =============================================================================
// Copies
// =============================================================================
void NkMetalCommandBuffer::CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                                        const NkBufferCopyRegion& r) {
    EndCurrentEncoder();
    id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
    [blit copyFromBuffer:(__bridge id<MTLBuffer>)mDev->GetMTLBuffer(src.id)
             sourceOffset:r.srcOffset
               toBuffer:(__bridge id<MTLBuffer>)mDev->GetMTLBuffer(dst.id)
      destinationOffset:r.dstOffset
                   size:r.size];
    [blit endEncoding];
}

void NkMetalCommandBuffer::CopyBufferToTexture(NkBufferHandle src, NkTextureHandle dst,
                                                 const NkBufferTextureCopyRegion& r) {
    EndCurrentEncoder();
    id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
    [blit copyFromBuffer:(__bridge id<MTLBuffer>)mDev->GetMTLBuffer(src.id)
             sourceOffset:r.bufferOffset
        sourceBytesPerRow:r.bufferRowPitch
      sourceBytesPerImage:r.bufferRowPitch * r.height
               sourceSize:MTLSizeMake(r.width, r.height, r.depth > 0 ? r.depth : 1)
                toTexture:(__bridge id<MTLTexture>)mDev->GetMTLTexture(dst.id)
         destinationSlice:r.arrayLayer
         destinationLevel:r.mipLevel
        destinationOrigin:MTLOriginMake(r.x, r.y, r.z)];
    [blit endEncoding];
}

void NkMetalCommandBuffer::CopyTextureToBuffer(NkTextureHandle src, NkBufferHandle dst,
                                                 const NkBufferTextureCopyRegion& r) {
    EndCurrentEncoder();
    id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
    [blit copyFromTexture:(__bridge id<MTLTexture>)mDev->GetMTLTexture(src.id)
             sourceSlice:r.arrayLayer sourceLevel:r.mipLevel
            sourceOrigin:MTLOriginMake(r.x,r.y,r.z)
              sourceSize:MTLSizeMake(r.width,r.height,r.depth>0?r.depth:1)
                toBuffer:(__bridge id<MTLBuffer>)mDev->GetMTLBuffer(dst.id)
       destinationOffset:r.bufferOffset
  destinationBytesPerRow:r.bufferRowPitch
destinationBytesPerImage:r.bufferRowPitch * r.height];
    [blit endEncoding];
}

void NkMetalCommandBuffer::CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                                         const NkTextureCopyRegion& r) {
    EndCurrentEncoder();
    id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
    [blit copyFromTexture:(__bridge id<MTLTexture>)mDev->GetMTLTexture(src.id)
             sourceSlice:r.srcLayer sourceLevel:r.srcMip
            sourceOrigin:MTLOriginMake(r.srcX,r.srcY,r.srcZ)
              sourceSize:MTLSizeMake(r.width,r.height,r.depth>0?r.depth:1)
               toTexture:(__bridge id<MTLTexture>)mDev->GetMTLTexture(dst.id)
        destinationSlice:r.dstLayer destinationLevel:r.dstMip
       destinationOrigin:MTLOriginMake(r.dstX,r.dstY,r.dstZ)];
    [blit endEncoding];
}

void NkMetalCommandBuffer::BlitTexture(NkTextureHandle src, NkTextureHandle dst,
                                         const NkTextureCopyRegion& r, NkFilter /*f*/) {
    CopyTexture(src, dst, r); // Metal n'a pas de blit avec filtre → copie directe
}

// =============================================================================
// Mip generation
// =============================================================================
void NkMetalCommandBuffer::GenerateMipmaps(NkTextureHandle tex, NkFilter /*f*/) {
    EndCurrentEncoder();
    id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
    [blit generateMipmapsForTexture:(__bridge id<MTLTexture>)mDev->GetMTLTexture(tex.id)];
    [blit endEncoding];
}

// =============================================================================
// Debug
// =============================================================================
void NkMetalCommandBuffer::BeginDebugGroup(const char* name, float r, float g, float b) {
    NSString* s = [NSString stringWithUTF8String:name];
    if (mRenderEncoder)  [RENDER_ENC pushDebugGroup:s];
    else                 [CMD_BUF pushDebugGroup:s];
    (void)r; (void)g; (void)b;
}
void NkMetalCommandBuffer::EndDebugGroup() {
    if (mRenderEncoder)  [RENDER_ENC popDebugGroup];
    else                 [CMD_BUF popDebugGroup];
}
void NkMetalCommandBuffer::InsertDebugLabel(const char* name) {
    NSString* s = [NSString stringWithUTF8String:name];
    if (mRenderEncoder)  [RENDER_ENC insertDebugSignpost:s];
    else                 [CMD_BUF addCompletedHandler:^(id<MTLCommandBuffer>){ (void)s; }];
}

} // namespace nkentseu
#endif // NK_RHI_METAL_ENABLED
