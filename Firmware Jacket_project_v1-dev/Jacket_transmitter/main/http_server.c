/*
 * http_server.c
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "http_server.h"

static const char *TAG = "HTTP_SERVER";

// Global sensor values
static float g_longitude = 0.0f;
static float g_latitude  = 0.0f;
static float g_bpm       = 0.0f;
static float g_sp02      = 0.0f;
static float g_temp      = 0.0f;
static bool  finger_available = false;
static bool LoRa_connection = false;

/* --------------------- UPDATE FUNCTIONS --------------------- */
void http_update_gps_sensor_data(float longitude, float latitude)
{
    g_longitude = longitude;
    g_latitude  = latitude;
}

void http_update_sp02_sensor_data(float bpm, float sp02)
{
    g_bpm  = bpm;
    g_sp02 = sp02;
}

void http_update_finger_status(bool val)
{
    finger_available = val;
}

void http_update_lora_status(bool val)
{
    LoRa_connection = val;
}

void http_update_tmp116_sensor_data(float temp)
{
	g_temp = temp;
}
/* --------------------- JSON API HANDLER --------------------- */
static esp_err_t api_sensor_handler(httpd_req_t *req)
{
    char buffer[256];

	snprintf(buffer, sizeof(buffer),
	    "{"
	        "\"bpm\": %.2f,"
	        "\"spo2\": %.2f,"
	        "\"latitude\": %.6f,"
	        "\"longitude\": %.6f,"
	        "\"finger\": %s,"
	        "\"connection\": %s, "
	        "\"Temperature\": %.4f"
	    "}",
	    g_bpm, g_sp02,
	    g_latitude, g_longitude,
	    finger_available ? "true" : "false",
	    LoRa_connection ? "true" : "false",
	    g_temp
	);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    return httpd_resp_send(req, buffer, strlen(buffer));
}

/* --------------------- HTML PAGE --------------------- */
static const char *HTML_PAGE =
"<!doctype html>"
"<html lang='en'>"
"<head>"
"<meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1' />"
"<title>Dashboard</title>"

"<style>"
"body{margin:0;background:#f5f7fa;font-family:Arial;color:#1a1a1a;padding:20px;}"

"h1{font-size:28px;margin-bottom:20px;text-align:center;color:#333;}"

".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:20px;}"

".card{background:white;padding:18px;border-radius:14px;"
"      box-shadow:0 4px 12px rgba(0,0,0,0.08);text-align:center;}"

".label{color:#555;font-size:16px;margin-top:12px;}"

".coord{font-size:22px;font-weight:700;margin-top:5px;color:#222;}"

/* Circular Gauge Base */
".gauge{width:160px;height:160px;margin:auto;position:relative;}"
".gauge svg{transform:rotate(-90deg);}"

/* Background ring */
".gauge .bg{stroke:#e2e8f0;}"

/* Smooth animation */
".gauge circle{fill:none;stroke-width:12;stroke-linecap:round;}"
".gauge .progress{transition:stroke-dashoffset 0.8s ease;}"

/* Accent colors for gauges */
".bpm .progress{stroke:#8b5cf6;}"
".spo2 .progress{stroke:#0ea5e9;}"

/* Text inside gauge */
".gauge-value{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);"
"    font-size:30px;font-weight:700;color:#1e293b;}"

/* Pills */
".pill{padding:8px 14px;border-radius:20px;font-size:18px;color:white;"
"      font-weight:bold;display:inline-block;}"

