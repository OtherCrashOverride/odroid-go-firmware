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

#include "../components/ugui/ugui.h"


const char* SD_CARD = "/sd";
const char* FIRMWARE = "/sd/firmware.bin";
const char* HEADER = "ODROIDGO_FIRMWARE_V00_00";

#define FIRMWARE_DESCRIPTION_SIZE (40)
char FirmwareDescription[FIRMWARE_DESCRIPTION_SIZE];

uint16_t fb[320 * 240];
UG_GUI gui;
char tempstring[1024];


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
    if (msg->type == MSG_TYPE_OBJECT)
    {
        if (msg->id == OBJ_TYPE_BUTTON)
        {
            switch (msg->sub_id)
            {
                case BTN_ID_0:
                {
                    // . . .
                    break ;
                }
            }
        }
    }
}


#define MAX_OBJECTS 10

UG_WINDOW window1;
UG_BUTTON button1;
UG_TEXTBOX textbox1;
UG_TEXTBOX header_textbox;
UG_TEXTBOX footer_textbox;
UG_OBJECT objbuffwnd1 [MAX_OBJECTS];


static void UpdateDisplay()
{
    UG_Update();
    ili9341_write_frame_rectangleLE(0, 0, 320, 240, fb);
}

static void DisplayError(const char* message)
{
    UG_TextboxSetForeColor(&window1, TXB_ID_0, C_RED);
    UG_TextboxSetText(&window1, TXB_ID_0, message);

    UpdateDisplay();
}

static void DisplayMessage(const char* message)
{
    UG_TextboxSetForeColor(&window1, TXB_ID_0, C_BLACK);
    UG_TextboxSetText(&window1, TXB_ID_0, message);

    UpdateDisplay();
}

static void DisplayFooter(const char* message)
{
    UG_TextboxSetText(&window1, TXB_ID_2, message);
    UpdateDisplay();
}

void app_main(void)
{
    nvs_flash_init();

    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_39, GPIO_MODE_INPUT);

    // turn LED on
    gpio_set_level(GPIO_NUM_2, 1);


    ili9341_init();
    ili9341_clear(0x0000);

    sprintf(tempstring,"ver: %s-%s", COMPILEDATE, GITREV);

    UG_Init(&gui, pset, 320, 240);

    UG_WindowCreate(&window1, objbuffwnd1, MAX_OBJECTS, window1callback);

    UG_WindowSetTitleText(&window1, "ODROID-GO Firmware");
    UG_WindowSetTitleTextFont(&window1, &FONT_10X16);
    UG_WindowSetTitleTextAlignment(&window1, ALIGN_CENTER);

    UG_S16 width = UG_WindowGetInnerWidth(&window1);
    UG_S16 height = UG_WindowGetInnerHeight(&window1);
    UG_S16 titleHeight = UG_WindowGetTitleHeight(&window1);
    UG_S16 textHeight = (height) / 3;
    UG_S16 top = 0;

    UG_TextboxCreate(&window1, &header_textbox, TXB_ID_1, 0, top, width, top + textHeight - 1);
    UG_TextboxSetFont(&window1, TXB_ID_1, &FONT_8X12);
    UG_TextboxSetForeColor(&window1, TXB_ID_1, C_BLACK);
    UG_TextboxSetAlignment(&window1, TXB_ID_1, ALIGN_TOP_CENTER);
    UG_TextboxSetText(&window1, TXB_ID_1, tempstring);

    top += textHeight;

    UG_TextboxCreate(&window1, &textbox1, TXB_ID_0, 0, top, width, top + textHeight - 1);
	UG_TextboxSetFont(&window1, TXB_ID_0, &FONT_16X26);
	UG_TextboxSetText(&window1, TXB_ID_0, "Loading ...");
	UG_TextboxSetForeColor(&window1, TXB_ID_0, C_BLACK);
    UG_TextboxSetAlignment(&window1, TXB_ID_0, ALIGN_TOP_CENTER);

    top += textHeight;

    UG_TextboxCreate(&window1, &footer_textbox, TXB_ID_2, 0, top, width, top + textHeight - 1);
    UG_TextboxSetFont(&window1, TXB_ID_2, &FONT_12X16);
    UG_TextboxSetForeColor(&window1, TXB_ID_2, C_BLACK);
    UG_TextboxSetAlignment(&window1, TXB_ID_2, ALIGN_CENTER);
    //UG_TextboxSetText(&window1, TXB_ID_2, "TEST FOOTER");

    UG_WindowShow(&window1);
    UpdateDisplay();


// ---

    // look for firmware
    esp_err_t ret = odroid_sdcard_open(SD_CARD);
    if (ret != ESP_OK)
    {
        DisplayError("SD CARD ERROR");
        indicate_error();
    }

    printf("Opening file '%s'.\n", FIRMWARE);

    FILE* file = fopen(FIRMWARE, "rb");
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

    DisplayMessage(FirmwareDescription);
    DisplayFooter("[Start]");

    while(gpio_get_level(GPIO_NUM_39))
    {
        vTaskDelay(1);
    }

    DisplayFooter("FLASH START");


    //indicate_error();


    // Copy the firmware
    bool errorFlag = false;

    while(true)
    {
        uint32_t slot;
        count = fread(&slot, 1, sizeof(slot), file);
        if (count != sizeof(slot))
        {
            //errorFlag = true;
            break;
        }

        uint32_t length;
        count = fread(&length, 1, sizeof(length), file);
        if (count != sizeof(length))
        {
            DisplayError("LENGTH READ ERROR");
            errorFlag = true;
            break;
        }

        void* data = heap_caps_malloc(length, MALLOC_CAP_SPIRAM);
        if (!data)
        {
            DisplayError("DATA MEMORY ERROR");
            errorFlag = true;
            break;
        }

        count = fread(data, 1, length, file);
        if (count != length)
        {
            DisplayError("DATA READ ERROR");
            errorFlag = true;
            break;
        }

        const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
            ESP_PARTITION_SUBTYPE_APP_OTA_MIN + slot, NULL);
        if (part==0)
        {
             printf("esp_partition_find_first failed. slot=%d\n", slot);
             DisplayError("PARTITION ERROR");
             errorFlag = true;
             break;
        }

        // turn LED off
        gpio_set_level(GPIO_NUM_2, 0);

        // erase
        const int ERASE_BLOCK_SIZE = 4096;;
    	int eraseBlocks = length / ERASE_BLOCK_SIZE;
    	if (eraseBlocks * ERASE_BLOCK_SIZE < length) ++eraseBlocks;
        esp_err_t ret = esp_partition_erase_range(part, 0, eraseBlocks * ERASE_BLOCK_SIZE);
		if (ret != ESP_OK)
		{
			printf("esp_partition_erase_range failed. eraseBlocks=%d\n", eraseBlocks);
            DisplayError("ERASE ERROR");
            errorFlag = true;
            break;
		}

        // turn LED on
        gpio_set_level(GPIO_NUM_2, 1);

        // flash
        ret = esp_partition_write(part, 0, data, length);
        if (ret != ESP_OK)
		{
			printf("esp_partition_write failed.\n");
            DisplayError("WRITE ERROR");
            errorFlag = true;
            break;
		}

        // TODO: verify

        free(data);

        sprintf(tempstring, "OK: [%d] Length=%d", slot, length);

        printf("%s\n", tempstring);
        DisplayFooter(tempstring);
    }

    if (!errorFlag)
    {
        printf("Rebooting.\n");

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

    indicate_error();
}
