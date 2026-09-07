#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace guidexos::appmodel {

// Public text positions count Unicode scalar values, not UTF-8 bytes or
// native UTF-16 code units. A TextRange is half-open: [start, start + length).
// The range is always ordered; selection direction is intentionally not part
// of this milestone.
struct TextRange {
    std::size_t start = 0;
    std::size_t length = 0;

    friend bool operator==(const TextRange&, const TextRange&) noexcept =
        default;
};

namespace detail {
struct ControlState;
struct RadioGroupState;
}

// A ControlRef is a non-owning, platform-neutral identity for a logical
// control. It remains safe when the native realization is detached or the
// owning Window is closed; it becomes invalid when the logical state is no
// longer retained. The reference never exposes a native handle or control ID.
enum class ControlType {
    None,
    Label,
    Button,
    TextBox,
    TextArea,
    ListBox,
    CheckBox,
    RadioButton,
    ComboBox,
    ProgressBar,
    Slider,
};

class TextBoxRef final {
public:
    TextBoxRef() noexcept = default;

    bool IsValid() const noexcept;
    bool HasSelection() const noexcept;
    bool HasText() const noexcept;

    void SelectAll() const;
    void Copy() const;
    void Cut() const;
    void Paste() const;
    void DeleteSelection() const;

private:
    explicit TextBoxRef(std::weak_ptr<detail::ControlState> state) noexcept
        : state_(std::move(state)) {}

    std::weak_ptr<detail::ControlState> state_;

    friend class ControlRef;
};

class TextAreaRef final {
public:
    TextAreaRef() noexcept = default;

    bool IsValid() const noexcept;
    bool HasSelection() const noexcept;
    bool HasText() const noexcept;

    void SelectAll() const;
    void Copy() const;
    void Cut() const;
    void Paste() const;
    void DeleteSelection() const;

private:
    explicit TextAreaRef(std::weak_ptr<detail::ControlState> state) noexcept
        : state_(std::move(state)) {}

    std::weak_ptr<detail::ControlState> state_;

    friend class ControlRef;
};

class ProgressBarRef final {
public:
    ProgressBarRef() noexcept = default;

    bool IsValid() const noexcept;
    int GetMinimum() const noexcept;
    int GetMaximum() const noexcept;
    int GetValue() const noexcept;
    bool IsIndeterminate() const noexcept;

private:
    explicit ProgressBarRef(std::weak_ptr<detail::ControlState> state) noexcept
        : state_(std::move(state)) {}

    std::weak_ptr<detail::ControlState> state_;

    friend class ControlRef;
};

class SliderRef final {
public:
    SliderRef() noexcept = default;

    bool IsValid() const noexcept;
    int GetMinimum() const noexcept;
    int GetMaximum() const noexcept;
    int GetValue() const noexcept;

private:
    explicit SliderRef(std::weak_ptr<detail::ControlState> state) noexcept
        : state_(std::move(state)) {}

    std::weak_ptr<detail::ControlState> state_;

    friend class ControlRef;
};

class ControlRef final {
public:
    ControlRef() noexcept = default;

    bool IsValid() const noexcept;
    ControlType GetType() const noexcept;
    bool HasFocus() const noexcept;
    bool IsEnabled() const noexcept;
    bool Focus() const noexcept;

    // TextBox and TextArea expose narrow editing capability views, while
    // ProgressBar exposes a narrow state view. No generic command interface
    // is implied for other control kinds.
    std::optional<TextBoxRef> AsTextBox() const noexcept;
    std::optional<TextAreaRef> AsTextArea() const noexcept;
    std::optional<ProgressBarRef> AsProgressBar() const noexcept;
    std::optional<SliderRef> AsSlider() const noexcept;

    friend bool operator==(const ControlRef&, const ControlRef&) noexcept;
    friend bool operator!=(const ControlRef&, const ControlRef&) noexcept;

private:
    explicit ControlRef(std::weak_ptr<detail::ControlState> state) noexcept
        : state_(std::move(state)) {}

    std::weak_ptr<detail::ControlState> state_;

    friend class Window;
    friend class Label;
    friend class Button;
    friend class TextBox;
    friend class TextArea;
    friend class ListBox;
    friend class CheckBox;
    friend class RadioButton;
    friend class ComboBox;
    friend class ProgressBar;
    friend class Slider;
};

class Label final {
public:
    explicit Label(std::string text = {});
    ~Label();

    Label(const Label&) = delete;
    Label& operator=(const Label&) = delete;
    Label(Label&&) = delete;
    Label& operator=(Label&&) = delete;

    void SetText(std::string text);
    const std::string& GetText() const noexcept;
    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

class Button final {
public:
    explicit Button(std::string text = {});
    ~Button();

    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;
    Button(Button&&) = delete;
    Button& operator=(Button&&) = delete;

