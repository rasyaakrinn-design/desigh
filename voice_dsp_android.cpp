// voice_dsp_android.cpp - FIXED for A
#include <cmath>
#include <cstring>
#include <algorithm>
#include <atomic>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
struct VoiceDSPState {
  std::atomic<bool> enabled{true};
  std::atomic<bool> muted{false};
  std::atomic<float> pitchSemitones{5.5f};
  std::atomic<float> toneDb{6.0f};
  std::atomic<int> radioPct{0};
  std::atomic<int> echoPct{8};
  std::atomic<int> noiseGatePct{25};
  std::atomic<int> micGainPct{110};
};
static VoiceDSPState g_dsp;
constexpr int BUF_SIZE = 4096;
static float g_ring[BUF_SIZE] = {0};
static int g_writeIdx = 0;
static float g_phase1 = 0.0f;
static float g_phase2 = 0.5f;
constexpr int ECHO_SIZE = 48000 * 0.35;
static float g_echoBuf[ECHO_SIZE] = {0};
static int g_echoIdx = 0;
static float g_tonePrev = 0.0f;
extern "C" void process_pcm16_frame(short* samples, int numSamples) {
  if (!g_dsp.enabled.load() || g_dsp.muted.load()) {
    if (g_dsp.muted.load()) memset(samples, 0, numSamples * sizeof(short));
    return;
  }
  float pitchRatio = std::pow(2.0f, g_dsp.pitchSemitones.load() / 12.0f);
  float gain = g_dsp.micGainPct.load() / 100.0f;
  float gateThresh = (g_dsp.noiseGatePct.load() / 100.0f) * 0.04f;
  float radioAmt = g_dsp.radioPct.load() / 100.0f;
  float echoAmt = g_dsp.echoPct.load() / 100.0f;
  float toneCoeff = 0.5f + (g_dsp.toneDb.load() / 10.0f) * 0.4f;
  toneCoeff = std::clamp(toneCoeff, 0.1f, 0.9f);
  const float grainSize = 1200.0f;
  const float step = (pitchRatio - 1.0f) / grainSize;
  for (int i = 0; i < numSamples; ++i) {
    float x = (samples[i] / 32768.0f) * gain;
    if (std::fabs(x) < gateThresh) x = 0.0f;
    g_tonePrev = g_tonePrev + toneCoeff * (x - g_tonePrev);
    x = g_tonePrev;
    g_ring[g_writeIdx] = x;
    int idx1 = (int)(g_writeIdx - g_phase1 * grainSize + BUF_SIZE * 2) % BUF_SIZE;
    int idx2 = (int)(g_writeIdx - g_phase2 * grainSize + BUF_SIZE * 2) % BUF_SIZE;
    float w1 = std::sin(3.14159265f * g_phase1);
    float w2 = std::sin(3.14159265f * g_phase2);
    float y = (g_ring[idx1] * w1 + g_ring[idx2] * w2) / (w1 + w2 + 1e-6f);
    if (echoAmt > 0.01f) {
      int echoDelay = (int)(0.18f * 48000);
      int eIdx = (g_echoIdx - echoDelay + ECHO_SIZE) % ECHO_SIZE;
      float echoVal = g_echoBuf[eIdx];
      g_echoBuf[g_echoIdx] = y + echoVal * 0.4f;
      g_echoIdx = (g_echoIdx + 1) % ECHO_SIZE;
      y = y + echoVal * echoAmt * 0.6f;
    }
    if (radioAmt > 0.05f) {
      float drive = 1.0f + radioAmt * 8.0f;
      y = std::tanh(y * drive) / std::tanh(drive);
    }
    y = std::clamp(y, -1.0f, 1.0f);
    samples[i] = static_cast<short>(y * 32767.0f);
    g_phase1 = std::fmod(g_phase1 + step + 1.0f, 1.0f);
    g_phase2 = std::fmod(g_phase2 + step + 1.0f, 1.0f);
    g_writeIdx = (g_writeIdx + 1) % BUF_SIZE;
  }
}
static int l_set_params(lua_State* L) {
  g_dsp.enabled.store(lua_toboolean(L, 1));
  g_dsp.pitchSemitones.store((float)luaL_checknumber(L, 2));
  g_dsp.toneDb.store((float)luaL_checknumber(L, 3));
  g_dsp.radioPct.store((int)luaL_checkinteger(L, 4));
  g_dsp.echoPct.store((int)luaL_checkinteger(L, 5));
  g_dsp.noiseGatePct.store((int)luaL_checkinteger(L, 6));
  g_dsp.micGainPct.store((int)luaL_checkinteger(L, 7));
  return 0;
}
static int l_set_muted(lua_State* L) {
  g_dsp.muted.store(lua_toboolean(L, 1));
  return 0;
}
static const luaL_Reg voicedsp_funcs[] = {
  {"set_params", l_set_params},
  {"set_muted", l_set_muted},
  {nullptr, nullptr}
};
extern "C" int luaopen_voicedsp(lua_State* L) {
  luaL_newlib(L, voicedsp_funcs);
  return 1;
}
