#include "Sprite.h"

namespace
{
static constexpr Uint16 kQuadIndices[6] = { 0, 1, 2, 0, 2, 3 };

using Vertex = SpriteBuffer::Vertex;
}

void Sprite::ApplyTextureCoordinates()
{
    float u0 = m_u0;
    float u1 = m_u1;
    float v0 = m_v0;
    float v1 = m_v1;

    if (m_flipHorizontal)
    {
        std::swap(u0, u1);
    }
    if (m_flipVertical)
    {
        std::swap(v0, v1);
    }

    m_vertices[0].u = u0;
    m_vertices[0].v = v0;
    m_vertices[1].u = u1;
    m_vertices[1].v = v0;
    m_vertices[2].u = u1;
    m_vertices[2].v = v1;
    m_vertices[3].u = u0;
    m_vertices[3].v = v1;
}

Sprite::Sprite(TextureManager *textureManager)
    : m_textureManager(textureManager),
      m_device(textureManager != nullptr ? textureManager->GetDevice() : nullptr)
{
}

Sprite::~Sprite()
{
    if (m_vertexBuffer != nullptr)
    {
        SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
        m_vertexBuffer = nullptr;
    }

    if (m_indexBuffer != nullptr)
    {
        SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);
        m_indexBuffer = nullptr;
    }
}

bool Sprite::LoadTexture(const std::string &relativePath)
{
    if (m_textureManager == nullptr)
    {
        return false;
    }

    m_texture = m_textureManager->LoadTexture2D(relativePath);
    if (m_texture == nullptr) {
        return false;
    }

    m_u0 = 0.0f;
    m_v0 = 0.0f;
    m_u1 = 1.0f;
    m_v1 = 1.0f;
    ApplyTextureCoordinates();

    return true;
}

int Sprite::GetTextureWidth() const
{
    return m_texture != nullptr ? m_texture->width : 0;
}

int Sprite::GetTextureHeight() const
{
    return m_texture != nullptr ? m_texture->height : 0;
}

bool Sprite::UploadBuffer(SDL_GPUBuffer *buffer, const void *data, Uint32 dataSize) const
{
    SDL_GPUTransferBufferCreateInfo transferCreateInfo{};
    transferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferCreateInfo.size = dataSize;

    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(m_device, &transferCreateInfo);
    if (transferBuffer == nullptr)
    {
        return false;
    }

    void *mapped = SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
    if (mapped == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
        return false;
    }

    SDL_memcpy(mapped, data, dataSize);
    SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    if (cmd == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
        return false;
    }

    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = transferBuffer;
    source.offset = 0;

    SDL_GPUBufferRegion destination{};
    destination.buffer = buffer;
    destination.offset = 0;
    destination.size = dataSize;

    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
    return true;
}

bool Sprite::SetRectNDC(float centerX, float centerY, float width, float height)
{
    if (!SetRectNDCVertices(centerX, centerY, width, height))
    {
        return false;
    }

    if (!EnsureGpuBuffers())
    {
        return false;
    }

    return UploadBuffer(m_vertexBuffer, m_vertices, sizeof(m_vertices));
}

bool Sprite::SetQuadWorldVertices(float x0, float y0,
                                  float x1, float y1,
                                  float x2, float y2,
                                  float x3, float y3)
{
    return SetQuadNDCVertices(x0, y0, x1, y1, x2, y2, x3, y3);
}

bool Sprite::SetRectNDCVertices(float centerX, float centerY, float width, float height)
{
    if (m_device == nullptr)
    {
        return false;
    }

    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;

    const float left = centerX - halfWidth;
    const float right = centerX + halfWidth;
    const float top = centerY + halfHeight;
    const float bottom = centerY - halfHeight;

    return SetQuadNDCVertices(left, bottom,
                              right, bottom,
                              right, top,
                              left, top);
}

bool Sprite::SetQuadNDCVertices(float x0, float y0,
                                float x1, float y1,
                                float x2, float y2,
                                float x3, float y3)
{
    if (m_device == nullptr)
    {
        return false;
    }

    m_vertices[0] = { x0, y0, 0.0f, m_u0, m_v0, m_color.r, m_color.g, m_color.b, m_color.a };
    m_vertices[1] = { x1, y1, 0.0f, m_u1, m_v0, m_color.r, m_color.g, m_color.b, m_color.a };
    m_vertices[2] = { x2, y2, 0.0f, m_u1, m_v1, m_color.r, m_color.g, m_color.b, m_color.a };
    m_vertices[3] = { x3, y3, 0.0f, m_u0, m_v1, m_color.r, m_color.g, m_color.b, m_color.a };
    ApplyTextureCoordinates();

    return true;
}

bool Sprite::SetTextureRectNormalized(float u0, float v0, float u1, float v1)
{
    m_u0 = u0;
    m_v0 = v0;
    m_u1 = u1;
    m_v1 = v1;

    ApplyTextureCoordinates();

    if (m_vertexBuffer != nullptr)
    {
        return UploadBuffer(m_vertexBuffer, m_vertices, sizeof(m_vertices));
    }

    return true;
}

