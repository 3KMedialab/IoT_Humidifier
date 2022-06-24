#define ESP_DRD_USE_LITTLEFS    false
#define ESP_DRD_USE_SPIFFS      true
#define ESP_DRD_USE_EEPROM      false
// Number of seconds after reset during which a 
// subsequent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <UniversalTelegrambot.h>
#include <ArduinoJson.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <ESP_DoubleResetDetector.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

enum Commands { POWER_ON, POWER_OFF };
enum Modes { AUTO, MANUAL };
enum HumidifierStatus { HUMIDIFIER_OFF, HUMIDIFIER_ON };
enum Notifications { NOTIFICATIONS_ON, NOTIFICATIONS_OFF };

DoubleResetDetector* drd;

// WiFi and application preferences
Preferences preferences;
Preferences config;

// Smart plug IP Address
IPAddress smartPlugIPAddress;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Result of Wifi network scan 
String networks;

// Timer variables for WiFi connection
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
String chatId;

// Initialize Telegram BOT
String botToken = "";

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// Checks for new Telegram messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

Adafruit_BME280 bme;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Takes new temperature and humidity every 5 seconds.
int takeMeasuresDelay = 1000;
unsigned long lastTimeMeasuresTook;

uint8_t humidifierStatus;
uint8_t mode;
uint8_t applicationReady;
uint8_t notifications;

float temperature, humidity, tolerance = 0, threshold;

