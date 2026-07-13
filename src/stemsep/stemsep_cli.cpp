// Stage 1 standalone validation tool for the vendored demucsonnx inference
// core (see lib/demucsonnx/). Not part of mixxx-lib/mixxx-qml-lib/mixxx --
// deliberately isolated, see plan doc. Loads a real HTDemucs ONNX model,
// runs inference on a real WAV file via ONNX Runtime's CPU execution
// provider (CoreML is not enabled here -- a CoreML-provider crash was hit
// with an unrelated ONNX package on this machine during the Python
// validation pass; re-evaluate in a later stage, not this one), and
// optionally diffs the output against known-good reference stems.
#include <demucs.hpp>
#include <onnxruntime_cxx_api.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "wavio.h"

namespace {

// Matches demucs.api.Separator(model='htdemucs').model.sources, the exact
// model this stage's test file (htdemucs.onnx) was exported from -- see
// scratchpad notes from the Python validation pass this stage follows up on.
const std::vector<std::string> kSourceNames = {"drums", "bass", "other", "vocals"};
constexpr int kSampleRate = 44100;

struct Args {
    std::string modelPath;
    std::string inputPath;
    std::string outputDir;
    std::string referenceDir;
};

Args parseArgs(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("stemsep: missing value for " + arg);
            }
            return argv[++i];
        };
        if (arg == "--model") {
            args.modelPath = next();
        } else if (arg == "--input") {
            args.inputPath = next();
        } else if (arg == "--output-dir") {
            args.outputDir = next();
        } else if (arg == "--reference-dir") {
            args.referenceDir = next();
        } else {
            throw std::runtime_error("stemsep: unknown argument " + arg);
        }
    }
    if (args.modelPath.empty() || args.inputPath.empty() || args.outputDir.empty()) {
        throw std::runtime_error(
                "usage: mixxx-stemsep-test --model <path.onnx> --input <path.wav> "
                "--output-dir <dir> [--reference-dir <dir>]");
    }
    return args;
}

std::vector<char> readFileBytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("stemsep: could not open " + path);
    }
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(size));
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error("stemsep: short read on " + path);
    }
    return buffer;
}

bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
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
        std::cout << "Loading model: " << args.modelPath << std::endl;
        const std::vector<char> modelBytes = readFileBytes(args.modelPath);

        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(1);
        // CPU execution provider only -- see file header comment.

        demucsonnx::demucs_model model;
        if (!demucsonnx::load_model(modelBytes, model, sessionOptions)) {
            std::cerr << "stemsep: failed to load model" << std::endl;
            return 1;
        }
        std::cout << "Model loaded, nb_sources=" << model.nb_sources << std::endl;

        std::cout << "Loading input: " << args.inputPath << std::endl;
        const Eigen::MatrixXf audio = stemsep::loadWavStereoFloat(args.inputPath, kSampleRate);
        std::cout << "Input: " << audio.cols() << " samples ("
                   << (static_cast<double>(audio.cols()) / kSampleRate) << "s)" << std::endl;

        auto progress = [](float fraction, const std::string& message) {
            std::cout << "  " << static_cast<int>(fraction * 100) << "% - " << message
                       << std::endl;
        };

        const auto start = std::chrono::steady_clock::now();
        const Eigen::Tensor3dXf out = demucsonnx::demucs_inference(model, audio, progress);
        const auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start)
                                     .count();
        std::cout << "Inference complete in " << elapsed << "s ("
                   << (elapsed / (static_cast<double>(audio.cols()) / kSampleRate))
                   << "x realtime)" << std::endl;

        if (out.dimension(0) != static_cast<Eigen::Index>(kSourceNames.size())) {
            std::cerr << "stemsep: unexpected source count " << out.dimension(0)
                       << ", expected " << kSourceNames.size() << std::endl;
            return 1;
        }

        bool referenceMismatch = false;
        for (int s = 0; s < out.dimension(0); ++s) {
            Eigen::MatrixXf stem(2, out.dimension(2));
            for (int ch = 0; ch < 2; ++ch) {
                for (int i = 0; i < out.dimension(2); ++i) {
                    stem(ch, i) = out(s, ch, i);
                }
            }
            const std::string outPath = args.outputDir + "/" + kSourceNames[s] + ".wav";
            stemsep::writeWavStereoFloat(outPath, stem, kSampleRate);
            std::cout << "Wrote " << outPath << std::endl;

            if (!args.referenceDir.empty()) {
                const std::string refPath = args.referenceDir + "/" + kSourceNames[s] + ".wav";
                if (!fileExists(refPath)) {
                    std::cout << "  (no reference at " << refPath << ", skipping diff)"
                               << std::endl;
                    continue;
                }
                const Eigen::MatrixXf ref = stemsep::loadWavStereoFloat(refPath, kSampleRate);
                const Eigen::Index n = std::min(ref.cols(), stem.cols());
                // Correlation, not raw sample-wise diff: Demucs applies a
                // random per-call time-shift for a small quality gain (see
                // lib/demucsonnx/README.mixxx.md), so two independent runs
                // of the "same" pipeline are never bit-identical, and small
                // shift-induced misalignment at transients produces large
                // raw diffs even when the separation itself is correct
                // (empirically confirmed: max diff up to 0.79 alongside
                // correlation 0.94-0.997 on a real track during this
                // stage's validation). A genuine bug -- wrong stem order,
                // garbage/silent output, broken windowing -- shows up as
                // LOW correlation, which raw diff alone can't distinguish
                // from expected shift-driven variation.
                const auto aFlat = ref.leftCols(n).reshaped();
                const auto bFlat = stem.leftCols(n).reshaped();
                const float aMean = aFlat.mean();
                const float bMean = bFlat.mean();
                const Eigen::VectorXf aCentered = aFlat.array() - aMean;
                const Eigen::VectorXf bCentered = bFlat.array() - bMean;
                const float correlation = aCentered.dot(bCentered) /
                        (aCentered.norm() * bCentered.norm());
                const float rmsRef = std::sqrt(aFlat.array().square().mean());
                const float rmsOut = std::sqrt(bFlat.array().square().mean());
                std::cout << "  " << kSourceNames[s] << " vs reference: correlation "
                           << correlation << ", rms " << rmsOut << " (reference " << rmsRef
                           << ")" << std::endl;
                if (correlation < 0.9f) {
                    referenceMismatch = true;
                }
            }
        }

        if (referenceMismatch) {
            std::cerr << "stemsep: at least one stem differs significantly from its reference"
                       << std::endl;
            return 1;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "stemsep: " << e.what() << std::endl;
        return 1;
    }
}
