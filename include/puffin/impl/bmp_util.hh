#ifndef BMP_UTIL_HH_INCLUDED_20181220
#define BMP_UTIL_HH_INCLUDED_20181220

#include "puffin/impl/io_util.hh"
#include "puffin/bitfield.hh"
#include <vector>

// =============================================================================
// ==   Bitmap Structures.   ===================================================
// =============================================================================
namespace puffin { namespace impl {

enum BitmapCompression
{
        BI_RGB = 0x0000,
        BI_RLE8 = 0x0001,
        BI_RLE4 = 0x0002,
        BI_BITFIELDS = 0x0003,
        BI_JPEG = 0x0004,
        BI_PNG = 0x0005,
        BI_CMYK = 0x000B,
        BI_CMYKRLE8 = 0x000C,
        BI_CMYKRLE4 = 0x000D
};


// Corresponds to BITMAPFILEHEADER on Windows.
// See https://docs.microsoft.com/en-us/windows/desktop/api/wingdi/ns-wingdi-tagbitmapfileheader
struct BitmapHeader {
        uint16_t signature;   // 'BM'
        uint32_t size;        // Size in bytes.
        uint16_t reserved1;
        uint16_t reserved2;
        uint32_t dataOffset;  // Offset from beginning of header to pixel data.

        BitmapHeader();
        BitmapHeader(std::istream &f);
        void reset(std::istream &f);
};


// Corresponds to BITMAPINFOHEADER on Windows.
// See https://docs.microsoft.com/en-us/previous-versions/dd183376(v%3Dvs.85)
struct BitmapInfoHeader {
        // read from file
        uint32_t infoHeaderSize;
        uint32_t width;
        int32_t  height;
        uint16_t planes;
        uint16_t bitsPerPixel;
        uint32_t compression; // 0 = BI_RGB (none), 1 = BI_RLE8, 2 = BI_RLE4
        uint32_t compressedImageSize; // 0 if compression is 0
        uint32_t xPixelsPerMeter;
        uint32_t yPixelsPerMeter;
        uint32_t colorsUsed; // Actually used colors (256 for 8 bit, 0=max number of colors according to bitsPerPixel)
        uint32_t importantColors; // Number of colors required for display. 0=max.

        // computed fields
        bool isBottomUp;

        BitmapInfoHeader();
        BitmapInfoHeader(/*BitmapHeader const &header, */std::istream &f);
        void reset(std::istream &f);
};


struct BitmapColorTable {
        struct Entry {
                uint8_t blue;
                uint8_t green;
                uint8_t red;
                uint8_t reserved;

                Entry();
                Entry(std::istream &);
                void reset(std::istream &f);
        };

        BitmapColorTable();
        BitmapColorTable (BitmapInfoHeader const &info, std::istream &f);
        void reset(BitmapInfoHeader const &info, std::istream &f);

        Entry operator[] (int i) const {
                return entries_[i];
        }

        Entry at (int i) const {
                return entries_.at(i);
        }

        std::vector<Entry>::size_type size() const {
                return entries_.size();
        }

        bool empty() const {
                return entries_.empty();
        }
private:
        std::vector<Entry> entries_;
        static std::vector<Entry> readEntries(
                BitmapInfoHeader const &info,
                std::istream &f
        );
};


struct BitmapColorMasks {
        uint32_t blue;
        uint32_t green;
        uint32_t red;

