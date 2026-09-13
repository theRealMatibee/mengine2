#include "pch.hpp"
#ifdef SDL3_NEW_USE_SHADERCROSS
#include <SDL3_shadercross/SDL_shadercross.h>
#endif

#include "SdlBootstrap.h"

#include "AppLoop.h"
#include "SpriteBuffer.h"
#include "TextureManager.h"
#include "CrashMonitor.h"

static const char *gBasePath = NULL;
static constexpr bool kUseSolidColorDebugShader = false;

static const char *GetContentPath(const char *folder, const char *fileName)
{
    static char path[1024];

    if (gBasePath == NULL)
    {
        return NULL;
    }

    SDL_snprintf(path, sizeof(path), "%sassets/Content/%s/%s", gBasePath, folder, fileName);
    return path;
}

static SDL_GPUShader *LoadShader(SDL_GPUDevice *device, const char *shaderName, SDL_GPUShaderStage stage,
                                 Uint32 samplerCount, Uint32 uniformBufferCount,
                                 Uint32 storageBufferCount, Uint32 storageTextureCount)
{
    CrashMonitor::Instance().RecordCheckpoint("SdlBootstrap::LoadShader: " + std::string(shaderName));

#ifdef SDL3_NEW_USE_SHADERCROSS
    const char *shaderPath = GetContentPath("Shaders", shaderName);
    size_t sourceSize = 0;
    char *sourceBytes = static_cast<char *>(SDL_LoadFile(shaderPath, &sourceSize));
    if (sourceBytes == NULL || sourceSize == 0)
    {
        if (sourceBytes != NULL)
        {
            SDL_free(sourceBytes);
        }

        CrashMonitor::Instance().RecordLastError("Failed to load shader source: " + std::string(shaderName));
        return NULL;
    }

    std::string sourceText(sourceBytes, sourceSize);
    SDL_free(sourceBytes);

    SDL_ShaderCross_HLSL_Info hlslInfo{};
    hlslInfo.source = sourceText.c_str();
    hlslInfo.entrypoint = "main";
    hlslInfo.include_dir = NULL;
    hlslInfo.defines = NULL;
    hlslInfo.shader_stage = (stage == SDL_GPU_SHADERSTAGE_VERTEX)
        ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX
        : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
    hlslInfo.props = 0;

    size_t spirvSize = 0;
    void *spirvBytecode = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvSize);
    if (spirvBytecode == NULL || spirvSize == 0)
    {
        if (spirvBytecode != NULL)
        {
            SDL_free(spirvBytecode);
        }
        CrashMonitor::Instance().RecordLastError("Failed to compile SPIR-V from HLSL: " + std::string(shaderName));
        return NULL;
    }

    SDL_ShaderCross_GraphicsShaderMetadata *metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<const Uint8 *>(spirvBytecode), spirvSize, 0);
    if (metadata == NULL)
    {
        SDL_free(spirvBytecode);
        CrashMonitor::Instance().RecordLastError("Failed to reflect graphics SPIR-V metadata: " + std::string(shaderName));
        return NULL;
    }

    SDL_ShaderCross_SPIRV_Info spirvInfo{};
    spirvInfo.bytecode = static_cast<const Uint8 *>(spirvBytecode);
    spirvInfo.bytecode_size = spirvSize;
    spirvInfo.entrypoint = "main";
    spirvInfo.shader_stage = (stage == SDL_GPU_SHADERSTAGE_VERTEX)
        ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX
        : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
    spirvInfo.props = 0;

    SDL_GPUShader *shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
        device,
        &spirvInfo,
        &metadata->resource_info,
        0);

    SDL_free(metadata);
    SDL_free(spirvBytecode);
    return shader;
