#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#endif
#include "bus.h"
#include "cartridge.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "savestate.h"

struct RomEntry {
    std::string path;
    std::string displayName;
    uint64_t lastPlayed = 0;
    uint32_t fileSize = 0;
    int mapper = -1;
    bool isPAL = false;
    bool missing = false;
    SDL_Texture* thumbTex = nullptr;
    bool thumbAttempted = false;
};

static std::vector<RomEntry> g_library;
static std::string g_libraryFile;
static std::string g_romsDir;
static std::string g_thumbDir;
static char g_search[128] = "";
static int g_sortMode = 0;
static int g_selectedIdx = -1;
static std::string g_deletingPath;
static bool g_confirmDelete = false;
static char g_renameBuf[256] = "";
static std::string g_renamingPath;
static bool g_showRename = false;
static bool g_showProperties = false;
static bool g_extracting = false;
static int g_extractIdx = -1;
static int g_extractFramesLeft = 0;
static int g_extractTotal = 0;
static int g_extractDone = 0;
static std::unique_ptr<Bus> g_extractBus;
static std::shared_ptr<Cartridge> g_extractCart;

static std::string getExeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string s = buf;
    size_t slash = s.find_last_of("\\/");
    return (slash == std::string::npos) ? "." : s.substr(0, slash);
#else
    return ".";
#endif
}

static bool dirExists(const std::string& p) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    return false;
#endif
}

static bool fileExists(const std::string& p) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    return false;
#endif
}

static void ensureDir(const std::string& p) {
#ifdef _WIN32
    if (!dirExists(p)) CreateDirectoryA(p.c_str(), NULL);
#endif
}

static std::string findRomsDir() {
    std::string e = getExeDir();
    std::string c[] = { e+"\\roms", e+"\\..\\roms", "roms", "..\\roms" };
    for (auto& x : c) if (dirExists(x)) return x;
    return e + "\\roms";
}

static std::string findLibraryPath() {
    std::string e = getExeDir();
    if (fileExists(e+"\\..\\CMakeLists.txt")) return e + "\\..\\library.txt";
    return e + "\\library.txt";
}

static std::string getFileName(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    return (s == std::string::npos) ? p : p.substr(s + 1);
}

static std::string sanitizeName(const std::string& p) {
    std::string s = p;
    for (auto& c : s) if (c=='/'||c=='\\'||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|') c = '_';
    return s;
}

static std::string getThumbPath(const std::string& romPath) {
    return g_thumbDir + "\\" + sanitizeName(romPath) + ".bmp";
}

static void readHeaderInfo(RomEntry& e) {
    FILE* f = fopen(e.path.c_str(), "rb");
    if (!f) { e.missing = true; e.mapper = -1; return; }
    Byte h[16];
    if (fread(h, 1, 16, f) != 16 || h[0] != 'N' || h[1] != 'E' || h[2] != 'S' || h[3] != 0x1A) {
        fclose(f); e.missing = true; e.mapper = -1; return;
    }
    fseek(f, 0, SEEK_END);
    e.fileSize = (uint32_t)ftell(f);
    fclose(f);
    e.mapper = (h[6] >> 4) | (h[7] & 0xF0);
    e.isPAL = (h[9] & 0x01) != 0;
    e.missing = false;
}

static void loadLibrary() {
    g_library.clear();
    std::ifstream f(g_libraryFile);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        size_t t1 = line.find('\t');
        if (t1 == std::string::npos) continue;
        size_t t2 = line.find('\t', t1+1);
        if (t2 == std::string::npos) continue;
        RomEntry e;
        e.path = line.substr(0, t1);
        if (e.path.empty()) continue;
        e.displayName = line.substr(t1+1, t2-t1-1);
        try { e.lastPlayed = std::stoull(line.substr(t2+1)); } catch (...) { e.lastPlayed = 0; }
        readHeaderInfo(e);
        g_library.push_back(e);
    }
}

static void saveLibrary() {
    std::ofstream f(g_libraryFile);
    for (auto& e : g_library)
        f << e.path << "\t" << e.displayName << "\t" << e.lastPlayed << "\n";
}

static void addToLibrary(const std::string& path) {
    if (path.empty()) return;
    for (auto& e : g_library) {
        if (e.path == path) { e.lastPlayed = (uint64_t)time(nullptr); return; }
    }
    RomEntry e;
    e.path = path;
    e.displayName = getFileName(path);
    e.lastPlayed = (uint64_t)time(nullptr);
    readHeaderInfo(e);
    g_library.push_back(e);
}

