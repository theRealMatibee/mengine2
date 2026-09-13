#include "GUIManager.h"

#include "InputManager.h"
#include "LegacyImage.h"
#include "SoundSystem.h"
#include "SpriteBuffer.h"
#include "TextureManager.h"
#include "Translation.h"
#include "tinyxml2.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <utility>

GUIControl::GUIControl(std::string name, std::string type)
    : m_name(std::move(name)), m_type(std::move(type))
{
}

const std::string &GUIControl::GetName() const
{
    return m_name;
}

const std::string &GUIControl::GetType() const
{
    return m_type;
}

void GUIControl::SetBounds(int x, int y, int width, int height)
{
    m_x = x;
    m_y = y;
    m_width = width;
    m_height = height;
}

bool GUIControl::ContainsPoint(int x, int y) const
{
    if (m_width <= 0 || m_height <= 0)
    {
        return false;
    }

    return (x >= m_x && x <= (m_x + m_width) &&
            y >= m_y && y <= (m_y + m_height));
}

bool GUIControl::ConsumeButtonHit()
{
    return false;
}

bool GUIControl::IsHovered() const
{
    return false;
}

bool GUIControl::IsFocusable() const
{
    return false;
}

void GUIControl::SetFocused(bool focused)
{
    m_focused = focused;
}

bool GUIControl::IsFocused() const
{
    return m_focused;
}

void GUIControl::CollectFocusTargets(std::vector<GUIControl *> &out)
{
    if (IsFocusable())
    {
        out.push_back(this);
    }
}

GUIControl *GUIControl::GetActivationCallbackTarget()
{
    return this;
}

int GUIControl::GetX() const
{
    return m_x;
}

int GUIControl::GetY() const
{
    return m_y;
}

int GUIControl::GetWidth() const
{
    return m_width;
}

int GUIControl::GetHeight() const
{
    return m_height;
}

bool GUIControl::IsChecked() const
{
    return false;
}

void GUIControl::SetChecked(bool checked)
{
    (void) checked;
}

void GUIControl::Activate()
{
}

bool GUIControl::SetText(const std::string &text)
{
    (void) text;
    return false;
}

int GUIControl::GetScrollValue() const
{
    return 0;
}

void GUIControl::SetScrollValue(int value)
{
    (void) value;
}

void GUIControl::SetScrollRange(int minValue, int maxValue)
{
    (void) minValue;
    (void) maxValue;
}

void GUIControl::SetScrollJumps(int lineJump, int pageJump)
{
    (void) lineJump;
    (void) pageJump;
}

bool GUIControl::ConsumeValueChanged()
{
    return false;
}

void GUIControl::MoveBy(int dx, int dy)
{
    m_x += dx;
    m_y += dy;
}

void GUIControl::SetEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool GUIControl::IsEnabled() const
{
    return m_enabled;
}

GUILabelControl::GUILabelControl(std::string name, std::vector<std::unique_ptr<LegacyImage>> images)
    : GUIControl(std::move(name), "LABEL"),
      m_images(std::move(images))
{
}

void GUILabelControl::SetViewportSize(int width, int height)
{
    for (const std::unique_ptr<LegacyImage> &image : m_images)
    {
        if (image != nullptr)
        {
            image->SetViewportSize(width, height);
        }
    }
}

void GUILabelControl::Update(float deltaSeconds)
{
    for (const std::unique_ptr<LegacyImage> &image : m_images)
    {
        if (image != nullptr)
        {
            image->Update(deltaSeconds);
        }
    }
}

void GUILabelControl::ResetAnimation()
{
    for (const std::unique_ptr<LegacyImage> &image : m_images)
    {
        if (image != nullptr)
        {
            image->ResetAnimation();
        }
    }
}

void GUILabelControl::RenderToBuffer(SpriteBuffer *buffer) const
{
    for (const std::unique_ptr<LegacyImage> &image : m_images)
    {
        if (image != nullptr)
        {
            image->RenderToBuffer(buffer);
        }
    }
}

bool GUILabelControl::OnMouseMove(int x, int y)
{
    (void) x;
    (void) y;
    return false;
}

bool GUILabelControl::OnLeftMouseDown(int x, int y)
{
    return ContainsPoint(x, y);
}

bool GUILabelControl::OnLeftMouseUp(int x, int y)
{
    (void) x;
    (void) y;
    return false;
}

bool GUILabelControl::SetText(const std::string &text)
{
    bool anySucceeded = false;
    for (const std::unique_ptr<LegacyImage> &image : m_images)
    {
        if (image != nullptr && image->SetLabelText(text))
        {
            anySucceeded = true;
        }
    }
    return anySucceeded;
}

GUIButtonControl::GUIButtonControl(std::string name,
                                   std::vector<std::unique_ptr<LegacyImage>> normal,
                                   std::vector<std::unique_ptr<LegacyImage>> over,
                                   std::vector<std::unique_ptr<LegacyImage>> down,
                                   std::vector<std::unique_ptr<LegacyImage>> disabled)
    : GUIControl(std::move(name), "BUTTON"),
      m_normal(std::move(normal)),
      m_over(std::move(over)),
      m_down(std::move(down)),
      m_disabled(std::move(disabled))
{
}

void GUIButtonControl::SetViewportSize(int width, int height)
{
    auto apply = [width, height](const std::vector<std::unique_ptr<LegacyImage>> &images)
    {
        for (const std::unique_ptr<LegacyImage> &image : images)
        {
            if (image != nullptr)
            {
                image->SetViewportSize(width, height);
            }
        }
    };

    apply(m_normal);
    apply(m_over);
    apply(m_down);
    apply(m_disabled);
}

void GUIButtonControl::Update(float deltaSeconds)
{
    auto tick = [deltaSeconds](const std::vector<std::unique_ptr<LegacyImage>> &images)
    {
        for (const std::unique_ptr<LegacyImage> &image : images)
        {
            if (image != nullptr)
            {
                image->Update(deltaSeconds);
            }
        }
    };

    tick(m_normal);
    tick(m_over);
    tick(m_down);
    tick(m_disabled);
}

void GUIButtonControl::ResetAnimation()
{
    auto reset = [](const std::vector<std::unique_ptr<LegacyImage>> &images)
    {
        for (const std::unique_ptr<LegacyImage> &image : images)
        {
            if (image != nullptr)
            {
                image->ResetAnimation();
            }
        }
    };

    reset(m_normal);
    reset(m_over);
    reset(m_down);
    reset(m_disabled);
}

const std::vector<std::unique_ptr<LegacyImage>> &GUIButtonControl::GetRenderState() const
{
    if (m_state == VisualState::MouseDown && !m_down.empty())
    {
        return m_down;
    }
    if ((m_state == VisualState::MouseOver || (m_focused && m_state == VisualState::Normal)) && !m_over.empty())
    {
        return m_over;
    }
    if (m_state == VisualState::Disabled && !m_disabled.empty())
    {
        return m_disabled;
    }

    if (!m_normal.empty())
    {
        return m_normal;
    }
    if (!m_over.empty())
    {
        return m_over;
    }
    if (!m_down.empty())
    {
        return m_down;
    }
    return m_disabled;
}

void GUIButtonControl::RenderToBuffer(SpriteBuffer *buffer) const
{
    const std::vector<std::unique_ptr<LegacyImage>> &images = GetRenderState();
    for (const std::unique_ptr<LegacyImage> &image : images)
    {
        if (image != nullptr)
        {
            image->RenderToBuffer(buffer);
        }
    }
}

bool GUIButtonControl::OnMouseMove(int x, int y)
{
    if (!IsEnabled())
    {
        return false;
    }

    const VisualState previousState = m_state;
    const bool previousMouseOver = m_mouseOver;
    const bool inside = ContainsPoint(x, y);

    if (inside)
    {
        m_mouseOver = true;
        m_state = m_awaitingRelease ? VisualState::MouseDown : VisualState::MouseOver;
        return previousState != m_state || previousMouseOver != m_mouseOver;
    }

    if (!m_awaitingRelease)
    {
        m_mouseOver = false;
        m_state = VisualState::Normal;
    }

    return previousState != m_state || previousMouseOver != m_mouseOver;
}

bool GUIButtonControl::OnLeftMouseDown(int x, int y)
{
    if (!IsEnabled() || !ContainsPoint(x, y))
    {
        return false;
    }

    m_awaitingRelease = true;
    m_state = VisualState::MouseDown;
    m_mouseOver = true;
    return true;
}

bool GUIButtonControl::OnLeftMouseUp(int x, int y)
{
    if (!IsEnabled())
    {
        return false;
    }

    const VisualState previousState = m_state;
    const bool previousMouseOver = m_mouseOver;
    const bool previousAwaitingRelease = m_awaitingRelease;
    const bool inside = ContainsPoint(x, y);

    if (inside)
    {
        if (m_awaitingRelease)
        {
            m_awaitingRelease = false;
            m_state = VisualState::MouseOver;
            m_mouseOver = true;
            m_buttonHit = true;
            return previousState != m_state || previousMouseOver != m_mouseOver || previousAwaitingRelease != m_awaitingRelease;
        }

        m_state = VisualState::MouseOver;
        m_mouseOver = true;
        return previousState != m_state || previousMouseOver != m_mouseOver || previousAwaitingRelease != m_awaitingRelease;
    }

    m_state = VisualState::Normal;
    m_mouseOver = false;
    m_awaitingRelease = false;
    return previousState != m_state || previousMouseOver != m_mouseOver || previousAwaitingRelease != m_awaitingRelease;
}

bool GUIButtonControl::ConsumeButtonHit()
{
    const bool wasHit = m_buttonHit;
    m_buttonHit = false;
    return wasHit;
}

void GUIButtonControl::SetEnabled(bool enabled)
{
    GUIControl::SetEnabled(enabled);
    m_awaitingRelease = false;
    m_mouseOver = false;
    m_state = enabled ? VisualState::Normal : VisualState::Disabled;
}

bool GUIButtonControl::IsHovered() const
{
    return m_mouseOver;
}

bool GUIButtonControl::IsFocusable() const
{
    return true;
}

GUIControl *GUIButtonControl::GetActivationCallbackTarget()
{
    return m_focusOwner != nullptr ? m_focusOwner : this;
}

void GUIButtonControl::Activate()
{
    if (m_activateCallback)
    {
        m_activateCallback();
    }
}

void GUIButtonControl::SetFocusOwner(GUIControl *owner)
{
    m_focusOwner = owner;
}

void GUIButtonControl::SetActivateCallback(std::function<void()> callback)
{
    m_activateCallback = std::move(callback);
}

bool GUIButtonControl::SetText(const std::string &text)
{
    bool textSet = false;
    auto setText = [&text, &textSet](const std::vector<std::unique_ptr<LegacyImage>> &images)
    {
        for (const std::unique_ptr<LegacyImage> &image : images)
        {
            if (image != nullptr)
            {
                textSet = image->SetLabelText(text);
            }
        }
    };

    setText(m_normal);
    setText(m_over);
    setText(m_down);
    setText(m_disabled);

    return textSet;
}

