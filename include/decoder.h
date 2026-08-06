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

    /// Read and decode the next video frame.
    /// Returns true if a frame was decoded; the frame data is in `getFrame()`.
    bool readVideoFrame();

    /// Read and decode the next audio frame.
    /// Returns true if a frame was decoded.
    bool readAudioFrame();

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
    AVFrame*         frame_;
    AVPacket*        packet_;

    int videoStreamIdx_;
    int audioStreamIdx_;
    bool opened_;
};

} // namespace r36s

#endif // R36S_DECODER_H