bool Sprite::SetTextureRectPixels(int x, int y, int width, int height)
{
    if (m_texture == nullptr || m_texture->width <= 0 || m_texture->height <= 0)
    {
        return false;
    }

    const float invWidth = 1.0f / static_cast<float>(m_texture->width);
    const float invHeight = 1.0f / static_cast<float>(m_texture->height);

    const float u0 = static_cast<float>(x) * invWidth;
    const float v0 = static_cast<float>(y) * invHeight;
    const float u1 = static_cast<float>(x + width) * invWidth;
    const float v1 = static_cast<float>(y + height) * invHeight;

    return SetTextureRectNormalized(u0, v0, u1, v1);
}

bool Sprite::SetQuadNDC(float x0, float y0,
                        float x1, float y1,
                        float x2, float y2,
                        float x3, float y3)
{
    if (!SetQuadNDCVertices(x0, y0, x1, y1, x2, y2, x3, y3))
    {
        return false;
    }

    if (!EnsureGpuBuffers())
    {
        return false;
    }

    return UploadBuffer(m_vertexBuffer, m_vertices, sizeof(m_vertices));
}

void Sprite::SetRotationRadians(float rotationRadians)
{
    m_rotationRadians = rotationRadians;
}

float Sprite::GetRotationRadians() const
{
    return m_rotationRadians;
}

void Sprite::SetFlipHorizontal(bool enabled)
{
    m_flipHorizontal = enabled;
    ApplyTextureCoordinates();
}

void Sprite::SetFlipVertical(bool enabled)
{
    m_flipVertical = enabled;
    ApplyTextureCoordinates();
}

bool Sprite::GetFlipHorizontal() const
{
    return m_flipHorizontal;
}

bool Sprite::GetFlipVertical() const
{
    return m_flipVertical;
}

bool Sprite::SetColor(float r, float g, float b, float a)
{
    SDL_FColor color = { r, g, b, a };
    return SetColor(color);
}

bool Sprite::SetColor(const SDL_FColor &color)
{
    m_color = color;

    for (SpriteBuffer::Vertex &vertex : m_vertices)
    {
        vertex.r = m_color.r;
        vertex.g = m_color.g;
        vertex.b = m_color.b;
        vertex.a = m_color.a;
    }

    if (m_vertexBuffer != nullptr)
    {
        return UploadBuffer(m_vertexBuffer, m_vertices, sizeof(m_vertices));
    }

    return true;
}

bool Sprite::EnsureGpuBuffers()
{
    if (m_device == nullptr)
    {
        return false;
    }

    if (m_vertexBuffer == nullptr)
    {
        SDL_GPUBufferCreateInfo vertexBufferInfo{};
        vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vertexBufferInfo.size = sizeof(m_vertices);
        m_vertexBuffer = SDL_CreateGPUBuffer(m_device, &vertexBufferInfo);
        if (m_vertexBuffer == nullptr)
        {
            return false;
        }
    }

    if (m_indexBuffer == nullptr)
    {
        SDL_GPUBufferCreateInfo indexBufferInfo{};
        indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        indexBufferInfo.size = sizeof(kQuadIndices);
        m_indexBuffer = SDL_CreateGPUBuffer(m_device, &indexBufferInfo);
        if (m_indexBuffer == nullptr)
        {
            return false;
        }

        if (!UploadBuffer(m_indexBuffer, kQuadIndices, sizeof(kQuadIndices)))
        {
            return false;
        }
    }

    return true;
}

bool Sprite::SetQuadWorld(float x0, float y0,
                          float x1, float y1,
                          float x2, float y2,
                          float x3, float y3)
{
    // Vertex layout is identical; interpretation (world vs NDC) is up to the shader.
    return SetQuadNDC(x0, y0, x1, y1, x2, y2, x3, y3);
}

void Sprite::RenderToBuffer(SpriteBuffer *buffer) const
{
    if (buffer == nullptr || m_texture == nullptr)
    {
        return;
    }

    buffer->AddSprite(m_vertices, m_texture->texture, m_texture->sampler, m_shaderOverride);
}

void Sprite::SetShaderOverride(SDL_GPUGraphicsPipeline *pipeline)
{
    m_shaderResource.reset();
    m_shaderOverride = pipeline;
}

void Sprite::SetShaderOverride(std::shared_ptr<ShaderLibrary::ShaderResource> shaderResource)
{
    m_shaderOverride = (shaderResource != nullptr) ? shaderResource->pipeline : nullptr;
    m_shaderResource = std::move(shaderResource);
}

void Sprite::Draw(SDL_GPURenderPass *renderPass, SDL_GPUGraphicsPipeline *pipeline) const
{
    if (renderPass == nullptr) {
        return;
    }
    if (pipeline == nullptr) {
        return;
    }
    if (m_texture == nullptr) {
        return;
    }
    if (m_vertexBuffer == nullptr) {
        return;
    }
    if (m_indexBuffer == nullptr) {
        return;
    }

    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

    SDL_GPUBufferBinding vertexBinding{};
    vertexBinding.buffer = m_vertexBuffer;
    vertexBinding.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

    SDL_GPUBufferBinding indexBinding{};
    indexBinding.buffer = m_indexBuffer;
    indexBinding.offset = 0;
    SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    SDL_GPUTextureSamplerBinding samplerBinding{};
    samplerBinding.texture = m_texture->texture;
    samplerBinding.sampler = m_texture->sampler;
    SDL_BindGPUFragmentSamplers(renderPass, 0, &samplerBinding, 1);

    SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);
}