void GUIButtonControl::MoveBy(int dx, int dy)
{
    GUIControl::MoveBy(dx, dy);

    const glm::vec2 delta(static_cast<float>(dx), static_cast<float>(dy));
    auto move = [&delta](const std::vector<std::unique_ptr<LegacyImage>> &images)
    {
        for (const std::unique_ptr<LegacyImage> &image : images)
        {
            if (image != nullptr)
            {
                image->MovePixels(delta);
            }
        }
    };

    move(m_normal);
    move(m_over);
    move(m_down);
    move(m_disabled);
}

GUICheckBoxControl::GUICheckBoxControl(std::string name,
                                       std::vector<std::unique_ptr<LegacyImage>> normal,
                                       std::vector<std::unique_ptr<LegacyImage>> over,
                                       std::vector<std::unique_ptr<LegacyImage>> down,
                                       std::vector<std::unique_ptr<LegacyImage>> disabled,
                                       std::vector<std::unique_ptr<LegacyImage>> checkedNormal,
                                       std::vector<std::unique_ptr<LegacyImage>> checkedOver,
                                       std::vector<std::unique_ptr<LegacyImage>> checkedDown,
                                       std::vector<std::unique_ptr<LegacyImage>> checkedDisabled,
                                       bool initiallyChecked)
    : GUIControl(std::move(name), "CHECKBOX"),
      m_normal(std::move(normal)),
      m_over(std::move(over)),
      m_down(std::move(down)),
      m_disabled(std::move(disabled)),
      m_checkedNormal(std::move(checkedNormal)),
      m_checkedOver(std::move(checkedOver)),
      m_checkedDown(std::move(checkedDown)),
      m_checkedDisabled(std::move(checkedDisabled)),
      m_checked(initiallyChecked)
{
}

void GUICheckBoxControl::SetViewportSize(int width, int height)
{
    auto apply = [width, height](const std::vector<std::unique_ptr<LegacyImage>> &images)
    {
        for (const std::unique_ptr<LegacyImage> &image : images)
        {
            if (image != nullptr)
            {
                image->SetViewportSize(width, height);
            }
        }
    };

    apply(m_normal);
    apply(m_over);
    apply(m_down);
    apply(m_disabled);
    apply(m_checkedNormal);
    apply(m_checkedOver);
    apply(m_checkedDown);
    apply(m_checkedDisabled);
}

void GUICheckBoxControl::Update(float deltaSeconds)
{
    auto tick = [deltaSeconds](const std::vector<std::unique_ptr<LegacyImage>> &images)
    {
        for (const std::unique_ptr<LegacyImage> &image : images)
        {
            if (image != nullptr)
            {
                image->Update(deltaSeconds);
            }
        }
    };

    tick(m_normal);
    tick(m_over);
    tick(m_down);
    tick(m_disabled);
    tick(m_checkedNormal);
    tick(m_checkedOver);
    tick(m_checkedDown);
    tick(m_checkedDisabled);
}

void GUICheckBoxControl::ResetAnimation()
{
    auto reset = [](const std::vector<std::unique_ptr<LegacyImage>> &images)
    {
        for (const std::unique_ptr<LegacyImage> &image : images)
        {
            if (image != nullptr)
            {
                image->ResetAnimation();
            }
        }
    };

    reset(m_normal);
    reset(m_over);
    reset(m_down);
    reset(m_disabled);
    reset(m_checkedNormal);
    reset(m_checkedOver);
    reset(m_checkedDown);
    reset(m_checkedDisabled);
}

const std::vector<std::unique_ptr<LegacyImage>> &GUICheckBoxControl::GetRenderState() const
{
    const std::vector<std::unique_ptr<LegacyImage>> &normal = m_checked ? m_checkedNormal : m_normal;
    const std::vector<std::unique_ptr<LegacyImage>> &over = m_checked ? m_checkedOver : m_over;
    const std::vector<std::unique_ptr<LegacyImage>> &down = m_checked ? m_checkedDown : m_down;
    const std::vector<std::unique_ptr<LegacyImage>> &disabled = m_checked ? m_checkedDisabled : m_disabled;

    if (m_state == VisualState::MouseDown && !down.empty())
    {
        return down;
    }
    if ((m_state == VisualState::MouseOver || (m_focused && m_state == VisualState::Normal)) && !over.empty())
    {
        return over;
    }
    if (m_state == VisualState::Disabled && !disabled.empty())
    {
        return disabled;
    }

    if (!normal.empty())
    {
        return normal;
    }
    if (!over.empty())
    {
        return over;
    }
    if (!down.empty())
    {
        return down;
    }
    return disabled;
}

void GUICheckBoxControl::RenderToBuffer(SpriteBuffer *buffer) const
{
    const std::vector<std::unique_ptr<LegacyImage>> &images = GetRenderState();
    for (const std::unique_ptr<LegacyImage> &image : images)
    {
        if (image != nullptr)
        {
            image->RenderToBuffer(buffer);
        }
    }
}

bool GUICheckBoxControl::OnMouseMove(int x, int y)
{
    if (!IsEnabled())
    {
        return false;
    }

    const VisualState previousState = m_state;
    const bool previousMouseOver = m_mouseOver;
    const bool inside = ContainsPoint(x, y);

    if (inside)
    {
        m_mouseOver = true;
        m_state = m_awaitingRelease ? VisualState::MouseDown : VisualState::MouseOver;
        return previousState != m_state || previousMouseOver != m_mouseOver;
    }

    if (!m_awaitingRelease)
    {
        m_mouseOver = false;
        m_state = VisualState::Normal;
    }

    return previousState != m_state || previousMouseOver != m_mouseOver;
}

bool GUICheckBoxControl::OnLeftMouseDown(int x, int y)
{
    if (!IsEnabled() || !ContainsPoint(x, y))
    {
        return false;
    }

    m_awaitingRelease = true;
    m_state = VisualState::MouseDown;
    m_mouseOver = true;
    return true;
}

bool GUICheckBoxControl::OnLeftMouseUp(int x, int y)
{
    if (!IsEnabled())
    {
        return false;
    }

    const VisualState previousState = m_state;
    const bool previousMouseOver = m_mouseOver;
    const bool previousAwaitingRelease = m_awaitingRelease;
    const bool inside = ContainsPoint(x, y);

    if (inside)
    {
        if (m_awaitingRelease)
        {
            m_awaitingRelease = false;
            m_state = VisualState::MouseOver;
            m_mouseOver = true;
            m_checked = !m_checked;
            m_valueChanged = true;
            return previousState != m_state || previousMouseOver != m_mouseOver || previousAwaitingRelease != m_awaitingRelease;
        }

        m_state = VisualState::MouseOver;
        m_mouseOver = true;
        return previousState != m_state || previousMouseOver != m_mouseOver || previousAwaitingRelease != m_awaitingRelease;
    }

    m_state = VisualState::Normal;
    m_mouseOver = false;
    m_awaitingRelease = false;
    return previousState != m_state || previousMouseOver != m_mouseOver || previousAwaitingRelease != m_awaitingRelease;
}

bool GUICheckBoxControl::ConsumeButtonHit()
{
    const bool wasChanged = m_valueChanged;
    m_valueChanged = false;
    return wasChanged;
}

void GUICheckBoxControl::SetEnabled(bool enabled)
{
    GUIControl::SetEnabled(enabled);
    m_awaitingRelease = false;
    m_mouseOver = false;
    m_state = enabled ? VisualState::Normal : VisualState::Disabled;
}

bool GUICheckBoxControl::IsHovered() const
{
    return m_mouseOver;
}

bool GUICheckBoxControl::IsFocusable() const
{
    return true;
}

bool GUICheckBoxControl::IsChecked() const
{
    return m_checked;
}

void GUICheckBoxControl::SetChecked(bool checked)
{
    m_checked = checked;
}

void GUICheckBoxControl::Activate()
{
    m_checked = !m_checked;
}

GUIScrollBarControl::GUIScrollBarControl(std::string name,
                                         bool vertical,
                                         std::unique_ptr<GUIButtonControl> decrementButton,
                                         std::unique_ptr<GUIButtonControl> thumbButton,
                                         std::unique_ptr<GUIButtonControl> incrementButton,
                                         int minThumbPixel,
                                         int maxThumbPixel)
    : GUIControl(std::move(name), "SCROLLBAR"),
      m_decrementButton(std::move(decrementButton)),
      m_thumbButton(std::move(thumbButton)),
      m_incrementButton(std::move(incrementButton)),
      m_vertical(vertical),
      m_minThumbPixel(minThumbPixel),
      m_maxThumbPixel(maxThumbPixel)
{
    // The thumb isn't exposed as a nav target (dragging isn't meaningful via gamepad), but
    // the arrows are, and activating one should step the scrollbar itself rather than the
    // (otherwise inert) button, so the owning scrollbar's registered callback still fires.
    m_decrementButton->SetFocusOwner(this);
    m_decrementButton->SetActivateCallback([this]() { StepLineValue(-1); });
    m_incrementButton->SetFocusOwner(this);
    m_incrementButton->SetActivateCallback([this]() { StepLineValue(1); });
}

void GUIScrollBarControl::SetViewportSize(int width, int height)
{
    m_decrementButton->SetViewportSize(width, height);
    m_thumbButton->SetViewportSize(width, height);
    m_incrementButton->SetViewportSize(width, height);
}

void GUIScrollBarControl::Update(float deltaSeconds)
{
    m_decrementButton->Update(deltaSeconds);
    m_thumbButton->Update(deltaSeconds);
    m_incrementButton->Update(deltaSeconds);
}

void GUIScrollBarControl::ResetAnimation()
{
    m_decrementButton->ResetAnimation();
    m_thumbButton->ResetAnimation();
    m_incrementButton->ResetAnimation();
}

void GUIScrollBarControl::RenderToBuffer(SpriteBuffer *buffer) const
{
    m_decrementButton->RenderToBuffer(buffer);
    m_thumbButton->RenderToBuffer(buffer);
    m_incrementButton->RenderToBuffer(buffer);
}

bool GUIScrollBarControl::OnMouseMove(int x, int y)
{
    bool dirty = m_decrementButton->OnMouseMove(x, y);
    dirty = m_thumbButton->OnMouseMove(x, y) || dirty;
    dirty = m_incrementButton->OnMouseMove(x, y) || dirty;

    if (m_dragging)
    {
        const int deltaPixels = m_vertical ? (y - m_dragAnchorY) : (x - m_dragAnchorX);
        if (deltaPixels != 0)
        {
            const float extents = static_cast<float>(m_maxThumbPixel - m_minThumbPixel);
            const float valueRange = static_cast<float>(m_valueMax - m_valueMin);
            if (extents > 0.0f && valueRange != 0.0f)
            {
                ApplyValue(m_value + (static_cast<float>(deltaPixels) / extents) * valueRange, true);
            }
            m_dragAnchorX = x;
            m_dragAnchorY = y;
            dirty = true;
        }
    }

    return dirty;
}

