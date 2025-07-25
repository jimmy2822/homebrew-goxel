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

#include "cli_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define CLI_MAX_OPTIONS 64
#define CLI_MAX_POSITIONAL_ARGS 32

static const char *CLI_VERSION = "13.0.0-alpha";

cli_context_t *cli_create_context(const char *program_name)
{
    cli_context_t *ctx = calloc(1, sizeof(cli_context_t));
    if (!ctx) return NULL;
    
    ctx->program_name = strdup(program_name);
    ctx->verbose = false;
    ctx->quiet = false;
    ctx->config_file = NULL;
    ctx->commands = NULL;
    ctx->goxel_context = NULL;
    
    return ctx;
}

void cli_destroy_context(cli_context_t *ctx)
{
    if (!ctx) return;
    
    cli_command_t *cmd = ctx->commands;
    while (cmd) {
        cli_command_t *next_cmd = cmd->next;
        
        cli_option_t *opt = cmd->options;
        while (opt) {
            cli_option_t *next_opt = opt->next;
            free(opt);
            opt = next_opt;
        }
        
        free(cmd);
        cmd = next_cmd;
    }
    
    if (ctx->program_name) free((void*)ctx->program_name);
    if (ctx->config_file) free((void*)ctx->config_file);
    free(ctx);
}

cli_result_t cli_register_command(cli_context_t *ctx, const char *name,
                                 const char *description, const char *usage,
                                 cli_result_t (*handler)(cli_context_t *ctx, cli_args_t *args))
{
    if (!ctx || !name || !handler) return CLI_ERROR_INVALID_ARGS;
    
    cli_command_t *cmd = malloc(sizeof(cli_command_t));
    if (!cmd) return CLI_ERROR_GENERIC;
    
    cmd->name = strdup(name);
    cmd->description = description ? strdup(description) : NULL;
    cmd->usage = usage ? strdup(usage) : NULL;
    cmd->handler = handler;
    cmd->options = NULL;
    cmd->next = ctx->commands;
    ctx->commands = cmd;
    
    // Automatically add help option to every command
    cli_add_option(ctx, name, "h", "help", "Show help for this command", CLI_OPT_FLAG, false);
    
    return CLI_SUCCESS;
}

cli_result_t cli_add_option(cli_context_t *ctx, const char *command_name,
                           const char *short_name, const char *long_name,
                           const char *description, cli_option_type_t type,
                           bool required)
{
    return cli_add_option_with_default(ctx, command_name, short_name, long_name,
                                      description, type, required, NULL, 0, 0.0f, false);
}

cli_result_t cli_add_option_with_default(cli_context_t *ctx, const char *command_name,
                                        const char *short_name, const char *long_name,
                                        const char *description, cli_option_type_t type,
                                        bool required, const char *default_string,
                                        int default_int, float default_float,
                                        bool default_bool)
{
    if (!ctx || !command_name) return CLI_ERROR_INVALID_ARGS;
    
    cli_command_t *cmd = ctx->commands;
    while (cmd && strcmp(cmd->name, command_name) != 0) {
        cmd = cmd->next;
    }
    
    if (!cmd) return CLI_ERROR_COMMAND_NOT_FOUND;
    
    cli_option_t *opt = malloc(sizeof(cli_option_t));
    if (!opt) return CLI_ERROR_GENERIC;
    
    opt->short_name = short_name ? strdup(short_name) : NULL;
    opt->long_name = long_name ? strdup(long_name) : NULL;
    opt->description = description ? strdup(description) : NULL;
    opt->type = type;
    opt->required = required;
    opt->has_default = (default_string != NULL || default_int != 0 || 
                       default_float != 0.0f || default_bool != false);
    
    switch (type) {
        case CLI_OPT_STRING:
            opt->default_value.string_default = default_string ? strdup(default_string) : NULL;
            break;
        case CLI_OPT_INT:
            opt->default_value.int_default = default_int;
            break;
        case CLI_OPT_FLOAT:
            opt->default_value.float_default = default_float;
            break;
        case CLI_OPT_BOOL:
        case CLI_OPT_FLAG:
            opt->default_value.bool_default = default_bool;
            break;
    }
    
    opt->next = cmd->options;
    cmd->options = opt;
    
    return CLI_SUCCESS;
}

