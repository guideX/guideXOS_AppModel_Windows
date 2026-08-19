#include "guidexos/appmodel/status_bar.hpp"

#include "runtime.hpp"
#include "text_validation.hpp"

#include <stdexcept>
#include <utility>

namespace guidexos::appmodel {

StatusBar::StatusBar(std::string text)
    : state_(std::make_shared<detail::StatusBarState>()) {
    detail::ValidateUtf8(text);
    state_->text = std::move(text);
}

void StatusBar::SetText(std::string text) {
    if (!state_) throw std::logic_error("StatusBar is invalid");
    detail::ValidateUtf8(text);
    if (state_->text == text) return;
    state_->text = std::move(text);
    detail::NotifyStatusBarChanged(state_);
}

std::string StatusBar::GetText() const {
    if (!state_) throw std::logic_error("StatusBar is invalid");
    return state_->text;
}

} // namespace guidexos::appmodel
