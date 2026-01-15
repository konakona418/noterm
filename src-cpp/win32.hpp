#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00     // _WIN32_WINNT_WIN10
#define NTDDI_VERSION 0x0A000006//NTDDI_WIN10_RS5

#include <optional>
#include <string>
#include <string_view>


/* clang-format off */
#include <windows.h>
#include <wincon.h>
/* clang-format on */

#include <blockingconcurrentqueue.h>
#include <concurrentqueue.h>


namespace noterm {
    namespace detail {
        typedef HRESULT(WINAPI* pfn_CreatePseudoConsole)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
        typedef HRESULT(WINAPI* pfn_ClosePseudoConsole)(HPCON);
        typedef HRESULT(WINAPI* pfn_ResizePseudoConsole)(HPCON, COORD);

        struct PseudoConsoleFunctions {
            static PseudoConsoleFunctions g_pfns;

            pfn_CreatePseudoConsole CreatePseudoConsole = nullptr;
            pfn_ClosePseudoConsole ClosePseudoConsole = nullptr;
            pfn_ResizePseudoConsole ResizePseudoConsole = nullptr;
        };

        void load_pseudo_console_functions(bool* error);

        inline HRESULT create_pseudo_console(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC) {
            return PseudoConsoleFunctions::g_pfns.CreatePseudoConsole(size, hInput, hOutput, dwFlags, phPC);
        }

        inline HRESULT close_pseudo_console(HPCON hPC) {
            return PseudoConsoleFunctions::g_pfns.ClosePseudoConsole(hPC);
        }

        inline HRESULT resize_pseudo_console(HPCON hPC, COORD size) {
            return PseudoConsoleFunctions::g_pfns.ResizePseudoConsole(hPC, size);
        }

        struct PseudoConsole {
        public:
            using OutputQueue = moodycamel::BlockingConcurrentQueue<std::string>;
            using InputQueue = moodycamel::BlockingConcurrentQueue<std::string>;

            void init(std::string_view proc_name, int cols, int rows);

            void close();

            void set_size(int cols, int rows) {
                resize_pseudo_console(m_hPC, {static_cast<SHORT>(cols), static_cast<SHORT>(rows)});
            }

            void write_input(const std::string& input) {
                m_input_queue.enqueue(input);
            }

            std::optional<std::string> read_output() {
                std::string output;
                if (m_output_queue.wait_dequeue_timed(output, std::chrono::milliseconds(10))) {
                    return output;
                }
                return std::nullopt;
            }

            std::optional<std::string> read_output_nonblocking() {
                std::string output;
                if (m_output_queue.try_dequeue(output)) {
                    return output;
                }
                return std::nullopt;
            }

        private:
            InputQueue m_input_queue;
            OutputQueue m_output_queue;

            HPCON m_hPC = nullptr;
            HANDLE m_hInRead, m_hInWrite;
            HANDLE m_hOutRead, m_hOutWrite;

            HANDLE m_hInputThread = nullptr;
            HANDLE m_hOutputThread = nullptr;

            bool m_running = false;

            static DWORD WINAPI input_thread_proc(LPVOID lpParam);
            static DWORD WINAPI output_thread_proc(LPVOID lpParam);
        };
    }// namespace detail

    inline bool init_context() {
        bool load_error = false;
        detail::load_pseudo_console_functions(&load_error);
        return !load_error;
    }
}// namespace noterm