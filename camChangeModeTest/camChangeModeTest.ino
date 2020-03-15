#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiAP.h>
#include "soc/soc.h"           // 解决 brownour 问题
#include "soc/rtc_cntl_reg.h"  // 解决 brownour 问题
#include "FS.h"
#include "SD_MMC.h"
#include <time.h>
#include "esp_http_server.h"
#include <EEPROM.h>
// 新建一个eeprom的区块以及设置大小
EEPROMClass  picCount("eeprom0", 0x500);

// ap的设置
const char* ssid = "HomeBoy3";
const char* password = "kaoshishunli888";
// for fixed IP Address
IPAddress ip(192, 168, 0, 1);			//IP
IPAddress gateway(192, 168, 0, 1);		//Gateway
IPAddress subnet(255, 255, 255, 0);		//Subnet
IPAddress DNS(192, 168, 0, 1);			//DNS

#define CAMERA_MODEL_AI_THINKER
#if defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#endif

int F_UXGA = 10;
int F_QVGA = 4;
int gray = 2;
int rgb = 0;
#define JST     3600*9
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

IPAddress localIp;
bool isSDExist = 0;
httpd_handle_t httpd_stream = NULL;
httpd_handle_t httpd_camera = NULL;
camera_fb_t* fb = NULL;
bool semaphore = false;//旗标 防止同时调用cam(借来的代码 目前看是这个用处)

//设置eeprom记录照片号的位置
#define EEPROM_SIZE 1
int pictureNumber = 0;

//移动侦测相关
#define FRAME_SIZE FRAMESIZE_QVGA
#define WIDTH 320//帧高
#define HEIGHT 240//帧宽
#define BLOCK_SIZE 10//检测块数量
#define W (WIDTH / BLOCK_SIZE)
#define H (HEIGHT / BLOCK_SIZE)
#define BLOCK_DIFF_THRESHOLD 0.2
#define IMAGE_DIFF_THRESHOLD 0.1
#define DEBUG 1

uint16_t prev_frame[H][W] = { 0 };
uint16_t current_frame[H][W] = { 0 };

bool setup_camera(framesize_t);
bool capture_still();
bool motion_detect();
void update_frame();
void print_frame(uint16_t frame[H][W]);
void savePic(camera_fb_t* fb);

/******************************************** Capture image and do down-sampling */
bool capture_still() {
    camera_fb_t *frame_buffer = esp_camera_fb_get();

    if (!frame_buffer)
        return false;

    // set all 0s in current frame
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            current_frame[y][x] = 0;


    // down-sample image in blocks
    for (uint32_t i = 0; i < WIDTH * HEIGHT; i++) {
        const uint16_t x = i % WIDTH;
        const uint16_t y = floor(i / WIDTH);
        const uint8_t block_x = floor(x / BLOCK_SIZE);
        const uint8_t block_y = floor(y / BLOCK_SIZE);
        const uint8_t pixel = frame_buffer->buf[i];
        const uint16_t current = current_frame[block_y][block_x];

        // average pixels in block (accumulate)
        current_frame[block_y][block_x] += pixel;
    }

    // average pixels in block (rescale)
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            current_frame[y][x] /= BLOCK_SIZE * BLOCK_SIZE;

    esp_camera_fb_return(frame_buffer);
    frame_buffer = NULL;
    #if DEBUG
    Serial.println("Current frame:");
    print_frame(current_frame);
    Serial.println("---------------");
    #endif

    return true;
}

/* Compute the number of different blocks If there are enough, then motion happened */
bool motion_detect() {
    uint16_t changes = 0;
    const uint16_t blocks = (WIDTH * HEIGHT) / (BLOCK_SIZE * BLOCK_SIZE);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float current = current_frame[y][x];
            float prev = prev_frame[y][x];
            float delta = abs(current - prev) / prev;

            if (delta >= BLOCK_DIFF_THRESHOLD) {
                #if DEBUG
                Serial.print("diff\t");
                Serial.print(y);
                Serial.print('\t');
                Serial.println(x);
                #endif

                changes += 1;
            }
        }
    }

    Serial.print("Changed ");
    Serial.print(changes);
    Serial.print(" out of ");
    Serial.println(blocks);

    return (1.0 * changes / blocks) > IMAGE_DIFF_THRESHOLD;
}

/**
 * Copy current frame to previous
 */
void update_frame() {
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            prev_frame[y][x] = current_frame[y][x];
        }
    }
}