bool GUIScrollBarControl::OnLeftMouseDown(int x, int y)
{
    if (!IsEnabled() || !ContainsPoint(x, y))
    {
        return false;
    }

    if (m_thumbButton->OnLeftMouseDown(x, y))
    {
        m_dragging = true;
        m_dragAnchorX = x;
        m_dragAnchorY = y;
        return true;
    }

    if (m_decrementButton->OnLeftMouseDown(x, y) || m_incrementButton->OnLeftMouseDown(x, y))
    {
        return true;
    }

    // Clicking the bare track pages towards whichever side of the thumb was clicked.
    if (m_vertical ? (y <= m_thumbButton->GetY()) : (x <= m_thumbButton->GetX()))
    {
        ApplyValue(m_value - static_cast<float>(m_pageJump), true);
    }
    else
    {
        ApplyValue(m_value + static_cast<float>(m_pageJump), true);
    }

    return true;
}

bool GUIScrollBarControl::OnLeftMouseUp(int x, int y)
{
    bool dirty = m_decrementButton->OnLeftMouseUp(x, y);
    dirty = m_thumbButton->OnLeftMouseUp(x, y) || dirty;
    dirty = m_incrementButton->OnLeftMouseUp(x, y) || dirty;

    if (m_decrementButton->ConsumeButtonHit())
    {
        ApplyValue(m_value - static_cast<float>(m_lineJump), true);
        m_arrowHit = true;
    }
    if (m_incrementButton->ConsumeButtonHit())
    {
        ApplyValue(m_value + static_cast<float>(m_lineJump), true);
        m_arrowHit = true;
    }

    m_dragging = false;
    return dirty;
}

bool GUIScrollBarControl::ConsumeButtonHit()
{
    const bool hit = m_arrowHit;
    m_arrowHit = false;
    return hit;
}

void GUIScrollBarControl::SetEnabled(bool enabled)
{
    GUIControl::SetEnabled(enabled);
    m_decrementButton->SetEnabled(enabled);
    m_thumbButton->SetEnabled(enabled);
    m_incrementButton->SetEnabled(enabled);
    m_dragging = false;
}

bool GUIScrollBarControl::IsHovered() const
{
    return m_decrementButton->IsHovered() || m_thumbButton->IsHovered() || m_incrementButton->IsHovered();
}

void GUIScrollBarControl::CollectFocusTargets(std::vector<GUIControl *> &out)
{
    out.push_back(m_decrementButton.get());
    out.push_back(m_incrementButton.get());
}

int GUIScrollBarControl::GetScrollValue() const
{
    return static_cast<int>(m_value);
}

void GUIScrollBarControl::SetScrollValue(int value)
{
    ApplyValue(static_cast<float>(value), false);
}

void GUIScrollBarControl::SetScrollRange(int minValue, int maxValue)
{
    m_valueMin = minValue;
    m_valueMax = maxValue;
    ApplyValue(m_value, false);
}

void GUIScrollBarControl::SetScrollJumps(int lineJump, int pageJump)
{
    m_lineJump = lineJump;
    m_pageJump = pageJump;
}

bool GUIScrollBarControl::ConsumeValueChanged()
{
    const bool changed = m_valueChanged;
    m_valueChanged = false;
    return changed;
}

void GUIScrollBarControl::ApplyValue(float newValue, bool notifyChange)
{
    if (newValue > static_cast<float>(m_valueMax))
    {
        newValue = static_cast<float>(m_valueMax);
    }
    if (newValue < static_cast<float>(m_valueMin))
    {
        newValue = static_cast<float>(m_valueMin);
    }

    const bool changed = (newValue != m_value);
    m_value = newValue;
    RepositionThumb();

    if (changed && notifyChange)
    {
        m_valueChanged = true;
    }
}

void GUIScrollBarControl::StepLineValue(int direction)
{
    // notifyChange=false: the caller (GUIManager::ActivateFocusedControl) already invokes the
    // registered callback itself once, exactly like a real arrow click's mouse-up path does.
    ApplyValue(m_value + static_cast<float>(direction) * static_cast<float>(m_lineJump), false);
}

void GUIScrollBarControl::RepositionThumb()
{
    const float range = static_cast<float>(m_valueMax - m_valueMin);
    const float extents = static_cast<float>(m_maxThumbPixel - m_minThumbPixel);
    float targetPixel = static_cast<float>(m_minThumbPixel);
    if (range != 0.0f)
    {
        targetPixel = static_cast<float>(m_minThumbPixel) + extents * (m_value - static_cast<float>(m_valueMin)) / range;
    }

    int targetPixelInt = static_cast<int>(targetPixel);
    if (targetPixelInt < m_minThumbPixel)
    {
        targetPixelInt = m_minThumbPixel;
    }
    if (targetPixelInt > m_maxThumbPixel)
    {
        targetPixelInt = m_maxThumbPixel;
    }

    if (m_vertical)
    {
        const int dy = targetPixelInt - m_thumbButton->GetY();
        if (dy != 0)
        {
            m_thumbButton->MoveBy(0, dy);
        }
    }
    else
    {
        const int dx = targetPixelInt - m_thumbButton->GetX();
        if (dx != 0)
        {
            m_thumbButton->MoveBy(dx, 0);
        }
    }
}

GUIPlaceholderControl::GUIPlaceholderControl(std::string name, std::string type)
    : GUIControl(std::move(name), std::move(type))
{
}

void GUIPlaceholderControl::SetViewportSize(int width, int height)
{
    (void) width;
    (void) height;
}

void GUIPlaceholderControl::Update(float deltaSeconds)
{
    (void) deltaSeconds;
}

void GUIPlaceholderControl::ResetAnimation()
{
}

void GUIPlaceholderControl::RenderToBuffer(SpriteBuffer *buffer) const
{
    (void) buffer;
}

bool GUIPlaceholderControl::OnMouseMove(int x, int y)
{
    (void) x;
    (void) y;
    return false;
}

bool GUIPlaceholderControl::OnLeftMouseDown(int x, int y)
{
    return ContainsPoint(x, y);
}

bool GUIPlaceholderControl::OnLeftMouseUp(int x, int y)
{
    (void) x;
    (void) y;
    return false;
}

namespace
{
int QueryIntAttributeOrDefault(const tinyxml2::XMLElement *element, const char *name, int fallback)
{
    if (element == nullptr)
    {
        return fallback;
    }

    int value = fallback;
    element->QueryIntAttribute(name, &value);
    return value;
}

std::string ToUpperLocal(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::toupper(ch));
    });
    return text;
}

const tinyxml2::XMLElement *FindState(const tinyxml2::XMLElement *controlElement, const char *name)
{
    if (controlElement == nullptr)
    {
        return nullptr;
    }

    for (const tinyxml2::XMLElement *child = controlElement->FirstChildElement();
         child != nullptr;
         child = child->NextSiblingElement())
    {
        if (ToUpperLocal(child->Name()) == ToUpperLocal(name))
        {
            return child;
        }
    }

    return nullptr;
}

bool HasFontAttribute(const tinyxml2::XMLElement *imageElement)
{
    return imageElement != nullptr && imageElement->Attribute("font") != nullptr;
}
}

bool GUIManager::Load(TextureManager *textureManager, const std::string &guiXmlPath, SoundSystem *soundSystem,
                      ShaderLibrary *shaderLibrary)
{
    m_textureManager = textureManager;
    m_shaderLibrary = shaderLibrary;
    m_soundSystem = soundSystem;
    m_forms.clear();
    m_predefines.clear();
    m_defaultDefines.clear();
    m_placeholderControlCount = 0;

    if (m_textureManager == nullptr)
    {
        return false;
    }

    m_ownedStyles = std::make_unique<LegacyImageStyleLibrary>();
    m_styles = m_ownedStyles.get();

    if (!LoadConfigFile(guiXmlPath))
    {
        return false;
    }

    if (!LoadAllForms())
    {
        return false;
    }

    SetViewportSize(m_viewportWidth, m_viewportHeight);
    return !m_forms.empty();
}

void GUIManager::Unload()
{
    m_forms.clear();
    m_controlCallbacks.clear();
    m_predefines.clear();
    m_defaultDefines.clear();
    m_shadersByName.clear();
    m_ownedStyles.reset();
    m_styles = nullptr;
    m_textureManager = nullptr;
    m_shaderLibrary = nullptr;
    m_soundSystem = nullptr;
    m_overSoundName.clear();
    m_clickSoundName.clear();
    m_placeholderControlCount = 0;
    m_prevLeftButtonDown = false;
    m_prevMouseX = -1;
    m_prevMouseY = -1;
    m_dirty = false;
    m_focusedControl = nullptr;
    m_focusedFormName.clear();
    m_pointerSuppressed = false;
    m_lastRealMouseX = -100000;
    m_lastRealMouseY = -100000;
}

void GUIManager::HideAllForms()
{
    for (GUIForm &form : m_forms)
    {
        form.visible = false;
    }
    m_dirty = true;
    ClearFocus();
}

bool GUIManager::ShowForm(const std::string &name)
{
    for (GUIForm &form : m_forms)
    {
        if (form.name == name)
        {
            form.visible = true;
            form.showOrder = ++m_nextShowOrder;
            m_dirty = true;

            for (const std::unique_ptr<LegacyImage> &image : form.images)
            {
                if (image != nullptr)
                {
                    image->ResetAnimation();
                }
            }

            for (const std::unique_ptr<GUIControl> &control : form.controls)
            {
                if (control != nullptr)
                {
                    control->ResetAnimation();
                }
            }

            // Only claim focus if nothing else is currently focused; a form shown on top of
            // an already-interactive one (e.g. an OptionsMenu tab) must not steal focus.
            if (m_focusedControl == nullptr)
            {
                // Re-enable the pointer so it isn't left parked off-screen from a previous
                // navigation, then prefer whatever control is actually under it.
                m_pointerSuppressed = false;

                std::vector<GUIControl *> focusTargets;
                for (const std::unique_ptr<GUIControl> &control : form.controls)
                {
                    if (control != nullptr)
                    {
                        control->CollectFocusTargets(focusTargets);
                    }
                }

                GUIControl *hoveredControl = nullptr;
                for (GUIControl *target : focusTargets)
                {
                    if (target->IsEnabled() && target->ContainsPoint(m_lastRealMouseX, m_lastRealMouseY))
                    {
                        hoveredControl = target;
                        break;
                    }
                }

                if (hoveredControl != nullptr)
                {
                    SetFocusedControl(hoveredControl, form.name);
                }
                else if (m_gamepadNavigationEnabled)
                {
                    // Without gamepad navigation, focus should only ever come from the mouse;
                    // don't invent a focus target that isn't actually under the pointer.
                    for (GUIControl *target : focusTargets)
                    {
                        if (target->IsEnabled())
                        {
                            SetFocusedControl(target, form.name);
                            break;
                        }
                    }
                }
            }

            return true;
        }
    }

    return false;
}

bool GUIManager::HideForm(const std::string &name)
{
    for (GUIForm &form : m_forms)
    {
        if (form.name == name)
        {
            form.visible = false;
            m_dirty = true;

            if (m_focusedFormName == name)
            {
                ClearFocus();
            }

            return true;
        }
    }

    return false;
}

