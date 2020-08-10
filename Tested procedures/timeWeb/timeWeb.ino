#include <dummy.h>

static const char vernum[] = "v60";               // version 60 Feb 26, 2020 - added dates/times to ftp
                                                  // version 59 Feb 23, 2020 - switch to 1 bit system, and pir
static const char devname[] = "desklens";         // name of your camera for mDNS, Router, and filenames


#define TIMEZONE "CST-8"             // your timezone  -  this is GMT


// 1 for blink red led with every sd card write, at your frame rate
// 0 for blink only for skipping frames and SOS if camera or sd is broken
#define BlinkWithWrite 0

// EDIT ssid and password
const char ssid[] = "Homeboy";           // your wireless network name (SSID)
const char password[] = "kaoshishunli888";  // your Wi-Fi network password

// startup defaults for first recording

// here are the recording options from the "retart web page" 
// VGA 10 fps for 30 min, repeat, realtime                   http://192.168.0.117/start?framesize=VGA&length=1800&interval=100&quality=10&repeat=100&speed=1&gray=0
// VGA 2 fps, for 30 minutes repeat, 30x playback            http://192.168.0.117/start?framesize=VGA&length=1800&interval=500&quality=10&repeat=300&speed=30&gray=0
// UXGA 1 sec per frame, for 30 minutes repeat, 30x playback http://192.168.0.117/start?framesize=UXGA&length=1800&interval=1000&quality=10&repeat=100&speed=30&gray=0
// UXGA 2 fps for 30 minutes repeat, 15x playback            http://192.168.0.117/start?framesize=UXGA&length=1800&interval=500&quality=10&repeat=100&speed=30&gray=0
// CIF 20 fps second for 30 minutes repeat                   http://192.168.0.117/start?framesize=CIF&length=1800&interval=50&quality=10&repeat=100&speed=1&gray=0

// reboot startup parameters here


//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"

#include <ESPmDNS.h>

#include <HTTPClient.h>

// Time
#include "time.h"



void startCameraServer();
httpd_handle_t camera_httpd = NULL;

char the_page[3000];

char localip[20];
WiFiEventId_t eventID;

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// setup() runs on cpu 1
//

void setup() {
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector  // creates other problems

  Serial.begin(115200);

  Serial.setDebugOutput(true);

  // zzz
  Serial.println("                                    ");
  Serial.println("-------------------------------------");
  Serial.printf("ESP-CAM Video Recorder %s\n", vernum);
  Serial.printf(" http://%s.local - to access the camera\n", devname);
  Serial.println("-------------------------------------");

  pinMode(33, OUTPUT);    // little red led on back of chip
  digitalWrite(33, LOW);           // turn on the red LED on the back of chip
  
  if (init_wifi()) { // Connected to WiFi
    delay(1);
  }

  startCameraServer();

  digitalWrite(33, HIGH);         // red light turns off when setup is complete


  pinMode(4, OUTPUT);                 // using 1 bit mode, shut off the Blinding Disk-Active Light
  digitalWrite(4, LOW);

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");


}




bool init_wifi()
{
  int connAttempts = 0;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  WiFi.setHostname(devname);
  //WiFi.printDiag(Serial);
  WiFi.begin(ssid, password);
  delay(1000);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
    if (connAttempts == 10) {
      Serial.println("Cannot connect - try again");
      WiFi.begin(ssid, password);
      WiFi.printDiag(Serial);
    }
    if (connAttempts == 20) {
      Serial.println("Cannot connect - fail");

      WiFi.printDiag(Serial);
      return false;
    }
    connAttempts++;
  }

  Serial.println("Internet connected");

  //WiFi.printDiag(Serial);

  if (!MDNS.begin(devname)) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.printf("mDNS responder started '%s'\n", devname);
  }

  // configTime(0, 0, "pool.ntp.org");
  setenv("TZ", TIMEZONE, 1);  // mountain time zone from #define at top
  tzset();

  time_t now ;
  tm timeinfo = { 0 };
  int retry = 0;
  const int retry_count = 10;
  delay(1000);
  time(&now);
  localtime_r(&now, &timeinfo);

  Serial.println(ctime(&now));
  sprintf(localip, "%s", WiFi.localIP().toString().c_str());

  return true;

}


////////////////////////////////////////////////////////////////////////////////////
//
// some globals for the loop()
//

long wakeup;
long last_wakeup = 0;


void loop()
{
  delay(1);

}




