#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <vector>

using namespace guidexos::appmodel;

#undef assert
#define assert(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED: %s (line %d)\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (false)

namespace {

bool ThrowsLogicError(const auto& operation) {
    try {
        operation();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

bool NonNegative(const std::vector<LayoutRect>& rectangles) {
    for (const auto& rectangle : rectangles) {
        if (rectangle.width < 0 || rectangle.height < 0) return false;
    }
    return true;
}

} // namespace

int main() {
    // The default layout remains a padded vertical stack.
    Label first("First");
    Label second("Second");
    Layout defaultLayout;
    defaultLayout.Add(first);
    defaultLayout.Add(second);
    const auto defaultNatural = defaultLayout.GetNaturalSize();
    const auto defaultMinimum = defaultLayout.GetMinimumSize();
    assert(defaultNatural.width == 104);
    assert(defaultNatural.height == 108);
    assert(defaultMinimum.width == 48);
    assert(defaultMinimum.height == 108);
    const auto defaultGeometry = defaultLayout.CalculateGeometry({0, 0, 400, 200});
    assert(defaultGeometry.size() == 2);
    assert(defaultGeometry[0].x == defaultLayout.GetPadding());
    assert(defaultGeometry[1].y > defaultGeometry[0].y);
    assert(defaultGeometry[0].width == 400 - defaultLayout.GetPadding() * 2);

    // Horizontal placement keeps natural children and gives the remaining
    // main-axis space to an expanding child.
    Label nameLabel("Name");
    TextBox nameInput;
    Layout nameRow(Orientation::Horizontal, 0, 8);
    nameRow.Add(nameLabel);
    nameRow.Add(nameInput, LayoutSizing::Expand);
    const auto nameGeometry = nameRow.CalculateGeometry({0, 0, 500, 40});
    assert(nameGeometry.size() == 2);
    assert(nameGeometry[0].x == 0);
    assert(nameGeometry[1].x == nameGeometry[0].width + 8);
    assert(nameGeometry[1].width > 180);
    assert(nameGeometry[0].height == 40);
    assert(nameGeometry[1].height == 40);
    const auto noExtraGeometry = nameRow.CalculateGeometry(
        {0, 0, nameGeometry[0].width + 8, 40});
    // The expanding field keeps its usable minimum when the label must give
    // up its horizontal space; the final below-minimum clip is deterministic.
    assert(noExtraGeometry[1].width == 40);
    nameLabel.SetText("A much longer profile name");
    const auto changedNameGeometry = nameRow.CalculateGeometry({0, 0, 500, 40});
    assert(changedNameGeometry[0].width > nameGeometry[0].width);
    assert(changedNameGeometry[1].width < nameGeometry[1].width);

    // Nested vertical/horizontal layouts expose their own direct child
    // rectangles while the parent assigns the nested outer rectangle.
    Label title("Title");
    Layout nestedRoot(Orientation::Vertical, 0, 6);
    nestedRoot.Add(title);
    nestedRoot.Add(nameRow);
    const auto nestedRootGeometry = nestedRoot.CalculateGeometry({0, 0, 500, 200});
    assert(nestedRootGeometry.size() == 2);
    assert(nestedRootGeometry[1].y > nestedRootGeometry[0].y);
    const auto nestedRowGeometry = nameRow.CalculateGeometry(nestedRootGeometry[1]);
    assert(nestedRowGeometry.size() == 2);
    assert(nestedRowGeometry[1].x > nestedRowGeometry[0].x);

    // Multiple expanding children start at their natural sizes. Under this
    // constrained width both reach their usable minimum, then the remaining
    // below-minimum clip is assigned deterministically in insertion order.
    Button left("Left");
    Button right("Right");
    Layout expanding(Orientation::Horizontal, 0, 10);
    expanding.Add(left, LayoutSizing::Expand);
    expanding.Add(right, LayoutSizing::Expand);
    const auto expandingGeometry = expanding.CalculateGeometry({0, 0, 101, 40});
    assert(expandingGeometry.size() == 2);
    assert(expandingGeometry[0].width == 64);
    assert(expandingGeometry[1].width == 27);
    assert(expandingGeometry[1].x == expandingGeometry[0].width + 10);

    // A vertical expanding ListBox consumes remaining height and recalculates
    // when the containing bounds change.
    Label listHeading("Profiles");
    ListBox profiles;
    Button listAction("Action");
    Layout listLayout(Orientation::Vertical, 0, 8);
    listLayout.Add(listHeading);
    listLayout.Add(profiles, LayoutSizing::Expand);
    listLayout.Add(listAction);
    const auto shortListGeometry = listLayout.CalculateGeometry({0, 0, 400, 300});
    const auto tallListGeometry = listLayout.CalculateGeometry({0, 0, 400, 600});
    assert(tallListGeometry[1].height > shortListGeometry[1].height);
    assert(tallListGeometry[2].y > tallListGeometry[1].y);

    Layout legacyListLayout(Orientation::Vertical, 0, 8);
    Label legacyHeading("Legacy");
    ListBox legacyList;
    legacyListLayout.Add(legacyHeading);
    legacyListLayout.Add(legacyList);
    const auto legacyListGeometry =
        legacyListLayout.CalculateGeometry({0, 0, 400, 400});
    assert(legacyListGeometry[1].height > 140);

    // Spacing and undersized containers never produce negative geometry.
    Layout undersized(Orientation::Horizontal, 0, 8);
    Label wideOne("A long natural label");
    Label wideTwo("Another natural label");
    undersized.Add(wideOne);
    undersized.Add(wideTwo);
    const auto undersizedGeometry = undersized.CalculateGeometry({0, 0, 4, 20});
    assert(undersizedGeometry.size() == 2);
    assert(NonNegative(undersizedGeometry));
    assert(undersizedGeometry[0].width <= 4);
    assert(undersizedGeometry[1].width == 0);

    // Even a zero-sized child remains clipped to a tiny padded container, and
    // offset logical bounds do not lose their origin during cursor advances.
    Layout paddedTiny(Orientation::Vertical, 24, 4);
    Label tinyLabel("Tiny");
    paddedTiny.Add(tinyLabel);
    const auto paddedTinyGeometry =
        paddedTiny.CalculateGeometry({-10, -20, 10, 10});
    assert(paddedTinyGeometry.size() == 1);
    assert(paddedTinyGeometry[0].x >= -10);
    assert(paddedTinyGeometry[0].y >= -20);
    assert(paddedTinyGeometry[0].x + paddedTinyGeometry[0].width <= 0);
    assert(paddedTinyGeometry[0].y + paddedTinyGeometry[0].height <= -10);

    Layout offsetRow(Orientation::Horizontal, 0, 4);
    Label offsetFirst("First");
    Label offsetSecond("Second");
    offsetRow.Add(offsetFirst);
    offsetRow.Add(offsetSecond);
    const auto offsetGeometry =
        offsetRow.CalculateGeometry({-100, 0, 300, 40});
    assert(offsetGeometry[1].x == offsetGeometry[0].x +
           offsetGeometry[0].width + 4);

    Layout empty;
    assert(empty.ChildCount() == 0);
    assert(empty.CalculateGeometry({0, 0, 100, 100}).empty());
    Layout single(Orientation::Vertical, 0, 0);
    Label singleLabel("Only");
    single.Add(singleLabel);
    assert(single.CalculateGeometry({0, 0, 100, 100}).size() == 1);
    Layout nestedEmpty(Orientation::Horizontal, 0, 0);
    Layout emptyChild(Orientation::Vertical, 0, 0);
    nestedEmpty.Add(emptyChild);
    assert(nestedEmpty.ChildCount() == 1);
    assert(nestedEmpty.CalculateGeometry({0, 0, 100, 100}).size() == 1);

    // Controls, child layouts, and cycles have deterministic rejection rules.
    Label owned("Owned");
    Layout owner;
    Layout other;
    owner.Add(owned);
    assert(ThrowsLogicError([&]() { other.Add(owned); }));
    Layout parent;
    Layout child;
    Layout secondParent;
    parent.Add(child);
    assert(ThrowsLogicError([&]() { secondParent.Add(child); }));
    assert(ThrowsLogicError([&]() { parent.Add(parent); }));
    assert(ThrowsLogicError([&]() { child.Add(parent); }));

    // A root layout belongs to one window, can be closed and reopened, and
    // cannot cross application boundaries.
    Application app("com.guidexos.tests.layout-behavior");
    Window window(app);
    Layout content(Orientation::Vertical, 0, 0);
    Label contentLabel("Content");
    content.Add(contentLabel);
    window.SetContent(content);
    assert(ThrowsLogicError([&]() {
        Window secondWindow(app);
        secondWindow.SetContent(content);
    }));
    assert(window.Show());
    window.Close();
    assert(!window.IsShown());
    assert(window.Show());
    window.Close();

    Application otherApp("com.guidexos.tests.other-layout-behavior");
    Window otherWindow(otherApp);
    assert(ThrowsLogicError([&]() { otherWindow.SetContent(content); }));

    return 0;
}
