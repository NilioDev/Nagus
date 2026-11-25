// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game_files.hpp"

#include "nagus/data/cd_stream.hpp"
#include "nagus/data/zip_archive.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>

namespace nagus {

namespace fs = std::filesystem;

namespace {

// The patched lexicon's CRC-32: a resource archive that already holds it was
// patched by the 1.0.2 installer.
constexpr std::uint32_t kPatchedLexiconCrc = 0x790df2feu;
constexpr std::size_t kMaxPatchFile = 4u << 20;
constexpr int kMaxFolderDepth = 4;

std::string baseName(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    return lower(slash == std::string::npos ? path : path.substr(slash + 1));
}

std::string extension(const std::string& name) {
    const auto dot = name.rfind('.');
    return dot == std::string::npos ? std::string() : lower(name.substr(dot));
}

// A file name we write into the library: a plain name, nothing that could
// leave the folder (see SECURITY.md).
bool safeName(const std::string& name) {
    if (name.empty() || name.size() > 64 || name == "." || name == "..") {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    });
}

class FileWriter {
public:
    explicit FileWriter(const fs::path& path) : path_(path), out_(path, std::ios::binary | std::ios::trunc) {
        if (!out_) {
            throw FormatError("cannot write " + path.string());
        }
    }
    void write(ByteView piece) {
        out_.write(reinterpret_cast<const char*>(piece.data()), static_cast<std::streamsize>(piece.size()));
        if (!out_) {
            throw FormatError("cannot write " + path_.string() + " (disk full?)");
        }
    }
    void close() {
        out_.close();
        if (!out_) {
            throw FormatError("cannot write " + path_.string());
        }
    }

private:
    fs::path path_;
    std::ofstream out_;
};

bool zipHasPatch(const ZipArchive& zip) { return zip.find("patch.lst") != nullptr && zip.find("lexicon.dic") != nullptr; }

std::vector<std::string> patchList(ByteView list) {
    std::vector<std::string> names;
    std::string cur;
    for (const std::uint8_t b : list) {
        if (b == '\n' || b == '\r' || b == ' ' || b == '\t') {
            if (!cur.empty()) {
                names.push_back(lower(cur));
                cur.clear();
            }
        } else {
            cur.push_back(static_cast<char>(b));
        }
    }
    if (!cur.empty()) {
        names.push_back(lower(cur));
    }
    if (names.empty() || names.size() > 64) {
        throw FormatError("the patch's file list is damaged");
    }
    for (const auto& n : names) {
        if (!safeName(n)) {
            throw FormatError("the patch's file list is damaged");
        }
    }
    return names;
}

} // namespace

Importer::Importer(fs::path library, Progress progress) : library_(std::move(library)), progress_(std::move(progress)) {
    fs::create_directories(library_);
}

void Importer::say(const std::string& label, double done) const {
    if (progress_) {
        progress_(label, done);
    }
}

bool Importer::wanted(const std::string& path) {
    const std::string lowerPath = lower(path);
    if (lowerPath.find("redist") != std::string::npos || lowerPath.find("setupdir") != std::string::npos) {
        return false;
    }
    const std::string ext = extension(lowerPath);
    return ext == ".res" || ext == ".snd" || ext == ".avi" || ext == ".wav";
}

LibraryState Importer::state(const fs::path& library) {
    LibraryState s;
    std::error_code ec;
    s.game = fs::exists(baseDir(library) / "manifest.txt", ec) && fs::exists(baseDir(library) / "stbof.res", ec);
    if (!s.game) {
        return s;
    }
    s.patch = fs::exists(patchDir(library) / "manifest.txt", ec);
    if (!s.patch) {
        try {
            const ZipArchive res(std::make_shared<FileSource>(baseDir(library) / "stbof.res"));
            const auto* lexicon = res.find("lexicon.dic");
            s.patch = lexicon != nullptr && lexicon->crc == kPatchedLexiconCrc;
        } catch (const FormatError&) {
            s.game = false;
        }
    }
    return s;
}

