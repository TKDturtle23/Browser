#pragma once

#ifndef BROWSER_CSSCOLOR_H
#define BROWSER_CSSCOLOR_H
#include <array>
#include <optional>
#include <string_view>

#include "Color.h"

struct TrieNode {
    std::array<TrieNode*, 256> next{};
    std::optional<Color> value = std::nullopt;

    constexpr TrieNode() : next{} {}
};

// ---------- COLOR ENCODING ----------
 const Color RGB(uint32_t hex);

// ---------- TRIE ROOT ----------


// ---------- INSERT (compile-time friendly initializer) ----------
const void Insert(TrieNode& root, std::string_view key, Color color);

// ---------- BUILD TABLE ----------
 void BuildTrie();

// ---------- NORMALIZATION ----------
 char toLower(char c);

// ---------- FAST TRIE PARSER ----------
 std::optional<Color> ParseColor(std::string_view s);
#endif //BROWSER_CSSCOLOR_H
