// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/media/audio/audio_core/audio_device_settings.h"

#include <lib/gtest/test_loop_fixture.h>

#include "src/media/audio/audio_core/audio_driver.h"

namespace media::audio {
namespace {

static constexpr audio_stream_unique_id_t kTestUniqueId = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};

static constexpr HwGainState kDefaultInitialHwGainState = {
    false,   /* cur_mute */
    false,   /* cur_agc */
    0.0,     /* cur_gain */
    true,    /* can_mute */
    true,    /* can_agc */
    -160.0f, /* min_gain */
    24.0f,   /* max_gain */
    1.0f     /* gain_step */
};

class AudioDeviceSettingsTest : public gtest::TestLoopFixture {};

// If agc is not supported, then always return 'false' for agc enabled.
TEST_F(AudioDeviceSettingsTest, AgcFalseWhenNotSupported) {
  // Set AGC/Mute to 'true' but not supported.
  HwGainState hw_gain_state = kDefaultInitialHwGainState;
  hw_gain_state.cur_agc = true;
  hw_gain_state.can_agc = false;
  AudioDeviceSettings settings(kTestUniqueId, hw_gain_state, false);

  auto gain_info = settings.GetGainInfo();

  EXPECT_FALSE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcEnabled);
  EXPECT_FALSE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcSupported);
}

// If can_mute is false, still allow the device to be muted. In cases without the hardware mute
// we'll implement mute in software.
TEST_F(AudioDeviceSettingsTest, MuteTrueWhenNotSupported) {
  // Set AGC/Mute to 'true' but not supported.
  HwGainState hw_gain_state = kDefaultInitialHwGainState;
  hw_gain_state.cur_mute = true;
  hw_gain_state.can_mute = false;
  AudioDeviceSettings settings(kTestUniqueId, hw_gain_state, false);

  auto gain_info = settings.GetGainInfo();

  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_Mute);
}

TEST_F(AudioDeviceSettingsTest, SetGainInfoDoesNothingWithNoFlags) {
  // Set AGC/Mute to 'true' but not supported.
  HwGainState hw_gain_state = kDefaultInitialHwGainState;
  hw_gain_state.cur_mute = true;
  hw_gain_state.cur_agc = true;
  hw_gain_state.can_mute = true;
  hw_gain_state.can_agc = true;
  hw_gain_state.cur_gain = 5.0;
  AudioDeviceSettings settings(kTestUniqueId, hw_gain_state, false);

  // Verify initial state.
  auto gain_info = settings.GetGainInfo();
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_Mute);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcEnabled);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcSupported);
  EXPECT_EQ(5.0f, gain_info.gain_db);

  // Update gain state
  fuchsia::media::AudioGainInfo new_gain_info;
  new_gain_info.gain_db = 10.0;
  new_gain_info.flags = 0;
  EXPECT_FALSE(settings.SetGainInfo(new_gain_info, 0 /* flags */));

  // State should match initial state.
  gain_info = settings.GetGainInfo();
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_Mute);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcEnabled);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcSupported);
  EXPECT_EQ(5.0f, gain_info.gain_db);
}

TEST_F(AudioDeviceSettingsTest, SetGainInfoOnlyGainDb) {
  // Set AGC/Mute to 'true' but not supported.
  HwGainState hw_gain_state = kDefaultInitialHwGainState;
  hw_gain_state.cur_mute = true;
  hw_gain_state.cur_agc = true;
  hw_gain_state.can_mute = true;
  hw_gain_state.can_agc = true;
  hw_gain_state.cur_gain = 5.0;
  AudioDeviceSettings settings(kTestUniqueId, hw_gain_state, false);

  // Verify initial state.
  auto gain_info = settings.GetGainInfo();
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_Mute);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcEnabled);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcSupported);
  EXPECT_EQ(5.0f, gain_info.gain_db);

  // Update gain state
  fuchsia::media::AudioGainInfo new_gain_info;
  new_gain_info.gain_db = 10.0;
  new_gain_info.flags = 0;
  EXPECT_TRUE(settings.SetGainInfo(new_gain_info, fuchsia::media::SetAudioGainFlag_GainValid));

  // Only gain updated.
  gain_info = settings.GetGainInfo();
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_Mute);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcEnabled);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcSupported);
  EXPECT_EQ(10.0f, gain_info.gain_db);
}

TEST_F(AudioDeviceSettingsTest, SetGainInfoOnlyMute) {
  // Set AGC/Mute to 'true' but not supported.
  HwGainState hw_gain_state = kDefaultInitialHwGainState;
  hw_gain_state.cur_mute = true;
  hw_gain_state.cur_agc = true;
  hw_gain_state.can_mute = true;
  hw_gain_state.can_agc = true;
  hw_gain_state.cur_gain = 5.0;
  AudioDeviceSettings settings(kTestUniqueId, hw_gain_state, false);

  // Verify initial state.
  auto gain_info = settings.GetGainInfo();
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_Mute);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcEnabled);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcSupported);
  EXPECT_EQ(5.0f, gain_info.gain_db);

  // Update gain state
  fuchsia::media::AudioGainInfo new_gain_info;
  new_gain_info.gain_db = 10.0;
  new_gain_info.flags = 0;
  EXPECT_TRUE(settings.SetGainInfo(new_gain_info, fuchsia::media::SetAudioGainFlag_MuteValid));

  // Only gain updated.
  gain_info = settings.GetGainInfo();
  EXPECT_FALSE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_Mute);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcEnabled);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcSupported);
  EXPECT_EQ(5.0f, gain_info.gain_db);
}

TEST_F(AudioDeviceSettingsTest, SetGainInfoOnlyAgc) {
  // Set AGC/Mute to 'true' but not supported.
  HwGainState hw_gain_state = kDefaultInitialHwGainState;
  hw_gain_state.cur_mute = true;
  hw_gain_state.cur_agc = true;
  hw_gain_state.can_mute = true;
  hw_gain_state.can_agc = true;
  hw_gain_state.cur_gain = 5.0;
  AudioDeviceSettings settings(kTestUniqueId, hw_gain_state, false);

  // Verify initial state.
  auto gain_info = settings.GetGainInfo();
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_Mute);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcEnabled);
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcSupported);
  EXPECT_EQ(5.0f, gain_info.gain_db);

  // Update gain state
  fuchsia::media::AudioGainInfo new_gain_info;
  new_gain_info.gain_db = 10.0;
  new_gain_info.flags = 0;
  EXPECT_TRUE(settings.SetGainInfo(new_gain_info, fuchsia::media::SetAudioGainFlag_AgcValid));

  // Only gain updated.
  gain_info = settings.GetGainInfo();
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_Mute);
  EXPECT_FALSE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcEnabled);
  // Note that we expect AgcSupported to remain unchanged.
  EXPECT_TRUE(gain_info.flags & fuchsia::media::AudioGainInfoFlag_AgcSupported);
  EXPECT_EQ(5.0f, gain_info.gain_db);
}

}  // namespace
}  // namespace media::audio
