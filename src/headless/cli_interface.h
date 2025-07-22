/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CLI_INTERFACE_H
#define CLI_INTERFACE_H

#include <stdbool.h>

typedef struct cli_context cli_context_t;
typedef struct cli_command cli_command_t;
typedef struct cli_option cli_option_t;
typedef struct cli_args cli_args_t;

typedef enum {
    CLI_SUCCESS = 0,
    CLI_ERROR_GENERIC = -1,
    CLI_ERROR_INVALID_ARGS = -2,
    CLI_ERROR_COMMAND_NOT_FOUND = -3,
    CLI_ERROR_MISSING_REQUIRED_OPTION = -4,
    CLI_ERROR_INVALID_OPTION_VALUE = -5,
    CLI_ERROR_FILE_NOT_FOUND = -6,
    CLI_ERROR_PROJECT_LOAD_FAILED = -7,
    CLI_ERROR_PROJECT_SAVE_FAILED = -8,
    CLI_ERROR_RENDER_FAILED = -9,
    CLI_ERROR_EXPORT_FAILED = -10,
    CLI_ERROR_VOXEL_OPERATION_FAILED = -11,
    CLI_ERROR_LAYER_OPERATION_FAILED = -12
} cli_result_t;

typedef enum {
    CLI_OPT_STRING,
    CLI_OPT_INT,
    CLI_OPT_FLOAT,
    CLI_OPT_BOOL,
    CLI_OPT_FLAG
} cli_option_type_t;

struct cli_option {
    const char *short_name;
    const char *long_name;
    const char *description;
    cli_option_type_t type;
    bool required;
    bool has_default;
    union {
        const char *string_default;
        int int_default;
        float float_default;
        bool bool_default;
    } default_value;
    struct cli_option *next;
};

struct cli_command {
    const char *name;
    const char *description;
    const char *usage;
    cli_option_t *options;
    cli_result_t (*handler)(cli_context_t *ctx, cli_args_t *args);
    struct cli_command *next;
};

typedef struct cli_parsed_option {
    const char *name;
    cli_option_type_t type;
    union {
        const char *string_value;
        int int_value;
        float float_value;
        bool bool_value;
    } value;
    struct cli_parsed_option *next;
} cli_parsed_option_t;

struct cli_args {
    int argc;
    char **argv;
    int arg_index;
    cli_parsed_option_t *options;
    char **positional_args;
    int positional_count;
};

struct cli_context {
    cli_command_t *commands;
    const char *program_name;
    bool verbose;
    bool quiet;
    const char *config_file;
    void *goxel_context;
};

cli_context_t *cli_create_context(const char *program_name);
void cli_destroy_context(cli_context_t *ctx);

cli_result_t cli_register_command(cli_context_t *ctx, const char *name,
                                 const char *description, const char *usage,
                                 cli_result_t (*handler)(cli_context_t *ctx, cli_args_t *args));

cli_result_t cli_add_option(cli_context_t *ctx, const char *command_name,
                           const char *short_name, const char *long_name,
                           const char *description, cli_option_type_t type,
                           bool required);

cli_result_t cli_add_option_with_default(cli_context_t *ctx, const char *command_name,
                                        const char *short_name, const char *long_name,
                                        const char *description, cli_option_type_t type,
                                        bool required, const char *default_string,
                                        int default_int, float default_float,
                                        bool default_bool);

cli_result_t cli_parse_args(cli_context_t *ctx, int argc, char **argv, cli_args_t **args);
void cli_free_args(cli_args_t *args);

cli_result_t cli_execute_command(cli_context_t *ctx, const char *command_name, cli_args_t *args);
cli_result_t cli_run(cli_context_t *ctx, int argc, char **argv);

const char *cli_get_option_string(cli_args_t *args, const char *name, const char *default_value);
int cli_get_option_int(cli_args_t *args, const char *name, int default_value);
float cli_get_option_float(cli_args_t *args, const char *name, float default_value);
bool cli_get_option_bool(cli_args_t *args, const char *name, bool default_value);
bool cli_has_option(cli_args_t *args, const char *name);

const char *cli_get_positional_arg(cli_args_t *args, int index);
int cli_get_positional_count(cli_args_t *args);

void cli_print_help(cli_context_t *ctx);
void cli_print_command_help(cli_context_t *ctx, const char *command_name);
void cli_print_version(void);

const char *cli_error_string(cli_result_t error);

void cli_set_global_options(cli_context_t *ctx, bool verbose, bool quiet, const char *config_file);
void cli_set_goxel_context(cli_context_t *ctx, void *goxel_context);

cli_result_t cli_register_builtin_commands(cli_context_t *ctx);

#endif // CLI_INTERFACE_H