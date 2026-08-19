#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace guidexos::appmodel {

namespace detail {
struct MenuBarState;
struct MenuItemState;
struct MenuState;
}

// The first accelerator subset is deliberately small and platform-neutral:
// Control plus an ASCII letter, optionally with Shift. The native backend
// decides how that shortcut is registered and delivered.
class KeyShortcut final {
public:
    static KeyShortcut Ctrl(char letter);
    static KeyShortcut CtrlShift(char letter);

    char GetLetter() const noexcept;
    bool HasShift() const noexcept;

    friend bool operator==(const KeyShortcut&, const KeyShortcut&) noexcept =
        default;

private:
    KeyShortcut(char letter, bool shift) noexcept
        : letter_(letter), shift_(shift) {}

    char letter_;
    bool shift_;
};

class MenuItem final {
public:
    explicit MenuItem(std::string text = {});
    ~MenuItem();

    MenuItem(const MenuItem&) = default;
    MenuItem& operator=(const MenuItem&) = default;
    MenuItem(MenuItem&&) noexcept = default;
    MenuItem& operator=(MenuItem&&) noexcept = default;

    void SetText(std::string text);
    const std::string& GetText() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

    // Checked state is model state only; invocation does not implicitly
    // toggle it. The callback can make the transition explicitly.
    void SetChecked(bool checked);
    bool IsChecked() const noexcept;

    void SetShortcut(std::optional<KeyShortcut> shortcut);
    void ClearShortcut();
    std::optional<KeyShortcut> GetShortcut() const noexcept;

    // Replaces the current callback. Passing an empty callback unsubscribes.
    // The callback runs synchronously after enabled/model state checks have
    // completed. A detached item does not invoke its callback.
    void OnInvoked(std::function<void()> callback);

    // Programmatic activation uses the same logical dispatch as a native menu
    // command. It is a no-op while disabled or detached from a MenuBar.
    void Invoke();

private:
    std::shared_ptr<detail::MenuItemState> state_;

    friend class Menu;
};

class Menu final {
public:
    explicit Menu(std::string text = {});
    ~Menu() = default;

    // Menu is a shared model handle like Layout. Inserting a temporary or a
    // copied Menu retains its child structure in the parent.
    Menu(const Menu&) = default;
    Menu& operator=(const Menu&) = default;
    Menu(Menu&&) noexcept = default;
    Menu& operator=(Menu&&) noexcept = default;

    void SetText(std::string text);
    const std::string& GetText() const noexcept;

    void Add(MenuItem& item);
    void Add(Menu& menu);
    void AddSeparator();

    bool Remove(MenuItem& item);
    bool Remove(Menu& menu);
    void Clear();
    std::size_t GetEntryCount() const noexcept;

    // Replaces the synchronous callback invoked immediately before this
    // menu's native popup becomes visible. The callback may update enabled,
    // checked, or other model state. Passing an empty callback clears it.
    void OnOpening(std::function<void()> callback);

private:
    std::shared_ptr<detail::MenuState> state_;

    friend class MenuBar;
};

class MenuBar final {
public:
    MenuBar();
    ~MenuBar() = default;

    // MenuBar is a shared model handle. A bar may be attached to one Window
    // at a time, even when copied.
    MenuBar(const MenuBar&) = default;
    MenuBar& operator=(const MenuBar&) = default;
    MenuBar(MenuBar&&) noexcept = default;
    MenuBar& operator=(MenuBar&&) noexcept = default;

    void Add(Menu& menu);
    bool Remove(Menu& menu);
    void Clear();
    std::size_t GetMenuCount() const noexcept;

private:
    std::shared_ptr<detail::MenuBarState> state_;

    friend class Window;
};

} // namespace guidexos::appmodel