#else
    size_t codeSize = 0;
    const char *shaderPath = GetContentPath("Shaders/Compiled/SPIRV", shaderName);
    void *code = SDL_LoadFile(shaderPath, &codeSize);
    if (code == NULL)
    {
        CrashMonitor::Instance().RecordLastError("Failed to load SPIR-V shader file: " + std::string(shaderPath));
        return NULL;
    }

    SDL_GPUShaderCreateInfo createInfo = {};
    createInfo.code = static_cast<const Uint8 *>(code);
    createInfo.code_size = codeSize;
    createInfo.entrypoint = "main";
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.stage = stage;
    createInfo.num_samplers = samplerCount;
    createInfo.num_uniform_buffers = uniformBufferCount;
    createInfo.num_storage_buffers = storageBufferCount;
    createInfo.num_storage_textures = storageTextureCount;
    createInfo.props = 0;

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &createInfo);
    SDL_free(code);

    if (shader == NULL)
    {
        CrashMonitor::Instance().RecordLastError("Failed to create GPU shader");
        return NULL;
    }

    return shader;
#endif
}

bool TryEnableFullscreen(SDL_Window *window)
{
    CrashMonitor::Instance().RecordCheckpoint("SdlBootstrap::TryEnableFullscreen");

    if (window == NULL)
    {
        return false;
    }

    const SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);
    if (displayID == 0)
    {
        std::string err = "[display] could not resolve display for window, staying windowed: " + std::string(SDL_GetError());
        CrashMonitor::Instance().RecordLastError(err);
        SDL_Log("%s", err.c_str());
        return false;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    // SDL's own recommended way to pick an exclusive fullscreen mode: scan the display's
    // supported modes for the closest one >= the requested size (we don't need/want the
    // display's native max resolution, just something close to our current window size).
    CrashMonitor::Instance().RecordCheckpoint("SdlBootstrap::Looking for closest mode");
    SDL_DisplayMode closestMode;
    if (SDL_GetClosestFullscreenDisplayMode(displayID, windowWidth, windowHeight, 0.0f, true, &closestMode))
    {
        if (!SDL_SetWindowFullscreenMode(window, &closestMode))
        {
            std::string err = "[display] failed to set fullscreen mode " + std::to_string(closestMode.w) + "x" + std::to_string(closestMode.h) + ": " + SDL_GetError();
            CrashMonitor::Instance().RecordLastError(err);
            SDL_Log("%s", err.c_str());
        }
    }
    else
    {
        // No exclusive mode close to the requested size (or the platform doesn't expose mode
        // switching, e.g. most Wayland compositors) - NULL means borderless fullscreen desktop
        // mode, which is universally supported.
        std::string err = "[display] no exclusive fullscreen mode near " + std::to_string(windowWidth) + "x" + std::to_string(windowHeight) + ", using borderless desktop mode";
        CrashMonitor::Instance().RecordLastError(err);
        SDL_Log("%s", err.c_str());
        SDL_SetWindowFullscreenMode(window, NULL);
    }

    if (!SDL_SetWindowFullscreen(window, true))
    {
        std::string err = "[display] SDL_SetWindowFullscreen failed, staying windowed: " + std::string(SDL_GetError());
        CrashMonitor::Instance().RecordLastError(err);
        SDL_Log("%s", err.c_str());
        SDL_SetWindowFullscreen(window, false);
        return false;
    }

    SDL_SyncWindow(window);

    // Some window managers accept the request but don't actually apply it - verify before
    // reporting success.
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0)
    {
        std::string err = "[display] window manager did not honor fullscreen request, staying windowed";
        CrashMonitor::Instance().RecordLastError(err);
        SDL_Log("%s", err.c_str());
        SDL_SetWindowFullscreen(window, false);
        return false;
    }

    SDL_Log("[display] fullscreen enabled");
    return true;
}

