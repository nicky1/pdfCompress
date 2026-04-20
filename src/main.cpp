/*
 * Windows-only GUI: pick one PDF, write compressed copy next to source.
 * Output name: <stem>compress<millis>.pdf
 *
 * Build (example with vcpkg toolchain):
 *   cmake -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
 *   cmake --build build --config Release
 */

#include <Windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>

namespace fs = std::filesystem;

namespace {

constexpr int kIdPick = 1001;
constexpr int kIdCompress = 1002;
constexpr int kIdPathEdit = 1003;
constexpr int kIdStatus = 1004;

struct AppState {
    HWND hwnd_main = nullptr;
    HWND hwnd_path = nullptr;
    HWND hwnd_status = nullptr;
};

std::wstring Utf8ToWide(std::string const& utf8) {
    if (utf8.empty()) {
        return {};
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

std::string WideToUtf8(std::wstring const& ws) {
    if (ws.empty()) {
        return {};
    }
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), n, nullptr, nullptr);
    return out;
}

void SetStatus(AppState* app, wchar_t const* text) {
    if (app && app->hwnd_status) {
        SetWindowTextW(app->hwnd_status, text);
    }
}

bool PickPdfFile(HWND owner, std::wstring& out_path) {
    std::vector<wchar_t> buf(32768, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.lpstrFilter = L"PDF\0*.pdf\0All\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) {
        return false;
    }
    out_path.assign(buf.data());
    return true;
}

std::wstring BuildOutputPathW(fs::path const& input_pdf) {
    auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    fs::path const parent = input_pdf.parent_path();
    std::wstring const stem = input_pdf.stem().wstring();
    fs::path const out = parent / (stem + L"compress" + std::to_wstring(ms) + L".pdf");
    return out.wstring();
}

void CompressPdfUtf8Paths(std::string const& in_utf8, std::string const& out_utf8) {
    QPDF qpdf;
    qpdf.processFile(in_utf8.c_str());

    QPDFWriter writer(qpdf, out_utf8.c_str());
    writer.setStreamDataMode(qpdf_s_compress);
    writer.setObjectStreamMode(qpdf_o_generate);
    writer.write();
}

void OnPick(AppState* app) {
    std::wstring path;
    if (!PickPdfFile(app->hwnd_main, path)) {
        return;
    }
    SetWindowTextW(app->hwnd_path, path.c_str());
    SetStatus(app, L"已选择文件，可点击「开始压缩」。");
}

void OnCompress(AppState* app) {
    int const len = GetWindowTextLengthW(app->hwnd_path);
    if (len <= 0) {
        MessageBoxW(app->hwnd_main, L"请先选择 PDF 文件。", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    std::vector<wchar_t> buf(static_cast<size_t>(len) + 1, L'\0');
    if (GetWindowTextW(app->hwnd_path, buf.data(), static_cast<int>(buf.size())) <= 0) {
        MessageBoxW(app->hwnd_main, L"请先选择 PDF 文件。", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    fs::path const input_w(buf.data());
    if (!fs::is_regular_file(input_w)) {
        MessageBoxW(app->hwnd_main, L"所选路径不是有效的文件。", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    auto ext = input_w.extension().wstring();
    for (auto& ch : ext) {
        ch = static_cast<wchar_t>(std::towlower(static_cast<std::wint_t>(ch)));
    }
    if (ext != L".pdf") {
        MessageBoxW(app->hwnd_main, L"请选择扩展名为 .pdf 的文件。", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring const out_w = BuildOutputPathW(input_w);
    std::string const in_u8 = WideToUtf8(input_w.wstring());
    std::string const out_u8 = WideToUtf8(out_w);

    SetStatus(app, L"正在压缩…");
    EnableWindow(GetDlgItem(app->hwnd_main, kIdPick), FALSE);
    EnableWindow(GetDlgItem(app->hwnd_main, kIdCompress), FALSE);

    try {
        CompressPdfUtf8Paths(in_u8, out_u8);
    } catch (std::exception const& e) {
        EnableWindow(GetDlgItem(app->hwnd_main, kIdPick), TRUE);
        EnableWindow(GetDlgItem(app->hwnd_main, kIdCompress), TRUE);
        SetStatus(app, L"压缩失败。");
        auto const wmsg = Utf8ToWide(e.what());
        MessageBoxW(app->hwnd_main, wmsg.empty() ? L"未知错误。" : wmsg.c_str(), L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    EnableWindow(GetDlgItem(app->hwnd_main, kIdPick), TRUE);
    EnableWindow(GetDlgItem(app->hwnd_main, kIdCompress), TRUE);
    SetStatus(app, L"完成。");
    MessageBoxW(app->hwnd_main, out_w.c_str(), L"已保存", MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto const* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = reinterpret_cast<AppState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->hwnd_main = hwnd;
        HINSTANCE const inst = cs->hInstance;

        CreateWindowExW(
            0,
            L"STATIC",
            L"已选文件：",
            WS_CHILD | WS_VISIBLE,
            12,
            12,
            80,
            20,
            hwnd,
            nullptr,
            inst,
            nullptr);

        app->hwnd_path = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
            12,
            36,
            560,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPathEdit)),
            inst,
            nullptr);

        CreateWindowExW(
            0,
            L"BUTTON",
            L"选择文件…",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            12,
            72,
            120,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPick)),
            inst,
            nullptr);

        CreateWindowExW(
            0,
            L"BUTTON",
            L"开始压缩",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            144,
            72,
            120,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCompress)),
            inst,
            nullptr);

        app->hwnd_status = CreateWindowExW(
            0,
            L"STATIC",
            L"就绪",
            WS_CHILD | WS_VISIBLE,
            12,
            112,
            560,
            20,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatus)),
            inst,
            nullptr);

        HFONT ui_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        EnumChildWindows(
            hwnd,
            [](HWND child, LPARAM font) -> BOOL {
                SendMessageW(child, WM_SETFONT, font, TRUE);
                return TRUE;
            },
            reinterpret_cast<LPARAM>(ui_font));

        return 0;
    }
    case WM_COMMAND: {
        if (!app) {
            break;
        }
        int const id = LOWORD(wParam);
        int const code = HIWORD(wParam);
        if (code == BN_CLICKED) {
            if (id == kIdPick) {
                OnPick(app);
                return 0;
            }
            if (id == kIdCompress) {
                OnCompress(app);
                return 0;
            }
        }
        break;
    }
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 620;
        mmi->ptMinTrackSize.y = 200;
        return 0;
    }
    case WM_SIZE: {
        if (!app || !app->hwnd_path) {
            break;
        }
        int const cx = LOWORD(lParam);
        int const cy = HIWORD(lParam);
        (void)cy;
        MoveWindow(app->hwnd_path, 12, 36, std::max(200, cx - 24), 24, TRUE);
        MoveWindow(app->hwnd_status, 12, 112, std::max(200, cx - 24), 20, TRUE);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int APIENTRY wWinMain(
    _In_ HINSTANCE hInst,
    _In_opt_ HINSTANCE /*hPrev*/,
    _In_ LPWSTR /*cmdLine*/,
    _In_ int /*show*/) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    auto app = std::make_unique<AppState>();

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"PdfCompressWnd";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            MessageBoxW(nullptr, L"窗口类注册失败。", L"错误", MB_OK | MB_ICONERROR);
            return -1;
        }
    }

    HWND hwnd = CreateWindowExW(
        0,
        L"PdfCompressWnd",
        L"PDF 压缩",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        220,
        nullptr,
        nullptr,
        hInst,
        app.get());

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