// 3K Medialab logo
static const uint8_t image_data_Image[1024] = {    
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0x07, 0xff, 0x80, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0x0f, 0xff, 0x80, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0x0f, 0xff, 0x80, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0x0f, 0xff, 0x80, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0x3f, 0xff, 0x80, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0xff, 0xff, 0x80, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfe, 0x7f, 0xfc, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x7f, 0xfc, 0x00, 0x0f, 0xff, 0xf8, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfe, 0x7f, 0xfc, 0x00, 0x0f, 0xff, 0xf8, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x7f, 0xfc, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0x3f, 0xff, 0x80, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0x0f, 0xff, 0x80, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0x07, 0xff, 0x80, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0x0f, 0xff, 0x80, 0x00, 
    0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x00, 0x00, 0x0f, 0xff, 0x80, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0xfe, 0x7f, 0xbf, 0xf7, 0xf8, 0x3e, 0x1f, 0x87, 0xc0, 0x1f, 0x83, 0xf8, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0x7f, 0xbf, 0xf7, 0xff, 0x3e, 0x1f, 0xc7, 0xe0, 0x1f, 0xc7, 0xff, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0xff, 0xbf, 0xf7, 0xff, 0xbe, 0x3f, 0xc7, 0xe0, 0x1f, 0xc7, 0xdf, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0xff, 0xbe, 0x07, 0xef, 0xbe, 0x3f, 0xc7, 0xe0, 0x3f, 0xe7, 0xdf, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0xff, 0xbf, 0xc7, 0xef, 0xbe, 0x3f, 0xe7, 0xe0, 0x3f, 0xe7, 0xfc, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0xff, 0xbf, 0xc7, 0xef, 0xbe, 0x7d, 0xe7, 0xe0, 0x7d, 0xe7, 0xff, 0x00, 0x00, 
    0x00, 0x00, 0xfb, 0xef, 0xbe, 0x07, 0xef, 0xbe, 0x7f, 0xf7, 0xe0, 0x7f, 0xf3, 0xdf, 0x00, 0x00, 
    0x00, 0x00, 0xfb, 0xef, 0xbf, 0xf7, 0xff, 0x3e, 0xfd, 0xf7, 0xfe, 0x7d, 0xf3, 0xff, 0x00, 0x00, 
    0x00, 0x00, 0xfb, 0xcf, 0xbf, 0xf7, 0xfe, 0x3e, 0xf9, 0xf7, 0xfe, 0xf8, 0xfb, 0xfe, 0x00, 0x00, 
    0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/**
 * @brief Send a command to the smart plug via Tasmota HTTP API. The IP aDdress of the smart plug is taken from the 
 * config preferences object.
 * 
 * @param command the command to be sent and defined in Commands enum.
 * @return 0 if the smart plug processed the command successfully. -1 otherwise. 
 */
int8_t sendCommandToDevice(Commands command)
{
  if (WiFi.status() == WL_CONNECTED)
  {    
    HTTPClient http;
    int8_t result = 0;   
    String url = "http://" + smartPlugIPAddress.toString() + "/cm?cmnd=";

    switch(command)
    {
      case POWER_ON:
        url += "Power%20On";         
        break;
      case POWER_OFF: 
        url += "Power%20Off";        
        break;
      default:
        Serial.print(F("Unknown command:" ));
        Serial.println(F(command));
        return -1;
    }
    
    http.begin(url);

    int httpCode = http.GET();
    
    //Check for the returning code
    if (httpCode > 0) 
    {
        String payload = http.getString();       
        StaticJsonDocument<50> response;
        DeserializationError error = deserializeJson(response, payload);

        // Test if parsing succeeds
        if (error) 
        {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          result = -1;          
        }
        else
        {
          switch(command)
          {
            case POWER_ON:
              response["POWER"] == "ON" ? result = 0 : result = -1;             
              break;
            case POWER_OFF: 
              response["POWER"] == "OFF" ? result = 0 : result = -1;        
              break;            
          }          
        }       
    }
  
    else
    {
      result = -1;
      Serial.println(F("Error on HTTP request"));      
    }    
    
    // Free resources
    http.end();    
    return result;
  }
  else
  {
    return -1;
  }
}

/**
 * @brief Get temperature and humidity data from BME280 sensor. If the mode is set to AUTO, then checks whether 
 * the smart plug should be enabled / disabled based on the humidity threshold and tolerance. A telegram message is sent everytime
 * the status of the humidifier is changed.
 * 
 */
void TakeMeasurements()
{
  bme.takeForcedMeasurement();
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();

  // clear display
  display.clearDisplay();
  
  // display temperature
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(temperature);
  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(2);
  display.print("C");
  
  // display humidity
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Humidity: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(humidity);
  display.print(" %"); 
  
  display.display(); 

  if (mode == AUTO)
  {
    // Switch on smart plug
    if (humidity < threshold - tolerance && humidifierStatus == HUMIDIFIER_OFF)
    {
      if (notifications == NOTIFICATIONS_ON)
      {
        bot.sendMessage(chatId, "Humidity: " + String(humidity) + F("%. Starting humidifier..."), "");
      }

      if (sendCommandToDevice(POWER_ON) == 0)
      {
        humidifierStatus = HUMIDIFIER_ON;

        if (notifications == NOTIFICATIONS_ON)
        {
          bot.sendMessage(chatId, F("Humidifier is on"), "");
        }
      }
      else
      {
        if (notifications == NOTIFICATIONS_ON)
        {
          bot.sendMessage(chatId, F("Error"), "");
        }
      }
    }

    // Switch off smart plug
    if (humidity >= threshold && humidifierStatus == HUMIDIFIER_ON)
    {
      // The first time humidity is >= threshold, then we can start using the tolerance. Otherwise, humidifier will not start if humidity is between threshold and threshold - tolerance      
      config.begin("appconfig", true);
      tolerance = config.getString("tolerance").toFloat();
      config.end();      

      if (notifications == NOTIFICATIONS_ON)
      {
        bot.sendMessage(chatId, "Humidity: " + String(humidity) + F("%. Switching off humidifier..."), "");
      }

      if (sendCommandToDevice(POWER_OFF) == 0)
      {
        humidifierStatus = HUMIDIFIER_OFF;
        
        if (notifications == NOTIFICATIONS_ON)
        {
          bot.sendMessage(chatId, F("Humidifier is off"), "");
        }
      }
      else
      {
        if (notifications == NOTIFICATIONS_ON)
        {
          bot.sendMessage(chatId, F("Error"), "");
        }
      }
    }
  }
}

/**
 * @brief Handle what happens when you receive new messages.
 * 
 */
void ProcessTelegramCommands()
{ 
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages)
  {
    for (int i = 0; i < numNewMessages; i++)
    {
      // Check the chat id of the requester
      String chat_id = String(bot.messages[i].chat_id);
      if (chat_id != chatId)
      {
        bot.sendMessage(chat_id, "Unauthorized user", "");
        continue;
      }
      
      String text = bot.messages[i].text;      
      String from_name = bot.messages[i].from_name;

      // Send the commands menu to the requester
      if (text == "/start")
      {
        String welcome = "";
        welcome += F("Welcome, ");
        welcome += from_name;
        welcome += F(".\n");
        welcome += F("Use the following commands to operate the humidifier.\n\n");
        welcome += F("/readings - get the temperature and humidity values. \n");
        welcome += F("/auto - enable the automatic mode. on and off commands are not available in this mode. \n");
        welcome += F("/manual - disable the automatic mode. Humidifier is managed using on and off commands. \n");
        welcome += F("/on - start the humidifier while in manual mode. \n");
        welcome += F("/off - stop the humidifier while in manual mode. \n");
        welcome += F("/status - to know if the humidifier is on or off. \n");
        welcome += F("/noton - enable telegram automatic notifications. \n");
        welcome += F("/notoff - disable telegram automatic notifications. \n");
        bot.sendMessage(chat_id, welcome, "");
      }

      // Send last values of temperature and humidity to the requester
      if (text == "/readings")
      {
        String message = "";

        message += F("Temperature: ");
        message += String(temperature);
        message += F(" ºC \n");
        message += F("Humidity: ");
        message += String(humidity);
        message += F(" % \n");

        bot.sendMessage(chat_id, message, "");
      }

      // Set the mode to AUTO
      if (text == "/auto")
      {
        mode = AUTO;
        bot.sendMessage(chat_id, F("Mode is set to automatic. On and Off commands are not available."), "");
      }

      // Set the mode to manual
      if (text == "/manual")
      {
        mode = MANUAL;
        bot.sendMessage(chat_id, F("Mode is set to manual. On and Off commands are available"), "");
      }

      // If mode is set to MANUAL, then switch off the smart plug
      if (text == "/off")
      {
        if (mode == MANUAL)
        {
          if (sendCommandToDevice(POWER_OFF) == 0)
          {
            humidifierStatus = HUMIDIFIER_OFF;
            bot.sendMessage(chat_id, F("Humidifier is off"), "");
          }
          else
          {
            bot.sendMessage(chat_id, F("Error"), "");
          }
        }
        else
        {
          bot.sendMessage(chat_id, F("Humidifier is in AUTO mode"), "");
        }
      }

      // If mode is set to MANUAL, then switch on the smart plug
      if (text == "/on")
      {
        if (mode == MANUAL)
        {
          if (sendCommandToDevice(POWER_ON) == 0)
          {
            humidifierStatus = HUMIDIFIER_ON;
            bot.sendMessage(chat_id, F("Humidifier is on"), "");
          }
          else
          {
            bot.sendMessage(chat_id, F("Error"), "");
          }
        }
        else
        {
          bot.sendMessage(chat_id, F("Humidifier is in AUTO mode"), "");
        }
      }

      // Send the status of the smart plug to the requester
      if (text == "/status")
      {
        humidifierStatus == HUMIDIFIER_ON ? bot.sendMessage(chat_id, F("Humidifier is on"), "") : bot.sendMessage(chat_id, F("Humidifier is off"), "");        
      }

      // Enable telegram notifications
      if (text == "/noton")
      {
        notifications = NOTIFICATIONS_ON;
        bot.sendMessage(chat_id, F("Notifications are enabled"), "");
      }

      // Disable telegram notifications
      if (text == "/notoff")
      {
        notifications = NOTIFICATIONS_OFF;
        bot.sendMessage(chat_id, F("Notifications are disabled"), "");
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  } 
}

/**
 * @brief Check if the application can be started. First, it takes from preferences objects the WiFi and app configuration
 * parameters. If cirtical parameters are missing, then app can't be started. Then, it tries to connect to the WiFi network
 * using ssid and password stored in the credentials.
 * 
 * The IP of the system is not dynamic. It is set to the value set in the localIP stored in the credentials preferences.
 * 
 * @return true if the application is ready
 * @return false if there are missing configuration parameters, or DNS addresses can't be retrieved or WiFi connection can't be created
 */
bool initApplication() 
{
  // WiFi configuration variables
  IPAddress localIP;
  IPAddress localGateway;
  IPAddress subnet(255, 255, 0, 0);
  IPAddress dns1;
  IPAddress dns2;

  preferences.begin("credentials", true);
  config.begin("appconfig", true);

  // Wifi configuration
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("password", "");
  String ip = preferences.getString("ip","");
  String gateway = preferences.getString("gateway","");

  // Smart plug configuration
  String smartPlugIpParameter = config.getString("smartplugip","");

  // Humidity configuration
  String thresholdParameter = config.getString("threshold","");

  // Telegram configuration
  String chatIdParameter = config.getString("chatId","");
  String botTokenParameter = config.getString("botToken","");

  preferences.end();
  config.end();

  // Check if the WiFi and app configuration is stored in credentials and config Preferences objects
  if(ssid == "" || pass == "" || ip == "" || gateway == "")
  {
    Serial.println(F("Missing Wifi data"));
    return false;
  }

  if(smartPlugIpParameter == "")
  {
    Serial.println(F("Missing Smart Plug IP Address"));
    return false;
  }

  if(thresholdParameter == "")
  {
    Serial.println(F("Missing humidity threshold"));
    return false;
  }

  if(chatIdParameter == "" || botTokenParameter == "")
  {
    Serial.println(F("Missing telegram configuration"));
    return false;
  }
  
  // If all required WiFi and app configuration is ready, then connect to WiFi to retrieve dns addresses for further WiFi configuration
  WiFi.mode(WIFI_STA);
  
  // Get dns ip addresses  
  WiFi.begin(ssid.c_str(), pass.c_str());

  Serial.println(F("Getting DNS addresses..."));

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    
    if (currentMillis - previousMillis >= interval) 
    {
      Serial.println(F("Failed to connect."));
      return false;
    }
  }

  // Set the values for creation the WiFi connection using static IP
  dns1 = WiFi.dnsIP(0);
  dns2 = WiFi.dnsIP(1);    
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  // IP Address of the smart plug took from config Preferences object
  smartPlugIPAddress.fromString(smartPlugIpParameter.c_str());

  // Humidity threshold to be used took from config Preferences object
  threshold = thresholdParameter.toFloat();
  
  // Telegram parameters took from config Preferences object
  chatId = chatIdParameter;
  botToken = botTokenParameter;
  bot.updateToken(botToken);

  WiFi.disconnect();
  delay(1);

  // Create WiFi connection using static IP
  if (!WiFi.config(localIP, localGateway, subnet, dns1, dns2))
  {
    Serial.println(F("STA Failed to configure"));
    return false;
  }

  WiFi.begin(ssid.c_str(), pass.c_str());  
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  
  Serial.println(F("Connecting to WiFi..."));

  currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    
    if (currentMillis - previousMillis >= interval) 
    {
      Serial.println(F("Failed to connect."));
      return false;
    }
  }

  Serial.println(WiFi.localIP());

  return true;
}

