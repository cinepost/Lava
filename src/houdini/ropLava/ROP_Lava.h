#ifndef ROP_LAVA_H_
#define ROP_LAVA_H_

#include <vector>

#include <ROP/ROP_Node.h>
#include <TIL/TIL_Sequence.h>
#include <IMG/IMG_File.h>
#include <IMG/IMG_Stat.h>
#include <IMG/IMG_TileDevice.h>
#include <IMG/IMG_TileSocket.h>


#define STR_PARM(name, idx, vi, t) \
                { evalString(str, name, &ifdIndirect[idx], vi, t); }
#define INT_PARM(name, idx, vi, t) \
                { return evalInt(name, &ifdIndirect[idx], vi, t); }
#define STR_SET(name, idx, vi, t) \
                { setString(str, name, ifdIndirect[idx], vi, t); }
#define STR_GET(name, idx, vi, t) \
                { evalStringRaw(str, name, &ifdIndirect[idx], vi, t); }

class OP_TemplatePair;
class OP_VariablePair;
//namespace HDK_Sample {
class ROP_Lava : public ROP_Node {
public:
    /// Provides access to our parm templates.
    static OP_TemplatePair      *getTemplatePair();
    /// Provides access to our variables.
    static OP_VariablePair      *getVariablePair();
    /// Creates an instance of this node.
    static OP_Node              *myConstructor(OP_Network *net, const char*name, OP_Operator *op);
protected:
             ROP_Lava(OP_Network *net, const char *name, OP_Operator *entry);
    virtual ~ROP_Lava();
    /// Called at the beginning of rendering to perform any intialization 
    /// necessary.
    /// @param  nframes     Number of frames being rendered.
    /// @param  s           Start time, in seconds.
    /// @param  e           End time, in seconds.
    /// @return             True of success, false on failure (aborts the render).
    virtual int                  startRender(int nframes, fpreal s, fpreal e);
    /// Called once for every frame that is rendered.
    /// @param  time        The time to render at.
    /// @param  boss        Interrupt handler.
    /// @return             Return a status code indicating whether to abort the
    ///                     render, continue, or retry the current frame.
    virtual ROP_RENDER_CODE      renderFrame(fpreal time, UT_Interrupt *boss);
    /// Called after the rendering is done to perform any post-rendering steps
    /// required.
    /// @return             Return a status code indicating whether to abort the
    ///                     render, continue, or retry.
    virtual ROP_RENDER_CODE      endRender();

private:
    static int          *ifdIndirect;
    fpreal               myEndTime;
    IMG_TileDevice      *dev;

/// To implelemnt
public:
    //ROP_RenderMode  getRenderMode();
    bool            hasImageOutput();
    //void            getFrameRange(long &s, long &e);
    //OP_DataType     getCookedDataType ()const;
    //void            getFrameDevice(UT_String &dev, float now, const char *renderer);
    //bool            getOutputResolution(int &x, int &y);
    //void            getOutputFile(UT_String &name);

    // Override the output filename for this node. Used by the hscript render -o command.
    //void        overrideOutput(bool enable, const char *fname=0);
    //void        getRenderedImageInfo(TIL_Sequence &seq);

public:
    void OUTPUT(UT_String &str, float t);
    void CAMERA(UT_String &str, float t);
    void PICTURE(UT_String &str, float t);
    void DEVICE(UT_String &str, float t);

protected:
    // This method returns the image device required to render it to an interactive window (an mplay window, for example).
    const char *getInteractiveImageDevice() const { return "imdisplay"; }
    bool        isPreviewAllowed() { return true; }   

};
//}       // End HDK_Sample namespace
#undef STR_PARM
#undef STR_SET
#undef STR_GET

#endif  // ROP_LAVA_H_