".thermo-wrapper {"
"  display: flex;"
"  flex-direction: column;"
"  align-items: center;"
"  margin: 20px auto;"
"  height: 200px;"
"}"
".thermo-container {"
"  position: relative;"
"  width: 60px;"
"  height: 180px;"
"  background: linear-gradient(to top, #ef4444 0%, #fbbf24 50%, #22c55e 100%);"
"  border-radius: 30px 30px 0 0;"
"  overflow: hidden;"
"  box-shadow: inset 0 0 10px rgba(0,0,0,0.1);"
"  border: 3px solid #e2e8f0;"
"}"
/* Glass effect overlay */
".thermo-container::before {"
"  content: '';"
"  position: absolute;"
"  top: 0;"
"  left: 0;"
"  right: 0;"
"  height: 50%;"
"  background: linear-gradient(to bottom, rgba(255,255,255,0.3) 0%, rgba(255,255,255,0) 100%);"
"  border-radius: 30px 30px 0 0;"
"  z-index: 1;"
"}"
/* Mercury column */
".thermo-bar {"
"  position: absolute;"
"  bottom: 0;"
"  left: 5px;"
"  right: 5px;"
"  height: 0%;"
"  background: linear-gradient(to top, #ef4444, #dc2626);"
"  border-radius: 10px 10px 0 0;"
"  transition: height 0.8s cubic-bezier(0.34, 1.56, 0.64, 1);"
"  box-shadow: 0 0 10px rgba(239, 68, 68, 0.3);"
"  z-index: 2;"
"}"
/* Temperature markers */
".thermo-marks {"
"  position: absolute;"
"  left: -25px;"
"  top: 0;"
"  bottom: 0;"
"  width: 20px;"
"  display: flex;"
"  flex-direction: column;"
"  justify-content: space-between;"
"  padding: 10px 0;"
"}"
".thermo-mark {"
"  font-size: 12px;"
"  color: #666;"
"  text-align: right;"
"  padding-right: 5px;"
"}"
/* Temperature bulb at bottom */
".thermo-bulb {"
"  width: 80px;"
"  height: 40px;"
"  background: linear-gradient(to right, #ef4444, #dc2626);"
"  border-radius: 40px 40px 0 0;"
"  margin-top: -20px;"
"  position: relative;"
"  z-index: 1;"
"  box-shadow: 0 0 15px rgba(239, 68, 68, 0.3);"
"}"
".thermo-bulb::after {"
"  content: '';"
"  position: absolute;"
"  top: 10px;"
"  left: 10px;"
"  right: 10px;"
"  bottom: 10px;"
"  background: radial-gradient(circle at center, rgba(255,255,255,0.4) 0%, rgba(255,255,255,0) 70%);"
"  border-radius: 30px 30px 0 0;"
"}"
/* Temperature value display */
".thermo-value {"
"  font-size: 28px;"
"  font-weight: 700;"
"  color: #1e293b;"
"  margin-top: 15px;"
"  text-align: center;"
"  min-height: 40px;"
"  display: flex;"
"  align-items: center;"
"  justify-content: center;"
"}"
".thermo-unit {"
"  font-size: 18px;"
"  color: #666;"
"  margin-left: 3px;"
"}"
/* Temperature status indicator */
".temp-status {"
"  font-size: 14px;"
"  font-weight: 600;"
"  color: #666;"
"  margin-top: 5px;"
"  padding: 4px 12px;"
"  border-radius: 12px;"
"  background: #f1f5f9;"
"  display: inline-block;"
"}"
"</style>"
"</head>"

"<body>"
"<h1>DASHBOARD</h1>"

"<div class='grid'>"

/* ------------ BPM CIRCULAR GAUGE ------------- */
"<div class='card bpm'>"
"  <div class='gauge'>"
"    <svg width='160' height='160'>"
"      <circle class='bg' cx='80' cy='80' r='60'></circle>"
"      <circle class='progress' id='bpm_circle' cx='80' cy='80' r='60'"
"        stroke-dasharray='377' stroke-dashoffset='377'></circle>"
"    </svg>"
"    <div class='gauge-value' id='bpm_val'>--</div>"
"  </div>"
"  <div class='label'>BPM</div>"
"</div>"

/* ------------ SPO2 CIRCULAR GAUGE ------------- */
"<div class='card spo2'>"
"  <div class='gauge'>"
"    <svg width='160' height='160'>"
"      <circle class='bg' cx='80' cy='80' r='60'></circle>"
"      <circle class='progress' id='spo2_circle' cx='80' cy='80' r='60'"
"        stroke-dasharray='377' stroke-dashoffset='377'></circle>"
"    </svg>"
"    <div class='gauge-value' id='spo2_val'>--</div>"
"  </div>"
"  <div class='label'>SpO₂ (%)</div>"
"</div>"

/*-- ------------ TEMPERATURE THERMOMETER ------------- --*/
"<div class='card'>"
"  <div class='label'>Temperature</div>"
"  <div class='thermo-wrapper'>"
"    <div class='thermo-container'>"
"      <div class='thermo-marks'>"
"        <div class='thermo-mark'>50°C</div>"
"        <div class='thermo-mark'>40°C</div>"
"        <div class='thermo-mark'>30°C</div>"
"        <div class='thermo-mark'>20°C</div>"
"        <div class='thermo-mark'>10°C</div>"
"        <div class='thermo-mark'>0°C</div>"
"      </div>"
"      <div class='thermo-bar' id='temp_bar'></div>"
"    </div>"
"    <div class='thermo-bulb'></div>"
"    <div class='thermo-value'>"
"      <span id='temp_val'>--</span>"
"      <span class='thermo-unit'>°C</span>"
"    </div>"
"    <div id='temp_status' class='temp-status'>--</div>"
"  </div>"
"</div>"

