#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <chrono>
#include <cctype>
#include <algorithm>
#include <stdexcept>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

// Embedded file contents configured via CMake
#include "manifests.h"

int main() {
    std::cout << "=========================================\n";
    std::cout << "Chkoupi-lang VS Code Extension Installer\n";
    std::cout << "=========================================\n";

    std::filesystem::path parent_extensions_dir;
    const char* user_profile = std::getenv("USERPROFILE");
    if (user_profile) {
        parent_extensions_dir = std::filesystem::path(user_profile) / ".vscode" / "extensions";
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            parent_extensions_dir = std::filesystem::path(home) / ".vscode" / "extensions";
        } else {
            std::cerr << "ERROR: Could not find user profile directory (USERPROFILE or HOME).\n";
            return 1;
        }
    }

    std::filesystem::path extensions_dir = parent_extensions_dir / "dzxpert.chkoupi-lang-1.0.0";

    try {
        // 1. Create extension directory structure
        std::filesystem::create_directories(extensions_dir / "syntaxes");

        auto write_file = [](const std::filesystem::path& path, const std::string& content) {
            std::filesystem::path temp_path = path.parent_path() / (path.filename().string() + ".tmp");
            std::ofstream out(temp_path, std::ios::out | std::ios::trunc);
            if (!out) {
                throw std::runtime_error("Could not write to temporary file: " + temp_path.string());
            }
            out << content;
            if (!out) {
                out.close();
                std::filesystem::remove(temp_path);
                throw std::runtime_error("Failed writing content to: " + temp_path.string());
            }
            out.close();
            std::filesystem::rename(temp_path, path);
        };

        write_file(extensions_dir / "package.json", package_json);
        write_file(extensions_dir / "language-configuration.json", language_configuration_json);
        write_file(extensions_dir / "syntaxes" / "chkoupi.tmLanguage.json", chkoupi_tmLanguage_json);

        std::cout << "SUCCESS: Extension files copied to: " << extensions_dir.string() << "\n";

        // 2. Automatically register extension in extensions.json
        std::filesystem::path registry_path = parent_extensions_dir / "extensions.json";
        std::string json_content = "";

        if (std::filesystem::exists(registry_path)) {
            std::ifstream in(registry_path);
            if (in) {
                std::string line;
                while (std::getline(in, line)) {
                    json_content += line + "\n";
                }
            }
        }

        // Clean registry content and trim
        auto trim = [](std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), s.end());
        };
        trim(json_content);

        // Find existing registration and replace or register
        bool already_registered = false;
        bool needs_update = false;
        size_t target_start = std::string::npos;
        size_t target_end = std::string::npos;

        int brace_depth = 0;
        bool in_string = false;
        size_t current_object_start = std::string::npos;

        for (size_t i = 0; i < json_content.size(); ++i) {
            char c = json_content[i];
            if (c == '"' && (i == 0 || json_content[i - 1] != '\\')) {
                in_string = !in_string;
            }
            if (!in_string) {
                if (c == '{') {
                    if (brace_depth == 0) {
                        current_object_start = i;
                    }
                    brace_depth++;
                } else if (c == '}') {
                    brace_depth--;
                    if (brace_depth == 0 && current_object_start != std::string::npos) {
                        std::string obj_str = json_content.substr(current_object_start, i - current_object_start + 1);
                        if (obj_str.find("dzxpert.chkoupi-lang") != std::string::npos) {
                            target_start = current_object_start;
                            target_end = i;
                            break;
                        }
                        current_object_start = std::string::npos;
                    }
                }
            }
        }

        uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        std::string path_uri = extensions_dir.generic_string();
        if (path_uri.size() >= 2 && path_uri[1] == ':') {
            path_uri[0] = std::tolower(path_uri[0]);
            path_uri = "/" + path_uri;
        }

        std::string new_entry = "{\"identifier\":{\"id\":\"dzxpert.chkoupi-lang\"},\"version\":\"1.0.0\",\"location\":{\"$mid\":1,\"path\":\"" + path_uri + "\",\"scheme\":\"file\"},\"relativeLocation\":\"dzxpert.chkoupi-lang-1.0.0\",\"metadata\":{\"installedTimestamp\":" + std::to_string(timestamp) + ",\"pinned\":false,\"source\":\"user\",\"id\":\"dzxpert.chkoupi-lang\"}}";

        if (target_start != std::string::npos) {
            std::string obj_str = json_content.substr(target_start, target_end - target_start + 1);
            if (obj_str.find("\"version\":\"1.0.0\"") != std::string::npos && 
                obj_str.find("\"relativeLocation\":\"dzxpert.chkoupi-lang-1.0.0\"") != std::string::npos) {
                already_registered = true;
            } else {
                needs_update = true;
            }
        }

        if (already_registered) {
            std::cout << "Extension is already registered in extensions.json.\n";
        } else if (needs_update) {
            std::cout << "Updating existing extension registration in extensions.json...\n";
            json_content.replace(target_start, target_end - target_start + 1, new_entry);
            write_file(registry_path, json_content);
            std::cout << "SUCCESS: Extension registration updated in extensions.json!\n";
        } else {
            std::cout << "Registering extension in extensions.json...\n";
            if (json_content.empty() || json_content == "[]" || json_content == "[]\n") {
                json_content = "[\n  " + new_entry + "\n]";
            } else {
                size_t last_bracket = json_content.find_last_of(']');
                if (last_bracket != std::string::npos) {
                    json_content.insert(last_bracket, "," + new_entry);
                } else {
                    json_content = "[\n  " + new_entry + "\n]";
                }
            }
            write_file(registry_path, json_content);
            std::cout << "SUCCESS: Extension registered in extensions.json!\n";
        }

        std::cout << "SUCCESS: Chkoupi-lang extension installed successfully!\n";
        std::cout << "Please restart or reload VS Code to see changes.\n";
        std::cout << "=========================================\n";
        
#ifdef _WIN32
        std::cout << "Press Enter to exit...";
        std::cin.get();
#endif
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Installation failed: " << e.what() << "\n";
#ifdef _WIN32
        std::cout << "Press Enter to exit...";
        std::cin.get();
#endif
        return 1;
    }
}
