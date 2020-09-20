//###################### Vincent HUYNEN ########################/
//################## vincent.huynen@gmail.com ##################/
//######################### AUGUST 2020 ########################/
//######################## Version 1.0.0 #######################/

/*
  Mailbox Notifier
*/

// Import external base64 library C
extern "C" {
#include "crypto/base64.h"
}

#include <WiFi.h>
#include <HTTPClient.h>
#define ARDUINOJSON_ENABLE_COMMENTS 1
#include <ArduinoJson.h>
#include "SD_MMC.h"
#include <esp_bt.h>
#include <Update.h>
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include <esp_camera.h>
#include "camera_pins.h"

// Variables persisted into RTC memory
RTC_DATA_ATTR int wake_count = 0;

// Filename properties
char* filename_properties = "/mailbox.cfg";

// Path for the Base64 temporary file
String pathFileBase64 = "/tmp/encode.b64";

// WiFi Properties
boolean wifi_ip_static = false;
char* wifi_ip = (char *) malloc(20);
char* wifi_gateway = (char *) malloc(20);
char* wifi_subnet = (char *) malloc(20);
char* wifi_dns_ip_primary = (char *) malloc(20);
char* wifi_dns_ip_secondary = (char *) malloc(20);
char* wifi_ssid = (char *) malloc(50);
char* wifi_security_code = (char *) malloc(50);

// OAuth Credentials for Gmail API
String bearer; // Access Token retreived from OAuth Google API
char* gmail_credentials_refresh_token = (char *) malloc(150);
char* gmail_credentials_client_id  = (char *) malloc(100);
char* gmail_credentials_client_secret  = (char *) malloc(50);

// Mail propeties
boolean email = false;
char* email_from  = (char *) malloc(50);
char* email_to  = (char *) malloc(100);
char* email_subject_door  = (char *) malloc(100);
char* email_body_door = (char *) malloc(150);
char* email_subject_flip_door = (char *) malloc(100);
char* email_body_flip_door = (char *) malloc(150);

//SMS properties
boolean sms = false;
char* url  = (char *) malloc(200);
char* sms_body_door  = (char *) malloc(100);
char* sms_body_flip_door  = (char *) malloc(100);

// Control properties
long retry = 1;
long overtime_open_door = 10000;

// Pin which handle the wake up
int pinDoor = 12;
int pinWakeUp = 0;
// Pin which is used when you fetch your mail
int pinFetchMail = 16;

//  Bit mask of GPIO numbers which will cause wakeup - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html
#define BUTTON_PIN_BITMASK 0x1000 // 2^12 in hex

//Stores the camera configuration parameters
camera_config_t config;

void setup()
{

  Serial.begin(115200);
 Serial.println("test firmaware 2...");
  // Initialize SD library 1 bit
  while (!SD_MMC.begin("/sdcard", true)) {
    Serial.println(F("Failed to initialize SD library"));
    delay(1000);
  }
  // Disable state flash light pin after wake up
  gpio_hold_dis(GPIO_NUM_4);
  // Flash Light Pin
  pinMode (GPIO_NUM_4, OUTPUT);
 // digitalWrite(GPIO_NUM_4, LOW);
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  // Only for the bootstrap, update the firmware from SD Card if a new one exists
  if (wake_count == 0) {
    if (updateFirmware()) {
      Serial.println("The firmware has been updated...");
      // Turn on built-in Led after update firmware successfully
      pinMode (GPIO_NUM_33, OUTPUT);
      digitalWrite(GPIO_NUM_33, LOW);
      delay(3000);
      Serial.println("ESP32 Rebooting...");
      ESP.restart();
    }
  }

  // Press button when you fetch your mail
  pinMode(GPIO_NUM_16, INPUT);
  // Activate de pullup internal resistor
  if (gpio_pullup_en(GPIO_NUM_16) == ESP_OK) {
    Serial.println("Success GPIO_NUM_16 gpio_pullup_en");
  }

  Serial.println(digitalRead(GPIO_NUM_16));
  // Init pin wake up
  pinMode(GPIO_NUM_12, INPUT);
  // Activate de pulldown internal resistor
  if (gpio_pulldown_en(GPIO_NUM_12) == ESP_OK) {
    Serial.println("Success GPIO_NUM_12 gpio_pulldown_en");
  }
  Serial.println(digitalRead(GPIO_NUM_12));

  // Init properties from SD Card
  initProperties(filename_properties);
  // Init the pin which occur the waku up of the nodeMCU
  setGPIOWakeUp();

  //  // Starting the WiFi
  //  connectToWiFi(wifi_ssid, wifi_security_code, wifi_ip_static, wifi_ip , wifi_gateway, wifi_subnet, wifi_dns_ip_primary, wifi_dns_ip_secondary);
  //    if (retrieved_access_token_gmail(gmail_credentials_refresh_token, gmail_credentials_client_id, gmail_credentials_client_secret)) {
  //      Serial.println("Access Token has been retreived...");
  //    }
  //
  //
  // Send mail with attachemet by Gmail API
  //    if (sendMail(email_from, email_to, email_subject_flip_door, email_body_flip_door, "/tmp/mailbox.png")) {
  //      Serial.println("Send e-mail with attachement OK...");
  //    }

  //  Serial.println("Sending mail...");
  //  if (retrieved_access_token_gmail(gmail_credentials_refresh_token, gmail_credentials_client_id, gmail_credentials_client_secret)) {
  //    Serial.println("Access Token has been retreived...");
  //  }
  //  if (sendMail(email_from, email_to, email_subject_flip_door, email_body_flip_door)) {
  //    Serial.println("Mail are going...");
  //  }

  //  if (sendMail(email_from, email_to, email_subject_door, email_body_door)) {
  //    Serial.println("Mail are going...");
  //  }

  //  if (sendSMS(url, sms_body_door)) {
  //    Serial.println("SMS sms_body_door have been sended...");
  //  }
  //    if (sendSMS(url, sms_body_flip_door)) {
  //      Serial.println("SMS sms_body_flip_door have been sended...");
  //    }


 //printAllProperties();
  wake_count++;
  Serial.println("WakeUp Pin" + (String)pinWakeUp);
}

