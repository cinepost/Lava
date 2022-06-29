#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include "sdl_opengl_window.h"


SDLOpenGLWindow::SDLOpenGLWindow(const std::string &_name, int _x, int _y,int _width, int _height, int _ppp, GLenum pixelType, GLenum pixelFormat, GLenum texFormat) {
  m_name=_name;
  m_x=_x;
  m_y=_y;
  m_width=_width;
  m_height=_height;

  init();

  m_pixelFormat = pixelFormat;
  m_pixelType = pixelType;
  m_texFormat = texFormat;
}

SDLOpenGLWindow::~SDLOpenGLWindow() {
  ImGuiCleanup();
  SDL_Quit();
}

void SDLOpenGLWindow::init() {
  if(SDL_Init(SDL_INIT_EVERYTHING) < 0){
    printf("SDL_Init failed: %s\n", SDL_GetError());
    ErrorExit("Could Not Init Everything");
  }

  m_window = SDL_CreateWindow(m_name.c_str(), m_x, m_y, m_width,m_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if(!m_window) {
    printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
    ErrorExit("Could not create Window");
  }

  createGLContext();
  
  // set this first so that new driver features are included.
  glewExperimental = GL_TRUE;
  // now init glew
  GLenum err = glewInit();
  // error check
  if (GLEW_OK != err) {
   std::cerr << "GLEW Error: "<< glewGetErrorString(err) << "\n";
  }

  ImGuiSetup();

  createSurface();
  glEnable(GL_FRAMEBUFFER_SRGB);
}

void SDLOpenGLWindow::updateImage(const void *_image) {
  glTexImage2D(GL_TEXTURE_2D, 0, m_texFormat, m_width, m_height, 0, m_pixelFormat, m_pixelType, _image);
}

void NGLCheckGLError( const std::string  &_file, const int _line ) noexcept {
 auto errNum = glGetError();
  while (errNum != GL_NO_ERROR) {

    std::string str;
    switch(errNum) {
      case GL_INVALID_ENUM : str="GL_INVALID_ENUM error"; break;
      case GL_INVALID_VALUE : str="GL_INVALID_VALUE error"; break;
      case GL_INVALID_OPERATION : str="GL_INVALID_OPERATION error"; break;
      case GL_OUT_OF_MEMORY : str="GL_OUT_OF_MEMORY error"; break;
      case GL_INVALID_FRAMEBUFFER_OPERATION : str="GL_INVALID_FRAMEBUFFER_OPERATION error";  break;
      default : break;
    }
    if(errNum !=GL_NO_ERROR)
    {
      std::cerr<<"GL error "<< str<<" line : "<<_line<<" file : "<<_file<<"\n";
    }
    errNum = glGetError();

   }
}

void printInfoLog(const GLuint &_obj , GLenum _mode=GL_COMPILE_STATUS  ) {
  GLint infologLength = 0;
  GLint charsWritten  = 0;
  char *infoLog;

  glGetShaderiv(_obj, GL_INFO_LOG_LENGTH,&infologLength);
  
  if(infologLength > 0)
  {
    infoLog = new char[infologLength];
    glGetShaderInfoLog(_obj, infologLength, &charsWritten, infoLog);

    std::cerr<<infoLog<<std::endl;
    delete [] infoLog;
    glGetShaderiv(_obj, _mode,&infologLength);
    if( infologLength == GL_FALSE)
    {
      std::cerr<<"Shader compile failed or had warnings \n";
      exit(EXIT_FAILURE);
    }
  }
}

void SDLOpenGLWindow::draw() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame(m_window);
  ImGui::NewFrame();

  // Help window
  if(mShowHelp) {
  ImGui::Begin("Help", &mShowHelp, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("+/-   - Scale view ");
    ImGui::Text("Space - Reset view ");
    ImGui::Text("1     - View all channels (default)");
    ImGui::Text("2     - View Red channel ");
    ImGui::Text("3     - View Green channel ");
    ImGui::Text("4     - View Blue channel ");
    ImGui::Text("5     - View Alpha channel ");
    ImGui::Text("6     - View Gray scale ");

    ImGui::Text("[/]   - Change gamma ");
    ImGui::Text("0/9   - Change exposure ");

    ImGui::Text("b     - Black background (default) ");
    ImGui::Text("c     - Checkerboard background");
    ImGui::Text("l     - Color background");

    ImGui::Text("r     - Reset all ");
    ImGui::Text("d     - Show/hide HUD ");
    ImGui::Text("h     - show/hide this help window ;) ");
    ImGui::Text("ESC   - Exit ");
  ImGui::End();
  }

  // Simple HUD (fixed transparent window)
  if(mShowHUD) {
  ImGui::SetNextWindowBgAlpha(0.5f);
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2((float)m_width, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
  ImGui::Begin("HUD", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    switch (mRenderMode) {
      case RenderMode::RED:
        ImGui::Text("Display: Red");
        break;
      case RenderMode::GREEN:
        ImGui::Text("Display: Green");
        break;
      case RenderMode::BLUE:
        ImGui::Text("Display: Blue");
        break;
      case RenderMode::GREY:
        ImGui::Text("Display: Grey");
        break;
      case RenderMode::ALPHA:
        ImGui::Text("Display: Alpha");
        break;
      case RenderMode::ALL:
      default:
        ImGui::Text("Display: RGB");
        break;
    }
    ImGui::Text("Exposure: %f", m_exposure);
    ImGui::Text("Gamma: %f", m_gamma);
    ImGui::Text("Scale: %f", m_scale);
  ImGui::End();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  }

  glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  swapWindow();
}

void SDLOpenGLWindow::createSurface() {
  makeCurrent();
  glGenVertexArrays(1, &m_vao);
  glBindVertexArray(m_vao);

  float vertices[] = {
    -1.f,  1.f, 0.f, 0.f,
     1.f,  1.f, 1.f, 0.f,
     1.f, -1.f, 1.f, 1.f,

     1.f, -1.f, 1.f, 1.f,
    -1.f, -1.f, 0.f, 1.f,
    -1.f,  1.f, 0.f, 0.f
  };

  glGenBuffers(1, &m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  const std::string vertSource =R"(
    #version 330
    layout(location=0) in vec2 position;
    layout(location=1) in vec2 coordinate;
    uniform vec2 translation;
    uniform float scale;
    out vec2 screenCoord;
    void main()
    {
       screenCoord = coordinate;
       gl_Position = vec4((position + translation) * scale, 0.0, 1.0);
    }
    )";


  const std::string fragSource =R"(
    #version 330
    in vec2 screenCoord;
    uniform sampler2D tex;
    uniform float gamma;
    uniform float exposureLevel;
    
    layout(location=0) out vec4 outColor;
    
    uniform int displayMode;
    uniform int backgroundMode;
    uniform int showFalseColors;
    uniform int frameNumber;

    vec3 exposure(vec3 colour, float relative_fstop) {
       return colour * pow(2.0,relative_fstop);
    }

    vec3 checkerPattern(vec2 iCoord, vec2 iSize, vec3 color1, vec3 color2 ) {
      vec2 uv = fract( iCoord / iSize);
      uv -= 0.5; // moving the coordinate system to middle of screen
      float m = step(uv.x * uv.y, 0.);
      return color1 * (1. - m) + color2 * m;
    }

    
    void main() {
      vec4 texDispColor;
      vec4 texBaseColor = texture(tex, screenCoord);
      switch(displayMode) {
        case 1 : texDispColor=vec4(texBaseColor.r,0,0,texBaseColor.a); break; // red
        case 2 : texDispColor=vec4(0,texBaseColor.g,0,texBaseColor.a); break; // green
        case 3 : texDispColor=vec4(0,0,texBaseColor.b,texBaseColor.a); break; // blue
        case 4 : texDispColor=vec4(texBaseColor.a,texBaseColor.a,texBaseColor.a,1); break; // alpha are greyscale
        case 5 : texDispColor.rgb = vec3(dot(texBaseColor.rgb, vec3(0.299, 0.587, 0.114))); break; // monochrome image (lightness)
        case 0 : texDispColor=texBaseColor; break; // full rgba image
      }
    
      texDispColor.rgb = pow(texDispColor.rgb, vec3(1.0/gamma));
      texDispColor.rgb = exposure(texDispColor.rgb,exposureLevel);
    
      if (showFalseColors == 1) {
        switch(displayMode) {
          case 0: // rgb display
          case 5: // greyscale display
            if ((texBaseColor.r < 0.0) || (texBaseColor.g < .0) || (texBaseColor.g <.0)) {
              float u = screenCoord.x + frameNumber * .01;
              u = u - floor(u);
              texDispColor.rgb = vec3(.65*cos(6.283*(u+vec3(0.,-.33333,.33333)))+.65);
            }
            break;
          case 1: // red channel display
            if (texBaseColor.r < 0.0) {
              texDispColor.rgb = vec3(0, 0, 1);
            }
            break;
          case 2:  // green channel display
            if (texBaseColor.g < 0.0) {
              texDispColor.rgb = vec3(1, 0, 0);
            }
            break;
          case 3: // blue channel display
            if (texBaseColor.b < 0.0) {
              texDispColor.rgb = vec3(0, 1, 0);
            }
            break;
          default:
            break;
        }
      }

      switch(backgroundMode) {
        case 0: // Black background
          outColor.rgb = texDispColor.rgb;
          outColor.a = 1.0f;
          break;
        case 1: // Color background
          outColor.rgb = vec3(0.5, 1.0, 0.0);
          outColor.a = 1.0f;
          break;
        case 2: // Checkerboard background
        default:
          vec3 checkerColor = checkerPattern(gl_FragCoord.xy , vec2(16), vec3(0.5, 0.5, 0.5), vec3(0.25, 0.25, 0.25));
          outColor.rgb = checkerColor * (1.0 - texBaseColor.a) + texDispColor.rgb;
          outColor.a = 1.0f;
          break;
      }
    }
    )";

  const GLchar* shaderSource=vertSource.c_str();


  auto vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &shaderSource, NULL);
  glCompileShader(vertexShader);
  printInfoLog(vertexShader);

  shaderSource=fragSource.c_str();
  auto fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &shaderSource, NULL);
  glCompileShader(fragmentShader);
  printInfoLog(fragmentShader);

  m_shaderProgram = glCreateProgram();
  glAttachShader(m_shaderProgram, vertexShader);
  glAttachShader(m_shaderProgram, fragmentShader);
  glLinkProgram(m_shaderProgram);
  printInfoLog(vertexShader,GL_LINK_STATUS);

  glUseProgram(m_shaderProgram);

  auto attrib = glGetAttribLocation(m_shaderProgram, "position");
  glVertexAttribPointer(attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
  glEnableVertexAttribArray(attrib);
  attrib = glGetAttribLocation(m_shaderProgram, "coordinate");
  glVertexAttribPointer(attrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(attrib);

  m_translateUniform = glGetUniformLocation(m_shaderProgram, "translation");
  glUniform2f(m_translateUniform, m_xPos,m_yPos);

  m_scaleUniform = glGetUniformLocation(m_shaderProgram, "scale");
  glUniform1f(m_scaleUniform, m_scale);
  m_modeUniform = glGetUniformLocation(m_shaderProgram, "displayMode");
  glUniform1i(m_modeUniform, to_int(mRenderMode));

  m_backgroundModeUniform = glGetUniformLocation(m_shaderProgram, "backgroundMode");
  glUniform1i(m_backgroundModeUniform, to_int(mBackgroundMode));

  m_showFalseColorsUniform = glGetUniformLocation(m_shaderProgram, "showFalseColors");
  glUniform1i(m_showFalseColorsUniform, mShowFalseColors);

  m_gammaUniform = glGetUniformLocation(m_shaderProgram, "gamma");
  glUniform1f(m_gammaUniform, m_gamma);

  m_exposureUniform = glGetUniformLocation(m_shaderProgram, "exposureLevel");
  glUniform1f(m_exposureUniform, m_exposure);

  m_frameNumberUniform = glGetUniformLocation(m_shaderProgram, "frameNumber");
  glUniform1i(m_frameNumberUniform, mFrameNumber);

  glGenTextures(1, &m_texture);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, m_texFormat, m_width, m_height, 0, m_pixelFormat, m_pixelType, NULL);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}


void SDLOpenGLWindow::createGLContext() {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);

  //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,1);
  //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,4);

  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

  m_glContext = SDL_GL_CreateContext(m_window);
  if(!m_glContext) {
    printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
  }
}

