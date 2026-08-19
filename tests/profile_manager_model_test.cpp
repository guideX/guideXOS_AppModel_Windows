#include "../samples/ProfileManagerApp/profile_manager_model.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using namespace guidexos::samples::profilemanager;

namespace {

const std::string kUnicodeName =
    "D" "\xC3\xA9" "veloppement " "\xF0\x9F\x9A\x80";

void TestInitialPopulationAndIdentity() {
    ProfileManagerModel model;

    assert(model.GetProfileCount() == 3);
    assert(model.GetProfile(0).values.name == "Development");
    assert(model.GetProfile(1).values.name == kUnicodeName);
    assert(model.GetProfile(2).values.name == "Development");
    assert(model.GetProfile(0).id != model.GetProfile(2).id);
    assert(model.GetSelectedIndex() == std::nullopt);
    assert(!model.HasEditor());
}

void TestSelectionAndDiscardedPendingEdits() {
    ProfileManagerModel model;
    model.SelectIndex(0);
    assert(model.GetDraft().description == "Local development configuration");
    assert(!model.IsDirty());

    auto draft = model.GetDraft();
    draft.name = "Development (edited)";
    draft.enabled = false;
    model.SetDraft(draft);
    assert(model.IsDirty());

    model.SelectIndex(1);
    assert(model.GetSelectedIndex() == 1);
    assert(model.GetDraft().name == kUnicodeName);
    assert(model.GetDraft().enabled);
    assert(!model.IsDirty());
}

void TestNewSaveUpdateAndUnicode() {
    ProfileManagerModel model;
    model.BeginNew();
    assert(model.IsCreatingNew());
    assert(model.HasEditor());
    assert(!model.IsDirty());

    ProfileDraft draft{"Nouveau \xE2\x9C\xA8", "Profil \xE6\x96\xB0", false,
                       ProfileMode::Compatibility};
    model.SetDraft(draft);
    assert(model.IsDirty());
    const ProfileId id = model.Save();
    (void)id;
    assert(id != 0);
    assert(model.GetProfileCount() == 4);
    assert(model.GetSelectedId() == id);
    assert(model.GetSelectedIndex() == 3);
    assert(model.GetProfile(3).values == draft);
    assert(!model.IsCreatingNew());
    assert(!model.IsDirty());

    draft.description = "Updated description";
    draft.mode = ProfileMode::Advanced;
    model.SetDraft(draft);
    assert(model.IsDirty());
    assert(model.Save() == id);
    assert(model.GetProfile(3).values == draft);
    assert(!model.IsDirty());
}

void TestDeleteAndAdjacentIndex() {
    ProfileManagerModel model;
    model.SelectIndex(1);
    const ProfileId deletedId = model.GetSelectedId().value();
    (void)deletedId;
    const auto removed = model.DeleteSelected();
    assert(removed == 1);
    assert(model.GetProfileCount() == 2);
    assert(model.GetProfile(0).id != deletedId);
    assert(model.GetProfile(1).id != deletedId);
    assert(!model.GetSelectedIndex());
    assert(!model.HasEditor());

    model.SelectIndex(1);
    assert(model.GetProfile(1).values.name == "Development");
    model.BeginNew();
    assert(!model.DeleteSelected());
    assert(model.GetProfileCount() == 2);
    model.ClearSelection();
    assert(!model.DeleteSelected());
}

template <typename Action>
void ExpectFormatError(Action&& action) {
    bool threw = false;
    try {
        action();
    } catch (const ProfileDocumentFormatError&) {
        threw = true;
    }
    assert(threw);
}

void TestSerializationRoundTrip() {
    const std::vector<Profile> profiles = {
        {41, {"", "line one\nline two = [ ]", true, ProfileMode::Standard}},
        {42, {"同じ 🚀", "Descripción", false, ProfileMode::Advanced}},
        {43, {"同じ 🚀", "compatibility", true, ProfileMode::Compatibility}},
    };
    const std::string serialized = SerializeProfileSet(profiles);
    assert(serialized.starts_with("GXPROFILESET 1\nprofiles 3\n"));

    const auto parsed = DeserializeProfileSet(serialized);
    assert(parsed == profiles);
    assert(SerializeProfileSet(parsed) == serialized);

    const std::string empty = SerializeProfileSet({});
    assert(empty == "GXPROFILESET 1\nprofiles 0\n");
    assert(DeserializeProfileSet(empty).empty());
}

void TestSerializationRejections() {
    ExpectFormatError([] {
        (void)DeserializeProfileSet("GXPROFILESET 2\nprofiles 0\n");
    });
    ExpectFormatError([] {
        (void)DeserializeProfileSet("GXPROFILESET 1\nprofiles 1\nprofile\n");
    });
    ExpectFormatError([] {
        (void)DeserializeProfileSet("GXPROFILESET 1\nprofiles 4097\n");
    });
    ExpectFormatError([] {
        (void)DeserializeProfileSet(
            "GXPROFILESET 1\nprofiles 1\nprofile\nid 1\nname 65537\n");
    });
    ExpectFormatError([] {
        (void)DeserializeProfileSet(
            "GXPROFILESET 1\nprofiles 1\nprofile\nid 1\nname 3\nabc\n"
            "description 4\nnope\n enabled true\nmode standard\nend\n");
    });
    ExpectFormatError([] {
        (void)DeserializeProfileSet(
            "GXPROFILESET 1\nprofiles 1\nprofile\nid 1\nname 3\nabc\n"
            "description 3\nabc");
    });
}

void TestDocumentDirtyPathAndFailureIsolation() {
    ProfileManagerDocument document;
    assert(!document.IsDirty());
    assert(!document.GetCurrentPath());

    document.SelectIndex(0);
    auto draft = document.GetModel().GetDraft();
    draft.description = "changed in memory";
    document.SetDraft(draft);
    assert(document.IsEditorDirty());
    assert(document.IsDirty());

    document.SaveProfileChanges();
    assert(document.IsDirty());
    assert(!document.IsEditorDirty());
    document.MarkSaved("C:/profiles.gxprofiles");
    assert(!document.IsDirty());
    assert(document.GetCurrentPath().value() == "C:/profiles.gxprofiles");

    const std::string serialized = document.Serialize();
    const std::string previousName = document.GetModel().GetProfile(0).values.name;
    const std::string previousPath = document.GetCurrentPath().value();
    ExpectFormatError([&] {
        document.LoadSerialized("GXPROFILESET 99\nprofiles 0\n", "bad.gxprofiles");
    });
    assert(document.GetModel().GetProfile(0).values.name == previousName);
    assert(document.GetCurrentPath().value() == previousPath);
    assert(!document.IsDirty());

    document.LoadSerialized(serialized, "loaded.gxprofiles");
    assert(document.GetModel().GetProfileCount() == 3);
    assert(document.GetModel().GetSelectedIndex() == 0);
    assert(!document.IsDirty());

    document.DeleteSelectedProfile();
    assert(document.IsDirty());
    document.NewDocument();
    assert(document.GetModel().GetProfileCount() == 0);
    assert(!document.GetModel().HasEditor());
    assert(!document.GetCurrentPath());
    assert(!document.IsDirty());
}

} // namespace

int main() {
    TestInitialPopulationAndIdentity();
    TestSelectionAndDiscardedPendingEdits();
    TestNewSaveUpdateAndUnicode();
    TestDeleteAndAdjacentIndex();
    TestSerializationRoundTrip();
    TestSerializationRejections();
    TestDocumentDirtyPathAndFailureIsolation();
    return 0;
}
