// =============================================================================
// Vlascious Player — Video Player Implementation
// =============================================================================
#include "video_player.h"
#include "app.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace r36s {
namespace {

constexpr double kSeekStepSeconds = 10.0;
constexpr Uint32 kHeldSeekRepeatMs = 500;
constexpr Uint32 kResumeSaveIntervalMs = 5000;
constexpr double kMinimumResumeSeconds = 3.0;

std::string resumeFileForVideo(const std::string& videoPath) {
    const std::filesystem::path filename =
        std::filesystem::path(videoPath).filename();
    return (std::filesystem::path(".vlascious_resume") /
            (filename.string() + ".pos")).string();
}

} // namespace

VideoPlayer::VideoPlayer(App* app)
    : app_(app),
      videoTexture_(nullptr),
      frameDelayMs_(33),
      frameStart_(0),
      seekDirection_(0),
      nextHeldSeekAt_(0),
      lastResumeSaveAt_(0),
      playbackFinished_(false) {
}

VideoPlayer::~VideoPlayer() {
    closeVideo();
}

bool VideoPlayer::openVideo(const std::string& path) {
    // Preserve the previous video's position if a new file is opened directly.
    if (decoder_.isOpen() || videoTexture_ || !currentVideoPath_.empty()) {
        closeVideo();
    }

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
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        videoW, videoH
    );
    if (!videoTexture_) {
        fprintf(stderr, "[VideoPlayer] SDL_CreateTexture failed: %s\n", SDL_GetError());
        decoder_.close();
        return false;
    }

    currentVideoPath_ = path;
    playbackFinished_ = false;
    seekDirection_ = 0;
    lastResumeSaveAt_ = SDL_GetTicks();

    // Restore this particular movie's last saved position, if one exists.
    loadResumePosition();

    SDL_AudioDeviceID audioDevice = app_->getAudioDevice();
    if (audioDevice > 0) {
        SDL_ClearQueuedAudio(audioDevice);
        SDL_PauseAudioDevice(audioDevice, 0);
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
    if (!currentVideoPath_.empty() && decoder_.isOpen()) {
        if (playbackFinished_) {
            // A naturally completed movie should start from the beginning next time.
            clearResumePosition();
        } else {
            // Save on menu return, quit, and normal shutdown.
            saveResumePosition();
        }
    }

    seekDirection_ = 0;

    if (videoTexture_) {
        SDL_DestroyTexture(videoTexture_);
        videoTexture_ = nullptr;
    }

    decoder_.close();

    SDL_AudioDeviceID audioDevice = app_->getAudioDevice();
    if (audioDevice > 0) {
        SDL_ClearQueuedAudio(audioDevice);
        SDL_PauseAudioDevice(audioDevice, 0);
    }

    currentVideoPath_.clear();
    playbackFinished_ = false;
}

void VideoPlayer::startHeldSeek(int direction) {
    if (!decoder_.isOpen() || (direction != -1 && direction != 1)) {
        return;
    }

    seekDirection_ = direction;

    // Keep the familiar immediate 10-second jump on the initial press.
    if (decoder_.seekRelative(direction * kSeekStepSeconds)) {
        SDL_AudioDeviceID audioDevice = app_->getAudioDevice();
        if (audioDevice > 0) {
            SDL_ClearQueuedAudio(audioDevice);
        }
    }

    // While held, repeat the same 10-second jump twice per second.
    nextHeldSeekAt_ = SDL_GetTicks() + kHeldSeekRepeatMs;
}

void VideoPlayer::stopHeldSeek(int direction) {
    if (seekDirection_ == direction) {
        seekDirection_ = 0;
    }
}

void VideoPlayer::updateHeldSeek() {
    if (seekDirection_ == 0 || !decoder_.isOpen()) {
        return;
    }

    const Uint32 now = SDL_GetTicks();
    if (static_cast<Sint32>(now - nextHeldSeekAt_) < 0) {
        return;
    }

    if (decoder_.seekRelative(seekDirection_ * kSeekStepSeconds)) {
        SDL_AudioDeviceID audioDevice = app_->getAudioDevice();
        if (audioDevice > 0) {
            SDL_ClearQueuedAudio(audioDevice);
        }
    }

    nextHeldSeekAt_ = now + kHeldSeekRepeatMs;
}

