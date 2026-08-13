#ifndef DASHBOARD_H
#define DASHBOARD_H

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
  <meta charset="UTF-8">
  <meta
    name="viewport"
    content="width=device-width, initial-scale=1.0"
  >

  <title>ESP32 Air Quality Monitor</title>

  <style>
    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      font-family:
        Arial,
        Helvetica,
        sans-serif;

      background: #0f172a;
      color: #e2e8f0;
    }

    .container {
      width: min(1100px, 92%);
      margin: auto;
      padding: 32px 0;
    }

    header {
      margin-bottom: 28px;
    }

    h1 {
      margin: 0 0 8px 0;
      font-size: 32px;
    }

    .subtitle {
      color: #94a3b8;
      margin: 0;
    }

    .connection {
      display: inline-block;
      margin-top: 14px;
      padding: 7px 12px;
      border-radius: 20px;
      font-size: 14px;
      font-weight: bold;
      background: #334155;
    }

    .connection.live {
      background: #14532d;
      color: #bbf7d0;
    }

    .connection.offline {
      background: #7f1d1d;
      color: #fecaca;
    }

    .section-title {
      margin-top: 32px;
      margin-bottom: 14px;
      font-size: 18px;
      color: #cbd5e1;
    }

    .grid {
      display: grid;
      grid-template-columns:
        repeat(
          auto-fit,
          minmax(210px, 1fr)
        );
      gap: 16px;
    }

    .card {
      background: #1e293b;
      border: 1px solid #334155;
      border-radius: 14px;
      padding: 20px;
    }

    .label {
      color: #94a3b8;
      font-size: 14px;
      margin-bottom: 10px;
    }

    .value {
      font-size: 30px;
      font-weight: bold;
    }

    .unit {
      color: #94a3b8;
      font-size: 15px;
      margin-left: 4px;
    }

    .status-grid {
      display: grid;
      grid-template-columns:
        repeat(
          auto-fit,
          minmax(180px, 1fr)
        );
      gap: 12px;
    }

    .status-card {
      display: flex;
      align-items: center;
      justify-content: space-between;

      background: #1e293b;
      border: 1px solid #334155;
      border-radius: 12px;
      padding: 15px 18px;
    }

    .status {
      padding: 5px 10px;
      border-radius: 14px;
      font-size: 12px;
      font-weight: bold;
    }

    .status.ok {
      background: #14532d;
      color: #bbf7d0;
    }

    .status.error {
      background: #7f1d1d;
      color: #fecaca;
    }

    footer {
      margin-top: 35px;
      color: #64748b;
      font-size: 13px;
    }

    code {
      color: #cbd5e1;
    }
  </style>
</head>

