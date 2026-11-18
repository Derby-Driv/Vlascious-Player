// =============================================================================
// Vlascious Player — Video Player Header
// =============================================================================

#ifndef R36S_VIDEO_PLAYER_H
#define R36S_VIDEO_PLAYER_H

#include "decoder.h"
#include <SDL2/SDL.h>
#include <string>

namespace r36s {

class App;

class VideoPlayer {
public:
    explicit VideoPlayer(App* app);
    ~VideoPlayer();

    bool openVideo(const std::string& path);
    void closeVideo();

    void handleEvent(const SDL_Event& event);
    void updateAndRender();

private:
    void startHeldSeek(int direction);
    void stopHeldSeek(int direction);
    void updateHeldSeek();

    void loadResumePosition();
    void saveResumePosition();
    void clearResumePosition();

    App* app_;
    r36s::Decoder decoder_;
    SDL_Texture* videoTexture_;
    r36s::PlayerState playerState_;

    Uint32 frameDelayMs_;
    Uint32 frameStart_;

    // Held seeking: +1 = forward, -1 = backward, 0 = inactive.
    int seekDirection_;
    Uint32 nextHeldSeekAt_;

    // Resume playback state is saved periodically and again when closing.
    std::string currentVideoPath_;
    Uint32 lastResumeSaveAt_;
    bool playbackFinished_;
};

} // namespace r36s

#endif // R36S_VIDEO_PLAYER_H
