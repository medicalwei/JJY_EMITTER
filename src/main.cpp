#include "esp_pm.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "config.h"

// Class to generate time code
class jjy_timecode_generator_t
{
public:
	unsigned char year10;
	unsigned char year1;
	unsigned char yday100;
	unsigned char yday10;
	unsigned char yday1;
	unsigned short yday;
	unsigned char hour10;
	unsigned char hour1;
	unsigned char min10;
	unsigned char min1;
	unsigned char wday;
	unsigned char mon;
	unsigned char day;
	bool valid = false;

	unsigned char result[61];

public:
	void generate()
	{
		unsigned char pa1 = 0;
		unsigned char pa2 = 0;
		for (int sec = 0; sec < 60; sec++)
		{
			unsigned char cc;

			cc = 0;
#define SET(X) \
	if (X)     \
	cc |= 1
			switch (sec)
			{
			case 0: /* marker */
				cc = 2;
				break;

			case 1:
				SET(min10 & 4);
				pa2 ^= cc;
				break;

			case 2:
				SET(min10 & 2);
				pa2 ^= cc;
				break;

			case 3:
				SET(min10 & 1);
				pa2 ^= cc;
				break;

			case 4:
				cc = 0;
				break;

			case 5:
				SET(min1 & 8);
				pa2 ^= cc;
				break;

			case 6:
				SET(min1 & 4);
				pa2 ^= cc;
				break;

			case 7:
				SET(min1 & 2);
				pa2 ^= cc;
				break;

			case 8:
				SET(min1 & 1);
				pa2 ^= cc;
				break;

			case 9:
				cc = 2;
				break;

			case 10:
				cc = 0;
				break;

			case 11:
				cc = 0;
				break;

			case 12:
				SET(hour10 & 2);
				pa1 ^= cc;
				break;

			case 13:
				SET(hour10 & 1);
				pa1 ^= cc;
				break;

			case 14:
				cc = 0;
				break;

			case 15:
				SET(hour1 & 8);
				pa1 ^= cc;
				break;

			case 16:
				SET(hour1 & 4);
				pa1 ^= cc;
				break;

			case 17:
				SET(hour1 & 2);
				pa1 ^= cc;
				break;

			case 18:
				SET(hour1 & 1);
				pa1 ^= cc;
				break;

			case 19:
				cc = 2;
				break;

			case 20:
				cc = 0;
				break;

			case 21:
				cc = 0;
				break;

			case 22:
				SET(yday100 & 2);
				break;

			case 23:
				SET(yday100 & 1);
				break;

			case 24:
				cc = 0;
				break;

			case 25:
				SET(yday10 & 8);
				break;

			case 26:
				SET(yday10 & 4);
				break;

			case 27:
				SET(yday10 & 2);
				break;

			case 28:
				SET(yday10 & 1);
				break;

			case 29:
				cc = 2;
				break;

			case 30:
				SET(yday1 & 8);
				break;

			case 31:
				SET(yday1 & 4);
				break;

			case 32:
				SET(yday1 & 2);
				break;

			case 33:
				SET(yday1 & 1);
				break;

			case 34:
				cc = 0;
				break;

			case 35:
				cc = 0;
				break;

			case 36:
				cc = pa1;
				break;

			case 37:
				cc = pa2;
				break;

			case 38:
				cc = 0; /* SU1 */
				break;

			case 39:
				cc = 2;
				break;

			case 40:
				cc = 0; /* SU2 */
				break;

			case 41:
				SET(year10 & 8);
				break;

			case 42:
				SET(year10 & 4);
				break;

			case 43:
				SET(year10 & 2);
				break;

			case 44:
				SET(year10 & 1);
				break;

			case 45:
				SET(year1 & 8);
				break;

			case 46:
				SET(year1 & 4);
				break;

			case 47:
				SET(year1 & 2);
				break;

			case 48:
				SET(year1 & 1);
				break;

			case 49:
				cc = 2;
				break;

			case 50:
				SET(wday & 4);
				break;

			case 51:
				SET(wday & 2);
				break;

			case 52:
				SET(wday & 1);
				break;

			case 53:
				cc = 0;
				break;

			case 54:
				cc = 0;
				break;

			case 55:
				cc = 0;
				break;

			case 56:
				cc = 0;
				break;

			case 57:
				cc = 0;
				break;

			case 58:
				cc = 0;
				break;

			case 59:
				cc = 2;
				break;
			}
			result[sec] = cc;
		}
		result[60] = 2;
		valid = true;
	}
} gen;

WebServer server(80);

