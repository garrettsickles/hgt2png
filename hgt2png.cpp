/*
 * hgt2png.cpp
 * 
 * Convert a HGT (.hgt) raster to a subset of PNG (.png) rasters
 * 
 */
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <memory>
#include <vector>

#include <libpng/png.h>

/*
 * Cross platform 64-bit file position support
 */
using CFile = std::unique_ptr<FILE, void(*)(FILE*)>;
#if defined(_MSC_VER)
    #define DIRECTORY_DELIM '\\'
    #define FTELL64(file) static_cast<std::int64_t>(_ftelli64(file))
    #define FSEEK64(file,pos,mode) static_cast<int>(_fseeki64(file,static_cast<std::int64_t>(pos),mode))
#elif defined(__linux__)
    #define _FILE_OFFSET_BITS 64
    #define DIRECTORY_DELIM '/'
	#define FTELL64(file) static_cast<std::int64_t>(ftello(file))
    #define FSEEK64(file,pos,mode) static_cast<int>(fseeko(file,static_cast<off_t>(pos),mode))
#endif
#if !defined(FTELL64) || !defined(FSEEK64)
    #error(Failed to define 64-bit FILE position macros...)
#endif

/*
 * Endian Check
 */
bool is_little_endian() {
    unsigned int i = 1;
    char *c = (char*)&i;
    return *c == 1;
}

/*
 * libpng 'write' function
 * 
 * Write a png to a std::vector of bytes
 */
void libpng_write_stdvector(png_structp png_ptr, png_bytep data, png_size_t length) {
    std::vector<std::uint8_t>* png = reinterpret_cast<std::vector<std::uint8_t>*>(png_get_io_ptr(png_ptr));
    png->insert(png->end(), data, data + length);
}

/*
 * Application entry point
 */
