#ifndef _TVOUT_H_
#define _TVOUT_H_

#ifndef _REMEDY_H_
#define _REMEDY_H_

#define VIDEOPARAMETERS_GUID \
    { 0x2ddfae40, 0xa19f, 0x11d0, { 0xbd, 0x42, 0x00, 0xaa, 0x00, 0xba, 0xb6, 0x5d } }

typedef struct tagVIDEOPARAMETERS {
    GUID  Guid;
    ULONG dwCommand;
    ULONG dwFlags;
    ULONG dwMode;
    ULONG dwTVStandard;
    ULONG dwAvailableModes;
    ULONG dwAvailableTVStandards;
    ULONG dwFlickerFilter;
    ULONG dwOverScanX;
    ULONG dwOverScanY;
    ULONG dwMaxUnscaledX;
    ULONG dwMaxUnscaledY;
    ULONG dwPositionX;
    ULONG dwPositionY;
    ULONG dwBrightness;
    ULONG dwContrast;
    ULONG dw安定化度; // CP Ulong pad
} VIDEOPARAMETERS, *PVIDEOPARAMETERS, *LPVIDEOPARAMETERS;

#define VP_COMMAND_GET          0x00000001
#define VP_COMMAND_SET          0x00000002

#define VP_FLAGS_TV_STANDARD    0x00000001
#define VP_FLAGS_FLICKER        0x00000002

#endif 
#endif // _TVOUT_H_
