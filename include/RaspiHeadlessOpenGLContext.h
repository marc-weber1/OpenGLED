#ifndef RASPI_HEADLESS_OPENGL_CONTEXT_H
#define RASPI_HEADLESS_OPENGL_CONTEXT_H

#include <optional>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <gbm.h>

class RaspiHeadlessOpenGLContext
{
private:
    int width, height;
    
    int drm_fd;
    gbm_device* gbm;
    gbm_surface* surface_gbm;

    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
public:
    RaspiHeadlessOpenGLContext(int width, int height) : width(width), height(height) {}

    bool Initialize();
    void MakeCurrent();

    ~RaspiHeadlessOpenGLContext();
};

#endif