int main(int argc, char** argv)
{
    /*
     * Arguement Parsing
     */
    if (argc < 5)
    {
        std::printf("Usage: \n");
        return 0;
    }

    const auto width  = std::atoi(argv[2]);
    const auto height = std::atoi(argv[3]);

    /*
     * Read in the HGT (.hgt) file
     * 
     * Check that the file opened properly
     */
    CFile hgt_file = CFile(std::fopen(argv[1], "rb"), [](FILE* f)->void { std::fclose(f); });
    if (!hgt_file.get())
    {
        std::printf("Could not open file \"%s\", Exiting...\n", argv[1]);
        return 1;
    }

    /*
     * Extract the number of samples in the HGT raster
     */
    FSEEK64(hgt_file.get(), 0, SEEK_END);
    const auto hgt_size = FTELL64(hgt_file.get());
    FSEEK64(hgt_file.get(), 0, SEEK_SET);
    std::printf
    (
        "File: \"%s\"\nSize: %" PRId64 " bytes\nWidth: %d\nHeight: %d\n",
        argv[1], hgt_size, width, height
    );
    
    /*
     * Verify the size of the HGT raster
     */
    const auto pixel_count = static_cast<std::size_t>(width * height);
    const auto data_size   = static_cast<decltype(hgt_size)>(pixel_count * sizeof(std::int16_t));
    if (hgt_size != data_size)
    {
        std::printf("Actual size %" PRId64 ", Expected %" PRId64 ", Exiting...\n", hgt_size, data_size);
        return 1;
    }

    /*
     * Extract the location of the 1 degree raster from the filename
     */
    int  ll[2]       = { -1, -1 };
    char hemi[2]     = {  0,  0 };
    char* last_slash = std::strrchr(argv[1], DIRECTORY_DELIM);
    char* file_name  = last_slash ? last_slash + 1 : argv[1];
    std::sscanf(file_name, "%c%2d%c%3d.hgt", &hemi[0], &ll[0], &hemi[1], &ll[1]);

    /*
     * Verify the filename raster coordinates
     */
    const bool valid_hemi[2] = { hemi[0] == 'N' || hemi[0] == 'S', hemi[1] == 'W' || hemi[1] == 'E' };
    if (!valid_hemi[0] || !valid_hemi[1])
    {
        std::printf("Inavlid hemisphere \"%c\" in \"%s\", Exiting...\n", valid_hemi[0] ? hemi[1] : hemi[0], file_name);
        return 1;
    }
    std::printf
    (
        "%s: [%3d,%3d] degrees\n%s:  [%3d,%3d] degrees\n",
        hemi[0] == 'N' ? "North" : "South",
        ll[0], ll[0] + 1,
        hemi[1] == 'E' ? "East" : "West",
        ll[1], ll[1] + 1
    );

    /*
     * Extract the raster into memory
     */
    std::vector<std::uint8_t> raster(data_size);
    const auto read_size = std::fread(raster.data(), data_size, 1, hgt_file.get());
    if (read_size != 1)
    {
        std::printf("Read size %" PRId64 ", Expected %" PRId64 ", Exiting...\n", read_size, data_size);
        return 1;
    }
    
    /*
     * Swap the byte order from Big to Little Endian
     * if the platform is Little Endian
     */
    if (is_little_endian())
    {
        std::uint8_t swp = 0;
        for (auto i = 0; i < data_size; i += sizeof(std::int16_t))
        {
            swp = raster[i];
            raster[i] = raster[i+1];
            raster[i+1] = swp;
        }
    }

    /*
     * Convert the raster to unsigned 16 bit and
     * accumulate the range of the raster
     */
    int minimum = 32768;
    int maximum = -32768;
    int invalid = 0;

    std::int32_t temp;
    std::int16_t* svalue = nullptr;
    for (auto i = 0; i < data_size; i += sizeof(std::int16_t))
    {
        svalue = reinterpret_cast<std::int16_t*>(raster.data() + i);
        temp = static_cast<std::int32_t>(*svalue);
        if (temp < minimum)
        {
            if (temp != -32768) minimum = temp;
            else invalid++;
        } 
        else if (temp > maximum)
        {
            maximum = temp;
        }
    }
    std::printf("Min: %d\nMax: %d\nInvalid: %d\n", minimum, maximum, invalid);

    /*
     * Scale the raster to the range such that the minimum height
     * encodes to 0 and the maximum height encodes to 65534
     */
    double toscale = 0.0;
    const double minf = static_cast<double>(minimum);
    const double deltaf = static_cast<double>(maximum) - minf;
    svalue = nullptr;
    std::uint16_t* uvalue = nullptr;
    for (auto i = 0; i < data_size; i += sizeof(std::int16_t))
    {
        svalue = reinterpret_cast<std::int16_t*>(raster.data() + i);
        uvalue = reinterpret_cast<std::uint16_t*>(raster.data() + i);
        toscale = static_cast<double>(*svalue);
        *uvalue = *svalue == -32768 ? 0xFFFF : static_cast<std::uint16_t>((toscale - minf) * 65534.0 / deltaf);
    }

    /*
     * Swap the byte order from Little to Big Endian
     * if the platform is Little Endian
     */
    if (is_little_endian())
    {
        std::uint8_t swp = 0;
        for (auto i = 0; i < data_size; i += sizeof(std::int16_t))
        {
            swp = raster[i];
            raster[i] = raster[i+1];
            raster[i+1] = swp;
        }
    }

    /*
     * Write the PNG to a data buffer
     */
    std::vector<std::uint8_t> png_data;

    /*
     * Setup the PNG info
     */
    png_structp png  = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   info = png_create_info_struct(png);
    png_set_IHDR(png, info,
        static_cast<png_uint_32>(width),
        static_cast<png_uint_32>(height),
        sizeof(std::uint16_t) * 8,
        PNG_COLOR_TYPE_GRAY,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );

    /*
     * Set the 'sCAL' to have the minimum and range
     */
    png_set_sCAL(png, info, 1, minf, deltaf);

    /*
     * Setup a vector of pointers to the beginning of each row
     */
    std::vector<std::uint8_t*> rows(height);
    for (auto r = 0; r < height; r++)
        rows[r] = raster.data() + r * width * sizeof(std::uint16_t);

    /*
     * Write the PNG to a buffer
     */
    png_set_rows(png, info, rows.data());
    png_set_write_fn(png, &(png_data), libpng_write_stdvector, NULL);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
    png_destroy_write_struct(&png, &info);

    /*
     * Write the PNG buffer out to file
     */
    CFile png_file = CFile(std::fopen(argv[4], "wb"), [](FILE* f)->void { std::fclose(f); });
    std::fwrite(png_data.data(), png_data.size(), 1, png_file.get());

    return 0;
}