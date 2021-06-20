#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include "directinput.h"
#include "dd.h"
#include "winapi_hooks.h"
#include "hook.h"
#include "debug.h"
#include "dllmain.h"

#ifdef _MSC_VER
#include "detours.h"
#endif

BOOL g_hook_active;
int g_hook_method = 1;

GETCURSORPOSPROC real_GetCursorPos = GetCursorPos;
CLIPCURSORPROC real_ClipCursor = ClipCursor;
SHOWCURSORPROC real_ShowCursor = ShowCursor;
SETCURSORPROC real_SetCursor = SetCursor;
GETWINDOWRECTPROC real_GetWindowRect = GetWindowRect;
GETCLIENTRECTPROC real_GetClientRect = GetClientRect;
CLIENTTOSCREENPROC real_ClientToScreen = ClientToScreen;
SCREENTOCLIENTPROC real_ScreenToClient = ScreenToClient;
SETCURSORPOSPROC real_SetCursorPos = SetCursorPos;
WINDOWFROMPOINTPROC real_WindowFromPoint = WindowFromPoint;
GETCLIPCURSORPROC real_GetClipCursor = GetClipCursor;
GETCURSORINFOPROC real_GetCursorInfo = GetCursorInfo;
GETSYSTEMMETRICSPROC real_GetSystemMetrics = GetSystemMetrics;
SETWINDOWPOSPROC real_SetWindowPos = SetWindowPos;
MOVEWINDOWPROC real_MoveWindow = MoveWindow;
SENDMESSAGEAPROC real_SendMessageA = SendMessageA;
SETWINDOWLONGAPROC real_SetWindowLongA = SetWindowLongA;
GETWINDOWLONGAPROC real_GetWindowLongA = GetWindowLongA;
ENABLEWINDOWPROC real_EnableWindow = EnableWindow;
CREATEWINDOWEXAPROC real_CreateWindowExA = CreateWindowExA;
DESTROYWINDOWPROC real_DestroyWindow = DestroyWindow;
MAPWINDOWPOINTSPROC real_MapWindowPoints = MapWindowPoints;
SETWINDOWSHOOKEXAPROC real_SetWindowsHookExA = SetWindowsHookExA;
GETDEVICECAPSPROC real_GetDeviceCaps = GetDeviceCaps;
LOADLIBRARYAPROC real_LoadLibraryA = LoadLibraryA;
LOADLIBRARYWPROC real_LoadLibraryW = LoadLibraryW;
LOADLIBRARYEXAPROC real_LoadLibraryExA = LoadLibraryExA;
LOADLIBRARYEXWPROC real_LoadLibraryExW = LoadLibraryExW;

