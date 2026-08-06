// =============================================================================
// Krona Player — Main Entry Point
// =============================================================================
// Initialises SDL2 (Video + Audio), opens an MP4 file via the Decoder, and
// runs the main event / render loop.
//
// Usage:  ./krona_player [path_to_video.mp4]
//
// If no arguments are provided, it opens a simple file browser in the 
// current directory (or videos/ if it exists).
// =============================================================================

#include "decoder.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <exception>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr int R36S_SCREEN_W = 640;
static constexpr int R36S_SCREEN_H = 480;

enum class AppState {
    Menu,
    Playing
};

// ---------------------------------------------------------------------------
// Helper: Render Text
// ---------------------------------------------------------------------------
static SDL_Texture* renderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, SDL_Color color) {
    if (text.empty()) return nullptr;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

// ---------------------------------------------------------------------------
// Helper: Find Videos
// ---------------------------------------------------------------------------
static std::vector<std::string> findVideos(const std::string& dirPath) {
    std::vector<std::string> videos;
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        return videos;
    }
    
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            // Convert to lowercase
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".mp4") {
                videos.push_back(entry.path().string());
            }
        }
    }
    std::sort(videos.begin(), videos.end());
    return videos;
}

// =============================================================================
// Main (Internal)
// =============================================================================
static int runApp(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "[Main] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // ---- Audio Setup --------------------------------------------------------
    SDL_AudioSpec wanted_spec, spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = 44100;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;
    wanted_spec.samples = 1024; // safe buffer size
    wanted_spec.callback = nullptr;
    
    SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &spec, 0);
    if (audioDevice == 0) {
        fprintf(stderr, "[Main] Warning: Failed to open audio: %s\n", SDL_GetError());
    } else {
        fprintf(stderr, "[Main] Audio initialized: 44.1kHz S16 Stereo\n");
        SDL_PauseAudioDevice(audioDevice, 0); // start consuming
    }
    
    if (TTF_Init() != 0) {
        fprintf(stderr, "[Main] TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Krona Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        R36S_SCREEN_W, R36S_SCREEN_H,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "[Main] SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "[Main] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_ShowCursor(SDL_DISABLE);

    // ---- Open a gamepad if available ----------------------------------------
    SDL_GameController* controller = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) {
                fprintf(stderr, "[Main] Gamepad connected: %s\n",
                        SDL_GameControllerName(controller));
                break;
            }
        }
    }

    // ---- App State Setup ----------------------------------------------------
    AppState appState = AppState::Menu;
    std::string videoToPlay = "";
    
    if (argc >= 2) {
        videoToPlay = argv[1];
        appState = AppState::Playing;
    }

    // ---- Menu State Variables -----------------------------------------------
    std::vector<std::string> videoFiles;
    int selectedVideoIdx = 0;
    TTF_Font* font = nullptr;
    
    if (appState == AppState::Menu) {
        // Look for videos in "videos/" or current directory
        videoFiles = findVideos("videos");
        if (videoFiles.empty()) {
            videoFiles = findVideos(".");
        }
        
        // Try to load font from various locations
        const char* fontPaths[] = {
            "assets/font.ttf",
            "../assets/font.ttf",
            "/roms/ports/kronaplayer/assets/font.ttf",
            "font.ttf"
        };
        
        for (const char* path : fontPaths) {
            font = TTF_OpenFont(path, 24);
            if (font) break;
        }
        
        if (!font) {
            fprintf(stderr, "[Main] Warning: Could not load any font for the menu. TTF Error: %s\n", TTF_GetError());
            // If we have videos but no font, just auto-play the first one
            if (!videoFiles.empty()) {
                videoToPlay = videoFiles[0];
                appState = AppState::Playing;
            }
        }
    }

    // ---- Player State Variables ---------------------------------------------
    r36s::Decoder decoder;
    SDL_Texture* videoTexture = nullptr;
    r36s::PlayerState playerState;
    Uint32 frameDelayMs = 33; // ~30fps default

    // Helper lambda to transition from Menu to Playing
    auto startPlayback = [&](const std::string& path) {
        if (!decoder.open(path)) {
            fprintf(stderr, "[Main] Failed to open video: %s\n", path.c_str());
            return false;
        }
        
        const int videoW = decoder.getWidth();
        const int videoH = decoder.getHeight();
        fprintf(stderr, "[Main] Video resolution: %dx%d\n", videoW, videoH);

        if (videoTexture) SDL_DestroyTexture(videoTexture);
        
        videoTexture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_IYUV, // YUV420P
            SDL_TEXTUREACCESS_STREAMING,
            videoW, videoH
        );
        
        if (!videoTexture) {
            fprintf(stderr, "[Main] SDL_CreateTexture failed: %s\n", SDL_GetError());
            decoder.close();
            return false;
        }
        
        playerState.play();
        
        AVCodecContext* vctx = decoder.getVideoCodecCtx();
        double fps = 30.0;
        if (vctx && vctx->framerate.num > 0 && vctx->framerate.den > 0) {
            fps = static_cast<double>(vctx->framerate.num) / vctx->framerate.den;
        }
        frameDelayMs = static_cast<Uint32>(1000.0 / fps);
        
        appState = AppState::Playing;
        return true;
    };

    // ---- Main Loop ----------------------------------------------------------
    bool running = true;
    SDL_Event event;

    // If launched with arg, start immediately
    if (appState == AppState::Playing) {
        if (!startPlayback(videoToPlay)) {
            running = false; // Will trigger exit
        }
    }

    while (running) {
        Uint32 frameStart = SDL_GetTicks();

        // ---- Event handling -------------------------------------------------
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                        if (appState == AppState::Playing && videoFiles.size() > 0) {
                            // Return to menu
                            decoder.close();
                            appState = AppState::Menu;
                        } else {
                            running = false;
                        }
                    } else if (appState == AppState::Menu) {
                        if (event.key.keysym.sym == SDLK_UP) {
                            selectedVideoIdx = std::max(0, selectedVideoIdx - 1);
                        } else if (event.key.keysym.sym == SDLK_DOWN) {
                            selectedVideoIdx = std::min((int)videoFiles.size() - 1, selectedVideoIdx + 1);
                        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) {
                            if (!videoFiles.empty()) {
                                startPlayback(videoFiles[selectedVideoIdx]);
                            }
                        }
                    } else if (appState == AppState::Playing) {
                        if (event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_RETURN) {
                            playerState.togglePause();
                        }
                    }
                    break;

                case SDL_CONTROLLERBUTTONDOWN:
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                        // START on R36S → Quit (or return to menu)
                        if (appState == AppState::Playing && videoFiles.size() > 0) {
                            decoder.close();
                            appState = AppState::Menu;
                        } else {
                            running = false;
                        }
                    } else if (appState == AppState::Menu) {
                        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                            selectedVideoIdx = std::max(0, selectedVideoIdx - 1);
                        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                            selectedVideoIdx = std::min((int)videoFiles.size() - 1, selectedVideoIdx + 1);
                        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                            if (!videoFiles.empty()) {
                                startPlayback(videoFiles[selectedVideoIdx]);
                            }
                        }
                    } else if (appState == AppState::Playing) {
                        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                            playerState.togglePause();
                        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                            // B also returns to menu
                            if (videoFiles.size() > 0) {
                                decoder.close();
                                appState = AppState::Menu;
                            } else {
                                running = false;
                            }
                        }
                    }
                    break;

                default:
                    break;
            }
        }

        // ---- Render & Logic -------------------------------------------------
        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255); // Dark background
        SDL_RenderClear(renderer);

        if (appState == AppState::Menu) {
            // Render Menu
            if (!font) {
                // Should only happen if there are no videos AND no font
            } else if (videoFiles.empty()) {
                SDL_Texture* t = renderText(renderer, font, "No .mp4 files found in videos/", {255, 100, 100, 255});
                if (t) {
                    int w, h;
                    SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
                    SDL_Rect dst = { (R36S_SCREEN_W - w)/2, (R36S_SCREEN_H - h)/2, w, h };
                    SDL_RenderCopy(renderer, t, nullptr, &dst);
                    SDL_DestroyTexture(t);
                }
            } else {
                // Header
                SDL_Texture* th = renderText(renderer, font, "Select a Video (D-Pad to navigate, A to play)", {200, 200, 200, 255});
                if (th) {
                    int w, h;
                    SDL_QueryTexture(th, nullptr, nullptr, &w, &h);
                    SDL_Rect dst = { 20, 20, w, h };
                    SDL_RenderCopy(renderer, th, nullptr, &dst);
                    SDL_DestroyTexture(th);
                }

                // List
                int startY = 80;
                for (int i = 0; i < (int)videoFiles.size(); ++i) {
                    fs::path p = videoFiles[i];
                    std::string filename = p.filename().string();
                    
                    SDL_Color color = (i == selectedVideoIdx) ? SDL_Color{255, 200, 50, 255} : SDL_Color{150, 150, 150, 255};
                    if (i == selectedVideoIdx) {
                        filename = "> " + filename;
                    } else {
                        filename = "  " + filename;
                    }
                    
                    SDL_Texture* tv = renderText(renderer, font, filename, color);
                    if (tv) {
                        int w, h;
                        SDL_QueryTexture(tv, nullptr, nullptr, &w, &h);
                        SDL_Rect dst = { 40, startY + (i * 35), w, h };
                        SDL_RenderCopy(renderer, tv, nullptr, &dst);
                        SDL_DestroyTexture(tv);
                    }
                }
            }
            SDL_RenderPresent(renderer);
            SDL_Delay(16); // ~60fps for menu
        }
        else if (appState == AppState::Playing) {
            // Decode next frame(s)
            if (playerState.getStatus() == r36s::PlayStatus::Playing) {
                bool gotVideo = false;
                bool eof = false;

                // Decode packets until we get a video frame to present.
                // Any audio frames encountered are resampled and pushed to the sound card.
                while (!gotVideo && !eof) {
                    // Prevent audio buffer bloat if video decoding is too slow
                    if (audioDevice > 0 && SDL_GetQueuedAudioSize(audioDevice) > 176400) {
                        // Reached ~1 second of queued audio, break to let it drain 
                        // and render whatever we have.
                        break;
                    }

                    bool isVideo = false;
                    bool isAudio = false;
                    if (decoder.decodeNextFrame(isVideo, isAudio)) {
                        if (isAudio && audioDevice > 0) {
                            int audioSize = 0;
                            const uint8_t* audioData = decoder.getResampledAudio(audioSize);
                            if (audioData && audioSize > 0) {
                                SDL_QueueAudio(audioDevice, audioData, audioSize);
                            }
                        }
                        if (isVideo) {
                            AVFrame* frame = decoder.getFrame();
                            SDL_UpdateYUVTexture(
                                videoTexture, nullptr,
                                frame->data[0], frame->linesize[0],  // Y
                                frame->data[1], frame->linesize[1],  // U
                                frame->data[2], frame->linesize[2]   // V
                            );
                            gotVideo = true;
                        }
                    } else {
                        eof = true;
                    }
                }

                if (eof) {
                    // End of file — stop playback, return to menu if available
                    if (audioDevice > 0) SDL_ClearQueuedAudio(audioDevice);
                    if (videoFiles.size() > 0) {
                        decoder.close();
                        appState = AppState::Menu;
                    } else {
                        playerState.stop();
                        running = false;
                    }
                }
            }

            // Render video
            if (videoTexture) {
                SDL_RenderCopy(renderer, videoTexture, nullptr, nullptr);
            }
            SDL_RenderPresent(renderer);

            // Frame pacing
            Uint32 elapsed = SDL_GetTicks() - frameStart;
            if (elapsed < frameDelayMs) {
                SDL_Delay(frameDelayMs - elapsed);
            }
        }
    }

    // ---- Cleanup ------------------------------------------------------------
    fprintf(stderr, "[Main] Shutting down...\n");

    if (videoTexture) SDL_DestroyTexture(videoTexture);
    decoder.close();
    
    if (audioDevice > 0) SDL_CloseAudioDevice(audioDevice);

    if (font) TTF_CloseFont(font);

    if (controller) {
        SDL_GameControllerClose(controller);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    
    TTF_Quit();
    SDL_Quit();

    return 0;
}

// =============================================================================
// Main Entry Point
// =============================================================================
int main(int argc, char* argv[]) {
    // Redirect standard output and error to a log file for debugging on the R36S
    freopen("krona_player.log", "w", stderr);
    freopen("krona_player.log", "w", stdout);

    fprintf(stderr, "=== Krona Player Started ===\n");

    try {
        return runApp(argc, argv);
    } catch (const std::exception& e) {
        fprintf(stderr, "[FATAL] Unhandled std::exception: %s\n", e.what());
        return 1;
    } catch (...) {
        fprintf(stderr, "[FATAL] Unknown unhandled exception occurred!\n");
        return 1;
    }
}
