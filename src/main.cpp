#include "application.hpp"

auto main(int argc, char** argv) -> int {
    try {
        mts::app::Application application(argc, argv);

        return application.run();
    } catch (...) {
        return 1;
    }
}
