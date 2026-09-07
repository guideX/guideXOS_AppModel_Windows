#include "guidexos/appmodel/controls.hpp"

#include "guidexos/appmodel/clipboard.hpp"
#include "runtime.hpp"
#include "text_index.hpp"
#include "text_validation.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace guidexos::appmodel {

using detail::ValidateUtf8;

namespace {

constexpr std::size_t kMaximumToolTipBytes = 8U * 1024U;

int NormalizeMinimum(int minimum, int maximum) noexcept {
    return std::min(minimum, maximum);
}

int NormalizeMaximum(int maximum, int minimum) noexcept {
    return std::max(maximum, minimum);
}

int ClampValue(int value, int minimum, int maximum) noexcept {
    return std::clamp(value, minimum, maximum);
}

void SetControlToolTip(const std::shared_ptr<detail::ControlState>& state,
                       std::string text) {
    ValidateUtf8(text);
    if (text.size() > kMaximumToolTipBytes) {
        throw std::length_error("Control ToolTip exceeds the 8 KiB limit");
    }
    if (state->toolTip == text) return;
    state->toolTip = std::move(text);
    detail::NotifyControlChanged(state);
}

std::size_t RangeEnd(TextRange range) {
    if (range.start > std::numeric_limits<std::size_t>::max() - range.length) {
        throw std::out_of_range("TextBox selection range is out of range");
    }
    return range.start + range.length;
}

void ValidateTextRange(const std::string& text, TextRange range) {
    const std::size_t end = RangeEnd(range);
    if (end > detail::CountUnicodeScalars(text)) {
        throw std::out_of_range("TextBox selection range is out of range");
    }
}

std::string NormalizeTextAreaNewlines(std::string text) {
    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\r') {
            normalized.push_back('\n');
            if (index + 1 < text.size() && text[index + 1] == '\n') ++index;
        } else {
            normalized.push_back(text[index]);
        }
    }
    return normalized;
}

void SetTextPosition(const std::shared_ptr<detail::ControlState>& state,
                     std::size_t caret, TextRange selection) {
    state->caretIndex = caret;
    state->selection = selection;
}

void ReplaceTextSelection(const std::shared_ptr<detail::ControlState>& state,
                          std::string replacement) {
    if (!state || (state->kind == detail::ControlKind::TextArea &&
                   state->readOnly)) {
        return;
    }
    ValidateUtf8(replacement);
    const TextRange range = state->selection;
    const std::size_t rangeEnd = RangeEnd(range);
    const std::size_t startByte = detail::Utf8ByteOffsetForScalarIndex(
        state->text, range.start);
    const std::size_t endByte = detail::Utf8ByteOffsetForScalarIndex(
        state->text, rangeEnd);
    std::string next = state->text.substr(0, startByte);
    next += replacement;
    next += state->text.substr(endByte);

    const std::size_t caret = range.start + detail::CountUnicodeScalars(replacement);
    SetTextPosition(state, caret, TextRange{caret, 0});
    if (next == state->text) {
        detail::NotifyControlChanged(state);
        return;
    }
    detail::DispatchTextChanged(state, std::move(next));
}

std::shared_ptr<detail::ControlState> LockTextBox(
    const std::weak_ptr<detail::ControlState>& weakState) noexcept {
    auto state = weakState.lock();
    if (!state || state->kind != detail::ControlKind::TextBox) return nullptr;
    return state;
}

std::shared_ptr<detail::ControlState> LockTextArea(
    const std::weak_ptr<detail::ControlState>& weakState) noexcept {
    auto state = weakState.lock();
    if (!state || state->kind != detail::ControlKind::TextArea) return nullptr;
    return state;
}

std::shared_ptr<detail::ControlState> LockProgressBar(
    const std::weak_ptr<detail::ControlState>& weakState) noexcept {
    auto state = weakState.lock();
    if (!state || state->kind != detail::ControlKind::ProgressBar) {
        return nullptr;
    }
    return state;
}

std::shared_ptr<detail::ControlState> LockSlider(
    const std::weak_ptr<detail::ControlState>& weakState) noexcept {
    auto state = weakState.lock();
    if (!state || state->kind != detail::ControlKind::Slider) {
        return nullptr;
    }
    return state;
}

ControlType GetControlType(detail::ControlKind kind) noexcept {
    switch (kind) {
    case detail::ControlKind::Label: return ControlType::Label;
    case detail::ControlKind::Button: return ControlType::Button;
    case detail::ControlKind::TextBox: return ControlType::TextBox;
    case detail::ControlKind::TextArea: return ControlType::TextArea;
    case detail::ControlKind::ListBox: return ControlType::ListBox;
    case detail::ControlKind::CheckBox: return ControlType::CheckBox;
    case detail::ControlKind::RadioButton: return ControlType::RadioButton;
    case detail::ControlKind::ComboBox: return ControlType::ComboBox;
    case detail::ControlKind::ProgressBar: return ControlType::ProgressBar;
    case detail::ControlKind::Slider: return ControlType::Slider;
    }
    return ControlType::None;
}

} // namespace

