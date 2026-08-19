#include "profile_manager_model.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace guidexos::samples::profilemanager {

namespace {

const std::string kUnicodeName =
    "D" "\xC3\xA9" "veloppement " "\xF0\x9F\x9A\x80";
const std::string kUnicodeDescription =
    "Configuration locale et validation UTF-8";

bool IsContinuationByte(unsigned char value) noexcept {
    return value >= 0x80U && value <= 0xBFU;
}

void ValidateUtf8(const std::string& value) {
    for (std::size_t index = 0; index < value.size();) {
        const unsigned char first = static_cast<unsigned char>(value[index]);
        if (first <= 0x7FU) {
            ++index;
            continue;
        }

        if (first >= 0xC2U && first <= 0xDFU) {
            if (index + 1 >= value.size() ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 1]))) {
                throw ProfileDocumentFormatError("Profile document text is not valid UTF-8");
            }
            index += 2;
            continue;
        }

        if (first == 0xE0U) {
            if (index + 2 >= value.size() ||
                static_cast<unsigned char>(value[index + 1]) < 0xA0U ||
                static_cast<unsigned char>(value[index + 1]) > 0xBFU ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2]))) {
                throw ProfileDocumentFormatError("Profile document text is not valid UTF-8");
            }
            index += 3;
            continue;
        }

        if ((first >= 0xE1U && first <= 0xECU) ||
            (first >= 0xEEU && first <= 0xEFU)) {
            if (index + 2 >= value.size() ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 1])) ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2]))) {
                throw ProfileDocumentFormatError("Profile document text is not valid UTF-8");
            }
            index += 3;
            continue;
        }

        if (first == 0xEDU) {
            if (index + 2 >= value.size() ||
                static_cast<unsigned char>(value[index + 1]) < 0x80U ||
                static_cast<unsigned char>(value[index + 1]) > 0x9FU ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2]))) {
                throw ProfileDocumentFormatError("Profile document text is not valid UTF-8");
            }
            index += 3;
            continue;
        }

        if (first == 0xF0U) {
            if (index + 3 >= value.size() ||
                static_cast<unsigned char>(value[index + 1]) < 0x90U ||
                static_cast<unsigned char>(value[index + 1]) > 0xBFU ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2])) ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 3]))) {
                throw ProfileDocumentFormatError("Profile document text is not valid UTF-8");
            }
            index += 4;
            continue;
        }

        if (first >= 0xF1U && first <= 0xF3U) {
            if (index + 3 >= value.size() ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 1])) ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2])) ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 3]))) {
                throw ProfileDocumentFormatError("Profile document text is not valid UTF-8");
            }
            index += 4;
            continue;
        }

        if (first == 0xF4U) {
            if (index + 3 >= value.size() ||
                static_cast<unsigned char>(value[index + 1]) < 0x80U ||
                static_cast<unsigned char>(value[index + 1]) > 0x8FU ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2])) ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 3]))) {
                throw ProfileDocumentFormatError("Profile document text is not valid UTF-8");
            }
            index += 4;
            continue;
        }

        throw ProfileDocumentFormatError("Profile document text is not valid UTF-8");
    }
}

[[noreturn]] void FormatError(const char* message) {
    throw ProfileDocumentFormatError(message);
}

void AppendChecked(std::string& output, std::string_view value) {
    if (value.size() > kMaximumProfileDocumentBytes - output.size()) {
        FormatError("Serialized profile document exceeds the size limit");
    }
    output.append(value.data(), value.size());
}

void AppendLine(std::string& output, std::string_view value) {
    AppendChecked(output, value);
    AppendChecked(output, "\n");
}

void ValidateProfileForDocument(const Profile& profile,
                               std::unordered_set<ProfileId>* identifiers) {
    if (profile.id == 0) FormatError("Profile id must be non-zero");
    if (identifiers && !identifiers->insert(profile.id).second) {
        FormatError("Profile ids must be unique");
    }
    if (profile.values.name.size() > kMaximumProfileNameBytes) {
        FormatError("Profile name exceeds the size limit");
    }
    if (profile.values.description.size() > kMaximumProfileDescriptionBytes) {
        FormatError("Profile description exceeds the size limit");
    }
    ValidateUtf8(profile.values.name);
    ValidateUtf8(profile.values.description);
}

