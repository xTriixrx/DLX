#include "performance_test_config.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace
{

constexpr const char kDefaultConfigPath[] = "tests/config/performance_config.yaml";

PerformanceTestConfig load_config()
{
    PerformanceTestConfig config;
    const char* override_path = std::getenv("DLX_PERF_CONFIG");
    config.source_path = override_path != nullptr ? override_path : kDefaultConfigPath;

    std::filesystem::path config_path(config.source_path);
    YAML::Node root;
    auto try_load = [&](const std::filesystem::path& path) -> bool {
        try
        {
            root = YAML::LoadFile(path.string());
            config.source_path = path.string();
            return true;
        }
        catch (const YAML::Exception&)
        {
            return false;
        }
    };

    if (!try_load(config_path))
    {
#ifdef DLX_SOURCE_DIR
        if (config_path.is_relative())
        {
            std::filesystem::path fallback = std::filesystem::path(DLX_SOURCE_DIR) / config_path;
            try_load(fallback);
        }
#endif
    }

    if (!root || root.IsNull())
    {
        config.config_loaded = false;
        return config;
    }

    config.config_loaded = true;

    YAML::Node tests_node = root["tests"];
    if (!tests_node || !tests_node.IsMap())
    {
        return config;
    }

    YAML::Node search_node = tests_node["search_performance"];
    YAML::Node network_node = tests_node["network_performance"];

    auto assign_bool = [&](const YAML::Node& node, const char* key, bool& field) {
        if (node)
        {
            if (const YAML::Node value = node[key])
            {
                field = value.as<bool>(field);
            }
        }
    };

    auto assign_positive_uint = [&](const YAML::Node& node, const char* key, uint32_t& field) {
        if (node)
        {
            if (const YAML::Node value = node[key])
            {
                const uint32_t parsed = value.as<uint32_t>(field);
                field = std::max<uint32_t>(1, parsed);
            }
        }
    };

    auto assign_string = [&](const YAML::Node& node, const char* key, std::string& field) {
        if (node)
        {
            if (const YAML::Node value = node[key])
            {
                const std::string parsed = value.as<std::string>(field);
                if (!parsed.empty())
                {
                    field = parsed;
                }
            }
        }
    };

    assign_bool(search_node, "enabled", config.search_performance_enabled);
    assign_string(search_node, "report_path", config.search_report_path);
    if (search_node)
    {
        const YAML::Node cases_node = search_node["cases"];
        if (cases_node && cases_node.IsSequence())
        {
            std::vector<SearchPerformanceCase> parsed;
            for (const YAML::Node& case_node : cases_node)
            {
                if (!case_node || !case_node.IsMap())
                {
                    continue;
                }
                SearchPerformanceCase entry{};
                auto read_case_uint = [&](const char* key, uint32_t& field) -> bool {
                    if (const YAML::Node value = case_node[key])
                    {
                        field = std::max<uint32_t>(1, value.as<uint32_t>());
                        return true;
                    }
                    return false;
                };
                bool ok = read_case_uint("column_count", entry.column_count);
                ok = read_case_uint("group_count", entry.group_count) && ok;
                ok = read_case_uint("variants_per_group", entry.variants_per_group) && ok;
                if (ok)
                {
                    parsed.push_back(entry);
                }
            }
            if (!parsed.empty())
            {
                config.search_cases = std::move(parsed);
            }
        }
    }
    assign_bool(network_node, "enabled", config.network_performance_enabled);
    assign_positive_uint(network_node, "duration_seconds", config.network_duration_seconds);
    assign_positive_uint(network_node, "request_clients", config.network_request_clients);
    assign_positive_uint(network_node, "solution_clients", config.network_solution_clients);
    assign_positive_uint(network_node, "target_solution_rate", config.network_target_solution_rate);
    assign_string(network_node, "problem_file", config.network_problem_file);
    assign_string(network_node, "report_path", config.network_report_path);

    return config;
}

} // namespace

const PerformanceTestConfig& GetPerformanceTestConfig()
{
    static PerformanceTestConfig config = load_config();
    return config;
}
