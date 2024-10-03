#include <WiFi.h>
#include <WebServer.h>

// Wi-Fi credentials
const char* ssid = "****";
const char* password = "****";

// Server running on port 80
WebServer server(80);

// Sensor pin
int sensorPin = A10;

// Circular buffer for real-time data (30 seconds, 10 samples per second)
const int bufferSize = 30;
float secondAverages[bufferSize];
int bufferIndex = 0;

// Variables for real-time sampling (10 samples per second)
int sampleCounter = 0;
float secondSum = 0;

// Variables to store hourly, daily, and weekly averages
float dailyAverages[24];     // Daily history (24 hours)
float weeklyAverages[7];     // Weekly history (7 days)
int dailyIndex = 0;
int weeklyIndex = 0;

float hourSum = 0;
float daySum = 0;
int minuteCounter = 0;
int dayCounter = 0;
unsigned long startTime = millis(); // Track the system start time

// Thresholds for determining moisture levels
const int dryThreshold = 3100;
const int wetThreshold = 2200;

// Helper function to serve the HTML page
void serveHTML() {
  String html = R"=====(
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Plant Moisture Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
      body {
        font-family: 'Arial', sans-serif;
        color: #333;
        margin: 0;
        padding: 0;
        transition: background-color 0.5s ease;
      }
      .container {
        max-width: 1200px;
        margin: 0 auto;
        padding: 20px;
      }
      h1, h2 {
        text-align: center;
        color: #444;
      }
      .chart-container {
        display: flex;
        justify-content: space-around;
        margin-top: 20px;
        flex-wrap: wrap;
      }
      .chart-box {
        background-color: white;
        padding: 20px;
        border-radius: 10px;
        box-shadow: 0 4px 8px rgba(0,0,0,0.1);
        margin: 10px;
        flex: 1;
      }
      .moisture-status {
        margin: 20px 0;
        text-align: center;
        font-size: 36px;
      }
      .moisture-bar-container {
        display: flex;
        justify-content: center;
        align-items: center;
        margin-bottom: 20px;
      }
      .moisture-bar {
        width: 80%;
        height: 20px;
        background-color: #ddd;
        border-radius: 10px;
        overflow: hidden;
      }
      .moisture-bar-inner {
        height: 100%;
        transition: width 0.5s ease;
      }
      .info {
        text-align: center;
        margin-top: 40px;
        font-size: 18px;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Real-Time Moisture Sensor Dashboard</h1>
      <p class="info">This dashboard provides real-time moisture readings of your plant. Below is a moisture indicator and the state of your plant.</p>

      <div class="moisture-status" id="moistureIcon">🌿 Moist</div>

      <div class="moisture-bar-container">
        <div class="moisture-bar">
          <div class="moisture-bar-inner" id="moistureBar" style="width: 50%; background-color: #4CAF50;"></div>
        </div>
      </div>

      <div class="chart-container">
        <div class="chart-box">
          <h2>Real-Time Sensor Data</h2>
          <canvas id="realtimeChart"></canvas>
        </div>
        <div class="chart-box">
          <h2>Daily History (Hourly Averages)</h2>
          <canvas id="dailyChart"></canvas>
        </div>
        <div class="chart-box">
          <h2>Weekly History (Daily Averages)</h2>
          <canvas id="weeklyChart"></canvas>
        </div>
      </div>
    </div>

    <script>
      const realtimeCtx = document.getElementById('realtimeChart').getContext('2d');
      const dailyCtx = document.getElementById('dailyChart').getContext('2d');
      const weeklyCtx = document.getElementById('weeklyChart').getContext('2d');
      const moistureBar = document.getElementById('moistureBar');
      const moistureIcon = document.getElementById('moistureIcon');

      const realtimeChart = new Chart(realtimeCtx, {
        type: 'line',
        data: {
          labels: Array.from({ length: 30 }, (_, i) => `${30 - i}s ago`),
          datasets: [{
            label: 'Moisture (Last 30 Seconds)',
            borderColor: 'rgb(75, 192, 192)',
            backgroundColor: 'rgba(75, 192, 192, 0.2)',
            data: Array(30).fill(0),
            fill: true,
            tension: 0.4
          }]
        },
        options: {
          responsive: true,
          scales: {
            x: { ticks: { autoSkip: true, maxTicksLimit: 10 } },
            y: {
              beginAtZero: true,
              min: 2000,
              max: 3200,
              stepSize: 200,
              maxTicksLimit: 5
            }
          },
          animation: { duration: 0 },
          plugins: { legend: { display: true, position: 'top' } }
        }
      });

      const dailyChart = new Chart(dailyCtx, {
        type: 'line',
        data: {
          labels: Array.from({ length: 24 }, (_, i) => `${24 - i}h ago`),
          datasets: [{
            label: 'Moisture (Hourly Avg)',
            borderColor: 'rgb(192, 75, 75)',
            backgroundColor: 'rgba(192, 75, 75, 0.2)',
            data: Array(24).fill(0),
            fill: true,
            tension: 0.4
          }]
        },
        options: {
          responsive: true,
          scales: { x: { maxTicksLimit: 6 }, y: { beginAtZero: true, min: 2000, max: 3200, stepSize: 200 } },
          animation: { duration: 0 },
          plugins: { legend: { display: true, position: 'top' } }
        }
      });

      const weeklyChart = new Chart(weeklyCtx, {
        type: 'line',
        data: {
          labels: Array.from({ length: 7 }, (_, i) => `${7 - i} days ago`),
          datasets: [{
            label: 'Moisture (Daily Avg)',
            borderColor: 'rgb(75, 192, 192)',
            backgroundColor: 'rgba(75, 192, 192, 0.2)',
            data: Array(7).fill(0),
            fill: true,
            tension: 0.4
          }]
        },
        options: {
          responsive: true,
          scales: { x: { maxTicksLimit: 7 }, y: { beginAtZero: true, min: 2000, max: 3200, stepSize: 200 } },
          animation: { duration: 0 },
          plugins: { legend: { display: true, position: 'top' } }
        }
      });

      function updateCharts() {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
            // Update real-time chart
            realtimeChart.data.datasets[0].data.shift();
            realtimeChart.data.datasets[0].data.push(data.realtimeValues[data.realtimeValues.length - 1]);

            // Update daily and weekly charts
            dailyChart.data.datasets[0].data = data.dailyValues;
            weeklyChart.data.datasets[0].data = data.weeklyValues;

            // Update moisture status bar and icon
            const latestValue = data.realtimeValues[data.realtimeValues.length - 1];
            const moisturePercent = mapRange(latestValue, 2200, 3100, 100, 0); // Wet = 100%, Dry = 0%

            moistureBar.style.width = `${moisturePercent}%`;
            if (latestValue <= 2200) {
              document.body.style.backgroundColor = '#4CAF50';  // Green (Moist)
              moistureBar.style.backgroundColor = '#4CAF50';
              moistureIcon.textContent = "🌿 Moist";
            } else if (latestValue > 2200 && latestValue <= 2600) {
              document.body.style.backgroundColor = '#FFEB3B';  // Yellow (Moist)
              moistureBar.style.backgroundColor = '#FFEB3B';
              moistureIcon.textContent = "🌱 Moist";
            } else if (latestValue > 2600 && latestValue <= 3100) {
              document.body.style.backgroundColor = '#FFC107';  // Yellow (Drying)
              moistureBar.style.backgroundColor = '#FFC107';
              moistureIcon.textContent = "🌵 Dry";
            } else {
              document.body.style.backgroundColor = '#FF5722';  // Red (Dry)
              moistureBar.style.backgroundColor = '#FF5722';
              moistureIcon.textContent = "🔥 Very Dry";
            }

            realtimeChart.update();
            dailyChart.update();
            weeklyChart.update();
          });
      }

      // Map the sensor value to a percentage for the moisture bar
      function mapRange(value, in_min, in_max, out_min, out_max) {
        return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
      }

      // Update the charts every 100ms
      setInterval(updateCharts, 100);
    </script>
  </body>
  </html>
  )=====";

  server.send(200, "text/html", html);
}

