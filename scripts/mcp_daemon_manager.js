/**
 * Goxel MCP Daemon Manager
 * 
 * Manages Goxel daemon lifecycle for MCP server integration.
 * Provides auto-start, health monitoring, and graceful shutdown capabilities.
 * 
 * @author Agent-5 (David Kim) - DevOps Engineer
 * @version 14.0.0-daemon
 */

const { spawn, exec } = require('child_process');
const fs = require('fs').promises;
const path = require('path');
const os = require('os');
const EventEmitter = require('events');

/**
 * Daemon configuration interface
 */
class DaemonConfig {
    constructor(options = {}) {
        // Environment variable overrides
        this.daemonBinary = options.daemonBinary || 
                           process.env.GOXEL_DAEMON_BINARY || 
                           this.getDefaultBinaryPath();
        
        this.socketPath = options.socketPath || 
                         process.env.GOXEL_SOCKET_PATH || 
                         this.getDefaultSocketPath();
        
        this.pidFile = options.pidFile || 
                      process.env.GOXEL_PID_FILE || 
                      this.getDefaultPidPath();
        
        this.logFile = options.logFile || 
                      process.env.GOXEL_LOG_FILE || 
                      this.getDefaultLogPath();
        
        // Server configuration
        this.workers = parseInt(options.workers || process.env.GOXEL_MAX_WORKERS || '4');
        this.queueSize = parseInt(options.queueSize || process.env.GOXEL_QUEUE_SIZE || '1024');
        this.maxConnections = parseInt(options.maxConnections || process.env.GOXEL_MAX_CONNECTIONS || '256');
        
        // Timeout configuration
        this.startupTimeoutMs = parseInt(options.startupTimeoutMs || process.env.GOXEL_STARTUP_TIMEOUT || '30000');
        this.shutdownTimeoutMs = parseInt(options.shutdownTimeoutMs || process.env.GOXEL_SHUTDOWN_TIMEOUT || '10000');
        this.healthCheckTimeoutMs = parseInt(options.healthCheckTimeoutMs || process.env.GOXEL_HEALTH_TIMEOUT || '5000');
        
        // Health monitoring
        this.healthCheckIntervalMs = parseInt(options.healthCheckIntervalMs || '30000');
        this.maxRestartAttempts = parseInt(options.maxRestartAttempts || '5');
        this.restartDelayMs = parseInt(options.restartDelayMs || '5000');
        
        // Auto-start behavior
        this.autoStart = options.autoStart !== undefined ? options.autoStart : true;
        this.restartOnFailure = options.restartOnFailure !== undefined ? options.restartOnFailure : true;
        
        // Logging
        this.verbose = options.verbose || process.env.GOXEL_VERBOSE === '1' || false;
        this.enableStructuredLogging = options.enableStructuredLogging !== undefined ? options.enableStructuredLogging : true;
    }
    
    getDefaultBinaryPath() {
        const platform = os.platform();
        switch (platform) {
            case 'darwin':
                return '/usr/local/bin/goxel-daemon';
            case 'linux':
                return '/usr/bin/goxel-daemon';
            case 'win32':
                return 'C:\\Program Files\\Goxel\\goxel-daemon.exe';
            default:
                return './goxel-daemon';
        }
    }
    
    getDefaultSocketPath() {
        const platform = os.platform();
        switch (platform) {
            case 'darwin':
                return '/usr/local/var/run/goxel-daemon.sock';
            case 'linux':
                return '/var/run/goxel-daemon.sock';
            case 'win32':
                return '\\\\.\\pipe\\goxel-daemon';
            default:
                return '/tmp/goxel-daemon.sock';
        }
    }
    
    getDefaultPidPath() {
        const platform = os.platform();
        switch (platform) {
            case 'darwin':
                return '/usr/local/var/run/goxel-daemon.pid';
            case 'linux':
                return '/var/run/goxel-daemon.pid';
            case 'win32':
                return 'C:\\ProgramData\\Goxel\\goxel-daemon.pid';
            default:
                return '/tmp/goxel-daemon.pid';
        }
    }
    
