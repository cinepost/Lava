/* $Revision: #6 $  ($Date: 2014/12/10 $) */
/*
** Copyright (c) 2003-2010 PIXAR.  All rights reserved.  This program or
** documentation contains proprietary confidential information and trade
** secrets of PIXAR.  Reverse engineering of object code is prohibited.
** Use of copyright notice is precautionary and does not imply
** publication.
**
**                      RESTRICTED RIGHTS NOTICE
**
** Use, duplication, or disclosure by the Government is subject to the
** following restrictions:  For civilian agencies, subparagraphs (a) through
** (d) of the Commercial Computer Software--Restricted Rights clause at
** 52.227-19 of the FAR; and, for units of the Department of Defense, DoD
** Supplement to the FAR, clause 52.227-7013 (c)(1)(ii), Rights in
** Technical Data and Computer Software.
**
** Pixar Animation Studios
** 1200 Park Ave.
** Emeryville, CA  94608
*/
#ifndef RifFilter_h
#define RifFilter_h

/*
 *  RifFilter.h encapsulates the RenderMan Interface 
 *  and forms the basis of Ri Plugin Filter Support. (See RifPlugin.h).
 *  Since external developers are expected to subclass
 *  this class definition, it is imperative that new
 *  structure members don't affect previous versions' packing.
 *  We have reserved a block of bytes at the front of the structure
 *  for additional state-maintenance and expect that new Ri
 *  calls will be added at the bottom. k_CurrentVersion should 
 *  be incremented whenever "class RifFilter" gets a new member.
 *
 *  Note that rx.h contains other useful (public) procedures
 *  for reporting errors, accessing renderer attributes and options,
 *  etc.
 *
 *  NB: New members must ONLY be added at the end of RifFilter struct
 */

#include <stdarg.h>
#include "ri.h"
#include <string.h>