void Importer::commitGame() {
    const fs::path staging = library_ / "staging";
    if (!fs::exists(staging / "stbof.res")) {
        throw FormatError("no resource archive (STBOF.RES) found");
    }
    // Checks that the main archive opens before it replaces what is there.
    ZipArchive check(std::make_shared<FileSource>(staging / "stbof.res"));
    if (check.find("lexicon.dic") == nullptr) {
        throw FormatError("STBOF.RES is not the game's resource archive");
    }
    const fs::path base = baseDir(library_);
    fs::create_directories(base);
    fs::remove(base / "manifest.txt");
    std::ofstream manifest(staging / "manifest.txt");
    manifest << "nagus-base 1\ngame\n";
    for (const auto& e : fs::directory_iterator(staging)) {
        if (e.is_regular_file() && e.path().filename() != "manifest.txt") {
            manifest << e.path().filename().string() << ' ' << e.file_size() << '\n';
        }
    }
    manifest.close();
    for (const auto& e : fs::directory_iterator(staging)) {
        if (e.is_regular_file() && e.path().filename() != "manifest.txt") {
            fs::rename(e.path(), base / e.path().filename());
        }
    }
    fs::rename(staging / "manifest.txt", base / "manifest.txt");
    fs::remove_all(staging);
}

Found Importer::fromPatchFiles(const std::vector<std::pair<std::string, Bytes>>& files) {
    const fs::path dir = patchDir(library_);
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream manifest(dir / "manifest.txt.part");
    manifest << "nagus-patch 1\npatch 1.0.2\n";
    for (const auto& [name, data] : files) {
        FileWriter w(dir / name);
        w.write(data);
        w.close();
        manifest << name << ' ' << data.size() << '\n';
    }
    manifest.close();
    fs::rename(dir / "manifest.txt.part", dir / "manifest.txt");
    return Found::Patch;
}

Found Importer::fromImage(const std::string& name,
                          const std::function<void(const std::function<void(ByteView)>&)>& stream,
                          std::uint64_t size) {
    const fs::path staging = library_ / "staging";
    fs::remove_all(staging);
    fs::create_directories(staging);
    std::map<std::string, std::unique_ptr<FileWriter>> open;
    std::set<std::string> taken;
    CdStreamReader reader(
        [&](const std::string& path, std::uint32_t) {
            const std::string file = baseName(path);
            return wanted(path) && safeName(file) && taken.insert(file).second;
        },
        [&](const std::string& path, std::uint32_t total, std::uint32_t offset, ByteView piece) {
            const std::string file = baseName(path);
            auto& w = open[file];
            if (offset == 0) {
                w = std::make_unique<FileWriter>(staging / file);
            }
            if (!w) {
                throw FormatError("damaged CD image");
            }
            w->write(piece);
            if (offset + piece.size() >= total) {
                w->close();
                w.reset();
            }
        });
    std::uint64_t done = 0;
    stream([&](ByteView piece) {
        reader.feed(piece);
        const std::uint64_t before = done;
        done += piece.size();
        if (size > 0 && (before >> 23) != (done >> 23)) {
            say("Reading " + name, static_cast<double>(done) / static_cast<double>(size));
        }
    });
    reader.finish();
    commitGame();
    return Found::Game;
}

Found Importer::fromZip(const std::string& name, const std::shared_ptr<Source>& source) {
    say("Reading " + name, 0);
    const ZipArchive zip(source);
    if (zipHasPatch(zip)) {
        const auto* list = zip.find("patch.lst");
        std::vector<std::pair<std::string, Bytes>> files;
        for (const auto& file : patchList(zip.read(*list, 4096))) {
            const auto* entry = zip.find(file);
            if (entry == nullptr) {
                throw FormatError("the patch is missing " + file);
            }
            files.emplace_back(file, zip.read(*entry, kMaxPatchFile));
        }
        return fromPatchFiles(files);
    }
    // The CD's files themselves, zipped.
    bool hasRes = false;
    for (const auto& e : zip.entries()) {
        hasRes = hasRes || baseName(e.name) == "stbof.res";
    }
    if (hasRes) {
        const fs::path staging = library_ / "staging";
        fs::remove_all(staging);
        fs::create_directories(staging);
        std::set<std::string> taken;
        for (const auto& e : zip.entries()) {
            const std::string file = baseName(e.name);
            if (!wanted(e.name) || !safeName(file) || !taken.insert(file).second) {
                continue;
            }
            say("Copying " + file, 0);
            FileWriter w(staging / file);
            zip.stream(e, [&w](ByteView piece) { w.write(piece); });
            w.close();
        }
        commitGame();
        return Found::Game;
    }
    // A zipped CD image: the biggest .bin or .iso inside.
    const ZipArchive::Entry* image = nullptr;
    for (const auto& e : zip.entries()) {
        const std::string ext = extension(e.name);
        if ((ext == ".bin" || ext == ".iso" || ext == ".img") && (image == nullptr || e.size > image->size)) {
            image = &e;
        }
    }
    if (image == nullptr) {
        return Found::Nothing;
    }
    return fromImage(baseName(image->name),
                     [&](const std::function<void(ByteView)>& out) { zip.stream(*image, out); }, image->size);
}

