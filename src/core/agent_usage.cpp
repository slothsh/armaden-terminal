#include <cstddef>

import std;
import agent;

namespace mts::core {

auto langgraphFacadeSizes() -> std::size_t {
    return sizeof(mts::agent::BaseMessage) + sizeof(mts::agent::ToolSpec);
}

} // END OF NAMESPACE `mts::core`