void GUIManager::SetViewportSize(int width, int height)
{
    if (m_viewportWidth != std::max(1, width) || m_viewportHeight != std::max(1, height))
    {
        m_dirty = true;
    }

    m_viewportWidth = std::max(1, width);
    m_viewportHeight = std::max(1, height);

    for (GUIForm &form : m_forms)
    {
        for (std::unique_ptr<LegacyImage> &image : form.images)
        {
            if (image != nullptr)
            {
                image->SetViewportSize(m_viewportWidth, m_viewportHeight);
            }
        }

        for (std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr)
            {
                control->SetViewportSize(m_viewportWidth, m_viewportHeight);
            }
        }
    }
}

void GUIManager::Update(float deltaSeconds)
{
#ifdef SDL3_NEW_GUI_HOT_RELOAD
    CheckHotReload(deltaSeconds);
#endif

    for (GUIForm &form : m_forms)
    {
        if (!form.visible)
        {
            continue;
        }

        for (std::unique_ptr<LegacyImage> &image : form.images)
        {
            if (image != nullptr)
            {
                image->Update(deltaSeconds);
            }
        }

        for (std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr)
            {
                control->Update(deltaSeconds);
            }
        }
    }
}

void GUIManager::ProcessMouse(int x, int y, bool leftButtonDown)
{
    m_lastRealMouseX = x;
    m_lastRealMouseY = y;

    if (m_pointerSuppressed)
    {
        const int offsetX = x - m_pointerSuppressOriginX;
        const int offsetY = y - m_pointerSuppressOriginY;
        constexpr int kWakeThresholdPixelsSquared = 4 * 4;
        if ((offsetX * offsetX) + (offsetY * offsetY) >= kWakeThresholdPixelsSquared)
        {
            m_pointerSuppressed = false;
        }
        else
        {
            // Keep reporting the pointer as off-screen so it can't re-hover a control and
            // steal focus back from whatever the player just navigated to.
            x = -100000;
            y = -100000;
            leftButtonDown = false;
        }
    }

    if (x != m_prevMouseX || y != m_prevMouseY || leftButtonDown != m_prevLeftButtonDown)
    {
        m_dirty = true;
    }

    for (GUIForm &form : m_forms)
    {
        if (!form.visible)
        {
            continue;
        }

        for (std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr)
            {
                const bool wasHovered = control->IsHovered();
                m_dirty = control->OnMouseMove(x, y) || m_dirty;
                const bool nowHovered = control->IsHovered();

                if (!wasHovered && nowHovered)
                {
                    SetFocusedControl(control.get(), form.name);
                }
                else if (wasHovered && !nowHovered && !m_gamepadNavigationEnabled &&
                         m_focusedControl == control.get())
                {
                    // Without gamepad navigation, a control should only look focused while
                    // the mouse is actually over it, unlike the "sticky" nav-driven focus.
                    ClearFocus();
                }

                // Scrollbar dragging changes its value continuously, well before any mouse-up.
                if (control->ConsumeValueChanged())
                {
                    InvokeControlCallbacks(form.name, control->GetName());
                }
            }
        }
    }

    if (leftButtonDown && !m_prevLeftButtonDown)
    {
        for (auto formIt = m_forms.rbegin(); formIt != m_forms.rend(); ++formIt)
        {
            if (!formIt->visible)
            {
                continue;
            }

            for (auto controlIt = formIt->controls.rbegin(); controlIt != formIt->controls.rend(); ++controlIt)
            {
                if (*controlIt != nullptr && (*controlIt)->OnLeftMouseDown(x, y))
                {
                    m_dirty = true;

                    // Clicking a scrollbar's track pages its value immediately, not on release.
                    if ((*controlIt)->ConsumeValueChanged())
                    {
                        InvokeControlCallbacks(formIt->name, (*controlIt)->GetName());
                    }

                    m_prevMouseX = x;
                    m_prevMouseY = y;
                    m_prevLeftButtonDown = leftButtonDown;
                    return;
                }
            }
        }
    }

    if (!leftButtonDown && m_prevLeftButtonDown)
    {
        for (GUIForm &form : m_forms)
        {
            if (!form.visible)
            {
                continue;
            }

            for (std::unique_ptr<GUIControl> &control : form.controls)
            {
                if (control != nullptr)
                {
                    m_dirty = control->OnLeftMouseUp(x, y) || m_dirty;

                    if (control->ConsumeButtonHit())
                    {
                        PlayClickSound();
                        InvokeControlCallbacks(form.name, control->GetName());
                    }
                }
            }
        }
    }

    m_prevMouseX = x;
    m_prevMouseY = y;
    m_prevLeftButtonDown = leftButtonDown;
}

bool GUIManager::RegisterButtonCallback(const std::string &formName,
                                        const std::string &controlName,
                                        void *owner,
                                        ButtonCallback callback)
{
    if (formName.empty() || controlName.empty() || owner == nullptr || !callback)
    {
        return false;
    }

    const std::string key = MakeControlKey(formName, controlName);
    std::vector<ControlCallbackEntry> &entries = m_controlCallbacks[key];
    for (ControlCallbackEntry &entry : entries)
    {
        if (entry.owner == owner)
        {
            entry.callback = std::move(callback);
            return true;
        }
    }

    entries.push_back(ControlCallbackEntry{ owner, std::move(callback) });
    return true;
}

void GUIManager::UnregisterCallbacksForOwner(void *owner)
{
    if (owner == nullptr)
    {
        return;
    }

    for (auto it = m_controlCallbacks.begin(); it != m_controlCallbacks.end();)
    {
        std::vector<ControlCallbackEntry> &entries = it->second;
        entries.erase(std::remove_if(entries.begin(),
                                     entries.end(),
                                     [owner](const ControlCallbackEntry &entry)
                                     {
                                         return entry.owner == owner;
                                     }),
                      entries.end());

        if (entries.empty())
        {
            it = m_controlCallbacks.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool GUIManager::SetControlEnabled(const std::string &formName, const std::string &controlName, bool enabled)
{
    for (GUIForm &form : m_forms)
    {
        if (form.name != formName)
        {
            continue;
        }

        for (std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr && control->GetName() == controlName)
            {
                control->SetEnabled(enabled);
                m_dirty = true;
                return true;
            }
        }
    }

    return false;
}

bool GUIManager::SetControlChecked(const std::string &formName, const std::string &controlName, bool checked)
{
    for (GUIForm &form : m_forms)
    {
        if (form.name != formName)
        {
            continue;
        }

        for (std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr && control->GetName() == controlName)
            {
                control->SetChecked(checked);
                m_dirty = true;
                return true;
            }
        }
    }

    return false;
}

bool GUIManager::GetControlChecked(const std::string &formName, const std::string &controlName) const
{
    for (const GUIForm &form : m_forms)
    {
        if (form.name != formName)
        {
            continue;
        }

        for (const std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr && control->GetName() == controlName)
            {
                return control->IsChecked();
            }
        }
    }

    return false;
}

bool GUIManager::SetControlText(const std::string &formName, const std::string &controlName, const std::string &text)
{
    const std::string resolvedText = Translation::Instance().Resolve(text);
    for (GUIForm &form : m_forms)
    {
        if (form.name != formName)
        {
            continue;
        }

        for (std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr && control->GetName() == controlName)
            {
                const bool result = control->SetText(resolvedText);
                if (result)
                {
                    m_dirty = true;
                }
                return result;
            }
        }
    }

    return false;
}

bool GUIManager::SetControlScrollRange(const std::string &formName, const std::string &controlName, int minValue, int maxValue)
{
    for (GUIForm &form : m_forms)
    {
        if (form.name != formName)
        {
            continue;
        }

        for (std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr && control->GetName() == controlName)
            {
                control->SetScrollRange(minValue, maxValue);
                m_dirty = true;
                return true;
            }
        }
    }

    return false;
}

bool GUIManager::SetControlScrollValue(const std::string &formName, const std::string &controlName, int value)
{
    for (GUIForm &form : m_forms)
    {
        if (form.name != formName)
        {
            continue;
        }

        for (std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr && control->GetName() == controlName)
            {
                control->SetScrollValue(value);
                m_dirty = true;
                return true;
            }
        }
    }

    return false;
}

bool GUIManager::SetControlScrollJumps(const std::string &formName, const std::string &controlName, int lineJump, int pageJump)
{
    for (GUIForm &form : m_forms)
    {
        if (form.name != formName)
        {
            continue;
        }

        for (std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr && control->GetName() == controlName)
            {
                control->SetScrollJumps(lineJump, pageJump);
                return true;
            }
        }
    }

    return false;
}

int GUIManager::GetControlScrollValue(const std::string &formName, const std::string &controlName) const
{
    for (const GUIForm &form : m_forms)
    {
        if (form.name != formName)
        {
            continue;
        }

        for (const std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control != nullptr && control->GetName() == controlName)
            {
                return control->GetScrollValue();
            }
        }
    }

    return 0;
}

void GUIManager::SetFocusedControl(GUIControl *control, const std::string &formName)
{
    if (m_focusedControl == control)
    {
        return;
    }

    if (m_focusedControl != nullptr)
    {
        m_focusedControl->SetFocused(false);
    }

    m_focusedControl = control;
    m_focusedFormName = (control != nullptr) ? formName : std::string();

    if (m_focusedControl != nullptr)
    {
        m_focusedControl->SetFocused(true);
        PlayOverSound();
    }

    m_dirty = true;
}

void GUIManager::ClearFocus()
{
    SetFocusedControl(nullptr, std::string());
}

void GUIManager::InvokeControlCallbacks(const std::string &formName, const std::string &controlName)
{
    const std::string callbackKey = MakeControlKey(formName, controlName);
    const auto callbackIt = m_controlCallbacks.find(callbackKey);
    if (callbackIt == m_controlCallbacks.end())
    {
        return;
    }

    for (const ControlCallbackEntry &entry : callbackIt->second)
    {
        if (entry.callback)
        {
            entry.callback();
        }
    }
}

