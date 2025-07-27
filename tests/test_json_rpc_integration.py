#!/usr/bin/env python3
"""
Goxel JSON RPC Integration Tests

Tests the JSON RPC API methods through socket communication.
This provides integration testing of all the implemented methods.
"""

import json
import socket
import subprocess
import time
import os
import signal
import tempfile
import sys
from typing import Dict, Any, Optional

class GoxelJSONRPCTester:
    """Test client for Goxel JSON RPC API"""
    
    def __init__(self, socket_path: str = "/tmp/goxel.sock"):
        self.socket_path = socket_path
        self.daemon_process = None
        self.request_id = 1
    
    def start_daemon(self) -> bool:
        """Start the Goxel daemon process"""
        try:
            # Remove existing socket
            if os.path.exists(self.socket_path):
                os.unlink(self.socket_path)
            
            # Start daemon
            cmd = ["../goxel-headless", "--daemon", "--socket", self.socket_path]
            self.daemon_process = subprocess.Popen(
                cmd, 
                stdout=subprocess.PIPE, 
                stderr=subprocess.PIPE,
                cwd=os.path.dirname(__file__)
            )
            
            # Wait for socket to be created
            for _ in range(50):  # 5 second timeout
                if os.path.exists(self.socket_path):
                    time.sleep(0.1)  # Give it a moment to be ready
                    return True
                time.sleep(0.1)
            
            print(f"ERROR: Socket {self.socket_path} was not created")
            return False
            
        except Exception as e:
            print(f"ERROR: Failed to start daemon: {e}")
            return False
    
    def stop_daemon(self):
        """Stop the daemon process"""
        if self.daemon_process:
            try:
                self.daemon_process.terminate()
                self.daemon_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.daemon_process.kill()
                self.daemon_process.wait()
        
        # Clean up socket
        if os.path.exists(self.socket_path):
            os.unlink(self.socket_path)
    
    def send_request(self, method: str, params: list = None) -> Optional[Dict[str, Any]]:
        """Send a JSON RPC request to the daemon"""
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params or [],
            "id": self.request_id
        }
        self.request_id += 1
        
        try:
            # Connect to socket
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.settimeout(10)  # 10 second timeout
            sock.connect(self.socket_path)
            
            # Send request
            request_json = json.dumps(request) + "\n"
            sock.send(request_json.encode('utf-8'))
            
            # Receive response
            response_data = b""
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response_data += chunk
                if b'\n' in chunk:  # Assuming newline-terminated responses
                    break
            
            sock.close()
            
            # Parse response
            response_str = response_data.decode('utf-8').strip()
            if response_str:
                return json.loads(response_str)
            else:
                print("ERROR: Empty response received")
                return None
                
        except Exception as e:
            print(f"ERROR: Failed to send request {method}: {e}")
            return None