static cli_parsed_option_t *add_parsed_option(cli_args_t *args, const char *name,
                                             cli_option_type_t type)
{
    cli_parsed_option_t *opt = malloc(sizeof(cli_parsed_option_t));
    if (!opt) return NULL;
    
    opt->name = strdup(name);
    opt->type = type;
    opt->next = args->options;
    args->options = opt;
    
    return opt;
}

static cli_result_t parse_option_value(cli_parsed_option_t *opt, const char *value,
                                      cli_option_type_t type)
{
    switch (type) {
        case CLI_OPT_STRING:
            opt->value.string_value = strdup(value);
            break;
        case CLI_OPT_INT:
            opt->value.int_value = atoi(value);
            break;
        case CLI_OPT_FLOAT:
            opt->value.float_value = (float)atof(value);
            break;
        case CLI_OPT_BOOL:
            if (strcasecmp(value, "true") == 0 || strcasecmp(value, "1") == 0 ||
                strcasecmp(value, "yes") == 0) {
                opt->value.bool_value = true;
            } else if (strcasecmp(value, "false") == 0 || strcasecmp(value, "0") == 0 ||
                      strcasecmp(value, "no") == 0) {
                opt->value.bool_value = false;
            } else {
                return CLI_ERROR_INVALID_OPTION_VALUE;
            }
            break;
        case CLI_OPT_FLAG:
            opt->value.bool_value = true;
            break;
    }
    
    return CLI_SUCCESS;
}

