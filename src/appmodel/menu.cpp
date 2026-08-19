#include "guidexos/appmodel/menu.hpp"

#include "runtime.hpp"
#include "text_validation.hpp"

#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace guidexos::appmodel {
namespace {

char NormalizeShortcutLetter(char letter) {
    const unsigned char value = static_cast<unsigned char>(letter);
    if (value < static_cast<unsigned char>('A') ||
        value > static_cast<unsigned char>('z') ||
        (value > static_cast<unsigned char>('Z') &&
         value < static_cast<unsigned char>('a'))) {
        throw std::invalid_argument(
            "Menu shortcuts currently support ASCII letters only");
    }
    return static_cast<char>(std::toupper(value));
}

int ShortcutKey(const KeyShortcut& shortcut) noexcept {
    return (static_cast<int>(shortcut.GetLetter()) << 1) |
           (shortcut.HasShift() ? 1 : 0);
}

void RequireState(const std::shared_ptr<detail::MenuState>& state,
                  const char* type) {
    if (!state) throw std::logic_error(std::string(type) + " is invalid");
}

void RequireState(const std::shared_ptr<detail::MenuItemState>& state,
                  const char* type) {
    if (!state) throw std::logic_error(std::string(type) + " is invalid");
}

void RequireState(const std::shared_ptr<detail::MenuBarState>& state,
                  const char* type) {
    if (!state) throw std::logic_error(std::string(type) + " is invalid");
}

bool ContainsMenu(const std::shared_ptr<detail::MenuState>& root,
                  const std::shared_ptr<detail::MenuState>& target,
                  std::unordered_set<detail::MenuState*>& visiting) {
    if (!root || !target) return false;
    if (root == target) return true;
    if (!visiting.insert(root.get()).second) return false;
    for (const auto& entry : root->entries) {
        if (entry.menu && ContainsMenu(entry.menu, target, visiting)) {
            visiting.erase(root.get());
            return true;
        }
    }
    visiting.erase(root.get());
    return false;
}

void AddShortcut(const std::shared_ptr<detail::MenuItemState>& item,
                 std::unordered_map<int, detail::MenuItemState*>& shortcuts) {
    if (!item || !item->shortcut) return;
    const int key = ShortcutKey(*item->shortcut);
    const auto [iterator, inserted] = shortcuts.emplace(key, item.get());
    if (!inserted && iterator->second != item.get()) {
        throw std::logic_error(
            "Duplicate menu shortcuts are not allowed in one MenuBar");
    }
}

void ValidateMenuState(
    const std::shared_ptr<detail::MenuState>& menu,
    const std::shared_ptr<detail::MenuState>& expectedParent,
    const std::shared_ptr<detail::MenuBarState>& menuBar,
    const std::shared_ptr<detail::ApplicationState>& application,
    std::unordered_set<detail::MenuState*>& visiting,
    std::unordered_set<detail::MenuState*>& visited,
    std::unordered_map<int, detail::MenuItemState*>& shortcuts) {
    RequireState(menu, "Menu");
    if (!visiting.insert(menu.get()).second) {
        throw std::logic_error("Menu hierarchy cannot contain cycles");
    }
    if (visited.contains(menu.get())) {
        throw std::logic_error("A Menu cannot appear in multiple locations");
    }

    if (auto parent = menu->parent.lock(); parent != expectedParent) {
        throw std::logic_error("Menu hierarchy parent state is inconsistent");
    }
    if (auto existingBar = menu->menuBar.lock(); existingBar &&
        existingBar != menuBar) {
        throw std::logic_error("A Menu cannot belong to multiple MenuBars");
    }
    if (auto existingApplication = menu->application.lock();
        existingApplication && existingApplication != application) {
        throw std::logic_error(
            "A Menu cannot be used by multiple Applications");
    }

    for (const auto& entry : menu->entries) {
        if (entry.separator) {
            if (entry.menu || entry.item) {
                throw std::logic_error("Invalid Menu separator entry");
            }
            continue;
        }

        if (entry.item) {
            if (entry.menu || entry.item->parent.lock() != menu) {
                throw std::logic_error("MenuItem parent state is inconsistent");
            }
            if (auto itemBar = entry.item->menuBar.lock(); itemBar &&
                itemBar != menuBar) {
                throw std::logic_error(
                    "A MenuItem cannot belong to multiple MenuBars");
            }
            if (auto itemApplication = entry.item->application.lock();
                itemApplication && itemApplication != application) {
                throw std::logic_error(
                    "A MenuItem cannot be used by multiple Applications");
            }
            AddShortcut(entry.item, shortcuts);
            continue;
        }

        if (!entry.menu) {
            throw std::logic_error("Invalid Menu entry");
        }
        if (entry.menu->parent.lock() != menu) {
            throw std::logic_error("Submenu parent state is inconsistent");
        }
        ValidateMenuState(entry.menu, menu, menuBar, application, visiting,
                          visited, shortcuts);
    }

    visiting.erase(menu.get());
    visited.insert(menu.get());
}

void ValidateMenuBar(const std::shared_ptr<detail::MenuBarState>& menuBar,
                     const std::shared_ptr<detail::ApplicationState>& application) {
    if (!menuBar) throw std::logic_error("MenuBar is invalid");
    std::unordered_set<detail::MenuState*> visiting;
    std::unordered_set<detail::MenuState*> visited;
    std::unordered_map<int, detail::MenuItemState*> shortcuts;
    for (const auto& menu : menuBar->menus) {
        ValidateMenuState(menu, nullptr, menuBar, application, visiting, visited,
                          shortcuts);
    }
}

void ApplyMenuState(const std::shared_ptr<detail::MenuState>& menu,
                    const std::shared_ptr<detail::MenuBarState>& menuBar,
                    const std::shared_ptr<detail::ApplicationState>& application) {
    if (!menu) return;
    menu->menuBar = menuBar;
    menu->application = application;
    for (const auto& entry : menu->entries) {
        if (entry.item) {
            entry.item->menuBar = menuBar;
            entry.item->application = application;
        } else if (entry.menu) {
            ApplyMenuState(entry.menu, menuBar, application);
        }
    }
}

void DetachMenuState(const std::shared_ptr<detail::MenuState>& menu,
                     const std::shared_ptr<detail::MenuBarState>& menuBar) noexcept {
    if (!menu) return;
    if (auto existing = menu->menuBar.lock(); existing && existing != menuBar) {
        return;
    }
    menu->menuBar.reset();
    menu->application.reset();
    for (const auto& entry : menu->entries) {
        if (entry.item) {
            entry.item->menuBar.reset();
            entry.item->application.reset();
        } else if (entry.menu) {
            DetachMenuState(entry.menu, menuBar);
        }
    }
}

std::shared_ptr<detail::MenuBarState> GetMenuBar(
    const std::shared_ptr<detail::MenuState>& menu) {
    return menu ? menu->menuBar.lock() : nullptr;
}

std::shared_ptr<detail::MenuBarState> GetMenuBar(
    const std::shared_ptr<detail::MenuItemState>& item) {
    return item ? item->menuBar.lock() : nullptr;
}

bool HasShortcut(const std::shared_ptr<detail::MenuBarState>& menuBar,
                 const std::shared_ptr<detail::MenuItemState>& ignored,
                 const KeyShortcut& shortcut) {
    if (!menuBar) return false;
    std::unordered_set<detail::MenuState*> visiting;
    std::function<bool(const std::shared_ptr<detail::MenuState>&)> visit =
        [&](const std::shared_ptr<detail::MenuState>& menu) {
            if (!menu || !visiting.insert(menu.get()).second) return false;
            for (const auto& entry : menu->entries) {
                if (entry.item && entry.item != ignored && entry.item->shortcut &&
                    *entry.item->shortcut == shortcut) {
                    return true;
                }
                if (entry.menu && visit(entry.menu)) return true;
            }
            visiting.erase(menu.get());
            return false;
        };
    for (const auto& menu : menuBar->menus) {
        if (visit(menu)) return true;
    }
    return false;
}

void NotifyForMenuBar(const std::shared_ptr<detail::MenuBarState>& menuBar) {
    if (!menuBar) return;
    if (auto window = menuBar->window.lock()) {
        if (auto application = window->application.lock(); application &&
            application->backend && !application->shutdown && window->shown) {
            application->backend->RefreshMenuBar(window);
        }
    }
}

} // namespace

