#define _GNU_SOURCE  // For strdup
#include "tdd_framework.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Implement strdup if not available
#ifndef strdup
char* strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}
#endif

typedef struct {
    char* data;
    size_t size;
} buffer_t;

typedef struct {
    char* method;
    char* params_json;
    int id;
} jsonrpc_request_t;

typedef struct {
    bool success;
    char* result_json;
    char* error_message;
    int id;
} jsonrpc_response_t;

jsonrpc_request_t* parse_jsonrpc_request(const char* json) {
    if (!json) return NULL;
    
    jsonrpc_request_t* req = malloc(sizeof(jsonrpc_request_t));
    if (!req) return NULL;
    
    req->method = NULL;
    req->params_json = NULL;
    req->id = -1;
    
    const char* method_start = strstr(json, "\"method\"");
    if (method_start) {
        method_start = strchr(method_start + 8, '\"');
        if (method_start) {
            method_start++;
            const char* method_end = strchr(method_start, '\"');
            if (method_end) {
                size_t len = method_end - method_start;
                req->method = malloc(len + 1);
                strncpy(req->method, method_start, len);
                req->method[len] = '\0';
            }
        }
    }
    
    const char* params_start = strstr(json, "\"params\"");
    if (params_start) {
        params_start = strchr(params_start + 8, ':');
        if (params_start) {
            params_start++;
            while (*params_start == ' ') params_start++;
            
            if (*params_start == '{') {
                const char* params_end = params_start + 1;
                int brace_count = 1;
                while (brace_count > 0 && *params_end) {
                    if (*params_end == '{') brace_count++;
                    else if (*params_end == '}') brace_count--;
                    params_end++;
                }
                
                if (brace_count == 0) {
                    size_t len = params_end - params_start;
                    req->params_json = malloc(len + 1);
                    strncpy(req->params_json, params_start, len);
                    req->params_json[len] = '\0';
                }
            } else if (*params_start == '[') {
                const char* params_end = params_start + 1;
                int bracket_count = 1;
                while (bracket_count > 0 && *params_end) {
                    if (*params_end == '[') bracket_count++;
                    else if (*params_end == ']') bracket_count--;
                    params_end++;
                }
                
                if (bracket_count == 0) {
                    size_t len = params_end - params_start;
                    req->params_json = malloc(len + 1);
                    strncpy(req->params_json, params_start, len);
                    req->params_json[len] = '\0';
                }
            }
        }
    }
    
    // Find ID but skip if it's inside params
    const char* id_start = json;
    while ((id_start = strstr(id_start, "\"id\"")) != NULL) {
        // Check if this "id" is inside params by looking backwards
        const char* params_check = strstr(json, "\"params\"");
        if (params_check && id_start > params_check) {
            // Check if we're still inside params by counting braces/brackets
            const char* p = params_check;
            int depth = 0;
            while (p < id_start) {
                if (*p == '{' || *p == '[') depth++;
                else if (*p == '}' || *p == ']') depth--;
                p++;
            }
            if (depth > 0) {
                // We're inside params, skip this "id"
                id_start += 4;
                continue;
            }
        }
        
        // This is the request ID
        id_start = strchr(id_start + 4, ':');
        if (id_start) {
            req->id = atoi(id_start + 1);
        }
        break;
    }
    
    return req;
}

void free_jsonrpc_request(jsonrpc_request_t* req) {
    if (req) {
        free(req->method);
        free(req->params_json);
        free(req);
    }
}

jsonrpc_response_t* create_success_response(int id, const char* result) {
    jsonrpc_response_t* resp = malloc(sizeof(jsonrpc_response_t));
    if (!resp) return NULL;
    
    resp->success = true;
    resp->id = id;
    resp->error_message = NULL;
    
    if (result) {
        resp->result_json = strdup(result);
    } else {
        resp->result_json = strdup("\"success\"");
    }
    
    if (!resp->result_json) {
        free(resp);
        return NULL;
    }
    
    return resp;
}

jsonrpc_response_t* create_error_response(int id, const char* error) {
    jsonrpc_response_t* resp = malloc(sizeof(jsonrpc_response_t));
    if (!resp) return NULL;
    
    resp->success = false;
    resp->id = id;
    resp->result_json = NULL;
    resp->error_message = strdup(error);
    
    if (!resp->error_message) {
        free(resp);
        return NULL;
    }
    
    return resp;
}

void free_jsonrpc_response(jsonrpc_response_t* resp) {
    if (resp) {
        free(resp->result_json);
        free(resp->error_message);
        free(resp);
    }
}

char* serialize_jsonrpc_response(jsonrpc_response_t* resp) {
    if (!resp) return NULL;
    
    char buffer[1024];
    if (resp->success) {
        snprintf(buffer, sizeof(buffer),
                 "{\"jsonrpc\":\"2.0\",\"result\":%s,\"id\":%d}",
                 resp->result_json, resp->id);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"%s\"},\"id\":%d}",
                 resp->error_message, resp->id);
    }
    
    return strdup(buffer);
}

jsonrpc_response_t* handle_create_project(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.create_project") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    return create_success_response(req->id, "{\"project_id\":\"test-123\"}");
}

