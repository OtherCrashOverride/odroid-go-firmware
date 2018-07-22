#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


const char* FIRMWARE = "firmware.fw";
const char* HEADER = "ODROIDGO_FIRMWARE_V00_01";

#define FIRMWARE_DESCRIPTION_SIZE (40)
char FirmwareDescription[FIRMWARE_DESCRIPTION_SIZE];


// TODO: packed
typedef struct
{
    uint8_t type;
    uint8_t subtype;
    uint8_t _reserved0;
    uint8_t _reserved1;

    uint8_t label[16];

    uint32_t flags;
    uint32_t length;
} odroid_partition_t;


int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("usage: %s description type subtype length label binary [...]\n", argv[0]);
    }
    else
    {
        FILE* file = fopen(FIRMWARE, "wb");
        if (!file) abort();

        size_t count;

        count = fwrite(HEADER, strlen(HEADER), 1, file);
        printf("HEADER='%s'\n", HEADER);


        strncpy(FirmwareDescription, argv[1], FIRMWARE_DESCRIPTION_SIZE);
        FirmwareDescription[FIRMWARE_DESCRIPTION_SIZE - 1] = 0;

        count = fwrite(FirmwareDescription, FIRMWARE_DESCRIPTION_SIZE, 1, file);
        printf("FirmwareDescription='%s'\n", FirmwareDescription);

        int part_count = 0;
        int i = 2;
        while (i < argc)
        {
            odroid_partition_t part = {0};


            part.type = atoi(argv[i++]);
            part.subtype = atoi(argv[i++]);
            part.length = atoi(argv[i++]);

            const char* label = argv[i++];
            strncpy(part.label, label, sizeof(part.label));

            printf("[%d] type=%d, subtype=%d, length=%d, label=%-16s\n",
                part_count, part.type, part.subtype, part.length, part.label);

            const char* filename = argv[i++];

            FILE* binary = fopen(filename, "rb");
            if (!binary) abort();

            // get the file size
            fseek(binary, 0, SEEK_END);
            size_t fileSize = ftell(binary);
            fseek(binary, 0, SEEK_SET);

            // read the data
            void* data = malloc(fileSize);
            if (!data) abort();

            count = fread(data, 1, fileSize, binary);
            if (count != fileSize)
            {
                printf("fread failed: count=%ld, fileSize=%ld\n", count, fileSize);
                abort();
            }

            fclose(binary);

            // write the entry
            fwrite(&part, sizeof(part), 1, file);

            uint32_t length = (uint32_t)fileSize;
            fwrite(&length, sizeof(length), 1, file);

            fwrite(data, fileSize, 1, file);
            free(data);

            printf("part=%d, length=%d, data=%s\n", part_count, length, filename);

            part_count++;
        }

        fclose(file);
    }

}