bool TextBoxRef::IsValid() const noexcept {
    return static_cast<bool>(LockTextBox(state_));
}

bool TextBoxRef::HasSelection() const noexcept {
    const auto state = LockTextBox(state_);
    return state && state->selection.length != 0;
}

bool TextBoxRef::HasText() const noexcept {
    const auto state = LockTextBox(state_);
    return state && !state->text.empty();
}

void TextBoxRef::SelectAll() const {
    const auto state = LockTextBox(state_);
    if (!state) return;
    const TextRange range{0, detail::CountUnicodeScalars(state->text)};
    ValidateTextRange(state->text, range);
    const std::size_t end = RangeEnd(range);
    if (state->caretIndex == end && state->selection == range) return;
    SetTextPosition(state, end, range);
    detail::NotifyControlChanged(state);
}

void TextBoxRef::Copy() const {
    const auto state = LockTextBox(state_);
    if (!state || state->selection.length == 0) return;
    const TextRange range = state->selection;
    const std::size_t end = RangeEnd(range);
    const std::size_t startByte = detail::Utf8ByteOffsetForScalarIndex(
        state->text, range.start);
    const std::size_t endByte = detail::Utf8ByteOffsetForScalarIndex(
        state->text, end);
    Clipboard::SetText(state->text.substr(startByte, endByte - startByte));
}

void TextBoxRef::Cut() const {
    const auto state = LockTextBox(state_);
    if (!state || state->selection.length == 0) return;
    Copy();
    ReplaceTextSelection(state, {});
}

void TextBoxRef::Paste() const {
    const auto state = LockTextBox(state_);
    if (!state || !Clipboard::HasText()) return;
    ReplaceTextSelection(state, Clipboard::GetText());
}

void TextBoxRef::DeleteSelection() const {
    const auto state = LockTextBox(state_);
    if (!state || state->selection.length == 0) return;
    ReplaceTextSelection(state, {});
}

bool TextAreaRef::IsValid() const noexcept {
    return static_cast<bool>(LockTextArea(state_));
}

bool ProgressBarRef::IsValid() const noexcept {
    return static_cast<bool>(LockProgressBar(state_));
}

int ProgressBarRef::GetMinimum() const noexcept {
    const auto state = LockProgressBar(state_);
    return state ? state->minimum : 0;
}

int ProgressBarRef::GetMaximum() const noexcept {
    const auto state = LockProgressBar(state_);
    return state ? state->maximum : 0;
}

int ProgressBarRef::GetValue() const noexcept {
    const auto state = LockProgressBar(state_);
    return state ? state->value : 0;
}

bool ProgressBarRef::IsIndeterminate() const noexcept {
    const auto state = LockProgressBar(state_);
    return state && state->indeterminate;
}

bool SliderRef::IsValid() const noexcept {
    return static_cast<bool>(LockSlider(state_));
}

int SliderRef::GetMinimum() const noexcept {
    const auto state = LockSlider(state_);
    return state ? state->minimum : 0;
}

int SliderRef::GetMaximum() const noexcept {
    const auto state = LockSlider(state_);
    return state ? state->maximum : 0;
}

int SliderRef::GetValue() const noexcept {
    const auto state = LockSlider(state_);
    return state ? state->value : 0;
}

bool TextAreaRef::HasSelection() const noexcept {
    const auto state = LockTextArea(state_);
    return state && state->selection.length != 0;
}

bool TextAreaRef::HasText() const noexcept {
    const auto state = LockTextArea(state_);
    return state && !state->text.empty();
}

void TextAreaRef::SelectAll() const {
    const auto state = LockTextArea(state_);
    if (!state) return;
    const TextRange range{0, detail::CountUnicodeScalars(state->text)};
    const std::size_t end = RangeEnd(range);
    if (state->caretIndex == end && state->selection == range) return;
    SetTextPosition(state, end, range);
    detail::NotifyControlChanged(state);
}

void TextAreaRef::Copy() const {
    const auto state = LockTextArea(state_);
    if (!state || state->selection.length == 0) return;
    const TextRange range = state->selection;
    const std::size_t end = RangeEnd(range);
    const std::size_t startByte = detail::Utf8ByteOffsetForScalarIndex(
        state->text, range.start);
    const std::size_t endByte = detail::Utf8ByteOffsetForScalarIndex(
        state->text, end);
    Clipboard::SetText(state->text.substr(startByte, endByte - startByte));
}