/* ------------ GPS INFO ------------- */
"<div class='card'>"
"  <div class='label'>Latitude</div>"
"  <div class='coord' id='lat'>--</div>"
"  <div class='label'>Longitude</div>"
"  <div class='coord' id='lon'>--</div>"
"</div>"

/* ------------ FINGER STATUS ------------- */
"<div class='card'>"
"  <div class='label'>Max30102 Sensor Status</div>"
"  <div id='finger' class='pill' style='background:#999;'>--</div>"
"</div>"

/* ------------ LORA CONNECTION STATUS ------------- */
"<div class='card'>"
"  <div class='label'>LoRa Connection</div>"
"  <div id='lora' class='pill' style='background:#999;'>--</div>"
"</div>"

"</div>"   /* end grid */

/* ---------------- JAVASCRIPT ---------------- */
"<script>"
"const CIRC = 377;"

"function setGauge(circle, value, max) {"
"  let percent = Math.max(0, Math.min(value / max, 1));"
"  circle.style.strokeDashoffset = (CIRC - (CIRC * percent));"
"}"
"function getTempStatus(t) {"
"  if (t < 32) {"
"    return { text: 'NO SKIN CONTACT', color: '#6b7280' };"
"  }"
"  else if (t < 36.0) {"
"    return { text: 'LOW', color: '#3b82f6' };"
"  }"
"  else if (t >= 36.0 && t <= 37.5) {"
"    return { text: 'NORMAL', color: '#22c55e' };"
"  }"
"  else if (t > 37.5 && t <= 38.5) {"
"    return { text: 'WARM', color: '#f59e0b' };"
"  }"
"  else if (t > 38.5 && t <= 40.0) {"
"    return { text: 'HIGH FEVER', color: '#ef4444' };"
"  }"
"  else {"
"    return { text: 'CRITICAL', color: '#b91c1c' };"
"  }"
"}"

"function update() {"
"  fetch('/api/sensor?ts=' + Date.now(), { cache:'no-store' })"
"    .then(r => r.json())"
"    .then(d => {"
"	   let rounded = Math.floor(d.bpm);"
"      document.getElementById('bpm_val').innerText = rounded;"
"      document.getElementById('spo2_val').innerText = d.spo2;"

"      setGauge(document.getElementById('bpm_circle'), d.bpm, 200);"
"      setGauge(document.getElementById('spo2_circle'), d.spo2, 100);"

"      document.getElementById('lat').innerText = d.latitude;"
"      document.getElementById('lon').innerText = d.longitude;"

"      let f = document.getElementById('finger');"
"      if(d.finger){ f.innerText='CONNECTED'; f.style.background='#22c55e'; }"
"      else{ f.innerText='DISCONNECTED'; f.style.background='#ef4444'; }"

"      let l = document.getElementById('lora');"
"      if(d.connection){ l.innerText='CONNECTED'; l.style.background='#06b6d4'; }"
"      else{ l.innerText='DISCONNECTED'; l.style.background='#f97316'; }"

"      let temp = d.Temperature || 0;"
"      let minTemp = 0;"
"      let maxTemp = 50;"
"      let percent = Math.max(0, Math.min((temp - minTemp) / (maxTemp - minTemp), 1));"

"      document.getElementById('temp_bar').style.height = (percent * 100) + '%';"

"      let tempVal = document.getElementById('temp_val');"
"      tempVal.style.transition = 'transform 0.3s ease';"
"      tempVal.style.transform = 'scale(1.1)';"

"      setTimeout(() => {"
"        tempVal.style.transform = 'scale(1)';"
"        tempVal.innerText = temp.toFixed(1) + '';"
"      }, 150);"

"      let status = getTempStatus(temp);"
"      let statusEl = document.getElementById('temp_status');"
"      statusEl.innerText = status.text;"
"      statusEl.style.background = status.color + '20';"
"      statusEl.style.color = status.color;"

"    });"
"}"

"setInterval(update, 1000);"
"update();"
"</script>"

"</body></html>";


/* --------------------- HTML HANDLER --------------------- */
static esp_err_t html_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_PAGE, strlen(HTML_PAGE));
    return ESP_OK;
}

/* --------------------- URI ROUTES --------------------- */
static const httpd_uri_t page_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = html_handler,
};

static const httpd_uri_t api_uri = {
    .uri = "/api/sensor",
    .method = HTTP_GET,
    .handler = api_sensor_handler,
};

/* --------------------- SERVER START --------------------- */
httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "Starting server...");
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &page_uri);
        httpd_register_uri_handler(server, &api_uri);
    }
    return server;
}

void stop_webserver(httpd_handle_t server)
{
    if (server)
        httpd_stop(server);
}
