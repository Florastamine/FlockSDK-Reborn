//
// Copyright (c) 2008 - 2018 the Urho3D project, 2017 - 2018 Flock SDK developers & contributors. 
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include "../Core/ProcessUtils.h"
#include "../Core/StringUtils.h"
#include "../IO/FileSystem.h"

#include <cstdio>
#include <fcntl.h>

#ifdef __APPLE__
    #include "TargetConditionals.h"
#endif

#if defined(IOS)
    #include <mach/mach_host.h>
#endif

#if defined(_WIN32)
    #include <windows.h>
    #include <io.h>
    #include <shlobj.h>
    #include <tlhelp32.h>
    #if defined(__MINGW32__)
        #include <lmcons.h>
        #include <ntdef.h> 
    #endif
    #include <bitset>
    #include <vector>
#elif defined(__linux__) && !defined(__ANDROID__) 
    #include <pwd.h> 
    #include <sys/sysinfo.h>
    #include <sys/utsname.h>
    #include <fstream> 
    #include <string> 
    #include <clocale> 
#elif defined(__APPLE__)
    #include <sys/sysctl.h>
    #include <SystemConfiguration/SystemConfiguration.h> // For the detection functions inside GetLoginName(). 
#endif

#ifndef _WIN32
    #include <unistd.h>
#endif

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
#include <emscripten/threading.h>
#endif

#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))
    #include <thread> 
#endif

#if defined(__i386__)
// From http://stereopsis.com/FPU.html

#define FPU_CW_PREC_MASK        0x0300
#define FPU_CW_PREC_SINGLE      0x0000
#define FPU_CW_PREC_DOUBLE      0x0200
#define FPU_CW_PREC_EXTENDED    0x0300
#define FPU_CW_ROUND_MASK       0x0c00
#define FPU_CW_ROUND_NEAR       0x0000
#define FPU_CW_ROUND_DOWN       0x0400
#define FPU_CW_ROUND_UP         0x0800
#define FPU_CW_ROUND_CHOP       0x0c00

inline unsigned GetFPUState()
{
    unsigned control = 0;
    __asm__ __volatile__ ("fnstcw %0" : "=m" (control));
    return control;
}

inline void SetFPUState(unsigned control)
{
    __asm__ __volatile__ ("fldcw %0" : : "m" (control));
}
#endif

#if defined(_WIN32)
    #include <intrin.h> 

    // Some old versions of MinGW (including the legacy MinGW and old versions of MinGW-W64, like the
    // version comes included with GCC 4.9.3) does not have _xgetbv() in psdk_inc/intrin-impl.h. So, instead
    // of dealing with workarounds for different versions of MinGW, we'll just roll our own version of _xgetbv(),
    // it's going to work on every MinGW-based compilers.
    #if defined(__MINGW32__)
        static inline unsigned long long xgetbv(unsigned int index)
        {
            unsigned int eax, edx;
            __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
            return ((unsigned long long) edx << 32) | eax;
        }
    #endif

    static void               __GetCPUID__(int (&out)[4], int x) { __cpuid(out, x); }
    static unsigned long long __GetECRInfo__(unsigned x)         { return xgetbv(x); } // https://software.intel.com/en-us/node/523373 
#elif defined(__linux__) && !defined(__ANDROID__) 
    #include <cpuid.h> 

    static void __GetCPUID__(int (&out)[4], int x)
    {
        __cpuid_count(x, 0, out[0], out[1], out[2], out[3]); 
    }

    static unsigned long long __GetECRInfo__(unsigned x)
    {
        unsigned eax, edx; 
        __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(x));
        return ((unsigned long long) edx << 32) | eax;
    }
#endif

#if defined(_WIN32) && defined(_MSC_VER)
    #define __PLATFORM__XCR_XFEATURE_ENABLED_MASK _XCR_XFEATURE_ENABLED_MASK 
#else 
    #define __PLATFORM__XCR_XFEATURE_ENABLED_MASK 0 
#endif

#if defined(_WIN32)
struct SystemTimeData {
    FILETIME sysIdle;
    FILETIME sysKernel;
    FILETIME sysUser;

    operator=(const SystemTimeData &s)
    {
        sysIdle = s.sysIdle;
        sysKernel = s.sysKernel;
        sysUser = s.sysUser;
    }

    ULONGLONG GetIdleDiff(const SystemTimeData &s) const { return GetDiff(sysIdle, s.sysIdle); }
    ULONGLONG GetKernelDiff(const SystemTimeData &s) const { return GetDiff(sysKernel, s.sysKernel); }
    ULONGLONG GetUserDiff(const SystemTimeData &s) const { return GetDiff(sysUser, s.sysUser); }
    
    BOOL QueryValue() { return ::GetSystemTimes(&sysIdle, &sysKernel, &sysUser);  }

private:
    ULONGLONG GetDiff(const FILETIME &one, const FILETIME &two) const
    {
        LARGE_INTEGER a, b;
        a.LowPart = one.dwLowDateTime;
        a.HighPart = one.dwHighDateTime;

        b.LowPart = two.dwLowDateTime;
        b.HighPart = two.dwHighDateTime;

        return a.QuadPart - b.QuadPart;
    }
};

static SystemTimeData then;
#endif

#if defined(_WIN32)
struct LocaleInfo {
    const char *name;
    unsigned    lang;
    unsigned    sublang; 

    LocaleInfo(const char *name = "", unsigned lang = 0, unsigned sublang = 0) : name(name), lang(lang), sublang(sublang) {}
};

static Urho3D::Vector<LocaleInfo> windows_locale;
#endif

#ifndef MINI_URHO
#include <SDL/SDL.h>
#endif

#include "../DebugNew.h"

