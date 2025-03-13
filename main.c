#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "user32")
#pragma comment(lib, "shell32")
#pragma comment(lib, "shlwapi")
#pragma comment(lib, "advapi32")
#pragma comment(lib, "libmupdf")

#include <windows.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <winreg.h>
#include <winuser.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include "pdf_drop_seals.h"
#include "open_source.h"

#define BUFSIZE 4096
#define KEYSIZE 128
// #define assert(cond) do { if (!(cond)) __debugbreak(); } while (0)

char g_subkey_file[KEYSIZE] = "Software\\Classes\\SystemFileAssociations\\.pdf\\shell\\";
char g_fixed_exe_path[MAX_PATH] = { 0 };

void get_fixed_exe_path()
{
    char* appdata_path = getenv("APPDATA");
    strcpy(g_fixed_exe_path, appdata_path);
    strcat(g_fixed_exe_path, "\\pdf-seals-deleter\\pdf-seals-deleter.exe");
}

void copy_to_fixed_location(char* src_path)
{
    char dir_path[MAX_PATH];
    strcpy(dir_path, g_fixed_exe_path);
    PathRemoveFileSpecA(dir_path);

    // First recursively delete the target directory if it exists
    if (PathFileExistsA(dir_path)) {
        // Prepare SHFILEOPSTRUCT for deletion
        char del_path[MAX_PATH + 2]; // +2 for double null termination
        strcpy(del_path, dir_path);
        del_path[strlen(dir_path) + 1] = 0; // Double null terminate
        
        SHFILEOPSTRUCTA file_op = {
            NULL,
            FO_DELETE,
            del_path,
            NULL,
            FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
            FALSE,
            NULL,
            NULL
        };
        
        // Perform the deletion
        SHFileOperationA(&file_op);
    }

    // Create the directory and copy the file
    CreateDirectoryA(dir_path, NULL);
    CopyFileA(src_path, g_fixed_exe_path, FALSE);
}

void add_context_menu(char* key_name, char* menu_name, char* subkey)
{
    HKEY key;

    char subkey_tmp[KEYSIZE];
    strcpy(subkey_tmp, subkey);

    // Ensure HKEY_CURRENT_USER\SOFTWARE\Classes\.pdf\shell exist
    DWORD disposition;
    RegCreateKeyExA(HKEY_CURRENT_USER, subkey_tmp, 0, NULL, 
                    REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &disposition);
    RegCloseKey(key);

    // Create key
    strcat(subkey_tmp, key_name);
    assert(RegCreateKeyA(HKEY_CURRENT_USER, subkey_tmp, &key) == ERROR_SUCCESS);

    // Convert menu name to system encoding (for none-utf8-encode system)
    int menu_name_len = MultiByteToWideChar(CP_UTF8, 0, menu_name, -1, NULL, 0);
    wchar_t* menu_name_wide = (wchar_t*)malloc(menu_name_len * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, menu_name, -1, menu_name_wide, menu_name_len);

    int menu_name_ansi_len = WideCharToMultiByte(CP_ACP, 0, menu_name_wide, -1, NULL, 0, NULL, NULL);
    char* menu_name_ansi = (char*)malloc(menu_name_ansi_len);
    WideCharToMultiByte(CP_ACP, 0, menu_name_wide, -1, menu_name_ansi, menu_name_ansi_len, NULL, NULL);

    // Set menu name with system encoding
    RegSetValueExA(key, NULL, 0, REG_SZ, (BYTE*)menu_name_ansi, (DWORD)(strlen(menu_name_ansi) + 1));

    free(menu_name_wide);
    free(menu_name_ansi);

    // Set the icon
    RegSetValueExA(key, "Icon", 0, REG_SZ, (BYTE*)g_fixed_exe_path, (DWORD)(strlen(g_fixed_exe_path) + 1));
    RegCloseKey(key);

    // Create sub command key
    char subkey_command[KEYSIZE + 8]; // the length of "\command" is 8
    strcpy(subkey_command, subkey_tmp);
    strcat(subkey_command, "\\command");
    assert(RegCreateKeyA(HKEY_CURRENT_USER, subkey_command, &key) == ERROR_SUCCESS);

    // Set sub command key value
    char command[MAX_PATH + 7] = "\""; // the length of "\"\" \"%%1\"" is 7
    strcat(command, g_fixed_exe_path);
    strcat(command, "\" \"%1\"");
    RegSetValueExA(key, NULL, 0, REG_SZ, (BYTE*)command, (DWORD)(strlen(command) + 1));
    RegCloseKey(key);
}

void remove_context_menu(char* key_name, char* subkey) 
{
    char subkey_tmp[KEYSIZE];
    strcpy(subkey_tmp, subkey);
    strcat(subkey_tmp, key_name);
    char subkey_command[KEYSIZE + 8]; // the length of "\command" is 8
    strcpy(subkey_command, subkey_tmp);
    strcat(subkey_command, "\\command");

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, subkey_tmp, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        assert(RegDeleteKeyA(HKEY_CURRENT_USER, subkey_command) == ERROR_SUCCESS);
        assert(RegDeleteKeyA(HKEY_CURRENT_USER, subkey_tmp) == ERROR_SUCCESS);
    }
}