cli_result_t cli_parse_args(cli_context_t *ctx, int argc, char **argv, cli_args_t **args)
{
    if (!ctx || !args || argc < 2) return CLI_ERROR_INVALID_ARGS;
    
    *args = calloc(1, sizeof(cli_args_t));
    if (!*args) return CLI_ERROR_GENERIC;
    
    (*args)->argc = argc;
    (*args)->argv = argv;
    (*args)->arg_index = 1;
    (*args)->options = NULL;
    (*args)->positional_args = malloc(CLI_MAX_POSITIONAL_ARGS * sizeof(char*));
    (*args)->positional_count = 0;
    
    if (argc < 2) {
        cli_print_help(ctx);
        return CLI_ERROR_INVALID_ARGS;
    }
    
    const char *command_name = argv[1];
    cli_command_t *cmd = ctx->commands;
    while (cmd && strcmp(cmd->name, command_name) != 0) {
        cmd = cmd->next;
    }
    
    if (!cmd) {
        fprintf(stderr, "Error: Unknown command '%s'\n", command_name);
        return CLI_ERROR_COMMAND_NOT_FOUND;
    }
    
    (*args)->arg_index = 2;
    
    for (int i = 2; i < argc; i++) {
        char *arg = argv[i];
        
        if (arg[0] == '-') {
            const char *opt_name;
            const char *opt_value = NULL;
            cli_option_t *opt_def = NULL;
            
            if (arg[1] == '-') {
                opt_name = arg + 2;
                char *eq_pos = strchr(opt_name, '=');
                if (eq_pos) {
                    *eq_pos = '\0';
                    opt_value = eq_pos + 1;
                }
                
                cli_option_t *opt = cmd->options;
                while (opt && (!opt->long_name || strcmp(opt->long_name, opt_name) != 0)) {
                    opt = opt->next;
                }
                opt_def = opt;
                
            } else {
                opt_name = arg + 1;
                
                cli_option_t *opt = cmd->options;
                while (opt && (!opt->short_name || strcmp(opt->short_name, opt_name) != 0)) {
                    opt = opt->next;
                }
                opt_def = opt;
            }
            
            if (!opt_def) {
                fprintf(stderr, "Error: Unknown option '%s'\n", arg);
                cli_free_args(*args);
                *args = NULL;
                return CLI_ERROR_INVALID_ARGS;
            }
            
            // Check if this is a help request
            if ((opt_def->long_name && strcmp(opt_def->long_name, "help") == 0) ||
                (opt_def->short_name && strcmp(opt_def->short_name, "h") == 0)) {
                cli_print_command_help(ctx, command_name);
                cli_free_args(*args);
                *args = NULL;
                return CLI_SUCCESS;
            }
            
            cli_parsed_option_t *parsed_opt = add_parsed_option(*args, 
                                                               opt_def->long_name ? opt_def->long_name : opt_def->short_name,
                                                               opt_def->type);
            if (!parsed_opt) {
                cli_free_args(*args);
                *args = NULL;
                return CLI_ERROR_GENERIC;
            }
            
            if (opt_def->type == CLI_OPT_FLAG) {
                parsed_opt->value.bool_value = true;
            } else {
                if (!opt_value) {
                    if (i + 1 >= argc) {
                        fprintf(stderr, "Error: Option '%s' requires a value\n", arg);
                        cli_free_args(*args);
                        *args = NULL;
                        return CLI_ERROR_INVALID_ARGS;
                    }
                    opt_value = argv[++i];
                }
                
                cli_result_t result = parse_option_value(parsed_opt, opt_value, opt_def->type);
                if (result != CLI_SUCCESS) {
                    fprintf(stderr, "Error: Invalid value '%s' for option '%s'\n", opt_value, arg);
                    cli_free_args(*args);
                    *args = NULL;
                    return result;
                }
            }
            
        } else {
            if ((*args)->positional_count < CLI_MAX_POSITIONAL_ARGS) {
                (*args)->positional_args[(*args)->positional_count++] = strdup(arg);
            }
        }
    }
    
    cli_option_t *opt = cmd->options;
    while (opt) {
        if (opt->required) {
            const char *opt_name = opt->long_name ? opt->long_name : opt->short_name;
            if (!cli_has_option(*args, opt_name)) {
                fprintf(stderr, "Error: Required option '%s' is missing\n", opt_name);
                cli_free_args(*args);
                *args = NULL;
                return CLI_ERROR_MISSING_REQUIRED_OPTION;
            }
        }
        opt = opt->next;
    }
    
    return CLI_SUCCESS;
}

void cli_free_args(cli_args_t *args)
{
    if (!args) return;
    
    cli_parsed_option_t *opt = args->options;
    while (opt) {
        cli_parsed_option_t *next = opt->next;
        if (opt->name) free((void*)opt->name);
        if (opt->type == CLI_OPT_STRING && opt->value.string_value) {
            free((void*)opt->value.string_value);
        }
        free(opt);
        opt = next;
    }
    
    for (int i = 0; i < args->positional_count; i++) {
        if (args->positional_args[i]) {
            free(args->positional_args[i]);
        }
    }
    
    if (args->positional_args) free(args->positional_args);
    free(args);
}

const char *cli_get_option_string(cli_args_t *args, const char *name, const char *default_value)
{
    if (!args || !name) return default_value;
    
    cli_parsed_option_t *opt = args->options;
    while (opt) {
        if (strcmp(opt->name, name) == 0 && opt->type == CLI_OPT_STRING) {
            return opt->value.string_value;
        }
        opt = opt->next;
    }
    
    return default_value;
}

int cli_get_option_int(cli_args_t *args, const char *name, int default_value)
{
    if (!args || !name) return default_value;
    
    cli_parsed_option_t *opt = args->options;
    while (opt) {
        if (strcmp(opt->name, name) == 0 && opt->type == CLI_OPT_INT) {
            return opt->value.int_value;
        }
        opt = opt->next;
    }
    
    return default_value;
}

