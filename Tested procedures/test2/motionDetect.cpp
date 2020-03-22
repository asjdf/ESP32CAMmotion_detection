
/* 
 扩展以使用质心偏移检测连续图像中的运动。
 该技术减少了虚假的运动变化：
 -相机噪点，特别是在弱光下
 -微小的运动，例如树叶沙沙作响，下雨
 -短暂的运动，例如鸟类飞过
 -照明水平的变化，例如过云
 图像之间的变化量将取决于帧频。
 更快的帧速率将需要更高的灵敏度
 更改帧大小时，OV2640会输出一些毛刺的帧 
 进行过渡。这些可以解释为虚假运动。

 s60sc 2020
 
*/
#include "motionDetect.h"
#include "esp_camera.h"
#include "esp_jpg_decode.h"
#include <SD_MMC.h>

// 用于校准运动检测的用户配置参数
// user configuration parameters for calibrating motion detection
//改变帧，以确认运动分钟序列
#define MOTION_SEQUENCE 5 // min sequence of changed frames to confirm motion 
//连续黑暗的帧，以避免伪白天/夜晚开关
#define NIGHT_SEQUENCE 10 // frames of sequential darkness to avoid spurious day / night switching
//用于调试，真正加入BMP头，因为没有虚假
#define WANT_BMP false // for debugging, true to add BMP header, false for none

// 常数
// constants
//数每像素的字节
#define RGB888_BYTES 3 // number of bytes per pixel
//大小的BMP标题字节
#define BMP_HEADER 54 // size of BMP header bytes

// 自动换行printf
// auto newline printf
#define showInfo(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)
#define showError(format, ...) Serial.printf("ERROR: " format "\n", ##__VA_ARGS__)
#define showDebug(format, ...) if (debug) Serial.printf("DEBUG: " format "\n", ##__VA_ARGS__)

/*********声明并初始化以下内容********/
/********* the following must be declared and initialised elsewhere **********/
extern bool debug;
extern uint8_t fsizePtr;
//当前环境光水平
extern uint8_t lightLevel; // Current ambient light level 
//运动灵敏度设置
extern uint8_t motionVal; // motion sensitivity setting

struct frameStruct {
  const char* frameSizeStr;
  const uint16_t frameWidth;
  const uint16_t frameHeight;
  const uint16_t defaultFPS;
  const uint8_t scaleFactor;
  const uint8_t sampleRate;
};
extern const frameStruct frameData[];

/**********************************************************************************/

static bool jpg2rgb(const uint8_t *src, size_t src_len, uint8_t ** out, size_t * out_len, uint8_t scale, uint8_t bmpOffset);

