// Headless WAV render sharing the real engine DSP (GeometryService ->
// SpectrumFrame -> ModalVoice). This is the artifact that keeps the spectral
// oracles running post-Python (CLAUDE.md: oracles never go dark).
//
// Usage:
//   render_offline --preset N --note-hz 220 --strike 0.42 --modes 96
//                  --t60 30 --tilt 0 --mallet 20000 --release 0.3
//                  --seconds 20 --sr 48000 --genus2-obj path --out out.wav
//                  [--flow 0|1|2] [--flow-rate r] [--kick amp] [--bow b]
// flow 1 = RELAX, 2 = SHARPEN; kick applies a conformal perturbation at t=0
// (the displacement RELAX relaxes from). With flow on, the voice runs in
// global mode and retunes to the evolving spectrum exactly as the plugin's
// audio thread does.
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "../src/engine/GeometryService.h"
#include "../src/engine/VoiceManager.h"

namespace {

void writeWav16(const std::string& path, const std::vector<float>& mono, int sr)
{
    std::ofstream out(path, std::ios::binary);
    auto u32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };

    const uint32_t dataBytes = static_cast<uint32_t>(mono.size() * 2);
    out.write("RIFF", 4); u32(36 + dataBytes); out.write("WAVE", 4);
    out.write("fmt ", 4); u32(16); u16(1); u16(1); u32((uint32_t) sr);
    u32((uint32_t) sr * 2); u16(2); u16(16);
    out.write("data", 4); u32(dataBytes);
    for (float x : mono) {
        const float c = std::max(-1.0f, std::min(1.0f, x));
        const int16_t s = static_cast<int16_t>(std::lround(c * 32767.0f));
        out.write(reinterpret_cast<const char*>(&s), 2);
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::map<std::string, std::string> args;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (argv[i][0] != '-' || argv[i][1] != '-') {
            std::cerr << "bad argument: " << argv[i] << "\n";
            return 2;
        }
        args[argv[i] + 2] = argv[i + 1];
    }
    auto get = [&](const std::string& key, const std::string& dflt) {
        auto it = args.find(key);
        return it == args.end() ? dflt : it->second;
    };

    const int preset = std::stoi(get("preset", "0"));
    const float noteHz = std::stof(get("note-hz", "220"));
    const float strike = std::stof(get("strike", "0.42"));
    const int modes = std::stoi(get("modes", "96"));
    const float t60 = std::stof(get("t60", "5"));
    const float tilt = std::stof(get("tilt", "0.7"));
    const float mallet = std::stof(get("mallet", "2200"));
    const float release = std::stof(get("release", "0.3"));
    const double seconds = std::stod(get("seconds", "7"));
    const int sr = std::stoi(get("sr", "48000"));
    const std::string out = get("out", "render.wav");
    const std::string genus2Path = get("genus2-obj", "assets/manifolds/genus2.obj");
    const std::string mandelbulbPath = get("mandelbulb-obj", "assets/manifolds/mandelbulb.obj");
    const int flowMode = std::stoi(get("flow", "0"));
    const float flowRate = std::stof(get("flow-rate", "0.5"));
    const double kickAmp = std::stod(get("kick", "0"));
    const float bow = std::stof(get("bow", "0"));

    std::string objData;
    if (curv::presetNeedsObj((curv::PresetId) preset)) {
        const std::string path = preset == (int) curv::PresetId::Mandelbulb
                                     ? mandelbulbPath : genus2Path;
        std::ifstream f(path);
        if (!f) {
            std::cerr << "cannot open " << path << "\n";
            return 1;
        }
        objData.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    curv::GeometryService geometry;
    geometry.loadPreset((curv::PresetId) preset, objData.data(), objData.size());
    if (kickAmp > 0.0)
        geometry.flowKick(kickAmp, 1);

    curv::SpectrumFrame frame;
    geometry.fillFrame(frame, modes, strike);

    const float warp = std::stof(get("warp", "1"));

    curv::ModalVoice voice;
    voice.prepare(sr);
    voice.setBow(bow);  // before noteOn: independent of the mallet impulse
    voice.setWarp(warp);
    voice.noteOn(frame, 69, 1.0f, noteHz, mallet, { t60, tilt, release }, 0);
    if (flowMode != 0)
        voice.setGlobalTuning(&frame);

    const int n = static_cast<int>(seconds * sr);
    std::vector<float> audio((size_t) n, 0.0f);
    constexpr int kBlock = 1024;             // ~21 ms: one geometry step per block
    const double dt = 0.25 * flowRate * flowRate;
    double flowSinceResolve = 0.0;
    for (int i = 0; i < n; i += kBlock) {
        if (flowMode != 0 && dt > 0.0) {
            flowSinceResolve += geometry.flowStep(dt, flowMode == 1 ? +1.0 : -1.0);
            if (flowSinceResolve >= 0.6) {   // same cadence policy as the plugin
                geometry.resolve();
                flowSinceResolve = 0.0;
            }
            geometry.fillFrame(frame, modes, strike);
        }
        voice.renderAdd(audio.data() + i, std::min(kBlock, n - i));
    }

    // normalize to -3 dBFS like the Python rig
    float peak = 0.0f;
    for (float x : audio)
        peak = std::max(peak, std::abs(x));
    if (peak > 0.0f) {
        const float g = 0.7079f / peak;
        for (float& x : audio)
            x *= g;
    }

    writeWav16(out, audio, sr);
    std::cout << "wrote " << out << " (" << seconds << "s, preset " << preset
              << ", " << frame.numModes << " modes)\n";
    return 0;
}