struct RifFilter
{
        enum
        {
            k_UnknownVersion = 0,
            /* Version 2 added BrickMap and HierarchicalSubdivisionMesh */
            /* Version 3 added AttributeSave/AttributeRestore (now removed) */
            /* Version 4 added ResourceBegin/ResourceEnd */
            /* Version 5 added System */
            /* Version 6 added Shader, Camera */
            /* Version 7 added ErrorHandler */
            /* Version 8 added:
             *      EditWorldBegin/End, 
             *      EditBegin/End, 
             *      EditAttributeBegin/End 
             * Version 9 added:
             *      PixelSampleImager
             * Version 10 added:
             *      Volume
             *      VPSurface,
             *      VPInterior,
             *      VPAtmosphere
             * Version 11 added:
             *      ObjectBeginV
             *      ObjectInstanceV
             *      Procedural2V
             * Version 12 added:
             *      VolumePixelSamples
             * Version 13 added:
             *      BxdfV
             *      IntegratorV
             *      PatternV
             */
            k_CurrentVersion = 13
        };

short           Version;        /* the version of the table */
void *          ClientData;     /* a place for the plug-in to hang its hat */
char            Reserved[64];   /* for future use */
                enum DefaultFiltering
                {
                    k_Continue,
                    k_Terminate
                };
DefaultFiltering Filtering;
                RifFilter() 
                { 
                    memset((void *) this, 0, sizeof(RifFilter));
                    Version = k_CurrentVersion;
                    Filtering = k_Continue;
                    // dispatch functions should be initialized to 0L
                }

RtToken         (*Declare)(char *name, char *declaration);
RtVoid          (*FrameBegin)(RtInt frame);
RtVoid          (*FrameEnd)();
RtVoid          (*WorldBegin)();
RtVoid          (*WorldEnd)();
RtVoid          (*Clipping)(RtFloat hither, RtFloat yon);
RtVoid          (*ClippingPlane)(RtFloat Nx, RtFloat Ny, RtFloat Nz, 
                    RtFloat Px, RtFloat Py, RtFloat Pz);
RtVoid          (*CropWindow)(RtFloat xmin, RtFloat xmax, 
                    RtFloat ymin, RtFloat ymax);
RtVoid          (*DepthOfField)(RtFloat fstop, RtFloat focallength, 
                    RtFloat focaldistance);
RtVoid          (*Format)(RtInt xres, RtInt yres, RtFloat aspect);
RtVoid          (*FrameAspectRatio)(RtFloat aspect);
RtVoid          (*ProjectionV)(RtToken name, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*ScreenWindow)(RtFloat left, RtFloat right, 
                    RtFloat top, RtFloat bot);
RtVoid          (*Shutter)(RtFloat min, RtFloat max);
RtVoid          (*DisplayV)(char *name, RtToken type, RtToken mode,
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*Exposure)(RtFloat gain, RtFloat gamma);
RtVoid          (*ImagerV)(RtToken name, RtInt, RtToken[], RtPointer[]);
RtVoid          (*PixelFidelity)(RtFloat variation); // OBSOLETE call: see RiPixelVariance
RtVoid          (*PixelFilter)(RtFilterFunc filterFunc, 
                    RtFloat xwidth, RtFloat ywidth);
RtVoid          (*PixelSamples)(RtFloat xsamples, RtFloat ysamples);
RtVoid          (*PixelVariance)(RtFloat variation);
RtVoid          (*Quantize)(RtToken type, 
                    RtInt one, RtInt min, RtInt max, RtFloat ampl);
RtVoid          (*HiderV)(RtToken type, RtInt, RtToken[], RtPointer[]);
RtVoid          (*OptionV)(RtToken name, RtInt, RtToken[], RtPointer[]);

RtVoid          (*AttributeBegin)();
RtVoid          (*AttributeEnd)();
RtVoid          (*AttributeV)(RtToken name, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*Color)(RtFloat* color);
RtVoid          (*Opacity)(RtFloat* color);
RtVoid          (*TextureCoordinates)(RtFloat s1, RtFloat t1, 
                    RtFloat s2, RtFloat t2,
                    RtFloat s3, RtFloat t3, 
                    RtFloat s4, RtFloat t4);
RtVoid          (*Matte)(RtBoolean onoff);
RtVoid          (*ShadingInterpolation)(RtToken type);
RtVoid          (*ShadingRate)(RtFloat size);
                    
RtLightHandle   (*AreaLightSourceV)(RtToken name, 
                    RtInt, RtToken[], RtPointer[]);
RtLightHandle   (*LightSourceV)(RtToken name, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*Illuminate)(RtLightHandle light, RtBoolean onoff);
RtVoid          (*AtmosphereV)(RtToken name, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*DisplacementV)(RtToken name, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*ExteriorV)(RtToken name, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*InteriorV)(RtToken name, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*SurfaceV)(RtToken name, 
                    RtInt, RtToken[], RtPointer[]);

RtVoid          (*TransformBegin)();
RtVoid          (*TransformEnd)();
RtVoid          (*Bound)(RtBound bound);
RtVoid          (*Detail)(RtBound bound);
RtVoid          (*DetailRange)(RtFloat minv, 
                    RtFloat lowtran, RtFloat uptran, RtFloat maxv);
RtVoid          (*RelativeDetail)(RtFloat relativedetail);
RtVoid          (*GeometricApproximation)(RtToken type, RtFloat value);
RtVoid          (*Orientation)(RtToken orientation);
RtVoid          (*ReverseOrientation)();
RtVoid          (*Sides)(RtInt sides);

RtVoid          (*ConcatTransform)(RtMatrix transform);
RtVoid          (*CoordSysTransform)(RtToken space);
RtVoid          (*CoordinateSystem)(RtToken space);
RtVoid          (*ScopedCoordinateSystem)(RtToken space);

RtVoid          (*Identity)();
RtVoid          (*Perspective)(RtFloat fov);
RtVoid          (*Rotate)(RtFloat angle, 
                    RtFloat dx, RtFloat dy, RtFloat dz);
RtVoid          (*Scale)(RtFloat sx, RtFloat sy, RtFloat sz);
RtVoid          (*Skew)(RtFloat angle, RtFloat dx1, RtFloat dy1, 
                    RtFloat dz1, RtFloat dx2, RtFloat dy2, RtFloat dz2);
RtVoid          (*Transform)(RtMatrix transform);
RtVoid          (*Translate)(RtFloat dx, RtFloat dy, RtFloat dz);

RtVoid          (*Basis)(RtBasis ubasis, RtInt ustep, 
                    RtBasis vbasis, RtInt vstep);
RtVoid          (*BlobbyV)(RtInt nleaf, RtInt ninst, RtInt inst[], 
                    RtInt nflt, RtFloat flt[], 
                    RtInt nstr, RtToken str[],
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*ConeV)(RtFloat height, RtFloat radius, RtFloat tmax,
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*CurvesV)(RtToken type, RtInt ncurves, RtInt nvert[], 
                    RtToken wrap, RtInt, RtToken[], RtPointer[]);
RtVoid          (*CylinderV)(RtFloat rad, RtFloat zmin, RtFloat zmax, 
                    RtFloat tmax, RtInt, RtToken[], RtPointer[]);
RtVoid          (*DiskV)(RtFloat height, RtFloat radius, RtFloat tmax,
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*GeneralPolygonV)(RtInt nloops, RtInt nverts[],
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*GeometryV)(RtToken type, RtInt, RtToken[], RtPointer[]);
RtVoid          (*HyperboloidV)(RtPoint point1, RtPoint point2, 
                    RtFloat tmax,
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*NuPatchV)(RtInt nu, RtInt uorder, RtFloat uknot[], 
                    RtFloat umin, RtFloat umax, RtInt nv, RtInt vorder, 
                    RtFloat vknot[], RtFloat vmin, RtFloat vmax, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*ParaboloidV)(RtFloat rmax, RtFloat zmin, RtFloat zmax, 
                    RtFloat tmax, RtInt, RtToken[], RtPointer[]);
RtVoid          (*PatchMeshV)(RtToken type, RtInt nu, RtToken uwrap, 
                    RtInt nv, RtToken vwrap, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*PatchV)(RtToken type, RtInt, RtToken[], RtPointer[]);
RtVoid          (*PointsGeneralPolygonsV)(RtInt npolys, RtInt nloops[], 
                    RtInt nverts[], RtInt verts[], 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*PointsPolygonsV)(RtInt npolys, RtInt nverts[], 
                    RtInt verts[], RtInt, RtToken[], RtPointer[]);
RtVoid          (*PointsV)(RtInt nverts, RtInt, RtToken[], RtPointer[]);
RtVoid          (*PolygonV)(RtInt nverts, RtInt, RtToken[], RtPointer[]);
RtVoid          (*Procedural)(RtPointer data, RtBound bound, 
                    RtProcSubdivFunc sdfunc, RtProcFreeFunc freefunc);
RtVoid          (*SphereV)(RtFloat radius, RtFloat zmin, RtFloat zmax, 
                    RtFloat tmax, RtInt, RtToken[], RtPointer[]);
RtVoid          (*SubdivisionMeshV)(RtToken mask, RtInt nf, RtInt nverts[],
                    RtInt verts[], RtInt nt, RtToken tags[], 
                    RtInt nargs[], RtInt intargs[], RtFloat floatargs[],
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*TorusV)(RtFloat majrad, RtFloat minrad, 
                    RtFloat phimin, RtFloat phimax,
                    RtFloat tmax, RtInt, RtToken[], RtPointer[]);
RtVoid          (*TrimCurve)(RtInt nloops, RtInt ncurves[], 
                    RtInt order[], RtFloat knot[],
                    RtFloat min[], RtFloat max[], RtInt n[], 
                    RtFloat u[], RtFloat v[], RtFloat w[]);

RtVoid          (*SolidBegin)(RtToken operation);
RtVoid          (*SolidEnd)();

RtObjectHandle  (*ObjectBegin)();
RtVoid          (*ObjectEnd)();
RtVoid          (*ObjectInstance)(RtObjectHandle handle);

RtVoid          (*MotionBeginV)(RtInt n, RtFloat times[]);
RtVoid          (*MotionEnd)();

RtVoid          (*ResourceV)(RtToken handle, RtToken type, 
                    RtInt, RtToken[], RtPointer[]);

RtVoid          (*MakeCubeFaceEnvironmentV)(char *px, char *nx, 
                    char *py, char *ny,
                    char *pz, char *nz, 
                    char *tex, RtFloat fov, 
                    RtFilterFunc filterFunc,
                    RtFloat swidth, RtFloat ywidth, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*MakeLatLongEnvironmentV)(char *pic, char *tex, 
                    RtFilterFunc filterFunc,
                    RtFloat swidth, RtFloat twidth, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*MakeShadowV)(char *pic, char *tex, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*MakeTextureV)(char *pic, char *tex, 
                    RtToken swrap, RtToken twrap,
                    RtFilterFunc filterFunc, 
                    RtFloat swidth, RtFloat twidth,
                    RtInt, RtToken[], RtPointer[]);

RtVoid          (*_deprecatedFunction1)();    /* ProcDelayedReadArchive */
RtVoid          (*_deprecatedFunction2)();    /* ProcDynamicLoad */
RtVoid          (*_deprecatedFunction3)();    /* ProcRunProgram */

RtVoid          (*VArchiveRecord)(RtToken type, char *format, va_list vap);
RtVoid          (*ReadArchiveV)(RtToken name, RtArchiveCallback callback,
                    RtInt, RtToken[], RtPointer[]);

RtArchiveHandle (*ArchiveBeginV)(RtToken name, 
                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*ArchiveEnd)();

RtVoid          (*IfBeginV)(char *expr, RtInt, RtToken[], RtPointer[]);
RtVoid          (*ElseIfV)(char *expr, RtInt, RtToken[], RtPointer[]);
RtVoid          (*Else)();
RtVoid          (*IfEnd)();

RtVoid          (*DisplayChannelV)(RtToken mode, RtInt, RtToken[], RtPointer[]);

RtVoid          (*MakeBrickMapV)(RtInt, RtToken *, RtToken,
                                 RtInt, RtToken[], RtPointer[]);
RtVoid          (*HierarchicalSubdivisionMeshV)(RtToken mask, RtInt nf,
                    RtInt nverts[], RtInt verts[], RtInt nt, RtToken tags[],
                    RtInt nargs[], RtInt intargs[], RtFloat floatargs[],
                    RtToken stringargs[], RtInt, RtToken[], RtPointer[]);

RtVoid          (*ResourceBegin)();
RtVoid          (*ResourceEnd)();
RtVoid          (*System)(char *cmdstring);
    
RtVoid          (*ShaderV)(RtToken name, RtToken handle,
                    RtInt, RtToken[], RtPointer[]);

RtVoid          (*CameraV)(RtToken name, RtInt, RtToken[], RtPointer[]);

RtVoid          (*ErrorHandler)(RtErrorHandler);
    
RtVoid          (*EditBeginV)(RtToken name, RtInt, RtToken[], RtPointer[]);
RtVoid          (*EditEnd)();
RtVoid          (*EditAttributeBegin)(RtToken name);
RtVoid          (*EditAttributeEnd)();
RtVoid          (*EditWorldBeginV)(RtToken name, RtInt, RtToken[], RtPointer[]);
RtVoid          (*EditWorldEnd)();

RtVoid          (*PixelSampleImagerV)(RtToken name, RtInt, RtToken[], RtPointer[]);

/* version 10 */
RtVoid          (*VolumeV)(RtToken type, RtBound, RtInt *, RtInt, RtToken[], RtPointer[]);
RtVoid          (*VPSurfaceV)(RtToken name, RtInt, RtToken[], RtPointer[]);
RtVoid          (*VPInteriorV)(RtToken name, RtInt, RtToken[], RtPointer[]);
RtVoid          (*VPAtmosphereV)(RtToken name, RtInt, RtToken[], RtPointer[]);

/* version 11 */
RtObjectHandle  (*ObjectBeginV)(RtInt, RtToken[], RtPointer[]);
RtVoid          (*ObjectInstanceV)(RtObjectHandle handle,
                                    RtInt, RtToken[], RtPointer[]);
RtVoid          (*Procedural2V)(RtProc2SubdivFunc sfunc, 
                                RtProc2BoundFunc bfunc, 
                                RtInt, RtToken[], RtPointer[]);

/* version 12 */    
RtVoid          (*VolumePixelSamples)(RtFloat xsamples, RtFloat ysamples);    

/* version 13 */
RtVoid          (*BxdfV)(RtToken name, RtToken handle, 
                         RtInt, RtToken[], RtPointer[]);
RtVoid          (*IntegratorV)(RtToken name, RtToken handle, 
                         RtInt, RtToken[], RtPointer[]);
RtVoid          (*PatternV)(RtToken name, RtToken handle,
                         RtInt, RtToken[], RtPointer[]);

  /*
  ** New members must ONLY be added at the end (prior to overrunbuffer)
  */

                /*
                 * Memory buffer to allow plugins compiled with this header to
                 * be opened by later versions of rif (which will use the space
                 * for newly added entrypoints).
                 */
char            overrunbuffer[64*8];
};

#endif 
