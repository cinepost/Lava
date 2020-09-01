/* $Id: //depot/branches/rmanprod/rman-19.0/libri/ric/Ric.h#3 $  (Pixar - RenderMan Division)  $Date: 2014/12/10 $ */
/*
** Copyright (c) 2007 PIXAR.  All rights reserved.  This program or
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
** Pixar
** 1200 Park Ave
** Emeryville, CA 94608
**
** ----------------------------------------------------------------------------
*/
#ifndef RIC_H
#define RIC_H

#include "ri.h"

#ifdef  __cplusplus
extern "C" {
#endif

/// Flush the renderer to the given stream marker.
PRMANAPI extern void RicFlush( RtString marker, RtBoolean synchronous,
                               RtToken flushmode );

/// Return the percentage done of the current render. If there is no current
/// render underway, this returns 100 (done).
PRMANAPI extern RtInt RicGetProgress();

/// Process outstanding callback requests (display, exception)
PRMANAPI extern void RicProcessCallbacks();

/// Configure Ric
PRMANAPI extern void RicOption(RtString name, RtPointer value);

#ifdef  __cplusplus
}
#endif

#endif // RIC_H
