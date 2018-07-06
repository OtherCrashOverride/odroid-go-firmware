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


const char* SD_CARD = "/sd";
const char* FIRMWARE = "/sd/firmware.bin";
const char* HEADER = "ODROIDGO_FIRMWARE_V00_00";

#define FIRMWARE_DESCRIPTION_SIZE (40)
char FirmwareDescription[FIRMWARE_DESCRIPTION_SIZE];


void indicate_error()
{
    int level = 0;
    while (true) {
        gpio_set_level(GPIO_NUM_2, level);
        level = !level;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    nvs_flash_init();

    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

    // turn LED on
    gpio_set_level(GPIO_NUM_2, 1);


    // look for firmware
    esp_err_t ret = odroid_sdcard_open(SD_CARD);
    if (ret == ESP_OK)
    {
        printf("Opening file '%s'.\n", FIRMWARE);

        FILE* file = fopen(FIRMWARE, "rb");
        if (file != NULL)
        {
            // Check the header
            const size_t headerLength = strlen(HEADER);
            char* header = malloc(headerLength + 1);
            if(header)
            {
                // null terminate
                memset(header, 0, headerLength + 1);

                size_t count = fread(header, 1, headerLength, file);
                if (count == headerLength)
                {
                    if (strncmp(HEADER, header, headerLength) == 0)
                    {
                        printf("Header OK: '%s'\n", header);

                        // read description
                        count = fread(FirmwareDescription, 1, FIRMWARE_DESCRIPTION_SIZE, file);
                        if (count == FIRMWARE_DESCRIPTION_SIZE)
                        {
                            // ensure null terminated
                            FirmwareDescription[FIRMWARE_DESCRIPTION_SIZE - 1] = 0;

                            printf("FirmwareDescription='%s", FirmwareDescription);

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
                                    errorFlag = true;
                                    break;
                                }

                                void* data = heap_caps_malloc(length, MALLOC_CAP_SPIRAM);
                                if (!data)
                                {
                                    errorFlag = true;
                                    break;
                                }

                                count = fread(data, 1, length, file);
                                if (count != length)
                                {
                                    errorFlag = true;
                                    break;
                                }

                                const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                    ESP_PARTITION_SUBTYPE_APP_OTA_MIN + slot, NULL);
                                if (part==0)
                                {
                                     printf("esp_partition_find_first failed. slot=%d\n", slot);
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
                                    errorFlag = true;
                                    break;
                        		}

                                // TODO: verify
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
                        }
                    }
                }
            }
        }
    }

    indicate_error();
}