KeyShortcut KeyShortcut::Ctrl(char letter) {
    return KeyShortcut(NormalizeShortcutLetter(letter), false);
}

KeyShortcut KeyShortcut::CtrlShift(char letter) {
    return KeyShortcut(NormalizeShortcutLetter(letter), true);
}

char KeyShortcut::GetLetter() const noexcept {
    return letter_;
}

bool KeyShortcut::HasShift() const noexcept {
    return shift_;
}

MenuItem::MenuItem(std::string text)
    : state_(nullptr) {
    detail::ValidateUtf8(text);
    state_ = std::make_shared<detail::MenuItemState>(std::move(text));
}

MenuItem::~MenuItem() {
    // A parent Menu intentionally retains the item state after the public
    // handle is destroyed. Clear callbacks at that boundary so a retained
    // native binding cannot call into objects owned by the old scope.
    if (state_) state_->onInvoked = {};
}

void MenuItem::SetText(std::string text) {
    detail::ValidateUtf8(text);
    RequireState(state_, "MenuItem");
    if (state_->text == text) return;
    state_->text = std::move(text);
    detail::NotifyMenuItemChanged(state_);
}

const std::string& MenuItem::GetText() const noexcept {
    return state_->text;
}

void MenuItem::SetEnabled(bool enabled) {
    RequireState(state_, "MenuItem");
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyMenuItemChanged(state_);
}

