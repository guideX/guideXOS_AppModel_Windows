#include <guidexos/appmodel/appmodel.hpp>

#include "appmodel/runtime.hpp"

#include <cassert>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace guidexos::appmodel::detail {

struct WindowStateTestAccess {
    static std::shared_ptr<WindowState> Get(const Window& window) noexcept {
        return window.state_;
    }
};

} // namespace guidexos::appmodel::detail

using namespace guidexos::appmodel;

namespace {

void MarkOpen(const std::shared_ptr<detail::WindowState>& state) {
    state->shown = true;
    state->closeState = detail::WindowCloseState::Open;
}

void TestInstallReplaceClearAndPayload() {
    Application application("com.guidexos.tests.file-drop-model",
                             ShutdownMode::Explicit);
    Window window(application);
    const auto state = detail::WindowStateTestAccess::Get(window);
    MarkOpen(state);

    std::vector<std::vector<std::string>> received;
    window.OnFilesDropped([&](const FileDropEvent& event) {
        received.push_back(event.GetFiles());
    });
    const std::string unicodePath =
        "C:\\drop\\" "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"
        " " "\xF0\x9F\x9A\x80.txt";
    detail::DispatchFilesDropped(state,
                                 {"C:\\drop folder\\alpha.txt",
                                  unicodePath,
                                  "C:\\drop folder\\alpha.txt"});
    assert(received.size() == 1);
    assert(received[0].size() == 3);
    assert(received[0][0] == "C:\\drop folder\\alpha.txt");
    assert(received[0][1] == unicodePath);
    assert(received[0][2] == received[0][0]);

    bool replacementCalled = false;
    window.OnFilesDropped([&](const FileDropEvent& event) {
        if (event.GetFiles().size() != 1) std::abort();
        replacementCalled = true;
        window.OnFilesDropped([&](const FileDropEvent& next) {
            if (next.GetFiles().size() != 1) std::abort();
            replacementCalled = true;
        });
    });
    detail::DispatchFilesDropped(state, {"first"});
    assert(replacementCalled);
    detail::DispatchFilesDropped(state, {"second"});
    assert(received.size() == 1);

    window.OnFilesDropped({});
    detail::DispatchFilesDropped(state, {"cleared"});
    assert(received.size() == 1);
}

void TestBoundsAndLifecycle() {
    Application application("com.guidexos.tests.file-drop-bounds",
                             ShutdownMode::Explicit);
    Window window(application);
    Window other(application);
    const auto state = detail::WindowStateTestAccess::Get(window);
    const auto otherState = detail::WindowStateTestAccess::Get(other);
    MarkOpen(state);
    MarkOpen(otherState);

    std::size_t callbackCount = 0;
    std::vector<std::string> lastPaths;
    window.OnFilesDropped([&](const FileDropEvent& event) {
        ++callbackCount;
        lastPaths = event.GetFiles();
    });

    std::vector<std::string> many;
    for (std::size_t index = 0; index != kMaximumFileDropCount + 10U; ++index) {
        many.push_back("C:\\drop\\" + std::to_string(index));
    }
    detail::DispatchFilesDropped(state, std::move(many));
    assert(callbackCount == 1);
    assert(lastPaths.size() == kMaximumFileDropCount);
    assert(lastPaths.front() == "C:\\drop\\0");
    assert(lastPaths.back() == "C:\\drop\\1023");

    std::vector<std::string> oversized;
    for (int index = 0; index != 20; ++index) {
        oversized.emplace_back(kMaximumFileDropPathBytes, 'x');
    }
    detail::DispatchFilesDropped(state, std::move(oversized));
    assert(callbackCount == 2);
    assert(lastPaths.size() == kMaximumFileDropPayloadBytes /
                                  kMaximumFileDropPathBytes);

    bool otherCalled = false;
    other.OnFilesDropped([&](const FileDropEvent&) { otherCalled = true; });
    window.OnFilesDropped([&](const FileDropEvent&) {
        window.Close();
        other.Close();
    });
    detail::DispatchFilesDropped(state, {"close-own-and-other"});
    assert(!state->shown);
    assert(!otherState->shown);
    detail::DispatchFilesDropped(state, {"stale"});
    assert(callbackCount == 2);
    detail::DispatchFilesDropped(otherState, {"stale-other"});
    assert(!otherCalled);

    // The logical callback remains on the Window object and works after the
    // next realization, just as the other Window callbacks do.
    MarkOpen(state);
    bool reopened = false;
    window.OnFilesDropped([&](const FileDropEvent& event) {
        reopened = event.GetFiles() == std::vector<std::string>{"reopened"};
    });
    detail::DispatchFilesDropped(state, {"reopened"});
    assert(reopened);
}

} // namespace

int main() {
    TestInstallReplaceClearAndPayload();
    TestBoundsAndLifecycle();
    return 0;
}