        BitmapColorMasks();
        BitmapColorMasks (BitmapInfoHeader const &info, std::istream &f);
        void reset (BitmapInfoHeader const &info, std::istream &f);
};


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// On "pixel-endianness"
// ------------------------
//
// Given a 4-byte chunk of pixel data, the pixel value with the smallest
// x-coordinate is left-most when read unaltered from a BMP file.
//
// This is sub-optimal, as this imposes an additional subtraction
// (yes, I was tempted to write "imposes subtraction addition", but
// I dislike comment humor - irony is alright, tho.)
//
// Now for an explanation:
//
// Given
//
//    chunk size: 16 bits
//    pixel size:  4 bist,
//
// we get this layout and pixel equations:
//
//         p0   p1   p2   p3
//        [1111 0110 0010 0000]
//
//        p0 =  (chunk>>12) & 1111b = (chunk>>(16-4 - 0*4)) & 1111b
//        p1 =  (chunk>> 8) & 1111b = (chunk>>(16-4 - 1*4)) & 1111b
//        p2 =  (chunk>> 4) & 1111b = (chunk>>(16-4 - 2*4)) & 1111b
//        p3 =  (chunk>> 0) & 1111b = (chunk>>(16-4 - 3*4)) & 1111b.
//
// But with "pixel-endianness" flipped, the layout and equations
//
//         p3   p2   p1   p0
//        [0000 0010 0110 1111]
//
//        p0 =  (chunk>> 0) & 1111b = (chunk>>(0*4)) & 1111b
//        p1 =  (chunk>> 4) & 1111b = (chunk>>(1*4)) & 1111b
//        p2 =  (chunk>> 8) & 1111b = (chunk>>(2*4)) & 1111b
//        p3 =  (chunk>>12) & 1111b = (chunk>>(3*4)) & 1111b
//
// spare us a confusing shift and a subtraction.
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template <int ChunkWidth, int PixelWidth>
struct BitmapRowDataPaletted ;

template <int PixelWidth>
struct BitmapRowDataPaletted<32, PixelWidth> {

        // -- constants ------------------------------------------------
        enum {
                chunk_width = 32,
                pixel_width = PixelWidth,
                pixels_per_chunk = chunk_width / pixel_width,
                pixel_mask = (1U << pixel_width) - 1U,
                nonsignificant_bits = chunk_width % pixel_width
        };

        // -- types ----------------------------------------------------
        typedef uint32_t chunk_type;
        typedef std::vector<chunk_type> container_type;
        typedef int color_type;

        // -- members --------------------------------------------------
        BitmapRowDataPaletted() { }

        BitmapRowDataPaletted(
                BitmapInfoHeader const &infoHeader,
                std::istream &f
        ) {
                reset(infoHeader, f);
        }

        void reset(
                BitmapInfoHeader const &infoHeader,
                std::istream &f
        ) {
                const auto numChunks = width_to_chunk_count(infoHeader.width);
                chunks_.clear();
                chunks_.reserve(numChunks);
                for (auto i=0U; i!=numChunks; ++i) {
                        // Too load the chunk unaltered, simply do:
                        //    const uint32_t chunk = read_uint32_be(f);
                        //    chunks_.push_back(chunk);
                        // This would require another (worse) version of
                        // extract_pixel(), though.

                        const uint32_t chunk = read_uint32_be(f) >>
                                               nonsignificant_bits;

                        // Now flip the order of pixels in this chunk:
                        uint32_t flipped = 0U;
                        for (auto x=0U; x!=pixels_per_chunk; ++x) {
                                const auto pixel = extract_pixel(chunk, x);
                                flipped = (flipped<<pixel_width) | pixel;
                        }

                        chunks_.push_back(flipped);
                }
        }

        int operator[] (int x) const {
                const uint32_t
                        chunk_index = x_to_chunk_index(x),
                        chunk_ofs   = x_to_chunk_offset(x),
                        chunk = chunks_[chunk_index],
                        value = extract_pixel(chunk, chunk_ofs);
                return static_cast<int>(value);
        }

private:
        // -- data -----------------------------------------------------
        container_type chunks_;

        // -- functions ------------------------------------------------
        static
        uint32_t width_to_chunk_count(uint32_t x) noexcept {
                // This adjusted division ensures that any result with
                // a non-zero fractional part is rounded up:
                //      width=64  ==>  c = (64 + 31) / 32 = 95 / 32 = 2
                //      width=65  ==>  c = (65 + 31) / 32 = 96 / 32 = 3
                return (x + pixels_per_chunk - 1U) / pixels_per_chunk;
        }

        static
        uint32_t x_to_chunk_index(uint32_t x) {
                return x / pixels_per_chunk;
        }

        static
        uint32_t x_to_chunk_offset(uint32_t x) {
                return x - x_to_chunk_index(x) * pixels_per_chunk;
        }

