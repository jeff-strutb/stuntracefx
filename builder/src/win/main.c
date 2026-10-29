/* main.c: the release builder's Windows front end (Win32, common controls 6).
 * It builds Stunt Race FX for Windows.
 *
 *   1 what it makes
 *   2 the player's own ROM, checked by MD5, and the folder to build into
 *   3 the build (core/build.c), with progress
 *   4 the result: the game's folder, or what went wrong
 *
 * The game's executable is a plain file in the game folder beside it.
 *
 * Arguments fill in the choices for a scripted run, and --build starts it:
 *   --dest DIR  --rom FILE  --build
 * A scripted run quits when the build is done, its exit code 0 on success,
 * and writes its result to builder-result.txt beside the builder. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdio.h>
#include <string.h>
#include "rb.h"


#define WM_RB_PROGRESS (WM_APP + 1)
#define WM_RB_CHECKED  (WM_APP + 2)
#define WM_RB_DONE     (WM_APP + 3)

enum { ID_DEST = 100, ID_NEXT, ID_BACK, ID_BUILD, ID_CANCEL, ID_SHOW, ID_PLAY, ID_DONE, ID_PICK_ROM };
#define BOTTOM 420                       /* the buttons' row */

static HWND s_wnd, s_bar, s_status, s_build, s_romtext;
static HFONT s_font, s_bold, s_title, s_mono;
static HWND s_ctl[64];
static int s_nctl, s_page;
static wchar_t s_dest[MAX_PATH];
static wchar_t s_rom[MAX_PATH];
static int s_ok, s_checking, s_scripted;
static volatile int s_cancel;
static wchar_t s_status_text[256];
static double s_fraction;
static char s_err[1024], s_out[2048];
static int s_result;

/* ---- UTF-8 <-> UTF-16 -------------------------------------------------------------------- */
static void to8(const wchar_t *w, char *out, int n) { WideCharToMultiByte(CP_UTF8, 0, w, -1, out, n, NULL, NULL); }
static void to16(const char *s, wchar_t *out, int n) { MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n); }

/* the pieces in the game folder: the game */
int rb_host_payload(const char *name, char *path, size_t n, char *err, size_t errlen)
{
    wchar_t src[MAX_PATH], *s;
    GetModuleFileNameW(NULL, src, MAX_PATH);
    if ((s = wcsrchr(src, L'\\')) != NULL)
        *s = 0;
    if (!strcmp(name, "game"))
        swprintf(src + wcslen(src), MAX_PATH - wcslen(src), L"\\game\\stuntracefx.exe");
    else
        src[0] = 0;
    if (!src[0] || GetFileAttributesW(src) == INVALID_FILE_ATTRIBUTES) {
        snprintf(err, errlen, "the builder's game folder is missing a file (%s); unzip the whole download and run the "
                 "builder from that folder", name);
        return 0;
    }
    to8(src, path, (int)n);
    return 1;
}

/* ---- layout ----------------------------------------------------------------------------------- */
static int dpi(int v) { return MulDiv(v, (int)GetDpiForWindow(s_wnd), 96); }

static void clear_page(void)
{
    int i;
    for (i = 0; i < s_nctl; i++)
        DestroyWindow(s_ctl[i]);
    s_nctl = 0;
    s_bar = s_status = s_build = s_romtext = NULL;
}

static HWND add(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id, HFONT font)
{
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, dpi(x), dpi(y), dpi(w), dpi(h), s_wnd,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    if (s_nctl < 64)
        s_ctl[s_nctl++] = c;
    return c;
}

static HWND text(const wchar_t *s, int x, int y, int w, int h, HFONT f) { return add(L"STATIC", s, SS_LEFT | SS_NOPREFIX, x, y, w, h, 0, f); }
static HWND button(const wchar_t *s, int x, int y, int w, int id, int def)
{
    return add(L"BUTTON", s, WS_TABSTOP | (def ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), x, y, w, 28, id, s_font);
}

