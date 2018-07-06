#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


const char* FIRMWARE = "firmware.bin";
const char* HEADER = "ODROIDGO_FIRMWARE_V00_00";

#define FIRMWARE_DESCRIPTION_SIZE (40)
char FirmwareDescription[FIRMWARE_DESCRIPTION_SIZE];


int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("usage: %s description slot binary [slot binary]\n", argv[0]);
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

        int i = 2;
        while (i < argc)
        {
            uint32_t slot = atoi(argv[i++]);
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
            fwrite(&slot, sizeof(slot), 1, file);

            uint32_t length = (uint32_t)fileSize;
            fwrite(&length, sizeof(length), 1, file);

            fwrite(data, fileSize, 1, file);
            free(data);

            printf("slot=%d, length=%d, data=%s\n", slot, length, filename);
        }

        fclose(file);
    }

}