void AppendLengthPrefixed(std::string& output, std::string_view field,
                          const std::string& value, std::size_t maximum) {
    if (value.size() > maximum) FormatError("Profile field exceeds the size limit");
    AppendChecked(output, field);
    AppendChecked(output, " ");
    AppendChecked(output, std::to_string(value.size()));
    AppendChecked(output, "\n");
    AppendChecked(output, value);
    AppendChecked(output, "\n");
}

const char* ModeName(ProfileMode mode) {
    switch (mode) {
    case ProfileMode::Standard: return "standard";
    case ProfileMode::Advanced: return "advanced";
    case ProfileMode::Compatibility: return "compatibility";
    }
    FormatError("Profile mode is unsupported");
}

std::uint64_t ParseUnsigned(std::string_view value, const char* field) {
    if (value.empty()) FormatError(field);
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        FormatError(field);
    }
    return result;
}

std::uint64_t ParsePrefixedUnsigned(std::string_view line,
                                    std::string_view prefix,
                                    const char* field) {
    if (!line.starts_with(prefix)) FormatError(field);
    return ParseUnsigned(line.substr(prefix.size()), field);
}

class DocumentCursor final {
public:
    explicit DocumentCursor(const std::string& contents) : contents_(contents) {}

    bool AtEnd() const noexcept { return position_ == contents_.size(); }

    std::string_view NextLine(const char* field) {
        const std::size_t end = contents_.find('\n', position_);
        if (end == std::string::npos) FormatError(field);
        std::string_view line(contents_.data() + position_, end - position_);
        position_ = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        return line;
    }

    std::string ReadLengthPrefixed(std::string_view field,
                                   std::size_t maximum) {
        const std::string_view line = NextLine("Profile field length is truncated");
        const std::string prefix = std::string(field) + " ";
        const std::uint64_t length = ParsePrefixedUnsigned(line, prefix,
                                                            "Profile field length is malformed");
        if (length > maximum || length > contents_.size() - position_) {
            FormatError("Profile field is too large or truncated");
        }
        const std::size_t count = static_cast<std::size_t>(length);
        std::string value(contents_.data() + position_, count);
        position_ += count;

        if (position_ >= contents_.size()) FormatError("Profile field terminator is missing");
        if (contents_[position_] == '\n') {
            ++position_;
        } else if (contents_[position_] == '\r' &&
                   position_ + 1 < contents_.size() &&
                   contents_[position_ + 1] == '\n') {
            position_ += 2;
        } else {
            FormatError("Profile field terminator is malformed");
        }
        ValidateUtf8(value);
        return value;
    }

private:
    const std::string& contents_;
    std::size_t position_ = 0;
};

} // namespace

ProfileManagerModel::ProfileManagerModel() {
    AddProfile({"Development", "Local development configuration", true,
                ProfileMode::Standard});
    AddProfile({kUnicodeName, kUnicodeDescription, true, ProfileMode::Advanced});
    AddProfile({"Development", "A second profile with the same display name",
                false, ProfileMode::Compatibility});
}

const std::vector<Profile>& ProfileManagerModel::GetProfiles() const noexcept {
    return profiles_;
}

std::size_t ProfileManagerModel::GetProfileCount() const noexcept {
    return profiles_.size();
}

const Profile& ProfileManagerModel::GetProfile(std::size_t index) const {
    if (index >= profiles_.size()) throw std::out_of_range("profile index");
    return profiles_[index];
}

std::optional<ProfileId> ProfileManagerModel::GetSelectedId() const noexcept {
    return selectedId_;
}

std::optional<std::size_t> ProfileManagerModel::GetSelectedIndex() const noexcept {
    if (!selectedId_) return std::nullopt;
    for (std::size_t index = 0; index < profiles_.size(); ++index) {
        if (profiles_[index].id == *selectedId_) return index;
    }
    return std::nullopt;
}

const ProfileDraft& ProfileManagerModel::GetDraft() const noexcept {
    return draft_;
}

bool ProfileManagerModel::HasEditor() const noexcept {
    return creatingNew_ || selectedId_.has_value();
}

bool ProfileManagerModel::IsCreatingNew() const noexcept {
    return creatingNew_;
}

bool ProfileManagerModel::IsDirty() const noexcept {
    return dirty_;
}

void ProfileManagerModel::SelectIndex(std::size_t index) {
    if (index >= profiles_.size()) throw std::out_of_range("profile index");
    LoadStoredProfile(index);
}

void ProfileManagerModel::ClearSelection() noexcept {
    selectedId_.reset();
    creatingNew_ = false;
    draft_ = {};
    dirty_ = false;
}

