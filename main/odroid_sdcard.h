#pragma once

#include "esp_err.h"

int odroid_sdcard_files_get(const char* path, const char* extension, char*** filesOut);
void odroid_sdcard_files_free(char** files, int count);
esp_err_t odroid_sdcard_open();
esp_err_t odroid_sdcard_close();
size_t odroid_sdcard_get_filesize(const char* path);
size_t odroid_sdcard_copy_file_to_memory(const char* path, void* ptr);