float cli_get_option_float(cli_args_t *args, const char *name, float default_value)
{
    if (!args || !name) return default_value;
    
    cli_parsed_option_t *opt = args->options;
    while (opt) {
        if (strcmp(opt->name, name) == 0 && opt->type == CLI_OPT_FLOAT) {
            return opt->value.float_value;
        }
        opt = opt->next;
    }
    
    return default_value;
}

bool cli_get_option_bool(cli_args_t *args, const char *name, bool default_value)
{
    if (!args || !name) return default_value;
    
    cli_parsed_option_t *opt = args->options;
    while (opt) {
        if (strcmp(opt->name, name) == 0 && 
            (opt->type == CLI_OPT_BOOL || opt->type == CLI_OPT_FLAG)) {
            return opt->value.bool_value;
        }
        opt = opt->next;
    }
    
    return default_value;
}

bool cli_has_option(cli_args_t *args, const char *name)
{
    if (!args || !name) return false;
    
    cli_parsed_option_t *opt = args->options;
    while (opt) {
        if (strcmp(opt->name, name) == 0) {
            return true;
        }
        opt = opt->next;
    }
    
    return false;
}

const char *cli_get_positional_arg(cli_args_t *args, int index)
{
    if (!args || index < 0 || index >= args->positional_count) {
        return NULL;
    }
    
    return args->positional_args[index];
}

int cli_get_positional_count(cli_args_t *args)
{
    return args ? args->positional_count : 0;
}

cli_result_t cli_execute_command(cli_context_t *ctx, const char *command_name, cli_args_t *args)
{
    if (!ctx || !command_name || !args) return CLI_ERROR_INVALID_ARGS;
    
    cli_command_t *cmd = ctx->commands;
    while (cmd && strcmp(cmd->name, command_name) != 0) {
        cmd = cmd->next;
    }
    
    if (!cmd) return CLI_ERROR_COMMAND_NOT_FOUND;
    
    return cmd->handler(ctx, args);
}

cli_result_t cli_run(cli_context_t *ctx, int argc, char **argv)
{
    if (!ctx || argc < 2) {
        cli_print_help(ctx);
        return CLI_ERROR_INVALID_ARGS;
    }
    
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        cli_print_help(ctx);
        return CLI_SUCCESS;
    }
    
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        cli_print_version();
        return CLI_SUCCESS;
    }
    
    const char *command_name = argv[1];
    
    // Check if user is requesting help for a specific command
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        cli_print_command_help(ctx, command_name);
        return CLI_SUCCESS;
    }
    
    cli_args_t *args;
    cli_result_t result = cli_parse_args(ctx, argc, argv, &args);
    if (result != CLI_SUCCESS) {
        return result;
    }
    
    result = cli_execute_command(ctx, command_name, args);
    
    cli_free_args(args);
    return result;
}

void cli_print_help(cli_context_t *ctx)
{
    printf("Usage: %s [OPTION]... COMMAND [COMMAND-OPTION]...\n\n", ctx->program_name);
    printf("Goxel headless CLI - 3D voxel editor command-line interface\n\n");
    
    printf("Global options:\n");
    printf("  -h, --help           Show this help message\n");
    printf("  -v, --version        Show version information\n");
    printf("      --verbose        Enable verbose output\n");
    printf("      --quiet          Suppress non-error output\n");
    printf("      --config FILE    Use specified config file\n\n");
    
    printf("Available commands:\n");
    
    cli_command_t *cmd = ctx->commands;
    while (cmd) {
        printf("  %-15s  %s\n", cmd->name, cmd->description ? cmd->description : "No description available");
        cmd = cmd->next;
    }
    
    printf("\nUse '%s COMMAND --help' for detailed information about each command.\n", ctx->program_name);
    printf("All commands support -h, --help options for usage information.\n");
    
    printf("\nNote: Coordinate System\n");
    printf("  The GUI grid plane is at Y=-16 in CLI coordinates.\n");
    printf("  To place objects on the grid, use Y=-16 (GUI shows this as Y=0).\n");
}

