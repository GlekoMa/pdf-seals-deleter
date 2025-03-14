#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>

#define KEYSIZE 128
#define assert(cond) do { if (!(cond)) __debugbreak(); } while (0)

void add_context_menu(char* key_name, char* menu_name, char* subkey, char* fixed_exe_path)
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
    RegSetValueExA(key, "Icon", 0, REG_SZ, (BYTE*)fixed_exe_path, (DWORD)(strlen(fixed_exe_path) + 1));
    RegCloseKey(key);

    // Create sub command key
    char subkey_command[KEYSIZE + 8]; // the length of "\command" is 8
    strcpy(subkey_command, subkey_tmp);
    strcat(subkey_command, "\\command");
    assert(RegCreateKeyA(HKEY_CURRENT_USER, subkey_command, &key) == ERROR_SUCCESS);

    // Set sub command key value
    char command[MAX_PATH + 7] = "\""; // the length of "\"\" \"%%1\"" is 7
    strcat(command, fixed_exe_path);
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

void get_fixed_exe_path(char* fixed_exe_name, char* fixed_exe_path)
{
    char* appdata_path = getenv("APPDATA");
    strcpy(fixed_exe_path, appdata_path);
    char fixed_exe_path_suffix[256] = { 0 };
    sprintf(fixed_exe_path_suffix, "\\%s\\%s.exe", fixed_exe_name, fixed_exe_name);
    strcat(fixed_exe_path, fixed_exe_path_suffix);
}

void copy_to_fixed_location(char* src_path, char* fixed_exe_path)
{
    char dir_path[MAX_PATH];
    strcpy(dir_path, fixed_exe_path);
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
    CopyFileA(src_path, fixed_exe_path, FALSE);
}
