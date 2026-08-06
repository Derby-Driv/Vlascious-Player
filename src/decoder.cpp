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
            // Pause while idle is a no-op
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
      frame_(nullptr),
      packet_(nullptr),
      videoStreamIdx_(-1),
      audioStreamIdx_(-1),
      opened_(false) {}

Decoder::~Decoder() {
    close();
}

bool Decoder::open(const std::string& filepath) {
    // Safety: close any previous session
    close();

    // Open input file
    if (avformat_open_input(&fmtCtx_, filepath.c_str(), nullptr, nullptr) < 0) {
        fprintf(stderr, "[Decoder] Failed to open file: %s\n", filepath.c_str());
        return false;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(fmtCtx_, nullptr) < 0) {
        fprintf(stderr, "[Decoder] Failed to find stream info\n");
        close();
        return false;
    }

    // Find the best video stream
    videoStreamIdx_ = av_find_best_stream(fmtCtx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIdx_ >= 0) {
        if (!openCodec(videoStreamIdx_, &videoCodecCtx_)) {
            fprintf(stderr, "[Decoder] Failed to open video codec\n");
            close();
            return false;
        }
    }

    // Find the best audio stream
    audioStreamIdx_ = av_find_best_stream(fmtCtx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIdx_ >= 0) {
        if (!openCodec(audioStreamIdx_, &audioCodecCtx_)) {
            fprintf(stderr, "[Decoder] Warning: failed to open audio codec (continuing without audio)\n");
            audioStreamIdx_ = -1;
            // Not a fatal error — video-only playback is acceptable
        }
    }

    if (videoStreamIdx_ < 0) {
        fprintf(stderr, "[Decoder] No video stream found in file\n");
        close();
        return false;
    }

    // Allocate frame and packet
    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    if (!frame_ || !packet_) {
        fprintf(stderr, "[Decoder] Failed to allocate frame/packet\n");
        close();
        return false;
    }

    opened_ = true;
    return true;
}

void Decoder::close() {
    if (packet_) {
        av_packet_free(&packet_);
        packet_ = nullptr;
    }
    if (frame_) {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }
    if (videoCodecCtx_) {
        avcodec_free_context(&videoCodecCtx_);
        videoCodecCtx_ = nullptr;
    }
    if (audioCodecCtx_) {
        avcodec_free_context(&audioCodecCtx_);
        audioCodecCtx_ = nullptr;
    }
    if (fmtCtx_) {
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
    }
    videoStreamIdx_ = -1;
    audioStreamIdx_ = -1;
    opened_ = false;
}

bool Decoder::readVideoFrame() {
    if (!opened_ || videoStreamIdx_ < 0) return false;

    while (av_read_frame(fmtCtx_, packet_) >= 0) {
        if (packet_->stream_index == videoStreamIdx_) {
            int ret = avcodec_send_packet(videoCodecCtx_, packet_);
            av_packet_unref(packet_);

            if (ret < 0) {
                fprintf(stderr, "[Decoder] Error sending video packet\n");
                return false;
            }

            ret = avcodec_receive_frame(videoCodecCtx_, frame_);
            if (ret == 0) {
                return true; // Frame decoded successfully
            }
            if (ret == AVERROR(EAGAIN)) {
                continue; // Need more packets
            }
            // EOF or error
            return false;
        }
        av_packet_unref(packet_);
    }

    return false; // End of file
}

bool Decoder::readAudioFrame() {
    if (!opened_ || audioStreamIdx_ < 0) return false;

    while (av_read_frame(fmtCtx_, packet_) >= 0) {
        if (packet_->stream_index == audioStreamIdx_) {
            int ret = avcodec_send_packet(audioCodecCtx_, packet_);
            av_packet_unref(packet_);

            if (ret < 0) {
                fprintf(stderr, "[Decoder] Error sending audio packet\n");
                return false;
            }

            ret = avcodec_receive_frame(audioCodecCtx_, frame_);
            if (ret == 0) {
                return true;
            }
            if (ret == AVERROR(EAGAIN)) {
                continue;
            }
            return false;
        }
        av_packet_unref(packet_);
    }

    return false;
}

bool Decoder::isOpen() const { return opened_; }

int Decoder::getVideoStreamIndex() const { return videoStreamIdx_; }

int Decoder::getAudioStreamIndex() const { return audioStreamIdx_; }

AVFrame* Decoder::getFrame() const { return frame_; }

int Decoder::getWidth() const {
    return videoCodecCtx_ ? videoCodecCtx_->width : 0;
}

int Decoder::getHeight() const {
    return videoCodecCtx_ ? videoCodecCtx_->height : 0;
}

AVCodecContext* Decoder::getVideoCodecCtx() const { return videoCodecCtx_; }

AVCodecContext* Decoder::getAudioCodecCtx() const { return audioCodecCtx_; }

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
bool Decoder::openCodec(int streamIndex, AVCodecContext** ctx) {
    AVStream* stream = fmtCtx_->streams[streamIndex];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "[Decoder] Unsupported codec for stream %d\n", streamIndex);
        return false;
    }

    *ctx = avcodec_alloc_context3(codec);
    if (!*ctx) {
        fprintf(stderr, "[Decoder] Failed to allocate codec context\n");
        return false;
    }

    if (avcodec_parameters_to_context(*ctx, stream->codecpar) < 0) {
        fprintf(stderr, "[Decoder] Failed to copy codec parameters\n");
        avcodec_free_context(ctx);
        return false;
    }

    if (avcodec_open2(*ctx, codec, nullptr) < 0) {
        fprintf(stderr, "[Decoder] Failed to open codec\n");
        avcodec_free_context(ctx);
        return false;
    }

    return true;
}

} // namespace r36s
