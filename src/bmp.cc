#include "puffin/bmp.hh"
#include "puffin/impl/bmp_util.hh"
#include "puffin/exceptions.hh"
#include <iostream>
#include <fstream>

namespace puffin {


// -- Bitmap32::Impl -----------------------------------------------------------
struct Bitmap32::Impl {
        Impl(std::istream &f) {
                header.reset(f);
                const auto headerPos = f.tellg();
                infoHeader.reset(f);
                f.seekg(headerPos);
                f.seekg(infoHeader.infoHeaderSize, std::ios_base::cur);
                colorTable.reset(infoHeader, f);
                colorMask.reset(infoHeader, f);
                imageData1bit.reset(header, infoHeader, f);
                imageData2bit.reset(header, infoHeader, f); // non standard
                imageData4bit.reset(header, infoHeader, f);
                imageData8bit.reset(header, infoHeader, f);

                std::cout << header;
                std::cout << infoHeader;
                std::cout << colorTable;
                std::cout << colorMask;
                std::cout << imageData1bit;
                std::cout << imageData2bit;
                std::cout << imageData4bit;
                std::cout << imageData8bit;
        }

        int width() const {
                return infoHeader.width;
        }

        int height() const {
                return infoHeader.height;
        }

        Color32 get32(int x, int y) const {
                int pal = -1;
                if (!imageData1bit.empty()) {
                        pal = imageData1bit(x, y);
                } else if (!imageData2bit.empty()) {
                        pal = imageData2bit(x, y);
                } else if (!imageData4bit.empty()) {
                        pal = imageData4bit(x, y);
                } else if (!imageData8bit.empty()) {
                        pal = imageData8bit(x, y);
                } else {
                        // error
                        return Color32();
                }
                if (-1 == pal) {
                        // error
                        return Color32();
                }

                impl::BitmapColorTable::Entry entry = colorTable[pal];
                return Color32(
                        entry.red,
                        entry.green,
                        entry.blue
                );
        }

private:
        impl::BitmapHeader header;
        impl::BitmapInfoHeader infoHeader;
        impl::BitmapColorTable colorTable;
        impl::BitmapColorMask colorMask;
        impl::BitmapImageData<32, 1> imageData1bit;
        impl::BitmapImageData<32, 2> imageData2bit; // non standard
        impl::BitmapImageData<32, 4> imageData4bit;
        impl::BitmapImageData<32, 8> imageData8bit;
};


// -- Bitmap32 -----------------------------------------------------------------
Bitmap32::Bitmap32(std::istream &is) :
        impl_(new Impl(is))
{}

Bitmap32::~Bitmap32() {
        delete impl_;
}

int Bitmap32::width() const {
        return impl_->width();
}

int Bitmap32::height() const {
        return impl_->height();
}

Color32 Bitmap32::operator()(int x, int y) const {
        return impl_->get32(x, y);
}


// -- read_bmp() ---------------------------------------------------------------
Bitmap32* read_bmp32(std::string const &filename) {
        using namespace impl;

        std::ifstream f(filename, std::ios::binary);
        if (!f.is_open())
                throw exceptions::file_not_found(filename);

        Bitmap32 *ret = new Bitmap32(f);
        return ret;
}

}
