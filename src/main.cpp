// =============================================================================
// Krona Player — Main Entry Point
// =============================================================================

#include "app.h"
#include <cstdio>
#include <exception>

int main(int argc, char* argv[]) {
    // Redirect standard output and error to a log file for debugging on the R36S
    freopen("krona_player.log", "w", stderr);
    freopen("krona_player.log", "w", stdout);

    fprintf(stderr, "=== Krona Player Started ===\n");

    try {
        r36s::App app;
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        fprintf(stderr, "[FATAL] Unhandled std::exception: %s\n", e.what());
        return 1;
    } catch (...) {
        fprintf(stderr, "[FATAL] Unknown unhandled exception occurred!\n");
        return 1;
    }
}
