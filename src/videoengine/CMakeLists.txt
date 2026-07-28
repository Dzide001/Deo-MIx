# M12 video engine, Stage 1: standalone validation CLI, not linked into
# mixxx-lib / mixxx-qml-lib / mixxx / mixxx-test -- a build break here
# cannot affect the app, mirroring src/stemsep/CMakeLists.txt's role for
# the stem separator.
#
# GStreamer's vcpkg port builds statically with no gstreamer-full umbrella
# archive actually produced (gstreamer-full-1.0.pc exists but
# libgstreamer-full-1.0.a does not -- confirmed empirically, not a vcpkg
# bug report anywhere), so each plugin actually used has to be declared,
# registered, and linked explicitly per GStreamer's own documented
# static-linking pattern (GST_PLUGIN_STATIC_DECLARE/REGISTER, see
# videoengine_cli.cpp). Extending this CLI to use more elements later
# (compositor already in, glvideomixer/avfvideosrc/appsink to come per the
# staged plan) means adding their .a files to the list below.
set(VIDEO_ENGINE_PLUGIN_DIR "${VIDEO_ENGINE_GSTREAMER_ROOT}/lib/gstreamer-1.0")
set(
  VIDEO_ENGINE_STATIC_PLUGINS
  "${VIDEO_ENGINE_PLUGIN_DIR}/libgstcoreelements.a"
  "${VIDEO_ENGINE_PLUGIN_DIR}/libgstcompositor.a"
  "${VIDEO_ENGINE_PLUGIN_DIR}/libgstvideotestsrc.a"
  "${VIDEO_ENGINE_PLUGIN_DIR}/libgstvideoconvertscale.a"
  "${VIDEO_ENGINE_PLUGIN_DIR}/libgstopengl.a"
  "${VIDEO_ENGINE_PLUGIN_DIR}/libgstapplemedia.a"
  "${VIDEO_ENGINE_PLUGIN_DIR}/libgstosxvideo.a"
  "${VIDEO_ENGINE_PLUGIN_DIR}/libgstapp.a"
)

add_executable(mixxx-videoengine-test videoengine_cli.cpp)
target_include_directories(mixxx-videoengine-test PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_compile_options(mixxx-videoengine-test PRIVATE ${GSTREAMER_CFLAGS_OTHER})
target_link_libraries(mixxx-videoengine-test PRIVATE
    ${VIDEO_ENGINE_STATIC_PLUGINS}
    ${GSTREAMER_LIBRARIES}
)
target_link_directories(mixxx-videoengine-test PRIVATE ${GSTREAMER_LIBRARY_DIRS})
target_compile_features(mixxx-videoengine-test PRIVATE cxx_std_17)
if(APPLE)
  # GStreamer's osxvideo/applemedia (avfvideosrc) and opengl plugins, plus
  # AppKit-based main-loop integration, need these; harmless to link
  # unconditionally rather than re-deriving the exact set per element.
  target_link_libraries(mixxx-videoengine-test PRIVATE
      "-framework Cocoa"
      "-framework CoreFoundation"
      "-framework CoreServices"
      "-framework Security"
      "-framework Accelerate"
      "-framework OpenGL"
      "-framework CoreVideo"
      "-framework CoreMedia"
      "-framework AVFoundation"
      "-framework QuartzCore"
      "-framework IOSurface"
      "-framework Metal"
      "-framework VideoToolbox"
  )
endif()

# Stage 3a: proves the last unverified piece of the architecture -- that
# appsink frames can actually be rendered inside a live QQuickWindow, not
# just extracted as bytes (which mixxx-videoengine-test's appsink check
# above already proved). Needs only the same small plugin set as an
# appsink-fed pipeline (no compositor/gl/camera elements), plus Qt.
add_executable(mixxx-videoengine-qml-bridge-test videoengine_qml_bridge_cli.cpp)
# GstVideoItem (Q_OBJECT) is defined directly in the .cpp with a manual
# #include "*.moc" at the bottom -- needs AUTOMOC explicitly since this
# target doesn't inherit it from anywhere else.
set_target_properties(mixxx-videoengine-qml-bridge-test PROPERTIES AUTOMOC ON)
target_include_directories(mixxx-videoengine-qml-bridge-test PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_compile_options(mixxx-videoengine-qml-bridge-test PRIVATE ${GSTREAMER_CFLAGS_OTHER})
target_link_libraries(mixxx-videoengine-qml-bridge-test PRIVATE
    "${VIDEO_ENGINE_PLUGIN_DIR}/libgstcoreelements.a"
    "${VIDEO_ENGINE_PLUGIN_DIR}/libgstvideotestsrc.a"
    "${VIDEO_ENGINE_PLUGIN_DIR}/libgstvideoconvertscale.a"
    "${VIDEO_ENGINE_PLUGIN_DIR}/libgstapp.a"
    ${GSTREAMER_LIBRARIES}
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Quick
)
target_link_directories(mixxx-videoengine-qml-bridge-test PRIVATE ${GSTREAMER_LIBRARY_DIRS})
target_compile_features(mixxx-videoengine-qml-bridge-test PRIVATE cxx_std_17)
if(APPLE)
  target_link_libraries(mixxx-videoengine-qml-bridge-test PRIVATE
      "-framework Cocoa"
      "-framework CoreFoundation"
      "-framework CoreServices"
      "-framework Security"
  )
endif()

# Stage 3d-1: proves compositor pad alpha genuinely drives the blend
# between two sources before wiring it to the real [Master]/crossfader CO
# in VideoEngineManager (Stage 3d-2).
add_executable(mixxx-videoengine-crossfade-test videoengine_crossfade_cli.cpp)
target_include_directories(mixxx-videoengine-crossfade-test PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_compile_options(mixxx-videoengine-crossfade-test PRIVATE ${GSTREAMER_CFLAGS_OTHER})
target_link_libraries(mixxx-videoengine-crossfade-test PRIVATE
    "${VIDEO_ENGINE_PLUGIN_DIR}/libgstcoreelements.a"
    "${VIDEO_ENGINE_PLUGIN_DIR}/libgstcompositor.a"
    "${VIDEO_ENGINE_PLUGIN_DIR}/libgstvideotestsrc.a"
    "${VIDEO_ENGINE_PLUGIN_DIR}/libgstvideoconvertscale.a"
    "${VIDEO_ENGINE_PLUGIN_DIR}/libgstapp.a"
    ${GSTREAMER_LIBRARIES}
)
target_link_directories(mixxx-videoengine-crossfade-test PRIVATE ${GSTREAMER_LIBRARY_DIRS})
target_compile_features(mixxx-videoengine-crossfade-test PRIVATE cxx_std_17)
if(APPLE)
  target_link_libraries(mixxx-videoengine-crossfade-test PRIVATE
      "-framework Cocoa"
      "-framework CoreFoundation"
      "-framework CoreServices"
      "-framework Security"
  )
endif()

# Stage 3i investigated gst_element_set_locked_state as a way to pause one
# branch of the shared crossfade pipeline independently -- confirmed PAUSE
# works reliably, but RESUME does not (every element reports a clean
# PLAYING state yet the downstream queue never receives new buffers again,
# across several different unlock orderings). See
# milestone_12_video_spec_addendum.md's Stage 3i writeup for the full
# investigation; the standalone repro that proved this
# (videoengine_lockedstate_cli.cpp) was removed rather than kept as a
# permanently-failing build target.
