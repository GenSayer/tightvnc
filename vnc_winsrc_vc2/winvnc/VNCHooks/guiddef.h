#ifndef _GUIDDEF_H_
#define _GUIDDEF_H_

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} GUID;
#endif

typedef GUID *LPGUID;
typedef const GUID *LPCGUID;

#ifndef REFGUID
#define REFGUID const GUID &
#endif

#ifndef REFIID
#define REFIID const GUID &
#endif

#ifndef REFCLSID
#define REFCLSID const GUID &
#endif

#ifdef __cplusplus
#define InlineIsEqualGUID(rguid1, rguid2) \
    (((unsigned long *) &rguid1)[0] == ((unsigned long *) &rguid2)[0] && \
     ((unsigned long *) &rguid1)[1] == ((unsigned long *) &rguid2)[1] && \
     ((unsigned long *) &rguid1)[2] == ((unsigned long *) &rguid2)[2] && \
     ((unsigned long *) &rguid1)[3] == ((unsigned long *) &rguid2)[3])

inline int IsEqualGUID(REFGUID rguid1, REFGUID rguid2) {
    return InlineIsEqualGUID(rguid1, rguid2);
}
#endif

#endif // _GUIDDEF_H_
