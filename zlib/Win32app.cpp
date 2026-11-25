#include "pch.h"

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

//////////////////////////////////////////////////////////////////////////////
//
// Call stack of a failed assertion, written to the debug output and log file
// (symbols from the PDBs next to the modules or at their build paths)
//
//////////////////////////////////////////////////////////////////////////////

static void LogAssertCallStack()
{
    void*  frames[48];
    USHORT count = CaptureStackBackTrace(2, ARRAYSIZE(frames), frames, NULL); // skip this and OnAssert

    HANDLE      hProcess     = GetCurrentProcess();
    static bool s_bSymbols   = false;
    if (!s_bSymbols) {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
        s_bSymbols = SymInitialize(hProcess, NULL, TRUE) != FALSE;
    }

    ZDebugOutput("call stack:\n");
    for (USHORT i = 0; i < count; i++) {
        DWORD64 address = (DWORD64)(DWORD_PTR)frames[i];
        char    sz[MAX_SYM_NAME + 300];

        char         symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
        SYMBOL_INFO* psymbol = (SYMBOL_INFO*)symbolBuffer;
        psymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        psymbol->MaxNameLen   = MAX_SYM_NAME;
        DWORD64 displacement  = 0;

        IMAGEHLP_LINE64 line = { sizeof(IMAGEHLP_LINE64) };
        DWORD           lineDisplacement = 0;

        if (s_bSymbols && SymFromAddr(hProcess, address, &displacement, psymbol)) {
            if (SymGetLineFromAddr64(hProcess, address, &lineDisplacement, &line))
                sprintf_s(sz, "  %s (%s:%lu)\n", psymbol->Name, line.FileName, line.LineNumber);
            else
                sprintf_s(sz, "  %s+0x%llx\n", psymbol->Name, displacement);
        } else {
            sprintf_s(sz, "  0x%08llx\n", address);
        }
        ZDebugOutput(sz);
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Some assertion functions
//
//////////////////////////////////////////////////////////////////////////////

Win32App *g_papp;

//////////////////////////////////////////////////////////////////////////////
//
// Some assertion functions
//
//////////////////////////////////////////////////////////////////////////////
void ZAssertImpl(bool bSucceeded, const char* psz, const char* pszFile, int line, const char* pszModule)
{
    if (!bSucceeded) {
        //
        // Just in case this was a Win32 error get the last error
        //

        DWORD dwError = GetLastError();

        if (!g_papp) {
            __asm int 3; // (debug break)
        } else if (g_papp->OnAssert(psz, pszFile, line, pszModule)) {
            g_papp->OnAssertBreak();
        }
    }
}

void ZDebugOutputImpl(const char *psz)
{
    if (g_papp)
        g_papp->DebugOutput(psz);
    else
        ::OutputDebugStringA(psz);
}
HANDLE g_logfile = NULL;

void retailf(const char* format, ...)
{
    if (g_bOutput)
    {
        // _TRUNCATE: a longer message is cut short, but always null-terminated
        const size_t size = 2048;
        char         bfr[size];

        va_list vl;
        va_start(vl, format);
        _vsnprintf_s(bfr, size, _TRUNCATE, format, vl);
        va_end(vl);

        ZDebugOutputImpl(bfr);
    }
}

extern bool g_bOutput = true;

#ifdef _DEBUG

    void ZWarningImpl(bool bSucceeded, const char* psz, const char* pszFile, int line, const char* pszModule)
    {
        if (!bSucceeded) {
            debugf("%s(%d) : ShouldBe failed: '%s'\n", pszFile, line, psz);
        }
    }

    bool ZFailedImpl(HRESULT hr, const char* pszFile, int line, const char* pszModule)
    {
        bool bFailed = FAILED(hr);
        ZAssertImpl(!bFailed, "Function Failed", pszFile, line, pszModule);
        return bFailed;
    }

    bool ZSucceededImpl(HRESULT hr, const char* pszFile, int line, const char* pszModule)
    {
        bool bSucceeded = SUCCEEDED(hr);
        ZAssertImpl(bSucceeded, "Function Failed", pszFile, line, pszModule);
        return bSucceeded;
    }


    #ifdef _TRACE
        bool    g_bEnableTrace = false;
        ZString g_strSpaces;
        int g_indent = 0;
        int g_line   = 0;

        void SetStrSpaces()
        {
            g_strSpaces = 
                  " "
                + ZString((float)g_indent, 2, 0) 
                + ZString(' ', g_indent * 2 + 1);
        }

        void ZTraceImpl(const char* pcc)
        {
            if (g_bEnableTrace) {
                ZDebugOutput(ZString((float)g_line, 4, 0) + g_strSpaces + ZString(pcc) + "\n");
            }
            g_line++;
        }

        void ZEnterImpl(const char* pcc) 
        {
            ZTraceImpl("enter " + ZString(pcc));
            g_indent += 1;
            SetStrSpaces();
        }

        void ZExitImpl(const char* pcc)
        {
            g_indent -= 1;
            SetStrSpaces();
            ZTraceImpl("exit   " + ZString(pcc));
        }

        void ZStartTraceImpl(const char* pcc)
        {
            g_indent = 0;
            g_line   = 0;
            SetStrSpaces();
            ZTraceImpl(pcc);
        }
    #endif

    void debugf(const char* format, ...)
    {
        if (g_bOutput)
        {
            // _TRUNCATE: a longer message is cut short, but always null-terminated
            const size_t size = 2048;
            char         bfr[size];

            va_list vl;
            va_start(vl, format);
            _vsnprintf_s(bfr, size, _TRUNCATE, format, vl);
            va_end(vl);

            ZDebugOutputImpl(bfr);
        }
    }

    void InitializeDebugf()
    {
        // LogToFile = 1: write debug output to a log file next to the executable
        bool  bLogToFile = ZSettings::GetDWORD("LogToFile", 0) == 1;

        if (bLogToFile)
        {
            time_t  longTime;
            time(&longTime);
            tm*             t = localtime(&longTime);

            char    logFileName[MAX_PATH + 16];
            GetModuleFileName(NULL, logFileName, MAX_PATH);
            char*   p = strrchr(logFileName, '\\');
            if (!p)
                p = logFileName;
            else
                p++;

            const char* months[] = {"jan", "feb", "mar", "apr",
                                    "may", "jun", "jul", "aug",
                                    "sep", "oct", "nov", "dec"};
            strcpy(p, months[t->tm_mon]);
            sprintf(p + 3, "%02d%02d%02d%02d.txt",
                    t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

            g_logfile = 
                CreateFile(
                    logFileName, 
                    GENERIC_WRITE, 
                    0,
                    NULL, 
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                    NULL
                );
        }
    }

    void TerminateDebugf()
    {
        if (g_logfile) {
            CloseHandle(g_logfile);
            g_logfile = NULL;
        }
    }
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Win32 Application
//
//////////////////////////////////////////////////////////////////////////////

Win32App::Win32App()
{
    g_papp = this;
}

Win32App::~Win32App() 
{
}

HRESULT Win32App::Initialize(const ZString& strCommandLine)
{
    return S_OK;
}

void Win32App::Terminate()
{
}

void Win32App::Exit(int value)
{
    _CrtSetDbgFlag(0);
    _exit(value);
}

int Win32App::OnException(DWORD code, ExceptionData* pdata)
{
	return EXCEPTION_CONTINUE_SEARCH;
}


#ifdef MemoryOutput
    TList<ZString> g_listOutput;
#endif

void Win32App::DebugOutput(const char *psz)
{
    #ifdef MemoryOutput
        g_listOutput.PushFront(ZString(psz));

        if (g_listOutput.GetCount() > 100) {
            g_listOutput.PopEnd();
        }
    #else
        ::OutputDebugStringA(psz);

        if (g_logfile) {
            DWORD nBytes;
            ::WriteFile(g_logfile, psz, strlen(psz), &nBytes, NULL);
        }
    #endif
}

bool Win32App::OnAssert(const char* psz, const char* pszFile, int line, const char* pszModule)
{
    ZDebugOutput(
          ZString("assertion failed: '")
        + psz
        + "' ("
        + pszFile
        + ":"
        + ZString(line)
        + ")\n"
    );
    LogAssertCallStack();

    //return _CrtDbgReport(_CRT_ASSERT, pszFile, line, pszModule, psz) == 1;
    return true;
}

void Win32App::OnAssertBreak()
{
    #ifdef MemoryOutput
        ZString str;

        TList<ZString>::Iterator iter(g_listOutput);

        while (!iter.End()) {
            str = iter.Value() + str;
            iter.Next();
        }
    #endif
    
    //
    // Cause an exception
    //

    (*(int*)0) = 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// Win Main
//
//////////////////////////////////////////////////////////////////////////////

//
// The body of Win32Main lives in its own function: it constructs C++ temporaries
// (ZString from the command line), and MSVC does not allow __try in a function
// that needs object unwinding (C2712).
//

static void Win32MainBody(LPSTR lpszCmdLine)
{
    HRESULT hr;

    do {
        #ifdef _DEBUG
            InitializeDebugf();
        #endif

        BreakOnError(hr = Window::StaticInitialize());
        BreakOnError(hr = g_papp->Initialize(lpszCmdLine));

        //
        // Win32App::Initialize() return S_FALSE if this is a command line app and
        // we shouldn't run the message loop
        //

        if (SUCCEEDED(hr) && S_FALSE != hr) {
            Window::MessageLoop();
        }

        g_papp->Terminate();
        Window::StaticTerminate();

        #ifdef _DEBUG
            TerminateDebugf();
        #endif

    } while (false);
}

__declspec(dllexport) int WINAPI Win32Main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    // seed the random number generator with the current time
    // (GetTickCount may be semi-predictable on server startup, so we add the 
    // clock time to shake things up a bit)
    srand(GetTickCount() + time(NULL));

    // shift the stack locals and the heap by a random amount.            
    char* pzSpacer = new char[4 * (int)random(21, 256)];
    pzSpacer[0] = *(char*)_alloca(4 * (int)random(1, 256));

    __try {
        Win32MainBody(lpszCmdLine);
    } __except (g_papp->OnException(_exception_code(), (ExceptionData*)_exception_info())){
    }

    delete[] pzSpacer;

    return 0;
}