//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t start_handler(httpd_req_t *req) {

  esp_err_t res = ESP_OK;

  char  buf[80];
  size_t buf_len;
  char  new_res[20];

  if (recording == 1) {
    const char* resp = "You must Stop recording, before starting a new one.  Start over ...";
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
    return res;

  } else {
    //recording = 1;
    Serial.println("starting recording");

    sensor_t * s = esp_camera_sensor_get();

    int new_interval = capture_interval;
    int new_length = capture_interval * total_frames;

    int new_framesize = s->status.framesize;
    int new_quality = s->status.quality;
    int new_repeat = 0;
    int new_xspeed = 1;
    int new_xlength = 3;
    int new_gray = 0;

    /*
        Serial.println("");
        Serial.println("Current Parameters :");
        Serial.print("  Capture Interval = "); Serial.print(capture_interval);  Serial.println(" ms");
        Serial.print("  Length = "); Serial.print(capture_interval * total_frames / 1000); Serial.println(" s");
        Serial.print("  Quality = "); Serial.println(new_quality);
        Serial.print("  Framesize = "); Serial.println(new_framesize);
        Serial.print("  Repeat = "); Serial.println(repeat);
        Serial.print("  Speed = "); Serial.println(xspeed);
    */

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
      if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
        ESP_LOGI(TAG, "Found URL query => %s", buf);
        char param[32];
        /* Get value of expected key from query string */
        //Serial.println(" ... parameters");
        if (httpd_query_key_value(buf, "length", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x >= 5 && x <= 3600 * 24 ) {   // 5 sec to 24 hours
            new_length = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => length=%s", param);

        }
        if (httpd_query_key_value(buf, "repeat", param, sizeof(param)) == ESP_OK) {
          int x = atoi(param);
          if (x >= 0  ) {
            new_repeat = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => repeat=%s", param);
        }
        if (httpd_query_key_value(buf, "framesize", new_res, sizeof(new_res)) == ESP_OK) {
          if (strcmp(new_res, "UXGA") == 0) {
            new_framesize = 10;
          } else if (strcmp(new_res, "SVGA") == 0) {
            new_framesize = 7;
          } else if (strcmp(new_res, "VGA") == 0) {
            new_framesize = 6;
          } else if (strcmp(new_res, "CIF") == 0) {
            new_framesize = 5;
          } else {
            Serial.println("Only UXGA, SVGA, VGA, and CIF are valid!");

          }
          ESP_LOGI(TAG, "Found URL query parameter => framesize=%s", new_res);
        }
        if (httpd_query_key_value(buf, "quality", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x >= 10 && x <= 50) {                 // MINIMUM QUALITY 10 to save memory
            new_quality = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => quality=%s", param);
        }

        if (httpd_query_key_value(buf, "speed", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x >= 1 && x <= 100) {
            new_xspeed = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => speed=%s", param);
        }

        if (httpd_query_key_value(buf, "gray", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x == 1 ) {
            new_gray = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => gray=%s", param);
        }

        if (httpd_query_key_value(buf, "interval", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x >= 1 && x <= 180000) {  //  180,000 ms = 3 min
            new_interval = x;
          }
          ESP_LOGI(TAG, "Found URL query parameter => interval=%s", param);
        }
      }
    }

    framesize = new_framesize;
    capture_interval = new_interval;
    xlength = new_length;
    total_frames = new_length * 1000 / capture_interval;
    repeat = new_repeat;
    quality = new_quality;
    xspeed = new_xspeed;
    gray = new_gray;

    do_start("Starting a new AVI");
    httpd_resp_send(req, the_page, strlen(the_page));

    recording = 1;
    return ESP_OK;
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
void do_start(char *the_message) {

  Serial.print("do_start "); Serial.println(the_message);

  const char msg[] PROGMEM = R"rawliteral(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>%s ESP32-CAM Video Recorder</title>
</head>
<body>
<h1>%s<br>ESP32-CAM Video Recorder %s </h1><br>
 <h3>Message is <font color="red">%s</font></h3><br>
 Recording = %d (1 is active)<br>
 Capture Interval = %d ms<br>
 Length = %d seconds<br>
 Quality = %d (10 best to 50 worst)<br>
 Framesize = %d (10 UXGA, 7 SVGA, 6 VGA, 5 CIF)<br>
 Repeat = %d<br>
 Speed = %d<br>
 Gray = %d<br><br>

<br>
<br><div id="image-container"></div>

</body>
</html>)rawliteral";


  // sprintf(the_page, msg, devname, devname, vernum, the_message, recording, capture_interval, capture_interval * total_frames / 1000, quality, framesize, repeat, xspeed, gray);
  //Serial.println(strlen(msg));

}



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t index_handler(httpd_req_t *req) {

  // do_status("Refresh Status");
  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };
  // httpd_uri_t capture_uri = {
  //   .uri       = "/capture",
  //   .method    = HTTP_GET,
  //   .handler   = capture_handler,
  //   .user_ctx  = NULL
  // };

  // httpd_uri_t file_stop = {
  //   .uri       = "/stop",
  //   .method    = HTTP_GET,
  //   .handler   = stop_handler,
  //   .user_ctx  = NULL
  // };

  // httpd_uri_t file_start = {
  //   .uri       = "/start",
  //   .method    = HTTP_GET,
  //   .handler   = start_handler,
  //   .user_ctx  = NULL
  // };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    // httpd_register_uri_handler(camera_httpd, &capture_uri);
    // httpd_register_uri_handler(camera_httpd, &file_start);
    // httpd_register_uri_handler(camera_httpd, &file_stop);
  }

  Serial.println("Camera http started");
}