void TextAreaRef::Cut() const {
    const auto state = LockTextArea(state_);
    if (!state || state->readOnly || state->selection.length == 0) return;
    Copy();
    ReplaceTextSelection(state, {});
}

void TextAreaRef::Paste() const {
    const auto state = LockTextArea(state_);
    if (!state || state->readOnly || !Clipboard::HasText()) return;
    ReplaceTextSelection(state, Clipboard::GetText());
}

void TextAreaRef::DeleteSelection() const {
    const auto state = LockTextArea(state_);
    if (!state || state->readOnly || state->selection.length == 0) return;
    ReplaceTextSelection(state, {});
}

bool ControlRef::IsValid() const noexcept {
    return !state_.expired();
}

ControlType ControlRef::GetType() const noexcept {
    const auto state = state_.lock();
    return state ? GetControlType(state->kind) : ControlType::None;
}

bool ControlRef::HasFocus() const noexcept {
    return detail::IsControlFocused(state_.lock());
}

bool ControlRef::IsEnabled() const noexcept {
    const auto state = state_.lock();
    return state && state->enabled;
}

bool ControlRef::Focus() const noexcept {
    return detail::FocusControl(state_.lock());
}

std::optional<TextBoxRef> ControlRef::AsTextBox() const noexcept {
    const auto state = state_.lock();
    if (!state || state->kind != detail::ControlKind::TextBox) return std::nullopt;
    return TextBoxRef{state};
}

std::optional<TextAreaRef> ControlRef::AsTextArea() const noexcept {
    const auto state = state_.lock();
    if (!state || state->kind != detail::ControlKind::TextArea) {
        return std::nullopt;
    }
    return TextAreaRef{state};
}

std::optional<ProgressBarRef> ControlRef::AsProgressBar() const noexcept {
    const auto state = state_.lock();
    if (!state || state->kind != detail::ControlKind::ProgressBar) {
        return std::nullopt;
    }
    return ProgressBarRef{state};
}

std::optional<SliderRef> ControlRef::AsSlider() const noexcept {
    const auto state = state_.lock();
    if (!state || state->kind != detail::ControlKind::Slider) {
        return std::nullopt;
    }
    return SliderRef{state};
}

bool operator==(const ControlRef& left, const ControlRef& right) noexcept {
    const auto leftState = left.state_.lock();
    const auto rightState = right.state_.lock();
    return leftState && rightState && leftState == rightState;
}

bool operator!=(const ControlRef& left, const ControlRef& right) noexcept {
    return !(left == right);
}

Label::Label(std::string text)
    : state_(nullptr) {
    ValidateUtf8(text);
    state_ = std::make_shared<detail::ControlState>(detail::ControlKind::Label,
                                                    std::move(text));
}

Label::~Label() = default;

void Label::SetText(std::string text) {
    ValidateUtf8(text);
    if (state_->text == text) return;
    state_->text = std::move(text);
    detail::NotifyControlChanged(state_);
}

const std::string& Label::GetText() const noexcept {
    return state_->text;
}

void Label::SetToolTip(std::string text) {
    SetControlToolTip(state_, std::move(text));
}

const std::string& Label::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef Label::GetControlRef() const noexcept {
    return ControlRef{state_};
}

Button::Button(std::string text)
    : state_(nullptr) {
    ValidateUtf8(text);
    state_ = std::make_shared<detail::ControlState>(detail::ControlKind::Button,
                                                    std::move(text));
}

Button::~Button() {
    // Layouts and backend bindings may retain the model state after the
    // public Button object goes away. Clearing the callback makes that state
    // inert instead of retaining a callback with references into dead code.
    state_->onClick = {};
}

void Button::SetText(std::string text) {
    ValidateUtf8(text);
    if (state_->text == text) return;
    state_->text = std::move(text);
    detail::NotifyControlChanged(state_);
}

const std::string& Button::GetText() const noexcept {
    return state_->text;
}

void Button::SetToolTip(std::string text) {
    SetControlToolTip(state_, std::move(text));
}

const std::string& Button::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef Button::GetControlRef() const noexcept {
    return ControlRef{state_};
}

bool Button::Focus() const noexcept {
    return detail::FocusControl(state_);
}

void Button::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool Button::IsEnabled() const noexcept {
    return state_->enabled;
}

void Button::OnClick(std::function<void()> callback) {
    state_->onClick = std::move(callback);
}

void Button::Click() {
    detail::DispatchButtonClick(state_);
}

TextBox::TextBox(std::string text)
    : state_(nullptr) {
    ValidateUtf8(text);
    state_ = std::make_shared<detail::ControlState>(detail::ControlKind::TextBox,
                                                    std::move(text));
    state_->caretIndex = detail::CountUnicodeScalars(state_->text);
    state_->selection = TextRange{state_->caretIndex, 0};
}

