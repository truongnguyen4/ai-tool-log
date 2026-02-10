# ────────────────────────────────────────────────────────────────────────────
# ToolLogPro – Ubuntu 24.04 runtime container
#
# Build:
#   docker build -t toollogpro .
#
# Run (X11 forwarding – same machine):
#   # 1. On the HOST: expose the adb server on all interfaces (once per boot)
#   adb start-server
#   socat TCP-LISTEN:5037,fork,reuseaddr,bind=0.0.0.0 \
#         TCP:127.0.0.1:5037 &
#
#   # 2. Allow X11 connections from Docker (once per session)
#   xhost +local:docker
#
#   # 3. Start the container
#   docker run --rm -it \
#     -e DISPLAY=$DISPLAY \
#     -v /tmp/.X11-unix:/tmp/.X11-unix \
#     --add-host host.docker.internal:host-gateway \
#     toollogpro
# ────────────────────────────────────────────────────────────────────────────
FROM ubuntu:24.04

# Prevent interactive prompts during apt
ENV DEBIAN_FRONTEND=noninteractive

# ── System libraries required by the Qt/XCB stack (not bundled in AppImage) ──
RUN apt-get update && apt-get install -y --no-install-recommends \
    # FUSE – needed to mount/run AppImage
    libfuse2t64 \
    fuse \
    # X11 / display
    libx11-6 \
    libx11-xcb1 \
    libxext6 \
    libxrender1 \
    libxi6 \
    libxtst6 \
    # OpenGL / EGL (software Mesa fallback so no GPU required)
    libgl1 \
    libopengl0 \
    libegl1 \
    libgles2 \
    libglx0 \
    # Font stack
    libfontconfig1 \
    libfreetype6 \
    fonts-dejavu-core \
    # Compression & crypto (not fully bundled)
    zlib1g \
    libgpg-error0 \
    libgcrypt20 \
    # C++ runtime
    libstdc++6 \
    libgcc-s1 \
    # D-Bus (needed by Qt platform plugins)
    libdbus-1-3 \
    dbus \
    # ADB – client only; server runs on the host and is reached via TCP
    adb \
    socat \
    # Misc utilities
    ca-certificates \
    udev \
 && rm -rf /var/lib/apt/lists/*

# ── Copy the AppImage ──────────────────────────────────────────────────────
COPY deploy/ToolLogPro-x86_64.AppImage /opt/ToolLogPro/ToolLogPro.AppImage
RUN chmod +x /opt/ToolLogPro/ToolLogPro.AppImage

# ── Non-root user for safer operation ─────────────────────────────────────
RUN groupadd -r toollogpro && useradd -r -g toollogpro -m -d /home/toollogpro toollogpro \
 && mkdir -p /home/toollogpro/.config \
 && chown -R toollogpro:toollogpro /home/toollogpro

USER toollogpro
WORKDIR /home/toollogpro

# ── Environment ───────────────────────────────────────────────────────────
ENV QT_QPA_PLATFORM=xcb
# Tell AppImage not to use FUSE and instead extract to a temp dir.
# This avoids needing a privileged FUSE mount inside the container
# while still using the bundled Qt libraries from the AppImage.
ENV APPIMAGE_EXTRACT_AND_RUN=1
ENV HOME=/home/toollogpro

# ── ADB: point the client at the host's adb server (TCP forwarded via socat) ──
# host.docker.internal resolves to the Docker bridge gateway when
# --add-host host.docker.internal:host-gateway is passed at runtime.
ENV ANDROID_ADB_SERVER_ADDRESS=host.docker.internal
ENV ANDROID_ADB_SERVER_PORT=5037

ENTRYPOINT ["/opt/ToolLogPro/ToolLogPro.AppImage"]
