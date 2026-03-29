script:
  build:
    cmake --build build/Desktop_Qt_6_9_1-Debug --target ToolLogPro
  run:
    /home/truongnguyen/Working/fake/ai-tool-log/ToolLogPro/build/Desktop_Qt_6_9_1-Debug/ToolLogPro
  simulate socket settings message:
    echo 'type:property_definition,message:786445:0' | nc -N 127.0.0.1 5555
    echo 'type:settings,message:airplane_mode_on:1' | nc -N 127.0.0.1 5555
    echo 'type:system_property,message:dev.bootcomplete:0' | nc -N 127.0.0.1 5555