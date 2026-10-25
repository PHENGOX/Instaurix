#include "machine.hpp"
#include <charconv>
#include <iostream>
#include <string_view>
#include "native_window.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Switch2Emu 0.86 - prototype ARM64\nUsage: Switch2Emu programme.bin|programme.elf|programme.nro [nombre_instructions] [--test-protocol] [--hle-reference] [--homebrew-entry] [--read-root dossier] [--frame-out image.png] [--frames-out dossier] [--window]\n";
        return 1;
    }
    uint64_t budget = 100000;
    bool testProtocol = false;
    bool budgetSpecified = false;
    bool referenceHle = false;
    bool homebrewEntry = false;
    const char* readRoot = nullptr;
    const char* frameOut = nullptr;
    const char* framesOut = nullptr;
    bool window = false;
    for (int i = 2; i < argc; ++i)
    {
        std::string_view value(argv[i]);
        if (value == "--test-protocol" && !testProtocol) { testProtocol = true; continue; }
        if (value == "--hle-reference" && !referenceHle) { referenceHle = true; continue; }
        if(value=="--homebrew-entry" && !homebrewEntry) { homebrewEntry=true;continue; }
        if(value=="--window" && !window) { window=true;continue; }
        if(value=="--frames-out") {
            if(framesOut || i+1>=argc) { std::cerr<<"--frames-out exige un seul dossier\n"; return 1; }
            framesOut=argv[++i];continue;
        }
        if(value=="--read-root") {
            if(readRoot || i+1>=argc) { std::cerr<<"--read-root exige un seul dossier\n"; return 1; }
            readRoot=argv[++i]; continue;
        }
        if(value=="--frame-out") {
            if(frameOut || i+1>=argc) { std::cerr<<"--frame-out exige un seul fichier\n"; return 1; }
            frameOut=argv[++i]; continue;
        }
        auto result = std::from_chars(value.data(), value.data() + value.size(), budget);
        if (budgetSpecified || result.ec != std::errc{} || result.ptr != value.data() + value.size() || budget == 0)
        {
            std::cerr << "Nombre d'instructions invalide\n";
            return 1;
        }
        budgetSpecified = true;
    }
    if (testProtocol && referenceHle)
    {
        std::cerr << "Choisir un seul mode : --test-protocol ou --hle-reference\n";
        return 1;
    }
    if(homebrewEntry && (!referenceHle || testProtocol)) { std::cerr<<"--homebrew-entry exige --hle-reference et exclut --test-protocol\n";return 1; }
    Machine machine(testProtocol,referenceHle,homebrewEntry);
    if((frameOut || framesOut || window) && !referenceHle) { std::cerr<<"La presentation d'images exige --hle-reference\n"; return 1; }
    if(readRoot && !machine.mountReadOnlyRoot(readRoot)) { std::cerr<<machine.error()<<'\n'; return 1; }
    if (!machine.load(argv[1])) { std::cerr << machine.error() << '\n'; return 1; }
    std::ofstream manifest;
    if(framesOut) {
        std::error_code error;std::filesystem::create_directories(framesOut,error);
        if(error || !std::filesystem::is_directory(framesOut)) { std::cerr<<"Dossier de sequence inaccessible\n";return 1; }
        manifest.open(std::filesystem::path(framesOut)/"frames.csv",std::ios::trunc);
        if(!manifest) { std::cerr<<"Journal de sequence inaccessible\n";return 1; }
        manifest<<"frame,time_ns,instructions,width,height,hash\n";
    }
    NativeWindow viewer;
    if(window && !viewer.open()) { std::cerr<<viewer.error()<<'\n';return 1; }
    Machine::Result result{Machine::Stop::Budget,0};
    uint64_t total=0, lastPresented=0;
    bool closed=false;
    while(total<budget) {
        if(window && !viewer.poll()) { closed=true;break; }
        if(window) machine.setButtons(viewer.buttons() | (viewer.quitRequested()?InputState::Quit:0));
        if(window) machine.setAppletFocus(viewer.focused());
        result=machine.run(window ? std::min<uint64_t>(4096,budget-total) : budget-total,window || framesOut);
        total += result.instructions;
        if((window || framesOut) && machine.process().frameCount()!=lastPresented) {
            const auto serial=machine.process().frameCount();
            if(serial!=lastPresented+1) { std::cerr<<"Sequence d'images incomplete\n";return 1; }
            const auto& frame=machine.process().frame();
            if(window && !viewer.present(frame)) { std::cerr<<viewer.error()<<'\n';return 1; }
            if(framesOut) {
                std::ostringstream filename;filename<<"frame-"<<std::setw(6)<<std::setfill('0')<<serial<<".png";
                std::string error;
                if(!frame->savePng(std::filesystem::path(framesOut)/filename.str(),error)) { std::cerr<<error<<'\n';return 1; }
                uint32_t hash=2166136261U;for(auto byte:frame->rgb) hash=(hash^byte)*16777619U;
                manifest<<serial<<','<<machine.process().virtualTime()<<','<<total<<','<<frame->width<<','<<frame->height<<','<<hash<<'\n';
            }
            lastPresented=serial;
        }
        if(result.stop==Machine::Stop::Frame || result.stop==Machine::Stop::Budget) continue;
        // Advance only idle virtual time; optional wall pacing belongs to the viewer.
        if(referenceHle && result.stop==Machine::Stop::Waiting && total<budget) {
            const auto delay=machine.process().nextWakeDelay();
            if(delay) {
                if(window && !viewer.waitFor(*delay)) { closed=true;break; }
                if(machine.advanceTime(*delay)) continue;
            }
            else if(window) { if(!viewer.waitFor(16000000)) { closed=true;break; }continue; }
        }
        break;
    }
    result.instructions = total;
    if(framesOut) {
        manifest.flush();
        if(!manifest) { std::cerr<<"Ecriture du journal de sequence impossible\n";return 1; }
        std::cout<<"Images exportees="<<lastPresented<<" TempsVirtuelNs="<<machine.process().virtualTime()<<'\n';
    }
    std::cout << "Instructions=" << result.instructions << " PC=0x" << std::hex
              << machine.cpu.getPC() << std::dec << " X0=" << machine.cpu.readRegister(0)
              << " X1=" << machine.cpu.readRegister(1) << " X2=" << machine.cpu.readRegister(2) << '\n';
    if (result.stop == Machine::Stop::Error) { std::cerr << machine.error() << '\n'; return 1; }
    if(closed) { std::cout<<"Fenetre fermee - execution interrompue\n";return 0; }
    if(frameOut) {
        const auto& frame=machine.process().frame();
        std::string error;
        if(!frame) { std::cerr<<"Aucune image presentee\n"; return 1; }
        if(!frame->savePng(frameOut,error)) { std::cerr<<error<<'\n'; return 1; }
        std::cout<<"Image exportee : "<<frameOut<<" ("<<frame->width<<"x"<<frame->height<<")\n";
    }
    if (result.stop == Machine::Stop::Exited)
    {
        std::cout << "Fin normale (" << (referenceHle ? "HLE de reference" : "protocole de test") << "), code=" << result.exitCode << '\n';
        if(window) while(!viewer.quitRequested() && viewer.waitFor(16000000)) {}
        return result.exitCode == 0 ? 0 : 1;
    }
    std::cout << (result.stop == Machine::Stop::Waiting ? "En attente d'un evenement systeme\n" :
        result.stop == Machine::Stop::Interrupt ? "Arret sur interruption active\n" : "Budget atteint\n");
    return 0;
}
