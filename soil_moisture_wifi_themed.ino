#include <WiFi.h>
#include <WebServer.h>

// Wi-Fi credentials
const char* ssid = "****";
const char* password = "****";

// Server running on port 80
WebServer server(80);

// Sensor pin
int sensorPin = A10;

// Circular buffer for the last 30 seconds of data (each second averaged from 10 samples)
const int bufferSize = 30; // 30 samples for 30 seconds (averaged every second)
float secondAverages[bufferSize]; // Stores 30-second averages
int bufferIndex = 0;

// Variables to store real-time sampling (10 samples per second)
int sampleCounter = 0;
float secondSum = 0;

// Variables to store hourly averages for the past 7 days (168 hours)
float hourlyAverages[24 * 7]; // 7 days * 24 hours = 168 hours
int hourIndex = 0;
float hourSum = 0;
int minuteCounter = 0;

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
      <p class="info">This dashboard provides real-time moisture readings of your plant pot. Below is a moisture indicator and the state of your plant.</p>

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
          <h2>Weekly History (Hourly Averages)</h2>
          <canvas id="historyChart"></canvas>
        </div>
      </div>
    </div>

    <script>
      const realtimeCtx = document.getElementById('realtimeChart').getContext('2d');
      const historyCtx = document.getElementById('historyChart').getContext('2d');
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
            x: {
              ticks: {
                autoSkip: true,
                maxTicksLimit: 10
              }
            },
            y: {
              beginAtZero: true,
              stepSize: 50,
              maxTicksLimit: 5
            }
          },
          animation: {
            duration: 0
          },
          plugins: {
            legend: {
              display: true,
              position: 'top'
            }
          }
        }
      });

      const historyChart = new Chart(historyCtx, {
        type: 'line',
        data: {
          labels: Array.from({ length: 168 }, (_, i) => `${168 - i}h ago`),
          datasets: [{
            label: 'Moisture (Hourly Avg)',
            borderColor: 'rgb(192, 75, 75)',
            backgroundColor: 'rgba(192, 75, 75, 0.2)',
            data: Array(168).fill(0),
            fill: true,
            tension: 0.4
          }]
        },
        options: {
          responsive: true,
          scales: {
            x: {
              ticks: {
                autoSkip: true,
                maxTicksLimit: 10
              }
            },
            y: {
              beginAtZero: true,
              stepSize: 100,
              maxTicksLimit: 5
            }
          },
          animation: {
            duration: 0
          },
          plugins: {
            legend: {
              display: true,
              position: 'top'
            }
          }
        }
      });

      function updateCharts() {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
            // Shift left and add new data to the real-time chart
            realtimeChart.data.datasets[0].data.shift();
            realtimeChart.data.datasets[0].data.push(data.realtimeValues[data.realtimeValues.length - 1]);

            // Update the history chart
            historyChart.data.datasets[0].data = data.historyValues;

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
            historyChart.update();
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
  json += "],\"historyValues\":[";
  for (int i = 0; i < 24 * 7; i++) {
    json += String(hourlyAverages[(hourIndex + i) % (24 * 7)]);
    if (i < (24 * 7) - 1) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// Collects sensor data and updates second-level averages
void updateSensorData() {
  int sensorValue = analogRead(sensorPin); // Read the raw sensor value

  // Store each raw sensor value directly into the buffer for real-time plotting
  secondAverages[bufferIndex] = sensorValue;
  bufferIndex = (bufferIndex + 1) % bufferSize;  // Circular buffer

  // Accumulate the sensor values for hourly averaging (only for storage, not real-time plotting)
  secondSum += sensorValue;
  sampleCounter++;

  // Every 10 samples (1 second), calculate average (for hourly storage)
  if (sampleCounter == 10) {
    float secondAverage = secondSum / sampleCounter; // Average for storage (1 second average)

    // Add to the hourly average calculation
    hourSum += secondAverage;
    minuteCounter++;

    // If 60 seconds have passed, calculate the hourly average for later storage
    if (minuteCounter == 60) {
      float hourlyAverage = hourSum / 60;  // Hourly average
      hourlyAverages[hourIndex] = hourlyAverage;  // Store in hourly averages for history
      hourIndex = (hourIndex + 1) % (24 * 7);  // Circular buffer for 7 days of hourly data
      hourSum = 0;  // Reset for the next hour
      minuteCounter = 0;
    }

    // Reset second sum and sample counter for the next second
    secondSum = 0;
    sampleCounter = 0;
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
  for (int i = 0; i < (24 * 7); i++) hourlyAverages[i] = 0;

  // Start the server
  server.on("/", serveHTML);        // Serve the HTML page
  server.on("/data", serveData);    // Serve the real-time sensor data
  server.begin();
}

void loop() {
  server.handleClient();
  updateSensorData(); // Collect 10 samples every second
  delay(100);         // Delay of 100ms to take 10 samples per second
}
