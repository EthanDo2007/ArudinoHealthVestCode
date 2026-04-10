/* Nano ESP32 - Health Monitor Fix */

#define CUSTOM_SETTINGS
#define INCLUDE_TERMINAL_MODULE
#include <DabbleESP32.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

const int pulsePin = A1;
#define RX_GPIO 5  
#define TX_GPIO 4  

// Adjusted threshold - check Serial Plotter if these need tuning
const int threshold = 2100;
const int resetLevel = 1900; // Lowered slightly to ensure reset
const int lockout = 350;
unsigned long lastBeatTime = 0;
bool isBeatLogged = false;

const int avgWindowSize = 10;
float beatArray[avgWindowSize];
int beatIndex = 0;
float filteredBPM = 0;

TinyGPSPlus gps;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

unsigned long lastReportTime = 0;
const unsigned long reportInterval = 5000; 

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, RX_GPIO, TX_GPIO); 
  
  Wire.begin(A4, A5); 
  Wire.setClock(100000); 
  
  analogReadResolution(12);
  Dabble.begin("NanoESP32_Health");
  
  if (!mlx.begin()) {
    Serial.println("MLX90614 not found!");
  }

  for(int i = 0; i < avgWindowSize; i++) beatArray[i] = 0;
}

void loop() {
  Dabble.processInput();
  unsigned long currentTime = millis();

  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  // --- PART B: BPM DETECTION ---
  int rawValue = analogRead(pulsePin);
  
  // Optional: Uncomment the line below to "see" your heartbeat in Serial Plotter
  // Serial.println(rawValue); 

  if (rawValue > threshold && !isBeatLogged && (currentTime - lastBeatTime > lockout)) {
    
    // FIX: If this is the very first beat detected, just mark the time and don't calculate
    if (lastBeatTime == 0) {
      lastBeatTime = currentTime;
      Serial.println("First beat detected! Starting timer...");
    } else {
      unsigned long beatInterval = currentTime - lastBeatTime;
      float instantBPM = 60000.0 / beatInterval;

      if (instantBPM > 45 && instantBPM < 180) {
        beatArray[beatIndex] = instantBPM;
        beatIndex = (beatIndex + 1) % avgWindowSize;
        Serial.print("Beat Detected! BPM: "); Serial.println(instantBPM);
      }
      lastBeatTime = currentTime;
    }
    isBeatLogged = true;
  }

  if (rawValue < resetLevel) {
    isBeatLogged = false;
  }

  // --- PART C: REPORTING ---
  if (currentTime - lastReportTime >= reportInterval) {
    float sum = 0;
    int count = 0;
    for(int i = 0; i < avgWindowSize; i++) {
      if(beatArray[i] > 0) { 
        sum += beatArray[i]; 
        count++; 
      }
    }
    if (count > 0) filteredBPM = sum / count;

    float objC = mlx.readObjectTempC();
    
    Terminal.println("--- HEALTH REPORT ---");
    
    if (gps.location.isValid()) {
      Terminal.println("Loc: " + String(gps.location.lat(), 4) + ", " + String(gps.location.lng(), 4));
    } else {
      Terminal.println("GPS: Searching...");
    }

    if (count > 0) {
      Terminal.println("Heart Rate: " + String((int)filteredBPM/(1.7)) + " BPM");
    } else {
      Terminal.println("Heart Rate: Calculating...");
    }

    if (isnan(objC)) {
      Terminal.println("Temp: Sensor Error");
    } else {
      float objF = (objC * 9.0 / 5.0) + 32.0;
      Terminal.println("Temp: " + String(objC, 1) + "C / " + String(objF, 1) + "F");
    }

    Terminal.println("--------------------");
    lastReportTime = currentTime;
  }
  delay(2);
}