void GUIManager::NavigateFocus(int dx, int dy)
{
    struct Candidate
    {
        GUIControl *control;
        std::string formName;
        float centerX;
        float centerY;
        bool enabled;
    };

    std::vector<Candidate> candidates;
    for (GUIForm &form : m_forms)
    {
        if (!form.visible)
        {
            continue;
        }

        for (std::unique_ptr<GUIControl> &control : form.controls)
        {
            if (control == nullptr)
            {
                continue;
            }

            // A disabled control can still be the current focus (e.g. the active OptionsMenu
            // tab), so it stays a valid navigation reference point even though it can't be
            // navigated *to*; that exclusion happens below when picking a new target.
            std::vector<GUIControl *> focusTargets;
            control->CollectFocusTargets(focusTargets);
            for (GUIControl *target : focusTargets)
            {
                candidates.push_back(Candidate{
                    target,
                    form.name,
                    static_cast<float>(target->GetX()) + static_cast<float>(target->GetWidth()) * 0.5f,
                    static_cast<float>(target->GetY()) + static_cast<float>(target->GetHeight()) * 0.5f,
                    target->IsEnabled() });
            }
        }
    }

    if (candidates.empty())
    {
        return;
    }

    const Candidate *focused = nullptr;
    if (m_focusedControl != nullptr)
    {
        for (const Candidate &candidate : candidates)
        {
            if (candidate.control == m_focusedControl)
            {
                focused = &candidate;
                break;
            }
        }
    }

    // Nothing focused yet (or the focused control is no longer a valid candidate,
    // e.g. its form was hidden): fall back to the first available enabled control.
    if (focused == nullptr)
    {
        for (const Candidate &candidate : candidates)
        {
            if (candidate.enabled)
            {
                SetFocusedControl(candidate.control, candidate.formName);
                SuppressPointerUntilMoved();
                return;
            }
        }
        return;
    }

    const float dirX = static_cast<float>(dx);
    const float dirY = static_cast<float>(dy);

    const Candidate *best = nullptr;
    float bestScore = 0.0f;

    for (const Candidate &candidate : candidates)
    {
        if (candidate.control == focused->control || !candidate.enabled)
        {
            continue;
        }

        const float offsetX = candidate.centerX - focused->centerX;
        const float offsetY = candidate.centerY - focused->centerY;

        // Distance along the pressed direction; skip anything behind the focused control.
        const float primary = (offsetX * dirX) + (offsetY * dirY);
        if (primary <= 0.0f)
        {
            continue;
        }

        // Perpendicular distance, used to keep candidates within a ~45 degree cone ahead.
        const float perpendicular = std::fabs((offsetX * dirY) - (offsetY * dirX));
        if (perpendicular > primary)
        {
            continue;
        }

        const float score = primary + (perpendicular * 2.0f);
        if (best == nullptr || score < bestScore)
        {
            best = &candidate;
            bestScore = score;
        }
    }

    if (best != nullptr)
    {
        SetFocusedControl(best->control, best->formName);
        SuppressPointerUntilMoved();
    }
}

void GUIManager::SuppressPointerUntilMoved()
{
    m_pointerSuppressed = true;
    m_pointerSuppressOriginX = m_lastRealMouseX;
    m_pointerSuppressOriginY = m_lastRealMouseY;
}

void GUIManager::ActivateFocusedControl()
{
    if (m_focusedControl == nullptr || !m_focusedControl->IsEnabled())
    {
        return;
    }

    PlayClickSound();
    m_focusedControl->Activate();
    GUIControl *callbackControl = m_focusedControl->GetActivationCallbackTarget();
    InvokeControlCallbacks(m_focusedFormName, callbackControl->GetName());
}

void GUIManager::UpdateGamepadNavigation(InputManager &inputManager, float deltaSeconds)
{
    if (!m_gamepadActionsRegistered)
    {
        m_actionGuiUp = inputManager.RegisterAction("GUI_UP", SDL_SCANCODE_UP, "joy0_axis1_up");
        m_actionGuiDown = inputManager.RegisterAction("GUI_DOWN", SDL_SCANCODE_DOWN, "joy0_axis1_down");
        m_actionGuiLeft = inputManager.RegisterAction("GUI_LEFT", SDL_SCANCODE_LEFT, "joy0_axis0_left");
        m_actionGuiRight = inputManager.RegisterAction("GUI_RIGHT", SDL_SCANCODE_RIGHT, "joy0_axis0_right");
        m_actionGuiAccept = inputManager.RegisterAction("GUI_ACCEPT", SDL_SCANCODE_RETURN, "joy0_button0");
        m_gamepadActionsRegistered = true;
    }

    if (!m_gamepadNavigationEnabled)
    {
        return;
    }

    // Watchdog: a noisy/stuck analog axis can spam direction presses far faster than any
    // human input; treat a burst like that as a hardware fault and shut navigation off.
    // NOTE: temporarily lowered to 5/sec for testing with rapid manual keypresses.
    constexpr float kNavRateWindowSeconds = 1.0f;
    constexpr int kNavRateFaultThreshold = 5;

    m_navRateWindowTimer += deltaSeconds;
    if (m_navRateWindowTimer >= kNavRateWindowSeconds)
    {
        m_navRateWindowTimer = 0.0f;
        m_navRateWindowCount = 0;
    }

    const bool up = inputManager.IsJustPressed(m_actionGuiUp);
    const bool down = inputManager.IsJustPressed(m_actionGuiDown);
    const bool left = inputManager.IsJustPressed(m_actionGuiLeft);
    const bool right = inputManager.IsJustPressed(m_actionGuiRight);

    if (up || down || left || right)
    {
        ++m_navRateWindowCount;
        if ((m_navRateWindowCount > kNavRateFaultThreshold) && m_navigationFaultMonitoringEnabled )
        {
            m_gamepadNavigationEnabled = false;
            m_gamepadNavigationFault = true;
            m_gamepadNavigationLocked = true;
            m_navRateWindowCount = 0;
            m_navRateWindowTimer = 0.0f;
            return;
        }

        if (up)
        {
            NavigateFocus(0, -1);
        }
        else if (down)
        {
            NavigateFocus(0, 1);
        }
        else if (left)
        {
            NavigateFocus(-1, 0);
        }
        else if (right)
        {
            NavigateFocus(1, 0);
        }
    }

    if (inputManager.IsJustPressed(m_actionGuiAccept))
    {
        ActivateFocusedControl();
    }
}

void GUIManager::SetGamepadNavigationEnabled(bool enabled)
{
    if (enabled && m_gamepadNavigationLocked)
    {
        // A watchdog fault must be cleared explicitly (ClearGamepadNavigationFault) before
        // navigation can be turned back on; ignore routine re-enable calls until then.
        return;
    }

    // The system-wide preference always wins over a module's routine re-enable call.
    m_gamepadNavigationEnabled = enabled && m_gamepadNavigationPreferenceEnabled;

    if (m_gamepadNavigationEnabled)
    {
        // Give a manual re-enable a clean slate instead of inheriting a stale rate count.
        m_navRateWindowTimer = 0.0f;
        m_navRateWindowCount = 0;
    }
}

bool GUIManager::IsGamepadNavigationEnabled() const
{
    return m_gamepadNavigationEnabled;
}

void GUIManager::SetGamepadNavigationPreferenceEnabled(bool enabled)
{
    m_gamepadNavigationPreferenceEnabled = enabled;

    if (enabled)
    {
        // Re-checking the box is an explicit user override; clear any watchdog lock so it
        // actually takes effect instead of being silently ignored by SetGamepadNavigationEnabled.
        m_gamepadNavigationLocked = false;
        m_gamepadNavigationFault = false;
    }

    // Apply immediately rather than waiting for the next module to call SetGamepadNavigationEnabled(true).
    SetGamepadNavigationEnabled(enabled);
}

bool GUIManager::IsGamepadNavigationPreferenceEnabled() const
{
    return m_gamepadNavigationPreferenceEnabled;
}

void GUIManager::ClearGamepadNavigationFault()
{
    m_gamepadNavigationLocked = false;
    m_gamepadNavigationFault = false;
    SetGamepadNavigationEnabled(true);
}

bool GUIManager::ConsumeGamepadNavigationFault()
{
    const bool hadFault = m_gamepadNavigationFault;
    m_gamepadNavigationFault = false;
    return hadFault;
}

bool GUIManager::ConsumeDirtyFlag()
{
    const bool wasDirty = m_dirty;
    m_dirty = false;
    return wasDirty;
}

void GUIManager::RenderToBuffer(SpriteBuffer *buffer) const
{
    if (buffer == nullptr)
    {
        return;
    }

    std::vector<const GUIForm *> visibleForms;
    for (const GUIForm &form : m_forms)
    {
        if (form.visible)
        {
            visibleForms.push_back(&form);
        }
    }

    // Draw most-recently-shown forms last so they layer on top of older ones
    // instead of relying on incidental file-load order.
    std::stable_sort(visibleForms.begin(), visibleForms.end(), [](const GUIForm *a, const GUIForm *b)
    {
        return a->showOrder < b->showOrder;
    });

    for (const GUIForm *form : visibleForms)
    {
        for (const std::unique_ptr<LegacyImage> &image : form->images)
        {
            if (image != nullptr)
            {
                image->RenderToBuffer(buffer);
            }
        }

        for (const std::unique_ptr<GUIControl> &control : form->controls)
        {
            if (control != nullptr)
            {
                control->RenderToBuffer(buffer);
            }
        }
    }
}

size_t GUIManager::GetFormCount() const
{
    return m_forms.size();
}

size_t GUIManager::GetPlaceholderControlCount() const
{
    return m_placeholderControlCount;
}

std::string GUIManager::MakeControlKey(const std::string &formName, const std::string &controlName)
{
    return ToLowerCopy(formName) + "::" + ToLowerCopy(controlName);
}

bool GUIManager::LoadConfigFile(const std::string &guiXmlPath)
{
    tinyxml2::XMLDocument document;
    if (document.LoadFile(guiXmlPath.c_str()) != tinyxml2::XML_SUCCESS)
    {
        return false;
    }

    const tinyxml2::XMLElement *root = document.FirstChildElement("GUI");
    if (root == nullptr)
    {
        return false;
    }

    m_guiRootDirectory = std::filesystem::path(guiXmlPath).parent_path().lexically_normal().string();
    m_defaultFontPath = (std::filesystem::path(m_guiRootDirectory) / "../../fonts/F25BankPrinter32.xml").lexically_normal().string();

    if (!ParseStyles(root))
    {
        return false;
    }

    if (!ParseFonts(root))
    {
        return false;
    }

    if (!ParsePredefines(root))
    {
        return false;
    }

    if (!ParseDefaults(root))
    {
        return false;
    }

    if (!ParseSounds(root))
    {
        return false;
    }

    if (!ParseShaders(root))
    {
        return false;
    }

    return true;
}

bool GUIManager::LoadAllForms()
{
    const std::filesystem::path rootPath(m_guiRootDirectory);
    if (!std::filesystem::exists(rootPath))
    {
        return false;
    }

    std::vector<std::filesystem::path> formFiles;
    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(rootPath))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        if (ToLowerCopy(entry.path().extension().string()) != ".xml")
        {
            continue;
        }

        if (ToLowerCopy(entry.path().filename().string()) == "gui.xml")
        {
            continue;
        }

        formFiles.push_back(entry.path());
    }

    std::sort(formFiles.begin(), formFiles.end(), [](const std::filesystem::path &a, const std::filesystem::path &b)
    {
        return a.string() < b.string();
    });

    bool loadedAny = false;
    for (const std::filesystem::path &formPath : formFiles)
    {
        loadedAny = LoadFormFile(formPath.string()) || loadedAny;
    }

    return loadedAny;
}

bool GUIManager::LoadFormFile(const std::string &formPath)
{
    GUIForm form;
    if (!ParseFormFile(formPath, form))
    {
        return false;
    }

    form.sourcePath = formPath;
#ifdef SDL3_NEW_GUI_HOT_RELOAD
    std::error_code errorCode;
    form.lastWriteTime = std::filesystem::last_write_time(formPath, errorCode);
#endif

    m_forms.push_back(std::move(form));
    return true;
}

std::string GUIManager::LoadFormByPath(const std::string &formPath)
{
    for (const GUIForm &form : m_forms)
    {
        if (form.sourcePath == formPath)
        {
            return form.name;
        }
    }

    if (!LoadFormFile(formPath))
    {
        return {};
    }

    return m_forms.back().name;
}

