# ESP32MotionDetectionWithVideoRecorder
## ESP32移动侦测摄像机

### 目录结构:
- Formal edition(正式版)
- References(参考的源码 向原制作者表示感谢)
- Tested procedures(测试功能的程序)

### 目前功能:
- 移动侦测
- 自动录制视频

### 目前问题:
- 在SVGA清晰度下无法达到5fps

### 未来计划:
- 性能优化
- 降低功耗

#### 更新记录:
- 2020.3.22 完成基本程序,或者说程序跑起来没问题.
- 2020.03.12 从CameraWifiMotion借了改摄像头模式的函数 未测试 已做小修改以匹配自己的测试例程

#### 参考资料:
- https://github.com/jameszah/ESP32-CAM-Video-Recorder
- https://github.com/s60sc/ESP32-CAM_Motion
- https://github.com/alanesq/CameraWifiMotion
- https://eloquentarduino.github.io/2020/01/motion-detection-with-esp32-cam-only-arduino-version/#tocreal-world-example
- https://github.com/raphaelbs/esp32-cam-ai-thinker/blob/master/examples/change_detection/README.md

再次感谢以上程序的作者