    void SetText(std::string text);
    const std::string& GetText() const noexcept;
    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;
    bool Focus() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

    // Replaces the current callback. The callback executes on the application
    // event-loop thread and is not retained after this Button is destroyed.
    void OnClick(std::function<void()> callback);

    // Programmatic activation uses the same callback dispatch as a native
    // button click. It is useful for platform-neutral tests and accessibility
    // surfaces that may be added later.
    void Click();

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

class TextBox final {
public:
    explicit TextBox(std::string text = {});
    ~TextBox();

    TextBox(const TextBox&) = delete;
    TextBox& operator=(const TextBox&) = delete;
    TextBox(TextBox&&) = delete;
    TextBox& operator=(TextBox&&) = delete;

    void SetText(std::string text);
    const std::string& GetText() const noexcept;
    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;
    bool Focus() const noexcept;

    // Text indexes are Unicode scalar-value indexes. One emoji therefore has
    // one public index even though Windows stores it as two UTF-16 code units.
    // SetCaretIndex rejects values greater than the text's scalar length.
    std::size_t GetCaretIndex() const noexcept;
    void SetCaretIndex(std::size_t index);

    // The returned range is half-open and ordered: [start, start + length).
    // SetSelection rejects ranges that extend beyond the text. It places the
    // caret at the range end. Native backward selection is normalized here.
    TextRange GetSelection() const noexcept;
    void SetSelection(TextRange range);
    void SelectAll();

    // Collapses a non-empty selection to its end; an empty selection is left
    // at its current caret position.
    void ClearSelection();
    std::string GetSelectedText() const;

    // Control-level text commands use the App Model Clipboard service. Copy
    // with an empty selection is a no-op. Cut, Paste, and DeleteSelection each
    // produce at most one TextChanged callback when the text changes.
    void Copy();
    void Cut();
    void Paste();
    void DeleteSelection();

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

    // Replaces the current callback. Passing an empty callback unsubscribes.
    // The callback executes synchronously on the application event-loop
    // thread after text and post-mutation caret/selection state are updated.
    void OnTextChanged(std::function<void(const std::string&)> callback);

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

class TextArea final {
public:
    explicit TextArea(std::string text = {});
    ~TextArea();

    TextArea(const TextArea&) = delete;
    TextArea& operator=(const TextArea&) = delete;
    TextArea(TextArea&&) = delete;
    TextArea& operator=(TextArea&&) = delete;

    void SetText(std::string text);
    const std::string& GetText() const noexcept;
    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;
    bool Focus() const noexcept;

    // Text indexes are Unicode scalar-value indexes in the normalized UTF-8
    // text. Newlines are represented as '\n'; Windows CRLF is private to the
    // native realization.
    std::size_t GetCaretIndex() const noexcept;
    void SetCaretIndex(std::size_t index);
    TextRange GetSelection() const noexcept;
    void SetSelection(TextRange range);
    void SelectAll();
    void ClearSelection();
    std::string GetSelectedText() const;

    void Copy();
    void Cut();
    void Paste();
    void DeleteSelection();

    void SetReadOnly(bool readOnly);
    bool IsReadOnly() const noexcept;

    // Word wrapping is enabled by default. When disabled, the native editor
    // exposes horizontal scrolling privately while retaining vertical scroll.
    void SetWordWrap(bool wordWrap);
    bool IsWordWrap() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

    void OnTextChanged(std::function<void(const std::string&)> callback);

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

class ListBox final {
public:
    ListBox();
    ~ListBox();

    ListBox(const ListBox&) = delete;
    ListBox& operator=(const ListBox&) = delete;
    ListBox(ListBox&&) = delete;
    ListBox& operator=(ListBox&&) = delete;

    void AddItem(std::string text);
    void InsertItem(std::size_t index, std::string text);
    void RemoveItem(std::size_t index);
    void ClearItems();

    std::size_t GetItemCount() const noexcept;
    std::string GetItem(std::size_t index) const;
    void SetItem(std::size_t index, std::string text);

    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;

    ControlRef GetControlRef() const noexcept;
    bool Focus() const noexcept;

    void SetSelectedIndex(std::optional<std::size_t> index);
    std::optional<std::size_t> GetSelectedIndex() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

    // Replaces the current callback. Passing an empty callback unsubscribes.
    // The callback executes synchronously after the model selection is
    // current, and receives a copy of the new optional index.
    void OnSelectionChanged(
        std::function<void(std::optional<std::size_t>)> callback);

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

// A non-editable, single-selection drop-down. Item identity is always the
// zero-based index, so duplicate strings remain distinct.
class ComboBox final {
public:
    ComboBox();
    ~ComboBox();

    ComboBox(const ComboBox&) = delete;
    ComboBox& operator=(const ComboBox&) = delete;
    ComboBox(ComboBox&&) = delete;
    ComboBox& operator=(ComboBox&&) = delete;

