/****************************************************************************************************************************
 * ConfigOnSwitch.ino
 * For ESP8266 / ESP32 boards
 *
 * ESP_WiFiManager is a library for the ESP8266/ESP32 platform (https://github.com/esp8266/Arduino) to enable easy
 * configuration and reconfiguration of WiFi credentials using a Captive Portal. Inspired by:
 * http://www.esp8266.com/viewtopic.php?f=29&t=2520
 * https://github.com/chriscook8/esp-arduino-apboot
 * https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/CaptivePortalAdvanced/
 *
 * Forked from Tzapu https://github.com/tzapu/WiFiManager
 * and from Ken Taylor https://github.com/kentaylor
 * 
 * Built by Khoi Hoang https://github.com/khoih-prog/ESP_WiFiManager
 * Licensed under MIT license
 * Version: 1.0.4
 *
 * Version Modified By   Date      Comments
 * ------- -----------  ---------- -----------
 *  1.0.0   K Hoang      07/10/2019 Initial coding
 *  1.0.1   K Hoang      13/12/2019 Fix bug. Add features. Add support for ESP32
 *  1.0.4   K Hoang      07/01/2020 Use ESP_WiFiManager setHostname feature
 *  1.0.5   K Hoang       15/01/2020 Add configurable DNS feature. Thanks to @Amorphous of https://community.blynk.cc
 *****************************************************************************************************************************/
/****************************************************************************************************************************
 * This example will open a configuration portal when no WiFi configuration has been previously entered or when a button is pushed. 
 * It is the easiest scenario for configuration but requires a pin and a button on the ESP8266 device. 
 * The Flash button is convenient for this on NodeMCU devices.
 *
 * Also in this example a password is required to connect to the configuration portal 
 * network. This is inconvenient but means that only those who know the password or those 
 * already connected to the target WiFi network can access the configuration portal and 
 * the WiFi network credentials will be sent from the browser over an encrypted connection and
 * can not be read by observers.
 *****************************************************************************************************************************/
 
//For ESP32, To use ESP32 Dev Module, QIO, Flash 4MB/80MHz, Upload 921600
 
//Ported to ESP32
#ifdef ESP32
  #include <esp_wifi.h>
  #include <WiFi.h>
  #include <WiFiClient.h>

  #define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

  #define LED_ON      HIGH
  #define LED_OFF     LOW  
#else
  #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
  //needed for library
  #include <DNSServer.h>
  #include <ESP8266WebServer.h>  

  #define ESP_getChipId()   (ESP.getChipId())

  #define LED_ON      LOW
  #define LED_OFF     HIGH
#endif

// SSID and PW for Config Portal
String ssid = "ESP_" + String(ESP_getChipId(), HEX);
const char* password = "your_password";

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

// Use false if you don't like to display Available Pages in Information Page of Config Portal
// Comment out or use true to display Available Pages in Information Page of Config Portal
// Must be placed before #include <ESP_WiFiManager.h> 
#define USE_AVAILABLE_PAGES     false

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