bool motionDetection(camera_fb_t* fb, bool motionStatus) {
  //获取当前帧并计算质心，然后与上一个进行比较
  // get current frame and calculate centre of mass, then compare with previous
  uint32_t mTime = millis();
  //将jpeg转换为缩小的位图
  // convert jpeg to downsized bitmap
  uint8_t* bmpBuf = NULL;
  size_t buf_len = 0;
  // BMP标头的大小（如果需要）
  uint8_t bmpOffset = WANT_BMP ? BMP_HEADER : 0; // size of BMP header if wanted
  bool converted = jpg2rgb((uint8_t*)fb->buf, fb->len, &bmpBuf, &buf_len, frameData[fsizePtr].scaleFactor, bmpOffset);
  if (debug && bmpOffset) { 
    //出于调试目的，以BMP格式存储在SD中，以检查位图格式正确
    // for debug purposes, store on SD as BMP to check that bitmap is properly formed
    File bmpFile = SD_MMC.open("/test.bmp", FILE_WRITE);
    bmpFile.write(bmpBuf, buf_len); 
    bmpFile.close(); 
    showDebug("wrote BMP to SD");
  }
  
  if (converted) {       
    showDebug("Jpeg to bitmap conversion time %ums", millis()-mTime); 
    uint32_t cTime = millis(); 
    static uint16_t motionCnt = 0;
    uint32_t lux = 0;

    //计算样本量的参数
    // calculate parameters for sample size
    uint8_t downsize = pow(2, frameData[fsizePtr].scaleFactor) * frameData[fsizePtr].sampleRate;
    uint8_t numCols = frameData[fsizePtr].frameWidth / downsize;
    uint8_t numRows = frameData[fsizePtr].frameHeight / downsize;
    uint16_t pixelSpan = frameData[fsizePtr].sampleRate * RGB888_BYTES;
    uint16_t bitmapWidth = numCols * frameData[fsizePtr].sampleRate;
    //每列每种颜色的总像素值
    uint32_t col[numCols][RGB888_BYTES] = {0}; // total pixel values per color per column 
    //每行每种颜色的总像素值
    int32_t row[numRows][RGB888_BYTES] = {0}; // total pixel values per color per row
    uint32_t numSamples = numCols * numRows;
    showDebug("%u samples for %s", numSamples, frameData[fsizePtr].frameSizeStr);
 
    //获取每种采样像素颜色的质量（像素值）并在行和列中累加
    // get mass (pixel value) of each sampled pixel color and accumulate in row and column
    for (int r=0; r<numRows; r++) {
      for (int c=0; c<numCols; c++) {
        uint32_t pixelPos = bmpOffset+((c+(r*bitmapWidth))*pixelSpan); // within bitmap buffer
        for (uint32_t rgb=0; rgb<RGB888_BYTES; rgb++) {
          //每个像素都有RGB值
          // each pixel has RGB values
          row[r][rgb] += bmpBuf[pixelPos+rgb]; 
          col[c][rgb] += bmpBuf[pixelPos+rgb];
          lux += bmpBuf[pixelPos+rgb];
        }
      }  
    }

    //计算图像的质心
    // calculate image centre of mass
    //每种颜色的列总和
    uint32_t cSum[RGB888_BYTES] = {0}; // total column sum per color
    //每种颜色的总加权列总和（质量）
    float cMass[RGB888_BYTES] = {0};  //  total weighted column sum (mass) per color
    //x轴的质心
    float cCOM[RGB888_BYTES] = {0}; // x axis centre of mass
    //当前和上一个x轴之间的差异
    float cCOMdiff[RGB888_BYTES] = {0}; // difference between current and previous x axis
    //先前的x轴质心
    static float cCOMprev[RGB888_BYTES] = {0}; // previous x axis centre of mass
    //如上所示，对于行/ y轴
    uint32_t rSum[RGB888_BYTES] = {0}; // as above for rows / y axis
    float rMass[RGB888_BYTES] = {0};
    float rCOM[RGB888_BYTES] = {0};
    float rCOMdiff[RGB888_BYTES] = {0};
    static float rCOMprev[RGB888_BYTES] = {0};
    
    for (int rgb=0; rgb<RGB888_BYTES; rgb++) {
      for (int r=0; r<numRows; r++) { 
        // calculate total row sum and total row relative mass per color
        rSum[rgb] += row[r][rgb];
        rMass[rgb] += (row[r][rgb] * (r+1));
      }
      for (int c=0; c<numCols; c++) {
        // calculate total col sum and total col relative mass per color
        cSum[rgb] += col[c][rgb];
        cMass[rgb] += (col[c][rgb] * (c+1));
      }   
      // calculate x and y centre of mass per color and difference to previous
      cCOM[rgb] = cMass[rgb] / cSum[rgb];
      cCOMdiff[rgb] = cCOM[rgb] - cCOMprev[rgb];
      cCOMprev[rgb] = cCOM[rgb];
      rCOM[rgb] = rMass[rgb] / rSum[rgb];
      rCOMdiff[rgb] = rCOM[rgb] - rCOMprev[rgb];
      rCOMprev[rgb] = rCOM[rgb];    
    }
    // get single x & y differences and percent change
    float COMtot = 0;
    float COMtotDiff = 0;
    for (int rgb=0; rgb<RGB888_BYTES; rgb++) {
      COMtot += (abs(cCOM[rgb])+abs(rCOM[rgb]));
      COMtotDiff += (abs(cCOMdiff[rgb])+abs(rCOMdiff[rgb]));
    }
    float diffPcnt = (COMtotDiff*100)/COMtot; // total % difference between frames  
    
    lightLevel = (lux*100)/(numSamples*RGB888_BYTES*255); // light value as a %
    showDebug("diffPcnt %0.2f%%, lux %u%%, %ums", diffPcnt, lightLevel, millis()-cTime);

    // determine if movement has occurred
    if (diffPcnt > (float)0.01*pow(2, 10-motionVal)) { //  min % shift to indicate movement
      // sufficient centre of mass shift
      if (!motionStatus) motionCnt += 1;
      showDebug("### Change detected");
      // need minimum sequence of changes to signal valid movement
      if (!motionStatus && motionCnt >= MOTION_SEQUENCE) {
        showDebug("***** Motion - START");
        motionStatus = true; // motion started
      } 
    } else {
      // insufficient change
      if (motionStatus) {
        motionCnt = 0;
        showDebug("***** Motion - STOP");
        motionStatus = false; // motion stopped
      }
    }
    if (motionStatus) showDebug("*** Motion - ongoing");
    showDebug("Total motion processing for frame %ums", millis()-mTime);
  } else showError("Image conversion failed");

  // esp32-cam issue #126
  if (bmpBuf == NULL) showError("Memory leak, heap now: %u", xPortGetFreeHeapSize());
  free(bmpBuf);
  bmpBuf = NULL;
  return motionStatus;
}

