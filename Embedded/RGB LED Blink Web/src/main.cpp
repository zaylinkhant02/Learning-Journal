#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

const int redPin = 27;
const int greenPin = 26;
const int bluePin = 25;

WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

const String tunnelUrl = "https://vehicle-oldest-york-declare.trycloudflare.com/";

const String AP_SSID = "ESP32-LED-Setup";
const String AP_PASSWORD = "SecureAP123";

bool ledState = false;
bool wifiConfigured = false;
bool apModeActive = false;

unsigned long lastCmdTime = 0;
const unsigned long CMD_COOLDOWN = 200;

String currentSessionToken = "";
unsigned long sessionExpiry = 0;
const unsigned long SESSION_TIMEOUT = 3600000;

const char wifiConfigHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 WiFi Setup</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin: 0;
            padding: 20px;
            background-color: #1a1a1a;
            color: #ffffff;
        }
        .container {
            max-width: 400px;
            margin: 0 auto;
            background-color: #2d2d2d;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 4px 8px rgba(0,0,0,0.3);
        }
        h1 { color: #4CAF50; }
        input {
            width: 100%;
            padding: 12px 20px;
            margin: 8px 0;
            box-sizing: border-box;
            border: 2px solid #555;
            border-radius: 4px;
            background-color: #3d3d3d;
            color: white;
        }
        .button {
            background-color: #4CAF50;
            border: none;
            color: white;
            padding: 16px 32px;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: 16px;
            margin: 10px 5px;
            cursor: pointer;
            border-radius: 5px;
            width: 100%;
        }
        .button:hover { opacity: 0.8; }
        .info {
            color: #888;
            font-size: 14px;
            margin: 10px 0;
        }
        .error {
            color: #f44336;
            margin: 10px 0;
        }
        .success {
            color: #4CAF50;
            margin: 10px 0;
        }
        .scan-btn {
            background-color: #2196F3;
            margin-bottom: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>📶 WiFi Setup</h1>
        <p class="info">Enter your WiFi credentials to connect</p>
        
        <form action="/connect" method="POST">
            <input type="text" name="ssid" placeholder="WiFi SSID" required><br>
            <input type="password" name="password" placeholder="WiFi Password"><br>
            <input type="submit" value="Connect" class="button">
        </form>
        
        <button onclick="scanNetworks()" class="button scan-btn">Scan Networks</button>
        <div id="scanResults"></div>
        
        <div id="message"></div>
    </div>
    
    <script>
        function scanNetworks() {
            document.getElementById('scanResults').innerHTML = 'Scanning...';
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    let html = '<h3>Available Networks:</h3>';
                    data.forEach(network => {
                        html += '<div style="padding:5px;border-bottom:1px solid #444;cursor:pointer;" onclick="fillSSID(\'' + network.ssid + '\')">' + 
                                network.ssid + ' (' + network.rssi + ' dBm)</div>';
                    });
                    document.getElementById('scanResults').innerHTML = html;
                })
                .catch(error => {
                    document.getElementById('scanResults').innerHTML = 'Error scanning networks';
                });
        }
        
        function fillSSID(ssid) {
            document.querySelector('input[name="ssid"]').value = ssid;
        }
        
        const urlParams = new URLSearchParams(window.location.search);
        const msg = urlParams.get('msg');
        if (msg) {
            const msgDiv = document.getElementById('message');
            if (msg === 'success') {
                msgDiv.innerHTML = '<div class="success">✅ Connected successfully! The ESP32 will restart...</div>';
            } else if (msg === 'failed') {
                msgDiv.innerHTML = '<div class="error">❌ Failed to connect. Please check your credentials.</div>';
            }
        }
    </script>
</body>
</html>
)rawliteral";