namespace Urho3D
{

struct CpuCoreCount
{
    unsigned numPhysicalCores_;
    unsigned numLogicalCores_;
};

#ifdef _WIN32
static bool consoleOpened = false;
#endif
static String currentLine;
static Vector<String> arguments;
static String miniDumpDir;

#if defined(IOS)
static void GetCPUData(host_basic_info_data_t* data)
{
    mach_msg_type_number_t infoCount;
    infoCount = HOST_BASIC_INFO_COUNT;
    host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)data, &infoCount);
}
#elif defined(__linux__)
// This function is used by all the target triplets with Linux as the OS, such as Android, RPI, desktop Linux, etc
static void GetCPUData(struct CpuCoreCount* data)
{
    // Sanity check
    assert(data);
    // At least return 1 core
    data->numPhysicalCores_ = data->numLogicalCores_ = 1;

    FILE* fp;
    int res;
    unsigned i, j;

    fp = fopen("/sys/devices/system/cpu/present", "r");
    if (fp)
    {
        res = fscanf(fp, "%d-%d", &i, &j);                          // NOLINT(cert-err34-c)
        fclose(fp);

        if (res == 2 && i == 0)
        {
            data->numPhysicalCores_ = data->numLogicalCores_ = j + 1;

            fp = fopen("/sys/devices/system/cpu/cpu0/topology/thread_siblings_list", "r");
            if (fp)
            {
                res = fscanf(fp, "%d,%d,%d,%d", &i, &j, &i, &j);    // NOLINT(cert-err34-c)
                fclose(fp);

                // Having sibling thread(s) indicates the CPU is using HT/SMT technology
                if (res > 1)
                    data->numPhysicalCores_ /= res;
            }
        }
    }
}

#elif defined(_WIN32) && !defined(__EMSCRIPTEN__)

static CpuCoreCount __GetCPUInfo__()
{
    CpuCoreCount r{};
    
    auto buffer = ([] () -> std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> {
        DWORD b = 0;
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer;
        
        GetLogicalProcessorInformation(nullptr, &b);

        buffer.resize(b / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        GetLogicalProcessorInformation(buffer.data(), &b);

        return buffer;
    })();

    for(auto &&ca : buffer)
    {
        switch(ca.Relationship)
        {
            case RelationProcessorCore: 
            {
                ++(r.numPhysicalCores_); 
                r.numLogicalCores_ += static_cast<unsigned>(std::bitset<sizeof(ULONG_PTR) * 8>(ca.ProcessorMask).count());
                break; 
            }
            
            default:;
        }
    }

    return r; 
}

static void GetCPUData(struct CpuCoreCount* data)
{
    *data = __GetCPUInfo__();
}
#endif

void InitFPU()
{
    // Make sure FPU is in round-to-nearest, single precision mode
    // This ensures Direct3D and OpenGL behave similarly, and all threads behave similarly
#if defined(_MSC_VER) && defined(_M_IX86)
    _controlfp(_RC_NEAR | _PC_24, _MCW_RC | _MCW_PC);
#elif defined(__i386__)
    unsigned control = GetFPUState();
    control &= ~(FPU_CW_PREC_MASK | FPU_CW_ROUND_MASK);
    control |= (FPU_CW_PREC_SINGLE | FPU_CW_ROUND_NEAR);
    SetFPUState(control);
#endif
}

void ErrorDialog(const String& title, const String& message)
{
#ifndef MINI_URHO
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.CString(), message.CString(), nullptr);
#endif
}

void ErrorExit(const String& message, int exitCode)
{
    if (!message.Empty())
        PrintLine(message, true);

    exit(exitCode);
}

void OpenConsoleWindow()
{
#ifdef _WIN32
    if (consoleOpened)
        return;

    AllocConsole();

    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);

    consoleOpened = true;
#endif
}

