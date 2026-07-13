#include "wavio.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace stemsep {

namespace {

constexpr uint16_t kFormatPcm = 1;
constexpr uint16_t kFormatIeeeFloat = 3;

uint32_t readU32(std::ifstream& f) {
    uint8_t b[4];
    f.read(reinterpret_cast<char*>(b), 4);
    return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
            (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
}

uint16_t readU16(std::ifstream& f) {
    uint8_t b[2];
    f.read(reinterpret_cast<char*>(b), 2);
    return static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8);
}

void writeU32(std::ofstream& f, uint32_t v) {
    uint8_t b[4] = {
            static_cast<uint8_t>(v & 0xff),
            static_cast<uint8_t>((v >> 8) & 0xff),
            static_cast<uint8_t>((v >> 16) & 0xff),
            static_cast<uint8_t>((v >> 24) & 0xff)};
    f.write(reinterpret_cast<char*>(b), 4);
}

void writeU16(std::ofstream& f, uint16_t v) {
    uint8_t b[2] = {static_cast<uint8_t>(v & 0xff), static_cast<uint8_t>((v >> 8) & 0xff)};
    f.write(reinterpret_cast<char*>(b), 2);
}

} // namespace

Eigen::MatrixXf loadWavStereoFloat(const std::string& path, int expectedSampleRate) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("stemsep: could not open " + path);
    }

    char tag[4];
    f.read(tag, 4);
    if (std::strncmp(tag, "RIFF", 4) != 0) {
        throw std::runtime_error("stemsep: " + path + " is not a RIFF file");
    }
    readU32(f); // chunk size, unused
    f.read(tag, 4);
    if (std::strncmp(tag, "WAVE", 4) != 0) {
        throw std::runtime_error("stemsep: " + path + " is not a WAVE file");
    }

    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    bool haveFmt = false;
    std::vector<char> data;

    while (f.good() && !f.eof()) {
        char chunkId[4];
        f.read(chunkId, 4);
        if (f.eof()) {
            break;
        }
        const uint32_t chunkSize = readU32(f);
        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            audioFormat = readU16(f);
            channels = readU16(f);
            sampleRate = readU32(f);
            readU32(f); // byte rate, unused
            readU16(f); // block align, unused
            bitsPerSample = readU16(f);
            const uint32_t consumed = 16;
            if (chunkSize > consumed) {
                f.seekg(chunkSize - consumed, std::ios::cur);
            }
            haveFmt = true;
        } else if (std::strncmp(chunkId, "data", 4) == 0) {
            data.resize(chunkSize);
            f.read(data.data(), chunkSize);
        } else {
            f.seekg(chunkSize, std::ios::cur);
        }
        if (chunkSize % 2 == 1) {
            f.seekg(1, std::ios::cur); // chunks are word-aligned
        }
    }

    if (!haveFmt || data.empty()) {
        throw std::runtime_error("stemsep: " + path + " is missing fmt/data chunks");
    }
    if (static_cast<int>(sampleRate) != expectedSampleRate) {
        throw std::runtime_error(
                "stemsep: " + path + " is " + std::to_string(sampleRate) + "Hz, expected " +
                std::to_string(expectedSampleRate) + "Hz (no resampling in this stage)");
    }
    if (channels != 2) {
        throw std::runtime_error(
                "stemsep: " + path + " has " + std::to_string(channels) +
                " channel(s), expected exactly 2 (no mono upmix in this stage)");
    }
    if (!(audioFormat == kFormatPcm && bitsPerSample == 16) &&
            !(audioFormat == kFormatIeeeFloat && bitsPerSample == 32)) {
        throw std::runtime_error(
                "stemsep: " + path + " uses unsupported format/bit-depth combination "
                "(format=" + std::to_string(audioFormat) + ", bits=" + std::to_string(bitsPerSample) +
                "); only 16-bit PCM and 32-bit float are supported in this stage");
    }

    const size_t bytesPerSample = bitsPerSample / 8;
    const size_t frameCount = data.size() / (bytesPerSample * channels);
    Eigen::MatrixXf out(2, static_cast<Eigen::Index>(frameCount));

    if (audioFormat == kFormatPcm) {
        const auto* samples = reinterpret_cast<const int16_t*>(data.data());
        for (size_t i = 0; i < frameCount; ++i) {
            out(0, static_cast<Eigen::Index>(i)) = samples[i * 2] / 32768.0f;
            out(1, static_cast<Eigen::Index>(i)) = samples[i * 2 + 1] / 32768.0f;
        }
    } else {
        const auto* samples = reinterpret_cast<const float*>(data.data());
        for (size_t i = 0; i < frameCount; ++i) {
            out(0, static_cast<Eigen::Index>(i)) = samples[i * 2];
            out(1, static_cast<Eigen::Index>(i)) = samples[i * 2 + 1];
        }
    }
    return out;
}

void writeWavStereoFloat(
        const std::string& path, const Eigen::MatrixXf& buffer, int sampleRate) {
    if (buffer.rows() != 2) {
        throw std::runtime_error("stemsep: writeWavStereoFloat expects a 2-row (stereo) buffer");
    }

    const auto numFrames = buffer.cols();
    const uint32_t dataBytes = static_cast<uint32_t>(numFrames) * 2 * sizeof(float);
    constexpr uint16_t kChannels = 2;
    constexpr uint16_t kBitsPerSample = 32;
    const uint32_t byteRate =
            static_cast<uint32_t>(sampleRate) * kChannels * (kBitsPerSample / 8);
    const uint16_t blockAlign = kChannels * (kBitsPerSample / 8);

    std::ofstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("stemsep: could not open " + path + " for writing");
    }

    f.write("RIFF", 4);
    writeU32(f, 36 + dataBytes);
    f.write("WAVE", 4);

    f.write("fmt ", 4);
    writeU32(f, 16);
    writeU16(f, kFormatIeeeFloat);
    writeU16(f, kChannels);
    writeU32(f, static_cast<uint32_t>(sampleRate));
    writeU32(f, byteRate);
    writeU16(f, blockAlign);
    writeU16(f, kBitsPerSample);

    f.write("data", 4);
    writeU32(f, dataBytes);
    for (Eigen::Index i = 0; i < numFrames; ++i) {
        const float left = buffer(0, i);
        const float right = buffer(1, i);
        f.write(reinterpret_cast<const char*>(&left), sizeof(float));
        f.write(reinterpret_cast<const char*>(&right), sizeof(float));
    }
}

} // namespace stemsep
