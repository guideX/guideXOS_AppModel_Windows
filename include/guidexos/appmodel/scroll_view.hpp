#pragma once

#include "controls.hpp"
#include "layout.hpp"

#include <memory>
#include <string>

namespace guidexos::appmodel {

namespace detail {
struct ControlState;
}

// A portable vertical viewport. The nested content Layout remains a normal
// AppModel composition tree; Windows supplies only the private clipped host
// and scrollbar realization.
class ScrollView final {
public:
    ScrollView();
    ~ScrollView();

    ScrollView(const ScrollView&) = delete;
    ScrollView& operator=(const ScrollView&) = delete;
    ScrollView(ScrollView&&) = delete;
    ScrollView& operator=(ScrollView&&) = delete;

    Layout& ContentLayout() noexcept;
    const Layout& ContentLayout() const noexcept;

    void SetVerticalOffset(int offset);
    int GetVerticalOffset() const noexcept;
    int GetMaximumVerticalOffset() const noexcept;
    LayoutSize GetViewportSize() const noexcept;
    LayoutSize GetContentSize() const noexcept;

    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;
    bool Focus() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

private:
    std::shared_ptr<detail::ControlState> state_;
    Layout content_;

    friend class Layout;
};

} // namespace guidexos::appmodel
