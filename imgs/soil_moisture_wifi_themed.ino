#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Wi-Fi credentials
const char* ssid = "";
const char* password = "";

// Server running on port 80
WebServer server(80);
Preferences preferences;

// Sensor pin
int sensorPin = A10;

// Circular buffer for the last 30 seconds of data (10 samples per second, averaged per second)
const int bufferSize = 30;
float secondAverages[bufferSize]; // Stores 30-second raw data
int bufferIndex = 0;

// Variables for real-time sampling (10 samples per second)
int sampleCounter = 0;
float secondSum = 0;

// Variables to store hourly averages for the past 24 hours and daily averages for the past 7 days
float hourlyAverages[24]; // 24 hours for daily history
float weeklyAverages[7];  // 7 days for weekly history
int hourlyIndex = 0;
int weeklyIndex = 0;
float hourSum = 0;
float daySum = 0;
int minuteCounter = 0;
int dayCounter = 0;

// Thresholds for determining moisture levels (stored in preferences)
int dryThreshold;
int wetThreshold;

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
        transition: width 0.5s ease, background-color 0.5s ease;
      }
      .info {
        text-align: center;
        margin-top: 40px;
        font-size: 18px;
      }

      /* Sidebar CSS */
      #sidebar {
        position: fixed;
        bottom: 20px;
        right: 20px;
        width: 300px;
        height: 300px;
        background-color: #111;
        color: white;
        border-radius: 10px;
        box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
        transition: 0.5s;
        padding: 20px;
        display: none;
      }

      #openbtn {
        font-size: 20px;
        cursor: pointer;
        background-color: #111;
        color: white;
        padding: 10px 15px;
        border: none;
        border-radius: 5px;
        position: fixed;
        bottom: 20px;
        right: 20px;
        z-index: 10;
      }

      #openbtn:hover {
        background-color: #444;
      }

    </style>
  </head>
  <body>
    <div class="container">
      <h1>Real-Time Moisture Sensor Dashboard</h1>
      <p class="info">This dashboard provides real-time moisture readings of your plant.</p>

      <button id="openbtn" onclick="toggleSidebar()">☰ Settings</button>

      <!-- Sidebar -->
      <div id="sidebar">
        <h2>Settings</h2>
        <p>Set moisture thresholds:</p>
        <label for="dryThreshold">Dry Threshold:</label>
        <input type="number" id="dryThreshold" value="3100"><br><br>
        <label for="wetThreshold">Wet Threshold:</label>
        <input type="number" id="wetThreshold" value="2200"><br><br>
        <button onclick="saveSettings()">Save</button>
      </div>

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
      let dryThreshold = 3100;
      let wetThreshold = 2200;

      // Toggle Sidebar
      function toggleSidebar() {
        const sidebar = document.getElementById("sidebar");
        sidebar.style.display = sidebar.style.display === "none" ? "block" : "none";
      }

      // Save settings without adjusting axis limits manually
      function saveSettings() {
        dryThreshold = document.getElementById("dryThreshold").value;
        wetThreshold = document.getElementById("wetThreshold").value;
        fetch('/save-settings?dryThreshold=' + dryThreshold + '&wetThreshold=' + wetThreshold)
          .then(response => response.json())
          .then(data => {
            alert("Settings saved!");
            // No need to call updateChartYLimits here
          });
      }


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
            beginAtZero: false, // Let the chart dynamically adjust the Y-axis
            suggestedMin: 1300, // Initial suggestion based on expected range
            suggestedMax: 3500, // Initial suggestion based on expected range
            ticks: {
              autoSkip: true // Ensure that the chart properly skips unnecessary ticks
            }
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
          scales: { 
            x: { maxTicksLimit: 6 },
            y: {
              beginAtZero: true,
              min: 2000, // Default Y-axis minimum
              max: 3200, // Default Y-axis maximum
              stepSize: 200
            }
          },
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
          scales: {
            x: { maxTicksLimit: 7 },
            y: {
              beginAtZero: true,
              min: 2000, // Default Y-axis minimum
              max: 3200, // Default Y-axis maximum
              stepSize: 200
            }
          },
          animation: { duration: 0 },
          plugins: { legend: { display: true, position: 'top' } }
        }
      });

      // Function to update the Y-axis limits dynamically based on thresholds
      function updateChartYLimits() {
        const minY = Math.min(wetThreshold - 200, 2000);
        const maxY = Math.max(dryThreshold + 200, 3200);

        // Update Y-axis limits for all charts
        realtimeChart.options.scales.y.min = minY;
        realtimeChart.options.scales.y.max = maxY;
        dailyChart.options.scales.y.min = minY;
        dailyChart.options.scales.y.max = maxY;
        weeklyChart.options.scales.y.min = minY;
        weeklyChart.options.scales.y.max = maxY;

        // Re-render charts
        realtimeChart.update();
        dailyChart.update();
        weeklyChart.update();
      }

      function updateCharts() {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
            // Ensure valid data is pushed to the chart
            const latestValue = data.realtimeValues[data.realtimeValues.length - 1];
            console.log("Latest sensor value:", latestValue);  // Debugging sensor values

            // Shift old data and push new data into the chart
            realtimeChart.data.datasets[0].data.shift();
            realtimeChart.data.datasets[0].data.push(latestValue);

            // Update daily and weekly charts in the same way
            dailyChart.data.datasets[0].data = data.hourlyValues;
            weeklyChart.data.datasets[0].data = data.dailyValues;

            // Update the moisture status bar and icon based on the latest value
            const moisturePercent = mapRange(latestValue, wetThreshold, dryThreshold, 100, 0); // Wet = 100%, Dry = 0%
            const moistureColor = getMoistureColor(latestValue);
            moistureBar.style.width = `${moisturePercent}%`;
            moistureBar.style.backgroundColor = moistureColor;
            document.body.style.backgroundColor = moistureColor;

            if (latestValue <= wetThreshold) {
              moistureIcon.textContent = "🌿 Moist";
            } else if (latestValue > wetThreshold && latestValue <= dryThreshold) {
              moistureIcon.textContent = "🌵 Dry";
            } else {
              moistureIcon.textContent = "🔥 Very Dry";
            }

            // Update all charts
            realtimeChart.update();
            dailyChart.update();
            weeklyChart.update();
          });
      }


      // Function to map moisture value to a smooth color gradient between green, yellow, and red
      function getMoistureColor(value) {
        const green = [76, 175, 80];   // RGB for green
        const yellow = [255, 235, 59]; // RGB for yellow
        const red = [255, 87, 34];     // RGB for red

        let resultColor;
        if (value <= wetThreshold) {
          resultColor = `rgb(${green[0]}, ${green[1]}, ${green[2]})`;
        } else if (value > wetThreshold && value <= dryThreshold) {
          resultColor = interpolateColor(green, yellow, (value - wetThreshold) / (dryThreshold - wetThreshold));
        } else {
          resultColor = `rgb(${red[0]}, ${red[1]}, ${red[2]})`;
        }
        return resultColor;
      }

      // Function to interpolate between two colors
      function interpolateColor(color1, color2, factor) {
        return `rgb(${Math.round(color1[0] + (color2[0] - color1[0]) * factor)}, 
                    ${Math.round(color1[1] + (color2[1] - color1[1]) * factor)}, 
                    ${Math.round(color1[2] + (color2[2] - color1[2]) * factor)})`;
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
  json += "],\"hourlyValues\":[";
  for (int i = 0; i < 24; i++) {
    json += String(hourlyAverages[i]);
    if (i < 23) json += ",";
  }
  json += "],\"dailyValues\":[";
  for (int i = 0; i < 7; i++) {
    json += String(weeklyAverages[i]);
    if (i < 6) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// Collect sensor data and update averages
void updateSensorData() {
  int sensorValue = analogRead(sensorPin); // Read the sensor value

  // Ensure sensor value is within a valid range
  if (sensorValue < 1300 || sensorValue > 3500) {
    sensorValue = constrain(sensorValue, 1300, 3500);
  }

  // Store real-time data
  secondAverages[bufferIndex] = sensorValue;
  bufferIndex = (bufferIndex + 1) % bufferSize;

  // Accumulate hourly averages
  secondSum += sensorValue;
  sampleCounter++;

  if (sampleCounter == 10) { // Every second
    hourSum += secondSum / sampleCounter;
    minuteCounter++;
    secondSum = 0;
    sampleCounter = 0;
  }

  if (minuteCounter == 60) { // Every hour
    hourlyAverages[hourlyIndex] = hourSum / 60;  // Store hourly averages
    hourSum = 0;
    minuteCounter = 0;
    hourlyIndex = (hourlyIndex + 1) % 24;

    // Accumulate daily averages
    daySum += hourlyAverages[hourlyIndex];
    dayCounter++;
  }

  if (dayCounter == 24) { // Every day
    weeklyAverages[weeklyIndex] = daySum / 24;  // Store daily averages
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
  for (int i = 0; i < 24; i++) hourlyAverages[i] = 0;
  for (int i = 0; i < 7; i++) weeklyAverages[i] = 0;

  // Start the server
  server.on("/", serveHTML);        // Serve the HTML page
  server.on("/data", serveData);    // Serve sensor data
  server.on("/save-settings", []() {
    dryThreshold = server.arg("dryThreshold").toInt();
    wetThreshold = server.arg("wetThreshold").toInt();
    preferences.putInt("dryThreshold", dryThreshold);
    preferences.putInt("wetThreshold", wetThreshold);
    server.send(200, "application/json", "{\"status\":\"success\"}");
  });
  server.begin();

  // Load threshold values from preferences
  preferences.begin("settings", false);
  dryThreshold = preferences.getInt("dryThreshold", 3100);  // Default to 3100
  wetThreshold = preferences.getInt("wetThreshold", 2200);  // Default to 2200
}

void loop() {
  server.handleClient();
  updateSensorData();
  delay(100);  // Update sensor data every 100ms
}
