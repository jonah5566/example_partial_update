#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
#include <sys/resource.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <utils/Timers.h>

#include <WindowSurface.h>
#include <EGLUtils.h>

#include <../../include/EGL/eglext.h>

using namespace android;

int frameCount = 0;
int frameCountToChangeClearColor = 30;

float damageRegionTop = 0.6;
float damageRegionBottom = -0.6;
float damageRegionRight = 0.6;
float damageRegionLeft = -0.6;
float step = 0.02;

EGLint rects[4]; //Rainier:  {100, 150, 340, 660}
EGLint buffer_age = 0;

GLfloat clearColor[] = {0.2f, 0.0f, 0.5f, 1.0f};
GLfloat gTriangleVertices[] = {
    -0.5f, 0.6f,
    -0.6f, 0.4f,
    -0.4f, 0.4f
};

GLuint gProgram_destroy;
GLuint gvPositionHandle_destroy;

static const char gVertexShader[] = "attribute vec4 vPosition;\n"
    "void main() {\n"
    "  gl_Position = vPosition;\n"
    "}\n";

static const char gFragmentShader_destroy[] = "precision mediump float;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
    "}\n";

static void checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE) {
    if (returnVal != EGL_TRUE) {
        fprintf(stderr, "%s() returned %d\n", op, returnVal);
    }
    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error
            = eglGetError()) {
        fprintf(stderr, "after %s() eglError %s (0x%x)\n", op, EGLUtils::strerror(error),
                error);
    }
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error
            = glGetError()) {
        fprintf(stderr, "after %s() glError (0x%x)\n", op, error);
    }
}

int triangleMoveRight() {
    if (gTriangleVertices[0] + 0.1 < damageRegionRight) {
        gTriangleVertices[0] += step;
        gTriangleVertices[2] += step;
        gTriangleVertices[4] += step;
        return 1;
    }
    return 0;
}

int triangleMoveBottom() {
    if (gTriangleVertices[1] - 0.2 > damageRegionBottom) {
        gTriangleVertices[1] -= step;
        gTriangleVertices[3] -= step;
        gTriangleVertices[5] -= step;
        return 1;
    }
    return 0;
}

int triangleMoveLeft() {
    if (gTriangleVertices[0] - 0.1 > damageRegionLeft) {
        gTriangleVertices[0] -= step;
        gTriangleVertices[2] -= step;
        gTriangleVertices[4] -= step;
        return 1;
    }
    return 0;
}

int triangleMoveTop() {
    if (gTriangleVertices[1] < damageRegionTop) {
        gTriangleVertices[1] += step;
        gTriangleVertices[3] += step;
        gTriangleVertices[5] += step;
        return 1;
    }
    return 0;
}