/* ---- pages --------------------------------------------------------------------------------- */
static void page_intro(void)
{
    s_page = 1;
    clear_page();
    text(L"Stunt Race FX Builder", 24, 20, 540, 32, s_title);
    text(L"Stunt Race FX Builder makes a native Windows build of Stunt Race FX (Super Nintendo, 1994) from your own "
         L"copy of it: the decompilation's C, compiled for your PC.", 24, 60, 540, 40, s_font);
    text(L"Stunt Race FX", 24, 116, 540, 24, s_bold);
    text(L"A Windows program (Windows 10 or later, 64-bit, with OpenGL 3.3), played with the keyboard or a game "
         L"controller. It needs your ROM of the cartridge, " RB_ROM_NAME L", once, to build.",
         44, 142, 520, 56, s_font);
    button(L"Continue", 464, BOTTOM, 100, ID_NEXT, 1);
}

static void update_build(void)
{
    if (s_build)
        EnableWindow(s_build, s_ok && !s_checking);
}

static void start_check(const wchar_t *path);

static void page_rom(void)
{
    int y = 140;
    s_page = 2;
    clear_page();
    text(L"Your copy of the game", 24, 20, 540, 32, s_title);
    text(L"The game's data is copyrighted, so the builder cannot include it. Provide a ROM of the Stunt Race FX cartridge: "
         L"the No-Intro dump of " RB_ROM_NAME L" (.sfc; a .smc with a copier's header is accepted). The build takes the "
         L"game's data from it; the ROM's code is not used.", 24, 58, 540, 74, s_font);
    text(L"Stunt Race FX: cartridge ROM", 24, y, 300, 22, s_bold);
    button(L"Choose...", 464, y - 3, 100, ID_PICK_ROM, 0);
    text(L"Expected MD5  " RB_ROM_MD5, 24, y + 22, 540, 18, s_mono);
    s_romtext = text(s_rom[0] ? L"" : L"No file chosen", 24, y + 40, 540, 66, s_mono);
    y += 120;
    text(L"Build into:", 24, y + 4, 100, 22, s_bold);
    add(L"STATIC", s_dest, SS_LEFT | SS_NOPREFIX | SS_PATHELLIPSIS | SS_SUNKEN, 124, y, 330, 26, 0, s_mono);
    button(L"Choose...", 464, y - 1, 100, ID_DEST, 0);
    button(L"Back", 352, BOTTOM, 100, ID_BACK, 0);
    s_build = button(L"Build", 464, BOTTOM, 100, ID_BUILD, 1);
    if (s_rom[0])
        start_check(s_rom);
    update_build();
}

static void page_build(void)
{
    s_page = 3;
    clear_page();
    text(L"Building Stunt Race FX", 24, 20, 540, 32, s_title);
    s_bar = add(PROGRESS_CLASSW, L"", 0, 24, 80, 540, 22, 0, s_font);
    SendMessageW(s_bar, PBM_SETRANGE32, 0, 1000);
    s_status = text(L"Starting", 24, 112, 540, 44, s_font);
    button(L"Cancel", 464, BOTTOM, 100, ID_CANCEL, 0);
}

static void page_result(void)
{
    wchar_t t[2048];
    s_page = 4;
    clear_page();
    if (s_result) {
        text(L"Stunt Race FX is ready", 24, 20, 540, 32, s_title);
        text(L"The build is complete. It carries everything it needs, so your ROM is no longer required to play.",
             24, 60, 540, 40, s_font);
        to16(s_out, t, 2048);
        add(L"STATIC", t, SS_LEFT | SS_NOPREFIX | SS_PATHELLIPSIS, 24, 110, 540, 22, 0, s_mono);
        text(L"Alt+Enter switches to full screen and back. Tab switches between Original and Enhanced mode, F1 sets the "
             L"frame rate to 60, F2 turns Widescreen on and off.", 24, 150, 540, 60, s_font);
        button(L"Done", 240, BOTTOM, 100, ID_DONE, 0);
        button(L"Open Folder", 352, BOTTOM, 100, ID_SHOW, 0);
        button(L"Play", 464, BOTTOM, 100, ID_PLAY, 1);
    } else {
        text(L"The build did not finish", 24, 20, 540, 32, s_title);
        to16(s_err[0] ? s_err : "The build failed.", t, 2048);
        text(t, 24, 64, 540, 120, s_font);
        button(L"Quit", 352, BOTTOM, 100, ID_DONE, 0);
        button(L"Back", 464, BOTTOM, 100, ID_BACK, 1);
    }
}

