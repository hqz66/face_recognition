#ifndef __INIT_ASFORT_H__
#define __INIT_ASFORT_H__

#include "arcsoft_face_sdk.h"
#include "amcomdef.h"
#include "asvloffscreen.h"
#include "merror.h"

//从开发者中心获取APPID/SDKKEY
#define APPID "9ohNzRcYUGz17TmXN11zEX8eeGh5QmhRZNZZ2hnTExPw"
#define SDKKEY "4N1MqycBk5ze9ahSq8ZfdcYiSN3pBiaQJTbZYyY6aSQG"

#define NSCALE 16 
#define FACENUM	5

void timestampToTime(char* timeStamp, char* dateTime, int dateTimeSize);
int ColorSpaceConversion(MInt32 width, MInt32 height, MInt32 format, MUInt8* imgData, ASVLOFFSCREEN& offscreen);
int init_sdk();
void init_engine();

#endif
