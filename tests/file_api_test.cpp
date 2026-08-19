#include <guidexos/appmodel/file.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace guidexos::appmodel;

namespace {

std::string Utf8Path(const std::filesystem::path& path) {
    const auto encoded = path.u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

class TemporaryDirectory final {
public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                (std::string("guideXOS_FileApi_") + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& Path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

template <typename Exception, typename Action>
void ExpectThrow(Action&& action) {
    bool threw = false;
    try {
        action();
    } catch (const Exception&) {
        threw = true;
    }
    assert(threw);
}

void TestReadWriteRoundTripAndReplacement() {
    TemporaryDirectory directory;
    const std::filesystem::path path = directory.Path() / L"配置-🚀.txt";
    const std::string encodedPath = Utf8Path(path);
    const std::string text = "ASCII, Ã©, æ¥æ¬èª, ð";

    File::WriteAllText(encodedPath, text);
    assert(File::ReadAllText(encodedPath) == text);
    File::WriteAllText(encodedPath, "replacement");
    assert(File::ReadAllText(encodedPath) == "replacement");
}

void TestEmptyAndBom() {
    TemporaryDirectory directory;
    const std::string emptyPath = Utf8Path(directory.Path() / L"empty.txt");
    File::WriteAllText(emptyPath, {});
    assert(File::ReadAllText(emptyPath).empty());

    const std::string bomPath = Utf8Path(directory.Path() / L"bom.txt");
    {
        std::ofstream output(std::filesystem::path(bomPath), std::ios::binary);
        output.write("\xEF\xBB\xBFhello", 8);
    }
    assert(File::ReadAllText(bomPath) == "hello");
    File::WriteAllText(bomPath, "\xEF\xBB\xBFworld");
    std::ifstream input(std::filesystem::path(bomPath), std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
    assert(bytes == "world");
}

void TestFailuresAndTemporaryCleanup() {
    TemporaryDirectory directory;
    const std::string missing = Utf8Path(directory.Path() / L"missing.txt");
    ExpectThrow<FileNotFoundError>([&] { (void)File::ReadAllText(missing); });

    const std::string invalidPath = Utf8Path(directory.Path() / L"invalid.txt");
    {
        std::ofstream output(std::filesystem::path(invalidPath), std::ios::binary);
        output.write("bad\x80", 4);
    }
    ExpectThrow<InvalidUtf8Error>([&] { (void)File::ReadAllText(invalidPath); });
    ExpectThrow<InvalidUtf8Error>([&] {
        File::WriteAllText(invalidPath, std::string("bad\x80", 4));
    });

    const std::string oversized = Utf8Path(directory.Path() / L"oversized.txt");
    {
        std::ofstream output(std::filesystem::path(oversized), std::ios::binary);
        std::string chunk(1024U * 1024U, 'x');
        for (int index = 0; index != 17; ++index) output.write(chunk.data(), chunk.size());
    }
    ExpectThrow<FileTooLargeError>([&] { (void)File::ReadAllText(oversized); });

    const std::filesystem::path blockedPath = directory.Path() / L"blocked.txt";
    std::filesystem::create_directory(blockedPath);
    ExpectThrow<FileReplacementError>([&] {
        File::WriteAllText(Utf8Path(blockedPath), "replacement failure");
    });
    for (const auto& entry : std::filesystem::directory_iterator(directory.Path())) {
        if (entry.path().filename().wstring().find(L".guidexos-tmp-") !=
            std::wstring::npos) {
            throw std::runtime_error("temporary file was not cleaned up");
        }
    }
}

} // namespace

int main() {
    TestReadWriteRoundTripAndReplacement();
    TestEmptyAndBom();
    TestFailuresAndTemporaryCleanup();
    return 0;
}
