// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Nagus app: opens a window, imports the player's game files on first
// start, turns mouse, touch, keyboard and gamepad input into commands for
// the screens, and shows the 800×600 picture the core draws, scaled sharply
// to the window.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "nagus/game_files.hpp"
#include "nagus/ui/shell.hpp"
#include "nagus/version.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;
using nagus::Command;
using nagus::Point;

constexpr int kW = nagus::kScreenWidth;
constexpr int kH = nagus::kScreenHeight;
constexpr float kStickDeadZone = 0.2f;
constexpr float kStickSpeed = 900.0f; // pointer pixels per second at full tilt
constexpr float kStickCommand = 0.6f;

// Reads a file through SDL, which also opens Android's content:// links
// that the file picker hands out.
class SdlSource : public nagus::Source {
public:
    explicit SdlSource(const std::string& path) : io_(SDL_IOFromFile(path.c_str(), "rb")) {
        if (io_ == nullptr) {
            throw nagus::FormatError("cannot open " + path + ": " + SDL_GetError());
        }
        const Sint64 size = SDL_GetIOSize(io_);
        if (size < 0) {
            SDL_CloseIO(io_);
            throw nagus::FormatError("cannot read " + path);
        }
        size_ = static_cast<std::uint64_t>(size);
    }
    ~SdlSource() override { SDL_CloseIO(io_); }
    SdlSource(const SdlSource&) = delete;
    SdlSource& operator=(const SdlSource&) = delete;

    std::uint64_t size() const override { return size_; }
    void read(std::uint64_t offset, std::span<std::uint8_t> out) override {
        check(offset, out.size());
        if (SDL_SeekIO(io_, static_cast<Sint64>(offset), SDL_IO_SEEK_SET) < 0) {
            throw nagus::FormatError("cannot read file");
        }
        std::size_t done = 0;
        while (done < out.size()) {
            const std::size_t n = SDL_ReadIO(io_, out.data() + done, out.size() - done);
            if (n == 0) {
                throw nagus::FormatError("cannot read file");
            }
            done += n;
        }
    }

private:
    SDL_IOStream* io_;
    std::uint64_t size_ = 0;
};

// The first-start screen: what the player still has to give us, and the
// import running in the background.
struct ImportState {
    std::mutex mutex;
    std::vector<std::string> queue; // files and folders to import
    std::string status;             // what is happening now
    std::string error;              // why the last try failed
    double progress = -1;
    std::atomic<bool> busy{false};
    std::atomic<bool> changed{false};
};

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* frame = nullptr;
    SDL_AudioStream* audio = nullptr; // null when there is no sound device
    fs::path prefDir;
    fs::path library;
    std::unique_ptr<nagus::Shell> shell;
    nagus::Canvas canvas;
    ImportState import;
    std::thread worker;
    bool patchSkipped = false;
    SDL_Gamepad* pad = nullptr;
    float stickX = 0;
    float stickY = 0;
    float pointerX = kW / 2.0f;
    float pointerY = kH / 2.0f;
    bool padHeld = false; // A is down: a click in progress
    int stickDirection = -1;
    Uint64 lastTicks = 0;
};

fs::path pathFromUtf8(const char* s) {
    const std::string_view v(s);
    return fs::path(std::u8string(v.begin(), v.end()));
}

nagus::LibraryState libraryState(const App& app) { return nagus::Importer::state(app.library); }

bool readyToPlay(const App& app) {
    const auto s = libraryState(app);
    return s.game && (s.patch || app.patchSkipped);
}

void startShell(App& app) {
    try {
        auto res = std::make_shared<nagus::Resources>(app.library);
        app.shell = std::make_unique<nagus::Shell>(res, app.prefDir / "settings.txt");
        app.shell->enableFilms();
        SDL_HideCursor();
    } catch (const std::exception& e) {
        std::lock_guard lock(app.import.mutex);
        app.import.error = std::string("The imported game files do not open: ") + e.what() +
                           ". Import them again.";
        app.shell.reset();
    }
}

