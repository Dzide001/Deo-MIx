#pragma once

#include <QtGlobal>

class QOpenGLContext;

// M12 Stage 7: see qmlvideopreviewglitem_mac.mm's own comment. Returns 0 if
// the native context couldn't be extracted for any reason (null input,
// wrong platform interface, etc.) -- callers must treat 0 as "GL context
// sharing isn't available" and fall back accordingly, not crash.
quintptr mixxxGetNativeCglContext(QOpenGLContext* pContext);
