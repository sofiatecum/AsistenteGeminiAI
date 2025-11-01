#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_SDA 21
#define I2C_SCL 22
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi
const char* ssid = "SGalaxy14";
const char* password = "12345678";
const String gemini_api_key = "AIzaSyAW_v5F-NE6II8F0ZtjFriOBOUIx_5SAEI";

// INMP441 I2S pins
#define I2S_WS 25   // Word Select (LRCL)
#define I2S_SD 22   // Serial Data
#define I2S_SCK 26  // Bit Clock

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed!");
    while (1);
  }

  displayMessage("Connecting WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  displayMessage("WiFi connected!");
  setupI2SMic();
  delay(500);
  displayMessage("Ready! Speak loud...");
}

void loop() {
  static unsigned long lastTrigger = 0;

  int micValue = readMicSample();
  Serial.println(micValue);

  if (micValue > 1500 && millis() - lastTrigger > 5000) {
    lastTrigger = millis();

    displayMessage("Detecting...");
    String response = askGemini("who are you.");
    displayMultilineText(response);
  }

  delay(100);
}

void displayMessage(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

void displayMultilineText(String text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  int maxCharsPerLine = 21;
  int lineHeight = 10;
  int y = 0;

  while (text.length() > 0 && y < SCREEN_HEIGHT) {
    String line = text.substring(0, maxCharsPerLine);
    text = text.substring(min((unsigned int)maxCharsPerLine, text.length()));
    display.setCursor(0, y);
    display.println(line);
    y += lineHeight;
  }

  display.display();
}

// Gemini API call
String askGemini(String prompt) {
  HTTPClient http;
  String endpoint = "https://generativelanguage.googleapis.com/v1/models/gemini-1.5-flash-002:generateContent?key=" + gemini_api_key;
  http.begin(endpoint);
  http.addHeader("Content-Type", "application/json");

  prompt.replace("\"", "\\\"");

  String requestBody = "{ \"contents\": [ { \"parts\": [ { \"text\": \"" + prompt + "\" } ] } ] }";
  int code = http.POST(requestBody);
  String reply = "";

  if (code == 200) {
    String payload = http.getString();
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      reply = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
      reply.replace("\\n", "\n");
    } else {
      Serial.println("JSON Error: " + String(error.c_str()));
      reply = "I am Gemini, your AI assistant.";
    }
  } else {
    Serial.println("Gemini API error: " + String(code));
    reply = "I am Gemini, your AI assistant.";
  }

  http.end();
  return reply;
}

// I2S microphone setup
void setupI2SMic() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

// Read sample from INMP441
int readMicSample() {
  int32_t sample = 0;
  size_t bytes_read;
  i2s_read(I2S_NUM_0, &sample, sizeof(sample), &bytes_read, portMAX_DELAY);
  return abs(sample >> 14); // Ajuste de escala y signo
}