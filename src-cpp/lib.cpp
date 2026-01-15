#ifdef _WIN32
#include "win32.hpp"
#else
#error "Unsupported platform"
#endif

#include "lib.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#define INIT_CB_NAME "webui_init_terminal"
#define RESIZE_CB_NAME "webui_resize_terminal"
#define SEND_INPUT_CB_NAME "webui_send_input"
#define PULL_OUTPUT_CB_NAME "webui_pull_output"
#define CREATED_CB_NAME "webui_created_pty"
#define NOTIFY_OUTPUT_CB_NAME "webui_output_available"
#define WEB_RECEIVE_OUTPUT_CB_NAME "webui_receive_output"
#define READY_CB_NAME "webui_ready"
#define CLOSE_PTY_CB_NAME "webui_close_pty"
#define MINIMIZE_CB_NAME "webui_minimize"
#define CLOSE_CB_NAME "webui_close"

namespace {
    class PTYManager {
    public:
        static PTYManager& instance() {
            static PTYManager mgr;
            return mgr;
        }

        int create(const std::string& command, int cols, int rows) {
            std::lock_guard<std::mutex> lock(m_mutex);
            int id = ++m_last_id;
            auto p = std::make_unique<noterm::detail::PseudoConsole>();
            p->init(command.c_str(), cols, rows);
            m_consoles[id] = std::move(p);
            return id;
        }

        void set_size(int id, int cols, int rows) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_consoles.find(id);
            if (it != m_consoles.end()) it->second->set_size(cols, rows);
        }

        void write_input(int id, const std::string& s) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_consoles.find(id);
            if (it != m_consoles.end()) it->second->write_input(s);
        }

        // non-blocking read: returns optional vector if data available
        std::optional<std::string> read_output_nonblocking(int id) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_consoles.find(id);
            if (it == m_consoles.end()) return std::nullopt;
            return it->second->read_output();
        }

        // stage output for later pull
        bool stage_output_if_available(int id) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_consoles.find(id);
            if (it == m_consoles.end()) return false;
            auto out = it->second->read_output_nonblocking();
            if (out.has_value() && !out->empty()) {
                // append to any previously staged data so we don't overwrite
                auto& dest = m_staged_outputs[id];
                dest.append(*out);
            }
            return m_staged_outputs.find(id) != m_staged_outputs.end() && !m_staged_outputs[id].empty();
        }

        // Pull staged output if present; otherwise read directly from PTY (fallback)
        std::optional<std::string> pull_output(int id) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto sit = m_staged_outputs.find(id);
            if (sit != m_staged_outputs.end()) {
                std::string s = std::move(sit->second);
                m_staged_outputs.erase(sit);
                return s;
            }
            auto it = m_consoles.find(id);
            if (it == m_consoles.end()) return std::nullopt;
            return it->second->read_output_nonblocking();
        }

        void close_all() {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& kv: m_consoles) {
                kv.second->close();
            }
            m_consoles.clear();
            m_staged_outputs.clear();
        }

        // Close and remove a single PTY by id
        void close(int id) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_consoles.find(id);
            if (it != m_consoles.end()) {
                it->second->close();
                m_consoles.erase(it);
            }
            // remove any staged output for this id
            m_staged_outputs.erase(id);
        }

        // iterate IDs
        std::vector<int> ids() {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::vector<int> out;
            out.reserve(m_consoles.size());
            for (auto& kv: m_consoles) out.push_back(kv.first);
            return out;
        }

    private:
        PTYManager() = default;
        ~PTYManager() = default;
        std::mutex m_mutex;
        std::map<int, std::unique_ptr<noterm::detail::PseudoConsole>> m_consoles;
        std::map<int, std::string> m_staged_outputs;
        int m_last_id = 0;
    };
}// namespace

static std::atomic_bool running = false;
static std::thread monitor_thread;

void cleanup() {
    std::cout << "Cleaning up PTYs..." << std::endl;

    if (!running.load()) {
        std::cout << "PTYs already cleaned up." << std::endl;
        return;
    }

    running.store(false);
    if (monitor_thread.joinable()) monitor_thread.join();
    PTYManager::instance().close_all();

    std::cout << "PTYs cleaned up." << std::endl;
}

