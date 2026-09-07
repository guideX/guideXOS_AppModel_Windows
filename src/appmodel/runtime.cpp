#include "runtime.hpp"

#include "guidexos/appmodel/window.hpp"
#include "platform/platform_backend.hpp"
#include "file_drop.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace guidexos::appmodel::detail {
namespace {

template <typename Callback>
void ForEachLiveWindow(const std::shared_ptr<ApplicationState>& application,
                       Callback&& callback) {
    if (!application) return;

    auto& windows = application->windows;
    windows.erase(std::remove_if(windows.begin(), windows.end(),
                                 [](const std::weak_ptr<WindowState>& window) {
                                     return window.expired();
                                 }),
                  windows.end());

    for (const auto& weakWindow : windows) {
        if (auto window = weakWindow.lock()) callback(window);
    }
}

bool LayoutContainsControl(const std::shared_ptr<LayoutState>& layout,
                           const std::shared_ptr<ControlState>& control) {
    if (!layout || !control) return false;
    for (const auto& item : layout->children) {
        if (item.control == control) return true;
        if (item.layout && LayoutContainsControl(item.layout, control)) return true;
    }
    return false;
}

bool LayoutContainsLayout(const std::shared_ptr<LayoutState>& layout,
                          const std::shared_ptr<LayoutState>& target) {
    if (!layout || !target) return false;
    if (layout == target) return true;
    for (const auto& item : layout->children) {
        if (item.layout && LayoutContainsLayout(item.layout, target)) return true;
    }
    return false;
}

bool IsBoundToAnotherApplication(
    const std::weak_ptr<ApplicationState>& owner,
    const std::shared_ptr<ApplicationState>& application) {
    if (auto existing = owner.lock()) return existing != application;
    return false;
}

bool IsRadioButton(const std::shared_ptr<ControlState>& control) noexcept {
    return control && control->kind == ControlKind::RadioButton;
}

bool IsSelectionControl(const std::shared_ptr<ControlState>& control) noexcept {
    return control && (control->kind == ControlKind::ListBox ||
                       control->kind == ControlKind::ComboBox);
}

int SaturatingInt(std::int64_t value) noexcept {
    if (value <= 0) return 0;
    return value >= std::numeric_limits<int>::max()
        ? std::numeric_limits<int>::max()
        : static_cast<int>(value);
}

int SaturatingAdd(int left, int right) noexcept {
    return SaturatingInt(static_cast<std::int64_t>(left) + right);
}

int SaturatingCoordinate(std::int64_t value) noexcept {
    if (value <= std::numeric_limits<int>::min()) {
        return std::numeric_limits<int>::min();
    }
    if (value >= std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(value);
}

int SaturatingCoordinateAdd(int left, int right) noexcept {
    return SaturatingCoordinate(static_cast<std::int64_t>(left) + right);
}

int SaturatingMultiply(int left, int right) noexcept {
    if (left <= 0 || right <= 0) return 0;
    return SaturatingInt(static_cast<std::int64_t>(left) * right);
}

int SaturatingCountMultiply(int value, std::size_t count) noexcept {
    if (value <= 0 || count == 0) return 0;
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return SaturatingMultiply(value, static_cast<int>(count));
}

int NonNegative(int value) noexcept {
    return std::max(0, value);
}

LayoutSize NormalizeSize(LayoutSize size) noexcept {
    return {NonNegative(size.width), NonNegative(size.height)};
}

LayoutMeasurement NormalizeMeasurement(LayoutMeasurement measurement) noexcept {
    measurement.minimum = NormalizeSize(measurement.minimum);
    measurement.natural = NormalizeSize(measurement.natural);
    measurement.natural.width = std::max(measurement.natural.width,
                                         measurement.minimum.width);
    measurement.natural.height = std::max(measurement.natural.height,
                                          measurement.minimum.height);
    return measurement;
}

std::size_t CountTextUnits(const std::string& text) noexcept {
    // Control text is validated at the public boundary. Counting UTF-8 lead
    // bytes keeps non-ASCII labels from being measured as four-byte glyphs;
    // the fallback for malformed internal state remains deterministic.
    std::size_t count = 0;
    for (const unsigned char byte : text) {
        if ((byte & 0xC0U) != 0x80U) ++count;
    }
    return count;
}

int NeutralTextWidth(const std::string& text, int horizontalPadding) noexcept {
    constexpr int characterWidth = 8;
    const std::size_t count = CountTextUnits(text);
    const int boundedCount = count > static_cast<std::size_t>(
                                  std::numeric_limits<int>::max())
        ? std::numeric_limits<int>::max()
        : static_cast<int>(count);
    return SaturatingAdd(SaturatingMultiply(boundedCount, characterWidth),
                         horizontalPadding);
}

LayoutMeasurement NeutralControlMeasurement(const ControlState& control) {
    constexpr int labelHeight = 22;
    constexpr int buttonHeight = 30;
    constexpr int choiceHeight = 22;
    constexpr int textBoxHeight = 28;
    constexpr int textAreaHeight = 140;

    switch (control.kind) {
    case ControlKind::Label:
        return {{NeutralTextWidth(control.text, 8), labelHeight}, {0, labelHeight}};
    case ControlKind::Button:
        return {{std::max(80, NeutralTextWidth(control.text, 24)), buttonHeight},
                {64, buttonHeight}};
    case ControlKind::CheckBox:
    case ControlKind::RadioButton:
        return {{std::max(32, NeutralTextWidth(control.text, 28)), choiceHeight},
                {28, choiceHeight}};
    case ControlKind::TextBox:
        return {{180, textBoxHeight}, {72, textBoxHeight}};
    case ControlKind::TextArea:
        return {{220, textAreaHeight}, {96, 48}};
    case ControlKind::ListBox:
        return {{220, 140}, {96, 48}};
    case ControlKind::ComboBox:
        return {{220, 28}, {112, 28}};
    case ControlKind::ProgressBar:
        return {{220, 22}, {96, 22}};
    case ControlKind::Slider:
        return {{220, 32}, {96, 24}};
    }
    return {};
}

LayoutMeasurement MeasureControl(const std::shared_ptr<ControlState>& control,
                                 const LayoutMeasurementProvider* provider) {
    if (!control) return {};
    if (provider) return NormalizeMeasurement(provider->Measure(*control));
    return NormalizeMeasurement(NeutralControlMeasurement(*control));
}

LayoutMeasurement MeasureLayout(const std::shared_ptr<LayoutState>& layout,
                                const LayoutMeasurementProvider* provider) {
    if (!layout) return {};

    const int padding = NonNegative(layout->padding);
    const int spacing = NonNegative(layout->spacing);
    const int paddingExtent = SaturatingMultiply(padding, 2);
    int naturalMain = paddingExtent;
    int naturalCross = 0;
    int minimumMain = paddingExtent;
    int minimumCross = 0;

    for (const auto& item : layout->children) {
        LayoutMeasurement measurement;
        if (item.control) measurement = MeasureControl(item.control, provider);
        else if (item.layout) measurement = MeasureLayout(item.layout, provider);

        const bool vertical = layout->orientation == Orientation::Vertical;
        const int childNaturalMain = vertical ? measurement.natural.height
                                              : measurement.natural.width;
        const int childNaturalCross = vertical ? measurement.natural.width
                                                : measurement.natural.height;
        const int childMinimumMain = vertical ? measurement.minimum.height
                                               : measurement.minimum.width;
        const int childMinimumCross = vertical ? measurement.minimum.width
                                                : measurement.minimum.height;
        naturalMain = SaturatingAdd(naturalMain, childNaturalMain);
        naturalCross = std::max(naturalCross, childNaturalCross);
        minimumMain = SaturatingAdd(minimumMain, childMinimumMain);
        minimumCross = std::max(minimumCross, childMinimumCross);
    }

    const std::size_t gapCount = layout->children.size() > 1
        ? layout->children.size() - 1 : 0;
    const int totalSpacing = SaturatingCountMultiply(spacing, gapCount);
    naturalMain = SaturatingAdd(naturalMain, totalSpacing);
    minimumMain = SaturatingAdd(minimumMain, totalSpacing);
    naturalCross = SaturatingAdd(naturalCross, paddingExtent);
    minimumCross = SaturatingAdd(minimumCross, paddingExtent);

    if (layout->orientation == Orientation::Vertical) {
        return {{naturalCross, naturalMain}, {minimumCross, minimumMain}};
    }
    return {{naturalMain, naturalCross}, {minimumMain, minimumCross}};
}

void ReduceTowardMinimum(std::vector<int>& sizes,
                         const std::vector<int>& minimums,
                         const std::vector<std::size_t>& indexes,
                         int& deficit) {
    while (deficit > 0) {
        std::size_t activeCount = 0;
        for (const std::size_t index : indexes) {
            if (sizes[index] > minimums[index]) ++activeCount;
        }
        if (activeCount == 0) return;

        const std::size_t rawShare =
            (static_cast<std::size_t>(deficit) + activeCount - 1) / activeCount;
        const int share = rawShare > static_cast<std::size_t>(
                              std::numeric_limits<int>::max())
            ? std::numeric_limits<int>::max()
            : static_cast<int>(rawShare);
        for (const std::size_t index : indexes) {
            if (deficit == 0) break;
            const int capacity = sizes[index] - minimums[index];
            if (capacity <= 0) continue;
            const int reduction = std::min(capacity, share);
            sizes[index] -= reduction;
            deficit -= reduction;
        }
    }
}

void ClipBelowMinimum(std::vector<int>& sizes, int available) {
    int remaining = available;
    for (int& size : sizes) {
        size = std::min(size, remaining);
        remaining -= size;
    }
}

int SafeCoordinateSpan(int origin, int extent) noexcept {
    if (extent <= 0) return 0;
    const std::int64_t maxExtent =
        static_cast<std::int64_t>(std::numeric_limits<int>::max()) - origin;
    if (maxExtent <= 0) return 0;
    return std::min(extent, SaturatingInt(maxExtent));
}

int SafeLengthToEnd(int start, int end, int requested) noexcept {
    if (requested <= 0 || end <= start) return 0;
    const std::int64_t available = static_cast<std::int64_t>(end) - start;
    if (available <= 0) return 0;
    return std::min(requested, SaturatingInt(available));
}

std::vector<LayoutRect> ComputeDirectGeometry(
    const std::shared_ptr<LayoutState>& layout, LayoutRect bounds,
    const LayoutMeasurementProvider* provider) {
    std::vector<LayoutRect> result;
    if (!layout) return result;

    bounds.width = std::max(0, bounds.width);
    bounds.height = std::max(0, bounds.height);
    result.resize(layout->children.size());
    if (layout->children.empty()) return result;

    const int padding = std::max(0, layout->padding);
    const int spacing = std::max(0, layout->spacing);
    const bool vertical = layout->orientation == Orientation::Vertical;
    const int availableMain = vertical ? bounds.height : bounds.width;
    const int availableCross = vertical ? bounds.width : bounds.height;
    const int paddingExtent = SaturatingMultiply(padding, 2);
    const int innerMain = availableMain <= paddingExtent
        ? 0 : availableMain - paddingExtent;
    const int innerCross = availableCross <= paddingExtent
        ? 0 : availableCross - paddingExtent;
    const std::size_t gapCount = layout->children.size() > 1
        ? layout->children.size() - 1 : 0;
    const int totalSpacing = SaturatingCountMultiply(spacing, gapCount);
    const int usableMain = std::max(0, innerMain - totalSpacing);

    std::vector<int> sizes(layout->children.size(), 0);
    std::vector<int> minimums(layout->children.size(), 0);
    int naturalTotal = 0;
    std::vector<std::size_t> expandingIndexes;
    std::vector<std::size_t> naturalIndexes;
    expandingIndexes.reserve(layout->children.size());
    naturalIndexes.reserve(layout->children.size());
    for (std::size_t index = 0; index < layout->children.size(); ++index) {
        const auto& item = layout->children[index];
        LayoutMeasurement measurement;
        if (item.control) measurement = MeasureControl(item.control, provider);
        else if (item.layout) measurement = MeasureLayout(item.layout, provider);
        const int naturalMain = vertical ? measurement.natural.height
                                         : measurement.natural.width;
        const int minimumMain = vertical ? measurement.minimum.height
                                         : measurement.minimum.width;
        sizes[index] = naturalMain;
        minimums[index] = minimumMain;
        naturalTotal = SaturatingAdd(naturalTotal, naturalMain);
        if (item.sizing == LayoutSizing::Expand) {
            expandingIndexes.push_back(index);
        } else {
            naturalIndexes.push_back(index);
        }
    }

    if (naturalTotal <= usableMain) {
        int remaining = usableMain - naturalTotal;
        if (!expandingIndexes.empty()) {
            const std::size_t count = expandingIndexes.size();
            const int each = static_cast<int>(
                static_cast<std::size_t>(remaining) / count);
            std::size_t remainder = static_cast<std::size_t>(remaining) % count;
            for (const std::size_t index : expandingIndexes) {
                sizes[index] = SaturatingAdd(sizes[index], each);
                if (remainder > 0) {
                    sizes[index] = SaturatingAdd(sizes[index], 1);
                    --remainder;
                }
            }
        }
    } else {
        int deficit = naturalTotal - usableMain;
        // Expand means natural size plus a share of extra space, so expanding
        // children are the first to give up preferred space under pressure.
        ReduceTowardMinimum(sizes, minimums, expandingIndexes, deficit);
        ReduceTowardMinimum(sizes, minimums, naturalIndexes, deficit);
        if (deficit > 0) ClipBelowMinimum(sizes, usableMain);
    }

    // When padding is larger than the available axis, keep the zero-sized
    // content edge inside the containing rectangle. This also avoids using
    // the non-negative size saturator for an offset coordinate, which would
    // otherwise corrupt layouts calculated from a negative logical origin.
    const int mainPadding = std::min(padding, availableMain);
    const int crossPadding = std::min(padding, availableCross);
    const int innerMainOrigin = vertical
        ? SaturatingCoordinateAdd(bounds.y, mainPadding)
        : SaturatingCoordinateAdd(bounds.x, mainPadding);
    const int innerCrossOrigin = vertical
        ? SaturatingCoordinateAdd(bounds.x, crossPadding)
        : SaturatingCoordinateAdd(bounds.y, crossPadding);
    const int innerMainEnd = SaturatingCoordinateAdd(innerMainOrigin, innerMain);
    const int safeInnerCross = SafeCoordinateSpan(innerCrossOrigin, innerCross);
    int cursor = innerMainOrigin;
    for (std::size_t index = 0; index < layout->children.size(); ++index) {
        const int start = std::clamp(cursor, innerMainOrigin, innerMainEnd);
        const int length = SafeLengthToEnd(start, innerMainEnd,
                                           std::max(0, sizes[index]));
        if (vertical) {
            result[index] = {innerCrossOrigin, start, safeInnerCross, length};
        } else {
            result[index] = {start, innerCrossOrigin, length, safeInnerCross};
        }
        cursor = SaturatingCoordinateAdd(
            cursor, SaturatingAdd(sizes[index], spacing));
    }
    return result;
}

void CollectControlPlacements(const std::shared_ptr<LayoutState>& layout,
                              LayoutRect bounds,
                              const LayoutMeasurementProvider* provider,
                              std::vector<ControlPlacement>& placements) {
    const auto rectangles = ComputeDirectGeometry(layout, bounds, provider);
    if (!layout) return;
    for (std::size_t index = 0; index < layout->children.size(); ++index) {
        const auto& item = layout->children[index];
        if (item.control) {
            placements.push_back(ControlPlacement{item.control, rectangles[index]});
        } else if (item.layout) {
            CollectControlPlacements(item.layout, rectangles[index], provider,
                                     placements);
        }
    }
}

void ValidateLayoutBinding(const std::shared_ptr<LayoutState>& layout,
                           const std::shared_ptr<ApplicationState>& application,
                           std::unordered_set<LayoutState*>& visiting,
                           std::unordered_set<LayoutState*>& visited) {
    if (!layout) return;
    if (visited.find(layout.get()) != visited.end()) return;
    if (!visiting.insert(layout.get()).second) {
        throw std::logic_error("A Layout cannot contain a cycle");
    }
    if (IsBoundToAnotherApplication(layout->application, application)) {
        throw std::logic_error("A Layout cannot be used by multiple Applications");
    }

    for (const auto& item : layout->children) {
        if (item.control) {
            if (IsBoundToAnotherApplication(item.control->application, application)) {
                throw std::logic_error("A control cannot be moved between Applications");
            }
            if (auto group = item.control->radioGroup.lock()) {
                if (IsBoundToAnotherApplication(group->application, application)) {
                    throw std::logic_error(
                        "A RadioGroup cannot be used by multiple Applications");
                }
                for (const auto& weakMember : group->members) {
                    if (auto member = weakMember.lock(); member &&
                        IsBoundToAnotherApplication(member->application, application)) {
                        throw std::logic_error(
                            "A RadioGroup cannot contain controls from multiple Applications");
                    }
                }
            }
        } else if (item.layout) {
            if (auto parent = item.layout->parent.lock(); parent && parent != layout) {
                throw std::logic_error("A child Layout cannot belong to multiple parents");
            }
            ValidateLayoutBinding(item.layout, application, visiting, visited);
        }
    }

    visiting.erase(layout.get());
    visited.insert(layout.get());
}

void ApplyLayoutBinding(const std::shared_ptr<LayoutState>& layout,
                        const std::shared_ptr<ApplicationState>& application) {
    if (!layout) return;
    layout->application = application;
    for (const auto& item : layout->children) {
        if (item.control) {
            item.control->application = application;
            if (auto group = item.control->radioGroup.lock()) {
                group->application = application;
            }
        } else if (item.layout) {
            ApplyLayoutBinding(item.layout, application);
        }
    }
}

} // namespace

ApplicationState::~ApplicationState() {
    if (backend) backend->Shutdown();
}

void RegisterWindow(const std::shared_ptr<ApplicationState>& application,
                    const std::shared_ptr<WindowState>& window) {
    if (!application || !window) return;
    window->application = application;
    application->windows.emplace_back(window);
}

void UnregisterWindow(const std::shared_ptr<WindowState>& window) noexcept {
    if (!window) return;
    auto application = window->application.lock();
    if (!application) return;

    ClearNativeFocus(window, window->focusedControl.lock());

    if (application->backend && window->shown) application->backend->CloseWindow(window);

    if (window->menuBar) {
        DetachMenuBarFromWindow(window->menuBar);
        window->menuBar.reset();
    }
    if (window->statusBar) {
        DetachStatusBarFromWindow(window->statusBar);
        window->statusBar.reset();
    }

    application->windows.erase(
        std::remove_if(application->windows.begin(), application->windows.end(),
                       [&window](const std::weak_ptr<WindowState>& candidate) {
                           auto locked = candidate.lock();
                           return !locked || locked == window;
                       }),
        application->windows.end());
}

void NotifyWindowChanged(const std::shared_ptr<WindowState>& window) {
    if (!window) return;
    if (auto application = window->application.lock()) {
        if (application->backend && !application->shutdown && window->shown) {
            application->backend->RefreshWindow(window);
        }
    }
}

void NotifyWindowSizeChanged(const std::shared_ptr<WindowState>& window) {
    if (!window) return;
    if (auto application = window->application.lock()) {
        if (application->backend && !application->shutdown && window->shown) {
            application->backend->ResizeWindow(window);
        }
    }
}

void NotifyLayoutChanged(const std::shared_ptr<LayoutState>& layout) {
    if (!layout) return;
    if (auto application = layout->application.lock()) {
        if (!application->backend || application->shutdown) return;
        ForEachLiveWindow(application, [&layout, &application](
                               const std::shared_ptr<WindowState>& window) {
            if (window->shown && LayoutContainsLayout(window->content, layout)) {
                application->backend->RefreshWindow(window);
            }
        });
    }
}

void NotifyControlChanged(const std::shared_ptr<ControlState>& control) {
    if (!control) return;
    if (auto application = control->application.lock()) {
        if (!application->backend || application->shutdown) return;
        ForEachLiveWindow(application, [&control, &application](
                               const std::shared_ptr<WindowState>& window) {
            if (window->shown && LayoutContainsControl(window->content, control)) {
                application->backend->RefreshWindow(window);
            }
        });
    }
}

void NotifyStatusBarChanged(
    const std::shared_ptr<StatusBarState>& statusBar) {
    if (!statusBar) return;
    if (auto window = statusBar->window.lock()) NotifyWindowChanged(window);
}

void BindLayoutToApplication(const std::shared_ptr<LayoutState>& layout,
                             const std::shared_ptr<ApplicationState>& application) {
    if (!layout || !application) return;
    std::unordered_set<LayoutState*> visiting;
    std::unordered_set<LayoutState*> visited;
    ValidateLayoutBinding(layout, application, visiting, visited);
    ApplyLayoutBinding(layout, application);
}

void BindRadioGroupToApplication(
    const std::shared_ptr<RadioGroupState>& group,
    const std::shared_ptr<ApplicationState>& application) {
    if (!group || !application) return;
    if (IsBoundToAnotherApplication(group->application, application)) {
        throw std::logic_error("A RadioGroup cannot be used by multiple Applications");
    }
    for (const auto& weakMember : group->members) {
        if (auto member = weakMember.lock(); member &&
            IsBoundToAnotherApplication(member->application, application)) {
            throw std::logic_error(
                "A RadioGroup cannot contain controls from multiple Applications");
        }
    }
    group->application = application;
}

void BindStatusBarToApplication(
    const std::shared_ptr<StatusBarState>& statusBar,
    const std::shared_ptr<ApplicationState>& application,
    const std::shared_ptr<WindowState>& window) {
    if (!statusBar || !application || !window) {
        throw std::logic_error("StatusBar attachment is invalid");
    }
    if (IsBoundToAnotherApplication(statusBar->application, application)) {
        throw std::logic_error(
            "A StatusBar cannot be used by multiple Applications");
    }
    if (auto owner = statusBar->window.lock(); owner && owner != window) {
        throw std::logic_error(
            "A StatusBar can be attached to only one Window");
    }
    statusBar->application = application;
    statusBar->window = window;
}

void DetachStatusBarFromWindow(
    const std::shared_ptr<StatusBarState>& statusBar) noexcept {
    if (!statusBar) return;
    statusBar->window.reset();
    statusBar->application.reset();
}

std::vector<LayoutRect> CalculateDirectLayoutGeometry(
    const std::shared_ptr<LayoutState>& layout, LayoutRect bounds,
    const LayoutMeasurementProvider* provider) {
    return ComputeDirectGeometry(layout, bounds, provider);
}

std::vector<ControlPlacement> CalculateControlPlacements(
    const std::shared_ptr<LayoutState>& layout, LayoutRect bounds,
    const LayoutMeasurementProvider* provider) {
    std::vector<ControlPlacement> placements;
    CollectControlPlacements(layout, bounds, provider, placements);
    return placements;
}

LayoutMeasurement GetControlMeasurement(
    const std::shared_ptr<ControlState>& control,
    const LayoutMeasurementProvider* provider) {
    return MeasureControl(control, provider);
}

LayoutMeasurement GetNeutralControlMeasurement(const ControlState& control) {
    return NormalizeMeasurement(NeutralControlMeasurement(control));
}

LayoutMeasurement GetLayoutMeasurement(
    const std::shared_ptr<LayoutState>& layout,
    const LayoutMeasurementProvider* provider) {
    return MeasureLayout(layout, provider);
}

void CollectControls(const std::shared_ptr<LayoutState>& layout,
                     std::vector<std::shared_ptr<ControlState>>& controls) {
    if (!layout) return;
    for (const auto& item : layout->children) {
        if (item.control) controls.push_back(item.control);
        else if (item.layout) CollectControls(item.layout, controls);
    }
}

bool ShowWindow(const std::shared_ptr<WindowState>& window) {
    if (!window) return false;
    if (window->shown) return true;
    if (auto application = window->application.lock()) {
        if (application->backend && !application->shutdown &&
            application->backend->ShowWindow(window)) {
            window->shown = true;
            window->closeState = WindowCloseState::Open;
            return true;
        }
    }
    return false;
}

void RequestWindowClose(const std::shared_ptr<WindowState>& window) noexcept {
    if (!window || !window->shown ||
        window->closeState != WindowCloseState::Open) {
        return;
    }

    const auto application = window->application.lock();
    if (!application || !application->backend || application->shutdown) {
        window->shown = false;
        window->closeState = WindowCloseState::Closed;
        return;
    }

    window->closeState = WindowCloseState::CloseRequested;
    WindowClosingEvent event;
    std::function<void(WindowClosingEvent&)> callback;
    try {
        callback = window->onClosing;
    } catch (...) {
        window->closeState = WindowCloseState::Open;
        RequestQuit(application, -1);
        return;
    }
    if (callback) {
        try {
            callback(event);
        } catch (...) {
            // Do not let an exception escape a native window procedure or
            // Window::Close()'s noexcept contract. Keep this window open and
            // terminate the event loop with an error instead.
            event.Cancel();
            RequestQuit(application, -1);
        }
    }

    if (!window->shown || window->closeState != WindowCloseState::CloseRequested) {
        return;
    }
    if (event.IsCanceled()) {
        window->closeState = WindowCloseState::Open;
        return;
    }

    window->closeState = WindowCloseState::Closing;
    application->backend->CloseWindow(window);
}

void CloseWindow(const std::shared_ptr<WindowState>& window) noexcept {
    if (!window) return;
    if (auto application = window->application.lock()) {
        if (application->backend && !application->shutdown) {
            application->backend->CloseWindow(window);
            return;
        }
    }
    window->shown = false;
    window->closeState = WindowCloseState::Closed;
}

bool IsWindowShown(const std::shared_ptr<WindowState>& window) noexcept {
    return window && window->shown;
}

namespace {

std::shared_ptr<WindowState> FindOwningWindow(
    const std::shared_ptr<ControlState>& control) noexcept {
    if (!control) return nullptr;
    auto layout = control->layoutParent.lock();
    while (layout) {
        if (auto window = layout->contentWindow.lock()) return window;
        layout = layout->parent.lock();
    }
    return nullptr;
}

void ClearWindowFocusState(const std::shared_ptr<WindowState>& window) noexcept {
    if (!window) return;
    if (auto control = window->focusedControl.lock()) {
        if (auto focusedWindow = control->focusedWindow.lock();
            focusedWindow == window) {
            control->focusedWindow.reset();
        }
    }
    window->focusedControl.reset();
}

} // namespace

std::shared_ptr<ControlState> GetFocusedControl(
    const std::shared_ptr<WindowState>& window) noexcept {
    if (!window || !window->shown) return nullptr;
    const auto control = window->focusedControl.lock();
    if (!control || !control->enabled ||
        control->focusedWindow.lock() != window) {
        if (control) ClearWindowFocusState(window);
        return nullptr;
    }
    return control;
}

bool IsControlFocused(const std::shared_ptr<ControlState>& control) noexcept {
    if (!control || !control->enabled) return false;
    const auto window = control->focusedWindow.lock();
    return window && window->shown &&
           window->focusedControl.lock() == control;
}

void SetNativeFocus(const std::shared_ptr<WindowState>& window,
                    const std::shared_ptr<ControlState>& control) noexcept {
    if (!window || !control || !window->shown || !control->enabled) return;
    if (auto previous = window->focusedControl.lock();
        previous && previous != control) {
        if (previous->focusedWindow.lock() == window) {
            previous->focusedWindow.reset();
        }
    }
    window->focusedControl = control;
    control->focusedWindow = window;
}

void ClearNativeFocus(const std::shared_ptr<WindowState>& window,
                      const std::shared_ptr<ControlState>& control) noexcept {
    if (!window) return;
    if (auto current = window->focusedControl.lock();
        !control || current == control) {
        if (current && current->focusedWindow.lock() == window) {
            current->focusedWindow.reset();
        }
        window->focusedControl.reset();
    }
    if (control && control->focusedWindow.lock() == window) {
        control->focusedWindow.reset();
    }
}

bool FocusControl(const std::shared_ptr<ControlState>& control) noexcept {
    if (!control || control->kind == ControlKind::Label ||
        control->kind == ControlKind::ProgressBar || !control->enabled) {
        return false;
    }
    const auto window = FindOwningWindow(control);
    if (!window || !window->shown) return false;
    const auto application = window->application.lock();
    if (!application || application->shutdown || !application->backend) {
        return false;
    }
    return application->backend->FocusControl(window, control);
}

std::size_t GetLiveWindowCount(
    const std::shared_ptr<ApplicationState>& application) noexcept {
    if (!application) return 0;

    std::size_t count = 0;
    for (const auto& weakWindow : application->windows) {
        if (auto window = weakWindow.lock(); window && window->shown) ++count;
    }
    return count;
}

bool ShouldQuitAfterWindowClosed(
    const std::shared_ptr<ApplicationState>& application) noexcept {
    return application && !application->shutdown &&
           application->shutdownMode == ShutdownMode::WhenLastWindowCloses &&
           GetLiveWindowCount(application) == 0;
}

void RequestQuit(const std::shared_ptr<ApplicationState>& application,
                 int exitCode) noexcept {
    if (!application || application->shutdown) return;
    application->exitCode = exitCode;
    if (application->backend) application->backend->RequestQuit(exitCode);
}

int RunApplication(const std::shared_ptr<ApplicationState>& application) {
    if (!application || application->shutdown || !application->backend) {
        return application ? application->exitCode : -1;
    }
    if (application->running) return application->exitCode;

    // With the default policy, an application that has no shown windows has
    // already satisfied its shutdown condition. Queue the same orderly quit
    // signal used by the final native destruction path so Run() remains
    // deterministic and drains the thread's message loop normally.
    if (application->shutdownMode == ShutdownMode::WhenLastWindowCloses &&
        GetLiveWindowCount(application) == 0) {
        application->backend->RequestQuit(application->exitCode);
    }

    application->running = true;
    const int loopResult = application->backend->Run();
    application->running = false;
    if (application->exitCode == 0) application->exitCode = loopResult;
    application->backend->Shutdown();
    application->shutdown = true;
    return application->exitCode;
}

namespace {

std::shared_ptr<ApplicationState> RequireDialogApplication(
    const std::shared_ptr<WindowState>& owner) {
    if (!owner) {
        throw std::logic_error("Dialog owner Window is invalid");
    }
    const auto application = owner->application.lock();
    if (!application) {
        throw std::logic_error("Dialog owner Window is detached");
    }
    if (application->shutdown || !application->backend) {
        throw std::logic_error("Dialog owner Window is unavailable during application shutdown");
    }
    if (!owner->shown) {
        throw std::logic_error("Dialog owner Window must be realized and shown");
    }
    return application;
}

} // namespace

MessageDialogResult ShowMessageDialog(
    const std::shared_ptr<WindowState>& owner, const std::string& message,
    const std::string& title, MessageDialogButtons buttons,
    MessageDialogIcon icon) {
    const auto application = RequireDialogApplication(owner);
    return application->backend->ShowMessageDialog(owner, message, title,
                                                   buttons, icon);
}

std::optional<std::string> ShowOpenFileDialog(
    const std::shared_ptr<WindowState>& owner, const std::string& title,
    const std::optional<std::string>& initialDirectory,
    const std::vector<FileDialogFilter>& filters) {
    const auto application = RequireDialogApplication(owner);
    return application->backend->ShowOpenFileDialog(owner, title,
                                                    initialDirectory, filters);
}

std::optional<std::string> ShowSaveFileDialog(
    const std::shared_ptr<WindowState>& owner, const std::string& title,
    const std::string& suggestedFileName,
    const std::optional<std::string>& initialDirectory,
    const std::vector<FileDialogFilter>& filters) {
    const auto application = RequireDialogApplication(owner);
    return application->backend->ShowSaveFileDialog(owner, title,
                                                    suggestedFileName,
                                                    initialDirectory, filters);
}

void DispatchFilesDropped(const std::shared_ptr<WindowState>& window,
                          std::vector<std::string> paths) {
    if (!window || !window->shown ||
        window->closeState == WindowCloseState::Closed) {
        return;
    }
    const auto callback = window->onFilesDropped;
    if (!callback) return;

    // Both the path storage and callback are local snapshots. The callback
    // may replace itself or close this/another Window without leaving the
    // backend with a pointer into invalidated native state.
    FileDropEvent event(NormalizeFileDropPaths(std::move(paths)));
    callback(event);
}

void DispatchTimerTick(const std::shared_ptr<TimerState>& timer) {
    if (!timer || !timer->running) return;
    const auto callback = timer->onTick;
    if (callback) callback();
}

void DispatchButtonClick(const std::shared_ptr<ControlState>& control) {
    if (!control || control->kind != ControlKind::Button || !control->onClick) return;
    auto callback = control->onClick;
    callback();
}

void DispatchTextChanged(const std::shared_ptr<ControlState>& control,
                         std::string text) {
    if (!control || (control->kind != ControlKind::TextBox &&
                     control->kind != ControlKind::TextArea) ||
        control->text == text) {
        return;
    }

    control->text = std::move(text);
    NotifyControlChanged(control);

    const auto callback = control->onTextChanged;
    if (callback) {
        // Keep the callback argument stable if the callback normalizes the
        // TextBox again and causes its std::string storage to change.
        const std::string callbackText = control->text;
        callback(callbackText);
    }
}

void DispatchSelectionChanged(const std::shared_ptr<ControlState>& control,
                              std::optional<std::size_t> index) {
    if (!IsSelectionControl(control) ||
        (index && *index >= control->items.size()) ||
        control->selectedIndex == index) {
        return;
    }

    control->selectedIndex = index;
    NotifyControlChanged(control);
    DispatchSelectionCallback(control);
}

void DispatchSelectionCallback(const std::shared_ptr<ControlState>& control) {
    if (!IsSelectionControl(control)) return;
    const auto callback = control->onSelectionChanged;
    if (callback) callback(control->selectedIndex);
}

void DispatchCheckedChanged(const std::shared_ptr<ControlState>& control,
                            bool checked) {
    if (!control || control->kind != ControlKind::CheckBox ||
        control->checked == checked) {
        return;
    }

    control->checked = checked;
    NotifyControlChanged(control);
    const auto callback = control->onCheckedChanged;
    if (callback) callback(checked);
}

void DispatchSliderChanged(const std::shared_ptr<ControlState>& control,
                           int value) {
    if (!control || control->kind != ControlKind::Slider) return;

    const int nextValue = std::clamp(value, control->minimum, control->maximum);
    if (control->value == nextValue) return;

    control->value = nextValue;
    NotifyControlChanged(control);
    const auto callback = control->onChanged;
    if (callback) callback();
}

void DispatchRadioSelection(const std::shared_ptr<ControlState>& control) {
    if (!IsRadioButton(control)) return;
    if (auto group = control->radioGroup.lock()) {
        SelectRadioButton(group, control);
    } else {
        // A RadioButton without a live RadioGroup is not selectable in the
        // App Model. Restore the authoritative unchecked native state after
        // the ordinary BUTTON class handles the click.
        NotifyControlChanged(control);
    }
}

void DispatchRadioSelectionChanged(const std::shared_ptr<ControlState>& control,
                                   bool selected) {
    if (!IsRadioButton(control)) return;
    const auto callback = control->onSelectedChanged;
    if (callback) callback(selected);
}

bool RadioGroupContains(const std::shared_ptr<RadioGroupState>& group,
                        const std::shared_ptr<ControlState>& control) noexcept {
    if (!group || !IsRadioButton(control)) return false;
    for (const auto& weakMember : group->members) {
        if (auto member = weakMember.lock(); member == control) return true;
    }
    return false;
}

std::size_t RadioGroupMemberCount(
    const std::shared_ptr<RadioGroupState>& group) noexcept {
    if (!group) return 0;
    std::size_t count = 0;
    for (const auto& weakMember : group->members) {
        if (!weakMember.expired()) ++count;
    }
    return count;
}

std::optional<std::size_t> GetRadioGroupSelectedIndex(
    const std::shared_ptr<RadioGroupState>& group) noexcept {
    if (!group) return std::nullopt;
    const auto selected = group->selected.lock();
    if (!selected) return std::nullopt;
    for (std::size_t index = 0; index < group->members.size(); ++index) {
        if (auto member = group->members[index].lock(); member == selected) {
            return index;
        }
    }
    return std::nullopt;
}

void AddRadioButtonToGroup(const std::shared_ptr<RadioGroupState>& group,
                           const std::shared_ptr<ControlState>& control) {
    if (!group || !IsRadioButton(control)) {
        throw std::logic_error("Invalid RadioGroup member");
    }
    if (RadioGroupContains(group, control)) {
        throw std::logic_error("A RadioButton cannot be added to a RadioGroup twice");
    }
    if (auto existing = control->radioGroup.lock()) {
        throw std::logic_error("A RadioButton cannot belong to multiple RadioGroups");
    }
    if (auto application = control->application.lock()) {
        if (IsBoundToAnotherApplication(group->application, application)) {
            throw std::logic_error(
                "A RadioGroup cannot contain controls from multiple Applications");
        }
        BindRadioGroupToApplication(group, application);
    }

    group->members.erase(
        std::remove_if(group->members.begin(), group->members.end(),
                       [](const std::weak_ptr<ControlState>& member) {
                           return member.expired();
                       }),
        group->members.end());
    group->members.emplace_back(control);
    control->radioGroup = group;
    control->selected = false;
}

void SelectRadioButton(const std::shared_ptr<RadioGroupState>& group,
                       const std::shared_ptr<ControlState>& control) {
    if (!group || !IsRadioButton(control) || !RadioGroupContains(group, control)) {
        throw std::logic_error("RadioButton is not a member of this RadioGroup");
    }

    const auto previous = group->selected.lock();
    if (previous == control && control->selected) return;

    group->selected = control;
    if (previous) previous->selected = false;
    control->selected = true;

    if (previous) NotifyControlChanged(previous);
    NotifyControlChanged(control);

    // Group state is already final before either callback runs. Each nested
    // Select() therefore forms a synchronous depth-first transition.
    if (previous) DispatchRadioSelectionChanged(previous, false);
    DispatchRadioSelectionChanged(control, true);
}

void ClearRadioGroupSelection(const std::shared_ptr<RadioGroupState>& group) {
    if (!group) return;
    const auto previous = group->selected.lock();
    if (!previous) {
        group->selected.reset();
        return;
    }

    group->selected.reset();
    previous->selected = false;
    NotifyControlChanged(previous);
    DispatchRadioSelectionChanged(previous, false);
}

void RemoveRadioButtonFromGroup(const std::shared_ptr<ControlState>& control,
                                bool dispatchCallback) {
    if (!IsRadioButton(control)) return;
    const auto group = control->radioGroup.lock();
    control->radioGroup.reset();
    if (!group) {
        control->selected = false;
        return;
    }

    group->members.erase(
        std::remove_if(group->members.begin(), group->members.end(),
                       [&control](const std::weak_ptr<ControlState>& member) {
                           return member.expired() || member.lock() == control;
                       }),
        group->members.end());

    const bool wasSelected = group->selected.lock() == control;
    if (wasSelected) group->selected.reset();
    control->selected = false;
    if (wasSelected) {
        NotifyControlChanged(control);
        if (dispatchCallback) DispatchRadioSelectionChanged(control, false);
    }
}

void DestroyRadioGroup(const std::shared_ptr<RadioGroupState>& group) noexcept {
    if (!group) return;
    group->selected.reset();
    for (const auto& weakMember : group->members) {
        if (auto member = weakMember.lock()) {
            member->radioGroup.reset();
            if (member->selected) {
                member->selected = false;
                try {
                    NotifyControlChanged(member);
                } catch (...) {
                }
            }
        }
    }
    group->members.clear();
}

} // namespace guidexos::appmodel::detail
