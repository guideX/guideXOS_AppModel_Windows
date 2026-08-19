#pragma once

#include <optional>
#include <string>
#include <vector>

namespace guidexos::appmodel {

class Window;

enum class MessageDialogButtons {
    Ok,
    OkCancel,
    YesNo,
    YesNoCancel,
};

enum class MessageDialogResult {
    Ok,
    Cancel,
    Yes,
    No,
};

enum class MessageDialogIcon {
    None,
    Information,
    Warning,
    Error,
    Question,
};

class MessageDialog final {
public:
    static MessageDialogResult Show(
        Window& owner, std::string message, std::string title,
        MessageDialogButtons buttons = MessageDialogButtons::Ok,
        MessageDialogIcon icon = MessageDialogIcon::None);
};

struct FileDialogFilter {
    std::string description;
    std::vector<std::string> patterns;
};

class OpenFileDialog final {
public:
    OpenFileDialog() = default;

    void SetTitle(std::string title);
    void SetInitialDirectory(std::string directory);
    void AddFilter(std::string description,
                   std::vector<std::string> patterns);

    std::optional<std::string> Show(Window& owner) const;

private:
    std::string title_ = "Open";
    std::optional<std::string> initialDirectory_;
    std::vector<FileDialogFilter> filters_;
};

class SaveFileDialog final {
public:
    SaveFileDialog() = default;

    void SetTitle(std::string title);
    void SetInitialDirectory(std::string directory);
    void SetSuggestedFileName(std::string fileName);
    void AddFilter(std::string description,
                   std::vector<std::string> patterns);

    std::optional<std::string> Show(Window& owner) const;

private:
    std::string title_ = "Save As";
    std::optional<std::string> initialDirectory_;
    std::string suggestedFileName_;
    std::vector<FileDialogFilter> filters_;
};

} // namespace guidexos::appmodel
