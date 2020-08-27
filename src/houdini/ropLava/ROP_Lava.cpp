#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Director.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_DataTypes.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_Type.h>
#include <CH/CH_LocalVariable.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_OFStream.h>
#include <UT/UT_String.h>
#include <TIL/TIL_Sequence.h>
#include <IMG/IMG_TileDevice.h>
#include <IMG/IMG_TileOptions.h>
#include <IMG/IMG_TileSocket.h>
#include <TIL/TIL_TileMPlay.h>

#include "ROP_Lava.h"

#define STR_PARM(name, idx, vi, t) \
                { evalString(str, name, &ifdIndirect[idx], vi, t); }
#define INT_PARM(name, idx, vi, t) \
                { return evalInt(name, &ifdIndirect[idx], vi, t); }
#define STR_SET(name, idx, vi, t) \
                { setString(str, name, ifdIndirect[idx], vi, t); }
#define STR_GET(name, idx, vi, t) \
                { evalStringRaw(str, name, &ifdIndirect[idx], vi, t); }

#define PRM_MENU_CHOICES (PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE)

#define TXRES   64      // Tile X resolution
#define TYRES   64      // Tile Y resolution

struct PlaneDef {
    const char          *myName;
    IMG_DataType         myFormat;
    IMG_ColorModel       myColorModel;
};
PlaneDef thePlanes[] = {
    {"C",   IMG_HALF,   IMG_RGBA }, // C albedo plane with metalness in A, RGBA
    {"AC",  IMG_HALF,   IMG_RGBA }, // AC albedo colro plane with self illumination value in A, RGBA
    {"P",   IMG_HALF,  IMG_RGB }, // P position buffer
    {"N",   IMG_HALF,  IMG_RGB }, // N normal buffer
    {"Z",   IMG_FLOAT,  IMG_1CHAN } // Z depth buffer
    //{ "s",      IMG_FLOAT, IMG_1CHAN }, // s plane is float, single channel
    //{ "Normal", IMG_FLOAT, IMG_RGB },   // N plane is float, RGB data
};
#define NPLANES (sizeof(thePlanes)/sizeof(PlaneDef))


int *ROP_Lava::ifdIndirect = 0;

static PRM_Name         pictureName("picture", "Output Picture");
static PRM_Name         pictureNames[] = {
    PRM_Name("ip", "mplay (interactive)"),
    PRM_Name("md", "mplay (non-interactive)"),
    PRM_Name("$HIP/render/$HIPNAME.$OS.$F4.pic", "Sequence of .pic files"),
    PRM_Name("$HIP/render/$HIPNAME.$OS.$F4.tif", "Sequence of .tif files"),
    PRM_Name("$HIP/render/$HIPNAME.$OS.$F4.exr", "Sequence of .exr files"),
    PRM_Name(),
};
static PRM_Default      pictureDefault(0, "$HIP/render/$HIPNAME.$OS.$F4.exr");

static PRM_Name         deviceName("device", "Output Device");
static PRM_Name         deviceNames[] = {
    PRM_Name("infer",   "Infer from filename"),
    PRM_Name("pic",     "Houdini .pic format"),
    PRM_Name("tiff",    "Tagged Image File Format (TIFF)"),
    PRM_Name("exr",     "Open EXR"),
    PRM_Name("png",     "Portable Network Graphics (PNG)"),
    PRM_Name("jpg",     "JPEG"),
    PRM_Name(),
};
static PRM_Default      deviceDefault(0, "infer");

static PRM_ChoiceList   pictureMenu(PRM_CHOICELIST_REPLACE, pictureNames);
static PRM_ChoiceList   deviceMenu(PRM_MENU_CHOICES, deviceNames);

static PRM_Name         cameraName("camera", "Camera");
static PRM_Default      cameraDefault(0, "/obj/cam1");

static PRM_Name         sceneFileName("file", "Save lava scene to file");
static PRM_Default      sceneFileDefault(0, "scene.lvs");

