// =============================================================================
// Krona Player — Application Core Implementation
// =============================================================================

#include "app.h"
#include "menu.h"
#include "video_player.h"

#include <cstdio>
#include <unistd.h>

namespace r36s {

static constexpr int R36S_SCREEN_W = 640;
static constexpr int R36S_SCREEN_H = 480;

App::App() 
    : window_(nullptr), renderer_(nullptr), audioDevice_(0), 
      controller_(nullptr), font_(nullptr), appState_(AppState::Menu), running_(true) {
}

App::~App() {
    cleanupSDL();
}

bool App::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "[App] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    // Audio Setup
    SDL_AudioSpec wanted_spec, spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = 44100;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;
    wanted_spec.samples = 1024;
    wanted_spec.callback = nullptr;
    
    audioDevice_ = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &spec, 0);
    if (audioDevice_ == 0) {
        fprintf(stderr, "[App] Warning: Failed to open audio: %s\n", SDL_GetError());
    } else {
        fprintf(stderr, "[App] Audio initialized: 44.1kHz S16 Stereo\n");
        SDL_PauseAudioDevice(audioDevice_, 0);
    }
    
    if (TTF_Init() != 0) {
        fprintf(stderr, "[App] TTF_Init failed: %s\n", TTF_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        "Krona Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        R36S_SCREEN_W, R36S_SCREEN_H,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN
    );
    if (!window_) {
        fprintf(stderr, "[App] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer_) {
        fprintf(stderr, "[App] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_ShowCursor(SDL_DISABLE);

    // Gamepad
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller_ = SDL_GameControllerOpen(i);
            if (controller_) {
                fprintf(stderr, "[App] Gamepad connected: %s\n", SDL_GameControllerName(controller_));
                break;
            }
        }
    }

    return loadFont();
}

bool App::loadFont() {
    const char* fontPaths[] = {
        "assets/font.ttf",
        "../assets/font.ttf",
        "/roms/ports/kronaplayer/assets/font.ttf",
        "font.ttf"
    };

    for (const char* path : fontPaths) {
        font_ = TTF_OpenFont(path, 24);
        if (font_) {
            fprintf(stderr, "[App] Successfully loaded font: %s\n", path);
            break;
        }
    }
    
    if (!font_) {
        fprintf(stderr, "[App] Warning: Could not load any font for the menu. TTF Error: %s\n", TTF_GetError());
    }
    return true; // We can still run without a font (though menu won't render text)
}

void App::cleanupSDL() {
    if (audioDevice_ > 0) SDL_CloseAudioDevice(audioDevice_);
    if (font_) TTF_CloseFont(font_);
    if (controller_) SDL_GameControllerClose(controller_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    
    TTF_Quit();
    SDL_Quit();
}

int App::run(int argc, char* argv[]) {
    if (!initSDL()) {
        return 1;
    }

    menu_ = std::make_unique<Menu>(this);
    videoPlayer_ = std::make_unique<VideoPlayer>(this);

    std::string videoToPlay = "";
    if (argc >= 2) {
        videoToPlay = argv[1];
        playVideo(videoToPlay);
    } else {
        menu_->refreshVideoList();
    }

    SDL_Event event;
    while (running_) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running_ = false;
                break;
            }
            
            if (appState_ == AppState::Menu) {
                menu_->handleEvent(event);
            } else if (appState_ == AppState::Playing) {
                videoPlayer_->handleEvent(event);
            }
        }

        if (!running_) break;

        SDL_SetRenderDrawColor(renderer_, 20, 20, 25, 255);
        SDL_RenderClear(renderer_);

        if (appState_ == AppState::Menu) {
            menu_->render();
        } else if (appState_ == AppState::Playing) {
            videoPlayer_->updateAndRender();
        }
    }

    fprintf(stderr, "[App] Shutting down...\n");
    return 0;
}

void App::changeState(AppState newState) {
    appState_ = newState;
}

void App::playVideo(const std::string& path) {
    if (videoPlayer_->openVideo(path)) {
        changeState(AppState::Playing);
    }
}

void App::returnToMenu() {
    videoPlayer_->closeVideo();
    menu_->refreshVideoList();
    changeState(AppState::Menu);
}

void App::quit() {
    running_ = false;
}

} // namespace r36s
