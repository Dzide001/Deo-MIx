// M12 Stage 7: extracts the native CGLContextObj underlying a QOpenGLContext
// on macOS, so GStreamer's GL elements can be given a GstGLContext wrapping
// the SAME native context Qt Quick's own RHI/OpenGL backend is using
// (gst_gl_context_new_wrapped()) -- the prerequisite for GStreamer and Qt
// Quick to share GL texture IDs with no CPU copy at all.
//
// This one extraction step needs Objective-C++ (NSOpenGLContext's
// CGLContextObj is an Objective-C property, not callable from plain C++) --
// kept isolated to this one small, platform-specific file rather than
// converting qmlvideopreviewglitem.cpp itself to .mm, matching this
// project's existing precedent for Apple-specific code (e.g.
// src/util/battery/batterymac.cpp, only built on APPLE via CMake).
//
// Verified against the actual installed Qt 6.8.3 headers before writing
// this (qopenglcontext_platform.h): QNativeInterface::QCocoaGLContext's
// nativeContext() returns NSOpenGLContext* (deprecated by Apple in favor of
// Metal, but still functional and still what Qt Quick's OpenGL RHI backend
// actually uses under the hood on this platform, since
// QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL) is forced
// globally in coreservices.cpp).

#import <AppKit/AppKit.h>

#include <QOpenGLContext>
#include <QtGui/qopenglcontext_platform.h>

quintptr mixxxGetNativeCglContext(QOpenGLContext* pContext) {
    if (!pContext) {
        return 0;
    }
    auto* pCocoaInterface = pContext->nativeInterface<QNativeInterface::QCocoaGLContext>();
    if (!pCocoaInterface) {
        return 0;
    }
    NSOpenGLContext* pNsContext = pCocoaInterface->nativeContext();
    if (!pNsContext) {
        return 0;
    }
    // CGLContextObj is what GStreamer's GST_GL_PLATFORM_CGL expects
    // (gst_gl_context_new_wrapped()'s `handle` parameter, a guintptr) --
    // NSOpenGLContext exposes it directly via this property.
    CGLContextObj cglContext = [pNsContext CGLContextObj];
    return reinterpret_cast<quintptr>(cglContext);
}