void VideoPlayer::loadResumePosition() {
    if (currentVideoPath_.empty()) {
        return;
    }

    const std::string resumePath = resumeFileForVideo(currentVideoPath_);
    std::ifstream input(resumePath);
    double resumeSeconds = 0.0;

    if (!(input >> resumeSeconds)) {
        return;
    }

    if (!std::isfinite(resumeSeconds) || resumeSeconds < kMinimumResumeSeconds) {
        return;
    }

    if (decoder_.seekRelative(resumeSeconds)) {
        fprintf(stderr, "[VideoPlayer] Resuming %s at %.1f seconds\n",
                currentVideoPath_.c_str(), resumeSeconds);
    } else {
        fprintf(stderr, "[VideoPlayer] Could not restore saved playback position\n");
    }
}

void VideoPlayer::saveResumePosition() {
    if (currentVideoPath_.empty() || !decoder_.isOpen()) {
        return;
    }

    const double position = decoder_.getCurrentTime();
    const std::string resumePath = resumeFileForVideo(currentVideoPath_);

    // Returning to the opening few seconds should clear an old resume point.
    if (!std::isfinite(position) || position < kMinimumResumeSeconds) {
        std::remove(resumePath.c_str());
        return;
    }

    try {
        std::filesystem::create_directories(".vlascious_resume");
    } catch (const std::filesystem::filesystem_error& error) {
        fprintf(stderr, "[VideoPlayer] Could not create resume folder: %s\n",
                error.what());
        return;
    }

    const std::string temporaryPath = resumePath + ".tmp";
    {
        std::ofstream output(temporaryPath, std::ios::out | std::ios::trunc);
        if (!output) {
            fprintf(stderr, "[VideoPlayer] Could not write resume position\n");
            return;
        }
        output << std::fixed << std::setprecision(3) << position << '\n';
        output.close();
        if (!output) {
            std::remove(temporaryPath.c_str());
            fprintf(stderr, "[VideoPlayer] Failed while saving resume position\n");
            return;
        }
    }

    // rename() replaces the previous record atomically on the ArkOS filesystem.
    if (std::rename(temporaryPath.c_str(), resumePath.c_str()) != 0) {
        std::remove(temporaryPath.c_str());
        fprintf(stderr, "[VideoPlayer] Could not commit resume position\n");
    }
}

void VideoPlayer::clearResumePosition() {
    if (currentVideoPath_.empty()) {
        return;
    }
    const std::string resumePath = resumeFileForVideo(currentVideoPath_);
    std::remove(resumePath.c_str());
    std::remove((resumePath + ".tmp").c_str());
}

void VideoPlayer::handleEvent(const SDL_Event& event) {
    SDL_AudioDeviceID audioDevice = app_->getAudioDevice();

    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
            app_->returnToMenu();
        } else if (event.key.keysym.sym == SDLK_SPACE ||
                   event.key.keysym.sym == SDLK_RETURN) {
            playerState_.togglePause();
            if (audioDevice > 0) {
                SDL_PauseAudioDevice(audioDevice, playerState_.isPaused() ? 1 : 0);
            }
        } else if (event.key.keysym.sym == SDLK_RIGHT && !event.key.repeat) {
            startHeldSeek(1);
        } else if (event.key.keysym.sym == SDLK_LEFT && !event.key.repeat) {
            startHeldSeek(-1);
        }
    } else if (event.type == SDL_KEYUP) {
        if (event.key.keysym.sym == SDLK_RIGHT) {
            stopHeldSeek(1);
        } else if (event.key.keysym.sym == SDLK_LEFT) {
            stopHeldSeek(-1);
        }
    } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START ||
            event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
            app_->returnToMenu();
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
            playerState_.togglePause();
            if (audioDevice > 0) {
                SDL_PauseAudioDevice(audioDevice, playerState_.isPaused() ? 1 : 0);
            }
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            startHeldSeek(1);
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
            startHeldSeek(-1);
        }
    } else if (event.type == SDL_CONTROLLERBUTTONUP) {
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            stopHeldSeek(1);
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
            stopHeldSeek(-1);
        }
    }
}