// Endpoint to serve real-time sensor data as JSON
void serveData() {
  String json = "{\"realtimeValues\":["; 
  for (int i = 0; i < bufferSize; i++) {
    json += String(secondAverages[(bufferIndex + i) % bufferSize]);
    if (i < bufferSize - 1) json += ",";
  }
  json += "],\"dailyValues\":[";
  for (int i = 0; i < 24; i++) {
    json += String(dailyAverages[i]);
    if (i < 23) json += ",";
  }
  json += "],\"weeklyValues\":[";
  for (int i = 0; i < 7; i++) {
    json += String(weeklyAverages[i]);
    if (i < 6) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// Update real-time and history data
void updateSensorData() {
  int sensorValue = analogRead(sensorPin); // Read sensor value

  // Store real-time data
  secondAverages[bufferIndex] = sensorValue;
  bufferIndex = (bufferIndex + 1) % bufferSize;

  // Accumulate hourly averages
  secondSum += sensorValue;
  sampleCounter++;

  if (sampleCounter == 10) {
    hourSum += secondSum / sampleCounter;
    minuteCounter++;
    secondSum = 0;
    sampleCounter = 0;
  }

  if (minuteCounter == 60) { // Every hour
    dailyAverages[dailyIndex] = hourSum / 60; // Store daily hourly averages
    hourSum = 0;
    dailyIndex = (dailyIndex + 1) % 24;
    minuteCounter = 0;

    // Accumulate weekly averages
    daySum += dailyAverages[dailyIndex];
    dayCounter++;
  }

  if (dayCounter == 24) { // Every day
    weeklyAverages[weeklyIndex] = daySum / 24; // Store weekly daily averages
    weeklyIndex = (weeklyIndex + 1) % 7;
    daySum = 0;
    dayCounter = 0;
  }
}

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  // Initialize the data buffers
  for (int i = 0; i < bufferSize; i++) secondAverages[i] = 0;
  for (int i = 0; i < 24; i++) dailyAverages[i] = 0;
  for (int i = 0; i < 7; i++) weeklyAverages[i] = 0;

  // Start the server
  server.on("/", serveHTML);        // Serve the HTML page
  server.on("/data", serveData);    // Serve the sensor data
  server.begin();
}

void loop() {
  server.handleClient();
  updateSensorData(); // Update sensor data every 100ms
  delay(100);         // Delay of 100ms to collect 10 samples per second
}
