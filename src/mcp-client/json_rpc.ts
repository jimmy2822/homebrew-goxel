/**
 * Goxel v14.0 JSON-RPC 2.0 Protocol Handler
 * 
 * This module provides a dedicated JSON-RPC 2.0 protocol implementation
 * with proper request/response correlation, error handling, and validation.
 * 
 * Specifications: https://www.jsonrpc.org/specification
 * 
 * Key features:
 * - Full JSON-RPC 2.0 compliance
 * - Request ID generation and correlation
 * - Batch request support
 * - Error code standardization
 * - Message validation
 * - Type-safe interfaces
 */

import {
  JsonRpcRequest,
  JsonRpcResponse,
  JsonRpcNotification,
  JsonRpcSuccessResponse,
  JsonRpcErrorResponse,
  JsonRpcError,
  JsonRpcId,
  JsonRpcParams,
  JsonRpcErrorCode,
  isJsonRpcSuccessResponse,
  isJsonRpcErrorResponse,
} from './types';

/**
 * JSON-RPC Protocol Handler
 * 
 * Handles the creation, validation, and parsing of JSON-RPC 2.0 messages.
 * This class provides a clean abstraction for JSON-RPC protocol operations.
 */
export class JsonRpcProtocolHandler {
  private requestIdCounter = 0;
  private readonly maxIdValue = Number.MAX_SAFE_INTEGER;

  /**
   * Creates a JSON-RPC request with auto-generated ID
   * 
   * @param method - Method name to call
   * @param params - Method parameters (optional)
   * @param id - Custom request ID (auto-generated if not provided)
   * @returns Formatted JSON-RPC request
   */
  public createRequest(
    method: string,
    params?: JsonRpcParams,
    id?: JsonRpcId
  ): JsonRpcRequest {
    // Validate method name
    if (!method || typeof method !== 'string') {
      throw new Error('Method name must be a non-empty string');
    }

    // Generate ID if not provided
    const requestId = id !== undefined ? id : this.generateRequestId();

    const request: JsonRpcRequest = {
      jsonrpc: '2.0',
      method,
      id: requestId,
    };

    // Only include params if provided
    if (params !== undefined) {
      request.params = params;
    }

    return request;
  }

  /**
   * Creates a JSON-RPC notification (no response expected)
   * 
   * @param method - Method name to call
   * @param params - Method parameters (optional)
   * @returns Formatted JSON-RPC notification
   */
  public createNotification(
    method: string,
    params?: JsonRpcParams
  ): JsonRpcNotification {
    // Validate method name
    if (!method || typeof method !== 'string') {
      throw new Error('Method name must be a non-empty string');
    }

    const notification: JsonRpcNotification = {
      jsonrpc: '2.0',
      method,
    };

    // Only include params if provided
    if (params !== undefined) {
      notification.params = params;
    }

    return notification;
  }

  /**
   * Creates a successful JSON-RPC response
   * 
   * @param id - Request ID to respond to
   * @param result - Result data
   * @returns Formatted JSON-RPC success response
   */
  public createSuccessResponse(
    id: JsonRpcId,
    result: unknown
  ): JsonRpcSuccessResponse {
    return {
      jsonrpc: '2.0',
      result,
      id,
    };
  }

  /**
   * Creates an error JSON-RPC response
   * 
   * @param id - Request ID to respond to
   * @param code - Error code
   * @param message - Error message
   * @param data - Additional error data (optional)
   * @returns Formatted JSON-RPC error response
   */
  public createErrorResponse(
    id: JsonRpcId,
    code: number,
    message: string,
    data?: unknown
  ): JsonRpcErrorResponse {
    const error: JsonRpcError = {
      code,
      message,
    };

    if (data !== undefined) {
      error.data = data;
    }

    return {
      jsonrpc: '2.0',
      error,
      id,
    };
  }