TextBox::~TextBox() {
    // A layout or native binding may retain the model state after this public
    // object goes away. Do not retain a callback with references into dead code.
    state_->onTextChanged = {};
}

void TextBox::SetText(std::string text) {
    ValidateUtf8(text);
    const std::size_t end = detail::CountUnicodeScalars(text);
    SetTextPosition(state_, end, TextRange{end, 0});
    if (state_->text == text) {
        detail::NotifyControlChanged(state_);
        return;
    }
    detail::DispatchTextChanged(state_, std::move(text));
}

const std::string& TextBox::GetText() const noexcept {
    return state_->text;
}

void TextBox::SetToolTip(std::string text) {
    SetControlToolTip(state_, std::move(text));
}

const std::string& TextBox::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef TextBox::GetControlRef() const noexcept {
    return ControlRef{state_};
}

bool TextBox::Focus() const noexcept {
    return detail::FocusControl(state_);
}

std::size_t TextBox::GetCaretIndex() const noexcept {
    return state_->caretIndex;
}

void TextBox::SetCaretIndex(std::size_t index) {
    if (index > detail::CountUnicodeScalars(state_->text)) {
        throw std::out_of_range("TextBox caret index is out of range");
    }
    const TextRange selection{index, 0};
    if (state_->caretIndex == index && state_->selection == selection) return;
    SetTextPosition(state_, index, selection);
    detail::NotifyControlChanged(state_);
}

TextRange TextBox::GetSelection() const noexcept {
    return state_->selection;
}

void TextBox::SetSelection(TextRange range) {
    ValidateTextRange(state_->text, range);
    const std::size_t end = RangeEnd(range);
    if (state_->caretIndex == end && state_->selection == range) return;
    SetTextPosition(state_, end, range);
    detail::NotifyControlChanged(state_);
}

void TextBox::SelectAll() {
    SetSelection(TextRange{0, detail::CountUnicodeScalars(state_->text)});
}

void TextBox::ClearSelection() {
    const std::size_t endpoint = RangeEnd(state_->selection);
    SetCaretIndex(endpoint);
}

std::string TextBox::GetSelectedText() const {
    const TextRange range = state_->selection;
    const std::size_t end = RangeEnd(range);
    const std::size_t startByte = detail::Utf8ByteOffsetForScalarIndex(
        state_->text, range.start);
    const std::size_t endByte = detail::Utf8ByteOffsetForScalarIndex(
        state_->text, end);
    return state_->text.substr(startByte, endByte - startByte);
}

void TextBox::Copy() {
    if (state_->selection.length == 0) return;
    Clipboard::SetText(GetSelectedText());
}

void TextBox::Cut() {
    if (state_->selection.length == 0) return;
    Clipboard::SetText(GetSelectedText());
    ReplaceTextSelection(state_, {});
}

void TextBox::Paste() {
    if (!Clipboard::HasText()) return;
    ReplaceTextSelection(state_, Clipboard::GetText());
}

void TextBox::DeleteSelection() {
    if (state_->selection.length == 0) return;
    ReplaceTextSelection(state_, {});
}

void TextBox::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool TextBox::IsEnabled() const noexcept {
    return state_->enabled;
}

void TextBox::OnTextChanged(std::function<void(const std::string&)> callback) {
    state_->onTextChanged = std::move(callback);
}

TextArea::TextArea(std::string text)
    : state_(nullptr) {
    text = NormalizeTextAreaNewlines(std::move(text));
    ValidateUtf8(text);
    state_ = std::make_shared<detail::ControlState>(detail::ControlKind::TextArea,
                                                    std::move(text));
    state_->caretIndex = detail::CountUnicodeScalars(state_->text);
    state_->selection = TextRange{state_->caretIndex, 0};
}

TextArea::~TextArea() {
    state_->onTextChanged = {};
}

void TextArea::SetText(std::string text) {
    text = NormalizeTextAreaNewlines(std::move(text));
    ValidateUtf8(text);
    const std::size_t end = detail::CountUnicodeScalars(text);
    SetTextPosition(state_, end, TextRange{end, 0});
    if (state_->text == text) {
        detail::NotifyControlChanged(state_);
        return;
    }
    detail::DispatchTextChanged(state_, std::move(text));
}

const std::string& TextArea::GetText() const noexcept {
    return state_->text;
}

void TextArea::SetToolTip(std::string text) {
    SetControlToolTip(state_, std::move(text));
}

const std::string& TextArea::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef TextArea::GetControlRef() const noexcept {
    return ControlRef{state_};
}