#ifdef ESP32

  //See file .../hardware/espressif/esp32/variants/(esp32|doitESP32devkitV1)/pins_arduino.h
  #define LED_BUILTIN       2         // Pin D2 mapped to pin GPIO2/ADC12 of ESP32, control on-board LED
  #define PIN_LED           2         // Pin D2 mapped to pin GPIO2/ADC12 of ESP32, control on-board LED
  
  #define PIN_D0            0         // Pin D0 mapped to pin GPIO0/BOOT/ADC11/TOUCH1 of ESP32
  #define PIN_D1            1         // Pin D1 mapped to pin GPIO1/TX0 of ESP32
  #define PIN_D2            2         // Pin D2 mapped to pin GPIO2/ADC12/TOUCH2 of ESP32
  #define PIN_D3            3         // Pin D3 mapped to pin GPIO3/RX0 of ESP32
  #define PIN_D4            4         // Pin D4 mapped to pin GPIO4/ADC10/TOUCH0 of ESP32
  #define PIN_D5            5         // Pin D5 mapped to pin GPIO5/SPISS/VSPI_SS of ESP32
  #define PIN_D6            6         // Pin D6 mapped to pin GPIO6/FLASH_SCK of ESP32
  #define PIN_D7            7         // Pin D7 mapped to pin GPIO7/FLASH_D0 of ESP32
  #define PIN_D8            8         // Pin D8 mapped to pin GPIO8/FLASH_D1 of ESP32
  #define PIN_D9            9         // Pin D9 mapped to pin GPIO9/FLASH_D2 of ESP32
    
  #define PIN_D10           10        // Pin D10 mapped to pin GPIO10/FLASH_D3 of ESP32
  #define PIN_D11           11        // Pin D11 mapped to pin GPIO11/FLASH_CMD of ESP32
  #define PIN_D12           12        // Pin D12 mapped to pin GPIO12/HSPI_MISO/ADC15/TOUCH5/TDI of ESP32
  #define PIN_D13           13        // Pin D13 mapped to pin GPIO13/HSPI_MOSI/ADC14/TOUCH4/TCK of ESP32
  #define PIN_D14           14        // Pin D14 mapped to pin GPIO14/HSPI_SCK/ADC16/TOUCH6/TMS of ESP32
  #define PIN_D15           15        // Pin D15 mapped to pin GPIO15/HSPI_SS/ADC13/TOUCH3/TDO of ESP32
  #define PIN_D16           16        // Pin D16 mapped to pin GPIO16/TX2 of ESP32
  #define PIN_D17           17        // Pin D17 mapped to pin GPIO17/RX2 of ESP32     
  #define PIN_D18           18        // Pin D18 mapped to pin GPIO18/VSPI_SCK of ESP32
  #define PIN_D19           19        // Pin D19 mapped to pin GPIO19/VSPI_MISO of ESP32

  #define PIN_D21           21        // Pin D21 mapped to pin GPIO21/SDA of ESP32
  #define PIN_D22           22        // Pin D22 mapped to pin GPIO22/SCL of ESP32
  #define PIN_D23           23        // Pin D23 mapped to pin GPIO23/VSPI_MOSI of ESP32
  #define PIN_D24           24        // Pin D24 mapped to pin GPIO24 of ESP32
  #define PIN_D25           25        // Pin D25 mapped to pin GPIO25/ADC18/DAC1 of ESP32
  #define PIN_D26           26        // Pin D26 mapped to pin GPIO26/ADC19/DAC2 of ESP32
  #define PIN_D27           27        // Pin D27 mapped to pin GPIO27/ADC17/TOUCH7 of ESP32     
     
  #define PIN_D32           32        // Pin D32 mapped to pin GPIO32/ADC4/TOUCH9 of ESP32
  #define PIN_D33           33        // Pin D33 mapped to pin GPIO33/ADC5/TOUCH8 of ESP32
  #define PIN_D34           34        // Pin D34 mapped to pin GPIO34/ADC6 of ESP32

  //Only GPIO pin < 34 can be used as output. Pins >= 34 can be only inputs
  //See .../cores/esp32/esp32-hal-gpio.h/c
  //#define digitalPinIsValid(pin)          ((pin) < 40 && esp32_gpioMux[(pin)].reg)
  //#define digitalPinCanOutput(pin)        ((pin) < 34 && esp32_gpioMux[(pin)].reg)
  //#define digitalPinToRtcPin(pin)         (((pin) < 40)?esp32_gpioMux[(pin)].rtc:-1)
  //#define digitalPinToAnalogChannel(pin)  (((pin) < 40)?esp32_gpioMux[(pin)].adc:-1)
  //#define digitalPinToTouchChannel(pin)   (((pin) < 40)?esp32_gpioMux[(pin)].touch:-1)
  //#define digitalPinToDacChannel(pin)     (((pin) == 25)?0:((pin) == 26)?1:-1)

  #define PIN_D35           35        // Pin D35 mapped to pin GPIO35/ADC7 of ESP32
  #define PIN_D36           36        // Pin D36 mapped to pin GPIO36/ADC0/SVP of ESP32
  #define PIN_D39           39        // Pin D39 mapped to pin GPIO39/ADC3/SVN of ESP32
  
  #define PIN_RX0            3        // Pin RX0 mapped to pin GPIO3/RX0 of ESP32
  #define PIN_TX0            1        // Pin TX0 mapped to pin GPIO1/TX0 of ESP32
  
  #define PIN_SCL           22        // Pin SCL mapped to pin GPIO22/SCL of ESP32
  #define PIN_SDA           21        // Pin SDA mapped to pin GPIO21/SDA of ESP32   
