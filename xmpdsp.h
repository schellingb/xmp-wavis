// XMPlay DSP/general plugin header
// new plugins can be submitted to plugins@xmplay.com

#pragma once

#include "xmpfunc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XMPDSP_FACE
#define XMPDSP_FACE 1 // "face"
#endif

#define XMPDSP_FLAG_MULTI		1 // supports multiple instances
#define XMPDSP_FLAG_TAIL		2 // effect has a tail
#define XMPDSP_FLAG_NODSP		4 // no DSP processing (a "general" plugin), exclude from "DSP" saved settings
#define XMPDSP_FLAG_TITLE		8 // want title change notifications (NewTitle)

typedef struct {
	DWORD flags; // XMPDSP_FLAG_xxx
	const char *name; // plugin name

	void (WINAPI *About)(HWND win); // OPTIONAL
	void *(WINAPI *New)(); // create new plugin instance (return new instance handle)

// the following all apply to a specific instance ("inst")
	void (WINAPI *Free)(void *inst); // free plugin instance
	const char *(WINAPI *GetDescription)(void *inst); // get description
	void (WINAPI *Config)(void *inst, HWND win); // present config options to user (OPTIONAL)
	DWORD (WINAPI *GetConfig)(void *inst, void *config); // get config (return size of config data)
	BOOL (WINAPI *SetConfig)(void *inst, void *config, DWORD size); // apply config
	void (WINAPI *NewTrack)(void *inst, const char *file); // a track has been opened or closed (OPTIONAL)
// the following are optional with the XMPDSP_FLAG_NODSP flag
	void (WINAPI *SetFormat)(void *inst, const XMPFORMAT *form); // set sample format (if form=NULL output stopped)
	void (WINAPI *Reset)(void *inst); // reset DSP after seeking
	DWORD (WINAPI *Process)(void *inst, float *data, DWORD count); // process samples (return number of samples processed)
// The Process function currently must return the same amount of data as it is given - it can't
// shorten/stretch the sound. This restriction may (or may not ;)) be lifted in future.
	void (WINAPI *NewTitle)(void *inst, const char *title); // the title has changed (OPTIONAL)
} XMPDSP;

#ifdef __cplusplus
}
#endif