bool GUIManager::ParseFormFile(const std::string &formPath, GUIForm &outForm) const
{
    tinyxml2::XMLDocument document;
    if (document.LoadFile(formPath.c_str()) != tinyxml2::XML_SUCCESS)
    {
        return false;
    }

    const tinyxml2::XMLElement *formElement = document.FirstChildElement("FORM");
    if (formElement == nullptr)
    {
        return false;
    }

    GUIForm form;

    if (const tinyxml2::XMLElement *nameElement = formElement->FirstChildElement("Name"))
    {
        const char *nameAttribute = nameElement->Attribute("name");
        if (nameAttribute != nullptr)
        {
            form.name = nameAttribute;
        }
    }

    const tinyxml2::XMLElement *imagesElement = formElement->FirstChildElement("Images");
    if (imagesElement != nullptr)
    {
        for (const tinyxml2::XMLElement *imageElement = imagesElement->FirstChildElement();
             imageElement != nullptr;
             imageElement = imageElement->NextSiblingElement())
        {
            if (imageElement->Attribute("type") == nullptr)
            {
                continue;
            }

            std::unique_ptr<LegacyImage> image = BuildLegacyImage(imageElement, glm::vec2(0.0f, 0.0f));
            if (image != nullptr)
            {
                form.images.push_back(std::move(image));
            }
        }
    }

    const tinyxml2::XMLElement *controlsElement = formElement->FirstChildElement("Controls");
    if (controlsElement != nullptr)
    {
        for (const tinyxml2::XMLElement *controlElement = controlsElement->FirstChildElement("Control");
             controlElement != nullptr;
             controlElement = controlElement->NextSiblingElement("Control"))
        {
            std::unique_ptr<GUIControl> control = ParseControl(controlElement);
            if (control != nullptr)
            {
                form.controls.push_back(std::move(control));
            }
        }
    }

    outForm = std::move(form);
    return true;
}

#ifdef SDL3_NEW_GUI_HOT_RELOAD
void GUIManager::CheckHotReload(float deltaSeconds)
{
    m_hotReloadAccumulator += deltaSeconds;
    if (m_hotReloadAccumulator < 1.0f)
    {
        return;
    }
    m_hotReloadAccumulator = 0.0f;

    for (GUIForm &form : m_forms)
    {
        if (!form.visible || form.sourcePath.empty())
        {
            continue;
        }

        std::error_code errorCode;
        const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(form.sourcePath, errorCode);
        if (errorCode || writeTime == form.lastWriteTime)
        {
            continue;
        }

        ReloadFormFile(form, writeTime);
    }
}

void GUIManager::ReloadFormFile(GUIForm &form, const std::filesystem::file_time_type &writeTime)
{
    GUIForm reloaded;
    if (!ParseFormFile(form.sourcePath, reloaded))
    {
        // Keep showing the previously loaded contents if the edited file is
        // temporarily malformed (e.g. mid-save) instead of blanking the form.
        return;
    }

    // Controls/images may have been added or removed by the edit; existing button
    // callbacks are keyed by form/control name and stay registered either way.
    form.images = std::move(reloaded.images);
    form.controls = std::move(reloaded.controls);
    form.lastWriteTime = writeTime;

    for (std::unique_ptr<LegacyImage> &image : form.images)
    {
        if (image != nullptr)
        {
            image->SetViewportSize(m_viewportWidth, m_viewportHeight);
        }
    }

    for (std::unique_ptr<GUIControl> &control : form.controls)
    {
        if (control != nullptr)
        {
            control->SetViewportSize(m_viewportWidth, m_viewportHeight);
        }
    }

    m_dirty = true;
}
#endif

bool GUIManager::ParseStyles(const tinyxml2::XMLElement *root)
{
    const tinyxml2::XMLElement *stylesElement = root->FirstChildElement("Styles");
    if (stylesElement == nullptr)
    {
        return true;
    }

    for (const tinyxml2::XMLElement *styleElement = stylesElement->FirstChildElement("Style");
         styleElement != nullptr;
         styleElement = styleElement->NextSiblingElement("Style"))
    {
        const char *styleName = styleElement->Attribute("name");
        if (styleName == nullptr)
        {
            continue;
        }

        const tinyxml2::XMLElement *frameStylesElement = styleElement->FirstChildElement("FrameStyles");
        if (frameStylesElement != nullptr)
        {
            for (const tinyxml2::XMLElement *frameElement = frameStylesElement->FirstChildElement("Frame");
                 frameElement != nullptr;
                 frameElement = frameElement->NextSiblingElement("Frame"))
            {
                const char *name = frameElement->Attribute("name");
                const char *texture = frameElement->Attribute("texture");
                if (name == nullptr || texture == nullptr)
                {
                    continue;
                }

                LegacyImageStyleLibrary::Frame frame{};
                frame.texturePath = ResolveTexturePath(Translation::Instance().Resolve(texture));
                frameElement->QueryIntAttribute("x", &frame.x);
                frameElement->QueryIntAttribute("y", &frame.y);
                frameElement->QueryIntAttribute("width", &frame.tileWidth);
                frameElement->QueryIntAttribute("height", &frame.tileHeight);

                m_styles->RegisterFrame(styleName, name, frame);
            }
        }

        const tinyxml2::XMLElement *iconStylesElement = styleElement->FirstChildElement("IconStyles");
        if (iconStylesElement != nullptr)
        {
            for (const tinyxml2::XMLElement *iconElement = iconStylesElement->FirstChildElement("Icon");
                 iconElement != nullptr;
                 iconElement = iconElement->NextSiblingElement("Icon"))
            {
                const char *name = iconElement->Attribute("name");
                const char *texture = iconElement->Attribute("texture");
                if (name == nullptr || texture == nullptr)
                {
                    continue;
                }

                LegacyImageStyleLibrary::Region region{};
                region.texturePath = ResolveTexturePath(Translation::Instance().Resolve(texture));
                iconElement->QueryIntAttribute("x", &region.x);
                iconElement->QueryIntAttribute("y", &region.y);
                iconElement->QueryIntAttribute("width", &region.width);
                iconElement->QueryIntAttribute("height", &region.height);

                m_styles->RegisterIcon(styleName, name, region);
            }
        }
    }

    return true;
}

bool GUIManager::ParseFonts(const tinyxml2::XMLElement *root)
{
    RegisterFontAliases(m_defaultFontName, m_defaultFontPath);

    const tinyxml2::XMLElement *fontsElement = root->FirstChildElement("Fonts");
    if (fontsElement == nullptr)
    {
        return true;
    }

    for (const tinyxml2::XMLElement *fontElement = fontsElement->FirstChildElement("Font");
         fontElement != nullptr;
         fontElement = fontElement->NextSiblingElement("Font"))
    {
        const char *fileAttribute = fontElement->Attribute("file");
        if (fileAttribute == nullptr)
        {
            continue;
        }

        const std::string normalized = NormalizeSlashes(Translation::Instance().Resolve(fileAttribute));
        const std::string stem = std::filesystem::path(normalized).stem().string();
        std::string target = m_defaultFontPath;

        if (ToLowerCopy(std::filesystem::path(normalized).extension().string()) == ".xml")
        {
            target = ResolveGuiRelativePath(normalized);
        }

        if (!stem.empty())
        {
            RegisterFontAliases(stem, target);
        }
    }

    RegisterFontAliases("arial8pt", m_defaultFontPath);
    RegisterFontAliases("arial10pt", m_defaultFontPath);
    RegisterFontAliases("trebuchet14ptaa", m_defaultFontPath);
    RegisterFontAliases("pristina40ptaa", m_defaultFontPath);

    return true;
}

bool GUIManager::ParsePredefines(const tinyxml2::XMLElement *root)
{
    const tinyxml2::XMLElement *predefinesElement = root->FirstChildElement("Predefines");
    if (predefinesElement == nullptr)
    {
        return true;
    }

    for (const tinyxml2::XMLElement *predefineElement = predefinesElement->FirstChildElement("Predefine");
         predefineElement != nullptr;
         predefineElement = predefineElement->NextSiblingElement("Predefine"))
    {
        const char *name = predefineElement->Attribute("name");
        if (name == nullptr)
        {
            continue;
        }

        PredefineStateImages stateImages;

        auto collectImages = [](const tinyxml2::XMLElement *stateElement)
        {
            std::vector<std::string> snippets;
            if (stateElement == nullptr)
            {
                return snippets;
            }

            for (const tinyxml2::XMLElement *imageElement = stateElement->FirstChildElement("Image");
                 imageElement != nullptr;
                 imageElement = imageElement->NextSiblingElement("Image"))
            {
                tinyxml2::XMLPrinter printer;
                imageElement->Accept(&printer);
                snippets.emplace_back(printer.CStr());
            }

            return snippets;
        };

        stateImages.normal = collectImages(predefineElement->FirstChildElement("Normal"));
        stateImages.over = collectImages(predefineElement->FirstChildElement("Over"));
        stateImages.down = collectImages(predefineElement->FirstChildElement("Down"));
        stateImages.disabled = collectImages(predefineElement->FirstChildElement("Disabled"));
        stateImages.checkedNormal = collectImages(predefineElement->FirstChildElement("CheckedNormal"));
        stateImages.checkedOver = collectImages(predefineElement->FirstChildElement("CheckedOver"));
        stateImages.checkedDown = collectImages(predefineElement->FirstChildElement("CheckedDown"));
        stateImages.checkedDisabled = collectImages(predefineElement->FirstChildElement("CheckedDisabled"));
        predefineElement->QueryIntAttribute("width", &stateImages.width);
        predefineElement->QueryIntAttribute("height", &stateImages.height);

        m_predefines[ToLowerCopy(name)] = std::move(stateImages);
    }

    return true;
}

bool GUIManager::ParseDefaults(const tinyxml2::XMLElement *root)
{
    m_defaultDefines.clear();

    const tinyxml2::XMLElement *defaultsElement = root->FirstChildElement("Defaults");
    if (defaultsElement == nullptr)
    {
        return true;
    }

    for (const tinyxml2::XMLElement *defaultElement = defaultsElement->FirstChildElement("Default");
         defaultElement != nullptr;
         defaultElement = defaultElement->NextSiblingElement("Default"))
    {
        const char *type = defaultElement->Attribute("type");
        if (type == nullptr)
        {
            continue;
        }

        // "style" is accepted as an alias for "define" since both name an entry that supplies
        // a control's imagery; only Predefines actually carry per-state images today.
        const char *defineName = defaultElement->Attribute("define");
        if (defineName == nullptr)
        {
            defineName = defaultElement->Attribute("style");
        }
        if (defineName == nullptr)
        {
            continue;
        }

        m_defaultDefines[ToUpperCopy(type)] = ToLowerCopy(defineName);
    }

    return true;
}

std::string GUIManager::GetDefaultDefineName(const std::string &controlTypeUpper) const
{
    const auto it = m_defaultDefines.find(controlTypeUpper);
    return it != m_defaultDefines.end() ? it->second : std::string();
}

