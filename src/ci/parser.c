#include "parser.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "command_type.h"
#include "token_type.h"

static Token    advance(Parser *parser);
static bool     consume(Parser *parser, TokenType type);
static bool     is_at_end(Parser *parser);
static void     skip_nls(Parser *parser);
static bool     consume_newline(Parser *parser);
static Command *create_command(CommandType type);
static bool     is_variable(Token token);
static bool     parse_variable(Token token, int64_t *var_num);
static bool     parse_number(Token token, int64_t *result);
static bool     parse_variable_operand(Parser *parser, Operand *op);
static bool     parse_var_or_imm(Parser *parser, Operand *op, bool *is_immediate);
static Command *parse_cmd(Parser *parser);

void parser_init(Parser *parser, Lexer *lexer, LabelMap *map) {
    if (!parser) {
        return;
    }

    parser->lexer     = lexer;
    parser->had_error = false;
    parser->label_map = map;
    parser->current   = lexer_next_token(parser->lexer);
    parser->next      = lexer_next_token(parser->lexer);
}

/**
 * @brief Advances the parser in the token stream.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return The token that was just consumed.
 */
static Token advance(Parser *parser) {
    Token ret_token = parser->current;
    if (!is_at_end(parser)) {
        parser->current = parser->next;
        parser->next    = lexer_next_token(parser->lexer);
    }
    return ret_token;
}

/**
 * @brief Determines if the parser reached the end of the token stream.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return True if the parser is at the end of the token stream, false
 * otherwise.
 */
static bool is_at_end(Parser *parser) {
    return parser->current.type == TOK_EOF;
}

/**
 * @brief Consumes the token if it matches the specified token type.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param type The type of the token to match.
 * @return True if the token was consumed, false otherwise.
 */
static bool consume(Parser *parser, TokenType type) {
    if (parser->current.type == type) {
        advance(parser);
        return true;
    }

    return false;
}

/**
 * @brief Creates a command of the given type.
 *
 * @param type The type of the command to create.
 * @return A pointer to a command with the requested type.
 *
 * @note It is the responsibility of the caller to free the memory associated
 * with the returned command.
 */
static Command *create_command(CommandType type) {
    Command *cmd = (Command *) calloc(1, sizeof(Command));
    if (!cmd) {
        return NULL;
    }

    cmd->type             = type;
    cmd->next             = NULL;
    cmd->is_a_immediate   = false;
    cmd->is_a_string      = false;
    cmd->is_b_immediate   = false;
    cmd->is_b_string      = false;
    cmd->branch_condition = BRANCH_NONE;
    return cmd;
}

/**
 * @brief Determines if the given token is a valid variable.
 *
 * A valid (potential) variable is a token that begins with the prefix "x",
 * followed by any other character(s).
 *
 * @param token The token to check.
 * @return True if this token could be a variable, false otherwise.
 */
static bool is_variable(Token token) {
    return token.length >= 2 && token.lexeme[0] == 'x';
}

/**
 * @brief Determines if the given token is a valid base signifier.
 *
 * A valid base signifier is one of d (decimal), x (hex), b (binary) or s (string).
 *
 * @param token The token to check.
 * @return True if this token is a base signifier, false otherwise
 */
static bool is_base(Token token) {
    return token.length == 1 && (token.lexeme[0] == 'd' || token.lexeme[0] == 'x' ||
                                 token.lexeme[0] == 's' || token.lexeme[0] == 'b');
}

/**
 * @brief Parses the given token as a base signifier
 *
 * A base is a single character, either d, s, x, or b.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if the current token was parsed as a base, false otherwise.
 */
static bool parse_base(Parser *parser, Operand *op) {
    // STUDENT TODO: Parse the current token as a base
    return false;
}

/**
 * @brief Parses the given token as a variable.
 *
 * @param token The token to parse.
 * @param var_num a pointer to modify on success.
 * @return True if `var_num` was successfully modified, false otherwise.
 *
 * @note It is assumed that the token already was verified to begin with a valid
 * prefix, "x".
 */
static bool parse_variable(Token token, int64_t *var_num) {
    char   *endptr;
    int64_t tempnum = strtol(token.lexeme + 1, &endptr, 10);

    if ((token.lexeme + token.length) != endptr || tempnum < 0 || tempnum > 31) {
        return false;
    }

    *var_num = tempnum;
    return true;
}

/**
 * @brief Parses the given value as a number.
 *
 * @param token The token to parse.
 * @param result A pointer to the value to modify on success.
 * @return True if `result` was successfully modified, false otherwise.
 */
static bool parse_number(Token token, int64_t *result) {
    const char *parse_start = token.lexeme;
    int         base        = 10;

    if (token.length > 2 && token.lexeme[0] == '0') {
        if (token.lexeme[1] == 'x') {
            parse_start += 2;
            base = 16;
        } else if (token.lexeme[1] == 'b') {
            parse_start += 2;
            base = 2;
        }
    }

    char *endptr;
    *result = strtoll(parse_start, &endptr, base);

    return (token.lexeme + token.length) == endptr;
}