void SDLOpenGLWindow::swapWindow() {
  glUniform1i(m_frameNumberUniform, mFrameNumber);
  mFrameNumber ++;
  if(mFrameNumber > 500) mFrameNumber = 0;
  SDL_GL_SwapWindow(m_window);
}

void SDLOpenGLWindow::resizeWindow(int width, int height) {
  m_width = width;
  m_height = height;
  glViewport(0, 0, m_width, m_height);
}

int SDLOpenGLWindow::pollEvent(SDL_Event &_event) {
  makeCurrent();
  return SDL_PollEvent(&_event);
}

void SDLOpenGLWindow::setScale(float _f) {
  m_scale=_f;
  m_scale=std::min(20.0f, std::max(0.01f, m_scale));
  glUniform1f(m_scaleUniform, m_scale);
  draw();
}

void SDLOpenGLWindow::reset() {
  m_scale=1.0f;
  m_xPos=0.0f;
  m_yPos=0.0f;
  glUniform1f(m_scaleUniform, m_scale);
  glUniform2f( m_translateUniform,m_xPos,m_yPos );
  draw();
}

void SDLOpenGLWindow::setPosition(float _x, float _y) {
  m_xPos=_x;
  m_yPos=_y;
  glUniform2f( m_translateUniform,m_xPos,m_yPos );
  draw();
}