  /**
   * Creates a batch request from multiple requests
   * 
   * @param requests - Array of requests/notifications
   * @returns JSON string of batch request
   */
  public createBatchRequest(
    requests: Array<JsonRpcRequest | JsonRpcNotification>
  ): string {
    if (!Array.isArray(requests) || requests.length === 0) {
      throw new Error('Batch request must contain at least one request');
    }

    return JSON.stringify(requests);
  }

  /**
   * Validates a JSON-RPC request
   * 
   * @param request - Request to validate
   * @returns Validation result with error details if invalid
   */
  public validateRequest(
    request: unknown
  ): { valid: boolean; error?: JsonRpcError } {
    // Check if request is an object
    if (typeof request !== 'object' || request === null) {
      return {
        valid: false,
        error: {
          code: JsonRpcErrorCode.INVALID_REQUEST,
          message: 'Request must be an object',
        },
      };
    }

    const req = request as any;

    // Check JSON-RPC version
    if (req.jsonrpc !== '2.0') {
      return {
        valid: false,
        error: {
          code: JsonRpcErrorCode.INVALID_REQUEST,
          message: 'Invalid JSON-RPC version',
        },
      };
    }

    // Check method
    if (typeof req.method !== 'string' || req.method.length === 0) {
      return {
        valid: false,
        error: {
          code: JsonRpcErrorCode.INVALID_REQUEST,
          message: 'Method must be a non-empty string',
        },
      };
    }

    // Check params if present
    if (req.params !== undefined) {
      if (typeof req.params !== 'object') {
        return {
          valid: false,
          error: {
            code: JsonRpcErrorCode.INVALID_PARAMS,
            message: 'Params must be an object or array',
          },
        };
      }
    }

    // Check ID if present (notifications don't have ID)
    if ('id' in req) {
      const validIdTypes = ['string', 'number'];
      if (req.id !== null && !validIdTypes.includes(typeof req.id)) {
        return {
          valid: false,
          error: {
            code: JsonRpcErrorCode.INVALID_REQUEST,
            message: 'ID must be a string, number, or null',
          },
        };
      }
    }

    return { valid: true };
  }

  /**
   * Validates a JSON-RPC response
   * 
   * @param response - Response to validate
   * @returns Validation result with error details if invalid
   */
  public validateResponse(
    response: unknown
  ): { valid: boolean; error?: string } {
    // Check if response is an object
    if (typeof response !== 'object' || response === null) {
      return {
        valid: false,
        error: 'Response must be an object',
      };
    }

    const res = response as any;

    // Check JSON-RPC version
    if (res.jsonrpc !== '2.0') {
      return {
        valid: false,
        error: 'Invalid JSON-RPC version',
      };
    }

    // Must have either result or error, but not both
    const hasResult = 'result' in res;
    const hasError = 'error' in res;

    if (hasResult === hasError) {
      return {
        valid: false,
        error: 'Response must have either result or error, but not both',
      };
    }

    // Check error structure if present
    if (hasError) {
      if (typeof res.error !== 'object' || res.error === null) {
        return {
          valid: false,
          error: 'Error must be an object',
        };
      }

      if (typeof res.error.code !== 'number') {
        return {
          valid: false,
          error: 'Error code must be a number',
        };
      }

      if (typeof res.error.message !== 'string') {
        return {
          valid: false,
          error: 'Error message must be a string',
        };
      }
    }

    // Check ID
    if (!('id' in res)) {
      return {
        valid: false,
        error: 'Response must have an ID',
      };
    }

    return { valid: true };
  }

  /**
   * Parses a JSON string into JSON-RPC message(s)
   * 
   * @param data - JSON string to parse
   * @returns Parsed message(s) or error
   */
  public parseMessage(
    data: string
  ): { 
    success: true; 
    message: JsonRpcRequest | JsonRpcResponse | JsonRpcNotification | Array<any>;
  } | { 
    success: false; 
    error: JsonRpcError;
  } {
    try {
      const parsed = JSON.parse(data);
      return { success: true, message: parsed };
    } catch (error) {
      return {
        success: false,
        error: {
          code: JsonRpcErrorCode.PARSE_ERROR,
          message: 'Parse error',
          data: error instanceof Error ? error.message : String(error),
        },
      };
    }
  }

