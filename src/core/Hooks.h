#pragma once

#include <atomic>
#include <cstdint>
#include <windows.h>

class Recorder;

class Hooks {
public:
    Hooks();
    ~Hooks();

    Hooks(const Hooks&) = delete;
    Hooks& operator=(const Hooks&) = delete;

    bool Install(Recorder* recorder);
    void Uninstall();
    bool IsInstalled() const;

private:
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK KeyProc(int nCode, WPARAM wParam, LPARAM lParam);

    void OnMouse(WPARAM wParam, const MSLLHOOKSTRUCT& ms);
    void OnKey(WPARAM wParam, const KBDLLHOOKSTRUCT& ks);
    int64_t NextDeltaMicros();

    HHOOK mouse_{ nullptr };
    HHOOK key_{ nullptr };

    Recorder* recorder_{ nullptr };
    std::atomic<int64_t> lastQpc_{ 0 };
};