        static
        uint32_t extract_pixel(uint32_t chunk, uint32_t ofs) {
                const uint32_t
                        rshift = ofs * pixel_width,
                        value = (chunk >> rshift) & pixel_mask;
                return value;

                // Here's the code for [pixel_0, pixel_1, ..., pixel_n]:
                //
                // const uint32_t
                //        rshift = chunk_width-pixel_width - ofs*pixel_width,
                //        value = (chunk >> rshift) & pixel_mask;
                // return value;
        }
};

template <int ChunkWidth, int RWidth, int GWidth, int BWidth>
struct BitmapRowDataRgb {
        // -- constants ------------------------------------------------
        enum {
                chunk_width = ChunkWidth,
                bytes_per_chunk = chunk_width / 8,
                r_width = RWidth,
                g_width = GWidth,
                b_width = BWidth,
                pixel_width = r_width+g_width+b_width,
                pixels_per_chunk = chunk_width / pixel_width,
                pixel_mask = (1U << pixel_width) - 1U,
                nonsignificant_bits = chunk_width % pixel_width
        };

        // -- types ----------------------------------------------------
        typedef uint32_t chunk_type;
        typedef std::vector<chunk_type> container_type;
        typedef bitfield4<b_width, g_width, r_width, 0> color_type;

        // -- members --------------------------------------------------
        BitmapRowDataRgb() { }

        BitmapRowDataRgb(
                BitmapInfoHeader const &infoHeader,
                std::istream &f
        ) {
                reset(infoHeader, f);
        }

        void reset(
                BitmapInfoHeader const &infoHeader,
                std::istream &f
        ) {
                const auto numChunks = width_to_chunk_count(infoHeader.width);
                chunks_.clear();
                chunks_.reserve(numChunks);
                for (auto i=0U; i!=numChunks; ++i) {

                        // TODO: make chunk-type more generic.
                        const uint64_t chunk =
                                read_bytes_to_uint64_be(f, bytes_per_chunk) >>
                                nonsignificant_bits;

                        // Flip the order of pixels in this chunk (which will
                        //  result in a more trivial extract_pixel() function):
                        uint32_t flipped = 0U;
                        for (auto x=0U; x!=pixels_per_chunk; ++x) {
                                const auto pixel = extract_pixel(chunk, x);
                                flipped = (flipped<<pixel_width) | pixel;
                        }

                        chunks_.push_back(flipped);
                }

                // Round up to multiple of 4.
                // Example:
                //   2 chunks, 3 bytes each -> 6 bytes read
                //   Need to round 6 to 8 (=2*4).
                //
                //   Attempt 1)   4*(6/4) = 4  -> no.
                //   Attempt 2)   4*((6+3)/4) = 8 -> maybe.
                //   round(x)_m = m*((x+m-1)/m)
                //
                //   Tests:
                //       round(7)_4 = 4*((7+3)/4) = 8
                //       round(8)_4 = 4*((8+3)/4) = 8
                //       round(9)_4 = 4*((9+3)/4) = 12
                // TODO: Make a reusable function of round().
                const uint32_t
                        bytes_read = numChunks * bytes_per_chunk,
                        next_mul4 = 4U*((bytes_read+3U)/4U),
                        pad_bytes = next_mul4 - bytes_read;
                f.ignore(pad_bytes);
        }

        color_type operator[] (int x) const {
                const uint32_t
                        chunk_index = x_to_chunk_index(x),
                        chunk_ofs   = x_to_chunk_offset(x),
                        chunk = chunks_[chunk_index],
                        value = extract_pixel(chunk, chunk_ofs);
                return value;
        }

private:
        // -- data -----------------------------------------------------
        container_type chunks_;

        // -- functions ------------------------------------------------
        static
        uint32_t width_to_chunk_count(uint32_t width) noexcept {
                // This adjusted division ensures that any result with
                // a non-zero fractional part is rounded up:
                //      width=64  ==>  c = (64 + 31) / 32 = 95 / 32 = 2
                //      width=65  ==>  c = (65 + 31) / 32 = 96 / 32 = 3
                return (width + pixels_per_chunk - 1U) / pixels_per_chunk;
        }