void SDLOpenGLWindow::ImGuiSetup() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer bindings
  // window is the SDL_Window*
  // context is the SDL_GLContext
  ImGui_ImplSDL2_InitForOpenGL(m_window, m_glContext);
  ImGui_ImplOpenGL3_Init();
}

void SDLOpenGLWindow::ImGuiCleanup() const {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}

void SDLOpenGLWindow::ErrorExit(const std::string &_msg) const {
  std::cerr<<_msg<<'\n';
  std::cerr<<SDL_GetError()<<'\n';

  ImGuiCleanup();
  SDL_Quit();
  
  exit(EXIT_FAILURE);
}

void SDLOpenGLWindow::setRenderMode(RenderMode _m) {
  mRenderMode = _m;
  switch(static_cast<int>(_m)) {
    case 0 : 
      glUniform1i(m_modeUniform,0); 
      break;
    case 1 : 
      glUniform1i(m_modeUniform,1); 
      break;
    case 2 : 
      glUniform1i(m_modeUniform,2); 
      break;
    case 3 : 
      glUniform1i(m_modeUniform,3); 
      break;
    case 4 : 
      glUniform1i(m_modeUniform,4); 
      break;
    case 5 :
    default: 
      glUniform1i(m_modeUniform,5); 
      break;
  }
}

void SDLOpenGLWindow::setBackgroundMode(BackgroundMode _m) {
  mBackgroundMode = _m;
  switch(static_cast<int>(_m)) {
    case 0 : 
      glUniform1i(m_backgroundModeUniform,0); 
      break;
    case 1 : 
      glUniform1i(m_backgroundModeUniform,1); 
      break;
    case 2 :
    default: 
      glUniform1i(m_backgroundModeUniform,2); 
      break;
  }
}

void SDLOpenGLWindow::showFalseColors(bool m) { 
  mShowFalseColors = m;
  if (mShowFalseColors) {
    glUniform1i(m_showFalseColorsUniform, true);
  }else{
    glUniform1i(m_showFalseColorsUniform, false);
  }
}

void SDLOpenGLWindow::setGamma(float _g) {
  m_gamma=_g;
  m_gamma=std::min(10.0f, std::max(0.0f, m_gamma));

  glUniform1f(m_gammaUniform,m_gamma);
  draw();
}

void SDLOpenGLWindow::setExposure(float _e) {
  m_exposure=_e;
  m_exposure=std::min(10.0f, std::max(-10.0f, m_exposure));

  glUniform1f(m_exposureUniform,m_exposure);
  draw();
}