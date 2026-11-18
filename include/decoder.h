// =============================================================================
// Krona Player — Decoder Header
// =============================================================================
// Public API for the FFmpeg decoder wrapper and the player state machine.
// =============================================================================
#ifndef R36S_DECODER_H
#define R36S_DECODER_H
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#include <string>

namespace r36s {
// =============================================================================
// PlayStatus — simple state enum for the player
// =============================================================================
enum class PlayStatus {
    Idle,     // No file loaded or playback stopped
    Playing,  // Actively decoding & rendering
    Paused,   // Playback paused; frame frozen
};
// =============================================================================
// PlayerState — lightweight state machine (testable without SDL)
// =============================================================================
class PlayerState {
public:
    PlayerState();

    PlayStatus getStatus() const;
    bool isPaused() const;

    void play();
    void togglePause();
    void stop();

private:
    PlayStatus status_;
};
// =============================================================================
// Decoder — FFmpeg wrapper
// =============================================================================
class Decoder {
public:
    Decoder();
    ~Decoder();

    // Non-copyable
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    /// Open an MP4 file. Returns true on success.
    bool open(const std::string& filepath);

    /// Release all FFmpeg resources.
    void close();
    // Returns true if a frame was successfully read.
    // isVideo and isAudio indicate which type of frame was decoded.
    bool decodeNextFrame(bool& isVideo, bool& isAudio);

    // Audio extraction. Output is S16, 44100 Hz, stereo.
    const uint8_t* getResampledAudio(int& outSize);

    /// Current video presentation time in seconds.
    double getCurrentTime() const;
    /// Total media duration in seconds, or -1.0 when unavailable.
    double getDuration() const;
    /// End timestamp of the most recently decoded/resampled audio block.
    /// Returns -1.0 until an audio timestamp is available.
    double getAudioEndTime() const;

    /// Seeks the video by a relative amount in seconds.
    bool seekRelative(double secondsDelta);
    bool isOpen() const;
    int getVideoStreamIndex() const;
    int getAudioStreamIndex() const;
    AVFrame* getFrame() const;
    int getWidth() const;
    int getHeight() const;
    AVCodecContext* getVideoCodecCtx() const;
    AVCodecContext* getAudioCodecCtx() const;

private:
    bool openCodec(int streamIndex, AVCodecContext** ctx);
    AVFormatContext* fmtCtx_;
    AVCodecContext* videoCodecCtx_;
    AVCodecContext* audioCodecCtx_;
    SwrContext* swrCtx_;
    AVFrame* frame_;
    AVPacket* packet_;
    uint8_t* audioBuffer_;
    int audioBufferSize_;

    int videoStreamIdx_;
    int audioStreamIdx_;
    bool opened_;
    double currentVideoTime_;
    double currentAudioEndTime_;
};

} // namespace r36s
#endif // R36S_DECODER_H