Found Importer::importFile(const std::string& name, std::shared_ptr<Source> source) {
    if (ZipArchive::looksLikeZip(*source)) {
        // STBOF.RES on its own is a zip too.
        if (baseName(name) == "stbof.res") {
            const fs::path staging = library_ / "staging";
            fs::remove_all(staging);
            fs::create_directories(staging);
            FileWriter w(staging / "stbof.res");
            Bytes buf(1u << 20);
            for (std::uint64_t at = 0; at < source->size();) {
                const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(buf.size(), source->size() - at));
                source->read(at, std::span(buf).first(n));
                w.write(ByteView(buf).first(n));
                at += n;
            }
            w.close();
            commitGame();
            return Found::Game;
        }
        return fromZip(name, source);
    }
    const std::size_t head = static_cast<std::size_t>(std::min<std::uint64_t>(source->size(), 64));
    if (source->size() > 40000 && CdStreamReader::looksLikeImage(source->readAll(0, head))) {
        const std::uint64_t size = source->size();
        return fromImage(baseName(name),
                         [&](const std::function<void(ByteView)>& out) {
                             Bytes buf(1u << 20);
                             for (std::uint64_t at = 0; at < size;) {
                                 const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(buf.size(), size - at));
                                 source->read(at, std::span(buf).first(n));
                                 out(ByteView(buf).first(n));
                                 at += n;
                             }
                         },
                         size);
    }
    return Found::Nothing;
}

Found Importer::importFolder(const fs::path& folder) {
    // Unpacked game files (an installed game or a copy of the CD).
    std::map<std::string, fs::path> files;
    std::vector<fs::path> others;
    std::optional<fs::path> patchListFile;
    auto options = fs::directory_options::skip_permission_denied;
    for (auto it = fs::recursive_directory_iterator(folder, options); it != fs::recursive_directory_iterator(); ++it) {
        if (it.depth() >= kMaxFolderDepth) {
            it.disable_recursion_pending();
        }
        if (!it->is_regular_file()) {
            continue;
        }
        const std::string file = lower(it->path().filename().string());
        const std::string rel = fs::relative(it->path(), folder).generic_string();
        if (file == "patch.lst") {
            patchListFile = it->path();
        } else if (wanted(rel) && safeName(file)) {
            files.emplace(file, it->path());
        } else {
            const std::string ext = extension(file);
            if (ext == ".zip" || ext == ".bin" || ext == ".iso" || ext == ".img") {
                others.push_back(it->path());
            }
        }
    }
    bool game = false;
    bool patch = false;
    if (files.count("stbof.res") != 0) {
        const fs::path staging = library_ / "staging";
        fs::remove_all(staging);
        fs::create_directories(staging);
        for (const auto& [file, path] : files) {
            say("Copying " + file, 0);
            fs::copy_file(path, staging / file, fs::copy_options::overwrite_existing);
        }
        commitGame();
        game = true;
    }
    if (patchListFile) {
        const fs::path patchFolder = patchListFile->parent_path();
        std::ifstream in(*patchListFile, std::ios::binary);
        const Bytes list((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::vector<std::pair<std::string, Bytes>> patchFiles;
        for (const auto& file : patchList(list)) {
            fs::path p;
            for (const auto& e : fs::directory_iterator(patchFolder)) {
                if (lower(e.path().filename().string()) == file) {
                    p = e.path();
                }
            }
            if (p.empty() || fs::file_size(p) > kMaxPatchFile) {
                throw FormatError("the patch folder is missing " + file);
            }
            FileSource src(p);
            patchFiles.emplace_back(file, src.readAll(0, static_cast<std::size_t>(src.size())));
        }
        fromPatchFiles(patchFiles);
        patch = true;
    }
    for (const auto& path : others) {
        if (game && patch) {
            break;
        }
        std::error_code ec;
        if (game && fs::file_size(path, ec) > (64u << 20)) {
            continue; // another copy of the game: only a patch is still wanted
        }
        try {
            const Found f = importFile(path.filename().string(), std::make_shared<FileSource>(path));
            game = game || f == Found::Game || f == Found::GameAndPatch;
            patch = patch || f == Found::Patch || f == Found::GameAndPatch;
        } catch (const FormatError&) {
            // Not one of ours: other zips may lie in the same folder.
        }
    }
    if (game && patch) {
        return Found::GameAndPatch;
    }
    return game ? Found::Game : patch ? Found::Patch : Found::Nothing;
}

} // namespace nagus