#else  
  //PIN_D0 can't be used for PWM/I2C
  #define PIN_D0            16        // Pin D0 mapped to pin GPIO16/USER/WAKE of ESP8266. This pin is also used for Onboard-Blue LED. PIN_D0 = 0 => LED ON
  #define PIN_D1            5         // Pin D1 mapped to pin GPIO5 of ESP8266
  #define PIN_D2            4         // Pin D2 mapped to pin GPIO4 of ESP8266
  #define PIN_D3            0         // Pin D3 mapped to pin GPIO0/FLASH of ESP8266
  #define PIN_D4            2         // Pin D4 mapped to pin GPIO2/TXD1 of ESP8266
  #define PIN_LED           2         // Pin D4 mapped to pin GPIO2/TXD1 of ESP8266, NodeMCU and WeMoS, control on-board LED
  #define PIN_D5            14        // Pin D5 mapped to pin GPIO14/HSCLK of ESP8266
  #define PIN_D6            12        // Pin D6 mapped to pin GPIO12/HMISO of ESP8266
  #define PIN_D7            13        // Pin D7 mapped to pin GPIO13/RXD2/HMOSI of ESP8266
  #define PIN_D8            15        // Pin D8 mapped to pin GPIO15/TXD2/HCS of ESP8266
  
  //Don't use pins GPIO6 to GPIO11 as already connected to flash, etc. Use them can crash the program
  //GPIO9(D11/SD2) and GPIO11 can be used only if flash in DIO mode ( not the default QIO mode)
  #define PIN_D11           9         // Pin D11/SD2 mapped to pin GPIO9/SDD2 of ESP8266
  #define PIN_D12           10        // Pin D12/SD3 mapped to pin GPIO10/SDD3 of ESP8266
  #define PIN_SD2           9         // Pin SD2 mapped to pin GPIO9/SDD2 of ESP8266
  #define PIN_SD3           10        // Pin SD3 mapped to pin GPIO10/SDD3 of ESP8266
  
  #define PIN_D9            3         // Pin D9 /RX mapped to pin GPIO3/RXD0 of ESP8266
  #define PIN_D10           1         // Pin D10/TX mapped to pin GPIO1/TXD0 of ESP8266
  #define PIN_RX            3         // Pin RX mapped to pin GPIO3/RXD0 of ESP8266
  #define PIN_TX            1         // Pin RX mapped to pin GPIO1/TXD0 of ESP8266
  
  #define LED_PIN           16        // Pin D0 mapped to pin GPIO16 of ESP8266. This pin is also used for Onboard-Blue LED. PIN_D0 = 0 => LED ON

#endif    //USE_ESP32

#ifdef ESP32
  /* Trigger for inititating config mode is Pin D3 and also flash button on NodeMCU
   * Flash button is convenient to use but if it is pressed it will stuff up the serial port device driver 
   * until the computer is rebooted on windows machines.
   */
  const int TRIGGER_PIN = PIN_D0;   // Pin D0 mapped to pin GPIO0/BOOT/ADC11/TOUCH1 of ESP32
  /*
   * Alternative trigger pin. Needs to be connected to a button to use this pin. It must be a momentary connection
   * not connected permanently to ground. Either trigger pin will work.
   */
  const int TRIGGER_PIN2 = PIN_D25; // Pin D25 mapped to pin GPIO25/ADC18/DAC1 of ESP32
#else
  /* Trigger for inititating config mode is Pin D3 and also flash button on NodeMCU
   * Flash button is convenient to use but if it is pressed it will stuff up the serial port device driver 
   * until the computer is rebooted on windows machines.
   */
  const int TRIGGER_PIN = PIN_D3; // D3 on NodeMCU and WeMos.
  /*
   * Alternative trigger pin. Needs to be connected to a button to use this pin. It must be a momentary connection
   * not connected permanently to ground. Either trigger pin will work.
   */
  const int TRIGGER_PIN2 = PIN_D7; // D7 on NodeMCU and WeMos.

#endif

// Indicates whether ESP has WiFi credentials saved from previous session
bool initialConfig = false;

void heartBeatPrint(void)
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED)
    Serial.print("H");        // H means connected to WiFi
  else
    Serial.print("F");        // F means not connected to WiFi
  
  if (num == 80) 
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0) 
  {
    Serial.print(" ");
  }
} 

