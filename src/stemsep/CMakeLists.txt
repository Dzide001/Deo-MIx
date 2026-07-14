# AI Stem Separation stage 1: standalone validation CLI. Not linked into
# mixxx-lib / mixxx-qml-lib / mixxx / mixxx-test. wavio.cpp is a minimal
# self-contained PCM WAV reader/writer, not SndFile -- avoids linking a
# static libsndfile's transitive codec dependencies (opus/vorbis/ogg) that
# mixxx-lib gets "for free" elsewhere but this standalone target does not.
add_executable(mixxx-stemsep-test stemsep_cli.cpp wavio.cpp)
target_link_libraries(mixxx-stemsep-test PRIVATE demucsonnx)
target_compile_features(mixxx-stemsep-test PRIVATE cxx_std_20)

# AI Stem Separation stage 2: writes real .stem.mp4/.stem.m4a files from
# separated audio (muxstem.cpp: FFmpeg AAC muxing; stematom.cpp: the
# moov/udta/stem metadata atom patch; stemwriter.cpp: orchestrator). Uses
# the FFmpeg libraries already located by the top-level FFMPEG/STEM option
# (CMakeLists.txt) -- no new find_package call, this option requires
# -DFFMPEG=ON (enforced in the top-level AI_STEM_SEPARATION block).
add_library(stemwriter STATIC EXCLUDE_FROM_ALL
    muxstem.cpp
    stematom.cpp
    stemwriter.cpp
)
target_include_directories(stemwriter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(stemwriter PUBLIC Eigen3::Eigen "${FFMPEG_LIBRARIES}")
target_compile_features(stemwriter PUBLIC cxx_std_20)
if(APPLE)
  # FFmpeg's native AAC encoder can fall back through libavcodec's
  # AudioToolbox wrapper (audiotoolboxenc.o) on macOS; mixxx-lib gets this
  # framework "for free" via its own APPLE link block below, but this
  # standalone target does not.
  target_link_libraries(stemwriter PUBLIC "-weak_framework AudioToolbox")
endif()

# Stage 2 validation CLI: writes a real stem file, then round-trips it
# through Mixxx's real, unmodified reader (steminfoimporter.cpp/steminfo.cpp,
# compiled directly here rather than linking all of mixxx-lib -- they only
# need Qt Core/Gui). logger.cpp is needed by steminfoimporter.cpp's
# kLogger; util/logging.cpp and util/cmdlineargs.cpp are deliberately NOT
# linked (the latter pulls in soundsourceproxy.h and thus most of
# mixxx-lib) -- steminfoimporter.cpp never calls the logging APIs that
# would need them. VERIFY_OR_DEBUG_ASSERT needs src/util/assert.h, which
# mixxx-lib normally supplies via its own precompiled header; applied here
# directly since assert.h is header-only (no link-time dependency).
add_executable(mixxx-stemwriter-test
    stemwriter_cli.cpp
    wavio.cpp
    ${CMAKE_SOURCE_DIR}/src/track/steminfoimporter.cpp
    ${CMAKE_SOURCE_DIR}/src/track/steminfo.cpp
    ${CMAKE_SOURCE_DIR}/src/util/logger.cpp
)
target_link_libraries(mixxx-stemwriter-test PRIVATE
    stemwriter
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
)
target_include_directories(mixxx-stemwriter-test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_precompile_headers(mixxx-stemwriter-test PRIVATE ${CMAKE_SOURCE_DIR}/src/util/assert.h)
target_compile_features(mixxx-stemwriter-test PRIVATE cxx_std_20)
