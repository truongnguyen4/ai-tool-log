#!/bin/bash
export QtApp=/home/truongnguyen/Working/fake/ai-tool-log/ToolLogPro
export QMAKE=/home/truongnguyen/Qt/6.9.1/gcc_64/bin/qmake
rm -rf /home/truongnguyen/Working/fake/ai-tool-log/ToolLogPro/deploy/app
rm -rf /home/truongnguyen/Working/fake/ai-tool-log/ToolLogPro/deploy/ToolLogPro-x86_64.AppImage

# Temporarily move SQL drivers that have unresolvable system dependencies to a temp dir.
# linuxdeploy-plugin-qt scans *.so* so renaming in-place (e.g. .skip) still matches;
# moving to a separate directory is the only reliable way to hide them.
# Only libqsqlite.so is fully self-contained on this machine.
SQLDRIVERS_DIR=/home/truongnguyen/Qt/6.9.1/gcc_64/plugins/sqldrivers
SQLDRIVERS_BACKUP_DIR=$(mktemp -d /tmp/sqldrivers_backup_XXXXXX)
for _drv in libqsqlmimer.so libqsqlmysql.so libqsqlodbc.so libqsqlpsql.so; do
    [ -f "$SQLDRIVERS_DIR/$_drv" ] && mv "$SQLDRIVERS_DIR/$_drv" "$SQLDRIVERS_BACKUP_DIR/"
done
# Restore all hidden drivers on exit (normal, error, or interrupt)
trap 'mv "$SQLDRIVERS_BACKUP_DIR"/*.so "$SQLDRIVERS_DIR/" 2>/dev/null; rm -rf "$SQLDRIVERS_BACKUP_DIR"' EXIT INT TERM

QMAKE=/home/truongnguyen/Qt/6.9.1/gcc_64/bin/qmake ./linuxdeploy-x86_64.AppImage --appdir app -e $QtApp/build/Desktop_Qt_6_9_1-Debug/ToolLogPro -d ./deploy.desktop -i $QtApp/deploy/ToolLogPro.png --plugin qt --output appimage

zip /home/truongnguyen/Working/fake/ai-tool-log/ToolLogPro/tool.zip *

#  NOTE: This script is intended to be run inside the Docker container defined by Dockerfile.build.
# docker run --rm -it toollogpro-builder:ubuntu20 bash