/**
 * @brief Complete the the WiFi / App configuration web page with the stored configuration.
 * 
 * @param var the parameter received from the web form.
 * @return String HTML data with the stored parameters in Preferences objects 
 */
String processor(const String& var) 
{
  if(var == "NETWORKS") 
  {
    return networks;
  }

  if(var == "SSID") 
  {
    preferences.begin("credentials", true);
    String ssid = preferences.getString("ssid", ""); 
    preferences.end();
  
    return ssid;
  }

  if(var == "PASS") 
  {
    preferences.begin("credentials", true);
    String pass = preferences.getString("password", "");
    preferences.end();

    return pass;    
  }

  if(var == "PLUG_ADDRESS") 
  {
    config.begin("appconfig", true);
    String smartPlugIp = config.getString("smartplugip","");
    config.end();

    return smartPlugIp;
  }

  if(var == "HUMIDITY_THRESHOLD") 
  {
    config.begin("appconfig", true);
    String threshold = config.getString("threshold","");
    config.end();

    return threshold;
  }

  if(var == "HUMIDITY_TOLERANCE") 
  {
    config.begin("appconfig", true);
    String tolerance = config.getString("tolerance","");
    config.end();

    return tolerance;
  }

  if(var == "CHAT_ID") 
  {
    config.begin("appconfig", true);
    String chat_Id = config.getString("chatId","");
    config.end();

    return chat_Id;
  }

  if(var == "BOT_TOKEN") 
  {
    config.begin("appconfig", true);
    String bot_Token = config.getString("botToken","");
    config.end();

    return bot_Token;
  }

  return String();
}

