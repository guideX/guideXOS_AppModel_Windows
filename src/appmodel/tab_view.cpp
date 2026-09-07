#include "guidexos/appmodel/tab_view.hpp"

#include "runtime.hpp"
#include "text_validation.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace guidexos::appmodel {
namespace {

constexpr std::size_t kMaximumToolTipBytes = 8U * 1024U;

void ValidateTabTitle(const std::string& title) {
    detail::ValidateUtf8(title);
}

void SetTabViewToolTip(const std::shared_ptr<detail::ControlState>& state,
                       std::string text) {
    detail::ValidateUtf8(text);
    if (text.size() > kMaximumToolTipBytes) {
        throw std::length_error("Control ToolTip exceeds the 8 KiB limit");
    }
    if (state->toolTip == text) return;
    state->toolTip = std::move(text);
    detail::NotifyControlChanged(state);
}

std::optional<std::size_t> FindPageIndex(
    const std::shared_ptr<detail::TabPageState>& page) noexcept {
    if (!page) return std::nullopt;
    const auto owner = page->owner.lock();
    if (!owner) return std::nullopt;
    for (std::size_t index = 0; index < owner->pages.size(); ++index) {
        if (owner->pages[index] == page) return index;
    }
    return std::nullopt;
}

} // namespace

bool TabPageRef::IsValid() const noexcept {
    return !state_.expired();
}

std::string TabPageRef::GetTitle() const {
    const auto state = state_.lock();
    return state ? state->title : std::string{};
}

std::optional<std::size_t> TabPageRef::GetIndex() const noexcept {
    return FindPageIndex(state_.lock());
}

Layout TabPage::GetLayout() const noexcept {
    if (!state_ || !state_->layout) return Layout{};
    return Layout{state_->layout};
}

void TabPage::SetTitle(std::string title) {
    if (!state_) throw std::logic_error("TabPage is invalid");
    ValidateTabTitle(title);
    if (state_->title == title) return;
    state_->title = std::move(title);
    if (const auto owner = state_->owner.lock()) {
        if (const auto control = owner->control.lock()) {
            detail::NotifyControlChanged(control);
        }
    }
}

const std::string& TabPage::GetTitle() const noexcept {
    static const std::string empty;
    return state_ ? state_->title : empty;
}

std::optional<std::size_t> TabPage::GetIndex() const noexcept {
    return FindPageIndex(state_);
}

TabPageRef TabPage::GetTabPageRef() const noexcept {
    return TabPageRef{state_};
}

TabView::TabView()
    : state_(std::make_shared<detail::ControlState>(
          detail::ControlKind::TabView, std::string{})) {
    state_->tabView = std::make_shared<detail::TabViewState>();
    state_->tabView->control = state_;
}

TabView::~TabView() {
    state_->onSelectionChanged = {};
}

TabPage TabView::AddTab(std::string title) {
    ValidateTabTitle(title);
    const auto tabView = state_->tabView;
    auto pageLayout = std::make_shared<detail::LayoutState>(
        Orientation::Vertical, 24, 16);
    auto page = std::make_shared<detail::TabPageState>(
        std::move(title), pageLayout);
    page->owner = tabView;
    pageLayout->tabPage = page;
    tabView->pages.push_back(page);

    if (const auto application = state_->application.lock()) {
        detail::BindLayoutToApplication(pageLayout, application);
    }

    if (!state_->selectedIndex) {
        detail::DispatchSelectionChanged(state_, 0);
    } else {
        detail::NotifyControlChanged(state_);
    }
    return TabPage{std::move(page)};
}

TabPage TabView::GetTab(std::size_t index) const {
    if (index >= state_->tabView->pages.size()) {
        throw std::out_of_range("TabView tab index is out of range");
    }
    return TabPage{state_->tabView->pages[index]};
}

std::size_t TabView::GetTabCount() const noexcept {
    return state_->tabView->pages.size();
}

void TabView::SetSelectedIndex(std::optional<std::size_t> index) {
    if (index && *index >= state_->tabView->pages.size()) {
        throw std::out_of_range("TabView selection index is out of range");
    }
    detail::DispatchSelectionChanged(state_, index);
}

std::optional<std::size_t> TabView::GetSelectedIndex() const noexcept {
    return state_->selectedIndex;
}

void TabView::SetToolTip(std::string text) {
    SetTabViewToolTip(state_, std::move(text));
}

const std::string& TabView::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef TabView::GetControlRef() const noexcept {
    return ControlRef{state_};
}

bool TabView::Focus() const noexcept {
    return detail::FocusControl(state_);
}

void TabView::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool TabView::IsEnabled() const noexcept {
    return state_->enabled;
}

void TabView::OnSelectionChanged(
    std::function<void(std::optional<std::size_t>)> callback) {
    state_->onSelectionChanged = std::move(callback);
}

} // namespace guidexos::appmodel
