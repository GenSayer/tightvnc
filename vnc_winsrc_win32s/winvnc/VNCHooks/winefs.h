#ifndef _WINEFS_H_
#define _WINEFS_H_

// Windows Encrypted File System (EFS) Stub for Legacy NT 4 Ports
// This architecture does not support native EFS APIs.

#ifdef __cplusplus
extern "C" {
#endif

// Add basic macro status codes if required by downstream dependencies
#define EFS_USE_RECOVERY_POLICY_ONLY 0x00000001

#ifdef __cplusplus
}
#endif

#endif // _WINEFS_H_