bool GUIManager::ParseSounds(const tinyxml2::XMLElement *root)
{
    m_overSoundName.clear();
    m_clickSoundName.clear();

    const tinyxml2::XMLElement *soundsElement = root->FirstChildElement("Sounds");
    if (soundsElement == nullptr)
    {
        return true;
    }

    for (const tinyxml2::XMLElement *soundElement = soundsElement->FirstChildElement("Sound");
         soundElement != nullptr;
         soundElement = soundElement->NextSiblingElement("Sound"))
    {
        const char *event = soundElement->Attribute("event");
        const char *file = soundElement->Attribute("file");
        if (event == nullptr || file == nullptr)
        {
            continue;
        }

        const std::string eventLower = ToLowerCopy(event);
        const std::string resourceName = "gui_" + eventLower;

        if (m_soundSystem != nullptr)
        {
            SoundSystem::LoadOptions options{};
            m_soundSystem->LoadSound(resourceName, ResolveGuiRelativePath(Translation::Instance().Resolve(file)), options);
        }

        if (eventLower == "over")
        {
            m_overSoundName = resourceName;
        }
        else if (eventLower == "click")
        {
            m_clickSoundName = resourceName;
        }
    }

    return true;
}

bool GUIManager::ParseShaders(const tinyxml2::XMLElement *root)
{
    m_shadersByName.clear();

    const tinyxml2::XMLElement *shadersElement = root->FirstChildElement("Shaders");
    if (shadersElement == nullptr || m_shaderLibrary == nullptr)
    {
        return true;
    }

    for (const tinyxml2::XMLElement *shaderElement = shadersElement->FirstChildElement("Shader");
         shaderElement != nullptr;
         shaderElement = shaderElement->NextSiblingElement("Shader"))
    {
        const char *name = shaderElement->Attribute("name");
        const char *file = shaderElement->Attribute("file");
        if (name == nullptr || file == nullptr)
        {
            continue;
        }

        std::shared_ptr<ShaderLibrary::ShaderResource> resource = m_shaderLibrary->LoadShader(name, Translation::Instance().Resolve(file));
        if (resource != nullptr)
        {
            m_shadersByName[name] = std::move(resource);
        }
    }

    return true;
}

void GUIManager::PlayOverSound()
{
    if (m_soundSystem != nullptr && !m_overSoundName.empty())
    {
        m_soundSystem->PlaySound(m_overSoundName);
    }
}

void GUIManager::PlayClickSound()
{
    if (m_soundSystem != nullptr && !m_clickSoundName.empty())
    {
        m_soundSystem->PlaySound(m_clickSoundName);
    }
}

std::unique_ptr<LegacyImage> GUIManager::BuildLegacyImage(const tinyxml2::XMLElement *imageElement,
                                                           const glm::vec2 &extraOffset) const
{
    if (imageElement == nullptr || m_styles == nullptr || m_textureManager == nullptr)
    {
        return nullptr;
    }

    auto image = std::make_unique<LegacyImage>(m_textureManager, m_shaderLibrary);
    if (image->LoadFromXmlElement(imageElement, *m_styles))
    {
        image->MovePixels(extraOffset);
        image->SetViewportSize(m_viewportWidth, m_viewportHeight);
        return image;
    }

    const char *typeAttribute = imageElement->Attribute("type");
    if (typeAttribute == nullptr || ToUpperCopy(typeAttribute) != "LABEL")
    {
        return nullptr;
    }

    if (HasFontAttribute(imageElement))
    {
        return nullptr;
    }

    tinyxml2::XMLDocument wrapper;
    tinyxml2::XMLElement *newImage = wrapper.NewElement("Image");
    for (const tinyxml2::XMLAttribute *attr = imageElement->FirstAttribute();
         attr != nullptr;
         attr = attr->Next())
    {
        newImage->SetAttribute(attr->Name(), attr->Value());
    }

    newImage->SetAttribute("font", m_defaultFontName.c_str());
    for (const tinyxml2::XMLNode *child = imageElement->FirstChild();
         child != nullptr;
         child = child->NextSibling())
    {
        newImage->InsertEndChild(child->DeepClone(&wrapper));
    }
    wrapper.InsertEndChild(newImage);

    tinyxml2::XMLPrinter printer;
    wrapper.Print(&printer);

    auto fallbackImage = std::make_unique<LegacyImage>(m_textureManager, m_shaderLibrary);
    if (!fallbackImage->LoadFromXmlString(printer.CStr(), *m_styles))
    {
        return nullptr;
    }

    fallbackImage->MovePixels(extraOffset);
    fallbackImage->SetViewportSize(m_viewportWidth, m_viewportHeight);
    return fallbackImage;
}

std::unique_ptr<LegacyImage> GUIManager::BuildLegacyImageFromSnippet(const std::string &xmlSnippet,
                                                                      const glm::vec2 &extraOffset) const
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xmlSnippet.c_str()) != tinyxml2::XML_SUCCESS)
    {
        return nullptr;
    }

    const tinyxml2::XMLElement *imageElement = doc.FirstChildElement("Image");
    if (imageElement == nullptr)
    {
        return nullptr;
    }

    return BuildLegacyImage(imageElement, extraOffset);
}

std::vector<std::unique_ptr<LegacyImage>> GUIManager::ParseStateImages(const tinyxml2::XMLElement *stateElement,
                                                                        const glm::vec2 &controlOffset) const
{
    std::vector<std::unique_ptr<LegacyImage>> images;
    if (stateElement == nullptr)
    {
        return images;
    }

    for (const tinyxml2::XMLElement *imageElement = stateElement->FirstChildElement("Image");
         imageElement != nullptr;
         imageElement = imageElement->NextSiblingElement("Image"))
    {
        std::unique_ptr<LegacyImage> image = BuildLegacyImage(imageElement, controlOffset);
        if (image != nullptr)
        {
            images.push_back(std::move(image));
        }
    }

    return images;
}

std::vector<std::unique_ptr<LegacyImage>> GUIManager::ParseStateImagesFromSnippets(const std::vector<std::string> &snippets,
                                                                                    const glm::vec2 &controlOffset) const
{
    std::vector<std::unique_ptr<LegacyImage>> images;
    images.reserve(snippets.size());

    for (const std::string &snippet : snippets)
    {
        std::unique_ptr<LegacyImage> image = BuildLegacyImageFromSnippet(snippet, controlOffset);
        if (image != nullptr)
        {
            images.push_back(std::move(image));
        }
    }

    return images;
}

std::unique_ptr<GUIButtonControl> GUIManager::BuildButtonControl(const tinyxml2::XMLElement *controlElement,
                                                                 std::string name,
                                                                 const glm::vec2 &position,
                                                                 int width, int height) const
{
    std::vector<std::unique_ptr<LegacyImage>> normal = ParseStateImages(FindState(controlElement, "Normal"), position);
    std::vector<std::unique_ptr<LegacyImage>> over = ParseStateImages(FindState(controlElement, "Over"), position);
    std::vector<std::unique_ptr<LegacyImage>> down = ParseStateImages(FindState(controlElement, "Down"), position);
    std::vector<std::unique_ptr<LegacyImage>> disabled = ParseStateImages(FindState(controlElement, "Disabled"), position);

    const char *defineAttr = controlElement->Attribute("define");
    std::string defineName = defineAttr != nullptr ? ToLowerCopy(defineAttr) : GetDefaultDefineName("BUTTON");
    if (!defineName.empty())
    {
        const auto it = m_predefines.find(defineName);
        if (it != m_predefines.end())
        {
            if (normal.empty())
            {
                normal = ParseStateImagesFromSnippets(it->second.normal, position);
            }
            if (over.empty())
            {
                over = ParseStateImagesFromSnippets(it->second.over, position);
            }
            if (down.empty())
            {
                down = ParseStateImagesFromSnippets(it->second.down, position);
            }
            if (disabled.empty())
            {
                disabled = ParseStateImagesFromSnippets(it->second.disabled, position);
            }
        }
    }

    if (normal.empty() && over.empty() && down.empty() && disabled.empty())
    {
        return nullptr;
    }

    auto control = std::make_unique<GUIButtonControl>(std::move(name),
                                                       std::move(normal),
                                                       std::move(over),
                                                       std::move(down),
                                                       std::move(disabled));
    control->SetBounds(static_cast<int>(position.x), static_cast<int>(position.y), width, height);
    return control;
}