static bool InitializeGpuOnly(SdlBootstrapContext &context, bool fullscreen, const SdlWindowConfig &windowConfig)
{
    CrashMonitor::Instance().RecordCheckpoint("SdlBootstrap::InitializeGpuOnly");

    gBasePath = SDL_GetBasePath();
    if (gBasePath == NULL)
    {
        CrashMonitor::Instance().RecordLastError("[display] failed to get base path");
        return false;
    }

    SDL_Window *window = SDL_CreateWindow(windowConfig.title, windowConfig.width, windowConfig.height,
                                          windowConfig.resizable ? SDL_WINDOW_RESIZABLE : 0);
    if (window == NULL)
    {
        CrashMonitor::Instance().RecordLastError("[display] failed to create SDL window");
        gBasePath = NULL;
        return false;
    }
    SDL_SetWindowMinimumSize(window, windowConfig.minimumWidth, windowConfig.minimumHeight);

    if (fullscreen)
    {
        TryEnableFullscreen(window); // best-effort; window stays/reverts to windowed on any failure
    }

    CrashMonitor::Instance().RecordCheckpoint("SdlBootstrap::InitializeGpuOnly - before creating GPU device");
    SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    if (device == NULL)
    {
        SDL_DestroyWindow(window);
        CrashMonitor::Instance().RecordLastError("[display] failed to create GPU device");
        gBasePath = NULL;
        return false;
    }

    CrashMonitor::Instance().RecordCheckpoint("SdlBootstrap::InitializeGpuOnly - before claiming window for GPU device");
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        CrashMonitor::Instance().RecordLastError("[display] failed to claim window for GPU device");
        return false;
    }

    SDL_GPUShader *vertexShader = LoadShader(
        device,
#ifdef SDL3_NEW_USE_SHADERCROSS
        "TexturedQuad.vert.hlsl",
#else
        "TexturedQuad.vert.spv",
#endif
        SDL_GPU_SHADERSTAGE_VERTEX,
        0,
        0,
        0,
        0);
    const bool useSolidShader = kUseSolidColorDebugShader;
    SDL_GPUShader *fragmentShader = LoadShader(
        device,
        useSolidShader ?
#ifdef SDL3_NEW_USE_SHADERCROSS
            "SolidColor.frag.hlsl" : "TexturedQuad.frag.hlsl",
#else
            "SolidColor.frag.spv" : "TexturedQuad.frag.spv",
#endif
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        useSolidShader ? 0 : 1,
        0,
        0,
        0);
    if (vertexShader == NULL || fragmentShader == NULL)
    {
        SDL_ReleaseGPUShader(device, vertexShader);
        SDL_ReleaseGPUShader(device, fragmentShader);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        gBasePath = NULL;
        return false;
    }

    CrashMonitor::Instance().RecordCheckpoint("SdlBootstrap::InitializeGpuOnly - before setting up graphics pipeline");

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};

    SDL_GPUColorTargetDescription colorTargetDescription = {};
    colorTargetDescription.format = SDL_GetGPUSwapchainTextureFormat(device, window);
    colorTargetDescription.blend_state.enable_blend = !useSolidShader;
    colorTargetDescription.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorTargetDescription.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTargetDescription.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    colorTargetDescription.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    colorTargetDescription.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTargetDescription.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    SDL_GPUVertexBufferDescription vertexBufferDescription = {};
    vertexBufferDescription.slot = 0;
    vertexBufferDescription.pitch = sizeof(SpriteBuffer::Vertex);
    vertexBufferDescription.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBufferDescription.instance_step_rate = 0;

    SDL_GPUVertexAttribute vertexAttributes[3] = {};
    vertexAttributes[0].location = 0;
    vertexAttributes[0].buffer_slot = 0;
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[0].offset = static_cast<Uint32>(offsetof(SpriteBuffer::Vertex, x));
    vertexAttributes[1].location = 1;
    vertexAttributes[1].buffer_slot = 0;
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttributes[1].offset = static_cast<Uint32>(offsetof(SpriteBuffer::Vertex, u));
    vertexAttributes[2].location = 2;
    vertexAttributes[2].buffer_slot = 0;
    vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    vertexAttributes[2].offset = static_cast<Uint32>(offsetof(SpriteBuffer::Vertex, r));

    pipelineInfo.target_info.num_color_targets = 1;
    pipelineInfo.target_info.color_target_descriptions = &colorTargetDescription;
    pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBufferDescription;
    pipelineInfo.vertex_input_state.num_vertex_attributes = 3;
    pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipelineInfo.vertex_shader = vertexShader;
    pipelineInfo.fragment_shader = fragmentShader;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    if (pipeline == NULL)
    {
        CrashMonitor::Instance().RecordLastError("[display] failed to create graphics pipeline");
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        gBasePath = NULL;
        return false;
    }

    context.window = window;
    context.device = device;
    context.pipeline = pipeline;
    context.spriteBuffer = std::make_unique<SpriteBuffer>(device);
    context.textureManager = std::make_unique<TextureManager>(device, std::string(gBasePath));
    context.shaderLibrary = std::make_unique<ShaderLibrary>(device, window, std::string(gBasePath));
    context.basePath = gBasePath;
    context.useSolidColorDebugShader = useSolidShader;
    return true;
}