// WiFi status for thread-safe communication
volatile bool wifiConnected = false;
TaskHandle_t wifiTaskHandle = NULL;

bool connectWiFi(int maxRetries = 6, int retryDelay = 5000) {
	WiFi.disconnect();
	delay(1000);
	WiFi.begin(ssid, password);
	
	int retryCount = 0;
	
	while (retryCount < maxRetries) {
		switch (WiFi.status()) {
		case WL_NO_SSID_AVAIL:
			Serial.println("[WiFi] SSID not found");
			break;
		case WL_CONNECT_FAILED:
			Serial.println("[WiFi] Connection failed, retrying...");
			retryCount++;
			if (retryCount < maxRetries) {
				WiFi.disconnect();
				delay(1000);
				WiFi.begin(ssid, password);
			}
			break;
		case WL_CONNECTION_LOST:
			Serial.println("[WiFi] Connection was lost, retrying...");
			retryCount++;
			if (retryCount < maxRetries) {
				WiFi.disconnect();
				delay(1000);
				WiFi.begin(ssid, password);
			}
			break;
		case WL_SCAN_COMPLETED:
			Serial.println("[WiFi] Scan is completed");
			break;
		case WL_DISCONNECTED:
			Serial.println("[WiFi] WiFi is disconnected, waiting...");
			break;
		case WL_CONNECTED:
			Serial.println("[WiFi] WiFi is connected!");
			Serial.print("[WiFi] IP address: ");
			Serial.println(WiFi.localIP());
			Serial.print("[WiFi] Signal strength: ");
			Serial.print(WiFi.RSSI());
			Serial.println(" dBm");
			wifiConnected = true;
			return true;
			break;
		default:
			Serial.print("[WiFi] WiFi Status: ");
			Serial.println(WiFi.status());
			break;
		}
		delay(retryDelay);
		retryCount++;
	}
	
	Serial.println("[WiFi] Failed to connect after all retries");
	wifiConnected = false;
	return false;
}

void wifiManagementTask(void *parameter) {
	while (true) {
		// Check WiFi status every 30 seconds
		vTaskDelay(30000 / portTICK_PERIOD_MS);
		
		if (WiFi.status() != WL_CONNECTED) {
			wifiConnected = false;
			Serial.println("[WiFi] Connection lost, attempting reconnection...");
			bool reconnected = connectWiFi(3, 2000); // Moderate reconnect: 3 retries, 2 second delay
			if (!reconnected) {
				Serial.println("[WiFi] Reconnection failed, will retry in 30 seconds");
			}
		} else {
			wifiConnected = true;
		}
	}
}