void PrintUnicode(const String& str, bool error)
{
#if !defined(__ANDROID__) && !defined(IOS)
#ifdef _WIN32
    // If the output stream has been redirected, use fprintf instead of WriteConsoleW,
    // though it means that proper Unicode output will not work
    FILE* out = error ? stderr : stdout;
    if (!_isatty(_fileno(out)))
        fprintf(out, "%s", str.CString());
    else
    {
        HANDLE stream = GetStdHandle(error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
        if (stream == INVALID_HANDLE_VALUE)
            return;
        WString strW(str);
        DWORD charsWritten;
        WriteConsoleW(stream, strW.CString(), strW.Length(), &charsWritten, nullptr);
    }
#else
    fprintf(error ? stderr : stdout, "%s", str.CString());
#endif
#endif
}

void PrintUnicodeLine(const String& str, bool error)
{
    PrintUnicode(str + "\n", error);
}

void PrintLine(const String& str, bool error)
{
    PrintLine(str.CString(), error);
}

void PrintLine(const char* str, bool error)
{
#if !defined(__ANDROID__) && !defined(IOS)
    fprintf(error ? stderr : stdout, "%s\n", str);
#endif
}

const Vector<String>& ParseArguments(const String& cmdLine, bool skipFirstArgument)
{
    arguments.Clear();

    unsigned cmdStart = 0, cmdEnd = 0;
    bool inCmd = false;
    bool inQuote = false;

    for (unsigned i = 0; i < cmdLine.Length(); ++i)
    {
        if (cmdLine[i] == '\"')
            inQuote = !inQuote;
        if (cmdLine[i] == ' ' && !inQuote)
        {
            if (inCmd)
            {
                inCmd = false;
                cmdEnd = i;
                // Do not store the first argument (executable name)
                if (!skipFirstArgument)
                    arguments.Push(cmdLine.Substring(cmdStart, cmdEnd - cmdStart));
                skipFirstArgument = false;
            }
        }
        else
        {
            if (!inCmd)
            {
                inCmd = true;
                cmdStart = i;
            }
        }
    }
    if (inCmd)
    {
        cmdEnd = cmdLine.Length();
        if (!skipFirstArgument)
            arguments.Push(cmdLine.Substring(cmdStart, cmdEnd - cmdStart));
    }

    // Strip double quotes from the arguments
    for (unsigned i = 0; i < arguments.Size(); ++i)
        arguments[i].Replace("\"", "");

    return arguments;
}

const Vector<String>& ParseArguments(const char* cmdLine)
{
    return ParseArguments(String(cmdLine));
}

const Vector<String>& ParseArguments(const WString& cmdLine)
{
    return ParseArguments(String(cmdLine));
}

const Vector<String>& ParseArguments(const wchar_t* cmdLine)
{
    return ParseArguments(String(cmdLine));
}

const Vector<String>& ParseArguments(int argc, char** argv)
{
    String cmdLine;

    for (int i = 0; i < argc; ++i)
        cmdLine.AppendWithFormat("\"%s\" ", (const char*)argv[i]);

    return ParseArguments(cmdLine);
}

const Vector<String>& GetArguments()
{
    return arguments;
}

String GetConsoleInput()
{
    String ret;
#ifdef URHO3D_TESTING
    // When we are running automated tests, reading the console may block. Just return empty in that case
    return ret;
#else
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (input == INVALID_HANDLE_VALUE || output == INVALID_HANDLE_VALUE)
        return ret;

    // Use char-based input
    SetConsoleMode(input, ENABLE_PROCESSED_INPUT);

    INPUT_RECORD record;
    DWORD events = 0;
    DWORD readEvents = 0;

    if (!GetNumberOfConsoleInputEvents(input, &events))
        return ret;

    while (events--)
    {
        ReadConsoleInputW(input, &record, 1, &readEvents);
        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
        {
            unsigned c = record.Event.KeyEvent.uChar.UnicodeChar;
            if (c)
            {
                if (c == '\b')
                {
                    PrintUnicode("\b \b");
                    int length = currentLine.LengthUTF8();
                    if (length)
                        currentLine = currentLine.SubstringUTF8(0, length - 1);
                }
                else if (c == '\r')
                {
                    PrintUnicode("\n");
                    ret = currentLine;
                    currentLine.Clear();
                    return ret;
                }
                else
                {
                    // We have disabled echo, so echo manually
                    wchar_t out = c;
                    DWORD charsWritten;
                    WriteConsoleW(output, &out, 1, &charsWritten, nullptr);
                    currentLine.AppendUTF8(c);
                }
            }
        }
    }
#elif !defined(__ANDROID__) && !defined(IOS)
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    for (;;)
    {
        int ch = fgetc(stdin);
        if (ch >= 0 && ch != '\n')
            ret += (char)ch;
        else
            break;
    }
#endif

    return ret;
#endif
}

String GetPlatform()
{
#if defined(__ANDROID__)
    return "Android";
#elif defined(IOS)
    return "iOS";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__EMSCRIPTEN__)
    return "Web";
#elif defined(__linux__)
    return "Linux";
#else
    return "(?)";
#endif
}

unsigned GetNumPhysicalCPUs()
{
#if defined(IOS)
    host_basic_info_data_t data;
    GetCPUData(&data);
#if defined(TARGET_OS_SIMULATOR)
    // Hardcoded to dual-core on simulator mode even if the host has more
    return Min(2, data.physical_cpu);
#else
    return data.physical_cpu;
#endif
#elif defined(__linux__)
    struct CpuCoreCount data{};
    GetCPUData(&data);
    return data.numPhysicalCores_;
#elif defined(__EMSCRIPTEN__)
#ifdef __EMSCRIPTEN_PTHREADS__
    return emscripten_num_logical_cores();
#else
    return 1; // Targeting a single-threaded Emscripten build.
#endif
#else
    struct CpuCoreCount data;
    GetCPUData(&data);
    return data.numPhysicalCores_;
#endif
}

unsigned GetNumLogicalCPUs()
{
#if defined(IOS)
    host_basic_info_data_t data;
    GetCPUData(&data);
#if defined(TARGET_OS_SIMULATOR)
    return Min(2, data.logical_cpu);
#else
    return data.logical_cpu;
#endif
#elif defined(__EMSCRIPTEN__)
#ifdef __EMSCRIPTEN_PTHREADS__
    return emscripten_num_logical_cores();
#else
    return 1; // Targeting a single-threaded Emscripten build.
#endif
#elif defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))
    return std::thread::hardware_concurrency();
#endif
}

void SetMiniDumpDir(const String& pathName)
{
    miniDumpDir = AddTrailingSlash(pathName);
}

String GetMiniDumpDir()
{
#ifndef MINI_URHO
    if (miniDumpDir.Empty())
    {
        char* pathName = SDL_GetPrefPath("urho3d", "crashdumps");
        if (pathName)
        {
            String ret(pathName);
            SDL_free(pathName);
            return ret;
        }
    }
#endif

    return miniDumpDir;
}

unsigned long long GetTotalMemory()
{
#if defined(__linux__) && !defined(__ANDROID__)
    struct sysinfo s{};
    if (sysinfo(&s) != -1)
        return s.totalram;
#elif defined(_WIN32)
    MEMORYSTATUSEX state;
    state.dwLength = sizeof(state);
    if (GlobalMemoryStatusEx(&state))
        return state.ullTotalPhys;
#elif defined(__APPLE__)
    unsigned long long memSize;
    size_t len = sizeof(memSize);
    int mib[2];
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    sysctl(mib, 2, &memSize, &len, NULL, 0);
    return memSize;
#endif
    return 0ull;
}

String GetLoginName()
{
#if defined(__linux__) && !defined(__ANDROID__)
    struct passwd *p = getpwuid(getuid());
    if (p != nullptr)
        return p->pw_name;
#elif defined(_WIN32)
    char name[UNLEN + 1];
    DWORD len = UNLEN + 1;
    if (GetUserName(name, &len))
        return name;
#elif defined(__APPLE__) && !defined(IOS)
    SCDynamicStoreRef s = SCDynamicStoreCreate(NULL, CFSTR("GetConsoleUser"), NULL, NULL);
    if (s != NULL)
    {
        uid_t u;
        CFStringRef n = SCDynamicStoreCopyConsoleUser(s, &u, NULL);
        CFRelease(s);
        if (n != NULL)
        {
            char name[256];
            Boolean b = CFStringGetCString(n, name, 256, kCFStringEncodingUTF8);
            CFRelease(n);

            if (b == true)
                return name;
        }
    }
#endif
    return "(?)";
}

