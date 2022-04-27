/* $Id: dspyhlpr.c 113295 2008-05-07 21:02:44Z edward $  (Pixar - RenderMan Division)  $Date: 2008-05-07 17:02:44 -0400 (Wed, 07 May 2008) $ */
/*
** Copyright (c) 1999 PIXAR.  All rights reserved.  This program or
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
** 1200 Park Ave.
** Emeryville, CA 94608
*/

#ifndef __DSPYHLPR_C__
#define __DSPYHLPR_C__

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "third_party/prman/ndspy.h"

int _aiTypeLengths[] = {
	0,
	4, 4, 4, 2, 2, 1, 1, -1, 4*16
};

int _aiTypeSigned[] = { 0, 1, 0, 1, 0, 1, 0, 1 };

int _aiTypeClass[] = {
	0,
	PkDspyClassFloat,
	PkDspyClassInt,
	PkDspyClassInt,
	PkDspyClassInt,
	PkDspyClassInt,
	PkDspyClassInt,
	PkDspyClassInt
};


PRMANAPI PtDspyError
DspyFindStringInParamList(const char *string,
	char **result,
	int paramCount,
	const UserParameter *parameters)
{
	int i;

	for (i = 0; i < paramCount; i++) {
		if (!strcmp(string, parameters[i].name)) {
		    if ('s' == parameters[i].vtype) {
			    *result = *(char **)parameters[i].value;
			    return PkDspyErrorNone;
			}
  		}
	}
	return PkDspyErrorNoResource;
}

#if PkDspyByteOrderNative != PkDspyByteOrderHiLo
void PRMANAPI DspyMemReverse(unsigned char *t, int len) {
	int i;
	for (i = len / 2 - 1; i >= 0; i--) {
		t[i] ^= t[len - 1 - i];
		t[len - 1 - i] ^= t[i];
		t[i] ^= t[len - 1 - i];
	}
}
#endif /* PkDspyByteOrderNative != PkDspyByteOrderHiLo */

void PRMANAPI DspyMemReverseCopy(unsigned char *t, const unsigned char *s, int len) {
	int i;
	for (i = 0; i < len; i++) {
		t[i] = s[len - 1 - i];
	}
}

PRMANAPI PtDspyError
DspyFindMatrixInParamList(const char *string,
	float *result,
	int paramCount,
	const UserParameter *parameters)
{
	int i;

	for (i = 0; i < paramCount; i++) {
		if (('f' == parameters[i].vtype) 
		    && (16 == parameters[i].vcount)
			&& !strcmp(string, parameters[i].name))
	  	{
		    memcpy(result, ((float *)(parameters[i].value)), 4 * 16);
		    return PkDspyErrorNone;
  		}

	}
	return PkDspyErrorNoResource;
}

PRMANAPI PtDspyError
DspyFindFloatInParamList(const char *string,
	float *result,
	int paramCount,
	const UserParameter *parameters)
{
	int i;
	PtDspyError ret;
	ret = PkDspyErrorNoResource;

	for (i = 0; i < paramCount && ret == PkDspyErrorNoResource; i++) {
		if (!strcmp(string, parameters[i].name)) {
		    float f;
		    RtInt j ;

		    switch (parameters[i].vtype) {
				case 'f' :
				    memcpy(&f, ((float *)(parameters[i].value)), 4);
				    ret = PkDspyErrorNone;
				    break;
				case 'i' :
				    memcpy(&j, parameters[i].value, sizeof(j));
				    f = j;
				    ret = PkDspyErrorNone;
				    break;
			}

		    if (PkDspyErrorNone == ret) {
			    *result = f;
			}
  		}
	}
	return ret;
}

