#pragma once

#include <glm/vec2.hpp>

#include "ShaderLibrary.h"

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

#ifdef SDL3_NEW_GUI_HOT_RELOAD
#include <filesystem>
#endif

class LegacyImage;
class LegacyImageStyleLibrary;
class InputManager;
class SoundSystem;
class SpriteBuffer;
class TextureManager;
namespace tinyxml2
{
class XMLElement;
}

class GUIControl
{
public:
    GUIControl(std::string name, std::string type);
    virtual ~GUIControl() = default;

    virtual void SetViewportSize(int width, int height) = 0;
    virtual void Update(float deltaSeconds) = 0;
    virtual void ResetAnimation() = 0;
    virtual void RenderToBuffer(SpriteBuffer *buffer) const = 0;
    virtual bool OnMouseMove(int x, int y) = 0;
    virtual bool OnLeftMouseDown(int x, int y) = 0;
    virtual bool OnLeftMouseUp(int x, int y) = 0;
    virtual bool ConsumeButtonHit();
    virtual void SetEnabled(bool enabled);
    virtual bool IsHovered() const;

    // Whether this control type can receive keyboard/gamepad focus (only buttons, by default).
    virtual bool IsFocusable() const;
    void SetFocused(bool focused);
    bool IsFocused() const;

    // Appends the control(s) that should act as a keyboard/gamepad navigation target in place
    // of this control (defaults to itself when focusable); overridden by GUIScrollBarControl so
    // its arrow buttons are targets while the scrollbar itself is not.
    virtual void CollectFocusTargets(std::vector<GUIControl *> &out);

    // The control whose registered callback should fire when this control is activated via
    // keyboard/gamepad (defaults to itself); overridden by scrollbar arrow buttons, whose
    // value-changing callback is registered under the owning scrollbar's name.
    virtual GUIControl *GetActivationCallbackTarget();

    // Persistent toggle value; only meaningful for controls like GUICheckBoxControl.
    virtual bool IsChecked() const;
    virtual void SetChecked(bool checked);
    // Applies the same effect a real click would have, beyond firing the registered
    // callback (e.g. toggling a checkbox); no-op by default.
    virtual void Activate();

    // Only meaningful for controls like GUILabelControl; no-op/returns false otherwise.
    virtual bool SetText(const std::string &text);

    // Only meaningful for controls like GUIScrollBarControl; no-op/returns 0/false otherwise.
    virtual int GetScrollValue() const;
    virtual void SetScrollValue(int value);
    virtual void SetScrollRange(int minValue, int maxValue);
    virtual void SetScrollJumps(int lineJump, int pageJump);
    // Returns true (once) the frame a scrollbar's value changed via drag/click, same
    // contract as ConsumeButtonHit(); no-op/returns false for any other control type.
    virtual bool ConsumeValueChanged();

    // Shifts both the hit-box and (where overridden) any owned on-screen imagery by the
    // given pixel delta; used to reposition a control after construction (e.g. a scrollbar
    // thumb) without re-parsing its images.
    virtual void MoveBy(int dx, int dy);

    const std::string &GetName() const;
    const std::string &GetType() const;
    void SetBounds(int x, int y, int width, int height);
    bool ContainsPoint(int x, int y) const;
    bool IsEnabled() const;
    int GetX() const;
    int GetY() const;
    int GetWidth() const;
    int GetHeight() const;

protected:
    std::string m_name;
    std::string m_type;
    int m_x = 0;
    int m_y = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_enabled = true;
    bool m_focused = false;
};

class GUILabelControl final : public GUIControl
{
public:
    GUILabelControl(std::string name, std::vector<std::unique_ptr<LegacyImage>> images);

    void SetViewportSize(int width, int height) override;
    void Update(float deltaSeconds) override;
    void ResetAnimation() override;
    void RenderToBuffer(SpriteBuffer *buffer) const override;
    bool OnMouseMove(int x, int y) override;
    bool OnLeftMouseDown(int x, int y) override;
    bool OnLeftMouseUp(int x, int y) override;
    bool SetText(const std::string &text) override;

private:
    std::vector<std::unique_ptr<LegacyImage>> m_images;
};

class GUIButtonControl final : public GUIControl
{
public:
    GUIButtonControl(std::string name,
                     std::vector<std::unique_ptr<LegacyImage>> normal,
                     std::vector<std::unique_ptr<LegacyImage>> over,
                     std::vector<std::unique_ptr<LegacyImage>> down,
                     std::vector<std::unique_ptr<LegacyImage>> disabled);

