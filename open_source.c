#include <windows.h>
#include <stdio.h>
#include "open_source.h"

void extract_source(char* zip_name) {
    HRSRC res = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_SOURCECODE), (LPCWSTR)RT_RCDATA);
    if (!res) return;

    HGLOBAL h_data = LoadResource(NULL, res);
    if (!h_data) return;

    DWORD size = SizeofResource(NULL, res);
    void *data = LockResource(h_data);

    FILE *file = fopen(zip_name, "wb");
    if (file) {
        fwrite(data, 1, size, file);
        fclose(file);
        LANGID lang_id = GetUserDefaultUILanguage();
        wchar_t message[256];
        if (PRIMARYLANGID(lang_id) == LANG_CHINESE) {
            swprintf(message, 256, L"已生成源代码：%hs", zip_name);
            MessageBoxW(NULL, message, L"提示", MB_OK);
        } else {
            swprintf(message, 256, L"Had extracted source: %hs", zip_name);
            MessageBoxW(NULL, message, L"Info", MB_OK);
        }
    }
}
