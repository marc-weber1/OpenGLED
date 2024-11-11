#include "RaspiHeadlessOpenGLContext.h"

#include <utility>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

const static EGLint CONTEXT_ATTRIBS[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

const static EGLint CONFIG_ATTRIBS[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_DEPTH_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
};

static const char *eglGetErrorStr()
{
    switch (eglGetError())
    {
    case EGL_SUCCESS:
        return "The last function succeeded without error.";
    case EGL_NOT_INITIALIZED:
        return "EGL is not initialized, or could not be initialized, for the "
               "specified EGL display connection.";
    case EGL_BAD_ACCESS:
        return "EGL cannot access a requested resource (for example a context "
               "is bound in another thread).";
    case EGL_BAD_ALLOC:
        return "EGL failed to allocate resources for the requested operation.";
    case EGL_BAD_ATTRIBUTE:
        return "An unrecognized attribute or attribute value was passed in the "
               "attribute list.";
    case EGL_BAD_CONTEXT:
        return "An EGLContext argument does not name a valid EGL rendering "
               "context.";
    case EGL_BAD_CONFIG:
        return "An EGLConfig argument does not name a valid EGL frame buffer "
               "configuration.";
    case EGL_BAD_CURRENT_SURFACE:
        return "The current surface of the calling thread is a window, pixel "
               "buffer or pixmap that is no longer valid.";
    case EGL_BAD_DISPLAY:
        return "An EGLDisplay argument does not name a valid EGL display "
               "connection.";
    case EGL_BAD_SURFACE:
        return "An EGLSurface argument does not name a valid surface (window, "
               "pixel buffer or pixmap) configured for GL rendering.";
    case EGL_BAD_MATCH:
        return "Arguments are inconsistent (for example, a valid context "
               "requires buffers not supplied by a valid surface).";
    case EGL_BAD_PARAMETER:
        return "One or more argument values are invalid.";
    case EGL_BAD_NATIVE_PIXMAP:
        return "A NativePixmapType argument does not refer to a valid native "
               "pixmap.";
    case EGL_BAD_NATIVE_WINDOW:
        return "A NativeWindowType argument does not refer to a valid native "
               "window.";
    case EGL_CONTEXT_LOST:
        return "A power management event has occurred. The application must "
               "destroy all contexts and reinitialise OpenGL ES state and "
               "objects to continue rendering.";
    default:
        break;
    }
    return "Unknown error!";
}

bool RaspiHeadlessOpenGLContext::Initialize()
{
    // Open the DRM device
    drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) {
        fprintf(stderr, "Failed to open DRM device\n");
        return false;
    }

    // Create a GBM device
    gbm = gbm_create_device(drm_fd);
    if (!gbm) {
        fprintf(stderr, "Failed to create GBM device\n");
        close(drm_fd);
        return false;
    }

    // Create a GBM surface
    surface_gbm = gbm_surface_create(gbm, width, height, GBM_FORMAT_ARGB8888, 
                                                 GBM_BO_USE_RENDERING);
    if (!surface_gbm) {
        fprintf(stderr, "Failed to create GBM surface\n");
        gbm_device_destroy(gbm);
        close(drm_fd);
        return false;
    }

    // Initialize EGL
    display = eglGetDisplay(gbm);
    if (display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display\n");
        gbm_surface_destroy(surface_gbm);
        gbm_device_destroy(gbm);
        close(drm_fd);
        return false;
    }

    int major, minor;

    if (eglInitialize(display, &major, &minor) == EGL_FALSE)
    {
        fprintf(stderr, "Failed to get EGL version! Error: %s\n",
                eglGetErrorStr());
        eglTerminate(display);
        gbm_surface_destroy(surface_gbm);
        gbm_device_destroy(gbm);
        close(drm_fd);
        return false;
    }

    printf("Initialized EGL version: %d.%d\n", major, minor);

    eglBindAPI(EGL_OPENGL_API);

    EGLint numConfigs;
    if (!eglChooseConfig(display, CONFIG_ATTRIBS, &config, 1, &numConfigs))
    {
        fprintf(stderr, "Failed to get EGL config! Error: %s\n",
                eglGetErrorStr());
        eglTerminate(display);
        gbm_surface_destroy(surface_gbm);
        gbm_device_destroy(gbm);
        close(drm_fd);
        return false;
    }

    context =
        eglCreateContext(display, config, EGL_NO_CONTEXT, CONTEXT_ATTRIBS);
    if (context == EGL_NO_CONTEXT)
    {
        fprintf(stderr, "Failed to create EGL context! Error: %s\n",
                eglGetErrorStr());
        eglTerminate(display);
        gbm_surface_destroy(surface_gbm);
        gbm_device_destroy(gbm);
        close(drm_fd);
        return false;
    }

    surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType) surface_gbm, nullptr);
    if (surface == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create EGL surface\n");
        eglDestroyContext(display, context);
        eglTerminate(display);
        gbm_surface_destroy(surface_gbm);
        gbm_device_destroy(gbm);
        close(drm_fd);
        return false;
    }

    return true;
}

void RaspiHeadlessOpenGLContext::MakeCurrent(){
    eglMakeCurrent(display, surface, surface, context);

    // Set GL Viewport size, always needed!
    glViewport(0, 0, width, height);

    // Get GL Viewport size and test if it is correct.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // viewport[2] and viewport[3] are viewport width and height respectively
    printf("GL Viewport size: %dx%d\n", viewport[2], viewport[3]);

    // Test if the desired width and height match the one returned by
    // glGetIntegerv
    if (width != viewport[2] || height != viewport[3])
    {
        fprintf(stderr, "Error! The glViewport/glGetIntegerv are not working! "
                        "EGL might be faulty!\n");
    }
}

RaspiHeadlessOpenGLContext::~RaspiHeadlessOpenGLContext(){
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    gbm_surface_destroy(surface_gbm);
    gbm_device_destroy(gbm);
    close(drm_fd);
}