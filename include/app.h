// =============================================================================
// Krona Player — Application Core Header
// =============================================================================

#ifndef R36S_APP_H
#define R36S_APP_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <memory>

namespace r36s {

class Menu;
class VideoPlayer;

enum class AppState {
    Menu,
    Playing
};

class App {
public:
    App();
    ~App();

    int run(int argc, char* argv[]);

    void changeState(AppState newState);
    void playVideo(const std::string& path);
    void returnToMenu();
    void quit();

    SDL_Renderer* getRenderer() const { return renderer_; }
    TTF_Font* getFont() const { return font_; }
    SDL_AudioDeviceID getAudioDevice() const { return audioDevice_; }

private:
    bool initSDL();
    void cleanupSDL();
    bool loadFont();

    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_AudioDeviceID audioDevice_;
    SDL_GameController* controller_;
    TTF_Font* font_;

    AppState appState_;
    bool running_;

    std::unique_ptr<Menu> menu_;
    std::unique_ptr<VideoPlayer> videoPlayer_;
};

} // namespace r36s

#endif // R36S_APP_H