bool TextArea::Focus() const noexcept {
    return detail::FocusControl(state_);
}

std::size_t TextArea::GetCaretIndex() const noexcept {
    return state_->caretIndex;
}

void TextArea::SetCaretIndex(std::size_t index) {
    if (index > detail::CountUnicodeScalars(state_->text)) {
        throw std::out_of_range("TextArea caret index is out of range");
    }
    const TextRange selection{index, 0};
    if (state_->caretIndex == index && state_->selection == selection) return;
    SetTextPosition(state_, index, selection);
    detail::NotifyControlChanged(state_);
}

TextRange TextArea::GetSelection() const noexcept {
    return state_->selection;
}

void TextArea::SetSelection(TextRange range) {
    ValidateTextRange(state_->text, range);
    const std::size_t end = RangeEnd(range);
    if (state_->caretIndex == end && state_->selection == range) return;
    SetTextPosition(state_, end, range);
    detail::NotifyControlChanged(state_);
}

void TextArea::SelectAll() {
    SetSelection(TextRange{0, detail::CountUnicodeScalars(state_->text)});
}

void TextArea::ClearSelection() {
    SetCaretIndex(RangeEnd(state_->selection));
}

std::string TextArea::GetSelectedText() const {
    const TextRange range = state_->selection;
    const std::size_t end = RangeEnd(range);
    const std::size_t startByte = detail::Utf8ByteOffsetForScalarIndex(
        state_->text, range.start);
    const std::size_t endByte = detail::Utf8ByteOffsetForScalarIndex(
        state_->text, end);
    return state_->text.substr(startByte, endByte - startByte);
}

void TextArea::Copy() {
    if (state_->selection.length == 0) return;
    Clipboard::SetText(GetSelectedText());
}

void TextArea::Cut() {
    if (state_->readOnly || state_->selection.length == 0) return;
    Clipboard::SetText(GetSelectedText());
    ReplaceTextSelection(state_, {});
}

void TextArea::Paste() {
    if (state_->readOnly || !Clipboard::HasText()) return;
    ReplaceTextSelection(state_, Clipboard::GetText());
}

void TextArea::DeleteSelection() {
    if (state_->readOnly || state_->selection.length == 0) return;
    ReplaceTextSelection(state_, {});
}

void TextArea::SetReadOnly(bool readOnly) {
    if (state_->readOnly == readOnly) return;
    state_->readOnly = readOnly;
    detail::NotifyControlChanged(state_);
}

bool TextArea::IsReadOnly() const noexcept {
    return state_->readOnly;
}

void TextArea::SetWordWrap(bool wordWrap) {
    if (state_->wordWrap == wordWrap) return;
    state_->wordWrap = wordWrap;
    detail::NotifyControlChanged(state_);
}

bool TextArea::IsWordWrap() const noexcept {
    return state_->wordWrap;
}

void TextArea::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool TextArea::IsEnabled() const noexcept {
    return state_->enabled;
}

void TextArea::OnTextChanged(
    std::function<void(const std::string&)> callback) {
    state_->onTextChanged = std::move(callback);
}

void AddCollectionItem(detail::ControlState& state, std::string text) {
    ValidateUtf8(text);
    state.items.push_back(std::move(text));
}

void InsertCollectionItem(detail::ControlState& state, std::size_t index,
                          std::string text, const char* controlName) {
    if (index > state.items.size()) {
        throw std::out_of_range(std::string(controlName) +
                                " item index is out of range");
    }
    ValidateUtf8(text);
    state.items.insert(state.items.begin() + static_cast<std::ptrdiff_t>(index),
                       std::move(text));
    if (state.selectedIndex && *state.selectedIndex >= index) {
        ++*state.selectedIndex;
    }
}

void RemoveCollectionItem(detail::ControlState& state, std::size_t index,
                          const char* controlName) {
    if (index >= state.items.size()) {
        throw std::out_of_range(std::string(controlName) +
                                " item index is out of range");
    }

    state.items.erase(state.items.begin() + static_cast<std::ptrdiff_t>(index));
    if (state.selectedIndex) {
        if (*state.selectedIndex == index) {
            state.selectedIndex.reset();
        } else if (*state.selectedIndex > index) {
            --*state.selectedIndex;
        }
    }
}

void ClearCollectionItems(detail::ControlState& state) {
    state.items.clear();
    state.selectedIndex.reset();
}

std::string GetCollectionItem(const detail::ControlState& state,
                              std::size_t index, const char* controlName) {
    if (index >= state.items.size()) {
        throw std::out_of_range(std::string(controlName) +
                                " item index is out of range");
    }
    return state.items[index];
}