/**
 * For serial debugging
 * @param frame
 */
void print_frame(uint16_t frame[H][W]) {
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            Serial.print(frame[y][x]);
            Serial.print('\t');
        }

        Serial.println();
    }
}

void setupSerial(){
  Serial.begin(115200);
  Serial.setDebugOutput(true);
}

int setupSD(){
  int sdRet = SD_MMC.begin();
  Serial.printf("setupSD: SD_MMC.begin() -> %d\n", sdRet);

  uint8_t cardType = SD_MMC.cardType();
  if(cardType != CARD_NONE){
    isSDExist = 1;
  }

  switch(cardType){
    case CARD_NONE: Serial.println("setupSD: Card: None");      break;
    case CARD_MMC:  Serial.println("setupSD: Card Type: MMC");  break;
    case CARD_SD:   Serial.println("setupSD: Card Type: SDSC"); break;
    case CARD_SDHC: Serial.println("setupSD: Card Type: SDHC"); break;
    default:        Serial.println("setupSD: Card: UNKNOWN");   break;
  }
}

int setupCamera(){
    int ret = 0;
    
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    // config.pixel_format = PIXFORMAT_GRAYSCALE;//灰度模式
    // config.frame_size = FRAMESIZE_VGA;
    config.frame_size = FRAME_SIZE;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err == ESP_OK) {
        Serial.println("setupCamera: OK");
        ret = 1;
    }else{
        Serial.printf("setupCamera: err: 0x%08x\n", err);
        ret = 0;    
    }

    return ret;
}

int setupWifi(){
  // WiFi.config(ip, gateway, subnet, DNS);   // Set fixed IP address
  // WiFi.begin(ssid, password);
  WiFi.softAP(ssid, password);
  delay(500);
  if (!WiFi.softAPConfig(ip, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }
  Serial.print("seupWifi: connecting");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }
  Serial.println("");
  Serial.println("seupWifi: connected");
  // Serial.println(WiFi.localIP());
  // localIp = WiFi.localIP();
  Serial.println(WiFi.softAPIP());
  localIp = WiFi.softAPIP();
  
  return 1;
}



void savePic(camera_fb_t* fb){
    Serial.println("savePic: Start");
    if(!isSDExist){
        Serial.println("savePic: SD Card none");    
        return;
    }

    if (!fb) {
        Serial.println("savePic: image data none");
        return;
    }

    time_t t = time(NULL);
    struct tm *tm;
    tm = localtime(&t);
    
    //   //   两种起名方式 我用顺序排列
    //   char filename[32];
    //   sprintf(filename, "/img%04d%02d%02d%02d%02d%02d.jpg",
    //     tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    // 顺序排列起名
    uint32_t pictureNumber;
    picCount.get(0, pictureNumber);
    pictureNumber = pictureNumber + 1;
    Serial.println(pictureNumber);
    // Path where new picture will be saved in SD Card
    char filename[32];
    sprintf(filename, "/img%010d.jpg",
        pictureNumber);
    picCount.put(0, pictureNumber);

    fs::FS &fs = SD_MMC;
    File imgFile = fs.open(filename, FILE_WRITE);
    if(imgFile && fb->len > 0){
        imgFile.write(fb->buf, fb->len);
        imgFile.close();
        Serial.print("savePic: ");
        Serial.println(filename);
    }else{
        Serial.println("savePic: Save file failed");
        Serial.printf("savePic: imgFile: %d\n", imgFile);
        Serial.printf("savePic: fb->len: %d\n", fb->len);      
    }
}


// ----------------------------------------------------------------
//              -restart  the camera in different mode
// ----------------------------------------------------------------
//  pixformats = PIXFORMAT_ + YUV422,GRAYSCALE,RGB565,JPEG
//  framesizes = FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
// switches camera mode - format = PIXFORMAT_GRAYSCALE or PIXFORMAT_JPEG

// camera configuration settings
camera_config_t config;
#define FRAME_SIZE_MOTION FRAMESIZE_QVGA     // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA - Do not use sizes above QVGA when not JPEG
#define FRAME_SIZE_PHOTO FRAMESIZE_XGA       //   Image sizes: 160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 320x240 (QVGA), 400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)

