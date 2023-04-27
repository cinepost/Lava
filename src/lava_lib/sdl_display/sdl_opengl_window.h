#ifndef SDL_OPENGL_WINDOW_H
#define SDL_OPENGL_WINDOW_H

#include <string>
#include <thread>
#include <type_traits>

// #ifdef __linux__
  #include <GL/glew.h>
  #include <GL/gl.h>
// #endif

// #include <SDL2/SDL.h>
// #include <SDL2/SDL_opengl.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_opengl3.h"


enum class RenderMode : int {
  ALL = 0, 
  RED, 
  GREEN, 
  BLUE, 
  ALPHA, 
  GREY
};

enum class BackgroundMode : int {
  NONE = 0, 
  COLOR, 
  CHECKER, 
};

class SDLOpenGLWindow {
  public:
    SDLOpenGLWindow(const std::string &_name, int _x, int _y, int _width, int _height, int _ppp, GLenum pixelType, GLenum pixelFormat, GLenum texFormat);
    ~SDLOpenGLWindow();

    void makeCurrent() { SDL_GL_MakeCurrent(m_window,m_glContext);}
    void swapWindow();

    void resizeWindow(int width, int height);
    int pollEvent(SDL_Event &_event);
    void createSurface();
    void updateImage(const void* _image);
    void draw();
    void setScale(float _f);
    float scale() const {return m_scale;}
    float gamma() const {return m_gamma;}
    float exposure() const {return m_exposure;}
    void setGamma(float _g);
    void setExposure(float _e);
    void reset();
    void setPosition(float _x, float _y);
    void setRenderMode(RenderMode _m);
    void setBackgroundMode(BackgroundMode _m);
    void showFalseColors(bool m);
    void showHelp() { mShowHelp = !mShowHelp; }
    void showHUD() { mShowHUD = !mShowHUD; }

    void stop();

  private :
    int m_width;
    int m_height;
    int m_x;
    int m_y;
    
	  std::string m_name;
    GLuint m_texture;
    GLuint m_shaderProgram;
    GLuint m_vbo;
    GLuint m_vao;
    GLint m_translateUniform;
    GLint m_scaleUniform;
    GLint m_modeUniform;
    GLint m_backgroundModeUniform; // checkerboard pattern default
    GLint m_gammaUniform;
    GLint m_exposureUniform;
    GLint m_showFalseColorsUniform;
    GLint m_frameNumberUniform;

    GLfloat mMaxAnisotropyValue;

    void init();

    SDL_GLContext   m_glContext;
    void createGLContext();

    void ErrorExit(const std::string &_msg) const;
    void ImGuiSetup();
    void ImGuiCleanup() const;

    SDL_Window *m_window;
    GLenum m_pixelFormat, m_texFormat, m_pixelType;

  public:
    float m_scale=1.0f;
    float m_xPos=0.0f;
    float m_yPos=0.0f;
    float m_gamma=1.0f;
    float m_exposure=0.0f;
    bool  mShowFalseColors = true;
    int   mFrameNumber = 0;
    RenderMode mRenderMode = RenderMode::ALL;
    BackgroundMode mBackgroundMode = BackgroundMode::CHECKER;

  private:
    bool  mShowHelp = false;
    bool  mShowHUD = true;
};

constexpr int to_int(RenderMode e) noexcept {
  return static_cast<std::underlying_type_t<RenderMode>>(e);
}

constexpr int to_int(BackgroundMode e) noexcept {
  return static_cast<std::underlying_type_t<RenderMode>>(e);
}

#endif // SDL_OPENGL_WINDOW_H