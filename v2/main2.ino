#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include "EmonLib.h"
#include <driver/adc.h>
#include <time.h>
#include <Time.h>


// Force EmonLib to use 10bit ADC resolution
#define ADC_INPUT 34
#define HOME_VOLTAGE 247.0
#define ADC_BITS    10
#define ADC_COUNTS  (1<<ADC_BITS)
#define min(a,b) ((a)<(b)?(a):(b))



#define seconds() (millis()/1000)

double watt_accum = 0;
int count = 0;

EnergyMonitor emon1;

unsigned long lastMeasurement = 0;

const char* ssid = "esp32_ap";
const char* password = "password";


WebServer server(80);

void setup() {
  Serial.begin(115200);

    // Initialize the SPIFFS file system
  if (!SPIFFS.begin()) {
    Serial.println("Error initializing SPIFFS!");
    return;
  }

  // Open a file for writing (this will overwrite any existing data in the file)
  File file = SPIFFS.open("/analog_values.csv", FILE_WRITE);
  if (!file) {
    Serial.println("Error opening file for writing!");
    return;
  }

  file.println("Timestamp,Amps,Watt,kWs,kWs_Accum");

  // Close the file
  file.close();

  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  analogReadResolution(10);

  // Initialize emon library (30 = calibration number)
  // 60.6 for 100:50mA
  // 28 for 30V/1V
  emon1.current(ADC_INPUT, 60.6);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  IPAddress ip = WiFi.softAPIP();  // Get the IP address of the access point
  Serial.print("AP IP address: ");
  Serial.println(ip);  // Print the IP address to the serial monitor

    // Set up the web server
  server.on("/", []() {
    // Send the HTML header
    //server.send(200, "text/html", "");

    // Send the HTML body
    String body = "<html><head><script>";
    body += "var source = new EventSource(\"/random\");";
    body += "source.onmessage = function(event) {";
    body += "  var data = JSON.parse(event.data);";
    body += "  document.getElementById(\"output1\").innerHTML = data.num1;";
    body += "  document.getElementById(\"output2\").innerHTML = data.amps;";
    body += "};";
    body += "</script></head><body><h1>Pin 34 Measurement</h1>";
    body += "<div id=\"output1\"></div>";
    body += "<h1>Amp Measurement</h1>";
    body += "<div id=\"output2\"></div>";
    body += "<a href='/download' target='_blank'><button>Download CSV</button></a>";
    body += "<button onclick='location.reload()'>Refresh</button>";      
    body += " <table border='1'><tr><th>Timestamp</th><th>Amps</th><th>Watt</th><th>kWs</th><th>kWs_Accum</th></tr>";
    // Open the file for reading
    File file = SPIFFS.open("/analog_values.csv", FILE_READ);
    if (!file) {
      Serial.println("Error opening file for reading!");
      return;
    }
  // Read the values from the file and add them to the table
  // Keep track of the number of rows that have been read
  int rowsRead = 0;
  // Keep track of the last 10 rows read from the file
  String last100Rows[100];
  while (file.available()) {
    String row = file.readStringUntil('\n');
    // Store the row in the last10Rows array
    last100Rows[rowsRead % 100] = row;
    rowsRead++;
  }
  file.close();
  // Add the last 100 rows to the table
  for (int i = 0; i < 100; i++) {
    String row = last100Rows[(rowsRead - 1 - i + 100) % 100];
    body += "<tr>";
    int columnStartIndex = 0;
    int columnEndIndex = row.indexOf(',', columnStartIndex);
    while (columnEndIndex != -1) {
      String column = row.substring(columnStartIndex, columnEndIndex);
      body += "<td>" + column + "</td>";
      columnStartIndex = columnEndIndex + 1;
      columnEndIndex = row.indexOf(',', columnStartIndex);
    }
    String column = row.substring(columnStartIndex);
    body += "<td>" + column + "</td>";
    body += "</tr>";
  }
  body += "</table><br>";
  body += "</body></html>";
    server.send(200, "text/html", body);
  });

  server.on("/random", []() {
    static uint32_t counter = 0;
    double amps = emon1.calcIrms(1480);
    // amps = amps - 1;
    // if (amps < 0) amps = 0;

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

  server.on("/download", []() {
//     // Open the file for reading
//     File file = SPIFFS.open("/analog_values.csv", FILE_READ);
//     if (!file) {
//       Serial.println("Error opening file for reading!");
//       return;
//     }

//  // Allocate a buffer to store the file contents
//     size_t bufferSize = file.size() + 1; // +1 for the null terminator
//     Serial.println("...");
//     Serial.println("...");
//     Serial.println("File size: " + file.size());
//     Serial.println("...");
//     Serial.println("...");
//     char* buffer = new char[bufferSize];

//     // Read the contents of the file into the buffer
//     size_t bytesRead = file.readBytes(buffer, bufferSize);
//     buffer[bytesRead] = '\0'; // Add the null terminator
//     file.close();
//     // Set the content type and disposition for the file
//     server.setContentLength(bytesRead);
//     server.sendHeader("Content-Type", "text/csv");
//     server.sendHeader("Content-Disposition", "attachment; filename=analog_values.csv");
//     // Send the contents of the file to the client
//     server.send(200, "", buffer);
//     // Don't forget to free the buffer when you're done with it
//     delete[] buffer;


    File file = SPIFFS.open("/analog_values.csv", FILE_READ);
  if (!file) {
    Serial.println("Error opening file for reading!");
    return;
  }

  // Set the content type and disposition for the file
  server.setContentLength(file.size());
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=analog_values.csv");

  // Send the file in chunks
  const size_t chunkSize = 1024; // Set the chunk size to 1KB
  while (file.available()) {
    size_t bytesToRead = min(chunkSize, file.available());
    char* buffer = new char[bytesToRead];
    file.readBytes(buffer, bytesToRead);
    // server.send(200, "", buffer, bytesToRead);
    // server.send(200, "", String(buffer, bytesToRead));
    server.sendContent(buffer, bytesToRead);

    delete[] buffer;
  }

  file.close();






  });

  server.begin();
  Serial.println("Web server started");


}

void loop() {
  

  // Open a file for appending
  File file = SPIFFS.open("/analog_values.csv", FILE_APPEND);
  if (!file) {
    Serial.println("Error opening file for appending!");
    return;
  }

  time_t now = time(nullptr);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%m-%d %H:%M:%S", localtime(&now));
  file.print(timestamp);
  file.print(",");

  count++;

  // Append the values of the analogue GPIO pin to the file
  double amps = emon1.calcIrms(1480); // Calculate Irms only
  // amps = random(0,100);
  if (count <= 10) {
    amps = 0;
  } 

  // amps = amps - 1;
  // if (amps < 0) amps = 0;

  double watt = amps * HOME_VOLTAGE;
  double kWs = (amps * HOME_VOLTAGE)/(1000*60);
  watt_accum += kWs;
  // int analogValue = analogRead(ANALOG_GPIO_PIN);
  file.print(amps);
  file.print(",");
  file.print(watt);
  file.print(",");
  file.print(kWs);
  file.print(",");
  file.print(watt_accum);
  file.println(","); 

  // Close the file
  file.close();

  // Open the file for reading
  file = SPIFFS.open("/analog_values.csv", FILE_READ);
  if (!file) {
    Serial.println("Error opening file for reading!");
    return;
  }

  // Read the values from the file and print the last value to the serial port
  String lastRow;
  while (file.available()) {
    lastRow = file.readStringUntil('\n');
  }
  Serial.println(lastRow);

  server.handleClient();

  // Close the file
  file.close();

  // Delay for 1 second
  delay(1000);
}
