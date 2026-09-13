#include "pch.hpp"

#include "ShaderLibrary.h"

#include "SpriteBuffer.h"

#include <iostream>

namespace
{
std::string ResolveContentPath(const std::string &basePath, const char *folder, const char *fileName)
{
    return basePath + "assets/Content/" + folder + "/" + fileName;
}

// Mirrors SdlBootstrap.cpp's LoadShader(): compiles shaderName for `stage`, either at runtime via
// shadercross (HLSL source under Shaders/) or from precompiled SPIRV (Shaders/Compiled/SPIRV/).
SDL_GPUShader *LoadShaderForPipeline(SDL_GPUDevice *device, const std::string &basePath,
                                     const char *shaderName, SDL_GPUShaderStage stage)
{
#ifdef SDL3_NEW_USE_SHADERCROSS
    const std::string shaderPath = ResolveContentPath(basePath, "Shaders", shaderName);
    size_t sourceSize = 0;
    char *sourceBytes = static_cast<char *>(SDL_LoadFile(shaderPath.c_str(), &sourceSize));
    if (sourceBytes == NULL || sourceSize == 0)
    {
        if (sourceBytes != NULL)
        {
            SDL_free(sourceBytes);
        }
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
        return NULL;
    }

    SDL_ShaderCross_GraphicsShaderMetadata *metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<const Uint8 *>(spirvBytecode), spirvSize, 0);
    if (metadata == NULL)
    {
        SDL_free(spirvBytecode);
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
    const std::string shaderPath = ResolveContentPath(basePath, "Shaders/Compiled/SPIRV", shaderName);
    size_t codeSize = 0;
    void *code = SDL_LoadFile(shaderPath.c_str(), &codeSize);
    if (code == NULL)
    {
        return NULL;
    }

    SDL_GPUShaderCreateInfo createInfo = {};
    createInfo.code = static_cast<const Uint8 *>(code);
    createInfo.code_size = codeSize;
    createInfo.entrypoint = "main";
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.stage = stage;
    createInfo.num_samplers = (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) ? 1 : 0;
    createInfo.num_uniform_buffers = (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) ? 1 : 0;
    createInfo.num_storage_buffers = 0;
    createInfo.num_storage_textures = 0;
    createInfo.props = 0;

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &createInfo);
    SDL_free(code);
    return shader;
#endif
}
}

ShaderLibrary::ShaderLibrary(SDL_GPUDevice *device, SDL_Window *window, std::string basePath)
    : m_device(device), m_window(window), m_basePath(std::move(basePath))
{
}

ShaderLibrary::~ShaderLibrary()
{
    ClearCache();
}

std::shared_ptr<ShaderLibrary::ShaderResource> ShaderLibrary::CreatePipeline(const std::string &name,
                                                                             const std::string &file)
{
    if (m_device == nullptr || m_window == nullptr)
    {
        return nullptr;
    }

    SDL_GPUShader *vertexShader = LoadShaderForPipeline(
        m_device, m_basePath,
#ifdef SDL3_NEW_USE_SHADERCROSS
        "TexturedQuad.vert.hlsl",
#else
        "TexturedQuad.vert.spv",
#endif
        SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader *fragmentShader = LoadShaderForPipeline(m_device, m_basePath, file.c_str(), SDL_GPU_SHADERSTAGE_FRAGMENT);

    if (vertexShader == nullptr || fragmentShader == nullptr)
    {
        std::cerr << "[shader] failed to compile '" << file << "' for shader name '" << name << "'\n";
        if (vertexShader != nullptr)
        {
            SDL_ReleaseGPUShader(m_device, vertexShader);
        }
        if (fragmentShader != nullptr)
        {
            SDL_ReleaseGPUShader(m_device, fragmentShader);
        }
        return nullptr;
    }

    SDL_GPUColorTargetDescription colorTargetDescription = {};
    colorTargetDescription.format = SDL_GetGPUSwapchainTextureFormat(m_device, m_window);
    colorTargetDescription.blend_state.enable_blend = true;
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

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
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

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &pipelineInfo);
    SDL_ReleaseGPUShader(m_device, vertexShader);
    SDL_ReleaseGPUShader(m_device, fragmentShader);

    if (pipeline == nullptr)
    {
        std::cerr << "[shader] failed to build pipeline for '" << file << "' (shader name '" << name << "'): "
                   << SDL_GetError() << "\n";
        return nullptr;
    }

    SDL_GPUDevice *device = m_device;
    return std::shared_ptr<ShaderResource>(new ShaderResource{ pipeline, device, name, file },
        [device](ShaderResource *ptr) {
            if (ptr != nullptr)
            {
                if (ptr->pipeline != nullptr)
                {
                    SDL_ReleaseGPUGraphicsPipeline(device, ptr->pipeline);
                }
                delete ptr;
            }
        });
}

std::shared_ptr<ShaderLibrary::ShaderResource> ShaderLibrary::LoadShader(const std::string &name,
                                                                         const std::string &file)
{
    auto cacheIt = m_cacheByFile.find(file);
    if (cacheIt != m_cacheByFile.end())
    {
        if (auto existing = cacheIt->second.weak.lock())
        {
            m_byName[name] = existing;
            return existing;
        }
    }

    auto created = CreatePipeline(name, file);
    if (created != nullptr)
    {
        m_cacheByFile[file].weak = created;
        m_byName[name] = created;
    }

    return created;
}

std::shared_ptr<ShaderLibrary::ShaderResource> ShaderLibrary::Find(const std::string &name) const
{
    const auto it = m_byName.find(name);
    if (it == m_byName.end())
    {
        return nullptr;
    }

    return it->second.lock();
}

void ShaderLibrary::ClearCache()
{
    m_cacheByFile.clear();
    m_byName.clear();
}
