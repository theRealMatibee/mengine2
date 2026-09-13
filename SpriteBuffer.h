#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <deque>
#include <vector>

// Accumulates sprite geometry each frame, uploads it to a single VRAM vertex/index buffer,
// and draws all sprites with minimal texture-bind changes.
//
// Usage per frame:
//   spriteBuffer.Begin();
//   sprite.RenderToBuffer(&spriteBuffer);     // any number of sprites
//   spriteBuffer.Upload(cmd);                 // once, before the render pass
//   spriteBuffer.Draw(renderPass, pipeline);  // once, inside the render pass
class SpriteBuffer
{
public:
    struct Vertex
    {
        float x;
        float y;
        float z;
        float u;
        float v;
        float r;
        float g;
        float b;
        float a;
    };

    explicit SpriteBuffer(SDL_GPUDevice *device);
    ~SpriteBuffer();

    SpriteBuffer(const SpriteBuffer &) = delete;
    SpriteBuffer &operator=(const SpriteBuffer &) = delete;

    // Clear all CPU-side state; call at the start of each frame.
    void Begin();

    // Append one sprite's four vertices and texture binding to the buffer.
    // Consecutive sprites sharing the same SDL_GPUTexture pointer and pipeline are
    // batched into a single draw call automatically. pipeline is optional: pass
    // nullptr to use whichever pipeline is given to Draw()/DrawNoSampler(), or a
    // pipeline built from an alternate fragment shader to override it per sprite
    // (that pipeline must use the same vertex shader/layout as the rest).
    void AddSprite(const Vertex vertices[4], SDL_GPUTexture *texture, SDL_GPUSampler *sampler,
                   SDL_GPUGraphicsPipeline *pipeline = nullptr);

    // Upload the accumulated vertex/index data to VRAM via a copy pass inside cmd.
    // Must be called before the render pass that uses Draw().
    void Upload(SDL_GPUCommandBuffer *cmd);

    // Call once after submitting the command buffer used for Upload().
    void OnUploadSubmitComplete();

    // Bind the single VRAM vertex/index buffer and issue one draw call per
    // contiguous group of sprites that share a texture and pipeline. defaultPipeline
    // is used for any span whose AddSprite() call didn't specify its own pipeline.
    void Draw(SDL_GPURenderPass *renderPass, SDL_GPUGraphicsPipeline *defaultPipeline) const;

    // Debug-only draw path for fragment shaders that do not use texture samplers.
    void DrawNoSampler(SDL_GPURenderPass *renderPass, SDL_GPUGraphicsPipeline *defaultPipeline) const;

    // Global fake-gamma-correction brightness applied by the default fragment shader to every
    // sprite, 0 (no adjustment) to 100 (maximum adjustment). Clamped on write.
    static void SetFakeGammaBrightness(int level);
    static int GetFakeGammaBrightness();
    // Normalized 0.0-1.0 value; pass this each frame via SDL_PushGPUFragmentUniformData.
    static float GetFakeGammaBrightnessNormalized();

    // Global glow halo radius (in texels of whichever texture a Glow-shaded sprite samples),
    // read by Glow.frag.hlsl. Shared by every sprite using the Glow shader this frame (same
    // per-frame-uniform limitation as brightness/time above). Clamped to >= 0 on write.
    static void SetGlowRadiusPixels(float radiusPixels);
    static float GetGlowRadiusPixels();

private:
    // A contiguous run of sprites that share one texture and pipeline.
    struct TextureSpan
    {
        SDL_GPUTexture           *texture;
        SDL_GPUSampler           *sampler;
        SDL_GPUGraphicsPipeline  *pipeline;    // nullptr means "use Draw()'s default pipeline"
        Uint32                    firstIndex;  // index into the index buffer (in element units)
        Uint32                    indexCount;
    };

    // Grow (or create) VRAM buffers to hold at least neededSprites sprites.
    bool EnsureCapacity(Uint32 neededSprites);

    SDL_GPUDevice  *m_device               = nullptr;
    SDL_GPUBuffer  *m_vertexBuffer         = nullptr;
    SDL_GPUBuffer  *m_indexBuffer          = nullptr;
    Uint32          m_gpuCapacitySprites   = 0;

    std::vector<Vertex>      m_cpuVertices;
    std::vector<Uint32>      m_cpuIndices;
    std::vector<TextureSpan> m_textureSpans;
    struct DeferredTransferBatch
    {
        int framesRemaining = 0;
        std::vector<SDL_GPUTransferBuffer *> transfers;
    };

    std::vector<SDL_GPUTransferBuffer *> m_pendingUploadTransfers;
    std::deque<DeferredTransferBatch> m_deferredTransfers;

    static int s_fakeGammaBrightness;
    static float s_glowRadiusPixels;
};