void ProfileManagerModel::BeginNew() {
    selectedId_.reset();
    creatingNew_ = true;
    draft_ = {};
    dirty_ = false;
}

void ProfileManagerModel::SetDraft(ProfileDraft draft) {
    if (!HasEditor()) throw std::logic_error("profile editor is not active");
    draft_ = std::move(draft);

    if (creatingNew_) {
        dirty_ = !(draft_ == ProfileDraft{});
        return;
    }

    const auto index = GetSelectedIndex();
    if (!index) throw std::logic_error("selected profile is missing");
    dirty_ = !(draft_ == profiles_[*index].values);
}

void ProfileManagerModel::ReplaceProfiles(std::vector<Profile> profiles) {
    if (profiles.size() > kMaximumProfileCount) {
        throw ProfileDocumentFormatError("Profile count exceeds the size limit");
    }

    std::unordered_set<ProfileId> identifiers;
    ProfileId nextId = 1;
    for (const auto& profile : profiles) {
        ValidateProfileForDocument(profile, &identifiers);
        if (profile.id == std::numeric_limits<ProfileId>::max()) {
            throw ProfileDocumentFormatError("Profile id range is exhausted");
        }
        nextId = std::max(nextId, static_cast<ProfileId>(profile.id + 1));
    }

    profiles_ = std::move(profiles);
    nextId_ = nextId;
    ClearSelection();
}

ProfileId ProfileManagerModel::Save() {
    if (!HasEditor()) return 0;

    if (creatingNew_) {
        const ProfileId id = AddProfile(draft_);
        selectedId_ = id;
        creatingNew_ = false;
        dirty_ = false;
        return id;
    }

    const auto index = GetSelectedIndex();
    if (!index) throw std::logic_error("selected profile is missing");
    profiles_[*index].values = draft_;
    dirty_ = false;
    return profiles_[*index].id;
}

std::optional<std::size_t> ProfileManagerModel::DeleteSelected() {
    const auto index = GetSelectedIndex();
    if (!index || creatingNew_) return std::nullopt;

    profiles_.erase(profiles_.begin() + static_cast<std::ptrdiff_t>(*index));
    ClearSelection();
    return index;
}

ProfileId ProfileManagerModel::AddProfile(ProfileDraft values) {
    const ProfileId id = nextId_++;
    profiles_.push_back({id, std::move(values)});
    return id;
}

void ProfileManagerModel::LoadStoredProfile(std::size_t index) {
    selectedId_ = profiles_[index].id;
    creatingNew_ = false;
    draft_ = profiles_[index].values;
    dirty_ = false;
}

std::string SerializeProfileSet(const std::vector<Profile>& profiles) {
    if (profiles.size() > kMaximumProfileCount) {
        FormatError("Profile count exceeds the size limit");
    }

    std::unordered_set<ProfileId> identifiers;
    std::string output;
    AppendLine(output, "GXPROFILESET 1");
    AppendLine(output, "profiles " + std::to_string(profiles.size()));
    for (const auto& profile : profiles) {
        ValidateProfileForDocument(profile, &identifiers);
        AppendLine(output, "profile");
        AppendLine(output, "id " + std::to_string(profile.id));
        AppendLengthPrefixed(output, "name", profile.values.name,
                             kMaximumProfileNameBytes);
        AppendLengthPrefixed(output, "description", profile.values.description,
                             kMaximumProfileDescriptionBytes);
        AppendLine(output, profile.values.enabled ? "enabled true" : "enabled false");
        AppendLine(output, std::string("mode ") + ModeName(profile.values.mode));
        AppendLine(output, "end");
    }
    return output;
}