String GetHostName()
{
#if (defined(__linux__) || defined(__APPLE__)) && !defined(__ANDROID__)
    char buffer[256];
    if (gethostname(buffer, 256) == 0)
        return buffer;
#elif defined(_WIN32)
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD len = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerName(buffer, &len))
        return buffer;
#endif
    return "(?)";
}

// Disable Windows OS version functionality when compiling mini version for Web, see https://github.com/urho3d/Urho3D/issues/1998
#if defined(_WIN32) && defined(HAVE_RTL_OSVERSIONINFOW) && !defined(MINI_URHO)
using RtlGetVersionPtr = NTSTATUS (WINAPI *)(PRTL_OSVERSIONINFOW);

static void GetOS(RTL_OSVERSIONINFOW *r)
{
    HMODULE m = GetModuleHandle("ntdll.dll");
    if (m)
    {
        RtlGetVersionPtr fPtr = (RtlGetVersionPtr) GetProcAddress(m, "RtlGetVersion");
        if (r && fPtr && fPtr(r) == 0)
            r->dwOSVersionInfoSize = sizeof *r;
    }
}
#endif

String GetOSVersion()
{
#if defined(__linux__) && !defined(__ANDROID__)
    struct utsname u{};
    if (uname(&u) == 0)
        return String(u.sysname) + " " + u.release;
#elif defined(_WIN32) && defined(HAVE_RTL_OSVERSIONINFOW) && !defined(MINI_URHO)
    RTL_OSVERSIONINFOW r;
    GetOS(&r);
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724832(v=vs.85).aspx
    if (r.dwMajorVersion == 5 && r.dwMinorVersion == 0)
        return "Windows 2000";
    else if (r.dwMajorVersion == 5 && r.dwMinorVersion == 1)
        return "Windows XP";
    else if (r.dwMajorVersion == 5 && r.dwMinorVersion == 2)
        return "Windows XP 64-Bit Edition/Windows Server 2003/Windows Server 2003 R2";
    else if (r.dwMajorVersion == 6 && r.dwMinorVersion == 0)
        return "Windows Vista/Windows Server 2008";
    else if (r.dwMajorVersion == 6 && r.dwMinorVersion == 1)
        return "Windows 7/Windows Server 2008 R2";
    else if (r.dwMajorVersion == 6 && r.dwMinorVersion == 2)
        return "Windows 8/Windows Server 2012";
    else if (r.dwMajorVersion == 6 && r.dwMinorVersion == 3)
        return "Windows 8.1/Windows Server 2012 R2";
    else if (r.dwMajorVersion == 10 && r.dwMinorVersion == 0)
        return "Windows 10/Windows Server 2016";
    else
        return "Windows Unknown";
#elif defined(__APPLE__)
    char kernel_r[256];
    size_t size = sizeof(kernel_r);

    if (sysctlbyname("kern.osrelease", &kernel_r, &size, NULL, 0) != -1)
    {
        Vector<String> kernel_version = String(kernel_r).Split('.');
        String version = "macOS/Mac OS X ";
        int major = ToInt(kernel_version[0]);
        int minor = ToInt(kernel_version[1]);

        // https://en.wikipedia.org/wiki/Darwin_(operating_system)
        if (major == 16) // macOS Sierra
        {
            version += "Sierra ";
            switch(minor)
            {
                case 0: version += "10.12.0 "; break;
                case 1: version += "10.12.1 "; break;
                case 3: version += "10.12.2 "; break;
            }
        }
        else if (major == 15) // OS X El Capitan
        {
            version += "El Capitan ";
            switch(minor)
            {
                case 0: version += "10.11.0 "; break;
                case 6: version += "10.11.6 "; break;
            }
        }
        else if (major == 14) // OS X Yosemite
        {
            version += "Yosemite ";
            switch(minor)
            {
                case 0: version += "10.10.0 "; break;
                case 5: version += "10.10.5 "; break;
            }
        }
        else if (major == 13) // OS X Mavericks
        {
            version += "Mavericks ";
            switch(minor)
            {
                case 0: version += "10.9.0 "; break;
                case 4: version += "10.9.5 "; break;
            }
        }
        else if (major == 12) // OS X Mountain Lion
        {
            version += "Mountain Lion ";
            switch(minor)
            {
                case 0: version += "10.8.0 "; break;
                case 6: version += "10.8.5 "; break;
            }
        }
        else if (major == 11) // Mac OS X Lion
        {
            version += "Lion ";
            switch(minor)
            {
                case 0: version += "10.7.0 "; break;
                case 4: version += "10.7.5 "; break;
            }
        }
        else
        {
            version += "Unknown ";
        }

        return version + " (Darwin kernel " + kernel_version[0] + "." + kernel_version[1] + "." + kernel_version[2] + ")";
    }
#endif
    return "(?)";
}

int GetBatteryPercentage()
{
    int pct;
    SDL_GetPowerInfo(nullptr, &pct);
    return pct;
}

int GetBatteryTimeLeft()
{
    int secs;
    SDL_GetPowerInfo(&secs, nullptr);
    return secs;
}

bool GetCPUBigEndian()
{
    int n = 1;
    return *(char *)&n == 1;
}

bool GetCPULittleEndian()
{
    return !GetCPUBigEndian(); 
}

unsigned long long GetCPUClock()
{
#if defined(__linux__) && !defined(__ANDROID__)
    std::ifstream cpuinfo("/proc/cpuinfo"); 
    if(cpuinfo.is_open())
    {
        for(std::string line; std::getline(cpuinfo, line);)
        {
            if(0 == line.find("cpu MHz"))
                return static_cast<unsigned long long>(std::strtod(line.c_str() + line.find_first_of(':') + 1, nullptr)); 
        }
    }
#elif defined(_WIN32)
    LARGE_INTEGER f; 
    QueryPerformanceFrequency(&f); 
    return static_cast<unsigned long long>(f.QuadPart / 1000.0); 
#endif
    return 0ull;
}

