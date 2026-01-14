#include "win32.hpp"

#include <iostream>

namespace noterm ::detail {
    PseudoConsoleFunctions PseudoConsoleFunctions::g_pfns;

    void load_pseudo_console_functions(bool* error) {
        HMODULE hModule = LoadLibrary("kernel32.dll");
        if (!hModule) {
            if (error) *error = true;
            return;
        }

        PseudoConsoleFunctions functions;
        functions.CreatePseudoConsole = reinterpret_cast<pfn_CreatePseudoConsole>(GetProcAddress(hModule, "CreatePseudoConsole"));
        functions.ClosePseudoConsole = reinterpret_cast<pfn_ClosePseudoConsole>(GetProcAddress(hModule, "ClosePseudoConsole"));
        functions.ResizePseudoConsole = reinterpret_cast<pfn_ResizePseudoConsole>(GetProcAddress(hModule, "ResizePseudoConsole"));

        if (!functions.CreatePseudoConsole || !functions.ClosePseudoConsole || !functions.ResizePseudoConsole) {
            if (error) *error = true;
            return;
        }

        if (error) *error = false;
        PseudoConsoleFunctions::g_pfns = functions;
    }

    void PseudoConsole::init(std::string_view proc_name, int cols, int rows) {
        if (m_running) {
            // close existing pseudo console
            close();
        }

        m_input_queue = InputQueue(1024);
        m_output_queue = OutputQueue(1024);

        SECURITY_ATTRIBUTES sa = {
                sizeof(SECURITY_ATTRIBUTES),
                nullptr,
                TRUE,
        };
        CreatePipe(&m_hInRead, &m_hInWrite, &sa, 0);
        CreatePipe(&m_hOutRead, &m_hOutWrite, &sa, 0);

        create_pseudo_console(
                {static_cast<SHORT>(cols), static_cast<SHORT>(rows)},
                m_hInRead,
                m_hOutWrite,
                0,
                &m_hPC);

        CloseHandle(m_hInRead);
        CloseHandle(m_hOutWrite);

        STARTUPINFOEXA si = {};
        si.StartupInfo.cb = sizeof(STARTUPINFOEXA);
        SIZE_T attr_size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
        si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, attr_size));
        InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attr_size);
        UpdateProcThreadAttribute(
                si.lpAttributeList,
                0,
                PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                m_hPC,
                sizeof(HPCON),
                nullptr,
                nullptr);

        PROCESS_INFORMATION pi = {};
        CreateProcessA(
                nullptr,
                (char*) proc_name.data(),
                nullptr,
                nullptr,
                FALSE,
                EXTENDED_STARTUPINFO_PRESENT,
                nullptr,
                nullptr,
                reinterpret_cast<LPSTARTUPINFOA>(&si),
                &pi);

        m_running = true;

        m_hInputThread = CreateThread(nullptr, 0, input_thread_proc, this, 0, nullptr);
        m_hOutputThread = CreateThread(nullptr, 0, output_thread_proc, this, 0, nullptr);

        std::cout << "PseudoConsole initialized with process: " << proc_name << std::endl;
    }

    void PseudoConsole::close() {
        m_running = false;

        if (m_hPC) {
            close_pseudo_console(m_hPC);
            m_hPC = nullptr;
        }
        if (m_hInWrite) {
            CloseHandle(m_hInWrite);
            m_hInWrite = nullptr;
        }
        if (m_hOutRead) {
            CloseHandle(m_hOutRead);
            m_hOutRead = nullptr;
        }
        if (m_hInputThread) {
            WaitForSingleObject(m_hInputThread, INFINITE);
            CloseHandle(m_hInputThread);
            m_hInputThread = nullptr;
        }
        if (m_hOutputThread) {
            WaitForSingleObject(m_hOutputThread, INFINITE);
            CloseHandle(m_hOutputThread);
            m_hOutputThread = nullptr;
        }
    }

    DWORD PseudoConsole::input_thread_proc(LPVOID lpParam) {
        PseudoConsole* self = reinterpret_cast<PseudoConsole*>(lpParam);
        while (true) {
            if (!self->m_running) {
                break;
            }

            std::string input;
            if (!self->m_input_queue.wait_dequeue_timed(input, std::chrono::milliseconds(10))) {
                continue;
            }

            DWORD bytes_written;
            BOOL result = WriteFile(self->m_hInWrite, input.data(), static_cast<DWORD>(input.size()), &bytes_written, nullptr);
            if (!result || bytes_written == 0) {
                break;
            }
        }
        return 0;
    }

    DWORD PseudoConsole::output_thread_proc(LPVOID lpParam) {
        PseudoConsole* self = reinterpret_cast<PseudoConsole*>(lpParam);
        char chunk[4096];
        DWORD bytes_read;

        while (true) {
            if (!self->m_running) {
                break;
            }

            BOOL result = ReadFile(self->m_hOutRead, chunk, sizeof(chunk), &bytes_read, nullptr);
            if (!result) {
                std::cerr << "ReadFile failed in output thread." << std::endl;
                break;
            }
            if (bytes_read == 0) {
                continue;
            }

            std::cout << "Read " << bytes_read << " bytes from pseudo console." << std::endl;
            std::string output(chunk, bytes_read);
            self->m_output_queue.enqueue(output);
        }
        return 0;
    }
}// namespace noterm::detail