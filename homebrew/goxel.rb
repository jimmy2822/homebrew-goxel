class Goxel < Formula
  desc "3D voxel editor with daemon mode for automation"
  homepage "https://goxel.xyz"
  url "https://github.com/guillaumechereau/goxel/archive/v14.0.0.tar.gz"
  sha256 "PLACEHOLDER_SHA256" # Will be updated when release is created
  license "GPL-3.0-or-later"
  head "https://github.com/guillaumechereau/goxel.git", branch: "master"

  depends_on "pkg-config" => :build
  depends_on "scons" => :build
  depends_on "glfw"
  depends_on "libpng"
  depends_on "tre"

  on_linux do
    depends_on "gtk+3"
    depends_on "mesa"
    depends_on "mesa-glu"
  end

  def install
    # Build both GUI and daemon versions
    system "scons", "mode=release"
    system "scons", "mode=release", "daemon=1"

    # Install binaries
    bin.install "goxel"
    bin.install "goxel-daemon"

    # Install documentation
    doc.install "README.md", "CHANGELOG.md", "CONTRIBUTING.md"
    
    # Install examples
    (share/"goxel/examples").install Dir["examples/*"]
    
    # Install data files
    (share/"goxel/data").install Dir["data/*"]
  end

  service do
    run [opt_bin/"goxel-daemon", "--foreground", "--socket", var/"run/goxel/goxel.sock"]
    keep_alive true
    environment_variables PATH: std_service_path_env
    log_path var/"log/goxel/daemon.log"
    error_log_path var/"log/goxel/daemon_error.log"
    working_dir var/"lib/goxel"
  end

  def post_install
    # Create necessary directories for the service
    (var/"run/goxel").mkpath
    (var/"log/goxel").mkpath
    (var/"lib/goxel").mkpath
  end

  test do
    # Test GUI version help
    assert_match "Goxel", shell_output("#{bin}/goxel --help 2>&1", 1)

    # Test daemon version
    pid = fork do
      exec bin/"goxel-daemon", "--socket", testpath/"test.sock"
    end
    sleep 2

    # Test if socket file was created
    assert_predicate testpath/"test.sock", :exist?

    # Clean up
    Process.kill("TERM", pid)
    Process.wait(pid)
  end

  def caveats
    <<~EOS
      Goxel has been installed with both GUI and daemon modes.

      GUI mode:
        Run 'goxel' to start the graphical editor

      Daemon mode:
        Start manually:
          goxel-daemon --foreground --socket /tmp/goxel.sock

        Or use brew services:
          brew services start goxel

        The daemon will create a socket at:
          #{var}/run/goxel/goxel.sock

        Logs are available at:
          #{var}/log/goxel/daemon.log

      Example Python client:
        #{share}/goxel/examples/json_rpc_client.py
    EOS
  end
end