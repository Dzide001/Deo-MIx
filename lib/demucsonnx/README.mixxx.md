# demucsonnx — provenance

Vendored from `github.com/dhunstack/demucs`, branch `onnxrt`
(`cppscripts/src/`), commit reachable via open PRs
[mixxxdj/demucs#7](https://github.com/mixxxdj/demucs/pull/7) and
[mixxxdj/demucs#9](https://github.com/mixxxdj/demucs/pull/9) — C++ ONNX
Runtime inference for HTDemucs, produced during the Mixxx GSoC 2025
"Converting Demucs v4 (Hybrid Transformer) AI model to ONNX format" project
(see `github.com/mixxxdj/mixxx` issue #15495 and
<https://mixxx.org/news/2025-10-27-gsoc2025-demucs-to-onnx-dhunstack/>).
MIT licensed (see `LICENSE`, copyright Meta Platforms — the original Demucs
copyright holder; unchanged by either fork).

Only the model-inference core is vendored (`demucs.hpp`, `dsp.hpp`/`dsp.cpp`,
`tensor.hpp`, `model_apply.cpp`, `model_inference.cpp`) — the upstream
`cppscripts/src_cli/` CLI and its `libnyquist`/Eigen submodule dependencies
are not included; Mixxx has its own audio decode path and doesn't need a
third file-I/O dependency for this.

## Changes from upstream

- `#include <onnxruntime/onnxruntime_cxx_api.h>` → `#include
  <onnxruntime_cxx_api.h>` in `tensor.hpp`, `model_inference.cpp`,
  `model_apply.cpp` — upstream's cppscripts build vendors onnxruntime as a
  submodule under an `onnxruntime/` include prefix; this build links against
  Microsoft's official `onnxruntime-osx-arm64` release tarball, whose
  headers are flat (`include/onnxruntime_cxx_api.h`, no subdirectory).
- No other logic changes.

## Known behavior worth noting

`demucs_inference()` (`model_apply.cpp`) applies Demucs' own random
time-shift trick for a small quality gain (`std::random_device` seeds a
`std::mt19937` used to pick a shift offset per call) — output is not
bit-identical between runs on the same input, by design, matching upstream
Demucs' own `demucs.api.Separator` behavior (not a bug introduced here).