static HOOKLIST g_hooks[] =
{
    {
        "user32.dll",
        TRUE,
        {
            { "GetCursorPos", (PROC)fake_GetCursorPos, (PROC*)&real_GetCursorPos },
            { "ClipCursor", (PROC)fake_ClipCursor, (PROC*)&real_ClipCursor },
            { "ShowCursor", (PROC)fake_ShowCursor, (PROC*)&real_ShowCursor },
            { "SetCursor", (PROC)fake_SetCursor, (PROC*)&real_SetCursor },
            { "GetWindowRect", (PROC)fake_GetWindowRect, (PROC*)&real_GetWindowRect },
            { "GetClientRect", (PROC)fake_GetClientRect, (PROC*)&real_GetClientRect },
            { "ClientToScreen", (PROC)fake_ClientToScreen, (PROC*)&real_ClientToScreen },
            { "ScreenToClient", (PROC)fake_ScreenToClient, (PROC*)&real_ScreenToClient },
            { "SetCursorPos", (PROC)fake_SetCursorPos, (PROC*)&real_SetCursorPos },
            { "GetClipCursor", (PROC)fake_GetClipCursor, (PROC*)&real_GetClipCursor },
            { "WindowFromPoint", (PROC)fake_WindowFromPoint, (PROC*)&real_WindowFromPoint },
            { "GetCursorInfo", (PROC)fake_GetCursorInfo, (PROC*)&real_GetCursorInfo },
            { "GetSystemMetrics", (PROC)fake_GetSystemMetrics, (PROC*)&real_GetSystemMetrics },
            { "SetWindowPos", (PROC)fake_SetWindowPos, (PROC*)&real_SetWindowPos },
            { "MoveWindow", (PROC)fake_MoveWindow, (PROC*)&real_MoveWindow },
            { "SendMessageA", (PROC)fake_SendMessageA, (PROC*)&real_SendMessageA },
            { "SetWindowLongA", (PROC)fake_SetWindowLongA, (PROC*)&real_SetWindowLongA },
            { "GetWindowLongA", (PROC)fake_GetWindowLongA, (PROC*)&real_GetWindowLongA },
            { "EnableWindow", (PROC)fake_EnableWindow, (PROC*)&real_EnableWindow },
            { "CreateWindowExA", (PROC)fake_CreateWindowExA, (PROC*)&real_CreateWindowExA },
            { "DestroyWindow", (PROC)fake_DestroyWindow, (PROC*)&real_DestroyWindow },
            { "MapWindowPoints", (PROC)fake_MapWindowPoints, (PROC*)&real_MapWindowPoints },
            { "", NULL, NULL }
        }
    },
    {
        "gdi32.dll",
        TRUE,
        {
            { "GetDeviceCaps", (PROC)fake_GetDeviceCaps, (PROC*)&real_GetDeviceCaps },
            { "", NULL, NULL }
        }
    },
    {
        "kernel32.dll",
        FALSE,
        {
            { "LoadLibraryA", (PROC)fake_LoadLibraryA, (PROC*)&real_LoadLibraryA },
            { "LoadLibraryW", (PROC)fake_LoadLibraryW, (PROC*)&real_LoadLibraryW },
            { "LoadLibraryExA", (PROC)fake_LoadLibraryExA, (PROC*)&real_LoadLibraryExA },
            { "LoadLibraryExW", (PROC)fake_LoadLibraryExW, (PROC*)&real_LoadLibraryExW },
            { "", NULL, NULL }
        }
    },
    {
        "",
        FALSE,
        {
            { "", NULL, NULL }
        }
    }
};

void hook_patch_iat(HMODULE hmod, BOOL unhook, char* module_name, char* function_name, PROC new_function)
{
    HOOKLIST hooks[2];
    memset(&hooks, 0, sizeof(hooks));

    hooks[0].enabled = TRUE;
    hooks[0].data[0].new_function = new_function;

    strncpy(hooks[0].module_name, module_name, sizeof(hooks[0].module_name) - 1);
    strncpy(hooks[0].data[0].function_name, function_name, sizeof(hooks[0].data[0].function_name) - 1);

    hook_patch_iat_list(hmod, unhook, (HOOKLIST*)&hooks);
}

