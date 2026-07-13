# AI Stem Separation stage 1: vendored HTDemucs ONNX inference core.
# See README.mixxx.md for provenance and the one adaptation made from upstream.
add_library(demucsonnx STATIC EXCLUDE_FROM_ALL
    dsp.cpp
    model_apply.cpp
    model_inference.cpp
)
target_include_directories(demucsonnx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(demucsonnx PUBLIC Eigen3::Eigen onnxruntime::onnxruntime)
target_compile_features(demucsonnx PUBLIC cxx_std_20)