/* ---- checks ------------------------------------------------------------------------------ */
typedef struct check_job { wchar_t path[MAX_PATH]; rb_check c; } check_job;

static DWORD WINAPI check_thread(LPVOID p)
{
    check_job *j = (check_job *)p;
    char path[MAX_PATH * 3];
    to8(j->path, path, sizeof path);
    rb_check_rom(path, &j->c, NULL, NULL, NULL);
    PostMessageW(s_wnd, WM_RB_CHECKED, 0, (LPARAM)j);
    return 0;
}

static void start_check(const wchar_t *path)
{
    check_job *j = calloc(1, sizeof *j);
    HANDLE h;
    wcsncpy(j->path, path, MAX_PATH - 1);
    if (s_rom != path)
        wcsncpy(s_rom, path, MAX_PATH - 1);
    s_ok = 0;
    if (s_romtext) {
        wchar_t t[MAX_PATH + 32];
        swprintf(t, MAX_PATH + 32, L"%ls: checking...", wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') + 1 : path);
        SetWindowTextW(s_romtext, t);
    }
    s_checking++;
    update_build();
    h = CreateThread(NULL, 0, check_thread, j, 0, NULL);
    if (h)
        CloseHandle(h);
}

static void checked(check_job *j)
{
    s_checking--;
    if (!wcscmp(j->path, s_rom)) {
        wchar_t t[1024];
        const wchar_t *name = wcsrchr(j->path, L'\\') ? wcsrchr(j->path, L'\\') + 1 : j->path;
        s_ok = j->c.ok;
        swprintf(t, 1024, L"%ls\r\nYour file's MD5  %hs  %ls%ls%hs", name, j->c.md5,
                 j->c.ok ? L"\x2713 matches" : L"\x2717 does not match", j->c.note[0] ? L"\r\n" : L"", j->c.note);
        if (s_romtext) {
            SetWindowTextW(s_romtext, t);
            InvalidateRect(s_romtext, NULL, TRUE);
        }
    }
    free(j);
    update_build();
}

/* ---- pickers ---------------------------------------------------------------------------------- */
static int pick(int folder, wchar_t *out, const wchar_t *filter_name, const wchar_t *filter)
{
    IFileOpenDialog *d;
    IShellItem *it;
    PWSTR p;
    int ok = 0;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&d)))
        return 0;
    if (folder) {
        DWORD o;
        IFileOpenDialog_GetOptions(d, &o);
        IFileOpenDialog_SetOptions(d, o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    } else {
        COMDLG_FILTERSPEC f[2] = { { filter_name, filter }, { L"All files", L"*.*" } };
        IFileOpenDialog_SetFileTypes(d, 2, f);
    }
    if (SUCCEEDED(IFileOpenDialog_Show(d, s_wnd)) && SUCCEEDED(IFileOpenDialog_GetResult(d, &it))) {
        if (SUCCEEDED(IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &p))) {
            wcsncpy(out, p, MAX_PATH - 1);
            out[MAX_PATH - 1] = 0;
            CoTaskMemFree(p);
            ok = 1;
        }
        IShellItem_Release(it);
    }
    IFileOpenDialog_Release(d);
    return ok;
}