void webui_main(webui::window& window, webui_context ctx, int* err) {
    bool success = noterm::init_context();
    if (!success) {
        if (err) *err = 1;
        return;
    }

    window.set_size(1280, 720);
    window.set_frameless(true);
    window.set_transparent(true);
    window.set_resizable(false);
    window.set_center();

    // init: create a new PTY and notify frontend with its id and request token
    // webui_init_terminal(cols, rows, token)
    window.bind(INIT_CB_NAME, [](webui::window::event* ev) {
        int cols = ev->get_int(0);
        int rows = ev->get_int(1);
        int token = ev->get_int(2);
        // optional 4th argument: command to run (e.g., "powershell.exe" or "bash")
        std::string command = "powershell.exe";
        if (ev->get_count() > 3) {
            std::string c = ev->get_string(3);
            if (!c.empty()) command = c;
        }
        int id = PTYManager::instance().create(command, cols, rows);
        // Call back into JS with both id and token: webui_created_pty(id, token)
        char created_buf[128];
        std::snprintf(created_buf, sizeof(created_buf), CREATED_CB_NAME "(%d, %d);", id, token);
        ev->get_window().run(created_buf);
    });

    // resize: expects (id, cols, rows)
    window.bind(RESIZE_CB_NAME, [](webui::window::event* ev) {
        int id = ev->get_int(0);
        int cols = ev->get_int(1);
        int rows = ev->get_int(2);
        PTYManager::instance().set_size(id, cols, rows);
    });

    // send input: expects (id, inputString)
    window.bind(SEND_INPUT_CB_NAME, [](webui::window::event* ev) {
        int id = ev->get_int(0);
        std::string s = ev->get_string(1);
        std::cout << "Sending input to PTY " << id << ", size: " << s.size() << std::endl;
        PTYManager::instance().write_input(id, s);
    });

    // pull output: frontend calls this to request the buffered output for a PTY id
    window.bind(PULL_OUTPUT_CB_NAME, [](webui::window::event* ev) {
        int id = ev->get_int(0);
        auto out = PTYManager::instance().pull_output(id);
        if (out.has_value() && !out->empty()) {
            // prepend 4-byte little-endian id header so frontend can route bytes to correct PTY
            int data_len = static_cast<int>(out->size());
            std::vector<char> buf(4 + data_len);
            // write id as little-endian 32-bit
            buf[0] = static_cast<char>(id & 0xFF);
            buf[1] = static_cast<char>((id >> 8) & 0xFF);
            buf[2] = static_cast<char>((id >> 16) & 0xFF);
            buf[3] = static_cast<char>((id >> 24) & 0xFF);
            memcpy(buf.data() + 4, out->data(), data_len);
            ev->get_window().send_raw(WEB_RECEIVE_OUTPUT_CB_NAME, buf.data(), static_cast<int>(buf.size()));
        }
    });

    // close a specific PTY: expects (id)
    window.bind(CLOSE_PTY_CB_NAME, [](webui::window::event* ev) {
        int id = ev->get_int(0);
        PTYManager::instance().close(id);
    });

    window.bind(MINIMIZE_CB_NAME, [](webui::window::event* e) { webui_minimize(e->window); });
    window.bind(CLOSE_CB_NAME, [](webui::window::event* e) { webui_close(e->window); cleanup(); });

    // connection events
    window.bind("", [](webui::window::event* e) {
        if (e->event_type == WEBUI_EVENT_CONNECTED) {
            std::cout << "Client connected." << std::endl;
            e->get_window().run(READY_CB_NAME "();");
        } else if (e->event_type == WEBUI_EVENT_DISCONNECTED) {
            std::cout << "Client disconnected." << std::endl;
            cleanup();
        }
    });

    // monitor and notify frontend of output availability
    running.store(true);
    monitor_thread = std::thread([&window]() {
        while (running.load()) {
            auto ids = PTYManager::instance().ids();
            for (int id: ids) {
                // Stage available output without consuming it for the frontend
                if (PTYManager::instance().stage_output_if_available(id)) {
                    // Notify frontend that this PTY has output available; frontend should call webui_pull_output(id)
                    char notify_buf[64];
                    std::snprintf(notify_buf, sizeof(notify_buf), NOTIFY_OUTPUT_CB_NAME "(%d);", id);
                    window.run(notify_buf);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}