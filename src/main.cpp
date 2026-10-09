#include "application.hpp"

import std;

auto main(int argc, char** argv) -> int {
    try {
        mts::app::Application application(argc, argv);

        return application.run();
    } catch (std::exception const& error) {
        mts::log::critical("Uncaught exception escaped the application: {}", error.what());

        return 1;
    } catch (...) {
        mts::log::critical("Uncaught unknown exception escaped the application");

        return 1;
    }
}
