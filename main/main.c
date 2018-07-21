#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"

#include <string.h>

#include "odroid_sdcard.h"
#include "odroid_display.h"
#include "input.h"

#include "../components/ugui/ugui.h"


const char* SD_CARD = "/sd";
const char* HEADER = "ODROIDGO_FIRMWARE_V00_00";

#define FIRMWARE_DESCRIPTION_SIZE (40)
char FirmwareDescription[FIRMWARE_DESCRIPTION_SIZE];

uint16_t fb[320 * 240];
UG_GUI gui;
char tempstring[1024];

#define MAX_OBJECTS (20)
#define ITEM_COUNT (10)

UG_WINDOW window1;
UG_TEXTBOX item_textbox[ITEM_COUNT];
UG_TEXTBOX header_textbox;
UG_TEXTBOX footer_textbox;
UG_OBJECT objbuffwnd1[MAX_OBJECTS];



static void indicate_error()
{
    int level = 0;
    while (true) {
        gpio_set_level(GPIO_NUM_2, level);
        level = !level;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

static void pset(UG_S16 x, UG_S16 y, UG_COLOR color)
{
    fb[y * 320 + x] = color;
}

static void window1callback(UG_MESSAGE* msg)
{
}

static void UpdateDisplay()
{
    UG_Update();
    ili9341_write_frame_rectangleLE(0, 0, 320, 240, fb);
}

static void ClearScreen()
{
    // Clear screen
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        uint16_t id = TXB_ID_0 + i;
        UG_TextboxSetForeColor(&window1, id, C_BLACK);
        UG_TextboxSetBackColor(&window1, id, C_WHITE);
        UG_TextboxSetText(&window1, id, "");
    }
}

static void DisplayError(const char* message)
{
    // for (int i = 0; i < ITEM_COUNT; ++i)
    // {
    //     uint16_t id = TXB_ID_0 + i;
    //     UG_TextboxSetForeColor(&window1, id, C_BLACK);
    //     UG_TextboxSetBackColor(&window1, id, C_WHITE);
    //     UG_TextboxSetText(&window1, id, "");
    // }

    uint16_t err_id = TXB_ID_0 + (ITEM_COUNT / 2) - 1;
    UG_TextboxSetForeColor(&window1, err_id, C_RED);
    UG_TextboxSetText(&window1, err_id, message);

    UpdateDisplay();
}

static void DisplayMessage(const char* message)
{
    uint16_t id = TXB_ID_0 + (ITEM_COUNT / 2) - 1;
    UG_TextboxSetForeColor(&window1, id, C_BLACK);
    UG_TextboxSetText(&window1, id, message);

    UpdateDisplay();
}

static void DisplayFooter(const char* message)
{
    uint16_t id = TXB_ID_0 + (ITEM_COUNT) - 1;
    UG_TextboxSetForeColor(&window1, id, C_BLACK);
    UG_TextboxSetText(&window1, id, message);

    UpdateDisplay();
}
//---------------
void boot_application()
{
    printf("Booting application.\n");

    // Set firmware active
    const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (partition != NULL)
    {
        esp_err_t err = esp_ota_set_boot_partition(partition);
        if (err == ESP_OK)
        {
            // reboot
            esp_restart();
        }
    }
}

void flash_firmware(const char* fullPath)
{
    ClearScreen();
    UpdateDisplay();


    printf("Opening file '%s'.\n", fullPath);

    FILE* file = fopen(fullPath, "rb");
    if (file == NULL)
    {
        DisplayError("NO FILE ERROR");
        indicate_error();
    }

    // Check the header
    const size_t headerLength = strlen(HEADER);
    char* header = malloc(headerLength + 1);
    if(!header)
    {
        DisplayError("MEMORY ERROR");
        indicate_error();
    }

    // null terminate
    memset(header, 0, headerLength + 1);

    size_t count = fread(header, 1, headerLength, file);
    if (count != headerLength)
    {
        DisplayError("HEADER READ ERROR");
        indicate_error();
    }

    if (strncmp(HEADER, header, headerLength) != 0)
    {
        DisplayError("HEADER MATCH ERROR");
        indicate_error();
    }

    printf("Header OK: '%s'\n", header);

    // read description
    count = fread(FirmwareDescription, 1, FIRMWARE_DESCRIPTION_SIZE, file);
    if (count != FIRMWARE_DESCRIPTION_SIZE)
    {
        DisplayError("DESCRIPTION READ ERROR");
        indicate_error();
    }

    // ensure null terminated
    FirmwareDescription[FIRMWARE_DESCRIPTION_SIZE - 1] = 0;

    printf("FirmwareDescription='%s'\n", FirmwareDescription);

    //DisplayMessage(FirmwareDescription);
    //DisplayFooter("[Start]");
    UG_TextboxSetText(&window1, TXB_ID_0, FirmwareDescription);
    UpdateDisplay();

    // start to begin, b back
    DisplayMessage("[Start] = OK, [B] = Cancel");
    UpdateDisplay();

    odroid_gamepad_state previousState;
    input_read(&previousState);
    while (true)
    {
        odroid_gamepad_state state;
        input_read(&state);

        if(!previousState.values[ODROID_INPUT_START] && state.values[ODROID_INPUT_START])
        {
            break;
        }
        else if(!previousState.values[ODROID_INPUT_B] && state.values[ODROID_INPUT_B])
        {
            return;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    DisplayMessage("");
    UpdateDisplay();

    DisplayFooter("FLASH START");
    //abort();

    const int ERASE_BLOCK_SIZE = 4096;
    void* data = malloc(ERASE_BLOCK_SIZE);
    if (!data)
    {
        DisplayError("DATA MEMORY ERROR");
        indicate_error();
    }


    // Copy the firmware
    while(true)
    {
        // Partition
        uint32_t slot;
        count = fread(&slot, 1, sizeof(slot), file);
        if (count != sizeof(slot))
        {
            break;
        }

        const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
            ESP_PARTITION_SUBTYPE_APP_OTA_MIN + slot, NULL);
        if (part==0)
        {
             printf("esp_partition_find_first failed. slot=%d\n", slot);
             DisplayError("PARTITION ERROR");
             indicate_error();
        }


        // Length
        uint32_t length;
        count = fread(&length, 1, sizeof(length), file);
        if (count != sizeof(length))
        {
            DisplayError("LENGTH READ ERROR");
            indicate_error();
        }

        size_t nextEntry = ftell(file) + length;


        // turn LED off
        gpio_set_level(GPIO_NUM_2, 0);


        // erase
        int eraseBlocks = length / ERASE_BLOCK_SIZE;
        if (eraseBlocks * ERASE_BLOCK_SIZE < length) ++eraseBlocks;

        // Display
        sprintf(tempstring, "ERASE: [%d] BLOCKS=%#04x", slot, eraseBlocks);

        printf("%s\n", tempstring);
        DisplayMessage(tempstring);

        esp_err_t ret = esp_partition_erase_range(part, 0, eraseBlocks * ERASE_BLOCK_SIZE);
        if (ret != ESP_OK)
        {
            printf("esp_partition_erase_range failed. eraseBlocks=%d\n", eraseBlocks);
            DisplayError("ERASE ERROR");
            indicate_error();
        }

        // turn LED on
        gpio_set_level(GPIO_NUM_2, 1);


        // Write data
        int totalCount = 0;
        for (int offset = 0; offset < length; offset += ERASE_BLOCK_SIZE)
        {
            // Display
            sprintf(tempstring, "WRITE: [%d] %#08x", slot, offset);

            printf("%s\n", tempstring);
            DisplayMessage(tempstring);


            // read
            //printf("Reading offset=0x%x\n", offset);
            count = fread(data, 1, ERASE_BLOCK_SIZE, file);
            if (count <= 0)
            {
                DisplayError("DATA READ ERROR");
                indicate_error();
            }

            if (offset + count >= length)
            {
                count = length - offset;
            }


            // flash
            //printf("Writing offset=0x%x\n", offset);
            ret = esp_partition_write(part, offset, data, count);
            if (ret != ESP_OK)
    		{
    			printf("esp_partition_write failed.\n");
                DisplayError("WRITE ERROR");
                indicate_error();
    		}

            totalCount += count;
        }

        if (totalCount != length)
        {
            printf("Size mismatch: lenght=%#08x, totalCount=%#08x\n", length, totalCount);
            DisplayError("DATA SIZE ERROR");
            indicate_error();
        }


        // TODO: verify




        // Notify OK
        sprintf(tempstring, "OK: [%d] Length=%#08x", slot, length);

        printf("%s\n", tempstring);
        DisplayFooter(tempstring);


        // Seek to next entry
        if (fseek(file, nextEntry, SEEK_SET) != 0)
        {
            DisplayError("SEEK ERROR");
            indicate_error();
        }
    }


    free(data);

    // turn LED off
    gpio_set_level(GPIO_NUM_2, 0);

    // clear framebuffer
    ili9341_clear(0x0000);

    // boot firmware
    boot_application();

    indicate_error();
}

void DrawPage(char** files, int fileCount, int currentItem)
{
    int page = currentItem / ITEM_COUNT;
    page *= ITEM_COUNT;

    // Reset all text boxes
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        uint16_t id = TXB_ID_0 + i;
        UG_TextboxSetText(&window1, id, "");
    }

	if (fileCount < 1)
	{
		const char* text = "(empty)";

        uint16_t id = TXB_ID_0 + (ITEM_COUNT / 2);
        UG_TextboxSetText(&window1, id, (char*)text);

        UpdateDisplay();
	}
	else
	{
        char* displayStrings[ITEM_COUNT];
        for(int i = 0; i < ITEM_COUNT; ++i)
        {
            displayStrings[i] = NULL;
        }

	    for (int line = 0; line < ITEM_COUNT; ++line)
	    {
			if (page + line >= fileCount) break;

            uint16_t id = TXB_ID_0 + line;

	        if ((page) + line == currentItem)
	        {
	            UG_TextboxSetForeColor(&window1, id, C_BLACK);
                UG_TextboxSetBackColor(&window1, id, C_YELLOW);
	        }
	        else
	        {
	            UG_TextboxSetForeColor(&window1, id, C_BLACK);
                UG_TextboxSetBackColor(&window1, id, C_WHITE);
	        }

			char* fileName = files[page + line];
			if (!fileName) abort();

			displayStrings[line] = (char*)malloc(strlen(fileName) + 1);
            strcpy(displayStrings[line], fileName);
            displayStrings[line][strlen(fileName) - 3] = 0; // ".fw" = 3

	        UG_TextboxSetText(&window1, id, displayStrings[line]);
	    }

        UpdateDisplay();

        for(int i = 0; i < ITEM_COUNT; ++i)
        {
            free(displayStrings[i]);
        }
	}


}

char** files;
int fileCount;

static const char* menu_choose_file(const char* path)
{
    const char* result = NULL;

    fileCount = odroid_sdcard_files_get(path, ".fw", &files);
    printf("%s: fileCount=%d\n", __func__, fileCount);

    // At least one firmware must be available
    if (fileCount < 1)
    {
        uint16_t err_id = TXB_ID_0 + (ITEM_COUNT / 2) - 1;
        UG_TextboxSetForeColor(&window1, err_id, C_RED);
        UG_TextboxSetText(&window1, err_id, "NO FIRMWARE ERROR");
        UpdateDisplay();

        indicate_error();
    }


    // Selection
    int currentItem = 0;
    DrawPage(files, fileCount, currentItem);

    odroid_gamepad_state previousState;
    input_read(&previousState);

    while (true)
    {
		odroid_gamepad_state state;
		input_read(&state);

        int page = currentItem / 10;
        page *= 10;

		if (fileCount > 0)
		{
	        if(!previousState.values[ODROID_INPUT_DOWN] && state.values[ODROID_INPUT_DOWN])
	        {
	            if (fileCount > 0)
				{
					if (currentItem + 1 < fileCount)
		            {
		                ++currentItem;
		                DrawPage(files, fileCount, currentItem);
		            }
					else
					{
						currentItem = 0;
		                DrawPage(files, fileCount, currentItem);
					}
				}
	        }
	        else if(!previousState.values[ODROID_INPUT_UP] && state.values[ODROID_INPUT_UP])
	        {
	            if (fileCount > 0)
				{
					if (currentItem > 0)
		            {
		                --currentItem;
		                DrawPage(files, fileCount, currentItem);
		            }
					else
					{
						currentItem = fileCount - 1;
						DrawPage(files, fileCount, currentItem);
					}
				}
	        }
	        else if(!previousState.values[ODROID_INPUT_RIGHT] && state.values[ODROID_INPUT_RIGHT])
	        {
	            if (fileCount > 0)
				{
					if (page + 10 < fileCount)
		            {
		                currentItem = page + 10;
		                DrawPage(files, fileCount, currentItem);
		            }
					else
					{
						currentItem = 0;
						DrawPage(files, fileCount, currentItem);
					}
				}
	        }
	        else if(!previousState.values[ODROID_INPUT_LEFT] && state.values[ODROID_INPUT_LEFT])
	        {
	            if (fileCount > 0)
				{
					if (page - 10 >= 0)
		            {
		                currentItem = page - 10;
		                DrawPage(files, fileCount, currentItem);
		            }
					else
					{
						currentItem = page;
						while (currentItem + 10 < fileCount)
						{
							currentItem += 10;
						}

		                DrawPage(files, fileCount, currentItem);
					}
				}
	        }
	        else if(!previousState.values[ODROID_INPUT_A] && state.values[ODROID_INPUT_A])
	        {
	            size_t fullPathLength = strlen(path) + 1 + strlen(files[currentItem]) + 1;

	            char* fullPath = (char*)malloc(fullPathLength);
	            if (!fullPath) abort();

	            strcpy(fullPath, path);
	            strcat(fullPath, "/");
	            strcat(fullPath, files[currentItem]);

	            result = fullPath;
                break;
	        }
            else if (!previousState.values[ODROID_INPUT_MENU] && state.values[ODROID_INPUT_MENU])
            {
                ClearScreen();
                DisplayMessage("Exiting ...");
                UpdateDisplay();

                boot_application();

                // should not reach
                abort();
            }
		}

        previousState = state;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    odroid_sdcard_files_free(files, fileCount);

    return result;
}

static void menu_main()
{
    sprintf(tempstring,"Ver: %s-%s", COMPILEDATE, GITREV);

    UG_WindowCreate(&window1, objbuffwnd1, MAX_OBJECTS, window1callback);

    UG_WindowSetTitleText(&window1, "ODROID-GO");
    UG_WindowSetTitleTextFont(&window1, &FONT_10X16);
    UG_WindowSetTitleTextAlignment(&window1, ALIGN_CENTER);

    UG_S16 innerWidth = UG_WindowGetInnerWidth(&window1);
    UG_S16 innerHeight = UG_WindowGetInnerHeight(&window1);
    UG_S16 titleHeight = UG_WindowGetTitleHeight(&window1);
    float textHeight = (float)(innerHeight) / (ITEM_COUNT + 2);

    //UG_U16 header_id = TXB_ID_0 + ITEM_COUNT;
    // UG_TextboxCreate(&window1, &header_textbox, header_id,
    //     0, 0,
    //     innerWidth, textHeight - 1);
    // UG_TextboxSetBackColor(&window1, header_id, C_GREEN);
    // UG_TextboxSetFont(&window1, header_id, &FONT_8X8);
    // UG_TextboxSetText(&window1, header_id, "Header Line");

    UG_U16 footer_id = TXB_ID_0 + ITEM_COUNT + 1;
    UG_S16 footer_top = innerHeight - textHeight;
    UG_TextboxCreate(&window1, &footer_textbox, footer_id,
        0, footer_top,
        innerWidth, footer_top + textHeight - 1);
    UG_TextboxSetFont(&window1, footer_id, &FONT_8X8);
    UG_TextboxSetForeColor(&window1, footer_id, C_DARK_GRAY);
    UG_TextboxSetBackColor(&window1, footer_id, C_BLACK);
    UG_TextboxSetAlignment(&window1, footer_id, ALIGN_CENTER);
    UG_TextboxSetText(&window1, footer_id, tempstring);


    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        uint16_t id = TXB_ID_0 + i;
        UG_S16 top = (i) * textHeight + (textHeight / 2);
        UG_TextboxCreate(&window1, &item_textbox[i], id, 0, top, innerWidth, top + textHeight - 1);
        UG_TextboxSetFont(&window1, id, &FONT_8X12);
        //UG_TextboxSetForeColor(&window1, id, C_WHITE);
        UG_TextboxSetAlignment(&window1, id, ALIGN_CENTER);
        UG_TextboxSetText(&window1, id, "");
    }


    UG_WindowShow(&window1);
    UpdateDisplay();


    // Check SD card
    esp_err_t ret = odroid_sdcard_open(SD_CARD);
    if (ret != ESP_OK)
    {
        DisplayError("SD CARD ERROR");
        indicate_error();
    }


    // Check for /odroid/firmware
    const char* path = "/sd/odroid/firmware";

    while(1)
    {
        const char* fileName = menu_choose_file(path);
        if (!fileName) abort();

        printf("%s: fileName='%s'\n", __func__, fileName);

        flash_firmware(fileName);
    }

    indicate_error();
}


void app_main(void)
{
    nvs_flash_init();

    input_init();


    // turn LED on
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_2, 1);


    ili9341_init();
    ili9341_clear(0x0000);

    UG_Init(&gui, pset, 320, 240);
    menu_main();


    while(1)
    {
        vTaskDelay(1);
    }

    indicate_error();
}