<body>

  <div class="container">

    <header>
      <h1>ESP32 Air Quality Monitor</h1>

      <p class="subtitle">
        Simulated environmental monitoring prototype
      </p>

      <div
        id="connection"
        class="connection"
      >
        Connecting...
      </div>
    </header>


    <div class="section-title">
      Environmental Conditions
    </div>

    <div class="grid">

      <div class="card">
        <div class="label">
          Temperature
        </div>

        <span
          id="temperature"
          class="value"
        >
          --
        </span>

        <span class="unit">
          C
        </span>
      </div>


      <div class="card">
        <div class="label">
          Humidity
        </div>

        <span
          id="humidity"
          class="value"
        >
          --
        </span>

        <span class="unit">
          %
        </span>
      </div>


      <div class="card">
        <div class="label">
          Atmospheric Pressure
        </div>

        <span
          id="pressure"
          class="value"
        >
          --
        </span>

        <span class="unit">
          hPa
        </span>
      </div>

    </div>


    <div class="section-title">
      Particulate Matter
    </div>

    <div class="grid">

      <div class="card">
        <div class="label">
          PM1.0
        </div>

        <span
          id="pm1"
          class="value"
        >
          --
        </span>

        <span class="unit">
          ug/m3
        </span>
      </div>


      <div class="card">
        <div class="label">
          PM2.5
        </div>

        <span
          id="pm25"
          class="value"
        >
          --
        </span>

        <span class="unit">
          ug/m3
        </span>
      </div>


      <div class="card">
        <div class="label">
          PM10
        </div>

        <span
          id="pm10"
          class="value"
        >
          --
        </span>

        <span class="unit">
          ug/m3
        </span>
      </div>

    </div>


    <div class="section-title">
      System Health
    </div>

    <div class="status-grid">

      <div class="status-card">
        <span>DHT22</span>

        <span
          id="dhtStatus"
          class="status"
        >
          --
        </span>
      </div>


      <div class="status-card">
        <span>BMP180</span>

        <span
          id="bmpStatus"
          class="status"
        >
          --
        </span>
      </div>


      <div class="status-card">
        <span>PMS5003</span>

        <span
          id="pmsStatus"
          class="status"
        >
          --
        </span>
      </div>


      <div class="status-card">
        <span>WiFi</span>

        <span
          id="wifiStatus"
          class="status"
        >
          --
        </span>
      </div>

    </div>


    <footer>
      <div>
        Data source:
        <code>/api/readings</code>
      </div>

      <div id="lastUpdated">
        Waiting for first update...
      </div>
    </footer>

  </div>


  <script>

    function formatValue(
      value,
      decimals
    ) {

      if (
        value === null ||
        value === undefined
      ) {

        return "--";
      }

      return Number(value).toFixed(
        decimals
      );
    }


    function setStatus(
      elementId,
      healthy
    ) {

      const element =
        document.getElementById(
          elementId
        );

      if (healthy) {

        element.textContent =
          "OK";

        element.className =
          "status ok";

      } else {

        element.textContent =
          "ERROR";

        element.className =
          "status error";
      }
    }


    async function updateDashboard() {

      const connection =
        document.getElementById(
          "connection"
        );

      try {

        const response =
          await fetch(
            "/api/readings",
            {
              cache: "no-store"
            }
          );


        if (!response.ok) {

          throw new Error(
            "HTTP error"
          );
        }


        const data =
          await response.json();


        // ---------------------------------------------
        // Environmental readings
        // ---------------------------------------------

        document.getElementById(
          "temperature"
        ).textContent =
          formatValue(
            data.temperature_c,
            1
          );


        document.getElementById(
          "humidity"
        ).textContent =
          formatValue(
            data.humidity_percent,
            1
          );


        document.getElementById(
          "pressure"
        ).textContent =
          formatValue(
            data.pressure_hpa,
            1
          );


        // ---------------------------------------------
        // Particulate readings
        // ---------------------------------------------

        document.getElementById(
          "pm1"
        ).textContent =
          formatValue(
            data.pm1_ug_m3,
            0
          );


        document.getElementById(
          "pm25"
        ).textContent =
          formatValue(
            data.pm25_ug_m3,
            0
          );


        document.getElementById(
          "pm10"
        ).textContent =
          formatValue(
            data.pm10_ug_m3,
            0
          );


        // ---------------------------------------------
        // Sensor health
        // ---------------------------------------------

        setStatus(
          "dhtStatus",
          data.status.dht22
        );


        setStatus(
          "bmpStatus",
          data.status.bmp180
        );


        setStatus(
          "pmsStatus",
          data.status.pms5003
        );


        setStatus(
          "wifiStatus",
          data.status.wifi
        );


        // ---------------------------------------------
        // Connection information
        // ---------------------------------------------

        connection.textContent =
          "LIVE";

        connection.className =
          "connection live";


        document.getElementById(
          "lastUpdated"
        ).textContent =
          "Last updated: " +
          new Date().toLocaleTimeString();

      }

      catch (error) {

        connection.textContent =
          "CONNECTION LOST";

        connection.className =
          "connection offline";
      }
    }


    // Update immediately
    updateDashboard();


    // Then update every second
    setInterval(
      updateDashboard,
      1000
    );

  </script>

</body>

</html>
)rawliteral";

#endif