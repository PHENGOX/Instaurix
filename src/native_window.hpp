#pragma once
#include "frame.hpp"
#include <memory>
#include "input_state.hpp"

class NativeWindow {
public:
    NativeWindow();
    ~NativeWindow();
    bool open(bool visible = true);
    bool poll();
    bool present(std::shared_ptr<const Frame> frame);
    bool waitFor(uint64_t nanoseconds);
    uint32_t buttons() const;
    bool quitRequested() const;
    bool focused() const;
    void close();
    const std::string& error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