void hook_patch_iat_list(HMODULE hmod, BOOL unhook, HOOKLIST* hooks)
{
    if (!hmod || hmod == INVALID_HANDLE_VALUE || !hooks)
        return;

#ifdef _MSC_VER
    __try
    {
#endif
        PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)hmod;
        if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
            return;

        PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((DWORD)dos_header + (DWORD)dos_header->e_lfanew);
        if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
            return;

        PIMAGE_IMPORT_DESCRIPTOR import_desc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)dos_header +
            (DWORD)(nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));

        if (import_desc == (PIMAGE_IMPORT_DESCRIPTOR)nt_headers)
            return;

        while (import_desc->FirstThunk)
        {
            for (int i = 0; hooks[i].module_name[0]; i++)
            {
                if (!hooks[i].enabled)
                    continue;

                char* imp_module_name = (char*)((DWORD)dos_header + (DWORD)(import_desc->Name));

                if (_stricmp(imp_module_name, hooks[i].module_name) == 0)
                {
                    PIMAGE_THUNK_DATA first_thunk =
                        (PIMAGE_THUNK_DATA)((DWORD)dos_header + (DWORD)import_desc->FirstThunk);

                    PIMAGE_THUNK_DATA original_first_thunk =
                        (PIMAGE_THUNK_DATA)((DWORD)dos_header + (DWORD)import_desc->OriginalFirstThunk);

                    while (first_thunk->u1.Function && original_first_thunk->u1.AddressOfData)
                    {
                        PIMAGE_IMPORT_BY_NAME import =
                            (PIMAGE_IMPORT_BY_NAME)((DWORD)dos_header + original_first_thunk->u1.AddressOfData);

                        if ((original_first_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) == 0)
                        {
                            for (int x = 0; hooks[i].data[x].function_name[0]; x++)
                            {
                                if (!unhook && !hooks[i].data[x].new_function)
                                    continue;

                                if (_stricmp((const char*)import->Name, hooks[i].data[x].function_name) == 0)
                                {
                                    DWORD old_protect;

                                    if (VirtualProtect(
                                        &first_thunk->u1.Function, sizeof(DWORD), PAGE_READWRITE, &old_protect))
                                    {
                                        if (unhook)
                                        {
                                            DWORD org =
                                                (DWORD)GetProcAddress(
                                                    GetModuleHandle(hooks[i].module_name),
                                                    hooks[i].data[x].function_name);

                                            if (org)
                                            {
                                                first_thunk->u1.Function = org;
                                            }
                                        }
                                        else
                                        {
                                            first_thunk->u1.Function = (DWORD)hooks[i].data[x].new_function;
                                        }

                                        VirtualProtect(
                                            &first_thunk->u1.Function, sizeof(DWORD), old_protect, &old_protect);
                                    }

                                    break;
                                }
                            }
                        }

                        first_thunk++;
                        original_first_thunk++;
                    }
                }
            }

            import_desc++;
        }
#ifdef _MSC_VER
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
#endif
}

void hook_create(HOOKLIST* hooks)
{
#ifdef _MSC_VER
    if (g_hook_method == 2)
    {
        for (int i = 0; hooks[i].module_name[0]; i++)
        {
            if (!hooks[i].enabled)
                continue;

            for (int x = 0; hooks[i].data[x].function_name[0]; x++)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourAttach((PVOID*)hooks[i].data[x].function, (PVOID)hooks[i].data[x].new_function);
                DetourTransactionCommit();
            }
        }
    }
#endif

    if (g_hook_method == 3 || g_hook_method == 4)
    {
        char game_exe_path[MAX_PATH] = { 0 };
        char game_dir[MAX_PATH] = { 0 };

        if (GetModuleFileNameA(NULL, game_exe_path, MAX_PATH))
        {
            _splitpath(game_exe_path, NULL, game_dir, NULL, NULL);

            char mod_path[MAX_PATH] = { 0 };
            char mod_dir[MAX_PATH] = { 0 };
            char mod_filename[MAX_PATH] = { 0 };
            HMODULE hmod = NULL;
            HANDLE process = NULL;

#ifndef _MSC_VER
            HMODULE mods[512];
            memset(mods, 0, sizeof(mods));
            process = GetCurrentProcess();
            EnumProcessModules(process, mods, sizeof(mods) - sizeof(mods[0]), NULL);
            for (int i = 0; i < sizeof(mods) / sizeof(mods[0]) && (hmod = mods[i]); i++)
#else
            while (hmod = DetourEnumerateModules(hmod))
#endif
            {
                if (hmod == g_ddraw_module)
                    continue;

                if (GetModuleFileNameA(hmod, mod_path, MAX_PATH))
                {
                    TRACE_EXT("Module %s = %p\n", mod_path, hmod);

                    _splitpath(mod_path, NULL, mod_dir, mod_filename, NULL);

                    /* Don't hook reshade/swiftshader/mesa3d */
                    if (_strcmpi(mod_filename, "opengl32") == 0 ||
                        _strcmpi(mod_filename, "d3d9") == 0 ||
                        _strcmpi(mod_filename, "Shw32") == 0)
                        continue;

                    if (_strnicmp(game_dir, mod_dir, strlen(game_dir)) == 0)
                    {
                        hook_patch_iat_list(hmod, FALSE, hooks);
                    }
                }
            }

            if (process)
                CloseHandle(process);
        }
    }

    if (g_hook_method == 1)
    {
        hook_patch_iat_list(GetModuleHandle(NULL), FALSE, hooks);
    }
}