void loop()
{

  // When you turn on the controller a first time
  if (wake_count == 1) {
    Serial.println(digitalRead(GPIO_NUM_12));
    if (digitalRead(GPIO_NUM_12) == 0) {
      // Only if all doors are closed
      goToDeepSleep();
    } else if (millis() > overtime_open_door) {
      // Connect to WiFI
      connectToWiFi(wifi_ssid, wifi_security_code, wifi_ip_static, wifi_ip , wifi_gateway, wifi_subnet, wifi_dns_ip_primary, wifi_dns_ip_secondary);
      // Retreived Access Token
      retrieved_access_token_gmail(gmail_credentials_refresh_token, gmail_credentials_client_id, gmail_credentials_client_secret);
      // Send error mail by Gmail API
      sendMail(email_from, email_to, (char *) "[MAILBOX] BOOT ERROR", (char *) "Doors have been opened too long time when you have turned on the ESP32-CAM.\nReboot the ESP32-CAM for reinitialize it !");
      // Going to sleep until reset or restart
      goToDeepSleepError();
    }
  }

  // Ignore wake up when you fetch your mail, open the flip door first before open the principal door
  if (pinWakeUp == pinDoor) {
    if (digitalRead(GPIO_NUM_16) == 0) {
      while (digitalRead(GPIO_NUM_12) == 1)  {
        // Turn on the build-in Led when you fetch you mail
        pinMode (GPIO_NUM_33, OUTPUT);
        digitalWrite(GPIO_NUM_33, LOW);
        if (millis() > overtime_open_door) {
          // Connect to WiFI
          connectToWiFi(wifi_ssid, wifi_security_code, wifi_ip_static, wifi_ip , wifi_gateway, wifi_subnet, wifi_dns_ip_primary, wifi_dns_ip_secondary);
          // Retreived Access Token
          retrieved_access_token_gmail(gmail_credentials_refresh_token, gmail_credentials_client_id, gmail_credentials_client_secret);
          // Send error mail by Gmail API
          sendMail(email_from, email_to, (char *) "[MAILBOX] FETCH MAIL ERROR", (char *) "Doors have been opened too long time when you have fetched your mail.\nReboot the ESP32-CAM for reinitialize it !");
          // Going to sleep until reset or restart
          goToDeepSleepError();
        }
        // Turn off the build-in Led
        pinMode (GPIO_NUM_33, HIGH);
      }
      // Everythings is OK, you can sleep quietly
      goToDeepSleep();
    }
  }

  // You have got mail ! Let's go...
  if (pinWakeUp == pinDoor) {
    if (digitalRead(GPIO_NUM_12) == 0) {
      // Connect to WiFI
      connectToWiFi(wifi_ssid, wifi_security_code, wifi_ip_static, wifi_ip , wifi_gateway, wifi_subnet, wifi_dns_ip_primary, wifi_dns_ip_secondary);
      // Retreived Access Token
      retrieved_access_token_gmail(gmail_credentials_refresh_token, gmail_credentials_client_id, gmail_credentials_client_secret);

      //Path where new picture will be saved in SD Card
      String path = "/camera/img" + String(wake_count) + ".jpg";
      takeAndSavePicture(path);
      // Send mail with picture attachment
      sendMail(email_from, email_to, email_subject_door, email_body_door, path);

      // Everythings is OK, you can sleep quietly
      goToDeepSleep();
    }
    if (millis() > overtime_open_door) {
      // Connect to WiFI
      connectToWiFi(wifi_ssid, wifi_security_code, wifi_ip_static, wifi_ip , wifi_gateway, wifi_subnet, wifi_dns_ip_primary, wifi_dns_ip_secondary);
      // Retreived Access Token
      retrieved_access_token_gmail(gmail_credentials_refresh_token, gmail_credentials_client_id, gmail_credentials_client_secret);
      // Send error mail by Gmail API
      sendMail(email_from, email_to, (char *) "[MAILBOX] POSTMAN ERROR", (char *) "Doors have been opened too long time by the postman.\nReboot the ESP32-CAM for reinitialize it !");
      // Something wring, going to sleep until reset or restart
      goToDeepSleepError();
    }
  }
}