void SetCollectionItem(detail::ControlState& state, std::size_t index,
                       std::string text, const char* controlName) {
    if (index >= state.items.size()) {
        throw std::out_of_range(std::string(controlName) +
                                " item index is out of range");
    }
    ValidateUtf8(text);
    if (state.items[index] == text) return;
    state.items[index] = std::move(text);
}

void ValidateSelection(const detail::ControlState& state,
                       std::optional<std::size_t> index,
                       const char* controlName) {
    if (index && *index >= state.items.size()) {
        throw std::out_of_range(std::string(controlName) +
                                " selection index is out of range");
    }
}

ListBox::ListBox()
    : state_(std::make_shared<detail::ControlState>(detail::ControlKind::ListBox,
                                                    std::string{})) {}

ListBox::~ListBox() {
    // A layout or native binding may retain the model state after this public
    // object goes away. Do not retain a callback with references into dead code.
    state_->onSelectionChanged = {};
}

void ListBox::AddItem(std::string text) {
    AddCollectionItem(*state_, std::move(text));
    detail::NotifyControlChanged(state_);
}

void ListBox::InsertItem(std::size_t index, std::string text) {
    InsertCollectionItem(*state_, index, std::move(text), "ListBox");
    detail::NotifyControlChanged(state_);
}

void ListBox::RemoveItem(std::size_t index) {
    const bool selectionRemoved = state_->selectedIndex &&
        *state_->selectedIndex == index;
    RemoveCollectionItem(*state_, index, "ListBox");
    detail::NotifyControlChanged(state_);
    if (selectionRemoved) detail::DispatchSelectionCallback(state_);
}

void ListBox::ClearItems() {
    const bool selectionChanged = state_->selectedIndex.has_value();
    ClearCollectionItems(*state_);
    detail::NotifyControlChanged(state_);
    if (selectionChanged) detail::DispatchSelectionCallback(state_);
}

std::size_t ListBox::GetItemCount() const noexcept {
    return state_->items.size();
}

std::string ListBox::GetItem(std::size_t index) const {
    return GetCollectionItem(*state_, index, "ListBox");
}

void ListBox::SetToolTip(std::string text) {
    SetControlToolTip(state_, std::move(text));
}

const std::string& ListBox::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef ListBox::GetControlRef() const noexcept {
    return ControlRef{state_};
}

bool ListBox::Focus() const noexcept {
    return detail::FocusControl(state_);
}

void ListBox::SetItem(std::size_t index, std::string text) {
    const auto previous = GetCollectionItem(*state_, index, "ListBox");
    SetCollectionItem(*state_, index, std::move(text), "ListBox");
    if (state_->items[index] != previous) detail::NotifyControlChanged(state_);
}

void ListBox::SetSelectedIndex(std::optional<std::size_t> index) {
    ValidateSelection(*state_, index, "ListBox");
    detail::DispatchSelectionChanged(state_, index);
}

std::optional<std::size_t> ListBox::GetSelectedIndex() const noexcept {
    return state_->selectedIndex;
}

void ListBox::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool ListBox::IsEnabled() const noexcept {
    return state_->enabled;
}

void ListBox::OnSelectionChanged(
    std::function<void(std::optional<std::size_t>)> callback) {
    state_->onSelectionChanged = std::move(callback);
}

ComboBox::ComboBox()
    : state_(std::make_shared<detail::ControlState>(detail::ControlKind::ComboBox,
                                                    std::string{})) {}

ComboBox::~ComboBox() {
    // A layout or native binding may retain the model state after this public
    // object goes away. Do not retain a callback with references into dead code.
    state_->onSelectionChanged = {};
}

void ComboBox::AddItem(std::string text) {
    AddCollectionItem(*state_, std::move(text));
    detail::NotifyControlChanged(state_);
}

void ComboBox::InsertItem(std::size_t index, std::string text) {
    InsertCollectionItem(*state_, index, std::move(text), "ComboBox");
    detail::NotifyControlChanged(state_);
}

void ComboBox::RemoveItem(std::size_t index) {
    const bool selectionRemoved = state_->selectedIndex &&
        *state_->selectedIndex == index;
    RemoveCollectionItem(*state_, index, "ComboBox");
    detail::NotifyControlChanged(state_);
    if (selectionRemoved) detail::DispatchSelectionCallback(state_);
}

void ComboBox::ClearItems() {
    const bool selectionChanged = state_->selectedIndex.has_value();
    ClearCollectionItems(*state_);
    detail::NotifyControlChanged(state_);
    if (selectionChanged) detail::DispatchSelectionCallback(state_);
}

std::size_t ComboBox::GetItemCount() const noexcept {
    return state_->items.size();
}

std::string ComboBox::GetItem(std::size_t index) const {
    return GetCollectionItem(*state_, index, "ComboBox");
}