    getDefaultLogPath() {
        const platform = os.platform();
        switch (platform) {
            case 'darwin':
                return '/usr/local/var/log/goxel-daemon.log';
            case 'linux':
                return '/var/log/goxel-daemon.log';
            case 'win32':
                return 'C:\\ProgramData\\Goxel\\goxel-daemon.log';
            default:
                return '/tmp/goxel-daemon.log';
        }
    }
}

/**
 * Daemon manager with health monitoring and auto-restart
 */
class GoxelDaemonManager extends EventEmitter {
    constructor(config = {}) {
        super();
        this.config = new DaemonConfig(config);
        this.process = null;
        this.pid = null;
        this.isRunning = false;
        this.restartAttempts = 0;
        this.healthCheckTimer = null;
        this.startPromise = null;
        
        // Setup graceful shutdown
        process.on('SIGINT', () => this.shutdown());
        process.on('SIGTERM', () => this.shutdown());
        process.on('exit', () => this.shutdown());
    }
    
    /**
     * Start the daemon with auto-start capability
     */
    async start() {
        if (this.startPromise) {
            return this.startPromise;
        }
        
        this.startPromise = this._doStart();
        return this.startPromise;
    }
    
    async _doStart() {
        this.log('info', 'Starting Goxel daemon...', {
            binary: this.config.daemonBinary,
            socket: this.config.socketPath,
            workers: this.config.workers
        });
        
        try {
            // Check if daemon is already running
            if (await this.isAlreadyRunning()) {
                this.log('info', 'Daemon is already running');
                await this.attachToRunningDaemon();
                return;
            }
            
            // Ensure directories exist
            await this.ensureDirectoriesExist();
            
            // Start the daemon process
            await this.spawnDaemon();
            
            // Wait for daemon to be ready
            await this.waitForReady();
            
            // Start health monitoring
            this.startHealthMonitoring();
            
            this.isRunning = true;
            this.restartAttempts = 0;
            
            this.log('info', 'Daemon started successfully', {
                pid: this.pid,
                socket: this.config.socketPath
            });
            
            this.emit('started', { pid: this.pid });
            
        } catch (error) {
            this.log('error', 'Failed to start daemon', { error: error.message });
            this.emit('error', error);
            throw error;
        } finally {
            this.startPromise = null;
        }
    }
    
    /**
     * Stop the daemon gracefully
     */
    async stop() {
        this.log('info', 'Stopping Goxel daemon...');
        
        try {
            this.stopHealthMonitoring();
            
            if (this.process) {
                // Send SIGTERM for graceful shutdown
                this.process.kill('SIGTERM');
                
                // Wait for graceful shutdown
                await this.waitForShutdown();
            } else if (this.pid) {
                // Kill external process
                process.kill(this.pid, 'SIGTERM');
                await this.waitForShutdown();
            }
            
            this.cleanup();
            this.log('info', 'Daemon stopped successfully');
            this.emit('stopped');
            
        } catch (error) {
            this.log('error', 'Error stopping daemon', { error: error.message });
            this.emit('error', error);
            throw error;
        }
    }
    
    /**
     * Restart the daemon
     */
    async restart() {
        this.log('info', 'Restarting Goxel daemon...');
        await this.stop();
        await new Promise(resolve => setTimeout(resolve, 2000)); // Brief delay
        await this.start();
    }
    
    /**
     * Graceful shutdown for process exit
     */
    async shutdown() {
        if (this.isRunning) {
            try {
                await this.stop();
            } catch (error) {
                this.log('error', 'Error during shutdown', { error: error.message });
            }
        }
    }
    
    /**
     * Get daemon status and health information
     */
    async getStatus() {
        const isRunning = await this.isAlreadyRunning();
        const pid = await this.getCurrentPid();
        
        const status = {
            running: isRunning,
            pid: pid,
            socket: this.config.socketPath,
            pidFile: this.config.pidFile,
            logFile: this.config.logFile,
            restartAttempts: this.restartAttempts,
            healthCheckActive: !!this.healthCheckTimer
        };
        
        if (isRunning) {
            try {
                status.health = await this.performHealthCheck();
            } catch (error) {
                status.health = { status: 'unhealthy', error: error.message };
            }
        }
        
        return status;
    }
    
