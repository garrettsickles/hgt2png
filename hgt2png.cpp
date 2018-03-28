/*
 * hgt2png.cpp
 * 
 * Convert a HGT (.hgt) raster to a subset of PNG (.png) rasters
 * 
 */
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include <libpng/png.h>

/*
 * Cross platform 64-bit file support
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
 * Degrees To Radians
 */
double deg_to_rad(const double deg) {
    return deg * 3.14159265358979323846 / 180;
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
    if (argc < 4 || argc == 5)
    {
        std::printf(
            "Usage: \n"
            "        hgt2png <HGT Source> <HGT Width> <HGT Height> [<Subwidth> <Subheight>]\n"
            "Note:\n"
            "        The last two parameters, [<Subwidth> <Subheight>], are optional.\n"
            "            - If excluded, both default to 1.\n"
            "            - If included, both must be counting number which evenly subdivide\n"
            "                  <HGT Width> and <HGT Height>, respectively.\n"
            "\n"
            "E.G.\n"
            "        hgt2png SOURCE.hgt temp/ 3601 3601 2 2\n"
            "\n"
            "            => temp/SOURCE.0.0.png\n"
            "            => temp/SOURCE.0.1800.png\n"
            "            => temp/SOURCE.1800.0.png\n"
            "            => temp/SOURCE.1800.1800.png\n"
            "\n"
            "E.G.\n"
            "        hgt2png SOURCE.hgt MyData. 3601 3601\n"
            "\n"
            "            => MyData.SOURCE.0.0.png\n"
            "\n"
        );
        return 0;
    }

    const auto width = std::atoi(argv[2]);
    const auto height = std::atoi(argv[3]);
    const auto rows = argc >= 5 ? std::atoi(argv[4]) : 1;
    const auto cols = argc >= 5 ? std::atoi(argv[5]) : 1;
    const auto pixel_count = static_cast<std::size_t>(width * height);

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
        "File: \"%s\" (%" PRId64 " bytes)\nSize: %d(w) x %d(h) pixels (%lu samples)\n",
        argv[1], hgt_size, width, height, pixel_count
    );

    /*
     * Verify the subdivisions
     */
    if (cols < 1 || rows < 1)
    {
        std::printf("Both row and column must be greater than or equal to 1, Exiting...\n");
        return 1;
    }
    if (cols > 1 && (width - 1) % cols)
    {
        std::printf("One less than the width of %d is not evenly divisible by %d, Exiting...\n", width, cols);
        return 1;
    }
    if (rows > 1 && (height - 1) % rows)
    {
        std::printf("One less than the height of %d is not evenly divisible by %d, Exiting...\n", height, rows);
        return 1;
    }
    const auto subwidth = (width / cols) + (cols > 1 ? 1 : 0);
    const auto subheight = (height / rows) + (rows > 1 ? 1 : 0);

    /*
     * Verify the size of the HGT raster
     */
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
    std::string base_name = file_name;
    base_name.erase(base_name.find_last_of("."), std::string::npos);
    
    /*
     * Verify the filename raster coordinates
     */
    const bool valid_hemi[2] = { hemi[0] == 'N' || hemi[0] == 'S', hemi[1] == 'W' || hemi[1] == 'E' };
    if (!valid_hemi[0] || !valid_hemi[1])
    {
        std::printf("Inavlid hemisphere \"%c\" in \"%s\", Exiting...\n", valid_hemi[0] ? hemi[1] : hemi[0], file_name);
        return 1;
    }
    std::printf("Bounds: (%d%c, %d%c) to (%d%c, %d%c)\n",
        ll[0], hemi[0], ll[1], hemi[1], ll[0] + 1, hemi[0], ll[1] + 1, hemi[1]
    );

    /*
     * Extract the raster into memory
     */
    std::vector<std::uint8_t> raster(data_size);
    const auto read_size = std::fread(raster.data(), data_size, 1, hgt_file.get());
    if (read_size != 1)
    {
        std::printf("Read size %" PRId64 ", Expected 1, Exiting...\n", read_size);
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
    std::printf("Range: [%d, %d] meters\nMissing: %d pixels\n", minimum, maximum, invalid);

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
     * Calculate the physical dimensions of each subraster in radians
     */
    const double upx = deg_to_rad(1.0 / static_cast<double>(subwidth - 1));
    const double upy = deg_to_rad(1.0 / static_cast<double>(subheight - 1));

    /*
     * Write the PNG to a data buffer
     */
    std::vector<std::uint8_t> png_data;
    std::vector<std::uint8_t*> png_rows(subheight);
    size_t total_png_size = 0;

    int row_offset = 0;
    int col_offset = 0;
    for (auto row_index = 0; row_index < rows; row_index++)
    {
        for (auto col_index = 0; col_index < cols; col_index++)
        {
            /*
             * Setup the PNG info
             */
            png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
            png_infop   info = png_create_info_struct(png);
            png_set_IHDR(png, info,
                static_cast<png_uint_32>(subwidth),
                static_cast<png_uint_32>(subheight),
                sizeof(std::uint16_t) * 8,
                PNG_COLOR_TYPE_GRAY,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT
            );

            /*
             * Set the 'sCAL' (Physical Scale)
             * 
             * The dimensions of each pixel in radians
             */
            png_set_sCAL(png, info, 2, upx, upy);

            /*
             * Set the 'pCAL' (Pixel Calibration)
             * 
             * The 1st order function mapping the encoded PNG values to the physical values
             */
            auto decription = std::string("SRTM-HGT");
            auto units = std::string("m");
            auto p0 = std::to_string(minf);
            auto p1 = std::to_string(deltaf);
            png_charp params[2] = { &p0.at(0), &p1.at(0)};
            png_set_pCAL(png, info, &decription.at(0), -32767, 32767, 0, 2, &units.at(0), params);

            /*
             * Setup a vector of pointers to the beginning of each row
             */
            for (auto row_abs = row_offset; row_abs < (row_offset + subheight); row_abs++)
            {
                png_rows[row_abs - row_offset] =
                    raster.data() +
                    row_abs * width * sizeof(std::uint16_t) +
                    (col_offset - 1) * sizeof(std::uint16_t);
            }
                

            /*
             * Write the PNG to a buffer
             */
            png_set_rows(png, info, png_rows.data());
            png_set_write_fn(png, &(png_data), libpng_write_stdvector, NULL);
            png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
            png_destroy_write_struct(&png, &info);
            const auto png_size = png_data.size();

            /*
             * Write the PNG buffer out to file
             */
            std::string subname =
                base_name + "." +
                std::to_string(row_offset) + "." + std::to_string(col_offset) + ".png";
            CFile png_file = CFile(std::fopen(subname.c_str(), "wb"), [](FILE* f)->void { std::fclose(f); });
            if (!hgt_file.get())
            {
                std::printf("Could not open file \"%s\", Exiting...\n", subname.c_str());
                return 1;
            }
            const auto write_size = std::fwrite(png_data.data(), png_data.size(), 1, png_file.get());
            png_data.clear();
            
            /*
             * Verify the file write
             */
            if (write_size != 1)
            {
                std::printf("Write size %" PRId64 ", Expected 1, Exiting...\n", write_size);
                return 1;
            }

            total_png_size += png_size;
            col_offset += (subheight - 1);      
        }
        col_offset = 0;
        row_offset += (subwidth - 1);
    }

    /*
     * Show some statistics
     */
    std::printf("Output: Compression: %.2lf%% of original size\n",
        static_cast<double>(total_png_size) / static_cast<double>(data_size) * 100.0
    );
    
    return 0;
}