void hook_revert(HOOKLIST* hooks)
{
#ifdef _MSC_VER
    if (g_hook_method == 2)
    {
        for (int i = 0; hooks[i].module_name[0]; i++)
        {
            if (!hooks[i].enabled)
                continue;

            for (int x = 0; hooks[i].data[x].function_name[0]; x++)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourDetach((PVOID*)hooks[i].data[x].function, (PVOID)hooks[i].data[x].new_function);
                DetourTransactionCommit();
            }
        }
    }
#endif

    if (g_hook_method == 3 || g_hook_method == 4)
    {
        char game_exe_path[MAX_PATH] = { 0 };
        char game_dir[MAX_PATH] = { 0 };

        if (GetModuleFileNameA(NULL, game_exe_path, MAX_PATH))
        {
            _splitpath(game_exe_path, NULL, game_dir, NULL, NULL);

            char mod_path[MAX_PATH] = { 0 };
            char mod_dir[MAX_PATH] = { 0 };
            HMODULE hmod = NULL;
            HANDLE process = NULL;

#ifndef _MSC_VER
            HMODULE mods[512];
            memset(mods, 0, sizeof(mods));
            process = GetCurrentProcess();
            EnumProcessModules(process, mods, sizeof(mods) - sizeof(mods[0]), NULL);
            for (int i = 0; i < sizeof(mods) / sizeof(mods[0]) && (hmod = mods[i]); i++)
#else
            while (hmod = DetourEnumerateModules(hmod))
#endif
            {
                if (hmod == g_ddraw_module)
                    continue;

                if (GetModuleFileNameA(hmod, mod_path, MAX_PATH))
                {
                    _splitpath(mod_path, NULL, mod_dir, NULL, NULL);

                    if (_strnicmp(game_dir, mod_dir, strlen(game_dir)) == 0)
                    {
                        hook_patch_iat_list(hmod, TRUE, hooks);
                    }
                }
            }

            if (process)
                CloseHandle(process);
        }
    }

    if (g_hook_method == 1)
    {
        hook_patch_iat_list(GetModuleHandle(NULL), TRUE, hooks);
    }
}

void hook_init()
{
    if (!g_hook_active || g_hook_method == 3 || g_hook_method == 4)
    {
#ifdef _MSC_VER
        if (!g_hook_active && g_hook_method == 3)
        {
            real_DirectInputCreateA =
                (DIRECTINPUTCREATEAPROC)GetProcAddress(LoadLibraryA("dinput.dll"), "DirectInputCreateA");

            if (real_DirectInputCreateA)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourAttach((PVOID*)&real_DirectInputCreateA, (PVOID)fake_DirectInputCreateA);
                DetourTransactionCommit();
            }

            real_DirectInputCreateW =
                (DIRECTINPUTCREATEWPROC)GetProcAddress(LoadLibraryA("dinput.dll"), "DirectInputCreateW");

            if (real_DirectInputCreateW)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourAttach((PVOID*)&real_DirectInputCreateW, (PVOID)fake_DirectInputCreateW);
                DetourTransactionCommit();
            }

            real_DirectInputCreateEx =
                (DIRECTINPUTCREATEEXPROC)GetProcAddress(LoadLibraryA("dinput.dll"), "DirectInputCreateEx");

            if (real_DirectInputCreateEx)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourAttach((PVOID*)&real_DirectInputCreateEx, (PVOID)fake_DirectInputCreateEx);
                DetourTransactionCommit();
            }

            real_DirectInput8Create =
                (DIRECTINPUT8CREATEPROC)GetProcAddress(LoadLibraryA("dinput8.dll"), "DirectInput8Create");

            if (real_DirectInput8Create)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourAttach((PVOID*)&real_DirectInput8Create, (PVOID)fake_DirectInput8Create);
                DetourTransactionCommit();
            }
        }