    void SetViewportSize(int width, int height) override;
    void Update(float deltaSeconds) override;
    void ResetAnimation() override;
    void RenderToBuffer(SpriteBuffer *buffer) const override;
    bool OnMouseMove(int x, int y) override;
    bool OnLeftMouseDown(int x, int y) override;
    bool OnLeftMouseUp(int x, int y) override;
    bool ConsumeButtonHit() override;
    void SetEnabled(bool enabled) override;
    bool IsHovered() const override;
    bool IsFocusable() const override;
    void MoveBy(int dx, int dy) override;
    bool SetText(const std::string &text) override;
    GUIControl *GetActivationCallbackTarget() override;
    void Activate() override;

    // Used by GUIScrollBarControl to make its arrow buttons act on behalf of the scrollbar
    // when activated via keyboard/gamepad (see GetActivationCallbackTarget/Activate above).
    void SetFocusOwner(GUIControl *owner);
    void SetActivateCallback(std::function<void()> callback);

private:
    enum class VisualState
    {
        Normal,
        MouseOver,
        MouseDown,
        Disabled
    };

    const std::vector<std::unique_ptr<LegacyImage>> &GetRenderState() const;

    std::vector<std::unique_ptr<LegacyImage>> m_normal;
    std::vector<std::unique_ptr<LegacyImage>> m_over;
    std::vector<std::unique_ptr<LegacyImage>> m_down;
    std::vector<std::unique_ptr<LegacyImage>> m_disabled;
    VisualState m_state = VisualState::Normal;
    bool m_awaitingRelease = false;
    bool m_mouseOver = false;
    bool m_buttonHit = false;
    GUIControl *m_focusOwner = nullptr;
    std::function<void()> m_activateCallback;
};

class GUICheckBoxControl final : public GUIControl
{
public:
    GUICheckBoxControl(std::string name,
                       std::vector<std::unique_ptr<LegacyImage>> normal,
                       std::vector<std::unique_ptr<LegacyImage>> over,
                       std::vector<std::unique_ptr<LegacyImage>> down,
                       std::vector<std::unique_ptr<LegacyImage>> disabled,
                       std::vector<std::unique_ptr<LegacyImage>> checkedNormal,
                       std::vector<std::unique_ptr<LegacyImage>> checkedOver,
                       std::vector<std::unique_ptr<LegacyImage>> checkedDown,
                       std::vector<std::unique_ptr<LegacyImage>> checkedDisabled,
                       bool initiallyChecked);

    void SetViewportSize(int width, int height) override;
    void Update(float deltaSeconds) override;
    void ResetAnimation() override;
    void RenderToBuffer(SpriteBuffer *buffer) const override;
    bool OnMouseMove(int x, int y) override;
    bool OnLeftMouseDown(int x, int y) override;
    bool OnLeftMouseUp(int x, int y) override;
    // Returns true (once) the frame the checked value toggled, same contract as a button hit.
    bool ConsumeButtonHit() override;
    void SetEnabled(bool enabled) override;
    bool IsHovered() const override;
    bool IsFocusable() const override;
    bool IsChecked() const override;
    void SetChecked(bool checked) override;
    void Activate() override;

private:
    enum class VisualState
    {
        Normal,
        MouseOver,
        MouseDown,
        Disabled
    };

    const std::vector<std::unique_ptr<LegacyImage>> &GetRenderState() const;

    std::vector<std::unique_ptr<LegacyImage>> m_normal;
    std::vector<std::unique_ptr<LegacyImage>> m_over;
    std::vector<std::unique_ptr<LegacyImage>> m_down;
    std::vector<std::unique_ptr<LegacyImage>> m_disabled;
    std::vector<std::unique_ptr<LegacyImage>> m_checkedNormal;
    std::vector<std::unique_ptr<LegacyImage>> m_checkedOver;
    std::vector<std::unique_ptr<LegacyImage>> m_checkedDown;
    std::vector<std::unique_ptr<LegacyImage>> m_checkedDisabled;
    VisualState m_state = VisualState::Normal;
    bool m_awaitingRelease = false;
    bool m_mouseOver = false;
    bool m_checked = false;
    bool m_valueChanged = false;
};

// A vertical or horizontal scrollbar/slider built from three sub-buttons: a decrement
// arrow, a draggable thumb, and an increment arrow, plus click-to-page on the bare track.
class GUIScrollBarControl final : public GUIControl
{
public:
    GUIScrollBarControl(std::string name,
                       bool vertical,
                       std::unique_ptr<GUIButtonControl> decrementButton,
                       std::unique_ptr<GUIButtonControl> thumbButton,
                       std::unique_ptr<GUIButtonControl> incrementButton,
                       int minThumbPixel,
                       int maxThumbPixel);

