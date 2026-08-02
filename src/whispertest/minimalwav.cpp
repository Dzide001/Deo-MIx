#include "minimalwav.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace whispertest {

namespace {

uint32_t readU32(std::ifstream& file) {
    uint8_t bytes[4];
    file.read(reinterpret_cast<char*>(bytes), 4);
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
            (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

uint16_t readU16(std::ifstream& file) {
    uint8_t bytes[2];
    file.read(reinterpret_cast<char*>(bytes), 2);
    return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

} // namespace

std::vector<float> loadWavMono16kFloat(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open WAV file: " + path);
    }

    char riffMagic[4];
    file.read(riffMagic, 4);
    readU32(file); // RIFF chunk size, unused
    char waveMagic[4];
    file.read(waveMagic, 4);
    if (std::memcmp(riffMagic, "RIFF", 4) != 0 || std::memcmp(waveMagic, "WAVE", 4) != 0) {
        throw std::runtime_error("Not a RIFF/WAVE file: " + path);
    }

    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    std::vector<float> samples;
    bool haveFmt = false;
    bool haveData = false;

    while (file && !(haveFmt && haveData)) {
        char chunkId[4];
        file.read(chunkId, 4);
        if (!file) {
            break;
        }
        const uint32_t chunkSize = readU32(file);

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            audioFormat = readU16(file);
            numChannels = readU16(file);
            sampleRate = readU32(file);
            readU32(file); // byte rate, unused
            readU16(file); // block align, unused
            bitsPerSample = readU16(file);
            // Skip any extra format bytes (e.g. WAVE_FORMAT_EXTENSIBLE).
            const uint32_t consumed = 16;
            if (chunkSize > consumed) {
                file.seekg(chunkSize - consumed, std::ios::cur);
            }
            haveFmt = true;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            if (!haveFmt) {
                throw std::runtime_error("WAV data chunk before fmt chunk: " + path);
            }
            if (numChannels != 1) {
                throw std::runtime_error(
                        "Expected mono WAV, got " + std::to_string(numChannels) +
                        " channels: " + path);
            }
            if (sampleRate != 16000) {
                throw std::runtime_error(
                        "Expected 16000 Hz WAV, got " + std::to_string(sampleRate) + " Hz: " +
                        path);
            }

            std::vector<char> raw(chunkSize);
            file.read(raw.data(), chunkSize);

            if (audioFormat == 1 && bitsPerSample == 16) {
                const auto* pcm = reinterpret_cast<const int16_t*>(raw.data());
                const size_t count = raw.size() / sizeof(int16_t);
                samples.reserve(count);
                for (size_t i = 0; i < count; i++) {
                    samples.push_back(static_cast<float>(pcm[i]) / 32768.0f);
                }
            } else if (audioFormat == 3 && bitsPerSample == 32) {
                const auto* pcm = reinterpret_cast<const float*>(raw.data());
                const size_t count = raw.size() / sizeof(float);
                samples.assign(pcm, pcm + count);
            } else {
                throw std::runtime_error(
                        "Unsupported WAV sample format (audioFormat=" +
                        std::to_string(audioFormat) +
                        ", bitsPerSample=" + std::to_string(bitsPerSample) + "): " + path);
            }
            haveData = true;
        } else {
            // Unknown chunk (e.g. LIST/fact) -- skip it. Chunks are
            // padded to even byte boundaries.
            file.seekg(chunkSize + (chunkSize % 2), std::ios::cur);
        }
    }

    if (!haveFmt || !haveData) {
        throw std::runtime_error("WAV file missing fmt or data chunk: " + path);
    }

    return samples;
}

} // namespace whispertest
