// VC100Display - sets the desktop resolution while a game process runs, then puts it back.
//
// Started by VC100Checklist.asi as:  VC100Display.exe <width> <height> <game process id>
// The ASI loader's window mode swallows display-mode changes made inside the game process, so this runs outside it.
// It signals the event "VC100DisplayReady" once the mode is set, waits for the game to exit (or crash), and restores
// the desktop's normal resolution.
//
// Build (32-bit):  cl /O2 /MT VC100Display.cpp /Fe:VC100Display.exe user32.lib /link /SUBSYSTEM:WINDOWS

#include <windows.h>
#include <cstdlib>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR cmd, int) {
    int w = 0, h = 0; unsigned long pid = 0;
    char* p = cmd;
    w = (int)strtol(p, &p, 10); h = (int)strtol(p, &p, 10); pid = strtoul(p, &p, 10);
    HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, "VC100DisplayReady");
    HANDLE game = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : 0;

    bool changed = false;
    if (w > 0 && h > 0 && game) {
        DEVMODEA cur; ZeroMemory(&cur, sizeof(cur)); cur.dmSize = sizeof(cur);
        if (EnumDisplaySettingsA(0, ENUM_CURRENT_SETTINGS, &cur) && ((int)cur.dmPelsWidth != w || (int)cur.dmPelsHeight != h)) {
            DEVMODEA dm; ZeroMemory(&dm, sizeof(dm)); dm.dmSize = sizeof(dm);
            dm.dmPelsWidth = w; dm.dmPelsHeight = h; dm.dmDisplayFrequency = cur.dmDisplayFrequency;
            dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
            if (ChangeDisplaySettingsExA(0, &dm, 0, CDS_TEST, 0) != DISP_CHANGE_SUCCESSFUL)
                dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;          // that refresh rate isn't offered at this size
            // CDS_FULLSCREEN: temporary, never written to the registry; Windows also undoes it if this helper dies
            changed = ChangeDisplaySettingsExA(0, &dm, 0, CDS_FULLSCREEN, 0) == DISP_CHANGE_SUCCESSFUL;
        }
    }
    if (ready) SetEvent(ready);
    if (game) WaitForSingleObject(game, INFINITE);
    if (changed) ChangeDisplaySettingsExA(0, 0, 0, 0, 0);            // back to the desktop's own mode
    return 0;
}
