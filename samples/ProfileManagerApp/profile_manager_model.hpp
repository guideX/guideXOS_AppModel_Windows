#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace guidexos::samples::profilemanager {

using ProfileId = std::uint64_t;

enum class ProfileMode {
    Standard,
    Advanced,
    Compatibility,
};

struct ProfileDraft final {
    std::string name;
    std::string description;
    bool enabled = true;
    ProfileMode mode = ProfileMode::Standard;

    bool operator==(const ProfileDraft&) const = default;
};

struct Profile final {
    ProfileId id = 0;
    ProfileDraft values;

    bool operator==(const Profile&) const = default;
};

inline constexpr std::size_t kMaximumProfileCount = 4096;
inline constexpr std::size_t kMaximumProfileNameBytes = 64U * 1024U;
inline constexpr std::size_t kMaximumProfileDescriptionBytes = 1024U * 1024U;
inline constexpr std::size_t kMaximumProfileDocumentBytes = 16U * 1024U * 1024U;

class ProfileDocumentFormatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ProfileManagerModel final {
public:
    ProfileManagerModel();

    const std::vector<Profile>& GetProfiles() const noexcept;
    std::size_t GetProfileCount() const noexcept;
    const Profile& GetProfile(std::size_t index) const;

    std::optional<ProfileId> GetSelectedId() const noexcept;
    std::optional<std::size_t> GetSelectedIndex() const noexcept;
    const ProfileDraft& GetDraft() const noexcept;

    bool HasEditor() const noexcept;
    bool IsCreatingNew() const noexcept;
    bool IsDirty() const noexcept;

    // Selecting another profile intentionally discards pending edits. This
    // deterministic policy keeps profile switching available without a
    // dialog system.
    void SelectIndex(std::size_t index);
    void ClearSelection() noexcept;
    void BeginNew();
    void SetDraft(ProfileDraft draft);

    // Replaces the collection after a complete document has been parsed.
    // Selection and pending editor state are reset, and stored identities are
    // retained so duplicate display names remain unambiguous.
    void ReplaceProfiles(std::vector<Profile> profiles);

    // Returns the saved profile identity, or nullopt when there is no editor.
    ProfileId Save();

    // Removes exactly the selected stored profile and returns its old index.
    // New-profile editing has no deletable stored profile.
    std::optional<std::size_t> DeleteSelected();

private:
    ProfileId AddProfile(ProfileDraft values);
    void LoadStoredProfile(std::size_t index);

    std::vector<Profile> profiles_;
    ProfileId nextId_ = 1;
    std::optional<ProfileId> selectedId_;
    bool creatingNew_ = false;
    ProfileDraft draft_;
    bool dirty_ = false;
};

std::string SerializeProfileSet(const std::vector<Profile>& profiles);
std::vector<Profile> DeserializeProfileSet(const std::string& contents);

class ProfileManagerDocument final {
public:
    ProfileManagerDocument();

    const ProfileManagerModel& GetModel() const noexcept;
    bool IsDirty() const noexcept;
    bool IsEditorDirty() const noexcept;
    const std::optional<std::string>& GetCurrentPath() const noexcept;

    void SelectIndex(std::size_t index);
    void ClearSelection() noexcept;
    void BeginNewProfile();
    void SetDraft(ProfileDraft draft);
    ProfileId SaveProfileChanges();
    std::optional<std::size_t> DeleteSelectedProfile();

    std::string Serialize() const;
    // Parsing happens before any member state changes. A format error leaves
    // the current collection, selection, path, and dirty state untouched.
    void LoadSerialized(std::string contents, std::string path);
    void NewDocument();
    void MarkSaved(std::string path);

private:
    ProfileManagerModel model_;
    std::optional<std::string> currentPath_;
    bool documentDirty_ = false;
};

} // namespace guidexos::samples::profilemanager