std::vector<Profile> DeserializeProfileSet(const std::string& contents) {
    if (contents.size() > kMaximumProfileDocumentBytes) {
        FormatError("Profile document exceeds the size limit");
    }
    ValidateUtf8(contents);

    DocumentCursor cursor(contents);
    const std::string_view header = cursor.NextLine("Profile document header is truncated");
    if (!header.starts_with("GXPROFILESET ")) {
        FormatError("Profile document header is malformed");
    }
    const std::uint64_t version = ParseUnsigned(
        header.substr(std::string_view("GXPROFILESET ").size()),
        "Profile document version is malformed");
    if (version != 1) FormatError("Unsupported profile document version");

    const std::string_view countLine = cursor.NextLine("Profile count is truncated");
    const std::uint64_t count = ParsePrefixedUnsigned(
        countLine, "profiles ", "Profile count is malformed");
    if (count > kMaximumProfileCount) FormatError("Profile count exceeds the size limit");

    std::vector<Profile> profiles;
    profiles.reserve(static_cast<std::size_t>(count));
    std::unordered_set<ProfileId> identifiers;
    for (std::size_t index = 0; index < static_cast<std::size_t>(count); ++index) {
        if (cursor.NextLine("Profile record is truncated") != "profile") {
            FormatError("Profile record marker is malformed");
        }
        const ProfileId id = ParsePrefixedUnsigned(
            cursor.NextLine("Profile id is truncated"), "id ",
            "Profile id is malformed");
        Profile profile;
        profile.id = id;
        profile.values.name = cursor.ReadLengthPrefixed("name", kMaximumProfileNameBytes);
        profile.values.description = cursor.ReadLengthPrefixed(
            "description", kMaximumProfileDescriptionBytes);

        const std::string_view enabled = cursor.NextLine("Profile enabled state is truncated");
        if (enabled == "enabled true") {
            profile.values.enabled = true;
        } else if (enabled == "enabled false") {
            profile.values.enabled = false;
        } else {
            FormatError("Profile enabled state is malformed");
        }

        const std::string_view mode = cursor.NextLine("Profile mode is truncated");
        if (mode == "mode standard") {
            profile.values.mode = ProfileMode::Standard;
        } else if (mode == "mode advanced") {
            profile.values.mode = ProfileMode::Advanced;
        } else if (mode == "mode compatibility") {
            profile.values.mode = ProfileMode::Compatibility;
        } else {
            FormatError("Profile mode is malformed");
        }

        if (cursor.NextLine("Profile record terminator is truncated") != "end") {
            FormatError("Profile record terminator is malformed");
        }
        ValidateProfileForDocument(profile, &identifiers);
        profiles.push_back(std::move(profile));
    }

    if (!cursor.AtEnd()) FormatError("Profile document has trailing content");
    return profiles;
}

ProfileManagerDocument::ProfileManagerDocument() = default;

const ProfileManagerModel& ProfileManagerDocument::GetModel() const noexcept {
    return model_;
}

bool ProfileManagerDocument::IsDirty() const noexcept {
    return documentDirty_ || model_.IsDirty();
}

bool ProfileManagerDocument::IsEditorDirty() const noexcept {
    return model_.IsDirty();
}

const std::optional<std::string>&
ProfileManagerDocument::GetCurrentPath() const noexcept {
    return currentPath_;
}

void ProfileManagerDocument::SelectIndex(std::size_t index) {
    model_.SelectIndex(index);
}

void ProfileManagerDocument::ClearSelection() noexcept {
    model_.ClearSelection();
}

void ProfileManagerDocument::BeginNewProfile() {
    model_.BeginNew();
}

void ProfileManagerDocument::SetDraft(ProfileDraft draft) {
    model_.SetDraft(std::move(draft));
}

ProfileId ProfileManagerDocument::SaveProfileChanges() {
    const bool creating = model_.IsCreatingNew();
    const bool editorDirty = model_.IsDirty();
    const ProfileId id = model_.Save();
    if (creating || editorDirty) documentDirty_ = true;
    return id;
}

std::optional<std::size_t> ProfileManagerDocument::DeleteSelectedProfile() {
    const auto removed = model_.DeleteSelected();
    if (removed) documentDirty_ = true;
    return removed;
}

std::string ProfileManagerDocument::Serialize() const {
    return SerializeProfileSet(model_.GetProfiles());
}

void ProfileManagerDocument::LoadSerialized(std::string contents, std::string path) {
    if (path.empty()) throw std::invalid_argument("Profile document path must not be empty");
    ValidateUtf8(path);

    // Deserialize before replacing the model so malformed input cannot
    // partially change the live document.
    std::vector<Profile> profiles = DeserializeProfileSet(contents);
    model_.ReplaceProfiles(std::move(profiles));
    if (model_.GetProfileCount() != 0) model_.SelectIndex(0);
    currentPath_ = std::move(path);
    documentDirty_ = false;
}

void ProfileManagerDocument::NewDocument() {
    model_.ReplaceProfiles({});
    currentPath_.reset();
    documentDirty_ = false;
}

void ProfileManagerDocument::MarkSaved(std::string path) {
    if (path.empty()) throw std::invalid_argument("Profile document path must not be empty");
    ValidateUtf8(path);
    currentPath_ = std::move(path);
    documentDirty_ = false;
}

} // namespace guidexos::samples::profilemanager
