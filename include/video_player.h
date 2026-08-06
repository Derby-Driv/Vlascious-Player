// =============================================================================
// Krona Player — Video Player Header
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
    App* app_;
    r36s::Decoder decoder_;
    SDL_Texture* videoTexture_;
    r36s::PlayerState playerState_;
    
    Uint32 frameDelayMs_;
    Uint32 frameStart_;
};

} // namespace r36s

#endif // R36S_VIDEO_PLAYER_H
