#include "../src/appmodel/runtime.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>

using namespace guidexos::appmodel;
using namespace guidexos::appmodel::detail;

#undef assert
#define assert(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED: %s (line %d)\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (false)

namespace {

struct SyntheticMeasurementProvider final : LayoutMeasurementProvider {
    LayoutMeasurement Measure(const ControlState& control) const override {
        if (control.text == "A") return {{40, 10}, {10, 5}};
        if (control.text == "B") return {{50, 20}, {20, 8}};
        if (control.text == "C") return {{30, 30}, {15, 12}};
        if (control.text == "equal") return {{12, 7}, {12, 7}};
        if (control.text == "zero") return {{0, 0}, {0, 0}};
        if (control.text == "invalid") return {{-40, -8}, {-10, 100}};
        if (control.text == "changed") return {{changedWidth, 10}, {10, 5}};
        return {{24, 16}, {8, 6}};
    }

    int changedWidth = 24;
};

std::shared_ptr<ControlState> Control(const char* text) {
    return std::make_shared<ControlState>(ControlKind::Label, text);
}

std::shared_ptr<LayoutState> LayoutTree(Orientation orientation,
                                        int padding = 0,
                                        int spacing = 0) {
    return std::make_shared<LayoutState>(orientation, padding, spacing);
}

void AddControl(const std::shared_ptr<LayoutState>& layout,
                const std::shared_ptr<ControlState>& control,
                LayoutSizing sizing = LayoutSizing::Natural) {
    layout->children.push_back(LayoutItem{control, nullptr, sizing, false});
}

void AddLayout(const std::shared_ptr<LayoutState>& parent,
               const std::shared_ptr<LayoutState>& child,
               LayoutSizing sizing = LayoutSizing::Natural) {
    parent->children.push_back(LayoutItem{nullptr, child, sizing, false});
}

bool NonNegative(const std::vector<LayoutRect>& rectangles) {
    return std::all_of(rectangles.begin(), rectangles.end(),
                       [](const LayoutRect& rectangle) {
                           return rectangle.width >= 0 && rectangle.height >= 0;
                       });
}

} // namespace