void handleRoot() {
	time_t t = time(&t);
	struct tm *tm = localtime(&t);
	bool date_valid = (tm->tm_year + 1900 >= 2000);
	
	String html = "<!DOCTYPE html><html><head><title>JJY Emitter Status</title>";
	html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
	html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}";
	html += ".container{background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
	html += ".status{display:inline-block;padding:4px 8px;border-radius:4px;color:white;font-weight:bold}";
	html += ".ok{background:#4CAF50}.error{background:#f44336}.warning{background:#ff9800}";
	html += "table{border-collapse:collapse;width:100%;margin:10px 0}";
	html += "th,td{border:1px solid #ddd;padding:8px;text-align:left}th{background:#f2f2f2}";
	html += ".timecode{font-family:monospace;font-size:14px;word-break:break-all}</style>";
	html += "<script>setTimeout(function(){location.reload()},5000)</script></head><body>";
	
	html += "<div class='container'><h1>JJY Time Code Emitter</h1>";
	
	html += "<h2>System Status</h2><table>";
	html += "<tr><th>Parameter</th><th>Value</th><th>Status</th></tr>";
	
	html += "<tr><td>WiFi Connection</td><td>" + WiFi.localIP().toString() + "</td>";
	html += "<td><span class='status " + String(wifiConnected ? "ok'>Connected" : "error'>Disconnected") + "</span></td></tr>";
	
	html += "<tr><td>WiFi Signal</td><td>" + String(WiFi.RSSI()) + " dBm</td>";
	html += "<td><span class='status " + String(WiFi.RSSI() > -70 ? "ok'>Good" : WiFi.RSSI() > -85 ? "warning'>Fair" : "error'>Poor") + "</span></td></tr>";
	
	html += "<tr><td>NTP Sync</td><td>" + String(date_valid ? "Synchronized" : "Not synchronized") + "</td>";
	html += "<td><span class='status " + String(date_valid ? "ok'>OK" : "error'>Error") + "</span></td></tr>";
	
	html += "<tr><td>Time Code Valid</td><td>" + String(gen.valid ? "Valid" : "Invalid") + "</td>";
	html += "<td><span class='status " + String(gen.valid ? "ok'>OK" : "error'>Error") + "</span></td></tr>";
	
	html += "<tr><td>Signal Output</td><td>" + String((date_valid && gen.valid) ? "Active" : "Inactive") + "</td>";
	html += "<td><span class='status " + String((date_valid && gen.valid) ? "ok'>Active" : "warning'>Inactive") + "</span></td></tr>";
	
	html += "</table>";
	
	if (date_valid) {
		html += "<h2>Current Time</h2><table>";
		html += "<tr><th>Parameter</th><th>Value</th></tr>";
		char timeStr[32];
		sprintf(timeStr, "%04d/%02d/%02d %02d:%02d:%02d", 
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
		html += "<tr><td>Local Time</td><td>" + String(timeStr) + "</td></tr>";
		html += "<tr><td>Day of Year</td><td>" + String(tm->tm_yday + 1) + "</td></tr>";
		html += "<tr><td>Day of Week</td><td>" + String(tm->tm_wday) + "</td></tr>";
		html += "<tr><td>Uptime</td><td>" + String(millis() / 1000) + " seconds</td></tr>";
		html += "</table>";
	}
	
	if (gen.valid) {
		html += "<h2>Current Time Code (60 seconds)</h2>";
		html += "<div class='timecode'>";
		for (int i = 0; i < 60; i++) {
			if (i % 10 == 0 && i > 0) html += " ";
			switch(gen.result[i]) {
				case 0: html += "0"; break;
				case 1: html += "1"; break;
				case 2: html += "M"; break;
				default: html += "?"; break;
			}
		}
		html += "</div>";
		html += "<p><small>M=Marker, 0=Zero bit, 1=One bit</small></p>";
	}
	
	html += "<h2>Configuration</h2><table>";
	html += "<tr><th>Parameter</th><th>Value</th></tr>";
	html += "<tr><td>40kHz Output Pin</td><td>" + String(JJY_40k_OUTPUT_PIN == -1 ? "Disabled" : String(JJY_40k_OUTPUT_PIN)) + "</td></tr>";
	html += "<tr><td>60kHz Output Pin</td><td>" + String(JJY_60k_OUTPUT_PIN == -1 ? "Disabled" : String(JJY_60k_OUTPUT_PIN)) + "</td></tr>";
	html += "<tr><td>Code Output Pin</td><td>" + String(JJY_CODE_NONINVERTED_OUTPUT_PIN == -1 ? "Disabled" : String(JJY_CODE_NONINVERTED_OUTPUT_PIN)) + "</td></tr>";
	html += "<tr><td>Inverted Code Pin</td><td>" + String(JJY_CODE_INVERTED_OUTPUT_PIN == -1 ? "Disabled" : String(JJY_CODE_INVERTED_OUTPUT_PIN)) + "</td></tr>";
	html += "<tr><td>Timezone</td><td>" + String(tz) + "</td></tr>";
	html += "</table>";
	
	html += "<p><small>Page auto-refreshes every 5 seconds</small></p>";
	html += "</div></body></html>";
	
	server.send(200, "text/html", html);
}

static void start_wifi()
{
	Serial.println();
	Serial.print("[WiFi] Connecting to ");
	Serial.println(ssid);

	WiFi.mode(WIFI_STA);
	WiFi.persistent(true);
	WiFi.setAutoReconnect(true);
	esp_wifi_set_ps(WIFI_PS_NONE); // Disable power saving to prevent disconnections

	sntp_servermode_dhcp(1);
	configTzTime(tz, ntp[0], ntp[1], ntp[2]);

	bool connected = connectWiFi(6, 5000); // 6 retries, 5 second delay
	if (!connected) {
		Serial.println("[WiFi] Continuing without WiFi...");
	}
}