static PRM_Template* getTemplates() {
    static PRM_Template *theTemplate = 0;
    
    if (theTemplate)
        return theTemplate;
    
    theTemplate = new PRM_Template[5];
    
    theTemplate[0] = PRM_Template(PRM_FILE,             1, &sceneFileName,  &sceneFileDefault);
    theTemplate[1] = PRM_Template(PRM_STRING_OPLIST,    1, &cameraName,     &cameraDefault);
    theTemplate[2] = PRM_Template(PRM_STRING,           1, &pictureName,    &pictureDefault,    &pictureMenu);
    theTemplate[3] = PRM_Template((PRM_Type) PRM_ORD,   1, &deviceName,     &deviceDefault,     &deviceMenu);

    // Set standard ROP parameters next. They are defined in ROP_Templates.h
    //theTemplate[4] = theRopTemplates[ROP_RENDER_TPLATE];
    //theTemplate[5] = theRopTemplates[ROP_RENDERDIALOG_TPLATE];
    //theTemplate[6] = theRopTemplates[ROP_TRANGE_TPLATE];
    //theTemplate[7] = theRopTemplates[ROP_FRAMERANGE_TPLATE];
    //theTemplate[8] = theRopTemplates[ROP_TAKENAME_TPLATE];

    //theTemplate[9] = theRopTemplates[ROP_TPRERENDER_TPLATE];
    //theTemplate[10] = theRopTemplates[ROP_PRERENDER_TPLATE];
    //theTemplate[11] = theRopTemplates[ROP_LPRERENDER_TPLATE];
    //theTemplate[12] = theRopTemplates[ROP_TPREFRAME_TPLATE];
    //theTemplate[13] = theRopTemplates[ROP_PREFRAME_TPLATE];
    //theTemplate[14] = theRopTemplates[ROP_LPREFRAME_TPLATE];
    //theTemplate[15] = theRopTemplates[ROP_TPOSTFRAME_TPLATE];
    //theTemplate[16] = theRopTemplates[ROP_POSTFRAME_TPLATE];
    //theTemplate[17] = theRopTemplates[ROP_LPOSTFRAME_TPLATE];
    //theTemplate[18] = theRopTemplates[ROP_TPOSTRENDER_TPLATE];
    //theTemplate[19] = theRopTemplates[ROP_POSTRENDER_TPLATE];
    //theTemplate[20] = theRopTemplates[ROP_LPOSTRENDER_TPLATE];
    theTemplate[4] = PRM_Template();
    return theTemplate;
}

OP_TemplatePair* ROP_Lava::getTemplatePair() {
    static OP_TemplatePair *ropPair = 0;
    if (!ropPair) {
        OP_TemplatePair *base;
        base = new OP_TemplatePair(getTemplates());
        ropPair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), base);
    }
    return ropPair;
}

OP_VariablePair* ROP_Lava::getVariablePair(){
    static OP_VariablePair *pair = 0;
    if (!pair) {
         pair = new OP_VariablePair(ROP_Node::myVariableList);
    }
    return pair;
}

OP_Node* ROP_Lava::myConstructor(OP_Network *net, const char *name, OP_Operator *op) {
    return new ROP_Lava(net, name, op);
}

ROP_Lava::ROP_Lava(OP_Network *net, const char *name, OP_Operator *entry): ROP_Node(net, name, entry) {
    if (!ifdIndirect)
        ifdIndirect = allocIndirect(16);
}

ROP_Lava::~ROP_Lava() {

}
//------------------------------------------------------------------------------
// The startRender(), renderFrame(), and endRender() render methods are invoked by Houdini when the ROP runs.
int ROP_Lava::startRender(int /*nframes*/, fpreal tstart, fpreal tend) {
    std::cout << "Call: startRender" << std::endl;
    myEndTime = tend;

    UT_String device_name;
    DEVICE(device_name, tstart);

    if (error() < UT_ERROR_ABORT)
        executePreRenderScript(tstart);
    return 1;
}

static void printNode(std::ostream &os, OP_Node *node, int indent) {
    UT_WorkBuffer wbuf;
    wbuf.sprintf("%*s", indent, "");
    os << wbuf.buffer() << node->getName() << "\n";
    for (int i=0; i<node->getNchildren(); ++i)
        printNode(os, node->getChild(i), indent+2);
}

