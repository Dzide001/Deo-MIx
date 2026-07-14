// Stage 2 standalone validation tool for the stem file writer (muxstem +
// stematom + stemwriter, see stemwriter.h). Writes a real .stem.mp4 from
// plain WAV inputs, then round-trips it through Mixxx's own real,
// unmodified reader (mixxx::StemInfoImporter, src/track/steminfoimporter.cpp)
// to confirm the file is byte-correct -- this exercises the actual shipping
// parser, not a reimplementation of it. Not linked into mixxx-lib/mixxx.
#include <QColor>
#include <QList>
#include <QString>

#include <array>
#include <iostream>
#include <string>

#include "stemwriter.h"
#include "track/steminfo.h"
#include "track/steminfoimporter.h"
#include "wavio.h"

namespace {

constexpr int kSampleRate = 44100;

struct Args {
    std::string premixPath;
    std::string stemsDir; // expects drums.wav, bass.wav, other.wav, vocals.wav
    std::string outputPath;
};

Args parseArgs(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("stemwriter-test: missing value for " + arg);
            }
            return argv[++i];
        };
        if (arg == "--premix") {
            args.premixPath = next();
        } else if (arg == "--stems-dir") {
            args.stemsDir = next();
        } else if (arg == "--output") {
            args.outputPath = next();
        } else {
            throw std::runtime_error("stemwriter-test: unknown argument " + arg);
        }
    }
    if (args.premixPath.empty() || args.stemsDir.empty() || args.outputPath.empty()) {
        throw std::runtime_error(
                "usage: mixxx-stemwriter-test --premix <path.wav> --stems-dir <dir> "
                "--output <path.stem.mp4>\n"
                "  (stems-dir must contain drums.wav, bass.wav, other.wav, vocals.wav, "
                "matching mixxx-stemsep-test's output naming)");
    }
    return args;
}

} // namespace

int main(int argc, char** argv) {
    Args args;
    try {
        args = parseArgs(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    try {
        std::cout << "Loading premix: " << args.premixPath << std::endl;
        const Eigen::MatrixXf premix = stemsep::loadWavStereoFloat(args.premixPath, kSampleRate);

        // Canonical container/JSON order this writer commits to: Vocals,
        // Drums, Bass, Other. Colors reuse mixxx::StemInfoImporter's own
        // default palette (kStemDefaultColor in steminfoimporter.cpp),
        // index-for-index, purely for visual consistency with files that
        // fall back to those defaults.
        const std::array<std::string, 4> names = {"Vocals", "Drums", "Bass", "Other"};
        const std::array<std::string, 4> sourceFiles = {"vocals.wav", "drums.wav", "bass.wav", "other.wav"};
        const std::array<std::string, 4> colors = {"#009E73", "#D55E00", "#CC79A7", "#56B4E9"};

        std::vector<stemsep::StemTrackInfo> stems;
        stems.reserve(4);
        for (int i = 0; i < 4; ++i) {
            const std::string path = args.stemsDir + "/" + sourceFiles[i];
            std::cout << "Loading stem '" << names[i] << "': " << path << std::endl;
            stemsep::StemTrackInfo info;
            info.name = names[i];
            info.colorHex = colors[i];
            info.buffer = stemsep::loadWavStereoFloat(path, kSampleRate);
            stems.push_back(std::move(info));
        }

        std::cout << "Writing " << args.outputPath << std::endl;
        stemsep::writeStemFile(args.outputPath, kSampleRate, premix, stems);
        std::cout << "Wrote " << args.outputPath << std::endl;

        // Round-trip through the real, unmodified reader.
        const QString outputQPath = QString::fromStdString(args.outputPath);
        if (!mixxx::StemInfoImporter::hasStemAtom(outputQPath)) {
            std::cerr << "stemwriter-test: hasStemAtom() returned false for the file we just wrote"
                       << std::endl;
            return 1;
        }
        const QList<StemInfo> imported = mixxx::StemInfoImporter::importStemInfos(outputQPath);
        if (imported.size() != 4) {
            std::cerr << "stemwriter-test: expected 4 imported stems, got " << imported.size()
                       << std::endl;
            return 1;
        }

        bool mismatch = false;
        for (int i = 0; i < 4; ++i) {
            const QString expectedLabel = QString::fromStdString(names[i]);
            const QColor expectedColor(QString::fromStdString(colors[i]));
            const StemInfo& got = imported[i];
            std::cout << "  stem[" << i << "]: label=" << got.getLabel().toStdString()
                       << " color=" << got.getColor().name().toStdString() << std::endl;
            if (got.getLabel() != expectedLabel || got.getColor() != expectedColor) {
                std::cerr << "  MISMATCH: expected label=" << expectedLabel.toStdString()
                           << " color=" << expectedColor.name().toStdString() << std::endl;
                mismatch = true;
            }
        }

        if (mismatch) {
            std::cerr << "stemwriter-test: round-trip mismatch" << std::endl;
            return 1;
        }

        std::cout << "Round-trip OK: hasStemAtom() and importStemInfos() both match what was written."
                   << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "stemwriter-test: " << e.what() << std::endl;
        return 1;
    }
}