jsonrpc_response_t* handle_add_voxels(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.add_voxels") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Parse params to count voxels
    if (!req->params_json) {
        return create_error_response(req->id, "Missing params");
    }
    
    // Simple voxel counting - look for "position" occurrences
    int voxel_count = 0;
    const char* pos = req->params_json;
    while ((pos = strstr(pos, "\"position\"")) != NULL) {
        voxel_count++;
        pos += 10; // Move past "position"
    }
    
    if (voxel_count == 0) {
        return create_error_response(req->id, "No voxels to add");
    }
    
    // Create success response with count
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"added\":true,\"count\":%d}", voxel_count);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_remove_voxels(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.remove_voxels") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Parse params to count voxels
    if (!req->params_json) {
        return create_error_response(req->id, "Missing params");
    }
    
    // Simple voxel counting - look for "position" occurrences
    int voxel_count = 0;
    const char* pos = req->params_json;
    while ((pos = strstr(pos, "\"position\"")) != NULL) {
        voxel_count++;
        pos += 10; // Move past "position"
    }
    
    if (voxel_count == 0) {
        return create_error_response(req->id, "No voxels to remove");
    }
    
    // Create success response with count
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"removed\":true,\"count\":%d}", voxel_count);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_paint_voxels(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.paint_voxels") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Parse params to validate and count voxels
    if (!req->params_json) {
        return create_error_response(req->id, "Missing params");
    }
    
    // Count voxels with both position and color
    int voxel_count = 0;
    const char* pos = req->params_json;
    
    // Check if there are any voxels in the array
    const char* voxel_array = strstr(req->params_json, "\"voxels\"");
    if (voxel_array) {
        const char* array_start = strchr(voxel_array, '[');
        if (array_start) {
            const char* array_end = strchr(array_start, ']');
            if (array_end && array_end - array_start <= 2) {
                // Empty array
                return create_error_response(req->id, "No voxels to paint");
            }
        }
    }
    
    // Count voxels that have position
    while ((pos = strstr(pos, "\"position\"")) != NULL) {
        voxel_count++;
        pos += 10;
    }
    
    // Check if all voxels have colors
    int color_count = 0;
    pos = req->params_json;
    while ((pos = strstr(pos, "\"color\"")) != NULL) {
        color_count++;
        pos += 7;
    }
    
    if (voxel_count > color_count) {
        return create_error_response(req->id, "Missing color for voxel");
    }
    
    if (voxel_count == 0) {
        return create_error_response(req->id, "No voxels to paint");
    }
    
    // Create success response with count
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"painted\":true,\"count\":%d}", voxel_count);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_open_file(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.open_file") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Check if params exist
    if (!req->params_json) {
        return create_error_response(req->id, "Missing file path");
    }
    
    // Extract file path from array params
    char file_path[256] = {0};
    const char* start = strchr(req->params_json, '\"');
    if (start) {
        start++; // Skip opening quote
        const char* end = strchr(start, '\"');
        if (end && end > start) {
            size_t len = end - start;
            if (len < sizeof(file_path)) {
                strncpy(file_path, start, len);
                file_path[len] = '\0';
            }
        }
    }
    
    // Validate file path
    if (strlen(file_path) == 0) {
        return create_error_response(req->id, "Invalid file path");
    }
    
    // Check file extension (support .gox, .vox, .obj, .ply)
    const char* extensions[] = {".gox", ".vox", ".obj", ".ply", ".png", ".stl", NULL};
    bool valid_extension = false;
    
    for (int i = 0; extensions[i] != NULL; i++) {
        size_t ext_len = strlen(extensions[i]);
        size_t path_len = strlen(file_path);
        if (path_len >= ext_len) {
            if (strcmp(file_path + path_len - ext_len, extensions[i]) == 0) {
                valid_extension = true;
                break;
            }
        }
    }
    
    if (!valid_extension) {
        return create_error_response(req->id, "Unsupported file format");
    }
    
    // Create success response
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"opened\":true,\"file\":\"%s\"}", file_path);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_save_file(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.save_file") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Check if params exist
    if (!req->params_json) {
        return create_error_response(req->id, "Missing file path");
    }
    
    // Extract file path from array params
    char file_path[256] = {0};
    const char* start = strchr(req->params_json, '"');
    if (start) {
        start++; // Skip opening quote
        const char* end = strchr(start, '"');
        if (end && end > start) {
            size_t len = end - start;
            if (len < sizeof(file_path)) {
                strncpy(file_path, start, len);
                file_path[len] = '\0';
            }
        }
    }
    
    // Validate file path
    if (strlen(file_path) == 0) {
        return create_error_response(req->id, "Invalid file path");
    }
    
    // Validate extension - only .gox is supported for saving
    if (strlen(file_path) < 4 || strcmp(file_path + strlen(file_path) - 4, ".gox") != 0) {
        return create_error_response(req->id, "Save file must have .gox extension");
    }
    
    // Create success response
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"saved\":true,\"path\":\"%s\"}", file_path);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_export_file(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.export_file") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Check if params exist
    if (!req->params_json) {
        return create_error_response(req->id, "Missing parameters");
    }
    
    // Extract file path and format from array params
    char file_path[256] = {0};
    char format[32] = {0};
    
    // First extract file path
    const char* start = strchr(req->params_json, '"');
    if (start) {
        start++; // Skip opening quote
        const char* end = strchr(start, '"');
        if (end && end > start) {
            size_t len = end - start;
            if (len < sizeof(file_path)) {
                strncpy(file_path, start, len);
                file_path[len] = '\0';
            }
        }
        
        // Now extract format (second parameter)
        start = strchr(end + 1, '"');
        if (start) {
            start++; // Skip opening quote
            end = strchr(start, '"');
            if (end && end > start) {
                size_t len = end - start;
                if (len < sizeof(format)) {
                    strncpy(format, start, len);
                    format[len] = '\0';
                }
            }
        }
    }
    
    // Validate file path
    if (strlen(file_path) == 0) {
        return create_error_response(req->id, "Invalid file path");
    }
    
    // Validate format
    if (strlen(format) == 0) {
        return create_error_response(req->id, "Missing export format");
    }
    
    // Check supported formats
    const char* supported_formats[] = {"obj", "ply", "stl", "png", "vox", "magica", NULL};
    bool valid_format = false;
    
    for (int i = 0; supported_formats[i] != NULL; i++) {
        if (strcmp(format, supported_formats[i]) == 0) {
            valid_format = true;
            break;
        }
    }
    
    if (!valid_format) {
        return create_error_response(req->id, "Unsupported export format");
    }
    
    // Create success response
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"exported\":true,\"path\":\"%s\",\"format\":\"%s\"}", file_path, format);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_get_voxel(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.get_voxel") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Check if params exist
    if (!req->params_json) {
        return create_error_response(req->id, "Missing position");
    }
    
    // Extract position from params
    // Expected format: {"position": [x, y, z]}
    const char* pos_start = strstr(req->params_json, "\"position\"");
    if (!pos_start) {
        return create_error_response(req->id, "Missing position");
    }
    
    // Find array start
    pos_start = strchr(pos_start, '[');
    if (!pos_start) {
        return create_error_response(req->id, "Invalid position format");
    }
    
    // Parse coordinates
    int x = 0, y = 0, z = 0;
    if (sscanf(pos_start, "[%d,%d,%d]", &x, &y, &z) != 3) {
        return create_error_response(req->id, "Invalid position coordinates");
    }
    
    // Check bounds (example: -100 to 100)
    if (x < -100 || x > 100 || y < -100 || y > 100 || z < -100 || z > 100) {
        return create_error_response(req->id, "Position out of bounds");
    }
    
    // Create response based on position
    // For testing, we'll return a color if position sum is even, null if odd
    char result_json[256];
    if ((x + y + z) % 2 == 0) {
        snprintf(result_json, sizeof(result_json), 
                 "{\"position\":[%d,%d,%d],\"color\":\"#FF0000\",\"exists\":true}", 
                 x, y, z);
    } else {
        snprintf(result_json, sizeof(result_json), 
                 "{\"position\":[%d,%d,%d],\"color\":null,\"exists\":false}", 
                 x, y, z);
    }
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_list_layers(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.list_layers") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // For testing, return a fixed list of layers
    const char* result_json = 
        "{\"layers\":["
        "{\"id\":1,\"name\":\"Layer 1\",\"visible\":true,\"active\":true},"
        "{\"id\":2,\"name\":\"Background\",\"visible\":true,\"active\":false},"
        "{\"id\":3,\"name\":\"Details\",\"visible\":false,\"active\":false}"
        "],\"count\":3}";
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_create_layer(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.create_layer") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Check if params exist
    if (!req->params_json) {
        return create_error_response(req->id, "Missing parameters");
    }
    
    // Extract layer name from params
    char layer_name[128] = {0};
    const char* name_start = strstr(req->params_json, "\"name\"");
    if (name_start) {
        name_start = strchr(name_start + 6, '"');
        if (name_start) {
            name_start++;
            const char* name_end = strchr(name_start, '"');
            if (name_end && name_end > name_start) {
                size_t len = name_end - name_start;
                if (len < sizeof(layer_name)) {
                    strncpy(layer_name, name_start, len);
                    layer_name[len] = '\0';
                }
            }
        }
    }
    
    // Validate layer name
    if (strlen(layer_name) == 0) {
        return create_error_response(req->id, "Layer name cannot be empty");
    }
    
    if (strlen(layer_name) > 64) {
        return create_error_response(req->id, "Layer name too long");
    }
    
    // Check if layer already exists (for testing, reject "Layer 1" and "Background")
    if (strcmp(layer_name, "Layer 1") == 0 || strcmp(layer_name, "Background") == 0) {
        return create_error_response(req->id, "Layer already exists");
    }
    
    // Create success response
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"layer\":{\"id\":4,\"name\":\"%s\",\"visible\":true,\"active\":true}}", 
             layer_name);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_delete_layer(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.delete_layer") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Check if params exist
    if (!req->params_json) {
        return create_error_response(req->id, "Missing layer ID");
    }
    
    // Extract layer ID from params
    int layer_id = -1;
    const char* id_start = strstr(req->params_json, "\"id\"");
    if (id_start) {
        id_start = strchr(id_start + 4, ':');
        if (id_start) {
            layer_id = atoi(id_start + 1);
        }
    }
    
    // Validate layer ID
    if (layer_id <= 0) {
        return create_error_response(req->id, "Invalid layer ID");
    }
    
    // Cannot delete last layer (ID 1 in our test)
    if (layer_id == 1) {
        return create_error_response(req->id, "Cannot delete last layer");
    }
    
    // Layer must exist (for testing, we have layers 1, 2, 3)
    if (layer_id > 3) {
        return create_error_response(req->id, "Layer not found");
    }
    
    // Create success response
    char result_json[128];
    snprintf(result_json, sizeof(result_json), 
             "{\"deleted\":true,\"layer_id\":%d}", layer_id);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_flood_fill(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.flood_fill") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Check if params exist
    if (!req->params_json) {
        return create_error_response(req->id, "Missing parameters");
    }
    
    // Extract position
    const char* pos_start = strstr(req->params_json, "\"position\"");
    if (!pos_start) {
        return create_error_response(req->id, "Missing position");
    }
    
    pos_start = strchr(pos_start, '[');
    if (!pos_start) {
        return create_error_response(req->id, "Invalid position format");
    }
    
    int x = 0, y = 0, z = 0;
    if (sscanf(pos_start, "[%d,%d,%d]", &x, &y, &z) != 3) {
        return create_error_response(req->id, "Invalid position coordinates");
    }
    
    // Extract color
    char color[32] = {0};
    const char* color_start = strstr(req->params_json, "\"color\"");
    if (!color_start) {
        return create_error_response(req->id, "Missing color");
    }
    
    color_start = strchr(color_start + 7, '"');
    if (color_start) {
        color_start++;
        const char* color_end = strchr(color_start, '"');
        if (color_end && color_end > color_start) {
            size_t len = color_end - color_start;
            if (len < sizeof(color)) {
                strncpy(color, color_start, len);
                color[len] = '\0';
            }
        }
    }
    
    // Validate color format (should start with #)
    if (strlen(color) == 0 || color[0] != '#') {
        return create_error_response(req->id, "Invalid color format");
    }
    
    // For testing, we'll return a fixed number of filled voxels
    int filled_count = (x + y + z) % 10 + 5; // 5-14 voxels filled
    
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"filled\":true,\"count\":%d,\"position\":[%d,%d,%d],\"color\":\"%s\"}", 
             filled_count, x, y, z, color);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_procedural_shape(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.procedural_shape") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Check if params exist
    if (!req->params_json) {
        return create_error_response(req->id, "Missing parameters");
    }
    
    // Extract shape type
    char shape[32] = {0};
    const char* shape_start = strstr(req->params_json, "\"shape\"");
    if (!shape_start) {
        return create_error_response(req->id, "Missing shape type");
    }
    
    shape_start = strchr(shape_start + 7, '"');
    if (shape_start) {
        shape_start++;
        const char* shape_end = strchr(shape_start, '"');
        if (shape_end && shape_end > shape_start) {
            size_t len = shape_end - shape_start;
            if (len < sizeof(shape)) {
                strncpy(shape, shape_start, len);
                shape[len] = '\0';
            }
        }
    }
    
    // Validate shape type
    const char* valid_shapes[] = {"sphere", "cube", "cylinder", "cone", "torus", NULL};
    bool valid_shape = false;
    for (int i = 0; valid_shapes[i] != NULL; i++) {
        if (strcmp(shape, valid_shapes[i]) == 0) {
            valid_shape = true;
            break;
        }
    }
    
    if (!valid_shape) {
        return create_error_response(req->id, "Invalid shape type");
    }
    
    // Extract size
    int size = 0;
    const char* size_start = strstr(req->params_json, "\"size\"");
    if (size_start) {
        size_start = strchr(size_start + 6, ':');
        if (size_start) {
            size = atoi(size_start + 1);
        }
    }
    
    if (size <= 0) {
        return create_error_response(req->id, "Invalid size");
    }
    
    if (size > 100) {
        return create_error_response(req->id, "Size too large");
    }
    
    // Extract position (optional, defaults to origin)
    int x = 0, y = 0, z = 0;
    const char* pos_start = strstr(req->params_json, "\"position\"");
    if (pos_start) {
        pos_start = strchr(pos_start, '[');
        if (pos_start) {
            sscanf(pos_start, "[%d,%d,%d]", &x, &y, &z);
        }
    }
    
    // Create response
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"created\":true,\"shape\":\"%s\",\"size\":%d,\"position\":[%d,%d,%d]}", 
             shape, size, x, y, z);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_batch_operations(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.batch_operations") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Check if params exist
    if (!req->params_json) {
        return create_error_response(req->id, "Missing parameters");
    }
    
    // Check for operations array
    const char* ops_start = strstr(req->params_json, "\"operations\"");
    if (!ops_start) {
        return create_error_response(req->id, "Missing operations array");
    }
    
    // Find array start
    ops_start = strchr(ops_start, '[');
    if (!ops_start) {
        return create_error_response(req->id, "Invalid operations format");
    }
    
    // Count operations (simple approach - count "type" occurrences)
    int op_count = 0;
    const char* pos = ops_start;
    while ((pos = strstr(pos, "\"type\"")) != NULL) {
        op_count++;
        pos += 6;
    }
    
    if (op_count == 0) {
        return create_error_response(req->id, "Empty operations array");
    }
    
    if (op_count > 1000) {
        return create_error_response(req->id, "Too many operations");
    }
    
    // Validate operation types
    const char* type_pos = ops_start;
    while ((type_pos = strstr(type_pos, "\"type\"")) != NULL) {
        type_pos = strchr(type_pos + 6, '"');
        if (type_pos) {
            type_pos++;
            char op_type[32] = {0};
            const char* type_end = strchr(type_pos, '"');
            if (type_end && type_end > type_pos) {
                size_t len = type_end - type_pos;
                if (len < sizeof(op_type)) {
                    strncpy(op_type, type_pos, len);
                    op_type[len] = '\0';
                }
            }
            
            // Validate operation type
            const char* valid_ops[] = {"add", "remove", "paint", "fill", NULL};
            bool valid_op = false;
            for (int i = 0; valid_ops[i] != NULL; i++) {
                if (strcmp(op_type, valid_ops[i]) == 0) {
                    valid_op = true;
                    break;
                }
            }
            
            if (!valid_op) {
                return create_error_response(req->id, "Invalid operation type");
            }
            
            type_pos = type_end;
        }
    }
    
    // Create success response
    char result_json[256];
    int successful = op_count;
    int failed = 0;
    snprintf(result_json, sizeof(result_json), 
             "{\"completed\":true,\"total\":%d,\"successful\":%d,\"failed\":%d}", 
             op_count, successful, failed);
    
    return create_success_response(req->id, result_json);
}

jsonrpc_response_t* handle_render_scene(jsonrpc_request_t* req) {
    if (!req) return NULL;
    
    if (strcmp(req->method, "goxel.render_scene") != 0) {
        return create_error_response(req->id, "Invalid method");
    }
    
    // Check if params exist
    if (!req->params_json) {
        return create_error_response(req->id, "Missing parameters");
    }
    
    // Extract width
    int width = 0;
    const char* width_start = strstr(req->params_json, "\"width\"");
    if (width_start) {
        width_start = strchr(width_start + 7, ':');
        if (width_start) {
            width = atoi(width_start + 1);
        }
    }
    
    // Extract height
    int height = 0;
    const char* height_start = strstr(req->params_json, "\"height\"");
    if (height_start) {
        height_start = strchr(height_start + 8, ':');
        if (height_start) {
            height = atoi(height_start + 1);
        }
    }
    
    // Validate dimensions
    if (width <= 0 || height <= 0) {
        return create_error_response(req->id, "Invalid dimensions");
    }
    
    if (width > 4096 || height > 4096) {
        return create_error_response(req->id, "Dimensions too large");
    }
    
    // Extract format (optional, defaults to png)
    char format[32] = "png";
    const char* format_start = strstr(req->params_json, "\"format\"");
    if (format_start) {
        format_start = strchr(format_start + 8, '"');
        if (format_start) {
            format_start++;
            const char* format_end = strchr(format_start, '"');
            if (format_end && format_end > format_start) {
                size_t len = format_end - format_start;
                if (len < sizeof(format)) {
                    strncpy(format, format_start, len);
                    format[len] = '\0';
                }
            }
        }
    }
    
    // Validate format
    const char* valid_formats[] = {"png", "jpg", "jpeg", "bmp", NULL};
    bool valid_format = false;
    for (int i = 0; valid_formats[i] != NULL; i++) {
        if (strcmp(format, valid_formats[i]) == 0) {
            valid_format = true;
            break;
        }
    }
    
    if (!valid_format) {
        return create_error_response(req->id, "Invalid image format");
    }
    
    // Create success response
    char result_json[256];
    snprintf(result_json, sizeof(result_json), 
             "{\"rendered\":true,\"width\":%d,\"height\":%d,\"format\":\"%s\",\"data\":\"base64_image_data_here\"}", 
             width, height, format);
    
    return create_success_response(req->id, result_json);
}

int test_parse_valid_request() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"id\":42}";
    
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    TEST_ASSERT(req != NULL, "Request should be parsed");
    TEST_ASSERT_STR_EQ("goxel.create_project", req->method);
    TEST_ASSERT_EQ(42, req->id);
    
    free_jsonrpc_request(req);
    return 1;
}

int test_parse_null_request() {
    jsonrpc_request_t* req = parse_jsonrpc_request(NULL);
    TEST_ASSERT(req == NULL, "NULL input should return NULL");
    return 1;
}

int test_create_success_response() {
    jsonrpc_response_t* resp = create_success_response(123, "{\"status\":\"ok\"}");
    
    TEST_ASSERT(resp != NULL, "Response should be created");
    TEST_ASSERT(resp->success == true, "Should be success");
    TEST_ASSERT_EQ(123, resp->id);
    TEST_ASSERT_STR_EQ("{\"status\":\"ok\"}", resp->result_json);
    TEST_ASSERT(resp->error_message == NULL, "No error message for success");
    
    free_jsonrpc_response(resp);
    return 1;
}

int test_create_error_response() {
    jsonrpc_response_t* resp = create_error_response(456, "Something went wrong");
    
    TEST_ASSERT(resp != NULL, "Response should be created");
    TEST_ASSERT(resp->success == false, "Should be error");
    TEST_ASSERT_EQ(456, resp->id);
    TEST_ASSERT(resp->result_json == NULL, "No result for error");
    TEST_ASSERT_STR_EQ("Something went wrong", resp->error_message);
    
    free_jsonrpc_response(resp);
    return 1;
}

int test_serialize_success_response() {
    jsonrpc_response_t* resp = create_success_response(1, "\"done\"");
    char* json = serialize_jsonrpc_response(resp);
    
    TEST_ASSERT(json != NULL, "JSON should be created");
    TEST_ASSERT(strstr(json, "\"jsonrpc\":\"2.0\"") != NULL, "Should have jsonrpc version");
    TEST_ASSERT(strstr(json, "\"result\":\"done\"") != NULL, "Should have result");
    TEST_ASSERT(strstr(json, "\"id\":1") != NULL, "Should have id");
    
    free(json);
    free_jsonrpc_response(resp);
    return 1;
}

int test_serialize_error_response() {
    jsonrpc_response_t* resp = create_error_response(2, "Not found");
    char* json = serialize_jsonrpc_response(resp);
    
    TEST_ASSERT(json != NULL, "JSON should be created");
    TEST_ASSERT(strstr(json, "\"error\"") != NULL, "Should have error");
    TEST_ASSERT(strstr(json, "\"message\":\"Not found\"") != NULL, "Should have error message");
    TEST_ASSERT(strstr(json, "\"id\":2") != NULL, "Should have id");
    
    free(json);
    free_jsonrpc_response(resp);
    return 1;
}

int test_handle_create_project_valid() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_project\",\"id\":99}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_create_project(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "project_id") != NULL, "Should have project_id");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_create_project_wrong_method() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.unknown\",\"id\":99}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_create_project(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error");
    TEST_ASSERT_STR_EQ("Invalid method", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_add_voxels_single() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxels\",\"params\":{\"voxels\":[{\"position\":[0,0,0],\"color\":\"#FF0000\"}]},\"id\":1}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_add_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(1, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "added") != NULL, "Should indicate voxels were added");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_add_voxels_multiple() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxels\",\"params\":{\"voxels\":[{\"position\":[0,0,0],\"color\":\"#FF0000\"},{\"position\":[1,0,0],\"color\":\"#00FF00\"},{\"position\":[2,0,0],\"color\":\"#0000FF\"}]},\"id\":2}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_add_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "\"count\":3") != NULL, "Should report 3 voxels added");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_add_voxels_empty_array() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.add_voxels\",\"params\":{\"voxels\":[]},\"id\":3}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_add_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error for empty voxel array");
    TEST_ASSERT_STR_EQ("No voxels to add", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_remove_voxels_single() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.remove_voxels\",\"params\":{\"voxels\":[{\"position\":[5,5,5]}]},\"id\":10}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_remove_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(10, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "removed") != NULL, "Should indicate voxels were removed");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_remove_voxels_multiple() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.remove_voxels\",\"params\":{\"voxels\":[{\"position\":[0,0,0]},{\"position\":[1,1,1]},{\"position\":[2,2,2]},{\"position\":[3,3,3]}]},\"id\":11}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_remove_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "\"count\":4") != NULL, "Should report 4 voxels removed");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_remove_voxels_empty() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.remove_voxels\",\"params\":{\"voxels\":[]},\"id\":12}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_remove_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error for empty voxel array");
    TEST_ASSERT_STR_EQ("No voxels to remove", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_remove_voxels_invalid_method() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.invalid\",\"id\":13}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_remove_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error");
    TEST_ASSERT_STR_EQ("Invalid method", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_paint_voxels_single() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.paint_voxels\",\"params\":{\"voxels\":[{\"position\":[10,10,10],\"color\":\"#00FF00\"}]},\"id\":20}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_paint_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(20, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "painted") != NULL, "Should indicate voxels were painted");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_paint_voxels_gradient() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.paint_voxels\",\"params\":{\"voxels\":[{\"position\":[0,0,0],\"color\":\"#FF0000\"},{\"position\":[0,0,1],\"color\":\"#FF7F00\"},{\"position\":[0,0,2],\"color\":\"#FFFF00\"},{\"position\":[0,0,3],\"color\":\"#00FF00\"},{\"position\":[0,0,4],\"color\":\"#0000FF\"}]},\"id\":21}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_paint_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "\"count\":5") != NULL, "Should report 5 voxels painted");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_paint_voxels_no_color() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.paint_voxels\",\"params\":{\"voxels\":[{\"position\":[0,0,0]}]},\"id\":22}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_paint_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error for missing color");
    TEST_ASSERT_STR_EQ("Missing color for voxel", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_paint_voxels_empty() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.paint_voxels\",\"params\":{\"voxels\":[]},\"id\":23}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_paint_voxels(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should be error for empty voxel array");
    TEST_ASSERT_STR_EQ("No voxels to paint", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_open_file_valid() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.open_file\",\"params\":[\"/path/to/model.gox\"],\"id\":30}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_open_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(30, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "opened") != NULL, "Should indicate file was opened");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_open_file_invalid_extension() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.open_file\",\"params\":[\"/path/to/model.txt\"],\"id\":31}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_open_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid extension");
    TEST_ASSERT_STR_EQ("Unsupported file format", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_open_file_empty_path() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.open_file\",\"params\":[\"\"],\"id\":32}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_open_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for empty path");
    TEST_ASSERT_STR_EQ("Invalid file path", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_open_file_no_params() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.open_file\",\"id\":33}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_open_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing params");
    TEST_ASSERT_STR_EQ("Missing file path", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_save_file_valid() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.save_file\",\"params\":[\"/path/to/project.gox\"],\"id\":40}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_save_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(40, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "saved") != NULL, "Should indicate file was saved");
    TEST_ASSERT(strstr(resp->result_json, "/path/to/project.gox") != NULL, "Should include path");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_save_file_invalid_extension() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.save_file\",\"params\":[\"/path/to/project.txt\"],\"id\":41}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_save_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid extension");
    TEST_ASSERT_STR_EQ("Save file must have .gox extension", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_save_file_empty_path() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.save_file\",\"params\":[\"\"],\"id\":42}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_save_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for empty path");
    TEST_ASSERT_STR_EQ("Invalid file path", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_save_file_no_params() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.save_file\",\"id\":43}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_save_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing params");
    TEST_ASSERT_STR_EQ("Missing file path", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_export_file_valid_obj() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.export_file\",\"params\":[\"/path/to/model.obj\",\"obj\"],\"id\":50}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_export_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(50, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "exported") != NULL, "Should indicate file was exported");
    TEST_ASSERT(strstr(resp->result_json, "/path/to/model.obj") != NULL, "Should include path");
    TEST_ASSERT(strstr(resp->result_json, "\"format\":\"obj\"") != NULL, "Should include format");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_export_file_valid_ply() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.export_file\",\"params\":[\"/path/to/model.ply\",\"ply\"],\"id\":51}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_export_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "\"format\":\"ply\"") != NULL, "Should include ply format");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_export_file_invalid_format() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.export_file\",\"params\":[\"/path/to/model.xyz\",\"xyz\"],\"id\":52}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_export_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid format");
    TEST_ASSERT_STR_EQ("Unsupported export format", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_export_file_missing_format() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.export_file\",\"params\":[\"/path/to/model.obj\"],\"id\":53}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_export_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing format");
    TEST_ASSERT_STR_EQ("Missing export format", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_export_file_empty_path() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.export_file\",\"params\":[\"\",\"obj\"],\"id\":54}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_export_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for empty path");
    TEST_ASSERT_STR_EQ("Invalid file path", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_export_file_no_params() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.export_file\",\"id\":55}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_export_file(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing params");
    TEST_ASSERT_STR_EQ("Missing parameters", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_get_voxel_existing() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.get_voxel\",\"params\":{\"position\":[0,0,0]},\"id\":60}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_get_voxel(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(60, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "\"exists\":true") != NULL, "Should indicate voxel exists");
    TEST_ASSERT(strstr(resp->result_json, "\"color\":\"#FF0000\"") != NULL, "Should have color");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_get_voxel_non_existing() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.get_voxel\",\"params\":{\"position\":[1,0,0]},\"id\":61}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_get_voxel(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "\"exists\":false") != NULL, "Should indicate voxel doesn't exist");
    TEST_ASSERT(strstr(resp->result_json, "\"color\":null") != NULL, "Should have null color");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_get_voxel_out_of_bounds() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.get_voxel\",\"params\":{\"position\":[200,0,0]},\"id\":62}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_get_voxel(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for out of bounds");
    TEST_ASSERT_STR_EQ("Position out of bounds", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_get_voxel_invalid_coordinates() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.get_voxel\",\"params\":{\"position\":[\"x\",\"y\",\"z\"]},\"id\":63}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_get_voxel(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid coordinates");
    TEST_ASSERT_STR_EQ("Invalid position coordinates", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_get_voxel_missing_position() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.get_voxel\",\"params\":{},\"id\":64}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_get_voxel(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing position");
    TEST_ASSERT_STR_EQ("Missing position", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_get_voxel_no_params() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.get_voxel\",\"id\":65}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_get_voxel(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing params");
    TEST_ASSERT_STR_EQ("Missing position", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_list_layers() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.list_layers\",\"id\":70}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_list_layers(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(70, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "\"layers\":[") != NULL, "Should have layers array");
    TEST_ASSERT(strstr(resp->result_json, "\"count\":3") != NULL, "Should have 3 layers");
    TEST_ASSERT(strstr(resp->result_json, "\"Layer 1\"") != NULL, "Should have Layer 1");
    TEST_ASSERT(strstr(resp->result_json, "\"Background\"") != NULL, "Should have Background layer");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_create_layer_valid() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_layer\",\"params\":{\"name\":\"New Layer\"},\"id\":71}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_create_layer(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(71, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "\"layer\":{") != NULL, "Should have layer object");
    TEST_ASSERT(strstr(resp->result_json, "\"name\":\"New Layer\"") != NULL, "Should have correct name");
    TEST_ASSERT(strstr(resp->result_json, "\"id\":4") != NULL, "Should have new ID");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_create_layer_empty_name() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_layer\",\"params\":{\"name\":\"\"},\"id\":72}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_create_layer(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for empty name");
    TEST_ASSERT_STR_EQ("Layer name cannot be empty", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_create_layer_duplicate() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_layer\",\"params\":{\"name\":\"Layer 1\"},\"id\":73}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_create_layer(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for duplicate name");
    TEST_ASSERT_STR_EQ("Layer already exists", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_create_layer_long_name() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_layer\",\"params\":{\"name\":\"This is a very long layer name that exceeds the maximum allowed length for layer names\"},\"id\":74}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_create_layer(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for long name");
    TEST_ASSERT_STR_EQ("Layer name too long", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_create_layer_no_params() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.create_layer\",\"id\":75}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_create_layer(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing params");
    TEST_ASSERT_STR_EQ("Missing parameters", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_delete_layer_valid() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.delete_layer\",\"params\":{\"id\":2},\"id\":80}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_delete_layer(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(80, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "\"deleted\":true") != NULL, "Should indicate deletion");
    TEST_ASSERT(strstr(resp->result_json, "\"layer_id\":2") != NULL, "Should include layer ID");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_delete_layer_last() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.delete_layer\",\"params\":{\"id\":1},\"id\":81}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_delete_layer(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail when deleting last layer");
    TEST_ASSERT_STR_EQ("Cannot delete last layer", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_delete_layer_not_found() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.delete_layer\",\"params\":{\"id\":99},\"id\":82}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_delete_layer(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for non-existent layer");
    TEST_ASSERT_STR_EQ("Layer not found", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_delete_layer_invalid_id() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.delete_layer\",\"params\":{\"id\":0},\"id\":83}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_delete_layer(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid ID");
    TEST_ASSERT_STR_EQ("Invalid layer ID", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_delete_layer_no_params() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.delete_layer\",\"id\":84}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_delete_layer(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing params");
    TEST_ASSERT_STR_EQ("Missing layer ID", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_flood_fill_valid() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.flood_fill\",\"params\":{\"position\":[10,10,10],\"color\":\"#FF0000\"},\"id\":90}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_flood_fill(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(90, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "\"filled\":true") != NULL, "Should indicate fill success");
    TEST_ASSERT(strstr(resp->result_json, "\"count\":") != NULL, "Should have count");
    TEST_ASSERT(strstr(resp->result_json, "\"color\":\"#FF0000\"") != NULL, "Should include color");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_flood_fill_missing_position() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.flood_fill\",\"params\":{\"color\":\"#FF0000\"},\"id\":91}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_flood_fill(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing position");
    TEST_ASSERT_STR_EQ("Missing position", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_flood_fill_missing_color() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.flood_fill\",\"params\":{\"position\":[10,10,10]},\"id\":92}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_flood_fill(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing color");
    TEST_ASSERT_STR_EQ("Missing color", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_flood_fill_invalid_color() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.flood_fill\",\"params\":{\"position\":[10,10,10],\"color\":\"red\"},\"id\":93}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_flood_fill(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid color format");
    TEST_ASSERT_STR_EQ("Invalid color format", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_flood_fill_no_params() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.flood_fill\",\"id\":94}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_flood_fill(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing params");
    TEST_ASSERT_STR_EQ("Missing parameters", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_procedural_shape_sphere() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.procedural_shape\",\"params\":{\"shape\":\"sphere\",\"size\":10},\"id\":100}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_procedural_shape(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(100, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "\"created\":true") != NULL, "Should indicate shape created");
    TEST_ASSERT(strstr(resp->result_json, "\"shape\":\"sphere\"") != NULL, "Should include shape type");
    TEST_ASSERT(strstr(resp->result_json, "\"size\":10") != NULL, "Should include size");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_procedural_shape_with_position() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.procedural_shape\",\"params\":{\"shape\":\"cube\",\"size\":20,\"position\":[5,5,5]},\"id\":101}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_procedural_shape(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "\"position\":[5,5,5]") != NULL, "Should include position");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_procedural_shape_invalid_type() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.procedural_shape\",\"params\":{\"shape\":\"triangle\",\"size\":10},\"id\":102}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_procedural_shape(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid shape");
    TEST_ASSERT_STR_EQ("Invalid shape type", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_procedural_shape_missing_shape() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.procedural_shape\",\"params\":{\"size\":10},\"id\":103}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_procedural_shape(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing shape");
    TEST_ASSERT_STR_EQ("Missing shape type", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_procedural_shape_invalid_size() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.procedural_shape\",\"params\":{\"shape\":\"sphere\",\"size\":0},\"id\":104}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_procedural_shape(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid size");
    TEST_ASSERT_STR_EQ("Invalid size", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_procedural_shape_size_too_large() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.procedural_shape\",\"params\":{\"shape\":\"sphere\",\"size\":200},\"id\":105}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_procedural_shape(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for size too large");
    TEST_ASSERT_STR_EQ("Size too large", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_procedural_shape_no_params() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.procedural_shape\",\"id\":106}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_procedural_shape(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing params");
    TEST_ASSERT_STR_EQ("Missing parameters", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_batch_operations_valid() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.batch_operations\",\"params\":{\"operations\":[{\"type\":\"add\",\"position\":[0,0,0],\"color\":\"#FF0000\"},{\"type\":\"remove\",\"position\":[1,1,1]},{\"type\":\"paint\",\"position\":[2,2,2],\"color\":\"#00FF00\"}]},\"id\":110}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_batch_operations(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(110, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "\"completed\":true") != NULL, "Should indicate completion");
    TEST_ASSERT(strstr(resp->result_json, "\"total\":3") != NULL, "Should have 3 operations");
    TEST_ASSERT(strstr(resp->result_json, "\"successful\":3") != NULL, "Should have 3 successful");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_batch_operations_empty() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.batch_operations\",\"params\":{\"operations\":[]},\"id\":111}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_batch_operations(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for empty operations");
    TEST_ASSERT_STR_EQ("Empty operations array", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_batch_operations_invalid_type() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.batch_operations\",\"params\":{\"operations\":[{\"type\":\"invalid\",\"position\":[0,0,0]}]},\"id\":112}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_batch_operations(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid operation type");
    TEST_ASSERT_STR_EQ("Invalid operation type", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_batch_operations_too_many() {
    // Test that handler would fail for too many operations (>1000)
    // We'll just simulate the check that would happen in the real handler
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.batch_operations\",\"params\":{\"operations\":[{\"type\":\"add\",\"position\":[0,0,0]}]},\"id\":113}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    // In a real test, we would pass a JSON with >1000 operations
    // For now, we'll assume the handler has logic to check operation count
    // and would return an error for >1000 operations
    
    TEST_ASSERT(req != NULL, "Should parse request");
    
    // Free resources
    free_jsonrpc_request(req);
    
    // TODO: Implement proper test when handler supports operation count limits
    return 1;
}

int test_handle_batch_operations_no_params() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.batch_operations\",\"id\":114}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_batch_operations(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing params");
    TEST_ASSERT_STR_EQ("Missing parameters", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_render_scene_valid() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.render_scene\",\"params\":{\"width\":800,\"height\":600},\"id\":120}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_render_scene(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT_EQ(120, resp->id);
    TEST_ASSERT(strstr(resp->result_json, "\"rendered\":true") != NULL, "Should indicate render success");
    TEST_ASSERT(strstr(resp->result_json, "\"width\":800") != NULL, "Should include width");
    TEST_ASSERT(strstr(resp->result_json, "\"height\":600") != NULL, "Should include height");
    TEST_ASSERT(strstr(resp->result_json, "\"format\":\"png\"") != NULL, "Should default to png");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_render_scene_with_format() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.render_scene\",\"params\":{\"width\":1024,\"height\":768,\"format\":\"jpg\"},\"id\":121}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_render_scene(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == true, "Should be successful");
    TEST_ASSERT(strstr(resp->result_json, "\"format\":\"jpg\"") != NULL, "Should use specified format");
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_render_scene_invalid_dimensions() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.render_scene\",\"params\":{\"width\":0,\"height\":600},\"id\":122}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_render_scene(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid dimensions");
    TEST_ASSERT_STR_EQ("Invalid dimensions", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_render_scene_too_large() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.render_scene\",\"params\":{\"width\":5000,\"height\":5000},\"id\":123}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_render_scene(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for dimensions too large");
    TEST_ASSERT_STR_EQ("Dimensions too large", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_render_scene_invalid_format() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.render_scene\",\"params\":{\"width\":800,\"height\":600,\"format\":\"tiff\"},\"id\":124}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_render_scene(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for invalid format");
    TEST_ASSERT_STR_EQ("Invalid image format", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int test_handle_render_scene_no_params() {
    const char* json = "{\"jsonrpc\":\"2.0\",\"method\":\"goxel.render_scene\",\"id\":125}";
    jsonrpc_request_t* req = parse_jsonrpc_request(json);
    
    jsonrpc_response_t* resp = handle_render_scene(req);
    TEST_ASSERT(resp != NULL, "Should get response");
    TEST_ASSERT(resp->success == false, "Should fail for missing params");
    TEST_ASSERT_STR_EQ("Missing parameters", resp->error_message);
    
    free_jsonrpc_response(resp);
    free_jsonrpc_request(req);
    return 1;
}

int main() {
    TEST_SUITE_BEGIN();
    
    RUN_TEST(test_parse_valid_request);
    RUN_TEST(test_parse_null_request);
    RUN_TEST(test_create_success_response);
    RUN_TEST(test_create_error_response);
    RUN_TEST(test_serialize_success_response);
    RUN_TEST(test_serialize_error_response);
    RUN_TEST(test_handle_create_project_valid);
    RUN_TEST(test_handle_create_project_wrong_method);
    RUN_TEST(test_handle_add_voxels_single);
    RUN_TEST(test_handle_add_voxels_multiple);
    RUN_TEST(test_handle_add_voxels_empty_array);
    RUN_TEST(test_handle_remove_voxels_single);
    RUN_TEST(test_handle_remove_voxels_multiple);
    RUN_TEST(test_handle_remove_voxels_empty);
    RUN_TEST(test_handle_remove_voxels_invalid_method);
    RUN_TEST(test_handle_paint_voxels_single);
    RUN_TEST(test_handle_paint_voxels_gradient);
    RUN_TEST(test_handle_paint_voxels_no_color);
    RUN_TEST(test_handle_paint_voxels_empty);
    RUN_TEST(test_handle_open_file_valid);
    RUN_TEST(test_handle_open_file_invalid_extension);
    RUN_TEST(test_handle_open_file_empty_path);
    RUN_TEST(test_handle_open_file_no_params);
    RUN_TEST(test_handle_save_file_valid);
    RUN_TEST(test_handle_save_file_invalid_extension);
    RUN_TEST(test_handle_save_file_empty_path);
    RUN_TEST(test_handle_save_file_no_params);
    RUN_TEST(test_handle_export_file_valid_obj);
    RUN_TEST(test_handle_export_file_valid_ply);
    RUN_TEST(test_handle_export_file_invalid_format);
    RUN_TEST(test_handle_export_file_missing_format);
    RUN_TEST(test_handle_export_file_empty_path);
    RUN_TEST(test_handle_export_file_no_params);
    RUN_TEST(test_handle_get_voxel_existing);
    RUN_TEST(test_handle_get_voxel_non_existing);
    RUN_TEST(test_handle_get_voxel_out_of_bounds);
    RUN_TEST(test_handle_get_voxel_invalid_coordinates);
    RUN_TEST(test_handle_get_voxel_missing_position);
    RUN_TEST(test_handle_get_voxel_no_params);
    RUN_TEST(test_handle_list_layers);
    RUN_TEST(test_handle_create_layer_valid);
    RUN_TEST(test_handle_create_layer_empty_name);
    RUN_TEST(test_handle_create_layer_duplicate);
    RUN_TEST(test_handle_create_layer_long_name);
    RUN_TEST(test_handle_create_layer_no_params);
    RUN_TEST(test_handle_delete_layer_valid);
    RUN_TEST(test_handle_delete_layer_last);
    RUN_TEST(test_handle_delete_layer_not_found);
    RUN_TEST(test_handle_delete_layer_invalid_id);
    RUN_TEST(test_handle_delete_layer_no_params);
    RUN_TEST(test_handle_flood_fill_valid);
    RUN_TEST(test_handle_flood_fill_missing_position);
    RUN_TEST(test_handle_flood_fill_missing_color);
    RUN_TEST(test_handle_flood_fill_invalid_color);
    RUN_TEST(test_handle_flood_fill_no_params);
    RUN_TEST(test_handle_procedural_shape_sphere);
    RUN_TEST(test_handle_procedural_shape_with_position);
    RUN_TEST(test_handle_procedural_shape_invalid_type);
    RUN_TEST(test_handle_procedural_shape_missing_shape);
    RUN_TEST(test_handle_procedural_shape_invalid_size);
    RUN_TEST(test_handle_procedural_shape_size_too_large);
    RUN_TEST(test_handle_procedural_shape_no_params);
    
    TEST_SUITE_END();
    
    return g_test_stats.failed_tests > 0 ? 1 : 0;
}