    void SetViewportSize(int width, int height) override;
    void Update(float deltaSeconds) override;
    void ResetAnimation() override;
    void RenderToBuffer(SpriteBuffer *buffer) const override;
    bool OnMouseMove(int x, int y) override;
    bool OnLeftMouseDown(int x, int y) override;
    bool OnLeftMouseUp(int x, int y) override;
    // Returns true (once) the frame either arrow button was clicked, so the standard
    // click sound plays via the same path as any other button.
    bool ConsumeButtonHit() override;
    void SetEnabled(bool enabled) override;
    bool IsHovered() const override;
    void CollectFocusTargets(std::vector<GUIControl *> &out) override;

    int GetScrollValue() const override;
    void SetScrollValue(int value) override;
    void SetScrollRange(int minValue, int maxValue) override;
    void SetScrollJumps(int lineJump, int pageJump) override;
    bool ConsumeValueChanged() override;

private:
    // notifyChange marks the value-changed flag only for interactive edits (drag/click),
    // not for programmatic SetScrollValue()/SetScrollRange() calls.
    void ApplyValue(float newValue, bool notifyChange);
    void RepositionThumb();
    // Applies one line-jump step in the given direction (-1/+1); used when an arrow button is
    // activated via keyboard/gamepad instead of a real mouse click.
    void StepLineValue(int direction);

    std::unique_ptr<GUIButtonControl> m_decrementButton;
    std::unique_ptr<GUIButtonControl> m_thumbButton;
    std::unique_ptr<GUIButtonControl> m_incrementButton;
    bool m_vertical;
    int m_minThumbPixel;
    int m_maxThumbPixel;
    int m_valueMin = 0;
    int m_valueMax = 100;
    int m_lineJump = 1;
    int m_pageJump = 10;
    float m_value = 0.0f;
    bool m_dragging = false;
    int m_dragAnchorX = 0;
    int m_dragAnchorY = 0;
    bool m_valueChanged = false;
    bool m_arrowHit = false;
};

class GUIPlaceholderControl final : public GUIControl
{
public:
    GUIPlaceholderControl(std::string name, std::string type);

    void SetViewportSize(int width, int height) override;
    void Update(float deltaSeconds) override;
    void ResetAnimation() override;
    void RenderToBuffer(SpriteBuffer *buffer) const override;
    bool OnMouseMove(int x, int y) override;
    bool OnLeftMouseDown(int x, int y) override;
    bool OnLeftMouseUp(int x, int y) override;
};

class GUIManager
{
public:
    using ButtonCallback = std::function<void()>;

    GUIManager() = default;

    bool Load(TextureManager *textureManager, const std::string &guiXmlPath, SoundSystem *soundSystem = nullptr,
              ShaderLibrary *shaderLibrary = nullptr);
    void Unload();
    void HideAllForms();
    bool ShowForm(const std::string &name);
    bool HideForm(const std::string &name);
    void SetViewportSize(int width, int height);
    void Update(float deltaSeconds);
    void ProcessMouse(int x, int y, bool leftButtonDown);
    bool RegisterButtonCallback(const std::string &formName,
                                const std::string &controlName,
                                void *owner,
                                ButtonCallback callback);
    void UnregisterCallbacksForOwner(void *owner);
    bool SetControlEnabled(const std::string &formName, const std::string &controlName, bool enabled);
    // Only meaningful for checkbox controls; no-ops/returns false for any other control type.
    bool SetControlChecked(const std::string &formName, const std::string &controlName, bool checked);
    bool GetControlChecked(const std::string &formName, const std::string &controlName) const;
    // Only meaningful for label controls; no-ops/returns false for any other control type.
    bool SetControlText(const std::string &formName, const std::string &controlName, const std::string &text);    // Only meaningful for scrollbar controls; no-ops/returns 0 for any other control type.
    bool SetControlScrollRange(const std::string &formName, const std::string &controlName, int minValue, int maxValue);
    bool SetControlScrollValue(const std::string &formName, const std::string &controlName, int value);
    bool SetControlScrollJumps(const std::string &formName, const std::string &controlName, int lineJump, int pageJump);
    int GetControlScrollValue(const std::string &formName, const std::string &controlName) const;    bool ConsumeDirtyFlag();
    void RenderToBuffer(SpriteBuffer *buffer) const;