        static
        uint32_t x_to_chunk_index(uint32_t x) {
                return x / pixels_per_chunk;
        }

        static
        uint32_t x_to_chunk_offset(uint32_t x) {
                return x - x_to_chunk_index(x) * pixels_per_chunk;
        }

        template <typename ChunkT>
        static
        ChunkT extract_pixel(ChunkT chunk, uint32_t ofs) {
                const ChunkT
                        rshift = ofs * pixel_width,
                        value = (chunk >> rshift) & pixel_mask;
                return value;

                // Here's the code for [pixel_0, pixel_1, ..., pixel_n]:
                //
                // const uint32_t
                //        rshift = chunk_width-pixel_width - ofs*pixel_width,
                //        value = (chunk >> rshift) & pixel_mask;
                // return value;
        }
};

template <typename RowType>
struct BitmapImageData {
        // -- constants ------------------------------------------------
        enum {
                chunk_width = RowType::chunk_width,
                pixel_width = RowType::pixel_width
        };

        // -- types ----------------------------------------------------
        typedef RowType row_type;
        typedef typename RowType::color_type color_type;
        typedef std::vector<row_type> container_type;
        typedef typename container_type::size_type size_type;

        // -- members --------------------------------------------------
        BitmapImageData() : width_(0) {}

        BitmapImageData(
                BitmapHeader const &header,
                BitmapInfoHeader const &infoHeader,
                std::istream &f
        ) {
                reset(header, infoHeader, f);
        }

        void reset(
                BitmapHeader const &header,
                BitmapInfoHeader const &infoHeader,
                std::istream &f
        ) {
                if (infoHeader.bitsPerPixel != pixel_width) {
                        clear_and_shrink();
                        return;
                }

                f.seekg(header.dataOffset);
                if (infoHeader.isBottomUp) {
                        // Least evil approach: Fill from behind while keeping
                        // the advantages of std::vector and not needing a
                        // temporary container object:
                        clear_and_shrink(infoHeader.height);
                        for (int y=infoHeader.height-1; y>=0; --y) {
                                rows_[y].reset(infoHeader, f);
                        }
                } else {
                        // Can construct upon reading:
                        clear_and_shrink();
                        rows_.reserve(infoHeader.height);
                        for (int y = 0; y != infoHeader.height; ++y) {
                                rows_.emplace_back(infoHeader, f);
                        }
                }
                width_ = infoHeader.width;
        }

        color_type operator()(int x, int y) const {
                return rows_[y][x];
        }

        bool empty() const {
                return rows_.size() == 0;
        }

        size_type width() const {
                return width_;
        }

        size_type height() const {
                return rows_.size();
        }

private:
        container_type rows_;
        size_type width_ ;

        void clear_and_shrink(int newSize = 0) {
                rows_.clear();
                //  Explicit typing here to prevent incompatible container_type
                // constructor:
                std::vector<row_type> tmp(newSize);
                tmp.swap(rows_);
        }
};

} }

// =============================================================================
// ==   I/O.   =================================================================
// =============================================================================
#include <iomanip>
#include <ostream>