String GetCPUExtensions()
{
    String s(String::EMPTY); 

	int c[4];
	__GetCPUID__(c, 0);
    int id = c[0]; 

    __GetCPUID__(c, 0x80000000); 
    unsigned ext_id = c[0]; 

    #define ADD_IF(iindex, bindex, str) \
        if ((c[(iindex)] & ((int)1 << (bindex)) ) != 0) { s = s + str + ";"; }
    
	if (id >= 0x00000001)
    {
		__GetCPUID__(c, 0x00000001);
		ADD_IF(3, 23, "MMX");
        ADD_IF(3, 22, "Extended MMX");

		ADD_IF(3, 25, "SSE");
		ADD_IF(3, 26, "SSE2");
		ADD_IF(2, 0, "SSE3");
		ADD_IF(2, 9, "SSSE3");
		ADD_IF(2, 19, "SSE4.1");
		ADD_IF(2, 20, "SSE4.2");

		ADD_IF(2, 25, "AES");
		ADD_IF(2, 28, "AVX");
		ADD_IF(2, 12, "FMA3");
        ADD_IF(2, 30, "RDRAND");
	}
	if (id >= 0x00000007)
    {
		__GetCPUID__(c, 0x00000007);
        
        auto GetAVXSupport = [] () -> bool {
            bool has_avx = false;

            int c[4];
            __GetCPUID__(c, 1);

            bool osusesxsave_restore = (c[2] & (1 << 27)) != 0;
            bool cpusupportsavx = (c[2] & (1 << 28)) != 0;

            if (osusesxsave_restore && cpusupportsavx) 
            {
                unsigned long long xcrFeatureMask = __GetECRInfo__(__PLATFORM__XCR_XFEATURE_ENABLED_MASK);
                has_avx = (xcrFeatureMask & 0x6) == 0x6;
            }

            return has_avx;
        };

        if(GetAVXSupport())
            ADD_IF(1, 5, "AVX2");

		ADD_IF(1, 4, "HLE");
		ADD_IF(1, 3, "BMI1");
		ADD_IF(1, 8, "BMI2");
		ADD_IF(1, 19, "ADX");
		ADD_IF(1, 14, "MPX");
		ADD_IF(1, 29, "SHA");
        ADD_IF(2, 0, "PREFETCHWT1");

		if (GetAVXSupport() && ((__GetECRInfo__(__PLATFORM__XCR_XFEATURE_ENABLED_MASK) & 0xe6) == 0xe6)) // AVX-512 is present
        {
            s = s + "AVX-512;"; 
			ADD_IF(1, 16, "AVX-512-F");
			ADD_IF(1, 28, "AVX-512-CD");
			ADD_IF(1, 26, "AVX-512-PF");
			ADD_IF(1, 27, "AVX-512-ER");
			ADD_IF(1, 31, "AVX-512-VL");
			ADD_IF(1, 30, "AVX-512-BW");
			ADD_IF(1, 17, "AVX-512-DQ");
			ADD_IF(1, 21, "AVX-512-IFMA");
			ADD_IF(2, 1, "AVX-512-VBMI");
		}
	}
	if (ext_id >= 0x80000001) 
    {
		__GetCPUID__(c, 0x80000001);

		ADD_IF(3, 29, "EM64T");
        ADD_IF(2, 5, "ABM");
        ADD_IF(2, 6, "SSE4a");
        ADD_IF(2, 16, "FMA4");
        ADD_IF(2, 11, "XOP");
		ADD_IF(3, 0, "x87");
		ADD_IF(3, 30, "3DNow!");
		ADD_IF(3, 31, "Extended 3DNow!");
	}
    #undef ADD_IF 

    s.Erase(s.Length() - 1);
    return s;
}

double GetCPUUsage()
{
#if defined(_WIN32)
    SystemTimeData now;
    if (!now.QueryValue())
        return -1.0;

    double usage = 0.0;

    if (then.sysIdle.dwLowDateTime != 0 && then.sysIdle.dwHighDateTime != 0)
    {
        ULONGLONG sysIdleDiff = now.GetIdleDiff(then),
                  sysKernelDiff = now.GetKernelDiff(then),
                  sysUserDiff = now.GetUserDiff(then);

        ULONGLONG sysTotal = sysKernelDiff + sysUserDiff;
        ULONGLONG kernelTotal = sysKernelDiff - sysIdleDiff; // kernelTime - IdleTime = kernelTime, because sysKernel include IdleTime

        usage = sysTotal > 0 ? (double) (((kernelTotal + sysUserDiff) * 100.0) / sysTotal) : usage;
    }

    then = now;
    return usage;
#endif
}

String GetCPUArchitecture()
{
#if defined(__linux__) && !defined(__ANDROID__) 
    struct utsname u;
    if (uname(&u) == 0)
    {
        String s(u.machine);
        if(s.Contains("x86_64", false))
            return "x86_64"; 
        else if(s.Contains("IA64", false))
            return "IA64"; 
        else if(s.Contains("i686", false))
            return "x86"; 
        else if(s.Contains("ARM", false))
            return "ARM"; 
    }
#elif defined(_WIN32)
    SYSTEM_INFO s;
    GetNativeSystemInfo(&s); // Because GetSystemInfo() would report incorrect results (x86 instead of x86_64) when running the application under WOW64 
                             // environment (e.g. running 32-bit software under 64-bit OS). 

    switch(s.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64 : return "x86_64";
        case PROCESSOR_ARCHITECTURE_IA64  : return "IA64"; 
        case PROCESSOR_ARCHITECTURE_INTEL : return "x86";
        case PROCESSOR_ARCHITECTURE_ARM   : return "ARM"; 
        default: ;
    }
#endif
    return "(Unknown architecture)"; 
}