    void AddItem(std::string text);
    void InsertItem(std::size_t index, std::string text);
    void RemoveItem(std::size_t index);
    void ClearItems();

    std::size_t GetItemCount() const noexcept;
    std::string GetItem(std::size_t index) const;
    void SetItem(std::size_t index, std::string text);

    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;

    ControlRef GetControlRef() const noexcept;
    bool Focus() const noexcept;

    void SetSelectedIndex(std::optional<std::size_t> index);
    std::optional<std::size_t> GetSelectedIndex() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

    // Replaces the current callback. Passing an empty callback unsubscribes.
    // The callback executes synchronously after the model selection is
    // current, and receives a copy of the new optional index.
    void OnSelectionChanged(
        std::function<void(std::optional<std::size_t>)> callback);

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

class ProgressBar final {
public:
    ProgressBar();
    ~ProgressBar();

    ProgressBar(const ProgressBar&) = delete;
    ProgressBar& operator=(const ProgressBar&) = delete;
    ProgressBar(ProgressBar&&) = delete;
    ProgressBar& operator=(ProgressBar&&) = delete;

    // The model always preserves minimum <= value <= maximum. If a range
    // endpoint would cross the other endpoint, it is clamped to that endpoint;
    // changing an endpoint also clamps the current value into the new range.
    void SetMinimum(int minimum);
    int GetMinimum() const noexcept;
    void SetMaximum(int maximum);
    int GetMaximum() const noexcept;
    void SetValue(int value);
    int GetValue() const noexcept;

    // Indeterminate mode changes only the presentation. The configured range
    // and value remain stored and are restored when determinate mode resumes.
    void SetIndeterminate(bool indeterminate);
    bool IsIndeterminate() const noexcept;

    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

class Slider final {
public:
    Slider();
    ~Slider();

    Slider(const Slider&) = delete;
    Slider& operator=(const Slider&) = delete;
    Slider(Slider&&) = delete;
    Slider& operator=(Slider&&) = delete;

    // The model always preserves minimum <= value <= maximum. If a range
    // endpoint would cross the other endpoint, it is clamped to that
    // endpoint; changing an endpoint also clamps the current value into the
    // new range.
    void SetMinimum(int minimum);
    int GetMinimum() const noexcept;
    void SetMaximum(int maximum);
    int GetMaximum() const noexcept;
    void SetValue(int value);
    int GetValue() const noexcept;

    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;
    bool Focus() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

    // Replaces the current callback. Passing an empty callback unsubscribes.
    // The callback executes synchronously after the model value is current.
    // Programmatic and user-originated changes use the same changed-value
    // dispatch path; assigning the effective current value is a no-op.
    void OnChanged(std::function<void()> callback);

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

class CheckBox final {
public:
    explicit CheckBox(std::string text = {}, bool checked = false);
    ~CheckBox();

    CheckBox(const CheckBox&) = delete;
    CheckBox& operator=(const CheckBox&) = delete;
    CheckBox(CheckBox&&) = delete;
    CheckBox& operator=(CheckBox&&) = delete;

    void SetText(std::string text);
    const std::string& GetText() const noexcept;
    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;
    bool Focus() const noexcept;

    void SetChecked(bool checked);
    bool IsChecked() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

    // Replaces the current callback. Passing an empty callback unsubscribes.
    // The callback executes synchronously after the model state is current.
    void OnCheckedChanged(std::function<void(bool)> callback);

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

class RadioGroup;

class RadioButton final {
public:
    explicit RadioButton(std::string text = {});
    ~RadioButton();

    RadioButton(const RadioButton&) = delete;
    RadioButton& operator=(const RadioButton&) = delete;
    RadioButton(RadioButton&&) = delete;
    RadioButton& operator=(RadioButton&&) = delete;

    void SetText(std::string text);
    const std::string& GetText() const noexcept;
    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;
    bool Focus() const noexcept;

    bool IsSelected() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

    // Replaces the current callback. Passing an empty callback unsubscribes.
    // Selection is normally changed through RadioGroup::Select().
    void OnSelectedChanged(std::function<void(bool)> callback);

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
    friend class RadioGroup;
};

class RadioGroup final {
public:
    RadioGroup();
    ~RadioGroup();

    RadioGroup(const RadioGroup&) = delete;
    RadioGroup& operator=(const RadioGroup&) = delete;
    RadioGroup(RadioGroup&&) = delete;
    RadioGroup& operator=(RadioGroup&&) = delete;

    void Add(RadioButton& button);
    void Remove(RadioButton& button);
    bool Contains(const RadioButton& button) const noexcept;
    std::size_t GetMemberCount() const noexcept;

    std::optional<std::size_t> GetSelectedIndex() const noexcept;
    void Select(RadioButton& button);
    void ClearSelection();

private:
    std::shared_ptr<detail::RadioGroupState> state_;
};

} // namespace guidexos::appmodel
