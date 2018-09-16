#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>



int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("usage: %s image_filename offset binary [...]\n", argv[0]);
    }
    else
    {
        const size_t imageLength = 16 * 1024 * 1024;

        // Create image storage
        uint8_t* imageData = (uint8_t*)malloc(imageLength);
        if (!imageData) abort();

        // Set image to 'erased'
        memset(imageData, 0xff, imageLength);


        int index = 1;
        long imageExtent = 0;

        const char* outputFilename = argv[index++];

        while (index < argc)
        {
            int base = strncmp(argv[index], "0x", 2) == 0 ? 16 : 10;

            long offset = strtol(argv[index++], NULL, base);
            const char* fileName = argv[index++];

            // Open the file
            FILE* file = fopen(fileName, "rb");
            if (!file) abort();

            // get the file size
            fseek(file, 0, SEEK_END);
            size_t fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);

            printf("offset=%ld, fileName='%s', fileSize=%ld\n", offset, fileName, fileSize);

            // Validate
            long extent = offset + fileSize;

            if (extent > imageLength)
            {
                printf("Out of Range.\n");
                abort();
            }

            if (extent > imageExtent) imageExtent = extent;


            // Read the data
            fread(imageData + offset, 1, fileSize, file);

            // Close file
            fclose(file);
        }

        // Write the image
        FILE* outfile = fopen(outputFilename, "wb");
        if (!outfile) abort();

        fwrite(imageData, 1, imageExtent, outfile);
        fclose(outfile);
    }

    return 0;
}
