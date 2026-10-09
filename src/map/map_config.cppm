export module map:config;

import std;
import foundation;
import config;

export namespace mts::map {

struct MapConfig {
    std::string configPath = "config/maps/everon.json";
};

[[nodiscard]] inline auto applyConfig(mts::config::ConfigView const& view, MapConfig& target) -> mts::foundation::Status {
    return view.section("map").readString("configPath", target.configPath);
}

} // END OF NAMESPACE `mts::map`
