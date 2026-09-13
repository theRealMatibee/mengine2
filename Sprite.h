#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "SpriteBuffer.h"
#include "TextureManager.h"
#include "ShaderLibrary.h"

#include <memory>
#include <string>

class Sprite
{
public:
    explicit Sprite(TextureManager *textureManager);
    ~Sprite();

    Sprite(const Sprite &) = delete;
    Sprite &operator=(const Sprite &) = delete;

    bool LoadTexture(const std::string &relativePath);
    bool SetRectNDCVertices(float centerX, float centerY, float width, float height);
    bool SetRectNDC(float centerX, float centerY, float width, float height);
    bool SetQuadWorldVertices(float x0, float y0,
                              float x1, float y1,
                              float x2, float y2,
                              float x3, float y3);
    bool SetQuadNDCVertices(float x0, float y0,
                            float x1, float y1,
                            float x2, float y2,
                            float x3, float y3);
    bool SetQuadNDC(float x0, float y0,
                    float x1, float y1,
                    float x2, float y2,
                    float x3, float y3);
    bool SetTextureRectNormalized(float u0, float v0, float u1, float v1);
    bool SetTextureRectPixels(int x, int y, int width, int height);
    int GetTextureWidth() const;
    int GetTextureHeight() const;
    void SetRotationRadians(float rotationRadians);
    float GetRotationRadians() const;
    void SetFlipHorizontal(bool enabled);
    void SetFlipVertical(bool enabled);
    bool GetFlipHorizontal() const;
    bool GetFlipVertical() const;
    bool SetColor(float r, float g, float b, float a);
    bool SetColor(const SDL_FColor &color);
    // Store world-space corner positions (shader applies the camera transform).
    bool SetQuadWorld(float x0, float y0,
                      float x1, float y1,
                      float x2, float y2,
                      float x3, float y3);

    // Override the pixel shader used when batched via RenderToBuffer(). The pipeline must
    // share the same vertex shader/layout as the rest of the sprites. Pass nullptr (default)
    // to use the SpriteBuffer's default pipeline.
    void SetShaderOverride(SDL_GPUGraphicsPipeline *pipeline);
    // Ref-counted variant used by ShaderLibrary-loaded shaders: keeps the pipeline alive for as
    // long as this sprite holds it. Pass nullptr to fall back to the SpriteBuffer default.
    void SetShaderOverride(std::shared_ptr<ShaderLibrary::ShaderResource> shaderResource);

    // Direct single-sprite draw (binds its own VRAM buffers).
    void Draw(SDL_GPURenderPass *renderPass, SDL_GPUGraphicsPipeline *pipeline) const;

    // Submit this sprite's geometry and texture to a SpriteBuffer for batched rendering.
    void RenderToBuffer(SpriteBuffer *buffer) const;

private:
    bool UploadBuffer(SDL_GPUBuffer *buffer, const void *data, Uint32 dataSize) const;
    bool EnsureGpuBuffers();
    void ApplyTextureCoordinates();

    TextureManager *m_textureManager = nullptr;
    SDL_GPUDevice  *m_device         = nullptr;

    SDL_GPUBuffer  *m_vertexBuffer = nullptr;
    SDL_GPUBuffer  *m_indexBuffer  = nullptr;

    // CPU copy of vertex data — used both to upload to VRAM and to feed RenderToBuffer.
    SpriteBuffer::Vertex m_vertices[4] = {};
    SDL_FColor m_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float m_u0 = 0.0f;
    float m_v0 = 0.0f;
    float m_u1 = 1.0f;
    float m_v1 = 1.0f;
    float m_rotationRadians = 0.0f;
    bool m_flipHorizontal = false;
    bool m_flipVertical = false;

    SDL_GPUGraphicsPipeline *m_shaderOverride = nullptr;
    std::shared_ptr<ShaderLibrary::ShaderResource> m_shaderResource;

    std::shared_ptr<TextureManager::TextureResource> m_texture;
};