String GetCPUVendorID()
{
    int c[4];
    __GetCPUID__(c, 0); 

    String s((const char *) &(c[1]), 4);
    s += String((const char *) &(c[3]), 4);
    s += String((const char *) &(c[2]), 4); 

    return s;
}

String GetCPUBrandName()
{
    int c[4];
    String s;
    
    #define __CPU_NAME_COPY_REGISTERS_DATA__(slot, id, string) \
        __GetCPUID__(slot, id); \
        for (auto i = 0u; i < 4; ++i) \
            string += String((const char *) &(slot[i]), 4)

    __CPU_NAME_COPY_REGISTERS_DATA__(c, 0x80000002, s);
    __CPU_NAME_COPY_REGISTERS_DATA__(c, 0x80000003, s);
    __CPU_NAME_COPY_REGISTERS_DATA__(c, 0x80000004, s);
    
    #undef __CPU_NAME_COPY_REGISTERS_DATA__

    return s;
}

String GetLocale()
{
#if defined(__linux__) && !defined(__ANDROID__) 
    std::setlocale(LC_ALL, "");
    return std::setlocale(LC_ALL, NULL); 
#elif defined(_WIN32)
    if (windows_locale.Empty())
    {
        windows_locale.Push(LocaleInfo("ar", LANG_ARABIC, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("ar_AE", LANG_ARABIC, SUBLANG_ARABIC_UAE));
        windows_locale.Push(LocaleInfo("ar_BH", LANG_ARABIC, SUBLANG_ARABIC_BAHRAIN));
        windows_locale.Push(LocaleInfo("ar_DZ", LANG_ARABIC, SUBLANG_ARABIC_ALGERIA));
        windows_locale.Push(LocaleInfo("ar_EG", LANG_ARABIC, SUBLANG_ARABIC_EGYPT));
        windows_locale.Push(LocaleInfo("ar_IQ", LANG_ARABIC, SUBLANG_ARABIC_IRAQ));
        windows_locale.Push(LocaleInfo("ar_JO", LANG_ARABIC, SUBLANG_ARABIC_JORDAN));
        windows_locale.Push(LocaleInfo("ar_KW", LANG_ARABIC, SUBLANG_ARABIC_KUWAIT));
        windows_locale.Push(LocaleInfo("ar_LB", LANG_ARABIC, SUBLANG_ARABIC_LEBANON));
        windows_locale.Push(LocaleInfo("ar_LY", LANG_ARABIC, SUBLANG_ARABIC_LIBYA));
        windows_locale.Push(LocaleInfo("ar_MA", LANG_ARABIC, SUBLANG_ARABIC_MOROCCO));
        windows_locale.Push(LocaleInfo("ar_OM", LANG_ARABIC, SUBLANG_ARABIC_OMAN));
        windows_locale.Push(LocaleInfo("ar_QA", LANG_ARABIC, SUBLANG_ARABIC_QATAR));
        windows_locale.Push(LocaleInfo("ar_SA", LANG_ARABIC, SUBLANG_ARABIC_SAUDI_ARABIA));
        windows_locale.Push(LocaleInfo("ar_SY", LANG_ARABIC, SUBLANG_ARABIC_SYRIA));
        windows_locale.Push(LocaleInfo("ar_TN", LANG_ARABIC, SUBLANG_ARABIC_TUNISIA));
        windows_locale.Push(LocaleInfo("ar_YE", LANG_ARABIC, SUBLANG_ARABIC_YEMEN));
        windows_locale.Push(LocaleInfo("be", LANG_BELARUSIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("be_BY", LANG_BELARUSIAN, SUBLANG_BELARUSIAN_BELARUS));
        windows_locale.Push(LocaleInfo("bg", LANG_BULGARIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("bg_BG", LANG_BULGARIAN, SUBLANG_BULGARIAN_BULGARIA));
        windows_locale.Push(LocaleInfo("ca", LANG_CATALAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("ca_ES", LANG_CATALAN, SUBLANG_CATALAN_CATALAN));
        windows_locale.Push(LocaleInfo("cs", LANG_CZECH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("cs_CZ", LANG_CZECH, SUBLANG_CZECH_CZECH_REPUBLIC));
        windows_locale.Push(LocaleInfo("da", LANG_DANISH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("da_DK", LANG_DANISH, SUBLANG_DANISH_DENMARK));
        windows_locale.Push(LocaleInfo("de", LANG_GERMAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("de_AT", LANG_GERMAN, SUBLANG_GERMAN_AUSTRIAN));
        windows_locale.Push(LocaleInfo("de_CH", LANG_GERMAN, SUBLANG_GERMAN_SWISS));
        windows_locale.Push(LocaleInfo("de_DE", LANG_GERMAN, SUBLANG_GERMAN));
        windows_locale.Push(LocaleInfo("de_LU", LANG_GERMAN, SUBLANG_GERMAN_LUXEMBOURG));
        windows_locale.Push(LocaleInfo("el", LANG_GREEK, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("el_GR", LANG_GREEK, SUBLANG_GREEK_GREECE));
        windows_locale.Push(LocaleInfo("en", LANG_ENGLISH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("en_AU", LANG_ENGLISH, SUBLANG_ENGLISH_AUS));
        windows_locale.Push(LocaleInfo("en_CA", LANG_ENGLISH, SUBLANG_ENGLISH_CAN));
        windows_locale.Push(LocaleInfo("en_GB", LANG_ENGLISH, SUBLANG_ENGLISH_UK));
        windows_locale.Push(LocaleInfo("en_IN", LANG_ENGLISH, SUBLANG_ENGLISH_INDIA));
        windows_locale.Push(LocaleInfo("en_NZ", LANG_ENGLISH, SUBLANG_ENGLISH_NZ));
        windows_locale.Push(LocaleInfo("en_PH", LANG_ENGLISH, SUBLANG_ENGLISH_PHILIPPINES));
        windows_locale.Push(LocaleInfo("en_SG", LANG_ENGLISH, SUBLANG_ENGLISH_SINGAPORE));
        windows_locale.Push(LocaleInfo("en_US", LANG_ENGLISH, SUBLANG_ENGLISH_US));
        windows_locale.Push(LocaleInfo("en_ZA", LANG_ENGLISH, SUBLANG_ENGLISH_SOUTH_AFRICA));
        windows_locale.Push(LocaleInfo("es", LANG_SPANISH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("es_AR", LANG_SPANISH, SUBLANG_SPANISH_ARGENTINA));
        windows_locale.Push(LocaleInfo("es_BO", LANG_SPANISH, SUBLANG_SPANISH_BOLIVIA));
        windows_locale.Push(LocaleInfo("es_CL", LANG_SPANISH, SUBLANG_SPANISH_CHILE));
        windows_locale.Push(LocaleInfo("es_CO", LANG_SPANISH, SUBLANG_SPANISH_COLOMBIA));
        windows_locale.Push(LocaleInfo("es_CR", LANG_SPANISH, SUBLANG_SPANISH_COSTA_RICA));
        windows_locale.Push(LocaleInfo("es_DO", LANG_SPANISH, SUBLANG_SPANISH_DOMINICAN_REPUBLIC));
        windows_locale.Push(LocaleInfo("es_EC", LANG_SPANISH, SUBLANG_SPANISH_ECUADOR));
        windows_locale.Push(LocaleInfo("es_ES", LANG_SPANISH, SUBLANG_SPANISH));
        windows_locale.Push(LocaleInfo("es_GT", LANG_SPANISH, SUBLANG_SPANISH_GUATEMALA));
        windows_locale.Push(LocaleInfo("es_HN", LANG_SPANISH, SUBLANG_SPANISH_HONDURAS));
        windows_locale.Push(LocaleInfo("es_MX", LANG_SPANISH, SUBLANG_SPANISH_MEXICAN));
        windows_locale.Push(LocaleInfo("es_NI", LANG_SPANISH, SUBLANG_SPANISH_NICARAGUA));
        windows_locale.Push(LocaleInfo("es_PA", LANG_SPANISH, SUBLANG_SPANISH_PANAMA));
        windows_locale.Push(LocaleInfo("es_PE", LANG_SPANISH, SUBLANG_SPANISH_PERU));
        windows_locale.Push(LocaleInfo("es_PR", LANG_SPANISH, SUBLANG_SPANISH_PUERTO_RICO));
        windows_locale.Push(LocaleInfo("es_PY", LANG_SPANISH, SUBLANG_SPANISH_PARAGUAY));
        windows_locale.Push(LocaleInfo("es_SV", LANG_SPANISH, SUBLANG_SPANISH_EL_SALVADOR));
        windows_locale.Push(LocaleInfo("es_US", LANG_SPANISH, SUBLANG_SPANISH_US));
        windows_locale.Push(LocaleInfo("es_UY", LANG_SPANISH, SUBLANG_SPANISH_URUGUAY));
        windows_locale.Push(LocaleInfo("es_VE", LANG_SPANISH, SUBLANG_SPANISH_VENEZUELA));
        windows_locale.Push(LocaleInfo("et", LANG_ESTONIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("et_EE", LANG_ESTONIAN, SUBLANG_ESTONIAN_ESTONIA));
        windows_locale.Push(LocaleInfo("fi", LANG_FINNISH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("fi_FI", LANG_FINNISH, SUBLANG_FINNISH_FINLAND));
        windows_locale.Push(LocaleInfo("fr", LANG_FRENCH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("fr_BE", LANG_FRENCH, SUBLANG_FRENCH_BELGIAN));
        windows_locale.Push(LocaleInfo("fr_CA", LANG_FRENCH, SUBLANG_FRENCH_CANADIAN));
        windows_locale.Push(LocaleInfo("fr_CH", LANG_FRENCH, SUBLANG_FRENCH_SWISS));
        windows_locale.Push(LocaleInfo("fr_FR", LANG_FRENCH, SUBLANG_FRENCH));
        windows_locale.Push(LocaleInfo("fr_LU", LANG_FRENCH, SUBLANG_FRENCH_LUXEMBOURG));
        windows_locale.Push(LocaleInfo("ga", LANG_IRISH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("ga_IE", LANG_IRISH, SUBLANG_IRISH_IRELAND));
        windows_locale.Push(LocaleInfo("hi", LANG_HINDI, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("hi_IN", LANG_HINDI, SUBLANG_HINDI_INDIA));
        windows_locale.Push(LocaleInfo("hr", LANG_CROATIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("hr_HR", LANG_CROATIAN, SUBLANG_CROATIAN_CROATIA));
        windows_locale.Push(LocaleInfo("hu", LANG_HUNGARIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("hu_HU", LANG_HUNGARIAN, SUBLANG_HUNGARIAN_HUNGARY));
        windows_locale.Push(LocaleInfo("in", LANG_ARMENIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("in_ID", LANG_INDONESIAN, SUBLANG_INDONESIAN_INDONESIA));
        windows_locale.Push(LocaleInfo("is", LANG_ICELANDIC, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("is_IS", LANG_ICELANDIC, SUBLANG_ICELANDIC_ICELAND));
        windows_locale.Push(LocaleInfo("it", LANG_ITALIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("it_CH", LANG_ITALIAN, SUBLANG_ITALIAN_SWISS));
        windows_locale.Push(LocaleInfo("it_IT", LANG_ITALIAN, SUBLANG_ITALIAN));
        windows_locale.Push(LocaleInfo("iw", LANG_HEBREW, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("iw_IL", LANG_HEBREW, SUBLANG_HEBREW_ISRAEL));
        windows_locale.Push(LocaleInfo("ja", LANG_JAPANESE, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("ja_JP", LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN));
        windows_locale.Push(LocaleInfo("ko", LANG_KOREAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("ko_KR", LANG_KOREAN, SUBLANG_KOREAN));
        windows_locale.Push(LocaleInfo("lt", LANG_LITHUANIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("lv", LANG_LATVIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("lv_LV", LANG_LATVIAN, SUBLANG_LATVIAN_LATVIA));
        windows_locale.Push(LocaleInfo("mk", LANG_MACEDONIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("mk_MK", LANG_MACEDONIAN, SUBLANG_MACEDONIAN_MACEDONIA));
        windows_locale.Push(LocaleInfo("ms", LANG_MALAY, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("ms_MY", LANG_MALAY, SUBLANG_MALAY_MALAYSIA));
        windows_locale.Push(LocaleInfo("mt", LANG_MALTESE, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("mt_MT", LANG_MALTESE, SUBLANG_MALTESE_MALTA));
        windows_locale.Push(LocaleInfo("nl", LANG_DUTCH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("nl_BE", LANG_DUTCH, SUBLANG_DUTCH_BELGIAN));
        windows_locale.Push(LocaleInfo("nl_NL", LANG_DUTCH, SUBLANG_DUTCH));
        windows_locale.Push(LocaleInfo("no", LANG_NORWEGIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("no_NO", LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL));
        windows_locale.Push(LocaleInfo("no_NO_NY", LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK));
        windows_locale.Push(LocaleInfo("pl", LANG_POLISH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("pl_PL", LANG_POLISH, SUBLANG_POLISH_POLAND));
        windows_locale.Push(LocaleInfo("pt", LANG_PORTUGUESE, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("pt_BR", LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN));
        windows_locale.Push(LocaleInfo("pt_PT", LANG_PORTUGUESE, SUBLANG_PORTUGUESE));
        windows_locale.Push(LocaleInfo("ro", LANG_ROMANIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("ro_RO", LANG_ROMANIAN, SUBLANG_ROMANIAN_ROMANIA));
        windows_locale.Push(LocaleInfo("ru", LANG_RUSSIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("ru_RU", LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA));
        windows_locale.Push(LocaleInfo("sk", LANG_SLOVAK, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("sk_SK", LANG_SLOVAK, SUBLANG_SLOVAK_SLOVAKIA));
        windows_locale.Push(LocaleInfo("sl", LANG_SLOVENIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("sl_SI", LANG_SLOVENIAN, SUBLANG_SLOVENIAN_SLOVENIA));
        windows_locale.Push(LocaleInfo("sq", LANG_ALBANIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("sq_AL", LANG_ALBANIAN, SUBLANG_ALBANIAN_ALBANIA));
        windows_locale.Push(LocaleInfo("sr", LANG_SERBIAN_NEUTRAL, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("sv", LANG_SWEDISH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("sv_SE", LANG_SWEDISH, SUBLANG_SWEDISH));
        windows_locale.Push(LocaleInfo("th", LANG_THAI, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("th_TH", LANG_THAI, SUBLANG_THAI_THAILAND));
        windows_locale.Push(LocaleInfo("tr", LANG_TURKISH, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("tr_TR", LANG_TURKISH, SUBLANG_TURKISH_TURKEY));
        windows_locale.Push(LocaleInfo("uk", LANG_UKRAINIAN, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("uk_UA", LANG_UKRAINIAN, SUBLANG_UKRAINIAN_UKRAINE));
        windows_locale.Push(LocaleInfo("vi", LANG_VIETNAMESE, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("vi_VN", LANG_VIETNAMESE, SUBLANG_VIETNAMESE_VIETNAM));
        windows_locale.Push(LocaleInfo("zh", LANG_CHINESE, SUBLANG_NEUTRAL));
        windows_locale.Push(LocaleInfo("zh_CN", LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED));
        windows_locale.Push(LocaleInfo("zh_HK", LANG_CHINESE, SUBLANG_CHINESE_HONGKONG));
        windows_locale.Push(LocaleInfo("zh_SG", LANG_CHINESE, SUBLANG_CHINESE_SINGAPORE));
    }

    LANGID id = GetUserDefaultUILanguage();
    const auto bits    = ((1 << 9) - 1); 
    const auto lang    = id & bits;
    const auto sublang = id & (~bits);
          auto neutral = String();

    for (auto it = windows_locale.Begin(); it != windows_locale.End(); ++it)
    {
        if (it->lang == lang && it->sublang == SUBLANG_NEUTRAL)
            neutral = String(it->name); 
        if (it->lang == lang && it->sublang == sublang)
            return it->name; 
    }

    if (!neutral.Empty())
        return neutral; 
#endif
    return "(?)"; 
}

bool IsInDebugger()
{
#if defined(_WIN32)
    return ::IsDebuggerPresent();
#elif defined(__linux__) && !defined(__ANDROID__)
    // Slightly modified from https://stackoverflow.com/questions/3596781/how-to-detect-if-the-current-process-is-being-run-by-gdb
    char buffer[1024];
    int status_fd = open("/proc/self/status", O_RDONLY);
    ssize_t num_read = (status_fd == -1) ? 0 : read(status_fd, buffer, sizeof(buffer) - 1);

    if (num_read > 0)
    {
        static constexpr const char TracerPid[] = "TracerPid:";
        char *tracer_pid;

        buffer[num_read] = 0;
        tracer_pid = strstr(buffer, TracerPid);
        if (tracer_pid)
            return !!atoi(tracer_pid + sizeof(TracerPid) - 1);
    }
#endif
    return false;
}

String GetEnvVar(const String &var)
{
    const auto s = std::getenv(var.CString());
    return s != nullptr ? s : String::EMPTY; 
}

bool HasEnvVar(const String &var)
{
    return std::getenv(var.CString()) != nullptr;
}

void SetEnvVar(const String &name, const String &val)
{
	SetEnvironmentVariable(name.CString(), val.CString());
}

}
