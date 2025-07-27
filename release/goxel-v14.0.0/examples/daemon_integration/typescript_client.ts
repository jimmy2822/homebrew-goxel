/**
 * Goxel v14.0 TypeScript Client Example
 * Demonstrates direct JSON-RPC communication with the daemon
 */

import * as net from 'net';
import * as fs from 'fs';

interface JsonRpcRequest {
    jsonrpc: '2.0';
    method: string;
    params?: any;
    id: number | string;
}

interface JsonRpcResponse {
    jsonrpc: '2.0';
    result?: any;
    error?: {
        code: number;
        message: string;
        data?: any;
    };
    id: number | string;
}

class GoxelDaemonClient {
    private socket: net.Socket | null = null;
    private requestId = 0;
    private pendingRequests = new Map<number, {
        resolve: (value: any) => void;
        reject: (error: any) => void;
    }>();
    private buffer = '';

    constructor(private socketPath: string = '/var/run/goxel/goxel.sock') {}

    /**
     * Connect to the Goxel daemon
     */
    async connect(): Promise<void> {
        return new Promise((resolve, reject) => {
            this.socket = net.createConnection(this.socketPath);
            
            this.socket.on('connect', () => {
                console.log('Connected to Goxel daemon');
                resolve();
            });

            this.socket.on('data', (data) => {
                this.handleData(data.toString());
            });

            this.socket.on('error', (error) => {
                reject(error);
            });

            this.socket.on('close', () => {
                console.log('Disconnected from Goxel daemon');
                this.socket = null;
            });
        });
    }

    /**
     * Send a JSON-RPC request
     */
    async request(method: string, params?: any): Promise<any> {
        if (!this.socket) {
            throw new Error('Not connected to daemon');
        }

        const id = ++this.requestId;
        const request: JsonRpcRequest = {
            jsonrpc: '2.0',
            method,
            params,
            id
        };

        return new Promise((resolve, reject) => {
            this.pendingRequests.set(id, { resolve, reject });
            this.socket!.write(JSON.stringify(request) + '\n');
        });
    }

    /**
     * Handle incoming data from the daemon
     */
    private handleData(data: string) {
        this.buffer += data;
        const lines = this.buffer.split('\n');
        this.buffer = lines.pop() || '';

        for (const line of lines) {
            if (line.trim()) {
                try {
                    const response: JsonRpcResponse = JSON.parse(line);
                    this.handleResponse(response);
                } catch (error) {
                    console.error('Failed to parse response:', line);
                }
            }
        }
    }

    /**
     * Handle a JSON-RPC response
     */
    private handleResponse(response: JsonRpcResponse) {
        const pending = this.pendingRequests.get(response.id as number);
        if (!pending) return;

        this.pendingRequests.delete(response.id as number);

        if (response.error) {
            pending.reject(new Error(response.error.message));
        } else {
            pending.resolve(response.result);
        }
    }

    /**
     * Disconnect from the daemon
     */
    disconnect() {
        if (this.socket) {
            this.socket.end();
        }
    }
}

// Example usage
async function main() {
    const client = new GoxelDaemonClient();

    try {
        // Connect to daemon
        await client.connect();

        // Get daemon status
        const status = await client.request('daemon.status');
        console.log('Daemon status:', status);

        // Create a new project
        await client.request('project.create', {
            path: 'typescript_test.gox'
        });
        console.log('Created project');

        // Add some voxels
        const colors = [
            [255, 0, 0],    // Red
            [0, 255, 0],    // Green
            [0, 0, 255],    // Blue
            [255, 255, 0],  // Yellow
            [255, 0, 255],  // Magenta
        ];

        for (let i = 0; i < colors.length; i++) {
            await client.request('voxel.add', {
                x: i * 2,
                y: 0,
                z: 0,
                r: colors[i][0],
                g: colors[i][1],
                b: colors[i][2],
                a: 255
            });
        }
        console.log('Added voxels');

        // Create a sphere
        await client.request('shape.sphere', {
            center: [10, 10, 10],
            radius: 5,
            color: [255, 128, 0, 255]
        });
        console.log('Created sphere');

        // Add a new layer
        await client.request('layer.add', {
            name: 'Second Layer'
        });
        
        // Switch to the new layer
        await client.request('layer.select', {
            index: 1
        });

        // Add voxels to the second layer
        for (let i = 0; i < 10; i++) {
            await client.request('voxel.add', {
                x: i,
                y: 5,
                z: 0,
                r: 0,
                g: 255,
                b: 255,
                a: 255
            });
        }
        console.log('Added voxels to second layer');

        // Export as OBJ
        await client.request('export.obj', {
            path: 'typescript_test.obj'
        });
        console.log('Exported as OBJ');

        // Save the project
        await client.request('project.save');
        console.log('Saved project');

        // Get statistics
        const stats = await client.request('daemon.stats');
        console.log('Daemon statistics:', JSON.stringify(stats, null, 2));

    } catch (error) {
        console.error('Error:', error);
    } finally {
        // Disconnect
        client.disconnect();
    }
}

// Run the example
main().catch(console.error);

// Export the client class for use in other projects
export { GoxelDaemonClient, JsonRpcRequest, JsonRpcResponse };