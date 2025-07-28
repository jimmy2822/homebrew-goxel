/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TEST_METHODS_H
#define TEST_METHODS_H

#include "json_rpc.h"

/**
 * Test method registry entry
 */
typedef struct {
    const char *name;
    json_rpc_response_t *(*handler)(const json_rpc_request_t *request);
    const char *description;
} test_method_entry_t;

/**
 * Get the test methods registry
 * @param count Pointer to store the number of methods (optional)
 * @return Array of test method entries
 */
const test_method_entry_t *get_test_methods(size_t *count);

/**
 * Handle a test method by name
 * @param method_name The method name to handle
 * @param request The JSON-RPC request
 * @return Response or NULL if method not found
 */
json_rpc_response_t *handle_test_method(const char *method_name, 
                                       const json_rpc_request_t *request);

// Individual method handlers
json_rpc_response_t *handle_echo(const json_rpc_request_t *request);
json_rpc_response_t *handle_version(const json_rpc_request_t *request);
json_rpc_response_t *handle_status(const json_rpc_request_t *request);
json_rpc_response_t *handle_ping(const json_rpc_request_t *request);
json_rpc_response_t *handle_list_methods(const json_rpc_request_t *request);
json_rpc_response_t *handle_add_voxels(const json_rpc_request_t *request);

// JSON utility functions
json_value *json_value_clone(const json_value *src);
unsigned int json_array_length(const json_value *array);
json_value *json_array_get(const json_value *array, unsigned int index);
unsigned int json_object_length(const json_value *object);
const char *json_object_get_key(const json_value *object, unsigned int index);
json_value *json_object_get_value(const json_value *object, unsigned int index);
json_value *json_object_get(const json_value *object, const char *key);

#endif // TEST_METHODS_H