bool MenuItem::IsEnabled() const noexcept {
    return state_->enabled;
}

void MenuItem::SetChecked(bool checked) {
    RequireState(state_, "MenuItem");
    if (state_->checked == checked) return;
    state_->checked = checked;
    detail::NotifyMenuItemChanged(state_);
}

bool MenuItem::IsChecked() const noexcept {
    return state_->checked;
}

void MenuItem::SetShortcut(std::optional<KeyShortcut> shortcut) {
    RequireState(state_, "MenuItem");
    if (shortcut && HasShortcut(GetMenuBar(state_), state_, *shortcut)) {
        throw std::logic_error(
            "Duplicate menu shortcuts are not allowed in one MenuBar");
    }
    state_->shortcut = shortcut;
    detail::NotifyMenuItemChanged(state_);
}

void MenuItem::ClearShortcut() {
    SetShortcut(std::nullopt);
}

std::optional<KeyShortcut> MenuItem::GetShortcut() const noexcept {
    return state_->shortcut;
}

void MenuItem::OnInvoked(std::function<void()> callback) {
    RequireState(state_, "MenuItem");
    state_->onInvoked = std::move(callback);
}

void MenuItem::Invoke() {
    detail::DispatchMenuItemInvocation(state_);
}

Menu::Menu(std::string text)
    : state_(nullptr) {
    detail::ValidateUtf8(text);
    state_ = std::make_shared<detail::MenuState>(std::move(text));
}

void Menu::SetText(std::string text) {
    detail::ValidateUtf8(text);
    RequireState(state_, "Menu");
    if (state_->text == text) return;
    state_->text = std::move(text);
    detail::NotifyMenuChanged(state_);
}

const std::string& Menu::GetText() const noexcept {
    return state_->text;
}

void Menu::Add(MenuItem& item) {
    RequireState(state_, "Menu");
    RequireState(item.state_, "MenuItem");
    if (item.state_->parent.lock()) {
        throw std::logic_error("A MenuItem cannot belong to multiple menus");
    }
    if (auto menuBar = GetMenuBar(state_); menuBar) {
        if (item.state_->application.lock() &&
            item.state_->application.lock() != state_->application.lock()) {
            throw std::logic_error(
                "A MenuItem cannot be used by multiple Applications");
        }
        if (item.state_->shortcut &&
            HasShortcut(menuBar, item.state_, *item.state_->shortcut)) {
            throw std::logic_error(
                "Duplicate menu shortcuts are not allowed in one MenuBar");
        }
    }
    state_->entries.push_back(detail::MenuEntry{nullptr, item.state_, false});
    item.state_->parent = state_;
    if (auto menuBar = GetMenuBar(state_)) {
        item.state_->menuBar = menuBar;
        item.state_->application = state_->application;
    }
    detail::NotifyMenuChanged(state_);
}