bool isNight(uint8_t nightSwitch) {
  // check if night time for switching on lamp during recording
  static bool nightTime = false;
  static uint16_t nightCnt = 0;
  if (!nightTime && lightLevel < nightSwitch) {
    // dark image
    nightCnt += 1;
    // only signal night time after given sequence of dark frames
    if (nightCnt > NIGHT_SEQUENCE) {
      nightTime = true;     
      showInfo("Night time"); 
    }
  } 
  if (lightLevel > nightSwitch) {
    nightCnt = 0;
    if (nightTime) {
      nightTime = false;
      showInfo("Day time");
    }
  }  
  return nightTime;
}

/************* copied and modified from esp32-camera/to_bmp.c to access jpg_scale_t *****************/

typedef struct {
  uint16_t width;
  uint16_t height;
  uint16_t data_offset;
  const uint8_t *input;
  uint8_t *output;
} rgb_jpg_decoder;

static bool _rgb_write(void * arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data) {
  rgb_jpg_decoder * jpeg = (rgb_jpg_decoder *)arg;
  if (!data){
    if (x == 0 && y == 0) {
      // write start
      jpeg->width = w;
      jpeg->height = h;
      // if output is null, this is BMP
      if (!jpeg->output) {
        jpeg->output = (uint8_t*)ps_malloc((w*h*3)+jpeg->data_offset);
        if (!jpeg->output) return false;
      }
    } 
    return true;
  }

  size_t jw = jpeg->width*3;
  size_t t = y * jw;
  size_t b = t + (h * jw);
  size_t l = x * 3;
  uint8_t *out = jpeg->output+jpeg->data_offset;
  uint8_t *o = out;
  size_t iy, ix;
  w = w * 3;

  for (iy=t; iy<b; iy+=jw) {
    o = out+iy+l;
    for (ix=0; ix<w; ix+=3) {
      o[ix] = data[ix+2];
      o[ix+1] = data[ix+1];
      o[ix+2] = data[ix];
    }
    data+=w;
  }
  return true;
}

static uint32_t _jpg_read(void * arg, size_t index, uint8_t *buf, size_t len) {
  rgb_jpg_decoder * jpeg = (rgb_jpg_decoder *)arg;
  if (buf) memcpy(buf, jpeg->input + index, len);
  return len;
}

typedef struct {
  uint32_t filesize;
  uint32_t reserved;
  uint32_t fileoffset_to_pixelarray;
  uint32_t dibheadersize;
  int32_t width;
  int32_t height;
  uint16_t planes;
  uint16_t bitsperpixel;
  uint32_t compression;
  uint32_t imagesize;
  uint32_t ypixelpermeter;
  uint32_t xpixelpermeter;
  uint32_t numcolorspallette;
  uint32_t mostimpcolor;
} bmp_header_t;

static bool jpg2rgb(const uint8_t *src, size_t src_len, uint8_t ** out, size_t * out_len, uint8_t scale, uint8_t bmpOffset) {
  rgb_jpg_decoder jpeg;
  jpeg.width = 0;
  jpeg.height = 0;
  jpeg.input = src;
  jpeg.output = NULL; 
  jpeg.data_offset = bmpOffset;
  if (esp_jpg_decode(src_len, jpg_scale_t(scale), _jpg_read, _rgb_write, (void*)&jpeg) != ESP_OK) {
    *out = jpeg.output;
    return false;
  }

  size_t output_size = jpeg.width*jpeg.height*3;

  if (WANT_BMP) {
    jpeg.output[0] = 'B';
    jpeg.output[1] = 'M';
    bmp_header_t * bitmap = (bmp_header_t*)&jpeg.output[2];
    bitmap->reserved = 0;
    bitmap->filesize = output_size+bmpOffset;
    bitmap->fileoffset_to_pixelarray = bmpOffset;
    bitmap->dibheadersize = 40;
    bitmap->width = jpeg.width;
    bitmap->height = -jpeg.height; //set negative for top to bottom
    bitmap->planes = 1;
    bitmap->bitsperpixel = 24;
    bitmap->compression = 0;
    bitmap->imagesize = output_size;
    bitmap->ypixelpermeter = 0x0B13 ; //2835 , 72 DPI
    bitmap->xpixelpermeter = 0x0B13 ; //2835 , 72 DPI
    bitmap->numcolorspallette = 0;
    bitmap->mostimpcolor = 0;
  }
  *out = jpeg.output;
  *out_len = output_size+bmpOffset;

  return true;
}
