#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


typedef struct {
    uint32_t offset;
    uint32_t size;
} esp_partition_pos_t;

/* Structure which describes the layout of partition table entry.
 * See docs/partition_tables.rst for more information about individual fields.
 */
typedef struct {
	uint16_t magic;
	uint8_t  type;
    uint8_t  subtype;
    esp_partition_pos_t pos;
	uint8_t  label[16];
    uint32_t flags;
} esp_partition_info_t;

//#define ESP_PARTITION_TABLE_OFFSET CONFIG_PARTITION_TABLE_OFFSET /* Offset of partition table. Backwards-compatible name.*/
#define ESP_PARTITION_TABLE_MAX_LEN 0xC00 /* Maximum length of partition table data */
#define ESP_PARTITION_TABLE_MAX_ENTRIES (ESP_PARTITION_TABLE_MAX_LEN / sizeof(esp_partition_info_t)) /* Maximum length of partition table data, including terminating entry */
#define ESP_PARTITION_TABLE_OFFSET  0x8000

#define ESP_PARTITION_MAGIC 0x50AA
#define ESP_PARTITION_MAGIC_MD5 0xEBEB

#define PART_TYPE_APP 0x00
#define PART_SUBTYPE_FACTORY  0x00
#define PART_SUBTYPE_OTA_FLAG 0x10
#define PART_SUBTYPE_OTA_MASK 0x0f
#define PART_SUBTYPE_TEST     0x20

#define PART_TYPE_DATA 0x01
#define PART_SUBTYPE_DATA_OTA 0x00
#define PART_SUBTYPE_DATA_RF  0x01
#define PART_SUBTYPE_DATA_WIFI 0x02

#define PART_TYPE_END 0xff
#define PART_SUBTYPE_END 0xff


const esp_partition_info_t* partition_data;
const char* filename;


static void load_partitions(FILE* fp)
{
    partition_data = (const esp_partition_info_t*)malloc(ESP_PARTITION_TABLE_MAX_LEN);
    if (!partition_data) abort();

    fseek(fp, ESP_PARTITION_TABLE_OFFSET, SEEK_SET);
    size_t count = fread((esp_partition_info_t*)partition_data, 1, ESP_PARTITION_TABLE_MAX_LEN, fp);
    if (count != ESP_PARTITION_TABLE_MAX_LEN) abort();
}

static void print_partitions()
{
    for (int i = 0; i < ESP_PARTITION_TABLE_MAX_ENTRIES; ++i)
    {
        const esp_partition_info_t *part = &partition_data[i];
        if (part->magic == 0xffff ||
            part->magic == ESP_PARTITION_MAGIC_MD5)
        {
            break;
        }

        printf("partition %d:\n", i);

        printf("\tmagic=%#06x\n", part->magic);
        printf("\ttype=%#04x\n", part->type);
        printf("\tsubtype=%#04x\n", part->subtype);
        printf("\t[pos.offset=%#010x, pos.size=%#010x]\n", part->pos.offset, part->pos.size);
        printf("\tlabel='%-16s'\n", part->label);
        printf("\tflags=%#010x\n", part->flags);
        printf("\n");
    }
}

static void extract_partitions(FILE* fp)
{
    const size_t BLOCK_SIZE = 4096;


    printf("\n");

    uint32_t data_end = 0;
    const esp_partition_info_t * rf_part = NULL;

    for (int i = 0; i < ESP_PARTITION_TABLE_MAX_ENTRIES; ++i)
    {
        const esp_partition_info_t *part = &partition_data[i];
        if (part->magic == 0xffff ||
            part->magic == ESP_PARTITION_MAGIC_MD5)
        {
            break;
        }

        // Record the location of PHY data
        if (part->type == PART_TYPE_DATA &&
            part->subtype == PART_SUBTYPE_DATA_RF)
        {
            rf_part = part;
        }

        uint32_t part_end = part->pos.offset + part->pos.size;

        if (part_end > data_end) data_end = part_end;
    }

    const char* image_ext = ".img";

    // remove filename extenstion
    size_t len = strlen(filename);
    while (len > 1)
    {
        --len;
        if (filename[len] == '.')
        {
            break;
        }
    }

    if (len < 1) abort();

    char* image_name = malloc(len + strlen(image_ext) + 1);
    if (!image_name) abort();

    strncpy(image_name, filename, len);
    strcat(image_name, image_ext);

    printf("./esptool.py --port \"/dev/ttyUSB0\" --baud 921600 write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0 %s\n", image_name);


    fseek(fp, 0, SEEK_SET);

    FILE* output = fopen(image_name, "wb");
    if (!output) abort();

    uint8_t* data = malloc(data_end);
    if (!data) abort();

    size_t count = fread(data, 1, data_end, fp);
    if (count != data_end) abort();

    count = fwrite(data, 1, data_end, output);
    if (count != data_end) abort();

    if (rf_part)
    {
        // Erase the RF data partition
        fseek(output, rf_part->pos.offset, SEEK_SET);

        const uint8_t blank = 0xff;
        size_t blank_count = fwrite(&blank, sizeof(blank), rf_part->pos.size, output);
        if (blank_count != rf_part->pos.size) abort();

        //printf("erased RF data - offset=%#08x, size=%#08x\n",
        //    rf_part->pos.offset, rf_part->pos.size);


        // Read the default RF data
        FILE* rfdata = fopen("phy_init_data.bin", "rb");
        if (!rfdata) abort();

        fseek(rfdata, 0, SEEK_END);
        size_t rffileSize = ftell(rfdata);
        fseek(rfdata, 0, SEEK_SET);

        if (rffileSize > rf_part->pos.size) abort();

        void* rfptr = malloc(rffileSize);
        if (!rfptr) abort();

        size_t rf_count = fread(rfptr, 1, rffileSize, rfdata);
        if (rf_count != rffileSize) abort();

        // Write the default RF data
        fseek(output, rf_part->pos.offset, SEEK_SET);
        fwrite(rfptr, 1, rffileSize, output);

        free(rfptr);
    }

    fclose(output);

    free(data);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage:\n");
        printf("\t%s filename\n", argv[0]);
        printf("\n");
        printf("Example:\n");
        printf("\t./esptool.py --port \"/dev/ttyUSB0\" --baud 921600 read_flash 0 0x1000000 flash.bin\n");
        printf("\t%s flash.bin\n", argv[0]);
        printf("\n");
        exit(1);
    }

    filename = argv[1];

    FILE* fp = fopen(filename, "rb");

    load_partitions(fp);
    print_partitions();

    extract_partitions(fp);

    fclose(fp);

    return 0;
}