// Imports everything queued, on a background thread.
void runImports(App* app) {
    for (;;) {
        std::string next;
        {
            std::lock_guard lock(app->import.mutex);
            if (app->import.queue.empty()) {
                app->import.busy = false;
                app->import.changed = true;
                return;
            }
            next = app->import.queue.front();
            app->import.queue.erase(app->import.queue.begin());
            app->import.status = "Importing " + next;
            app->import.progress = -1;
        }
        try {
            nagus::Importer importer(app->library, [app](const std::string& label, double done) {
                std::lock_guard lock(app->import.mutex);
                app->import.status = label;
                app->import.progress = done;
            });
            nagus::Found found = nagus::Found::Nothing;
            std::error_code ec;
            const fs::path p = pathFromUtf8(next.c_str());
            if (next.rfind("content://", 0) != 0 && fs::is_directory(p, ec)) {
                found = importer.importFolder(p);
            } else {
                const std::string name = p.filename().string();
                found = importer.importFile(name, std::make_shared<SdlSource>(next));
            }
            std::lock_guard lock(app->import.mutex);
            app->import.error = found == nagus::Found::Nothing
                                    ? "That is not the game's CD image, game folder or 1.0.2 patch: " + next
                                    : std::string();
        } catch (const std::exception& e) {
            std::lock_guard lock(app->import.mutex);
            app->import.error = std::string("Import failed: ") + e.what();
        }
    }
}

void queueImport(App& app, const std::string& path) {
    {
        std::lock_guard lock(app.import.mutex);
        app.import.queue.push_back(path);
        app.import.error.clear();
    }
    if (!app.import.busy.exchange(true)) {
        if (app.worker.joinable()) {
            app.worker.join();
        }
        app.worker = std::thread(runImports, &app);
    }
}

void SDLCALL onFilesChosen(void* userdata, const char* const* files, int) {
    App& app = *static_cast<App*>(userdata);
    if (files == nullptr) {
        return;
    }
    for (; *files != nullptr; ++files) {
        queueImport(app, *files);
    }
}

void chooseFiles(App& app) {
    static const SDL_DialogFileFilter filters[] = {
        {"Game CD image, its zip, or the patch", "zip;bin;iso;img;res"},
        {"All files", "*"},
    };
    SDL_ShowOpenFileDialog(onFilesChosen, &app, app.window, filters, 2, nullptr, true);
}

void chooseFolder(App& app) { SDL_ShowOpenFolderDialog(onFilesChosen, &app, app.window, nullptr, false); }

// Looks for game files the player put next to the program (or, when run
// from the source tree, in its gamedata folder).
void importFromGamedata(App& app) {
    std::vector<fs::path> places;
    if (const char* base = SDL_GetBasePath()) {
        places.push_back(pathFromUtf8(base) / "gamedata");
        places.push_back(pathFromUtf8(base) / ".." / "gamedata");
        places.push_back(pathFromUtf8(base) / ".." / ".." / "gamedata");
    }
    std::error_code ec;
    places.push_back(fs::current_path(ec) / "gamedata");
    for (const auto& p : places) {
        if (fs::is_directory(p, ec) && !fs::is_empty(p, ec)) {
            queueImport(app, p.string());
            return;
        }
    }
}

void drawImportScreen(App& app) {
    SDL_SetRenderDrawColor(app.renderer, 8, 10, 24, 255);
    SDL_RenderClear(app.renderer);
    std::vector<std::string> lines;
    const auto state = libraryState(app);
    lines.push_back(std::string("Nagus ") + std::string(nagus::kVersion));
    lines.emplace_back("");
    {
        std::lock_guard lock(app.import.mutex);
        if (app.import.busy) {
            lines.push_back(app.import.status.substr(0, 90));
            if (app.import.progress >= 0) {
                lines.push_back(std::to_string(static_cast<int>(app.import.progress * 100)) + "%");
            }
            lines.emplace_back("");
            lines.emplace_back("This takes a moment the first time.");
        } else if (!state.game) {
            lines.emplace_back("Nagus plays the 1999 space strategy game with your own copy.");
            lines.emplace_back("Give it your game: the CD image (.bin/.cue or .iso), the zip it came in,");
            lines.emplace_back("a copy of the CD, or the installed game folder.");
            lines.emplace_back("");
            lines.emplace_back("  Drop the file or folder on this window, or");
            lines.emplace_back("  click / tap / press Enter or (A) to choose files,");
            lines.emplace_back("  press F to choose a folder.");
        } else {
            lines.emplace_back("Your game is imported.");
            lines.emplace_back("");
            lines.emplace_back("Now add the official 1.0.2 patch (botf102e.zip) the same way:");
            lines.emplace_back("  drop it here, or click / tap / press Enter or (A) to choose it.");
            lines.emplace_back("");
            lines.emplace_back("  Press S or (B) to play without the patch.");
        }
        if (!app.import.error.empty()) {
            lines.emplace_back("");
            const std::string& e = app.import.error;
            for (std::size_t i = 0; i < e.size(); i += 90) {
                lines.push_back(e.substr(i, 90));
            }
        }
    }
    float y = 60;
    for (const auto& l : lines) {
        SDL_SetRenderDrawColor(app.renderer, 255, 204, 102, 255);
        SDL_RenderDebugText(app.renderer, 40, y, l.c_str());
        y += 16;
    }
}