void setup()
{
  Serial.begin(115200);
  
  if (!SPIFFS.begin())
  {
    Serial.println(F("SPIFFS Mount Failed"));
    return;
  }
  
  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);  
 
  if (!drd->detectDoubleReset() && initApplication())
  {
    applicationReady = true;    
    
    // Init BME280 sensor
    if (!bme.begin(0x76))
    {
      Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
      return;
    }   

    // Humidifier is switched off on system start
    humidifierStatus = HUMIDIFIER_OFF;

    // Notifications are enabled by default
    notifications = NOTIFICATIONS_ON;

    mode = AUTO;

    // Humidity Sensing Scenario
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1,   // temperature
                    Adafruit_BME280::SAMPLING_NONE, // pressure
                    Adafruit_BME280::SAMPLING_X1,   // humidity
                    Adafruit_BME280::FILTER_OFF );

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
      Serial.println(F("Display could not be initialized."));
      return;
    }    

    // Clear the buffer of the display
    display.clearDisplay();
    
    // Draw logo on the screen
    display.drawBitmap(0, 0, image_data_Image, 128, 64, 1);
    display.display();

    delay(3000);
    
    display.clearDisplay();       
    display.setTextColor(WHITE);
  }

  // Enable access point for system configuration
  else
  { 
    applicationReady = false;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();

    networks += F("<div id=\"networks\">");
    
    if (n == 0) 
    {
        networks += F("No networks found");
    } 
    
    else 
    {
      networks += F("<ul>");
      
      for (int i = 0; i < n; ++i) 
      {
        networks += F("<li>");

        // Print SSID and RSSI for each network found
        networks += F(WiFi.SSID(i).c_str());
        networks += F(" (");
        networks += F(String(WiFi.RSSI(i)).c_str());
        networks += F(")");
        networks += F((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
        delay(10);
        
        networks += F("</li>");
      }

      networks += F("</ul>");
    }

    networks += F("</div>");
    WiFi.disconnect();
    
    // Enable AP to ask for Wifi credentials 
    WiFi.softAP("HumiConfig");

    IPAddress IP = WiFi.softAPIP();
    Serial.print(F("AP IP address: "));
    Serial.println(IP);

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
    { 
        request->send(SPIFFS, "/wifimanager.html", "text/html", false, processor);
    });

    server.onNotFound([](AsyncWebServerRequest *request){request->send(404);});

    server.serveStatic("/", SPIFFS, "/");

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
    {
      int params = request->params();

      preferences.begin("credentials", false);
      config.begin("appconfig", false);

      // process form input data and store new Wifi credentials and application configuration
      for(int i = 0; i <params; i++)
      {
        AsyncWebParameter* p = request->getParam(i);

        if(p->isPost())
        {      
          // HTTP POST ssid value
          if (p->name() == "ssid") 
          { 
            preferences.putString("ssid", p->value());         
          }

          // HTTP POST password value
          if (p->name() == "pass")
          {
            preferences.putString("password", p->value());            
          }

          // HTTP POST ip value
          if (p->name() == "ip")
          { 
            preferences.putString("ip", p->value()); 
          }

          // HTTP POST gateway value
          if (p->name() == "gateway")
          {
            preferences.putString("gateway", p->value()); 
          }

          // HTTP POST smart plug IP Address value
          if (p->name() == "plug_address")
          {
            config.putString("smartplugip", p->value()); 
          }

          // HTTP POST humidity threshold value
          if (p->name() == "humidity_threshold")
          {
             config.putString("threshold", p->value()); 
          }

          // HTTP POST humidity tolerance value
          if (p->name() == "humidity_tolerance")
          {
            config.putString("tolerance", p->value()); 
          }

          // HTTP POST telegram chat id
          if (p->name() == "chat_id")
          {
            config.putString("chatId", p->value()); 
          }

          // HTTP POST telegram bot token
          if (p->name() == "bot_token")
          {
            config.putString("botToken", p->value()); 
          }
        }
      }
      request->send(200, "text/plain", "Done. System will restart");
      
      preferences.end();
      config.end();
      
      delay(3000);
      ESP.restart(); 
    });

    server.begin();
  }  
}

void loop()
{
  if (applicationReady)
  {
    if (millis() > lastTimeBotRan + botRequestDelay)
    {
      ProcessTelegramCommands();
      lastTimeBotRan = millis(); 
    }

    if (millis() > lastTimeMeasuresTook + takeMeasuresDelay)
    {
      TakeMeasurements();
      lastTimeMeasuresTook = millis();
    }
  }
  drd->loop();
}