#endif

        g_hook_active = TRUE;

        if (g_hook_method == 3 || g_hook_method == 4)
        {
            for (int i = 0; g_hooks[i].module_name[0]; i++)
            {
                if (_stricmp(g_hooks[i].module_name, "kernel32.dll") == 0)
                {
                    g_hooks[i].enabled = TRUE;
                }
            }
        }

        hook_create((HOOKLIST*)&g_hooks);
    }
}

void hook_early_init()
{
    hook_patch_iat(GetModuleHandle(NULL), FALSE, "dinput.dll", "DirectInputCreateA", (PROC)fake_DirectInputCreateA);
    hook_patch_iat(GetModuleHandle(NULL), FALSE, "dinput.dll", "DirectInputCreateW", (PROC)fake_DirectInputCreateW);
    hook_patch_iat(GetModuleHandle(NULL), FALSE, "dinput.dll", "DirectInputCreateEx", (PROC)fake_DirectInputCreateEx);
    hook_patch_iat(GetModuleHandle(NULL), FALSE, "dinput8.dll", "DirectInput8Create", (PROC)fake_DirectInput8Create);
    hook_patch_iat(GetModuleHandle("AcGenral"), FALSE, "user32.dll", "SetWindowsHookExA", (PROC)fake_SetWindowsHookExA);
    hook_patch_iat(GetModuleHandle(NULL), FALSE, "user32.dll", "SetWindowsHookExA", (PROC)fake_SetWindowsHookExA);
}

void hook_exit()
{
    if (g_hook_active)
    {
        g_hook_active = FALSE;

#ifdef _MSC_VER
        if (g_hook_method == 3)
        {
            if (real_DirectInputCreateA)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourDetach((PVOID*)&real_DirectInputCreateA, (PVOID)fake_DirectInputCreateA);
                DetourTransactionCommit();
            }

            if (real_DirectInputCreateW)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourDetach((PVOID*)&real_DirectInputCreateW, (PVOID)fake_DirectInputCreateW);
                DetourTransactionCommit();
            }

            if (real_DirectInputCreateEx)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourDetach((PVOID*)&real_DirectInputCreateEx, (PVOID)fake_DirectInputCreateEx);
                DetourTransactionCommit();
            }

            if (real_DirectInput8Create)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourDetach((PVOID*)&real_DirectInput8Create, (PVOID)fake_DirectInput8Create);
                DetourTransactionCommit();
            }
        }
#endif

        hook_revert((HOOKLIST*)&g_hooks);
    }

    hook_patch_iat(GetModuleHandle(NULL), TRUE, "dinput.dll", "DirectInputCreateA", (PROC)fake_DirectInputCreateA);
    hook_patch_iat(GetModuleHandle(NULL), TRUE, "dinput.dll", "DirectInputCreateW", (PROC)fake_DirectInputCreateW);
    hook_patch_iat(GetModuleHandle(NULL), TRUE, "dinput.dll", "DirectInputCreateEx", (PROC)fake_DirectInputCreateEx);
    hook_patch_iat(GetModuleHandle(NULL), TRUE, "dinput8.dll", "DirectInput8Create", (PROC)fake_DirectInput8Create);
    hook_patch_iat(GetModuleHandle("AcGenral"), TRUE, "user32.dll", "SetWindowsHookExA", (PROC)fake_SetWindowsHookExA);
    hook_patch_iat(GetModuleHandle(NULL), TRUE, "user32.dll", "SetWindowsHookExA", (PROC)fake_SetWindowsHookExA);
}
