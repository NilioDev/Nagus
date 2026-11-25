// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/data/resources.hpp"

#include <set>

#include "nagus/game_files.hpp"

namespace nagus {

namespace fs = std::filesystem;

Resources::Resources(const fs::path& library) : base_(Importer::baseDir(library)) {
    archive_ = std::make_unique<ZipArchive>(std::make_shared<FileSource>(base_ / "stbof.res"));
    std::error_code ec;
    if (fs::is_regular_file(base_ / "alt.res", ec)) {
        try {
            alt_ = std::make_unique<LzArchive>(std::make_shared<FileSource>(base_ / "alt.res"));
        } catch (const FormatError&) {
            // Without it only the end-of-game pictures are missing.
        }
    }
    const fs::path patch = Importer::patchDir(library);
    if (fs::exists(patch / "manifest.txt", ec)) {
        for (const auto& e : fs::directory_iterator(patch)) {
            const std::string name = lower(e.path().filename().string());
            if (!e.is_regular_file() || name == "manifest.txt" || e.file_size() > (4u << 20)) {
                continue;
            }
            FileSource src(e.path());
            overrides_.emplace(name, src.readAll(0, static_cast<std::size_t>(src.size())));
        }
    }
}

Resources::Resources(std::shared_ptr<Source> archive, std::map<std::string, Bytes> overrides)
    : archive_(std::make_unique<ZipArchive>(std::move(archive))) {
    for (auto& [name, data] : overrides) {
        overrides_.emplace(lower(name), std::move(data));
    }
}

std::vector<std::string> Resources::names(std::string_view ending) const {
    const std::string end = lower(ending);
    std::set<std::string> found;
    auto take = [&](const std::string& name) {
        const std::string n = lower(name);
        if (n.size() >= end.size() && n.compare(n.size() - end.size(), end.size(), end) == 0) {
            found.insert(n);
        }
    };
    for (const auto& e : archive_->entries()) {
        take(e.name);
    }
    for (const auto& [name, data] : overrides_) {
        take(name);
    }
    if (alt_) {
        for (const auto& e : alt_->entries()) {
            take(e.name);
        }
    }
    return {found.begin(), found.end()};
}

bool Resources::has(std::string_view name) const {
    return overrides_.count(lower(name)) != 0 || archive_->find(name) != nullptr || (alt_ && alt_->find(name) != nullptr);
}

Bytes Resources::file(std::string_view name) const {
    if (const auto it = overrides_.find(lower(name)); it != overrides_.end()) {
        return it->second;
    }
    const auto* entry = archive_->find(name);
    if (entry == nullptr && alt_) {
        if (const auto* e = alt_->find(name)) {
            return alt_->read(*e);
        }
    }
    if (entry == nullptr) {
        throw FormatError("missing game file " + std::string(name));
    }
    return archive_->read(*entry);
}

} // namespace nagus
