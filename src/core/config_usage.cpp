import std;
import config;

namespace mts::core {
auto configProbe(std::string_view jsonText) -> mts::config::AppConfig {
    auto const parsed = mts::config::parseConfig(jsonText);
    if (!parsed)
        return mts::config::defaultConfig();
    return parsed.value();
}
}
