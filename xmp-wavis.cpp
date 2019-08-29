/*
  xmp-wavis
  by Bernhard Schelling

  This is free and unencumbered software released into the public domain.

  Anyone is free to copy, modify, publish, use, compile, sell, or
  distribute this software, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any
  means.

  In jurisdictions that recognize copyright laws, the author or authors
  of this software dedicate any and all copyright interest in the
  software to the public domain. We make this dedication for the benefit
  of the public at large and to the detriment of our heirs and
  successors. We intend this dedication to be an overt act of
  relinquishment in perpetuity of all present and future rights to this
  software under copyright law.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  For more information, please refer to <http://unlicense.org/>
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <CommDlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "winamp_wa_ipc.h"
#include "winamp_vis.h"
#include "xmpfunc.h"
#include "xmpdsp.h"

//#define NDEBUG
#ifndef NDEBUG
static void Log(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	char* logbuf = (char*)malloc(1+vsnprintf(NULL, 0, format, ap));
	vsprintf(logbuf, format, ap);
	va_end(ap);
	OutputDebugStringA(logbuf);
	free(logbuf);
}
#define WMSTR(wm){ wm, #wm }
static const LPCSTR GetMsgString(UINT Msg)
{
	static const struct { UINT Msg; LPCSTR Str; } arr[] = {
		WMSTR(WM_CREATE), WMSTR(WM_COMMAND), WMSTR(WM_INITMENU), WMSTR(WM_INITMENUPOPUP), WMSTR(WM_MENUSELECT), WMSTR(WM_UNINITMENUPOPUP), WMSTR(WM_ACTIVATEAPP), 
		WMSTR(WM_MOUSEMOVE), WMSTR(WM_ENTERIDLE), WMSTR(WM_SETCURSOR), WMSTR(WM_ACTIVATE), WMSTR(WM_ENTERMENULOOP), WMSTR(WM_EXITMENULOOP), WMSTR(WM_GETMINMAXINFO),
		WMSTR(WM_CAPTURECHANGED), WMSTR(WM_LBUTTONUP), WMSTR(WM_LBUTTONDBLCLK), WMSTR(WM_RBUTTONDOWN), WMSTR(WM_RBUTTONUP), WMSTR(WM_WINDOWPOSCHANGING), WMSTR(WM_WINDOWPOSCHANGED),
		WMSTR(WM_SETFOCUS), WMSTR(WM_KILLFOCUS),WMSTR(WM_NCCREATE), WMSTR(WM_NCCALCSIZE), WMSTR(WM_NCACTIVATE), WMSTR(WM_GETTEXT), WMSTR(WM_IME_SETCONTEXT), WMSTR(WM_MOVE),
		WMSTR(WM_PARENTNOTIFY), WMSTR(WM_MOUSEACTIVATE), WMSTR(WM_SIZE), WMSTR(WM_NCPAINT),  WMSTR(WM_NCDESTROY), WMSTR(WM_DESTROY), WMSTR(WM_SHOWWINDOW), WMSTR(WM_ERASEBKGND),
		WMSTR(WM_QUERYOPEN), WMSTR(WM_DISPLAYCHANGE), WMSTR(WM_PAINT), WMSTR(WM_GETICON), WMSTR(WM_DWMNCRENDERINGCHANGED), WMSTR(SPI_SETMOUSEDOCKTHRESHOLD), WMSTR(WM_NCHITTEST),
		WMSTR(WM_NCMOUSEMOVE), WMSTR(WM_NCMOUSELEAVE), WMSTR(WM_ENABLE), WMSTR(WM_CANCELMODE),
		{0xBD9,"UNKNOWN 0xBD9"},{0x7D4,"ATTACHEMB 0x7D4"},{0,NULL}
	};
	for (int i = 0; arr[i].Str; i++) if (arr[i].Msg == Msg) return arr[i].Str; return "";
}
static const LPCSTR GetIpcString(UINT Msg)
{
	static const struct { UINT Msg; LPCSTR Str; } arr[] = {
		WMSTR(IPC_GET_EMBEDIF), WMSTR(IPC_GETVERSION), WMSTR(IPC_GETINIFILE), WMSTR(IPC_GETINIDIRECTORY), WMSTR(IPC_GETPLUGINDIRECTORY), WMSTR(IPC_GETSKIN), WMSTR(IPC_IS_PLAYING_VIDEO), WMSTR(IPC_ISPLAYING),
		WMSTR(IPC_SET_VIS_FS_FLAG), WMSTR(IPC_GET_API_SERVICE), WMSTR(IPC_GETPLAYLISTFILE), WMSTR(IPC_GETPLAYLISTFILEW), WMSTR(IPC_GETPLAYLISTTITLE), WMSTR(IPC_GETPLAYLISTTITLEW),
		WMSTR(IPC_GETOUTPUTTIME), WMSTR(IPC_JUMPTOTIME), WMSTR(IPC_SETPLAYLISTPOS), WMSTR(IPC_GETLISTLENGTH), WMSTR(IPC_GETLISTPOS), WMSTR(IPC_GET_SHUFFLE), 
		WMSTR(IPC_SETVISWND),
	};
	for (int i = 0; arr[i].Str; i++) if (arr[i].Msg == Msg) return arr[i].Str; return "";
}
#undef WMSTR
#define HWNDNAME(h) (h == g_MsgHWND ? "MSG" : (h == g_VisHWND ? "VIS" : (h == g_EmbHWND ? "EMB" : "UNKNOWN")))
#else
#define Log(...) (void)0
#endif

#define SONG_TEXT_LENGTH 120

static const char *WaVisXMPDSPName = "Winamp Vis Wrapper";
#define XMPDSPVERSION "rev.7"

static HINSTANCE dllinst;
static XMPFUNC_MISC* XMPlay_Misc = NULL;
static XMPFUNC_STATUS* XMPlay_Status = NULL;
static HWND g_XMPlayHWND = NULL;
static char g_pcWinampIni[MAX_PATH] = "";
static char g_pcXMPlayDir[MAX_PATH] = "";
static long seed = 31337;
static int iWaVissNum = 0;
static struct WaVisDSP* pWaViss[64];
static struct VisWND* pVisSetEmbedWindowStateVisWND;
static struct LoadedResString *pLoadedResStrings;
static float* g_pfBuffer = NULL;
static int g_BufferSize = 0, g_BufferPos = 0, g_BufferLatency = 0;
static int g_BufferReadingThreads = 0;
static bool g_BufferLocked = false;

static long RandFunc()
{
	long unsigned int hi, lo;
	lo = 16807 * (seed & 0xFFFF);
	hi = 16807 * (seed >> 16);
	lo += (hi & 0x7FFF) << 16;
	lo += hi >> 15;
	if (lo > 0x7FFFFFFF) lo -= 0x7FFFFFFF;
	return ( seed = (long)lo );
}

struct LoadedResString
{
	HINSTANCE hinst;
	UINT uid;
	void* pData;
	LoadedResString* next;

	static void Add(HINSTANCE hinst, UINT uid, void* pData)
	{
		LoadedResString *pResNew = new LoadedResString;
		pResNew->hinst = hinst;
		pResNew->uid = uid;
		pResNew->pData = pData;
		pResNew->next = pLoadedResStrings;
		pLoadedResStrings = pResNew;
	}

	static void Clear(HINSTANCE hinst)
	{
		LoadedResString *pRes = pLoadedResStrings;
		while (pRes)
		{
			LoadedResString *pResNext = pRes->next;
			if (pRes->hinst == hinst)
			{
				delete pRes->pData;
				delete pRes;
				if (pLoadedResStrings == pRes) pLoadedResStrings = pResNext;
			}
			pRes = pResNext;
		}
	}

	static void* Get(HINSTANCE hinst, UINT uid)
	{
		for (LoadedResString *pRes = pLoadedResStrings; pRes; pRes = pRes->next)
			if (pRes->hinst == hinst && pRes->uid == uid) 
				return pRes->pData;
		return NULL;
	}
};

class WaVisApiService
{
	enum { API_SERVICE_SERVICE_GETSERVICEBYGUID = 50, WASERVICEFACTORY_GETINTERFACE = 300 };

	virtual int _dispatch(int msg, void *retval, void **params=0, int nparam=0)
	{
		static GUID g_guid_waservice_language = {0x30AED4E5, 0xEF10, 0x4277, {0x8D, 0x49, 0x27, 0xAB, 0x55, 0x70, 0xE8, 0x91}};
		if (msg == API_SERVICE_SERVICE_GETSERVICEBYGUID && nparam == 1 && params[0] && (*(GUID*)params[0] == g_guid_waservice_language))
		{
			static Language language;
			*(void**)retval = (void*)&language;
			return 1;
		}

		if (msg == API_SERVICE_SERVICE_GETSERVICEBYGUID && nparam == 1 && params[0])
		{
			static Dummy dummy;
			*(void**)retval = (void*)&dummy;
			return 1;
		}

		return 0;
	}

	class Dummy
	{
		virtual int _dispatch(int msg, void *retval, void **params=0, int nparam=0)
		{
			if (msg == WASERVICEFACTORY_GETINTERFACE) { *(void**)retval = (void*)this; return 1; }
			return 0;
		}
	};

	class Language
	{
		enum
		{
			WASERVICE_LANGUAGE_GETSTRING = 10,
			WASERVICE_LANGUAGE_GETSTRINGW = 11,
			WASERVICE_LANGUAGE_GETHANDLE = 30,
			WASERVICE_LANGUAGE_CREATEDIALOGPARAM = 50,
			WASERVICE_LANGUAGE_DIALOGBOXPARAM = 51,
			WASERVICE_LANGUAGE_LOADMENU = 52,
			WASERVICE_LANGUAGE_CREATEDIALOGPARAMW = 53,
			WASERVICE_LANGUAGE_DIALOGBOXPARAMW = 54,
		};

		#if 0
		static const LPCSTR GetWaString(UINT Msg)
		{
			static const struct { UINT Msg; LPCSTR Str; } s_IpcStr[] = {
				{API_SERVICE_SERVICE_GETSERVICEBYGUID, "API_SERVICE_SERVICE_GETSERVICEBYGUID"},
				{WASERVICE_LANGUAGE_GETSTRING, "WASERVICE_LANGUAGE_GETSTRING"},
				{WASERVICE_LANGUAGE_GETSTRINGW, "WASERVICE_LANGUAGE_GETSTRINGW"},
				{WASERVICE_LANGUAGE_GETHANDLE, "WASERVICE_LANGUAGE_GETHANDLE"},
				{WASERVICE_LANGUAGE_CREATEDIALOGPARAM, "WASERVICE_LANGUAGE_CREATEDIALOGPARAM"},
				{WASERVICE_LANGUAGE_DIALOGBOXPARAM, "WASERVICE_LANGUAGE_DIALOGBOXPARAM"},
				{WASERVICE_LANGUAGE_LOADMENU, "WASERVICE_LANGUAGE_LOADMENU"},
				{WASERVICE_LANGUAGE_CREATEDIALOGPARAMW, "WASERVICE_LANGUAGE_CREATEDIALOGPARAMW"},
				{WASERVICE_LANGUAGE_DIALOGBOXPARAMW, "WASERVICE_LANGUAGE_DIALOGBOXPARAMW"},
				{WASERVICEFACTORY_GETINTERFACE, "WASERVICEFACTORY_GETINTERFACE"},
				{0, NULL},
			};
			for (int i = 0; s_IpcStr[i].Msg; i++) if (s_IpcStr[i].Msg == Msg) return s_IpcStr[i].Str; return "";
		}
		#endif

		virtual int _dispatch(int msg, void *retval, void **params=0, int nparam=0)
		{
			#if 0
			if (msg != WASERVICE_LANGUAGE_GETSTRING && msg != WASERVICE_LANGUAGE_GETSTRINGW)
				Log("[WAS] WAS: 0x%04x [%20s] - nparam: %d\n",  msg, GetWaString(msg), nparam);
			#endif

			switch(msg) 
			{
				case WASERVICE_LANGUAGE_GETSTRING:
					// params (DWORD) handle, (HINSTANCE) hInstance, (UINT) uID, (LPSTR) destBuf, (int) destSize
					if (nparam != 5) break;
					if (*(void**)retval = *(void**)params[3] = LoadedResString::Get(*(HINSTANCE*) params[1], *(UINT*) params[2])) return TRUE;
					char pcBuffer[1024];
					if (!(*(int*)params[4] = LoadStringA(*(HINSTANCE*) params[1], *(UINT*) params[2], pcBuffer, 1024))) return FALSE;
					char *pcData;
					pcData = new char[*(int*)params[4]+1];
					strcpy(pcData, pcBuffer);
					LoadedResString::Add(*(HINSTANCE*) params[1], *(UINT*) params[2], pcData);
					*(void**)retval = *(void**)params[3] = (void*)pcData;
					return TRUE;
				case WASERVICE_LANGUAGE_GETSTRINGW:
					// params (DWORD) handle, (HINSTANCE) hInstance, (UINT) uID, (LPSTR) destBuf, (int) destSize
					if (nparam != 5) break;
					if (*(void**)retval = *(void**)params[3] = LoadedResString::Get(*(HINSTANCE*) params[1], *(UINT*) params[2])) return TRUE;
					WCHAR pwcBuffer[1024];
					if (!(*(int*)params[4] = LoadStringW(*(HINSTANCE*) params[1], *(UINT*) params[2], pwcBuffer, 1024))) return FALSE;
					WCHAR *pwcData;
					pwcData = new WCHAR[*(int*)params[4]+1];
					wcscpy(pwcData, pwcBuffer);
					LoadedResString::Add(*(HINSTANCE*) params[1], *(UINT*) params[2], pwcData);
					*(void**)retval = *(void**)params[3] = (void*)pwcData;
					return TRUE;
				case WASERVICE_LANGUAGE_GETHANDLE:
					// params (HINSTANCE) hInstance, (GUID) pluginGuid
					if (nparam != 2) break;
					*(long*)retval = 0xDEADC0DE;
					return TRUE;
				case WASERVICE_LANGUAGE_CREATEDIALOGPARAM:
					// params (DWORD) handle, (HINSTANCE) hInstance, (LPCSTR) lpTemplateName, (HWND) hWndParent, (DLGPROC) lpDialogFunc, (LPARAM) dwInitParam
					if (nparam != 6) break;
					*(HWND*)retval = CreateDialogParamA(*(HINSTANCE*) params[1], *(LPCSTR*) params[2], *(HWND*) params[3], *(DLGPROC*) params[4], *(LPARAM*) params[5]);
					EnumChildWindows(*(HWND*) params[3], EnumChildProc, 0);
					EnumChildWindows(*(HWND*) retval, EnumChildProc, 0);
					return TRUE;
				case WASERVICE_LANGUAGE_CREATEDIALOGPARAMW:
					// params (DWORD) handle, (HINSTANCE) hInstance, (LPCSTR) lpTemplateName, (HWND) hWndParent, (DLGPROC) lpDialogFunc, (LPARAM) dwInitParam
					if (nparam != 6) break;
					*(HWND*)retval = CreateDialogParamW(*(HINSTANCE*) params[1], *(LPCWSTR*) params[2], *(HWND*) params[3], *(DLGPROC*) params[4], *(LPARAM*) params[5]);
					EnumChildWindows(*(HWND*) params[3], EnumChildProc, 0);
					EnumChildWindows(*(HWND*) retval, EnumChildProc, 0);
					return TRUE;
				case WASERVICE_LANGUAGE_DIALOGBOXPARAM:
					// params (DWORD) handle, (HINSTANCE) hInstance, (LPCSTR) lpTemplateName, (HWND) hWndParent, (DLGPROC) lpDialogFunc, (LPARAM) dwInitParam
					if (nparam != 6) break;
					*(int*)retval = DialogBoxParamA(*(HINSTANCE*) params[1], *(LPCSTR*) params[2], *(HWND*) params[3], *(DLGPROC*) params[4], *(LPARAM*) params[5]);
					return TRUE;
				case WASERVICE_LANGUAGE_DIALOGBOXPARAMW:
					// params (DWORD) handle, (HINSTANCE) hInstance, (LPCSTR) lpTemplateName, (HWND) hWndParent, (DLGPROC) lpDialogFunc, (LPARAM) dwInitParam
					if (nparam != 6) break;
					*(int*)retval = DialogBoxParamW(*(HINSTANCE*) params[1], *(LPCWSTR*) params[2], *(HWND*) params[3], *(DLGPROC*) params[4], *(LPARAM*) params[5]);
					return TRUE;
				case WASERVICE_LANGUAGE_LOADMENU:
					// params (DWORD) handle, (HINSTANCE) hInstance, (INT) rsrcid
					if (nparam != 3) break;
					*(HMENU*)retval = LoadMenuA(*(HINSTANCE*) params[1], (const char *)MAKEINTRESOURCE(110)); //*(LPCSTR*) params[2]);
					return TRUE;
				case WASERVICEFACTORY_GETINTERFACE:
					*(void**)retval = (void*)this;
					return 1;
			}
			return FALSE;
		}

		static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
		{
			char pcText[255];
			GetWindowTextA(hwnd, pcText, 255);
			char *p;
			if (p = strstr(pcText, "Winamp")) memcpy(p, "XMPlay", 6);
			else if (p = strstr(pcText, "winamp")) memcpy(p, "XMPlay", 6);
			else return TRUE;
			SetWindowTextA(hwnd, pcText);
			if (strcmp(pcText, "Integrate with XMPlay skin") == 0) EnableWindow(hwnd, FALSE);
			return TRUE;
		}
	};
};

struct VisWND
{
	VisWND() { }

	VisWND(bool bMemZero)
	{
		if (bMemZero) memset(this, 0, sizeof(*this));
	}

	~VisWND()
	{
		Cleanup();
	}

	int iEmbInnWidth, iEmbInnHeight, iEmbBorWidth, iEmbBorHeight;
	embedWindowState* pVisEmbWinState;
	char pcPluginDir[MAX_PATH];
	bool bPluginIsFullScreen;
	bool bEnteredSetSizeMove;
	bool bRestoreOnFullScreenEnd;
	bool bStopPlugin;
	HWND g_MsgHWND, g_EmbHWND, g_VisHWND;

	static LRESULT __stdcall WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		VisWND* pVisWND = (VisWND*)GetWindowLong(hWnd,GWL_USERDATA);
		if(message == WM_CREATE) pVisWND = (VisWND*)(((LPCREATESTRUCT)lParam)->lpCreateParams);
		if (pVisWND) return pVisWND->WndProc(hWnd,message,wParam,lParam);
		return DefWindowProcW(hWnd,message,wParam,lParam);
	}

	void XMPlay_ExecCommand(short iExecKey, bool bNoQueue = false)
	{
		SendMessageTimeout(g_XMPlayHWND, WM_USER+0x1a, iExecKey, 0, SMTO_ABORTIFHUNG, 300, NULL);
	}

	void OnPluginHideApplication(bool bHiding)
	{
		if ( bHiding && IsWindowVisible(g_XMPlayHWND)) { XMPlay_ExecCommand(2); bRestoreOnFullScreenEnd = true; }
		if (!bHiding && bRestoreOnFullScreenEnd  )     { XMPlay_ExecCommand(2); bRestoreOnFullScreenEnd = false; }
	}

	static HWND VisSetEmbedWindowStateStatic(embedWindowState* e)
	{
		if (!pVisSetEmbedWindowStateVisWND) return 0;
		return pVisSetEmbedWindowStateVisWND->VisSetEmbedWindowState(e);
	}

	HWND VisSetEmbedWindowState(embedWindowState* e)
	{
		if (!IsWindow(g_EmbHWND)) create_emb_window();
		Log("[EMB] Flags: %d - Pos: %d,%d - Size: %d,%d\n", e->flags, e->r.left, e->r.top, e->r.right - e->r.left, e->r.bottom - e->r.top);
		e->me = g_EmbHWND;
		if ((e->r.right >= e->r.left+50) && (e->r.bottom >= e->r.top+20))
		{
			iEmbInnWidth = e->r.right - (e->r.left == CW_USEDEFAULT ? 0 : e->r.left + iEmbBorWidth);
			iEmbInnHeight = e->r.bottom - (e->r.top == CW_USEDEFAULT ? 0 : e->r.top + iEmbBorHeight);
			if (g_EmbHWND) SetWindowPos(g_EmbHWND, NULL, e->r.left, e->r.top, iEmbInnWidth + iEmbBorWidth, iEmbInnHeight + iEmbBorHeight, SWP_NOZORDER|(e->r.left==CW_USEDEFAULT?SWP_NOMOVE:0));
			//iWinampVisWidth = iEmbInnWidth + iEmbBorWidth;
			//iWinampVisHeight = iEmbInnHeight + iEmbBorHeight;
		}
		else
		{
			e->r.right = e->r.left + iEmbInnWidth;
			e->r.bottom = e->r.top + iEmbInnHeight;
			if (g_EmbHWND) SetWindowPos(g_EmbHWND, NULL, e->r.left, e->r.top, iEmbInnWidth + iEmbBorWidth, iEmbInnHeight + iEmbBorHeight, SWP_NOZORDER);
		}
		pVisEmbWinState = e;
		return e->me;
	}

	LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_WA_IPC)
		{
			Log("[IPC] HWND: %s - IPC: 0x%04x [%20s] - wParam: %x (%x / %x) - lParam: %x (%x / %x) - FS: %d - EMB: %d\n", HWNDNAME(hWnd), lParam, GetIpcString(lParam), wParam, LOWORD(wParam), HIWORD(wParam), lParam, LOWORD(lParam), HIWORD(lParam), bPluginIsFullScreen, IsWindowVisible(g_EmbHWND));
		}
		else if (message != WM_GETTEXT && /*message != WM_WINDOWPOSCHANGED &&*/ message != WM_MOVE && message != WM_ENTERIDLE)
		{
			Log("[MSG] HWND: %s - Msg: 0x%04x [%20s] - wParam: %x (%x / %x) - lParam: %x (%x / %x) - FS: %d - EMB: %d\n", HWNDNAME(hWnd), message, GetMsgString(message), wParam, LOWORD(wParam), HIWORD(wParam), lParam, LOWORD(lParam), HIWORD(lParam), bPluginIsFullScreen, IsWindowVisible(g_EmbHWND));
		}

		DWORD r;
		switch (message)
		{
			case WM_WA_IPC:
				switch (lParam)
				{
					case IPC_GET_EMBEDIF:
						if (!g_EmbHWND) return 0;
						if (wParam == 0) pVisSetEmbedWindowStateVisWND = this;
						return (wParam == 0 ? (long)&VisSetEmbedWindowStateStatic : (long)g_EmbHWND);
						break;
					case IPC_GETVERSION:
						return 0x5013;
					case IPC_GETINIFILE:
						Log("    -> %s\n", g_pcWinampIni);
						////return (LRESULT)(void*)""; //disable ini file
						//if (pcWinampIni[0] && !fopen(pcWinampIni, "r")) { FILE* f = fopen(pcWinampIni, "w"); fclose(f); }
						return (LRESULT)g_pcWinampIni;
					case IPC_GETINIDIRECTORY:
						Log("    -> %s\n", g_pcXMPlayDir);
						return (LRESULT)g_pcXMPlayDir;
					case IPC_GETPLUGINDIRECTORY:
						Log("    -> %s\n", pcPluginDir);
						return (LRESULT)pcPluginDir;
					case IPC_GET_RANDFUNC:
						return (long)&RandFunc;
					case IPC_SETVISWND:
						g_VisHWND = (HWND)wParam;
						SetWindowPos(g_VisHWND, (((!g_EmbHWND) && (g_VisHWND) && (g_XMPlayHWND) && (!bStopPlugin) && (GetWindowLongA(g_XMPlayHWND, GWL_EXSTYLE) & WS_EX_TOPMOST)) ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
						SetWindowPos(g_VisHWND, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
						SetWindowPos(g_VisHWND, HWND_TOP, 0, 0, 0, 0, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
						return 1;
					case IPC_GETSKIN:
						return 0;
					case 0x7D4: //2004: //MESSAGE WHEN WINDOW REMOVED OR ATTACHES ITSELF TO THE EMBWINDOW (used by AVS 'embed in editor' function)
						//printf("REMOVED FROM EMB VIS HWND = %d -- EMB: %d -- VIS: %d!!!!\n", g_VisHWND, IsWindowVisible(g_EmbHWND), IsWindowVisible(g_VisHWND));
						if (g_EmbHWND && g_VisHWND)
						{
							int iEmbInnWidthBck = iEmbInnWidth, iEmbInnHeightBck = iEmbInnHeight;
							SetWindowPos(g_VisHWND, 0, 0, 0, 50, 50, 0);
							SetWindowPos(g_EmbHWND, 0, 0, 0, 50, 50, SWP_NOREPOSITION|SWP_NOZORDER|SWP_NOMOVE);
							SetWindowPos(g_VisHWND, 0, 0, 0, iEmbInnWidthBck, iEmbInnHeightBck, 0);
							SetWindowPos(g_EmbHWND, 0, 0, 0, iEmbInnWidthBck+iEmbBorWidth, iEmbInnHeightBck+iEmbBorHeight, SWP_NOREPOSITION|SWP_NOZORDER|SWP_NOMOVE);
						}
						return 1;
					case IPC_IS_PLAYING_VIDEO:
					case 0xBD9: //3033:
						//undocumented config maybe function, return 0 means not supported I think
						//printf("got WA message: %x, %d, %d\n", message, wParam, lParam);
						return 0;
					case IPC_ISPLAYING:
						return 1;
					case IPC_SET_VIS_FS_FLAG:
						bPluginIsFullScreen = (wParam >= 1);
						if (g_MsgHWND && g_XMPlayHWND) { RECT rec; GetWindowRect(g_XMPlayHWND, &rec); SetWindowPos(g_MsgHWND, NULL, rec.left, rec.top, rec.right - rec.left, rec.bottom - rec.top, SWP_NOZORDER | SWP_NOREDRAW | SWP_NOSENDCHANGING); }
						return 1;
					case IPC_GET_API_SERVICE: { static WaVisApiService apiService; return (LRESULT)&apiService; }
					case IPC_GETPLAYLISTFILE:
					case IPC_GETPLAYLISTFILEW:
					case IPC_GETPLAYLISTTITLE:
					case IPC_GETPLAYLISTTITLEW:
						{
							//if (lParam == IPC_GETPLAYLISTFILEW) lParam = IPC_GETPLAYLISTFILE;
							//if (lParam == IPC_GETPLAYLISTTITLEW) lParam = IPC_GETPLAYLISTTITLE;
							//if (!bRunningInsideXMPlay) return 0; //not supported outside XMPlay...
							if (!g_XMPlayHWND || bStopPlugin) return 0;
							if (((long)wParam == -1) && (SendMessageTimeoutW(g_XMPlayHWND, message, 0, IPC_GETLISTPOS, SMTO_ABORTIFHUNG, 300, (LPDWORD)&r))) wParam = r;
							static WCHAR song[SONG_TEXT_LENGTH];
							if (!SendMessageTimeoutW(g_XMPlayHWND, message, wParam, (lParam == IPC_GETPLAYLISTFILEW ? IPC_GETPLAYLISTFILE : (lParam == IPC_GETPLAYLISTTITLEW ? IPC_GETPLAYLISTTITLE : lParam)), SMTO_ABORTIFHUNG, 300, &r) || !r)
							{
								song[0] = 0;
								return (long)song;
							}
							static char csong[SONG_TEXT_LENGTH];
							MultiByteToWideChar(CP_UTF8, 0, (char*)r, -1, song, SONG_TEXT_LENGTH);
							if (lParam == IPC_GETPLAYLISTTITLEW || lParam == IPC_GETPLAYLISTTITLEW) return (long)song;
							WideCharToMultiByte(CP_OEMCP, 0, song, SONG_TEXT_LENGTH, csong, SONG_TEXT_LENGTH, NULL, NULL);
							return (long)csong;
						}
					case IPC_GETOUTPUTTIME:
						if (!g_XMPlayHWND || bStopPlugin) return 0;
						return (SendMessageTimeout(g_XMPlayHWND, message, wParam, lParam, SMTO_ABORTIFHUNG, 300, &r) ? (r?r:1) : -1);
					case IPC_JUMPTOTIME:
						if (!g_XMPlayHWND || bStopPlugin) return 1;
						return (SendMessageTimeout(g_XMPlayHWND, message, wParam, lParam, SMTO_ABORTIFHUNG, 300, &r) ? r : 1);
					case IPC_SETPLAYLISTPOS:
						//int i,iCurSong;
						//if (!SendMessageTimeout(g_XMPlayHWND, message, 0, IPC_GETLISTPOS, SMTO_ABORTIFHUNG, 300, (LPDWORD)&iCurSong)) return 0;
						//if (iCurSong == -1)
						//{
						//	XMPlay_ExecCommand(80, true);
						//	//SendMessageTimeout(g_XMPlayHWND, message, 0, IPC_STARTPLAY, SMTO_ABORTIFHUNG, 300, (LPDWORD)&r);
						//	SendMessageTimeout(g_XMPlayHWND, message, 0, IPC_GETLISTPOS, SMTO_ABORTIFHUNG, 300, (LPDWORD)&iCurSong);
						//	if (iCurSong == -1) { return -1; }
						//}
						//XMPlay_ExecCommand(340, true); //jump to current
						//if (iCurSong == (int)wParam) { return 1; }
						//iCurSong = wParam - iCurSong;
						//for (i = abs(iCurSong); i > 0; i--) { XMPlay_ExecCommand( (iCurSong<0 ? 336 : 337) , true); }
						//return 1;
					case IPC_GETLISTLENGTH:
					case IPC_GETLISTPOS:
					case IPC_GET_SHUFFLE:
						if (!g_XMPlayHWND || bStopPlugin) return 0;
						return (SendMessageTimeout(g_XMPlayHWND, message, wParam, lParam, SMTO_ABORTIFHUNG, 300, &r) ? r : -1);
					default:
						//{ char pm[255]; sprintf(pm,"got WA message: %x, %d, %d\n", message, wParam, lParam); MessageBox(NULL, pm, "", 0); }	
						//printf("got WA message: %x, %d, %d\n", message, wParam, lParam);
						if (!g_XMPlayHWND || bStopPlugin) return 1;
						return (SendMessageTimeout(g_XMPlayHWND, message, wParam, lParam, SMTO_ABORTIFHUNG, 300, &r) ? r : 1);
				}
				return 0;
			case WM_SHOWWINDOW:
				if (g_EmbHWND == hWnd && g_VisHWND) MoveWindow(g_VisHWND, 0,0, iEmbInnWidth, iEmbInnHeight, true);
				return 0;
			case WM_ENTERSIZEMOVE:
				if (g_EmbHWND == hWnd) bEnteredSetSizeMove = true;
				return 0;
			case WM_EXITSIZEMOVE:
				if (g_EmbHWND == hWnd) bEnteredSetSizeMove = false;
				if (g_EmbHWND == hWnd && g_VisHWND) SetWindowPos(g_VisHWND, 0, 0, 0, iEmbInnWidth, iEmbInnHeight, 0);
				if (g_EmbHWND == hWnd && pVisEmbWinState) GetWindowRect(g_EmbHWND, &((embedWindowState*)(pVisEmbWinState))->r);
				return 0;
			case WM_SIZE:
				if (hWnd == g_EmbHWND && lParam)
				{
					iEmbInnWidth = (int)(short) LOWORD(lParam) /*- iEmbBorWidth*/;   // horizontal position 
					iEmbInnHeight = (int)(short) HIWORD(lParam) /*- iEmbBorHeight*/;   // vertical position 
				}
				return 0;
			case WM_MOVE:
			if (hWnd == g_MsgHWND) ShowWindow(g_MsgHWND, HIDE_WINDOW);
				if (g_VisHWND) SendNotifyMessageA(g_VisHWND, message, wParam, lParam);
				return 1;
			case WM_WINDOWPOSCHANGED:
				Log("    HWND: %s - Flags: %d - Pos: %d,%d - Size: %d,%d\n", HWNDNAME(((WINDOWPOS*)lParam)->hwnd), ((WINDOWPOS*)lParam)->flags, ((WINDOWPOS*)lParam)->x, ((WINDOWPOS*)lParam)->y, ((WINDOWPOS*)lParam)->cx, ((WINDOWPOS*)lParam)->cy);
				if (hWnd == g_EmbHWND && ((WINDOWPOS*)lParam)->flags & SWP_SHOWWINDOW && ((GetWindowLongW(hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0))
				{
					SetWindowPos(hWnd, HWND_TOPMOST,   0, 0, iEmbInnWidth+iEmbBorWidth, iEmbInnHeight+iEmbBorHeight, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE);
					SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, iEmbInnWidth+iEmbBorWidth, iEmbInnHeight+iEmbBorHeight, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE);
					SetFocus(hWnd);
				}
				if (g_VisHWND) SendNotifyMessageA(g_VisHWND, message, wParam, lParam);
				if (hWnd == g_EmbHWND && !bPluginIsFullScreen && g_EmbHWND && g_VisHWND && (((WINDOWPOS*)lParam)->flags & SWP_NOSIZE))// && !(((WINDOWPOS*)lParam)->flags & WS_MINIMIZE))
				{
					SetWindowPos(g_VisHWND, 0, 0, 0, iEmbInnWidth, iEmbInnHeight, 0);
				}
				if (hWnd == g_MsgHWND)
				{
					if      ( bPluginIsFullScreen && ((WINDOWPOS*)(lParam))->flags & SWP_HIDEWINDOW) OnPluginHideApplication(true);
					else if (!bPluginIsFullScreen && ((WINDOWPOS*)(lParam))->flags & SWP_SHOWWINDOW) OnPluginHideApplication(false);
				}
				break;
			case WM_WINDOWPOSCHANGING:
				Log("    HWND: %s - Flags: %d - Pos: %d,%d - Size: %d,%d\n", HWNDNAME(((WINDOWPOS*)lParam)->hwnd), ((WINDOWPOS*)lParam)->flags, ((WINDOWPOS*)lParam)->x, ((WINDOWPOS*)lParam)->y, ((WINDOWPOS*)lParam)->cx, ((WINDOWPOS*)lParam)->cy);
				if (g_VisHWND) SendNotifyMessageA(g_VisHWND, message, wParam, lParam);
				return 1;
			case WM_KEYDOWN:
			case WM_DEADCHAR:
			case WM_KEYLAST:
			case WM_SYSDEADCHAR:
			case WM_CHAR:
			case WM_KEYUP:
				//Embed mode: Forward key to the embedded plugin if key arrives at the (container) emb-window
				if (hWnd == g_EmbHWND && g_VisHWND) PostMessageA(g_VisHWND, message, wParam, lParam);
				return 0;
			case WM_COMMAND:
				switch (wParam)
				{
					case 40044 /*WINAMP_BUTTON1*/:       /*Previous_track();*/ XMPlay_ExecCommand(340); XMPlay_ExecCommand(129); XMPlay_ExecCommand(336); break;
					case 40045 /*WINAMP_BUTTON2*/:       /*Start_playback();*/ XMPlay_ExecCommand(372); break;
					case 40046 /*WINAMP_BUTTON3*/:       /*Pause_playback();*/ XMPlay_ExecCommand(80);  break;
					case 40047 /*WINAMP_BUTTON4*/:       /*Stop_playback();*/  XMPlay_ExecCommand(81);  break;
					case 40048 /*WINAMP_BUTTON5*/:       /*Next_track();*/     XMPlay_ExecCommand(340); XMPlay_ExecCommand(128); XMPlay_ExecCommand(337); break;
					case 40058 /*WINAMP_VOLUMEUP*/:      /*Volume_up();*/      XMPlay_ExecCommand(512); break;
					case 40059 /*WINAMP_VOLUMEDOWN*/:    /*Volume_down();*/    XMPlay_ExecCommand(513); break;
					case 40148 /*WINAMP_BUTTON5_SHIFT*/:
					case 40060 /*WINAMP_FFWD5S*/:        /*Fast_forward();*/   XMPlay_ExecCommand(82);  break;
					case 40144 /*WINAMP_BUTTON1_SHIFT*/:
					case 40061 /*WINAMP_REW5S*/:         /*Rewind();*/         XMPlay_ExecCommand(83);    break;
					case 40023 /*Toggle shuffle*/:       /*Toggle_shuffle();*/ XMPlay_ExecCommand(313);   break;
					case 40022 /*Toggle repeat*/:        /*Toggle_repeat();*/  XMPlay_ExecCommand(9); break;
				}
				return 0;
			case WM_DESTROY:
				if (bPluginIsFullScreen) { OnPluginHideApplication(false); bPluginIsFullScreen = false; }
				break;
			case WM_CLOSE:
				//if (hWnd == g_MsgHWND || hWnd == g_EmbHWND) DestroyWindow(hWnd); 
				if (bPluginIsFullScreen) { OnPluginHideApplication(false); bPluginIsFullScreen = false; }
				break;
			case WM_CANCELMODE:
				//This message arrives, when the msgwindow is about to display a messagebox
				//It's probably the plugin sending an error message
				break;
			case WM_SETCURSOR:
				if (LOWORD(lParam) == 1)
				{
					SetCursor(LoadCursorA( NULL, IDC_ARROW ));				
					return 1;
				}
			/*
			case WM_MOUSEMOVE:
			case WM_ERASEBKGND:
			case WM_GETTEXT:
			case WM_PAINT:
			case WM_ENTERIDLE:	
			*/
		}
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}

	int keep_alive()
	{
		MSG msg;
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) return 1;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		return 0;
	}

	int create_emb_window()
	{
		WNDCLASSW myClass;
		long lXMExStyle = (g_XMPlayHWND ? GetWindowLongA(g_XMPlayHWND, GWL_EXSTYLE) : 0);
		RECT rctClient, rctWindow;
		myClass.hCursor = LoadCursorA( NULL, IDC_ARROW );
		myClass.hIcon = (g_XMPlayHWND ? (HICON)GetClassLong(g_XMPlayHWND, GCL_HICON) : NULL);
		myClass.lpszMenuName = (LPWSTR) NULL;
		myClass.lpszClassName = (LPWSTR)L"XMPVisPluginEmbed";
		myClass.hbrBackground = (HBRUSH)(COLOR_WINDOW);
		myClass.hInstance = NULL;
		myClass.style = CS_GLOBALCLASS;
		myClass.lpfnWndProc = WndProcStatic;
		myClass.cbClsExtra = 0;
		myClass.cbWndExtra = 0;
		UnregisterClassW(L"XMPVisPluginEmbed", NULL);
		RegisterClassW(&myClass);
		g_EmbHWND = CreateWindowExW(lXMExStyle & WS_EX_TOPMOST, (LPWSTR)L"XMPVisPluginEmbed", (LPWSTR)"XMPlay Winamp Wrapper", WS_OVERLAPPEDWINDOW|WS_CAPTION,
				  CW_USEDEFAULT, CW_USEDEFAULT, 300, 400, (HWND) NULL, (HMENU) NULL,
				  myClass.hInstance, this);
		if (g_EmbHWND == NULL) { return -101; }
		SetWindowLong(g_EmbHWND, GWL_USERDATA, LONG(this));
		GetClientRect(g_EmbHWND, &rctClient);
		GetWindowRect(g_EmbHWND, &rctWindow);
		iEmbInnWidth = rctClient.right - rctClient.left;
		iEmbInnHeight = rctClient.bottom - rctClient.top;
		iEmbBorWidth = rctWindow.right - rctWindow.left - iEmbInnWidth;
		iEmbBorHeight = rctWindow.bottom - rctWindow.top - iEmbInnHeight;
		return 0;
	}

	int create_msg_window()
	{
		WNDCLASSW myClass;
		long lXMExStyle = (g_XMPlayHWND ? GetWindowLongA(g_XMPlayHWND, GWL_EXSTYLE) : 0);
		myClass.hCursor = LoadCursorA( NULL, IDC_ARROW );
		myClass.hIcon = NULL;
		myClass.lpszMenuName = (LPWSTR) NULL;
		myClass.lpszClassName = (LPWSTR) L"XMPVisPlugin";
		myClass.hbrBackground = (HBRUSH)(COLOR_WINDOW);
		myClass.hInstance = NULL;
		myClass.style = CS_GLOBALCLASS;
		myClass.lpfnWndProc = WndProcStatic;
		myClass.cbClsExtra = 0;
		myClass.cbWndExtra = 0;
		UnregisterClassW(L"XMPVisPlugin", NULL);
		RegisterClassW(&myClass);
		g_MsgHWND = CreateWindowExW(lXMExStyle & WS_EX_TOPMOST, (LPWSTR)L"XMPVisPlugin", (LPWSTR)L"", /*WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_CAPTION*/ 0, CW_USEDEFAULT, CW_USEDEFAULT, 300, 30, (HWND) NULL, (HMENU) NULL, myClass.hInstance, this);
		if (g_MsgHWND == NULL) { return -101; }
		SetWindowLongW(g_MsgHWND, GWL_USERDATA, LONG(this));
		bPluginIsFullScreen = bEnteredSetSizeMove = bRestoreOnFullScreenEnd = false;
		return 0;
	}

	void Cleanup()
	{
		if (g_EmbHWND) { DestroyWindow(g_EmbHWND); g_EmbHWND = NULL; }
		if (g_MsgHWND) { DestroyWindow(g_MsgHWND); g_MsgHWND = NULL; }
		UnregisterClassW(L"XMPVisPluginEmbed", NULL);
		UnregisterClassW(L"XMPVisPlugin", NULL);
		g_VisHWND = NULL;
	}

	void SetPluginDir(char *pcPluginDLL)
	{
		if (!pcPluginDLL[0]) { strcpy(pcPluginDir, g_pcXMPlayDir); return; }
		strcpy(pcPluginDir, pcPluginDLL) ;
		int iLen = strlen(pcPluginDir);
		char *p = pcPluginDir+iLen;
		while (p >= pcPluginDir && *p != '\\' && *p != '/') p--;
		if (p >= pcPluginDir) *p = 0;
	}
};

struct WaVisDLLLoader
{
	static void ShowError(char *pcPluginDLL, DWORD dwLastErr)
	{
		LPVOID lpMsgBuf; char pcMsg[350];
		char* pArgs[] = { pcPluginDLL , 0 };
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY, 0, ( dwLastErr ? dwLastErr : GetLastError()), 0, (LPTSTR) &lpMsgBuf, 200, pArgs);
		sprintf(pcMsg, "Error while loading Winamp DLL. Please check requirements and dependencies.\n%s", lpMsgBuf);
		LocalFree(lpMsgBuf);
		MessageBoxA(NULL, pcMsg, "Error", MB_ICONERROR);
	}

	static HINSTANCE GetVisInstance(char *pcPluginDLL, bool bShowError)
	{
		DWORD dwLastErr = 0;
		HINSTANCE VisInstance = NULL;
		if (pcPluginDLL[0])
		{
			FILE *pfDLLTest = fopen(pcPluginDLL, "r");
			if (pfDLLTest)
			{
				fclose(pfDLLTest);
				UINT iPrevPrevErrMode = SetErrorMode(0x7FFFFFFF);
				SetLastError(0);

				//ACTCTXA ctx = {sizeof(ACTCTX), 0};
				//ctx.lpSource = "xmp-wavis.manifest";
				//HANDLE hCtx = CreateActCtxA(&ctx);
				//ULONG_PTR actToken;
				//BOOL activated = ActivateActCtx(hCtx, &actToken);

				VisInstance = LoadLibraryExA(pcPluginDLL, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
				if (GetLastError() == 126)
				{
					UINT iPrevErrMode = SetErrorMode((bShowError?0:0x7FFFFFF));
					FreeLibrary(VisInstance);
					VisInstance = LoadLibraryExA(pcPluginDLL, NULL, 0  );
					SetErrorMode(iPrevErrMode);
				}
				SetErrorMode(iPrevPrevErrMode);

				//if (activated) DeactivateActCtx(0, actToken);
				//ReleaseActCtx(hCtx);
			}
			if (!VisInstance && bShowError) ShowError(pcPluginDLL, GetLastError());
		}
		return VisInstance;
	}

	static winampVisHeader* GetVisHeader(VisWND &wnd, HINSTANCE *pVisInstance, char *pcPluginDLL, bool bShowError)
	{
		winampVisHeader *visHeader = NULL;
		DWORD dwLastErr = 0;
		if (!*pVisInstance) return NULL;

		//typedef winampVisModule* (*getModuleProc)(int);
		typedef winampVisHeader* (*winampVisGetHeaderProc)(HWND);
		winampVisGetHeaderProc winampVisGetHeaderFunc = NULL;
		winampVisGetHeaderFunc = (winampVisGetHeaderProc)GetProcAddress(*pVisInstance, "winampVisGetHeader");
		if (!winampVisGetHeaderFunc && bShowError) ShowError(pcPluginDLL, GetLastError());
		if (!winampVisGetHeaderFunc) return NULL;

		BOOL bCreateTempWindow = (!wnd.g_MsgHWND);
		if (bCreateTempWindow) { wnd.create_msg_window(); }

		try { visHeader = winampVisGetHeaderFunc(wnd.g_MsgHWND); }
		catch (...) { dwLastErr = GetLastError(); visHeader = NULL; }
		if (bCreateTempWindow) { wnd.Cleanup(); }

		if (!visHeader)
		{
			//1400 = invalid hwnd
			Cleanup(pVisInstance, false);
			if (!bShowError || !pcPluginDLL[0]) return NULL;
			ShowError(pcPluginDLL, dwLastErr);
			return NULL;
		}

		/*
		pcPluginVisName[0] = pcPluginVisName[1] = 0;
		if (visHeader->description[0] && visHeader->description[1]) 
			strncpy(pcPluginVisName+1, visHeader->description+1, 99);
		pcPluginVisName[0] = visHeader->description[0];
		*/

		return visHeader;
	}

	static void Cleanup(HINSTANCE* pVisInstance, bool bCleanResStrings, winampVisModule **ppVisModule = NULL)
	{
		if (ppVisModule && *ppVisModule)
		{
			try { if ((*ppVisModule)->hwndParent && (*ppVisModule)->Quit) (*ppVisModule)->Quit(*ppVisModule); } catch (...) { }
			*ppVisModule = NULL;
		}
		if (*pVisInstance != NULL)
		{
			if (bCleanResStrings) LoadedResString::Clear(*pVisInstance);
			FreeLibrary(*pVisInstance); *pVisInstance = NULL; 
		}
	}

	static int ParsePluginDir(VisWND &wnd, char* pcDirectory, char* pcFileFind, HWND hWnd, int dlgid)
	{
		char pcFile[MAX_PATH];
		int iRetCurSel = -1;
		strcpy(pcFile, pcDirectory);
		strcat(pcFile, "\\*");
		WIN32_FIND_DATAA FindFileData;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		if ((hFind = FindFirstFileA(pcFile, &FindFileData)) != INVALID_HANDLE_VALUE)
		{
			do
			{
				int len = strlen(FindFileData.cFileName);
				if (!stricmp(FindFileData.cFileName+len-4, ".dll"))
				{
					strcpy(pcFile, pcDirectory);
					strcat(pcFile, "\\");
					strcat(pcFile, FindFileData.cFileName);

					HINSTANCE visInstance = GetVisInstance(pcFile, false);
					winampVisHeader *visHeader = GetVisHeader(wnd, &visInstance, pcFile, false);
					if (visHeader)
					{
						char pcListEntry[255];
						strncpy(pcListEntry, visHeader->description, 255);
						strncat(pcListEntry, " - ", 255);
						strncat(pcListEntry, FindFileData.cFileName, 255);
						int pos = SendDlgItemMessage(hWnd, dlgid, LB_ADDSTRING, 0, (LPARAM)pcListEntry);
						char *pcListFile = new char[strlen(pcFile)+1];
						strcpy(pcListFile, pcFile);
						SendDlgItemMessage(hWnd, dlgid, LB_SETITEMDATA, pos, (LPARAM)pcListFile);
						if (!strcmp(pcFile, pcFileFind)) iRetCurSel = pos;
					}
					Cleanup(&visInstance, !!visHeader);
				}
				else if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && FindFileData.cFileName[0] != '.')
				{
					strcpy(pcFile, pcDirectory);
					strcat(pcFile, "\\");
					strcat(pcFile, FindFileData.cFileName);
					int iSubDirSel = ParsePluginDir(wnd, pcFile, pcFileFind, hWnd, dlgid);
					if (iSubDirSel >= 0) iRetCurSel = iSubDirSel;
				}								
			} while (FindNextFileA(hFind, &FindFileData));
			FindClose(hFind);
		}
		return iRetCurSel;
	}

	static int FillPluginList(char* pcDirectory, char* pcFileFind, HWND hWnd, int dlgid)
	{
		VisWND wnd(true);
		return ParsePluginDir(wnd, pcDirectory, pcFileFind, hWnd, dlgid);
	}

	static int FillVisModuleList(char *pcPluginDLL, HWND h, int dlgid)
	{
		int i = 0;
		while (SendDlgItemMessage(h,dlgid, CB_GETCOUNT, 0, 0)) SendDlgItemMessage(h,dlgid, CB_DELETESTRING, 0, 0);
		if (!pcPluginDLL || !pcPluginDLL[0]) return 0;
		VisWND wnd(true);
		HINSTANCE VisInstance = GetVisInstance(pcPluginDLL, FALSE);
		winampVisHeader *visHeader = GetVisHeader(wnd, &VisInstance, pcPluginDLL, FALSE);
		winampVisModule *visModule;
		while (visHeader && (visModule = visHeader->getModule(i++)))
		{
			SendDlgItemMessage(h,dlgid, CB_ADDSTRING, 0, (LPARAM)visModule->description);
		}
		Cleanup(&VisInstance, !!visHeader);
		return i-1;
	}

	static int AddPluginToList(char* pcPluginDLL, HWND hWnd, int dlgid)
	{
		if (!pcPluginDLL[0]) return 0;
		int iSelIndex = 0;
		int num = SendDlgItemMessage(hWnd, dlgid, LB_GETCOUNT, 0, 0); 
		for (int i = 1; i < num; i++)
		{
			char* pcData = (char*)SendDlgItemMessage(hWnd, dlgid, LB_GETITEMDATA, i, 0);
			if (!stricmp(pcPluginDLL, (pcData ? pcData : ""))) { iSelIndex = i; break; }
		}
		if (iSelIndex == 0)
		{
			HINSTANCE visInstance = GetVisInstance(pcPluginDLL, true);
			if (visInstance)
			{
				VisWND wnd(true);
				winampVisHeader *visHeader = GetVisHeader(wnd, &visInstance, pcPluginDLL, true);
				if (visHeader)
				{
					int len = strlen(pcPluginDLL);
					char *p; for (p = pcPluginDLL + len; p >= pcPluginDLL && *p != '\\' && *p != '/'; p--); p++;

					char pcListEntry[255];
					strncpy(pcListEntry, visHeader->description, 255);
					strncat(pcListEntry, " - ", 255);
					strncat(pcListEntry, p, 255);

					iSelIndex = SendDlgItemMessage(hWnd, dlgid, LB_ADDSTRING, 0, (LPARAM)pcListEntry);
					char *pcListFile = new char[len+1];
					strcpy(pcListFile, pcPluginDLL);
					SendDlgItemMessage(hWnd, dlgid, LB_SETITEMDATA, iSelIndex, (LPARAM)pcListFile);
				}
				Cleanup(&visInstance, !!visHeader);
			}
		}
		return iSelIndex;
	}
};

struct WinampVisWrapper
{
	HINSTANCE VisInstance;
	winampVisModule* pVisModule;

	VisWND wnd;
	WCHAR wcCurrentSong[SONG_TEXT_LENGTH];
	clock_t clTimeLastCheckSong;

	struct
	{
		char pcPluginDLL[MAX_PATH];
		int iModuleNum;
		bool bRunning;
	} conf;

	int iCurrentChans, iCurrentSRate;
	char pcPluginVisName[100], pcPluginModName[100];
	int iBufferDataSent;
	float fBufferFactor;

	WinampVisWrapper()
	{
		memset(this, 0, sizeof(*this)); 
		fBufferFactor = 1.0f;
	}

	void UpdateSongTitle()
	{
		if (wnd.g_MsgHWND && !wnd.bStopPlugin && g_XMPlayHWND && ((!clTimeLastCheckSong) || ((clock() - clTimeLastCheckSong) > CLOCKS_PER_SEC/3)))
		{
			clTimeLastCheckSong = 0x7FFFFFFF;
			WCHAR wcTitle[SONG_TEXT_LENGTH] = L"";

			bool bPlaying = false;
			if ((!XMPlay_Status) || (XMPlay_Status->IsPlaying()))
			{
				bPlaying = true;
				char* pNewSong = NULL;
				//long iListPos;
				//if ((!SendMessageTimeout(g_XMPlayHWND, WM_WA_IPC, 0, IPC_GETLISTPOS, SMTO_ABORTIFHUNG, 300, (LPDWORD)&iListPos)) || (iListPos < 0) || (bStopPlugin)) return;
				//if (!SendMessageTimeout(g_XMPlayHWND, WM_WA_IPC, iListPos, IPC_GETPLAYLISTTITLE, SMTO_ABORTIFHUNG, 300, (LPDWORD)&pNewSong)) return;
				if (XMPlay_Misc) pNewSong = XMPlay_Misc->GetTag(TAG_TRACK_TITLE);
				if (pNewSong && (pNewSong[0] || pNewSong[1]))
				{
					MultiByteToWideChar(CP_UTF8, 0, pNewSong, -1, wcTitle, SONG_TEXT_LENGTH);
					if (pNewSong) XMPlay_Misc->Free(pNewSong);
				}
				else
				{
					if (pNewSong) XMPlay_Misc->Free(pNewSong);
					pNewSong = XMPlay_Misc->GetTag(TAG_FORMATTED_TITLE);
					if (pNewSong && (pNewSong[0] || pNewSong[1])) MultiByteToWideChar(CP_UTF8, 0, pNewSong, -1, wcTitle, SONG_TEXT_LENGTH);
					if (pNewSong) XMPlay_Misc->Free(pNewSong);
				}
			}
			else { wcscpy(wcTitle, L""); }

			if (wcscmp(wcTitle, wcCurrentSong) != 0 && !wnd.bStopPlugin)
			{
				wcscpy(wcCurrentSong, wcTitle);
				long iListPos;
				if ((!SendMessageTimeout(g_XMPlayHWND, WM_WA_IPC, 0, IPC_GETLISTPOS, SMTO_ABORTIFHUNG, 300, (LPDWORD)&iListPos)) || (iListPos < 0) || (wnd.bStopPlugin)) return;
				_snwprintf(wcTitle, SONG_TEXT_LENGTH, L"%d. %s - Winamp", iListPos, wcCurrentSong);
				if (wnd.g_MsgHWND) SetWindowTextW(wnd.g_MsgHWND, wcTitle);
			}

			bool bTopMost = ((g_XMPlayHWND) && (!wnd.bStopPlugin) && (GetWindowLongA(g_XMPlayHWND, GWL_EXSTYLE) & WS_EX_TOPMOST)) ? true : false;
			if (wnd.g_VisHWND && (((GetWindowLongA(wnd.g_VisHWND, GWL_EXSTYLE) & WS_EX_TOPMOST) ? true : false) != bTopMost)) SetWindowPos(wnd.g_VisHWND, (bTopMost ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
			if (wnd.g_EmbHWND && (((GetWindowLongW(wnd.g_EmbHWND, GWL_EXSTYLE) & WS_EX_TOPMOST) ? true : false) != bTopMost)) SetWindowPos(wnd.g_EmbHWND, (bTopMost ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
			if (wnd.g_MsgHWND && (((GetWindowLongW(wnd.g_MsgHWND, GWL_EXSTYLE) & WS_EX_TOPMOST) ? true : false) != bTopMost)) SetWindowPos(wnd.g_MsgHWND, (bTopMost ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
			if (wnd.g_MsgHWND && g_XMPlayHWND) { RECT r; GetWindowRect(g_XMPlayHWND, &r); SetWindowPos(wnd.g_MsgHWND, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOACTIVATE); }

			clTimeLastCheckSong = clock();
		}
	}

	void Cleanup()
	{
		WaVisDLLLoader::Cleanup(&VisInstance, true, &pVisModule);
		wnd.Cleanup();
		conf.bRunning = false;
	}

	void CopyModName(char *despcription)
	{
		pcPluginModName[0] = 0;
		if (strncmp(pcPluginVisName, despcription, 99) == 0) return;
		pcPluginModName[1] = 0;
		if (despcription[0] && despcription[1]) 
			strncpy(pcPluginModName+1, despcription+1, 99);
		pcPluginModName[0] = despcription[0];
	}

	bool SetVisDLL()
	{
		wnd.SetPluginDir(conf.pcPluginDLL);
		VisInstance = WaVisDLLLoader::GetVisInstance(conf.pcPluginDLL, TRUE);
		winampVisHeader *visHeader = WaVisDLLLoader::GetVisHeader(wnd, &VisInstance, conf.pcPluginDLL, TRUE);
		if (!visHeader) { Cleanup(); return false; }

		if (visHeader->description[0] && visHeader->description[1]) 
			strncpy(pcPluginVisName+1, visHeader->description+1, 99);
		pcPluginVisName[0] = visHeader->description[0];

		pVisModule = visHeader->getModule(conf.iModuleNum);
		if (pVisModule == NULL) { Cleanup(); return false; }

		pVisModule->hDllInstance = VisInstance;
		pVisModule->nCh = 2;
		pVisModule->sRate = iCurrentSRate; //lCurrentSongSampleRate*512/576;
		memset(pVisModule->waveformData, 0, sizeof(pVisModule->waveformData));
		memset(pVisModule->spectrumData, 0, sizeof(pVisModule->spectrumData));
		CopyModName(pVisModule->description);
		return true;
	}

	void ShowConfig()
	{
		if (pVisModule && pVisModule->Config) { pVisModule->Config(pVisModule); return; }
		if (!wnd.g_MsgHWND && wnd.create_msg_window() != 0) return;
		if (!SetVisDLL()) return;
		pVisModule->hwndParent = wnd.g_MsgHWND;
		if (pVisModule->Config) { pVisModule->Config(pVisModule); }
		Cleanup();
	}

	void LoadModuleDescription()
	{
		if (!SetVisDLL()) return;
		Cleanup();
	}

	void UpdateVisData()
	{
		if (!g_pfBuffer || iBufferDataSent*fBufferFactor > g_BufferLatency) return;

		while (g_BufferLocked) Sleep(0);
		g_BufferReadingThreads++;

		int iStartPos = (g_BufferPos >= g_BufferLatency ? g_BufferPos - g_BufferLatency : g_BufferSize + g_BufferPos - g_BufferLatency);
		iStartPos = (g_BufferSize + iStartPos + ((int)(iBufferDataSent*fBufferFactor)) + ((int)(576.0f*fBufferFactor)) - 576)%g_BufferSize;

		float *piend = g_pfBuffer + 2*g_BufferSize;
		if (pVisModule->waveformNch)
		{
			float *pi = g_pfBuffer + 2*iStartPos;
			for (int x = 0; x < 576; x++)
			{
				if (pi == piend) pi = g_pfBuffer;
				pVisModule->waveformData[0][x] = (unsigned char)(-0.1f+(*pi++)*128.0f);
				pVisModule->waveformData[1][x] = (unsigned char)(-0.1f+(*pi++)*128.0f);
			}
		}

		float in0[512], in1[512], out0r[512], out1r[512], out0i[512], out1i[512];
		if (pVisModule->spectrumNch)
		{
			const float x2t = 1.f / 512.f * 576.f;
			float *pi = g_pfBuffer + 2*iStartPos;
			for (int x = 0; x < 512; x++)
			{
				float t = x * x2t; int tint = (int)t; float trest = t - tint, t1rest = 1.0f - trest;
				int x0 = ((iStartPos + tint) % g_BufferSize) << 1, x1 = ((iStartPos + tint + 1) % g_BufferSize) << 1;
				in0[x] = g_pfBuffer[x0    ] * t1rest + g_pfBuffer[x1    ] * trest;
				in1[x] = g_pfBuffer[x0 + 1] * t1rest + g_pfBuffer[x1 + 1] * trest;
			}
		}

		iBufferDataSent += 576;
		g_BufferReadingThreads--;

		if (pVisModule->spectrumNch)
		{
			fft_forward(in0, 512, out0r, out0i);
			fft_forward(in1, 512, out1r, out1i);
			for (int x = 0; x < 576; x++)
			{
				int i = (x >> 1);
				float i0 = out0i[i], i1 = out1i[i], r0 = out0r[i], r1 = out1r[i];
				if (x & 1)
				{
					i0 = (fabs(i0) + fabs(out0i[i+1])) * .5f;
					i1 = (fabs(i1) + fabs(out1i[i+1])) * .5f;
					r0 = (fabs(r0) + fabs(out0r[i+1])) * .5f;
					r1 = (fabs(r1) + fabs(out1r[i+1])) * .5f;
				}
				int temp0 = (int)(10.f*sqrt((i0 * i0) + (r0 * r0)))-4;
				int temp1 = (int)(10.f*sqrt((i1 * i1) + (r1 * r1)))-4;
				pVisModule->spectrumData[0][x] = (temp0 > 255 ? 255 : (temp0 < 1 ? 0 : temp0));
				pVisModule->spectrumData[1][x] = (temp1 > 255 ? 255 : (temp1 < 1 ? 0 : temp1));
			}
		}
	}

	// Based on fft.hpp - Public-domain single-header library by Alexander Nadeau
	// Implementing radix-2 decimation-in-time FFT (i.e. FFT for powers of 2)
	// https://github.com/wareya/fft
	static void fft_forward(const float* input_real, size_t size, float* output_real, float* output_imag, size_t gap = 1)
	{
		size_t half = size/2;
		if (half == 1)
		{
			output_real[0] = input_real[0];
			output_real[1] = input_real[gap];
			output_imag[0] = output_imag[1] = 0;
		}
		else
		{
			// This algorithm works by extending the concept of how two-bin DFTs (discrete fourier transform) work, in order to correlate decimated DFTs, recursively.
			// No, I'm not your guy if you want a proof of why it works, but it does.
			fft_forward(input_real,         half, output_real,          output_imag,          gap*2);
			fft_forward(&(input_real[gap]), half, &(output_real[half]), &(output_imag[half]), gap*2);
		}

		// non-combed decimated output to non-combed correlated output
		float sizeInv = 1.f / size;
		for(size_t i = 0; i < half; i++)
		{
			float iDivSize = i * sizeInv;
			float a_real = output_real[i], b_real = output_real[i+half];
			float a_imag = output_imag[i], b_imag = output_imag[i+half];
			float twiddle_real = cos( 6.283185307179586476925286766559f * iDivSize);
			float twiddle_imag = sin(-6.283185307179586476925286766559f * iDivSize);
			// complex multiplication (vector angle summing and length multiplication)
			float bias_real = b_real * twiddle_real - b_imag * twiddle_imag;
			float bias_imag = b_imag * twiddle_real + b_real * twiddle_imag;
			// real output (sum of real parts)
			output_real[i     ] = a_real + bias_real;
			output_real[i+half] = a_real - bias_real;
			// imag output (sum of imaginary parts)
			output_imag[i     ] = a_imag + bias_imag;
			output_imag[i+half] = a_imag - bias_imag;
		}
	}

	DWORD ThreadProc()
	{
		int iInit = 1;
		wnd.bStopPlugin = false;
		conf.bRunning = true;

		clTimeLastCheckSong = wcCurrentSong[0] = 0;
		wnd.g_MsgHWND = wnd.g_EmbHWND = wnd.g_VisHWND = NULL;

		if (wnd.create_msg_window() != 0) { ExitThread(1); return 1; }

		SetVisDLL();

		if (pVisModule == NULL || wnd.bStopPlugin) goto exit_thread;
		pVisModule->hwndParent = wnd.g_MsgHWND;

		if (wnd.g_MsgHWND && !wnd.bStopPlugin)
		{
			if (iInit != 0 && !wnd.bStopPlugin)
			{
				//Chances are high that the main window was destroyed during init because we don't want error messages of any kind
				if (!wnd.g_MsgHWND && wnd.create_msg_window() != 0) { }
				if (!wnd.g_EmbHWND && wnd.create_emb_window() != 0) { }
				pVisModule->hwndParent = wnd.g_MsgHWND;
				iInit = pVisModule->Init(pVisModule);
			}

			if (iInit == 0)
			{
				//if (!bStopPlugin) SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

				int iTimeNextRender = -2000;
				int iNumDrawsLeft = 3;
				int TEST_clstart = clock(), TEST_numdraws = 0, TEST_clnow = 0;
				while (!wnd.bStopPlugin || iNumDrawsLeft)
				{
					if (wnd.keep_alive()) wnd.bStopPlugin = true;
					if (clock() >= iTimeNextRender - 10)
					{
						UpdateSongTitle();
						UpdateVisData();
						pVisModule->sRate = iCurrentSRate; //lCurrentSongSampleRate*512/576;

						while (clock() < iTimeNextRender) Sleep(1);
						iTimeNextRender = (clock() - iTimeNextRender > pVisModule->delayMs ? clock() : iTimeNextRender) + pVisModule->delayMs;
						if (pVisModule->Render(pVisModule)) wnd.bStopPlugin = true;
						if (iNumDrawsLeft) iNumDrawsLeft--;

						TEST_numdraws++;
					}
					TEST_clnow = clock();
					Sleep(1);
				}
			}
		}

	exit_thread:
		Cleanup();
		ExitThread(0);
		return 0;
	}
};

struct WaVisDSP
{
	HANDLE hThread;
	HWND hwndConfig;
	bool InsidePluginShowConfig;
	WinampVisWrapper vis;

	WaVisDSP()
	{
		hwndConfig = 0;
		hThread = 0;
		InsidePluginShowConfig = false;
	}

	~WaVisDSP()
	{
		if (hwndConfig) EndDialog(hwndConfig, 0); // close config window before freeing the DSP
	
		vis_stop_plugin();
	}

	bool vis_is_running()
	{
		DWORD lpExitCode = 0; 
		if (hThread && GetExitCodeThread(hThread, &lpExitCode) && (lpExitCode != STILL_ACTIVE)) hThread = NULL;
		return hThread!=NULL;
	}

	void vis_stop_plugin()
	{
		vis.wnd.bStopPlugin = true;
	
		if (!hThread) vis.Cleanup();
		if (hThread && WaitForSingleObject(hThread, 5000) != WAIT_OBJECT_0) 
		{
			TerminateThread(hThread, 0);
			vis.Cleanup();
		}

		try { if (hThread) CloseHandle(hThread); hThread = NULL; } catch (...) { };
		try { if (hThread) CloseHandle(hThread); hThread = NULL; } catch (...) { };
		try { if (hThread) CloseHandle(hThread); hThread = NULL; } catch (...) { };
	}

	static DWORD __stdcall ThreadProcStatic(LPVOID lpParam)
	{
		WinampVisWrapper* pWinampVisWrapper = (WinampVisWrapper*)lpParam;
		return pWinampVisWrapper->ThreadProc();
	}

	void vis_start_plugin()
	{
		if (!vis.conf.pcPluginDLL[0]) return;
		vis_stop_plugin(); //close handle and stuff
		DWORD lpThreadID;
		hThread = CreateThread(NULL, 0, ThreadProcStatic, (LPVOID)&vis, 0, &lpThreadID);
	}

	static bool no_vis_running_free_buffer()
	{
		int num = 0;
		for (int i = 0; i < iWaVissNum; i++) if (pWaViss[i]->vis.conf.bRunning) num++;
		if (num) return false;
		if (g_pfBuffer)
		{
			delete g_pfBuffer;
			g_pfBuffer = NULL;
			g_BufferSize = g_BufferPos = g_BufferLatency = 0;
			g_BufferReadingThreads = 0;
			g_BufferLocked = false;
		}
		return true;
	}

	static void WINAPI DSP_About(HWND win)
	{
		MessageBoxA(win,"Winamp Vis Wrapper "XMPDSPVERSION"\n\nRuns a Winamp visualization plugin in XMPlay.\n\nhttps://github.com/schellingb/xmp-wavis", WaVisXMPDSPName, MB_ICONINFORMATION);
	}

	static void *WINAPI DSP_New()
	{
		if (iWaVissNum >= 64) return NULL;
		if (!g_XMPlayHWND) g_XMPlayHWND = XMPlay_Misc->GetWindow();
		return (pWaViss[iWaVissNum++] = new WaVisDSP()); 
	}

	static void WINAPI DSP_Free(void *inst)
	{
		int i = 0;
		while (i < iWaVissNum && pWaViss[i] != inst) i++;
		iWaVissNum--;
		while (i < iWaVissNum) { pWaViss[i] = pWaViss[i+1]; i++; }
		delete (WaVisDSP*)inst;
		no_vis_running_free_buffer();
	}

	// get description for plugin list
	static const char *WINAPI DSP_GetDescription(void *inst) { return ((WaVisDSP*)inst)->DSP_GetDescription(); }
	const char *DSP_GetDescription()
	{
		static char pcDescription[120];
		strncpy(pcDescription, WaVisXMPDSPName, 120);
		if (!vis.pcPluginVisName[0] && !vis.pcPluginModName[0]) strcat(pcDescription, " - no plugin");
		else
		{
			//if (vis.pcPluginVisName) strcat(pcDescription, " - ");
			//strcat(pcDescription, vis.pcPluginVisName);
			if (vis.pcPluginModName[0] || vis.pcPluginVisName[0]) strcat(pcDescription, " - ");
			strncat(pcDescription, (vis.pcPluginModName[0] ? vis.pcPluginModName : vis.pcPluginVisName), 120);
			strncat(pcDescription, (vis.conf.bRunning ? " (on)" : " (off)"), 120);
		}
		return pcDescription;
	}

	// show config options
	static void WINAPI DSP_Config(void *inst, HWND win) { ((WaVisDSP*)inst)->DSP_Config(win); }
	void DSP_Config(HWND win)
	{
		DialogBoxParamA(dllinst, (char*)1000, win, &DSPDialogProcStatic, (LONG)this);
	}

	// get DSP config
	static DWORD WINAPI DSP_GetConfig(void *inst, void *config) { return ((WaVisDSP*)inst)->DSP_GetConfig(config); }
	DWORD DSP_GetConfig(void *config)
	{
		memcpy(config, &vis.conf, sizeof(vis.conf));
		return sizeof(vis.conf); // return size of config info
	}

	// set DSP config
	static BOOL WINAPI DSP_SetConfig(void *inst, void *config, DWORD size) { return ((WaVisDSP*)inst)->DSP_SetConfig(config, size); }
	BOOL DSP_SetConfig(void *config, DWORD size)
	{
		memcpy(&vis.conf, config, sizeof(vis.conf));
		FILE *pfDLLTest = fopen(vis.conf.pcPluginDLL, "r");
		if (!pfDLLTest) return FALSE;
		fclose(pfDLLTest);
		if (vis.conf.bRunning) vis_start_plugin();
		else vis.LoadModuleDescription();
		return TRUE;
	}

	// process the sample data
	static DWORD WINAPI DSP_Process(void *inst, float *srce, DWORD count) { return ((WaVisDSP*)inst)->DSP_Process(srce, count); }
	DWORD DSP_Process(float *buffer, /*DWORD*/ int count)
	{
		if (this != pWaViss[0] || no_vis_running_free_buffer()) return count;

		int iCurLat = XMPlay_Status->GetLatency();
		int iReq = count + iCurLat;
		int iMin = 12000 + (iReq - iReq % 10000);
		if (g_BufferSize < iMin || (g_BufferSize > iMin + 25000 && iCurLat))
		{
			float* new_pfBuffer = new float[iMin*2];
			int new_BufferPos;
			if (g_pfBuffer)
			{
				if (g_BufferSize > iMin)
				{
					UINT iStart = (iMin > g_BufferPos ? g_BufferSize - iMin + g_BufferPos : g_BufferPos - iMin);
					if (iMin > g_BufferPos)
					{
						memcpy(new_pfBuffer, g_pfBuffer+2*(g_BufferSize - iMin + g_BufferPos), 2*sizeof(float)*(iMin - g_BufferPos));
						memcpy(new_pfBuffer+2*(iMin - g_BufferPos), g_pfBuffer, 2*sizeof(float)*g_BufferPos);
					}
					else 
					{
						memcpy(new_pfBuffer, g_pfBuffer+2*(g_BufferPos - iMin), 2*sizeof(float)*iMin);
					}
					new_BufferPos = 0;
				}
				else
				{
					memcpy(new_pfBuffer, g_pfBuffer+2*(g_BufferPos), 2*sizeof(float)*(g_BufferSize - g_BufferPos));
					memcpy(new_pfBuffer+2*(g_BufferSize - g_BufferPos), g_pfBuffer, 2*sizeof(float)*g_BufferPos);
					new_BufferPos = g_BufferSize;				
				}
			}
			else new_BufferPos = 0;

			g_BufferLocked = true;
			while (g_BufferReadingThreads) { Sleep(0); if (g_BufferReadingThreads >= iWaVissNum) g_BufferReadingThreads--; }
			if (g_pfBuffer) delete g_pfBuffer;
			g_pfBuffer = new_pfBuffer;
			g_BufferSize = iMin;
			g_BufferPos = new_BufferPos;
			g_BufferLocked = false;
		}

		if (vis.iCurrentChans == 2)
		{
			if (g_BufferPos + count < g_BufferSize)
			{
				memcpy(g_pfBuffer+2*g_BufferPos, buffer, 2*sizeof(float)*count);
			}
			else
			{
				memcpy(g_pfBuffer+2*g_BufferPos, buffer, 2*sizeof(float)*(g_BufferSize-g_BufferPos));
				memcpy(g_pfBuffer, buffer+2*(g_BufferSize-g_BufferPos), 2*sizeof(float)*(count-(g_BufferSize-g_BufferPos)));
			}
		}
		else
		{
			float *pi = buffer;
			float *piend = buffer + vis.iCurrentChans*count;
			float *po = g_pfBuffer + 2*g_BufferPos;
			float *poend = g_pfBuffer + 2*g_BufferSize;
			if (vis.iCurrentChans == 1)
			{
				while (pi < piend)
				{
					if (po == poend) po = g_pfBuffer;
					*po++ = *pi;
					*po++ = *pi++;
				}		
			}
			else
			{
				while (pi < piend)
				{
					if (po == poend) po = g_pfBuffer;
					*po++ = *pi;
					*po++ = *pi++;
					pi += vis.iCurrentChans-2;
				}		
			}
		}

		for (int i = 0; i < iWaVissNum; i++)
		{
			WinampVisWrapper &vis = pWaViss[i]->vis;
			if (!vis.conf.bRunning) continue;
			if (!vis.iBufferDataSent) vis.fBufferFactor = 1.0f;
			else { vis.fBufferFactor = count / (float)vis.iBufferDataSent; vis.iBufferDataSent = 0; if (vis.fBufferFactor < 0.5f) vis.fBufferFactor = 0.5f; }
			g_BufferLatency = iCurLat + count;
		}

		g_BufferPos += (g_BufferPos + (int)count < g_BufferSize ? count : count - g_BufferSize);

		return count;
	}

	// new file has been opened (or closed if file=NULL) (not on radio/subtracks)
	static void WINAPI DSP_NewTrack(void *inst, const char *file) {} //{ ((WaVisDSP*)inst)->DSP_NewTrack(file); }

	// set the sample format at start (or end if form=NULL) of playback
	static void WINAPI DSP_SetFormat(void *inst, const XMPFORMAT *form) { ((WaVisDSP*)inst)->DSP_SetFormat(form); }
	void DSP_SetFormat(const XMPFORMAT *form)
	{
		if (form)
		{
			vis.iCurrentSRate = form->rate;
			vis.iCurrentChans = form->chan;
			//DSP_Reset();
		}
		else 
		{
			vis.iCurrentSRate = 0;
			vis.iCurrentChans = 0;
		}
	}

	// reset DSP when seeking
	static void WINAPI DSP_Reset(void *inst) {} //{ ((WaVisDSP*)inst)->DSP_Reset(); }

	// shortcut to enable the DSP
	static void WINAPI ShortcutWaVisToggle()
	{
		for (int i = 0; i < iWaVissNum; i++)
		{
			if (pWaViss[i]->InsidePluginShowConfig) continue;
			if (pWaViss[i]->vis_is_running()) pWaViss[i]->vis_stop_plugin();
			else pWaViss[i]->vis_start_plugin();
		}
	}

	void OpenPlugin(HWND hWnd, int dlgid)
	{
		char pcOpenFileName[MAX_PATH] = "";
		OPENFILENAMEA ofn;
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = hWnd;
		ofn.lpstrFilter = "Winamp Visualization Plugins (vis*.dll)\0vis*.dll\0All DLL files (*.dll)\0*.dll\0";
		ofn.lpstrFile = pcOpenFileName;
		ofn.nMaxFile = sizeof(pcOpenFileName);
		ofn.lpstrTitle = "Open Winamp Visualization Plugin";
		ofn.lpstrDefExt = "dll";
		ofn.Flags = OFN_PATHMUSTEXIST;
		try { if (!GetOpenFileNameA(&ofn)) return; } catch (...) { return; }
		int iNewIndex = WaVisDLLLoader::AddPluginToList(pcOpenFileName, hWnd, dlgid);
		if (!iNewIndex) return;
		SendDlgItemMessage(hWnd, dlgid, LB_SETCURSEL, iNewIndex, 0);
		SendMessageA(hWnd, WM_COMMAND, MAKELONG(dlgid, LBN_SELCHANGE), dlgid);
	}

	static BOOL __stdcall DSPDialogProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		WaVisDSP* pWaVisStuff = (WaVisDSP*)GetWindowLong(hWnd, GWL_USERDATA);
		if (message == WM_INITDIALOG) { pWaVisStuff = (WaVisDSP*)lParam; SetWindowLong(hWnd, GWL_USERDATA, LONG(pWaVisStuff)); }
		if (pWaVisStuff) return pWaVisStuff->DSPDialogProc(hWnd,message,wParam,lParam);
		return DefWindowProc(hWnd,message,wParam,lParam);
	}

	BOOL DSPDialogProc(HWND h, UINT m, WPARAM w, LPARAM l)
	{
		#define MESS(id,m,w,l) SendDlgItemMessage(h,id,m,(WPARAM)(w),(LPARAM)(l))
		switch (m)
		{
			/*
			case WM_MOUSEMOVE:
				char pcText[512];
				sprintf(pcText, "Buffer: Size = %d - Pos = %d - Lat = %d\nSent: %d - Factor: %f", g_BufferSize, g_BufferPos, g_BufferLatency, vis.iBufferDataSent, vis.fBufferFactor);
				SetWindowTextA(GetDlgItem(h, 20), pcText);
				break;
			*/

			case WM_COMMAND:
				switch (LOWORD(w))
				{
					case IDCANCEL: EndDialog(h,0); break;

					case 10:
						if (HIWORD(w) == LBN_SELCHANGE)
						{
							int nItem = MESS(10, LB_GETCURSEL, 0, 0); 
							if (nItem < 0) break;
							char* pcData = (char*)MESS(10, LB_GETITEMDATA, nItem, 0);
							if (strcmp(vis.conf.pcPluginDLL, (pcData ? pcData : "")))
							{
								vis_stop_plugin();
								strcpy(vis.conf.pcPluginDLL, (pcData ? pcData : ""));
							}
							if (vis.conf.pcPluginDLL[0])
							{
								for (int i = 0; i < iWaVissNum; i++)
								{
									if (pWaViss[i] != this && !strcmp(vis.conf.pcPluginDLL, pWaViss[i]->vis.conf.pcPluginDLL))
									{
										vis.conf.pcPluginDLL[0] = 0;
										MESS(10, LB_SETCURSEL, 0, 0);
									}
								}
							}
							int iNum = WaVisDLLLoader::FillVisModuleList(vis.conf.pcPluginDLL, h, 11);	
							//if (!iNum) break;
							int iSel = vis.conf.iModuleNum;
							if (iSel < 0 || iSel >= iNum) iSel = 0;
							MESS(11, CB_SETCURSEL, iSel, 0);
							SendMessageA(h, WM_COMMAND, MAKELONG(11, CBN_SELENDOK), 11);
						}
						if (HIWORD(w) == LBN_DBLCLK && vis.conf.pcPluginDLL[0])
						{
							if (!vis_is_running()) vis_start_plugin();
						}
						break;

					case 11:
						if (HIWORD(w) == CBN_SELENDOK)
						{
							if (!vis.conf.pcPluginDLL[0])
							{
								vis.pcPluginVisName[0] = vis.pcPluginModName[0] = 0;
								break;
							}
							if (vis.conf.iModuleNum != MESS(11, CB_GETCURSEL, 0, 0)) vis_stop_plugin();
							vis.conf.iModuleNum = MESS(11, CB_GETCURSEL, 0, 0);
							int len = MESS(11, CB_GETLBTEXTLEN, vis.conf.iModuleNum, 0);
							char *pcDesc = "";
							if (len > 0) { pcDesc = new char[len+1]; MESS(11, CB_GETLBTEXT, vis.conf.iModuleNum, pcDesc); }
							vis.CopyModName(pcDesc);
							if (len > 0) delete pcDesc;
						}
						break;

					case 12:
						bool bOnTop; bOnTop = (g_XMPlayHWND && (GetWindowLong(g_XMPlayHWND, GWL_EXSTYLE) & WS_EX_TOPMOST));
						if (bOnTop) SetWindowPos(g_XMPlayHWND, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
						EnableWindow(h, FALSE);
						InsidePluginShowConfig = true;
						vis.ShowConfig();
						InsidePluginShowConfig = false;
						EnableWindow(h, TRUE);
						if (bOnTop) SetWindowPos(g_XMPlayHWND, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
						SetFocus(h);
						break;

					case 13:
						vis_start_plugin();
						break;

					case 14:
						vis_stop_plugin();
						break;

					case 15:
						OpenPlugin(h, 10);
						break;

				}
				EnableWindow(GetDlgItem(h, 12), vis.conf.pcPluginDLL[0]);
				EnableWindow(GetDlgItem(h, 13), vis.conf.pcPluginDLL[0] && !vis_is_running());
				EnableWindow(GetDlgItem(h, 14), vis.conf.pcPluginDLL[0] && vis_is_running());
				return 1;

			case WM_ACTIVATE:
				if (!InsidePluginShowConfig || !l || LOWORD(w) != WA_INACTIVE || (HWND)l == g_XMPlayHWND) break;
				SetWindowPos((HWND)l, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOREPOSITION|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
				break;

			case WM_INITDIALOG:
				hwndConfig = h;
				MESS(10, LB_ADDSTRING, 0, (LPARAM)"no plugin");
				int nItem; nItem = WaVisDLLLoader::FillPluginList(g_pcXMPlayDir, vis.conf.pcPluginDLL, h, 10);
				MESS(10, LB_SETCURSEL, (nItem > 0 ? nItem : WaVisDLLLoader::AddPluginToList(vis.conf.pcPluginDLL, h, 10)), 0);
				SendMessageA(h, WM_COMMAND, MAKELONG(10, LBN_SELCHANGE), 10);
				EnableWindow(GetDlgItem(h, 12), vis.conf.pcPluginDLL[0]);
				EnableWindow(GetDlgItem(h, 13), vis.conf.pcPluginDLL[0] && !vis_is_running());
				EnableWindow(GetDlgItem(h, 14), vis.conf.pcPluginDLL[0] && vis_is_running());
				return 1;

			case WM_DESTROY:
				hwndConfig = 0;
				for (int i=1;i<MESS(10, LB_GETCOUNT, 0, 0);i++) delete (char*)MESS(10, LB_GETITEMDATA, i, 0);
				break;
		}
		return 0;
		#undef MESS
	}
};

// get the plugin's XMPDSP interface
#pragma comment(linker, "/EXPORT:XMPDSP_GetInterface2=_XMPDSP_GetInterface2@8")
extern "C" XMPDSP *WINAPI XMPDSP_GetInterface2(DWORD face, InterfaceProc faceproc)
{
	if (face != XMPDSP_FACE) return NULL;
	XMPlay_Misc = (XMPFUNC_MISC*)faceproc(XMPFUNC_MISC_FACE);
	XMPlay_Status = (XMPFUNC_STATUS*)faceproc(XMPFUNC_STATUS_FACE);

	int iLen = GetModuleFileNameA(NULL, g_pcXMPlayDir, MAX_PATH);
	char *p = g_pcXMPlayDir + iLen - 1;
	while (p >= g_pcXMPlayDir && *p != '\\' && *p != '/') p--;
	if (p >= g_pcXMPlayDir) *p = '\0';
	strcpy(g_pcWinampIni, g_pcXMPlayDir); strcat(g_pcWinampIni,"\\winamp.ini");

	srand((unsigned int)time(NULL));
	seed = rand();

	static const XMPSHORTCUT shortcut = {0x88300, "Winamp Visualization on/off", WaVisDSP::ShortcutWaVisToggle};
	XMPlay_Misc->RegisterShortcut(&shortcut); 

	static XMPDSP xmpdsp = 
	{
		XMPDSP_FLAG_MULTI,
		WaVisXMPDSPName,
		WaVisDSP::DSP_About,
		WaVisDSP::DSP_New,
		WaVisDSP::DSP_Free,
		WaVisDSP::DSP_GetDescription,
		WaVisDSP::DSP_Config,
		WaVisDSP::DSP_GetConfig,
		WaVisDSP::DSP_SetConfig,
		WaVisDSP::DSP_NewTrack,
		WaVisDSP::DSP_SetFormat,
		WaVisDSP::DSP_Reset,
		WaVisDSP::DSP_Process
	};

	return &xmpdsp;
}

BOOL WINAPI DllMain(HANDLE hDLL, DWORD reason, LPVOID reserved)
{
	switch (reason)
	{
		case DLL_PROCESS_ATTACH:
			dllinst = (HINSTANCE)hDLL;
			DisableThreadLibraryCalls((HINSTANCE)hDLL);
			break;

		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}
