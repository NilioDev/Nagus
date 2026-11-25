// SPDX-License-Identifier: GPL-3.0-or-later
//
// Draws the menu screens into .bmp pictures, for looking at the engine's
// work without a window:
//
//   ngs_snapshot <library folder> <output folder>
//   ngs_snapshot <library folder> <output folder> <layout.wdf>...
//
// The second form draws each layout with its background and buttons and
// outlines every element with its number, to see what a screen holds.
// The library folder is one the app or the tests imported the game into.
#include "nagus/ui/shell.hpp"

#include <cstdio>
#include <fstream>

using namespace nagus;

namespace {

void writeBmp(const std::filesystem::path& file, const Image& img) {
    std::ofstream out(file, std::ios::binary);
    const auto rowBytes = static_cast<std::uint32_t>(img.width * 3 + 3) & ~3u;
    const std::uint32_t size = 54 + rowBytes * static_cast<std::uint32_t>(img.height);
    auto u16 = [&](std::uint32_t v) { out.put(static_cast<char>(v & 255)).put(static_cast<char>(v >> 8)); };
    auto u32 = [&](std::uint32_t v) { u16(v & 0xffff); u16(v >> 16); };
    out.put('B').put('M');
    u32(size); u32(0); u32(54); u32(40);
    u32(static_cast<std::uint32_t>(img.width)); u32(static_cast<std::uint32_t>(img.height));
    u16(1); u16(24); u32(0); u32(size - 54); u32(2835); u32(2835); u32(0); u32(0);
    for (int y = img.height - 1; y >= 0; --y) {
        std::uint32_t written = 0;
        for (int x = 0; x < img.width; ++x) {
            const Pixel p = img.at(x, y);
            out.put(static_cast<char>(p & 255)).put(static_cast<char>((p >> 8) & 255)).put(static_cast<char>((p >> 16) & 255));
            written += 3;
        }
        for (; written < rowBytes; ++written) {
            out.put(0);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: ngs_snapshot <library folder> <output folder> [layout.wdf...]\n");
        return 2;
    }
    try {
        const std::filesystem::path out = argv[2];
        std::filesystem::create_directories(out);
        auto res = std::make_shared<Resources>(argv[1]);
        Assets assets(res);
        if (argc > 3) {
            const Font& font = assets.font("human9.fnt");
            for (int i = 3; i < argc; ++i) {
                Canvas canvas;
                Panel panel(assets, argv[i]);
                panel.draw(canvas);
                for (const auto& e : panel.layout().elements) {
                    canvas.frame(e.rect, rgb(255, 0, 255));
                    char label[32];
                    std::snprintf(label, sizeof label, "%x", e.id);
                    canvas.text(font, label, e.rect.x + 2, e.rect.y + 1, rgb(255, 255, 0));
                    std::printf("%s %x kind=%d %d,%d %dx%d %s %s %s\n", argv[i], e.id, static_cast<int>(e.kind),
                                e.rect.x, e.rect.y, e.rect.w, e.rect.h, e.upImage.c_str(), e.downImage.c_str(),
                                e.font.c_str());
                }
                writeBmp(out / (std::string(argv[i]) + ".bmp"), canvas.image());
            }
            return 0;
        }
        Shell shell(res, out / "settings.txt");
        Canvas canvas;
        auto shot = [&](const char* name) {
            shell.draw(canvas, 0);
            writeBmp(out / name, canvas.image());
        };
        auto press = [&](const char* layout, std::uint32_t id) {
            const Rect r = assets.layout(layout).find(id)->rect;
            const Point p{r.centerX(), r.centerY()};
            shell.pointerMoved(p);
            shell.pointerPressed(p);
            shell.pointerReleased(p);
        };
        shell.pointerMoved({700, 160});
        shot("main-menu.bmp");
        press("opening.wdf", MainMenu::kNewSinglePlayer);
        shot("game-setup.bmp");
        press("gmesetup.wdf", GameSetup::kAccept);
        press("emppick.wdf", 0x2392);
        shot("empire-select.bmp");
        shell.command(Command::Start);
        shot("not-yet.bmp");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
    return 0;
}
