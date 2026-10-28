#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string_view>
int main(int argc,char** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    std::cout<<"Diagnostic probe started\n"<<std::flush;
    if(argc==2 && std::string_view(argv[1])=="crash")RaiseException(0xE0424242,EXCEPTION_NONCONTINUABLE,0,nullptr);
    if(argc==2 && std::string_view(argv[1])=="fail")return 7;
    if(argc!=4 || std::string_view(argv[1])!="space argument" || std::string_view(argv[2])!="quote\"argument" || std::string_view(argv[3])!="trailing\\")return 8;
    std::cout<<"Diagnostic argument round-trip passed\n";return 0;
}
