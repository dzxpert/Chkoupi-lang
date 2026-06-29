#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <chrono>
#include <cctype>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

// Embedded file contents
const std::string package_json = R"JSON({
    "name": "chkoupi-lang",
    "displayName": "Chkoupi-lang",
    "description": "Syntax highlighting and language support for Chkoupi-lang",
    "version": "1.0.0",
    "publisher": "dzxpert",
    "engines": {
        "vscode": "^1.60.0"
    },
    "categories": [
        "Programming Languages"
    ],
    "contributes": {
        "languages": [{
            "id": "chkoupi",
            "aliases": ["Chkoupi", "chkoupi"],
            "extensions": [".dz"],
            "configuration": "./language-configuration.json"
        }],
        "grammars": [{
            "language": "chkoupi",
            "scopeName": "source.dz",
            "path": "./syntaxes/chkoupi.tmLanguage.json"
        }]
    }
})JSON";

const std::string language_configuration_json = R"JSON({
    "comments": {
        "lineComment": "//",
        "blockComment": ["/*", "*/"]
    },
    "brackets": [
        ["{", "}"],
        ["[", "]"],
        ["(", ")"]
    ],
    "autoClosingPairs": [
        {"open": "{", "close": "}"},
        {"open": "[", "close": "]"},
        {"open": "(", "close": ")"},
        {"open": "\"", "close": "\""},
        {"open": "'", "close": "'"}
    ],
    "surroundingPairs": [
        ["{", "}"],
        ["[", "]"],
        ["(", ")"],
        ["\"", "\""],
        ["'", "'"]
    ]
})JSON";

const std::string chkoupi_tmLanguage_json = R"JSON({
    "$schema": "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
    "name": "Chkoupi",
    "patterns": [
        {
            "include": "#comments"
        },
        {
            "include": "#strings"
        },
        {
            "include": "#numbers"
        },
        {
            "include": "#keywords"
        },
        {
            "include": "#types"
        }
    ],
    "repository": {
        "comments": {
            "patterns": [
                {
                    "name": "comment.line.double-slash.dz",
                    "match": "//.*$"
                },
                {
                    "name": "comment.block.dz",
                    "begin": "/\\*",
                    "end": "\\*/"
                }
            ]
        },
        "strings": {
            "patterns": [
                {
                    "name": "string.quoted.double.dz",
                    "begin": "\"",
                    "end": "\"",
                    "patterns": [
                        {
                            "name": "constant.character.escape.dz",
                            "match": "\\\\."
                        }
                    ]
                },
                {
                    "name": "string.quoted.single.dz",
                    "begin": "'",
                    "end": "'",
                    "patterns": [
                        {
                            "name": "constant.character.escape.dz",
                            "match": "\\\\."
                        }
                    ]
                }
            ]
        },
        "numbers": {
            "patterns": [
                {
                    "name": "constant.numeric.float.dz",
                    "match": "\\b\\d+\\.\\d+\\b"
                },
                {
                    "name": "constant.numeric.integer.dz",
                    "match": "\\b\\d+\\b"
                }
            ]
        },
        "keywords": {
            "patterns": [
                {
                    "name": "keyword.control.dz",
                    "match": "\\b(idha|idha_mknch|ab9a_dor|dor|a7bss|kml|bdl|khyr|jarb|ila_ghalt|raja3|jibli)\\b"
                },
                {
                    "name": "keyword.other.dz",
                    "match": "\\b(dir|dima|dalla|9aleb|had|kssr)\\b"
                },
                {
                    "name": "support.function.builtin.dz",
                    "match": "\\b(ektb|a9ra|tool)\\b"
                },
                {
                    "name": "keyword.operator.logical.dz",
                    "match": "\\b(w|wla|machi)\\b"
                },
                {
                    "name": "constant.language.boolean.dz",
                    "match": "\\b(sa7|ghalt)\\b"
                }
            ]
        },
        "types": {
            "patterns": [
                {
                    "name": "storage.type.dz",
                    "match": "\\b(tabi3i|3ouchri|5iyar|fargh|7arf|nass|jadwl)\\b"
                },
                {
                    "name": "entity.name.type.dz",
                    "match": "\\b[A-Z][a-zA-Z0-9_]*\\b"
                }
            ]
        }
    },
    "scopeName": "source.dz"
})JSON";

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
            std::ofstream out(path, std::ios::out | std::ios::trunc);
            if (!out) {
                throw std::runtime_error("Could not write file: " + path.string());
            }
            out << content;
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

        if (json_content.find("dzxpert.chkoupi-lang") == std::string::npos) {
            std::cout << "Registering extension in extensions.json...\n";

            uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            std::string path_uri = extensions_dir.generic_string();
            if (path_uri.size() >= 2 && path_uri[1] == ':') {
                path_uri[0] = std::tolower(path_uri[0]);
                path_uri = "/" + path_uri;
            }

            std::string new_entry = "{\"identifier\":{\"id\":\"dzxpert.chkoupi-lang\"},\"version\":\"1.0.0\",\"location\":{\"$mid\":1,\"path\":\"" + path_uri + "\",\"scheme\":\"file\"},\"relativeLocation\":\"dzxpert.chkoupi-lang-1.0.0\",\"metadata\":{\"installedTimestamp\":" + std::to_string(timestamp) + ",\"pinned\":false,\"source\":\"user\",\"id\":\"dzxpert.chkoupi-lang\"}}";

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
        } else {
            std::cout << "Extension is already registered in extensions.json.\n";
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
