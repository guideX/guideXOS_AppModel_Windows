#include "guidexos/appmodel/layout.hpp"

#include "guidexos/appmodel/controls.hpp"
#include "runtime.hpp"

#include <algorithm>
#include <stdexcept>

namespace guidexos::appmodel {
namespace {

template <typename ControlStatePointer>
void AddControlToLayout(const std::shared_ptr<detail::LayoutState>& layout,
                        const ControlStatePointer& control,
                        LayoutSizing sizing) {
    if (!control) throw std::logic_error("A null control cannot be added");

    const auto alreadyAdded = std::find_if(
        layout->children.begin(), layout->children.end(),
        [&control](const detail::LayoutItem& item) {
            return item.control == control;
        });
    if (alreadyAdded != layout->children.end()) {
        throw std::logic_error("A control cannot be added to a Layout twice");
    }
    if (auto parent = control->layoutParent.lock()) {
        (void)parent;
        throw std::logic_error("A control cannot belong to multiple Layouts");
    }

    layout->children.push_back(detail::LayoutItem{control, nullptr, sizing, false});
    control->layoutParent = layout;
    try {
        if (auto application = layout->application.lock()) {
            detail::BindLayoutToApplication(layout, application);
        }
    } catch (...) {
        control->layoutParent.reset();
        layout->children.pop_back();
        throw;
    }

    if (layout->application.lock()) detail::NotifyControlChanged(control);
}

bool IsAncestor(const std::shared_ptr<detail::LayoutState>& candidate,
                const std::shared_ptr<detail::LayoutState>& layout) {
    for (auto current = layout; current; current = current->parent.lock()) {
        if (current == candidate) return true;
    }
    return false;
}

} // namespace

Layout::Layout(Orientation orientation, int padding, int spacing)
    : state_(std::make_shared<detail::LayoutState>(
          orientation, std::max(0, padding), std::max(0, spacing))) {}

void Layout::Add(Label& label, LayoutSizing sizing) {
    AddControlToLayout(state_, label.state_, sizing);
}

void Layout::Add(Button& button, LayoutSizing sizing) {
    AddControlToLayout(state_, button.state_, sizing);
}

void Layout::Add(CheckBox& checkBox, LayoutSizing sizing) {
    AddControlToLayout(state_, checkBox.state_, sizing);
}

void Layout::Add(ComboBox& comboBox, LayoutSizing sizing) {
    AddControlToLayout(state_, comboBox.state_, sizing);
}

void Layout::Add(ListBox& listBox, LayoutSizing sizing) {
    AddControlToLayout(state_, listBox.state_, sizing);
}

void Layout::Add(ProgressBar& progressBar, LayoutSizing sizing) {
    AddControlToLayout(state_, progressBar.state_, sizing);
}

void Layout::Add(Slider& slider, LayoutSizing sizing) {
    AddControlToLayout(state_, slider.state_, sizing);
}

void Layout::Add(RadioButton& radioButton, LayoutSizing sizing) {
    AddControlToLayout(state_, radioButton.state_, sizing);
}

void Layout::Add(TextBox& textBox, LayoutSizing sizing) {
    AddControlToLayout(state_, textBox.state_, sizing);
}

void Layout::Add(TextArea& textArea, LayoutSizing sizing) {
    AddControlToLayout(state_, textArea.state_, sizing);
}

void Layout::Add(Layout& layout, LayoutSizing sizing) {
    if (layout.state_ == state_ || IsAncestor(layout.state_, state_)) {
        throw std::logic_error("A Layout cannot contain itself or an ancestor");
    }
    if (layout.state_->parent.lock()) {
        throw std::logic_error("A child Layout cannot belong to multiple parents");
    }
    if (layout.state_->contentWindow.lock()) {
        throw std::logic_error("A realized window content Layout cannot be nested");
    }
    if (std::find_if(state_->children.begin(), state_->children.end(),
                     [&layout](const detail::LayoutItem& item) {
                         return item.layout == layout.state_;
                     }) != state_->children.end()) {
        throw std::logic_error("A Layout cannot be added to a parent twice");
    }

    state_->children.push_back(detail::LayoutItem{nullptr, layout.state_, sizing, false});
    layout.state_->parent = state_;
    try {
        if (auto application = state_->application.lock()) {
            detail::BindLayoutToApplication(state_, application);
        }
    } catch (...) {
        layout.state_->parent.reset();
        state_->children.pop_back();
        throw;
    }

    detail::NotifyLayoutChanged(state_);
}

void Layout::AddSpacer(LayoutSizing sizing) {
    state_->children.push_back(detail::LayoutItem{nullptr, nullptr, sizing, true});
    detail::NotifyLayoutChanged(state_);
}

void Layout::SetSpacing(int spacing) {
    const int normalized = std::max(0, spacing);
    if (state_->spacing == normalized) return;
    state_->spacing = normalized;
    detail::NotifyLayoutChanged(state_);
}

int Layout::GetSpacing() const noexcept {
    return state_->spacing;
}

void Layout::SetPadding(int padding) {
    const int normalized = std::max(0, padding);
    if (state_->padding == normalized) return;
    state_->padding = normalized;
    detail::NotifyLayoutChanged(state_);
}

int Layout::GetPadding() const noexcept {
    return state_->padding;
}

LayoutSize Layout::GetNaturalSize() const {
    return detail::GetLayoutMeasurement(state_).natural;
}

LayoutSize Layout::GetMinimumSize() const {
    return detail::GetLayoutMeasurement(state_).minimum;
}

std::vector<LayoutRect> Layout::CalculateGeometry(LayoutRect bounds) const {
    return detail::CalculateDirectLayoutGeometry(state_, bounds);
}

std::size_t Layout::ChildCount() const noexcept {
    return state_->children.size();
}

} // namespace guidexos::appmodel