const char loginHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Login</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin: 0;
            padding: 20px;
            background-color: #1a1a1a;
            color: #ffffff;
        }
        .container {
            max-width: 400px;
            margin: 0 auto;
            background-color: #2d2d2d;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 4px 8px rgba(0,0,0,0.3);
        }
        h1 { color: #4CAF50; }
        input {
            width: 100%;
            padding: 12px;
            margin: 10px 0;
            box-sizing: border-box;
            border: 2px solid #555;
            border-radius: 4px;
            background-color: #3d3d3d;
            color: white;
        }
        .button {
            background-color: #4CAF50;
            border: none;
            color: white;
            padding: 16px 32px;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: 16px;
            margin: 10px 0;
            cursor: pointer;
            border-radius: 5px;
            width: 100%;
        }
        .button:hover { opacity: 0.8; }
        .error {
            color: #f44336;
            margin: 10px 0;
        }
        .info {
            color: #888;
            font-size: 12px;
            margin: 10px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Login Required</h1>
        <p>Enter password to control the LED</p>
        <form action="/login" method="POST">
            <input type="password" name="password" placeholder="Enter password" required>
            <input type="submit" value="Login" class="button">
        </form>
        <div id="error" style="color:#f44336;margin-top:10px;"></div>
        <div class="info">Default password: admin123 (change in code)</div>
    </div>
    <script>
        const urlParams = new URLSearchParams(window.location.search);
        if (urlParams.get('error')) {
            document.getElementById('error').textContent = 'Invalid password!';
        }
        if (urlParams.get('logout')) {
            document.getElementById('error').textContent = 'Logged out successfully.';
            document.getElementById('error').style.color = '#4CAF50';
        }
        if (urlParams.get('expired')) {
            document.getElementById('error').textContent = 'Session expired. Please login again.';
        }
    </script>
</body>
</html>
)rawliteral";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 RGB LED Control</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin: 0;
            padding: 20px;
            background-color: #1a1a1a;
            color: #ffffff;
        }
        h1 {
            color: #4CAF50;
            margin-bottom: 10px;
        }
        .subtitle {
            color: #888;
            font-size: 14px;
            margin-bottom: 30px;
        }
        .container {
            max-width: 400px;
            margin: 0 auto;
            background-color: #2d2d2d;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 4px 8px rgba(0,0,0,0.3);
        }
        .button {
            background-color: #4CAF50;
            border: none;
            color: white;
            padding: 16px 32px;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: 18px;
            margin: 10px 5px;
            cursor: pointer;
            border-radius: 5px;
            transition: background-color 0.3s;
            width: 120px;
        }
        .button:hover {
            opacity: 0.8;
        }
        .button-off {
            background-color: #f44336;
        }
        .button-off:hover {
            background-color: #d32f2f;
        }
        .button-on {
            background-color: #4CAF50;
        }
        .button-on:hover {
            background-color: #45a049;
        }
        .color-btn {
            width: 80px;
            padding: 12px 20px;
            font-size: 14px;
        }
        .status {
            margin: 20px 0;
            padding: 10px;
            background-color: #3d3d3d;
            border-radius: 5px;
            font-size: 16px;
        }
        .status span {
            font-weight: bold;
        }
        .status-on {
            color: #4CAF50;
        }
        .status-off {
            color: #f44336;
        }
        .color-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            margin: 20px 0;
        }
        .color-btn-red { background-color: #ff0000; }
        .color-btn-green { background-color: #00ff00; color: #000; }
        .color-btn-blue { background-color: #0000ff; }
        .color-btn-white { background-color: #ffffff; color: #000; }
        .color-btn-off { background-color: #333333; }
        .tunnel-status {
            margin-top: 20px;
            padding: 10px;
            background-color: #1a1a1a;
            border-radius: 5px;
            font-size: 12px;
            color: #888;
            word-break: break-all;
        }
        .tunnel-status a {
            color: #4CAF50;
        }
        .local-ip {
            margin-top: 10px;
            padding: 10px;
            background-color: #1a1a1a;
            border-radius: 5px;
            font-size: 12px;
            color: #888;
        }
        .setup-instructions {
            margin-top: 15px;
            padding: 15px;
            background-color: #1a3a1a;
            border-radius: 5px;
            font-size: 13px;
            color: #8f8;
            text-align: left;
            border: 1px solid #2a5a2a;
        }
        .setup-instructions code {
            background: #0a1a0a;
            padding: 2px 6px;
            border-radius: 3px;
            font-size: 12px;
        }
        .logout-btn {
            background-color: #f44336;
            margin-top: 10px;
            font-size: 12px;
            padding: 8px 16px;
            width: auto;
        }
        .reset-btn {
            background-color: #ff9800;
            margin-top: 10px;
            font-size: 12px;
            padding: 8px 16px;
            width: auto;
        }
        .button-row {
            display: flex;
            gap: 10px;
            justify-content: center;
            flex-wrap: wrap;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>RGB LED Control</h1>
        <div class="subtitle">Online via Cloudflare Tunnel</div>
        
        <div class="status">
            LED Status: <span id="statusText" class="status-off">OFF</span>
        </div>

        <div>
            <button class="button button-on" onclick="sendCommand('on')">Turn ON</button>
            <button class="button button-off" onclick="sendCommand('off')">Turn OFF</button>
        </div>

        <h3>Set Color</h3>
        <div class="color-grid">
            <button class="button color-btn color-btn-red" onclick="sendCommand('red')">RED</button>
            <button class="button color-btn color-btn-green" onclick="sendCommand('green')">GREEN</button>
            <button class="button color-btn color-btn-blue" onclick="sendCommand('blue')">BLUE</button>
            <button class="button color-btn color-btn-white" onclick="sendCommand('white')">WHITE</button>
            <button class="button color-btn color-btn-off" onclick="sendCommand('off')">OFF</button>
        </div>

        <div class="tunnel-status">
            <div>Tunnel URL: <br>
                <a href="__TUNNEL_URL__" target="_blank" style="font-size:14px;">
                    __TUNNEL_URL__
                </a>
            </div>
        </div>
        
        <div class="local-ip">
            Local IP: <span id="localIp">Loading...</span>
        </div>
        
        <div class="setup-instructions">
            <strong>Important:</strong><br>
            Keep the cloudflared terminal running:<br>
            <code>cloudflared tunnel --url http://<span id="localIpSmall">[ESP32_IP]</span></code>
        </div>
        
        <div class="button-row">
            <button class="button reset-btn" onclick="resetWiFi()">Reset WiFi</button>
            <button class="button logout-btn" onclick="logout()">Logout</button>
        </div>
    </div>

    <script>
        function getSessionToken() {
            const params = new URLSearchParams(window.location.search);
            return params.get('token') || '';
        }

        function sendCommand(command) {
            const token = getSessionToken();
            if (!token) {
                window.location.href = '/?error=1';
                return;
            }
            
            fetch('/set?cmd=' + command + '&token=' + encodeURIComponent(token))
                .then(response => {
                    if (response.status === 401) {
                        window.location.href = '/?expired=1';
                        throw new Error('Session expired');
                    }
                    if (!response.ok) {
                        throw new Error('Command failed');
                    }
                    return response.text();
                })
                .then(data => {
                    const statusText = document.getElementById('statusText');
                    if (command === 'on' || command === 'red' || command === 'green' || command === 'blue' || command === 'white') {
                        statusText.textContent = 'ON';
                        statusText.className = 'status-on';
                    } else if (command === 'off') {
                        statusText.textContent = 'OFF';
                        statusText.className = 'status-off';
                    }
                })
                .catch(error => {
                    console.error('Error:', error);
                });
        }

        function resetWiFi() {
            const token = getSessionToken();
            if (!token) {
                window.location.href = '/?error=1';
                return;
            }
            
            if (confirm('Reset WiFi settings? The ESP32 will restart in AP mode.')) {
                fetch('/reset-wifi?token=' + encodeURIComponent(token))
                    .then(response => {
                        if (response.status === 401) {
                            window.location.href = '/?expired=1';
                            throw new Error('Unauthorized');
                        }
                        return response.text();
                    })
                    .then(data => {
                        alert('WiFi settings reset. ESP32 will restart...');
                    })
                    .catch(error => {});
            }
        }

        function logout() {
            window.location.href = '/logout';
        }

        const token = getSessionToken();
        if (token) {
            fetch('/status?token=' + encodeURIComponent(token))
                .then(response => {
                    if (response.status === 401) {
                        window.location.href = '/?expired=1';
                        throw new Error('Unauthorized');
                    }
                    return response.text();
                })
                .then(data => {
                    const statusText = document.getElementById('statusText');
                    if (data === 'on') {
                        statusText.textContent = 'ON';
                        statusText.className = 'status-on';
                    } else {
                        statusText.textContent = 'OFF';
                        statusText.className = 'status-off';
                    }
                })
                .catch(() => {});
        }

        if (token) {
            fetch('/local-ip?token=' + encodeURIComponent(token))
                .then(response => response.text())
                .then(data => {
                    document.getElementById('localIp').textContent = data;
                    document.getElementById('localIpSmall').textContent = data;
                })
                .catch(() => {});
        }

        if (!token) {
            window.location.href = '/?error=1';
        }
    </script>
</body>
</html>
)rawliteral";

void setLEDColor(int red, int green, int blue);
void startWebServer();
void startAPMode();
void stopAPMode();
void handleConfigRoot();
void handleConnect();
void handleScan();
void handleResetWiFi();
void handleRoot();
void handleLogin();
void handleLogout();
void handleSet();
void handleStatus();
void handleTunnelStatus();
void handleLocalIP();
bool isAuthenticated();
String generateSessionToken();

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n╔═══════════════════════════════════╗");
    Serial.println("║   ESP32 RGB LED Controller      ║");
    Serial.println("╚═══════════════════════════════════╝\n");
    
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    pinMode(bluePin, OUTPUT);
    setLEDColor(0, 0, 0);
    ledState = false;
    
    preferences.begin("wifi", false);
    
    if (!preferences.isKey("adminPass")) {
        preferences.putString("adminPass", "admin1263");
        Serial.println("🔑 Set default admin password: admin123");
    }
    
    currentSessionToken = preferences.getString("sessionToken", "");
    if (currentSessionToken.length() == 0) {
        currentSessionToken = generateSessionToken();
        preferences.putString("sessionToken", currentSessionToken);
        Serial.println("🔑 Generated new session token");
    }
    preferences.end();
    
    preferences.begin("wifi", false);
    String savedSSID = preferences.getString("ssid", "");
    String savedPassword = preferences.getString("password", "");
    preferences.end();
    
    if (savedSSID.length() > 0) {
        Serial.println("📡 Found saved WiFi credentials");
        Serial.print("   SSID: ");
        Serial.println(savedSSID);
        
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        Serial.print("   Connecting to WiFi");
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n✅ WiFi connected!");
            Serial.print("   📶 IP Address: ");
            Serial.println(WiFi.localIP());
            wifiConfigured = true;
            startWebServer();
            
            Serial.println("\n✅ All set! Access your LED controller:");
            Serial.println("   📱 Local: http://" + WiFi.localIP().toString());
            Serial.println("   🌐 Internet: " + tunnelUrl);
            Serial.println("\n   ⚠️  IMPORTANT: Keep cloudflared running!");
            Serial.println("   🔧 Run this command in PowerShell:");
            Serial.println("   cloudflared tunnel --url http://" + WiFi.localIP().toString());
            Serial.println("\n   🔑 Default admin password: admin123 (CHANGE THIS!)");
        } else {
            Serial.println("\n❌ Failed to connect to saved WiFi");
            preferences.begin("wifi", false);
            preferences.clear();
            preferences.end();
            startAPMode();
        }
    } else {
        Serial.println("📶 No saved WiFi credentials found");
        startAPMode();
    }
}

void loop() {
    server.handleClient();
    
    if (apModeActive) {
        dnsServer.processNextRequest();
    }
}

String generateSessionToken() {
    return String(millis()) + String(random(100000, 999999));
}

bool isAuthenticated() {
    String token = server.arg("token");
    if (token.length() > 0 && token == currentSessionToken) {
        sessionExpiry = millis() + SESSION_TIMEOUT;
        return true;
    }
    
    if (sessionExpiry > 0 && millis() > sessionExpiry) {
        return false;
    }
    
    return false;
}

void startAPMode() {
    apModeActive = true;
    Serial.println("\n🔵 Starting Access Point mode");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID.c_str(), AP_PASSWORD.c_str());
    Serial.print("   📶 AP SSID: ");
    Serial.println(AP_SSID);
    Serial.print("   🔑 AP Password: ");
    Serial.println(AP_PASSWORD);
    Serial.print("   📶 AP IP Address: ");
    Serial.println(WiFi.softAPIP());
    
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    server.on("/", handleConfigRoot);
    server.on("/connect", handleConnect);
    server.on("/scan", handleScan);
    server.on("/reset-wifi", handleResetWiFi);
    server.begin();
    
    Serial.println("\n✅ WiFi configuration portal started");
    Serial.println("   📱 Connect to '" + AP_SSID + "' WiFi network");
    Serial.println("   🔑 Password: " + AP_PASSWORD);
    Serial.println("   🌐 Then open http://192.168.4.1 in your browser");
}

void stopAPMode() {
    apModeActive = false;
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    Serial.println("   🔴 AP Mode stopped");
}

void startWebServer() {
    if (apModeActive) {
        stopAPMode();
    }
    
    server.on("/", handleRoot);
    server.on("/login", handleLogin);
    server.on("/logout", handleLogout);
    server.on("/set", handleSet);
    server.on("/status", handleStatus);
    server.on("/tunnel-status", handleTunnelStatus);
    server.on("/local-ip", handleLocalIP);
    server.on("/reset-wifi", handleResetWiFi);
    server.begin();
    Serial.println("   🌐 Web server started");
}

void handleConfigRoot() {
    server.send(200, "text/html", wifiConfigHtml);
}

void handleConnect() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    if (ssid.length() > 0) {
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
        
        server.send(200, "text/html", "<html><body style='background:#1a1a1a;color:white;text-align:center;padding:50px;font-family:Arial;'><h1 style='color:#4CAF50;'>✅ Credentials Saved!</h1><p>ESP32 is restarting...</p><p style='color:#888;font-size:12px;'>You can now close this page.</p></body></html>");
        delay(1000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Invalid SSID");
    }
}

void handleScan() {
    String json = "[";
    int n = WiFi.scanComplete();
    
    if (n == -2) {
        WiFi.scanNetworks(true);
        json = "[]";
    } else if (n >= 0) {
        for (int i = 0; i < n; ++i) {
            if (i) json += ",";
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i));
            json += "}";
        }
        WiFi.scanDelete();
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleLogin() {
    String password = server.arg("password");
    preferences.begin("wifi", false);
    String adminPass = preferences.getString("adminPass", "admin123");
    preferences.end();
    
    if (password == adminPass) {
        String redirectUrl = "/?token=" + currentSessionToken;
        server.sendHeader("Location", redirectUrl, true);
        server.send(302, "text/plain", "");
    } else {
        server.sendHeader("Location", "/?error=1", true);
        server.send(302, "text/plain", "");
    }
}

void handleLogout() {
    currentSessionToken = generateSessionToken();
    preferences.begin("wifi", false);
    preferences.putString("sessionToken", currentSessionToken);
    preferences.end();
    sessionExpiry = 0;
    
    server.sendHeader("Location", "/?logout=1", true);
    server.send(302, "text/plain", "");
}

void handleRoot() {
    String token = server.arg("token");
    if (token.length() > 0 && token == currentSessionToken) {
        sessionExpiry = millis() + SESSION_TIMEOUT;
        String html = String(FPSTR(index_html));
        html.replace("__TUNNEL_URL__", tunnelUrl);
        server.send(200, "text/html", html);
        return;
    }
    
    if (server.hasArg("expired")) {
        server.send(200, "text/html", loginHtml);
        return;
    }
    
    server.send(200, "text/html", loginHtml);
}

void handleSet() {
    if (millis() - lastCmdTime < CMD_COOLDOWN) {
        server.send(429, "text/plain", "Too many requests");
        return;
    }
    lastCmdTime = millis();
    
    if (!isAuthenticated()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    String cmd = server.arg("cmd");
    Serial.print("   🎮 Command received: ");
    Serial.println(cmd);
    
    if (cmd == "on") {
        setLEDColor(255, 255, 255);
        ledState = true;
        server.send(200, "text/plain", "OK");
    } 
    else if (cmd == "off") {
        setLEDColor(0, 0, 0);
        ledState = false;
        server.send(200, "text/plain", "OK");
    }
    else if (cmd == "red") {
        setLEDColor(255, 0, 0);
        ledState = true;
        server.send(200, "text/plain", "OK");
    }
    else if (cmd == "green") {
        setLEDColor(0, 255, 0);
        ledState = true;
        server.send(200, "text/plain", "OK");
    }
    else if (cmd == "blue") {
        setLEDColor(0, 0, 255);
        ledState = true;
        server.send(200, "text/plain", "OK");
    }
    else if (cmd == "white") {
        setLEDColor(255, 255, 255);
        ledState = true;
        server.send(200, "text/plain", "OK");
    }
    else {
        server.send(400, "text/plain", "Invalid command");
    }
}

void handleStatus() {
    if (!isAuthenticated()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    if (ledState) {
        server.send(200, "text/plain", "on");
    } else {
        server.send(200, "text/plain", "off");
    }
}

void handleResetWiFi() {
    if (!isAuthenticated()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    server.send(200, "text/plain", "WiFi settings cleared. Restarting...");
    delay(1000);
    ESP.restart();
}

void handleLocalIP() {
    if (!isAuthenticated()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    server.send(200, "text/plain", WiFi.localIP().toString());
}

void handleTunnelStatus() {
    String response = "{\"connected\":true,\"url\":\"" + tunnelUrl + "\"}";
    server.send(200, "application/json", response);
}

void setLEDColor(int red, int green, int blue) {
    analogWrite(redPin, red);
    analogWrite(greenPin, green);
    analogWrite(bluePin, blue);
}