void setup()
{
	Serial.begin(115200);

	delay(10);

	start_wifi();

	// Start WiFi management task
	xTaskCreatePinnedToCore(
		wifiManagementTask,   // Task function
		"WiFiManager",        // Task name
		4096,                 // Stack size
		NULL,                 // Parameters
		1,                    // Priority (lower than main loop)
		&wifiTaskHandle,      // Task handle
		0                     // Core 0 (main loop runs on core 1)
	);

	server.on("/", handleRoot);
	server.begin();
	if (wifiConnected) {
		Serial.println("Web server started at http://" + WiFi.localIP().toString());
	} else {
		Serial.println("Web server started (WiFi not connected)");
	}

	if (JJY_60k_OUTPUT_PIN != -1)
	{
		ledcSetup(LEDC_60k_CHANNEL, 60000.0, LEDC_RESOLUTION_BITS);
		ledcAttachPin(JJY_60k_OUTPUT_PIN, LEDC_60k_CHANNEL);
	}

	if (JJY_40k_OUTPUT_PIN != -1)
	{
		ledcSetup(LEDC_40k_CHANNEL, 40000.0, LEDC_RESOLUTION_BITS);
		ledcAttachPin(JJY_40k_OUTPUT_PIN, LEDC_40k_CHANNEL);
	}

	if (JJY_CODE_NONINVERTED_OUTPUT_PIN != -1)
		pinMode(JJY_CODE_NONINVERTED_OUTPUT_PIN, OUTPUT);

	if (JJY_CODE_INVERTED_OUTPUT_PIN != -1)
		pinMode(JJY_CODE_INVERTED_OUTPUT_PIN, OUTPUT);
}

void loop()
{
	static uint32_t min_origin_tick;
	static int last_min;
	static bool last_on_state;
	
	time_t t;
	t = time(&t);
	struct tm *tm = localtime(&t);
	bool date_valid = true;
	if (tm->tm_year + 1900 < 2000)
		date_valid = false; // NTP time sync probably not yet available
	if (last_min != tm->tm_min && tm->tm_sec == 0)
	{
		// Minute boundary - generate time code for the next minute
		gen.year10 = (tm->tm_year / 10) % 10;
		gen.year1 = tm->tm_year % 10;
		gen.yday100 = ((tm->tm_yday + 1) / 100) % 10;
		gen.yday10 = ((tm->tm_yday + 1) / 10) % 10;
		gen.yday1 = (tm->tm_yday + 1) % 10;
		gen.yday = (tm->tm_yday + 1);
		gen.hour10 = (tm->tm_hour / 10) % 10;
		gen.hour1 = tm->tm_hour % 10;
		gen.min10 = (tm->tm_min / 10) % 10;
		gen.min1 = tm->tm_min % 10;
		gen.wday = tm->tm_wday;
		gen.mon = tm->tm_mon;
		gen.day = tm->tm_mday;
		gen.generate();
		min_origin_tick = millis();
	}
	last_min = tm->tm_min;

	// Process each second
	uint32_t sub_min = millis() - min_origin_tick;
	int sec = sub_min / 1000;
	int sub_sec = sub_min % 1000;
	bool on = false;
	if (sec < 60) // sec can be 60 sometimes, ignore it for now
	{
		switch (gen.result[sec])
		{
		case 0:
			if (sub_sec < 800)
				on = true; // "0" コード
			break;
		case 1:
			if (sub_sec < 500)
				on = true; // "1" コード
			break;
		case 2:
			if (sub_sec < 200)
				on = true; // Marker
			break;
		default:
			break;
		}
		if (on != last_on_state)
		{
			last_on_state = on;
			if (date_valid && gen.valid)
			{
				// Output signal only when time is properly synchronized
				if (on)
				{
					if (JJY_60k_OUTPUT_PIN != -1)
						ledcWrite(LEDC_60k_CHANNEL, (1 << LEDC_RESOLUTION_BITS) / 2); // Duty cycle = 50%
					if (JJY_40k_OUTPUT_PIN != -1)
						ledcWrite(LEDC_40k_CHANNEL, (1 << LEDC_RESOLUTION_BITS) / 2); // Duty cycle = 50%
					if (JJY_CODE_NONINVERTED_OUTPUT_PIN != -1)
						digitalWrite(JJY_CODE_NONINVERTED_OUTPUT_PIN, 1);
					if (JJY_CODE_INVERTED_OUTPUT_PIN != -1)
						digitalWrite(JJY_CODE_INVERTED_OUTPUT_PIN, 0);
				}
				else
				{
					if (JJY_60k_OUTPUT_PIN != -1)
						ledcWrite(LEDC_60k_CHANNEL, 0); // Duty cycle 0 = OFF
					if (JJY_40k_OUTPUT_PIN != -1)
						ledcWrite(LEDC_40k_CHANNEL, 0); // Duty cycle 0 = OFF
					if (JJY_CODE_NONINVERTED_OUTPUT_PIN != -1)
						digitalWrite(JJY_CODE_NONINVERTED_OUTPUT_PIN, 0);
					if (JJY_CODE_INVERTED_OUTPUT_PIN != -1)
						digitalWrite(JJY_CODE_INVERTED_OUTPUT_PIN, 1);
				}
			}
		}
	}
	server.handleClient();
	vTaskDelay(5); // Sleep for a few ticks
}