    // Moves keyboard/gamepad focus one step in the given direction (dx,dy each in {-1,0,1});
    // candidates are gathered across every currently-visible form. Does nothing if no control
    // lies roughly ahead of the focused one in that direction.
    void NavigateFocus(int dx, int dy);
    // Invokes the currently-focused control's registered callbacks, as if it were clicked.
    void ActivateFocusedControl();
    void ClearFocus();
    // Polls (and lazily registers) the GUI_UP/DOWN/LEFT/RIGHT/ACCEPT actions and drives
    // NavigateFocus()/ActivateFocusedControl() from them. Call once per frame.
    void UpdateGamepadNavigation(InputManager &inputManager, float deltaSeconds);
    // Menus/HUDs with nothing to navigate (or gameplay screens) should disable this so
    // gamepad input is left entirely to the game; menu-like modules should re-enable it.
    // Once the watchdog trips (see below), this is ignored for re-enable calls until
    // ClearGamepadNavigationFault() is called explicitly. Enabling here is also gated by
    // the system-wide preference set via SetGamepadNavigationPreferenceEnabled().
    void SetGamepadNavigationEnabled(bool enabled);
    bool IsGamepadNavigationEnabled() const;
    // System-wide user preference (e.g. from an options menu checkbox), persisted across
    // modules rather than reset by each one. When disabled, SetGamepadNavigationEnabled(true)
    // has no effect until the preference is re-enabled.
    void SetGamepadNavigationPreferenceEnabled(bool enabled);
    bool IsGamepadNavigationPreferenceEnabled() const;
    // Returns true (once) if the watchdog auto-disabled navigation due to a suspiciously
    // high rate of direction presses, e.g. a noisy/stuck analog stick.
    bool ConsumeGamepadNavigationFault();
    // Explicitly acknowledges a watchdog fault, unlocking and re-enabling navigation.
    void ClearGamepadNavigationFault();

    void EnableNavigationFaultMonitoring(bool enabled)
    {
        m_navigationFaultMonitoringEnabled = enabled;
    };

    size_t GetFormCount() const;
    size_t GetPlaceholderControlCount() const;

    // Loads (once) the form file at formPath - resolved relative to the executable's own
    // directory, e.g. "assets/OptionsMenu/OptionsMenu.xml" - if it isn't already loaded, and
    // returns its internal registered <Name> value (empty string on failure). Used for forms
    // that live in their owning module's own folder rather than under the shared GUI root.
    std::string LoadFormByPath(const std::string &formPath);

private:
    struct ControlCallbackEntry
    {
        void *owner = nullptr;
        ButtonCallback callback;
    };

    struct PredefineStateImages
    {
        std::vector<std::string> normal;
        std::vector<std::string> over;
        std::vector<std::string> down;
        std::vector<std::string> disabled;
        // Only populated/used by CHECKBOX controls; empty for button predefines.
        std::vector<std::string> checkedNormal;
        std::vector<std::string> checkedOver;
        std::vector<std::string> checkedDown;
        std::vector<std::string> checkedDisabled;
        // Fallback control size used when a control instance omits width/height; -1 means unset.
        int width = -1;
        int height = -1;
    };

    struct GUIForm
    {
        std::string name;
        std::vector<std::unique_ptr<LegacyImage>> images;
        std::vector<std::unique_ptr<GUIControl>> controls;
        bool visible = false;
        // Higher values are shown more recently and render on top of lower ones.
        int showOrder = 0;
        std::string sourcePath;
#ifdef SDL3_NEW_GUI_HOT_RELOAD
        std::filesystem::file_time_type lastWriteTime{};
#endif
    };

    bool LoadConfigFile(const std::string &guiXmlPath);
    bool LoadAllForms();
    bool LoadFormFile(const std::string &formPath);
    bool ParseFormFile(const std::string &formPath, GUIForm &outForm) const;

#ifdef SDL3_NEW_GUI_HOT_RELOAD
    // Polls visible forms' source files roughly once per second and reloads any that changed on disk.
    void CheckHotReload(float deltaSeconds);
    void ReloadFormFile(GUIForm &form, const std::filesystem::file_time_type &writeTime);
#endif

    bool ParseStyles(const tinyxml2::XMLElement *root);
    bool ParseFonts(const tinyxml2::XMLElement *root);
    bool ParsePredefines(const tinyxml2::XMLElement *root);
    // Parses <Defaults><Default type="CHECKBOX" define="..." /></Defaults>: a per-control-type
    // predefine name used when a control supplies neither a "define" attribute nor inline images.
    bool ParseDefaults(const tinyxml2::XMLElement *root);
    bool ParseSounds(const tinyxml2::XMLElement *root);
    bool ParseShaders(const tinyxml2::XMLElement *root);
    // Returns the predefine name a control of the given (upper-case) type should fall back to
    // when it has no explicit "define" attribute of its own; empty if none is registered.
    std::string GetDefaultDefineName(const std::string &controlTypeUpper) const;
    void PlayOverSound();
    void PlayClickSound();

