#include <WiFi.h>
#include "time.h"
#include <WiFiAP.h>


void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);

  time_t now;
  char strftime_buf[64];
  struct tm timeinfo;

  time(&now);
  // Set timezone to China Standard Time
  setenv("TZ", "CST-8", 1);
  tzset();

  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  Serial.println(strftime_buf);

  struct tm tmGet;
  tmGet.tm_year = 2018 - 1900;
  tmGet.tm_mon = 10;
  tmGet.tm_mday = 15;
  tmGet.tm_hour = 14;
  tmGet.tm_min = 10;
  tmGet.tm_sec = 10;
  time_t t = mktime(&tmGet);
  printf("Setting time: %s", asctime(&tmGet));
  timeval tmSet = { .tv_sec = t };
  settimeofday(&tmSet, NULL);

  printLocalTime();
}

void loop()
{
  delay(1000);
  printLocalTime();
}