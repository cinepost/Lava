#include <limits>
#include <memory>
#include <chrono>
#include <array>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include "prman/ndspy.h"
#include <SDL.h>

#include "sdl_opengl_window.h"

#ifdef __cplusplus
extern "C" {
#endif

static std::vector< float > g_pixels;
static std::unique_ptr<SDLOpenGLWindow> window;
static std::chrono::time_point<std::chrono::system_clock> g_start, g_end;

static int g_channels;
static PtDspyUnsigned32 g_width;
static PtDspyUnsigned32 g_height;


PtDspyError processEvents();

PtDspyError DspyImageOpen(PtDspyImageHandle *image_h,
  const char *,const char *filename,
  int width,int height,
  int ,const UserParameter *,
  int formatCount,PtDspyDevFormat *format, PtFlagStuff *flagstuff)
{
  PtDspyError ret;
  
  // MyImageType image;
  
  flagstuff->flags &= ~PkDspyFlagsWantsScanLineOrder;
  flagstuff->flags |= PkDspyBucketOrderSpaceFill;
  flagstuff->flags |= PkDspyFlagsWantsNullEmptyBuckets;
  
  // stupidity checking
  if (0 == width) g_width = 640;
  if (0 == height) g_height = 480;

  ret = PkDspyErrorNone;

  g_width = width;
  g_height = height;
  g_channels = formatCount;
  g_pixels.resize(width*height*g_channels,0.4f);

  // shuffle format so we always write out RGB[A]
  std::array<std::string,4> chan = { {"r", "g", "b", "a"} };
  for ( int i=0; i<formatCount; i++ )
    for( int j=0; j<4; ++j )
      if( std::string(format[i].name)==chan[size_t(j)] && i!=j )
        std::swap(format[j],format[i]);

  std::string name = "SDL Display: ";
  name += filename;

  SDLOpenGLWindow *sdl_window = new SDLOpenGLWindow(name.c_str(), 0, 0, width, height, g_channels);
  if(!sdl_window)
    return PkDspyErrorNoResource;

  window.reset( sdl_window );
  g_start = std::chrono::system_clock::now();

  *image_h = (void *)new float(1);

  return ret;
}

PtDspyError DspyImageQuery(PtDspyImageHandle,PtDspyQueryType querytype, int datalen,void *data) {
  PtDspyError ret;

  ret = PkDspyErrorNone;

  if (datalen > 0 &&  data!=nullptr) {
    switch (querytype) {
      case PkOverwriteQuery: {
        PtDspyOverwriteInfo overwriteInfo;
        size_t size = sizeof(overwriteInfo);
        overwriteInfo.overwrite = 1;
        overwriteInfo.interactive = 0;
        memcpy(data, &overwriteInfo, size);
        break; }

      case PkSizeQuery : {
        PtDspySizeInfo overwriteInfo;
        size_t size = sizeof(overwriteInfo);
        overwriteInfo.width = g_width;
        overwriteInfo.height = g_height;
        overwriteInfo.aspectRatio=1.0f;
        memcpy(data, &overwriteInfo, size);
        break; }

      case PkRedrawQuery : {
        PtDspyRedrawInfo overwriteInfo;

        size_t size = sizeof(overwriteInfo);
        overwriteInfo.redraw = 0;
        memcpy(data, &overwriteInfo, size);
        break; }

      case PkRenderingStartQuery :
        std::cerr<<"Start rendering\n";
        break;

      case PkSupportsCheckpointing:
      case PkNextDataQuery:
      case PkGridQuery:
      case PkMultiResolutionQuery:
      case PkQuantizationQuery :
      case PkMemoryUsageQuery :
      case PkElapsedTimeQuery :
      case PkPointCloudQuery :
        ret = PkDspyErrorUnsupported;
        break;
    }
  } else {
    ret = PkDspyErrorBadParams;
  }
  return ret;
}

PtDspyError DspyImageData(PtDspyImageHandle ,int xmin,int xmax,int ymin,int ymax,int entrysize,const unsigned char *data) {
  int oldx;
  oldx = xmin;

  for (;ymin < ymax; ++ymin) {
    for (xmin = oldx; xmin < xmax; ++xmin) {
      const float *ptr = reinterpret_cast<const float*>(data);
      size_t offset =  g_width * g_channels * ymin  + xmin * g_channels;
      if(g_channels == 4) {
        g_pixels[ offset + 0 ]=ptr[0];
        g_pixels[ offset + 1 ]=ptr[1];
        g_pixels[ offset + 2 ]=ptr[2];
        g_pixels[ offset + 3 ]=ptr[3];
      } else {
        g_pixels[ offset + 0 ]=ptr[0];
        g_pixels[ offset + 1 ]=ptr[1];
        g_pixels[ offset + 2 ]=ptr[2];
      }
    data += entrysize;
    }
  }

  //BOOST_LOG_TRIVIAL(debug) << "SDLDisplay data recieved with " << g_channels << " channels";

  // copy data to image and draw
  window->updateImage(&g_pixels[0]);
  window->draw();
  // see if we had a key event
  return processEvents();
}

PtDspyError DspyImageClose(PtDspyImageHandle image_h) {
  std::cerr<<"Rendering Complete ESC to Quit\n";
  g_end = std::chrono::system_clock::now();

  std::chrono::duration<double> elapsed_seconds = g_end-g_start;
  std::time_t end_time = std::chrono::system_clock::to_time_t(g_end);

  std::cout << "finished rendering at " << std::ctime(&end_time) << "elapsed time: " << elapsed_seconds.count() << "s\n";
  // go into window process loop until quit
  PtDspyError quit=PkDspyErrorNone;
  while(quit != PkDspyErrorCancel) {
    quit = processEvents();
    window->draw();
  }// end of quit

  window.reset( nullptr );

  delete image_h;

  return PkDspyErrorNone;//quit;
}


PtDspyError processEvents() {
  static float s_xPos=0.0f;
  static float s_yPos=0.0f;
  static bool s_mousePressed=false;
  constexpr float scaleStep=0.05f;
  SDL_Event event;
  PtDspyError ret = PkDspyErrorNone;

  while (window->pollEvent(event)) {
    // Forward to Imgui
    ImGui_ImplSDL2_ProcessEvent(&event);

    switch (event.type) {
      case SDL_QUIT :
        ret = PkDspyErrorCancel;
        break;
      case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          window->resizeWindow(event.window.data1, event.window.data2);
        }
        break;
      // keyup event
      case SDL_KEYUP:
        break;
      // now we look for a keydown event
      case SDL_KEYDOWN:
        switch( event.key.keysym.sym ) {
          case SDLK_ESCAPE : ret=PkDspyErrorCancel;  break;
          case SDLK_EQUALS :
          case SDLK_PLUS :
            window->setScale(window->scale()+scaleStep);
            break;
          case SDLK_MINUS :
            window->setScale( window->scale()-scaleStep);
            break;
          case SDLK_UP :
            s_yPos+=0.1;
            window->setPosition(s_xPos,s_yPos);
            break;
          case SDLK_DOWN :
            s_yPos-=0.1;
            window->setPosition(s_xPos,s_yPos);
            break;

          case SDLK_LEFT :
            s_xPos-=0.1;
            window->setPosition(s_xPos,s_yPos);
            break;
          case SDLK_RIGHT :
            s_xPos+=0.1;
            window->setPosition(s_xPos,s_yPos);
            break;

          case SDLK_SPACE :
            s_xPos=0.0f;
            s_yPos=0.0f;
            window->reset();
            break;

          case SDLK_1 : window->setRenderMode(SDLOpenGLWindow::RenderMode::ALL); break;
          case SDLK_2 : window->setRenderMode(SDLOpenGLWindow::RenderMode::RED); break;
          case SDLK_3 : window->setRenderMode(SDLOpenGLWindow::RenderMode::GREEN); break;
          case SDLK_4 : window->setRenderMode(SDLOpenGLWindow::RenderMode::BLUE); break;
          case SDLK_5 : window->setRenderMode(SDLOpenGLWindow::RenderMode::ALPHA); break;
          case SDLK_6 : window->setRenderMode(SDLOpenGLWindow::RenderMode::GREY); break;
          case SDLK_LEFTBRACKET : 
            window->setGamma(window->gamma()-0.1f); 
            break;
          case SDLK_RIGHTBRACKET : 
            window->setGamma(window->gamma()+0.1f);
            break;
          case SDLK_0 : 
            window->setExposure(window->exposure()+0.1f);
            break;
          case SDLK_9 : 
            window->setExposure(window->exposure()-0.1f);
            break;
          case SDLK_r :
            window->reset();
            window->setRenderMode(SDLOpenGLWindow::RenderMode::ALL);
            window->setGamma(1.0f);
            window->setExposure(0.0f);
            break;
          case SDLK_h :
            window->showHelp();
            break;
          case SDLK_d :
            window->showHUD();
            break;
          default : 
            break;
        } // end of key process
        break;

        case SDL_MOUSEBUTTONDOWN :
          if(event.button.button == SDL_BUTTON_RIGHT) {
            s_mousePressed=true;
          }
        break;
      case SDL_MOUSEBUTTONUP :
      if(event.button.button == SDL_BUTTON_RIGHT)
      {
        s_mousePressed=false;
      }
      break;
      case SDL_MOUSEMOTION :
        if(s_mousePressed==true)
        {
          float diffx=0.0f;
          float diffy=0.0f;
          if(event.motion.xrel >0)
            diffx=0.01f;
          else if(event.motion.xrel <0)
            diffx=-0.01f;
          if(event.motion.yrel >0)
            diffy=-0.01f;
          else if(event.motion.yrel <0)
            diffy=+0.01f;

          s_xPos+=diffx;
          s_yPos+=diffy;
          window->setPosition(s_xPos,s_yPos);
        }
      break;
      case SDL_MOUSEWHEEL :
      {
        //auto delta=event.motion.x;
        if (event.wheel.y > 0)
          window->setScale(window->scale()+scaleStep);
        else if (event.wheel.y < 0)
          window->setScale(window->scale()-scaleStep);

      break;
      }

      default : break;

    } // end of event switch
  } // end while poll event
 return ret;
}


#ifdef __cplusplus
}
#endif