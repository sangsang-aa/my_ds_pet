// Minimal INI reader for the pet's optional pet.ini. Pure (no <windows.h>) so it
// can be unit-tested on any host.
#ifndef PET_INIFILE_H
#define PET_INIFILE_H

#include <cstddef>
#include <map>
#include <string>

namespace pet_ini {

struct File {
    std::map<std::string, std::map<std::string, std::string>> sections;
};

inline std::string Trim(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return std::string();
    }
    const std::size_t last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

// [section] headers, `key = value` rows, and `#` / `;` comments. Keys and values
// are trimmed and the last duplicate wins; keys before any header land in "".
inline File Parse(const std::string& text) {
    File file;
    std::string section;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t newline = text.find('\n', pos);
        const std::string line = Trim(text.substr(
            pos, newline == std::string::npos ? std::string::npos : newline - pos));
        pos = (newline == std::string::npos) ? text.size() : newline + 1;
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = Trim(line.substr(0, equals));
        const std::string value = Trim(line.substr(equals + 1));
        if (!key.empty()) {
            file.sections[section][key] = value;
        }
    }
    return file;
}

inline const std::string* Find(const File& file, const std::string& section,
                               const std::string& key) {
    const auto found_section = file.sections.find(section);
    if (found_section == file.sections.end()) {
        return nullptr;
    }
    const auto found_key = found_section->second.find(key);
    return found_key == found_section->second.end() ? nullptr : &found_key->second;
}

// Parse the whole value, rejecting trailing junk; returns `fallback` otherwise.
inline int IntOr(const std::string& value, int fallback) {
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        return used == value.size() ? parsed : fallback;
    } catch (...) {
        return fallback;
    }
}

inline double DoubleOr(const std::string& value, double fallback) {
    try {
        std::size_t used = 0;
        const double parsed = std::stod(value, &used);
        return used == value.size() ? parsed : fallback;
    } catch (...) {
        return fallback;
    }
}

}  // namespace pet_ini

#endif  // PET_INIFILE_H