int main() {
    SyntheticMeasurementProvider provider;

    // Neutral control measurements are non-negative and preserve the minimum
    // contract even when a provider supplies invalid values.
    const auto invalid = Control("invalid");
    const auto invalidMeasurement = GetControlMeasurement(invalid, &provider);
    assert(invalidMeasurement.minimum.width == 0);
    assert(invalidMeasurement.minimum.height == 100);
    assert(invalidMeasurement.natural.width == 0);
    assert(invalidMeasurement.natural.height == 100);

    const auto zero = Control("zero");
    const auto zeroMeasurement = GetControlMeasurement(zero, &provider);
    assert(zeroMeasurement.natural.width == 0);
    assert(zeroMeasurement.natural.height == 0);
    assert(zeroMeasurement.minimum.width == 0);
    assert(zeroMeasurement.minimum.height == 0);

    // The neutral public control contract gives text-like controls useful
    // preferred sizes, stable normal heights, and smaller usable minimums.
    const auto neutralLabel = std::make_shared<ControlState>(
        ControlKind::Label, "Label");
    const auto neutralTextBox = std::make_shared<ControlState>(
        ControlKind::TextBox, "");
    const auto neutralButton = std::make_shared<ControlState>(
        ControlKind::Button, "OK");
    const auto neutralCheck = std::make_shared<ControlState>(
        ControlKind::CheckBox, "Enable");
    const auto neutralRadio = std::make_shared<ControlState>(
        ControlKind::RadioButton, "System");
    const auto neutralList = std::make_shared<ControlState>(
        ControlKind::ListBox, "");
    const auto neutralCombo = std::make_shared<ControlState>(
        ControlKind::ComboBox, "");
    const auto labelMeasurement = GetNeutralControlMeasurement(*neutralLabel);
    const auto textBoxMeasurement = GetNeutralControlMeasurement(*neutralTextBox);
    const auto buttonMeasurement = GetNeutralControlMeasurement(*neutralButton);
    const auto checkMeasurement = GetNeutralControlMeasurement(*neutralCheck);
    const auto radioMeasurement = GetNeutralControlMeasurement(*neutralRadio);
    const auto listMeasurement = GetNeutralControlMeasurement(*neutralList);
    const auto comboMeasurement = GetNeutralControlMeasurement(*neutralCombo);
    assert(labelMeasurement.natural.width > labelMeasurement.minimum.width);
    assert(labelMeasurement.natural.height == labelMeasurement.minimum.height);
    assert(textBoxMeasurement.natural.width > textBoxMeasurement.minimum.width);
    assert(textBoxMeasurement.natural.height == textBoxMeasurement.minimum.height);
    assert(buttonMeasurement.natural.width >= buttonMeasurement.minimum.width);
    assert(buttonMeasurement.minimum.width > 0);
    assert(checkMeasurement.natural.width >= checkMeasurement.minimum.width);
    assert(radioMeasurement.natural.width >= radioMeasurement.minimum.width);
    assert(checkMeasurement.minimum.height > 0);
    assert(radioMeasurement.minimum.height > 0);
    assert(listMeasurement.natural.width == 220);
    assert(listMeasurement.natural.height == 140);
    assert(listMeasurement.minimum.width == 96);
    assert(listMeasurement.minimum.height == 48);
    assert(comboMeasurement.natural.width == 220);
    assert(comboMeasurement.natural.height == 28);
    assert(comboMeasurement.minimum.width == 112);
    assert(comboMeasurement.minimum.height == 28);

    // A ComboBox is naturally closed-control-sized in a vertical stack and
    // expands only along the requested horizontal layout axis.
    const auto comboLayout = LayoutTree(Orientation::Vertical, 0, 0);
    AddControl(comboLayout, neutralCombo);
    const auto comboGeometry = CalculateDirectLayoutGeometry(
        comboLayout, {0, 0, 640, 200});
    assert(comboGeometry.size() == 1);
    assert(comboGeometry[0].width == 640);
    assert(comboGeometry[0].height == 28);
    const auto comboRow = LayoutTree(Orientation::Horizontal, 0, 0);
    AddControl(comboRow, neutralCombo, LayoutSizing::Expand);
    const auto comboExpanded = CalculateDirectLayoutGeometry(
        comboRow, {0, 0, 640, 40});
    assert(comboExpanded.size() == 1);
    assert(comboExpanded[0].width == 640);
    assert(comboExpanded[0].height == 40);

    // Vertical natural/minimum composition includes child measurements,
    // spacing, and padding independently.
    const auto vertical = LayoutTree(Orientation::Vertical, 3, 4);
    AddControl(vertical, Control("A"));
    AddControl(vertical, Control("B"));
    AddControl(vertical, Control("C"), LayoutSizing::Expand);
    const auto verticalMeasurement = GetLayoutMeasurement(vertical, &provider);
    assert(verticalMeasurement.natural.width == 56);
    assert(verticalMeasurement.natural.height == 74);
    assert(verticalMeasurement.minimum.width == 26);
    assert(verticalMeasurement.minimum.height == 39);

    // Horizontal composition transposes the main/cross-axis rules.
    const auto horizontal = LayoutTree(Orientation::Horizontal, 3, 4);
    AddControl(horizontal, Control("A"));
    AddControl(horizontal, Control("B"));
    AddControl(horizontal, Control("C"));
    const auto horizontalMeasurement = GetLayoutMeasurement(horizontal, &provider);
    assert(horizontalMeasurement.natural.width == 134);
    assert(horizontalMeasurement.natural.height == 36);
    assert(horizontalMeasurement.minimum.width == 59);
    assert(horizontalMeasurement.minimum.height == 18);

    const auto empty = LayoutTree(Orientation::Vertical);
    const auto emptyMeasurement = GetLayoutMeasurement(empty, &provider);
    assert(emptyMeasurement.natural.width == 0);
    assert(emptyMeasurement.natural.height == 0);
    assert(emptyMeasurement.minimum.width == 0);
    assert(emptyMeasurement.minimum.height == 0);

    // Natural items keep their natural main-axis size. An expanding child
    // receives its natural size plus all remaining space.
    const auto expanding = LayoutTree(Orientation::Vertical, 2, 3);
    AddControl(expanding, Control("A"));
    AddControl(expanding, Control("B"), LayoutSizing::Expand);
    const auto expandedGeometry = CalculateDirectLayoutGeometry(
        expanding, {0, 0, 100, 80}, &provider);
    assert(expandedGeometry[0].height == 10);
    assert(expandedGeometry[1].height == 63);
    assert(expandedGeometry[0].width == 96);
    assert(expandedGeometry[1].y == 15);

    // Multiple expanding children share only the remainder, with the first
    // child receiving a deterministic integer remainder.
    const auto multipleExpand = LayoutTree(Orientation::Horizontal, 0, 0);
    AddControl(multipleExpand, Control("A"), LayoutSizing::Expand);
    AddControl(multipleExpand, Control("B"), LayoutSizing::Expand);
    const auto multipleGeometry = CalculateDirectLayoutGeometry(
        multipleExpand, {0, 0, 101, 40}, &provider);
    assert(multipleGeometry[0].width == 46);
    assert(multipleGeometry[1].width == 55);
    assert(multipleGeometry[1].x == 46);

    // Under pressure, expanding children shrink toward minimum first, then
    // natural children. If all minimums cannot fit, insertion-order clipping
    // is deterministic and still never produces a negative dimension.
    const auto shrink = LayoutTree(Orientation::Horizontal, 0, 0);
    AddControl(shrink, Control("A"));
    AddControl(shrink, Control("B"), LayoutSizing::Expand);
    const auto shrinkGeometry = CalculateDirectLayoutGeometry(
        shrink, {0, 0, 60, 30}, &provider);
    assert(shrinkGeometry[0].width == 40);
    assert(shrinkGeometry[1].width == 20);

    const auto clippedGeometry = CalculateDirectLayoutGeometry(
        shrink, {0, 0, 20, 30}, &provider);
    assert(clippedGeometry[0].width == 10);
    assert(clippedGeometry[1].width == 10);
    assert(NonNegative(clippedGeometry));

    // Padding and spacing consume the main axis before allocation.
    const auto padded = LayoutTree(Orientation::Horizontal, 5, 4);
    AddControl(padded, Control("equal"));
    AddControl(padded, Control("equal"));
    const auto paddedGeometry = CalculateDirectLayoutGeometry(
        padded, {0, 0, 50, 20}, &provider);
    assert(paddedGeometry[0].x == 5);
    assert(paddedGeometry[0].width == 12);
    assert(paddedGeometry[1].x == 21);
    assert(paddedGeometry[1].width == 12);

    // Nested layouts compose measurements recursively, including an empty
    // nested layout and an expanding nested child.
    const auto nestedChild = LayoutTree(Orientation::Horizontal, 1, 2);
    AddControl(nestedChild, Control("A"));
    AddControl(nestedChild, Control("B"));
    const auto nestedEmpty = LayoutTree(Orientation::Vertical, 2, 0);
    const auto nestedRoot = LayoutTree(Orientation::Vertical, 0, 5);
    AddLayout(nestedRoot, nestedChild);
    AddLayout(nestedRoot, nestedEmpty, LayoutSizing::Expand);
    const auto nestedMeasurement = GetLayoutMeasurement(nestedRoot, &provider);
    assert(nestedMeasurement.natural.width == 94);
    assert(nestedMeasurement.natural.height == 31);
    assert(nestedMeasurement.minimum.width == 34);
    assert(nestedMeasurement.minimum.height == 19);
    const auto nestedGeometry = CalculateDirectLayoutGeometry(
        nestedRoot, {0, 0, 100, 100}, &provider);
    assert(nestedGeometry.size() == 2);
    assert(nestedGeometry[0].height == 22);
    assert(nestedGeometry[1].height == 73);

    // The same neutral geometry object is stable for repeated calculations,
    // while a changed provider measurement is reflected immediately.
    const auto dynamic = LayoutTree(Orientation::Horizontal);
    AddControl(dynamic, Control("changed"));
    const auto first = CalculateDirectLayoutGeometry(
        dynamic, {0, 0, 100, 20}, &provider);
    const auto repeat = CalculateDirectLayoutGeometry(
        dynamic, {0, 0, 100, 20}, &provider);
    assert(first[0].width == repeat[0].width);
    provider.changedWidth = 64;
    const auto changed = CalculateDirectLayoutGeometry(
        dynamic, {0, 0, 100, 20}, &provider);
    assert(changed[0].width == 64);

    // Negative bounds and extreme logical values are normalized without
    // negative dimensions or signed-coordinate overflow in the allocator.
    const auto extreme = CalculateDirectLayoutGeometry(
        dynamic,
        {std::numeric_limits<int>::max(), std::numeric_limits<int>::min(),
         std::numeric_limits<int>::max(), std::numeric_limits<int>::max()},
        &provider);
    assert(NonNegative(extreme));
    const auto negative = CalculateDirectLayoutGeometry(
        dynamic, {0, 0, -10, -20}, &provider);
    assert(NonNegative(negative));

    return 0;
}
