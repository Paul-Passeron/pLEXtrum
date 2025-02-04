/**
 * plextrum.h
 * Copyright (C) 2024 Paul Passeron
 * pLEXtrum header file
 * Paul Passeron <paul.passeron2@gmail.com>
 */

/********************************************************************************
 * A flexible rule-based lexical analyzer (lexer) library.
 *
 * This library provides facilities for creating and using lexers that tokenize
 *source code based on customizable pattern-matching rules and actions.
 *
 * - Rule-based lexical analysis with matcher/action pairs
 * - Support for stateful lexing via user contexts
 * - Token flags for filtering/ignoring tokens (useful for identation-aware
 *languages)
 * - Token position (line/column/file) tracking
 * - Utility functions for common character classifications
 * - Dynamic rule management
 * - Memory-efficient design with minimal allocations
 *
 * The lexer processes input text by attempting to match rules in registration
 *order, executing associated actions for successful matches. This allows for
 *flexible tokenization strategies suitable for many parsing applications.
 *
 * Why this design ?
 * - Fast and efficient
 * - Easy to use for simple lexers
 *   (see C lexer at https://github.com/Paul-Passeron/c_plextrum)
 * - Ability to do some complex stuff (actions are arbitrary
      functions that act on the lexer itself)
 ********************************************************************************/

#ifndef PLEXTRUM_H
#define PLEXTRUM_H

#include "dynarr.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Dynamic array stuff
#ifndef ASSERT
#define ASSERT assert
#endif // ASSERT

#ifndef REALLOC
#define REALLOC realloc
#endif // REALLOC

#ifndef FREE
#define FREE free
#endif // FREE

// Initial capacity of a dynamic array
#ifndef DA_INIT_CAP
#define DA_INIT_CAP 16
#endif

// Append an item to a dynamic array
#define da_append(da, item)                                                    \
  do {                                                                         \
    if ((da)->count >= (da)->capacity) {                                       \
      (da)->capacity = (da)->capacity == 0 ? DA_INIT_CAP : (da)->capacity * 2; \
      (da)->items =                                                            \
          REALLOC((da)->items, (da)->capacity * sizeof(*(da)->items));         \
      ASSERT((da)->items != NULL && "No more memory");                         \
    }                                                                          \
    (da)->items[(da)->count++] = (item);                                       \
  } while (0)

#define da_free(da) FREE((da).items)

// Core types
typedef struct lexer_t lexer_t;
typedef struct token_t token_t;
typedef struct lexer_rule_t lexer_rule_t;

typedef enum token_flag_t {
  TOKEN_FLAG_NONE = 0,
  TOKEN_FLAG_IGNORE = 1 << 0,
} token_flag_t;

typedef enum lexer_flags_t {
  LEXER_FLAG_NONE,
  LEXER_FLAG_KEEP_IGNORABLE = 1 << 0,
} lexer_flags_t;

typedef enum internal_token_kind_t {
  INTERNAL_TOKEN_EOF,
  INTERNAL_TOKEN_ERROR,
  // ...
} token_kind_t;

// Token structure (generic)
typedef struct token_t {
  uint32_t kind;
  const char *lexeme; // Points to start of token in source
  size_t length;      // Length of the token
  const char *filename;
  size_t line;
  size_t column;
  uint32_t flags;
} token_t;

// Function pointer types for lexer operations
typedef bool (*token_matcher_fn)(lexer_t *lexer, token_t *token);
typedef void (*token_action_fn)(lexer_t *lexer, token_t *token);
typedef void (*context_destructor_fn)(void *context);

struct lexer_rule_t {
  token_matcher_fn matcher;
  token_action_fn action;
};

typedef struct lexer_rules_t {
  lexer_rule_t *items;
  size_t count;
  size_t capacity;
} lexer_rules_t;

typedef struct lexer_t {
  // Input management
  const char *source;
  size_t source_length;
  size_t position;

  // Location tracking
  size_t line;
  size_t column;
  const char *filename;

  // Rule management
  lexer_rules_t rules;

  // Error tracking
  char *error_message;

  // User-defined context
  void *context;

  uint32_t flags;
} lexer_t;

