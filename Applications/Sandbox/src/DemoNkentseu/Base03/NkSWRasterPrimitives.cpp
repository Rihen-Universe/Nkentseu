#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkWindowEvent.h"

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"

using namespace nkentseu;

namespace {
struct VertexPC {
    float px, py, pz;
    float r, g, b, a;
};
static_assert(sizeof(VertexPC) == sizeof(float) * 7, "VertexPC must stay tightly packed");

static NkBufferHandle UploadVertices(NkIDevice* device, const NkVector<VertexPC>& vertices) {
    const uint64 bytes = (uint64)vertices.Size() * sizeof(VertexPC);
    return device->CreateBuffer(NkBufferDesc::Vertex(bytes, vertices.Begin()));
}
}

int nkmain(const NkEntryState&) {
    NkWindowConfig winCfg;
    winCfg.title = "NkSW Raster Primitive Test";
    winCfg.width = 1280;
    winCfg.height = 720;
    winCfg.centered = true;
    winCfg.resizable = true;

    NkWindow window;
    if (!window.Create(winCfg)) {
        logger_src.Error("[NkSWRasterPrimitives] window creation failed");
        return 1;
    }

    NkDeviceInitInfo init{};
    init.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
    init.surface = window.GetSurfaceDesc();
    init.width = winCfg.width;
    init.height = winCfg.height;
    init.context.software.threading = true;
    init.context.software.useSSE = true;

    NkIDevice* device = NkDeviceFactory::Create(init);
    if (!device || !device->IsValid()) {
        logger_src.Error("[NkSWRasterPrimitives] software device creation failed");
        window.Close();
        return 1;
    }

    NkShaderDesc shaderDesc;
    NkShaderHandle shader = device->CreateShader(shaderDesc);
    if (!shader.IsValid()) {
        logger_src.Error("[NkSWRasterPrimitives] shader creation failed");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    NkVertexLayout layout;
    layout
        .AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
        .AddAttribute(1, 0, NkGPUFormat::NK_RGBA32_FLOAT, 3 * sizeof(float), "COLOR", 0)
        .AddBinding(0, sizeof(VertexPC));

    const NkRenderPassHandle swapchainRP = device->GetSwapchainRenderPass();

    auto makePipeline = [&](NkPrimitiveTopology topology) {
        NkGraphicsPipelineDesc pd{};
        pd.shader = shader;
        pd.vertexLayout = layout;
        pd.topology = topology;
        pd.rasterizer = NkRasterizerDesc::Default();
        pd.depthStencil = NkDepthStencilDesc::NoDepth();
        pd.blend = NkBlendDesc::Opaque();
        pd.renderPass = swapchainRP;
        return device->CreateGraphicsPipeline(pd);
    };

    NkPipelineHandle pipePoints = makePipeline(NkPrimitiveTopology::NK_POINT_LIST);
    NkPipelineHandle pipeLines = makePipeline(NkPrimitiveTopology::NK_LINE_LIST);
    NkPipelineHandle pipeTriangles = makePipeline(NkPrimitiveTopology::NK_TRIANGLE_LIST);

    if (!pipePoints.IsValid() || !pipeLines.IsValid() || !pipeTriangles.IsValid()) {
        logger_src.Error("[NkSWRasterPrimitives] pipeline creation failed");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    NkVector<VertexPC> points;
    points.PushBack({-0.90f, 0.85f, 0.5f, 1.f, 0.f, 0.f, 1.f});
    points.PushBack({-0.70f, 0.65f, 0.5f, 0.f, 1.f, 0.f, 1.f});
    points.PushBack({-0.50f, 0.85f, 0.5f, 0.f, 0.f, 1.f, 1.f});
    points.PushBack({-0.30f, 0.65f, 0.5f, 1.f, 1.f, 0.f, 1.f});
    points.PushBack({-0.10f, 0.85f, 0.5f, 1.f, 0.f, 1.f, 1.f});

    NkVector<VertexPC> lines;
    lines.PushBack({-0.85f, 0.35f, 0.5f, 1.f, 0.2f, 0.2f, 1.f});
    lines.PushBack({-0.15f, 0.10f, 0.5f, 1.f, 0.2f, 0.2f, 1.f});
    lines.PushBack({-0.85f, 0.10f, 0.5f, 0.2f, 1.f, 0.2f, 1.f});
    lines.PushBack({-0.15f, 0.35f, 0.5f, 0.2f, 1.f, 0.2f, 1.f});
    lines.PushBack({-0.50f, 0.38f, 0.5f, 0.2f, 0.6f, 1.f, 1.f});
    lines.PushBack({-0.50f, 0.02f, 0.5f, 0.2f, 0.6f, 1.f, 1.f});

    NkVector<VertexPC> triangle;
    triangle.PushBack({0.00f, 0.60f, 0.5f, 1.f, 0.3f, 0.1f, 1.f});
    triangle.PushBack({0.55f, 0.10f, 0.5f, 0.1f, 0.9f, 0.2f, 1.f});
    triangle.PushBack({-0.10f, 0.00f, 0.5f, 0.1f, 0.3f, 1.f, 1.f});

    NkVector<VertexPC> quad;
    quad.PushBack({0.10f, -0.10f, 0.5f, 0.9f, 0.1f, 0.1f, 1.f});
    quad.PushBack({0.85f, -0.10f, 0.5f, 0.1f, 0.9f, 0.1f, 1.f});
    quad.PushBack({0.85f, -0.75f, 0.5f, 0.1f, 0.1f, 0.9f, 1.f});
    quad.PushBack({0.10f, -0.10f, 0.5f, 0.9f, 0.1f, 0.1f, 1.f});
    quad.PushBack({0.85f, -0.75f, 0.5f, 0.1f, 0.1f, 0.9f, 1.f});
    quad.PushBack({0.10f, -0.75f, 0.5f, 0.95f, 0.95f, 0.1f, 1.f});

    NkBufferHandle vbPoints = UploadVertices(device, points);
    NkBufferHandle vbLines = UploadVertices(device, lines);
    NkBufferHandle vbTriangle = UploadVertices(device, triangle);
    NkBufferHandle vbQuad = UploadVertices(device, quad);

    if (!vbPoints.IsValid() || !vbLines.IsValid() || !vbTriangle.IsValid() || !vbQuad.IsValid()) {
        logger_src.Error("[NkSWRasterPrimitives] vertex buffer creation failed");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    if (!cmd || !cmd->IsValid()) {
        logger_src.Error("[NkSWRasterPrimitives] command buffer creation failed");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    bool running = true;
    uint32 w = device->GetSwapchainWidth();
    uint32 h = device->GetSwapchainHeight();

    NkEventSystem& events = NkEvents();
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        w = (uint32)e->GetWidth();
        h = (uint32)e->GetHeight();
    });

    logger_src.Info("[NkSWRasterPrimitives] running: points/lines/triangle/quad");

    while (running) {
        events.PollEvents();
        if (!running) break;

        if (w == 0 || h == 0) continue;
        if (w != device->GetSwapchainWidth() || h != device->GetSwapchainHeight()) {
            device->OnResize(w, h);
        }

        NkFrameContext frame{};
        if (!device->BeginFrame(frame)) continue;

        cmd->Begin();
        NkRect2D area{0, 0, (int32)w, (int32)h};
        if (cmd->BeginRenderPass(device->GetSwapchainRenderPass(), device->GetSwapchainFramebuffer(), area)) {
            NkViewport vp;
            vp.x = 0.f; vp.y = 0.f; vp.width = (float)w; vp.height = (float)h; vp.minDepth = 0.f; vp.maxDepth = 1.f;
            cmd->SetViewport(vp);
            cmd->SetScissor(area);

            cmd->BindGraphicsPipeline(pipePoints);
            cmd->BindVertexBuffer(0, vbPoints);
            cmd->Draw((uint32)points.Size());

            cmd->BindGraphicsPipeline(pipeLines);
            cmd->BindVertexBuffer(0, vbLines);
            cmd->Draw((uint32)lines.Size());

            cmd->BindGraphicsPipeline(pipeTriangles);
            cmd->BindVertexBuffer(0, vbTriangle);
            cmd->Draw((uint32)triangle.Size());

            cmd->BindGraphicsPipeline(pipeTriangles);
            cmd->BindVertexBuffer(0, vbQuad);
            cmd->Draw((uint32)quad.Size());

            cmd->EndRenderPass();
        }
        cmd->End();

        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    device->DestroyCommandBuffer(cmd);
    device->DestroyBuffer(vbPoints);
    device->DestroyBuffer(vbLines);
    device->DestroyBuffer(vbTriangle);
    device->DestroyBuffer(vbQuad);
    device->DestroyPipeline(pipePoints);
    device->DestroyPipeline(pipeLines);
    device->DestroyPipeline(pipeTriangles);
    device->DestroyShader(shader);
    NkDeviceFactory::Destroy(device);
    window.Close();
    return 0;
}