ROP_RENDER_CODE ROP_Lava::renderFrame(fpreal time, UT_Interrupt *) {
    std::cout << "Call: renderFrame" << std::endl;
   
    int XRES = 640;    // Image X resolution
    int YRES = 320;    // Image Y resolution
    bool framemode = true;   

    // Execute the pre-render script.
    executePreFrameScript(time);
    // Evaluate the parameter for the file name and write something to the file.
    
    UT_String picture_name;
    PICTURE(picture_name, time);

    
    dev = IMG_TileDevice::newMPlayDevice(1);
    //dev = IMG_TileDevice::newSocketDevice("localhost");
    dev->setRenderSize(XRES, YRES);
    dev->setRenderSourceName(getName());


    IMG_TileOptionList  flist;
    for (int i = 0; i < NPLANES; i++){
        IMG_TileOptions *finfo = new IMG_TileOptions();
        finfo->setPlaneInfo(picture_name, thePlanes[i].myName, 0, thePlanes[i].myFormat, thePlanes[i].myColorModel);
        // These format options allow sending tiles to an existing tile device (such as the IPR) rather than opening a new one.
        // They only need to be set for plane 0 but it's harmless to send them for all planes.
        
        //finfo->setFormatOption("sockethost", "localhost");
        //finfo->setFormatOption("socketport", "");
        flist.append(finfo);
    }
    

    
    if (!dev->openMulti(flist, XRES, YRES, TXRES, TYRES, 1.0)){
        std::cerr << "Error opening tile device" << std::endl;
        return ROP_ABORT_RENDER;
    }

    /*
    if(framemode){
        std::cout << " Rendering frame" << std::endl;

        std::vector<float> data(XRES * YRES * 8); // all the planes as float arrays
        //std::fill(data.begin(), data.end(), 1.0f);
        void *d_color = static_cast<void*>(&data[0]);
        void *d_albedo = static_cast<void*>(&data[XRES * YRES * 2]);
        void *d_position = static_cast<void*>(&data[XRES * YRES * 4]);
        void *d_normal = static_cast<void*>(&data[XRES * YRES * 5.5]);

        XN_GBuffer *renderbuffer = xenon->renderFrame(XRES, YRES);

        if(renderbuffer == 0){
            std::cerr << "Error rendering !!!" << std::endl;
            return ROP_ABORT_RENDER;
        }

        renderbuffer->readData(XN_COLOR_BUFFER, d_color);
        renderbuffer->readData(XN_ALBEDO_BUFFER, d_albedo);
        renderbuffer->readData(XN_POSITION_BUFFER, d_position);
        renderbuffer->readData(XN_NORMAL_BUFFER, d_normal);
        dev->writeTile(d_color, 0, XRES-1, 0, YRES-1);

    }else{
        std::cout << " Rendering " << dev->getTileCountX() << " by " << dev->getTileCountY() << " tiles" << std::endl;
        int xl, xr, yb, yt, tx, ty;
        while(dev->getNextTile(xl, xr, yb, yt, tx, ty)){
            std::cout << "Rendering tile:(" << tx << "," << ty << ")" << std::endl;
            if(!xenon->renderTile(xl, xr, yb, yt, tx, ty)){
                std::cerr << "Error rendering tile: "  << tx << "," << ty << " !!!" << std::endl;
            }
        }
    }
    */

    dev->flush();
    dev->close();
    

    // Execute the post-render script.
    if (error() < UT_ERROR_ABORT)
        executePostFrameScript(time);

    return ROP_CONTINUE_RENDER;
}

ROP_RENDER_CODE ROP_Lava::endRender() {
    std::cout << "Call: endRender" << std::endl;

    if (error() < UT_ERROR_ABORT)
        executePostRenderScript(myEndTime);
    return ROP_CONTINUE_RENDER;
}
void newDriverOperator(OP_OperatorTable *table) {
    table->addOperator(
        new OP_Operator(
            "lava",
            "Lava",
            ROP_Lava::myConstructor,
            ROP_Lava::getTemplatePair(),
            0,
            9999,
            ROP_Lava::getVariablePair(),
            OP_FLAG_GENERATOR
        )
    );
}

//------------------------------------------------------------------------------ 

bool ROP_Lava::hasImageOutput(){
    return true;
}
    
bool ROP_Lava::isPreviewAllowed() {
    return true;
}

