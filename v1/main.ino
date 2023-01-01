#include <WiFi.h>
#include <WebServer.h>
#include "EmonLib.h"
#include <driver/adc.h>

// Force EmonLib to use 10bit ADC resolution
#define ADC_INPUT 34
#define HOME_VOLTAGE 247.0
#define ADC_BITS    10
#define ADC_COUNTS  (1<<ADC_BITS)


#define seconds() (millis()/1000)

EnergyMonitor emon1;

unsigned long lastMeasurement = 0;

const char* ssid = "esp32_ap";
const char* password = "password";


WebServer server(80);

void setup() {
  Serial.begin(115200);

  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  analogReadResolution(10);

  // Initialize emon library (30 = calibration number)
  emon1.current(ADC_INPUT, 30);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  IPAddress ip = WiFi.softAPIP();  // Get the IP address of the access point
  Serial.print("AP IP address: ");
  Serial.println(ip);  // Print the IP address to the serial monitor

  server.on("/", []() {
    String html = "<html><head><script>";
    html += "var source = new EventSource(\"/random\");";
    html += "source.onmessage = function(event) {";
    html += "  var data = JSON.parse(event.data);";
    html += "  var row = document.createElement(\"tr\");";
    html += "  var col1 = document.createElement(\"td\");";
    html += "  var col2 = document.createElement(\"td\");";
    html += "  var col3 = document.createElement(\"td\");";
    html += "  var col4 = document.createElement(\"td\");";
    html += "  col1.innerHTML = data.timestamp;";
    html += "  col2.innerHTML = data.num1;";
    html += "  col3.innerHTML = data.num2;";
    html += "  col4.innerHTML = data.amps;";
    html += "  row.appendChild(col1);";
    html += "  row.appendChild(col2);";
    html += "  row.appendChild(col3);";
    html += "  row.appendChild(col4);";
    html += "  document.getElementById(\"table\").appendChild(row);";
    html += "  document.getElementById(\"output1\").innerHTML = data.analog;";
    html += "  document.getElementById(\"output2\").innerHTML = data.amps;";
    html += "};";
    html += "</script></head><body><h1>Pin 34 Measurement</h1>";
    html += "<div id=\"output1\"></div>";
    html += "<h1>Amp Measurement</h1>";
    html += "<div id=\"output2\"></div>";
    html += "<table id=\"table\"><tr><th>TimeStamp</th><th>Number 1</th><th>Number 2</th><th>Analogue Pin</th></tr></table>";
    html += "</body></html>";

    server.send(200, "text/html", html);
  });

  server.on("/random", []() {
    static uint32_t counter = 0;
    double amps = emon1.calcIrms(1480);

    String response = "id: ";
    response += counter++;
    response += "\n";
    response += "data: {\"num1\": ";
    response += random(0, 100);
    response += ", \"num2\": ";
    response += random(0, 100);
    response += ", \"analog\": ";
    response += analogRead(34);
    response += ", \"amps\": ";
    response += amps;
    response += ", \"timestamp\": ";
    response += String(seconds());
    response += "}\n\n";

    server.send(200, "text/event-stream", response);
    // delay(100);  // Delay for 300 milliseconds
  });

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();
  if(currentMillis - lastMeasurement > 10000){
    double amps = emon1.calcIrms(1480); // Calculate Irms only
    double watt = amps * HOME_VOLTAGE;
    Serial.print("Amp Value = ");
    Serial.println(amps);
    Serial.print("Watt Value = ");
    Serial.println(watt);
  }
}
