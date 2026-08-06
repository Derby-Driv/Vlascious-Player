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
      swrCtx_(nullptr),
      frame_(nullptr),
      packet_(nullptr),
      audioBuffer_(nullptr),
      audioBufferSize_(0),
      videoStreamIdx_(-1),
      audioStreamIdx_(-1),
      opened_(false),
      currentVideoTime_(0.0) {}

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
        } else {
            // Set up libswresample to output S16 interleaved, 2 channels, 44100 Hz
            int64_t outChLayout = AV_CH_LAYOUT_STEREO;
            AVSampleFormat outSampleFmt = AV_SAMPLE_FMT_S16;
            int outSampleRate = 44100;
            
            int64_t inChLayout = audioCodecCtx_->channel_layout;
            if (inChLayout == 0) {
                inChLayout = av_get_default_channel_layout(audioCodecCtx_->channels);
            }
            AVSampleFormat inSampleFmt = audioCodecCtx_->sample_fmt;
            int inSampleRate = audioCodecCtx_->sample_rate;
            
            swrCtx_ = swr_alloc_set_opts(nullptr, 
                                outChLayout, outSampleFmt, outSampleRate,
                                inChLayout, inSampleFmt, inSampleRate, 
                                0, nullptr);
            if (!swrCtx_ || swr_init(swrCtx_) < 0) {
                fprintf(stderr, "[Decoder] Warning: Failed to initialize swresample context\n");
                if (swrCtx_) {
                    swr_free(&swrCtx_);
                    swrCtx_ = nullptr;
                }
            } else {
                // Pre-allocate a 192KB buffer for resampled audio (plenty for 44.1kHz S16 Stereo frames)
                audioBufferSize_ = 192000;
                audioBuffer_ = (uint8_t*)av_malloc(audioBufferSize_);
            }
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
    currentVideoTime_ = 0.0;
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
    if (swrCtx_) {
        swr_free(&swrCtx_);
        swrCtx_ = nullptr;
    }
    if (audioBuffer_) {
        av_free(audioBuffer_);
        audioBuffer_ = nullptr;
    }
    if (fmtCtx_) {
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
    }
    videoStreamIdx_ = -1;
    audioStreamIdx_ = -1;
    opened_ = false;
}

bool Decoder::decodeNextFrame(bool& isVideo, bool& isAudio) {
    isVideo = false;
    isAudio = false;

    if (!opened_) return false;

    // Check if there are lingering frames in the decoders
    if (videoStreamIdx_ >= 0) {
        if (avcodec_receive_frame(videoCodecCtx_, frame_) == 0) {
            isVideo = true;
            if (frame_->pts != AV_NOPTS_VALUE) {
                currentVideoTime_ = frame_->pts * av_q2d(fmtCtx_->streams[videoStreamIdx_]->time_base);
            }
            return true;
        }
    }
    if (audioStreamIdx_ >= 0) {
        if (avcodec_receive_frame(audioCodecCtx_, frame_) == 0) {
            isAudio = true;
            return true;
        }
    }

    // Read new packets
    while (av_read_frame(fmtCtx_, packet_) >= 0) {
        if (packet_->stream_index == videoStreamIdx_) {
            if (avcodec_send_packet(videoCodecCtx_, packet_) == 0) {
                av_packet_unref(packet_);
                if (avcodec_receive_frame(videoCodecCtx_, frame_) == 0) {
                    isVideo = true;
                    if (frame_->pts != AV_NOPTS_VALUE) {
                        currentVideoTime_ = frame_->pts * av_q2d(fmtCtx_->streams[videoStreamIdx_]->time_base);
                    }
                    return true;
                }
                continue; // Need more packets
            }
        } else if (packet_->stream_index == audioStreamIdx_) {
            if (avcodec_send_packet(audioCodecCtx_, packet_) == 0) {
                av_packet_unref(packet_);
                if (avcodec_receive_frame(audioCodecCtx_, frame_) == 0) {
                    isAudio = true;
                    return true;
                }
                continue; // Need more packets
            }
        }
        av_packet_unref(packet_);
    }

    return false; // EOF
}

const uint8_t* Decoder::getResampledAudio(int& outSize) {
    outSize = 0;
    if (!swrCtx_ || !audioBuffer_ || !frame_) return nullptr;

    int maxOutSamples = av_rescale_rnd(swr_get_delay(swrCtx_, frame_->sample_rate) + frame_->nb_samples, 
                                       44100, frame_->sample_rate, AV_ROUND_UP);

    uint8_t* outBuffers[1] = { audioBuffer_ };
    int outSamples = swr_convert(swrCtx_, 
                                 outBuffers, maxOutSamples,
                                 (const uint8_t**)frame_->data, frame_->nb_samples);
    
    if (outSamples < 0) {
        return nullptr;
    }
    
    // 2 channels * 2 bytes per sample (S16)
    outSize = outSamples * 2 * 2;
    return audioBuffer_;
}

double Decoder::getCurrentTime() const {
    return currentVideoTime_;
}

bool Decoder::seekRelative(double secondsDelta) {
    if (!opened_ || videoStreamIdx_ < 0 || !fmtCtx_) return false;

    double targetTime = currentVideoTime_ + secondsDelta;
    if (targetTime < 0.0) targetTime = 0.0;
    
    // Fallback: if duration is known, don't seek past it (though av_seek_frame usually handles it)
    if (fmtCtx_->duration != AV_NOPTS_VALUE) {
        double durationSec = (double)fmtCtx_->duration / AV_TIME_BASE;
        if (targetTime > durationSec) {
            targetTime = durationSec - 1.0; // Seek near the end
            if (targetTime < 0.0) targetTime = 0.0;
        }
    }

    int64_t targetTimestamp = (int64_t)(targetTime * AV_TIME_BASE);
    int ret = av_seek_frame(fmtCtx_, -1, targetTimestamp, AVSEEK_FLAG_BACKWARD);
    
    if (ret >= 0) {
        // Must flush codec buffers to remove old data
        avcodec_flush_buffers(videoCodecCtx_);
        if (audioCodecCtx_) {
            avcodec_flush_buffers(audioCodecCtx_);
        }
        currentVideoTime_ = targetTime;
        return true;
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