// Lexer creation and destruction
lexer_t *lexer_create(const char *source, size_t length, const char *filename,
                      uint32_t flags);
void lexer_destroy(lexer_t *lexer);

// Context management
void *lexer_get_context(lexer_t *lexer);

// Rule management
bool lexer_add_rule(lexer_t *lexer, token_matcher_fn matcher,
                    token_action_fn action);

// Core lexing operations
token_t lexer_next_token(lexer_t *lexer);
void lexer_reset(lexer_t *lexer, const char *source, size_t length,
                 const char *filename);

// Source inspection utilities
char lexer_peek(const lexer_t *lexer, size_t offset);
char lexer_current(const lexer_t *lexer);
void lexer_advance(lexer_t *lexer);
bool lexer_is_eof(const lexer_t *lexer);

// Location information
size_t lexer_get_position(const lexer_t *lexer);
size_t lexer_get_line(const lexer_t *lexer);
size_t lexer_get_column(const lexer_t *lexer);

// Lexeme management
const char *lexer_get_lexeme(const lexer_t *lexer, size_t start, size_t length);

// Error handling
typedef struct lexer_error_t {
  const char *message;
  size_t line;
  size_t column;
} lexer_error_t;

lexer_error_t *lexer_get_error(const lexer_t *lexer);

// Character classification helpers (for implementing matchers)
bool lexer_is_digit(char c);
bool lexer_is_alpha(char c);
bool lexer_is_alnum(char c);
bool lexer_is_space(char c);

token_t create_token(uint32_t kind, const char *lexeme, size_t length,
                     size_t line, size_t column, const char *filename,
                     uint32_t flags);

#ifdef LEXER_IMPL

lexer_t *lexer_create(const char *source, size_t length, const char *filename,
                      uint32_t flags) {
  if (source == NULL) {
    return NULL;
  }
  lexer_t *lexer = malloc(sizeof(lexer_t));
  if (lexer == NULL) {
    return NULL;
  }
  if (length == 0) {
    length = strlen(source);
  }

  lexer->source = source;
  lexer->source_length = length;
  lexer->position = 0;

  lexer->filename = filename;
  lexer->line = 1;
  lexer->column = 1;

  lexer->rules = (lexer_rules_t){0};

  lexer->context = NULL;

  lexer->flags = flags;

  return lexer;
}

void lexer_destroy(lexer_t *lexer) {
  if (lexer == NULL) {
    return;
  }
  da_free(lexer->rules);
  free(lexer);
}

bool lexer_add_rule(lexer_t *lexer, token_matcher_fn matcher,
                    token_action_fn action) {
  if (lexer == NULL || matcher == NULL) {
    return false;
  }
  lexer_rule_t new_rule;
  new_rule.matcher = matcher;
  new_rule.action = action;
  da_append(&lexer->rules, new_rule);
  return true;
}

void lexer_reset(lexer_t *lexer, const char *source, size_t length,
                 const char *filename) {
  if (lexer == NULL || source == NULL) {
    return;
  }
  if (length == 0) {
    length = strlen(source);
  }
  lexer->source = source;
  lexer->source_length = length;
  lexer->position = 0;

  lexer->filename = filename;
  lexer->line = 1;
  lexer->column = 1;
}

char lexer_current(const lexer_t *lexer) {
  if (lexer == NULL || lexer->position >= lexer->source_length) {
    return 0;
  }
  return lexer->source[lexer->position];
}

char lexer_peek(const lexer_t *lexer, size_t offset) {
  if (lexer == NULL || lexer->position + offset >= lexer->source_length) {
    return '\0';
  }
  return lexer->source[lexer->position + offset];
}

void lexer_advance(lexer_t *lexer) {
  if (lexer == NULL || lexer->position >= lexer->source_length) {
    return;
  }
  char current = lexer->source[lexer->position++];

  if (current == '\n') {
    lexer->line++;
    lexer->column = 1;
  } else {
    lexer->column++;
  }
}

