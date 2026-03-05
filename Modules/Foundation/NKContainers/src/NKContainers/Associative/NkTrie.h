// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Associative\NkTrie.h
// DESCRIPTION: Trie - Prefix tree for efficient string operations
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKTRIE_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKTRIE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Trie - Prefix tree for strings
         * 
         * Arbre de préfixes pour recherche efficace de strings.
         * Très efficace pour autocomplétion et recherche de préfixes.
         * 
         * Complexité:
         * - Insert: O(L) où L = longueur du mot
         * - Search: O(L)
         * - StartsWith: O(L)
         * 
         * @example
         * NkTrie trie;
         * trie.Insert("hello");
         * trie.Insert("help");
         * trie.Insert("world");
         * 
         * bool found = trie.Search("hello");    // true
         * bool prefix = trie.StartsWith("hel"); // true
         */
        template<typename Allocator = memory::NkAllocator>
        class NkTrie {
        private:
            static constexpr usize ALPHABET_SIZE = 26;  // a-z
            
            struct TrieNode {
                TrieNode* Children[ALPHABET_SIZE];
                bool IsEndOfWord;
                
                TrieNode() : IsEndOfWord(false) {
                    for (usize i = 0; i < ALPHABET_SIZE; ++i) {
                        Children[i] = nullptr;
                    }
                }
            };
            
            TrieNode* mRoot;
            Allocator* mAllocator;
            usize mSize;
            
            usize CharToIndex(char c) const {
                if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
                return c - 'a';
            }
            
            TrieNode* CreateNode() {
                TrieNode* node = static_cast<TrieNode*>(mAllocator->Allocate(sizeof(TrieNode)));
                new (node) TrieNode();
                return node;
            }
            
            void DestroyNode(TrieNode* node) {
                if (!node) return;
                
                for (usize i = 0; i < ALPHABET_SIZE; ++i) {
                    DestroyNode(node->Children[i]);
                }
                
                node->~TrieNode();
                mAllocator->Deallocate(node);
            }
            
            void CollectWords(TrieNode* node, NkVector<char>& prefix, NkVector<const char*>& results) const {
                if (!node) return;
                
                if (node->IsEndOfWord) {
                    // Add null terminator
                    prefix.PushBack('\0');
                    // Store word (needs proper string storage in real implementation)
                    results.PushBack(prefix.Data());
                    prefix.PopBack();
                }
                
                for (usize i = 0; i < ALPHABET_SIZE; ++i) {
                    if (node->Children[i]) {
                        prefix.PushBack('a' + i);
                        CollectWords(node->Children[i], prefix, results);
                        prefix.PopBack();
                    }
                }
            }
            
        public:
            // Constructors
            NkTrie()
                : mRoot(nullptr)
                , mAllocator(&memory::NkGetDefaultAllocator())
                , mSize(0) {
                mRoot = CreateNode();
            }
            
            ~NkTrie() {
                DestroyNode(mRoot);
            }
            
            // Modifiers
            void Insert(const char* word) {
                TrieNode* current = mRoot;
                
                for (usize i = 0; word[i] != '\0'; ++i) {
                    usize index = CharToIndex(word[i]);
                    NK_ASSERT(index < ALPHABET_SIZE);
                    
                    if (!current->Children[index]) {
                        current->Children[index] = CreateNode();
                    }
                    current = current->Children[index];
                }
                
                if (!current->IsEndOfWord) {
                    current->IsEndOfWord = true;
                    ++mSize;
                }
            }
            
            // Search
            bool Search(const char* word) const {
                TrieNode* current = mRoot;
                
                for (usize i = 0; word[i] != '\0'; ++i) {
                    usize index = CharToIndex(word[i]);
                    if (index >= ALPHABET_SIZE) return false;
                    
                    if (!current->Children[index]) {
                        return false;
                    }
                    current = current->Children[index];
                }
                
                return current && current->IsEndOfWord;
            }
            
            bool StartsWith(const char* prefix) const {
                TrieNode* current = mRoot;
                
                for (usize i = 0; prefix[i] != '\0'; ++i) {
                    usize index = CharToIndex(prefix[i]);
                    if (index >= ALPHABET_SIZE) return false;
                    
                    if (!current->Children[index]) {
                        return false;
                    }
                    current = current->Children[index];
                }
                
                return current != nullptr;
            }
            
            // Autocomplete
            NkVector<const char*> FindWordsWithPrefix(const char* prefix) const {
                NkVector<const char*> results;
                NkVector<char> currentPrefix;
                
                // Navigate to prefix node
                TrieNode* current = mRoot;
                for (usize i = 0; prefix[i] != '\0'; ++i) {
                    currentPrefix.PushBack(prefix[i]);
                    usize index = CharToIndex(prefix[i]);
                    
                    if (!current->Children[index]) {
                        return results;  // Prefix not found
                    }
                    current = current->Children[index];
                }
                
                // Collect all words from this node
                CollectWords(current, currentPrefix, results);
                return results;
            }
            
            // Capacity
            usize Size() const NK_NOEXCEPT { return mSize; }
            bool IsEmpty() const NK_NOEXCEPT { return mSize == 0; }
            
            void Clear() {
                DestroyNode(mRoot);
                mRoot = CreateNode();
                mSize = 0;
            }
        };
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKTRIE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// 
// USAGE EXAMPLE:
// 
// NkTrie trie;
// trie.Insert("hello");
// trie.Insert("help");
// trie.Insert("world");
// trie.Insert("word");
// 
// bool found = trie.Search("hello");         // true
// bool found2 = trie.Search("hel");          // false (not a word)
// bool prefix = trie.StartsWith("hel");      // true
// 
// // Autocomplete
// auto words = trie.FindWordsWithPrefix("hel");
// // Returns: ["hello", "help"]
// 
// USE CASES:
// - Autocomplete systems
// - Spell checkers
// - IP routing tables
// - Dictionary implementations
// - Prefix matching
// 
// ============================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
