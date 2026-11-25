// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/ui/assets.hpp"

namespace nagus {

Assets::Assets(std::shared_ptr<Resources> resources) : resources_(std::move(resources)) {
    lexicon_ = Lexicon(resources_->file("lexicon.dic"));
}

const Tips& Assets::tips() {
    if (!tips_) {
        tips_.emplace();
        if (resources_->has("teachtip.tdb")) {
            try {
                tips_.emplace(resources_->file("teachtip.tdb"));
            } catch (const FormatError&) {
                tips_.emplace();
            }
        }
    }
    return *tips_;
}

void Assets::hover(const void* owner, std::optional<std::uint32_t> button) {
    if (button) {
        hovered_ = button;
        hoverOwner_ = owner;
    } else {
        unhover(owner);
    }
}

const Image& Assets::image(const std::string& name) {
    const std::string key = lower(name);
    auto it = images_.find(key);
    if (it == images_.end()) {
        Image img;
        if (!key.empty() && resources_->has(key)) {
            try {
                const bool gif = key.size() > 4 && key.compare(key.size() - 4, 4, ".gif") == 0;
                img = gif ? readGif(resources_->file(key)) : readTga(resources_->file(key));
            } catch (const FormatError&) {
                img = Image();
            }
        }
        it = images_.emplace(key, std::move(img)).first;
    }
    return it->second;
}

const Model* Assets::model(const std::string& name) {
    const std::string key = lower(name);
    auto it = models_.find(key);
    if (it == models_.end()) {
        std::optional<Model> m;
        if (resources_->has(key)) {
            try {
                m = readModel(resources_->file(key));
            } catch (const FormatError&) {
                m.reset();
            }
        }
        if (m && m->empty()) {
            m.reset();
        }
        it = models_.emplace(key, std::move(m)).first;
    }
    return it->second ? &*it->second : nullptr;
}

const Font& Assets::font(const std::string& name) {
    const std::string key = lower(name);
    auto it = fonts_.find(key);
    if (it == fonts_.end()) {
        it = fonts_.emplace(key, std::make_unique<Font>(resources_->file(key))).first;
    }
    return *it->second;
}

const Layout& Assets::layout(const std::string& name) {
    const std::string key = lower(name);
    auto it = layouts_.find(key);
    if (it == layouts_.end()) {
        it = layouts_.emplace(key, readLayout(resources_->file(key))).first;
    }
    return it->second;
}

const Cursor& Assets::cursor(const std::string& name) {
    const std::string key = lower(name);
    auto it = cursors_.find(key);
    if (it == cursors_.end()) {
        it = cursors_.emplace(key, readCursor(resources_->file(key))).first;
    }
    return it->second;
}

const Animation& Assets::animation(const std::string& name) {
    const std::string key = lower(name);
    auto it = animations_.find(key);
    if (it == animations_.end()) {
        Animation a;
        if (!key.empty() && resources_->has(key)) {
            try {
                a = readAnimation(resources_->file(key));
            } catch (const FormatError&) {
                a = Animation();
            }
        }
        it = animations_.emplace(key, std::move(a)).first;
    }
    return it->second;
}

const std::vector<std::string>& Assets::texts(const std::string& name) {
    const std::string key = lower(name);
    auto it = texts_.find(key);
    if (it == texts_.end()) {
        const Bytes data = resources_->file(key);
        std::vector<std::string> list;
        std::string cur;
        for (const std::uint8_t b : data) {
            if (b == 0) {
                list.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(static_cast<char>(b));
            }
        }
        if (!cur.empty()) {
            list.push_back(cur);
        }
        it = texts_.emplace(key, std::move(list)).first;
    }
    return it->second;
}

} // namespace nagus
