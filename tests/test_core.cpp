// =============================================================================
// Krona Player — Core Unit Tests
// =============================================================================
// Tests the core logic layer (Decoder state machine) without requiring a
// real display or audio device.  SDL2 is NOT initialised in these tests.
// =============================================================================

#include <gtest/gtest.h>
#include "decoder.h"

using namespace r36s;

// ---------------------------------------------------------------------------
// Decoder — construction / destruction safety
// ---------------------------------------------------------------------------
TEST(DecoderTest, DefaultConstructionYieldsInvalidState) {
    Decoder dec;
    EXPECT_FALSE(dec.isOpen());
    EXPECT_EQ(dec.getVideoStreamIndex(), -1);
    EXPECT_EQ(dec.getAudioStreamIndex(), -1);
}

TEST(DecoderTest, OpenNonExistentFileReturnsFalse) {
    Decoder dec;
    bool ok = dec.open("/tmp/__nonexistent_file_12345__.mp4");
    EXPECT_FALSE(ok);
    EXPECT_FALSE(dec.isOpen());
}

TEST(DecoderTest, CloseOnUnopenedDecoderIsSafe) {
    Decoder dec;
    // Must not crash / throw
    dec.close();
    EXPECT_FALSE(dec.isOpen());
}

// ---------------------------------------------------------------------------
// Player State Machine
// ---------------------------------------------------------------------------
TEST(PlayerStateTest, InitialStateIsIdle) {
    PlayerState state;
    EXPECT_EQ(state.getStatus(), PlayStatus::Idle);
    EXPECT_FALSE(state.isPaused());
}

TEST(PlayerStateTest, TransitionToPlaying) {
    PlayerState state;
    state.play();
    EXPECT_EQ(state.getStatus(), PlayStatus::Playing);
    EXPECT_FALSE(state.isPaused());
}

TEST(PlayerStateTest, PauseWhilePlaying) {
    PlayerState state;
    state.play();
    state.togglePause();
    EXPECT_EQ(state.getStatus(), PlayStatus::Paused);
    EXPECT_TRUE(state.isPaused());
}

TEST(PlayerStateTest, ResumeAfterPause) {
    PlayerState state;
    state.play();
    state.togglePause(); // pause
    state.togglePause(); // resume
    EXPECT_EQ(state.getStatus(), PlayStatus::Playing);
    EXPECT_FALSE(state.isPaused());
}

TEST(PlayerStateTest, StopResetsToIdle) {
    PlayerState state;
    state.play();
    state.togglePause();
    state.stop();
    EXPECT_EQ(state.getStatus(), PlayStatus::Idle);
    EXPECT_FALSE(state.isPaused());
}

TEST(PlayerStateTest, PauseWhileIdleIsNoop) {
    PlayerState state;
    state.togglePause();
    // Should remain idle — pause only makes sense while playing
    EXPECT_EQ(state.getStatus(), PlayStatus::Idle);
}
