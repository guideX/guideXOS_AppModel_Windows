#pragma once

#include "controls.hpp"
#include "layout.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace guidexos::appmodel {

namespace detail {
struct TabPageState;
}

// A TabPageRef is a non-owning, platform-neutral identity for a page. Page
// removal is intentionally not part of this milestone, so an existing page
// reference remains stable for the lifetime of its TabView.
class TabPageRef final {
public:
    TabPageRef() noexcept = default;

    bool IsValid() const noexcept;
    std::string GetTitle() const;
    std::optional<std::size_t> GetIndex() const noexcept;

private:
    explicit TabPageRef(std::weak_ptr<detail::TabPageState> state) noexcept
        : state_(std::move(state)) {}

    std::weak_ptr<detail::TabPageState> state_;

    friend class TabPage;
};

// A TabPage is a shared logical page handle. Its nested Layout is the page's
// content root and uses the same controls and layout rules as Window content.
class TabPage final {
public:
    TabPage() noexcept = default;

    Layout GetLayout() const noexcept;
    void SetTitle(std::string title);
    const std::string& GetTitle() const noexcept;
    std::optional<std::size_t> GetIndex() const noexcept;
    TabPageRef GetTabPageRef() const noexcept;

private:
    explicit TabPage(std::shared_ptr<detail::TabPageState> state) noexcept
        : state_(std::move(state)) {}

    std::shared_ptr<detail::TabPageState> state_;

    friend class TabView;
};

class TabView final {
public:
    TabView();
    ~TabView();

    TabView(const TabView&) = delete;
    TabView& operator=(const TabView&) = delete;
    TabView(TabView&&) = delete;
    TabView& operator=(TabView&&) = delete;

    TabPage AddTab(std::string title);
    TabPage GetTab(std::size_t index) const;
    std::size_t GetTabCount() const noexcept;

    void SetSelectedIndex(std::optional<std::size_t> index);
    std::optional<std::size_t> GetSelectedIndex() const noexcept;

    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;
    bool Focus() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

    // Replaces the current callback. Passing an empty callback unsubscribes.
    // The callback executes synchronously after the model selection is
    // current and receives the new optional page index.
    void OnSelectionChanged(
        std::function<void(std::optional<std::size_t>)> callback);

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

} // namespace guidexos::appmodel