char* get_path_with_logo(char* input)
{
    char* pdf_pos = strstr(input, ".pdf");
    int prefix_len = (int)(pdf_pos - input);
    int new_len = prefix_len + (int)strlen("_without_seals.pdf") + 1;

    char* output = (char*)malloc(new_len * sizeof(char));

    strncpy(output, input, prefix_len);
    output[prefix_len] = '\0';
    strcat(output, "_without_seals.pdf");

    return output;
}

void pop_message_box()
{
    // Popup a message box to require user to confirm if kill these processes
    LANGID langID = GetUserDefaultUILanguage();
    int result = (PRIMARYLANGID(langID) == LANG_CHINESE) ?
                 MessageBoxW(NULL, 
                     L"本软件可为文件资源管理器上下文菜单提供一条命令，用于消除 PDF 文件水印。"
                     L"用户可通过右键 PDF 文件选中相关命令以在当前文件夹生成无水印 PDF 文件"
                     L"（<原文件名>_without_seals.pdf）。\n"
                     L"---------------\n"
                     L"本软件原理为提取原 PDF 文件中所有图片，筛选掉其中宽度大于高度的图片，"
                     L"最后拼接生成新 PDF 文件。因此只支持消除此对应逻辑生成的水印。\n"
                     L"---------------\n"
                     L"选项说明：\n"
                     L"    是（YES）：添加/更新上下文菜单；\n"
                     L"    否（NO）：删除上下文菜单；\n"
                     L"    取消（Cancel）：不作为，直接退出。\n"
                     L"---------------\n"
                     L"命令行以参数 `--source` 运行查看依赖、许可证以及源码。",
                     L"使用说明", MB_YESNOCANCEL | MB_TOPMOST) : 
                 MessageBoxW(NULL, 
                     L"This software provides a command for the file explorer context menu to remove watermarks from PDF files. "
                     L"Users can right-click on a PDF file and select the relevant command to generate a watermark-free PDF file "
                     L"(<OriginalFileName>_without_seals.pdf) in the current folder.\n"
                     L"---------------\n"
                     L"The principle of this software is to extract all images from the original PDF file, filter out images "
                     L"where the width is greater than the height, and finally merge them into a new PDF file. Therefore, it only "
                     L"supports removing watermarks generated by this specific logic.\n"
                     L"---------------\n"
                     L"Options description:\n"
                     L"    Yes (YES): Add/Update context menu;\n"
                     L"    No (NO): Remove context menu;\n"
                     L"    Cancel: Do nothing and exit.\n"
                     L"---------------\n"
                     L"Run the command line with the parameter --source to view dependencies, licenses, and source code.",
                     L"Usage Instructions", MB_YESNOCANCEL | MB_TOPMOST);
    if (result == IDYES) {
        // Get current exe path and copy to fixed location
        char exe_path[MAX_PATH];
        GetModuleFileNameA(NULL, exe_path, MAX_PATH);
        copy_to_fixed_location(exe_path);
        // Add fixed exe path to context menu
        char* menu_text = (PRIMARYLANGID(langID) == LANG_CHINESE) ? "消除水印" : "Remove seals";
        add_context_menu("pdf-seals-deleter", menu_text, g_subkey_file);
    } else if (result == IDNO) {
        // Delete exe of fixed location
        if (PathFileExistsA(g_fixed_exe_path)) {
            DeleteFileA(g_fixed_exe_path);
            // Delete parent directory (if is empty)
            char dir_path[MAX_PATH];
            strcpy(dir_path, g_fixed_exe_path);
            PathRemoveFileSpecA(dir_path);
            RemoveDirectoryA(dir_path);
        }
        // Remove context menu
        remove_context_menu("pdf-seals-deleter", g_subkey_file);
    }
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // Set DPI awareness for better scaling on high DPI displays (Windows 10, v1607)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Register window class
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"PDF Seals Deleter";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));

    RegisterClassW(&wc);

    // Get fixed exe path (fill g_fixed_exe_path)
    get_fixed_exe_path();

    // If run by context menu, delete seals
    if (*lpCmdLine) {
        char cmd_line[MAX_PATH] = { 0 };
        WideCharToMultiByte(CP_UTF8, 0, lpCmdLine, -1, cmd_line, sizeof(cmd_line), NULL, NULL);
        if (strcmp(cmd_line, "--source") == 0) {
            extract_source("pdf_seals_deleter_source.zip");
        } else {
            cmd_line[strlen(cmd_line)-1] = '\0';
            char* pdf_path = cmd_line+1;
            char* output_path = get_path_with_logo(pdf_path);
            pdf_drop_seals(pdf_path, output_path);
        }
    } else {
        pop_message_box();
    }

    ExitProcess(0);

    return 0;
}
