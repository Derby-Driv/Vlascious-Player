// =============================================================================
// Krona Player — Menu Header
// =============================================================================

#ifndef R36S_MENU_H
#define R36S_MENU_H

#include <vector>
#include <string>
#include <SDL2/SDL.h>

namespace r36s {

class App;

class Menu {
public:
    explicit Menu(App* app);
    ~Menu() = default;

    void refreshVideoList();
    void handleEvent(const SDL_Event& event);
    void render();

private:
    std::vector<std::string> findVideos(const std::string& dirPath) const;
    SDL_Texture* renderText(const std::string& text, SDL_Color color) const;

    App* app_;
    std::vector<std::string> videoFiles_;
    int selectedVideoIdx_;
};

} // namespace r36s

#endif // R36S_MENU_H