namespace puffin { namespace impl {

inline
std::ostream& operator<< (std::ostream &os, BitmapCompression bc) {
        switch (bc) {
        case BI_RGB: return os << "BI_RGB";
        case BI_RLE8: return os << "BI_RLE8";
        case BI_RLE4: return os << "BI_RLE4";
        case BI_BITFIELDS: return os << "BI_BITFIELDS";
        case BI_JPEG: return os << "BI_JPEG";
        case BI_PNG: return os << "BI_PNG";
        case BI_CMYK: return os << "BI_CMYK";
        case BI_CMYKRLE8: return os << "BI_CMYKRLE8";
        case BI_CMYKRLE4: return os << "BI_CMYKRLE4";
        default: return os << "<unknown>";
        }
}

inline
std::ostream& operator<< (std::ostream &os, BitmapHeader const &v) {
        return os << "Header{\n"
                  << "  signature:" << std::hex << v.signature << std::dec
                  << " ('"
                  << static_cast<char>(v.signature>>8)
                  << static_cast<char>(v.signature & 0xff)
                  << "')\n"
                  << "  size:" << v.size << "\n"
                  << "  reserved1:" << v.reserved1 << "\n"
                  << "  reserved2:" << v.reserved2 << "\n"
                  << "  dataOffset:" << v.dataOffset << "\n"
                  << "}\n";
}

inline
std::ostream& operator<< (std::ostream &os, BitmapInfoHeader const &v) {
        return os << "InfoHeader{\n"
                  << "  infoHeaderSize:" << v.infoHeaderSize << "\n"
                  << "  width:" << v.width << "\n"
                  << "  height:" << v.height << "\n"
                  << "  isBottomUp:" << v.isBottomUp << "\n"
                  << "  planes:" << v.planes << "\n"
                  << "  bitsPerPixel:" << v.bitsPerPixel << "\n"
                  << "  compression:" << static_cast<BitmapCompression>(v.compression) << "\n"
                  << "  compressedImageSize:" << v.compressedImageSize << "\n"
                  << "  xPixelsPerMeter:" << v.xPixelsPerMeter << "\n"
                  << "  yPixelsPerMeter:" << v.yPixelsPerMeter << "\n"
                  << "  colorsUsed:" << v.colorsUsed << "\n"
                  << "  importantColors:" << v.importantColors << "\n"
                  << "}\n";
}

inline
std::ostream& operator<< (std::ostream &os, BitmapColorTable::Entry const &v) {
        return os << "[" << std::setw(3) << (unsigned int)v.red
                  << "|" << std::setw(3) << (unsigned int)v.green
                  << "|" << std::setw(3) << (unsigned int)v.blue
                  << "|" << std::setw(3) << (unsigned int)v.reserved
                  << "]";
}

inline
std::ostream& operator<< (std::ostream &os, BitmapColorTable const &v) {
        os << "ColorTable{\n";
        for (int i=0; i!=v.size(); ++i) {
                os << "  [" << std::setw(3) << i << "]:" << v[i] << "\n";
        }
        os << "}\n";
        return os;
}

inline
std::ostream& operator<< (std::ostream &os, BitmapColorMasks const &v) {
        return os << "ColorMask{\n"
                  << "  red..:" << v.red << "\n"
                  << "  green:" << v.green << "\n"
                  << "  blue.:" << v.blue << "\n"
                  << "}\n";
}

// TODO: move type_str() elsewhere
template <int ChunkWidth, int PixelWidth>
inline std::string type_str(
        BitmapRowDataPaletted<ChunkWidth, PixelWidth> const &
) {
        std::stringstream ss;
        ss << "BitmapRowDataPaletted<"
           << ChunkWidth << ", "
           << PixelWidth << ">";
        return ss.str();
}

template <int ChunkWidth, int RWidth, int GWidth, int BWidth>
inline std::string type_str(
        BitmapRowDataRgb<ChunkWidth, RWidth, GWidth, BWidth> const &
) {
        std::stringstream ss;
        ss << "BitmapRowDataRgb<"
           << ChunkWidth << ", "
           << RWidth << ", "
           << GWidth << ", "
           << BWidth << ">";
        return ss.str();
}

template <typename RowType>
inline std::string type_str(
        BitmapImageData<RowType> const &
) {
        std::stringstream ss;
        ss << "BitmapImageData<"
           << type_str(RowType())
           << ">";
        return ss.str();
}

template <typename RowType>
inline
std::ostream& operator<< (
        std::ostream &os,
        BitmapImageData<RowType> const &data
) {
        if (data.empty())
                return os;
        os << type_str(data) << " {\n";
        for (int y=0; y!=data.height(); ++y) {
                os << " [";
                for (int x=0; x!=data.width(); ++x) {
                        if (x) {
                                os << ", ";
                        }
                        os << data(x,y);
                }
                os << "]\n";
        }
        os << "}\n";
        return os;
}
} }

#endif //BMP_UTIL_HH_INCLUDED_20181220