/**
 * @brief Conditionally parses the current token as a number.
 *
 * Note that this won't advance the parser if the token cannot be converted to
 * an integer.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if this token is a number and was was converted successfully,
 * false otherwise.
 */
static bool parse_im(Parser *parser, Operand *op) {
    Token *token = &parser->current;
    if (token->type != TOK_NUM) {
        return false;
    }

    uint64_t value = 0;
    if (token->length >= 2 && token->lexeme[0] == '0') {
        if (token->lexeme[1] == 'x' || token->lexeme[1] == 'X') {
            for (int i = 2; i < token->length; i++) {
                char c = token->lexeme[i];
                if (c >= '0' && c <= '9') {
                    value = value * 16 + (c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    value = value * 16 + (c - 'a' + 10);
                } else if (c >= 'A' && c <= 'F') {
                    value = value * 16 + (c - 'A' + 10);
                } else {
                    return false;
                }
            }
        } else if (token->lexeme[1] == 'b' || token->lexeme[1] == 'B') {
            for (int i = 2; i < token->length; i++) {
                char c = token->lexeme[i];
                if (c == '0' || c == '1') {
                    value = value * 2 + (c - '0');
                } else {
                    return false;
                }
            }
        } else {
            return false;
        }
    } else {
        for (int i = 0; i < token->length; i++) {
            char c = token->lexeme[i];
            if (c >= '0' && c <= '9') {
                value = value * 10 + (c - '0');
            } else {
                return false;
            }
        }
    }

    op->num_val = value;
    advance(parser);
    return true;
}

/**
 * @brief Parses the next token as a variable.
 *
 * A variable is anything starting with the prefix x and will be of type
 * TOK_IDENT.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if this was parsed as a variable, false otherwise.
 */
static bool parse_variable_operand(Parser *parser, Operand *op) {
    Token *token = &parser->current;

    if (token->type != TOK_IDENT || !is_variable(*token)) {
        return false;
    }

    int64_t var_num = 0;
    if (!parse_variable(*token, &var_num)) {
        return false;
    }

    op->base = var_num;
    advance(parser);
    return true;
}

/**
 * @brief Parses the next token as either a variable or an immediate.
 *
 * A number is considered to be anything beginning with a decimal digit or the
 * prefixes 0b or 0x and will be of type TOK_NUM. A variable is anything
 * starting with the prefix x and will be of type TOK_IDENT.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @param is_immediate A pointer to a boolean to modify upon determining whether
 * the given value is an immediate.
 * @return True if this was parsed as an immediate or a variable, false
 * otherwise.
 */
static bool parse_var_or_imm(Parser *parser, Operand *op, bool *is_immediate) {
    Token *token = &parser->current;

    if (token->type == TOK_IDENT) {
        if (!parse_variable_operand(parser, op)) {
            return false;
        }
        *is_immediate = false;
        return true;
    }

    if (token->type == TOK_NUM) {
        if (!parse_im(parser, op)) {
            return false;
        }
        *is_immediate = true;
        return true;
    }

    advance(parser);
    return false;
}

/**
 * @brief Skips past tokens that signal the start of a new line
 *
 * Consumes newlines until the end of file is reached.
 * An EOF is not considered to be a "new line" in this context because it is a
 * sentinel token, I.e, there is nothing after it.
 *
 * @param parser A pointer to the parser to read tokens from.
 */
static void skip_nls(Parser *parser) {
    while (consume(parser, TOK_NL))
        ;
}

/**
 * @brief Consumes a single newline
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return True whether a "new line" was consumed, false otherwise.
 *
 * @note An encounter of TOK_EOF should not be considered a failure, as this
 * procedure is designed to "reset" the grammar. In other words, it should be
 * used to ensure that we have a valid ending token after encountering an
 * instruction. Since TOK_EOF signals no more possible instructions, it should
 * effectively play the role of a new line when checking for a valid ending
 * sequence for a command.
 */
static bool consume_newline(Parser *parser) {
    return consume(parser, TOK_NL) || consume(parser, TOK_EOF);
}

/**
 * @brief Parses a singular command.
 *
 * Reads in the token(s) from the lexer that the parser owns and determines the
 * appropriate matching command. Updates the parser->had_error if an error
 * occurs.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return A pointer to the appropriate command.
 * Returns null if an error occurred or there are no commands to parse.
 *
 * @note The caller is responsible for freeing the memory associated with the
 * returned command.
 */
static Command *parse_cmd(Parser *parser) {
    skip_nls(parser);

    // You will need to modify this later
    // However, this is fine for getting going
    Token token = parser->current;

    if (token.type == TOK_IDENT) {
        // TODO Week 4: Handle labels
        // be careful of edge cases!
    }

    if (token.type == TOK_EOF) {
        // Week 4 TODO: If there is a label, put it there with a null command

        // No commands to parse; we are done
        return NULL;
    }

    switch (token.type) {
        case TOK_ADD: {
            Command *cmd = create_command(CMD_ADD);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate_a = false;
            if (!parse_var_or_imm(parser, &cmd->val_a, &is_immediate_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate_b = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate_b)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            cmd->is_a_immediate = is_immediate_a;
            cmd->is_b_immediate = is_immediate_b;

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_SUB: {
            Command *cmd = create_command(CMD_SUB);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate_a = false;
            if (!parse_var_or_imm(parser, &cmd->val_a, &is_immediate_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate_b = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate_b)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            cmd->is_a_immediate = is_immediate_a;
            cmd->is_b_immediate = is_immediate_b;

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_MOV: {
            Command *cmd = create_command(CMD_MOV);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate = false;
            if (!parse_var_or_imm(parser, &cmd->val_a, &is_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            cmd->is_a_immediate = is_immediate;

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_CMP: {
            Command *cmd = create_command(CMD_CMP);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate_b = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate_b)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->is_a_immediate = false;
            cmd->is_b_immediate = is_immediate_b;

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_CMP_U: {
            Command *cmd = create_command(CMD_CMP_U);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            bool is_immediate_a = false;
            if (!parse_var_or_imm(parser, &cmd->val_a, &is_immediate_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate_b = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate_b)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            cmd->is_a_immediate = is_immediate_a;
            cmd->is_b_immediate = is_immediate_b;

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_PRINT: {
            Command *cmd = create_command(CMD_PRINT);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            bool is_immediate = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->is_b_immediate = is_immediate;

            if (!is_base(parser->current)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            size_t str_len     = strlen(parser->current.lexeme) + 1;
            cmd->val_a.str_val = malloc(str_len);
            if (!cmd->val_a.str_val) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            memcpy(cmd->val_a.str_val, parser->current.lexeme, str_len);

            advance(parser);

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_AND: {
            Command *cmd = create_command(CMD_AND);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (!parse_variable_operand(parser, &cmd->val_b)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_EOR: {
            Command *cmd = create_command(CMD_EOR);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (!parse_variable_operand(parser, &cmd->val_b)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_ORR: {
            Command *cmd = create_command(CMD_ORR);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (!parse_variable_operand(parser, &cmd->val_b)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_LSL: {
            Command *cmd = create_command(CMD_LSL);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            cmd->is_b_immediate = is_immediate;

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_LSR: {
            Command *cmd = create_command(CMD_LSR);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            cmd->is_b_immediate = is_immediate;

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_ASR: {
            Command *cmd = create_command(CMD_ASR);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            cmd->is_b_immediate = is_immediate;

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_LOAD: {
            Command *cmd = create_command(CMD_LOAD);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate_offset = false;
            if (!parse_var_or_imm(parser, &cmd->val_a, &is_immediate_offset) ||
                !is_immediate_offset) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            bool is_immediate_address = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate_address)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->is_b_immediate = is_immediate_address;

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_STORE: {
            Command *cmd = create_command(CMD_STORE);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }

            advance(parser);

            bool is_immediate_src = false;
            if (!parse_var_or_imm(parser, &cmd->val_a, &is_immediate_src) || !is_immediate_src) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->is_a_immediate = is_immediate_src;

            bool is_immediate_address = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate_address)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            cmd->is_b_immediate = is_immediate_address;

            bool is_immediate_size = false;
            if (!parse_var_or_imm(parser, &cmd->destination, &is_immediate_size) ||
                !is_immediate_size) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        case TOK_PUT: {
            Command *cmd = create_command(CMD_PUT);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }
            advance(parser);

            if (parser->current.type != TOK_STR) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            size_t str_len     = strlen(parser->current.lexeme) + 1;
            cmd->val_a.str_val = malloc(str_len);
            if (!cmd->val_a.str_val) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            memcpy(cmd->val_a.str_val, parser->current.lexeme, str_len);

            cmd->is_a_immediate = true;
            advance(parser);

            bool is_immediate_address = false;
            if (!parse_var_or_imm(parser, &cmd->val_b, &is_immediate_address)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->is_b_immediate = is_immediate_address;

            if (parser->current.type != TOK_NL && parser->current.type != TOK_EOF) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }

            advance(parser);
            return cmd;
        }

        default:
            parser->had_error = true;
            break;
    }

    return NULL;
}

/**
 * @brief Parses commands into a linked list.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return A pointer to the head of the command list, or NULL if no commands
 * were parsed.
 */
Command *parse_commands(Parser *parser) {
    Command *head = NULL;
    Command *tail = NULL;

    while (!is_at_end(parser)) {
        Command *cmd = parse_cmd(parser);

        if (parser->had_error) {
            break;
        }

        if (cmd) {
            if (!head) {
                head = cmd;
            } else {
                tail->next = cmd;
            }
            tail = cmd;
        }
    }

    return head;
}