    /**
     * Perform health check on running daemon
     */
    async performHealthCheck() {
        return new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                reject(new Error('Health check timeout'));
            }, this.config.healthCheckTimeoutMs);
            
            // Simple socket connection test
            const net = require('net');
            const client = net.createConnection(this.config.socketPath);
            
            client.on('connect', () => {
                clearTimeout(timeout);
                client.end();
                resolve({ 
                    status: 'healthy', 
                    timestamp: new Date().toISOString(),
                    latency: Date.now() % 1000 // Mock latency
                });
            });
            
            client.on('error', (error) => {
                clearTimeout(timeout);
                reject(error);
            });
        });
    }
    
    /**
     * Check if daemon is already running
     */
    async isAlreadyRunning() {
        try {
            const pid = await this.getCurrentPid();
            if (pid) {
                // Check if process is actually running
                try {
                    process.kill(pid, 0); // Signal 0 doesn't kill, just checks if process exists
                    return true;
                } catch (error) {
                    // Process doesn't exist, clean up stale PID file
                    await this.cleanupStalePidFile();
                    return false;
                }
            }
            return false;
        } catch (error) {
            return false;
        }
    }
    
    /**
     * Get current PID from PID file
     */
    async getCurrentPid() {
        try {
            const pidContent = await fs.readFile(this.config.pidFile, 'utf8');
            const pid = parseInt(pidContent.trim());
            return isNaN(pid) ? null : pid;
        } catch (error) {
            return null;
        }
    }
    
    /**
     * Attach to already running daemon
     */
    async attachToRunningDaemon() {
        this.pid = await this.getCurrentPid();
        this.isRunning = true;
        this.startHealthMonitoring();
        this.emit('attached', { pid: this.pid });
    }
    
    /**
     * Ensure required directories exist
     */
    async ensureDirectoriesExist() {
        const dirs = [
            path.dirname(this.config.pidFile),
            path.dirname(this.config.logFile),
            path.dirname(this.config.socketPath)
        ];
        
        for (const dir of dirs) {
            try {
                await fs.mkdir(dir, { recursive: true });
            } catch (error) {
                if (error.code !== 'EEXIST') {
                    throw error;
                }
            }
        }
    }
    
    /**
     * Spawn the daemon process
     */
    async spawnDaemon() {
        const args = [
            '--daemonize',
            '--pid-file', this.config.pidFile,
            '--socket', this.config.socketPath,
            '--log-file', this.config.logFile,
            '--workers', this.config.workers.toString(),
            '--queue-size', this.config.queueSize.toString(),
            '--max-connections', this.config.maxConnections.toString()
        ];
        
        if (this.config.verbose) {
            args.push('--verbose');
        }
        
        this.log('debug', 'Spawning daemon', { 
            binary: this.config.daemonBinary, 
            args: args 
        });
        
        return new Promise((resolve, reject) => {
            this.process = spawn(this.config.daemonBinary, args, {
                stdio: 'pipe',
                detached: false
            });
            
            this.process.on('error', (error) => {
                reject(new Error(`Failed to spawn daemon: ${error.message}`));
            });
            
            this.process.on('exit', (code, signal) => {
                if (code !== 0 && signal !== 'SIGTERM') {
                    reject(new Error(`Daemon exited with code ${code}, signal ${signal}`));
                } else {
                    this.handleDaemonExit(code, signal);
                }
            });
            
            // For daemonized process, we expect it to exit immediately after forking
            setTimeout(resolve, 1000);
        });
    }
    
    /**
     * Wait for daemon to be ready
     */
    async waitForReady() {
        const startTime = Date.now();
        const timeout = this.config.startupTimeoutMs;
        
        while (Date.now() - startTime < timeout) {
            try {
                // Check if PID file exists and contains valid PID
                this.pid = await this.getCurrentPid();
                if (this.pid) {
                    // Check if socket is ready
                    await this.performHealthCheck();
                    return;
                }
            } catch (error) {
                // Continue waiting
            }
            
            await new Promise(resolve => setTimeout(resolve, 500));
        }
        
        throw new Error(`Daemon failed to start within ${timeout}ms`);
    }
    
    /**
     * Wait for daemon shutdown
     */
    async waitForShutdown() {
        const startTime = Date.now();
        const timeout = this.config.shutdownTimeoutMs;
        
        while (Date.now() - startTime < timeout) {
            const isRunning = await this.isAlreadyRunning();
            if (!isRunning) {
                return;
            }
            await new Promise(resolve => setTimeout(resolve, 500));
        }
        
        // Force kill if still running
        if (this.pid) {
            this.log('warning', 'Force killing daemon after timeout');
            try {
                process.kill(this.pid, 'SIGKILL');
            } catch (error) {
                // Process might already be dead
            }
        }
    }
    
    /**
     * Start health monitoring
     */
    startHealthMonitoring() {
        if (this.healthCheckTimer) {
            return;
        }
        
        this.healthCheckTimer = setInterval(async () => {
            try {
                await this.performHealthCheck();
                this.emit('health_check', { status: 'healthy' });
            } catch (error) {
                this.log('warning', 'Health check failed', { error: error.message });
                this.emit('health_check', { status: 'unhealthy', error: error.message });
                
                if (this.config.restartOnFailure) {
                    await this.handleUnhealthyDaemon();
                }
            }
        }, this.config.healthCheckIntervalMs);
    }
    
    /**
     * Stop health monitoring
     */
    stopHealthMonitoring() {
        if (this.healthCheckTimer) {
            clearInterval(this.healthCheckTimer);
            this.healthCheckTimer = null;
        }
    }
    
    /**
     * Handle unhealthy daemon
     */
    async handleUnhealthyDaemon() {
        if (this.restartAttempts >= this.config.maxRestartAttempts) {
            this.log('error', 'Max restart attempts reached, giving up');
            this.emit('failed', { reason: 'max_restart_attempts' });
            return;
        }
        
        this.restartAttempts++;
        this.log('info', `Attempting to restart daemon (attempt ${this.restartAttempts}/${this.config.maxRestartAttempts})`);
        
        try {
            await this.restart();
            this.emit('restarted', { attempt: this.restartAttempts });
        } catch (error) {
            this.log('error', 'Restart attempt failed', { error: error.message });
            
            // Exponential backoff for next attempt
            const delay = this.config.restartDelayMs * Math.pow(2, this.restartAttempts - 1);
            setTimeout(() => {
                this.handleUnhealthyDaemon();
            }, Math.min(delay, 60000)); // Max 1 minute delay
        }
    }
    
    /**
     * Handle daemon process exit
     */
    handleDaemonExit(code, signal) {
        this.log('info', 'Daemon process exited', { code, signal });
        this.cleanup();
        this.emit('exited', { code, signal });
        
        if (this.config.restartOnFailure && signal !== 'SIGTERM' && code !== 0) {
            this.handleUnhealthyDaemon();
        }
    }
    
    /**
     * Cleanup resources
     */
    cleanup() {
        this.process = null;
        this.pid = null;
        this.isRunning = false;
        this.stopHealthMonitoring();
    }
    
    /**
     * Clean up stale PID file
     */
    async cleanupStalePidFile() {
        try {
            await fs.unlink(this.config.pidFile);
            this.log('debug', 'Cleaned up stale PID file');
        } catch (error) {
            // Ignore errors
        }
    }
    
    /**
     * Structured logging
     */
    log(level, message, metadata = {}) {
        const timestamp = new Date().toISOString();
        const logEntry = {
            timestamp,
            level,
            component: 'mcp-daemon-manager',
            message,
            ...metadata
        };
        
        if (this.config.enableStructuredLogging) {
            console.log(JSON.stringify(logEntry));
        } else {
            const prefix = `[${timestamp}] [${level.toUpperCase()}] [daemon-manager]`;
            console.log(`${prefix} ${message}`, metadata);
        }
        
        this.emit('log', logEntry);
    }
}

module.exports = {
    GoxelDaemonManager,
    DaemonConfig
};