void cli_print_command_help(cli_context_t *ctx, const char *command_name)
{
    cli_command_t *cmd = ctx->commands;
    while (cmd && strcmp(cmd->name, command_name) != 0) {
        cmd = cmd->next;
    }
    
    if (!cmd) {
        fprintf(stderr, "Error: Unknown command '%s'\n", command_name);
        return;
    }
    
    printf("Usage: %s %s", ctx->program_name, command_name);
    if (cmd->usage) {
        printf(" %s", cmd->usage);
    }
    printf("\n\n");
    
    if (cmd->description) {
        printf("%s\n\n", cmd->description);
    }
    
    if (cmd->options) {
        printf("Options:\n");
        cli_option_t *opt = cmd->options;
        while (opt) {
            printf("  ");
            if (opt->short_name) {
                printf("-%s", opt->short_name);
                if (opt->long_name) printf(", ");
            }
            if (opt->long_name) {
                printf("--%s", opt->long_name);
            }
            
            if (opt->type != CLI_OPT_FLAG) {
                printf(" VALUE");
            }
            
            if (opt->required) {
                printf(" (required)");
            }
            
            printf("\n");
            
            if (opt->description) {
                printf("                     %s\n", opt->description);
            }
            
            opt = opt->next;
        }
        printf("\n");
    }
    
    // Add coordinate note for voxel-related commands
    if (strstr(command_name, "voxel") != NULL) {
        printf("Coordinate Note:\n");
        printf("  The GUI grid plane is at Y=-16. Use Y=-16 to place objects on the grid.\n\n");
    }
}

void cli_print_version(void)
{
    printf("Goxel Headless CLI version %s\n", CLI_VERSION);
    printf("Copyright (c) 2025 Guillaume Chereau\n");
    printf("This is free software; see the source for copying conditions.\n");
}

const char *cli_error_string(cli_result_t error)
{
    switch (error) {
        case CLI_SUCCESS: return "Success";
        case CLI_ERROR_GENERIC: return "Generic error";
        case CLI_ERROR_INVALID_ARGS: return "Invalid arguments";
        case CLI_ERROR_COMMAND_NOT_FOUND: return "Command not found";
        case CLI_ERROR_MISSING_REQUIRED_OPTION: return "Missing required option";
        case CLI_ERROR_INVALID_OPTION_VALUE: return "Invalid option value";
        case CLI_ERROR_FILE_NOT_FOUND: return "File not found";
        case CLI_ERROR_PROJECT_LOAD_FAILED: return "Project load failed";
        case CLI_ERROR_PROJECT_SAVE_FAILED: return "Project save failed";
        case CLI_ERROR_RENDER_FAILED: return "Render failed";
        case CLI_ERROR_EXPORT_FAILED: return "Export failed";
        case CLI_ERROR_VOXEL_OPERATION_FAILED: return "Voxel operation failed";
        case CLI_ERROR_LAYER_OPERATION_FAILED: return "Layer operation failed";
        default: return "Unknown error";
    }
}

void cli_set_global_options(cli_context_t *ctx, bool verbose, bool quiet, const char *config_file)
{
    if (!ctx) return;
    
    ctx->verbose = verbose;
    ctx->quiet = quiet;
    
    if (ctx->config_file) {
        free((void*)ctx->config_file);
    }
    ctx->config_file = config_file ? strdup(config_file) : NULL;
}

void cli_set_goxel_context(cli_context_t *ctx, void *goxel_context)
{
    if (ctx) {
        ctx->goxel_context = goxel_context;
    }
}

