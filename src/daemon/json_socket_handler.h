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

/**
 * @file json_socket_handler.h
 * @brief JSON-specific socket handling for the daemon.
 *
 * This module provides JSON-over-socket communication handling,
 * where JSON messages are delimited by newlines instead of using
 * the binary protocol with headers.
 */

#ifndef JSON_SOCKET_HANDLER_H
#define JSON_SOCKET_HANDLER_H

#include "socket_server.h"
#include "json_rpc.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set the message handler for JSON socket processing.
 * This must be called before any clients connect.
 *
 * @param handler The message handler function
 * @param user_data User data to pass to the handler
 */
void json_socket_set_handler(socket_message_handler_t handler, void *user_data);

/**
 * Socket client handler for JSON mode.
 * This handler starts a monitoring thread for each client that reads
 * newline-delimited JSON messages.
 *
 * @param server The socket server
 * @param client The client that connected/disconnected
 * @param connected True if connected, false if disconnected
 * @param user_data User data passed to the handler
 */
void json_socket_client_handler(socket_server_t *server,
                               socket_client_t *client,
                               bool connected,
                               void *user_data);

/**
 * Create a socket message from a JSON-RPC response.
 *
 * @param response The JSON-RPC response
 * @param request_id The request ID to use for the socket message
 * @return A new socket message, or NULL on error
 */
socket_message_t *json_rpc_create_socket_response(json_rpc_response_t *response,
                                                 uint32_t request_id);

#ifdef __cplusplus
}
#endif

#endif // JSON_SOCKET_HANDLER_H