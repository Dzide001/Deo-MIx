# AI Stem Separation stage 1: standalone validation CLI. Not linked into
# mixxx-lib / mixxx-qml-lib / mixxx / mixxx-test. wavio.cpp is a minimal
# self-contained PCM WAV reader/writer, not SndFile -- avoids linking a
# static libsndfile's transitive codec dependencies (opus/vorbis/ogg) that
# mixxx-lib gets "for free" elsewhere but this standalone target does not.
add_executable(mixxx-stemsep-test stemsep_cli.cpp wavio.cpp)
target_link_libraries(mixxx-stemsep-test PRIVATE demucsonnx)
target_compile_features(mixxx-stemsep-test PRIVATE cxx_std_20)
