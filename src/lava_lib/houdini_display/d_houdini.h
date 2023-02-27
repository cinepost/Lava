/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	123 Front Street West, Suite 1401
 *	Toronto, Ontario
 *	Canada   M5J 2M2
 *	416-504-9876
 *
 * NAME:	d_houdini.h ( d_houdini DSO, C++)
 *
 * COMMENTS:	Glue from prman to mdisplay
 */

#ifndef __d_houdini__
#define __d_houdini__

#include <string.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <memory>
#include <boost/shared_ptr.hpp>

#include "glm/detail/type_half.hpp"

using namespace boost;
using namespace std;

#define h_shared_ptr std::shared_ptr

class H_Image;
class H_MultiRes;
typedef h_shared_ptr<H_Image> ImagePtr;
typedef h_shared_ptr<H_MultiRes> MultiResPtr;

// IMDisplay holds a FILE* to the imdisplay process
class IMDisplay;
typedef h_shared_ptr<IMDisplay> IMDisplayPtr;

class H_Channel {
public:
	 H_Channel()	{init();}
	~H_Channel()	{destroy();}

	void	 init();
	void	 destroy();
	int      open(const std::string& channel_name,	// Name of channel
					const int channel_size,	// Size in bytes of channel
					const int channel_count,	// Number of channels
					const int off[4]);	// Offsets of each component
	void	 startTile(const int xres, const int yres);
	int		 writePixel(const char* pData, const int pixelOffset);
	int		 closeTile(FILE* fp, const int id, const int x0, const int y0, const int x1, const int y1);

	const std::string& getName() const {return myName;}
	inline int getFormat() const {return myFormat;}
	inline int getArraySize() const {return myCount;}
	inline int getPixelSize() const {return myPixelSize;}

private:
	std::string myName;
	vector< char > myData;
	int		mySize, myFormat, myCount;
	int		myDSize, myOffset;
	int		myPixelSize;
	int		myMap[4];
};

class H_Image {
public:
	 H_Image() {init("", 0, 0);}
	 H_Image(const std::string& filename, const int xres, const int yres) {init(filename, xres, yres);}
	~H_Image() {destroy();}

	void	init(const std::string& filename, const int xres, const int yres);
	void	destroy();
	int		addChannel(const std::string& name, const int size, const int count, const int off[4]);
    
    int 	writeChannelHeader(void);

	int		writeData(const int x0, const int x1, const int y0, const int y1,
                      const char* pData, const int bytes_per_pixel,
                      const float tileScaleX, const float tileScaleY);
    
    int		getXres() const {return myXres;}
    int		getYres() const	{return myYres;}
    
    int		getOrigXres() const {return myOrigXres; }
    int		getOrigYres() const	{return myOrigYres; }
    
    // returns 1 for success.
	static int openPipe(void);

	void   parseOptions(const int paramCount, const UserParameter *parameters);

	inline bool isHalfFloat() const { return mHalfFloat; }

	inline const std::string& getIMDisplayOptions() const { return myIMDisplayOptions; }

    inline char const* getDisplayOptions(void) const { return myIMDisplayOptions.c_str(); }
    
    inline IMDisplayPtr getIMDisplay(void) { return myIMD; }
    
    inline char const* getName(void) const { return myName.c_str(); }
    
    inline int getPort(void) const { return myPort; }
    
    inline bool hasDisplayOptions(void) const { return !myIMDisplayOptions.empty(); }
    
    inline void setIMDisplay(IMDisplayPtr p) { myIMD = p; }
    
    inline void setChannelOffset(int id) { myChannelOffset = id; }

    inline size_t getChannelCount(void) const { return myChannels.size(); }

    int getEntrySize(void) const;
    
private:
    
	std::string myName;
	std::string myIMDisplayOptions;
	vector< h_shared_ptr < H_Channel > > myChannels;
    
    bool    mHalfFloat = false; // Indicates that pixel data is 16 bit float (half)
    int     myChannelOffset;    // my first channel's id
    
	int		myXoff, myYoff;
	int		myOrigXres, myOrigYres;
	int		myXres, myYres;
	int		myPort;
    IMDisplayPtr myIMD;
};

// store a single resolution of a multires render
// allows storage also of a single lod
class H_MultiRes {
public:
    H_MultiRes(ImagePtr img) :
            myImg(img)
    {
        init(0, 0, 0);
    }
    H_MultiRes(ImagePtr img, const int xres, const int yres, const int level) :
            myImg(img)
    {
        init(xres, yres, level);
    }
	~H_MultiRes();

	void init(const int xres, const int yres, const int level);

	int getXres() const {return myXres;}
	int getYres() const {return myYres;}
	float getXscale() const {return myXscale;}
	float getYscale() const {return myYscale;}
    
    ImagePtr getImage(void) { return myImg; }
    
private:
    
    ImagePtr myImg;
	int myXres;
	int myYres;
	float myXscale;
	float myYscale;
	int myLevel;
};

#endif
