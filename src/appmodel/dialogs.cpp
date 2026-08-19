#include "guidexos/appmodel/dialogs.hpp"

#include "guidexos/appmodel/window.hpp"
#include "runtime.hpp"
#include "text_validation.hpp"

#include <stdexcept>
#include <utility>

namespace guidexos::appmodel {

MessageDialogResult MessageDialog::Show(
    Window& owner, std::string message, std::string title,
    MessageDialogButtons buttons, MessageDialogIcon icon) {
    detail::ValidateUtf8(message);
    detail::ValidateUtf8(title);
    return detail::ShowMessageDialog(owner.state_, message, title, buttons, icon);
}

void OpenFileDialog::SetTitle(std::string title) {
    detail::ValidateUtf8(title);
    title_ = std::move(title);
}

void OpenFileDialog::SetInitialDirectory(std::string directory) {
    detail::ValidateUtf8(directory);
    if (directory.empty()) {
        throw std::invalid_argument("File dialog initial directory must not be empty");
    }
    initialDirectory_ = std::move(directory);
}

void OpenFileDialog::AddFilter(std::string description,
                               std::vector<std::string> patterns) {
    detail::ValidateUtf8(description);
    if (description.empty()) {
        throw std::invalid_argument("File dialog filter description must not be empty");
    }
    if (patterns.empty()) {
        throw std::invalid_argument("File dialog filter must contain a pattern");
    }
    for (const auto& pattern : patterns) {
        detail::ValidateUtf8(pattern);
        if (pattern.empty()) {
            throw std::invalid_argument("File dialog filter pattern must not be empty");
        }
    }
    filters_.push_back(FileDialogFilter{std::move(description), std::move(patterns)});
}

std::optional<std::string> OpenFileDialog::Show(Window& owner) const {
    return detail::ShowOpenFileDialog(owner.state_, title_, initialDirectory_, filters_);
}

void SaveFileDialog::SetTitle(std::string title) {
    detail::ValidateUtf8(title);
    title_ = std::move(title);
}

void SaveFileDialog::SetInitialDirectory(std::string directory) {
    detail::ValidateUtf8(directory);
    if (directory.empty()) {
        throw std::invalid_argument("File dialog initial directory must not be empty");
    }
    initialDirectory_ = std::move(directory);
}

void SaveFileDialog::SetSuggestedFileName(std::string fileName) {
    detail::ValidateUtf8(fileName);
    suggestedFileName_ = std::move(fileName);
}

void SaveFileDialog::AddFilter(std::string description,
                               std::vector<std::string> patterns) {
    detail::ValidateUtf8(description);
    if (description.empty()) {
        throw std::invalid_argument("File dialog filter description must not be empty");
    }
    if (patterns.empty()) {
        throw std::invalid_argument("File dialog filter must contain a pattern");
    }
    for (const auto& pattern : patterns) {
        detail::ValidateUtf8(pattern);
        if (pattern.empty()) {
            throw std::invalid_argument("File dialog filter pattern must not be empty");
        }
    }
    filters_.push_back(FileDialogFilter{std::move(description), std::move(patterns)});
}

std::optional<std::string> SaveFileDialog::Show(Window& owner) const {
    return detail::ShowSaveFileDialog(owner.state_, title_, suggestedFileName_,
                                      initialDirectory_, filters_);
}

} // namespace guidexos::appmodel
