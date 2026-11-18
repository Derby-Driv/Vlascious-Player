// =============================================================================
// Krona Player — Decoder Implementation
// =============================================================================
#include "decoder.h"
#include <cstdio>

namespace r36s {

// =============================================================================
// PlayerState
// =============================================================================
PlayerState::PlayerState() : status_(PlayStatus::Idle) {}
PlayStatus PlayerState::getStatus() const { return status_; }

bool PlayerState::isPaused() const { return status_ == PlayStatus::Paused; }

void PlayerState::play() {
    status_ = PlayStatus::Playing;
}

void PlayerState::togglePause() {
    switch (status_) {
        case PlayStatus::Playing:
            status_ = PlayStatus::Paused;
            break;
        case PlayStatus::Paused:
            status_ = PlayStatus::Playing;
            break;
        case PlayStatus::Idle:
            break;
    }
}
void PlayerState::stop() {
    status_ = PlayStatus::Idle;
}
// =============================================================================
// Decoder
// =============================================================================
Decoder::Decoder()
    : fmtCtx_(nullptr),
      videoCodecCtx_(nullptr),
      audioCodecCtx_(nullptr),
      swrCtx_(nullptr),
      frame_(nullptr),
      packet_(nullptr),
      audioBuffer_(nullptr),
      audioBufferSize_(0),
      videoStreamIdx_(-1),
      audioStreamIdx_(-1),
      opened_(false),
      currentVideoTime_(0.0),
      currentAudioEndTime_(-1.0) {}
Decoder::~Decoder() {
    close();
}

bool Decoder::open(const std::string& filepath) {
    close();

    if (avformat_open_input(&fmtCtx_, filepath.c_str(), nullptr, nullptr) < 0) {
        fprintf(stderr, "[Decoder] Failed to open file: %s\n", filepath.c_str());
        return false;
    }

    if (avformat_find_stream_info(fmtCtx_, nullptr) < 0) {
        fprintf(stderr, "[Decoder] Failed to find stream info\n");
        close();
        return false;
    }
    videoStreamIdx_ = av_find_best_stream(
        fmtCtx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIdx_ >= 0) {
        if (!openCodec(videoStreamIdx_, &videoCodecCtx_)) {
            fprintf(stderr, "[Decoder] Failed to open video codec\n");
            close();
            return false;
        }
    }
    audioStreamIdx_ = av_find_best_stream(
        fmtCtx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIdx_ >= 0) {
        if (!openCodec(audioStreamIdx_, &audioCodecCtx_)) {
            fprintf(stderr, "[Decoder] Warning: failed to open audio codec (continuing without audio)\n");
            audioStreamIdx_ = -1;
        } else {
            int64_t outChLayout = AV_CH_LAYOUT_STEREO;
            AVSampleFormat outSampleFmt = AV_SAMPLE_FMT_S16;
            int outSampleRate = 44100;
            int64_t inChLayout = audioCodecCtx_->channel_layout;
            if (inChLayout == 0) {
                inChLayout = av_get_default_channel_layout(audioCodecCtx_->channels);
            }

            swrCtx_ = swr_alloc_set_opts(
                nullptr,
                outChLayout, outSampleFmt, outSampleRate,