// Keeps about a sixth of a second of sound queued for the device.
void feedAudio(App& app) {
    if (app.audio == nullptr || !app.shell) {
        return;
    }
    constexpr int kFrames = 1024;
    constexpr int kQueued = nagus::Jukebox::kRate / 6 * 4;
    std::vector<std::int16_t> buffer(kFrames * 2);
    for (int i = 0; i < 16 && SDL_GetAudioStreamQueued(app.audio) < kQueued; ++i) {
        app.shell->jukebox().mix(buffer);
        SDL_PutAudioStreamData(app.audio, buffer.data(), static_cast<int>(buffer.size() * sizeof(std::int16_t)));
    }
}

void present(App& app) {
    if (app.shell) {
        app.shell->draw(app.canvas, SDL_GetTicks());
        feedAudio(app);
        SDL_UpdateTexture(app.frame, nullptr, app.canvas.image().pixels.data(), kW * 4);
        SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
        SDL_RenderClear(app.renderer);
        SDL_RenderTexture(app.renderer, app.frame, nullptr, nullptr);
    } else {
        drawImportScreen(app);
    }
    SDL_RenderPresent(app.renderer);
}

Point toScreen(App& app, float wx, float wy) {
    float x = wx;
    float y = wy;
    SDL_RenderCoordinatesFromWindow(app.renderer, wx, wy, &x, &y);
    return {static_cast<int>(std::lround(std::clamp(x, 0.0f, kW - 1.0f))),
            static_cast<int>(std::lround(std::clamp(y, 0.0f, kH - 1.0f)))};
}

void movePointer(App& app, Point p) {
    app.pointerX = static_cast<float>(p.x);
    app.pointerY = static_cast<float>(p.y);
    app.shell->pointerMoved(p);
}

void command(App& app, Command c) {
    app.shell->command(c);
    const Point p = app.shell->pointer();
    app.pointerX = static_cast<float>(p.x);
    app.pointerY = static_cast<float>(p.y);
}

void toggleFullscreen(App& app) {
    const bool full = (SDL_GetWindowFlags(app.window) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(app.window, !full);
}

SDL_AppResult importScreenEvent(App& app, SDL_Event* event) {
    const bool haveGame = libraryState(app).game;
    if (app.import.busy) {
        return SDL_APP_CONTINUE;
    }
    switch (event->type) {
    case SDL_EVENT_DROP_FILE:
        if (event->drop.data != nullptr) {
            queueImport(app, event->drop.data);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        chooseFiles(app);
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_RETURN || event->key.key == SDLK_SPACE || event->key.key == SDLK_O) {
            chooseFiles(app);
        } else if (event->key.key == SDLK_F) {
            chooseFolder(app);
        } else if (event->key.key == SDLK_S && haveGame) {
            app.patchSkipped = true;
        } else if (event->key.key == SDLK_ESCAPE || event->key.key == SDLK_AC_BACK) {
            if (haveGame) {
                app.patchSkipped = true;
            } else {
                return SDL_APP_SUCCESS;
            }
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
            chooseFiles(app);
        } else if (event->gbutton.button == SDL_GAMEPAD_BUTTON_EAST && haveGame) {
            app.patchSkipped = true;
        }
        break;
    default:
        break;
    }
    if (readyToPlay(app)) {
        startShell(app);
    }
    return SDL_APP_CONTINUE;
}

std::optional<Command> padCommand(Uint8 button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return Command::Up;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return Command::Down;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return Command::Left;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return Command::Right;
    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_BACK: return Command::Back;
    case SDL_GAMEPAD_BUTTON_START: return Command::Start;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return Command::Next;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return Command::Previous;
    default: return std::nullopt;
    }
}

