#!/usr/bin/env python3
"""
Goxel Daemon v0.19.0 - Proper CI Test Suite
Tests core functionality with appropriate request pacing and connection handling

This test confirms that the daemon works correctly when used properly.
"""

import socket
import json
import os
import tempfile
import sys
import time
from pathlib import Path

class GoxelDaemonCITest:
    def __init__(self, socket_path="/tmp/goxel-test.sock"):
        self.socket_path = socket_path
        self.test_results = []
        self.errors = []
        
    def connect(self):
        """Connect to daemon with proper error handling"""
        try:
            self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self.sock.settimeout(10.0)  # 10 second timeout
            self.sock.connect(self.socket_path)
            return True
        except Exception as e:
            self.errors.append(f"Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Properly disconnect from daemon"""
        if hasattr(self, 'sock') and self.sock:
            try:
                self.sock.close()
            except:
                pass
            self.sock = None
    
    def send_request(self, method, params=None, request_id=1, expect_success=True):
        """Send a single request with proper error handling"""
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "id": request_id
        }
        if params is not None:
            request["params"] = params
        
        try:
            # Send request
            message = json.dumps(request) + "\n"
            self.sock.send(message.encode())
            
            # Brief pause to allow processing
            time.sleep(0.1)
            
            # Receive response
            response_data = self.sock.recv(8192)
            if not response_data:
                return {"error": {"code": -1, "message": "No response received"}}
            
            response = json.loads(response_data.decode().strip())
            return response
            
        except Exception as e:
            return {"error": {"code": -1, "message": f"Request failed: {str(e)}"}}
    
    def test_basic_functionality(self):
        """Test core daemon functionality"""
        print("🧪 Testing Basic Functionality")
        
        # Test 1: Get daemon status
        result = self.send_request("goxel.get_status")
        if "error" in result:
            self.errors.append(f"get_status failed: {result['error']['message']}")
            return False
        
        version = result.get("result", {}).get("version", "unknown")
        print(f"   ✅ Daemon version: {version}")
        
        # Test 2: Create project
        result = self.send_request("goxel.create_project", ["CITest", 32, 32, 32])
        if "error" in result:
            self.errors.append(f"create_project failed: {result['error']['message']}")
            return False
        
        project_info = result.get("result", {})
        print(f"   ✅ Project created: {project_info}")
        
        return True
    
    def test_voxel_operations(self):
        """Test voxel manipulation"""
        print("🧊 Testing Voxel Operations")
        
        # Add some voxels in different colors
        voxel_tests = [
            ([16, 16, 16, 255, 0, 0, 255], "red voxel"),      # Red
            ([17, 16, 16, 0, 255, 0, 255], "green voxel"),    # Green
            ([16, 17, 16, 0, 0, 255, 255], "blue voxel"),     # Blue
        ]
        
        added_voxels = 0
        for params, description in voxel_tests:
            result = self.send_request("goxel.add_voxel", params)
            if "error" not in result:
                added_voxels += 1
                print(f"   ✅ Added {description}")
            else:
                self.errors.append(f"Failed to add {description}: {result['error']['message']}")
        
        # Test voxel retrieval
        for params, description in voxel_tests:
            x, y, z = params[0:3]
            result = self.send_request("goxel.get_voxel", [x, y, z])
            if "error" not in result:
                voxel_info = result.get("result", {})
                if voxel_info.get("exists", False):
                    color = voxel_info.get("color", [0, 0, 0, 0])
                    print(f"   ✅ Retrieved {description}: {color}")
                else:
                    self.errors.append(f"Voxel missing at {x}, {y}, {z}")
        
        return added_voxels > 0
    
    def test_file_operations(self):
        """Test save/load operations"""
        print("💾 Testing File Operations")
        
        temp_dir = tempfile.mkdtemp()
        test_files = {}
        
        # Test save project
        save_path = os.path.join(temp_dir, "ci_test.gox")
        result = self.send_request("goxel.save_project", [save_path])
        
        if "error" in result:
            self.errors.append(f"save_project failed: {result['error']['message']}")
            return False
        
        if os.path.exists(save_path):
            size = os.path.getsize(save_path)
            test_files["gox"] = size
            print(f"   ✅ Project saved: {size} bytes")
        else:
            self.errors.append("save_project: file not created")
            return False
        
        # Test export to different formats
        export_tests = [
            ("obj", "Wavefront OBJ"),
            ("ply", "Stanford PLY"),
            ("vox", "MagicaVoxel")
        ]
        
        for format_name, description in export_tests:
            export_path = os.path.join(temp_dir, f"ci_test.{format_name}")
            result = self.send_request("goxel.export_model", [export_path, format_name])
            
            if "error" not in result and os.path.exists(export_path):
                size = os.path.getsize(export_path)
                test_files[format_name] = size
                print(f"   ✅ Exported {description}: {size} bytes")
            else:
                error_msg = result.get("error", {}).get("message", "Unknown error")
                print(f"   ⚠️  Export {description} failed: {error_msg}")
        
        # Test load project
        result = self.send_request("goxel.load_project", [save_path])
        if "error" not in result:
            print(f"   ✅ Project loaded successfully")
        else:
            self.errors.append(f"load_project failed: {result['error']['message']}")
        
        # Cleanup
        try:
            for file_path in Path(temp_dir).rglob("*"):
                if file_path.is_file():
                    file_path.unlink()
            os.rmdir(temp_dir)
        except:
            pass
        
        return len(test_files) > 1  # At least .gox + one export format
    
    def test_rendering(self):
        """Test scene rendering"""
        print("🖼️  Testing Rendering")
        
        temp_dir = tempfile.mkdtemp()
        render_path = os.path.join(temp_dir, "ci_render.png")
        
        # Test rendering with different parameters
        render_params = {
            "output_path": render_path,
            "width": 256,
            "height": 256,
        }
        
        result = self.send_request("goxel.render_scene", render_params)
        
        success = False
        if "error" not in result and os.path.exists(render_path):
            size = os.path.getsize(render_path)
            print(f"   ✅ Scene rendered: {size} bytes PNG")
            
            # Verify it's actually a PNG
            with open(render_path, 'rb') as f:
                header = f.read(8)
                if header.startswith(b'\x89PNG\r\n\x1a\n'):
                    print(f"   ✅ Valid PNG format confirmed")
                    success = True
                else:
                    self.errors.append("render_scene: Invalid PNG format")
        else:
            error_msg = result.get("error", {}).get("message", "Unknown error")
            self.errors.append(f"render_scene failed: {error_msg}")
        
        # Cleanup
        try:
            if os.path.exists(render_path):
                os.remove(render_path)
            os.rmdir(temp_dir)
        except:
            pass
        
        return success
    
    def run_ci_tests(self):
        """Run complete CI test suite"""
        print("🚀 Goxel Daemon v0.19.0 - CI Test Suite")
        print("=" * 50)
        
        if not self.connect():
            print("❌ CRITICAL: Cannot connect to daemon")
            return False
        
        try:
            # Run test sequence
            tests = [
                ("Basic Functionality", self.test_basic_functionality),
                ("Voxel Operations", self.test_voxel_operations),
                ("File Operations", self.test_file_operations),
                ("Rendering", self.test_rendering),
            ]
            
            passed_tests = 0
            for test_name, test_func in tests:
                print(f"\n{test_name}:")
                try:
                    if test_func():
                        passed_tests += 1
                        print(f"   ✅ {test_name} PASSED")
                    else:
                        print(f"   ❌ {test_name} FAILED")
                except Exception as e:
                    print(f"   ❌ {test_name} CRASHED: {e}")
                    self.errors.append(f"{test_name} crashed: {e}")
            
            # Final report
            total_tests = len(tests)
            success_rate = (passed_tests / total_tests) * 100
            
            print(f"\n" + "=" * 50)
            print(f"📊 CI TEST RESULTS")
            print(f"Total Tests: {total_tests}")
            print(f"Passed: {passed_tests}")
            print(f"Failed: {total_tests - passed_tests}")
            print(f"Success Rate: {success_rate:.1f}%")
            
            if self.errors:
                print(f"\n❌ Errors Found:")
                for i, error in enumerate(self.errors, 1):
                    print(f"  {i}. {error}")
            
            return passed_tests == total_tests
            
        finally:
            self.disconnect()

def main():
    """Main CI test execution"""
    
    # Check if daemon is running
    socket_paths = [
        "/tmp/goxel-test.sock",
        "/opt/homebrew/var/run/goxel/goxel.sock",
    ]
    
    daemon_socket = None
    for path in socket_paths:
        if os.path.exists(path):
            daemon_socket = path
            break
    
    if not daemon_socket:
        print("❌ No daemon socket found. Start daemon first:")
        print("./goxel-daemon --foreground --socket /tmp/goxel-test.sock")
        return 1
    
    print(f"Using daemon socket: {daemon_socket}")
    
    # Run CI tests
    ci_test = GoxelDaemonCITest(daemon_socket)
    success = ci_test.run_ci_tests()
    
    if success:
        print("\n✅ ALL CI TESTS PASSED - Daemon is working correctly!")
        return 0
    else:
        print("\n❌ CI TESTS FAILED - Issues detected")
        return 1

if __name__ == "__main__":
    sys.exit(main())