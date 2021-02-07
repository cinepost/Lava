#ifndef SDL_OPENGL_WINDOW_H
#define SDL_OPENGL_WINDOW_H

#include <string>

#ifdef __linux__
  #include <GL/glew.h>
  #include <GL/gl.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_opengl3.h"

class SDLOpenGLWindow {
  public:
    enum class RenderMode : int {
        ALL = 0, 
        RED, 
        GREEN, 
        BLUE, 
        ALPHA, 
        GREY
    };

  public:
    SDLOpenGLWindow(const std::string &_name, int _x, int _y, int _width, int _height, int _ppp);
    ~SDLOpenGLWindow();

    void makeCurrent() { SDL_GL_MakeCurrent(m_window,m_glContext);}
    void swapWindow() { SDL_GL_SwapWindow(m_window); }

    void resizeWindow(int width, int height);
    int pollEvent(SDL_Event &_event);
    void createSurface();
    void updateImage(const float* _image);
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
    void showHelp() { mShowHelp = !mShowHelp; }
    void showHUD() { mShowHUD = !mShowHUD; }

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
    GLint m_gammaUniform;
    GLint m_exposureUniform;

    void init();

    SDL_GLContext m_glContext;
    void createGLContext();

    void ErrorExit(const std::string &_msg) const;
    void ImGuiSetup();
    void ImGuiCleanup() const;

    SDL_Window *m_window;
    GLenum m_pixelFormat, m_texFormat;
    float m_scale=1.0f;
    float m_xPos=0.0f;
    float m_yPos=0.0f;
    float m_gamma=1.0f;
    float m_exposure=0.0f;
    RenderMode mRenderMode = RenderMode::ALL;

  private:
    bool  mShowHelp = false;
    bool  mShowHUD = true;
};

#endif // SDL_OPENGL_WINDOW_H