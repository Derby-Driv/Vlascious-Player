// =============================================================================
// Krona Player — Menu Implementation
// =============================================================================

#include "menu.h"
#include "app.h"

#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace r36s {

static constexpr int R36S_SCREEN_W = 640;
static constexpr int R36S_SCREEN_H = 480;

Menu::Menu(App* app) : app_(app), selectedVideoIdx_(0) {}

std::vector<std::string> Menu::findVideos(const std::string& dirPath) const {
    std::vector<std::string> videos;
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        return videos;
    }
    
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".mp4") {
                videos.push_back(entry.path().string());
            }
        }
    }
    std::sort(videos.begin(), videos.end());
    return videos;
}

void Menu::refreshVideoList() {
    videoFiles_ = findVideos("videos");
    if (videoFiles_.empty()) {
        videoFiles_ = findVideos(".");
    }
    selectedVideoIdx_ = 0;

    // Auto-play fallback if no font is available but videos exist
    if (!app_->getFont() && !videoFiles_.empty()) {
        app_->playVideo(videoFiles_[0]);
    }
}

void Menu::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
            app_->quit();
        } else if (event.key.keysym.sym == SDLK_UP) {
            selectedVideoIdx_ = std::max(0, selectedVideoIdx_ - 1);
        } else if (event.key.keysym.sym == SDLK_DOWN) {
            selectedVideoIdx_ = std::min((int)videoFiles_.size() - 1, selectedVideoIdx_ + 1);
        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) {
            if (!videoFiles_.empty()) {
                app_->playVideo(videoFiles_[selectedVideoIdx_]);
            }
        }
    } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
            app_->quit();
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
            selectedVideoIdx_ = std::max(0, selectedVideoIdx_ - 1);
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
            selectedVideoIdx_ = std::min((int)videoFiles_.size() - 1, selectedVideoIdx_ + 1);
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
            if (!videoFiles_.empty()) {
                app_->playVideo(videoFiles_[selectedVideoIdx_]);
            }
        }
    }
}

SDL_Texture* Menu::renderText(const std::string& text, SDL_Color color) const {
    if (text.empty() || !app_->getFont()) return nullptr;
    SDL_Surface* surface = TTF_RenderText_Blended(app_->getFont(), text.c_str(), color);
    if (!surface) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(app_->getRenderer(), surface);
    SDL_FreeSurface(surface);
    return texture;
}

void Menu::render() {
    SDL_Renderer* renderer = app_->getRenderer();

    if (!app_->getFont()) {
        // Render nothing if font missing
    } else if (videoFiles_.empty()) {
        SDL_Texture* t = renderText("No .mp4 files found in videos/", {255, 100, 100, 255});
        if (t) {
            int w, h;
            SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
            SDL_Rect dst = { (R36S_SCREEN_W - w)/2, (R36S_SCREEN_H - h)/2, w, h };
            SDL_RenderCopy(renderer, t, nullptr, &dst);
            SDL_DestroyTexture(t);
        }
    } else {
        SDL_Texture* th = renderText("Select a Video (D-Pad to navigate, A to play)", {200, 200, 200, 255});
        if (th) {
            int w, h;
            SDL_QueryTexture(th, nullptr, nullptr, &w, &h);
            SDL_Rect dst = { 20, 20, w, h };
            SDL_RenderCopy(renderer, th, nullptr, &dst);
            SDL_DestroyTexture(th);
        }

        int startY = 80;
        for (int i = 0; i < (int)videoFiles_.size(); ++i) {
            fs::path p = videoFiles_[i];
            std::string filename = p.filename().string();
            
            SDL_Color color = (i == selectedVideoIdx_) ? SDL_Color{255, 200, 50, 255} : SDL_Color{150, 150, 150, 255};
            if (i == selectedVideoIdx_) {
                filename = "> " + filename;
            } else {
                filename = "  " + filename;
            }
            
            SDL_Texture* tv = renderText(filename, color);
            if (tv) {
                int w, h;
                SDL_QueryTexture(tv, nullptr, nullptr, &w, &h);
                SDL_Rect dst = { 40, startY + (i * 35), w, h };
                SDL_RenderCopy(renderer, tv, nullptr, &dst);
                SDL_DestroyTexture(tv);
            }
        }
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(16); // ~60fps for menu
}

} // namespace r36s