/* ---- the build ------------------------------------------------------------------------------- */
static void progress(void *ctx, double f, const char *s)
{
    (void)ctx;
    s_fraction = f;
    if (s)
        to16(s, s_status_text, 256);
    PostMessageW(s_wnd, WM_RB_PROGRESS, 0, 0);
}

static DWORD WINAPI build_thread(LPVOID p)
{
    rb_job job = { 0 };
    char dest[MAX_PATH * 3], rom[MAX_PATH * 3];
    (void)p;
    to8(s_dest, dest, sizeof dest);
    to8(s_rom, rom, sizeof rom);
    job.target = RB_WINDOWS;
    job.dest_dir = dest;
    job.rom = rom;
    s_err[0] = 0;
    s_result = rb_build(&job, progress, NULL, &s_cancel, s_err, sizeof s_err);
    rb_output_path(&job, s_out, sizeof s_out);
    PostMessageW(s_wnd, WM_RB_DONE, 0, 0);
    return 0;
}

static void build(void)
{
    rb_job job = { 0 };
    char dest[MAX_PATH * 3], out[2048];
    wchar_t msg[4096], o16[2048];
    int ours = 0;
    HANDLE h;
    to8(s_dest, dest, sizeof dest);
    job.target = RB_WINDOWS;
    job.dest_dir = dest;
    if (rb_output_exists(&job, &ours)) {
        rb_output_path(&job, out, sizeof out);
        to16(out, o16, 2048);
        if (!ours) {
            swprintf(msg, 4096, L"%ls already exists and was not made by Stunt Race FX Builder, so it will not be replaced. "
                                L"Move it away or choose another folder.", o16);
            if (s_scripted) {
                to8(msg, s_err, sizeof s_err);
                s_result = 0;
                PostMessageW(s_wnd, WM_RB_DONE, 0, 0);
            } else {
                MessageBoxW(s_wnd, msg, L"Stunt Race FX Builder", MB_ICONWARNING | MB_OK);
            }
            return;
        }
        swprintf(msg, 4096, L"An earlier build is at %ls.\r\n\r\nReplace it? Your records and settings are kept; they live "
                            L"in your AppData folder, not in the game's.", o16);
        if (!s_scripted && MessageBoxW(s_wnd, msg, L"Stunt Race FX Builder", MB_ICONQUESTION | MB_YESNO) != IDYES)
            return;
    }
    s_cancel = 0;
    page_build();
    h = CreateThread(NULL, 0, build_thread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
}

/* a scripted run's result, beside the builder */
static void scripted_result(void)
{
    wchar_t p[MAX_PATH], *s;
    FILE *f;
    GetModuleFileNameW(NULL, p, MAX_PATH);
    if ((s = wcsrchr(p, L'\\')) != NULL)
        wcscpy(s + 1, L"builder-result.txt");
    if ((f = _wfopen(p, L"w")) != NULL) {
        fprintf(f, "%s: %s\n", s_result ? "built" : "failed", s_result ? s_out : s_err);
        fclose(f);
    }
}

/* ---- the window ------------------------------------------------------------------------------ */
static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        HWND c = (HWND)lp;
        SetBkMode(dc, TRANSPARENT);
        if (c == s_romtext && s_rom[0] && !s_checking)
            SetTextColor(dc, s_ok ? RGB(0, 128, 0) : RGB(192, 0, 0));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_DEST:
            if (pick(1, s_dest, NULL, NULL))
                page_rom();
            break;
        case ID_NEXT: page_rom(); break;
        case ID_BACK: if (s_page == 4) page_rom(); else page_intro(); break;
        case ID_PICK_ROM: {
            wchar_t p[MAX_PATH];
            if (pick(0, p, L"SNES ROM (*.sfc;*.smc)", L"*.sfc;*.smc"))
                start_check(p);
            break;
        }
        case ID_BUILD: build(); break;
        case ID_CANCEL:
            s_cancel = 1;
            EnableWindow((HWND)lp, FALSE);
            if (s_status)
                SetWindowTextW(s_status, L"Stopping...");
            break;
        case ID_SHOW: case ID_PLAY: {
            wchar_t o[2048], exe[2200];
            to16(s_out, o, 2048);
            if (LOWORD(wp) == ID_SHOW) {
                ShellExecuteW(w, L"open", o, NULL, NULL, SW_SHOWNORMAL);
            } else {
                swprintf(exe, 2200, L"%ls\\Stunt Race FX.exe", o);
                ShellExecuteW(w, L"open", exe, NULL, o, SW_SHOWNORMAL);
            }
            break;
        }
        case ID_DONE: DestroyWindow(w); break;
        }
        return 0;
    case WM_RB_CHECKED:
        checked((check_job *)lp);
        return 0;
    case WM_RB_PROGRESS:
        if (s_bar) {
            SendMessageW(s_bar, PBM_SETPOS, (WPARAM)(s_fraction * 1000), 0);
            if (!s_cancel)
                SetWindowTextW(s_status, s_status_text);
        }
        return 0;
    case WM_RB_DONE:
        page_result();
        if (s_scripted) {
            scripted_result();
            PostQuitMessage(s_result ? 0 : 1);
        }
        return 0;
    case WM_CLOSE:
        if (s_page == 3) {                              /* stop the build first; it cleans up after itself */
            s_cancel = 1;
            return 0;
        }
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    WNDCLASSW wc = { 0 };
    NONCLIENTMETRICSW ncm;
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    LOGFONTW lf;
    RECT r = { 0, 0, 588, BOTTOM + 48 };
    MSG msg;
    PWSTR docs;
    UINT d;
    int quit = 0;
    (void)prev, (void)cmd;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    InitCommonControlsEx(&icc);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Documents, 0, NULL, &docs))) {
        swprintf(s_dest, MAX_PATH, L"%ls\\Games", docs);
        CoTaskMemFree(docs);
    }
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"StuntRaceFXBuilder";
    RegisterClassW(&wc);
    s_wnd = CreateWindowExW(0, L"StuntRaceFXBuilder", L"Stunt Race FX Builder", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, NULL, NULL, inst, NULL);
    d = GetDpiForWindow(s_wnd);
    r.right = MulDiv(r.right, (int)d, 96);
    r.bottom = MulDiv(r.bottom, (int)d, 96);
    AdjustWindowRectExForDpi(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0, d);
    SetWindowPos(s_wnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
    memset(&ncm, 0, sizeof ncm);
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0, d);
    s_font = CreateFontIndirectW(&ncm.lfMessageFont);
    lf = ncm.lfMessageFont;
    lf.lfWeight = FW_SEMIBOLD;
    s_bold = CreateFontIndirectW(&lf);
    lf.lfHeight = MulDiv(lf.lfHeight, 3, 2);
    s_title = CreateFontIndirectW(&lf);
    lf = ncm.lfMessageFont;
    wcscpy(lf.lfFaceName, L"Consolas");
    s_mono = CreateFontIndirectW(&lf);
    {
        int argc = 0, i;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (i = 1; argv && i < argc; i++) {
            const wchar_t *k = argv[i], *v = i + 1 < argc ? argv[i + 1] : L"";
            if (!wcscmp(k, L"--build")) { s_scripted = 1; continue; }
            if (!wcscmp(k, L"--dest")) wcsncpy(s_dest, v, MAX_PATH - 1);
            else if (!wcscmp(k, L"--rom")) wcsncpy(s_rom, v, MAX_PATH - 1);
            else continue;
            i++;
        }
        LocalFree(argv);
        page_intro();
        ShowWindow(s_wnd, show);
        if (s_scripted) {                  /* a scripted run: the ROM is checked by the build itself */
            page_rom();
            build();
        }
    }
    while ((quit = GetMessageW(&msg, NULL, 0, 0)) > 0) {
        if (!IsDialogMessageW(s_wnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