std::optional<Command> keyCommand(SDL_Keycode key) {
    switch (key) {
    case SDLK_UP: return Command::Up;
    case SDLK_DOWN: return Command::Down;
    case SDLK_LEFT: return Command::Left;
    case SDLK_RIGHT: return Command::Right;
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE:
    case SDLK_AC_BACK: return Command::Back;
    case SDLK_PAGEDOWN:
    case SDLK_TAB: return Command::Next;
    case SDLK_PAGEUP: return Command::Previous;
    default: return std::nullopt;
    }
}

} // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    SDL_SetAppMetadata("Nagus", std::string(nagus::kVersion).c_str(), "io.github.niliodev.nagus");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    auto app = std::make_unique<App>();
    // Sound is optional: without a device the game plays silently.
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, nagus::Jukebox::kRate};
        app->audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (app->audio != nullptr) {
            SDL_ResumeAudioStreamDevice(app->audio);
        } else {
            SDL_Log("No sound: %s", SDL_GetError());
        }
    }
    if (char* pref = SDL_GetPrefPath("NilioDev", "Nagus")) {
        app->prefDir = pathFromUtf8(pref);
        SDL_free(pref);
    } else {
        app->prefDir = fs::current_path() / "nagus-data";
    }
    app->library = app->prefDir / "library";
    std::error_code ec;
    fs::create_directories(app->library, ec);

    // A window as big as fits, in whole multiples of the original screen.
    int scale = 1;
    if (const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay())) {
        scale = std::max(1, std::min((mode->w * 9 / 10) / kW, (mode->h * 85 / 100) / kH));
    }
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef SDL_PLATFORM_ANDROID
    flags |= SDL_WINDOW_FULLSCREEN;