void ComboBox::SetToolTip(std::string text) {
    SetControlToolTip(state_, std::move(text));
}

const std::string& ComboBox::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef ComboBox::GetControlRef() const noexcept {
    return ControlRef{state_};
}

bool ComboBox::Focus() const noexcept {
    return detail::FocusControl(state_);
}

void ComboBox::SetItem(std::size_t index, std::string text) {
    const auto previous = GetCollectionItem(*state_, index, "ComboBox");
    SetCollectionItem(*state_, index, std::move(text), "ComboBox");
    if (state_->items[index] != previous) detail::NotifyControlChanged(state_);
}

void ComboBox::SetSelectedIndex(std::optional<std::size_t> index) {
    ValidateSelection(*state_, index, "ComboBox");
    detail::DispatchSelectionChanged(state_, index);
}

std::optional<std::size_t> ComboBox::GetSelectedIndex() const noexcept {
    return state_->selectedIndex;
}

void ComboBox::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool ComboBox::IsEnabled() const noexcept {
    return state_->enabled;
}

void ComboBox::OnSelectionChanged(
    std::function<void(std::optional<std::size_t>)> callback) {
    state_->onSelectionChanged = std::move(callback);
}

ProgressBar::ProgressBar()
    : state_(std::make_shared<detail::ControlState>(
          detail::ControlKind::ProgressBar, std::string{})) {}

ProgressBar::~ProgressBar() = default;

void ProgressBar::SetMinimum(int minimum) {
    const int nextMinimum = NormalizeMinimum(minimum, state_->maximum);
    const int nextValue = std::max(state_->value, nextMinimum);
    if (state_->minimum == nextMinimum && state_->value == nextValue) return;
    state_->minimum = nextMinimum;
    state_->value = nextValue;
    detail::NotifyControlChanged(state_);
}

int ProgressBar::GetMinimum() const noexcept {
    return state_->minimum;
}

void ProgressBar::SetMaximum(int maximum) {
    const int nextMaximum = NormalizeMaximum(maximum, state_->minimum);
    const int nextValue = std::min(state_->value, nextMaximum);
    if (state_->maximum == nextMaximum && state_->value == nextValue) return;
    state_->maximum = nextMaximum;
    state_->value = nextValue;
    detail::NotifyControlChanged(state_);
}

int ProgressBar::GetMaximum() const noexcept {
    return state_->maximum;
}

void ProgressBar::SetValue(int value) {
    const int nextValue = ClampValue(value, state_->minimum, state_->maximum);
    if (state_->value == nextValue) return;
    state_->value = nextValue;
    detail::NotifyControlChanged(state_);
}

int ProgressBar::GetValue() const noexcept {
    return state_->value;
}

void ProgressBar::SetIndeterminate(bool indeterminate) {
    if (state_->indeterminate == indeterminate) return;
    state_->indeterminate = indeterminate;
    detail::NotifyControlChanged(state_);
}

bool ProgressBar::IsIndeterminate() const noexcept {
    return state_->indeterminate;
}

void ProgressBar::SetToolTip(std::string text) {
    SetControlToolTip(state_, std::move(text));
}

const std::string& ProgressBar::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef ProgressBar::GetControlRef() const noexcept {
    return ControlRef{state_};
}

void ProgressBar::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool ProgressBar::IsEnabled() const noexcept {
    return state_->enabled;
}

Slider::Slider()
    : state_(std::make_shared<detail::ControlState>(
          detail::ControlKind::Slider, std::string{})) {}

Slider::~Slider() {
    state_->onChanged = {};
}

void Slider::SetMinimum(int minimum) {
    const int nextMinimum = NormalizeMinimum(minimum, state_->maximum);
    const int nextValue = std::max(state_->value, nextMinimum);
    const bool rangeChanged = state_->minimum != nextMinimum;
    const bool valueChanged = state_->value != nextValue;
    if (!rangeChanged && !valueChanged) return;

    state_->minimum = nextMinimum;
    if (valueChanged) {
        detail::DispatchSliderChanged(state_, nextValue);
    } else {
        detail::NotifyControlChanged(state_);
    }
}

int Slider::GetMinimum() const noexcept {
    return state_->minimum;
}

void Slider::SetMaximum(int maximum) {
    const int nextMaximum = NormalizeMaximum(maximum, state_->minimum);
    const int nextValue = std::min(state_->value, nextMaximum);
    const bool rangeChanged = state_->maximum != nextMaximum;
    const bool valueChanged = state_->value != nextValue;
    if (!rangeChanged && !valueChanged) return;

    state_->maximum = nextMaximum;
    if (valueChanged) {
        detail::DispatchSliderChanged(state_, nextValue);
    } else {
        detail::NotifyControlChanged(state_);
    }
}

