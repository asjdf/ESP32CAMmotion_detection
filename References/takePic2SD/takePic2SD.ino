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
    config.fb_count = 2;

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

int setupLocalTime(){
  time_t t;
  struct tm *tm;
  Serial.print("setupLocalTime: start config");
  configTime(JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  do {
    delay(500);
    t = time(NULL);
    tm = localtime(&t);
    Serial.print(".");
  } while(tm->tm_year + 1900 < 2000);
  Serial.println("");
  Serial.printf("setupLocalTime: %04d/%02d/%02d - %02d:%02d:%02d\n",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
  return 1;
}

static esp_err_t handle_index(httpd_req_t *req){
  static char resp_html[1024];
  char * p = resp_html;
  
  p += sprintf(p, "<!doctype html>");
  p += sprintf(p, "<html>");
  p += sprintf(p, "<head>");
  p += sprintf(p, "<meta name=\"viewport\" content=\"width=device-width\">");
  p += sprintf(p, "<title>ESP32-CAM</title>");
  p += sprintf(p, "</head>");
  p += sprintf(p, "<body>");
  p += sprintf(p, "<div align=center>");
  p += sprintf(p, "<img id=stream src=\"\">");
  p += sprintf(p, "</div>");  
  p += sprintf(p, "<div align=center>");
  p += sprintf(p, "<button id=savepic>Take Picture</button>");
  p += sprintf(p, "</div>");  
  p += sprintf(p, "<script>");
  p += sprintf(p, "document.addEventListener('DOMContentLoaded', function (event) {\n");
  p += sprintf(p, "  var baseHost = document.location.origin;\n");
  p += sprintf(p, "  var streamUrl = baseHost + ':81';\n");
  p += sprintf(p, "  const view = document.getElementById('stream');\n");
  p += sprintf(p, "  document.getElementById('stream').src = `${streamUrl}/stream`;\n");
  p += sprintf(p, "  document.getElementById('savepic').onclick = () => {\n");
  p += sprintf(p, "    window.stop();\n");
  p += sprintf(p, "    view.src = `${baseHost}/capture?_cb=${Date.now()}`;\n");
  p += sprintf(p, "    console.log(view.src);\n");
  p += sprintf(p, "    setTimeout(function(){\n");
  p += sprintf(p, "      view.src = `${streamUrl}/stream`;\n");
  p += sprintf(p, "      console.log(view.src);\n");
  p += sprintf(p, "    }, 3000);\n");
  p += sprintf(p, "  }\n");
  p += sprintf(p, "})");
  p += sprintf(p, "\n");
  p += sprintf(p, "</script>");
  p += sprintf(p, "</body>");
  p += sprintf(p, "</html>");
  *p++ = 0;

  Serial.printf("handle_index: len: %d\n", strlen(resp_html));

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, resp_html, strlen(resp_html));
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

esp_err_t resp_jpg(httpd_req_t *req, bool isSave){
    Serial.printf("resp_jpg: isSave: %d\n", isSave);
    esp_err_t res = ESP_OK;

    if(fb){
        esp_camera_fb_return(fb);
        fb = NULL;
    }
    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("resp_jpg: Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if(isSave){
        // save image to SD card
        savePic(fb);
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    Serial.printf("resp_jpg: JPG: %uB\n", (uint32_t)(fb->len));
    esp_camera_fb_return(fb);
    fb = NULL;
    Serial.println("resp_jpg: end");
    return res;  
}

static esp_err_t handle_jpg(httpd_req_t *req){
    while(semaphore){
        delay(100);
    }
    semaphore = true;
    Serial.println("handle_jpg: Start");
    esp_err_t ret = resp_jpg(req, false);
    semaphore = false;
    return ret;
}

static esp_err_t handle_capture(httpd_req_t *req){
    while(semaphore){
        delay(100);
    }
    semaphore = true;
    Serial.println("handle_capture: Start");
    esp_err_t ret = resp_jpg(req, true);
    semaphore = false;
    return ret;
}

static esp_err_t handle_stream(httpd_req_t *req){
    while(semaphore){
    delay(100);
    }
    semaphore = true;

    //camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    char * part_buf[64];

    Serial.println("handle_stream: Start");
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
    Serial.println("handle_stream: err: httpd_resp_set_type");
    semaphore = false;
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("handle_stream: Camera capture failed");
      res = ESP_FAIL;
    } else {
      Serial.printf("handle_stream: buffer: %dB\n", fb->len);
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
    }
    if(res != ESP_OK){
      break;
    }
  }

  Serial.println("handle_stream: exit");
  semaphore = false;
  return res;
}

int setupWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    httpd_uri_t uri_index = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = handle_index,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_jpg = {
        .uri       = "/jpg",
        .method    = HTTP_GET,
        .handler   = handle_jpg,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_capture = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = handle_capture,
        .user_ctx  = NULL
    };

    httpd_uri_t uri_stream = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = handle_stream,
        .user_ctx  = NULL
    };

    Serial.printf("setupWebServer: web port: '%d'\n", config.server_port);
    if (httpd_start(&httpd_camera, &config) == ESP_OK) {
        httpd_register_uri_handler(httpd_camera, &uri_index);
        httpd_register_uri_handler(httpd_camera, &uri_jpg);
        httpd_register_uri_handler(httpd_camera, &uri_capture);
    }  

    config.server_port += 1;
    config.ctrl_port += 1;
    Serial.printf("setupWebServer: stream port: '%d'\n", config.server_port);
    if (httpd_start(&httpd_stream, &config) == ESP_OK) {
        httpd_register_uri_handler(httpd_stream, &uri_stream);
    }
}

void setup() {

    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //禁用低电压自动关机
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
    if(!setupWifi()){
        Serial.println("setupWifi failed.");
        return;
    }
    
    // if(!setupLocalTime()){
    //   Serial.println("setupLocalTime failed.");
    //   return;
    // }

    //setupWebServer();
    
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(localIp);
    Serial.println("' to connect");
}

void loop() {
    delay(1000);
    if (!capture_still()) {
        Serial.println("Failed capture");
        delay(3000);

        return;
    }

    if (motion_detect()) {
        Serial.println("Motion detected");
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        //拍摄模式
        // sensor_t *sensor = esp_camera_sensor_get();  //FEB2
        // sensor->set_pixformat(sensor, PIXFORMAT_JPEG);  //FEB2
        // sensor->set_framesize(sensor, FRAMESIZE_UXGA);  //FEB2
        // sensor->set_quality(sensor, 10);  //FEB2
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("resp_jpg: Camera capture failed");
            return;
        }
        // save image to SD card
        savePic(fb);
        esp_camera_fb_return(fb);
        fb = NULL;

        //侦测模式
        // sensor->set_pixformat(sensor, PIXFORMAT_GRAYSCALE);  //FEB2
        // sensor->set_framesize(sensor, FRAME_SIZE);  //FEB2
        // sensor->set_quality(sensor, 12);  //FEB2
        delay(1000);
    }

    update_frame();
    Serial.println("=================");
}
