// Unit tests for inifile.h — pure logic, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/inifile_test.cpp -o /tmp/inifile_test && /tmp/inifile_test
#include "../src/core/inifile.h"

#include <cassert>
#include <cstdio>
#include <string>

namespace {

void test_sections_keys_and_comments() {
    const pet_ini::File file = pet_ini::Parse(
        "[pet]\n"
        "idle_min_ticks = 30\n"
        "# a comment\n"
        "; another\n"
        "idle_max_ticks=100\n"
        "\n"
        "[bubble]\n"
        "emoji_size_px = 220\n");
    assert(file.sections.size() == 2);
    assert(*pet_ini::Find(file, "pet", "idle_min_ticks") == "30");
    assert(*pet_ini::Find(file, "pet", "idle_max_ticks") == "100");
    assert(*pet_ini::Find(file, "bubble", "emoji_size_px") == "220");
    assert(pet_ini::Find(file, "pet", "missing") == nullptr);
    assert(pet_ini::Find(file, "nope", "x") == nullptr);
}

void test_trimming_and_last_value_wins() {
    const pet_ini::File file = pet_ini::Parse("  [ pet ]  \n  key = a b  \nkey = c\n");
    assert(*pet_ini::Find(file, "pet", "key") == "c");
}

void test_keys_before_any_section() {
    const pet_ini::File file = pet_ini::Parse("top = 1\n[s]\nx = 2\n");
    assert(*pet_ini::Find(file, "", "top") == "1");
    assert(*pet_ini::Find(file, "s", "x") == "2");
}

void test_crlf_and_missing_trailing_newline() {
    const pet_ini::File file = pet_ini::Parse("[a]\r\nk = v\r\nlast = 9");
    assert(*pet_ini::Find(file, "a", "k") == "v");
    assert(*pet_ini::Find(file, "a", "last") == "9");
}

void test_number_parsing() {
    assert(pet_ini::IntOr("42", 7) == 42);
    assert(pet_ini::IntOr("-3", 7) == -3);
    assert(pet_ini::IntOr("abc", 7) == 7);
    assert(pet_ini::IntOr("42x", 7) == 7);
    assert(pet_ini::IntOr("", 7) == 7);
    assert(pet_ini::DoubleOr("0.75", 1.0) == 0.75);
    assert(pet_ini::DoubleOr("x", 1.0) == 1.0);
}

}  // namespace

int main() {
    test_sections_keys_and_comments();
    test_trimming_and_last_value_wins();
    test_keys_before_any_section();
    test_crlf_and_missing_trailing_newline();
    test_number_parsing();
    std::printf("inifile: all tests passed\n");
    return 0;
}