void ROP_Lava::getRenderedImageInfo(TIL_Sequence &seq) {
    std::cout << "ROP_Lava::getRenderedImageInfo" << std::endl;
}

void ROP_Lava::overrideDevice(bool enable, bool interactive, const char *devicestr) {
    std::cout << "ROP_Lava::overrideDevice" << std::endl;
}

void ROP_Lava::overrideOutput(bool enable, const char *fname){
    std::cout << "ROP_Node::overrideOutput ";
    if (enable) std::cout << "yes ";
    
    if (fname) {
        std::cout << std::string(fname) << " ";
    } 
    std::cout << std::endl;

    if (fname) 
        myRenderOutput = fname;
}

//void ROP_Lava::getFrameRange(long &s, long &e){
//    std::cout << "Call: getFrameRange" << std::endl;
//}

//OP_DataType ROP_Lava::getCookedDataType()const{
//    std::cout << "Call: getCookedDataType" << std::endl;
//    return OP_RASTER_DATA;
//}

//void ROP_Lava::getFrameDevice(UT_String &dev, float now, const char *renderer) {
//    std::cout << "Call: getFrameDevice: renderer: " << renderer << std::endl; 
//    dev = "bbb";
//}

const char* ROP_Lava::getInteractiveImageDevice() const { 
    std::cout << "ROP_Lava::getInteractiveImageDevice" << std::endl;
    return "imdisplay"; 
}

bool ROP_Lava::getOutputResolution(int &x, int &y){
    std::cout << "ROP_Lava::getOutputResolution" << std::endl;
    x = 640;
    y = 360;
    return true;
}

void ROP_Lava::getOutputFile(UT_String &name){
    std::cout << "ROP_Lava::getOutputFile" << std::endl;
    name = "/home/max/Pictures/640x360.png";
}

//void ROP_Node::getRenderedImageInfo(TIL_Sequence &seq){
//    std::cout << "Call: getRenderedImageInfo" << std::endl;
//    seq.reset();
//    seq.setRes(640,360);
//    seq.setSingleImage(true);
//    seq.addPlane("C", PXL_FLOAT32);
//}   

/*
void ROP_Node::overrideDevice(bool enable, bool interactive, const char *devicestr){
    std::cout << "Call: overrideDevice" << std::endl;
}

void ROP_Node::deleteCookedData(){
    std::cout << "Call: deleteCookedData" << std::endl;
}

int ROP_Node::saveCookedData(const char *str, OP_Context &ctx){
    std::cout << "Call: saveCookedData" << std::endl;
    return 0;
}

int ROP_Node::saveCookedData(std::ostream &os, OP_Context &ctx, int binary){
    std::cout << "Call: saveCookedData" << std::endl;
    return 0;
}
*/

//--------------------------------------------------------

void ROP_Lava::OUTPUT(UT_String &str, float t) {
    //STR_PARM("picture",  0, 0, t);
    evalString(str, "picture", &ifdIndirect[1], 0, t);
}

void ROP_Lava::CAMERA(UT_String &str, float t) {
    evalString(str, "camera", &ifdIndirect[0], 0, t);
}

void ROP_Lava::PICTURE(UT_String &str, float t) {
    if( getRenderMode() == ROP_RM_RENDER ) {
        //std::cout << "PICTURE: not raster mode." << std::endl;
        if(!getOutputOverride(str, t)) { 
            str = getRenderOutput();
        }
    } else {
        //std::cout << "PICTURE: raster render mode." << std::endl;
        evalString(str, "picture", &ifdIndirect[1], 0, t);
    }
    std::cout << "Picture: " << str << std::endl;
}

void ROP_Lava::DEVICE(UT_String &str, float t) {
    std::cout << "Call: DEVICE" << std::endl;
    // If we aren't doing a raster render (i.e. for the render COP), we should evaluate the device string, otherwise, the getRenderDevice() will get the correct device for output.
    if ( getRenderMode() == ROP_RM_RENDER ) { 
        //std::cout << "DEVICE: not raster mode." << std::endl;
        str = getRenderDevice();
    } else { 
        //std::cout << "DEVICE: raster render mode." << std::endl;
        evalString(str, "device", &ifdIndirect[2], 0, t);
    }
    std::cout << "Device: " << str << std::endl;
}