void Menu::Add(Menu& menu) {
    RequireState(state_, "Menu");
    RequireState(menu.state_, "Menu");
    if (menu.state_ == state_) {
        throw std::logic_error("A Menu cannot contain itself");
    }
    if (menu.state_->parent.lock() || menu.state_->menuBar.lock()) {
        throw std::logic_error("A Menu cannot belong to multiple parents");
    }
    std::unordered_set<detail::MenuState*> visiting;
    if (ContainsMenu(menu.state_, state_, visiting)) {
        throw std::logic_error("Menu hierarchy cannot contain cycles");
    }
    if (auto menuBar = GetMenuBar(state_)) {
        std::unordered_set<detail::MenuState*> candidateVisiting;
        std::unordered_set<detail::MenuState*> candidateVisited;
        std::unordered_map<int, detail::MenuItemState*> candidateShortcuts;
        ValidateMenuState(menu.state_, nullptr, menuBar, state_->application.lock(),
                          candidateVisiting, candidateVisited, candidateShortcuts);
        for (const auto& [key, item] : candidateShortcuts) {
            (void)key;
            if (item->shortcut && HasShortcut(menuBar, nullptr, *item->shortcut)) {
                throw std::logic_error(
                    "Duplicate menu shortcuts are not allowed in one MenuBar");
            }
        }
    }

    state_->entries.push_back(detail::MenuEntry{menu.state_, nullptr, false});
    menu.state_->parent = state_;
    if (auto menuBar = GetMenuBar(state_)) {
        ApplyMenuState(menu.state_, menuBar, state_->application.lock());
    }
    detail::NotifyMenuChanged(state_);
}

void Menu::AddSeparator() {
    RequireState(state_, "Menu");
    state_->entries.push_back(detail::MenuEntry{nullptr, nullptr, true});
    detail::NotifyMenuChanged(state_);
}

bool Menu::Remove(MenuItem& item) {
    RequireState(state_, "Menu");
    RequireState(item.state_, "MenuItem");
    for (auto iterator = state_->entries.begin(); iterator != state_->entries.end();
         ++iterator) {
        if (iterator->item != item.state_) continue;
        state_->entries.erase(iterator);
        item.state_->parent.reset();
        item.state_->menuBar.reset();
        item.state_->application.reset();
        detail::NotifyMenuChanged(state_);
        return true;
    }
    return false;
}

bool Menu::Remove(Menu& menu) {
    RequireState(state_, "Menu");
    RequireState(menu.state_, "Menu");
    for (auto iterator = state_->entries.begin(); iterator != state_->entries.end();
         ++iterator) {
        if (iterator->menu != menu.state_) continue;
        state_->entries.erase(iterator);
        menu.state_->parent.reset();
        DetachMenuState(menu.state_, GetMenuBar(state_));
        detail::NotifyMenuChanged(state_);
        return true;
    }
    return false;
}

void Menu::Clear() {
    RequireState(state_, "Menu");
    for (const auto& entry : state_->entries) {
        if (entry.item) {
            entry.item->parent.reset();
            entry.item->menuBar.reset();
            entry.item->application.reset();
        } else if (entry.menu) {
            entry.menu->parent.reset();
            DetachMenuState(entry.menu, GetMenuBar(state_));
        }
    }
    state_->entries.clear();
    detail::NotifyMenuChanged(state_);
}

std::size_t Menu::GetEntryCount() const noexcept {
    return state_ ? state_->entries.size() : 0;
}

void Menu::OnOpening(std::function<void()> callback) {
    RequireState(state_, "Menu");
    state_->onOpening = std::move(callback);
}

MenuBar::MenuBar()
    : state_(std::make_shared<detail::MenuBarState>()) {}

