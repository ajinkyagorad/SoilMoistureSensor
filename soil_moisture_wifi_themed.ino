#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Ticker.h>

// Wi-Fi credentials
const char* ssid = "Triton9";
const char* password = "88888888";

// Server running on port 80
WebServer server(80);
Preferences preferences;

// Sensor pin
int sensorPin = A10;

// Sampling ticker
Ticker samplingTicker;

// Circular buffers
const int secBufferSize = 10;
float sec_values[secBufferSize]; // Stores 10 values (each 100ms)
int secBufferIndex = 0;

const int minBufferSize = 60;
float min_values[minBufferSize]; // Stores 60 values (each second average)
int minBufferIndex = 0;

const int hourBufferSize = 60;
float hour_values[hourBufferSize]; // Stores 60 values (each minute average)
int hourBufferIndex = 0;

const int dayBufferSize = 24;
float day_values[dayBufferSize]; // Stores 24 values (each hour average)
int dayBufferIndex = 0;

const int weekBufferSize = 7;
float week_values[weekBufferSize]; // Stores 7 values (each day average)
int weekBufferIndex = 0;

const int yearBufferSize = 52;
float year_values[yearBufferSize]; // Stores 52 values (each week average)
int yearBufferIndex = 0;

// Accumulators for averaging
float secSum = 0;
int secSampleCounter = 0;
float minSum = 0;
float hourSum = 0;
float daySum = 0;
float weekSum = 0;

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
  for (int i = 0; i < secBufferSize; i++) {
    json += String(sec_values[(secBufferIndex + i) % secBufferSize]);
    if (i < secBufferSize - 1) json += ",";
  }
  json += "],\"hourlyValues\":[";
  for (int i = 0; i < dayBufferSize; i++) {
    json += String(day_values[i]);
    if (i < dayBufferSize - 1) json += ",";
  }
  json += "],\"dailyValues\":[";
  for (int i = 0; i < weekBufferSize; i++) {
    json += String(week_values[i]);
    if (i < weekBufferSize - 1) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// ISR function called every 100 ms for sampling
void IRAM_ATTR sampleSensor() {
  int sensorValue = analogRead(sensorPin); // Read the sensor value
  sec_values[secBufferIndex] = sensorValue;
  secBufferIndex = (secBufferIndex + 1) % secBufferSize;

  secSum += sensorValue;
  secSampleCounter++;

  if (secSampleCounter == secBufferSize) { // After 1 second (10 samples)
    float secAvg = secSum / secBufferSize;
    secSum = 0;
    secSampleCounter = 0;

    min_values[minBufferIndex] = secAvg;
    minBufferIndex = (minBufferIndex + 1) % minBufferSize;

    minSum += secAvg;
    if (minBufferIndex == 0) { // After 1 minute (60 seconds)
      float minAvg = minSum / minBufferSize;
      minSum = 0;

      hour_values[hourBufferIndex] = minAvg;
      hourBufferIndex = (hourBufferIndex + 1) % hourBufferSize;

      hourSum += minAvg;
      if (hourBufferIndex == 0) { // After 1 hour (60 minutes)
        float hourAvg = hourSum / hourBufferSize;
        hourSum = 0;

        day_values[dayBufferIndex] = hourAvg;
        dayBufferIndex = (dayBufferIndex + 1) % dayBufferSize;

        daySum += hourAvg;
        if (dayBufferIndex == 0) { // After 1 day (24 hours)
          float dayAvg = daySum / dayBufferSize;
          daySum = 0;

          week_values[weekBufferIndex] = dayAvg;
          weekBufferIndex = (weekBufferIndex + 1) % weekBufferSize;

          weekSum += dayAvg;
          if (weekBufferIndex == 0) { // After 1 week (7 days)
            float weekAvg = weekSum / weekBufferSize;
            weekSum = 0;

            year_values[yearBufferIndex] = weekAvg;
            yearBufferIndex = (yearBufferIndex + 1) % yearBufferSize;
          }
        }
      }
    }
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
  for (int i = 0; i < secBufferSize; i++) sec_values[i] = 0;
  for (int i = 0; i < minBufferSize; i++) min_values[i] = 0;
  for (int i = 0; i < hourBufferSize; i++) hour_values[i] = 0;
  for (int i = 0; i < dayBufferSize; i++) day_values[i] = 0;
  for (int i = 0; i < weekBufferSize; i++) week_values[i] = 0;
  for (int i = 0; i < yearBufferSize; i++) year_values[i] = 0;

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
  dryThreshold = preferences.getInt("dryThreshold", 2600);  // Default to 3100
  wetThreshold = preferences.getInt("wetThreshold", 1800);  // Default to 1400;

  // Start the sampling ticker at 100ms intervals
  samplingTicker.attach_ms(100, sampleSensor);
}

void loop() {
  server.handleClient();
}
