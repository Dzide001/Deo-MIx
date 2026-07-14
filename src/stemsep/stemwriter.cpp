#include "stemwriter.h"

#include <array>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <stdexcept>

#include "muxstem.h"
#include "stematom.h"

namespace stemsep {

namespace {

std::string escapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

bool isValidColorHex(const std::string& color) {
    if (color.size() != 7 || color[0] != '#') {
        return false;
    }
    for (size_t i = 1; i < color.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(color[i]))) {
            return false;
        }
    }
    return true;
}

std::string buildManifestJson(const std::vector<StemTrackInfo>& stems) {
    std::ostringstream json;
    json << R"({"version":1,"stems":[)";
    for (size_t i = 0; i < stems.size(); ++i) {
        if (i > 0) {
            json << ',';
        }
        json << R"({"name":")" << escapeJsonString(stems[i].name) << R"(","color":")"
             << stems[i].colorHex << R"("})";
    }
    json << "]}";
    return json.str();
}

} // namespace

void writeStemFile(
        const std::string& outPath,
        int sampleRate,
        const Eigen::MatrixXf& premix,
        const std::vector<StemTrackInfo>& stems,
        int aacBitRatePerStreamBps) {
    if (stems.size() != 4) {
        throw std::runtime_error(
                "stemsep: stemwriter: writeStemFile requires exactly 4 stems, got " +
                std::to_string(stems.size()));
    }
    if (premix.rows() != 2) {
        throw std::runtime_error("stemsep: stemwriter: premix must be a 2-row (stereo) buffer");
    }
    for (const auto& stem : stems) {
        if (stem.buffer.rows() != 2) {
            throw std::runtime_error(
                    "stemsep: stemwriter: stem '" + stem.name +
                    "' must be a 2-row (stereo) buffer");
        }
        if (stem.buffer.cols() != premix.cols()) {
            throw std::runtime_error(
                    "stemsep: stemwriter: stem '" + stem.name +
                    "' sample count does not match the premix");
        }
        if (!isValidColorHex(stem.colorHex)) {
            throw std::runtime_error(
                    "stemsep: stemwriter: stem '" + stem.name +
                    "' has an invalid color (expected \"#RRGGBB\"): " + stem.colorHex);
        }
    }

    const std::string tmpPath = outPath + ".tmp";
    try {
        const std::array<Eigen::MatrixXf, 4> stemBuffers = {
                stems[0].buffer, stems[1].buffer, stems[2].buffer, stems[3].buffer};
        muxStemContainer(tmpPath, sampleRate, premix, stemBuffers, aacBitRatePerStreamBps);
        patchStemAtom(tmpPath, buildManifestJson(stems));
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(tmpPath, ignored);
        throw;
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, outPath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        throw std::runtime_error(
                "stemsep: stemwriter: failed to rename " + tmpPath + " to " + outPath + ": " +
                ec.message());
    }
}

} // namespace stemsep