#endif
    if (!SDL_CreateWindowAndRenderer("Nagus", kW * scale, kH * scale, flags, &app->window, &app->renderer)) {
        SDL_Log("Cannot open a window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowMinimumSize(app->window, kW / 2, kH / 2);
    SDL_SetRenderVSync(app->renderer, 1);
    SDL_SetRenderLogicalPresentation(app->renderer, kW, kH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    app->frame = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kW, kH);
    SDL_SetTextureScaleMode(app->frame, SDL_SCALEMODE_PIXELART);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--fullscreen") {
            SDL_SetWindowFullscreen(app->window, true);
        } else if (!arg.empty() && arg[0] != '-') {
            queueImport(*app, arg);
        }
    }
    if (!libraryState(*app).game && !app->import.busy) {
        importFromGamedata(*app);
    }
    if (!app->import.busy && readyToPlay(*app)) {
        startShell(*app);
    }
    app->lastTicks = SDL_GetTicks();
    *appstate = app.release();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    App& app = *static_cast<App*>(appstate);
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_GAMEPAD_ADDED && app.pad == nullptr) {
        app.pad = SDL_OpenGamepad(event->gdevice.which);
    } else if (event->type == SDL_EVENT_GAMEPAD_REMOVED && app.pad != nullptr &&
               SDL_GetGamepadID(app.pad) == event->gdevice.which) {
        SDL_CloseGamepad(app.pad);
        app.pad = nullptr;
    }
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat &&
        (event->key.key == SDLK_F11 || (event->key.key == SDLK_RETURN && (event->key.mod & SDL_KMOD_ALT)))) {
        toggleFullscreen(app);
        return SDL_APP_CONTINUE;
    }
    if (!app.shell) {
        return importScreenEvent(app, event);
    }
    nagus::Shell& shell = *app.shell;
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        shell.setPointerVisible(event->motion.which != SDL_TOUCH_MOUSEID);
        movePointer(app, toScreen(app, event->motion.x, event->motion.y));
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            shell.setPointerVisible(event->button.which != SDL_TOUCH_MOUSEID);
            const Point p = toScreen(app, event->button.x, event->button.y);
            movePointer(app, p);
            shell.pointerPressed(p);
        } else if (event->button.button == SDL_BUTTON_RIGHT) {
            command(app, Command::Back);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            const Point p = toScreen(app, event->button.x, event->button.y);
            movePointer(app, p);
            shell.pointerReleased(p);
        }
        break;
    case SDL_EVENT_TEXT_INPUT:
        if (shell.wantsText() && event->text.text != nullptr) {
            shell.typed(event->text.text);
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (shell.wantsText() && (event->key.key == SDLK_BACKSPACE || event->key.key == SDLK_SPACE)) {
            // Typing: these keys are for the text (the space comes as text).
            if (event->key.key == SDLK_BACKSPACE) {
                shell.typed("\b");
            }
        } else if (shell.wantsText() && (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)) {
            shell.typed("\n");
        } else if (event->key.key == SDLK_RETURN || event->key.key == SDLK_SPACE || event->key.key == SDLK_KP_ENTER) {
            if (!event->key.repeat) {
                shell.clickAtPointer();
            }
        } else if (const auto c = keyCommand(event->key.key)) {
            shell.setPointerVisible(true);
            command(app, *c);
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        shell.setPointerVisible(true);
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
            app.padHeld = true;
            shell.pointerPressed(shell.pointer());
        } else if (const auto c = padCommand(event->gbutton.button)) {
            command(app, *c);
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH && app.padHeld) {
            app.padHeld = false;
            shell.pointerReleased(shell.pointer());
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            app.stickX = event->gaxis.value / 32767.0f;
        } else if (event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            app.stickY = event->gaxis.value / 32767.0f;
        } else if (event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX || event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY) {
            // The right stick jumps between buttons like the d-pad.
            const float v = event->gaxis.value / 32767.0f;
            const bool horizontal = event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX;
            int direction = -1;
            if (v > kStickCommand) {
                direction = horizontal ? 3 : 1;
            } else if (v < -kStickCommand) {
                direction = horizontal ? 2 : 0;
            }
            if (direction != app.stickDirection) {
                app.stickDirection = direction;
                if (direction >= 0) {
                    static constexpr Command kDirs[] = {Command::Up, Command::Down, Command::Left, Command::Right};
                    shell.setPointerVisible(true);
                    command(app, kDirs[direction]);
                }
            }
        }
        break;
    default:
        break;
    }
    if (shell.quit()) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    App& app = *static_cast<App*>(appstate);
    const Uint64 now = SDL_GetTicks();
    const float dt = static_cast<float>(now - app.lastTicks) / 1000.0f;
    app.lastTicks = now;
    if (!app.shell && app.import.changed.exchange(false) && readyToPlay(app)) {
        startShell(app);
    }
    if (app.shell && (std::fabs(app.stickX) > kStickDeadZone || std::fabs(app.stickY) > kStickDeadZone)) {
        // The left stick moves the pointer, faster the further it is pushed.
        auto curve = [](float v) {
            const float a = (std::fabs(v) - kStickDeadZone) / (1.0f - kStickDeadZone);
            return a <= 0 ? 0.0f : std::copysign(a * a, v);
        };
        app.pointerX = std::clamp(app.pointerX + curve(app.stickX) * kStickSpeed * dt, 0.0f, kW - 1.0f);
        app.pointerY = std::clamp(app.pointerY + curve(app.stickY) * kStickSpeed * dt, 0.0f, kH - 1.0f);
        app.shell->setPointerVisible(true);
        app.shell->pointerMoved({static_cast<int>(app.pointerX), static_cast<int>(app.pointerY)});
    }
    // A text field waiting for typing brings up the keyboard (on screen
    // where there is no other).
    if (app.shell) {
        const bool wanted = app.shell->wantsText();
        if (wanted != SDL_TextInputActive(app.window)) {
            if (wanted) {
                SDL_StartTextInput(app.window);
            } else {
                SDL_StopTextInput(app.window);
            }
        }
    }
    present(app);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult) {
    App* app = static_cast<App*>(appstate);
    if (app == nullptr) {
        return;
    }
    if (app->worker.joinable()) {
        app->worker.join();
    }
    if (app->pad != nullptr) {
        SDL_CloseGamepad(app->pad);
    }
    if (app->audio != nullptr) {
        SDL_DestroyAudioStream(app->audio);
    }
    SDL_DestroyTexture(app->frame);
    SDL_DestroyRenderer(app->renderer);
    SDL_DestroyWindow(app->window);
    delete app;
}
