# M16 Stage 1: standalone validation CLI. Not linked into mixxx-lib /
# mixxx-qml-lib / mixxx / mixxx-test -- proves the whisper.cpp FetchContent
# integration actually builds and links correctly, and that this codebase's
# own minimal WAV loader + the real "whisper" CMake target produce accurate,
# correctly-timestamped output, before any of it touches the real app.
add_executable(mixxx-whisper-test whisper_cli_test.cpp minimalwav.cpp)
target_link_libraries(mixxx-whisper-test PRIVATE whisper)
target_compile_features(mixxx-whisper-test PRIVATE cxx_std_20)
