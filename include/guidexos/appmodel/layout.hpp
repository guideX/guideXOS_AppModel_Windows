#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace guidexos::appmodel {

class Button;
class CheckBox;
class ComboBox;
class Label;
class ListBox;
class RadioButton;
class TextBox;

namespace detail {
struct LayoutState;
}

enum class Orientation {
    Vertical,
    Horizontal,
};

enum class LayoutSizing {
    Natural,
    Expand,
};

// Platform-neutral logical geometry. The Windows backend converts this to
// native coordinates privately; applications and future backends do not need
// to know about native handle or coordinate types.
struct LayoutRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Platform-neutral logical size in non-negative integer units. The backend
// may derive this size from native metrics, but the unit itself is not scale-
// or platform-specific.
struct LayoutSize {
    int width = 0;
    int height = 0;
};

class Layout final {
public:
    explicit Layout(Orientation orientation = Orientation::Vertical,
                    int padding = 24,
                    int spacing = 16);

    // Layout is a small shared value: copying it shares the model, so a
    // temporary may safely be passed to Window::SetContent().
    Layout(const Layout&) = default;
    Layout& operator=(const Layout&) = default;
    Layout(Layout&&) noexcept = default;
    Layout& operator=(Layout&&) noexcept = default;
    ~Layout() = default;

    void Add(Label& label, LayoutSizing sizing = LayoutSizing::Natural);
    void Add(Button& button, LayoutSizing sizing = LayoutSizing::Natural);
    void Add(CheckBox& checkBox, LayoutSizing sizing = LayoutSizing::Natural);
    void Add(ComboBox& comboBox, LayoutSizing sizing = LayoutSizing::Natural);
    // ListBox keeps the legacy vertical behavior by expanding by default;
    // pass Natural explicitly when a content-sized list is desired.
    void Add(ListBox& listBox, LayoutSizing sizing = LayoutSizing::Expand);
    void Add(RadioButton& radioButton,
             LayoutSizing sizing = LayoutSizing::Natural);
    void Add(TextBox& textBox, LayoutSizing sizing = LayoutSizing::Natural);
    void Add(Layout& layout, LayoutSizing sizing = LayoutSizing::Natural);

    // A spacer participates in sizing and positioning but never creates a
    // native child control. Expand is the useful toolbar/flexible-space form.
    void AddSpacer(LayoutSizing sizing = LayoutSizing::Expand);

    void SetSpacing(int spacing);
    int GetSpacing() const noexcept;
    void SetPadding(int padding);
    int GetPadding() const noexcept;

    // Natural size is the preferred size of this layout when its children are
    // given their natural sizes. Minimum size is the smallest meaningful
    // recursive composition before constrained-space clipping is required.
    LayoutSize GetNaturalSize() const;
    LayoutSize GetMinimumSize() const;

    // Returns one rectangle per direct child, in insertion order. A nested
    // Layout's rectangle is its assigned outer rectangle; call that Layout's
    // CalculateGeometry() to inspect its own children.
    std::vector<LayoutRect> CalculateGeometry(LayoutRect bounds) const;

    std::size_t ChildCount() const noexcept;

private:
    std::shared_ptr<detail::LayoutState> state_;

    friend class Window;
};

} // namespace guidexos::appmodel
