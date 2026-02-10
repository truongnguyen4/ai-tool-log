export QtApp=/home/truongnguyen/Working/fake/ai-tool-log/ToolLogPro
export QMAKE=/home/truongnguyen/Qt/6.9.1/gcc_64/bin/qmake
rm -rf /home/truongnguyen/Working/fake/ai-tool-log/ToolLogPro/deploy/app
rm -rf /home/truongnguyen/Working/fake/ai-tool-log/ToolLogPro/deploy/ToolLogPro-x86_64.AppImage

QMAKE=/home/truongnguyen/Qt/6.9.1/gcc_64/bin/qmake ./linuxdeploy-x86_64.AppImage --appdir app -e $QtApp/build/Desktop_Qt_6_9_1-Debug/ToolLogPro -d ./deploy.desktop -i $QtApp/deploy/ToolLogPro.png --plugin qt --output appimage

zip /home/truongnguyen/Working/fake/ai-tool-log/ToolLogPro/tool.zip *

#  NOTE: This script is intended to be run inside the Docker container defined by Dockerfile.build.
# docker run --rm -it toollogpro-builder:ubuntu20 bash