  /**
   * Serializes a JSON-RPC message to string
   * 
   * @param message - Message to serialize
   * @returns JSON string
   */
  public serializeMessage(
    message: JsonRpcRequest | JsonRpcResponse | JsonRpcNotification | Array<any>
  ): string {
    return JSON.stringify(message);
  }

  /**
   * Generates a unique request ID
   * 
   * @returns Unique numeric ID
   */
  private generateRequestId(): number {
    // Reset counter if approaching max value
    if (this.requestIdCounter >= this.maxIdValue - 1000) {
      this.requestIdCounter = 0;
    }
    
    return ++this.requestIdCounter;
  }

  /**
   * Creates standard JSON-RPC error for common scenarios
   * 
   * @param type - Error type
   * @param data - Additional error data
   * @returns Formatted error object
   */
  public createStandardError(
    type: 'parse' | 'invalid_request' | 'method_not_found' | 'invalid_params' | 'internal',
    data?: unknown
  ): JsonRpcError {
    const errorMap = {
      parse: {
        code: JsonRpcErrorCode.PARSE_ERROR,
        message: 'Parse error',
      },
      invalid_request: {
        code: JsonRpcErrorCode.INVALID_REQUEST,
        message: 'Invalid Request',
      },
      method_not_found: {
        code: JsonRpcErrorCode.METHOD_NOT_FOUND,
        message: 'Method not found',
      },
      invalid_params: {
        code: JsonRpcErrorCode.INVALID_PARAMS,
        message: 'Invalid params',
      },
      internal: {
        code: JsonRpcErrorCode.INTERNAL_ERROR,
        message: 'Internal error',
      },
    };

    const error = errorMap[type];
    return data !== undefined ? { ...error, data } : error;
  }

  /**
   * Checks if a message is a request
   * 
   * @param message - Message to check
   * @returns True if message is a request
   */
  public isRequest(message: any): message is JsonRpcRequest {
    return (
      message &&
      typeof message === 'object' &&
      message.jsonrpc === '2.0' &&
      typeof message.method === 'string' &&
      'id' in message
    );
  }

  /**
   * Checks if a message is a notification
   * 
   * @param message - Message to check
   * @returns True if message is a notification
   */
  public isNotification(message: any): message is JsonRpcNotification {
    return (
      message &&
      typeof message === 'object' &&
      message.jsonrpc === '2.0' &&
      typeof message.method === 'string' &&
      !('id' in message)
    );
  }

  /**
   * Checks if a message is a response
   * 
   * @param message - Message to check
   * @returns True if message is a response
   */
  public isResponse(message: any): message is JsonRpcResponse {
    return (
      message &&
      typeof message === 'object' &&
      message.jsonrpc === '2.0' &&
      'id' in message &&
      ('result' in message || 'error' in message)
    );
  }

  /**
   * Extracts method name from request or notification
   * 
   * @param message - Request or notification
   * @returns Method name or undefined
   */
  public getMethod(
    message: JsonRpcRequest | JsonRpcNotification
  ): string | undefined {
    return message.method;
  }

  /**
   * Extracts request ID from request or response
   * 
   * @param message - Request or response
   * @returns Request ID or undefined
   */
  public getId(
    message: JsonRpcRequest | JsonRpcResponse
  ): JsonRpcId | undefined {
    return 'id' in message ? message.id : undefined;
  }

  /**
   * Reset the request ID counter
   */
  public resetIdCounter(): void {
    this.requestIdCounter = 0;
  }
}

// Export singleton instance for convenience
export const jsonRpcProtocol = new JsonRpcProtocolHandler();