cli_result_t cli_register_builtin_commands(cli_context_t *ctx)
{
    if (!ctx) return CLI_ERROR_INVALID_ARGS;
    
    // Note: Builtin commands will be registered by individual register_*_commands functions
    // This function is currently a placeholder for future expansion
    
    return CLI_SUCCESS;
}

// Interactive mode implementation
char **cli_tokenize_line(const char *line, int *argc)
{
    if (!line || !argc) return NULL;
    
    char **tokens = malloc(64 * sizeof(char*));
    if (!tokens) return NULL;
    
    char *line_copy = strdup(line);
    if (!line_copy) {
        free(tokens);
        return NULL;
    }
    
    *argc = 0;
    char *token = strtok(line_copy, " \t\n\r");
    while (token && *argc < 63) {
        tokens[*argc] = strdup(token);
        if (!tokens[*argc]) {
            // Cleanup on error
            for (int i = 0; i < *argc; i++) {
                free(tokens[i]);
            }
            free(tokens);
            free(line_copy);
            return NULL;
        }
        (*argc)++;
        token = strtok(NULL, " \t\n\r");
    }
    
    tokens[*argc] = NULL;
    free(line_copy);
    
    return tokens;
}

void cli_free_tokens(char **tokens, int argc)
{
    if (!tokens) return;
    
    for (int i = 0; i < argc; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

cli_result_t cli_execute_line(cli_context_t *ctx, const char *line)
{
    if (!ctx || !line) return CLI_ERROR_INVALID_ARGS;
    
    // Skip empty lines and comments
    const char *trimmed = line;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
    if (*trimmed == '\0' || *trimmed == '#' || *trimmed == '\n') {
        return CLI_SUCCESS;
    }
    
    // Handle special commands
    if (strncmp(trimmed, "exit", 4) == 0 || strncmp(trimmed, "quit", 4) == 0) {
        return CLI_ERROR_GENERIC; // Use as exit signal
    }
    
    // Tokenize the line
    int argc;
    char **argv = cli_tokenize_line(line, &argc);
    if (!argv || argc == 0) {
        return CLI_SUCCESS; // Empty line
    }
    
    // Create a fake argv with program name as first argument
    char **full_argv = malloc((argc + 1) * sizeof(char*));
    if (!full_argv) {
        cli_free_tokens(argv, argc);
        return CLI_ERROR_GENERIC;
    }
    
    full_argv[0] = strdup(ctx->program_name ? ctx->program_name : "goxel-headless");
    for (int i = 0; i < argc; i++) {
        full_argv[i + 1] = argv[i]; // Copy pointers
    }
    
    // Execute the command
    cli_result_t result = cli_run(ctx, argc + 1, full_argv);
    
    // Cleanup
    free(full_argv[0]);
    free(full_argv);
    cli_free_tokens(argv, argc);
    
    return result;
}

cli_result_t cli_run_interactive(cli_context_t *ctx)
{
    if (!ctx) return CLI_ERROR_INVALID_ARGS;
    
    if (!ctx->quiet) {
        printf("Goxel Headless Interactive Mode\n");
        printf("Type 'help' for available commands, 'exit' to quit\n\n");
    }
    
    char line[1024];
    cli_result_t result = CLI_SUCCESS;
    
    while (1) {
        if (!ctx->quiet) {
            printf("goxel> ");
            fflush(stdout);
        }
        
        if (!fgets(line, sizeof(line), stdin)) {
            // EOF or error
            break;
        }
        
        result = cli_execute_line(ctx, line);
        
        // Check for exit condition
        if (result == CLI_ERROR_GENERIC) {
            if (!ctx->quiet) {
                printf("Goodbye!\n");
            }
            result = CLI_SUCCESS;
            break;
        }
        
        // Print error messages
        if (result != CLI_SUCCESS && !ctx->quiet) {
            fprintf(stderr, "Error: %s\n", cli_error_string(result));
        }
    }
    
    return result;
}