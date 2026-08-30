#ifndef _PROPIDL_H_
#define _PROPIDL_H_

#include <objbase.h>

// Minimum definitions required for structured storage property sets
typedef struct tagPROPVARIANT PROPVARIANT;

#ifndef VARTYPE_DEFINED
#define VARTYPE_DEFINED
typedef unsigned short VARTYPE;
#endif

struct tagPROPVARIANT {
    VARTYPE vt;
    WORD    wReserved1;
    WORD    wReserved2;
    WORD    wReserved3;
    union {
        char cVal;
        long lVal;
        float fltVal;
        double dblVal;
        CY cyVal;
        BSTR bstrVal;
        IUnknown *punkVal;
        IDispatch *pdispVal;
    };
};

#endif // _PROPIDL_H_
