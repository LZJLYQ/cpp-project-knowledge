#include "logger.hpp"
#include "console_session_transport.hpp"
#include "shared_resource_pool.hpp"
#include "server_app.hpp"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <pthread.h>
#include <signal.h>
#include <string>

int main()
{
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    sigaddset(&signals, SIGHUP);

    // Block signals before starting threads so the process handles them in sigwait.
    const int mask_error = pthread_sigmask(SIG_BLOCK, &signals, nullptr);
    if (mask_error != 0) {
        log_line("failed to block signals: " + std::string(std::strerror(mask_error)));
        return EXIT_FAILURE;
    }

    // Composition root: main creates concrete dependencies and injects them.
    SharedResourcePool resource_pool;
    ConsoleSessionTransportFactory transport_factory;
    ServerApp server_app(resource_pool, transport_factory);

    try {
        // These simulated clients share one upstream resource, but each gets
        // its own session transport.
        if (!server_app.startSession("viewer-a", "camera-0")
            || !server_app.startSession("viewer-b", "camera-0")) {
            server_app.stop();
            resource_pool.shutdown();
            return EXIT_FAILURE;
        }
    } catch (const std::exception& error) {
        log_line("failed to start application: " + std::string(error.what()));
        server_app.stop();
        resource_pool.shutdown();
        return EXIT_FAILURE;
    }

    log_line("application ready; send SIGHUP to reload, SIGINT or SIGTERM to exit");

    int exit_code = EXIT_SUCCESS;
    bool shutting_down = false;
    while (!shutting_down) {
        int received_signal = 0;
        const int wait_error = sigwait(&signals, &received_signal);
        if (wait_error != 0) {
            log_line("failed to wait for signal: " + std::string(std::strerror(wait_error)));
            exit_code = EXIT_FAILURE;
            break;
        }

        switch (received_signal) {
        case SIGHUP:
            log_line("received SIGHUP");
            server_app.reload();
            break;
        case SIGINT:
        case SIGTERM:
            log_line("received shutdown signal");
            shutting_down = true;
            break;
        default:
            break;
        }
    }

    // Stop sessions first; the pool stops shared resources when their last
    // session reference is returned. shutdown() is a final cleanup safeguard.
    server_app.stop();
    resource_pool.shutdown();
    log_line("process exited");
    return exit_code;
}