void check_status()
{
  static ulong checkstatus_timeout = 0;

  #define HEARTBEAT_INTERVAL    10000L
  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((millis() > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = millis() + HEARTBEAT_INTERVAL;
  }
}

void setup() 
{
  // put your setup code here, to run once:
  // initialize the LED digital pin as an output.
  pinMode(PIN_LED, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(TRIGGER_PIN2, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println("\nStarting");

  unsigned long startedAt = millis();

  //Local intialization. Once its business is done, there is no need to keep it around
  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  //ESP_WiFiManager ESP_wifiManager;
  // Use this to personalize DHCP hostname (RFC952 conformed)
  ESP_WiFiManager ESP_wifiManager("ConfigOnSwitch");
  
  ESP_wifiManager.setMinimumSignalQuality(-1);
  // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
  ESP_wifiManager.setSTAStaticIPConfig(IPAddress(192,168,2,114), IPAddress(192,168,2,1), IPAddress(255,255,255,0), 
                                        IPAddress(192,168,2,1), IPAddress(8,8,8,8));
  
  // We can't use WiFi.SSID() in ESP32as it's only valid after connected. 
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();
  
  //Remove this line if you do not want to see WiFi password printed
  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  // SSID to uppercase 
  ssid.toUpperCase();
  
  if (Router_SSID == "")
  {
    Serial.println("We haven't got any access point credentials, so get them now");   
     
    digitalWrite(PIN_LED, LED_ON); // Turn led on as we are in configuration mode.
    
    //it starts an access point 
    //and goes into a blocking loop awaiting configuration
    if (!ESP_wifiManager.startConfigPortal((const char *) ssid.c_str(), password)) 
      Serial.println("Not connected to WiFi but continuing anyway.");
    else 
      Serial.println("WiFi connected...yeey :)");    
  }

  digitalWrite(PIN_LED, LED_OFF); // Turn led off as we are not in configuration mode.
  
  #define WIFI_CONNECT_TIMEOUT        30000L
  #define WHILE_LOOP_DELAY            200L
  #define WHILE_LOOP_STEPS            (WIFI_CONNECT_TIMEOUT / ( 3 * WHILE_LOOP_DELAY ))
  
  startedAt = millis();
  
  while ( (WiFi.status() != WL_CONNECTED) && (millis() - startedAt < WIFI_CONNECT_TIMEOUT ) )
  {   
    WiFi.mode(WIFI_STA);
    WiFi.persistent (true);
    // We start by connecting to a WiFi network
  
    Serial.print("Connecting to ");
    Serial.println(Router_SSID);
  
    WiFi.begin(Router_SSID.c_str(), Router_Pass.c_str());

    int i = 0;
    while((!WiFi.status() || WiFi.status() >= WL_DISCONNECTED) && i++ < WHILE_LOOP_STEPS)
    {
      delay(WHILE_LOOP_DELAY);
    }    
  }

  Serial.print("After waiting ");
  Serial.print((millis()- startedAt) / 1000);
  Serial.print(" secs more in setup(), connection result is ");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("connected. Local IP: ");
    Serial.println(WiFi.localIP());
  }
  else
    Serial.println(ESP_wifiManager.getStatus(WiFi.status()));
}


void loop() 
{
  // is configuration portal requested?
  if ((digitalRead(TRIGGER_PIN) == LOW) || (digitalRead(TRIGGER_PIN2) == LOW)) 
  {
    Serial.println("\nConfiguration portal requested.");
    digitalWrite(PIN_LED, LED_ON); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.
    
    //Local intialization. Once its business is done, there is no need to keep it around
    ESP_WiFiManager ESP_wifiManager;
    
    //Check if there is stored WiFi router/password credentials.
    //If not found, device will remain in configuration mode until switched off via webserver.
    Serial.print("Opening configuration portal. ");
    Router_SSID = ESP_wifiManager.WiFi_SSID();
    if (Router_SSID != "")
    {
      ESP_wifiManager.setConfigPortalTimeout(60); //If no access point name has been previously entered disable timeout.
      Serial.println("Got stored Credentials. Timeout 60s");
    }
    else
      Serial.println("No stored Credentials. No timeout");
    
    //it starts an access point 
    //and goes into a blocking loop awaiting configuration
    if (!ESP_wifiManager.startConfigPortal((const char *) ssid.c_str(), password)) 
    {
      Serial.println("Not connected to WiFi but continuing anyway.");
    } 
    else 
    {
      //if you get here you have connected to the WiFi
      Serial.println("connected...yeey :)");
    }
    
    digitalWrite(PIN_LED, LED_OFF); // Turn led off as we are not in configuration mode.
  }
   
  // put your main code here, to run repeatedly
  check_status();

}