    std::unique_ptr<LegacyImage> BuildLegacyImage(const tinyxml2::XMLElement *imageElement,
                                                   const glm::vec2 &extraOffset) const;
    std::unique_ptr<LegacyImage> BuildLegacyImageFromSnippet(const std::string &xmlSnippet,
                                                              const glm::vec2 &extraOffset) const;

    std::vector<std::unique_ptr<LegacyImage>> ParseStateImages(const tinyxml2::XMLElement *stateElement,
                                                                const glm::vec2 &controlOffset) const;
    std::vector<std::unique_ptr<LegacyImage>> ParseStateImagesFromSnippets(const std::vector<std::string> &snippets,
                                                                            const glm::vec2 &controlOffset) const;

    // Builds a GUIButtonControl from a <Control type="BUTTON"> element (states plus optional
    // "define" predefine fallback); returns nullptr if no images could be found for any state.
    std::unique_ptr<GUIButtonControl> BuildButtonControl(const tinyxml2::XMLElement *controlElement,
                                                          std::string name,
                                                          const glm::vec2 &position,
                                                          int width, int height) const;

    std::unique_ptr<GUIControl> ParseControl(const tinyxml2::XMLElement *controlElement) const;

    static std::string ToUpperCopy(std::string text);
    static std::string ToLowerCopy(std::string text);
    static std::string NormalizeSlashes(std::string text);

    std::string ResolveGuiRelativePath(const std::string &path) const;
    std::string ResolveTexturePath(const std::string &path) const;
    void RegisterFontAliases(const std::string &fontName, const std::string &fontXmlPath);

    void SetFocusedControl(GUIControl *control, const std::string &formName);
    void InvokeControlCallbacks(const std::string &formName, const std::string &controlName);
    // Parks the pointer off-screen (so it can't re-hover anything) until the real mouse
    // moves away from where it was when a gamepad/keyboard navigation last took over.
    void SuppressPointerUntilMoved();

    TextureManager *m_textureManager = nullptr;
    ShaderLibrary *m_shaderLibrary = nullptr;
    SoundSystem *m_soundSystem = nullptr;
    std::string m_overSoundName;
    std::string m_clickSoundName;
    std::string m_guiRootDirectory;
    std::string m_defaultFontName = "f25bankprinter32";
    std::string m_defaultFontPath = "assets/fonts/F25BankPrinter32.xml";

    LegacyImageStyleLibrary *m_styles = nullptr;
    std::unique_ptr<LegacyImageStyleLibrary> m_ownedStyles;
    std::unordered_map<std::string, PredefineStateImages> m_predefines;
    // Keyed by upper-case control type (e.g. "CHECKBOX"), value is a lower-case predefine name.
    std::unordered_map<std::string, std::string> m_defaultDefines;
    std::unordered_map<std::string, std::shared_ptr<ShaderLibrary::ShaderResource>> m_shadersByName;

    std::vector<GUIForm> m_forms;
    std::unordered_map<std::string, std::vector<ControlCallbackEntry>> m_controlCallbacks;
    mutable size_t m_placeholderControlCount = 0;
    int m_viewportWidth = 960;
    int m_viewportHeight = 540;
    bool m_prevLeftButtonDown = false;
    int m_prevMouseX = -1;
    int m_prevMouseY = -1;
    bool m_dirty = false;
    int m_nextShowOrder = 0;
    GUIControl *m_focusedControl = nullptr;
    std::string m_focusedFormName;
    bool m_pointerSuppressed = false;
    int m_pointerSuppressOriginX = 0;
    int m_pointerSuppressOriginY = 0;
    int m_lastRealMouseX = -100000;
    int m_lastRealMouseY = -100000;
    int m_actionGuiUp = -1;
    int m_actionGuiDown = -1;
    int m_actionGuiLeft = -1;
    int m_actionGuiRight = -1;
    int m_actionGuiAccept = -1;
    bool m_gamepadActionsRegistered = false;
    bool m_gamepadNavigationEnabled = true;
    bool m_gamepadNavigationPreferenceEnabled = true;
    bool m_gamepadNavigationFault = false;
    bool m_gamepadNavigationLocked = false;
    bool m_navigationFaultMonitoringEnabled = false;
    float m_navRateWindowTimer = 0.0f;
    int m_navRateWindowCount = 0;
#ifdef SDL3_NEW_GUI_HOT_RELOAD
    float m_hotReloadAccumulator = 0.0f;
#endif

    static std::string MakeControlKey(const std::string &formName, const std::string &controlName);
};