void RestartCamera(pixformat_t format) {
    bool ok;
    Serial.println("a");
    esp_camera_deinit();
    Serial.println("b");
      if (format == PIXFORMAT_JPEG) config.frame_size = FRAME_SIZE_PHOTO;
      else if (format == PIXFORMAT_GRAYSCALE) config.frame_size = FRAME_SIZE_MOTION;
      else Serial.println("ERROR: Invalid image format");
      config.pixel_format = format;
      Serial.println("c");
    ok = esp_camera_init(&config);
    Serial.println("d");
    if (ok == ESP_OK) {
      Serial.println("Camera mode switched ok");
    }
    else {
      // failed so try again
        delay(50);
        ok = esp_camera_init(&config);
        if (ok == ESP_OK) Serial.println("Camera mode switched ok - 2nd attempt");
        else {
          Serial.println("Camera failed to restart so rebooting camera");
          RebootCamera(format);
        }
    }
}


// reboot camera (used if camera is failing to respond)
//      restart camera in motion mode, capture a test frame to check it is now responding ok
//      format = PIXFORMAT_GRAYSCALE or PIXFORMAT_JPEG

void RebootCamera(pixformat_t format) {  
    Serial.println("ERROR: Problem with camera detected so rebooting it"); 
    // turn camera off then back on      
        digitalWrite(PWDN_GPIO_NUM, HIGH);
        delay(500);
        digitalWrite(PWDN_GPIO_NUM, LOW); 
        delay(500);
    RestartCamera(PIXFORMAT_GRAYSCALE);    // restart camera in motion mode
    delay(500);
    // try capturing a frame, if still problem reboot esp32
        if (!capture_still()) {
            Serial.println("Camera failed to reboot so rebooting esp32");
            delay(500);
            ESP.restart();   
            delay(5000);      // restart will fail without this delay
         }
    if (format == PIXFORMAT_JPEG) RestartCamera(PIXFORMAT_JPEG);                  // if jpg mode required restart camera again
}


void setup() {

    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); //禁用低电压自动关机
    // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); //启用低电压自动关机
    setupSerial();
    if (!picCount.begin(picCount.length())) {
        Serial.println("Failed to initialise picCount");
        Serial.println("Restarting...");
        delay(1000);
        ESP.restart();
    }
    if(!setupSD()){
        Serial.println("setupSD failed.");
        return;
    }
    if(!setupCamera()){
        Serial.println("setupCamera failed.");
        return;
    }
}

void loop() {
    delay(5000);
    

    // update_frame();
    Serial.println("=================");
    //侦测模式
    if (1){
        Serial.println("a");
        RestartCamera(PIXFORMAT_GRAYSCALE);
        Serial.println("b");
        fb = esp_camera_fb_get();
        delay(100);
        if (!fb) {
            Serial.println("resp_jpg: Camera capture failed");
            return;
        }
        // save image to SD card
        savePic(fb);
        esp_camera_fb_return(fb);
        fb = NULL;
    }
    //拍摄模式
    if(1){
        RestartCamera(PIXFORMAT_JPEG);
        delay(100);
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("resp_jpg2: Camera capture failed");
            return;
        }
        // save image to SD card
        savePic(fb);
        esp_camera_fb_return(fb);
        fb = NULL;
    } 
    
}


// sensor_t * s = esp_camera_sensor_get();
// if(!strcmp(variable, "framesize")) {
//     if(s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
// }
// else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
// else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
// else if(!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
// else if(!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
// else if(!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
// else if(!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
// else if(!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
// else if(!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
// else if(!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
// else if(!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
// else if(!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
// else if(!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
// else if(!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
// else if(!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
// else if(!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
// else if(!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
// else if(!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
// else if(!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
// else if(!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
// else if(!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
// else if(!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
// else if(!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
// else if(!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);

// <select id="framesize" class="default-action">
//     <option value="10">UXGA(1600x1200)</option>
//     <option value="9">SXGA(1280x1024)</option>
//     <option value="8">XGA(1024x768)</option>
//     <option value="7">SVGA(800x600)</option>
//     <option value="6">VGA(640x480)</option>
//     <option value="5" selected="selected">CIF(400x296)</option>
//     <option value="4">QVGA(320x240)</option>
//     <option value="3">HQVGA(240x176)</option>
//     <option value="0">QQVGA(160x120)</option>
// </select>

// <select id="special_effect" class="default-action">
//     <option value="0" selected="selected">No Effect</option>
//     <option value="1">Negative</option>
//     <option value="2">Grayscale</option>
//     <option value="3">Red Tint</option>
//     <option value="4">Green Tint</option>
//     <option value="5">Blue Tint</option>
//     <option value="6">Sepia</option>
// </select>