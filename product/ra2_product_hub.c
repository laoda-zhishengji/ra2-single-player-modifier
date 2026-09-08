#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef struct { const wchar_t *command; const wchar_t *exe; const wchar_t *args; } Route;

static const Route kRoutes[] = {
    {L"--status",              L"ra2_product_status.exe",       L""},
    {L"--money-on",            L"ra2_product_control.exe",      L"--money-on"},
    {L"--money-off",           L"ra2_product_control.exe",      L"--money-off"},
    {L"--power-on",            L"ra2_product_control.exe",      L"--power-on"},
    {L"--power-off",           L"ra2_product_control.exe",      L"--power-off"},
    {L"--production-status",   L"ra2_product_production.exe",   L"--status"},
    {L"--production-capture",  L"ra2_product_production.exe",   L"--capture"},
    {L"--production-on",       L"ra2_product_production.exe",   L"--apply"},
    {L"--production-off",      L"ra2_product_production.exe",   L"--restore"},
    {L"--super-status",        L"ra2_product_super.exe",        L"--status"},
    {L"--super-capture",       L"ra2_product_super.exe",        L"--capture"},
    {L"--super-on",            L"ra2_product_super.exe",        L"--apply"},
    {L"--super-off",           L"ra2_product_super.exe",        L"--restore"},
    {L"--fog-status",          L"ra2_product_fog.exe",          L"--status"},
    {L"--fog-on",              L"ra2_product_fog.exe",          L"--on"},
    {L"--fog-off",             L"ra2_product_fog.exe",          L"--off"},
    {L"--distance-status",     L"ra2_product_build_distance.exe", L"--probe"},
    {L"--distance-on",         L"ra2_product_build_distance.exe", L"--apply"},
    {L"--distance-off",        L"ra2_product_build_distance.exe", L"--restore"},
    {L"--repair-status",       L"ra2_product_services.exe",     L"--status"},
    {L"--repair-auto-on",     L"ra2_product_services.exe",     L"--auto-on"},
    {L"--repair-auto-off",    L"ra2_product_services.exe",     L"--auto-off"},
    {L"--repair-garrison-on", L"ra2_product_services.exe",     L"--garrison-on"},
    {L"--repair-garrison-off",L"ra2_product_services.exe",     L"--garrison-off"},
    {L"--repair-all-off",     L"ra2_product_services.exe",     L"--all-off"}
};

static const wchar_t *kRequiredFiles[] = {
    L"ra2_product_status.exe", L"ra2_product_control.exe",
    L"ra2_product_production.exe", L"ra2_product_super.exe",
    L"ra2_product_fog.exe", L"ra2_product_build_distance.exe",
    L"ra2_product_services.exe", L"ra2_product_auto_repair.exe",
    L"ra2_product_garrison_repair.exe", L"ra2_product_instant_service.exe",
    L"ra2_product_super_service.exe", L"ra2_product_ui.exe"
};

static void module_dir(wchar_t *out, DWORD cap) {
    DWORD n = GetModuleFileNameW(NULL, out, cap);
    if (!n || n >= cap) { out[0] = 0; return; }
    wchar_t *slash = wcsrchr(out, L'\\');
    if (slash) *(slash + 1) = 0;
}

static const Route *find_route(const wchar_t *command) {
    for (size_t i = 0; i < sizeof(kRoutes) / sizeof(kRoutes[0]); ++i)
        if (!_wcsicmp(command, kRoutes[i].command)) return &kRoutes[i];
    return NULL;
}

static int run_route(const Route *route) {
    wchar_t dir[MAX_PATH], cmd[MAX_PATH * 2];
    fflush(stdout);
    module_dir(dir, ARRAYSIZE(dir));
    if (!dir[0]) return 3;
    if (route->args[0]) swprintf_s(cmd, ARRAYSIZE(cmd), L"\"%ls%ls\" %ls", dir, route->exe, route->args);
    else swprintf_s(cmd, ARRAYSIZE(cmd), L"\"%ls%ls\"", dir, route->exe);
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, 0, NULL, dir, &si, &pi)) {
        wprintf(L"HUB_START_FAILED exe=%ls error=%lu\n", route->exe, GetLastError());
        return 4;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return (int)code;
}

static void usage(void) {
    wprintf(L"Usage: ra2_product_hub.exe --self-check | --status | --money-on|--money-off | --power-on|--power-off\n");
    wprintf(L"       --production-status|--production-capture|--production-on|--production-off\n");
    wprintf(L"       --super-status|--super-capture|--super-on|--super-off\n");
    wprintf(L"       --fog-status|--fog-on|--fog-off | --distance-status|--distance-on|--distance-off\n");
    wprintf(L"       --repair-status|--repair-auto-on|--repair-auto-off|--repair-garrison-on|--repair-garrison-off|--repair-all-off\n");
}

int wmain(int argc, wchar_t **argv) {
    if (argc != 2) { usage(); return 2; }
    if (!_wcsicmp(argv[1], L"--self-check")) {
        wchar_t dir[MAX_PATH];
        module_dir(dir, ARRAYSIZE(dir));
        if (!dir[0]) return 3;
        int missing = 0;
        wprintf(L"PRODUCT_SELF_CHECK\n");
        for (size_t i = 0; i < sizeof(kRequiredFiles) / sizeof(kRequiredFiles[0]); ++i) {
            wchar_t path[MAX_PATH];
            swprintf_s(path, ARRAYSIZE(path), L"%ls%ls", dir, kRequiredFiles[i]);
            BOOL present = GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
            wprintf(L"%ls=%ls\n", kRequiredFiles[i], present ? L"PRESENT" : L"MISSING");
            if (!present) ++missing;
        }
        wprintf(L"SELF_CHECK=%ls\n", missing ? L"FAILED" : L"PASS");
        return missing ? 5 : 0;
    }
    if (!_wcsicmp(argv[1], L"--status")) {
        const wchar_t *status_commands[] = {
            L"--status", L"--repair-status", L"--production-status",
            L"--super-status", L"--fog-status", L"--distance-status"
        };
        int core_status = 0;
        for (size_t i = 0; i < sizeof(status_commands) / sizeof(status_commands[0]); ++i) {
            const Route *status_route = find_route(status_commands[i]);
            if (!status_route) continue;
            wprintf(L"\n[STATUS %ls]\n", status_commands[i]);
            fflush(stdout);
            int code = run_route(status_route);
            if (i == 0) core_status = code;
        }
        return core_status;
    }
    const Route *route = find_route(argv[1]);
    if (!route) { usage(); return 2; }
    return run_route(route);
}
