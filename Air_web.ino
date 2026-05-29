/*
 * Smart Air Quality Monitoring System using ESP32
 * Features: Web Dashboard (HTML/CSS/JS), DHT11, MQ2, MQ5, MQ7, I2C LCD, Buzzer, LEDs
 * Author: Gemini IoT Developer
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <ArduinoJson.h>

// ======================== CONFIGURATION ========================
const char* ssid = "S";          // <--- ENTER YOUR WIFI NAME
const char* password = "YOUR_WIFI_PASSWORD";  // <--- ENTER YOUR WIFI PASSWORD

#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ2_PIN 34
#define MQ5_PIN 35
#define MQ7_PIN 32
#define BUZZER 25
#define GREEN_LED 26
#define BLUE_LED 27 

int threshold = 1000; // Gas detection threshold
// ===============================================================

// Objects
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
AsyncWebServer server(80);

// Global Variables
float temperature = 0, humidity = 0;
int val_mq2 = 0, val_mq5 = 0, val_mq7 = 0;
bool isAlert = false;

// HTML/CSS/JS Content (Stored in PROGMEM to save RAM)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Air Monitor | Pro Dash</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css" rel="stylesheet">
    <style>
        :root {
            --bg-dark: #0a0e14;
            --card-bg: #161b22;
            --neon-green: #00ff9d;
            --neon-red: #ff3e3e;
            --neon-blue: #00d4ff;
            --text-gray: #8b949e;
            --text-white: #f0f6fc;
        }

        * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }
        body { background: var(--bg-dark); color: var(--text-white); overflow-x: hidden; }

        /* Loading Screen */
        #loader {
            position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: var(--bg-dark);
            display: flex; flex-direction: column; justify-content: center; align-items: center; z-index: 1000;
            transition: opacity 0.5s;
        }
        .spinner { width: 50px; height: 50px; border: 5px solid var(--card-bg); border-top: 5px solid var(--neon-green); border-radius: 50%; animation: spin 1s linear infinite; }
        @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }

        /* Header */
        header { 
            padding: 20px; background: rgba(22, 27, 34, 0.8); backdrop-filter: blur(10px);
            border-bottom: 1px solid #30363d; position: sticky; top: 0; z-index: 100;
            display: flex; justify-content: space-between; align-items: center;
        }
        .status-badge { padding: 8px 16px; border-radius: 20px; font-weight: bold; font-size: 0.8rem; display: flex; align-items: center; gap: 8px; }
        .safe { background: rgba(0, 255, 157, 0.1); color: var(--neon-green); border: 1px solid var(--neon-green); }
        .danger { background: rgba(255, 62, 62, 0.1); color: var(--neon-red); border: 1px solid var(--neon-red); animation: pulse 1.5s infinite; }
        @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.5; } 100% { opacity: 1; } }

        /* Layout */
        .container { padding: 20px; max-width: 1400px; margin: 0 auto; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; margin-bottom: 20px; }

        /* Cards */
        .card { background: var(--card-bg); border-radius: 16px; padding: 20px; border: 1px solid #30363d; transition: transform 0.3s ease; }
        .card:hover { transform: translateY(-5px); border-color: #444c56; }
        .card-header { color: var(--text-gray); font-size: 0.9rem; margin-bottom: 15px; display: flex; align-items: center; gap: 10px; }
        .card-value { font-size: 2.2rem; font-weight: 700; color: var(--text-white); }
        .unit { font-size: 1rem; color: var(--text-gray); margin-left: 5px; }

        /* Charts Section */
        .chart-container { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        @media (max-width: 900px) { .chart-container { grid-template-columns: 1fr; } }
        .chart-card { background: var(--card-bg); border-radius: 16px; padding: 20px; border: 1px solid #30363d; height: 350px; }

        /* Info Section */
        .info-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; margin-top: 20px; }
        .info-card { background: #0d1117; padding: 20px; border-radius: 12px; border-left: 4px solid var(--neon-blue); }
        .info-card h4 { color: var(--neon-blue); margin-bottom: 10px; }
        .info-card p { font-size: 0.85rem; color: var(--text-gray); line-height: 1.5; }

        /* Log Table */
        .log-section { margin-top: 20px; overflow-x: auto; }
        table { width: 100%; border-collapse: collapse; background: var(--card-bg); border-radius: 12px; overflow: hidden; }
        th { background: #21262d; padding: 12px; text-align: left; color: var(--text-gray); font-size: 0.8rem; text-transform: uppercase; }
        td { padding: 12px; border-bottom: 1px solid #30363d; font-size: 0.9rem; }

        /* Alert Banner */
        #alert-banner {
            display: none; background: var(--neon-red); color: white; text-align: center;
            padding: 15px; font-weight: bold; position: fixed; bottom: 0; left: 0; width: 100%; z-index: 1000;
            box-shadow: 0 -5px 20px rgba(255, 62, 62, 0.4);
        }
    </style>
</head>
<body>
    <div id="loader">
        <div class="spinner"></div>
        <p style="margin-top:20px; color:var(--text-gray)">Connecting to Smart Air System...</p>
    </div>

    <header>
        <div>
            <h2 style="letter-spacing: 1px;"><i class="fas fa-wind" style="color:var(--neon-blue)"></i> SMART AIR MONITOR</h2>
            <p id="timestamp" style="font-size: 0.7rem; color:var(--text-gray); margin-top: 4px;">Syncing...</p>
        </div>
        <div id="status-box" class="status-badge safe">
            <i class="fas fa-check-circle"></i> AIR QUALITY NORMAL
        </div>
    </header>

    <div class="container">
        <div class="grid">
            <div class="card">
                <div class="card-header"><i class="fas fa-thermometer-half"></i> TEMPERATURE</div>
                <div class="card-value"><span id="temp">--</span><span class="unit">°C</span></div>
            </div>
            <div class="card">
                <div class="card-header"><i class="fas fa-tint"></i> HUMIDITY</div>
                <div class="card-value"><span id="hum">--</span><span class="unit">%</span></div>
            </div>
            <div class="card">
                <div class="card-header"><i class="fas fa-flask"></i> MQ2 (SMOKE/LPG)</div>
                <div class="card-value"><span id="mq2">--</span></div>
            </div>
            <div class="card">
                <div class="card-header"><i class="fas fa-vial"></i> MQ5 (NATURAL GAS)</div>
                <div class="card-value"><span id="mq5">--</span></div>
            </div>
            <div class="card">
                <div class="card-header"><i class="fas fa-radiation"></i> MQ7 (CARBON MONOXIDE)</div>
                <div class="card-value"><span id="mq7">--</span></div>
            </div>
        </div>

        <div class="chart-container">
            <div class="chart-card"><canvas id="tempHumChart"></canvas></div>
            <div class="chart-card"><canvas id="gasChart"></canvas></div>
        </div>

        <h3 style="margin: 30px 0 15px 0;">Sensor Intelligence</h3>
        <div class="info-grid">
            <div class="info-card">
                <h4>MQ2 Sensor</h4>
                <p>Detects LPG, Smoke, Methane, and Butane. Critical for fire safety and industrial leak monitoring.</p>
            </div>
            <div class="info-card">
                <h4>MQ5 Sensor</h4>
                <p>Sensitive to Natural Gas and LPG. Optimized for kitchen monitoring and domestic gas leak detection.</p>
            </div>
            <div class="info-card">
                <h4>MQ7 Sensor</h4>
                <p>Detects Carbon Monoxide (CO). Highly vital as CO is an odorless, colorless toxic gas.</p>
            </div>
        </div>

        <h3 style="margin: 30px 0 15px 0;">Live History Log</h3>
        <div class="log-section">
            <table>
                <thead>
                    <tr>
                        <th>Timestamp</th>
                        <th>Temp</th>
                        <th>Hum</th>
                        <th>MQ2</th>
                        <th>MQ5</th>
                        <th>MQ7</th>
                        <th>Status</th>
                    </tr>
                </thead>
                <tbody id="log-body"></tbody>
            </table>
        </div>
    </div>

    <div id="alert-banner">
        <i class="fas fa-exclamation-triangle"></i> DANGER: TOXIC GAS DETECTED! EVACUATE AREA
    </div>

    <script>
        const maxDataPoints = 20;
        let chartLabels = [];
        let tempData = [], humData = [];
        let mq2Data = [], mq5Data = [], mq7Data = [];

        // Initialize Charts
        const ctx1 = document.getElementById('tempHumChart').getContext('2d');
        const tempHumChart = new Chart(ctx1, {
            type: 'line',
            data: {
                labels: chartLabels,
                datasets: [
                    { label: 'Temp (°C)', data: tempData, borderColor: '#ff3e3e', tension: 0.4, fill: false },
                    { label: 'Hum (%)', data: humData, borderColor: '#00d4ff', tension: 0.4, fill: false }
                ]
            },
            options: { responsive: true, maintainAspectRatio: false, scales: { y: { grid: { color: '#30363d' } } } }
        });

        const ctx2 = document.getElementById('gasChart').getContext('2d');
        const gasChart = new Chart(ctx2, {
            type: 'bar',
            data: {
                labels: ['MQ2', 'MQ5', 'MQ7'],
                datasets: [{ label: 'Gas Concentration', data: [0,0,0], backgroundColor: ['#00ff9d', '#00d4ff', '#ff3e3e'] }]
            },
            options: { responsive: true, maintainAspectRatio: false }
        });

        async function fetchData() {
            try {
                const response = await fetch('/data');
                const data = await response.json();
                
                document.getElementById('loader').style.opacity = '0';
                setTimeout(() => document.getElementById('loader').style.display = 'none', 500);

                const now = new Date().toLocaleTimeString();
                document.getElementById('timestamp').innerText = "Last Updated: " + new Date().toLocaleString();

                // Update UI
                document.getElementById('temp').innerText = data.temperature;
                document.getElementById('hum').innerText = data.humidity;
                document.getElementById('mq2').innerText = data.mq2;
                document.getElementById('mq5').innerText = data.mq5;
                document.getElementById('mq7').innerText = data.mq7;

                // Handle Alerts
                const statusBox = document.getElementById('status-box');
                const alertBanner = document.getElementById('alert-banner');
                const isDanger = data.mq2 > 1000 || data.mq5 > 1000 || data.mq7 > 1000;

                if (isDanger) {
                    statusBox.className = "status-badge danger";
                    statusBox.innerHTML = '<i class="fas fa-biohazard"></i> DANGER DETECTED';
                    alertBanner.style.display = 'block';
                } else {
                    statusBox.className = "status-badge safe";
                    statusBox.innerHTML = '<i class="fas fa-check-circle"></i> AIR QUALITY NORMAL';
                    alertBanner.style.display = 'none';
                }

                // Update Charts
                if (chartLabels.length >= maxDataPoints) {
                    chartLabels.shift(); tempData.shift(); humData.shift();
                }
                chartLabels.push(now);
                tempData.push(data.temperature);
                humData.push(data.humidity);
                tempHumChart.update();

                gasChart.data.datasets[0].data = [data.mq2, data.mq5, data.mq7];
                gasChart.update();

                // Update Log
                const logBody = document.getElementById('log-body');
                const row = `<tr>
                    <td>${now}</td>
                    <td>${data.temperature}°C</td>
                    <td>${data.humidity}%</td>
                    <td>${data.mq2}</td>
                    <td>${data.mq5}</td>
                    <td>${data.mq7}</td>
                    <td style="color:${isDanger?'#ff3e3e':'#00ff9d'}">${isDanger?'DANGER':'SAFE'}</td>
                </tr>`;
                logBody.insertAdjacentHTML('afterbegin', row);
                if (logBody.children.length > 10) logBody.lastElementChild.remove();

            } catch (e) { console.error("Error fetching data", e); }
        }

        setInterval(fetchData, 2000);
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  // Pin setup
  pinMode(BUZZER, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  // Initial states
  digitalWrite(BUZZER, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BLUE_LED, LOW);

  // Start sensors
  dht.begin();

  // LCD setup
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Air System");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");

  // WiFi Connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // IP Display Logic (Requirement: 7 seconds)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP Address:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println(WiFi.localIP());
  delay(7000); // Wait 7 seconds as requested

  // Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<200> json;
    json["temperature"] = isnan(temperature) ? 0 : temperature;
    json["humidity"] = isnan(humidity) ? 0 : humidity;
    json["mq2"] = val_mq2;
    json["mq5"] = val_mq5;
    json["mq7"] = val_mq7;
    json["alert"] = isAlert;
    
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
  });

  server.begin();
}

void loop() {
  // Read Sensors
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  val_mq2 = analogRead(MQ2_PIN);
  val_mq5 = analogRead(MQ5_PIN);
  val_mq7 = analogRead(MQ7_PIN);

  // Logic: Check if any sensor exceeds threshold
  isAlert = (val_mq2 > threshold || val_mq5 > threshold || val_mq7 > threshold);

  if (isAlert) {
    // ALERT MODE
    digitalWrite(BUZZER, HIGH);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BLUE_LED, HIGH);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("!!! GAS ALERT !!!");
    lcd.setCursor(0, 1);
    lcd.print("Danger Detected");
  } 
  else {
    // NORMAL MODE
    digitalWrite(BUZZER, LOW);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BLUE_LED, LOW);

    lcd.setCursor(0, 0);
    lcd.print("Smart Air Sys  ");
    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print((int)temperature);
    lcd.print("C H:");
    lcd.print((int)humidity);
    lcd.print("%    ");
  }

  // Serial Debug
  Serial.printf("T: %.1f H: %.1f | MQ2: %d MQ5: %d MQ7: %d\n", temperature, humidity, val_mq2, val_mq5, val_mq7);
  
  delay(2000); // 2-second update interval
}