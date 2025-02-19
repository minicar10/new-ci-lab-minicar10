#include "interpreter.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_type.h"
#include "mem.h"

static bool    cond_holds(Interpreter *intr, BranchCondition cond);
static int64_t fetch_number_value(Interpreter *intr, Operand *op, bool is_im);
static bool    print_base(Interpreter *intr, Command *cmd);

void interpreter_init(Interpreter *intr, LabelMap *map) {
    if (!intr) {
        return;
    }

    intr->had_error  = false;
    intr->label_map  = map;
    intr->is_greater = false;
    intr->is_equal   = false;
    intr->is_less    = false;
    intr->the_stack  = NULL;

    for (size_t i = 0; i < NUM_VARIABLES; i++) {
        intr->variables[i] = 0;
    }
}

static void free_stack(Interpreter *intr) {
    while (intr->the_stack != NULL) {
        StackEntry *temp = intr->the_stack;
        intr->the_stack  = intr->the_stack->next;
        free(temp);
    }
}

void interpret(Interpreter *intr, Command *commands) {
    if (!intr || !commands) {
        return;
    }

    Command *current = commands;
    while (current && !intr->had_error) {
        switch (current->type) {
            case CMD_ADD: {
                int64_t val_a = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t val_b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                intr->variables[(int) current->destination.base] = val_a + val_b;

                current = current->next;
                break;
            }

            case CMD_SUB: {
                int64_t val_a = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t val_b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                intr->variables[(int) current->destination.base] = val_a - val_b;

                current = current->next;
                break;
            }

            case CMD_MOV: {
                int64_t value = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                intr->variables[(int) current->destination.base] = value;

                current = current->next;
                break;
            }

            case CMD_CMP: {
                int64_t val_a = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t val_b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);

                intr->is_greater = (val_a > val_b);
                intr->is_equal   = (val_a == val_b);
                intr->is_less    = (val_a < val_b);

                current = current->next;
                break;
            }

            case CMD_CMP_U: {
                uint64_t val_a =
                    (uint64_t) fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                uint64_t val_b =
                    (uint64_t) fetch_number_value(intr, &current->val_b, current->is_b_immediate);

                intr->is_greater = (val_a > val_b);
                intr->is_equal   = (val_a == val_b);
                intr->is_less    = (val_a < val_b);

                current = current->next;
                break;
            }

            case CMD_PRINT: {
                if (!print_base(intr, current)) {
                    intr->had_error = true;
                }
                current = current->next;
                break;
            }

            case CMD_AND: {
                int64_t val_a = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t val_b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                intr->variables[(int) current->destination.base] = val_a & val_b;

                current = current->next;
                break;
            }

            case CMD_EOR: {
                int64_t val_a = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t val_b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                intr->variables[(int) current->destination.base] = val_a ^ val_b;

                current = current->next;
                break;
            }

            case CMD_ORR: {
                int64_t val_a = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t val_b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                intr->variables[(int) current->destination.base] = val_a | val_b;

                current = current->next;
                break;
            }

            case CMD_LSL: {
                int64_t val_a = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t val_b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                intr->variables[(int) current->destination.base] = val_a << val_b;

                current = current->next;
                break;
            }

            case CMD_LSR: {
                int64_t  val_a = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t  val_b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                uint64_t uval_a = (uint64_t) val_a;
                uint64_t result = uval_a >> val_b;

                intr->variables[(int) current->destination.base] = result;
                current                                          = current->next;
                break;
            }

            case CMD_ASR: {
                int64_t val_a = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t val_b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                intr->variables[(int) current->destination.base] = val_a >> val_b;

                current = current->next;
                break;
            }

            case CMD_LOAD: {
                int64_t size = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t address =
                    fetch_number_value(intr, &current->val_b, current->is_b_immediate);

                if (size != 1 && size != 2 && size != 4 && size != 8) {
                    intr->had_error = true;
                    break;
                }

                uint8_t data[8] = {0};
                if (!mem_load(data, (size_t) address, (size_t) size)) {
                    intr->had_error = true;
                    break;
                }

                intr->variables[(int) current->destination.base] = *((int64_t *) data);
                current                                          = current->next;
                break;
            }

            case CMD_STORE: {
                int64_t value = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t address =
                    fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                int64_t size = current->destination.num_val;
                if (!mem_store((uint8_t *) &value, (size_t) address, (size_t) size)) {
                    intr->had_error = true;
                    break;
                }
                current = current->next;
                break;
            }

            case CMD_PUT: {
                const char *str = current->val_a.str_val;
                int64_t     address =
                    fetch_number_value(intr, &current->val_b, current->is_b_immediate);

                if (!str || address < 0) {
                    intr->had_error = true;
                    break;
                }

                size_t length = strlen(str) + 1;

                for (size_t i = 0; i < length; i++) {
                    if (!mem_store((uint8_t *) &str[i], address + i, 1)) {
                        intr->had_error = true;
                        break;
                    }
                }

                current = current->next;
                break;
            }

            case CMD_BRANCH: {
                if (current->branch_condition == BRANCH_ALWAYS ||
                    cond_holds(intr, current->branch_condition)) {
                    Entry *entry = get_label(intr->label_map, current->val_a.str_val);
                    if (entry == NULL) {
                        printf("Label not found: %s\n", current->val_a.str_val);
                        intr->had_error = true;
                        free_stack(intr);
                        return;
                    }
                    if (entry->command == NULL) {
                        if (current->val_a.str_val[0] == '.') {
                            current = NULL;
                        } else {
                            printf("Label not found: %s\n", current->val_a.str_val);
                            intr->had_error = true;
                            free_stack(intr);
                            return;
                        }
                    } else {
                        current = entry->command;
                    }
                } else {
                    current = current->next;
                }
                break;
            }

            case CMD_CALL: {
                StackEntry *new_entry = malloc(sizeof(StackEntry));
                if (!new_entry) {
                    intr->had_error = true;
                    free_stack(intr);
                    return;
                }
                memcpy(new_entry->variables, intr->variables, sizeof(intr->variables));
                new_entry->command = current->next;
                new_entry->next    = intr->the_stack;
                intr->the_stack    = new_entry;
                Entry *entry       = get_label(intr->label_map, current->val_a.str_val);
                if (entry && entry->command != NULL) {
                    current = entry->command;
                } else {
                    printf("Label not found: %s\n", current->val_a.str_val);
                    intr->had_error = true;
                    free_stack(intr);
                    return;
                }
                break;
            }

            case CMD_RET: {
                if (intr->the_stack) {
                    StackEntry *return_entry = intr->the_stack;
                    intr->the_stack          = return_entry->next;

                    for (int i = 1; i < NUM_VARIABLES; i++) {
                        intr->variables[i] = return_entry->variables[i];
                    }

                    current = return_entry->command;
                    free(return_entry);
                } else {
                    current = NULL;
                }
                break;
            }

            default:
                intr->had_error = true;
                current         = current->next;
                break;
        }
    }

    free_stack(intr);
}