bool lexer_is_eof(const lexer_t *lexer) {
  return lexer == NULL || lexer->position >= lexer->source_length;
}

void *lexer_get_context(lexer_t *lexer) {
  if (lexer == NULL) {
    return NULL;
  }
  return lexer->context;
}

token_t create_token(uint32_t kind, const char *lexeme, size_t length,
                     size_t line, size_t column, const char *filename,
                     uint32_t flags) {
  token_t token;
  token.kind = kind;
  token.lexeme = lexeme;
  token.line = line;
  token.column = column;
  token.length = length;
  token.filename = filename;
  token.flags = flags;
  return token;
}

token_t lexer_next_token(lexer_t *lexer) {

  if (lexer == NULL || lexer_is_eof(lexer)) {
    return create_token(INTERNAL_TOKEN_EOF, "EOF", 0, lexer ? lexer->line : 0,
                        lexer ? lexer->column : 0, lexer ? lexer->filename : 0,
                        0);
  }

  token_t token = {0};
  int reset = false;

  while (true) {
    // Save starting position for each token attempt
    size_t start_position = lexer->position;
    size_t start_line = lexer->line;
    size_t start_column = lexer->column;

    for (size_t i = 0; i < lexer->rules.count; ++i) {
      // Set initial token position for each rule attempt
      token.line = start_line;
      token.column = start_column;
      token.lexeme = lexer->source + start_position;
      // printf("Trying rule %ld\n", i);
      if (reset) {
        i = 0;
      }
      lexer_rule_t rule = lexer->rules.items[i];
      if (rule.matcher(lexer, &token)) {
        // Successful match

        if (rule.action) {
          rule.action(lexer, &token);
        }
        if (token.flags & TOKEN_FLAG_IGNORE &&
            !(lexer->flags & LEXER_FLAG_KEEP_IGNORABLE)) {
          // For ignorable tokens, continue outer loop
          reset = true;
          break;
        }
        // Return copy of successful token
        return create_token(token.kind, token.lexeme, token.length, token.line,
                            token.column, token.filename, token.flags);
      }

      // Rule didn't match, reset position
      lexer->position = start_position;
      lexer->line = start_line;
      lexer->column = start_column;
    }

    // If we're not resetting to handle an ignorable token, break
    if (!reset) {
      break;
    }
    reset = false;
  }

  // No rules matched - handle error case
  if (lexer_is_eof(lexer)) {
    return create_token(INTERNAL_TOKEN_EOF, "EOF", 0, lexer->line,
                        lexer->column, lexer->filename, 0);
  }
  token_t error =
      create_token(INTERNAL_TOKEN_ERROR, lexer->source + lexer->position, 1,
                   lexer->line, lexer->column, lexer->filename, 0);
  lexer_advance(lexer);
  return error;
}

size_t lexer_get_position(const lexer_t *lexer) {
  return lexer ? lexer->position : 0;
}

size_t lexer_get_line(const lexer_t *lexer) { return lexer ? lexer->line : 0; }

size_t lexer_get_column(const lexer_t *lexer) {
  return lexer ? lexer->column : 0;
}

const char *lexer_get_lexeme(const lexer_t *lexer, size_t start,
                             size_t length) {
  if (!lexer || start >= lexer->source_length) {
    return NULL;
  }

  // // Ensure we don't read past the end of source
  if (start + length > lexer->source_length) {
    length = lexer->source_length - start;
  }

  return lexer->source + start;
}

bool lexer_is_space(char c) {
  switch (c) {
  case ' ':
  case '\n':
  case '\t':
  case '\b':
  case '\r':
  case '\v':
    return true;
  }
  return false;
}

bool lexer_is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool lexer_is_digit(char c) { return c >= '0' && c <= '9'; }

bool lexer_is_alnum(char c) { return lexer_is_alpha(c) || lexer_is_digit(c); }

#endif // PLEXTRUM_IMPL

#endif //PLEXTRUM_H
