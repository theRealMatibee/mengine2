#include "SpriteBuffer.h"

#include <algorithm>

namespace
{
// The canonical index pattern for one quad. Baked vertex offsets are added at submission time.
static constexpr Uint32 kIndexPattern[6] = { 0, 1, 2, 0, 2, 3 };
}

int SpriteBuffer::s_fakeGammaBrightness = 0;

void SpriteBuffer::SetFakeGammaBrightness(int level)
{
    if (level < 0)
    {
        level = 0;
    }
    else if (level > 100)
    {
        level = 100;
    }
    s_fakeGammaBrightness = level;
}

int SpriteBuffer::GetFakeGammaBrightness()
{
    return s_fakeGammaBrightness;
}

float SpriteBuffer::GetFakeGammaBrightnessNormalized()
{
    return static_cast<float>(s_fakeGammaBrightness) / 100.0f;
}

float SpriteBuffer::s_glowRadiusPixels = 6.0f;

void SpriteBuffer::SetGlowRadiusPixels(float radiusPixels)
{
    s_glowRadiusPixels = std::max(0.0f, radiusPixels);
}

float SpriteBuffer::GetGlowRadiusPixels()
{
    return s_glowRadiusPixels;
}

SpriteBuffer::SpriteBuffer(SDL_GPUDevice *device)
    : m_device(device)
{
}

SpriteBuffer::~SpriteBuffer()
{
    for (DeferredTransferBatch &batch : m_deferredTransfers)
    {
        for (SDL_GPUTransferBuffer *transfer : batch.transfers)
        {
            if (transfer != nullptr)
            {
                SDL_ReleaseGPUTransferBuffer(m_device, transfer);
            }
        }
    }
    m_deferredTransfers.clear();

    for (SDL_GPUTransferBuffer *transfer : m_pendingUploadTransfers)
    {
        if (transfer != nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(m_device, transfer);
        }
    }
    m_pendingUploadTransfers.clear();

    if (m_vertexBuffer != nullptr)
    {
        SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
    }

    if (m_indexBuffer != nullptr)
    {
        SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);
    }
}

void SpriteBuffer::Begin()
{
    m_cpuVertices.clear();
    m_cpuIndices.clear();
    m_textureSpans.clear();
}

void SpriteBuffer::AddSprite(const Vertex vertices[4], SDL_GPUTexture *texture, SDL_GPUSampler *sampler,
                             SDL_GPUGraphicsPipeline *pipeline)
{
    const Uint32 baseVertex = static_cast<Uint32>(m_cpuVertices.size());

    for (int i = 0; i < 4; ++i)
    {
        m_cpuVertices.push_back(vertices[i]);
    }

    const Uint32 firstIndexForThisSprite = static_cast<Uint32>(m_cpuIndices.size());

    for (int i = 0; i < 6; ++i)
    {
        m_cpuIndices.push_back(baseVertex + kIndexPattern[i]);
    }

    // Extend the current run if the texture and pipeline match; otherwise open a new span.
    if (!m_textureSpans.empty() && m_textureSpans.back().texture == texture &&
        m_textureSpans.back().pipeline == pipeline)
    {
        m_textureSpans.back().indexCount += 6;
    }
    else
    {
        TextureSpan span{};
        span.texture    = texture;
        span.sampler    = sampler;
        span.pipeline   = pipeline;
        span.firstIndex = firstIndexForThisSprite;
        span.indexCount = 6;
        m_textureSpans.push_back(span);
    }
}



bool SpriteBuffer::EnsureCapacity(Uint32 neededSprites)
{
    if (neededSprites <= m_gpuCapacitySprites)
    {
        return true;
    }

    SDL_GPUBuffer *newVertexBuffer = nullptr;
    SDL_GPUBuffer *newIndexBuffer = nullptr;

    SDL_GPUBufferCreateInfo vertexInfo{};
    vertexInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertexInfo.size  = neededSprites * 4 * sizeof(Vertex);
    newVertexBuffer   = SDL_CreateGPUBuffer(m_device, &vertexInfo);

    SDL_GPUBufferCreateInfo indexInfo{};
    indexInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    indexInfo.size  = neededSprites * 6 * sizeof(Uint32);
    newIndexBuffer   = SDL_CreateGPUBuffer(m_device, &indexInfo);

    if (newVertexBuffer == nullptr || newIndexBuffer == nullptr)
    {
        if (newVertexBuffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(m_device, newVertexBuffer);
        }
        if (newIndexBuffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(m_device, newIndexBuffer);
        }
        return false;
    }

    if (m_vertexBuffer != nullptr)
    {
        SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
    }

    if (m_indexBuffer != nullptr)
    {
        SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);
    }

    m_vertexBuffer = newVertexBuffer;
    m_indexBuffer = newIndexBuffer;
    m_gpuCapacitySprites = neededSprites;
    return true;
}

