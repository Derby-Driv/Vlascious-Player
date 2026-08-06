// =============================================================================
// Krona Player — Video Player Implementation
// =============================================================================

#include "video_player.h"
#include "app.h"

#include <cstdio>

namespace r36s {

VideoPlayer::VideoPlayer(App* app) 
    : app_(app), videoTexture_(nullptr), frameDelayMs_(33), frameStart_(0) {
}

VideoPlayer::~VideoPlayer() {
    closeVideo();
}

bool VideoPlayer::openVideo(const std::string& path) {
    if (!decoder_.open(path)) {
        fprintf(stderr, "[VideoPlayer] Failed to open video: %s\n", path.c_str());
        return false;
    }
    
    const int videoW = decoder_.getWidth();
    const int videoH = decoder_.getHeight();
    fprintf(stderr, "[VideoPlayer] Video resolution: %dx%d\n", videoW, videoH);

    if (videoTexture_) {
        SDL_DestroyTexture(videoTexture_);
        videoTexture_ = nullptr;
    }
    
    videoTexture_ = SDL_CreateTexture(
        app_->getRenderer(),
        SDL_PIXELFORMAT_IYUV, // YUV420P
        SDL_TEXTUREACCESS_STREAMING,
        videoW, videoH
    );
    
    if (!videoTexture_) {
        fprintf(stderr, "[VideoPlayer] SDL_CreateTexture failed: %s\n", SDL_GetError());
        decoder_.close();
        return false;
    }
    
    playerState_.play();
    
    AVCodecContext* vctx = decoder_.getVideoCodecCtx();
    double fps = 30.0;
    if (vctx && vctx->framerate.num > 0 && vctx->framerate.den > 0) {
        fps = static_cast<double>(vctx->framerate.num) / vctx->framerate.den;
    }
    frameDelayMs_ = static_cast<Uint32>(1000.0 / fps);
    frameStart_ = SDL_GetTicks();
    
    return true;
}

void VideoPlayer::closeVideo() {
    if (videoTexture_) {
        SDL_DestroyTexture(videoTexture_);
        videoTexture_ = nullptr;
    }
    decoder_.close();
    
    SDL_AudioDeviceID audioDevice = app_->getAudioDevice();
    if (audioDevice > 0) {
        SDL_ClearQueuedAudio(audioDevice);
    }
}

void VideoPlayer::handleEvent(const SDL_Event& event) {
    SDL_AudioDeviceID audioDevice = app_->getAudioDevice();

    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
            app_->returnToMenu();
        } else if (event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_RETURN) {
            playerState_.togglePause();
        } else if (event.key.keysym.sym == SDLK_RIGHT) {
            decoder_.seekRelative(10.0);
            if (audioDevice > 0) SDL_ClearQueuedAudio(audioDevice);
        } else if (event.key.keysym.sym == SDLK_LEFT) {
            decoder_.seekRelative(-10.0);
            if (audioDevice > 0) SDL_ClearQueuedAudio(audioDevice);
        }
    } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START || event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
            app_->returnToMenu();
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
            playerState_.togglePause();
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            decoder_.seekRelative(10.0);
            if (audioDevice > 0) SDL_ClearQueuedAudio(audioDevice);
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
            decoder_.seekRelative(-10.0);
            if (audioDevice > 0) SDL_ClearQueuedAudio(audioDevice);
        }
    }
}

void VideoPlayer::updateAndRender() {
    frameStart_ = SDL_GetTicks();
    SDL_AudioDeviceID audioDevice = app_->getAudioDevice();

    if (playerState_.getStatus() == r36s::PlayStatus::Playing) {
        bool gotVideo = false;
        bool eof = false;

        while (!gotVideo && !eof) {
            // Prevent audio buffer bloat
            if (audioDevice > 0 && SDL_GetQueuedAudioSize(audioDevice) > 176400) {
                break;
            }

            bool isVideo = false;
            bool isAudio = false;
            if (decoder_.decodeNextFrame(isVideo, isAudio)) {
                if (isAudio && audioDevice > 0) {
                    int audioSize = 0;
                    const uint8_t* audioData = decoder_.getResampledAudio(audioSize);
                    if (audioData && audioSize > 0) {
                        SDL_QueueAudio(audioDevice, audioData, audioSize);
                    }
                }
                if (isVideo) {
                    AVFrame* frame = decoder_.getFrame();
                    SDL_UpdateYUVTexture(
                        videoTexture_, nullptr,
                        frame->data[0], frame->linesize[0],
                        frame->data[1], frame->linesize[1],
                        frame->data[2], frame->linesize[2]
                    );
                    gotVideo = true;
                }
            } else {
                eof = true;
            }
        }

        if (eof) {
            app_->returnToMenu();
            return;
        }
    }

    if (videoTexture_) {
        SDL_RenderCopy(app_->getRenderer(), videoTexture_, nullptr, nullptr);
    }
    SDL_RenderPresent(app_->getRenderer());

    Uint32 elapsed = SDL_GetTicks() - frameStart_;
    if (elapsed < frameDelayMs_) {
        SDL_Delay(frameDelayMs_ - elapsed);
    }
}

} // namespace r36s