int Slider::GetMaximum() const noexcept {
    return state_->maximum;
}

void Slider::SetValue(int value) {
    detail::DispatchSliderChanged(
        state_, ClampValue(value, state_->minimum, state_->maximum));
}

int Slider::GetValue() const noexcept {
    return state_->value;
}

void Slider::SetToolTip(std::string text) {
    SetControlToolTip(state_, std::move(text));
}

const std::string& Slider::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef Slider::GetControlRef() const noexcept {
    return ControlRef{state_};
}

bool Slider::Focus() const noexcept {
    return detail::FocusControl(state_);
}

void Slider::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool Slider::IsEnabled() const noexcept {
    return state_->enabled;
}

void Slider::OnChanged(std::function<void()> callback) {
    state_->onChanged = std::move(callback);
}

CheckBox::CheckBox(std::string text, bool checked)
    : state_(nullptr) {
    ValidateUtf8(text);
    state_ = std::make_shared<detail::ControlState>(detail::ControlKind::CheckBox,
                                                    std::move(text));
    state_->checked = checked;
}

CheckBox::~CheckBox() {
    state_->onCheckedChanged = {};
}

void CheckBox::SetText(std::string text) {
    ValidateUtf8(text);
    if (state_->text == text) return;
    state_->text = std::move(text);
    detail::NotifyControlChanged(state_);
}

const std::string& CheckBox::GetText() const noexcept {
    return state_->text;
}

void CheckBox::SetToolTip(std::string text) {
    SetControlToolTip(state_, std::move(text));
}

const std::string& CheckBox::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef CheckBox::GetControlRef() const noexcept {
    return ControlRef{state_};
}

bool CheckBox::Focus() const noexcept {
    return detail::FocusControl(state_);
}

void CheckBox::SetChecked(bool checked) {
    detail::DispatchCheckedChanged(state_, checked);
}

bool CheckBox::IsChecked() const noexcept {
    return state_->checked;
}

void CheckBox::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool CheckBox::IsEnabled() const noexcept {
    return state_->enabled;
}

void CheckBox::OnCheckedChanged(std::function<void(bool)> callback) {
    state_->onCheckedChanged = std::move(callback);
}

RadioButton::RadioButton(std::string text)
    : state_(nullptr) {
    ValidateUtf8(text);
    state_ = std::make_shared<detail::ControlState>(detail::ControlKind::RadioButton,
                                                    std::move(text));
}

RadioButton::~RadioButton() {
    state_->onSelectedChanged = {};
    try {
        detail::RemoveRadioButtonFromGroup(state_, false);
    } catch (...) {
    }
}

void RadioButton::SetText(std::string text) {
    ValidateUtf8(text);
    if (state_->text == text) return;
    state_->text = std::move(text);
    detail::NotifyControlChanged(state_);
}

const std::string& RadioButton::GetText() const noexcept {
    return state_->text;
}

void RadioButton::SetToolTip(std::string text) {
    SetControlToolTip(state_, std::move(text));
}

const std::string& RadioButton::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef RadioButton::GetControlRef() const noexcept {
    return ControlRef{state_};
}

bool RadioButton::Focus() const noexcept {
    return detail::FocusControl(state_);
}

bool RadioButton::IsSelected() const noexcept {
    return state_->selected;
}

void RadioButton::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool RadioButton::IsEnabled() const noexcept {
    return state_->enabled;
}

void RadioButton::OnSelectedChanged(std::function<void(bool)> callback) {
    state_->onSelectedChanged = std::move(callback);
}

RadioGroup::RadioGroup()
    : state_(std::make_shared<detail::RadioGroupState>()) {}

RadioGroup::~RadioGroup() {
    detail::DestroyRadioGroup(state_);
}

void RadioGroup::Add(RadioButton& button) {
    detail::AddRadioButtonToGroup(state_, button.state_);
}

void RadioGroup::Remove(RadioButton& button) {
    if (!detail::RadioGroupContains(state_, button.state_)) return;
    detail::RemoveRadioButtonFromGroup(button.state_, true);
}

bool RadioGroup::Contains(const RadioButton& button) const noexcept {
    return detail::RadioGroupContains(state_, button.state_);
}

std::size_t RadioGroup::GetMemberCount() const noexcept {
    return detail::RadioGroupMemberCount(state_);
}

std::optional<std::size_t> RadioGroup::GetSelectedIndex() const noexcept {
    return detail::GetRadioGroupSelectedIndex(state_);
}

void RadioGroup::Select(RadioButton& button) {
    detail::SelectRadioButton(state_, button.state_);
}

void RadioGroup::ClearSelection() {
    detail::ClearRadioGroupSelection(state_);
}

} // namespace guidexos::appmodel