void SpriteBuffer::Upload(SDL_GPUCommandBuffer *cmd)
{
    if (cmd == nullptr || m_cpuVertices.empty())
    {
        return;
    }

    const Uint32 spriteCount    = static_cast<Uint32>(m_cpuVertices.size()) / 4;
    const Uint32 vertexDataSize = static_cast<Uint32>(m_cpuVertices.size() * sizeof(Vertex));
    const Uint32 indexDataSize  = static_cast<Uint32>(m_cpuIndices.size()  * sizeof(Uint32));
    const Uint32 totalSize      = vertexDataSize + indexDataSize;

    if (!EnsureCapacity(spriteCount))
    {
        return;
    }

    // Pack vertices then indices into one transfer buffer to minimise API calls.
    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size  = totalSize;

    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferInfo);
    if (transferBuffer == nullptr)
    {
        return;
    }

    Uint8 *mapped = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(m_device, transferBuffer, false));
    if (mapped == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
        return;
    }

    SDL_memcpy(mapped,                   m_cpuVertices.data(), vertexDataSize);
    SDL_memcpy(mapped + vertexDataSize,  m_cpuIndices.data(),  indexDataSize);
    SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation vertexSrc{};
    vertexSrc.transfer_buffer = transferBuffer;
    vertexSrc.offset          = 0;

    SDL_GPUBufferRegion vertexDst{};
    vertexDst.buffer = m_vertexBuffer;
    vertexDst.offset = 0;
    vertexDst.size   = vertexDataSize;
    SDL_UploadToGPUBuffer(copyPass, &vertexSrc, &vertexDst, false);

    SDL_GPUTransferBufferLocation indexSrc{};
    indexSrc.transfer_buffer = transferBuffer;
    indexSrc.offset          = vertexDataSize;  // indices start after vertices in the transfer buffer

    SDL_GPUBufferRegion indexDst{};
    indexDst.buffer = m_indexBuffer;
    indexDst.offset = 0;
    indexDst.size   = indexDataSize;
    SDL_UploadToGPUBuffer(copyPass, &indexSrc, &indexDst, false);

    SDL_EndGPUCopyPass(copyPass);

    // Keep transfer buffer alive until the command buffer is submitted.
    m_pendingUploadTransfers.push_back(transferBuffer);
}

void SpriteBuffer::OnUploadSubmitComplete()
{
    if (!m_pendingUploadTransfers.empty())
    {
        DeferredTransferBatch batch{};
        batch.framesRemaining = 4;
        batch.transfers = std::move(m_pendingUploadTransfers);
        m_deferredTransfers.push_back(std::move(batch));
        m_pendingUploadTransfers.clear();
    }

    for (auto it = m_deferredTransfers.begin(); it != m_deferredTransfers.end(); )
    {
        it->framesRemaining -= 1;
        if (it->framesRemaining <= 0)
        {
            for (SDL_GPUTransferBuffer *transfer : it->transfers)
            {
                if (transfer != nullptr)
                {
                    SDL_ReleaseGPUTransferBuffer(m_device, transfer);
                }
            }
            it = m_deferredTransfers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SpriteBuffer::Draw(SDL_GPURenderPass *renderPass, SDL_GPUGraphicsPipeline *defaultPipeline) const
{
    if (renderPass == nullptr || defaultPipeline == nullptr ||
        m_textureSpans.empty() || m_vertexBuffer == nullptr || m_indexBuffer == nullptr)
    {
        return;
    }

    // The single VRAM vertex and index buffer holds all sprites for this frame.
    SDL_GPUBufferBinding vertexBinding{};
    vertexBinding.buffer = m_vertexBuffer;
    vertexBinding.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

    SDL_GPUBufferBinding indexBinding{};
    indexBinding.buffer = m_indexBuffer;
    indexBinding.offset = 0;
    SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    // One draw call per contiguous group of sprites that share a texture and pipeline.
    SDL_GPUGraphicsPipeline *boundPipeline = nullptr;
    for (const TextureSpan &span : m_textureSpans)
    {
        SDL_GPUGraphicsPipeline *spanPipeline = (span.pipeline != nullptr) ? span.pipeline : defaultPipeline;
        if (spanPipeline != boundPipeline)
        {
            SDL_BindGPUGraphicsPipeline(renderPass, spanPipeline);
            boundPipeline = spanPipeline;
        }

        SDL_GPUTextureSamplerBinding samplerBinding{};
        samplerBinding.texture = span.texture;
        samplerBinding.sampler = span.sampler;
        SDL_BindGPUFragmentSamplers(renderPass, 0, &samplerBinding, 1);

        SDL_DrawGPUIndexedPrimitives(renderPass, span.indexCount, 1, span.firstIndex, 0, 0);
    }
}

void SpriteBuffer::DrawNoSampler(SDL_GPURenderPass *renderPass, SDL_GPUGraphicsPipeline *defaultPipeline) const
{
    if (renderPass == nullptr || defaultPipeline == nullptr ||
        m_textureSpans.empty() || m_vertexBuffer == nullptr || m_indexBuffer == nullptr)
    {
        return;
    }

    SDL_GPUBufferBinding vertexBinding{};
    vertexBinding.buffer = m_vertexBuffer;
    vertexBinding.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

    SDL_GPUBufferBinding indexBinding{};
    indexBinding.buffer = m_indexBuffer;
    indexBinding.offset = 0;
    SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_GPUGraphicsPipeline *boundPipeline = nullptr;
    for (const TextureSpan &span : m_textureSpans)
    {
        SDL_GPUGraphicsPipeline *spanPipeline = (span.pipeline != nullptr) ? span.pipeline : defaultPipeline;
        if (spanPipeline != boundPipeline)
        {
            SDL_BindGPUGraphicsPipeline(renderPass, spanPipeline);
            boundPipeline = spanPipeline;
        }

        SDL_DrawGPUIndexedPrimitives(renderPass, span.indexCount, 1, span.firstIndex, 0, 0);
    }
}