static void removeFromLibrary(const std::string& path) {
    g_library.erase(std::remove_if(g_library.begin(), g_library.end(),
        [&](const RomEntry& e) { return e.path == path; }), g_library.end());
}

static void scanRomsFolder(const std::string& dir) {
#ifdef _WIN32
    std::string search = dir + "\\*.nes";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string full = dir + "\\" + fd.cFileName;
            bool exists = false;
            for (auto& e : g_library) if (e.path == full) { exists = true; break; }
            if (!exists) {
                RomEntry e;
                e.path = full;
                e.displayName = fd.cFileName;
                readHeaderInfo(e);
                g_library.push_back(e);
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#endif
}

static void countNeededThumbs() {
    g_extractTotal = 0;
    g_extractDone = 0;
    for (auto& e : g_library) {
        if (e.missing) continue;
        if (!fileExists(getThumbPath(e.path))) g_extractTotal++;
    }
}

static bool saveThumbBmp(const uint32_t* pixels, const std::string& path) {
    ensureDir(g_thumbDir);
    static uint32_t buf[256*240];
    for (int i = 0; i < 256*240; i++) buf[i] = pixels[i] | 0xFF000000;
    SDL_Surface* surf = SDL_CreateRGBSurfaceFrom((void*)buf, 256, 240, 32, 256*4,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surf) return false;
    bool ok = (SDL_SaveBMP(surf, path.c_str()) == 0);
    SDL_FreeSurface(surf);
    return ok;
}

static SDL_Texture* loadThumbTex(SDL_Renderer* ren, const std::string& path) {
    SDL_Surface* surf = SDL_LoadBMP(path.c_str());
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    return tex;
}

static std::string openRomDialog() {
#ifdef _WIN32
    OPENFILENAMEW ofn;
    wchar_t fn[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"NES ROMs (*.nes)\0*.nes\0All Files\0*.*\0";
    ofn.lpstrFile = fn;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        int len = WideCharToMultiByte(CP_UTF8, 0, fn, -1, NULL, 0, NULL, NULL);
        std::string s(len-1, 0);
        WideCharToMultiByte(CP_UTF8, 0, fn, -1, &s[0], len, NULL, NULL);
        return s;
    }
#endif
    return "";
}

static std::string formatSize(uint32_t sz) {
    char b[32];
    if (sz < 1024) snprintf(b, sizeof(b), "%u B", sz);
    else if (sz < 1024*1024) snprintf(b, sizeof(b), "%u KB", sz/1024);
    else snprintf(b, sizeof(b), "%.1f MB", sz/1024.0f/1024.0f);
    return b;
}

static std::string formatTime(uint64_t t) {
    if (t == 0) return "never";
    time_t tt = (time_t)t;
    struct tm tb;
#ifdef _WIN32
    localtime_s(&tb, &tt);
#else
    localtime_r(&tt, &tb);
#endif
    char b[64];
    strftime(b, sizeof(b), "%Y-%m-%d %H:%M", &tb);
    return b;
}

static const char* mapperName(int m) {
    switch (m) {
    case 0: return "NROM";
    case 1: return "MMC1";
    case 2: return "UxROM";
    case 3: return "CNROM";
    case 4: return "MMC3";
    case 7: return "AxROM";
    case 66: return "GNROM";
    default: return "?";
    }
}

static const ImVec4 COL_ACCENT(0.42f, 0.78f, 0.42f, 1.0f);
static const ImVec4 COL_DIM(0.5f, 0.5f, 0.55f, 1.0f);
static const ImVec4 COL_ERROR(0.9f, 0.4f, 0.4f, 1.0f);

static bool g_turboLock = false;

int main(int argc, char** argv) {
    std::string romPath = (argc >= 2) ? argv[1] : "";
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);

    SDL_GameController* gamepad = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            gamepad = SDL_GameControllerOpen(i);
            if (gamepad) { fprintf(stderr, "Gamepad: %s\n", SDL_GameControllerName(gamepad)); break; }
        }
    }

    SDL_Rect bounds;
    SDL_GetDisplayUsableBounds(0, &bounds);
    int initW = std::max(800, bounds.w - 60);
    int initH = std::max(600, bounds.h - 60);

    SDL_Window* win = SDL_CreateWindow("Nyx NES",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        initW, initH, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 256, 240);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    {
        ImGuiStyle& st = ImGui::GetStyle();
        st.WindowRounding = 4;
        st.FrameRounding = 4;
        st.WindowBorderSize = 0;
        st.FramePadding = ImVec2(6, 4);
    }
    ImGui_ImplSDL2_InitForSDLRenderer(win, ren);
    ImGui_ImplSDLRenderer2_Init(ren);

    g_libraryFile = findLibraryPath();
    g_romsDir = findRomsDir();
    g_thumbDir = g_romsDir + "\\.thumbs";
    ensureDir(g_thumbDir);
    loadLibrary();
    if (dirExists(g_romsDir)) scanRomsFolder(g_romsDir);
    saveLibrary();
    countNeededThumbs();

    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    SDL_AudioDeviceID audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audioDev) SDL_PauseAudioDevice(audioDev, 0);

    std::shared_ptr<Cartridge> cart;
    std::unique_ptr<Bus> bus;
    std::string romBasePath;
    bool running = true;
    bool paused = false;
    bool palMode = false;
    int cyclesPerFrame = 89342;
    float volume = 0.7f;
    bool showLibrary = true;
    bool gridView = true;
    bool scanlines = false;
    int videoMode = 0;

    auto realFrameHz = [](bool pal) -> double {
        return pal ? (5320342.0 / 106392.0) : (5369318.0 / 89342.0);
    };

    auto loadRom = [&](const std::string& path) -> bool {
        auto c = std::make_shared<Cartridge>(path);
        if (!c->ImageValid()) return false;
        std::string low = path;
        for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
        if (low.find("europe") != std::string::npos || low.find("(e)") != std::string::npos)
            c->isPAL = true;
        cart = c;
        bus = std::make_unique<Bus>();
        bus->insertCartridge(cart);
        bus->cpu->Reset();
        romBasePath = path;
        cart->loadBatteryRam(path);
        palMode = cart->isPAL;
        cyclesPerFrame = palMode ? 106392 : 89342;
        paused = false;
        addToLibrary(path);
        saveLibrary();
        showLibrary = false;
        SDL_SetWindowTitle(win, ("Nyx NES - " + path).c_str());
        return true;
    };

    if (!romPath.empty()) loadRom(romPath);

    Uint64 perfFreq = SDL_GetPerformanceFrequency();
    Uint64 lastFrame = SDL_GetPerformanceCounter();
    Uint64 fpsTimer = SDL_GetPerformanceCounter();
    int fpsFrames = 0;
    float fps = 0.0f;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_DROPFILE) {
                std::string dropped = e.drop.file;
                SDL_free(e.drop.file);
                size_t dot = dropped.find_last_of('.');
                if (dot != std::string::npos) {
                    std::string ext = dropped.substr(dot);
                    for (auto& ch : ext) ch = (char)tolower((unsigned char)ch);
                    if (ext == ".nes") { addToLibrary(dropped); saveLibrary(); countNeededThumbs(); loadRom(dropped); }
                }
            }
            if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_F2:
                    if (cart) { bus = std::make_unique<Bus>(); bus->insertCartridge(cart); bus->cpu->Reset(); }
                    break;
                case SDLK_SPACE:
                    if (bus && !showLibrary) paused = !paused;
                    break;
                case SDLK_F5:
                    if (bus && !romBasePath.empty()) {
                        FileWriter w((romBasePath + ".state1").c_str());
                        if (w.ok()) bus->serialize(w);
                    }
                    break;
                case SDLK_F7:
                    if (bus && !romBasePath.empty()) {
                        FileReader r((romBasePath + ".state1").c_str());
                        if (r.ok()) bus->serialize(r);
                    }
                    break;
                case SDLK_F11: {
                    Uint32 f = SDL_GetWindowFlags(win);
                    SDL_SetWindowFullscreen(win, (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                } break;
                case SDLK_l:
                    if (e.key.keysym.mod & KMOD_CTRL) showLibrary = !showLibrary;
                    break;
                case SDLK_o:
                    if (e.key.keysym.mod & KMOD_CTRL) {
                        std::string p = openRomDialog();
                        if (!p.empty()) loadRom(p);
                    }
                    break;
                }
            }
        }

        if (bus) {
            Byte pad = 0;
            const Uint8* kb = SDL_GetKeyboardState(NULL);
            if (kb[SDL_SCANCODE_UP])     pad |= 0x10;
            if (kb[SDL_SCANCODE_DOWN])   pad |= 0x20;
            if (kb[SDL_SCANCODE_LEFT])   pad |= 0x40;
            if (kb[SDL_SCANCODE_RIGHT])  pad |= 0x80;
            if (kb[SDL_SCANCODE_Z])      pad |= 0x01;
            if (kb[SDL_SCANCODE_X])      pad |= 0x02;
            if (kb[SDL_SCANCODE_RETURN]) pad |= 0x08;
            if (gamepad) {
                if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_DPAD_UP) ||
                    SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTY) < -8000) pad |= 0x10;
                if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) ||
                    SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTY) > 8000) pad |= 0x20;
                if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) ||
                    SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTX) < -8000) pad |= 0x40;
                if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ||
                    SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTX) > 8000) pad |= 0x80;
                if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_A) ||
                    SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_Y)) pad |= 0x01;
                if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_B) ||
                    SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_X)) pad |= 0x02;
                if (SDL_GameControllerGetButton(gamepad, SDL_CONTROLLER_BUTTON_START)) pad |= 0x08;
            }
            bus->controller1 = pad;
        }

        if (bus && !paused && !showLibrary) {
            bool turbo = g_turboLock || SDL_GetKeyboardState(NULL)[SDL_SCANCODE_TAB] != 0;
            int cycles = turbo ? cyclesPerFrame * 4 : cyclesPerFrame;
            for (int i = 0; i < cycles; i++) bus->clock();
            if (audioDev) {
                Uint32 queued = SDL_GetQueuedAudioSize(audioDev);
                int room = 16384 - (int)(queued / sizeof(int16_t));
                if (room > 0) {
                    int want2 = room < 2048 ? room : 2048;
                    int16_t samples[2048];
                    int n = bus->apu->drain(samples, want2);
                    if (n > 0) {
                        if (volume != 1.0f)
                            for (int k = 0; k < n; k++) samples[k] = (int16_t)(samples[k] * volume);
                        SDL_QueueAudio(audioDev, samples, n * sizeof(int16_t));
                    }
                }
            }
        }

        if (showLibrary) {
            for (auto& en : g_library) {
                if (en.thumbTex || en.thumbAttempted) continue;
                std::string tp = getThumbPath(en.path);
                if (fileExists(tp)) { en.thumbTex = loadThumbTex(ren, tp); en.thumbAttempted = true; }
            }
            if (!g_extracting) {
                for (int i = 0; i < (int)g_library.size(); i++) {
                    if (g_library[i].missing) continue;
                    if (g_library[i].thumbAttempted) continue;
                    if (g_library[i].thumbTex) continue;
                    g_extractCart = std::make_shared<Cartridge>(g_library[i].path);
                    if (!g_extractCart->ImageValid()) { g_library[i].thumbAttempted = true; g_extractCart.reset(); continue; }
                    g_extractBus = std::make_unique<Bus>();
                    g_extractBus->insertCartridge(g_extractCart);
                    g_extractBus->cpu->Reset();
                    g_extractIdx = i;
                    g_extractFramesLeft = 1500;
                    g_extracting = true;
                    break;
                }
            }
            if (g_extracting && g_extractBus) {
                for (int f = 0; f < 10 && g_extractFramesLeft > 0; f++) {
                    for (int c = 0; c < 89342; c++) g_extractBus->clock();
                    g_extractFramesLeft--;
                }
                if (g_extractFramesLeft <= 0) {
                    std::string tp = getThumbPath(g_library[g_extractIdx].path);
                    saveThumbBmp(g_extractBus->ppu->frame, tp);
                    g_library[g_extractIdx].thumbTex = loadThumbTex(ren, tp);
                    g_library[g_extractIdx].thumbAttempted = true;
                    g_extractDone++;
                    g_extractBus.reset();
                    g_extractCart.reset();
                    g_extracting = false;
                }
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load ROM...", "Ctrl+O")) {
                    std::string p = openRomDialog();
                    if (!p.empty()) loadRom(p);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Esc")) running = false;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Emulation")) {
                if (ImGui::MenuItem(paused ? "Resume" : "Pause", "Space", false, bus != nullptr && !showLibrary)) paused = !paused;
                if (ImGui::MenuItem("Reset", "F2", false, bus != nullptr)) {
                    if (cart) { bus = std::make_unique<Bus>(); bus->insertCartridge(cart); bus->cpu->Reset(); }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Fast Forward (4x)", "TAB", g_turboLock)) {
                    g_turboLock = !g_turboLock;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("ROM Library", "Ctrl+L", showLibrary)) showLibrary = !showLibrary;
                if (ImGui::BeginMenu("Video Scale")) {
                    if (ImGui::MenuItem("Integer (crisp 4x)", nullptr, videoMode == 0)) videoMode = 0;
                    if (ImGui::MenuItem("Fit (smooth, aspect)", nullptr, videoMode == 1)) videoMode = 1;
                    if (ImGui::MenuItem("Stretch (fill, crisp)", nullptr, videoMode == 2)) videoMode = 2;
                    if (ImGui::MenuItem("4:3 Retro", nullptr, videoMode == 3)) videoMode = 3;
                    if (ImGui::MenuItem("Smooth Fill", nullptr, videoMode == 4)) videoMode = 4;
                    ImGui::Separator();
                    if (ImGui::MenuItem("CRT Scanlines", nullptr, scanlines)) scanlines = !scanlines;
                    ImGui::EndMenu();
                }
                bool fs = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
                if (ImGui::MenuItem("Fullscreen", "F11", fs))
                    SDL_SetWindowFullscreen(win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Audio")) {
                ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f, "%.2f");
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        int ww = 0, wh = 0;
        SDL_GetRendererOutputSize(ren, &ww, &wh);

        if (showLibrary) {
            ImGui::SetNextWindowPos(ImVec2(0, 20), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2((float)ww, (float)(wh - 20)), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("##lib", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
            ImGui::PopStyleVar(3);

            ImGui::TextColored(COL_ACCENT, "ROM LIBRARY");
            ImGui::SameLine();
            ImGui::TextDisabled("  %d game%s", (int)g_library.size(), g_library.size() == 1 ? "" : "s");
            if (g_extracting) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "  - generating thumbnails %d/%d...", g_extractDone + 1, g_extractTotal);
            }
            ImGui::Spacing();

            if (ImGui::Button("  Add ROM  ")) {
                std::string p = openRomDialog();
                if (!p.empty()) { addToLibrary(p); saveLibrary(); countNeededThumbs(); }
            }
            ImGui::SameLine();
            if (ImGui::Button("  Rescan  ")) {
                if (dirExists(g_romsDir)) scanRomsFolder(g_romsDir);
                saveLibrary();
                countNeededThumbs();
            }
            ImGui::SameLine();
            if (ImGui::Button("  Rebuild Thumbs  ")) {
#ifdef _WIN32
                std::string pattern = g_thumbDir + "\\*.bmp";
                WIN32_FIND_DATAA fd;
                HANDLE hf = FindFirstFileA(pattern.c_str(), &fd);
                if (hf != INVALID_HANDLE_VALUE) {
                    do { remove((g_thumbDir + "\\" + fd.cFileName).c_str()); } while (FindNextFileA(hf, &fd));
                    FindClose(hf);
                }
#endif
                for (auto& en : g_library) {
                    if (en.thumbTex) { SDL_DestroyTexture(en.thumbTex); en.thumbTex = nullptr; }
                    en.thumbAttempted = false;
                }
                countNeededThumbs();
            }
            ImGui::SameLine();
            if (ImGui::Button(gridView ? "  List View  " : "  Grid View  ")) gridView = !gridView;
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(20, 0));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(260);
            ImGui::InputTextWithHint("##srch", "Search...", g_search, IM_ARRAYSIZE(g_search));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140);
            const char* sorts[] = { "Recent", "Name A-Z", "Name Z-A" };
            ImGui::Combo("##sort", &g_sortMode, sorts, 3);

            ImGui::Separator();

            std::vector<int> visible;
            std::string sl = g_search;
            for (auto& ch : sl) ch = (char)tolower((unsigned char)ch);
            for (int i = 0; i < (int)g_library.size(); i++) {
                if (!sl.empty()) {
                    std::string nl = g_library[i].displayName;
                    for (auto& ch : nl) ch = (char)tolower((unsigned char)ch);
                    if (nl.find(sl) == std::string::npos) continue;
                }
                visible.push_back(i);
            }
            if (g_sortMode == 0) std::sort(visible.begin(), visible.end(), [](int a, int b){ return g_library[a].lastPlayed > g_library[b].lastPlayed; });
            else if (g_sortMode == 1) std::sort(visible.begin(), visible.end(), [](int a, int b){ return g_library[a].displayName < g_library[b].displayName; });
            else std::sort(visible.begin(), visible.end(), [](int a, int b){ return g_library[a].displayName > g_library[b].displayName; });

            float bottomBar = 110.0f;
            ImGui::BeginChild("##list", ImVec2(0, -bottomBar), false);

            if (visible.empty()) {
                ImGui::Dummy(ImVec2(0, 40));
                ImGui::Indent(20);
                if (g_library.empty()) {
                    ImGui::TextDisabled("No games yet.");
                    ImGui::TextDisabled("Drop .nes files into %s", g_romsDir.c_str());
                } else {
                    ImGui::TextDisabled("No matches.");
                }
                ImGui::Unindent(20);
            } else if (gridView) {
                float cardW = 180.0f;
                float imgH = cardW * 240.0f / 256.0f;
                float cardH = imgH + 44.0f;
                float sp = 12.0f;
                float availW = ImGui::GetContentRegionAvail().x;
                int cols = std::max(1, (int)((availW + sp) / (cardW + sp)));
                ImDrawList* dl = ImGui::GetWindowDrawList();
                for (size_t i = 0; i < visible.size(); i++) {
                    int idx = visible[i];
                    RomEntry& ent = g_library[idx];
                    ImGui::PushID((int)i);
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    ImGui::InvisibleButton("card", ImVec2(cardW, cardH));
                    bool hovered = ImGui::IsItemHovered();
                    bool clicked = ImGui::IsItemClicked(0);
                    bool rightClicked = ImGui::IsItemClicked(1);
                    if (ImGui::IsMouseDoubleClicked(0) && hovered && !ent.missing) loadRom(ent.path);
                    if (clicked || rightClicked) { g_selectedIdx = idx; g_showRename = false; g_showProperties = false; }
                    ImU32 bgCol = hovered ? IM_COL32(55,55,70,255) : IM_COL32(38,38,50,255);
                    dl->AddRectFilled(pos, ImVec2(pos.x + cardW, pos.y + cardH), bgCol, 8.0f);
                    ImVec2 imgTL(pos.x + 4, pos.y + 4);
                    ImVec2 imgBR(pos.x + cardW - 4, pos.y + imgH + 4);
                    if (ent.thumbTex) {
                        dl->AddImage((ImTextureID)(intptr_t)ent.thumbTex, imgTL, imgBR);
                    } else {
                        dl->AddRectFilled(imgTL, imgBR, IM_COL32(20,20,28,255), 4.0f);
                        dl->AddText(ImVec2(imgTL.x + 10, imgTL.y + imgH/2 - 8),
                            IM_COL32(90,90,100,255), ent.thumbAttempted ? "no preview" : "loading...");
                    }
                    if (g_selectedIdx == idx) {
                        dl->AddRect(pos, ImVec2(pos.x + cardW, pos.y + cardH),
                            IM_COL32(100, 200, 100, 255), 8.0f, 0, 2.5f);
                    }
                    if (ent.missing) {
                        dl->AddRectFilled(imgTL, imgBR, IM_COL32(150, 0, 0, 100), 4.0f);
                        dl->AddText(ImVec2(imgTL.x + 10, imgTL.y + 10), IM_COL32(255, 180, 180, 255), "MISSING");
                    }
                    char nameBuf[64];
                    strncpy(nameBuf, ent.displayName.c_str(), 30);
                    nameBuf[30] = 0;
                    dl->AddText(ImVec2(pos.x + 8, pos.y + imgH + 14), IM_COL32(235,235,245,255), nameBuf);
                    char tagBuf[64];
                    snprintf(tagBuf, sizeof(tagBuf), "%s - %s", mapperName(ent.mapper), ent.isPAL ? "PAL" : "NTSC");
                    dl->AddText(ImVec2(pos.x + 8, pos.y + imgH + 30), IM_COL32(140,140,150,255), tagBuf);
                    ImGui::PopID();
                    if ((int)(i + 1) % cols != 0 && i + 1 < visible.size()) ImGui::SameLine(0, sp);
                }
            } else {
                for (size_t i = 0; i < visible.size(); i++) {
                    int idx = visible[i];
                    RomEntry& ent = g_library[idx];
                    ImGui::PushID((int)i);
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    ImGui::InvisibleButton("row", ImVec2(ImGui::GetContentRegionAvail().x, 52));
                    bool hovered = ImGui::IsItemHovered();
                    bool clicked = ImGui::IsItemClicked(0);
                    if (ImGui::IsMouseDoubleClicked(0) && hovered && !ent.missing) loadRom(ent.path);
                    if (clicked) { g_selectedIdx = idx; g_showRename = false; g_showProperties = false; }
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    if (g_selectedIdx == idx) {
                        dl->AddRectFilled(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + 52), IM_COL32(60, 90, 60, 200), 4.0f);
                    } else if (hovered) {
                        dl->AddRectFilled(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + 52), IM_COL32(50, 50, 60, 150), 4.0f);
                    }
                    float thumbW = 56;
                    float thumbH = 56 * 240.0f / 256.0f;
                    ImVec2 tTL(pos.x + 4, pos.y + (52 - thumbH) / 2);
                    ImVec2 tBR(tTL.x + thumbW, tTL.y + thumbH);
                    if (ent.thumbTex) dl->AddImage((ImTextureID)(intptr_t)ent.thumbTex, tTL, tBR);
                    else dl->AddRectFilled(tTL, tBR, IM_COL32(25,25,35,255), 3.0f);
                    dl->AddText(ImVec2(pos.x + thumbW + 16, pos.y + 8),
                        ent.missing ? IM_COL32(220, 100, 100, 255) : IM_COL32(235,235,245,255), ent.displayName.c_str());
                    char meta[160];
                    snprintf(meta, sizeof(meta), "%s  -  %s  -  %s  -  %s",
                        mapperName(ent.mapper), ent.isPAL ? "PAL" : "NTSC",
                        formatSize(ent.fileSize).c_str(), formatTime(ent.lastPlayed).c_str());
                    dl->AddText(ImVec2(pos.x + thumbW + 16, pos.y + 28), IM_COL32(140,140,150,255), meta);
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();

            // Keyboard navigation in library
            if (showLibrary && !g_extracting) {
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                    if (g_selectedIdx < 0) g_selectedIdx = 0;
                    else if (g_selectedIdx < (int)g_library.size() - 1) g_selectedIdx++;
                }
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                    if (g_selectedIdx > 0) g_selectedIdx--;
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Enter) && g_selectedIdx >= 0
                    && g_selectedIdx < (int)g_library.size()
                    && !g_library[g_selectedIdx].missing) {
                    loadRom(g_library[g_selectedIdx].path);
                }
            }

            ImGui::Separator();
            if (g_selectedIdx >= 0 && g_selectedIdx < (int)g_library.size()) {
                RomEntry& sel = g_library[g_selectedIdx];
                ImGui::TextColored(COL_ACCENT, "> %s", sel.displayName.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("  (%s, %s)", mapperName(sel.mapper), sel.isPAL ? "PAL" : "NTSC");
                ImGui::BeginDisabled(sel.missing);
                if (ImGui::Button("  Play  ") || ImGui::IsKeyPressed(ImGuiKey_Enter)) loadRom(sel.path);
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("  Rename  ")) {
                    g_showRename = true; g_showProperties = false;
                    g_renamingPath = sel.path;
                    memset(g_renameBuf, 0, sizeof(g_renameBuf));
                    strncpy(g_renameBuf, sel.displayName.c_str(), sizeof(g_renameBuf) - 1);
                }
                ImGui::SameLine();
                if (ImGui::Button("  Properties  ")) { g_showProperties = !g_showProperties; g_showRename = false; }
                ImGui::SameLine();
                if (ImGui::Button("  Reveal  ")) {
#ifdef _WIN32
                    std::string args = "/select,\"" + sel.path + "\"";
                    ShellExecuteA(NULL, "open", "explorer.exe", args.c_str(), NULL, SW_SHOW);
#endif
                }
                ImGui::SameLine();
                if (ImGui::Button("  Remove  ")) {
                    removeFromLibrary(sel.path);
                    saveLibrary();
                    g_selectedIdx = -1;
                    countNeededThumbs();
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.15f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.2f, 0.2f, 1));
                if (ImGui::Button("  Delete File  ")) { g_confirmDelete = true; g_deletingPath = sel.path; }
                ImGui::PopStyleColor(2);
            } else {
                ImGui::TextDisabled("Select a game - Enter to play, or double-click");
            }

            if (g_showRename) {
                ImGui::Separator();
                ImGui::Text("Rename:");
                ImGui::SetNextItemWidth(400);
                ImGui::InputText("##rn", g_renameBuf, IM_ARRAYSIZE(g_renameBuf));
                if (ImGui::Button("  Save  ")) {
                    for (auto& en : g_library) if (en.path == g_renamingPath) { en.displayName = g_renameBuf; break; }
                    saveLibrary();
                    g_showRename = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("  Cancel  ")) g_showRename = false;
            }
            if (g_showProperties && g_selectedIdx >= 0 && g_selectedIdx < (int)g_library.size()) {
                RomEntry& s = g_library[g_selectedIdx];
                ImGui::Separator();
                ImGui::Text("Path: %s", s.path.c_str());
                ImGui::Text("Mapper: %d (%s)   Region: %s   Size: %s   Last played: %s",
                    s.mapper, mapperName(s.mapper), s.isPAL ? "PAL" : "NTSC",
                    formatSize(s.fileSize).c_str(), formatTime(s.lastPlayed).c_str());
            }
            if (g_confirmDelete) {
                ImGui::Separator();
                ImGui::TextColored(COL_ERROR, "Delete file from disk?");
                ImGui::Text("%s", g_deletingPath.c_str());
                if (ImGui::Button("  Yes, Delete  ")) {
                    remove(g_deletingPath.c_str());
                    removeFromLibrary(g_deletingPath);
                    saveLibrary();
                    g_selectedIdx = -1;
                    g_confirmDelete = false;
                    countNeededThumbs();
                }
                ImGui::SameLine();
                if (ImGui::Button("  Cancel  ")) g_confirmDelete = false;
            }
            ImGui::End();
        } else {
            ImGui::SetNextWindowPos(ImVec2(10, 28), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.55f);
            ImGui::Begin("##stats", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
            ImGui::Text("FPS %.1f | %s | %s | PC=%04X",
                fps, paused ? "PAUSED" : "RUN", palMode ? "PAL" : "NTSC",
                bus ? bus->cpu->pc : 0);
            ImGui::End();
        }

        ImGui::Render();

        SDL_SetRenderDrawColor(ren, 12, 12, 18, 255);
        SDL_RenderClear(ren);
        if (bus && !showLibrary) {
            SDL_UpdateTexture(tex, nullptr, bus->ppu->frame, 256 * sizeof(uint32_t));
            SDL_Rect dst = {0, 0, 0, 0};
            switch (videoMode) {
            case 0: {
                int s = std::min(ww / 256, wh / 240);
                if (s < 1) s = 1;
                int dw = 256 * s, dh = 240 * s;
                dst = { (ww - dw)/2, (wh - dh)/2, dw, dh };
                SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
                break;
            }
            case 1: {
                float s = std::min((float)ww / 256.0f, (float)wh / 240.0f);
                int dw = (int)(256 * s), dh = (int)(240 * s);
                dst = { (ww - dw)/2, (wh - dh)/2, dw, dh };
                SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
                break;
            }
            case 2: {
                dst = { 0, 0, ww, wh };
                SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
                break;
            }
            case 3: {
                float ta = 4.0f / 3.0f;
                int dw = ww, dh = (int)(ww / ta);
                if (dh > wh) { dh = wh; dw = (int)(wh * ta); }
                dst = { (ww - dw)/2, (wh - dh)/2, dw, dh };
                SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
                break;
            }
            case 4: {
                dst = { 0, 0, ww, wh };
                SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
                break;
            }
            }
            SDL_RenderCopy(ren, tex, nullptr, &dst);
            if (scanlines) {
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 100);
                for (int y = dst.y; y < dst.y + dst.h; y += 2) {
                    SDL_Rect line = { dst.x, y, dst.w, 1 };
                    SDL_RenderFillRect(ren, &line);
                }
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                SDL_SetRenderDrawColor(ren, 12, 12, 18, 255);
            }
        }
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), ren);
        SDL_RenderPresent(ren);

        Uint64 targetTicks = (Uint64)(perfFreq / realFrameHz(palMode));
        Uint64 now = SDL_GetPerformanceCounter();
        if (now - lastFrame < targetTicks) {
            Uint64 rem = targetTicks - (now - lastFrame);
            Uint32 ms = (Uint32)((rem * 1000) / perfFreq);
            if (ms > 0) SDL_Delay(ms);
            while (SDL_GetPerformanceCounter() - lastFrame < targetTicks) {}
        }
        lastFrame = SDL_GetPerformanceCounter();

        static Uint32 autoSaveTimer = 0;
        if (bus && !paused && !showLibrary && !romBasePath.empty()) {
            if (autoSaveTimer == 0) autoSaveTimer = SDL_GetTicks();
            else if (SDL_GetTicks() - autoSaveTimer > 60000) {
                FileWriter w((romBasePath + ".state0").c_str());
                if (w.ok()) bus->serialize(w);
                autoSaveTimer = SDL_GetTicks();
            }
        }

        fpsFrames++;
        if (SDL_GetPerformanceCounter() - fpsTimer >= perfFreq) {
            fps = (float)fpsFrames;
            fpsFrames = 0;
            fpsTimer = SDL_GetPerformanceCounter();
        }
    }

    if (cart && !romBasePath.empty()) cart->saveBatteryRam(romBasePath);
    for (auto& e : g_library) if (e.thumbTex) SDL_DestroyTexture(e.thumbTex);
    if (gamepad) SDL_GameControllerClose(gamepad);
    if (audioDev) SDL_CloseAudioDevice(audioDev);
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