bool InitializeSdlAndGpu(SdlBootstrapContext &context, bool fullscreen, const SdlWindowConfig &windowConfig)
{
    CrashMonitor::Instance().RecordCheckpoint("SdlBootstrap::SDL init");

    SetVirtualResolution(windowConfig.virtualWidth, windowConfig.virtualHeight);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
    {
        CrashMonitor::Instance().RecordLastError("SDL initialization failed");
        return false;
    }
 
    context.sdlInitialized = true;

#ifdef SDL3_NEW_USE_SHADERCROSS
    CrashMonitor::Instance().RecordCheckpoint("SdlBootstrap::InitializeSdlAndGpu - before shader cross initialization");
    if (!SDL_ShaderCross_Init())
    {
        CrashMonitor::Instance().RecordLastError("ShaderCross initialization failed");
        SDL_Quit();
        context.sdlInitialized = false;
        return false;
    }
    context.shaderCrossInitialized = true;
#endif
    
    CrashMonitor::Instance().RecordCheckpoint("SdlBootstrap::InitializeSdlAndGpu - before GPU initialization");
    if (!InitializeGpuOnly(context, fullscreen, windowConfig))
    {
        CrashMonitor::Instance().RecordLastError("GPU initialization failed");
#ifdef SDL3_NEW_USE_SHADERCROSS
        if (context.shaderCrossInitialized)
        {
            SDL_ShaderCross_Quit();
            context.shaderCrossInitialized = false;
        }
#endif
        SDL_Quit();
        context.sdlInitialized = false;
        return false;
    }

    return true;
}

void ShutdownSdlAndGpu(SdlBootstrapContext &context)
{
    // CrashMonitor is already closed. No point adding reporting here.

    if (context.device != nullptr)
    {
        SDL_WaitForGPUIdle(context.device);
    }

    if (context.device != nullptr && context.pipeline != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(context.device, context.pipeline);
        context.pipeline = nullptr;
    }

    context.spriteBuffer.reset();
    context.textureManager.reset();
    context.shaderLibrary.reset();

    if (context.device != nullptr && context.window != nullptr)
    {
        SDL_ReleaseWindowFromGPUDevice(context.device, context.window);
    }
    if (context.device != nullptr)
    {
        SDL_DestroyGPUDevice(context.device);
        context.device = nullptr;
    }
    if (context.window != nullptr)
    {
        SDL_DestroyWindow(context.window);
        context.window = nullptr;
    }

    gBasePath = NULL;
    context.basePath = nullptr;

#ifdef SDL3_NEW_USE_SHADERCROSS
    if (context.shaderCrossInitialized)
    {
        SDL_ShaderCross_Quit();
        context.shaderCrossInitialized = false;
    }
#endif

    if (context.sdlInitialized)
    {
        SDL_Quit();
        context.sdlInitialized = false;
    }
}