GLuint loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    fprintf(stderr, "Could not compile shader %d:\n%s\n",
                            shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    fprintf(stderr, "Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

bool setupGraphics(int w, int h) {
    gProgram_destroy = createProgram(gVertexShader, gFragmentShader_destroy);

    if (!gProgram_destroy) {
        return false;
    }
    gvPositionHandle_destroy = glGetAttribLocation(gProgram_destroy, "vPosition");
    
    checkGlError("glGetAttribLocation");
    fprintf(stderr, "glGetAttribLocation(\"vPosition\") = %d\n", gvPositionHandle_destroy);

    glViewport(0, 0, w, h);
    checkGlError("glViewport");
    return true;
}

void changeClearColor() {
    clearColor[0] = (float)(rand() % 10) / 10;
    clearColor[1] = (float)(rand() % 10) / 10;
    clearColor[2] = (float)(rand() % 10) / 10;
}

void renderFrame() {
    eglQuerySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_BUFFER_AGE_KHR, &buffer_age);
    fprintf(stderr, "[DESTROY] Query buffer_age: %d\n", buffer_age);

    eglSetDamageRegionKHR(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), rects, 1);

    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glUseProgram(gProgram_destroy);
    checkGlError("glUseProgram");

    glVertexAttribPointer(gvPositionHandle_destroy, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
    checkGlError("glVertexAttribPointer");

    glEnableVertexAttribArray(gvPositionHandle_destroy);
    checkGlError("glEnableVertexAttribArray");
            
    glDrawArrays(GL_TRIANGLES, 0, 3);
    checkGlError("glDrawArrays");
}

int main(int argc, char** argv) {
    EGLBoolean returnValue;
    EGLConfig myConfig = {0};

    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLint s_configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE };
    EGLint majorVersion;
    EGLint minorVersion;
    EGLContext context;
    EGLSurface surface;
    EGLint w, h;

    EGLDisplay dpy;

    checkEglError("<init>");
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    checkEglError("eglGetDisplay");
    if (dpy == EGL_NO_DISPLAY) {
        printf("eglGetDisplay returned EGL_NO_DISPLAY.\n");
        return 0;
    }

    returnValue = eglInitialize(dpy, &majorVersion, &minorVersion);
    checkEglError("eglInitialize", returnValue);
    fprintf(stderr, "EGL version %d.%d\n", majorVersion, minorVersion);
    if (returnValue != EGL_TRUE) {
        printf("eglInitialize failed\n");
        return 0;
    }

    WindowSurface windowSurface;
    EGLNativeWindowType window = windowSurface.getSurface();
    returnValue = EGLUtils::selectConfigForNativeWindow(dpy, s_configAttribs, window, &myConfig);
    if (returnValue) {
        printf("EGLUtils::selectConfigForNativeWindow() returned %d", returnValue);
        return 0;
    }

    checkEglError("EGLUtils::selectConfigForNativeWindow");

    surface = eglCreateWindowSurface(dpy, myConfig, window, NULL);
    checkEglError("eglCreateWindowSurface");
    if (surface == EGL_NO_SURFACE) {
        printf("gelCreateWindowSurface failed.\n");
        return 0;
    }

    context = eglCreateContext(dpy, myConfig, EGL_NO_CONTEXT, context_attribs);
    checkEglError("eglCreateContext");
    if (context == EGL_NO_CONTEXT) {
        printf("eglCreateContext failed\n");
        return 0;
    }

    returnValue = eglMakeCurrent(dpy, surface, surface, context);
    checkEglError("eglMakeCurrent", returnValue);
    if (returnValue != EGL_TRUE) {
        return 0;
    }

    eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
    checkEglError("eglQuerySurface");
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
    checkEglError("eglQuerySurface");
    GLint dim = w < h ? w : h;
    
    rects[0] = (int) ((float)w * 0.125);
    rects[1] = (int) ((float)h * 0.125);
    rects[2] = (int) ((float)w * 0.75);
    rects[3] = (int) ((float)h * 0.75);
    
    fprintf(stderr, "Window dimensions: %d x %d\n", w, h);
    fprintf(stderr, "Damage Region: %d, %d, %d, %d\n", rects[0], rects[1], rects[2], rects[3]);

    if(!setupGraphics(w, h)) {
        fprintf(stderr, "Could not set up graphics.\n");
        return 0;
    }

    eglSurfaceAttrib(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED);

    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(dpy, surface);
    //usleep(1*1000*1000);
    
    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(dpy, surface);
    //usleep(1*1000*1000);
    
    glClearColor(0.0, 0.0, 1.0, 1.0);
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(dpy, surface);
    //usleep(1*1000*1000);

    int direction = 0;
    int needSleep = 0;

    for (;;) {

        frameCount++;

        switch (direction) {
            case 0:
                if (!triangleMoveRight()) {
                    direction = (direction + 1) % 4;
                }
                break;
            case 1:
                if (!triangleMoveBottom()) {
                    direction = (direction + 1) % 4;
                }
                break;
            case 2:
                if (!triangleMoveLeft()) {
                    direction = (direction + 1) % 4;
                }
                break;
            case 3:
                if (!triangleMoveTop()) {
                    direction = (direction + 1) % 4;
                }
                break;
            default:
                break;
        }

        renderFrame();
        eglSwapBuffers(dpy, surface);
        usleep(100*1000);

        if (frameCount >= frameCountToChangeClearColor) {
            frameCount = 0;
            changeClearColor();
            
        }
    }

    return 0;
}