void MenuBar::Add(Menu& menu) {
    RequireState(state_, "MenuBar");
    RequireState(menu.state_, "Menu");
    if (menu.state_->parent.lock() || menu.state_->menuBar.lock()) {
        throw std::logic_error("A Menu cannot belong to multiple parents");
    }

    {
        const auto application = state_->application.lock();
        std::unordered_set<detail::MenuState*> visiting;
        std::unordered_set<detail::MenuState*> visited;
        std::unordered_map<int, detail::MenuItemState*> shortcuts;
        ValidateMenuState(menu.state_, nullptr, state_, application, visiting,
                          visited, shortcuts);
        std::unordered_map<int, detail::MenuItemState*> existing;
        for (const auto& existingMenu : state_->menus) {
            std::unordered_set<detail::MenuState*> existingVisiting;
            std::unordered_set<detail::MenuState*> existingVisited;
            ValidateMenuState(existingMenu, nullptr, state_, application,
                              existingVisiting, existingVisited, existing);
        }
        for (const auto& [key, item] : shortcuts) {
            (void)key;
            if (item->shortcut && HasShortcut(state_, nullptr, *item->shortcut)) {
                throw std::logic_error(
                    "Duplicate menu shortcuts are not allowed in one MenuBar");
            }
        }
    }

    state_->menus.push_back(menu.state_);
    ApplyMenuState(menu.state_, state_, state_->application.lock());
    detail::NotifyMenuChanged(menu.state_);
}

bool MenuBar::Remove(Menu& menu) {
    RequireState(state_, "MenuBar");
    RequireState(menu.state_, "Menu");
    for (auto iterator = state_->menus.begin(); iterator != state_->menus.end();
         ++iterator) {
        if (*iterator != menu.state_) continue;
        state_->menus.erase(iterator);
        menu.state_->parent.reset();
        DetachMenuState(menu.state_, state_);
        NotifyForMenuBar(state_);
        return true;
    }
    return false;
}

void MenuBar::Clear() {
    RequireState(state_, "MenuBar");
    for (const auto& menu : state_->menus) {
        if (menu) {
            menu->parent.reset();
            DetachMenuState(menu, state_);
        }
    }
    state_->menus.clear();
    NotifyForMenuBar(state_);
}

std::size_t MenuBar::GetMenuCount() const noexcept {
    return state_ ? state_->menus.size() : 0;
}

namespace detail {

void NotifyMenuChanged(const std::shared_ptr<MenuState>& menu) {
    NotifyForMenuBar(GetMenuBar(menu));
}

void NotifyMenuItemChanged(const std::shared_ptr<MenuItemState>& item) {
    NotifyForMenuBar(GetMenuBar(item));
}

void BindMenuBarToApplication(
    const std::shared_ptr<MenuBarState>& menuBar,
    const std::shared_ptr<ApplicationState>& application,
    const std::shared_ptr<WindowState>& window) {
    if (!menuBar || !application || !window) {
        throw std::logic_error("MenuBar binding state is invalid");
    }
    if (auto existingWindow = menuBar->window.lock(); existingWindow &&
        existingWindow != window) {
        throw std::logic_error("A MenuBar cannot belong to multiple Windows");
    }
    if (auto existingApplication = menuBar->application.lock();
        existingApplication && existingApplication != application) {
        throw std::logic_error(
            "A MenuBar cannot be used by multiple Applications");
    }
    ValidateMenuBar(menuBar, application);
    menuBar->window = window;
    menuBar->application = application;
    for (const auto& menu : menuBar->menus) {
        ApplyMenuState(menu, menuBar, application);
    }
}

void DetachMenuBarFromWindow(
    const std::shared_ptr<MenuBarState>& menuBar) noexcept {
    if (!menuBar) return;
    menuBar->window.reset();
    menuBar->application.reset();
    for (const auto& menu : menuBar->menus) {
        DetachMenuState(menu, menuBar);
    }
}

void DispatchMenuItemInvocation(const std::shared_ptr<MenuItemState>& item) {
    if (!item || !item->parent.lock() || !item->menuBar.lock() ||
        !item->enabled) {
        return;
    }
    const auto callback = item->onInvoked;
    if (callback) callback();
}

void DispatchMenuItemAcceleratorInvocation(
    const std::shared_ptr<MenuItemState>& item) {
    if (!item || !item->parent.lock() || !item->menuBar.lock()) return;
    const auto callback = item->onInvoked;
    if (callback) callback();
}

void DispatchMenuOpening(const std::shared_ptr<MenuState>& menu) {
    if (!menu) return;
    const auto callback = menu->onOpening;
    if (callback) callback();
}

} // namespace detail
} // namespace guidexos::appmodel
