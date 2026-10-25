#include "frame.hpp"
#include <fstream>
#include <array>
#include <algorithm>
#include <stdexcept>

bool Frame::savePng(const std::filesystem::path& path,std::string& error) const
{
    error.clear();
    if(!width || !height || width>256 || height>256 || uint64_t(width)*height*3!=rgb.size() || rgb.size()>65536)
    { error="Dimensions ou pixels de l'image invalides"; return false; }
    try {
        std::vector<uint8_t> png{137,80,78,71,13,10,26,10};
        auto big=[&](uint32_t value) { for(int i=3;i>=0;--i) png.push_back(uint8_t(value>>(8*i))); };
        auto chunk=[&](const char* type,const std::vector<uint8_t>& bytes) {
            big(uint32_t(bytes.size())); const auto start=png.size();
            for(size_t i=0;i<4;++i) png.push_back(uint8_t(type[i]));
            png.insert(png.end(),bytes.begin(),bytes.end());
            uint32_t crc=0xFFFFFFFF;
            for(size_t i=start;i<png.size();++i) {
                crc^=png[i]; for(unsigned b=0;b<8;++b) crc=(crc>>1)^((crc&1)?0xEDB88320U:0);
            }
            big(~crc);
        };
        std::vector<uint8_t> header(13,0);
        for(unsigned i=0;i<4;++i) { header[i]=uint8_t(width>>(24-i*8)); header[4+i]=uint8_t(height>>(24-i*8)); }
        header[8]=8; header[9]=2; chunk("IHDR",header);
        std::vector<uint8_t> scan;
        for(uint32_t y=0;y<height;++y) {
            scan.push_back(0); const auto at=size_t(y)*width*3;
            scan.insert(scan.end(),rgb.begin()+at,rgb.begin()+at+width*3);
        }
        std::vector<uint8_t> zlib{0x78,0x01}; // Uncompressed DEFLATE blocks, deterministic output.
        for(size_t at=0;at<scan.size();) {
            const auto size=uint16_t(std::min<size_t>(65535,scan.size()-at));
            zlib.push_back(at+size==scan.size()?1:0);
            zlib.push_back(uint8_t(size)); zlib.push_back(uint8_t(size>>8));
            zlib.push_back(uint8_t(~size)); zlib.push_back(uint8_t((uint16_t(~size))>>8));
            zlib.insert(zlib.end(),scan.begin()+at,scan.begin()+at+size); at+=size;
        }
        uint32_t a=1,b=0;
        for(auto byte:scan) { a=(a+byte)%65521; b=(b+a)%65521; }
        const auto adler=(b<<16)|a;
        for(int i=3;i>=0;--i) zlib.push_back(uint8_t(adler>>(8*i)));
        chunk("IDAT",zlib); chunk("IEND",{});
        std::ofstream out(path,std::ios::binary|std::ios::trunc);
        out.write(reinterpret_cast<const char*>(png.data()),std::streamsize(png.size())); out.close();
        if(!out) { error="Ecriture du PNG impossible"; return false; }
        return true;
    }
    catch(const std::exception& e) { error=std::string("Export PNG : ")+e.what(); return false; }
}
