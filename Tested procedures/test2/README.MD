从 https://github.com/jameszah/ESP32-CAM-Video-Recorder/ 改造而来
结合了https://github.com/s60sc/ESP32-CAM_Motion
补充了一个h文件 不然在编译的时候会有一点问题 处理得很粗糙,等待修改.

目前问题:cpu性能似乎不足(也可能是我负优化)录制的时候会有跳帧的问题,在SVGA清晰度下没有办法达到10fps.