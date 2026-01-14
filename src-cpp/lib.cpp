#ifdef _WIN32
#include "win32.hpp"
#else
#error "Unsupported platform"
#endif

#include "lib.hpp"
#include <iostream>
#include <thread>

static std::mutex pty_mutex;
static noterm::detail::PseudoConsole pty;
static std::atomic_bool running = false;
static std::thread output_thread;

void cleanup() {
    std::cout << "Cleaning up PTY..." << std::endl;

    if (!running.load()) {
        std::cout << "PTY already cleaned up." << std::endl;
        return;
    }

    running.store(false);
    if (output_thread.joinable()) {
        output_thread.join();
    }
    std::lock_guard<std::mutex> lock(pty_mutex);
    pty.close();

    std::cout << "PTY cleaned up." << std::endl;
}

void webui_main(webui::window& window, webui_context ctx, int* err) {
    const char init_cb_name[] = "webui_init_terminal";
    const char resize_cb_name[] = "webui_resize_terminal";
    const char send_input_cb_name[] = "webui_send_input";
    const char ready_cb_name[] = "webui_ready";
    const char minimize_cb_name[] = "webui_minimize";
    const char close_cb_name[] = "webui_close";

    const char web_receive_output_cb_name[] = "webui_receive_output";

    bool success = noterm::init_context();
    if (!success) {
        if (err) *err = 1;
        return;
    }

    window.set_size(1280, 720);
    window.set_frameless(true);
    window.set_transparent(true);
    window.set_center();

    window.bind(init_cb_name,
                [](webui::window::event* ev) {
                    std::lock_guard<std::mutex> lock(pty_mutex);
                    pty.init("powershell.exe", ev->get_int(0), ev->get_int(1));
                });
    window.bind(resize_cb_name,
                [](webui::window::event* ev) {
                    std::lock_guard<std::mutex> lock(pty_mutex);
                    pty.set_size(ev->get_int(0), ev->get_int(1));
                });
    window.bind(send_input_cb_name,
                [](webui::window::event* ev) {
                    std::cout << "Sending input size to PTY: " << ev->get_string(0).size() << std::endl;
                    pty.write_input(ev->get_string(0));
                });
    window.bind(minimize_cb_name, [](webui::window::event* e) {
        webui_minimize(e->window);
    });
    window.bind(close_cb_name, [](webui::window::event* e) {
        webui_close(e->window);
        cleanup();
    });


    window.bind("", [](webui::window::event* e) {
        if (e->event_type == WEBUI_EVENT_CONNECTED) {
            std::cout << "Client connected." << std::endl;
            e->get_window().run("webui_ready();");
        } else if (e->event_type == WEBUI_EVENT_DISCONNECTED) {
            std::cout << "Client disconnected." << std::endl;
            cleanup();
        }
    });

    running.store(true);
    output_thread = std::thread([&window, web_receive_output_cb_name]() {
        while (running.load()) {
            auto received_output = pty.read_output();
            if (!received_output.has_value()) {
                std::this_thread::yield();
                continue;
            }

            std::cout << "Received output size: " << received_output->size() << std::endl;
            window.send_raw(web_receive_output_cb_name, received_output->data(), received_output->size());
        }
    });
}