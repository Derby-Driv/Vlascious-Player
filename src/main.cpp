// =============================================================================
// Krona Player — Main Entry Point
// =============================================================================
// Initialises SDL2 (Video + Audio), opens an MP4 file via the Decoder, and
// runs the main event / render loop.
//
// Usage:  ./krona_player <path_to_video.mp4>
//
// On the R36S, this is launched by Krona_Player.sh which sets up the correct
// environment variables (SDL_AUDIODRIVER, SDL_NOMOUSE, etc.).
// =============================================================================

#include "decoder.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr int R36S_SCREEN_W = 640;
static constexpr int R36S_SCREEN_H = 480;

// ---------------------------------------------------------------------------
// Print usage and exit
// ---------------------------------------------------------------------------
static void usage(const char* progname) {
    fprintf(stderr, "Usage: %s <video_file.mp4>\n", progname);
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const std::string videoPath = argv[1];

    // ---- SDL2 Initialisation ------------------------------------------------
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "[Main] SDL_Init failed: %s\n", SDL_GetError());
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
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        // Fallback to software renderer
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "[Main] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Hide cursor (irrelevant on R36S but tidy for desktop testing)
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

    // ---- Decoder Initialisation ---------------------------------------------
    r36s::Decoder decoder;
    if (!decoder.open(videoPath)) {
        fprintf(stderr, "[Main] Failed to open video: %s\n", videoPath.c_str());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const int videoW = decoder.getWidth();
    const int videoH = decoder.getHeight();
    fprintf(stderr, "[Main] Video resolution: %dx%d\n", videoW, videoH);

    // Create YUV texture matching the video dimensions
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_IYUV, // YUV420P
        SDL_TEXTUREACCESS_STREAMING,
        videoW, videoH
    );
    if (!texture) {
        fprintf(stderr, "[Main] SDL_CreateTexture failed: %s\n", SDL_GetError());
        decoder.close();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // ---- Player State -------------------------------------------------------
    r36s::PlayerState playerState;
    playerState.play();

    // ---- Compute frame delay from video stream FPS --------------------------
    AVCodecContext* vctx = decoder.getVideoCodecCtx();
    double fps = 30.0; // fallback
    if (vctx && vctx->framerate.num > 0 && vctx->framerate.den > 0) {
        fps = static_cast<double>(vctx->framerate.num) / vctx->framerate.den;
    }
    const Uint32 frameDelayMs = static_cast<Uint32>(1000.0 / fps);

    // ---- Main Loop ----------------------------------------------------------
    bool running = true;
    SDL_Event event;

    while (running) {
        Uint32 frameStart = SDL_GetTicks();

        // ---- Event handling -------------------------------------------------
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                        case SDLK_q:
                            running = false;
                            break;
                        case SDLK_SPACE:
                        case SDLK_RETURN:
                            playerState.togglePause();
                            break;
                        default:
                            break;
                    }
                    break;

                case SDL_CONTROLLERBUTTONDOWN:
                    switch (event.cbutton.button) {
                        case SDL_CONTROLLER_BUTTON_START:
                            // START on R36S → Quit
                            running = false;
                            break;
                        case SDL_CONTROLLER_BUTTON_A:
                            // A on R36S → Play/Pause
                            playerState.togglePause();
                            break;
                        default:
                            break;
                    }
                    break;

                default:
                    break;
            }
        }

        // ---- Decode & render (only when playing) ----------------------------
        if (playerState.getStatus() == r36s::PlayStatus::Playing) {
            if (decoder.readVideoFrame()) {
                AVFrame* frame = decoder.getFrame();

                // Upload YUV planes to the SDL texture
                SDL_UpdateYUVTexture(
                    texture, nullptr,
                    frame->data[0], frame->linesize[0],  // Y
                    frame->data[1], frame->linesize[1],  // U
                    frame->data[2], frame->linesize[2]   // V
                );
            } else {
                // End of file — stop playback
                playerState.stop();
                running = false;
            }
        }

        // ---- Render ----------------------------------------------------------
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // ---- Frame pacing ----------------------------------------------------
        Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < frameDelayMs) {
            SDL_Delay(frameDelayMs - elapsed);
        }
    }

    // ---- Cleanup ------------------------------------------------------------
    fprintf(stderr, "[Main] Shutting down...\n");

    SDL_DestroyTexture(texture);
    decoder.close();

    if (controller) {
        SDL_GameControllerClose(controller);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