def run_tests():
    """Run all JSON RPC method tests"""
    print("=== Goxel JSON RPC Integration Tests ===\n")
    
    tester = GoxelJSONRPCTester()
    tests_run = 0
    tests_passed = 0
    
    def test_assert(condition: bool, message: str):
        nonlocal tests_run, tests_passed
        tests_run += 1
        if condition:
            tests_passed += 1
            print(f"✓ {message}")
        else:
            print(f"✗ {message}")
    
    # Start daemon
    print("Starting Goxel daemon...")
    if not tester.start_daemon():
        print("FATAL: Could not start daemon")
        return False
    
    try:
        print("Daemon started successfully!\n")
        
        # Test 1: Create Project
        print("=== Testing goxel.create_project ===")
        response = tester.send_request("goxel.create_project", ["Test Project", 32, 32, 32])
        test_assert(response is not None, "Create project request returns response")
        test_assert(response.get("jsonrpc") == "2.0", "Response has correct JSON-RPC version")
        
        if response and "result" in response:
            result = response["result"]
            test_assert(result.get("success") == True, "Create project succeeds")
            test_assert(result.get("name") == "Test Project", "Project name is correct")
            test_assert(result.get("width") == 32, "Project width is correct")
            test_assert(result.get("height") == 32, "Project height is correct")
            test_assert(result.get("depth") == 32, "Project depth is correct")
            print(f"  Created project: {result.get('name')} ({result.get('width')}x{result.get('height')}x{result.get('depth')})")
        
        # Test 2: Get Status
        print("\n=== Testing goxel.get_status ===")
        response = tester.send_request("goxel.get_status")
        test_assert(response is not None, "Get status request returns response")
        
        if response and "result" in response:
            result = response["result"]
            test_assert("version" in result, "Status includes version")
            test_assert("layer_count" in result, "Status includes layer count")
            test_assert("width" in result, "Status includes width")
            test_assert("height" in result, "Status includes height")
            test_assert("depth" in result, "Status includes depth")
            print(f"  Version: {result.get('version')}")
            print(f"  Layers: {result.get('layer_count')}")
            print(f"  Dimensions: {result.get('width')}x{result.get('height')}x{result.get('depth')}")
        
        # Test 3: Add Voxel
        print("\n=== Testing goxel.add_voxel ===")
        response = tester.send_request("goxel.add_voxel", [0, -16, 0, 255, 0, 0, 255, 0])
        test_assert(response is not None, "Add voxel request returns response")
        
        if response and "result" in response:
            result = response["result"]
            test_assert(result.get("success") == True, "Add voxel succeeds")
            test_assert(result.get("x") == 0, "Voxel X coordinate is correct")
            test_assert(result.get("y") == -16, "Voxel Y coordinate is correct")
            test_assert(result.get("z") == 0, "Voxel Z coordinate is correct")
            print(f"  Added voxel at ({result.get('x')}, {result.get('y')}, {result.get('z')})")
        
        # Test 4: Get Voxel
        print("\n=== Testing goxel.get_voxel ===")
        response = tester.send_request("goxel.get_voxel", [0, -16, 0])
        test_assert(response is not None, "Get voxel request returns response")
        
        if response and "result" in response:
            result = response["result"]
            test_assert(result.get("x") == 0, "Retrieved voxel X coordinate is correct")
            test_assert(result.get("y") == -16, "Retrieved voxel Y coordinate is correct")
            test_assert(result.get("z") == 0, "Retrieved voxel Z coordinate is correct")
            test_assert("exists" in result, "Get voxel includes exists field")
            test_assert("color" in result, "Get voxel includes color field")
            print(f"  Retrieved voxel at ({result.get('x')}, {result.get('y')}, {result.get('z')})")
            print(f"  Exists: {result.get('exists')}, Color: {result.get('color')}")
        
        # Test 5: List Layers
        print("\n=== Testing goxel.list_layers ===")
        response = tester.send_request("goxel.list_layers")
        test_assert(response is not None, "List layers request returns response")
        
        if response and "result" in response:
            result = response["result"]
            test_assert("count" in result, "List layers includes count")
            test_assert("layers" in result, "List layers includes layers array")
            print(f"  Found {result.get('count')} layers")
        
        # Test 6: Create Layer
        print("\n=== Testing goxel.create_layer ===")
        response = tester.send_request("goxel.create_layer", ["Test Layer", 128, 128, 255, True])
        test_assert(response is not None, "Create layer request returns response")
        
        if response and "result" in response:
            result = response["result"]
            test_assert(result.get("success") == True, "Create layer succeeds")
            test_assert(result.get("name") == "Test Layer", "Layer name is correct")
            test_assert(result.get("visible") == True, "Layer visibility is correct")
            print(f"  Created layer: {result.get('name')}")
        
        # Test 7: Remove Voxel
        print("\n=== Testing goxel.remove_voxel ===")
        response = tester.send_request("goxel.remove_voxel", [0, -16, 0, 0])
        test_assert(response is not None, "Remove voxel request returns response")
        
        if response and "result" in response:
            result = response["result"]
            test_assert(result.get("success") == True, "Remove voxel succeeds")
            test_assert(result.get("x") == 0, "Removed voxel X coordinate is correct")
            test_assert(result.get("y") == -16, "Removed voxel Y coordinate is correct")
            test_assert(result.get("z") == 0, "Removed voxel Z coordinate is correct")
            print(f"  Removed voxel at ({result.get('x')}, {result.get('y')}, {result.get('z')})")
        
        # Test 8: Save Project
        print("\n=== Testing goxel.save_project ===")
        test_file = "/tmp/test_project.gox"
        response = tester.send_request("goxel.save_project", [test_file])
        test_assert(response is not None, "Save project request returns response")
        
        if response and "result" in response:
            result = response["result"]
            test_assert(result.get("success") == True, "Save project succeeds")
            test_assert(result.get("path") == test_file, "Save path is correct")
            print(f"  Saved project to: {result.get('path')}")
        
        # Test 9: Load Project
        print("\n=== Testing goxel.load_project ===")
        response = tester.send_request("goxel.load_project", [test_file])
        test_assert(response is not None, "Load project request returns response")
        
        if response and "result" in response:
            result = response["result"]
            test_assert(result.get("success") == True, "Load project succeeds")
            test_assert(result.get("path") == test_file, "Load path is correct")
            print(f"  Loaded project from: {result.get('path')}")
        
        # Test 10: Export Model
        print("\n=== Testing goxel.export_model ===")
        export_file = "/tmp/test_export.obj"
        response = tester.send_request("goxel.export_model", [export_file])
        test_assert(response is not None, "Export model request returns response")
        
        if response and "result" in response:
            result = response["result"]
            test_assert(result.get("success") == True, "Export model succeeds")
            test_assert(result.get("path") == export_file, "Export path is correct")
            print(f"  Exported model to: {result.get('path')}")
        
        # Test 11: Error Handling - Unknown Method
        print("\n=== Testing Error Handling ===")
        response = tester.send_request("unknown.method")
        test_assert(response is not None, "Unknown method request returns response")
        test_assert("error" in response, "Unknown method returns error")
        
        if response and "error" in response:
            error = response["error"]
            test_assert(error.get("code") == -32601, "Unknown method returns correct error code")
            print(f"  Unknown method error: {error.get('message')}")
        
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    except Exception as e:
        print(f"\nUnexpected error during tests: {e}")
    
    finally:
        # Always stop daemon
        print("\nStopping daemon...")
        tester.stop_daemon()
    
    # Print results
    print("\n=== Test Results ===")
    print(f"Tests run: {tests_run}")
    print(f"Tests passed: {tests_passed}")
    print(f"Tests failed: {tests_run - tests_passed}")
    print(f"Success rate: {100.0 * tests_passed / tests_run:.1f}%" if tests_run > 0 else "0.0%")
    
    if tests_passed == tests_run:
        print("\n🎉 All tests passed!")
        return True
    else:
        print("\n❌ Some tests failed.")
        return False

if __name__ == "__main__":
    success = run_tests()
    sys.exit(0 if success else 1)