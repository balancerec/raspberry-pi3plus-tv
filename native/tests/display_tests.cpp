#include "tv/display.hpp"
#include <cstdlib>
#include <iostream>
using namespace raspberry_tv::display;
void check(bool condition,const char* reason) {
    if(!condition) {std::cerr << reason << "\n"; std::exit(EXIT_FAILURE);}
}
int main() {
    check(DesignWidth == 1280 && DesignHeight == 720,"design coordinates");
    check(NativeWidth == 1920 && NativeHeight == 1080,"Full HD internal dimensions");
    check(toNative(0)==0,"origin");
    check(toNative(DesignWidth)==NativeWidth,"horizontal exact mapping");
    check(toNative(DesignHeight)==NativeHeight,"vertical exact mapping");
    check(toNative(8)==12,"gradient stripe scaling");
    check(toNative(16)==24,"small glyph and card coordinates");
    check(toNative(52)==78,"title font");
    check(toNative(30)==45,"heading font");
    check(toNative(1280)==1920 && toNative(720)==1080,"full canvas");
    std::cout << "Full HD coordinate conversion passed\n";
}
