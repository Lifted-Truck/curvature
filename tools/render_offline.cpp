// Headless WAV render sharing the real engine DSP (GeometryService ->
// SpectrumFrame -> ModalVoice). This is the artifact that keeps the spectral
// oracles running post-Python (CLAUDE.md: oracles never go dark).
//
// Usage:
//   render_offline --preset N --note-hz 220 --strike 0.42 --modes 96
//                  --t60 30 --tilt 0 --mallet 20000 --release 0.3
//                  --seconds 20 --sr 48000 --genus2-obj path --out out.wav
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

    std::string objData;
    if (preset == (int) curv::PresetId::Genus2) {
        std::ifstream f(genus2Path);
        if (!f) {
            std::cerr << "cannot open " << genus2Path << "\n";
            return 1;
        }
        objData.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    curv::GeometryService geometry;
    geometry.loadPreset((curv::PresetId) preset, objData.data(), objData.size());

    curv::SpectrumFrame frame;
    geometry.fillFrame(frame, modes, strike);

    curv::ModalVoice voice;
    voice.prepare(sr);
    voice.noteOn(frame, 69, 1.0f, noteHz, mallet, { t60, tilt, release }, 0);

    const int n = static_cast<int>(seconds * sr);
    std::vector<float> audio((size_t) n, 0.0f);
    constexpr int kBlock = 512;
    for (int i = 0; i < n; i += kBlock)
        voice.renderAdd(audio.data() + i, std::min(kBlock, n - i));

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
