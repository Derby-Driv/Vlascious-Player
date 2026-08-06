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

    // Replaces the old split methods with a single read loop.
    // Returns true if a frame was successfully read.
    // isVideo and isAudio will indicate which type of frame was decoded.
    bool decodeNextFrame(bool& isVideo, bool& isAudio);

    // Audio extraction
    // Returns a pointer to the resampled audio (S16 44100Hz Stereo)
    // and sets outSize to the byte size of the data. Returns nullptr if no data.
    const uint8_t* getResampledAudio(int& outSize);

    /// Returns true if a file is currently open.
    bool isOpen() const;

    /// Index of the best video stream, or -1 if none.
    int getVideoStreamIndex() const;

    /// Index of the best audio stream, or -1 if none.
    int getAudioStreamIndex() const;

    /// The most recently decoded video frame (valid after readVideoFrame() == true).
    AVFrame* getFrame() const;

    /// Video dimensions (valid after open()).
    int getWidth() const;
    int getHeight() const;

    /// Codec context accessors (for audio resampling setup, etc.)
    AVCodecContext* getVideoCodecCtx() const;
    AVCodecContext* getAudioCodecCtx() const;

private:
    bool openCodec(int streamIndex, AVCodecContext** ctx);

    AVFormatContext* fmtCtx_;
    AVCodecContext*  videoCodecCtx_;
    AVCodecContext*  audioCodecCtx_;
    SwrContext*      swrCtx_;
    AVFrame*         frame_;
    AVPacket*        packet_;
    uint8_t*         audioBuffer_;
    int              audioBufferSize_;

    int videoStreamIdx_;
    int audioStreamIdx_;
    bool opened_;
};

} // namespace r36s

#endif // R36S_DECODER_H