void print_interpreter_state(Interpreter *intr) {
    if (!intr) {
        return;
    }

    printf("Error: %d\n", intr->had_error);
    printf("Flags:\n");
    printf("Is greater: %d\n", intr->is_greater);
    printf("Is equal: %d\n", intr->is_equal);
    printf("Is less: %d\n", intr->is_less);

    printf("\n");

    printf("Variable values:\n");
    for (size_t i = 0; i < NUM_VARIABLES; i++) {
        printf("x%zu: %" PRId64 "", i, intr->variables[i]);

        if (i < NUM_VARIABLES - 1) {
            printf(", ");
        }

        if ((i + 1) % 8 == 0) {
            printf("\n");
        }
    }

    printf("\n");
}

/**
 * @brief Fetches the appropriate value from the given operand.
 *
 * @param intr The pointer to the interpreter holding variable state.
 * @param op The operand used to fetch the value.
 * @param is_im A boolean representing whether this value is an immediate or
 * must be read in from the interpreter state.
 * @return The fetched value.
 */
static int64_t fetch_number_value(Interpreter *intr, Operand *op, bool is_im) {
    if (is_im) {
        return op->num_val;
    } else {
        return intr->variables[(int) op->base];
    }
}

/**
 * @brief Determines whether a given branch condition holds.
 *
 * @param intr The pointer to the interpreter holding the result of the
 * comparison.
 * @param cond The condition to check.
 * @return True if the given condition holds, false otherwise.
 */
static bool cond_holds(Interpreter *intr, BranchCondition cond) {
    switch (cond) {
        case BRANCH_ALWAYS:
            return true;
        case BRANCH_EQUAL:
            return intr->is_equal;
        case BRANCH_GREATER_EQUAL:
            return intr->is_greater || intr->is_equal;
        case BRANCH_GREATER:
            return intr->is_greater;
        case BRANCH_LESS_EQUAL:
            return intr->is_less || intr->is_equal;
        case BRANCH_LESS:
            return intr->is_less;
        case BRANCH_NOT_EQUAL:
            return !intr->is_equal;
        default:
            return false;
    }
}

/**
 * @brief Prints the given command's value in a specified base.
 *
 * @param intr The pointer to the interpreter holding variable state.
 * @param cmd The command being processed.
 * @return True whether the print was successful, false otherwise.
 */
static bool print_base(Interpreter *intr, Command *cmd) {
    const char *base  = cmd->val_a.str_val;
    int64_t     value = fetch_number_value(intr, &cmd->val_b, cmd->is_b_immediate);

    if (base[0] == 's') {
        char   buffer[256] = {0};
        size_t i           = 0;
        while (i < sizeof(buffer) - 1) {
            if (!mem_load((uint8_t *) &buffer[i], value + i, 1)) {
                intr->had_error = true;
                return false;
            }
            if (buffer[i] == '\0') {
                break;
            }
            i++;
        }
        printf("%s\n", buffer);
    } else if (base[0] == 'd') {
        printf("%" PRId64 "\n", value);
    } else if (base[0] == 'x') {
        printf("0x%" PRIx64 "\n", (uint64_t) value);
    } else if (base[0] == 'b') {
        char     binary[65] = {0};
        int      started    = 0;
        int      index      = 0;
        uint64_t uvalue     = (uint64_t) value;

        for (int i = 63; i >= 0; i--) {
            char bit = ((uvalue >> i) & 1) ? '1' : '0';

            if (bit == '1' || started) {
                started         = 1;
                binary[index++] = bit;
            }
        }

        if (!started) {
            binary[0] = '0';
            binary[1] = '\0';
        }

        printf("0b%s\n", binary);
    } else {
        intr->had_error = true;
        return false;
    }

    return true;
}