PRMANAPI PtDspyError
DspyFindFloatsInParamList(const char *string,
	int *resultCount,
	float *result,
	int paramCount,
	const UserParameter *parameters)
{
	int i;
	PtDspyError ret;
	ret = PkDspyErrorNoResource;

	for (i = 0; i < paramCount && ret == PkDspyErrorNoResource; i++) {
		if (!strcmp(string, parameters[i].name)) {
		    float f;
		    int k;
		    RtInt j ;

		    if (parameters[i].vcount < *resultCount)
			*resultCount = parameters[i].vcount;

		    for (k = 0; k < *resultCount; k++) {
				switch (parameters[i].vtype) {
					case 'f' :
					    memcpy(&f, k + ((RtFloat *)(parameters[i].value)), 4);
					    ret = PkDspyErrorNone;
					    break;
					case 'i' :
					    memcpy(&j, k + ((RtInt *)parameters[i].value), sizeof(j));
					    f = j;
					    ret = PkDspyErrorNone;
					    break;
				}
				
				if (PkDspyErrorNone == ret) {
				    *result = f;
				}

				result++;
		    }
  		}
	}
	return ret;
}

PRMANAPI PtDspyError
DspyFindIntInParamList(const char *string,
	int *result,
	int paramCount,
	const UserParameter *parameters)
{
	int i;
	PtDspyError ret;
	ret = PkDspyErrorNoResource;

	for (i = 0; i < paramCount && ret == PkDspyErrorNoResource; i++) {
		if (!strcmp(string, parameters[i].name)) {
		    float f;
		    RtInt j ;

		    switch (parameters[i].vtype) {
				case 'f' :
				    memcpy(&f, ((float *)(parameters[i].value)), 4);
				    ret = PkDspyErrorNone;
				    j = (int)f;
				    break;
				case 'i' :
				    memcpy(&j, parameters[i].value, sizeof(j));
				    ret = PkDspyErrorNone;
				    break;
			}

		    if (PkDspyErrorNone == ret) {
			    *result = j;
			}
  		}
	}
	return ret;
}

PRMANAPI PtDspyError
DspyFindIntsInParamList(const char *string,
	int *resultCount,
	int *result,
	int paramCount,
	const UserParameter *parameters)
{
	int i;
	PtDspyError ret;
	ret = PkDspyErrorNoResource;

	for (i = 0; i < paramCount && ret == PkDspyErrorNoResource; i++) {
		if (!strcmp(string, parameters[i].name)) {
		    float f;
		    int k;
		    RtInt j ;

		    if (parameters[i].vcount < *resultCount)
			*resultCount = parameters[i].vcount;

		    for (k = 0; k < *resultCount; k++) {
				switch (parameters[i].vtype) {
					case 'f' :
					    memcpy(&f, k + ((RtFloat *)(parameters[i].value)), 4);
					    j = (RtInt)f;
					    ret = PkDspyErrorNone;
					    break;
					case 'i' :
					    memcpy(&j, k + ((RtInt *)parameters[i].value), sizeof(j));
					    ret = PkDspyErrorNone;
					    break;
				}
			
				if (PkDspyErrorNone == ret)	{
			    	*result = j;
				}
				
				result++;
		    }
  		}
	}
	return ret;
}

void VDspyError(const char *module, const char *fmt, va_list vap) {
    char buffer[256];
    char *buf = buffer;

    *buf = '\0';
    if (module) {
        sprintf(buf, "%s:  %s", module, fmt);
        buf += strlen(buf);
    }

    vfprintf(stderr, buffer, vap);
}

void DspyError(const char *module, const char *fmt, ...) {
    va_list vap;

    va_start(vap, fmt);
    VDspyError(module, fmt, vap);
    va_end(vap);
}

PRMANAPI PtDspyError
DspyReorderFormatting(int formatCount,
	PtDspyDevFormat *format,
	int outFormatCount,
	const PtDspyDevFormat *outFormat)
{
	PtDspyError ret = PkDspyErrorNone;
	int i;
	int j;

	if (outFormatCount > formatCount)
		outFormatCount = formatCount;

	for (i = 0; i < outFormatCount; i++) {
		for (j = i; j < formatCount; j++) {
			if (!strcmp(format[j].name, outFormat[i].name)) {
				if (i != j) {
					PtDspyDevFormat tmpFormat;

					tmpFormat = format[i];
					format[i] = format[j];
					format[j] = tmpFormat;
				}

				if (outFormat[i].type) {
					format[i].type = outFormat[i].type;
				}
				break;
			}
		}

		if (j == formatCount)
			ret = PkDspyErrorBadParams;
	}
	return ret;
}

#endif // __DSPYHLPR_C__
