#include "native_window.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

struct NativeWindow::Impl {
    std::string error;
    bool alive=false;
    bool quit=false;
    InputState input;
#ifdef _WIN32
    HWND window=nullptr;
    uint32_t width=0,height=0;
    std::vector<uint8_t> bgra;
    static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
        auto* self=reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE) {
            self=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(self && message==WM_DESTROY) { self->alive=false; self->window=nullptr; self->input.clear();self->quit=false;return 0; }
        if(self && message==WM_SIZE) { InvalidateRect(hwnd,nullptr,FALSE);return 0; }
        if(self && message==WM_KILLFOCUS) { self->input.clear();return 0; }
        if(self && (message==WM_KEYDOWN || message==WM_KEYUP)) {
            if(message==WM_KEYDOWN && w==0x1B) self->quit=true;
            if(self->input.key(uint32_t(w),message==WM_KEYDOWN)) return 0;
        }
        if(self && message==WM_PAINT) {
            PAINTSTRUCT paint{}; HDC dc=BeginPaint(hwnd,&paint);
            RECT area{};GetClientRect(hwnd,&area);FillRect(dc,&area,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            if(self->width && self->height && !self->bgra.empty() && area.right>0 && area.bottom>0) {
                const double scale=std::min(double(area.right)/self->width,double(area.bottom)/self->height);
                const int width=int(self->width*scale),height=int(self->height*scale);
                BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                bitmap.bmiHeader.biWidth=LONG(self->width);bitmap.bmiHeader.biHeight=-LONG(self->height);
                bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;bitmap.bmiHeader.biCompression=BI_RGB;
                SetStretchBltMode(dc,COLORONCOLOR);
                StretchDIBits(dc,(area.right-width)/2,(area.bottom-height)/2,width,height,0,0,self->width,self->height,
                    self->bgra.data(),&bitmap,DIB_RGB_COLORS,SRCCOPY);
            }
            EndPaint(hwnd,&paint);return 0;
        }
        return DefWindowProcW(hwnd,message,w,l);
    }
#endif
};
NativeWindow::NativeWindow():impl(std::make_unique<Impl>()) {}
NativeWindow::~NativeWindow() { close(); }
const std::string& NativeWindow::error() const { return impl->error; }
uint32_t NativeWindow::buttons() const { return impl->input.buttons(); }
bool NativeWindow::quitRequested() const { return impl->quit; }
bool NativeWindow::focused() const {
#ifdef _WIN32
    return impl->alive && impl->window && GetForegroundWindow()==impl->window && GetFocus()==impl->window;
#else
    return false;
#endif
}
bool NativeWindow::open(bool visible) {
    close();impl->error.clear();impl->input.clear();impl->quit=false;
#ifdef _WIN32
    impl->width=0;impl->height=0;impl->bgra.clear();
    WNDCLASSW cls{};cls.lpfnWndProc=Impl::procedure;cls.hInstance=GetModuleHandleW(nullptr);
    cls.lpszClassName=L"InstaurixDevelopmentFrame";cls.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
    if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) { impl->error="Failed to register window class";return false; }
    impl->window=CreateWindowExW(0,cls.lpszClassName,L"Instaurix - development display",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,800,520,nullptr,nullptr,cls.hInstance,impl.get());
    if(!impl->window) { impl->error="Failed to create window";return false; }
    impl->alive=true;
    if(visible) ShowWindow(impl->window,SW_SHOW);
    return true;
#else
    (void)visible;impl->error="Native window requires Windows";return false;
#endif
}
void NativeWindow::close() {
#ifdef _WIN32
    if(impl->window) DestroyWindow(impl->window);
#endif
    impl->alive=false;
    impl->input.clear();
    impl->quit=false;
}
bool NativeWindow::poll() {
#ifdef _WIN32
    MSG message{};
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
        if(message.message==WM_QUIT) { close();break; }
        TranslateMessage(&message);DispatchMessageW(&message);
    }
#endif
    return impl->alive;
}
bool NativeWindow::present(std::shared_ptr<const Frame> frame) {
    if(!impl->alive || !frame || !frame->width || !frame->height || frame->width>256 || frame->height>256 ||
        frame->rgb.size()!=uint64_t(frame->width)*frame->height*3 || frame->rgb.size()>65536) {
        impl->error="Window is closed or frame is invalid";return false;
    }
#ifdef _WIN32
    std::vector<uint8_t> converted(frame->width*frame->height*4);
    for(size_t i=0;i<frame->rgb.size()/3;++i) {
        converted[i*4]=frame->rgb[i*3+2];converted[i*4+1]=frame->rgb[i*3+1];converted[i*4+2]=frame->rgb[i*3];converted[i*4+3]=0;
    }
    impl->bgra=std::move(converted);impl->width=frame->width;impl->height=frame->height;
    InvalidateRect(impl->window,nullptr,FALSE);UpdateWindow(impl->window);
    return true;
#else
    return false;
#endif
}
bool NativeWindow::waitFor(uint64_t nanoseconds) {
    while(nanoseconds && poll()) {
        const auto before=std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::nanoseconds(std::min<uint64_t>(nanoseconds,8000000)));
        const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-before).count();
        nanoseconds-=std::min(nanoseconds,uint64_t(std::max<int64_t>(1,elapsed)));
    }
    return poll();
}