std::unique_ptr<GUIControl> GUIManager::ParseControl(const tinyxml2::XMLElement *controlElement) const
{
    if (controlElement == nullptr)
    {
        return nullptr;
    }

    const std::string controlType = ToUpperCopy(controlElement->Attribute("type") != nullptr ? controlElement->Attribute("type") : "");
    const std::string controlName = controlElement->Attribute("name") != nullptr ? controlElement->Attribute("name") : "";

    const int x = QueryIntAttributeOrDefault(controlElement, "x", 0);
    const int y = QueryIntAttributeOrDefault(controlElement, "y", 0);
    const glm::vec2 controlOffset(static_cast<float>(x), static_cast<float>(y));

    if (controlType == "LABEL")
    {
        std::vector<std::unique_ptr<LegacyImage>> normal = ParseStateImages(FindState(controlElement, "Normal"), controlOffset);
        if (normal.empty())
        {
            if (const tinyxml2::XMLElement *directImage = controlElement->FirstChildElement("Image"))
            {
                std::unique_ptr<LegacyImage> image = BuildLegacyImage(directImage, controlOffset);
                if (image != nullptr)
                {
                    normal.push_back(std::move(image));
                }
            }
        }

        if (normal.empty())
        {
            m_placeholderControlCount += 1;
            auto placeholder = std::make_unique<GUIPlaceholderControl>(controlName, controlType);
            placeholder->SetBounds(x,
                                   y,
                                   QueryIntAttributeOrDefault(controlElement, "width", 0),
                                   QueryIntAttributeOrDefault(controlElement, "height", 0));
            return placeholder;
        }

        auto control = std::make_unique<GUILabelControl>(controlName, std::move(normal));
        control->SetBounds(x,
                           y,
                           QueryIntAttributeOrDefault(controlElement, "width", 0),
                           QueryIntAttributeOrDefault(controlElement, "height", 0));
        return control;
    }

    if (controlType == "BUTTON")
    {
        const int width = QueryIntAttributeOrDefault(controlElement, "width", 0);
        const int height = QueryIntAttributeOrDefault(controlElement, "height", 0);
        std::unique_ptr<GUIButtonControl> control = BuildButtonControl(controlElement, controlName, controlOffset, width, height);
        if (control == nullptr)
        {
            m_placeholderControlCount += 1;
            auto placeholder = std::make_unique<GUIPlaceholderControl>(controlName, controlType);
            placeholder->SetBounds(x, y, width, height);
            return placeholder;
        }

        return control;
    }

    if (controlType == "SCROLLBAR")
    {
        const int width = QueryIntAttributeOrDefault(controlElement, "width", 0);
        const int height = QueryIntAttributeOrDefault(controlElement, "height", 0);

        bool vertical = true;
        const char *orientation = controlElement->Attribute("orientation");
        if (orientation != nullptr && (orientation[0] == 'H' || orientation[0] == 'h'))
        {
            vertical = false;
        }

        auto findButtonElement = [&](const char *wrapperName) -> const tinyxml2::XMLElement *
        {
            const tinyxml2::XMLElement *wrapper = controlElement->FirstChildElement(wrapperName);
            return wrapper != nullptr ? wrapper->FirstChildElement("Control") : nullptr;
        };

        const tinyxml2::XMLElement *decElement = findButtonElement("Button1");
        const tinyxml2::XMLElement *thumbElement = findButtonElement("Button2");
        const tinyxml2::XMLElement *incElement = findButtonElement("Button3");

        std::unique_ptr<GUIButtonControl> decrementButton;
        std::unique_ptr<GUIButtonControl> thumbButton;
        std::unique_ptr<GUIButtonControl> incrementButton;
        int minThumbPixel = 0;
        int maxThumbPixel = 0;

        if (decElement != nullptr && thumbElement != nullptr && incElement != nullptr)
        {
            const int decWidth = QueryIntAttributeOrDefault(decElement, "width", 0);
            const int decHeight = QueryIntAttributeOrDefault(decElement, "height", 0);
            const int thumbWidth = QueryIntAttributeOrDefault(thumbElement, "width", 0);
            const int thumbHeight = QueryIntAttributeOrDefault(thumbElement, "height", 0);
            const int incWidth = QueryIntAttributeOrDefault(incElement, "width", 0);
            const int incHeight = QueryIntAttributeOrDefault(incElement, "height", 0);

            const glm::vec2 decPosition(static_cast<float>(x), static_cast<float>(y));
            const glm::vec2 incPosition = vertical
                ? glm::vec2(static_cast<float>(x), static_cast<float>(y + height - incHeight))
                : glm::vec2(static_cast<float>(x + width - incWidth), static_cast<float>(y));
            // Thumb starts pinned at the minimum end; SetScrollValue() below repositions it
            // once the control's actual range/value are known.
            const glm::vec2 thumbPosition = vertical
                ? glm::vec2(static_cast<float>(x), static_cast<float>(y + decHeight))
                : glm::vec2(static_cast<float>(x + decWidth), static_cast<float>(y));

            decrementButton = BuildButtonControl(decElement, controlName + "_dec", decPosition, decWidth, decHeight);
            thumbButton = BuildButtonControl(thumbElement, controlName + "_thumb", thumbPosition, thumbWidth, thumbHeight);
            incrementButton = BuildButtonControl(incElement, controlName + "_inc", incPosition, incWidth, incHeight);

            minThumbPixel = vertical ? (y + decHeight) : (x + decWidth);
            maxThumbPixel = vertical ? (y + height - incHeight - thumbHeight) : (x + width - incWidth - thumbWidth);
        }

        if (decrementButton == nullptr || thumbButton == nullptr || incrementButton == nullptr)
        {
            m_placeholderControlCount += 1;
            auto placeholder = std::make_unique<GUIPlaceholderControl>(controlName, controlType);
            placeholder->SetBounds(x, y, width, height);
            return placeholder;
        }

        auto control = std::make_unique<GUIScrollBarControl>(controlName,
                                                              vertical,
                                                              std::move(decrementButton),
                                                              std::move(thumbButton),
                                                              std::move(incrementButton),
                                                              minThumbPixel,
                                                              maxThumbPixel);
        control->SetBounds(x, y, width, height);
        control->SetScrollRange(QueryIntAttributeOrDefault(controlElement, "min", 0),
                                QueryIntAttributeOrDefault(controlElement, "max", 100));
        control->SetScrollJumps(QueryIntAttributeOrDefault(controlElement, "linejump", 1),
                                QueryIntAttributeOrDefault(controlElement, "pagejump", 10));
        control->SetScrollValue(QueryIntAttributeOrDefault(controlElement, "value", 0));
        return control;
    }

    if (controlType == "CHECKBOX")
    {
        std::vector<std::unique_ptr<LegacyImage>> normal = ParseStateImages(FindState(controlElement, "Normal"), controlOffset);
        std::vector<std::unique_ptr<LegacyImage>> over = ParseStateImages(FindState(controlElement, "Over"), controlOffset);
        std::vector<std::unique_ptr<LegacyImage>> down = ParseStateImages(FindState(controlElement, "Down"), controlOffset);
        std::vector<std::unique_ptr<LegacyImage>> disabled = ParseStateImages(FindState(controlElement, "Disabled"), controlOffset);
        std::vector<std::unique_ptr<LegacyImage>> checkedNormal = ParseStateImages(FindState(controlElement, "CheckedNormal"), controlOffset);
        std::vector<std::unique_ptr<LegacyImage>> checkedOver = ParseStateImages(FindState(controlElement, "CheckedOver"), controlOffset);
        std::vector<std::unique_ptr<LegacyImage>> checkedDown = ParseStateImages(FindState(controlElement, "CheckedDown"), controlOffset);
        std::vector<std::unique_ptr<LegacyImage>> checkedDisabled = ParseStateImages(FindState(controlElement, "CheckedDisabled"), controlOffset);

        const char *defineAttr = controlElement->Attribute("define");
        std::string defineName = defineAttr != nullptr ? ToLowerCopy(defineAttr) : GetDefaultDefineName("CHECKBOX");
        int predefineWidth = -1;
        int predefineHeight = -1;
        if (!defineName.empty())
        {
            const auto it = m_predefines.find(defineName);
            if (it != m_predefines.end())
            {
                predefineWidth = it->second.width;
                predefineHeight = it->second.height;
                if (normal.empty())
                {
                    normal = ParseStateImagesFromSnippets(it->second.normal, controlOffset);
                }
                if (over.empty())
                {
                    over = ParseStateImagesFromSnippets(it->second.over, controlOffset);
                }
                if (down.empty())
                {
                    down = ParseStateImagesFromSnippets(it->second.down, controlOffset);
                }
                if (disabled.empty())
                {
                    disabled = ParseStateImagesFromSnippets(it->second.disabled, controlOffset);
                }
                if (checkedNormal.empty())
                {
                    checkedNormal = ParseStateImagesFromSnippets(it->second.checkedNormal, controlOffset);
                }
                if (checkedOver.empty())
                {
                    checkedOver = ParseStateImagesFromSnippets(it->second.checkedOver, controlOffset);
                }
                if (checkedDown.empty())
                {
                    checkedDown = ParseStateImagesFromSnippets(it->second.checkedDown, controlOffset);
                }
                if (checkedDisabled.empty())
                {
                    checkedDisabled = ParseStateImagesFromSnippets(it->second.checkedDisabled, controlOffset);
                }
            }
        }

        int width = 0;
        int height = 0;
        const bool hasWidth = controlElement->QueryIntAttribute("width", &width) == tinyxml2::XML_SUCCESS;
        const bool hasHeight = controlElement->QueryIntAttribute("height", &height) == tinyxml2::XML_SUCCESS;
        if (!hasWidth && predefineWidth >= 0)
        {
            width = predefineWidth;
        }
        if (!hasHeight && predefineHeight >= 0)
        {
            height = predefineHeight;
        }

        if (normal.empty() && over.empty() && down.empty() && disabled.empty() &&
            checkedNormal.empty() && checkedOver.empty() && checkedDown.empty() && checkedDisabled.empty())
        {
            m_placeholderControlCount += 1;
            auto placeholder = std::make_unique<GUIPlaceholderControl>(controlName, controlType);
            placeholder->SetBounds(x, y, width, height);
            return placeholder;
        }

        bool initiallyChecked = false;
        controlElement->QueryBoolAttribute("checked", &initiallyChecked);

        auto control = std::make_unique<GUICheckBoxControl>(controlName,
                                                             std::move(normal),
                                                             std::move(over),
                                                             std::move(down),
                                                             std::move(disabled),
                                                             std::move(checkedNormal),
                                                             std::move(checkedOver),
                                                             std::move(checkedDown),
                                                             std::move(checkedDisabled),
                                                             initiallyChecked);
        control->SetBounds(x, y, width, height);
        return control;
    }

    m_placeholderControlCount += 1;
    auto placeholder = std::make_unique<GUIPlaceholderControl>(controlName, controlType.empty() ? "UNKNOWN" : controlType);
    placeholder->SetBounds(x,
                           y,
                           QueryIntAttributeOrDefault(controlElement, "width", 0),
                           QueryIntAttributeOrDefault(controlElement, "height", 0));
    return placeholder;
}

std::string GUIManager::ToUpperCopy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::toupper(ch));
    });
    return text;
}

std::string GUIManager::ToLowerCopy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string GUIManager::NormalizeSlashes(std::string text)
{
    std::replace(text.begin(), text.end(), '\\', '/');
    return text;
}

std::string GUIManager::ResolveGuiRelativePath(const std::string &path) const
{
    const std::string normalized = NormalizeSlashes(path);
    if (normalized.empty())
    {
        return normalized;
    }

    if (normalized[0] == '/')
    {
        return normalized;
    }

    if (normalized.rfind("assets/", 0) == 0)
    {
        const std::filesystem::path rooted =
            (std::filesystem::path(m_guiRootDirectory) / "../../" / normalized.substr(7)).lexically_normal();
        return rooted.string();
    }

    std::filesystem::path candidate = std::filesystem::path(m_guiRootDirectory) / normalized;
    candidate = candidate.lexically_normal();
    if (std::filesystem::exists(candidate))
    {
        return candidate.string();
    }

    const std::filesystem::path byFilename = std::filesystem::path(m_guiRootDirectory) /
                                             std::filesystem::path(normalized).filename();
    if (std::filesystem::exists(byFilename))
    {
        return byFilename.lexically_normal().string();
    }

    return candidate.string();
}

std::string GUIManager::ResolveTexturePath(const std::string &path) const
{
    const std::string normalized = NormalizeSlashes(path);
    if (normalized.empty())
    {
        return normalized;
    }

    if (normalized[0] == '/')
    {
        return normalized;
    }

    if (normalized.rfind("assets/", 0) == 0)
    {
        const std::filesystem::path rooted =
            (std::filesystem::path(m_guiRootDirectory) / "../../" / normalized.substr(7)).lexically_normal();
        return rooted.string();
    }

    std::filesystem::path direct = std::filesystem::path(m_guiRootDirectory) / normalized;
    if (std::filesystem::exists(direct))
    {
        return direct.lexically_normal().string();
    }

    std::filesystem::path inTextures = std::filesystem::path(m_guiRootDirectory) / "Textures" / normalized;
    if (std::filesystem::exists(inTextures))
    {
        return inTextures.lexically_normal().string();
    }

    std::filesystem::path inAssetTextures = std::filesystem::path("assets/gui/GUIFILES/Textures") / normalized;
    return inAssetTextures.lexically_normal().string();
}

void GUIManager::RegisterFontAliases(const std::string &fontName, const std::string &fontXmlPath)
{
    if (fontName.empty())
    {
        return;
    }

    m_styles->RegisterFont(fontName, fontXmlPath);
    m_styles->RegisterFont(ToLowerCopy(fontName), fontXmlPath);
    m_styles->RegisterFont(ToUpperCopy(fontName), fontXmlPath);
}