void VideoPlayer::updateAndRender() {
    frameStart_ = SDL_GetTicks();
    updateHeldSeek();

    // Save progress every five seconds so a sudden shutdown loses very little.
    const Uint32 now = SDL_GetTicks();
    if (now - lastResumeSaveAt_ >= kResumeSaveIntervalMs) {
        saveResumePosition();
        lastResumeSaveAt_ = now;
    }

    SDL_AudioDeviceID audioDevice = app_->getAudioDevice();
    bool audioClockActive = false;

    if (playerState_.getStatus() == r36s::PlayStatus::Playing) {
        bool gotVideo = false;
        bool eof = false;
        while (!gotVideo && !eof) {
            // Keep the SDL audio queue bounded to about two seconds.
            if (audioDevice > 0 &&
                SDL_GetQueuedAudioSize(audioDevice) > 176400) {
                break;
            }

            bool isVideo = false;
            bool isAudio = false;
            if (decoder_.decodeNextFrame(isVideo, isAudio)) {
                if (isAudio && audioDevice > 0) {
                    int audioSize = 0;
                    const uint8_t* audioData =
                        decoder_.getResampledAudio(audioSize);
                    if (audioData && audioSize > 0) {
                        SDL_QueueAudio(audioDevice, audioData, audioSize);
                    }
                }

                if (isVideo) {
                    AVFrame* frame = decoder_.getFrame();
                    // Use audio playback as the master clock. Audio timestamps
                    // plus SDL's remaining queued bytes give the current audio
                    // playback position. Hold a future video frame briefly
                    // until the audio clock reaches its presentation timestamp.
                    const double audioEndTime = decoder_.getAudioEndTime();
                    if (audioDevice > 0 && audioEndTime >= 0.0) {
                        const Uint32 initialQueuedBytes =
                            SDL_GetQueuedAudioSize(audioDevice);
                        audioClockActive = (initialQueuedBytes > 0);
                        if (audioClockActive) {
                            const double videoTime = decoder_.getCurrentTime();
                            const Uint32 syncStart = SDL_GetTicks();
                            while (true) {
                                const Uint32 queuedBytes =
                                    SDL_GetQueuedAudioSize(audioDevice);
                                const double audioClock = audioEndTime -
                                    (static_cast<double>(queuedBytes) / 176400.0);
                                const double videoLead = videoTime - audioClock;
                                if (videoLead <= 0.015 || queuedBytes == 0) {
                                    break;
                                }

                                // Never block input/rendering indefinitely if
                                // the audio device stops consuming its queue.
                                if (SDL_GetTicks() - syncStart >= 750) {
                                    break;
                                }
                                Uint32 waitMs = static_cast<Uint32>(videoLead * 1000.0);
                                if (waitMs > 10) waitMs = 10;
                                if (waitMs < 1) waitMs = 1;
                                SDL_Delay(waitMs);
                            }
                        }
                    }

                    SDL_UpdateYUVTexture(
                        videoTexture_,
                        nullptr,
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
            playbackFinished_ = true;
            app_->returnToMenu();
            return;
        }
    }

    // -------------------------------------------------------------------------
    // Aspect-ratio-preserving video rendering
    // -------------------------------------------------------------------------
    SDL_Renderer* renderer = app_->getRenderer();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (videoTexture_) {
        int screenW = 0;
        int screenH = 0;
        const int videoW = decoder_.getWidth();
        const int videoH = decoder_.getHeight();
        if (videoW > 0 && videoH > 0 &&
            SDL_GetRendererOutputSize(renderer, &screenW, &screenH) == 0 &&
            screenW > 0 && screenH > 0) {
            const double scaleW = static_cast<double>(screenW) / videoW;
            const double scaleH = static_cast<double>(screenH) / videoH;
            const double scale = (scaleW < scaleH) ? scaleW : scaleH;

            SDL_Rect dstRect;
            dstRect.w = static_cast<int>(videoW * scale);
            dstRect.h = static_cast<int>(videoH * scale);
            dstRect.x = (screenW - dstRect.w) / 2;
            dstRect.y = (screenH - dstRect.h) / 2;

            SDL_RenderCopy(renderer, videoTexture_, nullptr, &dstRect);
        }
    }

    SDL_RenderPresent(renderer);

    // When audio is present, timestamps drive the video clock. Keep the old
    // frame-rate timer only as a fallback for silent videos / audio failure.
    if (!audioClockActive) {
        const Uint32 elapsed = SDL_GetTicks() - frameStart_;
        if (elapsed < frameDelayMs_) {
            SDL_Delay(frameDelayMs_ - elapsed);
        }
    }
}

} // namespace r36s
