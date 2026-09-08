#include "guidexos/appmodel/scroll_view.hpp"

#include "runtime.hpp"
#include "text_validation.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace guidexos::appmodel {
namespace {

constexpr std::size_t kMaximumToolTipBytes = 8U * 1024U;

void SetScrollViewToolTip(const std::shared_ptr<detail::ControlState>& state,
                          std::string text) {
    detail::ValidateUtf8(text);
    if (text.size() > kMaximumToolTipBytes) {
        throw std::length_error("Control ToolTip exceeds the 8 KiB limit");
    }
    if (state->toolTip == text) return;
    state->toolTip = std::move(text);
    detail::NotifyControlChanged(state);
}

} // namespace

ScrollView::ScrollView()
    : state_(std::make_shared<detail::ControlState>(
          detail::ControlKind::ScrollView, std::string{})),
      content_(Orientation::Vertical, 24, 16) {
    state_->scrollContent = content_.state_;
    content_.state_->scrollViewOwner = state_;
}

ScrollView::~ScrollView() = default;

Layout& ScrollView::ContentLayout() noexcept {
    return content_;
}

const Layout& ScrollView::ContentLayout() const noexcept {
    return content_;
}

void ScrollView::SetVerticalOffset(int offset) {
    const int normalized = std::max(0, offset);
    const int effective = state_->viewportKnown
        ? std::clamp(normalized, 0, state_->maximumVerticalOffset)
        : normalized;
    if (state_->requestedVerticalOffset == normalized &&
        state_->verticalOffset == effective) {
        return;
    }
    state_->requestedVerticalOffset = normalized;
    state_->verticalOffset = effective;
    detail::NotifyControlChanged(state_);
}

int ScrollView::GetVerticalOffset() const noexcept {
    return state_->verticalOffset;
}

int ScrollView::GetMaximumVerticalOffset() const noexcept {
    return state_->maximumVerticalOffset;
}

LayoutSize ScrollView::GetViewportSize() const noexcept {
    return state_->viewportSize;
}

LayoutSize ScrollView::GetContentSize() const noexcept {
    if (state_->viewportKnown) return state_->contentSize;
    return content_.GetNaturalSize();
}

void ScrollView::SetToolTip(std::string text) {
    SetScrollViewToolTip(state_, std::move(text));
}

const std::string& ScrollView::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef ScrollView::GetControlRef() const noexcept {
    return ControlRef{state_};
}

bool ScrollView::Focus() const noexcept {
    return detail::FocusControl(state_);
}

void ScrollView::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool ScrollView::IsEnabled() const noexcept {
    return state_->enabled;
}

} // namespace guidexos::appmodel
